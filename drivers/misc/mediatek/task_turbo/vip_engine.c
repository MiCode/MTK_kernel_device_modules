// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2025 MediaTek Inc.
 */

#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/sched/signal.h>
#include <uapi/linux/sched/types.h>
#include <linux/platform_device.h>
#include <linux/cpumask.h>
#include <kernel/sched/sched.h>
#include <drivers/android/binder_internal.h>
#include <linux/of.h>

#include <trace/hooks/sched.h>
#include <trace/hooks/binder.h>

#include "vip_engine.h"
#include "eas/eas_plus.h"
#if IS_ENABLED(CONFIG_MTK_CORE_CTL)
#include "core_ctl/core_ctl.h"
#endif
#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
#include "eas/vip.h"
#endif


#define CREATE_TRACE_POINTS
#include "trace_vip_engine.h"

LIST_HEAD(uclamp_data_head);
LIST_HEAD(vipServer_data_head);

#define TAG			"VIP-Engine"
#define VIP_PRIO_OFFSET		5
#define UCLAMP_MAX_VALUE	1024
#define UCLAMP_MIN_VALUE	0
#define HWCODE_IDX		20
#define SEGMENT_IDX		30

#define MAX_LOOM_BUF_SIZE 4

#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
#define is_VIP_basic(vts) (vts->basic_vip)
#define is_VVIP(vts) (vts->vvip)
#define is_priority_based_vip(vts) ((vts->priority_based_prio <= MAX_PRIORITY_BASED_VIP) &&	\
	(vts->priority_based_prio >= MIN_PRIORITY_BASED_VIP))
#endif

static DEFINE_SPINLOCK(check_lock);
static DEFINE_SPINLOCK(binder_uclamp_lock);
static DEFINE_SPINLOCK(binder_vipServer_lock);
static DEFINE_MUTEX(cpu_lock);
static DEFINE_MUTEX(cpu_loading_lock);
static DEFINE_MUTEX(wi_lock);
static DEFINE_MUTEX(enforced_qualified_lock);
static DEFINE_SPINLOCK(loom_affinity_lock);

static void init_turbo_attr(struct task_struct *p);
static int avg_cpu_loading;
static int cpu_loading_thres = 95;
static int vip_util_thres = 80;
#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
static int binder_vip_inheritance_enable = 1;
static int binder_nonvip_inheritance_enable;
static int tt_vip_enable = VIPE_MODE_SPECIAL;
#else
static int tt_vip_enable = VIPE_MODE_OFF;
#endif
static int binder_uclamp_inheritance_enable;
static int binder_uclamp_inheritance_enable_authority = 1;
static int binder_vipServer_enable;
static int binder_stuff_cmd;
static pid_t unset_binder_uclamp_pid;
static struct workqueue_struct *uclamp_wq;
static struct cpu_info ci;
static u64 checked_timestamp;
static int max_cpus;
static struct cpu_time *cur_wall_time, *cur_idle_time,
						*prev_wall_time, *prev_idle_time;
static void tt_vip_event_handler(struct work_struct *work);
static DECLARE_WORK(tt_vip_worker, tt_vip_event_handler);
static void tt_vip_periodic_handler(struct work_struct *work);
static DECLARE_WORK(tt_vip_periodic_worker, tt_vip_periodic_handler);
static int ssid_tgid;
static int sui_tgid;
static int f_tgid;
static ktime_t cur_touch_time;
static ktime_t cur_touch_down_time;
static u64 enforced_qualified_mask;
#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
static atomic_t vip_loom_flt_cfg;
static int flt_orig_mode = -1;
#endif
static atomic_t vip_loom_select_cfg;
static struct affinity_data_node loom_rec_buffer[MAX_LOOM_BUF_SIZE];
static atomic_t loom_cpu_dedicated_enable;
static const char * const caller_id_desc[MAX_TYPE] = {
	"DEBUG_NODE", "FPSGO", "UX", "VIDEO"
};

int  (*loom_set_cpus_allowed_ptr_by_kernel_fp)(struct task_struct *p, const struct cpumask *new_mask) = NULL;
EXPORT_SYMBOL(loom_set_cpus_allowed_ptr_by_kernel_fp);
int  (*wi_add_tgid_hook)(int pid) = NULL;
EXPORT_SYMBOL(wi_add_tgid_hook);
int  (*wi_del_tgid_hook)(int pid) = NULL;
EXPORT_SYMBOL(wi_del_tgid_hook);
int  (*disable_tt_vip_hook)(u64) = NULL;
EXPORT_SYMBOL(disable_tt_vip_hook);
void (*turn_on_tgd_hook)(void) = NULL;
EXPORT_SYMBOL(turn_on_tgd_hook);
void (*turn_off_tgd_hook)(void) = NULL;
EXPORT_SYMBOL(turn_off_tgd_hook);
int  (*set_tgd_hook)(int tgd) = NULL;
EXPORT_SYMBOL(set_tgd_hook);
int  (*unset_tgd_hook)(int tgd) = NULL;
EXPORT_SYMBOL(unset_tgd_hook);
void (*set_td_hook)(int td) = NULL;
EXPORT_SYMBOL(set_td_hook);
void (*unset_td_hook)(int td) = NULL;
EXPORT_SYMBOL(unset_td_hook);
void (*set_tdtgd_hook)(int tgd) = NULL;
EXPORT_SYMBOL(set_tdtgd_hook);
void (*unset_tdtgd_hook)(int tgd) = NULL;
EXPORT_SYMBOL(unset_tdtgd_hook);
int (*tt_vip_algo_hook)(int ct_vip_qualified, int launch_vip_qualified,
						int ssid_tgid, int sui_tgid, int f_tgid, bool touching) = NULL;
EXPORT_SYMBOL(tt_vip_algo_hook);
int  (*is_target_found_hook)(const char*, int) = NULL;
EXPORT_SYMBOL(is_target_found_hook);
#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
void (*binder_start_vip_inherit_hook)(int, int) = NULL;
EXPORT_SYMBOL(binder_start_vip_inherit_hook);
void (*binder_stop_vip_inherit_hook)(int, int) = NULL;
EXPORT_SYMBOL(binder_stop_vip_inherit_hook);
#endif

struct devinfo_tag {
	u32 data_size;
	u32 data[300];
};

struct loom_support_rule {
	u32 hwcode;
	u32 segment_mask;   /* 0x00 = don't care */
};

static const struct devinfo_tag *get_devinfo(void)
{
	static const struct devinfo_tag *tags;
	static bool initialized;

	if (likely(initialized))
		return tags;

	if (!initialized) {
		struct device_node *np;

		np = of_find_node_by_path("/chosen");
		if (np) {
			tags = (struct devinfo_tag *) of_get_property(np, "atag,devinfo", NULL);
			of_node_put(np);
		}
		initialized = true;
	}

	return tags;
}

static bool is_device_support_loom(void)
{
	static const struct loom_support_rule rules[] = {
		{ 0x1471, 0x00 },
		{ 0x6899, 0x08 },
	};

	const struct devinfo_tag *tags;
	size_t i;

	tags = get_devinfo();
	if (!tags) {
		pr_info("%s: devinfo property missing\n", __func__);
		return false;
	}

	for (i = 0; i < ARRAY_SIZE(rules); i++) {
		if (tags->data[HWCODE_IDX] != rules[i].hwcode)
			continue;

		if (!rules[i].segment_mask ||
		    (tags->data[SEGMENT_IDX] & rules[i].segment_mask))
			return true;
	}
	return false;
}

/*
 * get_cpu_loading - Calculates the CPU loading for each CPU
 * @_ci: Pointer to a cpu_info structure to store the loading of each CPU
 *
 * This function iterates over each possible CPU and calculates its loading
 * based on the difference between total wall time and idle time since the last
 * measurement. It stores the calculated CPU loading in the provided cpu_info
 * structure.
 *
 * The CPU loading is calculated as a percentage value representing the
 * proportion of non-idle time(active time) over the total measured wall time for each CPU.
 *
 * Return: 0 on success, negative error code on failure
 */
static int get_cpu_loading(struct cpu_info *_ci)
{
	int i, cpu_loading = 0;
	u64 wall_time = 0, idle_time = 0;

	mutex_lock(&cpu_lock);
	if (ZERO_OR_NULL_PTR(cur_wall_time)) {
		mutex_unlock(&cpu_lock);
		return -ENOMEM;
	}
	for (i = 0; i < max_cpus; i++)
		_ci->cpu_loading[i] = 0;
	for_each_possible_cpu(i) {
		if (i >= max_cpus)
			break;
		cpu_loading = 0;
		wall_time = 0;
		idle_time = 0;
		prev_wall_time[i].time = cur_wall_time[i].time;
		prev_idle_time[i].time = cur_idle_time[i].time;
		/*idle time include iowait time*/
		cur_idle_time[i].time = get_cpu_idle_time(i,
				&cur_wall_time[i].time, 1);
		if (cpu_active(i)) {
			wall_time = cur_wall_time[i].time - prev_wall_time[i].time;
			idle_time = cur_idle_time[i].time - prev_idle_time[i].time;
		}
		if (wall_time > 0 && wall_time > idle_time)
			cpu_loading = div_u64((100 * (wall_time - idle_time)),
			wall_time);
		_ci->cpu_loading[i] = cpu_loading;
	}
	mutex_unlock(&cpu_lock);
	return 0;
}

void exp_trace_turbo_vip(const char *desc, int pid)
{
	trace_turbo_vip(avg_cpu_loading, cpu_loading_thres, desc, pid, "-1", INVALID_VAL, enforced_qualified_mask);
}
EXPORT_SYMBOL(exp_trace_turbo_vip);

/*
 * update_win_pid_status - Update the status of a window PID
 * @buf: the user-provided buffer with the PID and status
 * @kp: kernel parameter structure
 *
 * Parses the PID and status from the user-provided buffer. If the parsing
 * succeeds, the function either adds or deletes the PID from the window
 * info list based on the status value.
 * status: 0 [PAUSED] APP in background, 1 [RESUMED] APP on screen, 3 [DEAD] APP is killed.
 *
 * Return: 0 on success, negative error code on failure
 */
static char win_pid_status_param[64] = "";
static int update_win_pid_status(const char *buf, const struct kernel_param *kp)
{
	int retval = 0, status = 0;
	int pid;

	if (sscanf(buf, "%d%d", &pid, &status) != 2)
		return -EINVAL;

	if (pid < 0 || pid > PID_MAX_DEFAULT)
		return -EINVAL;

	if (tt_vip_enable == VIPE_MODE_ON) {
		mutex_lock(&wi_lock);
		if (status == 1 && wi_add_tgid_hook)
			retval = wi_add_tgid_hook(pid);
		else if (wi_del_tgid_hook)
			retval = wi_del_tgid_hook(pid);
		mutex_unlock(&wi_lock);

		pr_info("turbo_vip: %s: retval=%d\n", __func__, retval);
	}
	return 0;
}

