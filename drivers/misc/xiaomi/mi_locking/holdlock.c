// SPDX-License-Identifier: GPL-2.0+
/*
 * mi_locking will show some kernel lock wait/hold time.
 *
 * Copyright (C) 2024 Xiaomi Ltd.
 */

#define MI_LOCK_LOG_TAG       "holdlock"
#include <linux/hrtimer.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <trace/events/napi.h>
#include <linux/init.h>
#include <linux/stacktrace.h>
#include <linux/mutex.h>
#include <trace/hooks/rwsem.h>
#include <trace/hooks/dtask.h>
#include <asm/syscall.h>
#include <linux/proc_fs.h>

#include "holdlock.h"
#include "waitlock.h"
#include "locking_main.h"
#include "waitlock.h"

#define MI_RWSEM_WRITER_LOCKED	(1UL << 0)
#define LOCK_END_FLAG           0

extern char *holdlock_str[];

unsigned int (*stack_trace_save_skip_hardirq)(struct pt_regs *regs,
                                               unsigned long *store,
                                               unsigned int size,
                                               unsigned int skipnr);
struct xm_stack_trace __percpu *p_mutex_stack_trace;
struct xm_stack_trace __percpu *p_rwsem_stack_trace;
struct lock_stats monitor_info[GRP_TYPES];

/******************************saving stack strace********************************/
void xm_store_stack_trace(struct pt_regs *regs,
				     struct stack_entry *stack_entry,
				     unsigned long *entries,
				     unsigned int max_entries, int skip)
{
	stack_entry->entries = entries;
	if (regs && stack_trace_save_skip_hardirq)
	{
	    pr_info("holdlock noschedule stack_trace_save_skip_hardirq \n");
		stack_entry->nr_entries = stack_trace_save_skip_hardirq(regs, entries, max_entries, skip);
	}
	else
	{
	    pr_info("holdlock noschedule stack_trace_save \n");
		stack_entry->nr_entries = stack_trace_save(entries, max_entries, skip);
	}
}

bool xm_stack_trace_record_for_opt_lock(struct task_struct* tsk, struct xm_stack_trace *stack_trace, struct pt_regs *regs, int duration, bool flag)
{
	unsigned int nr_entries, nr_stack_entries;
	struct stack_entry *stack_entry;

	nr_entries = stack_trace->nr_entries;
	if (nr_entries >= MAX_TRACE_ENTRIES)
	{
	    pr_err("xm_stack nr_entries is full, %u\n", nr_entries);
		return false;
	}

	nr_stack_entries = stack_trace->nr_stack_entries;
	if (nr_stack_entries >= MAX_STACE_TRACE_ENTRIES)
		return false;

	strlcpy(stack_trace->curr_comms[nr_stack_entries], current->comm, TASK_COMM_LEN);
	strlcpy(stack_trace->parent_comms[nr_stack_entries], current->group_leader->comm, TASK_COMM_LEN);
	stack_trace->pids[nr_stack_entries] = tsk->pid;
	stack_trace->duration[nr_stack_entries] = duration;
	stack_trace->timestamp[nr_stack_entries] = sched_clock();
	stack_trace->prio[nr_stack_entries] = current->prio;
	stack_trace->flag[nr_stack_entries] = flag;

	stack_entry = stack_trace->stack_entries + nr_stack_entries;
	xm_store_stack_trace(regs, stack_entry, stack_trace->entries + nr_entries, MAX_TRACE_ENTRIES - nr_entries, 0);
	stack_trace->nr_entries += stack_entry->nr_entries;

	smp_store_release(&stack_trace->nr_stack_entries, nr_stack_entries + 1);

	if ((stack_trace->nr_entries >= MAX_TRACE_ENTRIES)  || (stack_trace->nr_stack_entries > MAX_STACE_TRACE_ENTRIES)) {
		pr_err("BUG: xm_stack MAX_TRACE_ENTRIES too low cpu=%d, nr_entries=%u, nr_stack_entries=%u\n",
			smp_processor_id(),
			stack_trace->nr_entries,
			stack_trace->nr_stack_entries);
		return false;
	}

	return true;
}

/**
 * stack_trace_record - to save current backtrace
 * @param1: struct xm_stack_trace *stack_trace
 * @param2: duration time, ms
 * @param3: flag, useless
 *
 * eg:
 	 struct xm_stack_trace *stack_trace = this_cpu_ptr(p_mutex_stack_trace);
	 stack_trace_record(stack_trace, ms, flag);
 *
 */
inline bool stack_trace_record(struct xm_stack_trace *stack_trace, int duration, bool flag)
{
		return xm_stack_trace_record_for_opt_lock(current, stack_trace, get_irq_regs(), duration, flag);
}

static int noop_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return 0;
}

