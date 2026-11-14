// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/module.h>

#include <mbraink_ioctl_struct_def.h>
#include <mbraink_modules_ops_def.h>
#include <bridge/mbraink_bridge_wifi.h>
#include "mbraink_v6993_wifi.h"

#define MAX_WIFI_DATA_CNT 8192

void mbraink_v6993_get_wifi_rate_data(int current_idx,
				struct mbraink_wifi2mbr_lls_rate_data *rate_buffer)
{
	unsigned short len = 0;
	enum wifi2mbr_status ret = WIFI2MBR_FAILURE;
	struct wifi2mbr_llsRateInfo lls_rate;
	int loop = 0;
	int cnt = 0;

	memset(rate_buffer, 0, sizeof(struct mbraink_wifi2mbr_lls_rate_data));

	do {
		ret = mbraink_bridge_wifi_get_data(MBR2WIFI_TRX_BIG_DATA,
						WIFI2MBR_TAG_LLS_RATE,
						(void *)(&lls_rate), &len);
		loop++;

		if (ret == WIFI2MBR_NO_OPS)
			break;
		else if (ret == WIFI2MBR_FAILURE)
			continue;
		else if (ret == WIFI2MBR_SUCCESS) {
			cnt = rate_buffer->count;

			if (cnt < MAX_WIFI_RATE_SZ) {
				rate_buffer->rate_data[cnt].timestamp = lls_rate.timestamp;
				rate_buffer->rate_data[cnt].rate_idx = lls_rate.rate_idx;
				rate_buffer->rate_data[cnt].bitrate = lls_rate.bitrate;
				rate_buffer->rate_data[cnt].tx_mpdu = lls_rate.tx_mpdu;
				rate_buffer->rate_data[cnt].rx_mpdu = lls_rate.rx_mpdu;
				rate_buffer->rate_data[cnt].mpdu_lost = lls_rate.mpdu_lost;
				rate_buffer->rate_data[cnt].retries = lls_rate.retries;
				rate_buffer->count++;

				if (cnt == MAX_WIFI_RATE_SZ - 1) {
					rate_buffer->idx = current_idx + rate_buffer->count;
					break;
				}
			}
		} else if (ret ==  WIFI2MBR_END) {
			rate_buffer->idx = 0;
			break;
		}
	} while (loop < MAX_WIFI_DATA_CNT);

	if (loop == MAX_WIFI_DATA_CNT)
		pr_info("%s: loop cnt is MAX_WIFI_DATA_CNT\n", __func__);
}

void mbraink_v6993_get_wifi_radio_data(struct mbraink_wifi2mbr_lls_radio_data *radio_buffer)
{
	unsigned short len = 0;
	enum wifi2mbr_status ret = WIFI2MBR_FAILURE;
	struct wifi2mbr_llsRadioInfo lls_radio;
	int loop = 0;
	int cnt = 0;

	memset(radio_buffer, 0, sizeof(struct mbraink_wifi2mbr_lls_radio_data));

	do {
		ret = mbraink_bridge_wifi_get_data(MBR2WIFI_TRX_BIG_DATA,
						WIFI2MBR_TAG_LLS_RADIO,
						(void *)(&lls_radio), &len);
		loop++;

		if (ret == WIFI2MBR_NO_OPS)
			break;
		else if (ret == WIFI2MBR_FAILURE)
			continue;
		else if (ret == WIFI2MBR_SUCCESS) {
			cnt = radio_buffer->count;

			if (cnt < MAX_WIFI_RADIO_SZ) {
				radio_buffer->radio_data[cnt].timestamp = lls_radio.timestamp;
				radio_buffer->radio_data[cnt].radio = lls_radio.radio;
				radio_buffer->radio_data[cnt].on_time = lls_radio.on_time;
				radio_buffer->radio_data[cnt].tx_time = lls_radio.tx_time;
				radio_buffer->radio_data[cnt].rx_time = lls_radio.rx_time;
				radio_buffer->radio_data[cnt].on_time_scan = lls_radio.on_time_scan;
				radio_buffer->radio_data[cnt].on_time_roam_scan =
					lls_radio.on_time_roam_scan;
				radio_buffer->radio_data[cnt].on_time_pno_scan =
					lls_radio.on_time_pno_scan;
				radio_buffer->count++;
			} else {
				pr_info("%s: index is invalid\n", __func__);
				break;
			}
		} else if (ret ==  WIFI2MBR_END)
			break;
	} while (loop < MAX_WIFI_DATA_CNT);

	if (loop == MAX_WIFI_DATA_CNT)
		pr_info("%s: loop cnt is MAX_WIFI_DATA_CNT\n", __func__);

	radio_buffer->idx = 0;
}

