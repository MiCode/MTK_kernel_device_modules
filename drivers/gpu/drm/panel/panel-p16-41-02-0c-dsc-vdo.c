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
#include <linux/regmap.h>
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
#endif
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif
#include "../mediatek/mediatek_v2/mi_disp/mi_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_dsi.h"
#include <linux/hqsysfs.h>
#include <linux/pinctrl/pinctrl.h>
#include "../mediatek/mediatek_v2/mi_disp/mi_disp_lhbm.h"
#if IS_ENABLED(CONFIG_MIEV)
#include "../mediatek/mediatek_v2/mi_disp/mi_disp_event.h"
#endif

#define REGFLAG_CMD                 0xFFFA
#define REGFLAG_DELAY               0xFFFC
#define REGFLAG_UDELAY              0xFFFB
#define REGFLAG_END_OF_TABLE        0xFFFD
#define REGFLAG_RESET_LOW           0xFFFE
#define REGFLAG_RESET_HIGH          0xFFFF

#define DATA_RATE                   1140

#define FRAME_WIDTH                 (1280)
#define FRAME_HEIGHT                (2772)

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
#define PHYSICAL_WIDTH              72730
#define PHYSICAL_HEIGHT             157505

static unsigned int rc_buf_thresh[14] = {896, 1792, 2688, 3584, 4480, 5376, 6272, 6720, 7168, 7616, 7744, 7872, 8000, 8064};
static unsigned int range_min_qp[15] = {0, 4, 5, 5, 7, 7, 7, 7, 7, 7, 9, 9, 9, 13, 16};
static unsigned int range_max_qp[15] = {8, 8, 9, 10, 11, 11, 11, 12, 13, 14, 14, 15, 15, 16, 17};
static int range_bpg_ofs[15] = {2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -12, -12, -12, -12};

#define MAX_BRIGHTNESS_CLONE        16383
#define FACTORY_MAX_BRIGHTNESS      8191

#ifdef CONFIG_MI_DISP
extern void mi_dsi_panel_tigger_dimming_work(struct mtk_dsi *dsi);
#endif
extern int get_panel_dead_flag(void);
static struct drm_panel *this_panel;
static int last_bl_level;
static int last_non_zero_bl_level = 511;
static int doze_lbm_bl = 66;
static int doze_hbm_bl = 932;
bool doze_brightness_flag;
static bool lhbm_flag = false;
static bool fod_in_calibration = false;
static int bl_level_for_fod = 0;
static u8 data_reg[4] = {0};
#define REG_DATAR 0xB2
#define REG_DATAG 0xB5
#define REG_DATAB 0xB8
/* P16 code for BUGHQ-9264 by p-zhangyundan at 2025/7/30 start */
static char oled_lhbm_cmdline[80] = {0};
static u8 lhbm_cmdbuf[12] = {0};
static bool lhbm_need_double_read;
/* P16 code for BUGHQ-9264 by p-zhangyundan at 2025/7/30 end */
static const char *panel_name = "panel_name=dsi_p16_41_02_0c_dsc_vdo";

#define GIR_ENABLE 1
#define GIR_DISABLE 0

#define BACKLIGHT_MIN_LEVEL 15
bool nvt_gesture_flag_vox = false;
void set_nvt_gesture_flag_vox(bool en)
{
	nvt_gesture_flag_vox = en;
}
EXPORT_SYMBOL(set_nvt_gesture_flag_vox);

extern struct mtk_dsi * mi_get_primary_dsi_display(void);
struct regmap *map;
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

static struct LCM_setting_table local_hbm_first_cmd[] = {
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x02}},
	{0xBF, 02, {0x00, 0x09}},
	{0x6F, 01, {0x16}},
};

static struct LCM_setting_table local_hbm_normal_white_1200nit[] = {
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x02}},
	{0xD1, 06, {0x22, 0xCC, 0x20, 0x0C, 0x26, 0x98}},
	{0xA9, 12, {0x01, 0x00, 0x8B, 0x01, 0x01, 0x01, 0x01, 0x00, 0x87, 0x00, 0x00, 0x25}},
	{0x51, 02, {0x33, 0x90}},
};

static struct LCM_setting_table local_hbm_normal_white_200nit[] = {
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x02}},
	{0xD1, 06, {0x22, 0xCC, 0x20, 0x0C, 0x26, 0x98}},
	{0xA9, 12, {0x01, 0x00, 0x8B, 0x01, 0x01, 0x01, 0x01, 0x00, 0x87, 0x00, 0x00, 0x25}},
	{0x51, 02, {0x33, 0x90}},
};

static struct LCM_setting_table local_hbm_normal_off[] = {
	{0xA9, 12, {0x01, 0x00, 0x8B, 0x01, 0x01, 0x00, 0x01, 0x00, 0x87, 0x00, 0x00, 0x00}},
	{0x51, 02, {0x07, 0xFF}},
};
static struct LCM_setting_table lcm_aod_hbm[] = {
	{0x51, 02, {0x03, 0xA4}},
};
static struct LCM_setting_table lcm_aod_lbm[] = {
	{0x51, 02, {0x00, 0x42}},
};
/*static struct LCM_setting_table lcm_aod_off[] = {
	{0x51, 02, {0x00, 0x00}},
};*/
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
static struct regulator *disp_vci;
static struct regulator *disp_dvdd;

static int lcm_panel_vci_regulator_init(struct device *dev)
{
	static int panel_vci_regulator_inited;
	int ret = 0;

	if (panel_vci_regulator_inited)
		return ret;

	/* please only get regulator once in a driver */
	disp_vci = regulator_get(dev, "amoled-vci");
	if (IS_ERR(disp_vci)) { /* handle return value */
		ret = PTR_ERR(disp_vci);
		pr_info("get disp_vci fail, error: %d\n", ret);
		return ret;
	}

	panel_vci_regulator_inited = 1;
	return ret; /* must be 0 */
}

static unsigned int panel_vci_start_up = 1;
static int lcm_panel_vci_enable(struct device *dev)
{
	int ret = 0;
	int retval = 0;
	int status = 0;

	pr_info("%s +\n",__func__);

	/* set voltage with min & max*/
	ret = regulator_set_voltage(disp_vci, 3000000, 3000000);
	if (ret < 0)
		pr_info("set voltage disp_vci fail, ret = %d\n", ret);
	retval |= ret;

	status = regulator_is_enabled(disp_vci);
	pr_info("%s regulator_is_enabled = %d, panel_vci_start_up = %d\n", __func__, status, panel_vci_start_up);
	if(!status || panel_vci_start_up){
		/* enable regulator */
		ret = regulator_enable(disp_vci);
		if (ret < 0)
			pr_info("enable regulator disp_vci fail, ret = %d\n", ret);
		panel_vci_start_up = 0;
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
			pr_info("disable regulator disp_vci fail, ret = %d\n", ret);
	}
	retval |= ret;

	pr_info("%s -\n",__func__);

	return retval;
}

static int lcm_panel_dvdd_regulator_init(struct device *dev)
{
	static int dvdd_regulator_inited;
	int ret = 0;

	if (dvdd_regulator_inited)
		return ret;

	/* please only get regulator once in a driver */
	disp_dvdd = regulator_get(dev, "amoled-dvdd");
	if (IS_ERR(disp_dvdd)) { /* handle return value */
		ret = PTR_ERR(disp_dvdd);
		disp_dvdd = NULL;
		pr_info("get disp_dvdd fail, error: %d\n", ret);
		return ret;
	}

	dvdd_regulator_inited = 1;
	return ret; /* must be 0 */
}

