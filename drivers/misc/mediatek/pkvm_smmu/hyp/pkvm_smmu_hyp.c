// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#include "smmu.h"
#include "smmu_mgmt.h"
#include "mpool.h"
#include "malloc.h"
#include <asm/alternative-macros.h>
#include <asm/barrier.h>
#include <asm/kvm_mmu.h>
#include <asm/kvm_pkvm_module.h>
#include <asm/io.h>
#include <asm/kvm_host.h>
#include <asm/processor.h>
#include <asm/kvm_hyp.h>
#include <linux/arm-smccc.h>
#include <nvhe/trap_handler.h>
#include "hyp_spinlock.h"
#include <nvhe/iommu.h>
#include <kvm/iommu.h>

#ifdef memset
#undef memset
#endif
#ifdef memcpy
#undef memcpy
#endif
extern const struct pkvm_module_ops *pkvm_smmu_ops;
#define CALL_FROM_OPS(fn, ...) pkvm_smmu_ops->fn(__VA_ARGS__)
#define __pkvm_hyp_donate_host(x, y) CALL_FROM_OPS(hyp_donate_host, x, y)
#define __pkvm_host_donate_hyp(x, y) CALL_FROM_OPS(host_donate_hyp, x, y, false)
#define hyp_phys_to_virt(x) CALL_FROM_OPS(hyp_va, x)
#define kvm_iommu_snapshot_host_stage2(x) CALL_FROM_OPS(iommu_snapshot_host_stage2, x)
#ifdef kern_hyp_va
#undef kern_hyp_va
#endif
#define kern_hyp_va(x) ((u64 *)CALL_FROM_OPS(kern_hyp_va, (unsigned long)x))
#ifdef __hyp_pa
#undef __hyp_pa
#endif
#define __hyp_pa(x) CALL_FROM_OPS(hyp_pa, x)

void *memset(void *dst, int c, size_t count)
{
	return CALL_FROM_OPS(memset, dst, c, count);
}

void *memcpy(void *dst, const void *src, size_t count)
{
	return CALL_FROM_OPS(memcpy, dst, src, count);
}

#define REG64(addr) ((volatile uint64_t *)(uintptr_t)(addr))
#define writel_64(v, a) (*REG64(a) = (v))
#define readl_64(a) (*REG64(a))

typedef unsigned long paddr_t;
typedef uintptr_t vaddr_t;
typedef unsigned long size_t;
static unsigned int ste_init_bypass_times;

/* default 0: disable hw dvm */
void *dvm_mi_va_base;

unsigned int smmu_devices_count;
smmu_device_t *smmu_devices[SMMU_ID_NUM];
DEFINE_HYP_SPINLOCK(smmu_all_vm_lock);
#define IOMMU_DRIVER_MEM_PFN_MAX (100U)

struct fmpt {
	u64 *smpt;
	u64 mem_order;
};

struct mpt {
	/* Memory used by IOMMU driver */
	struct fmpt fmpt[IOMMU_DRIVER_MEM_PFN_MAX];
	u32 mem_block_num;
};

struct mpt hyp_mpool;

enum setup_vm_ops {
	DEFAULT_VMID = 0,
	VM_NO_MAP_MBLOCK,
	S2_BYPASS_SID,
	VMID,
	SID,
	IDENTITY_MAP_MODE,
	IDENTITY_MAP,
	IDENTITY_UNMAP,
	IDENTITY_MAP_MBLOCK,
	IDENTITY_UNMAP_MBLOCK,
	IDENTITY_MAP_MBLOCK_EXCLUSIVE,
	IPA_GRANULE
};

uint32_t has_dma_coherent_in_devices(void)
{
	unsigned int i, dma_coherent_dev_count = 0;

	for (i = 0; i < smmu_devices_count; i++) {
		if (smmu_devices[i]->dma_coherent == 1)
			dma_coherent_dev_count++;
	}

	return dma_coherent_dev_count;
}

uint32_t has_dma_coherent_by_smmu_id(uint32_t smmu_id)
{
	unsigned int i;

	for (i = 0; i < smmu_devices_count; i++)
		if (smmu_devices[i]->smmu_id == smmu_id &&
		    smmu_devices[i]->dma_coherent)
			return 1;
	return 0;
}

/* get smmu hw semaphore to make sure smmu hw power keep on */
static int get_target_smmu_hw_semaphore(smmu_device_t *source_smmu_dev)
{
	/* power_status==0 => power on; !=0 => power off */
	unsigned int power_status = -1;
	struct arm_smccc_res smc_res;
	unsigned int smmu_id, sip_id;

	sip_id = MTK_SIP_HYP_SMMU_CONTROL;
	smmu_id = source_smmu_dev->smmu_id;
	arm_smccc_1_1_smc(sip_id, HYP_SMMU_PM_GET, smmu_id, 0, 0, 0, 0, 0,
			  &smc_res);

	power_status = smc_res.a0;
	if (power_status == sip_id) {
		pkvm_smmu_ops->puts("smc fail because not support");
		WARN_ON(1);
		return -1;
	}

	return power_status;
}

/* put smmu hw semaphore */
static void put_target_smmu_hw_semaphore(smmu_device_t *source_smmu_dev)
{
	struct arm_smccc_res smc_res;
	unsigned int smmu_id, sip_id;

	sip_id = MTK_SIP_HYP_SMMU_CONTROL;
	smmu_id = source_smmu_dev->smmu_id;
	arm_smccc_1_1_smc(sip_id, HYP_SMMU_PM_PUT, smmu_id, 0, 0, 0, 0, 0,
			  &smc_res);
}

/* check smmu el1 cmdq enable status */
bool el1_smmu_cmdq_enable(smmu_device_t *smmu_dev)
{
	return (smmu_dev->guest_cmdq_regval) ? true : false;
}

/* software DVM */
static void broadcast_cmd_to_all_smmu(uint64_t *cmd0, uint64_t *cmd1)
{
	unsigned int subsys_smmu = 0;
	smmu_device_t *smmu_dev = NULL;
	uint64_t cmd_sync[CMD_SIZE_DW] = { 0ULL };
	int ret = 0;

	construct_cmd_sync(cmd_sync);
	for (subsys_smmu = 0; subsys_smmu < smmu_devices_count; subsys_smmu++) {
		smmu_dev = smmu_devices[subsys_smmu];
		/* not only check smmu power status but also check smmuv3_queue
		 * structure has been init. if both condiitons are true, then issue
		 * broascast cmd to the smmu cmdq.
		 */
		if (!el1_smmu_cmdq_enable(smmu_dev))
			continue;
		hyp_spin_lock(&smmu_dev->cmdq_batch_lock);
		ret = get_target_smmu_hw_semaphore(smmu_dev);
		if (ret == SMMU_POWER_ON) {
			/* cmd0 for linux-vm */
			if (!smmuv3_issue_cmd(smmu_dev, cmd0))
				pkvm_smmu_ops->puts("issue cmd0 fail");
			/* cmd1 for protected-vm */
			if (!smmuv3_issue_cmd(smmu_dev, cmd1))
				pkvm_smmu_ops->puts("issue cmd1 fail");
			/* cmd sync */
			if (!smmuv3_issue_cmd(smmu_dev, cmd_sync))
				pkvm_smmu_ops->puts("cmd sync fail");
			/* make sure that all cmds in this batch have been finished */
			smmuv3_rd_meets_wr_idx(smmu_dev);
			put_target_smmu_hw_semaphore(smmu_dev);
		}
		hyp_spin_unlock(&smmu_dev->cmdq_batch_lock);
	}
}

/*
 * Check SMMUEN bit in the CR0ACK guest register value to make sure that
 * SMMU has been initialized successfully.
 * Why do not access cr0ack register directly?
 * Cause, register may not have power, when accessing it. Besides,
 * guest_cr0ack_regval correctly imitiates guest use situation.
 */
static bool smmu_init_done(smmu_device_t *smmu_dev)
{
	return (smmu_dev->guest_cr0ack_regval  & SMMU_EN) ? true : false;
}

/*
 * Dump DVM MI register for debug purpose.
 * 1. FIFO register: Number of pending dvm messages,
 * 2. OSTD register: Number of waiting response dvm messages,
 */
