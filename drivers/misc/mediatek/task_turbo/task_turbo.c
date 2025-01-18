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
static DEFINE_MUTEX(wi_tbl_lock);
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
static int cpu_loading_thres = 85;
static int fps_drop_vip_throttle = 99;
static int fps_drop_vip_enable;
static int tt_vip_enable = 1;
static struct cpu_info ci;
static bool win_vip_status;
static bool touch_vip_status;
static u64 checked_timestamp;
static pid_t cam_hal_tgid = -1;
static pid_t cam_svr_tgid = -1;
static int max_cpus;
static struct cpu_time *cur_wall_time, *cur_idle_time,
						*prev_wall_time, *prev_idle_time;
static void tt_vip_handler(struct work_struct *work);
static DECLARE_WORK(tt_vip, tt_vip_handler);
static struct task_struct *ts_inputDispatcher;
atomic_long_t inputDispatcher_ptr;
static int last_ts_inputDispatcher_tgid;
static int touch_countdown_timer;
static int fps_drop_countdown_timer;

static struct win_info_tbl wi_tbl = { .wi = NULL, .size = 0 };
static struct win_info_tbl last_wi_tbl = { .wi = NULL, .size = 0 };

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

static void update_last_win_info_tbl(struct win_info_tbl *last_wi_tbl, struct win_info_tbl *new_wi_tbl)
{
	struct win_info *new_tbl;

	kfree(last_wi_tbl->wi);
	if (new_wi_tbl->size == 0) {
		last_wi_tbl->wi = NULL;
		last_wi_tbl->size = 0;
		return;
	}

	new_tbl = kmalloc(new_wi_tbl->size * sizeof(struct win_info), GFP_KERNEL);
	if (!new_tbl) {
		last_wi_tbl->wi = NULL;
		last_wi_tbl->size = 0;
		return;
	}

	memcpy(new_tbl, new_wi_tbl->wi, new_wi_tbl->size * sizeof(struct win_info));
	last_wi_tbl->wi = new_tbl;
	last_wi_tbl->size = new_wi_tbl->size;
}

static int add_tgid_to_tbl(pid_t pid)
{
	struct win_info *new_tbl;
	size_t new_size = wi_tbl.size + 1;

	new_tbl = krealloc(wi_tbl.wi, new_size * sizeof(struct win_info), GFP_KERNEL);
	if (!new_tbl)
		return -ENOMEM;

	wi_tbl.wi = new_tbl;
	wi_tbl.wi[wi_tbl.size].tgid = pid;
	wi_tbl.wi[wi_tbl.size].perf_idx = 0;
	wi_tbl.size = new_size;

	return 0;
}

static int del_tgid_from_tbl(pid_t pid)
{
	size_t i;
	int found = 0;
	struct win_info *new_tbl;

	for (i = 0; i < wi_tbl.size; i++) {
		if (wi_tbl.wi[i].tgid == pid) {
			found = 1;
			unset_task_basic_vip(wi_tbl.wi[i].tgid);
			unset_tgid_basic_vip(wi_tbl.wi[i].tgid);
			break;
		}
	}

	if (!found)
		return -ESRCH;

	for (; i < wi_tbl.size - 1; i++)
		wi_tbl.wi[i] = wi_tbl.wi[i + 1];

	wi_tbl.size--;
	if (wi_tbl.size == 0) {
		kfree(wi_tbl.wi);
		wi_tbl.wi = NULL;
	} else {
		new_tbl = krealloc(wi_tbl.wi, wi_tbl.size * sizeof(struct win_info), GFP_KERNEL);
		if (!new_tbl)
			return -ENOMEM;
		wi_tbl.wi = new_tbl;
	}
	update_last_win_info_tbl(&last_wi_tbl, &wi_tbl);

	return 0;
}

static char win_pid_status_param[64] = "";
static int update_win_pid_status(const char *buf, const struct kernel_param *kp)
{
	int retval = 0, status = 0;
	pid_t pid;

	if (sscanf(buf, "%d%d", &pid, &status) != 2)
		return -EINVAL;

	if (pid < 0 || pid > PID_MAX_DEFAULT)
		return -EINVAL;

	mutex_lock(&wi_tbl_lock);
	if (status == 1)
		retval = add_tgid_to_tbl(pid);
	else
		retval = del_tgid_from_tbl(pid);
	mutex_unlock(&wi_tbl_lock);

	pr_info("%s: retval=%d\n", __func__, retval);
	return 0;
}

static const struct kernel_param_ops update_win_pid_status_ops = {
	.set = update_win_pid_status,
	.get = param_get_charp,
};

module_param_cb(update_win_pid_status, &update_win_pid_status_ops, &win_pid_status_param, 0664);
MODULE_PARM_DESC(update_win_pid_status, "send window pid and status to task turbo");

