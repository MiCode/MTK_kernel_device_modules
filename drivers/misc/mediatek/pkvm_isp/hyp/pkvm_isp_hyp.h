/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <asm/kvm_pkvm_module.h>

#define PFX "[pkvm_isp] "
#define CALL_FROM_OPS(fn, ...) pkvm_isp_ops->fn(__VA_ARGS__)

extern const struct pkvm_module_ops *pkvm_isp_ops;
