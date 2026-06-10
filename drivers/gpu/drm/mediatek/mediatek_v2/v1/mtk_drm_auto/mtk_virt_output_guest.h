/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef MTK_VIRT_OUTPUT_H
#define MTK_VIRT_OUTPUT_H

#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_panel.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_encoder.h>
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_crtc.h"
#include "mtk_panel_ext.h"
#include "linux/workqueue.h"

/* all virt-path is internal, no support hotplug */
#define MTK_VIRT_WITH_NO_HOTPLUG
#ifndef MAX_PANEL_NAME_LEN
#define MAX_PANEL_NAME_LEN 64
#endif

enum MTK_VIRT_TYPE {
	MTK_VIRT_DSI,
	MTK_VIRT_DPI,
	MTK_VIRT_DVO,
};

enum MTK_DDP_OUT_ID {
	MTK_DISP_DSI0,
	MTK_DISP_DSI1,
	MTK_DISP_DSI2,
	MTK_DISP_DSI2_1,
	MTK_DISP_DP0,
	MTK_DISP_DP1,
	MTK_DISP_EDP,
	MTK_DISP_MAX
};

enum VIRT_DPTX_STATE {
	VIRT_DPTX_STATE_NO_DEVICE,
	VIRT_DPTX_STATE_ACTIVE,
};

struct hotplug_work {
	struct work_struct work_hotplug;
	unsigned int event;
	bool init;
};

struct mtk_virt {
	struct mtk_ddp_comp ddp_comp;
	struct drm_encoder encoder;
	struct drm_connector connector;
	struct drm_panel *panel;
	struct drm_bridge *bridge;
	struct device *dev;
	struct drm_device *drm_dev;
	struct drm_display_mode mode;
	struct mtk_panel_ext *ext;
	struct mtk_drm_connector_caps conn_caps;
	enum MTK_VIRT_TYPE type;
	int alias;
	bool virt_ready;
	#if IS_ENABLED(CONFIG_MTK_VIRTIO_DISP)
	bool is_shared_device;
	bool is_first_path;
	#endif
	/* host notify guest hotplug */
	struct hotplug_work hotplug_work_dp;
	/* used wait guest suspend first */
	struct workqueue_struct *hotplug_wq;
	atomic_t hotplug_done;
	wait_queue_head_t guest_suspend;
	char panel_name[MAX_PANEL_NAME_LEN];
};
void mtk_set_hotplug_status(int suspend_done);
int mtk_virt_get_panel_size(enum MTK_DDP_OUT_ID id,
	uint32_t *hdisplay, uint32_t *vdisplay);
struct mtk_ddp_comp *mtk_virt_get_all_output(int *count, int *con_id);

#endif
