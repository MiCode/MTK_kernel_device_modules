 /* Copyright (c) 2022 The Linux Foundation. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
/* this driver is compatible for wireless charger class */
#include <linux/module.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/err.h>
#include <linux/slab.h>
#include "wireless_charger_class.h"

static struct class *wireless_charger_class;
static ssize_t name_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct wireless_charger_device *chg_dev = to_wireless_charger_device(dev);
	return snprintf(buf, 20, "%s\n",
		       chg_dev->props.alias_name ?
		       chg_dev->props.alias_name : "wls_anonymous");
}
static void wireless_charger_device_release(struct device *dev)
{
	struct wireless_charger_device *chg_dev = to_wireless_charger_device(dev);
	kfree(chg_dev);
}
int wireless_charger_dev_enable_wls_reverse_chg(struct wireless_charger_device *chg_dev, bool en)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->wls_enable_reverse_chg)
		return chg_dev->ops->wls_enable_reverse_chg(chg_dev, en);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(wireless_charger_dev_enable_wls_reverse_chg);
int wireless_charger_dev_is_wireless_present(struct wireless_charger_device *chg_dev, bool *present)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->wls_is_wireless_present)
		return chg_dev->ops->wls_is_wireless_present(chg_dev, present);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(wireless_charger_dev_is_wireless_present);
int wireless_charger_dev_is_i2c_ok(struct wireless_charger_device *chg_dev, bool *i2c_ok)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->wls_is_i2c_ok)
		return chg_dev->ops->wls_is_i2c_ok(chg_dev, i2c_ok);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(wireless_charger_dev_is_i2c_ok);
int wireless_charger_dev_is_qc_enable(struct wireless_charger_device *chg_dev, bool *enable)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->wls_is_qc_enable)
		return chg_dev->ops->wls_is_qc_enable(chg_dev, enable);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(wireless_charger_dev_is_qc_enable);
int wireless_charger_dev_is_firmware_update(struct wireless_charger_device *chg_dev, bool *update)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->wls_is_firmware_update)
		return chg_dev->ops->wls_is_firmware_update(chg_dev, update);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(wireless_charger_dev_is_firmware_update);
int wireless_charger_dev_wls_set_vout(struct wireless_charger_device *chg_dev, int vout)
{
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->wls_set_vout)
		return chg_dev->ops->wls_set_vout(chg_dev, vout);
	return -ENOTSUPP;
}
EXPORT_SYMBOL(wireless_charger_dev_wls_set_vout);
int wireless_charger_dev_wls_notify_cp_status(struct wireless_charger_device *chg_dev, int status)
{
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->wls_notify_cp_status)
		return chg_dev->ops->wls_notify_cp_status(chg_dev, status);
	return -ENOTSUPP;
}
EXPORT_SYMBOL(wireless_charger_dev_wls_notify_cp_status);
int wireless_charger_dev_wls_get_chip_version(struct wireless_charger_device *chg_dev, char *buf)
{
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->wls_get_chip_version)
		return chg_dev->ops->wls_get_chip_version(chg_dev, buf);
	return -ENOTSUPP;
}
EXPORT_SYMBOL(wireless_charger_dev_wls_get_chip_version);
int wireless_charger_dev_wls_firmware_update(struct wireless_charger_device *chg_dev, int cmd)
{
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->wls_firmware_update)
		return chg_dev->ops->wls_firmware_update(chg_dev, cmd);
	return -ENOTSUPP;
}
EXPORT_SYMBOL(wireless_charger_dev_wls_firmware_update);
int wireless_charger_dev_wls_check_fw_version(struct wireless_charger_device *chg_dev)
{
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->wls_check_fw_version)
		return chg_dev->ops->wls_check_fw_version(chg_dev);
	return -ENOTSUPP;
}
EXPORT_SYMBOL(wireless_charger_dev_wls_check_fw_version);
int wireless_charger_dev_wls_get_vout(struct wireless_charger_device *chg_dev, int *vout)
{
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->wls_get_vout)
		return chg_dev->ops->wls_get_vout(chg_dev, vout);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(wireless_charger_dev_wls_get_vout);
int wireless_charger_dev_wls_get_iout(struct wireless_charger_device *chg_dev, int *iout)
{
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->wls_get_iout)
		return chg_dev->ops->wls_get_iout(chg_dev, iout);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(wireless_charger_dev_wls_get_iout);
int wireless_charger_dev_wls_get_vrect(struct wireless_charger_device *chg_dev, int *vrect)
{
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->wls_get_vrect)
		return chg_dev->ops->wls_get_vrect(chg_dev, vrect);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(wireless_charger_dev_wls_get_vrect);
