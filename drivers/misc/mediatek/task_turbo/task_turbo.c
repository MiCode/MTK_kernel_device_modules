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

#include <task_turbo.h>
#if IS_ENABLED(CONFIG_MTK_FPSGO_V3) || IS_ENABLED(CONFIG_MTK_FPSGO)
#include <fstb.h>
#endif

#define CREATE_TRACE_POINTS
#include <trace_task_turbo.h>

LIST_HEAD(hmp_domains);

/*TODO: find the magic bias number */
#define TOP_APP_GROUP_ID	((4-1)*10)
#define TURBO_PID_COUNT		8
#define INHERITED_RWSEM_COUNT	4
#define RENDER_THREAD_NAME	"RenderThread"
#define TAG			"Task-Turbo"
#define TURBO_ENABLE		1
#define TURBO_DISABLE		0
#define INHERIT_THRESHOLD	4
#define type_offset(type)		 (type * 4)
#define task_turbo_nice(nice) (nice == 0xbeef || nice == 0xbeee)
#define task_restore_nice(nice) (nice == 0xbeee)
#define type_number(type)		 (1U << type_offset(type))
#define get_value_with_type(value, type)				\
	(value & ((unsigned int)(0x0000000f) << (type_offset(type))))
#define for_each_hmp_domain_L_first(hmpd) \
		list_for_each_entry_reverse(hmpd, &hmp_domains, hmp_domains)
#define hmp_cpu_domain(cpu)     (per_cpu(hmp_cpu_domain, (cpu)))

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

static bool is_turbo_task(struct task_struct *p);
static void set_load_weight(struct task_struct *p, bool update_load);
static void rwsem_stop_turbo_inherit(struct rw_semaphore *sem);
static void rwsem_list_add(struct task_struct *task, struct list_head *entry,
				struct list_head *head);
static bool binder_start_turbo_inherit(struct task_struct *from,
					struct task_struct *to);
static void binder_stop_turbo_inherit(struct task_struct *p);
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
static unsigned long capacity_of(int cpu);
static void init_turbo_attr(struct task_struct *p);
static unsigned long cpu_util(int cpu, struct task_struct *p, int dst_cpu, int boost);
static inline unsigned long task_util(struct task_struct *p);
static inline unsigned long _task_util_est(struct task_struct *p);
static int avg_cpu_loading;
static int cpu_loading_thres = 85;
static int tt_vip_enable = 1;
static struct cpu_info ci;
static bool win_vip_status;
static bool touch_vip_status;
static bool cam_3rd_hal_vip_status;
static bool cam_3rd_svr_vip_status;
static u64 checked_timestamp;
static int max_cpus;
static struct cpu_time *cur_wall_time, *cur_idle_time,
						*prev_wall_time, *prev_idle_time;
static void tt_vip_handler(struct work_struct *work);
static DECLARE_WORK(tt_vip_worker, tt_vip_handler);
static void tt_vip_periodic_handler(struct work_struct *work);
static DECLARE_WORK(tt_vip_periodic_worker, tt_vip_periodic_handler);
static int inputDispatcher_tgid;
static int cur_inputDispatcher_tgid;
static ktime_t cur_touch_time;
static ktime_t cur_touch_down_time;
static size_t win_info_size;
static size_t cur_win_info_size;
static u64 enforced_qualified_mask;
#if IS_ENABLED(CONFIG_MTK_FPSGO_V3)
static pid_t cam_hal_tgid = INVALID_TGID;
static pid_t cam_svr_tgid = INVALID_TGID;
#endif

static struct win_info wi_head = {
	.list = LIST_HEAD_INIT(wi_head.list),
};

static struct win_info cur_wi_head = {
	.list = LIST_HEAD_INIT(cur_wi_head.list),
};

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

