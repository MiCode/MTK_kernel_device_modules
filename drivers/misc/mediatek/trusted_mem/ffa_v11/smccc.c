// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021 ARM Ltd.
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/printk.h>
#include <linux/kvm.h>
#include <linux/kprobes.h>
#include <linux/mutex.h>

#include "common.h"

#define PKVM_MGMT_SYMBOL	"pkvm_mgmt_get_ver"
#define APPLY_FFA_WA

#ifdef APPLY_FFA_WA
static struct kprobe tmp_kp;
static DEFINE_MUTEX(pkvm_mgmt_mutex);

static void *lookup_function_address(const char *name)
{
	int ret;
	void *addr = NULL;

	memset(&tmp_kp, 0, sizeof(struct kprobe));
	tmp_kp.symbol_name = name;

	ret = register_kprobe(&tmp_kp);
	if (ret < 0) {
		pr_info("register_kprobe failed for %s, returned %d\n", name, ret);
		return 0;
	}

	addr = tmp_kp.addr;

	unregister_kprobe(&tmp_kp);

	return addr;
}

static bool is_pkvm_mgmt_valid(void)
{
	static void *symbol_addr;
	static int has_run;

	mutex_lock(&pkvm_mgmt_mutex);
	if (has_run == 0) {
		symbol_addr = lookup_function_address(PKVM_MGMT_SYMBOL);
		has_run = 1;
	}
	mutex_unlock(&pkvm_mgmt_mutex);

	if (symbol_addr)
		return true;

	return false;
}
#endif

static void __arm_ffa_fn_smc(ffa_value_t args, ffa_value_t *res)
{
#ifdef APPLY_FFA_WA
	/* FF-A workaround, routing FFA to vendor module */
	if (is_protected_kvm_enabled() && is_pkvm_mgmt_valid())
		args.a0 |= 0x8000UL;
#endif
	arm_smccc_1_2_smc(&args, res);
}

static void __arm_ffa_fn_hvc(ffa_value_t args, ffa_value_t *res)
{
	arm_smccc_1_2_hvc(&args, res);
}

int __init ffa_transport_init(ffa_fn **invoke_ffa_fn)
{
	enum arm_smccc_conduit conduit;

	if (arm_smccc_get_version() < ARM_SMCCC_VERSION_1_2)
		return -EOPNOTSUPP;

	conduit = arm_smccc_1_1_get_conduit();
	if (conduit == SMCCC_CONDUIT_NONE) {
		pr_err("%s: invalid SMCCC conduit\n", __func__);
		return -EOPNOTSUPP;
	}

	if (conduit == SMCCC_CONDUIT_SMC)
		*invoke_ffa_fn = __arm_ffa_fn_smc;
	else
		*invoke_ffa_fn = __arm_ffa_fn_hvc;

	return 0;
}
