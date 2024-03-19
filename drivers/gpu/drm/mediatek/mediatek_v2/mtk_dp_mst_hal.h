/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __MTK_DP_MST_HAL_H__
#define __MTK_DP_MST_HAL_H__

#include "mtk_dp_common.h"

#define DPTX_DPCD_TRANS_BYTES_MAX	16
#define MST_HAL 1

#ifndef MIN
#define MIN(a, b) (((a) > (b)) ? (b) : (a))
#endif

#if MST_HAL
void mtk_dptx_hal_verify_clock(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u32 link_rate);
void mtk_dptx_hal_set_video_interlance(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 enable);
void mtk_dptx_hal_bypass_msa_enable(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 enable);
void mtk_dptx_hal_set_msa(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
	struct DPTX_TIMING_PARAMETER *timing);
void mtk_dptx_hal_mvid_renew(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id);
u8 mtk_dptx_hal_mn_overwrite(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
	u8 enable, u32 video_m, u32 video_n);
void mtk_dptx_hal_set_color_format(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 color_format);
void mtk_dptx_hal_set_color_depth(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 color_depth);
void mtk_dptx_hal_set_misc(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8  misc[2]);
void mtk_dptx_hal_set_mvidx2(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8  enable);
u8 mtk_dptx_hal_get_color_info(u8  color_depth, u8  color_format);
void mtk_dptx_hal_set_tu_sram_read_start(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u16 val);
void mtk_dptx_hal_set_sdp_count_hblank(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u16 val);
void mtk_dptx_hal_set_sdp_count_init(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u16 val);
void mtk_dptx_hal_set_sdp_asp_count_init(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, const u16 val);
void mtk_dptx_hal_set_tu_encoder(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id);
void mtk_dptx_hal_pg_enable(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 enable);
void mtk_dptx_hal_pg_pure_color(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8  rgb,
				u32 color_depth);
void mtk_dptx_hal_pg_v_ramping(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
			       u8  rgb,
			       u32 color_depth,
			       u8  location);
void mtk_dptx_hal_pg_h_ramping(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 rgb, u8 location);
void mtk_dptx_hal_pg_v_color_bar(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8  location);
void mtk_dptx_hal_pg_h_color_bar(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8  location);
void mtk_dptx_hal_pg_chess_board(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
				 u8  location,
				 u16 hde,
				 u16 vde);
void mtk_dptx_hal_pg_sub_pixel(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 location);
void mtk_dptx_hal_pg_frame(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
			   u8 location,
			   u16 hde,
			   u16 vde);
void mtk_dptx_hal_spkg_sdp(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
			   u8 enable,
			   u8 sdp_type,
			   u8 *hb,
			   u8 *db);
void mtk_dptx_hal_spkg_vsc_ext_vesa(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
					u8 enable,
					u8 hdr_num,
					u8 *db);
void mtk_dptx_hal_spkg_vsc_ext_cea(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
				   u8  enable,
				   u8  hdr_num,
				   u8 *db);
void mtk_dptx_hal_audio_sample_arrange(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 enable);
void mtk_dptx_hal_audio_pg_enable(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
				  u8 channel,
				  u8 fs,
				  u8 enable);
void mtk_dptx_hal_audio_ch_status_set(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
					  u8 channel,
					  u8 fs,
					  u8 wordlength);
void mtk_dptx_hal_audio_sdp_setting(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 channel);
void mtk_dptx_hal_audio_set_mdiv(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 div);
void mtk_dptx_hal_encoder_reset_all(struct mtk_dp *mtk_dp);
void mtk_dptx_hal_dsc_enable(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 enable);
void mtk_dptx_hal_set_chunk_size(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
				 u8	slice_num,
				 u16 chunk_num,
				 u8	remainder,
				 u8	lane_count,
				 u8	hde_last_num,
				 u8	hde_num_even);
void mtk_dptx_hal_video_mute(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, const u8 enable);
void mtk_dptx_hal_audio_mute(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 enable);
#endif

void mtk_dptx_mst_hal_mst_enable(struct mtk_dp *mtk_dp, const u8 enable);
void mtk_dptx_mst_hal_tx_enable(struct mtk_dp *mtk_dp, const u8 enable);
void mtk_dptx_mst_hal_tx_init(struct mtk_dp *mtk_dp);
void mtk_dptx_mst_hal_mst_config(struct mtk_dp *mtk_dp);
void mtk_dptx_mst_hal_set_mtp_size(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, const u8 num_slots);
void mtk_dptx_mst_hal_set_vcpi(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
	const u16 start_slot,
	const u16 end_slot,
	const u32 vcpi);
void mtk_dptx_mst_hal_set_id_buf(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, const UINT32 vcpi);
void mtk_dptx_mst_hal_reset_payload(struct mtk_dp *mtk_dp);
void mtk_dptx_mst_hal_vcp_table_update(struct mtk_dp *mtk_dp);
void mtk_dptx_mst_hal_stream_enable(struct mtk_dp *mtk_dp, const u8 vcpi_mask, const u32 max_payloads);
void mtk_dptx_mst_hal_trigger_act(struct mtk_dp *mtk_dp);
#endif