static int enable_tt_vip(const char *buf, const struct kernel_param *kp)
{
	int retval = 0, val = 0;
	size_t tbl_i;

	retval = kstrtouint(buf, 0, &val);

	if (retval)
		return -EINVAL;

	val = (val > 0) ? 1 : 0;
	if (val == 1) {
		tt_vip_enable = 1;
	} else {
		tt_vip_enable = 0;
		if (win_vip_status) {
			win_vip_status = false;
			for (tbl_i = 0; tbl_i < last_wi_tbl.size; tbl_i++) {
				unset_task_basic_vip(last_wi_tbl.wi[tbl_i].tgid);
				unset_tgid_basic_vip(last_wi_tbl.wi[tbl_i].tgid);
			}
		}
		if (cam_hal_tgid > 0) {
			unset_task_basic_vip(cam_hal_tgid);
			unset_tgid_basic_vip(cam_hal_tgid);
			cam_hal_tgid = -1;
		}
		if (cam_svr_tgid > 0) {
			unset_task_basic_vip(cam_svr_tgid);
			unset_tgid_basic_vip(cam_svr_tgid);
			cam_svr_tgid = -1;
		}
		if (touch_vip_status) {
			touch_vip_status = false;
			unset_task_basic_vip(last_ts_inputDispatcher_tgid);
			unset_tgid_basic_vip(last_ts_inputDispatcher_tgid);
		}
		touch_countdown_timer = 0;
	}

	return retval;
}

static const struct kernel_param_ops enable_tt_vip_ops = {
	.set = enable_tt_vip,
	.get = param_get_int,
};

module_param_cb(enable_tt_vip, &enable_tt_vip_ops, &tt_vip_enable, 0664);
MODULE_PARM_DESC(enable_tt_vip, "Enable or disable tt vip");

static void tt_vip_handler(struct work_struct *work)
{
	int ret = 0, i = 0, avg_cpu_loading = 0;
	size_t tbl_i;
	bool need_clean = false;
	bool need_set = false;
	bool cur_qualified = false;
	bool content_changed = false;
	bool status_3rd_cam = false;
	bool touching = false;
	struct task_struct *p;
	bool fps_drop = false;


	// get cpu_loading from magt
	ret = get_cpu_loading(&ci);

	if (ret < 0)
		return;

	for (i = 0; i < max_cpus; i++)
		avg_cpu_loading += ci.cpu_loading[i];
	avg_cpu_loading /= max_cpus;

	cur_qualified = avg_cpu_loading >= cpu_loading_thres;
	status_3rd_cam = get_cam_status_for_task_turbo();
	if (touch_countdown_timer > 0) {
		touch_countdown_timer--;
		touching = true;
		p = (struct task_struct *)atomic_long_read(&inputDispatcher_ptr);
	}
	mutex_lock(&wi_tbl_lock);
	if (fps_drop_vip_enable > 0) {
		if (fps_drop_countdown_timer > 0) {
			cur_qualified |= true;
			fps_drop_countdown_timer--;
		} else {
			for (tbl_i = 0; tbl_i < wi_tbl.size; tbl_i++) {
				if (ktime_sub(ktime_to_ms(ktime_get()), fps_drop_hint_for_task_turbo()) <= 1000)
					fps_drop = true;
				if (wi_tbl.wi[tbl_i].perf_idx == 100 && fps_drop) {
					cur_qualified |= true;
					fps_drop_countdown_timer = fps_drop_vip_throttle;
				}
			}
		}
	} else {
		fps_drop_countdown_timer = 0;
	}
	if ((!win_vip_status && !cur_qualified) || (last_wi_tbl.size == 0 && wi_tbl.size == 0) ||
		(wi_tbl.size == 0 && !win_vip_status) || (last_wi_tbl.size == 0 && !cur_qualified))
		goto out_unlock;

	if ((last_wi_tbl.size > 0 && win_vip_status) && (wi_tbl.size == 0 || !cur_qualified))
		need_clean = true;

	if (last_wi_tbl.size == wi_tbl.size) {
		for (tbl_i = 0; tbl_i < wi_tbl.size; tbl_i++) {
			if (wi_tbl.wi[tbl_i].tgid != last_wi_tbl.wi[tbl_i].tgid) {
				content_changed = true;
				break;
			}
		}
	} else
		content_changed = true;

	if (wi_tbl.size > 0 && cur_qualified) {
		need_set = true;
		if (last_wi_tbl.size > 0 && win_vip_status && content_changed)
			need_clean = true;
	}

	if (need_clean) {
		win_vip_status = false;
		for (tbl_i = 0; tbl_i < last_wi_tbl.size; tbl_i++) {
			unset_task_basic_vip(last_wi_tbl.wi[tbl_i].tgid);
			unset_tgid_basic_vip(last_wi_tbl.wi[tbl_i].tgid);
		}
	}
	if (cam_hal_tgid > 0 && (need_clean || !status_3rd_cam)) {
		unset_task_basic_vip(cam_hal_tgid);
		unset_tgid_basic_vip(cam_hal_tgid);
		cam_hal_tgid = -1;
	}
	if (cam_svr_tgid > 0 && (need_clean || !status_3rd_cam)) {
		unset_task_basic_vip(cam_svr_tgid);
		unset_tgid_basic_vip(cam_svr_tgid);
		cam_svr_tgid = -1;
	}
	if (touch_vip_status && (need_clean || !touching)) {
		touch_vip_status = false;
		unset_task_basic_vip(last_ts_inputDispatcher_tgid);
		unset_tgid_basic_vip(last_ts_inputDispatcher_tgid);
	}
	if (need_set) {
		win_vip_status = true;
		for (tbl_i = 0; tbl_i < wi_tbl.size; tbl_i++) {
			set_task_basic_vip(wi_tbl.wi[tbl_i].tgid);
			set_tgid_basic_vip(wi_tbl.wi[tbl_i].tgid);
		}
		if (status_3rd_cam) {
			if (cam_hal_tgid <= 0)
				cam_hal_tgid = get_cam_hal_pid_for_task_turbo();
			if (cam_hal_tgid > 0 && cam_hal_tgid <= PID_MAX_DEFAULT) {
				set_task_basic_vip(cam_hal_tgid);
				set_tgid_basic_vip(cam_hal_tgid);
			}
			if (cam_svr_tgid <= 0)
				cam_svr_tgid = get_cam_server_pid_for_task_turbo();
			if (cam_svr_tgid > 0 && cam_svr_tgid <= PID_MAX_DEFAULT) {
				set_task_basic_vip(cam_svr_tgid);
				set_tgid_basic_vip(cam_svr_tgid);
			}
		}
		if (touching) {
			touch_vip_status = true;
			set_task_basic_vip(p->tgid);
			set_tgid_basic_vip(p->tgid);
			last_ts_inputDispatcher_tgid = p->tgid;
		}
	}
	update_last_win_info_tbl(&last_wi_tbl, &wi_tbl);

out_unlock:
	mutex_unlock(&wi_tbl_lock);
}
module_param(cpu_loading_thres, int, 0644);
module_param(fps_drop_vip_throttle, int, 0644);
module_param(fps_drop_vip_enable, int, 0644);

