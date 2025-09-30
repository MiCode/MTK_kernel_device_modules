/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef __GPUFREQ_REG_MT6993_H__
#define __GPUFREQ_REG_MT6993_H__

#include <linux/io.h>
#include <linux/bits.h>

/**************************************************
 * GPUFREQ Register Operation
 **************************************************/

/**************************************************
 * GPUFREQ Register Definition
 **************************************************/
#define MALI_BASE                              (0x48000000)
#define MALI_GPU_ID                            (g_mali_base + 0x000)               /* 0x48000000 */

#define MFG_TOP_CFG_BASE                       (0x3A500000)
#define MFG_TOP_CG_CON                         (g_mfg_top_base + 0x0000)           /* 0x48500000 */
#define MFG_TOP_SRAM_FUL_SEL_ULV               (g_mfg_top_base + 0x0080)           /* 0x48500080 */
#define MFG_TOP_SRAM_FUL_SEL_ULV_TOP           (g_mfg_top_base + 0x0084)           /* 0x48500084 */
#define MFG_TOP_DEBUG_SEL                      (g_mfg_top_base + 0x0170)           /* 0x48500170 */
#define MFG_TOP_DEBUG_ASYNC                    (g_mfg_top_base + 0x017C)           /* 0x4850017C */
#define MFG_TOP_BRISKET_ST0_AO_CFG_0           (g_mfg_top_base + 0x0370)           /* 0x48500370 */
#define MFG_TOP_BRISKET_ST1_AO_CFG_0           (g_mfg_top_base + 0x0374)           /* 0x48500374 */
#define MFG_TOP_BRISKET_ST2_AO_CFG_0           (g_mfg_top_base + 0x0378)           /* 0x48500378 */
#define MFG_TOP_BRISKET_ST3_AO_CFG_0           (g_mfg_top_base + 0x037C)           /* 0x4850037C */
#define MFG_TOP_BRISKET_ST4_AO_CFG_0           (g_mfg_top_base + 0x03A0)           /* 0x485003A0 */
#define MFG_TOP_BRISKET_ST5_AO_CFG_0           (g_mfg_top_base + 0x03A4)           /* 0x485003A4 */
#define MFG_TOP_AXI_SLPPROT_FREQ_BRIDGE        (g_mfg_top_base + 0x082C)           /* 0x4850082C */
#define MFG_TOP_ACP_SLPPROT_FREQ_BRIDGE        (g_mfg_top_base + 0x0834)           /* 0x48500834 */
#define MFG_TOP_DEFAULT_DELSEL_00              (g_mfg_top_base + 0x0C80)           /* 0x48500C80 */
#define MFG_TOP_POWER_TRACKER_SETTING          (g_mfg_top_base + 0x0FE0)           /* 0x3A500FE0 */
#define MFG_TOP_POWER_TRACKER_PDC_STATUS0      (g_mfg_top_base + 0x0FE4)           /* 0x3A500FE4 */
#define MFG_TOP_POWER_TRACKER_PDC_STATUS1      (g_mfg_top_base + 0x0FE8)           /* 0x3A500FE8 */
#define MFG_TOP_POWER_TRACKER_PDC_STATUS2      (g_mfg_top_base + 0x0FEC)           /* 0x3A500FEC */
#define MFG_TOP_VOLT_TRACKER_CON_1             (g_mfg_top_base + 0x1644)           /* 0x48501644 */
#define MFG_TOP_VOLT_TRACKER_CON_2             (g_mfg_top_base + 0x1648)           /* 0x48501648 */
#define MFG_TOP_VOLT_TRACKER_CON_3             (g_mfg_top_base + 0x164C)           /* 0x4850164C */
#define MFG_TOP_VOLT_TRACKER_CON_4             (g_mfg_top_base + 0x1910)           /* 0x48501910 */
#define MFG_TOP_VOLT_TRACKER_CON_5             (g_mfg_top_base + 0x1914)           /* 0x48501914 */
#define MFG_TOP_VOLT_TRACKER_CON_6             (g_mfg_top_base + 0x1918)           /* 0x48501918 */
#define MFG_TOP_VOLT_TRACKER_CON_7             (g_mfg_top_base + 0x191C)           /* 0x4850191C */
#define MFG_TOP_VOLT_TRACKER_CON_8             (g_mfg_top_base + 0x1920)           /* 0x48501920 */
#define MFG_TOP_TOP_FREQ_TRACKER_CON_0         (g_mfg_top_base + 0x1990)           /* 0x48501990 */
#define MFG_TOP_TOP_FREQ_TRACKER_CON_1         (g_mfg_top_base + 0x1994)           /* 0x48501994 */
#define MFG_TOP_TOP_FREQ_TRACKER_CON_2         (g_mfg_top_base + 0x1998)           /* 0x48501998 */
#define MFG_TOP_TOP_FREQ_TRACKER_CON_3         (g_mfg_top_base + 0x199C)           /* 0x4850199C */
#define MFG_TOP_STACK_FREQ_TRACKER_CON_0       (g_mfg_top_base + 0x19A0)           /* 0x485019A0 */
#define MFG_TOP_STACK_FREQ_TRACKER_CON_1       (g_mfg_top_base + 0x19A4)           /* 0x485019A4 */
#define MFG_TOP_STACK_FREQ_TRACKER_CON_2       (g_mfg_top_base + 0x19A8)           /* 0x485019A8 */
#define MFG_TOP_STACK_FREQ_TRACKER_CON_3       (g_mfg_top_base + 0x19AC)           /* 0x485019AC */

