// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/sched.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>

#include "common.h"
#include "gather.h"
#include "sugov/cpufreq.h"

#define GEAR_HINTS_GATHERING_TH 30

static int gear_hints_gathering_th = GEAR_HINTS_GATHERING_TH;

bool gather_to_gear(struct task_struct *p, int *end_index)
{
	if (rt_task(p))
		return false;

	if (task_util(p) < READ_ONCE(gear_hints_gathering_th)) {
		*end_index = 0;

		return true;
	}

	return false;
}

void set_gear_hints_gathering_th(int tsk_util)
{
	if (_get_sched_debug_lock() == true)
		return;

	if (tsk_util == -1) {
		WRITE_ONCE(gear_hints_gathering_th, GEAR_HINTS_GATHERING_TH);
		return;
	}

	if (tsk_util < 0 || tsk_util > SCHED_CAPACITY_SCALE)
		return;

	WRITE_ONCE(gear_hints_gathering_th, tsk_util);
}
EXPORT_SYMBOL(set_gear_hints_gathering_th);

void reset_gear_hints_gathering_th(void)
{
	set_gear_hints_gathering_th(-1);
}
EXPORT_SYMBOL(reset_gear_hints_gathering_th);

int get_gear_hints_gathering_th(void)
{
	return gear_hints_gathering_th;
}
EXPORT_SYMBOL_GPL(get_gear_hints_gathering_th);
