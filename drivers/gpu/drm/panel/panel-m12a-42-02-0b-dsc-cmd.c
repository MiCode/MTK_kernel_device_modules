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
//#include "mtk_log.h"
#endif
#include <mtk_dsi.h>
//#include "../mediatek/mediatek_v2/mi_disp/mi_panel_ext.h"
//#include "mi_dsi_panel.h"
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

#include <linux/atomic.h>
//#include <uapi/drm/mi_disp.h>
//#include "mi_panel_ext.h"
//#include "mi_dsi_panel.h"
#include "include/panel-m12a-42-02-0b-dsc-cmd.h"
//#include "include/panel_o12_36_02_0b_alpha_data.h"

static int current_fps = 120;

#define SLEEP_IN_VIANO_KEEP_HBM_THRESHOLD 11000
#define MAX_BRIGHTNESS_CLONE 	16383
#define FACTORY_MAX_BRIGHTNESS 	8191
#define DDIC_DBV_PN 1
#define DDIC_FPS_PN 0

#define PANEL_NONE_APL2600  0
#define PANEL_APL2600  1
#define PANEL_APL2600_GAMMA  2
static unsigned char panel_build_id = PANEL_NONE_APL2600;
static char buildid_cmdline[4] = {0};
static char bl_tb0[] = {0x51, 0x3, 0xff};
static atomic_t doze_enable = ATOMIC_INIT(0);
static atomic_t lhbm_enable = ATOMIC_INIT(0);
static atomic_t hbm_state = ATOMIC_INIT(0);
static atomic_t gir_switch = ATOMIC_INIT(0);
static unsigned int bl_value = 0;
static int gDcThreshold = 450;
static bool gDcEnable = false;
static char oled_grayscale_cmdline[100] = {0};
static char oled_gir_cmdline[33] = {0};

//localhbm white gamma
static char oled_wp_cmdline[16] = {0};
static char oled_lhbm_cmdline[80] = {0};
//static bool lhbm_w1300_update_flag = true;
static char panel_sn_cmdline[15] = {0};
//static bool lhbm_w250_update_flag = true;
//static bool lhbm_g500_update_flag = true;
//static bool lhbm_w1300_readbackdone;
//static bool lhbm_w250_readbackdone;
//static bool lhbm_g500_readbackdone;
struct LHBM_WHITEBUF {
	unsigned char w250[GAMMA_RATIO_W250][6];
	unsigned char w1300[GAMMA_RATIO_W1300][6];
	unsigned char g500[GAMMA_RATIO_G500][6];
};

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

static unsigned int vibr_start_up = 1;
static int lcm_panel_vci_enable(struct device *dev)
{
	int ret = 0;
	int retval = 0;
	int status = 0;

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
	static int vrf18_regulator_inited;
	int ret = 0;

	if (vrf18_regulator_inited)
               return ret;

	/* please only get regulator once in a driver */
	disp_vddi = regulator_get(dev, "vrf18");
	if (IS_ERR(disp_vddi)) { /* handle return value */
		ret = PTR_ERR(disp_vddi);
		pr_err("get disp_vddi fail, error: %d\n", ret);
		return ret;
	}

	vrf18_regulator_inited = 1;
	return ret; /* must be 0 */
}

static unsigned int vrf18_start_up = 1;
static int lcm_panel_vddi_enable(struct device *dev)
{
	int ret = 0;
	int retval = 0;
	int status = 0;

	pr_info("%s +\n",__func__);

	/* set voltage with min & max*/
	ret = regulator_set_voltage(disp_vddi, 1800000, 1800000);
	if (ret < 0)
		pr_err("set voltage disp_vddi fail, ret = %d\n", ret);
	retval |= ret;

	status = regulator_is_enabled(disp_vddi);
	pr_info("%s regulator_is_enabled = %d, vrf18_start_up = %d\n", __func__, status, vrf18_start_up);
	if (!status || vrf18_start_up){
		/* enable regulator */
		ret = regulator_enable(disp_vddi);
		if (ret < 0)
			pr_err("enable regulator disp_vddi fail, ret = %d\n", ret);
		vrf18_start_up = 0;
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
	struct lcm *ctx = panel_to_lcm(panel);

	if (ctx->prepared)
		return 0;

	pr_info("%s !+\n", __func__);

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

	lcm_panel_vci_disable(ctx->dev);
	udelay(2000);

	ctx->dvdd_gpio = devm_gpiod_get_index(ctx->dev,
		"dvdd", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->dvdd_gpio)) {
		dev_err(ctx->dev, "%s: cannot get dvdd gpio %ld\n",
			__func__, PTR_ERR(ctx->dvdd_gpio));
		return PTR_ERR(ctx->dvdd_gpio);
	}
	gpiod_set_value(ctx->dvdd_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->dvdd_gpio);

	udelay(2000);
	lcm_panel_vddi_disable(ctx->dev);
	udelay(2000);
	atomic_set(&doze_enable, 0);
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

	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(5);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(125);

	ctx->error = 0;
	ctx->prepared = false;
	ctx->crc_level = 0;
	ctx->doze_suspend = false;

	return 0;
}

static int get_build_id(void) {
	static bool  is_update =false;

	if (is_update) {
		pr_info("%s: panel_build_id:%d  +\n", __func__,panel_build_id);
		return panel_build_id;
	}

	//pr_info("%s: buildid_cmdline:%s  +\n", __func__,panel_build_id);
	sscanf(buildid_cmdline, "%02hhx\n", &panel_build_id);
	is_update =true;
	return panel_build_id;
}

