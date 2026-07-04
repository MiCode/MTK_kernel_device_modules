// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/slab.h>

#define GPIO_HALL 6
int hall_gpio = -1;

static ssize_t hall_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int val = -1,res = 0;

	val = gpio_get_value(hall_gpio) ? 0 : 1;

	res = snprintf(buf, PAGE_SIZE, "%d\n", val);
	return res;
}

static struct device_attribute dev_attr_hall_state =
	__ATTR(hall_state, 0644, hall_state_show, NULL);

static int hall_state_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;

	ret = sysfs_create_file(&dev->kobj, &dev_attr_hall_state.attr);
	if (ret < 0) {
		pr_err("Failed to create attribute hall_state!\n");
	} else {
		pr_info("%s Device cerate hall_state file success!\n", __func__);
	}
	hall_gpio = of_get_named_gpio(np, "status-gpio", 0);
	if (hall_gpio < 0){
		pr_err("Failed to get hall gpio name!\n");
		return -EINVAL;
	}

	return 0;
}

static const struct of_device_id hall_state_of_match[] = {
	{ .compatible = "hall-state", },
	{},
};

static struct platform_driver hall_state_driver = {
	.driver = {
		.name = "hall-state",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(hall_state_of_match),
	},
	.probe = hall_state_probe,
};
module_platform_driver(hall_state_driver);
MODULE_DESCRIPTION("HALL state");
MODULE_LICENSE("GPL");
