/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef __GPUFREQ_REG_MT6899_H__
#define __GPUFREQ_REG_MT6899_H__

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
#define MALI_SHADER_READY_LO                   (g_mali_base + 0x140)               /* 0x13000140 */
#define MALI_TILER_READY_LO                    (g_mali_base + 0x150)               /* 0x13000150 */
#define MALI_L2_READY_LO                       (g_mali_base + 0x160)               /* 0x13000160 */
#define MALI_GPU_IRQ_STATUS                    (g_mali_base + 0x02C)               /* 0x1300002C */

#define MFG_HBVC_BASE                          (0x13F50000)
#define MFG_HBVC_FLL0_DBG_FRONTEND0            (g_mfg_hbvc_base + 0x400)           /* 0x13F50400 */
#define MFG_HBVC_FLL1_DBG_FRONTEND0            (g_mfg_hbvc_base + 0x404)           /* 0x13F50404 */
#define MFG_HBVC_GRP0_DBG_BACKEND0             (g_mfg_hbvc_base + 0x480)           /* 0x13F50480 */
#define MFG_HBVC_GRP1_DBG_BACKEND0             (g_mfg_hbvc_base + 0x484)           /* 0x13F50484 */

#define MFG_RPC_BASE                           (0x13F90000)
#define MFG_RPC_BRISKET_ST0_AO_CFG_0           (g_mfg_rpc_base + 0x0500)           /* 0x13F90500 */
#define MFG_RPC_BRISKET_ST2_AO_CFG_0           (g_mfg_rpc_base + 0x0508)           /* 0x13F90508 */
#define MFG_RPC_BRISKET_ST4_AO_CFG_0           (g_mfg_rpc_base + 0x0510)           /* 0x13F90510 */
#define MFG_RPC_BRISKET_ST6_AO_CFG_0           (g_mfg_rpc_base + 0x0518)           /* 0x13F90518 */
#define MFG_RPC_MFG_CK_FAST_REF_SEL            (g_mfg_rpc_base + 0x055C)           /* 0x13F9055C */
#define MFG_RPC_SLP_PROT_EN_STA                (g_mfg_rpc_base + 0x1048)           /* 0x13F91048 */
#define MFG_RPC_MFG1_PWR_CON                   (g_mfg_rpc_base + 0x1070)           /* 0x13F91070 */
#define MFG_RPC_MFG2_PWR_CON                   (g_mfg_rpc_base + 0x10A0)           /* 0x13F910A0 */
#define MFG_RPC_MFG3_PWR_CON                   (g_mfg_rpc_base + 0x10A4)           /* 0x13F910A4 */
#define MFG_RPC_MFG5_PWR_CON                   (g_mfg_rpc_base + 0x10AC)           /* 0x13F910AC */
#define MFG_RPC_MFG6_PWR_CON                   (g_mfg_rpc_base + 0x10B0)           /* 0x13F910B0 */
#define MFG_RPC_MFG8_PWR_CON                   (g_mfg_rpc_base + 0x10B8)           /* 0x13F910B8 */
#define MFG_RPC_MFG9_PWR_CON                   (g_mfg_rpc_base + 0x10BC)           /* 0x13F910BC */
#define MFG_RPC_MFG10_PWR_CON                  (g_mfg_rpc_base + 0x10C0)           /* 0x13F910C0 */
#define MFG_RPC_MFG11_PWR_CON                  (g_mfg_rpc_base + 0x10C4)           /* 0x13F910C4 */
#define MFG_RPC_MFG12_PWR_CON                  (g_mfg_rpc_base + 0x10C8)           /* 0x13F910C8 */
#define MFG_RPC_MFG13_PWR_CON                  (g_mfg_rpc_base + 0x10CC)           /* 0x13F910CC */
#define MFG_RPC_MFG14_PWR_CON                  (g_mfg_rpc_base + 0x10D0)           /* 0x13F910D0 */
#define MFG_RPC_MFG15_PWR_CON                  (g_mfg_rpc_base + 0x10D4)           /* 0x13F910D4 */
#define MFG_RPC_MFG16_PWR_CON                  (g_mfg_rpc_base + 0x10D8)           /* 0x13F910D8 */
#define MFG_RPC_BRISKET_TOP_AO_CFG_0           (g_mfg_rpc_base + 0x1100)           /* 0x13F91100 */
#define MFG_RPC_GTOP_DREQ_CFG                  (g_mfg_rpc_base + 0x1110)           /* 0x13F91110 */
#define MFG_RPC_MFG_PWR_CON_STATUS             (g_mfg_rpc_base + 0x1200)           /* 0x13F91200 */
#define MFG_RPC_MFG_PWR_CON_2ND_STATUS         (g_mfg_rpc_base + 0x1204)           /* 0x13F91204 */
#define MFG_RPC_MFG_PWR_CON_STATUS_1           (g_mfg_rpc_base + 0x1208)           /* 0x13F91208 */
#define MFG_RPC_MFG_PWR_CON_2ND_STATUS_1       (g_mfg_rpc_base + 0x120C)           /* 0x13F9120C */

