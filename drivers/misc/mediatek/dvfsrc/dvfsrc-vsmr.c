// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include "dvfsrc-vsmr.h"

static struct mtk_vsmr *vsmr_drv;

enum dvfsrc_regs {
	VSMR_GENERNAL_CON0,
	VSMR_SW_RESET,
	VSMR_TIMER_STA,
	VSMR_LEN_CON,
	VSMR_FORCE_DAT_REQ,
	VSMR_FORCE_DAT_ACK,
	VSMR_LAST_DAT_START,
	VSMR_LAST_DAT_END,
};

static const int mt6993_regs[] = {
	[VSMR_GENERNAL_CON0]  = 0x000,
	[VSMR_SW_RESET]       = 0x004,
	[VSMR_TIMER_STA]      = 0x010,
	[VSMR_LEN_CON]        = 0x014,
	[VSMR_FORCE_DAT_REQ]  = 0x018,
	[VSMR_FORCE_DAT_ACK]  = 0x01C,
	[VSMR_LAST_DAT_START] = 0x070,
	[VSMR_LAST_DAT_END]   = 0x4EC,

};

static u32 vsmr_read(struct mtk_vsmr *vsmr, u32 reg)
{
	void __iomem *addr = vsmr->regs + vsmr->dvd->config->regs[reg];

	return readl(addr);
}

static void vsmr_read_last_dat_regs(struct mtk_vsmr *vsmr, struct mtk_vsmr_header *header,u32 start, u32 end)
{
	int i = 0;
	void __iomem *addr;
	void __iomem *start_addr = vsmr->regs + vsmr->dvd->config->regs[start];
	void __iomem *end_addr = vsmr->regs + vsmr->dvd->config->regs[end];

	for (addr = start_addr; addr <= end_addr; addr += 4)
		header->last_data[i++] = readl(addr);
}

static void vsmr_write(struct mtk_vsmr *vsmr, u32 reg, u32 val)
{
	writel(val, vsmr->regs + vsmr->dvd->config->regs[reg]);
}

static void vsmr_update_bits(struct mtk_vsmr *vsmr,
						u32 reg, u32 val, u32 mask, u32 shift)
{
	u32 orig = 0;

	orig = vsmr_read(vsmr, reg);
	orig &= ~(mask << shift);
	orig |= (val << shift);
	vsmr_write(vsmr, reg, orig);
}

#define PRINT_ITEMS_PER_LINE 16

void vsmr_dump_header_data(struct mtk_vsmr_header *header)
{
	int i, j;

	pr_cont("VSMR_DBG VSMR_LEN_CON = 0x%x\n", vsmr_read(vsmr_drv, VSMR_LEN_CON));
	pr_cont("VSMR_DBG VSMR_TIMER_STA = 0x%x\n", vsmr_read(vsmr_drv, VSMR_TIMER_STA));

	for (i = 0; i < MAX_VSMR_DATA_SIZE; i += PRINT_ITEMS_PER_LINE) {
		pr_cont("VSMR_DBG 0x%04x = ",
		vsmr_drv->dvd->config->regs[VSMR_LAST_DAT_START] + 4 * i);

		for (j = 0; j < PRINT_ITEMS_PER_LINE; j++)
			pr_cont("0x%08x ", header->last_data[i + j]);

		pr_cont("\n");
	}
}

