// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#if IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)
#include "ged_kpi.h"
#endif
#include "mtk_drm_arr.h"
#include "fpsgo_frame_info.h"
#include "sbe_base.h"
#include "sbe_cpu_ctrl.h"
#include "sbe_usedext.h"
#include "sbe_sysfs.h"

#include "util/tsk_util.h"

#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include <linux/sched/cputime.h>
#include <linux/sched/task.h>
#include <sched/sched.h>

#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
#include <eas/vip.h>
#endif

static int sbe_query_is_running;
static int sbe_recycle_idle_cnt;
static int sbe_recycle_active = 1;
static int condition_notifier_wq;
static int sbe_disable_runnable_est;
static struct task_struct *sbe_ktsk;
static struct hrtimer sbe_recycle_hrt;
static LIST_HEAD(head);
static DEFINE_MUTEX(notifier_wq_lock);
static DEFINE_MUTEX(sbe_recycle_lock);
static DECLARE_WAIT_QUEUE_HEAD(notifier_wq_queue);

struct task_info g_dep_arr[FPSGO_MAX_TASK_NUM];

//ConsistencyEngine pointer interface for Taskturbo implement
//We are using sbe when ConsistencyEngine API called.
void (*task_turbo_do_set_binder_uclamp_param)(pid_t pid,
		int binder_uclamp_max, int binder_uclamp_min);
EXPORT_SYMBOL_GPL(task_turbo_do_set_binder_uclamp_param);
void (*task_turbo_do_unset_binder_uclamp_param)(pid_t pid);
EXPORT_SYMBOL_GPL(task_turbo_do_unset_binder_uclamp_param);
void (*task_turbo_do_binder_uclamp_stuff)(int cmd);
EXPORT_SYMBOL_GPL(task_turbo_do_binder_uclamp_stuff);
void (*task_turbo_do_enable_binder_uclamp_inheritance)(int enable);
EXPORT_SYMBOL_GPL(task_turbo_do_enable_binder_uclamp_inheritance);
void (*vip_engine_set_vip_ctrl_node_cs)(int pid, int vip_prio, unsigned int throttle_time);
EXPORT_SYMBOL_GPL(vip_engine_set_vip_ctrl_node_cs);
void (*vip_engine_unset_vip_ctrl_node_cs)(int pid, int vip_prio);
EXPORT_SYMBOL_GPL(vip_engine_unset_vip_ctrl_node_cs);

struct scroll_policy_details_info {
	char thread_name[MAX_PROCESS_NAME_LEN];
	int final_pid_arr[FPSGO_MAX_RENDER_INFO_SIZE];
	int local_specific_tid_arr[FPSGO_MAX_RENDER_INFO_SIZE];
	unsigned long long final_bufID_arr[FPSGO_MAX_RENDER_INFO_SIZE];
	struct task_info local_specific_action_arr[FPSGO_MAX_RENDER_INFO_SIZE];
};

static int sbe_get_render_tid_by_render_name(int tgid, char *name,
	int *out_tid_arr, unsigned long long *out_bufID_arr,
	int *out_tid_num, int out_tid_max_num);

static void sbe_do_recycle(struct work_struct *work)
{
	int non_empty = 0;

	sbe_get_tree_lock(__func__);
	non_empty += !!sbe_check_info_status();
	non_empty += !!sbe_check_render_info_status();
	non_empty += !!sbe_check_spid_loading_status();
	sbe_forece_reset_fpsgo_critical_tasks();
	sbe_put_tree_lock(__func__);

	mutex_lock(&sbe_recycle_lock);
	if (non_empty) {
		sbe_recycle_idle_cnt++;
		if (sbe_recycle_idle_cnt >= MAX_SBE_RECYCLE_IDLE_CNT) {
			sbe_recycle_active = 0;
			goto out;
		}
	}
	hrtimer_start(&sbe_recycle_hrt, ktime_set(0, NSEC_PER_SEC), HRTIMER_MODE_REL);

out:
	mutex_unlock(&sbe_recycle_lock);
}
static DECLARE_WORK(sbe_recycle_work, sbe_do_recycle);

static enum hrtimer_restart sbe_prepare_do_recycle(struct hrtimer *timer)
{
	schedule_work(&sbe_recycle_work);

	return HRTIMER_NORESTART;
}

static void sbe_check_restart_recycle_hrt(void)
{
	mutex_lock(&sbe_recycle_lock);
	if (sbe_recycle_idle_cnt) {
		sbe_recycle_idle_cnt = 0;
		if (!sbe_recycle_active) {
			sbe_recycle_active = 1;
			hrtimer_start(&sbe_recycle_hrt, ktime_set(0, NSEC_PER_SEC), HRTIMER_MODE_REL);
		}
	}
	mutex_unlock(&sbe_recycle_lock);
}

static void sbe_notifier_wq_cb_display_rate(int display_rate)
{
	sbe_set_display_rate(display_rate);
}

static void __sbe_receive_frame_start(struct sbe_render_info *f_render,
	unsigned long long frameID,
	unsigned long long frame_start_time,
	unsigned long long bufID)
{
	int i;

	sbe_systrace_c(f_render->pid, bufID, 1, "[ux]sbe_set_ctrl");

	for (i = 0; i < f_render->dep_num; i++)
		fpsgo_other2comp_set_no_boost_info(1, f_render->dep_arr[i], 1);

	sbe_do_frame_start(f_render, frameID, frame_start_time);
}

void _sbe_set_vip_with_scroll(struct sbe_render_info *thr)
{
	struct sbe_info *s_info = sbe_get_info(thr->tgid, 0);

	if (!s_info) {
		sbe_trace("%d: not find sbe_info ", __func__);
		return;
	}

	if (s_info->ux_scrolling)
		sbe_set_deplist_policy(thr, SBE_TASK_ENABLE);
}

int sbe_validate_time(unsigned long long start, unsigned long long end,
			unsigned long long t_dequeue_start, unsigned long long t_dequeue_end,
			unsigned long long t_enqueue_start, unsigned long long t_enqueue_end)
{

	// Check basic condition
	if (start >= end)
		return -1;

	// Check if all time points are between start and end
	if (t_dequeue_start <= start || t_dequeue_end >= end ||
		t_enqueue_start <= start || t_enqueue_end >= end) {
		return -1;
	}

	// Verify chronological order
	if (t_dequeue_start >= t_dequeue_end ||
		t_dequeue_end >= t_enqueue_start ||
		t_enqueue_start >= t_enqueue_end) {
		return -1;
	}

	// All conditions satisfied
	return 0;
}

void sbe_receive_webfunctor(int pid, unsigned long long identifier)
{
	struct sbe_render_info *f_render;

	sbe_get_tree_lock(__func__);
	f_render = sbe_get_render_info(pid, identifier, 0);
	if (!f_render) {
		sbe_put_tree_lock(__func__);
		return;
	}

	f_render->is_webfunctor = 1;
	sbe_systrace_c(f_render->pid, f_render->buffer_id,
				f_render->is_webfunctor, "[ux]webfunctor");
	// f_render->ux_affinity_task_basic_cap = 20;
	sbe_put_tree_lock(__func__);
}

int sbe_validate_dequeue_time_span(unsigned long long t_dequeue_start,
				unsigned long long t_dequeue_end)
{
	unsigned long long t_dequeue_time = t_dequeue_end - t_dequeue_start;
	unsigned long long dequeue_margin_time = get_sbe_extra_sub_deque_margin_time();

	if (t_dequeue_time >= dequeue_margin_time)
		return 0;

	return -1;
}