static void lcm_panel_init(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
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
	udelay(12 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(1 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(10 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(12 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	push_table(ctx, init_setting, sizeof(init_setting) / sizeof(struct LCM_setting_table));

	if (ctx->dynamic_fps == 120) {
		if (ctx->gir_status) {
			push_table(ctx, mode_120hz_setting_gir_on,
				sizeof(mode_120hz_setting_gir_on) / sizeof(struct LCM_setting_table));
		} else {
			push_table(ctx, mode_120hz_setting_gir_off,
				sizeof(mode_120hz_setting_gir_off) / sizeof(struct LCM_setting_table));
		}
	} else if (ctx->dynamic_fps == 60) {
		if (ctx->gir_status) {
			push_table(ctx, mode_60hz_setting_gir_on,
				sizeof(mode_60hz_setting_gir_on) / sizeof(struct LCM_setting_table));
		} else {
			push_table(ctx, mode_60hz_setting_gir_off,
				sizeof(mode_60hz_setting_gir_off) / sizeof(struct LCM_setting_table));
		}
	}


	ctx->prepared = true;
	ctx->doze_suspend = false;
err:
	pr_info("%s: -\n", __func__);
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s\n", __func__);
	if (ctx->prepared)
		return 0;

	lcm_panel_vddi_enable(ctx->dev);
	udelay(5 * 1000);
	
		/*VCAM_LDO_EN -------->  GPIO158*/
	ctx->cam_gpio = devm_gpiod_get_index(ctx->dev,
		"cam", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->cam_gpio)) {
		dev_err(ctx->dev, "%s: cannot get dvdd gpio %ld\n",
			__func__, PTR_ERR(ctx->cam_gpio));
		return PTR_ERR(ctx->cam_gpio);
	}
	gpiod_set_value(ctx->cam_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->cam_gpio);
	udelay(5 * 1000);

	ctx->dvdd_gpio = devm_gpiod_get_index(ctx->dev,
		"dvdd", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->dvdd_gpio)) {
		dev_err(ctx->dev, "%s: cannot get dvdd gpio %ld\n",
			__func__, PTR_ERR(ctx->dvdd_gpio));
		return PTR_ERR(ctx->dvdd_gpio);
	}
	gpiod_set_value(ctx->dvdd_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->dvdd_gpio);
	udelay(5 * 1000);

	lcm_panel_vci_enable(ctx->dev);
	lcm_panel_init(panel);

	ret = ctx->error;
	if (ret < 0) {
		lcm_unprepare(panel);
		lcm_panel_poweroff(panel);
	}

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
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
	.clock = 273139,
	.hdisplay = HACT_WEIGHT,
	.hsync_start = HACT_WEIGHT + MODE0_HFP,
	.hsync_end = HACT_WEIGHT + MODE0_HFP + MODE0_HSA,
	.htotal = HACT_WEIGHT + MODE0_HFP + MODE0_HSA + MODE0_HBP,
	.vdisplay = VACT_HEIGHT,
	.vsync_start = VACT_HEIGHT + MODE0_VFP,
	.vsync_end = VACT_HEIGHT + MODE0_VFP + MODE0_VSA,
	.vtotal = VACT_HEIGHT + MODE0_VFP + MODE0_VSA + MODE0_VBP,
};

static const struct drm_display_mode middle_mode = {
	.clock = 347526,
	.hdisplay = HACT_WEIGHT,
	.hsync_start = HACT_WEIGHT + MODE2_HFP,
	.hsync_end = HACT_WEIGHT + MODE2_HFP + MODE2_HSA,
	.htotal = HACT_WEIGHT + MODE2_HFP + MODE2_HSA + MODE2_HBP,
	.vdisplay = VACT_HEIGHT,
	.vsync_start = VACT_HEIGHT + MODE2_VFP,
	.vsync_end = VACT_HEIGHT + MODE2_VFP + MODE2_VSA,
	.vtotal = VACT_HEIGHT + MODE2_VFP + MODE2_VSA + MODE2_VBP,
};

static const struct drm_display_mode performence_mode = {
	.clock = 463368,
	.hdisplay = HACT_WEIGHT,
	.hsync_start = HACT_WEIGHT + MODE1_HFP,
	.hsync_end = HACT_WEIGHT + MODE1_HFP + MODE1_HSA,
	.htotal = HACT_WEIGHT + MODE1_HFP + MODE1_HSA + MODE1_HBP,
	.vdisplay = VACT_HEIGHT,
	.vsync_start = VACT_HEIGHT + MODE1_VFP,
	.vsync_end = VACT_HEIGHT + MODE1_VFP + MODE1_VSA,
	.vtotal = VACT_HEIGHT + MODE1_VFP + MODE1_VSA + MODE1_VBP,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.pll_clk = DATA_RATE0 / 2,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 69540,
	.physical_height_um = 154584,
	.cmd_null_pkt_en = 1,
	.cmd_null_pkt_len = 1000,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2712,
		.pic_width = 1220,
		.slice_height = 12,
		.slice_width = 610,
		.chunk_size = 610,
		.xmit_delay = 512,
		.dec_delay = 588,
		.scale_value = 32,
		.increment_interval = 292,
		.decrement_interval = 8,
		.line_bpg_offset = 13,
		.nfl_bpg_offset = 2421,
		.slice_bpg_offset = 1915,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = m12a_42_dphy_rc_buf_thresh,
			.range_min_qp = m12a_42_dphy_range_min_qp,
			.range_max_qp = m12a_42_dphy_range_max_qp,
			.range_bpg_ofs = m12a_42_dphy_range_bpg_ofs,
			},
		},
	.data_rate = DATA_RATE0,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_r,
#endif
};

static struct mtk_panel_params ext_params_90hz = {
	.pll_clk = DATA_RATE2 / 2,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 69540,
	.physical_height_um = 154584,
	.cmd_null_pkt_en = 1,
	.cmd_null_pkt_len = 800,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2712,
		.pic_width = 1220,
		.slice_height = 12,
		.slice_width = 610,
		.chunk_size = 610,
		.xmit_delay = 512,
		.dec_delay = 588,
		.scale_value = 32,
		.increment_interval = 292,
		.decrement_interval = 8,
		.line_bpg_offset = 13,
		.nfl_bpg_offset = 2421,
		.slice_bpg_offset = 1915,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = m12a_42_dphy_rc_buf_thresh,
			.range_min_qp = m12a_42_dphy_range_min_qp,
			.range_max_qp = m12a_42_dphy_range_max_qp,
			.range_bpg_ofs = m12a_42_dphy_range_bpg_ofs,
			},
		},
	.data_rate = DATA_RATE2,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_r,

#endif
};

static struct mtk_panel_params ext_params_120hz = {
	.pll_clk = DATA_RATE1 / 2,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 69540,
	.physical_height_um = 154584,
	.cmd_null_pkt_en = 1,
	.cmd_null_pkt_len = 250,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2712,
		.pic_width = 1220,
		.slice_height = 12,
		.slice_width = 610,
		.chunk_size = 610,
		.xmit_delay = 512,
		.dec_delay = 588,
		.scale_value = 32,
		.increment_interval = 292,
		.decrement_interval = 8,
		.line_bpg_offset = 13,
		.nfl_bpg_offset = 2421,
		.slice_bpg_offset = 1915,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = m12a_42_dphy_rc_buf_thresh,
			.range_min_qp = m12a_42_dphy_range_min_qp,
			.range_max_qp = m12a_42_dphy_range_max_qp,
			.range_bpg_ofs = m12a_42_dphy_range_bpg_ofs,
		},
	},
	.data_rate = DATA_RATE1,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_r,
#endif
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

