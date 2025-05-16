// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/kernel.h>
#include <linux/regmap.h>
#include <linux/sched/clock.h>

#include <soc/mediatek/emi.h>

#include "clk-mt6993-fmeter.h"
#include "clk-fmeter.h"
#include "uarthub_drv_core.h"
#include "uarthub_drv_export.h"
#include "common_def_id.h"
#include "inc/mt6993.h"
#include "inc/mt6993_debug.h"
#include "inc/mt6993_debug_keyword.h"
#if (UARTHUB_SUPPORT_FPGA) || (UARTHUB_SUPPORT_DVT)
#include "test/mt6993_test_api.h"
#include "test/ut/ut_tc.h"
#include "test/it/it_tc.h"
#endif

#if !(UARTHUB_SUPPORT_FPGA)
static int uarthub_get_peri_uart_pad_mode_mt6993(void);
static int uarthub_get_gpio_trx_info_mt6993(struct uarthub_gpio_trx_info *info);
#endif

static int uarthub_dump_debug_monitor_packet_info_mode_mt6993(const char *tag);
static int uarthub_dump_debug_monitor_check_data_mode_mt6993(const char *tag);
static int uarthub_dump_debug_monitor_crc_result_mode_mt6993(const char *tag);

struct uarthub_debug_ops_struct mt6993_plat_debug_data = {
	.uarthub_plat_get_intfhub_base_addr = uarthub_get_intfhub_base_addr_mt6993,
	.uarthub_plat_get_uartip_base_addr = uarthub_get_uartip_base_addr_mt6993,
	.uarthub_plat_dump_uartip_debug_info = uarthub_dump_uartip_debug_info_mt6993,
	.uarthub_plat_dump_intfhub_debug_info = uarthub_dump_intfhub_debug_info_mt6993,
#if UARTHUB_WAKEUP_DEBUG_EN
	.uarthub_plat_dump_sspm_wakeup_debug_info = uarthub_dump_sspm_wakeup_debug_info_mt6993,
#endif
	.uarthub_plat_dump_extend_debug_info = uarthub_dump_extend_debug_info_mt6993,
	.uarthub_plat_dump_debug_monitor = uarthub_dump_debug_monitor_mt6993,
	.uarthub_plat_debug_monitor_ctrl = uarthub_debug_monitor_ctrl_mt6993,
	.uarthub_plat_debug_monitor_stop = uarthub_debug_monitor_stop_mt6993,
	.uarthub_plat_debug_monitor_clr = uarthub_debug_monitor_clr_mt6993,
	.uarthub_plat_dump_inband_irq_debug = uarthub_dump_inband_irq_debug_mt6993,
	.uarthub_plat_dump_debug_tx_rx_count = uarthub_dump_debug_tx_rx_count_mt6993,
	.uarthub_plat_dump_debug_clk_info = uarthub_dump_debug_clk_info_mt6993,
	.uarthub_plat_dump_debug_byte_cnt_info = uarthub_dump_debug_byte_cnt_info_mt6993,
	.uarthub_plat_dump_debug_apdma_uart_info = uarthub_dump_debug_apdma_uart_info_mt6993,
	.uarthub_plat_dump_debug_bus_status_info = uarthub_dump_debug_bus_status_info_mt6993,
	.uarthub_plat_dump_sspm_log = uarthub_dump_sspm_log_mt6993,
	.uarthub_plat_trigger_fpga_testing = uarthub_trigger_fpga_testing_mt6993,
	.uarthub_plat_trigger_dvt_ut_testing = uarthub_trigger_dvt_ut_testing_mt6993,
	.uarthub_plat_trigger_dvt_it_testing = uarthub_trigger_dvt_it_testing_mt6993,
	.uarthub_plat_emiisu_record_off = uarthub_emiisu_record_off_mt6993,
};

uint64_t uarthub_get_xoff_ts_mt6993(unsigned int dev_index, unsigned int is_recv, unsigned int is_xoff)
{
	uint32_t sel, uart_sel;
	uint64_t ts;

	/* UART 1 is not supported */
	if (dev_index == 1)
		return 0;

	/* CR sel has UART 0/2/cmm , skips UART 1*/
	uart_sel = (dev_index > 1)? (dev_index - 1) : (dev_index);

	sel = (uart_sel << 2) + (is_recv << 1) + is_xoff;
	UART_XON_XOFF_TIME_LOG_CTRL_SET_uart_xon_xoff_time_sel(UART_XON_XOFF_TIME_LOG_CTRL_ADDR, sel);

	ts = UART_XON_XOFF_TIME_LOG_STA_2_GET_uart_xon_xoff_time_info_63_32(UART_XON_XOFF_TIME_LOG_STA_2_ADDR);
	ts  = (ts << 32) | UART_XON_XOFF_TIME_LOG_STA_1_GET_uart_xon_xoff_time_info_31_0(UART_XON_XOFF_TIME_LOG_STA_1_ADDR);
	return ts;
}

uint32_t uarthub_get_debug_fifo_data_mt6993(unsigned int dev_index, unsigned int offset, unsigned int is_rx)
{
	if (dev_index == 0) { // UART0
		UART0_FIFO_DUMP_CTRL_SET_uart_0_debug_raddr_set(UART0_FIFO_DUMP_CTRL_ADDR, offset);
		return UART0_FIFO_DUMP_CTRL_GET_uart_0_debug_fifo_data(UART0_FIFO_DUMP_CTRL_ADDR);
	} else if (dev_index == 1) { //UART1. Not supported.
		return 0;
	} else if (dev_index == 2) { // UART2
		UART2_FIFO_DUMP_CTRL_SET_uart_2_debug_raddr_set(UART2_FIFO_DUMP_CTRL_ADDR, offset);
		return UART2_FIFO_DUMP_CTRL_GET_uart_2_debug_fifo_data(UART2_FIFO_DUMP_CTRL_ADDR);
	} else if (dev_index == 3) { // UART CMM
		UART_CMM_FIFO_DUMP_CRTL_SET_uart_cmm_debug_raddr_set(UART_CMM_FIFO_DUMP_CRTL_ADDR, offset);
		return UART_CMM_FIFO_DUMP_CRTL_GET_uart_cmm_debug_fifo_data(UART_CMM_FIFO_DUMP_CRTL_ADDR);
	} else
		return 0;
}

uint32_t uarthub_get_debug_fifo_cur_mt6993(unsigned int dev_index, unsigned int is_rx)
{
	if (dev_index == 0 ) { // UART 0
		UART0_FIFO_DUMP_CTRL_SET_uart_0_debug_dir(UART0_FIFO_DUMP_CTRL_ADDR, is_rx);
		if (is_rx)
			return UART0_FIFO_DUMP_CTRL_GET_uart_0_rxfifo_waddr(UART0_FIFO_DUMP_CTRL_ADDR);
		else
			return UART0_FIFO_DUMP_CTRL_GET_uart_0_txfifo_waddr(UART0_FIFO_DUMP_CTRL_ADDR);
	} else if (dev_index == 1) { // UART 1. Not supported.
		return 0;
	} else if (dev_index == 2 ) { // UART 2
		UART2_FIFO_DUMP_CTRL_SET_uart_2_debug_dir(UART2_FIFO_DUMP_CTRL_ADDR, is_rx);
		if (is_rx)
			return UART2_FIFO_DUMP_CTRL_GET_uart_2_rxfifo_waddr(UART2_FIFO_DUMP_CTRL_ADDR);
		else
			return UART2_FIFO_DUMP_CTRL_GET_uart_2_txfifo_waddr(UART2_FIFO_DUMP_CTRL_ADDR);
	} else if (dev_index == 3) { // UART CMM
		UART_CMM_FIFO_DUMP_CRTL_SET_uart_cmm_debug_dir(UART_CMM_FIFO_DUMP_CRTL_ADDR, is_rx);
		if (is_rx)
			return UART_CMM_FIFO_DUMP_CRTL_GET_uart_cmm_rxfifo_waddr(UART_CMM_FIFO_DUMP_CRTL_ADDR);
		else
			return UART_CMM_FIFO_DUMP_CRTL_GET_uart_cmm_txfifo_waddr(UART_CMM_FIFO_DUMP_CRTL_ADDR);
	} else
		return 0;
}

int uarthub_get_adsp_uart_mux_info_mt6993(void)
{
	if (!topckgen_base_remap_addr_mt6993) {
		pr_notice("[%s] topckgen_base_remap_addr_mt6993 is NULL\n", __func__);
		return -1;
	}

	return (UARTHUB_REG_READ_BIT(topckgen_base_remap_addr_mt6993 + CLK_CFG_ADSP_UARTHUB,
		CLK_CFG_ADSP_UARTHUB_SEL_MASK) >> CLK_CFG_ADSP_UARTHUB_SEL_SHIFT);
}

int uarthub_get_uart_mux_info_mt6993(void)
{
	if (!topckgen_base_remap_addr_mt6993) {
		pr_notice("[%s] topckgen_base_remap_addr_mt6993 is NULL\n", __func__);
		return -1;
	}

	return (UARTHUB_REG_READ_BIT(topckgen_base_remap_addr_mt6993 + CLK_CFG_UART,
		CLK_CFG_UART_SEL_MASK) >> CLK_CFG_UART_SEL_SHIFT);
}

int uarthub_get_uarthub_mux_info_mt6993(void)
{
	if (!topckgen_base_remap_addr_mt6993) {
		pr_notice("[%s] topckgen_base_remap_addr_mt6993 is NULL\n", __func__);
		return -1;
	}

	return (UARTHUB_REG_READ_BIT(topckgen_base_remap_addr_mt6993 + CLK_CFG_UARTHUB,
		CLK_CFG_UARTHUB_SEL_MASK) >> CLK_CFG_UARTHUB_SEL_SHIFT);
}

#if !(UARTHUB_SUPPORT_FPGA)
int uarthub_get_uarthub_cg_info_mt6993(int *p_topckgen_cg, int *p_peri_cg)
{
	int topckgen_cg = 0, peri_cg = 0;

	if (!pericfg_ao_remap_addr_mt6993) {
		pr_notice("[%s] pericfg_ao_remap_addr_mt6993 is NULL\n", __func__);
		return -1;
	}

	if (!topckgen_base_remap_addr_mt6993) {
		pr_notice("[%s] topckgen_base_remap_addr_mt6993 is NULL\n", __func__);
		return -1;
	}

	topckgen_cg = (UARTHUB_REG_READ_BIT(topckgen_base_remap_addr_mt6993 + CLK_CFG_UARTHUB,
		CLK_CFG_PDN_UARTHUB_BCLK_MASK) >> CLK_CFG_PDN_UARTHUB_BCLK_SHIFT);

	peri_cg = (UARTHUB_REG_READ_BIT(pericfg_ao_remap_addr_mt6993 + PERI_CG_2,
		PERI_CG_2_UARTHUB_CG_MASK) >> PERI_CG_2_UARTHUB_CG_SHIFT);

	if (p_topckgen_cg)
		*p_topckgen_cg = topckgen_cg;

	if (p_peri_cg)
		*p_peri_cg = peri_cg;

	return 0;
}
#endif

#if !(UARTHUB_SUPPORT_FPGA)
int uarthub_get_peri_uart_pad_mode_mt6993(void)
{
	if (!pericfg_ao_remap_addr_mt6993) {
		pr_notice("[%s] pericfg_ao_remap_addr_mt6993 is NULL\n", __func__);
		return -1;
	}

	/* 1: UART_PAD mode */
	/* 0: UARTHUB mode */
	return (UARTHUB_REG_READ_BIT(pericfg_ao_remap_addr_mt6993 + PERI_UART_WAKEUP,
		PERI_UART_WAKEUP_UART_GPHUB_SEL_MASK) >> PERI_UART_WAKEUP_UART_GPHUB_SEL_SHIFT);
}
#endif

#if !(UARTHUB_SUPPORT_FPGA)
int uarthub_get_uart_src_clk_info_mt6993(void)
{
	if (!pericfg_ao_remap_addr_mt6993) {
		pr_notice("[%s] pericfg_ao_remap_addr_mt6993 is NULL\n", __func__);
		return -1;
	}

	return (UARTHUB_REG_READ_BIT(pericfg_ao_remap_addr_mt6993 + PERI_CLOCK_CON,
		PERI_UART_FBCLK_CKSEL_MASK) >> PERI_UART_FBCLK_CKSEL_SHIFT);
}
#endif

#if !(UARTHUB_SUPPORT_FPGA)
int uarthub_get_spm_res_info_mt6993(
	int *p_spm_res_uarthub, int *p_spm_res_internal)
{
	int spm_res_uarthub_1 = 0;
	int spm_res_internal = 0;

	if (!spm_remap_addr_mt6993 || !spm_remap_addr_0x9000_mt6993) {
		pr_notice("[%s] spm_remap_addr_mt6993 is NULL\n", __func__);
		return -1;
	}


	/* UART_HUB_INFRA_REQ = SPM_REQ_STA_15 [20]
	 * UART_HUB_VRF18_REQ = SPM_REQ_STA_15 [24]
	 * UART_HUB_VCORE_REQ = SPM_REQ_STA_15 [23]
	 * UART_HUB_SRCCLKENA = SPM_REQ_STA_15 [22]
	 * UART_HUB_PMIC_REQ = SPM_REQ_STA_15 [21] */
	spm_res_uarthub_1 = (UARTHUB_REG_READ_BIT(
		spm_remap_addr_mt6993 + SPM_REQ_STA_15,
		SPM_REQ_STA_15_UARTHUB_REQ_MASK) >>
		SPM_REQ_STA_15_UARTHUB_REQ_SHIFT);

	if (p_spm_res_uarthub)
		*p_spm_res_uarthub = (spm_res_uarthub_1);

#if SPM_RES_CHK_EN
	if (spm_res_uarthub_1 != SPM_REQ_STA_15_UARTHUB_REQ_FIELD)
		return 0;
#endif

	spm_res_internal = UARTHUB_REG_READ(spm_remap_addr_0x9000_mt6993 + SPM_INTERNAL_ACK_STA);

	/* spm_pmic_internal_ack = SPM_INTERNAL_ACK_STA [1]
	* spm_srcclkena_internal_ack = SPM_INTERNAL_ACK_STA[2] // ~MD26M_CK_OFF
	* spm_vcore_internal_ack = SPM_INTERNAL_ACK_STA [0]
	* spm_vrf18_internal_ack = SPM_INTERNAL_ACK_STA [4]
	* spm_infra_internal_ack = SPM_INTERNAL_ACK_STA [3] */
	spm_res_internal = ((spm_res_internal & SPM_INTERNAL_ACK_STA_UARTHUB_MASK) >>
		SPM_INTERNAL_ACK_STA_UARTHUB_SHIFT);

	if (p_spm_res_internal)
		*p_spm_res_internal = spm_res_internal;

#if SPM_RES_CHK_EN
	if (spm_res_internal != SPM_INTERNAL_ACK_STA_UARTHUB_FIELD)
		return 0;
#endif

	return 1;
}
#endif

