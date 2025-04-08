/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef MT6993_DEBUG_KEYWORD_H
#define MT6993_DEBUG_KEYWORD_H

#include "common_def_id.h"
#include "mt6993_debug.h"

#define UARTHUB_DEBUG_PRINT_RX_WOFFSET_DEBUG_KEYWORD(_v0, _v1, _v2, _v3, _v4, _err) \
	do {\
		if (_v0 > 0)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_uarthub, \
				log_uart0_rx_woffset_not_empty, __func__);\
		if (_v1 > 0)\
			BT_OVER_UAER_DUMP_LOG(1, mod_uarthub, mod_uarthub, \
				log_uart1_rx_woffset_not_empty, __func__);\
		if (_v2 > 0)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_uarthub, \
				log_uart2_rx_woffset_not_empty, __func__);\
		if (_v3 > 0)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_uarthub, \
				log_uartcmm_rx_woffset_not_empty, __func__);\
		if ((apuart_base_map_mt6993[3] != NULL) && (_v4 > 0))\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_ap_uart, \
				log_apuart_rx_woffset_not_empty, __func__);\
	} while (0)

#define UARTHUB_DEBUG_PRINT_TX_WOFFSET_DEBUG_KEYWORD(_v0, _v1, _v2, _v3, _v4, _err) \
	do {\
		if (_v0 > 0)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_ap_uart, \
				log_uart0_tx_woffset_not_empty, __func__);\
		if (_v1 > 0)\
			BT_OVER_UAER_DUMP_LOG(1, mod_uarthub, mod_uarthub, \
				log_uart1_tx_woffset_not_empty, __func__);\
		if (_v2 > 0)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_adsp_uart, \
				log_uart2_tx_woffset_not_empty, __func__);\
		if (_v3 > 0)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_bt_uart, \
				log_uartcmm_tx_woffset_not_empty, __func__);\
		if ((apuart_base_map_mt6993[3] != NULL) && (_v4 > 0))\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_uarthub, \
				log_apuart_tx_woffset_not_empty, __func__);\
	} while (0)

#define UARTHUB_DEBUG_PRINT_FRAME_ERROR_DEBUG_KEYWORD(_v0, _v1, _v2, _v3, _v4, _err) \
	do {\
		if (((_v0 & 0x8) >> 3) == 1)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_ap_uart, \
				log_uart0_frame_error, __func__);\
		if (((_v1 & 0x8) >> 3) == 1)\
			BT_OVER_UAER_DUMP_LOG(1, mod_uarthub, mod_uarthub, \
				log_uart1_frame_error, __func__);\
		if (((_v2 & 0x8) >> 3) == 1)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_adsp_uart, \
				log_uart2_frame_error, __func__);\
		if (((_v3& 0x8) >> 3) == 1)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_bt_uart, \
				log_uartcmm_frame_error, __func__);\
		if ((apuart_base_map_mt6993[3] != NULL) && (((_v4 & 0x8) >> 3) == 1))\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_uarthub, \
				log_apuart_frame_error, __func__);\
	} while (0)

#define UARTHUB_DEBUG_PRINT_DET_XOFF_DEBUG_KEYWORD(_v0, _v1, _v2, _v3, _v4, _err) \
	do {\
		if (_v0 == 1)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_ap_uart, \
				log_uart0_det_xoff, __func__);\
		if (_v1 == 1)\
			BT_OVER_UAER_DUMP_LOG(1, mod_uarthub, mod_uarthub, \
				log_uart1_det_xoff, __func__);\
		if (_v2 == 1)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_adsp_uart, \
				log_uart2_det_xoff, __func__);\
		if (_v3 == 1)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_bt_uart, \
				log_uartcmm_det_xoff, __func__);\
		if ((apuart_base_map_mt6993[3] != NULL) && (_v4 == 1))\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_uarthub, \
				log_apuart_det_xoff, __func__);\
	} while (0)

