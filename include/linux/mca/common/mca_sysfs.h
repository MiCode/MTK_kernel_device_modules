/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mca_sysfs.h
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
#ifndef _MCA_SYSFS_H_
#define _MCA_SYSFS_H_

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/slab.h>
#include <linux/debugfs.h>

#define SYSFS_DEV_1 "charger"
#define SYSFS_DEV_2 "fuelgauge"
#define SYSFS_DEV_3 "typec"
#define SYSFS_DEV_4 "battery"
#define SYSFS_DEV_5 "hw_monitor"

struct mca_sysfs_dev_node {
	struct list_head node;
	const char *name;
	struct device *dev;
};

struct mca_sysfs_class_node {
	struct list_head node;
	const char *class_name;
	struct class *mca_class;
	struct list_head dev_list_header;
};

struct mca_sysfs_attr_info {
	struct device_attribute attr;
	int sysfs_attr_name;
};

#define mca_sysfs_attr_ro(_func, _mode, _type, _name) \
{ \
	.attr = __ATTR(_name, _mode, _func##_show, NULL), \
	.sysfs_attr_name = _type, \
}

#define mca_sysfs_attr_wo(_func, _mode, _type, _name) \
{ \
	.attr = __ATTR(_name, _mode, NULL, _func##_store), \
	.sysfs_attr_name = _type, \
}

#define mca_sysfs_attr_rw(_func, _mode, _type, _name) \
{ \
	.attr = __ATTR(_name, _mode, _func##_show, _func##_store), \
	.sysfs_attr_name = _type, \
}

typedef ssize_t (*mca_debugfs_show)(void *, char *);
typedef ssize_t (*mca_debugfs_store)(void *, const char *, size_t);

struct mca_debugfs_attr_info {
	const char *file;
	unsigned short mode;
	int debugfs_attr_name;
	mca_debugfs_show show;
	mca_debugfs_store store;
};

struct mca_debugfs_attr_data {
	struct mca_debugfs_attr_info *attr_info;
	void *private;
};

#define mca_debugfs_attr(_func, _mode, _type, _name) \
{ \
	.file = __stringify(_name), \
	.mode = _mode, \
	.debugfs_attr_name = _type, \
	.show = _func##_show, \
	.store = _func##_store, \
}

extern int mca_debugfs_create_group(const char *dir_name,
	struct mca_debugfs_attr_info *attr_info, const int attr_size, void *dev_data);
extern struct device *mca_sysfs_create_group(const char *cls_name,
	const char *dev_name, const struct attribute_group *group);
extern void mca_sysfs_remove_group(const char *cls_name, struct device *dev,
	const struct attribute_group *group);
extern int mca_sysfs_create_link_group(const char *dev_name,
	const char *link_name, struct device *target_dev,
	const struct attribute_group *group);
extern void mca_sysfs_remove_link_group(const char *dev_name,
	const char *link_name, struct device *target_dev,
	const struct attribute_group *group);
extern void mca_sysfs_init_attrs(struct attribute **attrs,
	struct mca_sysfs_attr_info *attr_info, int size);
extern struct mca_sysfs_attr_info *mca_sysfs_lookup_attr(const char *name,
	struct mca_sysfs_attr_info *attr_info, int size);
extern int mca_sysfs_create_files(const char *dev_name,
	struct mca_sysfs_attr_info *attr, const int attr_size);

#endif /* _MCA_SYSFS_H_ */

