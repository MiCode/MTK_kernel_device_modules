// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include "pelt_hint_sysfs.h"

#define PELT_HINT_SYSFS_DIR_NAME "pelt_hint"

static struct kobject *pelt_hint_kobj;

int pelt_hint_sysfs_create_dir(struct kobject *parent,
		const char *name, struct kobject **ppsKobj)
{
	struct kobject *psKobj = NULL;

	if (name == NULL || ppsKobj == NULL) {
		pr_debug("Failed to create sysfs directory %p %p\n",
				name, ppsKobj);
		return -1;
	}

	parent = (parent != NULL) ? parent : pelt_hint_kobj;
	psKobj = kobject_create_and_add(name, parent);
	if (!psKobj) {
		pr_debug("Failed to create '%s' sysfs directory\n",
				name);
		return -1;
	}
	*ppsKobj = psKobj;

	return 0;
}

void pelt_hint_sysfs_remove_dir(struct kobject **ppsKobj)
{
	if (ppsKobj == NULL)
		return;
	kobject_put(*ppsKobj);
	*ppsKobj = NULL;
}

void pelt_hint_sysfs_create_file(struct kobject *parent,
		struct kobj_attribute *kobj_attr)
{
	if (kobj_attr == NULL) {
		pr_debug("Failed to create '%s' sysfs file kobj_attr=NULL\n", PELT_HINT_SYSFS_DIR_NAME);
		return;
	}

	parent = (parent != NULL) ? parent : pelt_hint_kobj;
	if (sysfs_create_file(parent, &(kobj_attr->attr))) {
		pr_debug("Failed to create sysfs file\n");
		return;
	}

}

void pelt_hint_sysfs_remove_file(struct kobject *parent,
		struct kobj_attribute *kobj_attr)
{
	if (kobj_attr == NULL)
		return;
	parent = (parent != NULL) ? parent : pelt_hint_kobj;
	sysfs_remove_file(parent, &(kobj_attr->attr));
}

void pelt_hint_sysfs_init(void)
{
	pelt_hint_kobj = kobject_create_and_add(PELT_HINT_SYSFS_DIR_NAME,
			kernel_kobj);
	if (!pelt_hint_kobj) {
		pr_debug("Failed to create '%s' sysfs root directory\n",
				PELT_HINT_SYSFS_DIR_NAME);
	}
}

void pelt_hint_sysfs_exit(void)
{
	kobject_put(pelt_hint_kobj);
	pelt_hint_kobj = NULL;
}