static unsigned int panel_dvdd_start_up = 1;
static int lcm_panel_dvdd_enable(struct device *dev)
{
	int ret = 0;
	int retval = 0;
	int status = 0;

	pr_info("%s +\n",__func__);
	if (disp_dvdd == NULL)
		return retval;

	/* set voltage with min & max*/
	ret = regulator_set_voltage(disp_dvdd, 1300000, 1300000);
	if (ret < 0)
		pr_info("set voltage disp_dvdd fail, ret = %d\n", ret);
	retval |= ret;

	status = regulator_is_enabled(disp_dvdd);
	pr_info("%s regulator_is_enabled = %d, panel_dvdd_start_up = %d\n", __func__, status, panel_dvdd_start_up);
	if (!status || panel_dvdd_start_up){
		/* enable regulator */
		ret = regulator_enable(disp_dvdd);
		if (ret < 0)
			pr_info("enable regulator disp_dvdd fail, ret = %d\n", ret);
		panel_dvdd_start_up = 0;
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
	if (disp_dvdd == NULL)
		return retval;

	status = regulator_is_enabled(disp_dvdd);
	pr_info("%s regulator_is_enabled = %d\n", __func__, status);
	if (status){
		ret = regulator_disable(disp_dvdd);
		if (ret < 0)
			pr_info("disable regulator disp_dvdd fail, ret = %d\n", ret);
	}

	retval |= ret;
	pr_info("%s -\n",__func__);

	return retval;
}
static void lcm_panel_init(struct lcm *ctx)
{
	struct mtk_dsi *dsi =  mi_get_primary_dsi_display();
	mutex_lock(&ctx->initialcode_lock);
	pr_info(" %s start \n", __func__);
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n", __func__, PTR_ERR(ctx->reset_gpio));
#if IS_ENABLED(CONFIG_MIEV)
		mi_disp_mievent_str(MI_EVENT_PANEL_HW_RESOURCE_GET_FAILED);
#endif
		return;
	}
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(1 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(2 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(17 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	//notify tp set reg
	if (dsi->mi_cfg.is_tddi_flag) {
		if(dsi->mi_cfg.mi_display_gesture_cb != NULL)
		{
			dsi->mi_cfg.mi_display_gesture_cb();
			pr_info("%s mi_display_gesture_cb call\n", __func__);
		}
	}
	//power down
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x05);
	lcm_dcs_write_seq_static(ctx, 0xCB, 0x33, 0x33, 0x33, 0x33, 0x33);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xEA, 0x91);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x07);
	lcm_dcs_write_seq_static(ctx, 0xEA, 0x01, 0x3F, 0x01, 0x3F);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xEA, 0x91);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x18);
	lcm_dcs_write_seq_static(ctx, 0xEA, 0x01, 0x68, 0x01, 0x68);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x04);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0x0A);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x09);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0x0A);
#ifndef  CONFIG_FACTORY_BUILD
	//acl
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x04);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x06, 0x00, 0x18, 0x00, 0x40, 0x00, 0x70, 0x00, 0xC6, 0x01, 0x76, 0x01, 0xC7, 0x01, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0xAA, 0x00, 0xAC, 0x00, 0xAA);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x17);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x00, 0x00, 0x00, 0x2D, 0x26, 0x33, 0x90, 0x3F, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x21);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9F, 0x00, 0x9F);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x2D);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9F);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x45);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x4B);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x4B);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x31);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x59);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x02, 0x80, 0x05, 0x00, 0x02, 0x80);
	lcm_dcs_write_seq_static(ctx, 0x55, 0x04);
#endif
#ifdef  CONFIG_FACTORY_BUILD
	pr_info(" %s: factory mode round off \n", __func__);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x07);
	lcm_dcs_write_seq_static(ctx, 0xC0, 0x00);
#endif
	//Initial Code
	//	lcm_dcs_write_seq_static(ctx, 0x2F, 0x00); 	//120HZ
	//	lcm_dcs_write_seq_static(ctx, 0x2F, 0x01); 	//90HZ
	//	lcm_dcs_write_seq_static(ctx, 0x2F, 0x02); 	//60HZ
	//================================After
	//Code=====================================
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01);
	//internal
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xC9, 0x60);
	//Auto write SR Quad/protect enable
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x04);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xC7, 0x00, 0x42);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x08);
	lcm_dcs_write_seq_static(ctx, 0xC7, 0x04);
	mdelay(5);
	//MTP Flash lock
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x04);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xC7, 0x11);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xC7, 0x00, 0x42, 0x00, 0x42, 0x00, 0x42);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x09);
	lcm_dcs_write_seq_static(ctx, 0xC7, 0x00, 0x42);
	//-----------------------CMD3----------------------
	lcm_dcs_write_seq_static(ctx, 0xFF, 0xAA, 0x55, 0xA5, 0x84);
	//dpc_Opt
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x10);
	lcm_dcs_write_seq_static(ctx, 0xF8, 0x02);
	//internal
	lcm_dcs_write_seq_static(ctx, 0x6F, 0xA9);
	lcm_dcs_write_seq_static(ctx, 0xF4, 0xF3);
	//GMA conv off
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x27);
	lcm_dcs_write_seq_static(ctx, 0xF7, 0x00);
	//RNDbistmodelatency
	lcm_dcs_write_seq_static(ctx, 0xF2, 0x15);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0xAA, 0x55, 0xA5, 0x80);
	//T_VOLTN_OFF
	//DLY_VOLTN_OFF
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x08);
	lcm_dcs_write_seq_static(ctx, 0xF6, 0x40);
	//T_BGP_OFF
	//DLY_BGP_OFF
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x0A);
	lcm_dcs_write_seq_static(ctx, 0xF6, 0x77);
	//T_VOLTP_OFF
	//DLY_VOLTP_OFF
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x0B);
	lcm_dcs_write_seq_static(ctx, 0xF6, 0x77);
	//T_VDDRX_OFF
	//DLY_VDDRX_OFF
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x0E);
	lcm_dcs_write_seq_static(ctx, 0xF6, 0x77);
	//T_V2I_OFF
	//DLY_V2I_OFF
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x0C);
	lcm_dcs_write_seq_static(ctx, 0xF6, 0x77);
	//T_BIASGEN_OFF
	//DLY_BIASGEN_OFF
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x0D);
	lcm_dcs_write_seq_static(ctx, 0xF6, 0x77);
	//T_IBIAS_OFF
	//DLY_IBIAS_OFF
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0xF6, 0x77);
	//SRLAT_DLY_OUT2_SD[7:0]=4
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x04);
	lcm_dcs_write_seq_static(ctx, 0xF7, 0x04);
	//SWC_ISOP[1:0]=1
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x1B);
	lcm_dcs_write_seq_static(ctx, 0xF4, 0x04);
	//-----------------------CMD1----------------------
	lcm_dcs_write_seq_static(ctx, 0x17, 0x03);
	//GC 120Hz Gamma Sel
	lcm_dcs_write_seq_static(ctx, 0x26, 0x00);
	//Frame rate, FCON0=120Hz
	lcm_dcs_write_seq_static(ctx, 0x2F, 0x00);
	//TEOn
	lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
	//VBP_EXT_FSET0=20
	//VFP_EXT_FSET0=56
	lcm_dcs_write_seq_static(ctx, 0x3B, 0x00, 0x14, 0x00, 0x38);
	//VBP_EXT_FSET1=20
	//VFP_EXT_FSET1=988
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x04);
	lcm_dcs_write_seq_static(ctx, 0x3B, 0x00, 0x14, 0x03, 0xDC);
	//VBP_EXT_FSET2=20
	//VFP_EXT_FSET2=2904
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x08);
	lcm_dcs_write_seq_static(ctx, 0x3B, 0x00, 0x14, 0x0B, 0x58);
	//VBP_EXT_IDLE=20
	//VFP_EXT_IDLE=56
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x10);
	lcm_dcs_write_seq_static(ctx, 0x3B, 0x00, 0x14, 0x00, 0x38);
	//DBV
	lcm_dcs_write_seq_static(ctx, 0x51, 0x00, 0x00);
	//idle_dimming_dbv
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x04);
	lcm_dcs_write_seq_static(ctx, 0x51, 0x3F, 0xF8);
	//BCTRL
	//DD
	lcm_dcs_write_seq_static(ctx, 0x53, 0x20);
	//CCTC ON
	lcm_dcs_write_seq_static(ctx, 0x62, 0x80);
	//vdo_ram_opt
	//set_dsi_mode
	lcm_dcs_write_seq_static(ctx, 0x71, 0x00);
	//DPC_AMBI_TEMP
	lcm_dcs_write_seq_static(ctx, 0x81, 0x01, 0x19);
	// For FPR
	lcm_dcs_write_seq_static(ctx, 0x88, 0x01, 0x02, 0x80, 0x09, 0xD7);
	//Compression_Method[2:0]=3(VESA);, Compression_Method_AOD[2:0]=3(VESA);
	lcm_dcs_write_seq_static(ctx, 0x8D, 0x00, 0x00, 0x04, 0xFF, 0x00, 0x00, 0x0A, 0xD3);
	lcm_dcs_write_seq_static(ctx, 0x03, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x90, 0x03, 0x43);
	//10bpc, 8bpp, slice_h=12
	lcm_dcs_write_seq_static(ctx, 0x91, 0xAB, 0xA8, 0x00, 0x0C, 0xC2, 0x00, 0x02, 0x41, 0x01, 0x38, 0x00, 0x08, 0x08, 0xBB, 0x07, 0x21, 0x10, 0xF0);
	lcm_dcs_write_seq_static(ctx, 0x11);
	mdelay(120);
	lcm_dcs_write_seq_static(ctx, 0x29);
	mutex_unlock(&ctx->initialcode_lock);
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

