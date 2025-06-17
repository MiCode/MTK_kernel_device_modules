// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2016 MediaTek Inc.
* Author: PC Chen <pc.chen@mediatek.com>
*       Tiffany Lin <tiffany.lin@mediatek.com>
*/

#include <linux/module.h>
#include <linux/dma-resv.h>
#include <media/videobuf2-dma-contig.h>
#include <linux/dma-heap.h>
#include <linux/dma-direction.h>
#include <uapi/linux/dma-heap.h>
#include <mtk_heap.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include <linux/vmalloc.h>

#include "mtk_vcodec_fence.h"
#include "mtk_vcodec_drv.h"
#include "mtk_vcodec_util.h"
#include "mtk_vcu.h"
#include "mtk_vcodec_dec.h"
#include "mtk_vcodec_enc.h"
#include "vdec_drv_if.h"
#include "venc_drv_if.h"
#ifdef MTK_SCHED_SUPPORT
#include "eas/group.h"
#endif
#ifdef MTK_VIDEO_GO_SUPPORT
#include "videogo_public.h"
#endif

#define LOG_PARAM_INFO_SIZE 64
#define MAX_SUPPORTED_LOG_PARAMS_COUNT 12

char mtk_vdec_tmp_log[LOG_PROPERTY_SIZE];
char mtk_venc_tmp_log[LOG_PROPERTY_SIZE];
char mtk_vdec_tmp_prop[LOG_PROPERTY_SIZE];
char mtk_venc_tmp_prop[LOG_PROPERTY_SIZE];

static struct mtk_vcodec_dev *dev_ptr[MTK_INST_MAX];
static void (*vcodec_trace_puts_ptr)(char *);
#ifdef MTK_VIDEO_GO_SUPPORT
static void (*vcodec_to_vgo)(int, void *);
#endif


void mtk_vcodec_set_dev(struct mtk_vcodec_dev *dev, enum mtk_instance_type type)
{
	if (dev && type < MTK_INST_MAX && type >= 0)
		dev_ptr[type] = dev;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_set_dev);

void mtk_vcodec_check_alive(struct timer_list *t)
{
	struct dvfs_params *params;
	struct mtk_vcodec_dev *dev;

	/* Only support vdec check alive now */
	if (mtk_vcodec_is_vcp(MTK_INST_DECODER)) {
		params = from_timer(params, t, vdec_active_checker);
		dev = container_of(params, struct mtk_vcodec_dev, vdec_dvfs_params);
		dev->check_alive_work.dev = dev;
		dev->check_alive_work.ctx = NULL;
		queue_work(dev->check_alive_workqueue, &dev->check_alive_work.work);

		/*retrigger timer for next check*/
		params->vdec_active_checker.expires =
			jiffies + msecs_to_jiffies(MTK_VDEC_CHECK_ACTIVE_INTERVAL);
		add_timer(&params->vdec_active_checker);
	}
}
EXPORT_SYMBOL_GPL(mtk_vcodec_check_alive);

static void mtk_vcodec_alive_checker_init(struct mtk_vcodec_ctx *ctx)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
#ifdef VDEC_CHECK_ALIVE
	struct mtk_vcodec_dev *dev = ctx->dev;

	/* Only support vdec check alive now */
	if (mtk_vcodec_is_vcp(MTK_INST_DECODER) && ctx->type == MTK_INST_DECODER) {
		if (!dev->vdec_dvfs_params.has_timer) {
			mtk_vcodec_dvfs_qos_log(false, "[%d][VDVFS][VDEC] init vdec alive checker...", ctx->id);
			timer_setup(&dev->vdec_dvfs_params.vdec_active_checker,
				mtk_vcodec_check_alive, 0);
			dev->vdec_dvfs_params.vdec_active_checker.expires =
			jiffies + msecs_to_jiffies(MTK_VDEC_CHECK_ACTIVE_INTERVAL);
			add_timer(&dev->vdec_dvfs_params.vdec_active_checker);
			dev->vdec_dvfs_params.has_timer = 1;
		}
	}
#endif
#endif
}

static void mtk_vcodec_alive_checker_deinit(struct mtk_vcodec_ctx *ctx, bool is_last)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
#ifdef VDEC_CHECK_ALIVE
	struct mtk_vcodec_dev *dev = ctx->dev;

	/* Only support vdec check alive now */
	if (mtk_vcodec_is_vcp(MTK_INST_DECODER) && ctx->type == MTK_INST_DECODER) {
		if (dev->vdec_dvfs_params.has_timer && is_last) {
			del_timer_sync(&dev->vdec_dvfs_params.vdec_active_checker);
			flush_workqueue(dev->check_alive_workqueue);
			dev->vdec_dvfs_params.has_timer = 0;
			mtk_vcodec_dvfs_qos_log(false, "[%d][VDVFS][VDEC] deinit vdec alive checker...",
				ctx->id);
		}
	}
#endif
#endif
}

/* For encoder, this will enable logs in venc/*/
bool mtk_vcodec_dbg;
EXPORT_SYMBOL_GPL(mtk_vcodec_dbg);

/* For vcodec performance measure */
bool mtk_vcodec_perf;
EXPORT_SYMBOL_GPL(mtk_vcodec_perf);

/* The log level of v4l2 encoder or decoder driver.
 * That is, files under mtk-vcodec/.
 */
int mtk_v4l2_dbg_level;
EXPORT_SYMBOL_GPL(mtk_v4l2_dbg_level);

/* The log level of decoder low power mode related log.
 */
int mtk_vdec_lpw_level;
EXPORT_SYMBOL_GPL(mtk_vdec_lpw_level);

/* For vdec kernel driver trace enable */
bool mtk_vcodec_trace_enable;
EXPORT_SYMBOL_GPL(mtk_vcodec_trace_enable);

/* For vcodec vcp debug */
int mtk_vcodec_vcp;
EXPORT_SYMBOL_GPL(mtk_vcodec_vcp);

/* For vdec & dvfs & qos log info */
bool mtk_vcodec_dvfs_qos_log_en;
EXPORT_SYMBOL_GPL(mtk_vcodec_dvfs_qos_log_en);

/* For vdec set property */
char *mtk_vdec_property = "";
EXPORT_SYMBOL(mtk_vdec_property);

/* For venc set property */
char *mtk_venc_property = "";
EXPORT_SYMBOL(mtk_venc_property);

/* For vdec vcp log info */
char *mtk_vdec_vcp_log = "";
EXPORT_SYMBOL(mtk_vdec_vcp_log);

/* For venc vcp log info */
char *mtk_venc_vcp_log = "";
EXPORT_SYMBOL(mtk_venc_vcp_log);

/* For venc slb cb info */
struct VENC_SLB_CB_T mtk_venc_slb_cb = {0};
EXPORT_SYMBOL(mtk_venc_slb_cb);

/* For vdec open set cgroup colocate enable time*/
int mtk_vdec_open_cgrp_delay = MTK_VDEC_OPEN_CGRP_MS;
EXPORT_SYMBOL_GPL(mtk_vdec_open_cgrp_delay);

/* For vdec slc switch on/off */
bool mtk_vdec_slc_enable = true;
EXPORT_SYMBOL_GPL(mtk_vdec_slc_enable);

/* For vdec acp switch on/off */
bool mtk_vdec_acp_enable;
EXPORT_SYMBOL_GPL(mtk_vdec_acp_enable);

/* For vecn acp switch on/off */
bool mtk_venc_acp_enable;
EXPORT_SYMBOL_GPL(mtk_venc_acp_enable);

/* For vecn acp switch on/off */
bool mtk_venc_input_acp_enable;
EXPORT_SYMBOL_GPL(mtk_venc_input_acp_enable);

/* For vecn acp switch on/off */
int mtk_vdec_acp_debug;
EXPORT_SYMBOL_GPL(mtk_vdec_acp_debug);

struct vcu_v4l2_func vcu_func = { NULL };
EXPORT_SYMBOL_GPL(vcu_func);

int support_svp_region;
EXPORT_SYMBOL_GPL(support_svp_region);

int support_wfd_region;
EXPORT_SYMBOL_GPL(support_wfd_region);

int mtk_vcodec_get_chipid(struct mtk_chipid *chip_id)
{
	struct device_node *node;
	int len;
	char ver_name[20] = {0};
	struct mtk_chipid *chip_id_get;

	node = of_find_node_by_path("/chosen");
	if (!node)
		node = of_find_node_by_path("/chosen@0");
	if (!node) {
		mtk_v4l2_err("chosen node not found in device tree");
		return -ENODEV;
	}

	chip_id_get = (struct mtk_chipid *)of_get_property(node, "atag,chipid", &len);
	if (!chip_id_get) {
		mtk_v4l2_err("atag,chipid found in chosen node");
		return -ENODEV;
	}
	memcpy(chip_id, chip_id_get, sizeof(struct mtk_chipid));

	switch (chip_id->sw_ver) {
	case MTK_CHIP_SW_VER_E1:
		snprintf(ver_name, sizeof(ver_name), "E1");
		break;
	case MTK_CHIP_SW_VER_E2:
		snprintf(ver_name, sizeof(ver_name), "E2");
		break;
	default:
		snprintf(ver_name, sizeof(ver_name), "ver not support");
	}

	mtk_v4l2_debug(0, "chip sw version: %s(0x%x)", ver_name, chip_id->sw_ver);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_get_chipid);

bool mtk_vcodec_is_vcp(int type)
{
	if (type > MTK_INST_ENCODER || type < MTK_INST_DECODER)
		return false;
	return (mtk_vcodec_vcp & (1 << type));
}
EXPORT_SYMBOL_GPL(mtk_vcodec_is_vcp);

