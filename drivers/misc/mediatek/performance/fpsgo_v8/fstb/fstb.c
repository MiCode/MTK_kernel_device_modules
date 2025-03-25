// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/wait.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/err.h>
#include <linux/syscalls.h>
#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/string.h>
#include <linux/average.h>
#include <linux/topology.h>
#include <linux/vmalloc.h>
#include <linux/sched/clock.h>
#include <linux/sched.h>
#include <linux/kmemleak.h>
#include <asm/div64.h>
#include <mt-plat/fpsgo_common.h>
#include "fpsgo_frame_info.h"
#include "fpsgo_base.h"
#include "../fbt/include/fbt_cpu.h"
#include "../fbt/include/xgf.h"
#include "fpsgo_sysfs.h"
#include "fstb.h"
#include "fstb_usedext.h"
#include "fpsgo_usedext.h"
#include "fps_composer.h"

#if IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)
#include "ged_kpi.h"
#endif

#define FSTB_USEC_DIVIDER 1000000
#define mtk_fstb_dprintk_always(fmt, args...) \
	pr_debug("[FSTB]" fmt, ##args)

struct k_list {
	struct list_head queue_list;
	int fpsgo2pwr_pid;
	int fpsgo2pwr_fps;
};

static int dfps_ceiling = DEFAULT_DFPS;
static int QUANTILE = 50;
static int condition_get_fps;
static long long FRAME_TIME_WINDOW_SIZE_US = USEC_PER_SEC;
static int fstb_enable, fstb_active, fstb_active_dbncd, fstb_idle_cnt;
static long long last_update_ts;
static int total_fstb_policy_cmd_num;
static int fstb_max_dep_path_num = DEFAULT_MAX_DEP_PATH_NUM;
static int fstb_max_dep_task_num = DEFAULT_MAX_DEP_TASK_NUM;
static unsigned long long dtime_ceiling = DEFAULT_DTIME;

int fstb_no_r_timer_enable;
EXPORT_SYMBOL(fstb_no_r_timer_enable);
int fstb_filter_poll_enable;
EXPORT_SYMBOL(fstb_filter_poll_enable);
int fstb_fi_detect_enable;
EXPORT_SYMBOL(fstb_fi_detect_enable);

DECLARE_WAIT_QUEUE_HEAD(pwr_queue);

static void fstb_fps_stats(struct work_struct *work);
static DECLARE_WORK(fps_stats_work,
		(void *) fstb_fps_stats);

static LIST_HEAD(head);
static HLIST_HEAD(fstb_frame_info_list);
static HLIST_HEAD(fstb_user_target_hint_list);
static HLIST_HEAD(fstb_policy_cmd_list);

static DEFINE_MUTEX(fstb_lock);
static DEFINE_MUTEX(fstb_fps_active_time);
static DEFINE_MUTEX(fpsgo2pwr_lock);
static DEFINE_MUTEX(fstb_ko_lock);
static DEFINE_MUTEX(fstb_app_time_info_lock);
static DEFINE_MUTEX(fstb_user_target_hint_lock);
static DEFINE_MUTEX(fstb_policy_cmd_lock);

static struct kobject *fstb_kobj;
static struct hrtimer fstb_hrt;
static struct workqueue_struct *fstb_wq;
static struct rb_root fstb_app_time_info_tree;
struct fstb_powerfps_list powerfps_array[64];

int (*fstb_get_target_fps_fp)(int pid, unsigned long long bufID, int tgid,
	int dfps_ceiling, int max_dep_path_num, int max_dep_task_num,
	int *target_fps_margin, int *ctrl_fps_tid, int *ctrl_fps_flag,
	unsigned long long cur_queue_end_ts, int eara_is_active, int detect_mode);
EXPORT_SYMBOL(fstb_get_target_fps_fp);
void (*fstb_check_render_info_status_fp)(int clear, int *r_pid_arr,
	unsigned long long *r_bufid_arr, int r_num);
EXPORT_SYMBOL(fstb_check_render_info_status_fp);
int (*fpsgo2msync_hint_frameinfo_fp)(unsigned int render_tid, unsigned int reader_bufID,
		unsigned int target_fps, unsigned long q2q_time, unsigned long q2q_time2);
EXPORT_SYMBOL(fpsgo2msync_hint_frameinfo_fp);
int (*fstb_get_logic_head_trace_event_fp)(int pid, unsigned long long bufID, int tgid,
	unsigned long long cur_queue_end_ts, unsigned long long prev_queue_end_ts,
	unsigned long long pprev_queue_end_ts, unsigned long long dequeue_start_ts,
	unsigned long long *logical_head, int *has_logic_head);
EXPORT_SYMBOL(fstb_get_logic_head_trace_event_fp);
int (*fstb_get_is_interpolation_is_on_fp)(int pid, unsigned long long bufID, int tgid,
	unsigned long long cur_queue_end_ts, int *target_fps);
EXPORT_SYMBOL(fstb_get_is_interpolation_is_on_fp);

// AutoTest
int (*test_fstb_hrtimer_info_update_fp)(int *tmp_tid, unsigned long long *tmp_ts, int tmp_num,
	int *i_tid, unsigned long long *i_latest_ts, unsigned long long *i_diff,
	int *i_idx, int *i_idle, int i_num,
	int *o_tid, unsigned long long *o_latest_ts, unsigned long long *o_diff,
	int *o_idx, int *o_idle, int o_num, int o_ret);
EXPORT_SYMBOL(test_fstb_hrtimer_info_update_fp);

static void enable_fstb_timer(void)
{
	ktime_t ktime;

	ktime = ktime_set(0,
			FRAME_TIME_WINDOW_SIZE_US * 1000);
	hrtimer_start(&fstb_hrt, ktime, HRTIMER_MODE_REL);
}

static void disable_fstb_timer(void)
{
	hrtimer_cancel(&fstb_hrt);
}

static enum hrtimer_restart mt_fstb(struct hrtimer *timer)
{
	if (fstb_wq)
		queue_work(fstb_wq, &fps_stats_work);

	return HRTIMER_NORESTART;
}

int is_fstb_active(long long time_diff)
{
	int active = 0;
	ktime_t cur_time;
	long long cur_time_us;

	cur_time = ktime_get();
	cur_time_us = ktime_to_us(cur_time);

	mutex_lock(&fstb_fps_active_time);

	if (cur_time_us - last_update_ts < time_diff)
		active = 1;

	mutex_unlock(&fstb_fps_active_time);

	return active;
}

static void fstb_sentcmd(int pid, int fps)
{
	static struct k_list *node;

	mutex_lock(&fpsgo2pwr_lock);
	node = kmalloc(sizeof(*node), GFP_KERNEL);
	if (node == NULL)
		goto out;
	node->fpsgo2pwr_pid = pid;
	node->fpsgo2pwr_fps = fps;
	list_add_tail(&node->queue_list, &head);
	condition_get_fps = 1;
out:
	mutex_unlock(&fpsgo2pwr_lock);
	wake_up_interruptible(&pwr_queue);
}

void fpsgo_ctrl2fstb_get_fps(int *pid, int *fps)
{
	static struct k_list *node;

	wait_event_interruptible(pwr_queue, condition_get_fps);
	mutex_lock(&fpsgo2pwr_lock);
	if (!list_empty(&head)) {
		node = list_first_entry(&head, struct k_list, queue_list);
		*pid = node->fpsgo2pwr_pid;
		*fps = node->fpsgo2pwr_fps;
		list_del(&node->queue_list);
		kfree(node);
	}
	if (list_empty(&head))
		condition_get_fps = 0;
	mutex_unlock(&fpsgo2pwr_lock);
}

static int fstb_enter_check_render_info_status(int clear, int *r_pid_arr,
	unsigned long long *r_bufid_arr, int r_num)
{
	int ret = 1;

	mutex_lock(&fstb_ko_lock);
	if (fstb_check_render_info_status_fp)
		fstb_check_render_info_status_fp(clear, r_pid_arr, r_bufid_arr, r_num);
	else {
		ret = -ENOENT;
		mtk_fstb_dprintk_always("fstb_check_render_info_status_fp is NULL\n");
	}
	mutex_unlock(&fstb_ko_lock);

	return ret;
}

static int fstb_enter_get_target_fps(int pid, unsigned long long bufID, int tgid,
	int *target_fps_margin, unsigned long long cur_queue_end_ts,
	int eara_is_active, int detect_mode)
{
	int ret = 0;
	int ctrl_fps_tid = 0, ctrl_fps_flag = 0;

	mutex_lock(&fstb_ko_lock);

	if (fstb_get_target_fps_fp)
		ret = fstb_get_target_fps_fp(pid, bufID, tgid,
			dfps_ceiling, fstb_max_dep_path_num, fstb_max_dep_task_num,
			target_fps_margin, &ctrl_fps_tid, &ctrl_fps_flag,
			cur_queue_end_ts, eara_is_active, detect_mode);
	else {
		ret = -ENOENT;
		mtk_fstb_dprintk_always("fstb_get_target_fps_fp is NULL\n");
	}

	fpsgo_systrace_c_fstb(pid, bufID, ctrl_fps_tid, "ctrl_fps_pid");
	fpsgo_systrace_c_fstb(pid, bufID, ctrl_fps_flag, "ctrl_fps_flag");

	mutex_unlock(&fstb_ko_lock);

	return ret;
}

static struct fstb_app_time_info *fstb_get_app_time_info(int pid,
	unsigned long long bufID, int create)
{
	struct rb_node **p = &fstb_app_time_info_tree.rb_node;
	struct rb_node *parent = NULL;
	struct fstb_app_time_info *iter = NULL;

	while (*p) {
		parent = *p;
		iter = rb_entry(parent, struct fstb_app_time_info, rb_node);

		if (pid < iter->pid)
			p = &(*p)->rb_left;
		else if (pid > iter->pid)
			p = &(*p)->rb_right;
		else {
			if (bufID < iter->bufid)
				p = &(*p)->rb_left;
			else if (bufID > iter->bufid)
				p = &(*p)->rb_right;
			else
				return iter;
		}
	}

	if (!create)
		return NULL;

	iter = kzalloc(sizeof(struct fstb_app_time_info), GFP_KERNEL);
	if (!iter)
		return NULL;

	iter->pid = pid;
	iter->bufid = bufID;
	iter->app_self_ctrl_time_num = 0;
	iter->app_self_ctrl_time_update = 0;
	iter->ts = 0;
	memset(iter->app_self_ctrl_time, 0,
		FRAME_TIME_BUFFER_SIZE * sizeof(unsigned long long));

	rb_link_node(&iter->rb_node, parent, p);
	rb_insert_color(&iter->rb_node, &fstb_app_time_info_tree);

	return iter;
}

static void fstb_delete_app_time_info(struct fstb_app_time_info *iter)
{
	if (iter) {
		rb_erase(&iter->rb_node, &fstb_app_time_info_tree);
		kfree(iter);
	}
}

static void fstb_check_app_time_info_status(int clear, int *r_pid_arr,
	unsigned long long *r_bufid_arr, int r_num)
{
	int i;
	struct fstb_app_time_info *iter = NULL;
	struct rb_node *rbn = NULL;

	mutex_lock(&fstb_app_time_info_lock);
	rbn = rb_first(&fstb_app_time_info_tree);
	while (rbn) {
		iter = rb_entry(rbn, struct fstb_app_time_info, rb_node);
		if (clear) {
			fstb_delete_app_time_info(iter);
			rbn = rb_first(&fstb_app_time_info_tree);
		} else {
			for (i = 0; i < r_num; i++) {
				if (r_pid_arr[i] == iter->pid && r_bufid_arr[i] == iter->bufid)
					break;
			}
			if (i < r_num) {
				fstb_delete_app_time_info(iter);
				rbn = rb_first(&fstb_app_time_info_tree);
			} else
				rbn = rb_next(rbn);
		}
	}
	mutex_unlock(&fstb_app_time_info_lock);
}

void fstb_record_app_self_ctrl_time(int pid, unsigned long long bufID,
	unsigned long long *time_arr, int time_num, int max_num, int update,
	unsigned long long ts)
{
	int i;
	struct fstb_app_time_info *iter = NULL;

	mutex_lock(&fstb_app_time_info_lock);

	iter = fstb_get_app_time_info(pid, bufID, 1);
	if (!iter)
		goto out;

	iter->ts = ts;
	iter->app_self_ctrl_time_update = update;
	if (!update)
		goto out;

	iter->app_self_ctrl_time_num = time_num;
	for (i = 0; i < time_num; i++)
		iter->app_self_ctrl_time[i] = time_arr[i];

out:
	mutex_unlock(&fstb_app_time_info_lock);
}
EXPORT_SYMBOL(fstb_record_app_self_ctrl_time);

static struct fstb_user_target_hint *fstb_add_user_target_hint(int mode, int pid,
	int prio, int fps, unsigned long long time, unsigned long long bufID)
{
	struct fstb_user_target_hint *iter = NULL;

	if (pid <= 0 || prio < 0 || prio >= MAX_USER_TARGET_PRIO ||
		!(fps || time))
		return NULL;

	iter = kzalloc(sizeof(struct fstb_user_target_hint), GFP_KERNEL);
	if (!iter)
		return NULL;

	iter->mode = mode;
	iter->tgid = mode ? -1 : pid;
	iter->pid = mode ? pid : -1;
	iter->bufid = mode ? bufID : 0;
	iter->target_fps_hint[prio] = fps;
	iter->target_time_hint[prio] = time;

	hlist_add_head(&iter->hlist, &fstb_user_target_hint_list);

	return iter;
}

static void fstb_delete_user_target_hint(struct fstb_user_target_hint *iter)
{
	if (iter) {
		hlist_del(&iter->hlist);
		kfree(iter);
	}
}

static struct fstb_user_target_hint *fstb_get_user_target_hint(int mode,
	int pid, unsigned long long bufID)
{
	struct fstb_user_target_hint *iter = NULL;
	struct hlist_node *h = NULL;

	hlist_for_each_entry_safe(iter, h, &fstb_user_target_hint_list, hlist) {
		if (iter->mode == mode) {
			if (iter->tgid == pid || (iter->pid == pid && iter->bufid == bufID))
				break;
		}
	}

	return iter;
}

static int fstb_arbitrate_target(int raw_target_fps, int raw_target_time,
	struct fstb_frame_info *iter)
{
	int i;
	int final_tfpks = raw_target_fps * 1000;
	unsigned long long final_ttime = raw_target_time;
	struct fstb_user_target_hint *final_hint, *process_hint, *render_hint;

	mutex_lock(&fstb_user_target_hint_lock);

	process_hint = fstb_get_user_target_hint(0, iter->proc_id, 0);
	render_hint = fstb_get_user_target_hint(1, iter->pid, iter->bufid);
	if (render_hint)
		final_hint = render_hint;
	else if (process_hint)
		final_hint = process_hint;
	else {
		clear_bit(USER_TYPE, &iter->master_type);
		goto out;
	}

	for (i = MAX_USER_TARGET_PRIO - 1; i >= 0; i--) {
		if (final_hint->target_fps_hint[i] > 0 || final_hint->target_time_hint[i] > 0)
			break;
	}
	if (unlikely(i < 0)){
		clear_bit(USER_TYPE, &iter->master_type);
		goto out;
	}

	if (final_hint->target_fps_hint[i]) {
		final_tfpks = final_hint->target_fps_hint[i] <= dfps_ceiling ?
						final_hint->target_fps_hint[i] : dfps_ceiling;
		final_tfpks *= 1000;
		final_ttime = div64_u64(1000 * NSEC_PER_SEC, final_tfpks);
	} else if (final_hint->target_time_hint[i]) {
		final_ttime = final_hint->target_time_hint[i] >= dtime_ceiling ?
						final_hint->target_time_hint[i] : dtime_ceiling;
		final_tfpks = div64_u64(NSEC_PER_SEC, final_ttime) * 1000;
	}

	set_bit(USER_TYPE, &iter->master_type);

	fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid, i, "fstb_arb_prio");

out:
	mutex_unlock(&fstb_user_target_hint_lock);

	iter->raw_target_fpks = final_tfpks;
	iter->raw_target_time = final_ttime;
	if (iter->target_fps_diff) {
		final_tfpks += iter->target_fps_diff;
		if (final_tfpks > 0)
			final_ttime = div64_u64(1000 * NSEC_PER_SEC, final_tfpks);
	}
	iter->final_target_fpks = final_tfpks;
	iter->final_target_time = final_ttime;
	fpsgo_main_trace("[fstb][%d][0x%llx] | final target %d %llu %d %llu",
		iter->pid, iter->bufid,
		iter->raw_target_fpks, iter->raw_target_time,
		iter->final_target_fpks, iter->final_target_time);

	return 0;
}

