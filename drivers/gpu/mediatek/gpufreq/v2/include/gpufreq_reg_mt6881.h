/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2025 MediaTek Inc.
 */

#ifndef __GPUFREQ_REG_MT6881_H__
#define __GPUFREQ_REG_MT6881_H__

#include <linux/io.h>
#include <linux/bits.h>

/**************************************************
 * GPUFREQ Register Operation
 **************************************************/

/**************************************************
 * GPUFREQ Register Definition
 **************************************************/
#define MALI_BASE                              (0x3A000000)
#define MALI_GPU_ID                            (g_mali_base + 0x000)               /* 0x3A000000 */

#define MFG_TOP_CFG_BASE                       (0x3A500000)
#define MFG_TOP_CG_CON                         (g_mfg_top_base + 0x0000)           /* 0x3A500000 */
#define MFG_TOP_CKMUX_CON                      (g_mfg_top_base + 0x00E8)           /* 0x3A5000E8 */
#define MFG_TOP_DEBUG_SEL                      (g_mfg_top_base + 0x0170)           /* 0x3A500170 */
#define MFG_TOP_DEBUG_ASYNC                    (g_mfg_top_base + 0x017C)           /* 0x3A50017C */
#define MFG_TOP_POWER_TRACKER_SETTING          (g_mfg_top_base + 0x0FE0)           /* 0x3A500FE0 */
#define MFG_TOP_POWER_TRACKER_PDC_STATUS0      (g_mfg_top_base + 0x0FE4)           /* 0x3A500FE4 */
#define MFG_TOP_POWER_TRACKER_PDC_STATUS1      (g_mfg_top_base + 0x0FE8)           /* 0x3A500FE8 */
#define MFG_TOP_POWER_TRACKER_PDC_STATUS2      (g_mfg_top_base + 0x0FEC)           /* 0x3A500FEC */

#define MFG_GPUEB_BUS_TRACKER_BASE             (0x3D1A0000)
#define MFG_GPUEB_BUS_DBG_CON_0                (g_mfg_eb_bus_trk_base + 0x000)     /* 0x3D1A0000 */
#define MFG_GPUEB_BUS_TIMEOUT_INFO             (g_mfg_eb_bus_trk_base + 0x028)     /* 0x3D1A0028 */
#define MFG_GPUEB_BUS_SYSTIMER_LATCH_SLVERR_L  (g_mfg_eb_bus_trk_base + 0x040)     /* 0x3D1A0040 */
#define MFG_GPUEB_BUS_SYSTIMER_LATCH_SLVERR_H  (g_mfg_eb_bus_trk_base + 0x044)     /* 0x3D1A0044 */
#define MFG_GPUEB_BUS_SYSTIMER_LATCH_L         (g_mfg_eb_bus_trk_base + 0x048)     /* 0x3D1A0048 */
#define MFG_GPUEB_BUS_SYSTIMER_LATCH_H         (g_mfg_eb_bus_trk_base + 0x04C)     /* 0x3D1A004C */
#define MFG_GPUEB_BUS_AR_SLVERR_ADDR_L         (g_mfg_eb_bus_trk_base + 0x080)     /* 0x3D1A0080 */
#define MFG_GPUEB_BUS_AR_SLVERR_ADDR_H         (g_mfg_eb_bus_trk_base + 0x084)     /* 0x3D1A0084 */
#define MFG_GPUEB_BUS_AR_SLVERR_ID             (g_mfg_eb_bus_trk_base + 0x088)     /* 0x3D1A0088 */
#define MFG_GPUEB_BUS_AR_SLVERR_LOG            (g_mfg_eb_bus_trk_base + 0x08C)     /* 0x3D1A008C */
#define MFG_GPUEB_BUS_AW_SLVERR_ADDR_L         (g_mfg_eb_bus_trk_base + 0x090)     /* 0x3D1A0090 */
#define MFG_GPUEB_BUS_AW_SLVERR_ADDR_H         (g_mfg_eb_bus_trk_base + 0x094)     /* 0x3D1A0094 */
#define MFG_GPUEB_BUS_AW_SLVERR_ID             (g_mfg_eb_bus_trk_base + 0x098)     /* 0x3D1A0098 */
#define MFG_GPUEB_BUS_AW_SLVERR_LOG            (g_mfg_eb_bus_trk_base + 0x09C)     /* 0x3D1A009C */
#define MFG_GPUEB_BUS_AR_TRACKER_LOG           (g_mfg_eb_bus_trk_base + 0x200)     /* 0x3D1A0200 */
#define MFG_GPUEB_BUS_AR_TRACKER_ID            (g_mfg_eb_bus_trk_base + 0x300)     /* 0x3D1A0300 */
#define MFG_GPUEB_BUS_AR_TRACKER_L             (g_mfg_eb_bus_trk_base + 0x400)     /* 0x3D1A0400 */
#define MFG_GPUEB_BUS_AW_TRACKER_LOG           (g_mfg_eb_bus_trk_base + 0x800)     /* 0x3D1A0800 */
#define MFG_GPUEB_BUS_AW_TRACKER_ID            (g_mfg_eb_bus_trk_base + 0x900)     /* 0x3D1A0900 */
#define MFG_GPUEB_BUS_AW_TRACKER_L             (g_mfg_eb_bus_trk_base + 0xA00)     /* 0x3D1A0A00 */

