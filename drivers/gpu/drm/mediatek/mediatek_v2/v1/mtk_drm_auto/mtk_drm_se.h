/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef MTK_DRM_SE_H
#define MTK_DRM_SE_H

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_HOST)
void mtk_drm_se_init(void);
void mtk_drm_se_crtc_init(struct mtk_drm_crtc *mtk_crtc);

void mtk_drm_se_check_plane(struct drm_crtc *crtc);
void mtk_drm_se_plane_update(struct drm_crtc *crtc, struct drm_plane *plane,
	struct mtk_plane_state *plane_state);


int mtk_drm_se_get_info_ioctl(struct drm_device *dev, void *data);
int mtk_drm_set_ovl_layer(struct drm_device *dev, void *data,
			 struct drm_file *file_priv);
int mtk_drm_map_dma_buf(struct drm_device *dev, void *data,
			 struct drm_file *file_priv);
int mtk_drm_unmap_dma_buf(struct drm_device *dev, void *data,
			 struct drm_file *file_priv);

extern int mtk_drm_get_panel_info(struct drm_device *dev,
			     struct drm_mtk_session_info *info, unsigned int crtc_id);
#endif

#endif /* MTK_DRM_DRV_H */
