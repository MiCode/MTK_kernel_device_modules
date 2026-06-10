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
#define DEVAPC_MMINFRA_AO_SYS0_BASE	0x1E820000
#define DEVAPC_MMINFRA_AO_SYS1_BASE	0x1E821000
#define DEVAPC_MMINFRA_AO_SYS2_BASE	0x1E822000

/* Slave Type */
#define SLAVE_TYPE_PREFIX_INFRA_SYS1	(0)
#define SLAVE_TYPE_PREFIX_INFRA_SYS2	(255)

/* MM slaves' config index */

/* SYS1 */
#define DAPC_INDEX_IMG1_APB_S_7		(283) /* 0x1500B000 */
#define DAPC_INDEX_IMG1_APB_S_49	(325) /* 0x15118000 */
#define DAPC_INDEX_IMG1_APB_S_50	(326) /* 0x15119000 */
#define DAPC_INDEX_IMG1_APB_S_51	(327) /* 0x1511A000 */
#define DAPC_INDEX_IMG1_APB_S_75	(351) /* 0x15218000 */
#define DAPC_INDEX_IMG1_APB_S_76	(352) /* 0x15219000 */
#define DAPC_INDEX_IMG1_APB_S_90	(366) /* 0x15318000 */
#define DAPC_INDEX_IMG1_APB_S_100	(376) /* 0x15518000 */
#define DAPC_INDEX_IMG1_APB_S_101	(377) /* 0x15519000 */
#define DAPC_INDEX_IMG1_APB_S_102	(378) /* 0x1551A000 */
#define DAPC_INDEX_IMG1_APB_S_145	(421) /* 0x15718000 */

#endif // __TLDAPC_INDEX_H__
