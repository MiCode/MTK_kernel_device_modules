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
#include "game_sysfs.h"
#include "game_trace_event.h"
#include "fi_base.h"
#include "frame_interpolate.h"

#define FRAME_INTERPOLATE_VERSION_MODULE "1.0"
#define GAME_BY_PID_DEFAULT_VAL -1
#define GAME_MAX_DEP_NUM 50
#define GAME_MAX_FRAME_COUNT INT_MAX
#define FRAME_INTERPOLATE_BUFFER_ID 1234

static int is_registered_cb;
static int frame_interpolation_enable;
static struct kobject *frame_interpolation_kobj;


extern int register_fpsgo_frame_info_callback(unsigned long mask, fpsgo_frame_info_callback cb);
extern int unregister_fpsgo_frame_info_callback(fpsgo_frame_info_callback cb);
extern int fpsgo_other2comp_user_create(int tgid, int render_tid, unsigned long long buffer_id,
	int *dep_arr, int dep_num, unsigned long long target_time);
extern int fpsgo_other2comp_report_workload(int tgid, int render_tid, unsigned long long buffer_id,
	unsigned long long tcpu, unsigned long long ts);
extern int switch_fpsgo_control(int mode, int pid, int set_ctrl, unsigned long long buffer_id);
extern int fpsgo_other2fstb_get_fps_info(int pid, unsigned long long bufID,
	struct render_fps_info *info);
extern int fpsgo_other2fstb_set_target(int mode, int pid, int use, int priority,
	int target_fps, unsigned long long target_time, unsigned long long bufID);
extern int fpsgo_other2xgf_get_critical_tasks(int pid, int max_num,
	struct task_info *arr, int filter_non_cfs, unsigned long long bufID);
extern int fpsgo_other2xgf_set_critical_tasks(int rpid, unsigned long long bufID,
	struct task_info *arr, int num, int use);
extern void fpsgo_other2xgf_calculate_dep(int pid, unsigned long long bufID,
	unsigned long long *raw_running_time, unsigned long long *ema_running_time,
	unsigned long long *enq_running_time,
	unsigned long long def_start_ts, unsigned long long def_end_ts,
	unsigned long long t_dequeue_start, unsigned long long t_dequeue_end,
	unsigned long long t_enqueue_start, unsigned long long t_enqueue_end,
	int skip);
extern int fpsgo_other2fstb_calculate_target_fps(int policy, int pid,
	unsigned long long bufID, unsigned long long cur_ts);


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

void fpsgo_fi_receive_q2q_cb(unsigned long cmd, struct render_frame_info *iter)
{
    int pid, add = 0;
	struct game_render_info *iter_thr = NULL;
	unsigned long long ts = 0;
	int target_fps = 0, set_target_fps = 0;

	if (!iter)
		goto out;

	ts = game_get_time();
	pid = iter->tgid;

	game_render_tree_lock();
	iter_thr = frame_interp_search_and_add_render_info(pid, add);
	if (iter_thr) {
		iter_thr->frame_info = *iter;
		iter_thr->frame_info.buffer_id = FRAME_INTERPOLATE_BUFFER_ID;
		iter_thr->frame_count++;
		iter_thr->frame_count = iter_thr->frame_count % GAME_MAX_FRAME_COUNT;
		iter_thr->updated_ts = ts;
		if (!iter_thr->is_fpsgo_render_created) {
			fpsgo_other2comp_user_create(iter_thr->frame_info.tgid, iter_thr->frame_info.pid,
				iter_thr->frame_info.buffer_id, NULL, 0, 0);
			iter_thr->is_fpsgo_render_created = 1;
		}

		switch_fpsgo_control(0, iter_thr->frame_info.tgid, 0, 0);

		target_fps = fpsgo_other2fstb_calculate_target_fps(2, iter_thr->frame_info.pid,
			iter_thr->frame_info.buffer_id, ts);
		set_target_fps = target_fps;
		do_div(set_target_fps, iter_thr->interpolation_ratio);

		fpsgo_other2fstb_set_target(1, iter_thr->frame_info.pid, 1, 0, set_target_fps, 0, iter_thr->frame_info.buffer_id);

		if (iter_thr->frame_count % iter_thr->interpolation_ratio == 0)
			frame_interpolate_fpsgo_render_control(iter_thr, ts);

		game_main_trace("[%s] tgid=%d, frame=%d, ts=%llu, target_fps=%d, set_fps=%d",
			__func__, iter_thr->frame_info.tgid, iter_thr->frame_count, ts, target_fps, set_target_fps);
	}
	game_render_tree_unlock();
out:
	return;
}
EXPORT_SYMBOL(fpsgo_fi_receive_q2q_cb);

