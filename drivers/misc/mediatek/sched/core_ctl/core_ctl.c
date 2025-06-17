// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/cpumask.h>
#include <linux/cpufreq.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/syscore_ops.h>
#include <linux/module.h>
#include <uapi/linux/sched/types.h>
#include <trace/hooks/sched.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include <linux/sched/clock.h>

#ifndef __CHECKER__
#define CREATE_TRACE_POINTS
#include "core_ctl_trace.h"
#endif

#include "sched_avg.h"
#include "core_ctl.h"
#include "common.h"
#include <sched_sys_common.h>
#include <eas/eas_plus.h>
#include <mtk_cpu_power_throttling.h>
#include <mt-plat/mtk_irq_mon.h>

#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
#include <thermal_interface.h>
#endif

#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_6893)
extern int get_immediate_tslvts1_1_wrap(void);
#endif

#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
#include "eas/grp_awr.h"
#endif

#define TAG	"core_ctl"
#define L_CLUSTER_ID	0
#define M_CLUSTER_ID	1
#define B_CLUSTER_ID	2
#define MAX_CLUSTERS	3
#define MAX_CPUS_PER_CLUSTER	6
#define MAX_BTASK_THRESH	100
#define BIG_TASK_AVG_THRESHOLD	25
#define WIN_SIZE	10
#define NORMAL_TEMP	35
#define THERMAL_TEMP	65

#define for_each_cluster(cluster, idx) \
	for ((cluster) = &cluster_state[idx]; (idx) < num_clusters;\
		(idx)++, (cluster) = &cluster_state[idx])

#define core_ctl_debug(x...)		\
	do {				\
		if (debug_enable >= DEBUG_DETAIL)	\
			pr_info(x);	\
	} while (0)

struct request_data {
	unsigned int have_demand;
	unsigned int min_cpus;
	unsigned int max_cpus;
};

struct ppm_table {
	unsigned long power;
	unsigned int leakage;
	unsigned int thermal_leakage;
	unsigned int freq;
	unsigned int capacity;
	unsigned long eff;
	unsigned long thermal_eff;
};

struct cluster_ppm_data {
	bool init;
	int opp_nr;
	struct ppm_table *ppm_tbl;
};

struct cluster_data {
	bool inited;
	bool enable;
	int nr_up;
	int nr_down;
	int need_spread_cpus;
	unsigned int cluster_id;
	unsigned int min_cpus;
	unsigned int max_cpus;
	unsigned int first_cpu;
	unsigned int active_cpus;
	unsigned int num_cpus;
	unsigned int nr_assist;
	unsigned int up_thres;
	unsigned int down_thres;
	unsigned int thermal_degree_thres;
	unsigned int thermal_up_thres;
	unsigned int nr_not_preferred_cpus;
	unsigned int need_cpus;
	unsigned int new_need_cpus;
	unsigned int boost;
	unsigned int nr_paused_cpus;
	unsigned int cpu_busy_up_thres;
	unsigned int cpu_busy_down_thres;
	unsigned int nr_task_thres;
	unsigned int rt_nr_task_thres;
	unsigned int active_loading_thres;
	cpumask_t cpu_mask;
	bool pending;
	spinlock_t pending_lock;
	struct task_struct *core_ctl_thread;
	struct kobject kobj;
	s64 offline_throttle_ms;
	s64 next_offline_time;
	struct request_data demand_list[MAX_DEMAND_REQUESTER];
	struct cluster_ppm_data ppm_data;
	unsigned int cap_turn_point;
	unsigned long freq_thres;
	bool boost_by_freq;
	bool boost_by_wlan;
	unsigned int deiso_reason;
};

struct cpu_data {
	bool not_preferred;
	bool paused_by_cc;
	bool force_paused;
	unsigned int cpu;
	unsigned int cpu_util_pct[WIN_SIZE];
	unsigned int is_busy;
	struct cluster_data *cluster;
	unsigned int force_pause_req;
	unsigned int win_idx;
	unsigned int cpu_active_loading[WIN_SIZE];
	u64 last_wall_time;
	u64 last_idle_time;
	int max_nr_state;
	int max_rt_nr_state;
	int max_vip_nr_state;
};

static unsigned int enable_policy;
static DEFINE_PER_CPU(struct cpu_data, cpu_state);
static struct cluster_data cluster_state[MAX_CLUSTERS];
static unsigned int num_clusters;
static DEFINE_SPINLOCK(core_ctl_state_lock);
static DEFINE_SPINLOCK(core_ctl_window_check_lock);
static DEFINE_SPINLOCK(core_ctl_pause_lock);
static bool initialized;
ATOMIC_NOTIFIER_HEAD(core_ctl_notifier);
static unsigned int default_min_cpus[MAX_CLUSTERS] = {4, 0, 0};
static unsigned int busy_up_thres[MAX_CLUSTERS] = {87, 87, 75};
static unsigned int busy_down_thres[MAX_CLUSTERS] = {57, 57, 45};
static unsigned int nr_task_thres[MAX_CLUSTERS] = {4, 4, 4};
static unsigned int rt_nr_task_thres[MAX_CLUSTERS] = {2, 2, 2};
static unsigned int active_loading_thres[MAX_CLUSTERS] = {80, 80, 80};
static unsigned long freq_thres[MAX_CLUSTERS] = {5000000, 5000000, 5000000};
struct cpumask cpu_force_pause_mask;
static unsigned int consider_VIP_task;

enum {
	DEISO_NONE	= 0,
	DEISO_BUSY_NR_TASK	= 1,
	DEISO_BUSY_ACT_LOAD	= 2,
	DEISO_BUSY_RT_NR_TASK	= 4,
	DEISO_HEAVY_TASK	= 8,
	DEISO_NORMAL_TASK	= 16,
	DEISO_OVER_FREQ_THRES	= 32,
	DEISO_WLAN_REQ	= 64
};

/* ==================== module parameter ======================== */

enum {
	DISABLE_DEBUG = 0,
	DEBUG_PERIODIC,
	DEBUG_DETAIL,
	DEBUG_CNT
};

static int debug_enable = DEBUG_PERIODIC;
module_param_named(debug_enable, debug_enable, int, 0600);

static void periodic_debug_handler(struct work_struct *work);
static int periodic_debug_enable = 1;
static int periodic_debug_delay = 2000;
module_param_named(periodic_debug_delay, periodic_debug_delay, int, 0600);
static DECLARE_DELAYED_WORK(periodic_debug, periodic_debug_handler);

static int set_core_ctl_debug_level(const char *buf,
			       const struct kernel_param *kp)
{
	int ret = 0;
	unsigned int val = 0;

	ret = kstrtouint(buf, 0, &val);
	if (val >= DEBUG_CNT)
		ret = -EINVAL;

	if (!ret) {
		if (periodic_debug_enable != val) {
			periodic_debug_enable = val;
			if (periodic_debug_enable > DISABLE_DEBUG)
				mod_delayed_work(system_power_efficient_wq,
						&periodic_debug,
						msecs_to_jiffies(periodic_debug_delay));
		}
	}
	return ret;
}

static struct kernel_param_ops set_core_ctl_debug_param_ops = {
	.set = set_core_ctl_debug_level,
	.get = param_get_uint,
};

param_check_uint(periodic_debug_enable, &periodic_debug_enable);
module_param_cb(periodic_debug_enable, &set_core_ctl_debug_param_ops, &periodic_debug_enable, 0600);
MODULE_PARM_DESC(periodic_debug_enable, "echo periodic debug trace if needed");

static void periodic_debug_handler(struct work_struct *work)
{
	struct cluster_data *cluster;
	unsigned int index = 0;
	unsigned int max_cpus[MAX_CLUSTERS];
	unsigned int min_cpus[MAX_CLUSTERS];
	unsigned int boost[MAX_CLUSTERS];
	unsigned int need_cpus[MAX_CLUSTERS];
	int gas_enable = 0;

	mod_delayed_work(system_power_efficient_wq,
			&periodic_debug, msecs_to_jiffies(periodic_debug_delay));

	if (periodic_debug_enable == DISABLE_DEBUG)
		return;

#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
	gas_enable = get_top_grp_aware();
#endif

	for_each_cluster(cluster, index) {
		max_cpus[index] = cluster->max_cpus;
		min_cpus[index] = cluster->min_cpus;
		boost[index] = cluster->boost || cluster->boost_by_freq || cluster->boost_by_wlan;
		need_cpus[index] = cluster->need_cpus;
	}

	pr_info("%s: en=%u max=%u|%u|%u min=%u|%u|%u need=%u|%u|%u bst=%u|%u|%u gas=%d act=%lx iso=%lx f_iso=%lx",
			TAG, enable_policy,
			max_cpus[0], max_cpus[1], max_cpus[2],
			min_cpus[0], min_cpus[1], min_cpus[2],
			need_cpus[0], need_cpus[1], need_cpus[2],
			boost[0], boost[1], boost[2],
			gas_enable, cpumask_bits(cpu_online_mask)[0],
			cpumask_bits(cpu_pause_mask)[0], cpumask_bits(&cpu_force_pause_mask)[0]);
}

/*
 *  core_ctl_enable_policy - enable policy of core control
 *  @enable: true if set, false if unset.
 */
int core_ctl_enable_policy(unsigned int policy)
{
	unsigned int old_val;
	bool success = false;

	if (policy != enable_policy) {
		old_val = enable_policy;
		enable_policy = policy;
		success = true;

		/* reset recorded data when turn on */
		if (!old_val && enable_policy)
			policy_chg_notify();
	}

	if (success)
		pr_info("%s: Change policy from %d to %d successfully.",
			TAG, old_val, policy);
	return 0;
}
EXPORT_SYMBOL(core_ctl_enable_policy);

static int set_core_ctl_policy(const char *buf,
			       const struct kernel_param *kp)
{
	int ret = 0;
	unsigned int val = 0;

	ret = kstrtouint(buf, 0, &val);
	if (val >= POLICY_CNT)
		ret = -EINVAL;

	if (!ret)
		core_ctl_enable_policy(val);
	return ret;
}

static struct kernel_param_ops set_core_ctl_policy_param_ops = {
	.set = set_core_ctl_policy,
	.get = param_get_uint,
};

param_check_uint(policy_enable, &enable_policy);
module_param_cb(policy_enable, &set_core_ctl_policy_param_ops, &enable_policy, 0600);
MODULE_PARM_DESC(policy_enable, "echo cpu pause policy if needed");

/* ==================== support function ======================== */

static unsigned int apply_limits(const struct cluster_data *cluster,
				 unsigned int need_cpus)
{
	return min(max(cluster->min_cpus, need_cpus), cluster->max_cpus);
}

/**
 * cpumask_complement - *dstp = ~*srcp
 * @dstp: the cpumask result
 * @srcp: the input to invert
 */
static inline void cpumask_complement(struct cpumask *dstp,
				      const struct cpumask *srcp)
{
	bitmap_complement(cpumask_bits(dstp), cpumask_bits(srcp),
					      nr_cpumask_bits);
}

int sched_isolate_count(const cpumask_t *mask, bool include_offline)
{
	cpumask_t count_mask = CPU_MASK_NONE;

	if (include_offline) {
		cpumask_complement(&count_mask, cpu_online_mask);
		cpumask_or(&count_mask, &count_mask, cpu_pause_mask);
		cpumask_and(&count_mask, &count_mask, mask);
	} else
		cpumask_and(&count_mask, mask, cpu_pause_mask);

	return cpumask_weight(&count_mask);
}

static unsigned int get_active_cpu_count(const struct cluster_data *cluster)
{
	return cluster->num_cpus -
		sched_isolate_count(&cluster->cpu_mask, true);
}

static bool is_active(const struct cpu_data *state)
{
	return cpu_online(state->cpu) && !cpu_paused(state->cpu);
}

static bool adjustment_possible(const struct cluster_data *cluster,
				unsigned int need)
{
	/*
	 * Why need to check nr_paused_cpu ?
	 * Consider the following situation,
	 * num_cpus = 4, min_cpus = 4 and a cpu
	 * force paused. That will do inactive
	 * core-on in the time.
	 */
	return (need < cluster->active_cpus || (need > cluster->active_cpus
			&& cluster->nr_paused_cpus));
}

static void wake_up_core_ctl_thread(struct cluster_data *cluster)
{
	unsigned long flags;

	spin_lock_irqsave(&cluster->pending_lock, flags);
	cluster->pending = true;
	spin_unlock_irqrestore(&cluster->pending_lock, flags);

	wake_up_process(cluster->core_ctl_thread);
}

bool is_cluster_init(unsigned int cid)
{
	return  cluster_state[cid].inited;
}