static void dvm_mi_reg_debug_dump(void)
{
	if (!dvm_mi_va_base) {
		pkvm_smmu_ops->puts("[pKVM_SMMU] dvm mi va is NULL");
		return;
	}

	/* FIFO register dump */
	pkvm_smmu_ops->puts("[pKVM_SMMU] AC_FIFO");
	pkvm_smmu_ops->putx64(readl((void *)(dvm_mi_va_base + AC_FIFO)));
	pkvm_smmu_ops->puts("[pKVM_SMMU] CR_FIFO");
	pkvm_smmu_ops->putx64(readl((void *)(dvm_mi_va_base + CR_FIFO)));
	pkvm_smmu_ops->puts("[pKVM_SMMU] AR_FIFO");
	pkvm_smmu_ops->putx64(readl((void *)(dvm_mi_va_base + AR_FIFO)));
	pkvm_smmu_ops->puts("[pKVM_SMMU] AW_R_FIFO");
	pkvm_smmu_ops->putx64(readl((void *)(dvm_mi_va_base + AW_R_FIFO)));
	/* OSTD register dump */
	pkvm_smmu_ops->puts("[pKVM_SMMU] ACP_OSTD_CNT_0");
	pkvm_smmu_ops->putx64(readl((void *)(dvm_mi_va_base + ACP_OSTD_CNT_0)));
	pkvm_smmu_ops->puts("[pKVM_SMMU] ACP_OSTD_CNT_1");
	pkvm_smmu_ops->putx64(readl((void *)(dvm_mi_va_base + ACP_OSTD_CNT_1)));
	pkvm_smmu_ops->puts("[pKVM_SMMU] AC_OSTD_CNT");
	pkvm_smmu_ops->putx64(readl((void *)(dvm_mi_va_base + AC_OSTD_CNT)));
	pkvm_smmu_ops->puts("[pKVM_SMMU] AR_OSTD_CNT");
	pkvm_smmu_ops->putx64(readl((void *)(dvm_mi_va_base + AR_OSTD_CNT)));
	pkvm_smmu_ops->puts("[pKVM_SMMU] SYNC_OSTD_CNT_0");
	pkvm_smmu_ops->putx64(readl((void *)(dvm_mi_va_base + SYNC_OSTD_CNT_0)));
	pkvm_smmu_ops->puts("[pKVM_SMMU] SYNC_OSTD_CNT_1");
	pkvm_smmu_ops->putx64(readl((void *)(dvm_mi_va_base + SYNC_OSTD_CNT_1)));
	pkvm_smmu_ops->puts("[pKVM_SMMU] COMP_OSTD_CNT");
	pkvm_smmu_ops->putx64(readl((void *)(dvm_mi_va_base + COMP_OSTD_CNT)));
	pkvm_smmu_ops->puts("[pKVM_SMMU] AW_OSTD_CNT");
	pkvm_smmu_ops->putx64(readl((void *)(dvm_mi_va_base + AW_OSTD_CNT)));
	pkvm_smmu_ops->puts("[pKVM_SMMU] W_OSTD_CNT");
	pkvm_smmu_ops->putx64(readl((void *)(dvm_mi_va_base + W_OSTD_CNT)));
	pkvm_smmu_ops->puts("[pKVM_SMMU] AOL_OSTD_CNT_0");
	pkvm_smmu_ops->putx64(readl((void *)(dvm_mi_va_base + AOL_OSTD_CNT_0)));
	pkvm_smmu_ops->puts("[pKVM_SMMU] AOL_OSTD_CNT_1");
	pkvm_smmu_ops->putx64(readl((void *)(dvm_mi_va_base + AOL_OSTD_CNT_1)));
}

/*
 * Issuie HW DVM Requirements:
 * 1. DVM MI register should be mapped to Hypervisor,
 * 2. All subsys SMMU can use DVM methods,
 * 2-1. All subsys SMMU init done,
 * 2-2. All subsys SMMU CR2 PTM should be set to 0,
 * 2-3. TCU PCTRL path must be enable,
 */
static unsigned int hw_dvm_check(void)
{
	unsigned int subsys_smmu = 0;
	smmu_device_t *smmu_dev = NULL;

	if (!dvm_mi_va_base) {
		pkvm_smmu_ops->puts("[pKVM_SMMU] dvm_mi register does not mapping");
		return HW_DVM_DISABLE;
	}

	for (subsys_smmu = 0; subsys_smmu < smmu_devices_count; subsys_smmu++) {
		smmu_dev = smmu_devices[subsys_smmu];
		if (!smmu_init_done(smmu_dev)) {
			/* SMMU have not been initialized yet, next time try again */
			return HW_DVM_RETRY;
		}

		if (smmu_dev->cr2_value & CR2_PTM) {
			/*
			 * PTM set to 1 means SMMU does not participate in broadcast
			 * TLB maintenance.
			 */
			pkvm_smmu_ops->puts("[pKVM_SMMU] PTM is set");
			return HW_DVM_DISABLE;
		}

		/*
		 * Check TCU pctrl enable state:
		 * No need, cause this logic has been checked in
		 * the function pctrl_enable, which is in the pkvm_smmu_host.c
		 */
	}
	pkvm_smmu_ops->puts("[pKVM_SMMU] hw dvm good to go");
	return HW_DVM_ENABLE;
}

/* MCUSYS and DVM_MI connect */
static bool dvm_mi_cpu_connect(void)
{
	void *reg_address_va = NULL;
	uint32_t polling_times = 0;
	uint32_t reg_value = 0;

	if (!dvm_mi_va_base) {
		pkvm_smmu_ops->puts("[pKVM_SMMU] dvm mi va is NULL");
		return false;
	}

	reg_address_va = (void *)(dvm_mi_va_base + DVM_MI_DVM_CTRL);
	reg_value = readl(reg_address_va);

	/* Check dvm mi and mcusys connect status */
	switch ((reg_value & SYSCO_STATUS_MASK)) {
	case DVM_CONNECTED:
		/* DVM has already connected with MCUSYS */
		return true;
	case DVM_EN_ACK:
		/*
		 * In this case, coherency connection signaling states is
		 * Disconnect(SYSCOREQ == 0 && SYSACK == 1). In the connect
		 * flow, this probably means that coherency connection
		 * signaling states is incorrect now.
		 */
		pkvm_smmu_ops->puts("[pKVM_SMMU] Unexpected dvm status");
		break;
	case DVM_DISCONNECTED:
		/* Connect system coherence to MCUSYS */
		writel((reg_value | DVM_EN_REQ), reg_address_va);
		fallthrough;
	case DVM_EN_REQ:
		for (polling_times = 0; polling_times < MAX_POLLING_TIME;
			polling_times++) {
			/* Polling until connect success */
			if ((readl(reg_address_va) & SYSCO_STATUS_MASK)	==
				 DVM_CONNECTED)
				return true;
		}
		pkvm_smmu_ops->puts("[pKVM_SMMU] dvm connect timeout");
		break;
	default:
		break;
	}

	dvm_mi_reg_debug_dump();
	return false;
}

/* MCUSYS and DVM_MI disconnect */
static void dvm_mi_cpu_disconnect(void)
{
	void *reg_address_va = NULL;
	uint32_t polling_times = 0;
	uint32_t reg_value = 0;

	if (!dvm_mi_va_base) {
		pkvm_smmu_ops->puts("[pKVM_SMMU] dvm mi va is NULL");
		return;
	}

	reg_address_va = (void *)(dvm_mi_va_base + DVM_MI_DVM_CTRL);
	reg_value = readl(reg_address_va);

	/* Check dvm mi and mcusys connect status */
	switch ((reg_value & SYSCO_STATUS_MASK)) {
	case DVM_DISCONNECTED:
		/* DVM has already disconnected with MCUSYS */
		return;
	case DVM_EN_REQ:
		/*
		 * In this case, coherency connection signaling states is
		 * Connect(SYSCOREQ == 1 && SYSACK == 0). In the disconnect
		 * flow, this probably means that coherency connection
		 * signaling states is incorrect now.
		 */
		pkvm_smmu_ops->puts("[pKVM_SMMU] Unexpected dvm status");
		break;
	case DVM_CONNECTED:
		/* Disconnect system coherence to MCUSYS */
		writel((reg_value & (~DVM_EN_REQ)), reg_address_va);
		fallthrough;
	case DVM_EN_ACK:
		for (polling_times = 0; polling_times < MAX_POLLING_TIME;
			polling_times++) {
			/* Polling until connect success */
			if ((readl(reg_address_va) & SYSCO_STATUS_MASK) ==
				 DVM_DISCONNECTED)
				return;
		}
		pkvm_smmu_ops->puts("[pKVM_SMMU] dvm disconnect timeout");
		break;
	default:
		break;
	}

	dvm_mi_reg_debug_dump();
	return;
}

static void issue_sw_dvm(void)
{
	uint64_t cmd0[CMD_SIZE_DW];
	uint64_t cmd1[CMD_SIZE_DW];

	construct_cmd_tlbi_s12_vmall(cmd0, 0);
	construct_cmd_tlbi_s12_vmall(cmd1, 1);
	broadcast_cmd_to_all_smmu(cmd0, cmd1);
}

/*
 * Issue TLBI command from CPU to SMMU:
 * AllE1IS: Invalidate stage 1 or stage 2 TLB entries with any VMID,
 * VMALLS12E1IS: Invalidate stage 1 or stage 2 TLB entries with current VMID,
 */
void issue_tlbi(void)
{
	dsb(ish);
	__tlbi(alle1is);
	dsb(ish);
}

/* according address to justify the address is belonged in which subsys smmu */
smmu_device_t *get_smmu_dev_from_id(uint32_t smmu_id)
{
	unsigned int subsys_smmu = 0;
	smmu_device_t *smmu_dev = NULL;

	for (subsys_smmu = 0; subsys_smmu < smmu_devices_count; subsys_smmu++) {
		smmu_dev = smmu_devices[subsys_smmu];
		if (smmu_dev->smmu_id == smmu_id)
			return smmu_dev;
	}
	return NULL;
}

/*
 * FIXME: This connection should implemented in the BK/RS flow.
 * This function connect infra smmu dvm and dvm mi
 */
static bool connect_infra_smmu_and_dvm_mi(void)
{
	uint32_t polling_times = 0;
	void *reg_address_va = NULL;
	smmu_device_t *smmu_dev = NULL;

	smmu_dev = get_smmu_dev_from_id(SMMU_SOC);
	if (!smmu_dev) {
		pkvm_smmu_ops->puts("[pKVM_SMMU] get smmu device failed");
		return false;
	}
	if (!smmu_dev->reg_base_va_addr) {
		pkvm_smmu_ops->puts("[pKVM_SMMU] reg base va is NULL, skip disconnect");
		return false;
	}

	reg_address_va = (void *)(smmu_dev->reg_base_va_addr + SMMU_TCU_CTL4);
	writel(0x1, reg_address_va);
	for (polling_times = 0; polling_times < MAX_POLLING_TIME; polling_times++) {
		if ((readl(reg_address_va) & 0x3) == 0x3)
			return true;
	}
	pkvm_smmu_ops->puts("[pKVM_SMMU] dvm mi and smmu connect timeout");

	return false;
}

