// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include "dvfsrc-mb.h"

static struct mtk_dvfsrc_mb *dvfsrc_drv;

static inline u32 dvfsrc_read(struct mtk_dvfsrc_mb *dvfs, u32 offset)
{
	return readl(dvfs->regs + offset);
}

void dvfsrc_mt6989_get_data(struct mtk_dvfsrc_header *header)
{
	header->module_id = dvfsrc_drv->dvd->module_id;
	header->data_offset = dvfsrc_drv->dvd->data_offset;
	header->data_length = dvfsrc_drv->dvd->data_length;
	header->version = dvfsrc_drv->dvd->max_ddr_info_ver;

	header->data[0] = dvfsrc_read(dvfsrc_drv, DVFSRC_RSV_1);
	header->data[1] = dvfsrc_read(dvfsrc_drv, DVFSRC_RSV_2);
	header->data[2] = dvfsrc_read(dvfsrc_drv, DVFSRC_RSV_3);
	header->data[3] = dvfsrc_read(dvfsrc_drv, DVFSRC_RSV_4);
	header->data[4] = dvfsrc_read(dvfsrc_drv, DVFSRC_RSV_5);
	header->data[5] = dvfsrc_read(dvfsrc_drv, DVFSRC_RSV_6);

	dev_info(dvfsrc_drv->dev, "%s %d-%d-%d-%d\n", __func__,
		header->module_id, header->version,
		header->data_offset,header->data_length);

	dev_info(dvfsrc_drv->dev, "%s %x-%x-%x-%x-%x-%x\n", __func__,
		header->data[0], header->data[1], header->data[2],
		header->data[3], header->data[4], header->data[5]);
}

void dvfsrc_get_data(struct mtk_dvfsrc_header *header)
{
	if (dvfsrc_drv == NULL) {
		header->data_length = 0;
		return;
	}

	dvfsrc_drv->dvd->config->get_data(header);
}
EXPORT_SYMBOL(dvfsrc_get_data);

static const uint32_t mt6991_regs[] = {
	[SW_REQ1] = 0x010,
	[SW_REQ2] = 0x014,
	[SW_REQ3] = 0x018,
	[SW_REQ4] = 0x01c,
	[SW_REQ5] = 0x020,
	[SW_REQ7] = 0x028,
	[SW_REQ10] = 0x5FC,
	[MD_DDQ] = 0x5E4,
	[DDR_QOS] = 0x5E8,
	[DBG_STA0] = 0x29C,
	[DBG_STA1] = 0x2A0,
	[DBG_STA2] = 0x2A4,
	[DBG_STA3] = 0x2A8,
	[DBG_STA4] = 0x2AC,
	[DBG_STA5] = 0x2B0,
	[DBG_STA6] = 0x2B4,
	[DBG_STA7] = 0x2B8,
	[DBG_STA8] = 0x2BC,
	[DBG_STA9] = 0x2C0,
	[DBG_STA10] = 0x2C4,
	[SW_BW0] = 0x1DC,
	[SW_BW1] = 0x1E0,
	[SW_BW2] = 0x1E4,
	[SW_BW3] = 0x1E8,
	[SW_BW4] = 0x1EC,
	[SW_BW5] = 0x1F0,
	[SW_BW6] = 0x1F4,
	[SW_BW7] = 0x1F8,
	[SW_BW8] = 0x1FC,
	[SW_BW9] = 0x200,
};

void dvfsrc_read_dvfs_info_reg(struct mtk_dvfsrc_dvfs_info_header *dvfs_info_header)
{
	uint32_t i = 0;

	dvfs_info_header->dvfs_info_version = dvfsrc_drv->dvd->dvfs_info_ver;

	for (i = 0; i < DVFS_INFO_REG_NUM; i++)
		dvfs_info_header->dvfs_info_val[i] =
			dvfsrc_read(dvfsrc_drv, dvfsrc_drv->dvd->dvfs_info_regs[i]);
}

