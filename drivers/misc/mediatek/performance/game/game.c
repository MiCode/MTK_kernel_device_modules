// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/kthread.h>
#include <linux/module.h>
#include <trace/hooks/sched.h>
#include <linux/sched/clock.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/sched/task.h>
#include <uapi/linux/sched/types.h>
#include "game.h"
#include "engine_cooler/game_ec.h"


static struct task_struct *kGame_task;

#define GAME_VERSION_MODULE "1.0"

enum TASK_STATE {
	IDLE_STATE = 0,
};

struct render_info_fps {
	int cur_fps;
	int target_fps;
};

int game_get_tgid(int pid)
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

void *game_alloc_atomic(int i32Size)
{
	void *pvBuf;

	if (i32Size <= PAGE_SIZE) {
		pvBuf = kmalloc(i32Size, GFP_ATOMIC);
		if (pvBuf)
			memset(pvBuf, 0, i32Size);
	} else {
		pvBuf = vmalloc(i32Size);
	}

	return pvBuf;
}

void game_free(void *pvBuf, int i32Size)
{
	if (!pvBuf)
		return;

	if (i32Size <= PAGE_SIZE)
		kfree(pvBuf);
	else
		vfree(pvBuf);
}

void game_noitfy_queue_end(int cur_pid)
{
	update_engine_cooler_data(cur_pid);
}

void game_register_func(void *funcPtr, void *data)
{
	register_trace_android_rvh_before_do_sched_yield(funcPtr, data);
}
EXPORT_SYMBOL(game_register_func);

uint64_t get_now_time(void)
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

static LIST_HEAD(head);
static int condition_notifier_wq;
static DEFINE_MUTEX(notifier_wq_lock);
static DECLARE_WAIT_QUEUE_HEAD(notifier_wq_queue);
int game_queue_work(struct game_package *pack)
{
	if (!kGame_task) {
		pr_debug("NULL WorkQueue\n");
		game_free(pack, sizeof(struct game_package));
		return -1;
	}

	mutex_lock(&notifier_wq_lock);
	list_add_tail(&pack->queue_list, &head);
	condition_notifier_wq = 1;
	mutex_unlock(&notifier_wq_lock);

	wake_up_interruptible(&notifier_wq_queue);
	return 0;
}

static void game_notifier_wq_cb(void)
{
	struct game_package *pack = NULL;
	int free_pack_data_size = 0;

	while (!kthread_should_stop()) {
		wait_event_interruptible(notifier_wq_queue, condition_notifier_wq);

		mutex_lock(&notifier_wq_lock);
		if (!list_empty(&head)) {
			pack = list_first_entry(&head,
				struct game_package, queue_list);
			list_del(&pack->queue_list);
			if (list_empty(&head))
				condition_notifier_wq = 0;
		} else {
			condition_notifier_wq = 0;
		}
		mutex_unlock(&notifier_wq_lock);
		if (!pack)
			goto end;

		switch (pack->event) {
		case GAME_EVENT_GET_FPSGO_RENDER_INFO:
			get_render_frame_info(pack);
			free_pack_data_size = sizeof(unsigned long);
			break;
		default:
			break;
		}

		if (pack->data) {
			game_free(pack->data, free_pack_data_size);
			pack->data = NULL;
		}

		game_free(pack, sizeof(struct game_package));
		pack = NULL;
		free_pack_data_size = 0;
	}
end:
	return;
}

static int gameMain(void *arg)
{
	struct sched_attr attr = {};

	attr.sched_policy = -1;
	attr.sched_flags =
		SCHED_FLAG_KEEP_ALL |
		SCHED_FLAG_UTIL_CLAMP |
		SCHED_FLAG_RESET_ON_FORK;
	attr.sched_util_min = 1;
	attr.sched_util_max = 1024;

	if (sched_setattr_nocheck(current, &attr) != 0)
		pr_debug("%s set uclamp fail\n", __func__);

	set_user_nice(current, -20);

	game_notifier_wq_cb();

	return 0;
}

static void __exit game_exit(void)
{
	if (kGame_task)
		kthread_stop(kGame_task);
}

static int __init game_init(void)
{
	int ret = 0;

	fpsgo2game_noitfy_queue_end = game_noitfy_queue_end;
	kGame_task = kthread_create(gameMain, NULL, "kGameThread");
	if (kGame_task == NULL) {
		ret = -EFAULT;
		goto end;
	}
	wake_up_process(kGame_task);
end:
	return ret;
}

module_init(game_init);
module_exit(game_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek GAME");
MODULE_AUTHOR("MediaTek Inc.");
MODULE_VERSION(GAME_VERSION_MODULE);
