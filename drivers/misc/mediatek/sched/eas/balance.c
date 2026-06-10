// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <sched/sched.h>
#include <linux/sched/clock.h>

#include "common.h"
#include "balance.h"
#include "eas/eas_plus.h"
#include "sugov/cpufreq.h"
#include "sched_trace.h"
#include "mt-plat/mtk_irq_mon.h"

static DEFINE_PER_CPU(u64, next_update_new_balance_time_ns);

static DEFINE_RAW_SPINLOCK(migration_lock);

/* modified from kmainline can_migrate_task() */
int mtk_can_migrate_task(struct task_struct *p, int dst_cpu)
{
	int src_cpu = task_cpu(p);
	struct rq *src_rq = cpu_rq(src_cpu);
	struct cpumask eff_mask;
	bool latency_sensitive;

	if (p->se.sched_delayed)
		return -6;

	if (cpu_paused(dst_cpu))
		return -5;

	if (!cpumask_test_cpu(dst_cpu, p->cpus_ptr))
		return -4;

	if (task_is_vip(p, VVIP)) {
		int num_vip_src = num_vip_in_cpu(src_cpu, VVIP);
		int num_vip_dst = num_vip_in_cpu(dst_cpu, VVIP);

		if (num_vip_src-1 < num_vip_dst) {
			return -3;
		} else if ((num_vip_src-1 == num_vip_dst) &&
			(arch_scale_cpu_capacity(src_cpu) > arch_scale_cpu_capacity(dst_cpu))) {
			return -2;
		}
	}

	if (task_on_cpu(src_rq, p))
		return -1;

	if (READ_ONCE(src_rq->rd->overutilized))
		return 1;

	compute_effective_softmask(p, &latency_sensitive, &eff_mask);
	if (latency_sensitive && !(cpumask_test_cpu(dst_cpu, &eff_mask)))
		return 0;

	return 2;
}

/* hooked from kmainline can_migrate_task() */
void hook_can_migrate_task(void *data, struct task_struct *p, int dst_cpu, int *can_migrate)
{
	if (!get_eas_hook())
		return;

	*can_migrate = (mtk_can_migrate_task(p, dst_cpu) > 0);
}

/* modified from kmainline detach_task() */
static void detach_task(struct task_struct *p, struct rq *src_rq, int dst_cpu)
{
	lockdep_assert_rq_held(src_rq);

	deactivate_task(src_rq, p, DEQUEUE_NOCLOCK);
	set_task_cpu(p, dst_cpu);
}

/* modified from kmainline detach_one_task() */
static struct task_struct *detach_one_task(struct rq *src_rq, int dst_cpu)
{
	struct task_struct *p = NULL, *best_task = NULL, *backup = NULL;
	int dst_capacity, src_capacity;
	unsigned int task_util_src, task_util_dst, margin_src;
	bool latency_sensitive = false, in_many_heavy_tasks;
	struct root_domain *rd __maybe_unused = cpu_rq(smp_processor_id())->rd;
	int type;

	lockdep_assert_rq_held(src_rq);

	rcu_read_lock(); /* must hold runqueue lock for queue se is currently on */

	in_many_heavy_tasks = rd->android_vendor_data1;

	if (!in_many_heavy_tasks)
		goto unlock;

	if (is_dpt_v2_support()) {
		src_capacity = DPT_V2_MAX_RUNNING_TIME_LOCAL;
		dst_capacity = DPT_V2_MAX_RUNNING_TIME_LOCAL * cpu_freq_ceiling(dst_cpu) / get_cpu_max_freq(dst_cpu);
	} else {
		src_capacity = arch_scale_cpu_capacity(src_rq->cpu);
		dst_capacity = cpu_cap_ceiling(dst_cpu);
		margin_src = get_adaptive_margin(src_rq->cpu);
	}

