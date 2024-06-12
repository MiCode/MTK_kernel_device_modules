// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/backlight.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <drm/drm_bridge.h>
#include <linux/delay.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif
#include "bridge-serdes-max96789.h"

#define SER_NODE_NAME							"ser"

#define DEFAULT_WIDTH							1920
#define DEFAULT_HEIGHT							1080
#define DEFAULT_HFP								14
#define DEFAULT_HSA								20
#define DEFAULT_HBP								20
#define DEFAULT_VFP								230
#define DEFAULT_VSA								28
#define DEFAULT_VBP								2
#define DEFAULT_PLL								478
#define DEFAULT_FPS								60
#define DEFAULT_LPPF							1
#define DEFAULT_CLOCK							278554

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int bpc;

	/**
	 * @width_mm: width of the panel's active display area
	 * @height_mm: height of the panel's active display area
	 */
	struct {
		unsigned int width_mm;
		unsigned int height_mm;
	} size;

	unsigned long mode_flags;
	enum mipi_dsi_pixel_format format;
	const struct panel_init_cmd *init_cmds;
	unsigned int lanes;
	bool discharge_on_disable;
};

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct drm_bridge *bridge;

	struct mipi_dsi_device *dsi;
	const struct panel_desc *desc;

	struct drm_display_mode disp_mode;
	struct mtk_panel_params ext_params;
	u32 pll;
	u32 lppf;
};

static const struct panel_desc serdes_panel_desc = {
	.bpc = 8,
	.size = {
		.width_mm = 141,
		.height_mm = 226,
	},
	.lanes = 4,
	.format = MIPI_DSI_FMT_RGB888,
	.mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
				MIPI_DSI_MODE_LPM,
};

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

static void get_timing(struct lcm *ctx)
{
	struct vdo_timing timing;

	pr_debug("%s +\n", __func__);
	serdes_get_modes(ctx->bridge, &timing);
	pr_debug("%s: timing[%dx%d@%d]\n", __func__, timing.width, timing.height, timing.fps);
	pr_debug("%s: timing[%d %d %d][%d %d %d]\n", __func__, timing.hfp, timing.hsa, timing.hbp,
			timing.vfp, timing.vsa, timing.vbp);
	if (!timing.width || !timing.height || !timing.fps ||
		!timing.hfp || !timing.hsa || !timing.hbp ||
		!timing.vfp || !timing.vsa || !timing.vbp) {
		pr_debug("%s: get timing error! used default parameter!", __func__);
		timing.width = DEFAULT_WIDTH;
		timing.height = DEFAULT_HEIGHT;
		timing.hfp = DEFAULT_HFP;
		timing.hsa = DEFAULT_HSA;
		timing.hbp = DEFAULT_HBP;
		timing.vfp = DEFAULT_VFP;
		timing.vsa = DEFAULT_VSA;
		timing.vbp = DEFAULT_VBP;
		timing.pll = DEFAULT_PLL;
		timing.fps = DEFAULT_FPS;
		timing.lppf = DEFAULT_LPPF;
	}

	ctx->disp_mode.hdisplay = timing.width;
	ctx->disp_mode.vdisplay = timing.height;
	ctx->disp_mode.hsync_start = timing.width + timing.hfp;
	ctx->disp_mode.hsync_end = timing.width + timing.hfp + timing.hsa;
	ctx->disp_mode.htotal = timing.width + timing.hfp + timing.hsa + timing.hbp;
	ctx->disp_mode.vsync_start = timing.height + timing.vfp;
	ctx->disp_mode.vsync_end = timing.height + timing.vfp + timing.vsa;
	ctx->disp_mode.vtotal = timing.height + timing.vfp + timing.vsa + timing.vbp;

	ctx->disp_mode.clock =
		DIV_ROUND_UP_ULL(ctx->disp_mode.vtotal * ctx->disp_mode.htotal * timing.fps, 1000);
	ctx->pll = (timing.pll == 0) ? DIV_ROUND_UP_ULL(ctx->disp_mode.clock * 3, 1000) : timing.pll;
	ctx->lppf = timing.lppf;
	ctx->disp_mode.width_mm = timing.physcial_w;
	ctx->disp_mode.height_mm = timing.physcial_h;
	if (timing.crop_width[0] != 0 && timing.crop_width[1] != 0) {
		ctx->ext_params.crop_width[0] = timing.crop_width[0];
		ctx->ext_params.crop_width[1] = timing.crop_width[1];
		ctx->ext_params.crop_height[0] = timing.crop_height[0];
		ctx->ext_params.crop_height[1] = timing.crop_height[1];
		ctx->ext_params.physical_width = timing.crop_width[0] + timing.crop_width[1];
		ctx->ext_params.physical_height = timing.crop_height[0];
	} else {
		ctx->ext_params.physical_width = ctx->disp_mode.hdisplay;
		ctx->ext_params.physical_height = ctx->disp_mode.vdisplay;
		ctx->ext_params.crop_width[0] = 0;
		ctx->ext_params.crop_width[1] = 0;
		ctx->ext_params.crop_height[0] = 0;
		ctx->ext_params.crop_height[1] = 0;
	}

	pr_debug("%s -\n", __func__);
}

static void lcm_poweron(struct lcm *ctx)
{
	pr_debug("%s +\n", __func__);
	pr_debug("%s -\n", __func__);
}

static void lcm_poweroff(struct lcm *ctx)
{
	pr_debug("%s +\n", __func__);
	pr_debug("%s -\n", __func__);
}

