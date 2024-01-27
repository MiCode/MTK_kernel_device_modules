// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Chris-YC Chen <chris-yc.chen@mediatek.com>
 */
#include "tile_driver.h"
#include "tile_mdp_func.h"
#include "mtk-mml-color.h"

enum isp_tile_message tile_rdma_init(struct tile_func_block *func,
				     struct tile_reg_map *reg_map)
{
	struct rdma_tile_data *data = &func->data->rdma;

	if (unlikely(!data))
		return MDP_MESSAGE_NULL_DATA;

	/* Specific constraints implied by different formats */

	/* In tile constraints
	 * Input Format | Tile Width
	 * -------------|-----------
	 *   Block mode | L
	 *       YUV420 | L
	 *       YUV422 | L * 2
	 * YUV444/RGB/Y | L * 4
	 */
	if (MML_FMT_AFBC_ARGB(data->src_fmt)) {
		/* For AFBC mode end x may be extend to block size
		 * and may exceed max tile width 640. So reduce width
		 * to prevent it.
		 */
		func->in_tile_width = ((data->max_width >> 5) - 1) << 5;
	} else if (MML_FMT_AFBC_YUV(data->src_fmt)) {
		func->in_tile_width = ((data->max_width >> 4) - 1) << 4;
	} else if (MML_FMT_HYFBC(data->src_fmt)) {
		/* For HyFBC block size 32x16, so tile rule same as RGB AFBC */
		func->in_tile_width = ((data->max_width >> 5) - 1) << 5;
	} else if (MML_FMT_BLOCK(data->src_fmt)) {
		func->in_tile_width = (data->max_width >> 6) << 6;
	} else if (MML_FMT_YUV420(data->src_fmt)) {
		func->in_tile_width = data->max_width;
	} else if (MML_FMT_YUV422(data->src_fmt)) {
		func->in_tile_width = data->max_width * 2;
	} else {
		func->in_tile_width = data->max_width * 4;
	}

	if (MML_FMT_H_SUBSAMPLE(data->src_fmt)) {
		/* YUV422 or YUV420 */
		/* Tile alignment constraints */
		func->in_const_x = 2;

		if (MML_FMT_V_SUBSAMPLE(data->src_fmt) &&
		    !MML_FMT_INTERLACED(data->src_fmt)) {
			/* YUV420 */
			func->in_const_y = 2;
		}
	}

	if (MML_FMT_10BIT_PACKED(data->src_fmt) &&
	    !MML_FMT_COMPRESS(data->src_fmt) &&
	    !MML_FMT_ALPHA(data->src_fmt) &&
	    !MML_FMT_BLOCK(data->src_fmt)) {
		/* 10-bit packed, not compress, not alpha 32-bit, not blk */
		func->in_const_x = 4;
	}

	func->in_tile_height = 65535;
	func->out_tile_height = 65535;

	func->crop_bias_x = data->crop.left;
	func->crop_bias_y = data->crop.top;

	return ISP_MESSAGE_TILE_OK;
}

enum isp_tile_message tile_rrot_init(struct tile_func_block *func,
				     struct tile_reg_map *reg_map)
{
	struct rdma_tile_data *data = &func->data->rdma;

	if (unlikely(!data))
		return MDP_MESSAGE_NULL_DATA;

	/* Specific constraints implied by different formats */

	/* In tile constraints
	 * Input Format | Tile Width
	 * -------------|-----------
	 *   Block mode | L
	 *       YUV420 | L
	 *       YUV422 | L * 2
	 * YUV444/RGB/Y | L * 4
	 */
	if (MML_FMT_AFBC_ARGB(data->src_fmt)) {
		/* For AFBC mode end x may be extend to block size
		 * and may exceed max tile width 640. So reduce width
		 * to prevent it.
		 */
		func->in_tile_width = ((data->max_width >> 5) - 1) << 5;
	} else if (MML_FMT_AFBC_YUV(data->src_fmt)) {
		func->in_tile_width = ((data->max_width >> 4) - 1) << 4;
	} else if (MML_FMT_HYFBC(data->src_fmt)) {
		/* For HyFBC block size 32x16, so tile rule same as RGB AFBC */
		func->in_tile_width = ((data->max_width >> 5) - 1) << 5;
	} else if (MML_FMT_BLOCK(data->src_fmt)) {
		func->in_tile_width = (data->max_width >> 6) << 6;
	} else {
		func->in_tile_width = data->max_width;
	}

	if (MML_FMT_H_SUBSAMPLE(data->src_fmt)) {
		/* YUV422 or YUV420 */
		/* Tile alignment constraints */
		func->in_const_x = 2;

		if (MML_FMT_V_SUBSAMPLE(data->src_fmt) &&
		    !MML_FMT_INTERLACED(data->src_fmt)) {
			/* YUV420 */
			func->in_const_y = 2;
		}
	}

	if (MML_FMT_10BIT_PACKED(data->src_fmt) &&
	    !MML_FMT_COMPRESS(data->src_fmt) &&
	    !MML_FMT_ALPHA(data->src_fmt) &&
	    !MML_FMT_BLOCK(data->src_fmt)) {
		/* 10-bit packed, not compress, not alpha 32-bit, not blk */
		func->in_const_x = 4;
	}

	func->in_tile_height = 65535;
	func->out_tile_height = 65535;
	if (data->read_rotate == MML_ROT_90 || data->read_rotate == MML_ROT_270)
		swap(func->in_const_x, func->in_const_y);

	func->crop_bias_x = data->crop.left;
	func->crop_bias_y = data->crop.top;

	return ISP_MESSAGE_TILE_OK;
}

enum isp_tile_message tile_prz_init(struct tile_func_block *func,
				    struct tile_reg_map *reg_map)
{
	struct rsz_tile_data *data = &func->data->rsz;