static int panel_power_down(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("  %s enter\n", __func__);

	if (nvt_gesture_flag_vox) {
		pr_info(" %s tp is gesture mode \n", __func__);
		return 0;
	}

	udelay(2000);
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
#if IS_ENABLED(CONFIG_MIEV)
		mi_disp_mievent_str(MI_EVENT_PANEL_HW_RESOURCE_GET_FAILED);
#endif
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	udelay(1000);

	//VCI 3.0V -> 0
	lcm_panel_vci_disable(ctx->dev);
	udelay(1000);

	//VDD 1.35V -> 0
	lcm_panel_dvdd_disable(ctx->dev);
	udelay(1000);

	// tp_spi_cs -> 0
	if (pinctrl_select_state(ctx->pctrl, ctx->gpio_cs_low)) {
#if IS_ENABLED(CONFIG_MIEV)
		mi_disp_mievent_str(MI_EVENT_PANEL_HW_RESOURCE_GET_FAILED);
#endif
		pr_err("%s pinctrl_select_state ctx->gpio_cs_low fail\n", __func__);
	}

	udelay(1000);

	//VDDI 1.8V -> 0
	ctx->vddi_gpio = devm_gpiod_get(ctx->dev, "vddi", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vddi_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vddi_gpio %ld\n",
			__func__, PTR_ERR(ctx->vddi_gpio));
#if IS_ENABLED(CONFIG_MIEV)
		mi_disp_mievent_str(MI_EVENT_PANEL_HW_RESOURCE_GET_FAILED);
#endif
		return PTR_ERR(ctx->vddi_gpio);
	}
	gpiod_set_value(ctx->vddi_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->vddi_gpio);
	udelay(1000);
	regmap_update_bits(map, 0x149f, 0x2, 0x2);
	return 0;
}

static int panel_init_power(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	regmap_update_bits(map, 0x149e, 0x2, 0x2);
	//VDDI 1.8V
	ctx->vddi_gpio = devm_gpiod_get(ctx->dev, "vddi", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vddi_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vddi_gpio %ld\n",
			__func__, PTR_ERR(ctx->vddi_gpio));
#if IS_ENABLED(CONFIG_MIEV)
		mi_disp_mievent_str(MI_EVENT_PANEL_HW_RESOURCE_GET_FAILED);
#endif
		return PTR_ERR(ctx->vddi_gpio);
	}
	gpiod_set_value(ctx->vddi_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->vddi_gpio);
	udelay(1000);

	//tp_spi_cd -> 1
	if ( pinctrl_select_state(ctx->pctrl, ctx->spi_cs_high)) {
#if IS_ENABLED(CONFIG_MIEV)
		mi_disp_mievent_str(MI_EVENT_PANEL_HW_RESOURCE_GET_FAILED);
#endif
		pr_err("%s pinctrl_select_state ctx->spi_cs_high fail\n", __func__);
	}

	udelay(2000);

	//VDD 1.2V
	lcm_panel_dvdd_enable(ctx->dev);
	udelay(1000);

	//VCI 3.0V
	lcm_panel_vci_enable(ctx->dev);

	udelay(10000);
	pr_info(" lcm %s\n", __func__);
	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info(" %s start \n", __func__);
	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, 0xA9, 0x02, 0x03, 0xC6, 0x2D, 0x2E, 0x00, 0x0A, 0x01, 0x00, 0x28, 0x00, 0x00, 0x00, 0x02, 0x0A, 0xD0, 0x03, 0x03, 0xA1);
	lcm_dcs_write_seq_static(ctx, 0xA9, 0x02, 0x0A, 0xD0, 0x00, 0x00, 0x04);
	msleep(50);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(150);

	ctx->error = 0;
	ctx->prepared = false;
	lhbm_flag = false;

	return 0;
}
static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("  %s enter\n", __func__);
	if (ctx->prepared)
		return 0;

	pr_info(" %s init\n", __func__);
	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif

#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

	pr_info(" %s exit\n", __func__);
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

#define HFP (104)
#define HSA (4)
#define HBP (20)
#define VFP (56)
#define VSA (2)
#define VBP (18)
#define VAC (2772)
#define HAC (1280)

#define MODE_0_FPS 60
#define MODE_0_VFP 2904
#define MODE_0_HFP HFP
#define MODE_0_DATA_RATE DATA_RATE

#define MODE_1_FPS 90
#define MODE_1_VFP 988
#define MODE_1_HFP HFP
#define MODE_1_DATA_RATE DATA_RATE

#define MODE_2_FPS 120
#define MODE_2_VFP 56
#define MODE_2_HFP HFP
#define MODE_2_DATA_RATE DATA_RATE

#define MODE_3_FPS 30
#define MODE_3_VFP 56
#define MODE_3_HFP 1770
#define MODE_3_DATA_RATE DATA_RATE

static u32 fake_heigh = 2772;
static u32 fake_width = 1280;
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

static int panel_get_max_brightness_clone(struct drm_panel *panel, u32 *max_brightness_clone)
{
	struct lcm *ctx;
	pr_info("%s +\n", __func__);
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
	pr_info("%s +\n", __func__);
	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}
	ctx = panel_to_lcm(panel);
	*max_brightness_clone = ctx->factory_max_brightness;
	return 0;
}
/* P16 code for BUGP16-6967 by p-zhangyundan at 2025/06/30 start */
static bool is_diming_on = false;
#define BACKLIGHT_FOR_DIMING 11380
#define BACKLIGHT_FOR_DIMING_END 13200
/* P16 code for BUGP16-6967 by p-zhangyundan at 2025/06/30 end */
static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_tb0[] = {0x51, 0x00, 0x00};
	char diming_on[] = {0x53, 0x28};
	char diming_off[] = {0x53, 0x20};
	struct lcm *ctx = panel_to_lcm(this_panel);
	struct mtk_dsi *mtk_dsi = (struct mtk_dsi *)dsi;

	pr_info(" %s start \n", __func__);

	if ((0 < level) && (level < BACKLIGHT_MIN_LEVEL))
		level = BACKLIGHT_MIN_LEVEL;

	if (level) {
		bl_tb0[1] = (level >> 8) & 0xFF;
		bl_tb0[2] = level & 0xFF;
	}

	if (!cb)
		return -1;

	pr_info("%s: last_bl_level = %d,level = %d\n", __func__, last_bl_level, level);
	last_bl_level = level;
	mtk_dsi->mi_cfg.last_bl_level = level;
	/* P16 code for BUGP16-6967 by p-zhangyundan at 2025/06/30 start */
	if (level > BACKLIGHT_FOR_DIMING && level <= BACKLIGHT_FOR_DIMING_END && is_diming_on == false) {
		cb(dsi, handle, diming_on, ARRAY_SIZE(diming_on));
		is_diming_on = true;
	} else if ((level <= BACKLIGHT_FOR_DIMING || level > BACKLIGHT_FOR_DIMING_END) && is_diming_on == true) {
		cb(dsi, handle, diming_off, ARRAY_SIZE(diming_off));
		is_diming_on = false;
	}
	/* P16 code for BUGP16-6967 by p-zhangyundan at 2025/06/30 end */
	if (lhbm_flag || fod_in_calibration) {
		pr_info("panel skip set backlight %d due to lhbm or fod calibration\n", level);
        } else {
		mutex_lock(&ctx->panel_lock);
		cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
		mutex_unlock(&ctx->panel_lock);
	}

	if (level != 0)
		last_non_zero_bl_level = level;

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
	.cust_esd_check = 1,
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
	.ssc_enable = 0,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
	//.vfp_low_power = 820,//idle 90hz
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 60,
		.cmds_counts_switch_fps = 30,
		.short_dfps_cmds_counts = 1,
		.short_dfps_cmds_start_index = 1,
