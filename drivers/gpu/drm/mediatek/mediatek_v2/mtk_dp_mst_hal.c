// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include "mtk_dp_mst_hal.h"
#include "mtk_dp_mst_drv.h"
#include "mtk_dp.h"
#include "mtk_dp_hal.h"
#include "mtk_dp_reg.h"
#include "mtk_dp_debug.h"

#if MST_HAL
void mtk_dptx_hal_verify_clock(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u32 link_rate)
{
#if ENABLE_DPTX_MST_DEBUG
	u32 m, n, Ls_clk, pix_clk;
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

	m = msRead4Byte(mtk_dp, REG_33C8_DP_ENCODER1_P0 + reg_offset_enc);
	n = 0x8000;
	Ls_clk = link_rate * 27;

	pix_clk = m * Ls_clk / n;
	DPTXMSG("Encoder %d, DPTX calc pixel clock = %d MHz, dp_intf clock = %dMHz\n",
		 encoder_id, pix_clk, pix_clk / 4);
#endif
}

void mtk_dptx_hal_set_video_interlance(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 enable)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

	if (enable) {
		msWriteByteMask(mtk_dp, (REG_3030_DP_ENCODER0_P0 + 1 + reg_offset_enc),
				BIT(6) | BIT(5), BIT(6) | BIT(5));
		//msWriteByteMask(mtk_dp, (REG_3368_DP_ENCODER1_P0 + 1), 0, BIT(5) | BIT(4));
		DPTXMSG("Encoder %d, DPTX imode force-ov\n", encoder_id);
	} else {
		msWriteByteMask(mtk_dp, (REG_3030_DP_ENCODER0_P0 + 1 + reg_offset_enc),
				BIT(6), BIT(6) | BIT(5));
		//msWriteByteMask(mtk_dp, (REG_3368_DP_ENCODER1_P0 + 1), BIT(4), BIT(5) | BIT(4));
		DPTXMSG("Encoder %d, DPTX pmode force-ov\n", encoder_id);
	}
}

void mtk_dptx_hal_bypass_msa_enable(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 enable)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

	if (enable)
		msWrite2ByteMask(mtk_dp, REG_3030_DP_ENCODER0_P0 + reg_offset_enc, 0, 0x03FF);
	else
		msWrite2ByteMask(mtk_dp, REG_3030_DP_ENCODER0_P0 + reg_offset_enc, 0x03FF, 0x03FF);
}

void mtk_dptx_hal_set_msa(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
	struct DPTX_TIMING_PARAMETER *timing)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

	DPTXFUNC();

	msWrite2Byte(mtk_dp, REG_3010_DP_ENCODER0_P0 + reg_offset_enc,
					(timing->Htt));
	msWrite2Byte(mtk_dp, REG_3018_DP_ENCODER0_P0 + reg_offset_enc,
					(timing->Hsw + timing->Hbp));
	msWrite2ByteMask(mtk_dp, REG_3028_DP_ENCODER0_P0 + reg_offset_enc,
					((timing->Hsw) << HSW_SW_DP_ENCODER0_P0_FLDMASK_POS),
					 HSW_SW_DP_ENCODER0_P0_FLDMASK);
	msWrite2ByteMask(mtk_dp, REG_3028_DP_ENCODER0_P0 + reg_offset_enc,
					 ((timing->bHsp) << HSP_SW_DP_ENCODER0_P0_FLDMASK_POS),
					 HSP_SW_DP_ENCODER0_P0_FLDMASK);
	msWrite2Byte(mtk_dp, REG_3020_DP_ENCODER0_P0 + reg_offset_enc,
					(timing->Hde));
	msWrite2Byte(mtk_dp, REG_3014_DP_ENCODER0_P0 + reg_offset_enc,
					(timing->Vtt));
	msWrite2Byte(mtk_dp, REG_301C_DP_ENCODER0_P0 + reg_offset_enc,
					timing->Vsw + timing->Vbp);
	msWrite2ByteMask(mtk_dp, REG_302C_DP_ENCODER0_P0 + reg_offset_enc,
					((timing->Vsw) << VSW_SW_DP_ENCODER0_P0_FLDMASK_POS),
					VSW_SW_DP_ENCODER0_P0_FLDMASK);
	msWrite2ByteMask(mtk_dp, REG_302C_DP_ENCODER0_P0 + reg_offset_enc,
					((timing->bVsp) << VSP_SW_DP_ENCODER0_P0_FLDMASK_POS),
					VSP_SW_DP_ENCODER0_P0_FLDMASK);
	msWrite2Byte(mtk_dp, REG_3024_DP_ENCODER0_P0 + reg_offset_enc,
					(timing->Vde));
	msWrite2Byte(mtk_dp, REG_3064_DP_ENCODER0_P0 + reg_offset_enc,
					(timing->Hde));

	msWrite2Byte(mtk_dp, REG_3154_DP_ENCODER0_P0 + reg_offset_enc,
					(timing->Htt));
	msWrite2Byte(mtk_dp, REG_3158_DP_ENCODER0_P0 + reg_offset_enc,
					(timing->Hfp));
	msWrite2Byte(mtk_dp, REG_315C_DP_ENCODER0_P0 + reg_offset_enc,
					(timing->Hsw));
	msWrite2Byte(mtk_dp, REG_3160_DP_ENCODER0_P0 + reg_offset_enc,
					(timing->Hbp) + (timing->Hsw));
	msWrite2Byte(mtk_dp, REG_3164_DP_ENCODER0_P0 + reg_offset_enc,
					(timing->Hde));

	msWrite2Byte(mtk_dp, REG_3168_DP_ENCODER0_P0 + reg_offset_enc,
					(timing->Vtt));
	msWrite2Byte(mtk_dp, REG_316C_DP_ENCODER0_P0 + reg_offset_enc,
					(timing->Vfp));
	msWrite2Byte(mtk_dp, REG_3170_DP_ENCODER0_P0 + reg_offset_enc,
					(timing->Vsw));
	msWrite2Byte(mtk_dp, REG_3174_DP_ENCODER0_P0 + reg_offset_enc,
					(timing->Vbp) + (timing->Vsw));
	msWrite2Byte(mtk_dp, REG_3178_DP_ENCODER0_P0 + reg_offset_enc,
					(timing->Vde));


	DPTXMSG("Encoder %d, MSA : Htt=%d Vtt=%d  Hact=%d  Vact=%d\n", reg_offset_enc,
		 timing->Htt, timing->Vtt,
		 timing->Hde, timing->Vde);
}

void mtk_dptx_hal_mvid_renew(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);
	u32 mvid, htt;

	htt = mtk_dp->info[encoder_id].DPTX_OUTBL.Htt;
	if (htt % 4 != 0) {
		mvid = msRead4Byte(mtk_dp, REG_33C8_DP_ENCODER1_P0 + reg_offset_enc);
		DPTXERR("\033[1;33mEncoder %d, Odd Htt %d Mvid %d overwrite !\033[m\n",
								encoder_id, htt, mvid);
		if (mtk_dp->info[encoder_id].input_src == DPTX_SRC_PG)
			mvid = mvid * htt / (htt - 2);
		mtk_dptx_hal_mn_overwrite(mtk_dp, encoder_id, TRUE, mvid, 0x8000);
	}
}

u8 mtk_dptx_hal_mn_overwrite(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 enable,
				u32 video_m,
				u32 video_n)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

	if (enable) {
		//Turn-on overwrite MN
		msWrite2Byte(mtk_dp, REG_3008_DP_ENCODER0_P0 + reg_offset_enc,
					video_m & 0xFFFF);
		msWriteByte(mtk_dp, REG_300C_DP_ENCODER0_P0 + reg_offset_enc,
					((video_m >> 16) & 0xFF));
		msWrite2Byte(mtk_dp, REG_3044_DP_ENCODER0_P0 + reg_offset_enc,
					video_n & 0xFFFF);
		msWriteByte(mtk_dp, REG_3048_DP_ENCODER0_P0 + reg_offset_enc,
					(video_n >> 16) & 0xFF);
		msWrite2Byte(mtk_dp, REG_3050_DP_ENCODER0_P0 + reg_offset_enc,
					video_n & 0xFFFF);
		msWriteByte(mtk_dp, REG_3054_DP_ENCODER0_P0 + reg_offset_enc,
					(video_n >> 16) & 0xFF);
		msWriteByteMask(mtk_dp, (REG_3004_DP_ENCODER0_P0 + 1 + reg_offset_enc),
					BIT(0), BIT(0));
	} else {
		//Turn-off overwrite MN
		msWriteByteMask(mtk_dp, (REG_3004_DP_ENCODER0_P0 + 1 + reg_offset_enc), 0, BIT(0));
	}

	return TRUE;
}

void mtk_dptx_hal_set_color_format(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 color_format)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

	msWriteByteMask(mtk_dp, REG_3034_DP_ENCODER0_P0 + reg_offset_enc,
						(color_format << 0x1), MASKBIT(2 : 1));

	if ((color_format == DP_COLOR_FORMAT_RGB_444)
	    || (color_format == DP_COLOR_FORMAT_YUV_444))
		msWriteByteMask(mtk_dp, (REG_303C_DP_ENCODER0_P0 + 1 + reg_offset_enc),
						(0), MASKBIT(6 : 4));

	else if (color_format == DP_COLOR_FORMAT_YUV_422)
		msWriteByteMask(mtk_dp, (REG_303C_DP_ENCODER0_P0 + 1 + reg_offset_enc),
						(BIT(4)), MASKBIT(6 : 4));

	else if (color_format == DP_COLOR_FORMAT_YUV_420)
		msWriteByteMask(mtk_dp, (REG_303C_DP_ENCODER0_P0 + 1 + reg_offset_enc),
						(BIT(5)), MASKBIT(6 : 4));
}

void mtk_dptx_hal_set_color_depth(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 color_depth)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

	msWriteByteMask(mtk_dp, REG_3034_DP_ENCODER0_P0 + reg_offset_enc,
						(color_depth << 0x5), 0xE0);

	switch (color_depth) {
	case DP_COLOR_DEPTH_6BIT:
		msWriteByteMask(mtk_dp, (REG_303C_DP_ENCODER0_P0 + 1 + reg_offset_enc), 4, 0x07);
		break;

	case DP_COLOR_DEPTH_8BIT:
		msWriteByteMask(mtk_dp, (REG_303C_DP_ENCODER0_P0 + 1 + reg_offset_enc), 3, 0x07);
		break;

	case DP_COLOR_DEPTH_10BIT:
		msWriteByteMask(mtk_dp, (REG_303C_DP_ENCODER0_P0 + 1 + reg_offset_enc), 2, 0x07);
		break;

	case DP_COLOR_DEPTH_12BIT:
		msWriteByteMask(mtk_dp, (REG_303C_DP_ENCODER0_P0 + 1 + reg_offset_enc), 1, 0x07);
		break;

	case DP_COLOR_DEPTH_16BIT:
		msWriteByteMask(mtk_dp, (REG_303C_DP_ENCODER0_P0 + 1 + reg_offset_enc), 0, 0x07);
		break;

	default:
		break;
	}
}

void mtk_dptx_hal_set_misc(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8  misc[2])
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

	msWriteByteMask(mtk_dp, REG_3034_DP_ENCODER0_P0 + reg_offset_enc, misc[0], 0xFE);
	msWriteByteMask(mtk_dp, (REG_3034_DP_ENCODER0_P0 + 1 + reg_offset_enc), misc[1], 0xFF);
}

void mtk_dptx_hal_set_mvidx2(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8  enable)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

	if (enable)
		msWriteByteMask(mtk_dp, (REG_300C_DP_ENCODER0_P0 + 1 + reg_offset_enc),
							BIT(4), BIT(6) | BIT(5) | BIT(4));
	else
		msWriteByteMask(mtk_dp, (REG_300C_DP_ENCODER0_P0 + 1 + reg_offset_enc), 0,
							BIT(6) | BIT(5) | BIT(4));
}

u8 mtk_dptx_hal_get_color_info(u8  color_depth, u8  color_format) // macshen refine
{
	u8 ColorBpp;

	switch (color_depth) {
	case DP_COLOR_DEPTH_6BIT:
		if (color_format == DP_COLOR_FORMAT_YUV_422)
			ColorBpp = 16;
		else if (color_format == DP_COLOR_FORMAT_YUV_420)
			ColorBpp = 12;
		else
			ColorBpp = 18;

		break;

	case DP_COLOR_DEPTH_8BIT:
		if (color_format == DP_COLOR_FORMAT_YUV_422)
			ColorBpp = 16;
		else if (color_format == DP_COLOR_FORMAT_YUV_420)
			ColorBpp = 12;
		else
			ColorBpp = 24;

		break;

	case DP_COLOR_DEPTH_10BIT:
		if (color_format == DP_COLOR_FORMAT_YUV_422)
			ColorBpp = 20;
		else if (color_format == DP_COLOR_FORMAT_YUV_420)
			ColorBpp = 15;
		else
			ColorBpp = 30;

		break;

	case DP_COLOR_DEPTH_12BIT:
		if (color_format == DP_COLOR_FORMAT_YUV_422)
			ColorBpp = 24;
		else if (color_format == DP_COLOR_FORMAT_YUV_420)
			ColorBpp = 18;
		else
			ColorBpp = 36;

		break;

	case DP_COLOR_DEPTH_16BIT:
		if (color_format == DP_COLOR_FORMAT_YUV_422)
			ColorBpp = 32;
		else if (color_format == DP_COLOR_FORMAT_YUV_420)
			ColorBpp = 24;
		else
			ColorBpp = 48;

		break;

	default:
		ColorBpp = 24;
		DPTXERR("Set Wrong Bpp = %d\n", ColorBpp);
		break;
	}

	return ColorBpp;
}