static const struct kernel_param_ops update_win_pid_status_ops = {
	.set = update_win_pid_status,
	.get = param_get_charp,
};

module_param_cb(update_win_pid_status, &update_win_pid_status_ops, &win_pid_status_param, 0664);
MODULE_PARM_DESC(update_win_pid_status, "send window pid and status to task turbo");

/*
 * enable_tt_vip - Master switch for enabling or disabling tt vip feature
 * @buf: the user-provided buffer with the value to set
 * @kp: kernel parameter structure (unused)
 *
 * Return: 0 on success, negative error code on failure
 */
static int enable_tt_vip(const char *buf, const struct kernel_param *kp)
{
	int retval = 0, val = 0;

	retval = kstrtouint(buf, 0, &val);

	if (retval)
		return -EINVAL;

	if (val < VIPE_MODE_OFF || val >= VIPE_MODE_MAX)
		return -EINVAL;

	tt_vip_enable = val;

	if ((tt_vip_enable == VIPE_MODE_OFF || tt_vip_enable == VIPE_MODE_SPECIAL) && disable_tt_vip_hook)
		disable_tt_vip_hook(enforced_qualified_mask);

	return retval;
}

static const struct kernel_param_ops enable_tt_vip_ops = {
	.set = enable_tt_vip,
	.get = param_get_int,
};

module_param_cb(enable_tt_vip, &enable_tt_vip_ops, &tt_vip_enable, 0664);
MODULE_PARM_DESC(enable_tt_vip, "Enable or disable tt vip");

static int enable_tgd_param;
static int enable_tgd(const char *buf, const struct kernel_param *kp)
{
	int retval = 0, val = 0;

	retval = kstrtouint(buf, 0, &val);

	if (retval)
		return -EINVAL;

	enable_tgd_param = !!val;

	if (turn_on_tgd_hook && turn_off_tgd_hook) {
		if (enable_tgd_param)
			turn_on_tgd_hook();
		else
			turn_off_tgd_hook();
		trace_turbo_vip(INVALID_LOADING, INVALID_LOADING, "DEBUG enable: tgd_hook:",
						enable_tgd_param, "-1", INVALID_VAL, enforced_qualified_mask);
	}

	return retval;
}

static const struct kernel_param_ops enable_tgd_ops = {
	.set = enable_tgd,
	.get = param_get_int,
};

module_param_cb(enable_tgd, &enable_tgd_ops, &enable_tgd_param, 0664);
MODULE_PARM_DESC(enable_tgd, "enable tgd to vip for debug");

static int set_tgd_param;
static int set_tgd(const char *buf, const struct kernel_param *kp)
{
	int retval = 0;

	set_tgd_param = -1;
	retval = kstrtouint(buf, 0, &set_tgd_param);

	if (retval)
		return -EINVAL;

	if (set_tgd_param < 0 || set_tgd_param > PID_MAX_DEFAULT)
		return -EINVAL;

	if (set_tgd_hook && tt_vip_enable == VIPE_MODE_ON) {
		set_tgd_hook(set_tgd_param);
		trace_turbo_vip(INVALID_LOADING, INVALID_LOADING, "DEBUG set: tgd_hook:",
						set_tgd_param, "-1", INVALID_VAL, enforced_qualified_mask);
	}

	return retval;
}

static const struct kernel_param_ops set_tgd_ops = {
	.set = set_tgd,
	.get = param_get_int,
};

module_param_cb(set_tgd, &set_tgd_ops, &set_tgd_param, 0664);
MODULE_PARM_DESC(set_tgd, "set tgd to vip for debug");

static int unset_tgd_param;
static int unset_tgd(const char *buf, const struct kernel_param *kp)
{
	int retval = 0;

	unset_tgd_param = -1;
	retval = kstrtouint(buf, 0, &unset_tgd_param);

	if (retval)
		return -EINVAL;

	if (unset_tgd_param < 0 || unset_tgd_param > PID_MAX_DEFAULT)
		return -EINVAL;

	if (unset_tgd_hook) {
		unset_tgd_hook(unset_tgd_param);
		trace_turbo_vip(INVALID_LOADING, INVALID_LOADING, "DEBUG unset: tgd_hook:",
						unset_tgd_param, "-1", INVALID_VAL, enforced_qualified_mask);
	}

	return retval;
}

static const struct kernel_param_ops unset_tgd_ops = {
	.set = unset_tgd,
	.get = param_get_int,
};

module_param_cb(unset_tgd, &unset_tgd_ops, &unset_tgd_param, 0664);
MODULE_PARM_DESC(unset_tgd, "unset tgd to vip for debug");

static int set_td_param;
static int set_td(const char *buf, const struct kernel_param *kp)
{
	int retval = 0;

	set_td_param = -1;
	retval = kstrtouint(buf, 0, &set_td_param);

	if (retval)
		return -EINVAL;

	if (set_td_param < 0 || set_td_param > PID_MAX_DEFAULT)
		return -EINVAL;

	if (set_td_hook && tt_vip_enable == VIPE_MODE_ON){
		set_td_hook(set_td_param);
		trace_turbo_vip(INVALID_LOADING, INVALID_LOADING, "DEBUG set: td_hook:",
						set_td_param, "-1", INVALID_VAL, enforced_qualified_mask);
	}

	return retval;
}

static const struct kernel_param_ops set_td_ops = {
	.set = set_td,
	.get = param_get_int,
};

module_param_cb(set_td, &set_td_ops, &set_td_param, 0664);
MODULE_PARM_DESC(set_td, "set td to vip for debug");

static int unset_td_param;
static int unset_td(const char *buf, const struct kernel_param *kp)
{
	int retval = 0;

	unset_td_param = -1;
	retval = kstrtouint(buf, 0, &unset_td_param);

	if (retval)
		return -EINVAL;

	if (unset_td_param < 0 || unset_td_param > PID_MAX_DEFAULT)
		return -EINVAL;

	if (unset_td_hook) {
		unset_td_hook(unset_td_param);
		trace_turbo_vip(INVALID_LOADING, INVALID_LOADING, "DEBUG unset: td_hook:",
						unset_td_param, "-1", INVALID_VAL, enforced_qualified_mask);
	}

	return retval;
}

static const struct kernel_param_ops unset_td_ops = {
	.set = unset_td,
	.get = param_get_int,
};

module_param_cb(unset_td, &unset_td_ops, &unset_td_param, 0664);
MODULE_PARM_DESC(unset_td, "unset td to vip for debug");

static int set_vvtd_param;
static int set_vvtd(const char *buf, const struct kernel_param *kp)
{
	int retval = 0;

	set_vvtd_param = -1;
	retval = kstrtouint(buf, 0, &set_vvtd_param);

	if (retval)
		return -EINVAL;

	if (set_vvtd_param < 0 || set_vvtd_param > PID_MAX_DEFAULT)
		return -EINVAL;

	set_vip_ctrl_node(set_vvtd_param, VVIP, 12);

	return retval;
}

static const struct kernel_param_ops set_vvtd_ops = {
	.set = set_vvtd,
	.get = param_get_int,
};

module_param_cb(set_vvtd, &set_vvtd_ops, &set_vvtd_param, 0664);
MODULE_PARM_DESC(set_vvtd, "set td to vvip for debug");

static int unset_vvtd_param;
static int unset_vvtd(const char *buf, const struct kernel_param *kp)
{
	int retval = 0;

	unset_vvtd_param = -1;
	retval = kstrtouint(buf, 0, &unset_vvtd_param);

	if (retval)
		return -EINVAL;

	if (unset_vvtd_param < 0 || unset_vvtd_param > PID_MAX_DEFAULT)
		return -EINVAL;

	unset_vip_ctrl_node(unset_vvtd_param, VVIP);

	return retval;
}

static const struct kernel_param_ops unset_vvtd_ops = {
	.set = unset_vvtd,
	.get = param_get_int,
};

module_param_cb(unset_vvtd, &unset_vvtd_ops, &unset_vvtd_param, 0664);
MODULE_PARM_DESC(unset_vvtd, "unset td to vvip for debug");

static int set_tdtgd_param;
static int set_tdtgd(const char *buf, const struct kernel_param *kp)
{
	struct task_struct *p;
	int retval = 0;

	set_tdtgd_param = -1;
	retval = kstrtouint(buf, 0, &set_tdtgd_param);

	if (retval)
		return -EINVAL;

	if (set_tdtgd_param < 0 || set_tdtgd_param > PID_MAX_DEFAULT)
		return -EINVAL;

	if (set_tdtgd_hook && tt_vip_enable == VIPE_MODE_ON){
		rcu_read_lock();
		p = find_task_by_vpid(set_tdtgd_param);
		if (p)
			set_tdtgd_hook(p->tgid);
		rcu_read_unlock();
		trace_turbo_vip(INVALID_LOADING, INVALID_LOADING, "DEBUG set: tdtgd_hook:",
						set_tdtgd_param, "-1", INVALID_VAL, enforced_qualified_mask);
	}

	return retval;
}

static const struct kernel_param_ops set_tdtgd_ops = {
	.set = set_tdtgd,
	.get = param_get_int,
};

module_param_cb(set_tdtgd, &set_tdtgd_ops, &set_tdtgd_param, 0664);
MODULE_PARM_DESC(set_tdtgd, "set tdtgd to vip for debug");

static int unset_tdtgd_param;
static int unset_tdtgd(const char *buf, const struct kernel_param *kp)
{
	struct task_struct *p;
	int retval = 0;

	unset_tdtgd_param = -1;
	retval = kstrtouint(buf, 0, &unset_tdtgd_param);

	if (retval)
		return -EINVAL;

	if (unset_tdtgd_param < 0 || unset_tdtgd_param > PID_MAX_DEFAULT)
		return -EINVAL;

	if (unset_tdtgd_hook) {
		rcu_read_lock();
		p = find_task_by_vpid(unset_tdtgd_param);
		if (p)
			unset_tdtgd_hook(p->tgid);
		rcu_read_unlock();
		trace_turbo_vip(INVALID_LOADING, INVALID_LOADING, "DEBUG unset: tdtgd_hook:",
						unset_tdtgd_param, "-1", INVALID_VAL, enforced_qualified_mask);
	}

	return retval;
}

static const struct kernel_param_ops unset_tdtgd_ops = {
	.set = unset_tdtgd,
	.get = param_get_int,
};

module_param_cb(unset_tdtgd, &unset_tdtgd_ops, &unset_tdtgd_param, 0664);
MODULE_PARM_DESC(unset_tdtgd, "unset tdtgd to vip for debug");