void dvfsrc_get_dvfs_info(struct mtk_dvfsrc_dvfs_info_header *dvfs_info_header)
{
	if (dvfsrc_drv && dvfsrc_drv->dvd && dvfsrc_drv->dvd->config)
		dvfsrc_drv->dvd->config->get_dvfs_info(dvfs_info_header);
	else
		dev_info(dvfsrc_drv->dev, "%s not support\n", __func__);
}
EXPORT_SYMBOL(dvfsrc_get_dvfs_info);

const struct mtk_dvfsrc_config mt6989_config = {
	.get_data = &dvfsrc_mt6989_get_data,
};

const struct mtk_dvfsrc_config mt6991_config = {
	.get_data = &dvfsrc_mt6989_get_data,
	.get_dvfs_info = &dvfsrc_read_dvfs_info_reg,
};

const struct mtk_dvfsrc_config mt6899_config = {
	.get_data = &dvfsrc_mt6989_get_data,
};

static const struct mtk_dvfsrc_data mt6989_data = {
	.module_id = 2,
	.data_offset = 0,
	.data_length = sizeof(struct mtk_dvfsrc_header),
	.max_ddr_info_ver = 0,
	.config = &mt6989_config,
};

static const struct mtk_dvfsrc_data mt6991_data = {
	.module_id = 2,
	.data_offset = 0,
	.data_length = sizeof(struct mtk_dvfsrc_header),
	.max_ddr_info_ver = 2,
	.dvfs_info_ver = 0x6991,
	.dvfs_info_regs = mt6991_regs,
	.config = &mt6991_config,
};

static const struct mtk_dvfsrc_data mt6899_data = {
	.module_id = 2,
	.data_offset = 0,
	.data_length = sizeof(struct mtk_dvfsrc_header),
	.max_ddr_info_ver = 2,
	.config = &mt6899_config,
};


static const struct of_device_id dvfsrc_mdv_of_match[] = {
	{
		.compatible = "mediatek,mt6989-dvfsrc",
		.data = &mt6989_data,
	}, {
		.compatible = "mediatek,mt6991-dvfsrc",
		.data = &mt6991_data,
	}, {
		.compatible = "mediatek,mt6899-dvfsrc",
		.data = &mt6899_data,
	}, {
		/* sentinel */
	},
};

static int dvfsrc_mb_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct device *dev = &pdev->dev;
	struct platform_device *parent_dev;
	struct resource *res;
	struct mtk_dvfsrc_mb *dvfsrc;

	dvfsrc = devm_kzalloc(&pdev->dev, sizeof(*dvfsrc), GFP_KERNEL);
	if (!dvfsrc)
		return -ENOMEM;

	dvfsrc->dev = &pdev->dev;

	parent_dev = to_platform_device(dev->parent);

	res = platform_get_resource_byname(parent_dev,
			IORESOURCE_MEM, "dvfsrc");
	if (!res) {
		dev_info(dev, "dvfsrc debug resource not found\n");
		return -ENODEV;
	}

	dvfsrc->regs = devm_ioremap(&pdev->dev, res->start,
		resource_size(res));
	if (IS_ERR(dvfsrc->regs))
		return PTR_ERR(dvfsrc->regs);

	platform_set_drvdata(pdev, dvfsrc);

	match = of_match_node(dvfsrc_mdv_of_match, dev->parent->of_node);
	if (!match) {
		dvfsrc_drv = NULL;
		dev_info(dev, "invalid compatible string\n");
		return -ENODEV;
	}

	dvfsrc->dvd = match->data;
	dvfsrc_drv = dvfsrc;

	return 0;
}

static const struct of_device_id mtk_dvfsrc_mb_of_match[] = {
	{
		.compatible = "mediatek,dvfsrc-mb",
	}, {
		/* sentinel */
	},
};

static struct platform_driver mtk_dvfsrc_mb_driver = {
	.probe	= dvfsrc_mb_probe,
	.driver = {
		.name = "mtk-mb",
		.of_match_table = of_match_ptr(mtk_dvfsrc_mb_of_match),
	},
};

int __init mtk_dvfsrc_mb_init(void)
{
	return platform_driver_register(&mtk_dvfsrc_mb_driver);
}

void __exit mtk_dvfsrc_mb_exit(void)
{
	platform_driver_unregister(&mtk_dvfsrc_mb_driver);
}