static void __sbe_receive_frame_end(struct sbe_render_info *f_render,
	unsigned long long frame_start_time,
	unsigned long long frame_end_time,
	unsigned long long frameid,
	unsigned long long bufID)
{
	int i;
	unsigned long long local_ema = 0, local_raw = 0;
	unsigned long long enq_running_time = 0;
	struct task_info *tmp_dep_arr = NULL;
	struct render_frame_info *fpsgo_render_info = NULL;
	int find_match_render = 0;
	int render_num = 0;
	unsigned long long t_dequeue_start = 0;
	unsigned long long t_dequeue_end   = 0;
	unsigned long long t_enqueue_start = 0;
	unsigned long long t_enqueue_end   = 0;

	if (get_sbe_extra_sub_en_deque_enable()) {
		fpsgo_render_info = kcalloc(FPSGO_MAX_RENDER_INFO_SIZE, sizeof(struct render_frame_info), GFP_KERNEL);
		if (!fpsgo_render_info) {
			fpsgo_other2xgf_calculate_dep(f_render->pid, f_render->buffer_id,
				&local_raw, &local_ema, &enq_running_time,
				frame_start_time, frame_end_time,
				0, 0, 0, 0, 0);
			sbe_trace("[SBE]: failed to allocate memory for fpsgo_render_info");
		} else {
			render_num = get_fpsgo_frame_info(FPSGO_MAX_RENDER_INFO_SIZE, 1 << GET_FPSGO_QDQ_TS,
						0, -1, fpsgo_render_info);
			sbe_trace("[SBE]: get f_render_info: %d", render_num);
			for (i = 0; i < render_num; i++) {
				if (fpsgo_render_info[i].pid == f_render->pid) {
					t_dequeue_start = fpsgo_render_info[i].t_dequeue_start;
					t_dequeue_end   = fpsgo_render_info[i].t_dequeue_end;
					t_enqueue_start = fpsgo_render_info[i].t_enqueue_start;
					t_enqueue_end   = fpsgo_render_info[i].t_enqueue_end;
					find_match_render = 1;
					break;
				}
			}
			kfree(fpsgo_render_info);
		}

		if (find_match_render
			&& !sbe_validate_time(frame_start_time, frame_end_time, t_dequeue_start,
			t_dequeue_end, t_enqueue_start, t_enqueue_end)
			&& !sbe_validate_dequeue_time_span(t_dequeue_start, t_dequeue_end)) {
			fpsgo_other2xgf_calculate_dep(f_render->pid, f_render->buffer_id,
				&local_raw, &local_ema, &enq_running_time,
				frame_start_time, frame_end_time,
				t_dequeue_start, t_dequeue_end,
				t_enqueue_start, t_enqueue_end, 0);
			sbe_trace("[SBE]:id:%llu,f_s:%llu,f_e:%llu,t_dq_s:%llu,t_dq_e:%llu,t_eq_s:%llu,t_eq_e:%llu\n",
			frameid, frame_start_time, frame_end_time, t_dequeue_start,
			t_dequeue_end, t_enqueue_start, t_enqueue_end);
		} else {
			fpsgo_other2xgf_calculate_dep(f_render->pid, f_render->buffer_id,
				&local_raw, &local_ema, &enq_running_time,
				frame_start_time, frame_end_time,
				0, 0, 0, 0, 0);
		}
	} else
		fpsgo_other2xgf_calculate_dep(f_render->pid, f_render->buffer_id,
			&local_raw, &local_ema, &enq_running_time,
			frame_start_time, frame_end_time,
			0, 0, 0, 0, 0);

	f_render->raw_running_time = local_raw;
	f_render->ema_running_time = local_ema;
	sbe_systrace_c(f_render->pid, bufID, (int)local_raw, "[ux]raw_t_cpu");
	sbe_systrace_c(f_render->pid, bufID, (int)local_ema, "[ux]t_cpu");

	f_render->target_fps = sbe_get_display_rate();
	f_render->target_time = div_u64(NSEC_PER_SEC, sbe_get_display_rate());
	sbe_do_frame_end(f_render, frameid, frame_start_time, frame_end_time);
	// notify GPU target fps
	ged_kpi_set_target_FPS_margin(f_render->buffer_id, f_render->target_fps,
		0, 0, f_render->ema_running_time);
	sbe_systrace_c(f_render->pid, bufID, 0, "[ux]sbe_set_ctrl");

	for (i = 0; i < f_render->dep_num; i++)
		fpsgo_other2comp_set_no_boost_info(1, f_render->dep_arr[i], 0);

	memset(f_render->dep_arr, 0, MAX_TASK_NUM * sizeof(int));
	tmp_dep_arr = kcalloc(MAX_TASK_NUM, sizeof(struct task_info), GFP_KERNEL);
	if (tmp_dep_arr) {
		f_render->dep_num = fpsgo_other2xgf_get_critical_tasks(f_render->pid,
			MAX_TASK_NUM, tmp_dep_arr, 1, f_render->buffer_id);
		if (f_render->dep_num > 0 && f_render->dep_num <= MAX_TASK_NUM) {
			for (i = 0; i < f_render->dep_num; i++)
				f_render->dep_arr[i] = tmp_dep_arr[i].pid;
		} else
			f_render->dep_num = 0;
		kfree(tmp_dep_arr);
	} else {
		sbe_systrace_c(f_render->pid, bufID, frameid, "[ux]get_dep_malloc_fail");
		sbe_systrace_c(f_render->pid, bufID, 0, "[ux]get_dep_malloc_fail");
	}
	//frame_end blc must be 0, set sbe enhance to new dep
	sbe_set_per_task_cap(f_render);
	_sbe_set_vip_with_scroll(f_render);
}

static void sbe_receive_frame_err(int pid,
	unsigned long long frameID,
	unsigned long long time,
	unsigned long long identifier)
{
	struct sbe_render_info *f_render;
	struct ux_frame_info *frame_info;
	int ux_frame_cnt = 0;
	int i;

	sbe_get_tree_lock(__func__);

	f_render = sbe_get_render_info(pid, identifier, 0);
	if (!f_render) {
		sbe_put_tree_lock(__func__);
		return;
	}

	f_render->latest_use_ts = time; // for recycle only.

	mutex_lock(&f_render->ux_mlock);
	frame_info = sbe_search_and_add_frame_info(f_render, frameID, time, 0);
	if (!frame_info) {
		sbe_systrace_c(pid, identifier, frameID, "[ux]start_not_found");
		sbe_systrace_c(pid, identifier, 0, "[ux]start_not_found");
	} else
		sbe_delete_frame_info(f_render, frame_info);
	ux_frame_cnt = sbe_count_frame_info(f_render, 1);
	if (ux_frame_cnt == 0)
		sbe_systrace_c(f_render->pid, identifier, 0, "[ux]sbe_set_ctrl");

	sbe_do_frame_err(f_render, ux_frame_cnt, frameID, time);
	sbe_systrace_c(pid, identifier, ux_frame_cnt, "[ux]ux_frame_cnt");
	mutex_unlock(&f_render->ux_mlock);

	for (i = 0; i < f_render->dep_num; i++)
		fpsgo_other2comp_set_no_boost_info(1, f_render->dep_arr[i], 0);

	sbe_put_tree_lock(__func__);
}

void sbe_receive_frame_start(int pid,
	unsigned long long frameID,
	unsigned long long frame_start_time,
	unsigned long long identifier)
{
	struct sbe_render_info *f_render;
	struct ux_frame_info *frame_info;
	int ux_frame_cnt = 0;

	sbe_get_tree_lock(__func__);

	// prepare render info
	f_render = sbe_get_render_info(pid, identifier, 1);
	if (!f_render) {
		sbe_put_tree_lock(__func__);
		return;
	}

	// fill the frame info
	f_render->buffer_id = identifier;	        // UX: using a magic number 5566
	f_render->latest_use_ts = frame_start_time; // for recycle only
	f_render->t_last_start = frame_start_time;
	f_render->dep_self_ctrl = 1;

	mutex_lock(&f_render->ux_mlock);
	frame_info = sbe_search_and_add_frame_info(f_render, frameID, frame_start_time, 1);
	if (!frame_info) {
		sbe_systrace_c(pid, identifier, frameID, "[ux]start_malloc_fail");
		sbe_systrace_c(pid, identifier, 0, "[ux]start_malloc_fail");
	}
	ux_frame_cnt = sbe_count_frame_info(f_render, 2);
	sbe_systrace_c(pid, identifier, ux_frame_cnt, "[ux]ux_frame_cnt");
	mutex_unlock(&f_render->ux_mlock);

	// if not overlap, call frame start.
	if (ux_frame_cnt == 1)
		__sbe_receive_frame_start(f_render, frameID, frame_start_time, identifier);

	sbe_put_tree_lock(__func__);

	// TODO : need to notify EAS ?
	// fpsgo_com_notify_fpsgo_is_boost(1);
}

