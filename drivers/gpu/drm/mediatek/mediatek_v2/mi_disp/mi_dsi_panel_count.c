// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */
#include "linux/notifier.h"
#include "mi_hwconf_manager.h"
#include "mi_dsi_panel_count.h"
#include <uapi/drm/mi_disp.h>
#include "mi_dsi_panel.h"
#include "mtk_dsi.h"

#define HWCONPONENT_NAME "display"
#define HWCONPONENT_KEY_LCD "LCD"
#define HWMON_CONPONENT_NAME "display"
#define HWMON_KEY_ACTIVE "panel_active"
#define HWMON_KEY_REFRESH "panel_refresh"
#define HWMON_KEY_BOOTTIME "kernel_boottime"
#define HWMON_KEY_DAYS "kernel_days"
#define HWMON_KEY_BL_AVG "bl_level_avg"
#define HWMON_KEY_BL_HIGH "bl_level_high"
#define HWMON_KEY_BL_LOW "bl_level_low"
#define HWMON_KEY_HBM_DRUATION "hbm_duration"
#define HWMON_KEY_HBM_TIMES "hbm_times"
#define HWMON_KEY_PANEL_ID "panel_id"
#define HWMON_KEY_POWERON_COST_AVG "poweron_cost_avg"
#define HWMON_KEY_ESD_TIMES "esd_times"
#define HWMON_KEY_TE_LOST_TIMES "te_lost_times"
#define HWMON_KEY_UNDERRUN_TIMES "underrun_times"
#define HWMON_KEY_OVERFLOW_TIMES "overflow_times"
#define HWMON_KEY_PINGPONG_TIMEOUT_TIMES "pingpong_timeout_times"
#define HWMON_KEY_COMMIT_LONG_TIMES "commit_long_times"
#define HWMON_KEY_CMDQ_TIMEOUT_TIMES "cmdq_timeout_times"

static char *hwmon_key_fps[FPS_MAX_NUM] = {"1fps_times", "10fps_times", "24fps_times", "30fps_times", "40fps_times", "48fps_times", "50fps_times",
	"60fps_times", "90fps_times", "120fps_times", "144fps_times"};
static const u32 dynamic_fps[FPS_MAX_NUM] = {1, 10, 24, 30, 40, 48, 50, 60, 90, 120, 144};
static char stored_system_build_version[100] = "null";
static char system_build_version[100] = "null";

#define DAY_SECS (60*60*24)
int disp_index = MI_DISP_PRIMARY;
static struct lcm * global_panel[MI_DISP_MAX];

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

void mi_dsi_panel_count_enter(struct drm_panel *panel, PANEL_COUNT_EVENT event, int value, int enable)
{
	struct lcm *lcm = panel_to_lcm(panel);
	if (!lcm) {
		pr_err("invalid input\n");
		return;
	}

	switch (event)
	{
	case PANEL_ACTIVE:
		mi_dsi_panel_state_count(lcm, enable);
		break;
	case PANEL_BACKLIGHT:
		mi_dsi_panel_backlight_count(lcm, value);
		break;
	case PANEL_HBM:
		mi_dsi_panel_HBM_count(lcm, value, enable);
		break;
	case PANEL_FPS:
		mi_dsi_panel_fps_count(lcm, value, enable);
		break;
	case PANEL_POWERON_COST:
		pr_info("%s PANEL_POWERON_COST\n", __func__);
		mi_dsi_panel_power_on_cost_count(lcm, value);
		break;
	case PANEL_ESD:
		mi_dsi_panel_esd_count(lcm, value);
		break;
	case PANEL_TE_LOST:
		mi_dsi_panel_te_lost_count(lcm, value);
		break;
	case UNDERRUN:
		mi_dsi_panel_underrun_count(lcm, value);
		break;
	case OVERFLOW:
		mi_dsi_panel_overflow_count(lcm, value);
		break;
	case PINGPONG_TIMEOUT:
		pr_info("%s PINGPONG_TIMEOUT not support\n", __func__);
		break;
	case COMMIT_LONG:
		mi_dsi_panel_commit_long_count(lcm, value);
		break;
	case CMDQ_TIMEOUT:
		mi_dsi_panel_cmdq_timeout_count(lcm, value);
		break;
	default:
		break;
	}
	return;
}
EXPORT_SYMBOL(mi_dsi_panel_count_enter);

