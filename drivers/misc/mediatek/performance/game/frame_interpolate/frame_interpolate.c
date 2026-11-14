// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <trace/hooks/sched.h>
#include <linux/sched/clock.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/rbtree.h>
#include <linux/vmalloc.h>
#include <linux/kmemleak.h>
#include "fpsgo_frame_info.h"
#include "game.h"
#include "game_sysfs.h"
#include "game_trace_event.h"
#include "fi_base.h"
#include "frame_interpolate.h"

#if IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)
#include "ged_kpi.h"
#endif

#define FRAME_INTERPOLATE_VERSION_MODULE "1.0"
#define GAME_MAX_DEP_NUM 50
#define GAME_MAX_FRAME_COUNT INT_MAX
#define FRAME_INTERPOLATE_BUFFER_ID 1234

static int is_registered_cb;
static int is_registered_detect_cb;
static int is_registered_all_fi_cb;
static int frame_interpolation_enable;
static int fi_detect_enable;
static struct kobject *frame_interpolation_kobj;

static DEFINE_MUTEX(fi_lock);
static DEFINE_MUTEX(fi_cb_lock);

static struct workqueue_struct *fi_target_fps_wq;

static long long fpsgo_task_sched_runtime(struct task_struct *p)
{
	//return task_sched_runtime(p);
	return p ? p->se.sum_exec_runtime : 0;
}

static int frame_get_heaviest_tid(int rpid, int tgid, int *l_tid,
	unsigned long long prev_ts, unsigned long long last_ts)
{
	int max_tid = -1;
	unsigned long long tmp_runtime, max_runtime = 0;
	struct task_struct *gtsk, *sib;

	if (last_ts - prev_ts < NSEC_PER_SEC)
		return 0;

	rcu_read_lock();
	gtsk = find_task_by_vpid(tgid);
	if (gtsk) {
		get_task_struct(gtsk);
		for_each_thread(gtsk, sib) {
			get_task_struct(sib);
			if (sib->pid == rpid) {
				put_task_struct(sib);
				continue;
			}
			tmp_runtime = (u64)fpsgo_task_sched_runtime(sib);
			if (tmp_runtime > max_runtime) {
				max_runtime = tmp_runtime;
				max_tid = sib->pid;
			}
			put_task_struct(sib);
		}
		put_task_struct(gtsk);
	}
	rcu_read_unlock();

	if (max_tid > 0 && max_runtime > 0)
		*l_tid = max_tid;
	else
		*l_tid = -1;

	return 1;
}

static void fi_queuework_cb(struct work_struct *psWork)
{
	struct fi_notifier_push_tag *vpPush = NULL;
	struct game_render_info *iter_thr = NULL;
	struct render_fps_info fps_info;
	int target_fps = 0, fps_info_ret = 0;

	vpPush = container_of(psWork, struct fi_notifier_push_tag, sWork);
	// TODO(Ann): It has still a bug when calculating target fps.
	target_fps = fpsgo_other2fstb_calculate_target_fps(2, vpPush->pid,
		vpPush->buffer_id, vpPush->cur_queue_end_ts);

	fps_info.target_fps_diff = 0;
	fps_info_ret = fpsgo_other2fstb_get_fps_info(vpPush->pid, vpPush->buffer_id,
		&fps_info);

	game_render_tree_lock();
	iter_thr = frame_interp_search_and_add_render_info(vpPush->tgid, 0);
	if (iter_thr) {
		iter_thr->fpsgo_target_fps = target_fps;
		if (!fps_info_ret)
			iter_thr->target_fps_diff = fps_info.target_fps_diff;
	}
	game_render_tree_unlock();

	kfree(vpPush);
}

static void fi_queuework_calculate_target_fps(int pid, int tgid, unsigned long long bufid,
	unsigned long long q_end_ts)
{
	struct fi_notifier_push_tag *vpPush = NULL;

	vpPush = kzalloc(sizeof(struct fi_notifier_push_tag), GFP_ATOMIC);
	if (!vpPush)
		goto out;

	vpPush->tgid = tgid;
	vpPush->pid = pid;
	vpPush->buffer_id = bufid;
	vpPush->cur_queue_end_ts = q_end_ts;

	INIT_WORK(&vpPush->sWork, fi_queuework_cb);
	queue_work(fi_target_fps_wq, &vpPush->sWork);

out:
	return;
}

