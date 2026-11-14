// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
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
/*O7 code for HQ-398849 by wangfengbin at 2024/07/04 start*/
#include <linux/hqsysfs.h>
/*O7 code for HQ-398849 by wangfengbin at 2024/07/04 end*/
#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
//#include "../mediatek/mediatek_v2/mtk_log.h"
#include "../mediatek/mediatek_v2/v1/mtk_drm_graphics_base.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mediatek_v2/v1/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

/* O7 code for HQ-393489 by wangfengbin at 2024/07/03 start */
#include "../mediatek/mediatek_v2/v1/mtk_log.h"
#include "../mediatek/mediatek_v2/v1/mi_disp/mi_panel_ext.h"
#include "../mediatek/mediatek_v2/v1/mi_disp/mi_dsi_panel.h"
#include "include/panel_alpha_data.h"
#include "../mediatek/mediatek_v2/v1/mtk_dsi.h"
#include "../mediatek/mediatek_v2/v1/mi_disp/mi_dsi_panel_count.h"

#define CMD_NUM_MAX                 20
/* O7 code for HQ-405817 by wangfengbin at 2024/07/31 start */
#define HBM_MAP_MAX_BRIGHTNESS      4095
#define NORMAL_MAX_BRIGHTNESS       2047
static bool hbm_on = false;
/* O7 code for HQ-405817 by wangfengbin at 2024/07/31 end */

//static struct mtk_ddic_dsi_msg *cmd_msg = NULL;
static const char *panel_name = "panel_name=dsi_o7_38_0c_0a_dsc_vdo";
static struct drm_panel *this_panel;
/* O7 code for HQ-393489 by wangfengbin at 2024/07/03 end */
/* O7 code for HQ-393490 by wangfengbin at 2024/07/08 start */
static int last_bl_level;
static int new_bl_level;
static bool fod_in_calibration = false;
static bool lhbm_flag = false;
static int bl_level_for_fod = 0;
static int hbm_level = 2047;
static bool skip_first_dimming_on = false;
/* O7 code for HQ-393490 by wangfengbin at 2024/07/08 end */
/* O7 code for HQ-387739 by wangfengbin at 2024/07/22 start */
static int doze_lbm_bl = 29;
static int doze_hbm_bl = 248;
/* O7 code for HQ-387739 by wangfengbin at 2024/07/22 end */

/*struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *vddi_gpio;
	struct gpio_desc *vci_gpio;
	bool prepared;
	bool enabled;
	int pmode_id;
	struct list_head probed_modes;
	int error;
	int gir_status;
	bool hbm_enabled;
	const char *panel_info;
	struct mutex lhbm_lock;
	struct mutex panel_lock;
	enum doze_brightness_state doze_brightness;
};*/

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
/* O7 code for HQ-392083 by wangfengbin at 2024/07/24 start */
static struct LCM_setting_table local_hbm_gir_on[] = {
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x89, 0xB1}},
	{0xB1, 01, {0x2D}},
	{0xB0, 03, {0x00, 0x8E, 0xB1}},
	{0xB1, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
};

static struct LCM_setting_table local_hbm_gir_off[] = {
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x89, 0xB1}},
	{0xB1, 01, {0x0D}},
	{0xB0, 03, {0x00, 0x8E, 0xB1}},
	{0xB1, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
};
/* O7 code for HQ-392083 by wangfengbin at 2024/07/24 end */
/* O7 code for HQ-393489 by wangfengbin at 2024/07/03 start */
/* O7 code for HQ-404413 by p-wangfengbin at 2024/08/25 start */
static struct LCM_setting_table local_hbm_normal_white_1200nit[] = {
	{0xF0, 02, {0x5A, 0x5A}},
	{0x53, 01, {0x24}},
	{0xB0, 03, {0x01, 0x53, 0xB2}},
	{0xB2, 14, {0x00, 0x04, 0x7F, 0xF7, 0xFF, 0x7F, 0xF7, 0xFF, 0x7F, 0xF7, 0xFF, 0x7F, 0xFF, 0xFF}},
	{0xB0, 03, {0x01, 0x08, 0xB5}},
	{0xB5, 15, {0x02, 0x00, 0x00, 0x00, 0x00, 0x1B, 0x87, 0xD9, 0x28, 0x08, 0xA1, 0x21, 0xC8, 0x3D, 0x5D}},
	{0xB0, 03, {0x01, 0x4E, 0xB2}},
	{0xB2, 05, {0x03, 0x33, 0xAF, 0x3F, 0xFF}},
	{0xB0, 03, {0x00, 0x19, 0xF4}},
	{0xF4, 01, {0x1C}},
	{0xB0, 03, {0x01, 0x17, 0xB2}},
	{0xB2, 01, {0x01}},
	{0xF7, 01, {0x0B}},
};
static struct LCM_setting_table local_hbm_normal_white_200nit[] = {
	{0xF0, 02, {0x5A, 0x5A}},
	{0x53, 01, {0x24}},
	{0xB0, 03, {0x01, 0x53, 0xB2}},
	{0xB2, 14, {0x00, 0x04, 0x7F, 0xF7, 0xFF, 0x7F, 0xF7, 0xFF, 0x7F, 0xF7, 0xFF, 0x7F, 0xFF, 0xFF}},
	{0xB0, 03, {0x01, 0x08, 0xB5}},
	{0xB5, 15, {0x02, 0x00, 0x00, 0x00, 0x00, 0x1B, 0x87, 0xD9, 0x28, 0x08, 0xA1, 0x21, 0xC8, 0x3D, 0x5D}},
	{0xB0, 03, {0x01, 0x4E, 0xB2}},
	{0xB2, 05, {0x03, 0x33, 0xAF, 0x3F, 0xFF}},
	{0xB0, 03, {0x00, 0x19, 0xF4}},
	{0xF4, 01, {0x1C}},
	{0xB0, 03, {0x01, 0x17, 0xB2}},
	{0xB2, 01, {0x01}},
	{0xF7, 01, {0x0B}},
};
static struct LCM_setting_table local_hbm_normal_green_500nit[] = {
	{0xF0, 02, {0x5A, 0x5A}},
	{0x53, 01, {0x24}},
	{0xB0, 03, {0x01, 0x53, 0xB2}},
	{0xB2, 14, {0x00, 0x04, 0x7F, 0xF7, 0xFF, 0x7F, 0xF7, 0xFF, 0x7F, 0xF7, 0xFF, 0x7F, 0xFF, 0xFF}},
	{0xB0, 03, {0x01, 0x08, 0xB5}},
	{0xB5, 15, {0x02, 0x33, 0xFF, 0x00, 0xFF, 0x1B, 0x87, 0xD9, 0x28, 0x08, 0xA1, 0x21, 0xC8, 0x3D, 0x5D}},
	{0xB0, 03, {0x01, 0x4E, 0xB2}},
	{0xB2, 05, {0x03, 0x33, 0x00, 0x01, 0x00}},
	{0xB0, 03, {0x00, 0x19, 0xF4}},
	{0xF4, 01, {0x1C}},
	{0xB0, 03, {0x01, 0x17, 0xB2}},
	{0xB2, 01, {0x01}},
	{0xF7, 01, {0x0B}},
};
static struct LCM_setting_table local_hbm_on_B4[] = {
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0xB5, 0xB4}},
	{0xB4, 03, {0x01, 0x00, 0x14}},
};
/* O7 code for HQ-404413 by p-wangfengbin at 2024/08/25 end */
static struct LCM_setting_table local_hbm_normal_off[] = {
	{0xF0, 02, {0x5A, 0x5A}},
	{0x53, 01, {0x28}},
	{0xB0, 03, {0x01, 0x17, 0xB2}},
	{0xB2, 01, {0x00}},
	{0xB0, 03, {0x00, 0x19, 0xF4}},
	{0xF4, 01, {0x0A}},
	{0xB0, 03, {0x01, 0x53, 0xB2}},
	{0xB2, 14, {0x01, 0xC2, 0x00, 0x40, 0x14, 0x02, 0x80, 0x52, 0x0A, 0x41, 0x48, 0x7F, 0xFF, 0xFF}},
	{0xB0, 03, {0x00, 0xB5, 0xB4}},
	{0xB4, 03, {0x00, 0x00, 0x14}},
	{0xF7, 01, {0x0B}},
};
/* O7 code for HQ-387739 by wangfengbin at 2024/07/22 start */
static struct LCM_setting_table normal_hbm_on[] = {
	{0x53, 01, {0xE8}},
	{0x51, 02, {0x07, 0xFF}},
	{0xF7, 01, {0x0B}}
};