static int power_cost_evaluation(struct cluster_data *cluster)
{
	struct cluster_data *prev_cluster;
	struct cpu_data *cpu_stat;
	int cpu, cid = 0, win_idx = 0;
	int need_isolated_cpu;
	unsigned long cap_avg = 0, prev_cap_avg = 0, prev_new_cap;
	unsigned int ret = 1;

	if (unlikely(!cluster || !cluster->inited))
		return ret;

	cid = cluster->cluster_id;

	if (cid == L_CLUSTER_ID)
		return ret;

	need_isolated_cpu = cluster->active_cpus - cluster->new_need_cpus;
	prev_cluster = &cluster_state[cid - 1];

	for_each_cpu(cpu, &cluster->cpu_mask) {
		cpu_stat = &per_cpu(cpu_state, cpu);
		win_idx = cpu_stat->win_idx;
		cap_avg += _capacity_of(cpu) * cpu_stat->cpu_util_pct[win_idx];
	}
	cap_avg = div_u64(cap_avg, cluster->active_cpus * 100);

	for_each_cpu(cpu, &prev_cluster->cpu_mask) {
		cpu_stat = &per_cpu(cpu_state, cpu);
		win_idx = cpu_stat->win_idx;
		prev_cap_avg += _capacity_of(cpu) * cpu_stat->cpu_util_pct[win_idx];
	}
	prev_cap_avg = div_u64(prev_cap_avg, prev_cluster->active_cpus * 100);

	prev_new_cap = prev_cap_avg + div_u64(need_isolated_cpu*cap_avg, prev_cluster->active_cpus);
	if (prev_new_cap > prev_cluster->cap_turn_point)
		ret = 0;

	return ret;
}

static void check_freq_min_req(void)
{
	unsigned int index = 0;
	unsigned long flags;
	struct cluster_data *cluster;
	unsigned long last_freq_qos_max_of_min;

	/* no need check with camera mode */
	if (enable_policy == CAMERA_MODE)
		return;

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	for_each_cluster(cluster, index) {
		last_freq_qos_max_of_min = get_freq_qos_max_of_min(index);
		if (last_freq_qos_max_of_min > cluster->freq_thres) {
			cluster->deiso_reason |= DEISO_OVER_FREQ_THRES;
			cluster->boost_by_freq = true;
		}
		else
			cluster->boost_by_freq = false;
	}
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
}

uint32_t (*wlan_cur_cpumask_req_hook)(void);
EXPORT_SYMBOL(wlan_cur_cpumask_req_hook);

static void check_wlan_request(void)
{
	int i;
	unsigned long flags;
	uint32_t wlan_cpu_req, cluster_cpu_mask;
	struct cluster_data *cluster;

	if (wlan_cur_cpumask_req_hook) {
		wlan_cpu_req = wlan_cur_cpumask_req_hook();

		spin_lock_irqsave(&core_ctl_state_lock, flags);
		cluster = &cluster_state[B_CLUSTER_ID];
		cluster_cpu_mask = 0;
		for_each_cpu(i, &cluster->cpu_mask)
			cluster_cpu_mask |= (1U << i);
		cluster->boost_by_wlan = (wlan_cpu_req & cluster_cpu_mask)?true:false;
		if (cluster->boost_by_wlan)
			cluster->deiso_reason |= DEISO_WLAN_REQ;

		cluster = &cluster_state[M_CLUSTER_ID];
		cluster_cpu_mask = 0;
		for_each_cpu(i, &cluster->cpu_mask)
			cluster_cpu_mask |= (1U << i);
		cluster->boost_by_wlan = (wlan_cpu_req & cluster_cpu_mask)?true:false;
		if (cluster->boost_by_wlan)
			cluster->deiso_reason |= DEISO_WLAN_REQ;
		spin_unlock_irqrestore(&core_ctl_state_lock, flags);
	}
}

static bool demand_eval(struct cluster_data *cluster)
{
	unsigned long flags;
	unsigned int need_cpus = 0;
	bool ret = false;
	bool need_flag = false;
	unsigned int cost_flag = 0;
	unsigned int new_need;
	unsigned int old_need;
	s64 now, elapsed;
	unsigned int cluster_boost_state, boost_state;
	unsigned int cid, active_cpus, min_cpus, max_cpus, enable, next_offline_time;
	int gas_enable = 0;

	irq_log_store();

	if (unlikely(!cluster->inited))
		return ret;

#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
	gas_enable = get_top_grp_aware();
#endif

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	cid = cluster->cluster_id;
	active_cpus = cluster->active_cpus;
	min_cpus = cluster->min_cpus;
	max_cpus = cluster->max_cpus;
	enable = cluster->enable;
	cluster_boost_state = cluster->boost;

	boost_state = cluster->boost || cluster->boost_by_wlan || cluster->boost_by_freq;
	if (boost_state || !cluster->enable || !enable_policy || gas_enable)
		need_cpus = cluster->max_cpus;
	else
		need_cpus = cluster->new_need_cpus;

	/* check again active cpus. */
	cluster->active_cpus = get_active_cpu_count(cluster);
	new_need = apply_limits(cluster, need_cpus);
	/*
	 * When there is no adjustment in need, avoid
	 * unnecessary waking up core_ctl thread
	 */
	need_flag = adjustment_possible(cluster, new_need);
	old_need = cluster->need_cpus;

	now = ktime_to_ms(ktime_get());

	/* core-on */
	if (new_need > cluster->active_cpus) {
		ret = true;
	} else {
		/*
		 * If no more CPUs are needed or paused,
		 * just update the next offline time.
		 */
		if (new_need == cluster->active_cpus) {
			cluster->next_offline_time = now;
			cluster->need_cpus = new_need;
			next_offline_time = now;
			goto unlock;
		}

		/* Does it exceed throttle time ? */
		elapsed = now - cluster->next_offline_time;
		ret = elapsed >= cluster->offline_throttle_ms;

		/* Calculate effective of isolation when core-off */
		if (ret) {
			cost_flag = power_cost_evaluation(cluster);
			ret = ret & cost_flag;
		}
	}

	if (ret) {
		cluster->next_offline_time = now;
		cluster->need_cpus = new_need;
	}
	next_offline_time = cluster->next_offline_time;
unlock:
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);

	if (new_need != active_cpus) {
		trace_core_ctl_demand_eval(cid,
				old_need, new_need,
				active_cpus,
				min_cpus, max_cpus,
				cluster_boost_state,
				gas_enable,
				enable, cost_flag,
				ret && need_flag,
				next_offline_time);
	}

	irq_log_store();
	return ret && need_flag;
}

static void apply_demand(struct cluster_data *cluster)
{
	if (demand_eval(cluster))
		wake_up_core_ctl_thread(cluster);
}

static int test_set_val(struct cluster_data *cluster, unsigned int val)
{
	unsigned long flags;
	struct cpumask disable_mask;
	unsigned int disable_cpus;
	unsigned int test_cluster_min_cpus[MAX_CLUSTERS];
	unsigned int total_min_cpus = 0;
	int i;

	cpumask_clear(&disable_mask);
	cpumask_complement(&disable_mask, cpu_online_mask);
	cpumask_or(&disable_mask, &disable_mask, cpu_pause_mask);
	cpumask_and(&disable_mask, &disable_mask, cpu_possible_mask);
	disable_cpus = cpumask_weight(&disable_mask);
	if (disable_cpus >= (nr_cpu_ids-2))
		return -EINVAL;

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	for(i=0; i<MAX_CLUSTERS; i++)
		test_cluster_min_cpus[i] = cluster_state[i].min_cpus;

	test_cluster_min_cpus[cluster->cluster_id] = val;
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);

	for(i=0; i<MAX_CLUSTERS; i++)
		total_min_cpus += test_cluster_min_cpus[i];
	if (total_min_cpus < 2){
		pr_info("%s: invalid setting, retain cpus should >= 2", TAG);
		return -EINVAL;
	}

	return 0;
}

static bool test_disable_cpu(unsigned int cpu);
static inline int core_ctl_pause_cpu(unsigned int cpu)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&core_ctl_pause_lock, flags);
	/* Unable to pause CPU */
	if(!test_disable_cpu(cpu)) {
		spin_unlock_irqrestore(&core_ctl_pause_lock, flags);
		return -EBUSY;
	}
	ret = sched_pause_cpu(cpu);
	spin_unlock_irqrestore(&core_ctl_pause_lock, flags);

	return ret;
}

static inline int core_ctl_resume_cpu(unsigned int cpu)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&core_ctl_pause_lock, flags);
	ret = sched_resume_cpu(cpu);
	spin_unlock_irqrestore(&core_ctl_pause_lock, flags);

	return ret;
}

int core_ctl_cpu_request_trace(void);
static void set_min_cpus(struct cluster_data *cluster, unsigned int val, int requester, unsigned int have_demand)
{
	unsigned long flags;
	unsigned int i, selected = UINT_MAX;
	unsigned int max_min = 0;

	/* check CPU boundary */
	if (val < cluster->min_cpus) {
		if (test_set_val(cluster, val))
			return;
	}

	if (requester < 0 || requester >= MAX_DEMAND_REQUESTER)
		return;

	spin_lock_irqsave(&core_ctl_state_lock, flags);

	/* update requester demand */
	cluster->demand_list[requester].have_demand = have_demand;
	cluster->demand_list[requester].min_cpus = val;

	/* Find biggest demand of min_cpus */
	for (i=0; i<MAX_DEMAND_REQUESTER; i++) {
		if (cluster->demand_list[i].have_demand && (cluster->demand_list[i].min_cpus >= max_min)) {
			selected = i;
			max_min = cluster->demand_list[i].min_cpus;
		}
	}
	core_ctl_debug("%s: cluster#%d max_min=%d demand aggregate from %u",
		TAG, cluster->cluster_id, max_min, selected);

	/* Aggregate with max_cpus */
	if (selected != UINT_MAX)
		cluster->min_cpus = min(max_min, cluster->max_cpus);
	else
		cluster->min_cpus = default_min_cpus[cluster->cluster_id];
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
	core_ctl_cpu_request_trace();
	wake_up_core_ctl_thread(cluster);
}

static void set_max_cpus(struct cluster_data *cluster, unsigned int val, int requester, unsigned int have_demand)
{
	unsigned long flags;
	unsigned int i, selected = UINT_MAX;
	unsigned int min_max = UINT_MAX;

	/* check CPU boundary */
	if(val < cluster->min_cpus){
		if (test_set_val(cluster, val))
			return;
	}

	if (requester < 0 || requester >= MAX_DEMAND_REQUESTER)
		return;

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	/* update requester demand */
	cluster->demand_list[requester].have_demand = have_demand;
	cluster->demand_list[requester].max_cpus = val;

	/* Find smallest demand of max_cpus */
	for (i=0; i<MAX_DEMAND_REQUESTER; i++) {
		if (cluster->demand_list[i].have_demand && (cluster->demand_list[i].max_cpus <= min_max)) {
			selected = i;
			min_max = cluster->demand_list[i].max_cpus;
		}
	}
	core_ctl_debug("%s: cluster#%d min_max=%d demand aggregate from %u",
		TAG, cluster->cluster_id, min_max, selected);

	if (selected != UINT_MAX) {
		min_max = min(min_max, cluster->num_cpus);
		cluster->max_cpus = min_max;

		/* Aggregate with max_cpus */
		cluster->min_cpus = min(cluster->min_cpus, cluster->max_cpus);
	} else
		cluster->max_cpus = cluster->num_cpus;
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
	core_ctl_cpu_request_trace();
	wake_up_core_ctl_thread(cluster);
}

static void set_thermal_up_thres(struct cluster_data *cluster, unsigned int val)
{
	unsigned long flags;

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	cluster->thermal_up_thres = val;
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
	wake_up_core_ctl_thread(cluster);
}

void set_offline_throttle_ms(struct cluster_data *cluster, unsigned int val)
{
	unsigned long flags;

	core_ctl_debug("%s: Adjust offline throttle time to %u ms", TAG, val);
	spin_lock_irqsave(&core_ctl_state_lock, flags);
	cluster->offline_throttle_ms = val;
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
	apply_demand(cluster);
}

static inline
void update_next_cluster_down_thres(unsigned int index,
				     unsigned int new_thresh)
{
	struct cluster_data *next_cluster;

	if (index == num_clusters - 1)
		return;

	next_cluster = &cluster_state[index + 1];
	next_cluster->down_thres = new_thresh;
}

static inline
void set_not_preferred_locked(int cpu, bool enable)
{
	struct cpu_data *c;
	struct cluster_data *cluster;
	bool changed = false;

	c = &per_cpu(cpu_state, cpu);
	cluster = c->cluster;
	if (enable) {
		changed = !c->not_preferred;
		c->not_preferred = 1;
	} else {
		if (c->not_preferred) {
			c->not_preferred = 0;
			changed = !c->not_preferred;
		}
	}

	if (changed) {
		if (enable)
			cluster->nr_not_preferred_cpus += 1;
		else
			cluster->nr_not_preferred_cpus -= 1;
	}
}

unsigned long get_freq_thres(unsigned int cid);
static int set_up_thres(struct cluster_data *cluster, unsigned int val)
{
	unsigned int old_thresh;
	unsigned long flags;
	int ret = 0;

	if (val > MAX_BTASK_THRESH)
		val = MAX_BTASK_THRESH;

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	old_thresh = cluster->up_thres;

	if (old_thresh != val) {
		ret = set_over_threshold(cluster->cluster_id, val);
		if (!ret) {
			cluster->up_thres = val;
			update_next_cluster_down_thres(
				cluster->cluster_id,
				cluster->up_thres);
		}
		cluster->freq_thres = get_freq_thres(cluster->cluster_id);
	}
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
	return ret;
}

static void set_cpu_busy_up_thres(struct cluster_data *cluster, unsigned int val)
{
	unsigned int old_thresh;
	unsigned long flags;

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	old_thresh = cluster->cpu_busy_up_thres;
	if (old_thresh != val)
		cluster->cpu_busy_up_thres = val;
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
}

