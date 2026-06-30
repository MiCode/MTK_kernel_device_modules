// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 ARM Ltd.
 * Copyright (c) 2025 MediaTek Inc.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#if defined(MTK_ADAPTED) && MTK_ADAPTED
#include "arm_ffa.h"
#else
#include <linux/arm_ffa.h>
#endif /* defined(MTK_ADAPTED) && MTK_ADAPTED */
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>

#include "common.h"

#define SCMI_UEVENT_MODALIAS_FMT	"arm_ffa:%04x:%pUb"

static DEFINE_IDA(ffa_bus_id);

static int ffa_device_match(struct device *dev, const struct device_driver *drv)
{
	const struct ffa_device_id *id_table = NULL;
	struct ffa_device *ffa_dev = NULL;

	id_table = to_ffa_driver(drv)->id_table;
	ffa_dev = to_ffa_dev(dev);

	while (!uuid_is_null(&id_table->uuid)) {
		/*
		 * FF-A v1.0 doesn't provide discovery of UUIDs, just the
		 * partition IDs, so match it unconditionally here and handle
		 * it via the installed bus notifier during driver binding.
		 */
		if (uuid_is_null(&ffa_dev->uuid))
			return 1;

		if (uuid_equal(&ffa_dev->uuid, &id_table->uuid))
			return 1;
		id_table++;
	}

	return 0;
}

static int ffa_device_probe(struct device *dev)
{
	struct ffa_driver *ffa_drv = to_ffa_driver(dev->driver);
	struct ffa_device *ffa_dev = to_ffa_dev(dev);

	/* UUID can be still NULL with FF-A v1.0, so just skip probing them */
	if (uuid_is_null(&ffa_dev->uuid))
		return -ENODEV;

	return ffa_drv->probe(ffa_dev);
}

static void ffa_device_remove(struct device *dev)
{
	struct ffa_driver *ffa_drv = to_ffa_driver(dev->driver);

	if (ffa_drv->remove)
		ffa_drv->remove(to_ffa_dev(dev));
}

static int ffa_device_uevent(const struct device *dev, struct kobj_uevent_env *env)
{
	const struct ffa_device *ffa_dev = to_ffa_dev(dev);

	return add_uevent_var(env, "MODALIAS=" SCMI_UEVENT_MODALIAS_FMT,
			      ffa_dev->vm_id, &ffa_dev->uuid);
}

static ssize_t modalias_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct ffa_device *ffa_dev = to_ffa_dev(dev);

	return sysfs_emit(buf, SCMI_UEVENT_MODALIAS_FMT, ffa_dev->vm_id,
			  &ffa_dev->uuid);
}
static DEVICE_ATTR_RO(modalias);

static ssize_t partition_id_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct ffa_device *ffa_dev = to_ffa_dev(dev);

	return sprintf(buf, "0x%04x\n", ffa_dev->vm_id);
}
static DEVICE_ATTR_RO(partition_id);

static ssize_t uuid_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct ffa_device *ffa_dev = to_ffa_dev(dev);

	return sprintf(buf, "%pUb\n", &ffa_dev->uuid);
}
static DEVICE_ATTR_RO(uuid);

static struct attribute *ffa_device_attributes_attrs[] = {
	&dev_attr_partition_id.attr,
	&dev_attr_uuid.attr,
	&dev_attr_modalias.attr,
	NULL,
};
ATTRIBUTE_GROUPS(ffa_device_attributes);

const struct bus_type ffa_bus_type = {
	.name		= "arm_ffa",
	.match		= ffa_device_match,
	.probe		= ffa_device_probe,
	.remove		= ffa_device_remove,
	.uevent		= ffa_device_uevent,
	.dev_groups	= ffa_device_attributes_groups,
};
EXPORT_SYMBOL_GPL(ffa_bus_type);

int ffa_driver_register(struct ffa_driver *driver, struct module *owner,
			const char *mod_name)
{
	int ret = 0;

	if (!driver->probe)
		return -EINVAL;

	driver->driver.bus = &ffa_bus_type;
	driver->driver.name = driver->name;
	driver->driver.owner = owner;
	driver->driver.mod_name = mod_name;

	ret = driver_register(&driver->driver);
	if (!ret)
		pr_debug("registered new ffa driver %s\n", driver->name);

	return ret;
}
EXPORT_SYMBOL_GPL(ffa_driver_register);