#define MFG_RPC_BASE                           (0x3D800000)
#define MFG_RPC_MFG0_PWR_CON                   (g_mfg_rpc_base + 0x0504)           /* 0x3D800504 */
#define MFG_RPC_MFG1_PWR_CON                   (g_mfg_rpc_base + 0x0500)           /* 0x3D800500 */
#define MFG_RPC_MFG2_PWR_CON                   (g_mfg_rpc_base + 0x0508)           /* 0x3D800508 */
#define MFG_RPC_MFG3_PWR_CON                   (g_mfg_rpc_base + 0x050C)           /* 0x3D80050C */
#define MFG_RPC_MFG5_PWR_CON                   (g_mfg_rpc_base + 0x0514)           /* 0x3D800514 */
#define MFG_RPC_MFG9_PWR_CON                   (g_mfg_rpc_base + 0x0524)           /* 0x3D800524 */
#define MFG_RPC_MFG10_PWR_CON                  (g_mfg_rpc_base + 0x0528)           /* 0x3D800528 */
#define MFG_RPC_MFG25_PWR_CON                  (g_mfg_rpc_base + 0x054C)           /* 0x3D800564 */
#define MFG_RPC_MFG26_PWR_CON                  (g_mfg_rpc_base + 0x0550)           /* 0x3D800568 */
#define MFG_RPC_PWR_CON_STATUS                 (g_mfg_rpc_base + 0x0600)           /* 0x3D800600 */

#define MFG_PLL0_BASE                          (0x3D810000)
#define MFG_PLL0_CON0                          (g_mfg_pll0_base + 0x008)           /* 0x3D810008 */
#define MFG_PLL0_CON1                          (g_mfg_pll0_base + 0x00C)           /* 0x3D81000C */
#define MFG_PLL0_FQMTR_CON0                    (g_mfg_pll0_base + 0x040)           /* 0x3D810040 */
#define MFG_PLL0_FQMTR_CON1                    (g_mfg_pll0_base + 0x044)           /* 0x3D810044 */

#define MFG_PLL1_BASE                          (0x3D810400)
#define MFG_PLL1_CON0                          (g_mfg_pll1_base + 0x008)           /* 0x3D810408 */
#define MFG_PLL1_CON1                          (g_mfg_pll1_base + 0x00C)           /* 0x3D81040C */
#define MFG_PLL1_FQMTR_CON0                    (g_mfg_pll1_base + 0x040)           /* 0x3D810410 */
#define MFG_PLL1_FQMTR_CON1                    (g_mfg_pll1_base + 0x044)           /* 0x3D810414 */

#define MFG_VCORE_AO_CFG_BASE                  (0x3D860000)
#define MFG_VCORE_AO_CK_FAST_REF_SEL           (g_mfg_vcore_ao_cfg_base + 0x000C)  /* 0x3D86000C */
#define MFG_VCORE_AO_PROT_EN_SET_0             (g_mfg_vcore_ao_cfg_base + 0x0080)  /* 0x3D860080 */
#define MFG_VCORE_AO_PROT_EN_CLR_0             (g_mfg_vcore_ao_cfg_base + 0x0084)  /* 0x3D860084 */
#define MFG_VCORE_AO_PROT_EN_STA_0             (g_mfg_vcore_ao_cfg_base + 0x0088)  /* 0x3D860088 */

