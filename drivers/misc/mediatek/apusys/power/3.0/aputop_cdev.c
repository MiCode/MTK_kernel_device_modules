// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/thermal.h>
#include <linux/types.h>

#include "apu_top.h"
#include "aputop_cdev.h"

static int apu_cooling_get_max_state(struct thermal_cooling_device *cdev,
	unsigned long *state)
{
	struct apu_cooling_device *apu_cdev = cdev->devdata;
	*state = apu_cdev->max_state;

	return 0;
}

static int apu_cooling_get_cur_state(struct thermal_cooling_device *cdev,
	unsigned long *state)
{
	struct apu_cooling_device *apu_cdev = cdev->devdata;
	*state = apu_cdev->target_state;

	return 0;
}

static int apu_cooling_set_cur_state(struct thermal_cooling_device *cdev,
	unsigned long state)
{
	struct apu_cooling_device *apu_cdev = cdev->devdata;
	int ret;

	/* Check if the cooling device is ready or not */
	if (apu_cdev->status == APUCDEV_NOT_READY)
		return -EAGAIN;

	/* Request state should be less than max_state */
	if (WARN_ON(state > apu_cdev->max_state || !apu_cdev->throttle))
		return -EINVAL;

	if (apu_cdev->target_state == state)
		return 0;

	ret = apu_cdev->throttle(apu_cdev, state);

	return ret;
}

static int apu_throttle(struct apu_cooling_device *apu_cdev, unsigned long state)
{
	struct device *dev = apu_cdev->dev;

	if (state <= apu_cdev->unlimite_state)
		state = apu_cdev->unlimite_state;

	apu_sw_throttle(&apu_cdev->request_id, state);

	apu_cdev->target_state = state;
	dev_info(dev, "%s: set state %ld done\n", apu_cdev->name, state);
	return 0;
}

static struct thermal_cooling_device_ops apu_cooling_ops = {
	.get_max_state		= apu_cooling_get_max_state,
	.get_cur_state		= apu_cooling_get_cur_state,
	.set_cur_state		= apu_cooling_set_cur_state,
};

int init_apu_cooling_device(struct device *dev, struct apu_cooling_device *apu_cdev)
{
	struct thermal_cooling_device *cdev;
	char *name = "apu_cooler";
	int ret;

	ret = snprintf(apu_cdev->name, MAX_APU_COOLER_NAME_LEN, "%s", name);
	if(ret < 0)
		goto init_fail;

	apu_cdev->throttle = apu_throttle;
	apu_cdev->dev = dev;

	/* Following three lines will be ready in platform probe function */
	apu_cdev->status = APUCDEV_NOT_READY;
	apu_cdev->max_state = 0;
	apu_cdev->target_state = 0;
	apu_cdev->request_id = 0;

	cdev = thermal_of_cooling_device_register(dev->of_node, apu_cdev->name,
			apu_cdev, &apu_cooling_ops);
	dev_info(dev, "register %s done, but not ready\n", apu_cdev->name);

	long err = PTR_ERR(cdev);

	if (IS_ERR(cdev)){
		dev_info(dev, "Failed to register cooling device: %ld\n", err);
		goto init_fail;
	}

	apu_cdev->cdev = cdev;

	return 0;

init_fail:
	return -EINVAL;
}

