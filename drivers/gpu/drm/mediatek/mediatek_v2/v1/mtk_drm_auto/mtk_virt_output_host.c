// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
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
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include "mtk_log.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_crtc.h"
#include "mtk_panel_ext.h"
#include "mtk_dp_api.h"
#include "mtk_dsi.h"
#include "mtk_drm_drv.h"
#include "mtk_dump.h"

enum MTK_VIRT_TYPE {
	MTK_VIRT_DSI,
	MTK_VIRT_DP,
};

struct mtk_virt {
	struct mtk_ddp_comp ddp_comp;
	struct drm_encoder encoder;
	struct drm_connector connector;
	struct drm_panel *panel;
	struct drm_bridge *bridge;
	struct mipi_dsi_host host;
	struct device *dev;
	struct drm_display_mode mode;
	struct mtk_panel_ext *ext;
	enum MTK_VIRT_TYPE type;
	enum drm_connector_status connect_status;
	int alias;
	bool enabled;
};

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

static void mtk_virt_encoder_disable(struct drm_encoder *encoder)
{
	struct mtk_virt *virt = mtk_virt_from_encoder(encoder);

	DDPINFO("%s+ comp %s\n", __func__, mtk_dump_comp_str_id(virt->ddp_comp.id));
	if (virt->ddp_comp.id == DDP_COMPONENT_DSI2_VIRTUAL) {
		drm_panel_disable(virt->panel);
		drm_panel_unprepare(virt->panel);
	}

	virt->enabled = false;

	DDPINFO("%s: -\n", __func__);
}

static void mtk_virt_encoder_enable(struct drm_encoder *encoder)
{
	struct mtk_virt *virt = mtk_virt_from_encoder(encoder);

	DDPINFO("%s+ comp %s\n", __func__, mtk_dump_comp_str_id(virt->ddp_comp.id));
	if (virt->ddp_comp.id == DDP_COMPONENT_DSI2_VIRTUAL) {
		drm_panel_prepare(virt->panel);
		drm_panel_enable(virt->panel);
	}

	virt->enabled = true;

	DDPINFO("%s: -\n", __func__);
}

static enum drm_connector_status mtk_virt_connector_detect(
	struct drm_connector *connector, bool force)
{
	struct mtk_virt *virt = mtk_virt_from_connector(connector);

	if (!virt) {
		DDPMSG("[E] %s invalid virt pointer\n", __func__);
		return connector_status_disconnected;
	}

	if (virt->ddp_comp.id == DDP_COMPONENT_DSI2_VIRTUAL)
		return virt->connect_status;

	return connector_status_disconnected;
}

static int mtk_virt_connector_get_modes(struct drm_connector *connector)
{
	struct drm_display_mode default_mode = {0};
	struct drm_display_mode *mode;
	int ret;
	struct mtk_virt *virt = mtk_virt_from_connector(connector);

	DDPINFO("%s+ %p\n", __func__, connector);

	if (virt->type == MTK_VIRT_DSI)
		return drm_panel_get_modes(virt->panel, connector);

	if (virt->mode.hdisplay == 0) {
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
		DDPMSG("%s+ %p used virtual mode\n", __func__, connector);
		mode = drm_mode_duplicate(connector->dev, &virt->mode);
	}

	if (mode) {
		drm_mode_set_name(mode);
		mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
		drm_mode_probed_add(connector, mode);
	} else {
		DRM_ERROR("Mode is NULL before setting name\n");
		return 0;
	}

	return 1;
}

static const struct drm_encoder_helper_funcs mtk_virt_encoder_helper_funcs = {
	.mode_fixup = mtk_virt_encoder_mode_fixup,
	.mode_set = mtk_virt_encoder_mode_set,
	.atomic_check = mtk_virt_atomic_check,
	.disable = mtk_virt_encoder_disable,
	.enable = mtk_virt_encoder_enable,
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
				 DRM_MODE_CONNECTOR_DSI);
	else
		ret = drm_connector_init(drm, &virt->connector, &mtk_virt_connector_funcs,
				 DRM_MODE_CONNECTOR_DisplayPort);
	if (ret) {
		DRM_ERROR("Failed to connector init to drm\n");
		return ret;
	}
	drm_connector_helper_add(&virt->connector,
				 &mtk_virt_connector_helper_funcs);
	virt->connector.dpms = DRM_MODE_DPMS_OFF;
	drm_connector_attach_encoder(&virt->connector, &virt->encoder);

	return 0;
}

