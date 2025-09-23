// SPDX-License-Identifier: GPL-2.0
/*
 * mca_sysfs.c
 *
 * mca sysfs driver
 *
 * Copyright (c) 2023-2023 Xiaomi Technologies Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */
#include <linux/mca/common/mca_sysfs.h>
#include <linux/version.h>

#define MCA_SYSFS_DEFAULT_DEV_NUM 5

#define MCA_DBG_ROOT_DIR	"charger_debug"

static LIST_HEAD(g_mca_sysfs_list_header);
static DEFINE_MUTEX(g_mca_sysfs_class_lock);

const char *g_mca_sysfs_dev_name_list[MCA_SYSFS_DEFAULT_DEV_NUM] = {
	"charger",
	"fuelgauge",
	"typec",
	"battery",
	"hw_monitor",
};

static struct mca_sysfs_class_node *mca_sysfs_get_class_node(const char *cls_name)
{
	struct list_head *header = &g_mca_sysfs_list_header;
	struct mca_sysfs_class_node *l_node = NULL;

	if (list_empty(header))
		return NULL;

	list_for_each_entry(l_node, header, node) {
		if (!strcmp(l_node->class_name, cls_name))
			return l_node;
	}

	return NULL;
}

static struct mca_sysfs_class_node *mca_sysfs_get_or_create_class(const char *cls_name)
{
	struct list_head *header = &g_mca_sysfs_list_header;
	struct mca_sysfs_class_node *l_node = NULL;

	mutex_lock(&g_mca_sysfs_class_lock);
	l_node = mca_sysfs_get_class_node(cls_name);
	if (l_node)
		goto find_node;

	l_node = kmalloc(sizeof(*l_node), GFP_KERNEL);
	if (!l_node)
		goto failed;

	l_node->class_name = cls_name;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 9))
	l_node->mca_class = class_create(cls_name);
#else
	l_node->mca_class = class_create(THIS_MODULE, cls_name);
#endif
	if (!l_node->mca_class) {
		kfree(l_node);
		goto failed;
	}
	INIT_LIST_HEAD(&l_node->dev_list_header);
	list_add_tail(&l_node->node, header);

find_node:
	mutex_unlock(&g_mca_sysfs_class_lock);
	return l_node;
failed:
	mutex_unlock(&g_mca_sysfs_class_lock);
	return NULL;
}

static struct device *mca_sysfs_get_device(struct mca_sysfs_class_node *class_node, const char *name)
{
	struct mca_sysfs_dev_node *dev_node = NULL;

	if (list_empty(&class_node->dev_list_header))
		return NULL;

	list_for_each_entry(dev_node, &class_node->dev_list_header, node) {
		if (!strcmp(dev_node->name, name))
			return dev_node->dev;
	}

	return NULL;
}

struct device *mca_sysfs_create_group(const char *cls_name,
	const char *dev_name, const struct attribute_group *group)
{
	struct device *l_device = NULL;
	struct mca_sysfs_class_node *class_node = NULL;
	struct mca_sysfs_dev_node *dev_node = NULL;

	if (!cls_name || !dev_name || !group)
		return NULL;

	class_node = mca_sysfs_get_or_create_class(cls_name);
	if (!class_node) {
		pr_err("can not find class %s", cls_name);
		return NULL;
	}

	l_device = device_create(class_node->mca_class, NULL, 0, NULL, "%s", dev_name);
	if (!l_device) {
		pr_err("create device %s fai\n", dev_name);
		return NULL;
	}

	if (sysfs_create_group(&l_device->kobj, group)) {
		pr_err("%s creat sys group fail\n", dev_name);
		goto error;
	}

	dev_node = kmalloc(sizeof(*dev_node), GFP_KERNEL);
	if (!dev_node)
		goto error;
	dev_node->name = dev_name;
	dev_node->dev = l_device;
	mutex_lock(&g_mca_sysfs_class_lock);
	list_add_tail(&dev_node->node, &class_node->dev_list_header);
	mutex_unlock(&g_mca_sysfs_class_lock);
	pr_info("group %s/%s create succ\n", cls_name, dev_name);
	return l_device;

error:
	put_device(l_device);
	device_unregister(l_device);
	return NULL;
}
EXPORT_SYMBOL(mca_sysfs_create_group);

void mca_sysfs_remove_group(const char *cls_name, struct device *dev,
	const struct attribute_group *group)
{
	struct mca_sysfs_class_node *class_node = NULL;
	struct mca_sysfs_dev_node *dev_node, *temp_node;

	if (!dev || !group)
		return;

	sysfs_remove_group(&dev->kobj, group);
	put_device(dev);
	class_node = mca_sysfs_get_class_node(cls_name);
	if (!class_node && !list_empty(&class_node->dev_list_header)) {
		list_for_each_entry_safe(dev_node, temp_node, &class_node->dev_list_header, node) {
			if (dev_node->dev == dev) {
				list_del(&dev_node->node);
				kfree(dev_node);
				break;
			}
		}

	}
	device_unregister(dev);
}
EXPORT_SYMBOL(mca_sysfs_remove_group);