void sbe_receive_frame_end(int pid,
	unsigned long long frameID,
	unsigned long long frame_end_time,
	unsigned long long identifier)
{
	struct sbe_render_info *f_render;
	struct ux_frame_info *frame_info;
	unsigned long long frame_start_time = 0;
	int ux_frame_cnt = 0;

	sbe_get_tree_lock(__func__);

	// prepare frame info.
	f_render = sbe_get_render_info(pid, identifier, 0);
	if (!f_render) {
		sbe_put_tree_lock(__func__);
		return;
	}

	// fill the frame info.
	f_render->latest_use_ts = frame_end_time; // for recycle only.

	mutex_lock(&f_render->ux_mlock);
	frame_info = sbe_search_and_add_frame_info(f_render, frameID, frame_start_time, 0);
	if (!frame_info) {
		sbe_systrace_c(pid, identifier, frameID, "[ux]start_not_found");
		sbe_systrace_c(pid, identifier, 0, "[ux]start_not_found");
	} else {
		frame_start_time = frame_info->start_ts;
		sbe_delete_frame_info(f_render, frame_info);
	}
	ux_frame_cnt = sbe_count_frame_info(f_render, 1);
	mutex_unlock(&f_render->ux_mlock);

	// frame end.
	__sbe_receive_frame_end(f_render, frame_start_time, frame_end_time, frameID, identifier);
	if (ux_frame_cnt == 1)
		__sbe_receive_frame_start(f_render, frameID, frame_end_time, identifier);
	sbe_systrace_c(pid, identifier, ux_frame_cnt, "[ux]ux_frame_cnt");

	sbe_put_tree_lock(__func__);
}

void sbe_receive_doframe_end(int pid,
	unsigned long long frameID,
	unsigned long long frame_end_time,
	unsigned long long identifier, long long frame_flags)
{
	struct sbe_render_info *f_render;

	sbe_get_tree_lock(__func__);
	f_render = sbe_get_render_info(pid, identifier, 0);
	if (!f_render) {
		sbe_put_tree_lock(__func__);
		return;
	}

	sbe_exec_doframe_end(f_render, frameID, frame_flags);
	sbe_put_tree_lock(__func__);
}

void sbe_set_critical_task(int cur_pid, unsigned long long id,
		int dep_mode, char *dep_name, int dep_num)
{
	int i;
	int local_specific_tid_num = 0;
	int *local_specific_tid_arr = NULL;
	struct task_info *local_specific_action_arr = NULL;
	int out_tid_arr[FPSGO_MAX_RENDER_INFO_SIZE];
	unsigned long long out_bufID_arr[FPSGO_MAX_RENDER_INFO_SIZE];
	int out_tid_num = 0;
	struct sbe_render_info *thr = NULL;

	sbe_get_render_tid_by_render_pid(sbe_get_tgid(cur_pid), cur_pid, out_tid_arr,
		out_bufID_arr, &out_tid_num, FPSGO_MAX_RENDER_INFO_SIZE);
	if (out_tid_num <= 0)
		return;

	if (dep_mode && dep_num > 0) {
		local_specific_tid_arr = kcalloc(dep_num, sizeof(int), GFP_KERNEL);
		local_specific_action_arr = kcalloc(dep_num, sizeof(struct task_info), GFP_KERNEL);
		if (local_specific_tid_arr && local_specific_action_arr) {
			int local_action = 0;
			int op_dep_by_tid = 0;

			switch (dep_mode) {
			case 1:
				local_action = XGF_DEL_DEP;
				break;
			case 2:
				local_action = XGF_ADD_DEP_NO_LLF;
				break;
			case 3:
				local_action = XGF_ADD_DEP_NO_LLF;
				op_dep_by_tid = 1;
				break;
			case 4:
				local_action = XGF_DEL_DEP;
				op_dep_by_tid = 1;
				break;
			default:
				local_action = -100;
				break;
			}

			if (local_action != -100) {
				if (op_dep_by_tid)
					local_specific_tid_num = sbe_split_task_tid(dep_name, dep_num,
						local_specific_tid_arr, __func__);
				else
					local_specific_tid_num = sbe_split_task_name(sbe_get_tgid(cur_pid),
						dep_name, dep_num, local_specific_tid_arr, __func__);

				for (i = 0; i < local_specific_tid_num; i++) {
					local_specific_action_arr[i].pid = local_specific_tid_arr[i];
					local_specific_action_arr[i].action = local_action;
				}

				//clear dep set before
				fpsgo_other2xgf_set_critical_tasks(cur_pid, id, NULL, 0, 0);
				//set new dep task
				if (local_specific_tid_num > 0) {
					fpsgo_other2xgf_set_critical_tasks(cur_pid, id,
						local_specific_action_arr, local_specific_tid_num, 1);
					sbe_get_tree_lock(__func__);
					thr = sbe_get_render_info(cur_pid, id, 1);
					if (thr)
						thr->fpsgo_critical_flag = 1;
					sbe_put_tree_lock(__func__);
				}
			}
		}

		kfree(local_specific_action_arr);
		kfree(local_specific_tid_arr);
	} else if (dep_mode == 5 && dep_num == 0) {
		//CLEAR dep set before
		fpsgo_other2xgf_set_critical_tasks(cur_pid, id, NULL, 0, 0);
		sbe_get_tree_lock(__func__);
		thr = sbe_get_render_info(cur_pid, id, 0);
		if (thr)
			thr->fpsgo_critical_flag = 0;
		sbe_put_tree_lock(__func__);
	}
}

static int sbe_check_render_tasks_exist(int tgid, char *dep_name, int dep_num)
{
	char *token, *str, *tmp_str;
	int out_tid_arr[FPSGO_MAX_RENDER_INFO_SIZE];
	unsigned long long out_bufID_arr[FPSGO_MAX_RENDER_INFO_SIZE];
	int out_tid_num;
	int found_tid = 0;

	if (!dep_name || dep_num <= 0)
		return 0;

	str = kstrdup(dep_name, GFP_KERNEL);
	if (!str)
		return 0;

	tmp_str = str;
	while ((token = strsep(&tmp_str, ",")) != NULL) {
		out_tid_num = 0;
		if (!sbe_get_render_tid_by_render_name(tgid, token,
				out_tid_arr, out_bufID_arr, &out_tid_num, FPSGO_MAX_RENDER_INFO_SIZE)
				&& out_tid_num > 0) {
			found_tid = 1;
			break;
		}
	}

	kfree(str);
	return found_tid;
}

void sbe_del_dep_if_render_in_same_proc(int cur_pid, unsigned long long id,
		int dep_mode, char *dep_name, int dep_num)
{
	int found_tid;
	int tgid;

	if (!dep_name || dep_num <= 0)
		return;

	tgid = sbe_get_tgid(cur_pid);
	if (tgid <= 0)
		return;

	found_tid = sbe_check_render_tasks_exist(tgid, dep_name, dep_num);
	if (found_tid > 0)
		sbe_set_critical_task(cur_pid, id, dep_mode, dep_name, dep_num);
}

static void sbe_notifier_wq_cb_rescue(int pid, int start, int enhance,
	int rescue_type, unsigned long long rescue_target, unsigned long long frameID)
{
	unsigned long long buffer_id = SBE_HWUI_BUFFER_ID; // align with HWUI buffer id
	struct sbe_render_info *f_render;

	sbe_get_tree_lock(__func__);
	f_render = sbe_get_render_info(pid, buffer_id, 0);
	if (f_render)
		sbe_do_rescue(f_render, start, enhance, rescue_type, rescue_target, frameID);
	sbe_put_tree_lock(__func__);
}

static void sbe_notifier_wq_cb_hwui_frame_hint(int start,
		int cur_pid, unsigned long long frameID,
		unsigned long long curr_ts, unsigned long long id,
		int dep_mode, char *dep_name, int dep_num, long long frame_flags)
{

	switch (start) {
	case -100:
		//hint this is webfunctor page
		sbe_receive_webfunctor(cur_pid, id);
		break;
	case -1:
		sbe_receive_frame_err(cur_pid, frameID, curr_ts, id);
		break;
	case 0:
		sbe_receive_frame_start(cur_pid, frameID, curr_ts, id);
		sbe_del_dep_if_render_in_same_proc(cur_pid, id, dep_mode, dep_name, dep_num);
		break;
	case 1:
		sbe_receive_frame_end(cur_pid, frameID, curr_ts, id);
		sbe_set_critical_task(cur_pid, id, dep_mode, dep_name, dep_num);
		break;
	case 2:
		sbe_receive_doframe_end(cur_pid, frameID, curr_ts, id, frame_flags);
		break;
	case 3://only dep actions
		sbe_set_critical_task(cur_pid, id, dep_mode, dep_name, dep_num);
		break;
	default:
		break;
	}
}