#ifdef CONFIG_FACTORY_BUILD
		.long_dfps_cmds_counts = 6,
#else
		.long_dfps_cmds_counts = 2,
		.ext_cmd_start = 1,
		.ext_cmd_counts = 5,
#endif
		.dfps_cmd_table[0] = {0, 2, {0x38, 0x00}},
		.dfps_cmd_table[1] = {0, 2, {0x2F, 0x02}},
		.dfps_cmd_table[2] = {0, 6, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x0A}},
		.dfps_cmd_table[3] = {0, 2, {0x6F, 0x03}},
		.dfps_cmd_table[4] = {0, 2, {0xD0, 0x71}},
		.dfps_cmd_table[5] = {0, 2, {0xD0, 0x04}},
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.change_fps_by_vfp_send_cmd = 1,
	.vdo_per_frame_lp_enable = 1,
};
static struct mtk_panel_params ext_params_mode_1 = {
	.lcm_index = 0,
	.pll_clk = DATA_RATE/2,
	.cust_esd_check = 1,
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
	.ssc_enable = 0,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
	//.vfp_low_power = 820,//idle 90hz
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 90,
		.cmds_counts_switch_fps = 30,
		.short_dfps_cmds_counts = 1,
		.short_dfps_cmds_start_index = 1,
#ifdef CONFIG_FACTORY_BUILD
		.long_dfps_cmds_counts = 6,
#else
		.long_dfps_cmds_counts = 2,
		.ext_cmd_start = 1,
		.ext_cmd_counts = 5,
#endif
		.dfps_cmd_table[0] = {0, 2, {0x38, 0x00}},
		.dfps_cmd_table[1] = {0, 2, {0x2F, 0x01}},
		.dfps_cmd_table[2] = {0, 6, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x0A}},
		.dfps_cmd_table[3] = {0, 2, {0x6F, 0x03}},
		.dfps_cmd_table[4] = {0, 2, {0xD0, 0x71}},
		.dfps_cmd_table[5] = {0, 2, {0xD0, 0x04}},
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.change_fps_by_vfp_send_cmd = 1,
	.vdo_per_frame_lp_enable = 1,
};
static struct mtk_panel_params ext_params_mode_2 = {
	.lcm_index = 0,
	.pll_clk = DATA_RATE/2,
	.cust_esd_check = 1,
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
	.ssc_enable = 0,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
	//.vfp_low_power = 820,//idle 90hz
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 120,
		.cmds_counts_switch_fps = 30,
		.short_dfps_cmds_counts = 1,
		.short_dfps_cmds_start_index = 1,
#ifdef CONFIG_FACTORY_BUILD
		.long_dfps_cmds_counts = 6,
#else
		.long_dfps_cmds_counts = 2,
		.ext_cmd_start = 1,
		.ext_cmd_counts = 5,
#endif
		.dfps_cmd_table[0] = {0, 2, {0x38, 0x00}},
		.dfps_cmd_table[1] = {0, 2, {0x2F, 0x00}},
		.dfps_cmd_table[2] = {0, 6, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x0A}},
		.dfps_cmd_table[3] = {0, 2, {0x6F, 0x03}},
		.dfps_cmd_table[4] = {0, 2, {0xD0, 0x71}},
		.dfps_cmd_table[5] = {0, 2, {0xD0, 0x04}},
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.change_fps_by_vfp_send_cmd = 1,
	.vdo_per_frame_lp_enable = 1,
};
static struct mtk_panel_params ext_params_mode_3 = {
	.lcm_index = 0,
	.pll_clk = DATA_RATE/2,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
#ifdef CONFIG_FACTORY_BUILD
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0xdc,
	},
#else
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
#endif
	.ssc_enable = 0,
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
	.ssc_enable = 0,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
	//.vfp_low_power = 820,//idle 90hz
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 30,
		.cmds_counts_switch_fps = 30,
		.short_dfps_cmds_counts = 3,
		.short_dfps_cmds_start_index = 0,
		.long_dfps_cmds_counts = 3,
#ifndef CONFIG_FACTORY_BUILD
		.ext_cmd_start = 3,
		.ext_cmd_counts = 4,
#endif
		.dfps_cmd_table[0] = {0, 2, {0x39, 0x00}},
		.dfps_cmd_table[1] = {0, 2, {0x6F, 0x04}},
		.dfps_cmd_table[2] = {0, 3, {0x51, 0x3F, 0xF8}},
#ifndef CONFIG_FACTORY_BUILD
		.dfps_cmd_table[3] = {0, 6, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x0A}},
		.dfps_cmd_table[4] = {0, 2, {0x6F, 0x03}},
		.dfps_cmd_table[5] = {0, 2, {0xD0, 0x70}},
		.dfps_cmd_table[6] = {0, 2, {0xD0, 0x04}},
#endif
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.change_fps_by_vfp_send_cmd = 1,
	.vdo_per_frame_lp_enable = 1,
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
	} else if (drm_mode_vrefresh(m) == MODE_2_FPS) {
		ext->params = &ext_params_mode_2;
	} else if (drm_mode_vrefresh(m) == MODE_3_FPS) {
		ext->params = &ext_params_mode_3;
		/* P16 code for BUGP16-1609 by p-zhangyundan at 2025/5/8 start */
		if (ctx->doze_brightness == DOZE_BRIGHTNESS_HBM) {
			ext_params_mode_3.dyn_fps.dfps_cmd_table[2].para_list[1] = 0x3F;
			ext_params_mode_3.dyn_fps.dfps_cmd_table[3].para_list[2] = 0xF8;
		} else if (ctx->doze_brightness == DOZE_BRIGHTNESS_LBM) {
			ext_params_mode_3.dyn_fps.dfps_cmd_table[2].para_list[1] = 0x05;
			ext_params_mode_3.dyn_fps.dfps_cmd_table[2].para_list[2] = 0x55;
		}
		/* P16 code for BUGP16-1609 by p-zhangyundan at 2025/5/8 end */
	} else {
		ret = 1;
	}

	if (!ret)
		ctx->dynamic_fps = drm_mode_vrefresh(m);
	return ret;
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
		lcm_dcs_write_seq_static(ctx, 0x5F, 0x00);
		ctx->gir_status = GIR_ENABLE;
	}
err:
	return ret;
}
static int panel_set_gir_off(struct drm_panel *panel)
{
	struct lcm *ctx = NULL;
	int ret = 0;
	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		ret = -1;
		goto err;
	}
	ctx = panel_to_lcm(panel);
	pr_info("%s: + ctx->gir_status = %d \n", __func__, ctx->gir_status);
	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
	} else {
#ifdef CONFIG_FACTORY_BUILD
		pr_info("%s:factory skip set gir: %d,return \n", __func__, ctx->gir_status);
#else
		lcm_dcs_write_seq_static(ctx, 0x5F, 0x01);
		ctx->gir_status = GIR_DISABLE;
#endif

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
	/* P16 code for BUGP16-188 by p-zhangyundan at 2025/4/17 start */
	pr_info("HBM contorled by backlight %d\n", level);
	return 0;
	/* P16 code for BUGP16-188 by p-zhangyundan at 2025/4/17 end */
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
	mi_disp_panel_ddic_send_cmd(fod_calibration, ARRAY_SIZE(fod_calibration), false);
	pr_info("backlight_for_calibration backlight %d\n", bl_level);
	return 0;
}