static void set_cpu_busy_down_thres(struct cluster_data *cluster, unsigned int val)
{
	unsigned int old_thresh;
	unsigned long flags;

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	old_thresh = cluster->cpu_busy_down_thres;
	if (old_thresh != val)
		cluster->cpu_busy_down_thres = val;
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
}

static void set_cpu_nr_task_thres(struct cluster_data *cluster, unsigned int val)
{
	unsigned int old_thresh;
	unsigned long flags;

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	old_thresh = cluster->nr_task_thres;
	if (old_thresh != val)
		cluster->nr_task_thres = val;
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
}

static void set_cpu_rt_nr_task_thres(struct cluster_data *cluster, unsigned int val)
{
	unsigned int old_thresh;
	unsigned long flags;

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	old_thresh = cluster->rt_nr_task_thres;
	if (old_thresh != val)
		cluster->rt_nr_task_thres = val;
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
}

static void set_cpu_active_loading_thres(struct cluster_data *cluster, unsigned int val)
{
	unsigned int old_thresh;
	unsigned long flags;

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	old_thresh = cluster->active_loading_thres;
	if (old_thresh != val)
		cluster->active_loading_thres = val;
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
}

static void set_freq_min_thres(struct cluster_data *cluster, unsigned int val)
{
	unsigned int old_thresh;
	unsigned long flags;

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	old_thresh = cluster->freq_thres;
	if (old_thresh != val)
		cluster->freq_thres = val;
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
}

/*
 * get_cpu_active_loading - Calculates the CPU loading for each CPU
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
static int get_cpu_active_loading(void)
{
	unsigned long flags;
	int cpu;
	unsigned int cpu_active_loading, idx;
	struct cpu_data *cpu_stat;
	u64 prev_wall_time, prev_idle_time, wall_time, idle_time;
	unsigned int cpu_active_loading_state[MAX_NR_CPUS], cpu_util_state[MAX_NR_CPUS];

	if (!initialized)
		return 0;

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	for_each_possible_cpu(cpu) {
		cpu_stat = &per_cpu(cpu_state, cpu);
		cpu_active_loading = 0;
		wall_time = 0;
		idle_time = 0;

		prev_wall_time = cpu_stat->last_wall_time;
		prev_idle_time = cpu_stat->last_idle_time;

		/* update both wall & idle time, and idle time include iowait */
		cpu_stat->last_idle_time = get_cpu_idle_time(cpu, &cpu_stat->last_wall_time, 1);

		if (cpu_active(cpu)) {
			wall_time = cpu_stat->last_wall_time - prev_wall_time;
			idle_time = cpu_stat->last_idle_time - prev_idle_time;
		}

		if (wall_time > 0 && wall_time > idle_time)
			cpu_active_loading = div_u64(((wall_time - idle_time)*100),
							wall_time);

		cpu_stat->win_idx = ((cpu_stat->win_idx + 1) % WIN_SIZE);
		idx = cpu_stat->win_idx;
		if (idx < WIN_SIZE) {
			cpu_stat->cpu_active_loading[idx] = cpu_active_loading;
			cpu_stat->cpu_util_pct[idx] = get_cpu_util_pct(cpu, false);
		}

		cpu_active_loading_state[cpu] = cpu_stat->cpu_active_loading[idx];
		cpu_util_state[cpu] = cpu_stat->cpu_util_pct[idx];
	}
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);

	trace_core_ctl_cpu_loading_util(cpu_active_loading_state, cpu_util_state);
	return 0;
}

int core_ctl_cpu_request_trace(void)
{
	struct cluster_data *cluster;
	struct cpu_data *cpu_stat;
	int cid, req_id, cpu;
	unsigned long flags;
	int min_cpus[MAX_CLUSTERS], max_cpus[MAX_CLUSTERS];
	unsigned int min_cpus_req[MAX_CLUSTERS][MAX_DEMAND_REQUESTER];
	unsigned int max_cpus_req[MAX_CLUSTERS][MAX_DEMAND_REQUESTER];
	unsigned int have_demand[MAX_CLUSTERS][MAX_DEMAND_REQUESTER];
	unsigned int force_pause_req[MAX_NR_CPUS];

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	for (cid=1; cid<MAX_CLUSTERS; cid++) {
		cluster = &cluster_state[cid];

		for (req_id=0; req_id < MAX_DEMAND_REQUESTER; req_id++) {
			have_demand[cid][req_id] = cluster->demand_list[req_id].have_demand;
			min_cpus_req[cid][req_id] = cluster->demand_list[req_id].min_cpus;
			max_cpus_req[cid][req_id] = cluster->demand_list[req_id].max_cpus;
		}
		min_cpus[cid] = cluster->min_cpus;
		max_cpus[cid] = cluster->max_cpus;
	}

	for (cpu=0; cpu<MAX_NR_CPUS; cpu++) {
		cpu_stat = &per_cpu(cpu_state, cpu);
		force_pause_req[cpu] = cpu_stat->force_pause_req;
	}
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);

	for (cid=1; cid<MAX_CLUSTERS; cid++) {
		trace_core_ctl_cpu_request(cid, min_cpus[cid], max_cpus[cid],
				have_demand[cid], min_cpus_req[cid], max_cpus_req[cid], force_pause_req);
	}

	return 0;
}

/* ==================== export function ======================== */

int core_ctl_get_min_cpus(unsigned int cid)
{
	struct cluster_data *cluster;
	if (cid >= num_clusters)
		return -EINVAL;

	cluster = &cluster_state[cid];
	return cluster->min_cpus;
}
EXPORT_SYMBOL(core_ctl_get_min_cpus);

int core_ctl_set_min_cpus(unsigned int cid, unsigned int min, int requester, unsigned int have_demand)
{
	struct cluster_data *cluster;

	if (cid >= num_clusters)
		return -EINVAL;
	if (requester < 0 || requester >= MAX_DEMAND_REQUESTER)
		return -EINVAL;

	cluster = &cluster_state[cid];
	set_min_cpus(cluster, min, requester, have_demand);
	return 0;
}
EXPORT_SYMBOL(core_ctl_set_min_cpus);

int core_ctl_get_max_cpus(unsigned int cid)
{
	struct cluster_data *cluster;
	if (cid >= num_clusters)
		return -EINVAL;

	cluster = &cluster_state[cid];
	return cluster->max_cpus;
}
EXPORT_SYMBOL(core_ctl_get_max_cpus);

int core_ctl_set_max_cpus(unsigned int cid, unsigned int max, int requester, unsigned int have_demand)
{
	struct cluster_data *cluster;

	if (cid >= num_clusters)
		return -EINVAL;
	if (requester < 0 || requester >= MAX_DEMAND_REQUESTER)
		return -EINVAL;

	cluster = &cluster_state[cid];
	set_max_cpus(cluster, max, requester, have_demand);
	return 0;
}
EXPORT_SYMBOL(core_ctl_set_max_cpus);

/*
 *  core_ctl_set_limit_cpus - set min/max cpus of the cluster
 *  @cid: cluster id
 *  @min: min cpus
 *  @max: max cpus.
 *
 *  return 0 if success, else return errno
 */
int core_ctl_set_limit_cpus(unsigned int cid,
			     unsigned int min,
			     unsigned int max)
{
	struct cluster_data *cluster;
	unsigned long flags;

	if (cid >= num_clusters)
		return -EINVAL;

	if (max < min)
		return -EINVAL;

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	cluster = &cluster_state[cid];
	max = min(max, cluster->num_cpus);
	min = min(min, max);
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
	set_max_cpus(cluster, max, POWERHAL, 1);
	set_min_cpus(cluster, min, POWERHAL, 1);
	core_ctl_debug("%s: Try to adjust cluster %u limit cpus. min_cpus: %u, max_cpus: %u",
			TAG, cid, min, max);
	return 0;
}
EXPORT_SYMBOL(core_ctl_set_limit_cpus);

/*
 *  core_ctl_set_offline_throttle_ms - set throttle time of core-off judgement
 *  @cid: cluster id
 *  @throttle_ms: The unit of throttle time is microsecond
 *
 *  return 0 if success, else return errno
 */
int core_ctl_set_offline_throttle_ms(unsigned int cid,
				     unsigned int throttle_ms)
{
	struct cluster_data *cluster;

	if (cid >= num_clusters)
		return -EINVAL;

	cluster = &cluster_state[cid];
	set_offline_throttle_ms(cluster, throttle_ms);
	return 0;
}
EXPORT_SYMBOL(core_ctl_set_offline_throttle_ms);

/*
 *  core_ctl_set_boost
 *  @return: 0 if success, else return errno
 *
 *  When boost is enbled, all cluster of CPUs will be core-on.
 */
int core_ctl_set_boost(bool boost)
{
	int ret = 0;
	unsigned int index = 0;
	unsigned long flags;
	struct cluster_data *cluster;
	bool changed = false;

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	for_each_cluster(cluster, index) {
		if (boost) {
			changed = !cluster->boost;
			cluster->boost = 1;
		} else {
			if (cluster->boost) {
				cluster->boost = 0;
				changed = !cluster->boost;
			} else {
				/* FIXME: change to continue ? */
				ret = -EINVAL;
				break;
			}
		}
	}
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);

	if (changed) {
		index = 0;
		for_each_cluster(cluster, index)
			apply_demand(cluster);
	}

	core_ctl_debug("%s: boost=%d ret=%d ", TAG, boost, ret);
	return ret;
}
EXPORT_SYMBOL(core_ctl_set_boost);

/*
 *  core_ctl_consider_VIP
 *  @return: 0 if success, else return errno
 *
 *  When consider_VIP_task is enabled, busy_cpus algorithm consider VIP task
 */
int core_ctl_consider_VIP(unsigned int enable)
{
	int ret = 0;
	unsigned long flags;

	if (enable == 0 || enable == 1) {
		spin_lock_irqsave(&core_ctl_state_lock, flags);
		consider_VIP_task = enable;
		spin_unlock_irqrestore(&core_ctl_state_lock, flags);
	} else
		ret = -EINVAL;
	core_ctl_debug("%s: consider=%d ret=%d ", TAG, consider_VIP_task, ret);
	return ret;
}
EXPORT_SYMBOL(core_ctl_consider_VIP);

#define	MAX_CPU_MASK	((1 << nr_cpu_ids) - 1)
/*
 *  core_ctl_set_not_preferred - set not_prefer for the specific cpu number
 *  @not_preferred_cpus: Stand for cpu bitmap, 1 if set, 0 if unset.
 *
 *  return 0 if success, else return errno
 */
int core_ctl_set_not_preferred(unsigned int not_preferred_cpus)
{
	unsigned long flags;
	int i;
	bool bval;

	if (not_preferred_cpus > MAX_CPU_MASK)
		return -EINVAL;

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	for (i = 0; i < nr_cpu_ids; i++) {
		bval = !!(not_preferred_cpus & (1 << i));
		set_not_preferred_locked(i, bval);
	}
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
	return 0;
}
EXPORT_SYMBOL_GPL(core_ctl_set_not_preferred);

/*
 *  core_ctl_set_up_thres - adjuset up threshold value
 *  @cid: cluster id
 *  @val: Percentage of big core capactity. (0 - 100)
 *
 *  return 0 if success, else return errno
 */
int core_ctl_set_up_thres(int cid, unsigned int val)
{
	struct cluster_data *cluster;

	if (cid >= num_clusters)
		return -EINVAL;

	/* Range of up thrash should be 0 - 100 */
	if (val > MAX_BTASK_THRESH)
		val = MAX_BTASK_THRESH;

	cluster = &cluster_state[cid];
	return set_up_thres(cluster, val);
}
EXPORT_SYMBOL(core_ctl_set_up_thres);

/*
 *  core_ctl_force_pause_request - force pause or resume cpu by request caller
 *  @cpu: cpu number
 *  @is_pause: set true if pause, set false if resume.
 *  @request_mask: caller of this pause request
 *
 *  return 0 if success, else return errno
 */
