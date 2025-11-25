/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

extern unsigned long mod_token;

enum mgmt_command {
	DISABLE_CPU_PROTECTION       = 1,
	ENABLE_CPU_PROTECTION        = 2,
	DISABLE_GPU_PROTECTION       = 3,
	ENABLE_GPU_PROTECTION        = 4,
	DISABLE_INFRA_MPU_PROTECTION = 5,
	ENABLE_INFRA_MPU_PROTECTION  = 6,
	DISABLE_SMMU_PROTECTION      = 7,
	ENABLE_SMMU_PROTECTION       = 8,
	DUMP_PROTECTION_STATUS       = 9,
	INVALID_COMMAND              = 10,
};

enum protection_type {
	CPU_PROTECTION       = 0,
	GPU_PROTECTION       = 1,
	INFRA_MPU_PROTECTION = 2,
	SMMU_PROTECTION      = 3,
	TOTAL_PROTECTION     = 4,
};

int debug_protection_init(void);
