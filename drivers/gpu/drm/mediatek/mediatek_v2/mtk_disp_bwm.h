/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DISP_BWM_H__
#define __MTK_DISP_BWM_H__

/* define for AFBC_V1_2 */
#define AFBC_V1_2_TILE_W (32)
#define AFBC_V1_2_TILE_H (8)
#define AFBC_V1_2_HEADER_ALIGN_BYTES (1024)
#define AFBC_V1_2_HEADER_SIZE_PER_TILE_BYTES (16)

struct mtk_disp_bwm_data {
	bool is_support_34bits;
	const struct compress_info *compr_info;
	bool fmt_rgb565_is_0;
	unsigned int fmt_uyvy;
	unsigned int fmt_yuyv;
	unsigned int (*aid_sel_mapping)(struct mtk_ddp_comp *comp);
	void __iomem *(*aid_sel_baddr_mapping)(struct mtk_ddp_comp *comp);
	const u32 aid_lye_ofs;
};

struct compress_info {
	/* naming rule: tech_version_MTK_sub-version,
	 * i.e.: PVRIC_V3_1_MTK_1
	 * sub-version is used when compression version is the same
	 * but mtk decoder is different among platforms.
	 */
	const char name[25];

	bool (*l_config)(struct mtk_ddp_comp *comp,
			unsigned int idx, struct mtk_plane_state *state,
			struct cmdq_pkt *handle);
};

#endif