/*
 * update_cur_win_info - Update current window information list
 * @cur_wi_head: head of the current window info list
 * @wi_head: head of the new window info list to update from
 *
 * This function will clear the current list of window information and
 * replace it with the contents of a new list. The new nodes are allocated
 * and initialized with the information from the new list, and the old nodes
 * are deleted and their memory is freed.
 */
static void update_cur_win_info(struct win_info *cur_wi_head, struct win_info *wi_head)
{
	struct win_info *pos, *n;
	struct win_info *new_node;

	list_for_each_entry_safe(pos, n, &cur_wi_head->list, list) {
		list_del(&pos->list);
		kfree(pos);
	}

	list_for_each_entry(pos, &wi_head->list, list) {
		new_node = kmalloc(sizeof(struct win_info), GFP_KERNEL);
		if (!new_node)
			return;

		*new_node = *pos;
		INIT_LIST_HEAD(&new_node->list);
		list_add_tail(&new_node->list, &cur_wi_head->list);
	}
	cur_win_info_size = win_info_size;
}

/*
 * wi_add_tgid - Add a task group ID to the window info list
 * @pid: the process ID (PID) of the task group leader
 *
 * Allocates a new window info node, initializes it with the provided PID,
 * and adds it to the global window info list.
 *
 * Return: 0 on success, -ENOMEM on memory allocation failure
 */
static int wi_add_tgid(pid_t pid)
{
	struct win_info *new_node;

	new_node = kmalloc(sizeof(struct win_info), GFP_KERNEL);
	if (!new_node)
		return -ENOMEM;

	new_node->tgid = pid;
	INIT_LIST_HEAD(&new_node->list);
	list_add_tail(&new_node->list, &wi_head.list);
	win_info_size++;

	return 0;
}

/*
 * wi_del_tgid - Delete a task group ID from the window info list
 * @pid: the process ID (PID) of the task group leader to remove
 *
 * Iterates over the global window info list to find the node with the
 * matching PID. If found, it removes the node from the list, frees its
 * memory, and updates the current window info list.
 *
 * Return: 0 on success, -ESRCH if the PID is not found
 */
static int wi_del_tgid(pid_t pid)
{
	struct win_info *pos, *n;

	list_for_each_entry_safe(pos, n, &wi_head.list, list) {
		if (pos->tgid == pid) {
			unset_task_basic_vip(pos->tgid);
			unset_tgid_vip(pos->tgid);
			trace_turbo_vip(INVALID_LOADING, cpu_loading_thres,
				"delete win: window unset_tgid_vip:", pos->tgid);

			list_del(&pos->list);
			kfree(pos);
			win_info_size--;
			update_cur_win_info(&cur_wi_head, &wi_head);
			return 0;
		}
	}

	return -ESRCH;
}

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
	pid_t pid;

	if (sscanf(buf, "%d%d", &pid, &status) != 2)
		return -EINVAL;

	if (pid < 0 || pid > PID_MAX_DEFAULT)
		return -EINVAL;

	mutex_lock(&wi_lock);
	if (status == 1)
		retval = wi_add_tgid(pid);
	else
		retval = wi_del_tgid(pid);
	mutex_unlock(&wi_lock);

	pr_info("turbo_vip: %s: retval=%d\n", __func__, retval);
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
 * Parses the value from the user-provided buffer and sets the tt_vip_enable
 * flag accordingly. When disabling, it also turns off all previously set
 * configurations by resetting related flags and clearing VIP settings.
 *
 * Return: 0 on success, negative error code on failure
 */