	list_for_each_entry_reverse(p, &src_rq->cfs_tasks, se.group_node) {
		type = mtk_can_migrate_task(p, dst_cpu);
		if (type <= 0)
			continue;

		/*task_util = uclamp_task_util(p);*/
		if (is_dpt_v2_support()) {
			int using_uclamp_freq = 0;

			task_util_src = uclamp_task_util_dpt_v2(p, src_rq->cpu, &using_uclamp_freq);
			margin_src = using_uclamp_freq ? NO_MARGIN : get_adaptive_margin(src_rq->cpu);

			task_util_dst = uclamp_task_util_dpt_v2(p, src_rq->cpu, &using_uclamp_freq);
		} else
			task_util_src = task_util_dst = uclamp_task_util(p);

		if (in_many_heavy_tasks &&
			!fits_capacity(task_util_src, src_capacity, margin_src)) {
			/* when too many big task, pull misfit runnable task */
			best_task = p;
			break;
		} else if (latency_sensitive &&
			task_util_dst <= dst_capacity) {
			best_task = p;
			break;
		} else if (latency_sensitive && !backup) {
			backup = p;
		}
	}
	p = best_task ? best_task : backup;

	if (p)
		detach_task(p, src_rq, dst_cpu);

unlock:
	rcu_read_unlock();

	return p;
}

/* cloned from kmainline attach_task() */
static void attach_task(struct rq *rq, struct task_struct *p)
{
	lockdep_assert_rq_held(rq);

	WARN_ON_ONCE(task_rq(p) != rq);
	activate_task(rq, p, ENQUEUE_NOCLOCK);
	wakeup_preempt(rq, p, 0);
}

/* cloned from kmainline attach_one_task() */
static void attach_one_task(struct rq *rq, struct task_struct *p)
{
	struct rq_flags rf;

	rq_lock(rq, &rf);
	update_rq_clock(rq);
	attach_task(rq, p);
	rq_unlock(rq, &rf);
}

void try_to_pull_VVIP(int this_cpu, bool *had_pull_vvip, struct rq_flags *src_rf)
{
	struct root_domain *rd;
	struct perf_domain *pd;
	struct rq *src_rq, *this_rq;
	struct task_struct *p;
	int cpu, vip_prio;

	if (!cpumask_test_cpu(this_cpu, &bcpus))
		return;

	if (cpu_paused(this_cpu))
		return;

	if (!cpu_active(this_cpu))
		return;

	this_rq = cpu_rq(this_cpu);
	rd = this_rq->rd;
	rcu_read_lock();
	pd = rcu_dereference(rd->pd);
	if (!pd)
		goto unlock;

	for (; pd; pd = pd->next) {
		for_each_cpu(cpu, perf_domain_span(pd)) {

			if (cpu_paused(cpu))
				continue;

			if (!cpu_active(cpu))
				continue;

			if (cpu == this_cpu)
				continue;

			src_rq = cpu_rq(cpu);

			if (num_vip_in_cpu(cpu, VVIP) < 1)
				continue;

			else if (num_vip_in_cpu(cpu, VVIP) == 1) {
				/* the only one VVIP in cpu is running */
				if (src_rq->curr) {
					vip_prio = get_vip_task_prio(src_rq->curr);
					if (prio_is_vip(vip_prio, VVIP))
						continue;
				}
			}

			/* There are runnables in cpu */
			rq_lock_irqsave(src_rq, src_rf);
			if (src_rq->curr)
				update_rq_clock(src_rq);
			p = next_vip_runnable_in_cpu(src_rq, VVIP);
			if (p && cpumask_test_cpu(this_cpu, p->cpus_ptr)) {
				deactivate_task(src_rq, p, DEQUEUE_NOCLOCK);
				set_task_cpu(p, this_cpu);
				rq_unlock_irqrestore(src_rq, src_rf);

				if (trace_sched_force_migrate_enabled())
					trace_sched_force_migrate(p, this_cpu, MIGR_IDLE_PULL_VIP_RUNNABLE);
				attach_one_task(this_rq, p);
				*had_pull_vvip = true;
				goto unlock;
			}
			rq_unlock_irqrestore(src_rq, src_rf);
		}
	}
unlock:
	rcu_read_unlock();
}