static struct LCM_setting_table normal_hbm_off[] = {
	{0x53, 01, {0x28}},
	{0xF7, 01, {0x0B}},
};
/* O7 code for HQ-405817 by wangfengbin at 2024/07/31 start */
static struct LCM_setting_table normal_hbm_off_7FF[] = {
	{0x53, 01, {0x28}},
	{0x51, 02, {0x07, 0xFF}},
	{0xF7, 01, {0x0B}},
};

static struct LCM_setting_table normal_hbm_on_001[] = {
	{0x53, 01, {0xE8}},
	{0x51, 02, {0x00, 0x01}},
	{0xF7, 01, {0x0B}},
};

static struct LCM_setting_table dimming_off[] = {
	{0x53, 01, {0x20}},
};

/* O7 code for HQ-405817 by wangfengbin at 2024/07/31 end */
static struct LCM_setting_table LCM_AOD_HBM[] = {
	{0x51, 02, {0x01, 0x18}},
};

static struct LCM_setting_table LCM_AOD_LBM[] = {
	{0x51, 02, {0x00, 0x16}},
};
/* O7 code for HQ-404421 by wangfengbin at 2024/07/30 start */
/*
static struct LCM_setting_table LCM_AOD_TO_NORMAL[] = {
	{0x51, 02, {0x00, 0x00}},
};
*/
/* O7 code for HQ-404421 by wangfengbin at 2024/07/30 end */
/* O7 code for HQ-387739 by wangfengbin at 2024/07/22 end */
/* O7 code for HQ-393490 by wangfengbin at 2024/07/08 start */
static struct LCM_setting_table exit_hbm_to_normal[] = {
	{0x51, 02, {0x07,0xff}},
};
/* O7 code for HQ-393490 by wangfengbin at 2024/07/08 end */
/* O7 code for HQ-393489 by wangfengbin at 2024/07/03 end */
static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
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
		//.flags = MIPI_DSI_MSG_USE_LPM,
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
		dev_info(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}
#ifdef PANEL_SUPPORT_READBACK
static int lcm_dcs_read(struct lcm *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_info(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret, cmd);
		ctx->error = ret;
	}

	return ret;
}

static void lcm_panel_get_data(struct lcm *ctx)
{
	u8 buffer[3] = {0};
	static int ret;

	if (ret == 0) {
		ret = lcm_dcs_read(ctx,  0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void lcm_panel_fhd120_init(struct lcm *ctx)
{
	lcm_dcs_write_seq_static(ctx, 0x11);
	mdelay(32);
/* O7 code for HQ-404696 by wangfengbin at 2024/07/31 start */
	/*FFC setting 96.3MHz*/
	lcm_dcs_write_seq_static(ctx, 0xFC, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x14, 0xDF);
	lcm_dcs_write_seq_static(ctx, 0xDF, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xDF, 0x09, 0x30, 0x95, 0x39, 0x86, 0x39, 0x86);
	lcm_dcs_write_seq_static(ctx, 0xFC, 0xA5, 0xA5);
/* O7 code for HQ-404696 by wangfengbin at 2024/07/31 end */
	/*11bit Dimming*/
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x01, 0x31);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	/*DSC*/
/* O7 code for HQ-387672 by wangfengbin at 2024/07/23 start */
	lcm_dcs_write_seq_static(ctx, 0x9F, 0xA5, 0xA5);
/* O7 code for HQ-387672 by wangfengbin at 2024/07/23 end */
	lcm_dcs_write_seq_static(ctx, 0x9D, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x9E, 0x11,0x00,0x00,0x89,0x30,0x80,0x09,0x60,
			0x04,0x38,0x00,0x28,0x02,0x1C,0x02,0x1C,
			0x02,0x00,0x02,0x0E,0x00,0x20,0x03,0xDD,
			0x00,0x07,0x00,0x0C,0x02,0x77,0x02,0x8B,
			0x18,0x00,0x10,0xF0,0x03,0x0C,0x20,0x00,
			0x06,0x0B,0x0B,0x33,0x0E,0x1C,0x2A,0x38,
			0x46,0x54,0x62,0x69,0x70,0x77,0x79,0x7B,
			0x7D,0x7E,0x01,0x02,0x01,0x00,0x09,0x40,
			0x09,0xBE,0x19,0xFC,0x19,0xFA,0x19,0xF8,
			0x1A,0x38,0x1A,0x78,0x1A,0xB6,0x2A,0xF6,
			0x2B,0x34,0x2B,0x74,0x3B,0x74,0x6B,0xF4,
			0x00);
/* O7 code for HQ-387672 by wangfengbin at 2024/07/23 start */
	lcm_dcs_write_seq_static(ctx, 0x9F, 0x5A, 0x5A);
/* O7 code for HQ-387672 by wangfengbin at 2024/07/23 end */
	/*brightness control*/
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0x60, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xF7, 0x0B);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	/*Steal Light Repair Code*/
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x40, 0xF2);
	lcm_dcs_write_seq_static(ctx, 0xF2, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	/*vint voltage control*/
	lcm_dcs_write_seq_static(ctx, 0xFC, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x11, 0xFE);
	lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xFC, 0xA5, 0xA5);
	/*dimming setting*/
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0D, 0xB2);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x08);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0C, 0xB2);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x20);
	lcm_dcs_write_seq_static(ctx, 0xF7, 0x0B);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	/*flat mode on*/
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x89, 0xB1);
	lcm_dcs_write_seq_static(ctx, 0xB1, 0x2D);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x8E, 0xB1);
	lcm_dcs_write_seq_static(ctx, 0xB1, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
/* O7 code for HQ-404696 by wangfengbin at 2024/07/31 start */
	/*LTPS Setting*/
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x1F, 0xC3);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0x2C, 0x26);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x24, 0xC3);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0x17, 0x23, 0x22);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x33, 0xC3);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0x1B);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x6F, 0xC3);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0x38, 0x15);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x74, 0xC3);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0x1E, 0x24, 0x1E);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x83, 0xC3);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0x3F);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0xA3, 0xC3);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0x2C, 0x26);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0xA8, 0xC3);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0x17, 0x23, 0x22);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0xB7, 0xC3);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0x1B);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0xF3, 0xC3);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0x1D, 0x1D, 0x1D, 0x1D);
	lcm_dcs_write_seq_static(ctx, 0xF7, 0x0B);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	/*Mux Setting*/
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0C, 0xF6);
	lcm_dcs_write_seq_static(ctx, 0xF6, 0x45, 0x20);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x1F, 0xF6);
	lcm_dcs_write_seq_static(ctx, 0xF6, 0x41, 0x20);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x32, 0xF6);
	lcm_dcs_write_seq_static(ctx, 0xF6, 0x45, 0x20);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x2C, 0xF6);
	lcm_dcs_write_seq_static(ctx, 0xF6, 0x00, 0x00, 0x00, 0x48, 0x4A);
	lcm_dcs_write_seq_static(ctx, 0xF7, 0x0B);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
/* O7 code for HQ-404696 by wangfengbin at 2024/07/31 end */
	/*err fg setting*/
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xFC, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xED, 0x01, 0xCD, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xE1, 0x93);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x06, 0xF4);
	lcm_dcs_write_seq_static(ctx, 0xF4, 0x1F);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	lcm_dcs_write_seq_static(ctx, 0xFC, 0xA5, 0xA5);
/* O7 code for HQ-420860 by p-zhanghuan25 at 20240918 start */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0xBE, 0xC3);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0xB1);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0xED, 0xC3);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0x2A, 0x04, 0x04);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0xF3, 0xC3);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0x1D, 0x1D, 0x1D, 0x1D);
	lcm_dcs_write_seq_static(ctx, 0xF7, 0x0B);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
/* O7 code for HQ-420860 by p-zhanghuan25 at 20240918 end */
	lcm_dcs_write_seq_static(ctx, 0x53, 0x28);
	lcm_dcs_write_seq_static(ctx, 0x51, 0x00, 0x00);
	/* O7 code for HQ-404413 by p-wangfengbin at 2024/08/25 start */
	/*LHBM on common setting*/
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0xB5, 0xB4);
	lcm_dcs_write_seq_static(ctx, 0xB4, 0x01, 0x00, 0x14);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0xBD, 0xB4);
	lcm_dcs_write_seq_static(ctx, 0xB4, 0x03, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x36, 0xB4);
	lcm_dcs_write_seq_static(ctx, 0xB4, 0x00, 0x40, 0x14, 0x02, 0x80, 0x52, 0x0A, 0x41, 0x48, 0x7F, 0xFF, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	/* O7 code for HQ-404413 by p-wangfengbin at 2024/08/25 end */
	mdelay(100);
/* O7 code for HQ-387672 by wangfengbin at 2024/07/23 start */
	lcm_dcs_write_seq_static(ctx, 0x9F, 0xA5, 0xA5);
/* O7 code for HQ-387672 by wangfengbin at 2024/07/23 end */
	lcm_dcs_write_seq_static(ctx, 0x29);
}

