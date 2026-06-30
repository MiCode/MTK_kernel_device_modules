// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef _V0_HWCCF_H_
#define _V0_HWCCF_H_
#include <linux/types.h> /* Kernel only */

#define V0_CCF_CG_HISTORY_NUM              (16)    /* Number of CG Transaction record */
#define V0_CCF_XPU_VOTE_HISTORY_NUM        (28)    /* Number of XPU Vote CG history record */
#define V0_CCF_XPU_VOTE_IRQ_HISTORY_NUM    (14)    /* Number of XPU Vote IRQ history record */
#define V0_CCF_TIMELINE_HISTORY_NUM        (40)    /* Number of Timeline history record */

/* XPU local Voter */
//#define CCF_OFS(ofs)    (HWCCF_BASE + ofs)
#define V0_CCF_XPU(n) ((n < 5) ?  (0x00000 + (n * 0x10000)) : \
							   (0x40000 + ((n - 4) * 0x1000)))
#define V0_CCF_XPU_CG_SET(n, x)       CCF_OFS(V0_CCF_XPU(n)         + (x) * 0x8)
#define V0_CCF_XPU_CG_CLR(n, x)       CCF_OFS(V0_CCF_XPU(n) + 0x004 + (x) * 0x8)

#define V0_CCF_XPU_MTCMOS0_SET(n)     CCF_OFS(V0_CCF_XPU(n) + 0x218)
#define V0_CCF_XPU_MTCMOS0_CLR(n)     CCF_OFS(V0_CCF_XPU(n) + 0x21C)
#define V0_CCF_XPU_MTCMOS1_SET(n)     CCF_OFS(V0_CCF_XPU(n) + 0x220)
#define V0_CCF_XPU_MTCMOS1_CLR(n)     CCF_OFS(V0_CCF_XPU(n) + 0x224)
#define V0_CCF_XPU_B1_SET(n)          CCF_OFS(V0_CCF_XPU(n) + 0x230)
#define V0_CCF_XPU_B1_CLR(n)          CCF_OFS(V0_CCF_XPU(n) + 0x234)
#define V0_CCF_XPU_B2_SET(n)          CCF_OFS(V0_CCF_XPU(n) + 0x238)
#define V0_CCF_XPU_B2_CLR(n)          CCF_OFS(V0_CCF_XPU(n) + 0x23C)

#define V0_CCF_XPU_PLL_SET(n)         CCF_OFS(V0_CCF_XPU(n) + 0x210)
#define V0_CCF_XPU_PLL_CLR(n)         CCF_OFS(V0_CCF_XPU(n) + 0x214)

#define V0_CCF_MTCMOS0_SET_OFS      (0x218)
#define V0_CCF_MTCMOS0_CLR_OFS      (0x21C)
#define V0_CCF_MTCMOS1_SET_OFS      (0x220)
#define V0_CCF_MTCMOS1_CLR_OFS      (0x224)
#define V0_CCF_B1_SET_OFS           (0x230)
#define V0_CCF_B1_CLR_OFS           (0x234)
#define V0_CCF_B2_SET_OFS           (0x238)
#define V0_CCF_B2_CLR_OFS           (0x23C)
#define V0_CCF_PLL_SET_OFS          (0x210)
#define V0_CCF_PLL_CLR_OFS          (0x214)

/* Utilities for HWCCF Voter */
#define V0_IS_SET_FROM_VOTER_ADDR(ofs)  ((((ofs) & 0x2ff) % 0x8) == 0)

