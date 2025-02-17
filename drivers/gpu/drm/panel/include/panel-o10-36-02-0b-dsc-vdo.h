/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2020 XiaoMi, Inc. All rights reserved.
 */

#ifndef _PANEL_O10_36_02_0B_DSC_VDO_H_
#define _PANEL_O10_36_02_0B_DSC_VDO_H_

#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <drm/mediatek_drm.h>
#include "mi_panel_ext.h"
#include "mi_dsi_panel_count.h"


#define REGFLAG_CMD			0xFFFA
#define REGFLAG_DELAY			0xFFFC
#define REGFLAG_UDELAY			0xFFFB
#define REGFLAG_END_OF_TABLE    	0xFFFD
#define REGFLAG_RESET_LOW		0xFFFE
#define REGFLAG_RESET_HIGH		0xFFFF
#define DATA_RATE			1100

#define FRAME_WIDTH			(1220)
#define FRAME_HEIGHT			(2712)

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
#define DSC_SLICE_WIDTH             610
#define DSC_CHUNK_SIZE              610
#define DSC_XMIT_DELAY              512
#define DSC_DEC_DELAY               562
#define DSC_SCALE_VALUE             32
#define DSC_INCREMENT_INTERVAL      305
#define DSC_DECREMENT_INTERVAL      8
#define DSC_LINE_BPG_OFFSET         12
#define DSC_NFL_BPG_OFFSET          2235
#define DSC_SLICE_BPG_OFFSET        1915
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
#define NORMAL_HW_BRIGHTNESS	2047

#define PHYSICAL_WIDTH              69540
#define PHYSICAL_HEIGHT             154584

/* Set delay of Vsync */
/* Send VSYNC after the lines following frame done */
#define PREFETCH_TIME          (0)


// Notice start
// when modifying the initial code should modify it.
#define FPS_INIT_INDEX 0
#if 0
#define TEMP_INDEX 58
// Notice end

#define TEMP_INDEX1_36 0x01
#define TEMP_INDEX2_32 0x20
#define TEMP_INDEX3_28 0x40
#define TEMP_INDEX4_off 0x80
#endif

static atomic_t doze_enable = ATOMIC_INIT(0);
static const int panel_id = PANEL_1ST;
static const int doze_hbm_dbv_level = 245;
static const int doze_lbm_dbv_level = 20;
static int current_fps = 120;
static struct lcm *panel_ctx;
static struct drm_panel * this_panel = NULL;
static int normal_max_bl = 2047;
static unsigned int last_non_zero_bl_level = 511;
static int PEAK_HDR_BL_LEVEL = 4094;
static int PEAK_MAX_BL_LEVEL = 4095;
static char oled_wp_cmdline[18] = {0};
static const char *panel_name = "panel_name=dsi_o10_36_02_0b_dsc_vdo";

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


#define FHD_FRAME_WIDTH    (1220)
#define FHD_FRAME_HEIGHT   (2712)

#define FHD_HFP_120            (44)
#define FHD_HSA_120            (6)
#define FHD_HBP_120            (6)
#define FHD_HTOTAL_120         (FHD_FRAME_WIDTH + FHD_HFP_120 + FHD_HSA_120 + FHD_HBP_120)

#define FHD_VFP_120            (56)
#define FHD_VSA_120            (4)
#define FHD_VBP_120            (44)
#define FHD_VTOTAL_120         (FHD_FRAME_HEIGHT + FHD_VFP_120 + FHD_VSA_120 + FHD_VBP_120)


#define FHD_HFP_90            (44)
#define FHD_HSA_90            (6)
#define FHD_HBP_90            (6)
#define FHD_HTOTAL_90         (FHD_FRAME_WIDTH + FHD_HFP_90 + FHD_HSA_90 + FHD_HBP_90)

#define FHD_VFP_90            (996)
#define FHD_VSA_90            (4)
#define FHD_VBP_90            (44)
#define FHD_VTOTAL_90         (FHD_FRAME_HEIGHT + FHD_VFP_90 + FHD_VSA_90 + FHD_VBP_90)

#define FHD_HFP_60            (44)
#define FHD_HSA_60            (6)
#define FHD_HBP_60            (6)
#define FHD_HTOTAL_60         (FHD_FRAME_WIDTH + FHD_HFP_60 + FHD_HSA_60 + FHD_HBP_60)

#define FHD_VFP_60            (2870)
#define FHD_VSA_60            (4)
#define FHD_VBP_60            (44)
#define FHD_VTOTAL_60         (FHD_FRAME_HEIGHT + FHD_VFP_60 + FHD_VSA_60 + FHD_VBP_60)