static void switch_fstb_active(void)
{
	fpsgo_systrace_c_fstb(-200, 0,
			fstb_active, "fstb_active");
	fpsgo_systrace_c_fstb(-200, 0,
			fstb_active_dbncd, "fstb_active_dbncd");
	mtk_fstb_dprintk_always("%s %d %d\n",
			__func__, fstb_active, fstb_active_dbncd);
	enable_fstb_timer();
}

static int is_exceed_max_fstb_frame_info_num(void)
{
	int num = 0;
	struct fstb_frame_info *iter = NULL;
	struct hlist_node *h = NULL;

	hlist_for_each_entry_safe(iter, h, &fstb_frame_info_list, hlist) {
		num++;
	}

	return (num >= FPSGO_MAX_RENDER_INFO_SIZE);
}

static struct fstb_frame_info *fstb_get_frame_info(int pid,
	unsigned long long bufID, int create)
{
	unsigned long master_type = 0;
	struct fstb_frame_info *iter = NULL;
	struct hlist_node *h = NULL;
	struct task_struct *tsk = NULL, *gtsk = NULL;

	hlist_for_each_entry_safe(iter, h, &fstb_frame_info_list, hlist) {
		if (iter->pid == pid && iter->bufid == bufID)
			break;
	}

	if (iter || !create || is_exceed_max_fstb_frame_info_num())
		goto out;

	iter = vzalloc(sizeof(struct fstb_frame_info));
	if (!iter)
		goto out;

	iter->pid = pid;
	iter->bufid = bufID;
	iter->hwui_flag = RENDER_INFO_HWUI_NONE;
	iter->queue_fps = dfps_ceiling;
	iter->raw_target_fpks = dfps_ceiling * 1000;
	iter->raw_target_time = dtime_ceiling;
	iter->final_target_fpks = dfps_ceiling * 1000;
	iter->final_target_time = dtime_ceiling;
	set_bit(FPSGO_TYPE, &master_type);
	iter->master_type = master_type;

	rcu_read_lock();
	tsk = find_task_by_vpid(pid);
	if (tsk) {
		get_task_struct(tsk);
		gtsk = find_task_by_vpid(tsk->tgid);
		put_task_struct(tsk);
		if (gtsk)
			get_task_struct(gtsk);
	}
	rcu_read_unlock();

	if (gtsk) {
		strscpy(iter->proc_name, gtsk->comm, 16);
		iter->proc_name[15] = '\0';
		iter->proc_id = gtsk->pid;
		put_task_struct(gtsk);
	} else {
		iter->proc_name[0] = '\0';
		iter->proc_id = 0;
	}

	kmemleak_not_leak(iter);

	hlist_add_head(&iter->hlist, &fstb_frame_info_list);

out:
	return iter;
}

static void fstb_delete_frame_info(int pid, unsigned long long bufID,
	struct fstb_frame_info *iter)
{
	struct fstb_frame_info *tmp;

	if (iter) {
		hlist_del(&iter->hlist);
		vfree(iter);
	} else if (pid > 0 && bufID > 0) {
		tmp = fstb_get_frame_info(pid, bufID, 0);
		if (tmp) {
			hlist_del(&tmp->hlist);
			vfree(tmp);
		}
	}
}

static void fstb_delete_policy_cmd(int mode, struct fstb_policy_cmd *iter)
{
	unsigned long long min_ts = ULLONG_MAX;
	struct fstb_policy_cmd *tmp_iter = NULL, *min_iter = NULL;
	struct hlist_node *h = NULL;

	if (iter) {
		if (iter->target_fps_policy_enable == BY_PID_DEFAULT_VAL &&
			iter->target_fps_detect_enable == BY_PID_DEFAULT_VAL) {
			min_iter = iter;
			goto delete;
		} else
			return;
	}

	hlist_for_each_entry_safe(tmp_iter, h, &fstb_policy_cmd_list, hlist) {
		if (tmp_iter->mode == mode && tmp_iter->ts < min_ts) {
			min_ts = tmp_iter->ts;
			min_iter = tmp_iter;
		}
	}

	if (!min_iter)
		return;

delete:
	hlist_del(&min_iter->hlist);
	kfree(min_iter);
	total_fstb_policy_cmd_num--;
}

