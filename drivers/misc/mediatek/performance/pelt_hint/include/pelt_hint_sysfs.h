/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef __CHI_SYSFS_H__
#define __CHI_SYSFS_H__

#include <linux/kobject.h>

#define CHI_SYSFS_MAX_BUFF_SIZE 2048

#define KOBJ_ATTR_RW(_name)	\
	struct kobj_attribute kobj_attr_##_name =	\
		__ATTR(_name, 0660,	\
		_name##_show, _name##_store)
#define KOBJ_ATTR_RO(_name)	\
	struct kobj_attribute kobj_attr_##_name =	\
		__ATTR(_name, 0440,	\
		_name##_show, NULL)

#define KOBJ_ATTR_WO(_name)	\
	struct kobj_attribute kobj_attr_##_name =	\
	__ATTR(_name, 0660, \
	NULL, _name##_store)

#define KOBJ_ATTR_RWO(_name)     \
	struct kobj_attribute kobj_attr_##_name =       \
		__ATTR(_name, 0664,     \
		_name##_show, _name##_store)
#define KOBJ_ATTR_ROO(_name)	\
	struct kobj_attribute kobj_attr_##_name =	\
		__ATTR(_name, 0444,	\
		_name##_show, NULL)

int chi_sysfs_create_dir(struct kobject *parent,
		const char *name, struct kobject **ppsKobj);
void chi_sysfs_remove_dir(struct kobject **ppsKobj);
void chi_sysfs_create_file(struct kobject *parent,
		struct kobj_attribute *kobj_attr);
void chi_sysfs_remove_file(struct kobject *parent,
		struct kobj_attribute *kobj_attr);
void chi_sysfs_init(void);
void chi_sysfs_exit(void);

extern struct kobject *kernel_kobj;

#endif
