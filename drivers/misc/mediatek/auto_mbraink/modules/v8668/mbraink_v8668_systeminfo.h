/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef MBRAINK_V8668_SYSTEMINFO_H
#define MBRAINK_V8668_SYSTEMINFO_H

#include "mbraink_ioctl_struct_def.h"

#define MT6661_RESERVED_VAL 0xce

int mbraink_v8668_systeminfo_init(struct device *dev);
int mbraink_v8668_systeminfo_deinit(struct device *dev);

int mbraink_v8668_pmic_read(void);
#endif /*end of MBRAINK_V8668_SYSTEMINFO_H*/