	if (unlikely(!data))
		return MDP_MESSAGE_NULL_DATA;

	// drs: C42 downsampler output frame width
	data->c42_out_frame_w = (func->full_size_x_in + 0x01) & ~0x01;

	// prz
	if (data->ver_scale) {
		/* Line buffer size constraints (H: horizontal; V: vertical)
		 * H. Scale | V. First | V. Scale |  V. Acc.   | Tile Width
		 * ---------|----------|----------|------------|---------------
		 * (INF, 1] |   Yes    | (INF, 1] | 6-tap      | Input = L
		 *          |          | (1, 1/2) | 6/4n-tap   | Input = L
		 *          |          | [1/2, 0) | 4n-tap/src | Input = L / 2
		 * (1, 1/2) |    No    | (inf, 1] | 6-tap      | Output = L
		 *          |          | (1, 1/2) | 6/4n-tap   | Output = L
		 *          |          | [1/2, 0) | 4n-tap/src | Output = L / 2
		 * [1/2, 0) |    No    | (inf, 1] | 6-tap      | Output = L
		 *          |          | (1, 1/2) | 6/4n-tap   | Output = L / 2
		 *          |          | [1/2, 0) | 4n-tap/src | Output = L / 2
		 */
		if (data->ver_first) {
			/* vertical first */
			if (data->ver_algo == SCALER_6_TAPS ||
			    data->ver_cubic_trunc) {
				func->in_tile_width = data->max_width;
			} else {
				func->in_tile_width = data->max_width >> 1;
			}
		} else {
			if (data->ver_algo == SCALER_6_TAPS ||
			    data->ver_cubic_trunc) {
				func->out_tile_width = data->max_width - 2;
				data->prz_out_tile_w = data->max_width;
			} else {
				func->out_tile_width = (data->max_width >> 1) - 2;
				data->prz_out_tile_w = data->max_width >> 1;
			}
		}
	}

	func->in_tile_height = 65535;
	func->out_tile_height = 65535;

	// urs: C24 upsampler input frame width
	data->c24_in_frame_w = (func->full_size_x_out + 0x01) & ~0x1;

	return ISP_MESSAGE_TILE_OK;
}

enum isp_tile_message tile_tdshp_init(struct tile_func_block *func,
				      struct tile_reg_map *reg_map)
{
	struct tdshp_tile_data *data = &func->data->tdshp;

	if (unlikely(!data))
		return MDP_MESSAGE_NULL_DATA;

	func->in_tile_width   = data->max_width;
	func->out_tile_width  = data->max_width;
	func->in_tile_height  = 65535;
	func->out_tile_height = 65535;

	if (!data->relay_mode) {
		func->l_tile_loss = 3;
		func->r_tile_loss = 3;
		func->t_tile_loss = 2;
		func->b_tile_loss = 2;
	}

	return ISP_MESSAGE_TILE_OK;
}

enum isp_tile_message tile_wrot_init(struct tile_func_block *func,
				     struct tile_reg_map *reg_map)
{
	struct wrot_tile_data *data = &func->data->wrot;

	if (unlikely(!data))
		return MDP_MESSAGE_NULL_DATA;

	if (data->racing) {
		if (data->rotate == MML_ROT_90 ||
		    data->rotate == MML_ROT_270) {
			func->out_tile_width  = data->racing_h;
			func->in_tile_height  = 65535;
			func->out_tile_height = 65535;
		} else {
			func->out_tile_width  = data->max_width;
			func->in_tile_height  = data->racing_h;
			func->out_tile_height = data->racing_h;
		}
	} else {
		if (data->rotate == MML_ROT_0 && MML_FMT_IS_RGB(data->dest_fmt) &&
			!MML_FMT_COMPRESS(data->dest_fmt)) {
			/* output rgb will directly output to dram, no limit */
			func->out_tile_width = 65535;
		} else if (data->rotate == MML_ROT_90 ||
		    data->rotate == MML_ROT_270 ||
		    data->flip) {
			/* 90, 270 degrees and flip */
			func->out_tile_width = data->max_width;
		} else {
			func->out_tile_width = data->max_width * 2;
		}
		func->in_tile_height  = 65535;
		func->out_tile_height = 65535;
	}

	/* For tile calculation */
	if (MML_FMT_AFBC(data->dest_fmt)) {
		func->out_tile_width = min(128, func->out_tile_width);
		data->align_x = 32;
		data->align_y = 8;
	} else if (MML_FMT_10BIT_PACKED(data->dest_fmt) &&
		   !MML_FMT_ALPHA(data->dest_fmt)) {
		/* 10-bit packed, not alpha 32-bit */
		data->align_x = 4;
		data->align_y = 2;
	} else if (data->yuv_pending) {
		/* use wrot pending */
		if (MML_FMT_YUV422(data->dest_fmt)) {
			data->align_x = 2;
			/* To update with rotation */
			if (data->rotate == MML_ROT_90 ||
			    data->rotate == MML_ROT_270) {
				/* 90, 270 degrees & YUV422 */
				data->align_y = 2;
			}
		} else if (MML_FMT_YUV420(data->dest_fmt)) {
			data->align_x = 2;
			data->align_y = 2;
		}
	} else {
		/* use tile alignment */
		if (MML_FMT_YUV422(data->dest_fmt)) {
			func->out_const_x = 2;
			/* To update with rotation */
			if (data->rotate == MML_ROT_90 ||
			    data->rotate == MML_ROT_270) {
				/* 90, 270 degrees & YUV422 */
				func->out_const_y = 2;
			}
		} else if (MML_FMT_YUV420(data->dest_fmt)) {
			func->out_const_x = 2;
			func->out_const_y = 2;
		}
	}
	data->first_x_pad =
		(data->rotate == MML_ROT_0 && data->flip) ||
		(data->rotate == MML_ROT_180 && !data->flip) ||
		(data->rotate == MML_ROT_270);
	data->first_y_pad =
		(data->rotate == MML_ROT_90 && !data->flip) ||
		(data->rotate == MML_ROT_180) ||
		(data->rotate == MML_ROT_270 && data->flip);

