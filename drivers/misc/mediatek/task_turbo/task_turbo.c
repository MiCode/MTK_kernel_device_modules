// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#undef pr_fmt
#define pr_fmt(fmt) "Task-Turbo: " fmt

#include <linux/sched/cputime.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <uapi/linux/sched/types.h>
#include <uapi/linux/prctl.h>
#include <linux/futex.h>
#include <linux/plist.h>
#include <linux/percpu-defs.h>
#include <linux/input.h>
#include <linux/cpumask.h>
#include <linux/sched/signal.h>
#include <linux/of.h>

#include <kernel/futex/futex.h>
#include <kernel/sched/sched.h>
#include <drivers/android/binder_internal.h>

#include <trace/hooks/vendor_hooks.h>
#include <trace/hooks/sched.h>
#include <trace/hooks/binder.h>
#include <trace/hooks/rwsem.h>
#include <trace/hooks/futex.h>
#include <trace/hooks/dtask.h>
#include <trace/hooks/cgroup.h>
#include <trace/hooks/sys.h>
#include <include/trace/events/task.h>

#include <task_turbo.h>
#include <task_turbo_v.h>
#include "eas/eas_plus.h"
#include <eas/vip.h>
#if IS_ENABLED(CONFIG_MTK_FPSGO_V3) || IS_ENABLED(CONFIG_MTK_FPSGO)
#include <fstb.h>
#endif

#define CREATE_TRACE_POINTS
#include <trace_task_turbo.h>
#if IS_ENABLED(CONFIG_MTK_CORE_CTL)
#include "core_ctl/core_ctl.h"
#endif

LIST_HEAD(hmp_domains);

/*TODO: find the magic bias number */
#define TURBO_PID_COUNT		8
#define INHERITED_RWSEM_COUNT	4
#define RENDER_THREAD_NAME	"RenderThread"
#define TAG			"Task-Turbo"
#define TURBO_ENABLE		1
#define TURBO_DISABLE		0
#define INHERIT_THRESHOLD	4
#define VIP_PRIO_OFFSET		5
#define type_offset(type)		 (type * 4)
#define task_turbo_nice(nice) (nice == 0xbeef || nice == 0xbeee)
#define task_restore_nice(nice) (nice == 0xbeee)
#define type_number(type)		 (1U << type_offset(type))
#define get_value_with_type(value, type)				\
	(value & ((unsigned int)(0x0000000f) << (type_offset(type))))
#define for_each_hmp_domain_L_first(hmpd) \
		list_for_each_entry_reverse(hmpd, &hmp_domains, hmp_domains)
#define hmp_cpu_domain(cpu)     (per_cpu(hmp_cpu_domain, (cpu)))

#define is_VIP_basic(vts) (vts->basic_vip)
#define is_VVIP(vts) (vts->vvip)
#define is_priority_based_vip(vts) ((vts->priority_based_prio <= MAX_PRIORITY_BASED_VIP) &&	\
	(vts->priority_based_prio >= MIN_PRIORITY_BASED_VIP))
/*
 * Unsigned subtract and clamp on underflow.
 *
 * Explicitly do a load-store to ensure the intermediate value never hits
 * memory. This allows lockless observations without ever seeing the negative
 * values.
 */
#define sub_positive(_ptr, _val) do {				\
	typeof(_ptr) ptr = (_ptr);				\
	typeof(*ptr) val = (_val);				\
	typeof(*ptr) res, var = READ_ONCE(*ptr);		\
	res = var - val;					\
	if (res > var)						\
		res = 0;					\
	WRITE_ONCE(*ptr, res);					\
} while (0)

/*
 * Remove and clamp on negative, from a local variable.
 *
 * A variant of sub_positive(), which does not use explicit load-store
 * and is thus optimized for local variable updates.
 */
#define lsub_positive(_ptr, _val) do {				\
	typeof(_ptr) ptr = (_ptr);				\
	*ptr -= min_t(typeof(*ptr), *ptr, _val);		\
} while (0)

#define RWSEM_READER_OWNED	(1UL << 0)
#define RWSEM_RD_NONSPINNABLE	(1UL << 1)
#define RWSEM_WR_NONSPINNABLE	(1UL << 2)
#define RWSEM_NONSPINNABLE	(RWSEM_RD_NONSPINNABLE | RWSEM_WR_NONSPINNABLE)
#define RWSEM_OWNER_FLAGS_MASK	(RWSEM_READER_OWNED | RWSEM_NONSPINNABLE)
#define RWSEM_WRITER_LOCKED	(1UL << 0)
#define RWSEM_WRITER_MASK	RWSEM_WRITER_LOCKED

DEFINE_PER_CPU(struct hmp_domain *, hmp_cpu_domain);
#if IS_ENABLED(CONFIG_ARM64)
DEFINE_PER_CPU(unsigned long, cpu_scale) = SCHED_CAPACITY_SCALE;
#endif

#define HWCODE_IDX		20
#define SEGMENT_IDX		30
#define MAX_LOOM_BUF_SIZE 4
#define MAX_NR_CPUS CONFIG_MAX_NR_CPUS
#define HWCODE_IDX		20
#define SEGMENT_IDX		30

static uint32_t latency_turbo = SUB_FEAT_LOCK | SUB_FEAT_BINDER |
				SUB_FEAT_SCHED;
static uint32_t launch_turbo =  SUB_FEAT_LOCK | SUB_FEAT_BINDER |
				SUB_FEAT_SCHED | SUB_FEAT_FLAVOR_BIGCORE;
static DEFINE_SPINLOCK(TURBO_SPIN_LOCK);
static DEFINE_SPINLOCK(RWSEM_SPIN_LOCK);
static DEFINE_SPINLOCK(check_lock);
static DEFINE_MUTEX(cpu_lock);
static DEFINE_MUTEX(cpu_loading_lock);
static DEFINE_MUTEX(wi_lock);
static DEFINE_MUTEX(enforced_qualified_lock);
static pid_t turbo_pid[TURBO_PID_COUNT] = {0};
static unsigned int task_turbo_feats;
static struct task_struct *inherited_rwsem_owners[INHERITED_RWSEM_COUNT] = {NULL};
static struct cgroup_subsys_state *top_app_css;

static bool is_turbo_task(struct task_struct *p);
static void set_load_weight(struct task_struct *p, bool update_load);
static void rwsem_stop_turbo_inherit(struct rw_semaphore *sem);
static void rwsem_list_add(struct task_struct *task, struct list_head *entry,
				struct list_head *head);
static bool binder_start_turbo_inherit(struct task_struct *from,
					struct task_struct *to);
static void binder_stop_turbo_inherit(struct task_struct *p);
void (*binder_start_vip_inherit_hook)(int to_pid, int inherited_vip_prio) = NULL;
EXPORT_SYMBOL(binder_start_vip_inherit_hook);
void (*binder_stop_vip_inherit_hook)(int pid, int inherited_vip_prio) = NULL;
EXPORT_SYMBOL(binder_stop_vip_inherit_hook);
static inline struct task_struct *rwsem_owner(struct rw_semaphore *sem);
static inline bool rwsem_test_oflags(struct rw_semaphore *sem, long flags);
static inline bool is_rwsem_reader_owned(struct rw_semaphore *sem);
static void rwsem_start_turbo_inherit(struct rw_semaphore *sem);
static bool sub_feat_enable(int type);
static bool start_turbo_inherit(struct task_struct *task, int type, int cnt);
static bool stop_turbo_inherit(struct task_struct *task, int type);
static inline bool should_set_inherit_turbo(struct task_struct *task);
static inline void add_inherit_types(struct task_struct *task, int type);
static inline void sub_inherit_types(struct task_struct *task, int type);
static inline void set_scheduler_tuning(struct task_struct *task);
static inline void unset_scheduler_tuning(struct task_struct *task);
static bool is_inherit_turbo(struct task_struct *task, int type);
static bool test_turbo_cnt(struct task_struct *task);
static int select_turbo_cpu(struct task_struct *p);
static int find_best_turbo_cpu(struct task_struct *p);
static void init_hmp_domains(void);
static void hmp_cpu_mask_setup(void);
static int arch_get_nr_clusters(void);
static void arch_get_cluster_cpus(struct cpumask *cpus, int cluster_id);
static int hmp_compare(void *priv, const struct list_head *a, const struct list_head *b);
static inline void fillin_cluster(struct cluster_info *cinfo,
		struct hmp_domain *hmpd);
