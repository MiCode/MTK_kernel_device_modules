/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_DISP_DBI_COUNT_H__
#define __MTK_DISP_DBI_COUNT_H__

#include <linux/types.h>
#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_disp_oddmr/mtk_disp_oddmr.h"

struct mtk_dbi_dma_buf {
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	u64 iova;
	void *va;
	uint32_t size;
	uint32_t used;
};

struct mtk_disp_dbi_count_data {
	bool need_bypass_shadow;
	bool is_support_stash;
	uint32_t countr_buffer_size;
	uint32_t countw_buffer_size;
	irqreturn_t (*irq_handler)(int irq, void *dev_id);
	unsigned int stash_lead_time;
	unsigned int min_stash_port_bw;
	bool use_slot_trigger;
};

enum DBI_COUNT_STATE {
	DBI_COUNT_INVALID = 0,
	DBI_COUNT_SW_INIT,
	DBI_COUNT_HW_INIT,
};

enum DBI_COUNT_MODE {
	HW_COUNTING_MODE,
	HW_SAMPLING_MODE,
	CAPTURE_MODE,
	LEGACY_MODE,
};

struct mtk_disp_dbi_count {
	struct mtk_ddp_comp	 ddp_comp;
	const struct mtk_disp_dbi_count_data *data;
	int status;
	int current_mode;
	atomic_t current_count_mode;
	atomic_t new_count_mode;
	uint64_t count_cnt;
	uint32_t current_bl;
	uint32_t current_fps;
	int current_temp;
	bool temp_chg;
	int current_freq;
	atomic_t buffer_full;
	uint32_t buffer_time;
	uint32_t data_fmt;
	struct mtk_dbi_dma_buf count_buffer;
	struct mtk_dbi_count_buf_cfg buffer_cfg;
	struct mtk_drm_dbi_cfg_info count_cfg;
	struct icc_path *qos_req_w;
	struct icc_path *qos_req_w_hrt;
	struct icc_path *qos_req_w_stash;
	struct icc_path *qos_req_r;
	struct icc_path *qos_req_r_hrt;
	struct icc_path *qos_req_r_stash;
	uint32_t qos_srt;
	uint32_t last_qos_srt;
	uint32_t last_hrt;
	uint32_t irq_num;
	uint32_t show_gain;
};

struct dbi_count_block_info {
	uint32_t block_h;
	uint32_t block_v;
	uint32_t channel;
};

int mtk_dbi_count_wait_disable_finish(struct mtk_ddp_comp *comp, void *data);
int mtk_drm_crtc_get_count_fence_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv);
int mtk_dbi_count_delete_timer(struct mtk_ddp_comp *comp, bool need_lock, bool mark_suspend);
int mtk_dbi_count_create_timer(struct mtk_ddp_comp *comp, void *data, bool need_lock, bool update_sec);
int mtk_dbi_count_timer_disable(struct drm_crtc *crtc);
int mtk_dbi_count_timer_enable(struct drm_crtc *crtc);
int mtk_dbi_count_wait_event(struct mtk_ddp_comp *comp, void *data);
int mtk_dbi_count_wait_disable_finish(struct mtk_ddp_comp *comp, void *data);
int mtk_dbi_count_wait_new_frame(struct mtk_ddp_comp *comp, void *data);
void mtk_crtc_dbi_count_init(struct mtk_drm_crtc *mtk_crtc);
void mtk_crtc_dbi_count_cfg(struct mtk_drm_crtc *mtk_crtc, struct mtk_crtc_state *crtc_state, struct cmdq_pkt *handle);
void mtk_crtc_dbi_count_pre_cfg(struct mtk_drm_crtc *mtk_crtc, struct mtk_crtc_state *crtc_state);

int mtk_dbi_count_load_buffer(struct mtk_ddp_comp *comp, void *data);
void mtk_dbi_count_hrt_cal(struct drm_device *dev, int disp_idx,
	uint32_t en, uint32_t slice_size, uint32_t slice_num,
	uint32_t block_h, uint32_t block_v, int *oddmr_hrt);
int mtk_dbi_count_clear_event(struct mtk_ddp_comp *comp, void *data);
int mtk_dbi_count_check_buffer(struct mtk_ddp_comp *comp, void *data);

void mtk_dbi_count_bypass(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle);
void mtk_oddmr_dbi_count_clk_off(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle);

struct dbi_count_block_info mtk_dbi_count_get_block_info(uint32_t block_h, uint32_t block_v);
void mtk_dbi_idle_count_insert_wb_fence(struct mtk_drm_crtc *mtk_crtc, unsigned int fence);
void mtk_dbi_idle_count_update_wb_fence(struct mtk_drm_crtc *mtk_crtc);

#endif
