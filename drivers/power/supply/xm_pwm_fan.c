// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * pwm-fan.c - Hwmon driver for fans connected to PWM lines.
 *
 * Copyright (c) 2024 Xiaomi Co., Ltd.
 *
 * Author: linjiashuo <linjiashuo@xiaomi.com>
 */

#include <linux/hwmon.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/regulator/consumer.h>
#include <linux/sysfs.h>
#include <linux/thermal.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <mt-plat/mtk_pwm.h>
#include <linux/pinctrl/consumer.h>
#include <linux/mca/common/mca_log.h>
#include <linux/mca/common/mca_event.h>
#include <linux/mca/common/mca_charge_mievent.h>
#include "xm_pwm_fan.h"

#ifndef MCA_LOG_TAG
#define MCA_LOG_TAG "xm_fan"
#endif


static struct pwm_spec_config fan_pwm_config = {
	.pwm_no = PWM2,
	.mode = PWM_MODE_OLD,
	.clk_div = CLK_DIV8,
	.clk_src = PWM_CLK_OLD_MODE_BLOCK,
	.pmic_pad = 0,
	.PWM_MODE_OLD_REGS.DATA_WIDTH = 100,
	.PWM_MODE_OLD_REGS.THRESH = 0,
	.PWM_MODE_OLD_REGS.IDLE_VALUE = IDLE_FALSE,
	.PWM_MODE_OLD_REGS.GUARD_VALUE = GUARD_FALSE,
	.PWM_MODE_OLD_REGS.GDURATION = 0,
	.PWM_MODE_OLD_REGS.WAVE_NUM = 0,
};


static int pwm_fan_power_on(struct pwm_fan_ctx *ctx, bool on)
{
	int ret = 0;

	if (!ctx || !ctx->fan_support) {
		mca_log_err("ctx is NULL or not support\n");
		return -EINVAL;
	}

	if (on == ctx->enabled) {
		mca_log_err("pwm_fan power mode no change\n");
		return 0;
	}

	if (ctx->use_extern_ldo) {
		if (!ctx->ldo) {
			mca_log_err("ldo regulator is NULL\n");
			return -EINVAL;
		}
		if (on) {
			ret = regulator_set_voltage(ctx->ldo, 3050*1000, INT_MAX);
			if (ret < 0) {
				mca_log_err("set ldo voltage failed\n");
				return ret;
			}
			ret = regulator_enable(ctx->ldo);
			if (ret < 0) {
				mca_log_err("regulator_enable failed\n");
				return ret;
			}
		} else {
			ret = regulator_disable(ctx->ldo);
			if (ret < 0) {
				mca_log_err("regulator_disable failed\n");
				return ret;
			}
		}
	} else {
		if (!gpio_is_valid(ctx->gpio_pwr_en)) {
			mca_log_err("gpio_pwr_en is invalid\n");
			return -EINVAL;
		}
		gpio_set_value(ctx->gpio_pwr_en, on);
	}

	mca_log_err("Power %s\n", on ? "ON" : "OFF");
	ctx->enabled = on;
	if (!on)
		ctx->real_speed = FAN_SPEED_DETECT_NOT_READY;

	return 0;
}

int pwm_reg_config(struct pwm_fan_ctx *ctx, int pwm_duty)
{
	int ret = 0;

	if (!ctx || !ctx->fan_support) {
		mca_log_err("ctx is NULL or not support\n");
		return -EINVAL;
	}

	mca_log_err("duty:%d->%d\n", ctx->pwm_duty, pwm_duty);
	fan_pwm_config.PWM_MODE_OLD_REGS.THRESH = pwm_duty;
	mt_pwm_clk_sel(fan_pwm_config.pwm_no, fan_pwm_config.pmic_pad, CLK_26M);
	ret = pwm_set_spec_config(&fan_pwm_config);

	return ret;
}

