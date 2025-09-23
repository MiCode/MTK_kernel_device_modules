// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <drm/drm_crtc.h>
#include <drm/drm_vblank.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_plane.h>
#include <linux/videodev2.h>

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO)
#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_log.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_plane.h"
#include "mtk_drm_gem.h"

#include "mtk_drm_ddp_comp_auto.h"
#include "mtk_drm_crtc_auto.h"
#include "mtk_virtio_disp.h"

#include "mtk_drm_se.h"

/*=======================================HOST===================================================*/
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_HOST)
static struct drm_device *g_drm_dev;

static bool mtk_drm_crtc_check_android_layer_on(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	unsigned int i, j;
	struct mtk_ddp_comp *comp;

	/* check android exdma layer on or not */
	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) {
		if (mtk_ddp_comp_is_rdma(comp) &&
		    mtk_ddp_comp_is_virt(comp) &&
		    mtk_ddp_comp_is_layer_on(comp)) {
			DDPMSG("%s CRTC%d comp %s is layer on\n",
				__func__, drm_crtc_index(crtc), mtk_dump_comp_str(comp));
			return true;
		}
	}

	return false;
}

static void mtk_drm_crtc_layer_off_logo_plane(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	struct drm_crtc_state *crtc_state = NULL;
	struct mtk_crtc_state *mt_crtc_state = NULL;
	struct mtk_ddp_comp *comp = mtk_crtc->first_exdma;

	struct cmdq_pkt *cmdq_handle = NULL;

	if (mtk_crtc->logo_state != LOGO_INIT)
		return;

	/* keep logo layer on until android exdma layer not on,  */
	if (!mtk_drm_crtc_check_android_layer_on(crtc))
		return;

	crtc_state = crtc->state;
	if (!crtc_state) {
		DDPMSG("[E]%s invalid crtc_state!\n", __func__);
		return;
	}

	mt_crtc_state = to_mtk_crtc_state(crtc_state);
	if (!crtc_state) {
		DDPMSG("[E]%s invalid state!\n", __func__);
		return;
	}

	if (!mtk_crtc->is_primary_layer_swap) {
		DDPMSG("%s CRTC%d no need swap layer\n", __func__, drm_crtc_index(crtc));
		return;
	}

	mtk_crtc_pkt_create(&cmdq_handle, crtc,
				mtk_crtc->gce_obj.client[CLIENT_CFG]);

	DDPMSG("%s CRTC%d comp %s cmdq_handle 0x%p\n",
	       __func__, drm_crtc_index(crtc), mtk_dump_comp_str(comp), cmdq_handle);

	mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle, DDP_FIRST_PATH, 0);

	mtk_ddp_comp_layer_off(comp, 0, 0, cmdq_handle);

	cmdq_pkt_flush(cmdq_handle);
	cmdq_pkt_destroy(cmdq_handle);

	mtk_crtc->logo_state = LOGO_LAYER_OFF;
}

static void mtk_drm_crtc_free_logo_buffer(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	if (mtk_crtc->logo_state != LOGO_LAYER_OFF)
		return;

	DDPMSG("%s CRTC%d free frame buffer\n", __func__, drm_crtc_index(crtc));
	mtk_drm_fb_gem_release(crtc->dev);
	free_fb_buf();

	mtk_crtc->logo_state = LOGO_FB_FREE;
}

static int mtk_drm_crtc_logo_layer_thread(void *data)
{
	int ret = 0;
	struct drm_crtc *crtc = (struct drm_crtc *)data;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	while (1) {
		ret = wait_event_interruptible(
			mtk_crtc->logo_layer_wq,
			atomic_read(&mtk_crtc->logo_layer_task_active));

		/* release logo buffer in 2nd vsync */
		mtk_drm_crtc_free_logo_buffer(crtc);

		/* layer off logo layer in 1st vsync */
		mtk_drm_crtc_layer_off_logo_plane(crtc);

		atomic_set(&mtk_crtc->logo_layer_task_active, 0);

		if (kthread_should_stop()) {
			DDPMSG("[E] %s stopped\n", __func__);
			break;
		}
	}

	return 0;
}