void mbraink_v6993_get_wifi_ac_data(struct mbraink_wifi2mbr_lls_ac_data *ac_buffer)
{
	unsigned short len = 0;
	enum wifi2mbr_status ret = WIFI2MBR_FAILURE;
	struct wifi2mbr_llsAcInfo lls_ac;
	int loop = 0;
	int cnt = 0;

	memset(ac_buffer, 0, sizeof(struct mbraink_wifi2mbr_lls_ac_data));

	do {
		ret = mbraink_bridge_wifi_get_data(MBR2WIFI_TRX_BIG_DATA,
						WIFI2MBR_TAG_LLS_AC,
						(void *)(&lls_ac), &len);
		loop++;

		if (ret == WIFI2MBR_NO_OPS)
			break;
		else if (ret == WIFI2MBR_FAILURE)
			continue;
		else if (ret == WIFI2MBR_SUCCESS) {
			cnt = ac_buffer->count;

			if (cnt < MBRAINK_MBR_WIFI_AC_MAX) {
				ac_buffer->ac_data[cnt].timestamp = lls_ac.timestamp;
				ac_buffer->ac_data[cnt].ac =
					(enum mbraink_enum_mbr_wifi_ac)(lls_ac.ac);
				ac_buffer->ac_data[cnt].tx_mpdu = lls_ac.tx_mpdu;
				ac_buffer->ac_data[cnt].rx_mpdu = lls_ac.rx_mpdu;
				ac_buffer->ac_data[cnt].tx_mcast = lls_ac.tx_mcast;
				ac_buffer->ac_data[cnt].tx_ampdu = lls_ac.tx_ampdu;
				ac_buffer->ac_data[cnt].mpdu_lost = lls_ac.mpdu_lost;
				ac_buffer->ac_data[cnt].retries = lls_ac.retries;
				ac_buffer->ac_data[cnt].contention_time_min =
					lls_ac.contention_time_min;
				ac_buffer->ac_data[cnt].contention_time_max =
					lls_ac.contention_time_max;
				ac_buffer->ac_data[cnt].contention_time_avg =
					lls_ac.contention_time_avg;
				ac_buffer->ac_data[cnt].contention_num_samples =
					lls_ac.contention_num_samples;
				ac_buffer->count++;
			} else {
				pr_info("%s: index is invalid\n", __func__);
				break;
			}
		} else if (ret ==  WIFI2MBR_END)
			break;
	} while (loop < MAX_WIFI_DATA_CNT);

	if (loop == MAX_WIFI_DATA_CNT)
		pr_info("%s: loop cnt is MAX_WIFI_DATA_CNT\n", __func__);

	ac_buffer->idx = 0;
}

void mbraink_v6993_get_wifi_lp_data(struct mbraink_wifi2mbr_lp_ratio_data *lp_buffer)
{
	unsigned short len = 0;
	enum wifi2mbr_status ret = WIFI2MBR_FAILURE;
	struct wifi2mbr_lpRatioInfo lp_ratio;
	int loop = 0;
	int cnt = 0;

	memset(lp_buffer, 0, sizeof(struct mbraink_wifi2mbr_lp_ratio_data));

	do {
		ret = mbraink_bridge_wifi_get_data(MBR2WIFI_TEST_LP_RATIO,
						WIFI2MBR_TAG_LP_RATIO,
						(void *)(&lp_ratio), &len);
		loop++;

		if (ret == WIFI2MBR_NO_OPS)
			break;
		else if (ret == WIFI2MBR_FAILURE)
			continue;
		else if (ret == WIFI2MBR_SUCCESS) {
			cnt = lp_buffer->count;
			if (cnt < MAX_WIFI_LP_SZ) {
				lp_buffer->lp_data[cnt].timestamp = lp_ratio.timestamp;
				lp_buffer->lp_data[cnt].radio = lp_ratio.radio;
				lp_buffer->lp_data[cnt].total_time = lp_ratio.total_time;
				lp_buffer->lp_data[cnt].tx_time = lp_ratio.tx_time;
				lp_buffer->lp_data[cnt].rx_time = lp_ratio.rx_time;
				lp_buffer->lp_data[cnt].rx_listen_time = lp_ratio.rx_listen_time;
				lp_buffer->lp_data[cnt].sleep_time = lp_ratio.sleep_time;
				lp_buffer->count++;
			} else {
				pr_info("%s: index is invalid\n", __func__);
				break;
			}
		} else if (ret ==  WIFI2MBR_END)
			break;
	} while (loop < MAX_WIFI_DATA_CNT);

	if (loop == MAX_WIFI_DATA_CNT)
		pr_info("%s: loop cnt is MAX_WIFI_DATA_CNT\n", __func__);

	lp_buffer->idx = 0;
}