#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
static int enable_binder_vip_inheritance(const char *buf, const struct kernel_param *kp)
{
	int retval = 0, val = 0;

	retval = kstrtouint(buf, 0, &val);

	if (retval)
		return -EINVAL;

	if (val < 0)
		return -EINVAL;

	binder_vip_inheritance_enable = !!val;
	return retval;
}

static const struct kernel_param_ops enable_binder_vip_inheritance_ops = {
	.set = enable_binder_vip_inheritance,
	.get = param_get_int,
};

module_param_cb(enable_binder_vip_inheritance
		, &enable_binder_vip_inheritance_ops, &binder_vip_inheritance_enable, 0664);
MODULE_PARM_DESC(enable_binder_vip_inheritance, "Enable or disable binder vip inheritance");

static int enable_binder_nonvip_inheritance(const char *buf, const struct kernel_param *kp)
{
	int retval = 0, val = 0;

	retval = kstrtouint(buf, 0, &val);

	if (retval)
		return -EINVAL;

	if (val < 0)
		return -EINVAL;

	binder_nonvip_inheritance_enable = !!val;
	return retval;
}

static const struct kernel_param_ops enable_binder_nonvip_inheritance_ops = {
	.set = enable_binder_nonvip_inheritance,
	.get = param_get_int,
};

module_param_cb(enable_binder_nonvip_inheritance
		, &enable_binder_nonvip_inheritance_ops, &binder_nonvip_inheritance_enable, 0664);
MODULE_PARM_DESC(enable_binder_nonvip_inheritance, "Enable or disable binder nonvip inheritance");
#endif

/*
 * enforce_ct_to_vip - Enforce critical task(ct) VIP status based on caller id
 * @val: the value indicating whether to enforce VIP status
 * @caller_id: the caller id, which determines the context of the enforcement
 *
 * Sets or clears the enforced VIP status based on the value and caller id.
 * If any caller id has enforced VIP status, the corresponding bit in the
 * mask is set, and process the VIP settings.
 * Otherwise, the enforcement is cleared. The function also logs the action
 * for debugging purposes.
 *
 * Return: 0 on success, -EINVAL if the caller id is out of range
 */
int enforce_ct_to_vip(int val, int caller_id)
{
	u64 tmp_mask;

	if (caller_id < 0 || caller_id >= MAX_TYPE)
		return -EINVAL;

	val = (val > 0) ? 1 : 0;
	mutex_lock(&enforced_qualified_lock);
	if (val)
		enforced_qualified_mask |= (1U << caller_id);
	else
		enforced_qualified_mask &= ~(1U << caller_id);

	tmp_mask = enforced_qualified_mask;
	mutex_unlock(&enforced_qualified_lock);
	if (tmp_mask && val && tt_vip_enable == VIPE_MODE_ON)
		queue_work(system_highpri_wq, &tt_vip_worker);

	trace_turbo_vip(INVALID_LOADING, INVALID_LOADING, "enforce:",
					INVALID_TGID, caller_id_desc[caller_id], val, tmp_mask);

	return 0;
}
EXPORT_SYMBOL_GPL(enforce_ct_to_vip);

/*
 * Callback for turbo state change notification from task_turbo.
 * This function will be called when turbo is enabled.
 * Return 0 on success, <0 on error.
 */
int task_turbo_launch_turbo_notify(void)
{
	if (tt_vip_enable == VIPE_MODE_ON)
		queue_work(system_highpri_wq, &tt_vip_worker);
	return 0;
}

/**
 * enforce_ct_to_vip_param - Callback for setting enforce_ct_to_vip via sysfs
 * @buf: the user-provided buffer with the value and caller id
 * @kp: kernel parameter structure
 *
 * Parses the value and caller id from the user-provided buffer and calls
 * enforce_ct_to_vip() to enforce or clear VIP status accordingly.
 *
 * Return: 0 on success, negative error code on failure
 */
static char enforced_qualified_param[64] = "";
static int enforce_ct_to_vip_param(const char *buf, const struct kernel_param *kp)
{
	int val = 0, caller_id = 0;

	if (sscanf(buf, "%d %d", &val, &caller_id) != 2)
		return -EINVAL;

	return enforce_ct_to_vip(val, caller_id);
}

static const struct kernel_param_ops enforce_ct_to_vip_param_ops = {
	.set = enforce_ct_to_vip_param,
	.get = param_get_int,
};
module_param_cb(enforce_ct_to_vip_param, &enforce_ct_to_vip_param_ops,
				&enforced_qualified_param, 0664);

static void sched_attr_work_func(struct work_struct *work)
{
	struct sched_attr_work *sched_work = container_of(work, struct sched_attr_work, work);
	struct task_turbo_t *task_turbo_data;
	int ret;

	if(uclamp_eff_value(sched_work->task, UCLAMP_MIN) == sched_work->attr.sched_util_min &&
			uclamp_eff_value(sched_work->task, UCLAMP_MAX) == sched_work->attr.sched_util_max) {
		trace_binder_uclamp_set(sched_work->task->pid,
			sched_work->attr.sched_util_max, sched_work->attr.sched_util_min, 1);
		kfree(sched_work);
		return;
	}
	ret = sched_setattr_nocheck(sched_work->task, &sched_work->attr);
	trace_binder_uclamp_set(sched_work->task->pid,
		sched_work->attr.sched_util_max, sched_work->attr.sched_util_min, ret);
	if (ret < 0) {
		if(sched_work->toSet==0){
			task_turbo_data = get_task_turbo_t(sched_work->task);
			task_turbo_data->uclamp_binder_cnt++;
		}
		switch (-ret) {
		case ESRCH:
			pr_info("%s: Error: No such process\n", __func__);
			break;
		case EINVAL:
			pr_info("%s: Error: Invalid argument\n", __func__);
			break;
		case EPERM:
			pr_info("%s: Error: Operation not permitted\n", __func__);
			break;
		case ENOMEM:
			pr_info("%s: Error: Out of memory\n", __func__);
			break;
		default:
			pr_info("%s: Error: unknown\n", __func__);
			break;
		}
	}
	kfree(sched_work);
}

void set_sched_attr_non_blocking(struct task_struct *task, const struct sched_attr *attr, int toSet)
{
	struct sched_attr_work *sched_work;

	sched_work = kmalloc(sizeof(*sched_work), GFP_NOWAIT);
	if(!sched_work)
		return;

	INIT_WORK(&sched_work->work, sched_attr_work_func);
	sched_work->task = task;
	sched_work->toSet = toSet;
	memcpy(&sched_work->attr, attr, sizeof(struct sched_attr));
	queue_work(uclamp_wq, &sched_work->work);
}

static void doUclamp(struct task_struct *to, int max, int min, int toSet)
{
	struct sched_attr attr = {};

	attr.sched_policy = -1;
	attr.sched_flags =
		SCHED_FLAG_KEEP_ALL |
		SCHED_FLAG_UTIL_CLAMP |
		SCHED_FLAG_RESET_ON_FORK;
	attr.sched_util_min = min;
	attr.sched_util_max = max;

	if(likely(to)) {
		attr.sched_policy = to->policy;
		if(rt_policy(to->policy))
			attr.sched_priority = to->rt_priority;
		set_sched_attr_non_blocking(to, &attr, toSet);
	}
}

int write_uclamp_to_list(pid_t pid)
{
	unsigned long flags;
	struct uclamp_data_node *newNode = kmalloc(sizeof(struct uclamp_data_node), GFP_NOWAIT);

	if(!newNode)
		return -ENOMEM;

	newNode->pid = pid;
	INIT_LIST_HEAD(&newNode->list);
	spin_lock_irqsave(&binder_uclamp_lock, flags);
	list_add_tail(&newNode->list, &uclamp_data_head);
	spin_unlock_irqrestore(&binder_uclamp_lock, flags);
	return 0;
}

void erase_uclamp_in_list(pid_t pid)
{
	struct list_head *pos, *n;
	struct uclamp_data_node *tmp_node;
	unsigned long flags;

	spin_lock_irqsave(&binder_uclamp_lock, flags);
	list_for_each_safe(pos, n, &uclamp_data_head){
		tmp_node = list_entry(pos, struct uclamp_data_node, list);
		if(tmp_node->pid == pid){
			list_del(&tmp_node->list);
			kfree(tmp_node);
		}
	}
	spin_unlock_irqrestore(&binder_uclamp_lock, flags);
}

void uclamp_list_clear(void)
{
	struct task_turbo_t *task_turbo_data;
	struct task_struct *task_struct_data;
	struct list_head *pos, *n;
	struct uclamp_data_node *tmp_node;
	unsigned long flags;

	spin_lock_irqsave(&binder_uclamp_lock, flags);
	list_for_each_safe(pos, n, &uclamp_data_head){
		tmp_node = list_entry(pos, struct uclamp_data_node, list);
		rcu_read_lock();
		task_struct_data = find_task_by_vpid(tmp_node->pid);
		if(!task_struct_data){
			rcu_read_unlock();
			continue;
		}
		get_task_struct(task_struct_data);
		rcu_read_unlock();
		task_turbo_data = get_task_turbo_t(task_struct_data);
		if(!task_turbo_data){
			put_task_struct(task_struct_data);
			continue;
		}
		doUclamp(task_struct_data, UCLAMP_MAX_VALUE, UCLAMP_MIN_VALUE, 0);
		task_turbo_data->uclamp_binder_cnt = 0;
		task_turbo_data->is_uclamp_binder = 0;
		task_turbo_data->uclamp_value_max = 0;
		task_turbo_data->uclamp_value_min = 0;
		put_task_struct(task_struct_data);
		list_del(&tmp_node->list);
		kfree(tmp_node);
	}
	spin_unlock_irqrestore(&binder_uclamp_lock, flags);
}

void print_uclamp_list(void)
{
	struct list_head *pos, *n;
	struct uclamp_data_node *tmp_node;
	unsigned long flags;

	spin_lock_irqsave(&binder_uclamp_lock, flags);
	list_for_each_safe(pos, n, &uclamp_data_head){
		tmp_node = list_entry(pos, struct uclamp_data_node, list);
		pr_info("%s: tid: %d", __func__, tmp_node->pid);
	}
	spin_unlock_irqrestore(&binder_uclamp_lock, flags);
}