static struct fstb_policy_cmd *fstb_get_policy_cmd(int mode, int id,
	unsigned long long bufID, int force)
{
	struct fstb_policy_cmd *iter = NULL;
	struct hlist_node *h = NULL;

	hlist_for_each_entry_safe(iter, h, &fstb_policy_cmd_list, hlist) {
		if (iter->mode == mode) {
			if (iter->tgid == id || (iter->pid == id && iter->bufid == bufID)) {
				iter->ts = fpsgo_get_time();
				break;
			}
		}
	}

	if (iter || !force)
		goto out;

	iter = kzalloc(sizeof(struct fstb_policy_cmd), GFP_KERNEL);
	if (!iter)
		goto out;

	iter->mode = mode;
	iter->tgid = mode ? -1 : id;
	iter->pid = mode ? id : -1;
	iter->target_fps_policy_enable = BY_PID_DEFAULT_VAL;
	iter->target_fps_detect_enable = BY_PID_DEFAULT_VAL;
	iter->bufid = mode ? bufID : 0;
	iter->ts = fpsgo_get_time();

	hlist_add_head(&iter->hlist, &fstb_policy_cmd_list);
	total_fstb_policy_cmd_num++;

	if (total_fstb_policy_cmd_num > MAX_FSTB_POLICY_CMD_NUM)
		fstb_delete_policy_cmd(mode, NULL);

out:
	return iter;
}

static void fstb_set_policy_cmd(int cmd, int mode, int id, int value, int op,
	unsigned long long bufID)
{
	struct fstb_policy_cmd *iter;

	iter = fstb_get_policy_cmd(mode, id, bufID, op);
	if (iter) {
		if (cmd == 0)
			iter->target_fps_policy_enable = value;
		else if (cmd == 1)
			iter->target_fps_detect_enable = value;

		if (!op)
			fstb_delete_policy_cmd(mode, iter);
	}
}

static void fstb_update_policy_cmd(struct fstb_frame_info *iter)
{
	struct fstb_policy_cmd *policy;

	iter->target_fps_policy = 0;
	iter->target_fps_detect = 0;

	mutex_lock(&fstb_policy_cmd_lock);

	policy = fstb_get_policy_cmd(0, iter->proc_id, 0, 0);
	if (!policy)
		goto by_render;
	if (policy->target_fps_policy_enable != BY_PID_DEFAULT_VAL)
		iter->target_fps_policy = policy->target_fps_policy_enable;
	if (policy->target_fps_detect_enable != BY_PID_DEFAULT_VAL)
		iter->target_fps_detect = policy->target_fps_detect_enable;

by_render:
	policy = fstb_get_policy_cmd(1, iter->pid, iter->bufid, 0);
	if (!policy)
		goto out;
	if (policy->target_fps_policy_enable != BY_PID_DEFAULT_VAL)
		iter->target_fps_policy = policy->target_fps_policy_enable;
	if (policy->target_fps_detect_enable != BY_PID_DEFAULT_VAL)
		iter->target_fps_detect = policy->target_fps_detect_enable;

out:
	mutex_unlock(&fstb_policy_cmd_lock);
	return;
}

static int cmplonglong(const void *a, const void *b)
{
	return *(long long *)a - *(long long *)b;
}

void fpsgo_ctrl2fstb_dfrc_fps(int fps)
{
	if (fps > CFG_MAX_FPS_LIMIT || fps < CFG_MIN_FPS_LIMIT)
		return;

	mutex_lock(&fstb_lock);
	dfps_ceiling = fps;
	dtime_ceiling = div_u64(NSEC_PER_SEC, fps);
	mutex_unlock(&fstb_lock);
}

void gpu_time_update(long long t_gpu, unsigned int cur_freq,
		unsigned int cur_max_freq, u64 ulID)
{
	struct fstb_frame_info *iter;

	ktime_t cur_time;
	long long cur_time_us;

	mutex_lock(&fstb_lock);

	if (!fstb_enable) {
		mutex_unlock(&fstb_lock);
		return;
	}

	if (!fstb_active)
		fstb_active = 1;

	if (!fstb_active_dbncd) {
		fstb_active_dbncd = 1;
		switch_fstb_active();
	}

	hlist_for_each_entry(iter, &fstb_frame_info_list, hlist) {
		if (iter->bufid == ulID)
			break;
	}

	if (iter == NULL) {
		mutex_unlock(&fstb_lock);
		return;
	}

	iter->gpu_time = t_gpu;

	if (iter->weighted_gpu_time_begin < 0 ||
	iter->weighted_gpu_time_end < 0 ||
	iter->weighted_gpu_time_begin > iter->weighted_gpu_time_end ||
	iter->weighted_gpu_time_end >= FRAME_TIME_BUFFER_SIZE) {

		/* purge all data */
		iter->weighted_gpu_time_begin = iter->weighted_gpu_time_end = 0;
	}

	/*get current time*/
	cur_time = ktime_get();
	cur_time_us = ktime_to_us(cur_time);

	/*remove old entries*/
	while (iter->weighted_gpu_time_begin < iter->weighted_gpu_time_end) {
		if (iter->weighted_gpu_time_ts[iter->weighted_gpu_time_begin] <
				cur_time_us - FRAME_TIME_WINDOW_SIZE_US)
			iter->weighted_gpu_time_begin++;
		else
			break;
	}

	if (iter->weighted_gpu_time_begin == iter->weighted_gpu_time_end &&
	iter->weighted_gpu_time_end == FRAME_TIME_BUFFER_SIZE - 1)
		iter->weighted_gpu_time_begin = iter->weighted_gpu_time_end = 0;

	/*insert entries to weighted_gpu_time*/
	/*if buffer full --> move array align first*/
	if (iter->weighted_gpu_time_begin < iter->weighted_gpu_time_end &&
	iter->weighted_gpu_time_end == FRAME_TIME_BUFFER_SIZE - 1) {

		memmove(iter->weighted_gpu_time,
		&(iter->weighted_gpu_time[iter->weighted_gpu_time_begin]),
		sizeof(long long) *
		(iter->weighted_gpu_time_end - iter->weighted_gpu_time_begin));
		memmove(iter->weighted_gpu_time_ts,
		&(iter->weighted_gpu_time_ts[iter->weighted_gpu_time_begin]),
		sizeof(long long) *
		(iter->weighted_gpu_time_end - iter->weighted_gpu_time_begin));

		/*reset index*/
		iter->weighted_gpu_time_end =
		iter->weighted_gpu_time_end - iter->weighted_gpu_time_begin;
		iter->weighted_gpu_time_begin = 0;
	}

	if (cur_max_freq > 0 && cur_max_freq >= cur_freq
			&& t_gpu > 0LL && t_gpu < 1000000000LL) {
		iter->weighted_gpu_time[iter->weighted_gpu_time_end] =
			t_gpu * cur_freq;
		do_div(iter->weighted_gpu_time[iter->weighted_gpu_time_end],
				cur_max_freq);
		iter->weighted_gpu_time_ts[iter->weighted_gpu_time_end] =
			cur_time_us;
		fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid,
		(int)iter->weighted_gpu_time[iter->weighted_gpu_time_end],
		"weighted_gpu_time");
		iter->weighted_gpu_time_end++;
	}

	fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid, (int)t_gpu, "t_gpu");
	fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
			(int)cur_freq, "cur_gpu_cap");
	fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
			(int)cur_max_freq, "max_gpu_cap");

	mutex_unlock(&fstb_lock);
}

long long fpsgo_base2fstb_get_gpu_time(int pid, unsigned long long bufID)
{
	long long ret = 0;
	struct fstb_frame_info *iter = NULL;

	mutex_lock(&fstb_lock);
	iter = fstb_get_frame_info(pid, bufID, 0);
	if (iter)
		ret = iter->gpu_time;
	mutex_unlock(&fstb_lock);

	return ret;
}

void eara2fstb_get_tfps(int max_cnt, int *is_camera, int *pid, unsigned long long *buf_id,
				int *tfps, int *rfps, int *hwui, char name[][16], int *proc_id)
{
	int count = 0;
	struct fstb_frame_info *iter;
	struct hlist_node *n;

	mutex_lock(&fstb_lock);
	*is_camera = 0;

	hlist_for_each_entry_safe(iter, n, &fstb_frame_info_list, hlist) {
		if (count == max_cnt)
			break;

		if (iter->raw_target_fpks <= 0)
			continue;

		pid[count] = iter->pid;
		proc_id[count] = iter->proc_id;
		hwui[count] = iter->hwui_flag;
		buf_id[count] = iter->bufid;
		rfps[count] = iter->queue_fps;
		if (!iter->target_fps_notifying)
			tfps[count] = iter->raw_target_fpks / 1000;
		else
			tfps[count] = iter->target_fps_notifying;
		if (name)
			strscpy(name[count], iter->proc_name, 16);
		count++;
	}

	mutex_unlock(&fstb_lock);
}
EXPORT_SYMBOL(eara2fstb_get_tfps);

void eara2fstb_tfps_mdiff(int pid, unsigned long long buf_id, int diff,
					int tfps)
{
	int tmp_target_fps;
	struct fstb_frame_info *iter;

	mutex_lock(&fstb_lock);

	iter = fstb_get_frame_info(pid, buf_id, 0);
	if (iter) {
		tmp_target_fps = iter->raw_target_fpks / 1000;

		if (tfps != iter->target_fps_notifying
			&& tfps != tmp_target_fps)
			goto out;

		iter->target_fps_diff = diff;
		fpsgo_systrace_c_fstb_man(pid, buf_id, diff, "eara_diff");

		if (iter->target_fps_notifying
			&& tfps == iter->target_fps_notifying) {
			iter->target_fps_notifying = 0;
			fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
				iter->raw_target_fpks / 1000, "fstb_target_fps1");
			fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid,
				0, "fstb_notifying");
		}
	}

out:
	mutex_unlock(&fstb_lock);
}
EXPORT_SYMBOL(eara2fstb_tfps_mdiff);

int (*eara_pre_change_fp)(void);
EXPORT_SYMBOL(eara_pre_change_fp);
int (*eara_pre_change_single_fp)(int pid, unsigned long long bufID,
			int target_fps);
EXPORT_SYMBOL(eara_pre_change_single_fp);

static void fstb_change_tfps(struct fstb_frame_info *iter, int target_fps,
		int notify_eara)
{
	int ret = -1;

	if (notify_eara && eara_pre_change_single_fp)
		ret = eara_pre_change_single_fp(iter->pid, iter->bufid, target_fps);

	if ((notify_eara && (ret == -1))
		|| iter->target_fps_notifying == target_fps) {
		iter->target_fps_notifying = 0;
		fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid,
					0, "fstb_notifying");
		fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
					iter->raw_target_fpks / 1000, "fstb_target_fps1");
	} else {
		iter->target_fps_notifying = target_fps;
		fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid,
			iter->target_fps_notifying, "fstb_notifying");
	}
}

