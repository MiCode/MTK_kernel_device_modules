/* SPDX-License-Identifier: GPL-2.0 */
/*
 * aw85601.h  --  driver for awinic HFDA80x codec
 *
 * Copyright(C) 2025  awinic Ltd
 * Author:
 */

#ifndef __AW85601_H__
#define __AW85601_H__

struct aw85601 {
	struct device *dev;
	struct regmap *regmap;
	struct gpio_desc *enable_gpio;
	struct gpio_desc *mute_gpio;
};

#define AW85601_DEVICE_ID1 0x40
#define AW85601_DEVICE_ID2 0x41

#endif /* __AW85601_H__ */