void mbraink_v6993_get_wifi_txtimeout_data(int current_idx,
				struct mbraink_wifi2mbr_txtimeout_data *txtimeout_buffer)
{
	unsigned short len = 0;
	enum wifi2mbr_status ret = WIFI2MBR_FAILURE;
	struct wifi2mbr_TxTimeoutInfo tx_timeout;
	int loop = 0;
	int cnt = 0;

	memset(txtimeout_buffer, 0, sizeof(struct mbraink_wifi2mbr_txtimeout_data));

	do {
		ret = mbraink_bridge_wifi_get_data(MBR2WIFI_TX_TIMEOUT,
						WIFI2MBR_TAG_TXTIMEOUT,
						(void *)(&tx_timeout), &len);
		loop++;

		if (ret == WIFI2MBR_NO_OPS)
			break;
		else if (ret == WIFI2MBR_FAILURE)
			continue;
		else if (ret == WIFI2MBR_SUCCESS) {
			cnt = txtimeout_buffer->count;

			if (cnt < MAX_WIFI_TXTIMEOUT_SZ) {
				txtimeout_buffer->txtimeout_data[cnt].timestamp =
								tx_timeout.timestamp;
				txtimeout_buffer->txtimeout_data[cnt].token_id =
								tx_timeout.token_id;
				txtimeout_buffer->txtimeout_data[cnt].wlan_index =
								tx_timeout.wlan_index;
				txtimeout_buffer->txtimeout_data[cnt].bss_index =
								tx_timeout.bss_index;
				txtimeout_buffer->txtimeout_data[cnt].timeout_duration =
								tx_timeout.timeout_duration;
				txtimeout_buffer->txtimeout_data[cnt].operation_mode =
								tx_timeout.operation_mode;
				txtimeout_buffer->txtimeout_data[cnt].idle_slot_diff_cnt =
								tx_timeout.idle_slot_diff_cnt;
				txtimeout_buffer->count++;

				if (cnt == MAX_WIFI_TXTIMEOUT_SZ - 1) {
					txtimeout_buffer->idx =
						current_idx + txtimeout_buffer->count;
					break;
				}
			}
		} else if (ret ==  WIFI2MBR_END) {
			txtimeout_buffer->idx = 0;
			break;
		}
	} while (loop < MAX_WIFI_DATA_CNT);

	if (loop == MAX_WIFI_DATA_CNT)
		pr_info("%s: loop cnt is MAX_WIFI_DATA_CNT\n", __func__);
}

void mbraink_v6993_get_wifi_pcie_data(int current_idx,
		struct mbraink_wifi2mbr_pcie_data *pcie_buffer)
{
	unsigned short len = 0;
	enum wifi2mbr_status ret = WIFI2MBR_FAILURE;
	struct wifi2mbr_PcieInfo pcie_info;
	int loop = 0;
	int cnt = 0;

	memset(pcie_buffer, 0, sizeof(struct mbraink_wifi2mbr_pcie_data));