static void sys_set_turbo_task(struct task_struct *p);
static void init_turbo_attr(struct task_struct *p);
static unsigned long cpu_util(int cpu, struct task_struct *p, int dst_cpu, int boost);
static inline unsigned long task_util(struct task_struct *p);
static inline unsigned long _task_util_est(struct task_struct *p);
static int avg_cpu_loading;
static int cpu_loading_thres = 95;
static int vip_util_thres = 80;
static int tt_vip_enable = 1;
static int binder_vip_inheritance_enable = 1;
static int binder_nonvip_inheritance_enable;
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
static DEFINE_SPINLOCK(loom_affinity_lock);
static struct affinity_data_node loom_rec_buffer[MAX_LOOM_BUF_SIZE];
static atomic_t loom_cpu_dedicated_enable;
static atomic_t loom_aff_ctl_enable = ATOMIC_INIT(1);

struct tracepoints_table {
	const char *name;
	void *func;
	struct tracepoint *tp;
	bool registered;
};

static const char * const caller_id_desc[] = {
	"DEBUG_NODE", "FPSGO", "UX", "VIDEO"
};

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
		{ 0x1357, 0x05 },
		{ 0x1357, 0x13 },
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
		    (tags->data[SEGMENT_IDX] == rules[i].segment_mask))
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

inline bool launch_turbo_enable(void);

void exp_trace_turbo_vip(const char *desc, int pid)
{
	trace_turbo_vip(avg_cpu_loading, cpu_loading_thres, desc, pid, "-1", INVALID_VAL, enforced_qualified_mask);
}
EXPORT_SYMBOL(exp_trace_turbo_vip);

int (*wi_add_tgid_hook)(int pid) = NULL;
EXPORT_SYMBOL(wi_add_tgid_hook);
int (*wi_del_tgid_hook)(int pid) = NULL;
EXPORT_SYMBOL(wi_del_tgid_hook);

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

	if (tt_vip_enable) {
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

int (*disable_tt_vip_hook)(u64) = NULL;
EXPORT_SYMBOL(disable_tt_vip_hook);

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

	if (val < 0)
		return -EINVAL;

	tt_vip_enable = !!val;

	if (!tt_vip_enable && disable_tt_vip_hook)
		disable_tt_vip_hook(enforced_qualified_mask);

	return retval;
}

static const struct kernel_param_ops enable_tt_vip_ops = {
	.set = enable_tt_vip,
	.get = param_get_int,
};

module_param_cb(enable_tt_vip, &enable_tt_vip_ops, &tt_vip_enable, 0664);
MODULE_PARM_DESC(enable_tt_vip, "Enable or disable tt vip");

void (*turn_on_tgd_hook)(void) = NULL;
EXPORT_SYMBOL(turn_on_tgd_hook);
void (*turn_off_tgd_hook)(void) = NULL;
EXPORT_SYMBOL(turn_off_tgd_hook);

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