static int panel_set_peak_hdr_status(struct mtk_dsi *dsi, int status)
{
	struct lcm *ctx = NULL;
	struct mi_dsi_panel_cfg *mi_cfg = NULL;
	//unsigned int format = FORMAT_NOLOCK;
	if (!dsi || !dsi->panel) {
		pr_err("%s: panel is NULL\n", __func__);
		goto out;
	}

	ctx = panel_to_lcm(dsi->panel);
	mi_cfg = &dsi->mi_cfg;

	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
		goto out;
	}

	if (panel_build_id == PANEL_NONE_APL2600) {
		pr_info("%s:PEAK HDR not support PANEL_NONE_APL2600\n", __func__);
		goto out;
	}

	if (ctx->peak_hdr_status == status) {
		pr_info("%s:PEAK HDR is the same, return = %d\n", __func__, status);
		goto out;
	}

	//if (status)
		//mi_disp_panel_ddic_send_cmd(peak_hdr_on, ARRAY_SIZE(peak_hdr_on), format);
	//else
		//mi_disp_panel_ddic_send_cmd(peak_hdr_off, ARRAY_SIZE(peak_hdr_off), format);

	ctx->peak_hdr_status = status;
	pr_info("%s: peak_hdr_status = %d\n", __func__, status);
out:
	return 0;
}

bool is_hbm_fod_on(struct mtk_dsi *dsi)
{
	struct mi_dsi_panel_cfg *mi_cfg = &dsi->mi_cfg;
	int feature_val;
	bool is_fod_on = false;
	feature_val = mi_cfg->feature_val[DISP_FEATURE_LOCAL_HBM];
	switch (feature_val) {
	case LOCAL_HBM_NORMAL_WHITE_1000NIT:
	case LOCAL_HBM_NORMAL_WHITE_750NIT:
	case LOCAL_HBM_NORMAL_WHITE_500NIT:
	case LOCAL_HBM_NORMAL_WHITE_110NIT:
	case LOCAL_HBM_NORMAL_GREEN_500NIT:
	case LOCAL_HBM_HLPM_WHITE_1000NIT:
	case LOCAL_HBM_HLPM_WHITE_110NIT:
		is_fod_on = true;
		break;
	default:
		break;
	}
	if (mi_cfg->feature_val[DISP_FEATURE_HBM_FOD] == FEATURE_ON) {
		is_fod_on = true;
	}
	return is_fod_on;
}
static void lcm_set_hbm_demura(void *dsi, dcs_write_gce cb,
	void *handle, bool enable)
{
	char cmd0_hbm_demura_144hz[] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00};
	char cmd1_hbm_demura_144hz[] = {0x6F, 0x2E};
	char cmd2_hbm_demura_144hz[] = {0xC0, 0x44, 0x44, 0x00, 0x00, 0x00};
	char cmd3_hbm_demura_144hz[] = {0x2F, 0x04};
	char cmd0_hbm_demura_non_144hz[] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00};
	char cmd1_hbm_demura_non_144hz[] = {0x6F, 0x2E};
	char cmd2_hbm_demura_non_144hz[] = {0xC0, 0x44, 0x44, 0x00, 0x00, 0x00};
	char cmd0_lbm_demura_144hz[] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00};
	char cmd1_lbm_demura_144hz[] = {0x6F, 0x2E};
	char cmd2_lbm_demura_144hz[] = {0xC0, 0x12, 0x34, 0x00, 0x00, 0x00};
	char cmd3_lbm_demura_144hz[] = {0x2F, 0x00};
	char cmd0_lbm_demura_non_144hz[] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00};
	char cmd1_lbm_demura_non_144hz[] = {0x6F, 0x2E};
	char cmd2_lbm_demura_non_144hz[] = {0xC0, 0x12, 0x34, 0x00, 0x00, 0x00};
	struct mtk_dsi *mtk_dsi = (struct mtk_dsi *)dsi;
	struct lcm * ctx = panel_to_lcm(mtk_dsi->panel);

	if (!ctx) {
		pr_err("ctx is null\n");
		return;
	}

	pr_info("%s: +, enable: %d fps: %d.\n", __func__, enable, ctx->dynamic_fps);
	if (enable == true) {
		if (ctx->dynamic_fps == 144) {
			cb(dsi, handle, cmd0_hbm_demura_144hz, ARRAY_SIZE(cmd0_hbm_demura_144hz));
			cb(dsi, handle, cmd1_hbm_demura_144hz, ARRAY_SIZE(cmd1_hbm_demura_144hz));
			cb(dsi, handle, cmd2_hbm_demura_144hz, ARRAY_SIZE(cmd2_hbm_demura_144hz));
			cb(dsi, handle, cmd3_hbm_demura_144hz, ARRAY_SIZE(cmd3_hbm_demura_144hz));
		} else {
			cb(dsi, handle, cmd0_hbm_demura_non_144hz, ARRAY_SIZE(cmd0_hbm_demura_non_144hz));
			cb(dsi, handle, cmd1_hbm_demura_non_144hz, ARRAY_SIZE(cmd1_hbm_demura_non_144hz));
			cb(dsi, handle, cmd2_hbm_demura_non_144hz, ARRAY_SIZE(cmd2_hbm_demura_non_144hz));
		}
	} else {
		if (ctx->dynamic_fps == 144) {
			cb(dsi, handle, cmd0_lbm_demura_144hz, ARRAY_SIZE(cmd0_lbm_demura_144hz));
			cb(dsi, handle, cmd1_lbm_demura_144hz, ARRAY_SIZE(cmd1_lbm_demura_144hz));
			cb(dsi, handle, cmd2_lbm_demura_144hz, ARRAY_SIZE(cmd2_lbm_demura_144hz));
			cb(dsi, handle, cmd3_lbm_demura_144hz, ARRAY_SIZE(cmd3_lbm_demura_144hz));
		} else {
			cb(dsi, handle, cmd0_lbm_demura_non_144hz, ARRAY_SIZE(cmd0_lbm_demura_non_144hz));
			cb(dsi, handle, cmd1_lbm_demura_non_144hz, ARRAY_SIZE(cmd1_lbm_demura_non_144hz));
			cb(dsi, handle, cmd2_lbm_demura_non_144hz, ARRAY_SIZE(cmd2_lbm_demura_non_144hz));
		}
	}

	pr_info("%s: -\n", __func__);
	return;
}
static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_tb[] = {0x51, 0x07, 0xff};
	char dimmingon_tb[] = {0x53, 0x28};
	char dimmingoff_tb[] = {0x53,0x20};
	char lhbm_off_page1[] = {0x87, 0x0F, 0xFF};
	char lhbm_off_page2[] = {0x88, 0x01};
	char lhbm_off_page3[] = {0x86, 0x01};

	struct mtk_dsi *mtk_dsi = (struct mtk_dsi *)dsi;
	struct drm_panel *panel = mtk_dsi->panel;
	struct lcm *ctx;
	static bool dimming_on = false;

	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		return -1;
	}
	ctx = panel_to_lcm(panel);

	if (level) {
		bl_tb0[1] = (level >> 8) & 0xFF;
		bl_tb0[2] = level & 0xFF;
	}

	if (gDcEnable && level < gDcThreshold && ctx->crc_level)
		level = gDcThreshold;

	bl_tb[1] = (level >> 8) & 0xFF;
	bl_tb[2] = level & 0xFF;
	if (!cb)
		return -1;
	if (atomic_read(&doze_enable)) {
		pr_info("%s: Return it when aod on, %d %d %d!\n", __func__, level, bl_tb[1], bl_tb[2]);
		return 0;
	}

	if (!mtk_dsi->mi_cfg.last_bl_level && level && is_hbm_fod_on(mtk_dsi)) {
		pr_info("lhbm off when first screen on \n");
		cb(dsi, handle, lhbm_off_page1, ARRAY_SIZE(lhbm_off_page1));
		cb(dsi, handle, lhbm_off_page2, ARRAY_SIZE(lhbm_off_page2));
		cb(dsi, handle, lhbm_off_page3, ARRAY_SIZE(lhbm_off_page3));
	}

	mtk_dsi->mi_cfg.last_bl_level = level;
	pr_info("%s %d %d %d\n", __func__, level, bl_tb[1], bl_tb[2]);
	cb(dsi, handle, bl_tb, ARRAY_SIZE(bl_tb));

	if (!dimming_on && level) {
		cb(dsi, handle, dimmingon_tb, ARRAY_SIZE(dimmingon_tb));
		dimming_on = true;
		pr_info("%s dimming status:%d, \n", __func__, dimming_on);
	} else if (dimming_on && !level) {
		cb(dsi, handle, dimmingoff_tb, ARRAY_SIZE(dimmingoff_tb));
		dimming_on = false;
		pr_info("%s dimming status:%d, \n", __func__, dimming_on);
	}

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
		{24, {0xB5, 0x8E,0x8E,0x8E,0x8E,0x8E,0x8E,0x8E,0x8E,0x8E,0x8E,0x8E,0x8E,0x8E,0x8E,0x8A,0x8A,0x00,0x00,0x80,0x80,0x6B,0x6B,0x63}},
		{2, {0x6F, 0x44}},
		{24, {0xB5, 0x8E,0x8E,0x8E,0x8E,0x8E,0x8E,0x8E,0x8E,0x8E,0x8E,0x8E,0x8E,0x8E,0x8E,0x8A,0x8A,0x00,0x00,0x80,0x80,0x6B,0x6B,0x63}},
		{6, {0xF0, 0x55,0xAA,0x52,0x08,0x01}},
		{2, {0x6F, 0x0B}},
		{24, {0xBB, 0xC8,0xC8,0xC8,0xC8,0xC8,0xC8,0xC8,0xC8,0xC8,0xC8,0xC8,0xC8,0xC8,0xC8,0xD8,0xD8,0x00,0x00,0x00,0x00,0x54,0x54,0x74}},
		{2, {0x6F, 0x29}},
		{24, {0xBB, 0xC8,0xC8,0xC8,0xC8,0xC8,0xC8,0xC8,0xC8,0xC8,0xC8,0xC8,0xC8,0xC8,0xC8,0xD8,0xD8,0x00,0x00,0x00,0x00,0x54,0x54,0x74}},
	};

	const unsigned int ELVSSBase = 0X85;
	const unsigned int AnodeOffset = 0X1EC;

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
	unsigned char AnodeOffset[] = "EC";
	unsigned char ELVSSBase[] = "85";
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
static int lcm_setbacklight_control(struct drm_panel *panel, unsigned int level)
{
	int ret = 0;
	char brightness_cmd[] = {0x51, 0x00, 0x00};

	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 1,
		.flags = 2,
		.tx_cmd_num = 1,
	};

	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
	return -1;
	}

	if (level) {
		brightness_cmd[1] = (level >> 8) & 0xFF;
		brightness_cmd[2] = level & 0xFF;
	}

	cmd_msg.type[0] = ARRAY_SIZE(brightness_cmd) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[0] = brightness_cmd;
	cmd_msg.tx_len[0] = ARRAY_SIZE(brightness_cmd);

	//ret = mtk_ddic_dsi_send_cmd(&cmd_msg, true, false);
	if (ret != 0) {
		DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
	}
	return ret;
}
static void mode_switch_to_120(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s state = %d\n", __func__, stage);
	if (stage == BEFORE_DSI_POWERDOWN) {
		if (ctx->gir_status)
			push_table(ctx, mode_120hz_setting_gir_on,
				sizeof(mode_120hz_setting_gir_on) / sizeof(struct LCM_setting_table));
		else
			push_table(ctx, mode_120hz_setting_gir_off,
				sizeof(mode_120hz_setting_gir_off) / sizeof(struct LCM_setting_table));
		ctx->dynamic_fps = 120;
	}
}

