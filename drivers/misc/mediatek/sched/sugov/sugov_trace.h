/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM scheduler

#if !defined(_TRACE_SUGOV_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SUGOV_H
#include <linux/string.h>
#include <linux/types.h>
#include <linux/tracepoint.h>

TRACE_EVENT(sched_runnable_boost,
	TP_PROTO(bool is_runnable_boost_enable, int boost, unsigned long rq_util_avg,
		unsigned long rq_util_est, unsigned long rq_load, unsigned long cpu_util_next),
	TP_ARGS(is_runnable_boost_enable, boost, rq_util_avg, rq_util_est, rq_load, cpu_util_next),
	TP_STRUCT__entry(
		__field(bool, is_runnable_boost_enable)
		__field(int, boost)
		__field(unsigned long, rq_util_avg)
		__field(unsigned long, rq_util_est)
		__field(unsigned long, rq_load)
		__field(unsigned long, cpu_util_next)
	),
	TP_fast_assign(
		__entry->is_runnable_boost_enable = is_runnable_boost_enable;
		__entry->boost = boost;
		__entry->rq_util_avg = rq_util_avg;
		__entry->rq_util_est = rq_util_est;
		__entry->rq_load = rq_load;
		__entry->cpu_util_next = cpu_util_next;
	),
	TP_printk("is_runnable_boost_enable=%d boost=%d rq_util_avg=%lu rq_util_est=%lu rq_load=%lu cpu_util_next=%lu",
		__entry->is_runnable_boost_enable,
		__entry->boost,
		__entry->rq_util_avg,
		__entry->rq_util_est,
		__entry->rq_load,
		__entry->cpu_util_next
	)
);

TRACE_EVENT(sugov_ext_act_sbb,
	TP_PROTO(int flag, int pid,
		int set, int success, int gear_id,
		int sbb_active_ratio),
	TP_ARGS(flag, pid, set, success, gear_id, sbb_active_ratio),
	TP_STRUCT__entry(
		__field(int, flag)
		__field(int, pid)
		__field(int, set)
		__field(int, success)
		__field(int, gear_id)
		__field(int, sbb_active_ratio)
	),
	TP_fast_assign(
		__entry->flag = flag;
		__entry->pid = pid;
		__entry->set = set;
		__entry->success = success;
		__entry->gear_id = gear_id;
		__entry->sbb_active_ratio = sbb_active_ratio;
	),
	TP_printk(
		"flag=%d pid=%d set=%d success=%d gear_id=%d, sbb_active_ratio=%d",
		__entry->flag,
		__entry->pid,
		__entry->set,
		__entry->success,
		__entry->gear_id,
		__entry->sbb_active_ratio)
);

TRACE_EVENT(sugov_ext_curr_uclamp,
	TP_PROTO(int cpu, int pid,
		int util_ori, int util, int u_min,
		int u_max),
	TP_ARGS(cpu, pid, util_ori, util, u_min, u_max),
	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned int, pid)
		__field(unsigned int, util_ori)
		__field(unsigned int, util)
		__field(unsigned int, u_min)
		__field(unsigned int, u_max)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->pid = pid;
		__entry->util_ori = util_ori;
		__entry->util = util;
		__entry->u_min = u_min;
		__entry->u_max = u_max;
	),
	TP_printk(
		"cpu=%d pid=%d util_ori=%d util_eff=%d u_min=%d, u_max=%d",
		__entry->cpu,
		__entry->pid,
		__entry->util_ori,
		__entry->util,
		__entry->u_min,
		__entry->u_max)
);

TRACE_EVENT(sugov_ext_gear_uclamp,
	TP_PROTO(int cpu, int util_ori,
		int umin, int umax, int util,
		int umax_gear),
	TP_ARGS(cpu, util_ori, umin, umax, util, umax_gear),
	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned int, util_ori)
		__field(unsigned int, umin)
		__field(unsigned int, umax)
		__field(unsigned int, util)
		__field(unsigned int, umax_gear)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->util_ori = util_ori;
		__entry->umin = umin;
		__entry->umax = umax;
		__entry->util = util;
		__entry->umax_gear = umax_gear;
	),
	TP_printk(
		"cpu=%d util_ori=%d umin=%d umax=%d util_ret=%d, umax_gear=%d",
		__entry->cpu,
		__entry->util_ori,
		__entry->umin,
		__entry->umax,
		__entry->util,
		__entry->umax_gear)
);

TRACE_EVENT(sched_cpu_util,

	TP_PROTO(int cpu, int cpu_util_tal, int cpu_util_clp, int cpu_util_mgn,
		int cpu_util_eff, int cpu_util_cfs,
		unsigned long *min, unsigned long *max, int dst_cpu, struct task_struct *p,
		int source, unsigned long cpu_util_iowait),

	TP_ARGS(cpu, cpu_util_tal, cpu_util_clp, cpu_util_mgn, cpu_util_eff, cpu_util_cfs,
		min, max, dst_cpu, p, source, cpu_util_iowait),

	TP_STRUCT__entry(
		__field(int, source)
		__field(int, cpu)
		__field(int, cpu_util_tal)
		__field(int, cpu_util_clp)
		__field(int, cpu_util_mgn)
		__field(int, cpu_util_eff)
		__field(unsigned long, cpu_util_iowait)
		__field(int, cpu_util_cfs)
		__field(int, min)
		__field(int, max)
		__field(int, dst_cpu)
		__field(int, pid)
		),

	TP_fast_assign(
		__entry->source          = source;
		__entry->cpu             = cpu;
		__entry->cpu_util_tal    = cpu_util_tal;
		__entry->cpu_util_clp    = cpu_util_clp;
		__entry->cpu_util_mgn    = cpu_util_mgn;
		__entry->cpu_util_eff    = cpu_util_eff;
		__entry->cpu_util_iowait = cpu_util_iowait;
		__entry->cpu_util_cfs    = cpu_util_cfs;
		__entry->min             = min ? (int)*min : -1;
		__entry->max             = max ? (int)*max : -1;
		__entry->dst_cpu         = dst_cpu;
		__entry->pid             = p ? p->pid : -1;
		),

	TP_printk("cpu=%1d cpu_util_tal=%4d cpu_util_clp=%4d cpu_util_mgn=%4d cpu_util_eff=%4d cpu_util_iowait=%4lu cpu_util_cfs=%4d rq_min_clamp=%4d rq_max_clamp=%4d dst_cpu=%1d pid=%5d source=%2d",
		__entry->cpu,
		__entry->cpu_util_tal,
		__entry->cpu_util_clp,
		__entry->cpu_util_mgn,
		__entry->cpu_util_eff,
		__entry->cpu_util_iowait,
		__entry->cpu_util_cfs,
		__entry->min,
		__entry->max,
		__entry->dst_cpu,
		__entry->pid,
		__entry->source
		)
);

