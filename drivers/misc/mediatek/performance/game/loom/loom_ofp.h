/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#ifndef _LOOM_CFP_H_
#define _LOOM_CFP_H_
struct cpu_info {
	u64 prev_wall_time;
	u64 prev_idle_time;
	u64 cur_wall_time;
	u64 cur_idle_time;
	int cur_loading;
	int is_isolated;
	// may add util info ??
};
extern int ofp_is_overload;
extern int loom_cpu_num;

void loom_notify_dedicated(int cpu_id, int set);
void loom_ofp_exit(void);
void loom_ofp_init(void);
#endif  // __LOOM_CFP_H__