static void mtk_drm_crtc_create_logo_layer_thread(struct mtk_drm_crtc *mtk_crtc, int pipe)
{
	char name[50];
	int len = 0;

	len = snprintf(name, sizeof(name), "crtc%d_logo", pipe);
	if (len < 0)
		DDPMSG("[E]%s:%d snprintf failed\n", __func__, __LINE__);
	mtk_crtc->logo_layer_task = kthread_create(
		mtk_drm_crtc_logo_layer_thread, &mtk_crtc->base, name);
	init_waitqueue_head(&mtk_crtc->logo_layer_wq);
	wake_up_process(mtk_crtc->logo_layer_task);

	DDPMSG("%s CRTC%d create thread %s\n", __func__, pipe, name);
}

void mtk_drm_crtc_init_logo_layer_on(struct mtk_drm_crtc *mtk_crtc, int pipe)
{
	struct mtk_ddp_comp *output_comp;

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp && mtk_ddp_comp_is_enable_from_lk(output_comp->id) && (pipe == 0)) {
		mtk_crtc->logo_state = LOGO_INIT;
		mtk_drm_crtc_create_logo_layer_thread(mtk_crtc, pipe);
	} else {
		mtk_crtc->logo_state = LOGO_STATE_INVALID;
	}

	DDPMSG("%s CRTC%d logo_state %d\n", __func__, pipe, mtk_crtc->logo_state);
}

void mtk_drm_crtc_wakeup_logo_layer_thread(struct mtk_drm_crtc *mtk_crtc)
{
	if (mtk_crtc->logo_state == LOGO_STATE_INVALID ||
	    mtk_crtc->logo_state == LOGO_FB_FREE)
		return;

	if (mtk_crtc->logo_state == LOGO_INIT ||
	    mtk_crtc->logo_state == LOGO_LAYER_OFF) {
		atomic_set(&mtk_crtc->logo_layer_task_active, 1);
		wake_up_interruptible(&mtk_crtc->logo_layer_wq);
	}
}

static void set_value_to_regs_field(u16 in_value, void __iomem *base_addr, int field)
{
	u32 val = 0, value = 0, mask = 0;

	SET_VAL_MASK(value, mask,
		in_value, field);

	val = readl(base_addr);
	val = (val & ~mask) | (value & mask);
	writel(val, base_addr);
}

static void mtk_drm_connector_notify_guest(struct mtk_drm_crtc *mtk_crtc,
					   unsigned int connector_enable)
{
	void __iomem *regs_base = NULL;
	struct mtk_ddp_comp *comp;

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (comp == NULL) {
		DDPMSG("[E] %s get comp fail!", __func__);
		return;
	}

	regs_base = ioremap(DUMMY_REG_BASE, 0x1000);
	if (regs_base == NULL) {
		DDPMSG("[E] %s regs base ioremap fail!", __func__);
		return;
	}

	if (comp->id == DDP_COMPONENT_DP_INTF0)
		set_value_to_regs_field(connector_enable, regs_base +
				MT6991_OVL_MDP_RSZ0_DUMM20, DP_INTF0_CONNECTOR_READY);

	iounmap(regs_base);
}

void mtk_get_output_timing(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp;
	struct drm_display_mode timing = {0};
	unsigned int connector_enable = 0;
	int ret = 0;

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (comp == NULL)
		return;

	drm_mode_copy(&timing, &crtc->state->adjusted_mode);

	// backup panel timing for android using.
	mtk_drm_backup_default_timing(mtk_crtc, &timing);

	mtk_ddp_comp_io_cmd(comp, NULL, CONNECTOR_IS_ENABLE, &connector_enable);
	mtk_drm_connector_notify_guest(mtk_crtc, connector_enable);
}