int sbe_get_render_tid_by_render_name(int tgid, char *name,
	int *out_tid_arr, unsigned long long *out_bufID_arr,
	int *out_tid_num, int out_tid_max_num)
{
	int i;
	int index = 0;
	struct render_fw_info *tmp_arr = NULL;
	struct task_struct *tsk;

	if (tgid <= 0 || !name ||
		!out_tid_arr || !out_bufID_arr ||
		!out_tid_num || out_tid_max_num <= 0)
		return -EINVAL;

	tmp_arr = kcalloc(out_tid_max_num, sizeof(struct render_fw_info), GFP_KERNEL);
	if (!tmp_arr)
		return -ENOMEM;

	fpsgo_other2comp_get_render_fw_info(0, out_tid_max_num, out_tid_num, tmp_arr);
	for (i = 0; i < *out_tid_num; i++) {
		if (tmp_arr[i].producer_tgid != tgid)
			continue;

		rcu_read_lock();
		tsk = find_task_by_vpid(tmp_arr[i].producer_pid);
		if (tsk) {
			get_task_struct(tsk);
			if (!strncmp(tsk->comm, name, 16) && index < out_tid_max_num) {
				out_tid_arr[index] = tmp_arr[i].producer_pid;
				out_bufID_arr[index] = tmp_arr[i].buffer_id;
				index++;
			}
			put_task_struct(tsk);
		}
		rcu_read_unlock();
	}
	*out_tid_num = index;

	kfree(tmp_arr);

	return 0;
}

void sbe_disable_runnable_est_boost(int disable)
{
	if (!get_sbe_disable_runnable_util_est_status()) {
		if (sbe_disable_runnable_est) {
			reset_runnable_boost_util_est_enable();
			sbe_disable_runnable_est = 0;
			sbe_trace("[SBE]: Runnable_EST_Boost=%d", 0);
		}
		return;
	}

	if (disable) {
		set_runnable_boost_util_est_enable(0);
		sbe_disable_runnable_est = 1;
	} else {
		reset_runnable_boost_util_est_enable();
		sbe_disable_runnable_est = 0;
	}

	sbe_trace("[SBE]: Runnable_EST_Boost=%d", sbe_disable_runnable_est);
}

void sbe_enforce_update_sbe_info_by_thread_name(int tgid, char *thread_name,
		int start, unsigned long long ts)
{
	struct sbe_render_info *sbe_thr = NULL;

	sbe_thr = sbe_get_render_info_by_thread_name(tgid, thread_name);
	if(!sbe_thr)
		return;

	if (!start) {
		sbe_thr->latest_use_ts = ts;
		sbe_thr->scroll_status = 0;
		__sbe_set_per_task_cap(sbe_thr, 0, 100);
		sbe_systrace_c(sbe_thr->pid, sbe_thr->buffer_id, 0, "[ux]sbe_set_ctrl");
		sbe_systrace_c(sbe_thr->pid, sbe_thr->buffer_id, 0, "[ux]perf_idx");
		sbe_systrace_c(sbe_thr->pid, sbe_thr->buffer_id, 100, "[ux]perf_idx_max");
		sbe_forece_reset_fpsgo_critical_tasks();
	}
}

static int sbe_do_running_check(int start, int tgid, char *specific_name, int num)
{
	int ret = 0;
	int local_specific_tid_num = 0;
	int local_specific_tid_arr[FPSGO_MAX_RENDER_INFO_SIZE];

	if (num < 0 || num >= FPSGO_MAX_RENDER_INFO_SIZE
		|| specific_name == NULL || tgid <= 0 ) {
		ret = SBE_INPUT_ERROR;
		return ret;
	}

	if (start) {
		local_specific_tid_num = sbe_split_task_name(tgid,
			specific_name, num, local_specific_tid_arr, __func__);
		sbe_update_spid_loading(local_specific_tid_arr,
			local_specific_tid_num, tgid);
	} else
		sbe_delete_spid_loading(tgid);

	return ret;
}

static int sbe_do_hwui_scrolling_policy(int tgid, int start, char *specific_name, int num,
				unsigned long mask)
{
	int ret = 0;
	struct sbe_info *sbe_info = NULL;

	if (num < 0 || num >= FPSGO_MAX_RENDER_INFO_SIZE
		|| tgid <= 0 ) {
		ret = SBE_INPUT_ERROR;
		return ret;
	}

	sbe_get_tree_lock(__func__);
	sbe_info = sbe_get_info(tgid, 1);
	if (sbe_info)
		sbe_info->ux_scrolling = start;
	sbe_put_tree_lock(__func__);

	set_sbe_thread_vip(start, tgid, specific_name, num);
	sbe_enable_vip_sitch(start, tgid);

	if (!start && (test_bit(SBE_PAGE_FLUTTER, &mask)
			|| test_bit(SBE_PAGE_WEBVIEW, &mask))) {
		//force clear vip when scrolling end
		update_fpsgo_hint_param(start, tgid);
	}

	return ret;
}

static int sbe_do_webview_notify_fpsgo_ctrl(int tgid, char *name, int start, char *specific_name,
					int num, unsigned long long ts)
{
	int ret = SBE_SUCCESS;
	int final_pid_arr_idx = 0;
	int local_specific_tid_num = 0;
	int i = 0;
	struct xgf_policy_cmd xgf_attr_iter;
	struct fpsgo_boost_attr attr_iter;
	struct sbe_render_info *thr = NULL;
	struct scroll_policy_details_info scroll_policy_info = {0};

	if (num < 0 || num >= FPSGO_MAX_RENDER_INFO_SIZE
		|| specific_name == NULL || tgid <= 0 || name == NULL) {
		ret = SBE_INPUT_ERROR;
		return ret;
	}

	if (strscpy(scroll_policy_info.thread_name, name, MAX_PROCESS_NAME_LEN) < 0) {
		ret = SBE_COPY_STR_ERROR;
		return ret;
	}
	scroll_policy_info.thread_name[MAX_PROCESS_NAME_LEN - 1] = '\0';

	sbe_get_render_tid_by_render_name(tgid, scroll_policy_info.thread_name,
		scroll_policy_info.final_pid_arr, scroll_policy_info.final_bufID_arr,
		&final_pid_arr_idx, FPSGO_MAX_RENDER_INFO_SIZE);

	if (!final_pid_arr_idx) {
		sbe_get_tree_lock(__func__);
		sbe_enforce_update_sbe_info_by_thread_name(tgid, scroll_policy_info.thread_name, start, ts);
		sbe_put_tree_lock(__func__);
		ret = SBE_PID_NOT_FIND;
		return ret;
	}

	for (i = 0; i < final_pid_arr_idx; i++) {
		sbe_get_tree_lock(__func__);
		thr = sbe_get_render_info(scroll_policy_info.final_pid_arr[i],
			scroll_policy_info.final_bufID_arr[i], 1);
		if (thr) {
			thr->latest_use_ts = ts;
			thr->scroll_status = start;
		}
		sbe_put_tree_lock(__func__);
		sbe_systrace_c(scroll_policy_info.final_pid_arr[i], scroll_policy_info.final_bufID_arr[i],
				start, "[ux]sbe_set_ctrl");
		sbe_trace("[SBE]: switch fpsgo control: pid=%d, frameID=%llu, start=%d",
				scroll_policy_info.final_pid_arr[i], scroll_policy_info.final_bufID_arr[i], start);

		update_fpsgo_hint_param(start, tgid);
		/*
		 * Call General API for notify FPSGO control
		 * switch_fpsgo_control:
		 * @mode: 0 by process control, 1 by render control
		 * @pid: tgid with mode is 0, render pid with mode is 1
		 * @set_ctrl: boost is 1, deboost is 0
		 * @buffer_id: 0 with mode is 0, render buffer id with mode is 1
		 */
		switch_fpsgo_control(1, scroll_policy_info.final_pid_arr[i], start,
							scroll_policy_info.final_bufID_arr[i]);

		if (start) {
			memset(&xgf_attr_iter, 0, sizeof(struct xgf_policy_cmd));
			xgf_attr_iter.mode = 1;
			xgf_attr_iter.pid = scroll_policy_info.final_pid_arr[i];
			xgf_attr_iter.bufid = scroll_policy_info.final_bufID_arr[i];
			xgf_attr_iter.calculate_dep_enable = 1;
			xgf_attr_iter.ts = sbe_get_time();
			fpsgo_other2xgf_set_attr(1, &xgf_attr_iter);

			memset(&attr_iter, 0, sizeof(struct fpsgo_boost_attr));
			attr_iter.aa_enable_by_pid = 1;
			attr_iter.rescue_enable_by_pid = 1;
			attr_iter.gcc_enable_by_pid = 0;
			attr_iter.qr_enable_by_pid = 0;
			set_fpsgo_attr(1, scroll_policy_info.final_pid_arr[i], 1, &attr_iter);
		} else {
			memset(&xgf_attr_iter, 0, sizeof(struct xgf_policy_cmd));
			xgf_attr_iter.mode = 1;
			xgf_attr_iter.pid = scroll_policy_info.final_pid_arr[i];
			xgf_attr_iter.bufid = scroll_policy_info.final_bufID_arr[i];
			xgf_attr_iter.ts = sbe_get_time();
			fpsgo_other2xgf_set_attr(0, &xgf_attr_iter);
			set_fpsgo_attr(1, scroll_policy_info.final_pid_arr[i], 0, &attr_iter);

			/*
			 * It's crucial to notify fpsgo when scrolling stops.
			 * Failure to do so will result in the thread's performance index
			 *remaining at its last value, preventing it from resetting.
			 */
			fpsgo_other2comp_control_pause(scroll_policy_info.final_pid_arr[i],
					scroll_policy_info.final_bufID_arr[i]);
		}

		sbe_trace("[SBE] %s %dth rtid:%d buffer_id:0x%llx", __func__, i+1,
			scroll_policy_info.final_pid_arr[i], scroll_policy_info.final_bufID_arr[i]);
	}

	if (final_pid_arr_idx > 0) {
		local_specific_tid_num = sbe_split_task_name(tgid,
				specific_name, num, scroll_policy_info.local_specific_tid_arr, __func__);
		for (i = 0; i < local_specific_tid_num; i++) {
			scroll_policy_info.local_specific_action_arr[i].pid =
				scroll_policy_info.local_specific_tid_arr[i];
			scroll_policy_info.local_specific_action_arr[i].action = 0;  // XGF_ADD_DEP
		}
		/*
		 * TODO: This might be redundant?
		 * Manually adding render thread to dep list.
		 * Consider removing this in the future.
		 * Evaluate the impact of removal and optimize
		 * only if it doesn't cause any issues.
		 */
		for (i = 0; i < final_pid_arr_idx; i++) {
			if (start && local_specific_tid_num > 0) {
				fpsgo_other2xgf_set_critical_tasks(scroll_policy_info.final_pid_arr[i],
					scroll_policy_info.final_bufID_arr[i],
					scroll_policy_info.local_specific_action_arr,
					local_specific_tid_num, 1);
				sbe_trace("[SBE]: %s pid %d, bufID %llu, start %d, local_s_tid_num %d",
					__func__,
					scroll_policy_info.final_pid_arr[i],
					scroll_policy_info.final_bufID_arr[i],
					start, local_specific_tid_num);

				sbe_get_tree_lock(__func__);
				thr = sbe_get_render_info(scroll_policy_info.final_pid_arr[i],
					scroll_policy_info.final_bufID_arr[i], 1);
				if (thr) {
					thr->latest_use_ts = ts;
					thr->scroll_status = start;
					thr->fpsgo_critical_flag = 1;
				}
				sbe_put_tree_lock(__func__);
			}
		}
	}

	if (!start) {
		sbe_get_tree_lock(__func__);
		sbe_forece_reset_fpsgo_critical_tasks();
		sbe_put_tree_lock(__func__);
	}

	return ret;
}

