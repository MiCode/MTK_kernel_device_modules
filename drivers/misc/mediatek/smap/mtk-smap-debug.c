// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/io.h>
#include "mtk-smap-common.h"

static struct mtk_smap *smap_data;

static unsigned int smap_read(u32 offset)
{
	int len;

	if (!smap_data) {
		smap_print("No smap_data\n");
		return 0;
	}

	len = readl(smap_data->regs + offset);
	return len;
}

static void smap_write(u32 offset, u32 val)
{
	if (!smap_data) {
		smap_print("No smap_data\n");
		return;
	}

	writel(val, smap_data->regs + offset);
}

static ssize_t smap_debugfs_write(struct file *f, const char __user *buf,
				    size_t count, loff_t *offset)
{
	unsigned int val, offs;
	char *kbuf;

	if (!smap_data)
		return -EFAULT;

	if (!buf || count == 0)
		return -EINVAL;

	kbuf = kzalloc(count + 1, GFP_KERNEL);
	if (kbuf == NULL)
		return -ENOMEM;

	if (copy_from_user(kbuf, buf, count)) {
		kfree(kbuf);
		return -EFAULT;
	}
	kbuf[count] = '\0';

	if (sscanf(kbuf, "%x %x\n", &offs, &val) == 2) {
		smap_write(offs, val);
		smap_data->reg_value = smap_read(offs);
	} else if (sscanf(kbuf, "%x\n", &offs) == 1) {
		smap_data->reg_value = smap_read(offs);
	} else {
		smap_print("parameter number not correct\n");
	}

	kfree(kbuf);
	return count;
}

static ssize_t smap_debugfs_read(struct file *file, char __user *buf,
				   size_t count, loff_t *pos)
{
	int len;
	char buffer[64];

	if (!smap_data)
		return -EFAULT;

	len = snprintf(buffer, sizeof(buffer), "0x%x\n", smap_data->reg_value);
	if (len < 0)
		return 0;

	return simple_read_from_buffer(buf, count, pos, buffer, len);
}

static const struct file_operations smap_debugfs_ops = {
	.write = smap_debugfs_write,
	.read = smap_debugfs_read,
};

static int smap_register_debugfs(struct device *dev)
{
	if (!debugfs_create_file("smap_debugfs_RW", 0644, NULL, NULL, &smap_debugfs_ops)) {
		dev_info(dev, "Unable to create smap_debugfs_RW\n");
		return -ENOMEM;
	}

	return 0;
}

static int mtk_smap_debug_probe(struct platform_device *pdev)
{
	struct platform_device *parent_dev;
	struct device *dev = &pdev->dev;

	parent_dev = to_platform_device(dev->parent);

	smap_data = platform_get_drvdata(parent_dev);
	if (!smap_data) {
		smap_print("Can't get parent smap_data\n");
		return -ENOMEM;
	}

	smap_register_debugfs(&pdev->dev);
	smap_print("Done\n");

	return 0;
}

static void mtk_smap_debug_remove(struct platform_device *pdev)
{
	smap_print("Remove\n");
}

static const struct of_device_id mtk_smap_debug_of_match[] = {
	{ .compatible = "mediatek,smap-debug", },
	{}
};
MODULE_DEVICE_TABLE(of, mtk_smap_debug_of_match);

static struct platform_driver mtk_smap_debug_driver = {
	.driver = {
		.name = "mtk-smap-debug",
		.of_match_table = mtk_smap_debug_of_match,
	},
	.probe = mtk_smap_debug_probe,
	.remove = mtk_smap_debug_remove,
};
module_platform_driver(mtk_smap_debug_driver);

MODULE_AUTHOR("Victor lin <victor-wc.lin@mediatek.com>");
MODULE_DESCRIPTION("Debug driver for MediaTek SMAP");
MODULE_LICENSE("GPL");