void mtk_dptx_hal_set_tu_sram_read_start(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u16 val)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);
	//[5:0]video sram start address
	msWriteByteMask(mtk_dp, REG_303C_DP_ENCODER0_P0 + reg_offset_enc, val, 0x3F);
}

void mtk_dptx_hal_set_sdp_count_hblank(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u16 val)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);
	//[11 : 0]REG_sdp_down_cnt_init_in_hblank
	msWrite2ByteMask(mtk_dp, REG_3364_DP_ENCODER1_P0 + reg_offset_enc, val, 0x0FFF);
}

void mtk_dptx_hal_set_sdp_count_init(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u16 val)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);
	//[11 : 0]REG_sdp_down_cnt_init
	msWrite2ByteMask(mtk_dp, REG_3040_DP_ENCODER0_P0 + reg_offset_enc, val, 0x0FFF);
}

void mtk_dptx_hal_set_sdp_asp_count_init(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, const u16 val)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);
	//[11 : 0] reg_sdp_down_asp_cnt_init
	msWrite2ByteMask(mtk_dp, REG_3374_DP_ENCODER1_P0 + reg_offset_enc,
		val << SDP_DOWN_ASP_CNT_INIT_DP_ENCODER1_P0_FLDMASK_POS,
		SDP_DOWN_ASP_CNT_INIT_DP_ENCODER1_P0_FLDMASK);
}

void mtk_dptx_hal_set_tu_encoder(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

	msWriteByteMask(mtk_dp, (REG_303C_DP_ENCODER0_P0 + 1 + reg_offset_enc), BIT(7), BIT(7));
	msWrite2Byte(mtk_dp, REG_3040_DP_ENCODER0_P0 + reg_offset_enc, 0x2020);
	msWrite2ByteMask(mtk_dp, REG_3364_DP_ENCODER1_P0 + reg_offset_enc, 0x2020, 0x0FFF);
	msWriteByteMask(mtk_dp, (REG_3300_DP_ENCODER1_P0 + 1 + reg_offset_enc), 0x02, BIT(1) | BIT(0));
	msWriteByteMask(mtk_dp, (REG_3364_DP_ENCODER1_P0 + 1 + reg_offset_enc), 0x40, 0x70);
}

void mtk_dptx_hal_pg_enable(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 enable)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

	if (enable)
		msWriteByteMask(mtk_dp, (REG_3038_DP_ENCODER0_P0 + 1 + reg_offset_enc), BIT(3), BIT(3));
	else
		msWriteByteMask(mtk_dp, (REG_3038_DP_ENCODER0_P0 + 1 + reg_offset_enc), 0, BIT(3));
}

void mtk_dptx_hal_pg_pure_color(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8  rgb,
				u32 color_depth)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

	// [11] video select hw or pattern gen 0:HW 1:PG
	msWriteByteMask(mtk_dp, (REG_3038_DP_ENCODER0_P0 + 1 + reg_offset_enc), BIT(3), BIT(3));
	// reg_pattern_sel
	msWriteByteMask(mtk_dp, REG_31B0_DP_ENCODER0_P0 + reg_offset_enc, 0, MASKBIT(6 : 4));

	switch (rgb) {
	case DPTX_PG_PURECOLOR_BLUE:
		msWrite2ByteMask(mtk_dp, REG_317C_DP_ENCODER0_P0 + reg_offset_enc,
					0, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_3180_DP_ENCODER0_P0 + reg_offset_enc,
					0, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_3184_DP_ENCODER0_P0 + reg_offset_enc,
					color_depth, MASKBIT(11 : 0));
		break;

	case DPTX_PG_PURECOLOR_GREEN:
		msWrite2ByteMask(mtk_dp, REG_317C_DP_ENCODER0_P0 + reg_offset_enc,
				0, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_3180_DP_ENCODER0_P0 + reg_offset_enc,
				color_depth, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_3184_DP_ENCODER0_P0 + reg_offset_enc,
				0, MASKBIT(11 : 0));
		break;

	case DPTX_PG_PURECOLOR_RED:
		msWrite2ByteMask(mtk_dp, REG_317C_DP_ENCODER0_P0 + reg_offset_enc,
				color_depth, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_3180_DP_ENCODER0_P0 + reg_offset_enc,
				0, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_3184_DP_ENCODER0_P0 + reg_offset_enc,
				0, MASKBIT(11 : 0));
		break;

	default: //Red
		msWrite2ByteMask(mtk_dp, REG_317C_DP_ENCODER0_P0 + reg_offset_enc,
				color_depth, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_3180_DP_ENCODER0_P0 + reg_offset_enc,
				0, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_3184_DP_ENCODER0_P0 + reg_offset_enc,
				0, MASKBIT(11 : 0));
		break;
	}
}

void mtk_dptx_hal_pg_v_ramping(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
			       u8  rgb,
			       u32 color_depth,
			       u8  location)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

	// [11] video select hw or pattern gen 0:HW 1:PG
	msWriteByteMask(mtk_dp, (REG_3038_DP_ENCODER0_P0 + 1 + reg_offset_enc), BIT(3), BIT(3));
	msWriteByteMask(mtk_dp, REG_31B0_DP_ENCODER0_P0 + reg_offset_enc,
		1 << PGEN_PATTERN_SEL_DP_ENCODER0_P0_FLDMASK_POS,
		PGEN_PATTERN_SEL_DP_ENCODER0_P0_FLDMASK);
	msWriteByteMask(mtk_dp, REG_31B0_DP_ENCODER0_P0 + reg_offset_enc,
		1 << PGEN_PAT_DIRECTION_DP_ENCODER0_P0_FLDMASK_POS,
		PGEN_PAT_DIRECTION_DP_ENCODER0_P0_FLDMASK);
	msWriteByteMask(mtk_dp, REG_31B0_DP_ENCODER0_P0 + reg_offset_enc,
		rgb << PGEN_PAT_RGB_ENABLE_DP_ENCODER0_P0_FLDMASK_POS,
		PGEN_PAT_RGB_ENABLE_DP_ENCODER0_P0_FLDMASK);

	switch (location) {
	case DPTX_PG_LOCATION_ALL://All
		msWrite2ByteMask(mtk_dp, REG_317C_DP_ENCODER0_P0 + reg_offset_enc,
				0, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_3180_DP_ENCODER0_P0 + reg_offset_enc,
				0, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_3184_DP_ENCODER0_P0 + reg_offset_enc,
				color_depth, MASKBIT(11 : 0));

		msWrite2Byte(mtk_dp, REG_31A0_DP_ENCODER0_P0 + reg_offset_enc,
				0x3FFF);
		break;

	case DPTX_PG_LOCATION_TOP://Top
		msWrite2ByteMask(mtk_dp, REG_317C_DP_ENCODER0_P0 + reg_offset_enc,
							0, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_3180_DP_ENCODER0_P0 + reg_offset_enc,
							color_depth, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_3184_DP_ENCODER0_P0 + reg_offset_enc,
							0, MASKBIT(11 : 0));
		msWrite2Byte(mtk_dp, REG_31A0_DP_ENCODER0_P0 + reg_offset_enc, 0x40);
		break;

	case DPTX_PG_LOCATION_BOTTOM://Bottom
		msWrite2ByteMask(mtk_dp, REG_317C_DP_ENCODER0_P0 + reg_offset_enc,
							color_depth, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_3180_DP_ENCODER0_P0 + reg_offset_enc,
							0, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_3184_DP_ENCODER0_P0 + reg_offset_enc,
							0, MASKBIT(11 : 0));
		msWrite2Byte(mtk_dp, REG_31A0_DP_ENCODER0_P0 + reg_offset_enc, 0x2FFF);
		break;

	default://All
		msWrite2ByteMask(mtk_dp, REG_317C_DP_ENCODER0_P0 + reg_offset_enc,
							0, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_3180_DP_ENCODER0_P0 + reg_offset_enc,
							0, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_3184_DP_ENCODER0_P0 + reg_offset_enc,
							color_depth, MASKBIT(11 : 0));
		msWrite2Byte(mtk_dp, REG_31A0_DP_ENCODER0_P0 + reg_offset_enc, 0x3FFF);
		break;
	}
}

void mtk_dptx_hal_pg_h_ramping(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8  rgb, u8  location)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);
	u32 Ramp = 0x3FFF;

	// [11] video select hw or pattern gen 0:HW 1:PG
	msWriteByteMask(mtk_dp, REG_3038_DP_ENCODER0_P0 + 1 + reg_offset_enc, BIT(3), BIT(3));
	msWriteByteMask(mtk_dp, REG_31B0_DP_ENCODER0_P0 + reg_offset_enc,
		2 << PGEN_PATTERN_SEL_DP_ENCODER0_P0_FLDMASK_POS,
		PGEN_PATTERN_SEL_DP_ENCODER0_P0_FLDMASK);
	msWriteByteMask(mtk_dp, REG_31B0_DP_ENCODER0_P0 + reg_offset_enc,
		1 << PGEN_PAT_DIRECTION_DP_ENCODER0_P0_FLDMASK_POS,
		PGEN_PAT_DIRECTION_DP_ENCODER0_P0_FLDMASK);
	msWriteByteMask(mtk_dp, REG_31B0_DP_ENCODER0_P0 + reg_offset_enc,
		rgb << PGEN_PAT_RGB_ENABLE_DP_ENCODER0_P0_FLDMASK_POS,
		PGEN_PAT_RGB_ENABLE_DP_ENCODER0_P0_FLDMASK);

	msWrite2Byte(mtk_dp, REG_31A0_DP_ENCODER0_P0 + reg_offset_enc, Ramp);

	switch (location) {
	case DPTX_PG_LOCATION_ALL: //all
		msWrite2ByteMask(mtk_dp, REG_317C_DP_ENCODER0_P0 + reg_offset_enc,
							0, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_3180_DP_ENCODER0_P0 + reg_offset_enc,
							0, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_3184_DP_ENCODER0_P0 + reg_offset_enc,
							0, MASKBIT(11 : 0));
		break;

	case DPTX_PG_LOCATION_LEFT_OF_TOP: //Left of Top
		msWrite2ByteMask(mtk_dp, REG_317C_DP_ENCODER0_P0 + reg_offset_enc,
							0, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_3180_DP_ENCODER0_P0 + reg_offset_enc,
							0, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_3184_DP_ENCODER0_P0 + reg_offset_enc,
							0, MASKBIT(11 : 0));
		msWrite2Byte(mtk_dp, REG_31A0_DP_ENCODER0_P0 + reg_offset_enc, 0x3FFF);
		break;

	case DPTX_PG_LOCATION_LEFT_OF_BOTTOM: //Left of Bottom
		msWrite2ByteMask(mtk_dp, REG_317C_DP_ENCODER0_P0 + reg_offset_enc,
							0, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_3180_DP_ENCODER0_P0 + reg_offset_enc,
							0, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_3184_DP_ENCODER0_P0 + reg_offset_enc,
							0, MASKBIT(11 : 0));
		msWrite2Byte(mtk_dp, REG_31A0_DP_ENCODER0_P0 + reg_offset_enc, 0x3FFF);
		break;

	default:
		break;

	}
}

void mtk_dptx_hal_pg_v_color_bar(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8  location)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

	// [11] video select hw or pattern gen 0:HW 1:PG
	msWriteByteMask(mtk_dp,
		(REG_3038_DP_ENCODER0_P0 + 1 + reg_offset_enc), BIT(3), BIT(3));
	msWriteByteMask(mtk_dp,
		REG_31B0_DP_ENCODER0_P0 + reg_offset_enc,
		3 << PGEN_PATTERN_SEL_DP_ENCODER0_P0_FLDMASK_POS,
		PGEN_PATTERN_SEL_DP_ENCODER0_P0_FLDMASK);

	switch (location) {
	case DPTX_PG_LOCATION_ALL: //All
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset_enc,
							0, MASKBIT(5 : 4));
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset_enc,
							0, MASKBIT(2 : 0));
		break;

	case DPTX_PG_LOCATION_LEFT: //Left
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset_enc,
							BIT(4), MASKBIT(5 : 4));
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset_enc,
							0, MASKBIT(2 : 0));
		break;

	case DPTX_PG_LOCATION_RIGHT: //right
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset_enc,
							BIT(4), MASKBIT(5 : 4));
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset_enc,
							BIT(2), MASKBIT(2 : 0));
		break;

	case DPTX_PG_LOCATION_LEFT_OF_LEFT: //Left of Left
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset_enc,
							BIT(5) | BIT(4), MASKBIT(5 : 4));
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset_enc,
							0, MASKBIT(2 : 0));
		break;

	case DPTX_PG_LOCATION_RIGHT_OF_LEFT: //Right of Left
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset_enc,
							BIT(5) | BIT(4), MASKBIT(5 : 4));
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset_enc,
							BIT(1), MASKBIT(2 : 0));
		break;

	case DPTX_PG_LOCATION_LEFT_OF_RIGHT: //Left of Right
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset_enc,
							BIT(5) | BIT(4), MASKBIT(5 : 4));
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset_enc,
							BIT(2), MASKBIT(2 : 0));
		break;

	case DPTX_PG_LOCATION_RIGHT_OF_RIGHT: //Right of Right
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset_enc,
							BIT(5) | BIT(4), MASKBIT(5 : 4));
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset_enc,
							BIT(2) | BIT(1), MASKBIT(2 : 0));
		break;

	default:
		break;
	}
}

