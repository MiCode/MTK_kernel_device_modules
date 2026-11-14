// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include "aiste_debug.h"

#define AI_SYS_TUNE_DNAME "ai_sys_tune"

static struct dentry *aiste_debug_root;
u32 g_uclamp_min, g_uclamp_max, g_klog;

int aiste_procfs_init(void)
{
	aiste_debug_root = debugfs_create_dir(AI_SYS_TUNE_DNAME, NULL);
	if (IS_ERR_OR_NULL(aiste_debug_root)) {
		aiste_err("%s: failed to create debug dir %s\n", __func__, AI_SYS_TUNE_DNAME);
		return -EINVAL;
	}

	g_uclamp_min = 0;
	g_uclamp_max = 1024;
	g_klog = AISTE_DBG_ALL;
	debugfs_create_u32("uclamp_min", 0644, aiste_debug_root, &g_uclamp_min);
	debugfs_create_u32("uclamp_max", 0644, aiste_debug_root, &g_uclamp_max);
	debugfs_create_u32("klog", 0644, aiste_debug_root, &g_klog);

	return -ENOMEM;
}

void aiste_procfs_remove(void)
{
	if (!IS_ERR_OR_NULL(aiste_debug_root))
		debugfs_remove_recursive(aiste_debug_root);
}
