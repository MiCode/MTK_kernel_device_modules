/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef MBRAINK_V6991_TOUCH_H
#define MBRAINK_V6991_TOUCH_H

#include <mbraink_ioctl_struct_def.h>

extern int mbraink_netlink_send_msg(const char *msg);

int mbraink_v6991_touch_init(void);
int mbraink_v6991_touch_deinit(void);

#endif /*end of MBRAINK_V6991_TOUCH_H*/
