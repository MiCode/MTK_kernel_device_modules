// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/rbtree.h>
#include <linux/atomic.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/cpufreq.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/math64.h>
#include <linux/math.h>
#include <linux/cpufreq.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include <linux/cgroup-defs.h>
#include <linux/sched/cputime.h>
#include <linux/atomic.h>
#include <sched/sched.h>

#include "eas/grp_awr.h"
#include "eas/group.h"
#include "eas/eas_plus.h"
#include "eas/vip.h"
#include "sugov/cpufreq.h"
#include "fpsgo_frame_info.h"
#include "sbe_base.h"
#include "sbe_cpu_ctrl.h"
#include "sbe_usedext.h"
#include "sbe_sysfs.h"
#include "core_ctl.h"

#define NSEC_PER_HUSEC 100000
#define SBE_RESCUE_MODE_UNTIL_QUEUE_END 2
#define RESCUE_MAX_MONITOR_DROP_ARR_SIZE 10
#define UX_TARGET_DEFAULT_FPS 60
#define GAS_ENABLE_FPS 70
#define SBE_FRAME_CAP_THREASHOLD_P 90
#define SBE_FRAME_CAP_THREASHOLD 30
#define SBE_FRAME_CAP_THREASHOLD_L 18
#define SBE_BUFFER_FILTER_DROP_THREASHOLD 2
#define SBE_BUFFER_FILTER_THREASHOLD 4
#define SBE_RESCUE_MORE_THREASHOLD 5
#define SBE_RESUCE_MODE_END 0
#define SBE_RESUCE_MODE_START 1
#define SBE_RESUCE_MODE_TO_QUEUE_END 2
#define SBE_RESUCE_MODE_UPDATE_RESCUE_STRENGTH 3
#define UTIL_EST_RESET_VALUE 0

static DEFINE_MUTEX(sbe_rescue_lock);

static struct kmem_cache *frame_info_cachep __ro_after_init;
static struct kmem_cache *ux_scroll_info_cachep __ro_after_init;
static struct kmem_cache *hwui_frame_info_cachep __ro_after_init;

static int sbe_notify_fpsgo_vir_boost;
static atomic_t sbe_notify_fpsgo_vir_boost_status = ATOMIC_INIT(0);
static int sbe_rescue_enable;
static int sbe_enhance_f;
static int sbe_dy_max_enhance;
static int sbe_dy_enhance_margin;
static int sbe_dy_rescue_enable;
static int sbe_dy_frame_threshold;
static int scroll_cnt;
static int global_ux_blc;
static int global_ux_max_pid;
static int set_deplist_vip;
static int set_deplist_affinity;
static int set_deplist_ls;
static int ux_general_policy;
static int ux_general_policy_type;
static int ux_general_policy_dpt_setwl;
static int gas_threshold;
static int gas_threshold_for_low_TLP;
static int gas_threshold_for_high_TLP;
static int global_sbe_dy_enhance;
static int global_sbe_dy_enhance_max_pid;
static int sbe_critical_basic_cap;
static int sbe_ai_ctrl_enabled;
static int sbe_uclamp_margin;
static int sbe_runnable_util_est_disable;
static int sbe_extra_sub_en_deque_enable;
static int sbe_extra_sub_deque_margin_time;
static int sbe_dptv2_enable;
static int sbe_dptv2_status;
static int sbe_force_bypass_dptv2;
static int sbe_loading_threashold_l;
static int sbe_loading_threashold_m;
static int sbe_loading_threashold_h;
static int sbe_affinity_task;
static int sbe_affinity_task_min_cap;
static int sbe_affinity_task_low_threshold_cap;
static int sbe_ignore_vip_task_enable;
static int sbe_ignore_vip_task_status;
/*For AI jank detection*/
static int ai_rescuing_frame_id;
static int registered;
static int curr_pid;
static unsigned long long curr_idf;


atomic_t g_web_or_flutter_tgid = ATOMIC_INIT(0);
struct task_info g_dep_arr_last[FPSGO_MAX_TASK_NUM];

typedef void (*heavy_fp)(int jank, int tgid, int pid, unsigned long long frameid);
int (*register_jank_ux_callback_fp)(heavy_fp cb);
EXPORT_SYMBOL(register_jank_ux_callback_fp);
int (*unregister_jank_ux_callback_fp)(heavy_fp cb);
EXPORT_SYMBOL(unregister_jank_ux_callback_fp);
void (*sbe_frame_hint_fp)(int frame_start, int perf_index, int capacity_area,
								int buffer_count, unsigned long long frame_id);
EXPORT_SYMBOL(sbe_frame_hint_fp);

void (*enable_ux_jank_detection_fp)(bool enable, const char *info, int tgid, int pid);
EXPORT_SYMBOL(enable_ux_jank_detection_fp);

void (*vip_engine_set_vip_ctrl_node_sbe)(int pid, int vip_prio, unsigned int throttle_time);
EXPORT_SYMBOL_GPL(vip_engine_set_vip_ctrl_node_sbe);
void (*vip_engine_unset_vip_ctrl_node_sbe)(int pid, int vip_prio);
EXPORT_SYMBOL_GPL(vip_engine_unset_vip_ctrl_node_sbe);

#if IS_ENABLED(CONFIG_ARM64)
#define SMART_LAUNCH_BOOST_SUPPORT_CLUSTER_NUM 3
static int smart_launch_off_on;
static int cluster_num;
static int nr_freq_opp_cnt;
struct smart_launch_capacity_info {
	unsigned int *capacity;
	int first_cpu_id;
	int num_opp;
};
struct smart_launch_capacity_info *capacity_info;
#endif

module_param(sbe_rescue_enable, int, 0644);
module_param(sbe_enhance_f, int, 0644);
module_param(sbe_dy_frame_threshold, int, 0644);
module_param(sbe_dy_rescue_enable, int, 0644);
module_param(sbe_dy_max_enhance, int, 0644);
module_param(sbe_dy_enhance_margin, int, 0644);
module_param(scroll_cnt, int, 0644);
module_param(set_deplist_vip, int, 0644);
module_param(set_deplist_affinity, int, 0644);
module_param(set_deplist_ls, int, 0644);
module_param(ux_general_policy, int, 0644);
module_param(ux_general_policy_type, int, 0644);
module_param(ux_general_policy_dpt_setwl, int, 0644);
module_param(gas_threshold_for_low_TLP, int, 0644);
module_param(gas_threshold_for_high_TLP, int, 0644);
module_param(smart_launch_off_on, int, 0644);
module_param(sbe_critical_basic_cap, int, 0644);
module_param(sbe_ai_ctrl_enabled, int, 0644);
module_param(sbe_uclamp_margin, int, 0644);
module_param(sbe_runnable_util_est_disable, int, 0644);
module_param(sbe_extra_sub_en_deque_enable, int, 0644);
module_param(sbe_extra_sub_deque_margin_time, int, 0644);
module_param(sbe_notify_fpsgo_vir_boost, int, 0644);
module_param(sbe_dptv2_enable, int, 0644);
module_param(sbe_force_bypass_dptv2, int, 0644);
module_param(sbe_loading_threashold_l, int, 0644);
module_param(sbe_loading_threashold_m, int, 0644);
module_param(sbe_loading_threashold_h, int, 0644);
module_param(sbe_affinity_task, int, 0644);
module_param(sbe_affinity_task_min_cap, int, 0644);
module_param(sbe_affinity_task_low_threshold_cap, int, 0644);
module_param(sbe_ignore_vip_task_enable, int, 0644);

static void update_hwui_frame_info(struct sbe_render_info *info,
		struct hwui_frame_info *frame, unsigned long long id,
		unsigned long long start_ts, unsigned long long end_ts,
		unsigned long long rsc_start_ts, unsigned long long rsc_end_ts,
		int rescue_reason);
static struct ux_scroll_info *get_latest_ux_scroll_info(struct sbe_render_info *thr);

/* main function*/
static int nsec_to_100usec(unsigned long long nsec)
{
	unsigned long long husec;

	husec = div64_u64(nsec, (unsigned long long)NSEC_PER_HUSEC);

	return (int)husec;
}

int get_sbe_force_bypass_dptv2(void)
{
	return sbe_force_bypass_dptv2;
}

int get_sbe_critical_basic_cap(void)
{
	return sbe_critical_basic_cap;
}

int get_sbe_extra_sub_en_deque_enable(void)
{
	return sbe_extra_sub_en_deque_enable;
}

unsigned long long get_sbe_extra_sub_deque_margin_time(void)
{
	if (sbe_extra_sub_deque_margin_time < 0 ||
		sbe_extra_sub_deque_margin_time >= SBE_DEFAULT_DEUQUE_MARGIN_MAX_TIME_NS)
		return SBE_DEFAULT_DEUQUE_MARGIN_TIME_NS;

	return sbe_extra_sub_deque_margin_time;
}

int get_sbe_disable_runnable_util_est_status(void)
{
	return sbe_runnable_util_est_disable;
}

int get_ux_general_policy(void)
{
	return ux_general_policy;
}

int sbe_get_perf(void)
{
	return global_ux_blc;
}

void sbe_core_ctl_ignore_vip_task(struct sbe_render_info *thr, int ignore_enable)
{
	if (sbe_ignore_vip_task_enable) {
		if (ignore_enable) {
			core_ctl_consider_VIP(0);
			sbe_ignore_vip_task_status = 1;
		} else {
			core_ctl_consider_VIP(1);
			sbe_ignore_vip_task_status = 0;
		}
		sbe_trace("[SBE] pid:%d, bufid:%llu, ignore_vip_task:%d",
					thr->pid, thr->buffer_id, ignore_enable);
	} else {
		if (sbe_ignore_vip_task_status) {
			core_ctl_consider_VIP(1);
			sbe_ignore_vip_task_status = 0;
		}
		sbe_trace("[SBE] pid:%d, bufid:%llu, ignore_vip_task:%d",
					thr->pid, thr->buffer_id, 0);
	}
}

void sbe_set_dptv2_policy(struct sbe_render_info *thr, int start)
{
	if (!sbe_dptv2_enable) {
		if (sbe_dptv2_status) {
			set_flt_coef_margin_ctrl(0);
			sbe_dptv2_status = 0;
			sbe_trace("%s: force set disable", __func__);
		}
		sbe_systrace_c(thr->pid, thr->buffer_id, 0, "[ux]dpt_policy");
		return;
	}

	if (start) {
		set_flt_coef_margin_ctrl(1);
		sbe_dptv2_status = 1;
		sbe_systrace_c(thr->pid, thr->buffer_id, 1, "[ux]dpt_policy");
		sbe_trace("%s: set enable", __func__);
	} else {
		set_flt_coef_margin_ctrl(0);
		sbe_dptv2_status = 0;
		sbe_systrace_c(thr->pid, thr->buffer_id, 0, "[ux]dpt_policy");
		sbe_trace("%s: set disable", __func__);
	}
}

int sbe_set_sbb(int pid, int set, int active_ratio)
{
	if (set)
		set_sbb(SBB_TASK, pid, true);
	else
		set_sbb(SBB_TASK, pid, false);

	if (active_ratio > 0 && active_ratio <= 100)
		set_sbb_active_ratio(active_ratio);

	return 0;
}

void fbt_ux_set_perf(int cur_pid, int cur_blc)
{
	global_ux_blc = cur_blc;
	global_ux_max_pid = cur_pid;
}

void sbe_set_global_sbe_dy_enhance(int cur_pid, int cur_dy_enhance)
{
	global_sbe_dy_enhance = cur_dy_enhance;
	global_sbe_dy_enhance_max_pid = cur_pid;
}

/**
 * sbe_notify_fpsgo_do_virtual_boost - Control virtual boost for FPSGO
 * @enable: Enable/disable virtual boost
 * @tgid: Task group ID
 * @render_tid: Render thread ID
 *
 * This function manages the virtual boost state for FPSGO performance optimization.
 * It handles the creation and cleanup of virtual boost components based on the
 * enable flag and current system state.
 *
 * Return: void
 */