void mtk_dptx_hal_pg_h_color_bar(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8  location)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

	// [11] video select hw or pattern gen 0:HW 1:PG
	msWriteByteMask(mtk_dp, (REG_3038_DP_ENCODER0_P0 + 1 + reg_offset_enc), BIT(3), BIT(3));
	msWriteByteMask(mtk_dp,
		REG_31B0_DP_ENCODER0_P0 + reg_offset_enc,
		4 << PGEN_PATTERN_SEL_DP_ENCODER0_P0_FLDMASK_POS,
		PGEN_PATTERN_SEL_DP_ENCODER0_P0_FLDMASK);

	switch (location) {
	case DPTX_PG_LOCATION_ALL: //All
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset_enc,
							0, MASKBIT(5 : 4));
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset_enc,
							0, MASKBIT(2 : 0));
		break;

	case DPTX_PG_LOCATION_TOP: //Top
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset_enc,
							BIT(4), MASKBIT(5 : 4));
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset_enc,
							0, MASKBIT(2 : 0));
		break;

	case DPTX_PG_LOCATION_BOTTOM: //bottom
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset_enc,
							BIT(4), MASKBIT(5 : 4));
		msWriteByteMask(mtk_dp, REG_3190_DP_ENCODER0_P0 + reg_offset_enc,
							BIT(2), MASKBIT(2 : 0));
		break;

	default:
		break;
	}
}

void mtk_dptx_hal_pg_chess_board(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
				 u8  location,
				 u16 hde,
				 u16 vde)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

	// [11] video select hw or pattern gen 0:HW 1:PG
	msWriteByteMask(mtk_dp, REG_3038_DP_ENCODER0_P0 + 1 + reg_offset_enc, BIT(3), BIT(3));
	msWriteByteMask(mtk_dp, REG_31B0_DP_ENCODER0_P0 + reg_offset_enc,
						BIT(6) | BIT(4), MASKBIT(6 : 4));

	if (location == DPTX_PG_LOCATION_ALL) {
		msWrite2ByteMask(mtk_dp, REG_317C_DP_ENCODER0_P0 + reg_offset_enc,
							0xFFF, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_3180_DP_ENCODER0_P0 + reg_offset_enc,
							0xFFF, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_3184_DP_ENCODER0_P0 + reg_offset_enc,
							0xFFF, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_3194_DP_ENCODER0_P0 + reg_offset_enc,
							0, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_3198_DP_ENCODER0_P0 + reg_offset_enc,
							0, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_319C_DP_ENCODER0_P0 + reg_offset_enc,
							0, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_31A8_DP_ENCODER0_P0 + reg_offset_enc,
							(hde / 8), MASKBIT(13 : 0));
		msWrite2ByteMask(mtk_dp, REG_31AC_DP_ENCODER0_P0 + reg_offset_enc,
							(vde / 8), MASKBIT(13 : 0));
	}
}

void mtk_dptx_hal_pg_sub_pixel(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 location)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

	// [11] video select hw or pattern gen 0:HW 1:PG
	msWriteByteMask(mtk_dp, (REG_3038_DP_ENCODER0_P0 + 1 + reg_offset_enc), BIT(3), BIT(3));
	msWriteByteMask(mtk_dp, REG_31B0_DP_ENCODER0_P0 + reg_offset_enc,
						BIT(6) | BIT(5), MASKBIT(6 : 4));

	switch (location) {
	case DPTX_PG_PIXEL_ODD_MASK: //odd sub pixel mask
		msWriteByteMask(mtk_dp, (REG_31B0_DP_ENCODER0_P0 + 1 + reg_offset_enc), 0, BIT(5));
		break;

	case DPTX_PG_PIXEL_EVEN_MASK: //even sub pixel mask
		msWriteByteMask(mtk_dp, (REG_31B0_DP_ENCODER0_P0 + 1 + reg_offset_enc), BIT(5), BIT(5));
		break;

	default:
		break;
	}
}

void mtk_dptx_hal_pg_frame(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
			   u8 location,
			   u16 hde,
			   u16 vde)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

	// [11] video select hw or pattern gen 0:HW 1:PG
	msWriteByteMask(mtk_dp, (REG_3038_DP_ENCODER0_P0 + 1 + reg_offset_enc), BIT(3), BIT(3));
	msWriteByteMask(mtk_dp, REG_31B0_DP_ENCODER0_P0 + reg_offset_enc,
						BIT(6) | BIT(5) | BIT(4), MASKBIT(6 : 4));

	if (location == DPTX_PG_PIXEL_ODD_MASK) {
		msWriteByteMask(mtk_dp, (REG_31B0_DP_ENCODER0_P0 + 1 + reg_offset_enc), 0, BIT(5));
		msWrite2ByteMask(mtk_dp, REG_317C_DP_ENCODER0_P0 + reg_offset_enc,
							0xFFF, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_3180_DP_ENCODER0_P0 + reg_offset_enc,
							0xFFF, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_3184_DP_ENCODER0_P0 + reg_offset_enc,
							0xFFF, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_3194_DP_ENCODER0_P0 + reg_offset_enc,
							0xFFF, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_3198_DP_ENCODER0_P0 + reg_offset_enc,
							0xFFF, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_319C_DP_ENCODER0_P0 + reg_offset_enc,
							0, MASKBIT(11 : 0));
		msWrite2ByteMask(mtk_dp, REG_31A8_DP_ENCODER0_P0 + reg_offset_enc,
							((hde / 8) - 12), MASKBIT(13 : 0));
		msWrite2ByteMask(mtk_dp, REG_31AC_DP_ENCODER0_P0 + reg_offset_enc,
							((vde / 8) - 12), MASKBIT(13 : 0));
		msWriteByteMask(mtk_dp, REG_31B4_DP_ENCODER0_P0 + reg_offset_enc,
							0x0B, MASKBIT(3 : 0));
	}
}

static void mtk_dptx_hal_spkg_asp_hb32(struct mtk_dp *mtk_dp, enum DPTX_ENCODER_ID encoder_id,
					u8 enable, u8 HB3, u8 HB2)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

	msWrite2ByteMask(mtk_dp, REG_30BC_DP_ENCODER0_P0 + reg_offset_enc,
			(enable ? 0x01 : 0x00) << ASP_HB23_SEL_DP_ENCODER0_P0_FLDMASK_POS,
			ASP_HB23_SEL_DP_ENCODER0_P0_FLDMASK);
	msWrite2ByteMask(mtk_dp, REG_312C_DP_ENCODER0_P0 + reg_offset_enc,
			HB2 << ASP_HB2_DP_ENCODER0_P0_FLDMASK_POS,
			ASP_HB2_DP_ENCODER0_P0_FLDMASK);
	msWrite2ByteMask(mtk_dp, REG_312C_DP_ENCODER0_P0 + reg_offset_enc,
			HB3 << ASP_HB3_DP_ENCODER0_P0_FLDMASK_POS,
			ASP_HB3_DP_ENCODER0_P0_FLDMASK);
}

