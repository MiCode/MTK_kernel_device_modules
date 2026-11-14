// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include "hyp_pmm_struct.h"

int hyp_pmm_hal_register(struct pmm_hal *hal);
void hyp_pmm_secure_range(u64 pa, u64 size, u8 attr);
void hyp_pmm_unsecure_range(u64 pa, u64 size, u8 attr);