static void core_ctl_call_notifier(unsigned int cpu, unsigned int is_pause);
int core_ctl_force_pause_request(unsigned int cpu, bool is_pause,
				 unsigned int request_mask)
{
	int ret = 0;
	unsigned long flags;
	unsigned int test_req_mask;
	struct cpu_data *cpu_stat;
	struct cluster_data *cluster;
	unsigned int force_paused_req;

	if (cpu > nr_cpu_ids)
		return -EINVAL;

	if (!cpu_online(cpu))
		return -EBUSY;

	if (!request_mask)
		return -EINVAL;

	if (request_mask & (request_mask - 1))
		return -EINVAL;

	if (request_mask >= MAX_FORCE_PAUSE_TYPE) {
		pr_info("%s:invalid force pause request %u", TAG, request_mask);
		return -EINVAL;
	}

	/* Avoid hotplug change online mask */
	cpu_hotplug_disable();
	if (is_pause && !test_disable_cpu(cpu)) {
		cpu_hotplug_enable();
		pr_info("%s: force pause failed, retain cpus should >= 2", TAG);
		return -EBUSY;
	}

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	cpu_stat = &per_cpu(cpu_state, cpu);
	cluster = cpu_stat->cluster;
	force_paused_req = cpu_stat->force_pause_req;
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);

	if (is_pause) {
		test_req_mask = force_paused_req | request_mask;
		ret = core_ctl_pause_cpu(cpu);
	} else {
		test_req_mask = force_paused_req & (~request_mask);
		if (test_req_mask == 0)
			ret = core_ctl_resume_cpu(cpu);
		else {
			cpu_stat->force_pause_req = test_req_mask;
			cpu_hotplug_enable();
			pr_info("[Core Force Pause] others still need paused CPU#%d, paused=0x%lx, online=0x%lx, act=0x%lx, req[%d]=%u\n",
				cpu, cpu_pause_mask->bits[0], cpu_online_mask->bits[0],
				cpu_active_mask->bits[0], cpu, cpu_stat->force_pause_req);
			return ret;
		}
	}

	/* pause/resume fail */
	if (ret < 0) {
		cpu_hotplug_enable();
		if (is_pause) {
			pr_info("[Core Force Pause] Pause request fail ret=%d, cpu=%d, paused=0x%lx, online=0x%lx, act=0x%lx, req[%d]=%u\n",
				ret, cpu, cpu_pause_mask->bits[0], cpu_online_mask->bits[0],
				cpu_active_mask->bits[0], cpu, cpu_stat->force_pause_req);
		} else {
			pr_info("[Core Force Pause] Resume request fail ret=%d, cpu=%d, paused=0x%lx, online=0x%lx, act=0x%lx, req[%d]=%u\n",
				ret, cpu, cpu_pause_mask->bits[0], cpu_online_mask->bits[0],
				cpu_active_mask->bits[0], cpu, cpu_stat->force_pause_req);
		}
		return ret;
	}

	/* success or already pause/resume, update cpu state */
	spin_lock_irqsave(&core_ctl_state_lock, flags);
	cpu_stat->force_paused = is_pause;

	if (is_pause) {
		/* Handle conflict with original policy */
		if (cpu_stat->paused_by_cc) {
			cpu_stat->paused_by_cc = false;
			cluster->nr_paused_cpus--;
		}
		cpumask_set_cpu(cpu, &cpu_force_pause_mask);
	} else
		cpumask_clear_cpu(cpu, &cpu_force_pause_mask);

	cluster->active_cpus = get_active_cpu_count(cluster);
	cpu_stat->force_pause_req = test_req_mask;
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
	cpu_hotplug_enable();

	if (is_pause) {
		if (ret) {
			pr_info("[Core Force Pause] Already Pause: cpu=%d, paused=0x%lx, online=0x%lx, act=0x%lx, req[%d]=%u\n",
				cpu, cpu_pause_mask->bits[0], cpu_online_mask->bits[0],
				cpu_active_mask->bits[0], cpu, cpu_stat->force_pause_req);
		} else {
			pr_info("[Core Force Pause] Pause success: cpu=%d, paused=0x%lx, online=0x%lx, act=0x%lx, req[%d]=%u\n",
				cpu, cpu_pause_mask->bits[0], cpu_online_mask->bits[0],
				cpu_active_mask->bits[0], cpu, cpu_stat->force_pause_req);
		}
	} else {
		if (ret) {
			pr_info("[Core Force Pause] Already Resume: cpu=%d, paused=0x%lx, online=0x%lx, act=0x%lx, req[%d]=%u\n",
				cpu, cpu_pause_mask->bits[0], cpu_online_mask->bits[0],
				cpu_active_mask->bits[0], cpu, cpu_stat->force_pause_req);
		} else {
			pr_info("[Core Force Pause] Resume success: cpu=%d, paused=0x%lx, online=0x%lx, act=0x%lx, req[%d]=%u\n",
				cpu, cpu_pause_mask->bits[0], cpu_online_mask->bits[0],
				cpu_active_mask->bits[0], cpu, cpu_stat->force_pause_req);
		}
	}

	core_ctl_call_notifier(cpu, is_pause);
	core_ctl_cpu_request_trace();
	return ret;
}
EXPORT_SYMBOL(core_ctl_force_pause_request);

/*
 *  core_ctl_force_pause_cpu - force pause or resume cpu
 *  @cpu: cpu number
 *  @is_pause: set true if pause, set false if resume.
 *
 *  return 0 if success, else return errno
 */
int core_ctl_force_pause_cpu(unsigned int cpu, bool is_pause)
{
	int ret = 0;
	pr_info("We recommend use core_ctl_force_pause_request instead of this API.\
		This API will be removed in the future.\n");
	ret = core_ctl_force_pause_request(cpu, is_pause, UNKNOWN_FORCE_PAUSE);
	return ret;
}
EXPORT_SYMBOL(core_ctl_force_pause_cpu);

static ssize_t store_thermal_up_thres(struct cluster_data *state,
		const char *buf, size_t threshold)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	set_thermal_up_thres(state, val);
	return threshold;
}

/*
 *  core_ctl_set_cpu_busy_up_thres - set threshold of cpu busy state
 *  @cid: cluster id
 *  @pct: percentage of cpu loading(0-100).
 *
 *  return 0 if success, else return errno
 */
int core_ctl_set_cpu_busy_up_thres(unsigned int cid, unsigned int pct)
{
	struct cluster_data *cluster;

	if (pct > 100 || cid > 2)
		return -EINVAL;

	cluster = &cluster_state[cid];
	set_cpu_busy_up_thres(cluster, pct);

	return 0;
}
EXPORT_SYMBOL(core_ctl_set_cpu_busy_up_thres);

/*
 *  core_ctl_set_cpu_busy_down_thres - set threshold of cpu non-busy state
 *  @cid: cluster id
 *  @pct: percentage of cpu loading(0-100).
 *
 *  return 0 if success, else return errno
 */
int core_ctl_set_cpu_busy_down_thres(unsigned int cid, unsigned int pct)
{
	struct cluster_data *cluster;

	if (pct > 100 || cid > 2)
		return -EINVAL;

	cluster = &cluster_state[cid];
	set_cpu_busy_down_thres(cluster, pct);

	return 0;
}
EXPORT_SYMBOL(core_ctl_set_cpu_busy_down_thres);

/*
 *  core_ctl_set_cpu_nr_task_thres - set threshold of cpu busy state
 *  @cid: cluster id
 *  @nr task: number of cpu task
 *
 *  return 0 if success, else return errno
 */
int core_ctl_set_cpu_nr_task_thres(unsigned int cid, unsigned int nr_task)
{
	struct cluster_data *cluster;

	if (cid > 2)
		return -EINVAL;

	cluster = &cluster_state[cid];
	set_cpu_nr_task_thres(cluster, nr_task);

	return 0;
}
EXPORT_SYMBOL(core_ctl_set_cpu_nr_task_thres);

/*
 *  core_ctl_set_cpu_rt_nr_task_thres - set threshold of cpu busy state
 *  @cid: cluster id
 *  @rt_nr task: number of cpu rt task
 *
 *  return 0 if success, else return errno
 */
int core_ctl_set_cpu_rt_nr_task_thres(unsigned int cid, unsigned int rt_nr_task)
{
	struct cluster_data *cluster;

	if (cid > 2)
		return -EINVAL;

	cluster = &cluster_state[cid];
	set_cpu_rt_nr_task_thres(cluster, rt_nr_task);

	return 0;
}
EXPORT_SYMBOL(core_ctl_set_cpu_rt_nr_task_thres);

/*
 *  core_ctl_set_cpu_active_loading - set threshold of cpu busy state
 *  @cid: cluster id
 *  @loading: percentage of cpu loading(0-100).
 *
 *  return 0 if success, else return errno
 */
int core_ctl_set_cpu_active_loading_thres(unsigned int cid, unsigned int loading)
{
	struct cluster_data *cluster;

	if (loading > 100 || cid > 2)
		return -EINVAL;

	cluster = &cluster_state[cid];
	set_cpu_active_loading_thres(cluster, loading);

	return 0;
}
EXPORT_SYMBOL(core_ctl_set_cpu_active_loading_thres);

/*
 *  core_ctl_set_freq_min_thres - set threshold of frequency min
 *  @cid: cluster id
 *  @freq: expected frequency(KHz)
 *
 *  return 0 if success, else return errno
 */
int core_ctl_set_freq_min_thres(unsigned int cid, unsigned int freq)
{
	struct cluster_data *cluster;

	if (cid > 2)
		return -EINVAL;

	cluster = &cluster_state[cid];
	set_freq_min_thres(cluster, freq);
	return 0;
}
EXPORT_SYMBOL(core_ctl_set_freq_min_thres);

void core_ctl_notifier_register(struct notifier_block *n)
{
	atomic_notifier_chain_register(&core_ctl_notifier, n);
}
EXPORT_SYMBOL(core_ctl_notifier_register);

void core_ctl_notifier_unregister(struct notifier_block *n)
{
	atomic_notifier_chain_unregister(&core_ctl_notifier, n);
}
EXPORT_SYMBOL(core_ctl_notifier_unregister);

unsigned int core_ctl_get_policy(void)
{
	return enable_policy;
}
EXPORT_SYMBOL(core_ctl_get_policy);
/* ==================== sysctl node ======================== */

static ssize_t store_min_cpus(struct cluster_data *state,
		const char *buf, size_t count)
{
	unsigned int val, have_demand;

	if (sscanf(buf, "%u%u\n", &val, &have_demand) != 2)
		return -EINVAL;

	set_min_cpus(state, val, SYSNODE, have_demand);
	return count;
}

static ssize_t show_min_cpus(const struct cluster_data *state, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->min_cpus);
}

static ssize_t store_max_cpus(struct cluster_data *state,
		const char *buf, size_t count)
{
	unsigned int val, have_demand;

	if (sscanf(buf, "%u%u\n", &val, &have_demand) != 2)
		return -EINVAL;

	set_max_cpus(state, val, SYSNODE, have_demand);
	return count;
}

static ssize_t show_max_cpus(const struct cluster_data *state, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->max_cpus);
}

static ssize_t store_offline_throttle_ms(struct cluster_data *state,
		const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	set_offline_throttle_ms(state, val);
	return count;
}

static ssize_t show_offline_throttle_ms(const struct cluster_data *state,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%lld\n", state->offline_throttle_ms);
}

static ssize_t store_up_thres(struct cluster_data *state,
		const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	if (val > MAX_BTASK_THRESH)
		val = MAX_BTASK_THRESH;

	set_up_thres(state, val);
	return count;
}

static ssize_t show_up_thres(const struct cluster_data *state, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->up_thres);
}

static ssize_t store_not_preferred(struct cluster_data *state,
		const char *buf, size_t count)
{
	unsigned int i;
	unsigned int val[MAX_CPUS_PER_CLUSTER];
	unsigned long flags;
	int ret;

	ret = sscanf(buf, "%u %u %u %u %u %u\n",
			&val[0], &val[1], &val[2], &val[3],
			&val[4], &val[5]);
	if (ret != state->num_cpus)
		return -EINVAL;

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	for (i = 0; i < state->num_cpus; i++)
		set_not_preferred_locked(i + state->first_cpu, val[i]);
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);

	return count;
}

static ssize_t show_not_preferred(const struct cluster_data *state, char *buf)
{
	ssize_t count = 0;
	struct cpu_data *c;
	unsigned long flags;
	int i;

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	for (i = 0; i < state->num_cpus; i++) {
		c = &per_cpu(cpu_state, i + state->first_cpu);
		count += scnprintf(buf + count, PAGE_SIZE - count,
				"CPU#%d: %u\n", c->cpu, c->not_preferred);
	}
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);

	return count;
}

static ssize_t store_core_ctl_boost(struct cluster_data *state,
		const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	/* only allow first cluster */
	if (state->cluster_id != 0)
		return -EINVAL;

	if (val == 0 || val == 1)
		core_ctl_set_boost(val);
	else
		return -EINVAL;

	return count;
}

static ssize_t show_core_ctl_boost(const struct cluster_data *state, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->boost);
}

static ssize_t store_enable(struct cluster_data *state,
		const char *buf, size_t count)
{
	unsigned int val;
	bool bval;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	bval = !!val;
	if (bval != state->enable) {
		state->enable = bval;
		apply_demand(state);
	}

	return count;
}

static ssize_t show_enable(const struct cluster_data *state, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", state->enable);
}

static ssize_t store_cpu_busy_up_thres(struct cluster_data *state,
		const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	if (val > MAX_BTASK_THRESH)
		val = MAX_BTASK_THRESH;

	set_cpu_busy_up_thres(state, val);
	return count;
}

static ssize_t show_cpu_busy_up_thres(const struct cluster_data *state, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", state->cpu_busy_up_thres);
}

static ssize_t store_cpu_busy_down_thres(struct cluster_data *state,
		const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	if (val > MAX_BTASK_THRESH)
		val = MAX_BTASK_THRESH;

	set_cpu_busy_down_thres(state, val);
	return count;
}

static ssize_t show_cpu_busy_down_thres(const struct cluster_data *state, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", state->cpu_busy_down_thres);
}

static ssize_t store_cpu_nr_task_thres(struct cluster_data *state,
		const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	set_cpu_nr_task_thres(state, val);
	return count;
}

static ssize_t show_cpu_nr_task_thres(const struct cluster_data *state, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", state->nr_task_thres);
}

static ssize_t store_cpu_rt_nr_task_thres(struct cluster_data *state,
		const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	set_cpu_rt_nr_task_thres(state, val);
	return count;
}

static ssize_t show_cpu_rt_nr_task_thres(const struct cluster_data *state, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", state->rt_nr_task_thres);
}

static ssize_t store_cpu_active_loading_thres(struct cluster_data *state,
		const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	set_cpu_active_loading_thres(state, val);
	return count;
}

