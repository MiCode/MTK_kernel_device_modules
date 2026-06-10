// SPDX-License-Identifier: GPL-2.0
//
/*
 * Copyright (C) 2024 MediaTek Inc.
 * Author: Light Hsieh <light.hsieh@mediatek.com>
 *
 */

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include "pinctrl-mtk-common-v2.h"
#include "pinctrl-paris.h"

#define MTK_SIMPLE_PIN(_number, _name, ...) {		\
		.number = _number,			\
		.name = _name,				\
		.funcs = (struct mtk_func_desc[]){	\
			__VA_ARGS__, { } },		\
	}

static const struct mtk_pin_desc mtk_pins_mt6667[] = {
	MTK_SIMPLE_PIN(0, "DUMMY0", MTK_FUNCTION(0, NULL)),
	MTK_SIMPLE_PIN(1, "GPIO1", MTK_FUNCTION(0, "GPIO1")),
	MTK_SIMPLE_PIN(2, "GPIO2", MTK_FUNCTION(0, "GPIO2")),
	MTK_SIMPLE_PIN(3, "GPIO3", MTK_FUNCTION(0, "GPIO3")),
	MTK_SIMPLE_PIN(4, "GPIO4", MTK_FUNCTION(0, "GPIO4")),
};

#define PIN_SIMPLE_FIELD_BASE(pin, addr, bit, width) {		\
		.s_pin = pin,					\
		.s_addr = addr,					\
		.s_bit = bit,					\
		.x_bits = width					\
	}

static const struct mtk_pin_field_calc mt6667_pin_smt_range[] = {
	PIN_SIMPLE_FIELD_BASE(1, 0xa4, 0, 1),
	PIN_SIMPLE_FIELD_BASE(2, 0xa4, 1, 1),
	PIN_SIMPLE_FIELD_BASE(3, 0xa4, 2, 1),
	PIN_SIMPLE_FIELD_BASE(4, 0xa4, 3, 1),
};

static const struct mtk_pin_field_calc mt6667_pin_drv_range[] = {
	PIN_SIMPLE_FIELD_BASE(1, 0xa7, 0, 2),
	PIN_SIMPLE_FIELD_BASE(2, 0xa7, 4, 2),
	PIN_SIMPLE_FIELD_BASE(3, 0xa8, 0, 2),
	PIN_SIMPLE_FIELD_BASE(4, 0xa8, 4, 2),
};

static const struct mtk_pin_field_calc mt6667_pin_dir_range[] = {
	PIN_SIMPLE_FIELD_BASE(1, 0x88, 0, 1),
	PIN_SIMPLE_FIELD_BASE(2, 0x88, 1, 1),
	PIN_SIMPLE_FIELD_BASE(3, 0x88, 2, 1),
	PIN_SIMPLE_FIELD_BASE(4, 0x88, 3, 1),
};

static const struct mtk_pin_field_calc mt6667_pin_pullen_range[] = {
	PIN_SIMPLE_FIELD_BASE(1, 0x8b, 0, 1),
	PIN_SIMPLE_FIELD_BASE(2, 0x8b, 1, 1),
	PIN_SIMPLE_FIELD_BASE(3, 0x8b, 2, 1),
	PIN_SIMPLE_FIELD_BASE(4, 0x8b, 3, 1),
};

static const struct mtk_pin_field_calc mt6667_pin_pullsel_range[] = {
	PIN_SIMPLE_FIELD_BASE(1, 0x8e, 0, 1),
	PIN_SIMPLE_FIELD_BASE(2, 0x8e, 1, 1),
	PIN_SIMPLE_FIELD_BASE(3, 0x8e, 2, 1),
	PIN_SIMPLE_FIELD_BASE(4, 0x8e, 3, 1),
};

static const struct mtk_pin_field_calc mt6667_pin_do_range[] = {
	PIN_SIMPLE_FIELD_BASE(1, 0x94, 0, 1),
	PIN_SIMPLE_FIELD_BASE(2, 0x94, 1, 1),
	PIN_SIMPLE_FIELD_BASE(3, 0x94, 2, 1),
	PIN_SIMPLE_FIELD_BASE(4, 0x94, 3, 1),
};

