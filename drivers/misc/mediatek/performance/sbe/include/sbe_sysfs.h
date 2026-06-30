/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef __SBE_SYSFS_H__
#define __SBE_SYSFS_H__

#include <linux/kobject.h>

#if IS_ENABLED(CONFIG_ARM64)
#define SBE_SYSFS_MAX_BUFF_SIZE 2048
#else
#define SBE_SYSFS_MAX_BUFF_SIZE 800
#endif

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

int sbe_sysfs_create_dir(struct kobject *parent,
		const char *name, struct kobject **ppsKobj);
void sbe_sysfs_remove_dir(struct kobject **ppsKobj);
void sbe_sysfs_create_file(struct kobject *parent,
		struct kobj_attribute *kobj_attr);
void sbe_sysfs_remove_file(struct kobject *parent,
		struct kobj_attribute *kobj_attr);
void sbe_sysfs_init(void);
void sbe_sysfs_exit(void);

extern struct kobject *kernel_kobj;

#endif