static int sbe_do_display_target_fps(int tgid, char *name, int start, unsigned long long ts)
{
	int ret = SBE_SUCCESS;
	int final_pid_arr_idx = 0;
	int i = 0;
	struct scroll_policy_details_info scroll_policy_info = {0};

	if (!name || tgid < 0) {
		ret = SBE_INPUT_ERROR;
		return ret;
	}

	if (strscpy(scroll_policy_info.thread_name, name, MAX_PROCESS_NAME_LEN) < 0) {
		ret = SBE_COPY_STR_ERROR;
		return ret;
	}
	scroll_policy_info.thread_name[MAX_PROCESS_NAME_LEN - 1] = '\0';

	sbe_get_render_tid_by_render_name(tgid, scroll_policy_info.thread_name,
		scroll_policy_info.final_pid_arr, scroll_policy_info.final_bufID_arr,
		&final_pid_arr_idx, FPSGO_MAX_RENDER_INFO_SIZE);

	if (!final_pid_arr_idx) {
		sbe_get_tree_lock(__func__);
		sbe_enforce_update_sbe_info_by_thread_name(tgid, scroll_policy_info.thread_name, start, ts);
		sbe_put_tree_lock(__func__);
		ret = SBE_PID_NOT_FIND;
		return ret;
	}

	for (i = 0; i < final_pid_arr_idx; i++)
		fpsgo_other2fstb_set_target(1, scroll_policy_info.final_pid_arr[i],
			start, 0, start ? sbe_get_display_rate() : 0, 0, scroll_policy_info.final_bufID_arr[i]);

	return ret;
}

static int sbe_do_clear_scrolling_info(int tgid, char *name, unsigned long long ts)
{
	int ret = SBE_SUCCESS;
	int i = 0;
	int final_pid_arr_idx = 0;
	struct sbe_render_info *thr = NULL;
	unsigned int display_rate = 0;
	struct scroll_policy_details_info scroll_policy_info = {0};

	if (!name || tgid < 0 ) {
		ret = SBE_INPUT_ERROR;
		return ret;
	}

	if (strscpy(scroll_policy_info.thread_name, name, MAX_PROCESS_NAME_LEN) < 0) {
		ret = SBE_COPY_STR_ERROR;
		return ret;
	}
	scroll_policy_info.thread_name[MAX_PROCESS_NAME_LEN - 1] = '\0';

	sbe_get_render_tid_by_render_name(tgid, scroll_policy_info.thread_name,
		scroll_policy_info.final_pid_arr, scroll_policy_info.final_bufID_arr,
		&final_pid_arr_idx, FPSGO_MAX_RENDER_INFO_SIZE);

	for (i = 0; i < final_pid_arr_idx; i++) {
		sbe_get_tree_lock(__func__);
		thr = sbe_get_render_info(scroll_policy_info.final_pid_arr[i],
			scroll_policy_info.final_bufID_arr[i], 0);
		if (!thr) {
			sbe_systrace_c(scroll_policy_info.final_pid_arr[i],
				scroll_policy_info.final_bufID_arr[i], -1, "[ux]sbe_set_ctrl");
			sbe_put_tree_lock(__func__);
			continue;
		}

		display_rate = sbe_get_display_rate();
		thr->target_fps = display_rate;
		if (display_rate > 0)
			thr->target_time = div_u64(NSEC_PER_SEC, display_rate);
		else
			thr->target_time = div_u64(NSEC_PER_SEC, SBE_DEFAULT_TARGET_FPS);
		thr->dep_self_ctrl = 0;
		thr->latest_use_ts = ts;
		thr->dep_self_ctrl = 1;

		clear_ux_info(thr);
		sbe_put_tree_lock(__func__);
	}

	return ret;
}