/* for check if ctx state is in specific state range, params means:
 * state_a & state_b != MTK_STATE_NULL: check if state_a <= ctx state = state_b,
 * state_a == MTK_STATE_NULL: check if ctx state <= state_b,
 * state_b == MTK_STATE_NULL: check if state_a <= ctx state,
 * state_a == state_b: check if ctx state == state_a/b
 */
bool mtk_vcodec_state_in_range(struct mtk_vcodec_ctx *ctx, int state_a, int state_b)
{
	unsigned long flags;

	if (!ctx)
		return false;

	spin_lock_irqsave(&ctx->state_lock, flags);
	if ((state_a == MTK_STATE_NULL || state_a <= ctx->state) &&
	    (state_b == MTK_STATE_NULL || ctx->state <= state_b)) {
		spin_unlock_irqrestore(&ctx->state_lock, flags);
		return true;
	}
	spin_unlock_irqrestore(&ctx->state_lock, flags);
	return false;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_state_in_range);

/* check if ctx state is specific state */
bool mtk_vcodec_is_state(struct mtk_vcodec_ctx *ctx, int state)
{
	return mtk_vcodec_state_in_range(ctx, state, state);
}
EXPORT_SYMBOL_GPL(mtk_vcodec_is_state);

int mtk_vcodec_get_state(struct mtk_vcodec_ctx *ctx)
{
	int state;
	unsigned long flags;

	if (!ctx)
		return MTK_STATE_FREE;

	spin_lock_irqsave(&ctx->state_lock, flags);
	state = ctx->state;
	spin_unlock_irqrestore(&ctx->state_lock, flags);
	return state;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_get_state);

/* if ctx state is specific state, then set state to target state */
int mtk_vcodec_set_state_from(struct mtk_vcodec_ctx *ctx, int target, int from)
{
	int state;
	unsigned long flags;

	if (!ctx)
		return MTK_STATE_FREE;

	spin_lock_irqsave(&ctx->state_lock, flags);
	state = ctx->state;
	if (ctx->state == from) {
		ctx->state = target;
		mtk_v4l2_debug(4, "[%d] set state %d from %d to %d",
			ctx->id, state, from, target);
	} else
		mtk_v4l2_debug(1, "[%d] set state %d from %d to %d fail",
			ctx->id, state, from, target);
	spin_unlock_irqrestore(&ctx->state_lock, flags);
	return state;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_set_state_from);

/* If ctx state is not except state, then set state to target state,
 * with checking for ABORT state only can be set to FREE state.
 * If except state is MTK_STATE_NULL, then will not check except state,
 * which means will only check if is ABORT state, otherwise just set to target state.
 */
int mtk_vcodec_set_state_except(struct mtk_vcodec_ctx *ctx, int target, int except_state)
{
	int state;
	unsigned long flags;

	if (!ctx)
		return MTK_STATE_FREE;

	spin_lock_irqsave(&ctx->state_lock, flags);
	state = ctx->state;
	if ((except_state == MTK_STATE_NULL || ctx->state != except_state) &&
	   (ctx->state != MTK_STATE_ABORT || target == MTK_STATE_FREE)) {
		ctx->state = target;
		mtk_v4l2_debug(4, "[%d] set state %d to %d (except %d)",
			ctx->id, state, target, except_state);
	} else
		mtk_v4l2_debug(1, "[%d] set state %d to %d fail (except %d)",
			ctx->id, state, target, except_state);
	spin_unlock_irqrestore(&ctx->state_lock, flags);
	return state;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_set_state_except);

int mtk_vcodec_set_state(struct mtk_vcodec_ctx *ctx, int target)
{
	return mtk_vcodec_set_state_except(ctx, target, MTK_STATE_NULL);
}
EXPORT_SYMBOL_GPL(mtk_vcodec_set_state);

/* disable on legacy platform by config dts */
int venc_disable_hw_break;
EXPORT_SYMBOL_GPL(venc_disable_hw_break);

void mtk_vcodec_register_trace(void *func)
{
	vcodec_trace_puts_ptr = func;
}
EXPORT_SYMBOL(mtk_vcodec_register_trace);

/* VCODEC FTRACE */
void vcodec_trace(const char *fmt, ...)
{
	char buf[256] = {0};
	va_list args;
	int len;

	va_start(args, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		buf[255] = '\0';

#ifdef MTK_VCODEC_DEBUG_SUPPORT
	trace_puts(buf);
#else
	if (vcodec_trace_puts_ptr)
		vcodec_trace_puts_ptr(buf);
#endif
}
EXPORT_SYMBOL(vcodec_trace);

void mtk_vcodec_in_out_trace_count(struct mtk_vcodec_ctx *ctx, unsigned int buf_type, bool in_kernel, int add_diff)
{
	bool is_input = V4L2_TYPE_IS_OUTPUT(buf_type);
	int trace_count;

	if (in_kernel) {
		trace_count = is_input ?
			v4l2_m2m_num_src_bufs_ready(ctx->m2m_ctx) : v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx);
	} else {
		if (is_input) {
			mtk_v4l2_debug(8, "[WARNING][%d] not counting input bufs in driver", ctx->id);
			return;
		}
		trace_count = atomic_add_return(add_diff, &ctx->output_cnt_in_driver);
	}

	vcodec_trace_tid_count(ctx->trace_count_tgid, trace_count,
		"%s-%d-%s_buf-in_%s", ctx->type == MTK_INST_DECODER ? "VDEC" : "VENC", ctx->id,
		is_input ? "in" : "out", in_kernel ? "kernel" : "driver");
}
EXPORT_SYMBOL_GPL(mtk_vcodec_in_out_trace_count);

void __iomem *mtk_vcodec_get_dec_reg_addr(struct mtk_vcodec_ctx *data,
	unsigned int reg_idx)
{
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)data;

	if (!data || reg_idx >= NUM_MAX_VDEC_REG_BASE) {
		mtk_v4l2_err("Invalid arguments, reg_idx=%d", reg_idx);
		return NULL;
	}
	return ctx->dev->dec_reg_base[reg_idx];
}
EXPORT_SYMBOL_GPL(mtk_vcodec_get_dec_reg_addr);

void __iomem *mtk_vcodec_get_enc_reg_addr(struct mtk_vcodec_ctx *data,
	unsigned int reg_idx)
{
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)data;

	if (!data || reg_idx >= NUM_MAX_VENC_REG_BASE) {
		mtk_v4l2_err("Invalid arguments, reg_idx=%d", reg_idx);
		return NULL;
	}
	return ctx->dev->enc_reg_base[reg_idx];
}
EXPORT_SYMBOL_GPL(mtk_vcodec_get_enc_reg_addr);


void mtk_vcodec_set_curr_ctx(struct mtk_vcodec_dev *dev,
	struct mtk_vcodec_ctx *ctx, unsigned int hw_id)
{
	unsigned long flags;

	if (dev == NULL || hw_id >= MTK_VDEC_HW_NUM) {
		mtk_v4l2_err("Invalid arguments, dev=0x%p, ctx=0x%p, hw_id=%d",
			dev, ctx, hw_id);
		return;
	}

	spin_lock_irqsave(&dev->irqlock, flags);
	dev->curr_dec_ctx[hw_id] = ctx;
	spin_unlock_irqrestore(&dev->irqlock, flags);
}
EXPORT_SYMBOL_GPL(mtk_vcodec_set_curr_ctx);

struct mtk_vcodec_ctx *mtk_vcodec_get_curr_ctx(struct mtk_vcodec_dev *dev,
	unsigned int hw_id)
{
	unsigned long flags;
	struct mtk_vcodec_ctx *ctx;

	if (!dev || hw_id >= MTK_VDEC_HW_NUM) {
		mtk_v4l2_err("Invalid arguments, dev=0x%p, hw_id=%d", dev, hw_id);
		return NULL;
	}

	spin_lock_irqsave(&dev->irqlock, flags);
	ctx = dev->curr_dec_ctx[hw_id];
	spin_unlock_irqrestore(&dev->irqlock, flags);
	return ctx;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_get_curr_ctx);

#ifdef MTK_VIDEO_GO_SUPPORT
static struct mtk_vcodec_ctx *mtk_vcodec_get_ctx_by_id(struct mtk_vcodec_dev *dev, unsigned int ctx_id)
{
	struct mtk_vcodec_ctx *ctx = NULL;

	if (dev == NULL)
		return NULL;

	mutex_lock(&dev->ctx_mutex);
	list_for_each_entry(ctx, &dev->ctx_list, list) {
		if (ctx != NULL && ctx != &dev->dev_ctx && ctx->id == ctx_id) {
			mutex_unlock(&dev->ctx_mutex);
			return ctx;
		}
	}
	mutex_unlock(&dev->ctx_mutex);
	return NULL;
}
#endif

void mtk_vcodec_add_ctx_list(struct mtk_vcodec_ctx *ctx)
{
	if (ctx != NULL) {
		mutex_lock(&ctx->dev->ctx_mutex);
		list_add(&ctx->list, &ctx->dev->ctx_list);
		mtk_vcodec_dump_ctx_list(ctx->dev, 4);
		if (ctx != ctx->dev_ctx)
			mtk_vcodec_alive_checker_init(ctx);
		mutex_unlock(&ctx->dev->ctx_mutex);
	}
}
EXPORT_SYMBOL_GPL(mtk_vcodec_add_ctx_list);

void mtk_vcodec_del_ctx_list(struct mtk_vcodec_ctx *ctx)
{
	if (ctx != NULL) {
		mutex_lock(&ctx->dev->ctx_mutex);
		mutex_lock(&ctx->ipi_use_lock);
		mtk_vcodec_dump_ctx_list(ctx->dev, 4);
		list_del_init(&ctx->list);
		if (ctx != ctx->dev_ctx)
			mtk_vcodec_alive_checker_deinit(ctx, mtk_vcodec_ctx_list_empty(ctx->dev));
		mutex_unlock(&ctx->ipi_use_lock);
		mutex_unlock(&ctx->dev->ctx_mutex);
	}
}
EXPORT_SYMBOL_GPL(mtk_vcodec_del_ctx_list);