TRACE_EVENT(sugov_ext_util,
	TP_PROTO(int cpu, unsigned long util,
		unsigned int min, unsigned int max, int idle),
	TP_ARGS(cpu, util, min, max, idle),
	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned long, util)
		__field(unsigned int, min)
		__field(unsigned int, max)
		__field(int, idle)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->util = util;
		__entry->min = min;
		__entry->max = max;
		__entry->idle = idle;
	),
	TP_printk(
		"cpu=%d util=%lu min=%u max=%u ignore_idle_util=%d",
		__entry->cpu,
		__entry->util,
		__entry->min,
		__entry->max,
		__entry->idle)
);

TRACE_EVENT(sugov_ext_util_freq,
	TP_PROTO(int cpu, unsigned long util, unsigned int freq),

	TP_ARGS(cpu, util, freq),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned long, util)
		__field(unsigned int, freq)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->util = util;
		__entry->freq = freq;
	),

	TP_printk(
		"cpu=%d util_with_uclamp=%lu cpu_freq=%d",
		__entry->cpu,
		__entry->util,
		__entry->freq)
);


TRACE_EVENT(sugov_ext_wl,
	TP_PROTO(unsigned int gear_id, unsigned int cpu, int wl_tcm,
		int wl_cpu_curr, int wl_cpu_delay, int wl_cpu_manual, int wl_dsu_curr,
		int wl_dsu_delay,int wl_dsu_manual),
	TP_ARGS(gear_id, cpu, wl_tcm, wl_cpu_curr, wl_cpu_delay, wl_cpu_manual, wl_dsu_curr,
		wl_dsu_delay, wl_dsu_manual),
	TP_STRUCT__entry(
		__field(unsigned int, gear_id)
		__field(unsigned int, cpu)
		__field(int, wl_tcm)
		__field(int, wl_cpu_curr)
		__field(int, wl_cpu_delay)
		__field(int, wl_cpu_manual)
		__field(int, wl_dsu_curr)
		__field(int, wl_dsu_delay)
		__field(int, wl_dsu_manual)
	),
	TP_fast_assign(
		__entry->gear_id = gear_id;
		__entry->cpu = cpu;
		__entry->wl_tcm= wl_tcm;
		__entry->wl_cpu_curr = wl_cpu_curr;
		__entry->wl_cpu_delay = wl_cpu_delay;
		__entry->wl_cpu_manual = wl_cpu_manual;
		__entry->wl_dsu_curr = wl_dsu_curr;
		__entry->wl_dsu_delay = wl_dsu_delay;
		__entry->wl_dsu_manual = wl_dsu_manual;
	),
	TP_printk(
		"gear_id=%u cpu=%u wl_tcm=%d wl_cpu_curr=%d wl_cpu_delay=%d wl_cpu_manual=%d wl_dsu_curr=%d wl_dsu_delay=%d wl_dsu_manual=%d",
		__entry->gear_id,
		__entry->cpu,
		__entry->wl_tcm,
		__entry->wl_cpu_curr,
		__entry->wl_cpu_delay,
		__entry->wl_cpu_manual,
		__entry->wl_dsu_curr,
		__entry->wl_dsu_delay,
		__entry->wl_dsu_manual)
);

TRACE_EVENT(sugov_ext_dsu_freq_vote,
	TP_PROTO(unsigned int wl, unsigned int gear_id, bool dsu_idle_ctrl,
		unsigned int cpu_freq, unsigned int dsu_freq_vote, unsigned int freq_thermal,
                bool dsu_fine_ctrl_enabled, bool dsu_fine_crtl, unsigned int dsu_fine_val_pct),
	TP_ARGS(wl, gear_id, dsu_idle_ctrl, cpu_freq, dsu_freq_vote, freq_thermal,
                dsu_fine_ctrl_enabled, dsu_fine_crtl, dsu_fine_val_pct),
	TP_STRUCT__entry(
		__field(unsigned int, wl)
		__field(unsigned int, gear_id)
		__field(bool, dsu_idle_ctrl)
		__field(unsigned int, cpu_freq)
		__field(unsigned int, dsu_freq_vote)
		__field(unsigned int, freq_thermal)
		__field(bool, dsu_fine_ctrl_enabled)
		__field(bool, dsu_fine_crtl)
		__field(unsigned int, dsu_fine_val_pct)
		__field(unsigned int, cpu_div_dsu_freq)
	),
	TP_fast_assign(
		__entry->wl = wl;
		__entry->gear_id = gear_id;
		__entry->dsu_idle_ctrl = dsu_idle_ctrl;
		__entry->cpu_freq = cpu_freq;
		__entry->dsu_freq_vote = dsu_freq_vote;
		__entry->freq_thermal = freq_thermal;
		__entry->dsu_fine_ctrl_enabled = dsu_fine_ctrl_enabled;
		__entry->dsu_fine_crtl = dsu_fine_crtl;
		__entry->dsu_fine_val_pct = dsu_fine_val_pct;
		__entry->cpu_div_dsu_freq = dsu_freq_vote * 100 / cpu_freq ;
	),
	TP_printk(
		"wl=%u gear_id=%u dsu_idle_ctrl=%d cpu_freq=%u dsu_freq_vote=%u freq_thermal=%u dsu_fine_ctrl_enabled=%d dsu_fine_crtl=%d, dsu_fine_val_pct=%d, cpu_div_dsu_freq=%d",
		__entry->wl,
		__entry->gear_id,
		__entry->dsu_idle_ctrl,
		__entry->cpu_freq,
		__entry->dsu_freq_vote,
		__entry->freq_thermal,
		__entry->dsu_fine_ctrl_enabled,
		__entry->dsu_fine_crtl,
		__entry->dsu_fine_val_pct,
		__entry->cpu_div_dsu_freq)
);