void mtk_dptx_hal_spkg_sdp(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
			   u8 enable,
			   u8 sdp_type,
			   u8 *hb,
			   u8 *db)
{
	u8  offset;
	u16 st_offset;
	u8  hb_offset;
	u8  reg_index;
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

	if (enable) {
		for (offset = 0; offset < 0x10; offset++)
			for (reg_index = 0; reg_index < 2; reg_index++) {
				u32 addr = REG_3200_DP_ENCODER1_P0
					      + offset * 4 + reg_index + reg_offset_enc;

				msWriteByte(mtk_dp, addr, (db[offset * 2 + reg_index]));
				DPTXDBG("SDP address %x, data %x\n",
						addr, db[offset * 2 + reg_index]);
			}

		if (sdp_type == DPTx_SDPTYP_DRM) {
			for (hb_offset = 0; hb_offset < 4 / 2; hb_offset++)
				for (reg_index = 0; reg_index < 2; reg_index++) {
					u32 addr = REG_3138_DP_ENCODER0_P0
					+ hb_offset * 4 + reg_index + reg_offset_enc;
					u8 pOffset = hb_offset * 2 + reg_index;

					msWriteByte(mtk_dp, addr, (hb[pOffset]));
					DPTXDBG("W Reg addr: %x, index %d\n", addr, pOffset);
				}
		} else if (sdp_type >= DPTx_SDPTYP_PPS0
			   && sdp_type <= DPTx_SDPTYP_PPS3) {
			for (hb_offset = 0; hb_offset < (4 / 2); hb_offset++)
				for (reg_index = 0; reg_index < 2; reg_index++) {
					u32 addr = REG_3130_DP_ENCODER0_P0
					+ hb_offset * 4 + reg_index + reg_offset_enc;
					u8 pOffset = hb_offset * 2 + reg_index;

					msWriteByte(mtk_dp, addr, hb[pOffset]);
					DPTXDBG("W H1 Reg addr:%x,index:%d\n", addr, pOffset);
				}
		} else if (sdp_type == DPTx_SDPTYP_ADS) {
			for (hb_offset = 0; hb_offset < (4 >> 1); hb_offset++)
				for (reg_index = 0; reg_index < 2; reg_index++) {
					u32 addr = REG_31F0_DP_ENCODER0_P0 + reg_offset_enc
									+ hb_offset * 8 + reg_index;
					u8 pOffset = hb_offset * 2 + reg_index;

					msWriteByte(mtk_dp, addr, hb[pOffset]);
				}
		} else {
			st_offset = (sdp_type - DPTx_SDPTYP_ACM) * 8;

			for (hb_offset = 0; hb_offset < 4 / 2; hb_offset++)
				for (reg_index = 0; reg_index < 2; reg_index++) {
					u32 addr = REG_30D8_DP_ENCODER0_P0
					+ st_offset
					+ hb_offset * 4 + reg_index + reg_offset_enc;
					u8 pOffset = hb_offset * 2 + reg_index;

					msWriteByte(mtk_dp, addr, hb[pOffset]);
					DPTXDBG("W H2 Reg addr: %x,index %d\n", addr, pOffset);
				}
		}
	}

	switch (sdp_type) {
	case DPTx_SDPTYP_NONE:
		break;

	case DPTx_SDPTYP_ACM:
		msWriteByte(mtk_dp, REG_30B4_DP_ENCODER0_P0 + reg_offset_enc, 0x00);

		if (enable) {
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc,
								DPTx_SDPTYP_ACM,
								(BIT(4) | BIT(3) | BIT(2) | BIT(1) | BIT(0)));
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc,
								BIT(5), BIT(5));
			msWriteByte(mtk_dp, REG_30B4_DP_ENCODER0_P0 + reg_offset_enc, 0x05);
			DPTXMSG("SENT SDP TYPE ACM\n");
		}

		break;

	case DPTx_SDPTYP_ISRC:
		msWriteByte(mtk_dp, (REG_30B4_DP_ENCODER0_P0 + 1 + reg_offset_enc), 0x00);

		if (enable) {
			msWriteByte(mtk_dp, (REG_31EC_DP_ENCODER0_P0 + 1 + reg_offset_enc), 0x1C);
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc,
								DPTx_SDPTYP_ISRC,
								(BIT(4) | BIT(3) | BIT(2) | BIT(1) | BIT(0)));

			if (hb[3] & BIT(2))
				msWriteByteMask(mtk_dp, REG_30BC_DP_ENCODER0_P0 + reg_offset_enc,
								BIT(0), BIT(0));
			else
				msWriteByteMask(mtk_dp, REG_30BC_DP_ENCODER0_P0 + reg_offset_enc,
								0, BIT(0));

			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc, BIT(5), BIT(5));
			msWriteByte(mtk_dp, (REG_30B4_DP_ENCODER0_P0 + 1 + reg_offset_enc), 0x05);
			DPTXMSG("SENT SDP TYPE ISRC\n");
		}

		break;

	case DPTx_SDPTYP_AVI:
		msWriteByte(mtk_dp, (REG_30A4_DP_ENCODER0_P0 + 1 + reg_offset_enc), 0x00);

		if (enable) {
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc,
								DPTx_SDPTYP_AVI,
								(BIT(4) | BIT(3) | BIT(2) | BIT(1) | BIT(0)));
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc, BIT(5), BIT(5));
			msWriteByte(mtk_dp, (REG_30A4_DP_ENCODER0_P0 + 1 + reg_offset_enc), 0x05);
			DPTXMSG("SENT SDP TYPE AVI\n");
		}

		break;

	case DPTx_SDPTYP_AUI:
		msWriteByte(mtk_dp, REG_30A8_DP_ENCODER0_P0 + reg_offset_enc, 0x00);

		if (enable) {
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc,
								DPTx_SDPTYP_AUI,
								(BIT(4) | BIT(3) | BIT(2) | BIT(1) | BIT(0)));
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc, BIT(5), BIT(5));
			msWriteByte(mtk_dp, REG_30A8_DP_ENCODER0_P0 + reg_offset_enc, 0x05);
			DPTXMSG("SENT SDP TYPE AUI\n");
		}

		break;

	case DPTx_SDPTYP_SPD:
		msWriteByte(mtk_dp, (REG_30A8_DP_ENCODER0_P0 + 1 + reg_offset_enc), 0x00);

		if (enable) {
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc,
								DPTx_SDPTYP_SPD,
								(BIT(4) | BIT(3) | BIT(2) | BIT(1) | BIT(0)));
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc, BIT(5), BIT(5));
			msWriteByte(mtk_dp, REG_30A8_DP_ENCODER0_P0 + 1 + reg_offset_enc, 0x05);
			DPTXMSG("SENT SDP TYPE SPD\n");
		}

		break;

	case DPTx_SDPTYP_MPEG:
		msWriteByte(mtk_dp, REG_30AC_DP_ENCODER0_P0 + reg_offset_enc, 0x00);

		if (enable) {
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc,
								DPTx_SDPTYP_MPEG,
								(BIT(4) | BIT(3) | BIT(2) | BIT(1) | BIT(0)));
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc, BIT(5), BIT(5));
			msWriteByte(mtk_dp, REG_30AC_DP_ENCODER0_P0 + reg_offset_enc, 0x05);
			DPTXMSG("SENT SDP TYPE MPEG\n");
		}

		break;

	case DPTx_SDPTYP_NTSC:
		msWriteByte(mtk_dp, (REG_30AC_DP_ENCODER0_P0 + 1 + reg_offset_enc), 0x00);

		if (enable) {
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc,
								DPTx_SDPTYP_NTSC,
								(BIT(4) | BIT(3) | BIT(2) | BIT(1) | BIT(0)));
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc, BIT(5), BIT(5));
			msWriteByte(mtk_dp, (REG_30AC_DP_ENCODER0_P0 + 1 + reg_offset_enc), 0x05);
			DPTXMSG("SENT SDP TYPE NTSC\n");
		}

		break;

	case DPTx_SDPTYP_VSP:
		msWriteByte(mtk_dp, REG_30B0_DP_ENCODER0_P0 + reg_offset_enc, 0x00);

		if (enable) {
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc,
								DPTx_SDPTYP_VSP,
								(BIT(4) | BIT(3) | BIT(2) | BIT(1) | BIT(0)));
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc, BIT(5), BIT(5));
			msWriteByte(mtk_dp, REG_30B0_DP_ENCODER0_P0 + reg_offset_enc, 0x05);
			DPTXMSG("SENT SDP TYPE VSP\n");
		}

		break;

	case DPTx_SDPTYP_VSC:
		msWriteByte(mtk_dp, REG_30B8_DP_ENCODER0_P0 + reg_offset_enc, 0x00);

		if (enable) {
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc,
								DPTx_SDPTYP_VSC,
								(BIT(4) | BIT(3) | BIT(2) | BIT(1) | BIT(0)));
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc, BIT(5), BIT(5));
			msWriteByte(mtk_dp, REG_30B8_DP_ENCODER0_P0 + reg_offset_enc, 0x05);
			DPTXMSG("SENT SDP TYPE VSC\n");
		}

		break;

	case DPTx_SDPTYP_EXT:
		msWriteByte(mtk_dp, (REG_30B0_DP_ENCODER0_P0 + 1 + reg_offset_enc), 0x00);

		if (enable) {
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc,
								DPTx_SDPTYP_EXT,
								(BIT(4) | BIT(3) | BIT(2) | BIT(1) | BIT(0)));
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc, BIT(5), BIT(5));
			msWriteByte(mtk_dp, REG_30B0_DP_ENCODER0_P0 + 1 + reg_offset_enc, 0x05);
			DPTXMSG("SENT SDP TYPE EXT\n");
		}

		break;

	case DPTx_SDPTYP_PPS0:
		msWriteByte(mtk_dp, REG_31E8_DP_ENCODER0_P0 + reg_offset_enc, 0x00);

		if (enable) {
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc,
								DPTx_SDPTYP_PPS0,
								(BIT(4) | BIT(3) | BIT(2) | BIT(1) | BIT(0)));
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc, BIT(5), BIT(5));
			msWriteByte(mtk_dp, REG_31E8_DP_ENCODER0_P0 + reg_offset_enc, 0x05);
			DPTXMSG("SENT SDP TYPE PPS0\n");
		}

		break;

	case DPTx_SDPTYP_PPS1:
		msWriteByte(mtk_dp, REG_31E8_DP_ENCODER0_P0 + reg_offset_enc, 0x00);

		if (enable) {
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc,
								DPTx_SDPTYP_PPS1,
								(BIT(4) | BIT(3) | BIT(2) | BIT(1) | BIT(0)));
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc, BIT(5), BIT(5));
			msWriteByte(mtk_dp, REG_31E8_DP_ENCODER0_P0 + reg_offset_enc, 0x05);
			DPTXMSG("SENT SDP TYPE PPS1\n");
		}

		break;

	case DPTx_SDPTYP_PPS2:
		msWriteByte(mtk_dp, REG_31E8_DP_ENCODER0_P0 + reg_offset_enc, 0x00);

		if (enable) {
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc,
								DPTx_SDPTYP_PPS2,
								(BIT(4) | BIT(3) | BIT(2) | BIT(1) | BIT(0)));
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc, BIT(5), BIT(5));
			msWriteByte(mtk_dp, REG_31E8_DP_ENCODER0_P0 + reg_offset_enc, 0x05);
			DPTXMSG("SENT SDP TYPE PPS2\n");
		}

		break;

	case DPTx_SDPTYP_PPS3:
		msWriteByte(mtk_dp, REG_31E8_DP_ENCODER0_P0 + reg_offset_enc, 0x00);

		if (enable) {
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc,
								DPTx_SDPTYP_PPS3,
								(BIT(4) | BIT(3) | BIT(2) | BIT(1) | BIT(0)));
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc, BIT(5), BIT(5));
			msWriteByte(mtk_dp, REG_31E8_DP_ENCODER0_P0 + reg_offset_enc, 0x05);
			DPTXMSG("SENT SDP TYPE PPS3\n");
		}

		break;

	case DPTx_SDPTYP_DRM:
		msWriteByte(mtk_dp, REG_31DC_DP_ENCODER0_P0 + reg_offset_enc, 0x00);

		if (enable) {
			msWriteByte(mtk_dp, REG_3138_DP_ENCODER0_P0 + reg_offset_enc, hb[0]);
			msWriteByte(mtk_dp, (REG_3138_DP_ENCODER0_P0 + 1 + reg_offset_enc), hb[1]);
			msWriteByte(mtk_dp, REG_313C_DP_ENCODER0_P0 + reg_offset_enc, hb[2]);
			msWriteByte(mtk_dp, REG_313C_DP_ENCODER0_P0 + 1 + reg_offset_enc, hb[3]);
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc,
								DPTx_SDPTYP_DRM,
								(BIT(4) | BIT(3) | BIT(2) | BIT(1) | BIT(0)));
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc, BIT(5), BIT(5));
			msWriteByte(mtk_dp, REG_31DC_DP_ENCODER0_P0 + reg_offset_enc, 0x05);
			DPTXMSG("SENT SDP TYPE DRM\n");
		}

		break;

	case DPTx_SDPTYP_ADS:
		// adaptive sync SDP transmit disable
		msWriteByteMask(mtk_dp, REG_31EC_DP_ENCODER0_P0 + reg_offset_enc, 0,
						ADS_CFG_DP_ENCODER0_P0_FLDMASK);
		if (enable) {
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc,
						DPTx_SDPTYP_ADS,
						SDP_PACKET_TYPE_DP_ENCODER1_P0_FLDMASK);
			//write sdp data trigger
			msWriteByteMask(mtk_dp, REG_3280_DP_ENCODER1_P0 + reg_offset_enc,
				1 << SDP_PACKET_W_DP_ENCODER1_P0_FLDMASK_POS,
				SDP_PACKET_W_DP_ENCODER1_P0_FLDMASK);
			// adaptive sync SDP transmit enable
			msWriteByteMask(mtk_dp, REG_31EC_DP_ENCODER0_P0 + reg_offset_enc,
						1 << ADS_CFG_DP_ENCODER0_P0_FLDMASK_POS,
						ADS_CFG_DP_ENCODER0_P0_FLDMASK);
			DPTXMSG("SENT SDP TYPE ADS\n");
		}

		break;

	default:
		break;
	}
}

void mtk_dptx_hal_spkg_vsc_ext_vesa(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
				    u8 enable,
				    u8 hdr_num,
				    u8 *db)
{
	u8  vsc_hb1 = 0x20;	// VESA : 0x20; CEA : 0x21
	u8  vsc_hb2;
	u8  pkg_cnt;
	u8  loop;
	u8  offset;
	u8  reg_index;
	u16 sdp_offset;
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

	if (!enable) {
		msWriteByteMask(mtk_dp, (REG_30A0_DP_ENCODER0_P0 + 1 + reg_offset_enc), 0, BIT(0));
		msWriteByteMask(mtk_dp, REG_328C_DP_ENCODER1_P0 + reg_offset_enc, 0, BIT(7));
		return;
	}

	vsc_hb2 = (hdr_num > 0) ? BIT(6) : 0x00;

	msWriteByte(mtk_dp, REG_31C8_DP_ENCODER0_P0 + reg_offset_enc, 0x00);
	msWriteByte(mtk_dp, (REG_31C8_DP_ENCODER0_P0 + 1 + reg_offset_enc), vsc_hb1);
	msWriteByte(mtk_dp, REG_31CC_DP_ENCODER0_P0 + reg_offset_enc, vsc_hb2);
	msWriteByte(mtk_dp, (REG_31CC_DP_ENCODER0_P0 + 1 + reg_offset_enc), 0x00);
	msWriteByte(mtk_dp, REG_31D8_DP_ENCODER0_P0 + reg_offset_enc, hdr_num);

	msWriteByteMask(mtk_dp, REG_328C_DP_ENCODER1_P0 + reg_offset_enc, BIT(0), BIT(0));
	msWriteByteMask(mtk_dp, REG_328C_DP_ENCODER1_P0 + reg_offset_enc, BIT(2), BIT(2));

	udelay(50);
	msWriteByteMask(mtk_dp, REG_328C_DP_ENCODER1_P0 + reg_offset_enc, 0, BIT(2));
	udelay(50);

	for (pkg_cnt = 0; pkg_cnt < (hdr_num + 1); pkg_cnt++) {
		sdp_offset = 0;

		for (loop = 0; loop < 4; loop++) {
			for (offset = 0; offset < 8 / 2; offset++) {
				for (reg_index = 0; reg_index < 2; reg_index++) {
					u32 addr = REG_3290_DP_ENCODER1_P0
					+ offset * 4 + reg_index + reg_offset_enc;
					u8 pOffset = sdp_offset
							+ offset * 2 + reg_index;

					msWriteByte(mtk_dp, addr, db[pOffset]);
				}
			}

			msWriteByteMask(mtk_dp, REG_328C_DP_ENCODER1_P0 + reg_offset_enc, BIT(6), BIT(6));
			sdp_offset += 8;
		}
	}

	msWriteByteMask(mtk_dp, (REG_30A0_DP_ENCODER0_P0 + 1 + reg_offset_enc), BIT(0), BIT(0));
	msWriteByteMask(mtk_dp, REG_328C_DP_ENCODER1_P0 + reg_offset_enc, BIT(7), BIT(7));
}

void mtk_dptx_hal_spkg_vsc_ext_cea(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
				   u8  enable,
				   u8  hdr_num,
				   u8 *db)
{
	u8  vsc_hb1 = 0x21;
	u8  vsc_hb2;
	u8  pkg_cnt;
	u8  loop;
	u8  offset;
	u8  reg_index;
	u16 sdp_offset;
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

	if (!enable) {
		msWriteByteMask(mtk_dp, (REG_30A0_DP_ENCODER0_P0 + 1  + reg_offset_enc), 0, BIT(4));
		msWriteByteMask(mtk_dp, REG_32A0_DP_ENCODER1_P0  + reg_offset_enc, 0, BIT(7));
		return;
	}

	vsc_hb2 = (hdr_num > 0) ? 0x40 : 0x00;

	msWriteByte(mtk_dp, REG_31D0_DP_ENCODER0_P0  + reg_offset_enc, 0x00);
	msWriteByte(mtk_dp, (REG_31D0_DP_ENCODER0_P0 + 1  + reg_offset_enc), vsc_hb1);
	msWriteByte(mtk_dp, REG_31D4_DP_ENCODER0_P0  + reg_offset_enc, vsc_hb2);
	msWriteByte(mtk_dp, (REG_31D4_DP_ENCODER0_P0 + 1  + reg_offset_enc), 0x00);
	msWriteByte(mtk_dp, (REG_31D8_DP_ENCODER0_P0 + 1  + reg_offset_enc), hdr_num);

	msWriteByteMask(mtk_dp, REG_32A0_DP_ENCODER1_P0  + reg_offset_enc, BIT(0), BIT(0));
	msWriteByteMask(mtk_dp, REG_32A0_DP_ENCODER1_P0  + reg_offset_enc, BIT(2), BIT(2));
	udelay(50);

	msWriteByteMask(mtk_dp, REG_32A0_DP_ENCODER1_P0  + reg_offset_enc, 0, BIT(2));

	for (pkg_cnt = 0; pkg_cnt < (hdr_num + 1); pkg_cnt++) {
		sdp_offset = 0;

		for (loop = 0; loop < 4; loop++) {
			for (offset = 0; offset < 4; offset++) {
				for (reg_index = 0; reg_index < 2; reg_index++) {
					u32 addr = REG_32A4_DP_ENCODER1_P0
					+ offset * 4 + reg_index  + reg_offset_enc;
					u8 pOffset = sdp_offset
							+ offset * 2 + reg_index;

					msWriteByte(mtk_dp, addr, db[pOffset]);
				}
			}

			msWriteByteMask(mtk_dp, REG_32A0_DP_ENCODER1_P0  + reg_offset_enc, BIT(6), BIT(6));
			sdp_offset += 8;
		}
	}

	msWriteByteMask(mtk_dp, (REG_30A0_DP_ENCODER0_P0 + 1  + reg_offset_enc), BIT(4), BIT(4));
	msWriteByteMask(mtk_dp, REG_32A0_DP_ENCODER1_P0  + reg_offset_enc, BIT(7), BIT(7));
}