static void frame_interpolate_fpsgo_render_control(struct game_render_info *iter, unsigned long long ts)
{
	int dep_num = 1, i = 0;
	struct task_info *dep_arr;
	int set_dep_ret = 0;
	unsigned long long raw_running_time = 0, ema_running_time = 0,
		enq_running_time = 0, start_time = 0, end_time = 0;
	int logic_tid = 0;

	if (!iter)
		return;

	start_time = iter->queue_end_ns;
	end_time = ts;
	iter->queue_end_ns = end_time;

	dep_arr = kcalloc(GAME_MAX_DEP_NUM, sizeof(struct task_info), GFP_KERNEL);
	if (!dep_arr)
		return;

	frame_get_heaviest_tid(iter->frame_info.pid, iter->frame_info.tgid, &logic_tid,
		start_time, end_time);
	if (logic_tid) {
		dep_arr[0].pid = logic_tid;
		dep_arr[0].action = XGF_ADD_DEP_FORCE_CPU_TIME;
	}

	set_dep_ret = fpsgo_other2xgf_set_critical_tasks(iter->frame_info.pid,
		iter->frame_info.buffer_id, dep_arr, dep_num, 1);

	fpsgo_other2xgf_calculate_dep(iter->frame_info.pid, iter->frame_info.buffer_id,
		&raw_running_time, &ema_running_time, &enq_running_time, start_time, end_time,
		0, 0, 0, 0, 0);
	game_main_trace("[fpsgo_other2xgf_calculate_dep]: raw_r_ts=%llu, ema=%llu, enq=%llu",
		raw_running_time, ema_running_time, enq_running_time);
	iter->cpu_time = ema_running_time;

	dep_num = fpsgo_other2xgf_get_critical_tasks(iter->frame_info.pid, GAME_MAX_DEP_NUM,
		dep_arr, 1, iter->frame_info.buffer_id);

	for(i = 0; i < dep_num; i++)
		game_main_trace("[game_dep_list] pid=%d", dep_arr[i].pid);

	fpsgo_other2comp_report_workload(iter->frame_info.tgid, iter->frame_info.pid,
		iter->frame_info.buffer_id, ema_running_time, end_time);

	game_main_trace("[%s] ts=%llu,pid=%d,tgid=%d,dep_num=%d,set_dep_ret=%d",
		__func__, ts, iter->frame_info.pid, iter->frame_info.tgid, dep_num, set_dep_ret);

	kfree(dep_arr);
}

static int game_register_queue_end_cb(int enable, int *is_registered, fpsgo_frame_info_callback cb)
{
	int ret = 0;
	unsigned long cb_mask;

	cb_mask = 1 << GET_FPSGO_QUEUE_END |
		1 << GET_FPSGO_QUEUE_START |
		1 << GET_FPSGO_DEQUEUE_START |
		1 << GET_FPSGO_DEQUEUE_END |
		1 << GET_FPSGO_BUFFER_TIME;

	switch(enable) {
	case 0:
		if (*is_registered) {
			mutex_lock(&fi_cb_lock);
			unregister_fpsgo_frame_info_callback(cb);
			*is_registered = 0;
			mutex_unlock(&fi_cb_lock);
		}
		break;
	case 1:
		if (!(*is_registered)) {
			mutex_lock(&fi_cb_lock);
			register_fpsgo_frame_info_callback(cb_mask, cb);
			*is_registered = 1;
			mutex_unlock(&fi_cb_lock);
		}
		break;
	case -1:
		break;

	}

	return ret;
}