/*
 * FIXME: This connection should implemented in the BK/RS flow.
 * This function disconnect infra smmu dvm and dvm mi
 */
static void disconnect_infra_smmu_and_dvm_mi(void)
{
	uint32_t polling_times = 0;
	void *reg_address_va = NULL;
	smmu_device_t *smmu_dev = NULL;

	smmu_dev = get_smmu_dev_from_id(SMMU_SOC);
	if (!smmu_dev) {
		pkvm_smmu_ops->puts("[pKVM_SMMU] get smmu device failed");
		return;
	}
	if (!smmu_dev->reg_base_va_addr) {
		pkvm_smmu_ops->puts("[pKVM_SMMU] reg base va is NULL, skip disconnect");
		return;
	}

	reg_address_va = (void *)(smmu_dev->reg_base_va_addr + SMMU_TCU_CTL4);
	writel(0x0, reg_address_va);
	for (polling_times = 0; polling_times < MAX_POLLING_TIME; polling_times++) {
		if ((readl(reg_address_va) & 0x3) == 0x0)
			return;
	}
	pkvm_smmu_ops->puts("[pKVM_SMMU] dvm mi and smmu disconnect timeout");
	dvm_mi_reg_debug_dump();
}

static void issue_hw_dvm(void)
{
	if (!connect_infra_smmu_and_dvm_mi()) {
		issue_sw_dvm();
		return;
	}

	if (!dvm_mi_cpu_connect()) {
		issue_sw_dvm();
		return;
	}

	issue_tlbi();
	dvm_mi_cpu_disconnect();
	disconnect_infra_smmu_and_dvm_mi();
}

static void broadcast_vmalls12e1is(void)
{
	static uint32_t hw_dvm_state = HW_DVM_RETRY;

	if (hw_dvm_state == HW_DVM_RETRY)
		hw_dvm_state = hw_dvm_check();

	/* SW DVM */
	if ((hw_dvm_state == HW_DVM_DISABLE) || (hw_dvm_state == HW_DVM_RETRY)) {
		issue_sw_dvm();
		return;
	}

	/* HW DVM */
	issue_hw_dvm();
}

void mtk_smmu_sync(void)
{
	broadcast_vmalls12e1is();
}

/* Merge linux vm page table into huge page */
void smmu_merge_s2_table(struct user_pt_regs *regs)
{
	hyp_spin_lock(&smmu_all_vm_lock);
	smmu_vm_defragment(0);
	/* No need to defrag protect vm-1
	 * because protect vm map memory qranulity will be
	 * the biggest one each time. e.g. granule=2MB while map 2MB
	 */
	mtk_smmu_sync();
	hyp_spin_unlock(&smmu_all_vm_lock);
	regs->regs[0] = SMCCC_RET_SUCCESS;
	regs->regs[1] = 0;
}

static bool unshare_memory_from_hyp(uint64_t region_start, uint64_t region_size)
{
	int ret = 0;
	uint64_t unshare_idx, region_pfn = region_start >> PAGE_SHIFT,
	  pfn_total = region_size >> PAGE_SHIFT;

	/* Unpin memory region from hyp */
	pkvm_smmu_ops->unpin_shared_mem(
		(void *)(pkvm_smmu_ops->hyp_va((phys_addr_t)region_start)),
		(((void *)pkvm_smmu_ops->hyp_va((phys_addr_t)region_start)) +
		 region_size));
	/* Unshare memory region from hyp */
	for (unshare_idx = 0; unshare_idx < pfn_total; unshare_idx++) {
		ret = pkvm_smmu_ops->host_unshare_hyp(region_pfn + unshare_idx);
		if (ret) {
			pkvm_smmu_ops->puts("unshare_memory_from_hyp: unshare memory fail");
			return false;
		}
	}
	return true;
}

static bool share_memory_to_hyp(uint64_t region_start, uint64_t region_size)
{
	int ret = 0;
	uint64_t share_idx, share_abort_idx, region_pfn = region_start >> PAGE_SHIFT,
	  pfn_total = region_size >> PAGE_SHIFT;

	for (share_idx = 0; share_idx < pfn_total; share_idx++) {
		/* host_share_hyp's input parameter is pfn no matter kernel pa or hyp pa */
		ret = pkvm_smmu_ops->host_share_hyp(region_pfn + share_idx);

		if (ret) {
			pkvm_smmu_ops->puts("share_memory_to_hyp : share memory fail");
			goto share_abort_handle;
		}
	}
	/* Pin those shared memory pages to hyp */
	ret = pkvm_smmu_ops->pin_shared_mem(
		(void *)(pkvm_smmu_ops->hyp_va((phys_addr_t)region_start)),
		(((void *)pkvm_smmu_ops->hyp_va((phys_addr_t)region_start)) +
		 region_size));

	if (ret) {
		pkvm_smmu_ops->puts("share_memory_to_hyp : pin memory fail");
		goto share_abort_handle;
	}

	return true;

share_abort_handle:
	/* Abort this memory share operation */
	for (share_abort_idx = 0; share_abort_idx < share_idx;
		 share_abort_idx++) {
		ret = pkvm_smmu_ops->host_unshare_hyp(region_pfn + share_abort_idx);
		if (ret) {
			pkvm_smmu_ops->puts(
				"share_memory_to_hyp : host_unshare_hyp fail");
			break;
		}
	}
	return false;
}

void mtk_smmu_share(struct user_pt_regs *regs)
{
	uint64_t region_start = 0, region_size = 0;
	uint32_t smmu_id, type;
	smmu_device_t *smmu_dev = NULL;
	uint64_t **share_addr = NULL;

	regs->regs[0] = SMCCC_RET_SUCCESS;
	smmu_id = regs->regs[1];
	type = regs->regs[2];

	smmu_dev = get_smmu_dev_from_id(smmu_id);
	if (!smmu_dev) {
		pkvm_smmu_ops->puts("get smmu device failed");
		return;
	}
	/* region_start is memory physical address */
	switch (type) {
	case CMDQ_SHARE:
		region_start = smmu_dev->guest_cmdq_pa;
		region_size =
			MTK_SMMU_CMDQ_SIZE(smmu_dev->guest_cmdq_regval);
		share_addr = &smmu_dev->guest_cmdq_va;
		break;
	case STE_SHARE:
		region_start = smmu_dev->guest_strtab_base_pa;
		region_size = MTK_SMMU_STE_SIZE((uint32_t)readl(
			(void *)(smmu_dev->reg_base_va_addr +
					STRTAB_BASE_CFG)));
		share_addr = &smmu_dev->guest_strtab_base_va;
		break;
	default:
		pkvm_smmu_ops->puts("mtk_smmu_share : Error type");
		return;
	}

	if (share_addr == NULL) {
		if (type == CMDQ_SHARE)
			pkvm_smmu_ops->puts("Can't get CMDQ memory structure va");
		if (type == STE_SHARE)
			pkvm_smmu_ops->puts("Can't get STE memory structure va");
		return;
	}

	if (!share_memory_to_hyp(region_start, region_size))  {
		pkvm_smmu_ops->puts("mtk_smmu_share : smmu share memory fail");
		pkvm_smmu_ops->puts("mtk_smmu_share : smmu id");
		pkvm_smmu_ops->putx64(smmu_id);
		return;
	}

	*share_addr =
		(uint64_t *)pkvm_smmu_ops->hyp_va((phys_addr_t)region_start);
}
/* Maintain an array to records MPU region info */
static struct mpu_record pkvm_mpu_rec[MPU_REQ_ORIGIN_EL2_ZONE_MAX];

void mtk_smmu_secure(struct user_pt_regs *regs)
{
	paddr_t region_start;
	size_t region_size;
	uint32_t zone_id = regs->regs[3];

	regs->regs[0] = SMCCC_RET_SUCCESS;

	if (zone_id >= MPU_REQ_ORIGIN_EL2_ZONE_MAX) {
		pkvm_smmu_ops->puts("mtk_smmu_secure : zone_id is invalid");
		return;
	}
	/* Get the region start and size */
	region_start = regs->regs[1];
	region_size = regs->regs[2];
	/* Store the information of region start and size into mpu_record array */
	pkvm_mpu_rec[zone_id].addr = region_start;
	pkvm_mpu_rec[zone_id].size = region_size;
	/* Unamp the region from normal VM and map those memory into protected VM */
	hyp_spin_lock(&smmu_all_vm_lock);
	smmu_lazy_free();
	smmu_vm_unmap(0, region_start, region_size);
	smmu_vm_map(1, region_start, region_size,
		    MM_MODE_R | MM_MODE_W | MM_MODE_X);
	hyp_spin_unlock(&smmu_all_vm_lock);
}

