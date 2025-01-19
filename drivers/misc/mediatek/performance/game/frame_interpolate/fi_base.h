/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef _FI_BASE_H_
#define _FI_BASE_H_

#include <linux/mutex.h>
#include <linux/rbtree.h>

#include "fpsgo_frame_info.h"

enum GAME_TRACE_TYPE {
	GAME_DEBUG_MANDATORY = 0,
};

struct game_render_info {
	struct rb_node entry;
    struct render_frame_info frame_info;
	unsigned long long updated_ts;

    int frame_count;
    int interpolation_ratio;
    int frame_interpolation_enable;

	int is_fpsgo_render_created;

	unsigned long long queue_end_ns;

	struct mutex thr_mlock;
};

/* composite key for render_info rbtree */
struct fpsgo_render_key {
	int key1;
	unsigned long long key2;
};

void game_render_tree_lock(void);
void game_render_tree_unlock(void);
void game_render_tree_lockprove(const char *tag);
void game_main_systrace(pid_t pid, unsigned long long bufID,
	int val, const char *fmt, ...);
void game_main_trace(const char *fmt, ...);
unsigned long long game_get_time(void);
struct game_render_info *frame_interp_search_and_add_render_info(int tgid, int add_flag);
int game_delete_render_info(struct game_render_info *iter_thr);
int game_get_render_tree_total_num(void);
void game_clear_render_info(void);
struct rb_root *game_get_render_pid_tree(void);
int init_fi_base(void);

#endif  // _FI_BASE_H_