static void store_timing_to_dummy(struct drm_display_mode *timing,
				  void __iomem *regs_base, enum mtk_ddp_comp_id id)
{
	if ((timing != NULL) && (regs_base != NULL)) {
		if (id == DDP_COMPONENT_DSI0) {
			set_value_to_regs_field(timing->hdisplay, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY0, DSI_HDISPLAY_VALUE);
			set_value_to_regs_field(timing->vdisplay, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY0, DSI_VDISPLAY_VALUE);
			set_value_to_regs_field(timing->hsync_start, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY1, DSI_HSYNC_START_VALUE);
			set_value_to_regs_field(timing->hsync_end, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY1, DSI_HSYNC_END_VALUE);
			set_value_to_regs_field(timing->vsync_start, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY2, DSI_VSYNC_START_VALUE);
			set_value_to_regs_field(timing->vsync_end, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY2, DSI_VSYNC_END_VALUE);
			set_value_to_regs_field(timing->htotal, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY3, DSI_HSYNC_TOTAL_VALUE);
			set_value_to_regs_field(timing->vtotal, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY3, DSI_VSYNC_TOTAL_VALUE);
			writel(timing->clock, regs_base + MT6991_OVL_MDP_RSZ0_DUMMY4);
			set_value_to_regs_field(timing->height_mm, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY5, DSI_HEIGHT_MM_VALUE);
			set_value_to_regs_field(timing->width_mm, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY5, DSI_WIDTH_MM_VALUE);
		} else if (id == DDP_COMPONENT_DP_INTF0) {
			set_value_to_regs_field(timing->hdisplay, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY6, DP_HDISPLAY_VALUE);
			set_value_to_regs_field(timing->vdisplay, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY6, DP_VDISPLAY_VALUE);
			set_value_to_regs_field(timing->hsync_start, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY7, DP_HSYNC_START_VALUE);
			set_value_to_regs_field(timing->hsync_end, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY7, DP_HSYNC_END_VALUE);
			set_value_to_regs_field(timing->vsync_start, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY8, DP_VSYNC_START_VALUE);
			set_value_to_regs_field(timing->vsync_end, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY8, DP_VSYNC_END_VALUE);
			set_value_to_regs_field(timing->htotal, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY9, DP_HSYNC_TOTAL_VALUE);
			set_value_to_regs_field(timing->vtotal, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY9, DP_VSYNC_TOTAL_VALUE);
			writel(timing->clock, regs_base + MT6991_OVL_MDP_RSZ0_DUMMY10);
			set_value_to_regs_field(timing->height_mm, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY11, DP_HEIGHT_MM_VALUE);
			set_value_to_regs_field(timing->width_mm, regs_base +
					MT6991_OVL_MDP_RSZ0_DUMMY11, DP_WIDTH_MM_VALUE);
		}

		DDPMSG("%s,%d,[%d]DUMMY 0=0x%x, 1=0x%x 2=0x%x, 3=0x%x 4=0x%x, 5=0x%x\n",
			__func__, __LINE__, id,
			readl(regs_base + MT6991_OVL_MDP_RSZ0_DUMMY6),
			readl(regs_base + MT6991_OVL_MDP_RSZ0_DUMMY7),
			readl(regs_base + MT6991_OVL_MDP_RSZ0_DUMMY8),
			readl(regs_base + MT6991_OVL_MDP_RSZ0_DUMMY9),
			readl(regs_base + MT6991_OVL_MDP_RSZ0_DUMMY10),
			readl(regs_base + MT6991_OVL_MDP_RSZ0_DUMMY11));

	} else {
		DDPMSG("[E] %s timing or regs_base is error!", __func__);
	}
}

