/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_ENERGY_MODEL_H__
#define __MTK_ENERGY_MODEL_H__

extern int mtk_static_power_init(void);
extern unsigned int mtk_get_leakage(unsigned int cpu, unsigned int opp, unsigned int temperature);
extern bool is_wl_support(void);

#endif /*__MTK_ENERGY_MODEL_H__ */