static int __set_pwm(struct pwm_fan_ctx *ctx, unsigned long duty)
{
	int ret = 0;

	if (!ctx || !ctx->fan_support) {
		mca_log_err("ctx is NULL or not support\n");
		return -EINVAL;
	}

	if (duty == ctx->pwm_duty) {
		mca_log_err("pwm duty is same, no need to set\n");
		return ret;
	}

	if (!ctx->enabled && duty > 0) {
		ret = pwm_fan_power_on(ctx, true);
		if (ret < 0) {
			mca_log_err("pwm_fan_power_on failed\n");
			return ret;
		}
		//ndelay(100);
		if (gpio_is_valid(ctx->gpio_shifter_en)) {
			gpio_set_value(ctx->gpio_shifter_en, 1);
		}
		ret = pwm_reg_config(ctx, 0);
		if (ret < 0) {
			mca_log_err("pre pwm_reg_config failed\n");
			return ret;
		}
		msleep(100);
		ret = pwm_reg_config(ctx, duty);
		if (ret < 0) {
			mca_log_err("pwm_reg_config failed\n");
			return ret;
		}
	} else if (ctx->enabled && duty == 0) {
		ret = pwm_reg_config(ctx, duty);
		if (ret < 0) {
			mca_log_err("pwm_reg_config failed\n");
			return ret;
		}
		mt_pwm_disable(fan_pwm_config.pwm_no, fan_pwm_config.pmic_pad);
		msleep(100);
		if (gpio_is_valid(ctx->gpio_shifter_en)) {
			gpio_set_value(ctx->gpio_shifter_en, 0);
		}
		//ndelay(100);
		ret = pwm_fan_power_on(ctx, false);
		if (ret < 0) {
			mca_log_err("pwm_fan_power_on failed\n");
			return ret;
		}
	} else {
		ret = pwm_reg_config(ctx, duty);
		if (ret < 0) {
			mca_log_err("pwm_reg_config failed\n");
			return ret;
		}
	}
	ctx->pwm_duty = duty;

	return ret;
}

static int set_pwm(struct pwm_fan_ctx *ctx, unsigned long duty)
{
	int ret = 0;

	if (!ctx || !ctx->fan_support) {
		mca_log_err("ctx is NULL or not support\n");
		return -EINVAL;
	}

	mutex_lock(&ctx->lock);
	ret = __set_pwm(ctx, duty);
	mutex_unlock(&ctx->lock);

	return ret;
}

static int get_pwn_fan_target_level(struct pwm_fan_ctx *ctx)
{
	int level = FAN_SPEED_LEVEL_OFF;

	if (!ctx || !ctx->fan_support) {
		mca_log_err("ctx is NULL or not support\n");
		return FAN_SPEED_LEVEL_OFF;
	}

	if (ctx->target_level_dbg == FAN_SPEED_LEVEL_DBG_OFF)
		level = ctx->target_level;
	else
		level = ctx->target_level_dbg - FAN_SPEED_LEVEL_DBG_OFF;

	return level;
}

