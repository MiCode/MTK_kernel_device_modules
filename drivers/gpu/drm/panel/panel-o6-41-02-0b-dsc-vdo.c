// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/backlight.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <linux/delay.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
/*O6 code for HQ-391992 by zhaobeidou at 2024/06/26 start*/
#include <linux/hqsysfs.h>
/*O6 code for HQ-391992 by zhaobeidou at 2024/06/26 end*/
#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/v1/mtk_drm_graphics_base.h"
#endif
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mediatek_v2/v1/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif
#include "../mediatek/mediatek_v2/v1/mi_disp/mi_panel_ext.h"
#include "../mediatek/mediatek_v2/v1/mi_disp/mi_dsi_panel.h"
#include "include/panel_alpha_data.h"
#include "../mediatek/mediatek_v2/v1/mtk_dsi.h"
#include "../mediatek/mediatek_v2/v1/mi_disp/mi_dsi_panel_count.h"

#define REGFLAG_CMD                 0xFFFA
#define REGFLAG_DELAY               0xFFFC
#define REGFLAG_UDELAY              0xFFFB
#define REGFLAG_END_OF_TABLE        0xFFFD
#define REGFLAG_RESET_LOW           0xFFFE
#define REGFLAG_RESET_HIGH          0xFFFF

#define DATA_RATE                   1016

#define FRAME_WIDTH                 (1080)
#define FRAME_HEIGHT                (2400)

#define DSC_ENABLE                  1
#define DSC_VER                     17
#define DSC_SLICE_MODE              1
#define DSC_RGB_SWAP                0
#define DSC_DSC_CFG                 34
#define DSC_RCT_ON                  1
#define DSC_BIT_PER_CHANNEL         8
#define DSC_DSC_LINE_BUF_DEPTH      9
#define DSC_BP_ENABLE               1
#define DSC_BIT_PER_PIXEL           128
#define DSC_SLICE_HEIGHT            20
#define DSC_SLICE_WIDTH             540
#define DSC_CHUNK_SIZE              540
#define DSC_XMIT_DELAY              512
#define DSC_DEC_DELAY               526
#define DSC_SCALE_VALUE             32
#define DSC_INCREMENT_INTERVAL      488
#define DSC_DECREMENT_INTERVAL      7
#define DSC_LINE_BPG_OFFSET         12
#define DSC_NFL_BPG_OFFSET          1294
#define DSC_SLICE_BPG_OFFSET        1302
#define DSC_INITIAL_OFFSET          6144
#define DSC_FINAL_OFFSET            4336
#define DSC_FLATNESS_MINQP          3
#define DSC_FLATNESS_MAXQP          12
#define DSC_RC_MODEL_SIZE           8192
#define DSC_RC_EDGE_FACTOR          6
#define DSC_RC_QUANT_INCR_LIMIT0    11
#define DSC_RC_QUANT_INCR_LIMIT1    11
#define DSC_RC_TGT_OFFSET_HI        3
#define DSC_RC_TGT_OFFSET_LO        3
#define PHYSICAL_WIDTH              69522
#define PHYSICAL_HEIGHT             154960
#define CMD_NUM_MAX                 20

#define REFRESH_AOD 30
static unsigned int rc_buf_thresh[14] = {896, 1792, 2688, 3584, 4480, 5376, 6272, 6720, 7168, 7616, 7744, 7872, 8000, 8064};
static unsigned int range_min_qp[15] = {0, 0, 1, 1, 3, 3, 3, 3, 3, 3, 5, 5, 5, 7, 13};
static unsigned int range_max_qp[15] = {4, 4, 5, 6, 7, 7, 7, 8, 9, 10, 11, 12, 13, 13, 15};
static int range_bpg_ofs[15] = {2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -12, -12, -12, -12};
//static struct mtk_ddic_dsi_msg *cmd_msg = NULL;
extern int get_panel_dead_flag(void);
static struct drm_panel *this_panel;
static int last_bl_level;
static int last_non_zero_bl_level = 511;
static int doze_lbm_bl = 29;
static int doze_hbm_bl = 248;
static bool lhbm_flag = false;
static bool fod_in_calibration = false;
static int bl_level_for_fod = 0;
static int hbm_level =2047;
enum lhbm_cmd_type {
	TYPE_WHITE_1200 = 0,
	TYPE_WHITE_750,
	TYPE_WHITE_500,
	TYPE_WHITE_200,
	TYPE_GREEN_500,
	TYPE_LHBM_OFF
};

/*struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *vddi_gpio;
	struct gpio_desc *vdd_gpio;
	struct gpio_desc *vci_gpio;
	bool prepared;
	bool enabled;
	unsigned int gate_ic;
	int error;
	int dynamic_fps;
	struct mutex panel_lock;
	struct mutex lhbm_lock;
	bool hbm_enabled;
	const char *panel_info;
	int gir_status;
	enum doze_brightness_state doze_brightness;
};*/

static const char *panel_name = "panel_name=dsi_o6_41_02_0b_dsc_vdo";

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

static struct LCM_setting_table local_hbm_normal_white_1200nit[] = {
	{0xA9, 23, {0x02, 0x00, 0xB5, 0x2C, 0x2C, 0x00, 0x01, 0x00, 0x87, 0x00, 0x05, 0x25, 0x3F, 0x50, 0x00, 0x00, 0x30, 0x01, 0x00, 0x8B, 0x00, 0x00, 0x10}},
};
static struct LCM_setting_table local_hbm_normal_white_200nit[] = {
	{0xA9, 23, {0x02, 0x00, 0xB5, 0x2C, 0x2C, 0x00, 0x01, 0x00, 0x87, 0x00, 0x05, 0x65, 0x38, 0x82, 0x00, 0x00, 0x40, 0x01, 0x00, 0x8B, 0x00, 0x00, 0x10}},
};
static struct LCM_setting_table local_hbm_normal_green_500nit[] = {
	{0xA9, 23, {0x02, 0x00, 0xB5, 0x2C, 0x2C, 0x00, 0x01, 0x00, 0x87, 0x00, 0x05, 0xA5, 0x03, 0xE1, 0x00, 0x00, 0x50, 0x01, 0x00, 0x8B, 0x00, 0x00, 0x10}},
};
static struct LCM_setting_table local_hbm_normal_off[] = {
	{0xA9, 21, {0x02, 0x00, 0xB5, 0x2C, 0x2C, 0x03, 0x01, 0x00, 0x87, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x8B, 0x00, 0x00, 0x00}},
	{0x51, 02, {0x07,0xff}},
};

static struct LCM_setting_table local_hbm_gir_on_120Hz[] = {
	{0x2f, 01, {0x00}},
	{0x5f, 01, {0x00}},
};
static struct LCM_setting_table local_hbm_gir_on_90Hz[] = {
	{0x2f, 01, {0x01}},
	{0x5f, 01, {0x00}},
};
static struct LCM_setting_table local_hbm_gir_on_60Hz[] = {
	{0x2f, 01, {0x02}},
	{0x5f, 01, {0x00}},
};
static struct LCM_setting_table local_hbm_gir_off_120Hz[] = {
	{0x2f, 01, {0x00}},
	{0x5f, 01, {0x01}},
};
static struct LCM_setting_table local_hbm_gir_off_90Hz[] = {
	{0x2f, 01, {0x01}},
	{0x5f, 01, {0x01}},
};
static struct LCM_setting_table local_hbm_gir_off_60Hz[] = {
	{0x2f, 01, {0x02}},
	{0x5f, 01, {0x01}},
};