bool mtk_vcodec_ctx_list_empty(struct mtk_vcodec_dev *dev)
{
	if (!dev)
		return true;

	// ctx_list is empty or only have dev_ctx
	if (list_empty(&dev->ctx_list) || (dev->ctx_list.next == &dev->dev_ctx.list))
		return true;

	return false;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_ctx_list_empty);

void mtk_vcodec_dump_ctx_list(struct mtk_vcodec_dev *dev, unsigned int debug_level)
{
	struct list_head *p, *q;
	struct mtk_vcodec_ctx *ctx;

	if (dev == NULL)
		return;

	list_for_each_safe(p, q, &dev->ctx_list) {
		ctx = list_entry(p, struct mtk_vcodec_ctx, list);
		if (ctx == NULL)
			mtk_v4l2_err("ctx null in ctx list");
		else
			mtk_v4l2_debug(debug_level, "[%d] %s ctx 0x%lx %p, drv_handle 0x%08lx %p, state %d",
				ctx->id, (ctx->type == MTK_INST_DECODER) ? "dec" : "enc",
				(unsigned long)ctx, ctx, ctx->drv_handle, (void *)ctx->drv_handle,
				mtk_vcodec_get_state(ctx));
	}
}
EXPORT_SYMBOL_GPL(mtk_vcodec_dump_ctx_list);

int mtk_vcodec_get_op_by_pid(enum mtk_instance_type type, int pid)
{
	struct mtk_vcodec_dev *dev = NULL;
	struct list_head *p, *q;
	struct mtk_vcodec_ctx *ctx;
	int fps = 0;

	if (type < MTK_INST_MAX && type >= 0)
		dev = dev_ptr[type];

	if (dev == NULL)
		return 0;

	mutex_lock(&dev->ctx_mutex);
	list_for_each_safe(p, q, &dev->ctx_list) {
		ctx = list_entry(p, struct mtk_vcodec_ctx, list);
		if (ctx != NULL && ctx->cpu_caller_pid == pid) {
			fps = ctx->op_rate_adaptive;
			mtk_v4l2_debug(2, "[%d] get fps %d by pid %d", ctx->id, fps, ctx->cpu_caller_pid);
		} else if (ctx != NULL)
			mtk_v4l2_debug(8, "[%d] pid %d, fps %d (not found pid %d)",
				ctx->id, ctx->cpu_caller_pid, ctx->op_rate_adaptive, pid);
		else
			mtk_v4l2_err("get NULL ctx in ctx list");
	}
	mutex_unlock(&dev->ctx_mutex);

	return fps;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_get_op_by_pid);

static void mtk_vcodec_set_uclamp(bool enable, int ctx_id, int pid)
{
	struct task_struct *p, *task_child;
	struct sched_attr attr = {};

	int ret = -1;

	attr.sched_policy = -1;
	attr.sched_flags =
		SCHED_FLAG_KEEP_ALL |
		SCHED_FLAG_UTIL_CLAMP |
		SCHED_FLAG_RESET_ON_FORK;
	attr.sched_util_max = -1;

	if(enable)
		attr.sched_util_min = 370;
	else
		attr.sched_util_min = -1;

	rcu_read_lock();
	p = find_task_by_vpid(pid);

	if (likely(p)) {
		get_task_struct(p);
		attr.sched_policy = p->policy;
		if(p->policy == SCHED_FIFO || p->policy == SCHED_RR)
			attr.sched_priority = p->rt_priority;
		ret = sched_setattr_nocheck(p, &attr);
		for_each_thread(p, task_child) {
			if(task_child) {
				get_task_struct(task_child);
				if(try_get_task_stack(task_child))
					ret = sched_setattr_nocheck(task_child, &attr);
				put_task_struct(task_child);
			}
		}
		put_task_struct(p);
		if(ret != 0)
			mtk_v4l2_err("[VDVFS][%d] set uclamp fail, pid: %d, ret: %d", ctx_id, pid, ret);
	}
	rcu_read_unlock();
};

void mtk_vcodec_set_cpu_hint(struct mtk_vcodec_dev *dev, bool enable,
	enum mtk_instance_type type, int ctx_id, int cpu_caller_pid, const char *debug_str)
{
	vcodec_trace_begin("%s(%d)(%s)", __func__, enable, debug_str);

	mutex_lock(&dev->cpu_hint_mutex);
	if (enable) {
		if (dev->cpu_hint_mode & (1 << MTK_GRP_AWARE_MODE)) { // cpu grp awr mode
			if (dev->cpu_hint_ref_cnt == 0) {
#ifdef MTK_SCHED_SUPPORT
				set_top_grp_aware(1, 0);
				set_grp_awr_min_opp_margin(0, 0, 2560);
				set_grp_awr_thr(0, 0, 1680000);
				set_grp_awr_min_opp_margin(1, 0, 2560);
				set_grp_awr_thr(1, 0, 2240000);
#endif
			}
		}
		if (dev->cpu_hint_mode & (1 << MTK_UCLAMP_MODE)) // uclamp mode
			mtk_vcodec_set_uclamp(enable, ctx_id, cpu_caller_pid);

		dev->cpu_hint_ref_cnt++;
		mtk_vcodec_dvfs_qos_log(true, "[VDVFS][%d][%s] enable CPU hint by %s (ref cnt %d, mode %d)",
			ctx_id, (type == MTK_INST_DECODER) ? "VDEC" : "VENC",
			debug_str, dev->cpu_hint_ref_cnt, dev->cpu_hint_mode);
	} else {
		dev->cpu_hint_ref_cnt--;
		if (dev->cpu_hint_mode & (1 << MTK_GRP_AWARE_MODE)) {
			if (dev->cpu_hint_ref_cnt == 0) {
#ifdef MTK_SCHED_SUPPORT
				set_top_grp_aware(0, 0);
#endif
			}
		}
		if (dev->cpu_hint_mode & (1 << MTK_UCLAMP_MODE))
			mtk_vcodec_set_uclamp(enable, ctx_id, cpu_caller_pid);

		mtk_vcodec_dvfs_qos_log(true, "[VDVFS][%d][%s] disable CPU hint by %s (ref cnt %d mode %d)",
			ctx_id, (type == MTK_INST_DECODER) ? "VDEC" : "VENC", debug_str,
			dev->cpu_hint_ref_cnt, dev->cpu_hint_mode);
	}
	mutex_unlock(&dev->cpu_hint_mutex);

	vcodec_trace_end();
}
EXPORT_SYMBOL_GPL(mtk_vcodec_set_cpu_hint);

void mtk_vcodec_init_slice_info(struct mtk_vcodec_ctx *ctx, struct mtk_video_dec_buf *dst_buf_info)
{
	struct vb2_v4l2_buffer *dst_vb2_v4l2;
	struct vdec_fb *pfb;
	struct dma_buf *dbuf;
	struct dma_fence *fence;

	if (ctx == NULL || dst_buf_info == NULL) {
		mtk_v4l2_err("Invalid arguments, ctx=0x%p, dst_buf_info=0x%p", ctx, dst_buf_info);
		return;
	}
	dst_vb2_v4l2 = &dst_buf_info->vb;
	pfb = &dst_buf_info->frame_buffer;
	dbuf = dst_vb2_v4l2->vb2_buf.planes[0].dbuf;

	pfb->slice_done_count = 0;
	fence = mtk_vcodec_create_fence(ctx->dec_params.slice_count);
	if (fence) {
		dma_resv_lock(dbuf->resv, NULL);
		dma_resv_add_fence(dbuf->resv, fence, DMA_RESV_USAGE_KERNEL);
		dma_resv_unlock(dbuf->resv);
		dma_fence_put(fence); // make dbuf->resv the only owner
	}
}
EXPORT_SYMBOL(mtk_vcodec_init_slice_info);

struct vdec_fb *mtk_vcodec_get_fb(struct mtk_vcodec_ctx *ctx)
{
	struct vb2_buffer *dst_vb = NULL;
	struct vdec_fb *pfb;
	struct mtk_video_dec_buf *dst_buf_info;
	struct vb2_v4l2_buffer *dst_vb2_v4l2;
	unsigned int num_planes;
	int i;

	if (!ctx) {
		mtk_v4l2_err("Ctx is NULL!");
		return NULL;
	}

	mtk_v4l2_debug_enter();

	mutex_lock(&ctx->buf_lock);
	if (ctx->input_driven)
		dst_vb2_v4l2 = v4l2_m2m_dst_buf_remove_check(ctx->m2m_ctx);
	else
		dst_vb2_v4l2 = v4l2_m2m_next_dst_buf(ctx->m2m_ctx);