static ssize_t show_cpu_active_loading_thres(const struct cluster_data *state, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", state->active_loading_thres);
}

static ssize_t store_freq_min_thres(struct cluster_data *state,
		const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	set_freq_min_thres(state, val);
	return count;
}

static ssize_t show_freq_min_thres(const struct cluster_data *state, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%lu\n", state->freq_thres);
}

static ssize_t show_thermal_up_thres(const struct cluster_data *state, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", state->thermal_up_thres);
}

static ssize_t store_consider_VIP(struct cluster_data *state,
		const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	core_ctl_consider_VIP(val);
	return count;
}

static ssize_t show_consider_VIP(const struct cluster_data *state, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", consider_VIP_task);
}

static ssize_t show_global_state(const struct cluster_data *state, char *buf)
{
	struct cpu_data *c;
	struct cluster_data *cluster;
	ssize_t count = 0;
	unsigned int cpu;

	spin_lock_irq(&core_ctl_state_lock);
	for_each_possible_cpu(cpu) {
		c = &per_cpu(cpu_state, cpu);
		cluster = c->cluster;
		if (!cluster || !cluster->inited)
			continue;

		/* Only show this cluster */
		if (!cpumask_test_cpu(cpu, &state->cpu_mask))
			continue;

		if (cluster->first_cpu == cpu) {
			count += snprintf(buf + count, PAGE_SIZE - count,
					"Cluster%u\n", state->cluster_id);
			count += snprintf(buf + count, PAGE_SIZE - count,
					"\tFirst CPU: %u\n", cluster->first_cpu);
			count += snprintf(buf + count, PAGE_SIZE - count,
					"\tActive CPUs: %u\n",
					get_active_cpu_count(cluster));
			count += snprintf(buf + count, PAGE_SIZE - count,
					"\tNeed CPUs: %u\n", cluster->need_cpus);
			count += snprintf(buf + count, PAGE_SIZE - count,
					"\tNR Paused CPUs(pause by core_ctl): %u\n",
					cluster->nr_paused_cpus);
		}

		count += snprintf(buf + count, PAGE_SIZE - count,
				"CPU%u\n", cpu);
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tOnline: %u\n", cpu_online(c->cpu));
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tPaused: %u\n", cpu_paused(c->cpu));
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tPaused by core_ctl: %u\n", c->paused_by_cc);
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tPaused by forced: %u\n", (unsigned int)c->force_paused);
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tForced pause req: %u\n", c->force_pause_req);
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tCPU utils(%%): %u\n", c->cpu_util_pct[c->win_idx]);
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tIs busy: %u\n", c->is_busy);
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tNot preferred: %u\n", c->not_preferred);
	}
	spin_unlock_irq(&core_ctl_state_lock);

	return count;
}

struct core_ctl_attr {
	struct attribute attr;
	ssize_t (*show)(const struct cluster_data *state, char *buf);
	ssize_t (*store)(struct cluster_data *state, const char *buf, size_t count);
};

#define core_ctl_attr_ro(_name)         \
static struct core_ctl_attr _name =     \
__ATTR(_name, 0400, show_##_name, NULL)

#define core_ctl_attr_rw(_name)                 \
static struct core_ctl_attr _name =             \
__ATTR(_name, 0600, show_##_name, store_##_name)

core_ctl_attr_rw(min_cpus);
core_ctl_attr_rw(max_cpus);
core_ctl_attr_rw(offline_throttle_ms);
core_ctl_attr_rw(up_thres);
core_ctl_attr_rw(not_preferred);
core_ctl_attr_rw(core_ctl_boost);
core_ctl_attr_rw(enable);
core_ctl_attr_ro(global_state);
core_ctl_attr_rw(thermal_up_thres);
core_ctl_attr_rw(cpu_busy_up_thres);
core_ctl_attr_rw(cpu_busy_down_thres);
core_ctl_attr_rw(cpu_nr_task_thres);
core_ctl_attr_rw(cpu_rt_nr_task_thres);
core_ctl_attr_rw(cpu_active_loading_thres);
core_ctl_attr_rw(freq_min_thres);
core_ctl_attr_rw(consider_VIP);

static struct attribute *default_attrs[] = {
	&min_cpus.attr,
	&max_cpus.attr,
	&offline_throttle_ms.attr,
	&up_thres.attr,
	&not_preferred.attr,
	&core_ctl_boost.attr,
	&enable.attr,
	&global_state.attr,
	&thermal_up_thres.attr,
	&cpu_busy_up_thres.attr,
	&cpu_busy_down_thres.attr,
	&cpu_nr_task_thres.attr,
	&cpu_rt_nr_task_thres.attr,
	&cpu_active_loading_thres.attr,
	&freq_min_thres.attr,
	&consider_VIP.attr,
	NULL
};
ATTRIBUTE_GROUPS(default);

#define to_cluster_data(k) container_of(k, struct cluster_data, kobj)
#define to_attr(a) container_of(a, struct core_ctl_attr, attr)
static ssize_t show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	struct cluster_data *data = to_cluster_data(kobj);
	struct core_ctl_attr *cattr = to_attr(attr);
	ssize_t ret = -EIO;

	if (cattr->show)
		ret = cattr->show(data, buf);

	return ret;
}

static ssize_t store(struct kobject *kobj, struct attribute *attr,
		const char *buf, size_t count)
{
	struct cluster_data *data = to_cluster_data(kobj);
	struct core_ctl_attr *cattr = to_attr(attr);
	ssize_t ret = -EIO;

	if (cattr->store)
		ret = cattr->store(data, buf, count);

	return ret;
}

static const struct sysfs_ops sysfs_ops = {
	.show   = show,
	.store  = store,
};

static struct kobj_type ktype_core_ctl = {
	.sysfs_ops      = &sysfs_ops,
	.default_groups  = default_groups,
};

/* ==================== algorithm of core control ======================== */

/*
 * Get number of the busy CPU cores
 */
static void get_busy_cpus(void)
{
	unsigned long flags;
	struct cluster_data *cluster;
	struct cpu_data *cpu_stat;
	int cpu = 0, cid = 0, cpu_count = 0, idx = 0;
	unsigned int busy_state[MAX_NR_CPUS] = {0};
	unsigned int max_nr_state[MAX_NR_CPUS] = {0};
	unsigned int max_rt_nr_state[MAX_NR_CPUS] = {0};
	unsigned int max_vip_nr_state[MAX_NR_CPUS] = {0};
	unsigned int over_nr_task = 0, over_act_load = 0, over_rt_vip_nr_task = 0;

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	/* Define need spread cpus */
	for (cid = 0; cid < num_clusters; cid++) {
		cpu_count = 0;
		cluster = &cluster_state[cid];

		if (!cluster || !cluster->inited)
			continue;

		for_each_cpu(cpu, &cluster->cpu_mask) {
			if (cpu >= MAX_NR_CPUS)
				continue;

			cpu_stat = &per_cpu(cpu_state, cpu);
			idx = cpu_stat->win_idx;

			/* check CPU is busy or not */
			if (cpu_stat->cpu_util_pct[idx] > cluster->cpu_busy_up_thres)
				cpu_stat->is_busy = true;
			else if (cpu_stat->cpu_util_pct[idx] < cluster->cpu_busy_down_thres)
				cpu_stat->is_busy = false;
			/* else remain previous status */

			cpu_stat->max_nr_state = get_max_nr_running(cpu);
			cpu_stat->max_rt_nr_state = get_max_rt_nr_running(cpu);
			cpu_stat->max_vip_nr_state = get_max_vip_nr_running(cpu);

			busy_state[cpu] = (unsigned int)cpu_stat->is_busy;
			max_nr_state[cpu] = cpu_stat->max_nr_state;
			max_rt_nr_state[cpu] = cpu_stat->max_rt_nr_state;
			max_vip_nr_state[cpu] = cpu_stat->max_vip_nr_state;

			over_nr_task = max_nr_state[cpu] > cluster->nr_task_thres;
			over_act_load = cpu_stat->cpu_active_loading[idx] > cluster->active_loading_thres;
			if (consider_VIP_task) {
				over_rt_vip_nr_task = max_rt_nr_state[cpu] + max_vip_nr_state[cpu]
					> cluster->rt_nr_task_thres;
			} else
				over_rt_vip_nr_task = max_rt_nr_state[cpu] > cluster->rt_nr_task_thres;

			if (busy_state[cpu] && over_nr_task) {
				cluster->deiso_reason |= DEISO_BUSY_NR_TASK;
				cpu_count++;
			} else if (enable_policy != CAMERA_MODE) {
				if (busy_state[cpu] && over_act_load) {
					cluster->deiso_reason |= DEISO_BUSY_ACT_LOAD;
					cpu_count++;
				} else if (over_rt_vip_nr_task) {
					cluster->deiso_reason |= DEISO_BUSY_RT_NR_TASK;
					cpu_count++;
				}
			}
		}
		cluster->need_spread_cpus = cpu_count;
	}
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);

	trace_core_ctl_busy_cpus(busy_state, max_nr_state, max_rt_nr_state, max_vip_nr_state, consider_VIP_task);
}

#define BIG_TASK_AVG_THRESHOLD 25
/*
 * Get number of big tasks
 */
static void get_nr_running_big_task(void)
{
	unsigned long flags;
	int avg_down[MAX_CLUSTERS] = {0};
	int avg_up[MAX_CLUSTERS] = {0};
	int nr_up[MAX_CLUSTERS] = {0};
	int nr_down[MAX_CLUSTERS] = {0};
	int cluster_nr_up[MAX_CLUSTERS] = {0};
	int cluster_nr_down[MAX_CLUSTERS] = {0};
	int i, delta;

	for (i = 0; i < num_clusters; i++) {
		sched_get_nr_over_thres_avg(i,
					  &avg_down[i],
					  &avg_up[i],
					  &nr_down[i],
					  &nr_up[i]);
	}

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	for (i = 0; i < num_clusters; i++) {
		/* reset nr_up and nr_down */
		cluster_state[i].nr_up = 0;
		cluster_state[i].nr_down = 0;

		if (nr_up[i]) {
			if (avg_up[i]/nr_up[i] > BIG_TASK_AVG_THRESHOLD)
				cluster_state[i].nr_up = nr_up[i];
			else /* min(avg_up[i]/BIG_TASK_AVG_THRESHOLD,nr_up[i]) */
				cluster_state[i].nr_up =
					avg_up[i]/BIG_TASK_AVG_THRESHOLD > nr_up[i] ?
					nr_up[i] : avg_up[i]/BIG_TASK_AVG_THRESHOLD;
		}
		/*
		 * The nr_up is part of nr_down, so
		 * the real nr_down is nr_down minus nr_up.
		 */
		delta = nr_down[i] - nr_up[i];
		if (nr_down[i] && delta > 0) {
			if (((avg_down[i]-avg_up[i]) / delta)
					> BIG_TASK_AVG_THRESHOLD)
				cluster_state[i].nr_down = delta;
			else
				cluster_state[i].nr_down =
					(avg_down[i]-avg_up[i])/
					delta < BIG_TASK_AVG_THRESHOLD ?
					delta : (avg_down[i]-avg_up[i])/
					BIG_TASK_AVG_THRESHOLD;
		}
		/* nr can't be negative */
		cluster_state[i].nr_up =
			cluster_state[i].nr_up < 0 ? 0 : cluster_state[i].nr_up;
		cluster_state[i].nr_down =
			cluster_state[i].nr_down < 0 ? 0 : cluster_state[i].nr_down;
	}

	for (i = 0; i < num_clusters; i++) {
		cluster_nr_up[i] = cluster_state[i].nr_up;
		cluster_nr_down[i] = cluster_state[i].nr_down;
	}

	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
	trace_core_ctl_nr_over_thres(cluster_nr_up, cluster_nr_down, nr_up, nr_down, avg_up, avg_down);
}

/*
 * prev_cluster_nr_assist:
 *   Tasks that are eligible to run on the previous
 *   cluster but cannot run because of insufficient
 *   CPUs there. It's indicative of number of CPUs
 *   in this cluster that should assist its
 *   previous cluster to makeup for insufficient
 *   CPUs there.
 */
static inline int get_prev_cluster_nr_assist(int index)
{
	struct cluster_data *prev_cluster;

	if (index == 0)
		return 0;

	index--;
	prev_cluster = &cluster_state[index];
	return prev_cluster->nr_assist;

}

#define CORE_CTL_PERIODIC_TRACK_MS	4
static inline bool window_check(void)
{
	unsigned long flags;
	ktime_t now = ktime_get();
	static ktime_t tracking_last_update;
	bool do_check = false;

	spin_lock_irqsave(&core_ctl_window_check_lock, flags);
	if (ktime_after(now, ktime_add_ms(
		tracking_last_update, CORE_CTL_PERIODIC_TRACK_MS))) {
		do_check = true;
		tracking_last_update = now;
	}
	spin_unlock_irqrestore(&core_ctl_window_check_lock, flags);
	return do_check;
}

enum {
	NO_NEED_RESCUE	= 0,
	BUSY_NR	= 1,
	BUSY_RT_NR	= 2,
	HEAVY_NORMAL_TASK	= 4
};