void mtk_smmu_unsecure(struct user_pt_regs *regs)
{
	paddr_t region_start;
	size_t region_size;
	uint8_t vm0_default_mode, vm1_default_mode;
	uint32_t zone_id = regs->regs[3];

	regs->regs[0] = SMCCC_RET_SUCCESS;

	if (zone_id >= MPU_REQ_ORIGIN_EL2_ZONE_MAX) {
		pkvm_smmu_ops->puts("mtk_smmu_unsecure : zone_id is invalid");
		return;
	}
	/* TODO: to support dts default attr */
	vm0_default_mode = MM_MODE_R | MM_MODE_W | MM_MODE_X;
	vm1_default_mode = MM_MODE_R;

	/* Get region information from mpu_record array */
	region_start = pkvm_mpu_rec[zone_id].addr;
	region_size = pkvm_mpu_rec[zone_id].size;
	/* Map the region into normal VM and change those memory permission in protected VM */
	hyp_spin_lock(&smmu_all_vm_lock);
	smmu_lazy_free();
	smmu_vm_map(0, region_start, region_size, vm0_default_mode);
	smmu_vm_map(1, region_start, region_size, vm1_default_mode);
	hyp_spin_unlock(&smmu_all_vm_lock);
}

/*
 *  PMM_MSG_ENTRY format
 *  page number = PA >> PAGE_SHIFT
 *   _______________________________________
 *  |  reserved  | page order | page number |
 *  |____________|____________|_____________|
 *  31         28 27        24 23          0
 */
void mtk_smmu_secure_v2(struct user_pt_regs *regs)
{
	int ret = 0;
	void *pglist_pa;
	phys_addr_t pfn;
	uint64_t pglist_pfn;
	uint32_t entry, order, i, count, *pmm_page;

	regs->regs[0] = SMCCC_RET_SUCCESS;
	pglist_pfn = regs->regs[1];
	count = regs->regs[3];
	pglist_pa = (void *)(pglist_pfn << ONE_PAGE_OFFSET);
	regs->regs[1] = 0;

	if (!share_memory_to_hyp(pglist_pfn << ONE_PAGE_OFFSET, ONE_PAGE_SIZE)) {
		pkvm_smmu_ops->puts("mtk_smmu_secure_v2 : share mem fail");
		return;
	}

	pmm_page = (uint32_t *)pkvm_smmu_ops->hyp_va((phys_addr_t)pglist_pa);
	for (i = 0; i < count; i++) {
		entry = pmm_page[i];
		if (entry == 0)
			break;

		order = GET_PMM_ENTRY_ORDER(entry);
		pfn = GET_PMM_ENTRY_PFN(entry);
		/*
		 *	Using host_donate_hyp() will trigger smmu idmap, and as a result,
		 *	The permission of the page would be like,
		 *	isolate the page from Linux, and let it be accessible in
		 *	protected VM.
		 *	 ___________________________
		 *	| Linux        | Protected  |
		 *	| VM 0 (unmap) | VM 1 (rwx) |
		 *	|___________________________|
		 */
		ret = pkvm_smmu_ops->host_donate_hyp(pfn, 1 << order, false);
		if (ret)
			pkvm_smmu_ops->puts("mtk_smmu_secure_v2 : host_donate_hyp fail");
	}
	mtk_smmu_sync();

	if(!unshare_memory_from_hyp(pglist_pfn << ONE_PAGE_OFFSET, ONE_PAGE_SIZE)) {
		pkvm_smmu_ops->puts("mtk_smmu_secure_v2 : unshare mem fail");
		return;
	}
}

void mtk_smmu_unsecure_v2(struct user_pt_regs *regs)
{
	int ret = 0;
	void *pglist_pa;
	phys_addr_t pfn;
	uint64_t pglist_pfn;
	void *secure_page_va = NULL;
	uint32_t entry, order, i, count, *pmm_page;

	regs->regs[0] = SMCCC_RET_SUCCESS;
	pglist_pfn = regs->regs[1];
	count = regs->regs[3];
	pglist_pa = (void *)(pglist_pfn << ONE_PAGE_OFFSET);
	regs->regs[1] = false;

	if (!share_memory_to_hyp(pglist_pfn << ONE_PAGE_OFFSET, ONE_PAGE_SIZE)) {
		pkvm_smmu_ops->puts("mtk_smmu_unsecure_v2 : share mem fail");
		return;
	}

	pmm_page = (uint32_t *)pkvm_smmu_ops->hyp_va((phys_addr_t)pglist_pa);
	for (i = 0; i < count; i++) {
		entry = pmm_page[i];
		if (entry == 0)
			break;

		order = GET_PMM_ENTRY_ORDER(entry);
		pfn = GET_PMM_ENTRY_PFN(entry);
		/*
		 *	Using hyp_donate_host() will trigger smmu idmap, and as a result,
		 *	retrieve the page from protected VM back to linux VM. Therefore,
		 *	VM0,VM1 map the memory back to default mode
		 *	 ___________________________
		 *	| Linux        | Protected  |
		 *	| VM 0 (rwx)   | VM 1 (ro)  |
		 *	|___________________________|
		 */

		/*
		 * Clear secure page content and flush data into dram,
		 * before return the page to host.
		 */
		secure_page_va = pkvm_smmu_ops->hyp_va((phys_addr_t)(pfn * ONE_PAGE_SIZE));
		if (!secure_page_va) {
			pkvm_smmu_ops->puts("secure_page_va translate fail\n");
			pkvm_smmu_ops->puts("secure_page_va fail pfn:");
			pkvm_smmu_ops->putx64(pfn);
		} else {
			memset(secure_page_va, 0, ONE_PAGE_SIZE * (1 << order));
			smmu_flush_dcache(secure_page_va, ONE_PAGE_SIZE * (1 << order));
		}

		/* Return secure page to host */
		ret = pkvm_smmu_ops->hyp_donate_host(pfn, 1 << order);
		if (ret)
			pkvm_smmu_ops->puts("mtk_smmu_secure_v2 : hyp_donate_host fail");
	}
	mtk_smmu_sync();

	if(!unshare_memory_from_hyp(pglist_pfn << ONE_PAGE_OFFSET, ONE_PAGE_SIZE)) {
		pkvm_smmu_ops->puts("mtk_smmu_unsecure_v2 : unshare mem fail");
		return;
	}

	regs->regs[1] = true;
}

void flush_dcache_range(void *ptr, uint32_t size)
{
	pkvm_smmu_ops->flush_dcache_to_poc(ptr, size);
}

/* map mblock reserve memory into pKVM and initialize global STE and cmdq memory */
/*  ARM-SMMU
 *  +------------------------------------+
 *  | soc cmdq (512K)                    |
 *  | apu cmdq (512K)                    |
 *  | mm  cmdq (512K)                    |
 *  | gpu cmdq (512K)                    | 2MB
 *  +------------------------------------+
 *  | global ste(16kb)                   | 16KB
 *  +------------------------------------+
 *  | mem pool (32MB)                    | 32MB
 *  +------------------------------------+
 */
static paddr_t get_smmu_contig_mpool_pa(smmu_device_t *dev, unsigned int type)
{
	unsigned int index = dev->smmu_id;
	paddr_t pa = 0;

	switch (type) {
	case GET_GLOBAL_STE:
		pa = smmu_get_global_ste_pa();
		break;
	case GET_CMDQ:
		if (index < SMMU_ID_NUM)
			pa = smmu_get_cmdq_buf_pa(index);
		break;
	default:
		break;
	}
	if (!pa)
		pkvm_smmu_ops->puts("get_smmu_contig_mpool_pa fail");
	return pa;
}

static void *get_smmu_contig_mpool_va(smmu_device_t *dev, unsigned int type)
{
	unsigned int index = dev ? dev->smmu_id : SMMU_ID_NUM;
	void *va = NULL;

	switch (type) {
	case GET_GLOBAL_STE:
		va = smmu_get_global_ste();
		break;
	case GET_CMDQ:
		if (index < SMMU_ID_NUM)
			va = smmu_get_cmdq_buf(index);
		break;
	default:
		break;
	}
	if (!va)
		pkvm_smmu_ops->puts("get_smmu_contig_mpool_va fail");
	return va;
}

static void read_ste(const uint64_t *st_entry, uint64_t *data)
{
	int i;

	/*
	 * read to memory from upper double word of Stream Table entry
	 */
	for (i = STE_SIZE_DW - 1U; i >= 0; i--)
		data[i] = st_entry[i];
}

/* for smmu debug purpose, dump smmu global ste for guest */
unsigned long smmu_ste_content_info_by_row(uint8_t smmu_type, uint32_t row,
					   uint8_t sid)
{
	unsigned int smmu_id, subsys_smmu;
	uint64_t *host_ste_base, *host_sid_addr;
	uint64_t host_ste[STE_SIZE_DW];
	smmu_device_t *smmu_dev = NULL;

	if (row >= STE_SIZE_DW) {
		pkvm_smmu_ops->puts("row is bigger than STE total rows");
		return INVALID_STE_ROW_BIT;
	}
	for (subsys_smmu = 0; subsys_smmu < smmu_devices_count; subsys_smmu++) {
		smmu_dev = smmu_devices[subsys_smmu];
		smmu_id = smmu_dev->smmu_id;
		if (smmu_type != smmu_id)
			continue;
		host_ste_base = (uint64_t *)smmu_dev->smmuv3->strtab_cfg.base;
		host_sid_addr = (host_ste_base + (STE_SIZE_DW * sid));
		read_ste(host_sid_addr, host_ste);
		return host_ste[row];
	}

	return INVALID_SMMU_TYPE_BIT;
}

