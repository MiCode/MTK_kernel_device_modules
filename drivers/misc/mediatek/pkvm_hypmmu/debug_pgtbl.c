// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/kvm.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>

#include <asm/kvm_pkvm_module.h>
#include "pkvm_hypmmu_host.h"
#include "debug_pgtbl.h"

static int debug_init_iommu_hvc;

static struct page *debug_iommu_page;

static int setup_hvc_call(void)
{
	debug_init_iommu_hvc = pkvm_register_el2_mod_call(
		kvm_nvhe_sym(register_iova_debug_info), mod_token);

	return 0;
}

int debug_pgtbl_init(void)
{
	int ret;

	setup_hvc_call();

	debug_iommu_page = alloc_pages(GFP_KERNEL | __GFP_ZERO, PGTBL_PAGE_ORDER);
	ret = pkvm_el2_mod_call(debug_init_iommu_hvc,
			page_to_phys(debug_iommu_page) >> PAGE_SHIFT, PGTBL_PAGE_ORDER);
	if (ret != 0)
		pr_info("debug_init_pgtbl fail\n");


	return 0;
}
