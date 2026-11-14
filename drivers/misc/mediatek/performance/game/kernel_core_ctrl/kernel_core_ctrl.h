/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef _KERNEL_CORE_CTRL_H
#define _KERNEL_CORE_CTRL_H

extern int (*set_cpus_allowed_ptr_by_kernel_fp)(struct task_struct *p, const struct cpumask *new_mask);
int set_cpus_allowed_ptr_by_kernel(struct task_struct *p, const struct cpumask *new_mask);

#endif
