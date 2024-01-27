/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM ged

#if !defined(_TRACE_GED_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_GED_H

#include <linux/tracepoint.h>

/* common tracepoints */
TRACE_EVENT(tracing_mark_write,

	TP_PROTO(int pid, const char *name, long long value),

	TP_ARGS(pid, name, value),

	TP_STRUCT__entry(
		__field(int, pid)
		__string(name, name)
		__field(long long, value)
	),

	TP_fast_assign(
		__entry->pid = pid;
		__assign_str(name, name);
		__entry->value = value;
	),

	TP_printk("C|%d|%s|%lld", __entry->pid, __get_str(name), __entry->value)
);

TRACE_EVENT(GPU_DVFS__Frequency,
	TP_PROTO(unsigned int virtual_stack, unsigned int real_stack, unsigned int real_top),
	TP_ARGS(virtual_stack, real_stack, real_top),
	TP_STRUCT__entry(
		__field(unsigned int, virtual_stack)
		__field(unsigned int, real_stack)
		__field(unsigned int, real_top)
	),
	TP_fast_assign(
		__entry->virtual_stack = virtual_stack;
		__entry->real_stack = real_stack;
		__entry->real_top = real_top;
	),
	TP_printk("virtual_stack=%u, real_stack=%u, real_top=%u",
		__entry->virtual_stack, __entry->real_stack, __entry->real_top)
);

TRACE_EVENT(GPU_DVFS__Loading,

	TP_PROTO(unsigned int active, unsigned int tiler, unsigned int frag,
		unsigned int comp, unsigned int iter, unsigned int mcu , unsigned int iter_u_mcu),

	TP_ARGS(active, tiler, frag, comp, iter, mcu ,iter_u_mcu),

	TP_STRUCT__entry(
		__field(unsigned int, active)
		__field(unsigned int, tiler)
		__field(unsigned int, frag)
		__field(unsigned int, comp)
		__field(unsigned int, iter)
		__field(unsigned int, mcu)
		__field(unsigned int, iter_u_mcu)
	),

	TP_fast_assign(
		__entry->active = active;
		__entry->tiler = tiler;
		__entry->frag = frag;
		__entry->comp = comp;
		__entry->iter = iter;
		__entry->mcu = mcu;
		__entry->iter_u_mcu = iter_u_mcu
	),

	TP_printk("active=%u, tiler=%u, frag=%u, comp=%u, iter=%u, mcu=%u, iter_u_mcu=%u",
		__entry->active, __entry->tiler, __entry->frag, __entry->comp,
		__entry->iter, __entry->mcu , __entry->iter_u_mcu)
);

TRACE_EVENT(GPU_DVFS__Policy__Common,

	TP_PROTO(unsigned int commit_type, unsigned int policy_state, unsigned int commit_reason),

	TP_ARGS(commit_type, policy_state, commit_reason),

	TP_STRUCT__entry(
		__field(unsigned int, commit_type)
		__field(unsigned int, policy_state)
		__field(unsigned int, commit_reason)
	),

	TP_fast_assign(
		__entry->commit_type = commit_type;
		__entry->policy_state = policy_state;
		__entry->commit_reason = commit_reason;
	),

	TP_printk("commit_type=%u, policy_state=%u, commit_reason=%u",
		__entry->commit_type, __entry->policy_state, __entry->commit_reason)
);

TRACE_EVENT(GPU_DVFS__Policy__Common__Commit_Reason,

	TP_PROTO(unsigned int same, unsigned int diff),

	TP_ARGS(same, diff),

	TP_STRUCT__entry(
		__field(unsigned int, same)
		__field(unsigned int, diff)
	),

	TP_fast_assign(
		__entry->same = same;
		__entry->diff = diff;
	),

	TP_printk("same=%u, diff=%u", __entry->same, __entry->diff)
);