TRACE_EVENT(sugov_ext_gear_state,
	TP_PROTO(unsigned int gear_id, unsigned int state),
	TP_ARGS(gear_id, state),
	TP_STRUCT__entry(
		__field(unsigned int, gear_id)
		__field(unsigned int, state)
	),
	TP_fast_assign(
		__entry->gear_id = gear_id;
		__entry->state = state;
	),
	TP_printk(
		"gear_id=%u state=%u",
		__entry->gear_id,
		__entry->state)
);

TRACE_EVENT(sugov_ext_sbb,
	TP_PROTO(int cpu, int pid, unsigned int boost,
		unsigned int util, unsigned int util_boost, unsigned int active_ratio,
		unsigned int threshold),
	TP_ARGS(cpu, pid, boost, util, util_boost, active_ratio, threshold),
	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, pid)
		__field(unsigned int, boost)
		__field(unsigned int, util)
		__field(unsigned int, util_boost)
		__field(unsigned int, active_ratio)
		__field(unsigned int, threshold)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->pid = pid;
		__entry->boost = boost;
		__entry->util = util;
		__entry->util_boost = util_boost;
		__entry->active_ratio = active_ratio;
		__entry->threshold = threshold;
	),
	TP_printk(
		"cpu=%d pid=%d boost=%d util=%d util_boost=%d active_ratio=%d, threshold=%d",
		__entry->cpu,
		__entry->pid,
		__entry->boost,
		__entry->util,
		__entry->util_boost,
		__entry->active_ratio,
		__entry->threshold)
);

TRACE_EVENT(sugov_ext_adaptive_margin,
	TP_PROTO(unsigned int gear_id, unsigned int margin, unsigned int ratio),
	TP_ARGS(gear_id, margin, ratio),
	TP_STRUCT__entry(
		__field(unsigned int, gear_id)
		__field(unsigned int, margin)
		__field(unsigned int, ratio)
	),
	TP_fast_assign(
		__entry->gear_id = gear_id;
		__entry->margin = margin;
		__entry->ratio = ratio;
	),
	TP_printk(
		"gear_id=%u margin=%u ratio=%u",
		__entry->gear_id,
		__entry->margin,
		__entry->ratio)
);

TRACE_EVENT(sugov_ext_tar,
	TP_PROTO(int cpu, unsigned long ret_util,
		unsigned long cpu_util, unsigned long umax, int am),
	TP_ARGS(cpu, ret_util, cpu_util, umax, am),
	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned long, ret_util)
		__field(unsigned long, cpu_util)
		__field(unsigned long, umax)
		__field(int, am)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->ret_util = ret_util;
		__entry->cpu_util = cpu_util;
		__entry->umax = umax;
		__entry->am = am;
	),
	TP_printk(
		"cpu=%d ret_util=%ld cpu_util=%ld umax=%ld am=%d",
		__entry->cpu,
		__entry->ret_util,
		__entry->cpu_util,
		__entry->umax,
		__entry->am)
);

TRACE_EVENT(sugov_ext_group_dvfs,
	TP_PROTO(int cpu, unsigned long ret, unsigned long flt_util,
		unsigned long pelt_util_with_margin, unsigned long pelt_util, unsigned long pelt_margin, int source),

	TP_ARGS(cpu, ret, flt_util, pelt_util_with_margin, pelt_util, pelt_margin, source),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned long, ret)
		__field(unsigned long, flt_util)
		__field(unsigned long, pelt_util_with_margin)
		__field(unsigned long, pelt_util)
		__field(unsigned long, pelt_margin)
		__field(int, source)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->ret = ret;
		__entry->flt_util = flt_util;
		__entry->pelt_util_with_margin = pelt_util_with_margin;
		__entry->pelt_util = pelt_util;
		__entry->pelt_margin = pelt_margin;
		__entry->source = source;
	),

	TP_printk(
		"cpu=%d ret=%4lu tar=%4lu pelt_util_with_margin=%4lu pelt_util=%4lu pelt_margin=%4lu source=%2d",
		__entry->cpu,
		__entry->ret,
		__entry->flt_util,
		__entry->pelt_util_with_margin,
		__entry->pelt_util,
		__entry->pelt_margin,
		__entry->source
	)
);

