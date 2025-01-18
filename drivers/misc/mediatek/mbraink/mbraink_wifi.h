/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#ifndef MBRAINK_WIFI_H
#define MBRAINK_WIFI_H

#include <mbraink_ioctl_struct_def.h>

int mbraink_wifi_init(void);
int mbraink_wifi_deinit(void);
void mbraink_get_wifi_data(unsigned int reason);
#endif

