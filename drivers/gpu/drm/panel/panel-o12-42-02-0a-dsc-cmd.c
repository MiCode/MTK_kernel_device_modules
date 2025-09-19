// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
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
#include <linux/of_gpio.h>
#include <linux/platform_device.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "mtk_panel_ext.h"
#include "mtk_drm_graphics_base.h"
#include "mtk_log.h"
#endif
#include <mtk_dsi.h>
#include "../mediatek/mediatek_v2/mi_disp/mi_panel_ext.h"
#include "mi_dsi_panel.h"
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

#include <linux/atomic.h>
#include <uapi/drm/mi_disp.h>
#include "mi_panel_ext.h"
#include "mi_dsi_panel.h"
#include "include/panel-o12-42-02-0a-dsc-cmd.h"
#include "include/panel_o12_42_02_0a_alpha_data.h"

static char buildid_cmdline[4] = {0};
static char bl_tb0[] = {0x51, 0x3, 0xff};
static int current_fps = 120;

#define SLEEP_IN_VIANO_KEEP_HBM_THRESHOLD 11000
#define MAX_BRIGHTNESS_CLONE 	16383
#define FACTORY_MAX_BRIGHTNESS 	8191
#define PANEL_BUILD_ID_P00  0x00
#define PANEL_BUILD_ID_P01  0x10
#define PANEL_BUILD_ID_P10  0x40
#define PANEL_BUILD_ID_P11  0x50
#define PANEL_BUILD_ID_P20  0x80
#define PANEL_BUILD_ID_P21  0x90
#define PANEL_BUILD_ID_MP   0xC0
static unsigned char panel_build_id = PANEL_BUILD_ID_MP;
#define DDIC_DBV_PN 1
#define DDIC_FPS_PN 0

#define DBI_TEMP_INDEX1 0x2D /* temperature 45 */
#define DBI_TEMP_INDEX2 0x28 /* temperature 40 */
#define DBI_TEMP_INDEX3 0x24 /* temperature 36 */
#define DBI_TEMP_INDEX4 0x20 /* temperature 32 */
#define DBI_TEMP_INDEX5 0x14 /* temperature 20 */
#define DBI_TEMP_OFF    0x00

#if defined(CONFIG_VIS_DISPLAY_DALI)
//Novatek ASIC
#include "vis_display.h"
unsigned int nvt_get_lcm_id_42(void)
{
	return 0x00000042;
}
EXPORT_SYMBOL(nvt_get_lcm_id_42);
#endif
//localhbm white gamma
static char oled_wp_cmdline[16] = {0};
static char oled_lhbm_cmdline[90] = {0};
static char panel_sn_cmdline[15] = {0};
//static bool lhbm_w900_update_flag = true;
//static bool lhbm_w110_update_flag = true;
//static bool lhbm_g500_update_flag = true;
//static bool lhbm_w900_readbackdone;
//static bool lhbm_w110_readbackdone;
//static bool lhbm_g500_readbackdone;
struct LHBM_WHITEBUF {
	unsigned char nit_700_80_250[6];
	unsigned char nit_80_2_250[6];
	unsigned char nit_1600_700_250[6];

	unsigned char nit_700_80_1300_1000_1018_1005[6];
	unsigned char nit_700_80_1300_996_1015_1005[6];
	unsigned char nit_700_80_1300_995_1012_1003[6];
	unsigned char nit_700_80_1300_993_1010_1000[6];
	unsigned char nit_700_80_1300_990_1006_998[6];
	unsigned char nit_700_80_1300_990_1003_998[6];
	unsigned char nit_700_80_1300_990_1000_995[6];
	unsigned char nit_700_80_1300_988_998_992[6];
	unsigned char nit_700_80_1300_985_998_990[6];
	unsigned char nit_700_80_1300_985_996_990[6];


	unsigned char nit_80_2_1300[6];
	unsigned char nit_1600_700_1300[6];
	unsigned char nit_700_80_500_100[2];
	unsigned char nit_700_80_500_101[2];
	unsigned char nit_80_2_500[2];
	unsigned char nit_1600_700_500[2];
};

//static struct LHBM_WHITEBUF lhbm_whitebuf;
enum lhbm_cmd_type {
	TYPE_WHITE_1300 = 0,
	TYPE_WHITE_250,
	TYPE_GREEN_500,
	TYPE_LHBM_OFF,
	TYPE_HLPM_W1300,
	TYPE_HLPM_W250,
	TYPE_HLPM_G500,
	TYPE_HLPM_OFF,
	TYPE_MAX
};

static atomic_t doze_enable = ATOMIC_INIT(0);
static unsigned int bl_value = 0;
static struct lcm *panel_ctx;

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

#ifdef PANEL_SUPPORT_READBACK
static int lcm_dcs_read(struct lcm *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	int ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret, cmd);
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

//AMOLED 1.2V:DVDD_1P2,GPIO92
//AMOLED 1.8V:VDDIO_1P8,VCN18IO
//AMOLED 3V:VCI_3P0,VIBR

static struct regulator *disp_vci;
static struct regulator *disp_vddi;
//static struct regulator *disp_dvdd;

static int lcm_panel_vci_regulator_init(struct device *dev)
{
	static int vibr_regulator_inited;
	int ret = 0;

	if (vibr_regulator_inited)
		return ret;

	/* please only get regulator once in a driver */
	disp_vci = regulator_get(dev, "vibr");
	if (IS_ERR(disp_vci)) { /* handle return value */
		ret = PTR_ERR(disp_vci);
		pr_err("get disp_vci fail, error: %d\n", ret);
		return ret;
	}

	vibr_regulator_inited = 1;
	return ret; /* must be 0 */
}

static int lcm_panel_vci_enable(struct device *dev)
{
	int ret = 0;
	int retval = 0;
	int status = 0;
	static unsigned int vibr_start_up = 1;

	pr_info("%s +\n",__func__);

	/* set voltage with min & max*/
	ret = regulator_set_voltage(disp_vci, 3000000, 3000000);
	if (ret < 0)
		pr_err("set voltage disp_vci fail, ret = %d\n", ret);
	retval |= ret;

	status = regulator_is_enabled(disp_vci);
	pr_info("%s regulator_is_enabled = %d, vibr_start_up = %d\n", __func__, status, vibr_start_up);
	if(!status || vibr_start_up){
		/* enable regulator */
		ret = regulator_enable(disp_vci);
		if (ret < 0)
			pr_err("enable regulator disp_vci fail, ret = %d\n", ret);
		vibr_start_up = 0;
		retval |= ret;
	}

	pr_info("%s -\n",__func__);
	return retval;
}

static int lcm_panel_vci_disable(struct device *dev)
{
	int ret = 0;
	int retval = 0;
	int status = 0;

	pr_info("%s +\n",__func__);

	status = regulator_is_enabled(disp_vci);
	pr_info("%s regulator_is_enabled = %d\n", __func__, status);
	if(status){
		ret = regulator_disable(disp_vci);
		if (ret < 0)
			pr_err("disable regulator disp_vci fail, ret = %d\n", ret);
	}
	retval |= ret;

	pr_info("%s -\n",__func__);

	return retval;
}

static int lcm_panel_vddi_regulator_init(struct device *dev)
{
	static int vcn18io_regulator_inited;
	int ret = 0;

	if (vcn18io_regulator_inited)
               return ret;

	/* please only get regulator once in a driver */
	disp_vddi = regulator_get(dev, "vcn18io");
	if (IS_ERR(disp_vddi)) { /* handle return value */
		ret = PTR_ERR(disp_vddi);
		pr_err("get disp_vddi fail, error: %d\n", ret);
		return ret;
	}

	vcn18io_regulator_inited = 1;
	return ret; /* must be 0 */
}

static int lcm_panel_vddi_enable(struct device *dev)
{
	int ret = 0;
	int retval = 0;
	int status = 0;
	static unsigned int vcn18io_start_up = 1;

	pr_info("%s +\n",__func__);

	/* set voltage with min & max*/
	ret = regulator_set_voltage(disp_vddi, 1800000, 1800000);
	if (ret < 0)
		pr_err("set voltage disp_vddi fail, ret = %d\n", ret);
	retval |= ret;

	status = regulator_is_enabled(disp_vddi);
	pr_info("%s regulator_is_enabled = %d, vcn18io_start_up = %d\n", __func__, status, vcn18io_start_up);
	if (!status || vcn18io_start_up){
		/* enable regulator */
		ret = regulator_enable(disp_vddi);
		if (ret < 0)
			pr_err("enable regulator disp_vddi fail, ret = %d\n", ret);
		vcn18io_start_up = 0;
		retval |= ret;
	}

	pr_info("%s -\n",__func__);
	return retval;
}

static int lcm_panel_vddi_disable(struct device *dev)
{
	int ret = 0;
	int retval = 0;
	int status = 0;

	pr_info("%s +\n",__func__);

	status = regulator_is_enabled(disp_vddi);
	pr_info("%s regulator_is_enabled = %d\n", __func__, status);
	if (status){
		ret = regulator_disable(disp_vddi);
		if (ret < 0)
			pr_err("disable regulator disp_vddi fail, ret = %d\n", ret);
	}

	retval |= ret;
	pr_info("%s -\n",__func__);

	return retval;
}

#if 0
static int lcm_panel_dvdd_regulator_init(struct device *dev)
{
	static int vbuck8_regulator_inited;
	int ret = 0;

	if (vbuck8_regulator_inited)
               return ret;

	/* please only get regulator once in a driver */
	disp_dvdd = regulator_get(dev, "vbuck8");
	if (IS_ERR(disp_dvdd)) { /* handle return value */
		ret = PTR_ERR(disp_dvdd);
		pr_err("get disp_dvdd fail, error: %d\n", ret);
		return ret;
	}

	vbuck8_regulator_inited = 1;
	return ret; /* must be 0 */
}

static int lcm_panel_dvdd_enable(struct device *dev)
{
	int ret = 0;
	int retval = 0;
	int status = 0;
	static unsigned int vbuck8_start_up = 1;

	pr_info("%s +\n",__func__);

	/* set voltage with min & max*/
	ret = regulator_set_voltage(disp_dvdd, 1200000, 1200000);
	if (ret < 0)
		pr_err("set voltage disp_dvdd fail, ret = %d\n", ret);
	retval |= ret;

	status = regulator_is_enabled(disp_dvdd);
	pr_info("%s regulator_is_enabled = %d, vbuck8_start_up = %d\n", __func__, status, vbuck8_start_up);
	if (!status || vbuck8_start_up){
		/* enable regulator */
		ret = regulator_enable(disp_dvdd);
		if (ret < 0)
			pr_err("enable regulator disp_dvdd fail, ret = %d\n", ret);
		vbuck8_start_up = 0;
		retval |= ret;
	}

	pr_info("%s -\n",__func__);
	return retval;
}

static int lcm_panel_dvdd_disable(struct device *dev)
{
	int ret = 0;
	int retval = 0;
	int status = 0;

	pr_info("%s +\n",__func__);

	status = regulator_is_enabled(disp_dvdd);
	pr_info("%s regulator_is_enabled = %d\n", __func__, status);
	if (status){
		ret = regulator_disable(disp_dvdd);
		if (ret < 0)
			pr_err("disable regulator disp_dvdd fail, ret = %d\n", ret);
	}

	retval |= ret;
	pr_info("%s -\n",__func__);

	return retval;
}
#endif

#define REGFLAG_DELAY       0xFFFC
#define REGFLAG_UDELAY  0xFFFB
#define REGFLAG_END_OF_TABLE    0xFFFD
#define REGFLAG_RESET_LOW   0xFFFE
#define REGFLAG_RESET_HIGH  0xFFFF

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

static int lcm_panel_poweroff(struct drm_panel *panel)
{
	struct lcm *ctx;

	if (panel == NULL)
		return 0;
	ctx = panel_to_lcm(panel);

	if (ctx->prepared)
		return 0;

	pr_info("%s\n",__func__);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	udelay(2000);

	lcm_panel_vci_disable(ctx->dev); //3.0,VIBR30
	udelay(2000);

	ctx->dvdd_gpio = devm_gpiod_get_index(ctx->dev,
		"dvdd", 0, GPIOD_OUT_HIGH); //1.2V,OLED_1P2
	if (IS_ERR(ctx->dvdd_gpio)) {
		dev_err(ctx->dev, "%s: cannot get dvdd gpio %ld\n",
			__func__, PTR_ERR(ctx->dvdd_gpio));
		return PTR_ERR(ctx->dvdd_gpio);
	}
	gpiod_set_value(ctx->dvdd_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->dvdd_gpio);
	udelay(2000);

	lcm_panel_vddi_disable(ctx->dev); //1.8V,VCN18IO
	udelay(2000);

	return 0;
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

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
	pr_info("%s +", __func__);

	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(10);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(105);

	ctx->error = 0;
	ctx->prepared = false;
	ctx->crc_level = 0;
	ctx->doze_suspend = false;
	ctx->lhbm_en = false;
	atomic_set(&doze_enable, 0);
	pr_info("%s -", __func__);
	return 0;
}

static void get_build_id(void) {
	pr_info("%s: buildid_cmdline:%s  +\n", __func__, buildid_cmdline);
	sscanf(buildid_cmdline, "%02hhx\n", &panel_build_id);
	pr_info("%s: panel_build_id:0x%02hhx  +\n", __func__, panel_build_id);
	return ;
}

