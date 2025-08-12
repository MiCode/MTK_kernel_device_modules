/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_DPC_V1_H__
#define __MTK_DPC_V1_H__

#include "mtk_dpc.h"

/*
 * EOF                                                      TE
 *  | OFF0 |                                     | SAFEZONE | OFF1 |
 *  |      |         OVL OFF                     |          |      | OVL OFF |
 *  |      |<-100->| DISP1 OFF           |<-100->|          |      | <-100-> | DISP1 OFF
 *  |      |<-100->| MMINFRA OFF |<-800->|       |          |      | <-100-> | MMINFRA OFF
 *  |      |       |             |       |       |          |      |         |
 *  |      OFF     OFF           ON      ON      ON         |      OFF       OFF
 *         0       4,11          12      5       1                 3         7,13
 */

#define DT_MAX_TIMEOUT   40329
#define DT_MIN_VBLANK    2500
#define DT_MIN_FRAME     2000

#define DT_TE_30  33000
#define DT_TE_45  21900
#define DT_TE_60  16300
#define DT_TE_90  10800
#define DT_TE_120 8000
#define DT_TE_150 6300
#define DT_TE_180 5200
#define DT_TE_210 4400
#define DT_TE_240 3800
#define DT_TE_360 2400
#define DT_TE_SAFEZONE 650
#define DT_OFF0 240
#define DT_OFF1 500
#define DT_PRE_DISP1_OFF 100
#define DT_POST_DISP1_OFF 100
#define DT_PRE_MMINFRA_OFF 100
#define DT_POST_MMINFRA_OFF 700 /* infra267 + mminfra300 + margin */

#define DT_MMINFRA_OFFSET (DT_TE_SAFEZONE + DT_POST_DISP1_OFF + DT_POST_MMINFRA_OFF)
#define DT_DISP1_OFFSET   (DT_TE_SAFEZONE + DT_POST_DISP1_OFF)
#define DT_OVL_OFFSET     (DT_TE_SAFEZONE)
#define DT_DISP1TE_OFFSET (300)

#define DT_11 (DT_OFF0)
#define DT_12 (DT_TE_360 - DT_MMINFRA_OFFSET)
#define DT_5  (DT_TE_360 - DT_DISP1_OFFSET)
#define DT_1  (DT_TE_360 - DT_OVL_OFFSET)
#define DT_6  (DT_TE_360 - DT_DISP1TE_OFFSET)
#define DT_3  (DT_OFF1)
#define DT_4  (DT_OFF0)
#define DT_7  (DT_OFF1 + DT_PRE_DISP1_OFF)
#define DT_13 (DT_OFF1 + DT_PRE_MMINFRA_OFF)

#define DT_OFF0_WO_MTCMOS 30
#define DT_TE_SAFEZONE_WO_MTCMOS 210
#define DT_POST_DVFS_OFF_WO_MTCMOS 1620
#define DT_DISP1TE_OFFSET_WO_MTCMOS (100)

#define DT_11_WO_MTCMOS (DT_OFF0_WO_MTCMOS)
#define DT_12_WO_MTCMOS (DT_TE_360 - DT_POST_DVFS_OFF_WO_MTCMOS)
#define DT_5_WO_MTCMOS  (DT_TE_360 - DT_TE_SAFEZONE_WO_MTCMOS)
#define DT_1_WO_MTCMOS  (DT_TE_360 - DT_TE_SAFEZONE_WO_MTCMOS)
#define DT_6_WO_MTCMOS  (DT_TE_360 - DT_DISP1TE_OFFSET_WO_MTCMOS)
#define DT_4_WO_MTCMOS  (DT_OFF0_WO_MTCMOS)

#define MTK_DPC_OF_DISP_SUBSYS(id)  ((int)(id) >= DPC_SUBSYS_DISP && (int)(id) <= DPC_SUBSYS_OVL0)
#define MTK_DPC_OF_MML_SUBSYS(id)  ((int)(id) >= DPC_SUBSYS_MML && (int)(id) <= DPC_SUBSYS_MML0)
#define MTK_DPC_OF_INVALID_SUBSYS(id)  ((int)(id) >= DPC_SUBSYS_CNT || (int)(id) < DPC_SUBSYS_DISP)

#endif
