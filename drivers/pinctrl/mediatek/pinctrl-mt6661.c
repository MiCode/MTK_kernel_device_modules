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

static const struct mtk_pin_desc mtk_pins_mt6661[] = {
	MTK_SIMPLE_PIN(0, "DUMMY0", MTK_FUNCTION(0, NULL)),
	MTK_SIMPLE_PIN(1, "DUMMY1", MTK_FUNCTION(0, NULL)),
	MTK_SIMPLE_PIN(2, "DUMMY2", MTK_FUNCTION(0, NULL)),
	MTK_SIMPLE_PIN(3, "GPIO3", MTK_FUNCTION(0, "GPIO3")),
	MTK_SIMPLE_PIN(4, "GPIO4", MTK_FUNCTION(0, "GPIO4")),
	MTK_SIMPLE_PIN(5, "GPIO5", MTK_FUNCTION(0, "GPIO5")),
	MTK_SIMPLE_PIN(6, "GPIO6", MTK_FUNCTION(0, "GPIO6")),
	MTK_SIMPLE_PIN(7, "GPIO7", MTK_FUNCTION(0, "GPIO7")),
	MTK_SIMPLE_PIN(8, "GPIO8", MTK_FUNCTION(0, "GPIO8")),
	MTK_SIMPLE_PIN(9, "GPIO9", MTK_FUNCTION(0, "GPIO9")),
	MTK_SIMPLE_PIN(10, "GPIO10", MTK_FUNCTION(0, "GPIO10")),
	MTK_SIMPLE_PIN(11, "GPIO11", MTK_FUNCTION(0, "GPIO11")),
};

#define PIN_SIMPLE_FIELD_BASE(pin, addr, bit, width) {		\
		.s_pin = pin,					\
		.s_addr = addr,					\
		.s_bit = bit,					\
		.x_bits = width					\
	}

static const struct mtk_pin_field_calc mt6661_pin_smt_range[] = {
	PIN_SIMPLE_FIELD_BASE(3, 0xc4, 7, 1),
	PIN_SIMPLE_FIELD_BASE(4, 0xc5, 0, 1),
	PIN_SIMPLE_FIELD_BASE(5, 0xc5, 1, 1),
	PIN_SIMPLE_FIELD_BASE(6, 0xc5, 2, 1),
	PIN_SIMPLE_FIELD_BASE(7, 0xc5, 3, 1),
	PIN_SIMPLE_FIELD_BASE(8, 0xc5, 4, 1),
	PIN_SIMPLE_FIELD_BASE(9, 0xc5, 5, 1),
	PIN_SIMPLE_FIELD_BASE(10, 0xc5, 6, 1),
	PIN_SIMPLE_FIELD_BASE(11, 0xc5, 7, 1),
};

static const struct mtk_pin_field_calc mt6661_pin_drv_range[] = {
	PIN_SIMPLE_FIELD_BASE(3, 0xca, 4, 2),
	PIN_SIMPLE_FIELD_BASE(4, 0xcb, 0, 2),
	PIN_SIMPLE_FIELD_BASE(5, 0xcb, 4, 2),
	PIN_SIMPLE_FIELD_BASE(6, 0xcc, 0, 2),
	PIN_SIMPLE_FIELD_BASE(7, 0xcc, 4, 2),
	PIN_SIMPLE_FIELD_BASE(8, 0xcd, 0, 2),
	PIN_SIMPLE_FIELD_BASE(9, 0xcd, 4, 2),
	PIN_SIMPLE_FIELD_BASE(10, 0xce, 0, 2),
	PIN_SIMPLE_FIELD_BASE(11, 0xce, 4, 2),
};

static const struct mtk_pin_field_calc mt6661_pin_dir_range[] = {
	PIN_SIMPLE_FIELD_BASE(3, 0x88, 3, 1),
	PIN_SIMPLE_FIELD_BASE(4, 0x88, 4, 1),
	PIN_SIMPLE_FIELD_BASE(5, 0x88, 5, 1),
	PIN_SIMPLE_FIELD_BASE(6, 0x88, 6, 1),
	PIN_SIMPLE_FIELD_BASE(7, 0x88, 7, 1),
	PIN_SIMPLE_FIELD_BASE(8, 0x8b, 0, 1),
	PIN_SIMPLE_FIELD_BASE(9, 0x8b, 1, 1),
	PIN_SIMPLE_FIELD_BASE(10, 0x8b, 2, 1),
	PIN_SIMPLE_FIELD_BASE(11, 0x8b, 3, 1),
};

