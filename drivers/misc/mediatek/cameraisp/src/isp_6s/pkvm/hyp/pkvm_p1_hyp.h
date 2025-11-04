/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include <asm/kvm_pkvm_module.h>
#include "pkvm_p1_defines.h"
#include "pkvm_p1_sec_regs.h"
#include <pkvm_mgmt/pkvm_mgmt.h>
#include <pkvm_trustzone.h>
#include <pkvm_sys.h>


/*******************************************************************************
 * Defines
 ******************************************************************************/
#define PFX "[pkvm_p1] "

#define CALL_FROM_OPS(fn, ...) pkvm_p1_ops->fn(__VA_ARGS__)

extern const struct pkvm_module_ops *pkvm_p1_ops;
