// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/module.h>

#include <mbraink_ioctl_struct_def.h>
#include <mbraink_modules_ops_def.h>
#include <bridge/mbraink_bridge_gps.h>

void mbraink_v6991_get_gnss_lp_data(void)
{
	enum gnss2mbr_status ret;
	struct gnss2mbr_lp_data lp_data;

	do {
		ret = mbraink_bridge_gps_get_lp_data(TEST, &lp_data);
		if (ret == NO_DATA)
			break;

		pr_info("%s: lp_data: ver=%u,%u, sz=%u, ap_res: idx=%u,ts=%llu, gps: sid=%u, st=%d,%d,%d",
			__func__,
			lp_data.hdr.major_ver,
			lp_data.hdr.minor_ver,
			lp_data.hdr.data_size,

			lp_data.ap_resume_index,
			lp_data.ap_resume_ts,

			lp_data.gnss_mcu_sid,

			lp_data.gnss_mcu_is_on,
			lp_data.gnss_pwr_is_hi,
			lp_data.gnss_pwr_wrn
		);
	} while (ret == DATA_OK_AGAIN);

}

void mbraink_v6991_get_gnss_mcu_data(void)
{
	enum gnss2mbr_status ret;
	struct gnss2mbr_mcu_data mcu_data;

	do {
		ret = mbraink_bridge_gps_get_mcu_data(TEST, &mcu_data);
		if (ret == NO_DATA)
			break;

		pr_info("%s: mcu_data: ver=%u,%u, sz=%u, gps: sid=%u,0x%x, o=%llu,%u, e=%d, c=%llu,%u",
			__func__,
			mcu_data.hdr.major_ver,
			mcu_data.hdr.minor_ver,
			mcu_data.hdr.data_size,

			mcu_data.gnss_mcu_sid,
			mcu_data.clock_cfg_val,

			mcu_data.open_ts,
			mcu_data.open_duration,
			mcu_data.has_exception,
			mcu_data.close_ts,
			mcu_data.close_duration
		);
	} while (ret == DATA_OK_AGAIN);

}

static struct mbraink_gps_ops mbraink_v6991_gps_ops = {
	.get_gnss_lp_data = mbraink_v6991_get_gnss_lp_data,
	.get_gnss_mcu_data = mbraink_v6991_get_gnss_mcu_data,
};

int mbraink_v6991_gps_init(void)
{
	int ret = 0;

	ret = register_mbraink_gps_ops(&mbraink_v6991_gps_ops);

	return ret;
}

int mbraink_v6991_gps_deinit(void)
{
	int ret = 0;

	ret = unregister_mbraink_gps_ops();

	return ret;
}