int (*set_tgd_hook)(int tgd) = NULL;
EXPORT_SYMBOL(set_tgd_hook);

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

	if (set_tgd_hook) {
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

int (*unset_tgd_hook)(int tgd) = NULL;
EXPORT_SYMBOL(unset_tgd_hook);

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

void (*set_td_hook)(int td) = NULL;
EXPORT_SYMBOL(set_td_hook);

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

	if (set_td_hook) {
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

void (*unset_td_hook)(int td) = NULL;
EXPORT_SYMBOL(unset_td_hook);

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

void (*set_tdtgd_hook)(int tgd) = NULL;
EXPORT_SYMBOL(set_tdtgd_hook);

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

	if (set_tdtgd_hook) {
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

void (*unset_tdtgd_hook)(int tgd) = NULL;
EXPORT_SYMBOL(unset_tdtgd_hook);

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
	if (tmp_mask && val && tt_vip_enable)
		queue_work(system_highpri_wq, &tt_vip_worker);

	trace_turbo_vip(INVALID_LOADING, INVALID_LOADING, "enforce:",
					INVALID_TGID, caller_id_desc[caller_id], val, tmp_mask);

	return 0;
}
EXPORT_SYMBOL_GPL(enforce_ct_to_vip);

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

#if IS_ENABLED(CONFIG_ARM64)
unsigned long cpu_cap_ceiling(int cpu)
{
	unsigned long cap_ceiling;

	cap_ceiling = min_t(unsigned long, arch_scale_cpu_capacity(cpu),
		get_cpu_gear_uclamp_max_capacity(cpu));
	return clamp_t(unsigned long, cap_ceiling,
		READ_ONCE(per_cpu(min_freq_scale, cpu)), READ_ONCE(per_cpu(max_freq_scale, cpu)));
}
#endif

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
		total_capacity += min(cpu_cap_ceiling(cpu), cpu_rq(cpu)->cpu_capacity);
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

int (*tt_vip_algo_hook)(int ct_vip_qualified, int ssid_tgid, int sui_tgid, int f_tgid, bool touching) = NULL;
EXPORT_SYMBOL(tt_vip_algo_hook);

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
	bool touching = false;
	int tmp_avg_cpu_loading = 0;

	/* The effect after touch ends lasts for TOUCH_SUSTAIN_MS milliseconds */
	if (ktime_to_ms(ktime_get() - cur_touch_time) < TOUCH_SUSTAIN_MS || launch_turbo_enable())
		touching = true;

	mutex_lock(&cpu_loading_lock);
	tmp_avg_cpu_loading = avg_cpu_loading;
	mutex_unlock(&cpu_loading_lock);
	ct_vip_qualified = tmp_avg_cpu_loading >= cpu_loading_thres;

	mutex_lock(&enforced_qualified_lock);
	/* enforced_qualified_mask can forcibly make critical task(s) to VIP */
	if (enforced_qualified_mask)
		ct_vip_qualified = true;
	mutex_unlock(&enforced_qualified_lock);

	mutex_lock(&wi_lock);

	if (tt_vip_algo_hook)
		tt_vip_algo_hook(ct_vip_qualified, ssid_tgid, sui_tgid, f_tgid, touching);

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

int (*is_target_found_hook)(const char*, int) = NULL;
EXPORT_SYMBOL(is_target_found_hook);

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
	if (tt_vip_enable && type == EV_KEY && code == BTN_TOUCH && value == TOUCH_DOWN) {
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
	struct task_turbo_t *turbo_data;

	init_turbo_attr(p);
	if (unlikely(is_turbo_task(current))) {
		turbo_data = get_task_turbo_t(current);
		if (task_has_dl_policy(p) || task_has_rt_policy(p))
			p->static_prio = NICE_TO_PRIO(turbo_data->nice_backup);
		else {
			p->static_prio = NICE_TO_PRIO(turbo_data->nice_backup);
			p->prio = p->normal_prio = p->static_prio;
			set_load_weight(p, false);
		}
		trace_turbo_prepare_prio_fork(turbo_data, p);
	}
}

static void probe_android_rvh_finish_prio_fork(void *ignore, struct task_struct *p)
{
	if (!dl_prio(p->prio) && !rt_prio(p->prio)) {
		struct task_turbo_t *turbo_data;

		turbo_data = get_task_turbo_t(p);
		/* prio and backup should be aligned */
		turbo_data->nice_backup = PRIO_TO_NICE(p->prio);
	}
}

static void probe_android_rvh_rtmutex_prepare_setprio(void *ignore, struct task_struct *p,
						struct task_struct *pi_task)
{
	int queued, running;
	struct rq_flags rf;
	struct rq *rq;

	/* if rt boost, recover prio with backup */
	if (unlikely(is_turbo_task(p))) {
		if (!dl_prio(p->prio) && !rt_prio(p->prio)) {
			struct task_turbo_t *turbo_data;
			int backup;

			turbo_data = get_task_turbo_t(p);
			backup = turbo_data->nice_backup;

			if (backup >= MIN_NICE && backup <= MAX_NICE) {
				rq = __task_rq_lock(p, &rf);
				update_rq_clock(rq);

				queued = task_on_rq_queued(p);
				running = task_current(rq, p);
				if (queued)
					deactivate_task(rq, p,
						DEQUEUE_SAVE | DEQUEUE_NOCLOCK);
				if (running)
					put_prev_task(rq, p);

				p->static_prio = NICE_TO_PRIO(backup);
				p->prio = p->normal_prio = p->static_prio;
				set_load_weight(p, false);

				if (queued)
					activate_task(rq, p,
						ENQUEUE_RESTORE | ENQUEUE_NOCLOCK);
				if (running)
					set_next_task(rq, p);

				trace_turbo_rtmutex_prepare_setprio(turbo_data, p);
				__task_rq_unlock(rq, &rf);
			}
		}
	}
}

static void probe_android_rvh_set_user_nice(void *ignore, struct task_struct *p,
						long *nice, bool *allowed)
{
	struct task_turbo_t *turbo_data;
	bool p_turbo;

	if ((*nice < MIN_NICE || *nice > MAX_NICE) && !task_turbo_nice(*nice)) {
		*allowed = false;
		return;
	} else
		*allowed = true;


	turbo_data = get_task_turbo_t(p);
	/* for general use, backup it */
	if (!task_turbo_nice(*nice))
		turbo_data->nice_backup = *nice;

	p_turbo = is_turbo_task(p);
	if (p_turbo && !task_restore_nice(*nice)) {
		*nice = rlimit_to_nice(task_rlimit(p, RLIMIT_NICE));
		if (unlikely(*nice > MAX_NICE)) {
			pr_warn("%s: pid=%d RLIMIT_NICE=%ld is not set\n",
				TAG, p->pid, *nice);
			*nice = turbo_data->nice_backup;
		}
	} else
		*nice = turbo_data->nice_backup;

	trace_sched_set_user_nice(p, *nice, p_turbo);
}

static void probe_android_rvh_setscheduler(void *ignore, struct task_struct *p)
{
	struct task_turbo_t *turbo_data;

	if (!dl_prio(p->prio) && !rt_prio(p->prio)) {
		turbo_data = get_task_turbo_t(p);
		turbo_data->nice_backup = PRIO_TO_NICE(p->prio);
	}
}

static void probe_android_vh_rwsem_wait_finish(void *ignore, struct rw_semaphore *sem)
{
	rwsem_stop_turbo_inherit(sem);
}

static void probe_android_vh_rwsem_init(void *ignore, struct rw_semaphore *sem)
{
	sem->android_vendor_data1 = 0;
}

static void probe_android_vh_alter_rwsem_list_add(void *ignore, struct rwsem_waiter *waiter,
							struct rw_semaphore *sem,
							bool *already_on_list)
{
	rwsem_list_add(waiter->task, &waiter->list, &sem->wait_list);
	*already_on_list = true;
}

static void probe_android_vh_rwsem_wait_start(void *ignore, struct rw_semaphore *sem)
{
	rwsem_start_turbo_inherit(sem);
}

static void probe_android_vh_binder_transaction_init(void *ignore, struct binder_transaction *t)
{
	t->android_vendor_data1 = 0;
}

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
		if (is_priority_based_vip(vts_to) && (vts_to->priority_based_prio==inherited_vip_prio))
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

static void probe_android_vh_binder_set_priority(void *ignore, struct binder_transaction *t,
							struct task_struct *task)
{
	if (binder_start_turbo_inherit(t->from ?
			t->from->task : NULL, task)) {
		t->android_vendor_data1 = (u64)task;
	}
	if (binder_vip_inheritance_enable && tt_vip_enable && binder_start_vip_inherit_hook)
		binder_start_vip_inherit(t->from ? t->from->task : NULL, task);
}

static void probe_android_vh_binder_restore_priority(void *ignore,
		struct binder_transaction *in_reply_to, struct task_struct *cur)
{
	struct task_struct *inherit_task;

	if (in_reply_to) {
		inherit_task = get_inherit_task(in_reply_to);
		if (cur && cur == inherit_task) {
			binder_stop_turbo_inherit(cur);
			in_reply_to->android_vendor_data1 = 0;
		}
	} else
		binder_stop_turbo_inherit(cur);
	if (cur && binder_stop_vip_inherit_hook)
		binder_stop_vip_inherit(cur);
}

static void probe_android_vh_alter_futex_plist_add(void *ignore, struct plist_node *q_list,
						struct plist_head *hb_chain, bool *already_on_hb)
{
	struct futex_q *this, *next;
	struct plist_node *current_node = q_list;
	struct plist_node *this_node;
	int prev_pid = 0;
	bool prev_turbo = 1;
	bool this_turbo;

	if (*already_on_hb)
		return;

	if (!sub_feat_enable(SUB_FEAT_LOCK) ||
	    !is_turbo_task(current)) {
		*already_on_hb = false;
		return;
	}

	plist_for_each_entry_safe(this, next, hb_chain, list) {
		this_turbo = is_turbo_task(this->task);
		if ((!this->pi_state || !this->rt_waiter) && !this_turbo) {
			this_node = &this->list;
			trace_turbo_futex_plist_add(prev_pid, prev_turbo,
					this->task->pid, this_turbo);
			list_add(&current_node->node_list,
				 this_node->node_list.prev);
			*already_on_hb = true;
			return;
		}
		prev_pid = this->task->pid;
		prev_turbo = this_turbo;
	}

	*already_on_hb = false;
}
static void probe_android_rvh_select_task_rq_fair(void *ignore, struct task_struct *p,
							int prev_cpu, int sd_flag,
							int wake_flags, int *target_cpu)
{
	/* skip if p is vip */
	if (get_vip_task_prio(p) != NOT_VIP)
		return;

	*target_cpu = select_turbo_cpu(p);
}

static inline struct task_struct *rwsem_owner(struct rw_semaphore *sem)
{
	return (struct task_struct *)
		(atomic_long_read(&sem->owner) & ~RWSEM_OWNER_FLAGS_MASK);
}

static inline bool rwsem_test_oflags(struct rw_semaphore *sem, long flags)
{
	return atomic_long_read(&sem->owner) & flags;
}

static inline bool is_rwsem_reader_owned(struct rw_semaphore *sem)
{
#if IS_ENABLED(CONFIG_DEBUG_RWSEMS)
	/*
	 * Check the count to see if it is write-locked.
	 */
	long count = atomic_long_read(&sem->count);

	if (count & RWSEM_WRITER_MASK)
		return false;
#endif
	return rwsem_test_oflags(sem, RWSEM_READER_OWNED);
}

/*
 * cpu_util_without: compute cpu utilization without any contributions from *p
 * @cpu: the CPU which utilization is requested
 * @p: the task which utilization should be discounted
 *
 * The utilization of a CPU is defined by the utilization of tasks currently
 * enqueued on that CPU as well as tasks which are currently sleeping after an
 * execution on that CPU.
 *
 * This method returns the utilization of the specified CPU by discounting the
 * utilization of the specified task, whenever the task is currently
 * contributing to the CPU utilization.
 */
static unsigned long cpu_util_without(int cpu, struct task_struct *p)
{
	/* Task has no contribution or is new */
	if (cpu != task_cpu(p) || !READ_ONCE(p->se.avg.last_update_time))
		p = NULL;

	return cpu_util(cpu, p, -1, 0);
}
/**
 * cpu_util() - Estimates the amount of CPU capacity used by CFS tasks.
 * @cpu: the CPU to get the utilization for
 * @p: task for which the CPU utilization should be predicted or NULL
 * @dst_cpu: CPU @p migrates to, -1 if @p moves from @cpu or @p == NULL
 * @boost: 1 to enable boosting, otherwise 0
 *
 * The unit of the return value must be the same as the one of CPU capacity
 * so that CPU utilization can be compared with CPU capacity.
 *
 * CPU utilization is the sum of running time of runnable tasks plus the
 * recent utilization of currently non-runnable tasks on that CPU.
 * It represents the amount of CPU capacity currently used by CFS tasks in
 * the range [0..max CPU capacity] with max CPU capacity being the CPU
 * capacity at f_max.
 *
 * The estimated CPU utilization is defined as the maximum between CPU
 * utilization and sum of the estimated utilization of the currently
 * runnable tasks on that CPU. It preserves a utilization "snapshot" of
 * previously-executed tasks, which helps better deduce how busy a CPU will
 * be when a long-sleeping task wakes up. The contribution to CPU utilization
 * of such a task would be significantly decayed at this point of time.
 *
 * Boosted CPU utilization is defined as max(CPU runnable, CPU utilization).
 * CPU contention for CFS tasks can be detected by CPU runnable > CPU
 * utilization. Boosting is implemented in cpu_util() so that internal
 * users (e.g. EAS) can use it next to external users (e.g. schedutil),
 * latter via cpu_util_cfs_boost().
 *
 * CPU utilization can be higher than the current CPU capacity
 * (f_curr/f_max * max CPU capacity) or even the max CPU capacity because
 * of rounding errors as well as task migrations or wakeups of new tasks.
 * CPU utilization has to be capped to fit into the [0..max CPU capacity]
 * range. Otherwise a group of CPUs (CPU0 util = 121% + CPU1 util = 80%)
 * could be seen as over-utilized even though CPU1 has 20% of spare CPU
 * capacity. CPU utilization is allowed to overshoot current CPU capacity
 * though since this is useful for predicting the CPU capacity required
 * after task migrations (scheduler-driven DVFS).
 *
 * Return: (Boosted) (estimated) utilization for the specified CPU.
 */
static unsigned long
cpu_util(int cpu, struct task_struct *p, int dst_cpu, int boost)
{
	struct cfs_rq *cfs_rq = &cpu_rq(cpu)->cfs;
	unsigned long util = READ_ONCE(cfs_rq->avg.util_avg);
	unsigned long runnable;

	if (boost) {
		runnable = READ_ONCE(cfs_rq->avg.runnable_avg);
		util = max(util, runnable);
	}

	/*
	 * If @dst_cpu is -1 or @p migrates from @cpu to @dst_cpu remove its
	 * contribution. If @p migrates from another CPU to @cpu add its
	 * contribution. In all the other cases @cpu is not impacted by the
	 * migration so its util_avg is already correct.
	 */
	if (p && task_cpu(p) == cpu && dst_cpu != cpu)
		lsub_positive(&util, task_util(p));
	else if (p && task_cpu(p) != cpu && dst_cpu == cpu)
		util += task_util(p);

	if (sched_feat(UTIL_EST) && is_util_est_enable()) {
		unsigned long util_est;

		util_est = READ_ONCE(cfs_rq->avg.util_est);

		/*
		 * During wake-up @p isn't enqueued yet and doesn't contribute
		 * to any cpu_rq(cpu)->cfs.avg.util_est.enqueued.
		 * If @dst_cpu == @cpu add it to "simulate" cpu_util after @p
		 * has been enqueued.
		 *
		 * During exec (@dst_cpu = -1) @p is enqueued and does
		 * contribute to cpu_rq(cpu)->cfs.util_est.enqueued.
		 * Remove it to "simulate" cpu_util without @p's contribution.
		 *
		 * Despite the task_on_rq_queued(@p) check there is still a
		 * small window for a possible race when an exec
		 * select_task_rq_fair() races with LB's detach_task().
		 *
		 *   detach_task()
		 *     deactivate_task()
		 *       p->on_rq = TASK_ON_RQ_MIGRATING;
		 *       -------------------------------- A
		 *       dequeue_task()                    \
		 *         dequeue_task_fair()              + Race Time
		 *           util_est_dequeue()            /
		 *       -------------------------------- B
		 *
		 * The additional check "current == p" is required to further
		 * reduce the race window.
		 */
		if (p && unlikely(task_on_rq_queued(p) || current == p))
			lsub_positive(&util_est, _task_util_est(p));

		util = max(util, util_est);
	}

	return min(util, capacity_orig_of(cpu));
}

static inline unsigned long task_util(struct task_struct *p)
{
	return READ_ONCE(p->se.avg.util_avg);
}

static inline unsigned long _task_util_est(struct task_struct *p)
{
	return READ_ONCE(p->se.avg.util_est) & ~UTIL_AVG_UNCHANGED;
}

int find_best_turbo_cpu(struct task_struct *p)
{
	struct hmp_domain *domain;
	int i = 0, iter_cpu;
	unsigned long spare_cap, max_spare_cap = 0;
	const struct cpumask *tsk_cpus_ptr = p->cpus_ptr;
	int max_spare_cpu = -1;
	int new_cpu = -1;

	/* The order is B, BL, LL cluster */
	for_each_hmp_domain_L_first(domain) {
		/* check fastest domain for turbo task */
		if (i != 0)
			break;
		for_each_cpu(iter_cpu, &domain->possible_cpus) {

			if (!cpu_online(iter_cpu) ||
			    !cpumask_test_cpu(iter_cpu, tsk_cpus_ptr) ||
			    !cpu_active(iter_cpu))
				continue;

			/*
			 * favor tasks that prefer idle cpus
			 * to improve latency
			 */
			if (idle_cpu(iter_cpu)) {
				new_cpu = iter_cpu;
				goto out;
			}

			spare_cap =
			     max_t(long, cpu_rq(iter_cpu)->cpu_capacity - cpu_util_without(iter_cpu, p), 0);
			if (spare_cap > max_spare_cap) {
				max_spare_cap = spare_cap;
				max_spare_cpu = iter_cpu;
			}
		}
		i++;
	}
	if (max_spare_cpu > 0)
		new_cpu = max_spare_cpu;
out:
	trace_select_turbo_cpu(new_cpu, p, max_spare_cap, max_spare_cpu);
	return new_cpu;
}

int select_turbo_cpu(struct task_struct *p)
{
	int target_cpu = -1;

	if (!is_turbo_task(p))
		return -1;

	if (!sub_feat_enable(SUB_FEAT_FLAVOR_BIGCORE))
		return -1;

	target_cpu = find_best_turbo_cpu(p);

	return target_cpu;
}

/* copy from sched/core.c */
static void set_load_weight(struct task_struct *p, bool update_load)
{
	int prio = p->static_prio - MAX_RT_PRIO;
	struct load_weight *load = &p->se.load;

	/*
	 * SCHED_IDLE tasks get minimal weight:
	 */
	if (task_has_idle_policy(p)) {
		load->weight = scale_load(WEIGHT_IDLEPRIO);
		load->inv_weight = WMULT_IDLEPRIO;
		return;
	}

	/*
	 * SCHED_OTHER tasks have to update their load when changing their
	 * weight
	 */
	if (update_load && p->sched_class == &fair_sched_class) {
		reweight_task(p, prio);
	} else {
		load->weight = scale_load(sched_prio_to_weight[prio]);
		load->inv_weight = sched_prio_to_wmult[prio];
	}
}

#if IS_ENABLED(CONFIG_ARM64)
/**
 * idle_cpu - is a given CPU idle currently?
 * @cpu: the processor in question.
 *
 * Return: 1 if the CPU is currently idle. 0 otherwise.
 */
int idle_cpu(int cpu)
{
	struct rq *rq = cpu_rq(cpu);

	if (rq->curr != rq->idle)
		return 0;

	if (rq->nr_running)
		return 0;

#if IS_ENABLED(CONFIG_SMP)
	if (rq->ttwu_pending)
		return 0;
#endif

	return 1;
}
#endif

static void rwsem_stop_turbo_inherit(struct rw_semaphore *sem)
{
	unsigned long flags;
	struct task_struct *inherited_owner;
	int i;
	bool found = false;

	spin_lock_irqsave(&RWSEM_SPIN_LOCK, flags);
	inherited_owner = get_inherit_task(sem);
	if (!inherited_owner)
		goto out_unlock;

	for (i = 0; i < INHERITED_RWSEM_COUNT; i++)
		if (inherited_rwsem_owners[i] == inherited_owner) {
			found = true;
			inherited_rwsem_owners[i] = NULL;
			break;
		}
	if (!found)
		goto out_unlock;

	stop_turbo_inherit(inherited_owner, RWSEM_INHERIT);
	sem->android_vendor_data1 = 0;
	trace_turbo_inherit_end(inherited_owner);
	put_task_struct(inherited_owner);

out_unlock:
	spin_unlock_irqrestore(&RWSEM_SPIN_LOCK, flags);
}

static void rwsem_list_add(struct task_struct *task,
			   struct list_head *entry,
			   struct list_head *head)
{
	if (!sub_feat_enable(SUB_FEAT_LOCK)) {
		list_add_tail(entry, head);
		return;
	}

	if (is_turbo_task(task)) {
		struct list_head *pos = NULL;
		struct list_head *n = NULL;
		struct rwsem_waiter *waiter = NULL;
		struct rwsem_waiter *entry_waiter;

		/*
		 * Insert turbo task prior to first non-turbo task.
		 * Make sure only the first waiter can have its handoff_set
		 * set here.
		 */
		entry_waiter = list_entry(entry, struct rwsem_waiter, list);
		list_for_each_safe(pos, n, head) {
			waiter = list_entry(pos,
					struct rwsem_waiter, list);
			if (!is_turbo_task(waiter->task)) {
				entry_waiter->handoff_set = waiter->handoff_set;
				waiter->handoff_set = false;
				list_add(entry, waiter->list.prev);
				return;
			}
		}
	}
	list_add_tail(entry, head);
}

static void rwsem_start_turbo_inherit(struct rw_semaphore *sem)
{
	unsigned long flags;
	bool should_inherit;
	struct task_struct *owner, *inherited_owner;
	struct task_turbo_t *turbo_data;
	int i;
	bool found = false;

	if (!sub_feat_enable(SUB_FEAT_LOCK))
		return;

	spin_lock_irqsave(&RWSEM_SPIN_LOCK, flags);
	owner = rwsem_owner(sem);
	should_inherit = should_set_inherit_turbo(current);
	if (should_inherit) {
		inherited_owner = get_inherit_task(sem);
		turbo_data = get_task_turbo_t(current);
		if (owner && !is_rwsem_reader_owned(sem) &&
		    !is_turbo_task(owner) &&
		    !inherited_owner) {
			for (i = 0; i < INHERITED_RWSEM_COUNT; i++)
				if (!inherited_rwsem_owners[i]) {
					found = true;
					inherited_rwsem_owners[i] = owner;
					break;
				}
			if (!found)
				goto out_unlock;
			get_task_struct(owner);
			start_turbo_inherit(owner,
					    RWSEM_INHERIT,
					    turbo_data->inherit_cnt);
			sem->android_vendor_data1 = (u64)owner;
			trace_turbo_inherit_start(current, owner);
		}
	}
out_unlock:
	spin_unlock_irqrestore(&RWSEM_SPIN_LOCK, flags);
}

static bool start_turbo_inherit(struct task_struct *task,
				int type,
				int cnt)
{
	struct task_turbo_t *turbo_data;

	if (type <= START_INHERIT || type >= END_INHERIT)
		return false;

	add_inherit_types(task, type);
	turbo_data = get_task_turbo_t(task);
	if (turbo_data->inherit_cnt < cnt + 1)
		turbo_data->inherit_cnt = cnt + 1;

	/* scheduler tuning start */
	set_scheduler_tuning(task);
	return true;
}

static bool stop_turbo_inherit(struct task_struct *task,
				int type)
{
	unsigned int inherit_types;
	bool ret = false;
	struct task_turbo_t *turbo_data;

	if (type <= START_INHERIT || type >= END_INHERIT)
		goto done;

	turbo_data = get_task_turbo_t(task);
	inherit_types = atomic_read(&turbo_data->inherit_types);
	if (inherit_types == 0)
		goto done;

	sub_inherit_types(task, type);
	turbo_data = get_task_turbo_t(task);
	inherit_types = atomic_read(&turbo_data->inherit_types);
	if (inherit_types > 0)
		goto done;

	/* scheduler tuning stop */
	unset_scheduler_tuning(task);
	turbo_data->inherit_cnt = 0;
	ret = true;
done:
	return ret;
}

static inline void set_scheduler_tuning(struct task_struct *task)
{
	int cur_nice = task_nice(task);

	if (!fair_policy(task->policy))
		return;

	if (!sub_feat_enable(SUB_FEAT_SCHED))
		return;

	/* trigger renice for turbo task */
	set_user_nice(task, 0xbeef);
	if (tt_vip_enable) {
		set_task_vvip_and_throttle(task_pid_nr(task), 60);
		trace_turbo_vvip_set(task_pid_nr(task));
	}

	trace_sched_turbo_nice_set(task, NICE_TO_PRIO(cur_nice), task->prio);
}

static inline void unset_scheduler_tuning(struct task_struct *task)
{
	int cur_prio = task->prio;

	if (!fair_policy(task->policy))
		return;

	set_user_nice(task, 0xbeee);
	unset_task_vvip(task_pid_nr(task));

	trace_turbo_vvip_unset(task_pid_nr(task));
	trace_sched_turbo_nice_set(task, cur_prio, task->prio);
}

static inline void add_inherit_types(struct task_struct *task, int type)
{
	struct task_turbo_t *turbo_data;

	turbo_data = get_task_turbo_t(task);
	atomic_add(type_number(type), &turbo_data->inherit_types);
}

static inline void sub_inherit_types(struct task_struct *task, int type)
{
	struct task_turbo_t *turbo_data;

	turbo_data = get_task_turbo_t(task);
	atomic_sub(type_number(type), &turbo_data->inherit_types);
}

static bool binder_start_turbo_inherit(struct task_struct *from,
					struct task_struct *to)
{
	bool ret = false;
	struct task_turbo_t *from_turbo_data;

	if (!sub_feat_enable(SUB_FEAT_BINDER))
		goto done;

	if (!from || !to)
		goto done;

	if (!is_turbo_task(from) ||
		!test_turbo_cnt(from)) {
		from_turbo_data = get_task_turbo_t(from);
		trace_turbo_inherit_failed(from_turbo_data->turbo,
					atomic_read(&from_turbo_data->inherit_types),
					from_turbo_data->inherit_cnt, __LINE__);
		goto done;
	}

	if (!is_turbo_task(to)) {
		from_turbo_data = get_task_turbo_t(from);
		ret = start_turbo_inherit(to, BINDER_INHERIT,
					from_turbo_data->inherit_cnt);
		trace_turbo_inherit_start(from, to);
	}
done:
	return ret;
}

static void binder_stop_turbo_inherit(struct task_struct *p)
{
	if (is_inherit_turbo(p, BINDER_INHERIT)) {
		stop_turbo_inherit(p, BINDER_INHERIT);
		trace_turbo_inherit_end(p);
	}
}

static bool is_inherit_turbo(struct task_struct *task, int type)
{
	unsigned int inherit_types;
	struct task_turbo_t *turbo_data;

	if (!task)
		return false;

	turbo_data = get_task_turbo_t(task);
	inherit_types = atomic_read(&turbo_data->inherit_types);

	if (inherit_types == 0)
		return false;

	return get_value_with_type(inherit_types, type) > 0;
}

static bool is_turbo_task(struct task_struct *p)
{
	struct task_turbo_t *turbo_data;

	if (!p)
		return false;

	turbo_data = get_task_turbo_t(p);
	return turbo_data->turbo || atomic_read(&turbo_data->inherit_types);
}

static bool test_turbo_cnt(struct task_struct *task)
{
	struct task_turbo_t *turbo_data;

	turbo_data = get_task_turbo_t(task);
	/* TODO:limit number should be discuss */
	return turbo_data->inherit_cnt < INHERIT_THRESHOLD;
}

static inline bool should_set_inherit_turbo(struct task_struct *task)
{
	return is_turbo_task(task) && test_turbo_cnt(task);
}

inline bool latency_turbo_enable(void)
{
	return task_turbo_feats == latency_turbo;
}

inline bool launch_turbo_enable(void)
{
	return task_turbo_feats == launch_turbo;
}

static void init_turbo_attr(struct task_struct *p)
{
	struct task_turbo_t *turbo_data = get_task_turbo_t(p);

	turbo_data->turbo = TURBO_DISABLE;
	turbo_data->render = 0;
	atomic_set(&(turbo_data->inherit_types), 0);
	turbo_data->inherit_cnt = 0;
	turbo_data->vip_prio_backup = 0;
	turbo_data->throttle_time_backup = 0;
}

int get_turbo_feats(void)
{
	return task_turbo_feats;
}

static bool sub_feat_enable(int type)
{
	return get_turbo_feats() & type;
}

/*
 * set task to turbo by pid
 */
static int set_turbo_task(int pid, int val)
{
	struct task_struct *p;
	struct task_turbo_t *turbo_data;
	int retval = 0;

	if (pid < 0 || pid > PID_MAX_DEFAULT)
		return -EINVAL;

	if (val < 0 || val > 1)
		return -EINVAL;

	rcu_read_lock();
	p = find_task_by_vpid(pid);

	if (p != NULL) {
		get_task_struct(p);
		turbo_data = get_task_turbo_t(p);
		turbo_data->turbo = val;
		/*TODO: scheduler tuning */
		if (turbo_data->turbo == TURBO_ENABLE)
			set_scheduler_tuning(p);
		else
			unset_scheduler_tuning(p);
		trace_turbo_set(p);
		put_task_struct(p);
	} else
		retval = -ESRCH;
	rcu_read_unlock();

	return retval;
}

static int unset_turbo_task(int pid)
{
	return set_turbo_task(pid, TURBO_DISABLE);
}

static int set_task_turbo_feats(const char *buf,
				const struct kernel_param *kp)
{
	int ret, i;
	unsigned int val;

	ret = kstrtouint(buf, 0, &val);
	if (ret)
		return ret;

	spin_lock(&TURBO_SPIN_LOCK);
	if (val == latency_turbo ||
	    val == launch_turbo  || val == 0)
		ret = param_set_uint(buf, kp);
	else
		ret = -EINVAL;

	/* if disable turbo, remove all turbo tasks */
	/* spin_lock(&TURBO_SPIN_LOCK); */
	if (val == 0) {
		for (i = 0; i < TURBO_PID_COUNT; i++) {
			if (turbo_pid[i]) {
				unset_turbo_task(turbo_pid[i]);
				turbo_pid[i] = 0;
			}
		}
	}
	spin_unlock(&TURBO_SPIN_LOCK);

	if (!ret) {
		pr_info("%s: task_turbo_feats is change to %d successfully",
				TAG, task_turbo_feats);
		trace_turbo_feats_set(task_turbo_feats);
	}
	return ret;
}

static const struct kernel_param_ops task_turbo_feats_param_ops = {
	.set = set_task_turbo_feats,
	.get = param_get_uint,
};

param_check_uint(feats, &task_turbo_feats);
module_param_cb(feats, &task_turbo_feats_param_ops, &task_turbo_feats, 0644);
MODULE_PARM_DESC(feats, "enable task turbo features if needed");

static bool add_turbo_list_locked(pid_t pid);
static void remove_turbo_list_locked(pid_t pid);

/*
 * use pid set turbo task
 */
static int add_turbo_list_by_pid(pid_t pid)
{
	int retval = -EINVAL;

	if (!task_turbo_feats)
		return retval;

	if (pid < 0 || pid > PID_MAX_DEFAULT)
		return retval;

	spin_lock(&TURBO_SPIN_LOCK);
	if (!add_turbo_list_locked(pid))
		goto unlock;

	retval = set_turbo_task(pid, TURBO_ENABLE);
unlock:
	spin_unlock(&TURBO_SPIN_LOCK);
	return retval;
}

static pid_t turbo_pid_param;
static int set_turbo_task_param(const char *buf,
				const struct kernel_param *kp)
{
	int retval = 0;
	pid_t pid;

	retval = kstrtouint(buf, 0, &pid);

	if (!retval)
		retval = add_turbo_list_by_pid(pid);

	if (!retval)
		turbo_pid_param = pid;

	return retval;
}

static struct kernel_param_ops turbo_pid_param_ops = {
	.set = set_turbo_task_param,
	.get = param_get_int,
};

param_check_uint(turbo_pid, &turbo_pid_param);
module_param_cb(turbo_pid, &turbo_pid_param_ops, &turbo_pid_param, 0644);
MODULE_PARM_DESC(turbo_pid, "set turbo task by pid");

static int unset_turbo_list_by_pid(pid_t pid)
{
	int retval = -EINVAL;

	if (pid < 0 || pid > PID_MAX_DEFAULT)
		return retval;

	spin_lock(&TURBO_SPIN_LOCK);
	remove_turbo_list_locked(pid);
	retval = unset_turbo_task(pid);
	spin_unlock(&TURBO_SPIN_LOCK);
	return retval;
}

static pid_t unset_turbo_pid_param;
static int unset_turbo_task_param(const char *buf,
				  const struct kernel_param *kp)
{
	int retval = 0;
	pid_t pid;

	retval = kstrtouint(buf, 0, &pid);

	if (!retval)
		retval = unset_turbo_list_by_pid(pid);

	if (!retval)
		unset_turbo_pid_param = pid;

	return retval;
}

static struct kernel_param_ops unset_turbo_pid_param_ops = {
	.set = unset_turbo_task_param,
	.get = param_get_int,
};

param_check_uint(unset_turbo_pid, &unset_turbo_pid_param);
module_param_cb(unset_turbo_pid, &unset_turbo_pid_param_ops,
		&unset_turbo_pid_param, 0644);
MODULE_PARM_DESC(unset_turbo_pid, "unset turbo task by pid");

static inline bool is_top_app(struct task_struct *task)
{
#if IS_ENABLED(CONFIG_CGROUP_SCHED)
	guard(rcu)();
	return top_app_css == task_css(task, cpu_cgrp_id);
#else
	return false;
#endif
}

static inline bool cgroup_check_set_turbo(struct task_struct *p)
{
	struct task_turbo_t *turbo_data;

	turbo_data = get_task_turbo_t(p);

	if (!launch_turbo_enable())
		return false;

	if (turbo_data->turbo)
		return false;

	/* set critical tasks for UI or UX to turbo */
	return (turbo_data->render ||
	       (p == p->group_leader &&
		p->real_parent->pid != 1));
}

/*
 * record task to turbo list
 */
static bool add_turbo_list_locked(pid_t pid)
{
	int i, free_idx = -1;
	bool ret = false;

	if (unlikely(!get_turbo_feats()))
		goto done;

	for (i = 0; i < TURBO_PID_COUNT; i++) {
		if (free_idx < 0 && !turbo_pid[i])
			free_idx = i;

		if (unlikely(turbo_pid[i] == pid)) {
			free_idx = i;
			break;
		}
	}

	if (free_idx >= 0) {
		turbo_pid[free_idx] = pid;
		ret = true;
	}
done:
	return ret;
}

static void add_turbo_list(struct task_struct *p)
{
	struct task_turbo_t *turbo_data;

	spin_lock(&TURBO_SPIN_LOCK);
	if (add_turbo_list_locked(p->pid)) {
		turbo_data = get_task_turbo_t(p);
		turbo_data->turbo = TURBO_ENABLE;
		/* TODO: scheduler tuninng */
		set_scheduler_tuning(p);
		trace_turbo_set(p);
	}
	spin_unlock(&TURBO_SPIN_LOCK);
}

/*
 * remove task from turbo list
 */
static void remove_turbo_list_locked(pid_t pid)
{
	int i;

	for (i = 0; i < TURBO_PID_COUNT; i++) {
		if (turbo_pid[i] == pid) {
			turbo_pid[i] = 0;
			break;
		}
	}
}

static void remove_turbo_list(struct task_struct *p)
{
	struct task_turbo_t *turbo_data;

	spin_lock(&TURBO_SPIN_LOCK);
	turbo_data = get_task_turbo_t(p);
	remove_turbo_list_locked(p->pid);
	turbo_data->turbo = TURBO_DISABLE;
	unset_scheduler_tuning(p);
	trace_turbo_set(p);
	spin_unlock(&TURBO_SPIN_LOCK);
}

static void probe_android_vh_cgroup_set_task(void *ignore, int ret, struct task_struct *p)
{
	struct task_turbo_t *turbo_data;

	if (ret)
		return;

	if (is_top_app(p)) {
		if (!cgroup_check_set_turbo(p))
			return;
		add_turbo_list(p);
	} else {
		turbo_data = get_task_turbo_t(p);
		if (turbo_data->turbo)
			remove_turbo_list(p);
	}
}

static void probe_android_rvh_set_task_comm(void *ignore, struct task_struct *p, bool exec)
{
	sys_set_turbo_task(p);
}

static inline void fillin_cluster(struct cluster_info *cinfo,
		struct hmp_domain *hmpd)
{
	int cpu;
	unsigned long cpu_perf;

	cinfo->hmpd = hmpd;
	cinfo->cpu = cpumask_any(&cinfo->hmpd->possible_cpus);

	for_each_cpu(cpu, &hmpd->possible_cpus) {
		cpu_perf = arch_scale_cpu_capacity(cpu);
		if (cpu_perf > 0)
			break;
	}
	cinfo->cpu_perf = cpu_perf;

	if (cpu_perf == 0)
		pr_info("%s: Uninitialized CPU performance (CPU mask: %lx)",
				TAG, cpumask_bits(&hmpd->possible_cpus)[0]);
}

int hmp_compare(void *priv, const struct list_head *a,
		      const struct list_head *b)
{
	struct cluster_info ca;
	struct cluster_info cb;

	fillin_cluster(&ca, list_entry(a, struct hmp_domain, hmp_domains));
	fillin_cluster(&cb, list_entry(b, struct hmp_domain, hmp_domains));

	return (ca.cpu_perf > cb.cpu_perf) ? -1 : 1;
}

void init_hmp_domains(void)
{
	struct hmp_domain *domain;
	struct cpumask cpu_mask;
	int id, maxid;

	cpumask_clear(&cpu_mask);
	maxid = arch_get_nr_clusters();

	/*
	 * Initialize hmp_domains
	 * Must be ordered with respect to compute capacity.
	 * Fastest domain at head of list.
	 */
	for (id = 0; id < maxid; id++) {
		arch_get_cluster_cpus(&cpu_mask, id);
		domain = (struct hmp_domain *)
			kmalloc(sizeof(struct hmp_domain), GFP_KERNEL);
		if (domain) {
			cpumask_copy(&domain->possible_cpus, &cpu_mask);
			cpumask_and(&domain->cpus, cpu_online_mask,
				&domain->possible_cpus);
			list_add(&domain->hmp_domains, &hmp_domains);
		}
	}

	/*
	 * Sorting HMP domain by CPU capacity
	 */
	list_sort(NULL, &hmp_domains, &hmp_compare);
	pr_info("Sort hmp_domains from little to big:\n");
	for_each_hmp_domain_L_first(domain) {
		pr_info("    cpumask: 0x%02lx\n",
				*cpumask_bits(&domain->possible_cpus));
	}
	hmp_cpu_mask_setup();
}

static void init_top_app_css(void)
{
	struct cgroup_subsys_state *root_css = &root_task_group.css;
	struct cgroup_subsys_state *css = root_css;

	guard(rcu)();
	css_for_each_child(css, root_css)
		if (css && css->cgroup && css->cgroup->kn && css->cgroup->kn->name &&
		    !strcmp(css->cgroup->kn->name, "top-app")) {
			top_app_css = css;
			return;
		}
}

void hmp_cpu_mask_setup(void)
{
	struct hmp_domain *domain;
	struct list_head *pos;
	int cpu;

	pr_info("Initializing HMP scheduler:\n");

	/* Initialize hmp_domains using platform code */
	if (list_empty(&hmp_domains)) {
		pr_info("HMP domain list is empty!\n");
		return;
	}

	/* Print hmp_domains */
	list_for_each(pos, &hmp_domains) {
		domain = list_entry(pos, struct hmp_domain, hmp_domains);

		for_each_cpu(cpu, &domain->possible_cpus)
			per_cpu(hmp_cpu_domain, cpu) = domain;
	}
	pr_info("Initializing HMP scheduler done\n");
}

int arch_get_nr_clusters(void)
{
	int __arch_nr_clusters = -1;
	int max_id = 0;
	unsigned int cpu;

	/* assume socket id is monotonic increasing without gap. */
	for_each_possible_cpu(cpu) {
		int cpu_cluster_id = topology_cluster_id(cpu);

		if (cpu_cluster_id > max_id)
			max_id = cpu_cluster_id;
	}
	__arch_nr_clusters = max_id + 1;
	return __arch_nr_clusters;
}

void arch_get_cluster_cpus(struct cpumask *cpus, int cluster_id)
{
	unsigned int cpu;

	cpumask_clear(cpus);
	for_each_possible_cpu(cpu) {
		int cpu_cluster_id = topology_cluster_id(cpu);

		if (cpu_cluster_id == cluster_id)
			cpumask_set_cpu(cpu, cpus);
	}
}

static void sys_set_turbo_task(struct task_struct *p)
{
	struct task_turbo_t *turbo_data;

	if (!launch_turbo_enable())
		return;

	if (!is_top_app(p))
		return;

	if (strcmp(p->comm, RENDER_THREAD_NAME))
		return;

	turbo_data = get_task_turbo_t(p);
	turbo_data->render = 1;
	add_turbo_list(p);
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

	if (tt_vip_enable) {
		wallclock = ktime_get_ns();
		if (!tt_vip_do_check(wallclock))
			return;

		queue_work(system_highpri_wq, &tt_vip_periodic_worker);
	}
}

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

	ret = set_cpus_allowed_ptr_by_kernel(p, &aff_mask);

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
	int tmp_aff_cpu;

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

int loom_aff_ctl(unsigned int enable)
{
	unsigned int old_val = atomic_read(&loom_aff_ctl_enable);
	unsigned int new_val;

	if (enable > 1)
		return -EINVAL;

	atomic_set(&loom_aff_ctl_enable, enable);
	new_val = atomic_read(&loom_aff_ctl_enable);

	pr_info("%s: loom affinity control enable from %u to %u\n",
		 __func__, old_val, new_val);

	return 0;
}
EXPORT_SYMBOL(loom_aff_ctl);

static int set_loom_aff_ctl_enable(const char *buf, const struct kernel_param *kp)
{
	int retval = 0, val = 0;

	retval = kstrtouint(buf, 0, &val);

	if (retval)
		return -EINVAL;

	retval = loom_aff_ctl(!!val);

	return retval;
}

static int get_loom_aff_ctl_enable(char *buffer, const struct kernel_param *kp)
{
	int len = 0;

	return scnprintf(buffer + len, PAGE_SIZE - len,
		"loom_aff_ctl_enable=%u\n", atomic_read(&loom_aff_ctl_enable));
}

static const struct kernel_param_ops loom_aff_ctl_enable_ops = {
	.set = set_loom_aff_ctl_enable,
	.get = get_loom_aff_ctl_enable,
};
module_param_cb(loom_aff_ctl_enable, &loom_aff_ctl_enable_ops, NULL, 0664);
MODULE_PARM_DESC(loom_aff_ctl_enable, "Enable or disable affinity control");

static unsigned int cpumask_to_uint(const cpumask_t *mask)
{
	if (mask)
		return (unsigned int)cpumask_bits(mask)[0];
	return 0;
}

static void loom_sched_process_fork(void *data, struct task_struct *parent, struct task_struct *child)
{
	unsigned long flags;
	int i;

	if (!atomic_read(&loom_cpu_dedicated_enable) || !atomic_read(&loom_aff_ctl_enable))
		return;

	/* check dedicated CPU for new task */
	spin_lock_irqsave(&loom_affinity_lock, flags);
	for(i=0; i<MAX_LOOM_BUF_SIZE; i++) {
		if (loom_rec_buffer[i].pid == parent->pid) {
			cpumask_setall(&child->cpus_mask);
			child->cpus_ptr = &child->cpus_mask;
			child->nr_cpus_allowed = cpumask_weight(&child->cpus_mask);
			trace_loom_affinity_ctl(parent->pid, loom_rec_buffer[i].aff_cpu,
				child->pid, cpumask_to_uint(child->cpus_ptr));
			break;
		}
	}
	spin_unlock_irqrestore(&loom_affinity_lock, flags);
}

#define FOR_EACH_INTEREST(i) \
	for (i = 0; i < sizeof(loom_tracepoints) / sizeof(struct tracepoints_table); i++)

struct tracepoints_table loom_tracepoints[] = {
	{.name = "sched_process_fork", .func = loom_sched_process_fork},
};

static void lookup_tracepoints(struct tracepoint *tp, void *ignore)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (strcmp(loom_tracepoints[i].name, tp->name) == 0)
			loom_tracepoints[i].tp = tp;
	}
}