#define MFG_SMMU_BASE                          (0x48600000)
#define MFG_SMMU_CR0                           (g_mfg_smmu_base + 0x0020)          /* 0x48600020 */
#define MFG_SMMU_GBPA                          (g_mfg_smmu_base + 0x0044)          /* 0x48600044 */

#define MFG_VGPU_BUS_TRACKER_BASE              (0x48810000)
#define MFG_VGPU_BUS_DBG_CON_0                 (g_mfg_vgpu_bus_trk_base + 0x000)   /* 0x48810000 */
#define MFG_VGPU_BUS_TIMEOUT_INFO              (g_mfg_vgpu_bus_trk_base + 0x028)   /* 0x48810028 */
#define MFG_VGPU_BUS_SYSTIMER_LATCH_SLVERR_L   (g_mfg_vgpu_bus_trk_base + 0x040)   /* 0x48810040 */
#define MFG_VGPU_BUS_SYSTIMER_LATCH_SLVERR_H   (g_mfg_vgpu_bus_trk_base + 0x044)   /* 0x48810044 */
#define MFG_VGPU_BUS_SYSTIMER_LATCH_L          (g_mfg_vgpu_bus_trk_base + 0x048)   /* 0x48810048 */
#define MFG_VGPU_BUS_SYSTIMER_LATCH_H          (g_mfg_vgpu_bus_trk_base + 0x04C)   /* 0x4881004C */
#define MFG_VGPU_BUS_AR_SLVERR_ADDR_L          (g_mfg_vgpu_bus_trk_base + 0x080)   /* 0x48810080 */
#define MFG_VGPU_BUS_AR_SLVERR_ADDR_H          (g_mfg_vgpu_bus_trk_base + 0x084)   /* 0x48810084 */
#define MFG_VGPU_BUS_AR_SLVERR_ID              (g_mfg_vgpu_bus_trk_base + 0x088)   /* 0x48810088 */
#define MFG_VGPU_BUS_AR_SLVERR_LOG             (g_mfg_vgpu_bus_trk_base + 0x08C)   /* 0x4881008C */
#define MFG_VGPU_BUS_AW_SLVERR_ADDR_L          (g_mfg_vgpu_bus_trk_base + 0x090)   /* 0x48810090 */
#define MFG_VGPU_BUS_AW_SLVERR_ADDR_H          (g_mfg_vgpu_bus_trk_base + 0x094)   /* 0x48810094 */
#define MFG_VGPU_BUS_AW_SLVERR_ID              (g_mfg_vgpu_bus_trk_base + 0x098)   /* 0x48810098 */
#define MFG_VGPU_BUS_AW_SLVERR_LOG             (g_mfg_vgpu_bus_trk_base + 0x09C)   /* 0x4881009C */
#define MFG_VGPU_BUS_AR_TRACKER_LOG            (g_mfg_vgpu_bus_trk_base + 0x200)   /* 0x48810200 */
#define MFG_VGPU_BUS_AR_TRACKER_ID             (g_mfg_vgpu_bus_trk_base + 0x300)   /* 0x48810300 */
#define MFG_VGPU_BUS_AR_TRACKER_L              (g_mfg_vgpu_bus_trk_base + 0x400)   /* 0x48810400 */
#define MFG_VGPU_BUS_AW_TRACKER_LOG            (g_mfg_vgpu_bus_trk_base + 0x800)   /* 0x48810800 */
#define MFG_VGPU_BUS_AW_TRACKER_ID             (g_mfg_vgpu_bus_trk_base + 0x900)   /* 0x48810900 */
#define MFG_VGPU_BUS_AW_TRACKER_L              (g_mfg_vgpu_bus_trk_base + 0xA00)   /* 0x48810A00 */

