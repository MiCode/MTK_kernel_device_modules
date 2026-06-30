/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GED_MT6991_H__
#define __GED_MT6991_H__

#include <linux/types.h>

#define LOG_SIZE sizeof(int)


/******************************************************************
 * SYSRAM for EB_DVFS_V2
 * Start address: 0x13D400~0x13E000 (borrow 3KB from MPU)
 * Size: 3KB
 * Debug RB: 0x13D400~0x13D800 (1KB), Each ringbuffer size: 10
 * TS RB: 0x13D800~0x13DC00 (1KB), Each ringbuffer size: 7
 * Mbrain: 0x13DC00~0x13E000 (1KB)
 ******************************************************************/
#define FDVFS_LEGACY_DATA_SIZE (0x800)
#define FDVFS_V2_RB_SIZE (0x400)  // 1 KB
#define FDVFS_TS_DATA_SIZE (0x400)  // 1 => 3.5 KB(DX5)
#define FDVFS_TS_REAL_DATA_START (FDVFS_LEGACY_DATA_SIZE + FDVFS_V2_RB_SIZE)
#define FDVFS_MBRAIN_REAL_DATA_START (FDVFS_TS_REAL_DATA_START + FDVFS_TS_DATA_SIZE)
#define FDVFS_NORMAL_REAL_DATA_START 443

#define SRAM_TS_RB_NUM 36 // 36 => 128(DX5)
#define MBRAIN_MAX_OPP_NUM 64 // 64 => 128(DX5)


/******************************************************************
 * SYSRAM for EB_DVFS_V2 (jayer 10kb)
 * Start address: 0x124800 + 0x800 (legacy 2KB)
 * Size: 8KB
 * Debug RB: 1KB / 0x400, Each ringbuffer size: 10
 * TS RB: 3.5KB / 0xE00, Each ringbuffer size: 7
 * Mbrain: 2KB / 0x800, 128 opp
 * Normal data: 1.5KB / 0x600
 * Order: legacy data => Debug RB => TS RB => Mbrain => Normal data
 *****************************************************************
#define FDVFS_LEGACY_DATA_SIZE (0x800)
#define FDVFS_V2_RB_SIZE (0x400)  // 1 KB
#define FDVFS_TS_DATA_SIZE (0xE00)  // 3.5 KB
#define FDVFS_MBRAIN_DATA_SIZE (0x800)  // 2 KB, DX5
#define FDVFS_TS_REAL_DATA_START (FDVFS_LEGACY_DATA_SIZE + FDVFS_V2_RB_SIZE)
#define FDVFS_MBRAIN_REAL_DATA_START (AP_FDVFS_TS_DATA_START + FDVFS_TS_DATA_SIZE)
#define FDVFS_NORMAL_REAL_DATA_START (FDVFS_MBRAIN_REAL_DATA_START + FDVFS_MBRAIN_DATA_SIZE)

#define SRAM_TS_RB_NUM 128
#define MBRAIN_MAX_OPP_NUM 128
*/

/******************************************************************
 * API for EB_DVFS_V2
 *****************************************************************/
unsigned int ged_platform_get_sysram(int virtual_offset);
unsigned int ged_platform_get_ts_rb_num(void);
unsigned int ged_platform_get_mbrain_max_num(void);

#endif // __GED_MT6991_H__