int game_switch_frame_inteprolate_onoff(int pid, int enable)
{
	struct game_render_info *iter_thr = NULL;
	int delete = 0, num = 0;

	switch(enable) {
	case 0:
		game_render_tree_lock();
		iter_thr = frame_interp_search_and_add_render_info(pid, 0);
		if (iter_thr) {
			if (iter_thr->is_fpsgo_render_created) {
				fpsgo_other2comp_user_close(iter_thr->frame_info.tgid, iter_thr->frame_info.pid,
					iter_thr->frame_info.buffer_id);
				switch_fpsgo_control(0, iter_thr->frame_info.tgid, 1, 0);
				iter_thr->is_fpsgo_render_created = 0;
			}
			delete = game_delete_render_info(iter_thr);
		}
		num = game_get_render_tree_total_num();
		game_render_tree_unlock();
		if (!num)
			game_register_queue_end_cb(0, &is_registered_cb, &fpsgo_fi_receive_q2q_cb);
		break;
	case 1:
		game_render_tree_lock();
		iter_thr = frame_interp_search_and_add_render_info(pid, 1);
		if (!iter_thr)
			return 1;
		set_bit(FI_ENABLE, &iter_thr->fi_enabled);
		clear_bit(FI_DETECTION, &iter_thr->fi_enabled);
		game_render_tree_unlock();

		game_register_queue_end_cb(1, &is_registered_cb, &fpsgo_fi_receive_q2q_cb);
		break;
	}

	game_main_trace("[%s] tgid=%d, enable=%d, delete=%d, num=%d", __func__, pid, enable,
		delete, num);

	return 0;
}

void game_fi_set_user_target_fps(int tgid, int target_fps)
{
	struct fi_policy_info *iter = NULL;
	int add = 0;

	if (target_fps > 0)
		add = 1;

	game_fi_policy_list_lock();
	iter = fi_get_policy_cmd(tgid, 0, 0, add);
	if (iter) {
		if (target_fps > 0)
			iter->user_target_fps = target_fps;
		else
			fi_delete_policy_cmd(iter);
	}


	game_fi_policy_list_unlock();

}

/*
 *	game_clear_render_info(int mode)
 *	to delete all game render info in the rb_tree (render_pid_tree)
 *	input:
 *		mode: 0->clear all info, 1->clear only FI_DETECTION mode.
 */
void game_clear_render_info(int mode)
{
	struct rb_root *root = NULL;
	struct rb_node *n = NULL;
	struct game_render_info *tmp_iter = NULL;
	int num = 0;

	game_render_tree_lock();

	root = game_get_render_pid_tree();
	n = rb_first(root);

	while (n) {
		tmp_iter = rb_entry(n, struct game_render_info, entry);
		if (tmp_iter) {
			if (mode && !test_bit(FI_DETECTION, &tmp_iter->fi_enabled)) {
				n = rb_next(n);
				continue;
			}
			fpsgo_other2comp_user_close(tmp_iter->frame_info.tgid, tmp_iter->frame_info.pid,
				tmp_iter->frame_info.buffer_id);
			switch_fpsgo_control(0, tmp_iter->frame_info.tgid, 1, 0);
			game_delete_render_info(tmp_iter);
		}

		root = game_get_render_pid_tree();
		n = rb_first(root);
	}

	num = game_get_render_tree_total_num();
	game_render_tree_unlock();
	if (!num)
		game_register_queue_end_cb(0, &is_registered_cb, &fpsgo_fi_receive_q2q_cb);
}

static void game_set_frame_render_val(int cmd, int value, int pid, int add)
{
	switch(cmd) {
	case 0:  // frame_interpolate_enable_by_pid
		game_switch_frame_inteprolate_onoff(pid, value);
		break;
	default:
		break;
	}

	game_main_trace("[%s] cmd=%d, pid=%d", __func__, cmd, pid);
}

static void game_set_fi_policy_val(int cmd, int value, int pid, int add)
{
	switch(cmd) {
	case 0:  // frame_interpolate_enable_by_pid
		// TODO (Ann): Add frame interpolation enable by pid node.
		break;
	case 1: // set_user_target_fps
		game_fi_set_user_target_fps(pid, value);
		break;
	default:
		break;
	}

	game_main_trace("[%s] cmd=%d, pid=%d", __func__, cmd, pid);
}

