// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <asm/kvm_pkvm_module.h>
#include <nvhe/spinlock.h>
#include <mtk-iommu-defines.h>
#include "include/hypmmu.h"
#include "include/mtk-iommu.h"

static u64 ac_table_pa;
static u32 ac_table_size;
static void *ac_table_va;
static u64 rmem_base;
static u64 rmem_size;
static void *rmem_base_va;
static u32 enable_jpeg_prot_2;
static u64 infra_mpu_percpu_ipc_pa;
static void *infra_mpu_percpu_ipc_va;

static DEFINE_HYP_SPINLOCK(ac_table_lock);

static struct mtk_iommu_device devices[MAX_IOMMU_DEVICES];

/*  MTK-IOMMU (page-base)
 *  +------------------------------------+
 *  | protect mm pgd (64kb)              |
 *  | protect apu pgd (64kb)             |
 *  | protect periph pgd (64kb)          |
 *  | normal periph pgd (64kb)           |
 *  | reserved (64kb)                    |
 *  | translation fault  (512 bytes * 3) |
 *  | reserved                           | 1MB
 *  +------------------------------------+
 *  | mm hw pgd (64kb)                   |
 *  | apu hw pgd (64kb)                  |
 *  | infra-mpu percpu ipc 8 * 32KB (tfa)|
 *  | reserved                           | 1MB
 *  +------------------------------------+
 *  | v7s page pool                      |
 *  |                                    |
 *  |                                    | 32MB
 *  +------------------------------------+
 */

#define PROT_MM_PGD_OFFSET	(0)
#define PROT_APU_PGD_OFFSET	(SZ_64K)
#define HW_MM_PGD_OFFSET	(SZ_1M)
#define HW_APU_PGD_OFFSET	(HW_MM_PGD_OFFSET + (SZ_64K))
#define V7S_PAGE_NODES_OFFSET	(HW_APU_PGD_OFFSET + (SZ_64K))
#define PAGE_POOL_OFFSET	(SZ_2M)

u64 guest_pgd_pa[MAX_TABLE_NUM];
u64 hw_pgd_pa[MAX_TABLE_NUM];
u64 prot_pgd_pa[MAX_TABLE_NUM];
u64 page_pool_base;
u64 page_pool_size;

u64 get_page_pool_base(void)
{
	return page_pool_base;
}

u64 get_page_pool_size(void)
{
	return page_pool_size;
}

static void static_alloc(void)
{
	prot_pgd_pa[0] = rmem_base + PROT_MM_PGD_OFFSET;
	prot_pgd_pa[1] = rmem_base + PROT_APU_PGD_OFFSET;
	hw_pgd_pa[0] = rmem_base + HW_MM_PGD_OFFSET;
	hw_pgd_pa[1] = rmem_base + HW_APU_PGD_OFFSET;
	infra_mpu_percpu_ipc_pa = rmem_base + V7S_PAGE_NODES_OFFSET;
	page_pool_base = rmem_base + PAGE_POOL_OFFSET;
	page_pool_size = rmem_size - PAGE_POOL_OFFSET;

#if (DEBUG_IOMMU)
	MOD_PUTS4("prot hw pgd", prot_pgd_pa[0], prot_pgd_pa[1], hw_pgd_pa[0], hw_pgd_pa[1]);
	MOD_PUTS2("page_pool base size", page_pool_base, page_pool_size);
#endif
}

bool is_enable_jpeg_prot_2(void)
{
	return (enable_jpeg_prot_2) ? true : false;
}

void set_guest_pgd(u64 pa, u32 table_id)
{
	if (table_id >= MAX_TABLE_NUM) {
		WARN_ON(1);
		return;
	}
	guest_pgd_pa[table_id] = pa;
}

u64 query_guest_pgd(u32 table_id)
{
	if (table_id >= MAX_TABLE_NUM) {
		WARN_ON(1);
		return 0;
	}
	return guest_pgd_pa[table_id];
}

u64 query_hw_pgd(u32 table_id)
{
	if (table_id >= MAX_TABLE_NUM) {
		WARN_ON(1);
		return 0;
	}
	return hw_pgd_pa[table_id];
}

u64 query_prot_pgd(u32 table_id)
{
	if (table_id >= MAX_TABLE_NUM) {
		WARN_ON(1);
		return 0;
	}
	return prot_pgd_pa[table_id];
}

/*
 *   7      4 3      0
 *  +--------+--------+
 *  | pfn#1  | pfn#0  |
 *  | attr   | attr   |
 *  +--------+--------+
 *
 *  one entry value is consist of two pages attributes
 */