int mca_sysfs_create_link_group(const char *dev_name,
	const char *link_name, struct device *target_dev,
	const struct attribute_group *group)
{
	struct device *dev;
	struct mca_sysfs_class_node *class_node = NULL;

	if (!dev_name || !link_name || !target_dev || !group)
		return -1;

	class_node = mca_sysfs_get_class_node("xm_power");
	if (!class_node || !class_node->mca_class)
		return -1;

	dev = mca_sysfs_get_device(class_node, dev_name);
	if (!dev)
		return -1;

	if (sysfs_create_group(&target_dev->kobj, group))
		return -1;

	if (sysfs_create_link(&dev->kobj, &target_dev->kobj, link_name))
		return -1;

	pr_info("creat %s/%s success\n", dev_name, link_name);
	return 0;
}
EXPORT_SYMBOL(mca_sysfs_create_link_group);

void mca_sysfs_remove_link_group(const char *dev_name,
	const char *link_name, struct device *target_dev,
	const struct attribute_group *group)
{
	struct device *dev;
	struct mca_sysfs_class_node *class_node = NULL;

	if (!dev_name || !link_name || !target_dev || !group)
		return;

	class_node = mca_sysfs_get_class_node("xm_power");
	if (!class_node || !class_node->mca_class)
		return;

	dev = mca_sysfs_get_device(class_node, dev_name);
	if (!dev)
		return;

	sysfs_remove_link(&dev->kobj, link_name);
	sysfs_remove_group(&target_dev->kobj, group);
}
EXPORT_SYMBOL(mca_sysfs_remove_link_group);

int mca_sysfs_create_files(const char *dev_name,
	struct mca_sysfs_attr_info *attr, const int attr_size)
{
	struct device *dev;
	struct mca_sysfs_class_node *class_node = NULL;
	int ret = 0;

	if (!dev_name || !attr)
		return -1;

	class_node = mca_sysfs_get_class_node("xm_power");
	if (!class_node || !class_node->mca_class)
		return -1;

	dev = mca_sysfs_get_device(class_node, dev_name);
	if (!dev)
		return -1;

	for (int i = 0; i < attr_size; i++) {
		ret = sysfs_create_file(&dev->kobj, &attr[i].attr.attr);
		if (ret) {
			pr_err("creat %s file failed\n", attr[i].attr.attr.name);
			sysfs_remove_file(&dev->kobj, &attr[i].attr.attr);
		}
	}
	return ret;
}
EXPORT_SYMBOL(mca_sysfs_create_files);

void mca_sysfs_init_attrs(struct attribute **attrs,
	struct mca_sysfs_attr_info *attr_info, int size)
{
	int i;

	if (!attrs || !attr_info)
		return;

	for (i = 0; i < size; i++)
		attrs[i] = &attr_info[i].attr.attr;

	attrs[size] = NULL;
}
EXPORT_SYMBOL(mca_sysfs_init_attrs);

struct mca_sysfs_attr_info *mca_sysfs_lookup_attr(const char *name,
	struct mca_sysfs_attr_info *attr_info, int size)
{
	int i;

	if (!name || !attr_info)
		return NULL;

	for (i = 0; i < size; i++) {
		if (!strcmp(name, attr_info[i].attr.attr.name))
			return &attr_info[i];
	}

	return NULL;
}
EXPORT_SYMBOL(mca_sysfs_lookup_attr);

static int mca_sysfs_init(void)
{
	struct mca_sysfs_dev_node *dev_node = NULL;
	struct mca_sysfs_class_node *class_node = NULL;
	int i;

	class_node = mca_sysfs_get_or_create_class("xm_power");
	if (!class_node)
		return -1;

	for (i = 0; i < MCA_SYSFS_DEFAULT_DEV_NUM; i++) {
		if (!dev_node) {
			dev_node = kmalloc(sizeof(*dev_node), GFP_KERNEL);
			if (!dev_node)
				return 0;
		}
		dev_node->name = g_mca_sysfs_dev_name_list[i];
		dev_node->dev = device_create(class_node->mca_class, NULL, 0, NULL, "%s", dev_node->name);
		if (!dev_node->dev) {
			pr_err("create device %s fail\n", dev_node->name);
			continue;
		}
		mutex_lock(&g_mca_sysfs_class_lock);
		list_add_tail(&dev_node->node, &class_node->dev_list_header);
		mutex_unlock(&g_mca_sysfs_class_lock);
		dev_node = NULL;
	}

	if (!dev_node)
		kfree(dev_node);

	return 0;
}

static struct dentry *g_mca_dbg_dir;
static DEFINE_MUTEX(g_mca_debugfs_dir_lock);

