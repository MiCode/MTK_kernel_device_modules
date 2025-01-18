// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include "mtk_dp_mst_drv.h"
#include "mtk_dp_mst_hal.h"
#include "mtk_dp.h"
#include "mtk_dp_hal.h"
#include "mtk_dp_reg.h"
#include "mtk_dp_debug.h"
#include "mtk_dp_hdcp1x.h"
#include "mtk_dp_hdcp2.h"

#if (DPTX_OS == DPTX_CTP)
#include "drm_dp_drv_mst.h"
#else
#include <drm/display/drm_dp_mst_helper.h>
//#include <drm/drm_fb_helper.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_edid.h>
#include <drm/display/drm_dp_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_modes.h>
#endif

#define MST_DRV 1

u8 dptx_current_slot = DP_PAYLOAD_START_SLOT;
u8 dptx_mst_extended_start_slot;
u8 dptx_mst_extended_slot;
enum DP_VIDEO_TIMING_TYPE dptx_mst_ideal_timing_debug = SINK_1920_1080;
enum DP_COLOR_DEPTH_TYPE dptx_mst_color_depth_debug = DP_COLOR_DEPTH_8BIT;
u8 dptx_mst_frame_rate_debug = 60;
//struct drm_dp_mst_topology_mgr mtk_topo_mgr;

#if (DPTX_OS == DPTX_CTP)
extern struct drm_dp_payload gDPTx_MstPayload[DPTX_PAYLOAD_MAX];
#else
u8 start_slot;
u8 num_slots;
#endif

extern struct drm_dp_sideband_msg_tx *txmsg_debug;
//extern struct drm_dp_mst_port port_debug_real[DPTX_LCT_MAX][DPTX_PORT_NUM_MAX];
//extern struct drm_dp_mst_branch branch_debug_real[DPTX_LCT_MAX];
/*******************************************************************************
 *                                                                             *
 *                               MST Function                                  *
 *                                                                             *
 *******************************************************************************/
#if MST_DRV
void mtk_dptx_drv_set_sdp_asp_count_init(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id)
{
	u16 usSDP_Down_ASP_Cnt_Init = 0x0000;

	if (mtk_dp->info[encoder_id].DPTX_OUTBL.PixRateKhz > 0) {
		if (mtk_dp->training_info.ubLinkRate <= DP_LINKRATE_HBR3)
			usSDP_Down_ASP_Cnt_Init =
				mtk_dp->info[encoder_id].DPTX_OUTBL.Hbk *
				mtk_dp->training_info.ubLinkRate * 27 * 250 /
					(mtk_dp->info[encoder_id].DPTX_OUTBL.PixRateKhz) * 4 / 5;
		else
			usSDP_Down_ASP_Cnt_Init =
				mtk_dp->info[encoder_id].DPTX_OUTBL.Hbk *
				mtk_dp->training_info.ubLinkRate * 10 * 1000 / 32 * 250 /
					(mtk_dp->info[encoder_id].DPTX_OUTBL.PixRateKhz) * 4 / 5;
	}

	mtk_dptx_hal_set_sdp_asp_count_init(mtk_dp, encoder_id, usSDP_Down_ASP_Cnt_Init);
}

void mtk_dptx_drv_set_sdp_count_init(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u16 sram_read_start)
{
	u32 sdp_down_cnt = 0;

	/* sram_read_start * lane_cnt * 2(pixelperaddr) * ubLinkRate / pixel_clock * 0.8(margin) */
	sdp_down_cnt = (u32)(sram_read_start * mtk_dp->training_info.ubLinkLaneCount * 2
			* mtk_dp->training_info.ubLinkRate * 2700 * 8)
			/ mtk_dp->info[encoder_id].DPTX_OUTBL.PixRateKhz;

	if (mtk_dp->info[encoder_id].format == DP_COLOR_FORMAT_YUV_420)
		sdp_down_cnt = sdp_down_cnt / 2;

	switch (mtk_dp->training_info.ubLinkLaneCount) {
	case DP_LANECOUNT_1:
		sdp_down_cnt = (sdp_down_cnt > 0x1E) ? sdp_down_cnt : 0x1E;
		break;

	case DP_LANECOUNT_2:
		sdp_down_cnt = (sdp_down_cnt > 0x14) ? sdp_down_cnt : 0x14;
		break;

	case DP_LANECOUNT_4:
		sdp_down_cnt = (sdp_down_cnt > 0x08) ? sdp_down_cnt : 0x08;
		break;

	default:
		sdp_down_cnt = (sdp_down_cnt > 0x08) ? sdp_down_cnt : 0x08;
		break;
	}

	DPTXDBG("PixRateKhz = %lu sdp_down_cnt = %x\n",
		 mtk_dp->info[encoder_id].DPTX_OUTBL.PixRateKhz, sdp_down_cnt);

	mtk_dptx_hal_set_sdp_count_init(mtk_dp, encoder_id, sdp_down_cnt);
}

void mtk_dptx_drv_set_sdp_count_hblank(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id)
{
	u32 sdp_down_cnt;

	/* hblank * ubLinkRate / pixel_clock * 0.8(margin) / 4(1T4B) */
	sdp_down_cnt = (u32)((mtk_dp->info[encoder_id].DPTX_OUTBL.Htt
							-mtk_dp->info[encoder_id].DPTX_OUTBL.Hde)
			* mtk_dp->training_info.ubLinkRate * 2700 * 2)
			/ mtk_dp->info[encoder_id].DPTX_OUTBL.PixRateKhz;

	if (mtk_dp->info[encoder_id].format == DP_COLOR_FORMAT_YUV_420)
		sdp_down_cnt = sdp_down_cnt / 2;

	switch (mtk_dp->training_info.ubLinkLaneCount) {
	case DP_LANECOUNT_1:
		sdp_down_cnt = (sdp_down_cnt > 0x1E) ? sdp_down_cnt : 0x1E;
		break;

	case DP_LANECOUNT_2:
		sdp_down_cnt = (sdp_down_cnt > 0x14) ? sdp_down_cnt : 0x14;
		break;

	case DP_LANECOUNT_4:
		sdp_down_cnt = (sdp_down_cnt > 0x08) ? sdp_down_cnt : 0x08;
		break;
	default:
		sdp_down_cnt = (sdp_down_cnt > 0x08) ? sdp_down_cnt : 0x08;
		break;
	}
	//sdp_down_cnt = 0x20;
	DPTXDBG("sdp_down_cnt_blank = %x\n", sdp_down_cnt);
	mtk_dptx_hal_set_sdp_count_hblank(mtk_dp, encoder_id, sdp_down_cnt);
}

void mtk_dptx_drv_set_tu(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id)
{
#if ENABLE_DPTX_MST_DEBUG
	u32 tu_size = 0;
	u32 n_value =  0;
	u32 f_value =  0;
	u32 pix_rate_mhz = 0;
	u8  color_bpp;
	u8  color_depth = mtk_dp->info[encoder_id].depth;
	u8  color_format = mtk_dp->info[encoder_id].format;
#endif
	u16 sram_read_start = 0;

#if ENABLE_DPTX_MST_DEBUG
	DPTXFUNC();

	color_bpp = mtk_dptx_hal_get_color_info(color_depth, color_format);
	pix_rate_mhz = mtk_dp->info[encoder_id].DPTX_OUTBL.PixRateKhz / 1000;
	tu_size = (640 * (pix_rate_mhz) * color_bpp) /
		  (mtk_dp->training_info.ubLinkRate * 27 *
		   mtk_dp->training_info.ubLinkLaneCount * 8);

	n_value = tu_size / 10;
	f_value = tu_size - n_value * 10;

	DPTXDBG("[DPTX] color_bpp= %d\n", color_bpp);
	DPTXDBG("[DPTX] tu_size %d,\n", tu_size);
	DPTXDBG("[DPTX] n_value %d,\n", n_value);
	DPTXDBG("[DPTX] f_value %d,\n", f_value);
#endif

	if (mtk_dp->training_info.ubLinkLaneCount > 0) {
		sram_read_start = mtk_dp->info[encoder_id].DPTX_OUTBL.Hde /
				  (mtk_dp->training_info.ubLinkLaneCount * 4 * 2 * 2);
		sram_read_start =
			(sram_read_start < DPTX_TBC_BUF_ReadStartAdrThrd) ?
			sram_read_start : DPTX_TBC_BUF_ReadStartAdrThrd;
		mtk_dptx_hal_set_tu_sram_read_start(mtk_dp, encoder_id, sram_read_start);
	}

	mtk_dptx_hal_set_tu_encoder(mtk_dp, encoder_id);
	mtk_dptx_hal_audio_sample_arrange(mtk_dp, encoder_id, TRUE);
	mtk_dptx_drv_set_sdp_count_hblank(mtk_dp, encoder_id);
	mtk_dptx_drv_set_sdp_count_init(mtk_dp, encoder_id, sram_read_start);
	mtk_dptx_drv_set_sdp_asp_count_init(mtk_dp, encoder_id);
}

void mtk_dptx_drv_calculate_mn(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id)
{
	u8 frame_rate = 60;
	u32 pix_clk = 148500000;
	u32 pll_rate; //Base = 1Khz
	u32 val, Mvid, Nvid;

	Nvid = 0x8000;

	//VPLL's input clock is 216*4P MHz , x10 for N.f , Base=100K
	pll_rate = (0x00D8 << 2) * 10;

	if (mtk_dp->info[encoder_id].DPTX_OUTBL.FrameRate > 0) {
		frame_rate = mtk_dp->info[encoder_id].DPTX_OUTBL.FrameRate;
		DPTXDBG("[DPTX] Frame Rate = %d\n",
					mtk_dp->info[encoder_id].DPTX_OUTBL.FrameRate);

		pix_clk = (u32) mtk_dp->info[encoder_id].DPTX_OUTBL.Htt *
			  (u32) mtk_dp->info[encoder_id].DPTX_OUTBL.Vtt *
			  (u32) frame_rate;
	} else if (mtk_dp->info[encoder_id].DPTX_OUTBL.PixRateKhz > 0) {
		frame_rate = 60;
		pix_clk = mtk_dp->info[encoder_id].DPTX_OUTBL.PixRateKhz * 1000;
		DPTXDBG("[DPTX] Pix Clk (kHz) = %lu\n",
			mtk_dp->info[encoder_id].DPTX_OUTBL.PixRateKhz);
	} else { // if no fill Frame && no fill PixRate
		frame_rate = 60;
		pix_clk = (u32) mtk_dp->info[encoder_id].DPTX_OUTBL.Htt *
			  (u32) mtk_dp->info[encoder_id].DPTX_OUTBL.Vtt *
			  (u32) frame_rate;

		DPTXDBG("[DPTX] both frame_rate & pix_clk = 0\n");
	}

	DPTXDBG("pix_clk = 0x%x\r\n", pix_clk);

	val = pix_clk / (100000); // x10 for N.f , Base=100K

	if (pix_clk > 0) {
		Mvid = (val * Nvid) / pll_rate;

		DPTXDBG("[DPTX] Cal PR = %d x(1/10) Mhz\n", val);
		DPTXDBG("[DPTX] Mvid 0x%x\n", Mvid);
		mtk_dp->info[encoder_id].DPTX_OUTBL.PixRateKhz = pix_clk / 1000;
	}

	if (mtk_dp->training_info.ubLinkRate > 0) {
		Mvid = (val * Nvid) / (mtk_dp->training_info.ubLinkRate * 270);
		DPTXDBG("[DPTX] Video_M 0x%x\n", Mvid);
	}

#ifdef DPTX_OVM_PATCH
	mtk_dptx_hal_mn_overwrite(mtk_dp, encoder_id, TRUE, Mvid, Nvid);
	DPTXMSG("[DPTX] MN Overwrite \r\n");
#endif
}

void mtk_dptx_drv_set_misc(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id)
{
	u8 format, depth;
	union MISC_T *misc = &mtk_dp->info[encoder_id].DPTX_OUTBL.misc;

	format = mtk_dp->info[encoder_id].format;
	depth = mtk_dp->info[encoder_id].depth;

	// reset misc
	misc->ucMISC[0] = 0;
	misc->ucMISC[1] = 0;

	// MISC 0/1 refernce to spec 1.4a p143 Table 2-96
	// MISC0[7:5] color depth
	switch (depth) {
	case DP_COLOR_DEPTH_6BIT:
	case DP_COLOR_DEPTH_8BIT:
	case DP_COLOR_DEPTH_10BIT:
	case DP_COLOR_DEPTH_12BIT:
	case DP_COLOR_DEPTH_16BIT:
	default:
		misc->dp_misc.color_depth = depth;
		break;
	}

	// MISC0[3]: 0->RGB, 1->YUV
	// MISC0[2:1]: 01b->4:2:2, 10b->4:4:4
	switch (format) {
	case DP_COLOR_FORMAT_YUV_444:
		misc->dp_misc.color_format = 0x2;  //10'b
		misc->dp_misc.spec_def1 = 0x1;
		break;

	case DP_COLOR_FORMAT_YUV_422:
		misc->dp_misc.color_format = 0x1;  //01'b
		misc->dp_misc.spec_def1 = 0x1;
		break;

	case DP_COLOR_FORMAT_YUV_420:
		//not support
		break;

	case DP_COLOR_FORMAT_RAW:
		misc->dp_misc.color_format = 0x1;
		misc->dp_misc.spec_def2 = 0x1;
		break;

	case DP_COLOR_FORMAT_YONLY:
		misc->dp_misc.color_format = 0x0;
		misc->dp_misc.spec_def2 = 0x1;
		break;

	case DP_COLOR_FORMAT_RGB_444:
	default:
		misc->dp_misc.color_format = 0x0;
		misc->dp_misc.spec_def2 = 0x0;
		break;
	}

	mtk_dptx_hal_set_misc(mtk_dp, encoder_id, misc->ucMISC);
}