static int sbe_do_hwui_scrolling_status_policy(int tgid, char *name, unsigned long mask,
				unsigned long long ts, int start,
				char *specific_name, int num)
{
	int ret = SBE_SUCCESS;
	int i = 0;
	int type = 0;
	int final_pid_arr_idx = 0;
	int add_new_scrolling = 1;
	int local_specific_tid_num = 0;
	int critical_basic_cap = 0;
	unsigned int display_rate = 0;

	struct sbe_render_info *thr = NULL;
	struct ux_scroll_info *last = NULL;
	struct xgf_policy_cmd xgf_attr_iter;
	struct fpsgo_boost_attr attr_iter;
	struct scroll_policy_details_info scroll_policy_info = {0};

	if (num < 0 || num >= FPSGO_MAX_RENDER_INFO_SIZE
		|| specific_name == NULL || tgid <= 0 || name == NULL) {
		ret = SBE_INPUT_ERROR;
		return ret;
	}

	if (strscpy(scroll_policy_info.thread_name, name, MAX_PROCESS_NAME_LEN) < 0) {
		ret = SBE_COPY_STR_ERROR;
		return ret;
	}
	scroll_policy_info.thread_name[MAX_PROCESS_NAME_LEN - 1] = '\0';

	sbe_get_render_tid_by_render_name(tgid, scroll_policy_info.thread_name,
		scroll_policy_info.final_pid_arr, scroll_policy_info.final_bufID_arr,
		&final_pid_arr_idx, FPSGO_MAX_RENDER_INFO_SIZE);

	sbe_notify_fpsgo_do_virtual_boost(start, tgid, tgid);

	if (!final_pid_arr_idx) {
		sbe_get_tree_lock(__func__);
		sbe_enforce_update_sbe_info_by_thread_name(tgid, scroll_policy_info.thread_name,
						start, ts);
		sbe_put_tree_lock(__func__);
		sbe_disable_runnable_est_boost(0);
		ret = SBE_PID_NOT_FIND;
		return ret;
	}

	sbe_disable_runnable_est_boost(start);

	for (i = 0; i < final_pid_arr_idx; i++) {
		sbe_get_tree_lock(__func__);
		thr = sbe_get_render_info(scroll_policy_info.final_pid_arr[i],
				scroll_policy_info.final_bufID_arr[i], 1);
		if (!thr) {
			sbe_systrace_c(scroll_policy_info.final_pid_arr[i],
				scroll_policy_info.final_bufID_arr[i], -1, "[ux]sbe_set_ctrl");
			sbe_put_tree_lock(__func__);
			continue;
		}

		sbe_systrace_c(scroll_policy_info.final_pid_arr[i], scroll_policy_info.final_bufID_arr[i],
			start, "[ux]sbe_set_ctrl");
		display_rate = sbe_get_display_rate();
		thr->target_fps = display_rate;
		if (display_rate > 0)
			thr->target_time = div_u64(NSEC_PER_SEC, display_rate);
		else
			thr->target_time = div_u64(NSEC_PER_SEC, SBE_DEFAULT_TARGET_FPS);
		/*
		 * It's essential to update the status here.
		 * This ensures timely reclamation of buffer information.
		 */
		thr->latest_use_ts = ts;
		thr->scroll_status = start;
		//thr->dpt_policy_enable = 1;
		thr->dep_self_ctrl = 1;
		thr->critical_basic_cap = 0;
		critical_basic_cap = get_sbe_critical_basic_cap();

		if (start) {
			if (critical_basic_cap > 0)
				thr->critical_basic_cap = clamp(critical_basic_cap, 0, 100);
		} else
			sbe_reset_frame_cap(thr);

		if (test_bit(SBE_PAGE_FLUTTER, &mask)
				|| test_bit(SBE_PAGE_WEBVIEW, &mask))
			thr->dy_compute_rescue = 0;
		else
			thr->dy_compute_rescue = 1;

		sbe_trace("[SBE] pid:%d, bufid:%llu, byPassWebFut:0, page_type:0, byPass_affinity:0",
					thr->pid, thr->buffer_id);

		if (test_bit(SBE_PAGE_FLUTTER, &mask)
			|| test_bit(SBE_PAGE_WEBVIEW, &mask)
			|| test_bit(SBE_DISABLE_DPT, &mask)
			|| test_bit(SBE_PAGE_MULTI_WINDOW, &mask)) {
			if (get_sbe_force_bypass_dptv2()) {
				sbe_trace("[SBE] pid:%d, bufid:%llu forceByPassDpt",
					thr->pid, thr->buffer_id);
			} else {
				thr->dpt_policy_enable = 0;
				sbe_trace("[SBE] pid:%d, bufid:%llu, page_type:%d",
					thr->pid, thr->buffer_id, mask);
			}
		}

		if (thr->dpt_policy_force_disable) {
			sbe_systrace_c(thr->pid, thr->buffer_id, 1, "[ux]affinity_force_off_dpt");
			sbe_systrace_c(thr->pid, thr->buffer_id, thr->affinity_task_mask_cnt, "[ux]dpt_affinity_cnt");
			sbe_set_dptv2_policy(thr, 0);
		} else {
			if (!thr->dpt_policy_enable)
				sbe_set_dptv2_policy(thr, 0);
			else
				sbe_set_dptv2_policy(thr, start);
		}

		if (thr->core_ctl_ignore_vip_task)
			sbe_core_ctl_ignore_vip_task(thr, start);
		else
			sbe_core_ctl_ignore_vip_task(thr, 0);

		if (start) {
			type = test_bit(SBE_MOVEING, &mask) ? SBE_MOVEING :
						(test_bit(SBE_FLING, &mask) ? SBE_FLING : 0);
			if (get_ux_list_length(&thr->scroll_list) >= 1) {
				last = list_first_entry(&thr->scroll_list, struct ux_scroll_info, queue_list);
				if (last) {
					if (last->type == SBE_MOVEING && type == SBE_FLING)
						add_new_scrolling = 0;

					if (add_new_scrolling && !last->end_ts) {
						last->end_ts = ts; // last scroll endtime is current ts
						sbe_ux_scrolling_end(thr);
					} else if (!add_new_scrolling) {
						last->type = SBE_FLING;
					}
				}
			}
			if (add_new_scrolling) {
				//add new scroll_info into sbe_render_info struct
				sbe_ux_scrolling_start(type, ts, thr);
			}
			memset(&xgf_attr_iter, 0, sizeof(struct xgf_policy_cmd));
			xgf_attr_iter.mode = 1;
			xgf_attr_iter.pid = scroll_policy_info.final_pid_arr[i];
			xgf_attr_iter.bufid = scroll_policy_info.final_bufID_arr[i];
			xgf_attr_iter.calculate_dep_enable = 1;
			xgf_attr_iter.ts = sbe_get_time();
			fpsgo_other2xgf_set_attr(1, &xgf_attr_iter);

			if (get_sbe_extra_sub_en_deque_enable()) {
				memset(&xgf_attr_iter, 0, sizeof(struct xgf_policy_cmd));
				xgf_attr_iter.mode = 1;
				xgf_attr_iter.pid = scroll_policy_info.final_pid_arr[i];
				xgf_attr_iter.bufid = SBE_HWUI_BUFFER_ID;
				xgf_attr_iter.ts = sbe_get_time();
				xgf_attr_iter.xgf_extra_sub = 1;
				sbe_trace("xgf_extra_sub enable: pid = %d, buffer_id: 0x%llx ",
					xgf_attr_iter.pid, xgf_attr_iter.bufid);
				fpsgo_other2xgf_set_attr(1, &xgf_attr_iter);
			}

			memset(&attr_iter, 0, sizeof(struct fpsgo_boost_attr));
			attr_iter.aa_enable_by_pid = 1;
			attr_iter.rescue_enable_by_pid = 1;
			attr_iter.gcc_enable_by_pid = 0;
			attr_iter.qr_enable_by_pid = 0;
			set_fpsgo_attr(1, scroll_policy_info.final_pid_arr[i], 1, &attr_iter);

			sbe_notify_ux_jank_detection(true, tgid, scroll_policy_info.final_pid_arr[i], mask,
				thr, scroll_policy_info.final_bufID_arr[i]);
		} else {
			//update scroll_info when scroll end
			if (get_ux_list_length(&thr->scroll_list) > 0) {
				last = list_first_entry(&thr->scroll_list, struct ux_scroll_info, queue_list);
				if (last) {
					last->end_ts = ts;
					if (last->end_ts > last->start_ts)
						last->dur_ts = last->end_ts - last->start_ts;
					sbe_ux_scrolling_end(thr);
				}
			}

			memset(&xgf_attr_iter, 0, sizeof(struct xgf_policy_cmd));
			xgf_attr_iter.mode = 1;
			xgf_attr_iter.pid = scroll_policy_info.final_pid_arr[i];
			xgf_attr_iter.bufid = scroll_policy_info.final_bufID_arr[i];
			xgf_attr_iter.ts = sbe_get_time();
			fpsgo_other2xgf_set_attr(0, &xgf_attr_iter);

			if (get_sbe_extra_sub_en_deque_enable()) {
				memset(&xgf_attr_iter, 0, sizeof(struct xgf_policy_cmd));
				xgf_attr_iter.mode = 1;
				xgf_attr_iter.pid = scroll_policy_info.final_pid_arr[i];
				xgf_attr_iter.bufid = SBE_HWUI_BUFFER_ID;
				xgf_attr_iter.ts = sbe_get_time();
				xgf_attr_iter.xgf_extra_sub = 0;
				sbe_trace("xgf_extra_sub disable: pid = %d, buffer_id: 0x%llx ",
					xgf_attr_iter.pid, xgf_attr_iter.bufid);
				fpsgo_other2xgf_set_attr(0, &xgf_attr_iter);
			}

			memset(&attr_iter, 0, sizeof(struct fpsgo_boost_attr));
			attr_iter.aa_enable_by_pid = 1;
			attr_iter.rescue_enable_by_pid = 1;
			attr_iter.gcc_enable_by_pid = 0;
			attr_iter.qr_enable_by_pid = 0;
			set_fpsgo_attr(1, scroll_policy_info.final_pid_arr[i], 0, &attr_iter);

			sbe_notify_ux_jank_detection(false, tgid, scroll_policy_info.final_pid_arr[i], mask,
				thr, scroll_policy_info.final_bufID_arr[i]);
		}
		sbe_put_tree_lock(__func__);
	}

	if (final_pid_arr_idx > 0) {
		local_specific_tid_num = sbe_split_task_name(tgid,
				specific_name, num, scroll_policy_info.local_specific_tid_arr, __func__);
		for (i = 0; i < local_specific_tid_num; i++) {
			scroll_policy_info.local_specific_action_arr[i].pid =
				scroll_policy_info.local_specific_tid_arr[i];
			scroll_policy_info.local_specific_action_arr[i].action = 0;  // XGF_ADD_DEP
		}
		/*
		 * TODO: This might be redundant?
		 * Manually adding render thread to dep list.
		 * Consider removing this in the future.
		 * Evaluate the impact of removal and optimize
		 * only if it doesn't cause any issues.
		 */
		for (i = 0; i < final_pid_arr_idx; i++) {
			if (start && local_specific_tid_num > 0) {
				fpsgo_other2xgf_set_critical_tasks(scroll_policy_info.final_pid_arr[i],
					scroll_policy_info.final_bufID_arr[i],
					scroll_policy_info.local_specific_action_arr,
					local_specific_tid_num, 1);
				sbe_trace("[SBE]: %s: pid %d, bufID %llu, start %d, local_s_tid_num %d",
					__func__,
					scroll_policy_info.final_pid_arr[i],
					scroll_policy_info.final_bufID_arr[i],
					start, local_specific_tid_num);

				sbe_get_tree_lock(__func__);
				thr = sbe_get_render_info(scroll_policy_info.final_pid_arr[i],
					scroll_policy_info.final_bufID_arr[i], 1);
				if (thr) {
					thr->latest_use_ts = ts;
					thr->scroll_status = start;
					thr->fpsgo_critical_flag = 1;
				}
				sbe_put_tree_lock(__func__);
			}
		}
	}

	if (!start) {
		sbe_get_tree_lock(__func__);
		sbe_forece_reset_fpsgo_critical_tasks();
		sbe_put_tree_lock(__func__);
	}

	return ret;
}