static const struct mtk_pin_field_calc mt6667_pin_di_range[] = {
	PIN_SIMPLE_FIELD_BASE(1, 0x97, 0, 1),
	PIN_SIMPLE_FIELD_BASE(2, 0x97, 1, 1),
	PIN_SIMPLE_FIELD_BASE(3, 0x97, 2, 1),
	PIN_SIMPLE_FIELD_BASE(4, 0x97, 3, 1),
};

static const struct mtk_pin_field_calc mt6667_pin_mode_range[] = {
	PIN_SIMPLE_FIELD_BASE(1, 0x99, 0, 3),
	PIN_SIMPLE_FIELD_BASE(2, 0x99, 3, 3),
	PIN_SIMPLE_FIELD_BASE(3, 0x9c, 0, 3),
	PIN_SIMPLE_FIELD_BASE(4, 0x9c, 3, 3),
};

#define MTK_RANGE(_a)		{ .range = (_a), .nranges = ARRAY_SIZE(_a), }

static const struct mtk_pin_reg_calc mt6667_reg_cals[PINCTRL_PIN_REG_MAX] = {
	[PINCTRL_PIN_REG_MODE] = MTK_RANGE(mt6667_pin_mode_range),
	[PINCTRL_PIN_REG_DIR] = MTK_RANGE(mt6667_pin_dir_range),
	[PINCTRL_PIN_REG_DI] = MTK_RANGE(mt6667_pin_di_range),
	[PINCTRL_PIN_REG_DO] = MTK_RANGE(mt6667_pin_do_range),
	[PINCTRL_PIN_REG_SMT] = MTK_RANGE(mt6667_pin_smt_range),
	[PINCTRL_PIN_REG_PULLEN] = MTK_RANGE(mt6667_pin_pullen_range),
	[PINCTRL_PIN_REG_PULLSEL] = MTK_RANGE(mt6667_pin_pullsel_range),
	[PINCTRL_PIN_REG_DRV] = MTK_RANGE(mt6667_pin_drv_range),
};

static int mt6667_lock(struct mtk_pinctrl *hw, int field, int enable)
{
	struct regmap *pinctrl_regmap;
	int err = 0;

	pinctrl_regmap = (struct regmap *)hw->base[0];
	if (field == PINCTRL_PIN_REG_SMT) {
		if (!enable) {
			err = regmap_write(pinctrl_regmap, 0x3b4, 0x98);
			if (err)
				return err;

			err = regmap_write(pinctrl_regmap, 0x3b5, 0x99);
		} else {
			err = regmap_write(pinctrl_regmap, 0x3b4, 0);
			if (err)
				return err;

			err = regmap_write(pinctrl_regmap, 0x3b5, 0);
		}
	}

	return err;
}

static const struct mtk_pin_soc mt6667_data = {
	.reg_cal = mt6667_reg_cals,
	.pins = mtk_pins_mt6667,
	.npins = 5,
	.ngrps = 5,
	.nfuncs = 1,
	.gpio_m = 0,
	.real_pin_start_idx = 1,
	.capability_flags = FLAG_MT66XX,
	.field_lock_flags[0] = 1 << PINCTRL_PIN_REG_SMT,
	.field_lock_ops = mt6667_lock,
};

static int mt6667_pinctrl_probe(struct platform_device *pdev)
{
	return mt63xx_pinctrl_probe(pdev, &mt6667_data);
}

static const struct of_device_id mt6667_pinctrl_of_match[] = {
	{ .compatible = "mediatek,mt6667-7-pinctrl", },
	{ .compatible = "mediatek,mt6667-8-pinctrl", },
	{ .compatible = "mediatek,mt6667-12-pinctrl", },
	{ .compatible = "mediatek,mt6667-13-pinctrl", },
	{ }
};

static struct platform_driver mt6667_pinctrl_driver = {
	.driver = {
		.name = "mt6667-pinctrl",
		.of_match_table = mt6667_pinctrl_of_match,
	},
	.probe = mt6667_pinctrl_probe,
};
module_platform_driver(mt6667_pinctrl_driver);

MODULE_AUTHOR("Light Hsieh <light.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MT6667 Pinctrl driver");
MODULE_LICENSE("GPL v2");