static int stack_trace_skip_hardirq_init(void)
{
	int ret;
	struct kprobe kp;
	unsigned long (*kallsyms_lookup_name_fun)(const char *name);

	ret = -1;
	kp.symbol_name = "kallsyms_lookup_name";
	kp.pre_handler = noop_pre_handler;
	stack_trace_save_skip_hardirq = NULL;

	ret = register_kprobe(&kp);
	if (ret < 0) {
	    pr_err("trace_irqoff register_kprobe failed!\n");
	    return -1;
	}
	pr_info("trace_irqoff register_kprobe successfully!\n");
	kallsyms_lookup_name_fun = (void*)kp.addr;

	pr_info("irqoff kallsyms_lookup_name_fun is %p\n", kallsyms_lookup_name_fun);

    if (!kallsyms_lookup_name_fun) {
		pr_err("irq-off kallsyms_lookup_name_fun get failed!\n");
        return 1;
    }

	unregister_kprobe(&kp);

	stack_trace_save_skip_hardirq =
		(void *)kallsyms_lookup_name_fun("stack_trace_save_regs");

	return 0;
}

static int track_stat_update(int grp_idx, int type, struct track_stat *ts, u64 time)
{
	int thres_type;

	if (NULL == ts)
		return -1;

	if (type >= LOCK_TYPES) {
		ml_err("pass error type param,""type = %d\n", type);
		return -1;
	}

	thres_type = waittime_thres_exceed_type(grp_idx, type, time); //LOW ,HIGH, FATAL
	if (thres_type < 0)
		return thres_type;

	atomic_inc(&ts->level[thres_type]);
//#ifdef CONFIG_XM_INTERNAL_VERSION
//	atomic64_add(time, &ts->exp_total_time);
//#endif
	cond_trace_printk(locking_opt_debug(LK_DEBUG_PRINTK),
		"[%s]: add exceed low thres item, grp = %s, type = %s, time = %llu\n", 
		__func__, group_str[grp_idx], holdlock_str[type], time);
	return thres_type;
}

static int lock_stats_update(int grp_idx, int type, u64 time)
{
	struct lock_stats *pcs;
	int ret;
	/*
	 * Get percpu data ptr.
	 * Prevent process from switching to another CPU, disable preempt.
	 * There only is a atomic_inc operation(and/or a trace printk)
	 * during the preemtion off, no particularly time-consuming operations.
	 */
	pcs = &monitor_info[grp_idx];

	ret = track_stat_update(grp_idx, type, &pcs->per_type_stat[type], time);

	return ret;
}

/**
 * handle_hold_stats
 * @param1: mutex or rwsemw
 * @param2: hold lock time， ns
 *
 * handle hold lock time, get cnt to show or
 * save stack trace if UX/RT in fatal
 *
 */
__always_inline void handle_hold_stats(int type, u64 time)
{
	int grp_idx;
	int ret;
	int flag = 1;

	grp_idx = get_task_grp_idx();

	ret = lock_stats_update(grp_idx, type, time); //update data

	if (g_opt_stack && ret >= 0) {
		/*
		 * Only record UX/RT's fatal info, We don't care other groups.
		 */
		if ((ret == FATAL_THRES) && (grp_idx <= GRP_RT)) {
			cond_trace_printk(locking_opt_debug(LK_DEBUG_PRINTK),
				"[%s]: type=%d, curr->pid=%d, curr->comm=%s, comm->name=%s, holdtime_ms=%llu\n",
				__func__, type, current->pid, current->comm, current->group_leader->comm, time/NSEC_PER_MSEC);
			if (type == MUTEX) {
				struct xm_stack_trace *stack_trace = this_cpu_ptr(p_mutex_stack_trace);
				stack_trace_record(stack_trace, time/NSEC_PER_MSEC, flag);
			}
			else if (type == RWSEM_WRITE){
				struct xm_stack_trace *stack_trace = this_cpu_ptr(p_rwsem_stack_trace);
				stack_trace_record(stack_trace, time/NSEC_PER_MSEC, flag);
			}
		}
	}
}

void android_vh_rwsem_write_finished_handler(void *ignore, struct rw_semaphore *sem)
{
	u64 delta = 0, ms;
	u16 nvcsw = 0,  nvcsw_delta = 0; // 0~65535

	if (g_opt_nvcsw) {
		ms = (sem->android_oem_data1[0] >> 16) & 0xFFFFFFFFFFFF;
		delta = sched_clock() - ms * NSEC_PER_MSEC;
		handle_hold_stats(RWSEM_WRITE,delta);
	}
	else {
		nvcsw = (u16)(sem->android_oem_data1[0] & 0xFFFF);
		nvcsw_delta  = current->nvcsw - nvcsw;
		if (nvcsw_delta == 0) {
			ms = (sem->android_oem_data1[0] >> 16) & 0xFFFFFFFFFFFF;
			delta = sched_clock() - ms * NSEC_PER_MSEC;
			handle_hold_stats(RWSEM_WRITE,delta);
		}
	}
}

