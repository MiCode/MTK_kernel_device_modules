// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/of_device.h>
#include <linux/types.h>
#include <linux/dma-direct.h>
#include <linux/dma-buf.h>
#include <uapi/linux/dma-buf.h>
#include <linux/list.h>
#include <linux/hashtable.h>
#include <mtk_heap.h>
#include <mtk-smmu-v3.h>

#include "apusys_core.h"
#include "apu_sysmem.h"
#include "apu_sysmem_drv.h"

#include "apu_sysmem_dbg.h"

uint32_t g_apu_sysmem_klog;	/* apu_sysmem kernel log level */

/* debug root node */
static struct dentry *apu_sysmem_dbg_root;

int apu_sysmem_dbg_init(struct dentry *apu_dbg_root)
{
	int ret = 0;

	g_apu_sysmem_klog = 0x3;

	/* create apu_sysmem FS root node */
	apu_sysmem_dbg_root = debugfs_create_dir("apu_sysmem", apu_dbg_root);
	if (IS_ERR_OR_NULL(apu_sysmem_dbg_root)) {
		ret = -EINVAL;
		apu_sysmem_err("failed to create debug dir.\n");
		goto fail;
	}

	/* create log level */
	debugfs_create_u32("klog", 0644,
			apu_sysmem_dbg_root, &g_apu_sysmem_klog);

	return ret;
fail:
	apu_sysmem_dbg_destroy();
	return ret;
}

void apu_sysmem_dbg_destroy(void)
{
	debugfs_remove_recursive(apu_sysmem_dbg_root);
}
