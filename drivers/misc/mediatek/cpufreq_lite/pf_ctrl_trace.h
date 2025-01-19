// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM cpufreq_lite

#if !defined(_PF_CTRL_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _PF_CTRL_TRACE_H

#include <linux/types.h>
#include <linux/tracepoint.h>

TRACE_EVENT(trigger_pf_work,
	TP_PROTO(int type, u64 pf_off_total_time),
	TP_ARGS(type, pf_off_total_time),

	TP_STRUCT__entry(
		__field(int, type)
		__field(u64, pf_off_total_time)
	),
	TP_fast_assign(
		__entry->type = type;
		__entry->pf_off_total_time = pf_off_total_time;
	),
	TP_printk("type=%d pf_off_total_time=%llu",
		__entry->type,
		__entry->pf_off_total_time)
);

#endif /* _PF_CTRL_TRACE_H */

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE pf_ctrl_trace
/* This part must be outside protection */
#include <trace/define_trace.h>
