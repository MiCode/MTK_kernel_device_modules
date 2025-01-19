// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#define pr_fmt(fmt) "[ts_scp]" fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/kthread.h>
#include <uapi/linux/sched/types.h>
#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/of.h>

#include "ts_scp_core.h"

atomic_t ts_scp_scene_flag = ATOMIC_INIT(0);
EXPORT_SYMBOL_GPL(ts_scp_scene_flag);
struct ts_scp_tp_scenario scene;
EXPORT_SYMBOL_GPL(scene);

unsigned int ts_scp_get_scenario_status(struct ts_scp_tp_scenario *scene)
{
	if (!scene) {
        ts_scp_err("Invalid scenario pointer provided\n");
        return 0;
    }
	scene->status = atomic_read(&ts_scp_scene_flag);
	return scene->status;
}

void ts_scp_set_scenario_status(unsigned int scene)
{
	atomic_or(scene, &ts_scp_scene_flag);
    complete(&ts_scp_scene_done);
}
EXPORT_SYMBOL_GPL(ts_scp_set_scenario_status);

void ts_scp_clear_scenario_status(unsigned int scene)
{
	atomic_andnot(scene, &ts_scp_scene_flag);
    complete(&ts_scp_scene_done);
}
EXPORT_SYMBOL_GPL(ts_scp_clear_scenario_status);

unsigned int ts_scp_scene_flag_changed(void)
{
    unsigned int current_status;
	
	current_status = ts_scp_get_scenario_status(&scene);
    if (current_status != scene.status_last) {
        scene.status_last = current_status;
        return 1;
    }
    return 0;
}

void ts_scp_set_scp_enable(void)
{
    ts_scp_set_scenario_status(STAT_SCP_ENABLED);
}
EXPORT_SYMBOL_GPL(ts_scp_set_scp_enable);

void ts_scp_set_scp_disable(void)
{
    ts_scp_clear_scenario_status(STAT_SCP_ENABLED);
}
EXPORT_SYMBOL_GPL(ts_scp_set_scp_disable);

bool ts_scp_check_is_scp_enabled(struct ts_scp_tp_scenario *scene)
{
    unsigned int current_status;

    current_status = ts_scp_get_scenario_status(scene);
    if (current_status & STAT_SCP_IS_ENABLED_MASK) {
        ts_scp_info("scp enabled");
        return true;
    } else {
        ts_scp_info("scp disabled");
        return false;
    }
}
EXPORT_SYMBOL_GPL(ts_scp_check_is_scp_enabled);

bool ts_scp_check_is_ready_probe(struct ts_scp_tp_scenario *scene)
{
    unsigned int current_status;

    current_status = ts_scp_get_scenario_status(scene);
    if (current_status & STAT_READY_PROBE_MASK) {
        ts_scp_info("scp is ready probe");
        return true;
    } else {
        ts_scp_info("scp is not ready probe");
        return false;
    }
}

bool ts_scp_check_is_finger_on_touch(struct ts_scp_tp_scenario *scene)
{
    unsigned int current_status;

    current_status = ts_scp_get_scenario_status(scene);
    if (current_status & STAT_FINGER_ON_TOUCH_MASK) {
        ts_scp_info("has finger on touch");
        return true;
    } else {
        ts_scp_info("no finger on touch");
        return false;
    }
}

bool ts_scp_check_is_crash_too_much(struct ts_scp_tp_scenario *scene)
{
    unsigned int current_status;

    current_status = ts_scp_get_scenario_status(scene);
    if (current_status & STAT_CRASH_TOO_MUCH_MASK) {
        ts_scp_info("scp crash too much");
        return true;
    } else {
        return false;
    }
}



