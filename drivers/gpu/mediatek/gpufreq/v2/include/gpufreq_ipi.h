/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPUFREQ_IPI_H__
#define __GPUFREQ_IPI_H__

/**************************************************
 * IMPORTANT:
 * This file must be aligned with GPUEB gpufreq_ipi.h
 **************************************************/

/**************************************************
 * Definition
 **************************************************/
#define GPUFREQ_IPI_DATA_LEN            (sizeof(struct gpufreq_ipi_data) / sizeof(unsigned int))

/**************************************************
 * IPI Command ID
 **************************************************/
static char *gpufreq_ipi_cmd_name[] = {
	/* Common */
	"CMD_INIT_SHARED_MEM",           // 0
	"CMD_GET_FREQ_BY_IDX",           // 1
	"CMD_GET_POWER_BY_IDX",          // 2
	"CMD_GET_OPPIDX_BY_FREQ",        // 3
	"CMD_GET_LEAKAGE_POWER",         // 4
	"CMD_SET_LIMIT",                 // 5
	"CMD_POWER_CONTROL",             // 6
	"CMD_ACTIVE_SLEEP_CONTROL",      // 7
	"CMD_COMMIT",                    // 8
	"CMD_DUAL_COMMIT",               // 9
	"CMD_PDCA_CONFIG",               // 10
	/* Debug */
	"CMD_UPDATE_DEBUG_OPP_INFO",     // 11
	"CMD_SWITCH_LIMIT",              // 12
	"CMD_FIX_TARGET_OPPIDX",         // 13
	"CMD_FIX_DUAL_TARGET_OPPIDX",    // 14
	"CMD_FIX_CUSTOM_FREQ_VOLT",      // 15
	"CMD_FIX_DUAL_CUSTOM_FREQ_VOLT", // 16
	"CMD_SET_MFGSYS_CONFIG",         // 17
	"CMD_MSSV_COMMIT",               // 18
	"CMD_NUM",                       // 19
};

enum gpufreq_ipi_cmd {
	/* Common */
	CMD_INIT_SHARED_MEM           = 0,
	CMD_GET_FREQ_BY_IDX           = 1,
	CMD_GET_POWER_BY_IDX          = 2,
	CMD_GET_OPPIDX_BY_FREQ        = 3,
	CMD_GET_LEAKAGE_POWER         = 4,
	CMD_SET_LIMIT                 = 5,
	CMD_POWER_CONTROL             = 6,
	CMD_ACTIVE_SLEEP_CONTROL      = 7,
	CMD_COMMIT                    = 8,
	CMD_DUAL_COMMIT               = 9,
	CMD_PDCA_CONFIG               = 10,
	/* Debug */
	CMD_UPDATE_DEBUG_OPP_INFO     = 11,
	CMD_SWITCH_LIMIT              = 12,
	CMD_FIX_TARGET_OPPIDX         = 13,
	CMD_FIX_DUAL_TARGET_OPPIDX    = 14,
	CMD_FIX_CUSTOM_FREQ_VOLT      = 15,
	CMD_FIX_DUAL_CUSTOM_FREQ_VOLT = 16,
	CMD_SET_MFGSYS_CONFIG         = 17,
	CMD_MSSV_COMMIT               = 18,
	CMD_NUM                       = 19,
};

/**************************************************
 * IPI Data Structure
 **************************************************/
struct gpufreq_ipi_data {
	enum gpufreq_ipi_cmd cmd_id;
	unsigned int target;
	union {
		int oppidx;
		int return_value;
		unsigned int freq;
		unsigned int volt;
		unsigned int power;
		unsigned int power_state;
		unsigned int mode;
		unsigned int value;
		struct {
			unsigned long long base;
			unsigned int size;
		} shared_mem;
		struct {
			unsigned int freq;
			unsigned int volt;
		} custom;
		struct {
			unsigned int limiter;
			int ceiling_info;
			int floor_info;
		} set_limit;
		struct {
			unsigned int target;
			unsigned int val;
		} mfg_cfg;
		struct {
			unsigned int target;
			unsigned int val;
		} mssv;
		struct {
			int gpu_oppidx;
			int stack_oppidx;
		} dual_commit;
		struct {
			unsigned int fgpu;
			unsigned int vgpu;
			unsigned int fstack;
			unsigned int vstack;
		} dual_custom;
	} u;
};

#endif /* __GPUFREQ_IPI_H__ */