TRACE_EVENT(sugov_ext_turn_point_margin,
	TP_PROTO(int cpu, int ret, int pelt_util, unsigned int turn_point,
		unsigned int target_margin, unsigned int target_margin_low, int pelt_util_with_orig_margin,
		int am_ctrl, int grp_dvfs_ctrl_mode),

	TP_ARGS(cpu, ret, pelt_util, turn_point, target_margin, target_margin_low, pelt_util_with_orig_margin,
		am_ctrl, grp_dvfs_ctrl_mode),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, ret)
		__field(int, pelt_util)
		__field(unsigned int, turn_point)
		__field(unsigned int, target_margin)
		__field(unsigned int, target_margin_low)
		__field(int, pelt_util_with_orig_margin)
		__field(int, am_ctrl)
		__field(int, grp_dvfs_ctrl_mode)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->ret = ret;
		__entry->pelt_util = pelt_util;
		__entry->turn_point = turn_point;
		__entry->target_margin = target_margin;
		__entry->target_margin_low = target_margin_low;
		__entry->pelt_util_with_orig_margin = pelt_util_with_orig_margin;
		__entry->am_ctrl = am_ctrl;
		__entry->grp_dvfs_ctrl_mode = grp_dvfs_ctrl_mode;
	),

	TP_printk(
		"cpu=%d ret=%d pelt_util=%d turn_point=%d target_margin=%d target_margin_low=%d pelt_util_with_orig_margin=%d am_ctrl=%d grp_dvfs_ctrl_mode=%d",
		__entry->cpu,
		__entry->ret,
		__entry->pelt_util,
		__entry->turn_point,
		__entry->target_margin,
		__entry->target_margin_low,
		__entry->pelt_util_with_orig_margin,
		__entry->am_ctrl,
		__entry->grp_dvfs_ctrl_mode
		)
);

TRACE_EVENT(collab_type_1_ret_function,

	TP_PROTO(unsigned long coef2_ltime, unsigned int coef1_ltime, unsigned int *coef1, unsigned int *coef2, unsigned int *cpu_all, unsigned int status),

	TP_ARGS(coef2_ltime, coef1_ltime, coef1, coef2, cpu_all, status),

	TP_STRUCT__entry(
		__field(unsigned int, coef2_ltime)
		__field(unsigned int, coef1_ltime)

		__field(unsigned int, coef1_cpu0)
        __field(unsigned int, coef2_cpu0)
        __field(unsigned int, cpu_all_cpu0)

		__field(unsigned int, coef1_cpu1)
        __field(unsigned int, coef2_cpu1)
        __field(unsigned int, cpu_all_cpu1)

		__field(unsigned int, coef1_cpu2)
        __field(unsigned int, coef2_cpu2)
        __field(unsigned int, cpu_all_cpu2)

		__field(unsigned int, coef1_cpu3)
        __field(unsigned int, coef2_cpu3)
        __field(unsigned int, cpu_all_cpu3)

		__field(unsigned int, coef1_cpu4)
        __field(unsigned int, coef2_cpu4)
        __field(unsigned int, cpu_all_cpu4)

		__field(unsigned int, coef1_cpu5)
        __field(unsigned int, coef2_cpu5)
    	__field(unsigned int, cpu_all_cpu5)

		__field(unsigned int, coef1_cpu6)
        __field(unsigned int, coef2_cpu6)
        __field(unsigned int, cpu_all_cpu6)

		__field(unsigned int, coef1_cpu7)
        __field(unsigned int, coef2_cpu7)
        __field(unsigned int, cpu_all_cpu7)

		__field(unsigned int, status)
	),
	TP_fast_assign(
		__entry->coef2_ltime = coef2_ltime;
		__entry->coef1_ltime = coef1_ltime;

		__entry->coef1_cpu0 = coef1[0];
        __entry->coef2_cpu0 = coef2[0];
        __entry->cpu_all_cpu0 = cpu_all[0];

		__entry->coef1_cpu1 = coef1[1];
        __entry->coef2_cpu1 = coef2[1];
        __entry->cpu_all_cpu1 = cpu_all[1];

		__entry->coef1_cpu2 = coef1[2];
        __entry->coef2_cpu2 = coef2[2];
        __entry->cpu_all_cpu2 = cpu_all[2];

		__entry->coef1_cpu3 = coef1[3];
        __entry->coef2_cpu3 = coef2[3];
        __entry->cpu_all_cpu3 = cpu_all[3];

		__entry->coef1_cpu4 = coef1[4];
        __entry->coef2_cpu4 = coef2[4];
        __entry->cpu_all_cpu4 = cpu_all[4];

		__entry->coef1_cpu5 = coef1[5];
        __entry->coef2_cpu5 = coef2[5];
    	__entry->cpu_all_cpu5 = cpu_all[5];

		__entry->coef1_cpu6 = coef1[6];
        __entry->coef2_cpu6 = coef2[6];
        __entry->cpu_all_cpu6 = cpu_all[6];

		__entry->coef1_cpu7 = coef1[7];
        __entry->coef2_cpu7 = coef2[7];
        __entry->cpu_all_cpu7 = cpu_all[7];

		__entry->status = status;
	),
	TP_printk(
		"coef1_ltime=%4u coef2_ltime=%4u cpu0=[%3u,%3u,%3u] cpu1=[%3u,%3u,%3u] cpu2=[%3u,%3u,%3u] cpu3=[%3u,%3u,%3u] cpu4=[%3u,%3u,%3u] cpu5=[%3u,%3u,%3u] cpu6=[%3u,%3u,%3u] cpu7=[%3u,%3u,%3u] status=%u",
		__entry->coef1_ltime, __entry->coef2_ltime,
		__entry->coef1_cpu0, __entry->coef2_cpu0, __entry->cpu_all_cpu0,
		__entry->coef1_cpu1, __entry->coef2_cpu1, __entry->cpu_all_cpu1,
		__entry->coef1_cpu2, __entry->coef2_cpu2, __entry->cpu_all_cpu2,
		__entry->coef1_cpu3, __entry->coef2_cpu3, __entry->cpu_all_cpu3,
		__entry->coef1_cpu4, __entry->coef2_cpu4, __entry->cpu_all_cpu4,
		__entry->coef1_cpu5, __entry->coef2_cpu5, __entry->cpu_all_cpu5,
		__entry->coef1_cpu6, __entry->coef2_cpu6, __entry->cpu_all_cpu6,
		__entry->coef1_cpu7, __entry->coef2_cpu7, __entry->cpu_all_cpu7,
		__entry->status)
);