#define MFG_PLL_BASE                           (0x13FA0000)
#define MFG_PLL_CON0                           (g_mfg_pll_base + 0x008)            /* 0x13FA0008 */
#define MFG_PLL_CON1                           (g_mfg_pll_base + 0x00C)            /* 0x13FA000C */
#define MFG_PLL_FQMTR_CON0                     (g_mfg_pll_base + 0x040)            /* 0x13FA0040 */
#define MFG_PLL_FQMTR_CON1                     (g_mfg_pll_base + 0x044)            /* 0x13FA0044 */

#define MFG_PLL_SC0_BASE                       (0x13FA0400)
#define MFG_PLL_SC0_CON0                       (g_mfg_pll_sc0_base + 0x008)        /* 0x13FA0408 */
#define MFG_PLL_SC0_CON1                       (g_mfg_pll_sc0_base + 0x00C)        /* 0x13FA040C */
#define MFG_PLL_SC0_FQMTR_CON0                 (g_mfg_pll_sc0_base + 0x040)        /* 0x13FA0440 */
#define MFG_PLL_SC0_FQMTR_CON1                 (g_mfg_pll_sc0_base + 0x044)        /* 0x13FA0444 */

#define MFG_TOP_CFG_BASE                       (0x13FBF000)
#define MFG_DEFAULT_DELSEL_00                  (g_mfg_top_base + 0xC80)            /* 0x13FBFC80 */
#define MFG_POWER_TRACKER_SETTING              (g_mfg_top_base + 0xFE0)            /* 0x13FBFFE0 */
#define MFG_POWER_TRACKER_PDC_STATUS0          (g_mfg_top_base + 0xFE4)            /* 0x13FBFFE4 */
#define MFG_POWER_TRACKER_PDC_STATUS1          (g_mfg_top_base + 0xFE8)            /* 0x13FBFFE8 */
#define MFG_POWER_TRACKER_PDC_STATUS2          (g_mfg_top_base + 0xFEC)            /* 0x13FBFFEC */
#define MFG_POWER_TRACKER_PDC_STATUS3          (g_mfg_top_base + 0xFF0)            /* 0x13FBFFF0 */
#define MFG_POWER_TRACKER_PDC_STATUS4          (g_mfg_top_base + 0xFF4)            /* 0x13FBFFF4 */
#define MFG_POWER_TRACKER_PDC_STATUS5          (g_mfg_top_base + 0xFF8)            /* 0x13FBFFF8 */

#define MCUSYS_PAR_WRAP_BASE                   (0x0C000000)
#define MCUSYS_PAR_WRAP_ACP_GALS_DBG           (g_mcusys_par_wrap_base + 0xB2C)    /* 0x0C000B2C */

#define TOPCKGEN_BASE                          (0x10000000)
#define TOPCK_CLK_CFG_5_STA                    (g_topckgen_base + 0x060)           /* 0x10000060 */