int fpsgo_fbt2fstb_update_cpu_frame_info(int pid, unsigned long long bufID,
	long long Runnging_time, unsigned int Curr_cap, unsigned int Max_cap)
{
	long long cpu_time_ns = (long long)Runnging_time;
	unsigned int max_current_cap = Curr_cap;
	unsigned int max_cpu_cap = Max_cap;
	unsigned long long wct = 0;

	struct fstb_frame_info *iter;

	ktime_t cur_time;
	long long cur_time_us;

	mutex_lock(&fstb_lock);

	if (!fstb_enable) {
		mutex_unlock(&fstb_lock);
		return 0;
	}

	if (!fstb_active)
		fstb_active = 1;

	if (!fstb_active_dbncd) {
		fstb_active_dbncd = 1;
		switch_fstb_active();
	}

	iter = fstb_get_frame_info(pid, bufID, 0);
	if (iter == NULL) {
		mutex_unlock(&fstb_lock);
		return 0;
	}

	if (iter->weighted_cpu_time_begin < 0 ||
		iter->weighted_cpu_time_end < 0 ||
		iter->weighted_cpu_time_begin > iter->weighted_cpu_time_end ||
		iter->weighted_cpu_time_end >= FRAME_TIME_BUFFER_SIZE) {

		/* purge all data */
		iter->weighted_cpu_time_begin = iter->weighted_cpu_time_end = 0;
	}

	/*get current time*/
	cur_time = ktime_get();
	cur_time_us = ktime_to_us(cur_time);

	/*remove old entries*/
	while (iter->weighted_cpu_time_begin < iter->weighted_cpu_time_end) {
		if (iter->weighted_cpu_time_ts[iter->weighted_cpu_time_begin] <
				cur_time_us - FRAME_TIME_WINDOW_SIZE_US)
			iter->weighted_cpu_time_begin++;
		else
			break;
	}

	if (iter->weighted_cpu_time_begin == iter->weighted_cpu_time_end &&
		iter->weighted_cpu_time_end == FRAME_TIME_BUFFER_SIZE - 1)
		iter->weighted_cpu_time_begin = iter->weighted_cpu_time_end = 0;

	/*insert entries to weighted_cpu_time*/
	/*if buffer full --> move array align first*/
	if (iter->weighted_cpu_time_begin < iter->weighted_cpu_time_end &&
		iter->weighted_cpu_time_end == FRAME_TIME_BUFFER_SIZE - 1) {

		memmove(iter->weighted_cpu_time,
		&(iter->weighted_cpu_time[iter->weighted_cpu_time_begin]),
		sizeof(long long) *
		(iter->weighted_cpu_time_end -
		 iter->weighted_cpu_time_begin));
		memmove(iter->weighted_cpu_time_ts,
		&(iter->weighted_cpu_time_ts[iter->weighted_cpu_time_begin]),
		sizeof(long long) *
		(iter->weighted_cpu_time_end -
		 iter->weighted_cpu_time_begin));

		/*reset index*/
		iter->weighted_cpu_time_end =
		iter->weighted_cpu_time_end - iter->weighted_cpu_time_begin;
		iter->weighted_cpu_time_begin = 0;
	}

	if (max_cpu_cap > 0 && Max_cap > Curr_cap
		&& cpu_time_ns > 0LL && cpu_time_ns < 1000000000LL) {
		wct = cpu_time_ns * max_current_cap;
		do_div(wct, max_cpu_cap);
	} else
		goto out;

	fpsgo_systrace_c_fstb_man(pid, iter->bufid, (int)wct,
		"weighted_cpu_time");

	iter->weighted_cpu_time[iter->weighted_cpu_time_end] =
		wct;

	iter->weighted_cpu_time_ts[iter->weighted_cpu_time_end] =
		cur_time_us;
	iter->weighted_cpu_time_end++;

out:
	iter->cpu_time = cpu_time_ns;

	fpsgo_systrace_c_fstb_man(pid, iter->bufid, (int)cpu_time_ns, "t_cpu");
	fpsgo_systrace_c_fstb(pid, iter->bufid, (int)max_current_cap,
			"cur_cpu_cap");
	fpsgo_systrace_c_fstb(pid, iter->bufid, (int)max_cpu_cap,
			"max_cpu_cap");

	mutex_unlock(&fstb_lock);
	return 0;
}

int fpsgo_comp2fstb_get_logic_head(int pid, unsigned long long bufID, int tgid,
	unsigned long long cur_queue_end, unsigned long long prev_queue_end_ts,
	unsigned long long pprev_queue_end_ts, unsigned long long dequeue_start_ts,
	unsigned long long *logic_head_ts, int *has_logic_head)
{
	int get_logic_ret = 0;

	if (!logic_head_ts || !has_logic_head) {
		get_logic_ret = -ENOMEM;
		goto out;
	}

	mutex_lock(&fstb_ko_lock);
	if (fstb_get_logic_head_trace_event_fp) {
		get_logic_ret = fstb_get_logic_head_trace_event_fp(pid, bufID, tgid, cur_queue_end,
			prev_queue_end_ts, pprev_queue_end_ts, dequeue_start_ts, logic_head_ts, has_logic_head);
	}
	mutex_unlock(&fstb_ko_lock);

	fpsgo_main_trace("[%s] ret=%d, logic_head_ts=%llu, q_ts=%llu", __func__, get_logic_ret,
		*logic_head_ts, cur_queue_end);

out:
	return get_logic_ret;
}

static int fstb_calculate_target_fps(int tgid, int pid, unsigned long long bufID,
	int target_fps_policy, int target_fps_margin, int hint, int eara_is_active,
	unsigned long long cur_queue_end_ts)
{
	int local_tfps = dfps_ceiling, margin = target_fps_margin;
	int target_fps_old = dfps_ceiling, target_fps_new = dfps_ceiling;
	unsigned long long local_ttime = dtime_ceiling;
	struct fstb_frame_info *iter = NULL;

	// use Target FPS V2
	if (target_fps_policy == 2) {
		local_tfps = fstb_enter_get_target_fps(pid, bufID, tgid,
			&margin, cur_queue_end_ts, eara_is_active, 0);
		// TODO(CHI): consider when need to clear struct fstb_app_time_info of render
		if (hint)
			fstb_record_app_self_ctrl_time(pid, bufID, NULL, 0, 0, 0, cur_queue_end_ts);
	} else if (target_fps_policy == 1)
		fstb_record_app_self_ctrl_time(pid, bufID, NULL, 0, 0, 0, cur_queue_end_ts);

	mutex_lock(&fstb_lock);
	iter = fstb_get_frame_info(pid, bufID, 0);
	if (!iter)
		goto out;

	if (target_fps_policy == 1) {
		// use Target FPS V1
		local_tfps = iter->queue_fps;
	} else if (target_fps_policy == 2) {
		// use Target FPS V2
		if (local_tfps <= 0)
			local_tfps = iter->raw_target_fpks / 1000;
		iter->target_fps_margin = hint ? 0 : margin;
	}
	if (local_tfps)
		local_ttime = div_u64(NSEC_PER_SEC, local_tfps);
	target_fps_old = iter->raw_target_fpks / 1000;
	fstb_arbitrate_target(local_tfps, local_ttime, iter);
	target_fps_new = iter->raw_target_fpks / 1000;
	if (target_fps_new != target_fps_old)
		fstb_change_tfps(iter, target_fps_new, 1);

	fpsgo_main_trace("[fstb][%d][0x%llx] | V%d target:%d %llu %d->%d",
		iter->pid, iter->bufid, target_fps_policy, local_tfps, local_ttime,
		target_fps_old, target_fps_new);
	fpsgo_main_trace("[fstb][%d][0x%llx] | dfrc:%d eara:%d margin:%d",
		iter->pid, iter->bufid,
		dfps_ceiling, eara_is_active, iter->target_fps_margin);

out:
	mutex_unlock(&fstb_lock);
	return local_tfps;
}

static void fstb_notifier_wq_cb(struct work_struct *psWork)
{
	int hint;
	struct fstb_notifier_push_tag *vpPush = NULL;

	vpPush = container_of(psWork, struct fstb_notifier_push_tag, sWork);

	if (!vpPush->only_detect) {
		mutex_lock(&fstb_user_target_hint_lock);
		hint = fstb_get_user_target_hint(0, vpPush->tgid, 0) ||
				fstb_get_user_target_hint(1, vpPush->pid, vpPush->bufid);
		mutex_unlock(&fstb_user_target_hint_lock);
		fstb_calculate_target_fps(vpPush->tgid, vpPush->pid, vpPush->bufid,
			vpPush->target_fps_policy, vpPush->target_fps_margin, hint,
			vpPush->eara_is_active, vpPush->cur_queue_end_ts);
	} else
		fstb_enter_get_target_fps(vpPush->pid, vpPush->bufid, vpPush->tgid,
			&vpPush->target_fps_margin, vpPush->cur_queue_end_ts, vpPush->eara_is_active,
			vpPush->only_detect);

	kfree(vpPush);
}

void fpsgo_comp2fstb_prepare_calculate_target_fps(int pid, unsigned long long bufID,
	unsigned long long cur_queue_end_ts)
{
	struct fstb_frame_info *iter = NULL;
	struct fstb_notifier_push_tag *vpPush = NULL;

	mutex_lock(&fstb_lock);

	iter = fstb_get_frame_info(pid, bufID, 0);
	if (!iter || !fstb_wq)
		goto out;

	if (!iter->target_fps_policy && !iter->target_fps_detect)
		goto out;

	vpPush = kzalloc(sizeof(struct fstb_notifier_push_tag), GFP_ATOMIC);
	if (!vpPush)
		goto out;

	vpPush->tgid = iter->proc_id;
	vpPush->pid = pid;
	vpPush->target_fps_policy = iter->target_fps_policy;
	vpPush->target_fps_margin = iter->target_fps_policy == 2 ? iter->target_fps_margin : 0;
	vpPush->eara_is_active = !!iter->target_fps_diff;
	vpPush->only_detect = iter->target_fps_detect;
	vpPush->bufid = bufID;
	vpPush->cur_queue_end_ts = cur_queue_end_ts;

	INIT_WORK(&vpPush->sWork, fstb_notifier_wq_cb);
	queue_work(fstb_wq, &vpPush->sWork);

out:
	mutex_unlock(&fstb_lock);
}

static long long get_cpu_frame_time(struct fstb_frame_info *iter)
{
	long long ret = INT_MAX;
	/*copy entries to temp array*/
	/*sort this array*/
	if (iter->weighted_cpu_time_end - iter->weighted_cpu_time_begin > 0 &&
		iter->weighted_cpu_time_end - iter->weighted_cpu_time_begin <
		FRAME_TIME_BUFFER_SIZE) {
		memcpy(iter->sorted_weighted_cpu_time,
		&(iter->weighted_cpu_time[iter->weighted_cpu_time_begin]),
		sizeof(long long) *
		(iter->weighted_cpu_time_end -
		 iter->weighted_cpu_time_begin));
		sort(iter->sorted_weighted_cpu_time,
		iter->weighted_cpu_time_end -
		iter->weighted_cpu_time_begin,
		sizeof(long long), cmplonglong, NULL);
	}

	/*update nth value*/
	if (iter->weighted_cpu_time_end - iter->weighted_cpu_time_begin) {
		if (
			iter->sorted_weighted_cpu_time[
				QUANTILE*
				(iter->weighted_cpu_time_end-
				 iter->weighted_cpu_time_begin)/100]
			> INT_MAX)
			ret = INT_MAX;
		else
			ret =
				iter->sorted_weighted_cpu_time[
					QUANTILE*
					(iter->weighted_cpu_time_end-
					 iter->weighted_cpu_time_begin)/100];
	} else
		ret = -1;

	fpsgo_systrace_c_fstb(iter->pid, iter->bufid, ret,
		"quantile_weighted_cpu_time");
	return ret;
}