void do_set_binder_uclamp_param(pid_t pid, int binder_uclamp_max, int binder_uclamp_min)
{
	struct task_turbo_t *task_turbo_data;
	struct task_struct *task_struct_data;

	if(!binder_uclamp_inheritance_enable_authority)
		return;
	trace_binder_uclamp_parameters_set(pid, binder_uclamp_max, binder_uclamp_min);
	if (binder_uclamp_min<UCLAMP_MIN_VALUE || binder_uclamp_min>UCLAMP_MAX_VALUE ||
		binder_uclamp_max<UCLAMP_MIN_VALUE || binder_uclamp_max>UCLAMP_MAX_VALUE)
		return;
	rcu_read_lock();
	task_struct_data = find_task_by_vpid(pid);
	if(!task_struct_data){
		rcu_read_unlock();
		return;
	}
	get_task_struct(task_struct_data);
	rcu_read_unlock();
	task_turbo_data = get_task_turbo_t(task_struct_data);
	if(!task_turbo_data){
		put_task_struct(task_struct_data);
		return;
	}
	if (!task_turbo_data->is_uclamp_binder){
		if(task_turbo_data->uclamp_binder_cnt == 0){
			if(unlikely(write_uclamp_to_list(pid))){
				put_task_struct(task_struct_data);
				return;
			}
		}
		task_turbo_data->is_uclamp_binder = 1;
	}
	task_turbo_data->uclamp_value_min = binder_uclamp_min;
	task_turbo_data->uclamp_value_max = binder_uclamp_max;
	task_turbo_data->uclamp_binder_cnt++;
	doUclamp(task_struct_data, binder_uclamp_max, binder_uclamp_min, 1);
	put_task_struct(task_struct_data);
}

void do_unset_binder_uclamp_param(int pid)
{
	struct task_turbo_t *task_turbo_data;
	struct task_struct *task_struct_data;

	if (pid < 0 || (!binder_uclamp_inheritance_enable_authority))
		return;

	unset_binder_uclamp_pid = pid;
	trace_binder_uclamp_parameters_set(unset_binder_uclamp_pid, -1, -1);

	rcu_read_lock();
	task_struct_data = find_task_by_vpid(unset_binder_uclamp_pid);
	if(!task_struct_data){
		rcu_read_unlock();
		return;
	}
	get_task_struct(task_struct_data);
	rcu_read_unlock();
	task_turbo_data = get_task_turbo_t(task_struct_data);
	if(!task_turbo_data){
		put_task_struct(task_struct_data);
		return;
	}
	if (task_turbo_data->is_uclamp_binder){
		task_turbo_data->is_uclamp_binder = 0;
		if(task_turbo_data->uclamp_binder_cnt > 0){
			if(--task_turbo_data->uclamp_binder_cnt == 0){
				doUclamp(task_struct_data, UCLAMP_MAX_VALUE, UCLAMP_MIN_VALUE, 0);
				erase_uclamp_in_list(pid);
				task_turbo_data->uclamp_value_min = 0;
				task_turbo_data->uclamp_value_max = 0;
			}
		}
	}
	put_task_struct(task_struct_data);
}

void do_binder_uclamp_stuff(int cmd)
{
	if(!binder_uclamp_inheritance_enable_authority)
		return;
	trace_binder_uclamp_parameters_set(cmd, -2, -2);
	if(cmd == PRINT_UCLAMP_LIST)
		print_uclamp_list();
	else if (cmd == CLEAR_UCLAMP_LIST)
		uclamp_list_clear();
}

void do_enable_binder_uclamp_inheritance(int enable)
{
	if(!binder_uclamp_inheritance_enable_authority)
		return;
	if (enable < 0)
		return;
	if (!enable)
		uclamp_list_clear();
	binder_uclamp_inheritance_enable = !!enable;
}

static int enable_binder_uclamp_inheritance(const char *buf, const struct kernel_param *kp)
{
	int retval = 0, val = 0;

	retval = kstrtoint(buf, 0, &val);
	if (retval)
		return -EINVAL;
	do_enable_binder_uclamp_inheritance(val);
	return retval;
}

static const struct kernel_param_ops enable_binder_uclamp_inheritance_ops = {
	.set = enable_binder_uclamp_inheritance,
	.get = param_get_int,
};

module_param_cb(enable_binder_uclamp_inheritance
		, &enable_binder_uclamp_inheritance_ops, &binder_uclamp_inheritance_enable, 0664);
MODULE_PARM_DESC(enable_binder_uclamp_inheritance, "Enable or disable binder uclamp inheritance");

static int enable_binder_uclamp_inheritance_authority(const char *buf, const struct kernel_param *kp)
{
	int retval = 0, val = 0;

	retval = kstrtoint(buf, 0, &val);
	if (retval)
		return -EINVAL;
	if(!val)
		do_enable_binder_uclamp_inheritance(val);
	binder_uclamp_inheritance_enable_authority = !!val;
	return retval;
}

static const struct kernel_param_ops enable_binder_uclamp_inheritance_authority_ops = {
	.set = enable_binder_uclamp_inheritance_authority,
	.get = param_get_int,
};

module_param_cb(enable_binder_uclamp_inheritance_authority
		, &enable_binder_uclamp_inheritance_authority_ops, &binder_uclamp_inheritance_enable_authority, 0664);
MODULE_PARM_DESC(enable_binder_uclamp_inheritance_authority, "Enable or disable binder_uclamp_inheritance_authority");

static char binder_uclamp_param[64] = "";
static int set_binder_uclamp_param(const char *buf, const struct kernel_param *kp)
{
	pid_t pid = 0;
	int binder_uclamp_min = UCLAMP_MIN_VALUE, binder_uclamp_max = UCLAMP_MAX_VALUE;

	if (sscanf(buf, "%d %d %d", &pid, &binder_uclamp_min, &binder_uclamp_max) != 3)
		return -EINVAL;
	do_set_binder_uclamp_param(pid, binder_uclamp_max, binder_uclamp_min);
	return 0;
}

static const struct kernel_param_ops set_binder_uclamp_param_ops = {
	.set = set_binder_uclamp_param,
	.get = param_get_int,
};
module_param_cb(set_binder_uclamp_param, &set_binder_uclamp_param_ops,
				&binder_uclamp_param, 0664);

static int unset_binder_uclamp_param(const char *buf, const struct kernel_param *kp)
{
	int retval = 0, val = 0;

	retval = kstrtoint(buf, 0, &val);

	if (retval)
		return -EINVAL;

	do_unset_binder_uclamp_param(val);
	return retval;
}

static const struct kernel_param_ops unset_binder_uclamp_param_ops = {
	.set = unset_binder_uclamp_param,
	.get = param_get_int,
};

module_param_cb(unset_binder_uclamp_param
		, &unset_binder_uclamp_param_ops, &unset_binder_uclamp_pid, 0664);
MODULE_PARM_DESC(unset_binder_uclamp_param, "unset binder uclamp parameters");

static int binder_uclamp_stuff(const char *buf, const struct kernel_param *kp)
{
	int retval = 0, val = 0;

	retval = kstrtoint(buf, 0, &val);
	if (retval)
		return -EINVAL;
	do_binder_uclamp_stuff(val);
	return retval;
}

static const struct kernel_param_ops binder_uclamp_stuff_ops = {
	.set = binder_uclamp_stuff,
	.get = param_get_int,
};

module_param_cb(binder_uclamp_stuff
		, &binder_uclamp_stuff_ops, &binder_stuff_cmd, 0664);
MODULE_PARM_DESC(binder_uclamp_stuff, "unset binder uclamp parameters");

int write_vipServer_to_list(pid_t proc_pid, pid_t pid)
{
	unsigned long flags;
	struct vipServer_data_node *newNode = kmalloc(sizeof(struct vipServer_data_node), GFP_NOWAIT);

	if(!newNode)
		return -ENOMEM;

	newNode->proc_pid = proc_pid;
	newNode->pid = pid;
	INIT_LIST_HEAD(&newNode->list);
	spin_lock_irqsave(&binder_vipServer_lock, flags);
	list_add_tail(&newNode->list, &vipServer_data_head);
	spin_unlock_irqrestore(&binder_vipServer_lock, flags);
	return 0;
}

void reset_and_erase_proc_vipServer_in_list(pid_t proc_pid)
{
	struct task_turbo_t *task_turbo_data;
	struct task_struct *task_struct_data;
	struct list_head *pos, *n;
	struct vipServer_data_node *tmp_node;
	unsigned long flags;

	spin_lock_irqsave(&binder_vipServer_lock, flags);
	list_for_each_safe(pos, n, &vipServer_data_head){
		tmp_node = list_entry(pos, struct vipServer_data_node, list);
		if((proc_pid != 0) && (tmp_node->proc_pid != proc_pid))
			continue;
		rcu_read_lock();
		task_struct_data = find_task_by_vpid(tmp_node->pid);
		if(!task_struct_data){
			rcu_read_unlock();
			list_del(&tmp_node->list);
			kfree(tmp_node);
			continue;
		}
		get_task_struct(task_struct_data);
		rcu_read_unlock();
		task_turbo_data = get_task_turbo_t(task_struct_data);
		if(!task_turbo_data){
			put_task_struct(task_struct_data);
			list_del(&tmp_node->list);
			kfree(tmp_node);
			continue;
		}
		if(task_turbo_data->is_binder_vip_server_done){
			task_turbo_data->is_binder_vip_server_done = 0;
			unset_task_basic_vip(tmp_node->pid);
			trace_binder_vip_server_vip_set(tmp_node->pid, 0);
		}
		put_task_struct(task_struct_data);
		list_del(&tmp_node->list);
		kfree(tmp_node);
	}
	spin_unlock_irqrestore(&binder_vipServer_lock, flags);
}

void check_dirty_vipServer_in_list(void)
{
	struct task_struct *task_struct_data;
	struct list_head *pos, *n;
	struct vipServer_data_node *tmp_node;
	unsigned long flags;

	spin_lock_irqsave(&binder_vipServer_lock, flags);
	list_for_each_safe(pos, n, &vipServer_data_head){
		tmp_node = list_entry(pos, struct vipServer_data_node, list);
		rcu_read_lock();
		task_struct_data = find_task_by_vpid(tmp_node->pid);
		if(!task_struct_data){
			list_del(&tmp_node->list);
			kfree(tmp_node);
		}
		rcu_read_unlock();
	}
	spin_unlock_irqrestore(&binder_vipServer_lock, flags);
}

void print_vipServer_list(void)
{
	struct list_head *pos, *n;
	struct vipServer_data_node *tmp_node;
	unsigned long flags;

	spin_lock_irqsave(&binder_vipServer_lock, flags);
	list_for_each_safe(pos, n, &vipServer_data_head){
		tmp_node = list_entry(pos, struct vipServer_data_node, list);
		pr_info("%s: %d:%d", __func__, tmp_node->proc_pid, tmp_node->pid);
	}
	spin_unlock_irqrestore(&binder_vipServer_lock, flags);
}


void do_binder_vipServer_stuff(int cmd)
{
	if(cmd == PRINT_UCLAMP_LIST)
		print_vipServer_list();
	else if (cmd == CLEAR_UCLAMP_LIST)
		reset_and_erase_proc_vipServer_in_list(0);
}