void sbe_notify_fpsgo_do_virtual_boost(int enable, int tgid, int render_tid)
{
	int ret = 0;

	/* Parameter validation */
	if (tgid <= 0 || render_tid <= 0) {
		sbe_systrace_c(tgid, SBE_HWUI_VIRTUAL_BUFFER_ID, -1, "virtual_boost");
		return;
	}

	/* Handle disabled state */
	if (!sbe_notify_fpsgo_vir_boost) {
		if (atomic_read(&sbe_notify_fpsgo_vir_boost_status)) {
			sbe_trace("Cleaning up virtual boost: tgid=%d, render_tid=%d\n",
				  tgid, render_tid);
			fpsgo_other2comp_control_pause(render_tid, SBE_HWUI_VIRTUAL_BUFFER_ID);
			fpsgo_other2comp_user_close(tgid, render_tid, SBE_HWUI_VIRTUAL_BUFFER_ID);
			atomic_set(&sbe_notify_fpsgo_vir_boost_status, 0);
		}
		sbe_systrace_c(tgid, SBE_HWUI_VIRTUAL_BUFFER_ID, 0, "virtual_boost");
		return;
	}

	/* Handle enable/disable requests */
	if (enable) {
		sbe_trace("Creating virtual boost: tgid=%d, render_tid=%d\n",
			  tgid, render_tid);
		ret = fpsgo_other2comp_user_create(tgid, render_tid,
						 SBE_HWUI_VIRTUAL_BUFFER_ID,
						 NULL, 0, 0);
		if (ret) {
			sbe_systrace_c(tgid, SBE_HWUI_VIRTUAL_BUFFER_ID, -2, "virtual_boost");
			return;
		}
		fpsgo_other2comp_control_resume(render_tid, SBE_HWUI_VIRTUAL_BUFFER_ID);
		atomic_set(&sbe_notify_fpsgo_vir_boost_status, 1);
	} else {
		sbe_trace("Disabling virtual boost: tgid=%d, render_tid=%d\n",
			  tgid, render_tid);
		fpsgo_other2comp_control_pause(render_tid, SBE_HWUI_VIRTUAL_BUFFER_ID);
		fpsgo_other2comp_user_close(tgid, render_tid, SBE_HWUI_VIRTUAL_BUFFER_ID);
		atomic_set(&sbe_notify_fpsgo_vir_boost_status, 0);
	}

	sbe_systrace_c(tgid, SBE_HWUI_VIRTUAL_BUFFER_ID, enable, "virtual_boost");
}

static int sbe_set_affinity(int pid, const struct cpumask *in_mask)
{
	struct task_struct *p;
	int retval;

	rcu_read_lock();

	p = find_task_by_vpid(pid);
	if (!p) {
		rcu_read_unlock();
		return -ESRCH;
	}

	/* Prevent p going away */
	get_task_struct(p);
	rcu_read_unlock();

	if (p->flags & PF_NO_SETAFFINITY) {
		retval = -EINVAL;
		goto out_put_task;
	}

	retval = set_cpus_allowed_ptr(p, in_mask);
out_put_task:
	put_task_struct(p);
	return retval;
}

void sbe_set_affinity_on_scrolling(int pid, int r_cpu_mask)
{
	int ret;
	int cpu;
	struct cpumask new_mask;

	cpumask_clear(&new_mask);
	for_each_possible_cpu(cpu) {
		if (r_cpu_mask & (1 << cpu))
			cpumask_set_cpu(cpu, &new_mask);
	}

	ret = sbe_set_affinity(pid, &new_mask);
	if (ret) {
		sbe_systrace_c(pid, 0, ret, "setaffinity fail");
		sbe_systrace_c(pid, 0, 0, "setaffinity fail");
	}
}

void sbe_set_task_ls(int pid, int set, unsigned int prefer_type)
{
#if IS_ENABLED(CONFIG_MTK_SCHEDULER)
	if (set)
		set_task_ls_prefer_cpus(pid, prefer_type);
	else
		unset_task_ls_prefer_cpus(pid);
#endif
}

void sbe_set_deplist_policy(struct sbe_render_info *thr, int policy)
{
	int i, ret;
	char pid_buf[320] = {0};

	if (!thr)
		return;

#if IS_ENABLED(CONFIG_MTK_SCHEDULER) && IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
	for (i = 0; i < thr->dep_num; i++) {
		if (thr->dep_arr[i] <= 0)
			continue;

		char pid_str[10];

		ret = snprintf(pid_str, sizeof(pid_str), "%d,", thr->dep_arr[i]);
		if (ret > 0
				&& strlen(pid_buf) + strlen(pid_str) < sizeof(pid_buf))
			strcat(pid_buf, pid_str);

		if (set_deplist_vip && vip_engine_set_vip_ctrl_node_sbe &&
				vip_engine_unset_vip_ctrl_node_sbe) {
			if (policy == SBE_TASK_DISABLE)
				vip_engine_unset_vip_ctrl_node_sbe(thr->dep_arr[i], WORKER_VIP);
			else if (policy == SBE_TASK_ENABLE)
				vip_engine_set_vip_ctrl_node_sbe(thr->dep_arr[i], WORKER_VIP, 12);
		}
		if (set_deplist_ls)
			sbe_set_task_ls(thr->dep_arr[i], policy, SBE_PREFER_NONE);
	}
#endif

	if (set_deplist_vip) {
		sbe_trace("[ux]set_deplist_vip=%d for %s\n", policy, pid_buf);
		sbe_systrace_c(thr->pid, thr->buffer_id, policy, "[ux]set_vip");
	}
	if (set_deplist_ls) {
		sbe_trace("[ux]set_deplist_ls=%d for %s\n", policy, pid_buf);
		sbe_systrace_c(thr->pid, thr->buffer_id, policy, "[ux]set_ls");
	}
}

void sbe_set_curr_thread_info(int pid, unsigned long long identifier)
{
	curr_pid = pid;
	curr_idf = identifier;
}

void receive_jank_detection(int perf, int tgid, int pid, unsigned long long frameid)
{
	struct sbe_render_info *sbe_rinfo = NULL;

	sbe_get_tree_lock(__func__);
	sbe_rinfo =	sbe_get_render_info(curr_pid, curr_idf, 0);

	if (sbe_rinfo != NULL) {
		sbe_trace("Received curr_pid %d, heavy detection: %d", curr_pid, perf);
		sbe_do_rescue(sbe_rinfo, 1, perf, RESCUE_TYPE_AI_RESCUE, 0, frameid);
	}
	sbe_put_tree_lock(__func__);
}

void sbe_notify_ux_jank_detection(bool enable, int tgid, int pid, unsigned long mask,
			struct sbe_render_info *sbe_thr, unsigned long long buf_id)
{
	char *proc_name = NULL;

	if (!enable_ux_jank_detection_fp || !sbe_ai_ctrl_enabled)
		return;

	proc_name = kcalloc(MAX_PROCESS_NAME_LEN, sizeof(char), GFP_KERNEL);
	if (!proc_name)
		goto out;

	proc_name[15] = '\0';
	sbe_get_proc_name(tgid, proc_name);
	enable_ux_jank_detection_fp(enable, proc_name, tgid, pid);
	sbe_trace("ux jank detection: tgid=%d, pid=%d, proc_name=%s,enable=%d, mask=%llu\n",
		tgid, pid, proc_name, enable, mask);

	if (enable && test_bit(SBE_HWUI, &mask) && sbe_thr != NULL)
		sbe_set_curr_thread_info(pid, buf_id);

	kfree(proc_name);
out:
	return;
}

void sbe_register_jank_cb(unsigned long mask)
{
	if (test_bit(SBE_HWUI, &mask)) {
		if (!sbe_ai_ctrl_enabled) {
			if (unregister_jank_ux_callback_fp && registered == 1) {
				unregister_jank_ux_callback_fp(receive_jank_detection);
				sbe_trace("unregister_jank_ux_callback_fp...");
				registered = 0;
			}
		} else {
			if (register_jank_ux_callback_fp && registered == 0) {
				register_jank_ux_callback_fp(receive_jank_detection);
				sbe_trace("register_jank_ux_callback_fp...");
				registered = 1;
			}
		}
	}

}

static int sbe_cal_perf(long long t_cpu_cur, long long target_time,
	struct sbe_render_info *thread_info, long aa)
{
	unsigned int blc_wt = 0U;
	int pid;
	unsigned long long buffer_id;
	unsigned long long t1, t2, t_Q2Q;
	long aa_n = -1;
	int ret = 0;

	if (!thread_info) {
		pr_debug("ERROR %d\n", __LINE__);
		return 0;
	}

	pid = thread_info->pid;
	buffer_id = thread_info->buffer_id;

	t1 = (unsigned long long)t_cpu_cur;
	t1 = nsec_to_100usec(t1);
	t2 = target_time;
	t2 = nsec_to_100usec(t2);
	t_Q2Q = thread_info->frame_time;
	t_Q2Q = nsec_to_100usec(t_Q2Q);
	aa_n = aa;

	ret = fpsgo_other2fbt_calculate_frame_loading(aa, t1, t_Q2Q, &aa_n);
	if (ret) {
		pr_debug("ERROR %d\n", __LINE__);
		goto out;
	}

	ret = fpsgo_other2fbt_calculate_blc(aa_n, t2, 0, t_Q2Q, 0, &blc_wt);
	if (ret)
		pr_debug("ERROR %d\n", __LINE__);

out:
	sbe_systrace_c(pid, buffer_id, aa_n, "[ux]aa");
	blc_wt = clamp(blc_wt, 1U, 100U);

	return blc_wt;
}

static void sbe_set_dep_affinity(struct sbe_render_info *thr, int r_cpu_mask)
{
	int i;
	struct task_struct *p;
	struct cpumask new_mask;
	int cpu;

	if(!set_deplist_affinity || !thr)
		return;

	cpumask_clear(&new_mask);
	for_each_possible_cpu (cpu) {
		if (r_cpu_mask & (1 << cpu))
			cpumask_set_cpu(cpu, &new_mask);
	}

	for (i = 0; i < thr->dep_num; i++) {
		if (thr->aff_dep_arr[i] <= 0)
			continue;

		rcu_read_lock();
		p = find_task_by_vpid(thr->aff_dep_arr[i]);
		if (likely(p))
			get_task_struct(p);
		rcu_read_unlock();

		if (likely(p)) {
			if ((p->flags & PF_NO_SETAFFINITY) == 0)
				set_cpus_allowed_ptr(p, &new_mask);

			put_task_struct(p);
		}
	}
}

void __sbe_set_per_task_cap(struct sbe_render_info *thr, int min_cap, int max_cap)
{
	int i;
	char temp[7] = {"\0"};
	char *local_dep_str = NULL;
	int ret = 0;
	unsigned int min_uclamp;
	unsigned int max_uclamp;
	unsigned long cur_min;
	unsigned long cur_max;
	struct sched_attr attr = {};
	struct task_struct *p;

	attr.sched_policy = -1;
	attr.sched_flags =
		SCHED_FLAG_KEEP_ALL |
		SCHED_FLAG_UTIL_CLAMP |
		SCHED_FLAG_RESET_ON_FORK;

	if (min_cap == 0 && max_cap == 100) {
		attr.sched_util_min = -1;
		attr.sched_util_max = -1;
	} else {
		max_uclamp = (max_cap << 10) / 100U;
		max_uclamp = clamp(max_uclamp, 1U, 1024U);
		min_uclamp = (min_cap << 10) / 100U;
		min_uclamp = clamp(min_uclamp, 1U, max_uclamp);

		if (sbe_uclamp_margin) {
			attr.sched_util_min = (min_uclamp << 10) / 1280;
			attr.sched_util_max = (max_uclamp << 10) / 1280;
		} else {
			attr.sched_util_min = min_uclamp;
			attr.sched_util_max = max_uclamp;
		}
	}

	local_dep_str = kcalloc(MAX_TASK_NUM + 1, 7 * sizeof(char), GFP_KERNEL);
	if (!local_dep_str)
		goto out;

	for (i = 0; i < thr->dep_num; i++) {
		if (thr->dep_arr[i] <= 0)
			continue;

		rcu_read_lock();
		p = find_task_by_vpid(thr->dep_arr[i]);
		if (likely(p))
			get_task_struct(p);
		rcu_read_unlock();

		if (likely(p)) {
			cur_min = uclamp_eff_value(p, UCLAMP_MIN);
			cur_max = uclamp_eff_value(p, UCLAMP_MAX);
			if (cur_min != attr.sched_util_min || cur_max != attr.sched_util_max) {
				attr.sched_policy = p->policy;

				if (rt_policy(p->policy))
					attr.sched_priority = p->rt_priority;
				else
					attr.sched_priority = 0;

				ret = sched_setattr_nocheck(p, &attr);
			}
			put_task_struct(p);
		}

		if (ret) {
			sbe_systrace_c(thr->dep_arr[i], 0, ret, "uclamp fail");
			sbe_systrace_c(thr->dep_arr[i], 0, 0, "uclamp fail");
		}
		sbe_systrace_c(thr->dep_arr[i], 0, attr.sched_util_min, "min_cap");
		sbe_systrace_c(thr->dep_arr[i], 0, attr.sched_util_max, "max_cap");

		if (strlen(local_dep_str) == 0)
			ret = snprintf(temp, sizeof(temp), "%d", thr->dep_arr[i]);
		else
			ret = snprintf(temp, sizeof(temp), ",%d", thr->dep_arr[i]);

		if (ret > 0 && strlen(local_dep_str) + strlen(temp) < 256)
			strncat(local_dep_str, temp, strlen(temp));
	}

	sbe_trace("[%d] dep-list %s", thr->pid, local_dep_str);

out:
	kfree(local_dep_str);
}

