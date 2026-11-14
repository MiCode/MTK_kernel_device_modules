/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __APUSYS_APU_SYSMEM_DEBUG_H__
#define __APUSYS_APU_SYSMEM_DEBUG_H__

#include <linux/debugfs.h>

int apu_sysmem_dbg_init(struct dentry *apu_dbg_root);
void apu_sysmem_dbg_destroy(void);

#endif
