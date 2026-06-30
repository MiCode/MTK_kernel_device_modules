/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM core_ctl

#if !defined(_CORE_CTL_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _CORE_CTL_TRACE_H

#include <linux/types.h>
#include <linux/tracepoint.h>

TRACE_EVENT(core_ctl_heaviest_util,

	TP_PROTO(
		unsigned int big_cpu_ts,
		unsigned int heaviest_thres,
		unsigned int max_util,
		unsigned int rescue_big_core),

	TP_ARGS(big_cpu_ts, heaviest_thres, max_util, rescue_big_core),

	TP_STRUCT__entry(
		__field(unsigned int, big_cpu_ts)
		__field(unsigned int, heaviest_thres)
		__field(unsigned int, max_util)
		__field(unsigned int, rescue_big_core)
	),

	TP_fast_assign(
		__entry->big_cpu_ts = big_cpu_ts;
		__entry->heaviest_thres = heaviest_thres;
		__entry->max_util = max_util;
		__entry->rescue_big_core = rescue_big_core;
	),

	TP_printk("big_cpu_ts=%u heaviest_thres=%u max_util=%u resc_b=%u",
		__entry->big_cpu_ts,
		__entry->heaviest_thres,
		__entry->max_util,
		__entry->rescue_big_core)
);

TRACE_EVENT(core_ctl_algo_info,

	TP_PROTO(
		unsigned int enable_policy,
		unsigned int *need_spread_cpu,
		unsigned int *nr_assist_cpu,
		unsigned int *orig_need_cpus,
		unsigned int active_cpus,
		unsigned int *boost_by_freq,
		unsigned int *boost_by_wlan,
		unsigned int *deiso_reason),

	TP_ARGS(enable_policy, need_spread_cpu, nr_assist_cpu, orig_need_cpus,
		active_cpus, boost_by_freq, boost_by_wlan, deiso_reason),

	TP_STRUCT__entry(
		__field(unsigned int, enable_policy)
		__array(unsigned int, need_spread_cpu, 3)
		__array(unsigned int, nr_assist_cpu, 3)
		__array(unsigned int, orig_need_cpus, 3)
		__field(unsigned int, active_cpus)
		__array(unsigned int, boost_by_freq, 3)
		__array(unsigned int, boost_by_wlan, 3)
		__array(unsigned int, deiso_reason, 3)
	),

	TP_fast_assign(
		__entry->enable_policy = enable_policy;
		memcpy(__entry->need_spread_cpu, need_spread_cpu, sizeof(unsigned int)*3);
		memcpy(__entry->nr_assist_cpu, nr_assist_cpu, sizeof(unsigned int)*3);
		memcpy(__entry->orig_need_cpus, orig_need_cpus, sizeof(unsigned int)*3);
		__entry->active_cpus = active_cpus;
		memcpy(__entry->boost_by_freq, boost_by_freq, sizeof(unsigned int)*3);
		memcpy(__entry->boost_by_wlan, boost_by_wlan, sizeof(unsigned int)*3);
		memcpy(__entry->deiso_reason, deiso_reason, sizeof(unsigned int)*3);
	),

	TP_printk("en=%d spread=%u|%u|%u assist=%u|%u|%u orig_need=%u|%u|%u act=%x, bst_freq=%u|%u|%u bst_wlan=%u|%u|%u, reason=%u|%u|%u",
		__entry->enable_policy,
		__entry->need_spread_cpu[0], __entry->need_spread_cpu[1], __entry->need_spread_cpu[2],
		__entry->nr_assist_cpu[0], __entry->nr_assist_cpu[1], __entry->nr_assist_cpu[2],
		__entry->orig_need_cpus[0], __entry->orig_need_cpus[1], __entry->orig_need_cpus[2],
		__entry->active_cpus,
		__entry->boost_by_freq[0], __entry->boost_by_freq[1], __entry->boost_by_freq[2],
		__entry->boost_by_wlan[0], __entry->boost_by_wlan[1], __entry->boost_by_wlan[2],
		__entry->deiso_reason[0], __entry->deiso_reason[1], __entry->deiso_reason[2])
);

