// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/err.h>
#include <linux/slab.h>

#include "fuelgauge_class.h"

static struct class *fg_class;

static ssize_t name_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct fg_device *fg_dev = to_fg_device(dev);

	return snprintf(buf, 20, "%s\n",
		       fg_dev->props.alias_name ?
		       fg_dev->props.alias_name : "anonymous");
}

static void fg_device_release(struct device *dev)
{
	struct fg_device *fg_dev = to_fg_device(dev);

	kfree(fg_dev);
}

static DEVICE_ATTR_RO(name);

static struct attribute *fg_class_attrs[] = {
	&dev_attr_name.attr,
	NULL,
};

static const struct attribute_group charger_group = {
	.attrs = fg_class_attrs,
};

static const struct attribute_group *charger_groups[] = {
	&charger_group,
	NULL,
};

int fuelgauge_dev_read_cyclecount(struct fg_device *fg_dev, int *cyclecount)
{
	if (fg_dev != NULL && fg_dev->ops != NULL &&
	    fg_dev->ops->read_cyclecount)
		return fg_dev->ops->read_cyclecount(fg_dev, cyclecount);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(fuelgauge_dev_read_cyclecount);

int fuelgauge_dev_read_rsoc(struct fg_device *fg_dev, int *rsoc)
{
	if (fg_dev != NULL && fg_dev->ops != NULL &&
	    fg_dev->ops->read_rsoc)
		return fg_dev->ops->read_rsoc(fg_dev, rsoc);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(fuelgauge_dev_read_rsoc);

int fuelgauge_dev_read_soh(struct fg_device *fg_dev, int *soh)
{
	if (fg_dev != NULL && fg_dev->ops != NULL &&
	    fg_dev->ops->read_soh)
		return fg_dev->ops->read_soh(fg_dev, soh);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(fuelgauge_dev_read_soh);

int fuelgauge_dev_get_raw_soc(struct fg_device *fg_dev, int *raw_soc)
{
	if (fg_dev != NULL && fg_dev->ops != NULL &&
	    fg_dev->ops->get_raw_soc)
		return fg_dev->ops->get_raw_soc(fg_dev, raw_soc);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(fuelgauge_dev_get_raw_soc);

int fuelgauge_dev_read_current(struct fg_device *fg_dev, int *ibat)
{
	if (fg_dev != NULL && fg_dev->ops != NULL &&
	    fg_dev->ops->read_current)
		return fg_dev->ops->read_current(fg_dev, ibat);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(fuelgauge_dev_read_current);

int fuelgauge_dev_read_temperature(struct fg_device *fg_dev, int *temp)
{
	if (fg_dev != NULL && fg_dev->ops != NULL &&
	    fg_dev->ops->read_temperature)
		return fg_dev->ops->read_temperature(fg_dev, temp);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(fuelgauge_dev_read_temperature);

int fuelgauge_dev_read_volt(struct fg_device *fg_dev, int *vbat)
{
	if (fg_dev != NULL && fg_dev->ops != NULL &&
	    fg_dev->ops->read_volt)
		return fg_dev->ops->read_volt(fg_dev, vbat);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(fuelgauge_dev_read_volt);

int fuelgauge_dev_read_status(struct fg_device *fg_dev, bool *batt_fc)
{
	if (fg_dev != NULL && fg_dev->ops != NULL &&
	    fg_dev->ops->read_status)
		return fg_dev->ops->read_status(fg_dev, batt_fc);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(fuelgauge_dev_read_status);

int fuelgauge_dev_update_battery_shutdown_vol(struct fg_device *fg_dev, int *shutdown_vbat)
{
	if (fg_dev != NULL && fg_dev->ops != NULL &&
	    fg_dev->ops->update_battery_shutdown_vol)
		return fg_dev->ops->update_battery_shutdown_vol(fg_dev, shutdown_vbat);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(fuelgauge_dev_update_battery_shutdown_vol);

int fuelgauge_dev_update_monitor_delay(struct fg_device *fg_dev, int *monitor_delay)
{
	if (fg_dev != NULL && fg_dev->ops != NULL &&
	    fg_dev->ops->update_monitor_delay)
		return fg_dev->ops->update_monitor_delay(fg_dev, monitor_delay);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(fuelgauge_dev_update_monitor_delay);

int fuelgauge_dev_update_eea_chg_support(struct fg_device *fg_dev, bool *eea_support)
{
	if (fg_dev != NULL && fg_dev->ops != NULL &&
	    fg_dev->ops->update_eea_chg_support)
		return fg_dev->ops->update_eea_chg_support(fg_dev, eea_support);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(fuelgauge_dev_update_eea_chg_support);

int fuelgauge_dev_get_charging_done_status(struct fg_device *fg_dev, bool *charging_done)
{
	if (fg_dev != NULL && fg_dev->ops != NULL &&
	    fg_dev->ops->get_charging_done_status)
		return fg_dev->ops->get_charging_done_status(fg_dev, charging_done);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(fuelgauge_dev_get_charging_done_status);

int fuelgauge_dev_get_en_smooth_full_status(struct fg_device *fg_dev, bool *en_smooth_full)
{
	if (fg_dev != NULL && fg_dev->ops != NULL &&
	    fg_dev->ops->get_en_smooth_full_status)
		return fg_dev->ops->get_en_smooth_full_status(fg_dev, en_smooth_full);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(fuelgauge_dev_get_en_smooth_full_status);

int fuelgauge_dev_get_rm(struct fg_device *fg_dev, int *rm)
{
	if (fg_dev != NULL && fg_dev->ops != NULL &&
	    fg_dev->ops->get_rm)
		return fg_dev->ops->get_rm(fg_dev, rm);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(fuelgauge_dev_get_rm);

int fuelgauge_dev_read_avg_current(struct fg_device *fg_dev, int *avg_current)
{
	if (fg_dev != NULL && fg_dev->ops != NULL &&
	    fg_dev->ops->read_avg_current)
		return fg_dev->ops->read_avg_current(fg_dev, avg_current);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(fuelgauge_dev_read_avg_current);

int fuelgauge_dev_read_i2c_error_count(struct fg_device *fg_dev, int *count)
{
	if (fg_dev != NULL && fg_dev->ops != NULL &&
	    fg_dev->ops->read_i2c_error_count)
		return fg_dev->ops->read_i2c_error_count(fg_dev, count);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(fuelgauge_dev_read_i2c_error_count);

int fuelgauge_dev_read_fcc(struct fg_device *fg_dev, int *fcc)
{
	if (fg_dev != NULL && fg_dev->ops != NULL &&
	    fg_dev->ops->read_fcc)
		return fg_dev->ops->read_fcc(fg_dev, fcc);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(fuelgauge_dev_read_fcc);

int fuelgauge_dev_read_design_capacity(struct fg_device *fg_dev, int *dc)
{
	if (fg_dev != NULL && fg_dev->ops != NULL &&
	    fg_dev->ops->read_design_capacity)
		return fg_dev->ops->read_design_capacity(fg_dev, dc);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(fuelgauge_dev_read_design_capacity);

int fuelgauge_dev_get_fg_chip_ok(struct fg_device *fg_dev, int *chip_ok)
{
	if (fg_dev != NULL && fg_dev->ops != NULL &&
	    fg_dev->ops->get_fg_chip_ok)
		return fg_dev->ops->get_fg_chip_ok(fg_dev, chip_ok);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(fuelgauge_dev_get_fg_chip_ok);

int fuelgauge_dev_set_fg_in_sleep(struct fg_device *fg_dev, int sleep)
{
	if (fg_dev != NULL && fg_dev->ops != NULL &&
	    fg_dev->ops->set_fg_in_sleep)
		return fg_dev->ops->set_fg_in_sleep(fg_dev, sleep);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(fuelgauge_dev_set_fg_in_sleep);

int register_fuelgauge_device_notifier(struct fg_device *fg_dev,
				struct notifier_block *nb)
{
	int ret;
	ret = srcu_notifier_chain_register(&fg_dev->evt_nh, nb);
	return ret;
}
EXPORT_SYMBOL(register_fuelgauge_device_notifier);
int unregister_fuelgauge_device_notifier(struct fg_device *fg_dev,
				struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&fg_dev->evt_nh, nb);
}
EXPORT_SYMBOL(unregister_fuelgauge_device_notifier);

/**
 * fg_device_register - create and register a new object of
 *   fg_device class.
 * @name: the name of the new object
 * @parent: a pointer to the parent device
 * @devdata: an optional pointer to be stored for private driver use.
 * The methods may retrieve it by using charger_get_data(charger_dev).
 * @ops: the charger operations structure.
 *
 * Creates and registers new charger device. Returns either an
 * ERR_PTR() or a pointer to the newly allocated device.
 */
struct fg_device *fg_device_register(const char *name,
		struct device *parent, void *devdata,
		const struct fg_ops *ops,
		const struct fg_properties *props)
{
	struct fg_device *fg_dev;
	static struct lock_class_key key;
	struct srcu_notifier_head *head;
	int rc;

	pr_debug("%s: name=%s\n", __func__, name);
	fg_dev = kzalloc(sizeof(*fg_dev), GFP_KERNEL);
	if (!fg_dev)
		return ERR_PTR(-ENOMEM);
	head = &fg_dev->evt_nh;
	srcu_init_notifier_head(head);
	/* Rename srcu's lock to avoid LockProve warning */
	lockdep_init_map(&(&head->srcu)->dep_map, name, &key, 0);
	mutex_init(&fg_dev->ops_lock);
	fg_dev->dev.class = fg_class;
	fg_dev->dev.parent = parent;
	fg_dev->dev.release = fg_device_release;
	dev_set_name(&fg_dev->dev, "%s", name);
	dev_set_drvdata(&fg_dev->dev, devdata);

	/* Copy properties */
	if (props) {
		memcpy(&fg_dev->props, props,
		       sizeof(struct fg_properties));
	}
	rc = device_register(&fg_dev->dev);
	if (rc) {
		kfree(fg_dev);
		return ERR_PTR(rc);
	}
	fg_dev->ops = ops;
	return fg_dev;
}
EXPORT_SYMBOL(fg_device_register);

/**
 * fg_device_unregister - unregisters a switching charger device
 * object.
 * @charger_dev: the switching charger device object to be unregistered
 * and freed.
 *
 * Unregisters a previously registered via fg_device_register object.
 */
void fg_device_unregister(struct fg_device *fg_dev)
{
	if (!fg_dev)
		return;

	mutex_lock(&fg_dev->ops_lock);
	fg_dev->ops = NULL;
	mutex_unlock(&fg_dev->ops_lock);
	device_unregister(&fg_dev->dev);
}
EXPORT_SYMBOL(fg_device_unregister);


static int charger_match_device_by_name(struct device *dev,
	const void *data)
{
	const char *name = data;

	return strcmp(dev_name(dev), name) == 0;
}

struct fg_device *get_fuelgauge_by_name(const char *name)
{
	struct device *dev;

	if (!name)
		return (struct fg_device *)NULL;
	dev = class_find_device(fg_class, NULL, name,
				charger_match_device_by_name);

	return dev ? to_fg_device(dev) : NULL;

}
EXPORT_SYMBOL(get_fuelgauge_by_name);

static void __exit fg_class_exit(void)
{
	class_destroy(fg_class);
}

static int __init fg_class_init(void)
{
	fg_class = class_create("fuelgauge_class");
	if (IS_ERR(fg_class)) {
		pr_notice("Unable to create charger class; errno = %ld\n",
			PTR_ERR(fg_class));
		return PTR_ERR(fg_class);
	}
	fg_class->dev_groups = charger_groups;
	return 0;
}

module_init(fg_class_init);
module_exit(fg_class_exit);

MODULE_DESCRIPTION("fuelgauge Class Device");
MODULE_AUTHOR("xiezhichang <xiezhichang@xiaomi.com>");
MODULE_VERSION("1.0.0_G");
MODULE_LICENSE("GPL");
