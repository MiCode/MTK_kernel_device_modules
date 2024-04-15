// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/kvm_pkvm_module.h>

#include "pkvm_tmem_host.h"

static int hvc_nr_region_protect;
static int hvc_nr_region_unprotect;
static int hvc_nr_page_protect;
static int hvc_nr_page_unprotect;

int get_hvc_nr_region_protect(void)
{
	return hvc_nr_region_protect;
}
EXPORT_SYMBOL(get_hvc_nr_region_protect);

int get_hvc_nr_region_unprotect(void)
{
	return hvc_nr_region_unprotect;
}
EXPORT_SYMBOL(get_hvc_nr_region_unprotect);

int get_hvc_nr_page_protect(void)
{
	return hvc_nr_page_protect;
}
EXPORT_SYMBOL(get_hvc_nr_page_protect);

int get_hvc_nr_page_unprotect(void)
{
	return hvc_nr_page_unprotect;
}
EXPORT_SYMBOL(get_hvc_nr_page_unprotect);

static int __init test_nvhe_init(void)
{
	int ret;
	unsigned long token;

	if (!is_protected_kvm_enabled()) {
		pr_info("skip to load pkvm_tmem\n");
		return 0;
	}

	ret = pkvm_load_el2_module(__kvm_nvhe_hyp_tmem_init, &token);
	if (ret) {
		pr_info("%s: pkvm_load_el2_module() fail, ret=%d\n", __func__, ret);
		return ret;
	}

	pr_info("hyp_tmem_init done\n");

	ret = pkvm_register_el2_mod_call(__kvm_nvhe_hyp_region_protect, token);
	if (ret < 0)
		return ret;

	hvc_nr_region_protect = ret;
	pr_info("pkvm_tmem hvc_nr_region_protect=0x%x\n", hvc_nr_region_protect);

	ret = pkvm_register_el2_mod_call(__kvm_nvhe_hyp_region_unprotect, token);
	if (ret < 0)
		return ret;

	hvc_nr_region_unprotect = ret;
	pr_info("pkvm_tmem hvc_nr_region_unprotect=0x%x\n", hvc_nr_region_unprotect);

	ret = pkvm_register_el2_mod_call(__kvm_nvhe_hyp_page_protect, token);
	if (ret < 0)
		return ret;

	hvc_nr_page_protect = ret;
	pr_info("pkvm_tmem hvc_nr_page_protect=0x%x\n", hvc_nr_page_protect);

	ret = pkvm_register_el2_mod_call(__kvm_nvhe_hyp_page_unprotect, token);
	if (ret < 0)
		return ret;

	hvc_nr_page_unprotect = ret;
	pr_info("pkvm_tmem hvc_nr_page_unprotect0x%x\n", hvc_nr_page_unprotect);

	return 0;
}
module_init(test_nvhe_init);
MODULE_LICENSE("GPL");
