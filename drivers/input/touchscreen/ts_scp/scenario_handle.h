/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef _TS_SCP_SCENARIO_H_
#define _TS_SCP_SCENARIO_H_

#include <linux/atomic.h>

#define STAT_NONE					0x0000
#define STAT_SCP_CRASH				0x0001
#define STAT_SCP_SUSPEND			0x0002
#define STAT_SCP_RESUME				0x0004
#define STAT_SCP_ENABLED			0x0008
#define STAT_SCP_SCP_READY			0x0010
#define STAT_SCP_AP_TOUCH_READY		0x0020
#define STAT_SCP_FINGER_ON_TOUCH	0x0040
#define STAT_SCP_CRASH_TOO_MUCH		0x0080

#define STAT_SCP_IS_ENABLED_MASK	(STAT_SCP_ENABLED)
#define STAT_READY_PROBE_MASK		(STAT_SCP_SCP_READY | STAT_SCP_AP_TOUCH_READY)
#define STAT_FINGER_ON_TOUCH_MASK	(STAT_SCP_FINGER_ON_TOUCH)
#define STAT_CRASH_TOO_MUCH_MASK	(STAT_SCP_CRASH_TOO_MUCH)

struct ts_scp_tp_scenario{
	unsigned int prio;
	unsigned int status;
	unsigned int status_last;
	unsigned int status_changed;
};

extern atomic_t ts_scp_scene_flag;
extern struct ts_scp_tp_scenario scene;

unsigned int ts_scp_get_scenario_status(struct ts_scp_tp_scenario *scenario);
void ts_scp_set_scenario_status(unsigned int scene);
void ts_scp_clear_scenario_status(unsigned int scene);
unsigned int ts_scp_scene_flag_changed(void);
void ts_scp_set_scp_enable(void);
void ts_scp_set_scp_disable(void);
bool ts_scp_check_is_scp_enabled(struct ts_scp_tp_scenario *scene);
bool ts_scp_check_is_ready_probe(struct ts_scp_tp_scenario *scene);
bool ts_scp_check_is_finger_on_touch(struct ts_scp_tp_scenario *scene);
bool ts_scp_check_is_crash_too_much(struct ts_scp_tp_scenario *scene);

#endif