void sbe_set_per_task_cap(struct sbe_render_info *thr)
{
	int set_blc_wt;
	int local_min_cap = 0;
	int local_max_cap = 100;
	int ai_boost = 0;
	bool is_valid_affinity;
	bool is_valid_threshold;

	set_blc_wt = thr->ux_blc_cur + thr->sbe_enhance;

	if (thr->critical_basic_cap > 0)
		set_blc_wt += thr->critical_basic_cap;

	set_blc_wt = clamp(set_blc_wt, 0, 100);

	/* Check affinity task conditions */
	is_valid_affinity = (thr->affinity_task_mask > 0) &&
		(thr->ux_affinity_task_basic_cap > 0) &&
		(thr->ux_affinity_task_basic_cap <= 100);

	is_valid_threshold = (sbe_affinity_task_low_threshold_cap > 0) &&
		(sbe_affinity_task_low_threshold_cap <= 100);

	/* Apply affinity task boost if conditions are met */
	if (is_valid_affinity && is_valid_threshold) {
		if ((set_blc_wt >= 0) && (thr->ux_blc_cur < sbe_affinity_task_low_threshold_cap)) {
			set_blc_wt += thr->ux_affinity_task_basic_cap;
			set_blc_wt = clamp(set_blc_wt, 0, 100);
		}
	}

	if (!sbe_ai_ctrl_enabled) {
		local_min_cap = set_blc_wt;
		if (!set_blc_wt || thr->sbe_enhance > 0)
			local_max_cap = 100;
		else
			local_max_cap = set_blc_wt;
	} else {
		ai_boost = thr->ai_boost > 0 ? thr->ai_boost : set_blc_wt;
		ai_boost = clamp(ai_boost, 0, 100);
		local_min_cap = ai_boost >= set_blc_wt ? thr->ai_boost : set_blc_wt;
		if (!set_blc_wt || (!thr->ai_boost || thr->sbe_enhance > 0))
			local_max_cap = 100;
		else
			local_max_cap = local_min_cap;

		sbe_systrace_c(thr->pid, thr->buffer_id, ai_boost, "[ux_ai]perf_idx");
		sbe_systrace_c(thr->pid, thr->buffer_id, set_blc_wt, "[ux_sbe]perf_idx");
	}

	__sbe_set_per_task_cap(thr, local_min_cap, local_max_cap);
	sbe_systrace_c(thr->pid, thr->buffer_id, local_min_cap, "[ux]perf_idx");
	sbe_systrace_c(thr->pid, thr->buffer_id, local_max_cap, "[ux]perf_idx_max");
}


void sbe_notify_ai_frame_hint(int frame_start, int capacity_area, struct sbe_render_info *thr,
							unsigned long long frameid)
{
	if (!sbe_frame_hint_fp || !thr || !sbe_ai_ctrl_enabled)
		return;

	if (frame_start) {
		thr->ai_boost = 0;
		sbe_trace("[ai_hint] start: %d, sbe_rescued:%d, enhance_f: %d, frameid: %llu",
				frame_start, thr->ux_blc_cur, thr->sbe_dy_enhance_f);
		sbe_frame_hint_fp(1, thr->ux_blc_cur, capacity_area, thr->cur_buffer_count, frameid);
	} else {
		sbe_trace("[ai_hint] start: %d, sbe_rescued:%d, enhance_f: %d, frameid: %llu",
				frame_start, thr->sbe_rescue, thr->sbe_dy_enhance_f);
		sbe_frame_hint_fp(0, 0, capacity_area, thr->cur_buffer_count, frameid);
	}

}

void sbe_do_frame_start(struct sbe_render_info *thr, unsigned long long frameid, unsigned long long ts)
{
	if (!thr)
		return;

	thr->ux_blc_cur = thr->ux_blc_next;

	thr->ux_affinity_task_basic_cap = 0;
	if (thr->affinity_task_mask)
		thr->ux_affinity_task_basic_cap = clamp(sbe_affinity_task_min_cap, 0, 100);

	sbe_set_per_task_cap(thr);

	if (thr->peak_frame_count >= 15 && thr->loading_type != RENDER_LOADING_PEAK) {
		thr->peak_frame_count = 0;
		thr->affinity_task_mask = SBE_PREFER_NONE;
		thr->loading_type = RENDER_LOADING_PEAK;
		sbe_set_dep_affinity(thr, SBE_PREFER_NONE);
		core_ctl_set_min_cpus(1/*Cluster 1*/, 3/*3 core*/,
				3/*UX Scenario*/, 1/*consider UX Scenario*/);
		core_ctl_set_min_cpus(2/*Cluster 1*/, 1/*1 core*/,
				3/*UX Scenario*/, 1/*consider UX Scenario*/);
	}

	sbe_notify_ai_frame_hint(1, -1, thr, frameid);

	if (sbe_dy_rescue_enable && !list_empty(&thr->scroll_list)) {
		struct hwui_frame_info *frame = get_valid_hwui_frame_info_from_pool(thr);

		if (frame) {
			frame->frameID = frameid;
			frame->start_ts = ts;
		}
	}
}

void sbe_reset_frame_cap(struct sbe_render_info *thr)
{
	if (!thr)
		return;

	thr->ux_blc_cur = 0;
	thr->ux_affinity_task_basic_cap = 0;
	__sbe_set_per_task_cap(thr, 0, 100);
	sbe_systrace_c(thr->pid, thr->buffer_id, 0, "[ux]perf_idx");
	sbe_systrace_c(thr->pid, thr->buffer_id, 100, "[ux]perf_idx_max");
}

void sbe_do_frame_end(struct sbe_render_info *thr, unsigned long long frameid,
		unsigned long long start_ts, unsigned long long end_ts)
{
	long long runtime;
	int targettime, targetfps;
	unsigned long long loading = 0;

	if (!thr)
		return;

	runtime = thr->ema_running_time;
	targetfps = thr->target_fps;
	targettime = thr->target_time;

	if (targetfps <= 0)
		targetfps = TARGET_UNLIMITED_FPS;

	if (start_ts == 0)
		goto EXIT;

	thr->frame_time = end_ts - start_ts;

	sbe_systrace_c(thr->pid, thr->buffer_id, targetfps, "[ux]target_fps");
	sbe_systrace_c(thr->pid, thr->buffer_id, targettime, "[ux]target_time");
	sbe_systrace_c(thr->pid, thr->buffer_id, (int)thr->frame_time, "[ux]frame_time");

	if (sbe_dy_rescue_enable) {
		struct hwui_frame_info *new_frame;
		struct hwui_frame_info *frame =
			get_hwui_frame_info_by_frameid(thr, frameid);
		if (frame) {// only insert rescued frame
			if (frame->rescue) {
				update_hwui_frame_info(thr, frame, frameid, 0, end_ts, 0, 0, 0);
				new_frame = insert_hwui_frame_info_from_tmp(thr, frame);
				if (new_frame)
					count_scroll_rescue_info(thr, new_frame);
			}
			reset_hwui_frame_info(frame);
		}
	}

	fpsgo_other2fbt_get_cpu_capacity_area(100/*magic number*/, thr->pid, thr->buffer_id,
		&loading, start_ts, end_ts);
	sbe_systrace_c(thr->pid, thr->buffer_id, (int)loading, "[ux]compute_loading");

	thr->ux_blc_next = sbe_cal_perf(runtime, targettime, thr, loading);

	if (sbe_dy_rescue_enable) {
		thr->frame_count++;
		thr->frame_cap_count += thr->ux_blc_cur;
		thr->frame_ctime_count += nsec_to_100usec(runtime);

		//last is peak, but this frame is finish as well
		if (thr->peak_frame_count > 0
				&& thr->frame_time < thr->target_time)
			thr->peak_frame_count = 0;

		if (thr->ux_blc_cur >= sbe_loading_threashold_h
				&& thr->frame_time > thr->target_time)
			thr->peak_frame_count++;

		if (thr->frame_time > thr->target_time)
			thr->jank_count++;
	}

	if ((thr->pid != global_ux_max_pid && thr->ux_blc_next > global_ux_blc) ||
			thr->pid == global_ux_max_pid)
		fbt_ux_set_perf(thr->pid, thr->ux_blc_next);

	sbe_systrace_c(thr->pid, thr->buffer_id, thr->ux_blc_next, "[ux]ux_blc_next");

EXIT:
	sbe_notify_ai_frame_hint(0, loading, thr, frameid);
	sbe_reset_deplist_task_priority(thr);
	sbe_reset_frame_cap(thr);
}

void sbe_do_frame_err(struct sbe_render_info *thr, int frame_count,
		unsigned long long frameID, unsigned long long ts)
{
	if (!thr)
		return;

	if (frame_count == 0) {
		sbe_set_deplist_policy(thr, SBE_TASK_DISABLE);
		sbe_reset_frame_cap(thr);
	}

	//try end this frame rescue when err
	if (thr->sbe_rescuing_frame_id == frameID && thr->sbe_rescue != 0)
		sbe_do_rescue(thr, 0, 0, 0, 0, frameID);

	if (sbe_dy_rescue_enable) {
		struct hwui_frame_info *frame =
			get_hwui_frame_info_by_frameid(thr, frameID);
		if (frame)
			reset_hwui_frame_info(frame);
	}
}

void sbe_delete_frame_info(struct sbe_render_info *thr, struct ux_frame_info *info)
{
	if (!info)
		return;
	rb_erase(&info->entry, &(thr->ux_frame_info_tree));
	kmem_cache_free(frame_info_cachep, info);
}

int sbe_query_cur_buffer_count(struct sbe_render_info *thr)
{
	int i;
	int tmp_render_num = 0;
	struct render_fw_info *tmp_render_arr = NULL;

	tmp_render_arr = kcalloc(FPSGO_MAX_RENDER_INFO_SIZE, sizeof(struct render_fw_info), GFP_KERNEL);
	if (!tmp_render_arr)
		return -ENOMEM;

	fpsgo_other2comp_get_render_fw_info(0, FPSGO_MAX_RENDER_INFO_SIZE, &tmp_render_num, tmp_render_arr);
	thr->cur_buffer_count = 0;
	for (i = 0; i < tmp_render_num; i++) {
		// TODO: need to consider if same render tid has different buffer id
		if (tmp_render_arr[i].producer_pid == thr->pid) {
			thr->cur_buffer_count = tmp_render_arr[i].buffer_count;
			break;
		}
	}
	kfree(tmp_render_arr);
	if (unlikely(i >= tmp_render_num)) {
		sbe_trace("pid:%d buffer_id:0x%llx not get cur_buffer_count", thr->pid, thr->buffer_id);
		return -EINVAL;
	}

	return 0;
}

