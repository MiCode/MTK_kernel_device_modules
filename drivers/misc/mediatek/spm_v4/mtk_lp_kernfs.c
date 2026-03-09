// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/kernfs.h>

#include <mtk_lp_kernfs.h>
#include <mtk_lp_sysfs.h>

#define MTK_LP_SYSFS_POWER_BUFFER_SZ	8192

#define LP_SYSFS_STATUS_INITIAL			0
#define LP_SYSFS_STATUS_READY			(1<<0)
#define LP_SYSFS_STATUS_READ_MORE		(1<<1)
#define LP_SYSFS_STATUS_IDIO_TYPE		(1<<2)

struct mtk_lp_kernfs_info {
	int status;
	struct mutex locker;
};

#define MTK_LP_INFO_SZ	sizeof(struct mtk_lp_kernfs_info)

int mtk_lp_kernfs_create_file(struct kernfs_node *parent,
				  struct kernfs_node **node,
				  unsigned int flag,
				  const char *name, umode_t mode,
				  void *attr)
{
	return 0;
}

int mtk_lp_kernfs_remove_file(struct kernfs_node *node)
{
	return 0;
}
EXPORT_SYMBOL(mtk_lp_kernfs_remove_file);

int mtk_lp_kernfs_create_group(struct kobject *kobj
						, struct attribute_group *grp)
{
	return 0;
}

size_t get_mtk_lp_kernfs_bufsz_max(void)
{
	return MTK_LP_SYSFS_POWER_BUFFER_SZ;
}