void mtk_drm_backup_default_timing(struct mtk_drm_crtc *mtk_crtc, struct drm_display_mode *timing)
{
	void __iomem *regs_base = NULL;
	struct mtk_ddp_comp *comp;

	DDPMSG("%s+, %d\n", __func__, __LINE__);

	if (timing == NULL) {
		DDPMSG("[E] %s invalid timing!", __func__);
		return;
	}

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (comp == NULL) {
		DDPMSG("[E] %s get comp fail!", __func__);
		return;
	}

	regs_base = ioremap(DUMMY_REG_BASE, 0x1000);
	if (regs_base == NULL) {
		DDPMSG("[E] %s regs base ioremap fail!", __func__);
		return;
	}

	store_timing_to_dummy(timing, regs_base, comp->id);

	iounmap(regs_base);
}

void mtk_drm_crtc_dev_init(struct drm_device *dev)
{
	if (IS_ERR_OR_NULL(dev))
		DDPMSG("%s, invalid dev\n", __func__);

	g_drm_dev = dev;
}

struct drm_device *mtk_drm_get_dev(void)
{
	return g_drm_dev;
}

struct mtk_drm_crtc *mtk_drm_get_crtc_by_output(unsigned int comp_id)
{
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_ddp_comp *comp;
	struct mtk_drm_private *priv;
	struct cmdq_pkt *cmdq_handle = NULL;
	int i;

	if (IS_ERR_OR_NULL(g_drm_dev)) {
		DDPMSG("%s invalid drm dev\n", __func__);
		return NULL;
	}

	priv = g_drm_dev->dev_private;

	for (i = 0; i < MAX_CRTC; i++) {
		if (!priv->crtc[i])
			continue;

		mtk_crtc = to_mtk_crtc(priv->crtc[i]);
		if (!mtk_crtc)
			continue;
		comp = mtk_ddp_comp_request_output(mtk_crtc);

		if (comp && comp->id == comp_id)
			return mtk_crtc;
	}

	return NULL;
}
#endif

/*======================================COMMON===================================================*/
u32 mtk_crtc_get_plane_local_index(struct mtk_drm_crtc *mtk_crtc,
				   struct mtk_plane_state *plane_state)
{
	struct drm_plane *plane = plane_state->base.plane;
	struct drm_plane *base_plane = &mtk_crtc->planes[0].base;
	unsigned int local_index = 0;

	if (!plane) {
		local_index = plane_state->comp_state.lye_id;
	} else {
		local_index = plane->index - base_plane->index;

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_HOST)
		if (mtk_crtc->is_primary_layer_swap)
			local_index = mtk_crtc->layer_nr - 1 - local_index;
#endif
		DDPINFO("%s plane index %d base %d\n", __func__, plane->index, base_plane->index);
	}

	return local_index;
}

struct mtk_ddp_comp *mtk_crtc_get_comp_with_index(struct mtk_drm_crtc *mtk_crtc,
						  u32 local_index)
{
	unsigned int exdma_idx = 0;

	unsigned int i, j;
	struct mtk_ddp_comp *comp;

	if (local_index >= mtk_crtc->layer_nr) {
		DDPMSG("[E]%s CRTC%d invalid layer index %d\n",
			__func__, drm_crtc_index(&mtk_crtc->base), local_index);
		return mtk_crtc->first_exdma;
	}

	DDPINFO("%s crtc %d local_index %d\n", __func__, drm_crtc_index(&mtk_crtc->base), local_index);

	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) {
		if (mtk_ddp_comp_is_rdma(comp) && !mtk_ddp_comp_is_virt(comp)) {
			if (exdma_idx == local_index) {
				DDPINFO("%s get comp %s\n",
					__func__, mtk_dump_comp_str(comp));
				return comp;
			}

			exdma_idx++;
		}
	}

	DDPMSG("[E]%s crtc %d local_index %d cannot find exmda\n",
		__func__, drm_crtc_index(&mtk_crtc->base), local_index);

	return NULL;
}

