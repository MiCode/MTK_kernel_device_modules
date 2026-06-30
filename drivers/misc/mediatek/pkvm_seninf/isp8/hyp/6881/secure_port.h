/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __SECURE_PORT_H__
#define __SECURE_PORT_H__

typedef enum {
	CUSTOM_CFG_CSI_PORT_0 = 0x0,    // 4D1C
	CUSTOM_CFG_CSI_PORT_1,          // 4D1C
	CUSTOM_CFG_CSI_PORT_2,          // 4D1C
	CUSTOM_CFG_CSI_PORT_3,          // 4D1C
	CUSTOM_CFG_CSI_PORT_4,          // 4D1C
	CUSTOM_CFG_CSI_PORT_5,          // 4D1C
	CUSTOM_CFG_CSI_PORT_0A,         // 2D1C or 2Trio
	CUSTOM_CFG_CSI_PORT_0B,         // 2D1C or 2Trio
	CUSTOM_CFG_CSI_PORT_1A,         // 2D1C or 2Trio
	CUSTOM_CFG_CSI_PORT_1B,         // 2D1C or 2Trio
	CUSTOM_CFG_CSI_PORT_2A,         // 2D1C or 2Trio
	CUSTOM_CFG_CSI_PORT_2B,         // 2D1C or 2Trio
	CUSTOM_CFG_CSI_PORT_3A,         // 2D1C or 2Trio
	CUSTOM_CFG_CSI_PORT_3B,         // 2D1C or 2Trio
	CUSTOM_CFG_CSI_PORT_4A,         // 2D1C or 2Trio
	CUSTOM_CFG_CSI_PORT_4B,         // 2D1C or 2Trio
	CUSTOM_CFG_CSI_PORT_5A,         // 2D1C or 2Trio
	CUSTOM_CFG_CSI_PORT_5B,         // 2D1C or 2Trio
	CUSTOM_CFG_CSI_PORT_MAX_NUM,
	CUSTOM_CFG_CSI_PORT_NONE        // for non-MIPI sensor
} CUSTOM_CFG_CSI_PORT;

#endif
