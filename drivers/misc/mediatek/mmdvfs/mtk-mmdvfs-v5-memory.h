/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Anthony Huang <anthony.huang@mediatek.com>
 */

#ifndef _MTK_MMDVFS_V5_MEMORY_H_
#define _MTK_MMDVFS_V5_MEMORY_H_

#if IS_ENABLED(CONFIG_MTK_MMDVFS) && IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
void *mmdvfs_get_mmup_base(phys_addr_t *pa);
void *mmdvfs_get_vcp_base(phys_addr_t *pa);
bool mmdvfs_get_mmup_enable(void);
#else
static inline void *mmdvfs_get_mmup_base(phys_addr_t *pa)
{
	if (pa)
		*pa = 0;
	return NULL;
}
static inline void *mmdvfs_get_vcp_base(phys_addr_t *pa)
{
	if (pa)
		*pa = 0;
	return NULL;
}
static inline bool mmdvfs_get_mmup_enable(void) { return false; }
#endif

#define MEM_MMUP_BASE		mmdvfs_get_mmup_base(NULL)
#define MEM_VCP_BASE		mmdvfs_get_vcp_base(NULL)
#define MEM_IPI_SYNC_FUNC(vcp)	((vcp ? MEM_VCP_BASE : MEM_MMUP_BASE) + 0x0)
#define MEM_IPI_SYNC_DATA(vcp)	((vcp ? MEM_VCP_BASE : MEM_MMUP_BASE) + 0x4)

#endif

