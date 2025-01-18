/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef MBRAINK_BRIDGE_WIFI_H
#define MBRAINK_BRIDGE_WIFI_H

enum wifi2mbr_status {
	WIFI2MBR_NO_DATA,
	WIFI2MBR_DATA_OK_END,
	WIFI2MBR_DATA_OK_AGAIN,
};

enum mbr2wifi_reason {
	MBR2WIFI_TEST1,
	MBR2WIFI_TEST2,
};

struct wifi2mbr_hdr {
	unsigned short tag;
	unsigned short ver;
	unsigned char ucReserved[4];
};

/* naming rule: WIFI2MBR_TAG_<module/feature>_XXX */
enum wifi2mbr_tag {
	WIFI2MBR_TAG_LLS_RATE,
	WIFI2MBR_TAG_LLS_RADIO,
	WIFI2MBR_TAG_LLS_AC,
	WIFI2MBR_TAG_MAX
};

#define LLS_RATE_PREAMBIE_MASK (GENMASK(2, 0))
#define LLS_RATE_NSS_MASK (GENMASK(4, 3))
#define LLS_RATE_BW_MASK (GENMASK(7, 5))
#define LLS_RATE_MCS_IDX_MASK (GENMASK(15, 8))
#define LLS_RATE_RSVD_MASK (GENMASK(31, 16))

/* struct for WIFI2MBR_TAG_LLS_RATE */
struct wifi2mbr_llsRateInfo {
	struct wifi2mbr_hdr hdr;
	unsigned int rate_idx; /* rate idx, ref: LLS_RATE_XXX_MASK */
	unsigned int bitrate;
	unsigned int tx_mpdu;
	unsigned int rx_mpdu;
	unsigned int mpdu_lost;
	unsigned int retries;
};

/* struct for WIFI2MBR_TAG_LLS_RADIO */
struct wifi2mbr_llsRadioInfo {
	struct wifi2mbr_hdr hdr;
	int radio;
	unsigned int on_time;
	unsigned int tx_time;
	unsigned int rx_time;
	unsigned int on_time_scan;
	unsigned int on_time_roam_scan;
	unsigned int on_time_pno_scan;
};

enum enum_mbr_wifi_ac {
	MBR_WIFI_AC_VO,
	MBR_WIFI_AC_VI,
	MBR_WIFI_AC_BE,
	MBR_WIFI_AC_BK,
	MBR_WIFI_AC_MAX,
};

/* struct for WIFI2MBR_TAG_LLS_AC */
struct wifi2mbr_llsAcInfo {
	struct wifi2mbr_hdr hdr;
	enum enum_mbr_wifi_ac ac;
	unsigned int tx_mpdu;
	unsigned int rx_mpdu;
	unsigned int tx_mcast;
	unsigned int rx_ampdu;
	unsigned int tx_ampdu;
	unsigned int mpdu_lost;
	unsigned int retries;
	unsigned int contention_time_min;
	unsigned int contention_time_max;
	unsigned int contention_time_avg;
	unsigned int contention_num_samples;
};

struct mbraink2wifi_ops {
	int (*get_next_tag)(void *priv, enum mbr2wifi_reason reason);
	enum wifi2mbr_status (*get_data)(void *priv, enum wifi2mbr_tag tag,
				void *data, unsigned short *real_len);
	void *priv;
};

void mbraink_bridge_wifi_init(void);
void mbraink_bridge_wifi_deinit(void);
void register_wifi2mbraink_ops(struct mbraink2wifi_ops *ops);
void unregister_wifi2mbraink_ops(void);
int mbraink_bridge_wifi_get_next_tag(enum mbr2wifi_reason reason);
enum wifi2mbr_status mbraink_bridge_wifi_get_data(enum wifi2mbr_tag tag,
						void *data,
						unsigned short *real_len);
#endif /*MBRAINK_BRIDGE_WIFI_H*/
