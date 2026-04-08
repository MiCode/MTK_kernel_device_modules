/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM cm_mgr_events

#if !defined(_TRACE_MTK_CM_MGR_EVENTS_MT8788_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MTK_CM_MGR_EVENTS_MT8788_H

#include <linux/tracepoint.h>

TRACE_EVENT(CM_MGR__stall_ratio_0,
	TP_PROTO(unsigned int ratio_ratio),
	TP_ARGS(ratio_ratio),
	TP_STRUCT__entry(
		__field(unsigned int, ratio_ratio)
	),
	TP_fast_assign(
		__entry->ratio_ratio = ratio_ratio;
	),
	TP_printk("mcucfg_reg__MP0_CPU_AVG_STALL_RATIO=%d",
		__entry->ratio_ratio)
);

TRACE_EVENT(CM_MGR__stall_ratio_1,
	TP_PROTO(unsigned int ratio_ratio),
	TP_ARGS(ratio_ratio),
	TP_STRUCT__entry(
		__field(unsigned int, ratio_ratio)
	),
	TP_fast_assign(
		__entry->ratio_ratio = ratio_ratio;
	),
	TP_printk("ca57a_12ffc_config__CPU_AVG_STALL_RATIO=%d",
		__entry->ratio_ratio)
);

TRACE_EVENT(CM_MGR__dvfsrc_set_power_model_ddr_request,
	TP_PROTO(int dram_level),
	TP_ARGS(dram_level),
	TP_STRUCT__entry(
		__field(int, dram_level)
	),
	TP_fast_assign(
		__entry->dram_level = dram_level;
	),
	TP_printk("dram_level=%d",
		__entry->dram_level)
);

#endif /* _TRACE_MTK_CM_MGR_EVENTS_MT8788_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE mtk_cm_mgr_platform_events_mt8788
#include <trace/define_trace.h>