static int enable_tt_vip(const char *buf, const struct kernel_param *kp)
{
	int retval = 0, val = 0;
	struct win_info *pos;

	retval = kstrtouint(buf, 0, &val);

	if (retval)
		return -EINVAL;

	if (val < 0)
		return -EINVAL;

	tt_vip_enable = !!val;

	if (!tt_vip_enable) {
		turn_off_tgid_vip();
		if (win_vip_status || enforced_qualified_mask) {
			win_vip_status = false;
			list_for_each_entry(pos, &cur_wi_head.list, list) {
				unset_task_basic_vip(pos->tgid);
				unset_tgid_vip(pos->tgid);
				trace_turbo_vip(INVALID_LOADING, cpu_loading_thres,
					"disable tt_vip: window unset_tgid_vip:", pos->tgid);
			}
		}
#if IS_ENABLED(CONFIG_MTK_FPSGO_V3)
		if (cam_3rd_hal_vip_status) {
			cam_3rd_hal_vip_status = false;
			if (cam_hal_tgid > 0) {
				unset_task_basic_vip(cam_hal_tgid);
				unset_tgid_vip(cam_hal_tgid);
				trace_turbo_vip(INVALID_LOADING, cpu_loading_thres,
					"disable tt_vip: cam_hal unset_tgid_vip:", cam_hal_tgid);
				cam_hal_tgid = INVALID_TGID;
			}
		}
		if (cam_3rd_svr_vip_status) {
			cam_3rd_svr_vip_status = false;
			if (cam_svr_tgid > 0) {
				unset_task_basic_vip(cam_svr_tgid);
				unset_tgid_vip(cam_svr_tgid);
				trace_turbo_vip(INVALID_LOADING, cpu_loading_thres,
					"disable tt_vip: cam_server unset_tgid_vip:", cam_svr_tgid);
				cam_svr_tgid = INVALID_TGID;
			}
		}
#endif
		if (touch_vip_status) {
			touch_vip_status = false;
			unset_task_basic_vip(cur_inputDispatcher_tgid);
			unset_tgid_vip(cur_inputDispatcher_tgid);
			trace_turbo_vip(INVALID_LOADING, cpu_loading_thres,
				"disable tt_vip: system_server unset_tgid_vip:", cur_inputDispatcher_tgid);
		}
	}

	return retval;
}

static const struct kernel_param_ops enable_tt_vip_ops = {
	.set = enable_tt_vip,
	.get = param_get_int,
};

module_param_cb(enable_tt_vip, &enable_tt_vip_ops, &tt_vip_enable, 0664);
MODULE_PARM_DESC(enable_tt_vip, "Enable or disable tt vip");


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
	char desc[256];
	u64 tmp_mask;
	static const char * const caller_id_desc[] = {"DEBUG_NODE", "FPSGO", "UX"};
	enum {
		DEBUG_NODE,
		FPSGO,
		UX,
		MAX_TYPE
	};

	if (caller_id >= MAX_TYPE)
		return -EINVAL;

	val = (val > 0) ? 1 : 0;
	mutex_lock(&enforced_qualified_lock);
	if (val)
		enforced_qualified_mask |= (1U << caller_id);
	else
		enforced_qualified_mask &= ~(1U << caller_id);

	snprintf(desc, sizeof(desc), "%s %s=%d, enforced_qualified_mask=%llu"
			, caller_id_desc[caller_id], __func__, val, enforced_qualified_mask);

	tmp_mask = enforced_qualified_mask;
	mutex_unlock(&enforced_qualified_lock);
	if (tmp_mask && val && tt_vip_enable)
		queue_work(system_highpri_wq, &tt_vip_worker);

	trace_turbo_vip(INVALID_LOADING, INVALID_LOADING, desc, INVALID_TGID);

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
	for (i = 0; i < max_cpus; i++)
		avg_cpu_loading += ci.cpu_loading[i];
	avg_cpu_loading /= max_cpus;
	mutex_unlock(&cpu_loading_lock);
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
	struct win_info *pos;
	struct win_info *pos_wi, *pos_cur_wi;
	struct list_head *wi_ptr, *cur_wi_ptr;
	bool need_clean = false;
	bool need_set = false;
	bool ct_vip_qualified = false;
	bool content_changed = false;
	bool touching = false;
	int tmp_avg_cpu_loading = 0;
