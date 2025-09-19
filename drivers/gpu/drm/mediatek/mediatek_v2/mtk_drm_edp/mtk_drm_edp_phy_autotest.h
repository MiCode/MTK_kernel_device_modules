/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019-2022 MediaTek Inc.
 * Copyright (c) 2022 BayLibre
 */

#ifndef _MTK_DRM_EDP_PHY_AUTOTEST_H_
#define _MTK_DRM_EDP_PHY_AUTOTEST_H_

#include <drm/display/drm_dp_helper.h>
#include <drm/display/drm_dp.h>
#include <drm/drm_edid.h>
#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/io.h>

#include "mtk_drm_edp_regs.h"

/* phy pattern */
#define PATTERN_NONE				0x0
#define PATTERN_D10_2				0x1
#define PATTERN_SYMBOL_ERR			0x2
#define PATTERN_PRBS7				0x3
#define PATTERN_80B					0x4
#define PATTERN_HBR2_COM_EYE		0x5
#define CP2520_PATTERN2				0x6
#define CP2520_PATTERN3				0x7

#define ENABLE_DPTX_EF_MODE		0x1
#if (ENABLE_DPTX_EF_MODE == 0x01)
#define DPTX_AUX_SET_ENAHNCED_FRAME	0x80
#else
#define DPTX_AUX_SET_ENAHNCED_FRAME	0x00
#endif
#define DPTX_TRAIN_RETRY_LIMIT		0x8
#define DPTX_TRAIN_MAX_ITERATION	0x5

#define MAX_LANECOUNT	DP_LANECOUNT_4

#define DPTX_TBC_SELBUF_CASE		2
#define DPTX_TBC_BUF_SIZE		DPTX_TBC_SELBUF_CASE
#if (DPTX_TBC_SELBUF_CASE == 2)
#define DPTX_TBC_BUF_ReadStartAdrThrd	0x08
#elif (DPTX_TBC_SELBUF_CASE == 1)
#define DPTX_TBC_BUF_ReadStartAdrThrd	0x10
#else
#define DPTX_TBC_BUF_ReadStartAdrThrd	0x1F
#endif

#define ENABLE_DPTX_FIX_LRLC		0

enum DPTX_LINK_RATE {
	DP_LINK_RATE_RBR = 0x6,
	DP_LINK_RATE_HBR = 0xA,
	DP_LINK_RATE_HBR2 = 0x14,
	DP_LINK_RATE_HBR3 = 0x1E,
};

enum DPTX_PG_TYPE {
	DPTX_PG_20BIT = 0,
	DPTX_PG_80BIT = 1,
	DPTX_PG_11BIT = 2,
	DPTX_PG_8BIT = 3,
	DPTX_PG_PRBS7 = 4,
};

enum DPTX_PG_TYPESEL {
	DPTX_PG_NONE			= 0x0,
	DPTX_PG_PURE_COLOR		= 0x1,
	DPTX_PG_VERTICAL_RAMPING	= 0x2,
	DPTX_PG_HORIZONTAL_RAMPING	= 0x3,
	DPTX_PG_VERTICAL_COLOR_BAR	= 0x4,
	DPTX_PG_HORIZONTAL_COLOR_BAR	= 0x5,
	DPTX_PG_CHESSBOARD_PATTERN	= 0x6,
	DPTX_PG_SUB_PIXEL_PATTERN	= 0x7,
	DPTX_PG_FRAME_PATTERN		= 0x8,
	DPTX_PG_MAX,
};

enum DPTX_PREEMPHASIS_LEVEL {
	DPTX_PREEMPHASIS0 = 0x00,
	DPTX_PREEMPHASIS1 = 0x01,
	DPTX_PREEMPHASIS2 = 0x02,
	DPTX_PREEMPHASIS3 = 0x03,
};

enum DPTX_TEST_COLOR_FORMAT {
	TEST_COLOR_FORMAT_RGB = 0,
	TEST_COLOR_FORMAT_YUV422 = 0x1,
	TEST_COLOR_FORMAT_YUV444 = 0x2,
	TEST_COLOR_FORMAT_RESERVED = 0x6,
};

enum DPTX_TEST_BIT_DEPTH {
	TEST_BIT_DEPTH_6 = 0,
	TEST_BIT_DEPTH_8 = 0x1,
	TEST_BIT_DEPTH_10 = 0x2,
	TEST_BIT_DEPTH_12 = 0x3,
	TEST_BIT_DEPTH_16 = 0x4,
	TEST_BIT_DEPTH_RESERVED,
};

