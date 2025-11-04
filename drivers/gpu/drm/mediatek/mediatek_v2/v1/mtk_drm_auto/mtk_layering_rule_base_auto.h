/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_LAYERING_RULE_BASE_AUTO__
#define __MTK_LAYERING_RULE_BASE_AUTO__

#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_gem.h>
#include <uapi/drm/mediatek_drm.h>
#include <drm/drm_modes.h>

int mtk_lye_get_exdma_comp_id_auto(int disp_idx, int layer_idx,
			     struct drm_device *drm_dev, int fun_lye, int rsz_lye);

void mtk_register_layering_rule_ops_for_auto(struct layering_rule_ops *ops,
				    struct layering_rule_info_t *info);


#endif
