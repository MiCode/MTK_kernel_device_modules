/*
 *  Silicon Integrated Co., Ltd haptic sih6889 regmap parameter define
 *
 *  Copyright (c) 2024 shanfa <shanfa.tang@si-in.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation
 */

#include <linux/regmap.h>
#include <linux/device.h>
#include "haptic_regmap.h"
#include "sih6889_reg.h"

static bool sih6889_writeable_register(struct device *dev, unsigned int reg)
{
	return true;
}

static bool sih6889_readable_register(struct device *dev, unsigned int reg)
{
	return true;
}

static bool sih6889_volatile_register(struct device *dev, unsigned int reg)
{
	return true;
}

const struct regmap_config sih6889_regmap_config = {
	.name = "sih6889",
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = SIH6889_REG_MAX,
	.writeable_reg = sih6889_writeable_register,
	.readable_reg = sih6889_readable_register,
	.volatile_reg = sih6889_volatile_register,
	.cache_type = REGCACHE_NONE,
};