#define BIT0 (1 << 0)
#define BIT1 (1 << 1)
#define BIT2 (1 << 2)
#define BIT3 (1 << 3)
#define BIT4 (1 << 4)
#define BIT5 (1 << 5)
#define BIT6 (1 << 6)
#define BIT7 (1 << 7)
#define BIT8 (1 << 8)
#define BIT9 (1 << 9)
#define BIT10 (1 << 10)
#define BIT11 (1 << 11)
#define BIT12 (1 << 12)
#define BIT13 (1 << 13)
#define BIT14 (1 << 14)
#define BIT15 (1 << 15)
#define BIT18 (1 << 18)
#define BIT19 (1 << 19)

enum DPTX_LT_PATTERN {
	DPTX_0 = 0,
	DPTX_TPS1 = BIT4,
	DPTX_TPS2 = BIT5,
	DPTX_TPS3 = BIT6,
	DPTX_TPS4 = BIT7,
	DPTx_20_TPS1 = BIT14,
	DPTx_20_TPS2 = BIT15,
};

struct dp_cts_auto_dptx_timing {
	u16 Htt;
	u16 Hde;
	u16 Hbk;
	u16 Hfp;
	u16 Hsw;

	bool bHsp;
	u16 Hbp;
	u16 Vtt;
	u16 Vde;
	u16 Vbk;
	u16 Vfp;
	u16 Vsw;

	bool bVsp;
	u16 Vbp;
	u8 FrameRate;
	u32 PixRateKhz;
	int Video_ip_mode;
};

union dptx_misc {
	struct {
		u8 is_sync_clock : 1;
		u8 color_format : 2;
		u8 spec_def1 : 2;
		u8 color_depth : 3;

		u8 interlaced : 1;
		u8 stereo_attr : 2;
		u8 reserved : 3;
		u8 is_vsc_sdp : 1;
		u8 spec_def2 : 1;

	} dp_misc;
	u8 byte[2];
};

struct dptx_timing_info {
	u8		video_ip_mode; // interlace or progressive
	u16		htt;	// H Totoal
	u16		hde;	// H de width
	u16		hbk;	// H Blanking => don't care
	u16		hfp;	// H front porch
	u16		hsw;	// H sync width
	u8		hsp;	// H sync polarity
	u16		hbp;	// H back porch
	u16		vtt;	// V de width
	u16		vde;	// V de width
	u16		vbk;	// V Blanking => don't care
	u16		vfp;	// V front porch
	u16		vsw;	// V sync width
	u8		vsp;	// V sync polarity
	u16		vbp;	// V back porch
	u8		frame_rate; // frame rate
	u32		pix_clk_khz;  // pixel rate in Khz // 27-> 2048 // 27Mhz > fill 27000
	union dptx_misc		misc;
};

struct dptx_video_info {
	u8 is_pattern_gen: 1;
	u8 fix_frame_rate: 1;
	u8 set_timing: 1;
	u8 set_audio_mute: 1;
	u8 set_video_mute: 1;
	u8 audio_mute: 1;
	u8 video_mute: 1;

	struct dptx_timing_info timing;

	u8  resolution;
	u32 audio_caps;
	u32 audio_config;

	u8  color_depth;
	u8  color_format;
	u8  pattern_id;
	u32 video_m;
	u32 video_n;
};

struct dp_training_info {
	bool sink_ext_cap_en : 1;
	bool tps3_support : 1;
	bool tps4_support : 1;
	bool sink_ssc_en : 1;
	bool cable_plug_in : 1;
	bool cable_state_change : 1;
	bool dp_mst_cap : 1;
	bool dp_mst_en : 1;
	bool dp_mst_branch : 1;
	bool dwn_strm_port_present : 1;
	bool cr_done : 1;
	bool eq_done : 1;

	u8 dp_version;
	u8 max_link_rate;
	u8 max_link_lane_count;
	u8 link_rate;
	u8 link_lane_count;
	u16 phy_status;
	u8 dpcd_rev;
	u8 sink_count;
	u8 check_cap_times;
	u8 ssc_delta;
	u8 channel_eq_pattern;
};

struct dp_cts_auto_req {
	u32 base;
	struct drm_dp_aux *aux;
	struct dp_training_info training_info;
	struct regmap *regs;
	struct regmap *phyd_regs;
	u8 has_fec:1;
	u8 swap_enable: 1;

