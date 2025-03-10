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

#define DRAM_MMUP_BASE		mmdvfs_get_mmup_base(NULL)
#define DRAM_VCP_BASE		mmdvfs_get_vcp_base(NULL)

#define DRAM_REC_CNT		(8)
#define DRAM_OBJ_CNT		(2) // sec, val

#define DRAM_USR_NUM_MAX	(16)
#define DRAM_USR_IDX(x)		(DRAM_VCP_BASE + 0x4 * (0 + (x))) // 16 users
#define DRAM_USR_SEC(x, y)	(DRAM_VCP_BASE + 0x4 * (16 + DRAM_OBJ_CNT * (DRAM_REC_CNT * x + y) + 0))
#define DRAM_USR_VAL(x, y)	(DRAM_VCP_BASE + 0x4 * (16 + DRAM_OBJ_CNT * (DRAM_REC_CNT * x + y) + 1))
// 272

#define DRAM_SRAM_OFFSET	(DRAM_VCP_BASE + 0xffc)

#define DRAM_DEC_USR_PWR(val)	((val >> 28) & 0xf)
#define DRAM_DEC_USR_LVL(val)	((val >> 24) & 0xf)
#define DRAM_DEC_USR_USEC(val)	((val >>  0) & 0xffffff)
#define DRAM_ENC_USR(pwr, lvl, usec)	(((pwr & 0xf) << 28) | ((lvl & 0xf) << 24) | (usec & 0xffffff) << 0)


#define SRAM_BASE		mmdvfs_get_mmup_sram()
#define SRAM_REC_CNT		(8)
#define SRAM_OBJ_CNT		(2) // sec, usec << 16 | idx << 8 | opp

#define SRAM_DEC_IDX(val)	((val >> 28) & 0xf)
#define SRAM_DEC_LVL(val)	((val >> 24) & 0xf)
#define SRAM_DEC_USEC(val)	((val >>  0) & 0xffffff)

#define SRAM_ENC_VAL(idx, lvl, usec)	((idx) << 28 | (lvl) << 24 | (usec) << 0)

enum {
	DUMP_IRQ,
	DUMP_PWR,
	DUMP_CLK,
	DUMP_CEIL,
	DUMP_XPC,
	DUMP_NUM
};

enum {
	DUMP_DFS_VCORE,
	DUMP_DFS_VMM,
	DUMP_DFS_VDISP,
	DUMP_DVS_VMM,
	DUMP_DVS_VDISP,
	DUMP_IRQ_NUM
};

enum {
	DUMP_VCORE,
	DUMP_VMM,
	DUMP_VDISP,
	DUMP_CAM,
	DUMP_HOP,
	DUMP_CLK_NUM
};

#define SRAM_IRQ_CNT		(5) // vcore dvs, vmm dvs, vdisp dvs, vmm dfs, vdisp dfs
#define SRAM_PWR_CNT		(3) // vcore, vmm, vdisp
#define SRAM_CLK_CNT		(5) // vcore, vmm, vdisp, cam, hop
#define SRAM_CEIL_CNT		(3) // vcore, vmm, vdisp
#define SRAM_XPC_CNT		(3) // mmpc, cpc, dpc

#define SRAM_IRQ_IDX(x)		(SRAM_BASE + 4 * (0 + x))
#define SRAM_PWR_IDX(x)		(SRAM_BASE + 4 * (5 + x))
#define SRAM_CLK_IDX(x)		(SRAM_BASE + 4 * (8 + x))
#define SRAM_CEIL_IDX(x)	(SRAM_BASE + 4 * (13 + x))
#define SRAM_XPC_IDX(x)		(SRAM_BASE + 4 * (16 + x))
// 19

#define SRAM_IRQ_SEC(x, y)	(SRAM_BASE + 4 * (20 + SRAM_OBJ_CNT * (SRAM_REC_CNT * x + y) + 0))
#define SRAM_IRQ_VAL(x, y)	(SRAM_BASE + 4 * (20 + SRAM_OBJ_CNT * (SRAM_REC_CNT * x + y) + 1))

#define SRAM_PWR_SEC(x, y)	(SRAM_BASE + 4 * (100 + SRAM_OBJ_CNT * (SRAM_REC_CNT * x + y) + 0))
#define SRAM_PWR_VAL(x, y)	(SRAM_BASE + 4 * (100 + SRAM_OBJ_CNT * (SRAM_REC_CNT * x + y) + 1))

#define SRAM_CLK_SEC(x, y)	(SRAM_BASE + 4 * (150 + SRAM_OBJ_CNT * (SRAM_REC_CNT * x + y) + 0))
#define SRAM_CLK_VAL(x, y)	(SRAM_BASE + 4 * (150 + SRAM_OBJ_CNT * (SRAM_REC_CNT * x + y) + 1))

#define SRAM_CEIL_SEC(x, y)	(SRAM_BASE + 4 * (230 + SRAM_OBJ_CNT * (SRAM_REC_CNT * x + y) + 0))
#define SRAM_CEIL_VAL(x, y)	(SRAM_BASE + 4 * (230 + SRAM_OBJ_CNT * (SRAM_REC_CNT * x + y) + 1))

#define SRAM_XPC_SEC(x, y)	(SRAM_BASE + 4 * (280 + SRAM_OBJ_CNT * (SRAM_REC_CNT * x + y) + 0))
#define SRAM_XPC_VAL(x, y)	(SRAM_BASE + 4 * (280 + SRAM_OBJ_CNT * (SRAM_REC_CNT * x + y) + 1))
// 330

#endif