struct mtk_ddp_comp *mtk_crtc_get_comp_with_plane_state(struct mtk_drm_crtc *mtk_crtc,
						  struct mtk_plane_state *plane_state)
{
	unsigned int local_index = 0;

	struct mtk_ddp_comp *comp;

	local_index = mtk_crtc_get_plane_local_index(mtk_crtc, plane_state);

	DDPINFO("%s crtc %d local_index %d\n", __func__, drm_crtc_index(&mtk_crtc->base), local_index);

	comp = mtk_crtc_get_comp_with_index(mtk_crtc, local_index);

	return comp;
}

void mtk_drm_crtc_init_layer_nr(struct mtk_drm_crtc *mtk_crtc, int pipe)
{
	unsigned int i, j;
	struct mtk_ddp_comp *comp, *next_comp;

	mtk_crtc->layer_nr = 0;

	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) {
		if (mtk_ddp_comp_is_rdma(comp) && !mtk_ddp_comp_is_virt(comp)) {
			next_comp = mtk_crtc_get_comp(&mtk_crtc->base, mtk_crtc->ddp_mode, i, j + 1);

			if (next_comp &&
			    (mtk_ddp_comp_get_type(next_comp->id) == MTK_OVL_BLENDER)) {
				DDPINFO("%s layer %d comp %s\n",
					__func__, mtk_crtc->layer_nr, mtk_dump_comp_str(comp));
				mtk_crtc->layer_nr++;
			}
		}
	}

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_HOST)
	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (comp && (comp->id == DDP_COMPONENT_DSI2 ||
		     mtk_ddp_comp_get_type(comp->id) == MTK_DISP_WDMA))
		mtk_crtc->layer_nr = 1;
#endif

	DDPMSG("%s crtc%d layer_nr %d\n", __func__, pipe, mtk_crtc->layer_nr);
}

int mtk_drm_crtc_init_plane(struct drm_device *drm_dev, struct mtk_drm_crtc *mtk_crtc, int pipe)
{
	unsigned int zpos;
	enum drm_plane_type type;
	int ret;

	DDPMSG("%s CRTC%d layer_nr %d\n",
	       __func__, pipe, mtk_crtc->layer_nr);

	for (zpos = 0; zpos < mtk_crtc->layer_nr; zpos++) {
		if (zpos == 0)
			type = DRM_PLANE_TYPE_PRIMARY;
		else
			type = DRM_PLANE_TYPE_OVERLAY;

		ret = mtk_plane_init(drm_dev, &mtk_crtc->planes[zpos], zpos,
				     BIT(pipe), type);
		if (ret) {
			DDPMSG("[E] %s CRTC%d plane %d init fail!\n", __func__, pipe, zpos);
			return ret;
		}
	}

	return 0;
}

void mtk_drm_crtc_auto_init(struct mtk_drm_crtc *mtk_crtc,
			    const struct mtk_crtc_path_data *path_data,
			    int pipe)
{
	unsigned int possible_crtcs = 0;
	struct mtk_ddp_comp *output_comp;

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_HOST)
	mtk_drm_se_crtc_init(mtk_crtc);
#endif

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_HOST)
	mtk_crtc->offset_x = 0;
	mtk_crtc->offset_y = 0;
	mtk_crtc->virtual_path = false;
	mtk_crtc->is_guest_exclusive_device = path_data->is_guest_exclusive_device;
	mtk_crtc->is_primary_layer_swap = path_data->is_primary_layer_swap;

	if (check_comp_in_crtc(path_data, MTK_DISP_CHIST))
		mtk_crtc->crtc_caps.crtc_ability |= ABILITY_CHIST;

	mtk_drm_crtc_init_logo_layer_on(mtk_crtc, pipe);