void sbe_exec_doframe_end(struct sbe_render_info *thr, unsigned long long frame_id,
				long long frame_flags)
{
	int frame_status = (frame_flags & FRAME_MASK) >> 16;
	int rescue_type = frame_flags & RESCUE_MASK;
	struct hwui_frame_info *frame;
	unsigned long long rescue_time = 0;
	unsigned long long ts = 0;

	if (!thr || !sbe_dy_rescue_enable)
		return;

	if (sbe_query_cur_buffer_count(thr))
		return;

	frame = get_hwui_frame_info_by_frameid(thr, frame_id);
	if (frame) {
		struct ux_scroll_info *scroll_info = NULL;

		frame->rescue_reason = rescue_type;
		frame->frame_status = frame_status;
		//this frame meet buffer count filter
		if (!frame->rescue
				&& ((rescue_type & RESCUE_WAITING_ANIMATION_END) != 0
				|| (rescue_type & RESCUE_TYPE_TRAVERSAL_DYNAMIC) != 0
				|| (rescue_type & RESCUE_TYPE_OI) != 0)
				&& frame->start_ts != 0 && thr->target_time > 0) {
			//thinking rescue time is 1/2 vsync rescue
			rescue_time = frame->start_ts + (thr->target_time >> 1);
			update_hwui_frame_info(thr, frame, frame_id, 0, 0, rescue_time, 0, 0);
		}
		thr->rescue_start_time = 0;

		scroll_info = get_latest_ux_scroll_info(thr);

		if (!scroll_info)
			return;

		scroll_info->last_frame_ID = frame_id;
		ts = sbe_get_time();

		//try enable buffer count filter
		if ((frame->rescue || (rescue_type & RESCUE_TRAVERSAL_OVER_VSYNC) != 0)
				&& thr->buffer_count_filter == 0) {
			//check if rescue more, if rescue, but buffer count is 3 or more
			if (thr->cur_buffer_count > SBE_BUFFER_FILTER_THREASHOLD)
				thr->rescue_more_count++;

			//if resue more count is more than 5 times, enable buffer count filter
			if (thr->rescue_more_count >= SBE_RESCUE_MORE_THREASHOLD)
				thr->buffer_count_filter = SBE_BUFFER_FILTER_DROP_THREASHOLD;

		}

		// buffer_count filter lead to frame drop, disable it
		if (thr->buffer_count_filter > 0
				&& scroll_info->rescue_filter_buffer_time > 0
				&& (ts - frame->start_ts) > scroll_info->rescue_filter_buffer_time) {
			thr->buffer_count_filter--;
			//if drop more, do not check rescue more in this page
			//will be reset to 0 after activity resume
			if (thr->buffer_count_filter == 0) {
				thr->buffer_count_filter = -1;
				thr->rescue_more_count = 0;
			}

			sbe_trace("doframe end -> %d frame:%lld buffer_count:%d, drop more\n",
					thr->pid, frame_id, thr->cur_buffer_count);
		}
		scroll_info->rescue_filter_buffer_time = 0;
	}
}

struct ux_frame_info *sbe_search_and_add_frame_info(struct sbe_render_info *thr,
		unsigned long long frameID, unsigned long long start_ts, int action)
{
	struct rb_node **p = &(thr->ux_frame_info_tree).rb_node;
	struct rb_node *parent = NULL;
	struct ux_frame_info *tmp = NULL;

	while (*p) {
		parent = *p;
		tmp = rb_entry(parent, struct ux_frame_info, entry);
		if (frameID < tmp->frameID)
			p = &(*p)->rb_left;
		else if (frameID > tmp->frameID)
			p = &(*p)->rb_right;
		else
			return tmp;
	}
	if (action == 0)
		return NULL;
	if (frame_info_cachep)
		tmp = kmem_cache_alloc(frame_info_cachep,
			GFP_KERNEL | __GFP_ZERO);
	if (!tmp)
		return NULL;

	tmp->frameID = frameID;
	tmp->start_ts = start_ts;
	rb_link_node(&tmp->entry, parent, p);
	rb_insert_color(&tmp->entry, &(thr->ux_frame_info_tree));

	return tmp;
}

static struct ux_frame_info *sbe_find_earliest_frame_info (struct sbe_render_info *thr)
{
	struct rb_node *cur;
	struct ux_frame_info *tmp = NULL, *ret = NULL;
	unsigned long long min_ts = ULLONG_MAX;

	cur = rb_first(&(thr->ux_frame_info_tree));

	while (cur) {
		tmp = rb_entry(cur, struct ux_frame_info, entry);
		if (tmp->start_ts < min_ts) {
			min_ts = tmp->start_ts;
			ret = tmp;
		}
		cur = rb_next(cur);
	}
	return ret;

}

int sbe_count_frame_info(struct sbe_render_info *thr, int target)
{
	struct rb_node *cur;
	struct ux_frame_info *tmp = NULL;
	int ret = 0;
	int remove = 0;

	if (RB_EMPTY_ROOT(&thr->ux_frame_info_tree) == 1)
		return ret;

	cur = rb_first(&(thr->ux_frame_info_tree));
	while (cur) {
		cur = rb_next(cur);
		ret += 1;
	}
	/* error handling */
	while (ret > target) {
		tmp = sbe_find_earliest_frame_info(thr);
		if (!tmp)
			break;

		sbe_delete_frame_info(thr, tmp);
		ret -= 1;
		remove += 1;
	}
	sbe_systrace_c(thr->pid, thr->buffer_id, remove, "[ux]rb_err_remove");
	return ret;
}

void sbe_ux_reset(struct sbe_render_info *thr)
{
	struct rb_node *cur;
	struct ux_frame_info *tmp = NULL;

	if(!thr)
		return;


	cur = rb_first(&(thr->ux_frame_info_tree));

	while (cur) {
		tmp = rb_entry(cur, struct ux_frame_info, entry);
		rb_erase(&tmp->entry, &(thr->ux_frame_info_tree));
		kmem_cache_free(frame_info_cachep, tmp);
		cur = rb_first(&(thr->ux_frame_info_tree));
	}

}

void sbe_reset_deplist_task_priority(struct sbe_render_info *thr)
{
	if (!thr) {
		pr_debug("%s: NON render info!!!!\n", __func__);
		return;
	}

	sbe_set_deplist_policy(thr, SBE_TASK_DISABLE);

}

#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE) && IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
void sbe_set_group_dvfs(int start)
{
	if (start){
		//disable group dvfs when scroll begin
		group_set_mode(0);
		flt_ctrl_force_set(1);
		set_grp_dvfs_ctrl(0);
	}else{
		//enable group dvfs when scroll end
		flt_ctrl_force_set(0);
		group_set_mode(1);
	}
}

void sbe_set_gas_policy(int start)
{
	if (start){
		group_set_mode(1);
		group_set_threshold(0, gas_threshold);
		set_top_grp_aware(1,0);
		flt_ctrl_force_set(1);
		set_grp_dvfs_ctrl(9);
	}else{
		group_reset_threshold(0);
		set_top_grp_aware(0,0);
	}
	sbe_trace("gas enabled : %d, %d", start, gas_threshold);
}

void update_ux_general_policy(void)
{
	if (ux_general_policy_type == 1) {
		//update threshold for multi window, multi window is hight TLP scenario
		group_set_threshold(0, gas_threshold_for_high_TLP);
	}
}
#endif

// TODO: how to apply ?
void sbe_boost_non_hwui_policy(struct sbe_render_info *thr, int set_vip)
{
	if (!thr) {
		sbe_trace("%s: NON render info!!!!\n",
			__func__);
		return;
	}
	if (set_vip)
		sbe_set_deplist_policy(thr, SBE_TASK_ENABLE);
	else
		sbe_set_deplist_policy(thr, SBE_TASK_DISABLE);
}

void sbe_set_ux_general_policy(int scrolling, unsigned long ux_mask)
{
	int need_gas_policy __maybe_unused = 1;

#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE) && IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
	set_ignore_idle_ctrl(scrolling);
	sbe_set_group_dvfs(scrolling);
#endif

	if (ux_general_policy_type == 0) {
		if (scrolling) {
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
			if (ux_general_policy_dpt_setwl) {
				set_wl_cpu_manual(0);
				sbe_trace("set_wl_cpu_manual: 0");
			}
#endif
			if (change_dpt_support_driver_hook)
				change_dpt_support_driver_hook(1);
			sbe_trace("start runtime power talbe");
		} else {
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
			if (ux_general_policy_dpt_setwl) {
				set_wl_cpu_manual(-1);
				sbe_trace("set_wl_cpu_manual: -1");
			}
#endif
			if (change_dpt_support_driver_hook)
				change_dpt_support_driver_hook(0);
			sbe_trace("end runtime power talbe");
		}
	} else if (ux_general_policy_type == 1) {
		sbe_trace("gas will call, fps = %d", sbe_get_display_rate());
		//If GAS policy, we will set gas_threshold for different ux type
		if (test_bit(SBE_HWUI, &ux_mask)) {
			gas_threshold = gas_threshold_for_low_TLP;
			// If dfrc < limit, dont enable gas policy
			if (sbe_get_display_rate() < GAS_ENABLE_FPS)
				need_gas_policy = 0;
		} else if (test_bit(SBE_NON_HWUI, &ux_mask)) {
			gas_threshold = gas_threshold_for_high_TLP;
		}

#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE) && IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
		if (need_gas_policy)
			sbe_set_gas_policy(scrolling);
#endif
	}
}

void sbe_enable_vip_sitch(int start, int tgid)
{
	if (start)
		set_vip_switch_push();
	else
		unset_vip_switch_push();
}

void set_sbe_thread_vip(int set_vip, int tgid, char *dep_name, int dep_num)
{
#if IS_ENABLED(CONFIG_MTK_SCHEDULER) && IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
	int sbe_pid, i;
	int local_specific_tid_num = 0;
	int *local_specific_tid_arr = NULL;
#endif

	if (!set_deplist_vip)
		return;

#if IS_ENABLED(CONFIG_MTK_SCHEDULER) && IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
	//set sbe process and framepolicy process priority to vip
	sbe_pid = sbe_get_kthread_tid();

	if (dep_num > 0) {
		local_specific_tid_arr = kcalloc(dep_num, sizeof(int), GFP_KERNEL);
		if (local_specific_tid_arr) {
			local_specific_tid_num = sbe_split_task_tid(dep_name, dep_num,
					local_specific_tid_arr, __func__);
		}
	}

	if (sbe_pid <= 0 || local_specific_tid_num <= 0) {
		pr_info("[sbe] not found sbe tids");
		goto out;
	}

	if(vip_engine_set_vip_ctrl_node_sbe && vip_engine_unset_vip_ctrl_node_sbe) {
		if (set_vip) {
			vip_engine_set_vip_ctrl_node_sbe(sbe_pid, 0, 12);
			for (i = 0; i < local_specific_tid_num; i++) {
				if (local_specific_tid_arr[i] > 0)
					vip_engine_set_vip_ctrl_node_sbe(local_specific_tid_arr[i], WORKER_VIP, 12);
			}
			sbe_trace("[SBE] set sbe task as vip %d", sbe_pid);
		} else {
			vip_engine_unset_vip_ctrl_node_sbe(sbe_pid, 0);
			for (i = 0; i < local_specific_tid_num; i++) {
				if (local_specific_tid_arr[i] > 0)
					vip_engine_unset_vip_ctrl_node_sbe(local_specific_tid_arr[i], WORKER_VIP);
			}
			sbe_trace("[SBE] reset sbe task priority");
		}
	}
out:
	kfree(local_specific_tid_arr);
#endif
}

