// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include "pelt_hint_sysfs.h"

#define CHI_SYSFS_DIR_NAME "chi"

static struct kobject *chi_kobj;

int chi_sysfs_create_dir(struct kobject *parent,
		const char *name, struct kobject **ppsKobj)
{
	struct kobject *psKobj = NULL;

	if (name == NULL || ppsKobj == NULL) {
		pr_debug("Failed to create sysfs directory %p %p\n",
				name, ppsKobj);
		return -1;
	}

	parent = (parent != NULL) ? parent : chi_kobj;
	psKobj = kobject_create_and_add(name, parent);
	if (!psKobj) {
		pr_debug("Failed to create '%s' sysfs directory\n",
				name);
		return -1;
	}
	*ppsKobj = psKobj;

	return 0;
}

void chi_sysfs_remove_dir(struct kobject **ppsKobj)
{
	if (ppsKobj == NULL)
		return;
	kobject_put(*ppsKobj);
	*ppsKobj = NULL;
}

void chi_sysfs_create_file(struct kobject *parent,
		struct kobj_attribute *kobj_attr)
{
	if (kobj_attr == NULL) {
		pr_debug("Failed to create '%s' sysfs file kobj_attr=NULL\n", CHI_SYSFS_DIR_NAME);
		return;
	}

	parent = (parent != NULL) ? parent : chi_kobj;
	if (sysfs_create_file(parent, &(kobj_attr->attr))) {
		pr_debug("Failed to create sysfs file\n");
		return;
	}

}

void chi_sysfs_remove_file(struct kobject *parent,
		struct kobj_attribute *kobj_attr)
{
	if (kobj_attr == NULL)
		return;
	parent = (parent != NULL) ? parent : chi_kobj;
	sysfs_remove_file(parent, &(kobj_attr->attr));
}

void chi_sysfs_init(void)
{
	chi_kobj = kobject_create_and_add(CHI_SYSFS_DIR_NAME,
			kernel_kobj);
	if (!chi_kobj) {
		pr_debug("Failed to create '%s' sysfs root directory\n",
				CHI_SYSFS_DIR_NAME);
	}
}

void chi_sysfs_exit(void)
{
	kobject_put(chi_kobj);
	chi_kobj = NULL;
}