#define FHD_HFP_30            (1650)
#define FHD_HSA_30            (6)
#define FHD_HBP_30            (6)
#define FHD_HTOTAL_30         (FHD_FRAME_WIDTH + FHD_HFP_30 + FHD_HSA_30 + FHD_HBP_30)

#define FHD_VFP_30            (56)
#define FHD_VSA_30            (4)
#define FHD_VBP_30            (44)
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
	{0x48, 1, {0x23} },
	{0x51, 2, {0x00, 0x00} },
	{0x53, 1, {0xE0} },
	{0x35, 1, {0x00} },
	{0x11, 0, {} },
	{REGFLAG_DELAY, 85, {} },

	{0x9C, 2, {0xA5, 0xA5} },
	{0xFD, 2, {0x5A, 0x5A} },
	{0x9F, 1, {0x0F} },
	{0xD7, 1, {0x33} },
	{0x9F, 1, {0x02} },
	{0xB3, 4, {0x03, 0x00, 0x04,0x05} },
	{0xED, 3, {0x00, 0x01, 0x80} },
	{0xEE, 17, {0x05, 0x00, 0x00, 0x00, 0x08, 0x88, 0x00, 0x00, 0x00, 0x30, 0x88, 0x88, 0x29, 0x00, 0x00, 0x00, 0x00} },
	{0x9F, 1, {0x01} },
	{0xB4, 4, {0x10, 0x00, 0xB8, 0x03} },
	{0xE2, 2, {0x46, 0x20} },
	{0xE4, 8, {0x00, 0x00, 0x80, 0x0C, 0xFF, 0x0F, 0x08, 0x00} },

	{0x9F, 1, {0x01} },
	{0xCA, 20, {0x05, 0x01, 0x00, 0x02, 0x02, 0x03, 0xB2, 0x03, 0x03, 0x03, 0xB2, 0x03, 0x03, 0x03, 0xB2, 0x03, 0x03, 0x88, 0xAA, 0x81} },
	{0xCB, 15, {0x01, 0x02, 0x02, 0x03, 0xB2, 0x03, 0x03, 0x03, 0xB2, 0x03, 0x03, 0x03, 0xB2, 0x03, 0x03} },
	{0xD3, 2, {0xA0, 0x00} },

	{0x9F, 1, {0x0B} },
	{0xB2, 1, {0x01} },
	{0x9C, 2, {0x5A, 0x5A} },
	{0xFD, 2, {0xA5, 0xA5} },

	{0x29, 0, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {}},
};


static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x51, 02, {0x00,0x00}},
	{REGFLAG_DELAY, 34, {} },
	{0x28, 0, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {}},
};

static struct LCM_setting_table gray_3d_lut[] = {
	{0x57, 01, {0x00}},
};