static void mode_switch_to_90(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s state = %d\n", __func__, stage);
	if (stage == BEFORE_DSI_POWERDOWN) {
		if (ctx->gir_status)
			push_table(ctx, mode_90hz_setting_gir_on,
				sizeof(mode_90hz_setting_gir_on) / sizeof(struct LCM_setting_table));
		else
			push_table(ctx, mode_90hz_setting_gir_off,
				sizeof(mode_90hz_setting_gir_off) / sizeof(struct LCM_setting_table));
		ctx->dynamic_fps = 90;
	}
}

static void mode_switch_to_60(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s state = %d\n", __func__, stage);
	if (stage == BEFORE_DSI_POWERDOWN) {
		if (ctx->gir_status)
			push_table(ctx, mode_60hz_setting_gir_on,
				sizeof(mode_60hz_setting_gir_on) / sizeof(struct LCM_setting_table));
		else
			push_table(ctx, mode_60hz_setting_gir_off,
				sizeof(mode_60hz_setting_gir_off) / sizeof(struct LCM_setting_table));
		ctx->dynamic_fps = 60;
	}
}

static int mode_switch(struct drm_panel *panel,
		struct drm_connector *connector, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	bool isFpsChange = false;

	struct drm_display_mode *m_dst = get_mode_by_id(connector, dst_mode);
	struct drm_display_mode *m_cur = get_mode_by_id(connector, cur_mode);

	if (cur_mode == dst_mode)
		return ret;

	isFpsChange = drm_mode_vrefresh(m_dst) == drm_mode_vrefresh(m_cur)? false: true;

	pr_info("%s isFpsChange = %d\n", __func__, isFpsChange);
	pr_info("%s dst_mode vrefresh = %d, vdisplay = %d, vdisplay = %d\n",
		__func__, drm_mode_vrefresh(m_dst), m_dst->vdisplay, m_dst->hdisplay);
	pr_info("%s cur_mode vrefresh = %d, vdisplay = %d, vdisplay = %d\n",
		__func__, drm_mode_vrefresh(m_cur), m_cur->vdisplay, m_cur->hdisplay);

	if (isFpsChange) {
		if (drm_mode_vrefresh(m_dst) == MODE0_FPS)
			mode_switch_to_60(panel, stage);
		else if (drm_mode_vrefresh(m_dst) == MODE2_FPS)
			mode_switch_to_90(panel, stage);
		else if (drm_mode_vrefresh(m_dst) == MODE1_FPS)
			mode_switch_to_120(panel, stage);
		else
			ret = 1;
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
	char cmd38_tb[] = {0x38, 0x00};
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

	cb(dsi, handle, cmd38_tb, ARRAY_SIZE(cmd38_tb));

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

static int panel_doze_suspend (struct drm_panel *panel, void * dsi, dcs_write_gce cb, void *handle) 
{
	struct lcm *ctx = NULL;
	char cmd2F_tb[] = {0x2F, 0x00};
	char brightnessh_tb[] = {0x51, 0x00, 0x3B, 0x00, 0x3B, 0x07, 0xFC};
	char cmd39_tb[] = {0x39, 0x00};

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

	pr_info("%s +\n", __func__);
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
		brightnessh_tb[5] = 0x0F;
		brightnessh_tb[6] = 0xFF;
	}

	cb(dsi, handle, cmd2F_tb, ARRAY_SIZE(cmd2F_tb));
	cb(dsi, handle, brightnessh_tb, ARRAY_SIZE(brightnessh_tb));
	cb(dsi, handle, cmd39_tb, ARRAY_SIZE(cmd39_tb));
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
		//ret = mi_disp_panel_ddic_send_cmd(doze_disable_t, ARRAY_SIZE(doze_disable_t), format);
		atomic_set(&doze_enable, 0);
		ctx->doze_suspend = false;
		goto exit;
	} else if (DOZE_BRIGHTNESS_LBM  == doze_brightness) {
		//ret = mi_disp_panel_ddic_send_cmd(doze_enable_l, ARRAY_SIZE(doze_enable_l), format);
		;
	} else if (DOZE_BRIGHTNESS_HBM == doze_brightness) {
		//ret = mi_disp_panel_ddic_send_cmd(doze_enable_h, ARRAY_SIZE(doze_enable_h), format);
		;
	}

#else

	if (DOZE_TO_NORMAL == doze_brightness) {
		// ret = mi_disp_panel_ddic_send_cmd(backlight_0, ARRAY_SIZE(backlight_0), format);
		atomic_set(&doze_enable, 0);
	} else if (DOZE_BRIGHTNESS_LBM  == doze_brightness) {
		//ret = mi_disp_panel_ddic_send_cmd(backlight_l, ARRAY_SIZE(backlight_l), format);
		;
	} else if (DOZE_BRIGHTNESS_HBM == doze_brightness) {
		//ret = mi_disp_panel_ddic_send_cmd(backlight_h, ARRAY_SIZE(backlight_h), format);
		;
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
		goto err;
	}

	mi_disp_panel_ddic_send_cmd(gir_on_settings, ARRAY_SIZE(gir_on_settings), format);

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
		goto err;
	}

	mi_disp_panel_ddic_send_cmd(gir_off_settings, ARRAY_SIZE(gir_off_settings), format);

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
	//struct mi_dsi_panel_cfg *mi_cfg = NULL;
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

	//mi_cfg = &dsi->mi_cfg;
	//bl_level = mi_cfg->last_no_zero_bl_level;
	if (atomic_read(&doze_enable)) {
		//if (mi_cfg->last_bl_level)
			//bl_level = mi_cfg->last_bl_level;
		//else
			//bl_level = 0;
		;
	} else {
		pr_err("%s, only restore from doze\n", __func__);
		return;
	}

	restore_backlight_level[0].para_list[0] = (bl_level >> 8) & 0xFF;
	restore_backlight_level[0].para_list[1] = bl_level & 0xFF;

	//mi_disp_panel_ddic_send_cmd(restore_backlight_level, ARRAY_SIZE(restore_backlight_level), format);

	//pr_info("%s setbacklight %d, doze_enabled: %d last_backlight: %d %d \n", __func__, bl_level, atomic_read(&doze_enable), mi_cfg->last_bl_level, mi_cfg->last_no_zero_bl_level);
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
	int i = 0, temp = 0, j = 0;
	static u16 lhbm_cmdbuf[9] = {0};

	pr_info("mi_parse_cmdline_perBL enter\n");

	if(!lhbm_w1300_update_flag && !lhbm_w250_update_flag && !lhbm_g500_update_flag) {
		pr_info("don't need update white rgb config");
		return;
	}

	if (lhbm_whitebuf == NULL) {
		pr_err("lhbm_status == NULL\n");
		return;
	}
	for (i = 0; i < 9; i++) {
		sscanf(oled_lhbm_cmdline + 4 * i, "%04hx", &lhbm_cmdbuf[i]);
	}

	for (i = 0; i < 6; i += 2) {
		// 250nit
		for (j = 0; j < GAMMA_RATIO_W250; j++) {
			temp = ((int)lhbm_cmdbuf[i/2]  * gamma_ratio_w250[j][i/2]) / 10000;
			lhbm_whitebuf->w250[j][i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->w250[j][i+1] = temp & 0xFF;
		}

		// 1300nit
		for (j = 0; j < GAMMA_RATIO_W1300; j++) {
			temp = ((int)lhbm_cmdbuf[i/2 + 3]  * gamma_ratio_w1300[j][i/2]) / 10000;
			lhbm_whitebuf->w1300[j][i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->w1300[j][i+1] = temp & 0xFF;
		}

		// 500nit
		for (j = 0; j < GAMMA_RATIO_G500; j++) {
			temp = ((int)lhbm_cmdbuf[i/2 + 6]  * gamma_ratio_g500[j][i/2]) / 10000;
			lhbm_whitebuf->g500[j][i] = (temp & 0xFF00) >> 8;
			lhbm_whitebuf->g500[j][i+1] = temp & 0xFF;
		}
	}

	pr_info("lhbm w250 \n");
	for (j = 0; j < GAMMA_RATIO_W250; j++){
		pr_info("%d: 0x%02hhx 0x%02hhx 0x%02hhx 0x%02hhx 0x%02hhx 0x%02hhx", j, lhbm_whitebuf->w250[j][0], lhbm_whitebuf->w250[j][1],
			lhbm_whitebuf->w250[j][2],lhbm_whitebuf->w250[j][3],lhbm_whitebuf->w250[j][4],lhbm_whitebuf->w250[j][5]);
	}

	pr_info("lhbm w1300 \n");
	for (j = 0; j < GAMMA_RATIO_W1300; j++){
		pr_info("%d: 0x%02hhx 0x%02hhx 0x%02hhx 0x%02hhx 0x%02hhx 0x%02hhx", j, lhbm_whitebuf->w1300[j][0], lhbm_whitebuf->w1300[j][1],
			lhbm_whitebuf->w1300[j][2],lhbm_whitebuf->w1300[j][3],lhbm_whitebuf->w1300[j][4],lhbm_whitebuf->w1300[j][5]);
	}

	pr_info("lhbm g500 \n");
	for (j = 0; j < GAMMA_RATIO_G500; j++){
		pr_info("%d: 0x%02hhx 0x%02hhx 0x%02hhx 0x%02hhx 0x%02hhx 0x%02hhx", j, lhbm_whitebuf->g500[j][0], lhbm_whitebuf->g500[j][1],
			lhbm_whitebuf->g500[j][2],lhbm_whitebuf->g500[j][3],lhbm_whitebuf->g500[j][4],lhbm_whitebuf->g500[j][5]);
	}

	lhbm_w1300_readbackdone = true;
	lhbm_w250_readbackdone = true;
	lhbm_g500_readbackdone = true;
	lhbm_w1300_update_flag = false;
	lhbm_w250_update_flag = false;
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
	unsigned int format = FORMAT_LP_MODE | FORMAT_BLOCK;
	static int last_bl_level = 0;
	if(!dsi) {
		pr_err("dsi is null\n");
		return -EINVAL;
	}

	if (last_bl_level>= LHBM_BL_INTERVAL_2_START && last_bl_level <= LHBM_BL_INTERVAL_2_END && type == TYPE_LHBM_OFF) {
		lhbm_off_bl_interval_2[0].para_list[0] = (bl_level >> 8) & 0xFF;
		lhbm_off_bl_interval_2[0].para_list[1] = bl_level & 0xFF;
		mi_disp_panel_ddic_send_cmd(lhbm_off_bl_interval_2, ARRAY_SIZE(lhbm_off_bl_interval_2), format);
		return 0;
	} else if (type == TYPE_LHBM_OFF) {
		lhbm_off_bl_interval_1[0].para_list[0] = (bl_level >> 8) & 0xFF;
		lhbm_off_bl_interval_1[0].para_list[1] = bl_level & 0xFF;
		mi_disp_panel_ddic_send_cmd(lhbm_off_bl_interval_1, ARRAY_SIZE(lhbm_off_bl_interval_1), format);
		return 0;
	}
	last_bl_level = bl_level;

	if (bl_level < LHBM_BL_INTERVAL_1_START)
		bl_level = LHBM_BL_INTERVAL_1_START;
	else if (bl_level > LHBM_BL_INTERVAL_3_END)
		bl_level = LHBM_BL_INTERVAL_3_END;

	if (bl_level >= LHBM_BL_INTERVAL_1_START && bl_level <= LHBM_BL_INTERVAL_1_END) { //2nit~80nit
		switch (type) {
			case TYPE_WHITE_1300:
			case TYPE_GREEN_500
:
			case TYPE_WHITE_250:
				mi_disp_panel_ddic_send_cmd(&lhbm_on_bl_interval_1[aod_off_cmd_line],
								ARRAY_SIZE(lhbm_on_bl_interval_1) - aod_off_cmd_line, format);
			break;
			case TYPE_HLPM_W1300:
			case TYPE_HLPM_W250:
				mi_disp_panel_ddic_send_cmd(lhbm_on_bl_interval_1, ARRAY_SIZE(lhbm_on_bl_interval_1), format);
			break;
			default:
				pr_err("unsuppport cmd \n");
			return -EINVAL;
		}
	 } else if (bl_level >= LHBM_BL_INTERVAL_2_START && bl_level <= LHBM_BL_INTERVAL_2_END) { //80nit~700nit
		switch (type) {
			case TYPE_WHITE_1300:
			case TYPE_GREEN_500:
			case TYPE_WHITE_250:
				mi_disp_panel_ddic_send_cmd(&lhbm_on_bl_interval_2[aod_off_cmd_line],
								ARRAY_SIZE(lhbm_on_bl_interval_2) - aod_off_cmd_line, format);
			break;
			case TYPE_HLPM_W1300:
			case TYPE_HLPM_W250:
				mi_disp_panel_ddic_send_cmd(lhbm_on_bl_interval_2, ARRAY_SIZE(lhbm_on_bl_interval_2), format);
			break;
			default:
				pr_err("unsuppport cmd \n");
			return -EINVAL;
		}
	} else if (bl_level >= LHBM_BL_INTERVAL_3_START && bl_level <= LHBM_BL_INTERVAL_3_END) { //700nit ~ 1600nit
		switch (type) {
			case TYPE_WHITE_1300:
			case TYPE_GREEN_500:
			case TYPE_WHITE_250:
				mi_disp_panel_ddic_send_cmd(&lhbm_on_bl_interval_3[aod_off_cmd_line],
								ARRAY_SIZE(lhbm_on_bl_interval_3) - aod_off_cmd_line, format);
			break;
			case TYPE_HLPM_W1300:
			case TYPE_HLPM_W250:
				mi_disp_panel_ddic_send_cmd(lhbm_on_bl_interval_3, ARRAY_SIZE(lhbm_on_bl_interval_3), format);
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
	int i = 0, j = 0;
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

	if(!lhbm_w1300_readbackdone ||
		 !lhbm_w250_readbackdone ||
		 !lhbm_g500_readbackdone) {
		pr_info("mi_disp_panel_update_lhbm_white_param cmdline_lhbm:%s\n", oled_lhbm_cmdline);

		mi_parse_cmdline_perBL(&lhbm_whitebuf);
	}

	if (bl_level < LHBM_BL_INTERVAL_1_START)
		bl_level = LHBM_BL_INTERVAL_1_START;
	else if (bl_level > LHBM_BL_INTERVAL_3_END)
		bl_level = LHBM_BL_INTERVAL_3_END;

	pr_info("lhbm update 0xD1, lhbm_cmd_type:%d backlight:%d \n", type, bl_level);
	if (bl_level >= LHBM_BL_INTERVAL_1_START && bl_level <= LHBM_BL_INTERVAL_1_END) { //2nit~80nit
		 switch (type) {
			 case TYPE_WHITE_1300:
			 case TYPE_HLPM_W1300:
				for (j = 0; j < GAMMA_RATIO_W1300; j++){
					if (bl_level <= gamma_ratio_w1300_inverval[j]) break;
				}
				for (i = 0; i < 6 && j < GAMMA_RATIO_W1300; i++)
					lhbm_on_bl_interval_1[lhbm_on_interval_1_gamma_index].para_list[i] = lhbm_whitebuf.w1300[j][i];
			 break;
			 case TYPE_WHITE_250:
			 case TYPE_HLPM_W250:
			 	for (j = 0; j < GAMMA_RATIO_W250; j++){
					if (bl_level <= gamma_ratio_w250_inverval[j]) break;
				}
				for (i = 0; i < 6 && j < GAMMA_RATIO_W250; i++)
					lhbm_on_bl_interval_1[lhbm_on_interval_1_gamma_index].para_list[i] = lhbm_whitebuf.w250[j][i];
			 break;
			 case TYPE_GREEN_500:
			 	for (j = 0; j < GAMMA_RATIO_G500; j++){
					if (bl_level <= gamma_ratio_g500_inverval[j]) break;
				}
				for (i = 0; i < 6 && j < GAMMA_RATIO_G500; i++)
					lhbm_on_bl_interval_1[lhbm_on_interval_1_gamma_index].para_list[i] = lhbm_whitebuf.g500[j][i];
			 break;
			 default:
				 pr_err("unsuppport cmd \n");
		 }
	 } else if (bl_level >= LHBM_BL_INTERVAL_2_START && bl_level <= LHBM_BL_INTERVAL_2_END) { //80nit~700nit
		 switch (type) {
			 case TYPE_WHITE_1300:
			 case TYPE_HLPM_W1300:
			 	for (j = 0; j < GAMMA_RATIO_W1300; j++){
					if (bl_level <= gamma_ratio_w1300_inverval[j]) break;
				}
				for (i = 0; i < 6 && j < GAMMA_RATIO_W1300; i++)
					lhbm_on_bl_interval_2[lhbm_on_interval_2_gamma_index].para_list[i] = lhbm_whitebuf.w1300[j][i];
			 break;
			 case TYPE_WHITE_250:
			 case TYPE_HLPM_W250:
			 	for (j = 0; j < GAMMA_RATIO_W250; j++){
					if (bl_level <= gamma_ratio_w250_inverval[j]) break;
				}
				for (i = 0; i < 6 && j < GAMMA_RATIO_W250; i++)
					lhbm_on_bl_interval_2[lhbm_on_interval_2_gamma_index].para_list[i] = lhbm_whitebuf.w250[j][i];
			 break;
			 case TYPE_GREEN_500:
			 	for (j = 0; j < GAMMA_RATIO_G500; j++){
					if (bl_level <= gamma_ratio_g500_inverval[j]) break;
				}
				for (i = 0; i < 6 && j < GAMMA_RATIO_G500; i++)
					lhbm_on_bl_interval_2[lhbm_on_interval_2_gamma_index].para_list[i] = lhbm_whitebuf.g500[j][i];
			 break;
			 default:
				 pr_err("unsuppport cmd \n");
		 }
	 } else if (bl_level >= LHBM_BL_INTERVAL_3_START && bl_level <= LHBM_BL_INTERVAL_3_END) { //700nit ~ 1600nit
		 switch (type) {
			 case TYPE_WHITE_1300:
			 case TYPE_HLPM_W1300:
			 	for (j = 0; j < GAMMA_RATIO_W1300; j++){
					if (bl_level <= gamma_ratio_w1300_inverval[j]) break;
				}
				 for (i = 0; i < 6 && j < GAMMA_RATIO_W1300; i++)
					 lhbm_on_bl_interval_3[lhbm_on_interval_3_gamma_index].para_list[i] = lhbm_whitebuf.w1300[j][i];
			 break;
			 case TYPE_WHITE_250:
			 case TYPE_HLPM_W250:
			 	for (j = 0; j < GAMMA_RATIO_W250; j++){
					if (bl_level <= gamma_ratio_w250_inverval[j]) break;
				}
				 for (i = 0; i < 6 && j < GAMMA_RATIO_W250; i++)
					 lhbm_on_bl_interval_3[lhbm_on_interval_3_gamma_index].para_list[i] = lhbm_whitebuf.w250[j][i];
			 break;
			 case TYPE_GREEN_500:
			 	for (j = 0; j < GAMMA_RATIO_G500; j++){
					if (bl_level <= gamma_ratio_g500_inverval[j]) break;
				}
				 for (i = 0; i < 6 && j < GAMMA_RATIO_G500; i++)
					 lhbm_on_bl_interval_3[lhbm_on_interval_3_gamma_index].para_list[i] = lhbm_whitebuf.g500[j][i];
			 break;
			 default:
				 pr_err("unsuppport cmd \n");
		 }
	 } else {
		 pr_info("Error--lhbm_cmd_type:%d , %d backlight is Out of range \n", type, bl_level);
	 }
	 return -1;
 }

static void mi_disp_panel_update_lhbm_alpha(struct mtk_dsi *dsi,enum lhbm_cmd_type type , int bl_level) {
	u8 alpha_buf[2] = {0};

	if (!dsi || type >= TYPE_MAX) {
		pr_err("invalid params\n");
		return;
	}

	pr_info("%s [%d] bl_lvl = %d,\n", __func__, type, bl_level);

	if (bl_level < LHBM_BL_INTERVAL_1_START)
		bl_level = LHBM_BL_INTERVAL_1_START;
	else if (bl_level > LHBM_BL_INTERVAL_3_END)
		bl_level = LHBM_BL_INTERVAL_3_END;

	if (bl_level >= LHBM_BL_INTERVAL_1_START && bl_level <= LHBM_BL_INTERVAL_1_END) { //2nit~80nit
		alpha_buf[0] = (aa_alpha_set_80_2nit[bl_level] >> 8) & 0xFF;
		alpha_buf[1] = aa_alpha_set_80_2nit[bl_level] & 0xFF;
		lhbm_on_bl_interval_1[lhbm_on_interval_1_alpha_index].para_list[1] = alpha_buf[0];
		lhbm_on_bl_interval_1[lhbm_on_interval_1_alpha_index].para_list[2] = alpha_buf[1];
	} else if (bl_level >= LHBM_BL_INTERVAL_2_START && bl_level <= LHBM_BL_INTERVAL_2_END) { //80nit ~ 700nit
		lhbm_on_bl_interval_2[lhbm_on_interval_2_51_index].para_list[0] = (bl_level >> 8) & 0xFF;
		lhbm_on_bl_interval_2[lhbm_on_interval_2_51_index].para_list[1] = bl_level & 0xFF;
	} else if (bl_level >= LHBM_BL_INTERVAL_3_START && bl_level <= LHBM_BL_INTERVAL_3_END) { //700nit ~ 1600nit
		alpha_buf[0] = (aa_alpha_set_1600_700nit[bl_level - LHBM_BL_INTERVAL_3_START] >> 8) & 0xFF;
		alpha_buf[1] = aa_alpha_set_1600_700nit[bl_level - LHBM_BL_INTERVAL_3_START] & 0xFF;
		lhbm_on_bl_interval_3[lhbm_on_interval_3_alpha_index].para_list[1] = alpha_buf[0];
		lhbm_on_bl_interval_3[lhbm_on_interval_3_alpha_index].para_list[2] = alpha_buf[1];
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
	int type = TYPE_WHITE_1300;
	bool send_lhbm_cmd = false;

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

	pr_info("%s local hbm_state :%d bl_level:%d  flat_mode:%d\n", __func__, lhbm_state, bl_level, flat_mode);

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
		pr_info("LOCAL_HBM_NORMAL_WHITE_250NIT\n");
		if (atomic_read(&doze_enable)) {
			bl_level = bl_level_doze;
			type = TYPE_HLPM_W250;
		} else {
			type = TYPE_WHITE_250;
		}
		send_lhbm_cmd = true;
		break;
	case LOCAL_HBM_NORMAL_GREEN_500NIT:
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		pr_info("LOCAL_HBM_NORMAL_GREEN_500NIT\n");
		type = TYPE_GREEN_500;
		send_lhbm_cmd = true;
		break;
	case LOCAL_HBM_NORMAL_WHITE_1000NIT:
		pr_info("LOCAL_HBM_NORMAL_WHITE_1300NIT in HBM\n");
		if (atomic_read(&doze_enable)) {
			type = TYPE_HLPM_W1300;
			bl_level = bl_level_doze;
		} else {
			type = TYPE_WHITE_1300;
		}
		send_lhbm_cmd = true;
		break;
	case LOCAL_HBM_HLPM_WHITE_1000NIT:
		pr_info("LOCAL_HBM_HLPM_WHITE_1300NIT in HBM\n");
		if (atomic_read(&doze_enable) || !bl_level) {
			type = TYPE_HLPM_W1300;
			bl_level = bl_level_doze;
		} else {
			type = TYPE_HLPM_W1300;
		}
		send_lhbm_cmd = true;
		break;
	case LOCAL_HBM_HLPM_WHITE_110NIT:
		pr_info("LOCAL_HBM_HLPM_WHITE_250NIT\n");
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		if (atomic_read(&doze_enable) || !bl_level) {
			bl_level = bl_level_doze;
			type = TYPE_HLPM_W250;
		} else {
			type = TYPE_HLPM_W250;
		}
		send_lhbm_cmd = true;
		break;
	default:
		pr_info("invalid local hbm value\n");
		break;
	}

	if (send_lhbm_cmd) {
		mi_disp_panel_update_lhbm_alpha(dsi, type, bl_level);
		mi_disp_panel_update_lhbm_white_param(dsi, type, bl_level);
		mi_disp_panel_send_lhbm(dsi, type, bl_level);
		ctx->lhbm_en = true;
		ctx->doze_suspend = false;
	}

	return 0;
}

static int panel_fod_state_check (void * dsi, dcs_write_gce cb, void *handle)
{
	struct lcm *ctx = NULL;
	struct mtk_dsi *mtk_dsi = (struct mtk_dsi *)dsi;
	struct mi_dsi_panel_cfg *mi_cfg = NULL;

	if (!mtk_dsi || !mtk_dsi->panel) {
		pr_err("dsi is null\n");
		return -1;
	}

	mi_cfg = &mtk_dsi->mi_cfg;
	if (!mi_cfg) {
		pr_err("mi_cfg is null\n");
		return -1;
	}

	ctx = panel_to_lcm(mtk_dsi->panel);

	if (ctx->lhbm_en) {
		/*char lhbm_off_page[] = {0x87,0x24};
		if (mi_cfg->last_no_zero_bl_level >= LHBM_BL_INTERVAL_2_START && 
			mi_cfg->last_no_zero_bl_level <= LHBM_BL_INTERVAL_2_END) //80nit~700nit
			lhbm_off_page[1] = 0x04;
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
	.setbacklight_control = lcm_setbacklight_control,
	//.set_bl_elvss_cmdq = lcm_set_bl_elvss_cmdq,
	.mode_switch = mode_switch,
	.panel_poweron = lcm_panel_poweron,
	.panel_poweroff = lcm_panel_poweroff,
	.doze_enable = panel_doze_enable,
	.doze_disable = panel_doze_disable,
	//.read_elvss_base_voltage = lcm_read_elvss_base_voltage,
	.lcm_update_roi_cmdq = lcm_update_roi_cmdq,
#ifdef CONFIG_MI_DISP
#ifdef CONFIG_MI_DISP_ESD_CHECK
	.esd_restore_backlight = lcm_esd_restore_backlight,
#endif
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
	struct drm_display_mode *mode0, *mode1, *mode2;

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

	connector->display_info.width_mm = 71;
	connector->display_info.height_mm = 153;

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

	pr_info("%s m12a-42-02-0b-dsc +\n", __func__);

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
	if (!ctx) {
		pr_info("%s  -ENOMEM\n", __func__);
		return -ENOMEM;
	}
	pr_info("%s  ---1\n", __func__);
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

		if (!ctx->backlight) {
			pr_info("%s ctx->backlight is null\n", __func__);
			return -EPROBE_DEFER;
		}
	}
	pr_info("%s  ---2\n", __func__);
	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);
	pr_info("%s  ---3\n", __func__);
	ctx->dvdd_gpio = devm_gpiod_get_index(dev, "dvdd", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->dvdd_gpio)) {
		dev_err(dev, "%s: cannot get dvdd_gpio 0 %ld\n",
			__func__, PTR_ERR(ctx->dvdd_gpio));
		return PTR_ERR(ctx->dvdd_gpio);
	}
	pr_info("%s  ---4\n", __func__);
	devm_gpiod_put(dev, ctx->dvdd_gpio);

	ret = lcm_panel_vci_regulator_init(dev);
	if (!ret)
		lcm_panel_vci_enable(dev);
	else
		pr_err("%s init vibr regulator error\n", __func__);
	pr_info("%s  ---5\n", __func__);
	ret = lcm_panel_vddi_regulator_init(dev);
	if (!ret)
		lcm_panel_vddi_enable(dev);
	else
		pr_err("%s init vrf18_aif regulator error\n", __func__);
	pr_info("%s  ---6\n", __func__);

	ext_params.err_flag_irq_gpio = of_get_named_gpio(
		dev->of_node, "mi,esd-err-irq-gpio",0);
	ext_params.err_flag_irq_flags = 0x2002;
	ext_params_120hz.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_120hz.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	pr_info("%s  ---7\n", __func__);
	ctx->prepared = true;
	ctx->enabled = true;
	ctx->panel_info = panel_name;
	ctx->dynamic_fps = 60;
	ctx->wqhd_en = true;
	ctx->max_brightness_clone = MAX_BRIGHTNESS_CLONE;
	ctx->spr_status = SPR_2D_RENDERING;
	ctx->dc_status = false;
	ctx->crc_level = 0;
	ctx->peak_hdr_status = 0;

	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);
	pr_info("%s  ---8\n", __func__);
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params_120hz, &ext_funcs, &ctx->panel);
	pr_info("%s  ---9\n", __func__);
	if (ret < 0)
		return ret;
#endif
	pr_info("%s -\n", __func__);
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
	{ .compatible = "m12a_42_02_0b_dsc_cmd,lcm", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "m12a_42_02_0b_dsc_cmd,lcm",
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
MODULE_DESCRIPTION("m12a_42_02_0b_dsc_cmd oled panel driver");
MODULE_LICENSE("GPL v2");
