/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTKFB_LOG_H
#define __MTKFB_LOG_H

#include <linux/kernel.h>
#include <linux/sched/clock.h>

bool g_disp_sync_log;

#define DDPINFO(fmt, arg...)                                                   \
	do {                                                                   \
		if (g_disp_sync_log)                                              \
			pr_info("[DISP]" pr_fmt(fmt), ##arg);     \
	} while (0)

#define DDPMSG(fmt, arg...) pr_info("[DISP]" pr_fmt(fmt), ##arg)

#define DDPERR(fmt, arg...) pr_info("[DISP][E]" pr_fmt(fmt), ##arg)

#endif
