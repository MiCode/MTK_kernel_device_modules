// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/version.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_blend.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_gem.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_drv.h>
#include <drm/drm_vblank.h>
#include <linux/delay.h>
#include <linux/component.h>
#include <linux/iommu.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/kmemleak.h>
#include <uapi/linux/sched/types.h>
#include <drm/drm_auth.h>
#include <soc/mediatek/dramc.h>
#include <uapi/drm/mediatek_drm.h>

#include <linux/clk.h>


#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_HOST)
#include "mtk_log.h"
#include "mtk_dump.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_se.h"
#include "mtk_drm_crtc_auto.h"

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_EDPTX_AUTO_SUPPORT)
#include "../mtk_drm_edp/mtk_drm_edp_api.h"
#endif

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_DPTX_AUTO)
#include "../mtk_drm_dp/mtk_drm_dp_api.h"
#endif

struct mtk_se_dma_map {
	int fd;
	struct dma_buf *dmabuf;
	struct sg_table *sgt;
	struct dma_buf_attachment *attach;
	struct list_head list;
};

static struct mtk_se_dma_map dma_map_list;
static bool dma_map_list_initialized;
static DEFINE_MUTEX(dma_map_list_mutex);

static DEFINE_MUTEX(se_lock);

static enum mtk_ddp_comp_id panel_crtc_map[] = {
	[MTK_PANEL_DSI0] = DDP_COMPONENT_DSI0,
	[MTK_PANEL_EDP] = DDP_COMPONENT_DISP_DVO,
	[MTK_PANEL_DP0] = DDP_COMPONENT_DP_INTF0,
	[MTK_PANEL_DP1] = DDP_COMPONENT_DP_INTF1,
	[MTK_PANEL_DSI1_0] = DDP_COMPONENT_DSI1,
	[MTK_PANEL_DSI1_1] = DDP_COMPONENT_DSI1,
	[MTK_PANEL_DSI2] = DDP_COMPONENT_DSI2,
};

static int mtk_drm_se_get_crtc_id(enum MTK_PANEL_ID panel_id, struct mtk_drm_private *private)
{
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_ddp_comp *comp;

	for (int crtc_id = 0; crtc_id < MAX_CRTC; crtc_id++) {
		crtc = private->crtc[crtc_id];
		if (!crtc)
			continue;

		mtk_crtc = to_mtk_crtc(crtc);
		comp = mtk_ddp_comp_request_output(mtk_crtc);
		if (comp == NULL)
			continue;
		if (comp->id == panel_crtc_map[panel_id]) {
			DDPINFO("panel_id:%d, output comp id:%d, crtc id:%d\n", panel_id, comp->id, crtc_id);
			return crtc_id;
		}
	}

	DDPMSG("%s %d get crtc id by panel id fail, return crtc id 0", __func__, __LINE__);

	return 0;
}

static bool mtk_drm_se_crtc_need_enable(struct mtk_drm_crtc *mtk_crtc)
{
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_ddp_comp *comp;

	if (crtc->enabled)
		return false;

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (comp == NULL) {
		DDPMSG("%s[%d] comp is null\n", __func__, __LINE__);
		return false;
	}

	if (comp->id == DDP_COMPONENT_DSI0 ||
	    comp->id == DDP_COMPONENT_DISP_DVO ||
	    comp->id == DDP_COMPONENT_DP_INTF0 ||
	    comp->id == DDP_COMPONENT_DP_INTF1) {
		DDPMSG("%s[%d] crtc%d needs enable, output comp:%s\n", __func__, __LINE__,
			drm_crtc_index(&mtk_crtc->base), mtk_dump_comp_str(comp));
		return true;
	}

	return false;
}

static int mtk_drm_se_crtc_enable(struct drm_device *dev, struct drm_crtc *crtc,
				  struct drm_atomic_state *state)
{
	struct drm_crtc_state *crtc_state;
	struct drm_display_mode *mode;
	struct drm_modeset_acquire_ctx ctx;
	struct drm_connector *connector;
	struct drm_connector_list_iter conn_iter;
	struct drm_encoder *encoder;
	struct drm_connector_state *conn_state;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int ret;
	bool found = false;

	if (!mtk_drm_se_crtc_need_enable(mtk_crtc)) {
		DDPMSG("%s[%d] crtc%d needn't enable\n", __func__, __LINE__, drm_crtc_index(crtc));
		return 0;
	}

