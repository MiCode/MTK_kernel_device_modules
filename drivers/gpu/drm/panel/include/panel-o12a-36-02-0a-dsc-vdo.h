/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2020 XiaoMi, Inc. All rights reserved.
 */

#ifndef _PANEL_O12A_36_02_0A_DSC_VDO_H_
#define _PANEL_O12A_36_02_0A_DSC_VDO_H_

#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <drm/mediatek_drm.h>
#include "mi_panel_ext.h"

#define REGFLAG_CMD			0xFFFA
#define REGFLAG_DELAY			0xFFFC
#define REGFLAG_UDELAY			0xFFFB
#define REGFLAG_END_OF_TABLE    	0xFFFD
#define REGFLAG_RESET_LOW		0xFFFE
#define REGFLAG_RESET_HIGH		0xFFFF
#define DATA_RATE			1100

#define FRAME_WIDTH			(1280)
#define FRAME_HEIGHT			(2772)

#define DSC_ENABLE                  1
#define DSC_VER                     18
#define DSC_SLICE_MODE              1
#define DSC_RGB_SWAP                0
#define DSC_DSC_CFG                 40
#define DSC_RCT_ON                  1
#define DSC_BIT_PER_CHANNEL         10
#define DSC_DSC_LINE_BUF_DEPTH      11
#define DSC_BP_ENABLE               1
#define DSC_BIT_PER_PIXEL           128
#define DSC_SLICE_HEIGHT            12
#define DSC_SLICE_WIDTH             640
#define DSC_CHUNK_SIZE              640
#define DSC_XMIT_DELAY              512
#define DSC_DEC_DELAY               577
#define DSC_SCALE_VALUE             32
#define DSC_INCREMENT_INTERVAL      312
#define DSC_DECREMENT_INTERVAL      8
#define DSC_LINE_BPG_OFFSET         12
#define DSC_NFL_BPG_OFFSET          2235
#define DSC_SLICE_BPG_OFFSET        1825
#define DSC_INITIAL_OFFSET          6144
#define DSC_FINAL_OFFSET            4336
#define DSC_FLATNESS_MINQP          7
#define DSC_FLATNESS_MAXQP          16
#define DSC_RC_MODEL_SIZE           8192
#define DSC_RC_EDGE_FACTOR          6
#define DSC_RC_QUANT_INCR_LIMIT0    15
#define DSC_RC_QUANT_INCR_LIMIT1    15
#define DSC_RC_TGT_OFFSET_HI        3
#define DSC_RC_TGT_OFFSET_LO        3

#define MAX_BRIGHTNESS_CLONE	16383
#define FACTORY_MAX_BRIGHTNESS	8191
#define NORMAL_HW_BRIGHTNESS	16383

#define PHYSICAL_WIDTH              72729
#define PHYSICAL_HEIGHT             157505

/* Set delay of Vsync */
/* Send VSYNC after the lines following frame done */
#define PREFETCH_TIME          (0)


// Notice start
// when modifying the initial code should modify it.
#define FPS_INIT_INDEX 48
#if 0
#define TEMP_INDEX 49
// Notice end

#define TEMP_INDEX1_36 0x01
#define TEMP_INDEX2_32 0x20
#define TEMP_INDEX3_28 0x40
#define TEMP_INDEX4_off 0x80
#endif

