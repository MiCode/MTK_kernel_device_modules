/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef APU_CONFIG_H
#define APU_CONFIG_H

#include "apu_ipi.h"

struct mtk_apu;

struct apu_ipi_config {
	u64 in_buf_da;
	u64 out_buf_da;
} __packed;

struct vpu_init_info {
	uint32_t vpu_num;
	uint32_t cfg_addr;
	uint32_t cfg_size;
	uint32_t algo_info_ptr[3 * 2];
	uint32_t rst_vec[3];
	uint32_t dmem_addr[3];
	uint32_t imem_addr[3];
	uint32_t iram_addr[3];
	uint32_t cmd_addr[3];
	uint32_t log_addr[3];
	uint32_t log_size[3];
} __packed;

struct apusys_chip_data {
	uint32_t s_code;
	uint32_t b_code;
	uint32_t r_code;
	uint32_t a_code;
} __packed;

struct logger_init_info {
	uint32_t iova;
	uint32_t iova_h;
	uint32_t aov_iova;
	uint32_t aov_iova_h;
	uint32_t aov_buf_sz;
	uint32_t lbc_sz;
	uint32_t buf_sz;
	uint32_t burst_len;
} __packed;

struct apummu_init_info {
	uint32_t boundary;
	uint64_t dram[32];
} __packed;

struct reviser_init_info {
	uint32_t boundary;
	uint64_t dram[32];
} __packed;

struct mvpu_preempt_data {
	uint32_t itcm_buffer_core_0[5];
	uint32_t l1_buffer_core_0[5];
	uint32_t itcm_buffer_core_1[5];
	uint32_t l1_buffer_core_1[5];
} __packed;

enum user_config {
	eAPU_IPI_CONFIG = 0x0,
	eVPU_INIT_INFO,
	eAPUSYS_CHIP_DATA,
	eLOGGER_INIT_INFO,
	eREVISER_INIT_INFO,
	eMVPU_PREEMPT_DATA,
	eUSER_CONFIG_MAX
};

struct config_v1_entry_table {
	u32 user_entry[eUSER_CONFIG_MAX];
} __packed;

struct tag_chipid {
	u32 size;
	u32 hw_code;
	u32 hw_subcode;
	u32 hw_ver;
	u32 sw_ver;
 };

struct config_v1 {
	/* header begin */
	u32 header_magic;
	u32 header_rev;
	u32 entry_offset;
	u32 config_size;
	/* header end */
	/* do not add new member before this line */

	/* system related config begin */
	u32 ramdump_offset;
	u32 ramdump_type;
	u32 ramdump_module;
	u64 time_offset;
	u64 time_diff;
	u64 time_diff_cycle;
	u32 debug_memory_iova;
	u32 chip_sw_ver;
	u32 ipi_krn_cb_rdy[(APU_IPI_MAX+31)/32];
	/* system related config end */

	/* entry table */
	u8 entry_tbl[sizeof(struct config_v1_entry_table)];

	/* user data payload begin */
	u8 user0_data[sizeof(struct apu_ipi_config)];
	u8 user1_data[sizeof(struct vpu_init_info)];
	u8 user2_data[sizeof(struct apusys_chip_data)];
	u8 user3_data[sizeof(struct logger_init_info)];
	u8 user4_data[sizeof(struct reviser_init_info)];
	u8 user5_data[sizeof(struct mvpu_preempt_data)];
	/* user data payload end */
} __packed;

void apu_ipi_config_remove(struct mtk_apu *apu);
int apu_ipi_config_init(struct mtk_apu *apu);

static inline void *get_apu_config_user_ptr(struct config_v1 *conf,
	enum user_config user_id)
{
	struct config_v1_entry_table *entry_tbl;

	if (!conf)
		return NULL;

	if (user_id >= eUSER_CONFIG_MAX)
		return NULL;

	entry_tbl = (struct config_v1_entry_table *)
		((void *)conf + conf->entry_offset);

	return (void *)conf + entry_tbl->user_entry[user_id];
}
#endif /* APU_CONFIG_H */
