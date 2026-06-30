/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef _FFA_COMMON_MTK_H
#define _FFA_COMMON_MTK_H

#if defined(MTK_ADAPTED) && MTK_ADAPTED

void arm_ffa_bus_exit(void);
int arm_ffa_bus_init(void);

#endif /* defined(MTK_ADAPTED) && MTK_ADAPTED */

#endif /* _FFA_COMMON_MTK_H */