static void lcm_panel_fhd60_init(struct lcm *ctx)
{
	lcm_dcs_write_seq_static(ctx, 0x11);
	mdelay(32);
/* O7 code for HQ-404696 by wangfengbin at 2024/07/31 start */
	/*FFC setting 96.3MHz*/
	lcm_dcs_write_seq_static(ctx, 0xFC, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x14, 0xDF);
	lcm_dcs_write_seq_static(ctx, 0xDF, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xDF, 0x09, 0x30, 0x95, 0x39, 0x86, 0x39, 0x86);
	lcm_dcs_write_seq_static(ctx, 0xFC, 0xA5, 0xA5);
/* O7 code for HQ-404696 by wangfengbin at 2024/07/31 end */
	/*11bit Dimming*/
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x01, 0x31);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	/*DSC*/
/* O7 code for HQ-387672 by wangfengbin at 2024/07/23 start */
	lcm_dcs_write_seq_static(ctx, 0x9F, 0xA5, 0xA5);
/* O7 code for HQ-387672 by wangfengbin at 2024/07/23 end */
	lcm_dcs_write_seq_static(ctx, 0x9D, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x9E, 0x11,0x00,0x00,0x89,0x30,0x80,0x09,0x60,
			0x04,0x38,0x00,0x28,0x02,0x1C,0x02,0x1C,
			0x02,0x00,0x02,0x0E,0x00,0x20,0x03,0xDD,
			0x00,0x07,0x00,0x0C,0x02,0x77,0x02,0x8B,
			0x18,0x00,0x10,0xF0,0x03,0x0C,0x20,0x00,
			0x06,0x0B,0x0B,0x33,0x0E,0x1C,0x2A,0x38,
			0x46,0x54,0x62,0x69,0x70,0x77,0x79,0x7B,
			0x7D,0x7E,0x01,0x02,0x01,0x00,0x09,0x40,
			0x09,0xBE,0x19,0xFC,0x19,0xFA,0x19,0xF8,
			0x1A,0x38,0x1A,0x78,0x1A,0xB6,0x2A,0xF6,
			0x2B,0x34,0x2B,0x74,0x3B,0x74,0x6B,0xF4,
			0x00);
/* O7 code for HQ-387672 by wangfengbin at 2024/07/23 start */
	lcm_dcs_write_seq_static(ctx, 0x9F, 0x5A, 0x5A);
/* O7 code for HQ-387672 by wangfengbin at 2024/07/23 end */
	/*brightness control*/
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0x60, 0x21);
	lcm_dcs_write_seq_static(ctx, 0xF7, 0x0B);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	/*Steal Light Repair Code*/
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x40, 0xF2);
	lcm_dcs_write_seq_static(ctx, 0xF2, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	/*vint voltage control*/
	lcm_dcs_write_seq_static(ctx, 0xFC, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x11, 0xFE);
	lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xFC, 0xA5, 0xA5);
	/*dimming setting*/
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0D, 0xB2);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x08);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0C, 0xB2);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x20);
	lcm_dcs_write_seq_static(ctx, 0xF7, 0x0B);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	/*flat mode on*/
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x89, 0xB1);
	lcm_dcs_write_seq_static(ctx, 0xB1, 0x2D);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x8E, 0xB1);
	lcm_dcs_write_seq_static(ctx, 0xB1, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
/* O7 code for HQ-404696 by wangfengbin at 2024/07/31 start */
	/*LTPS Setting*/
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x1F, 0xC3);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0x2C, 0x26);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x24, 0xC3);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0x17, 0x23, 0x22);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x33, 0xC3);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0x1B);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x6F, 0xC3);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0x38, 0x15);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x74, 0xC3);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0x1E, 0x24, 0x1E);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x83, 0xC3);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0x3F);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0xA3, 0xC3);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0x2C, 0x26);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0xA8, 0xC3);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0x17, 0x23, 0x22);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0xB7, 0xC3);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0x1B);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0xF3, 0xC3);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0x1D, 0x1D, 0x1D, 0x1D);
	lcm_dcs_write_seq_static(ctx, 0xF7, 0x0B);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	/*Mux Setting*/
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0C, 0xF6);
	lcm_dcs_write_seq_static(ctx, 0xF6, 0x45, 0x20);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x1F, 0xF6);
	lcm_dcs_write_seq_static(ctx, 0xF6, 0x41, 0x20);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x32, 0xF6);
	lcm_dcs_write_seq_static(ctx, 0xF6, 0x45, 0x20);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x2C, 0xF6);
	lcm_dcs_write_seq_static(ctx, 0xF6, 0x00, 0x00, 0x00, 0x48, 0x4A);
	lcm_dcs_write_seq_static(ctx, 0xF7, 0x0B);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
/* O7 code for HQ-404696 by wangfengbin at 2024/07/31 end */
	/*err fg setting*/
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xFC, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xED, 0x01, 0xCD, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xE1, 0x93);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x06, 0xF4);
	lcm_dcs_write_seq_static(ctx, 0xF4, 0x1F);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	lcm_dcs_write_seq_static(ctx, 0xFC, 0xA5, 0xA5);
/* O7 code for HQ-420860 by p-zhanghuan25 at 20240918 start */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0xBE, 0xC3);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0xB1);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0xED, 0xC3);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0x2A, 0x04, 0x04);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0xF3, 0xC3);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0x1D, 0x1D, 0x1D, 0x1D);
	lcm_dcs_write_seq_static(ctx, 0xF7, 0x0B);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
/* O7 code for HQ-420860 by p-zhanghuan25 at 20240918 end */
	lcm_dcs_write_seq_static(ctx, 0x53, 0x28);
	lcm_dcs_write_seq_static(ctx, 0x51, 0x00, 0x00);
	/* O7 code for HQ-404413 by p-wangfengbin at 2024/08/25 start */
	/*LHBM on common setting*/
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0xB5, 0xB4);
	lcm_dcs_write_seq_static(ctx, 0xB4, 0x01, 0x00, 0x14);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0xBD, 0xB4);
	lcm_dcs_write_seq_static(ctx, 0xB4, 0x03, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x36, 0xB4);
	lcm_dcs_write_seq_static(ctx, 0xB4, 0x00, 0x40, 0x14, 0x02, 0x80, 0x52, 0x0A, 0x41, 0x48, 0x7F, 0xFF, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	/* O7 code for HQ-404413 by p-wangfengbin at 2024/08/25 end */
	mdelay(100);
/* O7 code for HQ-387672 by wangfengbin at 2024/07/23 start */
	lcm_dcs_write_seq_static(ctx, 0x9F, 0xA5, 0xA5);
/* O7 code for HQ-387672 by wangfengbin at 2024/07/23 end */
	lcm_dcs_write_seq_static(ctx, 0x29);
}

static void lcm_panel_init(struct lcm *ctx)
{
	pr_info("[LCM/KE] %s\n", __func__);
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return;
	}
	/*gpiod_set_value(ctx->reset_gpio, 0);*/
	/*udelay(10 * 1000);*/
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(10);
	gpiod_set_value(ctx->reset_gpio, 0);
	mdelay(10);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(20);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	pr_info("[LCM/KE] %s++ pmode_id:%d\n", __func__, ctx->pmode_id);

	switch (ctx->pmode_id) {
	case 0:
		lcm_panel_fhd60_init(ctx);
		break;
	case 1:
		lcm_panel_fhd120_init(ctx);
		break;
	default:
		break;
	}
}

/*
static void push_table(struct lcm *ctx, struct LCM_setting_table *table, unsigned int count)
{
	unsigned int i, j;
	unsigned char temp[255] = {0};
	for (i = 0; i < count; i++) {
		unsigned int cmd;
		cmd = table[i].cmd;
		memset(temp, 0, sizeof(temp));
		switch (cmd) {
		case REGFLAG_DELAY:
			msleep(table[i].count);
			break;
		case REGFLAG_END_OF_TABLE:
			break;
		default:
			temp[0] = cmd;
			for (j = 0; j < table[i].count; j++)
				temp[j+1] = table[i].para_list[j];
			lcm_dcs_write(ctx, temp, table[i].count+1);
		}
	}
}
*/

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("[LCM/KE] %s\n", __func__);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}
/* O7 code for HQ-387672 by wangfengbin at 2024/07/23 start */
	lcm_dcs_write_seq_static(ctx, 0x9F, 0xA5, 0xA5);
