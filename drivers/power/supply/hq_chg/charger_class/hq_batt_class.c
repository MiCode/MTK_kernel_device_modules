// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2023 Huaqin Technology(Shanghai) Co., Ltd.
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/notifier.h>
#include <linux/err.h>
#include <linux/of.h>

#include "hq_batt_class.h"

static struct class *batt_info_class;

int batt_info_get_batt_id(struct batt_info_dev *batt_info)
{
	if (!batt_info || !batt_info->ops)
		return BATTERY_ID_UNKNOWN;
	if (batt_info->ops->get_batt_id == NULL)
		return BATTERY_ID_UNKNOWN;
	return batt_info->ops->get_batt_id(batt_info);
}
EXPORT_SYMBOL(batt_info_get_batt_id);

char *batt_info_get_batt_name(struct batt_info_dev *batt_info)
{
	if (!batt_info || !batt_info->ops)
		return BATTERY_NAME_UNKNOWN;
	if (batt_info->ops->get_batt_name == NULL)
		return BATTERY_NAME_UNKNOWN;
	return batt_info->ops->get_batt_name(batt_info);
}
EXPORT_SYMBOL(batt_info_get_batt_name);

int batt_info_get_chip_ok(struct batt_info_dev *batt_info)
{
	if (!batt_info || !batt_info->ops)
		return 0;
	if (batt_info->ops->get_batt_id == NULL)
		return 0;
	return batt_info->ops->get_chip_ok(batt_info);
}
EXPORT_SYMBOL(batt_info_get_chip_ok);


static int batt_info_match_device_by_name(struct device *dev, const void *data)
{
	const char *name = data;
	struct batt_info_dev *batt_info = dev_get_drvdata(dev);

	return strcmp(batt_info->name, name) == 0;
}

struct batt_info_dev *batt_info_find_dev_by_name(const char *name)
{
	struct batt_info_dev *batt_info = NULL;
	struct device *dev = class_find_device(batt_info_class, NULL, name,
					batt_info_match_device_by_name);

	if (dev) {
		batt_info = dev_get_drvdata(dev);
	}

	return batt_info;
}
EXPORT_SYMBOL(batt_info_find_dev_by_name);

struct batt_info_dev *batt_info_register(char *name, struct device *parent,
							struct batt_info_ops *ops, void *private)
{
	struct batt_info_dev *batt_info;
	struct device *dev;
	int ret;

	if (!parent)
		pr_warn("%s: Expected proper parent device\n", __func__);

	if (!ops || !name)
		return ERR_PTR(-EINVAL);

	batt_info = kzalloc(sizeof(*batt_info), GFP_KERNEL);
	if (!batt_info)
		return ERR_PTR(-ENOMEM);

	dev = &(batt_info->dev);

	device_initialize(dev);

	dev->class = batt_info_class;
	dev->parent = parent;
	dev_set_drvdata(dev, batt_info);

	batt_info->private = private;

	ret = dev_set_name(dev, "%s", name);
	if (ret)
		goto dev_set_name_failed;

	ret = device_add(dev);
	if (ret)
		goto device_add_failed;

	batt_info->name = name;
	batt_info->ops = ops;

	return batt_info;

device_add_failed:
dev_set_name_failed:
	put_device(dev);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(batt_info_register);

void *batt_info_get_private(struct batt_info_dev *batt_info)
{
	if (!batt_info)
		return ERR_PTR(-EINVAL);
	return batt_info->private;
}
EXPORT_SYMBOL(batt_info_get_private);

int batt_info_unregister(struct batt_info_dev *batt_info)
{
	device_unregister(&batt_info->dev);
	kfree(batt_info);
	return 0;
}

static int __init batt_info_class_init(void)
{
	batt_info_class = class_create("batt_info_class");
	if (IS_ERR(batt_info_class)) {
		return PTR_ERR(batt_info_class);
	}

	batt_info_class->dev_uevent = NULL;

	return 0;
}

static void __exit batt_info_class_exit(void)
{
	class_destroy(batt_info_class);
}

subsys_initcall(batt_info_class_init);
module_exit(batt_info_class_exit);

MODULE_DESCRIPTION("Huaqin batt Class Core");
MODULE_LICENSE("GPL v2");