#define MFG_GPUEB_BUS_TRACKER_BASE             (0x4B1A0000)
#define MFG_GPUEB_BUS_DBG_CON_0                (g_mfg_eb_bus_trk_base + 0x000)     /* 0x4B1A0000 */
#define MFG_GPUEB_BUS_TIMEOUT_INFO             (g_mfg_eb_bus_trk_base + 0x028)     /* 0x4B1A0028 */
#define MFG_GPUEB_BUS_SYSTIMER_LATCH_SLVERR_L  (g_mfg_eb_bus_trk_base + 0x040)     /* 0x4B1A0040 */
#define MFG_GPUEB_BUS_SYSTIMER_LATCH_SLVERR_H  (g_mfg_eb_bus_trk_base + 0x044)     /* 0x4B1A0044 */
#define MFG_GPUEB_BUS_SYSTIMER_LATCH_L         (g_mfg_eb_bus_trk_base + 0x048)     /* 0x4B1A0048 */
#define MFG_GPUEB_BUS_SYSTIMER_LATCH_H         (g_mfg_eb_bus_trk_base + 0x04C)     /* 0x4B1A004C */
#define MFG_GPUEB_BUS_AR_SLVERR_ADDR_L         (g_mfg_eb_bus_trk_base + 0x080)     /* 0x4B1A0080 */
#define MFG_GPUEB_BUS_AR_SLVERR_ADDR_H         (g_mfg_eb_bus_trk_base + 0x084)     /* 0x4B1A0084 */
#define MFG_GPUEB_BUS_AR_SLVERR_ID             (g_mfg_eb_bus_trk_base + 0x088)     /* 0x4B1A0088 */
#define MFG_GPUEB_BUS_AR_SLVERR_LOG            (g_mfg_eb_bus_trk_base + 0x08C)     /* 0x4B1A008C */
#define MFG_GPUEB_BUS_AW_SLVERR_ADDR_L         (g_mfg_eb_bus_trk_base + 0x090)     /* 0x4B1A0090 */
#define MFG_GPUEB_BUS_AW_SLVERR_ADDR_H         (g_mfg_eb_bus_trk_base + 0x094)     /* 0x4B1A0094 */
#define MFG_GPUEB_BUS_AW_SLVERR_ID             (g_mfg_eb_bus_trk_base + 0x098)     /* 0x4B1A0098 */
#define MFG_GPUEB_BUS_AW_SLVERR_LOG            (g_mfg_eb_bus_trk_base + 0x09C)     /* 0x4B1A009C */
#define MFG_GPUEB_BUS_AR_TRACKER_LOG           (g_mfg_eb_bus_trk_base + 0x200)     /* 0x4B1A0200 */
#define MFG_GPUEB_BUS_AR_TRACKER_ID            (g_mfg_eb_bus_trk_base + 0x300)     /* 0x4B1A0300 */
#define MFG_GPUEB_BUS_AR_TRACKER_L             (g_mfg_eb_bus_trk_base + 0x400)     /* 0x4B1A0400 */
#define MFG_GPUEB_BUS_AW_TRACKER_LOG           (g_mfg_eb_bus_trk_base + 0x800)     /* 0x4B1A0800 */
#define MFG_GPUEB_BUS_AW_TRACKER_ID            (g_mfg_eb_bus_trk_base + 0x900)     /* 0x4B1A0900 */
#define MFG_GPUEB_BUS_AW_TRACKER_L             (g_mfg_eb_bus_trk_base + 0xA00)     /* 0x4B1A0A00 */

