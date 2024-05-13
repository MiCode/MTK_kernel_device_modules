/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __MVPU2X_HANDLER_H__
#define __MVPU2X_HANDLER_H__

#include <linux/mutex.h>
#include <linux/kernel.h>
#include "apusys_device.h"

extern struct mutex mvpu_pool_lock;

void mvpu2X_handler_lite_init(void);
int mvpu2X_handler_lite(int type, void *hnd, struct apusys_device *dev);

#endif /* __MVPU2X_HANDLER_H__ */