#define MFG_VCORE_BUS_TRACKER_BASE             (0x3D910000)
#define MFG_VCORE_BUS_DBG_CON_0                (g_mfg_vcore_bus_trk_base + 0x000)  /* 0x3D910000 */
#define MFG_VCORE_BUS_TIMEOUT_INFO             (g_mfg_vcore_bus_trk_base + 0x028)  /* 0x3D910028 */
#define MFG_VCORE_BUS_SYSTIMER_LATCH_SLVERR_L  (g_mfg_vcore_bus_trk_base + 0x040)  /* 0x3D910040 */
#define MFG_VCORE_BUS_SYSTIMER_LATCH_SLVERR_H  (g_mfg_vcore_bus_trk_base + 0x044)  /* 0x3D910044 */
#define MFG_VCORE_BUS_SYSTIMER_LATCH_L         (g_mfg_vcore_bus_trk_base + 0x048)  /* 0x3D910048 */
#define MFG_VCORE_BUS_SYSTIMER_LATCH_H         (g_mfg_vcore_bus_trk_base + 0x04C)  /* 0x3D91004C */
#define MFG_VCORE_BUS_AR_SLVERR_ADDR_L         (g_mfg_vcore_bus_trk_base + 0x080)  /* 0x3D910080 */
#define MFG_VCORE_BUS_AR_SLVERR_ID             (g_mfg_vcore_bus_trk_base + 0x088)  /* 0x3D910088 */
#define MFG_VCORE_BUS_AR_SLVERR_LOG            (g_mfg_vcore_bus_trk_base + 0x08C)  /* 0x3D91008C */
#define MFG_VCORE_BUS_AW_SLVERR_ADDR_L         (g_mfg_vcore_bus_trk_base + 0x090)  /* 0x3D910090 */
#define MFG_VCORE_BUS_AW_SLVERR_ID             (g_mfg_vcore_bus_trk_base + 0x098)  /* 0x3D910098 */
#define MFG_VCORE_BUS_AW_SLVERR_LOG            (g_mfg_vcore_bus_trk_base + 0x09C)  /* 0x3D91009C */
#define MFG_VCORE_BUS_AR_TRACKER_LOG           (g_mfg_vcore_bus_trk_base + 0x200)  /* 0x3D910200 */
#define MFG_VCORE_BUS_AR_TRACKER_ID            (g_mfg_vcore_bus_trk_base + 0x300)  /* 0x3D910300 */
#define MFG_VCORE_BUS_AR_TRACKER_L             (g_mfg_vcore_bus_trk_base + 0x400)  /* 0x3D910400 */
#define MFG_VCORE_BUS_AW_TRACKER_LOG           (g_mfg_vcore_bus_trk_base + 0x800)  /* 0x3D910800 */
#define MFG_VCORE_BUS_AW_TRACKER_ID            (g_mfg_vcore_bus_trk_base + 0x900)  /* 0x3D910900 */
#define MFG_VCORE_BUS_AW_TRACKER_L             (g_mfg_vcore_bus_trk_base + 0xA00)  /* 0x3D910A00 */

#define SPM_BASE                               (0x1C001000)
#define SPM_SPM2GPUPM_CON                      (g_sleep + 0x0410)                  /* 0x1C001410 */
#define SPM_SRC_REQ                            (g_sleep + 0x0818)                  /* 0x1C001818 */
#define SPM_MFG0_PWR_CON                       (g_sleep + 0x0EB0)                  /* 0x1C001EB0 */
#define SPM_SOC_BUCK_ISO_CON                   (g_sleep + 0x0F28)                  /* 0x1C001F28 */
#define SPM_SOC_BUCK_ISO_CON_SET               (g_sleep + 0x0F2C)                  /* 0x1C001F2C */
#define SPM_SOC_BUCK_ISO_CON_CLR               (g_sleep + 0x0F30)                  /* 0x1C001F30 */
#define SPM_GPU_PWR_STATUS                     (g_sleep + 0x0F50)                  /* 0x1C001F50 */
#define SPM_GPU_PWR_STATUS_2ND                 (g_sleep + 0x0F54)                  /* 0x1C001F54 */

#endif /* __GPUFREQ_REG_MT6881_H__ */