static struct LCM_setting_table lcm_aod_hbm[] = {
	{0x51, 02, {0x00, 0xf8}},
};
static struct LCM_setting_table lcm_aod_lbm[] = {
	{0x51, 02, {0x00, 0x1d}},
};

static struct LCM_setting_table exit_hbm_to_normal[] = {
	{0x51, 02, {0x07,0xff}},
};

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

static void lcm_dcs_write(struct lcm *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;
	char *addr;
	if (ctx->error < 0)
		return;
	addr = (char *)data;
	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	else
		ret = mipi_dsi_generic_write(dsi, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}
/*
int panel_ddic_send_cmd(struct LCM_setting_table *table,
	unsigned int count, bool block)
{
	//int i = 0, j = 0, k = 0;
	int ret = 0;
	//unsigned char temp[25][255] = {0};
	//unsigned char cmd = {0};


	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 0,
		.flags = MIPI_DSI_MSG_USE_LPM,
		.tx_cmd_num = count,
	};

	if (table == NULL) {
		pr_err("invalid ddic cmd \n");
		return ret;
	}

	if (count == 0 || count > CMD_NUM_MAX) {
		pr_err("cmd count invalid, value:%d \n", count);
		return ret;
	}


	for (i = 0;i < count; i++) {
		memset(temp[i], 0, sizeof(temp[i]));
		// LCM_setting_table format: {cmd, count, {para_list[]}}
		cmd = (u8)table[i].cmd;
		temp[i][0] = cmd;
		for (j = 0; j < table[i].count; j++) {
			temp[i][j+1] = table[i].para_list[j];
		}

		cmd_msg.type[i] = table[i].count > 1 ? 0x39 : 0x15;
		cmd_msg.tx_buf[i] = temp[i];
		cmd_msg.tx_len[i] = table[i].count + 1;

		for (k = 0; k < cmd_msg.tx_len[i]; k++) {
			pr_info("%s cmd_msg.tx_buf:0x%02x\n", __func__, temp[i][k]);
		}

		pr_info("%s cmd_msg.tx_len:%zu\n", __func__, cmd_msg.tx_len[i]);
	}
	ret = mtk_ddic_dsi_send_cmd(&cmd_msg, block, false);
	if (ret != 0) {
		pr_err("%s: failed to send ddic cmd\n", __func__);
	} 
	return ret;
}
*/
static void lcm_panel_init(struct lcm *ctx)
{
	mutex_lock(&ctx->panel_lock);
	pr_info(" %s start \n", __func__);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(100);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(5);
	gpiod_set_value(ctx->reset_gpio, 0);
	mdelay(5);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(20);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	lcm_dcs_write_seq_static(ctx, 0xff, 0xaa, 0x55, 0xa5, 0x80);
	lcm_dcs_write_seq_static(ctx, 0x6f, 0x18);
	lcm_dcs_write_seq_static(ctx, 0xf4, 0x73);
	lcm_dcs_write_seq_static(ctx, 0x6f, 0x2a);
	lcm_dcs_write_seq_static(ctx, 0xf4, 0x08);
	lcm_dcs_write_seq_static(ctx, 0x6f, 0x1a);
	lcm_dcs_write_seq_static(ctx, 0xfe, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6f, 0x46);
	lcm_dcs_write_seq_static(ctx, 0xf4, 0x07, 0x09);
	lcm_dcs_write_seq_static(ctx, 0x6f, 0x4a);
	lcm_dcs_write_seq_static(ctx, 0xf4, 0x08, 0x0a);
	lcm_dcs_write_seq_static(ctx, 0x6f, 0x56);
	lcm_dcs_write_seq_static(ctx, 0xf4, 0x44, 0x44);
	lcm_dcs_write_seq_static(ctx, 0x6f, 0x0b);
	lcm_dcs_write_seq_static(ctx, 0xf5, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xff, 0xaa, 0x55, 0xa5, 0x81);
	lcm_dcs_write_seq_static(ctx, 0x6f, 0x3c);
	lcm_dcs_write_seq_static(ctx, 0xf5, 0x87);
	lcm_dcs_write_seq_static(ctx, 0x17, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x71, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x8d, 0x00, 0x00, 0x04, 0xc3, 0x00, 0x00, 0x0a, 0x97);
	lcm_dcs_write_seq_static(ctx, 0x88, 0x01, 0x02, 0x1d, 0x08, 0x75);
	lcm_dcs_write_seq_static(ctx, 0x2a, 0x00, 0x00, 0x04, 0x37);
	lcm_dcs_write_seq_static(ctx, 0x2b, 0x00, 0x00, 0x09, 0x5f);
	lcm_dcs_write_seq_static(ctx, 0x03, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x90, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x6f, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x90, 0x43);
	lcm_dcs_write_seq_static(ctx, 0x91, 0x89, 0x28, 0x00, 0x14, 0xC2, 0x00, 0x02,
				0x0E, 0x01, 0xE8, 0x00, 0x07, 0x05, 0x0E, 0x05, 0x16,
				0x10,0xF0);
	if (ctx->dynamic_fps == 120) {
		lcm_dcs_write_seq_static(ctx, 0x2f, 0x00);
	} else if (ctx->dynamic_fps == 90) {
		lcm_dcs_write_seq_static(ctx, 0x2f, 0x01);
	} else {
		lcm_dcs_write_seq_static(ctx, 0x2f, 0x02);
	}
	lcm_dcs_write_seq_static(ctx, 0x26, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x5f, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x53, 0x20);
	lcm_dcs_write_seq_static(ctx,0x3b, 0x00, 0x11, 0x00, 0x4f, 0x00, 0x11, 0x03,
				0x5b, 0x00, 0x11, 0x0a, 0x0f);
	lcm_dcs_write_seq_static(ctx, 0x6f, 0x10);
	lcm_dcs_write_seq_static(ctx, 0x3b, 0x00, 0x11, 0x00, 0x4f);
	lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x51, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6f, 0x04);
	lcm_dcs_write_seq_static(ctx, 0x51, 0x0f, 0xfe);
	lcm_dcs_write_seq_static(ctx, 0xf0, 0x55, 0xaa, 0x52, 0x08, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x6f, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xc7, 0xc7);
#ifdef  KERNEL_FACTORY_BUILD
	// round off
	lcm_dcs_write_seq_static(ctx, 0xf0, 0x55, 0xaa, 0x52, 0x08, 0x07);
	lcm_dcs_write_seq_static(ctx, 0xc0, 0x87);
	lcm_dcs_write_seq_static(ctx, 0x6f, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xc0, 0xaa, 0xaa, 0xaa, 0x55, 0x00, 0x00, 0x00, 0xaa, 0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00);
#endif
	lcm_dcs_write_seq_static(ctx, 0xf0, 0x55, 0xaa, 0x52, 0x08, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6f, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xb5, 0x34);
	lcm_dcs_write_seq_static(ctx, 0x11);
	mdelay(120);
	lcm_dcs_write_seq_static(ctx, 0x29);
	mutex_unlock(&ctx->panel_lock);
	pr_info(" %s end !\n", __func__);
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info(" %s start \n", __func__);
	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info(" %s start \n", __func__);
	if (!ctx->prepared)
		return 0;

	mutex_lock(&ctx->panel_lock);

	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(50);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(150);
	mutex_unlock(&ctx->panel_lock);

	udelay(2000);

	ctx->error = 0;
	ctx->prepared = false;

	return 0;
}
static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_err(" lcm_prepare 1 %s\n", __func__);
	if (ctx->prepared)
		return 0;

	pr_err(" lcm_prepare 2 %s\n", __func__);
	//devm_gpiod_put(ctx->dev, ctx->vdd_gpio);
	//udelay(1000);
	//pr_err(" lcm_prepare 3 %s\n", __func__);
	lcm_panel_init(ctx);
	pr_err(" lcm_prepare 4 %s\n", __func__);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;
	lhbm_flag = false;
	fod_in_calibration = false;

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif

#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

	pr_err(" lcm_prepare 5 %s\n", __func__);
	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info(" %s start \n", __func__);
	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}

#define HFP (50)
#define HSA (24)
#define HBP (50)
#define VFP (2575)
#define VSA (2)
#define VBP (15)
#define VAC (2400)
#define HAC (1080)

#define MODE_0_FPS 60
#define MODE_0_VFP 2575
#define MODE_0_HFP 50
#define MODE_0_DATA_RATE DATA_RATE

#define MODE_1_FPS 90
#define MODE_1_VFP 859
#define MODE_1_HFP 50
#define MODE_1_DATA_RATE DATA_RATE

#define MODE_2_FPS 120
#define MODE_2_VFP 79
#define MODE_2_HFP 50
#define MODE_2_DATA_RATE DATA_RATE
/* O6 code for HQ-392892 by p-zhangyundan at 2024/07/31 start */
#define MODE_3_FPS 30
#define MODE_3_VFP 79
#define MODE_3_HFP 1748
#define MODE_3_DATA_RATE DATA_RATE
/* O6 code for HQ-392892 by p-zhangyundan at 2024/07/31 end */
static u32 fake_heigh = 2400;
static u32 fake_width = 1080;
static bool need_fake_resolution;

static struct drm_display_mode default_mode = {
	.clock = (FRAME_WIDTH + MODE_0_HFP + HSA + HBP) *
		 (FRAME_HEIGHT + MODE_0_VFP + VSA + VBP) * MODE_0_FPS / 1000,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_0_HFP,
	.hsync_end = FRAME_WIDTH + MODE_0_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_0_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_0_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_0_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_0_VFP + VSA + VBP,
};

static struct drm_display_mode performance_mode_1 = {
	.clock = (FRAME_WIDTH + MODE_1_HFP + HSA + HBP) *
		 (FRAME_HEIGHT + MODE_1_VFP + VSA + VBP) * MODE_1_FPS / 1000,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_1_HFP,
	.hsync_end = FRAME_WIDTH + MODE_1_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_1_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_1_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_1_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_1_VFP + VSA + VBP,
};

static struct drm_display_mode performance_mode_2 = {
	.clock = (FRAME_WIDTH + MODE_2_HFP + HSA + HBP) *
		 (FRAME_HEIGHT + MODE_2_VFP + VSA + VBP) * MODE_2_FPS / 1000,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_2_HFP,
	.hsync_end = FRAME_WIDTH + MODE_2_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_2_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_2_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_2_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_2_VFP + VSA + VBP,
};

static struct drm_display_mode performance_mode_3 = {
	.clock = (FRAME_WIDTH + MODE_3_HFP + HSA + HBP) *
		 (FRAME_HEIGHT + MODE_3_VFP + VSA + VBP) * MODE_3_FPS / 1000,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_3_HFP,
	.hsync_end = FRAME_WIDTH + MODE_3_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_3_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_3_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_3_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_3_VFP + VSA + VBP,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info(" %s start \n", __func__);
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned char data[3] = {0x00, 0x00, 0x00};
	unsigned char id[3] = {0x40, 0x00, 0x00};
	ssize_t ret;

	ret = mipi_dsi_dcs_read(dsi, 0x4, data, 3);
	pr_info(" %s start \n", __func__);
	if (ret < 0) {
		pr_err("%s error\n", __func__);
		return 0;
	}

	pr_info("ATA read data %x %x %x\n", data[0], data[1], data[2]);

	if (data[0] == id[0] &&
			data[1] == id[1] &&
			data[2] == id[2])
		return 1;

	pr_info("ATA expect read data is %x %x %x\n",
			id[0], id[1], id[2]);

	return 0;
}

/*O6 code for HQ-413545 by zhaobeidou at 2024/09/23 start*/
static int panel_normal_peak_control(struct drm_panel *panel, uint32_t level)
{
	struct lcm *ctx;
	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}
	ctx = panel_to_lcm(panel);
	mutex_lock(&ctx->panel_lock);
	pr_info("%s level = %d\n", __func__, level);
	if (level == 1) {
		lcm_dcs_write_seq_static(ctx, 0x51, 0x0F, 0xFF);//apl code
		lcm_dcs_write_seq_static(ctx, 0x5f, 0x05, 0x00);
	} else if (level == 0) {
		lcm_dcs_write_seq_static(ctx, 0x5f, 0x00);
	}
	mutex_unlock(&ctx->panel_lock);
	return 0;
}
/*O6 code for HQ-413545 by zhaobeidou at 2024/09/23 end*/

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_tb0[] = {0x51, 0x00, 0x00};
	struct lcm *ctx = panel_to_lcm(this_panel);
	pr_info(" %s start \n", __func__);
	if (level) {
		bl_tb0[1] = (level >> 8) & 0xFF;
		bl_tb0[2] = level & 0xFF;
	}

	if (!cb)
		return -1;

	pr_info("%s: last_bl_level = %d,level = %d\n", __func__, last_bl_level, level);

/*O6 code for HQ-413545 by zhaobeidou at 2024/09/23 start*/
	if (level == 4095) {
		panel_normal_peak_control(&ctx->panel, 1);
		pr_info("%s peak level = %d \n", __func__, level);
	}
/*O6 code for HQ-413545 by zhaobeidou at 2024/09/23 end*/

	if (fod_in_calibration || lhbm_flag) {
		pr_info("panel skip set backlight %d due to fod hbm "
				"or fod calibration\n", level);
	} else {
		mutex_lock(&ctx->panel_lock);
		cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
		mutex_unlock(&ctx->panel_lock);
	}

	if (level != 0)
		last_non_zero_bl_level = level;
	last_bl_level = level;

	return 0;
}

static int lcm_get_virtual_heigh(void)
{
	return VAC;
}

static int lcm_get_virtual_width(void)
{
	return HAC;
}

static struct mtk_panel_params ext_params = {
	.lcm_index = 0,
	.pll_clk = DATA_RATE/2,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.is_cphy = 0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable                =  DSC_ENABLE,
		.ver                   =  DSC_VER,
		.slice_mode            =  DSC_SLICE_MODE,
		.rgb_swap              =  DSC_RGB_SWAP,
		.dsc_cfg               =  DSC_DSC_CFG,
		.rct_on                =  DSC_RCT_ON,
		.bit_per_channel       =  DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth    =  DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable             =  DSC_BP_ENABLE,
		.bit_per_pixel         =  DSC_BIT_PER_PIXEL,
		.pic_height            =  FRAME_HEIGHT,
		.pic_width             =  FRAME_WIDTH,
		.slice_height          =  DSC_SLICE_HEIGHT,
		.slice_width           =  DSC_SLICE_WIDTH,
		.chunk_size            =  DSC_CHUNK_SIZE,
		.xmit_delay            =  DSC_XMIT_DELAY,
		.dec_delay             =  DSC_DEC_DELAY,
		.scale_value           =  DSC_SCALE_VALUE,
		.increment_interval    =  DSC_INCREMENT_INTERVAL,
		.decrement_interval    =  DSC_DECREMENT_INTERVAL,
		.line_bpg_offset       =  DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset        =  DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset      =  DSC_SLICE_BPG_OFFSET,
		.initial_offset        =  DSC_INITIAL_OFFSET,
		.final_offset          =  DSC_FINAL_OFFSET,
		.flatness_minqp        =  DSC_FLATNESS_MINQP,
		.flatness_maxqp        =  DSC_FLATNESS_MAXQP,
		.rc_model_size         =  DSC_RC_MODEL_SIZE,
		.rc_edge_factor        =  DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0  =  DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1  =  DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi      =  DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo      =  DSC_RC_TGT_OFFSET_LO,
		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = rc_buf_thresh,
			.range_min_qp = range_min_qp,
			.range_max_qp = range_max_qp,
			.range_bpg_ofs = range_bpg_ofs,
		},
	},
	.data_rate = DATA_RATE,