#endif

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp) {
		if (output_comp->id == DDP_COMPONENT_DSI0 ||
			output_comp->id == DDP_COMPONENT_DSI0_VIRTUAL) {
			mtk_crtc->emi_req = true;
			DDPMSG("%s CRTC-%d emi_req %d\n", __func__, pipe, mtk_crtc->emi_req);
		}
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_GUEST)
		mtk_ddp_comp_io_cmd(output_comp, NULL, GET_DEVICE_TYPE,
			&(mtk_crtc->is_shared_device));
#endif
		possible_crtcs = 1 << pipe;
		mtk_ddp_comp_io_cmd(output_comp, NULL, SET_CRTC_ID,
				    &possible_crtcs);

		if (output_comp->id == DDP_COMPONENT_DSI2_VIRTUAL) {
			mtk_crtc->virtual_path = true;
			if (mtk_crtc->panel_ext && mtk_crtc->panel_ext->params)
				mtk_crtc->offset_x = mtk_crtc->panel_ext->params->crop_width[0];
			else
				DDPMSG("%s CRTC%d panel is not connect\n", __func__, pipe);
		}
	}

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_GUEST)
	mtk_crtc->is_virtio_path = true;

#ifndef MTK_VIRT_WITH_NO_HOTPLUG
	if (drm_crtc_index(&mtk_crtc->base) == 1)
		mtk_set_hotplug_status(1);
#endif
#endif
}

/* auto superframe api */
#if !IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_GUEST)
void mtk_drm_crtc_enable_path(struct mtk_drm_crtc *mtk_crtc)
{
	struct cmdq_client *client;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv =
			mtk_crtc->base.dev->dev_private;
	int i;

	DDPMSG("%s CRTC%d enable_cnt=%d\n",
		__func__, drm_crtc_index(&mtk_crtc->base), mtk_crtc->enable_cnt);
	if (mtk_crtc->enable_cnt > 0) {
		mtk_crtc->enable_cnt++;
		DDPMSG("%s enable_cnt=%d\n", __func__, mtk_crtc->enable_cnt);
		return;
	}
	mtk_crtc->enable_cnt++;

	/* 3. power on cmdq client */
	client = mtk_crtc->gce_obj.client[CLIENT_CFG];
	cmdq_mbox_enable(client->chan);

#ifndef DRM_CMDQ_DISABLE
	if (disp_helper_get_stage() == DISP_HELPER_STAGE_NORMAL)
		mtk_crtc_prepare_instr(crtc);
#endif
/* 4. start trigger loop first to keep gce alive */
	if (mtk_crtc_with_trigger_loop(crtc)) {
		if (mtk_crtc_with_sodi_loop(crtc) &&
			(!mtk_crtc_is_frame_trigger_mode(crtc)))
			mtk_crtc_start_sodi_loop(crtc);

		if (mtk_crtc_with_event_loop(crtc) &&
				(mtk_crtc_is_frame_trigger_mode(crtc)))
			mtk_crtc_start_event_loop(crtc);
		mtk_crtc_start_trig_loop(crtc);
	}

	/* 2. prepare modules would be used in this CRTC */
	mtk_crtc_ddp_prepare(mtk_crtc);

	/* 5. connect path */
	mtk_crtc_connect_default_path(mtk_crtc);

	mtk_crtc->qos_ctx->last_hrt_req = 0;
	mtk_crtc->qos_ctx->last_larb_hrt_req = 0;
	for (i = 0; i < BW_CHANNEL_NR; i++)
		mtk_crtc->qos_ctx->last_channel_req[i] = 0;

	/* 6. config ddp engine */
	mtk_crtc_config_default_path(mtk_crtc);
	// CRTC_MMP_MARK(crtc_id, enable, 1, 3);

	/* 7. disconnect addon module and config */
	mtk_crtc_connect_addon_module(&mtk_crtc->base);
	DDPINFO("%s --\n", __func__);
}

