/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __ADSP_PLATFORM_H__
#define __ADSP_PLATFORM_H__

#include "adsp_platform_interface.h"

struct adspsys_priv;

void adsp_mt_clr_sysirq(u32 cid);
void adsp_mt_clr_auidoirq(u32 cid);
void adsp_mt_disable_wdt(u32 cid);

void adsp_hardware_init(struct adspsys_priv *adspsys);
#endif