int wireless_charger_dev_wls_get_tx_adapter(struct wireless_charger_device *chg_dev, int *adapter)
{
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->wls_get_tx_adapter)
		return chg_dev->ops->wls_get_tx_adapter(chg_dev, adapter);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(wireless_charger_dev_wls_get_tx_adapter);
int wireless_charger_dev_wls_get_reverse_chg_state(struct wireless_charger_device *chg_dev, int *state)
{
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->wls_get_reverse_chg_state)
		return chg_dev->ops->wls_get_reverse_chg_state(chg_dev, state);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(wireless_charger_dev_wls_get_reverse_chg_state);

int wireless_charger_dev_is_enable(struct wireless_charger_device *chg_dev, bool *en)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->wls_is_enable)
		return chg_dev->ops->wls_is_enable(chg_dev, en);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(wireless_charger_dev_is_enable);

int wireless_charger_dev_enable_wls_chg(struct wireless_charger_device *chg_dev, bool en)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->wls_enable_chg)
		return chg_dev->ops->wls_enable_chg(chg_dev, en);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(wireless_charger_dev_enable_wls_chg);

int wireless_charger_dev_is_car_adapter(struct wireless_charger_device *chg_dev, bool *enable)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->wls_is_car_adapter)
		return chg_dev->ops->wls_is_car_adapter(chg_dev, enable);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(wireless_charger_dev_is_car_adapter);
int wireless_charger_dev_get_reverse_chg(struct wireless_charger_device *chg_dev, bool *enable)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->wls_get_reverse_chg)
		return chg_dev->ops->wls_get_reverse_chg(chg_dev, enable);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(wireless_charger_dev_get_reverse_chg);
int wireless_charger_dev_wls_set_rx_sleep_mode(struct wireless_charger_device *chg_dev, int sleep_for_dam)
{
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->wls_set_rx_sleep_mode)
		return chg_dev->ops->wls_set_rx_sleep_mode(chg_dev, sleep_for_dam);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(wireless_charger_dev_wls_set_rx_sleep_mode);

int wireless_charger_dev_wls_set_quiet_sts(struct wireless_charger_device *chg_dev, int quiet_sts)
{
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->wls_set_quiet_sts)
		return chg_dev->ops->wls_set_quiet_sts(chg_dev, quiet_sts);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(wireless_charger_dev_wls_set_quiet_sts);

int wireless_charger_dev_wls_set_parallel_charge(struct wireless_charger_device *chg_dev, bool parachg)
{
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->wls_set_parallel_charge)
		return chg_dev->ops->wls_set_parallel_charge(chg_dev, parachg);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(wireless_charger_dev_wls_set_parallel_charge);

int wireless_charger_dev_is_vout_range_set_done(struct wireless_charger_device *chg_dev, bool *is_done)
{
	if (chg_dev != NULL && chg_dev->ops != NULL &&
	    chg_dev->ops->wls_is_vout_range_set_done)
		return chg_dev->ops->wls_is_vout_range_set_done(chg_dev, is_done);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(wireless_charger_dev_is_vout_range_set_done);

int wireless_charger_dev_wls_get_adapter_chg_mode(struct wireless_charger_device *chg_dev, int *cp_chg_mode)
{
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->wls_get_adapter_chg_mode)
		return chg_dev->ops->wls_get_adapter_chg_mode(chg_dev, cp_chg_mode);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(wireless_charger_dev_wls_get_adapter_chg_mode);

int wireless_charger_dev_wls_ln_set_mode(struct wireless_charger_device *chg_dev, int value)
{
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->wls_ln_set_mode)
		return chg_dev->ops->wls_ln_set_mode(chg_dev, value);
	return -ENOTSUPP;
}
EXPORT_SYMBOL(wireless_charger_dev_wls_ln_set_mode);
int wireless_charger_dev_wls_ln_get_mode(struct wireless_charger_device *chg_dev, int *value)
{
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->wls_ln_get_mode)
		return chg_dev->ops->wls_ln_get_mode(chg_dev, value);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(wireless_charger_dev_wls_ln_get_mode);