	drm_modeset_acquire_init(&ctx, 0);
	state->acquire_ctx = &ctx;

	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		drm_connector_for_each_possible_encoder(connector, encoder) {
			if (encoder->possible_crtcs & drm_crtc_mask(crtc)) {
				found = true;
				break;
			}
		}
		if (found)
			break;
	}
	drm_connector_list_iter_end(&conn_iter);

	if (!connector) {
		ret = -EINVAL;
		DDPMSG("%s connector is not ready!", __func__);
		goto out;
	}

	if (connector->funcs->detect(connector, false) != connector_status_connected) {
		ret = -EINVAL;
		DDPMSG("%s connector status is not connected(%d)!", __func__, connector->status);
		goto out;
	}

	DDPMSG("%s[%d] conn:%d, enc:%d, poss crtc:0x%x\n", __func__, __LINE__,
		connector->base.id, encoder->base.id, encoder->possible_crtcs);

	mutex_lock(&dev->mode_config.mutex);
	connector->funcs->fill_modes(connector, dev->mode_config.max_width,
						dev->mode_config.max_height);
	mutex_unlock(&dev->mode_config.mutex);

	conn_state = drm_atomic_get_connector_state(state, connector);
	if (IS_ERR(conn_state)) {
		ret = PTR_ERR(conn_state);
		DDPPR_ERR("%s get connector state fail\n", __func__);
		goto out;
	}

	ret = drm_atomic_set_crtc_for_connector(conn_state, crtc);
	if (ret) {
		DDPPR_ERR("%s set crtc for connector fail\n", __func__);
		goto out;
	}

	crtc_state = drm_atomic_get_crtc_state(state, crtc);
	if (IS_ERR(crtc_state)) {
		ret = PTR_ERR(crtc_state);
		DDPPR_ERR("%s get crtc state fail\n", __func__);
		goto out;
	}
	crtc_state->active = true;
	crtc_state->mode_changed = true;

	mode = list_first_entry(&connector->modes, typeof(*mode), head);

	drm_mode_debug_printmodeline(mode);
	drm_mode_set_crtcinfo(mode, 0);
	ret = drm_atomic_set_mode_for_crtc(crtc_state, mode);
	if (ret) {
		DDPPR_ERR("%s set mode for crtc fail\n", __func__);
		goto out;
	}

	DDPMSG("%s[%d] ret:%d\n", __func__, __LINE__, ret);
out:
	drm_modeset_drop_locks(&ctx);
	drm_modeset_acquire_fini(&ctx);

	return ret;
}

static int mtk_drm_se_enable(struct drm_device *dev, struct mtk_drm_crtc *mtk_crtc)
{
	struct drm_atomic_state *state;
	struct drm_modeset_acquire_ctx ctx;
	struct drm_crtc *crtc_cur = &mtk_crtc->base;
	struct drm_crtc *crtc;
	int ret;

	if (crtc_cur->enabled) {
		DDPINFO("%s crtc%d already enable\n", __func__, drm_crtc_index(crtc_cur));
		return 0;
	}

	DDPMSG("%s[%d] crtc%d\n", __func__, __LINE__, drm_crtc_index(crtc_cur));

	state = drm_atomic_state_alloc(dev);
	if (!state) {
		DDPMSG("%s drm_atomic_state_alloc fail!\n", __func__);
		return -ENOMEM;
	}

	drm_for_each_crtc(crtc, dev) {
		mtk_drm_se_crtc_enable(dev, crtc, state);
	}

	drm_modeset_acquire_init(&ctx, 0);
	state->acquire_ctx = &ctx;

	drm_modeset_lock(&dev->mode_config.connection_mutex, state->acquire_ctx);
	ret = drm_atomic_commit(state);
	drm_modeset_unlock(&dev->mode_config.connection_mutex);

	drm_atomic_state_put(state);
	drm_modeset_drop_locks(&ctx);
	drm_modeset_acquire_fini(&ctx);

	DDPMSG("%s[%d] crtc%d ret %d\n", __func__, __LINE__, drm_crtc_index(crtc_cur), ret);

	return ret;
}

static void mtk_drm_se_cmdq_cb(struct cmdq_cb_data data)
{
	struct mtk_cmdq_cb_data *cb_data = data.data;

	cmdq_pkt_destroy(cb_data->cmdq_handle);
	kfree(cb_data);
}

