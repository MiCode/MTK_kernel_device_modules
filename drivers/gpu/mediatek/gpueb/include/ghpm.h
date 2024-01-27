/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __GHPM_H__
#define __GHPM_H__

#include <linux/platform_device.h>

#define GHPM_TEST                          (0)       /* use proc node to test ghpm on/off */

#ifndef GENMASK
#define GENMASK(h, l)   (((1U << ((h) - (l) + 1)) - 1) << (l))
#endif

#ifndef BIT
#define BIT(n)                             (1U << (n))
#endif

#define US_TO_NS(x)                        ((x)*1000)

#define GHPM_IPI_TIMEOUT                   (5000)
#define GPUEB_WAIT_TIMEOUT                 (5000)

#define GPUEB_SRAM_GPR10                   (g_gpueb_gpr_base + 0x28)

#define MFG_RPC_BASE                       (0x4B800000)
#define MFG_RPC_SLV_CTRL_UPDATE            (g_mfg_rpc_base + 0x0068)          /* 0x4B800068 */
#define MFG_RPC_SLV_SLP_PROT_RDY_STA       (g_mfg_rpc_base + 0x0078)          /* 0x4B800078 */
#define MFG_RPC_MFG0_PWR_CON               (g_mfg_rpc_base + 0x0504)          /* 0x4B800504 */
#define MFG0_PWR_ACK_2ND_BIT               (BIT(31))
#define MFG0_PWR_ACK_1ST_BIT               (BIT(30))
#define MFG0_PWR_ACK_BIT                   (MFG0_PWR_ACK_1ST_BIT | MFG0_PWR_ACK_2ND_BIT)
#define MFG_RPC_MFG1_PWR_CON               (g_mfg_rpc_base + 0x0500)          /* 0x4B800500 */
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

enum mfg0_pwr_sta {
	MFG0_PWR_OFF,
	MFGO_PWR_ON
};

enum ghpm_return {
	GHPM_SUCCESS           = 0,
	GHPM_ERR               = -1,
	GHPM_STATE_ERR         = -2,
	GHPM_PWR_STATE_ERR     = -3,
	GHPM_DUPLICATE_ON_ERR  = -4,
	GHPM_PWRCNT_ERR        = -5,
	GHPM_OFF_EBB_IPI_ERR   = -6
};

enum ghpm_state {
	GHPM_OFF = 0,
	GHPM_ON  = 1
};

enum gpueb_low_power_event {
	SUSPEND_POWER_OFF,  /* Suspend */
	SUSPEND_POWER_ON    /* Resume */
};

enum gpueb_low_power_state {
	GPUEB_ON_SUSPEND,
	GPUEB_ON_RESUME,
};

enum mfg0_off_state {
	MFG1_OFF = 0,  /* legacy on/off */
	MFG1_ON  = 1   /* vcore off allow state*/
};

enum wait_gpueb_ret {
	WAIT_DONE         = 0,
	WAIT_TIMEOUT      = -1,
	WAIT_INPUT_ERROR  = -2
};

struct gpueb_slp_ipi_data {
	enum gpueb_low_power_event event;
	enum mfg0_off_state off_state;
	int reserve;
};

int ghpm_init(struct platform_device *pdev);

enum mfg0_pwr_sta mfg0_pwr_sta(void);
int ghpm_ctrl(enum ghpm_state power, enum mfg0_off_state off_state);
int wait_gpueb(enum gpueb_low_power_event event, int timeout);
void dump_ghpm_info(void);

#endif /* __GHPM_H__ */