static const long lhbm_white_high_ratio[3][34] = {
        {
                1089488081281750, 1089878858929250, 1089683470105500, 1089878858929300, 1089683470105550,
                1090074247753050, 1090074247753050, 1087924970691650, 1082258694802650, 1077960140679950,
                1071121531848400, 1067995310668250, 1064282923016800, 1060961313012900, 1059398202422850,
                1056271981242700, 1054122704181300, 1041617819460700, 1033606877686600, 1026182102383750,
                1018171160609600, 1010746385306750, 1003321610003900, 996483001172335,  989058225869480,
                980265728800310,  973427119968740,  966783899960925,  960140679953105,  954083626416565,
                948221961703790,  941383352872220,  935912465806960,  933567799921845
        },
        {
                1079310344827550, 1079094827586200, 1079525862068950, 1079525862068950, 1079525862068950,
                1079956896551700, 1079741379310300, 1078663793103450, 1073060344827600, 1069396551724150,
                1063577586206900, 1060560344827550, 1057543103448250, 1054310344827600, 1053232758620700,
                1050646551724100, 1049137931034500, 1036853448275850, 1029094827586200, 1021336206896550,
                1014439655172400, 1006681034482750, 999784482758610,  992672413793105,  985775862068965,
                977586206896555,  970689655172415,  964224137931035,  957543103448280,  951293103448275,
                945474137931035,  939224137931035,  933836206896550,  931465517241380
        },
        {
                1089409722222200, 1089062500000000, 1088888888888900, 1089930555555600, 1089756944444450,
                1089583333333350, 1089409722222250, 1088020833333350, 1082465277777800, 1078125000000000,
                1072569444444450, 1068923611111150, 1065798611111100, 1062500000000000, 1061284722222200,
                1057465277777750, 1055381944444400, 1043055555555550, 1034722222222200, 1026562500000000,
                1019270833333350, 1011284722222250, 1003993055555550, 996354166666670,  989236111111115,
                980902777777780,  973437500000000,  967187500000000,  960416666666665,  954513888888890,
                947916666666665,  941319444444445,  936111111111110,  933333333333335
          },
};

static const long lhbm_white_low_ratio[3][34] = {
        {
                826299335678000, 826299335678000, 826299335678000, 826690113325520, 826299335678000,
                826103946854240, 825908558030480, 825517780382960, 820828448612740, 818874560375145,
                814380617428685, 811645173896055, 809105119187185, 806760453302075, 805588120359515,
                804220398593200, 801875732708085, 792497069167645, 787026182102385, 781555295037125,
                775693630324345, 769636576787810, 764165689722550, 758890191481050, 753614693239545,
                746776084407975, 741500586166470, 736420476748725, 731535756154750, 726846424384525,
                722352481438060, 717467760844080, 713169206721375, 711215318483785
        },
        {
                842025862068965, 842241379310340, 842025862068965, 842456896551720, 842241379310340,
                841594827586205, 841594827586210, 840948275862070, 836422413793105, 834051724137935,
                829525862068965, 826724137931035, 824137931034485, 821551724137935, 820474137931035,
                818534482758625, 816810344827585, 807327586206895, 801724137931035, 795905172413795,
                790086206896550, 784051724137930, 778448275862070, 773060344827590, 767887931034480,
                761206896551725, 756034482758620, 750862068965515, 746120689655175, 741594827586205,
                736637931034480, 732327586206900, 727586206896550, 725862068965515
        },
        {
                821354166666670, 821180555555560, 821354166666670, 821527777777780, 821354166666670,
                821180555555555, 820833333333330, 820659722222220, 816145833333330, 813888888888885,
                809548611111110, 807118055555555, 804687500000000, 802430555555555, 801388888888890,
                799826388888885, 797743055555555, 788020833333330, 782638888888890, 777083333333330,
                771180555555555, 765104166666665, 759548611111115, 754513888888890, 749305555555555,
                742534722222220, 737326388888890, 732118055555555, 727604166666665, 722916666666665,
                718055555555560, 713368055555555, 709027777777775, 707465277777780
        }
};

enum lhbm_cmd_type {
	TYPE_WHITE_1200 = 0,
	TYPE_WHITE_750,
	TYPE_WHITE_500,
	TYPE_WHITE_200,
	TYPE_GREEN_500,
	TYPE_LHBM_OFF
};

