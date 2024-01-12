// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

/* Attention
 *  1."FPS_INIT_INDEX" is related to switch frame rate,when modifying the initial code should modify it.
 *  owner: chenyuan8@xiaomi.com
 *  date: 2023/09/11
 *  changeId: 3341299
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
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#include "../mediatek/mediatek_v2/mtk_log.h"
#endif
//#include "../mediatek/mediatek_v2/mi_disp/mi_dsi_panel_count.h"
#include "../mediatek/mediatek_v2/mi_disp/mi_panel_ext.h"
#include "../mediatek/mediatek_v2/mi_disp/mi_dsi_panel.h"
#include "../mediatek/mediatek_v2/mtk_dsi.h"

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif
#include "include/panel_n11a_42_02_0a_alpha_data.h"
#include "include/panel-n11a-42-02-0a-dsc-vdo.h"

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
	ssize_t ret;

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

static struct regulator *disp_vci;
static struct regulator *disp_vddi;

static int lcm_panel_vci_regulator_init(struct device *dev)
{
	static int vibr_regulator_inited;
	int ret = 0;

	if (vibr_regulator_inited)
		return ret;

	/* please only get regulator once in a driver */
	disp_vci = regulator_get(dev, "vibr30");
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
	static int vio18_regulator_inited;
	int ret = 0;

	if (vio18_regulator_inited)
		return ret;

	/* please only get regulator once in a driver */
	disp_vddi = regulator_get(dev, "vio18");
	if (IS_ERR(disp_vddi)) { /* handle return value */
		ret = PTR_ERR(disp_vddi);
		disp_vddi = NULL;
		pr_err("get disp_vddi fail, error: %d\n", ret);
		return ret;
	}

	vio18_regulator_inited = 1;
	return ret; /* must be 0 */
}

static unsigned int vio18_start_up = 1;
static int lcm_panel_vddi_enable(struct device *dev)
{
	int ret = 0;
	int retval = 0;
	int status = 0;

	pr_info("%s +\n",__func__);
	if (disp_vddi == NULL)
		return retval;

	/* set voltage with min & max*/
	ret = regulator_set_voltage(disp_vddi, 1800000, 1800000);
	if (ret < 0)
		pr_err("set voltage disp_vddi fail, ret = %d\n", ret);
	retval |= ret;

	status = regulator_is_enabled(disp_vddi);
	pr_info("%s regulator_is_enabled = %d, vio18_start_up = %d\n", __func__, status, vio18_start_up);
	if (!status || vio18_start_up){
		/* enable regulator */
		ret = regulator_enable(disp_vddi);
		if (ret < 0)
			pr_err("enable regulator disp_vddi fail, ret = %d\n", ret);
		vio18_start_up = 0;
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
	if (disp_vddi == NULL)
		return retval;

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
	static int time = 1;

	if (!ctx->prepared && time > 2)
		return 0;

	pr_info("%s\n", __func__);

	mutex_lock(&ctx->panel_lock);
	if (time == 1) {
		push_table(ctx, lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table));
	} else {
		push_table(ctx, lcm_suspend_setting2, sizeof(lcm_suspend_setting2) / sizeof(struct LCM_setting_table));
	}
	++time;
	if (time == 3)
		time = 1;
	mutex_unlock(&ctx->panel_lock);

	ctx->error = 0;
	ctx->prepared = false;

	atomic_set(&doze_enable, 0);

	return 0;
}

static void lcm_panel_init(struct lcm *ctx)
{
	pr_info("%s+\n", __func__);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return;
	}

	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, (10 * 1000)+20);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(1 * 1000, (1* 1000)+20);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(11 * 1000, (11 * 1000)+20);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	pr_info("%s: dynamic_fps:%d, gir_status:%d\n", __func__,
		ctx->dynamic_fps, ctx->gir_status);

	if (ctx->dynamic_fps == 120  || ctx->dynamic_fps == 30) {
		if (init_setting_vdo[FPS_INIT_INDEX].cmd == 0x2F)
			init_setting_vdo[FPS_INIT_INDEX].para_list[0] = 0x00;
		else
			pr_info("%s: please check FPS_INIT_INDEX\n", __func__);
	} else if (ctx->dynamic_fps == 90) {
		if (init_setting_vdo[FPS_INIT_INDEX].cmd == 0x2F)
			init_setting_vdo[FPS_INIT_INDEX].para_list[0] = 0x01;
		else
			pr_info("%s: please check FPS_INIT_INDEX\n", __func__);
	} else if (ctx->dynamic_fps == 60) {
		if (init_setting_vdo[FPS_INIT_INDEX].cmd == 0x2F)
			init_setting_vdo[FPS_INIT_INDEX].para_list[0] = 0x02;
		else
		pr_info("%s: please check FPS_INIT_INDEX\n", __func__);
	}

	if (ctx->gray_level == TEMP_INDEX1_36) {
		if (init_setting_vdo[TEMP_INDEX].cmd == 0x57){
			init_setting_vdo[TEMP_INDEX].para_list[0] = 0x82;
                  pr_info("%s+, restore gray_level to TEMP_INDEX1_36 \n", __func__);
		}
	} else if (ctx->gray_level == TEMP_INDEX2_32) {
		if (init_setting_vdo[TEMP_INDEX].cmd == 0x57){
			init_setting_vdo[TEMP_INDEX].para_list[0] = 0x81;
                pr_info("%s+, restore gray_level to TEMP_INDEX2_32 \n", __func__);
                }
	} else if (ctx->gray_level == TEMP_INDEX3_28) {
		if (init_setting_vdo[TEMP_INDEX].cmd == 0x57){
			init_setting_vdo[TEMP_INDEX].para_list[0] = 0x80;
                 pr_info("%s+, restore gray_level to TEMP_INDEX3_28 \n", __func__);
                }
	} else if (ctx->gray_level == TEMP_INDEX4_off) {
		if (init_setting_vdo[TEMP_INDEX].cmd == 0x57)
			init_setting_vdo[TEMP_INDEX].para_list[0] = 0x00;
	}

	push_table(ctx, init_setting_vdo, sizeof(init_setting_vdo) / sizeof(struct LCM_setting_table));
	pr_info("%s-\n", __func__);
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

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
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

	return ret;
}

