/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __MVPU_DRIVER_H__
#define __MVPU_DRIVER_H__
#include <apusys_core.h>

static struct device *mvpu_dev;
static int mvpu_loglvl_drv;

int mvpu_init(struct apusys_core_info *info);
void mvpu_exit(void);

#endif /* __MVPU_DRIVER_H__ */
