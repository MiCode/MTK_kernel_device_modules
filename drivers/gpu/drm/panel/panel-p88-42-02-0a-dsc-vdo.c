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
#include <linux/kernel.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#include "../mediatek/mediatek_v2/mtk_log.h"
#endif

#include "../mediatek/mediatek_v2/mi_disp/mi_panel_ext.h"
#include "../mediatek/mediatek_v2/mi_disp/mi_dsi_panel.h"
#include "../mediatek/mediatek_v2/mtk_dsi.h"

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif
#include "include/panel-p88-42-02-0a-dsc-vdo.h"

extern struct mtk_dsi * mi_get_primary_dsi_display(void);
static int lcm_panel_poweroff(struct drm_panel *panel);
static int lcm_panel_poweron(struct drm_panel *panel);
static int lcm_panel_vddi_enable(struct lcm *ctx);
static int lcm_panel_vddi_disable(struct lcm *ctx);
static char buildid_cmdline[4] = {0};
static char panel_sn_cmdline[13] = {0};
#define PANEL_BUILD_ID_P00  0
#define PANEL_BUILD_ID_P01  0x10
#define PANEL_BUILD_ID_P10  0x40

extern unsigned int mipi_volt;
extern int ktz8866_bl_bias_conf(void);
extern int ktz8866_bias_enable(int enable);
extern int ktz8866_brightness_set(int level);
extern int ktz8866_reg_write_bytes(unsigned char addr, unsigned char value);
extern int ktz8866_reg_read_bytes(unsigned char addr, char *value);

static unsigned char panel_build_id = PANEL_BUILD_ID_P10;
static void get_build_id(void) {
	pr_info("%s: buildid_cmdline:%s  +\n", __func__, buildid_cmdline);
	sscanf(buildid_cmdline, "%02hhx\n", &panel_build_id);
	pr_info("%s: panel_build_id:%d  +\n", __func__, panel_build_id);
	return ;
}

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

static struct regulator *disp_vddi;

static int lcm_panel_vddi_regulator_init(struct device *dev)
{
	static int vio18_regulator_inited;
	int ret = 0;

	if (vio18_regulator_inited)
		return ret;

	/* please only get regulator once in a driver */
	disp_vddi = regulator_get(dev, "vcn18io");
	if (IS_ERR(disp_vddi)) { /* handle return value */
		ret = PTR_ERR(disp_vddi);
		disp_vddi = NULL;
		pr_err("get disp_vddi fail, error: %d\n", ret);
		return ret;
	}

	vio18_regulator_inited = 1;
	return ret; /* must be 0 */
}

static int lcm_panel_vddi_enable(struct lcm *ctx)
{
	int retval = 0;

	pr_info("%s +\n",__func__);
	if (ctx->prepared)
		return 0;

	ctx->dvdd_gpio = devm_gpiod_get_index(ctx->dev,
		"dvdd", 0, GPIOD_OUT_HIGH); //1.9V,DVDD_1P9
	if (IS_ERR(ctx->dvdd_gpio)) {
		dev_err(ctx->dev, "%s: cannot get dvdd gpio %ld\n",
			__func__, PTR_ERR(ctx->dvdd_gpio));
		return PTR_ERR(ctx->dvdd_gpio);
	}
	gpiod_set_value(ctx->dvdd_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->dvdd_gpio);

	pr_info("%s -\n",__func__);
	return retval;
}

static int lcm_panel_vddi_disable(struct lcm *ctx)
{
	int retval = 0;

	pr_info("%s +\n",__func__);
	if (ctx->prepared)
		return 0;

	ctx->dvdd_gpio = devm_gpiod_get_index(ctx->dev,
		"dvdd", 0, GPIOD_OUT_HIGH); //1.9V,DVDD_1P9
	if (IS_ERR(ctx->dvdd_gpio)) {
		dev_err(ctx->dev, "%s: cannot get dvdd gpio %ld\n",
			__func__, PTR_ERR(ctx->dvdd_gpio));
		return PTR_ERR(ctx->dvdd_gpio);
	}
	gpiod_set_value(ctx->dvdd_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->dvdd_gpio);

	pr_info("%s -\n",__func__);
	return retval;
}

#if 0
static int lcm_panel_vddi_enable(struct device *dev)
{
	int ret = 0;
	int retval = 0;
	int status = 0;
	static unsigned int vio18_start_up = 1;

	pr_info("%s +\n",__func__);
	if (disp_vddi == NULL)
		return retval;

	/* set voltage with min & max*/
	ret = regulator_set_voltage(disp_vddi, 1900000, 1900000);
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
#endif

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

static bool check_power_state(void)
{
	if (system_state == SYSTEM_RESTART || system_state == SYSTEM_HALT || system_state == SYSTEM_POWER_OFF)
	{
		pr_info("%s: system_state=%d\n", __func__, system_state);
		return true;;
	}
	return false;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mtk_dsi *dsi = mi_get_primary_dsi_display();

	if (!ctx->prepared)
		return 0;

	pr_info("%s\n", __func__);
	ktz8866_brightness_set(0);

	mutex_lock(&ctx->panel_lock);
	push_table(ctx, lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table));
	mutex_unlock(&ctx->panel_lock);


	if (!is_lcm_connected) {
		pr_info("%s is_lcm_connected=%d, not config backlight ic\n", __func__, is_lcm_connected);
	} else {
		pr_info("[%s]:  is_tddi_flag =%d, tddi_gesture_flag =%d, panel_dead_flag =%d\n", 
				__func__,
				dsi->mi_cfg.is_tddi_flag,
				dsi->mi_cfg.tddi_gesture_flag,
				dsi->mi_cfg.panel_dead_flag);
		if (dsi->mi_cfg.is_tddi_flag) {
			if (!dsi->mi_cfg.tddi_gesture_flag || dsi->mi_cfg.panel_dead_flag || check_power_state()) {
				pr_info("%s ldo is normal mode!!\n", __func__);
				ktz8866_bias_enable(0);
			} else if (dsi->mi_cfg.tddi_gesture_flag) {
				pr_info("%s ldo is TP gesture mode!!\n", __func__);
			}
		} else {
			ktz8866_bias_enable(0);
		}
	}

	ctx->error = 0;
	ctx->prepared = false;
	atomic_set(&doze_enable, 0);
	return 0;
}

