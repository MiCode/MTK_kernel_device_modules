/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef MTK_DRM_DRV_AUTO_H
#define MTK_DRM_DRV_AUTO_H

#include <drm/drm_fb_helper.h>
#include <uapi/drm/mediatek_drm.h>
#include <linux/types.h>
#include <linux/io.h>
#include <drm/drm_atomic.h>

#include "../mtk_drm_ddp_comp.h"

extern const struct mtk_crtc_path_data mt6991_mtk_main_path_data;

extern const struct mtk_crtc_path_data mt6991_mtk_main_full_set_data;

extern const struct mtk_crtc_path_data mt6991_mtk_ext_path_data;

extern const struct mtk_crtc_path_data mt6991_mtk_dp_w_tdshp_path_data;

extern const struct mtk_crtc_path_data mt6991_mtk_dp_wo_tdshp_path_data;

extern const struct mtk_crtc_path_data mt6991_mtk_secondary_path_data;

extern const struct mtk_crtc_path_data mt6991_mtk_discrete_path_data;

extern const struct mtk_crtc_path_data mt6991_mtk_fifth_path_data;

extern const struct mtk_crtc_path_data mt6991_mtk_sixth_path_data;

extern const struct mtk_crtc_path_data mt6991_mtk_seventh_path_data;

int mtk_drm_pm_notifier(struct notifier_block *notifier, unsigned long pm_event, void *unused);

#endif /* MTK_DRM_DRV_AUTO_H */
