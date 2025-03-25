/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef __GPUFREQ_MT6993_H__
#define __GPUFREQ_MT6993_H__

/**************************************************
 * GPUFREQ Config
 **************************************************/
#define GPUFREQ_KDEBUG_VERSION              (0x20250311)

/**************************************************
 * Clock Setting
 **************************************************/
#define POSDIV_2_MAX_FREQ                   (1900000)         /* KHz */
#define POSDIV_2_MIN_FREQ                   (750000)          /* KHz */
#define POSDIV_4_MAX_FREQ                   (950000)          /* KHz */
#define POSDIV_4_MIN_FREQ                   (375000)          /* KHz */
#define POSDIV_8_MAX_FREQ                   (475000)          /* KHz */
#define POSDIV_8_MIN_FREQ                   (187500)          /* KHz */
#define POSDIV_16_MAX_FREQ                  (237500)          /* KHz */
#define POSDIV_16_MIN_FREQ                  (125000)          /* KHz */
#define POSDIV_SHIFT                        (24)              /* bit */
#define DDS_SHIFT                           (14)              /* bit */
#define MFGPLL_FIN                          (26)              /* MHz */
#define MFG_TOP_SEL_BIT                     (BIT(0))
#define MFG_SC0_SEL_BIT                     (BIT(1))
#define FREQ_ROUNDUP_TO_10(freq)            ((freq % 10) ? (freq - (freq % 10) + 10) : freq)

/**************************************************
 * MTCMOS Setting
 **************************************************/
#define MFG_0_22_37_PWR_STATUS \
	(((DRV_Reg32(MFG_RPC_PWR_CON_STATUS) & GENMASK(22, 0)) & ~(BIT(8) | BIT(21))) | \
	(((DRV_Reg32(MFG_RPC_MFG37_PWR_CON) & BIT(30)) >> 30) << 23))

/**************************************************
 * Shader Core Setting
 **************************************************/
#define MFG3_SHADER_STACK0                  (T0C0 | T0C1)   /* MFG9,  MFG13 */
#define MFG4_SHADER_STACK1                  (T1C0 | T1C1)   /* MFG10, MFG14 */
#define MFG5_SHADER_STACK2                  (T2C0 | T2C1)   /* MFG11, MFG15 */
#define MFG22_SHADER_STACK3                 (T3C0 | T3C1)   /* MFG12, MFG16 */
#define MFG6_SHADER_STACK4                  (T4C0 | T4C1)   /* MFG17, MFG19 */
#define MFG7_SHADER_STACK5                  (T5C0 | T5C1)   /* MFG18, MFG20 */
#define GPU_SHADER_PRESENT_1 \
	(T0C0)
#define GPU_SHADER_PRESENT_2 \
	(MFG3_SHADER_STACK0)
#define GPU_SHADER_PRESENT_3 \
	(MFG3_SHADER_STACK0 | T1C0)
#define GPU_SHADER_PRESENT_4 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1)
#define GPU_SHADER_PRESENT_5 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | T2C0)
#define GPU_SHADER_PRESENT_6 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG5_SHADER_STACK2)
#define GPU_SHADER_PRESENT_7 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG5_SHADER_STACK2 | \
	 T3C0)
#define GPU_SHADER_PRESENT_8 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG5_SHADER_STACK2 | \
	 MFG22_SHADER_STACK3)
#define GPU_SHADER_PRESENT_9 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG5_SHADER_STACK2 | \
	 MFG22_SHADER_STACK3 | T4C0)
#define GPU_SHADER_PRESENT_10 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG5_SHADER_STACK2 | \
	 MFG22_SHADER_STACK3 | MFG6_SHADER_STACK4)
#define GPU_SHADER_PRESENT_11 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG5_SHADER_STACK2 | \
	 MFG22_SHADER_STACK3 | MFG6_SHADER_STACK4 | T5C0)