TRACE_EVENT(GPU_DVFS__Policy__Common__Commit_Reason__TID,

	TP_PROTO(int pid, int bqid, int count),

	TP_ARGS(pid, bqid, count),

	TP_STRUCT__entry(
		__field(int, pid)
		__field(int, bqid)
		__field(int, count)
	),

	TP_fast_assign(
		__entry->pid = pid;
		__entry->bqid = bqid;
		__entry->count = count;
	),

	TP_printk("%d_%d=%d", __entry->pid, __entry->bqid, __entry->count)
);

TRACE_EVENT(GPU_DVFS__Policy__Common__Check_Target,

	TP_PROTO(int pid, int bqid, int fps, int use),

	TP_ARGS(pid, bqid, fps, use),

	TP_STRUCT__entry(
		__field(int, pid)
		__field(int, bqid)
		__field(int, fps)
		__field(int, use)
	),

	TP_fast_assign(
		__entry->pid = pid;
		__entry->bqid = bqid;
		__entry->fps = fps;
		__entry->use = use;
	),

	TP_printk("gpu_fps=%d, pid=%d, q=%d, use=%d",
		__entry->fps, __entry->pid, __entry->bqid, __entry->use)
);

TRACE_EVENT(GPU_DVFS__Policy__Common__Sync_Api,

	TP_PROTO(int hint),

	TP_ARGS(hint),

	TP_STRUCT__entry(
		__field(int, hint)
	),

	TP_fast_assign(
		__entry->hint = hint;
	),

	TP_printk("hint=%d", __entry->hint)
);

/* frame-based policy tracepoints */
TRACE_EVENT(GPU_DVFS__Policy__Frame_based__Frequency,

	TP_PROTO(int target, int floor, int target_opp),

	TP_ARGS(target, floor, target_opp),

	TP_STRUCT__entry(
		__field(int, target)
		__field(int, floor)
		__field(int, target_opp)
	),

	TP_fast_assign(
		__entry->target = target;
		__entry->floor = floor;
		__entry->target_opp = target_opp;
	),

	TP_printk("target=%d, floor=%d, target_opp=%d",
		__entry->target, __entry->floor, __entry->target_opp)
);

TRACE_EVENT(GPU_DVFS__Policy__Frame_based__Workload,

	TP_PROTO(int cur, int avg, int real, int pipe, unsigned int mode),

	TP_ARGS(cur, avg, real, pipe, mode),

	TP_STRUCT__entry(
		__field(int, cur)
		__field(int, avg)
		__field(int, real)
		__field(int, pipe)
		__field(unsigned int, mode)
	),

	TP_fast_assign(
		__entry->cur = cur;
		__entry->avg = avg;
		__entry->real = real;
		__entry->pipe = pipe;
		__entry->mode = mode;
	),

	TP_printk("cur=%d, avg=%d, real=%d, pipe=%d, mode=%u", __entry->cur,
		__entry->avg, __entry->real, __entry->pipe, __entry->mode)
);

TRACE_EVENT(GPU_DVFS__Policy__Frame_based__GPU_Time,

	TP_PROTO(int cur, int target, int target_hd, int real, int pipe),

	TP_ARGS(cur, target, target_hd, real, pipe),

	TP_STRUCT__entry(
		__field(int, cur)
		__field(int, target)
		__field(int, target_hd)
		__field(int, real)
		__field(int, pipe)
	),

	TP_fast_assign(
		__entry->cur = cur;
		__entry->target = target;
		__entry->target_hd = target_hd;
		__entry->real = real;
		__entry->pipe = pipe;
	),

	TP_printk("cur=%d, target=%d, target_hd=%d, real=%d, pipe=%d",
		__entry->cur, __entry->target, __entry->target_hd, __entry->real,
		__entry->pipe)
);

TRACE_EVENT(GPU_DVFS__Policy__Frame_based__Margin,

	TP_PROTO(int ceil, int cur, int floor),

	TP_ARGS(ceil, cur, floor),

	TP_STRUCT__entry(
		__field(int, ceil)
		__field(int, cur)
		__field(int, floor)
	),

	TP_fast_assign(
		__entry->ceil = ceil;
		__entry->cur = cur;
		__entry->floor = floor;
	),

	TP_printk("ceil=%d, cur=%d, floor=%d", __entry->ceil, __entry->cur, __entry->floor)
);