static void lcm_panel_init(struct lcm *ctx)
{
	struct mtk_dsi *dsi =  mi_get_primary_dsi_display();
	pr_info("%s+\n", __func__);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return;
	}

	usleep_range(5 * 1000, (5* 1000)+20);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(1 * 1000, (1* 1000)+20);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(1 * 1000, (1* 1000)+20);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, (10 * 1000)+20);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	pr_info("%s: dynamic_fps:%d, gir_status:%d\n", __func__,
		ctx->dynamic_fps, ctx->gir_status);

	if (dsi->mi_cfg.is_tddi_flag) {
		if(dsi->mi_cfg.mi_display_gesture_cb != NULL)
		{
			dsi->mi_cfg.mi_display_gesture_cb();
			pr_info("%s mi_display_gesture_cb call\n", __func__);
		}
	}

	push_table(ctx, init_setting_vdo, sizeof(init_setting_vdo) / sizeof(struct LCM_setting_table));
	if (dsi->mi_cfg.panel_dead_flag) {
		dsi->mi_cfg.panel_dead_flag = false;
	}
	pr_info("%s-\n", __func__);
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mtk_dsi *dsi = mi_get_primary_dsi_display();
	int ret;

	pr_info("%s\n", __func__);

	if (ctx->prepared)
		return 0;

	if (!is_lcm_connected) {
		pr_info("%s is_lcm_connected=%d, not config backlight ic\n", __func__, is_lcm_connected);
	} else {
		pr_info("[%s]:  is_tddi_flag =%d, tddi_gesture_flag =%d, panel_dead_flag =%d\n", 
				__func__,
				dsi->mi_cfg.is_tddi_flag,
				dsi->mi_cfg.tddi_gesture_flag,
				dsi->mi_cfg.panel_dead_flag);
		if (dsi->mi_cfg.is_tddi_flag) {
			if (!dsi->mi_cfg.tddi_gesture_flag || dsi->mi_cfg.panel_dead_flag) {
				pr_info("%s ldo is normal mode!!\n", __func__);
				usleep_range(1200, 1220);
				ktz8866_bl_bias_conf();
				ktz8866_bias_enable(1);
			} else if (dsi->mi_cfg.tddi_gesture_flag) {
				pr_info("%s ldo is TP gesture mode!!\n", __func__);
			}
		} else {
			usleep_range(1200, 1220);
			ktz8866_bl_bias_conf();
			ktz8866_bias_enable(1);
		}
	}

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
	struct mtk_dsi *dsi = mi_get_primary_dsi_display();
	if (ctx->prepared)
		return 0;

	pr_info("%s+\n", __func__);
	msleep(60);
	ctx->reset_gpio = devm_gpiod_get_index(ctx->dev,
		"reset", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	dsi->mi_cfg.feature_val[DISP_FEATURE_CABC] = CABC_OFF;
	pr_info("[%s]:  is_tddi_flag =%d, tddi_gesture_flag =%d, panel_dead_flag =%d\n", 
			__func__,
			dsi->mi_cfg.is_tddi_flag,
			dsi->mi_cfg.tddi_gesture_flag,
			dsi->mi_cfg.panel_dead_flag);
	if (dsi->mi_cfg.is_tddi_flag) {
		if (!dsi->mi_cfg.tddi_gesture_flag || dsi->mi_cfg.panel_dead_flag || check_power_state()) {
			pr_info("%s ldo is normal mode!!\n", __func__);
			gpiod_set_value(ctx->reset_gpio, 0);
			devm_gpiod_put(ctx->dev, ctx->reset_gpio);
			lcm_panel_vddi_disable(ctx);
		} else if (dsi->mi_cfg.tddi_gesture_flag) {
			pr_info("%s ldo is TP gesture mode!!\n", __func__);
			devm_gpiod_put(ctx->dev, ctx->reset_gpio);
		}
	} else {
		gpiod_set_value(ctx->reset_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->reset_gpio);
		//VDDI 1.9V -> 0
		lcm_panel_vddi_disable(ctx);
	}

	return 0;
}