	struct drm_connector *connector;
	struct edid *edid;
	u8 rx_cap[16];
	u8 input_src;
	bool is_edp;
	int training_state;
	bool bDPTxAutoTest_EN;
	bool bSinkEXTCAP_En;
	u8 ubLinkRate;
	u8 ubLinkLaneCount;
	u8 ubSysMaxLinkRate;
	u8 ubSinkCountNum;
	bool bSinkSSC_En;
	bool cr_done;
	bool eq_done;
	bool bTPS3;
	bool bTPS4;
	unsigned int test_link_training;
	unsigned int test_pattern_req;
	unsigned int test_edid_read;
	unsigned int test_link_rate;	//06h:1.62Gbps, 0Ah:2.7Gbps
	unsigned int test_lane_count;
	//01h:color ramps,02h:black&white vertical,03h:color square
	unsigned int test_pattern;
	unsigned int test_h_total;
	unsigned int test_v_total;
	unsigned int test_h_start;
	unsigned int test_v_start;
	unsigned int test_hsync_width;
	unsigned int test_hsync_polarity;
	unsigned int test_vsync_width;
	unsigned int test_vsync_polarity;
	unsigned int test_h_width;
	unsigned int test_v_height;
	unsigned int test_sync_clk;
	unsigned int test_color_fmt;
	unsigned int test_dynamic_range;
	unsigned int test_YCbCr_coefficient;
	unsigned int test_bit_depth;
	unsigned int test_refresh_denominator;
	unsigned int test_interlaced;
	unsigned int test_refresh_rate_numerator;
	unsigned int test_aduio_channel_count;
	unsigned int test_aduio_samling_rate;
	struct dp_cts_auto_dptx_timing dp_cts_outbl;
};

enum DPTx_Return_Status {
	DPTX_NOERR			= 0,
	DPTX_PLUG_OUT			= 1,
	DPTX_TIMEOUT			= 2,
	DPTX_AUTH_FAIL			= 3,
	DPTX_EDID_FAIL			= 4,
	DPTX_TRANING_FAIL		= 5,
	DPTX_TRANING_STATE_CHANGE	= 6,
};

enum DP_LANECOUNT {
	DP_LANECOUNT_1 = 0x1,
	DP_LANECOUNT_2 = 0x2,
	DP_LANECOUNT_4 = 0x4,
};

enum DPTX_LANE_NUM {
	DPTX_LANE0 = 0x0,
	DPTX_LANE1 = 0x1,
	DPTX_LANE2 = 0x2,
	DPTX_LANE3 = 0x3,
	DPTX_LANE_MAX,
};

enum DPTX_LANE_COUNT {
	DPTX_1LANE = 0x01,
	DPTX_2LANE = 0x02,
	DPTX_4LANE = 0x04,
};

enum DPTx_SOURCE_TYPE {
	DPTX_SRC_DPINTF = 0,
	DPTX_SRC_PG	= 1,
};

enum DPTX_VIDEO_MODE {
	DPTX_VIDEO_PROGRESSIVE  = 0,
	DPTX_VIDEO_INTERLACE    = 1,
};

#define  IEC_CH_STATUS_LEN 5
union DPRX_AUDIO_CHSTS {
	struct{
		u8 rev : 1;
		u8 ISLPCM : 1;
		u8 CopyRight : 1;
		u8 AdditionFormatInfo : 3;
		u8 ChannelStatusMode : 2;
		u8 CategoryCode;
		u8 SourceNumber : 4;
		u8 ChannelNumber : 4;
		u8 SamplingFreq : 4;
		u8 ClockAccuary : 2;
		u8 rev2 : 2;
		u8 WordLen : 4;
		u8 OriginalSamplingFreq : 4;
	} iec_ch_sts;

	u8 AUD_CH_STS[IEC_CH_STATUS_LEN];
};

enum DP_CTS_COLOR_DEPTH_TYPE {
	DP_CTS_COLOR_DEPTH_6BIT       = 0,
	DP_CTS_COLOR_DEPTH_8BIT       = 1,
	DP_CTS_COLOR_DEPTH_10BIT      = 2,
	DP_CTS_COLOR_DEPTH_12BIT      = 3,
	DP_CTS_COLOR_DEPTH_16BIT      = 4,
	DP_CTS_COLOR_DEPTH_UNKNOWN    = 5,
};

