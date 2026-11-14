// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/cpumask.h>
#include <linux/module.h>
#include <trace/events/sched.h>
#include <trace/events/task.h>
#include <trace/hooks/sched.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include "common.h"
#include "eas/eas_plus.h"
#include "kernel_core_ctrl.h"

int set_cpus_allowed_ptr_by_kernel(struct task_struct *p, const struct cpumask *new_mask)
{
	struct cpumask *kernel_allowed_mask;
	int ret;

	if (!p)
		return -EINVAL;
	kernel_allowed_mask = &((struct mtk_task *) android_task_vendor_data(p))->kernel_allowed_mask;
	cpumask_copy(kernel_allowed_mask, new_mask);
	ret = set_cpus_allowed_ptr(p, new_mask);
	return ret;
}