void android_vh_record_rwsem_lock_starttime_handler(void *ignore, struct rw_semaphore *sem, unsigned long settime_jiffies)
{
	u64 ms;
	u16 nvcsw = 0;

	/* resem lock holding end flag */
	if (settime_jiffies != LOCK_END_FLAG) {
		if (atomic_long_read(&sem->count) & MI_RWSEM_WRITER_LOCKED){
			/*
			 *    0：15bit - save nvcsw
		 	 *    16：63bit  - save sched_clock()
		 	 */
			nvcsw = current->nvcsw;
			ms = sched_clock() / NSEC_PER_MSEC;
			sem->android_oem_data1[0] &= ~0xFFFF;          // Clear the lower 16 bits
			sem->android_oem_data1[0] |= (nvcsw & 0xFFFF); // Store the lower 16 bits of nvcsw
			sem->android_oem_data1[0] &= 0xFFFF;           // Clear the upper 48 bits
			sem->android_oem_data1[0] |= (ms << 16);       // Store the value of sched_time_ms in the upper 48 bits
		}
	}
}

void android_vh_record_mutex_lock_starttime_handler(void *nouse, struct mutex *lock, unsigned long settime_jiffies)
{
	u64 delta = 0, ms;
	u16 nvcsw = 0,  nvcsw_delta = 0; // 0~65535

	/* mutex lock holding start flag */
	if (settime_jiffies != LOCK_END_FLAG){
		/*
		 *    0：15bit - save nvcsw
		 *    16：63bit  - save sched_clock()
		 */
		nvcsw = current->nvcsw;
		ms = sched_clock() / NSEC_PER_MSEC;
		lock->android_oem_data1[0] &= ~0xFFFF;          // Clear the lower 16 bits
		lock->android_oem_data1[0] |= (nvcsw & 0xFFFF); // Store the lower 16 bits of nvcsw
		lock->android_oem_data1[0] &= 0xFFFF;           // Clear the upper 48 bits
		lock->android_oem_data1[0] |= (ms << 16);       // Store the value of sched_time_ms in the upper 48 bits
	}
	/* mutex lock holding end flag */
	else if (settime_jiffies == LOCK_END_FLAG){
		if (g_opt_nvcsw) {
			ms = (lock->android_oem_data1[0] >> 16) & 0xFFFFFFFFFFFF;
			delta = sched_clock() - ms * NSEC_PER_MSEC;
			handle_hold_stats(MUTEX,delta);
		}
		else {
			nvcsw = (u16)(lock->android_oem_data1[0] & 0xFFFF);
			nvcsw_delta  = current->nvcsw - nvcsw;
			if (nvcsw_delta == 0) {
				ms = (lock->android_oem_data1[0] >> 16) & 0xFFFFFFFFFFFF;
				delta = sched_clock() - ms * NSEC_PER_MSEC;
				handle_hold_stats(MUTEX,delta);
			}
		}
	}
}
 
int holdlock_init(void)
{
	int ret = 0;

	if (g_opt_enable & HOLD_LK_ENABLE) {
		REGISTER_TRACE_VH(android_vh_record_mutex_lock_starttime, android_vh_record_mutex_lock_starttime_handler);
		REGISTER_TRACE_VH(android_vh_record_rwsem_lock_starttime, android_vh_record_rwsem_lock_starttime_handler);
		REGISTER_TRACE_VH(android_vh_rwsem_write_finished, android_vh_rwsem_write_finished_handler);
	}

	p_mutex_stack_trace = alloc_percpu(struct xm_stack_trace);
	p_rwsem_stack_trace = alloc_percpu(struct xm_stack_trace);

	if (!p_mutex_stack_trace || !p_rwsem_stack_trace) {
		ml_err("alloc_percpu failed!");
		goto free_buf;
	}

	ret = stack_trace_skip_hardirq_init();
	if (ret) {
		ml_err("stack_trace_skip_hardirq_init failed, ret=%d\n", ret);
		goto out;
	}

	ret = holdlock_proc_init();
	if (ret) {
		ml_err(" failed, ret = %d\n", ret);
		goto out;
	}

	ml_info(" %s: succesed!\n", __func__);

 	return 0;

out:
	return 1;

free_buf:
	free_percpu(p_mutex_stack_trace);
	free_percpu(p_rwsem_stack_trace);

	return -ENOMEM;
}

void holdlock_exit(void)
{
	if (g_opt_enable & HOLD_LK_ENABLE) {
		UNREGISTER_TRACE_VH(android_vh_record_mutex_lock_starttime, android_vh_record_mutex_lock_starttime_handler);
		UNREGISTER_TRACE_VH(android_vh_record_rwsem_lock_starttime, android_vh_record_rwsem_lock_starttime_handler);
		UNREGISTER_TRACE_VH(android_vh_rwsem_write_finished, android_vh_rwsem_write_finished_handler);
	}

	free_percpu(p_mutex_stack_trace);
	free_percpu(p_rwsem_stack_trace);

	ml_info("finished!\n");

	return;
}