int mi_disp_panel_read_fod_reg(struct lcm *ctx, u8 reg)
{
	int ret = 0;
	unsigned char rx_buf[4] = {0x00};
	struct mtk_ddic_dsi_msg cmds[] = {
		{
			.channel = 0,
			.tx_cmd_num = 1,
			.rx_cmd_num = 1,
			.type[0] = 0x06,
			.tx_buf[0] = &reg,
			.tx_len[0] = 1,
			.rx_buf[0] = &rx_buf[0],
			.rx_len[0] = 4,
		}
	};
	ret = mtk_ddic_dsi_read_cmd(&cmds[0]);
	data_reg[0] = rx_buf[0];
	data_reg[1] = rx_buf[1];
	data_reg[2] = rx_buf[2];
	data_reg[3] = rx_buf[3];
	pr_info(" %s reg data =0x%x,0x%x,0x%x,0x%x\n", __func__,data_reg[0], data_reg[1],data_reg[2], data_reg[3]);
	return ret;
}
/* P16 code for BUGHQ-9264 by p-zhangyundan at 2025/7/30 start */
void lcm_get_lhbm_info(void)
{
	int i = 0;
	int lhbm_1200nit = 0;
	int lhbm_200nit = 0;
	pr_info ("oled_lhbm_cmdline = %s\n", oled_lhbm_cmdline);
	for (i = 0; i < 12; i++) {
		sscanf(oled_lhbm_cmdline + 2 * i, "%02hhx", &lhbm_cmdbuf[i]);
		pr_info ("lhbm_cmdbuf = 0x%x\n", lhbm_cmdbuf[i]);
	}
	lhbm_1200nit = lhbm_cmdbuf[0] |lhbm_cmdbuf[1] |lhbm_cmdbuf[2]|lhbm_cmdbuf[3]|lhbm_cmdbuf[4]|lhbm_cmdbuf[5];
	lhbm_200nit = lhbm_cmdbuf[6] |lhbm_cmdbuf[7] |lhbm_cmdbuf[8]|lhbm_cmdbuf[9]|lhbm_cmdbuf[10]|lhbm_cmdbuf[11];
	if ((lhbm_1200nit == 0) || (lhbm_200nit == 0)) {
		pr_info("%s:lhbm info get fail!\n", __func__);
		lhbm_need_double_read = true;
	} else {
		lhbm_need_double_read = false;
	}
}
/* P16 code for BUGHQ-9264 by p-zhangyundan at 2025/7/30 end */
static int mi_disp_panel_update_lhbm_reg(struct drm_panel *panel, enum lhbm_cmd_type type, int bl_lvl)
{
	int ret= 0, bl_index = 0;
	int tempr = 0, tempg = 0, tempb = 0;
	unsigned char datar[2] = {0x00, 0x00};
	unsigned char datag[2] = {0x00, 0x00};
	unsigned char datab[2] = {0x00, 0x00};
	u8 bl_tb[2] = {0};

	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);

	pr_info(" %s (%d)start \n", __func__, bl_lvl);
	if(!dsi) {
		pr_err("%s:panel is NULL\n", __func__);
		return -1;
	}
	if (lhbm_need_double_read == true) {
	//send cmd & read back B2/B5/B8
	if (type == TYPE_LHBM_OFF) {
		if (bl_lvl == 0)
			bl_lvl = last_bl_level;
		bl_tb[0] = (bl_lvl >> 8) & 0xFF;
		bl_tb[1] = bl_lvl & 0xFF;
		local_hbm_normal_off[1].para_list[0] = bl_tb[0];
		local_hbm_normal_off[1].para_list[1] = bl_tb[1];
		return 0;
        }

	mi_disp_panel_ddic_send_cmd(local_hbm_first_cmd, ARRAY_SIZE(local_hbm_first_cmd), false);
	ret = mi_disp_panel_read_fod_reg(ctx, REG_DATAR);
	if (ret < 0) {
		pr_err("%s error\n", __func__);
		return 0;
	}
	datar[0] = data_reg[0];
	datar[1] = data_reg[1];

	mi_disp_panel_ddic_send_cmd(local_hbm_first_cmd, ARRAY_SIZE(local_hbm_first_cmd), false);
	ret = mi_disp_panel_read_fod_reg(ctx, REG_DATAG);
	if (ret < 0) {
		pr_err("%s error\n", __func__);
		return 0;
	}
	datag[0] = data_reg[0];
	datag[1] = data_reg[1];

	mi_disp_panel_ddic_send_cmd(local_hbm_first_cmd, ARRAY_SIZE(local_hbm_first_cmd), false);
	ret = mi_disp_panel_read_fod_reg(ctx, REG_DATAB);
	if (ret < 0) {
		pr_err("%s error\n", __func__);
		return 0;
	}
	datab[0] = data_reg[0];
	datab[1] = data_reg[1];
	} else {
		/* P16 code for BUGHQ-9264 by p-zhangyundan at 2025/7/30 start */
		switch (type) {
		case TYPE_WHITE_1200:
			datar[0] = lhbm_cmdbuf[0];
			datar[1] = lhbm_cmdbuf[1];
			datag[0] = lhbm_cmdbuf[2];
			datag[1] = lhbm_cmdbuf[3];
			datab[0] = lhbm_cmdbuf[4];
			datab[1] = lhbm_cmdbuf[5];
		break;
		case TYPE_WHITE_200:
			datar[0] = lhbm_cmdbuf[6];
			datar[1] = lhbm_cmdbuf[7];
			datag[0] = lhbm_cmdbuf[8];
			datag[1] = lhbm_cmdbuf[9];
			datab[0] = lhbm_cmdbuf[10];
			datab[1] = lhbm_cmdbuf[11];
		break;
		case TYPE_LHBM_OFF:
			if (bl_lvl == 0)
				bl_lvl = last_bl_level;
			bl_tb[0] = (bl_lvl >> 8) & 0xFF;
			bl_tb[1] = bl_lvl & 0xFF;
			local_hbm_normal_off[1].para_list[0] = bl_tb[0];
			local_hbm_normal_off[1].para_list[1] = bl_tb[1];
			return 0;
		break;
		default:
			pr_err("unsuppport cmd \n");
			return -1;
		}
		/* P16 code for BUGHQ-9264 by p-zhangyundan at 2025/7/30 end */
	}

	ret = mi_disp_panel_read_fod_reg(ctx, 0x52);
	if (ret < 0) {
		pr_err("%s error\n", __func__);
		return 0;
	} else {
		bl_lvl = (data_reg[2] << 8) | data_reg[3];
	}

	pr_info(" %s bl_level = %d \n", __func__, bl_lvl);
	if (0x0 <= bl_lvl && bl_lvl <= 0x00F) bl_index = 0;
	else if (bl_lvl <= 0x0022) bl_index = 1;
	else if (bl_lvl <= 0x004C) bl_index = 2;
	else if (bl_lvl <= 0x00A6) bl_index = 3;
	else if (bl_lvl <= 0x0164) bl_index = 4;
	else if (bl_lvl <= 0x0438) bl_index = 5;
	else if (bl_lvl <= 0x0AA1) bl_index = 6;
	else if (bl_lvl <= 0x0DAF) bl_index = 7;
	else if (bl_lvl <= 0x10BB) bl_index = 8;
	else if (bl_lvl <= 0x13C9) bl_index = 9;
	else if (bl_lvl <= 0x16D7) bl_index = 10;
	else if (bl_lvl <= 0x19E4) bl_index = 11;
	else if (bl_lvl <= 0x1CF1) bl_index = 12;
	else if (bl_lvl <= 0x1FFE) bl_index = 13;
	else if (bl_lvl <= 0x2556) bl_index = 14;
	else if (bl_lvl <= 0x2AAD) bl_index = 15;
	else if (bl_lvl <= 0x2B33) bl_index = 16;
	else if (bl_lvl <= 0x2BB9) bl_index = 17;
	else if (bl_lvl <= 0x2C3F) bl_index = 18;
	else if (bl_lvl <= 0x2CC5) bl_index = 19;
	else if (bl_lvl <= 0x2D4A) bl_index = 20;
	else if (bl_lvl <= 0x2DD0) bl_index = 21;
	else if (bl_lvl <= 0x2E56) bl_index = 22;
	else if (bl_lvl <= 0x2EDC) bl_index = 23;
	else if (bl_lvl <= 0x2F61) bl_index = 24;
	else if (bl_lvl <= 0x2FE7) bl_index = 25;
	else if (bl_lvl <= 0x306D) bl_index = 26;
	else if (bl_lvl <= 0x30F3) bl_index = 27;
	else if (bl_lvl <= 0x3178) bl_index = 28;
	else if (bl_lvl <= 0x31FE) bl_index = 29;
	else if (bl_lvl <= 0x3284) bl_index = 30;
	else if (bl_lvl <= 0x330A) bl_index = 31;
	else if (bl_lvl <= 0x338F) bl_index = 32;
	else if (bl_lvl <= 0x3FFF) bl_index = 33;

	switch (type) {
	case TYPE_WHITE_1200:
		tempr = ((datar[0] << 8) | datar[1]) * (lhbm_white_high_ratio[0][bl_index] / 1000000000) * 4 / 1000000;
		tempg = ((datag[0] << 8) | datag[1]) * (lhbm_white_high_ratio[1][bl_index] / 1000000000) * 4 / 1000000;
		tempb = ((datab[0] << 8) | datab[1]) * (lhbm_white_high_ratio[2][bl_index] / 1000000000) * 4 / 1000000;
		local_hbm_normal_white_1200nit[1].para_list[0] = (tempr >> 8) & 0xff;
		local_hbm_normal_white_1200nit[1].para_list[1] = tempr & 0xFF;
		local_hbm_normal_white_1200nit[1].para_list[2] = (tempg >> 8) & 0xff;
		local_hbm_normal_white_1200nit[1].para_list[3] = tempg & 0xFF;
		local_hbm_normal_white_1200nit[1].para_list[4] = (tempb >> 8) & 0xff;
		local_hbm_normal_white_1200nit[1].para_list[5] = tempb & 0xFF;
		local_hbm_normal_white_1200nit[3].para_list[0] = (bl_lvl >> 8) & 0xff;
		local_hbm_normal_white_1200nit[3].para_list[1] = bl_lvl & 0xFF;
		break;
	case TYPE_WHITE_200:
		tempr = ((datar[0] << 8) | datar[1]) * (lhbm_white_low_ratio[0][bl_index] / 1000000000) * 4 / 1000000;
		tempg = ((datag[0] << 8) | datag[1]) * (lhbm_white_low_ratio[1][bl_index] / 1000000000) * 4 / 1000000;
		tempb = ((datab[0] << 8) | datab[1]) * (lhbm_white_low_ratio[2][bl_index] / 1000000000) * 4 / 1000000;
		local_hbm_normal_white_200nit[1].para_list[0] = (tempr >> 8) & 0xff;
		local_hbm_normal_white_200nit[1].para_list[1] = tempr & 0xFF;
		local_hbm_normal_white_200nit[1].para_list[2] = (tempg >> 8) & 0xff;
		local_hbm_normal_white_200nit[1].para_list[3] = tempg & 0xFF;
		local_hbm_normal_white_200nit[1].para_list[4] = (tempb >> 8) & 0xff;
		local_hbm_normal_white_200nit[1].para_list[5] = tempb & 0xFF;
		local_hbm_normal_white_200nit[3].para_list[0] = (bl_lvl >> 8) & 0xff;
		local_hbm_normal_white_200nit[3].para_list[1] = bl_lvl & 0xFF;
		break;
	case TYPE_LHBM_OFF:
		break;
	default:
		pr_err("unsuppport cmd \n");
		return -1;
	}
	pr_info(" %s tempr = %d, tempg = %d, tempb = %d\n", __func__, tempr, tempg, tempb);
	return 0;
}