enum DP_CTS_COLOR_FORMAT_TYPE {
	DP_CTS_COLOR_FORMAT_RGB_444     = 0,
	DP_CTS_COLOR_FORMAT_YUV_422     = 1,
	DP_CTS_COLOR_FORMAT_YUV_444     = 2,
	DP_CTS_COLOR_FORMAT_YUV_420     = 3,
	DP_CTS_COLOR_FORMAT_YONLY       = 4,
	DP_CTS_COLOR_FORMAT_RAW         = 5,
	DP_CTS_COLOR_FORMAT_RESERVED    = 6,
	DP_CTS_COLOR_FORMAT_DEFAULT     = DP_CTS_COLOR_FORMAT_RGB_444,
	DP_CTS_COLOR_FORMAT_UNKNOWN     = 15,
};

enum DPTX_COLOR_FORMAT_TYPE {
	DPTX_COLOR_FORMAT_RGB_444     = 0,
	DPTX_COLOR_FORMAT_YUV_422     = 1,
	DPTX_COLOR_FORMAT_YUV_444     = 2,
	DPTX_COLOR_FORMAT_YUV_420     = 3,
	DPTX_COLOR_FORMAT_YONLY       = 4,
	DPTX_COLOR_FORMAT_RAW         = 5,
	DPTX_COLOR_FORMAT_RESERVED    = 6,
	DPTX_COLOR_FORMAT_DEFAULT     = DPTX_COLOR_FORMAT_RGB_444,
	DPTX_COLOR_FORMAT_UNKNOWN     = 15,
};

enum DPTX_COLOR_DEPTH_TYPE {
	DPTX_COLOR_DEPTH_6BIT       = 0,
	DPTX_COLOR_DEPTH_8BIT       = 1,
	DPTX_COLOR_DEPTH_10BIT      = 2,
	DPTX_COLOR_DEPTH_12BIT      = 3,
	DPTX_COLOR_DEPTH_16BIT      = 4,
	DPTX_COLOR_DEPTH_UNKNOWN    = 5,
};

enum DPTX_SWING_LEVEL {
	DPTX_SWING0 = 0x00,
	DPTX_SWING1 = 0x01,
	DPTX_SWING2 = 0x02,
	DPTX_SWING3 = 0x03,
};

enum {
	DPTX_LANE_COUNT1 = 0x1,
	DPTX_LANE_COUNT2 = 0x2,
	DPTX_LANE_COUNT4 = 0x4,
};

#define EDP_PHY_AUTOTEST_DEBUG		"[edp-phy-autotest]"
#define MTK_DP_TRAIN_VOLTAGE_LEVEL_RETRY 5
#define MTK_DP_TRAIN_DOWNSCALE_RETRY 10
#define EDP_VIDEO_UNMUTE		0x22
#define MTK_SIP_DP_CONTROL \
	(0x82000523 | 0x40000000)
#define MTK_DP_SIP_CONTROL_AARCH32	MTK_SIP_SMC_CMD(0x523)
#define MTK_DP_SIP_ATF_EDP_VIDEO_UNMUTE	(BIT(0) | BIT(5))
#define MTK_DP_SIP_ATF_VIDEO_UNMUTE	BIT(5)


