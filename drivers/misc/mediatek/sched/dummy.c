// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
//#include "common.h"
#define CREATE_TRACE_POINTS

MODULE_LICENSE("GPL");

void set_task_ls(int pid)
{
	pr_info("%s is not support!\n", __func__);
}
EXPORT_SYMBOL_GPL(set_task_ls);

void unset_task_ls(int pid)
{
	pr_info("%s is not support! pid=%d\n", __func__, pid);
}
EXPORT_SYMBOL_GPL(unset_task_ls);

void set_task_priority_based_vip_and_throttle(int pid, int prio, unsigned int throttle_time)
{
	pr_info("%s is not support! pid=%d prio=%d, thtime=%d\n", __func__, pid, prio, throttle_time);
}
EXPORT_SYMBOL_GPL(set_task_priority_based_vip_and_throttle);


void set_task_priority_based_vip(int pid, int prio)
{
	pr_info("%s is not support! pid=%d prio=%d\n", __func__, pid, prio);
}
EXPORT_SYMBOL_GPL(set_task_priority_based_vip);

void unset_task_priority_based_vip(int pid)
{
	pr_info("%s is not support! pid=%d\n", __func__, pid);
}
EXPORT_SYMBOL(unset_task_priority_based_vip);



