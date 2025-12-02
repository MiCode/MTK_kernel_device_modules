// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <asm/kvm_pkvm_module.h>
#include <linux/arm-smccc.h>
#include "include/hyp_pmm.h"

#define MAX_PMM_HALS 8
#define TRACE 0

#ifdef memset
#undef memset
#endif

extern const struct pkvm_module_ops *pkvm_ops;
static void *pmm_hals[MAX_PMM_HALS];

#undef cmpxchg64_release
#define cmpxchg64_release	__lse__cmpxchg_case_rel_64
//#define cmpxchg64_release	__ll_sc__cmpxchg_case_rel_64

#define DEBUG_DYNAMIC_PROTECT	1

#define DEBUG_HYP_PMM	0

/*
    PMM_MSG_ENTRY format
    page number = PA >> hyp_pmm_el1_page_size_shift()
     _______________________________________
    |  reserved  | page order | page number |
    |____________|____________|_____________|
    31         28 27        24 23          0
*/
#define PMM_MSG_ORDER_SHIFT (24UL)
#define PMM_MSG_PFN_MASK ((1UL << PMM_MSG_ORDER_SHIFT) - 1)
#define GET_PMM_ENTRY_ORDER(entry) ((entry >> PMM_MSG_ORDER_SHIFT) & 0xf)
#define GET_PMM_ENTRY_PFN(entry) ((u64)(entry & PMM_MSG_PFN_MASK))

#define MAX_RECORD_TIMES        (100)
#define CPU_PGTBL_BASE_OFFSET   (0)
#define GPU_PGTBL_BASE_OFFSET   (CPU_PGTBL_BASE_OFFSET + MAX_RECORD_TIMES)
#define IOMMU_PGTBL_BASE_OFFSET (GPU_PGTBL_BASE_OFFSET + MAX_RECORD_TIMES)
#define IMPU_PGTBL_BASE_OFFSET  (IOMMU_PGTBL_BASE_OFFSET + MAX_RECORD_TIMES)
#define SMMU_PGTBL_BASE_OFFSET  (IMPU_PGTBL_BASE_OFFSET + MAX_RECORD_TIMES)
#define SEC_TO_US               (1000000)

static u64 *debug_page_va;
static u32 cur_idx;

#define for_each_pmm_hal(hal, i)				\
	for ((i) = 0, hal = (typeof(hal))__get_pmm_hal(0);	\
	     hal;						\
	     hal = (typeof(hal))__get_pmm_hal(++(i)))

static void *__get_pmm_hal(int i)
{
	if (i >= MAX_PMM_HALS)
		return NULL;

	i = array_index_nospec(i, MAX_PMM_HALS);

	return pmm_hals[i];
}

static int share_pmm_ipc(u64 pfn)
{
	int ret;
	void *va;

	ret = pkvm_ops->host_share_hyp(pfn);
	if (ret) {
		pkvm_ops->puts("share pmm_ipc failed");
		pkvm_ops->putx64(pfn);
		WARN_ON(ret != 0);
		return ret;
	}

	va = pkvm_ops->hyp_va(pfn << PAGE_SHIFT);
	ret = pkvm_ops->pin_shared_mem(va, va + PAGE_SIZE);
	if (ret) {
		pkvm_ops->puts("pin pmm_ipc failed");
		pkvm_ops->putx64(pfn);
		WARN_ON(ret != 0);
		return ret;
	}

	return 0;
}

static int unshare_pmm_ipc(u64 pfn)
{
	int ret;
	void *va = pkvm_ops->hyp_va(pfn << PAGE_SHIFT);

	pkvm_ops->unpin_shared_mem(va, va + PAGE_SIZE);

	ret = pkvm_ops->host_unshare_hyp(pfn);
	if (ret) {
		pkvm_ops->puts("unshare pmm_ipc failed");
		pkvm_ops->putx64(pfn);
		WARN_ON(ret != 0);
		return ret;
	}

	return 0;
}