#define SPM_BASE                               (0x1C001000)
#define SPM_SPM2GPUPM_CON                      (g_sleep + 0x410)                   /* 0x1C001410 */
#define SPM_SRC_REQ                            (g_sleep + 0x818)                   /* 0x1C001818 */
#define SPM_MFG0_PWR_CON                       (g_sleep + 0xEC8)                   /* 0x1C001EC8 */
#define SPM_SOC_BUCK_ISO_CON                   (g_sleep + 0xF1C)                   /* 0x1C001F1C */
#define SPM_XPU_PWR_STATUS                     (g_sleep + 0xF38)                   /* 0x1C001F38 */
#define SPM_XPU_PWR_STATUS_2ND                 (g_sleep + 0xF3C)                   /* 0x1C001F3C */

#define NTH_EMICFG_BASE                        (0x1021C000)
#define NTH_EMI_IFR_EMI_M7_GALS_SLV_DBG        (g_nth_emicfg_base + 0x82C)         /* 0x1021C82C */
#define NTH_EMI_IFR_EMI_M6_GALS_SLV_DBG        (g_nth_emicfg_base + 0x830)         /* 0x1021C830 */
#define NTH_EMI_APU_EMI1_GALS_SLV_DBG          (g_nth_emicfg_base + 0x840)         /* 0x1021C840 */
#define NTH_EMI_APU_EMI0_GALS_SLV_DBG          (g_nth_emicfg_base + 0x844)         /* 0x1021C844 */
#define NTH_EMI_MFG_EMI1_GALS_SLV_DBG          (g_nth_emicfg_base + 0x848)         /* 0x1021C848 */
#define NTH_EMI_MFG_EMI0_GALS_SLV_DBG          (g_nth_emicfg_base + 0x84C)         /* 0x1021C84C */

#define STH_EMICFG_BASE                        (0x1021E000)
#define STH_EMI_IFR_EMI_M7_GALS_SLV_DBG        (g_sth_emicfg_base + 0x82C)         /* 0x1021E82C */
#define STH_EMI_IFR_EMI_M6_GALS_SLV_DBG        (g_sth_emicfg_base + 0x830)         /* 0x1021E830 */
#define STH_EMI_APU_EMI1_GALS_SLV_DBG          (g_sth_emicfg_base + 0x840)         /* 0x1021E840 */
#define STH_EMI_APU_EMI0_GALS_SLV_DBG          (g_sth_emicfg_base + 0x844)         /* 0x1021E844 */
#define STH_EMI_MFG_EMI1_GALS_SLV_DBG          (g_sth_emicfg_base + 0x848)         /* 0x1021E848 */
#define STH_EMI_MFG_EMI0_GALS_SLV_DBG          (g_sth_emicfg_base + 0x84C)         /* 0x1021E84C */

#define NEMI_M6M7_MI32_MI33_SMI_BASE           (0x1025E000)
#define NEMI_M6M7_MI32_SMI_DEBUG_S0            (g_nemi_m6m7_mi32_mi33_smi + 0x400) /* 0x1025E400 */
#define NEMI_M6M7_MI32_SMI_DEBUG_S1            (g_nemi_m6m7_mi32_mi33_smi + 0x404) /* 0x1025E404 */
#define NEMI_M6M7_MI32_SMI_DEBUG_S2            (g_nemi_m6m7_mi32_mi33_smi + 0x408) /* 0x1025E408 */
#define NEMI_M6M7_MI32_SMI_DEBUG_M0            (g_nemi_m6m7_mi32_mi33_smi + 0x430) /* 0x1025E430 */
#define NEMI_M6M7_MI32_SMI_DEBUG_MISC          (g_nemi_m6m7_mi32_mi33_smi + 0x440) /* 0x1025E440 */
#define NEMI_M6M7_MI33_SMI_DEBUG_S0            (g_nemi_m6m7_mi32_mi33_smi + 0xC00) /* 0x1025EC00 */
#define NEMI_M6M7_MI33_SMI_DEBUG_S1            (g_nemi_m6m7_mi32_mi33_smi + 0xC04) /* 0x1025EC04 */
#define NEMI_M6M7_MI33_SMI_DEBUG_S2            (g_nemi_m6m7_mi32_mi33_smi + 0xC08) /* 0x1025EC08 */
#define NEMI_M6M7_MI33_SMI_DEBUG_M0            (g_nemi_m6m7_mi32_mi33_smi + 0xC30) /* 0x1025EC30 */
#define NEMI_M6M7_MI33_SMI_DEBUG_MISC          (g_nemi_m6m7_mi32_mi33_smi + 0xC40) /* 0x1025EC40 */