void mi_dsi_panel_state_count(struct lcm *lcm, int enable)
{
	static u64 timestamp_panelon;
	static u64 on_times;
	static u64 off_times;
	char ch[64] = {0};

	if (enable) {
		/* get panel on timestamp */
		timestamp_panelon = get_jiffies_64();
		on_times++;
		lcm->mi_count.panel_active_count_enable = true;
	} else {
		ktime_t boot_time;
		u32 delta_days = 0;
		u64 jiffies_time = 0;
		struct timespec64 rtctime;

		off_times++;
		pr_info("%s: on_times[%llu] off_times[%llu]\n", __func__, on_times, off_times);

		/* caculate panel active duration */
		jiffies_time = get_jiffies_64();
		if (time_after64(jiffies_time, timestamp_panelon))
			lcm->mi_count.panel_active += jiffies_time - timestamp_panelon;
		snprintf(ch, sizeof(ch), "%llu", lcm->mi_count.panel_active / HZ);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_ACTIVE, ch);

		memset(ch, 0, sizeof(ch));
		snprintf(ch, sizeof(ch), "%llu", lcm->mi_count.kickoff_count / 60);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_REFRESH, ch);

		boot_time = ktime_get_boottime();
		do_div(boot_time, NSEC_PER_SEC);
		memset(ch, 0, sizeof(ch));
		snprintf(ch, sizeof(ch), "%llu", lcm->mi_count.boottime + boot_time);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BOOTTIME, ch);

		ktime_get_real_ts64(&rtctime);
		if (lcm->mi_count.bootRTCtime != 0) {
			if (rtctime.tv_sec > lcm->mi_count.bootRTCtime) {
				if (rtctime.tv_sec - lcm->mi_count.bootRTCtime > 10 * 365 * DAY_SECS) {
					lcm->mi_count.bootRTCtime = rtctime.tv_sec;
				} else {
					if (rtctime.tv_sec - lcm->mi_count.bootRTCtime > DAY_SECS) {
						delta_days = (rtctime.tv_sec - lcm->mi_count.bootRTCtime) / DAY_SECS;
						lcm->mi_count.bootdays += delta_days;
						lcm->mi_count.bootRTCtime = rtctime.tv_sec -
							((rtctime.tv_sec - lcm->mi_count.bootRTCtime) % DAY_SECS);
					}
				}
			} else {
				pr_err("RTC time rollback!\n");
				lcm->mi_count.bootRTCtime = rtctime.tv_sec;
			}
		} else {
			pr_info("panel_info.bootRTCtime init!\n");
			lcm->mi_count.bootRTCtime = rtctime.tv_sec;
		}
		memset(ch, 0, sizeof(ch));
		snprintf(ch, sizeof(ch), "%llu", lcm->mi_count.bootdays);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_DAYS, ch);

		lcm->mi_count.panel_active_count_enable = false;
	}

	return;
}

void mi_dsi_panel_HBM_count(struct lcm *lcm, int off, int enable)
{
	static u64 timestamp_hbmon;
	static int last_HBM_status;
	u64 jiffies_time = 0;
	char ch[64] = {0};
	bool record = false;

	pr_info("hbm_times[%lld],en[%d] off[%d]\n", lcm->mi_count.hbm_times, enable, off);

	if (off) {
		if (last_HBM_status == 1)
			record = true;
	} else {
		if (enable) {
			if (!last_HBM_status) {
				/* get HBM on timestamp */
				timestamp_hbmon = get_jiffies_64();
			}
		} else if (last_HBM_status){
			record = true;
		}
	}

	if (record) {
		/* caculate panel hbm duration */
		jiffies_time = get_jiffies_64();
		if (time_after64(jiffies_time, timestamp_hbmon))
			lcm->mi_count.hbm_duration += (jiffies_time - timestamp_hbmon) / HZ;
		snprintf(ch, sizeof(ch), "%llu", lcm->mi_count.hbm_duration);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_HBM_DRUATION, ch);

		/* caculate panel hbm times */
		memset(ch, 0, sizeof(ch));
		lcm->mi_count.hbm_times++;
		snprintf(ch, sizeof(ch), "%llu", lcm->mi_count.hbm_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_HBM_TIMES, ch);
	}

	last_HBM_status = enable;

	return;
}