	return ISP_MESSAGE_TILE_OK;
}

enum isp_tile_message tile_rdma_for(struct tile_func_block *func,
				    struct tile_reg_map *reg_map)
{
	if (!reg_map->skip_x_cal && !func->tdr_h_disable_flag) {
		func->out_pos_xs = func->in_pos_xs - func->crop_bias_x;
		func->out_pos_xe = func->in_pos_xe - func->crop_bias_x;
	}

	if (!reg_map->skip_y_cal && !func->tdr_v_disable_flag) {
		func->out_pos_ys = func->in_pos_ys - func->crop_bias_y;
		func->out_pos_ye = func->in_pos_ye - func->crop_bias_y;
	}

	if (!reg_map->skip_x_cal && !func->tdr_h_disable_flag) {
		if (func->backward_output_xs_pos >= func->out_pos_xs) {
			func->bias_x = func->backward_output_xs_pos -
				       func->out_pos_xs;
			func->out_pos_xs = func->backward_output_xs_pos;
		} else {
			return MDP_MESSAGE_BACKWARD_START_LESS_THAN_FORWARD;
		}

		if (func->out_pos_xe > func->backward_output_xe_pos)
			func->out_pos_xe = func->backward_output_xe_pos;
	}

	if (!reg_map->skip_y_cal && !func->tdr_v_disable_flag) {
		if (func->backward_output_ys_pos >= func->out_pos_ys) {
			func->bias_y = func->backward_output_ys_pos -
				       func->out_pos_ys;
			func->out_pos_ys = func->backward_output_ys_pos;
		} else {
			return MDP_MESSAGE_BACKWARD_START_LESS_THAN_FORWARD;
		}

		if (func->out_pos_ye > func->backward_output_ye_pos)
			func->out_pos_ye = func->backward_output_ye_pos;
	}

	return ISP_MESSAGE_TILE_OK;
}

enum isp_tile_message tile_crop_for(struct tile_func_block *func,
				    struct tile_reg_map *reg_map)
{
	if (!reg_map->skip_x_cal && !func->tdr_h_disable_flag) {
		if (func->backward_output_xs_pos >= func->out_pos_xs) {
			func->bias_x = func->backward_output_xs_pos -
				       func->out_pos_xs;
			func->out_pos_xs = func->backward_output_xs_pos;
		} else {
			return MDP_MESSAGE_BACKWARD_START_LESS_THAN_FORWARD;
		}

		if (func->out_pos_xe > func->backward_output_xe_pos)
			func->out_pos_xe = func->backward_output_xe_pos;
	}

	if (!reg_map->skip_y_cal && !func->tdr_v_disable_flag) {
		if (func->backward_output_ys_pos >= func->out_pos_ys) {
			func->bias_y = func->backward_output_ys_pos -
				       func->out_pos_ys;
			func->out_pos_ys = func->backward_output_ys_pos;
		} else {
			return MDP_MESSAGE_BACKWARD_START_LESS_THAN_FORWARD;
		}

		if (func->out_pos_ye > func->backward_output_ye_pos)
			func->out_pos_ye = func->backward_output_ye_pos;
	}

	return ISP_MESSAGE_TILE_OK;
}

enum isp_tile_message tile_aal_for(struct tile_func_block *func,
				   struct tile_reg_map *reg_map)
{
	/* skip frame mode */
	if (reg_map->first_frame)
		return ISP_MESSAGE_TILE_OK;

	if (!reg_map->skip_x_cal && !func->tdr_h_disable_flag) {
		if (func->out_tile_width &&
		    func->out_pos_xe + 1 > func->out_pos_xs + func->out_tile_width) {
			func->out_pos_xe = func->out_pos_xs + func->out_tile_width - 1;
			func->h_end_flag = false;
		}
	}

	if (!reg_map->skip_y_cal && !func->tdr_v_disable_flag) {
		if (func->out_tile_height &&
		    func->out_pos_ye + 1 > func->out_pos_ys + func->out_tile_height)
			func->out_pos_ye = func->out_pos_ys + func->out_tile_height - 1;
	}

	return ISP_MESSAGE_TILE_OK;
}

enum isp_tile_message tile_prz_for(struct tile_func_block *func,
				   struct tile_reg_map *reg_map)
{
	s32 C42OutXLeft = 0;
	s32 C42OutXRight = 0;
	s32 C24InXLeft = 0;
	s32 C24InXRight = 0;
	struct rsz_tile_data *data = &func->data->rsz;

	if (unlikely(!data))
		return MDP_MESSAGE_NULL_DATA;