static int mtk_drm_se_plane_config(struct mtk_drm_crtc *mtk_crtc)
{
	int index = drm_crtc_index(&mtk_crtc->base);
	struct mtk_cmdq_cb_data *cb_data;
	struct cmdq_pkt *cmdq_handle;
	struct mtk_ddp_comp *comp;
	int i = 0, ret;
	struct mtk_plane_state tmp_state;
	struct mtk_plane_comp_state *t_s = &tmp_state.comp_state;

	DDPINFO("%s line:%d\n", __func__, __LINE__);

	cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
	if (!cb_data) {
		DDPMSG("%s cb data creation failed\n", __func__);
		return -1;
	}

	mtk_crtc_pkt_create(&cmdq_handle, &mtk_crtc->base, mtk_crtc->gce_obj.client[CLIENT_CFG]);
	if (!cmdq_handle) {
		DDPMSG("%s:%d NULL handle\n", __func__, __LINE__);
		kfree(cb_data);
		return -1;
	}
	//cmdq_handle = cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_CFG]);
	cb_data->cmdq_handle = cmdq_handle;
	cb_data->crtc = &mtk_crtc->base;

	mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle, DDP_FIRST_PATH, 0);

	//disable panel layers
	if (mtk_crtc->se_state == DISP_SE_START) {
		if (mtk_crtc->se_panel & (1 << MTK_PANEL_DP0)) {
			t_s->comp_id = DDP_COMPONENT_OVL2_2L;
			t_s->lye_id =  1;
			t_s->ext_lye_id = 0;
			comp = mtk_crtc_get_plane_comp(&mtk_crtc->base, &tmp_state);
			mtk_ddp_comp_layer_off(comp, t_s->lye_id, 0, cmdq_handle);
		} else if (mtk_crtc->se_panel & (1 << MTK_PANEL_DP1)) {
			//if (mtk_dpi_get_split_count() > 1) {
			//	t_s->comp_id = DDP_COMPONENT_OVL2_2L;
			//	t_s->lye_id =  0;
			//	t_s->ext_lye_id = 0;
			//	comp = mtk_crtc_get_plane_comp(&mtk_crtc->base, &tmp_state);
			//	mtk_ddp_comp_layer_off(comp, t_s->lye_id, 0, cmdq_handle);
			//} else {
			//	mtk_crtc_all_layer_off(mtk_crtc, cmdq_handle, 0);
			//}
		} //else
			//mtk_crtc_all_layer_off(mtk_crtc, cmdq_handle);
		mtk_crtc->se_state = DISP_SE_RUNNING;
	}

	for (i = 0; i < MTK_FB_SE_NUM; i++) {
		DDPINFO("%s %d mtk_crtc->se_plane[%d].state.comp_state.comp_id:%d\n", __func__, __LINE__,
			i, mtk_crtc->se_plane[i].state.comp_state.comp_id);
		if (mtk_crtc->se_plane[i].panel_id >= 0 &&
		    mtk_crtc->se_plane[i].state.comp_state.comp_id != 0) {
			comp = mtk_crtc_get_plane_comp(&mtk_crtc->base,
				&mtk_crtc->se_plane[i].state);
			DDPINFO("se crtc%d i%d comp%d,layer%d,size(%d %d %d %d)addr0x%llx\n",
				index, i, mtk_crtc->se_plane[i].state.comp_state.comp_id,
				mtk_crtc->se_plane[i].state.comp_state.lye_id,
				mtk_crtc->se_plane[i].state.pending.dst_x,
				mtk_crtc->se_plane[i].state.pending.dst_y,
				mtk_crtc->se_plane[i].state.pending.width,
				mtk_crtc->se_plane[i].state.pending.height,
				mtk_crtc->se_plane[i].state.pending.addr);
			//pts
			if (mtk_crtc->se_plane[i].state.pending.pts != 0) {
				DDPINFO("LATENCY_TEST %s t=%lld\n", __func__,
					mtk_crtc->se_plane[i].state.pending.pts);
			}
			if (mtk_crtc->se_plane[i].state.pending.enable)
				mtk_ddp_comp_layer_config(comp, 0,
					&mtk_crtc->se_plane[i].state,
					cmdq_handle);
			else {
				mtk_ddp_comp_layer_off(comp,
					mtk_crtc->se_plane[i].state.comp_state.lye_id,
					mtk_crtc->se_plane[i].state.comp_state.ext_lye_id,
					cmdq_handle);
				mtk_crtc->se_plane[i].panel_id = -1;
			}
		}
	}

	if (mtk_crtc->se_state == DISP_SE_STOP) {
		mtk_crtc->se_panel = 0;
		for (i = 0; i < mtk_crtc->static_plane.index; i++) {
			comp = mtk_crtc_get_plane_comp(&mtk_crtc->base,
				&mtk_crtc->static_plane.state[i]);
			DDPMSG("%s: back i %d, comp %d %d layer %d %d\n", __func__, i, comp->id,
				mtk_crtc->static_plane.state[i].comp_state.comp_id,
				mtk_crtc->static_plane.state[i].comp_state.lye_id,
				mtk_crtc->static_plane.state[i].comp_state.ext_lye_id);
			if (mtk_crtc->static_plane.state[i].pending.enable)
				mtk_ddp_comp_layer_config(comp, 0,
					&mtk_crtc->static_plane.state[i],
					cmdq_handle);
		}
		mtk_crtc->se_state = DISP_SE_STOPPED;
	}

	ret = cmdq_pkt_flush_threaded(cmdq_handle, mtk_drm_se_cmdq_cb, cb_data);
	if (ret < 0) {
		DDPMSG("%s[%d] cmdq_pkt_flush_threaded failed!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (mtk_crtc->se_state == DISP_SE_STOPPED) {
		for (i = 0; i < MTK_FB_SE_NUM; i++) {
			memset((void *)&mtk_crtc->se_plane[i].state, 0,
				sizeof(struct mtk_plane_state));
			mtk_crtc->se_plane[i].panel_id = -1;
		}
		msleep(33);
	}

	return 0;
}