static int get_gpu_frame_time(struct fstb_frame_info *iter)
{
	int ret = INT_MAX;
	/*copy entries to temp array*/
	/*sort this array*/
	if (iter->weighted_gpu_time_end - iter->weighted_gpu_time_begin
		> 0 &&
		iter->weighted_gpu_time_end - iter->weighted_gpu_time_begin
		< FRAME_TIME_BUFFER_SIZE) {
		memcpy(iter->sorted_weighted_gpu_time,
		&(iter->weighted_gpu_time[iter->weighted_gpu_time_begin]),
		sizeof(long long) *
		(iter->weighted_gpu_time_end - iter->weighted_gpu_time_begin));
		sort(iter->sorted_weighted_gpu_time,
		iter->weighted_gpu_time_end - iter->weighted_gpu_time_begin,
		sizeof(long long), cmplonglong, NULL);
	}

	/*update nth value*/
	if (iter->weighted_gpu_time_end - iter->weighted_gpu_time_begin) {
		if (
			iter->sorted_weighted_gpu_time[
				QUANTILE*
				(iter->weighted_gpu_time_end-
				 iter->weighted_gpu_time_begin)/100]
			> INT_MAX)
			ret = INT_MAX;
		else
			ret =
				iter->sorted_weighted_gpu_time[
					QUANTILE*
					(iter->weighted_gpu_time_end-
					 iter->weighted_gpu_time_begin)/100];
	} else
		ret = -1;

	fpsgo_systrace_c_fstb(iter->pid, iter->bufid, ret,
			"quantile_weighted_gpu_time");
	return ret;
}

void fpsgo_comp2fstb_queue_time_update(int pid, unsigned long long bufID,
	unsigned long long ts, int hwui_flag)
{
	struct fstb_frame_info *iter;

	mutex_lock(&fstb_lock);

	if (!fstb_enable) {
		mutex_unlock(&fstb_lock);
		return;
	}

	if (!fstb_active)
		fstb_active = 1;

	if (!fstb_active_dbncd) {
		fstb_active_dbncd = 1;
		switch_fstb_active();
	}

	iter = fstb_get_frame_info(pid, bufID, 1);
	if (!iter)
		goto out;

	iter->hwui_flag = hwui_flag;
	iter->latest_use_ts = ts;

	fstb_update_policy_cmd(iter);

	if (iter->queue_time_begin < 0 ||
			iter->queue_time_end < 0 ||
			iter->queue_time_begin > iter->queue_time_end ||
			iter->queue_time_end >= FRAME_TIME_BUFFER_SIZE) {
		/* purge all data */
		iter->queue_time_begin = iter->queue_time_end = 0;
	}

	/*remove old entries*/
	while (iter->queue_time_begin < iter->queue_time_end) {
		if (iter->queue_time_ts[iter->queue_time_begin] < ts -
				(long long)FRAME_TIME_WINDOW_SIZE_US * 1000)
			iter->queue_time_begin++;
		else
			break;
	}

	if (iter->queue_time_begin == iter->queue_time_end &&
			iter->queue_time_end == FRAME_TIME_BUFFER_SIZE - 1)
		iter->queue_time_begin = iter->queue_time_end = 0;

	/*insert entries to weighted_display_time*/
	/*if buffer full --> move array align first*/
	if (iter->queue_time_begin < iter->queue_time_end &&
			iter->queue_time_end == FRAME_TIME_BUFFER_SIZE - 1) {
		memmove(iter->queue_time_ts,
		&(iter->queue_time_ts[iter->queue_time_begin]),
		sizeof(unsigned long long) *
		(iter->queue_time_end - iter->queue_time_begin));
		/*reset index*/
		iter->queue_time_end =
			iter->queue_time_end - iter->queue_time_begin;
		iter->queue_time_begin = 0;
	}

	iter->queue_time_ts[iter->queue_time_end] = ts;
	iter->queue_time_end++;

out:
	mutex_unlock(&fstb_lock);

	mutex_lock(&fstb_fps_active_time);
	last_update_ts = ts;
	mutex_unlock(&fstb_fps_active_time);
}

void fpsgo_comp2fstb_notify_info(int pid, unsigned long long bufID,
	unsigned long long q2q_time, unsigned long long enq_length,
	unsigned long long deq_length)
{
	int local_final_tfps, local_fps_margin;
	struct fstb_frame_info *iter = NULL;

	mutex_lock(&fstb_lock);

	iter = fstb_get_frame_info(pid, bufID, 0);
	if (!iter) {
		mutex_unlock(&fstb_lock);
		return;
	}

	local_final_tfps = iter->final_target_fpks / 1000;
	local_fps_margin = iter->target_fps_policy == 2 ? iter->target_fps_margin : 0;
	ged_kpi_set_target_FPS_margin(iter->bufid, local_final_tfps,
		local_fps_margin, iter->target_fps_diff, iter->cpu_time);

	if (fpsgo2msync_hint_frameinfo_fp)
		fpsgo2msync_hint_frameinfo_fp(pid, bufID,
			local_final_tfps, q2q_time, q2q_time - enq_length - deq_length);

	mutex_unlock(&fstb_lock);
}

static int fstb_calculate_queue_fps(struct fstb_frame_info *iter,
		long long interval, int *is_fps_update)
{
	int i = iter->queue_time_begin, j;
	unsigned long long avg_frame_interval = 0;
	unsigned long long retval = CFG_MIN_FPS_LIMIT;
	int frame_count = 0;

	/* remove old entries */
	while (i < iter->queue_time_end) {
		if (iter->queue_time_ts[i] < sched_clock() - interval * 1000)
			i++;
		else
			break;
	}

	/* filter and asfc evaluation*/
	for (j = i + 1; j < iter->queue_time_end; j++) {
		avg_frame_interval +=
			(iter->queue_time_ts[j] -
			 iter->queue_time_ts[j - 1]);
		frame_count++;
	}

	*is_fps_update = frame_count ? 1 : 0;

	if (avg_frame_interval != 0) {
		retval = 1000000000ULL * frame_count;
		do_div(retval, avg_frame_interval);
	}
	fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid, (int)retval, "queue_fps");

	return retval;
}

static int fps_update(struct fstb_frame_info *iter)
{
	int is_fps_update = 0;

	iter->queue_fps =
		fstb_calculate_queue_fps(iter, FRAME_TIME_WINDOW_SIZE_US, &is_fps_update);

	return is_fps_update;
}

void fpsgo_fbt2fstb_query_fps(int pid, unsigned long long bufID,
		int *target_fps, int *target_fps_ori, int *target_cpu_time, int *fps_margin,
		int *quantile_cpu_time, int *quantile_gpu_time,
		int *target_fpks, int *cooler_on)
{
	int local_tfps, local_final_tfps, local_final_tfpks, tolerence_fps;
	unsigned long long total_time;
	struct fstb_frame_info *iter;

	mutex_lock(&fstb_lock);
	iter = fstb_get_frame_info(pid, bufID, 0);
	if (!iter) {
		(*quantile_cpu_time) = -1;
		(*quantile_gpu_time) = -1;
		local_tfps = dfps_ceiling;
		local_final_tfps = dfps_ceiling;
		local_final_tfpks = dfps_ceiling * 1000;
		tolerence_fps = 0;
		total_time = dtime_ceiling;
		*cooler_on = 0;
	} else {
		(*quantile_cpu_time) = iter->quantile_cpu_time;
		(*quantile_gpu_time) = iter->quantile_gpu_time;
		if (!iter->target_fps_policy)
			fstb_arbitrate_target(0, 0, iter);
		local_tfps = iter->raw_target_fpks / 1000;
		local_final_tfps = iter->final_target_fpks / 1000;
		local_final_tfpks = iter->final_target_fpks;
		tolerence_fps = iter->target_fps_policy == 2 ? iter->target_fps_margin : 0;
		total_time = iter->final_target_time;
		*cooler_on = !!iter->target_fps_diff;
	}
	mutex_unlock(&fstb_lock);

	if (target_fps)
		*target_fps = local_final_tfps;
	if (target_fpks)
		*target_fpks = local_final_tfpks;
	if (target_fps_ori)
		*target_fps_ori = local_tfps;
	if (fps_margin)
		*fps_margin = tolerence_fps;
	if (target_cpu_time)
		*target_cpu_time = total_time;
}

static int cmp_powerfps(const void *x1, const void *x2)
{
	const struct fstb_powerfps_list *r1 = x1;
	const struct fstb_powerfps_list *r2 = x2;

	if (r1->pid == 0)
		return 1;
	else if (r1->pid == -1)
		return 1;
	else if (r1->pid < r2->pid)
		return -1;
	else if (r1->pid == r2->pid && r1->fps < r2->fps)
		return -1;
	else if (r1->pid == r2->pid && r1->fps == r2->fps)
		return 0;
	return 1;
}

void fstb_cal_powerhal_fps(void)
{
	struct fstb_frame_info *iter;
	int i = 0, j = 0, hint = 0;

	memset(powerfps_array, 0, 64 * sizeof(struct fstb_powerfps_list));
	mutex_lock(&fstb_user_target_hint_lock);
	hlist_for_each_entry(iter, &fstb_frame_info_list, hlist) {
		if (i < 0 || i >= 64)
			break;

		powerfps_array[i].pid = iter->proc_id;
		hint = fstb_get_user_target_hint(0, iter->proc_id, 0) ||
			fstb_get_user_target_hint(1, iter->pid, iter->bufid);
		if (iter->target_fps_policy == 2 || hint)
			powerfps_array[i].fps = iter->raw_target_fpks / 1000;
		else
			powerfps_array[i].fps = -1;

		i++;
		if (i >= 64) {
			i = 63;
			break;
		}
	}
	mutex_unlock(&fstb_user_target_hint_lock);

	if (i < 0 || i >= 64)
		return;

	powerfps_array[i].pid = -1;

	sort(powerfps_array, i, sizeof(struct fstb_powerfps_list), cmp_powerfps, NULL);

	for (j = 0; j < i; j++) {
		if (powerfps_array[j].pid != powerfps_array[j + 1].pid)
			fstb_sentcmd(powerfps_array[j].pid, powerfps_array[j].fps);
	}
}