static int set_pwn_fan_target_level(struct pwm_fan_ctx *ctx, int level)
{
	int ret = 0;
	int temp = level;

	if (!ctx || !ctx->fan_support) {
		mca_log_err("ctx is NULL or not support\n");
		return -EINVAL;
	}

	switch (temp) {
	case FAN_SPEED_LEVEL_OFF:
	case FAN_SPEED_LEVEL_LOW:
	case FAN_SPEED_LEVEL_MED:
	case FAN_SPEED_LEVEL_HIGH:
	case FAN_SPEED_LEVEL_TURBO:
		if (ctx->target_level_dbg != FAN_SPEED_LEVEL_DBG_OFF) {
			mca_log_err("target_level_dbg active, target_level not set\n");
			return ret;
		} else if (ctx->target_level == temp) {
			mca_log_err("target_level is same, no need to set\n");
			return ret;
		}
		ctx->target_level = temp;
		break;
	case FAN_SPEED_LEVEL_DBG_OFF:
		if (ctx->target_level_dbg == temp) {
			mca_log_err("target_level_dbg is same, no need to set\n");
			return ret;
		}
		ctx->target_level_dbg = temp;
		temp = ctx->target_level;
		break;
	case FAN_SPEED_LEVEL_DBG_LOW:
	case FAN_SPEED_LEVEL_DBG_MED:
	case FAN_SPEED_LEVEL_DBG_HIGH:
	case FAN_SPEED_LEVEL_DBG_TURBO:
		ctx->target_level_dbg = temp;
		temp = ctx->target_level_dbg - FAN_SPEED_LEVEL_DBG_OFF;
		break;
	default:
		mca_log_err("target_level invalid\n");
		return ret;
	}

	ctx->target_speed = ctx->duty_speed_map[temp].speed;

	if ((ctx->enabled && temp == FAN_SPEED_LEVEL_OFF)
			|| (!ctx->enabled && temp != FAN_SPEED_LEVEL_OFF)) {
		ctx->speed_adjust_cnt = 0;
		ctx->err_stat_report_cnt = 0;
		cancel_delayed_work_sync(&ctx->speed_detect_req_work);
		cancel_delayed_work_sync(&ctx->ldo_status_detect_work);
		schedule_delayed_work(&ctx->speed_detect_req_work, msecs_to_jiffies(1000));
		schedule_delayed_work(&ctx->ldo_status_detect_work, msecs_to_jiffies(2000));
	}

	ret = set_pwm(ctx, ctx->duty_speed_map[temp].duty);
	if (ret) {
		mca_log_err("setting speed_level failed!\n");
		return ret;
	}

	mca_log_err("level:%d[%d %d], duty:%d, speed:%d, en:%d\n",
		temp, ctx->target_level, ctx->target_level_dbg,
		ctx->duty_speed_map[temp].duty, ctx->target_speed, ctx->enabled);

	cancel_delayed_work_sync(&ctx->speed_dynamic_adjust_work);
	if (ctx->enabled)
		schedule_delayed_work(&ctx->speed_dynamic_adjust_work, msecs_to_jiffies(5000));

	return ret;
}

static int pwm_fan_support_check(struct pwm_fan_ctx *ctx)
{
	struct device_node *np = ctx->dev->of_node;
	int ret = 0, len = 0;
	int i = 0;
	u32 id = 0;

	if (!ctx || !np) {
		mca_log_err("ctx or np is NULL\n");
		return -EINVAL;
	}

	ctx->fan_support = false;
	return 0;

	ctx->fan_support = true;

	if (!of_property_read_bool(np, "unsupport_check")) {
		mca_log_err("no need to check id, pwm fan support\n");
		return 0;
	}

	len = of_property_count_elems_of_size(np, "unsupport_id", sizeof(u32));
	if (len < 0) {
		mca_log_err("failed to read unsupport_id len of config\n");
		return -EINVAL;
	}
	ret = of_property_read_u32_array(np, "unsupport_id", (u32 *)ctx->unsupport_id, len);
	if (ret) {
		mca_log_err("failed to parse unsupport_id\n");
		return ret;
	}

	len = min(len, FAN_UNSUPPORT_ID_COUNT);
	id = get_hw_id_value();
	mca_log_err("id:%x, len:%d\n", id, len);

	for (i = 0; i < len; i++) {
		mca_log_info("unsupport_id[%d]:%x\n", i, ctx->unsupport_id[i]);
		if (id == ctx->unsupport_id[i]) {
			ctx->fan_support = false;
			mca_log_err("id:%x, pwm fan unsupport\n", ctx->unsupport_id[i]);
			break;
		}
	}

	return 0;
}

static ssize_t fan_support_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct pwm_fan_ctx *ctx = dev_get_drvdata(dev);

	if (!ctx) {
		mca_log_err("ctx is NULL\n");
		return snprintf(buf, PAGE_SIZE, "%d\n", 0);
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", ctx->fan_support);
}
static DEVICE_ATTR_RO(fan_support);

