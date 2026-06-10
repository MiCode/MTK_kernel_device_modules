/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2020 XiaoMi, Inc. All rights reserved.
 */

#ifndef _PANEL_p88_42_02_0A_DSC_VDO_H_
#define _PANEL_p88_42_02_0A_DSC_VDO_H_

#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <drm/mediatek_drm.h>
#include <uapi/drm/mi_disp.h>
//#include "mi_panel_ext.h"

#define REGFLAG_CMD			0xFFFA
#define REGFLAG_DELAY			0xFFFC
#define REGFLAG_UDELAY			0xFFFB
#define REGFLAG_END_OF_TABLE    	0xFFFD
#define REGFLAG_RESET_LOW		0xFFFE
#define REGFLAG_RESET_HIGH		0xFFFF
#define DATA_RATE			1174

#define FRAME_WIDTH			(1880)
#define FRAME_HEIGHT			(3008)

#define DSC_ENABLE                  1
#define DSC_VER                     18
#define DSC_DUAL_DSC_ENABLE         1
#define DSC_SLICE_MODE              1
#define DSC_RGB_SWAP                0
#define DSC_DSC_CFG                 40
#define DSC_RCT_ON                  1
#define DSC_BIT_PER_CHANNEL         10
#define DSC_DSC_LINE_BUF_DEPTH      11
#define DSC_BP_ENABLE               1
#define DSC_BIT_PER_PIXEL           128
#define DSC_SLICE_HEIGHT            8
#define DSC_SLICE_WIDTH             470
#define DSC_CHUNK_SIZE              470
#define DSC_XMIT_DELAY              512
#define DSC_DEC_DELAY               512
#define DSC_SCALE_VALUE             32
#define DSC_INCREMENT_INTERVAL      168
#define DSC_DECREMENT_INTERVAL      6
#define DSC_LINE_BPG_OFFSET         13
#define DSC_NFL_BPG_OFFSET          3804
#define DSC_SLICE_BPG_OFFSET        3705
#define DSC_INITIAL_OFFSET          6144
#define DSC_FINAL_OFFSET            4320
#define DSC_FLATNESS_MINQP          7
#define DSC_FLATNESS_MAXQP          16
#define DSC_RC_MODEL_SIZE           8192
#define DSC_RC_EDGE_FACTOR          6
#define DSC_RC_QUANT_INCR_LIMIT0    15
#define DSC_RC_QUANT_INCR_LIMIT1    15
#define DSC_RC_TGT_OFFSET_HI        3
#define DSC_RC_TGT_OFFSET_LO        3

#define MAX_BRIGHTNESS_CLONE	4095
#define FACTORY_MAX_BRIGHTNESS	2047
#define NORMAL_HW_BRIGHTNESS	1023

#define PHYSICAL_WIDTH              118496
#define PHYSICAL_HEIGHT             189594

/* Set delay of Vsync */
/* Send VSYNC after the lines following frame done */
#define PREFETCH_TIME          (0)


// Notice start
// when modifying the initial code should modify it.
#define FPS_INIT_INDEX 50
#define TEMP_INDEX 49
// Notice end

#define TEMP_INDEX1_36 0x01
#define TEMP_INDEX2_32 0x20
#define TEMP_INDEX3_28 0x40
#define TEMP_INDEX4_off 0x80


static atomic_t doze_enable = ATOMIC_INIT(0);
static const int panel_id = 1;
static const int doze_hbm_dbv_level = 242;
static const int doze_lbm_dbv_level = 20;
static int current_fps = 120;
static struct lcm *panel_ctx;
static struct drm_panel * this_panel = NULL;
static int normal_max_bl = 2047;
static unsigned int last_non_zero_bl_level = 511;
static int PEAK_HDR_BL_LEVEL = 4094;
static int PEAK_MAX_BL_LEVEL = 4095;
static char oled_wp_cmdline[18] = {0};
static const char *panel_name = "panel_name=dsi_p88_42_02_0a_dsc_vdo";
static char oled_lhbm_cmdline[80] = {0};
static bool lhbm_w1200_update_flag = true;
static bool lhbm_w200_update_flag = true;
static bool lhbm_g500_update_flag = true;
static bool lhbm_w1200_readbackdone;
static bool lhbm_w200_readbackdone;
static bool lhbm_g500_readbackdone;
static bool is_lcm_connected = false;
struct LHBM_WHITEBUF {
	unsigned char lhbm_1200[6];
	unsigned char lhbm_200[6];
	unsigned char lhbm_500[6];
};
static struct LHBM_WHITEBUF lhbm_whitebuf;

enum lhbm_cmd_type {
	TYPE_WHITE_1200 = 0,
	TYPE_WHITE_200,
	TYPE_GREEN_500,
	TYPE_LHBM_OFF,
	TYPE_HLPM_W1200,
	TYPE_HLPM_W200,
	TYPE_HLPM_G500,
	TYPE_HLPM_OFF,
	TYPE_MAX
};