/* for smmu debug purpose, dump smmu hw reg for guest */
unsigned long smmu_debug_dump_reg(uint8_t smmu_type, uint32_t reg)
{
	uint32_t bit64_reg_array[] = { GERROR_IRQ_CFG0, STRTAB_BASE,
				       CMDQ_BASE,	EVENTQ_BASE,
				       EVENTQ_IRQ_CFG0, PRIQ_BASE,
				       PRIQ_IRQ_CFG0,	GATOS_SID,
				       GATOS_ADDR,	GATOS_PAR };
	unsigned int smmu_id, subsys_smmu;
	int i, array_length = ARRAY_SIZE(bit64_reg_array);

	smmu_device_t *smmu_dev = NULL;

	for (subsys_smmu = 0; subsys_smmu < smmu_devices_count; subsys_smmu++) {
		smmu_dev = smmu_devices[subsys_smmu];
		smmu_id = smmu_dev->smmu_id;

		if (smmu_type != smmu_id)
			continue;
		for (i = 0; i < array_length; i++)
			if (reg == bit64_reg_array[i])
				return (uint64_t)readl_64(
					smmu_dev->smmuv3->base_addr + reg);
		return (uint64_t)readl(smmu_dev->smmuv3->base_addr + reg);
	}
	return INVALID_SMMU_TYPE_BIT;
}

/*
 * 1. return smmu info to guest for debug purpose.
 * 2. the debug_parameter get debug info from guest, which layout would be like:
 *  sid         [7:0]
 *  smmu type   [9:8]
 *  reg         [18:10]
 *  ste row     [26:19]
 *  action id   [31:27]
 * 3. this parameter layout be defines at guest driver code.
 */
void mtk_smmu_host_debug(struct user_pt_regs *regs)
{
	uint8_t sid, smmu_type, action_id, ste_row;
	uint32_t reg, fault_ipa, debug_parameter;
	uint64_t debug_info = 0;

	regs->regs[0] = SMCCC_RET_SUCCESS;
	fault_ipa = regs->regs[1];
	debug_parameter = regs->regs[2];
	sid = debug_parameter & 0xff;
	smmu_type = (debug_parameter >> 8) & 0x3;
	reg = (debug_parameter >> 10) & 0x3ff;
	ste_row = (debug_parameter >> 19) & 0x1ff;
	action_id = (debug_parameter >> 27) & 0x1f;

	switch (action_id) {
	case HYP_SMMU_STE_DUMP:
		/* provide ste content by row */
		debug_info =
			smmu_ste_content_info_by_row(smmu_type, ste_row, sid);
		break;
	case HYP_SMMU_TF_DUMP:
		/* provide tf and permission_fault debug info */
		debug_info = smmu_s2_table_content_info(fault_ipa, sid);
		/* DVM MI register dump */
		dvm_mi_reg_debug_dump();
		break;
	case HYP_SMMU_REG_DUMP:
		/* provide smmu hw reg */
		debug_info = smmu_debug_dump_reg(smmu_type, reg);
		break;
	default:
		debug_info = INVALID_ACTION_ID_BIT;
		break;
	}
	regs->regs[1] = debug_info;
}

void add_smmu_device(struct user_pt_regs *regs)
{
	smmu_device_t *smmu_dev = NULL;
	u64 pfn, smmu_size;
	int ret;

	regs->regs[0] = SMCCC_RET_SUCCESS;
	smmu_dev = (smmu_device_t *)malloc(sizeof(smmu_device_t));
	if (!smmu_dev) {
		pkvm_smmu_ops->puts("add_smmu_device: smmu_dev malloc failed");
		return;
	}

	smmu_dev->smmu_id = regs->regs[4];
	smmu_dev->smmuv3 =
		(struct smmuv3_driver *)malloc(sizeof(struct smmuv3_driver));
	if (!smmu_dev->smmuv3) {
		pkvm_smmu_ops->puts("add_smmu_device: smmu_dev->smmuv3 malloc failed");
		free(smmu_dev);
		return;
	}

	smmu_dev->reg_base_pa_addr = regs->regs[1];
	smmu_dev->reg_size = regs->regs[2];
	smmu_dev->dma_coherent = regs->regs[3];
	smmu_devices[smmu_devices_count] = smmu_dev;

	hyp_spin_lock_init(&smmu_dev->cmdq_batch_lock);
	hyp_spin_lock_init(&smmu_dev->smmuv3->cmd_queue.cmdq_issue_lock);

	pfn = smmu_dev->reg_base_pa_addr >> ONE_PAGE_OFFSET;
	smmu_size = smmu_dev->reg_size >> ONE_PAGE_OFFSET;
	ret = pkvm_smmu_ops->host_donate_hyp(pfn, smmu_size, true);
	if (ret) {
		pkvm_smmu_ops->puts(
			"add_smmu_device : host_donate_hyp : fail");
		return;
	}
	smmu_dev->reg_base_va_addr =
		hyp_phys_to_virt(smmu_dev->reg_base_pa_addr);

	smmu_devices_count++;
}

/* set cmdq base into real smmu cmdq hw register */
static void write_smmu_cmdq_base_addr(smmu_device_t *dev, uint64_t cmdq_regval)
{
	uint64_t cmdq_pa = 0ULL;
	struct smmuv3_driver *smmuv3 = NULL;

	smmuv3 = dev->smmuv3;
	cmdq_pa = ((uint64_t)cmdq_regval) & SMMU_CMDQ_BASE_ADDR_MASK;

	if (!dev->guest_cmdq_regval) {
		/* store guest cmdq base address setting */
		dev->guest_cmdq_regval = cmdq_regval;
		dev->guest_cmdq_pa = cmdq_pa;
		if (!smmuv3) {
			pkvm_smmu_ops->puts("alloc smmuv3 structure fail");
			return;
		}

		smmuv3->prop.cmdq_entries_log2 = MTK_SMMU_HOST_CMDQ_ENTRY;
		smmuv3->cmd_queue.prod_reg_base =
			(void *)(dev->reg_base_va_addr + CMDQ_PROD);
		smmuv3->cmd_queue.cons_reg_base =
			(void *)(dev->reg_base_va_addr + CMDQ_CONS);
		smmuv3->base_addr = (void *)dev->reg_base_va_addr;
		/* Set guest cmdq size */
		dev->guest_cmdq_entries_log2 = MTK_SMMU_GUEST_CMDQ_ENTRY;
		/* host cmdq va */
		dev->smmuv3->cmd_queue.q_base =
			get_smmu_contig_mpool_va(dev, GET_CMDQ);
		cmdq_regval =
			(cmdq_regval & MTK_SMMU_CMDQ_RA_MASK) |
			((uint64_t)get_smmu_contig_mpool_pa(dev, GET_CMDQ)) |
			(MTK_SMMU_HOST_CMDQ_ENTRY);
		writel_64(cmdq_regval,
			  (void *)(dev->reg_base_va_addr + CMDQ_BASE));
	}
}

static void write_smmu_strtab_cfg_reg(smmu_device_t *dev, uint32_t guest_reg)
{
	paddr_t guest_strtab_base_pa = 0;
	struct smmuv3_driver *smmuv3 = NULL;

	if (!dev) {
		pkvm_smmu_ops->puts(
			"write_smmu_strtab_cfg_reg smmu_device_t *dev is NULL");
		return;
	}

	smmuv3 = dev->smmuv3;
	guest_strtab_base_pa = dev->guest_strtab_base_pa;

	if (!guest_strtab_base_pa) {
		pkvm_smmu_ops->puts(
			"write_smmu_strtab_cfg_reg guest_strtab_base_pa != 0");
		return;
	}
	/* Create once */
	if (dev->guest_strtab_base_pa) {
		if (!smmuv3) {
			pkvm_smmu_ops->puts("alloc smmuv3 structure fail");
			return;
		}

		smmuv3->prop.stream_n_bits =
			(MTK_SMMU_STE_SIZE_MASK & guest_reg);
		/* Write host strtab_cfg reg value */
		writel(guest_reg,
		       (void *)(dev->reg_base_va_addr + STRTAB_BASE_CFG));
	}
}

/*
 * check_ste_update_content
 * case 1: The content of STE 0 can't changed.
 * case 2: guest(Linux) only allow to change EL1 related fild, also can't not
 * overwrite valid bit of STE.
 */
