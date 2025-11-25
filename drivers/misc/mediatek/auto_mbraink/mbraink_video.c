// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/rtc.h>
#include <linux/sched/clock.h>

#include "mbraink_video.h"

int mbraink_get_video_info(char *buffer)
{
	pr_info("%s: Do not support ioctl video query.\n", __func__);
	return 0;
}

int mbraink_get_vdec_fps_info(unsigned short pid)
{
	return 0;
}

