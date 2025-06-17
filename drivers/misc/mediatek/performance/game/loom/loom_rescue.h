/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __LOOM_RESCUE_H__
#define __LOOM_RESCUE_H__


void loom_init_jerk(struct loom_jerk *jerk, int id);
int loom_lc_set_jerk(struct loom_loading_ctrl *iter, unsigned long long ts, unsigned long long expected_fps);
void init_loom_rescue(void);

#endif  // __LOOM_RESCUE_H__