TRACE_EVENT(core_ctl_demand_eval,

		TP_PROTO(
			unsigned int cid,
			unsigned int old_need,
			unsigned int new_need,
			unsigned int active_cpus,
			unsigned int min_cpus,
			unsigned int max_cpus,
			unsigned int boost,
			unsigned int gas_enable,
			unsigned int enable,
			unsigned int cost_eff,
			unsigned int updated,
			unsigned int next_off_time),

		TP_ARGS(cid, old_need, new_need,
			active_cpus, min_cpus, max_cpus, boost,
			gas_enable, enable, cost_eff, updated, next_off_time),

		TP_STRUCT__entry(
			__field(u32, cid)
			__field(u32, old_need)
			__field(u32, new_need)
			__field(u32, active_cpus)
			__field(u32, min_cpus)
			__field(u32, max_cpus)
			__field(u32, boost)
			__field(u32, gas_enable)
			__field(u32, enable)
			__field(u32, cost_eff)
			__field(u32, updated)
			__field(u32, next_off_time)
		),

		TP_fast_assign(
			__entry->cid = cid;
			__entry->old_need = old_need;
			__entry->new_need = new_need;
			__entry->active_cpus = active_cpus;
			__entry->min_cpus = min_cpus;
			__entry->max_cpus = max_cpus;
			__entry->boost = boost;
			__entry->gas_enable = gas_enable;
			__entry->enable = enable;
			__entry->cost_eff = cost_eff;
			__entry->updated = updated;
			__entry->next_off_time = next_off_time;
		),

		TP_printk("cid=%u old=%u new=%u act=%u min=%u max=%u bst=%u gas=%u enbl=%u eff=%u update=%u next_off_time=%u",
			__entry->cid,
			__entry->old_need,
			__entry->new_need,
			__entry->active_cpus,
			__entry->min_cpus,
			__entry->max_cpus,
			__entry->boost,
			__entry->gas_enable,
			__entry->enable,
			__entry->cost_eff,
			__entry->updated,
			__entry->next_off_time)
);

TRACE_EVENT(core_ctl_nr_over_thres,

	TP_PROTO(
		unsigned int *cls_nr_up,
		unsigned int *cls_nr_down,
		unsigned int *nr_up,
		unsigned int *nr_down,
		unsigned int *prod_up,
		unsigned int *prod_down),

	TP_ARGS(cls_nr_up, cls_nr_down, nr_up, nr_down, prod_up, prod_down),

	TP_STRUCT__entry(
		__array(unsigned int, cls_nr_up, 3)
		__array(unsigned int, cls_nr_down, 3)
		__array(unsigned int, nr_up, 3)
		__array(unsigned int, nr_down, 3)
		__array(unsigned int, prod_up, 3)
		__array(unsigned int, prod_down, 3)
	),

	TP_fast_assign(
		memcpy(__entry->cls_nr_up, cls_nr_up, sizeof(unsigned int) * 3);
		memcpy(__entry->cls_nr_down, cls_nr_down, sizeof(unsigned int) * 3);
		memcpy(__entry->nr_up, nr_up, sizeof(unsigned int) * 3);
		memcpy(__entry->nr_down, nr_down, sizeof(unsigned int) * 3);
		memcpy(__entry->prod_up, prod_up, sizeof(unsigned int) * 3);
		memcpy(__entry->prod_down, prod_down, sizeof(unsigned int) * 3);
	),

	TP_printk("cls_nr_up/dn=%u|%u|%u %u|%u|%u nr_up/dn=%u|%u|%u %u|%u|%u prod_up/dn=%u|%u|%u %u|%u|%u",
		__entry->cls_nr_up[0], __entry->cls_nr_up[1], __entry->cls_nr_up[2],
		__entry->cls_nr_down[0], __entry->cls_nr_down[1], __entry->cls_nr_down[2],
		__entry->nr_up[0], __entry->nr_up[1], __entry->nr_up[2],
		__entry->nr_down[0], __entry->nr_down[1], __entry->nr_down[2], 
		__entry->prod_up[0], __entry->prod_up[1], __entry->prod_up[2], 
		__entry->prod_down[0], __entry->prod_down[1], __entry->prod_down[2])

);

