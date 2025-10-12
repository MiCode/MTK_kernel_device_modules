/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __MTK_LARB_PORT_H__
#define __MTK_LARB_PORT_H__

#include <mtk-memory-port.h>

/*
 * 1. define your larb base addr in order
 * 2. update SM_LARB_NR and smiLarbBasePA array if necessary
 * 3. copy region and your larb port definition from mt6855-larb-port.h in kernel
 * 4. update M4U_PORT_UNKNOWN if necessary, ensure last port less than M4U_PORT_UNKNOWN
 */

#define LARB0_BASE_PA	0x0
#define LARB1_BASE_PA	0x0
#define LARB2_BASE_PA	0x1f002000
#define LARB3_BASE_PA	0x0
#define LARB4_BASE_PA	0x0
#define LARB5_BASE_PA	0x0
#define LARB6_BASE_PA	0x0
#define LARB7_BASE_PA	0x17010000
#define LARB8_BASE_PA	0x0
#define LARB9_BASE_PA	0x1502e000
#define LARB10_BASE_PA	0x0
#define LARB11_BASE_PA	0x0
#define LARB12_BASE_PA	0x0
#define LARB13_BASE_PA	0x0
#define LARB14_BASE_PA	0x0
#define LARB15_BASE_PA	0x0
#define LARB16_BASE_PA	0x0
#define LARB17_BASE_PA	0x0
#define LARB18_BASE_PA	0x0
#define LARB19_BASE_PA	0x0
#define LARB20_BASE_PA	0x1b00f000

#define SMI_LARB_NR	(21)

static uint64_t smiLarbBasePA[SMI_LARB_NR] = {LARB0_BASE_PA,
	LARB1_BASE_PA, LARB2_BASE_PA, LARB3_BASE_PA, LARB4_BASE_PA,
	LARB5_BASE_PA, LARB6_BASE_PA, LARB7_BASE_PA, LARB8_BASE_PA,
	LARB9_BASE_PA, LARB10_BASE_PA, LARB11_BASE_PA, LARB12_BASE_PA,
	LARB13_BASE_PA, LARB14_BASE_PA, LARB15_BASE_PA, LARB16_BASE_PA,
	LARB17_BASE_PA, LARB18_BASE_PA, LARB19_BASE_PA, LARB20_BASE_PA
};

/* table id must be the same as mtk_iommu.h */
#define MM_TAB					(0)

/* iova region definition */
#define CAM_MDP_VENC_DOM			(0)
#define AIE_DOM					(1)
#define NORMAL_DOM				(2)
#define LK_RESV_DOM				(3)
#define VDO_REGION1				(4)
#define VDO_REGION2				(5)
#define VDO_REGION3				(6)
#define VDO_REGION4				(7)
#define VDEC_DOM				(8)
#define CAM_DOM					(1)

/* Larb2 -- 5 */
#define M4U_L2_P0_MDP_RDMA0			MTK_M4U_PORT_ID(MM_TAB, CAM_DOM, 2, 0)
#define M4U_L2_P1_MDP_RDMA1_DUMMY		MTK_M4U_PORT_ID(MM_TAB, CAM_DOM, 2, 1)
#define M4U_L2_P2_MDP_WROT0_WROT		MTK_M4U_PORT_ID(MM_TAB, CAM_DOM, 2, 2)
#define M4U_L2_P3_MDP_WROT2_WDMA		MTK_M4U_PORT_ID(MM_TAB, CAM_DOM, 2, 3)
#define M4U_L2_P4_MDP_FAKE_ENG0			MTK_M4U_PORT_ID(MM_TAB, CAM_DOM, 2, 4)

#define M4U_LARB20_PORT0			MTK_M4U_DOM_ID(AIE_DOM, 20, 0)
#define M4U_LARB20_PORT1			MTK_M4U_DOM_ID(AIE_DOM, 20, 1)
#define M4U_LARB20_PORT2			MTK_M4U_DOM_ID(AIE_DOM, 20, 2)
#define M4U_LARB20_PORT3			MTK_M4U_DOM_ID(AIE_DOM, 20, 3)

#define M4U_PORT_UNKNOWN			(M4U_LARB20_PORT3 + 1)


/*larb 7*/
#define M4U_LARB7_PORT0				MTK_M4U_DOM_ID(1, 7, 0)
#define M4U_LARB7_PORT1				MTK_M4U_DOM_ID(1, 7, 1)
#define M4U_LARB7_PORT2				MTK_M4U_DOM_ID(1, 7, 2)
#define M4U_LARB7_PORT3				MTK_M4U_DOM_ID(1, 7, 3)
#define M4U_LARB7_PORT4				MTK_M4U_DOM_ID(1, 7, 4)
#define M4U_LARB7_PORT5				MTK_M4U_DOM_ID(1, 7, 5)
#define M4U_LARB7_PORT6				MTK_M4U_DOM_ID(1, 7, 6)
#define M4U_LARB7_PORT7				MTK_M4U_DOM_ID(1, 7, 7)
#define M4U_LARB7_PORT8				MTK_M4U_DOM_ID(1, 7, 8)
#define M4U_LARB7_PORT9				MTK_M4U_DOM_ID(0, 7, 9)
#define M4U_LARB7_PORT10			MTK_M4U_DOM_ID(0, 7, 10)
#define M4U_LARB7_PORT11			MTK_M4U_DOM_ID(0, 7, 11)
#define M4U_LARB7_PORT12			MTK_M4U_DOM_ID(0, 7, 12)
#endif // __MTK_LARB_PORT_H__