enum cabc_mode {
	CABC_OFF,
	CABC_UI_ON,
	CABC_MOVIE_ON,
	CABC_STILL_ON
};

struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
	unsigned int mode);

#define lcm_dcs_write_seq(ctx, seq...) \
({\
	const u8 d[] = { seq };\
	BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64, "DCS sequence too big for stack");\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

#define lcm_dcs_write_seq_static(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})


#define FHD_FRAME_WIDTH    (1880)
#define FHD_FRAME_HEIGHT   (3008)

#define FHD_HFP_165            (32)
#define FHD_HSA_165            (8)
#define FHD_HBP_165            (22)
#define FHD_HTOTAL_165         (FHD_FRAME_WIDTH + FHD_HFP_165 + FHD_HSA_165 + FHD_HBP_165)

#define FHD_VFP_165            (110)
#define FHD_VSA_165            (2)
#define FHD_VBP_165            (34)
#define FHD_VTOTAL_165         (FHD_FRAME_HEIGHT + FHD_VFP_165 + FHD_VSA_165 + FHD_VBP_165)

#define FHD_HFP_144            (32)
#define FHD_HSA_144            (8)
#define FHD_HBP_144            (22)
#define FHD_HTOTAL_144         (FHD_FRAME_WIDTH + FHD_HFP_144 + FHD_HSA_144 + FHD_HBP_144)

#define FHD_VFP_144            (570)
#define FHD_VSA_144            (2)
#define FHD_VBP_144            (34)
#define FHD_VTOTAL_144         (FHD_FRAME_HEIGHT + FHD_VFP_144 + FHD_VSA_144 + FHD_VBP_144)

#define FHD_HFP_120            (86)
#define FHD_HSA_120            (8)
#define FHD_HBP_120            (86)
#define FHD_HTOTAL_120         (FHD_FRAME_WIDTH + FHD_HFP_120 + FHD_HSA_120 + FHD_HBP_120)

#define FHD_VFP_120            (118)
#define FHD_VSA_120            (2)
#define FHD_VBP_120            (176)
#define FHD_VTOTAL_120         (FHD_FRAME_HEIGHT + FHD_VFP_120 + FHD_VSA_120 + FHD_VBP_120)

#define FHD_HFP_90            (188)
#define FHD_HSA_90            (8)
#define FHD_HBP_90            (188)
#define FHD_HTOTAL_90         (FHD_FRAME_WIDTH + FHD_HFP_90 + FHD_HSA_90 + FHD_HBP_90)

#define FHD_VFP_90            (80)
#define FHD_VSA_90            (2)
#define FHD_VBP_90            (22)
#define FHD_VTOTAL_90         (FHD_FRAME_HEIGHT + FHD_VFP_90 + FHD_VSA_90 + FHD_VBP_90)

#define FHD_HFP_60            (86)
#define FHD_HSA_60            (8)
#define FHD_HBP_60            (86)
#define FHD_HTOTAL_60         (FHD_FRAME_WIDTH + FHD_HFP_60 + FHD_HSA_60 + FHD_HBP_60)

#define FHD_VFP_60            (3422)
#define FHD_VSA_60            (2)
#define FHD_VBP_60            (176)
#define FHD_VTOTAL_60         (FHD_FRAME_HEIGHT + FHD_VFP_60 + FHD_VSA_60 + FHD_VBP_60)

#define FHD_HFP_50            (188)
#define FHD_HSA_50            (8)
#define FHD_HBP_50            (188)
#define FHD_HTOTAL_50         (FHD_FRAME_WIDTH + FHD_HFP_50 + FHD_HSA_50 + FHD_HBP_50)

#define FHD_VFP_50            (2570)
#define FHD_VSA_50            (2)
#define FHD_VBP_50            (22)
#define FHD_VTOTAL_50         (FHD_FRAME_HEIGHT + FHD_VFP_50 + FHD_VSA_50 + FHD_VBP_50)

#define FHD_HFP_48            (188)
#define FHD_HSA_48            (8)
#define FHD_HBP_48            (188)
#define FHD_HTOTAL_48         (FHD_FRAME_WIDTH + FHD_HFP_48 + FHD_HSA_48 + FHD_HBP_48)

#define FHD_VFP_48            (2804)
#define FHD_VSA_48            (2)
#define FHD_VBP_48            (22)
#define FHD_VTOTAL_48         (FHD_FRAME_HEIGHT + FHD_VFP_48 + FHD_VSA_48 + FHD_VBP_48)

#define FHD_HFP_30            (188)
#define FHD_HSA_30            (8)
#define FHD_HBP_30            (188)
#define FHD_HTOTAL_30         (FHD_FRAME_WIDTH + FHD_HFP_30 + FHD_HSA_30 + FHD_HBP_30)

#define FHD_VFP_30            (6300)
#define FHD_VSA_30            (2)
#define FHD_VBP_30            (22)
#define FHD_VTOTAL_30         (FHD_FRAME_HEIGHT + FHD_VFP_30 + FHD_VSA_30 + FHD_VBP_30)

#define FHD_FRAME_TOTAL_165    (FHD_HTOTAL_165 * FHD_VTOTAL_165)
#define FHD_FRAME_TOTAL_144    (FHD_HTOTAL_144 * FHD_VTOTAL_144)
#define FHD_FRAME_TOTAL_120    (FHD_HTOTAL_120 * FHD_VTOTAL_120)
#define FHD_FRAME_TOTAL_90    (FHD_HTOTAL_90 * FHD_VTOTAL_90)
#define FHD_FRAME_TOTAL_60    (FHD_HTOTAL_60 * FHD_VTOTAL_60)
#define FHD_FRAME_TOTAL_50    (FHD_HTOTAL_50 * FHD_VTOTAL_50)
#define FHD_FRAME_TOTAL_48    (FHD_HTOTAL_48 * FHD_VTOTAL_48)
#define FHD_FRAME_TOTAL_30    (FHD_HTOTAL_30 * FHD_VTOTAL_30)

#define FHD_VREFRESH_165    (165)
#define FHD_VREFRESH_144    (144)
#define FHD_VREFRESH_120   (120)
#define FHD_VREFRESH_90    (90)
#define FHD_VREFRESH_60    (60)
#define FHD_VREFRESH_50    (50)
#define FHD_VREFRESH_48    (48)
#define FHD_VREFRESH_30    (30)

#define FHD_CLK_165_X10    ((FHD_FRAME_TOTAL_165 * FHD_VREFRESH_165) / 100)
#define FHD_CLK_144_X10    ((FHD_FRAME_TOTAL_144 * FHD_VREFRESH_144) / 100)
#define FHD_CLK_120_X10    ((FHD_FRAME_TOTAL_120 * FHD_VREFRESH_120) / 100)
#define FHD_CLK_90_X10     ((FHD_FRAME_TOTAL_90 * FHD_VREFRESH_90) / 100)
#define FHD_CLK_60_X10     ((FHD_FRAME_TOTAL_60 * FHD_VREFRESH_60) / 100)
#define FHD_CLK_50_X10     ((FHD_FRAME_TOTAL_50 * FHD_VREFRESH_50) / 100)
#define FHD_CLK_48_X10     ((FHD_FRAME_TOTAL_48 * FHD_VREFRESH_48) / 100)
#define FHD_CLK_30_X10     ((FHD_FRAME_TOTAL_30 * FHD_VREFRESH_30) / 100)

#define FHD_CLK_165		(((FHD_CLK_165_X10 % 10) != 0) ?             \
			(FHD_CLK_165_X10 / 10 + 1) : (FHD_CLK_165_X10 / 10))