static void check_heaviest_status(void)
{
	unsigned long flags;
	struct cluster_data *big_cluster;
	struct cluster_data *mid_cluster;
	unsigned int max_capacity, big_cpu_ts=0;
	unsigned long heaviest_thres;
	static unsigned int max_task_util;
	int cpu, rescue_big_core = NO_NEED_RESCUE;
	struct cpu_data *cpu_stat;

	if (num_clusters <= 2)
		return;

	sched_max_util_task(&max_task_util);
	big_cluster = &cluster_state[num_clusters - 1];
	mid_cluster = &cluster_state[big_cluster->cluster_id - 1];
#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
	big_cpu_ts = get_cpu_temp(big_cluster->first_cpu);
#elif IS_ENABLED(CONFIG_MTK_PLAT_POWER_6893)
	big_cpu_ts = get_immediate_tslvts1_1_wrap();
#endif

	heaviest_thres = mid_cluster->up_thres;
	if (big_cpu_ts > big_cluster->thermal_degree_thres)
		heaviest_thres = mid_cluster->thermal_up_thres;

	/*
	 * Check for biggest task in system,
	 * if it's over threshold, force to enable
	 * prime core.
	 */
	max_capacity = get_max_capacity(mid_cluster->cluster_id);
	heaviest_thres = div64_u64(heaviest_thres * max_capacity, 100);

	/* rescue prime core when busy */
	if (enable_policy != CAMERA_MODE) {
		spin_lock_irqsave(&core_ctl_state_lock, flags);
		for_each_cpu(cpu, &big_cluster->cpu_mask) {
			cpu_stat = &per_cpu(cpu_state, cpu);

			if (cpu_stat->is_busy && cpu_stat->max_nr_state > 1) {
				mid_cluster->new_need_cpus += (cpu_stat->max_nr_state - 1);
				rescue_big_core |= BUSY_NR;
			}
			if (cpu_stat->max_rt_nr_state > big_cluster->rt_nr_task_thres) {
				mid_cluster->new_need_cpus += cpu_stat->max_rt_nr_state;
				rescue_big_core |= BUSY_RT_NR;
			}
		}

		if (big_cluster->nr_down + big_cluster->nr_up > 1) {
			mid_cluster->new_need_cpus += (big_cluster->nr_down + big_cluster->nr_up);
			rescue_big_core |= HEAVY_NORMAL_TASK;
		}

		spin_unlock_irqrestore(&core_ctl_state_lock, flags);
	}

	if (big_cluster->new_need_cpus)
		goto print_log;

	/* If max util is over threshold */
	if (max_task_util > heaviest_thres) {
		spin_lock_irqsave(&core_ctl_state_lock, flags);
		big_cluster->new_need_cpus++;
		if (mid_cluster->new_need_cpus > 0)
			mid_cluster->new_need_cpus--;

		spin_unlock_irqrestore(&core_ctl_state_lock, flags);
	}

print_log:
	trace_core_ctl_heaviest_util(big_cpu_ts, heaviest_thres, max_task_util, rescue_big_core);
}

static inline void core_ctl_main_algo(void)
{
	unsigned long flags;
	unsigned int index = 0;
	struct cluster_data *cluster;
	unsigned int orig_need_cpu[MAX_CLUSTERS] = {0};
	struct cpumask active_cpus;
	unsigned int nr_assist_cpu[MAX_CLUSTERS] = {0};
	unsigned int need_spread_cpu[MAX_CLUSTERS] = {0};
	unsigned int boost_by_freq[MAX_CLUSTERS] = {0};
	unsigned int boost_by_wlan[MAX_CLUSTERS] = {0};
	unsigned int deiso_reason[MAX_CLUSTERS] = {0};

	for_each_cluster(cluster, index)
		cluster->deiso_reason = DEISO_NONE;

	/* get each cpu loading */
	get_cpu_active_loading();
	/* get needed spread cpus */
	get_busy_cpus();
	/* get TLP of over threshold tasks */
	get_nr_running_big_task();

	index = 0;
	spin_lock_irqsave(&core_ctl_state_lock, flags);
	/* Apply TLP of tasks */
	for_each_cluster(cluster, index) {
		int temp_need_cpus = 0;

		temp_need_cpus += cluster->nr_up;
		temp_need_cpus += cluster->nr_down;
		temp_need_cpus += cluster->need_spread_cpus;
		temp_need_cpus += get_prev_cluster_nr_assist(index);

		if (cluster->nr_up)
			cluster->deiso_reason |= DEISO_HEAVY_TASK;

		if (cluster->nr_down)
			cluster->deiso_reason |= DEISO_NORMAL_TASK;

		if (index == L_CLUSTER_ID)
			cluster->nr_assist = cluster->nr_up + cluster->need_spread_cpus;
		else {
			/* nr_assist(i) = max(0, need_cpus(i) - max_cpus(i)) */
			cluster->nr_assist =
				(temp_need_cpus > cluster->max_cpus ?
				(temp_need_cpus - cluster->max_cpus) : 0);
		}
		cluster->new_need_cpus = temp_need_cpus;
	}
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);

	/*
	 * Ensure prime cpu make core-on
	 * if heaviest task is over threshold.
	 */
	check_heaviest_status();

	check_freq_min_req();

	check_wlan_request();

	index = 0;
	spin_lock_irqsave(&core_ctl_state_lock, flags);
	for_each_cluster(cluster, index) {
		orig_need_cpu[index] = cluster->new_need_cpus;
		nr_assist_cpu[index] = cluster->nr_assist;
		need_spread_cpu[index] = cluster->need_spread_cpus;
		boost_by_freq[index] = cluster->boost_by_freq;
		boost_by_wlan[index] = cluster->boost_by_wlan;
		deiso_reason[index] = cluster->deiso_reason;
	}
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
	cpumask_andnot(&active_cpus, cpu_online_mask, cpu_pause_mask);

	trace_core_ctl_algo_info(enable_policy, need_spread_cpu, nr_assist_cpu, orig_need_cpu,
				cpumask_bits(&active_cpus)[0], boost_by_freq, boost_by_wlan, deiso_reason);
}

unsigned long (*calc_eff_hook)(unsigned int first_cpu, int opp, unsigned int temp,
       unsigned long dyn_power, unsigned int cap);
EXPORT_SYMBOL(calc_eff_hook);
static int need_update_ppm_eff = 1;
static int update_ppm_eff(void);

void core_ctl_tick(void *data, struct rq *rq)
{
	unsigned int index = 0;
	struct cluster_data *cluster;

	irq_log_store();

	/* prevent irq disable on cpu 0 */
	if (rq->cpu == 0)
		return;

	if (!window_check())
		return;

	if (need_update_ppm_eff != 0 && calc_eff_hook) {
		/* Prevent other threads */
		need_update_ppm_eff = 0;
		need_update_ppm_eff = update_ppm_eff();
	}

	irq_log_store();

	if (enable_policy)
		core_ctl_main_algo();

	irq_log_store();

	for_each_cluster(cluster, index)
		apply_demand(cluster);
	irq_log_store();
}

inline void core_ctl_update_active_cpu(unsigned int cpu)
{
	unsigned long flags;
	struct cpu_data *c;
	struct cluster_data *cluster;

	if (cpu > nr_cpu_ids)
		return;

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	c = &per_cpu(cpu_state, cpu);
	cluster = c->cluster;
	cluster->active_cpus = get_active_cpu_count(cluster);
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
}
EXPORT_SYMBOL(core_ctl_update_active_cpu);

static struct cpumask try_to_pause(struct cluster_data *cluster, int need)
{
	unsigned long flags;
	unsigned int num_cpus = cluster->num_cpus;
	unsigned int nr_paused = 0;
	int cpu;
	bool success;
	bool check_not_prefer = cluster->nr_not_preferred_cpus;
	bool check_busy = true;
	int ret = 0;
	struct cpumask cpu_pause_res;

	cpumask_clear(&cpu_pause_res);

again:
	nr_paused = 0;
	spin_lock_irqsave(&core_ctl_state_lock, flags);
	for (cpu = nr_cpu_ids-1; cpu >= 0; cpu--) {
		struct cpu_data *c;

		success = false;
		if (!cpumask_test_cpu(cpu, &cluster->cpu_mask))
			continue;

		if (!num_cpus--)
			break;

		c = &per_cpu(cpu_state, cpu);
		if (!is_active(c))
			continue;

		if (!test_disable_cpu(cpu))
			continue;

		if (check_busy && c->is_busy)
			continue;

		if (c->force_paused)
			continue;

		if (cluster->active_cpus == need)
			break;

		/*
		 * Pause only the not_preferred CPUs.
		 * If none of the CPUs are selected as not_preferred,
		 * then all CPUs are eligible for isolation.
		 */
		if (check_not_prefer && !c->not_preferred)
			continue;

		spin_unlock_irqrestore(&core_ctl_state_lock, flags);
		core_ctl_debug("%s: Trying to pause CPU%u\n", TAG, c->cpu);
		ret = core_ctl_pause_cpu(cpu);
		if (ret < 0){
			core_ctl_debug("%s Unable to pause CPU%u err=%d\n", TAG, c->cpu, ret);
		} else if (!ret) {
			success = true;
			cpumask_set_cpu(c->cpu, &cpu_pause_res);
			core_ctl_call_notifier(cpu, 1);
			nr_paused++;
		} else {
			cpumask_set_cpu(c->cpu, &cpu_pause_res);
			core_ctl_debug("%s Unable to pause CPU%u already paused\n", TAG, c->cpu);
		}
		spin_lock_irqsave(&core_ctl_state_lock, flags);
		if (success) {
			/* check again, prevent a seldom racing issue */
			if (cpu_online(c->cpu))
				c->paused_by_cc = true;
			else {
				nr_paused--;
				pr_info("%s: Pause failed because cpu#%d is offline. ",
					TAG, c->cpu);
			}
		}
		cluster->active_cpus = get_active_cpu_count(cluster);
	}
	cluster->nr_paused_cpus += nr_paused;
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
	/*
	 * If the number of not prefer CPUs is not
	 * equal to need CPUs, then check it again.
	 */
	if (check_busy || (check_not_prefer &&
		cluster->active_cpus != need)) {
		num_cpus = cluster->num_cpus;
		check_not_prefer = false;
		check_busy = false;
		goto again;
	}
	return cpu_pause_res;
}

static struct cpumask try_to_resume(struct cluster_data *cluster, int need)
{
	unsigned long flags;
	unsigned int num_cpus = cluster->num_cpus, cpu;
	unsigned int nr_resumed = 0;
	bool check_not_prefer = cluster->nr_not_preferred_cpus;
	bool success;
	int ret = 0;
	struct cpumask cpu_resume_res;

	cpumask_clear(&cpu_resume_res);

again:
	nr_resumed = 0;
	spin_lock_irqsave(&core_ctl_state_lock, flags);
	for_each_cpu(cpu, &cluster->cpu_mask) {
		struct cpu_data *c;

		success = false;
		if (!num_cpus--)
			break;

		c = &per_cpu(cpu_state, cpu);

		if (!c->paused_by_cc)
			continue;

		if (c->force_paused)
			continue;

		if (!cpu_online(c->cpu))
			continue;

		if (!cpu_paused(c->cpu))
			continue;

		if (cluster->active_cpus == need)
			break;

		/* The Normal CPUs are resumed prior to not prefer CPUs */
		if (!check_not_prefer && c->not_preferred)
			continue;

		spin_unlock_irqrestore(&core_ctl_state_lock, flags);

		core_ctl_debug("%s: Trying to resume CPU%u\n", TAG, c->cpu);
		ret = core_ctl_resume_cpu(cpu);
		if (ret < 0){
			core_ctl_debug("%s Unable to resume CPU%u err=%d\n", TAG, c->cpu, ret);
		} else if (!ret) {
			success = true;
			cpumask_set_cpu(c->cpu, &cpu_resume_res);
			core_ctl_call_notifier(cpu, 0);
			nr_resumed++;
		} else {
			cpumask_set_cpu(c->cpu, &cpu_resume_res);
			core_ctl_debug("%s: Unable to resume CPU%u already resumed\n", TAG, c->cpu);
		}
		spin_lock_irqsave(&core_ctl_state_lock, flags);
		if (success)
			c->paused_by_cc = false;
		cluster->active_cpus = get_active_cpu_count(cluster);
	}
	cluster->nr_paused_cpus -= nr_resumed;
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
	/*
	 * After un-isolated the number of prefer CPUs
	 * is not enough for need CPUs, then check
	 * not_prefer CPUs again.
	 */
	if (check_not_prefer &&
		cluster->active_cpus != need) {
		num_cpus = cluster->num_cpus;
		check_not_prefer = false;
		goto again;
	}
	return cpu_resume_res;
}

