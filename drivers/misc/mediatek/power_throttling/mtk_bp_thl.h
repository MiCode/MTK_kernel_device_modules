/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#ifndef __MTK_BATTERY_PERCENTAGE_THROTTLING_H__
#define __MTK_BATTERY_PERCENTAGE_THROTTLING_H__

#define BATTERY_PERCENT_LEVEL enum BATTERY_PERCENT_LEVEL_TAG
#define BATTERY_PERCENT_PRIO enum BATTERY_PERCENT_PRIO_TAG

enum BATTERY_PERCENT_LEVEL_TAG {
	BATTERY_PERCENT_LEVEL_0 = 0,
	BATTERY_PERCENT_LEVEL_1 = 1,
	BATTERY_PERCENT_LEVEL_2 = 2,
	BATTERY_PERCENT_LEVEL_3 = 3,
	BATTERY_PERCENT_LEVEL_4 = 4,
	BATTERY_PERCENT_LEVEL_5 = 5,
	BATTERY_PERCENT_LEVEL_NUM
};

enum BATTERY_PERCENT_PRIO_TAG {
	BATTERY_PERCENT_PRIO_CPU_B = 0,
	BATTERY_PERCENT_PRIO_CPU_L = 1,
	BATTERY_PERCENT_PRIO_GPU = 2,
	BATTERY_PERCENT_PRIO_MD = 3,
	BATTERY_PERCENT_PRIO_MD5 = 4,
	BATTERY_PERCENT_PRIO_FLASHLIGHT = 5,
	BATTERY_PERCENT_PRIO_VIDEO = 6,
	BATTERY_PERCENT_PRIO_WIFI = 7,
	BATTERY_PERCENT_PRIO_BACKLIGHT = 8,
	BATTERY_PERCENT_PRIO_AUDIO = 9,
	BATTERY_PERCENT_PRIO_APU = 11,
	BATTERY_PERCENT_PRIO_HPT = 13,
	BATTERY_PERCENT_PRIO_UT = 15
};

typedef void (*battery_percent_callback)(BATTERY_PERCENT_LEVEL tag);
typedef void (*bp_md_uisoc_callback)(unsigned int chg_state, unsigned int soc);

#if IS_ENABLED(CONFIG_MTK_BATTERY_PERCENT_THROTTLING)
void register_bp_thl_notify(
		battery_percent_callback bp_cb,
		BATTERY_PERCENT_PRIO prio_val);
void register_bp_thl_md_notify(
		battery_percent_callback md_cb,
		BATTERY_PERCENT_PRIO prio_val);
void register_bp_thl_md_uisoc_notify(
		bp_md_uisoc_callback md_uisoc_cb,
		BATTERY_PERCENT_PRIO prio_val);
void unregister_bp_thl_notify(BATTERY_PERCENT_PRIO prio_val);
void set_bp_thl_ut_status(int status);
#endif
extern void ccci_set_power_throttle_cb(int (*power_throttle_cb)(unsigned int data));


#endif /* __MTK_BATTERY_PERCENTAGE_THROTTLING_H__ */
