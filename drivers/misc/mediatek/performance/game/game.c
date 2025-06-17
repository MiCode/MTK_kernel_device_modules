// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/cpumask.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <trace/events/sched.h>
#include <trace/events/task.h>
#include <trace/hooks/sched.h>
#include <linux/sched/cputime.h>
#include <linux/sched/clock.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/sched/task.h>
#include <sched/sched.h>
#include <uapi/linux/sched/types.h>
#include "game.h"
#include "game_sysfs.h"
#include "engine_cooler/game_ec.h"
#include "frame_interpolate/frame_interpolate.h"
#include "loom/loom.h"
#include "common.h"
#include "eas/eas_plus.h"
#include "kernel_core_ctrl.h"

static struct task_struct *kGame_task;

#define GAME_VERSION_MODULE "1.0"

enum TASK_STATE {
	IDLE_STATE = 0,
};

struct render_info_fps {
	int cur_fps;
	int target_fps;
};

void game_lock_from_private(void *lock)
{
	if (lock != NULL)
		mutex_lock(lock);
}
EXPORT_SYMBOL(game_lock_from_private);

void game_unlock_from_private(void *lock)
{
	if (lock != NULL)
		mutex_unlock(lock);
}
EXPORT_SYMBOL(game_unlock_from_private);

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
		case GAME_EVENT_REGISTER_FPSGO_CALLBACK:
			if (((FUNC_REGISTER*)(pack->data))->is_register)
				register_fpsgo_frame_info_callback(((FUNC_REGISTER*)(pack->data))->mask, ((FUNC_REGISTER*)(pack->data))->register_cb);
			else
				unregister_fpsgo_frame_info_callback(((FUNC_REGISTER*)(pack->data))->register_cb);
			free_pack_data_size = sizeof(FUNC_REGISTER);
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

static void mtk_set_cpus_allowed_ptr(void *data, struct task_struct *p,
	struct affinity_context *ctx, bool *skip_user_ptr)
{
	struct cpumask *kernel_allowed_mask = &((struct mtk_task *) android_task_vendor_data(p))->kernel_allowed_mask;
	struct rq_flags rf;
	struct rq *rq = task_rq_lock(p, &rf);
	cpumask_t new_mask;

	// not set or invalid cpu mask
	if (cpumask_empty(kernel_allowed_mask))
		goto out;

	if (p->user_cpus_ptr &&
		!(ctx->flags & (SCA_USER | SCA_MIGRATE_ENABLE | SCA_MIGRATE_DISABLE)) &&
		cpumask_and(rq->scratch_mask, ctx->new_mask, p->user_cpus_ptr)) {
		*skip_user_ptr = true;
		cpumask_copy(rq->scratch_mask, kernel_allowed_mask);
		ctx->new_mask = rq->scratch_mask;
	}
	if (p->user_cpus_ptr && !cpumask_empty(kernel_allowed_mask)){
		cpumask_copy(&new_mask, ctx->new_mask);
		game_print_trace(
		"kernel_core_ctrl: pid = %d, skip_user = %d, user_mask = 0x%x, kernel_allowed_mask = 0x%x, new_mask = 0x%x",
		p->pid,
		*skip_user_ptr,
		cpumask_bits(p->user_cpus_ptr)[0],
		cpumask_bits(kernel_allowed_mask)[0],
		cpumask_bits(&new_mask)[0]);
	}

out:
	task_rq_unlock(rq, p, &rf);
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
	frame_interpolate_exit();

	return 0;
}

static void __exit game_exit(void)
{
	if (kGame_task)
		kthread_stop(kGame_task);
	set_cpus_allowed_ptr_by_kernel_fp = NULL;
	loom_exit();
	game_sysfs_exit();
}

static int __init game_init(void)
{
	int ret = 0;

	ret = register_trace_android_rvh_set_cpus_allowed_ptr(mtk_set_cpus_allowed_ptr, NULL);
	if (ret)
		pr_info("register mtk_set_cpus_allowed_ptr hooks failed, returned %d\n", ret);

	set_cpus_allowed_ptr_by_kernel_fp = &set_cpus_allowed_ptr_by_kernel;

	kGame_task = kthread_create(gameMain, NULL, "kGameThread");
	if (kGame_task == NULL) {
		ret = -EFAULT;
		goto end;
	}
	wake_up_process(kGame_task);
	game_sysfs_init();
	frame_interpolate_init();
	loom_init();
end:
	return ret;
}

module_init(game_init);
module_exit(game_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek GAME");
MODULE_AUTHOR("MediaTek Inc.");
MODULE_VERSION(GAME_VERSION_MODULE);
