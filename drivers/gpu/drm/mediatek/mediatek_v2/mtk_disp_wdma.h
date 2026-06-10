/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DISP_WDMA_H__
#define __MTK_DISP_WDMA_H__

struct mtk_disp_wdma_data {
	/* golden setting */
	unsigned int fifo_size_1plane;
	unsigned int fifo_size_uv_1plane;
	unsigned int fifo_size_2plane;
	unsigned int fifo_size_uv_2plane;
	unsigned int fifo_size_3plane;
	unsigned int fifo_size_uv_3plane;
	unsigned int force_ostdl_bw;
	unsigned int buf_con1_fld_fifo_pseudo_size;
	unsigned int buf_con1_fld_fifo_pseudo_size_uv;
	u32 bus_priority_mask;
	u8 stash_leading_time;

	void (*sodi_config)(struct drm_device *drm, enum mtk_ddp_comp_id id,
			    struct cmdq_pkt *handle, void *data);
	unsigned int (*aid_sel)(struct mtk_ddp_comp *comp);
	resource_size_t (*check_wdma_sec_reg)(struct mtk_ddp_comp *comp);
	bool support_shadow;
	bool need_bypass_shadow;
	bool is_support_34bits;
	bool is_support_ufbc;
	bool use_larb_control_sec;
	bool skip_secure;
	bool (*is_right_wdma_comp)(struct mtk_ddp_comp *comp);
	void (*aid_sel_manual)(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle);
	void (*sec_set)(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle, bool sec);
	void (*sec_aid_config)(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle, bool sec);
	void (*ddr_config)(struct mtk_ddp_comp *comp, struct golden_setting_context *gsc,
		struct cmdq_pkt *handle);
};

#endif
