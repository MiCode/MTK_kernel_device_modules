/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2025 MediaTek Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM vip_engine

#if !defined(_TRACE_VIP_ENGINE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_VIP_ENGINE_H

#include <linux/sched.h>
#include <linux/types.h>
#include <linux/tracepoint.h>
#include <vip_engine.h>

TRACE_EVENT(binder_vip_set,
	TP_PROTO(pid_t a_pid, pid_t b_pid, int a_vip_prio, unsigned int a_throttle,
		int b_vip_prio, unsigned int b_throttle),
	TP_ARGS(a_pid, b_pid, a_vip_prio, a_throttle, b_vip_prio, b_throttle),

	TP_STRUCT__entry(
		__field(pid_t, a_pid)
		__field(pid_t, b_pid)
		__field(int, a_vip_prio)
		__field(unsigned int, a_throttle)
		__field(int, b_vip_prio)
		__field(unsigned int, b_throttle)
	),
	TP_fast_assign(
		__entry->a_pid = a_pid;
		__entry->b_pid = b_pid;
		__entry->a_vip_prio = a_vip_prio;
		__entry->a_throttle = a_throttle;
		__entry->b_vip_prio = b_vip_prio;
		__entry->b_throttle = b_throttle;
	),
	TP_printk("%d -> %d: (%d, %d) -> (%d, %d)",
		__entry->a_pid,
		__entry->b_pid,
		__entry->a_vip_prio,
		__entry->a_throttle,
		__entry->b_vip_prio,
		__entry->b_throttle)
);

TRACE_EVENT(binder_vip_restore,
	TP_PROTO(pid_t b_pid, int restore_vip_prio),
	TP_ARGS(b_pid, restore_vip_prio),

	TP_STRUCT__entry(
		__field(pid_t, b_pid)
		__field(int, restore_vip_prio)
	),
	TP_fast_assign(
		__entry->b_pid = b_pid;
		__entry->restore_vip_prio = restore_vip_prio;
	),
	TP_printk("%d: restore to: %d",
		__entry->b_pid,
		__entry->restore_vip_prio)
);

TRACE_EVENT(binder_uclamp_parameters_set,
	TP_PROTO(pid_t b_pid, int max, int min),
	TP_ARGS(b_pid, max, min),

	TP_STRUCT__entry(
		__field(pid_t, b_pid)
		__field(int, max)
		__field(int, min)
	),
	TP_fast_assign(
		__entry->b_pid = b_pid;
		__entry->max = max;
		__entry->min = min;
	),
	TP_printk("%d set uclamp parameters: max=%d, min=%d",
		__entry->b_pid,
		__entry->max,
		__entry->min)
);

TRACE_EVENT(binder_uclamp_set,
	TP_PROTO(pid_t b_pid, u32 max, u32 min, int ret),
	TP_ARGS(b_pid, max, min, ret),

	TP_STRUCT__entry(
		__field(pid_t, b_pid)
		__field(u32, max)
		__field(u32, min)
		__field(int, ret)
	),
	TP_fast_assign(
		__entry->b_pid = b_pid;
		__entry->max = max;
		__entry->min = min;
		__entry->ret = ret;
	),
	TP_printk("%d set uclamp: max=%d, min=%d, ret:%d",
		__entry->b_pid,
		__entry->max,
		__entry->min,
		__entry->ret)
);

TRACE_EVENT(binder_start_uclamp_inherit,
	TP_PROTO(pid_t b_pid, int max, int min),
	TP_ARGS(b_pid, max, min),

	TP_STRUCT__entry(
		__field(pid_t, b_pid)
		__field(int, max)
		__field(int, min)
	),
	TP_fast_assign(
		__entry->b_pid = b_pid;
		__entry->max = max;
		__entry->min = min;
	),
	TP_printk("%d start uclamp inherit: max=%d, min=%d",
		__entry->b_pid,
		__entry->max,
		__entry->min)
);

TRACE_EVENT(turbo_vip,
	TP_PROTO(int avg_cpu_loading, int cpu_loading_thres, const char *vip_desc, int pid,
				const char *caller, int enf_val, u64 enf_mask),
	TP_ARGS(avg_cpu_loading, cpu_loading_thres, vip_desc, pid, caller, enf_val, enf_mask),
	TP_STRUCT__entry(
		__field(int, avg_cpu_loading)
		__field(int, cpu_loading_thres)
		__string(vip_desc, vip_desc)
		__field(int, pid)
		__string(caller, caller)
		__field(int, enf_val)
		__field(u64, enf_mask)
	),

	TP_fast_assign(
		__entry->avg_cpu_loading = avg_cpu_loading;
		__entry->cpu_loading_thres = cpu_loading_thres;
		__assign_str(vip_desc);
		__entry->pid = pid;
		__assign_str(caller);
		__entry->enf_val = enf_val;
		__entry->enf_mask = enf_mask;
	),

	TP_printk("avg_cpu_loading=%d cpu_loading_thres=%d %s tgid=%d enforce caller=%s val=%d mask=%llu",
		__entry->avg_cpu_loading,
		__entry->cpu_loading_thres,
		__get_str(vip_desc),
		__entry->pid,
		__get_str(caller),
		__entry->enf_val,
		__entry->enf_mask)
);

#endif /*_TRACE_VIP_ENGINE_H */

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE trace_vip_engine
/* This part must be outside protection */
#include <trace/define_trace.h>