	if (dst_vb2_v4l2 != NULL) {
		dst_vb = &dst_vb2_v4l2->vb2_buf;
		dst_buf_info = to_video_dec_buf(dst_vb2_v4l2);
		pfb = &dst_buf_info->frame_buffer;

		num_planes = dst_vb->num_planes;
		pfb->num_planes = num_planes;
		pfb->index = dst_vb->index;

		for (i = 0; i < num_planes; i++) {
			if (mtk_v4l2_dbg_level > 0)
				pfb->fb_base[i].va = vb2_plane_vaddr(dst_vb, i);
			pfb->fb_base[i].dma_addr = vb2_dma_contig_plane_dma_addr(dst_vb, i);
			pfb->fb_base[i].size = ctx->picinfo.fb_sz[i];
			pfb->fb_base[i].length = vb2_plane_size(dst_vb, i);
			pfb->fb_base[i].dmabuf = dst_vb->planes[i].dbuf;

			if (dst_buf_info->used == false) {
				if (pfb->fb_base[i].dmabuf)
					get_dma_buf(pfb->fb_base[i].dmabuf);
				mtk_v4l2_debug(4, "[%d][Ref cnt] id=%d Ref get dma %p",
					ctx->id, pfb->index, pfb->fb_base[i].dmabuf);
			}
		}
		pfb->status = FB_ST_INIT;
		dst_buf_info->used = true;
		mtk_vcodec_init_slice_info(ctx, dst_buf_info);
		ctx->fb_list[pfb->index + 1] = (uintptr_t)pfb;

		mtk_v4l2_debug(1, "[%d][FB_BUF] id=%d pfb=0x%lx VA=%p dma_addr[0]=%pad dma_addr[1]=%pad Size=%zx fd:%x, dma_general_buf = %p, dma_general_addr = %pad, general_buf_fd = %d, num_rdy_bufs=%d",
			ctx->id, dst_vb->index, (unsigned long)pfb, pfb->fb_base[0].va,
			&pfb->fb_base[0].dma_addr, &pfb->fb_base[1].dma_addr,
			pfb->fb_base[0].size, dst_vb->planes[0].m.fd,
			pfb->dma_general_buf, &pfb->dma_general_addr, pfb->general_buf_fd,
			v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx));
	} else {
		mtk_v4l2_debug(8, "[%d] No free framebuffer in v4l2!!\n", ctx->id);
		pfb = NULL;
	}
	mutex_unlock(&ctx->buf_lock);

	mtk_v4l2_debug_leave();

	return pfb;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_get_fb);

struct mtk_vcodec_mem *mtk_vcodec_get_bs(struct mtk_vcodec_ctx *ctx)
{
	struct vb2_buffer *dst_vb;
	struct mtk_vcodec_mem *pbs_buf;
	struct mtk_video_enc_buf *dst_buf_info;
	struct vb2_v4l2_buffer *dst_vb2_v4l2;

	if (!ctx) {
		mtk_v4l2_err("Ctx is NULL!");
		return NULL;
	}

	mtk_v4l2_debug_enter();
	dst_vb2_v4l2 = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
	if (dst_vb2_v4l2 == NULL) {
		mtk_v4l2_err("[%d] dst_buf empty!!", ctx->id);
		return NULL;
	}
	dst_vb = &dst_vb2_v4l2->vb2_buf;
	dst_buf_info = to_video_enc_buf(dst_vb2_v4l2);
	pbs_buf = &dst_buf_info->bs_buf;

	if (!ctx->enc_params.svp_mode && mtk_v4l2_dbg_level > 0)
		pbs_buf->va = vb2_plane_vaddr(dst_vb, 0);
	pbs_buf->dma_addr = vb2_dma_contig_plane_dma_addr(dst_vb, 0);
	pbs_buf->size = (size_t)vb2_plane_size(dst_vb, 0);
	pbs_buf->dmabuf = dst_vb->planes[0].dbuf;

	mtk_v4l2_debug(8, "[%d] index=%d, num_rdy_bufs=%d, dma_general_buf = %p, general_buf_fd = %d\n",
		ctx->id, dst_vb->index,
		v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx),
		pbs_buf->dma_general_buf,
		pbs_buf->general_buf_fd);
	mtk_v4l2_debug_leave();

	return pbs_buf;
}
EXPORT_SYMBOL(mtk_vcodec_get_bs);

int v4l2_m2m_buf_queue_check(struct v4l2_m2m_ctx *m2m_ctx,
		struct vb2_v4l2_buffer *vb2_v4l2)
{
	struct v4l2_m2m_buffer *b;
	struct mtk_vcodec_ctx *ctx = get_ctx_from_m2m(m2m_ctx);

	if (vb2_v4l2 == NULL) {
		mtk_v4l2_err("Invalid arguments, m2m_ctx=0x%p, vb2_v4l2=0x%p",
			m2m_ctx, vb2_v4l2);
		return -1;
	}
	b = container_of(vb2_v4l2, struct v4l2_m2m_buffer, vb);
	mtk_v4l2_debug(8, "[Debug] b %lx b->list.next %lx prev %lx %lx %lx\n",
		(unsigned long)b, (unsigned long)b->list.next, (unsigned long)b->list.prev,
		(unsigned long)LIST_POISON1, (unsigned long)LIST_POISON2);

	if (WARN_ON(IS_ERR_OR_NULL(m2m_ctx) ||
		(b->list.next != LIST_POISON1 && b->list.next) ||
		(b->list.prev != LIST_POISON2 && b->list.prev))) {
		v4l2_aee_print("b %lx next %lx prev %lx already in rdyq %lx %lx\n",
			(unsigned long)b, (unsigned long)b->list.next, (unsigned long)b->list.prev,
			(unsigned long)LIST_POISON1, (unsigned long)LIST_POISON2);
		return -1;
	}
	vcodec_trace_begin("%s(ts=%lld)", __func__, vb2_v4l2->vb2_buf.timestamp);
	v4l2_m2m_buf_queue(m2m_ctx, vb2_v4l2);
	vcodec_trace_end();
	mtk_vcodec_in_out_trace_count(ctx, vb2_v4l2->vb2_buf.type, true, 1);
	return 0;
}
EXPORT_SYMBOL_GPL(v4l2_m2m_buf_queue_check);

struct vb2_v4l2_buffer *v4l2_m2m_src_buf_remove_check(struct v4l2_m2m_ctx *m2m_ctx)
{
	struct mtk_vcodec_ctx *ctx = get_ctx_from_m2m(m2m_ctx);
	struct vb2_v4l2_buffer *vb2_v4l2;

	vb2_v4l2 = v4l2_m2m_src_buf_remove(m2m_ctx);
	if (vb2_v4l2 != NULL)
		mtk_vcodec_in_out_trace_count(ctx, vb2_v4l2->vb2_buf.type, true, -1);
	return vb2_v4l2;
}
EXPORT_SYMBOL_GPL(v4l2_m2m_src_buf_remove_check);

struct vb2_v4l2_buffer *v4l2_m2m_dst_buf_remove_check(struct v4l2_m2m_ctx *m2m_ctx)
{
	struct mtk_vcodec_ctx *ctx = get_ctx_from_m2m(m2m_ctx);
	struct vb2_v4l2_buffer *vb2_v4l2;

	vb2_v4l2 = v4l2_m2m_dst_buf_remove(m2m_ctx);
	if (vb2_v4l2 != NULL)
		mtk_vcodec_in_out_trace_count(ctx, vb2_v4l2->vb2_buf.type, true, -1);
	return vb2_v4l2;
}
EXPORT_SYMBOL_GPL(v4l2_m2m_dst_buf_remove_check);

int mtk_dma_sync_sg_range(const struct sg_table *sgt,
	struct device *dev, unsigned int size,
	enum dma_data_direction direction)
{
	struct sg_table *sgt_tmp;
	struct scatterlist *s_sgl, *d_sgl;
	unsigned int contig_size = 0;
	unsigned int sgl_len = 0;
	int ret, i;

	if (sgt == NULL || dev == NULL) {
		mtk_v4l2_err("sgt or dev is invalid");
		return -1;
	}
	if (size == 0) {
		mtk_v4l2_debug(1, "size %d no need to cache sync", size);
		return 0;
	}

	sgt_tmp = kzalloc(sizeof(*sgt_tmp), GFP_KERNEL);
	if (!sgt_tmp)
		return -1;

	ret = sg_alloc_table(sgt_tmp, sgt->orig_nents, GFP_KERNEL);
	if (ret) {
		mtk_v4l2_debug(0, "sg alloc table failed %d.\n", ret);
		kfree(sgt_tmp);
		return -1;
	}
	sgt_tmp->nents = 0;
	d_sgl = sgt_tmp->sgl;

	for_each_sg(sgt->sgl, s_sgl, sgt->orig_nents, i) {
		memcpy(d_sgl, s_sgl, sizeof(*s_sgl));
		contig_size += s_sgl->length;
		sgt_tmp->nents++;
		mtk_v4l2_debug(8, "%d contig_size %d bytesused %d.\n",
			i, contig_size, size);
		if (contig_size >= size) {
			sgl_len = PAGE_ALIGN(s_sgl->length - (contig_size - size));
			mtk_v4l2_debug(8, "trunc len from %u to %u.\n", s_sgl->length, sgl_len);
			sg_set_page(d_sgl, sg_page(s_sgl), sgl_len, s_sgl->offset);
			break;
		}
		d_sgl = sg_next(d_sgl);
	}
	if (direction == DMA_TO_DEVICE) {
		dma_sync_sg_for_device(dev, sgt_tmp->sgl, sgt_tmp->nents, direction);
	} else if (direction == DMA_FROM_DEVICE) {
		dma_sync_sg_for_cpu(dev, sgt_tmp->sgl, sgt_tmp->nents, direction);
	} else {
		mtk_v4l2_debug(0, "direction %d not correct\n", direction);
		sg_free_table(sgt_tmp);
		kfree(sgt_tmp);
		return -1;
	}
	mtk_v4l2_debug(8, "flush nents %d total nents %d\n",
		sgt_tmp->nents, sgt->orig_nents);
	sg_free_table(sgt_tmp);
	kfree(sgt_tmp);

	return 0;
}
EXPORT_SYMBOL(mtk_dma_sync_sg_range);

