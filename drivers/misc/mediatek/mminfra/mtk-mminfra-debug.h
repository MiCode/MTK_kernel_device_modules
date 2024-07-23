/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_MMINFRA_DEBUG_H
#define __MTK_MMINFRA_DEBUG_H

#if IS_ENABLED(CONFIG_MTK_MMINFRA)

enum mm_power_ver {
	mm_pwr_v1 = 1, /* 1st version */
	mm_pwr_v2 = 2, /* mt6989 */
	mm_pwr_v3 = 3, /* mt6991 */
};

enum mm_power {
	MM_0 = 0,
	MM_1,
	MM_AO,
	MM_PWR_NR,
};

enum mmpc_sta {
	MM_DDRSRC = 0,
	MM_EMI,
	MM_BUSPLL,
	MM_INFRA,
	MM_CK26M,
	MM_PMIC,
	MM_VCORE,
	MMPC_NR,
};

int mtk_mminfra_dbg_hang_detect(const char *user, bool skip_pm_runtime);

void mtk_mminfra_off_gipc(void);

#else

static inline int mtk_mminfra_dbg_hang_detect(const char *user, bool skip_pm_runtime)
{
	return 0;
}

static inline void mtk_mminfra_off_gipc(void) { }

#endif /* CONFIG_MTK_MMINFRA */

#endif /* __MTK_MMINFRA_DEBUG_H */
