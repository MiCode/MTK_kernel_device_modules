// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#ifndef _DSI_PANEL_MI_COUNT_H_
#define _DSI_PANEL_MI_COUNT_H_

#include <drm/drm_panel.h>
#include <drm/drm_connector.h>

struct lcm;

enum PANEL_ID{
	PANEL_1ST = 1,
	PANEL_2SD,
	PANEL_3RD,
	PANEL_4TH,
	PANEL_MAX_NUM,
};

void mi_dsi_panel_state_count(struct lcm *lcm, int enable);
void mi_dsi_panel_HBM_count(struct lcm *lcm, int off, int enable);
void mi_dsi_panel_backlight_count(struct lcm *lcm, int bl_lvl);
void mi_dsi_panel_fps_count(struct lcm *lcm, int fps, int enable);
void mi_dsi_panel_esd_count(struct lcm *lcm, int is_irq);
void mi_dsi_panel_set_build_version(struct drm_panel *panel, char * build_verison, u32 size);
void mi_dsi_panel_clean_data(void);
void mi_dsi_panel_power_on_cost_count(struct lcm *lcm, int is_start);
void mi_dsi_panel_te_lost_count(struct lcm *lcm, int value);
void mi_dsi_panel_underrun_count(struct lcm *lcm, int value);
void mi_dsi_panel_overflow_count(struct lcm *lcm, int value);
void mi_dsi_panel_pingpong_timeout_count(struct lcm *lcm, int value);
void mi_dsi_panel_commit_long_count(struct lcm *lcm, int value);
void mi_dsi_panel_cmdq_timeout_count(struct lcm *lcm, int value);
void mi_dsi_panel_count_init(struct drm_panel *panel);

int mi_dsi_panel_disp_count_set(struct lcm *lcm, const char *buf);
ssize_t mi_dsi_panel_disp_count_get(struct lcm *lcm, char *buf);
#endif /* _DSI_PANEL_MI_COUNT_H_ */