#define MFG_ACP_GALS_TOP_BASE                  (0x4B420300)
#define MFG_ACP_GALS0_SLV_RX_STA0              (g_mfg_acp_gals_top_base + 0x20)    /* 0x4B420320 */
#define MFG_ACP_GALS0_SLV_TX_STA0              (g_mfg_acp_gals_top_base + 0x64)    /* 0x4B420364 */

#define MFG_RPC_BASE                           (0x4B800000)
#define MFG_RPC_MFG0_PWR_CON                   (g_mfg_rpc_base + 0x0504)           /* 0x4B800504 */
#define MFG_RPC_MFG1_PWR_CON                   (g_mfg_rpc_base + 0x0500)           /* 0x4B800500 */
#define MFG_RPC_MFG37_PWR_CON                  (g_mfg_rpc_base + 0x0594)           /* 0x4B800594 */
#define MFG_RPC_MFG2_PWR_CON                   (g_mfg_rpc_base + 0x0508)           /* 0x4B800508 */
#define MFG_RPC_MFG3_PWR_CON                   (g_mfg_rpc_base + 0x050C)           /* 0x4B80050C */
#define MFG_RPC_MFG4_PWR_CON                   (g_mfg_rpc_base + 0x0510)           /* 0x4B800510 */
#define MFG_RPC_MFG5_PWR_CON                   (g_mfg_rpc_base + 0x0514)           /* 0x4B800514 */
#define MFG_RPC_MFG22_PWR_CON                  (g_mfg_rpc_base + 0x0558)           /* 0x4B800558 */
#define MFG_RPC_MFG6_PWR_CON                   (g_mfg_rpc_base + 0x0518)           /* 0x4B800518 */
#define MFG_RPC_MFG7_PWR_CON                   (g_mfg_rpc_base + 0x051C)           /* 0x4B80051C */
#define MFG_RPC_MFG9_PWR_CON                   (g_mfg_rpc_base + 0x0524)           /* 0x4B800524 */
#define MFG_RPC_MFG10_PWR_CON                  (g_mfg_rpc_base + 0x0528)           /* 0x4B800528 */
#define MFG_RPC_MFG11_PWR_CON                  (g_mfg_rpc_base + 0x052C)           /* 0x4B80052C */
#define MFG_RPC_MFG12_PWR_CON                  (g_mfg_rpc_base + 0x0530)           /* 0x4B800530 */
#define MFG_RPC_MFG13_PWR_CON                  (g_mfg_rpc_base + 0x0534)           /* 0x4B800534 */
#define MFG_RPC_MFG14_PWR_CON                  (g_mfg_rpc_base + 0x0538)           /* 0x4B800538 */
#define MFG_RPC_MFG15_PWR_CON                  (g_mfg_rpc_base + 0x053C)           /* 0x4B80053C */
#define MFG_RPC_MFG16_PWR_CON                  (g_mfg_rpc_base + 0x0540)           /* 0x4B800540 */
#define MFG_RPC_MFG17_PWR_CON                  (g_mfg_rpc_base + 0x0544)           /* 0x4B800544 */
#define MFG_RPC_MFG18_PWR_CON                  (g_mfg_rpc_base + 0x0548)           /* 0x4B800548 */
#define MFG_RPC_MFG19_PWR_CON                  (g_mfg_rpc_base + 0x054C)           /* 0x4B80054C */
#define MFG_RPC_MFG20_PWR_CON                  (g_mfg_rpc_base + 0x0550)           /* 0x4B800550 */
#define MFG_RPC_PWR_CON_STATUS                 (g_mfg_rpc_base + 0x0600)           /* 0x4B800600 */
#define MFG_RPC_BRISKET_TOP_AO_CFG_0           (g_mfg_rpc_base + 0x0620)           /* 0x4B800620 */