static int lcm_panel_poweroff(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	if (ctx->prepared)
		return 0;

	ctx->reset_gpio = devm_gpiod_get_index(ctx->dev,
		"reset", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	udelay(1000);

	//VCI 3.0V -> 0
	lcm_panel_vci_disable(ctx->dev);
	udelay(1000);

	//DVDD 1.2V -> 0
	ctx->dvdd_gpio = devm_gpiod_get_index(ctx->dev,
		"dvdd", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->dvdd_gpio)) {
		dev_err(ctx->dev, "%s: cannot get dvdd gpio %ld\n",
			__func__, PTR_ERR(ctx->dvdd_gpio));
		return PTR_ERR(ctx->dvdd_gpio);
	}
	gpiod_set_value(ctx->dvdd_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->dvdd_gpio);
	udelay(1000);

	//VDDI 1.8V -> 0
	lcm_panel_vddi_disable(ctx->dev);
	udelay(1000);


	return 0;
}

static int lcm_panel_poweron(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (ctx->prepared)
		return 0;

	pr_info("%s\n", __func__);

	//VDDI 1.8V
	lcm_panel_vddi_enable(ctx->dev);
	udelay(1000);

	//VDD 1.2V
	ctx->dvdd_gpio = devm_gpiod_get_index(ctx->dev,
		"dvdd", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->dvdd_gpio)) {
		dev_err(ctx->dev, "%s: cannot get dvdd gpio %ld\n",
			__func__, PTR_ERR(ctx->dvdd_gpio));
		return PTR_ERR(ctx->dvdd_gpio);
	}
	gpiod_set_value(ctx->dvdd_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->dvdd_gpio);
	udelay(1000);

	//VCI 3.0V
	lcm_panel_vci_enable(ctx->dev);
	udelay(12*1000);

	return 0;
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

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params_30hz = {
	.lcm_index = 1,
	.pll_clk = DATA_RATE / 2,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9C,
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
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 30,
		.cmds_counts_switch_fps = 30,
		.short_dfps_cmds_counts = 2,
		.short_dfps_cmds_start_index = 0,
		.long_dfps_cmds_counts = 2,
		.dfps_cmd_table[0] = {0, 3, {0x5F, 0x00, 0x40} },
		.dfps_cmd_table[1] = {0, 1, {0x39} },
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.prefetch_time = PREFETCH_TIME,
};

static struct mtk_panel_params ext_params_60hz = {
	.lcm_index = 1,
	.pll_clk = DATA_RATE / 2,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
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
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 60,
		.cmds_counts_switch_fps = 30,
		.short_dfps_cmds_counts = 1,
		.short_dfps_cmds_start_index = 4,
		.long_dfps_cmds_counts = 5,
		.dfps_cmd_table[0] = {0, 6, {0xF0,0x55,0xAA,0x52,0x08,0x03} },
		.dfps_cmd_table[1] = {0, 2, {0x6F,0x03} },
		.dfps_cmd_table[2] = {0, 2, {0xBA,0x00} },
		.dfps_cmd_table[3] = {0, 1, {0x38} },
		.dfps_cmd_table[4] = {0, 2, {0x2F, 0x02} },
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.prefetch_time = PREFETCH_TIME,
};

static struct mtk_panel_params ext_params_90hz = {
	.lcm_index = 1,
	.pll_clk = DATA_RATE / 2,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
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
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 90,
		.cmds_counts_switch_fps = 30,
		.short_dfps_cmds_counts = 1,
		.short_dfps_cmds_start_index = 4,
		.long_dfps_cmds_counts = 5,
		.dfps_cmd_table[0] = {0, 6, {0xF0,0x55,0xAA,0x52,0x08,0x03} },
		.dfps_cmd_table[1] = {0, 2, {0x6F,0x03} },
		.dfps_cmd_table[2] = {0, 2, {0xBA,0x00} },
		.dfps_cmd_table[3] = {0, 1, {0x38} },
		.dfps_cmd_table[4] = {0, 2, {0x2F, 0x01} },
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.prefetch_time = PREFETCH_TIME,
};

static struct mtk_panel_params ext_params_120hz = {
	.lcm_index = 0,
	.pll_clk = DATA_RATE / 2,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
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
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 120,
		.cmds_counts_switch_fps = 30,
		.short_dfps_cmds_counts = 1,
		.short_dfps_cmds_start_index = 4,
		.long_dfps_cmds_counts = 5,
		.dfps_cmd_table[0] = {0, 6, {0xF0,0x55,0xAA,0x52,0x08,0x03} },
		.dfps_cmd_table[1] = {0, 2, {0x6F,0x03} },
		.dfps_cmd_table[2] = {0, 2, {0xBA,0x00} },
		.dfps_cmd_table[3] = {0, 1, {0x38} },
		.dfps_cmd_table[4] = {0, 2, {0x2F, 0x00} },
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.prefetch_time = PREFETCH_TIME,
};

struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
	unsigned int mode)
{
	struct drm_display_mode *m = NULL;
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

	if (dst_fps == 30)
		*ext_param = &ext_params_30hz;
	else if (dst_fps == 60)
		*ext_param = &ext_params_60hz;
	else if (dst_fps == 90)
		*ext_param = &ext_params_90hz;
	else if (dst_fps == 120)
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
	struct lcm *ctx = panel_to_lcm(panel);
	struct mtk_dsi *dsi = NULL;

	dsi = container_of(connector, struct mtk_dsi, conn);
	pr_info("%s drm_mode_vrefresh = %d, m->hdisplay = %d\n",
		__func__, drm_mode_vrefresh(m_dst), m_dst->hdisplay);

	dst_fps = m_dst ? drm_mode_vrefresh(m_dst) : -EINVAL;

	if (dst_fps == 30) {
		ext->params = &ext_params_30hz;
		ctx->dynamic_fps = 30;
	} else if (dst_fps == 60) {
		if (dsi && (dsi->mi_cfg.feature_val[DISP_FEATURE_POWERSTATUS] == 1 ||
				dsi->mi_cfg.brightness_clone)) {
			ext_params_60hz.dyn_fps.dfps_cmd_table[2].para_list[1] = 0x80;
		} else if (dsi && (dsi->mi_cfg.feature_val[DISP_FEATURE_POWERSTATUS] == 2 ||
				dsi->mi_cfg.feature_val[DISP_FEATURE_POWERSTATUS] == 0)) {
			ext_params_60hz.dyn_fps.dfps_cmd_table[2].para_list[1] = 0x00;
		}
		ext->params = &ext_params_60hz;
		ctx->dynamic_fps = 60;
	} else if (dst_fps == 90) {
		if (dsi && (dsi->mi_cfg.feature_val[DISP_FEATURE_POWERSTATUS] == 1 ||
				dsi->mi_cfg.brightness_clone)) {
			ext_params_90hz.dyn_fps.dfps_cmd_table[2].para_list[1] = 0x80;
		} else if (dsi && (dsi->mi_cfg.feature_val[DISP_FEATURE_POWERSTATUS] == 2 ||
				dsi->mi_cfg.feature_val[DISP_FEATURE_POWERSTATUS] == 0)) {
			ext_params_90hz.dyn_fps.dfps_cmd_table[2].para_list[1] = 0x00;
		}
		ext->params = &ext_params_90hz;
		ctx->dynamic_fps = 90;
	} else if (dst_fps == 120) {
		if (dsi && (dsi->mi_cfg.feature_val[DISP_FEATURE_POWERSTATUS] == 1 ||
				dsi->mi_cfg.brightness_clone)) {
			ext_params_120hz.dyn_fps.dfps_cmd_table[2].para_list[1] = 0x80;
		} else if (dsi && (dsi->mi_cfg.feature_val[DISP_FEATURE_POWERSTATUS] == 2 ||
				dsi->mi_cfg.feature_val[DISP_FEATURE_POWERSTATUS] == 0)) {
			ext_params_120hz.dyn_fps.dfps_cmd_table[2].para_list[1] = 0x00;
		}
		ext->params = &ext_params_120hz;
		ctx->dynamic_fps = 120;
	} else {
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
	char gir_off_set[] = {0x5F,0x04,0x40};
	char gir_on_set[] = {0x5F,0x00,0x40};
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

	if (bl_level > PEAK_HDR_BL_LEVEL && bl_level <= PEAK_MAX_BL_LEVEL && !ctx->peak_hdr_status)
	{
		cb(dsi, handle, gir_off_set, ARRAY_SIZE(gir_off_set));
		ctx->gir_status = 0;
		mi_cfg->feature_val[DISP_FEATURE_FLAT_MODE]= 0;
		ctx->peak_hdr_status = 1;
	}

	if (bl_level != 0 && bl_level <= PEAK_HDR_BL_LEVEL && ctx->peak_hdr_status) {
		cb(dsi, handle, gir_on_set, ARRAY_SIZE(gir_on_set));
		ctx->gir_status = 1;
		mi_cfg->feature_val[DISP_FEATURE_FLAT_MODE]= 1;
		ctx->peak_hdr_status = 0;
	}

	pr_info("%s: peak_hdr_status = %d ctx->gir_status = %d\n", __func__, ctx->peak_hdr_status, ctx->gir_status);
err:
	return 1;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_tb0[] = {0x51, 0x00, 0x00};
	struct mtk_dsi *mtk_dsi = (struct mtk_dsi *)dsi;
	struct lcm *ctx = panel_to_lcm(this_panel);

	if (level) {
		bl_tb0[1] = (level >> 8) & 0xFF;
		bl_tb0[2] = level & 0xFF;
		mtk_dsi->mi_cfg.last_no_zero_bl_level = level;
	}

	if (!cb)
		return -1;

	if (atomic_read(&doze_enable)) {
		pr_info("%s: return when aod on, level %d = 0x%02X, 0x%02X\n\n",
			__func__, level, bl_tb0[1], bl_tb0[2]);
		return 0;
	}

	pr_info("%s: level %d = 0x%02X, 0x%02X\n", __func__, level, bl_tb0[1], bl_tb0[2]);

	mutex_lock(&ctx->panel_lock);
	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	mutex_unlock(&ctx->panel_lock);

	if (level != 0){
		last_non_zero_bl_level = level;
		panel_set_peak_hdr_status(mtk_dsi,cb,handle,level);
	}

	mtk_dsi->mi_cfg.last_bl_level = level;

	return 0;
}
static void mode_switch_to_120(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s\n", __func__);

	mutex_lock(&ctx->panel_lock);
	if (stage == BEFORE_DSI_POWERDOWN) {
		if (ctx->gir_status)
			push_table(ctx, mode_120hz_setting_gir_on,
				sizeof(mode_120hz_setting_gir_on) / sizeof(struct LCM_setting_table));
		else
			push_table(ctx, mode_120hz_setting_gir_off,
				sizeof(mode_120hz_setting_gir_off) / sizeof(struct LCM_setting_table));
		ctx->dynamic_fps = 120;
	}
	mutex_unlock(&ctx->panel_lock);
}


static void mode_switch_to_90(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s\n", __func__);

	mutex_lock(&ctx->panel_lock);
	if (stage == BEFORE_DSI_POWERDOWN) {
		if (ctx->gir_status)
			push_table(ctx, mode_90hz_setting_gir_on,
				sizeof(mode_90hz_setting_gir_on) / sizeof(struct LCM_setting_table));
		else
			push_table(ctx, mode_90hz_setting_gir_off,
				sizeof(mode_90hz_setting_gir_off) / sizeof(struct LCM_setting_table));
		ctx->dynamic_fps = 90;
	}
	mutex_unlock(&ctx->panel_lock);
}

static void mode_switch_to_60(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s\n", __func__);

	mutex_lock(&ctx->panel_lock);
	if (stage == BEFORE_DSI_POWERDOWN) {
		if (ctx->gir_status)
			push_table(ctx, mode_60hz_setting_gir_on,
				sizeof(mode_60hz_setting_gir_on) / sizeof(struct LCM_setting_table));
		else
			push_table(ctx, mode_60hz_setting_gir_off,
				sizeof(mode_60hz_setting_gir_off) / sizeof(struct LCM_setting_table));
		ctx->dynamic_fps = 60;
	}
	mutex_unlock(&ctx->panel_lock);
}

static int mode_switch(struct drm_panel *panel,
		struct drm_connector *connector, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	struct drm_display_mode *m_dst = get_mode_by_id(connector, dst_mode);
	struct drm_display_mode *m_cur = get_mode_by_id(connector, cur_mode);
	bool isFpsChange = false;

	if (cur_mode == dst_mode)
		return ret;

	isFpsChange = drm_mode_vrefresh(m_dst) == drm_mode_vrefresh(m_cur)? false: true;

	pr_info("%s isFpsChange = %d, dst_mode vrefresh = %d, cur_mode vrefresh = %d,, vdisplay = %d, hdisplay = %d\n",
			__func__, isFpsChange, drm_mode_vrefresh(m_dst), drm_mode_vrefresh(m_cur), m_dst->vdisplay, m_dst->hdisplay);

	if (isFpsChange) {
		if (drm_mode_vrefresh(m_dst) == 60) {
			mode_switch_to_60(panel, stage);
		} else if (drm_mode_vrefresh(m_dst) == 90) {
			mode_switch_to_90(panel, stage);
		} else if (drm_mode_vrefresh(m_dst) == 120) {
			mode_switch_to_120(panel, stage);
		} else
			ret = 1;
	}

	return ret;
}

static int panel_doze_enable(struct drm_panel *panel,
		void *dsi, dcs_write_gce cb, void *handle)
{
	atomic_set(&doze_enable, 1);
	pr_info("%s !-\n", __func__);
	return 0;
}

static int panel_doze_disable(struct drm_panel *panel,
		void *dsi_drv, dcs_write_gce cb, void *handle)
{
	atomic_set(&doze_enable, 0);
	pr_info("%s -\n", __func__);
	return 0;
}

static int panel_set_doze_brightness(struct drm_panel *panel, int doze_brightness)
{
	int ret = 0;
	struct lcm *ctx;
	unsigned int format = 0;
	char bl_tb0[] = {0x51, 0x00, 0x08};

	if (!panel) {
		pr_err("invalid params\n");
		return -1;
	}

	ctx = panel_to_lcm(panel);

	pr_info("%s set_doze_brightness state = %d",__func__, doze_brightness);

	if (DOZE_BRIGHTNESS_LBM  == doze_brightness || DOZE_BRIGHTNESS_HBM  == doze_brightness) {
		if (DOZE_BRIGHTNESS_LBM  == doze_brightness)
			ret = mi_disp_panel_ddic_send_cmd(lcm_aod_low_mode, ARRAY_SIZE(lcm_aod_low_mode), format);
		else if (DOZE_BRIGHTNESS_HBM == doze_brightness)
			ret = mi_disp_panel_ddic_send_cmd(lcm_aod_high_mode, ARRAY_SIZE(lcm_aod_high_mode), format);
		atomic_set(&doze_enable, 1);
	}

	if (DOZE_TO_NORMAL == doze_brightness) {
		pr_info("%s lcm_aod_exit_bl: 0x%02X 0x%02X\n", __func__, bl_tb0[1], bl_tb0[2]);
		mutex_lock(&ctx->panel_lock);
		lcm_dcs_write(ctx, bl_tb0, ARRAY_SIZE(bl_tb0));
		mutex_unlock(&ctx->panel_lock);
		//ret = mi_disp_panel_ddic_send_cmd(lcm_aod_mode_exit, ARRAY_SIZE(lcm_aod_mode_exit), format);
		atomic_set(&doze_enable, 0);
	}

	ctx->doze_brightness_state = doze_brightness;
	pr_info("%s set doze_brightness %d end -\n", __func__, doze_brightness);

	return ret;
}

static int panel_get_doze_brightness(struct drm_panel *panel, u32 *doze_brightness)
{
	int count = 0;
	struct lcm *ctx = panel_to_lcm(panel);

	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

	*doze_brightness = ctx->doze_brightness_state;

	pr_info("%s get doze_brightness %d end -\n", __func__, *doze_brightness);

	return count;
}

#ifdef CONFIG_MI_DISP
static void lcm_esd_restore_backlight(struct drm_panel *panel)
{
	char bl_tb0[] = {0x51, 0x00, 0x00};
	struct lcm *ctx = panel_to_lcm(panel);

	bl_tb0[1] = (last_non_zero_bl_level >> 8) & 0xFF;
	bl_tb0[2] = last_non_zero_bl_level & 0xFF;

	pr_info("%s: restore to level = %d\n", __func__, last_non_zero_bl_level);

	mutex_lock(&ctx->panel_lock);
	lcm_dcs_write(ctx, bl_tb0, ARRAY_SIZE(bl_tb0));
	mutex_unlock(&ctx->panel_lock);

#ifdef CONFIG_MI_DISP_PANEL_COUNT
	/* add for display backlight count */
	mi_dsi_panel_count_enter(panel, PANEL_BACKLIGHT, last_non_zero_bl_level, 0);
#endif
	return;
}

static unsigned int bl_level = 2047;
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
		bl_level = level;
	}
	pr_err("lcm_setbacklight_control backlight %d\n", level);

	return 0;
}