void mi_dsi_panel_backlight_count(struct lcm *lcm, int bl_lvl)
{
	static u32 last_bl_level;
	static u64 last_bl_start;
	u64 bl_level_end = 0;
	char ch[64] = {0};

	bl_level_end = get_jiffies_64();
	lcm->bl_max_level = 2047;

	if (last_bl_start == 0) {
		last_bl_level = bl_lvl;
		last_bl_start = bl_level_end;
		return;
	}

	if (last_bl_level > 0) {
		lcm->mi_count.bl_level_integral += last_bl_level * (bl_level_end - last_bl_start);
		lcm->mi_count.bl_duration += (bl_level_end - last_bl_start);
	}

	/* backlight level 3071 ==> 450 nit */
	if (last_bl_level > (lcm->bl_max_level*3/4)) {
		lcm->mi_count.bl_highlevel_duration += (bl_level_end - last_bl_start);
	} else if (last_bl_level > 0) {
		/* backlight level (0, 3071] */
		lcm->mi_count.bl_lowlevel_duration += (bl_level_end - last_bl_start);
	}

	last_bl_level = bl_lvl;
	last_bl_start = bl_level_end;

	if (bl_lvl == 0) {
		memset(ch, 0, sizeof(ch));
		if (lcm->mi_count.bl_duration > 0) {
			snprintf(ch, sizeof(ch), "%llu",
				lcm->mi_count.bl_level_integral / lcm->mi_count.bl_duration);
			update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BL_AVG, ch);
		}

		memset(ch, 0, sizeof(ch));
		snprintf(ch, sizeof(ch), "%llu", lcm->mi_count.bl_highlevel_duration / HZ);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BL_HIGH, ch);

		memset(ch, 0, sizeof(ch));
		snprintf(ch, sizeof(ch), "%llu", lcm->mi_count.bl_lowlevel_duration / HZ);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BL_LOW, ch);
	}

	return;
}

void mi_dsi_panel_fps_count(struct lcm *lcm, int fps, int enable)
{
	static u32 timming_index = FPS_MAX_NUM;
	static u64 timestamp_fps;
	static u32 cur_fps = 60;
	u64 jiffies_time = 0;
	int i = 0;
	char ch[64] = {0};

	if (!lcm || (!enable && timming_index == FPS_MAX_NUM))
		return;

	if (enable && !fps)
		fps = cur_fps;

	for(i = 0; i < FPS_MAX_NUM; i++) {
		if (fps == dynamic_fps[i])
			break;
	}

	if (i < FPS_MAX_NUM || !fps) {
		if (i != timming_index && timming_index < FPS_MAX_NUM) {
			jiffies_time = get_jiffies_64();
			if (time_after64(jiffies_time, timestamp_fps))
				lcm->mi_count.fps_times[timming_index] += (jiffies_time - timestamp_fps) / HZ;
			snprintf(ch, sizeof(ch), "%llu", lcm->mi_count.fps_times[timming_index]);
			update_hw_monitor_info(HWMON_CONPONENT_NAME, hwmon_key_fps[timming_index], ch);
			timestamp_fps = jiffies_time;
		} else if (timming_index == FPS_MAX_NUM)
			timestamp_fps = get_jiffies_64();

		timming_index = i;
	}

	cur_fps = fps ? fps : cur_fps;
}

void mi_dsi_panel_te_lost_count(struct lcm *lcm, int value)
{
	char ch[64] = {0};

	if (lcm->mi_count.panel_active_count_enable) {
		lcm->mi_count.te_lost_times++;
		pr_info("%s %llu", __func__, lcm->mi_count.te_lost_times);

		memset(ch, 0, sizeof(ch));
		snprintf(ch, sizeof(ch), "%llu", lcm->mi_count.te_lost_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_TE_LOST_TIMES, ch);
	}

	return;
}

void mi_dsi_panel_commit_long_count(struct lcm *lcm, int value)
{
	char ch[64] = {0};

	lcm->mi_count.commit_long_times++;
	pr_info("%s %llu", __func__, lcm->mi_count.commit_long_times);

	memset(ch, 0, sizeof(ch));
	snprintf(ch, sizeof(ch), "%llu", lcm->mi_count.commit_long_times);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_COMMIT_LONG_TIMES, ch);

	return;
}

void mi_dsi_panel_cmdq_timeout_count(struct lcm *lcm, int value)
{
	char ch[64] = {0};

	lcm->mi_count.cmdq_timeout_times++;
	pr_info("%s %llu", __func__, lcm->mi_count.cmdq_timeout_times);

	memset(ch, 0, sizeof(ch));
	snprintf(ch, sizeof(ch), "%llu", lcm->mi_count.cmdq_timeout_times);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_CMDQ_TIMEOUT_TIMES, ch);

	return;
}

void mi_dsi_panel_set_build_version(struct drm_panel *panel, char * build_verison, u32 size)
{
	if(!build_verison) {
		build_verison = "inValid";
		size = sizeof(build_verison);
	}
	memcpy(system_build_version, build_verison, size);
	pr_info("%s: last - %s to  new - %s\n",__func__,  stored_system_build_version, system_build_version);

	if (strcmp(stored_system_build_version, "null") && strcmp(stored_system_build_version, system_build_version)) {
		mi_dsi_panel_clean_data();
	}
}

