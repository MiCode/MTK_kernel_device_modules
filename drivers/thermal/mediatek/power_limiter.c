// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/power_supply.h>
#include <linux/delay.h>

#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/fs.h>

static struct power_supply *bat_psy;
static struct task_struct *kthread_task_handle;

#include "power_limiter.h"
#include "thermal_interface.h"

// PID parameter
static int Kp; // Proportional
static int Ki;  // Proportional
static int Kd;   // Derivative

// input parameter
static int target_power; //target power(8000mW=8)
static int output;      // TTj (modify TTj to adjust power)
static int integral;
static int last_error;
static int max_temperature;
static int min_temperature;
static int integral_max; // Integral max
static int integral_min; // Integral min
static int timer_ms;
static int scall;
static int steady_target_tj;
static int last_tj;
static int new_tj;

static void pid_initial(void)
{
	target_power = 8000;
	output = 0;
	integral = 0;
	last_error = 0;
	max_temperature = 95000;
	min_temperature = 35000;
	integral_max = 1000;
	integral_min = -1000;
	timer_ms = 1000;
	scall = 1000;
	steady_target_tj = 60000;
	last_tj = 0;
	new_tj = 0;
	Kp = 1000;
	Ki = 100;
	Kd = 50;
}

static ssize_t power_limiter_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n",
		target_power,
		max_temperature,
		min_temperature,
		Kp,
		Ki,
		Kd,
		integral_max,
		integral_min,
		timer_ms,
		scall,
		steady_target_tj);

	return len;
}

static int cmd_cmp(char cmd[], const char option[])
{
	if (strlen(cmd) != strlen(option))
		return 0;

	if (!strncmp(cmd, option, strlen(option)))
		return 1;

	return 0;
}

static ssize_t power_limiter_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	char param_name[20];
	int value1, value2, value3;

	while (*buf != '\0') {
		if (sscanf(buf, "%19s", param_name) == 1) {
			if (cmd_cmp(param_name, "reset")) {
				pid_initial();
				pr_info("power_limiter reset\n");
			} else if (cmd_cmp(param_name, "target_power")) {
				if (sscanf(buf + strlen(param_name), "%d", &value1) == 1) {
					target_power = value1;
					pr_info("set target_power %d\n", value1);
				}
			} else if (cmd_cmp(param_name, "temp")) {
				if (sscanf(buf + strlen(param_name), "%d %d", &value1, &value2) == 2) {
					max_temperature = value1;
					min_temperature = value2;
					pr_info("set temperature %d %d\n", value1, value2);
				}
			} else if (cmd_cmp(param_name, "Kpid")) {
				if (sscanf(buf + strlen(param_name), "%d %d %d", &value1, &value2, &value3) == 3) {
					Kp = value1;
					Ki = value2;
					Kd = value3;
					pr_info("set temperature %d %d\n", value1, value2);
				}
			} else if (cmd_cmp(param_name, "integral")) {
				if (sscanf(buf + strlen(param_name), "%d %d", &value1, &value2) == 2) {
					max_temperature = value1;
					min_temperature = value2;
				}
			} else if (cmd_cmp(param_name, "timer_ms")) {
				if (sscanf(buf + strlen(param_name), "%d", &value1) == 1)
					timer_ms = value1;
			} else if (cmd_cmp(param_name, "scall")) {
				if (sscanf(buf + strlen(param_name), "%d", &value1) == 1)
					scall = value1;
			} else if (cmd_cmp(param_name, "steady_target_tj")) {
				if (sscanf(buf + strlen(param_name), "%d", &value1) == 1)
					steady_target_tj = value1; // 更新最後目標值
			}
		}

		buf = strchr(buf, '\n');
		if (!buf)
			break;
		buf++;
	}

	pr_info("[%s] invalid input\n", __func__);

	return -EINVAL;
}

static struct kobj_attribute power_limiter_attr = __ATTR_RW(power_limiter);

static struct attribute *thermal_attrs[] = {
	&power_limiter_attr.attr,
	NULL
};
static struct attribute_group thermal_attr_group = {
	.name	= "power_limiter",
	.attrs	= thermal_attrs,
};

static void wait_test(void)
{
	new_tj = target_power*3+25000;
	set_ttj_for_fixed_power(FIX_PWR, new_tj);
	pr_info("%s new_tj=%d\n", __func__, new_tj);
}

static void pid_update(int current_power)
{
	int error;
	int derivative;

	if(integral == 0)
		last_tj = target_power*7+25000;

	error = target_power - current_power;
	integral += error;

	if (integral > integral_max)
		integral = integral_max;
	else if (integral < integral_min)
		integral = integral_min;

	derivative = error - last_error;
	output = (Kp * error + Ki * integral + Kd * derivative) / scall;

	pr_info("%s output(%d) = [Kp(%d) * error(%d) + Ki(%d) * integ.(%d) + Kd(%d) * deri.(%d)]/scall(%d), steady_T(%d)\n",
		__func__, output, Kp, error, Ki, integral, Kd,
		derivative, scall, steady_target_tj);
	new_tj = last_tj + output;
	pr_info("%s new_tj(%d) = last_tj(%d) + output(%d)\n", __func__, new_tj, last_tj, output);

	new_tj = max(min_temperature, min(new_tj, max_temperature));
	set_ttj_for_fixed_power(FIX_PWR, new_tj);
	pr_info("%s new_tj=%d\n", __func__, new_tj);

	last_tj = new_tj;
	last_error = error;
}

static void power_limiter(void)
{
	int ret = 0;
	unsigned long long power_current = 0;
	union power_supply_propval ps_current = { .intval = 0 }, ps_voltage = {.intval = 0};

	if (bat_psy != NULL) {
		ret = power_supply_get_property(bat_psy,
			POWER_SUPPLY_PROP_CURRENT_NOW, &ps_current);

//		current_avg:
//		ret = power_supply_get_property(bat_psy,
//			POWER_SUPPLY_PROP_CURRENT_AVG, &ps_current);

		if(ps_current.intval >= 0){
			pr_info("%s charging.\n", __func__);
			return;
		}

		ret = power_supply_get_property(bat_psy,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, &ps_voltage);
		power_current = (ps_current.intval/1000) *
			(ps_voltage.intval / 100) * -1;

		power_current = power_current / 10000;

		if(power_current <= 3000 && integral == 0 )
			wait_test();
		else
			pid_update(power_current);
	}
	pr_info("%s ret(%d)(%lld)\n", __func__, ret, power_current);
}

static int power_reader_and_limiter(void *data)
{
	bat_psy = power_supply_get_by_name("battery");

	pid_initial();
	while (!kthread_should_stop()) {
		power_limiter();
		msleep(timer_ms);
	}
	return 0;
}

static int __init power_limiter_init(void)
{
	int ret;

	kthread_task_handle = kthread_run(power_reader_and_limiter,
		NULL, "power_limiter");
	if (!kthread_task_handle)
		return -ECHILD;

	ret = sysfs_create_group(kernel_kobj, &thermal_attr_group);
	if (ret) {
		pr_info("%s failed to create thermal sysfs, ret=%d!\n",
			__func__, ret);
		return ret;
	}
	return 0;
}
static void __exit power_limiter_exit(void)
{
	if (kthread_task_handle){
		kthread_stop(kthread_task_handle);
		kthread_task_handle = NULL;
	}

	sysfs_remove_group(kernel_kobj, &thermal_attr_group);
}

module_init(power_limiter_init);
module_exit(power_limiter_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mediatek POWER LIMITER");
MODULE_AUTHOR("MediaTek Inc.");

