// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Kenny Liu <kenny.liu@mediatek.com>
 */

#ifndef MTK_MMINFRA_UTIL_H
#define MTK_MMINFRA_UTIL_H

enum mm_type {
	MM_TYPE_CG_LINK = 0,
	MM_TYPE_CMDQ,
	MM_TYPE_DISP,
	MM_TYPE_NR
};

enum mm_power_num {
	MM_0 = 0,
	MM_1,
	MM_AO,
	MM_PWR_NR,
};

#if IS_ENABLED(CONFIG_MTK_MMINFRA)

int mtk_mminfra_on_off(bool on_off, u32 mm_pwr, u32 mm_type);

#else

static inline int mtk_mminfra_on_off(bool on_off, u32 mm_pwr, u32 mm_type)
{
        return 0;
}

#endif /* CONFIG_MTK_MMINFRA */

#endif /* MTK_MMINFRA_UTIL_H */