static void pmm_poison_pages(u32 *ipc, u32 count)
{
	u32 entry, order, idx;
	u64 pa;
	void *va;
	u32 nr_pages;

	/* processing for each page from ipc */
	for (idx = 0; idx < count; idx++) {
		entry = ipc[idx];
		order = GET_PMM_ENTRY_ORDER(entry);
		pa = GET_PMM_ENTRY_PFN(entry) << PAGE_SHIFT;
		nr_pages = 1UL << order;

		while (nr_pages--) {
			va = pkvm_ops->fixmap_map(pa);
			pa += PAGE_SIZE;
			pkvm_ops->memset(va, 0, PAGE_SIZE);
			pkvm_ops->flush_dcache_to_poc(va, PAGE_SIZE);
			pkvm_ops->fixmap_unmap();
		}
	}
}

static int hal_secure_pages_v2(struct pmm_hal *hal,
			       u32 *ipc, u32 attr, u32 count, bool lock)
{
	u32 entry, order, idx;
	u64 pa;
	int ret = 0;

	/* secure_v2 and unsecure_v2 must be implemented */
	if (!hal->secure_v2 || !hal->unsecure_v2)
		return -EINVAL;

	if (hal->prepare)
		hal->prepare();

	/* processing for each page from ipc */
	for (idx = 0; idx < count; idx++) {
		entry = ipc[idx];
		order = GET_PMM_ENTRY_ORDER(entry);
		pa = GET_PMM_ENTRY_PFN(entry) << PAGE_SHIFT;
		if (lock)
			ret = hal->secure_v2(pa, order, attr);
		else
			ret = hal->unsecure_v2(pa, order, attr);

		if (ret) {
			pkvm_ops->puts("hal_secure_pages_v2 failed{");
			if (lock)
				pkvm_ops->puts("secure");
			else
				pkvm_ops->puts("unsecure");
			pkvm_ops->puts(hal->name);
			pkvm_ops->putx64(ret);
			pkvm_ops->putx64(pa);
			pkvm_ops->putx64((u64)order);
			pkvm_ops->putx64((u64)attr);
			pkvm_ops->puts("}");
			break;
		}
	}

	if (hal->sync)
		hal->sync();

	return ret;
}

#if (DEBUG_DYNAMIC_PROTECT == 1)
static int compare_name(const char *s1, const char *s2)
{
	while (*s1 != '\0' && *s1 == *s2) {
		s1++;
		s2++;
	}
	return (int)(*(unsigned char *)s1) - (int)(*(unsigned char *)s2);
}

static bool share_memory_to_hyp(uint64_t region_start, uint64_t region_size)
{
	int ret = 0;
	uint64_t share_idx, share_abort_idx, region_pfn = region_start >> PAGE_SHIFT,
	  pfn_total = region_size >> PAGE_SHIFT;

	for (share_idx = 0; share_idx < pfn_total; share_idx++) {
		/* host_share_hyp's input parameter is pfn no matter kernel pa or hyp pa */
		ret = pkvm_ops->host_share_hyp(region_pfn + share_idx);

		if (ret) {
			pkvm_ops->puts("host_share_hyp fail");
			goto share_abort_handle;
		}
	}
	/* Pin those shared memory pages to hyp */
	ret = pkvm_ops->pin_shared_mem(
		(void *)(pkvm_ops->hyp_va((phys_addr_t)region_start)),
		(((void *)pkvm_ops->hyp_va((phys_addr_t)region_start)) +
		 region_size));

	if (ret) {
		pkvm_ops->puts("pin_shared_mem fail");
		goto share_abort_handle;
	}

	return true;

share_abort_handle:
	/* Abort this memory share operation */
	for (share_abort_idx = 0; share_abort_idx < share_idx;
		 share_abort_idx++) {
		ret = pkvm_ops->host_unshare_hyp(region_pfn + share_abort_idx);
		if (ret) {
			pkvm_ops->puts("host_unshare_hyp fail");
			break;
		}
	}
	return false;
}

static void read_systimer_to_us(u64 *time_us)
{
	u64 cnt = __arch_counter_get_cntvct();

	*time_us = cnt * SEC_TO_US / arch_timer_get_cntfrq();
}

