// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <hds.h>
#include <hds_plat.h>
#include <hds_plat_1_0_tag.h>

/* PMU buf size */
/*
 * PMU buffer format
 * +-----------------------------------------------------------+
 * | inf_id(8)                                                 |
 * | num_inst(4)                 | reserved(4)                 |
 * | sw_info_offset(8)                                         |
 * | pmu_log_offset(8)                                         |
 * +-----------------------------------------------------------+
 * | sc_idx(4)                   | inst_idx(2)  | inst_type(2) |
 * | sc_idx(4)                   | inst_idx(2)  | inst_type(2) |
 * | ...                         |                             |
 * +-----------------------------------------------------------+
 * | hw pmu format                                             |
 * +-----------------------------------------------------------+
 */
struct hds_plat_1_0_pmu_header {
	uint64_t inf_id;
	uint32_t num_inst;
	uint32_t reserve;
	uint64_t sw_info_offset;
	uint64_t pmu_log_offset;
} __packed;
struct hds_plat_1_0_pmu_sw_info {
	int32_t sc_idx;
	uint16_t inst_idx;
	uint16_t inst_type;
} __packed;
struct hds_plat_1_0_pmu_log {
	uint32_t dev_idx;
	uint32_t dev_container; //0x4 - 0x8
	uint64_t ts_disp;  //0x8 - 0x10
	uint64_t reserve0; //0x10-0x8
	uint64_t ts_enque; //0x18-0x20
	uint64_t reserve1; //0x20-0x28
	uint64_t ts_solv_dep; //0x28-0x30
	uint64_t reserve2; //0x30
	uint64_t ts_int_recv;
} __packed;
//--------------------------------------------------------------
static int hds_1_0_plat_init(struct apu_hds_device *hdev)
{
	hds_1_0_tag_init();

	return 0;
}

static void hds_1_0_plat_deinit(struct apu_hds_device *hdev)
{
	hds_1_0_tag_deinit();
}

static int hds_1_0_cmd_postprocess_late(struct apu_hds_device *hdev, void *va, uint32_t size,
	uint32_t power_plcy)
{
	struct hds_plat_1_0_pmu_header *pmu_header = (struct hds_plat_1_0_pmu_header *)va;
	struct hds_plat_1_0_pmu_sw_info *pmu_sw_info = NULL;
	struct hds_plat_1_0_pmu_log *pmu_log = NULL;
	uint64_t num_inst = 0, idx = 0;
	/* boundary check */
	if (size < sizeof(*pmu_header)) {
		apu_hds_debug("appendix buffer(%u/%lu) check header size invalid\n", size, sizeof(*pmu_header));
		return 0;
	}

	if (!hdev->pmu_tag_en) {
		apu_hds_debug("clear hds buffer only\n");
		goto clear_buffer;
	}

	/* assign */
	pmu_sw_info = (struct hds_plat_1_0_pmu_sw_info *)(va + (uint64_t)pmu_header->sw_info_offset);
	pmu_log = (struct hds_plat_1_0_pmu_log *)(va + (uint64_t)pmu_header->pmu_log_offset);
	num_inst = pmu_header->num_inst;

	apu_hds_debug("appendix buffer(%pK/%u) offset(0x%llx/0x%llx) num_inst(%llu)\n",
		va, size,
		pmu_header->sw_info_offset, pmu_header->pmu_log_offset,
		num_inst);

	/* boundary check */
	if ((uint64_t)pmu_sw_info + num_inst * sizeof(*pmu_sw_info) > (uint64_t)va + size ||
		(uint64_t)pmu_log + num_inst * sizeof(*pmu_log) > (uint64_t)va + size) {
		apu_hds_err("appendix buffer(0x%llx/%u) check boundary num(%llu) failed\n",
			(uint64_t)va, size, num_inst);
		return -EINVAL;
	}

	/* output tag */
	for (idx = 0; idx < num_inst; idx++) {
		hds_1_0_pmu_trace(pmu_header->inf_id, pmu_sw_info[idx].sc_idx,
			(uint64_t)pmu_sw_info[idx].inst_idx, (uint64_t)pmu_sw_info[idx].inst_type,
			pmu_log[idx].dev_idx, pmu_log[idx].dev_container,
			pmu_log[idx].ts_enque, pmu_log[idx].ts_solv_dep,
			pmu_log[idx].ts_disp, pmu_log[idx].ts_int_recv);

		apu_hds_debug("inf_id(0x%llx) sc_idx(%d) inst_idx(%u) inst_type(%u) dev_idx(0x%x) dev_container(0x%x) ts(0x%llx/0x%llx/0x%llx/0x%llx)\n",
			pmu_header->inf_id, pmu_sw_info[idx].sc_idx,
			pmu_sw_info[idx].inst_idx, pmu_sw_info[idx].inst_type,
			pmu_log[idx].dev_idx, pmu_log[idx].dev_container,
			pmu_log[idx].ts_enque, pmu_log[idx].ts_solv_dep,
			pmu_log[idx].ts_disp, pmu_log[idx].ts_int_recv);
	}

clear_buffer:
	/* clear pmu buffer */
	memset(va, 0, size);
	return 0;
}

struct hds_plat_func hds_hw_plat_func_1_0 = {
	.plat_init = hds_1_0_plat_init,
	.plat_deinit = hds_1_0_plat_deinit,
	.cmd_postprocess_late = hds_1_0_cmd_postprocess_late,
};
