/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#include "game_external.h"
#ifndef _GAME_EC_H_
#define _GAME_EC_H_
void update_engine_cooler_data(int cur_pid);
int get_render_frame_info(struct game_package *pack);

struct engine_cooler_data_internal {
	struct engine_cooler_data data;
	uint64_t latest_update_timestamp;
};

#endif //_GAME_EC_H_