#define DPCD_00000		0x00000
#define DPCD_00001		0x00001
#define DPCD_00002		0x00002
#define DPCD_00003		0x00003
#define DPCD_00004		0x00004
#define DPCD_00005		0x00005
#define DPCD_0000A		0x0000A
#define DPCD_0000E		0x0000E
#define DPCD_00021		0x00021
#define DPCD_00030		0x00030
#define DPCD_00060		0x00060
#define DPCD_00080		0x00080
#define DPCD_00090		0x00090
#define DPCD_00100		0x00100
#define DPCD_00101		0x00101
#define DPCD_00102		0x00102
#define DPCD_00103		0x00103
#define DPCD_00104		0x00104
#define DPCD_00105		0x00105
#define DPCD_00106		0x00106
#define DPCD_00107		0x00107
#define DPCD_00111		0x00111
#define DPCD_00120		0x00120
#define DPCD_00160		0x00160
#define DPCD_001A1		0x001A1
#define DPCD_001C0		0x001C0
#define DPCD_00200		0x00200
#define DPCD_00201		0x00201
#define	AUTOMATED_TEST_REQUEST			BIT(1)
#define DPCD_00202		0x00202
#define DPCD_00203		0x00203
#define DPCD_00204		0x00204
#define DPCD_00205		0x00205
#define DPCD_00206		0x00206
#define DPCD_00210		0x00210
#define DPCD_00218		0x00218
#define TEST_LINK_TRAINING				BIT(0)
#define TEST_VIDEO_PATTERN_REQUESTED	BIT(1)
#define TEST_EDID_READ					BIT(2)
#define LINK_TEST_PATTERN				BIT(3)
#define PHY_TEST_CHANNEL_CODING_TYPE	GENMASK(5, 4)
#define TEST_AUDIO_PATTERN_REQUESTED	BIT(6)
#define TEST_AUDIO_DISABLED_VIDEO		BIT(7)
#define TEST_REQUEST_MASK				GENMASK(7, 0)
#define DPCD_00219		0x00219
#define DPCD_00220		0x00220
#define DPCD_00230		0x00230
#define TEST_INTERLACED					BIT(1)
#define DPCD_00248		0x00248
#define LINK_QUAL_PATTERN_SELECT_MASK	GENMASK(6, 0)
#define DPCD_00250		0x00250
#define DPCD_00260		0x00260
#define DPCD_00261		0x00261
#define DPCD_00271		0x00271
#define DPCD_00280		0x00280
#define DPCD_00281		0x00281
#define DPCD_00282		0x00282
#define DPCD_002C0		0x002C0
#define DPCD_00600		0x00600
#define DPCD_01000		0x01000
#define DPCD_01200		0x01200
#define DPCD_01400		0x01400
#define DPCD_01600		0x01600
#define DPCD_02002		0x02002
#define DPCD_02003		0x02003
#define DPCD_0200C		0x0200C
#define DPCD_0200D		0x0200D
#define DPCD_0200E		0x0200E
#define DPCD_0200F		0x0200F
#define DPCD_02200		0x02200
#define DPCD_02201		0x02201
#define DPCD_02202		0x02202
#define DPCD_02203		0x02203
#define DPCD_02204		0x02204
#define DPCD_02205		0x02205
#define DPCD_02206		0x02206
#define DPCD_02207		0x02207
#define DPCD_02208		0x02208
#define DPCD_02209		0x02209
#define DPCD_0220A		0x0220A
#define DPCD_0220B		0x0220B
#define DPCD_0220C		0x0220C
#define DPCD_0220D		0x0220D
#define DPCD_0220E		0x0220E
#define DPCD_0220F		0x0220F
#define DPCD_02210		0x02210
#define DPCD_02211		0x02211
#define DPCD_68000		0x68000
#define DPCD_68005		0x68005
#define DPCD_68007		0x68007
#define DPCD_6800C		0x6800C
#define DPCD_68014		0x68014
#define DPCD_68018		0x68018
#define DPCD_6801C		0x6801C
#define DPCD_68020		0x68020
#define DPCD_68024		0x68024
#define DPCD_68028		0x68028
#define DPCD_68029		0x68029
#define DPCD_6802A		0x6802A
#define DPCD_6802C		0x6802C
#define DPCD_6803B		0x6803B
#define DPCD_6921D		0x6921D
#define DPCD_69000		0x69000
#define DPCD_69008		0x69008
#define DPCD_6900B		0x6900B
#define DPCD_69215		0x69215
#define DPCD_6921D		0x6921D
#define DPCD_69220		0x69220
#define DPCD_692A0		0x692A0
#define DPCD_692B0		0x692B0
#define DPCD_692C0		0x692C0
#define DPCD_692E0		0x692E0
#define DPCD_692F0		0x692F0
#define DPCD_692F8		0x692F8
#define DPCD_69318		0x69318
#define DPCD_69328		0x69328
#define DPCD_69330		0x69330
#define DPCD_69332		0x69332
#define DPCD_69335		0x69335
#define DPCD_69345		0x69345
#define DPCD_693E0		0x693E0
#define DPCD_693F0		0x693F0
#define DPCD_693F3		0x693F3
#define DPCD_693F5		0x693F5
#define DPCD_69473		0x69473
#define DPCD_69493		0x69493
#define DPCD_69494		0x69494
#define DPCD_69518		0x69518

/* eDP PHY REG */
#define PHYD_OFFSET			0x0000
#define PHYD_DIG_LAN0_OFFSET	0x1000
#define PHYD_DIG_LAN1_OFFSET	0x1100
#define PHYD_DIG_LAN2_OFFSET	0x1200
#define PHYD_DIG_LAN3_OFFSET	0x1300
#define PHYD_DIG_GLB_OFFSET	    0x1400


