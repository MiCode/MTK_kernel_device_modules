// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * pwm-fan.c - Hwmon driver for fans connected to PWM lines.
 *
 * Copyright (c) 2024 Xiaomi Co., Ltd.
 *
 * Author: linjiashuo <linjiashuo@xiaomi.com>
 */

#ifndef __XM_PWM_FAN_H
#define __XM_PWM_FAN_H

#define MAX_FAN_NAME_LEN		20
#define FAN_UNSUPPORT_ID_COUNT		10
#define FAN_SPEED_CFG_COUNT		1
#define FAN_PWM_DUTY_MIN		0
#define FAN_PWM_DUTY_MAX		100
#define FAN_SPEED_DETECT_NOT_READY	2
#define FAN_LDO_ERROR_REPORT_CNT_MAX	3

enum pwm_fan_speed_level {
	FAN_SPEED_LEVEL_OFF = 0,
	FAN_SPEED_LEVEL_LOW,
	FAN_SPEED_LEVEL_MED,
	FAN_SPEED_LEVEL_HIGH,
	FAN_SPEED_LEVEL_TURBO,
	FAN_SPEED_LEVEL_MAX,
	FAN_SPEED_LEVEL_DBG_OFF = 100,
	FAN_SPEED_LEVEL_DBG_LOW,
	FAN_SPEED_LEVEL_DBG_MED,
	FAN_SPEED_LEVEL_DBG_HIGH,
	FAN_SPEED_LEVEL_DBG_TURBO,
	FAN_SPEED_LEVEL_DBG_MAX,
};

struct pwm_fan_duty_speed_map {
	unsigned int duty;
	unsigned int speed;
};

struct pwm_fan_speed_adjust_cfg {
	unsigned int min_speed;
	unsigned int max_speed;
	unsigned int speed_gap_thres;
	unsigned int adjust_cnt_max;
};

struct pwm_fan_ctx {
	char name[MAX_FAN_NAME_LEN];
	struct device *dev;
	struct mutex lock;
	struct regulator *ldo;
	struct notifier_block ldo_nb;
	bool fan_support;
	bool enabled;
	bool use_extern_ldo;
	int gpio_pwr_en;
	int gpio_shifter_en;
	unsigned int pwm_duty;
	unsigned int pwm_ch;
	unsigned int target_level;
	unsigned int target_level_dbg;
	unsigned int target_speed;
	unsigned int real_speed;
	unsigned int speed_adjust_cnt;
	unsigned int err_stat_report_cnt;
	unsigned int unsupport_id[FAN_UNSUPPORT_ID_COUNT];
	struct pwm_fan_speed_adjust_cfg speed_adjust_cfg[FAN_SPEED_CFG_COUNT];
	struct pwm_fan_duty_speed_map duty_speed_map[FAN_SPEED_LEVEL_MAX];
	struct delayed_work speed_dynamic_adjust_work;
	struct delayed_work speed_detect_req_work;
	struct delayed_work ldo_status_detect_work;
};

#endif /* __XM_PWM_FAN_H */