#if !(UARTHUB_SUPPORT_FPGA)
int uarthub_get_gpio_trx_info_mt6993(struct uarthub_gpio_trx_info *info)
{
	if (!info) {
		pr_notice("[%s] info is NULL\n", __func__);
		return -1;
	}

	if (!gpio_base_remap_addr_mt6993) {
		pr_notice("[%s] gpio_base_remap_addr_mt6993 is NULL\n", __func__);
		return -1;
	}

	if (!iocfg_tm1_base_remap_addr_mt6993) {
		pr_notice("[%s] iocfg_tm1_base_remap_addr_mt6993 is NULL\n", __func__);
		return -1;
	}

	UARTHUB_READ_GPIO_BIT(info->tx_mode, GPIO_BASE_ADDR, gpio_base_remap_addr_mt6993,
		GPIO_HUB_MODE_TX, GPIO_HUB_MODE_TX_MASK, GPIO_HUB_MODE_TX_SHIFT);
	info->tx_mode.value = (GPIO_HUB_MODE_TX_VALUE >> GPIO_HUB_MODE_TX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->tx_dir, GPIO_BASE_ADDR, gpio_base_remap_addr_mt6993,
		GPIO_HUB_MODE_TX_DIR, GPIO_HUB_MODE_TX_DIR_MASK, GPIO_HUB_MODE_TX_DIR_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->tx_dataout, GPIO_BASE_ADDR, gpio_base_remap_addr_mt6993,
		GPIO_HUB_MODE_TX_DATAOUT, GPIO_HUB_MODE_TX_DATAOUT_MASK, GPIO_HUB_MODE_TX_DATAOUT_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->tx_pu, IOCFG_TM1_BASE_ADDR, iocfg_tm1_base_remap_addr_mt6993,
		GPIO_HUB_MODE_TX_PU, GPIO_HUB_MODE_TX_PU_MASK, GPIO_HUB_MODE_TX_PU_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->tx_pd, IOCFG_TM1_BASE_ADDR, iocfg_tm1_base_remap_addr_mt6993,
		GPIO_HUB_MODE_TX_PD, GPIO_HUB_MODE_TX_PD_MASK, GPIO_HUB_MODE_TX_PD_SHIFT);

	UARTHUB_READ_GPIO_BIT(info->rx_mode, GPIO_BASE_ADDR, gpio_base_remap_addr_mt6993,
		GPIO_HUB_MODE_RX, GPIO_HUB_MODE_RX_MASK, GPIO_HUB_MODE_RX_SHIFT);
	info->rx_mode.value = (GPIO_HUB_MODE_RX_VALUE >> GPIO_HUB_MODE_RX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->rx_dir, GPIO_BASE_ADDR, gpio_base_remap_addr_mt6993,
		GPIO_HUB_MODE_RX_DIR, GPIO_HUB_MODE_RX_DIR_MASK, GPIO_HUB_MODE_RX_DIR_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->rx_dataout, GPIO_BASE_ADDR, gpio_base_remap_addr_mt6993,
		GPIO_HUB_MODE_RX_DATAOUT, GPIO_HUB_MODE_RX_DATAOUT_MASK, GPIO_HUB_MODE_RX_DATAOUT_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->rx_pu, IOCFG_TM1_BASE_ADDR, iocfg_tm1_base_remap_addr_mt6993,
		GPIO_HUB_MODE_RX_PU, GPIO_HUB_MODE_RX_PU_MASK, GPIO_HUB_MODE_RX_PU_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->rx_pd, IOCFG_TM1_BASE_ADDR, iocfg_tm1_base_remap_addr_mt6993,
		GPIO_HUB_MODE_RX_PD, GPIO_HUB_MODE_RX_PD_MASK, GPIO_HUB_MODE_RX_PD_SHIFT);

	UARTHUB_READ_GPIO_BIT(info->bt_rst_mode, GPIO_BASE_ADDR, gpio_base_remap_addr_mt6993,
		GPIO_BT_RST_PIN, GPIO_BT_RST_PIN_MASK, GPIO_BT_RST_PIN_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->bt_rst_dir, GPIO_BASE_ADDR, gpio_base_remap_addr_mt6993,
		GPIO_BT_RST_PIN_DIR, GPIO_BT_RST_PIN_DIR_MASK, GPIO_BT_RST_PIN_DIR_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->bt_rst_dataout, GPIO_BASE_ADDR, gpio_base_remap_addr_mt6993,
		GPIO_BT_RST_PIN_DATAOUT, GPIO_BT_RST_PIN_DATAOUT_MASK, GPIO_BT_RST_PIN_DATAOUT_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->bt_rst_pu, IOCFG_TM1_BASE_ADDR, iocfg_tm1_base_remap_addr_mt6993,
		GPIO_BT_RST_PIN_PU, GPIO_BT_RST_PIN_PU_MASK, GPIO_BT_RST_PIN_PU_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->bt_rst_pd, IOCFG_TM1_BASE_ADDR, iocfg_tm1_base_remap_addr_mt6993,
		GPIO_BT_RST_PIN_PD, GPIO_BT_RST_PIN_PD_MASK, GPIO_BT_RST_PIN_PD_SHIFT);

	return 0;
}
#endif

int uarthub_get_intfhub_base_addr_mt6993(void)
{
	return UARTHUB_INTFHUB_BASE_ADDR;
}

int uarthub_get_uartip_base_addr_mt6993(int dev_index)
{
	if (dev_index < 0 || dev_index > UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0)
		return UARTHUB_DEV_0_BASE_ADDR;
	else if (dev_index == 1)
		return UARTHUB_DEV_1_BASE_ADDR;
	else if (dev_index == 2)
		return UARTHUB_DEV_2_BASE_ADDR;

	return UARTHUB_CMM_BASE_ADDR;
}