static int lcm_panel_poweron(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mtk_dsi *dsi = mi_get_primary_dsi_display();

	if (ctx->prepared)
		return 0;

	pr_info("%s\n", __func__);
	pr_info("%s: is_tddi_flag =%d, tddi_gesture_flag =%d, panel_dead_flag =%d\n", __func__,
			dsi->mi_cfg.is_tddi_flag,
			dsi->mi_cfg.tddi_gesture_flag,
			dsi->mi_cfg.panel_dead_flag);
	if (dsi->mi_cfg.is_tddi_flag) {
		if (!dsi->mi_cfg.tddi_gesture_flag || dsi->mi_cfg.panel_dead_flag) {
			pr_info("%s ldo is normal mode!!\n", __func__);
			ctx->reset_gpio =
				devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_LOW);
			if (IS_ERR(ctx->reset_gpio)) {
				dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
					__func__, PTR_ERR(ctx->reset_gpio));
				return 0;
			}
			gpiod_set_value(ctx->reset_gpio, 0);
			devm_gpiod_put(ctx->dev, ctx->reset_gpio);

			//VDDI 1.9V
			lcm_panel_vddi_enable(ctx);
			usleep_range(11 * 1000, (11 * 1000)+20);
		} else if (dsi->mi_cfg.tddi_gesture_flag ) {
			pr_info("%s ldo is TP gesture mode!!\n", __func__);
			ctx->reset_gpio =
				devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_LOW);
			if (IS_ERR(ctx->reset_gpio)) {
				dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
					__func__, PTR_ERR(ctx->reset_gpio));
				return 0;
			}
			gpiod_set_value(ctx->reset_gpio, 0);
			usleep_range(1 * 1000, (1* 1000)+20);
			devm_gpiod_put(ctx->dev, ctx->reset_gpio);
		}
	} else {
		ctx->reset_gpio =
			devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_LOW);
		if (IS_ERR(ctx->reset_gpio)) {
			dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
				__func__, PTR_ERR(ctx->reset_gpio));
			return 0;
		}
		gpiod_set_value(ctx->reset_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->reset_gpio);

		//VDDI 1.9V
		lcm_panel_vddi_enable(ctx);
		usleep_range(11 * 1000, (11 * 1000)+20);
	}
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
	usleep_range(20 * 1000, (20* 1000) + 20);
	return 0;
}

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params_30hz = {
	.lcm_index = 1,
	.pll_clk = DATA_RATE / 2,
	.vdo_keep_hs_perline = 1,
	.phy_timcon = {
		.da_hs_exit=0xf,
		.ta_get=0x0e,
	},
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9C,
	},
	.is_cphy = 0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DUAL_PORT,
	.lcm_cmd_if = MTK_PANEL_DUAL_PORT,
	.dsc_params = {
		.enable                =  DSC_ENABLE,
		.dual_dsc_enable       =  DSC_DUAL_DSC_ENABLE,
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
	.data_rate_khz = 1173586,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 90,
	},
	.dyn = {
		.switch_en = 1,
		.vbp = 22,
		.vfp = 6300,
		.hbp = 188,
		.hfp = 188,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	//.prefetch_time = PREFETCH_TIME,
};

static struct mtk_panel_params ext_params_48hz = {
	.lcm_index = 1,
	.pll_clk = DATA_RATE / 2,
	.vdo_keep_hs_perline = 1,
	.phy_timcon = {
		.da_hs_exit=0xf,
		.ta_get=0x0e,
	},
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9C,
	},
	.is_cphy = 0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DUAL_PORT,
	.lcm_cmd_if = MTK_PANEL_DUAL_PORT,
	.dsc_params = {
		.enable                =  DSC_ENABLE,
		.dual_dsc_enable       =  DSC_DUAL_DSC_ENABLE,
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
	.data_rate_khz = 1173586,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 90,
	},
	.dyn = {
		.switch_en = 1,
		.vbp = 22,
		.vfp = 2804,
		.hbp = 188,
		.hfp = 188,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	//.prefetch_time = PREFETCH_TIME,
};

static struct mtk_panel_params ext_params_50hz = {
	.lcm_index = 1,
	.pll_clk = DATA_RATE / 2,
	.vdo_keep_hs_perline = 1,
	.phy_timcon = {
		.da_hs_exit=0xf,
		.ta_get=0x0e,
	},
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9C,
	},
	.is_cphy = 0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DUAL_PORT,
	.lcm_cmd_if = MTK_PANEL_DUAL_PORT,
	.dsc_params = {
		.enable                =  DSC_ENABLE,
		.dual_dsc_enable       =  DSC_DUAL_DSC_ENABLE,
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
	.data_rate_khz = 1173586,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 90,
	},
	.dyn = {
		.switch_en = 1,
		.vbp = 22,
		.vfp = 2570,
		.hbp = 188,
		.hfp = 188,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	//.prefetch_time = PREFETCH_TIME,
};

