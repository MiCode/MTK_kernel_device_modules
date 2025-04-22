// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/kvm.h>
#include <linux/module.h>
#include <linux/init.h>

#include <asm/kvm_pkvm_module.h>
#include <pkvm_mgmt/pkvm_mgmt.h>
#include "pkvm_mgmt_host.h"

#undef pr_fmt
#define pr_fmt(fmt) "[PKVM_MGMT]: " fmt
#define PKVM_MGMT_VER	(1)

int kvm_nvhe_sym(pkvm_mgmt_hyp_init)(const struct pkvm_module_ops *ops);

int kvm_nvhe_sym(hyp_pmm_hal_register)(void *hal);
EXPORT_SYMBOL(kvm_nvhe_sym(hyp_pmm_hal_register));

static unsigned long mod_token;
static int hyp_pmm_assign_buffer_v2_hcall;
static int hyp_pmm_unassign_buffer_v2_hcall;
static int hyp_pmm_defragment_hcall;

u32 pkvm_mgmt_get_ver(void)
{
	return PKVM_MGMT_VER;
}
EXPORT_SYMBOL(pkvm_mgmt_get_ver);

static int __init setup_hvc_call(void)
{
	struct arm_smccc_res res;

	hyp_pmm_assign_buffer_v2_hcall = pkvm_register_el2_mod_call(
		kvm_nvhe_sym(hyp_pmm_assign_buffer_v2), mod_token);
	hyp_pmm_unassign_buffer_v2_hcall = pkvm_register_el2_mod_call(
		kvm_nvhe_sym(hyp_pmm_unassign_buffer_v2), mod_token);
	hyp_pmm_defragment_hcall = pkvm_register_el2_mod_call(
		kvm_nvhe_sym(hyp_pmm_defragment), mod_token);

	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_ADD_HVC,
			  SMC_ID_MTK_PKVM_PMM_ASSIGN_BUFFER_V2,
			  hyp_pmm_assign_buffer_v2_hcall , 0, 0, 0, 0, &res);
	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_ADD_HVC,
			  SMC_ID_MTK_PKVM_PMM_UNASSIGN_BUFFER_V2,
			  hyp_pmm_unassign_buffer_v2_hcall , 0, 0, 0, 0, &res);
	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_ADD_HVC,
			  SMC_ID_MTK_PKVM_PMM_DEFRAGMENT,
			  hyp_pmm_defragment_hcall , 0, 0, 0, 0, &res);
	return 0;
}

static int __init pkvm_mgmt_nvhe_init(void)
{
	int ret;

	if (!is_protected_kvm_enabled())
		return 0;

	ret = pkvm_load_el2_module(kvm_nvhe_sym(pkvm_mgmt_hyp_init), &mod_token);
	if (ret) {
		pr_err("pkvm load el2 module failed\n");
		return ret;
	}

	ret = setup_hvc_call();
	if (ret) {
		pr_err("setup_hvc_call failed\n");
		return ret;
	}

	pr_info("pkvm_mgmt_nvhe_init successfully\n");
	return 0;
}
module_init(pkvm_mgmt_nvhe_init);
MODULE_LICENSE("GPL");
