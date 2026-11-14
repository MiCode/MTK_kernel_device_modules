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

TRACE_EVENT(set_pf_ctrl_enable,
	TP_PROTO(int pf_ctrl_enable, bool enable, unsigned int user, char *caller),
	TP_ARGS(pf_ctrl_enable, enable, user, caller),

	TP_STRUCT__entry(
		__field(int, pf_ctrl_enable)
		__field(bool, enable)
		__field(unsigned int, user)
		__string(caller, caller)
	),
	TP_fast_assign(
		__entry->pf_ctrl_enable = pf_ctrl_enable;
		__entry->enable = enable;
		__entry->user = user;
		__assign_str(caller);
	),
	TP_printk("pf_ctrl_enable=%d enable=%d user=%u caller=%s",
		__entry->pf_ctrl_enable,
		__entry->enable,
		__entry->user,
		__get_str(caller))
);

#endif /* _PF_CTRL_TRACE_H */

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE pf_ctrl_trace
/* This part must be outside protection */
#include <trace/define_trace.h>
