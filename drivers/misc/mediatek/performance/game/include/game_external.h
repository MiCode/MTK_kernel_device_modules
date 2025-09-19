/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef _GAME_EXTERNAL_H_
#define _GAME_EXTERNAL_H_
#include <linux/types.h>

enum GAME_TRACE_TYPE {
	GAME_DEBUG_MANDATORY = 0
};

struct engine_cooler_data {
	int pid;
	int render_pid;
	int heaviest_pid;
	int target_fps;
	int last_sleep_duration_ns;
};

#endif //_GAME_EXTERNAL_H_
