/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#ifndef __MTK_DISP_VDISP_AO_H__
#define __MTK_DISP_VDISP_AO_H__


struct mtk_vdisp_ao_irq_cfg {
	u32 offset;
	u8 shift;
	u16 value;
};

struct vdisp_ao_data {
	int ao_int_config;
	int irq_count;
	struct mtk_vdisp_ao_irq_cfg *irq_cfg;
};

struct mtk_vdisp_ao {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	const struct vdisp_ao_data *data;
};

void mtk_vdisp_ao_irq_config_MT6991(struct drm_device *drm);
void mtk_vdisp_ao_irq_config_MT6993(struct drm_device *drm);
void mtk_vdisp_ao_qos_config_MT6993(struct drm_device *drm);
void mtk_vdisp_ao_for_debug_config(struct mtk_drm_crtc *mtk_crtc, struct cmdq_pkt *cmdq_handle);

#endif
