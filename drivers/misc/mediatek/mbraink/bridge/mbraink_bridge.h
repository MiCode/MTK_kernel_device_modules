/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef MBRAINK_BRIDGE_H
#define MBRAINK_BRIDGE_H

#include <linux/ioctl.h>
#include <linux/cdev.h>

#include "mbraink_bridge_gps.h"

struct mbraink_bridge_data {
#define CHRDEV_NAME     "mbraink_bridge_chrdev"
	struct cdev mbraink_bridge_cdev;
};

#endif /*end of MBRAINK_BRIDGE_H*/