#if 0
static struct LCM_setting_table mode_120hz_setting_gir_off[] = {
	/* frequency select 120hz */
	{0x90, 01, {0x00}},
	{0x48, 01, {0x23}},
	{0x9C, 2, {0xA5,0xA5} },
	{0xFD, 2, {0x5A,0x5A} },
	{0x9F, 1, {0x08} },
	{0xB2, 1, {0x10} },
	{0x9C, 2, {0x5A,0x5A} },
	{0xFD, 2, {0xA5,0xA5} },
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_120hz_setting_gir_on[] = {
	/* frequency select 120hz */
	{0x90, 01, {0x00}},
	{0x48, 01, {0x23}},
	{0x9C, 2, {0xA5,0xA5} },
	{0xFD, 2, {0x5A,0x5A} },
	{0x9F, 1, {0x08} },
	{0xB2, 1, {0x12} },
	{0x9C, 2, {0x5A,0x5A} },
	{0xFD, 2, {0xA5,0xA5} },
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_90hz_setting_gir_off[] = {
	/* frequency select 90hz */
	{0x90, 01, {0x00}},
	{0x48, 01, {0x03}},
	{0x9C, 2, {0xA5,0xA5} },
	{0xFD, 2, {0x5A,0x5A} },
	{0x9F, 1, {0x08} },
	{0xB2, 1, {0x10} },
	{0x9C, 2, {0x5A,0x5A} },
	{0xFD, 2, {0xA5,0xA5} },
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_90hz_setting_gir_on[] = {
	/* frequency select 90hz */
	{0x90, 01, {0x00}},
	{0x48, 01, {0x03}},
	{0x9C, 2, {0xA5,0xA5} },
	{0xFD, 2, {0x5A,0x5A} },
	{0x9F, 1, {0x08} },
	{0xB2, 1, {0x12} },
	{0x9C, 2, {0x5A,0x5A} },
	{0xFD, 2, {0xA5,0xA5} },
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_60hz_setting_gir_off[] = {
	/* frequency select 60hz */
	{0x90, 01, {0x00}},
	{0x48, 01, {0x13}},
	{0x9C, 2, {0xA5,0xA5} },
	{0xFD, 2, {0x5A,0x5A} },
	{0x9F, 1, {0x08} },
	{0xB2, 1, {0x10} },
	{0x9C, 2, {0x5A,0x5A} },
	{0xFD, 2, {0xA5,0xA5} },
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_60hz_setting_gir_on[] = {
	/* frequency select 60hz */
	{0x90, 01, {0x00}},
	{0x48, 01, {0x13}},
	{0x9C, 2, {0xA5,0xA5} },
	{0xFD, 2, {0x5A,0x5A} },
	{0x9F, 1, {0x08} },
	{0xB2, 1, {0x12} },
	{0x9C, 2, {0x5A,0x5A} },
	{0xFD, 2, {0xA5,0xA5} },
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
#endif

static struct LCM_setting_table lhbm_normal_white_1200nit[] = {
	{0x97, 4, {0x11, 0x00, 0x00, 0x00}},
};

static struct LCM_setting_table lhbm_normal_white_200nit[] = {
	{0x97, 4, {0x11, 0x00, 0x00, 0x01}},
};

static struct LCM_setting_table lhbm_normal_green_500nit[] = {
	{0x97, 4, {0x11, 0x00, 0x00, 0x02}},
};

static struct LCM_setting_table lhbm_off[] = {
	{0x51, 2, {0x05, 0xFF}},
	{0x97, 3, {0x00, 0x0F, 0xFF}},
};
static struct LCM_setting_table lhbm_off_delay[] = {
	{0x51, 2, {0x00, 0x00}},
	{0x97, 3, {0x00, 0x0F, 0xFF}},
	{REGFLAG_DELAY, 17, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {}},
};
static struct LCM_setting_table lcm_aod_high_mode[] = {
	{0x51, 02, {0x03,0xFF}},
};
static struct LCM_setting_table lcm_aod_low_mode[] = {
	{0x51, 02, {0x01,0xFF}},
};
static struct LCM_setting_table lcm_aod_high_mode_normal[] = {
	{0x51, 02, {0x00, 0xF5}},
};
static struct LCM_setting_table lcm_aod_low_mode_normal[] = {
	{0x51, 02, {0x00, 0x14}},
};
#if 0
static struct LCM_setting_table bl_tb0[] = {
	{0x51, 02, {0x00,0x08}},
};
#endif
static struct LCM_setting_table apb_mode_off[] = {
	{0x9C, 2, {0xA5, 0xA5}},
	{0xFD, 2, {0x5A, 0x5A}},
	{0x9F, 1, {0x08}},
	{0xCC, 3, {0x02,0xFF,0x18}},
	{0x9C, 2, {0x5A, 0x5A}},
	{0xFD, 2, {0xA5, 0xA5}},
};
static struct LCM_setting_table apb_mode_on[] = {
	{0x48, 1, {0x33}},
	{0x51, 1, {0x0F, 0xFF}},
	{0x9C, 2, {0xA5, 0xA5}},
	{0xFD, 2, {0x5A, 0x5A}},
	{0x9F, 1, {0x08}},
	{0xB2, 1, {0x12}},
	{0xCC, 3, {0x03,0xFF,0x18}},
	{0x9C, 2, {0x5A, 0x5A}},
	{0xFD, 2, {0xA5, 0xA5}},
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
	{0x90, 01, {0x00}},
	{0x51, 02, {0x00, 0x00}},
	{REGFLAG_DELAY, 34, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {}},

};
static unsigned int rc_buf_thresh[14] = {896, 1792, 2688, 3584, 4480, 5376, 6272, 6720, 7168, 7616, 7744, 7872, 8000, 8064};
static unsigned int range_min_qp[15] = {0, 4, 5, 5, 7, 7, 7, 7, 7, 7, 9, 9, 9, 13, 16};
static unsigned int range_max_qp[15] = {8, 8, 9, 10, 11, 11, 11, 12, 13, 14, 14, 15, 15, 16, 17};
static int range_bpg_ofs[15] = {2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -12, -12, -12, -12};

#endif // end of _PANEL_O10_36_02_0B_DSC_VDO_H_
