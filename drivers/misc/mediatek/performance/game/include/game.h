/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef _GAME_H_
#define _GAME_H_
#include "game_external.h"
#include <linux/rbtree.h>
#include "fpsgo_frame_info.h"

#define MAX_RENDER_SIZE 10

enum GAME_EVENT {
	GAME_EVENT_GET_FPSGO_RENDER_INFO = 0,
	GAME_EVENT_REGISTER_FPSGO_CALLBACK = 1,
};

struct game_package {
	enum GAME_EVENT event;
	int cur_pid;
	struct list_head queue_list;
	void *data;
};

int game_get_tgid(int pid);

typedef struct func_register {
	bool is_register;
	int mask;
	fpsgo_frame_info_callback register_cb; 
} FUNC_REGISTER;

int game_queue_work(struct game_package *pack);
void *game_alloc_atomic(int i32Size);
void game_free(void *pvBuf, int i32Size);
uint64_t get_now_time(void);

void loom_main_trace(const char *fmt, ...);
void game_main_trace(const char *fmt, ...);
void game_print_trace(const char *fmt, ...);
void game_systrace_c(int type, pid_t pid, unsigned long long bufID,
	int value, const char *name, ...);
void game_systrace_b(int type, pid_t pid, const char *name, ...);
void game_systrace_e(int type);
unsigned long long game_get_time(void);

#endif //_GAME_H_