static struct mtk_panel_params ext_params_60hz = {
	.lcm_index = 1,
	.pll_clk = DATA_RATE / 2,
	.vdo_keep_hs_perline = 1,
	.phy_timcon = {
		.da_hs_exit=0xf,
		.ta_get=0x0e,
	},
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.is_cphy = 0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DUAL_PORT,
	.lcm_cmd_if = MTK_PANEL_DUAL_PORT,
	.dsc_params = {
		.enable                =  DSC_ENABLE,
		.dual_dsc_enable       =  DSC_DUAL_DSC_ENABLE,
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
	.data_rate_khz = 1173586,
#ifdef CONFIG_MI_DISP_FOD_SYNC
  	.bl_sync_enable = 1,
  	.aod_delay_enable = 1,
#endif
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 120,
	},
	.dyn = {
		.switch_en = 1,
		.vbp = 176,
		.vfp = 3422,
		.hbp = 86,
		.hfp = 86,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	//.prefetch_time = PREFETCH_TIME,
};

static struct mtk_panel_params ext_params_90hz = {
	.lcm_index = 1,
	.pll_clk = DATA_RATE / 2,
	.vdo_keep_hs_perline = 1,
	.phy_timcon = {
		.da_hs_exit=0xf,
		.ta_get=0x0e,
	},
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.is_cphy = 0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DUAL_PORT,
	.lcm_cmd_if = MTK_PANEL_DUAL_PORT,
	.dsc_params = {
		.enable                =  DSC_ENABLE,
		.dual_dsc_enable       =  DSC_DUAL_DSC_ENABLE,
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
	.data_rate_khz = 1173586,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 90,
	},
	.dyn = {
		.switch_en = 1,
		.vbp = 22,
		.vfp = 80,
		.hbp = 188,
		.hfp = 188,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	//.prefetch_time = PREFETCH_TIME,
};

static struct mtk_panel_params ext_params_120hz = {
	.lcm_index = 0,
	.pll_clk = DATA_RATE / 2,
	.vdo_keep_hs_perline = 1,
	.phy_timcon = {
		.da_hs_exit=0xf,
		.ta_get=0x0e,
	},
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.is_cphy = 0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DUAL_PORT,
	.lcm_cmd_if = MTK_PANEL_DUAL_PORT,
	.dsc_params = {
		.enable                =  DSC_ENABLE,
		.dual_dsc_enable       =  DSC_DUAL_DSC_ENABLE,
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
	.data_rate_khz = 1173586,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 120,
	},
	.dyn = {
		.switch_en = 1,
		.vbp = 176,
		.vfp = 118,
		.hbp = 86,
		.hfp = 86,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	//.prefetch_time = PREFETCH_TIME,
};

static struct mtk_panel_params ext_params_144hz = {
	.lcm_index = 0,
	.pll_clk = DATA_RATE / 2,
	.vdo_keep_hs_perline = 1,
	.phy_timcon = {
		.da_hs_exit=0xf,
		.ta_get=0x0e,
	},
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.is_cphy = 0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DUAL_PORT,
	.lcm_cmd_if = MTK_PANEL_DUAL_PORT,
	.dsc_params = {
		.enable                =  DSC_ENABLE,
		.dual_dsc_enable       =  DSC_DUAL_DSC_ENABLE,
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
	.data_rate_khz = 1173586,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 165,
	},
	.dyn = {
		.switch_en = 1,
		.vbp = 34,
		.vfp = 570,
		.hbp = 22,
		.hfp = 32,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	//.prefetch_time = PREFETCH_TIME,
};

static struct mtk_panel_params ext_params_165hz = {
	.lcm_index = 0,
	.pll_clk = DATA_RATE / 2,
	.vdo_keep_hs_perline = 1,
	.phy_timcon = {
		.da_hs_exit=0xf,
		.ta_get=0x0e,
	},
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.is_cphy = 0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DUAL_PORT,
	.lcm_cmd_if = MTK_PANEL_DUAL_PORT,
	.dsc_params = {
		.enable                =  DSC_ENABLE,
		.dual_dsc_enable       =  DSC_DUAL_DSC_ENABLE,
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
	.data_rate_khz = 1173586,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 165,
	},
	.dyn = {
		.switch_en = 1,
		.vbp = 34,
		.vfp = 110,
		.hbp = 22,
		.hfp = 32,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	//.prefetch_time = PREFETCH_TIME,
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

	if (dst_fps == 48)
		*ext_param = &ext_params_48hz;
	else if (dst_fps == 50)
		*ext_param = &ext_params_50hz;
	else if (dst_fps == 30)
		*ext_param = &ext_params_30hz;
	else if (dst_fps == 60)
		*ext_param = &ext_params_60hz;
	else if (dst_fps == 90)
		*ext_param = &ext_params_90hz;
	else if (dst_fps == 120)
		*ext_param = &ext_params_120hz;
	else if (dst_fps == 144)
		*ext_param = &ext_params_144hz;
	else if (dst_fps == 165)
		*ext_param = &ext_params_165hz;
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
	unsigned int format = FORMAT_NOLOCK;

	dsi = container_of(connector, struct mtk_dsi, conn);
	pr_info("%s drm_mode_vrefresh = %d, m->hdisplay = %d\n",
		__func__, drm_mode_vrefresh(m_dst), m_dst->hdisplay);

	dst_fps = m_dst ? drm_mode_vrefresh(m_dst) : -EINVAL;

	if (dst_fps == 48) {
		ext->params = &ext_params_48hz;
		ctx->dynamic_fps = 48;
	} else if (dst_fps == 50) {
		ext->params = &ext_params_50hz;
		ctx->dynamic_fps = 50;
	} else if (dst_fps == 30) {
		ext->params = &ext_params_30hz;
		ctx->dynamic_fps = 30;
	} else if (dst_fps == 60) {
		ext->params = &ext_params_60hz;
		ctx->dynamic_fps = 60;
	} else if (dst_fps == 90) {
		ext->params = &ext_params_90hz;
		ctx->dynamic_fps = 90;
	} else if (dst_fps == 120) {
		ext->params = &ext_params_120hz;
		ctx->dynamic_fps = 120;
	} else if (dst_fps == 144) {
		ext->params = &ext_params_144hz;
		ctx->dynamic_fps = 144;
	} else if (dst_fps == 165) {
		ext->params = &ext_params_165hz;
		ctx->dynamic_fps = 165;
	} else {
		pr_err("%s, dst_fps %d\n", __func__, dst_fps);
		ret = -EINVAL;
	}

	if (!ret)
		current_fps = dst_fps;

	if (ctx->dynamic_fps!=60 && ctx->peak_hdr_status) {
		ret = mi_disp_panel_ddic_send_cmd(lcm_peak_off, ARRAY_SIZE(lcm_peak_off), format);
		ctx->peak_hdr_status = 0;
	}else if(ctx->dynamic_fps==60 && !ctx->peak_hdr_status
		&& dsi->mi_cfg.last_bl_level == PEAK_MAX_BL_LEVEL) {
		ret = mi_disp_panel_ddic_send_cmd(lcm_peak_on, ARRAY_SIZE(lcm_peak_on), format);
		ctx->peak_hdr_status = 1;
	}
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

#if 0
static int panel_set_peak_hdr_status(struct mtk_dsi *dsi,dcs_write_gce cb,
	void *handle, unsigned int bl_level)
{
	char peak_f0[] = {0xF0,0x55,0xAA,0x52,0x08,0x08};
	char peak_6f[] = {0x6F,0x70};
	char peak_on_b9[] = {0xB9,0x03};
	char peak_on_5f[] = {0x5F,0x05,0x40};
	char peak_on_51[] = {0x51,0x0F,0xFF};

	char peak_off_b9[] = {0xB9,0x02};
	char peak_off_5f[] = {0x5F,0x00,0x40};
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

	if (bl_level > PEAK_HDR_BL_LEVEL && bl_level <= PEAK_MAX_BL_LEVEL && !ctx->peak_hdr_status
		&& ctx->dynamic_fps ==60)
	{
		cb(dsi, handle, peak_f0, ARRAY_SIZE(peak_f0));
		cb(dsi, handle, peak_6f, ARRAY_SIZE(peak_6f));
		cb(dsi, handle, peak_on_b9, ARRAY_SIZE(peak_on_b9));
		cb(dsi, handle, peak_on_5f, ARRAY_SIZE(peak_on_5f));
		cb(dsi, handle, peak_on_51, ARRAY_SIZE(peak_on_51));
		ctx->gir_status = 1;
		mi_cfg->feature_val[DISP_FEATURE_FLAT_MODE]= 0;
		ctx->peak_hdr_status = 1;
	}

	if (((bl_level != 0 && bl_level <= PEAK_HDR_BL_LEVEL) || ctx->dynamic_fps !=60)
		&& ctx->peak_hdr_status) {
		cb(dsi, handle, peak_f0, ARRAY_SIZE(peak_f0));
		cb(dsi, handle, peak_6f, ARRAY_SIZE(peak_6f));
		cb(dsi, handle, peak_off_b9, ARRAY_SIZE(peak_off_b9));
		cb(dsi, handle, peak_off_5f, ARRAY_SIZE(peak_off_5f));
		ctx->gir_status = 1;
		mi_cfg->feature_val[DISP_FEATURE_FLAT_MODE]= 1;
		ctx->peak_hdr_status = 0;
	}

	pr_info("%s: peak_hdr_status = %d ctx->gir_status = %d\n", __func__, ctx->peak_hdr_status, ctx->gir_status);
err:
	return 1;
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
#endif
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
	unsigned int format = FORMAT_GCE_BLOCK;

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
		//mi_disp_panel_ddic_send_cmd(bl_tb0, ARRAY_SIZE(bl_tb0), format);
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
	if (!panel) {
		pr_err("invalid params\n");
		return;
	}

	if (!is_lcm_connected) {
		pr_info("%s is_lcm_connected=%d, not config backlight ic\n", __func__, is_lcm_connected);
	} else {
		ktz8866_brightness_set(last_non_zero_bl_level);
	}

	pr_info("%s: restore to level = %d\n", __func__, last_non_zero_bl_level);

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

static int lcm_setbacklight_i2c(struct drm_panel *panel, unsigned int level)
{
	pr_info("lcm_setbacklight_i2c level=%d\n", level);
	struct lcm *ctx = panel_to_lcm(panel);
	if (level)
		ctx->prepared = true;
#ifdef CONFIG_FACTORY_BUILD
	if ((level >= 0 && level < 2047) || (ctx->hbm_enabled == false && (level == 2047)))
	{
		if (!is_lcm_connected) {
			pr_info("%s is_lcm_connected=%d, not config backlight ic\n", __func__, is_lcm_connected);
		} else {
			ktz8866_brightness_set(level);
		}
	}
	if (ctx->hbm_enabled)
		ctx->hbm_enabled = false;
#else
	if (!is_lcm_connected) {
		pr_info("%s is_lcm_connected=%d, not config backlight ic\n", __func__, is_lcm_connected);
	} else {
		ktz8866_brightness_set(level);
	}
#endif
	if (level != 0) {
		last_non_zero_bl_level = level;
	}
	return level;
}

void panel_set_cabc_mode(struct drm_panel *panel, int mode)
{
	unsigned int format = FORMAT_LP_MODE;
	struct mtk_dsi *dsi = mi_get_primary_dsi_display();
	if(dsi->mi_cfg.feature_val[DISP_FEATURE_CABC] == mode) {
		pr_info("CABC is the same, return\n");
		return;
	}

	if (mode != CABC_OFF)
		msleep(20);

	if (mode == CABC_OFF) {
		pr_info("panel_set_cabc_mode mode=%d\n", mode);
		mi_disp_panel_ddic_send_cmd(CABC_Off, ARRAY_SIZE(CABC_Off), format);
	} else if (mode == CABC_UI_ON) {
		pr_info("panel_set_cabc_mode mode=%d\n", mode);
		mi_disp_panel_ddic_send_cmd(CABC_UIMODE, ARRAY_SIZE(CABC_UIMODE), format);
	} else	if (mode == CABC_STILL_ON) {
		pr_info("panel_set_cabc_mode mode=%d\n", mode);
		mi_disp_panel_ddic_send_cmd(CABC_STILLMODE, ARRAY_SIZE(CABC_STILLMODE), format);
	} else	if (mode == CABC_MOVIE_ON) {
		pr_info("panel_set_cabc_mode mode=%d\n", mode);
		mi_disp_panel_ddic_send_cmd(CABC_MOVIVEMODE, ARRAY_SIZE(CABC_MOVIVEMODE), format);
	} else {
		pr_info("panel_set_cabc_mode the mode is error\n");
	}
	return;
}

static int panel_get_wp_info(struct drm_panel *panel, char *buf, size_t size)
{
	/* Read 8 bytes from 0xA1 register
	 * 	BIT[0-1] = Lux
	 * 	BIT[2-4] = Wx
	 * 	BIT[5-7] = Wy */
	static uint16_t lux = 0, wx = 0, wy = 0;
	int count = 0;
	u8 rx_buf[8] = {0x00};

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
		{0x5F, 2, {0x01,0x40} },
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

static int panel_get_ic_type(struct drm_panel *panel, u32 *ic_type)
{
	struct lcm *ctx;

	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

	ctx = panel_to_lcm(panel);
	*ic_type = ctx->ic_type;

	return 0;
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
	bl_level = mi_cfg->last_no_zero_bl_level;
	flat_mode = ctx->gir_status;
	format = FORMAT_LP_MODE | FORMAT_BLOCK;

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

	pr_info("%s local hbm_state:%d \n",__func__, lhbm_state);

	switch (lhbm_state) {
	case LOCAL_HBM_OFF_TO_NORMAL:
		pr_info("LOCAL_HBM_NORMAL off set bl %d\n",bl_level);
		mi_disp_panel_ddic_send_cmd(lhbm_off, ARRAY_SIZE(lhbm_off), format);
		ctx->lhbm_en = false;
		break;
	case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE:
		pr_info("LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE set bl %d\n",bl_level);
		if (atomic_read(&doze_enable)) {
			if (mi_cfg->last_bl_level) {
				mi_disp_panel_ddic_send_cmd(lhbm_off, ARRAY_SIZE(lhbm_off), format);
			}else{
				mi_disp_panel_ddic_send_cmd(lhbm_off, ARRAY_SIZE(lhbm_off), format);
			}
		}else{
			mi_disp_panel_ddic_send_cmd(lhbm_off, ARRAY_SIZE(lhbm_off), format);
		} 
		ctx->lhbm_en = false;
		break;
	case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT:
		pr_info("LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT  set bl %d\n",bl_level);
		if (atomic_read(&doze_enable)) {
			if (mi_cfg->last_bl_level) {
				mi_disp_panel_ddic_send_cmd(lhbm_off, ARRAY_SIZE(lhbm_off), format);
			}else{
				mi_disp_panel_ddic_send_cmd(lhbm_off, ARRAY_SIZE(lhbm_off), format);
			}
		}else{
			mi_disp_panel_ddic_send_cmd(lhbm_off, ARRAY_SIZE(lhbm_off), format);
		} 
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
		}
		format = FORMAT_LP_MODE | FORMAT_BLOCK;
		mi_disp_panel_ddic_send_cmd(lhbm_normal_white_200nit, ARRAY_SIZE(lhbm_normal_white_200nit), format);
		ctx->lhbm_en = true;
		break;
	case LOCAL_HBM_NORMAL_GREEN_500NIT:
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		pr_info("LOCAL_HBM_NORMAL_GREEN_500NIt\n");

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

	if (panel_build_id < PANEL_BUILD_ID_P10) {
		pr_err("%s: panel build id is earlier than p1.1\n", __func__);
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

static int lcm_led_i2c_reg_op(char *buffer, int op, int count)
{
	int i, ret = -EINVAL;
	char reg_addr = *buffer;
	char *reg_val = buffer;
	if (reg_val == NULL) {
		pr_err("%s,buffer is null\n", __func__);
		return ret;
	}

	pr_info("%s, op = %d, reg_addr = 0x%x\n", __func__, op, reg_addr);

	if (op == KTZ8866_REG_READ) {
		for (i = 0; i < count; i++) {
			ret = ktz8866_reg_read_bytes(reg_addr, reg_val);
			if (ret <= 0)
				break;
			reg_addr++; // for next reg addr, exp: 0x04h, 0x05h
			reg_val++; // for next buffer params
		}
	} else if (op == KTZ8866_REG_WRITE) {
		ret = ktz8866_reg_write_bytes(reg_addr, *(reg_val + 1));
	}
	return ret;
}
#endif

int panel_get_sn_info(struct drm_panel *panel, char *buf, size_t size)
{
	int count = 0;
	u8 rx_buf[12] = {0x00};
	int i = 0;
	pr_info("%s: %s+\n", __func__, panel_sn_cmdline);
	for (i = 0; i < 12; i++) {
		sscanf(panel_sn_cmdline + i, "%c", &rx_buf[i]);
	}
	for (i = 0; i < 12; i++) {
		buf[i] = rx_buf[i];
		pr_info("%s: rx_buf[%d]= %c\n", __func__, i,  buf[i]);
		count++;
	}
	pr_info("%s: -\n", __func__);

	return count;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.ext_param_set = mtk_panel_ext_param_set,
	.ext_param_get = mtk_panel_ext_param_get,
	//.mode_switch = mode_switch,
	.set_doze_brightness = panel_set_doze_brightness,
	.get_doze_brightness = panel_get_doze_brightness,
	.panel_poweron = lcm_panel_poweron,
	.panel_poweroff = lcm_panel_poweroff,
	.doze_disable = panel_doze_disable,
	.doze_enable = panel_doze_enable,
#ifdef CONFIG_MI_DISP
	.set_backlight_i2c = lcm_setbacklight_i2c,
	.esd_restore_backlight = lcm_esd_restore_backlight,
	.setbacklight_control = lcm_setbacklight_control,
	.led_i2c_reg_op = lcm_led_i2c_reg_op,
	.get_wp_info = panel_get_wp_info,
	.get_panel_info = panel_get_panel_info,
	.get_ic_type = panel_get_ic_type,
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
	.set_cabc_mode = panel_set_cabc_mode,
	.get_sn_info = panel_get_sn_info,
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
	struct drm_display_mode *mode_30, *mode_48, *mode_50, *mode_60, *mode_90, *mode_120, *mode_144, *mode_165;

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

	mode_48 = drm_mode_duplicate(connector->dev, &mode_48hz);
	if (!mode_48) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			mode_48hz.hdisplay, mode_48hz.vdisplay,
			drm_mode_vrefresh(&mode_48hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_48);
	mode_48->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_48);

	mode_30 = drm_mode_duplicate(connector->dev, &mode_30hz);
	if (!mode_30) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			mode_30hz.hdisplay, mode_30hz.vdisplay,
			drm_mode_vrefresh(&mode_30hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_30);
	mode_30->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_30);

	mode_50 = drm_mode_duplicate(connector->dev, &mode_50hz);
	if (!mode_50) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			mode_50hz.hdisplay, mode_50hz.vdisplay,
			drm_mode_vrefresh(&mode_50hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_50);
	mode_50->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_50);

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

	mode_144 = drm_mode_duplicate(connector->dev, &mode_144hz);
	if (!mode_144) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			mode_144hz.hdisplay, mode_144hz.vdisplay,
			drm_mode_vrefresh(&mode_144hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_144);
	mode_144->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_144);

	mode_165 = drm_mode_duplicate(connector->dev, &mode_165hz);
	if (!mode_165) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			mode_165hz.hdisplay, mode_165hz.vdisplay,
			drm_mode_vrefresh(&mode_165hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_165);
	mode_165->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_165);

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
#if defined(CONFIG_MI_DP_AUX_PN_SWAP)
	struct regulator *disp_dpaux;
#endif
	int ret;
	unsigned int lcm_id = 0;

	pr_info("%s p88-42-02-0a-dsc +\n", __func__);

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
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE;

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

	ctx->lcm_id_gpio = devm_gpiod_get_index(dev, "lcm_id", 0, GPIOD_IN);
	if (IS_ERR(ctx->lcm_id_gpio)) {
		dev_err(dev, "%s: cannot get lcm_id-gpios %ld\n",
			__func__, PTR_ERR(ctx->lcm_id_gpio));
		return PTR_ERR(ctx->lcm_id_gpio);
	}
	lcm_id = gpiod_get_value(ctx->lcm_id_gpio);
	is_lcm_connected = !lcm_id;
	pr_info("%s is_lcm_connected=%d, lcm_id=%d\n", __func__, is_lcm_connected, lcm_id);

	ret = gpiod_direction_output(ctx->lcm_id_gpio, 0);
	devm_gpiod_put(dev, ctx->lcm_id_gpio);

	ctx->dvdd_gpio = devm_gpiod_get_index(dev, "dvdd", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->dvdd_gpio)) {
		dev_err(dev, "%s: cannot get dvdd-gpios %ld\n",
			__func__, PTR_ERR(ctx->dvdd_gpio));
		return PTR_ERR(ctx->dvdd_gpio);
	}
	devm_gpiod_put(dev, ctx->dvdd_gpio);

#if defined(CONFIG_MI_DP_AUX_PN_SWAP)
	disp_dpaux = regulator_get(dev, "dpaux");
	if (IS_ERR(disp_dpaux)) { /* handle return value */
		ret = PTR_ERR(disp_dpaux);
		pr_err("get disp_dpaux fail, error: %d\n", ret);
	} else {
		regulator_set_voltage(disp_dpaux, 3000000, 3000000);
		ret = regulator_enable(disp_dpaux);
		if (ret < 0)
			pr_err("enable regulator disp_dpaux fail, ret = %d\n", ret);
	}
#endif

