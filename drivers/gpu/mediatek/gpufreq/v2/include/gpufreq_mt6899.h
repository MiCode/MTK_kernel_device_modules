/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef __GPUFREQ_MT6899_H__
#define __GPUFREQ_MT6899_H__

/**************************************************
 * GPUFREQ Config
 **************************************************/

/**************************************************
 * Clock Setting
 **************************************************/
#define POSDIV_SHIFT                        (24)              /* bit */
#define DDS_SHIFT                           (14)              /* bit */
#define MFGPLL_FIN                          (26)              /* MHz */
#define MFG_REF_SEL_BIT                     (BIT(24))
#define MFG_TOP_SEL_BIT                     (BIT(0))
#define MFG_SC0_SEL_BIT                     (BIT(1))
#define FREQ_ROUNDUP_TO_10(freq)            ((freq % 10) ? (freq - (freq % 10) + 10) : freq)

/**************************************************
 * MTCMOS Setting
 **************************************************/
#define MFG_0_1_PWR_STATUS \
	(((DRV_Reg32(SPM_XPU_PWR_STATUS) & BIT(1)) >> 1) | \
	(DRV_Reg32(MFG_RPC_MFG_PWR_CON_STATUS) & BIT(1)))
#define MFG_0_31_PWR_STATUS \
	(((DRV_Reg32(SPM_XPU_PWR_STATUS) & BIT(1)) >> 1) | \
	DRV_Reg32(MFG_RPC_MFG_PWR_CON_STATUS))

/**************************************************
 * Shader Core Setting
 **************************************************/
#define MFG3_SHADER_STACK0                  (T0C0 | T0C1)     /* MFG9,  MFG11 */
#define MFG5_SHADER_STACK2                  (T2C0 | T2C1)     /* MFG10, MFG12 */
#define MFG6_SHADER_STACK4                  (T4C0 | T4C1)     /* MFG13, MFG15 */
#define MFG8_SHADER_STACK6                  (T6C0 | T6C1)     /* MFG14, MFG16 */
#define GPU_SHADER_PRESENT_1 \
	(T0C0)
#define GPU_SHADER_PRESENT_2 \
	(MFG3_SHADER_STACK0)
#define GPU_SHADER_PRESENT_3 \
	(MFG3_SHADER_STACK0 | T2C0)
#define GPU_SHADER_PRESENT_4 \
	(MFG3_SHADER_STACK0 | MFG5_SHADER_STACK2)
#define GPU_SHADER_PRESENT_5 \
	(MFG3_SHADER_STACK0 | MFG5_SHADER_STACK2 | T4C0)
#define GPU_SHADER_PRESENT_6 \
	(MFG3_SHADER_STACK0 | MFG5_SHADER_STACK2 | MFG6_SHADER_STACK4)
#define GPU_SHADER_PRESENT_7 \
	(MFG3_SHADER_STACK0 | MFG5_SHADER_STACK2 | MFG6_SHADER_STACK4 | \
	 T6C0)
#define GPU_SHADER_PRESENT_8 \
	(MFG3_SHADER_STACK0 | MFG5_SHADER_STACK2 | MFG6_SHADER_STACK4 | \
	 MFG8_SHADER_STACK6)
#define SHADER_CORE_NUM                     (8)
struct gpufreq_core_mask_info g_core_mask_table[SHADER_CORE_NUM] = {
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
#define GPU_DYN_REF_POWER                   (1613)          /* mW  */
#define GPU_DYN_REF_POWER_FREQ              (1612000)       /* KHz */
#define GPU_DYN_REF_POWER_VOLT              (100000)        /* mV x 100 */
#define STACK_DYN_REF_POWER                 (9860)          /* mW  */
#define STACK_DYN_REF_POWER_FREQ            (1612000)       /* KHz */
#define STACK_DYN_REF_POWER_VOLT            (100000)        /* mV x 100 */

#endif /* __GPUFREQ_MT6899_H__ */
