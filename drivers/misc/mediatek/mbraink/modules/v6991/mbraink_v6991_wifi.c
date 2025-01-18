// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/module.h>

#include <mbraink_ioctl_struct_def.h>
#include <mbraink_modules_ops_def.h>
#include <bridge/mbraink_bridge_wifi.h>

void mbraink_v6991_get_wifi_data(unsigned int reason)
{
	int tag = -1;
	unsigned short len = 0;
	enum wifi2mbr_status ret = WIFI2MBR_NO_DATA;
	struct wifi2mbr_llsRateInfo lls_rate;
	struct wifi2mbr_llsRadioInfo lls_radio;
	struct wifi2mbr_llsAcInfo lls_ac;

	do {
		tag = mbraink_bridge_wifi_get_next_tag(reason);
		if (tag < 0)
			break;

		if (tag == WIFI2MBR_TAG_LLS_RATE) {
			ret = mbraink_bridge_wifi_get_data(tag, (void *)(&lls_rate), &len);
			pr_info("%s: ret = %u, tag = %u, len = %u\n",
				__func__, ret, tag, len);
			pr_info("%s: rate_idx = %u, bitrate=%u, tx_mpdu=%u, rx_mpdu=%u\n",
				__func__,
				lls_rate.rate_idx, lls_rate.bitrate,
				lls_rate.tx_mpdu, lls_rate.rx_mpdu);
			pr_info("%s:  mpdu_lost=%u, retries=%u\n",
				__func__, lls_rate.mpdu_lost, lls_rate.retries);
		} else if (tag == WIFI2MBR_TAG_LLS_RADIO) {
			ret = mbraink_bridge_wifi_get_data(tag, (void *)(&lls_radio), &len);
			pr_info("%s: ret = %u, tag = %u, len = %u\n",
				__func__, ret, tag, len);
			pr_info("%s: radio=%d, on_time=%u, tx_time=%u, rx_time=%u\n",
				__func__,
				lls_radio.radio, lls_radio.on_time,
				lls_radio.tx_time, lls_radio.rx_time);
			pr_info("%s: on_time_scan=%u, on_time_roam_scan=%u, on_time_pno_scan=%u\n",
				__func__,
				lls_radio.on_time_scan,
				lls_radio.on_time_roam_scan,
				lls_radio.on_time_pno_scan);
		} else if (tag == WIFI2MBR_TAG_LLS_AC) {
			ret = mbraink_bridge_wifi_get_data(tag, (void *)(&lls_ac), &len);
			pr_info("%s: ret = %u, tag = %u, len = %u\n",
				__func__, ret, tag, len);
			pr_info("%s: ac=%u, tx_mpdu=%u,	rx_mpdu=%u, tx_mcast=%u\n",
				__func__,
				lls_ac.ac, lls_ac.tx_mpdu, lls_ac.rx_mpdu, lls_ac.tx_mcast);
			pr_info("%s: rx_ampdu=%u, tx_ampdu=%u, mpdu_lost=%u, retries=%u\n",
				__func__,
				lls_ac.rx_ampdu, lls_ac.tx_ampdu,
				lls_ac.mpdu_lost, lls_ac.retries);
			pr_info("%s: contention_time_min=%u, contention_time_max=%u\n",
				__func__,
				lls_ac.contention_time_min, lls_ac.contention_time_max);
			pr_info("%s: contention_time_avg=%u, contention_num_samples=%u\n",
				__func__,
				lls_ac.contention_time_avg, lls_ac.contention_num_samples);
		} else {
			pr_info("%s : unknown tag.\n", __func__);
			break;
		}
	} while (ret == WIFI2MBR_DATA_OK_AGAIN);

}

static struct mbraink_wifi_ops mbraink_v6991_wifi_ops = {
	.get_wifi_data = mbraink_v6991_get_wifi_data,
};

int mbraink_v6991_wifi_init(void)
{
	int ret = 0;

	ret = register_mbraink_wifi_ops(&mbraink_v6991_wifi_ops);

	return ret;
}

int mbraink_v6991_wifi_deinit(void)
{
	int ret = 0;

	ret = unregister_mbraink_wifi_ops();

	return ret;
}