TRACE_EVENT(core_ctl_call_notifier,

	TP_PROTO(
		unsigned int cpu,
		unsigned int is_pause,
		unsigned int online_mask,
		unsigned int paused_mask),
	TP_ARGS(cpu, is_pause, online_mask, paused_mask),

	TP_STRUCT__entry(
		__field(unsigned int, cpu)
		__field(unsigned int, is_pause)
		__field(unsigned int, online_mask)
		__field(unsigned int, paused_mask)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->is_pause = is_pause;
		__entry->online_mask = online_mask;
		__entry->paused_mask = paused_mask;
	),

	TP_printk("cpu:%d, is_pause:%d, online_mask=0x%x, paused_mask=0x%x",
		__entry->cpu, __entry->is_pause, __entry->online_mask, __entry->paused_mask)
);

TRACE_EVENT(core_ctl_cpu_loading_util,

	TP_PROTO(
		unsigned int *active_loading,
		unsigned int *cpu_util),

	TP_ARGS(active_loading, cpu_util),

	TP_STRUCT__entry(
		__array(unsigned int, active_loading, 8)
		__array(unsigned int, cpu_util, 8)
	),

	TP_fast_assign(
		memcpy(__entry->active_loading, active_loading, sizeof(unsigned int) * 8);
		memcpy(__entry->cpu_util, cpu_util, sizeof(unsigned int) * 8);
	),

	TP_printk("loading=%d|%d|%d|%d|%d|%d|%d|%d util=%d|%d|%d|%d|%d|%d|%d|%d",
		__entry->active_loading[0], __entry->active_loading[1], __entry->active_loading[2],
		__entry->active_loading[3], __entry->active_loading[4], __entry->active_loading[5],
		__entry->active_loading[6], __entry->active_loading[7], __entry->cpu_util[0],
		__entry->cpu_util[1], __entry->cpu_util[2], __entry->cpu_util[3],
		__entry->cpu_util[4], __entry->cpu_util[5], __entry->cpu_util[6],
		__entry->cpu_util[7])
);

TRACE_EVENT(core_ctl_busy_cpus,

	TP_PROTO(
		unsigned int *busy_state,
		unsigned int *max_nr_state,
		unsigned int *max_rt_nr_state,
		unsigned int *max_vip_nr_state,
		unsigned int consider_VIP),

	TP_ARGS(busy_state, max_nr_state, max_rt_nr_state, max_vip_nr_state, consider_VIP),

	TP_STRUCT__entry(
		__array(unsigned int, busy_state, 8)
		__array(unsigned int, max_nr_state, 8)
		__array(unsigned int, max_rt_nr_state, 8)
		__array(unsigned int, max_vip_nr_state, 8)
		__field(unsigned int, consider_VIP)
	),

	TP_fast_assign(
		memcpy(__entry->busy_state, busy_state, sizeof(unsigned int) * 8);
		memcpy(__entry->max_nr_state, max_nr_state, sizeof(unsigned int) * 8);
		memcpy(__entry->max_rt_nr_state, max_rt_nr_state, sizeof(unsigned int) * 8);
		memcpy(__entry->max_vip_nr_state, max_vip_nr_state, sizeof(unsigned int) * 8);
		__entry->consider_VIP = consider_VIP;
	),

	TP_printk("busy=%d|%d|%d|%d|%d|%d|%d|%d max_nr=%d|%d|%d|%d|%d|%d|%d|%d max_rt_nr=%d|%d|%d|%d|%d|%d|%d|%d max_vip_nr=%d|%d|%d|%d|%d|%d|%d|%d con_vip=%d",
		__entry->busy_state[0], __entry->busy_state[1], __entry->busy_state[2],
		__entry->busy_state[3], __entry->busy_state[4], __entry->busy_state[5],
		__entry->busy_state[6], __entry->busy_state[7], __entry->max_nr_state[0],
		__entry->max_nr_state[1], __entry->max_nr_state[2], __entry->max_nr_state[3],
		__entry->max_nr_state[4], __entry->max_nr_state[5], __entry->max_nr_state[6],
		__entry->max_nr_state[7], __entry->max_rt_nr_state[0], __entry->max_rt_nr_state[1],
		__entry->max_rt_nr_state[2], __entry->max_rt_nr_state[3], __entry->max_rt_nr_state[4],
		__entry->max_rt_nr_state[5], __entry->max_rt_nr_state[6], __entry->max_rt_nr_state[7],
		__entry->max_vip_nr_state[0], __entry->max_vip_nr_state[1], __entry->max_vip_nr_state[2],
		__entry->max_vip_nr_state[3], __entry->max_vip_nr_state[4], __entry->max_vip_nr_state[5],
		__entry->max_vip_nr_state[6], __entry->max_vip_nr_state[7], __entry->consider_VIP)
);

