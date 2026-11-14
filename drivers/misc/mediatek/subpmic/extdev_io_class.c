// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/property.h>

#include <linux/extdev_io_class.h>

#define PREALLOC_RBUFFER_SIZE	(32)
#define PREALLOC_WBUFFER_SIZE	(512)

static struct class *extdev_io_class;

static ssize_t extdev_io_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf);
static ssize_t extdev_io_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count);

#define EXTDEV_IO_DEVICE_ATTR(_name, _mode)		\
{							\
	.attr = { .name = #_name, .mode = _mode },	\
	.show = extdev_io_show,				\
	.store = extdev_io_store,			\
}

static struct device_attribute extdev_io_device_attributes[] = {
	EXTDEV_IO_DEVICE_ATTR(reg, 0644),
	EXTDEV_IO_DEVICE_ATTR(size, 0644),
	EXTDEV_IO_DEVICE_ATTR(data, 0644),
	EXTDEV_IO_DEVICE_ATTR(type, 0444),
	EXTDEV_IO_DEVICE_ATTR(lock, 0644),
};

enum {
	EXTDEV_IO_DESC_REG,
	EXTDEV_IO_DESC_SIZE,
	EXTDEV_IO_DESC_DATA,
	EXTDEV_IO_DESC_TYPE,
	EXTDEV_IO_DESC_LOCK,
};

static int extdev_io_read(struct extdev_io_device *extdev, char *buf)
{
	int cnt = 0, i, ret;
	void *buffer;
	u8 *data;
	struct extdev_desc *desc = extdev->desc;

	if (extdev->data_buffer_size < extdev->size) {
		buffer = kzalloc(extdev->size, GFP_KERNEL);
		if (!buffer)
			return -ENOMEM;
		kfree(extdev->data_buffer);
		extdev->data_buffer = buffer;
		extdev->data_buffer_size = extdev->size;
	}
	/* read transfer */
	if (desc->rmap)
		ret = regmap_bulk_read(desc->rmap, extdev->reg, extdev->data_buffer, extdev->size);
	else if (desc->io_read)
		ret = desc->io_read(desc->drvdata, extdev->reg, extdev->data_buffer, extdev->size);
	else
		ret = -EPERM;
	if (ret < 0)
		return ret;
	data = extdev->data_buffer;
	cnt = snprintf(buf + cnt, 256, "0x");
	if (cnt >= 256)
		return -ENOMEM;
	for (i = 0; i < extdev->size; i++) {
		cnt += snprintf(buf + cnt, 256, "%02x,", *(data + i));
		if (cnt >= 256)
			return -ENOMEM;
	}
	cnt = snprintf(buf + cnt, 256, "\n");
	if (cnt >= 256)
		return -ENOMEM;
	return ret;
}

static int extdev_io_write(struct extdev_io_device *extdev, const char *buf_internal, ssize_t cnt)
{
	void *buffer;
	u8 *pdata;
	char buf[PREALLOC_WBUFFER_SIZE + 1], *token, *cur;
	int val_cnt = 0, ret;
	struct extdev_desc *desc = extdev->desc;

	if (cnt > PREALLOC_WBUFFER_SIZE)
		return -ENOMEM;
	memcpy(buf, buf_internal, cnt);
	buf[cnt] = 0;
	/* buffer size check */
	if (extdev->data_buffer_size < extdev->size) {
		buffer = kzalloc(extdev->size, GFP_KERNEL);
		if (!buffer)
			return -ENOMEM;
		kfree(extdev->data_buffer);
		extdev->data_buffer = buffer;
		extdev->data_buffer_size = extdev->size;
	}
	/* data parsing */
	cur = buf;
	pdata = extdev->data_buffer;
	while ((token = strsep(&cur, ",\n")) != NULL) {
		if (!*token)
			break;
		if (val_cnt++ >= extdev->size)
			break;
		if (kstrtou8(token, 16, pdata++))
			return -EINVAL;
	}
	if (val_cnt != extdev->size)
		return -EINVAL;
	/* write transfer */
	if (desc->rmap)
		ret = regmap_bulk_write(desc->rmap, extdev->reg, extdev->data_buffer, extdev->size);
	else if (desc->io_write)
		ret = desc->io_write(desc->drvdata, extdev->reg, extdev->data_buffer, extdev->size);
	else
		ret = -EPERM;
	return (ret < 0) ? ret : cnt;
}

static ssize_t extdev_io_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	struct extdev_io_device *extdev = dev_get_drvdata(dev);
	const ptrdiff_t offset = attr - extdev_io_device_attributes;
	int ret = 0;

	mutex_lock(&extdev->io_lock);
	switch (offset) {
	case EXTDEV_IO_DESC_REG:
		ret = snprintf(buf, 256, "0x%04x\n", extdev->reg);
		if (ret >= 256)
			ret = -ENOMEM;
		break;
	case EXTDEV_IO_DESC_SIZE:
		ret = snprintf(buf, 256, "%d\n", extdev->size);
		if (ret >= 256)
			ret = -ENOMEM;
		break;
	case EXTDEV_IO_DESC_DATA:
		ret = extdev_io_read(extdev, buf);
		break;
	case EXTDEV_IO_DESC_TYPE:
		ret = snprintf(buf, 256, "%s\n", extdev->desc->typestr);
		if (ret >= 256)
			ret = -ENOMEM;
		break;
	case EXTDEV_IO_DESC_LOCK:
		ret = snprintf(buf, 256, "%d\n", extdev->access_lock);
		if (ret >= 256)
			ret = -ENOMEM;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	mutex_unlock(&extdev->io_lock);
	return ret < 0 ? ret : strlen(buf);
}

static int get_parameters(char *buf, long *param1, int num_of_par)
{
	char *token;
	int base, cnt;

	token = strsep(&buf, " ");

	for (cnt = 0; cnt < num_of_par; cnt++) {
		if (token != NULL) {
			if ((token[1] == 'x') || (token[1] == 'X'))
				base = 16;
			else
				base = 10;

			if (kstrtoul(token, base, &param1[cnt]) != 0)
				return -EINVAL;

			token = strsep(&buf, " ");
			}
		else
			return -EINVAL;
	}
	return 0;
}

static ssize_t extdev_io_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct extdev_io_device *extdev = dev_get_drvdata(dev);
	const ptrdiff_t offset = attr - extdev_io_device_attributes;
	long val = 0;
	int ret = 0;

	mutex_lock(&extdev->io_lock);
	switch (offset) {
	case EXTDEV_IO_DESC_REG:
		get_parameters((char *)buf, &val, 1);
		extdev->reg = val;
		break;
	case EXTDEV_IO_DESC_SIZE:
		get_parameters((char *)buf, &val, 1);
		extdev->size = val;
		break;
	case EXTDEV_IO_DESC_DATA:
		ret = extdev_io_write(extdev, buf, count);
		break;
	case EXTDEV_IO_DESC_LOCK:
		get_parameters((char *)buf, &val, 1);
		if (!!val == extdev->access_lock)
			ret = -EFAULT;
		else
			extdev->access_lock = !!val;
		break;
	default:
		ret = -EINVAL;
	}
	mutex_unlock(&extdev->io_lock);
	return ret < 0 ? ret : count;
}

