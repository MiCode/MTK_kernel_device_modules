/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef _FI_BASE_H_
#define _FI_BASE_H_

#include <linux/mutex.h>
#include <linux/rbtree.h>
#include <linux/workqueue.h>

#include "fpsgo_frame_info.h"

#define GAME_BY_PID_DEFAULT_VAL -1

enum FI_ENABLE_INFO {
	FI_ENABLE = 0,
	FI_DETECTION = 1,
};

struct fi_notifier_push_tag {
	int tgid;
	int pid;
	unsigned long long buffer_id;
	unsigned long long cur_queue_end_ts;

	struct work_struct sWork;
};

struct fi_policy_info {
	int tgid;
	int pid;
	unsigned long long buffer_id;
	unsigned long long ts;

	int user_target_fps;

	struct hlist_node hlist;
};

struct game_render_info {
	struct rb_node entry;
    struct render_frame_info frame_info;
	unsigned long long updated_ts;

    int frame_count;
    int interpolation_ratio;
	/* param: fi_enabled
	 * 0: disable
	 * 1: enable for mfrc
	 * 2: disable for frame detection
	 * 3: enable for frame detection
	 */
	unsigned long fi_enabled;

	int is_fpsgo_render_created;

	unsigned long long queue_end_ns;
	unsigned long long cpu_time;
	int user_target_fps;
	int fpsgo_target_fps;
	int target_fps_diff;
	unsigned long long old_buffer_id;
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
struct game_render_info *frame_interp_search_and_add_render_info(int tgid, int add_flag);
int game_delete_render_info(struct game_render_info *iter_thr);
int game_get_render_tree_total_num(void);
struct rb_root *game_get_render_pid_tree(void);
int init_fi_base(void);
struct fi_policy_info *fi_get_policy_cmd(int tgid, int pid,
	unsigned long long bufID, int force);
void fi_delete_policy_cmd(struct fi_policy_info *iter);
void game_fi_policy_list_lock(void);
void game_fi_policy_list_unlock(void);
int game_get_fi_policy_list_total_num(void);
struct hlist_head *game_get_fi_policy_cmd_list(void);

#endif  // _FI_BASE_H_