static void set_ac_attr_by_pfn(u64 pfn, u8 attr)
{
	u8 old, new;
	u8 *ac_table = (u8 *)ac_table_va;
	u64 idx = pfn >> 1;

	old = ac_table[idx];

	if (pfn & 0x1)
		/* update high 4 bits attribute */
		new = (attr << 4 | (old & 0xf));
	else
		/* update low 4 bits attribute */
		new = ((old & 0xf0) | attr);
	ac_table[idx] = new;
}

u8 get_ac_attr_by_pfn(u64 pfn)
{
	u64 idx = pfn >> 1;
	u8 *ac_table = (u8 *)ac_table_va;
	u8 tmp, attr;

	tmp = ac_table[idx];
	if (pfn & 0x1)
		/* extract attr from high 4-bits */
		attr = tmp >> 4;
	else
		/* extract attr from low 4-bits */
		attr = tmp & 0xf;
	return attr;
}

u8 get_ac_attr(u64 pa)
{
	u8 ret;

	hyp_spin_lock(&ac_table_lock);
	ret = get_ac_attr_by_pfn(pa >> ARM_V7S_PAGE_SHIFT);
	hyp_spin_unlock(&ac_table_lock);

	return ret;
}

void set_ac_attr(u64 pa, u64 size, u8 attr)
{
	u64 pfn = pa >> ARM_V7S_PAGE_SHIFT;
	u64 nr_pages = size >> ARM_V7S_PAGE_SHIFT;

	hyp_spin_lock(&ac_table_lock);

	for (u64 i = 0; i < nr_pages; i++)
		set_ac_attr_by_pfn(pfn + i, attr);

	hyp_spin_unlock(&ac_table_lock);
}

static u32 attr_to_srinfo(u8 attr)
{
	u32 sr_info = MTK_IOMMU_SR_INFO_DENY;

	if (attr == HYP_PMM_ATTR_AP_MD_SHM)
		sr_info = 0;
	if (attr == HYP_PMM_ATTR_AP_SCP_SHM)
		sr_info = 0;
	if (attr == HYP_PMM_ATTR_WFD)
		sr_info = 11;
	if (attr == HYP_PMM_ATTR_SVP)
		sr_info = 8;
	if (attr == HYP_PMM_ATTR_PROT_MEM)
		sr_info = 10;
	if (attr == HYP_PMM_ATTR_SAPU_DATA_SHM)
		sr_info = 19;
	if (attr == HYP_PMM_ATTR_SAPU_PAGE)
		sr_info = 14;

	return sr_info;
}

u8 hypmmu_get_srinfo(u8 attr)
{
	return (u8)attr_to_srinfo(attr);
}

u32 query_ac_srinfo(u64 pa)
{
	u32 srinfo = 0;
	u8 attr;

	attr = get_ac_attr(pa);
	if (attr == 0)
		/* normal buffer */
		srinfo = SR_INFO_NSR;
	else
		srinfo = attr_to_srinfo(attr);

	//MOD_PUTS2("srinfo: pa srinfo", pa, srinfo);
	//srinfo = 0;
	return srinfo;
}

void register_smc_handler(void)
{
	mod_ops->register_host_smc_handler(mtk_iommu_smc_handler);
}

static void donate_to_hyp(u64 pa, u64 size)
{
	u64 pfn = pa >> PAGE_SHIFT;
	u64 nr_pages = size >> PAGE_SHIFT;
	int ret;

	ret = mod_ops->host_donate_hyp(pfn, nr_pages, false);
	if (ret) {
		MOD_PUTS3("host_donate_hyp failed", ret, pa, size);
		WARN_ON(1);
	}
}

static struct mtk_iommu_device *dev_alloc(void)
{
	for (u32 i = 0; i < MAX_IOMMU_DEVICES; i++) {
		if (!devices[i].inited)
			return &devices[i];
	}

	mod_ops->puts("dev-alloc failed");
	WARN_ON(1);
	return NULL;
}

static void iommu_reg_donate_hyp(struct mtk_iommu_reg *reg)
{
	bool mmio = true;
	u64 pfn, nr_pages;
	int ret;

	pfn = reg->reg_base >> PAGE_SHIFT;
	nr_pages = reg->reg_size >> PAGE_SHIFT;
	ret = mod_ops->host_donate_hyp(pfn, nr_pages, mmio);
	if (ret)
		MOD_PUTS3("iommu-reg-donate-hyp failed", ret, pfn, nr_pages);

	WARN_ON(ret != 0);
}