#define FHD_CLK_144		(((FHD_CLK_144_X10 % 10) != 0) ?             \
			(FHD_CLK_144_X10 / 10 + 1) : (FHD_CLK_144_X10 / 10))
#define FHD_CLK_120		(((FHD_CLK_120_X10 % 10) != 0) ?             \
			(FHD_CLK_120_X10 / 10 + 1) : (FHD_CLK_120_X10 / 10))
#define FHD_CLK_90		(((FHD_CLK_90_X10 % 10) != 0) ?              \
			(FHD_CLK_90_X10 / 10 + 1) : (FHD_CLK_90_X10 / 10))
#define FHD_CLK_60		(((FHD_CLK_60_X10 % 10) != 0) ?              \
			(FHD_CLK_60_X10 / 10 + 1) : (FHD_CLK_60_X10 / 10))
#define FHD_CLK_50		(((FHD_CLK_50_X10 % 10) != 0) ?             \
			(FHD_CLK_50_X10 / 10 + 1) : (FHD_CLK_50_X10 / 10))
#define FHD_CLK_48		(((FHD_CLK_48_X10 % 10) != 0) ?             \
			(FHD_CLK_48_X10 / 10 + 1) : (FHD_CLK_48_X10 / 10))
#define FHD_CLK_30		(((FHD_CLK_30_X10 % 10) != 0) ?             \
			(FHD_CLK_30_X10 / 10 + 1) : (FHD_CLK_30_X10 / 10))

static const struct drm_display_mode mode_165hz = {
	.clock = FHD_CLK_165,
	.hdisplay = FHD_FRAME_WIDTH,
	.hsync_start = FHD_FRAME_WIDTH + FHD_HFP_165,							//HFP
	.hsync_end = FHD_FRAME_WIDTH + FHD_HFP_165 + FHD_HSA_165,					//HSA
	.htotal = FHD_FRAME_WIDTH + FHD_HFP_165 + FHD_HSA_165 + FHD_HBP_165,		//HBP
	.vdisplay = FHD_FRAME_HEIGHT,
	.vsync_start = FHD_FRAME_HEIGHT + FHD_VFP_165,							//VFP
	.vsync_end = FHD_FRAME_HEIGHT + FHD_VFP_165 + FHD_VSA_165,				//VSA
	.vtotal = FHD_FRAME_HEIGHT + FHD_VFP_165 + FHD_VSA_165 + FHD_VBP_165,		//VBP
};