	if (!reg_map->skip_x_cal && !func->tdr_h_disable_flag) {
		/* drs: C42 downsampler forward */
		if (data->use_121filter && func->in_pos_xs > 0) {
			/* Fixed 2 column tile loss for 121 filter */
			C42OutXLeft = func->in_pos_xs + 2;
		} else {
			C42OutXLeft = func->in_pos_xs;
		}

		if (func->in_pos_xe + 1 >= func->full_size_x_in) {
			C42OutXRight = data->c42_out_frame_w - 1;
		} else {
			C42OutXRight = func->in_pos_xe;
			if (data->crop_aal_tile_loss)
				C42OutXRight -= 8;

			/* tile calculation: xe is not frame end then is odd
			 * HW behavior:
			 *	prz in xe = drs out xe
			 *		  = (drs in xe + 0x01) & ~0x01
			 *	prz out xe = urs in xe
			 *		   = (urs out xe + 0x01) & ~0x01
			 * can match tile calculation
			 * HW only needs drs in size and urs out size
			 */
			if (!(func->in_pos_xe & 0x1))
				C42OutXRight -= 1;
		}

		/* prz */
		switch (data->hor_algo) {
		case SCALER_6_TAPS:
			forward_6_taps(
				C42OutXLeft,	/* C42 out = Scaler input */
				C42OutXRight,	/* C42 out = Scaler input */
				data->c42_out_frame_w - 1,
				data->coeff_step_x,
				data->precision_x,
				data->crop.r.left,
				data->crop.x_sub_px,
				data->c24_in_frame_w - 1,
				2,
				data->prz_back_xs,
				reg_map->first_frame ||
				func->backward_output_xe_pos >= func->max_out_pos_xe,
				&C24InXLeft,	/* C24 in = Scaler output */
				&C24InXRight,	/* C24 in = Scaler output */
				&func->bias_x,
				&func->offset_x,
				&func->bias_x_c,
				&func->offset_x_c);
			break;
		case SCALER_SRC_ACC:
			forward_src_acc(
				C42OutXLeft,	/* C42 out = Scaler input */
				C42OutXRight,	/* C42 out = Scaler input */
				data->c42_out_frame_w - 1,
				data->coeff_step_x,
				data->precision_x,
				data->crop.r.left,
				data->crop.x_sub_px,
				data->c24_in_frame_w - 1,
				2,
				data->prz_back_xs,
				reg_map->first_frame ||
				func->backward_output_xe_pos >= func->max_out_pos_xe,
				&C24InXLeft,	/* C24 in = Scaler output */
				&C24InXRight,	/* C24 in = Scaler output */
				&func->bias_x,
				&func->offset_x,
				&func->bias_x_c,
				&func->offset_x_c);
			break;
		case SCALER_CUB_ACC:
			forward_cub_acc(
				C42OutXLeft,	/* C42 out = Scaler input */
				C42OutXRight,	/* C42 out = Scaler input */
				data->c42_out_frame_w - 1,
				data->coeff_step_x,
				data->precision_x,
				data->crop.r.left,
				data->crop.x_sub_px,
				data->c24_in_frame_w - 1,
				2,
				data->prz_back_xs,
				reg_map->first_frame ||
				func->backward_output_xe_pos >= func->max_out_pos_xe,
				&C24InXLeft,	/* C24 in = Scaler output */
				&C24InXRight,	/* C24 in = Scaler output */
				&func->bias_x,
				&func->offset_x,
				&func->bias_x_c,
				&func->offset_x_c);
			break;
		default:
			ASSERT(0);
			return MDP_MESSAGE_RESIZER_SCALING_ERROR;
		}
		mml_msg(
			"%s  prz in w:%d (%d,%d) out w:%d (%d,%d) back (%d,%d) c24 back w:%d (%d,%d) crop:%u.%u",
			__func__, data->c42_out_frame_w, C42OutXLeft, C42OutXRight,
			data->c24_in_frame_w, C24InXLeft, C24InXRight,
			data->prz_back_xs, data->prz_back_xe, func->full_size_x_out,
			func->backward_output_xs_pos, func->backward_output_xe_pos,
			data->crop.r.left, data->crop.x_sub_px);

		C24InXLeft = data->prz_back_xs;

		if (C24InXRight > data->prz_back_xe)
			C24InXRight = data->prz_back_xe;

		/* urs: C24 upsampler forward */
		func->out_pos_xs = C24InXLeft;
		/* Fixed 1 column tile loss for C24 upsampling while end is even */
		func->out_pos_xe = C24InXRight - 1;

		if (C24InXRight >= data->c24_in_frame_w - 1)
			func->out_pos_xe = func->full_size_x_out - 1;

		if (func->out_pos_xe > func->backward_output_xe_pos)
			func->out_pos_xe = func->backward_output_xe_pos;
	}

	if (!reg_map->skip_y_cal && !func->tdr_v_disable_flag) {
		/* drs: C42 downsampler forward */
		/* prz */
		switch (data->ver_algo) {
		case SCALER_6_TAPS:
			forward_6_taps(
				func->in_pos_ys,
				func->in_pos_ye,
				func->full_size_y_in - 1,
				data->coeff_step_y,
				data->precision_y,
				data->crop.r.top,
				data->crop.y_sub_px,
				func->full_size_y_out - 1,
				func->out_const_y,
				func->backward_output_ys_pos,
				reg_map->first_frame ||
				func->backward_output_ye_pos >= func->max_out_pos_ye,
				&func->out_pos_ys,
				&func->out_pos_ye,
				&func->bias_y,
				&func->offset_y,
				&func->bias_y_c,
				&func->offset_y_c);
			break;
		case SCALER_SRC_ACC:
			forward_src_acc(
				func->in_pos_ys,
				func->in_pos_ye,
				func->full_size_y_in - 1,
				data->coeff_step_y,
				data->precision_y,
				data->crop.r.top,
				data->crop.y_sub_px,
				func->full_size_y_out - 1,
				func->out_const_y,
				func->backward_output_ys_pos,
				reg_map->first_frame ||
				func->backward_output_ye_pos >= func->max_out_pos_ye,
				&func->out_pos_ys,
				&func->out_pos_ye,
				&func->bias_y,
				&func->offset_y,
				&func->bias_y_c,
				&func->offset_y_c);
			break;
		case SCALER_CUB_ACC:
			forward_cub_acc(
				func->in_pos_ys,
				func->in_pos_ye,
				func->full_size_y_in - 1,
				data->coeff_step_y,
				data->precision_y,
				data->crop.r.top,
				data->crop.y_sub_px,
				func->full_size_y_out - 1,
				func->out_const_y,
				func->backward_output_ys_pos,
				reg_map->first_frame ||
				func->backward_output_ye_pos >= func->max_out_pos_ye,
				&func->out_pos_ys,
				&func->out_pos_ye,
				&func->bias_y,
				&func->offset_y,
				&func->bias_y_c,
				&func->offset_y_c);
			break;
		default:
			ASSERT(0);
			return MDP_MESSAGE_RESIZER_SCALING_ERROR;
		}

		func->out_pos_ys = func->backward_output_ys_pos;

		if (func->out_pos_ye > func->backward_output_ye_pos)
			func->out_pos_ye = func->backward_output_ye_pos;

		/* urs: C24 upsampler forward */
	}

