/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __MTK_MML_TILE_H__
#define __MTK_MML_TILE_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>

#include "mtk-mml.h"
#include "mtk-mml-core.h"
#include "DpTileScaler.h"

struct rdma_tile_data {
	enum mml_color src_fmt;
	u32 blk_shift_w;
	u32 blk_shift_h;
	struct mml_rect crop;
	u32 max_width;
	u16 read_rotate;
};

struct rsz_tile_data {
	bool use_121filter;
	u32 coeff_step_x;
	u32 coeff_step_y;
	u32 precision_x;
	u32 precision_y;
	struct mml_crop crop;
	bool hor_scale;
	enum scaler_algo hor_algo;
	bool ver_scale;
	enum scaler_algo ver_algo;
	s32 c42_out_frame_w;
	s32 c24_in_frame_w;
	s32 prz_out_tile_w;
	s32 prz_back_xs;
	s32 prz_back_xe;
	bool ver_first;
	bool ver_cubic_trunc;
	u32 max_width;
	bool crop_aal_tile_loss;
};

struct tdshp_tile_data {
	bool relay_mode;
	u32 max_width;
};

struct wrot_tile_data {
	enum mml_color dest_fmt;
	u32 rotate;
	bool flip;
	bool alpharot;
	bool racing;
	u8 racing_h;
	bool enable_x_crop;
	bool enable_y_crop;
	bool yuv_pending;
	struct mml_rect crop;
	u32 max_width;
	u8 align_x;
	u8 align_y;
	bool first_x_pad;
	bool first_y_pad;
};

struct dlo_tile_data {
	bool enable_x_crop;
	struct mml_rect crop;
};

union mml_tile_data {
	struct rdma_tile_data rdma;
	struct rsz_tile_data rsz;
	struct tdshp_tile_data tdshp;
	struct wrot_tile_data wrot;
	struct dlo_tile_data dlo;
};

s32 calc_tile(struct mml_task *task, u32 pipe, struct mml_tile_cache *tile_cache);
void destroy_frame_tile(struct mml_frame_tile *frame_tile);
void dump_frame_tile(struct mml_frame_tile *frame_tile);

#endif	/* __MTK_MML_TILE_H__ */
