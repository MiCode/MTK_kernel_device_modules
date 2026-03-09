// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include "game_sysfs.h"

#define FI_SYSFS_DIR_NAME "game"

static struct kobject *game_kobj;

int game_get_sysfs_dir(struct kobject **ppsKobj)
{
	if (game_kobj) {
		*ppsKobj = game_kobj;
		return 0;
	} else
		return 1;
}
// --------------------------------------------------
void game_sysfs_create_file(struct kobject *parent,
		struct kobj_attribute *kobj_attr)
{
	if (kobj_attr == NULL) {
		pr_debug("Failed to create '%s' sysfs file kobj_attr=NULL\n",
			FI_SYSFS_DIR_NAME);
		return;
	}

	parent = (parent != NULL) ? parent : game_kobj;
	if (sysfs_create_file(parent, &(kobj_attr->attr))) {
		pr_debug("Failed to create sysfs file\n");
		return;
	}

}
// --------------------------------------------------
void game_sysfs_remove_file(struct kobject *parent,
		struct kobj_attribute *kobj_attr)
{
	if (kobj_attr == NULL)
		return;
	parent = (parent != NULL) ? parent : game_kobj;
	sysfs_remove_file(parent, &(kobj_attr->attr));
}
// --------------------------------------------------
void game_sysfs_init(void)
{
	game_kobj = kobject_create_and_add(FI_SYSFS_DIR_NAME,
			kernel_kobj);
	if (!game_kobj) {
		pr_debug("Failed to create '%s' sysfs root directory\n",
				FI_SYSFS_DIR_NAME);
	}

}
// --------------------------------------------------
void game_sysfs_exit(void)
{
	kobject_put(game_kobj);
	game_kobj = NULL;
}
