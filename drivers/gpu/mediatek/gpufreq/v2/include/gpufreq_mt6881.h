/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2025 MediaTek Inc.
 */

#ifndef __GPUFREQ_MT6881_H__
#define __GPUFREQ_MT6881_H__

/**************************************************
 * GPUFREQ Config
 **************************************************/
#define GPUFREQ_KDEBUG_VERSION              (0x20250910)

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
#define MFG_PWR_STATUS \
	(((DRV_Reg32(SPM_MFG0_PWR_CON) & BIT(30)) >> 30) | \
	((DRV_Reg32(MFG_RPC_PWR_CON_STATUS) & GENMASK(10, 1)) & ~(BIT(4) | GENMASK(8, 6))))
/**************************************************
 * Shader Core Setting
 **************************************************/
#define MFG3_SHADER_STACK0                  (T0C0)    /* MFG9 */
#define MFG5_SHADER_STACK2                  (T2C0)    /* MFG10 */
#define GPU_SHADER_PRESENT_1 \
	(MFG3_SHADER_STACK0)
#define GPU_SHADER_PRESENT_2 \
	(MFG3_SHADER_STACK0 | MFG5_SHADER_STACK2)
#define SHADER_CORE_NUM                 (2)
struct gpufreq_core_mask_info g_core_mask_table[SHADER_CORE_NUM] = {
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
 * VCORE Level Setting
 **************************************************/
#define VCORE_LEVEL_MASK                    (0x000000FF)
#define VCORE_LEVEL_SHIFT                   (0)
#define DDR_LEVEL_MASK                      (0x0000FF00)
#define DDR_LEVEL_SHIFT                     (8)
#define EMI_LEVEL_MASK                      (0x00FF0000)
#define EMI_LEVEL_SHIFT                     (16)

/**************************************************
 * Enumeration
 **************************************************/

#endif /* __GPUFREQ_MT6881_H__ */
