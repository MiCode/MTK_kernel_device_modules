/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef MBRAINK_V6989_H
#define MBRAINK_V6989_H

#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/pid.h>

struct mbraink_v6989_data {
#define CHRDEV_NAME     "mbraink_v6989_chrdev"
	struct cdev mbraink_v6989_cdev;
};

#endif /*end of MBRAINK_V6989_H*/