#ifdef CONFIG_MI_DISP_FOD_SYNC
 	.bl_sync_enable = 1,
 	.aod_delay_enable = 1,
#endif
	//.vfp_low_power = 820,//idle 90hz
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 60,
		/* O6 code for HQ-392892 by p-zhangyundan at 2024/07/31 start */
		.cmds_counts_switch_fps = 30,
		.short_dfps_cmds_counts = 1,
		.short_dfps_cmds_start_index = 1,
		.long_dfps_cmds_counts = 2,
		.dfps_cmd_table[0] = {0, 2, {0x38, 0x00}},
		.dfps_cmd_table[1] = {0, 2, {0x2F, 0x02}},
		/* O6 code for HQ-392892 by p-zhangyundan at 2024/07/31 end */
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
};

static int panel_get_panel_info(struct drm_panel *panel, char *buf)
	{
	int count = 0;
	struct lcm *ctx;
	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		return -EAGAIN;
	}
	ctx = panel_to_lcm(panel);
	count = snprintf(buf, PAGE_SIZE, "%s\n", ctx->panel_info);
	return count;
}
static int panel_normal_hbm_control(struct drm_panel *panel, uint32_t level)
{
	struct lcm *ctx;
	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}
	ctx = panel_to_lcm(panel);
	mutex_lock(&ctx->panel_lock);
	if (level == 1) {
		lcm_dcs_write_seq_static(ctx, 0x51, 0x0F, 0xFF);
	} else if (level == 0) {
		lcm_dcs_write_seq_static(ctx, 0x51, 0x07, 0xFF);
	}
	mutex_unlock(&ctx->panel_lock);
	return 0;
}
static int lcm_setbacklight_control(struct drm_panel *panel, unsigned int level)
{
	struct lcm *ctx;
	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}
	ctx = panel_to_lcm(panel);
	if (level > 2047) {
		ctx->hbm_enabled = true;
	}
	pr_err("lcm_setbacklight_control backlight %d\n", level);
	return 0;
}

static struct LCM_setting_table fod_calibration[] ={
	{0x51, 02, {0x07, 0xff}},
};
static int backlight_for_calibration(struct drm_panel *panel, unsigned int level)
{
	u8 bl_tb[2] = {0};
	unsigned int bl_level = -1;
	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}
	if (level == -1) {
		bl_level = last_bl_level;
		pr_err("FOD calibration brightness restore last_bl_level = %d\n", last_bl_level);
		fod_in_calibration = false;
	} else {
		bl_level = level;
		fod_in_calibration = true;
	}
	bl_level_for_fod = bl_level;
	bl_tb[0] = (bl_level >> 8) & 0xFF;
	bl_tb[1] = bl_level & 0xFF;
	fod_calibration[0].para_list[0] = bl_tb[0];
	fod_calibration[0].para_list[1] = bl_tb[1];
	mi_disp_panel_ddic_send_cmd(fod_calibration, ARRAY_SIZE(fod_calibration), true);
	pr_info("backlight_for_calibration backlight %d\n", bl_level);
	return 0;
}

static bool get_lcm_initialized(struct drm_panel *panel)
{
	bool ret = false;
	struct lcm *ctx;
	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}
	ctx = panel_to_lcm(panel);
	ret = ctx->prepared;
	return ret;
}

static int panel_set_doze_brightness(struct drm_panel *panel, int doze_brightness) {
	struct lcm *ctx = NULL;
	int ret = 0;
	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}
	ctx = panel_to_lcm(panel);
	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
		ret = -1;
		goto err;
	}
	/*
		DOZE_TO_NORMAL = 0,
		DOZE_BRIGHTNESS_HBM = 1,
		DOZE_BRIGHTNESS_LBM = 2,
	*/
	switch (doze_brightness) {
		case DOZE_BRIGHTNESS_HBM:
			if (lhbm_flag == false) {
				mi_disp_panel_ddic_send_cmd(lcm_aod_hbm, ARRAY_SIZE(lcm_aod_hbm), true);
                        }
			break;
		case DOZE_BRIGHTNESS_LBM:
			if (lhbm_flag == false) {
				mi_disp_panel_ddic_send_cmd(lcm_aod_lbm, ARRAY_SIZE(lcm_aod_lbm), true);
                        }
			break;
		case DOZE_TO_NORMAL:
			/* O6 code for HQ-392892 by p-zhangyundan at 2024/07/31 start */
			pr_info("exit aod_mode\n");
			/* O6 code for HQ-392892 by p-zhangyundan at 2024/07/31 end */
			break;
		default:
			pr_err("%s: doze_brightness is invalid\n", __func__);
			ret = -1;
			goto err;
	}
	pr_info("doze_brightness = %d\n", doze_brightness);
	ctx->doze_brightness = doze_brightness;
err:
	return ret;
}

static int panel_set_only_aod_backlight(struct drm_panel *panel, int doze_brightness)
{
	struct lcm *ctx;
	if (!panel) {
		pr_err("invalid params\n");
		return -1;
	}
	ctx = panel_to_lcm(panel);
	pr_info("%s set_doze_brightness state = %d",__func__, doze_brightness);

	if ((DOZE_BRIGHTNESS_LBM  == doze_brightness || DOZE_BRIGHTNESS_HBM  == doze_brightness) && lhbm_flag == false) {
		if (DOZE_BRIGHTNESS_LBM  == doze_brightness) {
			mi_disp_panel_ddic_send_cmd(lcm_aod_lbm, ARRAY_SIZE(lcm_aod_lbm), true);
		} else if (DOZE_BRIGHTNESS_HBM == doze_brightness) {
			mi_disp_panel_ddic_send_cmd(lcm_aod_hbm, ARRAY_SIZE(lcm_aod_hbm), true);
		}
	}

	ctx->doze_brightness = doze_brightness;
	pr_info("%s set doze_brightness %d end -\n", __func__, doze_brightness);
	return 0;
}

int panel_get_doze_brightness(struct drm_panel *panel, u32 *brightness) {
	struct lcm *ctx = NULL;
	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}
	ctx = panel_to_lcm(panel);
	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
		return -1;
	}
	*brightness = ctx->doze_brightness;
	return 0;
}