static void setup_iommu_banks(struct mtk_iommu_device *dev, u32 base, u32 size)
{
	/* setup ns bank */
	dev->bank_reg[NS_BANK].reg_base = base;
	dev->bank_reg[NS_BANK].reg_size = size;

	/* setup prot bank 1-2 */
	dev->bank_reg[PROT_1_BANK].reg_base = base + REG_MMU_BANK_OFFSET;
	dev->bank_reg[PROT_1_BANK].reg_size = size;

	/* skip prot-2 if not enabled the jpeg prot-2*/
	if (!enable_jpeg_prot_2)
		return;

	dev->bank_reg[PROT_2_BANK].reg_base = base + REG_MMU_BANK_OFFSET * 2;
	dev->bank_reg[PROT_2_BANK].reg_size = size;
}

static void enable_mmio_trap(struct mtk_iommu_device *dev)
{
	/* donate iommu registers to hyp */
	for (u32 i = 0; i < MAX_BANK; i++) {
		if (!dev->bank_reg[i].reg_base)
			break;

		iommu_reg_donate_hyp(&dev->bank_reg[i]);
	}
}

static void map_dev(struct mtk_iommu_device *dev)
{
	struct mtk_iommu_reg *reg;

	for (u32 i = 0; i < MAX_BANK; i++) {
		if (!dev->bank_reg[i].reg_base)
			break;

		reg = &dev->bank_reg[i];
		reg->base_va = mod_ops->hyp_va(reg->reg_base);
#if (DEBUG_IOMMU)
		MOD_PUTS3("map_dev", i, reg->reg_base, reg->base_va);
#endif
		if (!reg->base_va)
			MOD_PUTS2("map-dev failed", i, reg->reg_base);
	}
}

static int register_iommu_device(u32 reg_base, u32 size, u32 table_id)
{
	struct mtk_iommu_device *dev;

	dev = dev_alloc();
	if (!dev)
		return -ENOMEM;

	/* setup register for each bank */
	setup_iommu_banks(dev, reg_base, size);

	/* enable mmio trap*/
	enable_mmio_trap(dev);

	/* map */
	map_dev(dev);

	dev->table_id = table_id;
	dev->inited = true;

	return 0;
}

void mtk_iommu_add_device(struct user_pt_regs *regs)
{
	u32 reg, size, table_id;

	reg = regs->regs[1];
	size = regs->regs[2];
	table_id = regs->regs[3];

#if (DEBUG_IOMMU)
	MOD_PUTS3("mtk_iommu_add_device", reg, size, table_id);
#endif

	register_iommu_device(reg, size, table_id);

	regs->regs[0] = SMCCC_RET_SUCCESS;
	regs->regs[1] = 0;
}

struct mtk_iommu_device *mtk_iommu_lookup_dev(u64 addr)
{
	struct mtk_iommu_device *dev;
	u64 base, size;

#if (DEBUG_IOMMU >= 2)
	MOD_PUTS1("mtk_iommu_lookup_dev", addr);
#endif
	/* for each device */
	for (u32 i = 0; i < MAX_IOMMU_DEVICES; i++) {
		if (!devices[i].inited)
			break;

		dev = &devices[i];
		base = (u64)dev->bank_reg[NS_BANK].reg_base;
		size = (u64)dev->bank_reg[NS_BANK].reg_size;

		if ((base <= addr) && (addr < (base + size)))
			return dev;
	}

	return NULL;
}

void mtk_iommu_hyp_init(struct user_pt_regs *regs)
{
	u32 order;

	ac_table_pa = regs->regs[1];
	order = (u32)regs->regs[2];
	ac_table_size = (PAGE_SIZE << order);

	rmem_base = regs->regs[3];
	rmem_size = regs->regs[4];

#if (DEBUG_IOMMU)
	MOD_PUTS4("mtk_iommu_hyp_init", ac_table_pa, ac_table_size,
		rmem_base, rmem_size);
#endif

	donate_to_hyp(ac_table_pa, ac_table_size);
	donate_to_hyp(rmem_base, rmem_size);

	/* fixed layout allocation*/
	static_alloc();

	/* get va */
	ac_table_va = mod_ops->hyp_va(ac_table_pa);
	rmem_base_va = mod_ops->hyp_va(rmem_base);

	/* clean ac_table */
	mod_ops->memset(ac_table_va, 0, ac_table_size);

	create_v7s_pages(page_pool_base, page_pool_size);

	register_smc_handler();

	register_mtkiommu_pmm_hal();

	/* Share buffer with INFRA-MPU */
	infra_mpu_percpu_ipc_va = mod_ops->hyp_va(infra_mpu_percpu_ipc_pa);
	infra_mpu_set_ipc_base(infra_mpu_percpu_ipc_pa, infra_mpu_percpu_ipc_va);

	regs->regs[0] = SMCCC_RET_SUCCESS;
	regs->regs[1] = 0;
}