static void record_pgtbl_time(u64 t1_us, u64 t2_us, const char *name, u32 idx)
{
	u32 offset;

	if (!compare_name(name, "cpu"))
		offset = CPU_PGTBL_BASE_OFFSET;
	else if (!compare_name(name, "gpu-mpu"))
		offset = GPU_PGTBL_BASE_OFFSET;
	else if (!compare_name(name, "mtk-iommu"))
		offset = IOMMU_PGTBL_BASE_OFFSET;
	else if (!compare_name(name, "infra-mpu"))
		offset = IMPU_PGTBL_BASE_OFFSET;
	else if (!compare_name(name, "mtk-smmu"))
		offset = SMMU_PGTBL_BASE_OFFSET;
	else
		return;

	debug_page_va[offset + idx] = t2_us - t1_us;
}

static void recalc_pgtbl_idx(void)
{
	cur_idx++;

	if (cur_idx == MAX_RECORD_TIMES)
		cur_idx = 0;
}
void hyp_pmm_debug_hypmmu(struct user_pt_regs *regs)
{
	unsigned int cmd = regs->regs[1];
	char *dis_name;
	struct pmm_hal *hal;
	int i;
	bool enabled;
	uint64_t debug_page_pa, total_page;

	if (cmd == INIT_DEBUG_PAGE) {
		debug_page_pa = regs->regs[2] << PAGE_SHIFT;
		total_page = (1 << regs->regs[3]) * PAGE_SIZE;
		if (!share_memory_to_hyp(debug_page_pa, total_page))
			goto out;

		debug_page_va = (void *)pkvm_ops->hyp_va((phys_addr_t)debug_page_pa);

		pkvm_ops->memset(debug_page_va, 0, total_page);
		cur_idx = 0;
		goto out;
	}

	if (cmd == RESET_PGTBL_TIME) {
		cur_idx = 0;
		total_page = (1 << regs->regs[2]) * PAGE_SIZE;
		if (debug_page_va)
			pkvm_ops->memset(debug_page_va, 0, total_page);
		goto out;
	}

	switch (cmd) {
	case DISABLE_CPU_PROTECTION:
		dis_name = "cpu";
		enabled = false;
		break;
	case ENABLE_CPU_PROTECTION:
		dis_name = "cpu";
		enabled = true;
		break;
	case DISABLE_GPU_PROTECTION:
		dis_name = "gpu-mpu";
		enabled = false;
		break;
	case ENABLE_GPU_PROTECTION:
		dis_name = "gpu-mpu";
		enabled = true;
		break;
	case DISABLE_INFRA_MPU_PROTECTION:
		dis_name = "infra-mpu";
		enabled = false;
		break;
	case ENABLE_INFRA_MPU_PROTECTION:
		dis_name = "infra-mpu";
		enabled = true;
		break;
	case DISABLE_SMMU_PROTECTION:
		dis_name = "mtk-smmu";
		enabled = false;
		break;
	case ENABLE_SMMU_PROTECTION:
		dis_name = "mtk-smmu";
		enabled = true;
		break;
	case DUMP_PROTECTION_STATUS:
		dis_name = "";
		break;
	default:
		pkvm_ops->puts("pmm_debug_hypmmu: No match");
		dis_name = "";
		break;
	}

	for_each_pmm_hal(hal, i) {
		pkvm_ops->puts(hal->name);
		if (!compare_name(hal->name, dis_name))
			hal->is_enabled = enabled;

		if (cmd == DUMP_PROTECTION_STATUS) {
			pkvm_ops->puts("Protection Status:");
			pkvm_ops->puts(hal->name);
			pkvm_ops->putx64(hal->is_enabled);
		}
	}

out:
	regs->regs[0] = SMCCC_RET_SUCCESS;
	regs->regs[1] = 0;
}
#else
static void recalc_pgtbl_idx(void)
{
}

static void read_systimer_to_us(u64 *time_us)
{
}