TRACE_EVENT(collab_type_0_ret_function,

	TP_PROTO(unsigned int val, unsigned int status),

	TP_ARGS(val, status),

	TP_STRUCT__entry(
		__field(unsigned int, val)
		__field(unsigned int, status)
	),
	TP_fast_assign(
		__entry->val = val;
		__entry->status = status;
	),
	TP_printk(
		"val=%d status=%d",
		__entry->val,
		__entry->status)
);

TRACE_EVENT(sched_pd_opp2cap,
	TP_PROTO(int cpu, int opp, int quant, int wl,
		int val_1, int val_2, int val_r, int val_s, int val_m, bool r_o, int caller),

	TP_ARGS(cpu, opp, quant, wl, val_1, val_2, val_r, val_s, val_m, r_o, caller),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, opp)
		__field(int, quant)
		__field(int, wl)
		__field(int, val_1)
		__field(int, val_2)
		__field(int, val_r)
		__field(int, val_s)
		__field(int, val_m)
		__field(int, r_o)
		__field(int, caller)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->opp = opp;
		__entry->quant = quant;
		__entry->wl = wl;
		__entry->val_1 = val_1;
		__entry->val_2 = val_2;
		__entry->val_r = val_r;
		__entry->val_s = val_s;
		__entry->val_m = val_m;
		__entry->r_o = r_o;
		__entry->caller = caller;
	),
	TP_printk(
		"cpu=%d opp=%d quant=%d wl=%d val_1=%d val_2=%d val_r=%d val_s=%d val_m=%d r_o=%d caller=%d",
		__entry->cpu,
		__entry->opp,
		__entry->quant,
		__entry->wl,
		__entry->val_1,
		__entry->val_2,
		__entry->val_r,
		__entry->val_s,
		__entry->val_m,
		__entry->r_o,
		__entry->caller)
);

TRACE_EVENT(sched_pd_opp2pwr_eff,
	TP_PROTO(int cpu, int opp, int quant, int wl,
		int val_1, int val_2, int val_3, int *vals, int val_s, bool r_o, int caller),

	TP_ARGS(cpu, opp, quant, wl, val_1, val_2, val_3, vals, val_s, r_o, caller),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, opp)
		__field(int, quant)
		__field(int, wl)
		__field(int, val_1)
		__field(int, val_2)
		__field(int, val_3)
		__field(int, val_r1)
		__field(int, val_r2)
		__field(int, val_r3)
		__field(int, val_s)
		__field(int, r_o)
		__field(int, caller)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->opp = opp;
		__entry->quant = quant;
		__entry->wl = wl;
		__entry->val_1 = val_1;
		__entry->val_2 = val_2;
		__entry->val_3 = val_3;
		__entry->val_r1 = vals[0];
		__entry->val_r2 = vals[1];
		__entry->val_r3 = vals[2];
		__entry->val_s = val_s;
		__entry->r_o = r_o;
		__entry->caller = caller;
	),
	TP_printk(
		"cpu=%d opp=%d quant=%d wl=%d val_1=%d val_2=%d val_3=%d val_r1=%d val_r2=%d val_r3=%d val_s=%d r_o=%d caller=%d",
		__entry->cpu,
		__entry->opp,
		__entry->quant,
		__entry->wl,
		__entry->val_1,
		__entry->val_2,
		__entry->val_3,
		__entry->val_r1,
		__entry->val_r2,
		__entry->val_r3,
		__entry->val_s,
		__entry->r_o,
		__entry->caller)
);

TRACE_EVENT(sched_pd_opp2dyn_pwr,
	TP_PROTO(int cpu, int opp, int quant, int wl,
		int val_1, int val_r, int val_s, int val_m, bool r_o, int caller),

	TP_ARGS(cpu, opp, quant, wl, val_1, val_r, val_s, val_m, r_o, caller),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, opp)
		__field(int, quant)
		__field(int, wl)
		__field(int, val_1)
		__field(int, val_r)
		__field(int, val_s)
		__field(int, val_m)
		__field(int, r_o)
		__field(int, caller)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->opp = opp;
		__entry->quant = quant;
		__entry->wl = wl;
		__entry->val_1 = val_1;
		__entry->val_r = val_r;
		__entry->val_s = val_s;
		__entry->val_m = val_m;
		__entry->r_o = r_o;
		__entry->caller = caller;
	),
	TP_printk(
		"cpu=%d opp=%d quant=%d wl=%d val_1=%d val_r=%d val_s=%d val_m=%d r_o=%d caller=%d",
		__entry->cpu,
		__entry->opp,
		__entry->quant,
		__entry->wl,
		__entry->val_1,
		__entry->val_r,
		__entry->val_s,
		__entry->val_m,
		__entry->r_o,
		__entry->caller)
);