	ctx->prepared = true;
	ctx->enabled = true;
	ctx->hbm_enabled = false;
	ctx->panel_info = panel_name;
	ctx->dynamic_fps = 60;
	ctx->panel_id = panel_id;
	ctx->gir_status = 1;
	ctx->ic_type = 1;
	ctx->max_brightness_clone = MAX_BRIGHTNESS_CLONE;
	ctx->factory_max_brightness = FACTORY_MAX_BRIGHTNESS;
	ctx->peak_hdr_status = 0;
#ifdef CONFIG_MI_DISP
	ext_params_60hz.err_flag_irq_gpio = of_get_named_gpio(
		dev->of_node, "mi,esd-err-irq-gpio",0);
	ext_params_60hz.err_flag_irq_gpio_second = of_get_named_gpio(
		dev->of_node, "mi,esd-err-irq-gpio-second",0);
	ext_params_60hz.err_flag_irq_flags = 0x2002;
	ext_params_60hz.err_flag_irq_flags_second = 0x2002;
	ext_params_48hz.err_flag_irq_gpio = ext_params_60hz.err_flag_irq_gpio;
	ext_params_48hz.err_flag_irq_flags = ext_params_60hz.err_flag_irq_flags;
	ext_params_48hz.err_flag_irq_gpio_second = ext_params_60hz.err_flag_irq_gpio_second;
	ext_params_48hz.err_flag_irq_flags_second = ext_params_60hz.err_flag_irq_flags_second;
	ext_params_30hz.err_flag_irq_gpio = ext_params_60hz.err_flag_irq_gpio;
	ext_params_30hz.err_flag_irq_flags = ext_params_60hz.err_flag_irq_flags;
	ext_params_30hz.err_flag_irq_gpio_second = ext_params_60hz.err_flag_irq_gpio_second;
	ext_params_30hz.err_flag_irq_flags_second = ext_params_60hz.err_flag_irq_flags_second;
	ext_params_50hz.err_flag_irq_gpio = ext_params_60hz.err_flag_irq_gpio;
	ext_params_50hz.err_flag_irq_flags = ext_params_60hz.err_flag_irq_flags;
	ext_params_50hz.err_flag_irq_gpio_second = ext_params_60hz.err_flag_irq_gpio_second;
	ext_params_50hz.err_flag_irq_flags_second = ext_params_60hz.err_flag_irq_flags_second;
	ext_params_90hz.err_flag_irq_gpio = ext_params_60hz.err_flag_irq_gpio;
	ext_params_90hz.err_flag_irq_flags = ext_params_60hz.err_flag_irq_flags;
	ext_params_90hz.err_flag_irq_gpio_second = ext_params_60hz.err_flag_irq_gpio_second;
	ext_params_90hz.err_flag_irq_flags_second = ext_params_60hz.err_flag_irq_flags_second;
	ext_params_120hz.err_flag_irq_gpio = ext_params_60hz.err_flag_irq_gpio;
	ext_params_120hz.err_flag_irq_flags = ext_params_60hz.err_flag_irq_flags;
	ext_params_120hz.err_flag_irq_gpio_second = ext_params_60hz.err_flag_irq_gpio_second;
	ext_params_120hz.err_flag_irq_flags_second = ext_params_60hz.err_flag_irq_flags_second;
	ext_params_144hz.err_flag_irq_gpio = ext_params_60hz.err_flag_irq_gpio;
	ext_params_144hz.err_flag_irq_flags = ext_params_60hz.err_flag_irq_flags;
	ext_params_144hz.err_flag_irq_gpio_second = ext_params_60hz.err_flag_irq_gpio_second;
	ext_params_144hz.err_flag_irq_flags_second = ext_params_60hz.err_flag_irq_flags_second;
	ext_params_165hz.err_flag_irq_gpio = ext_params_60hz.err_flag_irq_gpio;
	ext_params_165hz.err_flag_irq_flags = ext_params_60hz.err_flag_irq_flags;
	ext_params_165hz.err_flag_irq_gpio_second = ext_params_60hz.err_flag_irq_gpio_second;
	ext_params_165hz.err_flag_irq_flags_second = ext_params_60hz.err_flag_irq_flags_second;
#endif
	mipi_volt = 0xC; //480mv
	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &lcm_drm_funcs;