	return ISP_MESSAGE_TILE_OK;
}

static int tile_wrot_align_out(
	const int alignment, const bool first_padding, const int full_size_out,
	const int out_str, int out_end)
{
	int remain = 0;

	if (first_padding) {
		/* first tile padding */
		if (out_str == 0) {
			remain = TILE_MOD(full_size_out - out_end - 1, alignment);
			if (remain)
				remain = alignment - remain;
		} else {
			remain = TILE_MOD(out_end - out_str + 1, alignment);
		}
	} else {
		/* last tile padding */
		if (out_end + 1 < full_size_out)
			remain = TILE_MOD(out_end - out_str + 1, alignment);
	}
	if (remain)
		out_end -= remain;
	return out_end;
}

static void tile_wrot_align_out_width(
	struct tile_func_block *func, const struct wrot_tile_data *data,
	const int full_size_x_out)
{
	if (data->align_x > 1) {
		func->out_pos_xe = tile_wrot_align_out(
			data->align_x, data->first_x_pad, full_size_x_out,
			func->out_pos_xs, func->out_pos_xe);
	}
}

static void tile_wrot_align_out_height(
	struct tile_func_block *func, const struct wrot_tile_data *data,
	const int full_size_y_out)
{
	if (data->align_y > 1) {
		func->out_pos_ye = tile_wrot_align_out(
			data->align_y, data->first_y_pad, full_size_y_out,
			func->out_pos_ys, func->out_pos_ye);
	}
}

enum isp_tile_message tile_wrot_for(struct tile_func_block *func,
				    struct tile_reg_map *reg_map)
{
	struct wrot_tile_data *data = &func->data->wrot;
	s32 remain = 0;

	if (unlikely(!data))
		return MDP_MESSAGE_NULL_DATA;

	/* frame mode */
	if (reg_map->first_frame) {
		if (data->enable_x_crop &&
		    !reg_map->skip_x_cal && !func->tdr_h_disable_flag) {
			if (func->min_out_pos_xs > func->out_pos_xs)
				func->out_pos_xs = func->min_out_pos_xs;
			if (func->out_pos_xe > func->max_out_pos_xe)
				func->out_pos_xe = func->max_out_pos_xe;
		} else if (data->enable_y_crop &&
			   !reg_map->skip_y_cal && !func->tdr_v_disable_flag) {
			if (func->min_out_pos_ys > func->out_pos_ys)
				func->out_pos_ys = func->min_out_pos_ys;
			if (func->out_pos_ye > func->max_out_pos_ye)
				func->out_pos_ye = func->max_out_pos_ye;
		}
		return ISP_MESSAGE_TILE_OK;
	}

	if (!reg_map->skip_x_cal && !func->tdr_h_disable_flag) {
		if (func->backward_output_xs_pos >= func->out_pos_xs) {
			func->bias_x = func->backward_output_xs_pos -
				       func->out_pos_xs;
			func->out_pos_xs = func->backward_output_xs_pos;
		} else {
			return MDP_MESSAGE_BACKWARD_START_LESS_THAN_FORWARD;
		}

		if (func->out_pos_xe >= func->backward_output_xe_pos) {
			func->out_pos_xe = func->backward_output_xe_pos;
		} else {
			/* Check out xe alignment */
			if (func->out_const_x > 1) {
				remain = TILE_MOD(func->out_pos_xe + 1,
						  func->out_const_x);
				if (remain)
					func->out_pos_xe -= remain;
			}

			/* Check out width alignment */
			tile_wrot_align_out_width(func, data, func->full_size_x_out);
		}
	}

	if (!reg_map->skip_y_cal && !func->tdr_v_disable_flag) {
		if (func->backward_output_ys_pos >= func->out_pos_ys) {
			func->bias_y = func->backward_output_ys_pos -
				       func->out_pos_ys;
			func->out_pos_ys = func->backward_output_ys_pos;
		} else {
			return MDP_MESSAGE_BACKWARD_START_LESS_THAN_FORWARD;
		}

		if (func->out_pos_ye >= func->backward_output_ye_pos) {
			func->out_pos_ye = func->backward_output_ye_pos;
		} else {
			/* Check out ye alignment */
			if (func->out_const_y > 1) {
				remain = TILE_MOD(func->out_pos_ye + 1,
						  func->out_const_y);
				if (remain)
					func->out_pos_ye -= remain;
			}

			/* Check out height alignment */
			tile_wrot_align_out_height(func, data, func->full_size_y_out);
		}
	}

	return ISP_MESSAGE_TILE_OK;
}

enum isp_tile_message tile_rdma_back(struct tile_func_block *func,
				     struct tile_reg_map *reg_map)
{
	struct rdma_tile_data *data = &func->data->rdma;
	s32 remain = 0, start = 0;

	if (unlikely(!data))
		return MDP_MESSAGE_NULL_DATA;

	if (!reg_map->skip_x_cal && !func->tdr_h_disable_flag) {
		func->in_pos_xs = func->out_pos_xs + func->crop_bias_x;
		func->in_pos_xe = func->out_pos_xe + func->crop_bias_x;
	}

