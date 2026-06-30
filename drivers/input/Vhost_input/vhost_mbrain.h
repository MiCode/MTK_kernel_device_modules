/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#if IS_ENABLED(CONFIG_TOUCHSCREEN_VHOST_TOUCH_ENABLE)
#ifndef ILITEK_MBRAIN_H
#define ILITEK_MBRAIN_H

#include <linux/notifier.h>

int touch_mb_register(struct notifier_block *nb);
int touch_mb_unregister(struct notifier_block *nb);
int touch_mb_event_trigger(unsigned long event_type, void *latency);

#endif
#endif