#define IPMUX_CONTROL			(PHYD_DIG_GLB_OFFSET + 0x98)
#define EDPTX_DSI_PHYD_SEL_FLDMASK		0x1
#define EDPTX_DSI_PHYD_SEL_FLDMASK_POS		0

#define DP_PHY_DIG_TX_CTL_0		(PHYD_DIG_GLB_OFFSET + 0x74)
#define TX_LN_EN_FLDMASK				0xf


#define MTK_DP_PHY_DIG_PLL_CTL_1	(PHYD_DIG_GLB_OFFSET + 0x14)
#define TPLL_SSC_EN			BIT(8)

#define MTK_DP_PHY_DIG_BIT_RATE		(PHYD_DIG_GLB_OFFSET + 0x3C)
#define BIT_RATE_RBR			0x1
#define BIT_RATE_HBR			0x4
#define BIT_RATE_HBR2			0x7
#define BIT_RATE_HBR3			0x9

#define MTK_DP_PHY_DIG_SW_RST		(PHYD_DIG_GLB_OFFSET + 0x38)
#define DP_GLB_SW_RST_PHYD		BIT(0)
#define DP_GLB_SW_RST_PHYD_MASK		BIT(0)

#define DRIVING_FORCE 0x30
#define EDP_TX_LN_VOLT_SWING_VAL_FLDMASK                                0x6
#define EDP_TX_LN_VOLT_SWING_VAL_FLDMASK_POS                            1
#define EDP_TX_LN_PRE_EMPH_VAL_FLDMASK                                  0x18
#define EDP_TX_LN_PRE_EMPH_VAL_FLDMASK_POS                              3



#define MTK_DP_LANE0_DRIVING_PARAM_3		(PHYD_OFFSET + 0x138)
#define MTK_DP_LANE1_DRIVING_PARAM_3		(PHYD_OFFSET + 0x238)
#define MTK_DP_LANE2_DRIVING_PARAM_3		(PHYD_OFFSET + 0x338)
#define MTK_DP_LANE3_DRIVING_PARAM_3		(PHYD_OFFSET + 0x438)
#define XTP_LN_TX_LCTXC0_SW0_PRE0_DEFAULT	BIT(4)
#define XTP_LN_TX_LCTXC0_SW0_PRE1_DEFAULT	(BIT(10) | BIT(12))
#define XTP_LN_TX_LCTXC0_SW0_PRE2_DEFAULT	GENMASK(20, 19)
#define XTP_LN_TX_LCTXC0_SW0_PRE3_DEFAULT	GENMASK(29, 29)
#define DRIVING_PARAM_3_DEFAULT	(XTP_LN_TX_LCTXC0_SW0_PRE0_DEFAULT | \
				 XTP_LN_TX_LCTXC0_SW0_PRE1_DEFAULT | \
				 XTP_LN_TX_LCTXC0_SW0_PRE2_DEFAULT | \
				 XTP_LN_TX_LCTXC0_SW0_PRE3_DEFAULT)

#define XTP_LN_TX_LCTXC0_SW1_PRE0_DEFAULT	GENMASK(4, 3)
#define XTP_LN_TX_LCTXC0_SW1_PRE1_DEFAULT	GENMASK(12, 9)
#define XTP_LN_TX_LCTXC0_SW1_PRE2_DEFAULT	(BIT(18) | BIT(21))
#define XTP_LN_TX_LCTXC0_SW2_PRE0_DEFAULT	GENMASK(29, 29)
#define DRIVING_PARAM_4_DEFAULT	(XTP_LN_TX_LCTXC0_SW1_PRE0_DEFAULT | \
				 XTP_LN_TX_LCTXC0_SW1_PRE1_DEFAULT | \
				 XTP_LN_TX_LCTXC0_SW1_PRE2_DEFAULT | \
				 XTP_LN_TX_LCTXC0_SW2_PRE0_DEFAULT)

#define XTP_LN_TX_LCTXC0_SW2_PRE1_DEFAULT	(BIT(3) | BIT(5))
#define XTP_LN_TX_LCTXC0_SW3_PRE0_DEFAULT	GENMASK(13, 12)
#define DRIVING_PARAM_5_DEFAULT	(XTP_LN_TX_LCTXC0_SW2_PRE1_DEFAULT | \
				 XTP_LN_TX_LCTXC0_SW3_PRE0_DEFAULT)

