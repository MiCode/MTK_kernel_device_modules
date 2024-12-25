// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#ifndef _DSI_PANEL_MI_COUNT_H_
#define _DSI_PANEL_MI_COUNT_H_

#include <drm/drm_panel.h>
#include <drm/drm_connector.h>

enum PANEL_FPS {
	FPS_1,
	FPS_10,
	FPS_24,
	FPS_30,
	FPS_40,
	FPS_48,
	FPS_50,
	FPS_60,
	FPS_90,
	FPS_120,
	FPS_144,
	FPS_MAX_NUM,
};

enum PANEL_ID{
	PANEL_1ST = 1,
	PANEL_2SD,
	PANEL_3RD,
	PANEL_4TH,
	PANEL_MAX_NUM,
};

struct mi_dsi_panel_count {
	/* Display count */
	bool panel_active_count_enable;
	u64 boottime;
	u64 bootRTCtime;
	u64 bootdays;
	u64 panel_active;
	u64 kickoff_count;
	u64 bl_duration;
	u64 bl_level_integral;
	u64 bl_highlevel_duration;
	u64 bl_lowlevel_duration;
	u64 hbm_duration;
	u64 hbm_times;
	u64 fps_times[FPS_MAX_NUM];
	u64 panel_id;
	u64 poweron_cost_avg;
	u64 esd_times;
	u64 te_lost_times;
	u64 underrun_times;
	u64 overflow_times;
	u64 pingpong_timeout_times;
	u64 commit_long_times;
	u64 cmdq_timeout_times;
};

typedef enum panel_count_event {
	PANEL_ACTIVE,
	PANEL_BACKLIGHT,
	PANEL_HBM,
	PANEL_FPS,
	PANEL_POWERON_COST,
	PANEL_ESD,
	PANEL_TE_LOST,
	UNDERRUN,
	OVERFLOW,
	PINGPONG_TIMEOUT,
	COMMIT_LONG,
	CMDQ_TIMEOUT,
	PANEL_EVENT_MAX,
} PANEL_COUNT_EVENT;

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos;
	struct gpio_desc *dvdd_gpio;
	struct gpio_desc *cam_gpio;
	struct gpio_desc *leden_gpio;
	struct gpio_desc *vddio18_gpio;
	struct gpio_desc *vci30_gpio;

	bool prepared;
	bool enabled;
	bool hbm_en;
	bool wqhd_en;
	bool dc_status;
	bool hbm_enabled;
	bool lhbm_en;
	bool doze_suspend;

	int error;
	const char *panel_info;
	int dynamic_fps;
	u32 doze_brightness_state;
	u32 last_doze_brightness_state;
	u32 doze_state;

	struct pinctrl *pinctrl_gpios;
	struct pinctrl_state *err_flag_irq;
	struct drm_connector *connector;

	u32 max_brightness_clone;
	u32 factory_max_brightness;
	struct mutex panel_lock;
	struct mi_dsi_panel_count mi_count;
	int bl_max_level;
	int gir_status;
	int spr_status;
	int crc_level;
	int mode_index;
	unsigned int gate_ic;
	int panel_id;
	int gray_level;

	/* DDIC auto update gamma */
	u32 last_refresh_rate;
	bool need_auto_update_gamma;
	ktime_t last_mode_switch_time;
	int peak_hdr_status;
};

void mi_dsi_panel_count_enter(struct drm_panel *panel, PANEL_COUNT_EVENT event, int value, int enable);

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