static const struct drm_display_mode mode_144hz = {
	.clock = FHD_CLK_144,
	.hdisplay = FHD_FRAME_WIDTH,
	.hsync_start = FHD_FRAME_WIDTH + FHD_HFP_144,							//HFP
	.hsync_end = FHD_FRAME_WIDTH + FHD_HFP_144 + FHD_HSA_144,					//HSA
	.htotal = FHD_FRAME_WIDTH + FHD_HFP_144 + FHD_HSA_144 + FHD_HBP_144,		//HBP
	.vdisplay = FHD_FRAME_HEIGHT,
	.vsync_start = FHD_FRAME_HEIGHT + FHD_VFP_144,							//VFP
	.vsync_end = FHD_FRAME_HEIGHT + FHD_VFP_144 + FHD_VSA_144,				//VSA
	.vtotal = FHD_FRAME_HEIGHT + FHD_VFP_144 + FHD_VSA_144 + FHD_VBP_144,		//VBP
};

static const struct drm_display_mode mode_120hz = {
	.clock = FHD_CLK_120,
	.hdisplay = FHD_FRAME_WIDTH,
	.hsync_start = FHD_FRAME_WIDTH + FHD_HFP_120,							//HFP
	.hsync_end = FHD_FRAME_WIDTH + FHD_HFP_120 + FHD_HSA_120,					//HSA
	.htotal = FHD_FRAME_WIDTH + FHD_HFP_120 + FHD_HSA_120 + FHD_HBP_120,		//HBP
	.vdisplay = FHD_FRAME_HEIGHT,
	.vsync_start = FHD_FRAME_HEIGHT + FHD_VFP_120,							//VFP
	.vsync_end = FHD_FRAME_HEIGHT + FHD_VFP_120 + FHD_VSA_120,				//VSA
	.vtotal = FHD_FRAME_HEIGHT + FHD_VFP_120 + FHD_VSA_120 + FHD_VBP_120,		//VBP
};

static const struct drm_display_mode mode_90hz = {
	.clock = FHD_CLK_90,
	.hdisplay = FHD_FRAME_WIDTH,
	.hsync_start = FHD_FRAME_WIDTH + FHD_HFP_90,								//HFP
	.hsync_end = FHD_FRAME_WIDTH + FHD_HFP_90 + FHD_HSA_90,						//HSA
	.htotal = FHD_FRAME_WIDTH + FHD_HFP_90 + FHD_HSA_90 + FHD_HBP_90,			//HBP
	.vdisplay = FHD_FRAME_HEIGHT,
	.vsync_start = FHD_FRAME_HEIGHT + FHD_VFP_90,								//VFP
	.vsync_end = FHD_FRAME_HEIGHT + FHD_VFP_90 + FHD_VSA_90,					//VSA
	.vtotal = FHD_FRAME_HEIGHT + FHD_VFP_90 + FHD_VSA_90 + FHD_VBP_90,			//VBP
};

static const struct drm_display_mode mode_60hz = {
	.clock = FHD_CLK_60,
	.hdisplay = FHD_FRAME_WIDTH,
	.hsync_start = FHD_FRAME_WIDTH + FHD_HFP_60,								//HFP
	.hsync_end = FHD_FRAME_WIDTH + FHD_HFP_60 + FHD_HSA_60,						//HSA
	.htotal = FHD_FRAME_WIDTH + FHD_HFP_60 + FHD_HSA_60 + FHD_HBP_60,			//HBP
	.vdisplay = FHD_FRAME_HEIGHT,
	.vsync_start = FHD_FRAME_HEIGHT + FHD_VFP_60,								//VFP
	.vsync_end = FHD_FRAME_HEIGHT + FHD_VFP_60 + FHD_VSA_60,					//VSA
	.vtotal = FHD_FRAME_HEIGHT + FHD_VFP_60 + FHD_VSA_60 + FHD_VBP_60,			//VBP
};

static const struct drm_display_mode mode_50hz = {
	.clock = FHD_CLK_50,
	.hdisplay = FHD_FRAME_WIDTH,
	.hsync_start = FHD_FRAME_WIDTH + FHD_HFP_50,								//HFP
	.hsync_end = FHD_FRAME_WIDTH + FHD_HFP_50 + FHD_HSA_50,						//HSA
	.htotal = FHD_FRAME_WIDTH + FHD_HFP_50 + FHD_HSA_50 + FHD_HBP_50,			//HBP
	.vdisplay = FHD_FRAME_HEIGHT,
	.vsync_start = FHD_FRAME_HEIGHT + FHD_VFP_50,								//VFP
	.vsync_end = FHD_FRAME_HEIGHT + FHD_VFP_50 + FHD_VSA_50,					//VSA
	.vtotal = FHD_FRAME_HEIGHT + FHD_VFP_50 + FHD_VSA_50 + FHD_VBP_50,			//VBP
};

