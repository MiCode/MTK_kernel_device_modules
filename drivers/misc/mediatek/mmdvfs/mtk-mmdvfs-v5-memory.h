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

bool mmdvfs_get_mmup_sram_enable(void);
void __iomem *mmdvfs_get_mmup_sram(void);
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

static inline bool mmdvfs_get_mmup_sram_enable(void) { return false; }
static inline void __iomem *mmdvfs_get_mmup_sram(void) { return; }
#endif

#define MEM_MMUP_BASE		mmdvfs_get_mmup_base(NULL)
#define MEM_VCP_BASE		mmdvfs_get_vcp_base(NULL)
#define MEM_BASE		(mmdvfs_get_mmup_enable() ? MEM_MMUP_BASE : MEM_VCP_BASE)

#define MEM_IPI_SYNC_FUNC(vcp)	((vcp ? MEM_VCP_BASE : MEM_MMUP_BASE) + 0x0)
#define MEM_IPI_SYNC_DATA(vcp)	((vcp ? MEM_VCP_BASE : MEM_MMUP_BASE) + 0x4)

#define MEM_SRAM_OFFSET		(MEM_BASE + 0xFFC)
#define SRAM_BASE		mmdvfs_get_mmup_sram()
#define SRAM_REC_CNT		(8)
#define SRAM_OBJ_CNT		(2) // sec, usec << 16 | idx << 8 | opp

#define SRAM_DEC_USEC(val)	((val >> 16) & 0xffff)
#define SRAM_DEC_IDX(val)	((val >>  8) & 0xff)
#define SRAM_DEC_LVL(val)	((val >>  0) & 0xff)

#define SRAM_IRQ_CNT		(5) // vcore dvs, vmm dvs, vdisp dvs, vmm dfs, vdisp dfs
#define SRAM_PWR_CNT		(4) // vcore, vmm, vdisp, ceil
#define SRAM_CLK_CNT		(5) // vcore, vmm, vdisp, cam, hop

#define SRAM_IRQ_IDX(x)		(SRAM_BASE + 4 * (0 + x))
#define SRAM_PWR_IDX(x)		(SRAM_BASE + 4 * (5 + x))
#define SRAM_CLK_IDX(x)		(SRAM_BASE + 4 * (9 + x))

#define SRAM_IRQ_SEC(x, y)	(SRAM_BASE + 4 * (20 + SRAM_OBJ_CNT * (SRAM_REC_CNT * x + y) + 0))
#define SRAM_IRQ_VAL(x, y)	(SRAM_BASE + 4 * (20 + SRAM_OBJ_CNT * (SRAM_REC_CNT * x + y) + 1))

#define SRAM_PWR_SEC(x, y)	(SRAM_BASE + 4 * (100 + SRAM_OBJ_CNT * (SRAM_REC_CNT * x + y) + 0))
#define SRAM_PWR_VAL(x, y)	(SRAM_BASE + 4 * (100 + SRAM_OBJ_CNT * (SRAM_REC_CNT * x + y) + 1))

#define SRAM_CLK_SEC(x, y)	(SRAM_BASE + 4 * (170 + SRAM_OBJ_CNT * (SRAM_REC_CNT * x + y) + 0))
#define SRAM_CLK_VAL(x, y)	(SRAM_BASE + 4 * (170 + SRAM_OBJ_CNT * (SRAM_REC_CNT * x + y) + 1))

// 250

#endif

