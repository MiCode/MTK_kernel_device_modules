// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "fi_base.h"

#include <linux/mutex.h>
#include <linux/rbtree.h>
#include <linux/kmemleak.h>
#include <linux/sched/clock.h>
#include "fpsgo_frame_info.h"
#include "game_sysfs.h"
#include "game.h"

#define FI_MAX_RENDER_INFO_SIZE 20

static int total_render_info_num;
static int total_fi_policy_cmd_num;

static struct rb_root render_pid_tree;
static HLIST_HEAD(fi_policy_cmd_list);

static DEFINE_MUTEX(render_tree_lock);
static DEFINE_MUTEX(fi_policy_cmd_lock);


// --------game render info rbtree manipulation--------
void game_render_tree_lockprove(const char *tag)
{
	WARN_ON(!mutex_is_locked(&render_tree_lock));
}

void game_render_tree_lock(void)
{
	mutex_lock(&render_tree_lock);
}

void game_render_tree_unlock(void)
{
	mutex_unlock(&render_tree_lock);
}

int game_get_render_tree_total_num(void)
{
	// game_render_tree_lockprove(__func__);
	return total_render_info_num;
}

static int game_is_exceed_render_info_limit(void)
{
	int ret = 0;

	if (total_render_info_num >= FI_MAX_RENDER_INFO_SIZE) {
		ret = 1;
#ifdef FPSGO_DEBUG
		struct game_render_info *r_iter = NULL;
		struct rb_node *rbn = NULL;
		for (rbn = rb_first(&render_pid_tree); rbn; rbn = rb_next(rbn)) {
			r_iter = rb_entry(rbn, struct game_render_info, entry);
			FPSGO_LOGE("[base] %s render %d 0x%llx exist\n",
				__func__, r_iter->tgid, r_iter->buffer_id);
		}
#endif
		// TODO: (Ann) Need to delete the oldest render info in the tree.
		// game_delete_render_info(0);
	}

	return ret;
}

struct game_render_info *frame_interp_search_and_add_render_info(int tgid, int add_flag)
{
    struct rb_node **p = &render_pid_tree.rb_node;
	struct rb_node *parent = NULL;
	struct game_render_info *iter_thr = NULL;

    game_render_tree_lockprove(__func__);

    while (*p) {
		parent = *p;
		iter_thr = rb_entry(parent, struct game_render_info, entry);

		if (tgid < iter_thr->frame_info.tgid)
			p = &(*p)->rb_left;
		else if (tgid > iter_thr->frame_info.tgid)
			p = &(*p)->rb_right;
		else
			return iter_thr;
	}

	if (!add_flag || game_is_exceed_render_info_limit())
		return NULL;

	iter_thr = vzalloc(sizeof(struct game_render_info));
	if (!iter_thr)
		return NULL;

	iter_thr->frame_info.tgid = tgid;
	iter_thr->frame_count = 0;
	iter_thr->interpolation_ratio = 2;
	iter_thr->fi_enabled = 0;
	iter_thr->fpsgo_target_fps = 0;
	iter_thr->user_target_fps = 0;
	iter_thr->target_fps_diff = 0;
	iter_thr->cpu_time = 0;
	iter_thr->updated_ts = game_get_time();
	iter_thr->old_buffer_id = 0;

	kmemleak_not_leak(iter_thr);

	rb_link_node(&iter_thr->entry, parent, p);
	rb_insert_color(&iter_thr->entry, &render_pid_tree);
	total_render_info_num++;

	return iter_thr;
}

int game_delete_render_info(struct game_render_info *iter_thr)
{
	// unsigned long long min_ts = ULLONG_MAX;
	// struct game_render_info *tmp_iter = NULL;
	struct game_render_info *min_iter = NULL;
	// struct rb_node *rbn = NULL;
	int is_delete = 0;

	if (iter_thr) {
		min_iter = iter_thr;
		goto delete;
	}

	// rbn = rb_first(&render_pid_tree);
	// while (rbn) {
	// 	tmp_iter = rb_entry(rbn, struct game_render_info, entry);
	// 	if (tmp_iter->updated_ts < min_ts) {
	// 		min_ts = tmp_iter->updated_ts;
	// 		min_iter = tmp_iter;
	// 	}
	// 	rbn = rb_next(rbn);
	// }

delete:
	if (!min_iter)
		return is_delete;

	rb_erase(&min_iter->entry, &render_pid_tree);
	vfree(min_iter);
	total_render_info_num--;
	is_delete = 1;

	return is_delete;
}


struct rb_root *game_get_render_pid_tree(void)
{
	game_render_tree_lockprove(__func__);

	return &render_pid_tree;
}

// ------------------------------------------------

struct fi_policy_info *fi_get_policy_cmd(int tgid, int pid,
	unsigned long long bufID, int force)
{
	struct fi_policy_info *iter = NULL;
	struct hlist_node *h = NULL;

	hlist_for_each_entry_safe(iter, h, &fi_policy_cmd_list, hlist) {
		if (iter->tgid == tgid) {
			iter->ts = game_get_time();
			break;
		}
	}

	if (iter || !force)
		goto out;

	iter = kzalloc(sizeof(struct fi_policy_info), GFP_KERNEL);
	if (!iter)
		goto out;

	iter->tgid = tgid;
	iter->pid = pid;
	iter->buffer_id = bufID;
	iter->user_target_fps = 0;
	iter->ts = game_get_time();

	hlist_add_head(&iter->hlist, &fi_policy_cmd_list);
	total_fi_policy_cmd_num++;

	if (total_fi_policy_cmd_num > FI_MAX_RENDER_INFO_SIZE)
		fi_delete_policy_cmd(NULL);

out:
	return iter;
}

void fi_delete_policy_cmd(struct fi_policy_info *iter)
{
	unsigned long long min_ts = ULLONG_MAX;
	struct fi_policy_info *tmp_iter = NULL, *min_iter = iter;
	struct hlist_node *h = NULL;

	if (iter)
		goto delete;

	hlist_for_each_entry_safe(tmp_iter, h, &fi_policy_cmd_list, hlist) {
		if (tmp_iter->ts < min_ts) {
			min_ts = tmp_iter->ts;
			min_iter = tmp_iter;
		}
	}

	if (!min_iter)
		return;

delete:
	hlist_del(&min_iter->hlist);
	kfree(min_iter);
	total_fi_policy_cmd_num--;
}

void game_fi_policy_list_lock(void)
{
	mutex_lock(&fi_policy_cmd_lock);
}

void game_fi_policy_list_unlock(void)
{
	mutex_unlock(&fi_policy_cmd_lock);
}

int game_get_fi_policy_list_total_num(void)
{
	return total_fi_policy_cmd_num;
}

struct hlist_head *game_get_fi_policy_cmd_list(void)
{
	return &fi_policy_cmd_list;
}

int init_fi_base(void)
{
	render_pid_tree = RB_ROOT;
	return 0;
}