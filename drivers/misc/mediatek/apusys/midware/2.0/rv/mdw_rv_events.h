/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mdw_rv_events
#if !defined(__MDW_RV_EVENTS_H__) || defined(TRACE_HEADER_MULTI_READ)
#define __MDW_RV_EVENTS_H__
#include <linux/tracepoint.h>

#define MDW_TAG_CMD_PRINT \
	"%u,pid=%d,uid=0x%llx,rvid=0x%llx,"\
	"num_subcmds=%u,priority=%u,"\
	"softlimit=%u,pwr_dtime=%u,sc_rets=0x%llx,"\
	"pwr_plcy=%x,tolerance=%x,start_ts=0x%llx"\

#define MDW_TAG_SUBCMD_PRINT \
	"%u,rvid=0x%llx,"\
	"%u,type=%u,ipstart_ts=0x%x,ipend_ts=0x%x,"\
	"was_preempted=0x%x,executed_core_bmp=0x%x,"\
	"tcm_usage=0x%x,history_iptime=%u"\

TRACE_EVENT(mdw_rv_cmd,
	TP_PROTO(uint32_t status,
		pid_t pid,
		uint64_t uid,
		uint64_t rvid,
		uint32_t num_subcmds,
		uint32_t priority,
		uint32_t softlimit,
		uint32_t pwr_dtime,
		uint64_t sc_rets,
		uint32_t pwr_plcy,
		uint32_t tolerance,
		uint64_t start_ts
		),
	TP_ARGS(status, pid, uid, rvid,
		num_subcmds, priority,
		softlimit, pwr_dtime,
		sc_rets, pwr_plcy, tolerance, start_ts
		),
	TP_STRUCT__entry(
		__field(uint32_t, status)
		__field(pid_t, pid)
		__field(uint64_t, uid)
		__field(uint64_t, rvid)
		__field(uint32_t, num_subcmds)
		__field(uint32_t, priority)
		__field(uint32_t, softlimit)
		__field(uint32_t, pwr_dtime)
		__field(uint64_t, sc_rets)
		__field(uint32_t, pwr_plcy)
		__field(uint32_t, tolerance)
		__field(uint64_t, start_ts)
	),
	TP_fast_assign(
		__entry->status = status;
		__entry->pid = pid;
		__entry->uid = uid;
		__entry->rvid = rvid;
		__entry->num_subcmds = num_subcmds;
		__entry->priority = priority;
		__entry->softlimit = softlimit;
		__entry->pwr_dtime = pwr_dtime;
		__entry->sc_rets = sc_rets;
		__entry->pwr_dtime = pwr_plcy;
		__entry->pwr_dtime = tolerance;
		__entry->pwr_dtime = start_ts;
	),
	TP_printk(
		MDW_TAG_CMD_PRINT,
		__entry->status,
		__entry->pid,
		__entry->uid,
		__entry->rvid,
		__entry->num_subcmds,
		__entry->priority,
		__entry->softlimit,
		__entry->pwr_dtime,
		__entry->sc_rets,
		__entry->pwr_plcy,
		__entry->tolerance,
		__entry->start_ts
	)
);

TRACE_EVENT(mdw_rv_subcmd,
	TP_PROTO(uint32_t status,
		uint64_t rvid,
		uint32_t sc_type,
		uint32_t sc_idx,
		uint32_t ipstart_ts,
		uint32_t ipend_ts,
		uint32_t was_preempted,
		uint32_t executed_core_bmp,
		uint32_t tcm_usage,
		uint32_t history_iptime
		),
	TP_ARGS(status, rvid, sc_type, sc_idx,
		ipstart_ts, ipend_ts,
		was_preempted, executed_core_bmp,
		tcm_usage, history_iptime
		),
	TP_STRUCT__entry(
		__field(uint32_t, status)
		__field(uint64_t, rvid)
		__field(uint32_t, sc_type)
		__field(uint32_t, sc_idx)
		__field(uint32_t, ipstart_ts)
		__field(uint32_t, ipend_ts)
		__field(uint32_t, was_preempted)
		__field(uint32_t, executed_core_bmp)
		__field(uint32_t, tcm_usage)
		__field(uint32_t, history_iptime)
	),
	TP_fast_assign(
		__entry->status = status;
		__entry->rvid = rvid;
		__entry->sc_type= sc_type;
		__entry->sc_idx= sc_idx;
		__entry->ipstart_ts = ipstart_ts;
		__entry->ipend_ts = ipend_ts;
		__entry->was_preempted = was_preempted;
		__entry->executed_core_bmp = executed_core_bmp;
		__entry->tcm_usage = tcm_usage;
		__entry->history_iptime = history_iptime;
	),
	TP_printk(
		MDW_TAG_SUBCMD_PRINT,
		__entry->status,
		__entry->rvid,
		__entry->sc_type,
		__entry->sc_idx,
		__entry->ipstart_ts,
		__entry->ipend_ts,
		__entry->was_preempted,
		__entry->executed_core_bmp,
		__entry->tcm_usage,
		__entry->history_iptime
	)
);

#undef MDW_TAG_CMD_PRINT
#undef MDW_TAG_SUBCMD_PRINT

#endif /* #if !defined(__MDW_RV_EVENTS_H__) || defined(TRACE_HEADER_MULTI_READ) */


/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mdw_rv_events
#include <trace/define_trace.h>