static void check_ste_update_content(unsigned int sid, smmu_device_t *dev)
{
	uint64_t *host_ste_base = NULL, *guest_ste_base = NULL, *step = NULL,
		 *host_sid_addr = NULL;
	uint64_t guest_ste[STE_SIZE_DW] = { 0ULL };
	uint64_t host_ste[STE_SIZE_DW] = { 0ULL };
	uint64_t combined_ste[STE_SIZE_DW] = { 0ULL };
	uint64_t stage1_ste_mask[STE_SIZE_DW] = { 0ULL };
	int i;

	if (!dev || !dev->smmuv3) {
		pkvm_smmu_ops->puts(
			"check_ste_update_content smmu_device_t *dev or dev->smmuv3 is NULL");
		return;
	}

	if (sid > STE_ENTRY_NUM(dev->smmuv3->prop.stream_n_bits)) {
		pkvm_smmu_ops->puts("sid over expect sid numbers");
		return;
	} else if (sid == 0) {
		pkvm_smmu_ops->puts("WARN: sid 0 can not be changed");
		return;
	}

	stage1_ste_mask[0] = STRTAB_STE_0_V | STRTAB_STE_0_S2_CFG |
					STRTAB_STE_0_PASS_CFG;
	stage1_ste_mask[2] = STRTAB_STE_2_S2_SETTING;
	stage1_ste_mask[3] = STRTAB_STE_3_S2_SETTING;
	guest_ste_base = (uint64_t *)dev->guest_strtab_base_va;
	if (!guest_ste_base) {
		pkvm_smmu_ops->puts(
			"check_ste_update_content guest_ste_base is NULL");
		return;
	}

	host_ste_base = (uint64_t *)dev->smmuv3->strtab_cfg.base;
	if (!host_ste_base) {
		pkvm_smmu_ops->puts(
			"check_ste_update_content host_ste_base is NULL");
		return;
	}

	step = (guest_ste_base + (STE_SIZE_DW * sid));
	host_sid_addr = (host_ste_base + (STE_SIZE_DW * sid));
	/* read sid's STE info to guest ste */
	smmu_flush_dcache(step, STE_SIZE);
	read_ste(step, guest_ste);
	read_ste(host_sid_addr, host_ste);
	/* stage 2 info can't be overwrited by guest */
	for (i = STE_SIZE_DW - 1U; i >= 0; i--) {
		guest_ste[i] &= (~stage1_ste_mask[i]); /* clear s2 */
		host_ste[i] &= (stage1_ste_mask[i]); /* keep s2 */
		combined_ste[i] = host_ste[i] | guest_ste[i];
	}

	write_ste(host_sid_addr, combined_ste);
}
/* copu guest ste setting into global ste */
static void copy_ste_from_guest_to_host(smmu_device_t *dev)
{
	uint64_t ste_size;
	unsigned int sid;
	void *strtab_base_cfg_reg;

	strtab_base_cfg_reg = dev->smmuv3->base_addr + STRTAB_BASE_CFG;
	ste_size = 1
		   << ((uint32_t)readl(strtab_base_cfg_reg) & MAKE_MASK(5, 0));
	for (sid = 0; sid < ste_size; sid++)
		check_ste_update_content(sid, dev);
	dump_ste();
}

void issue_to_protected_vm(uint64_t *guest_cmd, smmu_device_t *dev)
{
	uint64_t host_cmd[2] = { 0 };

	host_cmd[0] = (guest_cmd[0] & (~CMD_VMID_FIELD)) |
		      (PROTECTED_VMID << CMD_VMID_SHIFT);
	host_cmd[1] = guest_cmd[1];
	if (!smmuv3_issue_cmd(dev, host_cmd))
		pkvm_smmu_ops->puts("smmuv3_issue_cmd_fail");
}

/*
 * check_cmd
 * case 1: invalid specific STE content.
 * case 2: invalid all STE content.
 * case 2-1: global STE only could be initiated one time.
 * case 3: this cmd is target for some vmid; thus, copy the same cmd but change
 * target vmid to other else.
 */
static bool check_cmd(uint64_t *guest_cmd, smmu_device_t *dev)
{
	unsigned int sid;
	uint8_t cmd_op;
	uint32_t strtab_base_cfg_reg_val;
	void *strtab_base_cfg_reg;

	strtab_base_cfg_reg = dev->smmuv3->base_addr + STRTAB_BASE_CFG;
	strtab_base_cfg_reg_val = (uint32_t)readl(strtab_base_cfg_reg);
	cmd_op = guest_cmd[0] & MAKE_MASK(7, 0);
	switch (cmd_op) {
	case OP_CFGI_STE:
		if (!STE_2lvl(strtab_base_cfg_reg_val)) {
			sid = STE_SID_FIELD((*guest_cmd));
			check_ste_update_content(sid, dev);
			dump_ste_sid(sid);
		}
		break;
	case OP_CFGI_ALL:
		if (!STE_2lvl(strtab_base_cfg_reg_val)) {
			if (ste_init_bypass_times < 1) {
				copy_ste_from_guest_to_host(dev);
				ste_init_bypass_times++;
			}
		}
		break;
	case CMDQ_OP_TLBI_NH_ASID:
		issue_to_protected_vm(guest_cmd, dev);
		break;
	case CMDQ_OP_TLBI_NH_VA:
		issue_to_protected_vm(guest_cmd, dev);
		break;
	case CMDQ_OP_RESERVED:
		pkvm_smmu_ops->puts("[Debug] Don't allow cmd 0 to be executed");
		break;
	// case CMDQ_OP_TLBI_S12_VMALL:
	// case CMDQ_OP_TLBI_S2_IPA:
	//return ILLEGAL_CMD;
	default:
		break;
	}

	return LEGAL_CMD;
}

static void handle_guest_write_prod(u64 cmdq_prod_reg_value, smmu_device_t *dev)
{
	uint32_t new_prod_reg;
	uint32_t cur_prod_reg;
	uint32_t cur_prod_idx, new_prod_idx;
	uint32_t cmd_num, cur_wrap, new_wrap;
	uint32_t q_max_entries;
	uint32_t cmdq_entries_log2;
	uint32_t cmdq_idx_mask;
	uint32_t i;
	uint64_t *guest_cmd;
	uint64_t guest_cmd_array[2];
	uint64_t *cmdq_base;
	uint64_t offset;

	new_prod_reg = (cmdq_prod_reg_value & ALL_1s(32));
	cur_prod_reg = dev->guest_prod_reg;
	cmdq_entries_log2 = dev->guest_cmdq_entries_log2;
	cmdq_idx_mask = ALL_1s(cmdq_entries_log2);
	new_prod_idx = new_prod_reg & cmdq_idx_mask;
	cur_prod_idx = cur_prod_reg & cmdq_idx_mask;
	/* How many commands that issued from the guest */
	q_max_entries = 1U << cmdq_entries_log2;
	new_wrap = EXTRACT(new_prod_reg, cmdq_entries_log2, WRAP_MASK);
	cur_wrap = EXTRACT(cur_prod_reg, cmdq_entries_log2, WRAP_MASK);
	if (new_wrap == cur_wrap) {
		if (new_prod_idx >= cur_prod_idx)
			cmd_num = new_prod_idx - cur_prod_idx;
		else {
			cmd_num = 0;
			pkvm_smmu_ops->puts("abnormal index [new] [cur]");
			pkvm_smmu_ops->putx64((u64)new_prod_idx);
			pkvm_smmu_ops->putx64((u64)cur_prod_idx);
			WARN_ON(1);
		}
	} else
		cmd_num = (new_prod_idx + q_max_entries) - cur_prod_idx;
	if (cmd_num > q_max_entries)
		pkvm_smmu_ops->puts("cmd_num error");

	cmdq_base = (u64 *)dev->guest_cmdq_va;
	if (!cmdq_base)
		pkvm_smmu_ops->puts("cmdq not yet map to el2");
	else {
		smmu_flush_dcache(cmdq_base,
				  MTK_SMMU_CMDQ_SIZE(dev->guest_cmdq_regval));
		hyp_spin_lock(&dev->cmdq_batch_lock);
		for (i = 0; i < cmd_num; i++) {
			offset = CMDQ_INDEX((cur_prod_idx + i), cmdq_idx_mask) *
				 CMD_SIZE_DW;
			guest_cmd = (cmdq_base + offset);
			guest_cmd_array[0] = guest_cmd[0];
			guest_cmd_array[1] = guest_cmd[1];

			if (check_cmd(guest_cmd_array, dev)) {
				/* Change illegal cmd into sync cmd */
				construct_cmd_sync(guest_cmd_array);
				pkvm_smmu_ops->puts(
					"Change illegal cmd into sync cmd");
			}

			if (!smmuv3_issue_cmd(dev, guest_cmd_array))
				pkvm_smmu_ops->puts("smmuv3_issue_cmd_fail");
		}
		dev->guest_prod_reg = new_prod_reg;
		/* make sure that all cmds in this batch have been finished */
		if (smmuv3_rd_meets_wr_idx(dev) == true)
			dev->guest_cons_reg = dev->guest_prod_reg;
		hyp_spin_unlock(&dev->cmdq_batch_lock);
	}
}

static void write_guest_strtab_base_reg(smmu_device_t *dev, uint64_t reg_val)
{
	if (!dev) {
		pkvm_smmu_ops->puts(
			"write_guest_strtab_base_reg smmu_device_t *dev is NULL");
		return;
	}

	/* Write Once Protection */
	if (!dev->guest_strtab_base_reg) {
		uint64_t guest_ste_pa = reg_val & MTK_SMMU_STE_ADDR_MASK;
		struct smmuv3_driver *smmuv3 = dev->smmuv3;
		uint64_t host_reg = 0;

		if (!smmuv3) {
			pkvm_smmu_ops->puts(
				"write_guest_strtab_base_reg smmuv3_driver *smmuv3 is NULL");
			return;
		}

		dev->guest_strtab_base_pa = guest_ste_pa;
		/* Save guest strtab_base */
		dev->guest_strtab_base_reg = reg_val;
		/* Prepare host strstab reg value */
		host_reg = get_smmu_contig_mpool_pa(dev, GET_GLOBAL_STE);
		/* keep RA flag if guest has it */
		host_reg |= reg_val & MTK_SMMU_STE_RA_MASK;
		/* Write global_ste_pa instead of guest strtab_base pa */
		writel_64(host_reg,
			  (void *)(dev->reg_base_va_addr + STRTAB_BASE));
		/* for host ste base va */
		smmuv3->base_addr = (void *)dev->reg_base_va_addr;
		smmuv3->strtab_cfg.base =
			get_smmu_contig_mpool_va(dev, GET_GLOBAL_STE);
	} else {
		pkvm_smmu_ops->puts(
			"WARN: ste base only allow write once only!!");
	}
}

