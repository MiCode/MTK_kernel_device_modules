// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_gem_atomic_helper.h>
#include <drm/drm_blend.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_plane.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_fourcc.h>
#include <linux/mailbox_controller.h>

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO)

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_plane.h"

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_GUEST)
unsigned int to_crtc_plane_index(unsigned int plane_index)
{
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct drm_plane *base_plane;
	unsigned int plane_index;

	if (!plane) {
		DDPMSG("[E]%s invalid plane %p\n", __func__, plane);
		return 0;
	}

	if (plane->crtc) {
		crtc = plane->crtc;
	} else if (plane->state && plane->state->crtc) {
		crtc = plane->state->crtc;
	} else if (plane->dev) {
		drm_for_each_crtc(crtc, plane->dev) {
			if (plane->possible_crtcs & drm_crtc_mask(crtc))
				break;
		}
	} else {
		DDPMSG("[E]%s invalid plane %p %d crtc\n", __func__, plane, plane->index);
		return 0;
	}

	mtk_crtc = to_mtk_crtc(crtc);

	base_plane = &mtk_crtc->planes[0].base;

	plane_index = plane->index - base_plane->index;

	DDPINFO("%s crtc %d plane index %d %d possible_crtcs 0x%X\n",
		__func__, drm_crtc_index(crtc), plane->index, plane_index,
		plane->possible_crtcs);

	return plane_index;
}
#else
unsigned int to_crtc_plane_index(unsigned int plane_index)
{
	DDPINFO("%s plane index %d local_index 0\n", __func__, plane_index);

	return 0;
}
#endif
#endif