/* modified from kmainline active_load_balance_cpu_stop() */
static int mtk_active_load_balance_cpu_stop(void *data)
{
	struct task_struct *target_task = data;
	int busiest_cpu = smp_processor_id();
	struct rq *busiest_rq = cpu_rq(busiest_cpu);
	int target_cpu = busiest_rq->push_cpu;
	struct rq *target_rq = cpu_rq(target_cpu);
	struct rq_flags rf;
	int deactivated = 0;

	local_irq_disable();
	raw_spin_lock(&target_task->pi_lock);
	rq_lock(busiest_rq, &rf);

	if (task_cpu(target_task) != busiest_cpu ||
		(!cpumask_test_cpu(target_cpu, target_task->cpus_ptr)) ||
		task_on_cpu(busiest_rq, target_task) ||
		target_rq == busiest_rq)
		goto out_unlock;

	if (!task_on_rq_queued(target_task))
		goto out_unlock;

	if (!cpu_active(busiest_cpu) || !cpu_active(target_cpu))
		goto out_unlock;

	if (cpu_paused(busiest_cpu) || cpu_paused(target_cpu))
		goto out_unlock;

	/* Make sure the requested CPU hasn't gone down in the meantime: */
	if (unlikely(!busiest_rq->active_balance))
		goto out_unlock;

	/* Is there any task to move? */
	if (busiest_rq->nr_running <= 1)
		goto out_unlock;

	update_rq_clock(busiest_rq);
	deactivate_task(busiest_rq, target_task, DEQUEUE_NOCLOCK);
	set_task_cpu(target_task, target_cpu);
	deactivated = 1;
out_unlock:
	busiest_rq->active_balance = 0;
	rq_unlock(busiest_rq, &rf);

	if (deactivated)
		attach_one_task(target_rq, target_task);

	raw_spin_unlock(&target_task->pi_lock);
	put_task_struct(target_task);

	local_irq_enable();
	return 0;
}

/* modified from kmainline sched_balance_rq() about need_active_balance() */
int migrate_running_task(int this_cpu, struct task_struct *p, struct rq *target, int reason)
{
	int active_balance = false;
	unsigned long flags;
	bool latency_sensitive = false;
	struct cpumask effective_softmask;

	compute_effective_softmask(p, &latency_sensitive, &effective_softmask);

	raw_spin_rq_lock_irqsave(target, flags);
	if (!target->active_balance &&
		(task_rq(p) == target) && READ_ONCE((p)->__state) != TASK_DEAD &&
		 !(latency_sensitive && !cpumask_test_cpu(this_cpu, &effective_softmask))) {
		target->active_balance = 1;
		target->push_cpu = this_cpu;
		active_balance = true;
		get_task_struct(p);
	}

	preempt_disable();
	raw_spin_rq_unlock_irqrestore(target, flags);
	if (active_balance) {
		if (trace_sched_force_migrate_enabled())
			trace_sched_force_migrate(p, this_cpu, reason);

		stop_one_cpu_nowait(cpu_of(target),
				mtk_active_load_balance_cpu_stop,
				p, &target->active_balance_work);
	}
	preempt_enable();
	return active_balance;
}