static void guest_set_cr0_value(smmu_device_t *dev, uint32_t guest_reg)
{
	uint32_t guest_cr0_val = 0U, host_cr0_val = 0U, combine_cr0_val = 0U,
		 en_bit_mask = 0U;

	if (!dev) {
		pkvm_smmu_ops->puts(
			"guest_set_cr0_value smmu_device_t *dev is NULL");
		return;
	}

	guest_cr0_val = guest_reg;
	host_cr0_val = (uint32_t)readl((void *)(dev->reg_base_va_addr + CR0));
	en_bit_mask = CMDQ_EN | EVTQ_EN | PRIQ_EN | SMMU_EN;
	/* store guest expect value into smmu_device structure */
	dev->guest_cr0_regval = guest_cr0_val;
	/* enable bit only allow enable setting by guest */
	combine_cr0_val = (en_bit_mask & host_cr0_val) | guest_cr0_val;
	writel(combine_cr0_val, (void *)(dev->reg_base_va_addr + CR0));
}

/*
 * Record CR2 value. Therefore, Skip checking register value
 * when broadcast TLBI.
 */
static void guest_set_cr2_value(smmu_device_t *dev, uint32_t guest_reg)
{
	/* Store CR2 value into smmu device structure and set it into hw register */
	dev->cr2_value = guest_reg;
	writel(guest_reg, (void *)(dev->reg_base_va_addr + CR2_REG));
}

static int mmio_write(struct user_pt_regs *regs, smmu_device_t *smmu_dev,
		      u64 esr, size_t reg_offset)
{
	void *reg_address_va;
	int reg, is_64;
	unsigned long reg_val;

	is_64 = (esr & ESR_ELx_SF) >> ESR_ELx_SF_SHIFT;
	reg = (esr & ESR_ELx_SRT_MASK) >> ESR_ELx_SRT_SHIFT;
	reg_address_va = (void *)(smmu_dev->reg_base_va_addr + reg_offset);

	reg_val = (unsigned long)regs->regs[reg];
	if (is_64) {
		/* 64 bit access */
		switch (reg_offset) {
		case CMDQ_BASE:
			write_smmu_cmdq_base_addr(smmu_dev, reg_val);
			break;
		case STRTAB_BASE:
			/* Guest write - strtab_base */
			write_guest_strtab_base_reg(smmu_dev, reg_val);
			break;
		default:
			writel_64(reg_val, reg_address_va);
		}
	} else {
		/* 32 bit access */
		switch (reg_offset) {
		case CMDQ_PROD:
			handle_guest_write_prod(reg_val, smmu_dev);
			break;
		case CR0:
			guest_set_cr0_value(smmu_dev, reg_val);
			break;
		case CR2_REG:
			guest_set_cr2_value(smmu_dev, reg_val);
			break;
		case STRTAB_BASE_CFG:
			if (!STE_2lvl(reg_val)) {
				/* Guest write - strtab_base_cfg */
				write_smmu_strtab_cfg_reg(smmu_dev, reg_val);
			} else
				writel(reg_val, reg_address_va);
			break;
		default:
			writel(reg_val, reg_address_va);
		}
	}
	return 0;
}

/* guest_read_idr1
 * 1. Overwrite sid_bits number to 8, so SMMU use linear STE.
 * 2. Set guest cmdq size(512KB) smaller than host cmdq(1MB).
 */
static uint32_t guest_read_idr1(void *idr1_reg)
{
	uint32_t host_idr1, guest_idr1;

	host_idr1 = readl(idr1_reg);
	guest_idr1 = (((host_idr1 & ~MAKE_MASK(5, 0)) | STE_ENTRY_256) &
		      ~MAKE_MASK(25, 21)) |
		     (MTK_SMMU_GUEST_CMDQ_ENTRY << 21);
	return guest_idr1;
}
/*
 * MTK: don't retry, if cr0ack is not acked, return only.
 * let linux kernel do retry.
 */
static void cr0_reg_polling(smmu_device_t *dev)
{
	bool ack = false;

	if (((uint32_t)readl((void *)(dev->reg_base_va_addr) + CR0)) ==
	    ((uint32_t)readl((void *)(dev->reg_base_va_addr) + CR0ACK)))
		ack = true;
	/* if hw cr0 register sync, then update sw guset_cr0ack value */
	if (ack)
		/* simulate hw cr0ack ack case */
		dev->guest_cr0ack_regval = dev->guest_cr0_regval;
}

static int mmio_read(struct user_pt_regs *regs, smmu_device_t *smmu_dev,
		     u64 esr, size_t reg_offset)
{
	void *reg_address_va;
	u64 read_reg_64;
	u32 read_reg_32;
	int reg, is_64;

	is_64 = (esr & ESR_ELx_SF) >> ESR_ELx_SF_SHIFT;
	reg = (esr & ESR_ELx_SRT_MASK) >> ESR_ELx_SRT_SHIFT;

	reg_address_va = (void *)(smmu_dev->reg_base_va_addr + reg_offset);

	if (is_64) {
		/* 64 bit access */
		read_reg_64 = readl_64(reg_address_va);
		if (reg_offset == CMDQ_BASE)
			read_reg_64 = smmu_dev->guest_cmdq_regval;

		if (reg_offset == STRTAB_BASE)
			read_reg_64 = smmu_dev->guest_strtab_base_reg;

		regs->regs[reg] = read_reg_64;
	} else {
		/* 32 bit access */
		read_reg_32 = readl(reg_address_va);
		if (reg_offset == IDR1) {
			read_reg_32 = guest_read_idr1(reg_address_va);
		} else if (reg_offset == CR0ACK) {
			cr0_reg_polling(smmu_dev);
			read_reg_32 = smmu_dev->guest_cr0ack_regval;
		} else if (reg_offset == CR0) {
			read_reg_32 = smmu_dev->guest_cr0_regval;
		} else if (reg_offset == CMDQ_PROD) {
			read_reg_32 = smmu_dev->guest_prod_reg;
		} else if (reg_offset == CMDQ_CONS) {
			read_reg_32 = smmu_dev->guest_cons_reg;
		}
		regs->regs[reg] = read_reg_32;
	}
	return 0;
}

/* according address to justify the address is belonged in which subsys smmu */
smmu_device_t *get_smmu_dev_from_addr(u64 addr)
{
	unsigned long reg_base;
	unsigned int reg_size, subsys_smmu;

	smmu_device_t *smmu_dev = NULL;

	for (subsys_smmu = 0; subsys_smmu < smmu_devices_count; subsys_smmu++) {
		smmu_dev = smmu_devices[subsys_smmu];
		reg_base = smmu_dev->reg_base_pa_addr;
		reg_size = smmu_dev->reg_size;
		if ((reg_base <= addr) && (addr < (reg_base + reg_size)))
			return smmu_dev;
	}
	return NULL;
}

void dvm_mi_reg_mapping(struct user_pt_regs *regs)
{
	unsigned long reg_base_address = regs->regs[1];
	unsigned int reg_size = regs->regs[2];
	unsigned long private_mappings_va = 0UL;
	int ret = 0;

	regs->regs[0] = SMCCC_RET_SUCCESS;
	ret = pkvm_smmu_ops->create_private_mapping(reg_base_address, reg_size,
				  PAGE_HYP_DEVICE, &private_mappings_va);
	if (!ret) {
		pkvm_smmu_ops->puts("dvm_mi_reg_mapping success");
		dvm_mi_va_base = (void *)private_mappings_va;
	}
}

void setup_vm(struct user_pt_regs *regs)
{
	struct smmu_hyp_vms *smmu_hyp;
	struct smmu_vm *vm;
	unsigned int ops, vmid, sid, map_mode, granule;
	unsigned long mem_base, mem_size;

	ops = regs->regs[1];
	regs->regs[0] = SMCCC_RET_SUCCESS;

	switch (ops) {
	case DEFAULT_VMID:
		smmu_hyp = get_vms_data();
		vmid = regs->regs[2];
		smmu_hyp->default_vmid = vmid;
		break;
	case VM_NO_MAP_MBLOCK:
		mem_base = regs->regs[2];
		mem_size = regs->regs[3];
		add_to_nomap_region(mem_base, mem_size);
		break;
	case S2_BYPASS_SID:
		smmu_hyp = get_vms_data();
		sid = regs->regs[2];
		smmu_hyp->s2_bypass_sid[sid] = 1;
		break;
	case VMID:
		vmid = regs->regs[2];
		vm = get_vm(vmid);
		vm->vmid = vmid;
		break;
	case SID:
		vmid = regs->regs[2];
		sid = regs->regs[3];
		vm = get_vm(vmid);
		vm->sids[vm->sid_num] = sid;
		vm->sid_num++;
		break;
	case IDENTITY_MAP_MODE:
		vmid = regs->regs[2];
		vm = get_vm(vmid);
		map_mode = regs->regs[3];
		vm->identity_map_mode = map_mode;
		break;
	case IDENTITY_MAP:
		vmid = regs->regs[2];
		mem_base = regs->regs[3];
		mem_size = regs->regs[4];
		vm = get_vm(vmid);
		add_to_map_region(vm, mem_base, mem_size);
		vm->vm_ipa_range.base = mem_base;
		vm->vm_ipa_range.size = mem_size;
		break;
	case IDENTITY_MAP_MBLOCK:
		vmid = regs->regs[2];
		mem_base = regs->regs[3];
		mem_size = regs->regs[4];
		vm = get_vm(vmid);
		add_to_map_region(vm, mem_base, mem_size);
		break;
	case IDENTITY_UNMAP:
	case IDENTITY_UNMAP_MBLOCK:
		vmid = regs->regs[2];
		mem_base = regs->regs[3];
		mem_size = regs->regs[4];
		vm = get_vm(vmid);
		add_to_unmap_region(vm, mem_base, mem_size);
		break;
	case IDENTITY_MAP_MBLOCK_EXCLUSIVE:
		vmid = regs->regs[2];
		mem_base = regs->regs[3];
		mem_size = regs->regs[4];
		vm = get_vm(vmid);
		add_to_exclusive_map_region(vm->vmid, mem_base, mem_size,
					    vm->identity_map_mode);
		break;
	case IPA_GRANULE:
		vmid = regs->regs[2];
		granule = regs->regs[3];
		vm = get_vm(vmid);
		vm->ipa_granule = granule;
		break;
	default:
		break;
	}
}

