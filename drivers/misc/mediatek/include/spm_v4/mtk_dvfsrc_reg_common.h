/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#ifndef _COMMON_MTK_DVFSRC_REG_H
#define _COMMON_MTK_DVFSRC_REG_H

#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6763) || defined(CONFIG_MTK_PLAT_POWER_MT6739)

#include "mtk_dvfsrc_reg.h"

#elif IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT8788)

#include "mtk_dvfsrc_reg_mt6771.h"

#endif

#endif /* _COMMON_MTK_DVFSRC_REG_H */

