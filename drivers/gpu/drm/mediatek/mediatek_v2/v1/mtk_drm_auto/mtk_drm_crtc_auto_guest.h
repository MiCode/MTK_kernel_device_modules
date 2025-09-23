/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef MTK_DRM_CRTC_AUTO_GUEST_H
#define MTK_DRM_CRTC_AUTO_GUEST_H

#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/pm_wakeup.h>
#include <linux/timer.h>

#include <drm/drm_crtc.h>

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_GUEST)
#define CONFIG_MTK_DISP_NO_LK

struct mtk_crtc_fence_mon_node {
	struct list_head link;
	unsigned int crtc_id;
	unsigned int fence_idx;
	struct dma_fence *fence;
	unsigned int session_id;
	unsigned int timeline_id;
};

void mtk_drm_crtc_disable_virtio(struct drm_crtc *crtc);
void mtk_drm_crtc_enable_virtio(struct drm_crtc *crtc);

void mtk_drm_crtc_register_irq(struct mtk_drm_private *priv,
			       struct mtk_drm_crtc *mtk_crtc, int pipe);
int mtk_drm_get_host_crtc_obj_id(struct drm_device *dev, void *data, struct drm_file *file_priv);
void mtk_crtc_set_pf_fence_to_monitor(struct drm_crtc *crtc, unsigned int fence_idx);
void mtk_crtc_set_rel_fence_to_monitor(struct mtk_drm_private *priv, unsigned int session_id,
	unsigned int timeline_id, unsigned int fence_idx);
int mtk_drm_monitor_rel_fence_thread(void *data);
int mtk_drm_monitor_pf_fence_thread(void *data);
void mtk_get_fence_from_timeline(unsigned int session_id, unsigned int timeline_id,
	unsigned int index, struct dma_fence **fence);
int mtk_lye_get_exdma_comp_id_for_auto(int disp_idx, int layer_idx,
	struct drm_device *drm_dev, int fun_lye, int rsz_lye);
void clear_layer_for_two_android_layer(struct drm_mtk_layering_info *disp_info,
	struct drm_device *drm_dev);
void init_layer_mapping_table(enum HRT_TB_TYPE hrt_type, u32 layer_nr)
#endif


#endif /* MTK_DRM_CRTC_AUTO_GUEST_H */