TRACE_EVENT(GPU_DVFS__Policy__Frame_based__Margin__Detail,

	TP_PROTO(unsigned int margin_mode, int target_fps_margin,
		int min_margin_inc_step, int min_margin),

	TP_ARGS(margin_mode, target_fps_margin, min_margin_inc_step, min_margin),

	TP_STRUCT__entry(
		__field(unsigned int, margin_mode)
		__field(int, target_fps_margin)
		__field(int, min_margin_inc_step)
		__field(int, min_margin)
	),

	TP_fast_assign(
		__entry->margin_mode = margin_mode;
		__entry->target_fps_margin = target_fps_margin;
		__entry->min_margin_inc_step = min_margin_inc_step;
		__entry->min_margin = min_margin;
	),

	TP_printk("margin_mode=%u, target_fps_margin=%d, min_margin_inc_step=%d, min_margin=%d",
		__entry->margin_mode, __entry->target_fps_margin,
		__entry->min_margin_inc_step, __entry->min_margin)
);

TRACE_EVENT(GPU_DVFS__Policy__Frame_based__Async_ratio__Counter,

	TP_PROTO(unsigned int gpu_active, unsigned int iter_active, unsigned int compute_active,
		unsigned int l2_rd_stall, unsigned int irq_active, unsigned int mcu_active),

	TP_ARGS(gpu_active, iter_active, compute_active, l2_rd_stall, irq_active, mcu_active),

	TP_STRUCT__entry(
		__field(unsigned int, gpu_active)
		__field(unsigned int, iter_active)
		__field(unsigned int, compute_active)
		__field(unsigned int, l2_rd_stall)
		__field(unsigned int, irq_active)
		__field(unsigned int, mcu_active)
	),

	TP_fast_assign(
		__entry->gpu_active = gpu_active;
		__entry->iter_active = iter_active;
		__entry->compute_active = compute_active;
		__entry->l2_rd_stall = l2_rd_stall;
		__entry->irq_active = irq_active;
		__entry->mcu_active = mcu_active;
	),

	TP_printk("gpu_active=%u, iter_active=%u, compute_active=%u, l2_rd_stall=%u, irq_active=%u, mcu_active=%u",
		__entry->gpu_active, __entry->iter_active, __entry->compute_active,
		__entry->l2_rd_stall, __entry->irq_active, __entry->mcu_active)
);

TRACE_EVENT(GPU_DVFS__Policy__Frame_based__Async_ratio__Index,

	TP_PROTO(int is_decreasing, int async_ratio,
		int perf_improve, int fb_oppidx, int fb_tar_freq, int as_tar_opp),

	TP_ARGS(is_decreasing, async_ratio, perf_improve, fb_oppidx,
			fb_tar_freq, as_tar_opp),

	TP_STRUCT__entry(
		__field(int, is_decreasing)
		__field(int, async_ratio)
		__field(int, perf_improve)
		__field(int, fb_oppidx)
		__field(int, fb_tar_freq)
		__field(int, as_tar_opp)
	),

	TP_fast_assign(
		__entry->is_decreasing = is_decreasing;
		__entry->async_ratio = async_ratio;
		__entry->perf_improve = perf_improve;
		__entry->fb_oppidx = fb_oppidx;
		__entry->fb_tar_freq = fb_tar_freq;
		__entry->as_tar_opp = as_tar_opp;
	),

	TP_printk("is_decreasing=%d, async_ratio=%d, perf_improve=%d, fb_oppidx=%d, fb_tar_freq=%d, as_tar_opp=%d",
		__entry->is_decreasing, __entry->async_ratio,
		__entry->perf_improve, __entry->fb_oppidx,
		__entry->fb_tar_freq, __entry->as_tar_opp)
);

