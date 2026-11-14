/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM game

#if !defined(_MTK_GAME_TRACE_EVENT_H_) || defined(TRACE_HEADER_MULTI_READ)
#define _MTK_GAME_TRACE_EVENT_H_
#include <linux/tracepoint.h>

TRACE_EVENT(game_main_trace,
	TP_PROTO(
	const char *buf
	),

	TP_ARGS(buf),

	TP_STRUCT__entry(
	__string(buf, buf)
	),

	TP_fast_assign(
	__assign_str(buf);
	),

	TP_printk("%s",
	__get_str(buf)
	)
);

TRACE_EVENT(loom_main_trace,
	TP_PROTO(
	const char *buf
	),

	TP_ARGS(buf),

	TP_STRUCT__entry(
	__string(buf, buf)
	),

	TP_fast_assign(
	__assign_str(buf);
	),

	TP_printk("%s",
	__get_str(buf)
	)
);

#endif /* _MTK_GAME_TRACE_EVENT_H_ */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE game_trace_event
#include <trace/define_trace.h>
