/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef _UFS_MEDIATEK_SYSFS_H
#define _UFS_MEDIATEK_SYSFS_H

void ufs_mtk_init_sysfs(struct ufs_hba *hba);
void ufs_mtk_remove_sysfs(struct ufs_hba *hba);

/*
 * Clock Scaling operation
 */
enum {
	CLK_SCALE_OP_FREE_RUN           = 0,
	CLK_SCALE_OP_SCALE_DOWN         = 1,
	CLK_SCALE_OP_SCALE_UP           = 2,
	CLK_SCALE_OP_SCALE_DOWN_LOCK    = 3,
	CLK_SCALE_OP_SCALE_UP_LOCK      = 4,
	CLK_SCALE_OP_FREE_RUN_UNLOCK    = 5
};

#endif /* _UFS_MEDIATEK_SYSFS_H */