static void lcm_panel_init(struct lcm *ctx)
{
	struct mi_dsi_panel_cfg *mi_cfg = NULL;
	struct mipi_dsi_device *dsi = NULL;
	struct mtk_dsi *mtk_dsi = NULL;

	pr_info("%s: +\n", __func__);
	if (ctx->prepared) {
		pr_info("%s: panel has been prepared, nothing to do!\n", __func__);
		goto err;
	}

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		goto err;
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(11 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(2 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(2 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(11 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	if (panel_build_id >= PANEL_BUILD_ID_P11) {
		push_table(ctx, init_setting_p11,
			sizeof(init_setting_p11) / sizeof(struct LCM_setting_table));
	} else {
		push_table(ctx, init_setting,
			sizeof(init_setting) / sizeof(struct LCM_setting_table));
	}

	/*if (!ctx->gir_status) {
		pr_info("%s: gir state reload\n", __func__);
		push_table(ctx, gir_off_settings,
				sizeof(gir_off_settings) / sizeof(struct LCM_setting_table));
	}*/

	if (panel_build_id >= PANEL_BUILD_ID_P11) {
		if (ctx->dynamic_fps == 120) {
			push_table(ctx, mode_120hz_setting_p11,
				sizeof(mode_120hz_setting_p11) / sizeof(struct LCM_setting_table));
		} else if (ctx->dynamic_fps == 60) {
			push_table(ctx, mode_60hz_setting_p11,
				sizeof(mode_60hz_setting_p11) / sizeof(struct LCM_setting_table));
		} else if (ctx->dynamic_fps == 90) {
			push_table(ctx, mode_90hz_setting_p11,
				sizeof(mode_90hz_setting_p11) / sizeof(struct LCM_setting_table));
		} else if (ctx->dynamic_fps == 144) {
			push_table(ctx, mode_144hz_setting_p11,
				sizeof(mode_144hz_setting_p11) / sizeof(struct LCM_setting_table));
		}
	} else {
		if (ctx->dynamic_fps == 120) {
			push_table(ctx, mode_120hz_setting,
				sizeof(mode_120hz_setting) / sizeof(struct LCM_setting_table));
		} else if (ctx->dynamic_fps == 60) {
			push_table(ctx, mode_60hz_setting,
				sizeof(mode_60hz_setting) / sizeof(struct LCM_setting_table));
		} else if (ctx->dynamic_fps == 90) {
			push_table(ctx, mode_90hz_setting,
				sizeof(mode_90hz_setting) / sizeof(struct LCM_setting_table));
		} else if (ctx->dynamic_fps == 144) {
			push_table(ctx, mode_144hz_setting,
				sizeof(mode_144hz_setting) / sizeof(struct LCM_setting_table));
		}		
	}

	dsi = to_mipi_dsi_device(ctx->dev);
	if (dsi)
		mtk_dsi = mipi_dsi_host_to_dsi(dsi->host);
	if (mtk_dsi)
		mi_cfg = &mtk_dsi->mi_cfg;
	if (mi_cfg) {
		gray_3d_lut[0].para_list[1] = mi_cfg->feature_val[DISP_FEATURE_DBI];
		push_table(ctx, gray_3d_lut,
				sizeof(gray_3d_lut) / sizeof(struct LCM_setting_table));
		pr_info("%s, DISP_FEATURE_DBI:%d\n", __func__, mi_cfg->feature_val[DISP_FEATURE_DBI]);
	}

	ctx->prepared = true;
	ctx->doze_suspend = false;
	ctx->peak_hdr_status = 0;

err:
	pr_info("%s: -\n", __func__);
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

#if defined(CONFIG_VIS_DISPLAY_DALI)
	struct mtk_panel_ext *ext = find_panel_ext(panel);
#endif

	pr_info("%s\n", __func__);
	if (ctx->prepared)
		return 0;

	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;
	ctx->peak_hdr_status = 0;
	ctx->gir_status = 1;

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

#if defined(CONFIG_VIS_DISPLAY_DALI)
	//Novatek ASIC Notify Data Rate
	if (is_mi_dev_support_nova()) {
		vis_dsi_rate_notify((ext->params)->data_rate * 1000000);
	}
#endif

	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}

static int lcm_panel_poweron(struct drm_panel *panel)
{
	struct lcm *ctx;

	if (panel == NULL)
		return 0;
	ctx = panel_to_lcm(panel);

	if (ctx->prepared)
		return 0;

	pr_info("%s\n",__func__);

	lcm_panel_vddi_enable(ctx->dev); //1.8V,VCN18IO
	udelay(2 * 1000);

	ctx->dvdd_gpio = devm_gpiod_get_index(ctx->dev,
		"dvdd", 0, GPIOD_OUT_HIGH); //1.2V,OLED_1P2
	if (IS_ERR(ctx->dvdd_gpio)) {
		dev_err(ctx->dev, "%s: cannot get dvdd gpio %ld\n",
			__func__, PTR_ERR(ctx->dvdd_gpio));
		return PTR_ERR(ctx->dvdd_gpio);
	}
	gpiod_set_value(ctx->dvdd_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->dvdd_gpio);
	udelay(2 * 1000);

	lcm_panel_vci_enable(ctx->dev); //3.0,VIBR30
	udelay(10 * 1000);

	return 0;
}

static const struct drm_display_mode default_mode = {
	.clock = 289267,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE0_HFP,
	.hsync_end = FRAME_WIDTH + MODE0_HFP + MODE0_HSA,
	.htotal = FRAME_WIDTH + MODE0_HFP + MODE0_HSA + MODE0_HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE0_VFP,
	.vsync_end = FRAME_HEIGHT + MODE0_VFP + MODE0_VSA,
	.vtotal = FRAME_HEIGHT + MODE0_VFP + MODE0_VSA + MODE0_VBP,
};

static const struct drm_display_mode middle_mode = {
	.clock = 370378,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE2_HFP,
	.hsync_end = FRAME_WIDTH + MODE2_HFP + MODE2_HSA,
	.htotal = FRAME_WIDTH + MODE2_HFP + MODE2_HSA + MODE2_HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE2_VFP,
	.vsync_end = FRAME_HEIGHT + MODE2_VFP + MODE2_VSA,
	.vtotal = FRAME_HEIGHT + MODE2_VFP + MODE2_VSA + MODE2_VBP,
};

static const struct drm_display_mode performence_mode = {
	.clock = 493838,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE1_HFP,
	.hsync_end = FRAME_WIDTH + MODE1_HFP + MODE1_HSA,
	.htotal = FRAME_WIDTH + MODE1_HFP + MODE1_HSA + MODE1_HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE1_VFP,
	.vsync_end = FRAME_HEIGHT + MODE1_VFP + MODE1_VSA,
	.vtotal = FRAME_HEIGHT + MODE1_VFP + MODE1_VSA + MODE1_VBP,
};

static const struct drm_display_mode performence_mode_144 = {
	.clock = 592606,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE3_HFP,
	.hsync_end = FRAME_WIDTH + MODE3_HFP + MODE3_HSA,
	.htotal = FRAME_WIDTH + MODE3_HFP + MODE3_HSA + MODE3_HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE3_VFP,
	.vsync_end = FRAME_HEIGHT + MODE3_VFP + MODE3_VSA,
	.vtotal = FRAME_HEIGHT + MODE3_VFP + MODE3_VSA + MODE3_VBP,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.pll_clk = DATA_RATE0 / 2,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},
	.is_support_dbi = false,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 72730,
	.physical_height_um = 157505,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = DSC_ENABLE,
		.ver = DSC_VER,
		.slice_mode = DSC_SLICE_MODE,
		.rgb_swap = DSC_RGB_SWAP,
		.dsc_cfg = DSC_DSC_CFG,
		.rct_on = DSC_RCT_ON,
		.bit_per_channel = DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth = DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable = DSC_BP_ENABLE,
		.bit_per_pixel = DSC_BIT_PER_PIXEL,
		.pic_height = FRAME_HEIGHT,
		.pic_width = FRAME_WIDTH,
		.slice_height = DSC_SLICE_HEIGHT,
		.slice_width = DSC_SLICE_WIDTH,
		.chunk_size = DSC_CHUNK_SIZE,
		.xmit_delay = DSC_XMIT_DELAY,
		.dec_delay = DSC_DEC_DELAY,
		.scale_value = DSC_SCALE_VALUE,
		.increment_interval = DSC_INCREMENT_INTERVAL,
		.decrement_interval = DSC_DECREMENT_INTERVAL,
		.line_bpg_offset = DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset = DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset = DSC_SLICE_BPG_OFFSET,
		.initial_offset = DSC_INITIAL_OFFSET,
		.final_offset = DSC_FINAL_OFFSET,
		.flatness_minqp = DSC_FLATNESS_MINQP,
		.flatness_maxqp = DSC_FLATNESS_MAXQP,
		.rc_model_size = DSC_RC_MODEL_SIZE,
		.rc_edge_factor = DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0 = DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1 = DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi = DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo = DSC_RC_TGT_OFFSET_LO,
		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = o12_42_dphy_rc_buf_thresh,
			.range_min_qp = o12_42_dphy_range_min_qp,
			.range_max_qp = o12_42_dphy_range_max_qp,
			.range_bpg_ofs = o12_42_dphy_range_bpg_ofs,
			},
		},
	.data_rate = DATA_RATE1,
	.lp_perline_en = 0,
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_r,
#endif
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.real_te_duration = 16666,
	//.SilkyBrightnessDelay = 3000,
};

static struct mtk_panel_params ext_params_90hz = {
	.pll_clk = DATA_RATE2 / 2,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},
	.is_support_dbi = false,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 72730,
	.physical_height_um = 157505,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = DSC_ENABLE,
		.ver = DSC_VER,
		.slice_mode = DSC_SLICE_MODE,
		.rgb_swap = DSC_RGB_SWAP,
		.dsc_cfg = DSC_DSC_CFG,
		.rct_on = DSC_RCT_ON,
		.bit_per_channel = DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth = DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable = DSC_BP_ENABLE,
		.bit_per_pixel = DSC_BIT_PER_PIXEL,
		.pic_height = FRAME_HEIGHT,
		.pic_width = FRAME_WIDTH,
		.slice_height = DSC_SLICE_HEIGHT,
		.slice_width = DSC_SLICE_WIDTH,
		.chunk_size = DSC_CHUNK_SIZE,
		.xmit_delay = DSC_XMIT_DELAY,
		.dec_delay = DSC_DEC_DELAY,
		.scale_value = DSC_SCALE_VALUE,
		.increment_interval = DSC_INCREMENT_INTERVAL,
		.decrement_interval = DSC_DECREMENT_INTERVAL,
		.line_bpg_offset = DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset = DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset = DSC_SLICE_BPG_OFFSET,
		.initial_offset = DSC_INITIAL_OFFSET,
		.final_offset = DSC_FINAL_OFFSET,
		.flatness_minqp = DSC_FLATNESS_MINQP,
		.flatness_maxqp = DSC_FLATNESS_MAXQP,
		.rc_model_size = DSC_RC_MODEL_SIZE,
		.rc_edge_factor = DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0 = DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1 = DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi = DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo = DSC_RC_TGT_OFFSET_LO,
		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = o12_42_dphy_rc_buf_thresh,
			.range_min_qp = o12_42_dphy_range_min_qp,
			.range_max_qp = o12_42_dphy_range_max_qp,
			.range_bpg_ofs = o12_42_dphy_range_bpg_ofs,
			},
		},
	.data_rate = DATA_RATE2,
	.lp_perline_en = 0,
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_r,

#endif
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.real_te_duration = 11111,
};

static struct mtk_panel_params ext_params_120hz = {
	.pll_clk = DATA_RATE1 / 2,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},
	.is_support_dbi = false,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 72730,
	.physical_height_um = 157505,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = DSC_ENABLE,
		.ver = DSC_VER,
		.slice_mode = DSC_SLICE_MODE,
		.rgb_swap = DSC_RGB_SWAP,
		.dsc_cfg = DSC_DSC_CFG,
		.rct_on = DSC_RCT_ON,
		.bit_per_channel = DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth = DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable = DSC_BP_ENABLE,
		.bit_per_pixel = DSC_BIT_PER_PIXEL,
		.pic_height = FRAME_HEIGHT,
		.pic_width = FRAME_WIDTH,
		.slice_height = DSC_SLICE_HEIGHT,
		.slice_width = DSC_SLICE_WIDTH,
		.chunk_size = DSC_CHUNK_SIZE,
		.xmit_delay = DSC_XMIT_DELAY,
		.dec_delay = DSC_DEC_DELAY,
		.scale_value = DSC_SCALE_VALUE,
		.increment_interval = DSC_INCREMENT_INTERVAL,
		.decrement_interval = DSC_DECREMENT_INTERVAL,
		.line_bpg_offset = DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset = DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset = DSC_SLICE_BPG_OFFSET,
		.initial_offset = DSC_INITIAL_OFFSET,
		.final_offset = DSC_FINAL_OFFSET,
		.flatness_minqp = DSC_FLATNESS_MINQP,
		.flatness_maxqp = DSC_FLATNESS_MAXQP,
		.rc_model_size = DSC_RC_MODEL_SIZE,
		.rc_edge_factor = DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0 = DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1 = DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi = DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo = DSC_RC_TGT_OFFSET_LO,
		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = o12_42_dphy_rc_buf_thresh,
			.range_min_qp = o12_42_dphy_range_min_qp,
			.range_max_qp = o12_42_dphy_range_max_qp,
			.range_bpg_ofs = o12_42_dphy_range_bpg_ofs,
			},
		},
	.data_rate = DATA_RATE1,
	.lp_perline_en = 0,
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_r,
#endif
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.real_te_duration = 8333,
};

static struct mtk_panel_params ext_params_144hz = {
	.pll_clk = DATA_RATE3 / 2,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},
	.is_support_dbi = false,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 72730,
	.physical_height_um = 157505,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = DSC_ENABLE,
		.ver = DSC_VER,
		.slice_mode = DSC_SLICE_MODE,
		.rgb_swap = DSC_RGB_SWAP,
		.dsc_cfg = DSC_DSC_CFG,
		.rct_on = DSC_RCT_ON,
		.bit_per_channel = DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth = DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable = DSC_BP_ENABLE,
		.bit_per_pixel = DSC_BIT_PER_PIXEL,
		.pic_height = FRAME_HEIGHT,
		.pic_width = FRAME_WIDTH,
		.slice_height = DSC_SLICE_HEIGHT,
		.slice_width = DSC_SLICE_WIDTH,
		.chunk_size = DSC_CHUNK_SIZE,
		.xmit_delay = DSC_XMIT_DELAY,
		.dec_delay = DSC_DEC_DELAY,
		.scale_value = DSC_SCALE_VALUE,
		.increment_interval = DSC_INCREMENT_INTERVAL,
		.decrement_interval = DSC_DECREMENT_INTERVAL,
		.line_bpg_offset = DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset = DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset = DSC_SLICE_BPG_OFFSET,
		.initial_offset = DSC_INITIAL_OFFSET,
		.final_offset = DSC_FINAL_OFFSET,
		.flatness_minqp = DSC_FLATNESS_MINQP,
		.flatness_maxqp = DSC_FLATNESS_MAXQP,
		.rc_model_size = DSC_RC_MODEL_SIZE,
		.rc_edge_factor = DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0 = DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1 = DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi = DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo = DSC_RC_TGT_OFFSET_LO,
		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = o12_42_dphy_rc_buf_thresh,
			.range_min_qp = o12_42_dphy_range_min_qp,
			.range_max_qp = o12_42_dphy_range_max_qp,
			.range_bpg_ofs = o12_42_dphy_range_bpg_ofs,
			},
		},
	.data_rate = DATA_RATE3,
	.lp_perline_en = 0,
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_r,
#endif
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.real_te_duration = 6944,
};

struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
	unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;

	list_for_each_entry(m, &connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	return NULL;
}

static int mtk_panel_ext_param_get(struct drm_panel *panel,
	struct drm_connector *connector,
	struct mtk_panel_params **ext_param,
	unsigned int mode)
{
	int ret = 0;
	int dst_fps = 0;
	struct drm_display_mode *m_dst = get_mode_by_id(connector, mode);

	dst_fps = m_dst ? drm_mode_vrefresh(m_dst) : -EINVAL;

	if (dst_fps == MODE0_FPS)
		*ext_param = &ext_params;
	else if (dst_fps == MODE2_FPS)
		*ext_param = &ext_params_90hz;
	else if (dst_fps == MODE1_FPS)
		*ext_param = &ext_params_120hz;
	else if (dst_fps == MODE3_FPS)
		*ext_param = &ext_params_144hz;
	else {
		pr_err("%s, dst_fps %d\n", __func__, dst_fps);
		ret = -EINVAL;
	}

	if (!ret)
		current_fps = dst_fps;

	return ret;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	int dst_fps = 0;
	struct drm_display_mode *m_dst = get_mode_by_id(connector, mode);

	dst_fps = m_dst ? drm_mode_vrefresh(m_dst) : -EINVAL;

	if (dst_fps == MODE0_FPS)
		ext->params = &ext_params;
	else if (dst_fps == MODE2_FPS)
		ext->params = &ext_params_90hz;
	else if (dst_fps == MODE1_FPS)
		ext->params = &ext_params_120hz;
	else if (dst_fps == MODE3_FPS)
		ext->params = &ext_params_144hz;
	else {
		pr_err("%s, dst_fps %d\n", __func__, dst_fps);
		ret = -EINVAL;
	}

	if (!ret)
		current_fps = dst_fps;

	return ret;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

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


static int panel_set_peak_hdr_status(struct mtk_dsi *dsi,dcs_write_gce cb,
	void *handle, unsigned int bl_level)
{
#ifdef PEAK_WITH_GIR_OFF
	char gir_off_set[] = {0x5F,0x01};
	char gir_on_set[] = {0x5F,0x00};
#endif
	struct lcm *ctx = NULL;
	struct mi_dsi_panel_cfg *mi_cfg = NULL;

	if (!dsi || !dsi->panel) {
		pr_err("%s: panel is NULL\n", __func__);
		goto err;
	}

#ifdef CONFIG_FACTORY_BUILD
	pr_info("%s factory don't need to support.\n", __func__);
	return 0;
#endif

	ctx = panel_to_lcm(dsi->panel);
	mi_cfg = &dsi->mi_cfg;

	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
		goto err;
	}

	if (bl_level != 0 && bl_level <= PEAK_HDR_BL_LEVEL && ctx->peak_hdr_status) {
#ifdef PEAK_WITH_GIR_OFF
		cb(dsi, handle, gir_on_set, ARRAY_SIZE(gir_on_set));
		ctx->gir_status = 1;
#endif
		mi_cfg->feature_val[DISP_FEATURE_FLAT_MODE]= 1;
		ctx->peak_hdr_status = 0;
	}

	if (bl_level > PEAK_HDR_BL_LEVEL && bl_level <= PEAK_MAX_BL_LEVEL && !ctx->peak_hdr_status)
	{
#ifdef PEAK_WITH_GIR_OFF
		cb(dsi, handle, gir_off_set, ARRAY_SIZE(gir_off_set));
		ctx->gir_status = 0;
#endif
		mi_cfg->feature_val[DISP_FEATURE_FLAT_MODE]= 0;
		ctx->peak_hdr_status = 1;
	}

	pr_info("%s: peak_hdr_status = %d ctx->gir_status = %d\n", __func__, ctx->peak_hdr_status, ctx->gir_status);
err:
	return 1;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	struct lcm *ctx = NULL;
	char bl_tb[] = {0x51, 0x07, 0xff};
	char backlight_off_tb[] = {0xA9,0x02,0x01,0xBA,0x09,0x09,0x5F,0x02,0x01,0xBA,0x0F,0x10,0xF4,0xF4,0x02,0x01,0xBA,0x27,0x27,0x5F,0x02,0x01,0xBA,0x2D,0x2E,0xF4,0xF4,0x01,0x00,0x51,0x00,0x01,0x00,0x00};
	struct mtk_dsi *mtk_dsi = (struct mtk_dsi *)dsi;
	if (!mtk_dsi || !mtk_dsi->panel) {
		pr_err("dsi is null\n");
		return -1;
	}
	ctx = panel_to_lcm(mtk_dsi->panel);

	if (level) {
		bl_tb0[1] = (level >> 8) & 0xFF;
		bl_tb0[2] = level & 0xFF;
		mtk_dsi->mi_cfg.last_no_zero_bl_level = level;
	}

	bl_tb[1] = (level >> 8) & 0xFF;
	bl_tb[2] = level & 0xFF;
	if (!cb)
		return -1;
	if (atomic_read(&doze_enable)) {
		//mtk_dsi->mi_cfg.aod_unset_backlight = true;
		pr_info("%s: Return it when aod on, %d %d %d\n",
			__func__, level, bl_tb[1], bl_tb[2]);
		return 0;
	}

	if (level != 0){
		panel_set_peak_hdr_status(mtk_dsi,cb,handle,level);
	}

	pr_info("%s %d %d %d, last:%d\n", __func__, level, bl_tb[1], bl_tb[2], mtk_dsi->mi_cfg.last_bl_level);

	if (level == 0 && mtk_dsi->mi_cfg.last_bl_level > SLEEP_IN_VIANO_KEEP_HBM_THRESHOLD) {
		cb(dsi, handle, backlight_off_tb, ARRAY_SIZE(backlight_off_tb));
	} else {
		cb(dsi, handle, bl_tb, ARRAY_SIZE(bl_tb));
	}

#if defined(CONFIG_VIS_DISPLAY_DALI)
	if (is_mi_dev_support_nova()) {
		if (level == 0) {
			vis_dsi_cmd_send(EN_DSI_CMD_BEFORE_SET_BACKLIGHT_OFF, 0 , NULL);
		} else {
			vis_dsi_cmd_send(EN_DSI_CMD_BEFORE_SET_BACKLIGHT_ON, 0 , NULL);
		}
	}
#endif

	mtk_dsi->mi_cfg.last_bl_level = level;
	bl_value = level;

	return 0;
}

/*
static int lcm_set_bl_elvss_cmdq(void *dsi, dcs_grp_write_gce cb, void *handle,
		struct mtk_bl_ext_config *bl_ext_config)
{
	unsigned int PVEE_Reg, VREF_Anode_reg;

	static struct mtk_panel_para_table elvss_tb[] = {
		{6, {0xF0, 0x55,0xAA,0x52,0x08,0x00}},
		{2, {0x6F, 0x2D}},
		{24, {0xB5, 0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x84,0x84,0x00,0x00,0x77,0x77,0x5E,0x5E,0x59}},
		{2, {0x6F, 0x44}},
		{24, {0xB5, 0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x86,0x84,0x84,0x00,0x00,0x77,0x77,0x5E,0x5E,0x59}},
		{6, {0xF0, 0x55,0xAA,0x52,0x08,0x01}},
		{2, {0x6F, 0x0B}},
		{24, {0xBB, 0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x00,0x00,0x08,0x08,0x00,0x00,0x3C,0x3C,0xC4,0xC4,0xD8}},
		{2, {0x6F, 0x29}},
		{24, {0xBB, 0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x00,0x00,0x08,0x08,0x00,0x00,0x3C,0x3C,0xC4,0xC4,0xD8}},
	};

	const unsigned int ELVSSBase = 0x7B;
	const unsigned int AnodeOffset = 0x2C;

	if (!cb)
		return -1;

	PVEE_Reg = ELVSSBase + bl_ext_config->elvss_pn;
	VREF_Anode_reg = AnodeOffset - bl_ext_config->elvss_pn * 4;

	if (bl_ext_config->cfg_flag & (0x1<<SET_ELVSS_PN)) {
		pr_info("%s PVEE_Reg = %#x, VREF_Anode_reg = %#x\n", __func__, PVEE_Reg, VREF_Anode_reg);
		elvss_tb[2].para_list[17] = PVEE_Reg;
		elvss_tb[2].para_list[18] = PVEE_Reg;
		elvss_tb[4].para_list[17] = PVEE_Reg;
		elvss_tb[4].para_list[18] = PVEE_Reg;
		elvss_tb[7].para_list[17] = VREF_Anode_reg & 0xFF;
		elvss_tb[7].para_list[18] = VREF_Anode_reg & 0xFF;
		elvss_tb[9].para_list[17] = VREF_Anode_reg & 0xFF;
		elvss_tb[9].para_list[18] = VREF_Anode_reg & 0xFF;
		cb(dsi, handle, elvss_tb, ARRAY_SIZE(elvss_tb));
	}
	return 0;
}

static int lcm_read_elvss_base_voltage(void *dsi, ddic_dsi_send_cmd send_cb,
		dic_dsi_read_cmd read_cb, struct DISP_PANEL_BASE_VOLTAGE *base_voltage)
{
	unsigned char AnodeOffset[] = "2C";
	unsigned char ELVSSBase[] = "7B";
	base_voltage->DDICDbvPn = DDIC_DBV_PN;
	base_voltage->DDICFpsPn = DDIC_FPS_PN;
	memcpy(base_voltage->AnodeOffset, AnodeOffset, sizeof(AnodeOffset));
	memcpy(base_voltage->ELVSSBase, ELVSSBase, sizeof(ELVSSBase));
	base_voltage->AnodeBase[0] = '0';
	base_voltage->flag = true;
	pr_info("%s DDICDbvPn = %u, DDICFpsPn = %u, AnodeOffset = %s, ELVSSBase = %s, AnodeBase = %s, flag = %d",
		__func__, base_voltage->DDICDbvPn, base_voltage->DDICFpsPn, base_voltage->AnodeOffset, base_voltage->ELVSSBase, base_voltage->AnodeBase, base_voltage->flag);

	return 0;
}
*/

static void mode_switch_to_144(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s state = %d\n", __func__, stage);

	if (stage == BEFORE_DSI_POWERDOWN) {
#if defined(CONFIG_VIS_DISPLAY_DALI)
		if(is_mi_dev_support_nova()) {
			if(vis_display_get_CurrentUsecaseID() != 0) {
				vis_dsi_fps_switching_cmd(144, 0);
			}
		}
#endif
		if (panel_build_id >= PANEL_BUILD_ID_P11) {
			push_table(ctx, mode_144hz_setting_p11,
				sizeof(mode_144hz_setting_p11) / sizeof(struct LCM_setting_table));
		} else {
			push_table(ctx, mode_144hz_setting,
				sizeof(mode_144hz_setting) / sizeof(struct LCM_setting_table));
		}

		ctx->dynamic_fps = 144;
#if defined(CONFIG_VIS_DISPLAY_DALI)
		if(is_mi_dev_support_nova()) {
			if(vis_display_get_CurrentUsecaseID() != 0) {
				vis_dsi_fps_switching_cmd(144, 1);
			}
		}
#endif
	}
}

static void mode_switch_to_120(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s state = %d\n", __func__, stage);
	if (stage == BEFORE_DSI_POWERDOWN) {
#if defined(CONFIG_VIS_DISPLAY_DALI)
		if(is_mi_dev_support_nova()) {
			if(vis_display_get_CurrentUsecaseID() != 0) {
				vis_dsi_fps_switching_cmd(120, 0);
			}
		}
#endif
		if (panel_build_id >= PANEL_BUILD_ID_P11) {
			push_table(ctx, mode_120hz_setting_p11,
				sizeof(mode_120hz_setting_p11) / sizeof(struct LCM_setting_table));
		} else {
			push_table(ctx, mode_120hz_setting,
				sizeof(mode_120hz_setting) / sizeof(struct LCM_setting_table));
		}

		ctx->dynamic_fps = 120;
#if defined(CONFIG_VIS_DISPLAY_DALI)
		if(is_mi_dev_support_nova()) {
			if(vis_display_get_CurrentUsecaseID() != 0) {
				vis_dsi_fps_switching_cmd(120, 1);
			}
		}
#endif
	}
}

static void mode_switch_to_90(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s state = %d\n", __func__, stage);
	if (stage == BEFORE_DSI_POWERDOWN) {
#if defined(CONFIG_VIS_DISPLAY_DALI)
		if(is_mi_dev_support_nova()) {
			if(vis_display_get_CurrentUsecaseID() != 0) {
				vis_dsi_fps_switching_cmd(90, 0);
			}
		}
#endif
		if (panel_build_id >= PANEL_BUILD_ID_P11) {
			push_table(ctx, mode_90hz_setting_p11,
			sizeof(mode_90hz_setting_p11) / sizeof(struct LCM_setting_table));			
		} else {
			push_table(ctx, mode_90hz_setting,
			sizeof(mode_90hz_setting) / sizeof(struct LCM_setting_table));
		}

		ctx->dynamic_fps = 90;
#if defined(CONFIG_VIS_DISPLAY_DALI)
		if(is_mi_dev_support_nova()) {
			if(vis_display_get_CurrentUsecaseID() != 0) {
				vis_dsi_fps_switching_cmd(90, 1);
			}
		}
#endif
	}
}

