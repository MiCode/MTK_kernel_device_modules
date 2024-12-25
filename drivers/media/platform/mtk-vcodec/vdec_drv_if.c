// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: PC Chen <pc.chen@mediatek.com>
 *         Tiffany Lin <tiffany.lin@mediatek.com>
 */

#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include "vdec_drv_if.h"
#include "mtk_vcodec_dec.h"
#include "vdec_drv_base.h"
#include "mtk_vcodec_dec_pm.h"
#include "mtk_vcodec_dec_pm_plat.h"

#if IS_ENABLED(CONFIG_VIDEO_MEDIATEK_VCU)
#include "mtk_vcu.h"
const struct vdec_common_if *get_dec_vcu_if(void);
#endif

#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
const struct vdec_common_if *get_dec_vcp_if(void);
#endif


static const struct vdec_common_if *get_data_path_ptr(void)
{
#if IS_ENABLED(CONFIG_VIDEO_MEDIATEK_VCU)
	if (VCU_FPTR(vcu_get_plat_device)) {
#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
		if (mtk_vcodec_is_vcp(MTK_INST_DECODER))
			return get_dec_vcp_if();
#endif
		return get_dec_vcu_if();
	}
#endif
#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
	return get_dec_vcp_if();
#else
	return NULL;
#endif
}

int vdec_if_init(struct mtk_vcodec_ctx *ctx, unsigned int fourcc)
{
	int ret = 0;

	vcodec_trace_begin_func();

	mtk_dec_init_ctx_pm(ctx);

	switch (fourcc) {
	case V4L2_PIX_FMT_H264:
	case V4L2_PIX_FMT_H265:
	case V4L2_PIX_FMT_HEIF:
	case V4L2_PIX_FMT_MPEG1:
	case V4L2_PIX_FMT_MPEG2:
	case V4L2_PIX_FMT_MPEG4:
	case V4L2_PIX_FMT_H263:
	case V4L2_PIX_FMT_VP8:
	case V4L2_PIX_FMT_VP9:
	case V4L2_PIX_FMT_WMV1:
	case V4L2_PIX_FMT_WMV2:
	case V4L2_PIX_FMT_WMV3:
	case V4L2_PIX_FMT_WVC1:
	case V4L2_PIX_FMT_WMVA:
	case V4L2_PIX_FMT_RV30:
	case V4L2_PIX_FMT_RV40:
	case V4L2_PIX_FMT_AV1:
		ctx->dec_if = get_data_path_ptr();
		break;
	default:
		vcodec_trace_end();
		return -EINVAL;
	}

	if (ctx->dec_if == NULL) {
		vcodec_trace_end();
		return -EINVAL;
	}

	ret = ctx->dec_if->init(ctx, &ctx->drv_handle);

	vcodec_trace_end();
	return ret;
}

int vdec_if_decode(struct mtk_vcodec_ctx *ctx, struct mtk_vcodec_mem *bs,
				   struct vdec_fb *fb, unsigned int *src_chg)
{
	int ret = 0;
	unsigned int i = 0;

	vcodec_trace_begin_func();

	if (bs && !ctx->dec_params.svp_mode) {
		if ((bs->dma_addr & 63UL) != 0UL) {
			mtk_v4l2_err("bs dma_addr should 64 byte align");
			vcodec_trace_end();
			return -EINVAL;
		}
	}

	if (fb && !ctx->dec_params.svp_mode) {
		for (i = 0; i < fb->num_planes; i++) {
			if ((fb->fb_base[i].dma_addr & 511UL) != 0UL) {
				mtk_v4l2_err("fb addr should 512 byte align");
				vcodec_trace_end();
				return -EINVAL;
			}
		}
	}

	if (ctx->drv_handle == 0) {
		vcodec_trace_end();
		return -EIO;
	}

	ret = ctx->dec_if->decode(ctx->drv_handle, bs, fb, src_chg);

	vcodec_trace_end();
	return ret;
}

int vdec_if_get_param(struct mtk_vcodec_ctx *ctx, enum vdec_get_param_type type,
					  void *out)
{
	struct vdec_inst *inst = NULL;
	int ret = 0;
	bool drv_handle_exist = true;
	bool is_query_cap = (type == GET_PARAM_VDEC_CAP_SUPPORTED_FORMATS ||
			     type == GET_PARAM_VDEC_CAP_FRAME_SIZES);

	vcodec_trace_begin_func();

	if (!ctx->drv_handle && is_query_cap) {
		inst = kzalloc(sizeof(struct vdec_inst), GFP_KERNEL);
		if (inst == NULL) {
			vcodec_trace_end();
			return -ENOMEM;
		}
		inst->ctx = ctx;
		inst->vcu.ctx = ctx;
		ctx->drv_handle = (unsigned long)(inst);
		ctx->dec_if = get_data_path_ptr();
		mtk_vcodec_add_ctx_list(ctx);
		drv_handle_exist = false;
	}

	if (ctx->dec_if && ctx->drv_handle)
		ret = ctx->dec_if->get_param(ctx->drv_handle, type, out);
	else
		ret = -EINVAL;

	if (!drv_handle_exist) {
		inst->vcu.abort = true;
		mtk_vcodec_del_ctx_list(ctx);
		if (ctx->drv_handle == (unsigned long)inst)
			ctx->drv_handle = 0;
		kfree(inst);
	}

	vcodec_trace_end();
	return ret;
}