/* O7 code for HQ-387672 by wangfengbin at 2024/07/23 end */
	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(20);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(150);
	ctx->enabled = false;

	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("[LCM/KE] %s start!\n", __func__);

	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(20);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(150);
	ctx->error = 0;
	ctx->prepared = false;


	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);

	if (IS_ERR(ctx->vddi_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vddi_gpio %ld\n",
			__func__, PTR_ERR(ctx->vddi_gpio));
		return PTR_ERR(ctx->vddi_gpio);
	}
	gpiod_set_value(ctx->vddi_gpio, 0);


	msleep(20);

	if (IS_ERR(ctx->vci_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vci_gpio %ld\n",
			__func__, PTR_ERR(ctx->vci_gpio));
		return PTR_ERR(ctx->vci_gpio);
	}
	gpiod_set_value(ctx->vci_gpio, 0);
	udelay(5000);

	pr_info("[LCM/KE] %s end!\n", __func__);

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("[LCM/KE] %s start!\n", __func__);

	if (ctx->prepared)
		return 0;

	if (IS_ERR(ctx->vddi_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vddi_gpio %ld\n",
			__func__, PTR_ERR(ctx->vddi_gpio));
		return PTR_ERR(ctx->vddi_gpio);
	}
	gpiod_set_value(ctx->vddi_gpio, 1);
	udelay(3000);

	if (IS_ERR(ctx->vci_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vci_gpio %ld\n",
			__func__, PTR_ERR(ctx->vci_gpio));
		return PTR_ERR(ctx->vci_gpio);
	}
	gpiod_set_value(ctx->vci_gpio, 1);
	mdelay(20);

	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;
/* O7 code for HQ-393490 by wangfengbin at 2024/07/08 start */
	fod_in_calibration = false;
/* O7 code for HQ-393490 by wangfengbin at 2024/07/08 end */
/* O7 code for HQ-387739 by wangfengbin at 2024/07/22 start */
	lhbm_flag = false;
/* O7 code for HQ-387739 by wangfengbin at 2024/07/22 end */

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif
	pr_info("[LCM/KE] %s end!\n", __func__);
	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("[LCM/KE] %s start!\n", __func__);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}

/* O7 code for HQ-393066 by wangfengbin at 2024/07/12 start */
#define PLL_CLOCK_FHD 		(678)

#define DATA_RATE		1356

#define MODE0_FPS		60
/* O7 code for HQ-414498 by zhanghuan25 at 2024/10/18 start */
#define MODE0_HFP		254
/* O7 code for HQ-414498 by zhanghuan25 at 2024/10/18 end */
#define MODE0_HSA		16
#define MODE0_HBP		24
#define MODE0_VFP		2452
#define MODE0_VSA		2
#define MODE0_VBP		10

#define MODE2_FPS		90
/* O7 code for HQ-414498 by zhanghuan25 at 2024/10/18 start */
#define MODE2_HFP		254
/* O7 code for HQ-414498 by zhanghuan25 at 2024/10/18 end */
#define MODE2_HSA		16
#define MODE2_HBP		24
#define MODE2_VFP		836
#define MODE2_VSA		2
#define MODE2_VBP		10

#define MODE3_FPS		120
/* O7 code for HQ-414498 by zhanghuan25 at 2024/10/18 start */
#define MODE3_HFP		254
/* O7 code for HQ-414498 by zhanghuan25 at 2024/10/18 end */
#define MODE3_HSA		16
#define MODE3_HBP		24
#define MODE3_VFP		24
#define MODE3_VSA		2
#define MODE3_VBP		10
#define HACT_FHDP		1080
#define VACT_FHDP		2400

static struct drm_display_mode fhd_120_mode = {

	.clock		= (HACT_FHDP + MODE3_HFP + MODE3_HSA + MODE3_HBP)*(VACT_FHDP +
			   MODE3_VFP + MODE3_VSA + MODE3_VBP)*120/1000, //htotal*vtotal*fps
	.hdisplay	= HACT_FHDP,
	.hsync_start	= HACT_FHDP + MODE3_HFP,
	.hsync_end	= HACT_FHDP + MODE3_HFP + MODE3_HSA,
	.htotal		= HACT_FHDP + MODE3_HFP + MODE3_HSA + MODE3_HBP, //1374
	.vdisplay	= VACT_FHDP,
	.vsync_start	= VACT_FHDP + MODE3_VFP,
	.vsync_end	= VACT_FHDP + MODE3_VFP + MODE3_VSA,
	.vtotal		= VACT_FHDP + MODE3_VFP + MODE3_VSA + MODE3_VBP, //2432
};

static struct drm_display_mode fhd_90_mode = {
	.clock		= (HACT_FHDP + MODE2_HFP + MODE2_HSA + MODE2_HBP)*(VACT_FHDP +
			   MODE2_VFP + MODE2_VSA + MODE2_VBP)*90/1000, //htotal*vtotal*fps
	.hdisplay	= HACT_FHDP,
	.hsync_start	= HACT_FHDP + MODE2_HFP,
	.hsync_end	= HACT_FHDP + MODE2_HFP + MODE2_HSA,
	.htotal		= HACT_FHDP + MODE2_HFP + MODE2_HSA + MODE2_HBP, //1374
	.vdisplay	= VACT_FHDP,
	.vsync_start	= VACT_FHDP + MODE2_VFP,
	.vsync_end	= VACT_FHDP + MODE2_VFP + MODE2_VSA,
	.vtotal		= VACT_FHDP + MODE2_VFP + MODE2_VSA + MODE2_VBP, //3248
};

static struct drm_display_mode fhd_60_mode = {
	.clock		= (HACT_FHDP + MODE0_HFP + MODE0_HSA + MODE0_HBP)*(VACT_FHDP +
			   MODE0_VFP + MODE0_VSA + MODE0_VBP)*60/1000, //htotal*vtotal*fps
	.hdisplay	= HACT_FHDP,
	.hsync_start	= HACT_FHDP + MODE0_HFP,
	.hsync_end	= HACT_FHDP + MODE0_HFP + MODE0_HSA,
	.htotal		= HACT_FHDP + MODE0_HFP + MODE0_HSA + MODE0_HBP, //1374
	.vdisplay	= VACT_FHDP,
	.vsync_start	= VACT_FHDP + MODE0_VFP,
	.vsync_end	= VACT_FHDP + MODE0_VFP + MODE0_VSA,
	.vtotal		= VACT_FHDP + MODE0_VFP + MODE0_VSA + MODE0_VBP, //4864
};
/* O7 code for HQ-393066 by wangfengbin at 2024/07/12 end */

#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("[LCM/KE] %s start!\n", __func__);

	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, on);

	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned char data[3] = {0, 0, 0};
	unsigned char id[3] = {0x00, 0x80, 0x00};
	ssize_t ret;

	pr_info("[LCM/KE] %s success\n", __func__);

	ret = mipi_dsi_dcs_read(dsi, 0x4, data, 3);
	if (ret < 0)
		pr_info("%s error\n", __func__);

	pr_info("ATA read data %x %x %x\n", data[0], data[1], data[2]);

	if (data[0] == id[0] &&
			data[1] == id[1] &&
			data[2] == id[2])
		return 1;

	pr_info("ATA expect read data is %x %x %x\n",
			id[0], id[1], id[2]);

	return 1;
}

/* O7 code for HQ-410223 by zhanghuan at 2024/09/05 start */
static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	struct lcm *ctx = panel_to_lcm(this_panel);
	char bl_tb0[] = {0x51, 0x03, 0xFF};
	char hbm_set[] = {0x53, 0x28};
	bool hbm_is_change = false;

	if (level == 245 && last_bl_level == 0 && !skip_first_dimming_on){
		skip_first_dimming_on = true;
	} else {
		skip_first_dimming_on = false;
	}

/* O7 code for HQ-405817 by wangfengbin at 2024/07/31 start */
	pr_info("[LCM/KE] %s:level = %d\n", __func__, level);
	new_bl_level = level;
	if (level > HBM_MAP_MAX_BRIGHTNESS) {
		pr_err("[LCM/KE]%s err level;", __func__);
		return -1;
	} else if (level > NORMAL_MAX_BRIGHTNESS) {
		if (!hbm_on) {
			hbm_set[1] = 0xE8;
			hbm_on = true;
			hbm_is_change = true;
		}
		level = level - (NORMAL_MAX_BRIGHTNESS + 1);
	} else {
		if (hbm_on) {
			hbm_set[1] = 0x28;
			hbm_on = false;
			hbm_is_change = true;
		}
	}
/* O7 code for HQ-405817 by wangfengbin at 2024/07/31 end */
	bl_tb0[1] = ((level & 0xF00) >> 8) & 0xF;
	bl_tb0[2] = level & 0xFF;

	if (!cb)
		return -1;

/* O7 code for HQ-393490 by wangfengbin at 2024/07/08 start */
	pr_info("[LCM/KE] %s:last_bl_level = %d,level = %d\n", __func__, last_bl_level, level);