static void record_pgtbl_time(u64 t1_us, u64 t2_us, const char *name, u32 idx)
{
}

void hyp_pmm_debug_hypmmu(struct user_pt_regs *regs)
{
	debug_page_va = NULL;
	regs->regs[0] = SMCCC_RET_INVALID_PARAMETER;
}
#endif

static int secure_pages_v2(u64 ipc_pfn, u32 attr, u32 count, bool lock)
{
	struct pmm_hal *hal;
	void *ipc_va;
	int i, ret = 0;
	u64 pre_time_us, cur_time_us, pgtbl_idx = cur_idx;

#if (DEBUG_HYP_PMM == 1)
	if (lock == true)
		pkvm_ops->puts("HYP_PMM: secure_pages_v2{");
	else
		pkvm_ops->puts("HYP_PMM: unsecure_pages_v2{");
	pkvm_ops->putx64(ipc_pfn);
	pkvm_ops->putx64((u64)attr);
	pkvm_ops->putx64((u64)count);
	pkvm_ops->puts("}");
#endif
	if (!ipc_pfn)
		return -EINVAL;

	if (!count)
		return -EINVAL;

	ret = share_pmm_ipc(ipc_pfn);
	if (ret)
		goto out;
	ipc_va = pkvm_ops->hyp_va(ipc_pfn << PAGE_SHIFT);

	/* Poison(clean) pages before unlock */
	if (lock == false)
		pmm_poison_pages(ipc_va, count);

	for_each_pmm_hal(hal, i) {
		if (!hal->is_enabled)
			continue;

		read_systimer_to_us(&pre_time_us);

		ret = hal_secure_pages_v2(hal, ipc_va, attr, count, lock);
		if (ret) {
			pkvm_ops->puts("hal_secure_pages_v2 failed");
			pkvm_ops->puts(hal->name);
			pkvm_ops->putx64(ret);
			WARN_ON(ret != 0);
			break;
		}

		read_systimer_to_us(&cur_time_us);
		record_pgtbl_time(pre_time_us, cur_time_us, hal->name, pgtbl_idx);
	}
	recalc_pgtbl_idx();

	ret = unshare_pmm_ipc(ipc_pfn);
out:
	return ret;
}

void hyp_pmm_assign_buffer_v2(struct user_pt_regs *regs)
{
	u64 ipc_pfn = regs->regs[1];
	u32 attr = regs->regs[2];
	u32 count = regs->regs[3];
	int ret;

#if (DEBUG_HYP_PMM == 1)
	pkvm_ops->puts("HYP_PMM: assign buffer");
	pkvm_ops->putx64(ipc_pfn);
#endif
	ret = secure_pages_v2(ipc_pfn, attr, count, true);
	if (ret) {
		pkvm_ops->puts("hyp_pmm_assign_buffer_v2 failed");
		regs->regs[0] = ret;
		WARN_ON(ret != 0);
		return;
	}

	regs->regs[0] = SMCCC_RET_SUCCESS;
	regs->regs[1] = 0;
}

void hyp_pmm_unassign_buffer_v2(struct user_pt_regs *regs)
{
	u64 ipc_pfn = regs->regs[1];
	u32 attr = regs->regs[2];
	u32 count = regs->regs[3];
	int ret;

#if (DEBUG_HYP_PMM == 1)
	pkvm_ops->puts("HYP_PMM: unassign buffer");
	pkvm_ops->putx64(ipc_pfn);
#endif
	ret = secure_pages_v2(ipc_pfn, attr, count, false);
	if (ret) {
		pkvm_ops->puts("hyp_pmm_unassign_buffer_v2 failed");
		regs->regs[0] = ret;
		WARN_ON(ret != 0);
		return;
	}

	regs->regs[0] = SMCCC_RET_SUCCESS;
	regs->regs[1] = 0;
}

