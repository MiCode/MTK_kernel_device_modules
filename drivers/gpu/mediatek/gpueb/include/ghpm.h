/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __GHPM_H__
#define __GHPM_H__

#include <linux/platform_device.h>

#define GHPM_IPI_TIMEOUT                   (5000)
#define GPUEB_WAIT_TIMEOUT                 (5000)

#define MFG_RPC_SLV_CTRL_UPDATE            (g_mfg_rpc_base + 0x0068)          /* 0x4B800068 */
#define MFG_RPC_SLV_SLP_PROT_RDY_STA       (g_mfg_rpc_base + 0x0078)          /* 0x4B800078 */
#define MFG_RPC_MFG1_PWR_CON               (g_mfg_rpc_base + 0x0500)          /* 0x4B800500 */
#define MFG_RPC_MFG0_PWR_CON               (g_mfg_rpc_base + 0x0504)          /* 0x4B800504 */
#define MFG_RPC_MFG2_PWR_CON               (g_mfg_rpc_base + 0x0508)          /* 0x4B800508 */
#define MFG_RPC_MFG37_PWR_CON              (g_mfg_rpc_base + 0x0594)          /* 0x4B800594 */
#define MFG_GHPM_CFG0_CON                  (g_mfg_rpc_base + 0x0800)          /* 0x4B800800 */
#define APB_WDT_RST_EN                     (BIT(24))
#define POLL_WDT_RST_EN                    (BIT(23))
#define HRE_BAK_ACK_WDT_RST_EN             (BIT(22))
#define HRE_RES_ACK_WDT_RST_EN             (BIT(21))
#define SRC_ACK_WDT_RST_EN                 (BIT(20))
#define SW_OFF_SEQ_TRI                     (BIT(4))
#define SW_OFF_SEQ_TRI_SEL                 (BIT(3))
#define ON_SEQ_TRI                         (BIT(2))
#define GHPM_EN                            (BIT(0))
#define MFG_GHPM_RO0_CON                   (g_mfg_rpc_base + 0x09A4)          /* 0x4B8009A4 */
#define TIMEOUT_ERR_RECORD                 (BIT(18))
#define GHPM_PWR_STATE                     (BIT(16))
#define GHPM_STATE                         (GENMASK(7,0))
#define MFG_GHPM_RO1_CON                   (g_mfg_rpc_base + 0x09A8)          /* 0x4B8009A8 */
#define APB_TIMEOUT_RECORD                 (BIT(31))
#define HRE_BAK_ACK_SKIP_RECORD            (BIT(30))
#define HRE_RES_ACK_SKIP_RECORD            (BIT(29))
#define SRC_ACK_TIMEOUT_RECORD             (BIT(28))
#define MAINPLL_ACK_SKIP_RECORD            (BIT(27))
#define STATE_RECORD                       (GENMASK(7,0))

#define CLK_CKFG_6                         (g_clk_cfg_6)
#define CLK_CKFG_6_SET                     (g_clk_cfg_6_set)
#define CLK_CKFG_6_CLR                     (g_clk_cfg_6_clr)
#define PDN_MFG_EB_BIT                     (BIT(23))

/* MT6991 E2_ID_CON CODA meaning: E1->A0, E2->B0 */
#define MFG_MT6991_E1_ID                   (0)
#define MFG_MT6991_E2_ID                   (0x101)

enum mfg_mt6991_e2_con {
	MFG_MT6991_A0,
	MFG_MT6991_B0
};

#endif /* __GHPM_H__ */
