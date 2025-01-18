/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __POLICY_H
#define __POLICY_H

#include "debug.h"

#define FUNCTION_BITS	(8)
#define RESERVE_BITS	(3)
#define POLICY_BITS	(5)

#define FUNCTION_SHIFT	(0)
#define RESERVE_SHIFT	(FUNCTION_SHIFT + FUNCTION_BITS)
#define POLICY_SHIFT	(RESERVE_SHIFT + RESERVE_BITS)

#define FUNCTION_MAXNR	(1UL << (FUNCTION_BITS))
#define POLICY_MAXNR	(1UL << (POLICY_BITS))
#define FUNCTION_MASK	(FUNCTION_MAXNR - 1)
#define POLICY_MASK	(POLICY_MAXNR - 1)
#define FUNCTION_ID(x)	((x >> FUNCTION_SHIFT) & FUNCTION_MASK)
#define POLICY_ID(x)	((x >> POLICY_SHIFT) & POLICY_MASK)

#define MKP_HVC_CALL_ID(policy, hvc_func_num)	(((policy & POLICY_MASK) << POLICY_SHIFT) | \
	((hvc_func_num & FUNCTION_MASK) << FUNCTION_SHIFT))

/* MKP Policy ID */
enum mkp_policy_id {
	/* MKP default policies (0 ~ 15) */
	MKP_POLICY_MKP = 0,		/* Policy ID for MKP itself */
	MKP_POLICY_DRV,			/* Policy ID for kernel drivers */
	MKP_POLICY_SELINUX_STATE,	/* Policy ID for selinux_state */
	MKP_POLICY_SELINUX_AVC,		/* Policy ID for selinux_avc */
	MKP_POLICY_TASK_CRED,		/* Policy ID for task credential */
	MKP_POLICY_KERNEL_CODE,		/* Policy ID for kernel text */
	MKP_POLICY_KERNEL_RODATA,	/* Policy ID for kernel rodata */
	MKP_POLICY_KERNEL_PAGES,	/* Policy ID for other mapped kernel pages */
	MKP_POLICY_PGTABLE,		/* Policy ID for page table */
	MKP_POLICY_S1_MMU_CTRL,		/* Policy ID for stage-1 MMU control */
	MKP_POLICY_FILTER_SMC_HVC,	/* Policy ID for HVC/SMC call filtering */
	MKP_POLICY_DEFAULT_END,
	MKP_POLICY_DEFAULT_MAX = 15,

	/* Policies for vendors start from here (16 ~ 31) */
	MKP_POLICY_VENDOR_START = 16,
	MKP_POLICY_NR = POLICY_MAXNR,
};

/* Characteristic for Policy */
enum mkp_policy_char {
	NO_MAP_TO_DEVICE	= 0x00000001,	/**/
	NO_UPGRADE_TO_WRITE	= 0x00000002,
	NO_UPGRADE_TO_EXEC	= 0x00000004,
	HANDLE_PERMANENT	= 0x00000100,
	SB_ENTRY_DISORDERED	= 0x00010000,
	SB_ENTRY_ORDERED	= 0x00020000,
	ACTION_NOTIFICATION	= 0x00100000,
	ACTION_WARNING		= 0x00200000,
	ACTION_PANIC		= 0x00400000,
	ACTION_BITS		= 0x00700000,
	CHAR_POLICY_INVALID	= 0x40000000,
	CHAR_POLICY_AVAILABLE	= 0x80000000,
};

extern int policy_ctrl[MKP_POLICY_NR];
extern u32 mkp_policy_action[MKP_POLICY_NR];
void set_policy(u32 policy);	// TODO: __init attributes
int __init set_ext_policy(uint32_t policy);
void handle_mkp_err_action(uint32_t policy);
#endif