void mtk_drm_se_init(void)
{
	memset((void *)&dma_map_list, 0, sizeof(dma_map_list));
	INIT_LIST_HEAD(&dma_map_list.list);
}

void mtk_drm_se_crtc_init(struct mtk_drm_crtc *mtk_crtc)
{
	int i;

	mutex_init(&mtk_crtc->sol_lock);

	/*surfaceengine*/
	mtk_crtc->se_panel = 0;
	mtk_crtc->sideband_layer = -1;
	mtk_crtc->se_state = DISP_SE_IDLE;
	for (i = 0; i < MTK_FB_SE_NUM; i++)
		mtk_crtc->se_plane[i].panel_id = -1;
	/*end surfaceengine*/
}

int mtk_drm_set_ovl_layer(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_plane_state *state;
	struct mtk_drm_layer_info *layer_info = (struct mtk_drm_layer_info *)data;
	struct mtk_drm_private *private = dev->dev_private;
	int i = 0, enable_cnt = 0;
	struct mtk_panel_params *params;
	int index = 0, crtc_id = 0;
	struct mtk_crtc_se_plane *se_plane;
	u64 sys_time;
	struct mtk_ddp_comp *comp = NULL;
#ifdef CONFIG_MTK_ULTRARVC_SUPPORT
	static bool ultrarvc_start = true;
#endif

	unsigned int layer_id = 0;
	unsigned int read_value = 0;

	sys_time = ktime_to_ns(ktime_get_boottime());
	if (layer_info->pts != 0)
		DDPINFO("LATENCY_TEST %s %lld sys_time %lld\n",
			__func__, layer_info->pts, sys_time);

	crtc_id = mtk_drm_se_get_crtc_id(layer_info->panel_id, private);
	if (crtc_id >= MAX_CRTC) {
		DDPMSG("the crtc_id %d >= max %d\n", crtc_id, MAX_CRTC);
		return -1;
	}

	DDPINFO("%s crtc%d L:%d\n", __func__, crtc_id, __LINE__);

	if (layer_info->layer_id >= MTK_FB_SE_NUM) {
		DDPMSG("%s: panel:%d invalid layer id:%d\n", __func__,
			layer_info->panel_id, layer_info->layer_id);
		return -EINVAL;
	}

	if ((layer_info->panel_id == MTK_PANEL_DP0 ||
	    layer_info->panel_id == MTK_PANEL_DP1) &&
	    layer_info->layer_id >= 1) {
		DDPMSG("%s: panel:%d invalid layer id:%d\n", __func__,
			layer_info->panel_id, layer_info->layer_id);
		return -EINVAL;
	}

	mtk_crtc = to_mtk_crtc(private->crtc[crtc_id]);
	crtc = &mtk_crtc->base;

	index = drm_crtc_index(crtc);

	DDP_MUTEX_LOCK_NESTED(&mtk_crtc->sol_lock, index, __func__, __LINE__);

	if (!(crtc->enabled && mtk_crtc->enabled)) {
		if (mtk_crtc->se_panel)
			mtk_crtc->se_panel = 0;
		DDP_MUTEX_UNLOCK_NESTED(&mtk_crtc->sol_lock, index, __func__, __LINE__);
		DDPMSG("crtc%d still disable\n", index);
		return -EINVAL;
	}

	if (mtk_crtc->se_state != DISP_SE_START &&
	    mtk_crtc->se_state != DISP_SE_RUNNING &&
	    !layer_info->layer_en) {
		DDP_MUTEX_UNLOCK_NESTED(&mtk_crtc->sol_lock, index, __func__, __LINE__);
		DDPMSG("se still stop, not config\n");
		return 0;
	}

	layer_id = layer_info->layer_id;

	params = mtk_drm_get_lcm_ext_params(&mtk_crtc->base);

	if (layer_info->layer_en)
		mtk_crtc->se_panel |= 1 << layer_info->panel_id;

	se_plane = &mtk_crtc->se_plane[layer_id];
	se_plane->panel_id = layer_info->panel_id;
	state = &se_plane->state;
	state->pending.enable = layer_info->layer_en;
	state->pending.src_x = layer_info->src_x;
	state->pending.src_y = layer_info->src_y;
	state->pending.dst_x = layer_info->tgt_x;
	state->pending.dst_y = layer_info->tgt_y;
	state->pending.width = layer_info->tgt_w;
	state->pending.height = layer_info->tgt_h;
	state->pending.offset = (state->pending.dst_y << 16 | state->pending.dst_x);
	state->pending.format = layer_info->src_format;
	state->pending.pitch = layer_info->src_pitch;
	state->pending.modifier  = 1;
	state->pending.is_sec = 0;
	state->pending.addr = (dma_addr_t)layer_info->src_mvaddr;

	state->pending.prop_val[PLANE_PROP_COMPRESS] = layer_info->compress;
	state->base.alpha = 0xff << 8;
	state->base.pixel_blend_mode = DRM_MODE_BLEND_PREMULTI;

	DDPINFO("%s line:%d panel_id:%d en:%d, (%d %d %d %d w%d h%d) fmt:%d picth:%d addr:0x%llx, compress:%llu\n",
		__func__, __LINE__, se_plane->panel_id, state->pending.enable, state->pending.src_x,
		state->pending.src_y, state->pending.dst_x, state->pending.dst_y, state->pending.width,
		state->pending.height, state->pending.format, state->pending.pitch, state->pending.addr,
		state->pending.prop_val[PLANE_PROP_COMPRESS]);

	state->pending.dirty = true;
	state->comp_state.lye_id = mtk_crtc->layer_nr - 1;

	state->comp_state.ext_lye_id = LYE_NORMAL;
	state->pending.pts = layer_info->pts;

	comp = mtk_crtc_get_comp_with_plane_state(mtk_crtc, state);
	if (!comp) {
		DDPMSG("%s invalid comp\n", __func__);
		return -EINVAL;
	}

	state->comp_state.comp_id = comp->id;
	state->comp_state.lye_id = 0;

#ifdef CONFIG_MTK_ULTRARVC_SUPPORT
		/*{ultrarvc +*/
		if ((layer_info->panel_id == MTK_PANEL_DSI0_0) &&
			(layer_info->user_type == MTK_USER_AVM)) {
			comp = mtk_crtc_get_plane_comp_by_id(&mtk_crtc->base, DDP_COMPONENT_OVL0);
			if (is_ultrarvc_enable(comp)) {
				state->comp_state.comp_id = DDP_COMPONENT_OVL0;
#if (CONFIG_MTK_MULTI_DSI_PATH == 2)
				state->comp_state.lye_id = 1;
#else
				state->comp_state.lye_id = 3;
#endif
			}
		}
		/*ultrarvc -}*/
#endif

	if (mtk_crtc->sideband_layer >= 0)
		state->comp_state.lye_id = mtk_crtc->sideband_layer;

	for (i = 0; i < MTK_FB_SE_NUM; i++) {
		if (mtk_crtc->se_plane[i].state.pending.enable) {
			enable_cnt++;
			break;
		}
	}

#ifdef CONFIG_MTK_ULTRARVC_SUPPORT
		if (ultrarvc_start && (layer_info->user_type == MTK_USER_AVM) &&
			(mtk_crtc->se_state == DISP_SE_RUNNING)) {
			if (mtk_cam_fe_send_ipc_msg(MTK_CAM_FE_IPC_OPEN, CMD_RESPOND_NEEDED,
				NULL, 0) == SUCCESS) {
				ultrarvc_start = false;
				mtk_cam_fe_send_ipc_msg(MTK_CAM_FE_IPC_REVERSE,
					CMD_RESPOND_NEEDED, &ultrarvc_start, sizeof(bool));
				DDPMSG("disable ultrarvc\n");
			} else
				DDPMSG("open cam be failed\n");
		}
#endif

	DDPINFO("crtc%d panel%d comp_id%d layer_id%d\n", index,
		layer_info->panel_id, state->comp_state.comp_id, state->comp_state.lye_id);

	if ((mtk_crtc->se_state != DISP_SE_START) &&
		(mtk_crtc->se_state != DISP_SE_RUNNING) &&
		(enable_cnt))  {
		DDPMSG("crtc%d se first start\n", index);
		mtk_crtc->se_state = DISP_SE_START;

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_HOST)
		__mtk_disp_set_module_hrt(comp->hrt_qos_req, comp->id, 10000, true);
#else
		__mtk_disp_set_module_hrt(comp->hrt_qos_req, 10000, true);
#endif

		if (mtk_crtc->se_panel & (1 << MTK_PANEL_DSI1_0)) {
			writel(0x1, private->ovlsys0_rsz_regs + MT6991_OVL_MDP_RSZ0_DUMM30);
			read_value = readl(private->ovlsys0_rsz_regs + MT6991_OVL_MDP_RSZ0_DUMM30);
			DDPMSG("%s %d set dsi1 flag:%d, comp->id:%s\n", __func__, __LINE__,
				read_value, mtk_dump_comp_str(comp));
		}
	}

	if (!enable_cnt) {
		DDPMSG("crtc%d se stop\n", index);
		mtk_crtc->se_state = DISP_SE_STOP;
		if (mtk_crtc->se_panel & (1 << MTK_PANEL_DSI1_0)) {
			writel(0x0, private->ovlsys0_rsz_regs + MT6991_OVL_MDP_RSZ0_DUMM30);
			read_value = readl(private->ovlsys0_rsz_regs + MT6991_OVL_MDP_RSZ0_DUMM30);
			DDPMSG("%s %d dsi1 flag::%d\n", __func__, __LINE__, read_value);
		}
	}

	mtk_drm_se_plane_config(mtk_crtc);

	DDP_MUTEX_UNLOCK_NESTED(&mtk_crtc->sol_lock, index, __func__, __LINE__);

	return 0;
}