int sbe_calculate_dy_enhance(struct sbe_render_info *thr)
{
	struct ux_scroll_info *scroll_info;
	struct hwui_frame_info *hwui_info;
	int drop = 0;
	int all_rescue_cap_count = 0;
	unsigned long long all_rescue_frame_time_count = 0;
	int all_rescue_frame_count = 0;
	int target_fps = 0;
	unsigned long long target_time = 0LLU;
	int last_enhance = 0;
	int new_enhance = 0;
	int result = -1;
	int scroll_count = 0;
	unsigned long long rescue_target_time = 0LLU;
	int h_score = 0;
	int l_score = 0;
	int p_score = 0;
	int max_avg_cap = 0;
	int loading_scroll_limit = scroll_cnt > 0 ? scroll_cnt >> 1 : 1;

	if (!sbe_dy_rescue_enable || !thr || IS_ERR_OR_NULL(&thr->scroll_list))
		return result;

	target_time = thr->target_time;
	target_fps = thr->target_fps;

	if (target_time <= 0)
		return result;

	rescue_target_time = thr->sbe_rescue_target_time;
	if (rescue_target_time <= 0) {
		pr_debug("[SBE_UX] rescue target is err:%llu\n", rescue_target_time);
		rescue_target_time = target_time >> 1;
	}

	scroll_count = get_ux_list_length(&thr->scroll_list);

	if (scroll_count < loading_scroll_limit) {
		//TODO: scroll not too much to check dynamic rescue, check drop rate and jank rate
		//if scroll refresh rate do not match target fps, start to increase rescue
		return result;
	}

	list_for_each_entry (scroll_info, &thr->scroll_list, queue_list) {
		if (IS_ERR_OR_NULL(&scroll_info->frame_list))
			continue;

		//caclulate rescue info
		if (scroll_info->rescue_count > 0) {
			int rescue_rate = 0;

			all_rescue_cap_count += scroll_info->rescue_cap_count;
			all_rescue_frame_time_count += scroll_info->rescue_frame_time_count;
			all_rescue_frame_count += scroll_info->rescue_frame_count;

			if (scroll_info->frame_count > 0) {
				unsigned long long rescue_f =
						scroll_info->rescue_frame_count;
				unsigned long long percent = 100ULL;

				rescue_rate = (int)div64_u64(rescue_f * percent,
						scroll_info->frame_count);
				if (rescue_rate >= 80 && scroll_info->frame_count > target_fps)
					p_score += 5;
				else if (rescue_rate >= 10)
					h_score += 3;
				else if (rescue_rate >= 7)
					h_score += 2;
				else if (rescue_rate >= 3)
					h_score++;
			}

			if (scroll_info->rescue_frame_count >= 20)
				h_score += 3;
			else if (scroll_info->rescue_frame_count >= 10)
				h_score += 2;
			else if (scroll_info->rescue_frame_count > 6)
				h_score++;

			sbe_trace("SBE_UX caclulate rescue info RescueRate %d rescueFrameCount %llu",
					rescue_rate, scroll_info->rescue_frame_count);
		}

		//caclulate frame info
		if (scroll_info->frame_count > 0) {
			unsigned long long avg_tcpu_time = div64_u64(scroll_info->frame_ctime_count,
						scroll_info->frame_count);
			int avg_cap = (int)div64_u64(scroll_info->frame_cap_count,
						scroll_info->frame_count);
			unsigned long long target_time_100U = nsec_to_100usec(target_time);
			unsigned long long target_time_100U_95_p = div64_u64(target_time_100U * 95ULL, 100);

			sbe_trace("SBE_UX caclulate frame info avg_cap %d avg_tcpu_time %llu",
					avg_cap, avg_tcpu_time);

			if (avg_tcpu_time > target_time_100U)
				h_score++;
			else if (avg_tcpu_time < target_time_100U_95_p)
				l_score++;

			if (avg_cap >= sbe_loading_threashold_m)
				h_score += 4;

			if (avg_cap > max_avg_cap)
				max_avg_cap = avg_cap;

			if (avg_cap >= sbe_loading_threashold_h
					&& avg_tcpu_time > target_time_100U)
				p_score += 5;
		}
	}

	last_enhance = thr->sbe_dy_enhance_f > 0 ? thr->sbe_dy_enhance_f : sbe_enhance_f;

	//check rescue enhance
	if (all_rescue_frame_count > 20 || max_avg_cap >= sbe_loading_threashold_m) {
		if (last_enhance >= 60)
			h_score += 2;
		else if (last_enhance > sbe_loading_threashold_m)
			h_score++;
	}

	if (max_avg_cap > 0 && max_avg_cap < sbe_loading_threashold_l)
		l_score++;

	if (thr->is_webfunctor)
		h_score += 5;

	//update render loading type
	if (p_score >= 5)
		thr->loading_type = RENDER_LOADING_PEAK;
	else if (h_score >= 5)
		thr->loading_type = RENDER_LOADING_HIGH;
	else if (l_score > scroll_cnt) // every time scroll, avg c time less than 80%
		thr->loading_type = RENDER_LOADING_LOW;
	else
		thr->loading_type = RENDER_LOADING_MEDUIM;

	thr->affinity_task_mask = 0;
	thr->ux_affinity_task_basic_cap = 0;
	thr->core_ctl_ignore_vip_task = 0;
	thr->dpt_policy_enable = 0;

	if (thr->loading_type == RENDER_LOADING_LOW) {
		thr->core_ctl_ignore_vip_task = 1;
		thr->dpt_policy_enable = 1;
	} else if (thr->loading_type == RENDER_LOADING_HIGH) {
		if (set_deplist_affinity && sbe_affinity_task > 0) {
			thr->affinity_task_mask = sbe_affinity_task;
			thr->ux_affinity_task_basic_cap = clamp(sbe_affinity_task_min_cap, 0, 100);
		}
	} else if (thr->loading_type == RENDER_LOADING_PEAK)
		thr->affinity_task_mask = SBE_PREFER_NONE;

	sbe_systrace_c(thr->pid, thr->buffer_id, thr->affinity_task_mask, "[ux]affinity_task");
	sbe_trace("[SBE] dy enhance pid: %d, bufid:%llu, af_basic_cap: %d, l_type: %d",
			thr->pid, thr->buffer_id, thr->ux_affinity_task_basic_cap, thr->loading_type);

	// do not compute new rescue
	if (scroll_cnt > 0 && scroll_count < scroll_cnt)
		return result;

	if (thr->dy_compute_rescue
			&& all_rescue_frame_count > 0 && all_rescue_frame_time_count > 0) {
		int limit_min = thr->affinity_task_mask ? 15 : 10;
		int max_enhance = clamp(sbe_dy_max_enhance, 0, 100);
		int new = clamp((int)
			(div64_u64(all_rescue_cap_count, all_rescue_frame_count) + last_enhance), 0, 100);
		unsigned long long avg_frame_time =
				div64_u64(all_rescue_frame_time_count, all_rescue_frame_count);
		//TODO: consider msync case, how to compute new enhance
		new_enhance = (int)(div64_u64(new *avg_frame_time, target_time) - last_enhance);
		new_enhance = clamp(new_enhance, limit_min, max_enhance);
	}

	result = last_enhance;

	if (thr->dy_compute_rescue
			&& new_enhance > 0 && new_enhance != last_enhance) {
		int max_monitor_drop_frame = RESCUE_MAX_MONITOR_DROP_ARR_SIZE - 1;
		long long new_dur;
		long long old_tmp;
		int benifit_f_up = 0;
		int benifit_f_down = 1;
		int threshold = sbe_dy_frame_threshold > 0 ? sbe_dy_frame_threshold : 0;
		int tempScore[RESCUE_MAX_MONITOR_DROP_ARR_SIZE];

		list_for_each_entry (scroll_info, &thr->scroll_list, queue_list) {
			if (IS_ERR_OR_NULL(&scroll_info->frame_list)
					|| scroll_info->rescue_count <= 0) {
				continue;
			}
			drop = 0;
			for (size_t i = 0; i < RESCUE_MAX_MONITOR_DROP_ARR_SIZE; i++)
				tempScore[i] = 0;

			list_for_each_entry (hwui_info, &scroll_info->frame_list, queue_list) {
				if (!hwui_info->rescue)
					continue;

				int before = clamp((hwui_info->perf_idx + last_enhance), 0, 100);
				int after = clamp((hwui_info->perf_idx + new_enhance), 0, 100);

				if (new_enhance > last_enhance && hwui_info->dur_ts <= target_time) {
					tempScore[0] += 1;
					drop = 0;
				} else {
					if (hwui_info->dur_ts < rescue_target_time) {
						old_tmp = before * (long long)hwui_info->dur_ts;
						new_dur = div64_s64(old_tmp, after);
					} else {
						old_tmp = before * ((long long)hwui_info->dur_ts
							- (long long)rescue_target_time);
						new_dur = div64_s64(old_tmp, after) + (long long)rescue_target_time;
					}
					drop = (int)div64_u64(new_dur, target_time);

					if (drop < 0) {
						pr_debug("[SBE_UX] %s old_dur:%llu old_e:%d new_e:%d rescue_t %lld\n",
								__func__, hwui_info->dur_ts, last_enhance,
								new_enhance, rescue_target_time);
						drop = 0;
					}

					if (drop < max_monitor_drop_frame)
						tempScore[drop]++;
					else
						tempScore[max_monitor_drop_frame]++;
				}

				//compute drop diff with before
				if (new_enhance > last_enhance) {
					//increase enhance
					if (hwui_info->drop - drop > 0)
						benifit_f_up += (hwui_info->drop - drop);

					benifit_f_down = 0;
				} else {
					//decrease enhance, default allow, if drop more, disallow
					if (drop - hwui_info->drop > 0) {
						benifit_f_down = 0;
						break;
					}
				}
			}

			if (benifit_f_up > threshold || !benifit_f_down) {
				//already meet condition
				break;
			}
		}

		if (benifit_f_up >= threshold || benifit_f_down) {
			sbe_trace("SBE_UX enhance change from %d to %d",
					last_enhance, new_enhance);
			result = new_enhance;
		}
	}

	return result;
}

void sbe_ux_scrolling_start(int type, unsigned long long start_ts, struct sbe_render_info *thr)
{
	enqueue_ux_scroll_info(type, start_ts, thr);

	// enable cluster 1
	if (thr->affinity_task_mask) {
		if (thr->affinity_task_mask == SBE_PREFER_M
				|| thr->affinity_task_mask == SBE_PREFER_LM) {
			core_ctl_set_min_cpus(1/*Cluster 1*/, 3/*3 core*/,
					3/*UX Scenario*/, 1/*consider UX Scenario*/);
		} else if (thr->affinity_task_mask == SBE_PREFER_BIG) {
			core_ctl_set_min_cpus(2/*Cluster 1*/, 1/*1 core*/,
					3/*UX Scenario*/, 1/*consider UX Scenario*/);
		} else if (thr->affinity_task_mask == SBE_PREFER_NONE) {
			core_ctl_set_min_cpus(1/*Cluster 1*/, 3/*3 core*/,
					3/*UX Scenario*/, 1/*consider UX Scenario*/);
			core_ctl_set_min_cpus(2/*Cluster 1*/, 1/*1 core*/,
					3/*UX Scenario*/, 1/*consider UX Scenario*/);
		}
		//CLEAR BEFORE SET
		sbe_set_dep_affinity(thr, SBE_PREFER_NONE);

		//copy dep set new
		memset(thr->aff_dep_arr, 0, sizeof(int) * MAX_TASK_NUM);
		memcpy(thr->aff_dep_arr, thr->dep_arr, sizeof(int) * MAX_TASK_NUM);

		//Set new dep
		sbe_set_affinity_on_scrolling(thr->tgid, thr->affinity_task_mask);
		sbe_set_affinity_on_scrolling(thr->pid, thr->affinity_task_mask);
		sbe_set_dep_affinity(thr, thr->affinity_task_mask);
	}

}