void hyp_pmm_defragment(struct user_pt_regs *regs)
{
	struct pmm_hal *hal;
	int i;

#if (DEBUG_HYP_PMM == 1)
	pkvm_ops->puts("defragment{}");
#endif

	for_each_pmm_hal(hal, i) {
		if (hal->defragment)
			hal->defragment();
	}
	regs->regs[0] = SMCCC_RET_SUCCESS;
	regs->regs[1] = 0;
}

static void secure_range(u64 pa, u64 size, u8 attr, bool lock)
{
	struct pmm_hal *hal;
	int i;
	u64 pre_time_us, cur_time_us, pgtbl_idx = cur_idx;

	for_each_pmm_hal(hal, i) {
		if (!hal->is_enabled)
			continue;

		pkvm_ops->puts(hal->name);

#if (DEBUG_HYP_PMM == 1)
		if (lock)
			pkvm_ops->puts("secure range pa size attr lock");
		else
			pkvm_ops->puts("unsecure range pa size attr lock");

		pkvm_ops->putx64(pa);
		pkvm_ops->putx64(size);
		pkvm_ops->putx64((u64)attr);
		pkvm_ops->putx64((u64)lock);
#endif
		read_systimer_to_us(&pre_time_us);

		if (lock) {
			if (hal->secure_range)
				hal->secure_range(pa, size, attr);
		} else {
			if (hal->unsecure_range)
				hal->unsecure_range(pa, size, attr);
		}

		read_systimer_to_us(&cur_time_us);
		record_pgtbl_time(pre_time_us, cur_time_us, hal->name, pgtbl_idx);
	}
	recalc_pgtbl_idx();
}

void hyp_pmm_secure_range(u64 pa, u64 size, u8 attr)
{
	secure_range(pa, size, attr, true);
}

void hyp_pmm_unsecure_range(u64 pa, u64 size, u8 attr)
{
	secure_range(pa, size, attr, false);
}

static void kvm_secure_pages_common(u32 *ipc, u32 count, u8 attr)
{
	u32 i, entry, order, idx;
	u64 pa;
	struct pmm_hal *hal;
	u64 pre_time_us, cur_time_us, pgtbl_idx = cur_idx;

	for_each_pmm_hal(hal, i) {
		/* skip cpu hal, only non-cpu for kvm iommu */
		if (i == 0)
			continue;

		if (!hal->is_enabled)
			continue;

		if (hal->prepare)
			hal->prepare();

		read_systimer_to_us(&pre_time_us);

		/* processing for each page from ipc */
		for (idx = 0; idx < count; idx++) {
			entry = ipc[idx];
			order = GET_PMM_ENTRY_ORDER(entry);
			pa = GET_PMM_ENTRY_PFN(entry) << PAGE_SHIFT;
			if (attr)
				hal->secure_v2(pa, order, attr);
			else
				hal->unsecure_v2(pa, order, attr);
		}
		read_systimer_to_us(&cur_time_us);
		record_pgtbl_time(pre_time_us, cur_time_us, hal->name, pgtbl_idx);

		if (hal->sync)
			hal->sync();
	}
	recalc_pgtbl_idx();
}

void hyp_pmm_kvm_secure_pages(u32 *pmm_ipc, u32 count, u8 attr)
{
	kvm_secure_pages_common(pmm_ipc, count, attr);
}

void hyp_pmm_kvm_unsecure_pages(u32 *pmm_ipc, u32 count, u8 attr)
{
	kvm_secure_pages_common(pmm_ipc, count, attr);
}

static int hal_register(void *hal)
{
	int i;

	for (i = 0; i < MAX_PMM_HALS ; i++) {
		if (!cmpxchg64_release((void *)&pmm_hals[i], 0, (u64)hal)) {
			pmm_hals[i] = hal;
			return 0;
		}
	}
	pkvm_ops->puts("hal_register failed");
	WARN_ON(1);
	return -EBUSY;
}

int hyp_pmm_init(void)
{
	return 0;
}


int hyp_pmm_hal_register(struct pmm_hal *hal)
{
	hal->is_enabled = true;
	hal_register(hal);
	pkvm_ops->puts("HYP_PMM: hal register");
	pkvm_ops->puts(hal->name);
	return 0;
}