struct extdev_io_device *extdev_io_device_register(struct device *parent,
						   const struct extdev_desc *desc)
{
	struct extdev_io_device *extdev;

	if (!parent) {
		pr_err("%s: Expected proper parent device\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	if (!desc || !desc->devname || !desc->typestr)
		return ERR_PTR(-EINVAL);

	extdev = devm_kzalloc(parent, sizeof(*extdev), GFP_KERNEL);
	if (!extdev)
		return ERR_PTR(-ENOMEM);

	extdev->desc = devm_kmemdup(parent, desc, sizeof(*desc), GFP_KERNEL);
	if (!extdev->desc)
		return ERR_PTR(-ENOMEM);

	mutex_init(&extdev->io_lock);

	/* for MTK engineer setting */
	extdev->size = 1;

	extdev->data_buffer_size = PREALLOC_RBUFFER_SIZE;
	extdev->data_buffer = kzalloc(PREALLOC_RBUFFER_SIZE, GFP_KERNEL);
	if (!extdev->data_buffer) {
		kfree(extdev);
		return ERR_PTR(-ENOMEM);
	}

	extdev->dev = device_create_with_groups(extdev_io_class, parent, 0, extdev, NULL, "%s",
						desc->dirname);
	if (IS_ERR(extdev->dev)) {
		kfree(extdev->data_buffer);
		return ERR_CAST(extdev->dev);
	}

	return extdev;
}
EXPORT_SYMBOL_GPL(extdev_io_device_register);

void extdev_io_device_unregister(struct extdev_io_device *extdev)
{
	device_unregister(extdev->dev);
	kfree(extdev->data_buffer);
	mutex_destroy(&extdev->io_lock);
}
EXPORT_SYMBOL_GPL(extdev_io_device_unregister);

static void devm_extdev_io_device_release(struct device *dev, void *res)
{
	struct extdev_io_device **extdev = res;

	extdev_io_device_unregister(*extdev);
}

struct extdev_io_device *devm_extdev_io_device_register(struct device *parent,
							const struct extdev_desc *desc)
{
	struct extdev_io_device **ptr, *extdev;

	ptr = devres_alloc(devm_extdev_io_device_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	extdev = extdev_io_device_register(parent, desc);
	if (IS_ERR(extdev)) {
		devres_free(ptr);
	} else {
		*ptr = extdev;
		devres_add(parent, ptr);
	}

	return extdev;
}
EXPORT_SYMBOL_GPL(devm_extdev_io_device_register);

static void __maybe_unused extdev_io_pdev_unregister(void *data)
{
	struct platform_device *pdev = data;

	of_platform_device_destroy(&pdev->dev, NULL);
}

#define EXTDEV_IO_PDEV_NAME_SIZE		64
int devm_extdev_io_device_general_create(struct device *parent, struct platform_device **pdev,
					 struct extdev_io_device **extdev, struct regmap *rmap)
{
	const char *compatilble_name, *bus_name, *chip_name;
	char pdev_name[EXTDEV_IO_PDEV_NAME_SIZE] = "\0";
	struct extdev_desc extdev_desc = { 0 };
	size_t buf_size = sizeof(pdev_name);
	struct fwnode_handle *dbg_fwnode;
	int ret = 0, offset = 0;

	if (!parent) {
		pr_info("%s, No parent device\n", __func__);
		return -ENODEV;
	}

	dbg_fwnode = device_get_named_child_node(parent, "rtk-generic-dbg");
	if (!dbg_fwnode) {
		dev_info(parent, "%s, Failed to get rtk-generic-dbg dts node\n", __func__);
		return -ENODEV;
	}

	if (!pdev) {
		dev_info(parent, "%s, No platform device!", __func__);
		goto create_by_extdev_io_class;
	}

	/* 1st Method: Driver probe in subpmic-dbg */
	if (!fwnode_property_read_string(dbg_fwnode, "compatible", &compatilble_name)) {
		offset += scnprintf(pdev_name + offset, buf_size - offset,
				    "%s-dbg", dev_name(parent));

		*pdev = of_platform_device_create(to_of_node(dbg_fwnode), pdev_name, parent);
		if (*pdev) {
			ret = devm_add_action_or_reset(parent, extdev_io_pdev_unregister, *pdev);
			if (ret) {
				dev_info(parent, "%s, Failed to create devm pdev!\n", __func__);
				goto out;
			}

			dev_info(parent, "%s, Create %s dbg platform device successfully!",
				 __func__, pdev_name);
			goto out;
		} else {
			dev_info(parent, "%s, Failed to create %s\n", __func__, pdev_name);
			ret = -ENOMEM;
			goto out;
		}
	}

create_by_extdev_io_class:
	/* 2nd: Only create extdev_io by extdev_io class (without subpmic-dbg) */
	if (!rmap || !extdev) {
		dev_info(parent, "%s, No regmap, or no extdev!!\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	ret = fwnode_property_read_string(dbg_fwnode, "dbg-bus", &bus_name);
	if (ret) {
		dev_info(parent, "%s, Failed to get dbg-bus dts node!!\n", __func__);
		goto out;
	}

	ret = fwnode_property_read_string(dbg_fwnode, "dbg-name", &chip_name);
	if (ret) {
		dev_info(parent, "%s, Failed to get dbg-name dts node!!\n", __func__);
		goto out;
	}

	extdev_desc.dirname = devm_kasprintf(parent, GFP_KERNEL, "%s.%s",
					     chip_name, dev_name(parent));
	extdev_desc.devname = dev_name(parent);
	extdev_desc.typestr = devm_kasprintf(parent, GFP_KERNEL, "%s,%s", bus_name, chip_name);
	extdev_desc.rmap = rmap;
	*extdev = devm_extdev_io_device_register(parent, &extdev_desc);
	if (IS_ERR(*extdev)) {
		dev_info(parent, "%s, Failed to register extdev_io device\n", __func__);
		ret = PTR_ERR(*extdev);
	} else
		ret = 0;

out:
	fwnode_handle_put(dbg_fwnode);
	return ret;
}
EXPORT_SYMBOL_GPL(devm_extdev_io_device_general_create);

static struct attribute *extdev_io_class_attrs[] = {
	&extdev_io_device_attributes[0].attr,
	&extdev_io_device_attributes[1].attr,
	&extdev_io_device_attributes[2].attr,
	&extdev_io_device_attributes[3].attr,
	&extdev_io_device_attributes[4].attr,
	NULL,
};

static const struct attribute_group extdev_io_attr_group = {
	.attrs = extdev_io_class_attrs,
};

static const struct attribute_group *extdev_io_attr_groups[] = {
	&extdev_io_attr_group,
	NULL
};

static int __init extdev_io_class_init(void)
{
	pr_info("%s starting\n", __func__);
	extdev_io_class = class_create("extdev_io");
	if (IS_ERR(extdev_io_class)) {
		pr_err("Unable to create extdev_io class(%ld)\n", PTR_ERR(extdev_io_class));
		return PTR_ERR(extdev_io_class);
	}

	extdev_io_class->dev_groups = extdev_io_attr_groups;
	pr_info("extdev_io class init OK\n");
	return 0;
}

static void __exit extdev_io_class_exit(void)
{
	class_destroy(extdev_io_class);
	pr_info("extdev_io class deinit OK\n");
}

subsys_initcall(extdev_io_class_init);
module_exit(extdev_io_class_exit);

MODULE_DESCRIPTION("Extdev io class");
MODULE_AUTHOR("Gene Chen <gene_chen@richtek.com>");
MODULE_LICENSE("GPL");
