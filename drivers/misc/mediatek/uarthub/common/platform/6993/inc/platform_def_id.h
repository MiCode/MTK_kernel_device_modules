/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef PLATFORM_DEF_ID_H
#define PLATFORM_DEF_ID_H

/*GPIO*/
#define GPIO_BASE_ADDR                            (0x1002D000)
#define GPIO_HUB_MODE_TX                          (0x6C0)
#define GPIO_HUB_MODE_TX_MASK                     (0xF << 8)
#define GPIO_HUB_MODE_TX_SHIFT                    (8)
#define GPIO_HUB_MODE_TX_VALUE                    (0x100)
#define GPIO_HUB_MODE_RX                          (0x6C0)
#define GPIO_HUB_MODE_RX_MASK                     (0xF << 16)
#define GPIO_HUB_MODE_RX_SHIFT                    (16)
#define GPIO_HUB_MODE_RX_VALUE                    (0x10000)

#define GPIO_HUB_MODE_TX_DIR                      (0x70)
#define GPIO_HUB_MODE_TX_DIR_MASK                 (0x1 << 17)
#define GPIO_HUB_MODE_TX_DIR_SHIFT                (17)
#define GPIO_HUB_MODE_RX_DIR                      (0x70)
#define GPIO_HUB_MODE_RX_DIR_MASK                 (0x1 << 18)
#define GPIO_HUB_MODE_RX_DIR_SHIFT                (18)

#define GPIO_HUB_MODE_TX_DATAOUT                  (0x170)
#define GPIO_HUB_MODE_TX_DATAOUT_MASK             (0x1 << 17)
#define GPIO_HUB_MODE_TX_DATAOUT_SHIFT            (17)
#define GPIO_HUB_MODE_RX_DATAOUT                  (0x170)
#define GPIO_HUB_MODE_RX_DATAOUT_MASK             (0x1 << 18)
#define GPIO_HUB_MODE_RX_DATAOUT_SHIFT            (18)

#define GPIO_BT_RST_PIN                           (0x6E0)
#define GPIO_BT_RST_PIN_MASK                      (0xF << 8)
#define GPIO_BT_RST_PIN_SHIFT                     (8)
#define GPIO_BT_RST_PIN_DIR                       (0x70)
#define GPIO_BT_RST_PIN_DIR_MASK                  (0x1 << 25)
#define GPIO_BT_RST_PIN_DIR_SHIFT                 (25)
#define GPIO_BT_RST_PIN_DATAOUT                   (0x170)
#define GPIO_BT_RST_PIN_DATAOUT_MASK              (0x1 << 25)
#define GPIO_BT_RST_PIN_DATAOUT_SHIFT             (25)

/*IOCFG_TM1*/
#define IOCFG_TM1_BASE_ADDR                       (0x13820000)
#define GPIO_BT_RST_PIN_PU                        (0xC0)
#define GPIO_BT_RST_PIN_PU_MASK                   (0x1 << 4)
#define GPIO_BT_RST_PIN_PU_SHIFT                  (4)
#define GPIO_BT_RST_PIN_PD                        (0xB0)
#define GPIO_BT_RST_PIN_PD_MASK                   (0x1 << 4)
#define GPIO_BT_RST_PIN_PD_SHIFT                  (4)

#define GPIO_HUB_MODE_TX_PU                       (0xC0)
#define GPIO_HUB_MODE_TX_PU_MASK                  (0x1 << 6)
#define GPIO_HUB_MODE_TX_PU_SHIFT                 (6)
#define GPIO_HUB_MODE_RX_PU                       (0xC0)
#define GPIO_HUB_MODE_RX_PU_MASK                  (0x1 << 5)
#define GPIO_HUB_MODE_RX_PU_SHIFT                 (5)

#define GPIO_HUB_MODE_TX_PD                       (0xB0)
#define GPIO_HUB_MODE_TX_PD_MASK                  (0x1 << 6)
#define GPIO_HUB_MODE_TX_PD_SHIFT                 (6)
#define GPIO_HUB_MODE_RX_PD                       (0xB0)
#define GPIO_HUB_MODE_RX_PD_MASK                  (0x1 << 5)
#define GPIO_HUB_MODE_RX_PD_SHIFT                 (5)