static int panel_get_wp_info(struct drm_panel *panel, char *buf, size_t size)
{
	/* Read 8 bytes from 0xA1 register
	 * 	BIT[0-1] = Lux
	 * 	BIT[2-4] = Wx
	 * 	BIT[5-7] = Wy */
	static uint16_t lux = 0, wx = 0, wy = 0;
	int i, ret = 0, count = 0;
	struct lcm *ctx = NULL;
	u8 tx_buf[] = {0xA3};
	u8 rx_buf[8] = {0x00};
	struct mtk_ddic_dsi_msg cmds[] = {
		{
			.channel = 0,
			.tx_cmd_num = 1,
			.rx_cmd_num = 1,
			.type[0] = 0x06,
			.tx_buf[0] = &tx_buf[0],
			.tx_len[0] = 1,
			.rx_buf[0] = &rx_buf[0],
			.rx_len[0] = 8,
		},
	};

	pr_info("%s: +\n", __func__);

	/* try to get wp info from cache */
	if (lux > 0 && wx > 0 && wy > 0) {
		pr_info("%s: got wp info from cache\n", __func__);
		goto cache;
	}

	/* try to get wp info from cmdline */
	if (sscanf(oled_wp_cmdline, "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n",
			&rx_buf[0], &rx_buf[1], &rx_buf[2], &rx_buf[3],
			&rx_buf[4], &rx_buf[5]) == 8) {

		lux = rx_buf[0] << 8 | rx_buf[1];
		wx = rx_buf[2] << 8 | rx_buf[3];
		wy = rx_buf[4] << 8 | rx_buf[5];
		if (lux > 0 && wx > 0 && wy > 0) {
			pr_info("%s: got wp info from cmdline\n", __func__);
			goto done;
		}
	}

	/* try to get wp info from panel register */
	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		goto err;
	}

	ctx = panel_to_lcm(panel);
	if (!ctx || !ctx->enabled) {
		pr_err("%s: ctx is NULL or panel isn't enabled\n", __func__);
		goto err;
	}

	memset(rx_buf, 0, sizeof(rx_buf));
	for (i = 0; i < sizeof(cmds)/sizeof(struct mtk_ddic_dsi_msg); ++i) {
		ret |= mtk_ddic_dsi_read_cmd(&cmds[i]);
	}

	if (ret != 0) {
		pr_err("%s: failed to read ddic register\n", __func__);
		memset(rx_buf, 0, sizeof(rx_buf));
		goto err;
	}

	/* rx_buf[0-1] is lux(HEX), rx_buf[2-4] is wx(DEC), rx_buf[5-7] is wy(DEC) */
	lux = rx_buf[0] << 8 | rx_buf[1];
	wx = rx_buf[2] * 100 + rx_buf[3] * 10 + rx_buf[4];
	wy = rx_buf[5] * 100 + rx_buf[6] * 10 + rx_buf[7];

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