int mtk_drm_map_dma_buf(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	struct mtk_drm_dma_buf *dma_map = (struct mtk_drm_dma_buf *)data;
	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_se_dma_map *map_list;
	dma_addr_t mva;

	dma_map->mva = 0;

	if (dma_map->fd <= 0)
		return -EINVAL;

	if (!dma_map_list_initialized) {
		INIT_LIST_HEAD(&dma_map_list.list);
		dma_map_list_initialized = true;
	}

	map_list = kmalloc(sizeof(struct mtk_se_dma_map), GFP_KERNEL);
	if (!map_list) {
		DDPMSG("%s alloc fail\n", __func__);
		return -ENOMEM;
	}

	map_list->fd = dma_map->fd;

	map_list->dmabuf = dma_buf_get(dma_map->fd);
	if (IS_ERR(map_list->dmabuf)) {
		DDPMSG("%s:%d error! hnd:0x%p, fd:%d\n",
				__func__, __LINE__, map_list->dmabuf, dma_map->fd);
		goto release;
	}
	map_list->attach = dma_buf_attach(map_list->dmabuf, private->dma_dev);
	if (IS_ERR(map_list->attach)) {
		DDPMSG("%s:%d error! attach:0x%p, fd:%d\n",
				__func__, __LINE__, map_list->attach, dma_map->fd);
		goto fail_get;
	}

	map_list->sgt = dma_buf_map_attachment(map_list->attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(map_list->sgt))
		goto fail_detach;

	mva = sg_dma_address(map_list->sgt->sgl);

	dma_map->mva = mva;

	DDPMSG("%s dma fd is %d mva 0x%llx\n", __func__, dma_map->fd, dma_map->mva);

	INIT_LIST_HEAD(&map_list->list);

	mutex_lock(&dma_map_list_mutex);
	list_add_tail(&map_list->list, &dma_map_list.list);
	mutex_unlock(&dma_map_list_mutex);

	return 0;

fail_detach:
	dma_buf_detach(map_list->dmabuf, map_list->attach);
fail_get:
	dma_buf_put(map_list->dmabuf);
release:
	kfree(map_list);
	map_list = NULL;
	return -EINVAL;
}