	do {
		ret = mbraink_bridge_wifi_get_data(MBR2WIFI_PCIE_DATA,
						WIFI2MBR_TAG_PCIE,
						(void *)(&pcie_info), &len);
		loop++;

		if (ret == WIFI2MBR_NO_OPS)
			break;
		else if (ret == WIFI2MBR_FAILURE)
			continue;
		else if (ret == WIFI2MBR_SUCCESS) {
			cnt = pcie_buffer->count;

			if (cnt < MAX_WIFI_PCIE_SZ) {
				pcie_buffer->pcie_data[cnt].timestamp =
								pcie_info.timestamp;
				pcie_buffer->pcie_data[cnt].update_time =
					convert_to_utc_usec(pcie_info.update_time_utc_sec,
						pcie_info.update_time_utc_usec);
				pcie_buffer->pcie_data[cnt].req_recovery_count =
								pcie_info.req_recovery_count;
				pcie_buffer->pcie_data[cnt].l0_time =
					convert_to_utc_usec(pcie_info.l0_time_s,
						pcie_info.l0_time_us);
				pcie_buffer->pcie_data[cnt].l1_time =
					convert_to_utc_usec(pcie_info.l1_time_s,
						pcie_info.l1_time_us);
				pcie_buffer->pcie_data[cnt].l1p2_time =
					convert_to_utc_usec(pcie_info.l1ss_time_s,
						pcie_info.l1ss_time_us);
				pcie_buffer->count++;

				if (cnt == MAX_WIFI_PCIE_SZ - 1) {
					pcie_buffer->idx = current_idx + pcie_buffer->count;
					break;
				}
			}
		} else if (ret ==  WIFI2MBR_END) {
			pcie_buffer->idx = 0;
			break;
		}
	} while (loop < MAX_WIFI_DATA_CNT);

	if (loop == MAX_WIFI_DATA_CNT)
		pr_info("%s: loop cnt is MAX_WIFI_DATA_CNT\n", __func__);
}

void copy_txpwr_info(struct mbraink_wifi2mbr_txpwr_info *dest,
		     const struct wifi2mbr_txpwr_info *src)
{
	dest->epa_support = src->epa_support;
	dest->cal_type = src->cal_type;
	dest->center_ch = src->center_ch;
	dest->mcc_idx = src->mcc_idx;
	dest->rf_band = src->rf_band;
	dest->temp = src->temp;
	dest->antsel = src->antsel;

	dest->coex.bt_on = src->coex.bt_on;
	dest->coex.lte_on = src->coex.lte_on;
	memcpy(dest->coex.reserved, src->coex.reserved, sizeof(dest->coex.reserved));
	dest->coex.bt_profile = src->coex.bt_profile;
	dest->coex.pta_grant = src->coex.pta_grant;
	dest->coex.pta_req = src->coex.pta_req;
	dest->coex.curr_op_mode = src->coex.curr_op_mode;

	dest->d_die_info.delta = src->d_die_info.delta;
	dest->d_die_info.target_pwr = src->d_die_info.target_pwr;
	dest->d_die_info.comp_grp = src->d_die_info.comp_grp;
	dest->d_die_info.fe_gain_mode = src->d_die_info.fe_gain_mode;
	memcpy(dest->d_die_info.reserved, src->d_die_info.reserved, sizeof(dest->d_die_info.reserved));
}

void mbraink_v6993_get_wifi_tx_power_data(struct mbraink_wifi2mbr_tx_power_data *txpwr_buffer)
{
	unsigned short len = 0;
	enum wifi2mbr_status ret;
	struct wifi2mbr_txpwr txpwr_info;
	int retry_count = 0;

	memset(txpwr_buffer, 0, sizeof(struct mbraink_wifi2mbr_tx_power_data));

	while (retry_count < MAX_WIFI_DATA_CNT) {
		ret = mbraink_bridge_wifi_get_data(MBR2WIFI_TXPWR_RPT,
						   WIFI2MBR_TAG_TXPWR_RPT,
						   (void *)(&txpwr_info), &len);

		if (ret == WIFI2MBR_NO_OPS || ret == WIFI2MBR_END) {
			break;
		} else if (ret == WIFI2MBR_FAILURE) {
			retry_count++;
			continue;
		} else if (ret == WIFI2MBR_SUCCESS) {
			if (len != sizeof(struct wifi2mbr_txpwr)) {
				pr_info("%s: Received data length (%u) doesn't match expected size (%zu)",
					__func__, len, sizeof(struct wifi2mbr_txpwr));
				retry_count++;
				continue;
			}

			txpwr_buffer->timestamp = txpwr_info.timestamp;
			txpwr_buffer->rpt_type = txpwr_info.rpt_type;
			txpwr_buffer->max_bn_num = txpwr_info.max_bn_num;
			txpwr_buffer->max_ant_num = txpwr_info.max_ant_num;

			if (txpwr_buffer->max_bn_num > MAX_WIFI_BAND_NUM ||
			    txpwr_buffer->max_ant_num > MAX_WIFI_ANTENA_NUM) {
				pr_info("%s: Invalid max_bn_num (%u) or max_ant_num (%u)",
					__func__, txpwr_buffer->max_bn_num,
					txpwr_buffer->max_ant_num);
				retry_count++;
				continue;
			}

			for (int bn = 0; bn < txpwr_buffer->max_bn_num; bn++) {
				for (int ant = 0; ant < txpwr_buffer->max_ant_num; ant++) {
					copy_txpwr_info(&txpwr_buffer->info[bn][ant],
							&txpwr_info.info[bn][ant]);
				}
			}

			break;
		}
	}

	if (retry_count == MAX_WIFI_DATA_CNT)
		pr_info("%s: Reached maximum retry count\n", __func__);
}