/*PERI CFG*/
#define PERICFG_AO_BASE_ADDR                      (0x16640000)
#define PERI_CG_2                                 (0x18)
#define PERI_CG_2_UARTHUB_CG_MASK                 (0x7 << 18) // peri uart uarthub
#define PERI_CG_2_UARTHUB_CG_SHIFT                (18)
#define PERI_CG_2_UARTHUB_CLK_CG_MASK             (0x3 << 19)
#define PERI_CG_2_UARTHUB_CLK_CG_SHIFT            (19)
#define PERI_CLOCK_CON                            (0x20)
#define PERI_UART_FBCLK_CKSEL_MASK                (0x1 << 3)  // uart3
#define PERI_UART_FBCLK_CKSEL_SHIFT               (3)
#define PERI_UART_FBCLK_CKSEL_UART_CK             (0x1 << 3)
#define PERI_UART_FBCLK_CKSEL_26M_CK              (0x0 << 3)
#define PERI_UART_WAKEUP                          (0x50)
#define PERI_UART_WAKEUP_UART_GPHUB_SEL_MASK      (0x10)
#define PERI_UART_WAKEUP_UART_GPHUB_SEL_SHIFT     (4)

/*PERI PAR BUS*/
#define PERIPAR_AO_BASE_ADDR                      (0x16680000)
#define PERIPAR_DEBUG_AO_CTRL_0                   (0x0)
#define PERIPAR_AO_DEBUG_CKEN_SHIFT               (3)
#define PERIPAR_AO_DEBUG_CKEN_MASK                (0x1 << PERIPAR_AO_DEBUG_CKEN_SHIFT)
#define PERIPAR_SYS_TIMER_L                       (0x400)  // AO_RESULT0
#define PERIPAR_SYS_TIMER_H                       (0x404)  // AO_RESULT1

/* hwccf */
#define APMIXEDSYS_BASE_ADDR                      (0x1c213100)
#define FENC_STATUS_CON0                          (0x34)   // UNIVPLL_DBG_CON0
#define RG_UNIVPLL_FENC_STATUS_SHIFT              (4)    // UNIVPLL_FENC_STATUS
#define RG_UNIVPLL_FENC_STATUS_MASK               (0x1 << RG_UNIVPLL_FENC_STATUS_SHIFT)


#define TOPCKGEN_BASE_ADDR                        (0x1C210000)
/* uart sel*/
#define CLK_CFG_UART                              (0x100) /*CLK_CFG_15*/
#define CLK_CFG_UART_SET                          (0x104) /*CLK_CFG_15_SET*/
#define CLK_CFG_UART_CLR                          (0x108) /*CLK_CFG_15_CLR*/
#define CLK_CFG_UART_SEL_SHIFT                    (24)
#define CLK_CFG_UART_SEL_26M                      (0x0 << CLK_CFG_UART_SEL_SHIFT)
#define CLK_CFG_UART_SEL_52M                      (0x1 << CLK_CFG_UART_SEL_SHIFT)
#define CLK_CFG_UART_SEL_104M                     (0x2 << CLK_CFG_UART_SEL_SHIFT)
#define CLK_CFG_UART_SEL_208M                     (0x3 << CLK_CFG_UART_SEL_SHIFT)
#define CLK_CFG_UART_SEL_MASK                     (0x3 << CLK_CFG_UART_SEL_SHIFT)
#define CLK_CFG_UART_UPDATE                       (0xC) /*CLK_CFG_UPDATE2*/
#define CLK_CFG_UART_UPDATE_MASK                  (0x1 << 1)
/* uarthub sel */
#define CLK_CFG_UARTHUB                           (0x100) /*CLK_CFG_15*/
#define CLK_CFG_UARTHUB_SET                       (0x104) /*CLK_CFG_15_SET*/
#define CLK_CFG_UARTHUB_CLR                       (0x108) /*CLK_CFG_15_CLR*/
#define CLK_CFG_UARTHUB_SEL_SHIFT                 (16)
#define CLK_CFG_UARTHUB_SEL_26M                   (0x0 << CLK_CFG_UARTHUB_SEL_SHIFT)
#define CLK_CFG_UARTHUB_SEL_104M                  (0x1 << CLK_CFG_UARTHUB_SEL_SHIFT)
#define CLK_CFG_UARTHUB_SEL_208M                  (0x2 << CLK_CFG_UARTHUB_SEL_SHIFT)
#define CLK_CFG_UARTHUB_SEL_MASK                  (0x3 << CLK_CFG_UARTHUB_SEL_SHIFT)
#define CLK_CFG_UARTHUB_UPDATE                    (0xC) /*CLK_CFG_UPDATE2*/
#define CLK_CFG_UARTHUB_UPDATE_MASK               (0x1 << 0)
#define CLK_CFG_PDN_UARTHUB_BCLK_SHIFT            (23) /*pdn_uarthub_bclk*/
#define CLK_CFG_PDN_UARTHUB_BCLK_MASK             (0x1 <<CLK_CFG_PDN_UARTHUB_BCLK_SHIFT)
/* adsp uarthub sel */
#define CLK_CFG_ADSP_UARTHUB                      (0xA0) /*CLK_CFG_9*/
#define CLK_CFG_ADSP_UARTHUB_SET                  (0xA4) /*CLK_CFG_9_SET*/
#define CLK_CFG_ADSP_UARTHUB_CLR                  (0xA8) /*CLK_CFG_9_CLR*/
#define CLK_CFG_ADSP_UARTHUB_SEL_SHIFT            (0)
#define CLK_CFG_ADSP_UARTHUB_SEL_26M              (0x0 << CLK_CFG_ADSP_UARTHUB_SEL_SHIFT)
#define CLK_CFG_ADSP_UARTHUB_SEL_104M             (0x1 << CLK_CFG_ADSP_UARTHUB_SEL_SHIFT)
#define CLK_CFG_ADSP_UARTHUB_SEL_208M             (0x2 << CLK_CFG_ADSP_UARTHUB_SEL_SHIFT)
#define CLK_CFG_ADSP_UARTHUB_SEL_MASK             (0x3 << CLK_CFG_ADSP_UARTHUB_SEL_SHIFT)
#define CLK_CFG_ADSP_UARTHUB_UPDATE               (0x8) /*CLK_CFG_UPDATE1*/
#define CLK_CFG_ADSP_UARTHUB_UPDATE_MASK          (0x1 << 5)