void v4l_fill_mtk_fmtdesc(struct v4l2_fmtdesc *fmt)
{
	const char *descr = NULL;

	if (fmt == NULL) {
		mtk_v4l2_err("Invalid arguments, fmt=0x%p", fmt);
		return;
	}

	switch (fmt->pixelformat) {
	case V4L2_PIX_FMT_HEVC:
	    descr = "H.265"; break;
	case V4L2_PIX_FMT_HEIF:
	    descr = "HEIF"; break;
	case V4L2_PIX_FMT_WMV1:
	    descr = "WMV1"; break;
	case V4L2_PIX_FMT_WMV2:
	    descr = "WMV2"; break;
	case V4L2_PIX_FMT_WMV3:
	    descr = "WMV3"; break;
	case V4L2_PIX_FMT_WVC1:
	    descr = "WVC1"; break;
	case V4L2_PIX_FMT_WMVA:
	    descr = "WMVA"; break;
	case V4L2_PIX_FMT_RV30:
	    descr = "RealVideo 8"; break;
	case V4L2_PIX_FMT_RV40:
	    descr = "RealVideo 9/10"; break;
	case V4L2_PIX_FMT_AV1:
	    descr = "AV1"; break;
	case V4L2_PIX_FMT_MT10S:
	    descr = "MTK 10-bit compressed single"; break;
	case V4L2_PIX_FMT_MT10:
	    descr = "MTK 10-bit compressed"; break;
	case V4L2_PIX_FMT_P010S:
	    descr = "10-bit P010 LSB 6-bit single"; break;
	case V4L2_PIX_FMT_P010M:
	    descr = "10-bit P010 LSB 6-bit"; break;
	case V4L2_PIX_FMT_NV12_AFBC:
	    descr = "AFBC NV12"; break;
	case V4L2_PIX_FMT_NV21_AFBC:
	    descr = "AFBC NV21"; break;
	case V4L2_PIX_FMT_NV12_10B_AFBC:
	    descr = "10-bit AFBC NV12"; break;
	case V4L2_PIX_FMT_RGB32_AFBC:
	    descr = "32-bit AFBC A/XRGB 8-8-8-8"; break;
	case V4L2_PIX_FMT_BGR32_AFBC:
	    descr = "32-bit AFBC A/XBGR 8-8-8-8"; break;
	case V4L2_PIX_FMT_RGBA1010102_AFBC:
	    descr = "10-bit AFBC RGB 2-bit for A"; break;
	case V4L2_PIX_FMT_BGRA1010102_AFBC:
	    descr = "10-bit AFBC BGR 2-bit for A"; break;
	case V4L2_PIX_FMT_ARGB1010102:
	case V4L2_PIX_FMT_ABGR1010102:
	case V4L2_PIX_FMT_RGBA1010102:
	case V4L2_PIX_FMT_BGRA1010102:
	    descr = "10-bit for RGB, 2-bit for A"; break;
	case V4L2_PIX_FMT_NV12_HYFBC:
		descr = "8-bit yuv 420 HyFBC"; break;
	case V4L2_PIX_FMT_P010_HYFBC:
		descr = "10-bit yuv 420 HyFBC"; break;
	case V4L2_PIX_FMT_MT21:
	case V4L2_PIX_FMT_MT2110T:
	case V4L2_PIX_FMT_MT2110R:
	case V4L2_PIX_FMT_MT21C10T:
	case V4L2_PIX_FMT_MT21C10R:
	case V4L2_PIX_FMT_MT21CS:
	case V4L2_PIX_FMT_MT21S:
	case V4L2_PIX_FMT_MT21S10T:
	case V4L2_PIX_FMT_MT21S10R:
	case V4L2_PIX_FMT_MT21CS10T:
	case V4L2_PIX_FMT_MT21CS10R:
	case V4L2_PIX_FMT_MT21CSA:
	case V4L2_PIX_FMT_MT21S10TJ:
	case V4L2_PIX_FMT_MT21S10RJ:
	case V4L2_PIX_FMT_MT21CS10TJ:
	case V4L2_PIX_FMT_MT21CS10RJ:
		descr = "Mediatek Video Block Format"; break;
	default:
		mtk_v4l2_debug(8, "MTK Unknown pixelformat 0x%08x\n", fmt->pixelformat);
		break;
	}

	if (descr)
		WARN_ON(strscpy(fmt->description, descr, sizeof(fmt->description)) < 0);
}
EXPORT_SYMBOL_GPL(v4l_fill_mtk_fmtdesc);

long mtk_vcodec_dma_attach_map(struct device *dev, struct dma_buf *dmabuf,
	struct dma_buf_attachment **att_ptr, struct sg_table **sgt_ptr, dma_addr_t *addr_ptr,
	enum dma_data_direction direction, const char *debug_str, int debug_line)
{
	struct dma_buf_attachment *buf_att = NULL;
	struct sg_table *sgt = NULL;
	dma_addr_t dma_addr = 0;
	long ret = 0;

	if (debug_str == NULL)
		debug_str = __func__;

	/* initialize */
	if (att_ptr != NULL)
		*att_ptr = NULL;
	if (sgt_ptr != NULL)
		*sgt_ptr = NULL;
	if (addr_ptr != NULL)
		*addr_ptr = 0;

	if (dev == NULL || IS_ERR_OR_NULL(dmabuf) || direction >= DMA_NONE) {
		mtk_v4l2_err("%s %d: invalid dev %p, dmabuf %ld, direction %d",
			debug_str, debug_line, dev, PTR_ERR(dmabuf), direction);
		return -EINVAL;
	}

	buf_att = dma_buf_attach(dmabuf, dev);
	if (IS_ERR_OR_NULL(buf_att)) {
		mtk_v4l2_err("%s %d: attach fail ret %ld", debug_str, debug_line, PTR_ERR(buf_att));
		if (IS_ERR(buf_att))
			ret = PTR_ERR(buf_att);
		else
			ret = -EFAULT;
		goto dma_attach_map_err;
	}
	sgt = dma_buf_map_attachment_unlocked(buf_att, direction);
	if (IS_ERR_OR_NULL(sgt)) {
		mtk_v4l2_err("%s %d: map fail ret %ld", debug_str, debug_line, PTR_ERR(sgt));
		if (IS_ERR(sgt))
			ret = PTR_ERR(sgt);
		else
			ret = -EFAULT;
		goto dma_attach_map_err_map_fail;
	}
	dma_addr = sg_dma_address(sgt->sgl);

	if (att_ptr != NULL)
		*att_ptr = buf_att;
	if (sgt_ptr != NULL)
		*sgt_ptr = sgt;
	if (addr_ptr != NULL)
		*addr_ptr = dma_addr;

	if (ret || sgt_ptr == NULL)
		dma_buf_unmap_attachment_unlocked(buf_att, sgt, direction);
dma_attach_map_err_map_fail:
	if (ret || att_ptr == NULL)
		dma_buf_detach(dmabuf, buf_att);
dma_attach_map_err:
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_dma_attach_map);

void mtk_vcodec_dma_unmap_detach(struct dma_buf *dmabuf,
	struct dma_buf_attachment **att_ptr, struct sg_table **sgt_ptr, enum dma_data_direction direction)
{
	if (sgt_ptr != NULL && !IS_ERR_OR_NULL(*sgt_ptr) &&
	    att_ptr != NULL && !IS_ERR_OR_NULL(*att_ptr) && direction < DMA_NONE)
		dma_buf_unmap_attachment_unlocked(*att_ptr, *sgt_ptr, direction);
	if (!IS_ERR_OR_NULL(dmabuf) && att_ptr != NULL && !IS_ERR_OR_NULL(*att_ptr))
		dma_buf_detach(dmabuf, *att_ptr);

	if (att_ptr != NULL)
		*att_ptr = NULL;
	if (sgt_ptr != NULL)
		*sgt_ptr = NULL;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_dma_unmap_detach);

#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
#define VCP_CACHE_LINE 128
int mtk_vcodec_alloc_mem(struct vcodec_mem_obj *mem, struct device *dev,
	struct dma_buf_attachment **attach, struct sg_table **sgt, enum mtk_instance_type fmt)
{
	struct dma_heap *dma_heap;
	struct dma_buf *dbuf;
	dma_addr_t dma_addr;
	__u32 alloc_len;
	long ret = 0;

	if (mem == NULL || dev == NULL || attach == NULL || sgt == NULL) {
		mtk_v4l2_err("Invalid arguments, mem=0x%p, dev=0x%p, attach=0x%p, sgt=0x%p",
			mem, dev, attach, sgt);
		return -EINVAL;
	}
	alloc_len = mem->len;

	mem->iova = mem->va = mem->pa = 0;
	if (dev == NULL) {
		mtk_v4l2_err("dev null when type %u", mem->type);
		return -EPERM;
	}
	if (mem->len > CODEC_ALLOCATE_MAX_BUFFER_SIZE || mem->len == 0U) {
		mtk_v4l2_err("buffer len = %u invalid", mem->len);
		return -EPERM;
	}

	if (mem->type == MEM_TYPE_FOR_SW ||
	    mem->type == MEM_TYPE_FOR_HW ||
	    mem->type == MEM_TYPE_FOR_UBE_HW) {
		if (mtk_vcodec_is_vcp(fmt))
			dma_heap = dma_heap_find("mtk_mm-uncached");
		else
			dma_heap = dma_heap_find("mtk_mm");
	} else if (mem->type == MEM_TYPE_FOR_HW_CACHE) {
		dma_heap = dma_heap_find("mtk_mm");
	} else if (mem->type == MEM_TYPE_FOR_SEC_SW ||
		   mem->type == MEM_TYPE_FOR_SEC_HW ||
		   mem->type == MEM_TYPE_FOR_SEC_UBE_HW) {
		if (support_svp_region)
			dma_heap = dma_heap_find("mtk_svp_region-aligned");
		else
			dma_heap = dma_heap_find("mtk_svp_page-uncached");
	} else if (mem->type == MEM_TYPE_FOR_SEC_WFD_HW) {
		if (support_wfd_region)
			dma_heap = dma_heap_find("mtk_wfd_region-aligned");
		else
			dma_heap = dma_heap_find("mtk_wfd_page-uncached");
	} else {
		mtk_v4l2_err("wrong type %u", mem->type);
		return -EPERM;
	}