TRACE_EVENT(GPU_DVFS__Policy__Frame_based__Async_ratio__Policy,

	TP_PROTO(int cur_opp_id, int fb_oppidx,
		int async_id, int apply_async, int is_decreasing),

	TP_ARGS(cur_opp_id, fb_oppidx, async_id, apply_async,
			is_decreasing),

	TP_STRUCT__entry(
		__field(int, cur_opp_id)
		__field(int, fb_oppidx)
		__field(int, async_id)
		__field(int, apply_async)
		__field(int, is_decreasing)
	),

	TP_fast_assign(
		__entry->cur_opp_id = cur_opp_id;
		__entry->fb_oppidx = fb_oppidx;
		__entry->async_id = async_id;
		__entry->apply_async = apply_async;
		__entry->is_decreasing = is_decreasing;
	),

	TP_printk("cur_opp_id=%d, fb_oppidx=%d, async_id=%d, apply_async=%d, is_decreasing=%d",
		__entry->cur_opp_id, __entry->fb_oppidx,
		__entry->async_id, __entry->apply_async,
		__entry->is_decreasing)
);

/* loading-based policy tracepoints */
TRACE_EVENT(GPU_DVFS__Policy__Loading_based__Opp,

	TP_PROTO(int target),

	TP_ARGS(target),

	TP_STRUCT__entry(
		__field(int, target)
	),

	TP_fast_assign(
		__entry->target = target;
	),

	TP_printk("target=%d", __entry->target)
);

TRACE_EVENT(GPU_DVFS__Policy__Loading_based__Loading,

	TP_PROTO(unsigned int cur, unsigned int mode, unsigned int fb_adj, unsigned int win_size),

	TP_ARGS(cur, mode, fb_adj, win_size),

	TP_STRUCT__entry(
		__field(unsigned int, cur)
		__field(unsigned int, mode)
		__field(unsigned int, fb_adj)
		__field(unsigned int, win_size)
	),

	TP_fast_assign(
		__entry->cur = cur;
		__entry->mode = mode;
		__entry->fb_adj = fb_adj;
		__entry->win_size = win_size;
	),

	TP_printk("cur=%u, mode=%u, fb_adj=%u, win_size=%u", __entry->cur, __entry->mode,
			__entry->fb_adj, __entry->win_size)
);

TRACE_EVENT(GPU_DVFS__Policy__Loading_based__Bound,

	TP_PROTO(int ultra_high, int high, int low, int ultra_low),

	TP_ARGS(ultra_high, high, low, ultra_low),

	TP_STRUCT__entry(
		__field(int, ultra_high)
		__field(int, high)
		__field(int, low)
		__field(int, ultra_low)
	),

	TP_fast_assign(
		__entry->ultra_high = ultra_high;
		__entry->high = high;
		__entry->low = low;
		__entry->ultra_low = ultra_low;
	),

	TP_printk("ultra_high=%d, high=%d, low=%d, ultra_low=%d",
		__entry->ultra_high, __entry->high, __entry->low, __entry->ultra_low)
);

TRACE_EVENT(GPU_DVFS__Policy__Loading_based__GPU_Time,

	TP_PROTO(int cur, int target, int target_hd, int complete, int uncomplete),

	TP_ARGS(cur, target, target_hd, complete, uncomplete),

	TP_STRUCT__entry(
		__field(int, cur)
		__field(int, target)
		__field(int, target_hd)
		__field(int, complete)
		__field(int, uncomplete)
	),

	TP_fast_assign(
		__entry->cur = cur;
		__entry->target = target;
		__entry->target_hd = target_hd;
		__entry->complete = complete;
		__entry->uncomplete = uncomplete;
	),

	TP_printk("cur=%d, target=%d, target_hd=%d, complete=%d, uncomplete=%d",
		__entry->cur, __entry->target, __entry->target_hd, __entry->complete,
		__entry->uncomplete)
);

TRACE_EVENT(GPU_DVFS__Policy__Loading_based__Margin,

	TP_PROTO(int ceil, int cur, int floor),

	TP_ARGS(ceil, cur, floor),

	TP_STRUCT__entry(
		__field(int, ceil)
		__field(int, cur)
		__field(int, floor)
	),

	TP_fast_assign(
		__entry->ceil = ceil;
		__entry->cur = cur;
		__entry->floor = floor;
	),

	TP_printk("ceil=%d, cur=%d, floor=%d", __entry->ceil, __entry->cur, __entry->floor)
);