void fpsgo_fi_receive_q2q_cb(unsigned long cmd, struct render_frame_info *iter)
{
	int pid;
	struct game_render_info *iter_thr = NULL;
	struct fi_policy_info *fi_policy = NULL;
	unsigned long long ts = 0;
	int set_target_fps = 0, gpu_set_target_fps = 0;
	int is_control_frame = 0;

	if (!iter)
		goto out;

	ts = game_get_time();
	pid = iter->tgid;

	game_render_tree_lock();
	iter_thr = frame_interp_search_and_add_render_info(pid, 0);
	if (!iter_thr)
		goto out;

	is_control_frame = !(iter_thr->frame_count % iter_thr->interpolation_ratio);

	if (test_bit(FI_ENABLE, &iter_thr->fi_enabled)) {
		iter_thr->updated_ts = ts;
		if (!iter_thr->is_fpsgo_render_created) {
			iter_thr->frame_info = *iter;
			iter_thr->old_buffer_id = iter->buffer_id;
			iter_thr->frame_info.buffer_id = FRAME_INTERPOLATE_BUFFER_ID;
			fpsgo_other2comp_user_create(iter_thr->frame_info.tgid, iter_thr->frame_info.pid,
				iter_thr->frame_info.buffer_id, NULL, 0, 0);
			iter_thr->is_fpsgo_render_created = 1;
		}

		if (test_bit(GET_FPSGO_BUFFER_TIME, &cmd)) {
			fpsgo_other2comp_set_timestamp(iter_thr->frame_info.tgid, iter_thr->frame_info.pid,
				iter_thr->frame_info.buffer_id, FPSGO_BUFFER_QUOTA, ts);
		} else if (is_control_frame && test_bit(GET_FPSGO_QUEUE_START, &cmd)) {
			fpsgo_other2comp_set_timestamp(iter_thr->frame_info.tgid, iter_thr->frame_info.pid,
				iter_thr->frame_info.buffer_id, FPSGO_ENQUEUE_START, ts);
		} else if (is_control_frame && test_bit(GET_FPSGO_DEQUEUE_START, &cmd)) {
			fpsgo_other2comp_set_timestamp(iter_thr->frame_info.tgid, iter_thr->frame_info.pid,
				iter_thr->frame_info.buffer_id, FPSGO_DEQUEUE_START, ts);
		} else if (is_control_frame && test_bit(GET_FPSGO_DEQUEUE_END, &cmd)) {
			fpsgo_other2comp_set_timestamp(iter_thr->frame_info.tgid, iter_thr->frame_info.pid,
				iter_thr->frame_info.buffer_id, FPSGO_DEQUEUE_END, ts);
			fpsgo_other2fbt_deq_end(iter_thr->frame_info.tgid, iter_thr->frame_info.pid,
				iter_thr->frame_info.buffer_id);
		} else if (test_bit(GET_FPSGO_QUEUE_END, &cmd)) {
			switch_fpsgo_control(0, iter_thr->frame_info.tgid, 0, 0);

			fi_queuework_calculate_target_fps(iter_thr->frame_info.pid, iter_thr->frame_info.tgid,
			iter_thr->frame_info.buffer_id, ts);
			game_fi_policy_list_lock();
			fi_policy = fi_get_policy_cmd(iter_thr->frame_info.tgid, 0, 0, 0);
			if (fi_policy)
				iter_thr->user_target_fps = fi_policy->user_target_fps;
			game_fi_policy_list_unlock();

			set_target_fps =
				iter_thr->user_target_fps ? iter_thr->user_target_fps : iter_thr->fpsgo_target_fps;
			gpu_set_target_fps = set_target_fps;
			do_div(set_target_fps, iter_thr->interpolation_ratio);

			fpsgo_other2fstb_set_target(1, iter_thr->frame_info.pid, 1, 0, set_target_fps, 0,
				iter_thr->frame_info.buffer_id);

	#if IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)
			if (!test_bit(FI_DETECTION, &iter_thr->fi_enabled))  // mfrc only
				do_div(gpu_set_target_fps, iter_thr->interpolation_ratio);

			ged_kpi_set_target_FPS_margin(iter_thr->old_buffer_id, gpu_set_target_fps,
				0, iter_thr->target_fps_diff, iter_thr->cpu_time);
	#endif  // IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)

			if (is_control_frame)
				frame_interpolate_fpsgo_render_control(iter_thr, ts);

			iter_thr->frame_count++;
			iter_thr->frame_count = iter_thr->frame_count % GAME_MAX_FRAME_COUNT;
		}

		game_main_trace("[%s] cmd=%lu, tgid=%d, frame=%d, ts=%llu, target_fps=%d, set_fps=%d",
			__func__, cmd, iter_thr->frame_info.tgid, iter_thr->frame_count, ts,
			iter_thr->fpsgo_target_fps, set_target_fps);
	} else {
		if (iter_thr->is_fpsgo_render_created) {
			fpsgo_other2comp_user_close(iter_thr->frame_info.tgid, iter_thr->frame_info.pid,
				iter_thr->frame_info.buffer_id);
			switch_fpsgo_control(0, iter_thr->frame_info.tgid, 1, 0);
			iter_thr->is_fpsgo_render_created = 0;
		}
	}
out:
	game_render_tree_unlock();
	return;
}
EXPORT_SYMBOL(fpsgo_fi_receive_q2q_cb);