static int sbe_set_scroll_policy(int tgid, char *name, unsigned long mask,
				unsigned long long ts, int start,
				char *specific_name, int num)
{
	int ret = 0;

	sbe_trace("[SBE] scroll policy: tgid:%d, name:%s, mask:%lx, ts:%llu, start:%d, s_name:%s, num:%d",
		tgid, name, mask, ts, start, specific_name, num);

	if (IS_BIT_SET(mask, SBE_RUNNING_CHECK)) {
		/*
		 * The running check only performs update actions and doesn't involve any other behaviors.
		 * After updating the loading information, it should exit immediately.
		 */
		ret = sbe_do_running_check(start, tgid, specific_name, num);
		sbe_trace("[SBE]: Do running check ret %d", ret);
		goto out;
	}

	sbe_register_jank_cb(mask);

	if (IS_BIT_SET(mask, SBE_SCROLLING)) {
		/*
		 * The mask may include multiple scenarios simultaneously, so we need to check all cases.
		 * Each case is processed independently without early returns.
		 */
		ret = sbe_do_hwui_scrolling_policy(tgid, start, specific_name, num, mask);
		sbe_trace("[SBE] %s receive scrolling %d, ret = %d", __func__, start, ret);
	}

	if (IS_BIT_SET(mask, SBE_CPU_CONTROL)) {
		ret = sbe_do_webview_notify_fpsgo_ctrl(tgid, name, start, specific_name, num, ts);
		sbe_trace("[SBE] %s webview notify fpsgo ctrl start: %d, ret = %d", __func__, start, ret);
	}

	if (IS_BIT_SET(mask, SBE_DISPLAY_TARGET_FPS)) {
		ret = sbe_do_display_target_fps(tgid, name, start, ts);
		sbe_trace("[SBE] %s do display target fps start: %d, ret = %d", __func__, start, ret);
	}

	if (IS_BIT_SET(mask, SBE_CLEAR_SCROLLING_INFO)) {
		ret = sbe_do_clear_scrolling_info(tgid, name, ts);
		sbe_trace("[SBE] %s do clear scrolling info: %d, ret = %d", __func__, tgid, ret);
	}

	if (IS_BIT_SET(mask, SBE_HWUI)) {
		ret = sbe_do_hwui_scrolling_status_policy(tgid, name, mask, ts, start, specific_name, num);
		sbe_trace("[SBE] %s hwui scrolling status policy: %d, mask:%lx, start:%d, s_name:%s, num:%d, ret:%d",
			__func__, tgid, mask, start, specific_name, num, ret);
	}

out:
	return ret;
}

static void sbe_queue_work(struct SBE_NOTIFIER_PUSH_TAG *vpPush)
{
	mutex_lock(&notifier_wq_lock);
	list_add_tail(&vpPush->queue_list, &head);
	condition_notifier_wq = 1;
	mutex_unlock(&notifier_wq_lock);

	wake_up_interruptible(&notifier_wq_queue);
}

static void sbe_notifier_wq_cb(void)
{
	struct SBE_NOTIFIER_PUSH_TAG *vpPush = NULL;

	wait_event_interruptible(notifier_wq_queue, condition_notifier_wq);

	mutex_lock(&notifier_wq_lock);
	if (!list_empty(&head)) {
		vpPush = list_first_entry(&head, struct SBE_NOTIFIER_PUSH_TAG, queue_list);
		list_del(&vpPush->queue_list);
		if (list_empty(&head))
			condition_notifier_wq = 0;
		mutex_unlock(&notifier_wq_lock);
	} else {
		condition_notifier_wq = 0;
		mutex_unlock(&notifier_wq_lock);
		return;
	}

	switch (vpPush->ePushType) {
	case SBE_NOTIFIER_DISPLAY_RATE:
		sbe_notifier_wq_cb_display_rate(vpPush->display_rate);
		break;
	case SBE_NOTIFIER_RESCUE:
		sbe_notifier_wq_cb_rescue(vpPush->pid, vpPush->enable, vpPush->enhance,
			vpPush->rescue_type, vpPush->rescue_target, vpPush->frameID);
		break;
	case SBE_NOTIFIER_HWUI_FRAME_HINT:
		sbe_notifier_wq_cb_hwui_frame_hint(vpPush->start,
				vpPush->pid, vpPush->frameID,
				vpPush->cur_ts, vpPush->identifier,
				vpPush->mode, vpPush->specific_name, vpPush->num, vpPush->frame_flags);
		sbe_check_restart_recycle_hrt();
		break;
	case SBE_NOTIFIER_WEBVIEW_POLICY:
		sbe_set_scroll_policy(vpPush->pid,
				vpPush->name, vpPush->mask,
				vpPush->cur_ts, vpPush->start,
				vpPush->specific_name, vpPush->num);
		sbe_check_restart_recycle_hrt();
		break;
	case SBE_NOTIFIER_SET_SBB:
		sbe_set_sbb(vpPush->pid, vpPush->start, vpPush->mode);
		break;
	case SBE_NOTIFIER_FPSGO_CALLBACK_INFO:
		sbe_get_fpsgo_info(vpPush->tgid, vpPush->pid, vpPush->enhance,
				vpPush->mask, vpPush->rescue_type, vpPush->dep_arr);
		break;

	default:
		break;
	}

	kfree(vpPush);
}

int sbe_notify_hwui_frame_hint(int start,
	int pid, int frameID, unsigned long long id,
	int dep_mode, char *dep_name, int dep_num, long long frame_flags)
{
	int ret;
	unsigned long long cur_ts;
	struct SBE_NOTIFIER_PUSH_TAG *vpPush;

	vpPush = kzalloc(sizeof(struct SBE_NOTIFIER_PUSH_TAG), GFP_KERNEL);
	if (!vpPush)
		return -ENOMEM;

	if (!sbe_ktsk) {
		kfree(vpPush);
		return -ENOMEM;
	}

	cur_ts = sbe_get_time();

	vpPush->ePushType = SBE_NOTIFIER_HWUI_FRAME_HINT;
	vpPush->pid = pid;
	vpPush->cur_ts = cur_ts;
	vpPush->start = start;
	vpPush->frameID = frameID;
	vpPush->frame_flags = frame_flags;
	// SBE UX: bufid magic number.
	vpPush->identifier = SBE_HWUI_BUFFER_ID;
	vpPush->mode = dep_mode;
	if (dep_mode && dep_num > 0) {
		memcpy(vpPush->specific_name, dep_name, 1000);
		vpPush->num = dep_num;
	} else
		vpPush->num = 0;

	sbe_queue_work(vpPush);

	ret = sbe_get_perf();

	return ret;
}