void mbraink_v6993_get_wifi_rxtxperf_data(int current_idx,
		struct mbraink_wifi2mbr_rxtxperf_data *rxtxperf_buffer)
{
	unsigned short len = 0;
	enum wifi2mbr_status ret = WIFI2MBR_FAILURE;
	struct wifi2mbr_TRxPerfInfo rxtxperf_info;
	int loop = 0;
	int cnt = 0;

	memset(rxtxperf_buffer, 0, sizeof(struct mbraink_wifi2mbr_rxtxperf_data));

	do {
		ret = mbraink_bridge_wifi_get_data(MBR2WIFI_TRX_PERF,
						WIFI2MBR_TAG_TRX_PERF,
						(void *)(&rxtxperf_info), &len);
		loop++;

		if (ret == WIFI2MBR_NO_OPS)
			break;
		else if (ret == WIFI2MBR_FAILURE)
			continue;
		else if (ret == WIFI2MBR_SUCCESS) {
			cnt = rxtxperf_buffer->count;

			if (cnt < MAX_WIFI_RXTXPERF_SZ) {
				rxtxperf_buffer->rxtxperf_data[cnt].timestamp = rxtxperf_info.timestamp;
				rxtxperf_buffer->rxtxperf_data[cnt].bss_index = rxtxperf_info.bss_index;
				rxtxperf_buffer->rxtxperf_data[cnt].wlan_index = rxtxperf_info.wlan_index;
				rxtxperf_buffer->rxtxperf_data[cnt].tx_stop_timestamp =
								rxtxperf_info.tx_stop_timestamp;
				rxtxperf_buffer->rxtxperf_data[cnt].tx_resume_timestamp =
								rxtxperf_info.tx_resume_timestamp;
				rxtxperf_buffer->rxtxperf_data[cnt].bto_timestamp =
								rxtxperf_info.bto_timestamp;
				rxtxperf_buffer->rxtxperf_data[cnt].tx_timeout_timestamp =
								rxtxperf_info.tx_timeout_timestamp;
				rxtxperf_buffer->rxtxperf_data[cnt].token_id = rxtxperf_info.token_id;
				rxtxperf_buffer->rxtxperf_data[cnt].timeout_duration =
								rxtxperf_info.timeout_duration;
				rxtxperf_buffer->rxtxperf_data[cnt].operation_mode =
								rxtxperf_info.operation_mode;
				rxtxperf_buffer->rxtxperf_data[cnt].tput = rxtxperf_info.tput;
				rxtxperf_buffer->rxtxperf_data[cnt].idle_slot = rxtxperf_info.idle_slot;
				rxtxperf_buffer->rxtxperf_data[cnt].tx_per = rxtxperf_info.tx_per;
				rxtxperf_buffer->rxtxperf_data[cnt].tx_rate = rxtxperf_info.tx_rate;
				rxtxperf_buffer->rxtxperf_data[cnt].rx_rssi = rxtxperf_info.rx_rssi;
				rxtxperf_buffer->rxtxperf_data[cnt].rx_per = rxtxperf_info.rx_per;
				rxtxperf_buffer->rxtxperf_data[cnt].rx_rate = rxtxperf_info.rx_rate;
				rxtxperf_buffer->rxtxperf_data[cnt].rx_drop_total =
								rxtxperf_info.rx_drop_total;
				rxtxperf_buffer->rxtxperf_data[cnt].rx_drop_reorder =
								rxtxperf_info.rx_drop_reorder;
				rxtxperf_buffer->rxtxperf_data[cnt].rx_drop_sanity =
								rxtxperf_info.rx_drop_sanity;
				rxtxperf_buffer->rxtxperf_data[cnt].rx_napi_full =
								rxtxperf_info.rx_napi_full;
				rxtxperf_buffer->rxtxperf_data[cnt].rts_fail_rate =
								rxtxperf_info.rts_fail_rate;
				memcpy(rxtxperf_buffer->rxtxperf_data[cnt].ipi_hist, rxtxperf_info.ipi_hist,
						sizeof(unsigned int) * 22);
				rxtxperf_buffer->rxtxperf_data[cnt].nbi = rxtxperf_info.nbi;
				rxtxperf_buffer->rxtxperf_data[cnt].cu_all = rxtxperf_info.cu_all;
				rxtxperf_buffer->rxtxperf_data[cnt].cu_not_me = rxtxperf_info.cu_not_me;
				memcpy(rxtxperf_buffer->rxtxperf_data[cnt].snr, rxtxperf_info.snr,
						sizeof(unsigned char) * 2);

				rxtxperf_buffer->count++;

				if (cnt ==  MAX_WIFI_RXTXPERF_SZ - 1) {
					rxtxperf_buffer->idx = current_idx + rxtxperf_buffer->count;
					break;
				}
			}
		} else if (ret ==  WIFI2MBR_END) {
			rxtxperf_buffer->idx = 0;
			break;
		}
	} while (loop < MAX_WIFI_DATA_CNT);