#define NTH_EMICFG_AO_MEM_BASE                 (0x10270000)
#define NTH_EMI_AO_SLEEP_PROT_MASK             (g_nth_emicfg_ao_mem_base + 0x000)  /* 0x10270000 */
#define NTH_EMI_AO_GLITCH_PROT_EN              (g_nth_emicfg_ao_mem_base + 0x080)  /* 0x10270080 */
#define NTH_EMI_AO_GLITCH_PROT_RDY             (g_nth_emicfg_ao_mem_base + 0x08C)  /* 0x1027008C */
#define NTH_EMI_AO_M6M7_IDLE_BIT_EN_1          (g_nth_emicfg_ao_mem_base + 0x228)  /* 0x10270228 */
#define NTH_EMI_AO_M6M7_IDLE_BIT_EN_0          (g_nth_emicfg_ao_mem_base + 0x22C)  /* 0x1027022C */

#define SEMI_M6M7_MI32_MI33_SMI_BASE           (0x10309000)
#define SEMI_M6M7_MI32_SMI_DEBUG_S0            (g_semi_m6m7_mi32_mi33_smi + 0x400) /* 0x10309400 */
#define SEMI_M6M7_MI32_SMI_DEBUG_S1            (g_semi_m6m7_mi32_mi33_smi + 0x404) /* 0x10309404 */
#define SEMI_M6M7_MI32_SMI_DEBUG_S2            (g_semi_m6m7_mi32_mi33_smi + 0x408) /* 0x10309408 */
#define SEMI_M6M7_MI32_SMI_DEBUG_M0            (g_semi_m6m7_mi32_mi33_smi + 0x430) /* 0x10309430 */
#define SEMI_M6M7_MI32_SMI_DEBUG_MISC          (g_semi_m6m7_mi32_mi33_smi + 0x440) /* 0x10309440 */
#define SEMI_M6M7_MI33_SMI_DEBUG_S0            (g_semi_m6m7_mi32_mi33_smi + 0xC00) /* 0x10309C00 */
#define SEMI_M6M7_MI33_SMI_DEBUG_S1            (g_semi_m6m7_mi32_mi33_smi + 0xC04) /* 0x10309C04 */
#define SEMI_M6M7_MI33_SMI_DEBUG_S2            (g_semi_m6m7_mi32_mi33_smi + 0xC08) /* 0x10309C08 */
#define SEMI_M6M7_MI33_SMI_DEBUG_M0            (g_semi_m6m7_mi32_mi33_smi + 0xC30) /* 0x10309C30 */
#define SEMI_M6M7_MI33_SMI_DEBUG_MISC          (g_semi_m6m7_mi32_mi33_smi + 0xC40) /* 0x10309C40 */

#define STH_EMICFG_AO_MEM_BASE                 (0x1030E000)
#define STH_EMI_AO_SLEEP_PROT_MASK             (g_sth_emicfg_ao_mem_base + 0x000)  /* 0x1030E000 */
#define STH_EMI_AO_GLITCH_PROT_EN              (g_sth_emicfg_ao_mem_base + 0x080)  /* 0x1030E080 */
#define STH_EMI_AO_GLITCH_PROT_RDY             (g_sth_emicfg_ao_mem_base + 0x08C)  /* 0x1030E08C */
#define STH_EMI_AO_M6M7_IDLE_BIT_EN_1          (g_sth_emicfg_ao_mem_base + 0x228)  /* 0x1030E228 */
#define STH_EMI_AO_M6M7_IDLE_BIT_EN_0          (g_sth_emicfg_ao_mem_base + 0x22C)  /* 0x1030E22C */

#endif /* __GPUFREQ_REG_MT6899_H__ */