TRACE_EVENT(GPU_DVFS__Policy__Loading_based__Margin__Detail,

	TP_PROTO(unsigned int margin_mode, int margin_step, int min_margin),

	TP_ARGS(margin_mode, margin_step, min_margin),

	TP_STRUCT__entry(
		__field(unsigned int, margin_mode)
		__field(int, margin_step)
		__field(int, min_margin)
	),

	TP_fast_assign(
		__entry->margin_mode = margin_mode;
		__entry->margin_step = margin_step;
		__entry->min_margin = min_margin;
	),

	TP_printk("margin_mode=%u, margin_step=%d, min_margin=%d",
		__entry->margin_mode, __entry->margin_step,
		__entry->min_margin)
);

TRACE_EVENT(GPU_DVFS__Policy__Loading_based__Fallback_Tuning,

	TP_PROTO(int fallback_tuning, int fallback_idle, int uncomplete_type,
			int uncomplete_flag, int lb_last_opp),

	TP_ARGS(fallback_tuning, fallback_idle, uncomplete_type, uncomplete_flag, lb_last_opp),

	TP_STRUCT__entry(
		__field(int, fallback_tuning)
		__field(int, fallback_idle)
		__field(int, uncomplete_type)
		__field(int, uncomplete_flag)
		__field(int, lb_last_opp)
	),

	TP_fast_assign(
		__entry->fallback_tuning = fallback_tuning;
		__entry->fallback_idle = fallback_idle;
		__entry->uncomplete_type = uncomplete_type;
		__entry->uncomplete_flag = uncomplete_flag;
		__entry->lb_last_opp = lb_last_opp;
	),

	TP_printk("fallback_tuning=%d, fallback_idle=%d, uncomplete_type=%d, uncomplete_flag=%d, lb_last_opp=%d",
		__entry->fallback_tuning, __entry->fallback_idle,
		__entry->uncomplete_type, __entry->uncomplete_flag, __entry->lb_last_opp)
);

TRACE_EVENT(GPU_DVFS__Policy__Loading_based__Async_Ratio__MCU_index,

	TP_PROTO(unsigned int max_is_mcu, unsigned int avg_mcu, unsigned int max_mcu,
			unsigned int avg_mcu_th, unsigned int max_mcu_th),

	TP_ARGS(max_is_mcu, avg_mcu, max_mcu, avg_mcu_th, max_mcu_th),

	TP_STRUCT__entry(
		__field(unsigned int, max_is_mcu)
		__field(unsigned int, avg_mcu)
		__field(unsigned int, max_mcu)
		__field(unsigned int, avg_mcu_th)
		__field(unsigned int, max_mcu_th)
	),

	TP_fast_assign(
		__entry->max_is_mcu = max_is_mcu;
		__entry->avg_mcu = avg_mcu;
		__entry->max_mcu = max_mcu;
		__entry->avg_mcu_th = avg_mcu_th;
		__entry->max_mcu_th = max_mcu_th;
	),

	TP_printk("max_is_mcu=%u, avg_mcu=%u, max_mcu=%u, avg_mcu_th=%u, max_mcu_th=%u",
		__entry->max_is_mcu, __entry->avg_mcu,
		__entry->max_mcu, __entry->avg_mcu_th, __entry->max_mcu_th)
);

