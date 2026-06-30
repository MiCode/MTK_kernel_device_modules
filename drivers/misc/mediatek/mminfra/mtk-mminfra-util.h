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
	MM_TYPE_MML,
	MM_TYPE_MMINFRA_DBG,
	MM_TYPE_SMI,
	MM_TYPE_NR
};

enum mm_power_num {
	MM_PWR_MM_0 = 0,
	MM_PWR_MM_1,
	MM_PWR_MM_AO,
	MM_PWR_MM_2,
	MM_PWR_NUM_NR,
};

#if IS_ENABLED(CONFIG_MTK_MMINFRA)

bool mtk_mminfra_is_on(u32 mm_pwr);
void mtk_mminfra_power_debug(u32 mm_pwr);
void mtk_mminfra_all_power_debug(void);

int mtk_mminfra_on_off(bool on_off, u32 mm_pwr, u32 mm_type);

#else

static inline int mtk_mminfra_on_off(bool on_off, u32 mm_pwr, u32 mm_type)
{
        return 0;
}

#endif /* CONFIG_MTK_MMINFRA */

#endif /* MTK_MMINFRA_UTIL_H */