static const struct drm_display_mode mode_48hz = {
	.clock = FHD_CLK_48,
	.hdisplay = FHD_FRAME_WIDTH,
	.hsync_start = FHD_FRAME_WIDTH + FHD_HFP_48,								//HFP
	.hsync_end = FHD_FRAME_WIDTH + FHD_HFP_48 + FHD_HSA_48,						//HSA
	.htotal = FHD_FRAME_WIDTH + FHD_HFP_48 + FHD_HSA_48 + FHD_HBP_48,			//HBP
	.vdisplay = FHD_FRAME_HEIGHT,
	.vsync_start = FHD_FRAME_HEIGHT + FHD_VFP_48,								//VFP
	.vsync_end = FHD_FRAME_HEIGHT + FHD_VFP_48 + FHD_VSA_48,					//VSA
	.vtotal = FHD_FRAME_HEIGHT + FHD_VFP_48 + FHD_VSA_48 + FHD_VBP_48,			//VBP
};

static const struct drm_display_mode mode_30hz = {
	.clock = FHD_CLK_30,
	.hdisplay = FHD_FRAME_WIDTH,
	.hsync_start = FHD_FRAME_WIDTH + FHD_HFP_30,								//HFP
	.hsync_end = FHD_FRAME_WIDTH + FHD_HFP_30 + FHD_HSA_30,						//HSA
	.htotal = FHD_FRAME_WIDTH + FHD_HFP_30 + FHD_HSA_30 + FHD_HBP_30,			//HBP
	.vdisplay = FHD_FRAME_HEIGHT,
	.vsync_start = FHD_FRAME_HEIGHT + FHD_VFP_30,								//VFP
	.vsync_end = FHD_FRAME_HEIGHT + FHD_VFP_30 + FHD_VSA_30,					//VSA
	.vtotal = FHD_FRAME_HEIGHT + FHD_VFP_30 + FHD_VSA_30 + FHD_VBP_30,			//VBP
};

/* Attention
 *  1."FPS_INIT_INDEX" is related to switch frame rate,when modifying the initial code should modify it.
 *  2."TEMP_INDEX"  is related to 3D Lut temperature compensation,when modifying the initial code should modify it.
 *  owner: chenyuan8@xiaomi.com
 *  date: 2023/09/11
 *  changeId: 3341299
 */