static int panel_set_gir_on(struct drm_panel *panel)
{
	struct lcm *ctx = NULL;
	int ret = 0;
	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		ret = -1;
		goto err;
	}
	ctx = panel_to_lcm(panel);
	pr_info("%s: + ctx->gir_status = %d  \n", __func__, ctx->gir_status);
	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
	} else {
		if (ctx->dynamic_fps == 120) {
			mi_disp_panel_ddic_send_cmd(local_hbm_gir_on_120Hz, ARRAY_SIZE(local_hbm_gir_on_120Hz), true);
		} else if(ctx->dynamic_fps == 90) {
			mi_disp_panel_ddic_send_cmd(local_hbm_gir_on_90Hz, ARRAY_SIZE(local_hbm_gir_on_90Hz), true);
		} else if(ctx->dynamic_fps == 60) {
			mi_disp_panel_ddic_send_cmd(local_hbm_gir_on_60Hz, ARRAY_SIZE(local_hbm_gir_on_60Hz), true);
		}
		ctx->gir_status = 1;
	}
err:
	return ret;
}
static int panel_set_gir_off(struct drm_panel *panel)
{
	struct lcm *ctx = NULL;
	int ret = -1;
	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		goto err;
	}
	ctx = panel_to_lcm(panel);
	pr_info("%s: + ctx->gir_status = %d \n", __func__, ctx->gir_status);
	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
	} else {
		if (ctx->dynamic_fps == 120) {
			mi_disp_panel_ddic_send_cmd(local_hbm_gir_off_120Hz, ARRAY_SIZE(local_hbm_gir_off_120Hz), true);
		} else if(ctx->dynamic_fps == 90) {
			mi_disp_panel_ddic_send_cmd(local_hbm_gir_off_90Hz, ARRAY_SIZE(local_hbm_gir_off_90Hz), true);
		} else if(ctx->dynamic_fps == 60) {
			mi_disp_panel_ddic_send_cmd(local_hbm_gir_off_60Hz, ARRAY_SIZE(local_hbm_gir_off_60Hz), true);
		}
		ctx->gir_status = 0;
	}
err:
	return ret;
}

static int panel_get_gir_status(struct drm_panel *panel)
{
	struct lcm *ctx;
	if (!panel) {
		pr_err("%s; panel is NULL\n", __func__);
		return -1;
	}
	ctx = panel_to_lcm(panel);
	return ctx->gir_status;
}

static int mi_disp_panel_update_lhbm_reg(struct mtk_dsi * dsi, enum lhbm_cmd_type type, int bl_level)
{
	u8 alpha_buf[2] = {0};
	u8 bl_tb[2] = {0};
	if(!dsi) {
		pr_err("%s:panel is NULL\n", __func__);
		return -1;
	}
	pr_info("lhbm update 0x87, lhbm_cmd_type:%d, bl_level = %d\n", type, bl_level);
	if (bl_level >= 0) {
		alpha_buf[0] = (versionox_alpha_set[bl_level] >> 8) & 0xff;
		alpha_buf[1] = versionox_alpha_set[bl_level] & 0xff;
		bl_tb[0] = (bl_level >> 8) & 0xFF;
		bl_tb[1] = bl_level & 0xFF;
		pr_info("[%d] alpha_buf[0] = %02hhx, alpha_buf[1] = %02hhx\n",
					type, alpha_buf[0],  alpha_buf[1]);
	} else {
		pr_err("The backlight level wrong!\n");
		return -1;
	}
	switch (type) {
	case TYPE_WHITE_1200:
		local_hbm_normal_white_1200nit[0].para_list[12] = alpha_buf[0];
		local_hbm_normal_white_1200nit[0].para_list[13] = alpha_buf[1];
		break;
	case TYPE_WHITE_200:
		local_hbm_normal_white_200nit[0].para_list[12] = alpha_buf[0];
		local_hbm_normal_white_200nit[0].para_list[13] = alpha_buf[1];
		break;
	case TYPE_GREEN_500:
		local_hbm_normal_green_500nit[0].para_list[12] = alpha_buf[0];
		local_hbm_normal_green_500nit[0].para_list[13] = alpha_buf[1];
		break;
	case TYPE_LHBM_OFF:
		local_hbm_normal_off[1].para_list[0] = bl_tb[0];
		local_hbm_normal_off[1].para_list[1] = bl_tb[1];
		break;
	default:
		pr_err("unsuppport cmd \n");
		return -1;
	}
	return 0;
}

static int panel_set_lhbm_fod(struct mtk_dsi *dsi,  enum local_hbm_state lhbm_state)
{
	struct lcm *ctx = NULL;
	int bl_level = 0;
	struct mi_dsi_panel_cfg *mi_cfg = NULL;

	if(!dsi || !dsi->panel) {
		pr_err("%s:panel is NULL\n", __func__);
		return -1;
	}
	mi_cfg = &dsi->mi_cfg;
	ctx = panel_to_lcm(dsi->panel);
	if (!ctx || !ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
		return -1;
	}
	if (fod_in_calibration) {
		bl_level = bl_level_for_fod;
	} else if (ctx->doze_brightness == DOZE_BRIGHTNESS_LBM) {
		bl_level = doze_lbm_bl;
	} else if (ctx->doze_brightness == DOZE_BRIGHTNESS_HBM) {
		bl_level = doze_hbm_bl;
	} else {
		bl_level = last_bl_level;
	}

	if (bl_level > hbm_level) {
		if((lhbm_state == LOCAL_HBM_OFF_TO_NORMAL) || (lhbm_state == LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT) || (lhbm_state == LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE)) {
			pr_info("exit lhbm to hbm!\n");
                } else {
			pr_info("The bl_level is %d,exit hbm mode to normal!\n", bl_level);
			bl_level = hbm_level;
			mi_disp_panel_ddic_send_cmd(exit_hbm_to_normal, ARRAY_SIZE(exit_hbm_to_normal), true);
		}
	}
	pr_info("%s local hbm_state :%d \n", __func__, lhbm_state);

	switch (lhbm_state) {
	case LOCAL_HBM_OFF_TO_NORMAL://0
	case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT://10
		pr_info("LOCAL_HBM_NORMAL off\n");
		lhbm_flag = false;
		msleep(5);
		mutex_lock(&ctx->lhbm_lock);
		mi_disp_panel_update_lhbm_reg(dsi, TYPE_LHBM_OFF, bl_level);
		mi_disp_panel_ddic_send_cmd(local_hbm_normal_off, ARRAY_SIZE(local_hbm_normal_off),  true);
		mutex_unlock(&ctx->lhbm_lock);
		mi_cfg->lhbm_en = false;
		break;
	case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE://11
		pr_info("LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE\n");
		lhbm_flag = false;
		mutex_lock(&ctx->lhbm_lock);
		if ((ctx->doze_brightness == DOZE_BRIGHTNESS_LBM) || (ctx->doze_brightness == DOZE_BRIGHTNESS_HBM)) {
			mi_disp_panel_update_lhbm_reg(dsi, TYPE_LHBM_OFF, bl_level);
			mi_disp_panel_ddic_send_cmd(local_hbm_normal_off, ARRAY_SIZE(local_hbm_normal_off),  true);
		} else {
			mi_disp_panel_update_lhbm_reg(dsi, TYPE_LHBM_OFF, last_bl_level);
			mi_disp_panel_ddic_send_cmd(local_hbm_normal_off, ARRAY_SIZE(local_hbm_normal_off),  true);
		}
		mutex_unlock(&ctx->lhbm_lock);
		mi_cfg->lhbm_en = false;
		break;
	case LOCAL_HBM_HLPM_WHITE_1000NIT://6
	case LOCAL_HBM_NORMAL_WHITE_1000NIT://1
		pr_info("LOCAL_HBM_NORMAL_WHITE_1000NIT \n");
		lhbm_flag = true;
		mutex_lock(&ctx->lhbm_lock);
		mi_disp_panel_update_lhbm_reg(dsi, TYPE_WHITE_1200, bl_level);
		mi_disp_panel_ddic_send_cmd(local_hbm_normal_white_1200nit, ARRAY_SIZE(local_hbm_normal_white_1200nit), true);
		mutex_unlock(&ctx->lhbm_lock);
		mi_cfg->lhbm_en = true;
		break;
	case LOCAL_HBM_HLPM_WHITE_110NIT://7
	case LOCAL_HBM_NORMAL_WHITE_110NIT://4
		pr_info("LOCAL_HBM_NORMAL_WHITE_110NIT \n");
		lhbm_flag = true;
		mutex_lock(&ctx->lhbm_lock);
		mi_disp_panel_update_lhbm_reg(dsi, TYPE_WHITE_200, bl_level);
		mi_disp_panel_ddic_send_cmd(local_hbm_normal_white_200nit, ARRAY_SIZE(local_hbm_normal_white_200nit), true);
		mutex_unlock(&ctx->lhbm_lock);
		mi_cfg->lhbm_en = true;
		break;
	case LOCAL_HBM_NORMAL_GREEN_500NIT://5
		pr_info("LOCAL_HBM_NORMAL_GREEN_500NIT \n");
		lhbm_flag = true;
		mutex_lock(&ctx->lhbm_lock);
		mi_disp_panel_update_lhbm_reg(dsi, TYPE_GREEN_500, bl_level);
		mi_disp_panel_ddic_send_cmd(local_hbm_normal_green_500nit, ARRAY_SIZE(local_hbm_normal_green_500nit), true);
		mutex_unlock(&ctx->lhbm_lock);
		mi_cfg->lhbm_en = true;
		break;
	default:
		pr_info("invalid local hbm value\n");
		break;
	}

	return 0;

}

