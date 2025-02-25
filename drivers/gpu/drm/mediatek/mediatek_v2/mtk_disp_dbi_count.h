/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_DISP_DBI_COUNT_H__
#define __MTK_DISP_DBI_COUNT_H__

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"

struct mtk_dbi_dma_buf {
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;

	u64 iova;
	void *va;
};

int mtk_dbi_count_wait_disable_finish(struct drm_crtc *crtc, void *data);
int mtk_drm_crtc_get_count_fence_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv);
int mtk_dbi_count_delete_timer(struct drm_crtc *crtc, bool need_lock, bool mark_suspend);
int mtk_dbi_count_create_timer(struct drm_crtc *crtc, void *data, bool need_lock, bool update_sec);
int mtk_dbi_count_timer_disable(struct drm_crtc *crtc);
int mtk_dbi_count_timer_enable(struct drm_crtc *crtc);
int mtk_dbi_count_wait_event(struct drm_crtc *crtc, void *data);
int mtk_dbi_count_wait_disable_finish(struct drm_crtc *crtc, void *data);
int mtk_dbi_count_wait_new_frame(struct drm_crtc *crtc, void *data);
void mtk_crtc_dbi_count_init(struct mtk_drm_crtc *mtk_crtc);
void mtk_crtc_dbi_count_cfg(struct mtk_drm_crtc *mtk_crtc, struct mtk_crtc_state *crtc_state);
void mtk_crtc_dbi_count_release_fence(struct mtk_drm_crtc *mtk_crtc);
int mtk_dbi_count_load_buffer(struct drm_crtc *crtc,void *data);
void mtk_dbi_count_hrt_cal(uint32_t en, uint32_t slice_size,
	uint32_t slice_num, uint32_t block_h, uint32_t block_v, int *oddmr_hrt);
int mtk_dbi_count_clear_event(struct drm_crtc *crtc, void *data);
int mtk_dbi_count_check_buffer(struct drm_crtc *crtc, void *data);

#endif
