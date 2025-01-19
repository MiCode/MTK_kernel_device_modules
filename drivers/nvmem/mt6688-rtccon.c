// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2024 Mediatek Inc.
// Copyright (C) 2024 Richtek Technology Corp.
// Author: ChiYuan Huang <cy_huang@richtek.com>

#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/nvmem-provider.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#define MT6688_REG_TOP_FLAG1	0x10
#define MT6688_REG_SPAR_CON1	0x701

#define MT6688_RTC_SPAR_EVT	GENMASK(3, 2)

enum {
	MT6688_CELL_RTCEVT,
	MT6688_CELL_SPARCON1,
	MT6688_MAX_CELL,
};

struct mt6688_rtccon_data {
	struct device *dev;
	struct regmap *regmap;
	struct nvmem_config cfg;
};

static int mt6688_rtccon_read(void *priv, unsigned int offset, void *val, size_t bytes)
{
	struct mt6688_rtccon_data *data = priv;
	unsigned int i, vidx;
	int ret;

	if (offset >= MT6688_MAX_CELL)
		return -EINVAL;

	for (i = offset, vidx = 0; vidx < bytes && i < MT6688_MAX_CELL; i++, vidx++) {
		switch (i) {
		case MT6688_CELL_RTCEVT:
			ret = regmap_raw_read(data->regmap, MT6688_REG_TOP_FLAG1, val + vidx, 1);
			break;
		case MT6688_CELL_SPARCON1:
			ret = regmap_raw_read(data->regmap, MT6688_REG_SPAR_CON1, val + vidx, 1);
			break;
		default:
			break;
		}

		if (ret) {
			dev_err(data->dev, "Failed to read cell %d\n", i);
			return ret;
		}
	}

	return 0;
}

static int mt6688_rtccon_write(void *priv, unsigned int offset, void *val, size_t bytes)
{
	struct mt6688_rtccon_data *data = priv;
	unsigned int i, vidx, wval;
	int ret;

	if (offset >= MT6688_MAX_CELL)
		return -EINVAL;

	for (i = offset, vidx = 0; vidx < bytes && i < MT6688_MAX_CELL; i++, vidx++) {
		switch (i) {
		case MT6688_CELL_RTCEVT:
			/* This cell only support to wrclear RTCSPAR evt */
			wval = *(u8 *)(val + vidx);
			wval &= MT6688_RTC_SPAR_EVT;
			ret = regmap_write(data->regmap, MT6688_REG_TOP_FLAG1, wval);
			break;
		case MT6688_CELL_SPARCON1:
			ret = regmap_raw_write(data->regmap, MT6688_REG_SPAR_CON1, val + vidx, 1);
			break;
		default:
			break;
		}

		if (ret) {
			dev_err(data->dev, "Failed to write cell %d\n", i);
			return ret;
		}
	}

	return 0;
}

static int mt6688_rtccon_probe(struct platform_device *pdev)
{
	struct mt6688_rtccon_data *data;
	struct device *dev = &pdev->dev;
	struct nvmem_device *nvmem;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->dev = dev;

	data->regmap = dev_get_regmap(dev->parent, NULL);
	if (!data->regmap)
		return dev_err_probe(dev, -EINVAL, "Failed to init regmap\n");

	data->cfg.dev = dev;
	data->cfg.size = MT6688_MAX_CELL;
	data->cfg.priv = data;
	data->cfg.ignore_wp = true;
	data->cfg.owner = THIS_MODULE;
	data->cfg.name = dev_name(dev);
	data->cfg.id = NVMEM_DEVID_NONE;
	data->cfg.reg_read = mt6688_rtccon_read;
	data->cfg.reg_write = mt6688_rtccon_write;
	data->cfg.add_legacy_fixed_of_cells = true;

	nvmem = devm_nvmem_register(dev, &data->cfg);
	if (IS_ERR(nvmem))
		return dev_err_probe(dev, PTR_ERR(nvmem), "Failed to register nvmem device\n");

	return 0;
}

static const struct of_device_id mt6688_rtccon_dev_match_table[] = {
	{ .compatible = "mediatek,mt6688-rtccon" },
	{}
};
MODULE_DEVICE_TABLE(of, mt6688_rtccon_dev_match_table);

static struct platform_driver mt6688_rtccon_driver = {
	.driver = {
		.name = "mt6688-rtccon",
		.of_match_table = mt6688_rtccon_dev_match_table,
	},
	.probe = mt6688_rtccon_probe,
};
module_platform_driver(mt6688_rtccon_driver);

MODULE_AUTHOR("ChiYuan Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("Mediatek MT6688 RTCCON nvmem driver");
MODULE_LICENSE("GPL");
