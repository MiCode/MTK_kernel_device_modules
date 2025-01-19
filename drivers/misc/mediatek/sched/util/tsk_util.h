/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef _TSK_UTIL_H
#define _TSK_UTIL_H

void hook_mtk_util_est_update(void *data, struct cfs_rq *cfs_rq, struct task_struct *p, bool task_sleep, int *ret);

bool is_runnable_boost_util_est_enable(void);
void set_runnable_boost_util_est_enable(int ctrl);
void reset_runnable_boost_util_est_enable(void);

#endif /* _TSK_UTIL_H */