	if (loop == MAX_WIFI_DATA_CNT)
		pr_info("%s: loop cnt is MAX_WIFI_DATA_CNT\n", __func__);
}

void mbraink_v6993_get_wifi_wakeupinfo_data(int current_idx,
		struct mbraink_wifi2mbr_wakeupinfo_data *wakeupinfo_buffer)
{
	unsigned short len = 0;
	enum wifi2mbr_status ret = WIFI2MBR_FAILURE;
	struct wifi2mbr_WiFiWkUpRsnInfo wakeup_info;
	int loop = 0;
	int cnt = 0;

	memset(wakeupinfo_buffer, 0, sizeof(struct mbraink_wifi2mbr_wakeupinfo_data));

	do {
		ret = mbraink_bridge_wifi_get_data(MBR2WIFI_WIFI_WKUP_REASON,
						WIFI2MBR_TAG_WIFI_WKUP_REASON,
						(void *)(&wakeup_info), &len);
		loop++;

		if (ret == WIFI2MBR_NO_OPS)
			break;
		else if (ret == WIFI2MBR_FAILURE)
			continue;
		else if (ret == WIFI2MBR_SUCCESS) {
			cnt = wakeupinfo_buffer->count;

			if (cnt < MAX_WIFI_WAKEUP_INFO_SZ) {
				wakeupinfo_buffer->wakeup_data[cnt].timestamp = wakeup_info.timestamp;
				wakeupinfo_buffer->wakeup_data[cnt].wkup_reason =
					(enum mbraink_enum_mbr_wakeup_reason) wakeup_info.wkup_reason;
				wakeupinfo_buffer->wakeup_data[cnt].wkup_info = wakeup_info.wkup_info;
				wakeupinfo_buffer->wakeup_data[cnt].total_suspend_period =
					wakeup_info.total_suspend_period;
				wakeupinfo_buffer->wakeup_data[cnt].resume_time = wakeup_info.resume_time;
				wakeupinfo_buffer->wakeup_data[cnt].suspend_time = wakeup_info.suspend_time;
				wakeupinfo_buffer->wakeup_data[cnt].wifi_wkup_period = wakeup_info.wifi_wkup_period;
				wakeupinfo_buffer->wakeup_data[cnt].wifi_wkup_time = wakeup_info.wifi_wkup_time;
				wakeupinfo_buffer->wakeup_data[cnt].wifi_suspend_time = wakeup_info.wifi_suspend_time;
				wakeupinfo_buffer->count++;

				if (cnt ==  MAX_WIFI_WAKEUP_INFO_SZ - 1) {
					wakeupinfo_buffer->idx = current_idx + wakeupinfo_buffer->count;
					break;
				}
			}
		} else if (ret ==  WIFI2MBR_END) {
			wakeupinfo_buffer->idx = 0;
			break;
		}
	} while (loop < MAX_WIFI_DATA_CNT);

	if (loop == MAX_WIFI_DATA_CNT)
		pr_info("%s: loop cnt is MAX_WIFI_DATA_CNT\n", __func__);
}

