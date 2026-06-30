/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */
#ifndef __DAPC_PLATFORM_H__
#define __DAPC_PLATFORM_H__

#include "tlDapcIndex.h"

/* PORTING:
 *
 * Assign devapc base address and apc index registers.
 * If engine need to protect does not exist,
 *	add new index in enum CMDQ_DAPC and
 *	assign new offset in dapc_engine_base array if necessary.
 */
#define DAPC_BASE		DEVAPC_MMINFRA_AO_SYS0_BASE
#define DAPC_BASE2		DEVAPC_MMINFRA_AO_SYS1_BASE

#define DAPC_SYS_CNT		2

#define CMDQ_DAPC_SYS1_CNT	(256)
#define CMDQ_DAPC_SYS2_CNT	(37)
/* this count contains DAPC SYS1 + SYS2 */
#define CMDQ_MAX_DAPC_COUNT	(CMDQ_DAPC_SYS1_CNT + CMDQ_DAPC_SYS2_CNT)

#endif