static ssize_t pwm_duty_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int temp = 0, ret = 0;
	struct pwm_fan_ctx *ctx = dev_get_drvdata(dev);

	if (!ctx || !ctx->fan_support) {
		mca_log_err("ctx is NULL or not support\n");
		return count;
	}

	ret = sscanf(buf, "%d\n", &temp);
	if (!ret) {
		mca_log_err("param get failed\n");
		return count;
	}

	if (temp < FAN_PWM_DUTY_MIN)
		temp = FAN_PWM_DUTY_MIN;
	else if (temp > FAN_PWM_DUTY_MAX)
		temp = FAN_PWM_DUTY_MAX;

	if ((ctx->enabled && temp == FAN_PWM_DUTY_MIN)
			|| (!ctx->enabled && temp != FAN_PWM_DUTY_MIN)) {
		ctx->err_stat_report_cnt = 0;
		cancel_delayed_work_sync(&ctx->speed_detect_req_work);
		cancel_delayed_work_sync(&ctx->ldo_status_detect_work);
		schedule_delayed_work(&ctx->speed_detect_req_work, msecs_to_jiffies(1000));
		schedule_delayed_work(&ctx->ldo_status_detect_work, msecs_to_jiffies(2000));
	}

	ret = set_pwm(ctx, temp);
	if (ret) {
		mca_log_err("set pwm_duty failed!\n");
		return count;
	}

	return count;
}

static ssize_t pwm_duty_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct pwm_fan_ctx *ctx = dev_get_drvdata(dev);

	if (!ctx) {
		mca_log_err("ctx is NULL\n");
		return snprintf(buf, PAGE_SIZE, "%d\n", 0);
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", ctx->pwm_duty);
}
static DEVICE_ATTR_RW(pwm_duty);

static ssize_t target_level_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int temp = 0, ret = 0;
	struct pwm_fan_ctx *ctx = dev_get_drvdata(dev);

	if (!ctx || !ctx->fan_support) {
		mca_log_err("ctx is NULL or not support\n");
		return count;
	}

	ret = sscanf(buf, "%d\n", &temp);
	if (!ret) {
		mca_log_err("param get failed\n");
		return count;
	}

	ret = set_pwn_fan_target_level(ctx, temp);
	if (ret) {
		mca_log_err("set target_level failed!\n");
	}

	return count;
}

static ssize_t target_level_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct pwm_fan_ctx *ctx = dev_get_drvdata(dev);
	int temp = 0;

	if (!ctx) {
		mca_log_err("ctx is NULL\n");
		return snprintf(buf, PAGE_SIZE, "%d\n", 0);
	}

	if (ctx->target_level_dbg == FAN_SPEED_LEVEL_DBG_OFF)
		temp = ctx->target_level;
	else
		temp = ctx->target_level_dbg - FAN_SPEED_LEVEL_DBG_OFF;

	return snprintf(buf, PAGE_SIZE, "%d\n", temp);
}
static DEVICE_ATTR_RW(target_level);

static ssize_t real_speed_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int temp = 0, ret = 0;
	struct pwm_fan_ctx *ctx = dev_get_drvdata(dev);

	if (!ctx || !ctx->fan_support) {
		mca_log_err("ctx is NULL or not support\n");
		return count;
	}

	ret = sscanf(buf, "%d\n", &temp);
	if (!ret) {
		mca_log_err("real_speed param get failed\n");
		return count;
	}

	ctx->real_speed = temp;
	mca_log_err("%d\n", ctx->real_speed);

	return count;
}

static ssize_t real_speed_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct pwm_fan_ctx *ctx = dev_get_drvdata(dev);

	if (!ctx) {
		mca_log_err("ctx is NULL\n");
		return snprintf(buf, PAGE_SIZE, "%d\n", 0);
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", ctx->real_speed);
}
static DEVICE_ATTR_RW(real_speed);