static void __ref do_core_ctl(struct cluster_data *cluster)
{
	unsigned int need;
	struct cpumask cpu_pause_res;
	struct cpumask cpu_resume_res;
	bool pause_resume = 0;

	cpumask_clear(&cpu_pause_res);
	cpumask_clear(&cpu_resume_res);
	need = apply_limits(cluster, cluster->need_cpus);

	if (adjustment_possible(cluster, need)) {
		core_ctl_debug("%s: Trying to adjust cluster %u from %u to %u\n",
				TAG, cluster->cluster_id, cluster->active_cpus, need);

		/* Avoid hotplug change online mask */
		cpu_hotplug_disable();
		if (cluster->active_cpus > need){
			cpu_pause_res = try_to_pause(cluster, need);
			pause_resume = 0;
		} else if (cluster->active_cpus < need){
			cpu_resume_res = try_to_resume(cluster, need);
			pause_resume = 1;
		}

		if (!pause_resume){
			if (!cpumask_empty(&cpu_pause_res)){
				core_ctl_debug("[Core Pause] pause_res=0x%lx, online=0x%lx, act=0x%lx, paused=0x%lx\n",
					cpu_pause_res.bits[0], cpu_online_mask->bits[0],
					cpu_active_mask->bits[0], cpu_pause_mask->bits[0]);
			}
		} else {
			if (!cpumask_empty(&cpu_resume_res)){
				core_ctl_debug("[Core Pause] resume_res=0x%lx, online=0x%lx, act=0x%lx, paused=0x%lx\n",
					cpu_resume_res.bits[0], cpu_online_mask->bits[0],
					cpu_active_mask->bits[0], cpu_pause_mask->bits[0]);
			}
		}
		cpu_hotplug_enable();
	} else
		core_ctl_debug("%s: failed to adjust cluster %u from %u to %u. (min = %u, max = %u)\n",
				TAG, cluster->cluster_id, cluster->active_cpus, need,
				cluster->min_cpus, cluster->max_cpus);
}

static int __ref try_core_ctl(void *data)
{
	struct cluster_data *cluster = data;
	unsigned long flags;

	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);
		spin_lock_irqsave(&cluster->pending_lock, flags);
		if (!cluster->pending) {
			spin_unlock_irqrestore(&cluster->pending_lock, flags);
			schedule();
			if (kthread_should_stop())
				break;
			spin_lock_irqsave(&cluster->pending_lock, flags);
		}
		set_current_state(TASK_RUNNING);
		cluster->pending = false;
		spin_unlock_irqrestore(&cluster->pending_lock, flags);

		do_core_ctl(cluster);
	}

	return 0;
}

static int cpu_pause_cpuhp_state(unsigned int cpu,  bool online)
{
	struct cpu_data *state = &per_cpu(cpu_state, cpu);
	struct cluster_data *cluster = state->cluster;
	unsigned int need;
	bool do_wakeup = false, resume = false;
	unsigned long flags;
	struct cpumask cpu_resume_req;
	unsigned int pause_cpus;
	unsigned int i;

	if (unlikely(!cluster || !cluster->inited))
		return 0;

	spin_lock_irqsave(&core_ctl_state_lock, flags);

	if (online)
		cluster->active_cpus = get_active_cpu_count(cluster);
	else {
		pause_cpus = cpumask_weight(cpu_pause_mask);
		if (pause_cpus > 0){
			if (!test_disable_cpu(cpu)) {
				spin_unlock_irqrestore(&core_ctl_state_lock, flags);
				return -EINVAL;
			}
		}

		/*
		* When CPU is offline, CPU should be un-isolated.
		* Thus, un-isolate this CPU that is going down if
		* it was isolated by core_ctl.
		*/
		if (state->paused_by_cc) {
			state->paused_by_cc = false;
			cluster->nr_paused_cpus--;
			resume = true;
		}

		for (i=0; i<WIN_SIZE; i++) {
			state->cpu_util_pct[i] = 0;
			state->cpu_active_loading[i] = 0;
		}
		state->force_paused = false;
		cluster->active_cpus = get_active_cpu_count(cluster);
	}

	need = apply_limits(cluster, cluster->need_cpus);
	do_wakeup = adjustment_possible(cluster, need);
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);

	/* cpu is inactive */
	if (resume) {
		cpumask_clear(&cpu_resume_req);
		cpumask_set_cpu(cpu, &cpu_resume_req);
		resume_cpus(&cpu_resume_req);
	}
	if (do_wakeup)
		wake_up_core_ctl_thread(cluster);
	return 0;
}

static int core_ctl_prepare_online_cpu(unsigned int cpu)
{
	return cpu_pause_cpuhp_state(cpu, true);
}

static int core_ctl_prepare_dead_cpu(unsigned int cpu)
{
	return cpu_pause_cpuhp_state(cpu, false);
}

static bool test_disable_cpu(unsigned int cpu)
{
	struct cpumask disable_mask;
	unsigned int disable_cpus;

	cpumask_clear(&disable_mask);
	cpumask_complement(&disable_mask, cpu_online_mask);
	cpumask_or(&disable_mask, &disable_mask, cpu_pause_mask);
	cpumask_set_cpu(cpu, &disable_mask);
	cpumask_and(&disable_mask, &disable_mask, cpu_possible_mask);
	disable_cpus = cpumask_weight(&disable_mask);
	if (disable_cpus > (nr_cpu_ids-2))
		return false;

	return true;
}

static struct cluster_data *find_cluster_by_first_cpu(unsigned int first_cpu)
{
	unsigned int i;

	for (i = 0; i < num_clusters; ++i) {
		if (cluster_state[i].first_cpu == first_cpu)
			return &cluster_state[i];
	}

	return NULL;
}

static void core_ctl_call_notifier(unsigned int cpu, unsigned int is_pause)
{
	struct core_ctl_notif_data ndata = {0};
	struct notifier_block *nb;

	nb = rcu_dereference_raw(core_ctl_notifier.head);
	if(!nb)
		return;

	ndata.cpu = cpu;
	ndata.is_pause = is_pause;
	ndata.paused_mask = cpu_pause_mask->bits[0];
	ndata.online_mask = cpu_online_mask->bits[0];
	atomic_notifier_call_chain(&core_ctl_notifier, is_pause, &ndata);

	trace_core_ctl_call_notifier(cpu, is_pause, cpumask_bits(cpu_online_mask)[0], cpumask_bits(cpu_pause_mask)[0]);
}

/* ==================== init section ======================== */

static int ppm_data_init(struct cluster_data *cluster);
static int cluster_init(const struct cpumask *mask)
{
	struct device *dev;
	unsigned int first_cpu = cpumask_first(mask);
	struct cluster_data *cluster;
	struct cpu_data *state;
	unsigned int cpu;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO-1 };
	int ret = 0, i;

	/* first_cpu is defined */
	if (find_cluster_by_first_cpu(first_cpu))
		return ret;

	dev = get_cpu_device(first_cpu);
	if (!dev)
		return -ENODEV;


	core_ctl_debug("%s: Creating CPU group %d\n", TAG, first_cpu);

	if (num_clusters == MAX_CLUSTERS) {
		pr_info("%s: Unsupported number of clusters. Only %u supported\n",
				TAG, MAX_CLUSTERS);
		return -EINVAL;
	}
	cluster = &cluster_state[num_clusters];
	cluster->cluster_id = num_clusters;
	++num_clusters;

	cpumask_copy(&cluster->cpu_mask, mask);
	cluster->num_cpus = cpumask_weight(mask);
	if (cluster->num_cpus > MAX_CPUS_PER_CLUSTER) {
		pr_info("%s: HW configuration not supported\n", TAG);
		return -EINVAL;
	}
	cluster->first_cpu = first_cpu;
	cluster->max_cpus = cluster->num_cpus;
	cluster->need_cpus = cluster->num_cpus;
	cluster->offline_throttle_ms = 40;
	cluster->enable = true;
	cluster->nr_down = 0;
	cluster->nr_up = 0;
	cluster->nr_assist = 0;
	cluster->min_cpus = default_min_cpus[cluster->cluster_id];
	cluster->up_thres = get_over_threshold(cluster->cluster_id);

	if (cluster->cluster_id == 0)
		cluster->down_thres = 0;
	else
		cluster->down_thres =
			get_over_threshold(cluster->cluster_id - 1);

	if (cluster->cluster_id == B_CLUSTER_ID) {
		cluster->thermal_degree_thres = THERMAL_TEMP * 1000;
		cluster->thermal_up_thres = INT_MAX;
	} else {
		cluster->thermal_degree_thres = INT_MAX;
		cluster->thermal_up_thres = 80;
	}

	ret = ppm_data_init(cluster);
	if (ret)
		pr_info("initialize ppm data failed ret = %d", ret);

	cluster->nr_not_preferred_cpus = 0;
	spin_lock_init(&cluster->pending_lock);

	for_each_cpu(cpu, mask) {
		core_ctl_debug("%s: Init CPU%u state\n", TAG, cpu);
		state = &per_cpu(cpu_state, cpu);
		state->cluster = cluster;
		state->cpu = cpu;
		state->force_pause_req = CLEARED_FORCE_PAUSE;
		state->win_idx = 0;
	}

	cluster->cpu_busy_up_thres = busy_up_thres[cluster->cluster_id];
	cluster->cpu_busy_down_thres = busy_down_thres[cluster->cluster_id];
	cluster->nr_task_thres = nr_task_thres[cluster->cluster_id];
	cluster->rt_nr_task_thres = rt_nr_task_thres[cluster->cluster_id];
	cluster->active_loading_thres = active_loading_thres[cluster->cluster_id];
	cluster->freq_thres = get_freq_thres(cluster->cluster_id);

	cluster->next_offline_time =
		ktime_to_ms(ktime_get()) + cluster->offline_throttle_ms;
	cluster->active_cpus = get_active_cpu_count(cluster);

	for (i=0; i<MAX_DEMAND_REQUESTER; i++) {
		cluster->demand_list[i].have_demand = 0;
		cluster->demand_list[i].max_cpus = cluster->num_cpus;
		cluster->demand_list[i].min_cpus = cluster->num_cpus;
	}

	cluster->core_ctl_thread = kthread_run(try_core_ctl, (void *) cluster,
			"core_ctl_v5/%d", first_cpu);
	if (IS_ERR(cluster->core_ctl_thread))
		return PTR_ERR(cluster->core_ctl_thread);

	sched_setscheduler_nocheck(cluster->core_ctl_thread, SCHED_FIFO,
			&param);

	cluster->inited = true;

	kobject_init(&cluster->kobj, &ktype_core_ctl);
	return kobject_add(&cluster->kobj, &dev->kobj, "core_ctl");
}

static inline int get_opp_count(struct cpufreq_policy *policy)
{
	int opp_nr;
	struct cpufreq_frequency_table *freq_pos;

	cpufreq_for_each_entry_idx(freq_pos, policy->freq_table, opp_nr);
	return opp_nr;
}

/*
 * x1: BL cap
 * y1: BL eff
 * x2: B  cap
 * y2, B  eff
 */
bool check_eff_precisely(unsigned int x1,
			 unsigned long y1,
			 unsigned int x2,
			 unsigned long y2)
{
	unsigned int diff;
	unsigned long new_y1 = 0;

	diff = (unsigned int)div64_u64(x2 * 100, x1);
	new_y1 = (unsigned long)div64_u64(y1 * diff, 100);
	return y2 < new_y1;
}

static unsigned int find_turn_point(struct cluster_data *c1,
				    struct cluster_data *c2,
				    bool is_thermal)
{
	int i, j;
	bool changed = false;
	bool stop_going = false;
	unsigned int turn_point = 0;

	for (i = 0; i < c1->ppm_data.opp_nr - 1; i++) {
		changed = false;
		for (j = c2->ppm_data.opp_nr - 1; j >= 0; j--) {
			unsigned long c1_eff, c2_eff;

			if (c2->ppm_data.ppm_tbl[j].capacity <
					c1->ppm_data.ppm_tbl[i].capacity)
				continue;

			c1_eff = is_thermal ? c1->ppm_data.ppm_tbl[i].thermal_eff
				: c1->ppm_data.ppm_tbl[i].eff;
			c2_eff = is_thermal ? c2->ppm_data.ppm_tbl[j].thermal_eff
				: c2->ppm_data.ppm_tbl[j].eff;
			if (c2_eff < c1_eff ||
					check_eff_precisely(
						c1->ppm_data.ppm_tbl[i].capacity, c1_eff,
						c2->ppm_data.ppm_tbl[j].capacity, c2_eff)) {
				turn_point = c2->ppm_data.ppm_tbl[j].capacity;
				changed = true;
				/*
				 * If lowest capacity of BCPU is more efficient than
				 * any capacity of BLCPU, we should not need to find
				 * further.
				 */
				if (j == c2->ppm_data.opp_nr - 1)
					stop_going = true;
			}
			break;
		}
		if (!changed)
			break;
		if (stop_going)
			break;
	}
	return turn_point;
}

static void update_ppm_log_handler(struct work_struct *work);
static DECLARE_DELAYED_WORK(update_ppm_log, update_ppm_log_handler);

static void update_ppm_log_handler(struct work_struct *work)
{
	int cid;
	struct cluster_data *prev_cluster;

	for(cid=0; cid<MAX_CLUSTERS; cid++) {
		if (cid != L_CLUSTER_ID) {
			prev_cluster = &cluster_state[cid - 1];
			pr_info("%s: update ppm cid=%d turn_cap=%u ", TAG, cid-1, prev_cluster->cap_turn_point);
		}
	}
}

