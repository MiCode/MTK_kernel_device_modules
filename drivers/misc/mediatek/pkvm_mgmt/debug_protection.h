/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

extern unsigned long mod_token;

enum mgmt_command {
	DISABLE_CPU_PROTECTION       = 0,
	ENABLE_CPU_PROTECTION        = 1,
	DISABLE_GPU_PROTECTION       = 2,
	ENABLE_GPU_PROTECTION        = 3,
	DISABLE_INFRA_MPU_PROTECTION = 4,
	ENABLE_INFRA_MPU_PROTECTION  = 5,
	DUMP_PROTECTION_STATUS       = 7,
	INVALID_COMMAND              = 8,
};

enum protection_type {
	CPU_PROTECTION       = 0,
	GPU_PROTECTION       = 1,
	INFRA_MPU_PROTECTION = 2,
	TOTAL_PROTECTION     = 3,
};

int debug_protection_init(void);
