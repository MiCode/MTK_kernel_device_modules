/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM scheduler

#if !defined(_TRACE_COMMON_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_COMMON_H
#include <linux/string.h>
#include <linux/types.h>
#include <linux/tracepoint.h>
#include <linux/compat.h>

TRACE_EVENT(sched_runnable_boost,
	TP_PROTO(bool is_runnable_boost_enable, int boost, unsigned long rq_util_avg,
		unsigned long rq_util_est, unsigned long rq_load, unsigned long cpu_util_next),
	TP_ARGS(is_runnable_boost_enable, boost, rq_util_avg, rq_util_est, rq_load, cpu_util_next),
	TP_STRUCT__entry(
		__field(bool, is_runnable_boost_enable)
		__field(int, boost)
		__field(unsigned long, rq_util_avg)
		__field(unsigned long, rq_util_est)
		__field(unsigned long, rq_load)
		__field(unsigned long, cpu_util_next)
	),
	TP_fast_assign(
		__entry->is_runnable_boost_enable = is_runnable_boost_enable;
		__entry->boost = boost;
		__entry->rq_util_avg = rq_util_avg;
		__entry->rq_util_est = rq_util_est;
		__entry->rq_load = rq_load;
		__entry->cpu_util_next = cpu_util_next;
	),
	TP_printk("is_runnable_boost_enable=%d boost=%d rq_util_avg=%lu rq_util_est=%lu rq_load=%lu cpu_util_next=%lu",
		__entry->is_runnable_boost_enable,
		__entry->boost,
		__entry->rq_util_avg,
		__entry->rq_util_est,
		__entry->rq_load,
		__entry->cpu_util_next
	)
);

#endif /* _TRACE_COMMON_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE common_trace
/* This part must be outside protection */
#include <trace/define_trace.h>