void mtk_dptx_drv_prepare_output_timing(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id)
{
	if (mtk_dp->info[encoder_id].DPTX_OUTBL.Video_ip_mode == DPTX_VIDEO_INTERLACE)
		mtk_dptx_hal_set_video_interlance(mtk_dp, encoder_id, TRUE);
	else
		mtk_dptx_hal_set_video_interlance(mtk_dp, encoder_id, FALSE);

//  ########### SET DPTX MSA #######################################
	mtk_dptx_hal_set_msa(mtk_dp, encoder_id, &mtk_dp->info[encoder_id].DPTX_OUTBL);
//  ################################################################
}

void mtk_dptx_drv_set_dptx_out(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id)
{
	u8 format, depth;

	format = mtk_dp->info[encoder_id].format;
	depth = mtk_dp->info[encoder_id].depth;

	mtk_dptx_drv_video_config(mtk_dp, encoder_id);
	mtk_dptx_hal_bypass_msa_enable(mtk_dp, encoder_id, FALSE);
	mtk_dptx_drv_prepare_output_timing(mtk_dp, encoder_id);
	mtk_dptx_hal_set_color_format(mtk_dp, encoder_id, format);
	mtk_dptx_hal_set_color_depth(mtk_dp, encoder_id, depth);
	mtk_dptx_drv_calculate_mn(mtk_dp, encoder_id);

	switch (mtk_dp->info[encoder_id].input_src) {
	case DPTX_SRC_PG:
		mtk_dptx_drv_set_misc(mtk_dp, encoder_id);
		mtk_dptx_hal_pg_enable(mtk_dp, encoder_id, TRUE);
#if (DPTX_OS == DPTX_CTP)
		mtk_dptx_drv_video_clock(mtk_dp, encoder_id);
#endif
		mtk_dptx_hal_set_mvidx2(mtk_dp, encoder_id, FALSE);
		mtk_dptx_hal_audio_mute(mtk_dp, encoder_id, TRUE);
		DPTXDBG("[DPTX] Using PG\n");
		break;

	case DPTX_SRC_DPINTF:
		mtk_dptx_hal_pg_enable(mtk_dp, encoder_id, FALSE);
#if (DPTX_OS == DPTX_CTP)
		mtk_dptx_drv_video_clock(mtk_dp, encoder_id);
#endif
		DPTXDBG("[DPTX] Using DPINTF\n");
		break;

	default:
		mtk_dptx_hal_pg_enable(mtk_dp, encoder_id, TRUE);
		break;
	}

	mtk_dptx_hal_mvid_renew(mtk_dp, encoder_id);
	mtk_dptx_drv_set_tu(mtk_dp, encoder_id);
}

void mtk_dptx_drv_set_output_mode(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id)
{
	u8 pg_id = mtk_dp->info[encoder_id].pattern_id;

	switch (mtk_dp->info[encoder_id].input_src) {
	case DPTX_SRC_PG:
		mtk_dptx_drv_set_pg_mode(mtk_dp, encoder_id, TRUE, pg_id);
		mtk_dptx_drv_set_dptx_out(mtk_dp, encoder_id);
		DPTXDBG("[DPTX] Set Pattern Gen \r\n");
		break;

	case DPTX_SRC_DPINTF:
		mtk_dptx_drv_set_dptx_out(mtk_dp, encoder_id);
		DPTXDBG("[DPTX] Set DP_INTF Mode\r\n");
		break;

	default:
		break;
	}
}

void mtk_dptx_drv_spkg_sdp(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
			   u8  enable,
			   u8  sdp_type,
			   u8 *hb,
			   u8 *db)
{
	mtk_dptx_hal_spkg_sdp(mtk_dp, encoder_id, enable, sdp_type, hb, db);
}

void mtk_dptx_drv_spkg_vsc_ext(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
			       u8  enable,
			       u8  vsc_hb01,
			       u8  hdr_num,
			       u8 *db)
{
	if (hdr_num > 0x3F) {
		DPTXMSG("[DPTX] HDR_NUM Over Range %d!!\r\n", hdr_num);
		return;
	}

	switch (vsc_hb01) {
	case 0x20: // FOR VESA
		mtk_dptx_hal_spkg_vsc_ext_vesa(mtk_dp, encoder_id, enable, hdr_num, db);
		break;

	case 0x21: // FOR CEA
		mtk_dptx_hal_spkg_vsc_ext_cea(mtk_dp, encoder_id, enable, hdr_num, db);
		break;

	default:
		DPTXMSG("[DPTX] Encoder %d, Wrong VSC_EXT Type %d!!\r\n", encoder_id, vsc_hb01);
		break;

	}
}

void mtk_dptx_drv_pg_type_sel(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
			      enum DPTx_PG_TYPESEL pattern_type,
			      enum DPTx_PG_PURECOLOR  rgb,
			      u32 color_depth,
			      enum DPTx_PG_LOCATION  location,
			      enum DP_VIDEO_TIMING_TYPE pattern_res)
{
	u16 hde = mtk_dp->info[encoder_id].DPTX_OUTBL.Hde;
	u16 vde = mtk_dp->info[encoder_id].DPTX_OUTBL.Vde;

	mtk_dp->info[encoder_id].resolution = pattern_res;

	switch (pattern_type) {
	case DPTX_PG_PURE_COLOR:
		mtk_dptx_hal_pg_pure_color(mtk_dp, encoder_id, rgb, color_depth);
		DPTXMSG("[DPTX] Encoder %d, Pure Color \r\n", encoder_id);
		break;

	case DPTX_PG_VERTICAL_RAMPING:
		mtk_dptx_hal_pg_v_ramping(mtk_dp, encoder_id, rgb, color_depth, location);
		DPTXMSG("[DPTX] Encoder %d, Vertical Ramping \r\n", encoder_id);
		break;

	case DPTX_PG_HORIZONTAL_RAMPING:
		mtk_dptx_hal_pg_h_ramping(mtk_dp, encoder_id, rgb, location);
		DPTXMSG("[DPTX] Encoder %d, Horizontal Ramping \r\n", encoder_id);
		break;

	case DPTX_PG_VERTICAL_COLOR_BAR:
		mtk_dptx_hal_pg_v_color_bar(mtk_dp, encoder_id, location);
		DPTXMSG("[DPTX] Encoder %d, Vertical Color Bar \r\n", encoder_id);
		break;

	case DPTX_PG_HORIZONTAL_COLOR_BAR:
		mtk_dptx_hal_pg_h_color_bar(mtk_dp, encoder_id, location);
		DPTXMSG("[DPTX] Encoder %d, Horizontal Color Bar \r\n", encoder_id);
		break;

	case DPTX_PG_CHESSBOARD_PATTERN:
		mtk_dptx_hal_pg_chess_board(mtk_dp, encoder_id, location, hde, vde);
		DPTXMSG("[DPTX] Encoder %d, Chessboard Pattern \r\n", encoder_id);
		break;

	case DPTX_PG_SUB_PIXEL_PATTERN:
		mtk_dptx_hal_pg_sub_pixel(mtk_dp, encoder_id, location);
		DPTXMSG("[DPTX] Encoder %d, Sub Pixel Pattern \r\n", encoder_id);
		break;

	case DPTX_PG_FRAME_PATTERN:
		mtk_dptx_hal_pg_frame(mtk_dp, encoder_id, location, hde, vde);
		DPTXMSG("[DPTX] Encoder %d, Frame Pattern \r\n", encoder_id);
		break;

	default:
		break;
	}
}

void mtk_dptx_drv_video_mute(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, const u8 enable)
{
	if (mtk_dp->info[encoder_id].bSetVideoMute)
		mtk_dp->info[encoder_id].bVideoMute = TRUE;
	else
		mtk_dp->info[encoder_id].bVideoMute = enable;

	mtk_dptx_hal_video_mute(mtk_dp, encoder_id, enable);
}

void mtk_dptx_drv_video_mute_all(struct mtk_dp *mtk_dp)
{
	enum DPTX_ENCODER_ID encoder_id;

	for (encoder_id = 0; encoder_id < DPTX_ENCODER_ID_MAX; encoder_id++)
		mtk_dptx_hal_video_mute(mtk_dp, encoder_id, TRUE);
}

void mtk_dptx_drv_audio_mute(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 enable)
{
	if (mtk_dp->info[encoder_id].bSetAudioMute)
		mtk_dp->info[encoder_id].bAudioMute = TRUE;
	else
		mtk_dp->info[encoder_id].bAudioMute = enable;

	mtk_dptx_hal_audio_mute(mtk_dp, encoder_id, enable);
}

void mtk_dptx_drv_audio_mute_all(struct mtk_dp *mtk_dp)
{
	enum DPTX_ENCODER_ID encoder_id;

	for (encoder_id = 0; encoder_id < DPTX_ENCODER_ID_MAX; encoder_id++)
		mtk_dptx_drv_audio_mute(mtk_dp, encoder_id, TRUE);
}

void mtk_dptx_drv_video_enable(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 enable)
{
	DPTXMSG("Output Video %s!\n", enable ? "enable" : "disable");

	if (enable) {
		mtk_dptx_drv_set_dptx_out(mtk_dp, encoder_id);
		mtk_dptx_drv_video_mute(mtk_dp, encoder_id, FALSE);
		mtk_dptx_hal_verify_clock(mtk_dp, encoder_id, mtk_dp->training_info.ubLinkRate);
	} else
		mtk_dptx_drv_video_mute(mtk_dp, encoder_id, TRUE);
}

void mtk_dptx_drv_set_output_frame_rate(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
					u8 enable, u8 frame_rate)
{
	//mtk_dp->info[encoder_id].fix_frame_rate = enable;
	//mtk_dp->info[encoder_id].video_status_change =  TRUE;
	mtk_dp->info[encoder_id].DPTX_OUTBL.FrameRate = frame_rate;
}

void mtk_dptx_drv_set_pg_mode(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 enable, u8 pattern_id)
{
	if (pattern_id >= SINK_MAX)
		pattern_id = SINK_640_480;

	mtk_dp->info[encoder_id].input_src = DPTX_SRC_PG;
	mtk_dp->info[encoder_id].pattern_id = pattern_id;
	//mtk_dp->info[encoder_id].video_status_change =  enable;

	//if (enable && (mtk_dp->info[encoder_id].set_timing == FALSE))
	mtk_dptx_drv_set_pg_timing_info(mtk_dp, encoder_id, pattern_id);
}

void mtk_dptx_drv_set_pg_timing_info(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 pattern_id)
{
	mtk_dp->info[encoder_id].resolution = pattern_id;
	mtk_dptx_drv_video_config(mtk_dp, encoder_id);
}

void mtk_dptx_drv_set_color_format(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8  color_format)
{
	mtk_dp->info[encoder_id].format = color_format;
	mtk_dptx_hal_set_color_format(mtk_dp, encoder_id, color_format);
}

void mtk_dptx_drv_set_color_depth(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 color_depth)
{
	mtk_dp->info[encoder_id].depth = color_depth;
	mtk_dptx_hal_set_color_depth(mtk_dp, encoder_id, color_depth);
}


// ====================================================================
//
//          dptx input source from dpi function call
//
// ====================================================================
void mtk_dptx_drv_dpi_enable(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 enable)
{
	if (enable) {
		mtk_dp->info[encoder_id].input_src = DPTX_SRC_DPINTF;
		DPTXMSG("[DPTX] DPI Enable!!!\n");
	}  else {
		mtk_dp->info[encoder_id].input_src = DPTX_SRC_PG;
		DPTXMSG("[DPTX] DPI Disable!!!\n");
	}


	mtk_dptx_drv_set_output_mode(mtk_dp, encoder_id);
	mtk_dptx_drv_video_mute(mtk_dp, encoder_id, FALSE);
}

void mtk_dptx_drv_dpi_set_msa(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, void *input)
{
	struct DPTX_TIMING_PARAMETER *timing = (struct DPTX_TIMING_PARAMETER *) input;

	memcpy(&mtk_dp->info[encoder_id].DPTX_OUTBL, timing, sizeof(*timing));

	DPTXMSG("[DPTX] DPI MSA: Hde = %d\n", timing->Hde);
	DPTXMSG("[DPTX] DPI MSA: Hbk = %d\n", timing->Hbk);
	DPTXMSG("[DPTX] DPI MSA: Hfp = %d\n", timing->Hfp);
	DPTXMSG("[DPTX] DPI MSA: Hsw = %d\n", timing->Hsw);
	DPTXMSG("[DPTX] DPI MSA: Hbp = %d\n", timing->Hbp);
	DPTXMSG("[DPTX] DPI MSA: Htt = %d\n", timing->Htt);

	DPTXMSG("[DPTX] DPI MSA: Vde = %d\n", timing->Vde);
	DPTXMSG("[DPTX] DPI MSA: Vbk = %d\n", timing->Vbk);
	DPTXMSG("[DPTX] DPI MSA: Vfp = %d\n", timing->Vfp);
	DPTXMSG("[DPTX] DPI MSA: Vsw = %d\n", timing->Vsw);
	DPTXMSG("[DPTX] DPI MSA: Vbp = %d\n", timing->Vbp);
	DPTXMSG("[DPTX] DPI MSA: Vtt = %d\n", timing->Vtt);

	DPTXMSG("[DPTX] DPI MSA: bHsp = %d\n", timing->bHsp);
	DPTXMSG("[DPTX] DPI MSA: bVsp = %d\n", timing->bVsp);
	DPTXMSG("[DPTX] DPI MSA: ip_mode = %d\n", timing->Video_ip_mode);
	DPTXMSG("[DPTX] DPI MSA: FrameRate = %d\n", timing->FrameRate);
	DPTXMSG("[DPTX] DPI MSA: PixRateKhz = %lu\n", timing->PixRateKhz);

	mtk_dptx_hal_set_msa(mtk_dp, encoder_id, timing);
}

void mtk_dptx_drv_dpi_set_misc(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, union MISC_T misc)
{
	memcpy(&mtk_dp->info[encoder_id].DPTX_OUTBL.misc, &misc, sizeof(misc));
	mtk_dptx_hal_set_misc(mtk_dp, encoder_id, misc.ucMISC);
	DPTXMSG("[DPTX] DPI Set Encoder %d, MISC[0] = %d & MISC[1]= %d\n", encoder_id, misc.ucMISC[0], misc.ucMISC[1]);
}