TRACE_EVENT(GPU_DVFS__Policy__Loading_based__Async_Ratio__Policy,

	TP_PROTO(unsigned int apply_lb_async, unsigned int perf_improve, unsigned int adjust_ratio,
			unsigned int opp_diff),

	TP_ARGS(apply_lb_async, perf_improve, adjust_ratio, opp_diff),

	TP_STRUCT__entry(
		__field(unsigned int, apply_lb_async)
		__field(unsigned int, perf_improve)
		__field(unsigned int, adjust_ratio)
		__field(unsigned int, opp_diff)
	),

	TP_fast_assign(
		__entry->apply_lb_async = apply_lb_async;
		__entry->perf_improve = perf_improve;
		__entry->adjust_ratio = adjust_ratio;
		__entry->opp_diff = opp_diff;
	),

	TP_printk("apply_lb_async=%u, perf_improve=%u, adjust_ratio=%u, opp_diff=%u",
		__entry->apply_lb_async, __entry->perf_improve,
		__entry->adjust_ratio, __entry->opp_diff)
);

/* DCS tracepoints */
TRACE_EVENT(GPU_DVFS__Policy__DCS,

	TP_PROTO(int max_core, int current_core, unsigned int fix_core),

	TP_ARGS(max_core, current_core, fix_core),

	TP_STRUCT__entry(
		__field(int, max_core)
		__field(int, current_core)
		__field(int, fix_core)
	),

	TP_fast_assign(
		__entry->max_core = max_core;
		__entry->current_core = current_core;
		__entry->fix_core = fix_core;
	),

	TP_printk("max_core=%d, current_core=%d fix_core=%u",
	__entry->max_core, __entry->current_core, __entry->fix_core)
);

TRACE_EVENT(GPU_DVFS__Policy__DCS__Detail,

	TP_PROTO(unsigned int core_mask),

	TP_ARGS(core_mask),

	TP_STRUCT__entry(
		__field(unsigned int, core_mask)
	),

	TP_fast_assign(
		__entry->core_mask = core_mask;
	),

	TP_printk("core_mask=%u", __entry->core_mask)
);

TRACE_EVENT(GPU_DVFS__EB_Frequency,
	TP_PROTO(unsigned int virtual_stack, unsigned int real_stack, unsigned int real_top,
		unsigned int diff),
	TP_ARGS(virtual_stack, real_stack, real_top, diff),
	TP_STRUCT__entry(
		__field(unsigned int, virtual_stack)
		__field(unsigned int, real_stack)
		__field(unsigned int, real_top)
		__field(unsigned int, diff)
	),
	TP_fast_assign(
		__entry->virtual_stack = virtual_stack;
		__entry->real_stack = real_stack;
		__entry->real_top = real_top;
		__entry->diff = diff;
	),
	TP_printk("virtual_stack=%u real_stack=%u real_top=%u diff=%u",
		__entry->virtual_stack, __entry->real_stack, __entry->real_top, __entry->diff)
);

TRACE_EVENT(GPU_DVFS__EB_Loading,

	TP_PROTO(unsigned int active, unsigned int tiler, unsigned int frag,
		unsigned int comp, unsigned int iter, unsigned int mcu, unsigned int iter_u_mcu,
		unsigned int diff),

	TP_ARGS(active, tiler, frag, comp, iter, mcu ,iter_u_mcu, diff),

	TP_STRUCT__entry(
		__field(unsigned int, active)
		__field(unsigned int, tiler)
		__field(unsigned int, frag)
		__field(unsigned int, comp)
		__field(unsigned int, iter)
		__field(unsigned int, mcu)
		__field(unsigned int, iter_u_mcu)
		__field(unsigned int, diff)
	),

	TP_fast_assign(
		__entry->active = active;
		__entry->tiler = tiler;
		__entry->frag = frag;
		__entry->comp = comp;
		__entry->iter = iter;
		__entry->mcu = mcu;
		__entry->iter_u_mcu = iter_u_mcu;
		__entry->diff = diff;
	),

	TP_printk("active=%u tiler=%u frag=%u comp=%u iter=%u mcu=%u iter_u_mcu=%u diff=%u",
		__entry->active, __entry->tiler, __entry->frag, __entry->comp,
		__entry->iter, __entry->mcu , __entry->iter_u_mcu, __entry->diff)
);