void ffa_driver_unregister(struct ffa_driver *driver)
{
	driver_unregister(&driver->driver);
}
EXPORT_SYMBOL_GPL(ffa_driver_unregister);

static void ffa_release_device(struct device *dev)
{
	struct ffa_device *ffa_dev = to_ffa_dev(dev);

	ida_free(&ffa_bus_id, ffa_dev->id);
	kfree(ffa_dev);
}

static int __ffa_devices_unregister(struct device *dev, void *data)
{
	device_unregister(dev);

	return 0;
}

static void ffa_devices_unregister(void)
{
	bus_for_each_dev(&ffa_bus_type, NULL, NULL,
			 __ffa_devices_unregister);
}

bool ffa_device_is_valid(struct ffa_device *ffa_dev)
{
	bool valid = false;
	struct device *dev = NULL;
	struct ffa_device *tmp_dev = NULL;

	do {
		dev = bus_find_next_device(&ffa_bus_type, dev);
		tmp_dev = to_ffa_dev(dev);
		if (tmp_dev == ffa_dev) {
			valid = true;
			break;
		}
		put_device(dev);
	} while (dev);

	put_device(dev);

	return valid;
}

struct ffa_device *
ffa_device_register(const struct ffa_partition_info *part_info,
		    const struct ffa_ops *ops)
{
	int id = 0, ret = 0;
	uuid_t uuid = { 0U };
	struct device *dev = NULL;
	struct ffa_device *ffa_dev = NULL;

	if (!part_info)
		return NULL;

	id = ida_alloc_min(&ffa_bus_id, 1, GFP_KERNEL);
	if (id < 0)
		return NULL;

	ffa_dev = kzalloc(sizeof(*ffa_dev), GFP_KERNEL);
	if (!ffa_dev) {
		ida_free(&ffa_bus_id, id);
		return NULL;
	}

	dev = &ffa_dev->dev;
	dev->bus = &ffa_bus_type;
	dev->release = ffa_release_device;
	dev_set_name(&ffa_dev->dev, "arm-ffa-%d", id);

	ffa_dev->id = id;
	ffa_dev->vm_id = part_info->id;
	ffa_dev->properties = part_info->properties;
	ffa_dev->ops = ops;
	import_uuid(&uuid, (u8 *)part_info->uuid);
	uuid_copy(&ffa_dev->uuid, &uuid);

	ret = device_register(&ffa_dev->dev);
	if (ret) {
		dev_err(dev, "unable to register device %s err=%d\n",
			dev_name(dev), ret);
		put_device(dev);
		return NULL;
	}

	return ffa_dev;
}
EXPORT_SYMBOL_GPL(ffa_device_register);

void ffa_device_unregister(struct ffa_device *ffa_dev)
{
	if (!ffa_dev)
		return;

	device_unregister(&ffa_dev->dev);
}
EXPORT_SYMBOL_GPL(ffa_device_unregister);

#if defined(MTK_ADAPTED) && MTK_ADAPTED
int arm_ffa_bus_init(void)
#else
static int __init arm_ffa_bus_init(void)
#endif /* defined(MTK_ADAPTED) && MTK_ADAPTED */
{
	return bus_register(&ffa_bus_type);
}
#if !defined(MTK_ADAPTED) || !MTK_ADAPTED
subsys_initcall(arm_ffa_bus_init);
#endif /* !defined(MTK_ADAPTED) || !MTK_ADAPTED */

#if defined(MTK_ADAPTED) && MTK_ADAPTED
void arm_ffa_bus_exit(void)
#else
static void __exit arm_ffa_bus_exit(void)
#endif /* defined(MTK_ADAPTED) && MTK_ADAPTED */
{
	ffa_devices_unregister();
	bus_unregister(&ffa_bus_type);
	ida_destroy(&ffa_bus_id);
}
#if !defined(MTK_ADAPTED) || !MTK_ADAPTED
module_exit(arm_ffa_bus_exit);
#endif /* !defined(MTK_ADAPTED) || !MTK_ADAPTED */

MODULE_ALIAS("ffa-core");
MODULE_AUTHOR("Sudeep Holla <sudeep.holla@arm.com>");
MODULE_DESCRIPTION("ARM FF-A bus");
MODULE_LICENSE("GPL");
