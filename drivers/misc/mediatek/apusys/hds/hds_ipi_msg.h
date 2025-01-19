/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __MTK_APU_HDS_IPI_MSG_H__
#define __MTK_APU_HDS_IPI_MSG_H__

enum {
	APU_HDS_IPI_MSG_QUERY_INFO,
	APU_HDS_IPI_MSG_INIT,
};

struct apu_hds_ipi_msg {
	uint32_t op;
	union {
		struct {
			uint32_t version;
			uint32_t init_workbuf_size;
			uint32_t exec_per_cmd_size;
			uint32_t exec_subcmd_size;
			uint32_t pmu_per_cmd_size;
			uint32_t pmu_per_subcmd_size;
		} info;

		struct {
			uint64_t init_workbuf_va;
			uint32_t init_workbuf_size;
			uint64_t reserved0;
			uint64_t reserved1;
		} init;
	};
} __packed;

#endif