void mtk_dptx_hal_audio_sample_arrange(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 enable)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);
	u32 value = 0;

	//0x3374 [12] = enable,
	//0x3374 [11:0] = hblank * link_rate(MHZ) / pix_clk(MHZ) / 4 * 0.8
	value = (mtk_dp->info[encoder_id].DPTX_OUTBL.Htt
			- mtk_dp->info[encoder_id].DPTX_OUTBL.Hde) *
		mtk_dp->training_info.ubLinkRate * 27 * 200 /
		mtk_dp->info[encoder_id].DPTX_OUTBL.PixRateKhz;
	//value = 1;
	if (enable) {
		msWrite4ByteMask(mtk_dp, REG_3374_DP_ENCODER1_P0 + reg_offset_enc,
							BIT(12), BIT(12));
		msWrite4ByteMask(mtk_dp, REG_3374_DP_ENCODER1_P0 + reg_offset_enc,
							(u16)value, BITMASK(11 : 0));
	} else {
		msWrite4ByteMask(mtk_dp, REG_3374_DP_ENCODER1_P0 + reg_offset_enc, 0, BIT(12));
		msWrite4ByteMask(mtk_dp, REG_3374_DP_ENCODER1_P0 + reg_offset_enc,
							0, BITMASK(11 : 0));
	}

	DPTXDBG("Encoder %d, Audio arrange patch enable = %d, value = 0x%x\n",
				encoder_id, enable, value);
}

void mtk_dptx_hal_audio_pg_enable(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
				  u8 channel,
				  u8 fs,
				  u8 enable)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

#ifdef DPTX_FPGA
	//Gen an audio sample per 255T (audio clock) for FPGA
	//if (enable)
		//msWrite2ByteMask(mtk_dp, REG_3320_DP_ENCODER1_P0 + reg_offset_enc,
		//0xFF << AUDIO_PATTERN_GEN_DSTB_CNT_THRD_DP_ENCODER1_P0_FLDMASK_POS,
		//AUDIO_PATTERN_GEN_DSTB_CNT_THRD_DP_ENCODER1_P0_FLDMASK);
	//else
		msWrite2ByteMask(mtk_dp, REG_3320_DP_ENCODER1_P0 + reg_offset_enc,
		0x1FF << AUDIO_PATTERN_GEN_DSTB_CNT_THRD_DP_ENCODER1_P0_FLDMASK_POS,
		AUDIO_PATTERN_GEN_DSTB_CNT_THRD_DP_ENCODER1_P0_FLDMASK);
#endif
	msWrite2ByteMask(mtk_dp, REG_307C_DP_ENCODER0_P0 + reg_offset_enc, 0,
		HBLANK_SPACE_FOR_SDP_HW_EN_DP_ENCODER0_P0_FLDMASK);
	if (enable) {
		msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0 + reg_offset_enc,
				 AU_GEN_EN_DP_ENCODER0_P0_FLDMASK,
				 AU_GEN_EN_DP_ENCODER0_P0_FLDMASK);

		//[9 : 8] set 0x3 : PG
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0 + reg_offset_enc,
				 0x3<<AUDIO_SOURCE_MUX_DP_ENCODER1_P0_FLDMASK_POS,
				 AUDIO_SOURCE_MUX_DP_ENCODER1_P0_FLDMASK);
		msWrite2ByteMask(mtk_dp, REG_331C_DP_ENCODER1_P0 + reg_offset_enc,
				 0x0, TDM_AUDIO_DATA_EN_DP_ENCODER1_P0_FLDMASK);
		msWriteByteMask(mtk_dp, REG_33F4_DP_ENCODER1_P0 + reg_offset_enc, 0, BIT(0));
	} else {
		msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0 + reg_offset_enc,
				 0, AU_GEN_EN_DP_ENCODER0_P0_FLDMASK);
		//[ 9 : 8] set 0x0 : dprx, for Source project, it means for front-end audio
		//[10 : 8] set 0x4 : TDM after (include) Posnot
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0 + reg_offset_enc,
				0x4 << AUDIO_SOURCE_MUX_DP_ENCODER1_P0_FLDMASK_POS
				, AUDIO_SOURCE_MUX_DP_ENCODER1_P0_FLDMASK);
		//[0]: TDM to DPTX transfer enable
		msWrite2ByteMask(mtk_dp, REG_331C_DP_ENCODER1_P0 + reg_offset_enc,
				 TDM_AUDIO_DATA_EN_DP_ENCODER1_P0_FLDMASK,
				 TDM_AUDIO_DATA_EN_DP_ENCODER1_P0_FLDMASK);
		//[12:8]: TDM audio data 32 bit
		//32bit:0x1F
		//24bit:0x17
		//20bit:0x13
		//16bit:0x0F
		msWrite2ByteMask(mtk_dp, REG_331C_DP_ENCODER1_P0 + reg_offset_enc,
				(0x1F << TDM_AUDIO_DATA_BIT_DP_ENCODER1_P0_FLDMASK_POS),
				TDM_AUDIO_DATA_BIT_DP_ENCODER1_P0_FLDMASK);
		msWriteByteMask(mtk_dp, REG_33F4_DP_ENCODER1_P0 + reg_offset_enc, BIT(0), BIT(0));
	}

	msWriteByteMask(mtk_dp, REG_33F4_DP_ENCODER1_P0, 0, BIT(0)); //macshen refine

	DPTXMSG("encoder_id = %d, fs = %d, ch = %d\n", encoder_id, fs, channel);

	//audio channel count change reset
	msWriteByteMask(mtk_dp, (REG_33F4_DP_ENCODER1_P0 + 1 + reg_offset_enc), BIT(1), BIT(1));

	msWrite2ByteMask(mtk_dp, REG_3304_DP_ENCODER1_P0 + reg_offset_enc,
			 AU_PRTY_REGEN_DP_ENCODER1_P0_FLDMASK,
			 AU_PRTY_REGEN_DP_ENCODER1_P0_FLDMASK);
	msWrite2ByteMask(mtk_dp, REG_3304_DP_ENCODER1_P0 + reg_offset_enc,
			 AU_CH_STS_REGEN_DP_ENCODER1_P0_FLDMASK,
			 AU_CH_STS_REGEN_DP_ENCODER1_P0_FLDMASK);

	msWrite2ByteMask(mtk_dp, REG_3304_DP_ENCODER1_P0 + reg_offset_enc,
			 0x1000, 0x1000);
			 //,AUDIO_SAMPLE_PRSENT_REGEN_DP_ENCODER1_P0_FLDMASK);
			 //,AUDIO_SAMPLE_PRESENT_REGEN_DP_ENCODER1_P0_FLDMASK); //after Liber
	msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0 + reg_offset_enc,
			 AUDIO_2CH_SEL_DP_ENCODER0_P0_FLDMASK,
			 AUDIO_2CH_SEL_DP_ENCODER0_P0_FLDMASK);
	msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0 + reg_offset_enc,
			 AUDIO_MN_GEN_EN_DP_ENCODER0_P0_FLDMASK,
			 AUDIO_MN_GEN_EN_DP_ENCODER0_P0_FLDMASK);
	msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0 + reg_offset_enc,
			 AUDIO_8CH_SEL_DP_ENCODER0_P0_FLDMASK,
			 AUDIO_8CH_SEL_DP_ENCODER0_P0_FLDMASK);
	msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0 + reg_offset_enc,
			 AU_EN_DP_ENCODER0_P0_FLDMASK,
			 AU_EN_DP_ENCODER0_P0_FLDMASK);
	msWrite2ByteMask(mtk_dp, REG_3040_DP_ENCODER0_P0 + reg_offset_enc,
			 AUDIO_16CH_SEL_DP_ENCODER0_P0_FLDMASK,
			 AUDIO_16CH_SEL_DP_ENCODER0_P0_FLDMASK);
	msWrite2ByteMask(mtk_dp, REG_3040_DP_ENCODER0_P0 + reg_offset_enc,
			 AUDIO_32CH_SEL_DP_ENCODER0_P0_FLDMASK,
			 AUDIO_32CH_SEL_DP_ENCODER0_P0_FLDMASK);

	switch (fs) {
	case FS_44K:
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0 + reg_offset_enc,
				 (0x0 << AUDIO_PATGEN_FS_SEL_DP_ENCODER1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_FS_SEL_DP_ENCODER1_P0_FLDMASK);
		break;

	case FS_48K:
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0 + reg_offset_enc,
				 (0x1 << AUDIO_PATGEN_FS_SEL_DP_ENCODER1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_FS_SEL_DP_ENCODER1_P0_FLDMASK);
		break;

	case FS_192K:
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0 + reg_offset_enc,
				 (0x2 << AUDIO_PATGEN_FS_SEL_DP_ENCODER1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_FS_SEL_DP_ENCODER1_P0_FLDMASK);
		break;

	default:
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0 + reg_offset_enc,
				 (0x0 << AUDIO_PATGEN_FS_SEL_DP_ENCODER1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_FS_SEL_DP_ENCODER1_P0_FLDMASK);
		break;
	}

	msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0 + reg_offset_enc, 0,
			AUDIO_2CH_EN_DP_ENCODER0_P0_FLDMASK);
	msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0 + reg_offset_enc, 0,
						AUDIO_8CH_EN_DP_ENCODER0_P0_FLDMASK);
	msWrite2ByteMask(mtk_dp, REG_3040_DP_ENCODER0_P0 + reg_offset_enc, 0,
						AUDIO_16CH_EN_DP_ENCODER0_P0_FLDMASK);
	msWrite2ByteMask(mtk_dp, REG_3040_DP_ENCODER0_P0 + reg_offset_enc, 0,
						AUDIO_32CH_EN_DP_ENCODER0_P0_FLDMASK);

	switch (channel) {
	case 2:
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0 + reg_offset_enc,
				 (0x0 << AUDIO_PATGEN_FS_SEL_DP_ENCODER1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_CH_NUM_DP_ENCODER1_P0_FLDMASK);
		msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0 + reg_offset_enc,
				 (0x1 << AUDIO_2CH_EN_DP_ENCODER0_P0_FLDMASK_POS),
				 AUDIO_2CH_EN_DP_ENCODER0_P0_FLDMASK);
		//TDM audio interface, audio channel number, 1: 2ch
		msWrite2ByteMask(mtk_dp, REG_331C_DP_ENCODER1_P0 + reg_offset_enc,
				 (0x1 << TDM_AUDIO_DATA_CH_NUM_DP_ENCODER1_P0_FLDMASK_POS),
				 TDM_AUDIO_DATA_CH_NUM_DP_ENCODER1_P0_FLDMASK);
		mtk_dptx_hal_spkg_asp_hb32(mtk_dp, encoder_id, TRUE, DPTX_SDP_ASP_HB3_AU02CH, 0x0);
		break;

	case 8:
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0 + reg_offset_enc,
				 (0x1 << AUDIO_PATGEN_FS_SEL_DP_ENCODER1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_CH_NUM_DP_ENCODER1_P0_FLDMASK);
		msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0 + reg_offset_enc,
				 (0x1 << AUDIO_8CH_EN_DP_ENCODER0_P0_FLDMASK_POS),
				 AUDIO_8CH_EN_DP_ENCODER0_P0_FLDMASK);
		//TDM audio interface, audio channel number, 7: 8ch
		msWrite2ByteMask(mtk_dp, REG_331C_DP_ENCODER1_P0 + reg_offset_enc,
				 (0x7 << TDM_AUDIO_DATA_CH_NUM_DP_ENCODER1_P0_FLDMASK_POS),
				 TDM_AUDIO_DATA_CH_NUM_DP_ENCODER1_P0_FLDMASK);
		mtk_dptx_hal_spkg_asp_hb32(mtk_dp, encoder_id, TRUE, DPTX_SDP_ASP_HB3_AU08CH, 0x0);
		break;

	case 16:
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0 + reg_offset_enc,
				 (0x2 << AUDIO_PATGEN_FS_SEL_DP_ENCODER1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_CH_NUM_DP_ENCODER1_P0_FLDMASK);
		msWrite2ByteMask(mtk_dp, REG_3040_DP_ENCODER0_P0 + reg_offset_enc,
				 (0x1 << AUDIO_16CH_EN_DP_ENCODER0_P0_FLDMASK_POS),
				 AUDIO_16CH_EN_DP_ENCODER0_P0_FLDMASK);
		break;

	case 32:
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0 + reg_offset_enc,
				 (0x3 << AUDIO_PATGEN_FS_SEL_DP_ENCODER1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_CH_NUM_DP_ENCODER1_P0_FLDMASK);
		msWrite2ByteMask(mtk_dp, REG_3040_DP_ENCODER0_P0 + reg_offset_enc,
				 (0x1 << AUDIO_32CH_EN_DP_ENCODER0_P0_FLDMASK_POS),
				 AUDIO_32CH_EN_DP_ENCODER0_P0_FLDMASK);
		break;

	default:
		msWrite2ByteMask(mtk_dp, REG_3324_DP_ENCODER1_P0 + reg_offset_enc,
				 (0x0 << AUDIO_PATGEN_FS_SEL_DP_ENCODER1_P0_FLDMASK_POS),
				 AUDIO_PATTERN_GEN_CH_NUM_DP_ENCODER1_P0_FLDMASK);
		msWrite2ByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0 + reg_offset_enc,
				 (0x1 << AUDIO_2CH_EN_DP_ENCODER0_P0_FLDMASK_POS),
				 AUDIO_2CH_EN_DP_ENCODER0_P0_FLDMASK);
		//TDM audio interface, audio channel number, 1: 2ch
		msWrite2ByteMask(mtk_dp, REG_331C_DP_ENCODER1_P0 + reg_offset_enc,
				 (0x1 << TDM_AUDIO_DATA_CH_NUM_DP_ENCODER1_P0_FLDMASK_POS),
				 TDM_AUDIO_DATA_CH_NUM_DP_ENCODER1_P0_FLDMASK);
		mtk_dptx_hal_spkg_asp_hb32(mtk_dp, encoder_id, TRUE, DPTX_SDP_ASP_HB3_AU02CH, 0x0);
		break;
	}

	//TDM to DPTX reset [1]
	msWriteByteMask(mtk_dp, (REG_331C_DP_ENCODER1_P0 + reg_offset_enc),
				TDM_AUDIO_RST_DP_ENCODER1_P0_FLDMASK,
				TDM_AUDIO_RST_DP_ENCODER1_P0_FLDMASK);
	msWrite2ByteMask(mtk_dp, (REG_3004_DP_ENCODER0_P0 + reg_offset_enc),
				(0x1 << SDP_RESET_SW_DP_ENCODER0_P0_FLDMASK_POS),
				SDP_RESET_SW_DP_ENCODER0_P0_FLDMASK);
	udelay(5);
	msWriteByteMask(mtk_dp, (REG_331C_DP_ENCODER1_P0 + reg_offset_enc),
				0x0, TDM_AUDIO_RST_DP_ENCODER1_P0_FLDMASK);
	msWrite2ByteMask(mtk_dp, (REG_3004_DP_ENCODER0_P0 + reg_offset_enc),
				(0x0 << SDP_RESET_SW_DP_ENCODER0_P0_FLDMASK_POS),
				SDP_RESET_SW_DP_ENCODER0_P0_FLDMASK);
	//audio channel count change reset
	msWriteByteMask(mtk_dp, (REG_33F4_DP_ENCODER1_P0 + 1 + reg_offset_enc), 0, BIT(1));
}