struct mtk_panel_ext *mtk_virt_get_panel_ext(struct mtk_ddp_comp *comp)
{
	struct mtk_virt *virt = container_of(comp, struct mtk_virt, ddp_comp);

	return virt->ext;
}

unsigned long long mtk_virt_get_frame_hrt_bw_base_by_datarate(struct mtk_drm_crtc *mtk_crtc,
							      struct mtk_virt *virt)
{
	unsigned long long bw_base;
	int hact = mtk_crtc->base.state->adjusted_mode.hdisplay;
	int vtotal = mtk_crtc->base.state->adjusted_mode.vtotal;
	int vact = mtk_crtc->base.state->adjusted_mode.vdisplay;
	int vrefresh = drm_mode_vrefresh(&mtk_crtc->base.state->adjusted_mode);
	int crtc_idx = drm_crtc_index(&mtk_crtc->base);
	int tmp_vact = 0;

	//vdo mode
	bw_base = DO_COMMON_DIV((unsigned long long)vact * hact * vrefresh * 4, 1000);
	bw_base = DO_COMMON_DIV(bw_base * vtotal, vact);
	bw_base = DO_COMMON_DIV(bw_base, 1000);

	DDPINFO("%s crtc%d, vdo mode bw_base:%llu, vrefresh:%d\n",
		__func__, crtc_idx, bw_base, vrefresh);

	return bw_base;
}

static int mtk_virt_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			  enum mtk_ddp_io_cmd cmd, void *params)
{
	struct mtk_panel_ext **ext;
	struct mtk_virt *virt = container_of(comp, struct mtk_virt, ddp_comp);

	switch (cmd) {
	case REQ_PANEL_EXT:
		ext = (struct mtk_panel_ext **)params;
		*ext = mtk_virt_get_panel_ext(comp);
		break;
	case SET_CRTC_ID:
		DDPINFO("virt set possible crtcs 0x%x\n", *(unsigned int *)params);
		virt->encoder.possible_crtcs = *(unsigned int *)params;
		break;
	case GET_FRAME_HRT_BW_BY_DATARATE:
	{
		struct mtk_drm_crtc *crtc = comp->mtk_crtc;
		unsigned long long *base_bw =
			(unsigned long long *)params;

		*base_bw = mtk_virt_get_frame_hrt_bw_base_by_datarate(crtc, virt);
	}
		break;
	case GET_ENABLE_READY_STATUS:
	{
		*(bool *)params = virt->enabled;
	}
		break;
	default:
		break;
	}

	return 0;
}

static const struct mtk_ddp_comp_funcs mtk_virt_funcs = {
	.io_cmd = mtk_virt_io_cmd,
};

static bool mtk_drm_find_comp(struct mtk_ddp_comp ddp_comp,
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
				if (ddp_comp.id == path[j])
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

	DDPINFO("%s+\n", __func__);
	ret = mtk_ddp_comp_register(drm_dev, &virt->ddp_comp);
	if (ret < 0) {
		DDPMSG("Failed to register component %s: %d\n",
			    dev->of_node->full_name, ret);
		return ret;
	}
	ret = drm_encoder_init(drm_dev, &virt->encoder, &mtk_virt_encoder_funcs,
			       DRM_MODE_ENCODER_DSI, NULL);
	if (ret) {
		DDPMSG("Failed to initialize decoder: %d\n", ret);
		goto err_unregister;
	}
	drm_encoder_helper_add(&virt->encoder, &mtk_virt_encoder_helper_funcs);

	ret = drm_bridge_attach(&virt->encoder, virt->bridge, NULL, DRM_BRIDGE_ATTACH_NO_CONNECTOR);
	if (ret < 0) {
		/* Otherwise create our own connector and attach to a panel */
		ret = mtk_virt_create_connector(drm_dev, virt);
		if (ret < 0)
			goto err_cleanup;
	}

	if (virt && virt->ext && virt->ext->funcs && virt->ext->funcs->get_link_status &&
		virt->panel && (virt->ext->funcs->get_link_status(virt->panel) & 0x1))
		virt->connect_status = connector_status_connected;
	else
		virt->connect_status = connector_status_disconnected;

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
	if (virt->type == MTK_VIRT_DSI)
		mipi_dsi_host_unregister(&virt->host);
	mtk_ddp_comp_unregister(drm_dev, &virt->ddp_comp);
}