void mi_dsi_panel_clean_data(void)
{
	int i = MI_DISP_PRIMARY;
	char count_str[64] = {0};

	for(i = MI_DISP_PRIMARY; i < MI_DISP_MAX; i ++) {
		if (global_panel[i] == NULL)
			continue;
		memset(count_str, 0, sizeof(count_str));
		global_panel[i]->mi_count.esd_times = 0;
		snprintf(count_str, sizeof(count_str), "%llu", global_panel[i]->mi_count.esd_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_ESD_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		global_panel[i]->mi_count.te_lost_times = 0;
		snprintf(count_str, sizeof(count_str), "%llu", global_panel[i]->mi_count.te_lost_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_TE_LOST_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		global_panel[i]->mi_count.underrun_times = 0;
		snprintf(count_str, sizeof(count_str), "%llu", global_panel[i]->mi_count.underrun_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_UNDERRUN_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		global_panel[i]->mi_count.overflow_times = 0;
		snprintf(count_str, sizeof(count_str), "%llu", global_panel[i]->mi_count.overflow_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_OVERFLOW_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		global_panel[i]->mi_count.pingpong_timeout_times = 0;
		snprintf(count_str, sizeof(count_str), "%llu", global_panel[i]->mi_count.pingpong_timeout_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_PINGPONG_TIMEOUT_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		global_panel[i]->mi_count.commit_long_times = 0;
		snprintf(count_str, sizeof(count_str), "%llu", global_panel[i]->mi_count.commit_long_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_COMMIT_LONG_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		global_panel[i]->mi_count.cmdq_timeout_times = 0;
		snprintf(count_str, sizeof(count_str), "%llu", global_panel[i]->mi_count.cmdq_timeout_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_CMDQ_TIMEOUT_TIMES, count_str);
	}
}

void mi_dsi_panel_power_on_cost_count(struct lcm *lcm, int is_start)
{
	char ch[64] = {0};
	u64 jiffies_time = 0;
	u64 poweron_temp_time = 0;
	static u64 timestamp_power_start[MI_DISP_MAX];
	static int poweron_count_times[MI_DISP_MAX] = {[0 ... MI_DISP_MAX-1]=0};
	static int temp_poweron_cost_avg_times[MI_DISP_MAX] = {[0 ... MI_DISP_MAX-1]=0};
	static bool power_flag_get[MI_DISP_MAX] = {[0 ... MI_DISP_MAX-1]=false};
	static bool first_into_flag[MI_DISP_MAX] = {[0 ... MI_DISP_MAX-1]=true};
	pr_info("%s \n", __func__);

	if (first_into_flag[disp_index]) {
		if (poweron_count_times[disp_index] == 1) {
			first_into_flag[disp_index] = false;
			poweron_count_times[disp_index] = 0;
			return;
		}
		++poweron_count_times[disp_index];
		return;
	}

	if (is_start) {
		power_flag_get[disp_index] = true;
		timestamp_power_start[disp_index] = get_jiffies_64();
		pr_info("poweron_temp_time start\n");
	} else if (power_flag_get[disp_index]) {
		power_flag_get[disp_index] = false;
		jiffies_time = get_jiffies_64();
		if (time_after64(jiffies_time, timestamp_power_start[disp_index]))
			poweron_temp_time = jiffies_to_msecs(jiffies_time - timestamp_power_start[disp_index]);
		pr_info("poweron_temp_time = %lld, count: %d, disp_Index: %d\n", poweron_temp_time, poweron_count_times[disp_index], disp_index);
		if (poweron_count_times[disp_index] < 100) {
			++poweron_count_times[disp_index];
			temp_poweron_cost_avg_times[disp_index] =
				(temp_poweron_cost_avg_times[disp_index] *  (poweron_count_times[disp_index] - 1) + poweron_temp_time *10)
				 / poweron_count_times[disp_index];
			lcm->mi_count.poweron_cost_avg = temp_poweron_cost_avg_times[disp_index] /10;
		} else {
			poweron_count_times[disp_index] = 1;
			temp_poweron_cost_avg_times[disp_index] = poweron_temp_time * 10;
			lcm->mi_count.poweron_cost_avg = poweron_temp_time;
		}
		memset(ch, 0, sizeof(ch));
		snprintf(ch, sizeof(ch), "%llu", lcm->mi_count.poweron_cost_avg);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_POWERON_COST_AVG, ch);
	}

	return;
}

void mi_dsi_panel_esd_count(struct lcm *lcm, int is_irq)
{
	char ch[64] = {0};

	lcm->mi_count.esd_times++;

	memset(ch, 0, sizeof(ch));
	snprintf(ch, sizeof(ch), "%llu", lcm->mi_count.esd_times);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_ESD_TIMES, ch);

	return;
}

void mi_dsi_panel_underrun_count(struct lcm *lcm, int value)
{
	char ch[64] = {0};

	lcm->mi_count.underrun_times++;

	memset(ch, 0, sizeof(ch));
	snprintf(ch, sizeof(ch), "%llu", lcm->mi_count.underrun_times);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_UNDERRUN_TIMES, ch);

	return;
}

void mi_dsi_panel_overflow_count(struct lcm *lcm, int value)
{
	char ch[64] = {0};

	lcm->mi_count.overflow_times++;

	memset(ch, 0, sizeof(ch));
	snprintf(ch, sizeof(ch), "%llu", lcm->mi_count.overflow_times);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_OVERFLOW_TIMES, ch);

	return;
}

void mi_dsi_panel_count_init(struct drm_panel *panel)
{
	int i = 0;
	char count_str[64] = {0};
	struct lcm *lcm = panel_to_lcm(panel);
	if (!lcm) {
		pr_err("invalid input\n");
		return;
	}

        global_panel[disp_index] = lcm;
	lcm->mi_count.panel_active_count_enable = false;
	lcm->mi_count.panel_active = 0;
	lcm->mi_count.kickoff_count = 0;
	lcm->mi_count.bl_duration = 0;
	lcm->mi_count.bl_level_integral = 0;
	lcm->mi_count.bl_highlevel_duration = 0;
	lcm->mi_count.bl_lowlevel_duration = 0;
	lcm->mi_count.hbm_duration = 0;
	lcm->mi_count.hbm_times = 0;
	memset((void *)lcm->mi_count.fps_times, 0, sizeof(lcm->mi_count.fps_times));

	if (lcm->panel_id == PANEL_1ST) {
		lcm->mi_count.panel_id = 1;
	} else if (lcm->panel_id == PANEL_2SD)
		lcm->mi_count.panel_id = 2;
	else if (lcm->panel_id == PANEL_3RD)
		lcm->mi_count.panel_id = 3;
	else if (lcm->panel_id == PANEL_4TH)
		lcm->mi_count.panel_id = 4;
	else {
		pr_info("panel id: %d, please check!\n", lcm->panel_id);
		lcm->mi_count.panel_id = 1;
	}

	register_hw_monitor_info(HWMON_CONPONENT_NAME);
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_ACTIVE, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_REFRESH, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BOOTTIME, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_DAYS, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BL_AVG, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BL_HIGH, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BL_LOW, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_HBM_DRUATION, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_HBM_TIMES, "0");

	for (i = 0; i < FPS_MAX_NUM; i++)
		add_hw_monitor_info(HWMON_CONPONENT_NAME, hwmon_key_fps[i], "0");

	memset(count_str, 0, sizeof(count_str));
	snprintf(count_str, sizeof(count_str), "%llu", lcm->mi_count.panel_id);
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_PANEL_ID, count_str);
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_POWERON_COST_AVG, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_ESD_TIMES, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_TE_LOST_TIMES, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_UNDERRUN_TIMES, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_OVERFLOW_TIMES, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_PINGPONG_TIMEOUT_TIMES, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_COMMIT_LONG_TIMES, "0");
	add_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_CMDQ_TIMEOUT_TIMES, "0");

	mi_dsi_panel_state_count(lcm, 1);
	mi_dsi_panel_fps_count(lcm, 0, 1);

	return;
}
EXPORT_SYMBOL(mi_dsi_panel_count_init);