static void find_inputDispatcher(void)
{
	struct task_struct *p;

	rcu_read_lock();
	for_each_process(p) {
		if (strstr(p->comm, "system_server")) {
			ts_inputDispatcher = p;
			atomic_long_set(&inputDispatcher_ptr, (long)p);
			break;
		}
	}
	rcu_read_unlock();
}

static void tt_input_event(struct input_handle *handle, unsigned int type,
						   unsigned int code, int value)
{
	struct task_struct *p;

	if (tt_vip_enable > 0) {
		p = (struct task_struct *)atomic_long_read(&inputDispatcher_ptr);
		if (type == EV_KEY && code == BTN_TOUCH && value == TOUCH_DOWN) {
			if (!(p && strstr(p->comm, "system_server")))
				find_inputDispatcher();
			touch_countdown_timer = 2;
			queue_work(system_highpri_wq, &tt_vip);
		} else {
			touch_countdown_timer = 2;
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

void fpsgo2tt_hint_perf_idx(int pid, unsigned long long bufID,
	int perf_idx, int sbe_ctrl, unsigned long long ts)
{
	struct task_struct *p;
	int tgid;
	size_t i;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (!p) {
		rcu_read_unlock();
		return;
	}
	tgid = p->tgid;
	rcu_read_unlock();

	mutex_lock(&wi_tbl_lock);
	for (i = 0; i < wi_tbl.size; i++) {
		if (wi_tbl.wi[i].tgid == tgid) {
			wi_tbl.wi[i].perf_idx = perf_idx;
			break;
		}
	}
	mutex_unlock(&wi_tbl_lock);
}

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

	if (tt_vip_enable > 0) {
		wallclock = ktime_get_ns();
		if (!tt_vip_do_check(wallclock))
			return;

		queue_work(system_highpri_wq, &tt_vip);
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

#if IS_ENABLED(CONFIG_MTK_FPSGO_V3) || IS_ENABLED(CONFIG_MTK_FPSGO)
	fpsgo_other2fstb_register_perf_callback(FPSGO_PERF_IDX,
		fpsgo2tt_hint_perf_idx);
#endif

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