static int pwm_fan_setup_files(struct pwm_fan_ctx *ctx, struct platform_device *pdev)
{
	int ret = 0;

	ret = device_create_file(&pdev->dev, &dev_attr_fan_support);
	if (ret) {
		mca_log_err("error creating sysfs files: fan_support\n");
		return ret;
	}

	if (!ctx->fan_support) {
		mca_log_err("fan not support, no need to setup other fnodes\n");
		return 0;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_pwm_duty);
	if (ret) {
		mca_log_err("error creating sysfs files: pwm_duty\n");
		return ret;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_target_level);
	if (ret) {
		mca_log_err("error creating sysfs files: target_level\n");
		return ret;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_real_speed);
	if (ret) {
		mca_log_err("error creating sysfs files: real_speed\n");
		return ret;
	}

	return ret;
}

static void speed_dynamic_adjust_work_func(struct work_struct *work)
{
	struct pwm_fan_ctx *ctx = container_of(work, struct pwm_fan_ctx, speed_dynamic_adjust_work.work);
	int speed_gap = 0, tune = 0, duty = 0;
	char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
	struct mca_event_notify_data event_data = { 0 };
	int len;
	static unsigned int last_target_speed = 0;

	if (!ctx || !ctx->fan_support) {
		mca_log_err("ctx is NULL or not support\n");
		return;
	}

	if (!ctx->enabled) {
		mca_log_err("pwm fan is disabled\n");
		ctx->speed_adjust_cnt = 0;
		return;
	}

	if (get_pwn_fan_target_level(ctx) == FAN_SPEED_LEVEL_OFF) {
		mca_log_err("target level is 0\n");
		ctx->speed_adjust_cnt = 0;
		return;
	}

	if (ctx->real_speed == FAN_SPEED_DETECT_NOT_READY) {
		mca_log_err("speed detection is not ready, waiting...\n");
		schedule_delayed_work(&ctx->speed_dynamic_adjust_work, msecs_to_jiffies(1000));
		return;
	}

	if (ctx->real_speed < ctx->speed_adjust_cfg[0].min_speed || ctx->real_speed > ctx->speed_adjust_cfg[0].max_speed) {
		mca_log_err("real speed is out of range, report it\n");
		len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "MCA_FAN_ERROR_INFO=SPEED_OUT_OF_RANGE");
		event_data.event = event;
		event_data.event_len = len;
		mca_event_report_uevent(&event_data);
		return;
	}

	if (last_target_speed != ctx->target_speed) {
		mca_log_err("target speed changed, reset counter\n");
		ctx->speed_adjust_cnt = 0;
		last_target_speed = ctx->target_speed;
	}

	speed_gap = ctx->real_speed - ctx->target_speed;
	if (speed_gap > (int)(ctx->speed_adjust_cfg[0].speed_gap_thres)) {
		ctx->speed_adjust_cnt++;
		tune = -1;
	} else if (speed_gap < -((int)(ctx->speed_adjust_cfg[0].speed_gap_thres))) {
		ctx->speed_adjust_cnt++;
		tune = 1;
	} else {
		ctx->speed_adjust_cnt = 0;
		tune = 0;
	}

	duty = ctx->pwm_duty + tune;
	if (duty < FAN_PWM_DUTY_MIN)
		duty = FAN_PWM_DUTY_MIN;
	else if (duty > FAN_PWM_DUTY_MAX)
		duty = FAN_PWM_DUTY_MAX;

	if (ctx->speed_adjust_cnt >= ctx->speed_adjust_cfg[0].adjust_cnt_max) {
		mca_log_err("speed adjust failed, report it\n");
		len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "MCA_FAN_ERROR_INFO=SPEED_ADJUST_FAIL");
		event_data.event = event;
		event_data.event_len = len;
		mca_event_report_uevent(&event_data);
		return;
	}

	mca_log_err("speed_gap:%d[%d %d], adjust:%d[%d->%d]\n",
			speed_gap, ctx->target_speed, ctx->real_speed,
			ctx->speed_adjust_cnt, ctx->pwm_duty, duty);

	if (duty != ctx->pwm_duty)
		set_pwm(ctx, duty);
	if (tune)
		schedule_delayed_work(&ctx->speed_dynamic_adjust_work, msecs_to_jiffies(2000));

}

