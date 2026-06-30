/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM mmmc_events

#if !defined(_TRACE_MMMC_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MMMC_EVENTS_H

#include <linux/tracepoint.h>

TRACE_EVENT(mmmc__axi_mon_bwl_threshold,
	TP_PROTO(const char *r_w_type, int axi_id,
			int threshold),
	TP_ARGS(r_w_type, axi_id, threshold),
	TP_STRUCT__entry(
		__string(r_w_type, r_w_type)
		__field(int, axi_id)
		__field(int, threshold)
	),
	TP_fast_assign(
		__assign_str(r_w_type);
		__entry->axi_id = axi_id;
		__entry->threshold = threshold;
	),
	TP_printk("axi%d_%s_threshold=%d",
		(int)__entry->axi_id,
		__get_str(r_w_type),
		(int)__entry->threshold)
);

TRACE_EVENT(mmmc__axi_mon_bwl_budget,
	TP_PROTO(const char *r_w_type, int axi_id,
			int budget),
	TP_ARGS(r_w_type, axi_id, budget),
	TP_STRUCT__entry(
		__string(r_w_type, r_w_type)
		__field(int, axi_id)
		__field(int, budget)
	),
	TP_fast_assign(
		__assign_str(r_w_type);
		__entry->axi_id = axi_id;
		__entry->budget = budget;
	),
	TP_printk("axi%d_%s_budget=%d",
		(int)__entry->axi_id,
		__get_str(r_w_type),
		(int)__entry->budget)
);

TRACE_EVENT(mmmc__axi_mon_ostdbl,
	TP_PROTO(const char *r_w_type, int ostdbl,
			int axi_id),
	TP_ARGS(r_w_type, ostdbl, axi_id),
	TP_STRUCT__entry(
		__string(r_w_type, r_w_type)
		__field(int, ostdbl)
		__field(int, axi_id)
	),
	TP_fast_assign(
		__assign_str(r_w_type);
		__entry->ostdbl = ostdbl;
		__entry->axi_id = axi_id;
	),
	TP_printk("axi:%d_%s_ostdbl=%d",
		(int)__entry->axi_id,
		__get_str(r_w_type),
		(int)__entry->ostdbl)
);

#endif /* _TRACE_MMMC_EVENTS_H */

#undef TRACE_INCLUDE_FILE
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE mmmc_events

/* This part must be outside protection */
#include <trace/define_trace.h>
