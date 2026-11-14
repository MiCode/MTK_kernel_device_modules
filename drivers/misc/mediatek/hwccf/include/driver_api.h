// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef _MTK_HW_CCF_TEST_DRIVER_API_H
#define _MTK_HW_CCF_TEST_DRIVER_API_H

/* I/O */
#define __raw_readl(addr)         (*(volatile UINT32 *)((uintptr_t)addr))
#define __raw_writel(data, addr)  ((*(volatile UINT32 *)((uintptr_t)addr)) = (UINT32)(data))

#endif /* !_MTK_HW_CCF_TEST_DRIVER_API_H */