/* hooked from kmainline sched_balance_newidle() */
void hook_sched_balance_newidle(void *data, struct rq *this_rq, struct rq_flags *rf, int *pulled_task, int *done)
{
	int cpu;
	struct rq *src_rq, *misfit_task_rq = NULL;
	struct task_struct *p = NULL, *best_running_task = NULL;
	struct rq_flags src_rf;
	int this_cpu = this_rq->cpu;
	unsigned long misfit_load = 0;
	u64 now_ns;
	bool latency_sensitive = false;
	struct cpumask effective_softmask;
	bool had_pull_vvip = false;

	if (!get_eas_hook())
		return;

	if (cpu_paused(this_cpu)) {
		*done = 1;
		return;
	}

	/*
	 * There is a task waiting to run. No need to search for one.
	 * Return 0; the task will be enqueued when switching to idle.
	 */
	if (this_rq->ttwu_pending)
		return;

	/*
	 * We must set idle_stamp _before_ calling idle_balance(), such that we
	 * measure the duration of idle_balance() as idle time.
	 */
	this_rq->idle_stamp = rq_clock(this_rq);

	/*
	 * Do not pull tasks towards !active CPUs...
	 */
	if (!cpu_active(this_cpu))
		return;

	now_ns = ktime_get_real_ns();
	if (now_ns < per_cpu(next_update_new_balance_time_ns, this_cpu))
		return;
	per_cpu(next_update_new_balance_time_ns, this_cpu) = now_ns + new_idle_balance_interval_ns;

	if (trace_sched_next_new_balance_enabled())
		trace_sched_next_new_balance(now_ns, per_cpu(next_update_new_balance_time_ns, this_cpu));

	/*
	 * This is OK, because current is on_cpu, which avoids it being picked
	 * for load-balance and preemption/IRQs are still disabled avoiding
	 * further scheduler activity on it and we're being very careful to
	 * re-start the picking loop.
	 */
	rq_unpin_lock(this_rq, rf);
	raw_spin_rq_unlock(this_rq);

	this_cpu = this_rq->cpu;

	/* try to pull runnable VVIP if this_cpu is in big gear */
	try_to_pull_VVIP(this_cpu, &had_pull_vvip, &src_rf);
	if (had_pull_vvip)
		goto out;

	for_each_cpu(cpu, cpu_active_mask) {
		if (cpu == this_cpu)
			continue;

		src_rq = cpu_rq(cpu);
		rq_lock_irqsave(src_rq, &src_rf);
		update_rq_clock(src_rq);
		if (src_rq->active_balance) {
			rq_unlock_irqrestore(src_rq, &src_rf);
			continue;
		}
		if ((src_rq->misfit_task_load > misfit_load) &&
			(cpu_cap_ceiling(this_cpu) > cpu_cap_ceiling(cpu))) {
			p = src_rq->curr;
			if (p) {
				compute_effective_softmask(p, &latency_sensitive,
							&effective_softmask);
				if (p->policy == SCHED_NORMAL &&
					cpumask_test_cpu(this_cpu, p->cpus_ptr) &&
					!(latency_sensitive &&
					!cpumask_test_cpu(this_cpu, &effective_softmask))) {

					misfit_task_rq = src_rq;
					misfit_load = src_rq->misfit_task_load;
					if (best_running_task)
						put_task_struct(best_running_task);
					best_running_task = p;
					get_task_struct(best_running_task);
				}
			}
			p = NULL;
		}

		if (src_rq->nr_running <= 1) {
			rq_unlock_irqrestore(src_rq, &src_rf);
			continue;
		}
		p = detach_one_task(src_rq, this_cpu);
		rq_unlock_irqrestore(src_rq, &src_rf);

		if (p) {
			if (trace_sched_force_migrate_enabled())
				trace_sched_force_migrate(p, this_cpu, MIGR_IDLE_BALANCE);
			attach_one_task(this_rq, p);
			break;
		}
	}

	/*
	 * If p is null meaning that we have not pull a runnable task, we try to
	 * pull a latency sensitive running task.
	 */
	if (!p && misfit_task_rq)
		*done = migrate_running_task(this_cpu, best_running_task,
					misfit_task_rq, MIGR_IDLE_PULL_MISFIT_RUNNING);
	if (best_running_task)
		put_task_struct(best_running_task);
out:
	raw_spin_rq_lock(this_rq);
	/*
	 * While browsing the domains, we released the rq lock, a task could
	 * have been enqueued in the meantime. Since we're not going idle,
	 * pretend we pulled a task.
	 */
	if (this_rq->cfs.h_nr_running && !*pulled_task)
		*pulled_task = 1;

	/* Is there a task of a high priority class? */
	if (this_rq->nr_running != this_rq->cfs.h_nr_running)
		*pulled_task = -1;

	if (*pulled_task)
		this_rq->idle_stamp = 0;

	if (*pulled_task != 0)
		*done = 1;

	rq_repin_lock(this_rq, rf);

}

int select_idle_cpu_from_domains(struct task_struct *p, struct perf_domain **prefer_pds, unsigned int len)
{
	struct perf_domain *pd;
	int cpu, best_cpu = -1;

	for (unsigned int i = 0; i < len; i++) {
		pd = prefer_pds[i];
		for_each_cpu_and(cpu, perf_domain_span(pd),
						cpu_active_mask) {
			if (!cpumask_test_cpu(cpu, p->cpus_ptr))
				continue;
			if (mtk_available_idle_cpu(cpu)) {
				best_cpu = cpu;
				break;
			}
		}
		if (best_cpu != -1)
			break;
	}

	return best_cpu;
}