void sbe_ux_scrolling_end(struct sbe_render_info *thr)
{
	struct ux_scroll_info *scroll_info;

	if (!thr || !sbe_dy_rescue_enable)
		return;

	//disable cluster 1 -> add device config
	if (thr->affinity_task_mask) {
		sbe_set_dep_affinity(thr, SBE_PREFER_NONE);
		sbe_set_affinity_on_scrolling(thr->tgid, SBE_PREFER_NONE);
		sbe_set_affinity_on_scrolling(thr->pid, SBE_PREFER_NONE);
		if (thr->affinity_task_mask == SBE_PREFER_M
				|| thr->affinity_task_mask == SBE_PREFER_LM) {
			core_ctl_set_min_cpus(1, 0, 3, 0);
		} else if (thr->affinity_task_mask == SBE_PREFER_BIG) {
			core_ctl_set_min_cpus(2, 0, 3, 0);
		} else if (thr->affinity_task_mask == SBE_PREFER_NONE) {
			core_ctl_set_min_cpus(1, 0, 3, 0);
			core_ctl_set_min_cpus(2, 0, 3, 0);
		}
	}

	//reset rescue affnity if needed
	scroll_info = get_latest_ux_scroll_info(thr);
	if (scroll_info) {
		scroll_info->frame_count = thr->frame_count;
		scroll_info->frame_cap_count = thr->frame_cap_count;
		scroll_info->frame_ctime_count = thr->frame_ctime_count;
		scroll_info->jank_count = thr->jank_count;
		thr->frame_cap_count = 0;
		thr->frame_ctime_count = 0;
		thr->frame_count = 0;
		thr->jank_count = 0;
		if (scroll_info->rescue_with_perf_mode) {
			sbe_set_affinity_on_scrolling(thr->tgid, SBE_PREFER_NONE);
			sbe_set_affinity_on_scrolling(thr->pid, SBE_PREFER_NONE);
		}
	}

	thr->sbe_dy_enhance_f = sbe_calculate_dy_enhance(thr);
	if (thr->sbe_dy_enhance_f > 0
			&& ((thr->pid != global_sbe_dy_enhance_max_pid
			&& thr->sbe_dy_enhance_f > global_sbe_dy_enhance)
			|| thr->pid == global_sbe_dy_enhance_max_pid))
		sbe_set_global_sbe_dy_enhance(thr->pid, thr->sbe_dy_enhance_f);
	sbe_systrace_c(thr->pid, thr->buffer_id, thr->sbe_dy_enhance_f, "[ux]sbe_dy_enhance_f");
	sbe_systrace_c(thr->pid, thr->buffer_id, 0, "[ux]sbe_dy_enhance_f");
}

static void release_scroll(struct ux_scroll_info *info)
{
	struct hwui_frame_info *frame, *tmpFrame;

	list_for_each_entry_safe(frame, tmpFrame, &info->frame_list, queue_list) {
		list_del(&frame->queue_list);
		kmem_cache_free(hwui_frame_info_cachep, frame);
	}

	list_del(&info->queue_list);
	kfree(info->score);
	kmem_cache_free(ux_scroll_info_cachep, info);
}

static void release_all_ux_info(struct sbe_render_info *thr)
{
	struct ux_scroll_info *pos, *tmp;

	// clear ux scroll infos for new activity
	sbe_trace("SBE_UX clear_ux %d", thr->pid);
	list_for_each_entry_safe(pos, tmp, &thr->scroll_list, queue_list) {
		release_scroll(pos);
	}
	// reset sbe dy enhance
	thr->sbe_dy_enhance_f = -1;
}

void clear_ux_info(struct sbe_render_info *thr)
{
	//when activity resume, will call this function
	global_sbe_dy_enhance = 0;
	global_sbe_dy_enhance_max_pid = 0;
	thr->buffer_count_filter = 0;
	thr->rescue_more_count = 0;
	release_all_ux_info(thr);
}

static void update_sbe_dy_rescue(struct sbe_render_info *thr, int sbe_dy_enhance,
		int rescue_type, unsigned long long rescue_target, unsigned long long frame_id)
{
	unsigned long long ts = 0;

	if ((rescue_type & RESCUE_COUNTINUE_RESCUE) != 0
			&& (rescue_type & RESCUE_TYPE_PRE_ANIMATION) != 0
			&& (rescue_type & RESCUE_RUNNING) != 0
			&& thr->rescue_start_time > 0) {
		//pre-animation case, think frame start time same as rescue time
		struct hwui_frame_info *frame = get_hwui_frame_info_by_frameid(thr, frame_id);

		if (frame) {
			update_hwui_frame_info(thr, frame, frame_id,
					0, 0, thr->rescue_start_time, 0, 0);
			//update rescue start time
			frame->start_ts = thr->rescue_start_time;
		}

	} else if (((rescue_type & RESCUE_WAITING_ANIMATION_END) != 0
			|| (rescue_type & RESCUE_TYPE_TRAVERSAL_DYNAMIC) != 0
			|| (rescue_type & RESCUE_TYPE_OI) != 0
			|| (rescue_type & RESCUE_TYPE_PRE_ANIMATION) != 0)
			&& (rescue_type & RESCUE_COUNTINUE_RESCUE) == 0) {
		unsigned long long target_time = 0;
		struct hwui_frame_info *frame = get_hwui_frame_info_by_frameid(thr, frame_id);

		//update rescue start time
		ts = sbe_get_time();
		if (frame)
			update_hwui_frame_info(thr, frame, frame_id, 0, 0, ts, 0, 0);

		sbe_trace("[SBE] %s %d dy_rescue id %lld sbe_dy_e:%d final_e %d rescue_t:%llu ts:%llu",
			__func__, thr->pid, frame_id, thr->sbe_dy_enhance_f,
			sbe_dy_enhance, rescue_target, ts);

		target_time = thr->target_time;

		if (target_time > 0) {
			//update rescue_target, default is 1/2 target_time
			if (rescue_target > 0 && rescue_target < target_time) {
				if(thr->sbe_rescue_target_time != rescue_target)
					thr->sbe_rescue_target_time = rescue_target;
			} else
				thr->sbe_rescue_target_time = (target_time >> 1);
		}

		thr->rescue_start_time = ts;

	}
	sbe_systrace_c(thr->pid, thr->buffer_id, rescue_type, "[ux]rescue_type");
}

static struct ux_scroll_info *get_latest_ux_scroll_info(struct sbe_render_info *thr)
{
	struct ux_scroll_info *result = NULL;

	if (!thr || list_empty(&thr->scroll_list))
		return result;

	// get latest scrolling worker
	return list_first_entry_or_null(&(thr->scroll_list), struct ux_scroll_info, queue_list);
}

static void update_rescue_filter_info(struct sbe_render_info *thr)
{
	struct ux_scroll_info *last_scroll = get_latest_ux_scroll_info(thr);
	unsigned long long target_time = 0;

	if (sbe_query_cur_buffer_count(thr))
		return;

	sbe_systrace_c(thr->pid, thr->buffer_id,
			thr->cur_buffer_count, "[ux]rescue_filter");

	if (last_scroll) {
		target_time = thr->target_time;
		//target_time max thinking is 10hz, incase overflow
		// 100000000ns = 100ms * 1000 000, 100ms = 1s / 10hz
		if (target_time > 0 && target_time <= 100000000)
			last_scroll->rescue_filter_buffer_time =
					(thr->cur_buffer_count - 1) * target_time - (target_time >> 1);

	}
	sbe_systrace_c(thr->pid, thr->buffer_id, 0, "[ux]rescue_filter");
}

void fpsgo_ai_boost(struct sbe_render_info *thr, int start, int boost, int frame_id)
{
	mutex_lock(&sbe_rescue_lock);
	if (start) {
		struct hwui_frame_info *frame =
			get_hwui_frame_info_by_frameid(thr, frame_id);

		if (frame == NULL) {
			sbe_trace("frame not found!\n");
			goto leave;
		} else if (frame->end_ts > frame->start_ts) {
			sbe_trace("frame is end :%llu\n", frame_id);
			goto leave;
		}

		ai_rescuing_frame_id = frame_id;

		thr->ai_boost = boost < 0 ?  0 : boost;
		thr->ai_boost = clamp(thr->ai_boost, 0, 100);
		thr->ai_boost_ctl = 1;

		sbe_set_per_task_cap(thr);
		sbe_systrace_c(thr->pid, thr->buffer_id, thr->ai_boost, "[ux]ai_boost");
	} else {
		if (thr->ai_boost_ctl == 0)
			goto leave;
		if (frame_id < ai_rescuing_frame_id)
			goto leave;
		ai_rescuing_frame_id = -1;
		thr->ai_boost_ctl = 0;
		thr->ai_boost = 0;

		sbe_set_per_task_cap(thr);
		sbe_systrace_c(thr->pid, thr->buffer_id, thr->ai_boost, "[ux]ai_boost");
	}
leave:
	mutex_unlock(&sbe_rescue_lock);
}

void sbe_notify_ai_do_boost(struct sbe_render_info *thr, int start,
							int rescue_type, int boost, int frame_id)
{
	if (!thr || !sbe_ai_ctrl_enabled)
		return;

	if (rescue_type == RESCUE_TYPE_AI_RESCUE) {
		fpsgo_ai_boost(thr, start, boost, frame_id);
		return;
	}

	if (!start)
		fpsgo_ai_boost(thr, 0, 0, frame_id);

}

int sbe_switch_ai_clear_boost_info(struct sbe_render_info *thr)
{
	if (!thr || !sbe_ai_ctrl_enabled)
		return 0;

	if (thr->ai_boost_ctl) {
		thr->sbe_enhance = 0;
		return 1;
	}

	return 0;
}

void sbe_do_rescue(struct sbe_render_info *thr, int start, int enhance,
		int rescue_type, unsigned long long rescue_target, unsigned long long frame_id)
{
	unsigned long long ts = 0;
	int sbe_dy_enhance = -1;

	if (!thr || !sbe_rescue_enable)	//thr must find the 5566 one.
		return;

	if (sbe_query_cur_buffer_count(thr))
		return;

	sbe_notify_ai_do_boost(thr, start, rescue_type, enhance, frame_id);

	mutex_lock(&sbe_rescue_lock);
	if (start) {
		if (start == SBE_RESUCE_MODE_UPDATE_RESCUE_STRENGTH) {
			sbe_enhance_f = enhance;
			goto leave;
		}

		//before rescue check buffer count
		if (sbe_dy_rescue_enable && thr->buffer_count_filter > 0
				&& thr->cur_buffer_count > SBE_BUFFER_FILTER_THREASHOLD
				&& (rescue_type & RESCUE_TYPE_OI) == 0
				&& (rescue_type & RESCUE_RUNNING) == 0
				&& (rescue_type & RESCUE_TYPE_SECOND_RESCUE) == 0) {
			update_rescue_filter_info(thr);
			goto leave;
		}

		thr->sbe_rescuing_frame_id = frame_id;

		//update sbe rescue enhance
		if (sbe_dy_max_enhance > 0 && ((rescue_type & RESCUE_TYPE_MAX_ENHANCE) != 0))
			sbe_dy_enhance = sbe_dy_max_enhance;
		else if (!sbe_dy_rescue_enable)
			sbe_dy_enhance = sbe_enhance_f;
		else if (thr->sbe_dy_enhance_f <= 0) {
			//dy_rescue is enable, try use global_sbe_dy_enhance
			if (global_sbe_dy_enhance > 0)
				sbe_dy_enhance = thr->sbe_dy_enhance_f = global_sbe_dy_enhance;
			else
				sbe_dy_enhance = sbe_enhance_f;
		} else
			sbe_dy_enhance = thr->sbe_dy_enhance_f;

		if (sbe_dy_rescue_enable && sbe_dy_max_enhance > 0
				&& (rescue_type & RESCUE_TYPE_ENABLE_MARGIN) != 0) {
			sbe_dy_enhance += sbe_dy_enhance_margin;
			sbe_dy_enhance = clamp(sbe_dy_enhance, 0, sbe_dy_max_enhance);
		}

		thr->sbe_enhance = enhance < 0 ?  sbe_dy_enhance : (enhance + sbe_dy_enhance);
		thr->sbe_enhance = clamp(thr->sbe_enhance, 0, 100);

		if (sbe_dy_rescue_enable)
			update_sbe_dy_rescue(thr, sbe_dy_enhance,
					rescue_type, rescue_target, frame_id);


		//if sbe mark second rescue, then rescue again
		if (thr->sbe_rescue != 0)
			goto leave;
		thr->sbe_rescue = 1;

		if (sbe_dy_rescue_enable) {
			struct ux_scroll_info *last_scroll = get_latest_ux_scroll_info(thr);

			if (last_scroll && !last_scroll->rescue_with_perf_mode)
				last_scroll->rescue_with_perf_mode = 0;

			if (last_scroll && last_scroll->rescue_with_perf_mode > 0) {
				sbe_set_affinity_on_scrolling(thr->tgid, SBE_PREFER_M);
				sbe_set_affinity_on_scrolling(thr->pid, SBE_PREFER_M);
			}
		} else {
			#if SBE_AFFNITY_TASK
			sbe_set_affinity_on_scrolling(thr->tgid, SBE_PREFER_M);
			sbe_set_affinity_on_scrolling(thr->pid, SBE_PREFER_M);
			#endif
		}

		if (sbe_switch_ai_clear_boost_info(thr))
			goto leave;

		sbe_set_per_task_cap(thr);
		sbe_systrace_c(thr->pid, thr->buffer_id, thr->sbe_enhance, "[ux]sbe_rescue");
	} else {
		if (thr->sbe_rescue == 0)
			goto leave;
		if (frame_id < thr->sbe_rescuing_frame_id)
			goto leave;
		thr->sbe_rescuing_frame_id = -1;
		thr->sbe_rescue = 0;
		thr->sbe_enhance = 0;

		if (sbe_dy_rescue_enable ) {
			struct hwui_frame_info *frame = get_hwui_frame_info_by_frameid(thr, frame_id);
			struct ux_scroll_info *last_scroll = get_latest_ux_scroll_info(thr);
			//update rescue end time
			ts = sbe_get_time();
			update_hwui_frame_info(thr, frame, frame_id, 0, 0, 0, ts, 0);
			thr->rescue_start_time = 0;
			//frame->rescue_reason is update in doframe end
			if (last_scroll && last_scroll->rescue_with_perf_mode > 0) {
				sbe_set_affinity_on_scrolling(thr->tgid, SBE_PREFER_NONE);
				sbe_set_affinity_on_scrolling(thr->pid, SBE_PREFER_NONE);
			}
		} else {
			#if SBE_AFFNITY_TASK
			sbe_set_affinity_on_scrolling(thr->tgid, SBE_PREFER_NONE);
			sbe_set_affinity_on_scrolling(thr->pid, SBE_PREFER_NONE);
			#endif
		}

		sbe_set_per_task_cap(thr);
		sbe_systrace_c(thr->pid, thr->buffer_id, 0, "[ux]rescue_type");
		sbe_systrace_c(thr->pid, thr->buffer_id, thr->sbe_enhance, "[ux]sbe_rescue");
	}
leave:
	mutex_unlock(&sbe_rescue_lock);
}

