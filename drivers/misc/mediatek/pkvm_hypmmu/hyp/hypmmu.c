// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <asm/kvm_pkvm_module.h>
#include "include/hypmmu.h"

const struct pkvm_module_ops *mod_ops;

int pkvm_hypmmu_load_init(const struct pkvm_module_ops *ops)
{
	mod_ops = ops;
	ops->puts(__func__);
	return 0;
}