void vsmr_mt6993_get_last_data(struct mtk_vsmr_header *header)
{
	header->vsmr_support = vsmr_drv->vsmr_support;
	if (!header->vsmr_support) {
		pr_cont("VSMR not support\n");
		return;
	}

	header->module_id = vsmr_drv->dvd->module_id;
	header->data_offset = vsmr_drv->dvd->data_offset;
	header->data_length = vsmr_drv->dvd->data_length;
	header->version = vsmr_drv->dvd->version;

	/* enable VSMR */
	vsmr_update_bits(vsmr_drv, VSMR_GENERNAL_CON0, 0x1, 0x1, 0);

	/* send force data req */
	vsmr_write(vsmr_drv, VSMR_FORCE_DAT_REQ, 0x1);

	/* wait force data ack */
	while (vsmr_read(vsmr_drv, VSMR_FORCE_DAT_ACK)!= 0x1)
		;

	/* clear force data req */
	vsmr_write(vsmr_drv, VSMR_FORCE_DAT_REQ, 0x0);

	/* read last data */
	vsmr_read_last_dat_regs(vsmr_drv, header, VSMR_LAST_DAT_START, VSMR_LAST_DAT_END);

	/* read timer */
	header->timer = vsmr_read(vsmr_drv, VSMR_TIMER_STA);

	/* VSMR reset */
	vsmr_update_bits(vsmr_drv, VSMR_SW_RESET, 0x1, 0x1, 1);

	/* release VSMR reset */
	vsmr_update_bits(vsmr_drv, VSMR_SW_RESET, 0x0, 0x1, 1);

	vsmr_dump_header_data(header);
}

void vsmr_get_data(struct mtk_vsmr_header *header)
{
	if (vsmr_drv == NULL) {
		header->data_length = 0;
		return;
	}
	vsmr_drv->dvd->config->get_data(header);
}
EXPORT_SYMBOL(vsmr_get_data);

const struct mtk_dvfsrc_vsmr_config vsmr_mt6993_config = {
	.get_data = &vsmr_mt6993_get_last_data,
	.regs = mt6993_regs,
};

static const struct mtk_vsmr_data mt6993_data = {
	.module_id = 2,
	.data_offset = 0,
	.version = 1,
	.data_length = sizeof(struct mtk_vsmr_header),
	.config = &vsmr_mt6993_config,
};

static const struct of_device_id dvfsrc_vsmr_of_match[] = {
	{
		.compatible = "mediatek,mt6993-dvfsrc",
		.data = &mt6993_data,
	}, {
		/* sentinel */
	},
};

static int dvfsrc_vsmr_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct device *dev = &pdev->dev;
	struct platform_device *parent_dev;
	struct resource *res;
	struct mtk_vsmr *vsmr;

	vsmr = devm_kzalloc(&pdev->dev, sizeof(*vsmr), GFP_KERNEL);
	if (!vsmr)
		return -ENOMEM;

	vsmr->dev = &pdev->dev;

	parent_dev = to_platform_device(dev->parent);

	res = platform_get_resource_byname(parent_dev,
			IORESOURCE_MEM, "vsmr");
	if (!res) {
		dev_info(dev, "vsmr resource not found\n");
		return -ENODEV;
	}

	vsmr->regs = devm_ioremap(&pdev->dev, res->start,
		resource_size(res));
	if (IS_ERR(vsmr->regs))
		return PTR_ERR(vsmr->regs);

	platform_set_drvdata(pdev, vsmr);

	match = of_match_node(dvfsrc_vsmr_of_match, dev->parent->of_node);
	if (!match) {
		vsmr_drv = NULL;
		dev_info(dev, "invalid compatible string\n");
		return -ENODEV;
	}

	vsmr->dvd = match->data;
	vsmr_drv = vsmr;

	if (vsmr_read(vsmr_drv, VSMR_LEN_CON) == 0xDEADBEEF)
		vsmr_drv->vsmr_support = false;
	else
		vsmr_drv->vsmr_support = true;

	return 0;
}
static const struct of_device_id mtk_vsmr_of_match[] = {
	{
		.compatible = "mediatek,dvfsrc-vsmr",
	}, {
		/* sentinel */
	},
};
static struct platform_driver mtk_vsmr_driver = {
	.probe	= dvfsrc_vsmr_probe,
	.driver = {
		.name = "mtk-vsmr",
		.of_match_table = of_match_ptr(mtk_vsmr_of_match),
	},
};
int __init mtk_dvfsrc_vsmr_init(void)
{
	return platform_driver_register(&mtk_vsmr_driver);
}
void __exit mtk_dvfsrc_vsmr_exit(void)
{
	platform_driver_unregister(&mtk_vsmr_driver);
}