#define MFG_PLL_BASE                           (0x4B810000)
#define MFG_PLL_CON0                           (g_mfg_pll_base + 0x008)            /* 0x4B810008 */
#define MFG_PLL_CON1                           (g_mfg_pll_base + 0x00C)            /* 0x4B81000C */
#define MFG_PLL_CON5                           (g_mfg_pll_base + 0x01C)            /* 0x4B81001C */
#define MFG_PLL_FQMTR_CON0                     (g_mfg_pll_base + 0x040)            /* 0x4B810040 */
#define MFG_PLL_FQMTR_CON1                     (g_mfg_pll_base + 0x044)            /* 0x4B810044 */

#define MFG_PLL_SC0_BASE                       (0x4B810400)
#define MFG_PLL_SC0_CON0                       (g_mfg_pll_sc0_base + 0x008)        /* 0x4B810408 */
#define MFG_PLL_SC0_CON1                       (g_mfg_pll_sc0_base + 0x00C)        /* 0x4B81040C */
#define MFG_PLL_SC0_CON5                       (g_mfg_pll_sc0_base + 0x01C)        /* 0x4B81041C */
#define MFG_PLL_SC0_FQMTR_CON0                 (g_mfg_pll_sc0_base + 0x040)        /* 0x4B810410 */
#define MFG_PLL_SC0_FQMTR_CON1                 (g_mfg_pll_sc0_base + 0x044)        /* 0x4B810414 */

#define MFG_VCORE_AO_CFG_BASE                  (0x4B860000)
#define MFG_VCORE_AO_CK_FAST_REF_SEL           (g_mfg_vcore_ao_cfg_base + 0x000C)  /* 0x4B86000C */
#define MFG_VCORE_AO_PROT_EN_SET_0             (g_mfg_vcore_ao_cfg_base + 0x0080)  /* 0x4B860080 */
#define MFG_VCORE_AO_PROT_EN_CLR_0             (g_mfg_vcore_ao_cfg_base + 0x0084)  /* 0x4B860084 */
#define MFG_VCORE_AO_PROT_EN_STA_0             (g_mfg_vcore_ao_cfg_base + 0x0088)  /* 0x4B860088 */
#define MFG_VCORE_AO_DREQ_CONFIG               (g_mfg_vcore_ao_cfg_base + 0x00B4)  /* 0x4B8600B4 */