TRACE_EVENT(GPU_DVFS__Policy__EB_Common,

	TP_PROTO(unsigned int commit_type, unsigned int policy_state,
		unsigned int diff),

	TP_ARGS(commit_type, policy_state, diff),

	TP_STRUCT__entry(
		__field(unsigned int, commit_type)
		__field(unsigned int, policy_state)
		__field(unsigned int, diff)
	),

	TP_fast_assign(
		__entry->commit_type = commit_type;
		__entry->policy_state = policy_state;
		__entry->diff = diff;
	),

	TP_printk("commit_type=%u policy_state=%u diff=%u",
		__entry->commit_type, __entry->policy_state, __entry->diff)
);

TRACE_EVENT(GPU_DVFS__Policy__EB_Common__Sync_Api,

	TP_PROTO(int hint),

	TP_ARGS(hint),

	TP_STRUCT__entry(
		__field(int, hint)
	),

	TP_fast_assign(
		__entry->hint = hint;
	),

	TP_printk("hint=%d", __entry->hint)
);

/* frame-based policy tracepoints */
TRACE_EVENT(GPU_DVFS__Policy__Frame_based__EB_monitor,

	TP_PROTO(int cur, int target, unsigned int diff),

	TP_ARGS(cur, target, diff),

	TP_STRUCT__entry(
		__field(int, fb_t_gpu)
		__field(int, fb_target)
		__field(unsigned int, diff)
	),

	TP_fast_assign(
		__entry->fb_t_gpu = cur;
		__entry->fb_target = target;
		__entry->diff = diff;
	),

	TP_printk("fb_t_gpu=%d fb_target=%d diff=%u",
		__entry->fb_t_gpu, __entry->fb_target, __entry->diff)
);


/* loading-based policy tracepoints */
TRACE_EVENT(GPU_DVFS__Policy__Loading_based__EB_Opp,

	TP_PROTO(int target, unsigned int diff),

	TP_ARGS(target, diff),

	TP_STRUCT__entry(
		__field(int, target)
		__field(unsigned int, diff)
	),

	TP_fast_assign(
		__entry->target = target;
		__entry->diff = diff;
	),

	TP_printk("target=%d diff=%u", __entry->target, __entry->diff)
);

TRACE_EVENT(GPU_DVFS__Policy__Loading_based__EB_Loading,

	TP_PROTO(unsigned int cur, unsigned int diff),

	TP_ARGS(cur, diff),

	TP_STRUCT__entry(
		__field(unsigned int, cur)
		__field(unsigned int, diff)
	),

	TP_fast_assign(
		__entry->cur = cur;
		__entry->diff = diff;
	),

	TP_printk("cur=%u diff=%u", __entry->cur, __entry->diff)
);

TRACE_EVENT(GPU_DVFS__Policy__Loading_based__EB_Bound,

	TP_PROTO(int ultra_high, int high, int low, int ultra_low, unsigned int diff),

	TP_ARGS(ultra_high, high, low, ultra_low, diff),

	TP_STRUCT__entry(
		__field(int, ultra_high)
		__field(int, high)
		__field(int, low)
		__field(int, ultra_low)
		__field(unsigned int, diff)
	),

	TP_fast_assign(
		__entry->ultra_high = ultra_high;
		__entry->high = high;
		__entry->low = low;
		__entry->ultra_low = ultra_low;
		__entry->diff = diff;
	),

	TP_printk("ultra_high=%d high=%d low=%d ultra_low=%d diff=%u",
		__entry->ultra_high, __entry->high, __entry->low, __entry->ultra_low, __entry->diff)
);

TRACE_EVENT(GPU_DVFS__Policy__Loading_based__EB_GPU_Time,

	TP_PROTO(int cur, int target, int target_hd, int complete, int uncomplete, unsigned int diff),

	TP_ARGS(cur, target, target_hd, complete, uncomplete, diff),

	TP_STRUCT__entry(
		__field(int, lb_t_gpu)
		__field(int, lb_target)
		__field(int, lb_target_hd)
		__field(int, lb_complete)
		__field(int, lb_uncomplete)
		__field(unsigned int, diff)
	),

	TP_fast_assign(
		__entry->lb_t_gpu = cur;
		__entry->lb_target = target;
		__entry->lb_target_hd = target_hd;
		__entry->lb_complete = complete;
		__entry->lb_uncomplete = uncomplete;
		__entry->diff = diff;
	),

	TP_printk("lb_t_gpu=%d lb_target=%d lb_target_hd=%d lb_complete=%d lb_uncomplete=%d diff=%u",
		__entry->lb_t_gpu, __entry->lb_target, __entry->lb_target_hd, __entry->lb_complete,
		__entry->lb_uncomplete, __entry->diff)

);