static void tracepoint_cleanup(void)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (loom_tracepoints[i].registered) {
			tracepoint_probe_unregister(
				loom_tracepoints[i].tp,
				loom_tracepoints[i].func, NULL);
			loom_tracepoints[i].registered = false;
		}
	}
}

int init_tracepoints(void)
{
	int i = 0, ret = 0;

	FOR_EACH_INTEREST(i) {
		if (loom_tracepoints[i].tp == NULL) {
			pr_info("[LOOM] %s: invalid register trace-name=%s\n", __func__, loom_tracepoints[i].name);
			tracepoint_cleanup();
			return -1;
		}
	}

	FOR_EACH_INTEREST(i) {
		ret = tracepoint_probe_register(loom_tracepoints[i].tp, loom_tracepoints[i].func,  NULL);
		if (ret) {
			pr_info("[LOOM] %s: invalid activate trace-name=%s\n", __func__, loom_tracepoints[i].name);
			continue;
		}
		loom_tracepoints[i].registered = true;
	}
	return ret;
}


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

static int __init init_task_turbo(void)
{
	int ret, ret_erri_line;

	ret = register_trace_android_rvh_rtmutex_prepare_setprio(
			probe_android_rvh_rtmutex_prepare_setprio, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
		goto failed;
	}

	ret = register_trace_android_rvh_prepare_prio_fork(
			probe_android_rvh_prepare_prio_fork, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
		goto failed;
	}

	ret = register_trace_android_rvh_finish_prio_fork(
			probe_android_rvh_finish_prio_fork, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
		goto failed;
	}

	ret = register_trace_android_rvh_set_user_nice(
			probe_android_rvh_set_user_nice, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
		goto failed;
	}

	ret = register_trace_android_rvh_setscheduler(
			probe_android_rvh_setscheduler, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
	goto failed;
	}

	ret = register_trace_android_vh_binder_transaction_init(
			probe_android_vh_binder_transaction_init, NULL);
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

	ret = register_trace_android_vh_rwsem_init(
			probe_android_vh_rwsem_init, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
		goto failed;
	}

	ret = register_trace_android_vh_rwsem_read_wait_finish(
			probe_android_vh_rwsem_wait_finish, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
		goto failed;
	}

	ret = register_trace_android_vh_rwsem_write_wait_finish(
			probe_android_vh_rwsem_wait_finish, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
		goto failed;
	}

	ret = register_trace_android_vh_rwsem_read_wait_start(
			probe_android_vh_rwsem_wait_start, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
		goto failed;
	}

	ret = register_trace_android_vh_rwsem_write_wait_start(
			probe_android_vh_rwsem_wait_start, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
		goto failed;
	}

	ret = register_trace_android_vh_alter_rwsem_list_add(
			probe_android_vh_alter_rwsem_list_add, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
		goto failed;
	}

	ret = register_trace_android_vh_alter_futex_plist_add(
			probe_android_vh_alter_futex_plist_add, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
		goto failed;
	}

	ret = register_trace_android_rvh_select_task_rq_fair(
			probe_android_rvh_select_task_rq_fair, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
		goto failed;
	}

	ret = register_trace_android_vh_cgroup_set_task(
			probe_android_vh_cgroup_set_task, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
		goto failed;
	}

	ret = register_trace_android_rvh_set_task_comm(
			probe_android_rvh_set_task_comm, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
		goto failed;
	}

	init_hmp_domains();
	init_top_app_css();

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

	task_turbo_enforce_ct_to_vip_fp = enforce_ct_to_vip;
	vip_loom_select_task_rq_fair_hook = vip_loom_select_task_rq_fair;

	loom_affinity_node_init();

	//Init Once Time
	for_each_kernel_tracepoint(lookup_tracepoints, NULL);
	//Enable API
	init_tracepoints();

failed:
	if (ret)
		pr_err("register hooks failed, ret %d line %d\n", ret, ret_erri_line);

	return ret;

register_failed:
	unregister_trace_android_vh_scheduler_tick(tt_tick, NULL);
	return ret;

}
static void  __exit exit_task_turbo(void)
{
	/*
	 * vendor hook cannot unregister, please check vendor_hook.h
	 */
	 //Disable API
	tracepoint_cleanup();
}
module_init(init_task_turbo);
module_exit(exit_task_turbo);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("MediaTek task-turbo");
