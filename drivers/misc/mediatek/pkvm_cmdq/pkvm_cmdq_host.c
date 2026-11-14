// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <asm/kvm_pkvm_module.h>
#include <linux/arm-smccc.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_reserved_mem.h>
#include <linux/of.h>
#include <pkvm_mgmt/pkvm_mgmt.h>

#include "pkvm_cmdq_host.h"

static int cmdq_memory_mapping;
static int cam_preview_support;

static int cmdq_hvc_register(unsigned long token)
{
	struct arm_smccc_res res;
	int ret;

	ret = pkvm_register_el2_mod_call(kvm_nvhe_sym(cmdq_hyp_submit_task), token);
	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_ADD_HVC,
		SMC_ID_MTK_PKVM_CMDQ_SUBMIT_TASK, ret, 0, 0, 0, 0, &res);

	ret = pkvm_register_el2_mod_call(kvm_nvhe_sym(cmdq_hyp_res_release), token);
	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_ADD_HVC,
		SMC_ID_MTK_PKVM_CMDQ_RES_RELEASE, ret, 0, 0, 0, 0, &res);

	ret = pkvm_register_el2_mod_call(kvm_nvhe_sym(cmdq_hyp_cancel_task), token);
	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_ADD_HVC,
		SMC_ID_MTK_PKVM_CMDQ_CANCEL_TASK, ret, 0, 0, 0, 0, &res);

	ret = pkvm_register_el2_mod_call(kvm_nvhe_sym(cmdq_hyp_path_res_allocate), token);
	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_ADD_HVC,
		SMC_ID_MTK_PKVM_CMDQ_PATH_RES_ALLOCATE, ret, 0, 0, 0, 0, &res);

	ret = pkvm_register_el2_mod_call(kvm_nvhe_sym(cmdq_hyp_path_res_release), token);
	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_ADD_HVC,
		SMC_ID_MTK_PKVM_CMDQ_PATH_RES_RELEASE, ret, 0, 0, 0, 0, &res);

	ret = pkvm_register_el2_mod_call(kvm_nvhe_sym(cmdq_hyp_pkvm_init), token);
	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_ADD_HVC,
		SMC_ID_MTK_PKVM_CMDQ_PKVM_INIT, ret, 0, 0, 0, 0, &res);

	ret = pkvm_register_el2_mod_call(kvm_nvhe_sym(cmdq_hyp_pkvm_disable), token);
	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_ADD_HVC,
		SMC_ID_MTK_PKVM_CMDQ_PKVM_DISABLE, ret, 0, 0, 0, 0, &res);

	cmdq_memory_mapping = pkvm_register_el2_mod_call(
		kvm_nvhe_sym(cmdq_hyp_get_memory), token);
	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_ADD_HVC,
		SMC_ID_MTK_PKVM_CMDQ_GET_MEMORY,
		cmdq_memory_mapping, 0, 0, 0, 0, &res);

	cam_preview_support = pkvm_register_el2_mod_call(
		kvm_nvhe_sym(cmdq_hyp_cam_preview_support), token);
	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_ADD_HVC,
		SMC_ID_MTK_PKVM_CMDQ_CAM_PREVIEW_SUPPORT,
		cam_preview_support, 0, 0, 0, 0, &res);

	return 0;
}

void cmdq_reserved_memory_probe(void)
{
	struct device_node *node = NULL;
	struct reserved_mem *rmem = NULL;
	phys_addr_t base, size;
	const char *support = NULL;
	bool preview_support = false;

	node = of_find_compatible_node(NULL, NULL, "mediatek,me_cmdq_reserved");
	rmem = of_reserved_mem_lookup(node);
	if (rmem) {
		base = rmem->base;
		size = rmem->size;
		pr_info("%s: rmem base: %pa, rmem size: %pa\n", __func__, &base, &size);

		pkvm_el2_mod_call(cmdq_memory_mapping, base, size);
	}

	node = of_find_node_by_name(NULL, "pkvm");
	if (node) {
		of_property_read_string(node, "mtkcam-security-cam-normal-preview-support",
			&support);
		if (strncmp(support, "okay", sizeof("okay")) == 0)
			preview_support = true;

		pkvm_el2_mod_call(cam_preview_support, preview_support);
	}
	pr_info("%s: mtkcam security cam normal preview support: %d\n",
		__func__, preview_support);
}

static int __init cmdq_nvhe_init(void)
{
	unsigned long token;
	int ret;

	if (!is_protected_kvm_enabled()) {
		pr_info("%s: skip to init pkvm cmdq module\n", __func__);
		return 0;
	}

	ret = pkvm_load_el2_module(kvm_nvhe_sym(cmdq_hyp_init), &token);
	if (ret) {
		pr_info("%s: failed to load pkvm cmdq module, ret %d\n", __func__, ret);
		return ret;
	}

	ret = cmdq_hvc_register(token);
	if (ret) {
		pr_info("%s: failed to register cmdq hvc, ret %d\n", __func__, ret);
		return ret;
	}

	cmdq_reserved_memory_probe();

	pr_info("%s: success to load pkvm cmdq module\n", __func__);
	return 0;
}
module_init(cmdq_nvhe_init);
MODULE_LICENSE("GPL");