void fpsgo_fi_receive_fi_detect_cb(unsigned long cmd, struct render_frame_info *iter)
{
	int pid = 0;
	struct game_render_info *iter_thr = NULL;
	int target_fps = 0, is_interpolation_on = 0;

	if (!iter)
		goto out;

	pid = iter->tgid;

	game_render_tree_lock();
	iter_thr = frame_interp_search_and_add_render_info(pid, 1);

	if (!iter_thr)
		goto out;

	// Distinct from mfrc.
	if (!test_bit(FI_ENABLE, &iter_thr->fi_enabled)) {
		set_bit(FI_DETECTION, &iter_thr->fi_enabled);
		set_bit(FI_ENABLE, &iter_thr->fi_enabled);
		iter_thr->frame_info = *iter;
		iter_thr->frame_info.buffer_id = FRAME_INTERPOLATE_BUFFER_ID;
		iter_thr->interpolation_ratio = 1;
	}

	if (!test_bit(FI_DETECTION, &iter_thr->fi_enabled))
		goto out;

	if (fstb_get_is_interpolation_is_on_fp)
		is_interpolation_on = fstb_get_is_interpolation_is_on_fp(iter_thr->frame_info.pid,
			iter_thr->frame_info.buffer_id, iter_thr->frame_info.tgid, 0, &target_fps);
	if (is_interpolation_on == 1) {
		iter_thr->interpolation_ratio = 2;
		iter_thr->user_target_fps = target_fps;
	} else {
		iter_thr->interpolation_ratio = 1;
		iter_thr->user_target_fps = 0;
	}

	if (iter_thr->target_fps_diff != 0) {
		iter_thr->interpolation_ratio = 1;
		iter_thr->user_target_fps = 0;
	}

	game_main_trace("[%s] pid=%d, buf=%llx, tgid=%d, target_fps=%d, fi_enabled=%lu, interpol=%d, frs=%d",
		__func__, iter_thr->frame_info.pid, iter_thr->frame_info.buffer_id, iter_thr->frame_info.tgid,
		target_fps, iter_thr->fi_enabled, is_interpolation_on, iter_thr->target_fps_diff);
out:
	game_render_tree_unlock();
	return;
}
EXPORT_SYMBOL(fpsgo_fi_receive_fi_detect_cb);

void fpsgo_fi_receive_all_fi_cb(unsigned long cmd, struct render_frame_info *iter)
{
	int pid;
	struct game_render_info *iter_thr = NULL;
	int target_fps = 0;

	if (!iter)
		goto out;

	pid = iter->tgid;

	game_render_tree_lock();
	iter_thr = frame_interp_search_and_add_render_info(pid, 1);

	if (!iter_thr)
		goto out;

	if (!test_bit(FI_ENABLE, &iter_thr->fi_enabled)) {
		set_bit(FI_DETECTION, &iter_thr->fi_enabled);
		set_bit(FI_ENABLE, &iter_thr->fi_enabled);
		iter_thr->frame_info = *iter;
		iter_thr->frame_info.buffer_id = FRAME_INTERPOLATE_BUFFER_ID;
		if (fstb_get_is_interpolation_is_on_fp)
			fstb_get_is_interpolation_is_on_fp(iter_thr->frame_info.pid, iter_thr->frame_info.buffer_id,
				iter_thr->frame_info.tgid, 0, &target_fps);
		iter_thr->interpolation_ratio = 2;
		iter_thr->user_target_fps = target_fps;
	}

	if (test_bit(FI_DETECTION, &iter_thr->fi_enabled)) {
		if (iter_thr->target_fps_diff != 0) {
			iter_thr->interpolation_ratio = 1;
			iter_thr->user_target_fps = 0;
		}
	}

	game_main_trace("[%s] pid=%d, buf=%llu, tgid=%d, target_fps=%d, fi_enabled=%lu, ratio=%d, frs=%d",
		__func__, iter->pid, iter->buffer_id, iter->tgid, target_fps,
		iter_thr->fi_enabled, iter_thr->interpolation_ratio,
		iter_thr->target_fps_diff);
out:
	game_render_tree_unlock();
}
EXPORT_SYMBOL(fpsgo_fi_receive_all_fi_cb);

