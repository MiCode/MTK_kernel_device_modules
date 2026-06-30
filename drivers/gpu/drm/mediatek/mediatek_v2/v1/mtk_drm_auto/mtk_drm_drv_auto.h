/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef MTK_DRM_DRV_AUTO_H
#define MTK_DRM_DRV_AUTO_H

#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/pm_wakeup.h>
#include <linux/timer.h>

#include <drm/drm_crtc.h>

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO)
#if !IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_HOST)
#if !IS_ENABLED (CONFIG_DRM_MEDIATEK_AUTO_GUEST)
	#define MTK_DRM_MEDIATEK_AUTO_AN_ONLY 1
#endif
#endif
#endif

extern struct mtk_crtc_path_data *mtk_disp_crtc_path_data[];

extern struct virtio_disp_rsp_crtc_path_info mt6991_mtk_an_crtc_path_info;

int mtk_drm_path_data_update(struct mtk_drm_private *private);
int mtk_drm_path_data_update_from_host(struct mtk_drm_private *private);
int mtk_drm_path_crtc_create(struct drm_device *drm);

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_GUEST)
extern bool mtk_drm_find_comp(struct mtk_ddp_comp *comp,
			      const struct mtk_crtc_path_data *path_data);
int mtk_drm_path_find_crtc(struct mtk_ddp_comp *comp);
bool mtk_drm_path_is_shared_device(struct mtk_ddp_comp *comp);

int mtk_drm_pm_ctl_auto_guest(struct mtk_drm_private *priv, enum disp_pm_action action);
void mtk_drm_get_top_clk_auto_guest(struct mtk_drm_private *priv);
void mtk_drm_top_clk_prepare_enable_auto_guest(struct drm_crtc *crtc);
void mtk_drm_top_clk_disable_unprepare_auto_guest(struct drm_crtc *crtc);
bool mtk_drm_top_clk_isr_get_auto_guest(struct mtk_ddp_comp *comp);
void mtk_drm_top_clk_isr_put_auto_guest(struct mtk_ddp_comp *comp);
#endif

#endif /* MTK_DRM_CRTC_AUTO_H */
