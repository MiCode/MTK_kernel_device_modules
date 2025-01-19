/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_APU_MDW_POWER_BUDGET_H__
#define __MTK_APU_MDW_POWER_BUDGET_H__

/* power budget functions */
int mdw_pb_get(enum mdw_pwrplcy_type type, uint32_t debounce_ms);
int mdw_pb_put(enum mdw_pwrplcy_type type);
int mdw_pb_init(struct mdw_device *mdev); //call at device init
void mdw_pb_deinit(struct mdw_device *mdev); //call at device release

#endif