static int panel_set_gir_on(struct drm_panel *panel)
{
	struct LCM_setting_table gir_on_set[] = {
		{0x5F, 2, {0x00,0x40} },
	};
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
		mi_disp_panel_ddic_send_cmd(gir_on_set, ARRAY_SIZE(gir_on_set), format);
	}

err:
	pr_info("%s: -\n", __func__);
	return ret;
}

static int panel_set_gir_off(struct drm_panel *panel)
{
	struct LCM_setting_table gir_off_set[] = {
		{0x5F, 2, {0x04,0x40} },
	};
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
		mi_disp_panel_ddic_send_cmd(gir_off_set, ARRAY_SIZE(gir_off_set), format);
	}

err:
	pr_info("%s: -\n", __func__);
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

static bool get_panel_initialized(struct drm_panel *panel)
{
	struct lcm *ctx;
	bool ret = false;

	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		goto err;
	}

	ctx = panel_to_lcm(panel);
	ret = ctx->prepared;
err:
	return ret;
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

static int panel_get_dynamic_fps(struct drm_panel *panel, u32 *fps)
{
	int ret = 0;
	struct lcm *ctx;
 
	if (!panel || !fps) {
		pr_err("%s: panel or fps is NULL\n", __func__);
		ret = -1;
		goto err;
	}

	ctx = panel_to_lcm(panel);
	*fps = ctx->dynamic_fps;
err:
	return ret;
}


static int panel_fod_lhbm_init (struct mtk_dsi* dsi)
{
	if (!dsi) {
		pr_info("invalid dsi point\n");
		return -1;
	}
	pr_info("panel_fod_lhbm_init enter\n");
	dsi->display_type = "primary";
	dsi->mi_cfg.lhbm_ui_ready_delay_frame = 4;
	dsi->mi_cfg.lhbm_ui_ready_delay_frame_aod = 5;
	dsi->mi_cfg.local_hbm_enabled = 1;
	return 0;
}

static int panel_hbm_fod_control(struct drm_panel *panel, bool en)
{
	struct lcm *ctx;
	int ret = -1;
	struct LCM_setting_table hbm_fod_on_cmd[] = {
		{0x51, 02, {0x0F,0xFF}},
	};
	struct LCM_setting_table hbm_fod_off_cmd[] = {
		{0x51, 02, {0x07,0xFF}},
	};
	unsigned int format = 0;

	pr_info("%s: +\n", __func__);
 	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		goto err;
	}
 	ctx = panel_to_lcm(panel);
	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
		goto err;
	}
 	if (en) {
		ret = mi_disp_panel_ddic_send_cmd(hbm_fod_on_cmd, ARRAY_SIZE(hbm_fod_on_cmd), format);
	} else {
		ret = mi_disp_panel_ddic_send_cmd(hbm_fod_off_cmd, ARRAY_SIZE(hbm_fod_on_cmd), format);
	}
	if (ret != 0) {
		DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
	}
err:
	pr_info("%s: -\n", __func__);
	return ret;
}

static void mi_parse_cmdline_perBL(struct LHBM_WHITEBUF * lhbm_whitebuf) {
	int i = 0;
	static u8 lhbm_cmdbuf[18] = {0};

	pr_info("mi_parse_cmdline_perBL enter\n");

	if(!lhbm_w1200_update_flag && !lhbm_w200_update_flag && !lhbm_g500_update_flag) {
		pr_info("don't need update white rgb config");
		return;
	}

	if (lhbm_whitebuf == NULL) {
		pr_err("lhbm_status == NULL\n");
		return;
	}

	for (i = 0; i < 18; i++) {
		sscanf(oled_lhbm_cmdline + 2 * i, "%02hhx", &lhbm_cmdbuf[i]);
	}

	for (i = 0; i < 6; i++){
		lhbm_whitebuf->lhbm_1200[i] = lhbm_cmdbuf[i];
		lhbm_whitebuf->lhbm_500[i] = lhbm_cmdbuf[i+6];
		lhbm_whitebuf->lhbm_200[i] = lhbm_cmdbuf[i+12];
	}

	pr_info("lhbm_1200 \n");
	for (i = 0; i < 6; i++){
		lhbm_normal_white_1200nit[22].para_list[i] = lhbm_whitebuf->lhbm_1200[i];
		pr_info("0x%02hhx",lhbm_whitebuf->lhbm_1200[i]);
	}

	pr_info("lhbm_200 \n");
	for (i = 0; i < 6; i++){
		lhbm_normal_white_200nit[22].para_list[i] = lhbm_whitebuf->lhbm_200[i];
		pr_info("0x%02hhx ",lhbm_whitebuf->lhbm_200[i]);
	}

	pr_info("lhbm_500 \n");
	for (i = 0; i < 2; i++){
		lhbm_normal_green_500nit[22].para_list[i] = lhbm_whitebuf->lhbm_500[i];
		pr_info("0x%02hhx ",lhbm_whitebuf->lhbm_500[i]);
	}

	lhbm_w1200_readbackdone = true;
	lhbm_w200_readbackdone = true;
	lhbm_g500_readbackdone = true;
	lhbm_w1200_update_flag = false;
	lhbm_w200_update_flag = false;
	lhbm_g500_update_flag =false;

	return;
}