int mtk_drm_unmap_dma_buf(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	struct mtk_se_dma_map *map_list = NULL;
	struct mtk_se_dma_map *n = NULL;
	unsigned int value = *(unsigned int *)data;
	unsigned int fd = value & 0x3FF;
	unsigned int unmap_all_once = (value >> 10) & 0x1;

	DDPMSG("%s dma fd\n", __func__);

	mutex_lock(&dma_map_list_mutex);
	list_for_each_entry_safe(map_list, n, &dma_map_list.list, list) {
		if (unmap_all_once == 1 || fd == map_list->fd) {
			DDPMSG("%s dma fd is %d, unmap_all_once:%d\n", __func__, map_list->fd, unmap_all_once);
			list_del_init(&map_list->list);
			dma_buf_unmap_attachment(map_list->attach, map_list->sgt,
				DMA_BIDIRECTIONAL);
			dma_buf_detach(map_list->dmabuf, map_list->attach);
			dma_buf_put(map_list->dmabuf);

			kfree(map_list);
			map_list = NULL;
			if (unmap_all_once != 1)
				break;
		}
	}
	mutex_unlock(&dma_map_list_mutex);

	return 0;
}

void mtk_drm_se_check_plane(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_plane_comp_state *comp_state;
	int i = 0;
	int lye_id;

	mtk_crtc->sideband_layer = -1;
	mtk_crtc->static_plane.index = 0;

	for (i = 0; i < mtk_crtc->layer_nr; i++) {
		struct drm_plane *plane = &mtk_crtc->planes[i].base;
		struct mtk_plane_state *plane_state;

		plane_state = to_mtk_plane_state(plane->state);
		comp_state = &(plane_state->comp_state);
		lye_id = plane_state->comp_state.lye_id;

		if (plane_state->prop_val[PLANE_PROP_MODE] == MTK_PLANE_SIDEBAND) {
			if ((plane_state->comp_state.comp_id == DDP_COMPONENT_OVL1_2L) ||
				(plane_state->comp_state.comp_id == DDP_COMPONENT_OVL3_2L))
				mtk_crtc->sideband_layer = lye_id + 2;
			else
				mtk_crtc->sideband_layer = lye_id;

			DDPINFO("%s, sideband com %d, ly %d\n", __func__,
				plane_state->comp_state.comp_id, mtk_crtc->sideband_layer);
		}
	}
}