#define XTP_LN_TX_LCTXCP1_SW0_PRE0_DEFAULT	0
#define XTP_LN_TX_LCTXCP1_SW0_PRE1_DEFAULT	GENMASK(10, 10)
#define XTP_LN_TX_LCTXCP1_SW0_PRE2_DEFAULT	GENMASK(19, 19)
#define XTP_LN_TX_LCTXCP1_SW0_PRE3_DEFAULT	GENMASK(28, 28)
#define DRIVING_PARAM_6_DEFAULT	(XTP_LN_TX_LCTXCP1_SW0_PRE0_DEFAULT | \
				 XTP_LN_TX_LCTXCP1_SW0_PRE1_DEFAULT | \
				 XTP_LN_TX_LCTXCP1_SW0_PRE2_DEFAULT | \
				 XTP_LN_TX_LCTXCP1_SW0_PRE3_DEFAULT)

#define XTP_LN_TX_LCTXCP1_SW1_PRE0_DEFAULT	0
#define XTP_LN_TX_LCTXCP1_SW1_PRE1_DEFAULT	GENMASK(10, 9)
#define XTP_LN_TX_LCTXCP1_SW1_PRE2_DEFAULT	GENMASK(19, 18)
#define XTP_LN_TX_LCTXCP1_SW2_PRE0_DEFAULT	0
#define DRIVING_PARAM_7_DEFAULT	(XTP_LN_TX_LCTXCP1_SW1_PRE0_DEFAULT | \
				 XTP_LN_TX_LCTXCP1_SW1_PRE1_DEFAULT | \
				 XTP_LN_TX_LCTXCP1_SW1_PRE2_DEFAULT | \
				 XTP_LN_TX_LCTXCP1_SW2_PRE0_DEFAULT)

#define XTP_LN_TX_LCTXCP1_SW2_PRE1_DEFAULT	GENMASK(3, 3)
#define XTP_LN_TX_LCTXCP1_SW3_PRE0_DEFAULT	0
#define DRIVING_PARAM_8_DEFAULT	(XTP_LN_TX_LCTXCP1_SW2_PRE1_DEFAULT | \
				 XTP_LN_TX_LCTXCP1_SW3_PRE0_DEFAULT)

/* ePD MAC REG */
#define MTK_DP_ENC0_P0_313C (ENC0_OFFSET + 0x13C)

#define MTK_DP_TRANS_P0_3428 (TRANS_OFFSET + 0x028)

#define MTK_DP_TRANS_P0_3440 (TRANS_OFFSET + 0x040)
#define MTK_DP_TRANS_P0_3444 (TRANS_OFFSET + 0x044)
#define MTK_DP_TRANS_P0_3448 (TRANS_OFFSET + 0x048)
#define MTK_DP_TRANS_P0_344C (TRANS_OFFSET + 0x04c)
#define MTK_DP_TRANS_P0_3450 (TRANS_OFFSET + 0x050)

#define MTK_DP_TRANS_P0_3478 (TRANS_OFFSET + 0x078)

#define HSW_SW_DP_ENCODER0_P0_FLDMASK                                   0x7fff
#define HSW_SW_DP_ENCODER0_P0_FLDMASK_POS                               0
#define HSW_SW_DP_ENCODER0_P0_FLDMASK_LEN                               15

#define HSP_SW_DP_ENCODER0_P0_FLDMASK                                   0x8000
#define HSP_SW_DP_ENCODER0_P0_FLDMASK_POS                               15
#define HSP_SW_DP_ENCODER0_P0_FLDMASK_LEN                               1

#define VSW_SW_DP_ENCODER0_P0_FLDMASK                                   0x7fff
#define VSW_SW_DP_ENCODER0_P0_FLDMASK_POS                               0
#define VSW_SW_DP_ENCODER0_P0_FLDMASK_LEN                               15

#define VSP_SW_DP_ENCODER0_P0_FLDMASK                                   0x8000
#define VSP_SW_DP_ENCODER0_P0_FLDMASK_POS                               15
#define VSP_SW_DP_ENCODER0_P0_FLDMASK_LEN                               1
#define VBID_AUDIO_MUTE_FLAG_SW_DP_ENCODER0_P0_FLDMASK                  0x800
#define VBID_AUDIO_MUTE_SW_DP_ENCODER0_P0_FLDMASK_POS                   11
#define VBID_AUDIO_MUTE_FLAG_SEL_DP_ENCODER0_P0_FLDMASK                 0x1000
#define VBID_AUDIO_MUTE_SEL_DP_ENCODER0_P0_FLDMASK_POS                  12
#define AU_EN_DP_ENCODER0_P0_FLDMASK                                      0x40
#define AU_EN_DP_ENCODER0_P0_FLDMASK_POS                                  6

