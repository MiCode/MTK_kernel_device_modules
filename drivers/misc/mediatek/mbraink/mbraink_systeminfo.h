/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef MBRAINK_SYSTEMINFO_H
#define MBRAINK_SYSTEMINFO_H

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <mbraink_ioctl_struct_def.h>

int mbraink_systeminfo_init(void);
int mbraink_systeminfo_deinit(void);
int mbraink_get_chipid_info(struct mbraink_chipid_info *chipid_info);

#endif /*end of MBRAINK_MEMORY_H*/