int wireless_charger_dev_wls_get_pen_soc(struct wireless_charger_device *chg_dev, int *soc)
{
	if (chg_dev != NULL && chg_dev->ops != NULL && chg_dev->ops->wls_get_pen_soc)
		return chg_dev->ops->wls_get_pen_soc(chg_dev, soc);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(wireless_charger_dev_wls_get_pen_soc);
static DEVICE_ATTR_RO(name);
static struct attribute *wireless_charger_class_attrs[] = {
	&dev_attr_name.attr,
	NULL,
};
static const struct attribute_group wireless_charger_group = {
	.attrs = wireless_charger_class_attrs,
};
static const struct attribute_group *wireless_charger_groups[] = {
	&wireless_charger_group,
	NULL,
};
int register_wireless_charger_device_notifier(struct wireless_charger_device *chg_dev,
				struct notifier_block *nb)
{
	int ret;
	ret = srcu_notifier_chain_register(&chg_dev->evt_nh, nb);
	return ret;
}
EXPORT_SYMBOL(register_wireless_charger_device_notifier);
int unregister_wireless_charger_device_notifier(struct wireless_charger_device *chg_dev,
				struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&chg_dev->evt_nh, nb);
}
EXPORT_SYMBOL(unregister_wireless_charger_device_notifier);
/**
 * wireless_charger_device_register - create and register a new object of
 *   wireless charger_device class.
 * @name: the name of the new object
 * @parent: a pointer to the parent device
 * @devdata: an optional pointer to be stored for private driver use.
 * The methods may retrieve it by using wireless charger_get_data(charger_dev).
 * @ops: the charger operations structure.
 *
 * Creates and registers new wireless charger device. Returns either an
 * ERR_PTR() or a pointer to the newly allocated device.
 */
struct wireless_charger_device *wireless_charger_device_register(const char *name,
		struct device *parent, void *devdata,
		const struct wireless_charger_ops *ops,
		const struct wireless_charger_properties *props)
{
	struct wireless_charger_device *chg_dev;
	static struct lock_class_key key;
	struct srcu_notifier_head *head;
	int rc;
	pr_debug("%s: name=%s\n", __func__, name);
	chg_dev = kzalloc(sizeof(*chg_dev), GFP_KERNEL);
	if (!chg_dev)
		return ERR_PTR(-ENOMEM);
	head = &chg_dev->evt_nh;
	srcu_init_notifier_head(head);
	/* Rename srcu's lock to avoid LockProve warning */
	lockdep_init_map(&(&head->srcu)->dep_map, name, &key, 0);
	mutex_init(&chg_dev->ops_lock);
	chg_dev->dev.class = wireless_charger_class;
	chg_dev->dev.parent = parent;
	chg_dev->dev.release = wireless_charger_device_release;
	dev_set_name(&chg_dev->dev, name);
	dev_set_drvdata(&chg_dev->dev, devdata);
	/* Copy properties */
	if (props) {
		memcpy(&chg_dev->props, props,
		       sizeof(struct wireless_charger_properties));
	}
	rc = device_register(&chg_dev->dev);
	if (rc) {
		kfree(chg_dev);
		return ERR_PTR(rc);
	}
	chg_dev->ops = ops;
	return chg_dev;
}
EXPORT_SYMBOL(wireless_charger_device_register);
/**
 * wireless_charger_device_unregister - unregisters a wireless charger device
 * object.
 * @wireless_charger_dev: the wireless charger device object to be unregistered
 * and freed.
 *
 * Unregisters a previously registered via wireless_charger_device_register object.
 */
void wireless_charger_device_unregister(struct wireless_charger_device *chg_dev)
{
	if (!chg_dev)
		return;
	mutex_lock(&chg_dev->ops_lock);
	chg_dev->ops = NULL;
	mutex_unlock(&chg_dev->ops_lock);
	device_unregister(&chg_dev->dev);
}
EXPORT_SYMBOL(wireless_charger_device_unregister);
static int wireless_charger_match_device_by_name(struct device *dev,
	const void *data)
{
	const char *name = data;
	return strcmp(dev_name(dev), name) == 0;
}
struct wireless_charger_device *get_wireless_charger_by_name(const char *name)
{
	struct device *dev;
	if (!name)
		return (struct wireless_charger_device *)NULL;
	dev = class_find_device(wireless_charger_class, NULL, name,
				wireless_charger_match_device_by_name);
	return dev ? to_wireless_charger_device(dev) : NULL;
}
EXPORT_SYMBOL(get_wireless_charger_by_name);
static void __exit wireless_charger_class_exit(void)
{
	class_destroy(wireless_charger_class);
}
static int __init wireless_charger_class_init(void)
{
	wireless_charger_class = class_create("wireless_charger");
	if (IS_ERR(wireless_charger_class)) {
		pr_notice("Unable to create wireless charger class; errno = %ld\n",
			PTR_ERR(wireless_charger_class));
		return PTR_ERR(wireless_charger_class);
	}
	wireless_charger_class->dev_groups = wireless_charger_groups;
	return 0;
}
module_init(wireless_charger_class_init);
module_exit(wireless_charger_class_exit);
MODULE_DESCRIPTION("Wireless Charger Class Device");
MODULE_AUTHOR("xiezhichang <xiezhichang@xiaomi.com>");
MODULE_VERSION("1.0.0_G");
MODULE_LICENSE("GPL");
