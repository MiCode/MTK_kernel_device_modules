/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#ifndef __MTK_DSI_LPC_H__
#define __MTK_DSI_LPC_H__

bool mtk_dsi_lpc_en(void);
void mtk_dsi_lpc_init_config(struct drm_crtc *crtc);
void mtk_dsi_lpc_hwvsync_en(bool en, int index);
void mtk_dsi_lpc_sof_ts(long long *sof_ts, struct mtk_drm_crtc *mtk_crtc);
void mtk_dsi_lpc_set_interrupt_enable(struct mtk_drm_crtc *mtk_crtc);
void mtk_dsi_lpc_resync_ts(long long *resync_ts, struct mtk_drm_crtc *mtk_crtc);
void mtk_dsi_lpc_update_panel_params(struct mtk_drm_crtc *mtk_crtc,
	struct mtk_ddp_comp *comp, struct cmdq_pkt *cmdq_handle,
	struct mtk_panel_params *params);
#endif