int get_ux_list_length(struct list_head *head)
{
	struct list_head *temp;
	int count = 0;

	list_for_each(temp, head) {
		count++;
	}
	return count;
}

struct ux_scroll_info *search_ux_scroll_info(unsigned long long ts,
						 struct sbe_render_info *thr)
{
	struct ux_scroll_info *scr;

	list_for_each_entry (scr, &thr->scroll_list, queue_list) {
		if (scr->start_ts == ts)
			return scr;
	}
	return NULL;
}

void enqueue_ux_scroll_info(int type, unsigned long long start_ts, struct sbe_render_info *thr)
{
	int list_length;
	struct ux_scroll_info *new_node;
	struct ux_scroll_info *tmp;
	struct ux_scroll_info *first_node = NULL;

	if (!sbe_dy_rescue_enable)
		return;

	if (!thr)
		return;

	new_node = kmem_cache_alloc(ux_scroll_info_cachep, GFP_KERNEL);
	if (!new_node)
		return;

	if (new_node != NULL)
		memset(new_node, 0, sizeof(struct ux_scroll_info));

	new_node->score = kmalloc(
					(RESCUE_MAX_MONITOR_DROP_ARR_SIZE) * sizeof(int)
					, GFP_KERNEL | __GFP_ZERO);

	if (!new_node->score) {
		kmem_cache_free(ux_scroll_info_cachep, new_node);
		return;
	}

	if (new_node->score != NULL)
		memset(new_node->score, 0, (RESCUE_MAX_MONITOR_DROP_ARR_SIZE) * sizeof(int));

	INIT_LIST_HEAD(&new_node->frame_list);
	INIT_LIST_HEAD(&new_node->queue_list);
	new_node->start_ts = start_ts;
	new_node->type = type;

	list_length = get_ux_list_length(&thr->scroll_list);
	if (list_length == 0) {
		for (size_t i = 0; i < HWUI_MAX_FRAME_SAME_TIME; i++) {
			if (thr->tmp_hwui_frame_info_arr[i])
				continue;

			struct hwui_frame_info *frame = kmem_cache_alloc(hwui_frame_info_cachep, GFP_KERNEL);

			if (!frame)//allocate memory failed
				break;

			reset_hwui_frame_info(frame);
			INIT_LIST_HEAD(&frame->queue_list);
			thr->tmp_hwui_frame_info_arr[i] = frame;
		}
		thr->hwui_arr_idx = 0;
	}

	if (list_length >= scroll_cnt) {
		list_for_each_entry_reverse(tmp, &thr->scroll_list, queue_list) {
			first_node = tmp;
			break;
		}
		if (first_node != NULL)
			release_scroll(first_node);

	}
	list_add(&new_node->queue_list, &thr->scroll_list);

}

static void update_hwui_frame_info(struct sbe_render_info *info,
		struct hwui_frame_info *frame, unsigned long long id,
		unsigned long long start_ts, unsigned long long end_ts,
		unsigned long long rsc_start_ts, unsigned long long rsc_end_ts,
		int rescue_reason)
{
	if (frame && frame->frameID == id) {
		// Update the fields for this frame.
		if (start_ts) {
			frame->start_ts = start_ts;
			frame->display_rate = info->target_fps;
		}

		if (rsc_start_ts) {
			frame->rescue = true;
			frame->rsc_start_ts = rsc_start_ts;
		}
		if (rsc_end_ts)
			frame->rsc_dur_ts = rsc_end_ts - frame->rsc_start_ts;
		if (rescue_reason)
			frame->rescue_reason = rescue_reason;
		if (end_ts) {
			frame->end_ts = end_ts;
			frame->dur_ts = end_ts - frame->start_ts;
			if (frame->dur_ts > info->target_time) {
				frame->overtime = true;
				frame->rescue_suc = false;
			} else {
				frame->overtime = false;
				if(frame->rescue)
					frame->rescue_suc = true;
			}
			if (frame->rescue && !frame->rsc_dur_ts)
				frame->rsc_dur_ts = end_ts - frame->rsc_start_ts;

		}
	}
}

void reset_hwui_frame_info(struct hwui_frame_info *frame)
{
	if (frame) {
		//make frameID max
		frame->frameID = -1;
		frame->start_ts = 0;
		frame->end_ts = 0;
		frame->dur_ts = 0;
		frame->rsc_start_ts = 0;
		frame->rsc_dur_ts = 0;
		frame->overtime = false;
		frame->rescue = false;
		frame->rescue_suc = false;
		frame->rescue_reason = 0;
		frame->perf_idx = -1;
		frame->frame_status = 0;
		frame->display_rate = 0;
		frame->promotion = false;
		frame->drop = -1;
	}
}

struct hwui_frame_info *get_valid_hwui_frame_info_from_pool(struct sbe_render_info *info)
{
	struct hwui_frame_info *frame;

	if (!info || list_empty(&info->scroll_list))
		return NULL;

	if (info->hwui_arr_idx < 0
			|| info->hwui_arr_idx >= HWUI_MAX_FRAME_SAME_TIME)
		info->hwui_arr_idx = 0;

	frame = info->tmp_hwui_frame_info_arr[info->hwui_arr_idx];

	info->hwui_arr_idx++;
	return frame;
}

struct hwui_frame_info *get_hwui_frame_info_by_frameid(
		struct sbe_render_info *info, unsigned long long frameid)
{
	if (!info || list_empty(&info->scroll_list))
		return NULL;

	for (size_t i = 0; i < HWUI_MAX_FRAME_SAME_TIME; i++) {
		struct hwui_frame_info *frame = info->tmp_hwui_frame_info_arr[i];

		if (frame && frameid == frame->frameID)
			return frame;
	}
	return NULL;
}

struct hwui_frame_info *insert_hwui_frame_info_from_tmp(struct sbe_render_info *thr,
				struct hwui_frame_info *frame)
{
	struct hwui_frame_info *new_frame;
	struct ux_scroll_info *last = NULL;

	if (!thr  || list_empty(&thr->scroll_list) || !frame)
		return NULL;

	// add to the frame list.
	last = list_first_entry_or_null(&(thr->scroll_list), struct ux_scroll_info, queue_list);

	if (!last)
		return NULL;

	new_frame = kmem_cache_alloc(hwui_frame_info_cachep, GFP_KERNEL);
	if (!new_frame)
		return NULL;

	new_frame->frameID = frame->frameID;
	new_frame->start_ts = frame->start_ts;
	new_frame->end_ts = frame->end_ts;
	new_frame->dur_ts = frame->dur_ts;
	new_frame->rsc_start_ts = frame->rsc_start_ts;
	new_frame->rsc_dur_ts = frame->rsc_dur_ts;
	new_frame->display_rate = frame->display_rate;
	new_frame->overtime = frame->overtime;
	new_frame->rescue = frame->rescue;
	new_frame->rescue_suc = frame->rescue_suc;
	new_frame->rescue_reason = frame->rescue_reason;
	new_frame->frame_status = frame->frame_status;
	new_frame->perf_idx = frame->perf_idx;
	new_frame->promotion = frame->promotion;
	new_frame->drop = frame->drop;
	list_add_tail(&(new_frame->queue_list), &(last->frame_list));
	return new_frame;
}

void count_scroll_rescue_info(struct sbe_render_info *thr, struct hwui_frame_info *hwui_info)
{
	struct ux_scroll_info *scroll_info = NULL;
	unsigned long long target_time;
	int drop = 0;
	int max_monitor_drop_frame;

	if (!thr  || list_empty(&thr->scroll_list))
		return;

	// add to the frame list.
	scroll_info = list_first_entry_or_null(&(thr->scroll_list), struct ux_scroll_info, queue_list);

	if (!scroll_info)
		return;

	target_time = thr->target_time;
	if (target_time <= 0)
		return;

	max_monitor_drop_frame = RESCUE_MAX_MONITOR_DROP_ARR_SIZE - 1;
	if (hwui_info->rescue) {
		// a. check is running or continue rescue
		if ((hwui_info->rescue_reason & RESCUE_COUNTINUE_RESCUE) != 0
				|| hwui_info->dur_ts <= 0) {
			return;
		}
		scroll_info->rescue_cap_count += hwui_info->perf_idx;
		scroll_info->rescue_frame_time_count += hwui_info->dur_ts;
		scroll_info->rescue_frame_count += 1;

		if (hwui_info->dur_ts <= target_time) {
			scroll_info->score[0] += 1;
			hwui_info->drop = 0;
		} else {
			drop = (int)div64_u64(hwui_info->dur_ts, target_time);
			if (drop < 0)
				drop = 0;

			hwui_info->drop = drop;
			if (drop < max_monitor_drop_frame)
				scroll_info->score[drop] += 1;
			else
				scroll_info->score[max_monitor_drop_frame] += 1;
		}
		scroll_info->rescue_count++;
		sbe_trace("%d SBE_UX: frameid %lld cap:%d dur_ts %lld drop %d",
					thr->pid, hwui_info->frameID,
					hwui_info->perf_idx, hwui_info->dur_ts, drop);
	}

}

void sbe_del_ux(struct sbe_render_info *info)
{
	if (!info)
		return;

	sbe_reset_deplist_task_priority(info);

	if (sbe_dy_rescue_enable) {
		release_all_ux_info(info);
		for (size_t i = 0; i < HWUI_MAX_FRAME_SAME_TIME; i++) {
			if (info->tmp_hwui_frame_info_arr[i])
				kmem_cache_free(hwui_frame_info_cachep, info->tmp_hwui_frame_info_arr[i]);
		}
	}
	//reset sbe tag when render del
	sbe_systrace_c(info->pid, info->buffer_id, 2, "[ux]sbe_set_ctrl");
	list_del(&(info->scroll_list));

	//delete ux_frame_info
	sbe_ux_reset(info);
}