static const struct mtk_pin_field_calc mt6661_pin_pullen_range[] = {
	PIN_SIMPLE_FIELD_BASE(3, 0x8e, 3, 1),
	PIN_SIMPLE_FIELD_BASE(4, 0x8e, 4, 1),
	PIN_SIMPLE_FIELD_BASE(5, 0x8e, 5, 1),
	PIN_SIMPLE_FIELD_BASE(6, 0x8e, 6, 1),
	PIN_SIMPLE_FIELD_BASE(7, 0x8e, 7, 1),
	PIN_SIMPLE_FIELD_BASE(8, 0x91, 0, 1),
	PIN_SIMPLE_FIELD_BASE(9, 0x91, 1, 1),
	PIN_SIMPLE_FIELD_BASE(10, 0x91, 2, 1),
	PIN_SIMPLE_FIELD_BASE(11, 0x91, 3, 1),
};

static const struct mtk_pin_field_calc mt6661_pin_pullsel_range[] = {
	PIN_SIMPLE_FIELD_BASE(3, 0x94, 3, 1),
	PIN_SIMPLE_FIELD_BASE(4, 0x94, 4, 1),
	PIN_SIMPLE_FIELD_BASE(5, 0x94, 5, 1),
	PIN_SIMPLE_FIELD_BASE(6, 0x94, 6, 1),
	PIN_SIMPLE_FIELD_BASE(7, 0x94, 7, 1),
	PIN_SIMPLE_FIELD_BASE(8, 0x97, 0, 1),
	PIN_SIMPLE_FIELD_BASE(9, 0x97, 1, 1),
	PIN_SIMPLE_FIELD_BASE(10, 0x97, 2, 1),
	PIN_SIMPLE_FIELD_BASE(11, 0x97, 3, 1),
};

static const struct mtk_pin_field_calc mt6661_pin_do_range[] = {
	PIN_SIMPLE_FIELD_BASE(3, 0xa0, 3, 1),
	PIN_SIMPLE_FIELD_BASE(4, 0xa0, 4, 1),
	PIN_SIMPLE_FIELD_BASE(5, 0xa0, 5, 1),
	PIN_SIMPLE_FIELD_BASE(6, 0xa0, 6, 1),
	PIN_SIMPLE_FIELD_BASE(7, 0xa0, 7, 1),
	PIN_SIMPLE_FIELD_BASE(8, 0xa3, 0, 1),
	PIN_SIMPLE_FIELD_BASE(9, 0xa3, 1, 1),
	PIN_SIMPLE_FIELD_BASE(10, 0xa3, 2, 1),
	PIN_SIMPLE_FIELD_BASE(11, 0xa3, 3, 1),
};

static const struct mtk_pin_field_calc mt6661_pin_di_range[] = {
	PIN_SIMPLE_FIELD_BASE(3, 0xa6, 1, 1),
	PIN_SIMPLE_FIELD_BASE(4, 0xa6, 2, 1),
	PIN_SIMPLE_FIELD_BASE(5, 0xa6, 3, 1),
	PIN_SIMPLE_FIELD_BASE(6, 0xa6, 4, 1),
	PIN_SIMPLE_FIELD_BASE(7, 0xa6, 5, 1),
	PIN_SIMPLE_FIELD_BASE(8, 0xa7, 0, 1),
	PIN_SIMPLE_FIELD_BASE(9, 0xa7, 1, 1),
	PIN_SIMPLE_FIELD_BASE(10, 0xa7, 2, 1),
	PIN_SIMPLE_FIELD_BASE(11, 0xa7, 3, 1),
};

static const struct mtk_pin_field_calc mt6661_pin_mode_range[] = {
	PIN_SIMPLE_FIELD_BASE(3, 0xad, 3, 3),
	PIN_SIMPLE_FIELD_BASE(4, 0xb0, 0, 3),
	PIN_SIMPLE_FIELD_BASE(5, 0xb0, 3, 3),
	PIN_SIMPLE_FIELD_BASE(6, 0xb3, 0, 3),
	PIN_SIMPLE_FIELD_BASE(7, 0xb3, 3, 3),
	PIN_SIMPLE_FIELD_BASE(8, 0xb6, 0, 3),
	PIN_SIMPLE_FIELD_BASE(9, 0xb6, 3, 3),
	PIN_SIMPLE_FIELD_BASE(10, 0xb9, 0, 3),
	PIN_SIMPLE_FIELD_BASE(11, 0xb9, 3, 3),
};

