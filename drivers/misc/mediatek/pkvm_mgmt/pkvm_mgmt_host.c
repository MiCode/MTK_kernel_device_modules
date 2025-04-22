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

#undef pr_fmt
#define pr_fmt(fmt) "[PKVM_MGMT]: " fmt
#define PKVM_MGMT_VER	(1)

int __kvm_nvhe_mtk_smc_handler_hyp_init(const struct pkvm_module_ops *ops);

u32 pkvm_mgmt_get_ver(void)
{
	return PKVM_MGMT_VER;
}
EXPORT_SYMBOL(pkvm_mgmt_get_ver);

static int __init mtk_smc_handler_nvhe_init(void)
{
	int ret;
	unsigned long token;

	if (!is_protected_kvm_enabled())
		return 0;

	ret = pkvm_load_el2_module(__kvm_nvhe_mtk_smc_handler_hyp_init, &token);
	if (ret)
		return ret;
	pr_info("pkvm smc handler okay\n");
	return 0;
}
module_init(mtk_smc_handler_nvhe_init);
MODULE_LICENSE("GPL");