#define AUDIO_SAMPLE_PATCH 0
void mtk_dptx_drv_audio_enable(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 enable)
{
#if AUDIO_SAMPLE_PATCH
	mtk_dptx_hal_audio_sample_arrange(mtk_dp, encoder_id, enable);
#endif

	if (enable) {
		mtk_dptx_drv_i2s_audio_set_mdiv(mtk_dp, encoder_id, DPTX_AUDIO_M_DIV_D2);
		mtk_dptx_drv_audio_mute(mtk_dp, encoder_id, FALSE);
		DPTXMSG("[DPTX] Encoder %d, Audio Enable!\n", encoder_id);
	} else {
		mtk_dptx_drv_i2s_audio_set_mdiv(mtk_dp, encoder_id, DPTX_AUDIO_M_DIV_D2);
		mtk_dptx_drv_audio_mute(mtk_dp, encoder_id, TRUE);
		DPTXMSG("[DPTX] Encoder %d, Audio Disable!\n", encoder_id);
	}
}

void mtk_dptx_drv_i2s_audio_set_mdiv(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 divider)
{
#if ENABLE_DPTX_MST_DEBUG
	u8 table[8][5] = {"X1", "X2", "X4", "X8",
			     "/2", "/4", "X1", "/8"
			    };

	DPTXMSG("[DPTX] Encoder %d, I2S Set Audio M div %s\n", encoder_id, table[divider]);
#endif
	mtk_dptx_hal_audio_set_mdiv(mtk_dp, encoder_id, divider);
}

void mtk_dptx_drv_i2s_audio_config(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id)
{
	u8  ch, fs, len;
	u32 tmp = mtk_dp->info[encoder_id].audio_config;

	if (!mtk_dp->dp_ready) {
		DPTXERR("%s, DP is not ready!\n", __func__);
		return;
	}

	ch = (tmp >> DP_CAPABILITY_CHANNEL_SFT) & DP_CAPABILITY_CHANNEL_MASK;
	fs = (tmp >> DP_CAPABILITY_SAMPLERATE_SFT) & DP_CAPABILITY_SAMPLERATE_MASK;
	len = (tmp >> DP_CAPABILITY_BITWIDTH_SFT) & DP_CAPABILITY_BITWIDTH_MASK;

	switch (ch) {
	case DP_CHANNEL_2:
		ch = 2;
		break;

	case DP_CHANNEL_8:
		ch = 8;
		break;

	default:
		ch = 2;
		break;
	}

	switch (fs) {
	case DP_SAMPLERATE_32:
		fs = FS_32K;
		break;

	case DP_SAMPLERATE_44:
		fs = FS_44K;
		break;

	case DP_SAMPLERATE_48:
		fs = FS_48K;
		break;

	case DP_SAMPLERATE_96:
		fs = FS_96K;
		break;

	case DP_SAMPLERATE_192:
		fs = FS_192K;
		break;

	default:
		fs = FS_48K;
		break;
	}

	switch (len) {
	case DP_BITWIDTH_16:
		len = WL_16bit;
		break;

	case DP_BITWIDTH_20:
		len = WL_20bit;
		break;

	case DP_BITWIDTH_24:
		len = WL_24bit;
		break;

	default:
		len = WL_24bit;
		break;
	}

	mtk_dptx_drv_i2s_audio_sdp_setting(mtk_dp, encoder_id, ch, fs, len);
	mtk_dptx_drv_i2s_audio_ch_status_set(mtk_dp, encoder_id, ch, fs, len);

	mtk_dptx_hal_audio_pg_enable(mtk_dp, encoder_id, ch, fs, FALSE);
	mtk_dptx_drv_i2s_audio_set_mdiv(mtk_dp, encoder_id, DPTX_AUDIO_M_DIV_D2);
}

void mtk_dptx_drv_i2s_audio_sdp_setting(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
					u8 ch,
					u8 fs,
					u8 len)
{
	u8 SDP_DB[32] = {0};
	u8 SDP_HB[4] = {0};

	SDP_HB[1] = DP_SPEC_SDPTYP_AINFO;
	SDP_HB[2] = 0x1B;
	SDP_HB[3] = 0x48;

	SDP_DB[0x0] = 0x10 | (ch - 1); //L-PCM[7:4], channel-1[2:0]
	SDP_DB[0x1] = fs << 2 | len; // fs[4:2], len[1:0]
	SDP_DB[0x2] = 0x0;

	if (ch == 8)
		SDP_DB[0x3] = 0x13;
	else
		SDP_DB[0x3] = 0x00;

	mtk_dptx_hal_audio_sdp_setting(mtk_dp, encoder_id, ch);
	DPTXMSG("Encoder %d, I2S Set Audio Channel = %d\n", encoder_id, ch);
	mtk_dptx_drv_spkg_sdp(mtk_dp, encoder_id, TRUE, DPTx_SDPTYP_AUI, SDP_HB, SDP_DB);
}

void mtk_dptx_drv_i2s_audio_ch_status_set(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
				u8 ch, u8 fs, u8 len)
{
	mtk_dptx_hal_audio_ch_status_set(mtk_dp, encoder_id, ch, fs, len);
	DPTXMSG("[DPTX] I2S Set Audio Channel Status !\n");
	DPTXMSG("[DPTX] (encoder_id, ch, fs, len) = %d, %d, %d, %d\n", encoder_id, ch, fs, len);
}

// ====================================================================

void mtk_dptx_drv_dsc_set_param(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, union PPS_T *pps)
{
	u16 chunk_num, pic_width, slice_width;
	u8 r8, r16;
	u8 hde_last_num, hde_num_even, slice_num;
	u8 lane_count = mtk_dp->training_info.ubLinkLaneCount;

	u8 q16[16] = {0b110, 0b001, 0b001, 0b011, 0b011, 0b101, 0b101, 0b111,
			 0b111, 0b000, 0b000, 0b010, 0b010, 0b100, 0b100, 0b110};

	u8   q8[8] = {0b110, 0b001, 0b011, 0b101, 0b111, 0b000, 0b010, 0b100};


	chunk_num = (pps->ucPPS[14] << 8) + pps->ucPPS[15];
	pic_width = (pps->ucPPS[8] << 8) + pps->ucPPS[9];
	slice_width = (pps->ucPPS[12] << 8) + pps->ucPPS[13];
	slice_num = pic_width / slice_width;

	DPTXMSG("encoder_id = %d\n", encoder_id);
	DPTXMSG("pic_width = %d\n", pic_width);
	DPTXMSG("slice_width = %d\n", slice_width);
	DPTXMSG("slice_num = %d\n", slice_num);
	DPTXMSG("chunk_size = %d\n", chunk_num);

	DPTXMSG("lane count = %d\n", lane_count);
	if (lane_count >= DP_LANECOUNT_2) {
		r16 = (((chunk_num + 2) * slice_num + 2) / 3) % 16;
		DPTXMSG("r16 = %d\n", r16);
		hde_last_num = (q16[r16] & (BIT(1) | BIT(2))) >> 1;
		hde_num_even = q16[r16] & BIT(0);
	} else {
		r8 = (((chunk_num + 1) * slice_num + 2) / 3) % 8;
		DPTXMSG("r8 = %d\n", r8);
		hde_last_num = (q8[r8] & (BIT(1) | BIT(2))) >> 1;
		hde_num_even = q8[r8] & BIT(0);
	}

	mtk_dptx_hal_set_chunk_size(mtk_dp, encoder_id,
				    slice_num - 1,
				    chunk_num,
				    chunk_num % 12,
				    lane_count,
				    hde_last_num,
				    hde_num_even);

}

void mtk_dptx_drv_dsc_set_pps(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, union PPS_T *pps, u8 enable)
{
	u8 hb[4] = {0x0, 0x10, 0x7F, 0x0};
	u8 color_depth_sel[5] = {0x6, 0x8, 0xA, 0xC, 0x0};
	u8 color_depth = mtk_dp->info[encoder_id].depth;

	pps->dp_pps.major = 0x1;
	pps->dp_pps.minor = 0x2;

	if (color_depth == DP_COLOR_DEPTH_6BIT) {
		DPTXMSG("[DSC] Not Support 6 bpc !!\n");
		return;
	}

	pps->dp_pps.color_depth = color_depth_sel[color_depth];
	pps->dp_pps.bp_enable = TRUE;

	switch (mtk_dp->info[encoder_id].format) {
	case DP_COLOR_FORMAT_RGB_444:
		pps->dp_pps.convert_rgb = 0x1;
		pps->dp_pps.native_420 = 0x1;
		break;

	case DP_COLOR_FORMAT_YUV_420:
		pps->dp_pps.convert_rgb = 0x0;
		pps->dp_pps.native_420 = 0x1;
		break;

	case DP_COLOR_FORMAT_YUV_422:
		pps->dp_pps.convert_rgb = 0x0;
		pps->dp_pps.native_422 = 0x1;
		break;

	case DP_COLOR_FORMAT_YUV_444:
		pps->dp_pps.convert_rgb = 0x0;
		break;

	default:
		DPTXMSG("[DSC] Color Format Not support !!\n");
		return;
	}

	pps->dp_pps.pic_height = mtk_dp->info[encoder_id].DPTX_OUTBL.Vde;
	pps->dp_pps.pic_width = mtk_dp->info[encoder_id].DPTX_OUTBL.Hde;

	mtk_dptx_drv_spkg_sdp(mtk_dp, encoder_id, enable, DPTx_SDPTYP_PPS0, hb, pps->ucPPS +  0);
	mtk_dptx_drv_spkg_sdp(mtk_dp, encoder_id, enable, DPTx_SDPTYP_PPS1, hb, pps->ucPPS + 32);
	mtk_dptx_drv_spkg_sdp(mtk_dp, encoder_id, enable, DPTx_SDPTYP_PPS2, hb, pps->ucPPS + 64);
	mtk_dptx_drv_spkg_sdp(mtk_dp, encoder_id, enable, DPTx_SDPTYP_PPS3, hb, pps->ucPPS + 96);
}

#if (DPTX_OS == DPTX_CTP)
void mtk_dptx_drv_video_clock(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id)
{
	u8  clk_req;

	clk_req = mtk_dp->info[encoder_id].resolution;
	//mtk_dptx_hw_video_clock(clk_req);  macshen to check
}
#endif