static struct LCM_setting_table init_setting_vdo[] = {
	{0xFF,1,{0x23}},
	{0xFB,1,{0x01}},
	{0x89,1,{0xA4}},
	{0xFF,1,{0x25}},
	{0xFB,1,{0x01}},
	{0xED,1,{0x00}},
	{0xFF,1,{0x27}},
	{0xFB,1,{0x01}},
	{0x13,1,{0x00}},
	{0x14,1,{0x22}},
	{0xFF,1,{0xE0}},
	{0xFB,1,{0x01}},
	{0xBB,1,{0x00}},
	{0xFF,1,{0xF0}},
	{0xFB,1,{0x01}},
	{0xE6,1,{0x02}},
	{0xFF,1,{0x10}},
	{0xFB,1,{0x01}},
	{0x3B,9,{0x03,0xB2,0x76,0x04,0x04,0x00,0x80,0x24,0x18}},
	{0x60,1,{0x00}},
	{0xB3,1,{0x40}},
	{0xFF,1,{0x20}},
	{0xFB,1,{0x01}},
	{0x7D,1,{0x99}},
	{0xFF,1,{0x24}},
	{0xFB,1,{0x01}},
	{0x93,6,{0x76,0x00,0x6E,0x00,0x50,0x00}},
	{0x94,4,{0xB2,0x00,0x24,0x18}},
	{0xFF,1,{0x26}},
	{0xFB,1,{0x01}},
	{0xCD,3,{0x49,0x66,0x00}},
	{0xCE,3,{0x46,0x66,0x00}},
	{0xFF,1,{0x27}},
	{0xFB,1,{0x01}},
	{0x03,1,{0x16}},
	{0x5B,1,{0xB1}},
	/* ESD detect */
	{0xFF,1,{0x27}},
	{0xFB,1,{0x01}},
	{0xD0,1,{0x31}},
	{0xD1,2,{0x02,0x08}},
	{0xD2,1,{0x03}},
	{0xD3,1,{0x10}},
	{0xD4,1,{0x08}},
	{0xDE,1,{0x43}},
	{0xDF,1,{0x02}},
	{0xFF,1,{0x10}},
	/* CABC */
	{0xFF,1,{0x23}},
	{0xFB,1,{0x01}},
	//PWM 12bit
	{0x00,1,{0x80}},
	//dimming enable
	{0x01,1,{0x84}},
	{0x05,1,{0x2D}},
	{0x06,1,{0x00}},
	//PWM frequency=10Khz
	{0x07,1,{0x20}},
	{0x08,1,{0x01}},
	{0x09,1,{0xC2}},
	//resolution 1880x3008
	{0x11,1,{0x03}},
	{0x12,1,{0x73}},
	{0x13,1,{0x00}},
	{0x15,1,{0x5F}},
	{0x16,1,{0x15}},
	{0x6E,1,{0x3F}},
	{0x6F,1,{0xCA}},
	{0x70,1,{0x0A}},
	{0x71,1,{0x3F}},
	//image compensation
	{0x0A,1,{0x1C}},
	{0x0B,1,{0x1C}},
	{0x0C,1,{0x0E}},
	{0x0D,1,{0x1A}},
	{0x0E,1,{0x20}},
	{0x0F,1,{0x00}},
	{0x19,1,{0x11}},
	{0x1A,1,{0x11}},
	{0x1B,1,{0x12}},
	{0x1C,1,{0x12}},
	{0x1D,1,{0x12}},
	{0x1E,1,{0x13}},
	{0x1F,1,{0x14}},
	{0x20,1,{0x14}},
	{0x21,1,{0x15}},
	{0x22,1,{0x19}},
	{0x23,1,{0x1E}},
	{0x24,1,{0x23}},
	{0x25,1,{0x29}},
	{0x26,1,{0x2F}},
	{0x27,1,{0x36}},
	{0x28,1,{0x3F}},
	{0x29,1,{0x08}},
	{0x2A,1,{0x3F}},
	{0x2B,1,{0x19}},
	//UI mode
	{0x30,1,{0xFF}},
	{0x31,1,{0xFD}},
	{0x32,1,{0xFC}},
	{0x33,1,{0xFA}},
	{0x34,1,{0xF9}},
	{0x35,1,{0xF7}},
	{0x36,1,{0xF5}},
	{0x37,1,{0xF4}},
	{0x38,1,{0xF2}},
	{0x39,1,{0xF0}},
	{0x3A,1,{0xEF}},
	{0x3B,1,{0xED}},
	{0x3D,1,{0xEB}},
	{0x3F,1,{0xE9}},
	{0x40,1,{0xE8}},
	{0x41,1,{0xE6}},
	//STILL mode
	{0x45,1,{0xFF}},
	{0x46,1,{0xF6}},
	{0x47,1,{0xEC}},
	{0x48,1,{0xE3}},
	{0x49,1,{0xD9}},
	{0x4A,1,{0xD0}},
	{0x4B,1,{0xC6}},
	{0x4C,1,{0xBD}},
	{0x4D,1,{0xB3}},
	{0x4E,1,{0xA8}},
	{0x4F,1,{0x9D}},
	{0x50,1,{0x92}},
	{0x51,1,{0x87}},
	{0x52,1,{0x7C}},
	{0x53,1,{0x71}},
	{0x54,1,{0x66}},
	//MOVING mode
	{0x58,1,{0xFF}},
	{0x59,1,{0xFB}},
	{0x5A,1,{0xF6}},
	{0x5B,1,{0xF1}},
	{0x5C,1,{0xEC}},
	{0x5D,1,{0xE7}},
	{0x5E,1,{0xE3}},
	{0x5F,1,{0xDE}},
	{0x60,1,{0xD9}},
	{0x61,1,{0xD4}},
	{0x62,1,{0xCE}},
	{0x63,1,{0xC9}},
	{0x64,1,{0xC3}},
	{0x65,1,{0xBE}},
	{0x66,1,{0xB8}},
	{0x67,1,{0xB3}},
	{0xFF,1,{0x10}},
	{0xFB,1,{0x01}},
	{0x51,2,{0x0F,0xFF}},
	{0x53,1,{0x24}},
	{0x11,0,{}},
	{REGFLAG_DELAY,100,{}},
	{0x29,0,{}},
	/*CABC diming on*/
	{0xFF,1,{0x10}},
	{0x53,1,{0x2C}},
	{REGFLAG_END_OF_TABLE, 0x00, {}},
};

static struct LCM_setting_table CABC_Off[] = {
	{0xFF,1,{0x10}},
	{0x55,1,{0x00}},
};

static struct LCM_setting_table CABC_UIMODE[] = {
	{0xFF,1,{0x10}},
	{0x55,1,{0x01}},
};

static struct LCM_setting_table CABC_STILLMODE[] = {
	{0xFF,1,{0x10}},
	{0x55,1,{0x02}},
};

static struct LCM_setting_table CABC_MOVIVEMODE[] = {
	{0xFF,1,{0x10}},
	{0x55,1,{0x03}},
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0xFF, 1, {0x10}},
	{0x28, 0, {} },
	{REGFLAG_DELAY, 10, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY,120,{}},
	{REGFLAG_END_OF_TABLE, 0x00, {}},
};