#if IS_ENABLED(CONFIG_MTK_FPSGO_V3)
	bool in_cam_3rd = false;

	/* Check if we are in a 3rd camera scenario */
	in_cam_3rd = get_cam_status_for_task_turbo();
#endif

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
	mutex_unlock(&enforced_qualified_lock);

	mutex_lock(&wi_lock);
	if (!cam_3rd_hal_vip_status && !cam_3rd_svr_vip_status && !touch_vip_status) {
		/* If conditions are not met and there are no VIPs to clear, directly unlock and exit */
		if (!ct_vip_qualified && !win_vip_status)
			goto out_unlock;
		if (!ct_vip_qualified && list_empty(&cur_wi_head.list))
			goto out_unlock;

		/* If there are no targets to set as VIP and no VIPs to clear, directly unlock and exit */
		if (list_empty(&wi_head.list) && list_empty(&cur_wi_head.list))
			goto out_unlock;
		if (list_empty(&wi_head.list) && !win_vip_status)
			goto out_unlock;
	}

	/*
	 * If conditions are not met or there are no targets to set,but VIPs need to be cleared,
	 * set the need_clean flag to true
	 */
	if ((!ct_vip_qualified || list_empty(&wi_head.list)) && (cur_win_info_size > 0 && win_vip_status)) {
		need_clean = true;
		turn_off_tgid_vip();
	}

	/* Check if the content of the current list has changed compared to the previous list */
	if (win_info_size == cur_win_info_size) {
		wi_ptr = wi_head.list.next;
		cur_wi_ptr = cur_wi_head.list.next;

		while (wi_ptr != &wi_head.list && cur_wi_ptr != &cur_wi_head.list) {
			pos_wi = list_entry(wi_ptr, struct win_info, list);
			pos_cur_wi = list_entry(cur_wi_ptr, struct win_info, list);

			if (pos_wi->tgid != pos_cur_wi->tgid) {
				content_changed = true;
				break;
			}

			wi_ptr = wi_ptr->next;
			cur_wi_ptr = cur_wi_ptr->next;
		}
	} else
		content_changed = true;

	/* If conditions are met and there are targets to set as VIP, set the need_set flag to true */
	if (ct_vip_qualified && win_info_size > 0)
		need_set = true;

	/*
	 * If the need_clean flag is true or the win_info list content has changed,
	 * clear the VIP status in the win_info list
	 */
	if (need_clean || (cur_win_info_size > 0 && win_vip_status && content_changed)) {
		win_vip_status = false;
		list_for_each_entry(pos, &cur_wi_head.list, list) {
			unset_task_basic_vip(pos->tgid);
			unset_tgid_vip(pos->tgid);
			trace_turbo_vip(tmp_avg_cpu_loading, cpu_loading_thres,
				"ct_vip_unqualified: window unset_tgid_vip:", pos->tgid);
		}
	}
#if IS_ENABLED(CONFIG_MTK_FPSGO_V3)
	/*
	 * If the need_clean flag is true or we are not in a 3rd camera scenario,
	 * clear the camerahal/cameraserver VIP status
	 */
	if (cam_3rd_hal_vip_status && (need_clean || !in_cam_3rd)) {
		cam_3rd_hal_vip_status = false;
		if (cam_hal_tgid > 0) {
			unset_task_basic_vip(cam_hal_tgid);
			unset_tgid_vip(cam_hal_tgid);
			trace_turbo_vip(tmp_avg_cpu_loading, cpu_loading_thres,
				"ct_vip_unqualified: cam_hal unset_tgid_vip:", cam_hal_tgid);
			cam_hal_tgid = INVALID_TGID;
		}
	}
	if (cam_3rd_svr_vip_status && (need_clean || !in_cam_3rd)) {
		cam_3rd_svr_vip_status = false;
		if (cam_svr_tgid > 0) {
			unset_task_basic_vip(cam_svr_tgid);
			unset_tgid_vip(cam_svr_tgid);
			trace_turbo_vip(tmp_avg_cpu_loading, cpu_loading_thres,
				"ct_vip_unqualified: cam_server unset_tgid_vip:", cam_svr_tgid);
			cam_svr_tgid = INVALID_TGID;
		}
	}