static const struct component_ops mtk_virt_component_ops = {
	.bind = mtk_virt_bind,
	.unbind = mtk_virt_unbind,
};

static const struct of_device_id mtk_virt_of_ids[] = {
	{.compatible = "mediatek,mt8678-virt",},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_virt_of_ids);
static int mtk_virt_dsi_host_attach(struct mipi_dsi_host *host,
			       struct mipi_dsi_device *device)
{
	return 0;
}

static const struct mipi_dsi_host_ops mtk_dsi_ops = {
	.attach = mtk_virt_dsi_host_attach,
};

static __maybe_unused int mtk_virt_init_device(struct device *dev, struct mtk_virt *virt)
{
	struct device_node *node = NULL;
	struct device_node *remote_node, *endpoint;
	struct drm_bridge *bridge;
	struct drm_panel *panel = NULL;
	int ret;

	DDPINFO("%s+:%s\n", __func__, dev->of_node->name);
	if (!virt)
		return -1;
	if (virt->ddp_comp.id == DDP_COMPONENT_DSI2_VIRTUAL)
		virt->type = MTK_VIRT_DSI;

	if (virt->type == MTK_VIRT_DSI) {
		virt->host.ops = &mtk_dsi_ops;
		virt->host.dev = virt->dev;
		ret = mipi_dsi_host_register(&virt->host);
		if (ret < 0) {
			pr_info("failed to register DSI virtual host:%d\n", ret);
			return -EPROBE_DEFER;
		}
	}
	node = dev->of_node;
	endpoint = of_graph_get_next_endpoint(node, NULL);
	if (endpoint) {
		remote_node = of_graph_get_remote_port_parent(endpoint);
		if (!remote_node) {
			mipi_dsi_host_unregister(&virt->host);
			DDPMSG("No bridge or panel connected\n");
			return -ENODEV;
		}
		bridge = of_drm_find_bridge(remote_node);
		panel = of_drm_find_panel(remote_node);
		of_node_put(remote_node);
		if (IS_ERR_OR_NULL(bridge) && IS_ERR_OR_NULL(panel)) {
			mipi_dsi_host_unregister(&virt->host);
			DDPMSG("Waiting for bridge or panel driver\n");
			return -EPROBE_DEFER;
		}
		dev_info(dev, "%s found bridge %pK panel %pK\n", __func__, bridge, panel);
		if (!IS_ERR_OR_NULL(bridge))
			virt->bridge = bridge;
		if (!IS_ERR_OR_NULL(panel)) {
			virt->panel = panel;
			virt->panel->dev = dev;
			if (virt->type == MTK_VIRT_DSI)
				virt->ext = find_panel_ext(panel);
		}
	}
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
	ret = mtk_virt_init_device(dev, virt);

	if (ret)
		return ret;

	platform_set_drvdata(pdev, virt);
	ret = component_add(dev, &mtk_virt_component_ops);
	if (ret) {
		DDPMSG("Failed to add component: %d\n", ret);
		return ret;
	}
	DDPINFO("%s-\n", __func__);
	return 0;
}

static int mtk_virt_remove(struct platform_device *pdev)
{
	return 0;
}

struct platform_driver mtk_virt_driver = {
	.probe = mtk_virt_probe,
	.remove = mtk_virt_remove,
	.driver = {
		.name = "mediatek-virt",
		.of_match_table = mtk_virt_of_ids,
	},
};