void mtk_dptx_hal_audio_ch_status_set(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
				      u8 channel,
				      u8 fs,
				      u8 wordlength)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);
	union DPRX_AUDIO_CHSTS AudChSts;

	memset(&AudChSts, 0, sizeof(AudChSts));

	switch (fs) {
	case FS_32K:
		AudChSts.iec_ch_sts.SamplingFreq = 3;
		break;

	case FS_44K:
		AudChSts.iec_ch_sts.SamplingFreq = 0;
		break;

	case FS_48K:
		AudChSts.iec_ch_sts.SamplingFreq = 2;
		break;

	case FS_88K:
		AudChSts.iec_ch_sts.SamplingFreq = 8;
		break;

	case FS_96K:
		AudChSts.iec_ch_sts.SamplingFreq = 0xA;
		break;

	case FS_192K:
		AudChSts.iec_ch_sts.SamplingFreq = 0xE;
		break;

	default:
		AudChSts.iec_ch_sts.SamplingFreq = 0x1;
		break;
	}

	switch (wordlength) {
	case WL_16bit:
		AudChSts.iec_ch_sts.WordLen = 0b0010; //0x02;
		break;

	case WL_20bit:
		AudChSts.iec_ch_sts.WordLen = 0b0011; //0x03;
		break;

	case WL_24bit:
		AudChSts.iec_ch_sts.WordLen = 0b1011; //0x0B;
		break;
	}

	msWrite2Byte(mtk_dp, REG_308C_DP_ENCODER0_P0 + reg_offset_enc,
			 ((AudChSts.AUD_CH_STS[1] << 8) | AudChSts.AUD_CH_STS[0]));
	msWrite2Byte(mtk_dp, REG_3090_DP_ENCODER0_P0 + reg_offset_enc,
			 ((AudChSts.AUD_CH_STS[3] << 8) | AudChSts.AUD_CH_STS[2]));
	msWriteByte(mtk_dp, REG_3094_DP_ENCODER0_P0 + reg_offset_enc, AudChSts.AUD_CH_STS[4]);
}

void mtk_dptx_hal_audio_sdp_setting(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 channel)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

	//[7 : 0] //HB2
	msWriteByteMask(mtk_dp, REG_312C_DP_ENCODER0_P0 + reg_offset_enc, 0x00, 0xFF);

	if (channel == 8)
		//[15 : 8]channel-1
		msWrite2ByteMask(mtk_dp, REG_312C_DP_ENCODER0_P0 + reg_offset_enc, 0x0700, 0xFF00);
	else
		msWrite2ByteMask(mtk_dp, REG_312C_DP_ENCODER0_P0 + reg_offset_enc, 0x0100, 0xFF00);
}

void mtk_dptx_hal_audio_set_mdiv(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 div)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

#ifdef DPTX_FPGA
	div = DPTX_AUDIO_M_DIV_D4;
#endif
	msWrite2ByteMask(mtk_dp, REG_30BC_DP_ENCODER0_P0 + reg_offset_enc,
			 (div << AUDIO_M_CODE_MULT_DIV_SEL_DP_ENCODER0_P0_FLDMASK_POS),
			 AUDIO_M_CODE_MULT_DIV_SEL_DP_ENCODER0_P0_FLDMASK);
}

static void mtk_dptx_hal_encoder_reset(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

	// dp tx encoder reset all sw
	msWrite2ByteMask(mtk_dp, (REG_3004_DP_ENCODER0_P0 + reg_offset_enc),
			1 << DP_TX_ENCODER_4P_RESET_SW_DP_ENCODER0_P0_FLDMASK_POS,
			DP_TX_ENCODER_4P_RESET_SW_DP_ENCODER0_P0_FLDMASK);
	DPTXMSG("Reset DPTX encoder all (delay)\n");

	// dp tx encoder reset all sw
	msWrite2ByteMask(mtk_dp, (REG_3004_DP_ENCODER0_P0 + reg_offset_enc),
			0,
			DP_TX_ENCODER_4P_RESET_SW_DP_ENCODER0_P0_FLDMASK);
}

void mtk_dptx_hal_encoder_reset_all(struct mtk_dp *mtk_dp)
{
	enum DPTX_ENCODER_ID encoder_id;

	for (encoder_id = 0; encoder_id < DPTX_ENCODER_ID_MAX; encoder_id++)
		mtk_dptx_hal_encoder_reset(mtk_dp, encoder_id);
}

void mtk_dptx_hal_dsc_enable(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 enable)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

	DPTXFUNC();

	mtk_dp->dsc_enable = enable;

	if (enable) {
		// [0] : DSC Enable
		msWriteByteMask(mtk_dp, REG_336C_DP_ENCODER1_P0 + reg_offset_enc, BIT(0), BIT(0));

		//300C [9] : VB-ID[6] DSC enable
		msWriteByteMask(mtk_dp, REG_300C_DP_ENCODER0_P0 + 1 + reg_offset_enc, BIT(1), BIT(1));

		//303C[10 : 8] : DSC color depth
		msWriteByteMask(mtk_dp, (REG_303C_DP_ENCODER0_P0 + 1 + reg_offset_enc),
							0x7, MASKBIT(2 : 0));

		//303C[14 : 12] : DSC color format
		msWriteByteMask(mtk_dp, (REG_303C_DP_ENCODER0_P0 + 1 + reg_offset_enc),
							(0x7 << 4), MASKBIT(6 : 4));

		//31FC[12] : HDE last num control
		msWriteByteMask(mtk_dp, (REG_31FC_DP_ENCODER0_P0 + 1 + reg_offset_enc), BIT(4), BIT(4));
	} else {
		// DSC Disable
		msWriteByteMask(mtk_dp, REG_336C_DP_ENCODER1_P0 + reg_offset_enc, 0, BIT(0));

		msWriteByteMask(mtk_dp, (REG_300C_DP_ENCODER0_P0 + 1 + reg_offset_enc), 0, BIT(1));

		//default 8bit
		msWriteByteMask(mtk_dp, (REG_303C_DP_ENCODER0_P0 + 1 + reg_offset_enc),
							0x3, MASKBIT(2 : 0));

		//default RGB
		msWriteByteMask(mtk_dp, REG_303C_DP_ENCODER0_P0 + 1 + reg_offset_enc,
							0x0, MASKBIT(6 : 4));
		msWriteByteMask(mtk_dp, (REG_31FC_DP_ENCODER0_P0 + 1 + reg_offset_enc), 0x0, BIT(4));
	}
}

void mtk_dptx_hal_set_chunk_size(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
				 u8  slice_num,
				 u16 chunk_num,
				 u8  remainder,
				 u8  lane_count,
				 u8  hde_last_num,
				 u8  hde_num_even)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

	msWriteByteMask(mtk_dp, REG_336C_DP_ENCODER1_P0 + reg_offset_enc,
						slice_num << 4, MASKBIT(7 : 4));
	msWriteByteMask(mtk_dp, (REG_336C_DP_ENCODER1_P0 + 1 + reg_offset_enc),
						remainder, MASKBIT(3 : 0));
	msWrite2Byte(mtk_dp, REG_3370_DP_ENCODER1_P0 + reg_offset_enc, chunk_num);//set chunk_num

	if (lane_count == 1) {
		//last data catch on lane 0
		msWriteByteMask(mtk_dp, REG_31FC_DP_ENCODER0_P0 + reg_offset_enc,
						hde_last_num, MASKBIT(1 : 0));

		//sram last data catch on lane 0
		msWriteByteMask(mtk_dp, (REG_31FC_DP_ENCODER0_P0 + 1 + reg_offset_enc),
						hde_num_even, BIT(0));
	} else {
		msWriteByteMask(mtk_dp, REG_31FC_DP_ENCODER0_P0 + reg_offset_enc,
						hde_last_num, MASKBIT(1 : 0));
		msWriteByteMask(mtk_dp, REG_31FC_DP_ENCODER0_P0 + reg_offset_enc,
						(hde_last_num << 2), MASKBIT(3 : 2));
		msWriteByteMask(mtk_dp, (REG_31FC_DP_ENCODER0_P0 + 1 + reg_offset_enc),
						hde_num_even, BIT(0));
		msWriteByteMask(mtk_dp, (REG_31FC_DP_ENCODER0_P0 + 1 + reg_offset_enc),
						hde_num_even << 1, BIT(1));
	}
}

void mtk_dptx_hal_video_mute(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, const u8 enable)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

	DPTXFUNC("Encoder %d, video mute = %d\n", encoder_id, enable);

	if (enable) {
		msWriteByteMask(mtk_dp,
			REG_3000_DP_ENCODER0_P0 + reg_offset_enc,
			BIT(3) | BIT(2),
			BIT(3) | BIT(2));
		//Video mute enable
		msWriteByteMask(mtk_dp, REG_304C_DP_ENCODER0_P0 + reg_offset_enc, BIT(2), BIT(2));
		//HW workaround, switch video path to idle (DSC bypass is NULL for Source)
		msWrite2ByteMask(mtk_dp, REG_31C4_DP_ENCODER0_P0 + reg_offset_enc,
		1 << DSC_BYPASS_EN_DP_ENCODER0_P0_FLDMASK_POS,
		DSC_BYPASS_EN_DP_ENCODER0_P0_FLDMASK);
	} else {
		msWriteByteMask(mtk_dp,
			REG_3000_DP_ENCODER0_P0 + reg_offset_enc,
			BIT(3),
			BIT(3) | BIT(2));
		// [3] Sw ov Mode [2] mute value
		msWriteByteMask(mtk_dp, REG_304C_DP_ENCODER0_P0 + reg_offset_enc, 0, BIT(2));
		msWrite2ByteMask(mtk_dp, REG_31C4_DP_ENCODER0_P0 + reg_offset_enc, 0,
		DSC_BYPASS_EN_DP_ENCODER0_P0_FLDMASK);
	}
}