void mtk_drm_crtc_disable_path(struct mtk_drm_crtc *mtk_crtc, bool need_wait)
{

	struct mtk_drm_private *priv =
			mtk_crtc->base.dev->dev_private;
#ifndef DRM_CMDQ_DISABLE
	struct cmdq_client *client;
#endif

	DDPMSG("%s enable_cnt=%d\n", __func__, mtk_crtc->enable_cnt);
	if (mtk_crtc->enable_cnt > 0)
		mtk_crtc->enable_cnt--;
	if (mtk_crtc->enable_cnt > 0) {
		DDPMSG("%s enable_cnt=%d\n", __func__, mtk_crtc->enable_cnt);
		return;
	}

	/* 4. stop CRTC */
	mtk_crtc_stop(mtk_crtc, need_wait);
	// CRTC_MMP_MARK(crtc_id, disable, 1, 1);

	/* 5. disconnect addon module and recover config */
	mtk_crtc_disconnect_addon_module(&mtk_crtc->base);

	/* 6. disconnect path */
	mtk_crtc_disconnect_default_path(mtk_crtc);

	/* 9. power off all modules in this CRTC */
	mtk_crtc_ddp_unprepare(mtk_crtc);

#ifndef DRM_CMDQ_DISABLE
	/* 8. power off cmdq client */
	client = mtk_crtc->gce_obj.client[CLIENT_CFG];
	cmdq_mbox_disable(client->chan);
#endif
}

bool mtk_drm_skip_update(struct drm_crtc *crtc)
{
	int index = 0;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *private = crtc->dev->dev_private;
	struct mtk_drm_crtc *mtk_crtc_p = NULL;

	if (mtk_crtc->virtual_path) {
		index = drm_crtc_index(crtc);
		mtk_crtc_p = to_mtk_crtc(private->crtc[index - 1]);
		while (mtk_crtc_p->virtual_path) {
			index--;
			mtk_crtc_p = to_mtk_crtc(private->crtc[index - 1]);
		}
		if (!mtk_crtc_p->enabled) {
			DDPMSG("the parent is disable, skip this commit\n");
			return true;
		}
	}
	return false;
}

void mtk_drm_crtc_disable_virtual(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	unsigned int crtc_id = drm_crtc_index(&mtk_crtc->base);

	if (!mtk_crtc->enabled) {
		DDPINFO("crtc%d skip %s\n", crtc_id, __func__);
		return;
	}
	mtk_drm_crtc_disable_path(mtk_crtc->p_mtk_crtc, true);

	DDPINFO("%s:%d crtc%d+\n", __func__, __LINE__, crtc_id);

	if (mtk_crtc_with_trigger_loop(crtc))
		mtk_crtc_stop_trig_loop(crtc);

	drm_crtc_vblank_off(crtc);

	mtk_crtc_set_status(crtc, false);

	DDPINFO("crtc%d %s:%d -\n", crtc_id, __func__, __LINE__);
}

void mtk_drm_crtc_enable_virtual(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	unsigned int crtc_id = drm_crtc_index(crtc);

	if (mtk_crtc->enabled) {
		DDPINFO("crtc%d skip %s\n", crtc_id, __func__);
		return;
	}

	if ((mtk_crtc->p_mtk_crtc) && !mtk_crtc->p_mtk_crtc->enabled) {
		DDPMSG("crtc%d mtk_crtc_attach_ddp_comp dp phy %s\n", crtc_id, __func__);
		/* attach the crtc to each componet */
		mtk_crtc_attach_ddp_comp(mtk_crtc->crtc_p, mtk_crtc->p_mtk_crtc->ddp_mode, true);
	}

	DDPMSG("crtc%d + %s\n", crtc_id, __func__);
	mtk_crtc_attach_ddp_comp(crtc, mtk_crtc->ddp_mode, true);

	if (mtk_crtc_with_trigger_loop(crtc))
		mtk_crtc_start_trig_loop(crtc);

	if (mtk_crtc->p_mtk_crtc)
		mtk_drm_crtc_enable_path(mtk_crtc->p_mtk_crtc);

	drm_crtc_vblank_on(crtc);

	mtk_crtc_set_status(crtc, true);

	DDPINFO("crtc%d - %s\n", crtc_id, __func__);
}

