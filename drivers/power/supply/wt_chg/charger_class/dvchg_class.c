// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
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

#include "dvchg_class.h"

static struct class *dvchg_class;

int dvchg_set_chip_init(struct dvchg_dev *dvchg)
{
	if (!dvchg || !dvchg->ops)
		return -EINVAL;
	if (dvchg->ops->set_chip_init == NULL)
		return -EOPNOTSUPP;
	return dvchg->ops->set_chip_init(dvchg);
}

int dvchg_set_enable(struct dvchg_dev *dvchg, bool enable)
{
	if (!dvchg || !dvchg->ops)
		return -EINVAL;
	if (dvchg->ops->set_enable == NULL)
		return -EOPNOTSUPP;
	return dvchg->ops->set_enable(dvchg, enable);
}
EXPORT_SYMBOL(dvchg_set_enable);

int dvchg_set_vbus_ovp(struct dvchg_dev *dvchg, int mv)
{
	if (!dvchg || !dvchg->ops)
		return -EINVAL;
	if (dvchg->ops->set_vbus_ovp == NULL)
		return -EOPNOTSUPP;
	return dvchg->ops->set_vbus_ovp(dvchg, mv);
}

int dvchg_set_ibus_ocp(struct dvchg_dev *dvchg, int ma)
{
	if (!dvchg || !dvchg->ops)
		return -EINVAL;
	if (dvchg->ops->set_ibus_ocp == NULL)
		return -EOPNOTSUPP;
	return dvchg->ops->set_ibus_ocp(dvchg, ma);
}
EXPORT_SYMBOL(dvchg_set_ibus_ocp);

int dvchg_set_vbat_ovp(struct dvchg_dev *dvchg, int mv)
{
	if (!dvchg || !dvchg->ops)
		return -EINVAL;
	if (dvchg->ops->set_vbat_ovp == NULL)
		return -EOPNOTSUPP;
	return dvchg->ops->set_vbat_ovp(dvchg, mv);
}
EXPORT_SYMBOL(dvchg_set_vbat_ovp);

int dvchg_set_ibat_ocp(struct dvchg_dev *dvchg, int ma)
{
	if (!dvchg || !dvchg->ops)
		return -EINVAL;
	if (dvchg->ops->set_ibat_ocp == NULL)
		return -EOPNOTSUPP;
	return dvchg->ops->set_ibat_ocp(dvchg, ma);
}
EXPORT_SYMBOL(dvchg_set_ibat_ocp);

int dvchg_set_enable_adc(struct dvchg_dev *dvchg, bool enable)
{
	if (!dvchg || !dvchg->ops)
		return -EINVAL;
	if (dvchg->ops->set_enable_adc == NULL)
		return -EOPNOTSUPP;
	return dvchg->ops->set_enable_adc(dvchg, enable);
}
EXPORT_SYMBOL(dvchg_set_enable_adc);

int dvchg_get_is_enable(struct dvchg_dev *dvchg, bool *enable)
{
	if (!dvchg || !dvchg->ops)
		return -EINVAL;
	if (dvchg->ops->get_is_enable == NULL)
		return -EOPNOTSUPP;
	return dvchg->ops->get_is_enable(dvchg, enable);
}
EXPORT_SYMBOL(dvchg_get_is_enable);

int dvchg_get_status(struct dvchg_dev *dvchg, uint32_t *status)
{
	if (!dvchg || !dvchg->ops)
		return -EINVAL;
	if (dvchg->ops->get_status == NULL)
		return -EOPNOTSUPP;
	return dvchg->ops->get_status(dvchg, status);
}
EXPORT_SYMBOL(dvchg_get_status);

int dvchg_get_adc_value(struct dvchg_dev *dvchg, enum sc_adc_channel ch, int *value)
{
	if (!dvchg || !dvchg->ops)
		return -EINVAL;
	if (dvchg->ops->get_adc_value == NULL)
		return -EOPNOTSUPP;
	return dvchg->ops->get_adc_value(dvchg, ch, value);
}
EXPORT_SYMBOL(dvchg_get_adc_value);

static int dvchg_match_device_by_name(struct device *dev, const void *data)
{
	const char *name = data;
	struct dvchg_dev *dvchg = dev_get_drvdata(dev);

	return strcmp(dvchg->name, name) == 0;
}

struct dvchg_dev *dvchg_find_dev_by_name(const char *name)
{
	struct dvchg_dev *dvchg = NULL;
	struct device *dev = class_find_device(dvchg_class, NULL, name,
					dvchg_match_device_by_name);

	if (dev) {
		dvchg = dev_get_drvdata(dev);
	}

	return dvchg;
}
EXPORT_SYMBOL(dvchg_find_dev_by_name);

struct dvchg_dev *dvchg_register(char *name, struct device *parent,
							struct dvchg_ops *ops, void *private)
{
	struct dvchg_dev *dvchg;
	struct device *dev;
	int ret;

	if (!parent)
		pr_warn("%s: Expected proper parent device\n", __func__);

	if (!ops || !name)
		return ERR_PTR(-EINVAL);

	dvchg = kzalloc(sizeof(*dvchg), GFP_KERNEL);
	if (!dvchg)
		return ERR_PTR(-ENOMEM);

	dev = &(dvchg->dev);

	device_initialize(dev);

	dev->class = dvchg_class;
	dev->parent = parent;
	dev_set_drvdata(dev, dvchg);

	dvchg->private = private;

	ret = dev_set_name(dev, "%s", name);
	if (ret)
		goto dev_set_name_failed;

	ret = device_add(dev);
	if (ret)
		goto device_add_failed;

	dvchg->name = name;
	dvchg->ops = ops;

	return dvchg;

device_add_failed:
dev_set_name_failed:
	put_device(dev);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(dvchg_register);

void * dvchg_get_private(struct dvchg_dev *dvchg)
{
	if (!dvchg)
		return ERR_PTR(-EINVAL);
	return dvchg->private;
}
EXPORT_SYMBOL(dvchg_get_private);

int dvchg_unregister(struct dvchg_dev *dvchg)
{
	device_unregister(&dvchg->dev);
	kfree(dvchg);
	return 0;
}

static int __init dvchg_class_init(void)
{
	dvchg_class = class_create(THIS_MODULE, "dvchg_class");
	if (IS_ERR(dvchg_class)) {
		return PTR_ERR(dvchg_class);
	}

	dvchg_class->dev_uevent = NULL;

	return 0;
}

static void __exit dvchg_class_exit(void)
{
	class_destroy(dvchg_class);
}

subsys_initcall(dvchg_class_init);
module_exit(dvchg_class_exit);

MODULE_AUTHOR("Lipei Liu <Lipei-liu@southchip.com>");
MODULE_DESCRIPTION("Southchip Charger Pump Class Core");
MODULE_LICENSE("GPL v2");