int uarthub_dump_uartip_debug_info_mt6993(
	const char *tag, struct mutex *uartip_lock)
{
	const char *def_tag = "HUB_DBG_UIP";
	int print_ap = 0;
	char dmp_info_buf[DBG_LOG_LEN] = {'\0'};
	int lst_det_xoff[5] = { 0 };
	int lst_wsend_xoff[5] = { 0 };
	int lst_frame_error[5] = { 0 };
	int lst_rx_woffset[5] = { 0 };
	int lst_tx_woffset[5] = { 0 };

	if (!uartip_lock)
		pr_notice("[%s] uartip_lock is NULL\n", __func__);

	if (uartip_lock) {
		if (mutex_lock_killable(uartip_lock)) {
			pr_notice("[%s] mutex_lock_killable(uartip_lock) fail\n", __func__);
			return UARTHUB_ERR_MUTEX_LOCK_FAIL;
		}
	}

	if (apuart_base_map_mt6993[3] != NULL)
		print_ap = 1;

	UARTHUB_DEBUG_PRINT_OP_RX_REQ(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_IP_TX_DMA(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_RX_WOFFSET(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_TX_WOFFSET(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_TX_ROFFSET(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_ROFFSET_DMA(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_XCSTATE_WSEND_XOFF(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_SWTXDIS_DET_XOFF(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_FEATURE_SEL(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_HIGHSPEEND(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_DLL(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_SAMPLE_CNT(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_SAMPLE_PT(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_FRACDIV_L(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_FRACDIV_M(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_DMA_EN(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_IIR(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_LCR(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_EFR(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_XON1(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_XOFF1(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_XON2(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_XOFF2(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_ESCAPE_EN(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_ESCAPE_DAT(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_RXTRI_AD(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_FCR_RD(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_MCR(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_TX_OFFSET_DMA(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_LSR(def_tag, tag, print_ap, 1);

	UARTHUB_DEBUG_PRINT_RX_WOFFSET_DEBUG_KEYWORD(lst_rx_woffset[0], lst_rx_woffset[1],
		lst_rx_woffset[2], lst_rx_woffset[3], lst_rx_woffset[4], 0, 0);
	UARTHUB_DEBUG_PRINT_TX_WOFFSET_DEBUG_KEYWORD(lst_tx_woffset[0], lst_tx_woffset[1],
		lst_tx_woffset[2], lst_tx_woffset[3], lst_tx_woffset[4], 0, 0);
	UARTHUB_DEBUG_PRINT_FRAME_ERROR_DEBUG_KEYWORD(lst_frame_error[0], lst_frame_error[1],
		lst_frame_error[2], lst_frame_error[3], lst_frame_error[4], 1);
	UARTHUB_DEBUG_PRINT_DET_XOFF_DEBUG_KEYWORD(lst_det_xoff[0], lst_det_xoff[1],
		lst_det_xoff[2], lst_det_xoff[3], lst_det_xoff[4], 1);
	UARTHUB_DEBUG_PRINT_WSEND_XOFF_DEBUG_KEYWORD(lst_wsend_xoff[0], lst_wsend_xoff[1],
		lst_wsend_xoff[2], lst_wsend_xoff[3], lst_wsend_xoff[4], 1);

	if (uartip_lock)
		mutex_unlock(uartip_lock);

	return 0;
}

int uarthub_dump_intfhub_debug_info_mt6993(const char *tag)
{
	int val = 0, val1 = 0, val2 = 0;
	unsigned int sta_irq = 0;
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int len = 0;
	int ret = 0;
	const char *def_tag = "HUB_DBG";
	int dev0_sta = 0, dev1_sta = 0, dev2_sta = 0;

#if !(UARTHUB_SUPPORT_FPGA)
	int topckgen_cg = 0, peri_cg = 0;
	int spm_res_uarthub = 0, spm_res_internal = 0;
	struct uarthub_gpio_trx_info gpio_base_addr;

	val = DBG_CTRL_GET_intfhub_dbg_sel(DBG_CTRL_ADDR);
	len = 0;
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s][%s][%s] IDBG=[0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag), MT6993_UARTHUB_DUMP_VERSION, val);
	if (ret > 0)
		len += ret;

	val = uarthub_is_apb_bus_clk_enable_mt6993();
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",APB_BUS=[%d]", val);
	if (ret > 0)
		len += ret;

	if (val == 0) {
		pr_info("%s\n", dmp_info_buf);
		return 0;
	}

	val = uarthub_get_uarthub_cg_info_mt6993(&topckgen_cg, &peri_cg);
	if (val >= 0) {
		/* the expect value is 0x0 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",HUB_CG=[topck:0x%x,peri:0x%x]", topckgen_cg, peri_cg);
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_uart_src_clk_info_mt6993();
	if (val >= 0) {
		/* the expect value is 0x1 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UART_SRC_CLK=[0x%x(%s)]", val, ((val == 0) ? "26M" : "TOPCK"));
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_peri_uart_pad_mode_mt6993();
	if (val >= 0) {
		/* the expect value is 0x0 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UART_MODE=[0x%x(%s)]", val, ((val == 0) ? "HUB" : "UART_PAD"));
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_spm_res_info_mt6993(&spm_res_uarthub, &spm_res_internal);
	if (val == 1) {
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",SPM=[1]");
		if (ret > 0)
			len += ret;
	} else if (val == 0 || val == 2) {
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",SPM=[%d(0x%x/0x%x,exp:0x%x/0x%x)]",
			val, spm_res_uarthub, spm_res_internal,
			SPM_REQ_STA_15_UARTHUB_REQ_FIELD, SPM_INTERNAL_ACK_STA_UARTHUB_FIELD);
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_hwccf_univpll_on_info_mt6993();
	if (val >= 0) {
		/* the expect value is 0x1 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UNIVPLL=[%d]", val);
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_uarthub_mux_info_mt6993();
	val1 = uarthub_get_uart_mux_info_mt6993();
	val2 = uarthub_get_adsp_uart_mux_info_mt6993();
	if (val >= 0) {
		/* the expect value is 0x2 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",HUB/UART/ADSP_MUX=[%d(%s)-%d(%s)-%d(%s)]", val,
			((val == 0) ? "26M" : ((val == 1) ? "104M" : "208M")), val1,
			((val1 == 0) ? "26M" : ((val1 == 1) ? "52M" :
				((val1 == 2) ? "104M" : "208M"))), val2,
			((val2 == 0) ? "26M" : ((val2 == 1) ? "104M" : "208M")));
		if (ret > 0)
			len += ret;
	}

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len, ",HUB/UART/ADSP_CLK=[%d-%d-%d]",
		mt_get_fmeter_freq(FM_UARTHUB_B_CK, CKGEN),
		mt_get_fmeter_freq(FM_UART_CK, CKGEN),
		mt_get_fmeter_freq(F_FADSP_UARTHUB_BCLK_CK, CKGEN));
	if (ret > 0)
		len += ret;

	val = DBG_CTRL_GET_intfhub_dbg_sel(DBG_CTRL_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IDBG=[0x%x]", val);
	if (ret > 0)
		len += ret;

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	val = uarthub_get_gpio_trx_info_mt6993(&gpio_base_addr);
	if (val == 0) {
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] GPIO[mode-dir-out-pu-pd]=[R:%d-%s-%s-%d-%d,T:%d-%s-%s-%d-%d,BT_RST:%d-%s-%s-%d-%d]",
			def_tag, ((tag == NULL) ? "null" : tag),
			gpio_base_addr.rx_mode.gpio_value,
			((gpio_base_addr.rx_dir.gpio_value == 0) ? "IN" : "OUT"),
			((gpio_base_addr.rx_dataout.gpio_value == 0) ? "LOW" : "HIGH"),
			gpio_base_addr.rx_pu.gpio_value,
			gpio_base_addr.rx_pd.gpio_value,
			gpio_base_addr.tx_mode.gpio_value,
			((gpio_base_addr.tx_dir.gpio_value == 0) ? "IN" : "OUT"),
			((gpio_base_addr.tx_dataout.gpio_value == 0) ? "LOW" : "HIGH"),
			gpio_base_addr.tx_pu.gpio_value,
			gpio_base_addr.tx_pd.gpio_value,
			gpio_base_addr.bt_rst_mode.gpio_value,
			((gpio_base_addr.bt_rst_dir.gpio_value == 0) ? "IN" : "OUT"),
			((gpio_base_addr.bt_rst_dataout.gpio_value == 0) ? "LOW" : "HIGH"),
			gpio_base_addr.bt_rst_pu.gpio_value,
			gpio_base_addr.bt_rst_pd.gpio_value);
		if (ret > 0)
			len += ret;
	}

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ILPBACK(0xe4)=[0x%x]", UARTHUB_REG_READ(LOOPBACK_ADDR));
	if (ret > 0)
		len += ret;

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IDBG(0xf4)=[0x%x]", UARTHUB_REG_READ(DBG_CTRL_ADDR));
	if (ret > 0)
		len += ret;

	pr_info("%s\n", dmp_info_buf);

	UARTHUB_DEBUG_PRINT_GPIO_DEBUG_KEYWORD(
		gpio_base_addr.rx_mode, gpio_base_addr.tx_mode, gpio_base_addr.bt_rst_mode,
		gpio_base_addr.bt_rst_dir, gpio_base_addr.bt_rst_dataout, 1);

	len = 0;
	dev0_sta = UARTHUB_REG_READ(DEV0_STA_ADDR);
	dev1_sta = UARTHUB_REG_READ(DEV1_STA_ADDR);
	dev2_sta = UARTHUB_REG_READ(DEV2_STA_ADDR);
	ret = snprintf(dmp_info_buf, DBG_LOG_LEN,
		"[%s][%s] IDEVx_STA(0x0/0x40/0x80)=[0x%x-0x%x-0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag),
		dev0_sta, dev1_sta, dev2_sta);
	if (ret > 0)
		len += ret;
#else
	len = 0;
	dev0_sta = UARTHUB_REG_READ(DEV0_STA_ADDR);
	dev1_sta = UARTHUB_REG_READ(DEV1_STA_ADDR);
	dev2_sta = UARTHUB_REG_READ(DEV2_STA_ADDR);
	ret = snprintf(dmp_info_buf, DBG_LOG_LEN,
		"[%s][%s][%s] IDEVx_STA(0x0/0x40/0x80)=[0x%x-0x%x-0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag), MT6993_UARTHUB_DUMP_VERSION,
		dev0_sta, dev1_sta, dev2_sta);
	if (ret > 0)
		len += ret;
#endif

	dev0_sta = UARTHUB_REG_READ(DEV0_PKT_CNT_ADDR);
	dev1_sta = UARTHUB_REG_READ(DEV1_PKT_CNT_ADDR);
	dev2_sta = UARTHUB_REG_READ(DEV2_PKT_CNT_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IDEVx_PKT_CNT(0x1c/0x50/0x90)=[0x%x-0x%x-0x%x]",
		dev0_sta, dev1_sta, dev2_sta);
	if (ret > 0)
		len += ret;

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",PKT_CNT=[R:%d-%d-%d,T:%d-%d-%d]",
		((dev0_sta & REG_FLD_MASK(DEV0_PKT_CNT_FLD_dev0_rx_pkt_cnt)) >>
			REG_FLD_SHIFT(DEV0_PKT_CNT_FLD_dev0_rx_pkt_cnt)),
		((dev1_sta & REG_FLD_MASK(DEV1_PKT_CNT_FLD_dev1_rx_pkt_cnt)) >>
			REG_FLD_SHIFT(DEV1_PKT_CNT_FLD_dev1_rx_pkt_cnt)),
		((dev2_sta & REG_FLD_MASK(DEV2_PKT_CNT_FLD_dev2_rx_pkt_cnt)) >>
			REG_FLD_SHIFT(DEV2_PKT_CNT_FLD_dev2_rx_pkt_cnt)),
		((dev0_sta & REG_FLD_MASK(DEV0_PKT_CNT_FLD_dev0_tx_pkt_cnt)) >>
			REG_FLD_SHIFT(DEV0_PKT_CNT_FLD_dev0_tx_pkt_cnt)),
		((dev1_sta & REG_FLD_MASK(DEV1_PKT_CNT_FLD_dev1_tx_pkt_cnt)) >>
			REG_FLD_SHIFT(DEV1_PKT_CNT_FLD_dev1_tx_pkt_cnt)),
		((dev2_sta & REG_FLD_MASK(DEV2_PKT_CNT_FLD_dev2_tx_pkt_cnt)) >>
			REG_FLD_SHIFT(DEV2_PKT_CNT_FLD_dev2_tx_pkt_cnt)));
	if (ret > 0)
		len += ret;

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",bt_on_count=[%d]", uarthub_get_bt_on_count_mt6993());
	if (ret > 0)
		len += ret;

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	dev0_sta = UARTHUB_REG_READ(DBG_STATE_ADDR);
	ret = snprintf(dmp_info_buf, DBG_LOG_LEN,
		"[%s][%s] IDBG_STA=[0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag), dev0_sta);
	if (ret > 0)
		len += ret;

	val = ((dev0_sta & REG_FLD_MASK(DBG_STATE_FLD_dbg_intfhub_ctrl_fsm_state)) >>
		REG_FLD_SHIFT(DBG_STATE_FLD_dbg_intfhub_ctrl_fsm_state));

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",intfhub_fsm=[%s]",
		((val == 0) ? "IDLE" : ((val == 1) ? "PREPARE" :
			((val == 2) ? "READY" : ((val == 3) ? "CKON" :
			((val == 4) ? "CKOFF" : "WAIT"))))));
	if (ret > 0)
		len += ret;

	val = ((dev0_sta & REG_FLD_MASK(DBG_STATE_FLD_dbg_tx_fsm_state)) >>
		REG_FLD_SHIFT(DBG_STATE_FLD_dbg_tx_fsm_state));

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",tx_fsm=[%s]",
		((val == 0) ? "IDLE" : ((val == 1) ? "HEADER" :
			((val == 2) ? "PAYLOAD" : ((val == 3) ? "CRC" :
			((val == 4) ? "ESP" : "END"))))));
	if (ret > 0)
		len += ret;

	val = ((dev0_sta & REG_FLD_MASK(DBG_STATE_FLD_dbg_rx_fsm_state)) >>
		REG_FLD_SHIFT(DBG_STATE_FLD_dbg_rx_fsm_state));

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",rx_fsm=[%s]",
		((val == 0) ? "IDLE" : ((val == 1) ? "DEC" :
			((val == 2) ? "DEV0" : ((val == 3) ? "DEV1" :
			((val == 4) ? "DEV2" : "END"))))));
	if (ret > 0)
		len += ret;

	val = ((dev0_sta & REG_FLD_MASK(DBG_STATE_FLD_dbg_dev0_rx_fsm_state)) >>
		REG_FLD_SHIFT(DBG_STATE_FLD_dbg_dev0_rx_fsm_state));

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",dev0_rx_fsm=[%s]",
		((val == 0) ? "IDLE" : ((val == 1) ? "HEADER" :
			((val == 2) ? "PAYLOAD" : ((val == 3) ? "CRC" :
			((val == 4) ? "ESP" : "END"))))));
	if (ret > 0)
		len += ret;

	val = ((dev0_sta & REG_FLD_MASK(DBG_STATE_FLD_dbg_dev1_rx_fsm_state)) >>
		REG_FLD_SHIFT(DBG_STATE_FLD_dbg_dev1_rx_fsm_state));

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",dev1_rx_fsm=[%s]",
		((val == 0) ? "IDLE" : ((val == 1) ? "HEADER" :
			((val == 2) ? "PAYLOAD" : ((val == 3) ? "CRC" :
			((val == 4) ? "ESP" : "END"))))));
	if (ret > 0)
		len += ret;

	val = ((dev0_sta & REG_FLD_MASK(DBG_STATE_FLD_dbg_dev2_rx_fsm_state)) >>
		REG_FLD_SHIFT(DBG_STATE_FLD_dbg_dev2_rx_fsm_state));

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",dev2_rx_fsm=[%s]",
		((val == 0) ? "IDLE" : ((val == 1) ? "HEADER" :
			((val == 2) ? "PAYLOAD" : ((val == 3) ? "CRC" :
			((val == 4) ? "ESP" : "END"))))));
	if (ret > 0)
		len += ret;

	val = ((dev0_sta & REG_FLD_MASK(DBG_STATE_FLD_intfhub_dbg)) >>
		REG_FLD_SHIFT(DBG_STATE_FLD_intfhub_dbg));

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",intfhub_dbg=[0x%x]", val);
	if (ret > 0)
		len += ret;

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	dev0_sta = UARTHUB_REG_READ(DEV0_CRC_STA_ADDR);
	dev1_sta = UARTHUB_REG_READ(DEV1_CRC_STA_ADDR);
	dev2_sta = UARTHUB_REG_READ(DEV2_CRC_STA_ADDR);
	ret = snprintf(dmp_info_buf, DBG_LOG_LEN,
		"[%s][%s] IDEVx_CRC_STA(0x20/0x54/0x94)=[0x%x-0x%x-0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag),
		dev0_sta, dev1_sta, dev2_sta);
	if (ret > 0)
		len += ret;

	dev0_sta = UARTHUB_REG_READ(DEV0_RX_ERR_CRC_STA_ADDR);
	dev1_sta = UARTHUB_REG_READ(DEV1_RX_ERR_CRC_STA_ADDR);
	dev2_sta = UARTHUB_REG_READ(DEV2_RX_ERR_CRC_STA_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IDEVx_RX_ERR_CRC_STA(0x10/0x14/0x18)=[0x%x-0x%x-0x%x]",
		dev0_sta, dev1_sta, dev2_sta);
	if (ret > 0)
		len += ret;

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	sta_irq = UARTHUB_REG_READ(DEV0_IRQ_STA_ADDR);
	ret = snprintf(dmp_info_buf, DBG_LOG_LEN,
		"[%s][%s] IDEV0_IRQ_STA/MASK(0x30/0x38)=[0x%x-0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag),
		sta_irq, UARTHUB_REG_READ(DEV0_IRQ_MASK_ADDR));
	if (ret > 0)
		len += ret;

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IIRQ_STA/MASK(0xd0/0xd8)=[0x%x-0x%x]",
		UARTHUB_REG_READ(IRQ_STA_ADDR),
		UARTHUB_REG_READ(IRQ_MASK_ADDR));
	if (ret > 0)
		len += ret;

	val = UARTHUB_REG_READ(STA0_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ISTA0(0xe0)=[0x%x]", val);
	if (ret > 0)
		len += ret;

	val = UARTHUB_REG_READ(CON2_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ICON2(0xc8)=[0x%x]", val);
	if (ret > 0)
		len += ret;

	pr_info("%s\n", dmp_info_buf);
	UARTHUB_DEBUG_PRINT_UARTHUB_IRQ_ERR_DEBUG_KEYWORD(sta_irq, 1);

	return 0;
}

#if UARTHUB_WAKEUP_DEBUG_EN
int uarthub_dump_sspm_wakeup_debug_info_mt6993(const char *tag)
{
	int val = 0;
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int len = 0;
	int ret = 0;
	const char *def_tag = "SSPM_WAKEUP_DBG";

	val = SPM_26M_ULPOSC_ACK_STA_GET_spm_26m_ulposc_ack(SPM_26M_ULPOSC_ACK_STA_ADDR);
	len = 0;
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s][%s][%s] spm_26m_ulposc_sta(0x4)=[%d]",
		def_tag, ((tag == NULL) ? "null" : tag), MT6993_UARTHUB_DUMP_VERSION, val);
	if (ret > 0)
		len += ret;

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",SSPM_WAKEUP_IRQ_STA/EN/MASK(0x8/0xc/0x14)=[0x%x-0x%x-0x%x]",
		UARTHUB_REG_READ(SSPM_WAKEUP_IRQ_STA_ADDR),
		UARTHUB_REG_READ(SSPM_WAKEUP_IRQ_STA_EN_ADDR),
		UARTHUB_REG_READ(SSPM_WAKEUP_IRQ_STA_MASK_ADDR));
	if (ret > 0)
		len += ret;

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",NOACK_TIMEOUT_SPM/UARTHUB(0x18/0x1c)=[0x%x-0x%x]",
		SPM_NOACK_TIMEOUT_SET_GET_spm_noack_timeout_set(
			SPM_NOACK_TIMEOUT_SET_ADDR),
		UARTHUB_NOACK_TIMEOUT_SET_GET_uarthub_noack_timeout_set(
			UARTHUB_NOACK_TIMEOUT_SET_ADDR));
	if (ret > 0)
		len += ret;

	return 0;
}
#endif

int uarthub_dump_extend_debug_info_mt6993(const char *tag)
{
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int len = 0, ret = 0;
	const char *def_tag = "HUB_DBG_EX";
	int peri_cken;
	uint64_t peri_par_ts;
	uint64_t peri_par_ts_l, peri_par_ts_h;
	uint8_t fifo_cur_idx[2][4] = {0};
	uint8_t fifo_cur_data[2][4][32] = {0};

	// Dump xon/xoff timestamp
	len = 0;
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN,
		"[%s][%s] XOFF[send,recv]=[u0:%llu,%llu,u2:%llu,%llu,ucmm:%llu,%llu]",
		def_tag, ((tag == NULL) ? "null" : tag),
		uarthub_get_xoff_ts_mt6993(uartip_id_ap, 0, 1), // UART0, send, xoff
		uarthub_get_xoff_ts_mt6993(uartip_id_ap, 1, 1), // UART0, recv, xoff
		uarthub_get_xoff_ts_mt6993(uartip_id_adsp, 0, 1), // UART2, send, xoff
		uarthub_get_xoff_ts_mt6993(uartip_id_adsp, 1, 1), // UART2, recv, xoff
		uarthub_get_xoff_ts_mt6993(uartip_id_cmm, 0, 1), // CMM , send, xoff
		uarthub_get_xoff_ts_mt6993(uartip_id_cmm, 1, 1));// CMM , recv, xoff
	if (ret > 0)
		len += ret;

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",XON[send,recv]=[u0:%llu,%llu,u2:%llu,%llu,ucmm:%llu,%llu]",
		uarthub_get_xoff_ts_mt6993(uartip_id_ap, 0, 0), // UART0, send, xon
		uarthub_get_xoff_ts_mt6993(uartip_id_ap, 1, 0), // UART0, recv, xon
		uarthub_get_xoff_ts_mt6993(uartip_id_adsp, 0, 0), // UART2, send, xon
		uarthub_get_xoff_ts_mt6993(uartip_id_adsp, 1, 0), // UART2, recv, xon
		uarthub_get_xoff_ts_mt6993(uartip_id_cmm, 0, 0), // CMM , send, xon
		uarthub_get_xoff_ts_mt6993(uartip_id_cmm, 1, 0));// CMM , recv, xon
	if (ret > 0)
		len += ret;

	peri_cken = (UARTHUB_REG_READ_BIT(peri_par_remap_addr_mt6993 + PERIPAR_DEBUG_AO_CTRL_0,
		PERIPAR_AO_DEBUG_CKEN_MASK) >> PERIPAR_AO_DEBUG_CKEN_SHIFT);
	peri_par_ts_h =	UARTHUB_REG_READ(peri_par_remap_addr_mt6993 + PERIPAR_SYS_TIMER_H);
	peri_par_ts_l = UARTHUB_REG_READ(peri_par_remap_addr_mt6993 + PERIPAR_SYS_TIMER_L);
	peri_par_ts = UARTHUB_REG_READ(peri_par_remap_addr_mt6993 + PERIPAR_SYS_TIMER_H);
	peri_par_ts = (peri_par_ts << 32) | UARTHUB_REG_READ(peri_par_remap_addr_mt6993 + PERIPAR_SYS_TIMER_L);

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len, ",peri_cken=[%d],peri_sys_ts=[%llu],h=[%llu],l=[%llu]",
		peri_cken, peri_par_ts, peri_par_ts_h, peri_par_ts_l);
	if (ret > 0)
		len += ret;

	pr_info("%s\n", dmp_info_buf);

	// Dump UART fifo data
	UARTHUB_DEBUG_GET_DEBUG_FIFO_32_BYTE_DATA(0xF);
	UARTHUB_DEBUG_PRINT_DEBUG_FIFO_32_BYTE_DATA_BY_DEV(def_tag, tag, uartip_id_ap);
	UARTHUB_DEBUG_PRINT_DEBUG_FIFO_32_BYTE_DATA_BY_DEV(def_tag, tag, uartip_id_adsp);
	UARTHUB_DEBUG_PRINT_DEBUG_FIFO_32_BYTE_DATA_BY_DEV(def_tag, tag, uartip_id_cmm);

	return 0;
}

int uarthub_read_dbg_monitor_mt6993(int *sel, int *tx_monitor, int *rx_monitor)
{
	if (sel == NULL || tx_monitor == NULL || rx_monitor == NULL) {
		pr_info("%s failed due to parameter is NULL\n", __func__);
		return -1;
	}

	if (DEBUG_MODE_CRTL_GET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR) == 0) {
		pr_info("%s intfhub_cg_en is 0\n", __func__);
		return -2;
	}

	*sel = DEBUG_MODE_CRTL_GET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR);
#if UARTHUB_DEBUG_LOG
	pr_info("%s sel = %d\n", __func__, *sel);
#endif

	tx_monitor[0] = DEBUG_TX_MOINTOR_0_GET_intfhub_debug_tx_monitor0(DEBUG_TX_MOINTOR_0_ADDR);
	tx_monitor[1] = DEBUG_TX_MOINTOR_1_GET_intfhub_debug_tx_monitor1(DEBUG_TX_MOINTOR_1_ADDR);
	tx_monitor[2] = DEBUG_TX_MOINTOR_2_GET_intfhub_debug_tx_monitor2(DEBUG_TX_MOINTOR_2_ADDR);
	tx_monitor[3] = DEBUG_TX_MOINTOR_3_GET_intfhub_debug_tx_monitor3(DEBUG_TX_MOINTOR_3_ADDR);

	rx_monitor[0] = DEBUG_RX_MOINTOR_0_GET_intfhub_debug_rx_monitor0(DEBUG_RX_MOINTOR_0_ADDR);
	rx_monitor[1] = DEBUG_RX_MOINTOR_1_GET_intfhub_debug_rx_monitor1(DEBUG_RX_MOINTOR_1_ADDR);
	rx_monitor[2] = DEBUG_RX_MOINTOR_2_GET_intfhub_debug_rx_monitor2(DEBUG_RX_MOINTOR_2_ADDR);
	rx_monitor[3] = DEBUG_RX_MOINTOR_3_GET_intfhub_debug_rx_monitor3(DEBUG_RX_MOINTOR_3_ADDR);

	return 0;
}

int uarthub_dump_debug_monitor_mt6993(const char *tag)
{
	uarthub_dump_debug_monitor_packet_info_mode_mt6993(tag);
	uarthub_dump_debug_monitor_check_data_mode_mt6993(tag);
	uarthub_dump_debug_monitor_crc_result_mode_mt6993(tag);

	return 0;
}

int uarthub_dump_inband_irq_debug_mt6993(const char *tag)
{
	const char *def_tag = "HUB_DBG_INB";
	unsigned int inb_esc_char = 0;
	unsigned int inb_sta_char = 0;
	unsigned int inb_irq_ctrl = 0;
	unsigned int inb_sta = 0;

	inb_esc_char = UARTHUB_REG_READ(INB_ESC_CHAR_ADDR(uartip_base_map_mt6993[uartip_id_cmm]));
	inb_sta_char = UARTHUB_REG_READ(INB_STA_CHAR_ADDR(uartip_base_map_mt6993[uartip_id_cmm]));
	inb_irq_ctrl = UARTHUB_REG_READ(INB_IRQ_CTL_ADDR(uartip_base_map_mt6993[uartip_id_cmm]));
	inb_sta = UARTHUB_REG_READ(INB_STA_ADDR(uartip_base_map_mt6993[uartip_id_cmm]));

	pr_info("[%s][%s] INB_ESC_CHAR=[0x%x],INB_STA_CHAR=[0x%x],INB_IRQ_CTL=[0x%x],INB_STA=[0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag),
		inb_esc_char, inb_sta_char, inb_irq_ctrl, inb_sta);

	return 0;
}

int uarthub_debug_monitor_ctrl_mt6993(int enable, int mode, int ctrl)
{
	if (mode < 0 || mode > 2) {
		pr_notice("[%s] not support mode(%d)\n", __func__, mode);
		return UARTHUB_ERR_PARA_WRONG;
	}

	DEBUG_MODE_CRTL_SET_intfhub_cg_en(
		DEBUG_MODE_CRTL_ADDR, ((enable == 0) ? 0 : 1));

	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR, mode);

	if (mode == 0)
		DEBUG_MODE_CRTL_SET_packet_info_bypass_esp_pkt_en(
			DEBUG_MODE_CRTL_ADDR, ctrl);
	else if (mode == 1)
		DEBUG_MODE_CRTL_SET_check_data_mode_select(
			DEBUG_MODE_CRTL_ADDR, ctrl);
	else if (mode == 2)
		DEBUG_MODE_CRTL_SET_tx_monitor_display_rx_crc_data_en(
			DEBUG_MODE_CRTL_ADDR, ctrl);

	return 0;
}

int uarthub_debug_monitor_stop_mt6993(int stop)
{
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_stop(DEBUG_MODE_CRTL_ADDR, stop);
	return 0;
}

int uarthub_debug_monitor_clr_mt6993(void)
{
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_clr(DEBUG_MODE_CRTL_ADDR, 1);
	return 0;
}

int uarthub_dump_debug_monitor_packet_info_mode_mt6993(const char *tag)
{
	int debug_monitor_sel = 0;
	const char *def_tag = "HUB_DBG_PKT_INF";
	int tx_monitor[4] = { 0 };
	int rx_monitor[4] = { 0 };

	if (uarthub_read_dbg_monitor_mt6993(&debug_monitor_sel, tx_monitor, rx_monitor) < 0)
		return 0;

	if (debug_monitor_sel != 0x0)
		return 0;

	pr_info("[%s][%s] TIME_PRECISE=[0x%x],BYPASS_ESP=[%d],PKT_INFO_MON[0:3]=[R(%d):%s%s%s%s%s-0x%x-0x%x,%s%s%s%s%s-0x%x-0x%x,%s%s%s%s%s-0x%x-0x%x,%s%s%s%s%s-0x%x-0x%x,T(%d):%s%s%s%s%s-0x%x-0x%x,%s%s%s%s%s-0x%x-0x%x,%s%s%s%s%s-0x%x-0x%x,%s%s%s%s%s-0x%x-0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag),
		DEBUG_MODE_CRTL_GET_packet_info_mode_time_precise(DEBUG_MODE_CRTL_ADDR),
		DEBUG_MODE_CRTL_GET_packet_info_bypass_esp_pkt_en(DEBUG_MODE_CRTL_ADDR),
		DEBUG_MODE_CRTL_GET_packet_info_mode_rx_monitor_pointer(DEBUG_MODE_CRTL_ADDR),
		UARTHUB_DEBUG_GET_DBG_MONITOR_PKT_INFO(rx_monitor[0]),
		UARTHUB_DEBUG_GET_DBG_MONITOR_PKT_INFO(rx_monitor[1]),
		UARTHUB_DEBUG_GET_DBG_MONITOR_PKT_INFO(rx_monitor[2]),
		UARTHUB_DEBUG_GET_DBG_MONITOR_PKT_INFO(rx_monitor[3]),
		DEBUG_MODE_CRTL_GET_packet_info_mode_tx_monitor_pointer(DEBUG_MODE_CRTL_ADDR),
		UARTHUB_DEBUG_GET_DBG_MONITOR_PKT_INFO(tx_monitor[0]),
		UARTHUB_DEBUG_GET_DBG_MONITOR_PKT_INFO(tx_monitor[1]),
		UARTHUB_DEBUG_GET_DBG_MONITOR_PKT_INFO(tx_monitor[2]),
		UARTHUB_DEBUG_GET_DBG_MONITOR_PKT_INFO(tx_monitor[3]));

	return 0;
}

int uarthub_dump_debug_monitor_check_data_mode_mt6993(const char *tag)
{
	int debug_monitor_sel = 0;
	const char *def_tag = "HUB_DBG_CHK_DATA";
	int tx_monitor[4] = { 0 };
	int rx_monitor[4] = { 0 };
	int rx_monitor_pointer, tx_monitor_pointer;
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int len = 0, ret = 0;
	int separate_pos;

	if (uarthub_read_dbg_monitor_mt6993(&debug_monitor_sel, tx_monitor, rx_monitor) < 0)
		return 0;

	if (debug_monitor_sel != 0x1)
		return 0;

	rx_monitor_pointer = DEBUG_MODE_CRTL_GET_check_data_mode_rx_monitor_pointer(DEBUG_MODE_CRTL_ADDR);
	tx_monitor_pointer = DEBUG_MODE_CRTL_GET_check_data_mode_tx_monitor_pointer(DEBUG_MODE_CRTL_ADDR);

	len = 0;
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s][%s] CHK_DATA_MON[0:15]=[R(%d):",
		def_tag, ((tag == NULL) ? "null" : tag), rx_monitor_pointer);
	if (ret > 0)
		len += ret;

	if (rx_monitor_pointer != 15)
		separate_pos = len + ((rx_monitor_pointer * 3) + 2);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"%s%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
		((rx_monitor_pointer == 15) ? "|" : ""),
		UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA(rx_monitor[0]),
		UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA(rx_monitor[1]),
		UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA(rx_monitor[2]),
		UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA(rx_monitor[3]));
	if (ret > 0)
		len += ret;

	if (rx_monitor_pointer != 15)
		dmp_info_buf[separate_pos] = '|';

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",T(%d):", tx_monitor_pointer);
	if (ret > 0)
		len += ret;

	if (tx_monitor_pointer != 15)
		separate_pos = len + ((tx_monitor_pointer * 3) + 2);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"%s%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X]",
		((tx_monitor_pointer == 15) ? "|" : ""),
		UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA(tx_monitor[0]),
		UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA(tx_monitor[1]),
		UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA(tx_monitor[2]),
		UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA(tx_monitor[3]));
	if (ret > 0)
		len += ret;

	if (tx_monitor_pointer != 15)
		dmp_info_buf[separate_pos] = '|';

	pr_info("%s\n", dmp_info_buf);

	return 0;
}

int uarthub_dump_debug_monitor_crc_result_mode_mt6993(const char *tag)
{
	int debug_monitor_sel = 0;
	int rx_crc_data_en = 0;
	const char *def_tag = "HUB_DBG_CRC_INF";
	int tx_monitor[4] = { 0 };
	int rx_monitor[4] = { 0 };

	if (uarthub_read_dbg_monitor_mt6993(&debug_monitor_sel, tx_monitor, rx_monitor) < 0)
		return 0;

	if (debug_monitor_sel != 0x2)
		return 0;

	rx_crc_data_en =
		DEBUG_MODE_CRTL_GET_tx_monitor_display_rx_crc_data_en(DEBUG_MODE_CRTL_ADDR);

	pr_info("[%s][%s] RX_CRC_DATA_EN=[0x%x],MON_PTR=[R:%d,T:%d],RX_CRC_CNT=[M:%d,D:%d]",
		def_tag, ((tag == NULL) ? "null" : tag), rx_crc_data_en,
		DEBUG_MODE_CRTL_GET_crc_result_mode_rx_monitor_pointer(DEBUG_MODE_CRTL_ADDR),
		DEBUG_MODE_CRTL_GET_crc_result_mode_tx_monitor_pointer(DEBUG_MODE_CRTL_ADDR),
		CRC_CNT_GET_crc_rx_match_cnt(CRC_CNT_ADDR),
		CRC_CNT_GET_crc_rx_dismatch_cnt(CRC_CNT_ADDR));

	pr_info("[%s][%s] %s[7:0]=[0x%x-0x%x-0x%x-0x%x-0x%x-0x%x-0x%x-0x%x],RX_CRC_RESULT[7:0]=[0x%x-0x%x-0x%x-0x%x-0x%x-0x%x-0x%x-0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag),
		((rx_crc_data_en == 1) ? "RX_CRC_DATA" : "TX_CRC_RESULT"),
		((tx_monitor[3] & 0xFFFF0000) >> 16), (tx_monitor[3] & 0xFFFF),
		((tx_monitor[2] & 0xFFFF0000) >> 16), (tx_monitor[2] & 0xFFFF),
		((tx_monitor[1] & 0xFFFF0000) >> 16), (tx_monitor[1] & 0xFFFF),
		((tx_monitor[0] & 0xFFFF0000) >> 16), (tx_monitor[0] & 0xFFFF),
		((rx_monitor[3] & 0xFFFF0000) >> 16), (rx_monitor[3] & 0xFFFF),
		((rx_monitor[2] & 0xFFFF0000) >> 16), (rx_monitor[2] & 0xFFFF),
		((rx_monitor[1] & 0xFFFF0000) >> 16), (rx_monitor[1] & 0xFFFF),
		((rx_monitor[0] & 0xFFFF0000) >> 16), (rx_monitor[0] & 0xFFFF));

	return 0;
}

int uarthub_dump_debug_tx_rx_count_mt6993(const char *tag, int trigger_point)
{
	static int cur_tx_pkt_cnt_d0, cur_tx_pkt_cnt_d1, cur_tx_pkt_cnt_d2;
	static int cur_rx_pkt_cnt_d0, cur_rx_pkt_cnt_d1, cur_rx_pkt_cnt_d2;
	static int first_cur_tx_pkt_cnt_d0 = -1, last_cur_tx_pkt_cnt_d0 = -1;
	static int first_cur_rx_pkt_cnt_d0 = -1, last_cur_rx_pkt_cnt_d0 = -1;
	int cur_pkt_cnt_d0_modify = 0;
	static int debug_monitor_sel;
	static int tx_monitor[4] = { 0 };
	static int rx_monitor[4] = { 0 };
	static int tx_monitor_pointer;
	static int rx_monitor_pointer;
	static int fsm_dbg_sta;
	static int d0_wait_for_send_xoff, d1_wait_for_send_xoff, d2_wait_for_send_xoff;
	static int cmm_wait_for_send_xoff, ap_wait_for_send_xoff;
	static int d0_detect_xoff, d1_detect_xoff, d2_detect_xoff;
	static int cmm_detect_xoff, ap_detect_xoff;
	static int d0_rx_bcnt, d1_rx_bcnt, d2_rx_bcnt, cmm_rx_bcnt, ap_rx_bcnt;
	static int d0_tx_bcnt, d1_tx_bcnt, d2_tx_bcnt, cmm_tx_bcnt, ap_tx_bcnt;
	static int first_ap_tx_bcnt = -1, last_ap_tx_bcnt = -1;
	static int first_ap_rx_bcnt = -1, last_ap_rx_bcnt = -1;
	static int first_cmm_tx_bcnt = -1, last_cmm_tx_bcnt = -1;
	static int first_cmm_rx_bcnt = -1, last_cmm_rx_bcnt = -1;
	int bcnt_modify = 0;
	static int pre_trigger_point = -1;
	struct uarthub_uartip_debug_info pkt_cnt = {0};
	struct uarthub_uartip_debug_info debug1 = {0}, debug2 = {0}, debug3 = {0}, debug4 = {0};
	struct uarthub_uartip_debug_info debug5 = {0}, debug6 = {0}, debug7 = {0}, debug8 = {0};
	uint8_t fifo_cur_idx[2][4] = {0};
	uint8_t fifo_cur_data[2][4][32] = {0};
	static int d0_tx_fifoc, d1_tx_fifoc, d2_tx_fifoc, cmm_tx_fifoc, ap_tx_fifoc;
	static int d0_rx_fifoc, d1_rx_fifoc, d2_rx_fifoc, cmm_rx_fifoc, ap_rx_fifoc;
	static struct uarthub_gpio_trx_info gpio_base_addr;
	const char *def_tag = "HUB_DBG";
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int len = 0;
	int ret = 0;
	static uint32_t icon2;
	static uint32_t bypass_mode;
	static int first_bypass_mode = -1, last_bypass_mode = -1;
	int bypass_mode_modify = 0;
	static int bypass_mode_err;
	static uint64_t dump_ts;
	uint32_t dump_nsec = 0;

	if (trigger_point != DUMP0 && trigger_point != DUMP1) {
		pr_notice("[%s] trigger_point = %d is invalid\n", __func__, trigger_point);
		return -1;
	}

	if (trigger_point == DUMP1 && pre_trigger_point == DUMP0) {
		len = 0;
		dump_nsec = do_div(dump_ts, 1000000000);
#if !(UARTHUB_SUPPORT_FPGA)
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s], dump0[%5lu.%06lu] GPIO[mode-dir-out-pu-pd]=[R:%d-%s-%s-%d-%d,T:%d-%s-%s-%d-%d,BT_RST:%d-%s-%s-%d-%d]",
			def_tag, ((tag == NULL) ? "null" : tag),
			(unsigned long)dump_ts, (unsigned long)(dump_nsec/1000),
			gpio_base_addr.rx_mode.gpio_value,
			((gpio_base_addr.rx_dir.gpio_value == 0) ? "IN" : "OUT"),
			((gpio_base_addr.rx_dataout.gpio_value == 0) ? "LOW" : "HIGH"),
			gpio_base_addr.rx_pu.gpio_value,
			gpio_base_addr.rx_pd.gpio_value,
			gpio_base_addr.tx_mode.gpio_value,
			((gpio_base_addr.tx_dir.gpio_value == 0) ? "IN" : "OUT"),
			((gpio_base_addr.tx_dataout.gpio_value == 0) ? "LOW" : "HIGH"),
			gpio_base_addr.tx_pu.gpio_value,
			gpio_base_addr.tx_pd.gpio_value,
			gpio_base_addr.bt_rst_mode.gpio_value,
			((gpio_base_addr.bt_rst_dir.gpio_value == 0) ? "IN" : "OUT"),
			((gpio_base_addr.bt_rst_dataout.gpio_value == 0) ? "LOW" : "HIGH"),
			gpio_base_addr.bt_rst_pu.gpio_value,
			gpio_base_addr.bt_rst_pd.gpio_value);
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",pcnt=[R:%d-%d-%d,T:%d-%d-%d]",
			cur_rx_pkt_cnt_d0, cur_rx_pkt_cnt_d1, cur_rx_pkt_cnt_d2,
			cur_tx_pkt_cnt_d0, cur_tx_pkt_cnt_d1, cur_tx_pkt_cnt_d2);
		if (ret > 0)
			len += ret;
#else
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s], dump0[%5lu.%06lu] pcnt=[R:%d-%d-%d,T:%d-%d-%d]",
			def_tag, ((tag == NULL) ? "null" : tag),
			(unsigned long)dump_ts, (unsigned long)(dump_nsec/1000),
			cur_rx_pkt_cnt_d0, cur_rx_pkt_cnt_d1, cur_rx_pkt_cnt_d2,
			cur_tx_pkt_cnt_d0, cur_tx_pkt_cnt_d1, cur_tx_pkt_cnt_d2);
		if (ret > 0)
			len += ret;
#endif

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",ICON2(0xc8)=[0x%x]", icon2);
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",bcnt=[R:%d-%d-%d-%d-%d,T:%d-%d-%d-%d-%d]",
			d0_rx_bcnt, d1_rx_bcnt, d2_rx_bcnt, cmm_rx_bcnt, ap_rx_bcnt,
			d0_tx_bcnt, d1_tx_bcnt, d2_tx_bcnt, cmm_tx_bcnt, ap_tx_bcnt);
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",fifo_woffset=[R:%d-%d-%d-%d-%d,T:%d-%d-%d-%d-%d]",
			d0_rx_fifoc, d1_rx_fifoc, d2_rx_fifoc, cmm_rx_fifoc, ap_rx_fifoc,
			d0_tx_fifoc, d1_tx_fifoc, d2_tx_fifoc, cmm_tx_fifoc, ap_tx_fifoc);
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",wsend_xoff=[%d-%d-%d-%d-%d]",
			d0_wait_for_send_xoff, d1_wait_for_send_xoff,
			d2_wait_for_send_xoff, cmm_wait_for_send_xoff,
			ap_wait_for_send_xoff);
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",det_xoff=[%d-%d-%d-%d-%d]",
				d0_detect_xoff, d1_detect_xoff, d2_detect_xoff,
				cmm_detect_xoff, ap_detect_xoff);
		if (ret > 0)
			len += ret;

		if (debug_monitor_sel == 0x1) {
			len = uarthub_record_check_data_mode_sta_to_buffer_mt6993(
				dmp_info_buf, len, debug_monitor_sel, tx_monitor, rx_monitor,
				tx_monitor_pointer, rx_monitor_pointer, NULL);
		}

		UARTHUB_DEBUG_PRINT_DEBUG_FIFO_32_BYTE_DATA_ONE_LINE(NULL, 0xF);

		len = uarthub_record_intfhub_fsm_sta_to_buffer(
			dmp_info_buf, len, fsm_dbg_sta);

		pr_info("%s\n", dmp_info_buf);

#if !(UARTHUB_SUPPORT_FPGA)
		UARTHUB_DEBUG_PRINT_GPIO_DEBUG_KEYWORD(
			gpio_base_addr.rx_mode, gpio_base_addr.tx_mode, gpio_base_addr.bt_rst_mode,
			gpio_base_addr.bt_rst_dir, gpio_base_addr.bt_rst_dataout, 1);
#endif
		UARTHUB_DEBUG_PRINT_DET_XOFF_DEBUG_KEYWORD(d0_detect_xoff, d1_detect_xoff,
			d2_detect_xoff, cmm_detect_xoff, ap_detect_xoff, 0);
		UARTHUB_DEBUG_PRINT_WSEND_XOFF_DEBUG_KEYWORD(d0_wait_for_send_xoff,
			d1_wait_for_send_xoff, d2_wait_for_send_xoff,
			cmm_wait_for_send_xoff, ap_wait_for_send_xoff, 0);
	}

	dump_ts = sched_clock();

	if (uarthub_is_apb_bus_clk_enable_mt6993() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

#if !(UARTHUB_SUPPORT_FPGA)
	uarthub_get_gpio_trx_info_mt6993(&gpio_base_addr);
#endif

	icon2 = UARTHUB_REG_READ(CON2_ADDR);
	bypass_mode = ((icon2 & REG_FLD_MASK(CON2_FLD_intfhub_bypass)) >>
		REG_FLD_SHIFT(CON2_FLD_intfhub_bypass));

	pkt_cnt.dev0 = UARTHUB_REG_READ(DEV0_PKT_CNT_ADDR);
	pkt_cnt.dev1 = UARTHUB_REG_READ(DEV1_PKT_CNT_ADDR);
	pkt_cnt.dev2 = UARTHUB_REG_READ(DEV2_PKT_CNT_ADDR);

	cur_tx_pkt_cnt_d0 = ((pkt_cnt.dev0 & REG_FLD_MASK(DEV0_PKT_CNT_FLD_dev0_tx_pkt_cnt)) >>
		REG_FLD_SHIFT(DEV0_PKT_CNT_FLD_dev0_tx_pkt_cnt));
	cur_tx_pkt_cnt_d1 = ((pkt_cnt.dev1 & REG_FLD_MASK(DEV1_PKT_CNT_FLD_dev1_tx_pkt_cnt)) >>
		REG_FLD_SHIFT(DEV1_PKT_CNT_FLD_dev1_tx_pkt_cnt));
	cur_tx_pkt_cnt_d2 = ((pkt_cnt.dev2 & REG_FLD_MASK(DEV2_PKT_CNT_FLD_dev2_tx_pkt_cnt)) >>
		REG_FLD_SHIFT(DEV2_PKT_CNT_FLD_dev2_tx_pkt_cnt));
	cur_rx_pkt_cnt_d0 = ((pkt_cnt.dev0 & REG_FLD_MASK(DEV0_PKT_CNT_FLD_dev0_rx_pkt_cnt)) >>
		REG_FLD_SHIFT(DEV0_PKT_CNT_FLD_dev0_rx_pkt_cnt));
	cur_rx_pkt_cnt_d1 = ((pkt_cnt.dev1 & REG_FLD_MASK(DEV1_PKT_CNT_FLD_dev1_rx_pkt_cnt)) >>
		REG_FLD_SHIFT(DEV1_PKT_CNT_FLD_dev1_rx_pkt_cnt));
	cur_rx_pkt_cnt_d2 = ((pkt_cnt.dev2 & REG_FLD_MASK(DEV2_PKT_CNT_FLD_dev2_rx_pkt_cnt)) >>
		REG_FLD_SHIFT(DEV2_PKT_CNT_FLD_dev2_rx_pkt_cnt));

	if (uarthub_read_dbg_monitor_mt6993(&debug_monitor_sel, tx_monitor, rx_monitor) == 0) {
		if (debug_monitor_sel == 0x1) {
			tx_monitor_pointer = DEBUG_MODE_CRTL_GET_check_data_mode_tx_monitor_pointer(
				DEBUG_MODE_CRTL_ADDR);
			rx_monitor_pointer = DEBUG_MODE_CRTL_GET_check_data_mode_rx_monitor_pointer(
				DEBUG_MODE_CRTL_ADDR);
		}
	}

	// Dump UART fifo data
	UARTHUB_DEBUG_GET_DEBUG_FIFO_32_BYTE_DATA(0xF);

	fsm_dbg_sta = UARTHUB_REG_READ(DBG_STATE_ADDR);

	UARTHUB_DEBUG_READ_DEBUG_REG(dev0, uartip, uartip_id_ap);
	UARTHUB_DEBUG_READ_DEBUG_REG(dev1, uartip, uartip_id_md);
	UARTHUB_DEBUG_READ_DEBUG_REG(dev2, uartip, uartip_id_adsp);
	UARTHUB_DEBUG_READ_DEBUG_REG(cmm, uartip, uartip_id_cmm);

	if (apuart_base_map_mt6993[3] != NULL) {
		UARTHUB_DEBUG_READ_DEBUG_REG(ap, apuart, 3);
	} else {
		debug1.ap = 0;
		debug2.ap = 0;
		debug3.ap = 0;
		debug5.ap = 0;
		debug6.ap = 0;
		debug8.ap = 0;
	}

	d0_wait_for_send_xoff = UARTHUB_DEBUG_GET_XCSTATE_WSEND_XOFF(debug1.dev0);
	d1_wait_for_send_xoff = UARTHUB_DEBUG_GET_XCSTATE_WSEND_XOFF(debug1.dev1);
	d2_wait_for_send_xoff = UARTHUB_DEBUG_GET_XCSTATE_WSEND_XOFF(debug1.dev2);
	cmm_wait_for_send_xoff = UARTHUB_DEBUG_GET_XCSTATE_WSEND_XOFF(debug1.cmm);
	ap_wait_for_send_xoff = UARTHUB_DEBUG_GET_XCSTATE_WSEND_XOFF(debug1.ap);

	d0_detect_xoff = UARTHUB_DEBUG_GET_SWTXDIS_DET_XOFF(debug8.dev0);
	d1_detect_xoff = UARTHUB_DEBUG_GET_SWTXDIS_DET_XOFF(debug8.dev1);
	d2_detect_xoff = UARTHUB_DEBUG_GET_SWTXDIS_DET_XOFF(debug8.dev2);
	cmm_detect_xoff = UARTHUB_DEBUG_GET_SWTXDIS_DET_XOFF(debug8.cmm);
	ap_detect_xoff = UARTHUB_DEBUG_GET_SWTXDIS_DET_XOFF(debug8.ap);

	d0_rx_bcnt = UARTHUB_DEBUG_GET_OP_RX_REQ(debug5.dev0, debug6.dev0);
	d1_rx_bcnt = UARTHUB_DEBUG_GET_OP_RX_REQ(debug5.dev1, debug6.dev1);
	d2_rx_bcnt = UARTHUB_DEBUG_GET_OP_RX_REQ(debug5.dev2, debug6.dev2);
	cmm_rx_bcnt = UARTHUB_DEBUG_GET_OP_RX_REQ(debug5.cmm, debug6.cmm);
	ap_rx_bcnt = UARTHUB_DEBUG_GET_OP_RX_REQ(debug5.ap, debug6.ap);
	d0_tx_bcnt = UARTHUB_DEBUG_GET_IP_TX_DMA(debug2.dev0, debug3.dev0);
	d1_tx_bcnt = UARTHUB_DEBUG_GET_IP_TX_DMA(debug2.dev1, debug3.dev1);
	d2_tx_bcnt = UARTHUB_DEBUG_GET_IP_TX_DMA(debug2.dev2, debug3.dev2);
	cmm_tx_bcnt = UARTHUB_DEBUG_GET_IP_TX_DMA(debug2.cmm, debug3.cmm);
	ap_tx_bcnt = UARTHUB_DEBUG_GET_IP_TX_DMA(debug2.ap, debug3.ap);

	d0_rx_fifoc = UARTHUB_DEBUG_GET_RX_WOFFSET(debug7.dev0);
	d1_rx_fifoc = UARTHUB_DEBUG_GET_RX_WOFFSET(debug7.dev1);
	d2_rx_fifoc = UARTHUB_DEBUG_GET_RX_WOFFSET(debug7.dev2);
	cmm_rx_fifoc = UARTHUB_DEBUG_GET_RX_WOFFSET(debug7.cmm);
	ap_rx_fifoc = UARTHUB_DEBUG_GET_RX_WOFFSET(debug7.ap);
	d0_tx_fifoc = UARTHUB_DEBUG_GET_TX_WOFFSET(debug4.dev0);
	d1_tx_fifoc = UARTHUB_DEBUG_GET_TX_WOFFSET(debug4.dev1);
	d2_tx_fifoc = UARTHUB_DEBUG_GET_TX_WOFFSET(debug4.dev2);
	cmm_tx_fifoc = UARTHUB_DEBUG_GET_TX_WOFFSET(debug4.cmm);
	ap_tx_fifoc = UARTHUB_DEBUG_GET_TX_WOFFSET(debug4.ap);

	if (trigger_point == DUMP0) {
		UARTHUB_DEBUG_INIT_AP_TX_CMD_TMO_BYPASS_ERR(first, 1, last, 0);
		if (bypass_mode == 0) {
			UARTHUB_DEBUG_INIT_AP_TX_CMD_TMO_PKT_CNT_ERR(
				first, 1, last, 0);
		}
		UARTHUB_DEBUG_INIT_AP_TX_CMD_TMO_BYTE_CNT_ERR(first, 1, last, 0);
	} else {
		if (last_bypass_mode != bypass_mode) {
			bypass_mode_modify = 1;
			UARTHUB_DEBUG_INIT_AP_TX_CMD_TMO_BYPASS_ERR(first, -1, last, 1);
		}

		if (bypass_mode == 0) {
			if ((last_cur_tx_pkt_cnt_d0 != cur_tx_pkt_cnt_d0) ||
					(last_cur_rx_pkt_cnt_d0 != cur_rx_pkt_cnt_d0)) {
				cur_pkt_cnt_d0_modify = 1;
				UARTHUB_DEBUG_INIT_AP_TX_CMD_TMO_PKT_CNT_ERR(first, -1, last, 1);
			}

			if ((last_ap_tx_bcnt != ap_tx_bcnt) || (last_ap_rx_bcnt != ap_rx_bcnt) ||
				(last_cmm_tx_bcnt != cmm_tx_bcnt) || (last_cmm_rx_bcnt != cmm_rx_bcnt)) {
				bcnt_modify = 1;
				UARTHUB_DEBUG_INIT_AP_TX_CMD_TMO_BYTE_CNT_ERR(first, -1, last, 1);
			}
		} else {
			if ((last_ap_tx_bcnt != ap_tx_bcnt) || (last_ap_rx_bcnt != ap_rx_bcnt)) {
				bcnt_modify = 1;
				UARTHUB_DEBUG_INIT_AP_TX_CMD_TMO_BYTE_CNT_ERR(first, -1, last, 1);
			}
		}
	}

	if (trigger_point != DUMP0) {
		len = 0;
		dump_nsec = do_div(dump_ts, 1000000000);
#if !(UARTHUB_SUPPORT_FPGA)
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s], dump1[%5lu.%06lu] GPIO[mode-dir-out-pu-pd]=[R:%d-%s-%s-%d-%d,T:%d-%s-%s-%d-%d,BT_RST:%d-%s-%s-%d-%d]",
			def_tag, ((tag == NULL) ? "null" : tag),
			(unsigned long)dump_ts, (unsigned long)(dump_nsec/1000),
			gpio_base_addr.rx_mode.gpio_value,
			((gpio_base_addr.rx_dir.gpio_value == 0) ? "IN" : "OUT"),
			((gpio_base_addr.rx_dataout.gpio_value == 0) ? "LOW" : "HIGH"),
			gpio_base_addr.rx_pu.gpio_value,
			gpio_base_addr.rx_pd.gpio_value,
			gpio_base_addr.tx_mode.gpio_value,
			((gpio_base_addr.tx_dir.gpio_value == 0) ? "IN" : "OUT"),
			((gpio_base_addr.tx_dataout.gpio_value == 0) ? "LOW" : "HIGH"),
			gpio_base_addr.tx_pu.gpio_value,
			gpio_base_addr.tx_pd.gpio_value,
			gpio_base_addr.bt_rst_mode.gpio_value,
			((gpio_base_addr.bt_rst_dir.gpio_value == 0) ? "IN" : "OUT"),
			((gpio_base_addr.bt_rst_dataout.gpio_value == 0) ? "LOW" : "HIGH"),
			gpio_base_addr.bt_rst_pu.gpio_value,
			gpio_base_addr.bt_rst_pd.gpio_value);
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",pcnt=[R:%d-%d-%d,T:%d-%d-%d]",
			cur_rx_pkt_cnt_d0, cur_rx_pkt_cnt_d1, cur_rx_pkt_cnt_d2,
			cur_tx_pkt_cnt_d0, cur_tx_pkt_cnt_d1, cur_tx_pkt_cnt_d2);
		if (ret > 0)
			len += ret;
#else
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s], dump1[%5lu.%06lu] pcnt=[R:%d-%d-%d,T:%d-%d-%d]",
			def_tag, ((tag == NULL) ? "null" : tag),
			(unsigned long)dump_ts, (unsigned long)(dump_nsec/1000),
			cur_rx_pkt_cnt_d0, cur_rx_pkt_cnt_d1, cur_rx_pkt_cnt_d2,
			cur_tx_pkt_cnt_d0, cur_tx_pkt_cnt_d1, cur_tx_pkt_cnt_d2);
		if (ret > 0)
			len += ret;
#endif

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",ICON2(0xc8)=[0x%x]", icon2);
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",bcnt=[R:%d-%d-%d-%d-%d,T:%d-%d-%d-%d-%d]",
			d0_rx_bcnt, d1_rx_bcnt, d2_rx_bcnt, cmm_rx_bcnt, ap_rx_bcnt,
			d0_tx_bcnt, d1_tx_bcnt, d2_tx_bcnt, cmm_tx_bcnt, ap_tx_bcnt);
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",fifo_woffset=[R:%d-%d-%d-%d-%d,T:%d-%d-%d-%d-%d]",
			d0_rx_fifoc, d1_rx_fifoc, d2_rx_fifoc, cmm_rx_fifoc, ap_rx_fifoc,
			d0_tx_fifoc, d1_tx_fifoc, d2_tx_fifoc, cmm_tx_fifoc, ap_tx_fifoc);
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",wsend_xoff=[%d-%d-%d-%d-%d]",
			d0_wait_for_send_xoff, d1_wait_for_send_xoff,
			d2_wait_for_send_xoff, cmm_wait_for_send_xoff,
			ap_wait_for_send_xoff);
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",det_xoff=[%d-%d-%d-%d-%d]",
			d0_detect_xoff, d1_detect_xoff,
			d2_detect_xoff, cmm_detect_xoff,
			ap_detect_xoff);
		if (ret > 0)
			len += ret;

		if (debug_monitor_sel == 0x1) {
			len = uarthub_record_check_data_mode_sta_to_buffer_mt6993(
				dmp_info_buf, len, debug_monitor_sel, tx_monitor, rx_monitor,
				tx_monitor_pointer, rx_monitor_pointer, NULL);
		}

		UARTHUB_DEBUG_PRINT_DEBUG_FIFO_32_BYTE_DATA_ONE_LINE(NULL, 0xF);

		len = uarthub_record_intfhub_fsm_sta_to_buffer(
			dmp_info_buf, len, fsm_dbg_sta);

		pr_info("%s\n", dmp_info_buf);

		if (pre_trigger_point == DUMP0) {
			UARTHUB_DEBUG_PRINT_RX_WOFFSET_DEBUG_KEYWORD(d0_rx_fifoc, d1_rx_fifoc,
				d2_rx_fifoc, cmm_rx_fifoc, ap_rx_fifoc, 0, 0);
			UARTHUB_DEBUG_PRINT_TX_WOFFSET_DEBUG_KEYWORD(d0_tx_fifoc, d1_tx_fifoc,
				d2_tx_fifoc, cmm_tx_fifoc, ap_tx_fifoc, 0, 0);
			UARTHUB_DEBUG_PRINT_DET_XOFF_DEBUG_KEYWORD(d0_detect_xoff, d1_detect_xoff,
				d2_detect_xoff, cmm_detect_xoff, ap_detect_xoff, 1);
			UARTHUB_DEBUG_PRINT_WSEND_XOFF_DEBUG_KEYWORD(d0_wait_for_send_xoff,
				d1_wait_for_send_xoff, d2_wait_for_send_xoff,
				cmm_wait_for_send_xoff, ap_wait_for_send_xoff, 1);
		}

		if (bypass_mode_modify == 1) {
			bypass_mode_modify = 0;
			UARTHUB_DEBUG_PRINT_AP_TX_CMD_TMO_BYPASS_ERR_DEBUG_KEYWORD(
				first_bypass_mode, last_bypass_mode, 1);
			if (first_bypass_mode != last_bypass_mode)
				bypass_mode_err = 1;
		}

		if (bypass_mode == 0 && bypass_mode_err == 0 && cur_pkt_cnt_d0_modify == 1) {
			cur_pkt_cnt_d0_modify = 0;
			UARTHUB_DEBUG_PRINT_AP_TX_CMD_TMO_PKT_CNT_ERR_DEBUG_KEYWORD(
				first_cur_tx_pkt_cnt_d0, last_cur_tx_pkt_cnt_d0,
				first_cur_rx_pkt_cnt_d0, last_cur_rx_pkt_cnt_d0, 1);
		}

		if (bypass_mode_err == 0 && bcnt_modify == 1) {
			bcnt_modify = 0;
			if (bypass_mode == 0) {
				UARTHUB_DEBUG_PRINT_AP_TX_CMD_TMO_BYTE_CNT_ERR_DEBUG_KEYWORD(
					first_ap_tx_bcnt, last_ap_tx_bcnt,
					first_ap_rx_bcnt, last_ap_rx_bcnt,
					first_cmm_tx_bcnt, last_cmm_tx_bcnt,
					first_cmm_rx_bcnt, last_cmm_rx_bcnt, 1);
			} else {
				UARTHUB_DEBUG_PRINT_AP_TX_CMD_TMO_BYTE_CNT_ERR_DEBUG_BYPASS_KEYWORD(
					first_ap_tx_bcnt, last_ap_tx_bcnt,
					first_ap_rx_bcnt, last_ap_rx_bcnt, 1);
			}
		}
	}

	pre_trigger_point = trigger_point;
	return 0;
}

int uarthub_dump_debug_clk_info_mt6993(const char *tag)
{
	int val = 0, val1 = 0, val2 = 0;
#if !(UARTHUB_SUPPORT_FPGA)
	int spm_res_uarthub = 0, spm_res_internal = 0;
	int topckgen_cg = 0, peri_cg = 0;
#endif
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int dev0_sta = 0, dev1_sta = 0, dev2_sta = 0;
	int len = 0;
	int ret = 0;
	int debug_monitor_sel = 0;
	int tx_monitor[4] = { 0 };
	int rx_monitor[4] = { 0 };
	int tx_monitor_pointer = 0, rx_monitor_pointer = 0;
	int fsm_dbg_sta = 0;
	uint8_t fifo_cur_idx[2][4] = {0};
	uint8_t fifo_cur_data[2][4][32] = {0};
	struct uarthub_uartip_debug_info pkt_cnt = {0};
	int cur_tx_pkt_cnt_d0;
	int cur_tx_pkt_cnt_d1;
	int cur_tx_pkt_cnt_d2;
	int cur_rx_pkt_cnt_d0;
	int cur_rx_pkt_cnt_d1;
	int cur_rx_pkt_cnt_d2;
	struct uarthub_uartip_debug_info debug1 = {0};
	struct uarthub_uartip_debug_info debug2 = {0};
	struct uarthub_uartip_debug_info debug3 = {0};
	struct uarthub_uartip_debug_info debug4 = {0};
	struct uarthub_uartip_debug_info debug5 = {0};
	struct uarthub_uartip_debug_info debug6 = {0};
	struct uarthub_uartip_debug_info debug7 = {0};
	struct uarthub_uartip_debug_info debug8 = {0};

	if (uarthub_is_apb_bus_clk_enable_mt6993() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	val = DBG_CTRL_GET_intfhub_dbg_sel(DBG_CTRL_ADDR);
	len = 0;
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s] IDBG=[0x%x]", ((tag == NULL) ? "null" : tag), val);
	if (ret > 0)
		len += ret;

	val = UARTHUB_REG_READ(STA0_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ISTA0=[0x%x]", val);
	if (ret > 0)
		len += ret;

	val = uarthub_is_apb_bus_clk_enable_mt6993();
	if (val == 0) {
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len, ",APB=[0x0]");
		if (ret > 0)
			len += ret;
		pr_info("%s\n", dmp_info_buf);
		return -1;
	}

	pkt_cnt.dev0 = UARTHUB_REG_READ(DEV0_PKT_CNT_ADDR);
	pkt_cnt.dev1 = UARTHUB_REG_READ(DEV1_PKT_CNT_ADDR);
	pkt_cnt.dev2 = UARTHUB_REG_READ(DEV2_PKT_CNT_ADDR);

	cur_tx_pkt_cnt_d0 = ((pkt_cnt.dev0 & REG_FLD_MASK(DEV0_PKT_CNT_FLD_dev0_tx_pkt_cnt)) >>
		REG_FLD_SHIFT(DEV0_PKT_CNT_FLD_dev0_tx_pkt_cnt));
	cur_tx_pkt_cnt_d1 = ((pkt_cnt.dev1 & REG_FLD_MASK(DEV1_PKT_CNT_FLD_dev1_tx_pkt_cnt)) >>
		REG_FLD_SHIFT(DEV1_PKT_CNT_FLD_dev1_tx_pkt_cnt));
	cur_tx_pkt_cnt_d2 = ((pkt_cnt.dev2 & REG_FLD_MASK(DEV2_PKT_CNT_FLD_dev2_tx_pkt_cnt)) >>
		REG_FLD_SHIFT(DEV2_PKT_CNT_FLD_dev2_tx_pkt_cnt));
	cur_rx_pkt_cnt_d0 = ((pkt_cnt.dev0 & REG_FLD_MASK(DEV0_PKT_CNT_FLD_dev0_rx_pkt_cnt)) >>
		REG_FLD_SHIFT(DEV0_PKT_CNT_FLD_dev0_rx_pkt_cnt));
	cur_rx_pkt_cnt_d1 = ((pkt_cnt.dev1 & REG_FLD_MASK(DEV1_PKT_CNT_FLD_dev1_rx_pkt_cnt)) >>
		REG_FLD_SHIFT(DEV1_PKT_CNT_FLD_dev1_rx_pkt_cnt));
	cur_rx_pkt_cnt_d2 = ((pkt_cnt.dev2 & REG_FLD_MASK(DEV2_PKT_CNT_FLD_dev2_rx_pkt_cnt)) >>
		REG_FLD_SHIFT(DEV2_PKT_CNT_FLD_dev2_rx_pkt_cnt));

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",pcnt=[R:%d-%d-%d,T:%d-%d-%d]",
		cur_rx_pkt_cnt_d0, cur_rx_pkt_cnt_d1, cur_rx_pkt_cnt_d2,
		cur_tx_pkt_cnt_d0, cur_tx_pkt_cnt_d1, cur_tx_pkt_cnt_d2);
	if (ret > 0)
		len += ret;

	UARTHUB_DEBUG_READ_DEBUG_REG(dev0, uartip, uartip_id_ap);
	UARTHUB_DEBUG_READ_DEBUG_REG(dev1, uartip, uartip_id_md);
	UARTHUB_DEBUG_READ_DEBUG_REG(dev2, uartip, uartip_id_adsp);
	UARTHUB_DEBUG_READ_DEBUG_REG(cmm, uartip, uartip_id_cmm);

	if (apuart_base_map_mt6993[3] != NULL) {
		UARTHUB_DEBUG_READ_DEBUG_REG(ap, apuart, 3);
	}

	UARTHUB_DEBUG_PRINT_DEBUG_2_REG(debug5, 0xF0, 4, debug6, 0x3, 4, ",bcnt=[R:%d-%d-%d-%d-%d");
	UARTHUB_DEBUG_PRINT_DEBUG_2_REG(debug2, 0xF0, 4, debug3, 0x3, 4, ",T:%d-%d-%d-%d-%d]");
	UARTHUB_DEBUG_PRINT_DEBUG_1_REG(debug7, 0x3F, 0, ",fifo_woffset=[R:%d-%d-%d-%d-%d");
	UARTHUB_DEBUG_PRINT_DEBUG_1_REG(debug4, 0x3F, 0, ",T:%d-%d-%d-%d-%d]");
	UARTHUB_DEBUG_PRINT_DEBUG_WSEND_XOFF_REG(debug1, ",wsend_xoff=[%d-%d-%d-%d-%d]", 0);
	UARTHUB_DEBUG_PRINT_DEBUG_DET_XOFF_REG(debug8, ",det_xoff=[%d-%d-%d-%d-%d]", 0);

#if !(UARTHUB_SUPPORT_FPGA)
	val = uarthub_get_uarthub_cg_info_mt6993(&topckgen_cg, &peri_cg);
	if (val >= 0) {
		/* the expect value is 0x0 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",HUB_CG=[topck:0x%x,peri:0x%x,exp:0x0]", topckgen_cg, peri_cg);
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_uart_src_clk_info_mt6993();
	if (val >= 0) {
		/* the expect value is 0x1 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UART_SRC_CLK=[0x%x(%s)]", val, ((val == 0) ? "26M" : "TOPCK"));
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_spm_res_info_mt6993(
		&spm_res_uarthub, &spm_res_internal);
	if (val == 1) {
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",SPM=[1]");
		if (ret > 0)
			len += ret;
	} else if (val == 0 || val == 2) {
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",SPM=[%d(0x%x/0x%x,exp:0x%x/0x%x)]",
			val, spm_res_uarthub, spm_res_internal,
			SPM_REQ_STA_15_UARTHUB_REQ_FIELD, SPM_INTERNAL_ACK_STA_UARTHUB_FIELD);
	if (ret > 0)
			len += ret;
	}

	val = uarthub_get_hwccf_univpll_on_info_mt6993();
	if (val >= 0) {
		/* the expect value is 0x1 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UNIVPLL=[%d]", val);
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_uarthub_mux_info_mt6993();
	val1 = uarthub_get_uart_mux_info_mt6993();
	val2 = uarthub_get_adsp_uart_mux_info_mt6993();
	if (val >= 0) {
		/* the expect value is 0x2 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",HUB/UART/ADSP_MUX=[%d(%s)-%d(%s)-%d(%s)]", val,
			((val == 0) ? "26M" : ((val == 1) ? "104M" : "208M")), val1,
			((val1 == 0) ? "26M" : ((val1 == 1) ? "52M" :
				((val1 == 2) ? "104M" : "208M"))), val2,
			((val2 == 0) ? "26M" : ((val2 == 1) ? "104M" : "208M")));
		if (ret > 0)
			len += ret;
	}

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len, ",HUB/UART/ADSP_CLK=[%d-%d-%d]",
		mt_get_fmeter_freq(FM_UARTHUB_B_CK, CKGEN),
		mt_get_fmeter_freq(FM_UART_CK, CKGEN),
		mt_get_fmeter_freq(F_FADSP_UARTHUB_BCLK_CK, CKGEN));
	if (ret > 0)
		len += ret;
#endif

	dev0_sta = UARTHUB_REG_READ(DEV0_STA_ADDR);
	dev1_sta = UARTHUB_REG_READ(DEV1_STA_ADDR);
	dev2_sta = UARTHUB_REG_READ(DEV2_STA_ADDR);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta) {
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",IDEV_STA=[0x%x]", dev0_sta);
		if (ret > 0)
			len += ret;
	} else {
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",IDEV_STA=[0x%x-0x%x-0x%x]", dev0_sta, dev1_sta, dev2_sta);
		if (ret > 0)
			len += ret;
	}

	val = UARTHUB_REG_READ(STA0_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ISTA0=[0x%x]", val);
	if (ret > 0)
		len += ret;

	val = DBG_CTRL_GET_intfhub_dbg_sel(DBG_CTRL_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IDBG=[0x%x]", val);
	if (ret > 0)
		len += ret;

	if (uarthub_read_dbg_monitor_mt6993(&debug_monitor_sel, tx_monitor, rx_monitor) == 0) {
		if (debug_monitor_sel == 0x1) {
			tx_monitor_pointer = DEBUG_MODE_CRTL_GET_check_data_mode_tx_monitor_pointer(
				DEBUG_MODE_CRTL_ADDR);
			rx_monitor_pointer = DEBUG_MODE_CRTL_GET_check_data_mode_rx_monitor_pointer(
				DEBUG_MODE_CRTL_ADDR);

			len = uarthub_record_check_data_mode_sta_to_buffer_mt6993(
				dmp_info_buf, len, debug_monitor_sel, tx_monitor, rx_monitor,
				tx_monitor_pointer, rx_monitor_pointer, NULL);
		}
	}

	// Dump UART fifo data
	UARTHUB_DEBUG_GET_DEBUG_FIFO_32_BYTE_DATA(0xF);
	UARTHUB_DEBUG_PRINT_DEBUG_FIFO_32_BYTE_DATA_ONE_LINE(NULL, 0xF);

	fsm_dbg_sta = UARTHUB_REG_READ(DBG_STATE_ADDR);
	len = uarthub_record_intfhub_fsm_sta_to_buffer(dmp_info_buf, len, fsm_dbg_sta);

	pr_info("%s\n", dmp_info_buf);

	return 0;
}

int uarthub_dump_debug_byte_cnt_info_mt6993(const char *tag)
{
	struct uarthub_uartip_debug_info debug1 = {0};
	struct uarthub_uartip_debug_info debug2 = {0};
	struct uarthub_uartip_debug_info debug3 = {0};
	struct uarthub_uartip_debug_info debug4 = {0};
	struct uarthub_uartip_debug_info debug5 = {0};
	struct uarthub_uartip_debug_info debug6 = {0};
	struct uarthub_uartip_debug_info debug7 = {0};
	struct uarthub_uartip_debug_info debug8 = {0};
	struct uarthub_uartip_debug_info pkt_cnt = {0};
	int cur_tx_pkt_cnt_d0;
	int cur_tx_pkt_cnt_d1;
	int cur_tx_pkt_cnt_d2;
	int cur_rx_pkt_cnt_d0;
	int cur_rx_pkt_cnt_d1;
	int cur_rx_pkt_cnt_d2;
	uint8_t fifo_cur_idx[2][4] = {0};
	uint8_t fifo_cur_data[2][4][32] = {0};
	int dev0_sta = 0, dev1_sta = 0, dev2_sta = 0;
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int len = 0;
	int val = 0, val1 = 0, val2 = 0;
	int ret = 0;
	int debug_monitor_sel = 0;
	int tx_monitor[4] = { 0 };
	int rx_monitor[4] = { 0 };
	int tx_monitor_pointer = 0, rx_monitor_pointer = 0;
	int fsm_dbg_sta = 0;

	val = DBG_CTRL_GET_intfhub_dbg_sel(DBG_CTRL_ADDR);
	len = 0;
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s] IDBG=[0x%x]", ((tag == NULL) ? "null" : tag), val);
	if (ret > 0)
		len += ret;

	val = UARTHUB_REG_READ(STA0_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ISTA0=[0x%x]", val);
	if (ret > 0)
		len += ret;

	pkt_cnt.dev0 = UARTHUB_REG_READ(DEV0_PKT_CNT_ADDR);
	pkt_cnt.dev1 = UARTHUB_REG_READ(DEV1_PKT_CNT_ADDR);
	pkt_cnt.dev2 = UARTHUB_REG_READ(DEV2_PKT_CNT_ADDR);

	cur_tx_pkt_cnt_d0 = ((pkt_cnt.dev0 & REG_FLD_MASK(DEV0_PKT_CNT_FLD_dev0_tx_pkt_cnt)) >>
		REG_FLD_SHIFT(DEV0_PKT_CNT_FLD_dev0_tx_pkt_cnt));
	cur_tx_pkt_cnt_d1 = ((pkt_cnt.dev1 & REG_FLD_MASK(DEV1_PKT_CNT_FLD_dev1_tx_pkt_cnt)) >>
		REG_FLD_SHIFT(DEV1_PKT_CNT_FLD_dev1_tx_pkt_cnt));
	cur_tx_pkt_cnt_d2 = ((pkt_cnt.dev2 & REG_FLD_MASK(DEV2_PKT_CNT_FLD_dev2_tx_pkt_cnt)) >>
		REG_FLD_SHIFT(DEV2_PKT_CNT_FLD_dev2_tx_pkt_cnt));
	cur_rx_pkt_cnt_d0 = ((pkt_cnt.dev0 & REG_FLD_MASK(DEV0_PKT_CNT_FLD_dev0_rx_pkt_cnt)) >>
		REG_FLD_SHIFT(DEV0_PKT_CNT_FLD_dev0_rx_pkt_cnt));
	cur_rx_pkt_cnt_d1 = ((pkt_cnt.dev1 & REG_FLD_MASK(DEV1_PKT_CNT_FLD_dev1_rx_pkt_cnt)) >>
		REG_FLD_SHIFT(DEV1_PKT_CNT_FLD_dev1_rx_pkt_cnt));
	cur_rx_pkt_cnt_d2 = ((pkt_cnt.dev2 & REG_FLD_MASK(DEV2_PKT_CNT_FLD_dev2_rx_pkt_cnt)) >>
		REG_FLD_SHIFT(DEV2_PKT_CNT_FLD_dev2_rx_pkt_cnt));

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",pcnt=[R:%d-%d-%d,T:%d-%d-%d]",
		cur_rx_pkt_cnt_d0, cur_rx_pkt_cnt_d1, cur_rx_pkt_cnt_d2,
		cur_tx_pkt_cnt_d0, cur_tx_pkt_cnt_d1, cur_tx_pkt_cnt_d2);
	if (ret > 0)
		len += ret;

	UARTHUB_DEBUG_READ_DEBUG_REG(dev0, uartip, uartip_id_ap);
	UARTHUB_DEBUG_READ_DEBUG_REG(dev1, uartip, uartip_id_md);
	UARTHUB_DEBUG_READ_DEBUG_REG(dev2, uartip, uartip_id_adsp);
	UARTHUB_DEBUG_READ_DEBUG_REG(cmm, uartip, uartip_id_cmm);

	if (apuart_base_map_mt6993[3] != NULL) {
		UARTHUB_DEBUG_READ_DEBUG_REG(ap, apuart, 3);
	}

	UARTHUB_DEBUG_PRINT_DEBUG_2_REG(debug5, 0xF0, 4, debug6, 0x3, 4, ",bcnt=[R:%d-%d-%d-%d-%d");
	UARTHUB_DEBUG_PRINT_DEBUG_2_REG(debug2, 0xF0, 4, debug3, 0x3, 4, ",T:%d-%d-%d-%d-%d]");
	UARTHUB_DEBUG_PRINT_DEBUG_1_REG(debug7, 0x3F, 0, ",fifo_woffset=[R:%d-%d-%d-%d-%d");
	UARTHUB_DEBUG_PRINT_DEBUG_1_REG(debug4, 0x3F, 0, ",T:%d-%d-%d-%d-%d]");
	UARTHUB_DEBUG_PRINT_DEBUG_WSEND_XOFF_REG(debug1, ",wsend_xoff=[%d-%d-%d-%d-%d]", 0);
	UARTHUB_DEBUG_PRINT_DEBUG_DET_XOFF_REG(debug8, ",det_xoff=[%d-%d-%d-%d-%d]", 0);

#if !(UARTHUB_SUPPORT_FPGA)
	val = uarthub_get_hwccf_univpll_on_info_mt6993();
	if (val >= 0) {
		/* the expect value is 0x1 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UNIVPLL=[%d]", val);
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_uart_src_clk_info_mt6993();
	if (val >= 0) {
		/* the expect value is 0x1 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UART_SRC_CLK=[0x%x(%s)]", val, ((val == 0) ? "26M" : "TOPCK"));
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_uarthub_mux_info_mt6993();
	val1 = uarthub_get_uart_mux_info_mt6993();
	val2 = uarthub_get_adsp_uart_mux_info_mt6993();
	if (val >= 0) {
		/* the expect value is 0x2 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",HUB/UART/ADSP_MUX=[%d(%s)-%d(%s)-%d(%s)]", val,
			((val == 0) ? "26M" : ((val == 1) ? "104M" : "208M")), val1,
			((val1 == 0) ? "26M" : ((val1 == 1) ? "52M" :
				((val1 == 2) ? "104M" : "208M"))), val2,
			((val2 == 0) ? "26M" : ((val2 == 1) ? "104M" : "208M")));
		if (ret > 0)
			len += ret;
	}

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len, ",HUB/UART/ADSP_CLK=[%d-%d-%d]",
		mt_get_fmeter_freq(FM_UARTHUB_B_CK, CKGEN),
		mt_get_fmeter_freq(FM_UART_CK, CKGEN),
		mt_get_fmeter_freq(F_FADSP_UARTHUB_BCLK_CK, CKGEN));
	if (ret > 0)
		len += ret;
#endif

	dev0_sta = UARTHUB_REG_READ(DEV0_STA_ADDR);
	dev1_sta = UARTHUB_REG_READ(DEV1_STA_ADDR);
	dev2_sta = UARTHUB_REG_READ(DEV2_STA_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IDEV_STA=[0x%x-0x%x-0x%x]", dev0_sta, dev1_sta, dev2_sta);
	if (ret > 0)
		len += ret;

	val = UARTHUB_REG_READ(STA0_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ISTA0=[0x%x]", val);
	if (ret > 0)
		len += ret;

	val = DBG_CTRL_GET_intfhub_dbg_sel(DBG_CTRL_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IDBG=[0x%x]", val);
	if (ret > 0)
		len += ret;

	if (uarthub_read_dbg_monitor_mt6993(&debug_monitor_sel, tx_monitor, rx_monitor) == 0) {
		if (debug_monitor_sel == 0x1) {
			tx_monitor_pointer = DEBUG_MODE_CRTL_GET_check_data_mode_tx_monitor_pointer(
				DEBUG_MODE_CRTL_ADDR);
			rx_monitor_pointer = DEBUG_MODE_CRTL_GET_check_data_mode_rx_monitor_pointer(
				DEBUG_MODE_CRTL_ADDR);

			len = uarthub_record_check_data_mode_sta_to_buffer_mt6993(
				dmp_info_buf, len, debug_monitor_sel, tx_monitor, rx_monitor,
				tx_monitor_pointer, rx_monitor_pointer, NULL);
		}
	}

	// Dump UART fifo data
	UARTHUB_DEBUG_GET_DEBUG_FIFO_32_BYTE_DATA(0xF);
	UARTHUB_DEBUG_PRINT_DEBUG_FIFO_32_BYTE_DATA_ONE_LINE(NULL, 0xF);

	fsm_dbg_sta = UARTHUB_REG_READ(DBG_STATE_ADDR);
	len = uarthub_record_intfhub_fsm_sta_to_buffer(dmp_info_buf, len, fsm_dbg_sta);

	pr_info("%s\n", dmp_info_buf);

	return 0;
}

int uarthub_dump_debug_apdma_uart_info_mt6993(const char *tag)
{
	const char *def_tag = "HUB_DBG_APMDA";

	pr_info("[%s][%s] 0=[0x%x],4=[0x%x],8=[0x%x],c=[0x%x],10=[0x%x],14=[0x%x],18=[0x%x]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6993 + 0x00),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6993 + 0x04),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6993 + 0x08),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6993 + 0x0c),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6993 + 0x10),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6993 + 0x14),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6993 + 0x18));

	pr_info("[%s][%s] 1c=[0x%x],20=[0x%x],24=[0x%x],28=[0x%x],2c=[0x%x],30=[0x%x],34=[0x%x]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6993 + 0x1c),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6993 + 0x20),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6993 + 0x24),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6993 + 0x28),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6993 + 0x2c),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6993 + 0x30),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6993 + 0x34));

	pr_info("[%s][%s] 38=[0x%x],3c=[0x%x],40=[0x%x],44=[0x%x],48=[0x%x],4c=[0x%x],50=[0x%x]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6993 + 0x38),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6993 + 0x3c),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6993 + 0x40),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6993 + 0x44),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6993 + 0x48),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6993 + 0x4c),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6993 + 0x50));

	pr_info("[%s][%s] 54=[0x%x],58=[0x%x],5c=[0x%x],60=[0x%x],64=[0x%x]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6993 + 0x54),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6993 + 0x58),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6993 + 0x5c),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6993 + 0x60),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6993 + 0x64));

	return 0;
}

int uarthub_dump_debug_bus_status_info_mt6993(const char *tag)
{
	struct uarthub_uartip_debug_info debug1 = {0};
	struct uarthub_uartip_debug_info debug2 = {0};
	struct uarthub_uartip_debug_info debug3 = {0};
	struct uarthub_uartip_debug_info debug4 = {0};
	struct uarthub_uartip_debug_info debug5 = {0};
	struct uarthub_uartip_debug_info debug6 = {0};
	struct uarthub_uartip_debug_info debug7 = {0};
	struct uarthub_uartip_debug_info debug8 = {0};
	struct uarthub_uartip_debug_info DMA_EN = {0};
	struct uarthub_uartip_debug_info EFR = {0};
	struct uarthub_uartip_debug_info FCR_RD = {0};
	struct uarthub_uartip_debug_info RXTRI_AD = {0};
	struct uarthub_uartip_debug_info LSR = {0};
	struct uarthub_uartip_debug_info pkt_cnt = {0};
	int cur_tx_pkt_cnt_d0;
	int cur_tx_pkt_cnt_d1;
	int cur_tx_pkt_cnt_d2;
	int cur_rx_pkt_cnt_d0;
	int cur_rx_pkt_cnt_d1;
	int cur_rx_pkt_cnt_d2;
	uint8_t fifo_cur_idx[2][4] = {0};
	uint8_t fifo_cur_data[2][4][32] = {0};
	int dev0_sta = 0, dev1_sta = 0, dev2_sta = 0;
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int len = 0;
	int val = 0;
	int ret = 0;
	int debug_monitor_sel = 0;
	int tx_monitor[4] = { 0 };
	int rx_monitor[4] = { 0 };
	int tx_monitor_pointer = 0, rx_monitor_pointer = 0;
	int fsm_dbg_sta = 0;

	val = DBG_CTRL_GET_intfhub_dbg_sel(DBG_CTRL_ADDR);
	len = 0;
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s] IDBG=[0x%x]", ((tag == NULL) ? "null" : tag), val);
	if (ret > 0)
		len += ret;

	val = UARTHUB_REG_READ(STA0_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ISTA0=[0x%x]", val);
	if (ret > 0)
		len += ret;

	pkt_cnt.dev0 = UARTHUB_REG_READ(DEV0_PKT_CNT_ADDR);
	pkt_cnt.dev1 = UARTHUB_REG_READ(DEV1_PKT_CNT_ADDR);
	pkt_cnt.dev2 = UARTHUB_REG_READ(DEV2_PKT_CNT_ADDR);

	cur_tx_pkt_cnt_d0 = ((pkt_cnt.dev0 & REG_FLD_MASK(DEV0_PKT_CNT_FLD_dev0_tx_pkt_cnt)) >>
		REG_FLD_SHIFT(DEV0_PKT_CNT_FLD_dev0_tx_pkt_cnt));
	cur_tx_pkt_cnt_d1 = ((pkt_cnt.dev1 & REG_FLD_MASK(DEV1_PKT_CNT_FLD_dev1_tx_pkt_cnt)) >>
		REG_FLD_SHIFT(DEV1_PKT_CNT_FLD_dev1_tx_pkt_cnt));
	cur_tx_pkt_cnt_d2 = ((pkt_cnt.dev2 & REG_FLD_MASK(DEV2_PKT_CNT_FLD_dev2_tx_pkt_cnt)) >>
		REG_FLD_SHIFT(DEV2_PKT_CNT_FLD_dev2_tx_pkt_cnt));
	cur_rx_pkt_cnt_d0 = ((pkt_cnt.dev0 & REG_FLD_MASK(DEV0_PKT_CNT_FLD_dev0_rx_pkt_cnt)) >>
		REG_FLD_SHIFT(DEV0_PKT_CNT_FLD_dev0_rx_pkt_cnt));
	cur_rx_pkt_cnt_d1 = ((pkt_cnt.dev1 & REG_FLD_MASK(DEV1_PKT_CNT_FLD_dev1_rx_pkt_cnt)) >>
		REG_FLD_SHIFT(DEV1_PKT_CNT_FLD_dev1_rx_pkt_cnt));
	cur_rx_pkt_cnt_d2 = ((pkt_cnt.dev2 & REG_FLD_MASK(DEV2_PKT_CNT_FLD_dev2_rx_pkt_cnt)) >>
		REG_FLD_SHIFT(DEV2_PKT_CNT_FLD_dev2_rx_pkt_cnt));

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",pcnt=[R:%d-%d-%d,T:%d-%d-%d]",
		cur_rx_pkt_cnt_d0, cur_rx_pkt_cnt_d1, cur_rx_pkt_cnt_d2,
		cur_tx_pkt_cnt_d0, cur_tx_pkt_cnt_d1, cur_tx_pkt_cnt_d2);
	if (ret > 0)
		len += ret;

	UARTHUB_DEBUG_READ_DEBUG_REG(dev0, uartip, uartip_id_ap);
	UARTHUB_DEBUG_READ_DEBUG_REG(dev1, uartip, uartip_id_md);
	UARTHUB_DEBUG_READ_DEBUG_REG(dev2, uartip, uartip_id_adsp);
	UARTHUB_DEBUG_READ_DEBUG_REG(cmm, uartip, uartip_id_cmm);

	if (apuart_base_map_mt6993[3] != NULL) {
		UARTHUB_DEBUG_READ_DEBUG_REG(ap, apuart, 3);
	}

	UARTHUB_DEBUG_READ_CODA_ID_REG(DMA_EN);
	UARTHUB_DEBUG_READ_CODA_ID_REG(EFR);
	UARTHUB_DEBUG_READ_CODA_ID_REG(FCR_RD);
	UARTHUB_DEBUG_READ_CODA_ID_REG(RXTRI_AD);
	UARTHUB_DEBUG_READ_CODA_ID_REG(LSR);

	UARTHUB_DEBUG_PRINT_DEBUG_2_REG(debug5, 0xF0, 4, debug6, 0x3, 4, ",bcnt=[R:%d-%d-%d-%d-%d");
	UARTHUB_DEBUG_PRINT_DEBUG_2_REG(debug2, 0xF0, 4, debug3, 0x3, 4, ",T:%d-%d-%d-%d-%d]");
	UARTHUB_DEBUG_PRINT_DEBUG_1_REG(debug7, 0x3F, 0, ",fifo_woffset=[R:%d-%d-%d-%d-%d");
	UARTHUB_DEBUG_PRINT_DEBUG_1_REG(debug4, 0x3F, 0, ",T:%d-%d-%d-%d-%d]");
	UARTHUB_DEBUG_PRINT_DEBUG_WSEND_XOFF_REG(debug1, ",wsend_xoff=[%d-%d-%d-%d-%d]", 0);
	UARTHUB_DEBUG_PRINT_DEBUG_DET_XOFF_REG(debug8, ",det_xoff=[%d-%d-%d-%d-%d]", 0);

	UARTHUB_DEBUG_PRINT_DEBUG_1_REG(DMA_EN, BIT_0xFFFF_FFFF, 0, ",DMA_EN(0x4c)=[0x%x-0x%x-0x%x-0x%x-0x%x]");
	UARTHUB_DEBUG_PRINT_DEBUG_1_REG(EFR, BIT_0xFFFF_FFFF, 0, ",EFR(0x98)=[0x%x-0x%x-0x%x-0x%x-0x%x]");
	UARTHUB_DEBUG_PRINT_DEBUG_1_REG(FCR_RD, BIT_0xFFFF_FFFF, 0, ",FCR_RD(0x5c)=[0x%x-0x%x-0x%x-0x%x-0x%x]");
	UARTHUB_DEBUG_PRINT_DEBUG_1_REG(RXTRI_AD, BIT_0xFFFF_FFFF, 0, ",RXTRI_AD(0x50)=[0x%x-0x%x-0x%x-0x%x-0x%x]");
	UARTHUB_DEBUG_PRINT_DEBUG_LSR_REG(LSR, ",LSR(0x14)=[0x%x-0x%x-0x%x-0x%x-0x%x]", 1);

	dev0_sta = UARTHUB_REG_READ(DEV0_STA_ADDR);
	dev1_sta = UARTHUB_REG_READ(DEV1_STA_ADDR);
	dev2_sta = UARTHUB_REG_READ(DEV2_STA_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IDEV_STA=[0x%x-0x%x-0x%x]", dev0_sta, dev1_sta, dev2_sta);
	if (ret > 0)
		len += ret;

	if (uarthub_read_dbg_monitor_mt6993(&debug_monitor_sel, tx_monitor, rx_monitor) == 0) {
		if (debug_monitor_sel == 0x1) {
			tx_monitor_pointer = DEBUG_MODE_CRTL_GET_check_data_mode_tx_monitor_pointer(
				DEBUG_MODE_CRTL_ADDR);
			rx_monitor_pointer = DEBUG_MODE_CRTL_GET_check_data_mode_rx_monitor_pointer(
				DEBUG_MODE_CRTL_ADDR);

			len = uarthub_record_check_data_mode_sta_to_buffer_mt6993(
				dmp_info_buf, len, debug_monitor_sel, tx_monitor, rx_monitor,
				tx_monitor_pointer, rx_monitor_pointer, NULL);
		}
	}

	// Dump UART fifo data
	UARTHUB_DEBUG_GET_DEBUG_FIFO_32_BYTE_DATA(0x7);
	UARTHUB_DEBUG_PRINT_DEBUG_FIFO_32_BYTE_DATA_ONE_LINE(NULL, 0x7);

	fsm_dbg_sta = UARTHUB_REG_READ(DBG_STATE_ADDR);
	len = uarthub_record_intfhub_fsm_sta_to_buffer(dmp_info_buf, len, fsm_dbg_sta);

	pr_info("%s\n", dmp_info_buf);

	return 0;
}

#define UARTHUB_TMP_BUF_SZ  512
char g_buf_mt6993[UARTHUB_TMP_BUF_SZ];

int uarthub_dump_sspm_log_mt6993(const char *tag)
{
	void __iomem *log_addr = NULL;
	int i, n, used;
	uint32_t val, irq_idx = 0, tsk_idx = 0;
	uint32_t v1, v2, v3;
	uint64_t t;
	char *tmp;
	const char *def_tag = "HUB_DBG_SSPM";

	g_buf_mt6993[0] = '\0';
	log_addr = UARTHUB_LOG_IRQ_IDX_ADDR(sys_sram_remap_addr_mt6993);
	irq_idx = UARTHUB_REG_READ(log_addr);
	log_addr += 4;

	tmp = g_buf_mt6993;
	used = 0;
	for (i = 0; i < UARTHUB_IRQ_OP_LOG_SIZE; i++) {
		t = UARTHUB_REG_READ(log_addr);
		t = t << 32 | UARTHUB_REG_READ(log_addr + 4);
		val = UARTHUB_REG_READ(log_addr + 8);
		n = snprintf(tmp + used, UARTHUB_TMP_BUF_SZ - used, "[%llu:%X(%s)]",
			t, val,
			((val == 0) ? "RESTORE": ((val == 1) ?
				"CKOFF" : ((val == 2) ? "CKON" : "UNKNOWN"))));
		if (n > 0)
			used += n;
		log_addr += UARTHUB_LOG_IRQ_PKT_SIZE;
	}
	pr_info("[%s][%s] [%x] %s",
		def_tag, ((tag == NULL) ? "null" : tag), irq_idx, g_buf_mt6993);

	log_addr = UARTHUB_LOG_TSK_IDX_ADDR(sys_sram_remap_addr_mt6993);
	tsk_idx = UARTHUB_REG_READ(log_addr);
	log_addr += 4;
	g_buf_mt6993[0] = '\0';
	tmp = g_buf_mt6993;
	used = 0;
	for (i = 0; i < UARTHUB_TSK_OP_LOG_SIZE; i++) {
		t = UARTHUB_REG_READ(log_addr);
		t = t << 32 | UARTHUB_REG_READ(log_addr + 4);
		n = snprintf(tmp + used, UARTHUB_TMP_BUF_SZ - used, "[%llu:%x-%x-%x]",
							t,
							UARTHUB_REG_READ(log_addr + 8),
							UARTHUB_REG_READ(log_addr + 12),
							UARTHUB_REG_READ(log_addr + 16));
		if (n > 0) {
			used += n;
			if ((i % (UARTHUB_TSK_OP_LOG_SIZE/2))
					== ((UARTHUB_TSK_OP_LOG_SIZE/2) - 1)) {
				pr_info("[%s][%s] [%x] %s",
					def_tag, ((tag == NULL) ? "null" : tag),
					tsk_idx, g_buf_mt6993);
				g_buf_mt6993[0] = '\0';
				tmp = g_buf_mt6993;
				used = 0;
			}
		}
		log_addr += UARTHUB_LOG_TSK_PKT_SIZE;
	}

	log_addr = UARTHUB_CK_CNT_ADDR(sys_sram_remap_addr_mt6993);
	val = UARTHUB_REG_READ(log_addr);

	log_addr = UARTHUB_LAST_CK_ON(sys_sram_remap_addr_mt6993);
	v1 = UARTHUB_REG_READ(log_addr);
	v2 = UARTHUB_REG_READ(log_addr + 4);

	log_addr = UARTHUB_LAST_CK_ON_CNT(sys_sram_remap_addr_mt6993);
	v3 = UARTHUB_REG_READ(log_addr);

	pr_info("[%s][%s] off/on cnt=[%d][%d] ckon=[%x][%x] cnt=[%x]",
		def_tag, ((tag == NULL) ? "null" : tag),
		(val & 0xFFFF), (val >> 16),
		v1, v2, v3);

	return 0;
}

int uarthub_trigger_fpga_testing_mt6993(int type)
{
#if UARTHUB_SUPPORT_FPGA
	pr_info("[%s] FPGA type=[%d]\n", __func__, type);
#else
	pr_info("[%s] NOT support FPGA\n", __func__);
#endif
	return 0;
}
/* Entry : dvt it test */
int uarthub_trigger_dvt_it_testing_mt6993(int type)
{
#if UARTHUB_SUPPORT_DVT
	int state = 0;
	pr_info("[%s] IT DVT type=[%d]\n", __func__, type);

	if (type < 0 || type >= sizeof(dvt_it_funcs_name)) {
		pr_info("[%s] Unkonwn type\n", __func__);
		return 0;
	}
	state = dvt_it_funcs[type]();
	pr_info("[IT DVT_%d] %s: RESULT=[%s], state=[%d]\n",
		type, dvt_it_funcs_name[type], ((state) ? "FAIL" : "PASS"), state);

#else
	pr_info("[%s] NOT support DVT\n", __func__);
#endif
	return 0;
}

/* Entry : dvt ut test */
int uarthub_trigger_dvt_ut_testing_mt6993(int type)
{
#if UARTHUB_SUPPORT_DVT
	int state = 0;

	pr_info("[%s] UT DVT type=[%d]\n", __func__, type);

	if (type < 0 || type >= sizeof(dvt_ut_funcs_name)) {
		pr_info("[%s] Unkonwn type\n", __func__);
		return 0;
	}
	state = dvt_ut_funcs[type]();
	pr_info("[UT DVT_%d] %s: RESULT=[%s], state=[%d]\n",
		type, dvt_ut_funcs_name[type], ((state) ? "FAIL" : "PASS"), state);

#else
	pr_info("[%s] NOT support DVT\n", __func__);
#endif
	return 0;
}

int uarthub_emiisu_record_off_mt6993(void)
{
#if !IS_ENABLED(CONFIG_MTK_EMI_LEGACY)
	mtk_emiisu_record_off();
	pr_info("[%s] Turn off EMIISU\n", __func__);
#endif
	return 0;
}

int uarthub_start_sample_baud_rate_mt6993(int dev_index)
{
	if (dev_index < 0 || dev_index > UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	DEBUG_MODE_CRTL_SET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR, 1);
	DBG_CTRL_SET_sample_baud_en(DBG_CTRL_ADDR, 1);
	DBG_CTRL_SET_sample_baud_sel(DBG_CTRL_ADDR, dev_index);
	DBG_CTRL_SET_sample_baud_clr(DBG_CTRL_ADDR, 1);

	return 0;
}

int uarthub_stop_sample_baud_rate_mt6993(void)
{
	int cg_en = 0;
	int sample_baud_en = 0;

	cg_en = DEBUG_MODE_CRTL_GET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR);
	sample_baud_en = DBG_CTRL_GET_sample_baud_en(DBG_CTRL_ADDR);

	if (cg_en == 0 || sample_baud_en == 0) {
		pr_notice("[%s] failed. cg_en = %d, sample_baud_en = %d\n",
			__func__, cg_en, sample_baud_en);
		return -1;
	}

	pr_info("[%s] sample_baud_sel = %d, sample_baud_count = %d\n", __func__,
		DBG_CTRL_GET_sample_baud_sel(DBG_CTRL_ADDR),
		DBG_CTRL_GET_sample_baud_count(DBG_CTRL_ADDR));

	DBG_CTRL_SET_sample_baud_en(DBG_CTRL_ADDR, 0);
	DEBUG_MODE_CRTL_SET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR, 0);

	return 0;
}

int uarthub_record_check_data_mode_sta_to_buffer_mt6993(
	unsigned char *dmp_info_buf, int len,
	int debug_monitor_sel,
	int *tx_monitor, int *rx_monitor,
	int tx_monitor_pointer, int rx_monitor_pointer, const char *tag)
{
	int ret = 0;

	if (debug_monitor_sel == 0x1) {
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",%s=[R(%d):", ((tag == NULL) ? "dataMon" : tag), rx_monitor_pointer);
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X",
			UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA_WITH_SEPARATE(rx_monitor[0], 0, rx_monitor_pointer),
			UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA_WITH_SEPARATE(rx_monitor[1], 1, rx_monitor_pointer),
			UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA_WITH_SEPARATE(rx_monitor[2], 2, rx_monitor_pointer),
			UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA_WITH_SEPARATE(rx_monitor[3], 3, rx_monitor_pointer));
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",T(%d):", tx_monitor_pointer);
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X]",
			UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA_WITH_SEPARATE(tx_monitor[0], 0, tx_monitor_pointer),
			UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA_WITH_SEPARATE(tx_monitor[1], 1, tx_monitor_pointer),
			UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA_WITH_SEPARATE(tx_monitor[2], 2, tx_monitor_pointer),
			UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA_WITH_SEPARATE(tx_monitor[3], 3, tx_monitor_pointer));
		if (ret > 0)
			len += ret;
	}

	return len;
}

int uarthub_record_uart_fifo_sta_to_buffer_mt6993(
	unsigned char *dmp_info_buf, int len, const char *tag, unsigned char type,
	uint8_t fifo_cur_t0, uint8_t fifo_cur_r0,
	uint8_t fifo_cur_t2, uint8_t fifo_cur_r2,
	uint8_t fifo_cur_tcmm, uint8_t fifo_cur_rcmm,
	uint8_t *fifo_data_t0, uint8_t *fifo_data_r0,
	uint8_t *fifo_data_t2, uint8_t *fifo_data_r2,
	uint8_t *fifo_data_tcmm, uint8_t *fifo_data_rcmm)
{
	int ret = 0;
	int is_first = 1;

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",%s=[", ((tag == NULL) ? "UART_FIFO" : tag));
	if (ret > 0)
		len += ret;

	if (type & 0x1) {
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"R0(%u):%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X",
			fifo_cur_r0,
			UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES_WITH_SEPARATE(fifo_data_r0, 0, fifo_cur_r0),
			UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES_WITH_SEPARATE(fifo_data_r0, 8, fifo_cur_r0),
			UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES_WITH_SEPARATE(fifo_data_r0, 16, fifo_cur_r0),
			UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES_WITH_SEPARATE(fifo_data_r0, 24, fifo_cur_r0));
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",T0(%u):%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X",
			fifo_cur_t0,
			UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES_WITH_SEPARATE(fifo_data_t0, 0, fifo_cur_t0),
			UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES_WITH_SEPARATE(fifo_data_t0, 8, fifo_cur_t0),
			UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES_WITH_SEPARATE(fifo_data_t0, 16, fifo_cur_t0),
			UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES_WITH_SEPARATE(fifo_data_t0, 24, fifo_cur_t0));
		if (ret > 0)
			len += ret;

		is_first = 0;
	}

	if (type & 0x4) {
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"%sR2(%u):%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X",
			((is_first == 0) ? "," : ""),
			fifo_cur_r2,
			UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES_WITH_SEPARATE(fifo_data_r2, 0, fifo_cur_r2),
			UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES_WITH_SEPARATE(fifo_data_r2, 8, fifo_cur_r2),
			UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES_WITH_SEPARATE(fifo_data_r2, 16, fifo_cur_r2),
			UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES_WITH_SEPARATE(fifo_data_r2, 24, fifo_cur_r2));
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",T2(%u):%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X",
			fifo_cur_t2,
			UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES_WITH_SEPARATE(fifo_data_t2, 0, fifo_cur_t2),
			UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES_WITH_SEPARATE(fifo_data_t2, 8, fifo_cur_t2),
			UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES_WITH_SEPARATE(fifo_data_t2, 16, fifo_cur_t2),
			UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES_WITH_SEPARATE(fifo_data_t2, 24, fifo_cur_t2));
		if (ret > 0)
			len += ret;

		is_first = 0;
	}

	if (type & 0x8) {
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"%sRcmm(%u):%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X",
			((is_first == 0) ? "," : ""),
			fifo_cur_rcmm,
			UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES_WITH_SEPARATE(fifo_data_rcmm, 0, fifo_cur_rcmm),
			UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES_WITH_SEPARATE(fifo_data_rcmm, 8, fifo_cur_rcmm),
			UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES_WITH_SEPARATE(fifo_data_rcmm, 16, fifo_cur_rcmm),
			UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES_WITH_SEPARATE(fifo_data_rcmm, 24, fifo_cur_rcmm));
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",Tcmm(%u):%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X%s%02X",
			fifo_cur_tcmm,
			UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES_WITH_SEPARATE(fifo_data_tcmm, 0, fifo_cur_tcmm),
			UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES_WITH_SEPARATE(fifo_data_tcmm, 8, fifo_cur_tcmm),
			UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES_WITH_SEPARATE(fifo_data_tcmm, 16, fifo_cur_tcmm),
			UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES_WITH_SEPARATE(fifo_data_tcmm, 24, fifo_cur_tcmm));
		if (ret > 0)
			len += ret;
	}

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len, "]");
	if (ret > 0)
		len += ret;

	return len;
}

int uarthub_record_uart_fifo_sta_to_buffer_by_dev_mt6993(
	unsigned char *dmp_info_buf, int len, uint32_t dev,
	uint8_t fifo_cur_idx_rx, uint8_t fifo_cur_idx_tx,
	uint8_t *fifo_cur_data_rx, uint8_t *fifo_cur_data_tx)
{
	int ret = 0;
	uint32_t separate_pos = 0;

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"UART_%d_FIFO[0:31]=[R(%u):", dev, fifo_cur_idx_rx);
	if (ret > 0)
		len += ret;

	if (fifo_cur_idx_rx != 0)
		separate_pos = len + ((fifo_cur_idx_rx * 3) - 1);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"%s%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
		((fifo_cur_idx_rx == 0) ? "|" : ""),
		UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES(fifo_cur_data_rx, 0),
		UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES(fifo_cur_data_rx, 8),
		UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES(fifo_cur_data_rx, 16),
		UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES(fifo_cur_data_rx, 24));
	if (ret > 0)
		len += ret;

	if (fifo_cur_idx_rx != 0)
		dmp_info_buf[separate_pos] = '|';

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len, ",T(%u):", fifo_cur_idx_tx);
	if (ret > 0)
		len += ret;

	if (fifo_cur_idx_tx != 0)
		separate_pos = len + ((fifo_cur_idx_tx * 3) - 1);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"%s%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X]",
		((fifo_cur_idx_tx == 0) ? "|" : ""),
		UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES(fifo_cur_data_tx, 0),
		UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES(fifo_cur_data_tx, 8),
		UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES(fifo_cur_data_tx, 16),
		UARTHUB_DEBUG_GET_DEBUG_FIFO_16_BYTES(fifo_cur_data_tx, 24));
	if (ret > 0)
		len += ret;

	if (fifo_cur_idx_tx != 0)
		dmp_info_buf[separate_pos] = '|';

	return len;
}

int uarthub_record_packet_info_mode_sta_to_buffer_mt6993(
	unsigned char *dmp_info_buf, int len,
	int debug_monitor_sel,
	int *tx_monitor, int *rx_monitor,
	int tx_monitor_pointer, int rx_monitor_pointer, const char *tag)
{
	int ret = 0;

	if (debug_monitor_sel == 0x0) {
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",%s=[R(%d):%s%s%s%s%s-0x%x-0x%x,%s%s%s%s%s-0x%x-0x%x,%s%s%s%s%s-0x%x-0x%x,%s%s%s%s%s-0x%x-0x%x,T(%d):%s%s%s%s%s-0x%x-0x%x,%s%s%s%s%s-0x%x-0x%x,%s%s%s%s%s-0x%x-0x%x,%s%s%s%s%s-0x%x-0x%x]",
			((tag == NULL) ? "PKT_INFO_MON[0:3]" : tag),
			rx_monitor_pointer,
			UARTHUB_DEBUG_GET_DBG_MONITOR_PKT_INFO(rx_monitor[0]),
			UARTHUB_DEBUG_GET_DBG_MONITOR_PKT_INFO(rx_monitor[1]),
			UARTHUB_DEBUG_GET_DBG_MONITOR_PKT_INFO(rx_monitor[2]),
			UARTHUB_DEBUG_GET_DBG_MONITOR_PKT_INFO(rx_monitor[3]),
			tx_monitor_pointer,
			UARTHUB_DEBUG_GET_DBG_MONITOR_PKT_INFO(tx_monitor[0]),
			UARTHUB_DEBUG_GET_DBG_MONITOR_PKT_INFO(tx_monitor[1]),
			UARTHUB_DEBUG_GET_DBG_MONITOR_PKT_INFO(tx_monitor[2]),
			UARTHUB_DEBUG_GET_DBG_MONITOR_PKT_INFO(tx_monitor[3]));
		if (ret > 0)
			len += ret;
	}

	return len;
}

int uarthub_record_intfhub_fsm_sta_to_buffer(
	unsigned char *dmp_info_buf, int len, int fsm_dbg_sta)
{
	int ret = 0;
	int val = 0;

	val = ((fsm_dbg_sta & REG_FLD_MASK(DBG_STATE_FLD_dbg_intfhub_ctrl_fsm_state)) >>
		REG_FLD_SHIFT(DBG_STATE_FLD_dbg_intfhub_ctrl_fsm_state));

	/* ID:IDLE, PP:PREPARE, RD:READY, CO:CKON, CF:CKOFF, WI:WAIT */
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",fsm=[hub:%s",
		((val == 0) ? "ID" : ((val == 1) ? "PP" :
			((val == 2) ? "RD" : ((val == 3) ? "CO" :
			((val == 4) ? "CF" : "WI"))))));
	if (ret > 0)
		len += ret;

	val = ((fsm_dbg_sta & REG_FLD_MASK(DBG_STATE_FLD_dbg_tx_fsm_state)) >>
		REG_FLD_SHIFT(DBG_STATE_FLD_dbg_tx_fsm_state));

	/* ID:IDLE, HD:HEADER, PL:PAYLOAD, CC:CRC, ES:ESP, ED:END */
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",tx:%s",
		((val == 0) ? "ID" : ((val == 1) ? "HD" :
			((val == 2) ? "PL" : ((val == 3) ? "CC" :
			((val == 4) ? "ES" : "ED"))))));
	if (ret > 0)
		len += ret;

	val = ((fsm_dbg_sta & REG_FLD_MASK(DBG_STATE_FLD_dbg_rx_fsm_state)) >>
		REG_FLD_SHIFT(DBG_STATE_FLD_dbg_rx_fsm_state));

	/* ID:IDLE, DE:DEC, D0:DEV0, D1:DEV1, D2:DEV2, ED:END */
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",rx:%s",
		((val == 0) ? "ID" : ((val == 1) ? "DE" :
			((val == 2) ? "D0" : ((val == 3) ? "D1" :
			((val == 4) ? "D2" : "ED"))))));
	if (ret > 0)
		len += ret;

	val = ((fsm_dbg_sta & REG_FLD_MASK(DBG_STATE_FLD_dbg_dev0_rx_fsm_state)) >>
		REG_FLD_SHIFT(DBG_STATE_FLD_dbg_dev0_rx_fsm_state));

	/* ID:IDLE, HD:HEADER, PL:PAYLOAD, CC:CRC, ES:ESP, ED:END */
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"-%s",
		((val == 0) ? "ID" : ((val == 1) ? "HD" :
			((val == 2) ? "PL" : ((val == 3) ? "CC" :
			((val == 4) ? "ES" : "ED"))))));
	if (ret > 0)
		len += ret;

	val = ((fsm_dbg_sta & REG_FLD_MASK(DBG_STATE_FLD_dbg_dev1_rx_fsm_state)) >>
		REG_FLD_SHIFT(DBG_STATE_FLD_dbg_dev1_rx_fsm_state));

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"-%s",
		((val == 0) ? "ID" : ((val == 1) ? "HD" :
			((val == 2) ? "PL" : ((val == 3) ? "CC" :
			((val == 4) ? "ES" : "ED"))))));
	if (ret > 0)
		len += ret;

	val = ((fsm_dbg_sta & REG_FLD_MASK(DBG_STATE_FLD_dbg_dev2_rx_fsm_state)) >>
		REG_FLD_SHIFT(DBG_STATE_FLD_dbg_dev2_rx_fsm_state));

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"-%s]",
		((val == 0) ? "ID" : ((val == 1) ? "HD" :
			((val == 2) ? "PL" : ((val == 3) ? "CC" :
			((val == 4) ? "ES" : "ED"))))));
	if (ret > 0)
		len += ret;

	val = ((fsm_dbg_sta & REG_FLD_MASK(DBG_STATE_FLD_intfhub_dbg)) >>
		REG_FLD_SHIFT(DBG_STATE_FLD_intfhub_dbg));

	return len;
}
