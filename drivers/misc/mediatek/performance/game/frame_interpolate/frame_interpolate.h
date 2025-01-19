/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _FRAME_INTERPOLATE_H_
#define _FRAME_INTERPOLATE_H_

int init_frame_interpolate(void);
void fpsgo_fi_receive_q2q_cb(unsigned long cmd, struct render_frame_info *iter);
void frame_interpolate_exit(void);
int frame_interpolate_init(void);

#endif  // _FRAME_INTERPOLATION_H_
