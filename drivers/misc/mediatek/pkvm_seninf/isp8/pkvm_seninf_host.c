// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <asm/kvm_pkvm_module.h>
#include <linux/arm-smccc.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <pkvm_mgmt/pkvm_mgmt.h>

#include "pkvm_seninf_host.h"

static int seninf_hvc_register(unsigned long token)
{
	struct arm_smccc_res res;
	int ret;

	ret = pkvm_register_el2_mod_call(kvm_nvhe_sym(seninf_hyp_checkpipe), token);
	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_ADD_HVC,
		SMC_ID_MTK_PKVM_SENINF_CHECKPIPE, ret, 0, 0, 0, 0, &res);

	ret = pkvm_register_el2_mod_call(kvm_nvhe_sym(seninf_hyp_free), token);
	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_ADD_HVC,
		SMC_ID_MTK_PKVM_SENINF_FREE, ret, 0, 0, 0, 0, &res);

	return 0;
}

static int __init seninf_nvhe_init(void)
{
	unsigned long token;
	int ret;

	if (!is_protected_kvm_enabled()) {
		pr_info(PFX "%s: skip to init pkvm seninf module\n", __func__);
		return 0;
	}

	ret = pkvm_load_el2_module(kvm_nvhe_sym(seninf_hyp_init), &token);
	if (ret) {
		pr_info(PFX "%s: failed to load pkvm seninf module, ret %d\n", __func__, ret);
		return ret;
	}

	ret = seninf_hvc_register(token);
	if (ret) {
		pr_info(PFX "%s: failed to register seninf hvc, ret %d\n", __func__, ret);
		return ret;
	}

	pr_info(PFX "%s: success to load pkvm seninf module\n", __func__);
	return 0;
}
module_init(seninf_nvhe_init);
MODULE_LICENSE("GPL");