	if (!reg_map->skip_y_cal && !func->tdr_v_disable_flag) {
		func->in_pos_ys = func->out_pos_ys + func->crop_bias_y;
		func->in_pos_ye = func->out_pos_ye + func->crop_bias_y;
	}

	/* frame mode */
	if (reg_map->first_frame) {
		/* Specific handle for block format */
		if (func->in_pos_xe + 1 > data->crop.left + data->crop.width)
			func->in_pos_xe = data->crop.left + data->crop.width - 1;

		if (MML_FMT_BLOCK(data->src_fmt)) {
			/* Alignment x right in block boundary */
			func->in_pos_xe = ((1 + (func->in_pos_xe >>
				data->blk_shift_w)) << data->blk_shift_w) - 1;

			if (func->in_pos_xe + 1 > func->full_size_x_in)
				func->in_pos_xe = func->full_size_x_in - 1;
		}

		if (func->in_const_x > 1) {
			remain = TILE_MOD(func->in_pos_xe + 1, func->in_const_x);
			if (remain)
				func->in_pos_xe += func->in_const_x - remain;

			remain = TILE_MOD(func->in_pos_xs, func->in_const_x);
			if (remain)
				func->in_pos_xs -= remain;
		}

		if (func->in_pos_ye + 1 > data->crop.top + data->crop.height)
			func->in_pos_ye = data->crop.top + data->crop.height - 1;

		if (MML_FMT_BLOCK(data->src_fmt)) {
			/* Alignment y bottom in block boundary */
			func->in_pos_ye = ((1 + (func->in_pos_ye >>
				data->blk_shift_h)) << data->blk_shift_h) - 1;

			if (func->in_pos_ye + 1 > func->full_size_y_in)
				func->in_pos_ye = func->full_size_y_in - 1;
		}

		if (func->in_const_y > 1) {
			remain = TILE_MOD(func->in_pos_ye + 1, func->in_const_y);
			if (remain)
				func->in_pos_ye += func->in_const_y - remain;

			remain = TILE_MOD(func->in_pos_ys, func->in_const_y);
			if (remain)
				func->in_pos_ys -= remain;
		}
		return ISP_MESSAGE_TILE_OK;
	}

	/* Specific handle for block format */
	if (!reg_map->skip_x_cal && !func->tdr_h_disable_flag) {
		if (func->in_pos_xe + 1 > data->crop.left + data->crop.width)
			func->in_pos_xe = data->crop.left + data->crop.width - 1;

		if (MML_FMT_BLOCK(data->src_fmt)) {
			/* Alignment x left in block boundary */
			start = ((func->in_pos_xs >> data->blk_shift_w) << data->blk_shift_w);

			/* For video block mode, FIFO limit is before crop */
			if (func->in_pos_xe + 1 > start + func->in_tile_width)
				func->in_pos_xe = start + func->in_tile_width - 1;

			/* Alignment x right in block boundary */
			func->in_pos_xe = ((1 + (func->in_pos_xe >>
				data->blk_shift_w)) << data->blk_shift_w) - 1;

			if (func->in_pos_xe + 1 > func->full_size_x_in)
				func->in_pos_xe = func->full_size_x_in - 1;
		}

		if (func->in_const_x > 1) {
			remain = TILE_MOD(func->in_pos_xe + 1, func->in_const_x);
			if (remain)
				func->in_pos_xe += func->in_const_x - remain;

			remain = TILE_MOD(func->in_pos_xs, func->in_const_x);
			if (remain)
				func->in_pos_xs -= remain;

			if (func->in_tile_width &&
			    func->in_pos_xe + 1 > func->in_pos_xs + func->in_tile_width)
				func->in_pos_xe = func->in_pos_xs + func->in_tile_width - 1;
		}
	}

	if (!reg_map->skip_y_cal && !func->tdr_v_disable_flag) {
		if (func->in_pos_ye + 1 > data->crop.top + data->crop.height)
			func->in_pos_ye = data->crop.top + data->crop.height - 1;

		if (MML_FMT_BLOCK(data->src_fmt)) {
			/* Alignment y top in block boundary */
			start = ((func->in_pos_ys >> data->blk_shift_h) << data->blk_shift_h);

			/* For video block mode, FIFO limit is before crop */
			if (func->in_pos_ye + 1 > start + func->in_tile_height)
				func->in_pos_ye = start + func->in_tile_height - 1;

			/* Alignment y bottom in block boundary */
			func->in_pos_ye = ((1 + (func->in_pos_ye >>
				data->blk_shift_h)) << data->blk_shift_h) - 1;

			if (func->in_pos_ye + 1 > func->full_size_y_in)
				func->in_pos_ye = func->full_size_y_in - 1;
		}

		if (func->in_const_y > 1) {
			remain = TILE_MOD(func->in_pos_ye + 1, func->in_const_y);
			if (remain)
				func->in_pos_ye += func->in_const_y - remain;

			remain = TILE_MOD(func->in_pos_ys, func->in_const_y);
			if (remain)
				func->in_pos_ys -= remain;

			if (func->in_tile_height &&
			    func->in_pos_ye + 1 > func->in_pos_ys + func->in_tile_height)
				func->in_pos_ye = func->in_pos_ys + func->in_tile_height - 1;
		}
	}
	return ISP_MESSAGE_TILE_OK;
}

enum isp_tile_message tile_prz_back(struct tile_func_block *func,
				    struct tile_reg_map *reg_map)
{
	s32 C24InXLeft = 0;
	s32 C24InXRight = 0;
	s32 C42OutXLeft = 0;
	s32 C42OutXRight = 0;
	struct rsz_tile_data *data = &func->data->rsz;

	if (unlikely(!data))
		return MDP_MESSAGE_NULL_DATA;