static int game_switch_fi_detect_onoff(int fi_detect_active)
{
	switch(fi_detect_active) {
	case 0:
	case 2:
		// Only clear all FI_DETECTION's render.
		fstb_fi_detect_enable = 0;
		game_clear_render_info(1);
		game_register_queue_end_cb(0, &is_registered_detect_cb,
			&fpsgo_fi_receive_fi_detect_cb);
		game_register_queue_end_cb(0, &is_registered_all_fi_cb,
			&fpsgo_fi_receive_all_fi_cb);
		break;
	case 1:
		fstb_fi_detect_enable = 1;
		game_clear_render_info(1);
		game_register_queue_end_cb(0, &is_registered_all_fi_cb,
			&fpsgo_fi_receive_all_fi_cb);
		game_register_queue_end_cb(1, &is_registered_detect_cb,
			&fpsgo_fi_receive_fi_detect_cb);
		game_register_queue_end_cb(1, &is_registered_cb, &fpsgo_fi_receive_q2q_cb);
		break;
	case 3:
		// TODO(Ann): Activate 3.
		fstb_fi_detect_enable = 1;
		game_clear_render_info(1);
		game_register_queue_end_cb(0, &is_registered_detect_cb,
			&fpsgo_fi_receive_fi_detect_cb);
		game_register_queue_end_cb(1, &is_registered_all_fi_cb,
			&fpsgo_fi_receive_all_fi_cb);
		game_register_queue_end_cb(1, &is_registered_cb, &fpsgo_fi_receive_q2q_cb);
		break;
	}

	return 0;
}

static ssize_t fi_detect_enable_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char *acBuffer = NULL;
	int arg;
	int min = 0, max = 3;

	acBuffer = kcalloc(FI_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FI_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FI_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0) {
				if (arg >= (min) && arg <= (max)) {
					mutex_lock(&fi_lock);
					fi_detect_enable = arg;
					game_switch_fi_detect_onoff(fi_detect_enable);
					mutex_unlock(&fi_lock);
				}
			}
		}
	}
out:
	kfree(acBuffer);
	return count;
}

#define FRAME_INTERPOLATION_SYSFS_READ(name, show, variable); \
static ssize_t name##_show(struct kobject *kobj, \
		struct kobj_attribute *attr, \
		char *buf) \
{ \
	if ((show)) \
		return scnprintf(buf, PAGE_SIZE, "%d\n", (variable)); \
	else \
		return 0; \
}

#define FRAME_INTERPOLATION_SYSFS_WRITE_VALUE(name, cmd, min, max); \
static ssize_t name##_store(struct kobject *kobj, \
		struct kobj_attribute *attr, \
		const char *buf, size_t count) \
{ \
	char *acBuffer = NULL; \
	int arg; \
\
	acBuffer = kcalloc(FI_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL); \
	if (!acBuffer) \
		goto out; \
\
	if ((count > 0) && (count < FI_SYSFS_MAX_BUFF_SIZE)) { \
		if (scnprintf(acBuffer, FI_SYSFS_MAX_BUFF_SIZE, "%s", buf)) { \
			if (kstrtoint(acBuffer, 0, &arg) == 0) { \
				if (arg >= (min) && arg <= (max)) \
					game_set_frame_render_val(cmd, arg, -1, 1); \
			} \
		} \
	} \
\
out: \
	kfree(acBuffer); \
	return count; \
}

