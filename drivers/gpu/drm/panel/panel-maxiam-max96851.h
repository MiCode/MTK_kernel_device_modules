/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _PANEL_MAXIAM_MAX96851_H_
#define _PANEL_MAXIAM_MAX96851_H_

#define PANLE_NAME_SIZE		64
#define USE_DEFAULT_SETTING	"use-default-setting"
#define PANEL_NAME			"panel-name"
#define PANEL_MODE			"panel-mode"
#define DEBUG_DP_INFO		"[DPTX-panel]"
#define DEBUG_DP_MST1_INFO	"[DPTX-MST1-panel]"
#define DEBUG_EDP_INFO		"[eDPTX-panel]"

struct panel_edp {
	struct drm_panel panel;
	struct device *dev;
	char panel_name[PANLE_NAME_SIZE];
	char panel_mode[PANLE_NAME_SIZE];

	const char *label;
	unsigned int width;
	unsigned int height;
	struct videomode video_mode;
	struct backlight_device *backlight;
	struct regulator *supply;

	struct gpio_desc *power3v3_gpio;
	struct gpio_desc *enable_gpio;
	struct gpio_desc *reset_gpio;

	bool is_dp;
	bool use_default;
	char debug_str[32];
};

#endif
