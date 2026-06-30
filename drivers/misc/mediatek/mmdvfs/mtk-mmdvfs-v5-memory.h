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

#define MEM_ENC_VAL(idx, lvl, usec)	((idx & 0xF) << 28 | (lvl & 0xF) << 24 | (usec & 0xFFFFFF) << 0)

#define MEM_DEC_IDX(val)	((val >> 28) & 0xF)
#define MEM_DEC_LVL(val)	((val >> 24) & 0xF)
#define MEM_DEC_USEC(val)	((val >>  0) & 0xFFFFFF)

#define MEM_OBJ_CNT		(2)
#define MEM_REC_CNT		(8)
#define MEM_OPP_CNT		(8)

#define DRAM_MMUP_BASE		mmdvfs_get_mmup_base(NULL)
#define DRAM_VCP_BASE		mmdvfs_get_vcp_base(NULL)

#define DRAM_USR_NUM		(16)
#define DRAM_USR_IDX(x)		(DRAM_VCP_BASE + 0x4 * (0 + (x))) // DRAM_USR_NUM
#define DRAM_USR_SEC(x, y)	(DRAM_VCP_BASE + 0x4 * (DRAM_USR_NUM + MEM_OBJ_CNT * (MEM_REC_CNT * (x) + (y)) + 0))
#define DRAM_USR_VAL(x, y)	(DRAM_VCP_BASE + 0x4 * (DRAM_USR_NUM + MEM_OBJ_CNT * (MEM_REC_CNT * (x) + (y)) + 1))

#define DRAM_CMD_NUM		(24)
#define DRAM_CMD_VCP_IDX	(12)
#define DRAM_CMD_IDX(x)		(DRAM_VCP_BASE + 0x4 * (272 + (x))) // DRAM_CMD_NUM
#define DRAM_CMD_SEC(x, y)	(DRAM_VCP_BASE + 0x4 * (272+ DRAM_CMD_NUM + MEM_OBJ_CNT * (MEM_REC_CNT * (x)+(y))+0))
#define DRAM_CMD_VAL(x, y)	(DRAM_VCP_BASE + 0x4 * (272 + DRAM_CMD_NUM + MEM_OBJ_CNT * (MEM_REC_CNT * (x)+(y))+1))

/* mbrain : u64(2) * DRAM_USR_NUM * MEM_OPP_CNT = 256 */
#define DRAM_USR_TOTAL(x, y)	(DRAM_VCP_BASE + 0x4 * (680 + MEM_OBJ_CNT * (MEM_OPP_CNT * (x) + (y))))
// 936

#define DRAM_SRAM_OFFSET	(DRAM_VCP_BASE + 0xffc)

enum {
	DUMP_IRQ,
	DUMP_PWR,
	DUMP_CLK,
	DUMP_CEIL,
	DUMP_XPC,
	DUMP_CCPRO,
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

#define SRAM_BASE		mmdvfs_get_mmup_sram()

#define SRAM_IRQ_CNT		(5) // vcore dvs, vmm dvs, vdisp dvs, vmm dfs, vdisp dfs
#define SRAM_PWR_CNT		(3) // vcore, vmm, vdisp
#define SRAM_CLK_CNT		(5) // vcore, vmm, vdisp, cam, hop
#define SRAM_CEIL_CNT		(3) // vcore, vmm, vdisp
#define SRAM_XPC_CNT		(3) // mmpc, cpc, dpc
#define SRAM_PROF_CNT		(9) // PROFILE_NUM
#define SRAM_PWR_TOTAL_CNT	(4) // vcore, vmm, vdisp, cam
#define SRAM_CCPRO_CNT		(2) // vcore, vmm

#define SRAM_IRQ_IDX(x)		(SRAM_BASE + 4 * (0 + (x)))
#define SRAM_PWR_IDX(x)		(SRAM_BASE + 4 * (5 + (x)))
#define SRAM_CLK_IDX(x)		(SRAM_BASE + 4 * (8 + (x)))
#define SRAM_CEIL_IDX(x)	(SRAM_BASE + 4 * (13 + (x)))
#define SRAM_XPC_IDX(x)		(SRAM_BASE + 4 * (16 + (x)))

#define SRAM_IRQ_SEC(x, y)	(SRAM_BASE + 4 * (20 + MEM_OBJ_CNT * (MEM_REC_CNT * (x) + (y)) + 0))
#define SRAM_IRQ_VAL(x, y)	(SRAM_BASE + 4 * (20 + MEM_OBJ_CNT * (MEM_REC_CNT * (x) + (y)) + 1))

#define SRAM_PWR_SEC(x, y)	(SRAM_BASE + 4 * (100 + MEM_OBJ_CNT * (MEM_REC_CNT * (x) + (y)) + 0))
#define SRAM_PWR_VAL(x, y)	(SRAM_BASE + 4 * (100 + MEM_OBJ_CNT * (MEM_REC_CNT * (x) + (y)) + 1))

#define SRAM_CLK_SEC(x, y)	(SRAM_BASE + 4 * (150 + MEM_OBJ_CNT * (MEM_REC_CNT * (x) + (y)) + 0))
#define SRAM_CLK_VAL(x, y)	(SRAM_BASE + 4 * (150 + MEM_OBJ_CNT * (MEM_REC_CNT * (x) + (y)) + 1))

#define SRAM_CEIL_SEC(x, y)	(SRAM_BASE + 4 * (230 + MEM_OBJ_CNT * (MEM_REC_CNT * (x) + (y)) + 0))
#define SRAM_CEIL_VAL(x, y)	(SRAM_BASE + 4 * (230 + MEM_OBJ_CNT * (MEM_REC_CNT * (x) + (y)) + 1))

#define SRAM_XPC_SEC(x, y)	(SRAM_BASE + 4 * (280 + MEM_OBJ_CNT * (MEM_REC_CNT * (x) + (y)) + 0))
#define SRAM_XPC_VAL(x, y)	(SRAM_BASE + 4 * (280 + MEM_OBJ_CNT * (MEM_REC_CNT * (x) + (y)) + 1))

#define SRAM_PROF_SEC(x)	(SRAM_BASE + 4 * (330 + MEM_OBJ_CNT * (x) + 0))
#define SRAM_PROF_VAL(x)	(SRAM_BASE + 4 * (330 + MEM_OBJ_CNT * (x) + 1))

/* mbrain : u64(2) * SRAM_PWR_TOTAL_CNT * MEM_OPP_CNT = 64 */
#define SRAM_PWR_TOTAL(x, y)	(SRAM_BASE + 4 * (350 + MEM_OBJ_CNT * (MEM_OPP_CNT * (x) + (y))))
/* mbrain : u64(2) * SRAM_XPC_CNT * MEM_OPP_CNT = 48 */
#define SRAM_USR_TOTAL(x, y)	(SRAM_BASE + 4 * (414 + MEM_OBJ_CNT * (MEM_OPP_CNT * (x) + (y)))) //mmpc, cpc, dpc

#define SRAM_CCPRO_IDX(x)	(SRAM_BASE + 4 * (462 + (x)))
#define SRAM_CCPRO_SEC(x, y)	(SRAM_BASE + 4 * (464 + MEM_OBJ_CNT * (MEM_REC_CNT * (x) + (y)) + 0))
#define SRAM_CCPRO_VAL(x, y)	(SRAM_BASE + 4 * (464 + MEM_OBJ_CNT * (MEM_REC_CNT * (x) + (y)) + 1))
//496

#endif