static void mode_switch_to_60(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s state = %d\n", __func__, stage);
	if (stage == BEFORE_DSI_POWERDOWN) {
#if defined(CONFIG_VIS_DISPLAY_DALI)
		if(is_mi_dev_support_nova()) {
			if(vis_display_get_CurrentUsecaseID() != 0) {
				vis_dsi_fps_switching_cmd(60, 0);
			}
		}
#endif
		if (panel_build_id >= PANEL_BUILD_ID_P11) {
			push_table(ctx, mode_60hz_setting_p11,
			sizeof(mode_60hz_setting_p11) / sizeof(struct LCM_setting_table));
		} else {
			push_table(ctx, mode_60hz_setting,
			sizeof(mode_60hz_setting) / sizeof(struct LCM_setting_table));
		}

		ctx->dynamic_fps = 60;
#if defined(CONFIG_VIS_DISPLAY_DALI)
		if(is_mi_dev_support_nova()) {
			if(vis_display_get_CurrentUsecaseID() != 0) {
				vis_dsi_fps_switching_cmd(60, 1);
			}
		}
#endif
	}
}

static int mode_switch(struct drm_panel *panel,
		struct drm_connector *connector, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	int dst_fps = 0, cur_fps = 0;
	int dst_vdisplay = 0, dst_hdisplay = 0;
	int cur_vdisplay = 0, cur_hdisplay = 0;
	bool isFpsChange = false;
	struct drm_display_mode *m_dst = get_mode_by_id(connector, dst_mode);
	struct drm_display_mode *m_cur = get_mode_by_id(connector, cur_mode);

	if (cur_mode == dst_mode)
		return ret;

	dst_fps = m_dst ? drm_mode_vrefresh(m_dst) : -EINVAL;
	dst_vdisplay = m_dst ? m_dst->vdisplay : -EINVAL;
	dst_hdisplay = m_dst ? m_dst->hdisplay : -EINVAL;
	cur_fps = m_cur ? drm_mode_vrefresh(m_cur) : -EINVAL;
	cur_vdisplay = m_cur ? m_cur->vdisplay : -EINVAL;
	cur_hdisplay = m_cur ? m_cur->hdisplay : -EINVAL;

	isFpsChange = ((dst_fps == cur_fps) && (dst_fps != -EINVAL)
			&& (cur_fps != -EINVAL)) ? false : true;

	pr_info("%s isFpsChange = %d\n", __func__, isFpsChange);
	pr_info("%s dst_mode vrefresh = %d, vdisplay = %d, vdisplay = %d\n",
		__func__, dst_fps, dst_vdisplay, dst_hdisplay);
	pr_info("%s cur_mode vrefresh = %d, vdisplay = %d, vdisplay = %d\n",
		__func__, cur_fps, cur_vdisplay, cur_hdisplay);
	if (isFpsChange) {
		if (dst_fps == MODE0_FPS)
			mode_switch_to_60(panel, stage);
		else if (dst_fps == MODE2_FPS)
			mode_switch_to_90(panel, stage);
		else if (dst_fps == MODE1_FPS)
			mode_switch_to_120(panel, stage);
		else if (dst_fps == MODE3_FPS)
			mode_switch_to_144(panel, stage);
		else
			ret = 1;

#if defined(CONFIG_VIS_DISPLAY_DALI)
		//Novatek ASIC Notify fps
		if (is_mi_dev_support_nova()) {
			if (dst_fps == MODE0_FPS)
				vis_fps_notify(60);
			else if (dst_fps == MODE2_FPS)
				vis_fps_notify(90);
			else if (dst_fps == MODE1_FPS)
				vis_fps_notify(120);
			else if (dst_fps == MODE3_FPS)
				vis_fps_notify(144);
		}
#endif
	}

	return ret;
}

static int panel_doze_enable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	struct mtk_dsi *mtk_dsi = NULL;

	mtk_dsi = (struct mtk_dsi *)dsi;
	atomic_set(&doze_enable, 1);
	//mtk_dsi->mi_cfg.aod_unset_backlight = false;
	pr_info("%s !-\n", __func__);
	return 0;
}

static int panel_doze_disable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	struct lcm *ctx = NULL;
	struct mtk_dsi *mtk_dsi = NULL;
	char aod2normal_tb[] = {0x38, 0x00};
	char mode_60hz_setting[] = {0x2F, 0x02};
	char mode_90hz_setting[] = {0x2F, 0x01};
	char mode_120hz_setting[] = {0x2F, 0x00};
	char mode_144hz_setting[] = {0x2F, 0x03};

#ifdef CONFIG_FACTORY_BUILD
	pr_info("%s factory\n", __func__);
	return 0;
#endif

	if (!dsi) {
		pr_err("%s dsi is null\n", __func__);
		return -1;
	}

	if (!panel) {
		pr_err("%s invalid panel\n", __func__);
		return -1;
	}

	pr_info("%s +\n", __func__);
	ctx = panel_to_lcm(panel);
	mtk_dsi = (struct mtk_dsi *)dsi;

	cb(dsi, handle, aod2normal_tb, ARRAY_SIZE(aod2normal_tb));

	if (ctx->dynamic_fps == 120) {
		cb(dsi, handle, mode_120hz_setting, ARRAY_SIZE(mode_120hz_setting));
	} else if (ctx->dynamic_fps == 60) {
		cb(dsi, handle, mode_60hz_setting, ARRAY_SIZE(mode_60hz_setting));
	} else if (ctx->dynamic_fps == 90) {
		cb(dsi, handle, mode_90hz_setting, ARRAY_SIZE(mode_90hz_setting));
	} else if (ctx->dynamic_fps == 144) {
		cb(dsi, handle, mode_144hz_setting, ARRAY_SIZE(mode_144hz_setting));
	}

	ctx->doze_suspend = false;
	atomic_set(&doze_enable, 0);
	pr_info("%s -\n", __func__);

	return 0;
}

static int panel_doze_suspend(struct drm_panel *panel, void * dsi, dcs_write_gce cb, void *handle) 
{
	struct lcm *ctx = NULL;
	char enter_120hz[] = {0x2F, 0x00};
	//char gir_on[] = {0x5F, 0x00};
	char normal2aod_tb[] ={0x39, 0x00};
	char brightnessh_tb[] = {0x51, 0x00, 0x3B, 0x00, 0x3B, 0x05, 0x54};

	if (!dsi) {
		pr_err("%s dsi is null\n", __func__);
		return -1;
	}

	if (!panel) {
		pr_err("%s invalid panel\n", __func__);
		return -1;
	}

#ifdef CONFIG_FACTORY_BUILD
	pr_info("%s factory\n", __func__);
	return 0;
#endif

	ctx = panel_to_lcm(panel);

	if (!ctx) {
		pr_err("ctx is null\n");
		return -1;
	}

	if (ctx->doze_suspend) {
		pr_info("%s already suspend, skip\n", __func__);
		goto exit;
	}

	if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_HBM) {
		brightnessh_tb[1] = 0x03;
		brightnessh_tb[2] = 0x24;
		brightnessh_tb[3] = 0x03;
		brightnessh_tb[4] = 0x24;
		brightnessh_tb[5] = 0x3F;
		brightnessh_tb[6] = 0xFF;
	}

	cb(dsi, handle, enter_120hz, ARRAY_SIZE(enter_120hz));
	//cb(dsi, handle, gir_on, ARRAY_SIZE(gir_on));
	cb(dsi, handle, normal2aod_tb, ARRAY_SIZE(normal2aod_tb));
	cb(dsi, handle, brightnessh_tb, ARRAY_SIZE(brightnessh_tb));
	ctx->doze_suspend = true;
	//usleep_range(5 * 1000, 5* 1000 + 10);
	pr_info("lhbm enter aod in doze_suspend\n");

exit:
	pr_info("%s !-\n", __func__);
	return 0;
}

/*
static int panel_set_dimming_on(struct mtk_dsi *dsi,  int level)
{
	unsigned int format = FORMAT_NOLOCK;
	int ret = 0;
	struct lcm *ctx = NULL;
	struct mi_dsi_panel_cfg *mi_cfg = NULL;
	if(level)
		return ret;

	if (!dsi || !dsi->panel) {
		pr_err("%s: panel is NULL\n", __func__);
		return -1;
	}

	ctx = panel_to_lcm(dsi->panel);
	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
		return -1;
	}

	mi_cfg = &dsi->mi_cfg;

	if (mi_cfg->last_bl_level >= DIMMING_ON_THRESHOLD_BEFORE_BL_OFF
	    && dsi->output_en ) {
		ret = mi_disp_panel_ddic_send_cmd(dimming_on, ARRAY_SIZE(dimming_on), format);
	}
	pr_info("%s end -\n", __func__);
	return ret;
}
*/

static int panel_get_doze_brightness(struct drm_panel *panel, u32 *doze_brightness)
{
	int count = 0;
	struct lcm *ctx = panel_to_lcm(panel);
	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

	*doze_brightness = ctx->doze_brightness_state;
	return count;

}

static int panel_set_doze_brightness(struct drm_panel *panel, int doze_brightness)
{
	struct lcm *ctx;
	int ret = 0;
	unsigned int format = 0;
#ifndef CONFIG_FACTORY_BUILD
	struct LCM_setting_table backlight_h[] = {
			{0x51, 06, {0x03, 0x24, 0x03, 0x24, 0x3F, 0xFF}},
	};
	struct LCM_setting_table backlight_l[] = {
			{0x51, 06, {0x00, 0x3B, 0x00, 0x3B, 0x05, 0x54}},
	};
	// struct LCM_setting_table backlight_0[] = {
	// 		{0x51, 06, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	// };
#endif

	if (!panel) {
		pr_err("invalid params\n");
		return -1;
	}

	ctx = panel_to_lcm(panel);

	if (ctx->doze_brightness_state == doze_brightness) {
		pr_info("%s skip same doze_brightness set:%d\n", __func__, doze_brightness);
		return 0;
	}

	if (!atomic_read(&doze_enable)) {
		pr_info("%s normal mode cannot set doze brightness\n", __func__);
		goto exit;
	}

#ifdef CONFIG_FACTORY_BUILD
	if (DOZE_TO_NORMAL == doze_brightness) {
		ret = mi_disp_panel_ddic_send_cmd(doze_disable_t, ARRAY_SIZE(doze_disable_t), format);
		atomic_set(&doze_enable, 0);
		ctx->doze_suspend = false;
		goto exit;
	} else if (DOZE_BRIGHTNESS_LBM  == doze_brightness) {
		ret = mi_disp_panel_ddic_send_cmd(doze_enable_l, ARRAY_SIZE(doze_enable_l), format);

	} else if (DOZE_BRIGHTNESS_HBM == doze_brightness) {
		ret = mi_disp_panel_ddic_send_cmd(doze_enable_h, ARRAY_SIZE(doze_enable_h), format);
	}

#else

	if (DOZE_TO_NORMAL == doze_brightness) {
		// ret = mi_disp_panel_ddic_send_cmd(backlight_0, ARRAY_SIZE(backlight_0), format);
		atomic_set(&doze_enable, 0);
	} else if (DOZE_BRIGHTNESS_LBM  == doze_brightness) {
		ret = mi_disp_panel_ddic_send_cmd(backlight_l, ARRAY_SIZE(backlight_l), format);

	} else if (DOZE_BRIGHTNESS_HBM == doze_brightness) {
		ret = mi_disp_panel_ddic_send_cmd(backlight_h, ARRAY_SIZE(backlight_h), format);
	}
#endif

	if (ret != 0) {
		DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
	}
exit :
	//ctx->last_doze_brightness_state = ctx->doze_brightness_state;
	ctx->doze_brightness_state = doze_brightness;
	pr_info("%s end -\n", __func__);
	return ret;
}

static bool get_panel_initialized(struct drm_panel *panel)
{
	struct lcm *ctx;
	bool ret = false;

	if (!panel) {
		pr_err("%s panel is NULL\n", __func__);
		goto err;
	}

	ctx = panel_to_lcm(panel);
	ret = ctx->prepared;
err:
	return ret;
}

#if 0
static int panel_set_gir_on(struct drm_panel *panel)
{
	struct lcm *ctx;
	int ret = 0;
	unsigned int format = 0;

	pr_info("%s: +\n", __func__);

	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		ret = -1;
		goto err;
	}

	ctx = panel_to_lcm(panel);
	ctx->gir_status = 1;
	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
	} else {
		mi_disp_panel_ddic_send_cmd(gir_on_settings, ARRAY_SIZE(gir_on_settings), format);
	}

err:
	pr_info("%s: -\n", __func__);
	return ret;
}

static int panel_set_gir_off(struct drm_panel *panel)
{
	struct lcm *ctx;
	int ret = -1;
	unsigned int format = 0;

	pr_info("%s: +\n", __func__);

	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		goto err;
	}

	ctx = panel_to_lcm(panel);
	ctx->gir_status = 0;
	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
	} else {
		mi_disp_panel_ddic_send_cmd(gir_off_settings, ARRAY_SIZE(gir_off_settings), format);
	}

err:
	pr_info("%s: -\n", __func__);
	return ret;
}
#endif

static void lcm_esd_restore_backlight(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	lcm_dcs_write(ctx, bl_tb0, ARRAY_SIZE(bl_tb0));

	pr_info("%s high_bl = 0x%x, low_bl = 0x%x \n", __func__, bl_tb0[1], bl_tb0[2]);

	return;
}

#ifdef CONFIG_MI_DISP_FP_STATE
static void lcm_fp_state_restore_backlight(struct mtk_dsi *dsi)
{
	struct lcm *ctx = NULL;
	struct mi_dsi_panel_cfg *mi_cfg = NULL;
	int bl_level = 0;
	unsigned int format = 0;
	struct LCM_setting_table restore_backlight_level[] = {
		{0x51, 02, {0x03, 0xFF}},
	};

	if (!dsi || !dsi->panel) {
		pr_err("%s: panel is NULL\n", __func__);
		return;
	}

	ctx = panel_to_lcm(dsi->panel);
	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
		return;
	}

	mi_cfg = &dsi->mi_cfg;
	bl_level = mi_cfg->last_no_zero_bl_level;
	if (atomic_read(&doze_enable)) {
		if (mi_cfg->last_bl_level)
			bl_level = mi_cfg->last_bl_level;
		else
			bl_level = 0;
	} else {
		pr_err("%s, only restore from doze\n", __func__);
		return;
	}

	restore_backlight_level[0].para_list[0] = (bl_level >> 8) & 0xFF;
	restore_backlight_level[0].para_list[1] = bl_level & 0xFF;

	mi_disp_panel_ddic_send_cmd(restore_backlight_level, ARRAY_SIZE(restore_backlight_level), format);

	pr_info("%s setbacklight %d, doze_enabled: %d last_backlight: %d %d \n", __func__, bl_level, atomic_read(&doze_enable), mi_cfg->last_bl_level, mi_cfg->last_no_zero_bl_level);
	return;
}
#endif