#define MFG_VCORE_BUS_TRACKER_BASE             (0x4B910000)
#define MFG_VCORE_BUS_DBG_CON_0                (g_mfg_vcore_bus_trk_base + 0x000)  /* 0x4B910000 */
#define MFG_VCORE_BUS_TIMEOUT_INFO             (g_mfg_vcore_bus_trk_base + 0x028)  /* 0x4B910028 */
#define MFG_VCORE_BUS_SYSTIMER_LATCH_SLVERR_L  (g_mfg_vcore_bus_trk_base + 0x040)  /* 0x4B910040 */
#define MFG_VCORE_BUS_SYSTIMER_LATCH_SLVERR_H  (g_mfg_vcore_bus_trk_base + 0x044)  /* 0x4B910044 */
#define MFG_VCORE_BUS_SYSTIMER_LATCH_L         (g_mfg_vcore_bus_trk_base + 0x048)  /* 0x4B910048 */
#define MFG_VCORE_BUS_SYSTIMER_LATCH_H         (g_mfg_vcore_bus_trk_base + 0x04C)  /* 0x4B91004C */
#define MFG_VCORE_BUS_AR_SLVERR_ADDR_L         (g_mfg_vcore_bus_trk_base + 0x080)  /* 0x4B910080 */
#define MFG_VCORE_BUS_AR_SLVERR_ID             (g_mfg_vcore_bus_trk_base + 0x088)  /* 0x4B910088 */
#define MFG_VCORE_BUS_AR_SLVERR_LOG            (g_mfg_vcore_bus_trk_base + 0x08C)  /* 0x4B91008C */
#define MFG_VCORE_BUS_AW_SLVERR_ADDR_L         (g_mfg_vcore_bus_trk_base + 0x090)  /* 0x4B910090 */
#define MFG_VCORE_BUS_AW_SLVERR_ID             (g_mfg_vcore_bus_trk_base + 0x098)  /* 0x4B910098 */
#define MFG_VCORE_BUS_AW_SLVERR_LOG            (g_mfg_vcore_bus_trk_base + 0x09C)  /* 0x4B91009C */
#define MFG_VCORE_BUS_AR_TRACKER_LOG           (g_mfg_vcore_bus_trk_base + 0x200)  /* 0x4B910200 */
#define MFG_VCORE_BUS_AR_TRACKER_ID            (g_mfg_vcore_bus_trk_base + 0x300)  /* 0x4B910300 */
#define MFG_VCORE_BUS_AR_TRACKER_L             (g_mfg_vcore_bus_trk_base + 0x400)  /* 0x4B910400 */
#define MFG_VCORE_BUS_AW_TRACKER_LOG           (g_mfg_vcore_bus_trk_base + 0x800)  /* 0x4B910800 */
#define MFG_VCORE_BUS_AW_TRACKER_ID            (g_mfg_vcore_bus_trk_base + 0x900)  /* 0x4B910900 */
#define MFG_VCORE_BUS_AW_TRACKER_L             (g_mfg_vcore_bus_trk_base + 0xA00)  /* 0x4B910A00 */

#define EMI_IFR_NONCOH_GALS_BASE               (0x11014000)
#define EMI_IFR_NONCOH_GALS_NTH_APU_M0_RX_STA0 (g_emi_infra_noncoh_gals + 0x120)   /* 0x11014120 */
#define EMI_IFR_NONCOH_GALS_NTH_APU_M0_TX_STA0 (g_emi_infra_noncoh_gals + 0x164)   /* 0x11014164 */
#define EMI_IFR_NONCOH_GALS_NTH_APU_M1_RX_STA0 (g_emi_infra_noncoh_gals + 0x220)   /* 0x11014220 */
#define EMI_IFR_NONCOH_GALS_NTH_APU_M1_TX_STA0 (g_emi_infra_noncoh_gals + 0x264)   /* 0x11014264 */
#define EMI_IFR_NONCOH_GALS_STH_APU_M0_RX_STA0 (g_emi_infra_noncoh_gals + 0x320)   /* 0x11014320 */
#define EMI_IFR_NONCOH_GALS_STH_APU_M0_TX_STA0 (g_emi_infra_noncoh_gals + 0x364)   /* 0x11014364 */
#define EMI_IFR_NONCOH_GALS_STH_APU_M1_RX_STA0 (g_emi_infra_noncoh_gals + 0x420)   /* 0x11014420 */
#define EMI_IFR_NONCOH_GALS_STH_APU_M1_TX_STA0 (g_emi_infra_noncoh_gals + 0x464)   /* 0x11014464 */
#define EMI_IFR_NONCOH_GALS_STH_IFR_M0_RX_STA0 (g_emi_infra_noncoh_gals + 0x520)   /* 0x11014520 */
#define EMI_IFR_NONCOH_GALS_STH_IFR_M0_TX_STA0 (g_emi_infra_noncoh_gals + 0x564)   /* 0x11014564 */
#define EMI_IFR_NONCOH_GALS_STH_IFR_M1_RX_STA0 (g_emi_infra_noncoh_gals + 0x620)   /* 0x11014620 */
#define EMI_IFR_NONCOH_GALS_STH_IFR_M1_TX_STA0 (g_emi_infra_noncoh_gals + 0x664)   /* 0x11014664 */
#define EMI_IFR_NONCOH_GALS_NTH_IFR_M0_RX_STA0 (g_emi_infra_noncoh_gals + 0x720)   /* 0x11014720 */
#define EMI_IFR_NONCOH_GALS_NTH_IFR_M0_TX_STA0 (g_emi_infra_noncoh_gals + 0x764)   /* 0x11014764 */
#define EMI_IFR_NONCOH_GALS_NTH_IFR_M1_RX_STA0 (g_emi_infra_noncoh_gals + 0x820)   /* 0x11014820 */
#define EMI_IFR_NONCOH_GALS_NTH_IFR_M1_TX_STA0 (g_emi_infra_noncoh_gals + 0x864)   /* 0x11014864 */
#define EMI_IFR_NONCOH_GALS_MM_M1_RX_STA0      (g_emi_infra_noncoh_gals + 0x920)   /* 0x11014920 */
#define EMI_IFR_NONCOH_GALS_MM_M1_TX_STA0      (g_emi_infra_noncoh_gals + 0x964)   /* 0x11014964 */
#define EMI_IFR_NONCOH_GALS_MM_M0_RX_STA0      (g_emi_infra_noncoh_gals + 0xA20)   /* 0x11014A20 */
#define EMI_IFR_NONCOH_GALS_MM_M0_TX_STA0      (g_emi_infra_noncoh_gals + 0xA64)   /* 0x11014A64 */

