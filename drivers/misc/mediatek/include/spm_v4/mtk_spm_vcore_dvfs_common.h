/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#ifndef _COMMON_MTK_SPM_VCORE_DVFS_H
#define _COMMON_MTK_SPM_VCORE_DVFS_H
#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6757) || IS_ENABLED(CONFIG_MACH_KIBOPLUS)

#include "spm_v2/mtk_spm_vcore_dvfs_mt6757.h"

#elif IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6763) || IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6739)

#include "mtk_spm_vcore_dvfs.h"

#elif IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT8788)

#include "mtk_spm_vcore_dvfs_mt6771.h"

#endif

extern char *spm_vcorefs_dump_dvfs_regs(char *p);

#endif /* _COMMON_MTK_SPM_VCORE_DVFS_H */

