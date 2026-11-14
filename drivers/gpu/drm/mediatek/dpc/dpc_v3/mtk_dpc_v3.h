/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_DPC_V3_H__
#define __MTK_DPC_V3_H__

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

#define DT_TE_60 16000
#define DT_TE_120 8000
#define DT_TE_360 2650
#define DT_TE_SAFEZONE 350
#define DT_OFF0 38000
#define DT_OFF1 500
#define DT_PRE_DISP1_OFF 100
#define DT_POST_DISP1_OFF 100
#define DT_PRE_MMINFRA_OFF 100
#define DT_POST_MMINFRA_OFF 800 /* infra267 + mminfra300 + margin */

#define DT_MMINFRA_OFFSET (DT_TE_SAFEZONE + DT_POST_DISP1_OFF + DT_POST_MMINFRA_OFF)
#define DT_DISP1_OFFSET   (DT_TE_SAFEZONE + DT_POST_DISP1_OFF)
#define DT_OVL_OFFSET     (DT_TE_SAFEZONE)
#define DT_DISP1TE_OFFSET (DT_TE_SAFEZONE - 50)
#define DT_VCORE_OFFSET   (DT_TE_SAFEZONE + 3000)

#define DT_12 (DT_TE_360 - DT_MMINFRA_OFFSET)
#define DT_5  (DT_TE_360 - DT_DISP1_OFFSET)
#define DT_1  (DT_TE_360 - DT_OVL_OFFSET)
#define DT_6  (DT_TE_360 - DT_DISP1TE_OFFSET)
#define DT_3  (DT_OFF1)
#define DT_7  (DT_OFF1 + DT_PRE_DISP1_OFF)
#define DT_13 (DT_OFF1 + DT_PRE_MMINFRA_OFF)

#define DPC2_DT_PRESZ 600
#define DPC2_DT_POSTSZ 500
#define DPC2_DT_MTCMOS 100
#define DPC2_DT_INFRA 300
#define DPC2_DT_MMINFRA 700
#define DPC2_DT_VCORE 850
#define DPC2_DT_DSION 1000
#define DPC2_DT_DSIOFF 100
#define DPC2_DT_TE_120 8300

#endif