static int mi_disp_panel_update_lhbm_A9reg(struct mtk_dsi * dsi, enum lhbm_cmd_type type, int flat_mode, int bl_level)
{
	u8 alpha_buf[2] = {0};
	unsigned int fix_alpha = 0;

	if(!dsi)
		return -EINVAL;

	if(!lhbm_w1200_readbackdone ||
		!lhbm_w200_readbackdone ||
		!lhbm_g500_readbackdone) {
		pr_info("mi_disp_panel_update_lhbm_white_param cmdline_lhbm:%s\n", oled_lhbm_cmdline);
		mi_parse_cmdline_perBL(&lhbm_whitebuf);
	}

	if (!(type == TYPE_LHBM_OFF || type == TYPE_HLPM_OFF)) {
		if (bl_level > NORMAL_HW_BRIGHTNESS) {
			fix_alpha = 1;
			bl_level = NORMAL_HW_BRIGHTNESS;
		}
	}

	if (bl_level > normal_max_bl) {
		fix_alpha = 1;
		bl_level = normal_max_bl;
	}

	if(flat_mode)
		pr_info("lhbm update 0xA9, lhbm_cmd_type:%d, flat_mode:%d, bl_level = %d, alpha = %d, fix_alpha = %u\n", type, flat_mode, bl_level, giron_alpha_set[bl_level], fix_alpha);
	else
		pr_info("lhbm update 0xA9, lhbm_cmd_type:%d, flat_mode:%d, bl_level = %d, alpha = %d, fix_alpha = %u\n", type, flat_mode, bl_level, giroff_alpha_set[bl_level], fix_alpha);
	switch (type) {
	case TYPE_WHITE_1200:
		if (flat_mode) {
			alpha_buf[0] = (giron_alpha_set[bl_level] >> 8) & 0xff;
			alpha_buf[1] = giron_alpha_set[bl_level] & 0xff;
			pr_info("[%d] alpha_buf[0] = %02hhx, alpha_buf[1] = %02hhx\n",
					type, alpha_buf[0],  alpha_buf[1]);
			lhbm_normal_white_1200nit[23].para_list[12] = alpha_buf[0];
			lhbm_normal_white_1200nit[23].para_list[13] = alpha_buf[1];
		} else {
			alpha_buf[0] = (giroff_alpha_set[bl_level] >> 8) & 0xff;
			alpha_buf[1] = giroff_alpha_set[bl_level] & 0xff;
			pr_info("[%d] alpha_buf[0] = %02hhx, alpha_buf[1] = %02hhx\n",
					type, alpha_buf[0],  alpha_buf[1]);
			lhbm_normal_white_1200nit[23].para_list[12] = alpha_buf[0];
			lhbm_normal_white_1200nit[23].para_list[13] = alpha_buf[1];
		}
		if (fix_alpha) {
			//alpha fixed on 0x3F,0xF0
			lhbm_normal_white_1200nit[4].para_list[0] = 0x01;
			lhbm_normal_white_1200nit[6].para_list[1] = 0x0A;
		} else {
			//follow alpha table
			lhbm_normal_white_1200nit[4].para_list[0] = 0x09;
			lhbm_normal_white_1200nit[6].para_list[1] = 0x1A;
		}
		break;
	case TYPE_WHITE_200:
		if (flat_mode) {
			alpha_buf[0] = (giron_alpha_set[bl_level] >> 8) & 0xff;
			alpha_buf[1] = giron_alpha_set[bl_level] & 0xff;
			pr_info("[%d] alpha_buf[0] = %02hhx, alpha_buf[1] = %02hhx\n",
					type, alpha_buf[0],  alpha_buf[1]);
			lhbm_normal_white_200nit[23].para_list[12] = alpha_buf[0];
			lhbm_normal_white_200nit[23].para_list[13] = alpha_buf[1];
		} else {
			alpha_buf[0] = (giroff_alpha_set[bl_level] >> 8) & 0xff;
			alpha_buf[1] = giroff_alpha_set[bl_level] & 0xff;
			pr_info("[%d] alpha_buf[0] = %02hhx, alpha_buf[1] = %02hhx\n",
					type, alpha_buf[0],  alpha_buf[1]);
			lhbm_normal_white_200nit[23].para_list[12] = alpha_buf[0];
			lhbm_normal_white_200nit[23].para_list[13] = alpha_buf[1];
		}
		if (fix_alpha) {
			//alpha fixed on 0x3F,0xF0
			lhbm_normal_white_200nit[4].para_list[0] = 0x01;
			lhbm_normal_white_200nit[6].para_list[1] = 0x0A;
		} else {
			//follow alpha table
			lhbm_normal_white_200nit[4].para_list[0] = 0x09;
			lhbm_normal_white_200nit[6].para_list[1] = 0x1A;
		}
		break;
	case TYPE_GREEN_500:
		if (flat_mode) {
			alpha_buf[0] = (giron_alpha_set[bl_level] >> 8) & 0xff;
			alpha_buf[1] = giron_alpha_set[bl_level] & 0xff;
			pr_info("[%d] alpha_buf[0] = %02hhx, alpha_buf[1] = %02hhx\n",
					type, alpha_buf[0],  alpha_buf[1]);
			lhbm_normal_green_500nit[23].para_list[12] = alpha_buf[0];
			lhbm_normal_green_500nit[23].para_list[13] = alpha_buf[1];
		} else {
			alpha_buf[0] = (giroff_alpha_set[bl_level] >> 8) & 0xff;
			alpha_buf[1] = giroff_alpha_set[bl_level] & 0xff;
			pr_info("[%d] alpha_buf[0] = %02hhx, alpha_buf[1] = %02hhx\n",
					type, alpha_buf[0],  alpha_buf[1]);
			lhbm_normal_green_500nit[23].para_list[12] = alpha_buf[0];
			lhbm_normal_green_500nit[23].para_list[13] = alpha_buf[1];
		}
		if (fix_alpha) {
			//alpha fixed on 0x3F,0xF0
			lhbm_normal_green_500nit[4].para_list[0] = 0x01;
			lhbm_normal_green_500nit[6].para_list[1] = 0x0A;
		} else {
			//follow alpha table
			lhbm_normal_green_500nit[4].para_list[0] = 0x09;
			lhbm_normal_green_500nit[6].para_list[1] = 0x1A;
		}
		pr_info("green 500 update flat mode off 120\n");
		break;
	default:
		pr_err("unsuppport cmd \n");
	return -EINVAL;
	}

	return 0;
}