#define FRAME_INTERPOLATE_SYSFS_WRITE_PID_CMD(name, cmd, min, max); \
static ssize_t name##_store(struct kobject *kobj, \
		struct kobj_attribute *attr, \
		const char *buf, size_t count) \
{ \
	char *acBuffer = NULL; \
	int tgid; \
	int arg; \
\
	acBuffer = kcalloc(FI_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL); \
	if (!acBuffer) \
		goto out; \
\
	if ((count > 0) && (count < FI_SYSFS_MAX_BUFF_SIZE)) { \
		if (scnprintf(acBuffer, FI_SYSFS_MAX_BUFF_SIZE, "%s", buf)) { \
			if (sscanf(acBuffer, "%d %d", &tgid, &arg) == 2) { \
				if (arg >= (min) && arg <= (max)) \
					game_set_frame_render_val(cmd, arg, tgid, 1); \
				else \
					game_set_frame_render_val(cmd, GAME_BY_PID_DEFAULT_VAL, \
						tgid, 0); \
			} \
		} \
	} \
\
out: \
	kfree(acBuffer); \
	return count; \
}

#define FI_POLICY_CMD_SYSFS_WRITE_PID_CMD(name, cmd, min, max); \
static ssize_t name##_store(struct kobject *kobj, \
		struct kobj_attribute *attr, \
		const char *buf, size_t count) \
{ \
	char *acBuffer = NULL; \
	int tgid; \
	int arg; \
\
	acBuffer = kcalloc(FI_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL); \
	if (!acBuffer) \
		goto out; \
\
	if ((count > 0) && (count < FI_SYSFS_MAX_BUFF_SIZE)) { \
		if (scnprintf(acBuffer, FI_SYSFS_MAX_BUFF_SIZE, "%s", buf)) { \
			if (sscanf(acBuffer, "%d %d", &tgid, &arg) == 2) { \
				if (arg >= (min) && arg <= (max)) \
					game_set_fi_policy_val(cmd, arg, tgid, 1); \
				else \
					game_set_fi_policy_val(cmd, GAME_BY_PID_DEFAULT_VAL, \
						tgid, 0); \
			} \
		} \
	} \
\
out: \
	kfree(acBuffer); \
	return count; \
}


FRAME_INTERPOLATION_SYSFS_READ(frame_interpolation_enable, 1, frame_interpolation_enable);
FRAME_INTERPOLATION_SYSFS_WRITE_VALUE(frame_interpolation_enable, frame_interpolation_enable, 0, 1);
static KOBJ_ATTR_RW(frame_interpolation_enable);

FRAME_INTERPOLATION_SYSFS_READ(fi_detect_enable, 1, fi_detect_enable);
static KOBJ_ATTR_RW(fi_detect_enable);

FRAME_INTERPOLATE_SYSFS_WRITE_PID_CMD(frame_interpolate_enable_by_pid, 0, -1, 3);
static KOBJ_ATTR_WO(frame_interpolate_enable_by_pid);

FI_POLICY_CMD_SYSFS_WRITE_PID_CMD(fi_set_target_fps_by_pid, 1, 0, 200);
static KOBJ_ATTR_WO(fi_set_target_fps_by_pid);

static ssize_t fi_info_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct game_render_info *iter;
	struct rb_root *root = NULL;
	struct rb_node *n = NULL;
	char *temp = NULL;
	int pos = 0;
	int length = 0;

	temp = kcalloc(FI_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	game_render_tree_lock();

	length = scnprintf(temp + pos, FI_SYSFS_MAX_BUFF_SIZE - pos,
			"total fi render tree num:%d\n", game_get_render_tree_total_num());
	pos += length;

	length = scnprintf(temp + pos, FI_SYSFS_MAX_BUFF_SIZE - pos,
	"tgid\tbufID\t\tenable\tratio\tf_count\tuser_target_fps\tfpsgo_target_fps\n");
	pos += length;

	root = game_get_render_pid_tree();
	n = rb_first(root);

	while (n) {
		iter = rb_entry(n, struct game_render_info, entry);
		length = scnprintf(temp + pos, FI_SYSFS_MAX_BUFF_SIZE - pos,
				"%d\t0x%llx\t\t%lu\t%d\t%d\t%d\t%d\n",
				iter->frame_info.tgid,
				iter->frame_info.buffer_id,
				iter->fi_enabled,
				iter->interpolation_ratio,
				iter->frame_count,
				iter->user_target_fps,
				iter->fpsgo_target_fps
				);
		pos += length;
		n = rb_next(n);
	}

	game_render_tree_unlock();

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}
static KOBJ_ATTR_RO(fi_info);