void do_enable_binder_vipServer(int enable)
{
	if (!enable)
		reset_and_erase_proc_vipServer_in_list(0);
	binder_vipServer_enable = !!enable;
}
EXPORT_SYMBOL_GPL(do_enable_binder_vipServer);

int do_set_server_vip(pid_t pid, int enable)
{
	struct task_turbo_t *task_turbo_data;
	struct task_struct *task_struct_data;

	if (pid <= 0)
		return -EINVAL;
	if (enable < 0)
		return -EINVAL;

	check_dirty_vipServer_in_list();
	trace_binder_vip_server_parameters_set(pid, enable);
	rcu_read_lock();
	task_struct_data = find_task_by_vpid(pid);
	if(!task_struct_data){
		rcu_read_unlock();
		return -ESRCH;
	}
	get_task_struct(task_struct_data);
	rcu_read_unlock();
	task_turbo_data = get_task_turbo_t(task_struct_data);
	if(!task_turbo_data){
		put_task_struct(task_struct_data);
		return -ESRCH;
	}
	task_turbo_data->is_binder_vip_server = !!enable;
	if(!enable)
		reset_and_erase_proc_vipServer_in_list(pid);
	put_task_struct(task_struct_data);
	return 0;
}
EXPORT_SYMBOL_GPL(do_set_server_vip);

static int binder_vipServer_stuff(const char *buf, const struct kernel_param *kp)
{
	int retval = 0, val = 0;

	retval = kstrtoint(buf, 0, &val);
	if (retval)
		return -EINVAL;
	do_binder_vipServer_stuff(val);
	return retval;
}

static const struct kernel_param_ops binder_vipServer_stuff_ops = {
	.set = binder_vipServer_stuff,
	.get = param_get_int,
};

module_param_cb(binder_vipServer_stuff
		, &binder_vipServer_stuff_ops, &binder_stuff_cmd, 0664);
MODULE_PARM_DESC(binder_vipServer_stuff, "unset binder vipServer parameters");

static int enable_binder_vipServer(const char *buf, const struct kernel_param *kp)
{
	int retval = 0, val = 0;

	retval = kstrtoint(buf, 0, &val);
	if (retval)
		return -EINVAL;
	if (val < 0)
		return -EINVAL;
	do_enable_binder_vipServer(val);
	return retval;
}

static const struct kernel_param_ops enable_binder_vipServer_ops = {
	.set = enable_binder_vipServer,
	.get = param_get_int,
};

module_param_cb(enable_binder_vipServer
		, &enable_binder_vipServer_ops, &binder_vipServer_enable, 0664);
MODULE_PARM_DESC(enable_binder_vipServer, "Enable or disable binder vip server");

static char binder_server_vip_param[64] = "";
static int set_server_vip(const char *buf, const struct kernel_param *kp)
{
	pid_t pid = 0;
	int enable = 0, retval = 0;

	if (sscanf(buf, "%d %d", &pid, &enable) != 2)
		return -EINVAL;
	retval = do_set_server_vip(pid, enable);
	return retval;
}

static const struct kernel_param_ops set_server_vip_ops = {
	.set = set_server_vip,
	.get = param_get_int,
};

module_param_cb(set_server_vip
		, &set_server_vip_ops, &binder_server_vip_param, 0664);
MODULE_PARM_DESC(set_server_vip, "set server vip");

/*
 * update_cpu_loading - Update the average CPU loading
 *
 * Retrieves the CPU loading (CPU active time / wall time duration) and
 * updates the average CPU loading across all CPUs. The value is then used
 * to determine if tasks should be promoted to VIP status based on CPU load.
 */
static void update_cpu_loading(void)
{
	int ret = 0, i = 0;

	ret = get_cpu_loading(&ci);

	if (ret < 0)
		return;

	mutex_lock(&cpu_loading_lock);
	avg_cpu_loading = 0;
	for (i = 0; i < max_cpus; i++)
		avg_cpu_loading += ci.cpu_loading[i];
	avg_cpu_loading /= max_cpus;
	mutex_unlock(&cpu_loading_lock);
}

unsigned long capacity_of(int cpu)
{
	return cpu_rq(cpu)->cpu_capacity;
}

unsigned long cpu_cap_ceiling(int cpu)
{
	unsigned long cap_ceiling;

	cap_ceiling = min_t(unsigned long, arch_scale_cpu_capacity(cpu),
		get_cpu_gear_uclamp_max_capacity(cpu, GU_RET_UTIL));
	return clamp_t(unsigned long, cap_ceiling,
		READ_ONCE(per_cpu(min_freq_scale, cpu)), READ_ONCE(per_cpu(max_freq_scale, cpu)));
}

unsigned long get_tgid_util(int tgid)
{
	struct task_struct *p, *t;
	unsigned long vip_total_util = 0;
	int i = 0;

	rcu_read_lock();
	p = find_task_by_vpid(tgid);
	if (!p) {
		rcu_read_unlock();
		return 0;
	}

	get_task_struct(p);
	for_each_thread(p, t) {
		i++;
		if (i > MAX_NR_THREAD)
			break;
		get_task_struct(t);
		if (READ_ONCE(t->__state) == TASK_RUNNING)
			vip_total_util += task_util(t);
		put_task_struct(t);
	}
	put_task_struct(p);
	rcu_read_unlock();

	return vip_total_util;
}
EXPORT_SYMBOL(get_tgid_util);

bool is_util_overhigh(unsigned long util)
{
	int cpu;
	unsigned long total_capacity = 0;

	for_each_online_cpu(cpu) {
		total_capacity += min(cpu_cap_ceiling(cpu), capacity_of(cpu));
	}
	if (util*100 > vip_util_thres*total_capacity)
		return true;

	return false;
}
EXPORT_SYMBOL(is_util_overhigh);

static inline void cpumask_complement(struct cpumask *dstp,
				      const struct cpumask *srcp)
{
	bitmap_complement(cpumask_bits(dstp), cpumask_bits(srcp),
					      nr_cpumask_bits);
}

int tt_cpu_count_check(void)
{
	cpumask_t count_mask = CPU_MASK_NONE;

	cpumask_complement(&count_mask, cpu_pause_mask);
	cpumask_and(&count_mask, &count_mask, cpu_online_mask);

	return cpumask_weight(&count_mask);
}

/*
 * tt_vip - Task Turbo VIP management routine
 *
 * Manages the VIP status of tasks based on various conditions such as CPU load,
 * enforced VIP status, and touch input. It sets or clears the VIP status for
 * tasks and logs these actions for debugging purposes.
 */
static void tt_vip(void)
{
	bool ct_vip_qualified = false;
	bool launch_vip_qualified = false;
	bool touching = false;
	int tmp_avg_cpu_loading = 0;

	/* The effect after touch ends lasts for TOUCH_SUSTAIN_MS milliseconds */
	if (ktime_to_ms(ktime_get() - cur_touch_time) < TOUCH_SUSTAIN_MS)
		touching = true;

	mutex_lock(&cpu_loading_lock);
	tmp_avg_cpu_loading = avg_cpu_loading;
	mutex_unlock(&cpu_loading_lock);
	ct_vip_qualified = tmp_avg_cpu_loading >= cpu_loading_thres;

	mutex_lock(&enforced_qualified_lock);
	/* enforced_qualified_mask can forcibly make critical task(s) to VIP */
	if (enforced_qualified_mask)
		ct_vip_qualified = true;
	if (tt_cpu_count_check() <= MIN_CPUS)
		ct_vip_qualified = false;
	mutex_unlock(&enforced_qualified_lock);

#if IS_ENABLED(CONFIG_MTK_TASK_TURBO)
	launch_vip_qualified = (launch_turbo_enable() && ct_vip_qualified);
#endif
	mutex_lock(&wi_lock);
	if (tt_vip_algo_hook)
		tt_vip_algo_hook(ct_vip_qualified, launch_vip_qualified, ssid_tgid, sui_tgid, f_tgid, touching);

	mutex_unlock(&wi_lock);
}
module_param(cpu_loading_thres, int, 0644);
module_param(vip_util_thres, int, 0644);

/**
 * tt_vip_event_handler - Event-triggered work handler for VIP management
 *
 * This function is designed to be called in response to specific events that require
 * immediate attention to VIP status management.
 */
static void tt_vip_event_handler(struct work_struct *work)
{
	tt_vip();
}

/*
 * tt_vip_periodic_handler - Periodic work handler for VIP management
 *
 * This function is invoked periodically, to perform regular updates
 * of the CPU loading information and TGID.
 * After updating the CPU loading, it calls the tt_vip function
 * to manage the VIP status.
 */
static void tt_vip_periodic_handler(struct work_struct *work)
{
	struct task_struct *p;

	if (is_target_found_hook) {
		rcu_read_lock();
		p = find_task_by_vpid(f_tgid);
		if (!p || !is_target_found_hook(p->comm, 3)) {
			f_tgid = 0;
			for_each_process(p) {
				if (is_target_found_hook(p->comm, 3))
					f_tgid = p->tgid;
			}
		}
		rcu_read_unlock();
	}
	update_cpu_loading();
	tt_vip();
}

bool is_task_exiting(struct task_struct *task)
{
	return task->exit_state == EXIT_ZOMBIE || task->exit_state == EXIT_DEAD;
}

/*
 * tt_input_event - Handles touch input events for VIP management
 * @handle: input handle associated with the event
 * @type: type of input event (e.g., EV_KEY for key events)
 * @code: event code (e.g., BTN_TOUCH for touch events)
 * @value: value of the event (e.g., TOUCH_DOWN)
 *
 * This function is invoked upon receiving input events and serves two primary purposes:
 * 1. Updates the `cur_touch_time` with the current time for all types of touch events. This
 *	timestamp is used to determine if touch-based VIP management should be active, allowing
 *	for VIP status to be maintained for a period after the last touch event.
 * 2. Specifically for TOUCH_DOWN events (indicating the start of a touch), it triggers the
 *	VIP management logic if the VIP feature is enabled.
 *	This is done by finding the `ss` task (if not already known) and
 *	updating the `ssid_tgid` with its task group ID, then queuing the VIP management
 *	work (`tt_vip_worker`).
 *
 * The effect of this logic is to potentially elevate tasks to VIP status from the moment of touch
 * down and to continue considering them for VIP status for up to TOUCH_SUSTAIN_MS milliseconds after
 * the last touch event. This approach aims to ensure that the system remains responsive during
 * and shortly after user interactions.
 */
