/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#ifndef MBRAINK_TOUCH_H
#define MBRAINK_TOUCH_H

#include "mbraink_ioctl_struct_def.h"

extern int mbraink_netlink_send_msg(const char *msg);

int mbraink_touch_init(void);
int mbraink_touch_deinit(void);
int mbraink_get_touch_ghost_info(struct mbraink_touch_ghost_info *touch_ghost_info);

#endif /*end of MBRAINK_TOUCH_H*/