#define DP_TX0_VOLT_SWING_FLDMASK                                         0x3
#define DP_TX0_VOLT_SWING_FLDMASK_POS                                     0
#define DP_TX0_VOLT_SWING_FLDMASK_LEN                                     2

#define DP_TX0_PRE_EMPH_FLDMASK                                           0xc
#define DP_TX0_PRE_EMPH_FLDMASK_POS                                       2
#define DP_TX0_PRE_EMPH_FLDMASK_LEN                                       2

#define DP_TX0_DATAK_FLDMASK                                              0xf0
#define DP_TX0_DATAK_FLDMASK_POS                                          4
#define DP_TX0_DATAK_FLDMASK_LEN                                          4

#define DP_TX1_VOLT_SWING_FLDMASK                                         0x300
#define DP_TX1_VOLT_SWING_FLDMASK_POS                                     8
#define DP_TX1_VOLT_SWING_FLDMASK_LEN                                     2

#define DP_TX1_PRE_EMPH_FLDMASK                                           0xc00
#define DP_TX1_PRE_EMPH_FLDMASK_POS                                       10
#define DP_TX1_PRE_EMPH_FLDMASK_LEN                                       2

#define DP_TX1_DATAK_FLDMASK                                              0xf000
#define DP_TX1_DATAK_FLDMASK_POS                                          12
#define DP_TX1_DATAK_FLDMASK_LEN                                          4

#define DP_TX2_VOLT_SWING_FLDMASK                              0x30000
#define DP_TX2_VOLT_SWING_FLDMASK_POS                          16
#define DP_TX2_VOLT_SWING_FLDMASK_LEN                          2

#define DP_TX2_PRE_EMPH_FLDMASK                                0xc0000
#define DP_TX2_PRE_EMPH_FLDMASK_POS                            18
#define DP_TX2_PRE_EMPH_FLDMASK_LEN                            2

#define DP_TX2_DATAK_FLDMASK                                   0xf00000
#define DP_TX2_DATAK_FLDMASK_POS                               20
#define DP_TX2_DATAK_FLDMASK_LEN                               4

#define DP_TX3_VOLT_SWING_FLDMASK                              0x3000000
#define DP_TX3_VOLT_SWING_FLDMASK_POS                          24
#define DP_TX3_VOLT_SWING_FLDMASK_LEN                          2

#define DP_TX3_PRE_EMPH_FLDMASK                                0xc000000
#define DP_TX3_PRE_EMPH_FLDMASK_POS                            26
#define DP_TX3_PRE_EMPH_FLDMASK_LEN                            2

#define DP_TX3_DATAK_FLDMASK                                   0xf0000000L
#define DP_TX3_DATAK_FLDMASK_POS                               28
#define DP_TX3_DATAK_FLDMASK_LEN                               4

#define POST_MISC_PN_SWAP_EN_LANE0_DP_TRANS_P0_FLDMASK                    0x10
#define POST_MISC_PN_SWAP_EN_LANE0_DP_TRANS_P0_FLDMASK_POS                4
#define POST_MISC_PN_SWAP_EN_LANE0_DP_TRANS_P0_FLDMASK_LEN                1

#define POST_MISC_PN_SWAP_EN_LANE1_DP_TRANS_P0_FLDMASK                    0x20
#define POST_MISC_PN_SWAP_EN_LANE1_DP_TRANS_P0_FLDMASK_POS                5
#define POST_MISC_PN_SWAP_EN_LANE1_DP_TRANS_P0_FLDMASK_LEN                1

#define POST_MISC_PN_SWAP_EN_LANE2_DP_TRANS_P0_FLDMASK                    0x40
#define POST_MISC_PN_SWAP_EN_LANE2_DP_TRANS_P0_FLDMASK_POS                6
#define POST_MISC_PN_SWAP_EN_LANE2_DP_TRANS_P0_FLDMASK_LEN                1

#define POST_MISC_PN_SWAP_EN_LANE3_DP_TRANS_P0_FLDMASK                    0x80
#define POST_MISC_PN_SWAP_EN_LANE3_DP_TRANS_P0_FLDMASK_POS                7
#define POST_MISC_PN_SWAP_EN_LANE3_DP_TRANS_P0_FLDMASK_LEN                1

bool mtk_edp_phy_auto_test(struct dp_cts_auto_req *mtk_edp_test, u8 dpcd_201);

#endif