	if (!reg_map->skip_x_cal && !func->tdr_h_disable_flag) {
		/* urs: C24 upsampler backward */
		C24InXLeft = func->out_pos_xs;

		if (C24InXLeft & 0x1)
			C24InXLeft -= 1;

		if (func->out_tile_width &&
		    func->out_pos_xe + 1 > C24InXLeft + func->out_tile_width) {
			func->out_pos_xe = C24InXLeft + func->out_tile_width - 1;
			func->h_end_flag = false;
		}

		if (func->out_pos_xe + 1 >= func->full_size_x_out) {
			C24InXRight = data->c24_in_frame_w - 1;
		} else {
			/* Fixed 2 column tile loss for C24 upsampling while end is odd */
			C24InXRight = func->out_pos_xe + 2;

			if (!(func->out_pos_xe & 0x1))
				C24InXRight -= 1;
		}

		/* prz */
		if (data->prz_out_tile_w && func->out_tile_width &&
		    C24InXRight + 1 > C24InXLeft + data->prz_out_tile_w)
			C24InXRight = C24InXLeft + data->prz_out_tile_w - 1;

		if (C24InXRight + 1 > data->c24_in_frame_w)
			C24InXRight = data->c24_in_frame_w - 1;
		if (C24InXLeft < 0)
			C24InXLeft = 0;

		switch (data->hor_algo) {
		case SCALER_6_TAPS:
			backward_6_taps(
				C24InXLeft,	/* C24 in = Scaler output */
				C24InXRight,	/* C24 in = Scaler output */
				data->coeff_step_x,
				data->precision_x,
				data->crop.r.left,
				data->crop.x_sub_px,
				data->c42_out_frame_w - 1,
				2,
				&C42OutXLeft,	/* C42 out = Scaler input */
				&C42OutXRight);	/* C42 out = Scaler input */
			break;
		case SCALER_SRC_ACC:
			backward_src_acc(
				C24InXLeft,	/* C24 in = Scaler output */
				C24InXRight,	/* C24 in = Scaler output */
				data->coeff_step_x,
				data->precision_x,
				data->crop.r.left,
				data->crop.x_sub_px,
				data->c42_out_frame_w - 1,
				2,
				&C42OutXLeft,	/* C42 out = Scaler input */
				&C42OutXRight);	/* C42 out = Scaler input */
			break;
		case SCALER_CUB_ACC:
			backward_cub_acc(
				C24InXLeft,	/* C24 in = Scaler output */
				C24InXRight,	/* C24 in = Scaler output */
				data->coeff_step_x,
				data->precision_x,
				data->crop.r.left,
				data->crop.x_sub_px,
				data->c42_out_frame_w - 1,
				2,
				&C42OutXLeft,	/* C42 out = Scaler input */
				&C42OutXRight);	/* C42 out = Scaler input */
			break;
		default:
			ASSERT(0);
			return MDP_MESSAGE_RESIZER_SCALING_ERROR;
		}
		mml_msg("%s prz in w:%d (%d,%d)",
			__func__, data->c42_out_frame_w, C42OutXLeft, C42OutXRight);

		if (data->crop_aal_tile_loss) {
			C42OutXLeft -= 8;
			if (C42OutXLeft < 0)
				C42OutXLeft = 0;
			C42OutXRight += 8;
			if (C42OutXRight + 1 > data->c42_out_frame_w)
				C42OutXRight = data->c42_out_frame_w - 1;
		}

		if (func->in_tile_width &&
		    C42OutXRight + 1 > C42OutXLeft + func->in_tile_width)
			C42OutXRight = C42OutXLeft + func->in_tile_width - 1;
		data->prz_back_xs = C24InXLeft;
		data->prz_back_xe = C24InXRight;

		/* drs: C42 downsampler backward */
		func->in_pos_xs = C42OutXLeft;
		func->in_pos_xe = C42OutXRight;

		if (data->use_121filter) {
			/* Fixed 2 column tile loss for 121 filter */
			func->in_pos_xs -= 2;
		}

		if (func->in_pos_xs < 0)
			func->in_pos_xs = 0;
		if (func->in_pos_xe + 1 > func->full_size_x_in)
			func->in_pos_xe = func->full_size_x_in - 1;
	}

	if (!reg_map->skip_y_cal && !func->tdr_v_disable_flag) {
		/* urs: C24 upsampler backward */

		/* prz */
		switch (data->ver_algo) {
		case SCALER_6_TAPS:
			backward_6_taps(
				func->out_pos_ys,
				func->out_pos_ye,
				data->coeff_step_y,
				data->precision_y,
				data->crop.r.top,
				data->crop.y_sub_px,
				func->full_size_y_in - 1,
				func->in_const_y,
				&func->in_pos_ys,
				&func->in_pos_ye);
			break;
		case SCALER_SRC_ACC:
			backward_src_acc(
				func->out_pos_ys,
				func->out_pos_ye,
				data->coeff_step_y,
				data->precision_y,
				data->crop.r.top,
				data->crop.y_sub_px,
				func->full_size_y_in - 1,
				func->in_const_y,
				&func->in_pos_ys,
				&func->in_pos_ye);
			break;
		case SCALER_CUB_ACC:
			backward_cub_acc(
				func->out_pos_ys,
				func->out_pos_ye,
				data->coeff_step_y,
				data->precision_y,
				data->crop.r.top,
				data->crop.y_sub_px,
				func->full_size_y_in - 1,
				func->in_const_y,
				&func->in_pos_ys,
				&func->in_pos_ye);
			break;
		default:
			ASSERT(0);
			return MDP_MESSAGE_RESIZER_SCALING_ERROR;
		}

		/* drs: C42 downsampler backward */
	}

	return ISP_MESSAGE_TILE_OK;
}

enum isp_tile_message tile_wrot_back(struct tile_func_block *func,
				     struct tile_reg_map *reg_map)
{
	struct wrot_tile_data *data = &func->data->wrot;

