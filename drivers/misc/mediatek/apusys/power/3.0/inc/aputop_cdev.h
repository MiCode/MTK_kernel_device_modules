/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __APUTOP_CDEV_H__
#define __APUTOP_CDEV_H__

#define MAX_APU_COOLER_NAME_LEN	(20)

enum aputop_cdev_status{
	APUCDEV_UNVALID = 0,
	APUCDEV_NOT_READY,
	APUCDEV_READY,
};

/*==================================================
 * Type Definitions
 *==================================================
 */
/**
 * struct apu_cooling_device - data for apu cooling device
 * @name: naming string for this cooling device
 * @target_state: target cooling state which is set in set_cur_state()
 *	callback.
 * @max_state: maximum state supported for this cooling device
 * @unlimite_state: minimum state supported for this cooling device
 * @cdev: thermal_cooling_device pointer to keep track of the
 *	registered cooling device.
 * @throttle: callback function to handle throttle request
 * @dev: device node pointer
 * @status: status of apu cooling device
 * @request_id: id for throttle request
 */
struct apu_cooling_device {
	char name[MAX_APU_COOLER_NAME_LEN];
	unsigned long target_state;
	unsigned long max_state;
	unsigned long unlimite_state;
	struct thermal_cooling_device *cdev;
	int (*throttle)(struct apu_cooling_device *bl_cdev, unsigned long state);
	struct device *dev;
	enum aputop_cdev_status status;
	int request_id;
};

int init_apu_cooling_device(struct device *dev, struct apu_cooling_device *apu_cdev);

#endif