int vdec_if_set_param(struct mtk_vcodec_ctx *ctx, enum vdec_set_param_type type,
					  void *in)
{
	struct vdec_inst *inst = NULL;
	int ret = 0;
	bool drv_handle_exist = true;
	bool is_set_prop = (type == SET_PARAM_VDEC_PROPERTY ||
			    type == SET_PARAM_VDEC_VCP_LOG_INFO ||
			    type == SET_PARAM_VDEC_VCU_VPUD_LOG);

	vcodec_trace_begin_func();

	if (!ctx->drv_handle && is_set_prop) {
		inst = kzalloc(sizeof(struct vdec_inst), GFP_KERNEL);
		if (inst == NULL) {
			vcodec_trace_end();
			return -ENOMEM;
		}
		inst->ctx = ctx;
		inst->vcu.ctx = ctx;
		ctx->drv_handle = (unsigned long)(inst);
		ctx->dec_if = get_data_path_ptr();
		mtk_vcodec_add_ctx_list(ctx);
		drv_handle_exist = false;
	}

	if (ctx->dec_if && ctx->drv_handle)
		ret = ctx->dec_if->set_param(ctx->drv_handle, type, in);
	else
		ret = -EINVAL;

	if (!drv_handle_exist) {
		inst->vcu.abort = true;
		mtk_vcodec_del_ctx_list(ctx);
		if (ctx->drv_handle == (unsigned long)inst)
			ctx->drv_handle = 0;
		kfree(inst);
	}

	vcodec_trace_end();
	return ret;
}

void vdec_if_deinit(struct mtk_vcodec_ctx *ctx)
{
	if (ctx->drv_handle == 0)
		return;

	vcodec_trace_begin_func();

	ctx->dec_if->deinit(ctx->drv_handle);
	ctx->drv_handle = 0;

	vcodec_trace_end();
}

int vdec_if_flush(struct mtk_vcodec_ctx *ctx, struct mtk_vcodec_mem *bs,
				   struct vdec_fb *fb, enum vdec_flush_type type)
{
	int ret;

	if (ctx->drv_handle == 0)
		return -EIO;

	vcodec_trace_begin_func();

	if (ctx->dec_if->flush == NULL) {
		unsigned int src_chg;

		ret = vdec_if_decode(ctx, bs, fb, &src_chg);
	} else
		ret = ctx->dec_if->flush(ctx->drv_handle, fb, type);

	vcodec_trace_end();
	return ret;
}

void vdec_decode_prepare(void *ctx_prepare,
	unsigned int hw_id)
{
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)ctx_prepare;
	int ret;

	if (ctx == NULL || hw_id >= MTK_VDEC_HW_NUM)
		return;

	mutex_lock(&ctx->hw_status);
	ret = mtk_vdec_lock(ctx, hw_id);
	mtk_vcodec_set_curr_ctx(ctx->dev, ctx, hw_id);

	mtk_vcodec_dec_clock_on(&ctx->dev->pm, hw_id);

	if (ret == 0 && !mtk_vcodec_is_vcp(MTK_INST_DECODER))
		enable_irq(ctx->dev->dec_irq[hw_id]);
	mtk_vdec_dvfs_begin_frame(ctx, hw_id);
	mtk_vdec_pmqos_begin_frame(ctx);
	if (hw_id == MTK_VDEC_CORE)
		vcodec_trace_count("VDEC_HW_CORE", 1);
	else
		vcodec_trace_count("VDEC_HW_LAT", 1);
	mutex_unlock(&ctx->hw_status);
}

void vdec_decode_unprepare(void *ctx_unprepare,
	unsigned int hw_id)
{
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)ctx_unprepare;

	if (ctx == NULL || hw_id >= MTK_VDEC_HW_NUM)
		return;

	mutex_lock(&ctx->hw_status);
	if (ctx->dev->vdec_reg) // per frame mmdvfs in AP
		mtk_vdec_dvfs_end_frame(ctx, hw_id);
	mtk_vdec_pmqos_end_frame(ctx);
	if (ctx->dev->dec_sem[hw_id].count != 0) {
		mtk_v4l2_debug(0, "HW not prepared, dec_sem[%d].count = %d",
			hw_id, ctx->dev->dec_sem[hw_id].count);
		mutex_unlock(&ctx->hw_status);
		return;
	}
	if (hw_id == MTK_VDEC_CORE)
		vcodec_trace_count("VDEC_HW_CORE", 0);
	else
		vcodec_trace_count("VDEC_HW_LAT", 0);

	if (!mtk_vcodec_is_vcp(MTK_INST_DECODER))
		disable_irq(ctx->dev->dec_irq[hw_id]);

	mtk_vcodec_dec_clock_off(&ctx->dev->pm, hw_id);

	mtk_vcodec_set_curr_ctx(ctx->dev, NULL, hw_id);
	mtk_vdec_unlock(ctx, hw_id);
	mutex_unlock(&ctx->hw_status);

}

void vdec_check_release_lock(void *ctx_check)
{
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)ctx_check;
	int i;

	for (i = 0; i < MTK_VDEC_HW_NUM; i++) {
		if (ctx->hw_locked[i] == 1) {
			vdec_decode_unprepare(ctx, i);
			mtk_v4l2_err("[%d] daemon killed when holding lock %d",
				ctx->id, i);
		}
	}
	if (ctx->dev->dec_cnt == 1) {
		for (i = 0; i < MTK_VDEC_HW_NUM; i++)
			if (atomic_read(&ctx->dev->dec_clk_ref_cnt[i]))
				mtk_v4l2_err("[%d] hw_id %d: dec_clk_ref_cnt %d",
					ctx->id, i, atomic_read(&ctx->dev->dec_clk_ref_cnt[i]));
		if (atomic_read(&ctx->dev->dec_larb_ref_cnt))
			mtk_v4l2_err("[%d] dec_larb_ref_cnt %d",
				ctx->id, atomic_read(&ctx->dev->dec_larb_ref_cnt));
	}
}