	if (!dma_heap) {
		mtk_v4l2_err("heap find fail");
		return -EPERM;
	}

	if (mem->type == MEM_TYPE_FOR_SW ||
		mem->type == MEM_TYPE_FOR_SEC_SW) {
		alloc_len = (ROUND_N(mem->len, VCP_CACHE_LINE) + VCP_CACHE_LINE);
	}

	dbuf = dma_heap_buffer_alloc(dma_heap, alloc_len,
		O_RDWR | O_CLOEXEC, DMA_HEAP_VALID_HEAP_FLAGS);
	if (IS_ERR_OR_NULL(dbuf)) {
		mtk_v4l2_err("buffer alloc fail");
		ret = PTR_ERR(dbuf);
		goto alloc_mem_err;
	}

	if (fmt == MTK_INST_DECODER)
		mtk_dma_buf_set_name(dbuf, "vdec_working");
	else
		mtk_dma_buf_set_name(dbuf, "venc_working");

	ret = mtk_vcodec_dma_attach_map(dev, dbuf, attach, sgt, &dma_addr, DMA_BIDIRECTIONAL, __func__, __LINE__);
	if (ret || dma_addr == 0) {
		mtk_v4l2_err("alloc failed, va 0x%p 0x%llx pa %pad iova %pad len %d type %u",
			dbuf, (__u64)dbuf, &dma_addr, &dma_addr, mem->len, mem->type);
		goto alloc_mem_err_attach_map_fail;
	}
	mem->va = (__u64)dbuf;
	mem->pa = (__u64)dma_addr;
	mem->iova = mem->pa;
	mtk_v4l2_debug(8, "va 0x%p 0x%llx pa 0x%llx iova 0x%llx len %d type %u",
		dbuf, mem->va, mem->pa, mem->iova, mem->len, mem->type);

	return 0;

alloc_mem_err_attach_map_fail:
	dma_heap_buffer_free(dbuf);
alloc_mem_err:
	if (ret == 0)
		ret = -EPERM;
	return (int)ret;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_alloc_mem);

int mtk_vcodec_free_mem(struct vcodec_mem_obj *mem, struct device *dev,
	struct dma_buf_attachment *attach, struct sg_table *sgt)
{
	if (mem == NULL || IS_ERR_OR_NULL((void *)mem->va)) {
		mtk_v4l2_err("Invalid arguments, mem=0x%p, dev=0x%p, attach=0x%p, sgt=0x%p",
			mem, dev, attach, sgt);
		return -EINVAL;
	}

	if (mem->type < MEM_TYPE_MAX && mem->type != MEM_TYPE_FOR_SHM) {
		mtk_vcodec_dma_unmap_detach((struct dma_buf *)mem->va, &attach, &sgt, DMA_BIDIRECTIONAL);
		dma_heap_buffer_free((struct dma_buf *)mem->va);
	} else {
		mtk_v4l2_err("wrong type %d\n", mem->type);
		return -EPERM;
	}

	mtk_v4l2_debug(8, "va 0x%llx pa 0x%llx iova 0x%llx len %d\n",
		mem->va, mem->pa, mem->iova, mem->len);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_free_mem);

static unsigned char hyfbc_10bit_black_pattern_y[192] = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static unsigned char hyfbc_10bit_black_pattern_c[192] = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

int mtk_vcodec_vp_mode_buf_prepare(struct mtk_vcodec_dev *dev, int bitdepth)
{
	unsigned int j = 0;
	int i = 0;
	struct vdec_vp_mode_buf *src_buf;
	struct iosys_map map;
	void *va = NULL;
	struct dma_buf *dmabuf = NULL;
	struct device *io_dev = NULL;
	int ret;
	unsigned char *pattern;
	unsigned int pattern_len, copy_times;
	unsigned char short_pattern[4];
	int idx = (bitdepth == 8) ? 0 : 1;

	if (dev == NULL || (bitdepth != 8 && bitdepth != 10)) {
		mtk_v4l2_err("Invalid argument dev 0x%p, bitdepth %d", dev, bitdepth);
		return -1;
	}
	io_dev = dev->smmu_dev;

	mutex_lock(&dev->vp_mode_buf_mutex);
	for (i = 0; i < 3; i++) {
		switch (i) {
		case 0: // y data
			if (bitdepth == 10) {
				pattern = hyfbc_10bit_black_pattern_y;
				pattern_len = (unsigned int)sizeof(hyfbc_10bit_black_pattern_y);
			} else {
				memcpy(short_pattern, "\x10\x10\x10\x10", sizeof(short_pattern));
				pattern = short_pattern;
				pattern_len = (unsigned int)sizeof(short_pattern);
			}
			break;
		case 1: // c data
			if (bitdepth == 10) {
				pattern = hyfbc_10bit_black_pattern_c;
				pattern_len = (unsigned int)sizeof(hyfbc_10bit_black_pattern_c);
			} else {
				memcpy(short_pattern, "\x80\x80\x80\x80", sizeof(short_pattern));
				pattern = short_pattern;
				pattern_len = (unsigned int)sizeof(short_pattern);
			}
			break;
		case 2: // hyfbc len
			if (bitdepth == 10) {
				memcpy(short_pattern, "\xcf\x27\xcf\x27", sizeof(short_pattern));
				pattern = short_pattern;
				pattern_len = (unsigned int)sizeof(short_pattern);
			} else {
				memcpy(short_pattern, "\xbf\x1f\xbf\x1f", sizeof(short_pattern));
				pattern = short_pattern;
				pattern_len = (unsigned int)sizeof(short_pattern);
			}
			break;
		}

		src_buf = &dev->vp_mode_buf[idx][i];
		src_buf->mem.len = 192 * dev->vp_mode_buf_size_multi + 16;
		src_buf->mem.type = MEM_TYPE_FOR_HW_CACHE;

		ret = mtk_vcodec_alloc_mem(&src_buf->mem, io_dev, &src_buf->attach, &src_buf->sgt, MTK_INST_DECODER);
		if (ret) {
			mtk_v4l2_err("vp mode src_buf[%d][%d] alloc failed", idx, i);
			goto err_out;
		}

		/* mapping va */
		memset(&map, 0, sizeof(struct iosys_map));
		dmabuf = (struct dma_buf *)src_buf->mem.va;
		if (dma_buf_vmap_unlocked(dmabuf, &map)) {
			mtk_v4l2_err("vp mode src_buf[%d][%d] dma vmap failed", idx, i);
			goto err_out;
		}
		va = map.vaddr;

		/* fill working buffer */
		ret = dma_buf_begin_cpu_access(dmabuf, DMA_TO_DEVICE);
		if (ret)
			mtk_v4l2_err("vp mode src_buf[%d][%d] dma_buf_begin_cpu_access failed %d", idx, i, ret);
		copy_times = src_buf->mem.len / pattern_len;
		for (j = 0; j < copy_times; j++)
			memcpy((va + j * pattern_len), pattern, pattern_len);
		ret = dma_buf_end_cpu_access(dmabuf, DMA_TO_DEVICE);
		if (ret)
			mtk_v4l2_err("vp mode src_buf[%d][%d] dma_buf_end_cpu_access failed %d", idx, i, ret);

		dma_buf_vunmap_unlocked(dmabuf, &map);
	}
	mutex_unlock(&dev->vp_mode_buf_mutex);

	return 0;

err_out:
	for (i = 0; i < 3; i++) {
		src_buf = &dev->vp_mode_buf[idx][i];
		if (!IS_ERR_OR_NULL(ERR_PTR((long)src_buf->mem.va)))
			mtk_vcodec_free_mem(&src_buf->mem, io_dev, src_buf->attach, src_buf->sgt);
		memset(src_buf, 0, sizeof(struct vdec_vp_mode_buf));
	}
	mutex_unlock(&dev->vp_mode_buf_mutex);
	return -1;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_vp_mode_buf_prepare);

void mtk_vcodec_vp_mode_buf_unprepare(struct mtk_vcodec_dev *dev)
{
	struct vdec_vp_mode_buf *src_buf;
	int i, j;

	if (dev == NULL) {
		mtk_v4l2_err("Invalid argument");
		return;
	}

	mutex_lock(&dev->vp_mode_buf_mutex);
	for (i = 0; i < 2; i++) {
		mtk_v4l2_debug(2, "vp mode src_buf[%d] free y 0x%llx, c 0x%llx, len 0x%llx", i,
		    dev->vp_mode_buf[i][0].mem.iova, dev->vp_mode_buf[i][1].mem.iova, dev->vp_mode_buf[i][2].mem.iova);
		for (j = 0; j < 3; j++) {
			src_buf = &dev->vp_mode_buf[i][j];
			if (!IS_ERR_OR_NULL(ERR_PTR((long)src_buf->mem.va)))
				mtk_vcodec_free_mem(&src_buf->mem, dev->smmu_dev, src_buf->attach, src_buf->sgt);
			memset(src_buf, 0, sizeof(struct vdec_vp_mode_buf));
		}
	}
	mutex_unlock(&dev->vp_mode_buf_mutex);
}
EXPORT_SYMBOL_GPL(mtk_vcodec_vp_mode_buf_unprepare);
#endif