#define EMI_IFR_CFG_BASE                       (0x11025000)
#define EMI_IFR_CFG_MFG_EMI1_STH_GALS          (emi_infra_cfg_base + 0x83C)        /* 0x1102583C */
#define EMI_IFR_CFG_MFG_EMI1_NTH_GALS          (emi_infra_cfg_base + 0x840)        /* 0x11025840 */
#define EMI_IFR_CFG_MFG_EMI0_STH_GALS          (emi_infra_cfg_base + 0x844)        /* 0x11025844 */
#define EMI_IFR_CFG_MFG_EMI0_NTH_GALS          (emi_infra_cfg_base + 0x848)        /* 0x11025848 */

#define EMI_IFR_PDN_BRCM_BASE                  (0x1102B000)
#define EMI_IFR_NTH_M6_RW_MI_CTRL              (g_emi_infra_pdn_bcrm_base + 0x0A8) /* 0x1102B0A8 */
#define EMI_IFR_NTH_M7_RW_MI_CTRL              (g_emi_infra_pdn_bcrm_base + 0x0B4) /* 0x1102B0B4 */
#define EMI_IFR_STH_M6_RW_MI_CTRL              (g_emi_infra_pdn_bcrm_base + 0x0C0) /* 0x1102B0C0 */
#define EMI_IFR_STH_M7_RW_MI_CTRL              (g_emi_infra_pdn_bcrm_base + 0x0CC) /* 0x1102B0CC */
#define EMI_IFR_ACP_MFG_DVM_PROT_CTRL          (g_emi_infra_pdn_bcrm_base + 0x160) /* 0x1102B160 */
#define EMI_IFR_ACP_TCU_EFP_M_CTRL             (g_emi_infra_pdn_bcrm_base + 0x1A8) /* 0x1102B1A8 */
#define EMI_IFR_ACP_DVM_SI_CTRL                (g_emi_infra_pdn_bcrm_base + 0x1B0) /* 0x1102B1B0 */
#define EMI_IFR_ACP_CHI0_RW_MI_CTRL            (g_emi_infra_pdn_bcrm_base + 0x174) /* 0x1102B174 */
#define EMI_IFR_ACP_CHI1_RW_MI_CTRL            (g_emi_infra_pdn_bcrm_base + 0x180) /* 0x1102B180 */
#define EMI_IFR_ACP_CHI2_RW_MI_CTRL            (g_emi_infra_pdn_bcrm_base + 0x18C) /* 0x1102B18C */
#define EMI_IFR_ACP_CHI3_RW_MI_CTRL            (g_emi_infra_pdn_bcrm_base + 0x198) /* 0x1102B198 */
#define EMI_IFR_MFG_NTH_M0_PROT_CTRL           (g_emi_infra_pdn_bcrm_base + 0x1DC) /* 0x1102B1DC */
#define EMI_IFR_MFG_NTH_M1_PROT_CTRL           (g_emi_infra_pdn_bcrm_base + 0x1E4) /* 0x1102B1E4 */
#define EMI_IFR_MFG_STH_M0_PROT_CTRL           (g_emi_infra_pdn_bcrm_base + 0x1EC) /* 0x1102B1EC */
#define EMI_IFR_MFG_STH_M1_PROT_CTRL           (g_emi_infra_pdn_bcrm_base + 0x1F4) /* 0x1102B1F4 */
#define EMI_IFR_MFG_NTH_M0_CTRL                (g_emi_infra_pdn_bcrm_base + 0x21C) /* 0x1102B21C */
#define EMI_IFR_MFG_NTH_M1_CTRL                (g_emi_infra_pdn_bcrm_base + 0x220) /* 0x1102B220 */
#define EMI_IFR_MFG_STH_M0_CTRL                (g_emi_infra_pdn_bcrm_base + 0x224) /* 0x1102B224 */
#define EMI_IFR_MFG_STH_M1_CTRL                (g_emi_infra_pdn_bcrm_base + 0x228) /* 0x1102B228 */
#define EMI_IFR_MFG_FAKE0_MI_CTRL              (g_emi_infra_pdn_bcrm_base + 0x22C) /* 0x1102B22C */
#define EMI_IFR_MFG_FAKE1_MI_CTRL              (g_emi_infra_pdn_bcrm_base + 0x230) /* 0x1102B230 */
#define EMI_IFR_MFG_FAKE2_MI_CTRL              (g_emi_infra_pdn_bcrm_base + 0x234) /* 0x1102B234 */
#define EMI_IFR_MFG_FAKE3_MI_CTRL              (g_emi_infra_pdn_bcrm_base + 0x238) /* 0x1102B238 */