static void tt_input_event(struct input_handle *handle, unsigned int type,
						   unsigned int code, int value)
{
	struct task_struct *p;
	ktime_t diff = 0;

	cur_touch_time = ktime_get();
	if (tt_vip_enable == VIPE_MODE_ON && type == EV_KEY && code == BTN_TOUCH && value == TOUCH_DOWN) {
		diff = cur_touch_time - cur_touch_down_time;
		cur_touch_down_time = cur_touch_time;
		if (diff >= TOUCH_SUSTAIN_MS) {
			if (!is_target_found_hook || is_task_exiting(current))
				goto hook_unready;

			rcu_read_lock();
			p = find_task_by_vpid(ssid_tgid);
			if (!p || !is_target_found_hook(p->comm, 1)) {
				ssid_tgid = 0;
				for_each_process(p) {
					if (is_target_found_hook(p->comm, 1))
						ssid_tgid = p->tgid;
				}
			}
			rcu_read_unlock();
			rcu_read_lock();
			p = find_task_by_vpid(sui_tgid);
			if (!p || !is_target_found_hook(p->comm, 2)) {
				sui_tgid = 0;
				for_each_process(p) {
					if (is_target_found_hook(p->comm, 2))
						sui_tgid = p->tgid;
				}
			}
			rcu_read_unlock();
hook_unready:
		if (ssid_tgid > 0 || sui_tgid > 0)
			queue_work(system_highpri_wq, &tt_vip_worker);
		}
	}
}

static int tt_input_connect(struct input_handler *handler,
		struct input_dev *dev,
		const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);

	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "tt_touch_handle";

	error = input_register_handle(handle);

	if (error)
		goto err2;

	error = input_open_device(handle);

	if (error)
		goto err1;

	return 0;
err1:
	input_unregister_handle(handle);
err2:
	kfree(handle);
	return error;
}

static void tt_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id tt_input_ids[] = {
	{.driver_info = 1},
	{},
};

static struct input_handler tt_input_handler = {
	.event = tt_input_event,
	.connect = tt_input_connect,
	.disconnect = tt_input_disconnect,
	.name = "tt_input_handler",
	.id_table = tt_input_ids,
};

static void probe_android_rvh_prepare_prio_fork(void *ignore, struct task_struct *p)
{
	init_turbo_attr(p);
}

#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
bool binder_start_vip_inherit(struct task_struct *from,
					struct task_struct *to)
{
	struct task_turbo_t *to_turbo_data;
	struct vip_task_struct *vts_from;
	struct vip_task_struct *vts_to;
	int inherited_vip_prio;
	int to_pid;

	if (!from || !to)
		goto done;

	inherited_vip_prio = get_vip_task_prio(from);
	vts_from = get_vip_t(from);
	if (inherited_vip_prio == NOT_VIP) {
		if (!rt_task(from))
			goto done;
		if (is_VVIP(vts_from))
			inherited_vip_prio = VVIP;
		else if (is_priority_based_vip(vts_from))
			inherited_vip_prio = vts_from->priority_based_prio;
		else if (is_VIP_basic(vts_from))
			inherited_vip_prio = WORKER_VIP;
		else
			goto done;
	}
	to_turbo_data = get_task_turbo_t(to);
	if (to_turbo_data->vip_prio_backup != 0)
		goto done;

	if (get_vip_task_prio(to) == inherited_vip_prio)
		goto done;

	vts_to = get_vip_t(to);
	if (inherited_vip_prio == VVIP) {
		if (is_VVIP(vts_to))
			goto done;
	} else if (inherited_vip_prio == WORKER_VIP) {
		if (is_VIP_basic(vts_to))
			goto done;
	} else {
		if (is_priority_based_vip(vts_to))
			goto done;
	}
	to_pid = to->pid;
	to_turbo_data->vip_prio_backup = inherited_vip_prio + VIP_PRIO_OFFSET;
	trace_binder_vip_set(from->pid, to_pid, inherited_vip_prio, vts_from->throttle_time,
					get_vip_task_prio(to), vts_to->throttle_time);
	binder_start_vip_inherit_hook(to_pid, inherited_vip_prio);
	vts_to->throttle_time = vts_from->throttle_time;
	return true;
done:
	return false;
}

void binder_stop_vip_inherit(struct task_struct *p)
{
	struct task_turbo_t *turbo_data;
	int pid;
	int inherited_vip_prio;

	turbo_data = get_task_turbo_t(p);
	inherited_vip_prio = turbo_data->vip_prio_backup;
	if (inherited_vip_prio == 0)
		return;
	inherited_vip_prio -= VIP_PRIO_OFFSET;
	pid = p->pid;
	trace_binder_vip_restore(pid, inherited_vip_prio);
	binder_stop_vip_inherit_hook(pid, inherited_vip_prio);
	turbo_data->vip_prio_backup = 0;
}
#endif

void binder_start_uclamp_inherit(struct task_struct *from,
					struct task_struct *to)
{
	struct task_turbo_t *from_turbo_data;
	struct task_turbo_t *to_turbo_data;
	int from_min, from_max;

	if (!from || !to)
		return;

	from_turbo_data = get_task_turbo_t(from);
	if (from_turbo_data->uclamp_binder_cnt == 0)
		return;
	from_min = from_turbo_data->uclamp_value_min;
	from_max = from_turbo_data->uclamp_value_max;
	trace_binder_start_uclamp_inherit(from->pid, to->pid, from_max, from_min);

	if (from_min<UCLAMP_MIN_VALUE || from_min>UCLAMP_MAX_VALUE ||
		from_max<UCLAMP_MIN_VALUE || from_max>UCLAMP_MAX_VALUE)
		return;

	to_turbo_data = get_task_turbo_t(to);
	if(to_turbo_data->uclamp_binder_cnt==0){
		if(unlikely(write_uclamp_to_list(to->pid)))
			return;
	}
	to_turbo_data->uclamp_value_min = from_min;
	to_turbo_data->uclamp_value_max = from_max;
	to_turbo_data->uclamp_binder_cnt++;
	doUclamp(to, from_max, from_min, 1);
}

void binder_stop_uclamp_inherit(struct task_struct *p)
{
	struct task_turbo_t *turbo_data;

	turbo_data = get_task_turbo_t(p);
	if(turbo_data->uclamp_binder_cnt>0){
		trace_binder_stop_uclamp_inherit(p->pid);
		if(--turbo_data->uclamp_binder_cnt==0){
			if(turbo_data->is_uclamp_binder){
				turbo_data->uclamp_binder_cnt = 1;
				return;
			}
			turbo_data->uclamp_value_min = 0;
			turbo_data->uclamp_value_max = 0;
			erase_uclamp_in_list(p->pid);
			doUclamp(p, UCLAMP_MAX_VALUE, UCLAMP_MIN_VALUE, 0);
		}
	}
}

void binder_start_set_vipServer(struct task_struct *from,
					struct task_struct *to)
{
	struct task_turbo_t *to_turbo_data;
	struct task_turbo_t *to_proc_turbo_data;

	if (!from || !to)
		return;
	to_turbo_data = get_task_turbo_t(to);
	if(to_turbo_data->is_binder_vip_server_done)
		return;

	to_proc_turbo_data = get_task_turbo_t(to->group_leader);
	if(to_proc_turbo_data->is_binder_vip_server){
		if(unlikely(write_vipServer_to_list(to->tgid, to->pid)))
			return;
		to_turbo_data->is_binder_vip_server_done = 1;
		set_task_basic_vip(to->pid);
		trace_binder_vip_server_vip_set(to->pid, 1);
	}
}


static void probe_android_vh_binder_set_priority(void *ignore, struct binder_transaction *t,
							struct task_struct *task)
{
#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
	if (binder_vip_inheritance_enable && tt_vip_enable == VIPE_MODE_ON && binder_start_vip_inherit_hook)
		binder_start_vip_inherit(t->from ? t->from->task : NULL, task);
#endif
	if (binder_vipServer_enable && tt_vip_enable == VIPE_MODE_ON)
		binder_start_set_vipServer(t->from ? t->from->task : NULL, task);
	if (binder_uclamp_inheritance_enable && binder_uclamp_inheritance_enable_authority)
		binder_start_uclamp_inherit(t->from ? t->from->task : NULL, task);
}

static void probe_android_vh_binder_restore_priority(void *ignore,
		struct binder_transaction *in_reply_to, struct task_struct *cur)
{
#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
	if (cur && binder_stop_vip_inherit_hook)
		binder_stop_vip_inherit(cur);
#endif
	if (cur)
		binder_stop_uclamp_inherit(cur);
}

static void init_turbo_attr(struct task_struct *p)
{
	struct task_turbo_t *turbo_data = get_task_turbo_t(p);

	turbo_data->vip_prio_backup = 0;
	turbo_data->throttle_time_backup = 0;
	turbo_data->is_uclamp_binder = 0;
	turbo_data->uclamp_binder_cnt = 0;
	turbo_data->uclamp_value_min = 0;
	turbo_data->uclamp_value_max = 0;
	turbo_data->is_binder_vip_server = 0;
	turbo_data->is_binder_vip_server_done = 0;
}

int init_cpu_time(void)
{
	int i;

	mutex_lock(&cpu_lock);
	max_cpus = num_possible_cpus();

	ci.cpu_loading = kmalloc_array(max_cpus, sizeof(int), GFP_KERNEL);

	cur_wall_time = kcalloc(max_cpus, sizeof(struct cpu_time), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(cur_wall_time))
		goto err_cur_wall_time;

	cur_idle_time = kcalloc(max_cpus, sizeof(struct cpu_time), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(cur_idle_time))
		goto err_cur_idle_time;

	prev_wall_time = kcalloc(max_cpus, sizeof(struct cpu_time), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(prev_wall_time))
		goto err_prev_wall_time;

	prev_idle_time = kcalloc(max_cpus, sizeof(struct cpu_time), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(prev_idle_time))
		goto err_prev_idle_time;

	for_each_possible_cpu(i) {
		prev_wall_time[i].time = cur_wall_time[i].time = 0;
		prev_idle_time[i].time = cur_idle_time[i].time = 0;
	}
	mutex_unlock(&cpu_lock);
	return 0;

err_prev_idle_time:
	kfree(prev_wall_time);
err_prev_wall_time:
	kfree(cur_idle_time);
err_cur_idle_time:
	kfree(cur_wall_time);
err_cur_wall_time:
	pr_debug(TAG "%s failed to alloc cpu time", __func__);
	mutex_unlock(&cpu_lock);
	return -ENOMEM;
}

int init_tt_input_handler(void)
{
	cur_touch_time = 0;
	cur_touch_down_time = 0;
	return input_register_handler(&tt_input_handler);
}

static inline bool tt_vip_do_check(u64 wallclock)
{
	bool do_check = false;
	unsigned long flags;

	/* check interval */
	spin_lock_irqsave(&check_lock, flags);
	if ((s64)(wallclock - checked_timestamp)
			>= (s64)(1000 * NSEC_PER_MSEC)) {
		checked_timestamp = wallclock;
		do_check = true;
	}
	spin_unlock_irqrestore(&check_lock, flags);

	return do_check;
}

