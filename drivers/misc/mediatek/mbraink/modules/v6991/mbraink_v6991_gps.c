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
		memset(&lp_data, 0, sizeof(lp_data));
		lp_data.hdr_in.data_size = sizeof(lp_data);
		lp_data.hdr_in.major_ver = LP_DATA_MAJOR_VER;
		lp_data.hdr_in.minor_ver = LP_DATA_MINOR_VER;

		ret = mbraink_bridge_gps_get_lp_data(MBR2GNSS_TEST, &lp_data);
		if (ret == GNSS2MBR_NO_DATA)
			break;

		if (lp_data.hdr_out.data_size != lp_data.hdr_in.data_size)
			pr_info("%s: out.data_size is not match in.data_size : %u, %u\n",
				__func__,
				lp_data.hdr_out.data_size,
				lp_data.hdr_in.data_size);

		pr_info("%s: lp_data: ver=%u,%u, sz=%u,%u, idx=%u,ts=%llu, sid=%u, st=%d,%d,%d,%u",
			__func__,
			lp_data.hdr_out.major_ver,
			lp_data.hdr_out.minor_ver,
			lp_data.hdr_out.data_size,
			lp_data.hdr_in.data_size,

			lp_data.dump_index,
			lp_data.dump_ts,

			lp_data.gnss_mcu_sid,

			lp_data.gnss_mcu_is_on,
			lp_data.gnss_pwr_is_hi,
			lp_data.gnss_pwr_wrn,
			lp_data.gnss_pwr_wrn_cnt
		);
	} while (ret == GNSS2MBR_OK_MORE);

}

void mbraink_v6991_get_gnss_mcu_data(void)
{
	enum gnss2mbr_status ret;
	struct gnss2mbr_mcu_data mcu_data;

	do {
		memset(&mcu_data, 0, sizeof(mcu_data));
		mcu_data.hdr_in.data_size = sizeof(mcu_data);
		mcu_data.hdr_in.major_ver = MCU_DATA_MAJOR_VER;
		mcu_data.hdr_in.minor_ver = MCU_DATA_MINOR_VER;

		ret = mbraink_bridge_gps_get_mcu_data(MBR2GNSS_TEST, &mcu_data);
		if (ret == GNSS2MBR_NO_DATA)
			break;

		if (mcu_data.hdr_out.data_size != mcu_data.hdr_in.data_size)
			pr_info("%s: out.data_size is not match in.data_size : %u, %u\n",
				__func__,
				mcu_data.hdr_out.data_size,
				mcu_data.hdr_in.data_size);

		pr_info("%s: mcu_data: ver=%u,%u, sz=%u,%u, sid=%u,0x%x, o=%llu,%u,%u, e=%d,%d,%u,%u, c=%llu,%u,%u",
			__func__,
			mcu_data.hdr_out.major_ver,
			mcu_data.hdr_out.minor_ver,
			mcu_data.hdr_out.data_size,
			mcu_data.hdr_in.data_size,

			mcu_data.gnss_mcu_sid,
			mcu_data.clock_cfg_val,

			mcu_data.open_ts,
			mcu_data.open_duration,
			mcu_data.open_duration_max,

			mcu_data.has_exception,
			mcu_data.force_close,
			mcu_data.exception_cnt,
			mcu_data.force_close_cnt,

			mcu_data.close_ts,
			mcu_data.close_duration,
			mcu_data.close_duration_max
		);
	} while (ret == GNSS2MBR_OK_MORE);

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
