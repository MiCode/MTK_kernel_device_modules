/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#ifndef __MT_SPM_REG_H___
#define __MT_SPM_REG_H___

#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6763)

#include "mtk_spm_reg_mt6763.h"

#elif IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6739)

#include "mtk_spm_reg_mt6739.h"

#elif IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT8788)

#include "mtk_spm_reg_mt6771.h"

#endif

#endif /* __MT_SPM_REG_H___ */

