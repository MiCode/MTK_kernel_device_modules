/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __DVFSRC_VSMR_H__
#define __DVFSRC_VSMR_H__
#define MAX_VSMR_DATA_SIZE 288

struct mtk_vsmr_header{
	uint8_t module_id;
	uint8_t version;
	uint16_t data_offset;
	uint32_t data_length;
	uint32_t last_data[MAX_VSMR_DATA_SIZE];
	uint32_t timer;
	bool vsmr_support;
};

struct mtk_dvfsrc_vsmr_config {
	void (*get_data)(struct mtk_vsmr_header *header);
	const int *regs;
};

struct mtk_vsmr_data {
	uint8_t module_id;
	uint8_t version;
	uint16_t data_offset;
	uint32_t data_length;
	const struct mtk_dvfsrc_vsmr_config *config;
};

struct mtk_vsmr {
	struct device *dev;
	void __iomem *regs;
	const struct mtk_vsmr_data *dvd;
	bool vsmr_support;
};

enum vsmr_last_data_regs {
	SW_DDR_REQUEST,
	EMI_QOS,
	DDR_QOS_SECOND,
	SECURITY,
	DISP_URGENT_DDR_VOTE,
	DDR_BW_MONITOR,
	DDR_BW_MONITOR_ONLY_DRAMC,
	CHINFRA_BW_MONITOR_DDR_VOTE,
	EMI_LATENCY_DDR_REQ,
	HIFI3_LATENCY_DDR_REQ,
	HRT_BW_REQ,
	DDR_MD_IMP_REQ,
	MD_BW_LEVEL_REQ,
	DDR_MD_LATENCY_VOTE0,
	DDR_MD_LATENCY_VOTE1,
	TARGET_DDR_GEAR_PRE,
	TARGET_DDR_GEAR_INCR,
	WLA_URGENT_VOTE,
	VSMR_LAST_REG_MAX,
};

void vsmr_get_data(struct mtk_vsmr_header *header);

#endif /*__DVFSRC_VSMR_H__*/