static void lcm_panel_init(struct lcm *ctx)
{
	pr_debug("%s+\n", __func__);
	pr_debug("%s-\n", __func__);
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_debug("%s +\n", __func__);

	lcm_poweroff(ctx);

	pr_debug("%s -\n", __func__);
	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_debug("%s +\n", __func__);

	lcm_poweron(ctx);
	lcm_panel_init(ctx);
	serdes_pre_enable(ctx->bridge);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif
	pr_debug("%s -\n", __func__);

	return 0;
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_debug("%s +\n", __func__);

	serdes_disable(ctx->bridge);

	pr_debug("%s -\n", __func__);

	return 0;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_debug("%s +\n", __func__);

	serdes_enable(ctx->bridge);

	pr_debug("%s -\n", __func__);

	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
	pr_debug("%s +\n", __func__);
	pr_debug("%s -\n", __func__);
	return 1;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	pr_debug("%s +\n", __func__);
	pr_debug("%s -\n", __func__);
	return 0;
}

#if IS_ENABLED(CONFIG_ENABLE_SERDES_HOTPLUG)
static int panel_get_link_status(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_debug("%s +\n", __func__);
	return serdes_get_link_status(ctx->bridge);
	pr_debug("%s -\n", __func__);
}
#endif

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.ata_check = panel_ata_check,
#if IS_ENABLED(CONFIG_ENABLE_SERDES_HOTPLUG)
	.get_link_status = panel_get_link_status,
#endif
};

static int lcm_get_modes(struct drm_panel *panel,
	struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	struct lcm *ctx = panel_to_lcm(panel);

	pr_debug("%s:%s +\n", __func__,ctx->dev->of_node->name);
	pr_debug("mode:[%dx%d][%d/%d/%d][%d/%d/%d]\n",
		ctx->disp_mode.hdisplay,
		ctx->disp_mode.vdisplay,
		ctx->disp_mode.hsync_start,
		ctx->disp_mode.hsync_end,
		ctx->disp_mode.htotal,
		ctx->disp_mode.vsync_start,
		ctx->disp_mode.vsync_end,
		ctx->disp_mode.vtotal);
	mode = drm_mode_duplicate(connector->dev, &ctx->disp_mode);
	if (!mode) {
		pr_info("failed to add mode %ux%ux@%u\n",
			  ctx->disp_mode.hdisplay,
			  ctx->disp_mode.vdisplay,
			  drm_mode_vrefresh(&ctx->disp_mode));
		return -ENOMEM;
	}

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;

	drm_mode_set_name(mode);
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = ctx->disp_mode.width_mm;
	connector->display_info.height_mm = ctx->disp_mode.height_mm;

	pr_debug("%s -\n", __func__);

	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL, *ser_node;
	const phandle *ser_phandle;
	struct lcm *ctx;
	int ret;
	const struct panel_desc *desc;

	pr_debug("%s+, name:%s\n", __func__, dev->of_node->name);

	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_info("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_debug("device node name:%s\n", remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		pr_info("%s+ skip probe due to not current lcm\n", __func__);
		return -ENODEV;
	}

	ser_phandle = of_get_property(dev->of_node, SER_NODE_NAME, NULL);
	if (!ser_phandle) {
		pr_info("default dsi panel ser node is empty\n");
		return -1;
	}
	ser_node = of_find_node_by_phandle(be32_to_cpup(ser_phandle));

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->bridge = of_drm_find_bridge(ser_node);
	if (!ctx->bridge) {
		pr_info("%s: serdes not init, return!\n", __func__);
		return -EPROBE_DEFER;
	}

	ctx->dev = dev;

	desc = of_device_get_match_data(&dsi->dev);
	dsi->lanes = desc->lanes;
	dsi->format = desc->format;
	dsi->mode_flags = desc->mode_flags;
	ctx->desc = desc;
	ctx->dsi = dsi;

	get_timing(ctx);

	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);
	mipi_dsi_set_drvdata(dsi, ctx);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		pr_info("%s: attach fail\n", __func__);
		drm_panel_remove(&ctx->panel);
	}

#if defined(CONFIG_MTK_PANEL_EXT)
	ctx->ext_params.pll_clk = ctx->pll;
	ctx->ext_params.vdo_per_frame_lp_enable = ctx->lppf;
	pr_debug("pll_clk=%d, lppf=%d\n", ctx->ext_params.pll_clk, ctx->ext_params.vdo_per_frame_lp_enable);
	if (ctx->ext_params.crop_width[0] && ctx->ext_params.crop_width[1]) {
		pr_debug("super frame![%d*%d]+[%d*%d]=[%d*%d]\n", ctx->ext_params.crop_width[0],
			ctx->ext_params.crop_height[1],
			ctx->ext_params.crop_width[0],
			ctx->ext_params.crop_height[1],
			ctx->ext_params.physical_width,
			ctx->ext_params.physical_height);
	}
	ret = mtk_panel_ext_create(dev, &ctx->ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0) {
		pr_info("%s error!\n", __func__);
		return ret;
	}
#endif

	pr_debug("%s-\n", __func__);

	return ret;
}

static void lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
#endif

}

static const struct of_device_id lcm_of_match[] = {
	{
	    .compatible = "lcm,dsi,max96789",
		.data = &serdes_panel_desc
	},
	{}
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-maxiam-serdes",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Henry Tu <henry.tu@mediatek.com>");
MODULE_DESCRIPTION("Maxiam serdes Panel Driver");
MODULE_LICENSE("GPL");

