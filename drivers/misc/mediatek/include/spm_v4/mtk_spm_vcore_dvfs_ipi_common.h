/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 MediaTek Inc.
 */

#ifndef __MTK_SPM_VCORE_DVFS_IPI_H__
#define __MTK_SPM_VCORE_DVFS_IPI_H__

#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6757)

#include "spm_v3/mtk_spm_vcore_dvfs_ipi_mt6775.h"

#elif IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT8788)

#include "mtk_spm_vcore_dvfs_ipi_mt6771.h"

#endif

#endif