// TODO: (Ann) Call unregister function.
static int game_register_queue_end_cb(int enable)
{
	int ret = 0;

	switch(enable) {
	case 0:
		if (is_registered_cb) {
			unregister_fpsgo_frame_info_callback(&fpsgo_fi_receive_q2q_cb);
			is_registered_cb = 0;
		}
		break;
	case 1:
		if (!is_registered_cb) {
			register_fpsgo_frame_info_callback(1 << GET_FPSGO_QUEUE_HINT, &fpsgo_fi_receive_q2q_cb);
			is_registered_cb = 1;
		}
		break;
	case -1:
		break;

	}

	return ret;
}

static int game_switch_frame_inteprolate_onoff(int pid, int enable)
{
	struct game_render_info *iter_thr = NULL;
	int delete = 0, num = 0;

	game_render_tree_lockprove(__func__);

	switch(enable) {
	case 0:
		iter_thr = frame_interp_search_and_add_render_info(pid, 0);
		if (iter_thr) {
			fpsgo_other2comp_user_close(iter_thr->frame_info.tgid, iter_thr->frame_info.pid,
				iter_thr->frame_info.buffer_id);
			switch_fpsgo_control(0, iter_thr->frame_info.tgid, 1, 0);
			delete = game_delete_render_info(iter_thr);

		}
		num = game_get_render_tree_total_num();
		if (!num)
			game_register_queue_end_cb(0);
		break;
	case 1:
		iter_thr = frame_interp_search_and_add_render_info(pid, 1);
		if (!iter_thr)
			return 1;
		iter_thr->frame_interpolation_enable = 1;
		game_register_queue_end_cb(1);
		break;
	}

	game_main_trace("[%s] tgid=%d, enable=%d, delete=%d, num=%d", __func__, pid, enable,
		delete, num);

	return 0;
}


static void game_set_frame_render_val(int cmd, int value, int pid, int add)
{
	game_render_tree_lockprove(__func__);

	switch(cmd) {
	case 0:  // frame_interpolate_enable_by_pid
		game_switch_frame_inteprolate_onoff(pid, value);
		break;
	default:
		break;
	}

	game_main_trace("[%s] cmd=%d, pid=%d", __func__, cmd, pid);
	return;
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

#define FRAME_INTERPOLATION_SYSFS_WRITE_VALUE(name, variable, min, max); \
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
				if (arg >= (min) && arg <= (max)) { \
					(variable) = arg; \
				} \
			} \
		} \
	} \
\
out: \
	kfree(acBuffer); \
	return count; \
}

#define FPSGO_COM_SYSFS_WRITE_PID_CMD(name, cmd, min, max); \
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
				game_render_tree_lock(); \
				if (arg >= (min) && arg <= (max)) \
					game_set_frame_render_val(cmd, arg, tgid, 1); \
				else \
					game_set_frame_render_val(cmd, GAME_BY_PID_DEFAULT_VAL, \
						tgid, 0); \
				game_render_tree_unlock(); \
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

FPSGO_COM_SYSFS_WRITE_PID_CMD(frame_interpolate_enable_by_pid, 0, -1, 3);
static KOBJ_ATTR_WO(frame_interpolate_enable_by_pid);

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
	"tgid\tbufID\t\tenable\tratio\tcount\n");
	pos += length;

	root = game_get_render_pid_tree();
	n = rb_first(root);

	while (n) {
		iter = rb_entry(n, struct game_render_info, entry);
		length = scnprintf(temp + pos, FI_SYSFS_MAX_BUFF_SIZE - pos,
				"%d\t0x%llx\t\t%d\t%d\t%d\n",
				iter->frame_info.tgid,
				iter->frame_info.buffer_id,
				iter->frame_interpolation_enable,
				iter->interpolation_ratio,
				iter->frame_count
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

int init_frame_interpolation(void)
{
    is_registered_cb = 0;

	if (!game_get_sysfs_dir(&frame_interpolation_kobj)) {
        game_sysfs_create_file(frame_interpolation_kobj, &kobj_attr_frame_interpolation_enable);
		game_sysfs_create_file(frame_interpolation_kobj, &kobj_attr_frame_interpolate_enable_by_pid);
		game_sysfs_create_file(frame_interpolation_kobj, &kobj_attr_fi_info);
    }

	return 0;
}

int exit_frame_interpolation(void)
{
	game_register_queue_end_cb(0);
	game_clear_render_info();
	game_sysfs_remove_file(frame_interpolation_kobj, &kobj_attr_frame_interpolation_enable);
	game_sysfs_remove_file(frame_interpolation_kobj, &kobj_attr_frame_interpolate_enable_by_pid);
	game_sysfs_remove_file(frame_interpolation_kobj, &kobj_attr_fi_info);

	return 0;
}

void frame_interpolate_exit(void)
{
	exit_frame_interpolation();
	game_sysfs_exit();
}

int frame_interpolate_init(void)
{
	init_fi_base();
	game_sysfs_init();
	init_frame_interpolation();
	return 0;
}