TRACE_EVENT(core_ctl_cpu_request,

	TP_PROTO(
		unsigned int cid,
		unsigned int min_cpus,
		unsigned int max_cpus,
		unsigned int *have_demand,
		unsigned int *min_cpus_req,
		unsigned int *max_cpus_req,
		unsigned int *force_pause_req),

	TP_ARGS(cid, min_cpus, max_cpus, have_demand, min_cpus_req, max_cpus_req, force_pause_req),

	TP_STRUCT__entry(
		__field(unsigned int, cid)
		__field(unsigned int, min_cpus)
		__field(unsigned int, max_cpus)
		__array(unsigned int, have_demand, 5)
		__array(unsigned int, min_cpus_req, 5)
		__array(unsigned int, max_cpus_req, 5)
		__array(unsigned int, force_pause_req, 8)
	),

	TP_fast_assign(
		__entry->cid = cid;
		__entry->min_cpus = min_cpus;
		__entry->max_cpus = max_cpus;
		memcpy(__entry->have_demand, have_demand, sizeof(unsigned int) * 5);
		memcpy(__entry->min_cpus_req, min_cpus_req, sizeof(unsigned int) * 5);
		memcpy(__entry->max_cpus_req, max_cpus_req, sizeof(unsigned int) * 5);
		memcpy(__entry->force_pause_req, force_pause_req, sizeof(unsigned int) * 8);
	),

	TP_printk("cid=%d, min/max=%d|%d demand=%d|%d|%d|%d|%d min_rq=%d|%d|%d|%d|%d max_rq=%d|%d|%d|%d|%d fc_rq=%d|%d|%d|%d|%d|%d|%d|%d",
		__entry->cid, __entry->min_cpus, __entry->max_cpus,
		__entry->have_demand[0], __entry->have_demand[1], __entry->have_demand[2],
		__entry->have_demand[3], __entry->have_demand[4],
		__entry->min_cpus_req[0], __entry->min_cpus_req[1], __entry->min_cpus_req[2],
		__entry->min_cpus_req[3], __entry->min_cpus_req[4],
		__entry->max_cpus_req[0], __entry->max_cpus_req[1], __entry->max_cpus_req[2],
		__entry->max_cpus_req[3], __entry->max_cpus_req[4],
		__entry->force_pause_req[0], __entry->force_pause_req[1], __entry->force_pause_req[2],
		__entry->force_pause_req[3], __entry->force_pause_req[4], __entry->force_pause_req[5],
		__entry->force_pause_req[6], __entry->force_pause_req[7])
);

#endif /*_CORE_CTL_TRACE_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH core_ctl
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE core_ctl_trace
/* This part must be outside protection */
#include <trace/define_trace.h>