	if (unlikely(!data))
		return MDP_MESSAGE_NULL_DATA;

	/* frame mode */
	if (reg_map->first_frame) {
		if (data->enable_x_crop &&
		    !reg_map->skip_x_cal && !func->tdr_h_disable_flag) {
			func->out_pos_xs = data->crop.left;
			func->out_pos_xe = data->crop.left + data->crop.width - 1;
			func->in_pos_xs = func->out_pos_xs;
			func->in_pos_xe = func->out_pos_xe;
			func->min_out_pos_xs = func->out_pos_xs;
			func->max_out_pos_xe = func->out_pos_xe;
		} else if (data->enable_y_crop &&
			   !reg_map->skip_y_cal && !func->tdr_v_disable_flag) {
			func->out_pos_ys = data->crop.top;
			func->out_pos_ye = data->crop.top + data->crop.height - 1;
			func->in_pos_ys = func->out_pos_ys;
			func->in_pos_ye = func->out_pos_ye;
			func->min_out_pos_ys = func->out_pos_ys;
			func->max_out_pos_ye = func->out_pos_ye;
		}
		return ISP_MESSAGE_TILE_OK;
	}

	if (!reg_map->skip_x_cal && !func->tdr_h_disable_flag) {
		int full_size_x_out = func->full_size_x_out;

		if (data->enable_x_crop) {
			if (func->valid_h_no == 0) {
				/* first tile */
				func->out_pos_xs = data->crop.left;
				func->in_pos_xs = func->out_pos_xs;
			}
			if (func->out_tile_width) {
				func->out_pos_xe = func->out_pos_xs +
						   func->out_tile_width - 1;
				func->in_pos_xe = func->out_pos_xe;
			}

			full_size_x_out = data->crop.left + data->crop.width;

			if (func->out_pos_xe + 1 >= full_size_x_out) {
				func->out_pos_xe = full_size_x_out - 1;
				func->in_pos_xe = func->out_pos_xe;
				/* func->h_end_flag = true; */
			}
		}

		if (data->alpharot) {
			if (func->out_pos_xe + 1 < full_size_x_out &&
			    func->out_pos_xe + 9 + 1 > full_size_x_out &&
			    func->out_pos_xe != func->out_pos_xs) {
				func->out_pos_xe =
					((full_size_x_out - 9 - 1 + 1) >> 2 << 2) - 1;
				func->in_pos_xe = func->out_pos_xe;
			}
		}

		/* Check out width alignment */
		tile_wrot_align_out_width(func, data, full_size_x_out);
	}

	if (!reg_map->skip_y_cal && !func->tdr_v_disable_flag) {
		int full_size_y_out = func->full_size_y_out;

		if (data->enable_y_crop) {
			if (func->valid_v_no == 0) {
				/* first tile */
				func->out_pos_ys = data->crop.top;
				func->in_pos_ys = func->out_pos_ys;
			}
			if (func->out_tile_height) {
				func->out_pos_ye = func->out_pos_ys +
						   func->out_tile_height - 1;
				func->in_pos_ye = func->out_pos_ye;
			}

			full_size_y_out = data->crop.top + data->crop.height;

			if (func->out_pos_ye + 1 >= full_size_y_out) {
				func->out_pos_ye = full_size_y_out - 1;
				func->in_pos_ye = func->out_pos_ye;
				/* func->v_end_flag = true; */
			}
		}

		/* Check out height alignment */
		tile_wrot_align_out_height(func, data, full_size_y_out);
	}

	return ISP_MESSAGE_TILE_OK;
}

enum isp_tile_message tile_dlo_back(struct tile_func_block *func,
				    struct tile_reg_map *reg_map)
{
	struct dlo_tile_data *data = &func->data->dlo;

	if (unlikely(!data))
		return MDP_MESSAGE_NULL_DATA;

	/* frame mode */
	if (reg_map->first_frame) {
		if (!reg_map->skip_x_cal && !func->tdr_h_disable_flag) {
			func->out_pos_xs = data->crop.left;
			func->out_pos_xe = data->crop.left + data->crop.width - 1;
			func->in_pos_xs = func->out_pos_xs;
			func->in_pos_xe = func->out_pos_xe;
			func->min_out_pos_xs = func->out_pos_xs;
			func->max_out_pos_xe = func->out_pos_xe;
			if (func->in_pos_xs > 0)
				func->tdr_edge &= ~TILE_EDGE_LEFT_MASK;
		}
		if (!reg_map->skip_y_cal && !func->tdr_v_disable_flag) {
			func->out_pos_ys = data->crop.top;
			func->out_pos_ye = data->crop.top + data->crop.height - 1;
			func->in_pos_ys = func->out_pos_ys;
			func->in_pos_ye = func->out_pos_ye;
			func->min_out_pos_ys = func->out_pos_ys;
			func->max_out_pos_ye = func->out_pos_ye;
			if (func->in_pos_ys > 0)
				func->tdr_edge &= ~TILE_EDGE_TOP_MASK;
		}
		return ISP_MESSAGE_TILE_OK;
	}

	if (!reg_map->skip_x_cal && !func->tdr_h_disable_flag) {
		int full_size_x_out = func->full_size_x_out;

		if (data->enable_x_crop) {
			if (func->valid_h_no == 0) {
				/* first tile */
				func->out_pos_xs = data->crop.left;
				func->in_pos_xs = func->out_pos_xs;
			}

			full_size_x_out = data->crop.left + data->crop.width;

			if (func->out_pos_xe + 1 >= full_size_x_out) {
				func->out_pos_xe = full_size_x_out - 1;
				func->in_pos_xe = func->out_pos_xe;
				/* func->h_end_flag = true; */
			}
		}
	}

	return ISP_MESSAGE_TILE_OK;
}