static int panel_set_lhbm_fod(struct mtk_dsi *dsi, enum local_hbm_state lhbm_state)
{
	struct lcm *ctx = NULL;
	struct mi_dsi_panel_cfg *mi_cfg = NULL;
	int bl_level;
	int flat_mode;
	int bl_level_doze = doze_hbm_dbv_level;
	unsigned int format = 0;

	if (!dsi || !dsi->panel) {
		pr_err("%s: panel is NULL\n", __func__);
		return -1;
	}

	ctx = panel_to_lcm(dsi->panel);
	mi_cfg = &dsi->mi_cfg;
	bl_level = mi_cfg->last_bl_level;
	flat_mode = ctx->gir_status;

	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
		return -1;
	}

	if (atomic_read(&doze_enable) &&
		ctx->doze_brightness_state == DOZE_BRIGHTNESS_LBM)
			bl_level_doze = doze_lbm_dbv_level;
	else if (atomic_read(&doze_enable) &&
		ctx->doze_brightness_state == DOZE_BRIGHTNESS_HBM)
			bl_level_doze = doze_hbm_dbv_level;
	else
			bl_level_doze = mi_cfg->last_no_zero_bl_level;

	pr_info("%s local hbm_state:%d \n",__func__, lhbm_state);

	switch (lhbm_state) {
	case LOCAL_HBM_OFF_TO_NORMAL:
	case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE:
	case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT:
		pr_info("LOCAL_HBM_OFF\n");

		format = FORMAT_LP_MODE | FORMAT_BLOCK;
		mi_disp_panel_ddic_send_cmd(lhbm_off, ARRAY_SIZE(lhbm_off), format);
		ctx->lhbm_en = false;
		break;
	case LOCAL_HBM_NORMAL_WHITE_750NIT:
		break;
	case LOCAL_HBM_NORMAL_WHITE_500NIT:
		break;
	case LOCAL_HBM_NORMAL_WHITE_110NIT:
		pr_info("LOCAL_HBM_NORMAL_WHITE_200NIT\n");

		if (atomic_read(&doze_enable)) {
			mutex_unlock(&dsi->dsi_lock);
			if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_HBM)
				mi_cfg->last_no_zero_bl_level = bl_level_doze;
			else if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_LBM)
				mi_cfg->last_no_zero_bl_level = bl_level_doze;
			mutex_lock(&dsi->dsi_lock);
			mi_disp_panel_update_lhbm_A9reg(dsi, TYPE_WHITE_200, flat_mode, doze_lbm_dbv_level);
		} else {
			mi_disp_panel_update_lhbm_A9reg(dsi, TYPE_WHITE_200, flat_mode, bl_level);
		}
		format = FORMAT_LP_MODE | FORMAT_BLOCK;
		mi_disp_panel_ddic_send_cmd(lhbm_normal_white_200nit, ARRAY_SIZE(lhbm_normal_white_200nit), format);
		ctx->lhbm_en = true;
		break;
	case LOCAL_HBM_NORMAL_GREEN_500NIT:
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		pr_info("LOCAL_HBM_NORMAL_GREEN_500NIt\n");

		mi_disp_panel_update_lhbm_A9reg(dsi, TYPE_GREEN_500, flat_mode, bl_level);

		format = FORMAT_LP_MODE | FORMAT_BLOCK;
		mi_disp_panel_ddic_send_cmd(lhbm_normal_green_500nit, ARRAY_SIZE(lhbm_normal_green_500nit), format);
		ctx->lhbm_en = true;
		break;
	case LOCAL_HBM_NORMAL_WHITE_1000NIT:
		mi_cfg->dimming_state = STATE_DIM_BLOCK;

		if (atomic_read(&doze_enable)) {
			mutex_unlock(&dsi->dsi_lock);
			if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_HBM)
				mi_cfg->last_no_zero_bl_level = bl_level_doze;
			else if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_LBM)
				mi_cfg->last_no_zero_bl_level = bl_level_doze;
			mutex_lock(&dsi->dsi_lock);
			mi_disp_panel_update_lhbm_A9reg(dsi, TYPE_WHITE_1200, flat_mode, doze_lbm_dbv_level);
		} else {
			mi_disp_panel_update_lhbm_A9reg(dsi, TYPE_WHITE_1200, flat_mode, bl_level);
		}

		pr_info("LOCAL_HBM_NORMAL_WHITE_1200NIT in HBM\n");

		format = FORMAT_LP_MODE | FORMAT_BLOCK;
		mi_disp_panel_ddic_send_cmd(lhbm_normal_white_1200nit, ARRAY_SIZE(lhbm_normal_white_1200nit), format);

		ctx->lhbm_en = true;
		break;
	case LOCAL_HBM_HLPM_WHITE_110NIT:
		pr_info("LOCAL_HBM_HLPM_WHITE_200NIT\n");
		mi_cfg->dimming_state = STATE_DIM_BLOCK;

		if (atomic_read(&doze_enable)) {
			mutex_unlock(&dsi->dsi_lock);
			if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_HBM)
				mi_cfg->last_no_zero_bl_level = bl_level_doze;
			else if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_LBM)
				mi_cfg->last_no_zero_bl_level = bl_level_doze;
			mutex_lock(&dsi->dsi_lock);
		}

		mi_disp_panel_update_lhbm_A9reg(dsi, TYPE_WHITE_200, flat_mode, bl_level_doze);
		format = FORMAT_LP_MODE | FORMAT_BLOCK;
		mi_disp_panel_ddic_send_cmd(lhbm_normal_white_200nit, ARRAY_SIZE(lhbm_normal_white_200nit), format);

		ctx->lhbm_en = true;
		break;
	case LOCAL_HBM_HLPM_WHITE_1000NIT:
		pr_info("LOCAL_HBM_HLPM_WHITE_1200NIT\n");
		mi_cfg->dimming_state = STATE_DIM_BLOCK;

		if (atomic_read(&doze_enable)) {
			mutex_unlock(&dsi->dsi_lock);
			if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_HBM)
				mi_cfg->last_no_zero_bl_level = bl_level_doze;
			else if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_LBM)
				mi_cfg->last_no_zero_bl_level = bl_level_doze;
			mutex_lock(&dsi->dsi_lock);
		}

		mi_disp_panel_update_lhbm_A9reg(dsi, TYPE_WHITE_1200, flat_mode, bl_level_doze);

		format = FORMAT_LP_MODE | FORMAT_BLOCK;
		mi_disp_panel_ddic_send_cmd(lhbm_normal_white_1200nit, ARRAY_SIZE(lhbm_normal_white_1200nit), format);
		ctx->lhbm_en = true;
		break;
	case LOCAL_HBM_OFF_TO_HLPM:
		pr_info("LOCAL_HBM_OFF_TO_HLPM\n");
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		break;
	case LOCAL_HBM_OFF_TO_LLPM:
		pr_info("LOCAL_HBM_OFF_TO_LLPM\n");
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		break;
	default:
		pr_info("invalid local hbm value\n");
		break;
	}
	return 0;
}