int mi_dsi_panel_disp_count_set(struct lcm *lcm, const char *buf)
{
	char count_str[64] = {0};
	int i = 0;
	u64 panel_active = 0;
	u64 kickoff_count = 0;
	u64 kernel_boottime = 0;
	u64 kernel_rtctime = 0;
	u64 kernel_days = 0;
	u64 record_end = 0;
	u32 delta_days = 0;
	u64 bl_duration = 0;
	u64 bl_level_integral = 0;
	u64 bl_highlevel_duration = 0;
	u64 bl_lowlevel_duration = 0;
	u64 hbm_duration = 0;
	u64 hbm_times = 0;
	u64 fps_times[FPS_MAX_NUM] = {0};
	u64 panel_id = 1;
	u64 poweron_cost_avg = 0;
	u64 esd_times = 0;
	u64 te_lost_times = 0;
	u64 underrun_times = 0;
	u64 overflow_times = 0;
	u64 pingpong_timeout_times = 0;
	u64 commit_long_times = 0;
	u64 cmdq_timeout_times = 0;

	ssize_t result;
	struct timespec64 rtctime;

	pr_info("[LCD] %s: begin\n", __func__);

	if (!lcm) {
		pr_err("invalid panel\n");
		return -EINVAL;
	}

	result = sscanf(buf,
		"panel_active=%llu\n"
		"panel_kickoff_count=%llu\n"
		"kernel_boottime=%llu\n"
		"kernel_rtctime=%llu\n"
		"kernel_days=%llu\n"
		"bl_duration=%llu\n"
		"bl_level_integral=%llu\n"
		"bl_highlevel_duration=%llu\n"
		"bl_lowlevel_duration=%llu\n"
		"hbm_duration=%llu\n"
		"hbm_times=%llu\n"
		"fps1_times=%llu\n"
		"fps10_times=%llu\n"
		"fps24_times=%llu\n"
		"fps30_times=%llu\n"
		"fps40_times=%llu\n"
		"fps48_times=%llu\n"
		"fps50_times=%llu\n"
		"fps60_times=%llu\n"
		"fps90_times=%llu\n"
		"fps120_times=%llu\n"
		"fps144_times=%llu\n"
		"panel_id=%llu\n"
		"poweron_cost_avg=%llu\n"
		"esd_times=%llu\n"
		"te_lost_times=%llu\n"
		"underrun_times=%llu\n"
		"overflow_times=%llu\n"
		"pingpong_timeout_times=%llu\n"
		"commit_long_times=%llu\n"
		"cmdq_timeout_times=%llu\n"
		"system_build_version=%s\n"
		"record_end=%llu\n",
		&panel_active,
		&kickoff_count,
		&kernel_boottime,
		&kernel_rtctime,
		&kernel_days,
		&bl_duration,
		&bl_level_integral,
		&bl_highlevel_duration,
		&bl_lowlevel_duration,
		&hbm_duration,
		&hbm_times,
		&fps_times[FPS_1],
		&fps_times[FPS_10],
		&fps_times[FPS_24],
		&fps_times[FPS_30],
		&fps_times[FPS_40],
		&fps_times[FPS_48],
		&fps_times[FPS_50],
		&fps_times[FPS_60],
		&fps_times[FPS_90],
		&fps_times[FPS_120],
		&fps_times[FPS_144],
		&panel_id,
		&poweron_cost_avg,
		&esd_times,
		&te_lost_times,
		&underrun_times,
		&overflow_times,
		&pingpong_timeout_times,
		&commit_long_times,
		&cmdq_timeout_times,
		&stored_system_build_version,
		&record_end);

	if (result != 33) {
		pr_err("sscanf buf error!\n");
		return -EINVAL;
	}
#if 0
	if (panel_active < panel.panel_active) {
		pr_err("Current panel_active < panel_info.panel_active!\n");
		return -EINVAL;
	}

	if (kickoff_count < panel.kickoff_count) {
		pr_err("Current kickoff_count < panel_info.kickoff_count!\n");
		return -EINVAL;
	}
#endif

	ktime_get_real_ts64(&rtctime);
	if (rtctime.tv_sec > kernel_rtctime) {
		if (rtctime.tv_sec - kernel_rtctime > 10 * 365 * DAY_SECS) {
			lcm->mi_count.bootRTCtime = rtctime.tv_sec;
		} else {
			if (rtctime.tv_sec - kernel_rtctime > DAY_SECS) {
				delta_days = (rtctime.tv_sec - kernel_rtctime) / DAY_SECS;
				lcm->mi_count.bootRTCtime = rtctime.tv_sec - ((rtctime.tv_sec - kernel_rtctime) % DAY_SECS);
			} else {
				lcm->mi_count.bootRTCtime = kernel_rtctime;
			}
		}
	} else {
		pr_err("RTC time rollback!\n");
		lcm->mi_count.bootRTCtime = kernel_rtctime;
	}

	lcm->mi_count.panel_active = panel_active;
	snprintf(count_str, sizeof(count_str), "%llu", lcm->mi_count.panel_active/HZ);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_ACTIVE, count_str);

	memset(count_str, 0, sizeof(count_str));
	lcm->mi_count.kickoff_count = kickoff_count;
	snprintf(count_str, sizeof(count_str), "%llu", lcm->mi_count.kickoff_count/60);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_REFRESH, count_str);

	memset(count_str, 0, sizeof(count_str));
	lcm->mi_count.boottime = kernel_boottime;
	snprintf(count_str, sizeof(count_str), "%llu", lcm->mi_count.boottime);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BOOTTIME, count_str);

	memset(count_str, 0, sizeof(count_str));
	lcm->mi_count.bootdays = kernel_days + delta_days;
	snprintf(count_str, sizeof(count_str), "%llu", lcm->mi_count.bootdays);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_DAYS, count_str);

	memset(count_str, 0, sizeof(count_str));
	lcm->mi_count.bl_level_integral = bl_level_integral;
	lcm->mi_count.bl_duration = bl_duration;
	if (lcm->mi_count.bl_duration > 0) {
		snprintf(count_str, sizeof(count_str), "%llu",
			lcm->mi_count.bl_level_integral / lcm->mi_count.bl_duration);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BL_AVG, count_str);
	}

	memset(count_str, 0, sizeof(count_str));
	lcm->mi_count.bl_highlevel_duration = bl_highlevel_duration;
	snprintf(count_str, sizeof(count_str), "%llu", lcm->mi_count.bl_highlevel_duration);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BL_HIGH, count_str);

	memset(count_str, 0, sizeof(count_str));
	lcm->mi_count.bl_lowlevel_duration = bl_lowlevel_duration;
	snprintf(count_str, sizeof(count_str), "%llu", lcm->mi_count.bl_lowlevel_duration);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_BL_LOW, count_str);

	memset(count_str, 0, sizeof(count_str));
	lcm->mi_count.hbm_duration = hbm_duration;
	snprintf(count_str, sizeof(count_str), "%llu", lcm->mi_count.hbm_duration);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_HBM_DRUATION, count_str);

	memset(count_str, 0, sizeof(count_str));
	lcm->mi_count.hbm_times = hbm_times;
	snprintf(count_str, sizeof(count_str), "%llu", lcm->mi_count.hbm_times);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_HBM_TIMES, count_str);

	for (i = 0; i < FPS_MAX_NUM; i++) {
		memset(count_str, 0, sizeof(count_str));
		lcm->mi_count.fps_times[i] = fps_times[i];
		snprintf(count_str, sizeof(count_str), "%llu", lcm->mi_count.fps_times[i]);
		add_hw_monitor_info(HWMON_CONPONENT_NAME, hwmon_key_fps[i], count_str);
	}

	memset(count_str, 0, sizeof(count_str));
	lcm->mi_count.poweron_cost_avg = poweron_cost_avg;
	snprintf(count_str, sizeof(count_str), "%llu", lcm->mi_count.poweron_cost_avg);
	update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_POWERON_COST_AVG, count_str);

	if (strcmp(stored_system_build_version, "null") && strcmp(system_build_version, "null")
			&& strcmp(stored_system_build_version, system_build_version)) {
		memset(count_str, 0, sizeof(count_str));
		lcm->mi_count.esd_times = 0;
		snprintf(count_str, sizeof(count_str), "%llu", lcm->mi_count.esd_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_ESD_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		lcm->mi_count.te_lost_times = 0;
		snprintf(count_str, sizeof(count_str), "%llu", lcm->mi_count.te_lost_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_TE_LOST_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		lcm->mi_count.underrun_times = 0;
		snprintf(count_str, sizeof(count_str), "%llu", lcm->mi_count.underrun_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_UNDERRUN_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		lcm->mi_count.overflow_times = 0;
		snprintf(count_str, sizeof(count_str), "%llu", lcm->mi_count.overflow_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_OVERFLOW_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		lcm->mi_count.pingpong_timeout_times = 0;
		snprintf(count_str, sizeof(count_str), "%llu", lcm->mi_count.pingpong_timeout_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_PINGPONG_TIMEOUT_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		lcm->mi_count.commit_long_times = 0;
		snprintf(count_str, sizeof(count_str), "%llu", lcm->mi_count.commit_long_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_COMMIT_LONG_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		lcm->mi_count.cmdq_timeout_times = 0;
		snprintf(count_str, sizeof(count_str), "%llu", lcm->mi_count.cmdq_timeout_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_CMDQ_TIMEOUT_TIMES, count_str);
	}else{
		memset(count_str, 0, sizeof(count_str));
		lcm->mi_count.esd_times = esd_times;
		snprintf(count_str, sizeof(count_str), "%llu", lcm->mi_count.esd_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_ESD_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		lcm->mi_count.te_lost_times = te_lost_times;
		snprintf(count_str, sizeof(count_str), "%llu", lcm->mi_count.te_lost_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_TE_LOST_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		lcm->mi_count.underrun_times = underrun_times;
		snprintf(count_str, sizeof(count_str), "%llu", lcm->mi_count.underrun_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_UNDERRUN_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		lcm->mi_count.overflow_times = overflow_times;
		snprintf(count_str, sizeof(count_str), "%llu", lcm->mi_count.overflow_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_OVERFLOW_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		lcm->mi_count.pingpong_timeout_times = pingpong_timeout_times;
		snprintf(count_str, sizeof(count_str), "%llu", lcm->mi_count.pingpong_timeout_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_PINGPONG_TIMEOUT_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		lcm->mi_count.commit_long_times = commit_long_times;
		snprintf(count_str, sizeof(count_str), "%llu", lcm->mi_count.commit_long_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_COMMIT_LONG_TIMES, count_str);

		memset(count_str, 0, sizeof(count_str));
		lcm->mi_count.cmdq_timeout_times = cmdq_timeout_times;
		snprintf(count_str, sizeof(count_str), "%llu", lcm->mi_count.cmdq_timeout_times);
		update_hw_monitor_info(HWMON_CONPONENT_NAME, HWMON_KEY_CMDQ_TIMEOUT_TIMES, count_str);
	}
	pr_info("[LCD] %s: end\n", __func__);

	return 0;
}

ssize_t mi_dsi_panel_disp_count_get(struct lcm *lcm, char *buf)
{
	int ret = -1;
	ktime_t boot_time;
	u64 record_end = 0;
	/* struct timespec rtctime; */

	if (!lcm) {
		pr_err("invalid panel\n");
		return -EINVAL;
	}

	if (buf == NULL) {
		pr_err("dsi_panel_disp_count_get buffer is NULL!\n");
		return -EINVAL;
	}

	boot_time = ktime_get_boottime();
	do_div(boot_time, NSEC_PER_SEC);
	/* getnstimeofday(&rtctime); */

	ret = scnprintf(buf, PAGE_SIZE,
		"panel_active=%llu\n"
		"panel_kickoff_count=%llu\n"
		"kernel_boottime=%llu\n"
		"kernel_rtctime=%llu\n"
		"kernel_days=%llu\n"
		"bl_duration=%llu\n"
		"bl_level_integral=%llu\n"
		"bl_highlevel_duration=%llu\n"
		"bl_lowlevel_duration=%llu\n"
		"hbm_duration=%llu\n"
		"hbm_times=%llu\n"
		"fps1_times=%llu\n"
		"fps10_times=%llu\n"
		"fps24_times=%llu\n"
		"fps30_times=%llu\n"
		"fps40_times=%llu\n"
		"fps48_times=%llu\n"
		"fps50_times=%llu\n"
		"fps60_times=%llu\n"
		"fps90_times=%llu\n"
		"fps120_times=%llu\n"
		"fps144_times=%llu\n"
		"panel_id=%llu\n"
		"poweron_cost_avg=%llu\n"
		"esd_times=%llu\n"
		"te_lost_times=%llu\n"
		"underrun_times=%llu\n"
		"overflow_times=%llu\n"
		"pingpong_timeout_times=%llu\n"
		"commit_long_times=%llu\n"
		"cmdq_timeout_times=%llu\n"
		"system_build_version=%s\n"
		"record_end=%llu\n",
		lcm->mi_count.panel_active / HZ,
		lcm->mi_count.kickoff_count,
		lcm->mi_count.boottime + boot_time,
		lcm->mi_count.bootRTCtime,
		lcm->mi_count.bootdays,
		lcm->mi_count.bl_duration / HZ,
		lcm->mi_count.bl_level_integral / HZ,
		lcm->mi_count.bl_highlevel_duration / HZ,
		lcm->mi_count.bl_lowlevel_duration / HZ,
		lcm->mi_count.hbm_duration,
		lcm->mi_count.hbm_times,
		lcm->mi_count.fps_times[FPS_1],
		lcm->mi_count.fps_times[FPS_10],
		lcm->mi_count.fps_times[FPS_24],
		lcm->mi_count.fps_times[FPS_30],
		lcm->mi_count.fps_times[FPS_40],
		lcm->mi_count.fps_times[FPS_48],
		lcm->mi_count.fps_times[FPS_50],
		lcm->mi_count.fps_times[FPS_60],
		lcm->mi_count.fps_times[FPS_90],
		lcm->mi_count.fps_times[FPS_120],
		lcm->mi_count.fps_times[FPS_144],
		lcm->mi_count.panel_id,
		lcm->mi_count.poweron_cost_avg,
		lcm->mi_count.esd_times,
		lcm->mi_count.te_lost_times,
		lcm->mi_count.underrun_times,
		lcm->mi_count.overflow_times,
		lcm->mi_count.pingpong_timeout_times,
		lcm->mi_count.commit_long_times,
		lcm->mi_count.cmdq_timeout_times,
		system_build_version,
		record_end);

	return ret;
}
