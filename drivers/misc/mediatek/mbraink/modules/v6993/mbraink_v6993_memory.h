/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef MBRAINK_V6993_MEMORY_H
#define MBRAINK_V6993_MEMORY_H

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <mbraink_ioctl_struct_def.h>

#if IS_ENABLED(CONFIG_DEVICE_MODULES_SCSI_UFS_MEDIATEK)
#include "ufs-mediatek-mbrain.h"
#endif

struct mbraink_v6993_slbc_info {
	unsigned long long start;
	unsigned long long end;
	unsigned long long slbc_gpu_wb;
	unsigned int count;
};

extern int mbraink_netlink_send_msg(const char *msg);


int mbraink_v6993_memory_init(struct device *dev);
int mbraink_v6993_memory_deinit(struct device *dev);

#if IS_ENABLED(CONFIG_DEVICE_MODULES_SCSI_UFS_MEDIATEK)
int ufs2mbrain_event_notify(struct ufs_mbrain_event *event);
#endif

#endif /*end of MBRAINK_V6993_MEMORY_H*/