/* O7 code for HQ-411673 by at 2024/09/19 start */
	if (fod_in_calibration || lhbm_flag) {
		pr_info("[LCM/KE] panel skip set backlight %d due to fod hbm or fod calibration\n", level);
	} else if (hbm_is_change) {
		mutex_lock(&ctx->panel_lock);
		cb(dsi, handle, hbm_set, ARRAY_SIZE(hbm_set));
		mutex_unlock(&ctx->panel_lock);
	}

	mutex_lock(&ctx->panel_lock);
	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	mutex_unlock(&ctx->panel_lock);
/* O7 code for HQ-411673 by zhanghuan25 at 2024/09/19 end */
	last_bl_level = level;
/* O7 code for HQ-393490 by wangfengbin at 2024/07/08 end */

	return 0;
}
/* O7 code for HQ-410223 by zhanghuan at 2024/09/05 end */

static struct mtk_panel_params ext_params_fhd_120 = {
	.data_rate = PLL_CLOCK_FHD * 2,
/* O7 code for HQ-405328 by p-wujiaming1 at 2024/09/05 start */
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9F,
	},
/* O7 code for HQ-405328 by p-wujiaming1 at 2024/09/05 end */
	.physical_width_um = 69550,
	.physical_height_um = 154560,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 40,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 989,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 631,
		.slice_bpg_offset = 651,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,

	},
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
/* O7 code for HQ-414498 by zhanghuan25 at 2024/10/18 start */
	.phy_timcon = {
		.hs_trail = 15,
	},
/* O7 code for HQ-414498 by zhanghuan25 at 2024/10/18 end */
	//.change_fps_by_vfp_send_cmd = 1,
	.dyn_fps = {
		.switch_en=1,
		.vact_timing_fps = 120,
		.dfps_cmd_table[0] = {0, 3 , {0xF0, 0x5A, 0x5A}},
		.dfps_cmd_table[1] = {0, 2 , {0x60, 0x01}},
		.dfps_cmd_table[2] = {0, 2 , {0xF7, 0x0B}},
  	},
};

static struct mtk_panel_params ext_params_fhd_90 = {
	.data_rate = PLL_CLOCK_FHD * 2,
/* O7 code for HQ-405328 by p-wujiaming1 at 2024/09/05 start */
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9F,
	},
/* O7 code for HQ-405328 by p-wujiaming1 at 2024/09/05 end */
	.physical_width_um = 69550,
	.physical_height_um = 154560,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 40,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 989,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 631,
		.slice_bpg_offset = 651,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,

	},
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
/* O7 code for HQ-414498 by zhanghuan25 at 2024/10/18 start */
	.phy_timcon = {
		.hs_trail = 15,
	},
/* O7 code for HQ-414498 by zhanghuan25 at 2024/10/18 end */
	//.change_fps_by_vfp_send_cmd = 1,
	.dyn_fps = {
		.switch_en=1,
		.vact_timing_fps = 120,
		.dfps_cmd_table[0] = {0, 3 , {0xF0, 0x5A, 0x5A}},
		.dfps_cmd_table[1] = {0, 3 , {0xF1, 0x5A, 0x5A}},
		.dfps_cmd_table[2] = {0, 2 , {0x60, 0x01}},
		.dfps_cmd_table[3] = {0, 4 , {0xB0, 0x00, 0x0B, 0xF2}},
		.dfps_cmd_table[4] = {0, 3 , {0xF2, 0x03, 0x44}},
		.dfps_cmd_table[5] = {0, 4 , {0xB0, 0x01, 0x9E, 0xB2}},
		.dfps_cmd_table[6] = {0, 15 , {0xB2, 0x0C, 0xB0, 0x0C, 0x60, 0x0C, 0x20, 0x0B, 0x91, 0x0B, 0x02, 0x0A, 0x65, 0x09, 0x47}},
		.dfps_cmd_table[7] = {0, 19 , {0xB3, 0x08, 0x1A, 0x06, 0xFC, 0x05, 0xCF, 0x04, 0xB1, 0x03, 0x84, 0x01, 0xD7, 0x00, 0x1C, 0x00, 0x1C, 0x00, 0x1C}},
		.dfps_cmd_table[8] = {0, 2 , {0xF7, 0x0B}},
		.dfps_cmd_table[9] = {0, 3 , {0xF1, 0xA5,0xA5}},
		.dfps_cmd_table[10] = {0, 3 , {0xF0, 0xA5,0xA5}},
	},
};

static struct mtk_panel_params ext_params_fhd_60 = {
	.data_rate = PLL_CLOCK_FHD * 2,
/* O7 code for HQ-405328 by p-wujiaming1 at 2024/09/05 start */
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9F,
	},
/* O7 code for HQ-405328 by p-wujiaming1 at 2024/09/05 end */
	.physical_width_um = 69550,
	.physical_height_um = 154560,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 40,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 989,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 631,
		.slice_bpg_offset = 651,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
	},
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
/* O7 code for HQ-414498 by zhanghuan25 at 2024/10/18 start */
	.phy_timcon = {
		.hs_trail = 15,
	},
/* O7 code for HQ-414498 by zhanghuan25 at 2024/10/18 start */
	//.change_fps_by_vfp_send_cmd = 1,
	.dyn_fps = {
  		.switch_en=1,
		.vact_timing_fps = 120,
  		.dfps_cmd_table[0] = {0, 3 , {0xF0, 0x5A, 0x5A}},
  		.dfps_cmd_table[1] = {0, 2 , {0x60, 0x21}},
  		.dfps_cmd_table[2] = {0, 2 , {0xF7, 0x0B}},
  	},
};

int convert_mode_id_to_pmode_id(struct drm_panel *panel,
	struct drm_connector *connector, unsigned int mode)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct drm_display_mode *m, *pmode;
	unsigned int i = 0;

	list_for_each_entry(m, &connector->modes, head) {
		if (i == mode)
			break;
		i++;
	}

	if (list_empty(&ctx->probed_modes)) {
		pr_info("ctx->probed_modes is empty\n");
		return -1;
	}

	i = 0;
	list_for_each_entry(pmode, &ctx->probed_modes, head) {
		if (drm_mode_equal(pmode, m))
			return i;
		i++;
	}

	return -1;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	int pmode_id = convert_mode_id_to_pmode_id(panel, connector, mode);

	pr_info("[LCM/KE] %s mode:%d, pmode_id:%d\n", __func__, mode, pmode_id);

	switch (pmode_id) {
	case 0:
		ext->params = &ext_params_fhd_60;
		break;
	case 1:
		ext->params = &ext_params_fhd_120;
		break;
	case 2:
		ext->params = &ext_params_fhd_90;
		break;
	default:
		ret = 1;
	}

	return ret;
}

static int mtk_panel_ext_param_get(struct drm_panel *panel,
	struct drm_connector *connector,
	struct mtk_panel_params **ext_param,
	unsigned int mode)
{
	int ret = 0;
	int pmode_id = convert_mode_id_to_pmode_id(panel, connector, mode);

	pr_info("[LCM/KE] %s mode:%d, pmode_id:%d\n", __func__, mode, pmode_id);

	switch (pmode_id) {
	case 0:
		*ext_param = &ext_params_fhd_60;
		break;
	case 1:
		*ext_param = &ext_params_fhd_120;
		break;
	case 2:
		*ext_param = &ext_params_fhd_90;
		break;
	default:
		ret = 1;
	}

	if (*ext_param)
		pr_info("[LCM/KE] data_rate:%d\n", (*ext_param)->data_rate);
	else
		pr_info("[LCM/KE] ext_param is NULL;\n");

	return ret;
}

static void mode_switch_to_fhd120(struct drm_panel *panel,
			enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (stage == BEFORE_DSI_POWERDOWN) {
	} else if (stage == AFTER_DSI_POWERON) {
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x01);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0B);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	}
}

static void mode_switch_to_fhd60(struct drm_panel *panel,
			enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (stage == BEFORE_DSI_POWERDOWN) {
	} else if (stage == AFTER_DSI_POWERON) {
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x21);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0B);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

	}
}

static int mode_switch(struct drm_panel *panel,
		struct drm_connector *connector, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	struct lcm *ctx = panel_to_lcm(panel);
	int pmode_id = 0;

	if (cur_mode == dst_mode)
		return ret;

	pmode_id = convert_mode_id_to_pmode_id(panel, connector, dst_mode);
	pr_info("[LCM/KE] %s cur_mode:%d, dst_mode:%d, pmode_id:%d\n", __func__,
		cur_mode, dst_mode, pmode_id);

	switch (pmode_id) {
	case 0:
		mode_switch_to_fhd60(panel, stage);
		break;
	case 1:
		mode_switch_to_fhd120(panel, stage);
		break;
	default:
		ret = 1;
	}

	if (stage == AFTER_DSI_POWERON)
		ctx->pmode_id = pmode_id;

	return ret;
}