TRACE_EVENT(sched_pd_util2opp,
	TP_PROTO(int cpu, int quant, int wl,
		int val_1, int val_2, int val_3, int val_4, int val_r, int val_s, int val_m, bool r_o, int caller),

	TP_ARGS(cpu, quant, wl, val_1, val_2, val_3, val_4, val_r, val_s, val_m, r_o, caller),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, quant)
		__field(int, wl)
		__field(int, val_1)
		__field(int, val_2)
		__field(int, val_3)
		__field(int, val_4)
		__field(int, val_r)
		__field(int, val_s)
		__field(int, val_m)
		__field(int, r_o)
		__field(int, caller)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->quant = quant;
		__entry->wl = wl;
		__entry->val_1 = val_1;
		__entry->val_2 = val_2;
		__entry->val_3 = val_3;
		__entry->val_4 = val_4;
		__entry->val_r = val_r;
		__entry->val_s = val_s;
		__entry->val_m = val_m;
		__entry->r_o = r_o;
		__entry->caller = caller;
	),
	TP_printk(
		"cpu=%d quant=%d wl=%d val_1=%d val_2=%d val_3=%d val_4=%d val_r=%d val_s=%d val_m=%d r_o=%d caller=%d",
		__entry->cpu,
		__entry->quant,
		__entry->wl,
		__entry->val_1,
		__entry->val_2,
		__entry->val_3,
		__entry->val_4,
		__entry->val_r,
		__entry->val_s,
		__entry->val_m,
		__entry->r_o,
		__entry->caller)
);

TRACE_EVENT(sched_update_cpu_capacity,
	TP_PROTO(int cpu, struct rq *rq, unsigned long orig_cap_of, unsigned long gear_umax,
		unsigned long min_f_scale, unsigned long max_f_scale, unsigned long thermal_pressure, unsigned long *freq_for_debug),

	TP_ARGS(cpu, rq, orig_cap_of, gear_umax, min_f_scale, max_f_scale, thermal_pressure, freq_for_debug),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned long, cap_orig_of)
		__field(unsigned long, cap_of)
		__field(unsigned long, orig_cap_of)
		__field(unsigned long, gear_umax)
		__field(unsigned long, min_f_scale)
		__field(unsigned long, max_f_scale)
		__field(unsigned long, thermal_pressure)
		__field(unsigned long, __freq_ceiling)
		__field(unsigned long, gear_umax_freq)
		__field(unsigned long, max_f)
		__field(unsigned long, thermal_freq)
		__field(unsigned long, min_f)
		__field(unsigned long, __freq_floor)
		__field(unsigned long, dpt_v2_capacity_local)
		__field(unsigned long, dpt_v2_capacity_global)
		__field(unsigned long, cpu_ratio)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->cap_orig_of = arch_scale_cpu_capacity(cpu);
		__entry->cap_of = rq->cpu_capacity;
		__entry->orig_cap_of = orig_cap_of;
		__entry->gear_umax = gear_umax;
		__entry->min_f_scale = min_f_scale;
		__entry->max_f_scale = max_f_scale;
		__entry->thermal_pressure = thermal_pressure;
		__entry->__freq_ceiling = freq_for_debug[0];
		__entry->gear_umax_freq = freq_for_debug[1];
		__entry->max_f = freq_for_debug[2];
		__entry->thermal_freq = freq_for_debug[3];
		__entry->min_f = freq_for_debug[4];
		__entry->dpt_v2_capacity_local = freq_for_debug[5];
	),

	TP_printk("cpu=%d capacity_orig_of=%lu capacity_of=%lu orig_capacity_of=%lu gear_umax=%lu min_f_scale=%lu max_f_scale=%lu thermal_pressure=%lu __freq_ceiling=%lu gear_umax_freq=%lu max_f=%lu thermal_freq=%lu min_f=%lu dpt_v2_capacity_local=%lu",
		__entry->cpu,
		__entry->cap_orig_of,
		__entry->cap_of,
		__entry->orig_cap_of,
		__entry->gear_umax,
		__entry->min_f_scale,
		__entry->max_f_scale,
		__entry->thermal_pressure,
		__entry->__freq_ceiling,
		__entry->gear_umax_freq,
		__entry->max_f,
		__entry->thermal_freq,
		__entry->min_f,
		__entry->dpt_v2_capacity_local)
);

TRACE_EVENT(sugov_ext_curr_task_uclamp,
	TP_PROTO(int cpu, int pid, int flg_curr_tas, int flg_exit_state,
		int cpu_util_clp, int cpu_util_mgn,
		unsigned long task_uclamp, unsigned long task_uclamp_eff, int rq_uclamp),

	TP_ARGS(cpu, pid, flg_curr_tas, flg_exit_state,
		cpu_util_clp, cpu_util_mgn, task_uclamp, task_uclamp_eff, rq_uclamp),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, pid)
		__field(int, flg_curr_tas)
		__field(int, flg_exit_state)
		__field(int, cpu_util_clp)
		__field(int, cpu_util_mgn)
		__field(unsigned long, task_uclamp)
		__field(unsigned long, task_uclamp_eff)
		__field(int, rq_uclamp)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->pid = pid;
		__entry->flg_curr_tas = flg_curr_tas;
		__entry->flg_exit_state = flg_exit_state;
		__entry->cpu_util_clp = cpu_util_clp;
		__entry->cpu_util_mgn = cpu_util_mgn;
		__entry->task_uclamp = task_uclamp;
		__entry->task_uclamp_eff = task_uclamp_eff;
		__entry->rq_uclamp = rq_uclamp;
	),

	TP_printk(
		"cpu=%d pid=%5d flg_curr_task=%d flg_exit_state=%d cpu_util_clp=%4d cpu_util_mgn=%4d task_max_clamp_max=%4lu task_max_uclamp_eff=%4lu rq_max_clamp=%4d",
		__entry->cpu,
		__entry->pid,
		__entry->flg_curr_tas,
		__entry->flg_exit_state,
		__entry->cpu_util_clp,
		__entry->cpu_util_mgn,
		__entry->task_uclamp,
		__entry->task_uclamp_eff,
		__entry->rq_uclamp)
);

