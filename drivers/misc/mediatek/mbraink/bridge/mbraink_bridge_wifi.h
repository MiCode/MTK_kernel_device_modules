/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef MBRAINK_BRIDGE_WIFI_H
#define MBRAINK_BRIDGE_WIFI_H

enum wifi2mbr_status {
	WIFI2MBR_SUCCESS,
	WIFI2MBR_FAILURE,
	WIFI2MBR_END,
	WIFI2MBR_NO_OPS,
};

enum mbr2wifi_reason {
	MBR2WIFI_TRX_BIG_DATA,
	MBR2WIFI_TEST_LP_RATIO,
	MBR2WIFI_TX_TIMEOUT,
	MBR2WIFI_PCIE_DATA,
	MBR2WIFI_TXPWR_RPT,
	MBR2WIFI_TRX_PERF,
	MBR2WIFI_WIFI_WKUP_REASON,
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
	WIFI2MBR_TAG_LP_RATIO,
	WIFI2MBR_TAG_TXTIMEOUT,
	WIFI2MBR_TAG_PCIE,
	WIFI2MBR_TAG_TXPWR_RPT,
	WIFI2MBR_TAG_TRX_PERF,
	WIFI2MBR_TAG_WIFI_WKUP_REASON,
	WIFI2MBR_TAG_MAX
};

struct wifi2mbr_data {
	enum wifi2mbr_tag tag;
	void *data;
	unsigned short len;
	unsigned short count;
};

#define LLS_RATE_PREAMBIE_MASK (GENMASK(2, 0))
#define LLS_RATE_NSS_MASK (GENMASK(4, 3))
#define LLS_RATE_BW_MASK (GENMASK(7, 5))
#define LLS_RATE_MCS_IDX_MASK (GENMASK(15, 8))
#define LLS_RATE_RSVD_MASK (GENMASK(31, 16))

/* struct for WIFI2MBR_TAG_LLS_RATE */
struct wifi2mbr_llsRateInfo {
	struct wifi2mbr_hdr hdr;
	u64 timestamp;
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
	u64 timestamp;
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
	u64 timestamp;
	enum enum_mbr_wifi_ac ac;
	unsigned int tx_mpdu;
	unsigned int rx_mpdu;
	unsigned int tx_mcast;
	unsigned int tx_ampdu;
	unsigned int mpdu_lost;
	unsigned int retries;
	unsigned int contention_time_min;
	unsigned int contention_time_max;
	unsigned int contention_time_avg;
	unsigned int contention_num_samples;
};

/* struct for WIFI2MBR_TAG_LP_RATIO */
struct wifi2mbr_lpRatioInfo {
	struct wifi2mbr_hdr hdr;
	u64 timestamp;
	int radio;
	unsigned int total_time;
	unsigned int tx_time;
	unsigned int rx_time;
	unsigned int rx_listen_time;
	unsigned int sleep_time;
};

struct wifi2mbr_TxTimeoutInfo {
	struct wifi2mbr_hdr hdr;
	u64 timestamp;
	unsigned int token_id;
	unsigned int wlan_index;
	unsigned int bss_index;
	unsigned int timeout_duration;
	unsigned int operation_mode;
	unsigned int idle_slot_diff_cnt;
};

/* struct for WIFI2MBR_TAG_PCIE */
struct wifi2mbr_PcieInfo {
	struct wifi2mbr_hdr hdr;
	u64 timestamp;
	unsigned int update_time_utc_sec;
	unsigned int update_time_utc_usec;
	unsigned int req_recovery_count;
	unsigned int l0_time_s;
	unsigned int l0_time_us;
	unsigned int l1_time_s;
	unsigned int l1_time_us;
	unsigned int l1ss_time_s;
	unsigned int l1ss_time_us;
};

/* struct for WIFI2MBR_TAG_txpwr_info */
#define WIFI2MBR_MAX_BAND_NUM               3
#define WIFI2MBR_MAX_ANTENA_NUM             2

struct wifi2mbr_txpwr_coex_info {
	bool bt_on;
	bool lte_on;
	unsigned char reserved[2];
	unsigned int bt_profile;
	unsigned int pta_grant;
	unsigned int pta_req;
	unsigned int curr_op_mode;
};

struct wifi2mbr_txpwr_d_die_info {
	unsigned int delta;
	signed char target_pwr;
	unsigned char comp_grp;
	unsigned char fe_gain_mode;
	unsigned char reserved[5];
};

struct wifi2mbr_txpwr_info {
	bool epa_support;
	unsigned char cal_type;
	unsigned char center_ch;
	unsigned char mcc_idx;
	unsigned int rf_band;
	signed int temp;
	unsigned int antsel;
	struct wifi2mbr_txpwr_coex_info coex;
	struct wifi2mbr_txpwr_d_die_info d_die_info;
};