static int panel_set_lhbm_fod(struct mtk_dsi *dsi, enum local_hbm_state lhbm_state)
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

	/*if (bl_level > hbm_level) {
		if((lhbm_state == LOCAL_HBM_OFF_TO_NORMAL) || (lhbm_state == LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT) || (lhbm_state == LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE)) {
			pr_info("exit lhbm to hbm!\n");
                } else {
			pr_info("The bl_level is %d,exit hbm mode to normal!\n", bl_level);
			bl_level = hbm_level;
			mi_disp_panel_ddic_send_cmd(exit_hbm_to_normal, ARRAY_SIZE(exit_hbm_to_normal), false);
		}
	}
*/
	pr_info("%s local hbm_state :%d\n", __func__, lhbm_state);
	switch (lhbm_state) {
	case LOCAL_HBM_OFF_TO_NORMAL://0
	case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT://10
	case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE://11
		pr_info("LOCAL_HBM_NORMAL off\n");
		lhbm_flag = false;
		mutex_lock(&ctx->lhbm_lock);
		mi_disp_panel_update_lhbm_reg(dsi->panel, TYPE_LHBM_OFF, bl_level);
		mi_disp_panel_ddic_send_cmd(local_hbm_normal_off, ARRAY_SIZE(local_hbm_normal_off), false);
		mutex_unlock(&ctx->lhbm_lock);
		mi_cfg->lhbm_en = false;
		break;
	case LOCAL_HBM_HLPM_WHITE_1000NIT://6
	case LOCAL_HBM_NORMAL_WHITE_1000NIT://1
		pr_info("LOCAL_HBM_NORMAL_WHITE_1000NIT \n");
		lhbm_flag = true;
		mutex_lock(&ctx->lhbm_lock);
		mi_disp_panel_update_lhbm_reg(dsi->panel, TYPE_WHITE_1200, bl_level);
		mi_disp_panel_ddic_send_cmd(local_hbm_normal_white_1200nit, ARRAY_SIZE(local_hbm_normal_white_1200nit), false);
		mutex_unlock(&ctx->lhbm_lock);
		mi_cfg->lhbm_en = true;
		break;
	case LOCAL_HBM_HLPM_WHITE_110NIT://7
	case LOCAL_HBM_NORMAL_WHITE_110NIT://4
		pr_info("LOCAL_HBM_NORMAL_WHITE_110NIT \n");
		lhbm_flag = true;
		mutex_lock(&ctx->lhbm_lock);
		mi_disp_panel_update_lhbm_reg(dsi->panel, TYPE_WHITE_200, bl_level);
		mi_disp_panel_ddic_send_cmd(local_hbm_normal_white_200nit, ARRAY_SIZE(local_hbm_normal_white_200nit), false);
		mutex_unlock(&ctx->lhbm_lock);
		mi_cfg->lhbm_en = true;
		break;
	default:
		pr_info("invalid local hbm value\n");
		break;
	}
	return 0;
}
static int panel_fod_lhbm_init (struct mtk_dsi* dsi)
{
	if (!dsi) {
		pr_info("invalid dsi point\n");
		return -1;
	}
	pr_info("panel_fod_lhbm_init enter\n");
	dsi->display_type = "primary";
	dsi->mi_cfg.lhbm_ui_ready_delay_frame = 3;
	dsi->mi_cfg.lhbm_ui_ready_delay_frame_aod = 3;
	dsi->mi_cfg.local_hbm_enabled = 1;
	return 0;
}

static int panel_get_wp_info(struct drm_panel *panel, char *buf, size_t size)
{
	struct device_node *chosen;
	char *tmp_buf = NULL;
	unsigned long tmp_size = 0;

	pr_info(" %s start \n", __func__);
	chosen = of_find_node_by_path("/chosen");
	if (chosen) {
		tmp_buf = (char *)of_get_property(chosen, "lcm_white_point", (int *)&tmp_size);
		if (tmp_size > 0) {
			strncpy(buf, tmp_buf, tmp_size);
			pr_info("[%s]: white_point = %s, size = %lu\n", __func__, buf, tmp_size);
		} else {
/* P16 code for HQFEAT-89044 by p-zhangyundan at 2025/4/27 start */
#if IS_ENABLED(CONFIG_MIEV)
			mi_disp_mievent_str(MI_EVENT_PANEL_WP_READ_FAILED);
#endif
			pr_err("Invalid lcm whitepoint temp\n");
		}
	} else {
#if IS_ENABLED(CONFIG_MIEV)
		mi_disp_mievent_str(MI_EVENT_PANEL_WP_READ_FAILED);
#endif
/* P16 code for HQFEAT-89044 by p-zhangyundan at 2025/4/27 end */
		pr_err("[%s]:find chosen failed\n", __func__);
	}
	return tmp_size;
}

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
	return;
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
			if (lhbm_flag == false)
				mi_disp_panel_ddic_send_cmd(lcm_aod_hbm, ARRAY_SIZE(lcm_aod_hbm), false);
			doze_brightness_flag = true;
			break;
		case DOZE_BRIGHTNESS_LBM:
			if (lhbm_flag == false)
				mi_disp_panel_ddic_send_cmd(lcm_aod_lbm, ARRAY_SIZE(lcm_aod_lbm), false);
			doze_brightness_flag = true;
			break;
		case DOZE_TO_NORMAL:
			//mi_disp_panel_ddic_send_cmd(lcm_aod_off, ARRAY_SIZE(lcm_aod_off), false);
			doze_brightness_flag = false;
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
static int panel_set_only_aod_backlight(struct drm_panel *panel, int doze_brightness)
{
	struct lcm *ctx = panel_to_lcm(panel);
	if (!panel) {
		pr_err("invalid params\n");
		return -1;
	}
	pr_info("%s set_doze_brightness state = %d",__func__, doze_brightness);
	if (DOZE_BRIGHTNESS_LBM  == doze_brightness) {
		mi_disp_panel_ddic_send_cmd(lcm_aod_lbm, ARRAY_SIZE(lcm_aod_lbm), false);
	} else if (DOZE_BRIGHTNESS_HBM == doze_brightness) {
		mi_disp_panel_ddic_send_cmd(lcm_aod_hbm, ARRAY_SIZE(lcm_aod_hbm), false);
	}
	ctx->doze_brightness = doze_brightness;
	pr_info("%s set doze_brightness %d end -\n", __func__, doze_brightness);
	return 0;
}
static int panel_hdr_set_cmdq(struct drm_panel *panel, void *dsi,
			    dcs_write_gce cb, void *handle, bool en)
{
	struct lcm *ctx = panel_to_lcm(panel);
	char hdr_tb0[] = {0x55, 0x04};
	pr_info("%s hdr_en state = %d",__func__, en);
	if (ctx->hdr_en == en)
		goto done;
	if (en) {
		hdr_tb0[1] = 0x05;
	} else {
		hdr_tb0[1] = 0x04;
        }
	cb(dsi, handle, hdr_tb0, ARRAY_SIZE(hdr_tb0));
	ctx->hdr_en = en;
done:
	return 0;
}
static void panel_hdr_get_state(struct drm_panel *panel, bool *state)
{
	struct lcm *ctx = panel_to_lcm(panel);
	*state = ctx->hdr_en;
}

