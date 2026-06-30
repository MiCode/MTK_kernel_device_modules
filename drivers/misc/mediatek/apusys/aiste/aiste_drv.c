// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/regmap.h>
#include "apusys_core.h"
#include "aiste_debug.h"
#include "aiste_ioctl.h"
#include "aiste_scmi.h"
#include "aiste_qos.h"

static unsigned int g_aiste_addr;
static unsigned int g_aiste_size;
static void __iomem *g_aiste_buf_addr;

static int mtk_aiste_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *aiste_node = pdev->dev.of_node;

	ret = of_property_read_u32(aiste_node, "aiste_addr", &g_aiste_addr);
	if (ret) {
		g_aiste_addr = 0;
		aiste_err("%s: get g_aiste_addr fail\n", __func__);
	}

	ret = of_property_read_u32(aiste_node, "aiste_size", &g_aiste_size);
	if (ret) {
		g_aiste_size = 0;
		aiste_err("%s: get g_aiste_size fail\n", __func__);
	}

	if ((g_aiste_addr > 0) && (g_aiste_size > 0)) {
		is_aiste_supported = true;
		g_aiste_buf_addr = ioremap_wc((phys_addr_t)g_aiste_addr, g_aiste_size);
		pr_info("aiste addr=0x%x, size=0x%x, buf_addr=0x%lx, %s\n",
			g_aiste_addr, g_aiste_size, (unsigned long)g_aiste_buf_addr, __func__);
	} else {
		is_aiste_supported = false;
	}
	dev_info(&pdev->dev, "%s probed\n", __func__);
	return 0;
}

static const struct of_device_id mtk_aiste_of_match[] = {
	{ .compatible = "mediatek,aiste"},
	{},
};

static struct platform_driver mtk_aiste_driver = {
	.probe = mtk_aiste_probe,
	.driver	= {
		.name = "mtk-aiste",
		.of_match_table = mtk_aiste_of_match,
	},
};

int aiste_init(struct apusys_core_info *info)
{
	int ret = 0;

	aiste_drv_debug("%s register misc...\n", __func__);
	ret = misc_register(aiste_get_misc_dev());
	if (ret) {
		aiste_err("%s: failed to register aiste misc driver\n", __func__);
		goto out;
	}

	aiste_drv_debug("%s register platorm...\n", __func__);
	ret = platform_driver_register(&mtk_aiste_driver);
	if (ret) {
		aiste_err("%s: failed to register aiste driver\n", __func__);
		goto unregister_misc_dev;
	}

	aiste_procfs_init();
	aiste_scmi_init(g_aiste_addr, g_aiste_size);
	aiste_qos_init();
	goto out;

unregister_misc_dev:
	misc_deregister(aiste_get_misc_dev());
out:
	return 0;
}

void aiste_exit(void)
{
	aiste_qos_deinit();
	aiste_procfs_remove();
	platform_driver_unregister(&mtk_aiste_driver);
	misc_deregister(aiste_get_misc_dev());
}