int select_bigger_idle_cpu(struct task_struct *p)
{
	struct root_domain *rd = cpu_rq(smp_processor_id())->rd;
	struct perf_domain *pd, *prefer_pds[MAX_NR_CPUS];
	int cpu = task_cpu(p), bigger_idle_cpu = -1;
	unsigned int i = 0;
	long max_capacity = cpu_cap_ceiling(cpu);
	long capacity;

	rcu_read_lock();
	pd = rcu_dereference(rd->pd);

	for (; pd; pd = pd->next) {
		capacity = cpu_cap_ceiling(cpumask_first(perf_domain_span(pd)));
		if (capacity > max_capacity &&
			cpumask_intersects(p->cpus_ptr, perf_domain_span(pd))) {
			prefer_pds[i++] = pd;
		}
	}

	if (i != 0)
		bigger_idle_cpu = select_idle_cpu_from_domains(p, prefer_pds, i);

	rcu_read_unlock();
	return bigger_idle_cpu;
}

void check_for_migration(struct task_struct *p)
{
	int new_cpu = -1, better_idle_cpu = -1;
	int cpu = task_cpu(p);
	struct rq *rq = cpu_rq(cpu);

	irq_log_store();

	if (rq->misfit_task_load) {
		struct em_perf_domain *pd;
		struct cpufreq_policy *policy;
		int opp_curr = 0, thre = 0, thre_idx = 0;

		if (rq->curr->__state != TASK_RUNNING ||
			rq->curr->nr_cpus_allowed == 1)
			return;

		pd = em_cpu_get(cpu);
		if (!pd)
			return;

		thre_idx = (pd->nr_perf_states >> 3) - 1;
		if (thre_idx >= 0)
			thre = pd->em_table->state[thre_idx].frequency;

		policy = cpufreq_cpu_get(cpu);
		irq_log_store();

		if (policy) {
			opp_curr = policy->cur;
			cpufreq_cpu_put(policy);
		}

		if (opp_curr <= thre) {
			irq_log_store();
			return;
		}

		raw_spin_lock(&migration_lock);
		irq_log_store();
		raw_spin_lock(&p->pi_lock);
		irq_log_store();

		new_cpu = p->sched_class->select_task_rq(p, cpu, WF_TTWU);
		irq_log_store();

		raw_spin_unlock(&p->pi_lock);

		if ((new_cpu < 0) || new_cpu >= MAX_NR_CPUS ||
			(cpu_cap_ceiling(new_cpu) <= cpu_cap_ceiling(cpu)))
			better_idle_cpu = select_bigger_idle_cpu(p);

		if (better_idle_cpu >= 0)
			new_cpu = better_idle_cpu;

		if (new_cpu < 0) {
			raw_spin_unlock(&migration_lock);
			irq_log_store();
			return;
		}

		irq_log_store();
		if ((better_idle_cpu >= 0) ||
			(new_cpu < MAX_NR_CPUS && new_cpu >= 0 &&
			(cpu_cap_ceiling(new_cpu) > cpu_cap_ceiling(cpu)))) {
			raw_spin_unlock(&migration_lock);

			migrate_running_task(new_cpu, p, rq, MIGR_TICK_PULL_MISFIT_RUNNING);
			irq_log_store();
		} else {
#if IS_ENABLED(CONFIG_MTK_SCHED_BIG_TASK_ROTATE)
			int thre_rot = 0, thre_rot_idx = 0;

			thre_rot_idx = (pd->nr_perf_states >> 1) - 1;
			if (thre_rot_idx >= 0)
				thre_rot = pd->em_table->state[thre_rot_idx].frequency;

			if (opp_curr > thre_rot) {
				task_check_for_rotation(rq);
				irq_log_store();
			}

#endif
			raw_spin_unlock(&migration_lock);
		}
	}

	irq_log_store();
}
