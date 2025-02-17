// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <trace/hooks/sched.h>
#include <linux/sched/clock.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include "fpsgo_base.h"
#include "fpsgo_frame_info.h"


static int yield_pid = -1;
static int yield_duration = 2000;
static int lr_frame_time_buffer = 300000;
static int smallest_yield_time;
static int last_sleep_duration;
static int render_pid = -1;
static int targetFps = -1;
#define GAME_VERSION_MODULE "1.0"
#define MAX_RENDER_TID 10

enum TASK_STATE {
	IDLE_STATE = 0,
};

module_param(yield_duration, int ,0644);
module_param(lr_frame_time_buffer, int ,0644);
module_param(smallest_yield_time, int, 0644);

int (*game2fpsgo_get_fpsgo_frame_info)(int max_num, unsigned long mask,
	struct render_frame_info *frame_info_arr);
EXPORT_SYMBOL(game2fpsgo_get_fpsgo_frame_info);

struct render_info_fps {
	int cur_fps;
	int target_fps;
};

static int game_get_tgid(int pid)
{
	struct task_struct *tsk;
	int tgid = 0;

	rcu_read_lock();
	tsk = find_task_by_vpid(pid);
	if (tsk)
		get_task_struct(tsk);
	rcu_read_unlock();

	if (!tsk)
		return 0;

	tgid = tsk->tgid;
	put_task_struct(tsk);

	return tgid;
}

static int get_render_frame_info(int pid, void *data, unsigned long query_mask)
{
	struct render_frame_info *render;
	int ret = 0;

	render = kcalloc(MAX_RENDER_TID, sizeof(struct render_frame_info), GFP_KERNEL);
	if (!render)
		return -ENOMEM;

	if (!game2fpsgo_get_fpsgo_frame_info) {
		pr_debug("func pointer game2fpsgo_get_fpsgo_frame_info is null");
		goto end;
	}

	ret = game2fpsgo_get_fpsgo_frame_info(MAX_RENDER_TID, query_mask, render);
	if (ret >= 0) {
		int i = 0;
		int render_item = -1;

		for (i = 0; i < ret; i++) {
			if (pid == render[i].pid) {
				render_item = i;
				break;
			}
		}

		if (render_item == -1) {
			ret = -EINVAL;
			goto end;
		}

		if (query_mask == (1 << GET_FPSGO_TARGET_FPS | 1 << GET_FPSGO_QUEUE_FPS)) {
			((struct render_info_fps *)data)->cur_fps = render[render_item].queue_fps;
			((struct render_info_fps *)data)->target_fps = render[render_item].target_fps;
		}
	}
end:
	kfree(render);
	return ret;
}

void game_set_heaviest_pid(int heaviest_pid)
{
	yield_pid = heaviest_pid;
}
EXPORT_SYMBOL_GPL(game_set_heaviest_pid);

void game_set_fps(int pid, int target_fps)
{
	if (pid == render_pid)
		targetFps = target_fps / 1000;
}
EXPORT_SYMBOL_GPL(game_set_fps);

void game_engine_cooler_set_last_sleep_duration(int cur_pid)
{
	if (yield_pid != -1 && game_get_tgid(cur_pid) == game_get_tgid(yield_pid)) {
		last_sleep_duration = 0;
		render_pid = cur_pid;
	}
}
EXPORT_SYMBOL_GPL(game_engine_cooler_set_last_sleep_duration);

void game_register_func(void *funcPtr, void *data)
{
	register_trace_android_rvh_before_do_sched_yield(funcPtr, data);
}
EXPORT_SYMBOL(game_register_func);

int engine_cooler_get_last_sleep_duration(void)
{
	return last_sleep_duration;
}
EXPORT_SYMBOL(engine_cooler_get_last_sleep_duration);

void engine_cooler_set_last_sleep_duration(int duration)
{
	last_sleep_duration = duration;
}
EXPORT_SYMBOL(engine_cooler_set_last_sleep_duration);

int engine_cooler_get_yield_monitor_pid(void)
{
	return yield_pid;
}
EXPORT_SYMBOL(engine_cooler_get_yield_monitor_pid);


int engine_cooler_get_firtst_sleep_duration(void)
{
	return yield_duration * 1000;
}
EXPORT_SYMBOL(engine_cooler_get_firtst_sleep_duration);

int engine_cooler_get_target_fps(void)
{
	int ret = 0;
	struct render_info_fps info;
	unsigned long query_mask = (1 << GET_FPSGO_TARGET_FPS | 1 << GET_FPSGO_QUEUE_FPS);

	if (targetFps == -1) {
		if (render_pid != -1) {
			ret = get_render_frame_info(render_pid, &info, query_mask);
			if (ret >= 0)
				return info.target_fps;
		}
	} else {
		return targetFps;
	}

	return -1;
}
EXPORT_SYMBOL(engine_cooler_get_target_fps);

int engine_cooler_get_lr_frame_time_buffer(void)
{
	return lr_frame_time_buffer;
}
EXPORT_SYMBOL(engine_cooler_get_lr_frame_time_buffer);

int engine_cooler_get_smallest_yield_time(void)
{
	return smallest_yield_time;
}
EXPORT_SYMBOL(engine_cooler_get_smallest_yield_time);

u64 get_now_time(void)
{
	return sched_clock();
}
EXPORT_SYMBOL(get_now_time);

void call_usleep_range_state(int min, int max, int state)
{
	if (state == IDLE_STATE)
		usleep_range_state(min, max, TASK_IDLE);
}
EXPORT_SYMBOL(call_usleep_range_state);

static void __exit game_exit(void)
{

}

static int __init game_init(void)
{
	return 0;
}

module_init(game_init);
module_exit(game_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek GAME");
MODULE_AUTHOR("MediaTek Inc.");
MODULE_VERSION(GAME_VERSION_MODULE);
