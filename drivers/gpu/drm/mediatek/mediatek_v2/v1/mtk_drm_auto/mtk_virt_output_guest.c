// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_panel.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_encoder.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/component.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/device.h>
#include "mtk_log.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_crtc.h"
#include "mtk_panel_ext.h"
#include "mtk_dp_api.h"
#include "mtk_dsi.h"
#include "mtk_drm_drv.h"
#include "mtk_dump.h"
#include "mtk_virtio_disp.h"
#include "mtk_virt_output_guest.h"
#include "mtk_drm_crtc_auto.h"
#include "mtk_drm_drv_auto.h"
#include "mtk_drm_ddp_comp_auto.h"

struct notify_dev {
	const char *name;
	struct device *dev;
	int index;
	int state;

	ssize_t (*print_name)(struct notify_dev *sdev, char *buf);
	ssize_t (*print_state)(struct notify_dev *sdev, char *buf);
};

#ifndef PANEL_MODE_NODE_NAME
#define PANEL_MODE_NODE_NAME "panel-mode-setting"
#endif
#ifndef PANEL_WIDTH_NODE_NAME
#define PANEL_WIDTH_NODE_NAME "panel-mode-width"
#endif
#ifndef PANEL_HEIGHT_NODE_NAME
#define PANEL_HEIGHT_NODE_NAME "panel-mode-height"
#endif
#ifndef PANEL_HFP_NODE_NAME
#define PANEL_HFP_NODE_NAME "panel-mode-hfp"
#endif
#ifndef PANEL_HSA_NODE_NAME
#define PANEL_HSA_NODE_NAME "panel-mode-hsa"
#endif
#ifndef PANEL_HBP_NODE_NAME
#define PANEL_HBP_NODE_NAME "panel-mode-hbp"
#endif
#ifndef PANEL_VSA_NODE_NAME
#define PANEL_VSA_NODE_NAME "panel-mode-vsa"
#endif
#ifndef PANEL_VBP_NODE_NAME
#define PANEL_VBP_NODE_NAME "panel-mode-vbp"
#endif
#ifndef PANEL_VFP_NODE_NAME
#define PANEL_VFP_NODE_NAME "panel-mode-vfp"
#endif
#ifndef PANEL_VREFRESH_NODE_NAME
#define PANEL_VREFRESH_NODE_NAME "panel-mode-vrefresh"
#endif
#ifndef PANEL_WIDTH_MM_NODE_NAME
#define PANEL_WIDTH_MM_NODE_NAME "panel-mode-width-mm"
#endif
#ifndef PANEL_HEIGHT_MM_NODE_NAME
#define PANEL_HEIGHT_MM_NODE_NAME "panel-mode-height-mm"
#endif

static struct mtk_virt *g_mtk_virt[MTK_DISP_MAX];

#ifdef MTK_VIRT_WITH_HOTPLUG
struct notify_dev virt_dp_notify_data;
#endif

static struct mtk_panel_params ext_params_dp = {
	.physical_width = 1920,
	.physical_height = 1080,
	.physical_width_um = 129,
	.physical_height_um = 64,
};

static struct mtk_panel_params ext_params = {
	.vfp_low_power = 112,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.ssc_enable = 0,
	.lane_swap_en = 0,
	.lane_swap[0][MIPITX_PHY_LANE_0] = MIPITX_PHY_LANE_0,
	.lane_swap[0][MIPITX_PHY_LANE_1] = MIPITX_PHY_LANE_1,
	.lane_swap[0][MIPITX_PHY_LANE_2] = MIPITX_PHY_LANE_3,
	.lane_swap[0][MIPITX_PHY_LANE_3] = MIPITX_PHY_LANE_2,
	.lane_swap[0][MIPITX_PHY_LANE_CK] = MIPITX_PHY_LANE_CK,
	.lane_swap[0][MIPITX_PHY_LANE_RX] = MIPITX_PHY_LANE_0,
	.lane_swap[1][MIPITX_PHY_LANE_0] = MIPITX_PHY_LANE_0,
	.lane_swap[1][MIPITX_PHY_LANE_1] = MIPITX_PHY_LANE_1,
	.lane_swap[1][MIPITX_PHY_LANE_2] = MIPITX_PHY_LANE_3,
	.lane_swap[1][MIPITX_PHY_LANE_3] = MIPITX_PHY_LANE_2,
	.lane_swap[1][MIPITX_PHY_LANE_CK] = MIPITX_PHY_LANE_CK,
	.lane_swap[1][MIPITX_PHY_LANE_RX] = MIPITX_PHY_LANE_0,
	.data_rate = 836,
	.physical_width = 1920,
	.physical_height = 1080,
	.physical_width_um = 129,
	.physical_height_um = 64,
};

#ifdef MTK_VIRT_WITH_HOTPLUG
struct class *switch_virt_class;
static atomic_t device_count;

static ssize_t state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;
	struct notify_dev *sdev = (struct notify_dev *)
		dev_get_drvdata(dev);

	if (sdev->print_state) {
		ret = sdev->print_state(sdev, buf);
		if (ret >= 0)
			return ret;
	}
	return sprintf(buf, "%d\n", sdev->state);
}

static ssize_t name_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int ret;
	struct notify_dev *sdev = (struct notify_dev *)
		dev_get_drvdata(dev);

	if (sdev->print_name) {
		ret = sdev->print_name(sdev, buf);
		if (ret >= 0)
			return ret;
	}
	return sprintf(buf, "%s\n", sdev->name);
}


static DEVICE_ATTR_RO(state);
static DEVICE_ATTR_RO(name);

static int mtk_virt_create_switch_class(void)
{
	if (!switch_virt_class) {
		switch_virt_class = class_create(THIS_MODULE, "switch");
		if (IS_ERR(switch_virt_class))
			return PTR_ERR(switch_virt_class);
		atomic_set(&device_count, 0);
	}
	return 0;
}

