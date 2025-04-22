/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef __HYP_PMM_H__
#define __HYP_PMM_H__

#include <asm/kvm_pkvm_module.h>
#include "hyp_pmm_struct.h"

int hyp_pmm_init(void);
/* register cpu hal for hyp-pmm */
int register_cpu_hal(void);

#endif