static void fstb_fps_stats(struct work_struct *work)
{
	struct fstb_frame_info *iter;
	struct hlist_node *n;
	int idle = 1;
	int eara_ret = -1;

	if (work != &fps_stats_work)
		kfree(work);

	mutex_lock(&fstb_lock);

	hlist_for_each_entry_safe(iter, n, &fstb_frame_info_list, hlist) {
		if (fps_update(iter)) {

			idle = 0;

			iter->quantile_cpu_time = (int)get_cpu_frame_time(iter);
			iter->quantile_gpu_time = (int)get_gpu_frame_time(iter);

			fpsgo_systrace_c_fstb_man(iter->pid, 0,
					dfps_ceiling, "dfrc");
		}
	}

	fstb_cal_powerhal_fps();

	/* check idle twice to avoid fstb_active ping-pong */
	if (idle)
		fstb_idle_cnt++;
	else
		fstb_idle_cnt = 0;

	if (fstb_idle_cnt >= FSTB_IDLE_DBNC) {
		fstb_active_dbncd = 0;
		fstb_idle_cnt = 0;
	} else if (fstb_idle_cnt >= 2) {
		fstb_active = 0;
	}

	if (fstb_enable && fstb_active_dbncd)
		enable_fstb_timer();
	else
		disable_fstb_timer();

	mutex_unlock(&fstb_lock);

	if (eara_pre_change_fp)
		eara_ret = eara_pre_change_fp();

	if (eara_ret == -1) {
		mutex_lock(&fstb_lock);
		hlist_for_each_entry_safe(iter, n, &fstb_frame_info_list, hlist) {
			if (!iter->target_fps_notifying
				|| iter->target_fps_notifying == -1)
				continue;
			iter->target_fps_notifying = 0;
			fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid,
					0, "fstb_notifying");
			fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
					iter->raw_target_fpks / 1000, "fstb_target_fps1");
		}
		mutex_unlock(&fstb_lock);
	}
}

void fpsgo_comp2fstb_delete_render_info(int pid, unsigned long long bufID)
{
	mutex_lock(&fstb_lock);
	fstb_delete_frame_info(pid, bufID, NULL);
	mutex_unlock(&fstb_lock);
}

int fpsgo_comp2fstb_do_recycle(void)
{
	int ret = 0;
	int *r_pid_arr = NULL;
	int r_num = 0;
	unsigned long long *r_bufid_arr = NULL;
	unsigned long long cur_ts = fpsgo_get_time();
	struct fstb_frame_info *iter;
	struct hlist_node *h;

	r_pid_arr = kcalloc(FPSGO_MAX_RENDER_INFO_SIZE, sizeof(int), GFP_KERNEL);
	r_bufid_arr = kcalloc(FPSGO_MAX_RENDER_INFO_SIZE,
		sizeof(unsigned long long), GFP_KERNEL);
	if (!r_pid_arr || !r_bufid_arr)
		goto out;

	mutex_lock(&fstb_lock);

	if (hlist_empty(&fstb_frame_info_list)) {
		ret = 1;
		goto out;
	}

	hlist_for_each_entry_safe(iter, h, &fstb_frame_info_list, hlist) {
		if (cur_ts - iter->latest_use_ts > 2 * NSEC_PER_SEC) {
			if (r_num < FPSGO_MAX_RENDER_INFO_SIZE) {
				r_pid_arr[r_num] = iter->pid;
				r_bufid_arr[r_num] = iter->bufid;
				r_num++;
			}
			fstb_delete_frame_info(iter->pid, iter->bufid, iter);
		}
	}

out:
	mutex_unlock(&fstb_lock);

	if (r_num > 0) {
		fstb_enter_check_render_info_status(0, r_pid_arr, r_bufid_arr, r_num);
		fstb_check_app_time_info_status(0, r_pid_arr, r_bufid_arr, r_num);
	}

	kfree(r_bufid_arr);
	kfree(r_pid_arr);

	return ret;
}

int fpsgo_ctrl2fstb_switch_fstb(int enable)
{
	struct fstb_frame_info *iter;
	struct hlist_node *t;

	mutex_lock(&fstb_lock);
	if (fstb_enable == enable) {
		mutex_unlock(&fstb_lock);
		return 0;
	}

	fstb_enable = enable;
	if (!fstb_enable) {
		hlist_for_each_entry_safe(iter, t, &fstb_frame_info_list, hlist) {
			fstb_delete_frame_info(iter->pid, iter->bufid, iter);
		}
	}
	fpsgo_systrace_c_fstb(-200, 0, fstb_enable, "fstb_enable");
	mtk_fstb_dprintk_always("%s %d\n", __func__, fstb_enable);
	mutex_unlock(&fstb_lock);

	if (!fstb_enable) {
		fstb_enter_check_render_info_status(1, NULL, NULL, 0);
		fstb_check_app_time_info_status(1, NULL, NULL, 0);
	}

	return 0;
}

/*
 * General API to calculate target fps
 * @policy: use Target FPS V1 or V2
 * @pid: one key of render_info data structure
 * @bufID: one key of render_info data structure
 * @cur_ts: end timestamp of window to calculate, start timestamp of window use previous cur_ts
 */
int fpsgo_other2fstb_calculate_target_fps(int policy, int pid,
	unsigned long long bufID, unsigned long long cur_ts)
{
	int tfps = -1, hint = 0;
	struct fstb_frame_info *iter = NULL;
	struct fstb_notifier_push_tag *vpPush = NULL;

	if (policy < 1 || policy > 2 || pid <= 0 || !bufID)
		return -EINVAL;

	vpPush = kzalloc(sizeof(struct fstb_notifier_push_tag), GFP_KERNEL);
	if (!vpPush)
		goto out;

	mutex_lock(&fstb_lock);
	iter = fstb_get_frame_info(pid, bufID, 0);
	if (!iter) {
		mutex_unlock(&fstb_lock);
		goto out;
	}
	vpPush->tgid = iter->proc_id;
	vpPush->target_fps_margin = iter->target_fps_margin;
	vpPush->eara_is_active = !!iter->target_fps_diff;
	mutex_unlock(&fstb_lock);

	mutex_lock(&fstb_user_target_hint_lock);
	hint = fstb_get_user_target_hint(0, vpPush->tgid, 0) || fstb_get_user_target_hint(1, pid, bufID);
	mutex_unlock(&fstb_user_target_hint_lock);

	tfps = fstb_calculate_target_fps(vpPush->tgid, pid, bufID,
		policy, vpPush->target_fps_margin,
		hint, vpPush->eara_is_active, cur_ts);

out:
	kfree(vpPush);
	return tfps;
}
EXPORT_SYMBOL(fpsgo_other2fstb_calculate_target_fps);

/*
 * General API for notify FPSGO FSTB target fps
 * @mode: 0 by process control, 1 by render control
 * @pid: tgid with mode is 0, render pid with mode is 1
 * @use: bool value for set or unset
 * @priority: how important in arbitration
 * @target_fps: expected fps of user
 * @target_time: expected time of user
 * @bufID: if mode is 1, one key of render_info data structure
 */
int fpsgo_other2fstb_set_target(int mode, int pid, int use, int priority,
	int target_fps, unsigned long long target_time, unsigned long long bufID)
{
	int i;
	struct fstb_user_target_hint *iter = NULL;

	if (pid <= 0 || priority < 0 || priority >= MAX_USER_TARGET_PRIO)
		return -EINVAL;

	mutex_lock(&fstb_user_target_hint_lock);

	// add user setting to data structure
	iter = fstb_get_user_target_hint(mode, pid, bufID);
	if (use && !iter)
		iter = fstb_add_user_target_hint(mode, pid, priority,
			target_fps, target_time, bufID);
	if (!iter) {
		mutex_unlock(&fstb_user_target_hint_lock);
		return -ENOMEM;
	}

	iter->target_fps_hint[priority] = target_fps;
	iter->target_time_hint[priority] = target_time;

	// check whether no user use data structure
	for (i = 0; i < MAX_USER_TARGET_PRIO; i++) {
		if (iter->target_fps_hint[i] > 0 || iter->target_time_hint[i] > 0)
			break;
	}
	if (i >= MAX_USER_TARGET_PRIO)
		fstb_delete_user_target_hint(iter);

	mutex_unlock(&fstb_user_target_hint_lock);

	return 0;
}
EXPORT_SYMBOL(fpsgo_other2fstb_set_target);

/*
 * General API to get FPSGO FSTB estimated average of app self ctrl time
 * @pid: one key of render_info data strcutre
 * @bufID: one key of render_info data structure
 */
unsigned long long fpsgo_other2fstb_get_app_self_ctrl_time(int pid, unsigned long long bufID)
{
	int i;
	unsigned long long avg = 0;
	struct fstb_app_time_info *iter = NULL;

	mutex_lock(&fstb_app_time_info_lock);

	iter = fstb_get_app_time_info(pid, bufID, 0);

	if (!iter || !iter->app_self_ctrl_time_update ||
		iter->app_self_ctrl_time_num <= 0)
		goto out;

	for (i = 0; i < iter->app_self_ctrl_time_num; i++)
		avg += iter->app_self_ctrl_time[i];
	avg = div_u64(avg, iter->app_self_ctrl_time_num);

out:
	mutex_unlock(&fstb_app_time_info_lock);
	return avg;
}
EXPORT_SYMBOL(fpsgo_other2fstb_get_app_self_ctrl_time);

/*
 * General API to get FPSGO FSTB fps information
 * @pid: one key of render_info data strcutre
 * @bufID: one key of render_info data structure
 * @info: return fps information
 */
int fpsgo_other2fstb_get_fps_info(int pid, unsigned long long bufID,
	struct render_fps_info *info)
{
	struct fstb_frame_info *iter;

	if (pid <= 0 || !bufID || !info)
		return -EINVAL;

	mutex_lock(&fstb_lock);
	iter = fstb_get_frame_info(pid, bufID, 0);
	if (iter) {
		info->queue_fps = iter->queue_fps;
		info->raw_target_fps = iter->raw_target_fpks / 1000;
		info->target_fps_diff = iter->target_fps_diff;
	}
	mutex_unlock(&fstb_lock);

	return 0;
}
EXPORT_SYMBOL(fpsgo_other2fstb_get_fps_info);