void mtk_iommu_init(struct user_pt_regs *regs)
{
	struct mpt in_mpt;
	u64 *smpt = NULL;
	unsigned long pa = 0UL;
	unsigned int i = 0U;
	int ret = 0;
	void *pglist_pa = NULL;
	uint64_t pglist_pfn = 0ULL;
	void *pmm_page = NULL;

	regs->regs[0] = SMCCC_RET_SUCCESS;
	pglist_pfn = regs->regs[1];
	if (!share_memory_to_hyp(pglist_pfn << ONE_PAGE_OFFSET, ONE_PAGE_SIZE)) {
		pkvm_smmu_ops->puts("mtk_iommu_init : share fail");
		goto error;
	}

	pglist_pa = (void *)(pglist_pfn << ONE_PAGE_OFFSET);
	pmm_page = (void *)pkvm_smmu_ops->hyp_va((phys_addr_t)pglist_pa);

	memcpy(&in_mpt, pmm_page, sizeof(in_mpt));
	/* donate smmu mpool memory to hypervisor */
	for (i = 0; i < in_mpt.mem_block_num; i++) {
		smpt = kern_hyp_va(in_mpt.fmpt[i].smpt);
		pa = __hyp_pa(smpt);
		ret = __pkvm_host_donate_hyp(pa >> PAGE_SHIFT,
					     (1 << in_mpt.fmpt[i].mem_order));
		if (ret)
			pkvm_smmu_ops->puts(
				"mtk_iommu_init : host_donate_hyp fail");
		else {
			hyp_mpool.fmpt[i].smpt = smpt;
			hyp_mpool.fmpt[i].mem_order = in_mpt.fmpt[i].mem_order;
		}
	}

	if (!unshare_memory_from_hyp(pglist_pfn << ONE_PAGE_OFFSET, ONE_PAGE_SIZE))
		pkvm_smmu_ops->puts("mtk_iommu_init : unshare fail");

	/* mpool init */
	smmu_map_mpool();
	/* add memory into mpool */
	for (i = 0; i < in_mpt.mem_block_num; i++) {
		if (hyp_mpool.fmpt[i].smpt) {
			add_mem_to_mpool(hyp_mpool.fmpt[i].smpt,
					 (1U << hyp_mpool.fmpt[i].mem_order) *
						 SZ_4KB);
			/* To prevent IO masters are able to access smmu mpool memory */
			add_to_nomap_region(
				__hyp_pa(hyp_mpool.fmpt[i].smpt),
				(1U << hyp_mpool.fmpt[i].mem_order) * SZ_4KB);
		} else {
			pkvm_smmu_ops->puts(
				"mtk_iommu_init : add_mem_to_mpool : smpt is NULL");
		}
	}
	/* malloc init */
	ret = malloc_init((struct pkvm_module_ops *)pkvm_smmu_ops);
	if (ret) {
		pkvm_smmu_ops->puts("mtk_iommu_init : malloc init fail");
		goto error;
	}

	smmu_mem_init(0);
	/* flush all mpool memory */
	for (i = 0; i < in_mpt.mem_block_num; i++) {
		if (hyp_mpool.fmpt[i].smpt)
			smmu_flush_dcache(hyp_mpool.fmpt[i].smpt,
					  (1U << hyp_mpool.fmpt[i].mem_order) *
						  SZ_4KB);
		else {
			pkvm_smmu_ops->puts(
				"mtk_iommu_init : smmu_flush_dcache : smpt is NULL");
		}
	}
	smmu_dump_all_vm_stage2();

error:
	regs->regs[1] = ret;
}

static bool mtk_iommu_host_dabt_handler(struct user_pt_regs *regs, u64 esr,
				       u64 addr)
{
	smmu_device_t *smmu_dev;
	bool is_write;
	size_t off;
	u64 far;

	far = read_sysreg_el2(SYS_FAR);
	addr |= far & FAR_MASK;

	smmu_dev = get_smmu_dev_from_addr(addr);
	if (!smmu_dev)
		return false;

	is_write = esr & ESR_ELx_WNR;
	off = addr - smmu_dev->reg_base_pa_addr;

	if (is_write)
		mmio_write(regs, smmu_dev, esr, off);
	else
		mmio_read(regs, smmu_dev, esr, off);

	return true;
}

static int mtk_smmu_init(void)
{
	return 0;
}

static struct kvm_hyp_iommu *mtk_smmu_id_to_iommu(pkvm_handle_t smmu_id)
{
	return 0;
}

int mtk_smmu_alloc_domain(struct kvm_hyp_iommu_domain *domain, int type)
{
	return 0;
}

void mtk_smmu_free_domain(struct kvm_hyp_iommu_domain *domain)
{
}

static int mtk_smmu_attach_dev(struct kvm_hyp_iommu *iommu, struct kvm_hyp_iommu_domain *domain,
			   u32 sid, u32 pasid, u32 pasid_bits, unsigned long flags)
{
	return 0;
}

static int mtk_smmu_detach_dev(struct kvm_hyp_iommu *iommu, struct kvm_hyp_iommu_domain *domain,
			   u32 sid, u32 pasid)
{
	return 0;
}

bool mtk_smmu_dabt_handler(struct user_pt_regs *regs, u64 esr, u64 addr)
{
	return mtk_iommu_host_dabt_handler(regs, esr, addr);
}

int mtk_smmu_suspend(struct kvm_hyp_iommu *iommu)
{
	return 0;
}

int mtk_smmu_resume(struct kvm_hyp_iommu *iommu)
{
	return 0;
}

static void mtk_smmu_iotlb_sync(struct kvm_hyp_iommu_domain *domain,
			    struct iommu_iotlb_gather *gather)
{
}
/* Checking idmap range is in the range of vm's ipa range, otherwise return false */
bool kvm_iommu_idmap_range_check(phys_addr_t start, phys_addr_t end,
				 unsigned int vmid)
{
	struct smmu_vm *vm;

	vm = get_vm(vmid);
	if (!vm)
		return false;

	return	address_vm_range_check(vm, start, end);
}
/* According to snapshot status, change protected VM permission mapping */
static bool snapshot_done;

static void mtk_smmu_host_stage2_idmap(struct kvm_hyp_iommu_domain *domain,
				       phys_addr_t start, phys_addr_t end,
				       int prot)
{
	phys_addr_t paddr;
	uint32_t size;

	paddr = start;
	size = end - start;
	if ((kvm_iommu_idmap_range_check(start, end, 0) == false) ||
	    (kvm_iommu_idmap_range_check(start, end, 1) == false))
		return;

	hyp_spin_lock(&smmu_all_vm_lock);
	smmu_lazy_free();

	if (!prot) {
		/* unmap vm */
		smmu_vm_unmap(0, paddr, size);
	} else {
		/* return page */
		if ((prot & KVM_PGTABLE_PROT_R) ||
		    (prot & KVM_PGTABLE_PROT_W)) {
			smmu_vm_map(0, paddr, size,
				    MM_MODE_R | MM_MODE_W | MM_MODE_X);
		}
	}

	hyp_spin_unlock(&smmu_all_vm_lock);
}

static void mtk_smmu_host_stage2_idmap_complete(bool map)
{
	if (!map)
		mtk_smmu_sync();
}

void smmu_finalise(struct user_pt_regs *regs)
{
	int ret;

	ret = kvm_iommu_snapshot_host_stage2(NULL);
	regs->regs[0] = ret;
	snapshot_done = true;
}

struct kvm_iommu_ops smmu_ops = {
	.init				= mtk_smmu_init,
	.get_iommu_by_id		= mtk_smmu_id_to_iommu,
	.alloc_domain			= mtk_smmu_alloc_domain,
	.free_domain			= mtk_smmu_free_domain,
	.attach_dev			= mtk_smmu_attach_dev,
	.detach_dev			= mtk_smmu_detach_dev,
	.dabt_handler			= mtk_smmu_dabt_handler,
	.suspend			= mtk_smmu_suspend,
	.resume				= mtk_smmu_resume,
	.iotlb_sync			= mtk_smmu_iotlb_sync,
	.host_stage2_idmap		= mtk_smmu_host_stage2_idmap,
	.host_stage2_idmap_complete     = mtk_smmu_host_stage2_idmap_complete,
};

int smmu_hyp_init(const struct pkvm_module_ops *ops)
{
	int ret;

	pkvm_smmu_ops = ops;

	ret = register_hyp_pmm_hal();
	if (ret)
		pkvm_smmu_ops->puts("SMMU: register pmm hal failed");

	return ret;
}
