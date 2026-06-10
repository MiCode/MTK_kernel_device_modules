/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef MTK_DRM_DDP_COMP_AUTO_H
#define MTK_DRM_DDP_COMP_AUTO_H

#include <linux/io.h>
#include <linux/kernel.h>
#include "mtk_drm_ddp_comp.h"

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO)
#define PHY_COMP		0
#define VIRT_COMP		1
#define SHARE_COMP		3

#define DSI0_MAC0_ULTRA				0
#define DSI1_MAC0_ULTRA				1
#define DSI1_MAC1_ULTRA				2
#define DSI2_MAC0_ULTRA				3
#define DP_INTF0_ULTRA				4
#define DP_INTF1_ULTRA				5
#define DVO0_ULTRA				6

enum mtk_ddp_lk_comp_id {
	DDP_LK_DSI0,
	DDP_LK_DP0,
	DDP_LK_WB0,
	DDP_LK_EDP,
	DDP_LK_DSI1,
	DDP_LK_DSI2,
	DDP_LK_DSI2_1,
	DDP_LK_DP1,
	DDP_LK_MAX,
};


extern unsigned int mtk_disp_num_from_atag(void);
bool mtk_ddp_comp_check_output_comp(enum mtk_ddp_comp_id virt_id,
				    enum mtk_ddp_comp_id phy_id);
enum mtk_ddp_comp_id mtk_ddp_comp_get_virt_output_comp(enum mtk_ddp_comp_id phy_id);
enum mtk_ddp_comp_id mtk_ddp_comp_get_phy_output_comp(enum mtk_ddp_comp_id virt_id);

bool mtk_ddp_comp_is_rdma(struct mtk_ddp_comp *comp);
bool mtk_ddp_comp_is_rdma_by_id(enum mtk_ddp_comp_id id);
bool mtk_ddp_comp_is_virt(struct mtk_ddp_comp *comp);
bool mtk_ddp_comp_is_virt_by_id(struct mtk_drm_private *private, enum mtk_ddp_comp_id id);
int mtk_ddp_comp_is_layer_on(struct mtk_ddp_comp *comp);
bool mtk_ddp_comp_is_comp_out_cb_by_id(enum mtk_ddp_comp_id id);
void mtk_ddp_comp_init_type(struct mtk_drm_private *private, enum mtk_ddp_comp_id id, int comp_type);
bool mtk_ddp_comp_is_share_comp(struct mtk_ddp_comp *comp);
void mtk_drm_notify_to_android(int dev_id, unsigned int event);
enum mtk_ddp_lk_comp_id mtk_ddp_comp_map_lk_id(enum mtk_ddp_comp_id id);
bool mtk_ddp_comp_is_enable_from_lk(enum mtk_ddp_comp_id id);

void mtk_ddp_comp_exdma_ultra_sel_config(struct mtk_drm_crtc *mtk_crtc);
enum mtk_ddp_comp_id mtk_ddp_comp_get_map_id(u32 comp_id);

#endif
#endif /* MTK_DRM_DDP_COMP_AUTO_H */
