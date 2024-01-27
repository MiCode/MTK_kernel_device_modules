/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef _UFS_MEDIATEK_RPMB_H
#define _UFS_MEDIATEK_RPMB_H

#if IS_ENABLED(CONFIG_RPMB)

#include <linux/rpmb.h>

void ufs_mtk_rpmb_init(struct ufs_hba *hba);
struct rpmb_dev *ufs_mtk_rpmb_get_raw_dev(void);

#else

#define ufs_mtk_rpmb_get_raw_dev(...)
#define ufs_mtk_rpmb_init(...)

#endif /* CONFIG_RPMB */

#endif /* _UFS_MEDIATEK_RPMB_H */

