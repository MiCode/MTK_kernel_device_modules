/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */
#ifndef __TLDAPC_INDEX_H__
#define __TLDAPC_INDEX_H__

// #include "tlApisec.h"

#define PLATFORM_TAG			"<6><8><5><5>"
#define DEVAPC_HAL_SUPPORT

/* Register definition for CMDQ usage */
#define DEVAPC_MMINFRA_AO_SYS0_BASE	0x30050000
#define DEVAPC_MMINFRA_AO_SYS1_BASE	0x30051000

/* Slave Type */
#define SLAVE_TYPE_PREFIX_INFRA_SYS1	(0)
#define SLAVE_TYPE_PREFIX_INFRA_SYS2	(255)

/* MM slaves' config index */

/* SYS0 */
#define DAPC_INDEX_VENC_APB_S_2		(48) /* 0x17020000 */
#define DAPC_INDEX_IMG1_APB_S_1		(171) /* 0x15021000 */
#define DAPC_INDEX_IMG1_APB_S_2		(172) /* 0x15022000 */
#define DAPC_INDEX_IMG1_APB_S_3		(173) /* 0x15023000 */
#define DAPC_INDEX_IMG1_APB_S_4		(174) /* 0x15024000 */
#define DAPC_INDEX_IMG1_APB_S_5		(175) /* 0x15025000 */
#define DAPC_INDEX_IMG1_APB_S_6		(176) /* 0x15026000 */
#define DAPC_INDEX_IMG1_APB_S_7		(177) /* 0x15027000 */
#define DAPC_INDEX_IMG1_APB_S_8		(178) /* 0x15028000 */
#define DAPC_INDEX_IMG1_APB_S_11	(181) /* 0x1502B000 */
#define DAPC_INDEX_IMG1_APB_S_12	(182) /* 0x1502C000 */

/* SYS1 */
#define DAPC_INDEX_MDP_APB_S_3		(267) /* 0x1F003000 */
#define DAPC_INDEX_MDP_APB_S_15		(279) /* 0x1F00F000 */
#define DAPC_INDEX_MDP_APB_S_21		(285) /* 0x1F015000 */

#endif // __TLDAPC_INDEX_H__