void mtk_dptx_hal_audio_mute(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id, u8 enable)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

	if (enable) {
		msWrite2ByteMask(mtk_dp, REG_3030_DP_ENCODER0_P0 + reg_offset_enc,
				 VBID_AUDIO_MUTE_FLAG_SW_DP_ENCODER0_P0_FLDMASK,
				 VBID_AUDIO_MUTE_FLAG_SW_DP_ENCODER0_P0_FLDMASK);

		msWrite2ByteMask(mtk_dp, REG_3030_DP_ENCODER0_P0 + reg_offset_enc,
				 VBID_AUDIO_MUTE_FLAG_SEL_DP_ENCODER0_P0_FLDMASK,
				 VBID_AUDIO_MUTE_FLAG_SEL_DP_ENCODER0_P0_FLDMASK);

		msWriteByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0 + reg_offset_enc,
				 0x0, AU_EN_DP_ENCODER0_P0_FLDMASK);
		msWriteByte(mtk_dp, REG_30A4_DP_ENCODER0_P0 + reg_offset_enc, 0x00);

		//a fifo reset
		msWrite2ByteMask(mtk_dp, REG_33F4_DP_ENCODER1_P0 + reg_offset_enc, BIT(9), BIT(9));
		msWrite2ByteMask(mtk_dp, REG_33F4_DP_ENCODER1_P0 + reg_offset_enc, 0x0, BIT(9));
	} else {
		msWrite2ByteMask(mtk_dp, REG_3030_DP_ENCODER0_P0 + reg_offset_enc, (0x00),
				 VBID_AUDIO_MUTE_FLAG_SEL_DP_ENCODER0_P0_FLDMASK);

		msWriteByteMask(mtk_dp, REG_3088_DP_ENCODER0_P0 + reg_offset_enc,
				AU_EN_DP_ENCODER0_P0_FLDMASK,
				AU_EN_DP_ENCODER0_P0_FLDMASK);

		msWriteByte(mtk_dp, REG_30A4_DP_ENCODER0_P0 + reg_offset_enc, 0x0F);
	}
}
#endif

/**
 * mtk_dptx_mst_hal_enc_enable() - enable encoder MST mode
 * @enable: set TRUE if want to enable MST mode
 */
static void mtk_dptx_mst_hal_enc_enable(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
									const u8 enable)
{
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

	/* encoder to MST Tx must set 4lane, set 2 */
	if (enable)
		msWriteByteMask(mtk_dp, (REG_3000_DP_ENCODER0_P0 + reg_offset_enc),
			    2 << LANE_NUM_DP_ENCODER0_P0_FLDMASK_POS,
			    LANE_NUM_DP_ENCODER0_P0_FLDMASK);

	msWriteByteMask(mtk_dp, (REG_3308_DP_ENCODER1_P0 + reg_offset_enc),
		((enable > 0) ? 0x01 : 0x00) << DP_MST_EN_DP_ENCODER1_P0_FLDMASK_POS,
		DP_MST_EN_DP_ENCODER1_P0_FLDMASK);

	/* mixer_sdp & enhanced_frame should be disable in MST mode*/
	msWrite4ByteMask(mtk_dp, (REG_3030_DP_ENCODER0_P0 + reg_offset_enc),
		((enable == 0) ? 0x01 : 0x00) << MIXER_SDP_EN_DP_ENCODER0_P0_FLDMASK_POS,
		MIXER_SDP_EN_DP_ENCODER0_P0_FLDMASK);
	msWriteByteMask(mtk_dp, (REG_3000_DP_ENCODER0_P0 + reg_offset_enc),
		((enable == 0) ? 0x01 : 0x00) << ENHANCED_FRAME_EN_DP_ENCODER0_P0_FLDMASK_POS,
		ENHANCED_FRAME_EN_DP_ENCODER0_P0_FLDMASK);
}

void mtk_dptx_mst_hal_mst_enable(struct mtk_dp *mtk_dp, const u8 enable)
{
	msWriteByteMask(mtk_dp, REG_3808_DP_MST_DPTX,
		((enable > 0) ? 0x01 : 0x00) << MST_EN_TX_DP_MST_DPTX_FLDMASK_POS,
		MST_EN_TX_DP_MST_DPTX_FLDMASK);

	/* disable HDCP ECF bypass */
	msWriteByteMask(mtk_dp, REG_3888_DP_MST_DPTX,
		((enable == 0) ? 0x01 : 0x00),
		HDCP_ECF_BYPASS_DP_MST_DPTX_FLDMASK);

#if (DPTX_MST_HDCP_ENABLE == 0x1)
	msWriteByteMask(mtk_dp, REG_3980_DP_MST_DPTX,
		((enable > 0) ? 0x01 : 0x00) << ENCRYPTION_EN_MST_TX_DP_MST_DPTX_FLDMASK_POS,
		ENCRYPTION_EN_MST_TX_DP_MST_DPTX_FLDMASK); // Enable HDCP encryption
#endif
}

static void mtk_dptx_mst_hal_trans_enable(struct mtk_dp *mtk_dp, const bool enable)
{
	u32 port_mux = enable ? 0x04 : 0x00;
	u32 value = 0x0;

	if ((mtk_dp->training_state == DPTX_NTSTATE_NORMAL) && (enable != 0x0))
		value = 0x1;

	/* enable need wait training lock otherwise TPS4 training fail */
	msWrite4ByteMask(mtk_dp, REG_3480_DP_TRANS_P0,
						value << MST_EN_DP_TRANS_P0_FLDMASK_POS,
						MST_EN_DP_TRANS_P0_FLDMASK);

	/* PRE_MISC_PORT_MUX: 4 = MST mode */
	msWrite4ByteMask(mtk_dp, REG_3400_DP_TRANS_P0,
					(0 << PRE_MISC_LANE0_MUX_DP_TRANS_P0_FLDMASK_POS) |
					(1 << PRE_MISC_LANE1_MUX_DP_TRANS_P0_FLDMASK_POS) |
					(2 << PRE_MISC_LANE2_MUX_DP_TRANS_P0_FLDMASK_POS) |
					(3 << PRE_MISC_LANE3_MUX_DP_TRANS_P0_FLDMASK_POS) |
					(port_mux << PRE_MISC_PORT_MUX_DP_TRANS_P0_FLDMASK_POS),
					PRE_MISC_LANE0_MUX_DP_TRANS_P0_FLDMASK |
					PRE_MISC_LANE1_MUX_DP_TRANS_P0_FLDMASK |
					PRE_MISC_LANE2_MUX_DP_TRANS_P0_FLDMASK |
					PRE_MISC_LANE3_MUX_DP_TRANS_P0_FLDMASK |
					PRE_MISC_PORT_MUX_DP_TRANS_P0_FLDMASK);
}

/** reduce drv & hal layer mst enable function
 * mtk_dptx_hal_mst_tx_enable() - set lane count & enable encoder + transmitter
 * @enable: set TRUE if want to enable MST function
 */
void mtk_dptx_mst_hal_tx_enable(struct mtk_dp *mtk_dp, const u8 enable)
{
	enum DPTX_ENCODER_ID encoder_id;

	DPTXMSG("Mst hal tx enable %d\n", enable);
	for (encoder_id = 0; encoder_id < DPTX_ENCODER_ID_MAX; encoder_id++)
		mtk_dptx_mst_hal_enc_enable(mtk_dp, encoder_id, enable);

	mtk_dptx_mst_hal_mst_enable(mtk_dp, enable);
	mtk_dptx_mst_hal_trans_enable(mtk_dp, enable);
}

/**
 * mtk_dptx_hal_mst_tx_init() - initialize MST HW function
 */
void mtk_dptx_mst_hal_tx_init(struct mtk_dp *mtk_dp)
{
	u32 reg_offset_enc;
	enum DPTX_ENCODER_ID encoder_id = DPTX_ENCODER_ID_MAX;

	for (encoder_id = 0; encoder_id < DPTX_ENCODER_ID_MAX; encoder_id++) {
		reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

		msWriteByteMask(mtk_dp, (REG_3308_DP_ENCODER1_P0 + reg_offset_enc),
						(1 << MST_RESET_SW_DP_ENCODER1_P0_FLDMASK_POS) |
						(1 << MST_SDP_FIFO_RST_DP_ENCODER1_P0_FLDMASK_POS),
						(MST_RESET_SW_DP_ENCODER1_P0_FLDMASK) |
						(MST_SDP_FIFO_RST_DP_ENCODER1_P0_FLDMASK));
		udelay(20);

		msWriteByteMask(mtk_dp, (REG_3308_DP_ENCODER1_P0 + reg_offset_enc), 0x0,
						(MST_RESET_SW_DP_ENCODER1_P0_FLDMASK) |
						(MST_SDP_FIFO_RST_DP_ENCODER1_P0_FLDMASK));
	}

	// MST_SW_RESET:
	// [0] rx_data_shift,	[1] rx_lane_cnt_adjust,	[2] rx_vcp_allocate,
	// [3] rx_vcp__table,	[4] fifo,		[5] tx_merge,
	// [6]tx_payload_mux,	[7] tx_rate_governer,	[8] tx_vc_payload_id_sync,
	// [9] tx_vc_payload_update,			[15] reset all
	msWrite2ByteMask(mtk_dp, REG_3800_DP_MST_DPTX, BIT(15), BIT(15));
	msWrite2ByteMask(mtk_dp, REG_3894_DP_MST_DPTX,
		(1 << RST_MST_FIFO_WPTR_DP_MST_DPTX_FLDMASK_POS) |
		(1 << RST_MST_FIFO_RPTR_DP_MST_DPTX_FLDMASK_POS),
		RST_MST_FIFO_WPTR_DP_MST_DPTX_FLDMASK |
		RST_MST_FIFO_RPTR_DP_MST_DPTX_FLDMASK);
	mdelay(1);
	msWrite2ByteMask(mtk_dp, REG_3800_DP_MST_DPTX, 0x0, BIT(15));
	msWrite2ByteMask(mtk_dp, REG_3894_DP_MST_DPTX, 0x0,
		RST_MST_FIFO_WPTR_DP_MST_DPTX_FLDMASK |
		RST_MST_FIFO_RPTR_DP_MST_DPTX_FLDMASK);
	/* reg_mst_source_sel
	 * 3'd0 : MST original
	 * 3'd1 : encode 0
	 * 3'd2 : encode 1
	 * 3'd3 : encode 2
	 * 3'd4 : encode 3
	 */
	msWrite2Byte(mtk_dp, REG_3898_DP_MST_DPTX, 0x4321);

	if (encoder_id >= 4)
		DPTXERR("Should add reg_mst_source_sel configurations\n");

	/* early enable mst when we know the primary branch support MST*/
	mtk_dptx_mst_hal_mst_enable(mtk_dp, TRUE);

	for (encoder_id = 0; encoder_id < DPTX_ENCODER_ID_MAX; encoder_id++) {
		reg_offset_enc = DP_REG_OFFSETA00(encoder_id);
		//[CH]: workaround for audio pkt lost from audio engine(I2S)
		msWrite2ByteMask(mtk_dp, REG_330C_DP_ENCODER1_P0 + reg_offset_enc,
				(0x09 << SDP_MST_INSERT_CNT_DP_ENCODER1_P0_FLDMASK_POS) |
				(0x01 << MST_MTP_CNT_VIDEO_MASK_DP_ENCODER1_P0_FLDMASK_POS),
				SDP_MST_INSERT_CNT_DP_ENCODER1_P0_FLDMASK |
				MST_MTP_CNT_VIDEO_MASK_DP_ENCODER1_P0_FLDMASK);
	}
	// top gp bank wait for coda TBD
	//msWriteByteMask(mtk_dp, REG_01B0_DP_TX_TOP_GP, 0x01, RTX_CLK_SEL_P0);
	//msWriteByteMask(mtk_dp, REG_0030_DP_TX_TOP_GP, 0x07, TX_PIX_CLK_SEL_0);
	//msWriteByteMask(mtk_dp, REG_0054_DP_TX_TOP_GP, 0x07, TX_MAINLINK_CLK_SEL_0);
}


void mtk_dptx_mst_hal_mst_config(struct mtk_dp *mtk_dp)
{
	/* This function is executed after link training done */
	enum DPTX_LANE_COUNT lane_count = mtk_dp->training_info.ubLinkLaneCount;

	if (mtk_dp->training_state != DPTX_NTSTATE_NORMAL) {
		DPTXERR("Un-expected training state %d while setting mst lane count\n",
				mtk_dp->training_state);
	}

	/* 0: 1lane,  1: 2lane,  2: 4lane */
	msWriteByteMask(mtk_dp, REG_3808_DP_MST_DPTX,
				(lane_count >> 1) << LANE_NUM_TX_DP_MST_DPTX_FLDMASK_POS,
				LANE_NUM_TX_DP_MST_DPTX_FLDMASK);
#if (DPTX_MST_HDCP_ENABLE == 0x1)
	msWrite4ByteMask(mtk_dp, REG_3884_DP_MST_DPTX, 0x0,
				HDCP_ECF_FIFO_OV_DP_MST_DPTX_FLDMASK); // Disable HDCP bypass
#endif
}

