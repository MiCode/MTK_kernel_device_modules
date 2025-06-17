/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef MBRAINK_V6993_HRT_H
#define MBRAINK_V6993_HRT_H

#include "mbraink_ioctl_struct_def.h"

typedef void (*hrt_notify_callback)(int threshold);

extern int mbraink_netlink_send_msg(const char *msg);
extern int mtk_mbrain2disp_register_hrt_cb(hrt_notify_callback func);
extern int mtk_mbrain2disp_unregister_hrt_cb(hrt_notify_callback func);

int mbraink_v6993_hrt_init(struct device *dev);
int mbraink_v6993_hrt_deinit(struct device *dev);

#endif /*end of MBRAINK_V6993_HRT_H*/