// CG VIP Status
#define V0_CCF_VIP_CG_DONE(x)    CCF_OFS(0x12600 + (x) * 0x4)
// MTCMOS VIP Status
#define V0_CCF_MTCMOS_EN_0	CCF_OFS(0x1410)
#define V0_CCF_MTCMOS_DONE_0	CCF_OFS(0x141C)
#define V0_CCF_MTCMOS_EN_1	CCF_OFS(0x1420)
#define V0_CCF_MTCMOS_DONE_1	CCF_OFS(0x142C)
// Backup VIP Status
#define V0_CCF_BACKUP1_EN	CCF_OFS(0x1430)
#define V0_CCF_BACKUP1_DONE	CCF_OFS(0x143C)
#define V0_CCF_BACKUP2_EN	CCF_OFS(0x1440)
#define V0_CCF_BACKUP2_DONE	CCF_OFS(0x144C)
// PLL VIP Status
#define V0_CCF_PLL_EN		CCF_OFS(0x1400)
#define V0_CCF_PLL_DONE		CCF_OFS(0x140C)


/* levarage TFA addressmap */
//#define AP_HWCCF_BASE 0x1C040000
//#define MM_HWCCF_BASE 0x31B00000

/* Wrapper AP voter */

/* CG set/clr id x to ofs */
#define V0_CG_SET_OFS(x)       V0_CCF_XPU_CG_SET(HWV_XPU_0, x)
#define V0_CG_CLR_OFS(x)       V0_CCF_XPU_CG_CLR(HWV_XPU_0, x)
#define V0_CG_DONE_OFS(x)      V0_CCF_VIP_CG_DONE(x)

/* MTCMOS id to ofs */
#define V0_MTCMOS0_SET_OFS     V0_CCF_XPU_MTCMOS0_SET(HWV_XPU_0)
#define V0_MTCMOS0_CLR_OFS     V0_CCF_XPU_MTCMOS0_CLR(HWV_XPU_0)
#define V0_MTCMOS0_DONE_OFS    V0_CCF_MTCMOS_DONE_0

#define V0_MTCMOS1_SET_OFS     V0_CCF_XPU_MTCMOS1_SET(HWV_XPU_0)
#define V0_MTCMOS1_CLR_OFS     V0_CCF_XPU_MTCMOS1_CLR(HWV_XPU_0)
#define V0_MTCMOS1_DONE_OFS    V0_CCF_MTCMOS_DONE_1

/* BACKUP id to ofs */
#define V0_XPU_B1_SET          V0_CCF_XPU_B1_SET(HWV_XPU_0)
#define V0_XPU_B1_CLR          V0_CCF_XPU_B1_CLR(HWV_XPU_0)
#define V0_XPU_B1_DONE         V0_CCF_BACKUP1_DONE

#define V0_XPU_B2_SET          V0_CCF_XPU_B2_SET(HWV_XPU_0)
#define V0_XPU_B2_CLR          V0_CCF_XPU_B2_CLR(HWV_XPU_0)
#define V0_XPU_B2_DONE         V0_CCF_BACKUP2_DONE

/* PLL Voter */
#define V0_XPU_PLL_SET          V0_CCF_XPU_PLL_SET(HWV_XPU_0)
#define V0_XPU_PLL_CLR          V0_CCF_XPU_PLL_CLR(HWV_XPU_0)
#define V0_XPU_PLL_DONE         V0_CCF_XPU_PLL_DONE



/* dx4 for check */
#define V0_XPU_MTCMOS0_SET_STA		CCF_OFS(0x146C)
#define V0_XPU_MTCMOS0_CLR_STA		CCF_OFS(0x1470)
#define V0_XPU_MTCMOS1_SET_STA		CCF_OFS(0x1474)
#define V0_XPU_MTCMOS1_CLR_STA		CCF_OFS(0x1478)
#define V0_PLL_SET_STA			CCF_OFS(0x1464)
#define V0_PLL_CLR_STA			CCF_OFS(0x1468)
#define V0_CCF_BACKUP1_SET_STA		CCF_OFS(0x1484)
#define V0_CCF_BACKUP1_CLR_STA		CCF_OFS(0x1488)
#define V0_CCF_BACKUP2_SET_STA		CCF_OFS(0x148C)
#define V0_CCF_BACKUP2_CLR_STA		CCF_OFS(0x1490)

#endif /* _V0_HWCCF_H_ */