int mtk_virt_uevent_dev_register(struct notify_dev *sdev)
{
	int ret;

	if (!switch_virt_class) {
		ret = mtk_virt_create_switch_class();

		if (ret == 0) {
			DDPMSG("create switch class success\n");
		} else {
			DDPPR_ERR("create switch class fail\n");
			return ret;
		}
	}

	sdev->index = atomic_inc_return(&device_count);
	sdev->dev = device_create(switch_virt_class, NULL,
				  MKDEV(0, sdev->index), NULL, sdev->name);

	if (sdev->dev) {
		DDPDBG("device create ok, index:0x%x\n", sdev->index);
		ret = 0;
	} else {
		DDPPR_ERR("device create fail, index:0x%x\n", sdev->index);
		ret = -1;
		return ret;
	}

	ret = device_create_file(sdev->dev, &dev_attr_state);
	if (ret < 0) {
		device_destroy(switch_virt_class, MKDEV(0, sdev->index));
		DDPPR_ERR("switch: Failed to register driver %s\n",
		       sdev->name);
	}

	ret = device_create_file(sdev->dev, &dev_attr_name);
	if (ret < 0) {
		device_remove_file(sdev->dev, &dev_attr_state);
		DDPPR_ERR("switch: Failed to register driver %s\n",
		       sdev->name);
	}

	dev_set_drvdata(sdev->dev, sdev);
	sdev->state = 0;

	return ret;
}


int mtk_virt_dp_notify_uevent_user(struct notify_dev *sdev, int state)
{
	int ret;
	char *envp[3];
	char name_buf[120];
	char state_buf[120];

	if (!sdev)
		return -1;

	if (sdev->state != state)
		sdev->state = state;

	ret = snprintf(name_buf, sizeof(name_buf), "SWITCH_NAME=%s", sdev->name);
	if (ret < 0) {
		DDPPR_ERR("%s, snprintf fail\n", __func__);
		return ret;
	}
	envp[0] = name_buf;
	ret = snprintf(state_buf, sizeof(state_buf), "SWITCH_STATE=%d", sdev->state);
	if (ret < 0) {
		DDPPR_ERR("%s, snprintf fail\n", __func__);
		return ret;
	}
	envp[1] = state_buf;
	envp[2] = NULL;
	DDPMSG("uevent name:%s, state:%s\n", envp[0], envp[1]);

	kobject_uevent_env(&sdev->dev->kobj, KOBJ_CHANGE, envp);

	return 0;
}

static void mtk_virt_dp0_update_state(int state)
{
	DDPMSG("update_state %d.\n", state);
	virt_dp_notify_data.state = state;
}

void mtk_virt_dp0_hotplug_uevent(unsigned int event)
{
	struct mtk_virt *mtk_virt_dp0 = g_mtk_virt[MTK_DISP_DP0];

	if (mtk_virt_dp0->drm_dev) {
		DDPFUNC("notify drm framework hotplug event\n");
		drm_helper_hpd_irq_event(mtk_virt_dp0->drm_dev);
	} else {
		DDPFUNC("there is no drm dev\n");
	}

	mtk_virt_dp_notify_uevent_user(&virt_dp_notify_data,
				  event > 0 ? VIRT_DPTX_STATE_ACTIVE : VIRT_DPTX_STATE_NO_DEVICE);
}

void mtk_virt_dp0_hotplug_from_cb(unsigned int event)
{
	struct mtk_virt *mtk_virt_dp0 = g_mtk_virt[MTK_DISP_DP0];

	if (mtk_virt_dp0 && mtk_virt_dp0->hotplug_work_dp.init) {
		mtk_virt_dp0->hotplug_work_dp.event = event;
		schedule_work(&mtk_virt_dp0->hotplug_work_dp.work_hotplug);
	}
}
#endif

static inline struct mtk_virt *mtk_virt_from_encoder(struct drm_encoder *e)
{
	return container_of(e, struct mtk_virt, encoder);
}

static inline struct mtk_virt *mtk_virt_from_connector(struct drm_connector *c)
{
	return container_of(c, struct mtk_virt, connector);
}

static void mtk_virt_encoder_destroy(struct drm_encoder *encoder)
{
	drm_encoder_cleanup(encoder);
}

static const struct drm_encoder_funcs mtk_virt_encoder_funcs = {
	.destroy = mtk_virt_encoder_destroy,
};

static bool mtk_virt_encoder_mode_fixup(struct drm_encoder *encoder,
				       const struct drm_display_mode *mode,
				       struct drm_display_mode *adjusted_mode)
{
	return true;
}

static void mtk_virt_encoder_mode_set(struct drm_encoder *encoder,
				     struct drm_display_mode *mode,
				     struct drm_display_mode *adjusted_mode)
{
	struct mtk_virt *virt = mtk_virt_from_encoder(encoder);

	DDPMSG("%s+ w:%d H:%d.\n", __func__,
		adjusted_mode->vdisplay, adjusted_mode->hdisplay);

	drm_mode_copy(&virt->mode, adjusted_mode);
}

static int mtk_virt_atomic_check(struct drm_encoder *encoder,
				struct drm_crtc_state *crtc_state,
				struct drm_connector_state *conn_state)
{
	struct mtk_drm_crtc *mtk_crtc = container_of(conn_state->crtc,
						     struct mtk_drm_crtc, base);
	mtk_crtc->bpc = conn_state->connector->display_info.bpc;

	return 0;
}

static enum drm_connector_status mtk_virt_connector_detect(
	struct drm_connector *connector, bool force)
{
	struct mtk_virt *virt = mtk_virt_from_connector(connector);

	return ((virt->virt_ready) ? connector_status_connected :
		connector_status_disconnected);
}

int mtk_virt_get_panel_size(enum MTK_DDP_OUT_ID id,
	uint32_t *width, uint32_t *height)
{
	struct mtk_virt *mtk_virt_out = NULL;
	enum mtk_ddp_comp_id ddp_id = DDP_COMPONENT_ID_MAX;
	struct virtio_disp_cmd *cmd;
	int ret = 0;

	if (id < MTK_DISP_MAX) {
		mtk_virt_out = g_mtk_virt[id];
		if (mtk_virt_out) {
			*width = mtk_virt_out->mode.hdisplay;
			*height = mtk_virt_out->mode.vdisplay;
		} else {
#if IS_ENABLED(CONFIG_MTK_VIRTIO_DISP)
			if (id == MTK_DISP_DSI0)
				ddp_id = DDP_COMPONENT_DSI0;
			else if (id == MTK_DISP_DSI1)
				ddp_id = DDP_COMPONENT_DSI1;
			else if (id == MTK_DISP_DSI2)
				ddp_id = DDP_COMPONENT_DSI2;
			else if (id == MTK_DISP_DSI2_1)
				ddp_id = DDP_COMPONENT_DSI2_VIRTUAL;
			else if (id == MTK_DISP_DP0)
				ddp_id = DDP_COMPONENT_DP_INTF0;
			else if (id == MTK_DISP_DP1)
				ddp_id = DDP_COMPONENT_DP_INTF1;
			else if (id == MTK_DISP_EDP)
				ddp_id = DDP_COMPONENT_DISP_DVO;

			cmd = virtio_disp_cmd_create();
			cmd->req.cmd = VIRTIO_DISP_CMD_GET_PANEL;
			cmd->req.param.panel.output_comp_id = (uint32_t)ddp_id;
			ret = virtio_disp_cmd_submit(cmd);
			if (ret || cmd->rsp.rc || cmd->rsp.param.panel.width == 0 ||
				cmd->rsp.param.panel.height == 0 || cmd->rsp.param.panel.vrefresh == 0) {
				ret = -1;
				DDPMSG("error: get panel_timing fail.!\n");
			}

			*width = cmd->rsp.param.panel.width;
			*height = cmd->rsp.param.panel.height;
			virtio_disp_cmd_destroy(cmd);
#endif
		}
	}

	return ret;
}
EXPORT_SYMBOL(mtk_virt_get_panel_size);