TRACE_EVENT(GPU_DVFS__Policy__Loading_based__EB_Margin,

	TP_PROTO(int ceil, int cur, int floor, unsigned int diff),

	TP_ARGS(ceil, cur, floor, diff),

	TP_STRUCT__entry(
		__field(int, ceil)
		__field(int, cur)
		__field(int, floor)
		__field(unsigned int, diff)
	),

	TP_fast_assign(
		__entry->ceil = ceil;
		__entry->cur = cur;
		__entry->floor = floor;
		__entry->diff = diff;

	),

	TP_printk("ceil=%d cur=%d floor=%d diff=%u", __entry->ceil, __entry->cur, __entry->floor, __entry->diff)
);

TRACE_EVENT(GPU_DVFS__Policy__EB_DEBUG,

	TP_PROTO(unsigned int count, unsigned int diff),

	TP_ARGS(count, diff),

	TP_STRUCT__entry(
		__field(unsigned int, count)
		__field(unsigned int, diff)
	),

	TP_fast_assign(
		__entry->count = count;
		__entry->diff = diff;

	),

	TP_printk("count=%u diff=%u", __entry->count, __entry->diff)
);

TRACE_EVENT(GPU_DVFS__Policy__EB_RINBUFFER,
	TP_PROTO(const char *name, const int *arg, const u32 *diff_time),
	TP_ARGS(name, arg, diff_time),

	TP_STRUCT__entry(
		__string(name, name)
		__field(unsigned int, u0)
		__field(unsigned int, u1)
		__field(unsigned int, u2)
		__field(unsigned int, u3)
		__field(unsigned int, u4)
		__field(unsigned int, u5)
		__field(unsigned int, u6)
		__field(unsigned int, u7)
		__field(unsigned int, t0)
		__field(unsigned int, t1)
		__field(unsigned int, t2)
		__field(unsigned int, t3)
		__field(unsigned int, t4)
		__field(unsigned int, t5)
		__field(unsigned int, t6)
		__field(unsigned int, t7)
	),
	TP_fast_assign(
		__assign_str(name, name);
		__entry->u0 = arg[0];
		__entry->u1 = arg[1];
		__entry->u2 = arg[2];
		__entry->u3 = arg[3];
		__entry->u4 = arg[4];
		__entry->u5 = arg[5];
		__entry->u6 = arg[6];
		__entry->u7 = arg[7];
		__entry->t0 = diff_time[0];
		__entry->t1 = diff_time[1];
		__entry->t2 = diff_time[2];
		__entry->t3 = diff_time[3];
		__entry->t4 = diff_time[4];
		__entry->t5 = diff_time[5];
		__entry->t6 = diff_time[6];
		__entry->t7 = diff_time[7];

	),
	TP_printk("name=%s u0=%u u1=%u u2=%u u3=%u u4=%u u5=%u u6=%u u7=%u t0=%u t1=%u t2=%u t3=%u t4=%u t5=%u t6=%u t7=%u",
		 __get_str(name),
		__entry->u0, __entry->u1, __entry->u2,
		 __entry->u3, __entry->u4, __entry->u5,
		 __entry->u6, __entry->u7,
		 __entry->t0, __entry->t1, __entry->t2,
		__entry->t3, __entry->t4, __entry->t5,
		 __entry->t6, __entry->t7)
);

#endif /* _TRACE_GED_H */


/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../drivers/gpu/mediatek/ged/include
#define TRACE_INCLUDE_FILE ged_tracepoint
#include <trace/define_trace.h>