int fpsgo_ktf2fstb_add_delete_render_info(int mode, int pid, unsigned long long bufID,
	int target_fps, int queue_fps)
{
	int ret = 0;
	struct fstb_frame_info *iter = NULL;

	mutex_lock(&fstb_lock);

	if (mode) {
		iter = fstb_get_frame_info(pid, bufID, 1);
		if (!iter) {
			ret = -ENOMEM;
			goto out;
		}
		iter->raw_target_fpks = target_fps * 1000;
		iter->final_target_fpks = iter->raw_target_fpks;
		iter->raw_target_time = target_fps > 0 ? div_u64(NSEC_PER_SEC, target_fps) : dtime_ceiling;
		iter->final_target_time = iter->raw_target_time;
		iter->queue_fps = queue_fps;
		iter->master_type = 1 << KTF_TYPE;
	} else
		fstb_delete_frame_info(pid, bufID, NULL);

#if IS_ENABLED(CONFIG_ARM64)
	mtk_fstb_dprintk_always("[ktf] size:%lu mode:%d pid:%d bufID:0x%llx fps:%d %d\n",
			sizeof(struct fstb_frame_info), mode, pid, bufID, target_fps,queue_fps);
#endif

out:
	mutex_unlock(&fstb_lock);
	return ret;
}

int fpsgo_ktf2fstb_test_fstb_hrtimer_info_update(int *tmp_tid,
	unsigned long long *tmp_ts, int tmp_num,
	int *i_tid, unsigned long long *i_latest_ts, unsigned long long *i_diff,
	int *i_idx, int *i_idle, int i_num,
	int *o_tid, unsigned long long *o_latest_ts, unsigned long long *o_diff,
	int *o_idx, int *o_idle, int o_num, int o_ret)
{
	int ret = 0;

	WARN_ON(!test_fstb_hrtimer_info_update_fp);

	mutex_lock(&fstb_ko_lock);
	if (test_fstb_hrtimer_info_update_fp)
		ret = test_fstb_hrtimer_info_update_fp(tmp_tid, tmp_ts, tmp_num,
				i_tid, i_latest_ts, i_diff, i_idx, i_idle, i_num,
				o_tid, o_latest_ts, o_diff, o_idx, o_idle, o_num,
				o_ret);
	mutex_unlock(&fstb_ko_lock);

	return ret;
}
EXPORT_SYMBOL(fpsgo_ktf2fstb_test_fstb_hrtimer_info_update);

#define FSTB_SYSFS_READ(name, show, variable); \
static ssize_t name##_show(struct kobject *kobj, \
		struct kobj_attribute *attr, \
		char *buf) \
{ \
	if ((show)) \
		return scnprintf(buf, PAGE_SIZE, "%d\n", (variable)); \
	else \
		return 0; \
}

#define FSTB_SYSFS_WRITE_VALUE(name, variable, min, max); \
static ssize_t name##_store(struct kobject *kobj, \
		struct kobj_attribute *attr, \
		const char *buf, size_t count) \
{ \
	char *acBuffer = NULL; \
	int arg; \
\
	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL); \
	if (!acBuffer) \
		goto out; \
\
	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) { \
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) { \
			if (kstrtoint(acBuffer, 0, &arg) == 0) { \
				if (arg >= (min) && arg <= (max)) { \
					mutex_lock(&fstb_lock); \
					(variable) = arg; \
					mutex_unlock(&fstb_lock); \
				} \
			} \
		} \
	} \
\
out: \
	kfree(acBuffer); \
	return count; \
}

#define FSTB_SYSFS_WRITE_POLICY_CMD_BY_PROCESS(name, cmd, min, max); \
static ssize_t name##_store(struct kobject *kobj, \
		struct kobj_attribute *attr, \
		const char *buf, size_t count) \
{ \
	char *acBuffer = NULL; \
	int tgid; \
	int arg; \
\
	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL); \
	if (!acBuffer) \
		goto out; \
\
	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) { \
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) { \
			if (sscanf(acBuffer, "%d %d", &tgid, &arg) == 2) { \
				mutex_lock(&fstb_policy_cmd_lock); \
				if (arg >= (min) && arg <= (max)) \
					fstb_set_policy_cmd(cmd, 0, tgid, arg, 1, 0); \
				else \
					fstb_set_policy_cmd(cmd, 0, tgid, BY_PID_DEFAULT_VAL, 0, 0); \
				mutex_unlock(&fstb_policy_cmd_lock); \
			} \
		} \
	} \
\
out: \
	kfree(acBuffer); \
	return count; \
}

#define FSTB_SYSFS_WRITE_POLICY_CMD_BY_RENDER(name, cmd, min, max); \
static ssize_t name##_store(struct kobject *kobj, \
		struct kobj_attribute *attr, \
		const char *buf, size_t count) \
{ \
	char *acBuffer = NULL; \
	int pid; \
	int arg; \
	unsigned long long bufID; \
\
	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL); \
	if (!acBuffer) \
		goto out; \
\
	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) { \
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) { \
			if (sscanf(acBuffer, "%d %llu %d", &pid, &bufID, &arg) == 3) { \
				mutex_lock(&fstb_policy_cmd_lock); \
				if (arg >= (min) && arg <= (max)) \
					fstb_set_policy_cmd(cmd, 1, pid, arg, 1, bufID); \
				else \
					fstb_set_policy_cmd(cmd, 1, pid, BY_PID_DEFAULT_VAL, 0, bufID); \
				mutex_unlock(&fstb_policy_cmd_lock); \
			} \
		} \
	} \
\
out: \
	kfree(acBuffer); \
	return count; \
}

FSTB_SYSFS_READ(fstb_tune_quantile, 1, QUANTILE);
FSTB_SYSFS_WRITE_VALUE(fstb_tune_quantile, QUANTILE, 0, 100);
static KOBJ_ATTR_RW(fstb_tune_quantile);

FSTB_SYSFS_READ(fstb_no_r_timer_enable, 1, fstb_no_r_timer_enable);
FSTB_SYSFS_WRITE_VALUE(fstb_no_r_timer_enable, fstb_no_r_timer_enable, 0, 1);
static KOBJ_ATTR_RW(fstb_no_r_timer_enable);

FSTB_SYSFS_READ(fstb_filter_poll_enable, 1, fstb_filter_poll_enable);
FSTB_SYSFS_WRITE_VALUE(fstb_filter_poll_enable, fstb_filter_poll_enable, 0, 1);
static KOBJ_ATTR_RW(fstb_filter_poll_enable);

static ssize_t fstb_debug_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char *temp = NULL;
	int pos = 0;
	int length = 0;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"fstb_enable %d\n", fstb_enable);
	pos += length;
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"fstb_active %d\n", fstb_active);
	pos += length;
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"fstb_active_dbncd %d\n", fstb_active_dbncd);
	pos += length;
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"fstb_idle_cnt %d\n", fstb_idle_cnt);
	pos += length;

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static ssize_t fstb_debug_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char *acBuffer = NULL;
	int k_enable, klog_on;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (sscanf(acBuffer, "%d %d",
				&k_enable, &klog_on) >= 1) {
				if (k_enable == 0 || k_enable == 1)
					fpsgo_ctrl2fstb_switch_fstb(k_enable);
			}
		}
	}

out:
	kfree(acBuffer);
	return count;
}
static KOBJ_ATTR_RW(fstb_debug);

static ssize_t fpsgo_status_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct fstb_frame_info *iter;
	char *temp = NULL;
	int pos = 0;
	int length = 0;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	mutex_lock(&fstb_lock);

	if (!fstb_enable) {
		mutex_unlock(&fstb_lock);
		goto out;
	}

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
	"tid\tbufID\t\tname\t\tcurrentFPS\ttargetFPS\tFPS_margin\tHWUI\tt_gpu\t\tpolicy\n");
	pos += length;

	hlist_for_each_entry(iter, &fstb_frame_info_list, hlist) {
		if (iter) {
			length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
					"%d\t0x%llx\t%s\t%d\t\t%d\t\t%d\t\t%d\t\t%lld\t\t(%d,%lu)\n",
					iter->pid,
					iter->bufid,
					iter->proc_name,
					iter->queue_fps > dfps_ceiling ?
					dfps_ceiling : iter->queue_fps,
					iter->raw_target_fpks / 1000,
					iter->target_fps_policy ?
					iter->target_fps_margin : 0,
					iter->hwui_flag,
					iter->gpu_time,
					iter->target_fps_policy,
					iter->master_type);
			pos += length;
		}
	}

	mutex_unlock(&fstb_lock);

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"dfps_ceiling:%d\n", dfps_ceiling);
	pos += length;

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}
static KOBJ_ATTR_ROO(fpsgo_status);

static ssize_t fstb_tfps_info_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char *temp = NULL;
	int i = 1;
	int pos = 0;
	int length = 0;
	struct fstb_frame_info *iter;
	struct fstb_app_time_info *t_iter = NULL;
	struct fstb_user_target_hint *hint_iter = NULL;
	struct rb_node *rbn = NULL;
	struct hlist_node *h = NULL;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	mutex_lock(&fstb_lock);

	hlist_for_each_entry_safe(iter, h, &fstb_frame_info_list, hlist) {
		length = scnprintf(temp + pos,
			FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"%dth\t[%d][0x%llx]\ttype:%lu\ttarget_fps:%d %d\ttarget_time:%llu %llu\n",
			i,
			iter->pid,
			iter->bufid,
			iter->master_type,
			iter->raw_target_fpks, iter->final_target_fpks,
			iter->raw_target_time, iter->final_target_time);
		pos += length;
		i++;
	}

	mutex_unlock(&fstb_lock);

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
		"\n");
	pos += length;

	mutex_lock(&fstb_app_time_info_lock);
	for (rbn = rb_first(&fstb_app_time_info_tree); rbn; rbn = rb_next(rbn)) {
		t_iter = rb_entry(rbn, struct fstb_app_time_info, rb_node);
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"[%d][0x%llx] app_self_ctrl_time_num:%d app_self_ctrl_time_update:%d ts:%llu\n",
			t_iter->pid, t_iter->bufid,
			t_iter->app_self_ctrl_time_num, t_iter->app_self_ctrl_time_update, t_iter->ts);
		pos += length;
		for (i = 0; i < t_iter->app_self_ctrl_time_num; i++) {
			length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				"\t%llu\n", t_iter->app_self_ctrl_time[i]);
			pos += length;
		}
	}
	mutex_unlock(&fstb_app_time_info_lock);

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
		"\n");
	pos += length;

	mutex_lock(&fstb_user_target_hint_lock);
	hlist_for_each_entry_safe(hint_iter, h, &fstb_user_target_hint_list, hlist) {
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"mode:%d tgid:%d pid:%d bufID:0x%llx\n\ttarget_fps_hint:[ ",
			hint_iter->mode, hint_iter->tgid, hint_iter->pid, hint_iter->bufid);
		pos += length;
		for (i = 0; i < MAX_USER_TARGET_PRIO; i++) {
			length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				"%d ", hint_iter->target_fps_hint[i]);
			pos += length;
		}
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"]\n\ttarget_time_hint:[ ");
		pos += length;
		for (i = 0; i < MAX_USER_TARGET_PRIO; i++) {
			length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				"%llu ", hint_iter->target_time_hint[i]);
			pos += length;
		}
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"]\n");
		pos += length;
	}
	mutex_unlock(&fstb_user_target_hint_lock);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static KOBJ_ATTR_RO(fstb_tfps_info);