static int panel_set_gray_by_temperature (struct drm_panel *panel, int level)
{
	int ret = 0;
	struct lcm *ctx;
	unsigned int format = 0;

	pr_info("%s: +\n", __func__);

	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		ret = -1;
		goto err;
	}

	ctx = panel_to_lcm(panel);
	if (!ctx->enabled) {
		ctx->gray_level = level;
		ret = -1;
		pr_err("%s: panel isn't enabled\n", __func__);
		goto err;
	}

	pr_info("%s: level = %x\n", __func__, level);

	ctx->gray_level = level;

	if (level == TEMP_INDEX1_36) {
		gray_3d_lut[0].para_list[0] = 0x82;
		mi_disp_panel_ddic_send_cmd(gray_3d_lut, ARRAY_SIZE(gray_3d_lut), format);
	} else if (level == TEMP_INDEX2_32) {
		gray_3d_lut[0].para_list[0] = 0x81;
		mi_disp_panel_ddic_send_cmd(gray_3d_lut, ARRAY_SIZE(gray_3d_lut), format);
	} else if (level == TEMP_INDEX3_28) {
		gray_3d_lut[0].para_list[0] = 0x80;
		mi_disp_panel_ddic_send_cmd(gray_3d_lut, ARRAY_SIZE(gray_3d_lut), format);
	} else if (level == TEMP_INDEX4_off ) {
		gray_3d_lut[0].para_list[0] = 0x00;
		mi_disp_panel_ddic_send_cmd(gray_3d_lut, ARRAY_SIZE(gray_3d_lut), format);
	}

err:
	pr_info("%s: -\n", __func__);
	return ret;
}
#endif

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.ext_param_set = mtk_panel_ext_param_set,
	.ext_param_get = mtk_panel_ext_param_get,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.mode_switch = mode_switch,
	.set_doze_brightness = panel_set_doze_brightness,
	.get_doze_brightness = panel_get_doze_brightness,
	.panel_poweron = lcm_panel_poweron,
	.panel_poweroff = lcm_panel_poweroff,
	.doze_disable = panel_doze_disable,
	.doze_enable = panel_doze_enable,
#ifdef CONFIG_MI_DISP
	.esd_restore_backlight = lcm_esd_restore_backlight,
	.setbacklight_control = lcm_setbacklight_control,
	.get_wp_info = panel_get_wp_info,
	.get_panel_info = panel_get_panel_info,
	.panel_set_gir_on = panel_set_gir_on,
	.panel_set_gir_off = panel_set_gir_off,
	.panel_get_gir_status = panel_get_gir_status,
	.get_panel_factory_max_brightness = panel_get_factory_max_brightness,
	.get_panel_initialized = get_panel_initialized,
	.get_panel_max_brightness_clone = panel_get_max_brightness_clone,
	.get_panel_dynamic_fps =  panel_get_dynamic_fps,
	.panel_fod_lhbm_init = panel_fod_lhbm_init,
	.set_lhbm_fod = panel_set_lhbm_fod,
	.hbm_fod_control = panel_hbm_fod_control,
	.set_gray_by_temperature = panel_set_gray_by_temperature,
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
	struct drm_display_mode *mode_30, *mode_60, *mode_90, *mode_120;

	mode_30 = drm_mode_duplicate(connector->dev, &mode_30hz);
	if (!mode_30) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			mode_30hz.hdisplay, mode_30hz.vdisplay,
			drm_mode_vrefresh(&mode_30hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_30);
	mode_30->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode_30);

	mode_60 = drm_mode_duplicate(connector->dev, &mode_60hz);
	if (!mode_60) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			mode_60hz.hdisplay, mode_60hz.vdisplay,
			drm_mode_vrefresh(&mode_60hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_60);
	mode_60->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode_60);

	mode_90 = drm_mode_duplicate(connector->dev, &mode_90hz);
	if (!mode_90) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			mode_90hz.hdisplay, mode_90hz.vdisplay,
			drm_mode_vrefresh(&mode_90hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_90);
	mode_90->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_90);

	mode_120 = drm_mode_duplicate(connector->dev, &mode_120hz);
	if (!mode_120) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			mode_120hz.hdisplay, mode_120hz.vdisplay,
			drm_mode_vrefresh(&mode_120hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_120);
	mode_120->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_120);

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

	pr_info("%s n11a-42-02-0a-dsc +\n", __func__);

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

	ctx->reset_gpio = devm_gpiod_get_index(dev, "reset", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->dvdd_gpio = devm_gpiod_get_index(dev, "dvdd", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->dvdd_gpio)) {
		dev_err(dev, "%s: cannot get dvdd-gpios %ld\n",
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
		pr_err("%s init vio18 regulator error\n", __func__);

	ctx->prepared = true;
	ctx->enabled = true;
	ctx->hbm_enabled = false;
	ctx->panel_info = panel_name;
	ctx->dynamic_fps = 60;
	ctx->panel_id = panel_id;
	ctx->gir_status = 1;
	ctx->max_brightness_clone = MAX_BRIGHTNESS_CLONE;
	ctx->factory_max_brightness = FACTORY_MAX_BRIGHTNESS;
	ctx->peak_hdr_status = 0;
#ifdef CONFIG_MI_DISP
	ext_params_60hz.err_flag_irq_gpio = of_get_named_gpio_flags(
		dev->of_node, "mi,esd-err-irq-gpio",
		0, (enum of_gpio_flags *)&(ext_params_60hz.err_flag_irq_flags));
	ext_params_30hz.err_flag_irq_gpio = ext_params_60hz.err_flag_irq_gpio;
	ext_params_30hz.err_flag_irq_flags = ext_params_60hz.err_flag_irq_flags;
	ext_params_90hz.err_flag_irq_gpio = ext_params_60hz.err_flag_irq_gpio;
	ext_params_90hz.err_flag_irq_flags = ext_params_60hz.err_flag_irq_flags;
	ext_params_120hz.err_flag_irq_gpio = ext_params_60hz.err_flag_irq_gpio;
	ext_params_120hz.err_flag_irq_flags = ext_params_60hz.err_flag_irq_flags;
#endif
	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &lcm_drm_funcs;

	mutex_init(&ctx->panel_lock);
	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	//mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params_60hz, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif
	this_panel = &ctx->panel;

	pr_info("%s n11a-42-02-0a-dsc-vdo -\n", __func__);
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
	{ .compatible = "n11a_42_02_0a_dsc_vdo,lcm", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "n11a_42_02_0a_dsc_vdo,lcm",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

module_param_string(oled_wp, oled_wp_cmdline, sizeof(oled_wp_cmdline), 0600);
MODULE_PARM_DESC(oled_wp, "oled_wp=<white_point_info>");

module_param_string(oled_lhbm, oled_lhbm_cmdline, sizeof(oled_lhbm_cmdline), 0600);
MODULE_PARM_DESC(oled_lhbm, "oled_lhbm=<local_hbm_info>");

MODULE_AUTHOR("chenyuan <chenyuan8@xiaomi.com>");
MODULE_DESCRIPTION("n11a_42_02_0a_dsc_vdo oled panel driver");
MODULE_LICENSE("GPL v2");
