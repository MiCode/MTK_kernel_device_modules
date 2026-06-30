// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 Mediatek Inc.
//

#include <linux/init.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of_device.h>
#include <sound/initval.h>
#include <sound/soc.h>

#include "aw85601.h"

static const struct reg_default init_reg_tab[] = {
	/* physical register filling configuration */
	{0x7F,0x00},
	{0x00,0x46},
	{0x06,0x01},
	{0x0C,0x00},
	{0x0E,0x11},
	{0x0F,0x11},
	{0x10,0x11},
	{0x11,0x11},
	{0x17,0x01},
	{0x18,0x01},
	{0x45,0x8C},
	{0x46,0x36},
	{0x4B,0x0C},
	{0x4C,0x0C},
	{0x4D,0x0C},
	{0x4E,0x0C},
	{0x50,0x62},
	{0x53,0x43},
	{0x7E,0x22},
	{0x7F,0x01},
	{0x04,0xFB},
	{0x0E,0x0C},
	{0x13,0x90},
	{0x14,0x70},
	{0x15,0x11},
	{0x16,0x89},
	{0x1D,0x1C},
	{0x1E,0xB2},
	{0x1F,0x0A},
	{0x20,0x81},
	{0x23,0xAB},
	{0x25,0x1C},
	{0x26,0xB2},
	{0x27,0x0A},
	{0x28,0x81},
	{0x2B,0xAB},
	{0x2D,0x1C},
	{0x2E,0xB2},
	{0x2F,0x08},
	{0x30,0xB5},
	{0x33,0xAB},
	{0x35,0x1C},
	{0x36,0xB2},
	{0x37,0x0A},
	{0x38,0x81},
	{0x3B,0xAB},
	{0x65,0x10},
	{0x7F,0x00}
};

static int aw85601_read_chipid(struct aw85601 *aw85601)
{
	int i = 0;

	struct {
		unsigned int val;
		const int def;
	} devid[] = {
		{0, 0x22},
		{0, 0x06},
	};

	for (i = 0; i < ARRAY_SIZE(devid); i++) {
		devid[i].val = regmap_read(aw85601->regmap, AW85601_DEVICE_ID1 + i, &devid[i].val);
		if (devid[i].val > 0xFF) {
			dev_info(aw85601->dev, "%s() Read ID Error!\n", __func__);
			return -EIO;
		}

		if (devid[i].val != devid[i].def) {
			dev_info(aw85601->dev, "%s() it is not aw85601 dev", __func__);
			return -EIO;
		}
	}

	dev_info(aw85601->dev, "read chip id: 0x%x", devid[i].val);

	return 0;
}

static int aw85601_init(struct aw85601 *aw85601)
{
	int i = 0, ret = 0;
	unsigned int addr;
	unsigned int data;

	// i2c enable
	ret = regmap_update_bits(aw85601->regmap, 0x00, 0x3, 0x2);
	if (ret) {
		dev_info(aw85601->dev, "%s, i2c enable failed, ret:%d\n", __func__, ret);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(init_reg_tab); i++) {
		addr = init_reg_tab[i].reg;
		data = init_reg_tab[i].def;

		if ((addr == 0x0E) || (addr == 0x0F) || (addr == 0x10) || (addr == 0x11))
			data = data & (~(1 << 0 | 1 << 4));
		else if (addr == 0x17)
			data = data & (~(1 << 0));

		ret = regmap_write(aw85601->regmap, addr, data);
		if (ret) {
			dev_info(aw85601->dev, "%s, init reg failed, ret:%d\n", __func__, ret);
			return ret;
		}
	}

	return ret;
}

static int aw85601_stop(struct aw85601 *aw85601)
{
	/* enable I2C */
	regmap_update_bits(aw85601->regmap, 0x00, 0x3, 0x2);
	/* enable mute */
	regmap_update_bits(aw85601->regmap, 0x0E, 0x1, 0x0);
	regmap_update_bits(aw85601->regmap, 0x0F, 0x1, 0x0);
	regmap_update_bits(aw85601->regmap, 0x10, 0x1, 0x0);
	regmap_update_bits(aw85601->regmap, 0x11, 0x1, 0x0);
	/* disable pwm */
	regmap_update_bits(aw85601->regmap, 0x0E, 0x1 << 4, 0x0);
	regmap_update_bits(aw85601->regmap, 0x0F, 0x1 << 4, 0x0);
	regmap_update_bits(aw85601->regmap, 0x10, 0x1 << 4, 0x0);
	regmap_update_bits(aw85601->regmap, 0x11, 0x1 << 4, 0x0);
	/* disable pa */
	regmap_update_bits(aw85601->regmap, 0x17, 0x1, 0x0);

	return 0;
}

