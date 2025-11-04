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
	if (plane_index < OVL_LAYER_NR)
		return plane_index;
	else if (plane_index < (OVL_LAYER_NR + EXTERNAL_INPUT_LAYER_NR))
		return plane_index - OVL_LAYER_NR;
	else if (plane_index < (OVL_LAYER_NR + EXTERNAL_INPUT_LAYER_NR + MEMORY_INPUT_LAYER_NR))
		return plane_index - OVL_LAYER_NR - EXTERNAL_INPUT_LAYER_NR;
	else if (plane_index < MAX_PLANE_NR)
		return plane_index - OVL_LAYER_NR - EXTERNAL_INPUT_LAYER_NR - MEMORY_INPUT_LAYER_NR;
	else
		return 0;

}
#else
unsigned int to_crtc_plane_index(unsigned int plane_index)
{
	DDPINFO("%s plane index %d local_index 0\n", __func__, plane_index);

	return 0;
}
#endif
#endif

