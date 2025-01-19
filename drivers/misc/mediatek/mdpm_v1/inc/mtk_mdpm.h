/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

#ifndef _MTK_MDPM_H_
#define _MTK_MDPM_H_

#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6761)
#include "mtk_mdpm_platform_mt6761.h"
#endif

#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6768)
#include "mtk_mdpm_platform_mt6768.h"
#endif

#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6877)
#include "mtk_mdpm_platform_mt6877.h"
#endif

#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6765)
#include "mtk_mdpm_platform_6765.h"
#endif

#endif /* _MTK_MDPM_H_ */
