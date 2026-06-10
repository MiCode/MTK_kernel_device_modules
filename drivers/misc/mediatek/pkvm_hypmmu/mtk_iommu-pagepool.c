// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/of.h>

#include "mtk_iommu-pagepool.h"

#define IOMMU_PGTBL_PAGE_MEMORY_DT_UNAME "iommu-pgtbl-page-memory"
#define IOMMU_MPOOL_SIZE SZ_32M

bool is_iommu_pgtbl_page_memory(void)
{
	struct device_node *dt_node;

	dt_node = of_find_node_by_name(NULL, IOMMU_PGTBL_PAGE_MEMORY_DT_UNAME);
	if (!dt_node)
		return false;

	return true;
}

static void alloc_iommu_memory(struct mpt *mpt, unsigned long long target_page_counts,
		       unsigned long long *acc_page_num)
{
	/* allocate memory from 2M contious memory to 256KB contious memory */
	for (int page_order = 9; page_order >= 6; page_order--) {
		struct page *pmm_page;

		if ((target_page_counts - *acc_page_num) <
		    (1ULL << page_order)) {
			pr_info("%s: left %#llx < request page  %#llx\n",
			       __func__, (target_page_counts - *acc_page_num),
			       (1ULL << page_order));
			continue;
		}
		pmm_page = alloc_pages(GFP_KERNEL | __GFP_ZERO, page_order);

		if (pmm_page) {
			mpt->fmpt[mpt->mem_block_num].smpt = (u64 *)(page_to_phys(pmm_page) >> PAGE_SHIFT);
			mpt->fmpt[mpt->mem_block_num].mem_order = page_order;
			*acc_page_num += (1ULL << page_order);
			mpt->mem_block_num++;
			pr_info("%s: addr=0x%llx, page_order=%#x\n", __func__, page_to_phys(pmm_page), page_order);
			break;
		}
		pr_info("%s: get page_order %#x fail\n", __func__, page_order);
	}
}

static unsigned long alloc_mpool_mem(struct mpt *mpt,
				unsigned long long target_page_counts)
{
	unsigned long addr;
	unsigned long pfn;
	unsigned long long acc_page_num = 0;

	if (sizeof(*mpt) > PAGE_SIZE) {
		pr_info("%s: mpt size %lx bigger than PAGE_SIZE\n", __func__, sizeof(*mpt));
		pr_info("%s: struct fmpt size %lx\n", __func__, sizeof(struct fmpt));
		return 0;
	}
	addr = get_zeroed_page(GFP_KERNEL);

	if (!addr) {
		pr_info("%s: alloc page fail\n", __func__);
		return 0;
	}

	mpt = (struct mpt *)addr;
	mpt->mem_block_num = 0;

	if (target_page_counts == 0)
		return 0;

	pr_info("%s: target_page_counts %#llx\n", __func__, target_page_counts);

	while (acc_page_num < target_page_counts) {
		if (mpt->mem_block_num >= IOMMU_DRIVER_MEM_PFN_MAX) {
			pr_info("%s: mpt->mem_block_num: %#x over IOMMU_DRIVER_MEM_PFN_MAX: %#x\n",
				__func__, mpt->mem_block_num, IOMMU_DRIVER_MEM_PFN_MAX);
			break;
		}
		alloc_iommu_memory(mpt, target_page_counts, &acc_page_num);
	}
	pfn = __pa(mpt) >> PAGE_SHIFT;

	return pfn;
}

unsigned long alloc_iommu_pgtbl_page(void)
{
	unsigned long long target_page_counts;
	struct arm_smccc_res smc_res;
	struct mpt *mpt = NULL;
	unsigned long pfn;

	target_page_counts = IOMMU_MPOOL_SIZE / SZ_4K;
	/* Allocate memory from body system for mpool*/
	pfn = alloc_mpool_mem(mpt, target_page_counts);

	if (!is_protected_kvm_enabled()) {
		arm_smccc_smc(HYPPMM_SET_CMA_REGION, pfn, 0, 1,
			0, 0, 0, 0, &smc_res);
		if (smc_res.a0 != 0) {
			pr_info("HYP_ENABLE_STAGE2_PROTECTION smc_res.a0=%#lx\n", smc_res.a0);
			return -EINVAL;
		}
	}

	return pfn;
}
