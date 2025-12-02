/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

extern unsigned long mod_token;

#define PGTBL_PAGE_ORDER        (0)
#define SEC_TO_US               (1000000)
#define ONE_LINE_LEN            (10)
#define MAX_RECORD_TIMES        (100)
#define CPU_PGTBL_BASE_OFFSET   (0)
#define GPU_PGTBL_BASE_OFFSET   (CPU_PGTBL_BASE_OFFSET + MAX_RECORD_TIMES)
#define IOMMU_PGTBL_BASE_OFFSET (GPU_PGTBL_BASE_OFFSET + MAX_RECORD_TIMES)
#define IMPU_PGTBL_BASE_OFFSET  (IOMMU_PGTBL_BASE_OFFSET + MAX_RECORD_TIMES)
#define SMMU_PGTBL_BASE_OFFSET  (IMPU_PGTBL_BASE_OFFSET + MAX_RECORD_TIMES)

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
	INIT_DEBUG_PAGE              = 10,
	RESET_PGTBL_TIME             = 11,
	INVALID_COMMAND              = 12,
};

enum protection_type {
	CPU_PROTECTION       = 0,
	GPU_PROTECTION       = 1,
	INFRA_MPU_PROTECTION = 2,
	SMMU_PROTECTION      = 3,
	TOTAL_PROTECTION     = 4,
};

int debug_protection_init(void);