static int panel_get_panel_info(struct drm_panel *panel, char *buf)
{
	int count = 0;
	struct lcm *ctx;

	if (!panel) {
		pr_err("[LCM/KE] %s panel is NULL\n", __func__);
		return -EAGAIN;
	}

	ctx = panel_to_lcm(panel);
	count = snprintf(buf, PAGE_SIZE, "%s\n", ctx->panel_info);

	return count;
}
/* O7 code for HQ-393490 by wangfengbin at 2024/07/08 start */
static struct LCM_setting_table fod_calibration[] ={
	{0x51, 02, {0x07, 0xff}},
};
static int backlight_for_calibration(struct drm_panel *panel, unsigned int level)
{
	u8 bl_tb[2] = {0};
	unsigned int bl_level = 0;
	if (!panel) {
		pr_err("[LCM/KE] %s invalid params\n",__func__);
		return -EAGAIN;
	}

	if (level == -1) {
		bl_level = last_bl_level;
		pr_err("[LCM/KE] FOD calibration brightness restore last_bl_level = %d\n", last_bl_level);
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
	pr_info("[LCM/KE] backlight_for_calibration backlight %d\n", bl_level);
	return 0;
}
/* O7 code for HQ-393490 by wangfengbin at 2024/07/08 end */
static bool get_lcm_initialized(struct drm_panel *panel)
{
	bool ret = false;
	struct lcm *ctx;
	if (!panel) {
		pr_err("[LCM/KE] invalid params\n");
		return -EAGAIN;
	}
	ctx = panel_to_lcm(panel);
	ret = ctx->prepared;
	return ret;
}
/* O7 code for HQ-387672 by wangfengbin at 2024/07/23 start */
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
	pr_info("[LCM/KE] lcm_esd_restore_backlight \n");
}
#endif
/* O7 code for HQ-387672 by wangfengbin at 2024/07/23 end */
/* O7 code for HQ-393489 by wangfengbin at 2024/07/03 start */
static int panel_set_lhbm_fod(struct mtk_dsi *dsi,  enum local_hbm_state lhbm_state)
{
	struct lcm *ctx = NULL;
/* O7 code for HQ-393490 by wangfengbin at 2024/07/08 start */
	int bl_level = 0;
/* O7 code for HQ-393490 by wangfengbin at 2024/07/08 end */
	struct mi_dsi_panel_cfg *mi_cfg = NULL;

	if(!dsi || !dsi->panel) {
		pr_err("[LCM/KE] %s:panel is NULL\n", __func__);
		return -1;
	}
	mi_cfg = &dsi->mi_cfg;
	ctx = panel_to_lcm(dsi->panel);
	if (!ctx || !ctx->enabled) {
		pr_err("[LCM/KE] %s: panel isn't enabled\n", __func__);
		return -1;
	}
/* O7 code for HQ-393490 by wangfengbin at 2024/07/08 start */
	if (fod_in_calibration) {
		bl_level = bl_level_for_fod;
/* O7 code for HQ-387739 by wangfengbin at 2024/07/22 start */
	} else if (ctx->doze_brightness == DOZE_BRIGHTNESS_LBM) {
		bl_level = doze_lbm_bl;
	} else if (ctx->doze_brightness == DOZE_BRIGHTNESS_HBM) {
		bl_level = doze_hbm_bl;
/* O7 code for HQ-387739 by wangfengbin at 2024/07/22 end */
	} else {
		bl_level = last_bl_level;
	}
	if (bl_level > hbm_level) {
		if((lhbm_state == LOCAL_HBM_OFF_TO_NORMAL) || (lhbm_state == LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT) || (lhbm_state == LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE)) {
			pr_info("[LCM/KE] exit lhbm to hbm!\n");
		} else {
			pr_info("[LCM/KE] The bl_level is %d,exit hbm mode to normal!\n", bl_level);
			bl_level = hbm_level;
			mi_disp_panel_ddic_send_cmd(exit_hbm_to_normal, ARRAY_SIZE(exit_hbm_to_normal), true);
		}
	}
/* O7 code for HQ-393490 by wangfengbin at 2024/07/08 end */
	pr_info("[LCM/KE] %s local hbm_state :%d, HBM_ON:%d \n", __func__, lhbm_state, hbm_on);

	switch (lhbm_state) {
	case LOCAL_HBM_OFF_TO_NORMAL://0
	case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT://10
	case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE://11
		pr_info("[LCM/KE] LOCAL_HBM_NORMAL off\n");
/* O7 code for HQ-422122 by zhanghuan25 at 2024/10/12 start */
		if ((ctx->doze_brightness != DOZE_BRIGHTNESS_LBM) && (ctx->doze_brightness != DOZE_BRIGHTNESS_HBM)) {
			bl_level =  last_bl_level;
		}

		normal_hbm_on_001[1].para_list[0] = (bl_level >> 8) & 0xFF;
		normal_hbm_on_001[1].para_list[1] = bl_level & 0xFF;
/* O7 code for HQ-422122 by zhanghuan25 at 2024/10/12 end  */

/* O7 code for HQ-393490 by wangfengbin at 2024/07/08 start */
		lhbm_flag = false;
/* O7 code for HQ-393490 by wangfengbin at 2024/07/08 end */
		mutex_lock(&ctx->lhbm_lock);
/* O7 code for HQ-405817 by wangfengbin at 2024/07/31 start */
		if(hbm_on){
			mi_disp_panel_ddic_send_cmd(local_hbm_normal_off, ARRAY_SIZE(local_hbm_normal_off),  true);
			mi_disp_panel_ddic_send_cmd(normal_hbm_on_001, ARRAY_SIZE(normal_hbm_on_001), true);
		} else {
			mi_disp_panel_ddic_send_cmd(local_hbm_normal_off, ARRAY_SIZE(local_hbm_normal_off),  true);
		}
		/* O7 code for HQ-404413 by p-wangfengbin at 2024/08/25 start */
		udelay(8400);
		mi_disp_panel_ddic_send_cmd(local_hbm_on_B4, ARRAY_SIZE(local_hbm_on_B4),  true);
		if(!hbm_on){
			mi_disp_panel_ddic_send_cmd(dimming_off, ARRAY_SIZE(dimming_off),  true);
		}
		/* O7 code for HQ-404413 by p-wangfengbin at 2024/08/25 end */
/* O7 code for HQ-405817 by wangfengbin at 2024/07/31 end */
		mutex_unlock(&ctx->lhbm_lock);
		break;
	case LOCAL_HBM_HLPM_WHITE_1000NIT://6
	case LOCAL_HBM_NORMAL_WHITE_1000NIT://1
		pr_info("[LCM/KE] LOCAL_HBM_NORMAL_WHITE_1000NIT \n");
/* O7 code for HQ-393490 by wangfengbin at 2024/07/08 start */
		lhbm_flag = true;
/* O7 code for HQ-393490 by wangfengbin at 2024/07/08 end */
		mutex_lock(&ctx->lhbm_lock);
/* O7 code for HQ-405817 by wangfengbin at 2024/07/31 start */
		if(hbm_on){
			mi_disp_panel_ddic_send_cmd(normal_hbm_off_7FF, ARRAY_SIZE(normal_hbm_off_7FF), true);
			mi_disp_panel_ddic_send_cmd(local_hbm_normal_white_1200nit, ARRAY_SIZE(local_hbm_normal_white_1200nit), true);
		} else {
			mi_disp_panel_ddic_send_cmd(local_hbm_normal_white_1200nit, ARRAY_SIZE(local_hbm_normal_white_1200nit), true);
		}
/* O7 code for HQ-405817 by wangfengbin at 2024/07/31 end */
		mutex_unlock(&ctx->lhbm_lock);
		break;
	case LOCAL_HBM_HLPM_WHITE_110NIT://7
	case LOCAL_HBM_NORMAL_WHITE_110NIT://4
		pr_info("[LCM/KE] LOCAL_HBM_NORMAL_WHITE_110NIT \n");
/* O7 code for HQ-393490 by wangfengbin at 2024/07/08 start */
		lhbm_flag = true;
/* O7 code for HQ-393490 by wangfengbin at 2024/07/08 end */
		mutex_lock(&ctx->lhbm_lock);
/* O7 code for HQ-405817 by wangfengbin at 2024/07/31 start */
		if(hbm_on){
			mi_disp_panel_ddic_send_cmd(normal_hbm_off_7FF, ARRAY_SIZE(normal_hbm_off_7FF), true);
			mi_disp_panel_ddic_send_cmd(local_hbm_normal_white_200nit, ARRAY_SIZE(local_hbm_normal_white_200nit), true);
		} else {
			mi_disp_panel_ddic_send_cmd(local_hbm_normal_white_200nit, ARRAY_SIZE(local_hbm_normal_white_200nit), true);
		}
/* O7 code for HQ-405817 by wangfengbin at 2024/07/31 end */
		mutex_unlock(&ctx->lhbm_lock);
		break;
	case LOCAL_HBM_NORMAL_GREEN_500NIT://5
		pr_info("[LCM/KE] LOCAL_HBM_NORMAL_GREEN_500NIT \n");
/* O7 code for HQ-393490 by wangfengbin at 2024/07/08 start */
		lhbm_flag = true;
/* O7 code for HQ-393490 by wangfengbin at 2024/07/08 end */
		mutex_lock(&ctx->lhbm_lock);
/* O7 code for HQ-405817 by wangfengbin at 2024/07/31 start */
		if(hbm_on){
			mi_disp_panel_ddic_send_cmd(normal_hbm_off_7FF, ARRAY_SIZE(normal_hbm_off_7FF), true);
			mi_disp_panel_ddic_send_cmd(local_hbm_normal_green_500nit, ARRAY_SIZE(local_hbm_normal_green_500nit), true);
		} else {
			mi_disp_panel_ddic_send_cmd(local_hbm_normal_green_500nit, ARRAY_SIZE(local_hbm_normal_green_500nit), true);
		}
/* O7 code for HQ-405817 by wangfengbin at 2024/07/31 end */
		mutex_unlock(&ctx->lhbm_lock);
		break;
	default:
		pr_info("[LCM/KE] invalid local hbm value\n");
		break;
	}

	return 0;
}