#ifdef CONFIG_DEBUG_FS
static int mca_debugfs_template_show(struct seq_file *s, void *d)
{
	struct mca_debugfs_attr_data *pattr = s->private;
	char *buf = NULL;
	int ret;

	if (!pattr || !pattr->attr_info->show) {
		pr_err("invalid show\n");
		return -EINVAL;
	}

	buf = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = pattr->attr_info->show(pattr, buf);
	seq_write(s, buf, ret);
	kfree(buf);

	pr_err("show:%s ret=%d\n", pattr->attr_info->file, ret);
	return 0;
}

static ssize_t mca_debugfs_template_write(struct file *file,
	const char __user *data, size_t size, loff_t *ppos)
{
	struct mca_debugfs_attr_data *pattr = NULL;
	char *buf = NULL;
	int ret;

	pattr = ((struct seq_file *)file->private_data)->private;
	if (!pattr || !pattr->attr_info->store) {
		pr_err("invalid store\n");
		return -EINVAL;
	}

	buf = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (size >= PAGE_SIZE) {
		pr_err("input too long\n");
		kfree(buf);
		return -ENOMEM;
	}

	if (copy_from_user(buf, data, size)) {
		pr_err("can not copy data form user to kernel\n");
		kfree(buf);
		return -ENOSPC;
	}
	buf[size] = '\0';

	ret = pattr->attr_info->store(pattr, buf, size);
	kfree(buf);

	pr_err("store: %s ret=%d\n", pattr->attr_info->file, ret);
	return ret;
}

static int mca_debugfs_template_open(struct inode *inode, struct file *file)
{
	return single_open(file, mca_debugfs_template_show, inode->i_private);
}

static const struct file_operations mca_debugfs_template_fops = {
	.open = mca_debugfs_template_open,
	.read = seq_read,
	.write = mca_debugfs_template_write,
	.release = single_release,
};

static struct dentry *mca_debugfs_get_or_create_dir(const char *dir_name)
{
	struct dentry *dir = NULL;

	if (!g_mca_dbg_dir) {
		pr_err("root directory is null");
		return NULL;
	}

	mutex_lock(&g_mca_debugfs_dir_lock);
	dir = debugfs_lookup(dir_name, g_mca_dbg_dir);
	if (IS_ERR_OR_NULL(dir))
		dir = debugfs_create_dir(dir_name, g_mca_dbg_dir);
	if (IS_ERR_OR_NULL(dir)) {
		pr_err("%s dir create fail", dir_name);
		dir = NULL;
	}
	mutex_unlock(&g_mca_debugfs_dir_lock);
	return dir;
}

int mca_debugfs_create_group(const char *dir_name,
	struct mca_debugfs_attr_info *attr_info, const int attr_size, void *dev_data)
{
	struct dentry *dir = NULL;
	struct dentry *file = NULL;
	struct mca_debugfs_attr_data *attr_data = NULL;

	dir = mca_debugfs_get_or_create_dir(dir_name);
	if (!dir)
		return -1;

	attr_data = kcalloc(attr_size, sizeof(*attr_data), GFP_KERNEL);
	if (!attr_data)
		return -1;

	for (int i = 0; i < attr_size; i++) {
		attr_data[i].attr_info = &attr_info[i];
		attr_data[i].private = dev_data;
		file = debugfs_create_file(attr_info[i].file, attr_info[i].mode,
			dir, &attr_data[i], &mca_debugfs_template_fops);
		if (file == NULL) {
			pr_err("%s file create fail", attr_info[i].file);
			kfree(attr_data);
			return -1;
		}
	}
	return 0;
}
EXPORT_SYMBOL(mca_debugfs_create_group);

static int mca_debugfs_init(void)
{
	int rc = 0;

	g_mca_dbg_dir = debugfs_create_dir(MCA_DBG_ROOT_DIR, NULL);
	if (IS_ERR(g_mca_dbg_dir)) {
		rc = PTR_ERR(g_mca_dbg_dir);
		pr_err("Failed to create charger debugfs root directory: %d\n", rc);
	}
	return rc;
}
#else
int mca_debugfs_create_group(const char *dir_name,
	struct mca_debugfs_attr_info *attr_info, const int attr_size, void *dev_data)
{
	return 0;
}
EXPORT_SYMBOL(mca_debugfs_create_group);

static int mca_debugfs_init(void)
{
	return 0;
}
#endif

static int __init mca_fs_init(void)
{
	mca_debugfs_init();
	mca_sysfs_init();
	return 0;
}

static void __exit mca_fs_exit(void)
{
	debugfs_remove_recursive(g_mca_dbg_dir);
}

module_init(mca_fs_init);
module_exit(mca_fs_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("sysfs for xiaomi power driver");
MODULE_AUTHOR("liyuze1@xiaomi.com");