struct wifi2mbr_txpwr {
	struct wifi2mbr_hdr hdr;
	u64 timestamp;
	unsigned char rpt_type;
	unsigned char max_bn_num;
	unsigned char max_ant_num;
	struct wifi2mbr_txpwr_info info[WIFI2MBR_MAX_BAND_NUM][WIFI2MBR_MAX_ANTENA_NUM];
};

struct wifi2mbr_TRxPerfInfo {
	struct wifi2mbr_hdr hdr;
	u64 timestamp;
	unsigned int bss_index;
	unsigned int wlan_index;

	/* trx event timestamp */
	u64 tx_stop_timestamp;
	u64 tx_resume_timestamp;
	u64 bto_timestamp;

	/* txtimeout index */
	u64 tx_timeout_timestamp;
	unsigned int token_id;
	unsigned int timeout_duration;
	unsigned int operation_mode;

	/* trx performance index*/
	u64 tput;
	u64 idle_slot; /* idle slot diff count */

	unsigned int tx_per;
	unsigned int tx_rate;

	int rx_rssi;
	unsigned int rx_per;
	unsigned int rx_rate;

	unsigned int rx_drop_total; /* total rx drop diff count */
	unsigned int rx_drop_reorder; /* reorder drop diff count */
	unsigned int rx_drop_sanity; /* sanity drop diff count */

	unsigned int rx_napi_full; /* napi full diff count */

	/* latency index*/
	unsigned int rts_fail_rate;
	unsigned int ipi_hist[2][11]; /* idle power indicate histogram*/
	unsigned int nbi; /* narrowband interference */
	unsigned char cu_all; /* channel utilization*/
	unsigned char cu_not_me; /* channel utilization others*/
	unsigned char snr[2]; /* signal-to-noise ratio */
};

enum enum_mbr_wifi_wkup_reason {
	MBR_WIFI_NO_WKUP,
	MBR_WIFI_RX_DATA_WKUP,
	MBR_WIFI_RX_EVENT_WKUP,
	MBR_WIFI_RX_MGMT_WKUP,
	MBR_WIFI_RX_OTHERS_WKUP,
};

struct wifi2mbr_WiFiWkUpRsnInfo {
	struct wifi2mbr_hdr hdr;
	u64 timestamp;
	enum enum_mbr_wifi_wkup_reason wkup_reason;
	unsigned int wkup_info;
	u64 total_suspend_period;
	u64 resume_time;
	u64 suspend_time;
	u64 wifi_wkup_period;
	u64 wifi_wkup_time;
	u64 wifi_suspend_time;
};

/**
 * struct mbraink2wifi_ops - Operations for MBrainK Bridge to WiFi communication
 * @get_data: Callback to get data from WiFi driver
 * @priv: Private data for use by the WiFi driver
 */
struct mbraink2wifi_ops {
	enum wifi2mbr_status (*get_data)(void *priv,
				enum mbr2wifi_reason reason,
				enum wifi2mbr_tag tag,
				void *data, unsigned short *real_len);
	void *priv;
};

/**
 * struct wifi2mbraink_set_ops - Operations for WiFi to MBrainK Bridge communication
 * @set_data: Callback to set data from WiFi driver
 */
struct wifi2mbraink_set_ops {
	int (*set_data)(struct wifi2mbr_data *wifi_data);
};

void mbraink_bridge_wifi_init(void);
void mbraink_bridge_wifi_deinit(void);
void register_wifi2mbraink_ops(struct mbraink2wifi_ops *ops);
void unregister_wifi2mbraink_ops(void);
int register_platform_to_bridge_ops(struct wifi2mbraink_set_ops *ops);
void unregister_platform_to_bridge_ops(void);
enum wifi2mbr_status
mbraink_bridge_wifi_get_data(enum mbr2wifi_reason reason,
			enum wifi2mbr_tag tag,
			void *data,
			unsigned short *real_len);
void wifi2mbrain_notify(enum wifi2mbr_tag tag,
			void *data,
			unsigned short len,
			unsigned short count);

/**
 * convert_to_utc_usec - Convert seconds and microseconds to UTC microseconds
 * @sec: seconds since Unix epoch
 * @usec: microseconds part
 *
 * This function converts the given seconds and microseconds to a UTC
 * timestamp in microseconds, preserving microsecond precision.
 *
 * Note: This function uses u64 to avoid overflow and preserve precision.
 *
 * Return: UTC timestamp in microseconds as u64
 */
static inline u64 convert_to_utc_usec(unsigned int sec, unsigned int usec)
{
	return (u64)sec * 1000000 + (u64)usec;
}
#endif /*MBRAINK_BRIDGE_WIFI_H*/