static int panel_get_panel_info(struct drm_panel *panel, char *buf)
{
	int count = 0;
	struct lcm *ctx;

	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

	ctx = panel_to_lcm(panel);
	count = snprintf(buf, PAGE_SIZE, "%s\n", ctx->panel_info);

	return count;
}

static int panel_get_max_brightness_clone(struct drm_panel *panel, u32 *max_brightness_clone)
{
	struct lcm *ctx;

	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

	ctx = panel_to_lcm(panel);
	*max_brightness_clone = ctx->max_brightness_clone;

	return 0;
}

static int panel_get_factory_max_brightness(struct drm_panel *panel, u32 *max_brightness_clone)
{
	struct lcm *ctx;

	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

	ctx = panel_to_lcm(panel);
	*max_brightness_clone = ctx->factory_max_brightness;

	return 0;
}

#if 0
static void mi_parse_cmdline_perBL(struct LHBM_WHITEBUF * lhbm_whitebuf) {
	int i = 0, temp = 0;
	int gamma_coffee_w250_R[3] = {980,981,1032};
	int gamma_coffee_w250_G[3] = {980,985,1030};
	int gamma_coffee_w250_B[3] = {980,982,1033};
	int gamma_coffee_w1300_other_R[2] = {1000,1031};
	int gamma_coffee_w1300_other_G[2] = {1025,1030};
	int gamma_coffee_w1300_other_B[2] = {1010,1042};
	int gamma_coffee_w1300_700_80_R[10] = {1000,996 ,995 ,993 ,990 ,990 ,990 ,988 ,985 ,985};
	int gamma_coffee_w1300_700_80_G[10] = {1018,1015,1012,1010,1006,1003,1000,998 ,998 ,996};
	int gamma_coffee_w1300_700_80_B[10] = {1005,1005,1003,1000,998 ,998 ,995 ,992 ,990 ,990};
	int gamma_coffee_w500[4] = {1000,1010,998,990};

	static u16 lhbm_cmdbuf[21] = {0};

	pr_info("mi_parse_cmdline_perBL enter\n");

	if(!lhbm_w900_update_flag && !lhbm_w110_update_flag && !lhbm_g500_update_flag) {
		pr_info("don't need update white rgb config");
		return;
	}

	if (lhbm_whitebuf == NULL) {
		pr_err("lhbm_status == NULL\n");
		return;
	}
	for (i = 0; i < 21; i++) {
		sscanf(oled_lhbm_cmdline + 4 * i, "%04hx", &lhbm_cmdbuf[i]);
	}

	for (i = 0; i < 6; i +=2) {
		if(i == 0) { // R
			//250nit
			temp = (int)lhbm_cmdbuf[i/2]*gamma_coffee_w250_R[0]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_250[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_250[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 3]*gamma_coffee_w250_R[1]* 4 / 1000;
			lhbm_whitebuf->nit_80_2_250[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_80_2_250[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 6]*gamma_coffee_w250_R[2]* 4 / 1000;
			lhbm_whitebuf->nit_1600_700_250[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_1600_700_250[i+1] = temp & 0xFF;

			//1300nit
			temp = (int)lhbm_cmdbuf[i/2 + 9]*gamma_coffee_w1300_700_80_R[0]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_1300_1000_1018_1005[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_1300_1000_1018_1005[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 9]*gamma_coffee_w1300_700_80_R[1]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_1300_996_1015_1005[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_1300_996_1015_1005[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 9]*gamma_coffee_w1300_700_80_R[2]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_1300_995_1012_1003[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_1300_995_1012_1003[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 9]*gamma_coffee_w1300_700_80_R[3]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_1300_993_1010_1000[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_1300_993_1010_1000[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 9]*gamma_coffee_w1300_700_80_R[4]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_1300_990_1006_998[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_1300_990_1006_998[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 9]*gamma_coffee_w1300_700_80_R[5]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_1300_990_1003_998[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_1300_990_1003_998[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 9]*gamma_coffee_w1300_700_80_R[6]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_1300_990_1000_995[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_1300_990_1000_995[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 9]*gamma_coffee_w1300_700_80_R[7]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_1300_988_998_992[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_1300_988_998_992[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 9]*gamma_coffee_w1300_700_80_R[8]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_1300_985_998_990[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_1300_985_998_990[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 9]*gamma_coffee_w1300_700_80_R[9]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_1300_985_996_990[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_1300_985_996_990[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 12]*gamma_coffee_w1300_other_R[0]* 4 / 1000;
			lhbm_whitebuf->nit_80_2_1300[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_80_2_1300[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 15]*gamma_coffee_w1300_other_R[1] * 4 / 1000;
			lhbm_whitebuf->nit_1600_700_1300[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_1600_700_1300[i+1] = temp & 0xFF;
		} else if(i == 2) { // G
			//250nit
			temp = (int)lhbm_cmdbuf[i/2]*gamma_coffee_w250_G[0]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_250[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_250[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 3]*gamma_coffee_w250_G[1]* 4 / 1000;
			lhbm_whitebuf->nit_80_2_250[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_80_2_250[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 6]*gamma_coffee_w250_G[2]* 4 / 1000;
			lhbm_whitebuf->nit_1600_700_250[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_1600_700_250[i+1] = temp & 0xFF;

			//1300nit
			temp = (int)lhbm_cmdbuf[i/2 + 9]*gamma_coffee_w1300_700_80_G[0]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_1300_1000_1018_1005[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_1300_1000_1018_1005[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 9]*gamma_coffee_w1300_700_80_G[1]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_1300_996_1015_1005[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_1300_996_1015_1005[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 9]*gamma_coffee_w1300_700_80_G[2]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_1300_995_1012_1003[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_1300_995_1012_1003[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 9]*gamma_coffee_w1300_700_80_G[3]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_1300_993_1010_1000[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_1300_993_1010_1000[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 9]*gamma_coffee_w1300_700_80_G[4]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_1300_990_1006_998[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_1300_990_1006_998[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 9]*gamma_coffee_w1300_700_80_G[5]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_1300_990_1003_998[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_1300_990_1003_998[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 9]*gamma_coffee_w1300_700_80_G[6]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_1300_990_1000_995[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_1300_990_1000_995[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 9]*gamma_coffee_w1300_700_80_G[7]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_1300_988_998_992[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_1300_988_998_992[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 9]*gamma_coffee_w1300_700_80_G[8]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_1300_985_998_990[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_1300_985_998_990[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 9]*gamma_coffee_w1300_700_80_G[9]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_1300_985_996_990[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_1300_985_996_990[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 12]*gamma_coffee_w1300_other_G[0]* 4 / 1000;
			lhbm_whitebuf->nit_80_2_1300[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_80_2_1300[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 15]*gamma_coffee_w1300_other_G[1]*4/1000;
			lhbm_whitebuf->nit_1600_700_1300[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_1600_700_1300[i+1] = temp & 0xFF;
		} else if(i == 4) {  // B
			//250nit
			temp = (int)lhbm_cmdbuf[i/2]*gamma_coffee_w250_B[0]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_250[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_250[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 3]*gamma_coffee_w250_B[1]* 4 / 1000;
			lhbm_whitebuf->nit_80_2_250[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_80_2_250[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 6]*gamma_coffee_w250_B[2]* 4 / 1000;
			lhbm_whitebuf->nit_1600_700_250[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_1600_700_250[i+1] = temp & 0xFF;

			//1300nit
			temp = (int)lhbm_cmdbuf[i/2 + 9]*gamma_coffee_w1300_700_80_B[0]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_1300_1000_1018_1005[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_1300_1000_1018_1005[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 9]*gamma_coffee_w1300_700_80_B[1]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_1300_996_1015_1005[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_1300_996_1015_1005[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 9]*gamma_coffee_w1300_700_80_B[2]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_1300_995_1012_1003[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_1300_995_1012_1003[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 9]*gamma_coffee_w1300_700_80_B[3]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_1300_993_1010_1000[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_1300_993_1010_1000[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 9]*gamma_coffee_w1300_700_80_B[4]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_1300_990_1006_998[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_1300_990_1006_998[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 9]*gamma_coffee_w1300_700_80_B[5]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_1300_990_1003_998[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_1300_990_1003_998[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 9]*gamma_coffee_w1300_700_80_B[6]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_1300_990_1000_995[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_1300_990_1000_995[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 9]*gamma_coffee_w1300_700_80_B[7]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_1300_988_998_992[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_1300_988_998_992[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 9]*gamma_coffee_w1300_700_80_B[8]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_1300_985_998_990[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_1300_985_998_990[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 9]*gamma_coffee_w1300_700_80_B[9]* 4 / 1000;
			lhbm_whitebuf->nit_700_80_1300_985_996_990[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_700_80_1300_985_996_990[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 12]*gamma_coffee_w1300_other_B[0]* 4 / 1000;
			lhbm_whitebuf->nit_80_2_1300[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_80_2_1300[i+1] = temp & 0xFF;

			temp = (int)lhbm_cmdbuf[i/2 + 15]*gamma_coffee_w1300_other_B[1]*4/1000;
			lhbm_whitebuf->nit_1600_700_1300[i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->nit_1600_700_1300[i+1] = temp & 0xFF;
		}
	}

	//500 nit
	temp = (int)lhbm_cmdbuf[18]*gamma_coffee_w500[0]*4/1000;
	lhbm_whitebuf->nit_700_80_500_100[0] = (temp & 0xFF00) >> 8;
	lhbm_whitebuf->nit_700_80_500_100[1] =  temp & 0xFF;

	temp = (int)lhbm_cmdbuf[18]*gamma_coffee_w500[1]*4/1000;;
	lhbm_whitebuf->nit_700_80_500_101[0] = (temp & 0xFF00) >> 8;
	lhbm_whitebuf->nit_700_80_500_101[1] =  temp & 0xFF;

	temp = (int)lhbm_cmdbuf[19]*gamma_coffee_w500[2]*4/1000;
	lhbm_whitebuf->nit_80_2_500[0] = (temp & 0xFF00) >> 8;
	lhbm_whitebuf->nit_80_2_500[1] =  temp & 0xFF;

	temp = (int)lhbm_cmdbuf[20]*gamma_coffee_w500[3]*4/1000;
	lhbm_whitebuf->nit_1600_700_500[0] = (temp & 0xFF00) >> 8;
	lhbm_whitebuf->nit_1600_700_500[1] =  temp & 0xFF;

	pr_info("250nit 700-80 \n");
	for (i = 0; i < 6; i++){
		pr_info("0x%02hhx",lhbm_whitebuf->nit_700_80_250[i]);
	}

	pr_info("\n250nit 80-2 \n");
	for (i = 0; i < 6; i++){
		pr_info("0x%02hhx ",lhbm_whitebuf->nit_80_2_250[i]);
	}

	pr_info("\n250nit 1600-700 \n");
	for (i = 0; i < 6; i++){
		pr_info("0x%02hhx ",lhbm_whitebuf->nit_1600_700_250[i]);
	}

	pr_info("\n1300nit 700-80 *1\n");
	for (i = 0; i < 6; i++){
		pr_info("0x%02hhx",lhbm_whitebuf->nit_700_80_1300_1000_1018_1005[i]);
		pr_info("0x%02hhx",lhbm_whitebuf->nit_700_80_1300_996_1015_1005[i]);
		pr_info("0x%02hhx",lhbm_whitebuf->nit_700_80_1300_995_1012_1003[i]);
		pr_info("0x%02hhx",lhbm_whitebuf->nit_700_80_1300_993_1010_1000[i]);
		pr_info("0x%02hhx",lhbm_whitebuf->nit_700_80_1300_990_1006_998[i]);
		pr_info("0x%02hhx",lhbm_whitebuf->nit_700_80_1300_990_1003_998[i]);
		pr_info("0x%02hhx",lhbm_whitebuf->nit_700_80_1300_990_1000_995[i]);
		pr_info("0x%02hhx",lhbm_whitebuf->nit_700_80_1300_988_998_992[i]);
		pr_info("0x%02hhx",lhbm_whitebuf->nit_700_80_1300_985_998_990[i]);
		pr_info("0x%02hhx",lhbm_whitebuf->nit_700_80_1300_985_996_990[i]);
	}

	pr_info("\n1300nit 80-2 \n");
	for (i = 0; i < 6; i++){
		pr_info("0x%02hhx ",lhbm_whitebuf->nit_80_2_1300[i]);
	}

	pr_info("\n1300nit 1600-700 \n");
	for (i = 0; i < 6; i++){
		pr_info("0x%02hhx ",lhbm_whitebuf->nit_1600_700_1300[i]);
	}


	pr_info("gir 500 0x%02hhx 0x%02hhx 0x%02hhx 0x%02hhx 0x%02hhx 0x%02hhx,\n",
		lhbm_whitebuf->nit_700_80_500_100[0],lhbm_whitebuf->nit_700_80_500_100[1],
		lhbm_whitebuf->nit_80_2_500[0], lhbm_whitebuf->nit_80_2_500[1],
		lhbm_whitebuf->nit_1600_700_500[0], lhbm_whitebuf->nit_1600_700_500[1]);

	lhbm_w900_readbackdone = true;
	lhbm_w110_readbackdone = true;
	lhbm_g500_readbackdone = true;
	lhbm_w900_update_flag = false;
	lhbm_w110_update_flag = false;
	lhbm_g500_update_flag =false;

	return;
}

static int panel_fod_lhbm_init (struct mtk_dsi* dsi)
{
	if (!dsi) {
		pr_info("invalid dsi point\n");
		return -1;
	}
	pr_info("panel_fod_lhbm_init enter\n");
	dsi->display_type = "primary";
	dsi->mi_cfg.lhbm_ui_ready_delay_frame = 5;
	dsi->mi_cfg.lhbm_ui_ready_delay_frame_aod = 7;
	dsi->mi_cfg.local_hbm_enabled = 1;

	return 0;
}

static int mi_disp_panel_send_lhbm(struct mtk_dsi * dsi, enum lhbm_cmd_type type, int bl_level)
{

	struct lcm * ctx = NULL;
	unsigned int format = FORMAT_LP_MODE | FORMAT_BLOCK;
	if(!dsi) {
		pr_err("dsi is null\n");
		return -EINVAL;
	}

	ctx = panel_to_lcm(dsi->panel);
	if(!ctx) {
		pr_err("ctx is null\n");
		return -EINVAL;
	}

	if (type == TYPE_LHBM_OFF) {
		lhbm_off[0].para_list[0] = (bl_level >> 8) & 0xFF;
		lhbm_off[0].para_list[1] = bl_level & 0xFF;
		mi_disp_panel_ddic_send_cmd(lhbm_off, ARRAY_SIZE(lhbm_off), format);
		return 0;
	}

	if (bl_level < LHBM_BL_INTERVAL_1_START)
		bl_level = LHBM_BL_INTERVAL_1_START;
	else if (bl_level > LHBM_BL_INTERVAL_3_END)
		bl_level = LHBM_BL_INTERVAL_3_END;

	if (bl_level >= LHBM_BL_INTERVAL_2_START && bl_level <= LHBM_BL_INTERVAL_2_END) {//700nit~80nit
		switch (type) {
			case TYPE_WHITE_1300:
			case TYPE_GREEN_500:
			case TYPE_WHITE_250:
				mi_disp_panel_ddic_send_cmd(lhbm_normal_700nit_80nit, ARRAY_SIZE(lhbm_normal_700nit_80nit), format);
			break;
			case TYPE_HLPM_W1300:
			case TYPE_HLPM_W250:
				mi_disp_panel_ddic_send_cmd(lhbm_hlpm_700nit_80nit, ARRAY_SIZE(lhbm_hlpm_700nit_80nit), format);
			break;
			default:
				pr_err("unsuppport cmd \n");
			return -EINVAL;
		}
	} else if (bl_level >= LHBM_BL_INTERVAL_1_START && bl_level <= LHBM_BL_INTERVAL_1_END) {//80nit~2nit
		switch (type) {
			case TYPE_WHITE_1300:
			case TYPE_GREEN_500
:
			case TYPE_WHITE_250:
				mi_disp_panel_ddic_send_cmd(lhbm_normal_80nit_2nit, ARRAY_SIZE(lhbm_normal_80nit_2nit), format);
			break;
			case TYPE_HLPM_W1300:
			case TYPE_HLPM_W250:
				mi_disp_panel_ddic_send_cmd(lhbm_hlpm_80nit_2nit, ARRAY_SIZE(lhbm_hlpm_80nit_2nit), format);
			break;
			default:
				pr_err("unsuppport cmd \n");
			return -EINVAL;
		}
	} else if (bl_level >= LHBM_BL_INTERVAL_3_START && bl_level <= LHBM_BL_INTERVAL_3_END) {//1600nit~700nit
		switch (type) {
			case TYPE_WHITE_1300:
			case TYPE_GREEN_500:
			case TYPE_WHITE_250:
				mi_disp_panel_ddic_send_cmd(lhbm_normal_1600nit_700nit, ARRAY_SIZE(lhbm_normal_1600nit_700nit), format);
			break;
			case TYPE_HLPM_W1300:
			case TYPE_HLPM_W250:
				mi_disp_panel_ddic_send_cmd(lhbm_hlpm_1600nit_700nit, ARRAY_SIZE(lhbm_hlpm_1600nit_700nit), format);
			break;
			default:
				pr_err("unsuppport cmd \n");
			return -EINVAL;
		}
	} else {
		pr_info("Error--lhbm_cmd_type:%d , %d backlight is Out of range \n", type, bl_level);
	}
	return 0;
}

static int mi_disp_panel_update_lhbm_white_param(struct mtk_dsi * dsi, enum lhbm_cmd_type type, int bl_level)
{
	int i = 0;
	struct lcm * ctx = NULL;

	if(!dsi) {
		pr_err("dsi is null\n");
		return -EINVAL;
	}

	ctx = panel_to_lcm(dsi->panel);
	if(!ctx) {
		pr_err("ctx is null\n");
		return -EINVAL;
	}
	/*
	if (!panel->panel_initialized) {
		DISP_ERROR("[%s] Panel not initialized\n", panel->type);
		rc = -EINVAL;
		goto exit;
	}
	*/

	if(!lhbm_w900_readbackdone ||
		 !lhbm_w110_readbackdone ||
		 !lhbm_g500_readbackdone) {
		pr_info("mi_disp_panel_update_lhbm_white_param cmdline_lhbm:%s\n", oled_lhbm_cmdline);

		mi_parse_cmdline_perBL(&lhbm_whitebuf);
	}

	if (bl_level < LHBM_BL_INTERVAL_1_START)
		bl_level = LHBM_BL_INTERVAL_1_START;
	else if (bl_level > LHBM_BL_INTERVAL_3_END)
		bl_level = LHBM_BL_INTERVAL_3_END;

	pr_info("lhbm update 0xD0, lhbm_cmd_type:%d backlight:%d \n", type, bl_level);

	if (bl_level >= LHBM_BL_INTERVAL_2_START && bl_level <= LHBM_BL_INTERVAL_2_END) {//700nit~80nit
		switch (type) {
			case TYPE_WHITE_1300:
				if (bl_level >= 0x51D && bl_level <= 0xC1D) {// R/G/B = 1.000/1.018/1.005
					for (i = 0; i < 6; i++) {
						lhbm_normal_700nit_80nit[15].para_list[i] = lhbm_whitebuf.nit_700_80_1300_1000_1018_1005[i];
					}
				} else if (bl_level >= 0xC1E && bl_level <= 0xE1A) {// R/G/B = 0.996/1.015/1.005
					for (i = 0; i < 6; i++) {
						lhbm_normal_700nit_80nit[15].para_list[i] = lhbm_whitebuf.nit_700_80_1300_996_1015_1005[i];
					}
				} else if (bl_level >= 0xE1B && bl_level <= 0x1017) {// R/G/B = 0.995/1.012/1.003
					for (i = 0; i < 6; i++) {
						lhbm_normal_700nit_80nit[15].para_list[i] = lhbm_whitebuf.nit_700_80_1300_995_1012_1003[i];
					}
				} else if (bl_level >= 0x1018 && bl_level <= 0x1214) {// R/G/B = 0.993/1.010/1.000
					for (i = 0; i < 6; i++) {
						lhbm_normal_700nit_80nit[15].para_list[i] = lhbm_whitebuf.nit_700_80_1300_993_1010_1000[i];
					}
				} else if (bl_level >= 0x1215 && bl_level <= 0x160E) {// R/G/B = 0.990/1.006/0.998
					for (i = 0; i < 6; i++) {
						lhbm_normal_700nit_80nit[15].para_list[i] = lhbm_whitebuf.nit_700_80_1300_990_1006_998[i];
					}
				}  else if (bl_level >= 0x160F && bl_level <= 0x180B) {// R/G/B = 0.990/1.003/0.998
					for (i = 0; i < 6; i++) {
						lhbm_normal_700nit_80nit[15].para_list[i] = lhbm_whitebuf.nit_700_80_1300_990_1003_998[i];
					}
				}  else if (bl_level >= 0x180C && bl_level <= 0x1C05) {// R/G/B = 0.990/1.000/0.995
					for (i = 0; i < 6; i++) {
						lhbm_normal_700nit_80nit[15].para_list[i] = lhbm_whitebuf.nit_700_80_1300_990_1000_995[i];
					}
				}  else if (bl_level >= 0x1C06 && bl_level <= 0x1FFE) {// R/G/B = 0.988/0.998/0.992
					for (i = 0; i < 6; i++) {
						lhbm_normal_700nit_80nit[15].para_list[i] = lhbm_whitebuf.nit_700_80_1300_988_998_992[i];
					}
				}  else if (bl_level >= 0x1FFF && bl_level <= 0x2664) {// R/G/B = 0.985/0.998/0.990
					for (i = 0; i < 6; i++) {
						lhbm_normal_700nit_80nit[15].para_list[i] = lhbm_whitebuf.nit_700_80_1300_985_998_990[i];
					}
				}  else if (bl_level >= 0x2665 && bl_level <= 0x2CCB) {// R/G/B = 0.985/0.996/0.990
					for (i = 0; i < 6; i++) {
						lhbm_normal_700nit_80nit[15].para_list[i] = lhbm_whitebuf.nit_700_80_1300_985_996_990[i];
					}
				}
			break;
			case TYPE_HLPM_W1300:
				if (bl_level >= 0x51D && bl_level <= 0xC1D) {// R/G/B = 1.000/1.018/1.005
					for (i = 0; i < 6; i++) {
						lhbm_hlpm_700nit_80nit[16].para_list[i] = lhbm_whitebuf.nit_700_80_1300_1000_1018_1005[i];
					}
				} else if (bl_level >= 0xC1E && bl_level <= 0xE1A) {// R/G/B = 0.996/1.015/1.005
					for (i = 0; i < 6; i++) {
						lhbm_hlpm_700nit_80nit[16].para_list[i] = lhbm_whitebuf.nit_700_80_1300_996_1015_1005[i];
					}
				} else if (bl_level >= 0xE1B && bl_level <= 0x1017) {// R/G/B = 0.995/1.012/1.003
					for (i = 0; i < 6; i++) {
						lhbm_hlpm_700nit_80nit[16].para_list[i] = lhbm_whitebuf.nit_700_80_1300_995_1012_1003[i];
					}
				} else if (bl_level >= 0x1018 && bl_level <= 0x1214) {// R/G/B = 0.993/1.010/1.000
					for (i = 0; i < 6; i++) {
						lhbm_hlpm_700nit_80nit[16].para_list[i] = lhbm_whitebuf.nit_700_80_1300_993_1010_1000[i];
					}
				} else if (bl_level >= 0x1215 && bl_level <= 0x160E) {// R/G/B = 0.990/1.006/0.998
					for (i = 0; i < 6; i++) {
						lhbm_hlpm_700nit_80nit[16].para_list[i] = lhbm_whitebuf.nit_700_80_1300_990_1006_998[i];
					}
				}  else if (bl_level >= 0x160F && bl_level <= 0x180B) {// R/G/B = 0.990/1.003/0.998
					for (i = 0; i < 6; i++) {
						lhbm_hlpm_700nit_80nit[16].para_list[i] = lhbm_whitebuf.nit_700_80_1300_990_1003_998[i];
					}
				}  else if (bl_level >= 0x180C && bl_level <= 0x1C05) {// R/G/B = 0.990/1.000/0.995
					for (i = 0; i < 6; i++) {
						lhbm_hlpm_700nit_80nit[16].para_list[i] = lhbm_whitebuf.nit_700_80_1300_990_1000_995[i];
					}
				}  else if (bl_level >= 0x1C06 && bl_level <= 0x1FFE) {// R/G/B = 0.988/0.998/0.992
					for (i = 0; i < 6; i++) {
						lhbm_hlpm_700nit_80nit[16].para_list[i] = lhbm_whitebuf.nit_700_80_1300_988_998_992[i];
					}
				}  else if (bl_level >= 0x1FFF && bl_level <= 0x2664) {// R/G/B = 0.985/0.998/0.990
					for (i = 0; i < 6; i++) {
						lhbm_hlpm_700nit_80nit[16].para_list[i] = lhbm_whitebuf.nit_700_80_1300_985_998_990[i];
					}
				}  else if (bl_level >= 0x2665 && bl_level <= 0x2CCB) {// R/G/B = 0.985/0.996/0.990
					for (i = 0; i < 6; i++) {
						lhbm_hlpm_700nit_80nit[16].para_list[i] = lhbm_whitebuf.nit_700_80_1300_985_996_990[i];
					}
				}
			break;
			case TYPE_WHITE_250:
				for (i = 0; i < 6; i++) {
					lhbm_normal_700nit_80nit[15].para_list[i] = lhbm_whitebuf.nit_700_80_250[i];
				}
			break;
			case TYPE_HLPM_W250:
				for (i = 0; i < 6; i++) {
					lhbm_hlpm_700nit_80nit[16].para_list[i] = lhbm_whitebuf.nit_700_80_250[i];
				}
			break;
			case TYPE_GREEN_500:
			if (bl_level >= 0x51D && bl_level <= 0x180B) {//1.01
				for (i = 0; i < 6; i++) {
					lhbm_normal_700nit_80nit[15].para_list[i] =
						(i==2 || i==3)?lhbm_whitebuf.nit_700_80_500_101[i-2]:0;
				}
			} else if (bl_level >= 0x180C && bl_level <= 0x2CCB) {//1.0
				for (i = 0; i < 6; i++) {
					lhbm_normal_700nit_80nit[15].para_list[i] =
						(i==2 || i==3)?lhbm_whitebuf.nit_700_80_500_100[i-2]:0;
				}
			}
			break;
			default:
				pr_err("unsuppport cmd \n");
		}

	} else if (bl_level >= LHBM_BL_INTERVAL_1_START && bl_level <= LHBM_BL_INTERVAL_1_END) {//80nit~2nit
		switch (type) {
			case TYPE_WHITE_1300:
				for (i = 0; i < 6; i++) {
					lhbm_normal_80nit_2nit[21].para_list[i] = lhbm_whitebuf.nit_80_2_1300[i];
				}
			break;
			case TYPE_HLPM_W1300:
				for (i = 0; i < 6; i++) {
					lhbm_hlpm_80nit_2nit[22].para_list[i] = lhbm_whitebuf.nit_80_2_1300[i];
				}
			break;
			case TYPE_WHITE_250:
				for (i = 0; i < 6; i++) {
					lhbm_normal_80nit_2nit[21].para_list[i] = lhbm_whitebuf.nit_80_2_250[i];
				}
			break;
			case TYPE_HLPM_W250:
				for (i = 0; i < 6; i++) {
					lhbm_hlpm_80nit_2nit[22].para_list[i] = lhbm_whitebuf.nit_80_2_250[i];
				}
			break;
			case TYPE_GREEN_500
:
				for (i = 0; i < 6; i++) {
					lhbm_normal_80nit_2nit[21].para_list[i] =
						(i==2 || i==3)?lhbm_whitebuf.nit_80_2_500[i-2]:0;
				}
			break;
			default:
				pr_err("unsuppport cmd \n");
		}
	} else if (bl_level >= LHBM_BL_INTERVAL_3_START && bl_level <= LHBM_BL_INTERVAL_3_END) {//1600nit~700nit
		switch (type) {
			case TYPE_WHITE_1300:
				for (i = 0; i < 6; i++) {
					lhbm_normal_1600nit_700nit[21].para_list[i] = lhbm_whitebuf.nit_1600_700_1300[i];
				}
			break;
			case TYPE_HLPM_W1300:
				for (i = 0; i < 6; i++) {
					lhbm_hlpm_1600nit_700nit[22].para_list[i] = lhbm_whitebuf.nit_1600_700_1300[i];
				}
			break;
			case TYPE_WHITE_250:
				for (i = 0; i < 6; i++) {
					lhbm_normal_1600nit_700nit[21].para_list[i] = lhbm_whitebuf.nit_1600_700_250[i];
				}
			break;
			case TYPE_HLPM_W250:
				for (i = 0; i < 6; i++) {
					lhbm_hlpm_1600nit_700nit[22].para_list[i] = lhbm_whitebuf.nit_1600_700_250[i];
				}
			break;
			case TYPE_GREEN_500:
				for (i = 0; i < 6; i++) {
					lhbm_normal_1600nit_700nit[21].para_list[i] =
						(i==2 || i==3)?lhbm_whitebuf.nit_1600_700_500[i-2]:0;
				}
			break;
			default:
				pr_err("unsuppport cmd \n");
		}
	} else {
		pr_info("Error--lhbm_cmd_type:%d , %d backlight is Out of range \n", type, bl_level);
	}
	return -1;
}

static void mi_disp_panel_update_lhbm_backlight(struct mtk_dsi *dsi,enum lhbm_cmd_type type , int bl_level) {
	u8 backlight_buf[2] = {0};
	struct lcm *ctx = NULL;

	if (!dsi || type >= TYPE_MAX) {
		pr_err("invalid params\n");
		return;
	}

	ctx = panel_to_lcm(dsi->panel);
	pr_info("%s [%d] bl_lvl = %d,\n", __func__, type, bl_level);

	backlight_buf[0] = bl_level >> 8;
	backlight_buf[1] = bl_level & 0xFF;

	switch (type) {
	case TYPE_HLPM_W1300:
		lhbm_hlpm_white_1200nit[6].para_list[1] = backlight_buf[0];
		lhbm_hlpm_white_1200nit[6].para_list[2] = backlight_buf[1];
		lhbm_hlpm_white_1200nit[6].para_list[3] = backlight_buf[0];
		lhbm_hlpm_white_1200nit[6].para_list[4] = backlight_buf[1];
		if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_HBM)
			lhbm_hlpm_white_1200nit[6].para_list[5] = 0x03;
		else if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_LBM)
			lhbm_hlpm_white_1200nit[6].para_list[5] = 0x01;
		break;
	case TYPE_HLPM_W250:
		lhbm_hlpm_white_250nit[6].para_list[1] = backlight_buf[0];
		lhbm_hlpm_white_250nit[6].para_list[2] = backlight_buf[1];
		lhbm_hlpm_white_250nit[6].para_list[3] = backlight_buf[0];
		lhbm_hlpm_white_250nit[6].para_list[4] = backlight_buf[1];
		if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_HBM)
			lhbm_hlpm_white_250nit[6].para_list[5] = 0x03;
		else if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_LBM)
			lhbm_hlpm_white_250nit[6].para_list[5] = 0x01;
		break;
	default:
		pr_err("unsupport update backlight type\n");
		return;
	}
}

static void mi_disp_panel_update_lhbm_alpha(struct mtk_dsi *dsi,enum lhbm_cmd_type type , int bl_level) {
	u8 alpha_buf[2] = {0};
	uint32_t mi_hw_country_ver = CountryCN; /*get_hw_country_version();*/
	if (!dsi || type >= TYPE_MAX) {
		pr_err("invalid params\n");
		return;
	}

	if (bl_level < LHBM_BL_INTERVAL_1_START)
		bl_level = LHBM_BL_INTERVAL_1_START;
	else if (bl_level > LHBM_BL_INTERVAL_3_END)
		bl_level = LHBM_BL_INTERVAL_3_END;

	pr_info("%s [%d] bl_lvl = %d,\n", __func__, type, bl_level);

	if (bl_level >= LHBM_BL_INTERVAL_1_START && bl_level <= LHBM_BL_INTERVAL_1_END) {//80nit~2nit
		alpha_buf[0] = (aa_alpha_set_80_2[bl_level-1] >> 8) & 0xff;
		alpha_buf[1] = aa_alpha_set_80_2[bl_level-1] & 0xff;
		switch (type) {
			case TYPE_WHITE_1300:
			case TYPE_GREEN_500:
			case TYPE_WHITE_250:
				lhbm_normal_80nit_2nit[19].para_list[1] = alpha_buf[0];
				lhbm_normal_80nit_2nit[19].para_list[2] = alpha_buf[1];
				if (mi_hw_country_ver != CountryCN) {
					lhbm_normal_80nit_2nit[6].para_list[0] = 0x39;
				}
			break;
			case TYPE_HLPM_W1300:
			case TYPE_HLPM_W250:
				lhbm_hlpm_80nit_2nit[20].para_list[1] = alpha_buf[0];
				lhbm_hlpm_80nit_2nit[20].para_list[2] = alpha_buf[1];
				if (mi_hw_country_ver != CountryCN) {
					lhbm_hlpm_80nit_2nit[7].para_list[0] = 0x39;
				}
			break;
			default:
				pr_err("unsuppport cmd \n");
		}
	} else if (bl_level >= LHBM_BL_INTERVAL_3_START && bl_level <= LHBM_BL_INTERVAL_3_END) {//1600nit~700nit
		alpha_buf[0] = (aa_alpha_set_1600_700[bl_level-0x2CCC] >> 8) & 0xff;
		alpha_buf[1] = aa_alpha_set_1600_700[bl_level-0x2CCC] & 0xff;
		switch (type) {
			case TYPE_WHITE_1300:
			case TYPE_GREEN_500:
			case TYPE_WHITE_250:
				lhbm_normal_1600nit_700nit[19].para_list[1] = alpha_buf[0];
				lhbm_normal_1600nit_700nit[19].para_list[2] = alpha_buf[1];
				if (mi_hw_country_ver != CountryCN) {
					lhbm_normal_1600nit_700nit[6].para_list[0] = 0x11;
				}
			break;
			case TYPE_HLPM_W1300:
			case TYPE_HLPM_W250:
				lhbm_hlpm_1600nit_700nit[20].para_list[1] = alpha_buf[0];
				lhbm_hlpm_1600nit_700nit[20].para_list[2] = alpha_buf[1];
				if (mi_hw_country_ver != CountryCN) {
					lhbm_normal_1600nit_700nit[7].para_list[0] = 0x11;
				}
			break;
			default:
				pr_err("unsuppport cmd \n");
		}
	}
	pr_info("mi_disp_panel_update_lhbm_alpha end\n");
	return;
}

static int panel_set_lhbm_fod(struct mtk_dsi *dsi, enum local_hbm_state lhbm_state)
{
	struct lcm *ctx = NULL;
	struct mi_dsi_panel_cfg *mi_cfg = NULL;
	int bl_level;
	int flat_mode;
	int bl_level_doze = doze_lbm_dbv_level;

	if (!dsi || !dsi->panel) {
		pr_err("%s: panel is NULL\n", __func__);
		return -1;
	}

	ctx = panel_to_lcm(dsi->panel);
	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
		return -1;
	}

	mi_cfg = &dsi->mi_cfg;
	bl_level = mi_cfg->last_no_zero_bl_level;
	flat_mode = mi_cfg->feature_val[DISP_FEATURE_FLAT_MODE];

	if (atomic_read(&doze_enable) &&
		ctx->doze_brightness_state == DOZE_BRIGHTNESS_LBM)
			bl_level_doze = doze_lbm_dbv_level;
	else if (atomic_read(&doze_enable) &&
		ctx->doze_brightness_state == DOZE_BRIGHTNESS_HBM)
			bl_level_doze = doze_hbm_dbv_level;

	pr_info("%s local hbm_state :%d \n", __func__, lhbm_state);
	pr_info("bl_level:%d  flat_mode:%d\n", bl_level, flat_mode);
	switch (lhbm_state) {
	case LOCAL_HBM_OFF_TO_NORMAL:
		pr_info("LOCAL_HBM_NORMAL off\n");
		mi_disp_panel_send_lhbm(dsi, TYPE_LHBM_OFF, bl_level);
		ctx->lhbm_en = false;
		break;
	case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT:
		pr_info("LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT\n");
		if (atomic_read(&doze_enable)) {
			if (mi_cfg->last_bl_level)
				mi_disp_panel_send_lhbm(dsi, TYPE_LHBM_OFF, mi_cfg->last_bl_level);
			else
				mi_disp_panel_send_lhbm(dsi, TYPE_LHBM_OFF, bl_level_doze);
		} else {
			mi_disp_panel_send_lhbm(dsi, TYPE_LHBM_OFF, bl_level);
		}
		ctx->lhbm_en = false;
		break;
	case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE:
		pr_info("LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE set bl %d\n",bl_level);
		if (atomic_read(&doze_enable)) {
			if (mi_cfg->last_bl_level)
				mi_disp_panel_send_lhbm(dsi, TYPE_LHBM_OFF, mi_cfg->last_bl_level);
			else
				mi_disp_panel_send_lhbm(dsi, TYPE_LHBM_OFF, 0);
		} else {
			mi_disp_panel_send_lhbm(dsi, TYPE_LHBM_OFF, bl_level);
		}
		ctx->lhbm_en = false;
		break;
	case LOCAL_HBM_NORMAL_WHITE_750NIT:
	case LOCAL_HBM_NORMAL_WHITE_500NIT:
		break;
	case LOCAL_HBM_NORMAL_WHITE_110NIT:
		if (atomic_read(&doze_enable)) {
			mi_disp_panel_update_lhbm_alpha(dsi, TYPE_HLPM_W250, bl_level_doze);
			mi_disp_panel_update_lhbm_white_param(dsi, TYPE_HLPM_W250, bl_level_doze);
			//mi_disp_panel_update_lhbm_backlight(dsi, TYPE_HLPM_W1300, bl_level_doze);
			mi_disp_panel_send_lhbm(dsi, TYPE_HLPM_W250, bl_level_doze);
			ctx->doze_suspend = false;
		} else {
			mi_disp_panel_update_lhbm_alpha(dsi, TYPE_WHITE_250, bl_level);
			mi_disp_panel_update_lhbm_white_param(dsi, TYPE_WHITE_250, bl_level);
			mi_disp_panel_send_lhbm(dsi, TYPE_WHITE_250, bl_level);
		}
		ctx->lhbm_en = true;
		break;
	case LOCAL_HBM_NORMAL_GREEN_500NIT:
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		pr_info("LOCAL_HBM_NORMAL_GREEN_500NIt\n");
		mi_disp_panel_update_lhbm_alpha(dsi, TYPE_GREEN_500, bl_level);
		mi_disp_panel_update_lhbm_white_param(dsi, TYPE_GREEN_500, bl_level);
		mi_disp_panel_send_lhbm(dsi, TYPE_GREEN_500, bl_level);
		ctx->doze_suspend = false;
		ctx->lhbm_en = true;
		break;
	case LOCAL_HBM_NORMAL_WHITE_1000NIT:
		pr_info("LOCAL_HBM_NORMAL_WHITE_1300NIT in HBM\n");
		if (atomic_read(&doze_enable)) {
			mi_disp_panel_update_lhbm_alpha(dsi, TYPE_HLPM_W1300, bl_level_doze);
			mi_disp_panel_update_lhbm_white_param(dsi, TYPE_HLPM_W1300, bl_level_doze);
			//mi_disp_panel_update_lhbm_backlight(dsi, TYPE_HLPM_W1300, bl_level_doze);
			mi_disp_panel_send_lhbm(dsi, TYPE_HLPM_W1300, bl_level_doze);
			ctx->doze_suspend = false;
		} else {
			mi_disp_panel_update_lhbm_alpha(dsi, TYPE_WHITE_1300, bl_level);
			mi_disp_panel_update_lhbm_white_param(dsi, TYPE_WHITE_1300, bl_level);
			mi_disp_panel_send_lhbm(dsi, TYPE_WHITE_1300, bl_level);
		}
		ctx->lhbm_en = true;
		break;
	case LOCAL_HBM_HLPM_WHITE_1000NIT:
		pr_info("LOCAL_HBM_HLPM_WHITE_1300NIT in HBM\n");
		if (atomic_read(&doze_enable) || !bl_level) {
			mi_disp_panel_update_lhbm_alpha(dsi, TYPE_HLPM_W1300, bl_level_doze);
			mi_disp_panel_update_lhbm_white_param(dsi, TYPE_HLPM_W1300, bl_level_doze);
			mi_disp_panel_send_lhbm(dsi, TYPE_HLPM_W1300, bl_level_doze);
		} else {
			mi_disp_panel_update_lhbm_alpha(dsi, TYPE_HLPM_W1300, bl_level);
			mi_disp_panel_update_lhbm_white_param(dsi, TYPE_HLPM_W1300, bl_level);
			mi_disp_panel_send_lhbm(dsi, TYPE_HLPM_W1300, bl_level);
		}
		//mi_disp_panel_update_lhbm_backlight(dsi, TYPE_HLPM_W1300, bl_level_doze);
		ctx->doze_suspend = false;
		ctx->lhbm_en = true;
		break;
	case LOCAL_HBM_HLPM_WHITE_110NIT:
		pr_info("LOCAL_HBM_HLPM_WHITE_250NIT\n");
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		if (atomic_read(&doze_enable) || !bl_level) {
			mi_disp_panel_update_lhbm_alpha(dsi, TYPE_HLPM_W250, bl_level_doze);
			mi_disp_panel_update_lhbm_white_param(dsi, TYPE_HLPM_W250, bl_level_doze);
			mi_disp_panel_send_lhbm(dsi, TYPE_HLPM_W250, bl_level_doze);
		} else {
			mi_disp_panel_update_lhbm_alpha(dsi, TYPE_HLPM_W250, bl_level);
			mi_disp_panel_update_lhbm_white_param(dsi, TYPE_HLPM_W250, bl_level);
			mi_disp_panel_send_lhbm(dsi, TYPE_HLPM_W250, bl_level);
		}
		//mi_disp_panel_update_lhbm_backlight(dsi, TYPE_HLPM_W1300, bl_level_doze);
		ctx->doze_suspend = false;
		ctx->lhbm_en = true;
		break;
	default:
		pr_info("invalid local hbm value\n");
		break;
	}

	return 0;
}

static int panel_fod_state_check (void * dsi, dcs_write_gce cb, void *handle)
{
	struct lcm *ctx = NULL;
	struct mtk_dsi *mtk_dsi = (struct mtk_dsi *)dsi;

	if (!mtk_dsi || !mtk_dsi->panel) {
		pr_err("dsi is null\n");
		return -1;
	}

	ctx = panel_to_lcm(mtk_dsi->panel);

	if (ctx->lhbm_en) {
		/*char lhbm_off_page[] = {0x87,0x24};
		cb(dsi, handle, lhbm_off_page, ARRAY_SIZE(lhbm_off_page));*/
		ctx->lhbm_en = false;
		pr_info("%s set lhbm off\n", __func__);
	} else {
		pr_info("%s lhbm not enable\n", __func__);
	}

	pr_info("%s !-\n", __func__);
	return 0;
}
#endif

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

int panel_get_wp_info(struct drm_panel *panel, char *buf, size_t size)
{
	/* Read 3 bytes from 0xDF register
	 * 	BIT[0] = Lux
	 * 	BIT[1] = Wx
	 * 	BIT[2] = Wy */
	static uint16_t lux = 0, wx = 0, wy = 0;
	int count = 0;
	u8 rx_buf[6] = {0x00};
	pr_info("%s: +\n", __func__);

	/* try to get wp info from cache */
	if (lux > 0 && wx > 0 && wy > 0) {
		pr_info("%s: got wp info from cache\n", __func__);
		goto cache;
	}

	/* try to get wp info from cmdline */
	if (sscanf(oled_wp_cmdline, "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n",
			&rx_buf[0], &rx_buf[1], &rx_buf[2], &rx_buf[3],
			&rx_buf[4], &rx_buf[5]) == 6) {

		if (rx_buf[0] == 1 && rx_buf[1] == 2 && rx_buf[2] == 3 &&
			rx_buf[3] == 4 && rx_buf[4] == 5 && rx_buf[5] == 6) {
			pr_err("No panel is Connected !");
			goto err;
		}

		lux = rx_buf[0] << 8 | rx_buf[1];
		wx = rx_buf[2] << 8 | rx_buf[3];
		wy = rx_buf[4] << 8 | rx_buf[5];
		if (lux > 0 && wx > 0 && wy > 0) {
			pr_info("%s: got wp info from cmdline\n", __func__);
			goto done;
		}
	} else {
		pr_info("%s: get error\n", __func__);
		goto err;
	}

cache:
	rx_buf[0]  = (lux >> 8) & 0x00ff;
	rx_buf[1] = lux & 0x00ff;

	rx_buf[2] = (wx >> 8) & 0x00ff;
	rx_buf[3] = wx & 0x00ff;

	rx_buf[4] = (wy >> 8) & 0x00ff;
	rx_buf[5] = wy & 0x00ff;
done:
	count = snprintf(buf, size, "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n",
			rx_buf[0], rx_buf[1], rx_buf[2], rx_buf[3],
			rx_buf[4], rx_buf[5]);

	pr_info("%s: Lux=0x%04hx, Wx=0x%04hx, Wy=0x%04hx\n", __func__, lux, wx, wy);
err:
	pr_info("%s: -\n", __func__);
	return count;
}

static int panel_set_gray_by_temperature (struct drm_panel *panel, int level)
{
	int ret = 0;
	struct lcm *ctx;
	pr_info("%s: +\n", __func__);

	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		ret = -1;
		goto err;
	}
	/*if (panel_build_id < PANEL_BUILD_ID_P11_Temp) {
		pr_err("%s: panel build id is earlier than p1.1\n", __func__);
		ret = -1;
		goto err;
	}*/
	ctx = panel_to_lcm(panel);
	if (!ctx->enabled) {
		ctx->gray_level = level;
		ret = -1;
		pr_err("%s: panel isn't enabled\n", __func__);
		goto err;
	}
	pr_info("%s: level = %x\n", __func__, level);
	ctx->gray_level = level;
	if (level > DBI_TEMP_INDEX1) 
		level = DBI_TEMP_INDEX1;
	else if (level < DBI_TEMP_INDEX5)
		level = DBI_TEMP_INDEX5;

	gray_3d_lut[0].para_list[1] = level;
	mi_disp_panel_ddic_send_cmd(gray_3d_lut, ARRAY_SIZE(gray_3d_lut), false);
err:
	pr_info("%s: -, level:0x%x\n", __func__, level);
	return ret;
}

int panel_get_sn_info(struct drm_panel *panel, char *buf, size_t size)
{
	int count = 0;
	u8 rx_buf[14] = {0x00};
	int i = 0;
	pr_info("%s: %s+\n", __func__, panel_sn_cmdline);
	for (i = 0; i < 14; i++) {
		sscanf(panel_sn_cmdline + i, "%c", &rx_buf[i]);
	}
	for (i = 0; i < 14; i++) {
		buf[i] = rx_buf[i];
		pr_info("%s: rx_buf[%d]= %c\n", __func__, i,  buf[i]);
		count++;
	}
	pr_info("%s: -\n", __func__);
	return count;
}

static int lcm_update_roi_cmdq(void *dsi, dcs_write_gce cb, void *handle,
	unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
	int ret = 0;
	//unsigned int x0 = x;
	unsigned int y0 = y;
	//unsigned int x1 = x0 + w - 1;
	unsigned int y1 = y0 + h - 1;
	//unsigned char x0_msb = ((x0 >> 8) & 0xFF);
	//unsigned char x0_lsb = (x0 & 0xFF);
	//unsigned char x1_msb = ((x1 >> 8) & 0xFF);
	//unsigned char x1_lsb = (x1 & 0xFF);
	unsigned char y0_msb = ((y0 >> 8) & 0xFF);
	unsigned char y0_lsb = (y0 & 0xFF);
	unsigned char y1_msb = ((y1 >> 8) & 0xFF);
	unsigned char y1_lsb = (y1 & 0xFF);

	//set TE scan line: display total line - slice height + 8 = 2368
	//char te_sl[] = { 0x44, 0x09, 0x40};
	//char roi_x[] = { 0x2A, x0_msb, x0_lsb, x1_msb, x1_lsb};
	char roi_y[] = { 0x2B, y0_msb, y0_lsb, y1_msb, y1_lsb};

	pr_info("%s (x,y,w,h): (%d,%d,%d,%d)\n", __func__, x, y, w, h);

	if (!cb)
		return -1;

	//cb(dsi, handle, te_sl, ARRAY_SIZE(te_sl));
	//cb(dsi, handle, roi_x, ARRAY_SIZE(roi_x));
	cb(dsi, handle, roi_y, ARRAY_SIZE(roi_y));

	return ret;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.ext_param_set = mtk_panel_ext_param_set,
	.ext_param_get = mtk_panel_ext_param_get,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	//.set_bl_elvss_cmdq = lcm_set_bl_elvss_cmdq,
	.mode_switch = mode_switch,
	.panel_poweron = lcm_panel_poweron,
	.panel_poweroff = lcm_panel_poweroff,
	.doze_enable = panel_doze_enable,
	.doze_disable = panel_doze_disable,
	//.read_elvss_base_voltage = lcm_read_elvss_base_voltage,
	.lcm_update_roi_cmdq = lcm_update_roi_cmdq,
#ifdef CONFIG_MI_DISP
	.esd_restore_backlight = lcm_esd_restore_backlight,
#ifdef CONFIG_MI_DISP_FP_STATE
	.fp_state_restore_backlight = lcm_fp_state_restore_backlight,
#endif
	//.panel_set_gir_on = panel_set_gir_on,
	//.panel_set_gir_off = panel_set_gir_off,
	.panel_get_gir_status = panel_get_gir_status,
	.get_panel_max_brightness_clone = panel_get_max_brightness_clone,
	.get_panel_factory_max_brightness = panel_get_factory_max_brightness,
	.get_panel_initialized = get_panel_initialized,
	.get_panel_info = panel_get_panel_info,
	.set_doze_brightness = panel_set_doze_brightness,
	.get_doze_brightness = panel_get_doze_brightness,
	.doze_suspend = panel_doze_suspend,
	//.panel_fod_lhbm_init = panel_fod_lhbm_init,
	//.set_lhbm_fod = panel_set_lhbm_fod,
	.get_wp_info = panel_get_wp_info,
	.set_gray_by_temperature = panel_set_gray_by_temperature,
	//.fod_state_check = panel_fod_state_check,
	.get_sn_info = panel_get_sn_info,
	//.set_dimming_on = panel_set_dimming_on,
#endif
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

static int lcm_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct drm_display_mode *mode0, *mode1, *mode2, *mode3;

	mode0 = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode0) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}
	drm_mode_set_name(mode0);
	mode0->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode0);

	mode2 = drm_mode_duplicate(connector->dev, &middle_mode);
	if (!mode2) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			middle_mode.hdisplay, middle_mode.vdisplay,
			drm_mode_vrefresh(&middle_mode));
		return -ENOMEM;
	}
	drm_mode_set_name(mode2);
	mode2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode2);

	mode1 = drm_mode_duplicate(connector->dev, &performence_mode);
	if (!mode1) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			performence_mode.hdisplay, performence_mode.vdisplay,
			drm_mode_vrefresh(&performence_mode));
		return -ENOMEM;
	}
	drm_mode_set_name(mode1);
	mode1->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode1);

	mode3 = drm_mode_duplicate(connector->dev, &performence_mode_144);
	if (!mode3) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			performence_mode_144.hdisplay, performence_mode_144.vdisplay,
			drm_mode_vrefresh(&performence_mode_144));
		return -ENOMEM;
	}
	drm_mode_set_name(mode3);
	mode3->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode3);

	connector->display_info.width_mm = PHYSICAL_WIDTH/1000;
	connector->display_info.height_mm = PHYSICAL_HEIGHT/1000;
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
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct lcm *ctx;
	struct device_node *backlight;
	int ret;

	pr_info("%s o12-42-02-0a-dsc-cmd +\n", __func__);

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

	panel_ctx = ctx;
	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_NO_EOT_PACKET
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

	ctx->dvdd_gpio = devm_gpiod_get_index(dev, "dvdd", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->dvdd_gpio)) {
		dev_err(dev, "%s: cannot get dvdd_gpio 0 %ld\n",
			__func__, PTR_ERR(ctx->dvdd_gpio));
		return PTR_ERR(ctx->dvdd_gpio);
	}
	devm_gpiod_put(dev, ctx->dvdd_gpio);

	ret = lcm_panel_vci_regulator_init(dev);
	if (!ret)
		lcm_panel_vci_enable(dev);
	else
		pr_err("%s init vibr regulator error\n", __func__);

	ret = lcm_panel_vddi_regulator_init(dev);
	if (!ret)
		lcm_panel_vddi_enable(dev);
	else
		pr_err("%s init vcn18io regulator error\n", __func__);

/*
	ret = lcm_panel_dvdd_regulator_init(dev);
	if (!ret)
		lcm_panel_dvdd_enable(dev);
	else
		pr_err("%s init vbuck8 regulator error\n", __func__);
*/

	ext_params.err_flag_irq_gpio = of_get_named_gpio(
		dev->of_node, "mi,esd-err-irq-gpio",0);
	ext_params.err_flag_irq_flags = 0x2002;
	ext_params_90hz.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_90hz.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ext_params_120hz.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_120hz.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ext_params_144hz.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_144hz.err_flag_irq_flags = ext_params.err_flag_irq_flags;

	ctx->prepared = true;
	ctx->enabled = true;
	ctx->dynamic_fps = 120;
	ctx->gir_status = 1;
	ctx->peak_hdr_status = 0;
	ctx->wqhd_en = true;
	ctx->dc_status = false;
	ctx->crc_level = 0;
	ctx->panel_info = panel_name;
	ctx->panel_id = panel_id;
	ctx->max_brightness_clone = MAX_BRIGHTNESS_CLONE;
	ctx->factory_max_brightness = FACTORY_MAX_BRIGHTNESS;

	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);
	get_build_id();
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params_120hz, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif
#if defined(CONFIG_VIS_DISPLAY_DALI)
	//Novatek ASIC Notify lcm_id and tricking.
	if (is_mi_dev_support_nova()) {
		nvt_get_lcm_id_notify(nvt_get_lcm_id_42);
	}
#endif
	return ret;
}

static void lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
#endif
}

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "o12_42_02_0a_dsc_cmd,lcm", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "o12_42_02_0a_dsc_cmd,lcm",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

module_param_string(oled_lhbm, oled_lhbm_cmdline, sizeof(oled_lhbm_cmdline), 0600);
MODULE_PARM_DESC(oled_lhbm, "oled_lhbm=<local_hbm_info>");

module_param_string(oled_wp, oled_wp_cmdline, sizeof(oled_wp_cmdline), 0600);
MODULE_PARM_DESC(oled_wp, "oled_wp=<white_point_info>");

module_param_string(build_id, buildid_cmdline, sizeof(buildid_cmdline), 0600);
MODULE_PARM_DESC(build_id, "build_id=<buildid_info>");

module_param_string(panel_sn, panel_sn_cmdline, sizeof(panel_sn_cmdline), 0600);
MODULE_PARM_DESC(panel_sn, "panel_sn=<panel_sn_info>");


MODULE_AUTHOR("Tang Honghui <tanghonghui@xiaomi.com>");
MODULE_DESCRIPTION("o12_42_02_0a_dsc_cmd oled panel driver");
MODULE_LICENSE("GPL v2");
