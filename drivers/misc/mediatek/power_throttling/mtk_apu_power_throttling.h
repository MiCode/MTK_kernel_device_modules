/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Victor Lin <Victor-wc.lin@mediatek.com>
 */

#ifndef _MTK_APU_POWER_THROTTLING_H_
#define _MTK_APU_POWER_THROTTLING_H_

enum apu_pt_type {
	LBAT_POWER_THROTTLING,
	OC_POWER_THROTTLING,
	SOC_POWER_THROTTLING,
	POWER_THROTTLING_TYPE_MAX
};

typedef int (*apu_throttle_callback)(int *request_id, unsigned long state);
int register_pt_low_battery_apu_cb(apu_throttle_callback cb);
int register_pt_over_current_apu_cb(apu_throttle_callback cb);
int register_pt_battery_percent_apu_cb(apu_throttle_callback cb);

#endif