static atomic_t doze_enable = ATOMIC_INIT(0);
static const int panel_id = 1;
static const int doze_hbm_dbv_level = 808;
static const int doze_lbm_dbv_level = 56;
static int current_fps = 120;
static struct lcm *panel_ctx;
static struct drm_panel * this_panel = NULL;
static int normal_max_bl = 2047;
static unsigned int last_non_zero_bl_level = 511;
static int PEAK_HDR_BL_LEVEL = 4094;
static int PEAK_MAX_BL_LEVEL = 4095;
static char oled_wp_cmdline[18] = {0};
static char panel_sn_cmdline[15] = {0};
static const char *panel_name = "panel_name=dsi_o12a_36_02_0a_dsc_vdo";
static char oled_lhbm_cmdline[80] = {0};
static char oled_tplockdown_cmdline[18] = {0};
static bool lhbm_w1200_update_flag = true;
static bool lhbm_w200_update_flag = true;
//static bool lhbm_g500_update_flag = true;
static bool lhbm_w1200_readbackdone;
static bool lhbm_w200_readbackdone;
//static bool lhbm_g500_readbackdone;
struct LHBM_WHITEBUF {
	unsigned char lhbm_1200[6];
	unsigned char lhbm_200[6];
	//unsigned char lhbm_500[6];
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


#define FHD_FRAME_WIDTH    (1280)
#define FHD_FRAME_HEIGHT   (2772)

#define FHD_HFP_120            (69)
#define FHD_HSA_120            (18)
#define FHD_HBP_120            (20)
#define FHD_HTOTAL_120         (FHD_FRAME_WIDTH + FHD_HFP_120 + FHD_HSA_120 + FHD_HBP_120)

#define FHD_VFP_120            (56)
#define FHD_VSA_120            (2)
#define FHD_VBP_120            (18)
#define FHD_VTOTAL_120         (FHD_FRAME_HEIGHT + FHD_VFP_120 + FHD_VSA_120 + FHD_VBP_120)


#define FHD_HFP_90            (69)
#define FHD_HSA_90            (18)
#define FHD_HBP_90            (20)
#define FHD_HTOTAL_90         (FHD_FRAME_WIDTH + FHD_HFP_90 + FHD_HSA_90 + FHD_HBP_90)

#define FHD_VFP_90            (988)
#define FHD_VSA_90            (2)
#define FHD_VBP_90            (18)
#define FHD_VTOTAL_90         (FHD_FRAME_HEIGHT + FHD_VFP_90 + FHD_VSA_90 + FHD_VBP_90)


#define FHD_HFP_60            (69)
#define FHD_HSA_60            (18)
#define FHD_HBP_60            (20)
#define FHD_HTOTAL_60         (FHD_FRAME_WIDTH + FHD_HFP_60 + FHD_HSA_60 + FHD_HBP_60)

#define FHD_VFP_60            (2904)
#define FHD_VSA_60            (2)
#define FHD_VBP_60            (18)
#define FHD_VTOTAL_60         (FHD_FRAME_HEIGHT + FHD_VFP_60 + FHD_VSA_60 + FHD_VBP_60)

#define FHD_HFP_30            (1675)
#define FHD_HSA_30            (18)
#define FHD_HBP_30            (20)
#define FHD_HTOTAL_30         (FHD_FRAME_WIDTH + FHD_HFP_30 + FHD_HSA_30 + FHD_HBP_30)

#define FHD_VFP_30            (56)
#define FHD_VSA_30            (2)
#define FHD_VBP_30            (18)
#define FHD_VTOTAL_30         (FHD_FRAME_HEIGHT + FHD_VFP_30 + FHD_VSA_30 + FHD_VBP_30)

#define FHD_FRAME_TOTAL_120    (FHD_HTOTAL_120 * FHD_VTOTAL_120)
#define FHD_FRAME_TOTAL_90    (FHD_HTOTAL_90 * FHD_VTOTAL_90)
#define FHD_FRAME_TOTAL_60    (FHD_HTOTAL_60 * FHD_VTOTAL_60)
#define FHD_FRAME_TOTAL_30    (FHD_HTOTAL_30 * FHD_VTOTAL_30)

#define FHD_VREFRESH_120   (120)
#define FHD_VREFRESH_90    (90)
#define FHD_VREFRESH_60    (60)
#define FHD_VREFRESH_30    (30)

#define FHD_CLK_120_X10    ((FHD_FRAME_TOTAL_120 * FHD_VREFRESH_120) / 100)
#define FHD_CLK_90_X10     ((FHD_FRAME_TOTAL_90 * FHD_VREFRESH_90) / 100)
#define FHD_CLK_60_X10     ((FHD_FRAME_TOTAL_60 * FHD_VREFRESH_60) / 100)
#define FHD_CLK_30_X10     ((FHD_FRAME_TOTAL_30 * FHD_VREFRESH_30) / 100)

#define FHD_CLK_120		(((FHD_CLK_120_X10 % 10) != 0) ?             \
			(FHD_CLK_120_X10 / 10 + 1) : (FHD_CLK_120_X10 / 10))