static int handle_txtimeout_data(struct wifi2mbr_data *wifi_data)
{
	struct wifi2mbr_TxTimeoutInfo *tx_timeout;
	char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};
	int n = 0;
	struct timespec64 tv = { 0 };
	long long timestamp = 0;

	if (wifi_data->len != sizeof(struct wifi2mbr_TxTimeoutInfo)) {
		pr_info("Invalid TXTIMEOUT data size\n");
		return -EINVAL;
	}

	tx_timeout = (struct wifi2mbr_TxTimeoutInfo *)wifi_data->data;

	ktime_get_real_ts64(&tv);
	timestamp = (tv.tv_sec * 1000) + (tv.tv_nsec / 1000000);

	n = snprintf(netlink_buf, NETLINK_EVENT_MESSAGE_SIZE,
				 "%s:%u:%lld:%llu:%u:%u:%u:%u:%u:%u",
				 NETLINK_EVENT_WIFI_NOTIFY, wifi_data->tag, timestamp, tx_timeout->timestamp,
				 tx_timeout->token_id, tx_timeout->wlan_index, tx_timeout->bss_index,
				 tx_timeout->timeout_duration, tx_timeout->operation_mode,
				 tx_timeout->idle_slot_diff_cnt);

	if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE)
		return -EINVAL;

	mbraink_netlink_send_msg(netlink_buf);
	return 0;
}

static inline int append_txpwr_info(char *buf, int pos, int max_size,
									int bn, int ant,
									const struct wifi2mbr_txpwr_info *info)
{
	int n = snprintf(buf + pos, max_size - pos,
					":%d:%d:%u:%u:%u:%u:%u:%d:%u:%u:%u:%u:%u:%u:%u:%u:%u:%u:%d:%u:%u:%u:%u:%u:%u:%u",
					 bn, ant, info->epa_support, info->cal_type, info->center_ch,
					 info->mcc_idx, info->rf_band, info->temp, info->antsel,
					 info->coex.bt_on, info->coex.lte_on,
					 info->coex.reserved[0], info->coex.reserved[1],
					 info->coex.bt_profile, info->coex.pta_grant, info->coex.pta_req,
					 info->coex.curr_op_mode, info->d_die_info.delta,
					 info->d_die_info.target_pwr, info->d_die_info.comp_grp,
					 info->d_die_info.fe_gain_mode,
					 info->d_die_info.reserved[0], info->d_die_info.reserved[1],
					 info->d_die_info.reserved[2], info->d_die_info.reserved[3],
					 info->d_die_info.reserved[4]);

	if (n < 0 || n >= max_size - pos)
		return -1;

	return n;
}

static int append_txpower_report(char *buf, int *pos, int max_size,
				 unsigned int tag, long long timestamp,
				 const struct wifi2mbr_txpwr *txpwr)
{
	int n, bn, ant;

	n = snprintf(buf + *pos, max_size - *pos,
		     ":%u:%lld:%llu:%u:%u:%u",
		     tag, timestamp, txpwr->timestamp,
		     txpwr->rpt_type, txpwr->max_bn_num, txpwr->max_ant_num);

	if (n < 0 || n >= max_size - *pos)
		return -1;
	*pos += n;

	for (bn = 0; bn < txpwr->max_bn_num && bn < MAX_WIFI_BAND_NUM; bn++) {
		for (ant = 0; ant < txpwr->max_ant_num && ant < MAX_WIFI_ANTENA_NUM; ant++) {
			n = append_txpwr_info(buf, *pos, max_size,
					      bn, ant, &txpwr->info[bn][ant]);
			if (n < 0)
				return -1;
			*pos += n;
		}
	}

	return 0;
}