/* SPM */
#define SPM_BASE_ADDR                             (0x1C004000)
#define SPM_SYS_TIMER_L                           (0x504)
#define SPM_SYS_TIMER_H                           (0x508)
/* UART_HUB_INFRA_REQ = SPM_REQ_STA_15 [20]
 * UART_HUB_VRF18_REQ = SPM_REQ_STA_15 [24]
 * UART_HUB_VCORE_REQ = SPM_REQ_STA_15 [23]
 * UART_HUB_SRCCLKENA = SPM_REQ_STA_15 [22]
 * UART_HUB_PMIC_REQ  = SPM_REQ_STA_15 [21] */
#define SPM_REQ_STA_15                            (0x8A4)
#define SPM_REQ_STA_15_UARTHUB_REQ_FIELD          (0x1F)
#define SPM_REQ_STA_15_UARTHUB_REQ_SHIFT          (20)
#define SPM_REQ_STA_15_UARTHUB_REQ_MASK    (SPM_REQ_STA_15_UARTHUB_REQ_FIELD << SPM_REQ_STA_15_UARTHUB_REQ_SHIFT)
/* spm_vcore_internal_ack = SPM_INTERNAL_ACK_STA [0]
 * spm_pmic_internal_ack = SPM_INTERNAL_ACK_STA  [1]
 * spm_srcclkena_internal_ack = SPM_INTERNAL_ACK_STA[2]  //  ~MD26M_CK_OFF
 * spm_vrf18_internal_ack = SPM_INTERNAL_ACK_STA [4]
 * spm_infra_internal_ack = SPM_INTERNAL_ACK_STA [3] */
#define SPM_BASE_ADDR_0x9000                      (SPM_BASE_ADDR + 0x9000) // for near mapping
#define SPM_INTERNAL_ACK_STA                      (0xA4C) // SPM_BASE_ADDR + 0x9A4C
#define SPM_INTERNAL_ACK_STA_UARTHUB_FIELD        (0x1F)
#define SPM_INTERNAL_ACK_STA_UARTHUB_SHIFT        (0)
#define SPM_INTERNAL_ACK_STA_UARTHUB_MASK  (SPM_INTERNAL_ACK_STA_UARTHUB_FIELD << SPM_INTERNAL_ACK_STA_UARTHUB_SHIFT)

/* uart*/
#define UART1_BASE_ADDR                           (0x16010000)
#define UART2_BASE_ADDR                           (0x16020000)
#define UART3_BASE_ADDR                           (0x16030000)
#define AP_DMA_UART_TX_INT_FLAG_ADDR              (0x16550000)

/* connsys sysram*/
#define SYS_SRAM_BASE_ADDR                        (0x00113C00)

#endif /* PLATFORM_DEF_ID_H */