#endif
	/*
	 * If the need_clean flag is true or we are not touching,
	 * clear the VIP status of system_server (including inputDispatcher)
	 */
	if (touch_vip_status && (need_clean || !touching)) {
		touch_vip_status = false;
		unset_task_basic_vip(cur_inputDispatcher_tgid);
		unset_tgid_vip(cur_inputDispatcher_tgid);
		trace_turbo_vip(tmp_avg_cpu_loading, cpu_loading_thres,
			"ct_vip_unqualified: system_server unset_tgid_vip:", cur_inputDispatcher_tgid);
	}

	/* If the need_set flag is true, check if VIP status needs to be set */
	if (need_set) {
		/*
		 * If the win_info list content has changed or is not yet set as VIP,
		 * set the windows in the win_info list as VIP
		 */
		if (content_changed || !win_vip_status) {
			win_vip_status = true;
			list_for_each_entry(pos, &wi_head.list, list) {
				set_task_basic_vip(pos->tgid);
				set_tgid_vip(pos->tgid);
				turn_on_tgid_vip();
				trace_turbo_vip(tmp_avg_cpu_loading, cpu_loading_thres,
					"ct_vip_qualified: window set_tgid_vip:", pos->tgid);
			}
		}
#if IS_ENABLED(CONFIG_MTK_FPSGO_V3)
		/* If we are in a 3rd camera scenario, set camerahal/cameraserver as VIP */
		if (in_cam_3rd) {
			if (cam_hal_tgid <= 0)
				cam_hal_tgid = get_cam_hal_pid_for_task_turbo();
			if (cam_hal_tgid > 0 && cam_hal_tgid <= PID_MAX_DEFAULT && !cam_3rd_hal_vip_status) {
				cam_3rd_hal_vip_status = true;
				set_task_basic_vip(cam_hal_tgid);
				set_tgid_vip(cam_hal_tgid);
				turn_on_tgid_vip();
				trace_turbo_vip(tmp_avg_cpu_loading, cpu_loading_thres,
					"ct_vip_qualified: cam_hal set_tgid_vip:", cam_hal_tgid);
			}
			if (cam_svr_tgid <= 0)
				cam_svr_tgid = get_cam_server_pid_for_task_turbo();
			if (cam_svr_tgid > 0 && cam_svr_tgid <= PID_MAX_DEFAULT && !cam_3rd_svr_vip_status) {
				cam_3rd_svr_vip_status = true;
				set_task_basic_vip(cam_svr_tgid);
				set_tgid_vip(cam_svr_tgid);
				turn_on_tgid_vip();
				trace_turbo_vip(tmp_avg_cpu_loading, cpu_loading_thres,
					"ct_vip_qualified: cam_server set_tgid_vip:", cam_svr_tgid);
			}
		}
#endif
		/* If touching, set system_server (including inputDispatcher) as VIP */
		if (touching && inputDispatcher_tgid > 0 && !touch_vip_status) {
			touch_vip_status = true;
			set_task_basic_vip(inputDispatcher_tgid);
			set_tgid_vip(inputDispatcher_tgid);
			turn_on_tgid_vip();
			trace_turbo_vip(tmp_avg_cpu_loading, cpu_loading_thres,
				"ct_vip_qualified: system_server set_tgid_vip:", inputDispatcher_tgid);
			cur_inputDispatcher_tgid = inputDispatcher_tgid;
		}
	}
	/* Synchronize the content of the win_info lists */
	update_cur_win_info(&cur_wi_head, &wi_head);

out_unlock:
	mutex_unlock(&wi_lock);
}
module_param(cpu_loading_thres, int, 0644);