	mutex_init(&ctx->panel_lock);
	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

	get_build_id();
#if defined(CONFIG_MTK_PANEL_EXT)
	//mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params_60hz, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif
	this_panel = &ctx->panel;

	pr_info("%s p88-42-02-0a-dsc-vdo -\n", __func__);
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
	{ .compatible = "p88_42_02_0a_dsc_vdo,lcm", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "p88_42_02_0a_dsc_vdo,lcm",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

module_param_string(oled_wp, oled_wp_cmdline, sizeof(oled_wp_cmdline), 0600);
MODULE_PARM_DESC(oled_wp, "oled_wp=<white_point_info>");

module_param_string(oled_lhbm, oled_lhbm_cmdline, sizeof(oled_lhbm_cmdline), 0600);
MODULE_PARM_DESC(oled_lhbm, "oled_lhbm=<local_hbm_info>");

module_param_string(build_id, buildid_cmdline, sizeof(buildid_cmdline), 0600);
MODULE_PARM_DESC(build_id, "build_id=<buildid_info>");

module_param_string(panel_sn, panel_sn_cmdline, sizeof(panel_sn_cmdline), 0600);
MODULE_PARM_DESC(panel_sn, "panel_sn=<panel_sn_info>");

MODULE_AUTHOR("wangjunbo1 <wangjunbo1@xiaomi.com>");
MODULE_DESCRIPTION("p88_42_02_0a_dsc_vdo oled panel driver");
MODULE_LICENSE("GPL v2");
