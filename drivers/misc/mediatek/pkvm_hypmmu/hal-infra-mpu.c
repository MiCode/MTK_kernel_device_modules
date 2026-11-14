// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/kvm_pkvm_module.h>

#include "pkvm_hypmmu_host.h"

#undef pr_fmt
#define pr_fmt(fmt) "[PKVM_HYPMMU_INFRAMPU]: " fmt

static int init_hvc;

static void setup_hvc_call(void)
{
	init_hvc = pkvm_register_el2_mod_call(
		kvm_nvhe_sym(infra_mpu_hyp_init), mod_token);
}

static int init_hvc_call(void)
{
	int ret;

	ret = pkvm_el2_mod_call(init_hvc);

	return ret;
}

int init_inframpu(void)
{
	int ret;

	setup_hvc_call();

	ret = init_hvc_call();
	if (ret)
		pr_info("init hvc call failed ret=%d\n", ret);

	return ret;
}