#define EMI_IFR_ACP_RSI_BASE                   (0x11037000)
#define EMI_IFR_ACP_RSI_AWOSTD_M0              (g_emi_infra_acp_rsi_base + 0x80)   /* 0x11037080 */
#define EMI_IFR_ACP_RSI_AWOSTD_M1              (g_emi_infra_acp_rsi_base + 0x84)   /* 0x11037084 */
#define EMI_IFR_ACP_RSI_WOSTD_M0               (g_emi_infra_acp_rsi_base + 0xA0)   /* 0x110370A0 */
#define EMI_IFR_ACP_RSI_WOSTD_M1               (g_emi_infra_acp_rsi_base + 0xA4)   /* 0x110370A4 */
#define EMI_IFR_ACP_RSI_AROSTD_M0              (g_emi_infra_acp_rsi_base + 0xC0)   /* 0x110370C0 */
#define EMI_IFR_ACP_RSI_AROSTD_M1              (g_emi_infra_acp_rsi_base + 0xC4)   /* 0x110370C4 */

#define SPM_BASE                               (0x1C004000)
#define SPM_SPM2GPUPM_CON                      (g_sleep + 0x0410)                  /* 0x1C004410 */
#define SPM_SRC_REQ                            (g_sleep + 0x0818)                  /* 0x1C004818 */
#define SPM_SOC_BUCK_ISO_CON                   (g_sleep + 0x0F30)                  /* 0x1C004F30 */
#define SPM_SOC_BUCK_ISO_CON_SET               (g_sleep + 0x0F34)                  /* 0x1C004F34 */
#define SPM_SOC_BUCK_ISO_CON_CLR               (g_sleep + 0x0F38)                  /* 0x1C004F38 */

#endif /* __GPUFREQ_REG_MT6993_H__ */
