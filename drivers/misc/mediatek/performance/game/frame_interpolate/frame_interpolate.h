/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _FRAME_INTERPOLATE_H_
#define _FRAME_INTERPOLATE_H_

extern int fstb_fi_detect_enable;
int init_frame_interpolate(void);
void fpsgo_fi_receive_q2q_cb(unsigned long cmd, struct render_frame_info *iter);
void frame_interpolate_exit(void);
int frame_interpolate_init(void);
void game_clear_render_info(int mode);
int game_switch_frame_inteprolate_onoff(int pid, int fi_info);
extern int (*fstb_get_is_interpolation_is_on_fp)(int pid, unsigned long long bufID, int tgid,
	unsigned long long cur_queue_end_ts, int *target_fps);

#endif  // _FRAME_INTERPOLATION_H_