#define GPU_SHADER_PRESENT_12 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG5_SHADER_STACK2 | \
	 MFG22_SHADER_STACK3 | MFG6_SHADER_STACK4 | MFG7_SHADER_STACK5)
#define SHADER_CORE_NUM                 (12)
struct gpufreq_core_mask_info g_core_mask_table[SHADER_CORE_NUM] = {
	{12, GPU_SHADER_PRESENT_12},
	{11, GPU_SHADER_PRESENT_11},
	{10, GPU_SHADER_PRESENT_10},
	{9, GPU_SHADER_PRESENT_9},
	{8, GPU_SHADER_PRESENT_8},
	{7, GPU_SHADER_PRESENT_7},
	{6, GPU_SHADER_PRESENT_6},
	{5, GPU_SHADER_PRESENT_5},
	{4, GPU_SHADER_PRESENT_4},
	{3, GPU_SHADER_PRESENT_3},
	{2, GPU_SHADER_PRESENT_2},
	{1, GPU_SHADER_PRESENT_1},
};

/**************************************************
 * Dynamic Power Setting
 **************************************************/
#define GPU_DYN_REF_POWER                   (1102)          /* mW  */
#define GPU_DYN_REF_POWER_FREQ              (1360000)       /* KHz */
#define GPU_DYN_REF_POWER_VOLT              (90000)         /* mV x 100 */
#define STACK_DYN_REF_POWER                 (28652)         /* mW  */
#define STACK_DYN_REF_POWER_FREQ            (1800000)       /* KHz */
#define STACK_DYN_REF_POWER_VOLT            (100000)        /* mV x 100 */

/**************************************************
 * Bus Tracker Setting
 **************************************************/
#define BUS_TRACKER_OP(_info, _count, _type, _timestamp, _log, _id, _addr) \
	{ \
		_info.type = _type; \
		_info.timestamp = _timestamp; \
		_info.log = _log; \
		_info.id = _id; \
		_info.addr = _addr; \
		_count++; \
	}

/**************************************************
 * FREQ/VOLT Tracker Setting
 **************************************************/
#define FTRACKER_FREQ_CONVERT(freq)         ((freq) / 1000 * 8 / 26)
#define FTRACKER_FREQ_REVERT(freq)          ((freq) * 1000 / 8 * 26)
#define VTRACKER_VOLT_CONVERT(volt)         ((volt) / 100)
#define VTRACKER_VOLT_REVERT(volt)          ((volt) * 100)
#define FTRACKER_FGPU                       \
	(FTRACKER_FREQ_REVERT(DRV_Reg32(MFG_TOP_TOP_FREQ_TRACKER_CON_3) & GENMASK(10, 0)))
#define FTRACKER_FSTACK                     \
	(FTRACKER_FREQ_REVERT(DRV_Reg32(MFG_TOP_STACK_FREQ_TRACKER_CON_3) & GENMASK(10, 0)))
#define FTRACKER_TGPU                       \
	(DRV_Reg32(MFG_TOP_TOP_FREQ_TRACKER_CON_2))
#define FTRACKER_TSTACK                     \
	(DRV_Reg32(MFG_TOP_STACK_FREQ_TRACKER_CON_2))
#define VTRACKER_VGPU                       \
	(VTRACKER_VOLT_REVERT(DRV_Reg32(MFG_TOP_VOLT_TRACKER_CON_7) & GENMASK(10, 0)))
#define VTRACKER_VSTACK                     \
	(VTRACKER_VOLT_REVERT(DRV_Reg32(MFG_TOP_VOLT_TRACKER_CON_3) & GENMASK(10, 0)))
#define VTRACKER_TGPU                       \
	(DRV_Reg32(MFG_TOP_VOLT_TRACKER_CON_6))
#define VTRACKER_TSTACK                     \
	(DRV_Reg32(MFG_TOP_VOLT_TRACKER_CON_2))

/**************************************************
 * Enumeration
 **************************************************/

#endif /* __GPUFREQ_MT6993_H__ */
