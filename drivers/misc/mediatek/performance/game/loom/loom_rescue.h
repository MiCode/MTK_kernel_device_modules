/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __LOOM_RESCUE_H__
#define __LOOM_RESCUE_H__


void loom_init_jerk(struct loom_jerk *jerk, int id);
int loom_lc_set_jerk(struct loom_loading_ctrl *iter, unsigned long long ts);
void init_loom_rescue(void);

extern void fpsgo_other2fstb_query_fps(int pid, unsigned long long bufID,
		int *target_fps, int *target_fps_ori, int *fps_margin,
		int *target_fpks, int *cooler_on);

#endif  // __LOOM_RESCUE_H__
