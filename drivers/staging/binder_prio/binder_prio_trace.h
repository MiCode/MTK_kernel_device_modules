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

TRACE_EVENT(binder_prio_proc_default_prio,
			TP_PROTO(char *msg, int rt),

			TP_ARGS(msg, rt),

			TP_STRUCT__entry(
					__string(msg, msg)
					__field(int, rt)
			),

			TP_fast_assign(
					__assign_str(msg, msg);
					__entry->rt = rt;
			),

			TP_printk("%s:%d", __get_str(msg), __entry->rt)
);

TRACE_EVENT(binder_prio_restore,
			TP_PROTO(int debug_id, pid_t form_pid, pid_t form_tid, int to_pid, unsigned int sched, int prio),

			TP_ARGS(debug_id, form_pid, form_tid, to_pid, sched, prio),

			TP_STRUCT__entry(
					__field(int, debug_id)
					__field(pid_t, form_pid)
                    __field(pid_t, form_tid)
                    __field(int, to_pid)
					__field(unsigned int, sched)
                    __field(int, prio)
			),

			TP_fast_assign(
					__entry->debug_id = debug_id;
                    __entry->form_pid = form_pid;
                    __entry->form_tid = form_tid;
                    __entry->to_pid = to_pid;
                    __entry->sched = sched;
                    __entry->prio = prio;
			),

			TP_printk("t=%d form %d:%d to %d prio %d:%d", __entry->debug_id, __entry->form_pid, __entry->form_tid,
					  __entry->to_pid, __entry->sched, __entry->prio)
);

#endif /* _BINDER_PRIO_TRACE_H_ */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../drivers/staging/binder_prio/
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE binder_prio_trace
#include <trace/define_trace.h>