struct mtk_ddp_comp *mtk_virt_get_all_output(int *count, int *con_id)
{
	int i = 0, j = 0;

	if (!count)
		return NULL;

	for (i = 0; i < MTK_DISP_MAX; i++) {
		if (g_mtk_virt[i] != NULL)
			j++;
	}

	if (*count == -1) {
		*count = j;
		return NULL;
	}

	for (i = 0, j = 0; i < MTK_DISP_MAX; i++) {
		if (g_mtk_virt[i] != NULL) {
			if (*count == j)
				break;
			j++;
		}
	}

	if (i < MTK_DISP_MAX) {
		if (con_id)
			*con_id = g_mtk_virt[i]->connector.base.id;
		return &g_mtk_virt[i]->ddp_comp;
	}

	return NULL;
}

static int mtk_virt_connector_get_modes(struct drm_connector *connector)
{
	struct drm_display_mode default_mode = {0};
	struct drm_display_mode *mode;
	struct mtk_virt *virt = mtk_virt_from_connector(connector);

	DDPINFO("%s+ %p\n", __func__, connector);

	if (virt->mode.hdisplay == 0) {
		DDPMSG("%s+ %p used default_mode.\n", __func__, connector);
		default_mode.hdisplay = 1920;
		default_mode.vdisplay = 1080;
		default_mode.hsync_start = default_mode.hdisplay + 14;
		default_mode.hsync_end = default_mode.hsync_start + 20;
		default_mode.htotal = default_mode.hsync_end + 20;
		default_mode.vsync_start = default_mode.vdisplay  + 230;
		default_mode.vsync_end = default_mode.vsync_start  + 2;
		default_mode.vtotal = default_mode.vsync_end  + 28;
		default_mode.clock = (u32)((u64)default_mode.vtotal
				* default_mode.htotal * 60 / 1000);
		default_mode.width_mm = 129;
		default_mode.height_mm = 64;

		mode = drm_mode_duplicate(connector->dev, &default_mode);
	} else {
		DDPMSG("%s %s type %d mode."DRM_MODE_FMT "\n",
			__func__, mtk_dump_comp_str(&virt->ddp_comp),
			virt->type, DRM_MODE_ARG(&virt->mode));
		mode = drm_mode_duplicate(connector->dev, &virt->mode);
	}
	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_encoder_helper_funcs mtk_virt_encoder_helper_funcs = {
	.mode_fixup = mtk_virt_encoder_mode_fixup,
	.mode_set = mtk_virt_encoder_mode_set,
	.atomic_check = mtk_virt_atomic_check,
};

static const struct drm_connector_funcs mtk_virt_connector_funcs = {
	.detect = mtk_virt_connector_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = drm_connector_cleanup,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static const struct drm_connector_helper_funcs mtk_virt_connector_helper_funcs = {
	.get_modes = mtk_virt_connector_get_modes,
};

static int mtk_virt_create_connector(struct drm_device *drm, struct mtk_virt *virt)
{
	int ret;

	if (virt->type == MTK_VIRT_DSI)
		ret = drm_connector_init(drm, &virt->connector, &mtk_virt_connector_funcs,
				 DRM_MODE_CONNECTOR_DSI);  //temp modify, need hwc modify to match
	else
		ret = drm_connector_init(drm, &virt->connector, &mtk_virt_connector_funcs,
				 DRM_MODE_CONNECTOR_DPI);

	if (ret) {
		DRM_ERROR("Failed to connector init to drm\n");
		return ret;
	}

	drm_connector_helper_add(&virt->connector,
				 &mtk_virt_connector_helper_funcs);

	virt->connector.dpms = DRM_MODE_DPMS_OFF;
	drm_connector_attach_encoder(&virt->connector, &virt->encoder);

	DDPINFO("%s connect...", __func__);

	return 0;
}

struct mtk_panel_ext *mtk_virt_get_panel_ext(struct mtk_ddp_comp *comp)
{
	struct mtk_virt *virt = container_of(comp, struct mtk_virt, ddp_comp);

	return virt->ext;
}

static int mtk_virt_get_frame_hrt_bw_base_by_datarate(
	struct mtk_drm_crtc *mtk_crtc, struct mtk_virt *virt)
{
	static unsigned long long bw_base;
	int hact = 0, vtotal = 0, vact = 1, vrefresh = 0;

	if (!mtk_crtc) {
		DDPDBG("%s mtk_crtc is null\n", __func__);
		return 0;
	}

	if (mtk_crtc->base.state) {
		hact = mtk_crtc->base.state->adjusted_mode.hdisplay;
		vtotal = mtk_crtc->base.state->adjusted_mode.vtotal;
		vact = mtk_crtc->base.state->adjusted_mode.vdisplay;
		vrefresh = drm_mode_vrefresh(&mtk_crtc->base.state->adjusted_mode);
	}

	bw_base = (unsigned long long)vact * (unsigned long long)hact * vrefresh * 4 / 1000;
	bw_base = bw_base * vtotal / vact;
	bw_base = bw_base / 1000;

	if (!bw_base)
		DDPMSG("%s crtc %d Frame Bw:%llu\n",
			__func__, drm_crtc_index(&mtk_crtc->base), bw_base);

	DDPDBG("Frame Bw:%llu", bw_base);
	return bw_base;
}

static void mtk_virt_get_panels_info(struct mtk_ddp_comp *comp, void *params)
{
	struct drm_device *dev;
	struct drm_encoder *encoder;
	int virt_cnt = 0;
	int crtc0_conn_id = -1;
	bool only_check_mode = false;
	struct mtk_virt *virt;
	struct mtk_drm_panels_info *panel_ctx;
	char *panel_name = NULL;
	struct mtk_ddp_comp *out_comp = NULL;
	struct mtk_drm_private *priv;

	virt = container_of(comp, struct mtk_virt, ddp_comp);

	panel_ctx = (struct mtk_drm_panels_info *)params;

	if (!panel_ctx) {
		DDPMSG("[E]%s invalid panel_info_ctx ptr\n", __func__);
		return;
	}

	if (virt->encoder.dev) {
		dev = virt->encoder.dev;
	} else {
		DDPMSG("[E]%s invalid drm device in mtk_virt\n", __func__);
		return;
	}

	if (panel_ctx->connector_cnt == -1)
		only_check_mode = true;

	drm_for_each_encoder(encoder, dev) {
		if (!only_check_mode) {
			virt = container_of(encoder, struct mtk_virt, encoder);

			panel_name = virt->panel_name;

			DDPMSG("%s encoder %d name %s id %d type %d possible crtc %x panel %s\n",
				__func__, virt_cnt,
				encoder->name, encoder->base.id, encoder->encoder_type,
				encoder->possible_crtcs,
				panel_name ? panel_name : "invalid panel name");

			if (panel_name) {
				strscpy(panel_ctx->panel_name[virt_cnt], panel_name, GET_PANELS_STR_LEN);
				panel_ctx->connector_obj_id[virt_cnt] = virt->connector.base.id;
				panel_ctx->possible_crtc[virt_cnt][0] = encoder->possible_crtcs;
			} else {
				DDPPR_ERR("%s NULL panel_name\n", __func__);
				break;
			}
		}
		++virt_cnt;
	}

	if (only_check_mode) {
		priv = dev->dev_private;

		/* PQ service request CRTC0's dsi output comp as default connector */
		if (priv && priv->crtc[0])
			out_comp = mtk_ddp_comp_request_output(to_mtk_crtc(priv->crtc[0]));
		if (out_comp)
			virt = container_of(out_comp, struct mtk_virt, ddp_comp);
		if (virt)
			crtc0_conn_id = virt->connector.base.id;

		DDPMSG("%s virt_cnt %d\n", __func__, virt_cnt);
		panel_ctx->connector_cnt = virt_cnt;
		panel_ctx->default_connector_id = crtc0_conn_id;
	}
}

static int mtk_virt_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			  enum mtk_ddp_io_cmd cmd, void *params)
{
	struct mtk_panel_ext **ext;
	struct drm_display_mode **mode;
	struct mtk_virt *virt = container_of(comp, struct mtk_virt, ddp_comp);
	bool *para;
#ifdef MTK_VIRT_WITH_HOTPLUG
	int value;
#endif

	switch (cmd) {
	case REQ_PANEL_EXT:
	{
		ext = (struct mtk_panel_ext **)params;
		*ext = mtk_virt_get_panel_ext(comp);
	}
		break;
	case DSI_GET_TIMING:
	{
		mode  = (struct drm_display_mode **)params;
		*mode = &virt->mode;
	}
		break;
	case GET_FRAME_HRT_BW_BY_DATARATE:
	{
		struct mtk_drm_crtc *crtc = comp->mtk_crtc;
		unsigned long long *base_bw =
			(unsigned long long *)params;

		*base_bw = mtk_virt_get_frame_hrt_bw_base_by_datarate(crtc, virt);
	}
		break;
	case GET_FRAME_HRT_BW_BY_MODE:
	{
		struct mtk_drm_crtc *crtc = comp->mtk_crtc;
		unsigned long long *base_bw = (unsigned long long *)params;

		*base_bw = mtk_virt_get_frame_hrt_bw_base_by_datarate(crtc, virt);
	}
		break;
	case SET_CRTC_ID:
		DDPMSG("virt set possible crtcs 0x%x\n", *(unsigned int *)params);
		virt->encoder.possible_crtcs = *(unsigned int *)params;
		break;
#if IS_ENABLED(CONFIG_MTK_VIRTIO_DISP)
	case GET_DEVICE_TYPE:
		para = (bool *)params;

		*para = virt->is_shared_device;
		break;
#endif
#ifdef MTK_VIRT_WITH_HOTPLUG
	case UPDATE_DP_CONNECT_STATE:
		value = *(int *)params;

		virt->virt_ready = (value > 0) ? true : false;
		mtk_virt_dp0_update_state(value);
		break;
#endif
	case GET_ALL_CONNECTOR_PANEL_NAME:
		mtk_virt_get_panels_info(comp, params);
		break;
	default:
		break;
	}

	return 0;
}

static const struct mtk_ddp_comp_funcs mtk_virt_funcs = {
	.io_cmd = mtk_virt_io_cmd,
};

bool mtk_drm_find_comp(struct mtk_ddp_comp *comp,
		      const struct mtk_crtc_path_data *path_data)
{
	unsigned int i, j, ddp_mode;
	const enum mtk_ddp_comp_id *path = NULL;

	if (path_data == NULL) {
		DDPMSG("%s path_data is null\n", __func__);
		return false;
	}

	for (ddp_mode = 0U; ddp_mode < DDP_MODE_NR; ddp_mode++)
		for (i = 0U; i < DDP_PATH_NR; i++) {
			path = path_data->path[ddp_mode][i];
			for (j = 0U; j < path_data->path_len[ddp_mode][i]; j++) {
				if (comp->id == path[j])
					return true;
			}
		}

	return false;
}

static int mtk_virt_bind(struct device *dev, struct device *master, void *data)
{
	struct mtk_virt *virt = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;
	int crtcs = 0;
	static int crtc_bit_count;

	DDPINFO("%s+\n", __func__);
	ret = mtk_ddp_comp_register(drm_dev, &virt->ddp_comp);
	if (ret < 0) {
		DDPMSG("Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	ret = mtk_drm_path_find_crtc(&virt->ddp_comp);
	if (ret < 0) {
		DDPMSG("Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return 0;
	}
	crtcs = BIT(ret);
	DDPMSG("%s %s possible_crtcs 0x%x crtc_bit_count %d\n",
		__func__, mtk_dump_comp_str(&virt->ddp_comp), crtcs, crtc_bit_count);

	ret = drm_encoder_init(drm_dev, &virt->encoder, &mtk_virt_encoder_funcs,
			       DRM_MODE_ENCODER_DSI, NULL);
	if (ret) {
		DDPMSG("Failed to initialize decoder: %d\n", ret);
		goto err_unregister;
	}
	drm_encoder_helper_add(&virt->encoder, &mtk_virt_encoder_helper_funcs);

	crtc_bit_count++;
	virt->encoder.possible_crtcs = crtcs;

	ret = drm_bridge_attach(&virt->encoder, virt->bridge, NULL, DRM_BRIDGE_ATTACH_NO_CONNECTOR);
	if (ret < 0) {
		/* Otherwise create our own connector and attach to a panel */
		ret = mtk_virt_create_connector(drm_dev, virt);
		if (ret < 0)
			goto err_cleanup;
	}

	virt->drm_dev = drm_dev;

#ifdef MTK_VIRT_WITH_HOTPLUG
	if (virt->ddp_comp.id == DDP_COMPONENT_DPI0_VIRTUAL) {
		if (virt->virt_ready)
			mtk_virt_dp0_hotplug_uevent(1);
	}
#endif
	DDPINFO("%s-\n", __func__);
	return 0;

err_cleanup:
	drm_encoder_cleanup(&virt->encoder);
err_unregister:
	mtk_ddp_comp_unregister(drm_dev, &virt->ddp_comp);
	return ret;
}

static void mtk_virt_unbind(struct device *dev, struct device *master,
			   void *data)
{
	struct mtk_virt *virt = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	drm_encoder_cleanup(&virt->encoder);
	mtk_ddp_comp_unregister(drm_dev, &virt->ddp_comp);
}

static const struct component_ops mtk_virt_component_ops = {
	.bind = mtk_virt_bind,
	.unbind = mtk_virt_unbind,
};

static const struct of_device_id mtk_virt_of_ids[] = {
	{ .compatible = "mediatek,mt8678-virt",},
	{},
};


MODULE_DEVICE_TABLE(of, mtk_virt_of_ids);
#define DUMMY_REG_BASE 0x329D0000

#if !IS_ENABLED(CONFIG_MTK_VIRTIO_DISP)
static int mtk_get_output_timing(struct device *dev, struct mtk_virt *virt)
{
	void __iomem *dummy_regs = NULL;

	dummy_regs = ioremap(DUMMY_REG_BASE, 0x4);

	DDPINFO("%s, %d, dummy_regs=0x%p type:%d\n", __func__, __LINE__, dummy_regs, virt->type);

	if (!dummy_regs) {
		DDPPR_ERR("%s: dummy_regs is NULL\n", __func__);
		return -1;
	}

	virt->virt_ready = 0;

	if (virt->type == MTK_VIRT_DSI) {
		virt->mode.hdisplay = DISP_REG_GET_FIELD(DSI_HDISPLAY_VALUE,
								dummy_regs + MT6991_OVL_MDP_RSZ0_DUMMY0);
		virt->mode.vdisplay = DISP_REG_GET_FIELD(DSI_VDISPLAY_VALUE,
								dummy_regs + MT6991_OVL_MDP_RSZ0_DUMMY0);
		virt->mode.hsync_start = DISP_REG_GET_FIELD(DSI_HSYNC_START_VALUE,
							dummy_regs + MT6991_OVL_MDP_RSZ0_DUMMY1);
		virt->mode.hsync_end = DISP_REG_GET_FIELD(DSI_HSYNC_END_VALUE,
							dummy_regs + MT6991_OVL_MDP_RSZ0_DUMMY1);
		virt->mode.vsync_start = DISP_REG_GET_FIELD(DSI_VSYNC_START_VALUE,
							dummy_regs + MT6991_OVL_MDP_RSZ0_DUMMY2);
		virt->mode.vsync_end = DISP_REG_GET_FIELD(DSI_VSYNC_END_VALUE,
							dummy_regs + MT6991_OVL_MDP_RSZ0_DUMMY2);
		virt->mode.htotal = DISP_REG_GET_FIELD(DSI_HSYNC_TOTAL_VALUE,
							dummy_regs + MT6991_OVL_MDP_RSZ0_DUMMY3);
		virt->mode.vtotal = DISP_REG_GET_FIELD(DSI_VSYNC_TOTAL_VALUE,
							dummy_regs + MT6991_OVL_MDP_RSZ0_DUMMY3);
		virt->mode.clock = readl(dummy_regs + MT6991_OVL_MDP_RSZ0_DUMMY4);
		virt->mode.height_mm = DISP_REG_GET_FIELD(DSI_HEIGHT_MM_VALUE,
							dummy_regs + MT6991_OVL_MDP_RSZ0_DUMMY5);
		virt->mode.width_mm = DISP_REG_GET_FIELD(DSI_WIDTH_MM_VALUE,
							dummy_regs + MT6991_OVL_MDP_RSZ0_DUMMY5);
		virt->virt_ready = 1;
	} else if (virt->type == MTK_VIRT_DPI) {
		virt->mode.hdisplay = DISP_REG_GET_FIELD(DP_HDISPLAY_VALUE,
								dummy_regs + MT6991_OVL_MDP_RSZ0_DUMMY6);
		virt->mode.vdisplay = DISP_REG_GET_FIELD(DP_VDISPLAY_VALUE,
								dummy_regs + MT6991_OVL_MDP_RSZ0_DUMMY6);
		virt->mode.hsync_start = DISP_REG_GET_FIELD(DP_HSYNC_START_VALUE,
							dummy_regs + MT6991_OVL_MDP_RSZ0_DUMMY7);
		virt->mode.hsync_end = DISP_REG_GET_FIELD(DP_HSYNC_END_VALUE,
							dummy_regs + MT6991_OVL_MDP_RSZ0_DUMMY7);
		virt->mode.vsync_start = DISP_REG_GET_FIELD(DP_VSYNC_START_VALUE,
							dummy_regs + MT6991_OVL_MDP_RSZ0_DUMMY8);
		virt->mode.vsync_end = DISP_REG_GET_FIELD(DP_VSYNC_END_VALUE,
							dummy_regs + MT6991_OVL_MDP_RSZ0_DUMMY8);
		virt->mode.htotal = DISP_REG_GET_FIELD(DP_HSYNC_TOTAL_VALUE,
							dummy_regs + MT6991_OVL_MDP_RSZ0_DUMMY9);
		virt->mode.vtotal = DISP_REG_GET_FIELD(DP_VSYNC_TOTAL_VALUE,
							dummy_regs + MT6991_OVL_MDP_RSZ0_DUMMY9);
		virt->mode.clock = readl(dummy_regs + MT6991_OVL_MDP_RSZ0_DUMMY10);
		virt->mode.height_mm = DISP_REG_GET_FIELD(DP_HEIGHT_MM_VALUE,
							dummy_regs + MT6991_OVL_MDP_RSZ0_DUMMY11);
		virt->mode.width_mm = DISP_REG_GET_FIELD(DP_WIDTH_MM_VALUE,
							dummy_regs + MT6991_OVL_MDP_RSZ0_DUMMY11);
		virt->virt_ready = DISP_REG_GET_FIELD(DP_INTF0_CONNECTOR_READY,
							dummy_regs + MT6991_OVL_MDP_RSZ0_DUMM20);
	}

	DDPMSG("%s %d rdy %d h %d v %d hs %d he %d ht %d vs %d ve %d vt %d cLk %d wmm %d hmm %d\n",
		__func__, __LINE__,
		virt->virt_ready,
		virt->mode.hdisplay,
		virt->mode.vdisplay,
		virt->mode.hsync_start,
		virt->mode.hsync_end,
		virt->mode.htotal,
		virt->mode.vsync_start,
		virt->mode.vsync_end,
		virt->mode.vtotal,
		virt->mode.clock,
		virt->mode.width_mm,
		virt->mode.height_mm);

	if (virt->mode.hdisplay > 3840 || virt->mode.hdisplay < 0 ||
		virt->mode.vdisplay > 2160 || virt->mode.vdisplay < 0 ||
		virt->mode.htotal > 5000 || virt->mode.htotal < 0 ||
		virt->mode.vtotal > 4000 || virt->mode.vdisplay < 0 ||
		virt->mode.clock == 0) {
		virt->virt_ready = 0;
		return -1;
	}

	return 0;
}

static int mtk_get_default_timing(struct device *dev, struct mtk_virt *virt)
{
	int ret;
	u32 read_value = 0;
	char mode_name[] = PANEL_MODE_NODE_NAME;
	struct device_node *mode_node = NULL;

	mode_node = of_find_node_by_name(dev->of_node, mode_name);
	if (mode_node) {
		ret = of_property_read_u32(mode_node, PANEL_WIDTH_NODE_NAME, &read_value);
		if (!ret) {
			virt->mode.hdisplay = read_value;
			DDPMSG("disp_mode.hdisplay = %d\n", read_value);
		} else {
			DDPMSG("error: read width error!\n");
			return -1;
		}
		ret = of_property_read_u32(mode_node, PANEL_HEIGHT_NODE_NAME, &read_value);
		if (!ret) {
			virt->mode.vdisplay = read_value;
			DDPMSG("disp_mode.vdisplay = %d\n", read_value);
		} else {
			DDPMSG("error: read height error!\n");
			return -1;
		}
		ret = of_property_read_u32(mode_node, PANEL_HFP_NODE_NAME, &read_value);
		if (!ret) {
			virt->mode.hsync_start = virt->mode.hdisplay + read_value;
			DDPMSG("disp_mode.hfp = %d\n", read_value);
		} else {
			DDPMSG("error: read hfp error!\n");
			return -1;
		}
		ret = of_property_read_u32(mode_node, PANEL_HSA_NODE_NAME, &read_value);
		if (!ret) {
			virt->mode.hsync_end = virt->mode.hsync_start + read_value;
			DDPMSG("disp_mode.hsa = %d\n", read_value);
		} else {
			DDPMSG("error: read hsa error!\n");
			return -1;
		}
		ret = of_property_read_u32(mode_node, PANEL_HBP_NODE_NAME, &read_value);
		if (!ret) {
			virt->mode.htotal = virt->mode.hsync_end + read_value;
			DDPMSG("disp_mode.hbp = %d\n", read_value);
		} else {
			DDPMSG("error: read hbp error!\n");
			return -1;
		}
		ret = of_property_read_u32(mode_node, PANEL_VFP_NODE_NAME, &read_value);
		if (!ret) {
			virt->mode.vsync_start = virt->mode.vdisplay + read_value;
			DDPMSG("disp_mode.vfp = %d\n", read_value);
		} else {
			DDPMSG("error: read vfp error!\n");
			return -1;
		}
		ret = of_property_read_u32(mode_node, PANEL_VSA_NODE_NAME, &read_value);
		if (!ret) {
			virt->mode.vsync_end = virt->mode.vsync_start + read_value;
			DDPMSG("disp_mode.vsa = %d\n", read_value);
		} else {
			DDPMSG("error: read vsa error!\n");
			return -1;
		}
		ret = of_property_read_u32(mode_node, PANEL_VBP_NODE_NAME, &read_value);
		if (!ret) {
			virt->mode.vtotal = virt->mode.vsync_end + read_value;
			DDPMSG("disp_mode.vbp = %d\n", read_value);
		} else {
			DDPMSG("error: read vbp error!\n");
			return -1;
		}
		ret = of_property_read_u32(mode_node, PANEL_VREFRESH_NODE_NAME, &read_value);
		if (!ret) {
			virt->mode.clock = (u32)((u64)virt->mode.vtotal
				* virt->mode.htotal * read_value / 1000);
			DDPMSG("disp_mode.clock = %d\n", virt->mode.clock);
		} else {
			DDPMSG("error: read fps error!\n");
			return -1;
		}
		ret = of_property_read_u32(mode_node, PANEL_WIDTH_MM_NODE_NAME, &read_value);
		if (!ret) {
			virt->mode.width_mm = read_value;
			DDPMSG("disp_mode.width_mm = %d\n", read_value);
		} else {
			DDPMSG("width_mm not set!\n");
			return -1;
		}
		ret = of_property_read_u32(mode_node, PANEL_HEIGHT_MM_NODE_NAME, &read_value);
		if (!ret) {
			virt->mode.height_mm = read_value;
			DDPMSG("disp_mode.height_mm = %d\n", read_value);
		} else {
			DDPMSG("height_mm not set!\n");
			return -1;
		}
		of_node_put(mode_node);
	} else {
		DDPMSG("error: %s error!\n", __func__);
		return -1;
	}
	return 0;
}

static void mtk_vir_set_ext_params(struct mtk_virt *virt)
{
	struct mtk_panel_params *params = virt->ext->params;

	params->physical_width = virt->mode.hdisplay;
	params->physical_height = virt->mode.vdisplay;
	params->physical_width_um = virt->mode.width_mm;
	params->physical_height_um = virt->mode.height_mm;
}
#else
static int mtk_virt_get_panel_timing(struct device *dev,
	struct mtk_virt *virt, mtk_virt_hotplug_cb cb)
{
	struct virtio_disp_cmd *cmd;
	int ret = 0;

	cmd = virtio_disp_cmd_create();
	cmd->req.cmd = VIRTIO_DISP_CMD_GET_PANEL;
	if (cb)
		cmd->cb = cb;
	cmd->req.param.panel.output_comp_id = mtk_ddp_comp_get_phy_output_comp(virt->ddp_comp.id);

	DDPMSG("%s size of rsp.param %lu\n", __func__, sizeof(cmd->rsp.param));

	ret = virtio_disp_cmd_submit(cmd);
	if (ret || cmd->rsp.rc || cmd->rsp.param.panel.width == 0 ||
		cmd->rsp.param.panel.height == 0 || cmd->rsp.param.panel.vrefresh == 0) {
		ret = -1;
		DDPMSG("error: get panel_timing fail.!\n");
		goto out;
	}
	strscpy_pad(virt->panel_name, cmd->rsp.param.panel.panel_name,
				MAX_PANEL_NAME_LEN - 1);
	virt->mode.hdisplay = cmd->rsp.param.panel.width;
	virt->mode.vdisplay = cmd->rsp.param.panel.height;
	virt->mode.width_mm = cmd->rsp.param.panel.width_mm;
	virt->mode.height_mm = cmd->rsp.param.panel.height_mm;
	virt->mode.hsync_start = virt->mode.hdisplay + cmd->rsp.param.panel.hfp;
	virt->mode.hsync_end = virt->mode.hsync_start + cmd->rsp.param.panel.hsa;
	virt->mode.htotal = virt->mode.hsync_end + cmd->rsp.param.panel.hbp;
	virt->mode.vsync_start = virt->mode.vdisplay + cmd->rsp.param.panel.vfp;
	virt->mode.vsync_end = virt->mode.vsync_start + cmd->rsp.param.panel.vsa;
	virt->mode.vtotal = virt->mode.vsync_end + cmd->rsp.param.panel.vbp;
	virt->mode.clock = (u32)((u64)virt->mode.vtotal * virt->mode.htotal * cmd->rsp.param.panel.vrefresh / 1000);

	virt->conn_caps.conn_caps.lcm_degree = cmd->rsp.param.panel.degree;
	if ((virt->ext != NULL) && virt->ext->params != NULL) {
		virt->ext->params->lcm_degree = cmd->rsp.param.panel.degree;
		virt->ext->params->physical_width = virt->mode.hdisplay;
		virt->ext->params->physical_height = virt->mode.vdisplay;
		virt->ext->params->physical_width_um = virt->mode.width_mm;
		virt->ext->params->physical_height_um = virt->mode.height_mm;

		DDPMSG("%s %ux%u@%u degree:%u, mm:%ux%u", __func__, virt->mode.hdisplay, virt->mode.vdisplay,
			cmd->rsp.param.panel.vrefresh, virt->ext->params->lcm_degree,
			virt->ext->params->physical_width_um,
			virt->ext->params->physical_height_um);
	}
	DDPMSG("%s h:%u,%u,%u; v:%u,%u,%u; clock:%u name:%s\n",
		__func__,
		virt->mode.hsync_start, virt->mode.hsync_end, virt->mode.htotal,
		virt->mode.vsync_start, virt->mode.vsync_end, virt->mode.vtotal,
		virt->mode.clock,
		virt->panel_name);
	virt->virt_ready = 1;

out:
	virtio_disp_cmd_destroy(cmd);

	return ret;
}
#endif

static int mtk_virt_init_panel(struct device *dev, struct mtk_virt *virt)
{
	struct mtk_panel_ext *ext;

	ext = devm_kzalloc(dev, sizeof(struct mtk_panel_ctx), GFP_KERNEL);
	if (!ext)
		return -ENOMEM;

	if (virt->type == MTK_VIRT_DSI)
		ext->params = &ext_params;
	else
		ext->params = &ext_params_dp;

	virt->ext = ext;

	return 0;
}

#ifdef MTK_VIRT_WITH_HOTPLUG
void mtk_set_hotplug_status(int suspend_done)
{
	struct mtk_virt *mtk_virt_dp0 = g_mtk_virt[MTK_DISP_DP0];

	atomic_set(&mtk_virt_dp0->hotplug_done, suspend_done);

	if (suspend_done)
		wake_up_interruptible(&mtk_virt_dp0->guest_suspend);
}

static void mtk_hypdp_hotplug(struct work_struct *work)
{
	unsigned int event = container_of(work, struct hotplug_work, work_hotplug)->event;
	struct virtio_disp_cmd *cmd;
	long ret = 0;
	struct mtk_virt *mtk_virt_dp0 = g_mtk_virt[MTK_DISP_DP0];

	if (atomic_read(&mtk_virt_dp0->hotplug_done) == 1) {
		if (event) {
			if (mtk_virt_dp0->mode.hdisplay == 0 ||
				mtk_virt_dp0->mode.vdisplay == 0) {
				mtk_virt_get_panel_timing(NULL, mtk_virt_dp0, NULL);
			}
			mtk_virt_dp0_hotplug_uevent(event);
		} else
			DDPMSG("[W] %s DP suspend, ignore disconnect event.\n", __func__);
	} else {
		if (event) {
			DDPMSG("[W] %s DP resumed, ignore connect event.\n", __func__);
		} else {
			mtk_virt_dp0_hotplug_uevent(event);
			ret = wait_event_interruptible_timeout(
				mtk_virt_dp0->guest_suspend,
				atomic_read(&mtk_virt_dp0->hotplug_done) == 1,
				2*HZ);
			if (!ret)
				DDPMSG("[E] %s wait hotplug out done timeout, ret=%ld\n", __func__, ret);

			DDPMSG("%s %d wait hotplug out done %d.\n",
			       __func__, __LINE__, atomic_read(&mtk_virt_dp0->hotplug_done));

			cmd = virtio_disp_cmd_create();
			cmd->req.cmd = VIRTIO_DISP_CMD_HOTPLUG_STATUS;
			cmd->req.param.event = 1;
			(void)virtio_disp_cmd_submit(cmd);
			virtio_disp_cmd_destroy(cmd);
		}
	}
}
#endif

static int mtk_virt_init_device(struct device *dev, struct mtk_virt *virt)
{
	int ret = -1;
	mtk_virt_hotplug_cb cb = NULL;

	if (!virt)
		return ret;

	DDPINFO("%s+ virt->ddp_comp.id:%d.\n", __func__, virt->ddp_comp.id);

#if IS_ENABLED(CONFIG_MTK_VIRTIO_DISP)
	if (mtk_drm_path_find_crtc(&virt->ddp_comp) == 0)
		virt->is_first_path = true;

	DDPMSG("%s %s is_first_path %d.\n",
		__func__, mtk_dump_comp_str(&virt->ddp_comp),
		virt->is_first_path);
#endif

#ifdef MTK_VIRT_WITH_HOTPLUG
	if ((virt->ddp_comp.id == DDP_COMPONENT_DSI0_VIRTUAL) ||
		(virt->ddp_comp.id == DDP_COMPONENT_DSI1_VIRTUAL) ||
		(virt->ddp_comp.id == DDP_COMPONENT_DSI2_VIRTUAL) ||
		(virt->ddp_comp.id == DDP_COMPONENT_DSI2_1_VIRTUAL))
		virt->type = MTK_VIRT_DSI;
	else if ((virt->ddp_comp.id == DDP_COMPONENT_DPI0_VIRTUAL) ||
		(virt->ddp_comp.id == DDP_COMPONENT_DPI1_VIRTUAL)) {
#if IS_ENABLED(CONFIG_MTK_VIRTIO_DISP)
		if (virt->is_first_path)
			virt->type = MTK_VIRT_DSI;
		else
#endif
			virt->type = MTK_VIRT_DPI;
		cb = mtk_virt_dp0_hotplug_from_cb;
	} else if (virt->ddp_comp.id == DDP_COMPONENT_DVO_VIRTUAL) {
#if IS_ENABLED(CONFIG_MTK_VIRTIO_DISP)
		if (virt->is_first_path)
			virt->type = MTK_VIRT_DSI;
		else
#endif
			virt->type = MTK_VIRT_DVO;
	}
#else
	virt->type = MTK_VIRT_DSI;
#endif

	virt->panel = NULL;
#if IS_ENABLED(CONFIG_MTK_VIRTIO_DISP)
	ret = mtk_virt_get_panel_timing(dev, virt, cb);
	if (mtk_drm_path_is_shared_device(&virt->ddp_comp))
		virt->is_shared_device = true;

	DDPMSG("%s %s is_shared_device %d.\n",
		__func__, mtk_dump_comp_str(&virt->ddp_comp),
		virt->is_shared_device);
#else
	if (ret < 0) {
		ret = mtk_get_output_timing(dev, virt);
		if (ret < 0)
			mtk_get_default_timing(dev, virt);

		mtk_vir_set_ext_params(virt);
	}
#endif
	DDPINFO("%s-\n", __func__);

	return 0;
}

static int mtk_virt_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_virt *virt;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	DDPINFO("%s+\n", __func__);

	virt = devm_kzalloc(dev, sizeof(*virt), GFP_KERNEL);
	if (!virt)
		return -ENOMEM;

	virt->dev = dev;
	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_VIRT_OUTPUT);
	if ((int)comp_id < 0) {
		DDPMSG("Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}
	DDPINFO("%s comp_id:%d\n", __func__, comp_id);
	ret = mtk_ddp_comp_init(dev, dev->of_node, &virt->ddp_comp, comp_id,
				&mtk_virt_funcs);
	if (ret) {
		DDPMSG("Failed to initialize component: %d\n", ret);
		return ret;
	}

	virt->alias = mtk_ddp_comp_get_alias(virt->ddp_comp.id);

	mtk_virt_init_panel(dev, virt);

	if (comp_id == DDP_COMPONENT_DSI0_VIRTUAL)
		g_mtk_virt[MTK_DISP_DSI0] = virt;
	else if (comp_id == DDP_COMPONENT_DPI0_VIRTUAL) {
		g_mtk_virt[MTK_DISP_DP0] = virt;
#ifdef MTK_VIRT_WITH_HOTPLUG
		virt_dp_notify_data.name = "hdmi";	/* now hwc not support DP */
		virt_dp_notify_data.index = 0;
		virt_dp_notify_data.state = VIRT_DPTX_STATE_NO_DEVICE;

		ret = mtk_virt_uevent_dev_register(&virt_dp_notify_data);
		if (ret)
			DDPPR_ERR("switch_dev_register failed, returned:%d!\n", ret);

		virt->hotplug_work_dp.event = 0;
		INIT_WORK(&virt->hotplug_work_dp.work_hotplug, mtk_hypdp_hotplug);
		virt->hotplug_work_dp.init = true;

		virt->hotplug_wq = create_workqueue("mtk_hypdp_suspend");
		init_waitqueue_head(&virt->guest_suspend);
#endif
	} else if (comp_id == DDP_COMPONENT_DSI1_VIRTUAL)
		g_mtk_virt[MTK_DISP_DSI1] = virt;
	else if (comp_id == DDP_COMPONENT_DSI2_VIRTUAL)
		g_mtk_virt[MTK_DISP_DSI2] = virt;
	else if (comp_id == DDP_COMPONENT_DPI1_VIRTUAL)
		g_mtk_virt[MTK_DISP_DP1] = virt;
	else if (comp_id == DDP_COMPONENT_DVO_VIRTUAL)
		g_mtk_virt[MTK_DISP_EDP] = virt;
	else if (comp_id == DDP_COMPONENT_DSI2_1_VIRTUAL)
		g_mtk_virt[MTK_DISP_DSI2_1] = virt;

	mtk_virt_init_device(dev, virt);

	platform_set_drvdata(pdev, virt);
	ret = component_add(dev, &mtk_virt_component_ops);
	if (ret) {
		DDPMSG("Failed to add component: %d\n", ret);
		return ret;
	}

	DDPINFO("%s-\n", __func__);

	return 0;
}

static void mtk_virt_remove(struct platform_device *pdev)
{
}

struct platform_driver mtk_virt_driver = {
	.probe = mtk_virt_probe,
	.remove = mtk_virt_remove,
	.driver = {
		.name = "mediatek-virt",
		.of_match_table = mtk_virt_of_ids,
	},
};