TRACE_EVENT(sugov_ext_util_dpt_v2,
	TP_PROTO(int cpu, unsigned long freq, unsigned long capacity,
		unsigned int cpu_util, unsigned int coef1_util, unsigned int coef2_util),

	TP_ARGS(cpu, freq, capacity, cpu_util, coef1_util, coef2_util),
	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned long, freq)
		__field(unsigned long, capacity)
		__field(unsigned int, cpu_util)
		__field(unsigned int, coef1_util)
		__field(unsigned int, coef2_util)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->freq = freq;
		__entry->capacity = capacity;
		__entry->cpu_util = cpu_util;
		__entry->coef1_util = coef1_util;
		__entry->coef2_util = coef2_util;
	),
	TP_printk(
		"cpu=%d freq=%lu capacity_needed=%lu cpu_util=%u coef1_util=%u coef2_util=%u",
		__entry->cpu,
		__entry->freq,
		__entry->capacity,
		__entry->cpu_util,
		__entry->coef1_util,
		__entry->coef2_util)
);

TRACE_EVENT(sugov_ext_util_debug_dpt_v2,
	TP_PROTO(int cpu, struct rq *rq, unsigned long util,
		unsigned long *utils_debug, unsigned long util_dl, unsigned long util_irq,
		unsigned long bw_dl, int exit_id, int scaling_factor),

	TP_ARGS(cpu, rq, util, utils_debug, util_dl, util_irq, bw_dl, exit_id, scaling_factor),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned long, util)
		__field(unsigned long, cpu_util)
		__field(unsigned long, orig_cpu_util)
		__field(unsigned long, rt_cpu_util)
		__field(unsigned long, coef1_util)
		__field(unsigned long, orig_coef1_util)
		__field(unsigned long, rt_coef1_util)
		__field(unsigned long, coef2_util)
		__field(unsigned long, orig_coef2_util)
		__field(unsigned long, rt_coef2_util)
		__field(unsigned long, util_dl)
		__field(unsigned long, util_irq)
		__field(unsigned long, bw_dl)
		__field(int, exit_id)
		__field(int, scaling_factor)
		__field(unsigned long, orig_umin)
		__field(unsigned long, orig_umax)
		__field(unsigned long, rq_umin)
		__field(unsigned long, rq_umax)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->util = util;
		__entry->cpu_util = utils_debug[0];
		__entry->orig_cpu_util = utils_debug[3];
		__entry->rt_cpu_util = cpu_util_rt_dpt_v2(rq, CPU_UTIL);
		__entry->coef1_util = utils_debug[1];
		__entry->orig_coef1_util = utils_debug[4];
		__entry->rt_coef1_util = cpu_util_rt_dpt_v2(rq, COEF1_UTIL);
		__entry->coef2_util = utils_debug[2];
		__entry->orig_coef2_util = utils_debug[5];
		__entry->rt_coef2_util = cpu_util_rt_dpt_v2(rq, COEF2_UTIL);
		__entry->util_dl = util_dl;
		__entry->util_irq = util_irq;
		__entry->bw_dl = bw_dl;
		__entry->exit_id = exit_id;
		__entry->scaling_factor = scaling_factor;
		__entry->orig_umin = utils_debug[6];
		__entry->orig_umax = utils_debug[7];
		__entry->rq_umin = utils_debug[8];
		__entry->rq_umax = utils_debug[9];
	),
	TP_printk(
		"cpu=%d util=%lu cpu_util=%lu orig_cpu_util=%lu rt_cpu_util=%lu coef1_util=%lu orig_coef1_util=%lu rt_coef1_util=%lu coef2_util=%lu orig_coef2_util=%lu rt_coef2_util=%lu dl=%lu irq=%lu bw_dl=%lu exit_id=%d scaling_factor=%d orig_umin=%lu orig_umax=%lu rq_umin=%lu rq_umax=%lu",
		__entry->cpu,
		__entry->util,
		__entry->cpu_util,
		__entry->orig_cpu_util,
		__entry->rt_cpu_util,
		__entry->coef1_util,
		__entry->orig_coef1_util,
		__entry->rt_coef1_util,
		__entry->coef2_util,
		__entry->orig_coef2_util,
		__entry->rt_coef2_util,
		__entry->util_dl,
		__entry->util_irq,
		__entry->bw_dl,
		__entry->exit_id,
		__entry->scaling_factor,
		__entry->orig_umin,
		__entry->orig_umax,
		__entry->rq_umin,
		__entry->rq_umax)
);

TRACE_EVENT(sugov_ext_gear_uclamp_dpt_v2,
	TP_PROTO(int cpu, int util_ori,
		int umin, int umax, int util,
		int umax_gear, unsigned long cpu_util, unsigned long coef1_util, unsigned long coef2_util),

	TP_ARGS(cpu, util_ori, umin, umax, util, umax_gear, cpu_util, coef1_util, coef2_util),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned int, util_ori)
		__field(unsigned int, umin)
		__field(unsigned int, umax)
		__field(unsigned int, util)
		__field(unsigned int, umax_gear)
		__field(unsigned long, cpu_util)
		__field(unsigned long, coef1_util)
		__field(unsigned long, coef2_util)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->util_ori = util_ori;
		__entry->umin = umin;
		__entry->umax = umax;
		__entry->util = util;
		__entry->umax_gear = umax_gear;
		__entry->cpu_util = cpu_util;
		__entry->coef1_util = coef1_util;
		__entry->coef2_util = coef2_util;
	),
	TP_printk(
		"cpu=%d util_ori=%d umin=%d umax=%d util_ret=%d, umax_gear=%d cpu_util=%lu coef1_util=%lu coef2_util=%lu",
		__entry->cpu,
		__entry->util_ori,
		__entry->umin,
		__entry->umax,
		__entry->util,
		__entry->umax_gear,
		__entry->cpu_util,
		__entry->coef1_util,
		__entry->coef2_util)
);

