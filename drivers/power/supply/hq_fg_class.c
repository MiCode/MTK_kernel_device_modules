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

#include "hq_fg_class.h"

static struct class *fuel_gauge_class;

int fuel_gauge_get_soc_decimal(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->get_soc_decimal == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->get_soc_decimal(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_get_soc_decimal);

int fuel_gauge_get_soc_decimal_rate(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->get_soc_decimal_rate == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->get_soc_decimal_rate(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_get_soc_decimal_rate);

int fuel_gauge_get_rsoc(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->get_rsoc == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->get_rsoc(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_get_rsoc);

int fuel_gauge_set_rsoc_update0(struct fuel_gauge_dev *fuel_gauge, bool value)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->set_rsoc_update0 == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->set_rsoc_update0(fuel_gauge, value);
}
EXPORT_SYMBOL(fuel_gauge_set_rsoc_update0);

int fuel_gauge_get_batt_id(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->get_batt_id == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->get_batt_id(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_get_batt_id);

int fuel_gauge_get_batt_auth(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->get_batt_auth == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->get_batt_auth(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_get_batt_auth);

int fuel_gauge_get_batt_id_voltage(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->get_batt_id_voltage == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->get_batt_id_voltage(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_get_batt_id_voltage);

int fuel_gauge_get_c_car(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->get_c_car == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->get_c_car(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_get_c_car);

int fuel_gauge_get_v_car(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->get_v_car == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->get_v_car(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_get_v_car);

int fuel_gauge_check_fg_status(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->check_fg_status == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->check_fg_status(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_check_fg_status);

int fuel_gauge_get_raw_soc(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->get_raw_soc == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->get_raw_soc(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_get_raw_soc);

int fuel_gauge_set_fastcharge_mode(struct fuel_gauge_dev *fuel_gauge, bool en)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->set_fastcharge_mode == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->set_fastcharge_mode(fuel_gauge, en);
}
EXPORT_SYMBOL(fuel_gauge_set_fastcharge_mode);

int fuel_gauge_get_fastcharge_mode(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->get_fastcharge_mode == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->get_fastcharge_mode(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_get_fastcharge_mode);

int fuel_gauge_report_fg_soc100(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge || !fuel_gauge->ops)
		return -EINVAL;
	if (fuel_gauge->ops->report_fg_soc100 == NULL)
		return -EOPNOTSUPP;
	return fuel_gauge->ops->report_fg_soc100(fuel_gauge);
}
EXPORT_SYMBOL(fuel_gauge_report_fg_soc100);

static int fuel_gauge_match_device_by_name(struct device *dev, const void *data)
{
	const char *name = data;
	struct fuel_gauge_dev *fuel_gauge = dev_get_drvdata(dev);

	return strcmp(fuel_gauge->name, name) == 0;
}

struct fuel_gauge_dev *fuel_gauge_find_dev_by_name(const char *name)
{
	struct fuel_gauge_dev *fuel_gauge = NULL;
	struct device *dev = class_find_device(fuel_gauge_class, NULL, name,
					fuel_gauge_match_device_by_name);

	if (dev) {
		fuel_gauge = dev_get_drvdata(dev);
	}

	return fuel_gauge;
}
EXPORT_SYMBOL(fuel_gauge_find_dev_by_name);


struct fuel_gauge_dev *fuel_gauge_register(char *name, struct device *parent,
							struct fuel_gauge_ops *ops, void *private)
{
	struct fuel_gauge_dev *fuel_gauge;
	struct device *dev;
	int ret;

	if (!parent)
		pr_warn("%s: Expected proper parent device\n", __func__);

	if (!ops || !name)
		return ERR_PTR(-EINVAL);

	fuel_gauge = kzalloc(sizeof(*fuel_gauge), GFP_KERNEL);
	if (!fuel_gauge)
		return ERR_PTR(-ENOMEM);

	dev = &(fuel_gauge->dev);

	device_initialize(dev);

	dev->class = fuel_gauge_class;
	dev->parent = parent;
	dev_set_drvdata(dev, fuel_gauge);

	fuel_gauge->private = private;

	ret = dev_set_name(dev, "%s", name);
	if (ret)
		goto dev_set_name_failed;

	ret = device_add(dev);
	if (ret)
		goto device_add_failed;

	fuel_gauge->name = name;
	fuel_gauge->ops = ops;

	return fuel_gauge;

device_add_failed:
dev_set_name_failed:
	put_device(dev);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(fuel_gauge_register);


void *fuel_gauge_get_private(struct fuel_gauge_dev *fuel_gauge)
{
	if (!fuel_gauge)
		return ERR_PTR(-EINVAL);
	return fuel_gauge->private;
}
EXPORT_SYMBOL(fuel_gauge_get_private);

int fuel_gauge_unregister(struct fuel_gauge_dev *fuel_gauge)
{
	device_unregister(&fuel_gauge->dev);
	kfree(fuel_gauge);
	return 0;
}

static int __init fuel_gauge_class_init(void)
{
	fuel_gauge_class = class_create(THIS_MODULE, "fuel_gauge_class");
	if (IS_ERR(fuel_gauge_class)) {
		return PTR_ERR(fuel_gauge_class);
	}

	fuel_gauge_class->dev_uevent = NULL;

	return 0;
}

static void __exit fuel_gauge_class_exit(void)
{
	class_destroy(fuel_gauge_class);
}

subsys_initcall(fuel_gauge_class_init);
module_exit(fuel_gauge_class_exit);

MODULE_DESCRIPTION("Huaqin Fuel Gauge Class Core");
MODULE_LICENSE("GPL v2");