static void mtk_vcodec_sync_log(struct mtk_vcodec_dev *dev,
	const char *param_key, const char *param_val, enum mtk_vcodec_log_index index)
{
	struct mtk_vcodec_log_param *pram, *tmp;
	struct list_head *plist;
	struct mutex *plist_mutex;

	if (index == MTK_VCODEC_LOG_INDEX_LOG) {
		plist = &dev->log_param_list;
		plist_mutex = &dev->log_param_mutex;
	} else if (index == MTK_VCODEC_LOG_INDEX_PROP) {
		plist = &dev->prop_param_list;
		plist_mutex = &dev->prop_param_mutex;
	} else {
		mtk_v4l2_err("invalid index: %d", index);
		return;
	}

	mutex_lock(plist_mutex);

	list_for_each_entry(pram, plist, list) {
		// find existed param, replace its value
		if (strcmp(pram->param_key, param_key) == 0) {
			mtk_v4l2_debug(8, "replace old key: %s, value: %s -> %s\n",
				pram->param_key, pram->param_val, param_val);
			SNPRINTF(pram->param_val, LOG_PARAM_INFO_SIZE, "%s", param_val);
			// move to list head
			list_del(&pram->list);
			list_add(&pram->list, plist);
			mutex_unlock(plist_mutex);
			return;
		}
	}

	// cannot find, add new
	pram = vzalloc(sizeof(*pram));
	SNPRINTF(pram->param_key, LOG_PARAM_INFO_SIZE, "%s", param_key);
	SNPRINTF(pram->param_val, LOG_PARAM_INFO_SIZE, "%s", param_val);
	mtk_v4l2_debug(8, "add new key: %s, value: %s\n",
		pram->param_key, pram->param_val);
	list_add(&pram->list, plist);

	// remove disabled log param from list if value is empty
	list_for_each_entry_safe(pram, tmp, plist, list) {
		pram->param_val[LOG_PARAM_INFO_SIZE - 1] = 0;
		if (strlen(pram->param_val) == 0) {
			mtk_v4l2_debug(8, "remove deprecated key: %s, value: %s\n",
				pram->param_key, pram->param_val);
			list_del_init(&pram->list);
			vfree(pram);
		}
	}
	mutex_unlock(plist_mutex);
}

static void mtk_vcodec_build_log_string(struct mtk_vcodec_dev *dev,
	enum mtk_vcodec_log_index log_index)
{
	struct mtk_vcodec_log_param *pram;
	struct list_head *plist;
	struct mutex *plist_mutex;
	char *temp_str;
	char temp_info[LOG_PARAM_INFO_SIZE * 2 + 2] = {0};
	int str_len = 0, temp_info_len = 0, temp_len, ret;

	if (log_index == MTK_VCODEC_LOG_INDEX_LOG) {
		plist = &dev->log_param_list;
		plist_mutex = &dev->log_param_mutex;
		if (dev->vfd_dec)
			temp_str = mtk_vdec_tmp_log;
		else
			temp_str = mtk_venc_tmp_log;
	} else if (log_index == MTK_VCODEC_LOG_INDEX_PROP) {
		plist = &dev->prop_param_list;
		plist_mutex = &dev->prop_param_mutex;
		if (dev->vfd_dec)
			temp_str = mtk_vdec_tmp_prop;
		else
			temp_str = mtk_venc_tmp_prop;
	} else {
		mtk_v4l2_err("invalid log_index: %d", log_index);
		return;
	}

	mutex_lock(plist_mutex);

	memset(temp_str, 0x00, LOG_PROPERTY_SIZE);
	str_len = 0;
	list_for_each_entry(pram, plist, list) {
		mtk_v4l2_debug(8, "existed log param: %s %s\n",
				pram->param_key, pram->param_val);
		ret = snprintf(temp_info, LOG_PARAM_INFO_SIZE * 2 + 2,
			"%s %s", pram->param_key, pram->param_val);
		if (ret <= 0) {
			mtk_v4l2_err("temp_info err usage: snprintf ret %d", ret);
			continue;
		}
		temp_info_len = strlen(temp_info);
		if (temp_info_len > 0 && str_len + 1 + temp_info_len < LOG_PROPERTY_SIZE) {
			if (str_len == 0)
				temp_len = snprintf(temp_str, LOG_PROPERTY_SIZE, "%s", temp_info);
			else
				temp_len = snprintf(temp_str + str_len, LOG_PROPERTY_SIZE - str_len,
					" %s", temp_info);
			if (temp_len > 0)
				str_len += temp_len;
			else
				mtk_v4l2_err("temp_str err usage: temp_len: %d", temp_len);
		} else
			mtk_v4l2_err("temp_str err usage: str_len: %d, temp_info_len: %d",
				str_len, temp_info_len);
	}

	if (dev->vfd_dec) {
		if (log_index == MTK_VCODEC_LOG_INDEX_LOG) {
			mtk_vdec_vcp_log = temp_str;
			mtk_v4l2_debug(8, "build mtk_vdec_vcp_log: %s\n", mtk_vdec_vcp_log);
		} else if (log_index == MTK_VCODEC_LOG_INDEX_PROP) {
			mtk_vdec_property = temp_str;
			mtk_v4l2_debug(8, "build mtk_vdec_property: %s\n", mtk_vdec_property);
		}
	} else {
		if (log_index == MTK_VCODEC_LOG_INDEX_LOG) {
			mtk_venc_vcp_log = temp_str;
			mtk_v4l2_debug(8, "build mtk_venc_vcp_log: %s\n", mtk_venc_vcp_log);
		} else if (log_index == MTK_VCODEC_LOG_INDEX_PROP) {
			mtk_venc_property = temp_str;
			mtk_v4l2_debug(8, "build mtk_venc_property: %s\n", mtk_venc_property);
		}
	}
	mutex_unlock(plist_mutex);
}

void mtk_vcodec_set_log(struct mtk_vcodec_ctx *ctx, struct mtk_vcodec_dev *dev,
	const char *val, enum mtk_vcodec_log_index log_index,
	void (*set_vcu_vpud_log)(struct mtk_vcodec_ctx *ctx, void *in))
{
	int i, argc = 0;
	char (*argv)[LOG_PARAM_INFO_SIZE] = NULL;
	char *temp = NULL;
	char *token = NULL;
	long temp_val = 0;
	char *log = NULL;
	char vcu_log[128] = {0};

	if (dev == NULL || val == NULL || strlen(val) == 0)
		return;

	mtk_v4l2_debug(0, "val: %s, log_index: %d", val, log_index);

	argv = vzalloc(MAX_SUPPORTED_LOG_PARAMS_COUNT * 2 * LOG_PARAM_INFO_SIZE);
	if (!argv)
		return;
	log = vzalloc(LOG_PROPERTY_SIZE);
	if (!log) {
		vfree(argv);
		return;
	}

	SNPRINTF(log, LOG_PROPERTY_SIZE, "%s", val);
	temp = log;
	for (token = strsep(&temp, "\n\r ");
	     token != NULL && argc < MAX_SUPPORTED_LOG_PARAMS_COUNT * 2;
	     token = strsep(&temp, "\n\r ")) {
		if (strlen(token) == 0)
			continue;
		SNPRINTF(argv[argc], LOG_PARAM_INFO_SIZE, "%s", token);
		argc++;
	}

	for (i = 0; i < argc-1; i += 2) {
		if (strlen(argv[i]) == 0)
			continue;
		if (strcmp("-mtk_vcodec_dbg", argv[i]) == 0) {
			if (kstrtol(argv[i+1], 0, &temp_val) == 0) {
				mtk_vcodec_dbg = temp_val;
				mtk_v4l2_debug(0, "mtk_vcodec_dbg: %d\n", mtk_vcodec_dbg);
			}
		} else if (strcmp("-mtk_vcodec_perf", argv[i]) == 0) {
			if (kstrtol(argv[i+1], 0, &temp_val) == 0) {
				mtk_vcodec_perf = temp_val;
				mtk_v4l2_debug(0, "mtk_vcodec_perf: %d\n", mtk_vcodec_perf);
			}
		} else if (strcmp("-mtk_v4l2_dbg_level", argv[i]) == 0) {
			if (kstrtol(argv[i+1], 0, &temp_val) == 0) {
				mtk_v4l2_dbg_level = temp_val;
				mtk_v4l2_debug(0, "mtk_v4l2_dbg_level: %d\n", mtk_v4l2_dbg_level);
			}
		} else if (strcmp("-mtk_vcodec_dvfs_qos_log_en", argv[i]) == 0) {
			if (kstrtol(argv[i+1], 0, &temp_val) == 0) {
				mtk_vcodec_dvfs_qos_log_en = temp_val;
				mtk_v4l2_debug(0, "mtk_vcodec_dvfs_qos_log_en: %d\n", mtk_vcodec_dvfs_qos_log_en);
			}
		} else {
			// uP path
			if ((dev->vfd_dec && mtk_vcodec_is_vcp(MTK_INST_DECODER))
				|| (dev->vfd_enc && mtk_vcodec_is_vcp(MTK_INST_ENCODER))) {
				mtk_vcodec_sync_log(dev, argv[i], argv[i+1], log_index);
			} else { // vcu path
				if (ctx == NULL) {
					mtk_v4l2_err("ctx is null, cannot set log to vpud");
					vfree(argv);
					vfree(log);
					return;
				}
				if (log_index != MTK_VCODEC_LOG_INDEX_LOG) {
					mtk_v4l2_err(
						"invalid index: %d, only support set log on vcu path",
						log_index);
					vfree(argv);
					vfree(log);
					return;
				}
				memset(vcu_log, 0x00, sizeof(vcu_log));
				SNPRINTF(vcu_log, sizeof(vcu_log), "%s %s", argv[i], argv[i+1]);
				if (set_vcu_vpud_log != NULL)
					set_vcu_vpud_log(ctx, vcu_log);
			}
		}
	}

	// uP path
	if (mtk_vcodec_is_vcp(MTK_INST_DECODER) || mtk_vcodec_is_vcp(MTK_INST_ENCODER))
		mtk_vcodec_build_log_string(dev, log_index);

	vfree(argv);
	vfree(log);
}
EXPORT_SYMBOL_GPL(mtk_vcodec_set_log);

