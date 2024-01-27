/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _MTK_DRM_PANEL_DRV_H_
#define _MTK_DRM_PANEL_DRV_H_

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <video/of_videomode.h>
#include <video/videomode.h>
#include <linux/backlight.h>
#include <linux/spinlock.h>
#include "mtk_drm_panel_helper.h"

struct mtk_panel_context {
	struct device *dev;
	spinlock_t lock;

	/* drm panel interfaces */
	struct drm_panel panel;

	/* backlight device */
	struct backlight_device *backlight;

	/* gate ic operations */
	struct mtk_gateic_funcs gateic_ops;

	/* panel params and operations parsed from dtsi */
	struct mtk_panel_resource *panel_resource;

	/* panel params and operations parsed from dtsi */
	struct mtk_lcm_mode_dsi *current_mode;

	/* panel status of prepared */
	atomic_t prepared;

	/* panel status of enabled */
	atomic_t enabled;

	/* panel status of ddic error */
	atomic_t error;

	/* panel status of hight backlight mode */
	atomic_t hbm_en;

	/* panel required delay of hight backlight mode*/
	atomic_t hbm_wait;

	/* current fps mode */
	atomic_t current_backlight;

	/* fake mode */
	atomic_t fake_mode;

	/* doze status */
	atomic_t doze_enabled;
};

extern struct platform_driver mtk_drm_panel_dbi_driver;
extern struct platform_driver mtk_drm_panel_dpi_driver;
extern struct mipi_dsi_driver mtk_drm_panel_dsi_driver;

/* register customization callback of panel operation
 * func: MTK_LCM_FUNC_DBI/DPI/DSI
 * cust: customized function callback
 * return: 0 for success, else for failed
 */
int mtk_panel_register_drv_customization_funcs(char func,
		const struct mtk_panel_cust *cust);
int mtk_panel_register_dbi_customization_funcs(
		const struct mtk_panel_cust *cust);
int mtk_panel_register_dpi_customization_funcs(
		const struct mtk_panel_cust *cust);
int mtk_panel_register_dsi_customization_funcs(
		const struct mtk_panel_cust *cust);


/* deregister customization callback of panel operation
 * func: MTK_LCM_FUNC_DBI/DPI/DSI
 * cust: customized function callback
 * return: 0 for success, else for failed
 */
int mtk_panel_deregister_drv_customization_funcs(char func,
		const struct mtk_panel_cust *cust);
int mtk_panel_deregister_dbi_customization_funcs(
		const struct mtk_panel_cust *cust);
int mtk_panel_deregister_dpi_customization_funcs(
		const struct mtk_panel_cust *cust);
int mtk_panel_deregister_dsi_customization_funcs(
		const struct mtk_panel_cust *cust);

int mtk_lcm_dsi_power_on(void);
int mtk_lcm_dsi_power_off(void);
int mtk_lcm_dsi_power_reset(int value);

struct mtk_panel_context *panel_to_lcm(
		struct drm_panel *panel);

#endif