#if IS_ENABLED(CONFIG_ARM64)
int updata_smart_launch_capacity_tb(void)
{
#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
	int i = 0;
	int big_clsuter_num = cluster_num -1;
	int big_cluster_cpu = capacity_info[big_clsuter_num].first_cpu_id;

	if ((cluster_num > 0) && capacity_info &&
		capacity_info[big_clsuter_num].capacity &&
		(nr_freq_opp_cnt > 0)) {
		for (i = 0; i < nr_freq_opp_cnt; i++) {
			capacity_info[big_clsuter_num].capacity[i]
			= pd_X2Y(big_cluster_cpu, i, OPP, CAP, true, DPT_CALL_FBT_CLUSTER_X2Y);
		}
	}
#endif
	return 0;
}

static inline int abs_f(int value)
{
	return value < 0 ? -value : value;
}

int findBestFitNumTabIdx(int *numbers, int size, int target)
{
	int idx = 0;
	int closest = 0;
	int min_diff = 0;
	int diff = 0;

	if (size == 0)
		return idx;

	closest = numbers[0];
	min_diff = abs_f(target - closest);

	for (int i = 0; i < size; ++i) {
		diff = abs_f(target - numbers[i]);
		if (diff < min_diff) {
			min_diff = diff;
			closest = numbers[i];
			idx = i;
		}
	}
	return idx;
}

int sbe_notify_smart_launch_algorithm(int feedback_time, int target_time,
			int pre_opp, int ration)
{
	int delta = 0;
	int gap_capacity = 0;
	int next_capacity = 0;
	int pre_capacity = 0;
	int min_cap = 0;
	int max_cap = 1024;
	int kp = 1;
	int big_cluster = cluster_num -1;
	int next_opp = pre_opp;

	if (target_time <= 0 || feedback_time <= 0
		|| pre_opp < 0 || ration < 0)
		return next_opp;

	if (capacity_info && big_cluster>= 0 &&
		capacity_info[big_cluster].capacity &&
		nr_freq_opp_cnt >= 1 &&
		pre_opp <= nr_freq_opp_cnt-1) {
		pre_capacity = capacity_info[big_cluster].capacity[pre_opp];
		next_capacity = pre_capacity;
		min_cap = capacity_info[big_cluster].capacity[nr_freq_opp_cnt -1];
		max_cap = capacity_info[big_cluster].capacity[0];
	} else {
		return next_opp;
	}

	delta = feedback_time - target_time;

	if (delta <= 0) {
		delta = abs(delta) << 10;
		kp = div64_u64(delta, target_time);
		gap_capacity = (kp * pre_capacity) >> 10;
		gap_capacity = gap_capacity * (100 - ration);
		gap_capacity = div64_u64(gap_capacity, 100);
		next_capacity = pre_capacity - gap_capacity;
	} else {
		delta = delta << 10;
		kp = div64_u64(delta, target_time);
		gap_capacity = (kp * pre_capacity) >> 10;
		gap_capacity = gap_capacity * (100 + ration);
		gap_capacity = div64_u64(gap_capacity, 100);
		next_capacity = pre_capacity + gap_capacity;
	}

	next_capacity = clamp(next_capacity, min_cap, max_cap);
	next_opp = findBestFitNumTabIdx(capacity_info[big_cluster].capacity,
				nr_freq_opp_cnt, next_capacity);
	return next_opp;
}

int init_smart_launch_capacity_tb(int cluster_num)
{
	struct cpufreq_policy *policy;
	int cluster = 0;
	int cpu;

	for_each_possible_cpu(cpu) {
		if (cluster_num <= 0 || cluster >= cluster_num)
			break;
		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			break;

		capacity_info[cluster].first_cpu_id = cpumask_first(policy->related_cpus);
		capacity_info[cluster].num_opp =
		sbe_arch_nr_get_opp_cpu(capacity_info[cluster].first_cpu_id);
		capacity_info[cluster].capacity = kcalloc(nr_freq_opp_cnt,
					sizeof(unsigned int), GFP_KERNEL);

		cluster++;
		cpu = cpumask_last(policy->related_cpus);
		cpufreq_cpu_put(policy);
	}
	return 0;
}
#endif

void init_smart_launch_engine(void)
{
#if IS_ENABLED(CONFIG_ARM64)
	smart_launch_off_on = 0;
	if (smart_launch_off_on) {
		cluster_num  = sbe_arch_nr_clusters();
		if (cluster_num != SMART_LAUNCH_BOOST_SUPPORT_CLUSTER_NUM)
			return;
		nr_freq_opp_cnt = sbe_arch_nr_max_opp_cpu();
		capacity_info = kcalloc(cluster_num,
				sizeof(struct smart_launch_capacity_info), GFP_KERNEL);
		init_smart_launch_capacity_tb(cluster_num);
		updata_smart_launch_capacity_tb();
		sbe_notify_smart_launch_algorithm_fp =
		sbe_notify_smart_launch_algorithm;
	}
#endif
}

void destroy_smart_launch_capinfo(void)
{
#if IS_ENABLED(CONFIG_ARM64)
	int i = 0;

	if (smart_launch_off_on) {
		for (i = 0; i < cluster_num; i++) {
			if (capacity_info && capacity_info[i].capacity)
				kfree(capacity_info[i].capacity);
		}
		kfree(capacity_info);
	}
#endif
}

void fpsgo2sbe_hint_frameinfo(unsigned long cmd, struct render_frame_info *iter)
{
	struct task_info *tmp_dep_arr = NULL;
	int dep_num = 0;
	int sbe_tgid = atomic_read(&g_web_or_flutter_tgid);

	if (sbe_tgid <= 0 || iter->tgid != sbe_tgid)
		return;

	tmp_dep_arr = kcalloc(MAX_TASK_NUM, sizeof(struct task_info), GFP_KERNEL);
	//get cur render dep
	if (tmp_dep_arr)
		dep_num = fpsgo_other2xgf_get_critical_tasks(iter->pid,
						MAX_TASK_NUM, tmp_dep_arr, 1, iter->buffer_id);

	if (dep_num > 0)
		sbe_notify_update_fpsgo_jerk_boost_info(iter->tgid, iter->pid,
				iter->blc, cmd,  iter->jerk_boost_flag, tmp_dep_arr);

	kfree(tmp_dep_arr);
}

void update_fpsgo_hint_param(int scrolling, int tgid)
{
	if (scrolling) {
		if (tgid > 0)
			atomic_set(&g_web_or_flutter_tgid, tgid);
	} else {
		//reset the tgid
		atomic_set(&g_web_or_flutter_tgid, 0);
		//clear vip!!

		sbe_get_tree_lock(__func__);
		for (size_t i = 0; i < FPSGO_MAX_TASK_NUM; i++) {
			struct task_info *dep_task = g_dep_arr_last;

			if (dep_task && dep_task[i].pid > 0) {
				if (set_deplist_vip)
					unset_task_basic_vip(dep_task[i].pid);
				if (set_deplist_affinity)
					sbe_set_affinity_on_scrolling(dep_task[i].pid, SBE_PREFER_NONE);
			}
		}
		memset(g_dep_arr_last, 0, sizeof(struct task_info) * FPSGO_MAX_TASK_NUM);
		sbe_put_tree_lock(__func__);
	}
}

int sbe_get_fpsgo_info(int tgid, int pid, int blc,
		unsigned long mask, int jerk_boost_flag, struct task_info *dep_arr)
{
	if (tgid == atomic_read(&g_web_or_flutter_tgid)
			&& dep_arr && vip_engine_set_vip_ctrl_node_sbe &&
			vip_engine_unset_vip_ctrl_node_sbe) {
		sbe_get_tree_lock(__func__);
		for (size_t i = 0; i < FPSGO_MAX_TASK_NUM; i++) {
			struct task_info *dep_task = g_dep_arr_last;

			if (dep_task && dep_task[i].pid > 0) {
				if (set_deplist_vip)
					vip_engine_unset_vip_ctrl_node_sbe(dep_task[i].pid, WORKER_VIP);
				if (set_deplist_affinity)
					sbe_set_affinity_on_scrolling(dep_task[i].pid, SBE_PREFER_NONE);
			}
		}

		//copy once reset when scrolling end!!!!
		memset(g_dep_arr_last, 0, sizeof(struct task_info) * FPSGO_MAX_TASK_NUM);
		memcpy(g_dep_arr_last, dep_arr, sizeof(struct task_info) * FPSGO_MAX_TASK_NUM);

		for (size_t i = 0; i < FPSGO_MAX_TASK_NUM; i++) {
			struct task_info *dep_task = g_dep_arr_last;

			if (dep_task && dep_task[i].pid > 0) {
				if (set_deplist_vip)
					vip_engine_set_vip_ctrl_node_sbe(dep_task[i].pid, WORKER_VIP, 12);
				if (set_deplist_affinity)
					sbe_set_affinity_on_scrolling(dep_task[i].pid, SBE_PREFER_M);
			}
		}
		sbe_put_tree_lock(__func__);
	}
	return 0;
}

void __exit sbe_cpu_ctrl_exit(void)
{
	unregister_fpsgo_frame_info_callback(fpsgo2sbe_hint_frameinfo);
	destroy_smart_launch_capinfo();
	kmem_cache_destroy(frame_info_cachep);
	kmem_cache_destroy(ux_scroll_info_cachep);
	kmem_cache_destroy(hwui_frame_info_cachep);
}

int __init sbe_cpu_ctrl_init(void)
{
	sbe_rescue_enable = 1;
	init_smart_launch_engine();
	ux_general_policy = 0;
	ux_general_policy_type = 0;
	ux_general_policy_dpt_setwl = 0;
	sbe_enhance_f = 50;
	sbe_critical_basic_cap = 0;
	sbe_dy_max_enhance = 70;
	sbe_dy_enhance_margin = 15;
	sbe_dy_frame_threshold = 3;
	sbe_dy_rescue_enable = 1;
	scroll_cnt = 6;
	set_deplist_vip = 1;
	set_deplist_affinity = 1;
	set_deplist_ls = 1;
	gas_threshold = 10;
	gas_threshold_for_low_TLP = 10;
	gas_threshold_for_high_TLP = 5;
	sbe_runnable_util_est_disable = 1;
	sbe_extra_sub_en_deque_enable = 1;
	sbe_notify_fpsgo_vir_boost = 0;
	sbe_dptv2_enable = 0;
	sbe_loading_threashold_l = SBE_FRAME_CAP_THREASHOLD_L;
	sbe_loading_threashold_m = SBE_FRAME_CAP_THREASHOLD;
	sbe_loading_threashold_h = SBE_FRAME_CAP_THREASHOLD_P;
	sbe_affinity_task = SBE_PREFER_M;
	sbe_affinity_task_min_cap = SBE_DEFAULT_AFFINITY_TASK_MIN_CAP;
	sbe_affinity_task_low_threshold_cap = SBE_DEFAULT_AFFINITY_TASK_LOW_THRESHOLD_CAP;
	sbe_extra_sub_deque_margin_time = SBE_DEFAULT_DEUQUE_MARGIN_TIME_NS;
	sbe_force_bypass_dptv2 = 0;
	sbe_dptv2_status = 0;
	sbe_ignore_vip_task_enable = 1;
	sbe_ignore_vip_task_status = 0;

	ai_rescuing_frame_id = -1;
	registered = 0;
	sbe_ai_ctrl_enabled = 0;
	sbe_uclamp_margin = 0;

	atomic_set(&g_web_or_flutter_tgid, 0);

	register_fpsgo_frame_info_callback(1 << GET_FPSGO_Q2Q_TIME,
			fpsgo2sbe_hint_frameinfo);

	frame_info_cachep = kmem_cache_create("ux_frame_info",
		sizeof(struct ux_frame_info), 0, SLAB_HWCACHE_ALIGN, NULL);
	ux_scroll_info_cachep = kmem_cache_create("ux_scroll_info",
		sizeof(struct ux_scroll_info), 0, SLAB_HWCACHE_ALIGN, NULL);
	hwui_frame_info_cachep = kmem_cache_create("hwui_frame_info",
		sizeof(struct hwui_frame_info), 0, SLAB_HWCACHE_ALIGN, NULL);
	if (!frame_info_cachep || !ux_scroll_info_cachep || !hwui_frame_info_cachep)
		return -1;

	return 0;
}