void mtk_vcodec_get_log(struct mtk_vcodec_ctx *ctx, struct mtk_vcodec_dev *dev,
	char *val, enum mtk_vcodec_log_index log_index,
	void (*get_vcu_vpud_log)(struct mtk_vcodec_ctx *ctx, void *out))
{
	int len = 0;

	if (!dev || !val) {
		mtk_v4l2_err("Invalid arguments, dev=0x%p, val=0x%p", dev, val);
		return;
	}

	memset(val, 0x00, LOG_PROPERTY_SIZE);

	if ((dev->vfd_dec && mtk_vcodec_is_vcp(MTK_INST_DECODER))
		|| (dev->vfd_enc && mtk_vcodec_is_vcp(MTK_INST_ENCODER))) { // uP path
		if (dev->vfd_dec) {
			if (log_index == MTK_VCODEC_LOG_INDEX_LOG) {
				SNPRINTF(val, LOG_PROPERTY_SIZE, "%s", mtk_vdec_vcp_log);
			} else if (log_index == MTK_VCODEC_LOG_INDEX_PROP) {
				SNPRINTF(val, LOG_PROPERTY_SIZE, "%s", mtk_vdec_property);
			} else {
				mtk_v4l2_err("invalid index: %d", log_index);
				return;
			}
		} else {
			if (log_index == MTK_VCODEC_LOG_INDEX_LOG) {
				SNPRINTF(val, LOG_PROPERTY_SIZE, "%s", mtk_venc_vcp_log);
			} else if (log_index == MTK_VCODEC_LOG_INDEX_PROP) {
				SNPRINTF(val, LOG_PROPERTY_SIZE, "%s", mtk_venc_property);
			} else {
				mtk_v4l2_err("invalid index: %d", log_index);
				return;
			}
		}
	} else { // vcu path
		if (ctx == NULL) {
			mtk_v4l2_err("ctx is null, cannot set log to vpud");
			return;
		}
		if (log_index != MTK_VCODEC_LOG_INDEX_LOG) {
			mtk_v4l2_err(
				"invalid index: %d, only support get log on vcu path", log_index);
			return;
		}

		if (get_vcu_vpud_log != NULL)
			get_vcu_vpud_log(ctx, val);
	}

	// join kernel log level
	if (log_index == MTK_VCODEC_LOG_INDEX_LOG) {
		len = strlen(val);
		if (len < LOG_PROPERTY_SIZE)
			SNPRINTF(val + len, LOG_PROPERTY_SIZE - len,
				" %s %d", "-mtk_vcodec_dbg", mtk_vcodec_dbg);

		len = strlen(val);
		if (len < LOG_PROPERTY_SIZE)
			SNPRINTF(val + len, LOG_PROPERTY_SIZE - len,
				" %s %d", "-mtk_vcodec_perf", mtk_vcodec_perf);

		len = strlen(val);
		if (len < LOG_PROPERTY_SIZE)
			SNPRINTF(val + len, LOG_PROPERTY_SIZE - len,
				" %s %d", "-mtk_v4l2_dbg_level", mtk_v4l2_dbg_level);

		len = strlen(val);
		if (len < LOG_PROPERTY_SIZE)
			SNPRINTF(val + len, LOG_PROPERTY_SIZE - len,
				" %s %d", "-mtk_vcodec_dvfs_qos_log_en", mtk_vcodec_dvfs_qos_log_en);
	}

	mtk_v4l2_debug(0, "val: %s, log_index: %d", val, log_index);
}
EXPORT_SYMBOL_GPL(mtk_vcodec_get_log);

/* utility functions for video go*/
void mtk_vcodec_vgo_send(int type, void *data)
{
#ifdef MTK_VIDEO_GO_SUPPORT
	struct mtk_vcodec_ctx *ctx;

	switch (type) {
	case VGO_SEND_UPDATE_FN:
		vcodec_to_vgo = data;
		mtk_v4l2_debug(0, "VGO_SEND_UPDATE_FN 0x%p (type %d)", data, type);
		break;
	case VGO_SEND_OPRATE: {
		struct vgo_data *vgo_info = (struct vgo_data *)data;
		struct oprate_data *op_data;
		int i;

		for (i = 0; i < vgo_info->count; i++) {
			op_data = &vgo_info->data[i];
			if (op_data->inst_type >= MTK_INST_MAX || op_data->inst_type < 0) {
				mtk_v4l2_err("VGO_SEND_OPRATE data[%d] inst_type %d invalid (ctx_id %d, adaptive_oprate %d)",
					i, op_data->inst_type, op_data->ctx_id, op_data->oprate);
				continue;
			}
			ctx = mtk_vcodec_get_ctx_by_id(dev_ptr[op_data->inst_type], op_data->ctx_id);
			if (!ctx) {
				mtk_v4l2_err("VGO_SEND_OPRATE data[%d] inst_type %d ctx_id %d, but ctx not found (adaptive_oprate %d)",
					i, op_data->inst_type, op_data->ctx_id, op_data->oprate);
				continue;
			}
			ctx->vgo_op_rate = (unsigned int)op_data->oprate;
			mtk_v4l2_debug(0, "[%d] %s VGO_SEND_OPRATE data[%d] %d",
				ctx->id, INST_TYPE_STR(ctx->type), i, ctx->vgo_op_rate);
		}
		break;
	}
	default:
		mtk_v4l2_err("type %d not support", type);
	}
#endif
}
EXPORT_SYMBOL_GPL(mtk_vcodec_vgo_send);

void mtk_vcodec_send_info_to_vgo(struct mtk_vcodec_ctx *ctx, enum mtk_vcodec_send_vgo_type type)
{
#ifdef MTK_VIDEO_GO_SUPPORT
	int vgo_type = -1;

	if (!vcodec_to_vgo) {
		mtk_v4l2_debug(1, "[%d] v4l2_to_vgo not exist (type %d)", ctx->id, type);
		return;
	}

	switch (type) {
	case MTK_VCODEC_VGO_OPEN: {
		struct oprate_data data = {0};

		vgo_type = VGO_RECV_STATE_OPEN;
		data.inst_type = ctx->type;
		data.ctx_id    = ctx->id;

		mtk_v4l2_debug(2, "[%d] vgo open (type %d,%d): inst_type %s(%d)",
			ctx->id, type, vgo_type, INST_TYPE_STR(data.inst_type), data.inst_type);
		vcodec_to_vgo(vgo_type, &data);
		break;
	}
	case MTK_VCODEC_VGO_ADD_INST:
		vgo_type = VGO_RECV_INSTANCE_INC;
		goto vgo_set_instance_data;
	case MTK_VCODEC_VGO_DEL_INST:
		vgo_type = VGO_RECV_INSTANCE_DEC;
vgo_set_instance_data:
	{
		struct inst_init_data data = {0};

		data.inst_type  = ctx->type;
		data.ctx_id     = ctx->id;
		data.caller_pid = ctx->cpu_caller_pid;
		if (ctx->type == MTK_INST_DECODER) {
			data.fourcc = ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc;
			data.oprate = (int)ctx->dec_params.operating_rate;
			data.width  = (int)ctx->last_decoded_picinfo.pic_w;
			data.height = (int)ctx->last_decoded_picinfo.pic_h;
		} else {
			data.fourcc = ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc;
			data.oprate = (int)ctx->enc_params.operationrate;
			data.width  = (int)ctx->q_data[MTK_Q_DATA_SRC].visible_width;
			data.height = (int)ctx->q_data[MTK_Q_DATA_SRC].visible_height;
		}
		mtk_v4l2_debug(2, "[%d] vgo %s inst (type %d,%d): inst_type %s(%d), caller_pid %d, fourcc %s(0x%x), oprate %d, w/h %d,%d",
			ctx->id, (type == MTK_VCODEC_VGO_ADD_INST) ? "add" : "del", type, vgo_type,
			INST_TYPE_STR(data.inst_type), data.inst_type, data.caller_pid,
			FOURCC_STR(data.fourcc), data.fourcc, data.oprate, data.width, data.height);
		vcodec_to_vgo(vgo_type, &data);
		break;
	}
	case MTK_VCODEC_VGO_UPDATE: {
		struct inst_data data = {0};
		int i, hw_num = 0;

		if (ctx->type == MTK_INST_DECODER) {
			if (ctx->dev->vdec_hw_ipm == VCODEC_IPM_V1)
				hw_num = 1; // core
			else if (ctx->dev->vdec_hw_ipm == VCODEC_IPM_V2)
				hw_num = 2; // lat + core
		} else if (ctx->type == MTK_INST_ENCODER)
			hw_num = (ctx->dev->hw_max_count <= 3) ? ctx->dev->hw_max_count : 3;

		vgo_type = VGO_RECV_RUNNING_UPDATE;
		data.inst_type = ctx->type;
		data.ctx_id    = ctx->id;
		data.oprate    = ctx->op_rate_adaptive;
		for (i = 0; i < hw_num; i++)
			data.hw_proc_time[i] = ctx->hw_proc_time[i];

		mtk_v4l2_debug(2, "[%d] vgo update (type %d,%d): inst_type %s(%d), oprate %d,hw_proc_time %d,%d,%d",
			ctx->id, type, vgo_type, INST_TYPE_STR(data.inst_type), data.inst_type, data.oprate,
			data.hw_proc_time[0], data.hw_proc_time[1], data.hw_proc_time[2]);
		vcodec_to_vgo(vgo_type, &data);
		break;
	}
	default:
		mtk_v4l2_err("[%d] type %d not support", ctx->id, type);
	}
#endif
}
EXPORT_SYMBOL_GPL(mtk_vcodec_send_info_to_vgo);

MODULE_IMPORT_NS(DMA_BUF);
MODULE_LICENSE("GPL v2");