static int panel_fod_lhbm_init(struct mtk_dsi* dsi)
{
	if (!dsi) {
		pr_err("[LCM/KE] invalid dsi point\n");
		return -1;
	}
	pr_info("[LCM/KE]panel_fod_lhbm_init enter\n");
	dsi->display_type = "primary";
	dsi->mi_cfg.lhbm_ui_ready_delay_frame = 3;
	dsi->mi_cfg.lhbm_ui_ready_delay_frame_aod = 3;
	dsi->mi_cfg.local_hbm_enabled = 1;
	return 0;
}

static int panel_normal_hbm_control(struct drm_panel *panel, uint32_t level)
{
	struct lcm *ctx;
	if (!panel) {
		pr_err("[LCM/KE]invalid params\n");
		return -EAGAIN;
	}
	ctx = panel_to_lcm(panel);
	mutex_lock(&ctx->panel_lock);
	if (level == 1) {
		/* O7 code for HQ-387739 by wangfengbin at 2024/07/22 start */
		mi_disp_panel_ddic_send_cmd(normal_hbm_on, ARRAY_SIZE(normal_hbm_on), true);
	} else if (level == 0) {
		mi_disp_panel_ddic_send_cmd(normal_hbm_off, ARRAY_SIZE(normal_hbm_off), true);
		/* O7 code for HQ-387739 by wangfengbin at 2024/07/22 end */
	}
	mutex_unlock(&ctx->panel_lock);
	return 0;
}

static int lcm_setbacklight_control(struct drm_panel *panel, unsigned int level)
{
	struct lcm *ctx;
	if (!panel) {
		pr_err("[LCM/KE]invalid params\n");
		return -EAGAIN;
	}
	ctx = panel_to_lcm(panel);
	if (level > 2047) {
		ctx->hbm_enabled = true;
	}
	pr_err("[LCM/KE] lcm_setbacklight_control backlight %d\n", level);
	return 0;
}
/* O7 code for HQ-393489 by wangfengbin at 2024/07/03 end */

/*O7 code for HQ-393513 by wangfengbin at 2024/07/04 start*/
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
			pr_info("[LCM/KE][%s]: white_point = %s, size = %lu\n", __func__, buf, tmp_size);
		}
	} else {
		pr_info("[LCM/KE][%s]:find chosen failed\n", __func__);
	}
	return tmp_size;
}
/*O7 code for HQ-393513 by wangfengbin at 2024/07/04 end*/
/* O7 code for HQ-387739 by wangfengbin at 2024/07/22 start */
static int panel_set_doze_brightness(struct drm_panel *panel, int doze_brightness) {
	struct lcm *ctx = NULL;
	int ret = 0;
	if (!panel) {
		pr_err("[LCM/KE] %s: invalid params\n", __func__);
		return -EAGAIN;
	}
	ctx = panel_to_lcm(panel);
	if (!ctx || !ctx->enabled) {
		pr_err("[LCM/KE] %s: panel isn't enabled\n", __func__);
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
			mi_disp_panel_ddic_send_cmd(LCM_AOD_HBM, ARRAY_SIZE(LCM_AOD_HBM), true);
			break;
		case DOZE_BRIGHTNESS_LBM:
			mi_disp_panel_ddic_send_cmd(LCM_AOD_LBM, ARRAY_SIZE(LCM_AOD_LBM), true);
			break;
		case DOZE_TO_NORMAL:
		/* O7 code for HQ-404421 by wangfengbin at 2024/07/30 start */
			//mi_disp_panel_ddic_send_cmd(LCM_AOD_TO_NORMAL, ARRAY_SIZE(LCM_AOD_TO_NORMAL), true);
		/* O7 code for HQ-404421 by wangfengbin at 2024/07/30  */
			break;
		default:
			pr_err("[LCM/KE]%s: doze_brightness is invalid\n", __func__);
			ret = -1;
			goto err;
	}
	pr_info("[LCM/KE]doze_brightness = %d\n", doze_brightness);
	ctx->doze_brightness = doze_brightness;
err:
	return ret;
}

int panel_get_doze_brightness(struct drm_panel *panel, u32 *brightness) {
	struct lcm *ctx = NULL;
	if (!panel) {
		pr_err("[LCM/KE]%s invalid params\n", __func__);
		return -EAGAIN;
	}
	ctx = panel_to_lcm(panel);
	if (!ctx || !ctx->enabled) {
		pr_err("[LCM/KE]%s: panel isn't enabled\n", __func__);
		return -1;
	}
	*brightness = ctx->doze_brightness;
	return 0;
}

static int panel_set_only_aod_backlight(struct drm_panel *panel, int doze_brightness)
{
	struct lcm *ctx = NULL;

	if (!panel) {
		pr_err("[LCM/KE]%s invalid params\n", __func__);
		return -1;
	}

	ctx = panel_to_lcm(panel);
	if (!ctx) {
		pr_err("[LCM/KE]%s: ctx is null\n", __func__);
		return -1;
	}
	pr_info("[LCM/KE]%s set_doze_brightness state = %d",__func__, doze_brightness);
	if ((DOZE_BRIGHTNESS_LBM  == doze_brightness || DOZE_BRIGHTNESS_HBM  == doze_brightness) && lhbm_flag == false) {
		if (DOZE_BRIGHTNESS_LBM  == doze_brightness) {
			mi_disp_panel_ddic_send_cmd(LCM_AOD_HBM, ARRAY_SIZE(LCM_AOD_HBM), true);
		} else if (DOZE_BRIGHTNESS_HBM == doze_brightness) {
			mi_disp_panel_ddic_send_cmd(LCM_AOD_LBM, ARRAY_SIZE(LCM_AOD_LBM), true);
		}
	}
	ctx->doze_brightness = doze_brightness;
	pr_info("[LCM/KE]%s set doze_brightness %d end -\n", __func__, doze_brightness);
	return 0;
}
/* O7 code for HQ-387739 by wangfengbin at 2024/07/22 end */
/* O7 code for HQ-392083 by wangfengbin at 2024/07/24 start */
static int panel_set_gir_on(struct drm_panel *panel)
{
	struct lcm *ctx = NULL;
	int ret = 0;
	if (!panel) {
		pr_err("[LCM/KE] %s: panel is NULL\n", __func__);
		ret = -1;
		goto err;
	}
	ctx = panel_to_lcm(panel);
	if (!ctx || !ctx->enabled) {
		pr_err("[LCM/KE] %s: panel isn't enabled\n", __func__);
	} else {
		pr_info("[LCM/KE] %s: + ctx->gir_status = %d  \n", __func__, ctx->gir_status);
		//if (cmd_msg != NULL) {
			mi_disp_panel_ddic_send_cmd(local_hbm_gir_on, ARRAY_SIZE(local_hbm_gir_on), true);
		//}
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
		pr_err("[LCM/KE]%s: panel is NULL\n", __func__);
		goto err;
	}
	ctx = panel_to_lcm(panel);
	if (!ctx || !ctx->enabled) {
		pr_err("[LCM/KE] %s: panel isn't enabled\n", __func__);
	} else {
		pr_info("[LCM/KE] %s: + ctx->gir_status = %d \n", __func__, ctx->gir_status);
		//if (cmd_msg != NULL) {
			mi_disp_panel_ddic_send_cmd(local_hbm_gir_off, ARRAY_SIZE(local_hbm_gir_off), true);
		//}
		ctx->gir_status = 0;
	}
err:
	return ret;
}