static int handle_txpower_data(struct wifi2mbr_data *wifi_data)
{
	struct wifi2mbr_txpwr *txpwr;
	char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};
	int pos = 0;
	struct timespec64 tv = { 0 };
	long long timestamp = 0;
	int remaining_count = wifi_data->count - 1;

	txpwr = (struct wifi2mbr_txpwr *)wifi_data->data;

	ktime_get_real_ts64(&tv);
	timestamp = (tv.tv_sec * 1000) + (tv.tv_nsec / 1000000);

	pos = snprintf(netlink_buf, NETLINK_EVENT_MESSAGE_SIZE,
		       "%s", NETLINK_EVENT_WIFI_NOTIFY);
	if (pos < 0 || pos >= NETLINK_EVENT_MESSAGE_SIZE)
		return -1;

	// Handle the first report
	if (append_txpower_report(netlink_buf, &pos, NETLINK_EVENT_MESSAGE_SIZE,
				  wifi_data->tag, timestamp, txpwr) < 0)
		return -1;

	// Handle remaining reports
	while (remaining_count > 0 && remaining_count < MAX_WIFI_NOTIFY_TXPOWER_RPT_NUM) {
		unsigned short len = 0;
		enum wifi2mbr_status ret;
		struct wifi2mbr_txpwr additional_txpwr;
		int retry_count = 0;

		while (retry_count < MAX_WIFI_DATA_CNT) {
			ret = mbraink_bridge_wifi_get_data(MBR2WIFI_TXPWR_RPT,
							   WIFI2MBR_TAG_TXPWR_RPT,
							   (void *)(&additional_txpwr), &len);

			if (ret == WIFI2MBR_NO_OPS || ret == WIFI2MBR_END) {
				break;
			} else if (ret == WIFI2MBR_FAILURE) {
				retry_count++;
				continue;
			} else if (ret == WIFI2MBR_SUCCESS) {
				if (len != sizeof(struct wifi2mbr_txpwr)) {
					pr_info("%s: Received data length (%u) doesn't match expected size (%zu)",
						__func__, len, sizeof(struct wifi2mbr_txpwr));
					retry_count++;
					continue;
				}

				if (append_txpower_report(netlink_buf, &pos, NETLINK_EVENT_MESSAGE_SIZE,
							  wifi_data->tag, timestamp, &additional_txpwr) < 0)
					return -1;

				break;
			}
		}

		if (retry_count == MAX_WIFI_DATA_CNT) {
			pr_info("%s: Reached maximum retry count for additional TX power data ", __func__);
			break;
		}

		remaining_count--;
	}

	mbraink_netlink_send_msg(netlink_buf);
	pr_info("%s: End of TX power report handling ", __func__);
	return 0;
}

static int mbraink_v6993_set_wifi_data(struct wifi2mbr_data *wifi_data)
{
	switch (wifi_data->tag) {
	case WIFI2MBR_TAG_TXTIMEOUT:
		return handle_txtimeout_data(wifi_data);
	case WIFI2MBR_TAG_TXPWR_RPT:
		return handle_txpower_data(wifi_data);
	default:
		pr_info("Unknown wifi data tag: %d\n", wifi_data->tag);
		return -EINVAL;
	}
}

static struct mbraink_wifi_ops mbraink_v6993_wifi_ops = {
	.get_wifi_rate_data = mbraink_v6993_get_wifi_rate_data,
	.get_wifi_radio_data = mbraink_v6993_get_wifi_radio_data,
	.get_wifi_ac_data = mbraink_v6993_get_wifi_ac_data,
	.get_wifi_lp_data = mbraink_v6993_get_wifi_lp_data,
	.get_wifi_txtimeout_data = mbraink_v6993_get_wifi_txtimeout_data,
	.get_wifi_pcie_data = mbraink_v6993_get_wifi_pcie_data,
	.get_wifi_tx_power_data = mbraink_v6993_get_wifi_tx_power_data,
	.get_wifi_rxtxperf_data = mbraink_v6993_get_wifi_rxtxperf_data,
	.get_wifi_wakeupinfo_data = mbraink_v6993_get_wifi_wakeupinfo_data,
};

static struct wifi2mbraink_set_ops mbraink_v6993_wifi_set_ops = {
	.set_data = mbraink_v6993_set_wifi_data,
};

int mbraink_v6993_wifi_init(void)
{
	int ret = 0;

	ret = register_mbraink_wifi_ops(&mbraink_v6993_wifi_ops);
	if (ret) {
		pr_info("Failed to register mbraink wifi ops: %d\n", ret);
		return ret;
	}

	ret = register_platform_to_bridge_ops(&mbraink_v6993_wifi_set_ops);
	if (ret)
		pr_info("Failed to register platform to bridge ops: %d\n", ret);

	return 0;
}

int mbraink_v6993_wifi_deinit(void)
{
	unregister_platform_to_bridge_ops();
	unregister_mbraink_wifi_ops();

	return 0;
}

