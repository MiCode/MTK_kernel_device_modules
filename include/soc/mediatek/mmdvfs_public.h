/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef MTK_MMDVFS_PUBLIC_H
#define MTK_MMDVFS_PUBLIC_H

enum {
	MMDVFS_VER_V3,
	MMDVFS_VER_V35,
	MMDVFS_VER_V5,

	MMDVFS_VER_NUM
};

#if IS_ENABLED(CONFIG_MTK_MMDVFS) && IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
int mtk_mmdvfs_enable_vcp(const bool enable, const u8 idx);
int mmdvfs_get_version(void);
#else
static inline int mtk_mmdvfs_enable_vcp(const bool enable, const u8 idx) { return 0; }
static inline int mmdvfs_get_version(void) { return 0; }
#endif

#endif /* MTK_MMDVFS_PUBLIC_H */