void mtk_drm_se_plane_update(struct drm_crtc *crtc, struct drm_plane *plane,
			     struct mtk_plane_state *plane_state)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp = mtk_crtc_get_plane_comp(crtc, plane_state);
	unsigned int index = mtk_crtc->static_plane.index++;
	int layer_id = 0, comp_id = 0;
	int crtc_index = drm_crtc_index(crtc);

	if (!comp) {
		DDPMSG("[E] %s invalid comp\n", __func__);
		return;
	}

	DDPDBG("%s %d %s\n", __func__, __LINE__, mtk_dump_comp_str_id(comp->id));

	memcpy((void *)&mtk_crtc->static_plane.state[index],
		(void *)plane_state, sizeof(struct mtk_plane_state));

	if (mtk_crtc->sideband_layer >= 0) {
		if (plane_state->pending.prop_val[PLANE_PROP_MODE] == MTK_PLANE_SIDEBAND) {
			layer_id = plane_state->comp_state.lye_id;
			comp_id = plane_state->comp_state.comp_id;
			memcpy((void *)plane_state, (void *)&mtk_crtc->se_plane[0].state,
				sizeof(struct mtk_plane_state));
			plane_state->comp_state.lye_id = layer_id;
			plane_state->comp_state.comp_id = comp_id;
			DDPINFO("crtc%d memcpy layer %d, comp_id %d\n",
				crtc_index, layer_id, comp_id);
		}
		return;
	}

	if (!mtk_crtc->se_panel)
		return;

	layer_id = plane_state->comp_state.lye_id;
	if (comp && (comp->id == mtk_crtc->se_plane[layer_id].state.comp_state.comp_id) &&
		mtk_crtc->se_plane[layer_id].state.pending.enable) {
		memcpy((void *)plane_state, (void *)&mtk_crtc->se_plane[layer_id].state,
			sizeof(struct mtk_plane_state));
		plane_state->comp_state.lye_id = layer_id;
		plane_state->base = mtk_crtc->static_plane.state[index].base;
		DDPDBG("%s %d %s\n", __func__, __LINE__, mtk_dump_comp_str_id(comp->id));
	} //else {
		//plane_state->pending.enable = false;
	//}

	DDPINFO("%s+ comp_id:%d, en:%d, lye_id:%d\n",
		__func__, plane_state->comp_state.comp_id,
		plane_state->pending.enable, plane_state->comp_state.lye_id);
}

