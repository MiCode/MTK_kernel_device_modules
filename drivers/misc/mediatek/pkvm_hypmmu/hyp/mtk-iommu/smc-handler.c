// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <asm/io.h>
#include <asm/kvm_hyp.h>
#include <asm/kvm_pkvm_module.h>
#include <linux/math.h>
#include <linux/arm-smccc.h>
#include <pkvm_mgmt/spinlock.h>

#include <mtk-iommu-defines.h>
#include "include/hypmmu.h"
#include "include/mtk-iommu.h"

/* hyp-pmm fastcalls */
#define HYP_PMM_GET_HYPMMU_TYPE2_EN		(0XBB00FFA4)
#define HYP_PMM_REG_HYPMMU_SHARE_REGION		(0XBB00FFA5)
#define HYP_PMM_IOVA_TO_PHYS			(0XBB00FFA6)
#define HYP_PMM_HYPMMU_TYPE2_INV		(0XBB00FFA7)

static hyp_spinlock_t debug_lock;

/* convert iova register format to u64 iova value */
static u64 reg_to_iova(u32 reg)
{
	return (reg & TLB_ADDR_LO_MASK) | ((u64)(reg & TLB_ADDR_HI_MASK) << 32);
}

static void register_share_region(struct user_pt_regs *regs)
{
	u64 iova = regs->regs[1] << ARM_V7S_PAGE_SHIFT;
	u64 size = regs->regs[2] << ARM_V7S_PAGE_SHIFT;
	u64 table_id = regs->regs[3];

	add_share_region(iova, size, table_id);
#if (DEBUG_IOMMU)
	MOD_PUTS3("register-share-region", iova, size, table_id);
#endif

	regs->regs[0] = 0x0;
}

static void handle_iova_to_phys(struct user_pt_regs *regs)
{
	u64 iova = regs->regs[1] | (regs->regs[2] << 32);
	u64 table_id= regs->regs[3];
	u64 ret = 0;
	u64 h_pgd_pa, p_pgd_pa;
	u32 *h_pgd, *p_pgd;
	u32 lvl, hw_pte, prot_pte, hw_ret, prot_ret;

	MOD_PUTS2("tf: io_get_pte: iova table_id", iova, table_id);

	if (table_id >= MAX_TABLE_NUM) {
		MOD_PUTS1("iova_to_phys: invalid table_id", table_id);
		ret = (u64)-1UL;
		goto out;
	}

	h_pgd_pa = query_hw_pgd(table_id);
	p_pgd_pa = query_prot_pgd(table_id);

	if (!h_pgd_pa || !p_pgd_pa) {
		MOD_PUTS2("iova_to_phys: invalid pgd pa", h_pgd_pa, p_pgd_pa);
		ret = (u64)-1UL;
		goto out;
	}

	h_pgd = hyp_phys_to_virt(h_pgd_pa);
	p_pgd = hyp_phys_to_virt(p_pgd_pa);

	lvl = debug_io_get_pte(h_pgd, iova, &hw_pte, table_id);
	hw_ret = debug_make_result(iova, hw_pte, lvl);
	MOD_PUTS2("io_get_pte: hw_pte lvl", hw_pte, lvl);

	lvl = debug_io_get_pte(p_pgd, iova, &prot_pte, table_id);
	prot_ret = debug_make_result(iova, prot_pte, lvl);
	MOD_PUTS2("io_get_pte: prot_pte lvl", prot_pte, lvl);

	ret = (u64)hw_ret | ((u64)prot_ret << 32);
out:
	regs->regs[0] = ret;
}

/*
 * Handle iova range invalidation
 * Call to io-pgtable handler of shadow page management
 */
static void handle_inv(struct user_pt_regs *regs)
{
	u32 reg_sa = (u32)regs->regs[1];
	u32 reg_ea = (u32)regs->regs[2];
	u64 table_id = (u32)regs->regs[3];
	u64 iova_start, iova_end, iova_size;
	int ret = 0;

	iova_start = reg_to_iova(reg_sa);
	iova_end = reg_to_iova(reg_ea) + V7S_PAGE_SIZE;
	iova_size = iova_end - iova_start;
#if (DEBUG_IOMMU >= 2)
	MOD_PUTS6("handle-inv: reg_sa reg_ea iova_s iova_e iova_sz tid",
		reg_sa, reg_ea, iova_start, iova_end, iova_size, table_id);
#endif
	ret = io_pgtable_handler(iova_start, iova_size, table_id);

	//regs->regs[0] = (ret) ? (u64)-1 : 0UL;
	regs->regs[0] = 0UL;
}

bool mtk_iommu_smc_handler(struct user_pt_regs *regs)
{
	u64 smc_id = regs->regs[0] & ~ARM_SMCCC_CALL_HINTS;
	bool handled = false;

	if (!(smc_id >= 0xBB00FFA2 && regs->regs[0] <= 0xBB00FFA7))
		return false;

#if (DEBUG_IOMMU >= 2)
	MOD_PUTS1("mtk_iommu_smc_handler", regs->regs[0]);
#endif

	hyp_spin_lock(&debug_lock);

	switch(smc_id) {
	case HYP_PMM_GET_HYPMMU_TYPE2_EN:
		mod_ops->puts("hypmmu enabled");
		regs->regs[0] = 0x1;	/* hypmmu enabled */
		handled = true;
		break;
	case HYP_PMM_REG_HYPMMU_SHARE_REGION:
		register_share_region(regs);
		handled = true;
		break;
	case HYP_PMM_IOVA_TO_PHYS:
		handle_iova_to_phys(regs);
		handled = true;
		break;
	case HYP_PMM_HYPMMU_TYPE2_INV:
		handle_inv(regs);
		handled = true;
		break;
	default:
		MOD_PUTS1("mtk-iommu-smc-handler: unknown smc", smc_id);
		break;
	}

	hyp_spin_unlock(&debug_lock);

	return handled;
}
