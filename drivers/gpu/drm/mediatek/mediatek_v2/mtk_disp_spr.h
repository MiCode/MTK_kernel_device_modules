/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_DISP_SPR_H__
#define __MTK_DISP_SPR_H__

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"

struct mtk_disp_spr_tile_overhead_v {
	unsigned int top_overhead_v;
	unsigned int bot_overhead_v;
	unsigned int comp_overhead_v;
};
int mtk_spr_check_postalign_status(struct mtk_drm_crtc *mtk_crtc);

enum SPR_IP_TYPE {
	DISP_MTK_SPR = 0,
	DISP_NVT_SPR,
};

struct mtk_drm_spr_share_info {
	unsigned int spr_ip_type;
	unsigned int spr_hw_enable;
	unsigned int backup_reg_size;
	unsigned int backup_spr_reg_size;
	unsigned int backup_reg_pa;
	unsigned int backup_value_pa;
	unsigned int panel_width;
	unsigned int panel_height;
};

unsigned int mtk_spr_get_format(struct mtk_drm_crtc *mtk_crtc);
int mtk_spr_check_postalign_status(struct mtk_drm_crtc *mtk_crtc);
bool mtk_drm_spr_backup(struct drm_crtc *crtc, void *get_phys, void *get_virt,
	unsigned int offset, unsigned int size);

#endif
