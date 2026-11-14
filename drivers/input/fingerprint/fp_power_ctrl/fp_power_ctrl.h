/*
 * Copyright (C) 2013-2016, Shenzhen Huiding Technology Co., Ltd.
 * All Rights Reserved.
 */
#ifndef __FP_POWER_CTRL_H
#define __FP_POWER_CTRL_H

#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include  <linux/pinctrl/consumer.h>
#include  <linux/regulator/consumer.h>

struct fp_power_ctrl_data {
	struct device *dev;
	struct regulator *vreg;
	int power_status;
};

int fp_power_ctrl_on(void);
int fp_power_ctrl_off(void);

#endif	/* __FP_POWER_CTRL_H */
