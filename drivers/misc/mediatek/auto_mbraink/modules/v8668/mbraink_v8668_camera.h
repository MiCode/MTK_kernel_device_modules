/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#ifndef MBRAINK_V8668_CAMERA_H
#define MBRAINK_V8668_CAMERA_H

extern int mbraink_netlink_send_msg(const char *msg);

int mbraink_v8668_camera_init(void);
int mbraink_v8668_camera_deinit(void);

#endif /*end of MBRAINK_V8668_CAMERA_H*/