u8 PPS_4k60_[128] = {
	0x12, 0x00, 0x00, 0x8d, 0x30, 0x80, 0x08, 0x70, 0x0f, 0x00, 0x00, 0x08,
	0x07, 0x80, 0x07, 0x80,	0x02, 0x00, 0x04, 0xc0, 0x00, 0x20, 0x01, 0x1e,
	0x00, 0x1a, 0x00, 0x0c, 0x0d, 0xb7, 0x03, 0x94,	0x18, 0x00, 0x10, 0xf0,
	0x03, 0x0c, 0x20, 0x00, 0x06, 0x0b, 0x0b, 0x33, 0x0e, 0x1c, 0x2a, 0x38,
	0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7b, 0x7d, 0x7e, 0x01, 0x02,
	0x01, 0x00, 0x09, 0x40,	0x09, 0xbe, 0x19, 0xfc, 0x19, 0xfa, 0x19, 0xf8,
	0x1a, 0x38, 0x1a, 0x78, 0x22, 0xb6, 0x2a, 0xb6, 0x2a, 0xf6, 0x2a, 0xf4,
	0x43, 0x34, 0x63, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

union PPS_T PPS_USER[DPTX_ENCODER_ID_MAX];

void mtk_dptx_drv_video_config(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id)
{
	struct DPTX_TIMING_PARAMETER *DPTX_TBL = &mtk_dp->info[encoder_id].DPTX_OUTBL;

	if (!mtk_dp->dp_ready) {
		DPTXERR("%s, DP is not ready!\n", __func__);
		//return;
	}

	if (mtk_dp->info[encoder_id].resolution >= SINK_MAX) {
		DPTXERR("DPTX doesn't support this resolution(%d)!\n",
			 mtk_dp->info[encoder_id].resolution);
		return;
	}

	mtk_dptx_hal_mn_overwrite(mtk_dp, encoder_id, FALSE, 0x0, 0x8000);

	switch (mtk_dp->info[encoder_id].resolution) {
	case SINK_7680_4320:
		DPTX_TBL->FrameRate = 60;
		DPTX_TBL->Htt = 8040;
		DPTX_TBL->Hbp = 240;
		DPTX_TBL->Hsw = 96;
		DPTX_TBL->bHsp = 0;
		DPTX_TBL->Hfp = 24;
		DPTX_TBL->Hde = 7680;
		DPTX_TBL->Vtt = 4381;
		DPTX_TBL->Vbp = 6;
		DPTX_TBL->Vsw = 8;
		DPTX_TBL->bVsp = 0;
		DPTX_TBL->Vfp = 47;
		DPTX_TBL->Vde = 4320;
		break;

	case SINK_3840_2160:
		DPTX_TBL->FrameRate = 60;
		DPTX_TBL->Htt = 4400;
		DPTX_TBL->Hbp = 296;
		DPTX_TBL->Hsw = 88;
		DPTX_TBL->bHsp = 0;
		DPTX_TBL->Hfp = 176;
		DPTX_TBL->Hde = 3840;
		DPTX_TBL->Vtt = 2250;
		DPTX_TBL->Vbp = 72;
		DPTX_TBL->Vsw = 10;
		DPTX_TBL->bVsp = 0;
		DPTX_TBL->Vfp = 8;
		DPTX_TBL->Vde = 2160;
		break;

	case SINK_3840_2160_30:
		DPTX_TBL->FrameRate = 30;
		DPTX_TBL->Htt = 4400;
		DPTX_TBL->Hbp = 296;
		DPTX_TBL->Hsw = 88;
		DPTX_TBL->bHsp = 0;
		DPTX_TBL->Hfp = 176;
		DPTX_TBL->Hde = 3840;
		DPTX_TBL->Vtt = 2250;
		DPTX_TBL->Vbp = 72;
		DPTX_TBL->Vsw = 10;
		DPTX_TBL->bVsp = 0;
		DPTX_TBL->Vfp = 8;
		DPTX_TBL->Vde = 2160;
		break;

	case SINK_2560_1600:
		DPTX_TBL->FrameRate = 60;
		DPTX_TBL->Htt = 2720;
		DPTX_TBL->Hbp = 80;
		DPTX_TBL->Hsw = 32;
		DPTX_TBL->bHsp = 0;
		DPTX_TBL->Hfp = 48;
		DPTX_TBL->Hde = 2560;
		DPTX_TBL->Vtt = 1646;
		DPTX_TBL->Vbp = 37;
		DPTX_TBL->Vsw = 6;
		DPTX_TBL->bVsp = 1;
		DPTX_TBL->Vfp = 3;
		DPTX_TBL->Vde = 1600;
		break;

	case SINK_1920_1440:
		DPTX_TBL->FrameRate = 60;
		DPTX_TBL->Htt = 2600;
		DPTX_TBL->Hbp = 344;
		DPTX_TBL->Hsw = 208;
		DPTX_TBL->bHsp = 1;
		DPTX_TBL->Hfp = 128;
		DPTX_TBL->Hde = 1920;
		DPTX_TBL->Vtt = 1500;
		DPTX_TBL->Vbp = 56;
		DPTX_TBL->Vsw = 3;
		DPTX_TBL->bVsp = 0;
		DPTX_TBL->Vfp = 1;
		DPTX_TBL->Vde = 1440;
		break;

	case SINK_1920_1200:
		DPTX_TBL->FrameRate = 60;
		DPTX_TBL->Htt = 2080;
		DPTX_TBL->Hbp = 80;
		DPTX_TBL->Hsw = 32;
		DPTX_TBL->bHsp = 0;
		DPTX_TBL->Hfp = 48;
		DPTX_TBL->Hde = 1920;
		DPTX_TBL->Vtt = 1235;
		DPTX_TBL->Vbp = 26;
		DPTX_TBL->Vsw = 6;
		DPTX_TBL->bVsp = 0;
		DPTX_TBL->Vfp = 3;
		DPTX_TBL->Vde = 1200;
		break;

	case SINK_1920_1080:
		DPTX_TBL->FrameRate = 60;
		DPTX_TBL->Htt = 2200;
		DPTX_TBL->Hbp = 148;
		DPTX_TBL->Hsw = 44;
		DPTX_TBL->bHsp = 0;
		DPTX_TBL->Hfp = 88;
		DPTX_TBL->Hde = 1920;
		DPTX_TBL->Vtt = 1125;
		DPTX_TBL->Vbp = 36;
		DPTX_TBL->Vsw = 5;
		DPTX_TBL->bVsp = 0;
		DPTX_TBL->Vfp = 4;
		DPTX_TBL->Vde = 1080;
		break;

	case SINK_1080_2460:
		DPTX_TBL->FrameRate = 60;
		DPTX_TBL->Htt = 1172;
		DPTX_TBL->Hbp = 30;
		DPTX_TBL->Hsw = 32;
		DPTX_TBL->bHsp = 1;
		DPTX_TBL->Hfp = 30;
		DPTX_TBL->Hde = 1080;
		DPTX_TBL->Vtt = 2476;
		DPTX_TBL->Vbp = 5;
		DPTX_TBL->Vsw = 2;
		DPTX_TBL->bVsp = 0;
		DPTX_TBL->Vfp = 9;
		DPTX_TBL->Vde = 2460;
		break;

	case SINK_1280_1024:
		DPTX_TBL->FrameRate = 60;
		DPTX_TBL->Htt = 1688;
		DPTX_TBL->Hbp = 248;
		DPTX_TBL->Hsw = 112;
		DPTX_TBL->bHsp = 0;
		DPTX_TBL->Hfp = 88;
		DPTX_TBL->Hde = 1280;
		DPTX_TBL->Vtt = 1066;
		DPTX_TBL->Vbp = 38;
		DPTX_TBL->Vsw = 3;
		DPTX_TBL->bVsp = 0;
		DPTX_TBL->Vfp = 4;
		DPTX_TBL->Vde = 1024;
		break;

	case SINK_1280_960:
		DPTX_TBL->FrameRate = 60;
		DPTX_TBL->Htt = 1800;
		DPTX_TBL->Hbp = 312;
		DPTX_TBL->Hsw = 112;
		DPTX_TBL->bHsp = 0;
		DPTX_TBL->Hfp = 96;
		DPTX_TBL->Hde = 1280;
		DPTX_TBL->Vtt = 1000;
		DPTX_TBL->Vbp = 36;
		DPTX_TBL->Vsw = 3;
		DPTX_TBL->bVsp = 0;
		DPTX_TBL->Vfp = 1;
		DPTX_TBL->Vde = 960;
		break;

	case SINK_1280_720:
		DPTX_TBL->FrameRate = 60;
		DPTX_TBL->Htt = 1650;
		DPTX_TBL->Hbp = 220;
		DPTX_TBL->Hsw = 40;
		DPTX_TBL->bHsp = 0;
		DPTX_TBL->Hfp = 110;
		DPTX_TBL->Hde = 1280;
		DPTX_TBL->Vtt = 750;
		DPTX_TBL->Vbp = 20;
		DPTX_TBL->Vsw = 5;
		DPTX_TBL->bVsp = 0;
		DPTX_TBL->Vfp = 5;
		DPTX_TBL->Vde = 720;
		break;

	case SINK_800_600:
		DPTX_TBL->FrameRate = 60;
		DPTX_TBL->Htt = 1056;
		DPTX_TBL->Hbp = 88;
		DPTX_TBL->Hsw = 128;
		DPTX_TBL->bHsp = 0;
		DPTX_TBL->Hfp = 40;
		DPTX_TBL->Hde = 800;
		DPTX_TBL->Vtt = 628;
		DPTX_TBL->Vbp = 23;
		DPTX_TBL->Vsw = 4;
		DPTX_TBL->bVsp = 0;
		DPTX_TBL->Vfp = 16;
		DPTX_TBL->Vde = 600;
		break;

	case SINK_640_480:
	default:
		DPTX_TBL->FrameRate = 60;
		DPTX_TBL->Htt = 800;
		DPTX_TBL->Hbp = 48;
		DPTX_TBL->Hsw = 96;
		DPTX_TBL->bHsp = 1;
		DPTX_TBL->Hfp = 16;
		DPTX_TBL->Hde = 640;
		DPTX_TBL->Vtt = 525;
		DPTX_TBL->Vbp = 33;
		DPTX_TBL->Vsw = 2;
		DPTX_TBL->bVsp = 1;
		DPTX_TBL->Vfp = 10;
		DPTX_TBL->Vde = 480;
		break;
	}

#ifdef DPTX_DSC_PATCH  // DSC for 4k60 patch
	u8  overwrite = FALSE;
	u32 mvid = 0;

	if (mtk_dp->info[encoder_id].resolution == SINK_3840_2160) {
		// patch for 4k@60 with DSC 3 times compress
		mtk_dp->dsc_enable = TRUE;

		switch (mtk_dp->training_info.ubLinkRate) {
		case DP_LINKRATE_HBR3:
			mvid = 0x5DDE;
			break;

		case DP_LINKRATE_HBR2:
			mvid = 0x8CCD;
			break;
		}

		overwrite = TRUE;
	}

	mtk_dptx_hal_mn_overwrite(mtk_dp, encoder_id, overwrite, mvid, 0x8000);
#endif

	if (mtk_dp->has_dsc) {
		u8 Data[1];

		Data[0] = (u8) mtk_dp->dsc_enable;
		drm_dp_dpcd_write(&mtk_dp->aux, 0x160, Data, 0x1);
	}

	//interlace not support
	DPTX_TBL->Video_ip_mode = DPTX_VIDEO_PROGRESSIVE;
	mtk_dptx_hal_set_msa(mtk_dp, encoder_id, DPTX_TBL);

	mtk_dptx_drv_set_misc(mtk_dp, encoder_id);

	if (mtk_dp->info[encoder_id].bPatternGen) {
		if (mtk_dp->is_mst_start) {
			enum DPTX_STREAM_ID stream_id = (enum DPTX_STREAM_ID) encoder_id;

			DPTXMSG("[DPTX] Enter MST PG\n");
			mtk_dptx_drv_pg_type_sel(mtk_dp, encoder_id,
					mtk_dp->stream_info[stream_id].pg_type,
					DPTX_PG_PURECOLOR_BLUE,
					mtk_dp->stream_info[stream_id].color_depth,
					DPTX_PG_LOCATION_ALL,
					mtk_dp->stream_info[stream_id].final_timing);
		} else {
			DPTXMSG("[DPTX] Enter SST PG\n");
			mtk_dptx_drv_pg_type_sel(mtk_dp, encoder_id,
					DPTX_PG_VERTICAL_COLOR_BAR,
					DPTX_PG_PURECOLOR_BLUE,
					0xFFF,
					DPTX_PG_LOCATION_ALL,
					mtk_dp->info[encoder_id].pattern_id);
		}
	} else
		DPTXMSG("[DPTX] Enter NA PG\n");

	if (!mtk_dp->dsc_enable) {
		mtk_dptx_drv_set_color_depth(mtk_dp, encoder_id,
				mtk_dp->info[encoder_id].depth);
		mtk_dptx_drv_set_color_format(mtk_dp, encoder_id,
				mtk_dp->info[encoder_id].format);
	} else {
		mtk_dptx_drv_dsc_pps_send(mtk_dp, encoder_id, PPS_4k60_);
		mtk_dptx_hal_dsc_enable(mtk_dp, encoder_id, TRUE);
	}
}

void mtk_dptx_drv_dsc_pps_send(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 *pps_128)
{
	u8  dsc_cap[16];
	// u16 chunk_size  = pps_128[14] << 8 | pps_128[15];
	// u16 pic_width   = pps_128[8]  << 8 | pps_128[9];
	// u16 slice_width = pps_128[12] << 8 | pps_128[13];

	if (!mtk_dp->dp_ready) {
		DPTXMSG("%s, DP is not ready!\n", __func__);
		return;
	}

	mtk_dp_get_dsc_capability(dsc_cap);

	pps_128[0x0]
		= ((dsc_cap[0x1] & 0xf) << 4) | ((dsc_cap[0x1] & 0xf0) >> 4);

	if (dsc_cap[0x6] & BIT(0))
		pps_128[0x4] |= (0x1 << 5);
	else
		pps_128[0x4] &= ~(0x1 << 5);

	mtk_dptx_drv_dsc_set_pps(mtk_dp, encoder_id, (union PPS_T *)pps_128, TRUE);
	mtk_dptx_drv_dsc_set_param(mtk_dp, encoder_id, (union PPS_T *)pps_128);
}

#endif

/**
 * mtk_dptx_mst_drv_print_state() - print the status of MST state machine
 */
static void mtk_dptx_mst_drv_print_state(struct mtk_dp *mtk_dp)
{
	switch (mtk_dp->state) {
	case DPTXSTATE_INITIAL:
		DPTXMSG("[MST] DPTXSTATE_INITIAL\n");
		break;
	case DPTXSTATE_AUTH:
		DPTXMSG("[MST] DPTXSTATE_AUTH\n");
		break;
	case DPTXSTATE_PREPARE:
		DPTXMSG("[MST] DPTXSTATE_PREPARE\n");
		break;
	case DPTXSTATE_IDLE:
		DPTXMSG("[MST] DPTXSTATE_IDLE\n");
		break;
	case DPTXSTATE_NORMAL:
		DPTXMSG("[MST] DPTXSTATE_NORMAL\n");
		break;
	default:
		DPTXERR("[MST] ERROR State !\n");
		break;
	}
}

/**
 * mtk_dptx_mst_drv_init_variable() - initialize all MST relative SW parameters
 */
static void mtk_dptx_mst_drv_init_variable(struct mtk_dp *mtk_dp)
{
	enum DPTX_STREAM_ID stream_id;

	/* enc_id equals to stream_id */
	for (stream_id = DPTX_STREAM_ID_0; stream_id < DPTX_STREAM_MAX; stream_id++) {
		//mtk_dp->stream_info[stream_id].ideal_timing = SINK_1920_1080;
		mtk_dp->stream_info[stream_id].final_timing = SINK_MAX;
		//mtk_dp->stream_info[stream_id].color_depth = DPTX_COLOR_DEPTH_8BIT;
		mtk_dp->stream_info[stream_id].pg_type = DPTX_PG_HORIZONTAL_RAMPING + stream_id;
		mtk_dp->stream_info[stream_id].color_format = DP_COLOR_FORMAT_RGB_444;
		mtk_dp->stream_info[stream_id].is_dsc = FALSE;
		mtk_dp->stream_info[stream_id].audio_freq = FS_192K;
		mtk_dp->stream_info[stream_id].audio_ch = 2;
		mtk_dp->stream_info[stream_id].port = NULL;

		memset(&mtk_dp->info[stream_id], 0x0, sizeof(struct DPTX_INFO));

		mtk_dp->info[stream_id].input_src = DPTX_SRC_PG;
		mtk_dp->info[stream_id].bPatternGen = TRUE;
		//mtk_dp->info[stream_id].pattern_id =
			//mtk_dp->stream_info[stream_id].ideal_timing;
		mtk_dp->info[stream_id].format = DP_COLOR_FORMAT_RGB_444;
		//mtk_dp->info[stream_id].depth =
			//mtk_dp->stream_info[stream_id].color_depth;
		//mtk_dp->info[stream_id].DPTX_OUTBL.FrameRate = 60;
	}

	mtk_dp->stream_info[DPTX_STREAM_ID_0].pg_type = DPTX_PG_VERTICAL_COLOR_BAR;
#if (DPTX_ENCODER_NUM >= 2)
	mtk_dp->stream_info[DPTX_STREAM_ID_1].pg_type = DPTX_PG_HORIZONTAL_COLOR_BAR;
#endif

	//memset(&port_debug_real[0][0], 0x0,
	//		DPTX_LCT_MAX  * DPTX_PORT_NUM_MAX * sizeof(struct drm_dp_mst_port));
	//memset(&branch_debug_real[0], 0x0, DPTX_LCT_MAX * sizeof(struct drm_dp_mst_branch));

	dptx_current_slot = DP_PAYLOAD_START_SLOT  + dptx_mst_extended_start_slot;
}

static void mtk_dptx_mst_drv_fec_enable(struct mtk_dp *mtk_dp, const u8 enable)
{
	//u8 dpcd_buf = 0x0;

	if (enable == TRUE) {
		//drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00120, &dpcd_buf, 0x1);

		//dpcd_buf |= BIT(0);
		mhal_DPTx_EnableFEC(mtk_dp, TRUE);
		//drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00120, &dpcd_buf, 0x1);

		DPTXMSG("Set FEC on !\r\n");
	} else {
		//drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00120, &dpcd_buf, 0x1);
		//dpcd_buf = dpcd_buf & (~BIT(0));
		mhal_DPTx_EnableFEC(mtk_dp, FALSE);
		//drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00120, &dpcd_buf, 0x1);
		mtk_dp->is_mst_fec_en = FALSE;
		DPTXMSG("Set FEC Off !\r\n");
	}
}

/**
 * mtk_dptx_mst_drv_stream_enable() - start to output video & audio streams
 */
static void mtk_dptx_mst_drv_stream_enable(struct mtk_dp *mtk_dp)
{
	enum DPTX_STREAM_ID stream_id;
	enum DPTX_ENCODER_ID encoder_id;
	UINT8 channel;
	enum AUDIO_FS fs;
	enum AUDIO_WORD_LEN len = WL_24bit;
	enum DPTX_AUDIO_M_DIV div = DPTX_AUDIO_M_DIV_D2;
#if (DPTX_OS == DPTX_LINUX)
	struct drm_dp_mst_topology_state *state;
#endif

#if (DPTX_OS == DPTX_CTP)
	mtk_dptx_mst_hal_stream_enable(mtk_dp, mtk_dp->mgr.vcpi_mask, mtk_dp->mgr.max_payloads);
#else
	state = to_drm_dp_mst_topology_state(mtk_dp->mgr.base.state);
	mtk_dptx_mst_hal_stream_enable(mtk_dp, state->payload_mask, mtk_dp->mgr.max_payloads);
#endif


	for (stream_id = DPTX_STREAM_ID_0; stream_id < DPTX_STREAM_MAX; stream_id++) {
		encoder_id = (enum DPTX_ENCODER_ID) stream_id;
		channel = mtk_dp->stream_info[stream_id].audio_ch;
		fs = mtk_dp->stream_info[stream_id].audio_freq;

		if (mtk_dp->stream_info[stream_id].final_timing < SINK_MAX) {
			DPTXMSG("Encoder %d, PG type %d, Color Depth %d, pattern_id %d\n",
				 encoder_id,
				 mtk_dp->stream_info[stream_id].pg_type,
				 mtk_dp->stream_info[stream_id].color_depth,
				 mtk_dp->stream_info[stream_id].final_timing);

			mtk_dp->info[encoder_id].resolution =
				mtk_dp->stream_info[stream_id].final_timing;
			mtk_dp->info[encoder_id].depth =
				mtk_dp->stream_info[stream_id].color_depth;
			mtk_dp->info[encoder_id].format =
				mtk_dp->stream_info[stream_id].color_format;
			mtk_dp->info[encoder_id].pattern_id =
				mtk_dp->stream_info[stream_id].final_timing;

			mtk_dptx_drv_video_enable(mtk_dp, encoder_id, TRUE);
			if (mtk_dp->audio_enable) {
				if (mtk_dp->info[encoder_id].input_src == DPTX_SRC_PG) {
					DPTXMSG("audio %d ch %d, Fs %d, len %d, div %d\n",
								encoder_id, channel, fs, len, div);

					mtk_dptx_hal_audio_pg_enable(mtk_dp, encoder_id, channel, fs, TRUE);
					mtk_dptx_hal_audio_ch_status_set(mtk_dp, encoder_id,
									channel, fs, len);
					mtk_dptx_hal_audio_set_mdiv(mtk_dp, encoder_id, div);
					mtk_dptx_hal_audio_sdp_setting(mtk_dp, encoder_id, channel);
				} else {
					mtk_dptx_hal_audio_pg_enable(mtk_dp, encoder_id,
							channel, fs, FALSE);
					mtk_dptx_drv_i2s_audio_config(mtk_dp, encoder_id);
					//mtk_dptx_hw_audio_setting(channel, TRUE);

				}
				mtk_dptx_drv_audio_mute(mtk_dp, encoder_id, FALSE);
				//mtk_dptx_hw_audio_clock(channel, fs);
			}

		}
	}
}

/**
 * mtk_dptx_mst_drv_vcp_table_update() - update vcp table for all devices in the topology
 */
static void mtk_dptx_mst_drv_update_vcp_table(struct mtk_dp *mtk_dp)
{
	enum DPTX_ENCODER_ID encoder_id = 0;
	u16 start_slot, end_slot;
	u16 mst_stream_en_mask;
	u16 mst_stream_en_shift;
	//u32 vcpi_mask_local_temp;
	u32 vcpi_mask_local;
	int payload_idx;
#if (DPTX_OS == DPTX_CTP)
	struct drm_dp_payload *payload;
#else
	struct drm_dp_mst_atomic_payload *payload;
	struct drm_dp_mst_topology_state *state;
#endif


	mst_stream_en_shift = MIN(8, mtk_dp->mgr.max_payloads);
	mst_stream_en_mask = (1 << mst_stream_en_shift) - 1;
#if (DPTX_OS == DPTX_CTP)
	vcpi_mask_local = mtk_dp->mgr.vcpi_mask & mst_stream_en_mask;
#else
	state = to_drm_dp_mst_topology_state(mtk_dp->mgr.base.state);
	vcpi_mask_local = state->payload_mask & mst_stream_en_mask;
#endif
	//mtk_dp->enc_id = DPTX_ENC_ID_0;

	mtk_dptx_mst_hal_reset_payload(mtk_dp);

	if (vcpi_mask_local == 0) {
		DPTXMSG("No payload id was assigned, skip payload configuration\n");
		return;
	}

	//for (payload_idx = (DPTX_STREAM_MAX - 1); payload_idx >= 0; payload_idx--) {
	for (payload_idx = 0; payload_idx < DPTX_STREAM_MAX; payload_idx++) {
		if (((vcpi_mask_local >> payload_idx) & 0x1) == 0x0)
			continue;

#if (DPTX_OS == DPTX_CTP)
		payload = &gDPTx_MstPayload[payload_idx]; //macshen todo

		start_slot = payload->start_slot;
		end_slot = start_slot + payload->num_slots;

		mtk_dptx_mst_hal_set_mtp_size(mtk_dp, encoder_id, payload->num_slots);

		//vcpi_mask_local_temp = vcpi_mask_local >> 1;

		DPTXMSG("Start allocate VCPI %d, start slot %d, end slot %d\n",
			payload->vcpi, start_slot, end_slot - 1);
		/* reg_vc_payload_timeslot */
		if ((payload->vcpi < 1) || (payload->vcpi > (DPTX_STREAM_MAX)))
			DPTXERR("Invalid VCPI, vcpi %d\n", payload->vcpi);
		else if ((start_slot > 64) || (end_slot > 64))
			DPTXERR("Invalid slot region, start_slot %d, end_slot %d\n",
					start_slot, end_slot);
		else {
			mtk_dptx_mst_hal_set_vcpi(mtk_dp, encoder_id, start_slot, end_slot, payload->vcpi);
			mtk_dptx_mst_hal_set_id_buf(mtk_dp, encoder_id, payload->vcpi);
		}
#else
		payload = drm_atomic_get_mst_payload_state(state, mtk_dp->mst_connector[encoder_id]->port);

		start_slot = payload->vc_start_slot;
		end_slot = start_slot + payload->time_slots;

		mtk_dptx_mst_hal_set_mtp_size(mtk_dp, encoder_id, payload->time_slots);

		//vcpi_mask_local_temp = vcpi_mask_local >> 1;

		DPTXMSG("Start allocate VCPI %d, start slot %d, end slot %d\n",
			payload->vcpi, start_slot, end_slot - 1);
		/* reg_vc_payload_timeslot */
		if ((payload->vcpi < 1) || (payload->vcpi > (DPTX_STREAM_MAX)))
			DPTXERR("Invalid VCPI, vcpi %d\n", payload->vcpi);
		else if ((start_slot > 64) || (end_slot > 64))
			DPTXERR("Invalid slot region, start_slot %d, end_slot %d\n",
					start_slot, end_slot);
		else {
			mtk_dptx_mst_hal_set_vcpi(mtk_dp, encoder_id, start_slot, end_slot, payload->vcpi);
			mtk_dptx_mst_hal_set_id_buf(mtk_dp, encoder_id, payload->vcpi);
		}
#endif
		encoder_id++;
	}

	mtk_dptx_mst_hal_vcp_table_update(mtk_dp);
}

/**
 * mtk_dptx_mst_drv_update_payload() - update payload by DRM API
 */
static void mtk_dptx_mst_drv_update_payload(struct mtk_dp *mtk_dp)
{
	signed char status;
	signed char counter = 5;

	DPTXMSG("update_payload_part1\n");
#if (DPTX_OS == DPTX_CTP)
	drm_dp_update_payload_part1(&mtk_dp->mgr, dptx_current_slot); //macshen todo
#endif
	/* Base on vcpi to update payload, should assign payload ID first */
	mtk_dptx_mst_drv_update_vcp_table(mtk_dp);

	do {
		mtk_dptx_mst_hal_trigger_act(mtk_dp);
		DPTXMSG("trigger_act\n");
		status = drm_dp_check_act_status(&mtk_dp->mgr);
		DPTXMSG("DPCD_0020C status %x\n", status);
		if (counter-- <= 0)
			break;
	} while (!((status & DP_PAYLOAD_ACT_HANDLED) && (status > 0)));

	DPTXMSG("update_payload_part2\n");

#if (DPTX_OS == DPTX_CTP)
	drm_dp_update_payload_part2(&mtk_dp->mgr); //macshen todo
#endif
}

void mtk_dptx_drv_video_timing_parsing(const enum DP_VIDEO_TIMING_TYPE resolution,
								struct DPTX_TIMING_PARAMETER *p_DPTX_TBL)
{
	switch (resolution) {
	case SINK_7680_4320:
		p_DPTX_TBL->FrameRate = 60;
		p_DPTX_TBL->Htt = 8040;
		p_DPTX_TBL->Hbp = 240;
		p_DPTX_TBL->Hsw = 96;
		p_DPTX_TBL->bHsp = 0;
		p_DPTX_TBL->Hfp = 24;
		p_DPTX_TBL->Hde = 7680;
		p_DPTX_TBL->Vtt = 4381;
		p_DPTX_TBL->Vbp = 6;
		p_DPTX_TBL->Vsw = 8;
		p_DPTX_TBL->bVsp = 0;
		p_DPTX_TBL->Vfp = 47;
		p_DPTX_TBL->Vde = 4320;
		break;
	case SINK_3840_2160:
		p_DPTX_TBL->FrameRate = 60;
		p_DPTX_TBL->Htt = 4400;
		p_DPTX_TBL->Hbp = 296;
		p_DPTX_TBL->Hsw = 88;
		p_DPTX_TBL->bHsp = 0;
		p_DPTX_TBL->Hfp = 176;
		p_DPTX_TBL->Hde = 3840;
		p_DPTX_TBL->Vtt = 2250;
		p_DPTX_TBL->Vbp = 72;
		p_DPTX_TBL->Vsw = 10;
		p_DPTX_TBL->bVsp = 0;
		p_DPTX_TBL->Vfp = 8;
		p_DPTX_TBL->Vde = 2160;
		break;
	case SINK_3840_2160_30:
		p_DPTX_TBL->FrameRate = 30;
		p_DPTX_TBL->Htt = 4400;
		p_DPTX_TBL->Hbp = 296;
		p_DPTX_TBL->Hsw = 88;
		p_DPTX_TBL->bHsp = 0;
		p_DPTX_TBL->Hfp = 176;
		p_DPTX_TBL->Hde = 3840;
		p_DPTX_TBL->Vtt = 2250;
		p_DPTX_TBL->Vbp = 72;
		p_DPTX_TBL->Vsw = 10;
		p_DPTX_TBL->bVsp = 0;
		p_DPTX_TBL->Vfp = 8;
		p_DPTX_TBL->Vde = 2160;
		break;
	case SINK_2560_1600:
		p_DPTX_TBL->FrameRate = 60;
		p_DPTX_TBL->Htt = 2720;
		p_DPTX_TBL->Hbp = 80;
		p_DPTX_TBL->Hsw = 32;
		p_DPTX_TBL->bHsp = 0;
		p_DPTX_TBL->Hfp = 48;
		p_DPTX_TBL->Hde = 2560;
		p_DPTX_TBL->Vtt = 1646;
		p_DPTX_TBL->Vbp = 37;
		p_DPTX_TBL->Vsw = 6;
		p_DPTX_TBL->bVsp = 1;
		p_DPTX_TBL->Vfp = 3;
		p_DPTX_TBL->Vde = 1600;
		break;
	case SINK_1920_1440:
		p_DPTX_TBL->FrameRate = 60;
		p_DPTX_TBL->Htt = 2600;
		p_DPTX_TBL->Hbp = 344;
		p_DPTX_TBL->Hsw = 208;
		p_DPTX_TBL->bHsp = 1;
		p_DPTX_TBL->Hfp = 128;
		p_DPTX_TBL->Hde = 1920;
		p_DPTX_TBL->Vtt = 1500;
		p_DPTX_TBL->Vbp = 56;
		p_DPTX_TBL->Vsw = 3;
		p_DPTX_TBL->bVsp = 0;
		p_DPTX_TBL->Vfp = 1;
		p_DPTX_TBL->Vde = 1440;
		break;
	case SINK_1920_1200:
		p_DPTX_TBL->FrameRate = 60;
		p_DPTX_TBL->Htt = 2080;
		p_DPTX_TBL->Hbp = 80;
		p_DPTX_TBL->Hsw = 32;
		p_DPTX_TBL->bHsp = 0;
		p_DPTX_TBL->Hfp = 48;
		p_DPTX_TBL->Hde = 1920;
		p_DPTX_TBL->Vtt = 1235;
		p_DPTX_TBL->Vbp = 26;
		p_DPTX_TBL->Vsw = 6;
		p_DPTX_TBL->bVsp = 0;
		p_DPTX_TBL->Vfp = 3;
		p_DPTX_TBL->Vde = 1200;
		break;
	case SINK_1920_1080:
		p_DPTX_TBL->FrameRate = 60;
		p_DPTX_TBL->Htt = 2200;
		p_DPTX_TBL->Hbp = 148;
		p_DPTX_TBL->Hsw = 44;
		p_DPTX_TBL->bHsp = 0;
		p_DPTX_TBL->Hfp = 88;
		p_DPTX_TBL->Hde = 1920;
		p_DPTX_TBL->Vtt = 1125;
		p_DPTX_TBL->Vbp = 36;
		p_DPTX_TBL->Vsw = 5;
		p_DPTX_TBL->bVsp = 0;
		p_DPTX_TBL->Vfp = 4;
		p_DPTX_TBL->Vde = 1080;
		break;
	case SINK_1080_2460:
		p_DPTX_TBL->FrameRate = 60;
		p_DPTX_TBL->Htt = 1172;
		p_DPTX_TBL->Hbp = 30;
		p_DPTX_TBL->Hsw = 32;
		p_DPTX_TBL->bHsp = 1;
		p_DPTX_TBL->Hfp = 30;
		p_DPTX_TBL->Hde = 1080;
		p_DPTX_TBL->Vtt = 2476;
		p_DPTX_TBL->Vbp = 5;
		p_DPTX_TBL->Vsw = 2;
		p_DPTX_TBL->bVsp = 0;
		p_DPTX_TBL->Vfp = 9;
		p_DPTX_TBL->Vde = 2460;
		break;
	case SINK_1280_1024:
		p_DPTX_TBL->FrameRate = 60;
		p_DPTX_TBL->Htt = 1688;
		p_DPTX_TBL->Hbp = 248;
		p_DPTX_TBL->Hsw = 112;
		p_DPTX_TBL->bHsp = 0;
		p_DPTX_TBL->Hfp = 88;
		p_DPTX_TBL->Hde = 1280;
		p_DPTX_TBL->Vtt = 1066;
		p_DPTX_TBL->Vbp = 38;
		p_DPTX_TBL->Vsw = 3;
		p_DPTX_TBL->bVsp = 0;
		p_DPTX_TBL->Vfp = 4;
		p_DPTX_TBL->Vde = 1024;
		break;
	case SINK_1280_960:
		p_DPTX_TBL->FrameRate = 60;
		p_DPTX_TBL->Htt = 1800;
		p_DPTX_TBL->Hbp = 312;
		p_DPTX_TBL->Hsw = 112;
		p_DPTX_TBL->bHsp = 0;
		p_DPTX_TBL->Hfp = 96;
		p_DPTX_TBL->Hde = 1280;
		p_DPTX_TBL->Vtt = 1000;
		p_DPTX_TBL->Vbp = 36;
		p_DPTX_TBL->Vsw = 3;
		p_DPTX_TBL->bVsp = 0;
		p_DPTX_TBL->Vfp = 1;
		p_DPTX_TBL->Vde = 960;
		break;
	case SINK_1280_720:
		p_DPTX_TBL->FrameRate = 60;
		p_DPTX_TBL->Htt = 1650;
		p_DPTX_TBL->Hbp = 220;
		p_DPTX_TBL->Hsw = 40;
		p_DPTX_TBL->bHsp = 0;
		p_DPTX_TBL->Hfp = 110;
		p_DPTX_TBL->Hde = 1280;
		p_DPTX_TBL->Vtt = 750;
		p_DPTX_TBL->Vbp = 20;
		p_DPTX_TBL->Vsw = 5;
		p_DPTX_TBL->bVsp = 0;
		p_DPTX_TBL->Vfp = 5;
		p_DPTX_TBL->Vde = 720;
		break;
	case SINK_800_600:
		p_DPTX_TBL->FrameRate = 60;
		p_DPTX_TBL->Htt = 1056;
		p_DPTX_TBL->Hbp = 88;
		p_DPTX_TBL->Hsw = 128;
		p_DPTX_TBL->bHsp = 0;
		p_DPTX_TBL->Hfp = 40;
		p_DPTX_TBL->Hde = 800;
		p_DPTX_TBL->Vtt = 628;
		p_DPTX_TBL->Vbp = 23;
		p_DPTX_TBL->Vsw = 4;
		p_DPTX_TBL->bVsp = 0;
		p_DPTX_TBL->Vfp = 16;
		p_DPTX_TBL->Vde = 600;
		break;
	case SINK_640_480:
	default:
		p_DPTX_TBL->FrameRate = 60;
		p_DPTX_TBL->Htt = 800;
		p_DPTX_TBL->Hbp = 48;
		p_DPTX_TBL->Hsw = 96;
		p_DPTX_TBL->bHsp = 1;
		p_DPTX_TBL->Hfp = 16;
		p_DPTX_TBL->Hde = 640;
		p_DPTX_TBL->Vtt = 525;
		p_DPTX_TBL->Vbp = 33;
		p_DPTX_TBL->Vsw = 2;
		p_DPTX_TBL->bVsp = 1;
		p_DPTX_TBL->Vfp = 10;
		p_DPTX_TBL->Vde = 480;
		break;
	}
}

/**
 * mtk_dptx_mst_drv_choose_timing() - select output video timing by mtk policy
 * @avai_pbn: current available pbns of the link bandwidth
 */
static int mtk_dptx_mst_drv_choose_timing(struct mtk_dp *mtk_dp, enum DPTX_STREAM_ID stream_id, int avai_pbn)
{
	u32 pixel_clock;
	u32 allocate_pbn;
	enum DP_VIDEO_TIMING_TYPE pattern_id;
	u32 htt, vtt;
	u8  bpp;
	//res: config expected video resolution
	//color_depth: config expected video color depth
	//is_dsc: is this video need DSC function or not
	u8 res, color_depth, is_dsc;

	res = mtk_dp->stream_info[stream_id].ideal_timing;
	color_depth = mtk_dp->stream_info[stream_id].color_depth;
	is_dsc = mtk_dp->stream_info[stream_id].is_dsc;

	bpp =  mtk_dptx_hal_get_color_info(color_depth, mtk_dp->stream_info[stream_id].color_format);

	if (avai_pbn < 0) {
		DPTXERR("Available PBN is invalid, available PBN %d\n", avai_pbn);
		return -1;
	}

	/* search timing to meet current available PBN */
	for (pattern_id = res; pattern_id >= 0; pattern_id--) {
		//mtk_dp->info[stream_id].resolution = pattern_id;
		//mtk_dptx_drv_timing_config();

		mtk_dptx_drv_video_timing_parsing(pattern_id,
					&mtk_dp->info[stream_id].DPTX_OUTBL);
		if (mtk_dp->dsc_enable) {
			//Calculate pixel clock for Compressed timing
			//According formula of eDP simulation
			//Compressed HDE = CEIL(MSA_HDE * bpp/12/8)*4
			UINT8 DSCbpp = ((PPS_USER[stream_id].ucPPS[4] & 0x3) << 4) |
						(PPS_USER[stream_id].ucPPS[5]  >> 4); //1/16;
			u16 MSAHDE = mtk_dp->info[stream_id].DPTX_OUTBL.Hde;
			u16 MSAHBP = mtk_dp->info[stream_id].DPTX_OUTBL.Hbp;
			u16 MSAHFP = mtk_dp->info[stream_id].DPTX_OUTBL.Hfp;
			u16 MSAHSW = mtk_dp->info[stream_id].DPTX_OUTBL.Hsw;
			u16 DSCHDE = ((MSAHDE * DSCbpp + (12 * 8 - 1)) / (12 * 8)) * 4;

			htt = DSCHDE + MSAHBP + MSAHFP + MSAHSW;
		} else
			htt = mtk_dp->info[stream_id].DPTX_OUTBL.Htt;

		vtt = mtk_dp->info[stream_id].DPTX_OUTBL.Vtt;
		pixel_clock = htt * vtt * mtk_dp->info[stream_id].DPTX_OUTBL.FrameRate;

		/* unit: kBps, 640_480x30 fps = 12.6MBps, 7680_4320x120 fps = 4,226.8 MBps */
		pixel_clock = (pixel_clock + (1000 - 1)) / 1000;
		allocate_pbn = drm_dp_calc_pbn_mode(pixel_clock, bpp);

		DPTXMSG("pattern_id %d, htt %d, vtt %d, FrameRate %d, bpp %d, pixel_clock %d\n",
			pattern_id, htt, vtt, mtk_dp->info[stream_id].DPTX_OUTBL.FrameRate,
			bpp, pixel_clock);

		if (allocate_pbn < (u32) avai_pbn) {
			mtk_dp->stream_info[stream_id].final_timing = pattern_id;
			DPTXMSG("Chose timing done, pattern_id %d\n", pattern_id);
			DPTXMSG("require PBN %d, available PBN %d\n",
				 allocate_pbn, (avai_pbn - allocate_pbn));

			return allocate_pbn;
		}

		DPTXERR("Chose resolution fail, pattern_num %d\n",
			 pattern_id);
		DPTXERR("require_PBN %d, available PBN %d\n",
			 allocate_pbn, avai_pbn);
		mtk_dp->stream_info[stream_id].final_timing = SINK_MAX;
		return allocate_pbn;
	}

	mtk_dp->stream_info[stream_id].final_timing = SINK_MAX;

	return -1;
}

static u8 mtk_dptx_mst_drv_find_vcpi_slots(struct mtk_dp *mtk_dp, const int pbn_allocating)
{
#if (DPTX_OS == DPTX_CTP)
	signed char slots = drm_dp_find_vcpi_slots(&mtk_dp->mgr, pbn_allocating);
#else
	signed char slots = 60; //macshen todo
#endif

	DPTXDBG("Slots before fine-tune %d\n", slots);

	switch (mtk_dp->training_info.ubLinkLaneCount) {
	case DP_LANECOUNT_1:
		slots += (4 - (slots % 4));
		break;
	case DP_LANECOUNT_2:
		slots += (2 - (slots % 2));
		break;
	case DP_LANECOUNT_4:
		slots++;
		break;
	}

	//mdr_dptx_mst_GetReqSlotForAudioSymbol(dpTx_ID, dpOutStreamID, &slots);

	if ((slots < 1) || (slots > 63)) {
		DPTXERR("Un-expected slots %d\n", slots);
		return 0;
	}

	DPTXDBG("Slots after fine-tune %d\n", slots);
	return slots;
}

/**
 * mtk_dptx_mst_drv_allocate_vcpi() - allocate vcp id to all devices in the topology
 *
 * According to the output video timing for each device, calculate its pbn and vcpi
 */
static int mtk_dptx_mst_drv_allocate_vcpi(struct mtk_dp *mtk_dp,
		struct drm_dp_mst_branch *mstb, enum DPTX_STREAM_ID *p_stream_id, const u8 is_enable)
{
	struct drm_dp_mst_port *port;
	int allocate_pbn;
	u32 slots;
	int ret = 1;
	int ret_tmp = 0;
	int avail_pbn = drm_dp_get_vc_payload_bw(&mtk_dp->mgr,
		mtk_dp->training_info.ubLinkRate, mtk_dp->training_info.ubLinkLaneCount) * 63;
	enum DPTX_STREAM_ID  stream_id = *p_stream_id;

	list_for_each_entry(port, &mstb->ports, next) {
		struct drm_dp_mst_branch *mstb_child = NULL;

		if (port->input || !port->ddps) {
			DPTXMSG("Skip! This port's input %d ddps %d !\n",
				port->input, port->ddps);
			continue;
		}

#if (DPTX_OS == DPTX_CTP)
		if (is_enable && drm_dp_mst_is_end_device(port->pdt, port->mcs)) {
#else
		if (is_enable) { //macshen todo
#endif
			if (port->port_num >= DPTX_PORT_NUM_MAX) {
				continue;
				DPTXMSG("skip port_num %d!\n", port->port_num);
			}
			if (stream_id >= DPTX_STREAM_MAX) {
				DPTXMSG("Return! All streams have been allocated !\n");
				return 0;
			}
			DPTXMSG("allocate vcpi on LCT %d with port_num %d\n",
						mstb->lct, port->port_num);
			allocate_pbn = mtk_dptx_mst_drv_choose_timing(mtk_dp, stream_id, avail_pbn);

			DPTXMSG("Previous new allocating PBN %d\n", allocate_pbn);

			if (allocate_pbn < 0)
				continue;

			slots = mtk_dptx_mst_drv_find_vcpi_slots(mtk_dp, allocate_pbn);
			avail_pbn -= drm_dp_get_vc_payload_bw(&mtk_dp->mgr,
				mtk_dp->training_info.ubLinkRate, mtk_dp->training_info.ubLinkLaneCount) * slots;
		#if DPTX_MST_DEBUG
			slots += dptx_mst_extended_slot;
		#endif

#if (DPTX_OS == DPTX_CTP)
			DPTXDBG("Slots %d, PBN %d, pbn_div %d\n",
						slots, allocate_pbn, mtk_dp->mgr.pbn_div);
			ret_tmp = drm_dp_mst_allocate_vcpi(&mtk_dp->mgr, port, allocate_pbn, slots); //macshen todo
#endif

			mtk_dp->stream_info[stream_id].port = port;

			(*p_stream_id)++;
			stream_id = *p_stream_id;

			if (ret_tmp > 0)
				ret = ret_tmp;
		}

#if (DPTX_OS == DPTX_CTP)
		if (port->mstb)
			mstb_child = drm_dp_mst_topology_get_mstb_validated(&mtk_dp->mgr, port->mstb);
#endif
		if (mstb_child) {
			ret_tmp = mtk_dptx_mst_drv_allocate_vcpi(mtk_dp, mstb_child, p_stream_id, is_enable);
			stream_id = *p_stream_id;
			if (ret_tmp < 0)
				ret = ret_tmp;
#if (DPTX_OS == DPTX_CTP)
			drm_dp_mst_topology_put_mstb(mstb_child);
#endif
		}
	}
	return ret;
}

/**
 * mtk_dptx_mst_drv_allocate_stream() - allocate vcpi and streams
 */
static void mtk_dptx_mst_drv_allocate_stream(struct mtk_dp *mtk_dp, bool is_enable)
{
	struct drm_dp_mst_branch *mstb = mtk_dp->mgr.mst_primary;
	enum DPTX_STREAM_ID stream_id = 0;

	DPTXMSG("Start check_and_allocate_vcpi\n");
	mtk_dptx_mst_drv_allocate_vcpi(mtk_dp, mstb, &stream_id, is_enable);
	mtk_dptx_mst_drv_stream_enable(mtk_dp);
}

/**
 * mtk_dptx_mst_drv_clear_vcpi() - clear vcpi table for devices in the topology
 */
static void mtk_dptx_mst_drv_clear_vcpi(struct mtk_dp *mtk_dp)
{
	enum DPTX_STREAM_ID stream_id;
	u8 temp_value[0x3] = {0x0, 0x0, 0x3F};
	UINT8 ret = 0;

	for (stream_id = DPTX_STREAM_ID_0; stream_id < DPTX_STREAM_MAX; stream_id++) {
		DPTXDBG("Stream %d\n", (int)stream_id);
#if (DPTX_OS == DPTX_CTP)
		if (mtk_dp->stream_info[stream_id].port  != 0)
			drm_dp_mst_deallocate_vcpi(&mtk_dp->mgr, mtk_dp->stream_info[stream_id].port); //macshen todo
#endif
		mtk_dp->stream_info[stream_id].port = NULL;
		mtk_dp->stream_info[stream_id].final_timing = SINK_MAX;
	}
	mtk_dp->mgr.payload_id_table_cleared = FALSE;

	/* clear DPCD_001C0 ~ DPCD_001C2 */
	ret = drm_dp_dpcd_write(&mtk_dp->aux, DPCD_001C0, temp_value, 0x3);
	DPTXMSG("Clear DPCD_001C0 ~ DPCD_001C2, result %d\n", ret);
}

#if (DPTX_OS == DPTX_CTP)
BYTE *mtk_dptx_mst_drv_send_remote_i2c_read(
								struct drm_dp_mst_branch *mstb,
								struct drm_dp_mst_port *port)
{
	#define msg_num 2
	struct i2c_msg msgs[3];
	int ret = 1;
	u8 block_cnt = 0;
	u8 i2c_data_to_write_buff[1] = {0};
	u8 i2c_data_to_write_buff_segment[1] = {0x0};
	u8 block_index = 0;
	u8 segment_index = 0;
	u8 edid_temp[128] = {0x0};
	u8 *edid = vmalloc(DPTX_EDID_SIZE);

	if (edid == NULL) {
		DPTXERR("malloc EDID fail\n");
	} else {
		//memset(edid, 0, DPTX_EDID_SIZE);
		DPTXMSG("malloc for EDID %p\n", edid);
	}

	msgs[0].addr = 0x30; //Write_I2C_Device_Identifier
	msgs[0].buf = i2c_data_to_write_buff_segment; //I2C_Data_To_Write
	msgs[0].flags = I2C_M_STOP; // No_Stop_Bit
	msgs[0].len = 1; //Number_Of_Bytes_To_Write

	msgs[1].addr = 0x50; //Write_I2C_Device_Identifier
	msgs[1].buf = i2c_data_to_write_buff; //I2C_Data_To_Write
	msgs[1].flags = I2C_M_STOP; // No_Stop_Bit
	msgs[1].len = 1; //Number_Of_Bytes_To_Write

	msgs[2].addr = 0x50; //Write_I2C_Device_Identifier
	msgs[2].buf = edid_temp;
	msgs[2].flags = 0; // Don't care
	msgs[2].len = 128; //Number_Of_Bytes_To_Read

	//if (port->port_num >= 8) {
	if (port->pdt == DP_PEER_DEVICE_SST_SINK) {
		do {
			DPTXMSG("Start to do I2C read, lct %u, port_num %u, mstb %p, port %p\n",
						mstb->lct, port->port_num, mstb, port);
#if (DPTX_OS == DPTX_CTP)
			if (segment_index <= 1)
				ret = drm_dp_mst_i2c_read(mstb, port,
					&msgs[1-segment_index], 2 + segment_index);
			else {
				DPTXERR("Un-supported EDID size %x\n", block_cnt * 128);
				break;
			}
#endif
			if (ret < 0)
				DPTXERR("Remote I2C read Error, lct %u, port_num %u\n",
						mstb->lct, port->port_num);

			memcpy(&(edid[block_index * 128]), msgs[2].buf, sizeof(BYTE) * 128);

			if (block_index == 0)
				block_cnt = msgs[2].buf[0x7E];

			block_index++;
			segment_index = (block_index >> 1);

			i2c_data_to_write_buff[0] = 128 * (block_index % 2);

			if (segment_index >= 1)
				i2c_data_to_write_buff_segment[0] = segment_index;
		} while (block_cnt-- != 0);

	}
#if ENABLE_DPTX_MST_DEBUG
	{
		u16 i = 0;

		DPTXMSG("EDID total msg:");
		for (i = 0; i < DPTX_EDID_SIZE; i++) {
			if ((i%0x10) == 0x0)
				DPTXMSG("\nidx %x: ", i);
			DPTXMSG("%x ", edid[i]);
		}
		DPTXMSG("\n");
	}
#endif
	#undef msg_num
	return edid;
}
#endif

static signed short mtk_dptx_mst_drv_send_remote_dpcd_read_fec_en(
						struct drm_dp_mst_topology_mgr *mgr,
						struct drm_dp_mst_branch *mstb, u16 *p_bytes)
{
	struct drm_dp_mst_port *port;
	signed short ret = 0;
	signed short ret_tmp = 0;
	u16 bytes_tmp = 0;

	port = list_first_entry(&mstb->ports, typeof(*port), next);

	list_for_each_entry(port, &mstb->ports, next) {
		struct drm_dp_mst_branch *mstb_child = NULL;

		if (port->input || !port->ddps)
			continue;

		DPTXDBG("To send remote DPCD read, lct %u, port %p, port_num %d\n",
					mstb->lct, port, port->port_num);
		ret_tmp = drm_dp_dpcd_read(&port->aux, DPCD_00090, &bytes_tmp, 0x1);
		if (ret_tmp < 0) {
			DPTXERR("Remote DPCD read fail %d, lct %d, port_num %d\n",
					ret_tmp, mstb->lct, port->port_num);
			return ret_tmp;
		}

		bytes_tmp &= BIT(0);
		*p_bytes &= bytes_tmp;

		DPTXDBG("FEC supported %d, lct %d, port %p, port_num %d\n", bytes_tmp, mstb->lct,
				port, port->port_num);
		if (*p_bytes == BIT(0)) {
#if (DPTX_OS == DPTX_CTP)
			if (port->mstb)
				mstb_child = drm_dp_mst_topology_get_mstb_validated(mgr,
						port->mstb);
#endif
			if (mstb_child) {
				ret_tmp = mtk_dptx_mst_drv_send_remote_dpcd_read_fec_en(mgr,
						mstb_child, p_bytes);
				if (ret_tmp < 0)
					ret = ret_tmp;
#if (DPTX_OS == DPTX_CTP)
				drm_dp_mst_topology_put_mstb(mstb_child);
#endif
			}
		} else {
			DPTXMSG("FEC un-supported, lct %d, port %p, port_num %d\n",
					mstb->lct, port, port->port_num);
		}
	}

	return ret;
}

/**
 * mtk_dptx_mst_drv_payload_handler() - start to output MST streams
 */
void mtk_dptx_mst_drv_payload_handler(struct mtk_dp *mtk_dp)
{
	if (mtk_dp->mst_enable == FALSE) {
		DPTXERR("connected device does not support MST, return !\n");
		return;
	}

	if (mtk_dp->is_mst_start == FALSE) {
		DPTXERR("unexpected code flow\n");
		return;
	}

	DPTXMSG("MST PayloadHandler start\n");

	/* allocate vcpi */
	mtk_dptx_mst_drv_allocate_stream(mtk_dp, TRUE);

	/* update payload, after update vcpi */
	mtk_dptx_mst_drv_update_payload(mtk_dp);
}

static void mtk_dptx_mst_drv_fec_handler(struct mtk_dp *mtk_dp)
{
	struct drm_dp_mst_topology_mgr *mgr = &mtk_dp->mgr;
	struct drm_dp_mst_branch *mstb = mgr->mst_primary;
	u16 dpcd_fec_support = 0x0;

	mtk_dptx_mst_drv_send_remote_dpcd_read_fec_en(
			mgr, mstb, &dpcd_fec_support);

	mtk_dp->is_mst_fec_en = dpcd_fec_support && mtk_dp->has_fec;
	DPTXMSG("Support FEC %d\n", mtk_dp->is_mst_fec_en);

	// should call mhal_DPTx_FECInitialSetting() before this function
	mtk_dptx_mst_drv_fec_enable(mtk_dp, mtk_dp->is_mst_fec_en); //DPCD_00090 bit0
}

#if (DPTX_OS == DPTX_LINUX)
static void mtk_mst_connector_destroy(struct drm_connector *connector)
{
	struct mtk_dp *mtk_dp;
	struct mtk_dp_mst_connector *mst_connector;
	int i;

	DPTXFUNC();

	mst_connector = container_of(connector, struct mtk_dp_mst_connector, connector);
	mtk_dp = mst_connector->mtk_dp;

	drm_connector_cleanup(&mst_connector->connector);
	drm_dp_mst_put_port_malloc(mst_connector->port);

	for (i = 0; i < DPTX_ENCODER_NUM; i++) {
		if (mtk_dp->mst_connector[i] == mst_connector) {
			mtk_dp->mst_connector[i] = NULL;
			DPTXMSG("[MST] destroy connector from dp:%p\n", mst_connector);
		}
	}

	kfree(mst_connector);
}

static const struct drm_connector_funcs mtk_mst_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = mtk_mst_connector_destroy,
};

static enum drm_mode_status mtk_mst_connector_mode_valid(struct drm_connector *connector,
		     struct drm_display_mode *mode)
{
	/* TODO: calculate the PBN from the dotclock and validate against the
	 * MSTB's max possible PBN
	 */
	DPTXFUNC();

	return MODE_OK;
}

static int mtk_mst_connector_get_modes(struct drm_connector *connector)
{
	struct mtk_dp_mst_connector *mst_connector;
	int ret = 0;

	DPTXFUNC();

	mst_connector = container_of(connector, struct mtk_dp_mst_connector, connector);

	mst_connector->edid = drm_dp_mst_get_edid(&mst_connector->connector,
		mst_connector->port->mgr, mst_connector->port);
	drm_connector_update_edid_property(&mst_connector->connector, mst_connector->edid);
	if (mst_connector->edid)
		ret = drm_add_edid_modes(&mst_connector->connector, mst_connector->edid);

	/*
	 * XXX: Since we don't use HDR in userspace quite yet, limit the bpc
	 * to 8 to save bandwidth on the topology. In the future, we'll want
	 * to properly fix this by dynamically selecting the highest possible
	 * bpc that would fit in the topology
	 */
	if (connector->display_info.bpc)
		connector->display_info.bpc =
			clamp(connector->display_info.bpc, 6U, 8U);
	else
		connector->display_info.bpc = 8;

	if (mst_connector->native)
		drm_mode_destroy(mst_connector->connector.dev, mst_connector->native);

	return ret;
}

static int mtk_mst_connector_atomic_check(struct drm_connector *connector,
		       struct drm_atomic_state *state)
{
	struct mtk_dp_mst_connector *mst_connector;
	struct drm_dp_mst_topology_mgr *mgr;

	DPTXFUNC();

	mst_connector = container_of(connector, struct mtk_dp_mst_connector, connector);
	mgr = &mst_connector->mtk_dp->mgr;

	return drm_dp_atomic_release_time_slots(state, mgr, mst_connector->port);
}

static const struct drm_connector_helper_funcs mtk_mst_connector_helper_funcs = {
	.get_modes = mtk_mst_connector_get_modes,
	.mode_valid = mtk_mst_connector_mode_valid,
	.atomic_check = mtk_mst_connector_atomic_check,
};

static struct drm_connector *mtk_mst_add_connector(struct drm_dp_mst_topology_mgr *mgr,
	struct drm_dp_mst_port *port, const char *path)
{
	struct mtk_dp *mtk_dp;
	struct mtk_dp_mst_connector *mst_connector;
	int ret;
	int i;

	DPTXFUNC();

	mtk_dp = container_of(mgr, struct mtk_dp, mgr);

	mst_connector = kzalloc(sizeof(*mst_connector), GFP_KERNEL);
	if (!mst_connector) {
		DPTXERR("[MST] fail to kzalloc!\n");
		return NULL;
	}

	mst_connector->mtk_dp = mtk_dp;
	mst_connector->port = port;

	ret = drm_connector_init(mtk_dp->drm_dev, &mst_connector->connector, &mtk_mst_connector_funcs,
		DRM_MODE_CONNECTOR_DisplayPort);
	if (ret) {
		kfree(mst_connector);
		return NULL;
	}

	for (i = 0; i < DPTX_ENCODER_NUM; i++) {
		if (mtk_dp->mst_connector[i] == NULL) {
			mtk_dp->mst_connector[i] = mst_connector;
			DPTXMSG("[MST] add connector to dp:%p\n", mst_connector);
		}
	}

	drm_connector_helper_add(&mst_connector->connector, &mtk_mst_connector_helper_funcs);

	drm_object_attach_property(&mst_connector->connector.base, mtk_dp->drm_dev->mode_config.path_property, 0);
	drm_object_attach_property(&mst_connector->connector.base, mtk_dp->drm_dev->mode_config.tile_property, 0);
	drm_connector_set_path_property(&mst_connector->connector, path);
	drm_dp_mst_get_port_malloc(port);

	return &mst_connector->connector;
}

static const struct drm_dp_mst_topology_cbs mtk_mst_topology_cbs = {
	.add_connector = mtk_mst_add_connector,
};
#endif

/**
 * mtk_dptx_mst_drv_init() - initialize DRM mgr structure, and DPTx HW & SW
 */
void mtk_dptx_mst_drv_init(struct mtk_dp *mtk_dp)
{
	/* executed when we know the primary Branch supports MST */
	signed short ret = 0;
	signed short max_payloads = DPTX_STREAM_MAX;
	signed short conn_base_id = 0;

	DPTXFUNC();

#if (DPTX_OS == DPTX_LINUX)
	//if (mtk_dp->mgr == NULL)
	//	mtk_dp->mgr = kzalloc(sizeof(struct drm_dp_mst_topology_mgr), GFP_KERNEL); //macshen no pointer
#else
	//mtk_dp->mgr = &mtk_topo_mgr;
#endif

	/* dptx global variable init */
	mtk_dptx_mst_drv_init_variable(mtk_dp);

#if (DPTX_OS == DPTX_LINUX)
	mtk_dp->mgr.cbs = &mtk_mst_topology_cbs;
#endif

	/* init topology mgr */
#if (DPTX_OS == DPTX_LINUX)
	ret = drm_dp_mst_topology_mgr_init(&mtk_dp->mgr, mtk_dp->drm_dev, mtk_dp->mgr.aux,
				DPTX_DPCD_TRANS_BYTES_MAX, max_payloads, conn_base_id);
#else
	ret = drm_dp_mst_topology_mgr_init(&mtk_dp->mgr, DPTX_DPCD_TRANS_BYTES_MAX, max_payloads,
				DPTX_SUPPORT_MAX_LANECOUNT, DPTX_SUPPORT_MAX_LINKRATE * 27000,
				conn_base_id);
#endif

	if (ret < 0)
		DPTXERR("Topology mgr init fail\n");

	mtk_dptx_mst_hal_tx_init(mtk_dp);
}

/**
 * mtk_dptx_mst_drv_start() - start to create MST topology
 *
 * Search all device in the MST topology and power-up phy,
 * then remoted read EDIDs and start to output MST video.
 */
void mtk_dptx_mst_drv_start(struct mtk_dp *mtk_dp)
{
	/* executed when the link training with the primary Branch has been done */
	u8 dpcd[DP_RECEIVER_CAP_SIZE];

	DPTXMSG("MST start\n");

	/* check capability */
	dpcd[0] = mtk_dp->training_info.ubDPSysVersion;
#if (DPTX_OS == DPTX_CTP)
	if (drm_dp_read_mst_cap(dpcd) == FALSE) {
		DPTXMSG("connected device does not support MST\n");
		return;
	}
#else
	if (drm_dp_read_mst_cap(&mtk_dp->aux, dpcd) == FALSE) {
		DPTXMSG("connected device does not support MST\n");
		return;
	}
#endif

	mtk_dp->is_mst_start = TRUE;

	DPTXMSG("[MST] set mst and discover the topology\n");
	/*set mst and discover the topology*/
	drm_dp_mst_topology_mgr_set_mst(&mtk_dp->mgr, TRUE);

	mtk_dptx_mst_hal_tx_enable(mtk_dp, TRUE);
	mtk_dptx_mst_hal_mst_config(mtk_dp);
}

/**
 * mtk_dptx_mst_drv_stop() - source is going to stop mst
 */
void mtk_dptx_mst_drv_stop(struct mtk_dp *mtk_dp)
{
	//if (mtk_dp->is_mst_start == FALSE) {
		//DPTX_ERR("Unexpected flow, call MST Stop w/o MST Start\n");
		//return;
	//}
	//else {
		mtk_dp->is_mst_start = FALSE;
		DPTXMSG("MST stop\n");
	//}

	mtk_dptx_mst_drv_fec_enable(mtk_dp, FALSE);

	/* de-allocate vcpi */
	mtk_dptx_mst_drv_clear_vcpi(mtk_dp);
	mtk_dptx_mst_drv_stream_enable(mtk_dp);

	/* disable MST output (transmitter/MST TX) */
	mtk_dptx_mst_hal_tx_enable(mtk_dp, FALSE);

	/* destroy topology mgr */
	drm_dp_mst_topology_mgr_destroy(&mtk_dp->mgr);

	mtk_dptx_mst_hal_tx_init(mtk_dp);

	/* DPTx global variable reset */
	mtk_dptx_mst_drv_init_variable(mtk_dp);

	mtk_dp->mst_enable = FALSE;
}

/**
 * mtk_dptx_mst_drv_reset() - reset MST stream for devices in the topology
 * @is_plug: result in the current plug status
 */
void mtk_dptx_mst_drv_reset(struct mtk_dp *mtk_dp, u8 is_plug)
{
	u8 dpcd[DP_RECEIVER_CAP_SIZE];

	DPTXMSG("Reset MST, is_plug %d\r\n", is_plug);
	mtk_dptx_drv_video_mute_all(mtk_dp);
	mtk_dptx_drv_audio_mute_all(mtk_dp);

	dpcd[0] = mtk_dp->training_info.ubDPSysVersion;
#if (DPTX_OS == DPTX_CTP)
	if (drm_dp_read_mst_cap(dpcd) == FALSE) {
#else
	if (drm_dp_read_mst_cap(&mtk_dp->aux, dpcd) == FALSE) {
#endif
		DPTXMSG("connected device not support MST\n");
		mtk_dp->mst_enable = FALSE;
	}

	//if (is_plug) {
		mtk_dptx_mst_drv_stop(mtk_dp);
		mtk_dptx_drv_video_mute_all(mtk_dp);
		mtk_dptx_drv_audio_mute_all(mtk_dp);
		mtk_dp->state = DPTXSTATE_INITIAL;
		mtk_dp->mst_enable = TRUE;
	//} else {
		//mtk_dptx_mst_drv_update_payload(mtk_dp);
		//mtk_dptx_mst_drv_stream_enable(mtk_dp);
	//}
}

/**
 * mtk_dptx_mst_drv_handler() - handle MST finite state machine
 *
 * It's a state machine with 6 states.
 * 1st state "DPTXSTATE_INITIAL": wait and check primary branch support MST
 * 2nd state "DPTXSTATE_STARTUP": wait for source training done
 * 3rd state "DPTXSTATE_AUTH": handle hdcp auth. flow if support hdcp
 * 4th state "DPTXSTATE_PREPARE": prepare before output valid timing
 * 5th state "DPTXSTATE_NORMAL": already output MST video and here if work normally
 * 6th state "DPTXSTATE_IDLE": wait here until link status change
 */
u8 mtk_dptx_mst_drv_handler(struct mtk_dp *mtk_dp)
{
	u8 ret = DPTX_NOERR;

	if (mtk_dp->dp_ready == FALSE)
		return DPTX_PLUG_OUT;

	if (!mtk_dp->training_info.bCablePlugIn)
		return DPTX_PLUG_OUT;

	if (mtk_dp->mst_enable == FALSE)
		return DPTX_NOERR;

	/* update MST state machine and print log */
	if (mtk_dp->state != mtk_dp->state_pre)
		mtk_dptx_mst_drv_print_state(mtk_dp);

	mtk_dp->state_pre = mtk_dp->state;

	if ((mtk_dp->state > DPTXSTATE_IDLE) &&
		(mtk_dp->training_state != DPTX_NTSTATE_NORMAL)) {
		DPTXERR("lose lock!!!traininig state %x\n\n", mtk_dp->training_state);
		mtk_dptx_mst_drv_stop(mtk_dp);
		mtk_dptx_drv_video_mute_all(mtk_dp);
		mtk_dptx_drv_audio_mute_all(mtk_dp);
		mtk_dp->state = DPTXSTATE_INITIAL;
		mtk_dp->mst_enable = true;
	}

	switch (mtk_dp->state) {
	case DPTXSTATE_INITIAL:  // wait primary branch support Mst
		if (mtk_dp->mst_enable == TRUE) {
			DPTXMSG("Mst Enable!!!\n\n");
			mtk_dptx_drv_video_mute_all(mtk_dp);
			mtk_dptx_drv_audio_mute_all(mtk_dp);
		#if DPTX_MST_HDCP_ENABLE
			mtk_dptx_hdcp13_enable_encrypt(FALSE);
			mtk_dptx_hdcp23_enable_encrypt(FALSE);
		#endif

			mtk_dptx_mst_drv_init(mtk_dp);
			mtk_dp->state = DPTXSTATE_IDLE;
		}
		break;

	case DPTXSTATE_IDLE: //wait DP Tx training done
		if (mtk_dp->training_state == DPTX_NTSTATE_NORMAL) {
			mtk_dptx_mst_drv_start(mtk_dp);
			mtk_dp->state = DPTXSTATE_PREPARE;
		}
		break;

	case DPTXSTATE_PREPARE:

	#if DPTX_MST_FEC_ENABLE
		mtk_dptx_mst_drv_fec_handler(mtk_dp);
	#endif
		mtk_dptx_mst_drv_payload_handler(mtk_dp);
		mtk_dptx_hal_encoder_reset_all(mtk_dp);
	#if DPTX_MST_HDCP_ENABLE
		mtk_dp->state = DPTXSTATE_AUTH;

		if (mtk_dp->hdcp_handle.enable) {
			DPTXMSG("Start Auth HDCP22!\n");
			mtk_dptx_hdcp23_set_start_auth(TRUE);
		} else if (mtk_dp->hdcp13_info.enable) {
			DPTXMSG("[DPTX] HDCP13 auth start!\n");
			mtk_dp->hdcp13_info.main_state = HDCP13_MAIN_STATE_A0;
			mtk_dp->hdcp13_info.sub_state = HDCP13_SUB_STATE_IDLE;
			mtk_dp->hdcp13_info.retry_count = 0;
		}
	#else
		mtk_dp->state = DPTXSTATE_NORMAL;
	#endif
		break;

#if DPTX_MST_HDCP_ENABLE
	case DPTXSTATE_AUTH:

		if (mtk_dp->hdcp13_info.enable) {
			if (mtk_dp->auth_status == AUTH_PASS) {
				DPTXMSG("HDCP 13 auth passed!\n");
				mtk_dp->state = DPTXSTATE_NORMAL;
			} else if (mtk_dp->auth_status == AUTH_FAIL) {
				mtk_dp->state = DPTXSTATE_IDLE;
				DPTXERR("HDCP 13 auth failed!\n");
				mtk_dptx_hdcp13_enable_encrypt(FALSE);
			}
			mtk_dptx_hdcp13_fsm();
		} else if (mtk_dp->hdcp_handle.enable) {
			if (mtk_dp->auth_status == AUTH_PASS) {
				DPTXMSG("HDCP 23 auth passed!\n");
				mtk_dp->state = DPTXSTATE_NORMAL;
			} else if (mtk_dp->auth_status == AUTH_FAIL) {
				DPTXERR("HDCP 23 auth failed!\n");
				mtk_dp->state = DPTXSTATE_IDLE;
				mtk_dptx_hdcp23_enable_encrypt(FALSE);
			}
			mtk_dptx_hdcp23_fsm();
		} else {
			DPTXMSG("Sink Not support HDCP, skip auth!\n");
			mtk_dp->state = DPTXSTATE_NORMAL;
		}

		break;
#endif

	case DPTXSTATE_NORMAL:
		#if DPTX_MST_HDCP_ENABLE
		if (mtk_dp->hdcp13_info.enable)
			mtk_dptx_hdcp13_fsm();
		else if (mtk_dp->hdcp_handle.enable)
			mtk_dptx_hdcp23_fsm();
		#endif

		if (mtk_dp->training_state != DPTX_NTSTATE_NORMAL) {
			DPTXMSG("[MST] DPTX Link Status Change!%d\r\n", mtk_dp->training_state);
			mtk_dptx_mst_drv_stop(mtk_dp);
			mtk_dptx_drv_video_mute_all(mtk_dp);
			mtk_dptx_drv_audio_mute_all(mtk_dp);
			mtk_dp->state = DPTXSTATE_INITIAL;
			mtk_dp->mst_enable = true;
		#if DPTX_MST_HDCP_ENABLE
			mtk_dptx_hdcp13_enable_encrypt(FALSE);
			mtk_dptx_hdcp23_enable_encrypt(FALSE);
		#endif
		}
		break;
	default:
		DPTXERR("[MST] Current State invalid, please check !!!\n");
		break;
	}

	return ret;
}

void mtk_dptx_mst_drv_sideband_msg_irq_clear(struct mtk_dp *mtk_dp)
{
	mtk_dp->training_info.phy_status &= (~DPTX_PHY_HPD_INT_EVNET);
}

void mtk_dptx_mst_drv_sideband_msg_rdy_clear(struct mtk_dp *mtk_dp)
{
	u8 dpcd_buf[2], temp[1];

	/*Clear RDY bit after got the sideband MSG*/
	if (mtk_dp->training_info.bSinkEXTCAP_En) {
		drm_dp_dpcd_read(&mtk_dp->aux, DPCD_02002, dpcd_buf, 0x2);
		if (dpcd_buf[0x1] & 0x30) {
			temp[0] = (dpcd_buf[0x1] & 0x30);
			drm_dp_dpcd_write(&mtk_dp->aux, DPCD_02003, temp, 0x1);
		}
	} else {
		drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00200, dpcd_buf, 0x2);
		if (dpcd_buf[0x1] & 0x30) {
			temp[0] = (dpcd_buf[0x1] & 0x30);
			drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00201, temp, 0x1);
		}
	}
}
