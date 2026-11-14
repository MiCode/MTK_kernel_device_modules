/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#ifndef __MTK_DSI_LPC_H__
#define __MTK_DSI_LPC_H__

void mtk_dsi_lpc_for_debug_config(struct mtk_drm_crtc *mtk_crtc, struct cmdq_pkt *cmdq_handle);
void mtk_dsi_lpc_set_te_en(struct mtk_drm_crtc *mtk_crtc, bool en);

#endif