static int panel_get_gir_status(struct drm_panel *panel)
{
	struct lcm *ctx = NULL;
	if (!panel) {
		pr_err("[LCM/KE] %s; panel is NULL\n", __func__);
		return -1;
	}
	ctx = panel_to_lcm(panel);
	if (!ctx) {
		pr_err("[LCM/KE]%s: ctx is null\n", __func__);
		return -1;
	}
	return ctx->gir_status;
}
/* O7 code for HQ-392083 by wangfengbin at 2024/07/24 end */

static int local_hbm_dimming(struct drm_panel *panel, bool en)
{
	struct LCM_setting_table local_hbm_dimming[] = {
		{0x53, 01, {0x20}},
	};

	if (en) {
		if (new_bl_level > NORMAL_MAX_BRIGHTNESS) {
			local_hbm_dimming[0].para_list[0] = 0xE8;
			pr_info("Enter 0xE8\n");
		} else {
			local_hbm_dimming[0].para_list[0] = 0x28;
			pr_info("Enter 0X28\n");
		}
	}
	if (!panel) {
		pr_err("invalid params\n");
		return -1;
	} else {
		if (skip_first_dimming_on) {
			skip_first_dimming_on = false;
			pr_info("%s: skip_first_dimming_on\n", __func__);
			return 0;
		} else {
			mi_disp_panel_ddic_send_cmd(local_hbm_dimming, ARRAY_SIZE(local_hbm_dimming), false);
			pr_info("%s: open_dimming_after_power_on\n", __func__);
			return 1;
		}
	}
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
	.ext_param_set = mtk_panel_ext_param_set,
	.ext_param_get = mtk_panel_ext_param_get,
	.mode_switch = mode_switch,
	.get_panel_initialized = get_lcm_initialized,
/* O7 code for HQ-387672 by wangfengbin at 2024/07/23 start */
#ifdef CONFIG_MI_ESD_SUPPORT
	.esd_restore_backlight = lcm_esd_restore_backlight,
#endif
/* O7 code for HQ-387672 by wangfengbin at 2024/07/23 end */
/* O7 code for HQ-393489 by wangfengbin at 2024/07/03 start */
	.get_panel_info = panel_get_panel_info,
	.set_lhbm_fod = panel_set_lhbm_fod,
	.panel_fod_lhbm_init = panel_fod_lhbm_init,
	.normal_hbm_control = panel_normal_hbm_control,
	.setbacklight_control = lcm_setbacklight_control,
/* O7 code for HQ-393489 by wangfengbin at 2024/07/03 end */
/* O7 code for HQ-393513 by wangfengbin at 2024/07/04 start */
	.get_wp_info = panel_get_wp_info,
/* O7 code for HQ-393513 by wangfengbin at 2024/07/04 end */
/* O7 code for HQ-393490 by wangfengbin at 2024/07/08 start */
	.backlight_for_calibration = backlight_for_calibration,
/* O7 code for HQ-393490 by wangfengbin at 2024/07/08 end */
/* O7 code for HQ-387739 by wangfengbin at 2024/07/22 start */
	.set_doze_brightness = panel_set_doze_brightness,
	.get_doze_brightness = panel_get_doze_brightness,
	.set_only_aod_backlight = panel_set_only_aod_backlight,
/* O7 code for HQ-387739 by wangfengbin at 2024/07/22 end */
/* O7 code for HQ-392083 by wangfengbin at 2024/07/24 start */
	.panel_set_gir_on = panel_set_gir_on,
	.panel_set_gir_off = panel_set_gir_off,
	.panel_get_gir_status = panel_get_gir_status,
/* O7 code for HQ-392083 by wangfengbin at 2024/07/24 end */
	.local_hbm_dimming = local_hbm_dimming,
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

static int lcm_get_modes(struct drm_panel *panel,
			struct drm_connector *connector)
{
	struct drm_display_mode *mode0;
	struct drm_display_mode *mode1;
	struct drm_display_mode *mode2;
	struct lcm *ctx = panel_to_lcm(panel);

	mode0 = drm_mode_duplicate(connector->dev, &fhd_60_mode);
	if (!mode0) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			fhd_60_mode.hdisplay, fhd_60_mode.vdisplay,
			drm_mode_vrefresh(&fhd_60_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode0);
	mode0->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode0);
	ctx->pmode_id = 0;
	list_add_tail(&fhd_60_mode.head, &ctx->probed_modes);

	mode1 = drm_mode_duplicate(connector->dev, &fhd_120_mode);
	if (!mode1) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			fhd_120_mode.hdisplay,
			fhd_120_mode.vdisplay,
			drm_mode_vrefresh(&fhd_120_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode1);
	mode1->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode1);
	list_add_tail(&fhd_120_mode.head, &ctx->probed_modes);

	mode2 = drm_mode_duplicate(connector->dev, &fhd_90_mode);
	if (!mode2) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			fhd_90_mode.hdisplay,
			fhd_90_mode.vdisplay,
			drm_mode_vrefresh(&fhd_90_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode2);
	mode2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode2);
	list_add_tail(&fhd_90_mode.head, &ctx->probed_modes);

	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct lcm *ctx;
	struct device_node *backlight;
	int ret;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	pr_info("[LCM/KE] %s+ dsi_panel_o17_38_0c_0a_fhdp_vdo\n", __func__);

	dsi_node = of_get_parent(dev->of_node);
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

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST
			  | MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_NO_EOT_PACKET | MIPI_DSI_CLOCK_NON_CONTINUOUS;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->vddi_gpio =
		devm_gpiod_get(ctx->dev, "vddi", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vddi_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vddi_gpio %ld\n",
			__func__, PTR_ERR(ctx->vddi_gpio));
		return PTR_ERR(ctx->vddi_gpio);
	}
	gpiod_set_value(ctx->vddi_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->vddi_gpio);

	ctx->vci_gpio =
		devm_gpiod_get(ctx->dev, "vci", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vci_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vci_gpio %ld\n",
			__func__, PTR_ERR(ctx->vci_gpio));
		return PTR_ERR(ctx->vci_gpio);
	}
	devm_gpiod_put(ctx->dev, ctx->vci_gpio);

#ifdef CONFIG_FACTORY_BUILD
	pr_info("Factory build no need ESD function!\n");
#else
	//ext_params_fhd_120.err_flag_irq_gpio = of_get_named_gpio_flags(
	//	dev->of_node, "mi,esd-err-irq-gpio",
	//	0, (enum of_gpio_flags *)&(ext_params_fhd_120.err_flag_irq_flags));
#endif
#ifndef CONFIG_MTK_DISP_NO_LK
	ctx->prepared = true;
	ctx->enabled = true;
#endif
/* O7 code for HQ-392083 by wangfengbin at 2024/07/24 start */
	ctx->gir_status = 1;
/* O7 code for HQ-392083 by wangfengbin at 2024/07/24 end */
/* O7 code for HQ-393489 by wangfengbin at 2024/07/03 start */
	ctx->panel_info = panel_name;
/* O7 code for HQ-393489 by wangfengbin at 2024/07/03 end */
	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &lcm_drm_funcs;

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params_fhd_60, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif
/* O7 code for HQ-393489 by wangfengbin at 2024/07/03 start */
	ctx->hbm_enabled = false;
	this_panel = &ctx->panel;
	//check_is_need_fake_resolution(dev);

	/*cmd_msg = (struct mtk_ddic_dsi_msg *)vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	if (cmd_msg == NULL) {
		ret= -ENOMEM;
		pr_err("[LCM/KE] fail to vmalloc for cmd_msg\n");
	} else {
		memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));
    }*/
/* O7 code for HQ-393489 by wangfengbin at 2024/07/03 end */
/* O7 code for HQ-398849 by wangfengbin at 2024/07/04 start */
	hq_regiser_hw_info(HWID_LCM, "oncell,vendor:38,IC:0c");
/* O7 code for HQ-398849 by wangfengbin at 2024/07/04 end */
	// ctx->panel_info = panel_name;
	INIT_LIST_HEAD(&ctx->probed_modes);
	pr_info("[LCM/KE]%s-panel-o7-38-0c-0a-dsc-vdo\n", __func__);

	return ret;
}

static void lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif
/* O7 code for HQ-393489 by wangfengbin at 2024/07/03 start */
	pr_info("[LCM/KE] %s start \n", __func__);
	//vfree(cmd_msg);
/* O7 code for HQ-393489 by wangfengbin at 2024/07/03 end */
	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
#endif

}

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "o7_38_0c_0a_dsc_vdo,lcm", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-o7-38-0c-0a-dsc-vdo",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Fengbin Wang <wangfengbin@huaqin.com>");
MODULE_DESCRIPTION("panel-o7-38-0c-0a-dsc-vdo Panel Driver");
MODULE_LICENSE("GPL v2");