static ssize_t fstb_policy_cmd_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char *temp = NULL;
	int pos = 0;
	int length = 0;
	struct fstb_policy_cmd *iter;
	struct hlist_node *h = NULL;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	mutex_lock(&fstb_policy_cmd_lock);

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
		"by process\n");
	pos += length;
	hlist_for_each_entry_safe(iter, h, &fstb_policy_cmd_list, hlist) {
		if (iter->mode == 0) {
			length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				"tgid:%d\ttarget_fps_policy_enable:%d\ttarget_fps_detect_enable:%d\tts:%llu\n",
				iter->tgid,
				iter->target_fps_policy_enable,
				iter->target_fps_detect_enable,
				iter->ts);
			pos += length;
		}
	}

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
		"\nby render\n");
	pos += length;
	hlist_for_each_entry_safe(iter, h, &fstb_policy_cmd_list, hlist) {
		if (iter->mode == 1) {
			length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				"render:%d 0x%llx\ttarget_fps_policy_enable:%d\ttarget_fps_detect_enable:%d\tts:%llu\n",
				iter->pid, iter->bufid,
				iter->target_fps_policy_enable,
				iter->target_fps_detect_enable,
				iter->ts);
			pos += length;
		}
	}

	mutex_unlock(&fstb_policy_cmd_lock);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static KOBJ_ATTR_RO(fstb_policy_cmd);

static ssize_t fstb_frs_info_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char *temp = NULL;
	int i = 0;
	int pos = 0;
	int length = 0;
	struct fstb_frame_info *iter = NULL;
	struct hlist_node *h = NULL;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	mutex_lock(&fstb_lock);

	hlist_for_each_entry_safe(iter, h, &fstb_frame_info_list, hlist) {
		length = scnprintf(temp + pos,
			FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"%d\t%d\t0x%llx\t%d\t%d\n",
			i+1,
			iter->pid,
			iter->bufid,
			iter->raw_target_fpks / 1000,
			iter->target_fps_diff);
		pos += length;
		i++;
	}

	mutex_unlock(&fstb_lock);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static KOBJ_ATTR_RO(fstb_frs_info);

FSTB_SYSFS_WRITE_POLICY_CMD_BY_PROCESS(fstb_target_fps_policy_enable_by_process, 0, 0, 2);
static KOBJ_ATTR_WO(fstb_target_fps_policy_enable_by_process);
FSTB_SYSFS_WRITE_POLICY_CMD_BY_RENDER(fstb_target_fps_policy_enable_by_render, 0, 0, 2);
static KOBJ_ATTR_WO(fstb_target_fps_policy_enable_by_render);
FSTB_SYSFS_WRITE_POLICY_CMD_BY_RENDER(fstb_target_fps_detect_enable_by_render, 1, 0, 1);
static KOBJ_ATTR_WO(fstb_target_fps_detect_enable_by_render);

static ssize_t notify_fstb_target_fps_by_pid_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	char *acBuffer = NULL;
	int tgid;
	int fps = 0;
	unsigned long long time = 0;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (sscanf(acBuffer, "%d %d", &tgid, &fps) == 2) {
				if (fps >= CFG_MIN_FPS_LIMIT && fps <= dfps_ceiling) {
					time = div_u64(NSEC_PER_SEC, fps);
					fpsgo_other2fstb_set_target(0, tgid, 1, 0, fps, time, 0);
				} else
					fpsgo_other2fstb_set_target(0, tgid, 0, 0, fps, time, 0);
			}
		}
	}

out:
	kfree(acBuffer);
	return count;
}
static KOBJ_ATTR_WO(notify_fstb_target_fps_by_pid);

static ssize_t set_user_target_by_prio_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	char *acBuffer = NULL;
	int mode = -1, id1 = -1, prio = -1;
	unsigned long long id2 = 0;
	unsigned long long target = 0;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (sscanf(acBuffer, "%d %d %llu %d %llu",
				&mode, &id1, &id2, &prio, &target) != 5) {
				FPSGO_LOGE("[fstb] %s user set wrong args\n", __func__);
				goto out;
			}

			if (prio < 0 || prio >= MAX_USER_TARGET_PRIO) {
				FPSGO_LOGE("[fstb] %s user set wrong priority:%d\n", __func__, prio);
				goto out;
			}

			if (target >= CFG_MIN_FPS_LIMIT && target <= dfps_ceiling) {
				fpsgo_other2fstb_set_target(mode, id1, 1, prio,
					(int)target, 0, mode ? id2 : 0);
				FPSGO_LOGE("[fstb] user set target mode:%d id:%d 0x%llx prio:%d fps:%d\n",
					mode, id1, id2, prio, (int)target);
			} else if (target >= dtime_ceiling && target < NSEC_PER_SEC) {
				fpsgo_other2fstb_set_target(mode, id1, 1, prio, 0, target, mode ? id2 : 0);
				FPSGO_LOGE("[fstb] user set target mode:%d id:%d 0x%llx prio:%d time:%llu\n",
					mode, id1, id2, prio, target);
			} else {
				fpsgo_other2fstb_set_target(mode, id1, 0, prio, 0, 0, mode ? id2 : 0);
				FPSGO_LOGE("[fstb] user unset target mode:%d id:%d 0x%llx prio:%d\n",
					mode, id1, id2, prio);
			}
		}
	}

out:
	kfree(acBuffer);
	return count;
}
static KOBJ_ATTR_WO(set_user_target_by_prio);

void fpsgo_ktf2fstb_fuzz_test_node(char *input_data, int op, int cmd)
{
	struct kobject *kobj = NULL;
	struct kobj_attribute *attr = NULL;
	char *buf = NULL;

	kobj = kzalloc(sizeof(struct kobject), GFP_KERNEL);
	if (!kobj)
		goto out;

	attr = kzalloc(sizeof(struct kobj_attribute), GFP_KERNEL);
	if (!attr)
		goto out;

	buf = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!buf)
		goto out;

	if (input_data && op)
		scnprintf(buf, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", input_data);

	switch (cmd) {
	case FPSGO_STATUS:
		if (!op)
			fpsgo_ktf_test_read_node(kobj, attr, buf, fpsgo_status_show);
		break;
	case FSTB_DEBUG:
		if (op)
			fpsgo_ktf_test_write_node(kobj, attr, buf, fstb_debug_store);
		else
			fpsgo_ktf_test_read_node(kobj, attr, buf, fstb_debug_show);
		break;
	case FSTB_FPS_LIST:
		break;
	case FSTB_POLICY_CMD:
		if (!op)
			fpsgo_ktf_test_read_node(kobj, attr, buf, fstb_policy_cmd_show);
		break;
	case FSTB_TFPS_INFO:
		if (!op)
			fpsgo_ktf_test_read_node(kobj, attr, buf, fstb_tfps_info_show);
		break;
	case FSTB_FRS_INFO:
		if (!op)
			fpsgo_ktf_test_read_node(kobj, attr, buf, fstb_frs_info_show);
		break;
	case FSTB_TARGET_FPS_POLICY_ENABLE_GLOBAL:
		break;
	case FSTB_TARGET_FPS_POLICY_ENABLE_BY_PID:
		if (op)
			fpsgo_ktf_test_write_node(kobj, attr, buf, fstb_target_fps_policy_enable_by_process_store);
		break;
	case NOTIFY_FSTB_TARGET_FPS_BY_PID:
		if (op)
			fpsgo_ktf_test_write_node(kobj, attr, buf, notify_fstb_target_fps_by_pid_store);
		break;
	case FSTB_NO_R_TIMER_ENABLE:
		if (op)
			fpsgo_ktf_test_write_node(kobj, attr, buf, fstb_no_r_timer_enable_store);
		else
			fpsgo_ktf_test_read_node(kobj, attr, buf, fstb_no_r_timer_enable_show);
		break;
	case FSTB_MARGIN_MODE:
		break;
	case FSTB_MARGIN_MODE_DBNC_A:
		break;
	case FSTB_MARGIN_MODE_DBNC_B:
		break;
	case FSTB_RESET_TOLERENCE:
		break;
	case FSTB_TUNE_QUANTILE:
		if (op)
			fpsgo_ktf_test_write_node(kobj, attr, buf, fstb_tune_quantile_store);
		else
			fpsgo_ktf_test_read_node(kobj, attr, buf, fstb_tune_quantile_show);
		break;
	case GPU_SLOWDOWN_CHECK:
		break;
	case FSTB_SOFT_LEVEL:
		break;
	default:
		break;
	}

out:
	kfree(buf);
	kfree(attr);
	kfree(kobj);
}
EXPORT_SYMBOL(fpsgo_ktf2fstb_fuzz_test_node);

int mtk_fstb_init(void)
{
	fstb_app_time_info_tree = RB_ROOT;

	if (!fpsgo_sysfs_create_dir(NULL, "fstb", &fstb_kobj)) {
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fpsgo_status);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_debug);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_tune_quantile);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_no_r_timer_enable);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_filter_poll_enable);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_policy_cmd);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_target_fps_policy_enable_by_process);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_target_fps_policy_enable_by_render);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_target_fps_detect_enable_by_render);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_notify_fstb_target_fps_by_pid);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_tfps_info);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_frs_info);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_set_user_target_by_prio);
	}

	fstb_wq = alloc_ordered_workqueue("%s", WQ_MEM_RECLAIM | WQ_HIGHPRI, "mt_fstb");

	hrtimer_init(&fstb_hrt, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	fstb_hrt.function = &mt_fstb;

#if IS_ENABLED(CONFIG_MTK_GPU_COMMON_DVFS_SUPPORT)
	ged_kpi_output_gfx_info2_fp = gpu_time_update;
#endif

	return 0;
}

int __exit mtk_fstb_exit(void)
{
	disable_fstb_timer();

	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fpsgo_status);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_debug);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_tune_quantile);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_no_r_timer_enable);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_filter_poll_enable);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_policy_cmd);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_target_fps_policy_enable_by_process);
	fpsgo_sysfs_remove_file(fstb_kobj,
				&kobj_attr_fstb_target_fps_policy_enable_by_render);
	fpsgo_sysfs_remove_file(fstb_kobj,
				&kobj_attr_fstb_target_fps_detect_enable_by_render);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_notify_fstb_target_fps_by_pid);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_tfps_info);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_frs_info);
	fpsgo_sysfs_remove_file(fstb_kobj,
				&kobj_attr_set_user_target_by_prio);

	fpsgo_sysfs_remove_dir(&fstb_kobj);

	return 0;
}
