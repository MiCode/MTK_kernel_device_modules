/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef MBRAINK_V6991_H
#define MBRAINK_V6991_H

#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/pid.h>

struct mbraink_v6991_data {
#define CHRDEV_NAME     "mbraink_v6991_chrdev"
	struct cdev mbraink_v6991_cdev;
};

#endif /*end of MBRAINK_V6991_H*/
