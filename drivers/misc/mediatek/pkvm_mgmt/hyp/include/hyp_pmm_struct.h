// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/types.h>

struct pmm_hal {
    const char *name;

    int (*prepare)(void);

    /* Page-base v1(base on CMA & MPU), 2MB granularity page base*/
    int (*secure)(u64 paddr, u32 size, u8 pmm_attr);
    int (*unsecure)(u64 paddr, u32 size, u8 pmm_attr);

    /* Region based(base on CMA & MPU), 2MB alignment */
    int (*secure_range)(u64 paddr, u32 size, u8 pmm_attr);
    int (*unsecure_range)(u64 paddr, u32 size, u8 pmm_attr);

    /* Page-base v2(base on buddy-system), page order granularity */
    int (*secure_v2)(u64 paddr, u8 order, u8 pmm_attr);
    int (*unsecure_v2)(u64 paddr, u8 order, u8 pmm_attr);

    int (*sync)(void);
    int (*defragment)(void);
};