static struct mtk_panel_params ext_params_mode_1 = {
	.lcm_index = 0,
	.pll_clk = DATA_RATE/2,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.is_cphy = 0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable                =  DSC_ENABLE,
		.ver                   =  DSC_VER,
		.slice_mode            =  DSC_SLICE_MODE,
		.rgb_swap              =  DSC_RGB_SWAP,
		.dsc_cfg               =  DSC_DSC_CFG,
		.rct_on                =  DSC_RCT_ON,
		.bit_per_channel       =  DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth    =  DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable             =  DSC_BP_ENABLE,
		.bit_per_pixel         =  DSC_BIT_PER_PIXEL,
		.pic_height            =  FRAME_HEIGHT,
		.pic_width             =  FRAME_WIDTH,
		.slice_height          =  DSC_SLICE_HEIGHT,
		.slice_width           =  DSC_SLICE_WIDTH,
		.chunk_size            =  DSC_CHUNK_SIZE,
		.xmit_delay            =  DSC_XMIT_DELAY,
		.dec_delay             =  DSC_DEC_DELAY,
		.scale_value           =  DSC_SCALE_VALUE,
		.increment_interval    =  DSC_INCREMENT_INTERVAL,
		.decrement_interval    =  DSC_DECREMENT_INTERVAL,
		.line_bpg_offset       =  DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset        =  DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset      =  DSC_SLICE_BPG_OFFSET,
		.initial_offset        =  DSC_INITIAL_OFFSET,
		.final_offset          =  DSC_FINAL_OFFSET,
		.flatness_minqp        =  DSC_FLATNESS_MINQP,
		.flatness_maxqp        =  DSC_FLATNESS_MAXQP,
		.rc_model_size         =  DSC_RC_MODEL_SIZE,
		.rc_edge_factor        =  DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0  =  DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1  =  DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi      =  DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo      =  DSC_RC_TGT_OFFSET_LO,
		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = rc_buf_thresh,
			.range_min_qp = range_min_qp,
			.range_max_qp = range_max_qp,
			.range_bpg_ofs = range_bpg_ofs,
		},
	},
	.data_rate = DATA_RATE,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
 	.aod_delay_enable = 1,
#endif
	//.vfp_low_power = 820,//idle 90hz
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 90,
		/* O6 code for HQ-392892 by p-zhangyundan at 2024/07/31 start */
		.cmds_counts_switch_fps = 30,
		.short_dfps_cmds_counts = 1,
		.short_dfps_cmds_start_index = 1,
		.long_dfps_cmds_counts = 2,
		.dfps_cmd_table[0] = {0, 2, {0x38, 0x00}},
		.dfps_cmd_table[1] = {0, 2, {0x2F, 0x01}},
		/* O6 code for HQ-392892 by p-zhangyundan at 2024/07/31 end */
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
};

static struct mtk_panel_params ext_params_mode_2 = {
	.lcm_index = 0,
	.pll_clk = DATA_RATE/2,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.is_cphy = 0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable                =  DSC_ENABLE,
		.ver                   =  DSC_VER,
		.slice_mode            =  DSC_SLICE_MODE,
		.rgb_swap              =  DSC_RGB_SWAP,
		.dsc_cfg               =  DSC_DSC_CFG,
		.rct_on                =  DSC_RCT_ON,
		.bit_per_channel       =  DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth    =  DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable             =  DSC_BP_ENABLE,
		.bit_per_pixel         =  DSC_BIT_PER_PIXEL,
		.pic_height            =  FRAME_HEIGHT,
		.pic_width             =  FRAME_WIDTH,
		.slice_height          =  DSC_SLICE_HEIGHT,
		.slice_width           =  DSC_SLICE_WIDTH,
		.chunk_size            =  DSC_CHUNK_SIZE,
		.xmit_delay            =  DSC_XMIT_DELAY,
		.dec_delay             =  DSC_DEC_DELAY,
		.scale_value           =  DSC_SCALE_VALUE,
		.increment_interval    =  DSC_INCREMENT_INTERVAL,
		.decrement_interval    =  DSC_DECREMENT_INTERVAL,
		.line_bpg_offset       =  DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset        =  DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset      =  DSC_SLICE_BPG_OFFSET,
		.initial_offset        =  DSC_INITIAL_OFFSET,
		.final_offset          =  DSC_FINAL_OFFSET,
		.flatness_minqp        =  DSC_FLATNESS_MINQP,
		.flatness_maxqp        =  DSC_FLATNESS_MAXQP,
		.rc_model_size         =  DSC_RC_MODEL_SIZE,
		.rc_edge_factor        =  DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0  =  DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1  =  DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi      =  DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo      =  DSC_RC_TGT_OFFSET_LO,
		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = rc_buf_thresh,
			.range_min_qp = range_min_qp,
			.range_max_qp = range_max_qp,
			.range_bpg_ofs = range_bpg_ofs,
		},
	},
	.data_rate = DATA_RATE,
#ifdef CONFIG_MI_DISP_FOD_SYNC
 	.bl_sync_enable = 1,
 	.aod_delay_enable = 1,
#endif
	//.vfp_low_power = 820,//idle 90hz
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 120,
		/* O6 code for HQ-392892 by p-zhangyundan at 2024/07/31 start */
		.cmds_counts_switch_fps = 30,
		.short_dfps_cmds_counts = 1,
		.short_dfps_cmds_start_index = 1,
		.long_dfps_cmds_counts = 2,
		.dfps_cmd_table[0] = {0, 2, {0x38, 0x00}},
		.dfps_cmd_table[1] = {0, 2, {0x2F, 0x00}},
		/* O6 code for HQ-392892 by p-zhangyundan at 2024/07/31 end */
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
};