#define UARTHUB_DEBUG_PRINT_WSEND_XOFF_DEBUG_KEYWORD(_v0, _v1, _v2, _v3, _v4, _err) \
	do {\
		if (_v0 == 2 || _v0 == 4 || _v0 == 5)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_uarthub, ((_v0 == 2) ? \
				log_uart0_keep_sending_xoff : ((_v0 == 4) ? log_uart0_send_xoff : \
			log_uart0_keep_sending_xon)), __func__);\
		if (_v1 == 2 || _v1 == 4 || _v1 == 5)\
			BT_OVER_UAER_DUMP_LOG(1, mod_uarthub, mod_uarthub, ((_v1 == 2) ? \
				log_uart1_keep_sending_xoff : ((_v1 == 4) ? log_uart1_send_xoff : \
			log_uart1_keep_sending_xon)), __func__);\
		if (_v2 == 2 || _v2 == 4 || _v2 == 5)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_uarthub, ((_v2 == 2) ? \
				log_uart2_keep_sending_xoff : ((_v2 == 4) ? log_uart2_send_xoff : \
			log_uart2_keep_sending_xon)), __func__);\
		if (_v3 == 2 || _v3 == 4 || _v3 == 5)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_uarthub, ((_v3 == 2) ? \
				log_uartcmm_keep_sending_xoff : ((_v3 == 4) ? log_uartcmm_send_xoff : \
			log_uartcmm_keep_sending_xon)), __func__);\
		if ((apuart_base_map_mt6993[3] != NULL) && (_v4 == 2 || _v4 == 4 || _v4 == 5))\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_ap_uart, ((_v4 == 2) ? \
				log_apuart_keep_sending_xoff : ((_v4 == 4) ? log_apuart_send_xoff : \
			log_apuart_keep_sending_xon)), __func__);\
	} while (0)

#define UARTHUB_DEBUG_PRINT_GPIO_DEBUG_KEYWORD(_rm, _tm, _bm, _bd, _bo, _err) \
	do {\
		if (_rm.gpio_value != _rm.value)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_bt_drv, \
				log_gpio_rx_mode_err, __func__);\
		if (_tm.gpio_value != _tm.value)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_bt_drv, \
				log_gpio_tx_mode_err, __func__);\
		if (_bm.gpio_value != 0)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_bt_drv, \
				log_gpio_bt_rst_mode_err, __func__);\
		if (_bd.gpio_value != 1)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_bt_drv, \
				log_gpio_bt_rst_dir_err, __func__);\
		if (_bo.gpio_value != 1)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_bt_drv, \
				log_gpio_bt_rst_out_err, __func__);\
	} while (0)

#define UARTHUB_DEBUG_PRINT_AP_TX_CMD_TMO_PKT_CNT_ERR_DEBUG_KEYWORD(_ft, _lt, _fr, _lr, _err) \
	do {\
		if (_ft == _lt)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_ap_uart, \
				log_ap_tx_cmd_tmo_tx_pkt_cnt_err, __func__);\
		else if (_fr == _lr)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_bt_uart, \
				log_ap_tx_cmd_tmo_rx_pkt_cnt_err, __func__);\
	} while (0)

#define UARTHUB_DEBUG_PRINT_AP_TX_CMD_TMO_BYTE_CNT_ERR_DEBUG_KEYWORD(\
_fat, _lat, _far, _lar, _fct, _lct, _fcr, _lcr, _err) \
	do {\
		if (apuart_base_map_mt6993[3] != NULL) {\
			if (_fat == _lat)\
				BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_ap_uart, \
					log_ap_tx_cmd_tmo_apuart_tx_byte_cnt_err, __func__);\
			else if (_fct == _lct)\
				BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_uarthub, \
					log_ap_tx_cmd_tmo_uartcmm_tx_byte_cnt_err, __func__);\
			else if (_fcr == _lcr)\
				BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_bt_uart, \
					log_ap_tx_cmd_tmo_uartcmm_rx_byte_cnt_err, __func__);\
			else if (_far == _lar)\
				BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_uarthub, \
					log_ap_tx_cmd_tmo_apuart_rx_byte_cnt_err, __func__);\
			else\
				BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_ap_dma, \
					log_ap_tx_cmd_tmo_apdma_err, __func__);\
		} else {\
			if (_fct != _lct && _fcr == _lcr)\
				BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_bt_uart, \
					log_ap_tx_cmd_tmo_uartcmm_rx_byte_cnt_err, __func__);\
		}\
	} while (0)

