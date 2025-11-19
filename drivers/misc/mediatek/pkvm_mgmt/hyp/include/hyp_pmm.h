/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef __HYP_PMM_H__
#define __HYP_PMM_H__

#include <asm/kvm_pkvm_module.h>
#include "hyp_pmm_struct.h"

enum mgmt_command {
	DISABLE_CPU_PROTECTION       = 1,
	ENABLE_CPU_PROTECTION        = 2,
	DISABLE_GPU_PROTECTION       = 3,
	ENABLE_GPU_PROTECTION        = 4,
	DISABLE_INFRA_MPU_PROTECTION = 5,
	ENABLE_INFRA_MPU_PROTECTION  = 6,
	DUMP_PROTECTION_STATUS       = 9,
	INVALID_COMMAND              = 10,
};

int hyp_pmm_init(void);
/* register cpu hal for hyp-pmm */
int register_cpu_hal(void);

#endif