static struct mtk_panel_params ext_params_mode_3 = {
	.lcm_index = 0,
	.pll_clk = DATA_RATE/2,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.is_cphy = 0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable                =  DSC_ENABLE,
		.ver                   =  DSC_VER,
		.slice_mode            =  DSC_SLICE_MODE,
		.rgb_swap              =  DSC_RGB_SWAP,
		.dsc_cfg               =  DSC_DSC_CFG,
		.rct_on                =  DSC_RCT_ON,
		.bit_per_channel       =  DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth    =  DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable             =  DSC_BP_ENABLE,
		.bit_per_pixel         =  DSC_BIT_PER_PIXEL,
		.pic_height            =  FRAME_HEIGHT,
		.pic_width             =  FRAME_WIDTH,
		.slice_height          =  DSC_SLICE_HEIGHT,
		.slice_width           =  DSC_SLICE_WIDTH,
		.chunk_size            =  DSC_CHUNK_SIZE,
		.xmit_delay            =  DSC_XMIT_DELAY,
		.dec_delay             =  DSC_DEC_DELAY,
		.scale_value           =  DSC_SCALE_VALUE,
		.increment_interval    =  DSC_INCREMENT_INTERVAL,
		.decrement_interval    =  DSC_DECREMENT_INTERVAL,
		.line_bpg_offset       =  DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset        =  DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset      =  DSC_SLICE_BPG_OFFSET,
		.initial_offset        =  DSC_INITIAL_OFFSET,
		.final_offset          =  DSC_FINAL_OFFSET,
		.flatness_minqp        =  DSC_FLATNESS_MINQP,
		.flatness_maxqp        =  DSC_FLATNESS_MAXQP,
		.rc_model_size         =  DSC_RC_MODEL_SIZE,
		.rc_edge_factor        =  DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0  =  DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1  =  DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi      =  DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo      =  DSC_RC_TGT_OFFSET_LO,
		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = rc_buf_thresh,
			.range_min_qp = range_min_qp,
			.range_max_qp = range_max_qp,
			.range_bpg_ofs = range_bpg_ofs,
		},
	},
	.data_rate = DATA_RATE,

#ifdef CONFIG_MI_DISP_FOD_SYNC
 	.bl_sync_enable = 1,
 	.aod_delay_enable = 1,
#endif
	//.vfp_low_power = 820,//idle 90hz
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 30,
		/* O6 code for HQ-392892 by p-zhangyundan at 2024/07/31 start */
		.cmds_counts_switch_fps = 30,
		.short_dfps_cmds_counts = 3,
		.short_dfps_cmds_start_index = 0,
		.long_dfps_cmds_counts = 3,
		.dfps_cmd_table[0] = {0, 2, {0x6F, 0x04}},
		.dfps_cmd_table[1] = {0, 3, {0x51, 0x01, 0x55}},
		.dfps_cmd_table[2] = {0, 2, {0x39, 0x00}},
		/* O6 code for HQ-392892 by p-zhangyundan at 2024/07/31 end */
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
};

static struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
					       unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;

	list_for_each_entry (m, &connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	return NULL;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
				   struct drm_connector *connector,
				   unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	struct lcm *ctx = panel_to_lcm(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, mode);

	if (m == NULL) {
		pr_err("%s:%d invalid display_mode\n", __func__, __LINE__);
		return -1;
	}
	if (drm_mode_vrefresh(m) == MODE_0_FPS) {
		ext->params = &ext_params;
	} else if (drm_mode_vrefresh(m) == MODE_1_FPS) {
		ext->params = &ext_params_mode_1;
	} else if (drm_mode_vrefresh(m) == MODE_2_FPS){
		ext->params = &ext_params_mode_2;
	} else if (drm_mode_vrefresh(m) == MODE_3_FPS) {
		/* O6 code for HQ-392892 by p-zhangyundan at 2024/07/31 start */
		if (ctx->doze_brightness == DOZE_BRIGHTNESS_HBM) {
			ext_params_mode_3.dyn_fps.dfps_cmd_table[1].para_list[1] = 0x0F;
			ext_params_mode_3.dyn_fps.dfps_cmd_table[1].para_list[2] = 0xFE;
		} else if (ctx->doze_brightness == DOZE_BRIGHTNESS_LBM) {
			ext_params_mode_3.dyn_fps.dfps_cmd_table[1].para_list[1] = 0x01;
			ext_params_mode_3.dyn_fps.dfps_cmd_table[1].para_list[2] = 0x55;
		}
		/* O6 code for HQ-392892 by p-zhangyundan at 2024/07/31 end */
		ext->params = &ext_params_mode_3;
	} else{
		ret = 1;
	}
	if (!ret)
		ctx->dynamic_fps = drm_mode_vrefresh(m);
	return ret;
}

static int panel_get_wp_info(struct drm_panel *panel, char *buf, size_t size)
{
	struct device_node *chosen;
	char *tmp_buf = NULL;
	unsigned long tmp_size = 0;
	chosen = of_find_node_by_path("/chosen");
	if (chosen) {
		tmp_buf = (char *)of_get_property(chosen, "lcm_white_point", (int *)&tmp_size);
		if (tmp_size > 0) {
			strncpy(buf, tmp_buf, tmp_size);
			pr_info("[%s]: white_point = %s, size = %lu\n", __func__, buf, tmp_size);
		}
	} else {
		pr_info("[%s]:find chosen failed\n", __func__);
	}

	return tmp_size;
}

static int panel_init_power(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	//VDDI 1.8V
	ctx->vddi_gpio = devm_gpiod_get(ctx->dev, "vddi", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vddi_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vddi_gpio %ld\n",
			__func__, PTR_ERR(ctx->vddi_gpio));
		return PTR_ERR(ctx->vddi_gpio);
	}
	gpiod_set_value(ctx->vddi_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->vddi_gpio);
	udelay(1000);
	//VDD 1.2V
	ctx->vdd_gpio = devm_gpiod_get(ctx->dev, "vdd", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vdd_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vdd_gpio %ld\n",
			__func__, PTR_ERR(ctx->vdd_gpio));
		return PTR_ERR(ctx->vdd_gpio);
	}
	gpiod_set_value(ctx->vdd_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->vdd_gpio);
	udelay(1000);
	//VCI 3.0V
	ctx->vci_gpio = devm_gpiod_get(ctx->dev, "vci", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vci_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vci_gpio %ld\n",
			__func__, PTR_ERR(ctx->vci_gpio));
		return PTR_ERR(ctx->vci_gpio);
	}
	gpiod_set_value(ctx->vci_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->vci_gpio);
	udelay(10000);
	pr_info(" lcm %s\n", __func__);
	return 0;
}

static int panel_power_down(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	udelay(2000);
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	udelay(1000);

	//VCI 3.0V -> 0
	ctx->vci_gpio = devm_gpiod_get(ctx->dev, "vci", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vci_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vci_gpio %ld\n",
			__func__, PTR_ERR(ctx->vci_gpio));
		return PTR_ERR(ctx->vci_gpio);
	}
	gpiod_set_value(ctx->vci_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->vci_gpio);
	udelay(1000);

	//VDD 1.2V -> 0
	ctx->vdd_gpio = devm_gpiod_get(ctx->dev, "vdd", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vdd_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vdd_gpio %ld\n",
			__func__, PTR_ERR(ctx->vdd_gpio));
		return PTR_ERR(ctx->vdd_gpio);
	}
	gpiod_set_value(ctx->vdd_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->vdd_gpio);
	udelay(1000);

	//VDDI 1.8V -> 0
	ctx->vddi_gpio = devm_gpiod_get(ctx->dev, "vddi", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vddi_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vddi_gpio %ld\n",
			__func__, PTR_ERR(ctx->vddi_gpio));
		return PTR_ERR(ctx->vddi_gpio);
	}
	gpiod_set_value(ctx->vddi_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->vddi_gpio);
	udelay(1000);
	return 0;
}

