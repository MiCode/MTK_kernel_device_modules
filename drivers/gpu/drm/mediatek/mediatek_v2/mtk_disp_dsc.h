/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DISP_DSC_H__
#define __MTK_DISP_DSC_H__
/* O7 code for HQ-415208 by p-wangfengbin at 2024/10/28 start */
struct mtk_disp_dsc_data {
	bool support_shadow;
	bool need_bypass_shadow;
	bool need_obuf_sw;
	bool dsi_buffer;
	unsigned int shadow_ctrl_reg;
	bool decrease_outstream_buf;
	bool reset_after_eof;
#ifdef CONFIG_BUILD_TARGET_TANZANITE
	bool need_dsc_reset;
#endif
};
#ifdef CONFIG_BUILD_TARGET_TANZANITE
void mtk_dsc_sw_rst(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle);
#endif
/* O7 code for HQ-415208 by p-wangfengbin at 2024/10/28 end */
#endif