#define UARTHUB_DEBUG_PRINT_UARTHUB_IRQ_ERR_DEBUG_KEYWORD(_sta, _err) \
	do {\
		if (((_sta >> 0) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_bt_uart, \
				log_dev0_crc_err, __func__);\
		if (((_sta >> 1) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_bt_uart, \
				log_dev1_crc_err, __func__);\
		if (((_sta >> 2) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_bt_uart, \
				log_dev2_crc_err, __func__);\
		if (((_sta >> 3) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_undefined, \
				log_dev0_tx_timeout_err, __func__);\
		if (((_sta >> 4) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_uarthub, \
				log_dev1_tx_timeout_err, __func__);\
		if (((_sta >> 5) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_undefined, \
				log_dev2_tx_timeout_err, __func__);\
		if (((_sta >> 6) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_ap_uart, \
				log_dev0_tx_pkt_type_err, __func__);\
		if (((_sta >> 7) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_uarthub, \
				log_dev1_tx_pkt_type_err, __func__);\
		if (((_sta >> 8) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_adsp_uart, \
				log_dev2_tx_pkt_type_err, __func__);\
		if (((_sta >> 9) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_bt_uart, \
				log_dev0_rx_timeout_err, __func__);\
		if (((_sta >> 10) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_bt_uart, \
				log_dev1_rx_timeout_err, __func__);\
		if (((_sta >> 11) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_bt_uart, \
				log_dev2_rx_timeout_err, __func__);\
		if (((_sta >> 12) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_bt_uart, \
				log_rx_pkt_type_err, __func__);\
		if (((_sta >> 14) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_bt_uart, \
				log_dev_rx_err, __func__);\
		if (((_sta >> 15) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_bt_drv, \
				log_dev0_tx_err, __func__);\
		if (((_sta >> 16) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_uarthub, \
				log_dev1_tx_err, __func__);\
		if (((_sta >> 17) & 0x1) == 0x1)\
			BT_OVER_UAER_DUMP_LOG(_err, mod_uarthub, mod_adsp_host, \
				log_dev2_tx_err, __func__);\
	} while (0)

#define UARTHUB_DEBUG_PRINT_DEBUG_LSR_REG(_v1, _str, _err) \
	do {\
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len, \
			_str, _v1.dev0, _v1.dev1, _v1.dev2, _v1.cmm, _v1.ap);\
		UARTHUB_DEBUG_PRINT_FRAME_ERROR_DEBUG_KEYWORD(\
			_v1.dev0, _v1.dev1, _v1.dev2, _v1.cmm, _v1.ap, _err);\
	} while (0)

#define UARTHUB_DEBUG_PRINT_DEBUG_WSEND_XOFF_REG(_v1, _str, _err) \
	do {\
		int _d0, _d1, _d2, _cmm, _ap;\
		_d0 = ((_v1.dev0 & 0xE0) >> 5);\
		_d1 = ((_v1.dev1 & 0xE0) >> 5);\
		_d2 = ((_v1.dev2 & 0xE0) >> 5);\
		_cmm = ((_v1.cmm & 0xE0) >> 5);\
		_ap = ((_v1.ap & 0xE0) >> 5);\
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len, \
			_str, _d0, _d1, _d2, _cmm, _ap);\
		UARTHUB_DEBUG_PRINT_WSEND_XOFF_DEBUG_KEYWORD(\
			_d0, _d1, _d2, _cmm, _ap, _err);\
	} while (0)

#define UARTHUB_DEBUG_PRINT_DEBUG_DET_XOFF_REG(_v1, _str, _err) \
	do {\
		int _d0, _d1, _d2, _cmm, _ap;\
		_d0 = ((_v1.dev0 & 0x8) >> 3);\
		_d1 = ((_v1.dev1 & 0x8) >> 3);\
		_d2 = ((_v1.dev2 & 0x8) >> 3);\
		_cmm = ((_v1.cmm & 0x8) >> 3);\
		_ap = ((_v1.ap & 0x8) >> 3);\
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len, \
			_str, _d0, _d1, _d2, _cmm, _ap);\
		UARTHUB_DEBUG_PRINT_DET_XOFF_DEBUG_KEYWORD(\
			_d0, _d1, _d2, _cmm, _ap, _err);\
	} while (0)

#define UARTHUB_DEBUG_PRINT_DEBUG_RX_WOFFSET_REG(_v1, _str, _err) \
	do {\
		int _d0, _d1, _d2, _cmm, _ap;\
		_d0 = (_v1.dev0 & 0x3F);\
		_d1 = (_v1.dev1 & 0x3F);\
		_d2 = (_v1.dev2 & 0x3F);\
		_cmm = (_v1.cmm & 0x3F);\
		_ap = (_v1.ap & 0x3F);\
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len, \
			_str, _d0, _d1, _d2, _cmm, _ap);\
		UARTHUB_DEBUG_PRINT_RX_WOFFSET_DEBUG_KEYWORD(\
			_d0, _d1, _d2, _cmm, _ap, _err);\
	} while (0)

#define UARTHUB_DEBUG_PRINT_DEBUG_TX_WOFFSET_REG(_v1, _str, _err) \
	do {\
		int _d0, _d1, _d2, _cmm, _ap;\
		_d0 = (_v1.dev0 & 0x3F);\
		_d1 = (_v1.dev1 & 0x3F);\
		_d2 = (_v1.dev2 & 0x3F);\
		_cmm = (_v1.cmm & 0x3F);\
		_ap = (_v1.ap & 0x3F);\
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len, \
			_str, _d0, _d1, _d2, _cmm, _ap);\
		UARTHUB_DEBUG_PRINT_TX_WOFFSET_DEBUG_KEYWORD(\
			_d0, _d1, _d2, _cmm, _ap, _err);\
	} while (0)

#define UARTHUB_DEBUG_INIT_AP_TX_CMD_TMO_PKT_CNT_ERR(_prefix, _cur) \
	do {\
		if (_cur == 0) {\
			_prefix##_cur_tx_pkt_cnt_d0 = -1;\
			_prefix##_cur_rx_pkt_cnt_d0 = -1;\
		} else {\
			_prefix##_cur_tx_pkt_cnt_d0 = cur_tx_pkt_cnt_d0;\
			_prefix##_cur_rx_pkt_cnt_d0 = cur_rx_pkt_cnt_d0;\
		}\
	} while (0)

#define UARTHUB_DEBUG_INIT_AP_TX_CMD_TMO_BYTE_CNT_ERR(_prefix, _cur) \
	do {\
		if (_cur == 0) {\
			_prefix##_ap_tx_bcnt = -1;\
			_prefix##_ap_rx_bcnt = -1;\
			_prefix##_cmm_tx_bcnt = -1;\
			_prefix##_cmm_rx_bcnt = -1;\
		} else {\
			_prefix##_ap_tx_bcnt = ap_tx_bcnt;\
			_prefix##_ap_rx_bcnt = ap_rx_bcnt;\
			_prefix##_cmm_tx_bcnt = cmm_tx_bcnt;\
			_prefix##_cmm_rx_bcnt = cmm_rx_bcnt;\
		}\
	} while (0)

#endif