static int update_ppm_eff(void)
{
	int opp_nr, first_cpu, cid, i;
	struct cluster_data *cluster;
	struct ppm_table *ppm_tbl;

	for(cid=0; cid<MAX_CLUSTERS; cid++) {
		cluster = &cluster_state[cid];
		first_cpu = cluster->first_cpu;

		if (!cluster->ppm_data.init)
			continue;

		/* false: gearless, true: legacy */
		opp_nr = pd_freq2opp(first_cpu, 0, true, 0) + 1;
		ppm_tbl = cluster->ppm_data.ppm_tbl;

		for(i=0; i<opp_nr; i++) {
			cluster->ppm_data.ppm_tbl[i].eff = calc_eff_hook(first_cpu,
				i, NORMAL_TEMP, ppm_tbl[i].power, ppm_tbl[i].capacity);
			cluster->ppm_data.ppm_tbl[i].thermal_eff = calc_eff_hook(first_cpu,
				i, THERMAL_TEMP, ppm_tbl[i].power, ppm_tbl[i].capacity);
		}

		/* calculate turning point */
		if (cid != L_CLUSTER_ID) {
			struct cluster_data *prev_cluster = &cluster_state[cid - 1];
			unsigned int turn_point = 0;

			turn_point = find_turn_point(prev_cluster, cluster, false);
			if (!turn_point)
				turn_point = prev_cluster->ppm_data.ppm_tbl[0].capacity;

			prev_cluster->cap_turn_point = turn_point;
		}
	}
	mod_delayed_work(system_power_efficient_wq, &update_ppm_log,
						msecs_to_jiffies(100));
	return 0;
}

static int ppm_data_init(struct cluster_data *cluster)
{
	struct cpufreq_policy *policy;
	struct ppm_table *ppm_tbl;
	struct em_perf_domain *pd;
	struct em_perf_state *ps;
	int opp_nr, first_cpu, i;
	int cid = cluster->cluster_id;

	first_cpu = cluster->first_cpu;
	policy = cpufreq_cpu_get(first_cpu);
	if (!policy) {
		pr_info("%s: cpufreq policy is not found for cpu#%d",
				TAG, first_cpu);
		return -ENOMEM;
	}

	opp_nr = get_opp_count(policy);
	ppm_tbl = kcalloc(opp_nr, sizeof(struct ppm_table), GFP_KERNEL);

	cluster->ppm_data.ppm_tbl = ppm_tbl;
	if (!cluster->ppm_data.ppm_tbl) {
		pr_info("%s: Failed to allocate ppm_table for cluster %d",
				TAG, cluster->cluster_id);
		cpufreq_cpu_put(policy);
		return -ENOMEM;
	}

	pd = em_cpu_get(first_cpu);
	if (!pd)
		return -ENOMEM;

	/* get power and capacity and calculate efficiency */
	for (i = 0; i < opp_nr; i++) {
		ps = &pd->em_table->state[opp_nr-1-i];
		ppm_tbl[i].power = ps->power;
		ppm_tbl[i].freq = ps->frequency;
		ppm_tbl[i].capacity = pd_get_opp_capacity_legacy(first_cpu, i);
		ppm_tbl[i].thermal_eff = ppm_tbl[i].eff = div64_u64(ppm_tbl[i].power, ppm_tbl[i].capacity);
	}

	cluster->ppm_data.ppm_tbl = ppm_tbl;
	cluster->ppm_data.opp_nr = opp_nr;
	cluster->ppm_data.init = 1;
	cpufreq_cpu_put(policy);

	/* calculate turning point */
	if (cid != L_CLUSTER_ID) {
		struct cluster_data *prev_cluster = &cluster_state[cid - 1];
		unsigned int turn_point = 0;

		turn_point = find_turn_point(prev_cluster, cluster, false);
		if (!turn_point)
			turn_point = prev_cluster->ppm_data.ppm_tbl[0].capacity;

		prev_cluster->cap_turn_point = turn_point;
		pr_info("%s: init ppm cid=%d, turn_cap=%u ", TAG, cid-1, turn_point);
	}
	return 0;
}

unsigned long get_freq_thres(unsigned int cid)
{
	struct cluster_data *cluster = &cluster_state[cid];
	int i, opp_nr;
	unsigned int target_freq;
	unsigned int max_capacity, target_capacity;

	if (cid >= MAX_CLUSTERS)
		return 0;

	if (!cluster->ppm_data.init)
		return freq_thres[cid];

	opp_nr = cluster->ppm_data.opp_nr;
	max_capacity = cluster->ppm_data.ppm_tbl[0].capacity;
	target_capacity = div_u64(cluster->up_thres*max_capacity, 100);
	target_freq = cluster->ppm_data.ppm_tbl[0].freq;

	for (i=1; i<opp_nr; i++) {
		if (cluster->ppm_data.ppm_tbl[i].capacity < target_capacity) {
			target_freq = cluster->ppm_data.ppm_tbl[i-1].freq;
			break;
		}
	}

	return target_freq;
}

static unsigned long core_ctl_copy_from_user(void *pvTo,
		const void __user *pvFrom, unsigned long ulBytes)
{
	if (access_ok(pvFrom, ulBytes))
		return __copy_from_user(pvTo, pvFrom, ulBytes);

	return ulBytes;
}

static int core_ctl_show(struct seq_file *m, void *v)
{
	return 0;
}

static int core_ctl_open(struct inode *inode, struct file *file)
{
	return single_open(file, core_ctl_show, inode->i_private);
}

static long core_ioctl_impl(struct file *filp,
		unsigned int cmd, unsigned long arg, void *pKM)
{
	ssize_t ret = 0;
	void __user *ubuf = (struct _CORE_CTL_PACKAGE *)arg;
	struct _CORE_CTL_PACKAGE msgKM = {0};
	bool bval;
	unsigned int val;

	switch (cmd) {
	case CORE_CTL_FORCE_PAUSE_CPU:
		if (core_ctl_copy_from_user(&msgKM, ubuf, sizeof(struct _CORE_CTL_PACKAGE)))
			return -1;

		bval = !!msgKM.is_pause;
		ret = core_ctl_force_pause_request(msgKM.cpu, bval, POWERHAL_FORCE_PAUSE);
		break;
	case CORE_CTL_SET_OFFLINE_THROTTLE_MS:
		if (core_ctl_copy_from_user(&msgKM, ubuf, sizeof(struct _CORE_CTL_PACKAGE)))
			return -1;
		ret = core_ctl_set_offline_throttle_ms(msgKM.cid, msgKM.throttle_ms);
		break;
	case CORE_CTL_SET_LIMIT_CPUS:
		if (core_ctl_copy_from_user(&msgKM, ubuf, sizeof(struct _CORE_CTL_PACKAGE)))
			return -1;
		ret = core_ctl_set_limit_cpus(msgKM.cid, msgKM.min, msgKM.max);
		break;
	case CORE_CTL_SET_NOT_PREFERRED:
		if (core_ctl_copy_from_user(&msgKM, ubuf, sizeof(struct _CORE_CTL_PACKAGE)))
			return -1;

		ret = core_ctl_set_not_preferred(msgKM.not_preferred_cpus);
		break;
	case CORE_CTL_SET_BOOST:
		if (core_ctl_copy_from_user(&msgKM, ubuf, sizeof(struct _CORE_CTL_PACKAGE)))
			return -1;

		bval = !!msgKM.boost;
		ret = core_ctl_set_boost(bval);
		break;
	case CORE_CTL_SET_UP_THRES:
		if (core_ctl_copy_from_user(&msgKM, ubuf, sizeof(struct _CORE_CTL_PACKAGE)))
			return -1;
		ret = core_ctl_set_up_thres(msgKM.cid, msgKM.thres);
		break;
	case CORE_CTL_ENABLE_POLICY:
		if (core_ctl_copy_from_user(&msgKM, ubuf, sizeof(struct _CORE_CTL_PACKAGE)))
			return -1;

		val = msgKM.enable_policy;
		ret = core_ctl_enable_policy(val);
		break;
	case CORE_CTL_SET_CPU_BUSY_THRES:
		if (core_ctl_copy_from_user(&msgKM, ubuf, sizeof(struct _CORE_CTL_PACKAGE)))
			return -1;
		core_ctl_set_cpu_busy_up_thres(msgKM.cid, msgKM.thres);
	break;
	case CORE_CTL_SET_CPU_NONBUSY_THRES:
		if (core_ctl_copy_from_user(&msgKM, ubuf, sizeof(struct _CORE_CTL_PACKAGE)))
			return -1;
		core_ctl_set_cpu_busy_down_thres(msgKM.cid, msgKM.thres);
	break;
	case CORE_CTL_SET_NR_TASK_THRES:
		if (core_ctl_copy_from_user(&msgKM, ubuf, sizeof(struct _CORE_CTL_PACKAGE)))
			return -1;
		core_ctl_set_cpu_nr_task_thres(msgKM.cid, msgKM.thres);
	break;
	case CORE_CTL_SET_FREQ_MIN_THRES:
		if (core_ctl_copy_from_user(&msgKM, ubuf, sizeof(struct _CORE_CTL_PACKAGE)))
			return -1;
		core_ctl_set_freq_min_thres(msgKM.cid, msgKM.thres);
	break;
	case CORE_CTL_SET_ACT_LOAD_THRES:
		if (core_ctl_copy_from_user(&msgKM, ubuf, sizeof(struct _CORE_CTL_PACKAGE)))
			return -1;
		core_ctl_set_cpu_active_loading_thres(msgKM.cid, msgKM.thres);
	break;
	case CORE_CTL_SET_RT_NR_TASK_THRES:
		if (core_ctl_copy_from_user(&msgKM, ubuf, sizeof(struct _CORE_CTL_PACKAGE)))
			return -1;
		core_ctl_set_cpu_rt_nr_task_thres(msgKM.cid, msgKM.thres);
	break;
	default:
		pr_info("%s: %s %d: unknown cmd %x\n",
			TAG, __FILE__, __LINE__, cmd);
		ret = -1;
		goto ret_ioctl;
	}

ret_ioctl:
	return ret;
}

static long core_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	return core_ioctl_impl(filp, cmd, arg, NULL);
}

static const struct proc_ops core_ctl_Fops = {
	.proc_ioctl = core_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.proc_compat_ioctl = core_ioctl,
#endif
	.proc_open = core_ctl_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int __init core_ctl_init(void)
{
	int ret, cluster_nr, i, ret_error_line, hpstate_dead, hpstate_online;
	struct cpumask cluster_cpus;
	struct proc_dir_entry *pe, *parent;

	hpstate_dead = cpuhp_setup_state(CPUHP_AP_ONLINE_DYN,
			"core_ctl/cpu_pause:dead",
			NULL, core_ctl_prepare_dead_cpu);
	pr_info("%s: hpstate_dead:%d", TAG, hpstate_dead);
	if (hpstate_dead == -ENOSPC) {
		pr_info("%s: fail to setup cpuhp_dead", TAG);
		ret = -ENOSPC;
		goto failed;
	}

	hpstate_online = cpuhp_setup_state(CPUHP_AP_ONLINE_DYN,
			"core_ctl/cpu_pause:online",
			core_ctl_prepare_online_cpu, NULL);
	pr_info("%s: hpstate_online:%d", TAG, hpstate_online);
	if (hpstate_online == -ENOSPC) {
		pr_info("%s: fail to setup cpuhp_online", TAG);
		ret = -ENOSPC;
		goto failed;
	}

	ret = init_sched_avg();
	if (ret)
		goto failed_remove_hpstate;

	/* register traceprob */
	ret = register_trace_android_vh_scheduler_tick(core_ctl_tick, NULL);
	if (ret) {
		ret_error_line = __LINE__;
		pr_info("%s: vendor hook register failed ret %d line %d\n",
			TAG, ret, ret_error_line);
		goto failed_exit_sched_avg;
	}

	/* init cluster_data */
	cluster_nr = arch_get_nr_clusters();

	for (i = 0; i < cluster_nr; i++) {
		arch_get_cluster_cpus(&cluster_cpus, i);
		ret = cluster_init(&cluster_cpus);
		if (ret) {
			pr_info("%s: unable to create core ctl group: %d\n", TAG, ret);
			goto failed_deprob;
		}
	}

	/* init force pause mask */
	cpumask_clear(&cpu_force_pause_mask);
	consider_VIP_task = 1;

	/* init core_ctl ioctl */
	pr_info("%s: start to init core_ioctl driver\n", TAG);
	parent = proc_mkdir("cpumgr", NULL);
	pe = proc_create("core_ioctl", 0660, parent, &core_ctl_Fops);
	if (!pe) {
		pr_info("%s: Could not create /proc/cpumgr/core_ioctl.\n", TAG);
		ret = -ENOMEM;
		goto failed_deprob;
	}

	ret = register_pt_isolate_cb(core_ctl_force_pause_request);
	if (ret)
		pr_info("%s: Could not register register_pt_isolate_cb\n", TAG);

	/* start periodic debug msg */
	mod_delayed_work(system_power_efficient_wq,
			&periodic_debug, msecs_to_jiffies(periodic_debug_delay));
	initialized = true;
	return 0;

failed_deprob:
	unregister_trace_android_vh_scheduler_tick(core_ctl_tick, NULL);
failed_exit_sched_avg:
	exit_sched_avg();
	tracepoint_synchronize_unregister();
failed_remove_hpstate:
	cpuhp_remove_state(hpstate_dead);
	cpuhp_remove_state(hpstate_online);
failed:
	return ret;
}

static void __exit core_ctl_exit(void)
{
	exit_sched_avg();
	unregister_trace_android_vh_scheduler_tick(core_ctl_tick, NULL);
	tracepoint_synchronize_unregister();
}

module_init(core_ctl_init);
module_exit(core_ctl_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mediatek Inc.");
MODULE_DESCRIPTION("Mediatek core_ctl");