/**
 * mtk_dptx_hal_mst_set_mtp_size() - set slots number according to lane count
 * @num_slots: target slots number
 */
void mtk_dptx_mst_hal_set_mtp_size(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
					const u8 num_slots)
{
	u16 num_slot_enc;
	u32 reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

	switch (mtk_dp->training_info.ubLinkLaneCount) {
	case DP_LANECOUNT_4:
		num_slot_enc = num_slots;
		break;
	case DP_LANECOUNT_2:
		num_slot_enc = num_slots >> 1;
		break;
	case DP_LANECOUNT_1:
		num_slot_enc = num_slots >> 2;
		break;
	default:
		num_slot_enc = num_slots;
		DPTXERR("Unknown lane count %d\n", mtk_dp->training_info.ubLinkLaneCount);
		break;
	}

	/* update slots number */
	msWrite2ByteMask(mtk_dp, (REG_3310_DP_ENCODER1_P0 + reg_offset_enc),
				num_slot_enc << MST_TIME_SLOT_DP_ENCODER1_P0_FLDMASK_POS,
				MST_TIME_SLOT_DP_ENCODER1_P0_FLDMASK);
}


/**
 * mtk_dptx_hal_mst_set_vcp_timeslot() - set vcp id of each slots
 * @start_slot: start of slot, for example 5
 * @end_slot: end of slot, for example 20
 * @vcpi: vc payload id, for example 1
 *
 * Means from MTP slots 5 ~ 20 will set vcpi = 1
 * Even slots use LSB, Odd slots use MSB
 */
void mtk_dptx_mst_hal_set_vcpi(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
				const u16 start_slot, const u16 end_slot, const u32 vcpi)
{
	u32 slot, addr;
#if (DPTX_MST_HDCP_ENABLE == 0x1)
	u32 hdcp_bitmap_upper = 0x0;
	u32 hdcp_bitmap_lower = 0x0;
#endif


	if (vcpi > DPTX_STREAM_MAX)
		DPTXERR("Un-expected vcpi%d", vcpi);

	/* update corresponding vcpi */
	/*0x38A8*/
	/*     slot0           slot1  */
	/*bit0___bit6 bit8___bit14*/
	/*0x38AC*/
	/*     slot2           slot3  */
	/*bit0___bit6 bit8___bit14*/
	for (slot = start_slot; slot < end_slot; slot++) {
		addr = REG_38A8_DP_MST_DPTX + ((slot >> 1) << 2);
		if (slot % 2)	/* odd slots */
			msWrite2ByteMask(mtk_dp, addr,
					vcpi << VC_PAYLOAD_TIMESLOT_1_DP_MST_DPTX_FLDMASK_POS,
					VC_PAYLOAD_TIMESLOT_1_DP_MST_DPTX_FLDMASK);
		else		/* even slots */
			msWrite2ByteMask(mtk_dp, addr,
					vcpi << VC_PAYLOAD_TIMESLOT_0_DP_MST_DPTX_FLDMASK_POS,
					VC_PAYLOAD_TIMESLOT_0_DP_MST_DPTX_FLDMASK);
	}

#if (DPTX_MST_HDCP_ENABLE == 0x1)
	if (start_slot < 32) {
		hdcp_bitmap_lower = ((0xFFFFFFFF >> start_slot) << start_slot);
		hdcp_bitmap_upper = 0xFFFFFFFF;
	} else {
		hdcp_bitmap_lower = 0x0;
		hdcp_bitmap_upper = ((0xFFFFFFFF >> start_slot) << start_slot);
	}

	if (end_slot < 32) {
		hdcp_bitmap_lower = ((hdcp_bitmap_lower << (31-end_slot) >> (31-end_slot)));
		hdcp_bitmap_upper = 0x0;
	} else {
		hdcp_bitmap_upper = ((hdcp_bitmap_upper << (31-end_slot) >> (31-end_slot)));
	}

	msWrite2ByteMask(mtk_dp, REG_3984_DP_MST_DPTX, hdcp_bitmap_lower,
		hdcp_bitmap_lower & HDCP_TIMESLOT_MST_TX_0_DP_MST_DPTX_FLDMASK);
	msWrite2ByteMask(mtk_dp, REG_3988_DP_MST_DPTX, hdcp_bitmap_lower >> 16,
		(hdcp_bitmap_lower >> 16) & HDCP_TIMESLOT_MST_TX_1_DP_MST_DPTX_FLDMASK);
	msWrite2ByteMask(mtk_dp, REG_398C_DP_MST_DPTX, hdcp_bitmap_upper,
		hdcp_bitmap_upper & HDCP_TIMESLOT_MST_TX_2_DP_MST_DPTX_FLDMASK);
	msWrite2ByteMask(mtk_dp, REG_3990_DP_MST_DPTX, hdcp_bitmap_upper >> 16,
		(hdcp_bitmap_upper >> 16) & HDCP_TIMESLOT_MST_TX_3_DP_MST_DPTX_FLDMASK);

	/*reg_trig_hdcp_timeslot, WO*/
	msWriteByteMask(mtk_dp, REG_3984_DP_MST_DPTX, 0, BIT(0));
	msWrite2ByteMask(mtk_dp, REG_3980_DP_MST_DPTX,
					1 << TRIG_HDCP_TIMESLOT_MST_TX_DP_MST_DPTX_FLDMASK_POS,
					TRIG_HDCP_TIMESLOT_MST_TX_DP_MST_DPTX_FLDMASK);
#endif
}

void mtk_dptx_mst_hal_set_id_buf(struct mtk_dp *mtk_dp, const enum DPTX_ENCODER_ID encoder_id,
				const UINT32 vcpi)
{
	/* overwrite ID buffer define */
	if (encoder_id % 2) /* encoder 1/3/... */
		msWrite2ByteMask(mtk_dp, (REG_3854_DP_MST_DPTX + ((encoder_id >> 1) << 2)),
					vcpi << ID_BUF_2_OV_DP_MST_DPTX_FLDMASK_POS,
					ID_BUF_2_OV_DP_MST_DPTX_FLDMASK);
	else			/* encoder 0/2... */
		msWrite2ByteMask(mtk_dp, (REG_3854_DP_MST_DPTX + ((encoder_id >> 1) << 2)),
					vcpi << ID_BUF_1_OV_DP_MST_DPTX_FLDMASK_POS,
					ID_BUF_1_OV_DP_MST_DPTX_FLDMASK);
}

static void mtk_dptx_mst_hal_clear_id_buf(struct mtk_dp *mtk_dp)
{
	msWrite2Byte(mtk_dp, REG_3854_DP_MST_DPTX, 0x0);
	msWrite2Byte(mtk_dp, REG_3858_DP_MST_DPTX, 0x0);
	msWrite2Byte(mtk_dp, REG_385C_DP_MST_DPTX, 0x0);
	msWrite2Byte(mtk_dp, REG_3860_DP_MST_DPTX, 0x0);
}

/**
 * mtk_dptx_hal_mst_reset_payload() - reset vcpi = 0 for each slots
 */
void mtk_dptx_mst_hal_reset_payload(struct mtk_dp *mtk_dp)
{
	u32 encoder_id, i;
	u32 reg_offset_enc;

	mtk_dptx_mst_hal_clear_id_buf(mtk_dp);

	for (encoder_id = 0; encoder_id < DPTX_ENCODER_ID_MAX; encoder_id++) {
		reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

		/* update slots number */
		msWrite2ByteMask(mtk_dp, (REG_3310_DP_ENCODER1_P0 + reg_offset_enc),
					0x00 << MST_TIME_SLOT_DP_ENCODER1_P0_FLDMASK_POS,
					MST_TIME_SLOT_DP_ENCODER1_P0_FLDMASK);
	}

	/* update corresponding vcpi */
	for (i = 0; i < 32; i++)
		msWrite2ByteMask(mtk_dp, (REG_38A8_DP_MST_DPTX + (i << 2)), 0x00,
						VC_PAYLOAD_TIMESLOT_0_DP_MST_DPTX_FLDMASK |
						VC_PAYLOAD_TIMESLOT_1_DP_MST_DPTX_FLDMASK);

#if (DPTX_MST_HDCP_ENABLE == 0x1)
	for (i = 0; i < 4; i++)
		msWrite2ByteMask(mtk_dp, REG_3984_DP_MST_DPTX  + (i << 2),
					0x0, HDCP_TIMESLOT_MST_TX_0_DP_MST_DPTX_FLDMASK);
#endif
}

/**
 * mtk_dptx_hal_mst_vcp_table_update() - trigger vcpi update
 */
void mtk_dptx_mst_hal_vcp_table_update(struct mtk_dp *mtk_dp)
{
	/* update TX ID buffer */
	msWrite2ByteMask(mtk_dp, REG_3868_DP_MST_DPTX,
				1 << UPDATE_ID_BUF_TX_TRIG_DP_MST_DPTX_FLDMASK_POS,
				UPDATE_ID_BUF_TX_TRIG_DP_MST_DPTX_FLDMASK);

	DPTXMSG("Update VC payload id table\n");

	/* trigger to update VC payload table */
	msWriteByteMask(mtk_dp, REG_3980_DP_MST_DPTX,
				1 << VC_PAYLOAD_TABLE_TX_UPDATE_DP_MST_DPTX_FLDMASK_POS,
				VC_PAYLOAD_TABLE_TX_UPDATE_DP_MST_DPTX_FLDMASK);
}


/**
 * mtk_dptx_hal_mst_stream_enable() - mapping stream to encoder and start output
 * @vcpi_mask: vc payload id bitwise mask (1b'1 means used 1b'0 means unused)
 * @max_payloads: maximum number of stream payloads
 *
 * Default mapping encoder N to stream N
 */
void mtk_dptx_mst_hal_stream_enable(struct mtk_dp *mtk_dp, const u8 vcpi_mask, const u32 max_payloads)
{
	u32 reg_offset_enc, encoder_id;
	u32 mst_stream_en_mask;
	u32 mst_stream_en_shift;
	u32 stream_count = 0;

	mst_stream_en_shift = MIN((u32) 8, (u32) max_payloads);
	mst_stream_en_mask = (1 << mst_stream_en_shift) - 1;

	for (encoder_id = 0; encoder_id < DPTX_STREAM_MAX; encoder_id++) {
		reg_offset_enc = DP_REG_OFFSETA00(encoder_id);
		if ((vcpi_mask >> encoder_id) & 0x1) {
			/*reg_dp_mst_en*/
			msWriteByteMask(mtk_dp, (REG_3308_DP_ENCODER1_P0 + reg_offset_enc),
							1 << DP_MST_EN_DP_ENCODER1_P0_FLDMASK_POS,
							DP_MST_EN_DP_ENCODER1_P0_FLDMASK);
			stream_count++;
		}
	}

	/* disable remaining encoders */
	for (encoder_id = stream_count; encoder_id < DPTX_ENCODER_ID_MAX; encoder_id++) {
		reg_offset_enc = DP_REG_OFFSETA00(encoder_id);

		msWriteByteMask(mtk_dp, (REG_3308_DP_ENCODER1_P0 + reg_offset_enc),
					0x0,
					DP_MST_EN_DP_ENCODER1_P0_FLDMASK);
	}

	msWriteByteMask(mtk_dp, REG_3930_DP_MST_DPTX, ((0x1 << stream_count) - 1), mst_stream_en_mask);

	DPTXMSG("Configure reg_dp_mst_en %x\n", vcpi_mask);
}

/**
 * mtk_dptx_hal_mst_trigger_act() - trigger ACT update event
 */
void mtk_dptx_mst_hal_trigger_act(struct mtk_dp *mtk_dp)
{

	/* trigger VCPF */
	msWriteByteMask(mtk_dp, REG_3894_DP_MST_DPTX,
			(0x1 << RST_MST_FIFO_WPTR_DP_MST_DPTX_FLDMASK_POS) |
			(0x1 << RST_MST_FIFO_RPTR_DP_MST_DPTX_FLDMASK_POS),
			RST_MST_FIFO_WPTR_DP_MST_DPTX_FLDMASK |
			RST_MST_FIFO_RPTR_DP_MST_DPTX_FLDMASK);

	/* write one trigger */
	msWriteByteMask(mtk_dp, REG_3980_DP_MST_DPTX,
			0x1 << ACT_TRIGGER_MST_TX_DP_MST_DPTX_FLDMASK_POS,
			ACT_TRIGGER_MST_TX_DP_MST_DPTX_FLDMASK);
	udelay(10);

	/* trigger VCPF */
	msWriteByteMask(mtk_dp, REG_3894_DP_MST_DPTX, 0,
			RST_MST_FIFO_WPTR_DP_MST_DPTX_FLDMASK |
			RST_MST_FIFO_RPTR_DP_MST_DPTX_FLDMASK);
#if (DPTX_MST_HDCP_ENABLE == 0x1)
	msWriteByteMask(mtk_dp, REG_3980_DP_MST_DPTX,
			0x1 << TRIG_HDCP_TIMESLOT_MST_TX_DP_MST_DPTX_FLDMASK_POS,
			TRIG_HDCP_TIMESLOT_MST_TX_DP_MST_DPTX_FLDMASK);
#endif
}

