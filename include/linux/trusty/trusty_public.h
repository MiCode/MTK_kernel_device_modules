/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2020 Google, Inc.
 */
/*
 * Copyright (C) 2025 MediaTek Inc.
 */

#ifndef __LINUX_TRUSTY_TRUSTY_PUBLIC_H
#define __LINUX_TRUSTY_TRUSTY_PUBLIC_H

#include <linux/types.h>

#include "ffa_v11/arm_ffa.h"

struct ffa_device *trusty_ffa_get_dev(void);
u32 is_google_real_driver(void);

#endif /* __LINUX_TRUSTY_TRUSTY_PUBLIC_H */
