/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

 #ifndef __BRIDGE_SERDES_H__
 #define __BRIDGE_SERDES_H__

#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <drm/drm_bridge.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"

struct vdo_timing {
	u32 width;
	u32 height;
	u32 hfp;
	u32 hsa;
	u32 hbp;
	u32 vfp;
	u32 vsa;
	u32 vbp;
	u32 fps;
	u32 pll;
	u32 prefetch;
	u32 physcial_w;
	u32 physcial_h;
};

struct priv_panel_data {
	struct drm_panel panel;
	struct mtk_panel_params ext_params;
};

#endif