static struct LCM_setting_table gray_3d_lut[] = {
	{0x57, 01, {0x00}},
};
#if 0

static struct LCM_setting_table mode_120hz_setting_gir_off[] = {
	/* frequency select 120hz */
	{0x2F, 01, {0x00}},
	{0x5F, 02, {0x01, 0x40}},	//gir off
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_120hz_setting_gir_on[] = {
	/* frequency select 120hz */
	{0x2F, 01, {0x00}},
	{0x5F, 02, {0x00, 0x40}},	//gir on
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_90hz_setting_gir_off[] = {
	/* frequency select 90hz */
	{0x2F, 01, {0x01}},
	{0x5F, 02, {0x01, 0x40}},	//gir off
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_90hz_setting_gir_on[] = {
	/* frequency select 90hz */
	{0x2F, 01, {0x01}},
	{0x5F, 02, {0x00, 0x40}},	//gir on
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_60hz_setting_gir_off[] = {
	/* frequency select 60hz */
	{0x2F, 01, {0x02}},
	{0x5F, 02, {0x01, 0x40}},	//gir off
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_60hz_setting_gir_on[] = {
	/* frequency select 60hz */
	{0x2F, 01, {0x02}},
	{0x5F, 02, {0x00, 0x40}},	//gir on
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
#endif

static struct LCM_setting_table lhbm_normal_white_1200nit[] = {
	//page7
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x07}},
	{0xC0, 01, {0x87}},    //RCN on
	{0x8B, 01, {0x10}},
	//CMD2 Page 8
	{0xF0, 05, {0x55,0xAA,0x52,0x08,0x08}},
	{0xB6, 16, {0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF}},
	{0x6F, 01, {0x10}},
	{0xB6, 16, {0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF}},
	{0x6F, 01, {0x20}},
	{0xB6, 14, {0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF}},
	{0x6F, 01, {0x55}},
	{0xB8, 11, {0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10}},
	{0x6F, 01, {0x60}},
	{0xB8, 16, {0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10}},
	{0x6F, 01, {0x70}},
	{0xB8, 16, {0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10}},
	{0x6F, 01, {0x80}},
	{0xB8, 03, {0x00,0x10,0x00}},
	/*1200nit gir on*/
	{0xF0, 05, {0x55,0xAA,0x52,0x08,0x02}},
	{0xD1, 06, {0x27,0x04,0x23,0xD0,0x2e,0xD0 }},
	/*FPR_ON*/
	{0xA9, 14, {0x02,0x00,0xB5,0x2C,0x2C,0x00,0x01,0x00,0x87,0x00,0x02,0x25,0x3F,0xF0}},
};

static struct LCM_setting_table lhbm_normal_white_200nit[] = {
	//page7
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x07}},
	{0xC0, 01, {0x87}},    //RCN on
	{0x8B, 01, {0x10}},
	//CMD2 Page 8
	{0xF0, 05, {0x55,0xAA,0x52,0x08,0x08}},
	{0xB6, 16, {0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF}},
	{0x6F, 01, {0x10}},
	{0xB6, 16, {0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF}},
	{0x6F, 01, {0x20}},
	{0xB6, 14, {0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF}},
	{0x6F, 01, {0x55}},
	{0xB8, 11, {0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10}},
	{0x6F, 01, {0x60}},
	{0xB8, 16, {0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10}},
	{0x6F, 01, {0x70}},
	{0xB8, 16, {0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10}},
	{0x6F, 01, {0x80}},
	{0xB8, 03, {0x00,0x10,0x00}},
	/*200nit gir on*/
	{0xF0, 05, {0x55,0xAA,0x52,0x08,0x02}},
	{0xD1, 06, {0x1B,0xBB,0x19,0x7A,0x1F,0x0A}},

	/*FPR_ON*/
	{0xA9, 14, {0x02,0x00,0xB5,0x2C,0x2C,0x00,0x01,0x00,0x87,0x00,0x02,0x25,0X32,0XB1}},
};

static struct LCM_setting_table lhbm_normal_green_500nit[] = {
	//page7
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x07}},
	{0xC0, 01, {0x87}},    //RCN on
	{0x8B, 01, {0x10}},
	//CMD2 Page 8
	{0xF0, 05, {0x55,0xAA,0x52,0x08,0x08}},
	{0xB6, 16, {0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF}},
	{0x6F, 01, {0x10}},
	{0xB6, 16, {0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF}},
	{0x6F, 01, {0x20}},
	{0xB6, 14, {0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF}},
	{0x6F, 01, {0x55}},
	{0xB8, 11, {0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10}},
	{0x6F, 01, {0x60}},
	{0xB8, 16, {0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10}},
	{0x6F, 01, {0x70}},
	{0xB8, 16, {0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10}},
	{0x6F, 01, {0x80}},
	{0xB8, 03, {0x00,0x10,0x00}},
	/*500nit gir on*/
	{0xF0, 05, {0x55,0xAA,0x52,0x08,0x02}},
	{0xD1, 06, {0x00,0x00,0x1F,0x98,0x00,0x00 }},
	/*FPR_ON*/
	{0xA9, 14, {0x02,0x00,0xB5,0x2C,0x2C,0x00,0x01,0x00,0x87,0x00,0x02,0x25,0x3F,0xF0}},
};