static int aw85601_start(struct aw85601 *aw85601)
{
	/* enable I2C */
	regmap_update_bits(aw85601->regmap, 0x00, 0x2, 0x2);
	/* enable pa */
	regmap_update_bits(aw85601->regmap, 0x17, 0x1, 0x1);
	/* delay 120ms */
	mdelay(120);
	/* enable pwm */
	regmap_update_bits(aw85601->regmap, 0x0E, 0x1 << 4, 0x1);
	regmap_update_bits(aw85601->regmap, 0x0F, 0x1 << 4, 0x1);
	regmap_update_bits(aw85601->regmap, 0x10, 0x1 << 4, 0x1);
	regmap_update_bits(aw85601->regmap, 0x11, 0x1 << 4, 0x1);
	/* delay 2ms */
	mdelay(2);
	/* enable mute */
	regmap_update_bits(aw85601->regmap, 0x0E, 0x1, 0x1);
	regmap_update_bits(aw85601->regmap, 0x0F, 0x1, 0x1);
	regmap_update_bits(aw85601->regmap, 0x10, 0x1, 0x1);
	regmap_update_bits(aw85601->regmap, 0x11, 0x1, 0x1);

	return 0;
}

static const struct regmap_config aw85601_regmap_config = {
	.name = "aw85601",
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0x7F,
	.cache_type = REGCACHE_RBTREE,
};

/******************************************************
 *
 * i2c driver
 *
 ******************************************************/
static int aw85601_i2c_probe(struct i2c_client *i2c)
{
	struct aw85601 *aw85601 = NULL;
	int ret = 0;

	aw85601 = devm_kzalloc(&i2c->dev, sizeof(struct aw85601), GFP_KERNEL);
	if (aw85601 == NULL)
		return -ENOMEM;

	aw85601->dev = &i2c->dev;

	aw85601->mute_gpio = devm_gpiod_get_optional(&i2c->dev, "aw85601,mute", GPIOD_OUT_LOW);
	if (IS_ERR(aw85601->mute_gpio)) {
		ret = PTR_ERR(aw85601->mute_gpio);
		dev_info(&i2c->dev, "Failed to get mute gpio: %d\n", ret);
		return ret;
	}

	aw85601->enable_gpio = devm_gpiod_get_optional(&i2c->dev, "aw85601,enable", GPIOD_OUT_HIGH);
	if (IS_ERR(aw85601->enable_gpio)) {
		ret = PTR_ERR(aw85601->enable_gpio);
		dev_info(&i2c->dev, "Failed to get enable gpio: %d\n", ret);
		return ret;
	}

	aw85601->regmap = devm_regmap_init_i2c(i2c, &aw85601_regmap_config);
	if (IS_ERR(aw85601->regmap)) {
		ret = PTR_ERR(aw85601->regmap);
		dev_info(&i2c->dev, "Failed to allocate register map: %d\n", ret);
		return ret;
	}

	i2c_set_clientdata(i2c, aw85601);

	if (aw85601->enable_gpio)
		gpiod_set_value(aw85601->enable_gpio, 0);

	/* aw85601 chip id */
	ret = aw85601_read_chipid(aw85601);
	if (ret < 0)
		return ret;

	/*aw pa init*/
	ret = aw85601_init(aw85601);
	if (ret < 0)
		return ret;

	aw85601_start(aw85601);

	if (aw85601->mute_gpio)
		gpiod_set_value(aw85601->mute_gpio, 0);

	dev_info(&i2c->dev, "probe done");

	return 0;

}

void aw85601_i2c_remove(struct i2c_client *i2c)
{
	struct aw85601 *aw85601 = i2c_get_clientdata(i2c);

	dev_info(aw85601->dev, "enter");

	aw85601_stop(aw85601);

	gpiod_set_value(aw85601->mute_gpio, 1);
	gpiod_set_value(aw85601->enable_gpio, 1);
}

#ifdef CONFIG_PM
static int aw85601_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct aw85601 *aw85601 = i2c_get_clientdata(client);

	dev_info(aw85601->dev, "enter");

	aw85601_start(aw85601);

	return 0;
}

static int aw85601_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct aw85601 *aw85601 = i2c_get_clientdata(client);

	dev_info(aw85601->dev, "enter");

	aw85601_stop(aw85601);

	return 0;
}

static const struct dev_pm_ops aw85601_dev_pm_ops = {
	.suspend = aw85601_i2c_suspend,
	.resume  = aw85601_i2c_resume,
};
#endif

static const struct i2c_device_id aw85601_i2c_id[] = {
	{"aw85601", 0},
	{}
};

static const struct of_device_id aw85601_dt_match[] = {
	{.compatible = "awinic,aw85601"},
	{},
};

static struct i2c_driver aw85601_i2c_driver = {
	.driver = {
		.name = "aw85601",
		.of_match_table = of_match_ptr(aw85601_dt_match),
#ifdef CONFIG_PM
		.pm = &aw85601_dev_pm_ops,
#endif
	},

	.probe = aw85601_i2c_probe,
	.remove = aw85601_i2c_remove,
	.id_table = aw85601_i2c_id,
};
module_i2c_driver(aw85601_i2c_driver);

MODULE_DESCRIPTION("ASoC AW85601 PA Driver");
MODULE_LICENSE("GPL");
