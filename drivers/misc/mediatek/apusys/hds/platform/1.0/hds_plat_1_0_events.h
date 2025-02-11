/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM hds_plat_1_0_events
#if !defined(__HDS_PLAT_1_0_EVENTS_H__) || defined(TRACE_HEADER_MULTI_READ)
#define __HDS_PLAT_1_0_EVENTS_H__

#include <linux/tracepoint.h>

#define HDS_1_0_TAG_PMU_PRINT \
	"inf_id=0x%llx,sc_idx=%lld,inst_idx=%llu,inst_type=0x%llx,"\
	"dev_idx=0x%llx,dev_container=0x%llx,"\
	"ts_enque=0x%llx,ts_dep_solv=0x%llx,ts_disp=0x%llx,ts_irq_recv=0x%llx"

TRACE_EVENT(hds_1_0_pmu,
	TP_PROTO(uint64_t inf_id,
		int64_t sc_idx,
		uint64_t inst_idx,
		uint64_t inst_type,
		uint64_t dev_idx,
		uint64_t dev_container,
		uint64_t ts_enque,
		uint64_t ts_dep_solv,
		uint64_t ts_disp,
		uint64_t ts_irq_recv
		),
	TP_ARGS(inf_id, sc_idx,
		inst_idx, inst_type,
		dev_idx, dev_container,
		ts_enque, ts_dep_solv,
		ts_disp, ts_irq_recv
		),
	TP_STRUCT__entry(
		__field(uint64_t, inf_id)
		__field(uint64_t, sc_idx)
		__field(uint64_t, inst_idx)
		__field(uint64_t, inst_type)
		__field(uint64_t, dev_idx)
		__field(uint64_t, dev_container)
		__field(uint64_t, ts_enque)
		__field(uint64_t, ts_dep_solv)
		__field(uint64_t, ts_disp)
		__field(uint64_t, ts_irq_recv)
	),
	TP_fast_assign(
		__entry->inf_id = inf_id;
		__entry->sc_idx = sc_idx;
		__entry->inst_idx = inst_idx;
		__entry->inst_type = inst_type;
		__entry->dev_idx = dev_idx;
		__entry->dev_container = dev_container;
		__entry->ts_enque = ts_enque;
		__entry->ts_dep_solv = ts_dep_solv;
		__entry->ts_disp = ts_disp;
		__entry->ts_irq_recv = ts_irq_recv;
	),
	TP_printk(
		HDS_1_0_TAG_PMU_PRINT,
		__entry->inf_id,
		__entry->sc_idx,
		__entry->inst_idx,
		__entry->inst_type,
		__entry->dev_idx,
		__entry->dev_container,
		__entry->ts_enque,
		__entry->ts_dep_solv,
		__entry->ts_disp,
		__entry->ts_irq_recv
	)
);

#undef HDS_1_0_TAG_PMU_PRINT

#endif /* #if !defined(__HDS_PLAT_1_0_EVENTS_H__) || defined(TRACE_HEADER_MULTI_READ) */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE hds_plat_1_0_events

#include <trace/define_trace.h>