/**
 * tt_vip_handler - Event-triggered work handler for VIP management
 *
 * This function is designed to be called in response to specific events that require
 * immediate attention to VIP status management.
 */
static void tt_vip_handler(struct work_struct *work)
{
	tt_vip();
}

/*
 * tt_vip_periodic_handler - Periodic work handler for VIP management
 *
 * This function is invoked periodically, to perform regular updates
 * of the CPU loading information. After updating the CPU loading, it calls the
 * tt_vip function to manage the VIP status.
 */
static void tt_vip_periodic_handler(struct work_struct *work)
{
	update_cpu_loading();
	tt_vip();
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
 *	This is done by finding the `system_server` task (if not already known) and
 *	updating the `inputDispatcher_tgid` with its task group ID, then queuing the VIP management
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
	int find_ret = 0;
	ktime_t diff = 0;

	cur_touch_time = ktime_get();
	if (tt_vip_enable && type == EV_KEY && code == BTN_TOUCH && value == TOUCH_DOWN) {
		diff = cur_touch_time - cur_touch_down_time;
		cur_touch_down_time = cur_touch_time;
		if (diff >= TOUCH_SUSTAIN_MS) {
			rcu_read_lock();
			p = find_task_by_vpid(inputDispatcher_tgid);
			if (!p || !strstr(p->comm, "system_server")) {
				find_ret = -ESRCH;
				for_each_process(p) {
					if (strstr(p->comm, "system_server")) {
						inputDispatcher_tgid = p->tgid;
						find_ret = 0;
						break;
					}
				}
			}
			rcu_read_unlock();
			if (find_ret == 0)
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

static void probe_android_vh_binder_set_priority(void *ignore, struct binder_transaction *t,
							struct task_struct *task)
{
	if (binder_start_turbo_inherit(t->from ?
			t->from->task : NULL, task)) {
		t->android_vendor_data1 = (u64)task;
	}
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

static unsigned long capacity_of(int cpu)
{
	return cpu_rq(cpu)->cpu_capacity;
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

		util_est = READ_ONCE(cfs_rq->avg.util_est.enqueued);

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
		if (dst_cpu == cpu)
			util_est += _task_util_est(p);
		else if (p && unlikely(task_on_rq_queued(p) || current == p))
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
	struct util_est ue = READ_ONCE(p->se.avg.util_est);

	return max(ue.ewma, (ue.enqueued & ~UTIL_AVG_UNCHANGED));
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
			     max_t(long, capacity_of(iter_cpu) - cpu_util_without(iter_cpu, p), 0);
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

		/* insert turbo task pior to first non-turbo task */
		list_for_each_safe(pos, n, head) {
			waiter = list_entry(pos,
					struct rwsem_waiter, list);
			if (!is_turbo_task(waiter->task)) {
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

	trace_sched_turbo_nice_set(task, NICE_TO_PRIO(cur_nice), task->prio);
}

static inline void unset_scheduler_tuning(struct task_struct *task)
{
	int cur_prio = task->prio;

	if (!fair_policy(task->policy))
		return;

	set_user_nice(task, 0xbeee);

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

	if (!ret)
		pr_info("%s: task_turbo_feats is change to %d successfully",
				TAG, task_turbo_feats);
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

static inline int get_st_group_id(struct task_struct *task)
{
#if IS_ENABLED(CONFIG_CGROUP_SCHED)
	const int subsys_id = cpu_cgrp_id;
	struct cgroup *grp;

	rcu_read_lock();
	grp = task_cgroup(task, subsys_id);
	rcu_read_unlock();
	return grp->kn->id;
#else
	return 0;
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

	if (get_st_group_id(p) == TOP_APP_GROUP_ID) {
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

	if (get_st_group_id(p) != TOP_APP_GROUP_ID)
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
}
module_init(init_task_turbo);
module_exit(exit_task_turbo);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("MediaTek task-turbo");