static struct LCM_setting_table lhbm_off[] = {
	{0x51, 2, {0x05, 0xFF}},
	{0x8B, 01, {0x00}},
	//page8
	{0xF0, 05, {0x55,0xAA,0x52,0x08,0x08}},
	{0xB6, 16, {0x09,0x62,0x09,0x62,0x0A,0x35,0x0A,0x35,0x0A,0xCA,0x0A,0xCA,0x0B,0x56,0x0B,0x56}},
	{0x6F, 01, {0x10}},
	{0xB6, 16, {0x0B,0xA5,0x0B,0xA5,0x0B,0xD9,0x0B,0xD9,0x09,0x25,0x09,0x25,0x0A,0xAF,0x0A,0xAF}},
	{0x6F, 01, {0x20}},
	{0xB6, 14, {0x0B,0xAE,0x0B,0xAE,0x0C,0x89,0x0C,0x89,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF}},
	{0x6F, 01, {0x55}},
	{0xB8, 11, {0x1B,0x45,0x1B,0x45,0x19,0x12,0x19,0x12,0x17,0xB8,0x17}},
	{0x6F, 01, {0x60}},
	{0xB8, 16, {0xB8,0x16,0x93,0x16,0x93,0x15,0xFA,0x15,0xFA,0x15,0x99,0x15,0x99,0x1B,0xFB,0x1B}},
	{0x6F, 01, {0x70}},
	{0xB8, 16, {0xFB,0x17,0xF4,0x17,0xF4,0x15,0xE9,0x15,0xE9,0x14,0x6A,0x14,0x6A,0x10,0x00,0x10}},
	{0x6F, 01, {0x80}},
	{0xB8, 03, {0x00,0x10,0x00}},
	{0xA9, 12, {0x02,0x00,0xB5,0x2C,0x2C,0x03,0x01,0x00,0x87,0x00,0x00,0x20}},
};

static struct LCM_setting_table lcm_aod_high_mode[] = {
	{0x5F, 02, {0x00,0x40}},
	{0x51, 06, {0x00,0xF2,0x00,0x00,0x0F,0xFF}},
};
static struct LCM_setting_table lcm_aod_low_mode[] = {
	{0x5F, 02, {0x00,0x40}},
	{0x51, 06, {0x00,0x14,0x00,0x00,0x01,0x55}},
};

#if 0
static struct LCM_setting_table bl_tb0[] = {
	{0x51, 02, {0x00,0x08}},
};
#endif

static struct LCM_setting_table lcm_peak_on[] = {
	{0xF0, 05, {0x55,0xAA,0x52,0x08,0x08}},
	{0x6F, 01, {0x70}},
	{0xB9, 01, {0x03}},
	{0x5F, 02, {0x05,0x40}},
	{0x51, 02, {0x0F,0xFF}},
};
static struct LCM_setting_table lcm_peak_off[] = {
	{0xF0, 05, {0x55,0xAA,0x52,0x08,0x08}},
	{0x6F, 01, {0x70}},
	{0xB9, 01, {0x02}},
	{0x5F, 02, {0x00,0x40}},
};
/*
static struct LCM_setting_table lcm_aod_high_mode_enter[] = {
	{0x5F, 02, {0x01,0x40}},
	{0x51, 06, {0x00,0xF2,0x00,0x00,0x0F,0xFF}},
	{0x39, 01, {0x00}},
};
static struct LCM_setting_table lcm_aod_low_mode_enter[] = {
	{0x5F, 02, {0x01,0x40}},
	{0x51, 06, {0x00,0x14,0x00,0x00,0x01,0x55}},
	{0x39, 01, {0x00}},
};
*/

static struct LCM_setting_table lcm_aod_mode_exit[] = {
	{0x38, 01, {0x00}},
	{0x51, 02, {0x00, 0x00}},
	{REGFLAG_DELAY, 34, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {}},
};

static unsigned int rc_buf_thresh[14] = {896, 1792, 2688, 3584, 4480, 5376, 6272, 6720, 7168, 7616, 7744, 7872, 8000, 8064};
static unsigned int range_min_qp[15] = {0, 4, 5, 5, 7, 7, 7, 7, 7, 7, 9, 9, 9, 13, 16};
static unsigned int range_max_qp[15] = {8, 8, 9, 10, 11, 11, 11, 12, 13, 14, 14, 15, 15, 16, 17};
static int range_bpg_ofs[15] = {2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -12, -12, -12, -12};

#endif // end of _PANEL_p88_42_02_0A_DSC_VDO_H_