int mtk_drm_se_get_info_ioctl(struct drm_device *dev, void *data)
{
	int ret = 0;
	struct drm_mtk_session_info *info = data;
	int s_dev = MTK_SESSION_DEV(info->session_id);

	struct mtk_drm_private *priv = dev->dev_private;
	int crtc_id = mtk_drm_se_get_crtc_id(s_dev, priv);
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(priv->crtc[crtc_id]);
	unsigned int msleep_times = 0;
	bool is_rdy = false;
	struct mtk_ddp_comp *output_comp;

	DDPMSG("%s %d, s_dev:%d crtc:%d\n", __func__, __LINE__, s_dev, crtc_id);

	if (!mtk_crtc) {
		DDPMSG("[E] %s invalid CRTC%d\n", __func__, crtc_id);
		return -EINVAL;
	}

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (unlikely(!output_comp)) {
		DDPMSG("[E] %s:CRTC%d invalid output comp\n", __func__, crtc_id);
		return -EINVAL;
	}

	mtk_ddp_comp_io_cmd(output_comp, NULL, GET_ENABLE_READY_STATUS, &is_rdy);

#ifdef IF_ZERO
	{
		DDPMSG("%s do se enable\n", __func__);
		mutex_lock(&se_lock);
		ret = mtk_drm_se_enable(dev, mtk_crtc);
		mutex_unlock(&se_lock);
		if (ret < 0) {
			DDPMSG("%s se enable fail, no panel info to get\n", __func__);
			return ret;
		}
	}
#endif

	while (!mtk_crtc->enabled ||
		(((s_dev == MTK_PANEL_DP0) ||
		  (s_dev == MTK_PANEL_EDP) ||
		  (s_dev == MTK_PANEL_DSI0)) && !is_rdy)) {
		msleep(20);
		msleep_times++;
		if (msleep_times >= 500) {
			DDPMSG("%s:wait CRTC%d or %s ready timeout!\n", __func__,
				crtc_id, mtk_dump_comp_str(output_comp));
			return -EINVAL;
		}
		if (is_rdy == false)
			mtk_ddp_comp_io_cmd(output_comp, NULL, GET_ENABLE_READY_STATUS, &is_rdy);
	}

	if (s_dev == MTK_PANEL_EDP) {
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_EDPTX_AUTO_SUPPORT)
		ret = mtk_drm_dvo_get_info(dev, info);
#endif
	} else if ((s_dev == MTK_PANEL_DP0) || (s_dev == MTK_PANEL_DP1)) {
		int alias_id = 0;

		alias_id = mtk_ddp_comp_get_alias(output_comp->id);
		if (alias_id < 0) {
			DDPPR_ERR("%s get alias fail\n", __func__);
			return -EINVAL;
		}

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_HOST)
		ret = mtk_drm_dp_get_info(dev, info, alias_id);
#endif
		DDPINFO("%s crtc_id:%d output_comp:%d alias_id:%d\n",
			__func__, crtc_id, output_comp->id, alias_id);
	} else {
		ret = mtk_drm_get_panel_info(dev, info, crtc_id);
	}

	return ret;
}
#endif
