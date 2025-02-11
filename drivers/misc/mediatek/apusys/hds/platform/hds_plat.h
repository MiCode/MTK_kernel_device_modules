/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __MTK_APU_HDS_PLAT_H__
#define __MTK_APU_HDS_PLAT_H__

extern struct hds_plat_func hds_hw_plat_func_1_0;
struct hds_plat_func *hds_plat_get_funcs(uint32_t version_hw, uint32_t version_date, uint32_t version_revision);

#endif
