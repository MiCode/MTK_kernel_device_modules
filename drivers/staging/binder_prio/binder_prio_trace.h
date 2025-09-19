/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM binder_prio

#if !defined(_BINDER_PRIO_TRACE_H_) || defined(TRACE_HEADER_MULTI_READ)
#define _BINDER_PRIO_TRACE_H_

#include <linux/tracepoint.h>

TRACE_EVENT(binder_prio_proc_transaction_finish,
	TP_PROTO(char *msg),

	TP_ARGS(msg),

	TP_STRUCT__entry(
		__string(msg, msg)
	),

	TP_fast_assign(
		__assign_str(msg, msg);
	),

	TP_printk("%s", __get_str(msg))
);

#endif /* _BINDER_PRIO_TRACE_H_ */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../drivers/staging/binder_prio/
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE binder_prio_trace
#include <trace/define_trace.h>
