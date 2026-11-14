// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 MediaTek Inc.
 *
 * Author: KY Liu <ky.liu@mediatek.com>
 */

#include <linux/kernel.h>
#include <linux/mfd/core.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/spmi.h>

static const struct mfd_cell mt6687_devs[] = {
	{
		.name = "mt6687-clkbuf",
		.of_compatible = "mediatek,mt6687-clkbuf",
	}, {
		.name = "mt6687-consys",
		.of_compatible = "mediatek,mt6687-consys",
	}, {
		.name = "mt6687-connv3",
		.of_compatible = "mediatek,mt6687-connv3",
	}, {
		.name = "mt6687-rtc",
		.of_compatible = "mediatek,mt6687-rtc",
	}, {
		.name = "mt6687-audclk",
		.of_compatible = "mediatek,mt6687-audclk",
	}, {
		.name = "mt6687-gps",
		.of_compatible = "mediatek,mt6687-gps",
	},
};

static const struct regmap_config spmi_regmap_config = {
	.reg_bits	= 16,
	.val_bits	= 8,
	.max_register	= 0x2000,
	.fast_io	= true,
	.use_single_read = true,
	.use_single_write = true
};

static int mt6687_spmi_probe(struct spmi_device *sdev)
{
	int ret;
	struct regmap *regmap;

	regmap = devm_regmap_init_spmi_ext(sdev, &spmi_regmap_config);
	if (IS_ERR(regmap)) {
		pr_info("Failed to init mt6687 regmap: %ld\n", PTR_ERR(regmap));
		return PTR_ERR(regmap);
	}

	ret = devm_mfd_add_devices(&sdev->dev, -1, mt6687_devs,
				   ARRAY_SIZE(mt6687_devs), NULL, 0, NULL);
	if (ret) {
		pr_info("Failed to add child devices: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct of_device_id mt6687_id_table[] = {
	{ .compatible = "mediatek,mt6687", },
	{ }
};
MODULE_DEVICE_TABLE(of, mt6687_id_table);

static struct spmi_driver mt6687_spmi_driver = {
	.probe = mt6687_spmi_probe,
	.driver = {
		.name = "mt6687",
		.of_match_table = mt6687_id_table,
	},
};
module_spmi_driver(mt6687_spmi_driver);

MODULE_DESCRIPTION("Mediatek SPMI MT6687 Clock IC driver");
MODULE_AUTHOR("KY Liu <ky.liu@mediatek.com>");
MODULE_LICENSE("GPL");
