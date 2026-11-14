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
			uint32_t version_hw;
			uint32_t version_date;
			uint32_t version_revision;
			uint32_t init_workbuf_size;
			uint32_t per_cmd_appendix_size;
			uint32_t per_subcmd_appendix_size;
			uint32_t reserve[2];
		} info;

		struct {
			uint64_t init_workbuf_va;
			uint32_t init_workbuf_size;
			uint32_t reserve[4];
		} init;
	};
} __packed;

#endif