static void speed_detect_req_work_func(struct work_struct *work)
{
	struct pwm_fan_ctx *ctx = container_of(work, struct pwm_fan_ctx, speed_detect_req_work.work);
	char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
	struct mca_event_notify_data event_data = { 0 };
	int len;
	int target_level = 0;

	if (!ctx || !ctx->fan_support) {
		mca_log_err("ctx is NULL or not support\n");
		return;
	}

	target_level = get_pwn_fan_target_level(ctx);
	if (target_level == FAN_SPEED_LEVEL_OFF || !ctx->enabled) {
		len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "MCA_FAN_ENABLE=0");
		event_data.event = event;
		event_data.event_len = len;
		mca_event_report_uevent(&event_data);
	} else {
		len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "MCA_FAN_ENABLE=1");
		event_data.event = event;
		event_data.event_len = len;
		mca_event_report_uevent(&event_data);
	}
}

static void ldo_status_detect_work_func(struct work_struct *work)
{
	struct pwm_fan_ctx *ctx = container_of(work, struct pwm_fan_ctx, ldo_status_detect_work.work);
	char event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
	struct mca_event_notify_data event_data = { 0 };
	int len;
	unsigned int flags = 0;

	if (!ctx || !ctx->fan_support) {
		mca_log_err("ctx is NULL or not support\n");
		return;
	}

	if (!ctx->enabled) {
		mca_log_err("pwm fan is disabled\n");
		ctx->err_stat_report_cnt = 0;
		return;
	}

	if (get_pwn_fan_target_level(ctx) == FAN_SPEED_LEVEL_OFF) {
		mca_log_err("target level is 0\n");
		ctx->err_stat_report_cnt = 0;
		return;
	}

	flags = regulator_get_mode(ctx->ldo);
	mca_log_err("ldo_error flags:%d, cnt:%d\n", flags, ctx->err_stat_report_cnt);
	if (flags) {
		ctx->err_stat_report_cnt++;
		len = snprintf(event, MCA_EVENT_NOTIFY_SIZE, "MCA_FAN_ERROR_INFO=FAN_LDO_ERROR");
		event_data.event = event;
		event_data.event_len = len;
		mca_event_report_uevent(&event_data);
	}

	if (ctx->err_stat_report_cnt < FAN_LDO_ERROR_REPORT_CNT_MAX)
		schedule_delayed_work(&ctx->ldo_status_detect_work, msecs_to_jiffies(2000));
}

static void pwm_fan_cleanup(void *__ctx)
{
	struct pwm_fan_ctx *ctx = __ctx;

	if (!ctx) {
		mca_log_err("ctx is NULL\n");
		return;
	}

	if (ctx->fan_support)
		pwm_fan_power_on(ctx, false);
}

