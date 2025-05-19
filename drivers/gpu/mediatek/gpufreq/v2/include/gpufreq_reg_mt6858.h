/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2025 MediaTek Inc.
 */

#ifndef __GPUFREQ_REG_MT6858_H__
#define __GPUFREQ_REG_MT6858_H__

#include <linux/io.h>
#include <linux/bits.h>

/**************************************************
 * GPUFREQ Register Operation
 **************************************************/

/**************************************************
 * GPUFREQ Register Definition
 **************************************************/
#define MALI_BASE                              (0x13000000)
#define MALI_GPU_ID                            (g_mali_base + 0x000)               /* 0x13000000 */

#define MFG_TOP_CFG_BASE                       (0x13FBF000)
#define MFG_TOP_CG_CON                         (g_mfg_top_base + 0x000)            /* 0x13FBF000 */
#define MFG_TOP_CKMUX_CON                      (g_mfg_top_base + 0x0E8)            /* 0x13FBF0E8 */
#define MFG_TOP_DEBUG_SEL                      (g_mfg_top_base + 0x170)            /* 0x13FBF170 */
#define MFG_TOP_DEBUG_TOP                      (g_mfg_top_base + 0x178)            /* 0x13FBF178 */
#define MFG_TOP_DEBUG_ASYNC                    (g_mfg_top_base + 0x17C)            /* 0x13FBF17C */
#define MFG_TOP_POWER_TRACKER_SETTING          (g_mfg_top_base + 0xFE0)            /* 0x13FBFFE0 */
#define MFG_TOP_POWER_TRACKER_PDC_STATUS0      (g_mfg_top_base + 0xFE4)            /* 0x13FBFFE4 */
#define MFG_TOP_POWER_TRACKER_PDC_STATUS1      (g_mfg_top_base + 0xFE8)            /* 0x13FBFFE8 */

#define MFG_RPC_BASE                           (0x13F90000)
#define MFG_RPC_MFG1_PWR_CON                   (g_mfg_rpc_base + 0x1070)           /* 0x13F91070 */
#define MFG_RPC_MFG2_PWR_CON                   (g_mfg_rpc_base + 0x10A0)           /* 0x13F910A0 */
#define MFG_RPC_MFG3_PWR_CON                   (g_mfg_rpc_base + 0x10A4)           /* 0x13F910A4 */
#define MFG_RPC_MFG5_PWR_CON                   (g_mfg_rpc_base + 0x10AC)           /* 0x13F910AC */
#define MFG_RPC_MFG9_PWR_CON                   (g_mfg_rpc_base + 0x10BC)           /* 0x13F910BC */
#define MFG_RPC_MFG10_PWR_CON                  (g_mfg_rpc_base + 0x10C0)           /* 0x13F910C0 */
#define MFG_RPC_PWR_CON_STATUS                 (g_mfg_rpc_base + 0x1200)           /* 0x13F91200 */

#define MFG_PLL4H_TOP_BASE                     (0x13FA0000)
#define MFG_PLL4H_PLL1_CON0                    (g_mfg_pll4h_top_base + 0x008)      /* 0x13FA0008 */
#define MFG_PLL4H_PLL1_CON1                    (g_mfg_pll4h_top_base + 0x00C)      /* 0x13FA000C */
#define MFG_PLL4H_PLL4_CON0                    (g_mfg_pll4h_top_base + 0x038)      /* 0x13FA0038 */
#define MFG_PLL4H_PLL4_CON1                    (g_mfg_pll4h_top_base + 0x03C)      /* 0x13FA003C */
#define MFG_PLL4H_FQMTR_CON0                   (g_mfg_pll4h_top_base + 0x200)      /* 0x13FA0200 */
#define MFG_PLL4H_FQMTR_CON1                   (g_mfg_pll4h_top_base + 0x204)      /* 0x13FA0204 */

#define NTH_EMICFG_BASE                        (0x1021C000)
#define MFG_EMI1_GALS_SLV_DBG                  (g_nth_emicfg_base + 0x82C)         /* 0x1021C82C */
#define MFG_EMI0_GALS_SLV_DBG                  (g_nth_emicfg_base + 0x830)         /* 0x1021C830 */

#define NEMI_MI32_SMI_SUB_BASE                 (0x1025E000)
#define NEMI_MI32_SMI_DEBUG_S0                 (g_nemi_mi32_smi_sub_base + 0x400)  /* 0x1025E400 */
#define NEMI_MI32_SMI_DEBUG_S1                 (g_nemi_mi32_smi_sub_base + 0x404)  /* 0x1025E404 */
#define NEMI_MI32_SMI_DEBUG_S2                 (g_nemi_mi32_smi_sub_base + 0x408)  /* 0x1025E408 */
#define NEMI_MI32_SMI_DEBUG_M0                 (g_nemi_mi32_smi_sub_base + 0x430)  /* 0x1025E430 */
#define NEMI_MI32_SMI_DEBUG_MISC               (g_nemi_mi32_smi_sub_base + 0x440)  /* 0x1025E440 */

#define NEMI_MI33_SMI_SUB_BASE                 (0x1025F000)
#define NEMI_MI33_SMI_DEBUG_S0                 (g_nemi_mi33_smi_sub_base + 0x400)  /* 0x1025F400 */
#define NEMI_MI33_SMI_DEBUG_S1                 (g_nemi_mi33_smi_sub_base + 0x404)  /* 0x1025F404 */
#define NEMI_MI33_SMI_DEBUG_M0                 (g_nemi_mi33_smi_sub_base + 0x430)  /* 0x1025F430 */
#define NEMI_MI33_SMI_DEBUG_MISC               (g_nemi_mi33_smi_sub_base + 0x440)  /* 0x1025F440 */

#define EMI_IFR_PDN_BCRM_BASE                  (0x10276000)
#define EMI_IFR_NEMI_M0_AXI_SLPPORT_IDLE       (g_emi_infra_pdn_bcrm_base + 0x1B0) /* 0x102761B0 */
#define EMI_IFR_NEMI_M1_AXI_SLPPORT_IDLE       (g_emi_infra_pdn_bcrm_base + 0x1C4) /* 0x102761C4 */
#define EMI_IFR_NEMI_M0_M31_BUSY_SIGNAL        (g_emi_infra_pdn_bcrm_base + 0x1E4) /* 0x102761E4 */

#define SPM_BASE                               (0x1C001000)
#define SPM_SPM2GPUPM_CON                      (g_sleep + 0x410)                   /* 0x1C001410 */
#define SPM_SRC_REQ                            (g_sleep + 0x818)                   /* 0x1C001818 */
#define SPM_MFG0_PWR_CON                       (g_sleep + 0xEB0)                   /* 0x1C001EB0 */
#define SPM_GPU_PWR_STATUS                     (g_sleep + 0xF50)                   /* 0x1C001F50 */
#define SPM_GPU_PWR_STATUS_2ND                 (g_sleep + 0xF54)                   /* 0x1C001F54 */

#endif /* __GPUFREQ_REG_MT6858_H__ */