void mtk_drm_crtc_enable_auto(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_GUEST)
	if (mtk_crtc->is_virtio_path)
		mtk_drm_crtc_enable_virtio(crtc);
	else
#elif IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_HOST)
	else if (mtk_crtc->virtual_path)
		mtk_drm_crtc_enable_virtual(crtc);
	else
#endif
		mtk_drm_crtc_enable(crtc, false);
}

void mtk_drm_crtc_disable_auto(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_GUEST)
	if (mtk_crtc->is_virtio_path)
		mtk_drm_crtc_disable_virtio(crtc);
	else
#elif IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_HOST)
	if (mtk_crtc->virtual_path)
		mtk_drm_crtc_disable_virtual(crtc);
	else
#endif
		mtk_drm_crtc_disable(crtc, true);

}


void mtk_drm_crtc_phy_map(struct mtk_drm_private *private, int i)
{
	int j = 0;
	struct mtk_ddp_comp *phy_output_comp = NULL;
	struct mtk_ddp_comp *output_comp = NULL;
	struct mtk_drm_crtc *phy_mtk_crtc = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;

	mtk_crtc = to_mtk_crtc(private->crtc[i]);

	if (!mtk_crtc) {
		DDPMSG("[E] %s:%d invalid mtk_crtc\n", __func__, __LINE__);
		return;
	}

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (!output_comp) {
		DDPPR_ERR("%s:%d NULL output_comp\n", __func__, __LINE__);
		return;
	}

	mtk_crtc->p_mtk_crtc = NULL;
	mtk_crtc->crtc_p = NULL;

	for (j = 0; j < private->num_pipes; j++) {
		phy_mtk_crtc = to_mtk_crtc(private->crtc[j]);

		phy_output_comp = mtk_ddp_comp_request_output(phy_mtk_crtc);
		if (!phy_output_comp) {
			DDPPR_ERR("%s:%d NULL phy_output_comp\n",
				__func__, __LINE__);
			return;
		}
		if (mtk_ddp_comp_check_output_comp(output_comp->id, phy_output_comp->id)) {
			mtk_crtc->p_mtk_crtc = phy_mtk_crtc;
			mtk_crtc->crtc_p = private->crtc[j];
			DDPMSG("%s virt crtc-%d map phy crtc-%d\n", __func__, i, j);
		}
	}
}
/* auto superframe api */
#endif

void mtk_drm_crtc_backup_ovl_status_for_pq(struct mtk_drm_crtc *mtk_crtc,
					   struct cmdq_pkt *cmdq_handle)
{
	u32 crtc_idx = drm_crtc_index(&mtk_crtc->base);
	struct mtk_ddp_comp *comp;

	comp = mtk_crtc->first_exdma;

	if (IS_ERR_OR_NULL(comp))
		DDPMSG("%s CRTC%d failed to backup ovl status\n", __func__, crtc_idx);
	else
		mtk_ddp_comp_io_cmd(comp, cmdq_handle,
				    BACKUP_OVL_STATUS_FOR_PQ, NULL);
}

void mtk_drm_crtc_check_ovl_status_for_pq(struct mtk_drm_crtc *mtk_crtc)
{
	u32 crtc_idx = drm_crtc_index(&mtk_crtc->base);
	unsigned int ovl_status = 0;

	if (mtk_crtc->enabled) {
		ovl_status = *(unsigned int *)mtk_get_gce_backup_slot_va(mtk_crtc,
				DISP_SLOT_OVL_STATUS_FOR_PQ(crtc_idx));

		if (ovl_status & 1)
			DDPPR_ERR("%s CRTC%d ovl status error:0x%x\n",
				  __func__, crtc_idx, ovl_status);
	}
}

/*=======================================GUEST===================================================*/

#endif