static const struct mtk_pin_field_calc mt6661_pin_ad_sw_switch_range[] = {
	PIN_SIMPLE_FIELD_BASE(8, 0xd5, 0, 1),
	PIN_SIMPLE_FIELD_BASE(9, 0xd5, 1, 1),
	PIN_SIMPLE_FIELD_BASE(10, 0xd5, 2, 1),
	PIN_SIMPLE_FIELD_BASE(11, 0xd5, 3, 1),
};

static const struct mtk_pin_field_calc mt6661_pin_ad_switch_range[] = {
	PIN_SIMPLE_FIELD_BASE(8, 0xd6, 0, 1),
	PIN_SIMPLE_FIELD_BASE(9, 0xd6, 1, 1),
	PIN_SIMPLE_FIELD_BASE(10, 0xd6, 2, 1),
	PIN_SIMPLE_FIELD_BASE(11, 0xd6, 3, 1),
};

#define MTK_RANGE(_a)		{ .range = (_a), .nranges = ARRAY_SIZE(_a), }

static const struct mtk_pin_reg_calc mt6661_reg_cals[PINCTRL_PIN_REG_MAX] = {
	[PINCTRL_PIN_REG_MODE] = MTK_RANGE(mt6661_pin_mode_range),
	[PINCTRL_PIN_REG_DIR] = MTK_RANGE(mt6661_pin_dir_range),
	[PINCTRL_PIN_REG_DI] = MTK_RANGE(mt6661_pin_di_range),
	[PINCTRL_PIN_REG_DO] = MTK_RANGE(mt6661_pin_do_range),
	[PINCTRL_PIN_REG_SMT] = MTK_RANGE(mt6661_pin_smt_range),
	[PINCTRL_PIN_REG_PULLEN] = MTK_RANGE(mt6661_pin_pullen_range),
	[PINCTRL_PIN_REG_PULLSEL] = MTK_RANGE(mt6661_pin_pullsel_range),
	[PINCTRL_PIN_REG_DRV] = MTK_RANGE(mt6661_pin_drv_range),
	[PINCTRL_PIN_REG_AD_SWITCH] = MTK_RANGE(mt6661_pin_ad_switch_range),
	[PINCTRL_PIN_REG_AD_SW_SWITCH] = MTK_RANGE(mt6661_pin_ad_sw_switch_range),
};

static int mt6661_lock(struct mtk_pinctrl *hw, int field, int enable)
{
	struct regmap *pinctrl_regmap;
	int err = 0;

	pinctrl_regmap = (struct regmap *)hw->base[0];
	if (field == PINCTRL_PIN_REG_SMT) {
		if (!enable) {
			err = regmap_write(pinctrl_regmap, 0x3b4, 0x9e);
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

static const struct mtk_pin_soc mt6661_data = {
	.reg_cal = mt6661_reg_cals,
	.pins = mtk_pins_mt6661,
	.npins = 12,
	.ngrps = 12,
	.nfuncs = 1,
	.gpio_m = 0,
	.real_pin_start_idx = 3,
	.capability_flags = FLAG_MT66XX | FLAG_NEED_AD_SWITCH,
	.field_lock_flags[0] = 1 << PINCTRL_PIN_REG_SMT,
	.field_lock_ops = mt6661_lock,
};

static int mt6661_pinctrl_probe(struct platform_device *pdev)
{
	return mt63xx_pinctrl_probe(pdev, &mt6661_data);
}

static const struct of_device_id mt6661_pinctrl_of_match[] = {
	{ .compatible = "mediatek,mt6661-3-pinctrl", },
	{ .compatible = "mediatek,mt6661-4-pinctrl", },
	{ .compatible = "mediatek,mt6661-5-pinctrl", },
	{ .compatible = "mediatek,mt6661-6-pinctrl", },
	{ }
};

static struct platform_driver mt6661_pinctrl_driver = {
	.driver = {
		.name = "mt6661-pinctrl",
		.of_match_table = mt6661_pinctrl_of_match,
	},
	.probe = mt6661_pinctrl_probe,
};
module_platform_driver(mt6661_pinctrl_driver);

MODULE_AUTHOR("Light Hsieh <light.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MT6661 Pinctrl driver");
MODULE_LICENSE("GPL v2");