static ssize_t fi_policy_cmd_info_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct fi_policy_info *tmp_iter = NULL;
	struct hlist_head *fi_policy_list = NULL;
	struct hlist_node *h = NULL;
	char *temp = NULL;
	int pos = 0;
	int length = 0;

	temp = kcalloc(FI_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	game_fi_policy_list_lock();

	length = scnprintf(temp + pos, FI_SYSFS_MAX_BUFF_SIZE - pos,
			"total fi policy num:%d\n", game_get_fi_policy_list_total_num());
	pos += length;

	length = scnprintf(temp + pos, FI_SYSFS_MAX_BUFF_SIZE - pos,
	"tgid\tuser_target_fps\tts\n");
	pos += length;

	fi_policy_list = game_get_fi_policy_cmd_list();

	hlist_for_each_entry_safe(tmp_iter, h, fi_policy_list, hlist) {
		length = scnprintf(temp + pos, FI_SYSFS_MAX_BUFF_SIZE - pos,
				"%d\t%d\t%llu\n",
				tmp_iter->tgid,
				tmp_iter->user_target_fps,
				tmp_iter->ts
				);
		pos += length;
	}

	game_fi_policy_list_unlock();

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}
static KOBJ_ATTR_RO(fi_policy_cmd_info);

int init_frame_interpolation(void)
{
    is_registered_cb = 0;
	is_registered_detect_cb = 0;
	is_registered_all_fi_cb = 0;

	if (!game_get_sysfs_dir(&frame_interpolation_kobj)) {
        game_sysfs_create_file(frame_interpolation_kobj, &kobj_attr_frame_interpolation_enable);
		game_sysfs_create_file(frame_interpolation_kobj, &kobj_attr_frame_interpolate_enable_by_pid);
		game_sysfs_create_file(frame_interpolation_kobj, &kobj_attr_fi_set_target_fps_by_pid);
		game_sysfs_create_file(frame_interpolation_kobj, &kobj_attr_fi_info);
		game_sysfs_create_file(frame_interpolation_kobj, &kobj_attr_fi_policy_cmd_info);
		game_sysfs_create_file(frame_interpolation_kobj, &kobj_attr_fi_detect_enable);
    }

	fi_target_fps_wq = alloc_ordered_workqueue("%s", WQ_MEM_RECLAIM | WQ_HIGHPRI, "mt_game_fi");

	return 0;
}

int exit_frame_interpolation(void)
{
	game_register_queue_end_cb(0, &is_registered_cb, &fpsgo_fi_receive_q2q_cb);
	game_register_queue_end_cb(0, &is_registered_detect_cb, &fpsgo_fi_receive_fi_detect_cb);
	game_register_queue_end_cb(0, &is_registered_all_fi_cb,	&fpsgo_fi_receive_all_fi_cb);
	game_clear_render_info(0);

	game_sysfs_remove_file(frame_interpolation_kobj, &kobj_attr_frame_interpolation_enable);
	game_sysfs_remove_file(frame_interpolation_kobj, &kobj_attr_frame_interpolate_enable_by_pid);
	game_sysfs_remove_file(frame_interpolation_kobj, &kobj_attr_fi_set_target_fps_by_pid);
	game_sysfs_remove_file(frame_interpolation_kobj, &kobj_attr_fi_info);
	game_sysfs_remove_file(frame_interpolation_kobj, &kobj_attr_fi_policy_cmd_info);
	game_sysfs_remove_file(frame_interpolation_kobj, &kobj_attr_fi_detect_enable);

	return 0;
}

void frame_interpolate_exit(void)
{
	exit_frame_interpolation();
}

int frame_interpolate_init(void)
{
	init_fi_base();
	init_frame_interpolation();

	return 0;
}