int sbe_notify_webview_policy(int pid, char *name, unsigned long mask,
	int start, char *specific_name, int num)
{
	unsigned long long cur_ts;
	struct SBE_NOTIFIER_PUSH_TAG *vpPush;

	vpPush = kzalloc(sizeof(struct SBE_NOTIFIER_PUSH_TAG), GFP_KERNEL);
	if (!vpPush)
		return -ENOMEM;

	if (!sbe_ktsk) {
		kfree(vpPush);
		return -ENOMEM;
	}

	if (test_bit(SBE_RUNNING_QUERY, &mask)) {
		kfree(vpPush);
		sbe_query_is_running = sbe_query_spid_loading();
		return sbe_query_is_running ? 10001 : 0;
	}

	cur_ts = sbe_get_time();

	vpPush->ePushType = SBE_NOTIFIER_WEBVIEW_POLICY;
	vpPush->pid = pid;
	vpPush->cur_ts = cur_ts;
	vpPush->mask = mask;
	vpPush->start = start;
	memcpy(vpPush->name, name, 16);
	memcpy(vpPush->specific_name, specific_name, 1000);
	vpPush->num = num;

	sbe_queue_work(vpPush);

	return 0;
}

void sbe_notify_rescue(int pid, int start, int enhance, int rescue_type,
	unsigned long long rescue_target, unsigned long long frameID)
{
	struct SBE_NOTIFIER_PUSH_TAG *vpPush;

	vpPush = kzalloc(sizeof(struct SBE_NOTIFIER_PUSH_TAG), GFP_KERNEL);
	if (!vpPush)
		return;

	if (!sbe_ktsk) {
		kfree(vpPush);
		return;
	}

	vpPush->ePushType = SBE_NOTIFIER_RESCUE;
	vpPush->pid = pid;
	vpPush->enable = start;
	vpPush->enhance = enhance;
	vpPush->rescue_type = rescue_type;
	vpPush->rescue_target = rescue_target;
	vpPush->frameID = frameID;

	sbe_queue_work(vpPush);
}

void sbe_consistency_policy(int enabled, int pid, int uclamp_min, int uclamp_max)
{
	sbe_trace("Consistency_policy, pid=%d enable=%d uclamp_min=%d uclamp_max=%d",
			pid, enabled, uclamp_min, uclamp_max);
#if IS_ENABLED(CONFIG_MTK_SCHEDULER) && IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
	if (vip_engine_set_vip_ctrl_node_cs &&
			vip_engine_unset_vip_ctrl_node_cs) {
		if (enabled == 1) {
			// turn_on_tgid_vip();
			// set_tgid_vip(pid);
			vip_engine_set_vip_ctrl_node_cs(pid, VVIP, 12);
		} else {
			vip_engine_unset_vip_ctrl_node_cs(pid, VVIP);
			// unset_tgid_vip(pid);
			// turn_off_tgid_vip();
		}
	}
#endif
	if (task_turbo_do_enable_binder_uclamp_inheritance &&
			task_turbo_do_set_binder_uclamp_param &&
			task_turbo_do_unset_binder_uclamp_param) {
		if(enabled == 1) {
			task_turbo_do_enable_binder_uclamp_inheritance(enabled);
			task_turbo_do_set_binder_uclamp_param(pid, uclamp_max, uclamp_min);
		} else {
			task_turbo_do_unset_binder_uclamp_param(pid);
		}
	}
}

int sbe_notify_set_sbb(int pid, int set, int active_ratio)
{
	struct SBE_NOTIFIER_PUSH_TAG *vpPush;

	vpPush = kzalloc(sizeof(struct SBE_NOTIFIER_PUSH_TAG), GFP_KERNEL);
	if (!vpPush)
		return -ENOMEM;

	if (!sbe_ktsk) {
		kfree(vpPush);
		return -ENOMEM;
	}

	vpPush->ePushType = SBE_NOTIFIER_SET_SBB;
	vpPush->pid = pid;
	vpPush->start = set;
	vpPush->mode = active_ratio;

	sbe_queue_work(vpPush);

	return 0;
}

void sbe_receive_display_rate(unsigned int fps_limit)
{
	unsigned int vTmp = TARGET_UNLIMITED_FPS;
	struct SBE_NOTIFIER_PUSH_TAG *vpPush;

	if (fps_limit > 0 && fps_limit <= TARGET_UNLIMITED_FPS)
		vTmp = fps_limit;

	vpPush = kzalloc(sizeof(struct SBE_NOTIFIER_PUSH_TAG), GFP_KERNEL);
	if (!vpPush)
		return;

	if (!sbe_ktsk) {
		kfree(vpPush);
		return;
	}

	vpPush->ePushType = SBE_NOTIFIER_DISPLAY_RATE;
	vpPush->display_rate = vTmp;

	sbe_queue_work(vpPush);
}

void sbe_notify_update_fpsgo_jerk_boost_info(int tgid, int pid, int blc, unsigned long mask,
		int jerk_boost_flag, struct task_info *dep_arr_fpsgo)
{
	struct SBE_NOTIFIER_PUSH_TAG *vpPush;

	vpPush = kzalloc(sizeof(struct SBE_NOTIFIER_PUSH_TAG), GFP_KERNEL);
	if (!vpPush)
		return;

	if (!sbe_ktsk) {
		kfree(vpPush);
		return;
	}

	sbe_get_tree_lock(__func__);
	if (dep_arr_fpsgo) {
		memset(g_dep_arr, 0, sizeof(struct task_info) * FPSGO_MAX_TASK_NUM);
		memcpy(g_dep_arr, dep_arr_fpsgo, sizeof(struct task_info) * FPSGO_MAX_TASK_NUM);
	}

	vpPush->ePushType = SBE_NOTIFIER_FPSGO_CALLBACK_INFO;
	vpPush->tgid = tgid;
	vpPush->pid = pid;
	vpPush->mask = mask;
	vpPush->enhance = blc;
	vpPush->rescue_type = jerk_boost_flag;
	if (dep_arr_fpsgo)
		vpPush->dep_arr = g_dep_arr;

	sbe_queue_work(vpPush);
	sbe_put_tree_lock(__func__);
}

int sbe_get_kthread_tid(void)
{
	return sbe_ktsk ? sbe_ktsk->pid : 0;
}

static int sbe_kthread(void *arg)
{
	while (!kthread_should_stop())
		sbe_notifier_wq_cb();

	return 0;
}

static int __init sbe_init(void)
{
	hrtimer_init(&sbe_recycle_hrt, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	sbe_recycle_hrt.function = &sbe_prepare_do_recycle;
	hrtimer_start(&sbe_recycle_hrt, ktime_set(0, NSEC_PER_SEC), HRTIMER_MODE_REL);

	sbe_ktsk = kthread_create(sbe_kthread, NULL, "sbe_kthread");
	if (sbe_ktsk == NULL)
		return -EFAULT;
	wake_up_process(sbe_ktsk);

	sbe_notify_rescue_fp = sbe_notify_rescue;
	sbe_notify_hwui_frame_hint_fp = sbe_notify_hwui_frame_hint;
	sbe_notify_webview_policy_fp = sbe_notify_webview_policy;
	sbe_consistency_policy_fp = sbe_consistency_policy;
	sbe_set_sbb_fp = sbe_notify_set_sbb;
#if IS_ENABLED(CONFIG_DEVICE_MODULES_DRM_MEDIATEK)
	drm_register_fps_chg_callback(sbe_receive_display_rate);
#endif

	sbe_sysfs_init();
	sbe_base_init();
	sbe_cpu_ctrl_init();

	return 0;
}

static void __exit sbe_exit(void)
{
	if (sbe_ktsk)
		kthread_stop(sbe_ktsk);

#if IS_ENABLED(CONFIG_DEVICE_MODULES_DRM_MEDIATEK)
	drm_unregister_fps_chg_callback(sbe_receive_display_rate);
#endif

	sbe_cpu_ctrl_exit();
	sbe_base_exit();
	sbe_sysfs_exit();
}

module_init(sbe_init);
module_exit(sbe_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek Scenario Boost Engine");
MODULE_AUTHOR("MediaTek Inc.");