#ifdef CONFIG_MI_ESD_SUPPORT
static void lcm_esd_restore_backlight(struct mtk_dsi *dsi, struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	char bl_tb0[] = {0x51, 0x03, 0xFF};

	if (last_bl_level) {
		bl_tb0[1] = (last_bl_level >> 8) & 0xFF;
		bl_tb0[2] = last_bl_level & 0xFF;
	}
	pr_info(" esd backlight_setting last_level=%d,bl_tb0[1]=%d,bl_tb0[2]= %d\n", last_bl_level, bl_tb0[1], bl_tb0[2]);
	lcm_dcs_write(ctx, bl_tb0, ARRAY_SIZE(bl_tb0));
	pr_info("lcm_esd_restore_backlight \n");
}
#endif

static int panel_fod_lhbm_init (struct mtk_dsi* dsi)
{
	if (!dsi) {
		pr_err("invalid dsi point\n");
		return -1;
	}
	pr_info("panel_fod_lhbm_init enter\n");
	dsi->display_type = "primary";
	dsi->mi_cfg.lhbm_ui_ready_delay_frame = 3;
	dsi->mi_cfg.lhbm_ui_ready_delay_frame_aod = 3;
	dsi->mi_cfg.local_hbm_enabled = 1;
	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ext_param_set = mtk_panel_ext_param_set,
	.ata_check = panel_ata_check,
	.get_virtual_heigh = lcm_get_virtual_heigh,
	.get_virtual_width = lcm_get_virtual_width,
	.get_panel_info = panel_get_panel_info,
	.normal_hbm_control = panel_normal_hbm_control,
	.setbacklight_control = lcm_setbacklight_control,
	.get_panel_initialized = get_lcm_initialized,
	.set_lhbm_fod = panel_set_lhbm_fod,
	.panel_set_gir_on = panel_set_gir_on,
	.panel_set_gir_off = panel_set_gir_off,
	.panel_get_gir_status = panel_get_gir_status,
	.get_wp_info = panel_get_wp_info,
	.set_doze_brightness = panel_set_doze_brightness,
	.get_doze_brightness = panel_get_doze_brightness,
	.init_power = panel_init_power,
	.power_down = panel_power_down,
#ifdef CONFIG_MI_ESD_SUPPORT
	.esd_restore_backlight = lcm_esd_restore_backlight,
#endif
	.backlight_for_calibration = backlight_for_calibration,
	.panel_fod_lhbm_init = panel_fod_lhbm_init,
	.set_only_aod_backlight = panel_set_only_aod_backlight,
};
#endif

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int num_modes;
	unsigned int bpc;
	struct {
		unsigned int width;
		unsigned int height;
	} size;
	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

static void change_drm_disp_mode_params(struct drm_display_mode *mode)
{
	if (fake_heigh > 0 && fake_heigh < VAC) {
		mode->vdisplay = fake_heigh;
		mode->vsync_start = fake_heigh + VFP;
		mode->vsync_end = fake_heigh + VFP + VSA;
		mode->vtotal = fake_heigh + VFP + VSA + VBP;
	}
	if (fake_width > 0 && fake_width < HAC) {
		mode->hdisplay = fake_width;
		mode->hsync_start = fake_width + HFP;
		mode->hsync_end = fake_width + HFP + HSA;
		mode->htotal = fake_width + HFP + HSA + HBP;
	}
}

static int lcm_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode_1;
	struct drm_display_mode *mode_2;
	struct drm_display_mode *mode_3;
	pr_info(" %s lcm_get_modes start \n", __func__);
	if (need_fake_resolution)
		change_drm_disp_mode_params(&default_mode);
	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}
	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);
	mode_1 = drm_mode_duplicate(connector->dev, &performance_mode_1);
	if (!mode_1) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 performance_mode_1.hdisplay,
			 performance_mode_1.vdisplay,
			 drm_mode_vrefresh(&performance_mode_1));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_1);
	mode_1->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_1);
	mode_2 = drm_mode_duplicate(connector->dev, &performance_mode_2);
	if (!mode_2) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 performance_mode_2.hdisplay,
			 performance_mode_2.vdisplay,
			 drm_mode_vrefresh(&performance_mode_2));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_2);
	mode_2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_2);
	mode_3 = drm_mode_duplicate(connector->dev, &performance_mode_3);
	if (!mode_3) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			performance_mode_3.hdisplay, performance_mode_3.vdisplay,
			drm_mode_vrefresh(&performance_mode_3));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_3);
	mode_3->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_3);
	connector->display_info.width_mm = PHYSICAL_WIDTH / 1000;
	connector->display_info.height_mm = PHYSICAL_HEIGHT / 1000;
	pr_info(" %s lcm_get_modes end \n", __func__);
	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static void check_is_need_fake_resolution(struct device *dev)
{
	unsigned int ret = 0;
	ret = of_property_read_u32(dev->of_node, "fake_heigh", &fake_heigh);
	pr_info(" %s start \n", __func__);
	if (ret)
		need_fake_resolution = false;
	ret = of_property_read_u32(dev->of_node, "fake_width", &fake_width);
	if (ret)
		need_fake_resolution = false;
	if (fake_heigh > 0 && fake_heigh < VAC)
		need_fake_resolution = true;
	if (fake_width > 0 && fake_width < HAC)
		need_fake_resolution = true;
}

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct lcm *ctx;
	struct device_node *backlight;
	int ret;
	pr_info(" %s+\n", __func__);
	dsi_node = of_get_parent(dev->of_node);
	//pr_info("lcm: %d\n", dsi_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_info("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_info("device node name:%s\n", remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		pr_info("%s+ skip probe due to not current lcm\n", __func__);
		return -ENODEV;
	}

	pr_info(" %s+\n", __func__);
	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);

	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE
			 | MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_NO_EOT_PACKET
			 | MIPI_DSI_CLOCK_NON_CONTINUOUS;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);

	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->prepared = true;
	ctx->enabled = true;
	ctx->panel_info = panel_name;
	ctx->gir_status = 1;
	ctx->dynamic_fps = 60;
	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif
	ctx->hbm_enabled = false;
	this_panel = &ctx->panel;
	check_is_need_fake_resolution(dev);
	/*O6 code for HQ-391992 by zhaobeidou at 2024/06/26 start*/
	hq_regiser_hw_info(HWID_LCM, "oncell,vendor:41,IC:02");
	/*O6 code for HQ-391992 by zhaobeidou at 2024/06/26 end*/
	/*cmd_msg = (struct mtk_ddic_dsi_msg *)vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	if (cmd_msg == NULL) {
		ret= -ENOMEM;
		pr_err("fail to vmalloc for cmd_msg\n");
	} else {
		memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));
    }*/
	pr_info("%s-\n", __func__);

	return ret;
}

static void lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);

#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif
	pr_info(" %s start \n", __func__);
	//vfree(cmd_msg);
	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
#endif

}

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "o6_41_02_0b_dsc_vdo,lcm", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-o6-41-02-0b-dsc-vdo",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("beidou zhao <zhaobeidou@huaqin.com>");
MODULE_DESCRIPTION("panel-o6-41-02-0b-dsc-vdo Panel Driver");
MODULE_LICENSE("GPL v2");