TRACE_EVENT(sugov_get_freq_margin,
	TP_PROTO(int cpu, unsigned long margin, int margin_id),

	TP_ARGS(cpu, margin, margin_id),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned long, margin)
		__field(int, margin_id)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->margin = margin;
		__entry->margin_id = margin_id;
	),
	TP_printk(
		"cpu=%d margin=%lu margin_id=%d",
		__entry->cpu,
		__entry->margin,
		__entry->margin_id)
);

TRACE_EVENT(sugov_ext_dpt_v2_get_uclamped_cpu_util,
	TP_PROTO(int cpu, unsigned long umin, unsigned long umax, unsigned long cpu_util_prime,
		unsigned long cpu_util, unsigned long coef1_util, unsigned long coef2_util,
		unsigned long  target_cap, unsigned long util, unsigned long *freq_arr_debug, unsigned long result),

	TP_ARGS(cpu, umin, umax, cpu_util_prime, cpu_util, coef1_util, coef2_util, target_cap, util, freq_arr_debug, result),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned long, umin)
		__field(unsigned long, umax)
		__field(unsigned long, cpu_util_prime)
		__field(unsigned long, cpu_util)
		__field(unsigned long, coef1_util)
		__field(unsigned long, coef2_util)
		__field(unsigned long, target_cap)
		__field(unsigned long, util)
		__field(unsigned long, umin_freq)
		__field(unsigned long, umax_freq)
		__field(unsigned long, util_freq)
		__field(unsigned long, result)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->umin = umin;
		__entry->umax = umax;
		__entry->cpu_util_prime = cpu_util_prime;
		__entry->cpu_util = cpu_util;
		__entry->coef1_util = coef1_util;
		__entry->coef2_util = coef2_util;
		__entry->target_cap = target_cap;
		__entry->util = util;
		__entry->umin_freq = freq_arr_debug[0];
		__entry->umax_freq = freq_arr_debug[1];
		__entry->util_freq = freq_arr_debug[2];
		__entry->result = result;
	),
	TP_printk(
		"cpu=%d umin=%lu umax=%lu cpu_util_prime=%lu cpu_util=%lu coef1_util=%lu coef2_util=%lu target_cap=%lu util=%lu umin_freq=%lu umax_freq=%lu util_freq=%lu result=%lu",
		__entry->cpu,
		__entry->umin,
		__entry->umax,
		__entry->cpu_util_prime,
		__entry->cpu_util,
		__entry->coef1_util,
		__entry->coef2_util,
		__entry->target_cap,
		__entry->util,
		__entry->umin_freq,
		__entry->umax_freq,
		__entry->util_freq,
		__entry->result)
);

TRACE_EVENT(sugov_ext_mtk_map_util_freq_dpt_v2,
	TP_PROTO(int cpu, unsigned long freq, unsigned long util, unsigned long orig_util,
		unsigned long cpu_util_local, unsigned long coef1_util_local, unsigned long coef2_util_local,
		unsigned long min, unsigned long max, unsigned long *coefs),

	TP_ARGS(cpu, freq, util, orig_util, cpu_util_local, coef1_util_local, coef2_util_local, min, max, coefs),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned long, freq)
		__field(unsigned long, util)
		__field(unsigned long, orig_util)
		__field(unsigned long, cpu_util_local)
		__field(unsigned long, coef1_util_local)
		__field(unsigned long, coef2_util_local)
		__field(unsigned long, coef1_param)
		__field(unsigned long, coef2_param)
		__field(unsigned long, min)
		__field(unsigned long, max)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->freq = freq;
		__entry->util = util;
		__entry->orig_util = orig_util;
		__entry->cpu_util_local = cpu_util_local;
		__entry->coef1_util_local = coef1_util_local;
		__entry->coef2_util_local = coef2_util_local;
		__entry->coef1_param = coefs[0];
		__entry->coef2_param = coefs[1];
		__entry->min = min;
		__entry->max = max;
	),
	TP_printk(
		"cpu=%d freq=%lu util=%lu orig_util=%lu cpu_util_local=%lu coef1_util_local=%lu coef2_util_local=%lu coef1_param=%lu coef2_param=%lu min=%lu max=%lu",
		__entry->cpu,
		__entry->freq,
		__entry->util,
		__entry->orig_util,
		__entry->cpu_util_local,
		__entry->coef1_util_local,
		__entry->coef2_util_local,
		__entry->coef1_param,
		__entry->coef2_param,
		__entry->min,
		__entry->max)
);

TRACE_EVENT(sugov_ext_flt_util_with_coef_margin,
	TP_PROTO(int cpu, int pelt_util, int flt_util, unsigned int flt_margin),
	TP_ARGS(cpu, pelt_util, flt_util, flt_margin),
	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, pelt_util)
		__field(int, flt_util)
		__field(unsigned int, flt_margin)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->pelt_util = pelt_util;
		__entry->flt_util = flt_util;
		__entry->flt_margin = flt_margin;
	),
	TP_printk(
		"cpu=%d pelt_util=%d flt_util=%d flt_margin=%d",
		__entry->cpu,
		__entry->pelt_util,
		__entry->flt_util,
		__entry->flt_margin)
);

#endif /* _TRACE_SCHEDULER_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH sugov
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE sugov_trace
/* This part must be outside protection */
#include <trace/define_trace.h>