int panel_set_gray_by_temperature(struct drm_panel *panel, int level)
{
	/*struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s temperature_level %d start\n", __func__, level);
	if (!panel) {
		pr_err("invalid params\n");
		return -EINVAL;
	}
	if (ctx->dynamic_fps == 120)
		csc_setting[3].para_list[0] = 0x01;
	else
		csc_setting[3].para_list[0] = 0x07;
	if (level <= 25) {
		csc_setting[4].para_list[1] = 0x19;
	} else if (level <= 30) {
		csc_setting[4].para_list[1] = 0x1E;
	} else if (level <= 35) {
		csc_setting[4].para_list[1] = 0x23;
	} else if (level <= 40) {
		csc_setting[4].para_list[1] = 0x28;
	} else {
		csc_setting[4].para_list[1] = 0x2D;
	}
	mi_disp_panel_ddic_send_cmd(csc_setting, ARRAY_SIZE(csc_setting), false);*/
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
	.panel_set_gir_on = panel_set_gir_on,
	.panel_set_gir_off = panel_set_gir_off,
	.panel_get_gir_status = panel_get_gir_status,
	.get_wp_info = panel_get_wp_info,
	.get_panel_max_brightness_clone = panel_get_max_brightness_clone,
	.get_panel_factory_max_brightness = panel_get_factory_max_brightness,
	.normal_hbm_control = panel_normal_hbm_control,
	.setbacklight_control = lcm_setbacklight_control,
	.get_panel_initialized = get_lcm_initialized,
	.set_lhbm_fod = panel_set_lhbm_fod,
	.panel_fod_lhbm_init = panel_fod_lhbm_init,
	.backlight_for_calibration = backlight_for_calibration,
	.panel_poweron = panel_init_power,
	.panel_poweroff = panel_power_down,
	.esd_restore_backlight = lcm_esd_restore_backlight,
	.set_doze_brightness = panel_set_doze_brightness,
	.get_doze_brightness = panel_get_doze_brightness,
	.set_only_aod_backlight = panel_set_only_aod_backlight,
	.hdr_set_cmdq = panel_hdr_set_cmdq,
	.hdr_get_state = panel_hdr_get_state,
	.set_gray_by_temperature = panel_set_gray_by_temperature,
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

	pr_info(" %s start \n", __func__);
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
	mode->type = DRM_MODE_TYPE_DRIVER;
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
	mode_2->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode_2);
	mode_3 = drm_mode_duplicate(connector->dev, &performance_mode_3);
	if (!mode_3) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 performance_mode_3.hdisplay,
			 performance_mode_3.vdisplay,
			 drm_mode_vrefresh(&performance_mode_3));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_3);
	mode_3->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_3);
	connector->display_info.width_mm = PHYSICAL_WIDTH / 1000;
	connector->display_info.height_mm = PHYSICAL_HEIGHT / 1000;
	pr_info(" %s end \n", __func__);
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
	struct device_node *np;
	struct platform_device *pmic_pdev = NULL;
	pr_info(" %s+\n", __func__);
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

	pr_info(" %s+\n", __func__);
	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);

	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO |
			MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_NO_EOT_PACKET |
			MIPI_DSI_CLOCK_NON_CONTINUOUS;

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
/* P16 code for HQFEAT-89044 by p-zhangyundan at 2025/4/27 start */
#if IS_ENABLED(CONFIG_MIEV)
		mi_disp_mievent_str(MI_EVENT_PANEL_HW_RESOURCE_GET_FAILED);
#endif
/* P16 code for HQFEAT-89044 by p-zhangyundan at 2025/4/27 end */
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->vddi_gpio = devm_gpiod_get(dev, "vddi", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vddi_gpio)) {
		dev_err(dev, "%s: cannot get vddi-gpios %ld\n",
			__func__, PTR_ERR(ctx->vddi_gpio));
/* P16 code for HQFEAT-89044 by p-zhangyundan at 2025/4/27 start */
#if IS_ENABLED(CONFIG_MIEV)
		mi_disp_mievent_str(MI_EVENT_PANEL_HW_RESOURCE_GET_FAILED);
#endif
/* P16 code for HQFEAT-89044 by p-zhangyundan at 2025/4/27 end */
		return PTR_ERR(ctx->vddi_gpio);
	}
	devm_gpiod_put(dev, ctx->vddi_gpio);

	ret = lcm_panel_vci_regulator_init(dev);
	if (!ret)
		lcm_panel_vci_enable(dev);
	else
		pr_info("%s init vibr regulator error\n", __func__);

	ret = lcm_panel_dvdd_regulator_init(dev);
	if (!ret)
		lcm_panel_dvdd_enable(dev);
	else
		pr_info("%s init vibr regulator error\n", __func__);

	ctx->pctrl = devm_pinctrl_get(dev);
	if(IS_ERR(ctx->pctrl)){
		pr_err("devm_pinctrl_get error\n");
		return PTR_ERR(ctx->pctrl);
	}
	ctx->spi_cs_high = pinctrl_lookup_state(ctx->pctrl,"default");
	if(IS_ERR(ctx->spi_cs_high)){
		pr_err("pinctrl_lookup_state ctx->spi_cs_high error\n");
		return PTR_ERR(ctx->spi_cs_high);
	}
	ctx->gpio_cs_low = pinctrl_lookup_state(ctx->pctrl,"spi_cs_low");
	if(IS_ERR(ctx->gpio_cs_low)){
		pr_err("pinctrl_lookup_state ctx->gpio_cs_low error\n");
		return PTR_ERR(ctx->gpio_cs_low);
	}

	ctx->prepared = true;
	ctx->enabled = true;
	ctx->panel_info = panel_name;

	ext_params.err_flag_irq_gpio = of_get_named_gpio(
		dev->of_node, "mi,esd-err-irq-gpio",0);
	ext_params.err_flag_irq_flags = 0x2002;
	ext_params_mode_1.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_mode_1.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ext_params_mode_2.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_mode_2.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ext_params_mode_3.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_mode_3.err_flag_irq_flags = ext_params.err_flag_irq_flags;

	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params_mode_2, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif
	this_panel = &ctx->panel;
	ctx->hbm_enabled = false;
	doze_brightness_flag = false;
	ctx->gir_status = GIR_ENABLE;
	ctx->max_brightness_clone = MAX_BRIGHTNESS_CLONE;
	ctx->factory_max_brightness = FACTORY_MAX_BRIGHTNESS;
	ctx->hdr_en = false;
	is_diming_on = false;
	check_is_need_fake_resolution(dev);
	mutex_init(&ctx->panel_lock);
	mutex_init(&ctx->initialcode_lock);
	hq_regiser_hw_info(HWID_LCM, "oncell,vendor:41,IC:02");
	np = of_find_node_by_name(NULL, "pmic");
	if (!np) {
		pr_err("pmic node not found\n");
	}
	pmic_pdev = of_find_device_by_node(np->child);
	if (!pmic_pdev) {
		pr_err("pmic child device not found\n");
	}
	/* get regmap */
	map = dev_get_regmap(pmic_pdev->dev.parent, NULL);
	if (!map) {
		pr_err("regmap get failed\n");
	}
	/* P16 code for BUGHQ-9264 by p-zhangyundan at 2025/7/30 start */
	lcm_get_lhbm_info();
	/* P16 code for BUGHQ-9264 by p-zhangyundan at 2025/7/30 end */
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
	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	if (ctx->pctrl) {
		devm_pinctrl_put(ctx->pctrl);
		ctx->pctrl = NULL;
		pr_info("%s-release ctx->pctrl！\n", __func__);
	}

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
#endif
	mutex_destroy(&ctx->panel_lock);
	mutex_destroy(&ctx->initialcode_lock);
}

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "p16_41_02_0c_dsc_vdo,lcm", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-p16-41-02-0c-dsc-vdo",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);
/* P16 code for BUGHQ-9264 by p-zhangyundan at 2025/7/30 start */
module_param_string(oled_lhbm, oled_lhbm_cmdline, sizeof(oled_lhbm_cmdline), 0600);
MODULE_PARM_DESC(oled_lhbm, "oled_lhbm=<local_hbm_info>");
/* P16 code for BUGHQ-9264 by p-zhangyundan at 2025/7/30 end */
MODULE_AUTHOR("liaoxianguo <liaoxianguo@huaqin.com>");
MODULE_DESCRIPTION("panel-p16-41-02-0c-dsc-vdo Panel Driver");
MODULE_LICENSE("GPL v2");