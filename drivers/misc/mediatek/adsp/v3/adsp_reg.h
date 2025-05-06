/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef __ADSP_REG_H
#define __ADSP_REG_H

/*#define ADSP_BASE in use file */
#define ADSP_SW_INT_SET             (ADSP_BASE + 0x0018)
#define ADSP_A_SW_INT               (1 << 0)
#define ADSP_B_SW_INT               (1 << 1)
#define ADSP_AB_SW_INT              (1 << 2)

#define ADSP_GENERAL_IRQ_CLR        (ADSP_BASE + 0x0038)
#define ADSP_A_2HOST_IRQ_BIT        (1 << 0)
#define ADSP_B_2HOST_IRQ_BIT        (1 << 1)
#define ADSP_A_AFE2HOST_IRQ_BIT     (1 << 2)
#define ADSP_B_AFE2HOST_IRQ_BIT     (1 << 3)
#define ADSP_GENERAL_IRQ_INUSED     \
	(ADSP_A_2HOST_IRQ_BIT | ADSP_B_2HOST_IRQ_BIT \
	| ADSP_A_AFE2HOST_IRQ_BIT | ADSP_B_AFE2HOST_IRQ_BIT)

#define ADSP_A_SPM_WAKEUPSRC        (ADSP_BASE + 0x005C)
#define ADSP_B_SPM_WAKEUPSRC        (ADSP_BASE + 0x0060)
#define ADSP_WAKEUP_SPM             (0x1 << 0)

#define ADSP_SEMAPHORE              (ADSP_BASE + 0x0064)

#define ADSP_B_WDT_REG              (ADSP_BASE + 0x0068)
#define ADSP_A_WDT_REG              (ADSP_BASE + 0x007C)
#define WDT_EN_BIT  (1 << 31)

#define ADSP_CFGREG_RSV_RW_REG0     (ADSP_BASE + 0x008C)
#define ADSP_CFGREG_RSV_RW_REG1     (ADSP_BASE + 0x0090)

#define ADSP_DBG_PEND_CNT           (ADSP_BASE + 0x015C)
#define ADSP_SLEEP_STATUS_REG       (ADSP_BASE + 0x0158)

#define ADSP_A_IS_WFI               (1 << 0)
#define ADSP_B_IS_WFI               (1 << 1)
#define ADSP_AXI_BUS_IS_IDLE        (1 << 2)

#define ADSP_DPSW_REQ                      (ADSP_BASE + 0x0590)
#define ADSP_DPSW_ACK                      (ADSP_BASE + 0x0594)
#define ADSP_DPSW_REQ_MASK                 (0x1 << 0)
#define ADSP_DPSW_ACK_MASK                 (0x1 << 0)
#define DPSW_AD_VLOGIC_ON_ACK_STATUS       (ADSP_BASE_CFG2 + 0x0050)
#define DPSW_AD_SRAM_ON_ACK_STATUS         (ADSP_BASE_CFG2 + 0x0054)
#endif
