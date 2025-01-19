/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __DVFSRC_MB_H__
#define __DVFSRC_MB_H__

#define MAX_DATA_SIZE 6

enum dvfs_info_regs {
	SW_REQ1 = 0,
	SW_REQ2,
	SW_REQ3,
	SW_REQ4,
	SW_REQ5,
	SW_REQ7,
	SW_REQ10,
	MD_DDQ,
	DDR_QOS,
	DBG_STA0,
	DBG_STA1,
	DBG_STA2,
	DBG_STA3,
	DBG_STA4,
	DBG_STA5,
	DBG_STA6,
	DBG_STA7,
	DBG_STA8,
	DBG_STA9,
	DBG_STA10,
	SW_BW0,
	SW_BW1,
	SW_BW2,
	SW_BW3,
	SW_BW4,
	SW_BW5,
	SW_BW6,
	SW_BW7,
	SW_BW8,
	SW_BW9,
	DVFS_INFO_REG_NUM,
};

struct mtk_dvfsrc_header {
	uint8_t module_id;
	uint8_t version;
	uint16_t data_offset;
	uint32_t data_length;
	uint32_t data[MAX_DATA_SIZE];
};

struct mtk_dvfsrc_dvfs_info_header {
	uint32_t dvfs_info_version;
	uint32_t dvfs_info_val[DVFS_INFO_REG_NUM];
};

struct mtk_dvfsrc_config {
	void (*get_dvfs_info)(struct mtk_dvfsrc_dvfs_info_header *dvfs_info_header);
	void (*get_data)(struct mtk_dvfsrc_header *header);
};

struct mtk_dvfsrc_data {
	uint8_t module_id;
	uint8_t version;
	uint32_t data_offset;
	uint32_t data_length;
	const struct mtk_dvfsrc_config *config;
	uint32_t max_ddr_info_ver;
	uint32_t dvfs_info_ver;
	const uint32_t *dvfs_info_regs;
};

struct mtk_dvfsrc_mb {
	struct device *dev;
	void __iomem *regs;
	const struct mtk_dvfsrc_data *dvd;
};

#define DVFSRC_RSV_0        (0x788)
#define DVFSRC_RSV_1        (0x78C)
#define DVFSRC_RSV_2        (0x790)
#define DVFSRC_RSV_3        (0x794)
#define DVFSRC_RSV_4        (0x798)
#define DVFSRC_RSV_5        (0x79C)
#define DVFSRC_RSV_6        (0x7A0)

void dvfsrc_get_data(struct mtk_dvfsrc_header *header);
void dvfsrc_get_dvfs_info(struct mtk_dvfsrc_dvfs_info_header *dvfs_info_header);

#endif /*__DVFSRC_MB_H__*/
