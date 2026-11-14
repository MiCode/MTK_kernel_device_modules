/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __MTK_APU_HDS_TAG_1_0_H__
#define __MTK_APU_HDS_TAG_1_0_H__

void hds_1_0_pmu_trace(uint64_t inf_id, int64_t sc_idx,
	uint64_t inst_idx, uint64_t inst_type,
	uint64_t dev_idx, uint64_t dev_container,
	uint64_t ts_enque, uint64_t ts_dep_solv,
	uint64_t ts_disp, uint64_t ts_irq_recv);

int hds_1_0_tag_init(void);
void hds_1_0_tag_deinit(void);

#endif
