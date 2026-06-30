/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __MTK_DISP_CHIST_EXT_H__
#define __MTK_DISP_CHIST_EXT_H__

#include <linux/uaccess.h>
#include <uapi/drm/mediatek_drm.h>
#include "mtk_drm_ddp_comp.h"

#define DISP_CHIST_CHANNEL_COUNT 7
#define DISP_CHIST_COUNT 2
#define CHIST_CHANNEL_CONFIG_MAX 14
#define BUFFER_SIZE 14512  //(1+3*7+256*7)*4*2

struct mtk_disp_chist_primary_ext {
	struct mtk_disp_block_config block_config[DISP_CHIST_CHANNEL_COUNT];
	struct drm_mtk_channel_config chist_config[DISP_CHIST_CHANNEL_COUNT];
	struct drm_mtk_channel_hist_ext disp_hist[CHIST_CHANNEL_CONFIG_MAX];

	atomic_t irq_event;
	struct wait_queue_head event_wq;
	spinlock_t power_lock;
	spinlock_t data_lock;
	atomic_t clock_on;
	bool need_restore;
	unsigned int present_fence;
	unsigned int pipe_width;
	unsigned int pre_frame_width;
	unsigned int frame_width;
	unsigned int frame_height;
};

struct mtk_disp_chist_ext {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	const struct mtk_disp_chist_data *data;
	unsigned int tile_overhead;
	bool is_right_pipe;
	int path_order;
	struct mtk_ddp_comp *companion;
	struct mtk_disp_chist_primary_ext *primary_data;
};

static inline struct mtk_disp_chist_ext *comp_to_chist_ext(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_chist_ext, ddp_comp);
}

int mtk_drm_ioctl_chist_get_hist(struct drm_device *dev, void *data,
	struct drm_file *file_priv);

int mtk_drm_ioctl_chist_get_caps(struct drm_device *dev, void *data,
	struct drm_file *file_priv);

int mtk_drm_ioctl_chist_set_config(struct drm_device *dev, void *data,
	struct drm_file *file_priv);

void disp_chist_set_tile_overhead(struct mtk_drm_crtc *mtk_crtc, int overhead, bool is_right);
int disp_chist_probe_ext(struct platform_device *pdev);
int disp_chist_remove_ext(struct platform_device *pdev);

#endif