static int pwm_fan_ldo_notify(struct notifier_block *nb, unsigned long event, void *data)
{
	struct pwm_fan_ctx *ctx = container_of(nb, struct pwm_fan_ctx, ldo_nb);
	char mca_event[MCA_EVENT_NOTIFY_SIZE] = { 0 };
	struct mca_event_notify_data mca_event_data = { 0 };
	int len;

	if (!ctx || !ctx->fan_support) {
		mca_log_err("ctx is NULL or not support\n");
		return -EINVAL;
	}

	mca_log_err("get ldo event: %lu\n", event);
	switch (event) {
	case REGULATOR_EVENT_OVER_CURRENT:
		cancel_delayed_work_sync(&ctx->speed_dynamic_adjust_work);
		ctx->target_level_dbg = FAN_SPEED_LEVEL_DBG_OFF;
		ctx->real_speed = FAN_SPEED_DETECT_NOT_READY;
		set_pwn_fan_target_level(ctx, FAN_SPEED_LEVEL_OFF);
		len = snprintf(mca_event, MCA_EVENT_NOTIFY_SIZE, "MCA_FAN_ERROR_INFO=FAN_LDO_OCP");
		mca_event_data.event = mca_event;
		mca_event_data.event_len = len;
		mca_event_report_uevent(&mca_event_data);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static int pwm_fan_parse_dt(struct pwm_fan_ctx *ctx)
{
	struct device_node *np = ctx->dev->of_node;
	int ret = 0;
	int len = 0;

	if (!ctx || !np) {
		mca_log_err("ctx or node is NULL\n");
		return -EINVAL;
	}

	of_property_read_u32(np, "pwm_ch", &ctx->pwm_ch);
	if (ctx->pwm_ch >= PWM_MAX) {
		mca_log_err("Invalid pwm_ch: %d\n", ctx->pwm_ch);
		return -EINVAL;
	}
	fan_pwm_config.pwm_no = (unsigned int)ctx->pwm_ch;

	len = of_property_count_elems_of_size(np, "speed_adjust_cfg", sizeof(u32));
	if (len < 0) {
		mca_log_err("failed to read speed_adjust_cfg len of config\n");
		return -EINVAL;
	}
	ret = of_property_read_u32_array(np, "speed_adjust_cfg", (u32 *)ctx->speed_adjust_cfg, len);
	if (ret) {
		mca_log_err("failed to parse speed_adjust_cfg\n");
		return ret;
	}

	len = of_property_count_elems_of_size(np, "duty_speed_map", sizeof(u32));
	if (len < 0) {
		mca_log_err("failed to read duty_speed_map len of config\n");
		return -EINVAL;
	}
	ret = of_property_read_u32_array(np, "duty_speed_map", (u32 *)ctx->duty_speed_map, len);
	if (ret) {
		mca_log_err("failed to parse duty_speed_map\n");
		return ret;
	}

	ctx->gpio_shifter_en = of_get_named_gpio(np, "shifter_en", 0);
	if (!gpio_is_valid(ctx->gpio_shifter_en)) {
		mca_log_err("Invalid GPIO_SHIFTER_EN: %d\n", ctx->gpio_shifter_en);
		return ctx->gpio_shifter_en;
	}
	ret = devm_gpio_request(ctx->dev, ctx->gpio_shifter_en, "shifter_en");
	if (ret) {
		mca_log_err("Failed to request GPIO_SHIFTER_EN: %d\n", ret);
		return ret;
	}
	ret = gpio_direction_output(ctx->gpio_shifter_en, 0);
	if (ret) {
		mca_log_err("Failed to set GPIO_SHIFTER_EN as output: %d\n", ret);
		return ret;
	}

	ctx->use_extern_ldo = of_property_read_bool(np, "use_extern_ldo");
	if (ctx->use_extern_ldo) {
		if (!of_property_read_bool(np, "ldo-supply")) {
			mca_log_err("Failed to get ldo-supply cfg\n");
			return -EINVAL;
		}
		ctx->ldo =  devm_regulator_get(ctx->dev, "ldo");
		if (IS_ERR(ctx->ldo)) {
			mca_log_err("Failed to get ldo regulator\n");
			ctx->ldo = NULL;
			return PTR_ERR(ctx->ldo);
		}
		ctx->ldo_nb.notifier_call = pwm_fan_ldo_notify;
		ret = devm_regulator_register_notifier(ctx->ldo, &ctx->ldo_nb);
		if (ret) {
			mca_log_err("Failed to register ldo notifier: %d\n", ret);
			return ret;
		}
		if (regulator_is_enabled(ctx->ldo)) {
			mca_log_err("ldo is on, need to turn off\n");
			ret = regulator_disable(ctx->ldo);
			if (ret)
				mca_log_err("Failed to disable ldo: %d\n", ret);
			msleep(10);
		}
		ctx->enabled = regulator_is_enabled(ctx->ldo);
		mca_log_err("ldo is %s\n", ctx->enabled ? "on" : "off");
	} else {
		ctx->gpio_pwr_en = of_get_named_gpio(np, "power_enable", 0);
		if (!gpio_is_valid(ctx->gpio_pwr_en)) {
			mca_log_err("Invalid GPIO_PWR_EN: %d\n", ctx->gpio_pwr_en);
			return ctx->gpio_pwr_en;
		}

		ret = devm_gpio_request(ctx->dev, ctx->gpio_pwr_en, "power_enable");
		if (ret) {
			mca_log_err("Failed to request GPIO_PWR_EN: %d\n", ret);
			return ret;
		}

		ret = gpio_direction_output(ctx->gpio_pwr_en, 0);
		if (ret) {
			mca_log_err("Failed to set GPIO_PWR_EN as output: %d\n", ret);
			return ret;
		}
		ctx->enabled = false;
	}

	return ret;
}

static int pwm_fan_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct pwm_fan_ctx *ctx;
	int len = 0, offset = 0;
	int ret = 0;

	mca_log_err("enter pwm_fan_probe\n");

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		mca_log_err("failed to allocate memory\n");
		return -ENOMEM;
	}

	ctx->dev = dev;
	ctx->target_level_dbg = FAN_SPEED_LEVEL_DBG_OFF;
	ctx->real_speed = FAN_SPEED_DETECT_NOT_READY;
	ctx->err_stat_report_cnt = 0;
	mutex_init(&ctx->lock);
	platform_set_drvdata(pdev, ctx);

	len = (strlen(np->name) > (MAX_FAN_NAME_LEN - 1)) ? (MAX_FAN_NAME_LEN - 1) : strlen(np->name);
	offset = strscpy(ctx->name, np->name, len + 1);
	if (offset < 0) {
		mca_log_err("%s: offset=%d, len=%d fail\n", ctx->name, offset, len);
		return -EINVAL;
	}

	pwm_fan_support_check(ctx);
	pwm_fan_setup_files(ctx, pdev);
	if (!ctx->fan_support) {
		mca_log_err("fan is not supported\n");
		return -EINVAL;
	}

	ret = pwm_fan_parse_dt(ctx);
	if (ret) {
		mca_log_err("failed to parse dt\n");
		return ret;
	}

	ret = set_pwm(ctx, FAN_PWM_DUTY_MIN);
	if (ret)
		mca_log_err("Failed to configure PWM: %d\n", ret);

	INIT_DELAYED_WORK(&ctx->speed_dynamic_adjust_work, speed_dynamic_adjust_work_func);
	INIT_DELAYED_WORK(&ctx->speed_detect_req_work, speed_detect_req_work_func);
	INIT_DELAYED_WORK(&ctx->ldo_status_detect_work, ldo_status_detect_work_func);

	mca_log_err("pwm_fan_probe successful\n");
	return 0;
}