#define FHD_CLK_90		(((FHD_CLK_90_X10 % 10) != 0) ?              \
			(FHD_CLK_90_X10 / 10 + 1) : (FHD_CLK_90_X10 / 10))
#define FHD_CLK_60		(((FHD_CLK_60_X10 % 10) != 0) ?              \
			(FHD_CLK_60_X10 / 10 + 1) : (FHD_CLK_60_X10 / 10))
#define FHD_CLK_30		(((FHD_CLK_30_X10 % 10) != 0) ?              \
			(FHD_CLK_30_X10 / 10 + 1) : (FHD_CLK_30_X10 / 10))

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
	/*LHBM GAMMA CODE*/
	{0xF0, 5, {0x55,0xAA,0x52,0x08,0x00} },
	{0xDF, 2, {0x21,0x54} },
	{0xFF, 4, {0xAA,0x55,0xA5,0x80} },
	{0x6F, 1, {0x08} },
	{0xF6, 1, {0x77} },
	{0x6F, 1, {0x0A} },
	{0xF6, 2, {0x77,0x77} },
	{0x6F, 1, {0x0E} },
	{0xF6, 1, {0x77} },
	{0x6F, 1, {0x0C} },
	{0xF6, 2, {0x77,0x77} },
	{0x6F, 1, {0x0F} },
	{0xF6, 1, {0x77} },
	{0xF0, 5, {0x55,0xAA,0x52,0x08,0x01} },
	{0xEA, 1, {0x91} },
	{0x6F, 1, {0x07} },
	{0xEA, 4, {0x01,0x3F,0x01,0x3F} },
	{0x6F, 1, {0x11} },
	{0xEA, 1, {0x91} },
	{0x6F, 1, {0x18} },
	{0xEA, 4, {0x01,0x68,0x01,0x68} },
	{0x6F, 1, {0x04} },
	{0xC3, 1, {0x0A} },
	{0x6F, 1, {0x09} },
	{0xC3, 1, {0x0A} },
	{0xFF, 4, {0xAA,0x55,0xA5,0x84} },
	{0xF2, 1, {0x15} },
	{0xFF, 4, {0xAA,0x55,0xA5,0x80} },
	{0x6F, 1, {0x04} },
	{0xF7, 1, {0x04} },
	{0x35, 1, {0x00} },
	{0x3B, 4, {0x00,0x14,0x00,0x38} },
	{0x6F, 1, {0x04} },
	{0x3B, 4, {0x00,0x14,0x03,0xDC} },
	{0x6F, 1, {0x08} },
	{0x3B, 4, {0x00,0x14,0x0B,0x58} },
	{0x51, 2, {0x00,0x00} },
	{0x71, 1, {0x00} },
	{0x8D, 8, {0x00,0x00,0x04,0xFF,0x00,0x00,0x0A,0xD3} },
	/*AOD demura On*/
	{0x17, 1, {0x03} },
	{0xF0, 5, {0x55,0xAA,0x52,0x08,0x01} },
	{0x6F, 1, {0x0B} },
	{0xD2, 1, {0x00} },
	{0x03, 1, {0x01} },
	{0x90, 2, {0x03,0x43} },
	{0x91, 18, {0xAB,0xA8,0x00,0x0C,0xC2,0x00,0x02,0x41,0x01,0x38,0x00,0x08,0x08,0xBB,0x07,0x21,0x10,0xF0} },
	{0x5F, 1, {0x00} },
	{0x81, 2, {0x01,0x19} },
	{0x2F, 1, {0x00} },
	{0x55, 1, {0x04} },
	{0x53, 1, {0x20} },
	{0x88, 5, {0x01,0x02,0x80,0x09,0xD6} },
	{0xF0, 5, {0x55,0xAA,0x52,0x08,0x08} },
	{0x6F, 1, {0x5F} },
	{0xB8, 1, {0x01} },
	{0xFF, 4, {0xAA,0x55,0xA5,0x84} },
	{0x6F, 1, {0x10} },
	{0xF8, 1, {0x02} },
	{0xFF, 4, {0xAA,0x55,0xA5,0x80} },
	{0x6F, 1, {0x1B} },
	{0XF4, 1, {0x04} },
	/*Optimize OSC Jitter*/
	{0xFF, 4, {0xAA,0x55,0xA5,0x80} },
	{0x6F, 1, {0x47} },
	{0xF2, 1, {0x21} },
	{0xF0, 5, {0x55,0xAA,0x52,0x08,0x01} },
	{0x6F, 1, {0x01} },
	{0xC5, 1, {0x02} },
	/*ESD PPS block on*/
	{0xF0, 5, {0x55,0xAA,0x52,0x08,0x00} },
	{0x6F, 1, {0x0F} },
	{0xD8, 1, {0x42} },
	/*DDIC config MIPI HS*/
	{0xF0, 5, {0x55,0xAA,0x52,0x08,0x00} },
	{0x6F, 1, {0xD4} },
	{0xBA, 1, {0x20} },
	/*Hs Drop*/
	{0xF0, 5, {0x55,0xAA,0x52,0x08,0x03} },
	{0xC6, 1, {0x87} },
	/*PMIC ELVSS jitter*/
	{0xF0, 5, {0x55,0xAA,0x52,0x08,0x01} },
	{0x6F, 1, {0x30} },
	{0xB9, 2, {0x10,0x01} },
	{0x6F, 1, {0x32} },
	{0xB9, 3, {0x00,0x5E,0x68} },
	{0x6F, 1, {0x0C} },
	{0xD8, 2, {0x40,0xA8} },
	/*open esd detect */
	{0xF0, 5, {0x55,0xAA,0x52,0x08,0x00} },
	{0xBE, 2, {0x47,0X45} },
	/*normal high,active low*/
	{0x6F, 1, {0x05} },
	{0xBE, 1, {0x88} },
	/*closed AOD watch dog*/
	{0xF0, 5, {0x55,0xAA,0x52,0x08,0x01} },
	{0x6F, 1, {0x09} },
	{0xD2, 1, {0x00} },
	/*lv detect on*/
	{0xF0, 5, {0x55,0xAA,0x52,0x08,0x01} },
	{0x6F, 1, {0x03} },
	{0xC7, 1, {0x47} },
	/*STV Abnormal Power off*/
	{0xF0, 5, {0x55,0xAA,0x52,0x08,0x05} },
	{0xCA, 1, {0x13} },
	{0xCB, 5, {0x13,0x13,0x13,0x13,0x13} },
	{0xF0, 5, {0x55,0xAA,0x52,0x08,0x03} },
	{0xB6, 1, {0x33} },
	/*Auto Write SR Quad Protect enable */
	{0xF0, 5, {0x55,0xAA,0x52,0x08,0x04} },
	{0x6F, 1, {0x02} },
	{0xC7, 2, {0x00,0x42} },
	{0x6F, 1, {0x08} },
	{0xC7, 1, {0x04} },
	{REGFLAG_DELAY, 5, {} },
	/*MTP Flash lock*/
	{0xF0, 5, {0x55,0xAA,0x52,0x08,0x04} },
	{0x6F, 1, {0x01} },
	{0xC7, 1, {0x11} },
	{0x6F, 1, {0x02} },
	{0xC7, 6, {0x00,0x42,0x00,0x42,0x00,0x42} },
	{0x6F, 1, {0x09} },
	{0xC7, 2, {0x00,0x42} },
	/*AVEE*/
	{0xF0, 5, {0x55,0xAA,0x52,0x08,0x01} },
	{0x6F, 1, {0x23} },
	{0xD8, 1, {0x67} },

	{0x11, 0,  {} },
	{REGFLAG_DELAY, 120, {} },
	{0x29, 0, {} },
	{0xF0,5,{0x55,0xAA,0x52,0x08,0x04}},
	{0x6F,1,{0x00}},
	{0xE3,12,{0xA1,0x7E,0x5A,0x43,0x32,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{0x6F,1,{0x0C}},
	{0xE3,12,{0xFF,0xFF,0xE1,0xA6,0x7B,0x10,0x0A,0x0A,0x08,0x08,0x08,0x08}},
	{0x6F,1,{0x18}},
	{0xE3,12,{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x80,0x80,0x80}},
	{0x6F,1,{0x24}},
	{0xE3,12,{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x80,0x80,0x80}},
	{0x6F,1,{0x30}},
	{0xE3,12,{0xA1,0x7E,0x5A,0x43,0x32,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {}},
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x51, 06, {0x00,0x00,0x00,0x00,0x00,0x00}},
	{REGFLAG_DELAY, 34, {} },
	{0xA9, 19, {0x02,0x03,0xC6,0x2D,0x2E,0x05,0x90,0x01,0x00,0x28,0x00,0x00,0x00,0x02,0x0A,0xD0,0x03,0x03,0xA1}},
	{0xA9, 6, {0x02,0x0A,0xD0,0x00,0x00,0x04}},
	{REGFLAG_DELAY, 50, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 100, {} },
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
static u8 lhbm_cmdbuf_1200nit[6] = {0};
static u8 lhbm_cmdbuf_200nit[6] = {0};

static struct LCM_setting_table lhbm_normal_white_1200nit[] = {
	//CMD2 Page 8
	{0xF0, 05, {0x55,0xAA,0x52,0x08,0x00}},
	{0xDF, 02, {0x21,0x54}},
	{0xF0, 05, {0x55,0xAA,0x52,0x08,0x02}},
	/*gamma value*/
	{0xD1, 06, {0x00,0x00,0x00,0x00,0x00,0x00}},
	/*enter LHBM*/
	{0x6F, 01, {0x01}},
	{0x8B, 01, {0x01}},
	{0x87, 01, {0x25}},
};

static struct LCM_setting_table lhbm_normal_white_200nit[] = {
	//CMD2 Page 8
	{0xF0, 05, {0x55,0xAA,0x52,0x08,0x00}},
	{0xDF, 02, {0x21,0x54}},
	{0xF0, 05, {0x55,0xAA,0x52,0x08,0x02}},
	/*gamma value*/
	{0xD1, 06, {0x00,0x00,0x00,0x00,0x00,0x00}},
	/*enter LHBM*/
	{0x6F, 01, {0x01}},
	{0x8B, 01, {0x01}},
	{0x87, 01, {0x25}},
};

static struct LCM_setting_table lhbm_off[] = {
	{0x51, 2, {0x05, 0xFF}},
	/*exit LHBM*/
	{0x87, 01, {0x00}},
	{0x6F, 01, {0x01}},
	{0x8B, 01, {0x00}},
};

static struct LCM_setting_table lcm_aod_high_mode[] = {
	{0x51, 06, {0x03,0x28,0x00,0x00,0x07,0xFF}},
};
static struct LCM_setting_table lcm_aod_low_mode[] = {
	{0x51, 06, {0x00,0x38,0x00,0x00,0x01,0xFF}},
};

#if 0
static struct LCM_setting_table bl_tb0[] = {
	{0x51, 02, {0x00,0x08}},
};

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
#endif
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

#endif // end of _PANEL_O12A_36_02_0A_DSC_VDO_H_