static void tt_tick(void *data, struct rq *rq)
{
	u64 wallclock;

	if (tt_vip_enable == VIPE_MODE_ON) {
		wallclock = ktime_get_ns();
		if (!tt_vip_do_check(wallclock))
			return;

		queue_work(system_highpri_wq, &tt_vip_periodic_worker);
	}
}

int set_task_priority(struct task_struct *task, int prio)
{

	struct sched_param param;
	int policy;

	if (prio < 0 || prio >= MAX_NORMAL_PRIO)
		return -EINVAL;

	if (!task)
		return -ESRCH;

	if (prio < MAX_RT_PRIO) {
		policy = SCHED_FIFO;
		param.sched_priority = prio;
		if (sched_setscheduler_nocheck(task, policy, &param) != 0)
			return -EINVAL;
	} else {
		policy = SCHED_NORMAL;
		param.sched_priority = 0;
		if (sched_setscheduler_nocheck(task, policy, &param) != 0)
			return -EINVAL;

		set_user_nice(task, prio - 120);
	}
	return 0;
}
EXPORT_SYMBOL(set_task_priority);
/* end of task rt prio interface */

void set_vip_ctrl_node(int pid, int vip_prio, unsigned int throttle_time)
{
	if (tt_vip_enable == VIPE_MODE_OFF)
		return;

	switch (vip_prio) {
	case WORKER_VIP:
		if (set_td_hook)
			set_td_hook(pid);
		break;
	case MIN_PRIORITY_BASED_VIP ... MAX_PRIORITY_BASED_VIP:
		set_task_priority_based_vip_and_throttle(pid, vip_prio, throttle_time);
		break;
	case VVIP:
		set_task_vvip_and_throttle(pid, throttle_time);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL_GPL(set_vip_ctrl_node);

void unset_vip_ctrl_node(int pid, int vip_prio)
{
	switch (vip_prio) {
	case WORKER_VIP:
		if (unset_td_hook)
			unset_td_hook(pid);
		break;
	case MIN_PRIORITY_BASED_VIP ... MAX_PRIORITY_BASED_VIP:
		unset_task_priority_based_vip(pid);
		break;
	case VVIP:
		unset_task_vvip(pid);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL_GPL(unset_vip_ctrl_node);

static char set_tdp_param[64] = "";
static int set_tdp(const char *buf, const struct kernel_param *kp)
{
	pid_t pid;
	int prio;
	struct task_struct *task;

	if (sscanf(buf, "%d %d", &pid, &prio) != 2)
		return -EINVAL;

	task = find_task_by_vpid(pid);
	if (!task)
		return -ESRCH;

	return set_task_priority(task, prio);
}

static const struct kernel_param_ops set_tdp_ops = {
	.set = set_tdp,
	.get = param_get_int,
};

module_param_cb(set_tdp, &set_tdp_ops, &set_tdp_param, 0664);
MODULE_PARM_DESC(set_tdp, "set task priority for debug");

static int platform_vip_engine_probe(struct platform_device *pdev)
{
	int ret = 0, retval = 0;

	pr_info("%s called, read vip-enable\n", __func__);
	ret = of_property_read_u32(pdev->dev.of_node,
				"vip-enable", &retval);
	if (!ret)
		tt_vip_enable = retval;
	else
		pr_info("%s unable to get vip-enable\n", __func__);
	return 0;
}

static const struct of_device_id platform_vip_engine_of_match[] = {
	{ .compatible = "mediatek,vip-engine", },
	{},
};

static const struct platform_device_id platform_vip_engine_id_table[] = {
	{"vip-engine", 0},
	{ },
};

static struct platform_driver mtk_platform_vip_engine_driver = {
	.probe = platform_vip_engine_probe,
	.driver = {
		.name = "vip-engine",
		.owner = THIS_MODULE,
		.of_match_table = platform_vip_engine_of_match,
	},
	.id_table = platform_vip_engine_id_table,
};

void vip_loom_select_task_rq_fair(struct task_struct *p, int *target_cpu, int *flag)
{
#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
	int vip_prio = get_vip_task_prio(p);
	bool is_vip = prio_is_vip(vip_prio, NOT_VIP);

	if (is_vip)
		return;
#endif

	if (atomic_read(&vip_loom_select_cfg))
		*flag = ORIGINAL_PATH;
}

int vip_loom_select_cfg_apply(int val, int caller_id)
{
	if (!is_device_support_loom())
		return -EINVAL;

	if (val < 0 || caller_id < 0 || caller_id >= MAX_TYPE)
		return -EINVAL;

	atomic_set(&vip_loom_select_cfg, !!val);

	trace_vip_loom("select_cfg", !!val, caller_id_desc[caller_id]);

	return 0;
}
EXPORT_SYMBOL(vip_loom_select_cfg_apply);

#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
int vip_loom_flt_cfg_apply(int val, int caller_id)
{
	if (!is_device_support_loom())
		return -EINVAL;

	if (val < 0 || caller_id < 0 || caller_id >= MAX_TYPE)
		return -EINVAL;

	atomic_set(&vip_loom_flt_cfg, !!val);

	if (atomic_read(&vip_loom_flt_cfg) && flt_get_mode() != FLT_MODE_0) {
		if (flt_orig_mode == -1)
			flt_orig_mode = flt_get_mode();
		flt_set_mode(FLT_MODE_0);
	} else if (!atomic_read(&vip_loom_flt_cfg) && flt_get_mode() != flt_orig_mode) {
		if (flt_orig_mode != -1)
			flt_set_mode(flt_orig_mode);
	}

	trace_vip_loom("flt_cfg", !!val, caller_id_desc[caller_id]);

	return 0;
}
EXPORT_SYMBOL(vip_loom_flt_cfg_apply);
#endif

#if IS_ENABLED(CONFIG_MTK_CORE_CTL)
static int loom_set_affinity(int pid, int aff_cpu)
{
	struct task_struct *p;
	struct cpumask aff_mask;
	int cpu = 0, ret = 0;

	if (!atomic_read(&loom_cpu_dedicated_enable))
		return -EINVAL;

	if (pid < 0 || pid >= PID_MAX_DEFAULT)
		return -EINVAL;

	if (aff_cpu >= MAX_NR_CPUS || aff_cpu < LOOM_NO_AFFINITY)
		return -EINVAL;

	cpumask_clear(&aff_mask);

	/* Unset affinity */
	if (aff_cpu == LOOM_NO_AFFINITY) {
		for_each_possible_cpu(cpu)
			cpumask_set_cpu(cpu, &aff_mask);
	} else
		cpumask_set_cpu(aff_cpu, &aff_mask);

	/* find task struct by pid */
	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (!p) {
		rcu_read_unlock();
		return -ESRCH;
	}

	/* prevent p going away */
	get_task_struct(p);
	rcu_read_unlock();

	/* task not allowed to set affinity */
	if (p->flags & PF_NO_SETAFFINITY) {
		ret = -EINVAL;
		goto out_put_task;
	}

	if (loom_set_cpus_allowed_ptr_by_kernel_fp)
		ret = loom_set_cpus_allowed_ptr_by_kernel_fp(p, &aff_mask);
	else
		ret = set_cpus_allowed_ptr(p, &aff_mask);

out_put_task:
	put_task_struct(p);
	return ret;
}

static int loom_unset_affinity_buf(unsigned int index)
{
	int ret = 0;
	unsigned long flags;
	int pid, cpu;

	spin_lock_irqsave(&loom_affinity_lock, flags);
	pid = loom_rec_buffer[index].pid;
	cpu = loom_rec_buffer[index].aff_cpu;
	loom_rec_buffer[index].pid = LOOM_NO_TASK;
	loom_rec_buffer[index].aff_cpu = LOOM_NO_AFFINITY;
	spin_unlock_irqrestore(&loom_affinity_lock, flags);

	if (cpu != LOOM_NO_AFFINITY) {
		ret = core_ctl_force_pause_request(cpu, false, LOOM_FORCE_PAUSE);
		if (ret < 0) {
			pr_info("%s: Fail to reset buffer[%u] with force resume CPU#%d.",
				__func__, index, cpu);
			return ret;
		}
	}

	if (pid != LOOM_NO_TASK) {
		ret = loom_set_affinity(pid, LOOM_NO_AFFINITY);
		if (ret < 0) {
			pr_info("%s: Fail to reset buffer[%u] with pid=%d affinity.",
				__func__, index, pid);
			return ret;
		}
	}

	return ret;
}

static int loom_update_affinity_buf(int pid, int aff_cpu)
{
	int ret = 0;
	unsigned long flags;
	unsigned int now_update_idx;
	static unsigned int update_idx;

	spin_lock_irqsave(&loom_affinity_lock, flags);
	now_update_idx = update_idx;
	update_idx = (update_idx + 1) % MAX_LOOM_BUF_SIZE;
	spin_unlock_irqrestore(&loom_affinity_lock, flags);

	/* remove old PID and CPU affinity with buffer */
	ret = loom_unset_affinity_buf(now_update_idx);

	/* update buffer */
	spin_lock_irqsave(&loom_affinity_lock, flags);
	loom_rec_buffer[now_update_idx].pid = pid;
	loom_rec_buffer[now_update_idx].aff_cpu = aff_cpu;
	spin_unlock_irqrestore(&loom_affinity_lock, flags);

	ret = core_ctl_force_pause_request(aff_cpu, true, LOOM_FORCE_PAUSE);
	if (ret < 0) {
		spin_lock_irqsave(&loom_affinity_lock, flags);
		loom_rec_buffer[now_update_idx].pid = LOOM_NO_TASK;
		loom_rec_buffer[now_update_idx].aff_cpu = LOOM_NO_AFFINITY;
		spin_unlock_irqrestore(&loom_affinity_lock, flags);
		pr_info("%s: Fail to set pid=%d into record buffer[%u] with CPU#%d, reset it.\n",
			__func__ , pid, now_update_idx, aff_cpu);
	} else {
		pr_info("%s: Success to set pid=%d into record buffer[%u] with CPU#%d\n",
			__func__ , pid, now_update_idx, aff_cpu);
	}

	return ret;
}

static int loom_specify_pause(int pid, int aff_cpu)
{
	unsigned long flags;
	int i, ret = 0;
	bool dup_set = false;
	int rec_pid[MAX_LOOM_BUF_SIZE], rec_aff_cpu[MAX_LOOM_BUF_SIZE];
	int tmp_pid, tmp_aff_cpu;

	if ((aff_cpu >= MAX_NR_CPUS) || (aff_cpu < LOOM_NO_AFFINITY) || aff_cpu == 0)
		return -EINVAL;

	if (pid < 0 || pid >= PID_MAX_DEFAULT)
		return -EINVAL;

	/* check already assigned CPU */
	spin_lock_irqsave(&loom_affinity_lock, flags);
	for(i=0; i<MAX_LOOM_BUF_SIZE; i++) {
		tmp_pid = loom_rec_buffer[i].pid;
		tmp_aff_cpu = loom_rec_buffer[i].aff_cpu;

		if (tmp_aff_cpu == LOOM_NO_AFFINITY)
			continue;

		if (tmp_aff_cpu == aff_cpu) {
			/* set already setting again */
			if (tmp_pid == pid) {
				dup_set = true;
				break;
			}

			pr_info("%s: CPU#%d is already bound to pid=%d\n",
				__func__, tmp_aff_cpu, tmp_pid);
			ret = -EINVAL;
			break;
		}
	}
	spin_unlock_irqrestore(&loom_affinity_lock, flags);
	if (dup_set) {
		ret = core_ctl_force_pause_request(aff_cpu, true, LOOM_FORCE_PAUSE);
		goto print_log;
	}
	/* CPU already been bound or force paused */
	if (ret < 0)
		return ret;

	/* remove existing pid node */
	for(i=0; i<MAX_LOOM_BUF_SIZE; i++) {
		spin_lock_irqsave(&loom_affinity_lock, flags);
		tmp_pid = loom_rec_buffer[i].pid;
		tmp_aff_cpu = loom_rec_buffer[i].aff_cpu;
		spin_unlock_irqrestore(&loom_affinity_lock, flags);

		if (tmp_pid == pid)
			ret = loom_unset_affinity_buf(i);
	}

	/* Add new node to buffer */
	if (aff_cpu != LOOM_NO_AFFINITY)
		ret = loom_update_affinity_buf(pid, aff_cpu);

print_log:
	for(i=0; i<MAX_LOOM_BUF_SIZE; i++) {
		spin_lock_irqsave(&loom_affinity_lock, flags);
		rec_pid[i] = loom_rec_buffer[i].pid;
		rec_aff_cpu[i] = loom_rec_buffer[i].aff_cpu;
		spin_unlock_irqrestore(&loom_affinity_lock, flags);
	}

	trace_loom_bind_to_specify_cpu(atomic_read(&loom_cpu_dedicated_enable),
		rec_pid, rec_aff_cpu, dup_set, ret);
	return ret;
}

int loom_ctask_cpu_dedicated(int pid, int aff_cpu)
{
	int ret = 0;

	if (!is_device_support_loom())
		return -EINVAL;

	if (!atomic_read(&loom_cpu_dedicated_enable))
		return -EINVAL;

	ret = loom_specify_pause(pid, aff_cpu);
	if (ret < 0)
		return ret;

	ret = loom_set_affinity(pid, aff_cpu);
	/* rollback force pause */
	if (ret < 0) {
		loom_specify_pause(pid, LOOM_NO_AFFINITY);
		pr_info("%s: Pause CPU#%d success, but bound affinity pid=%d fail.\n",
			__func__ , aff_cpu, pid);
	}

	return ret;
}
EXPORT_SYMBOL(loom_ctask_cpu_dedicated);

int loom_force_pause(int cpu, bool is_pause)
{
	int i, ret = 0;
	unsigned long flags;
	int tmp_pid, tmp_aff_cpu;

	if (!is_device_support_loom())
		return -EINVAL;

	if (!atomic_read(&loom_cpu_dedicated_enable))
		return -EINVAL;

	/* Not allow to force pause CPU0 */
	if ((cpu >= MAX_NR_CPUS) || (cpu <= 0))
		return -EINVAL;

	/* Unset all already set combination with this CPU */
	for (i=0; i<MAX_LOOM_BUF_SIZE; i++) {
		spin_lock_irqsave(&loom_affinity_lock, flags);
		tmp_pid = loom_rec_buffer[i].pid;
		tmp_aff_cpu = loom_rec_buffer[i].aff_cpu;
		spin_unlock_irqrestore(&loom_affinity_lock, flags);

		if (tmp_aff_cpu == cpu)
			ret = loom_unset_affinity_buf(i);
	}

	/* Add new node to buffer, without pid */
	if (is_pause) {
		ret = loom_update_affinity_buf(LOOM_NO_TASK, cpu);
		if (ret == 0)
			pr_info("%s: Loom force pause success with CPU#%d\n", __func__ , cpu);
	} else
		pr_info("%s: Loom force resume success with CPU#%d\n", __func__ , cpu);

	return ret;
}
EXPORT_SYMBOL(loom_force_pause);

static int get_loom_force_pause(char *buffer, const struct kernel_param *kp)
{
	int len = 0;
	unsigned int force_pause_mask = core_ctl_get_force_pause_mask();

	return scnprintf(buffer + len, PAGE_SIZE - len,
		"force_pause_mask=%x\n", force_pause_mask);
}

static const struct kernel_param_ops loom_force_pause_ops = {
	.set = NULL,
	.get = get_loom_force_pause,
};

module_param_cb(loom_force_pause, &loom_force_pause_ops, NULL, 0664);
MODULE_PARM_DESC(loom_force_pause, "get pause mask status ");

int loom_cpu_dedicated(unsigned int enable)
{
	int i;
	int ret = 0;
	unsigned int old_val = atomic_read(&loom_cpu_dedicated_enable);
	unsigned int new_val;

	/* disable loom affinity feature */
	if (enable == 0) {
		for(i=0; i<MAX_LOOM_BUF_SIZE; i++)
			ret = loom_unset_affinity_buf(i);
	}

	atomic_set(&loom_cpu_dedicated_enable, enable);
	new_val = atomic_read(&loom_cpu_dedicated_enable);

	pr_info("%s: enable from %u to %u\n",
		 __func__, old_val, new_val);

	return ret;
}
EXPORT_SYMBOL(loom_cpu_dedicated);

static int get_loom_cpu_dedicated_enable(char *buffer, const struct kernel_param *kp)
{
	int len = 0;

	return scnprintf(buffer + len, PAGE_SIZE - len,
		"loom_cpu_dedicated_enable=%u\n", atomic_read(&loom_cpu_dedicated_enable));
}

static const struct kernel_param_ops loom_cpu_dedicated_enable_ops = {
	.set = NULL,
	.get = get_loom_cpu_dedicated_enable,
};

module_param_cb(loom_cpu_dedicated_enable, &loom_cpu_dedicated_enable_ops, NULL, 0664);
MODULE_PARM_DESC(loom_cpu_dedicated_enable, "enable API of loom affinity");

static int get_loom_ctask_cpu_dedicated(char *buffer, const struct kernel_param *kp)
{
	int i, len = 0;

	for (i = 0; i < MAX_LOOM_BUF_SIZE; i++) {
		len += scnprintf(buffer + len, PAGE_SIZE - len, "buf[%d]: pid=%d, aff_cpu=%d\n",
			i, loom_rec_buffer[i].pid, loom_rec_buffer[i].aff_cpu);
	}

	len += scnprintf(buffer + len, PAGE_SIZE - len, "\n");
	return len;
}

static const struct kernel_param_ops loom_ctask_cpu_dedicated_ops = {
	.set = NULL,
	.get = get_loom_ctask_cpu_dedicated,
};

module_param_cb(loom_ctask_cpu_dedicated, &loom_ctask_cpu_dedicated_ops, NULL, 0664);
MODULE_PARM_DESC(loom_ctask_cpu_dedicated, "set pid bind to specify cpu for specify usage");
#endif /* CONFIG_MTK_CORE_CTL */

static void loom_affinity_node_init(void)
{
	int i;
	unsigned long flags;

	spin_lock_irqsave(&loom_affinity_lock, flags);
	for (i=0;i<MAX_LOOM_BUF_SIZE;i++) {
		loom_rec_buffer[i].pid = LOOM_NO_TASK;
		loom_rec_buffer[i].aff_cpu = LOOM_NO_AFFINITY;
	}
	spin_unlock_irqrestore(&loom_affinity_lock, flags);
}

static int __init init_vip_engine(void)
{
	int ret, ret_erri_line;

	pr_info("%s called, register platform driver\n", __func__);
	platform_driver_register(&mtk_platform_vip_engine_driver);

	ret = register_trace_android_rvh_prepare_prio_fork(
			probe_android_rvh_prepare_prio_fork, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
		goto failed;
	}

	ret = register_trace_android_vh_binder_set_priority(
			probe_android_vh_binder_set_priority, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
		goto failed;
	}

	ret = register_trace_android_vh_binder_restore_priority(
			probe_android_vh_binder_restore_priority, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
		goto failed;
	}

	/* register tracepoint of scheduler_tick */
	ret = register_trace_android_vh_scheduler_tick(tt_tick, NULL);
	if (ret) {
		pr_info("%s: register hooks failed, returned %d\n", TAG, ret);
		goto register_failed;
	}

	ret = init_cpu_time();
	if (ret) {
		pr_info("%s: init cpu time failed, returned %d\n", TAG, ret);
		goto register_failed;
	}

	ret = init_tt_input_handler();
	if (ret)
		pr_info("%s: init input handler failed, returned %d\n", TAG, ret);
#if IS_ENABLED(CONFIG_MTK_TASK_TURBO)
	tt_vip_enable_p = &tt_vip_enable;
	task_turbo_launch_turbo_cb = task_turbo_launch_turbo_notify;
#endif
#if IS_ENABLED(CONFIG_MTK_FPSGO_V8) || IS_ENABLED(CONFIG_MTK_FPSGO)
	task_turbo_enforce_ct_to_vip_fp = enforce_ct_to_vip;
#endif
	task_turbo_do_set_binder_uclamp_param = do_set_binder_uclamp_param;
	task_turbo_do_unset_binder_uclamp_param = do_unset_binder_uclamp_param;
	task_turbo_do_binder_uclamp_stuff = do_binder_uclamp_stuff;
	task_turbo_do_enable_binder_uclamp_inheritance = do_enable_binder_uclamp_inheritance;
	vip_engine_set_vip_ctrl_node_cs = set_vip_ctrl_node;
	vip_engine_unset_vip_ctrl_node_cs = unset_vip_ctrl_node;
	vip_engine_set_vip_ctrl_node_sbe = set_vip_ctrl_node;
	vip_engine_unset_vip_ctrl_node_sbe = unset_vip_ctrl_node;
	uclamp_wq = create_singlethread_workqueue("uclamp_singlethread_wq");
	vip_loom_select_task_rq_fair_hook = vip_loom_select_task_rq_fair;
	loom_affinity_node_init();

failed:
	if (ret)
		pr_info("register hooks failed, ret %d line %d\n", ret, ret_erri_line);
	return ret;

register_failed:
	unregister_trace_android_vh_scheduler_tick(tt_tick, NULL);
	return ret;
}

static void  __exit exit_vip_engine(void)
{
	uclamp_list_clear();
	destroy_workqueue(uclamp_wq);
}
module_init(init_vip_engine);
module_exit(exit_vip_engine);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("MediaTek vip-engine");