static void pwm_fan_shutdown(struct platform_device *pdev)
{
	struct pwm_fan_ctx *ctx = platform_get_drvdata(pdev);
	device_remove_file(&pdev->dev, &dev_attr_pwm_duty);
	pwm_fan_cleanup(ctx);
}

static int pwm_fan_suspend(struct device *dev)
{
	return 0;
}

static int pwm_fan_resume(struct device *dev)
{
	return 0;
}

static DEFINE_SIMPLE_DEV_PM_OPS(pwm_fan_pm, pwm_fan_suspend, pwm_fan_resume);

static const struct of_device_id of_pwm_fan_match[] = {
	{ .compatible = "xm,xm-pwm-fan", },
	{},
};
MODULE_DEVICE_TABLE(of, of_pwm_fan_match);

static struct platform_driver pwm_fan_driver = {
	.probe		= pwm_fan_probe,
	.shutdown	= pwm_fan_shutdown,
	.driver	= {
		.name		= "pwm-fan",
		.pm		= pm_sleep_ptr(&pwm_fan_pm),
		.of_match_table	= of_pwm_fan_match,
	},
};

module_platform_driver(pwm_fan_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("linjiashuo <linjiashuo@xiaomi.com>");
MODULE_ALIAS("platform:pwm-fan");
MODULE_DESCRIPTION("PWM FAN driver");
