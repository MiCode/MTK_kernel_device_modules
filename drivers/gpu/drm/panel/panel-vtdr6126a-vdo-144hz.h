/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef _PANEL_VTDR6126A_VDO_H_
#define _PANEL_VTDR6126A_VDO_H_

//#define LCM_DSI_CMD_MODE 1

//peng add
#define MAX_BRIGHTNESS (3615)
//#define FHD_FRAME_WIDTH (1260)
//#define FHD_FRAME_HEIGHT (2800)
#define VFHD_FRAME_WIDTH     (1080)
#define VFHD_FRAME_HEIGHT    (2400)
#define MIPI_DATA_RATE_120HZ (1532)

//#define PHYSICAL_WIDTH (70680)
//#define PHYSICAL_HEIGHT (157100)

/* maximum delay 255ms once, beyone 255 will be abnormal */
#define REGFLAG_MDELAY 0x0
#define HFP (140)
//#define HSA (10)
//#define HBP (40)
#define VFP (148)
//#define VSA (2)
//#define VBP (38)
/*FHD DSC setting*/
#define FHD_DSC_ENABLE                  1
#define FHD_DSC_VER                     1
#define FHD_DSC_SLICE_MODE              1
#define FHD_DSC_RGB_SWAP                0
#define FHD_DSC_DSC_CFG                 34
#define FHD_DSC_RCT_ON                  1
#define FHD_DSC_BIT_PER_CHANNEL         8
#define FHD_DSC_DSC_LINE_BUF_DEPTH      9
#define FHD_DSC_BP_ENABLE               1
#define FHD_DSC_BIT_PER_PIXEL           128
#define FHD_DSC_SLICE_HEIGHT            20
#define FHD_DSC_SLICE_WIDTH             630
#define FHD_DSC_CHUNK_SIZE              630
#define FHD_DSC_XMIT_DELAY              512
#define FHD_DSC_DEC_DELAY               571
#define FHD_DSC_SCALE_VALUE             32
#define FHD_DSC_INCREMENT_INTERVAL      526
#define FHD_DSC_DECREMENT_INTERVAL      8
#define FHD_DSC_LINE_BPG_OFFSET         12
#define FHD_DSC_NFL_BPG_OFFSET          1294
#define FHD_DSC_SLICE_BPG_OFFSET        1116
#define FHD_DSC_INITIAL_OFFSET          6144
#define FHD_DSC_FINAL_OFFSET            4336
#define FHD_DSC_FLATNESS_MINQP          3
#define FHD_DSC_FLATNESS_MAXQP          12
#define FHD_DSC_RC_MODEL_SIZE           8192
#define FHD_DSC_RC_EDGE_FACTOR          6
#define FHD_DSC_RC_QUANT_INCR_LIMIT0    11
#define FHD_DSC_RC_QUANT_INCR_LIMIT1    11
#define FHD_DSC_RC_TGT_OFFSET_HI        3
#define FHD_DSC_RC_TGT_OFFSET_LO        3

enum MODE_ID {
	FHD_120 = 0,
	FHD_90 = 1,
	FHD_60 = 2,
	MODE_NUM,
};

struct LCD_setting_table {
	unsigned char count;
	unsigned char para_list[200];
};

static struct LCD_setting_table cmd_bl_level[] = {
	{ 0x03, {0x51, 0x07, 0xFF} },
};

static struct LCD_setting_table cmd_brightness_dimming_off_dly_off[] = {
	{0x02, {0x53, 0x20}},
};

static struct LCD_setting_table cmd_brightness_dimming_on_dly_on[] = {
	{0x02, {0x53, 0x28}},
};

static struct LCD_setting_table cmd_brightness_dimming_off_dly_on[] = {
	{0x02, {0x53, 0x20}},
};

static struct LCD_setting_table seed_crc_p3[] = {
};

static struct LCD_setting_table seed_scr_srgb[] = {
};

static struct LCD_setting_table seed_crc_off[] = {
};

static struct LCD_setting_table cmd_aod_on_osc120hz[] = {

};

static struct LCD_setting_table cmd_aod_on_osc90hz[] = {
};

static struct LCD_setting_table cmd_aod_on_osc60hz[] = {

};

static struct LCD_setting_table cmd_aod_on_30fps[] = {
};

static struct LCD_setting_table cmd_aod_on_1fps[] = {
};

static struct LCD_setting_table cmd_aod_on_display_on[] = {

};

static struct LCD_setting_table cmd_local_hbm_on[] = {

};

static struct LCD_setting_table cmd_bl_alpha[] = {

};

static struct LCD_setting_table cmd_local_hbm_off[] = {

};

static struct LCD_setting_table cmd_aod_off_120hz[] = {

};
static struct LCD_setting_table cmd_aod_off_90hz[] = {

};
static struct LCD_setting_table cmd_aod_off_60hz[] = {

};

static struct LCD_setting_table cmd_aod_off_auto_mode[] = {
};

static struct LCD_setting_table cmd_hbm_on_dimming[] = {
};

static struct LCD_setting_table cmd_hbm_on_no_dimming[] = {
};

static struct LCD_setting_table cmd_hbm_off_dimming[] = {
};

static struct LCD_setting_table cmd_hbm_off_no_dimming[] = {
};

/* p3 seed crc compenstate default 550~2047 */
static struct LCD_setting_table sm3010a_seed_crc_compensate_p3_level0_cmd[] = {

};

/* p3 seed crc compenstate 100nit 250~550 */
static struct LCD_setting_table sm3010a_seed_crc_compensate_p3_level1_cmd[] = {

};

/* p3 seed crc compenstate 20nit 50~250 */
static struct LCD_setting_table sm3010a_seed_crc_compensate_p3_level2_cmd[] = {
};

/* p3 seed crc compenstate 3nit 4~50 */
static struct LCD_setting_table sm3010a_seed_crc_compensate_p3_level3_cmd[] = {
};

/* srgb seed crc compenstate default 550~2047 */
static struct LCD_setting_table sm3010a_seed_crc_compensate_srgb_level0_cmd[] = {
};

/* srgb seed crc compenstate 100nit 250~550 */
static struct LCD_setting_table sm3010a_seed_crc_compensate_srgb_level1_cmd[] = {
};

/* srgb seed crc compenstate 20nit 50~250 */
static struct LCD_setting_table sm3010a_seed_crc_compensate_srgb_level2_cmd[] = {
};

/* srgb seed crc compenstate 3nit 4~50 */
static struct LCD_setting_table sm3010a_seed_crc_compensate_srgb_level3_cmd[] = {
};

static struct LCD_setting_table cmd_set_fps_120hz[] =  {

};

static struct LCD_setting_table cmd_set_fps_90hz[] =  {

};

static struct LCD_setting_table cmd_set_fps_60hz[] =  {

};
#endif
