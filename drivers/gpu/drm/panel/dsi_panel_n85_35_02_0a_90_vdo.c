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
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include "../mediatek/mediatek_v2/mtk_disp_notify.h"

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

#include "../../../misc/mediatek/gate_ic/ocp2138_drv.h"
#include "../../../misc/mediatek/gate_ic/ktz8864.h"

#define HFP (80)
#define HSA (8)
#define HBP (80)
#define VFP (240)
#define VSA (2)
#define VBP (14)
#define VAC (1340)
#define HAC (800)

//90hz
#define  MODE_0_FPS (90)
//#define MODE_0_VFP (240)

#define  MODE_1_FPS (60)
#define  MODE_1_VFP (1038)

#define  MODE_2_FPS (50)
#define  MODE_2_VFP (1517)

#define  MODE_3_FPS (48)
#define  MODE_3_VFP (1637)

#define CABC_CTRL_REG               0x55

#define HBM_MAP_MAX_BRIGHTNESS      4095
#define NORMAL_MAX_BRIGHTNESS       2047
#define REG_MAX_BRIGHTNESS          2047
#define REG_HBM_BRIGHTNESS_FOR_KTZ8864         1930
#define REG_HBM_BRIGHTNESS_FOR_AW9967          1935
//the max brightness value write to 51 reg when  the current of the backlight ic reaches 22mA
#define REG_NORMAL_MAX_BRIGHTNESS_FOR_KTZ8864       1600
#define REG_NORMAL_MAX_BRIGHTNESS_FOR_AW9967       1615
static u32 fake_heigh = 1340;
static u32 fake_width = 800;
static bool need_fake_resolution;
static int current_fps = 90;
static struct touch_panel_notify_data tp_notify_data;
int bias_id = -1;
static unsigned int reg_normal_max_brightness = 0;
static unsigned int reg_hbm_max_brightness = 0;
static bool cabc_flag = true;

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos, *bias_neg;

	bool prepared;
	bool enabled;

	unsigned int gate_ic;

	int cabc_status;

	int error;
};

struct tag_videolfb {
	u64 fb_base;
	u32 islcmfound;
	u32 fps;
	u32 vram;
	char lcmname[1];
};

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

static int lcm_panel_bias_enable(void)
{
	int retval = 0;

	pr_info("%s+\n", __func__);

	if (bias_id == 18) {
	    lcd_set_bias(1);
	} else  {
	   _gate_ic_panel_bias_enable(1);
	}

	pr_info("%s-\n", __func__);
	return retval;
}

static int lcm_panel_bias_disable(void)
{
	int retval = 0;

	pr_info("%s+\n", __func__);

	if (bias_id == 18) {
	    lcd_set_bias(0);
	} else  {
	   _gate_ic_panel_bias_enable(0);
	}

	pr_info("%s-\n", __func__);
	return retval;
}

static void lcm_mdelay(unsigned int ms)
{
	if (ms < 10)
		udelay(ms * 1000);
	else if (ms <= 20)
		usleep_range(ms*1000, (ms+1)*1000);
	else
		usleep_range(ms * 1000 - 100, ms * 1000);
}

static void lcm_panel_init(struct lcm *ctx)
{

	pr_info("%s+\n", __func__);
	// lcd reset H -> L -> L
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
			dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
			return;
	} 
	lcm_mdelay(15);
	gpiod_set_value(ctx->reset_gpio, 0);
	lcm_mdelay(5);
	gpiod_set_value(ctx->reset_gpio, 1);
	lcm_mdelay(10);
	gpiod_set_value(ctx->reset_gpio, 0);
	lcm_mdelay(5);
	gpiod_set_value(ctx->reset_gpio, 1);
	lcm_mdelay(15);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	// end

	//cabc start
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x23);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x00, 0x60);
	lcm_dcs_write_seq_static(ctx, 0x05, 0x2d);
	lcm_dcs_write_seq_static(ctx, 0x07, 0x20);
	lcm_dcs_write_seq_static(ctx, 0x08, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x09, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x10, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x11, 0x40);
	lcm_dcs_write_seq_static(ctx, 0x12, 0xa8);
	lcm_dcs_write_seq_static(ctx, 0x15, 0x7d);
	lcm_dcs_write_seq_static(ctx, 0x16, 0x13);

	//UI Mode PWM
	lcm_dcs_write_seq_static(ctx, 0x30, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0x31, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0x32, 0xFE);
	lcm_dcs_write_seq_static(ctx, 0x33, 0xFD);
	lcm_dcs_write_seq_static(ctx, 0x34, 0xFC);
	lcm_dcs_write_seq_static(ctx, 0x35, 0xFB);
	lcm_dcs_write_seq_static(ctx, 0x36, 0xFA);
	lcm_dcs_write_seq_static(ctx, 0x37, 0xF8);
	lcm_dcs_write_seq_static(ctx, 0x38, 0xF6);
	lcm_dcs_write_seq_static(ctx, 0x39, 0xF4);
	lcm_dcs_write_seq_static(ctx, 0x3A, 0xF2);
	lcm_dcs_write_seq_static(ctx, 0x3B, 0xF0);
	lcm_dcs_write_seq_static(ctx, 0x3D, 0xEE);
	lcm_dcs_write_seq_static(ctx, 0x3F, 0xEB);
	lcm_dcs_write_seq_static(ctx, 0x40, 0xE8);
	lcm_dcs_write_seq_static(ctx, 0x41, 0xE5);
	lcm_dcs_write_seq_static(ctx, 0x29, 0x10);

	//Still Mode pwm
	lcm_dcs_write_seq_static(ctx, 0x45, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0x46, 0xFD);
	lcm_dcs_write_seq_static(ctx, 0x47, 0xF9);
	lcm_dcs_write_seq_static(ctx, 0x48, 0xF2);
	lcm_dcs_write_seq_static(ctx, 0x49, 0xEA);
	lcm_dcs_write_seq_static(ctx, 0x4A, 0xE2);
	lcm_dcs_write_seq_static(ctx, 0x4B, 0xD5);
	lcm_dcs_write_seq_static(ctx, 0x4C, 0xC8);
	lcm_dcs_write_seq_static(ctx, 0x4D, 0xBB);
	lcm_dcs_write_seq_static(ctx, 0x4E, 0xB0);
	lcm_dcs_write_seq_static(ctx, 0x4F, 0xA6);
	lcm_dcs_write_seq_static(ctx, 0x50, 0x9C);
	lcm_dcs_write_seq_static(ctx, 0x51, 0x90);
	lcm_dcs_write_seq_static(ctx, 0x52, 0x84);
	lcm_dcs_write_seq_static(ctx, 0x53, 0x78);
	lcm_dcs_write_seq_static(ctx, 0x54, 0x65);
	lcm_dcs_write_seq_static(ctx, 0x2A, 0x10);

	//Moving Mode pwm
	lcm_dcs_write_seq_static(ctx, 0x58, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0x59, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0x5A, 0xFB);
	lcm_dcs_write_seq_static(ctx, 0x5B, 0xF7);
	lcm_dcs_write_seq_static(ctx, 0x5C, 0xF2);
	lcm_dcs_write_seq_static(ctx, 0x5D, 0xED);
	lcm_dcs_write_seq_static(ctx, 0x5E, 0xE8);
	lcm_dcs_write_seq_static(ctx, 0x5F, 0xE3);
	lcm_dcs_write_seq_static(ctx, 0x60, 0xDD);
	lcm_dcs_write_seq_static(ctx, 0x61, 0xD7);
	lcm_dcs_write_seq_static(ctx, 0x62, 0xD1);
	lcm_dcs_write_seq_static(ctx, 0x63, 0xCC);
	lcm_dcs_write_seq_static(ctx, 0x64, 0xC6);
	lcm_dcs_write_seq_static(ctx, 0x65, 0xC0);
	lcm_dcs_write_seq_static(ctx, 0x66, 0xBA);
	lcm_dcs_write_seq_static(ctx, 0x67, 0xB2);
	lcm_dcs_write_seq_static(ctx, 0x2B, 0x10);
	//cabc end

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x2A);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x64, 0x2E);
	lcm_dcs_write_seq_static(ctx, 0x67, 0x26);
	lcm_dcs_write_seq_static(ctx, 0x6A, 0x26);
	lcm_dcs_write_seq_static(ctx, 0x70, 0x26);
	lcm_dcs_write_seq_static(ctx, 0x73, 0x26);
	lcm_dcs_write_seq_static(ctx, 0x79, 0x26);
	lcm_dcs_write_seq_static(ctx, 0x7C, 0x26);
	lcm_dcs_write_seq_static(ctx, 0x7F, 0x26);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x24);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x95, 0x82);
	lcm_dcs_write_seq_static(ctx, 0x55, 0x86, 0x06);
	lcm_dcs_write_seq_static(ctx, 0x59, 0x30);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x26);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xCA, 0xCE);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0xF0);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x9E, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xEE, 0x38);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x10);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x68, 0x04, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xBB, 0x13);
	lcm_dcs_write_seq_static(ctx, 0x53, 0x2C);
	lcm_dcs_write_seq_static(ctx, 0x55, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x5E, 0x00,0x06);//(6+1)/2048
	lcm_dcs_write_seq_static(ctx, 0xE9, 0x02);

	lcm_dcs_write_seq_static(ctx, 0x11);
	msleep(100);
	lcm_dcs_write_seq_static(ctx, 0x29);
	lcm_mdelay(20);

	pr_info("%s-\n", __func__);
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s+\n", __func__);
	cabc_flag = false;
	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	pr_info("%s-\n", __func__);
	return 0;
}
bool nvt_gesture_90hz_status = false;
void nvt_get_gesture_90hz_status(bool gst_status)
{
	nvt_gesture_90hz_status = gst_status;
	pr_info("[nt36523D] %s gst_status = %d\n", 
		__func__, gst_status);
}
EXPORT_SYMBOL_GPL(nvt_get_gesture_90hz_status);
bool nvt_90hz_proximity_status = false;
void nvt_90hz_proximity_mode(bool proximity_mode)
{
	nvt_90hz_proximity_status = proximity_mode;
	pr_info("[nvt] %s proxmity_status = %d\n", __func__, nvt_90hz_proximity_status);
}
EXPORT_SYMBOL_GPL(nvt_90hz_proximity_mode);
static bool set_bias_flag = false;
static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s+\n", __func__);

	if (!ctx->prepared)
		return 0;

	tp_notify_data.blank = MTK_DRM_BLANK_POWERDOWN;
	mtk_disp_notifier_call_chain(MTK_DRM_EARLY_EVENT_BLANK, &tp_notify_data);
	pr_info("[nvt] : %s +++ tp_notify_data.blank = MTK_DRM_BLANK_POWERDOWN +++", __func__);

	lcm_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);

	if (nvt_gesture_90hz_status == true && nvt_90hz_proximity_status == 0) {
		lcm_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
		lcm_mdelay(100);
	}
/*	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
*/
	//pull down bias power
	if ((nvt_90hz_proximity_status == false) && (nvt_gesture_90hz_status == false)) {
		lcm_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
		lcm_mdelay(100);
		lcm_panel_bias_disable();
		ctx->bias_neg = devm_gpiod_get(ctx->dev, "biasn", GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_neg))
		{
			dev_info(ctx->dev, "cannot get biasn_gpio %ld\n", PTR_ERR(ctx->bias_neg));
			return PTR_ERR(ctx->bias_neg);
		}
		gpiod_set_value(ctx->bias_neg, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);
		lcm_mdelay(8);
		ctx->bias_pos = devm_gpiod_get(ctx->dev, "biasp", GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_pos))
		{
			dev_info(ctx->dev, "cannot get biasp_gpio %ld\n", PTR_ERR(ctx->bias_pos));
			return PTR_ERR(ctx->bias_pos);
		}
		gpiod_set_value(ctx->bias_pos, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);

		lcm_mdelay(20);
		set_bias_flag = true;
	} else {
		pr_info("[nt36523D] %s nvt_90hz_proximity_status = %d\n",
			__func__, nvt_90hz_proximity_status);
		pr_info("[nt36523D] %s nvt_gesture_90hz_status = %d\n",
			__func__, nvt_gesture_90hz_status);
	}

	ctx->error = 0;
	ctx->prepared = false;

	pr_info("%s-\n", __func__);

	return 0;
}
static int lcm_enter_prepare = -1;
static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s+\n", __func__);
	lcm_enter_prepare = 1;
	if (ctx->prepared)
		return 0;
	if (((nvt_90hz_proximity_status == false) && (nvt_gesture_90hz_status == false))||
			set_bias_flag == true) {
		ctx->bias_pos = devm_gpiod_get(ctx->dev, "biasp", GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_pos))
		{
			dev_info(ctx->dev, "cannot get biasp_gpio %ld\n", PTR_ERR(ctx->bias_pos));
			return PTR_ERR(ctx->bias_pos);
		}

		gpiod_set_value(ctx->bias_pos, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);

		udelay(2000);

		ctx->bias_neg = devm_gpiod_get(ctx->dev, "biasn", GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_neg))
		{
			dev_info(ctx->dev, "cannot get biasn_gpio %ld\n", PTR_ERR(ctx->bias_neg));
			return PTR_ERR(ctx->bias_neg);
		}
		gpiod_set_value(ctx->bias_neg, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);

		lcm_panel_bias_enable();
		set_bias_flag = false;
	}
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

	tp_notify_data.blank = MTK_DRM_BLANK_UNBLANK;
	mtk_disp_notifier_call_chain(MTK_DRM_EVENT_BLANK, &tp_notify_data);
	pr_info("[nvt] : %s +++ tp_notify_data.blank = MTK_DRM_BLANK_UNBLANK +++", __func__);

	pr_info("%s-\n", __func__);

	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s+\n", __func__);
	cabc_flag = true;

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	pr_info("%s-\n", __func__);

	return 0;
}

//90hz
static struct drm_display_mode default_mode = {
	.clock = (HAC + HFP + HSA + HBP) *
		(VAC + VFP + VSA + VBP) * MODE_0_FPS / 1000,   //139043, h_total * v_total * fps / 1000
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP,
	.vsync_end = VAC + VFP + VSA,
	.vtotal = VAC + VFP + VSA + VBP,
};

//60hz
static struct drm_display_mode performance_mode_1 = {
	.clock = (HAC + HFP + HSA + HBP) *
		(VAC + MODE_1_VFP + VSA + VBP) * MODE_1_FPS / 1000,    //h_total * v_total * fps / 1000
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + MODE_1_VFP,
	.vsync_end = VAC + MODE_1_VFP + VSA,
	.vtotal = VAC + MODE_1_VFP + VSA + VBP,
};

//50hz
static struct drm_display_mode performance_mode_2 = {
	.clock = (HAC + HFP + HSA + HBP) *
		(VAC + MODE_2_VFP + VSA + VBP) * MODE_2_FPS / 1000,    //h_total * v_total * fps / 1000
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + MODE_2_VFP,
	.vsync_end = VAC + MODE_2_VFP + VSA,
	.vtotal = VAC + MODE_2_VFP + VSA + VBP,
};

//48hz
static struct drm_display_mode performance_mode_3 = {
	.clock = (HAC + HFP + HSA + HBP) *
		(VAC + MODE_3_VFP + VSA + VBP) * MODE_3_FPS / 1000,    //h_total * v_total * fps / 1000
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + MODE_3_VFP,
	.vsync_end = VAC + MODE_3_VFP + VSA,
	.vtotal = VAC + MODE_3_VFP + VSA + VBP,
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

#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
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
	unsigned char id[3] = {0x00, 0x00, 0x00};
	ssize_t ret;

	ret = mipi_dsi_dcs_read(dsi, 0x4, data, 3);
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

extern bool is_kernel_power_off_charging_lcd(void);
static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_tb0[] = {0x51, 0x07,0xFF};
	char bl_tb1[] = {0x53, 0x24};
	char bl_dimming_high[] = {0x68, 0x04,0x01};
	char bl_dimming_low[] = {0x68, 0x05,0x01};

	pr_info("%s backlight level is %d.\n", __func__, level);

	//The backlight will be set to 0, accelerate the speed of turning off the backlight by disable dimming function.
	if(0 == level) {
		pr_info("%s disable dimming function.\n", __func__);

		if (!cb)
			return -1;

		cb(dsi, handle, bl_tb1, ARRAY_SIZE(bl_tb1));
	}

	if (is_kernel_power_off_charging_lcd()) {
		pr_err("[kernel/lcm]%s detect power off charging,set backlight 307", __func__);
		level = 307;
	}

	if(lcm_enter_prepare == 1)
	{
		cb(dsi, handle, bl_dimming_low, ARRAY_SIZE(bl_dimming_low));
	}

	if (level > HBM_MAP_MAX_BRIGHTNESS) {
		pr_err("[kernel/lcm]%s err level;", __func__);
		return -EPERM;
	} else if (level > NORMAL_MAX_BRIGHTNESS) {
		pr_info("[kernel/lcm]%s hbm_on, level = %d", __func__, level);
		level = (level - REG_MAX_BRIGHTNESS) * (reg_hbm_max_brightness - reg_normal_max_brightness)
			 / (HBM_MAP_MAX_BRIGHTNESS - REG_MAX_BRIGHTNESS) + reg_normal_max_brightness;
	} else {
		pr_info("[kernel/lcm]%s hbm_off, level = %d", __func__, level);
		level = level * reg_normal_max_brightness / REG_MAX_BRIGHTNESS;
	}

	pr_info("%s the backlight level after convert is %d.\n", __func__, level);

#ifdef FACTORY_BUILD
	if (18 == bias_id) { //KTZ8864
		if (level > 0 && level <= 16)
			level = 6;
	} else { //AW9967DNR
		if (level > 0 && level <= 16)
			level = 6;
	}
#else
	if (18 == bias_id) { //KTZ8864
		if (level > 0 && level <= 6)
			level = 6;
	} else { //AW9967DNR
		if (level > 0 && level <= 6)
			level = 6;
	}
#endif

	pr_info("%s the backlight level set to 51 reg is %d.\n", __func__, level);

	bl_tb0[1] = level >> 8 & 0x07;
	bl_tb0[2] = level & 0xFF;
	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

	if(lcm_enter_prepare == 1)
	{
		msleep(40);
		lcm_enter_prepare = 0;
		cb(dsi, handle, bl_dimming_high, ARRAY_SIZE(bl_dimming_high));
	}

	return 0;
}

static int lcm_set_cabc_mode(struct drm_panel *panel, void *dsi, dcs_write_gce cb, void *handle, unsigned int mode)
{
	struct lcm *ctx = panel_to_lcm(panel);

	char cabc_state[] = {CABC_CTRL_REG, mode};

	if (cabc_flag == false){
		pr_err("cabc_flag is false");
		return -1;
	}

	pr_info("%s enter, set cabc_mode: %d\n", __func__, mode);

	switch (mode)
	{
		case DDIC_CABC_UI_ON:
		case DDIC_CABC_STILL_ON:
		case DDIC_CABC_MOVIE_ON:
			cabc_state[1] = mode & 0xFF;
			ctx->cabc_status = mode;
			break;
		default:
			cabc_state[1] = 0x00;
			ctx->cabc_status = DDIC_CABC_OFF;
			break;
	}

	if (!cb){
		pr_err("cb error");
		return -1;
	}

	cb(dsi, handle, cabc_state, ARRAY_SIZE(cabc_state));

	pr_info("%s exit, final cabc_status is: %d\n", __func__, ctx->cabc_status);

	return 0;
}

static int lcm_get_cabc_mode(struct drm_panel *panel, unsigned int *mode)
{
	struct lcm *ctx;

	ctx = panel_to_lcm(panel);
	*mode = ctx->cabc_status;

	pr_info("%s enter, cabc_mode: %d\n", __func__, ctx->cabc_status);

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

//90hz
static struct mtk_panel_params ext_params = {
	.pll_clk = 449,
	.data_rate = 898,
	.vfp_low_power = MODE_2_VFP, //50hz
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.phy_timcon = {
		.hs_trail = 0x0a,
	},
};

//60hz
static struct mtk_panel_params ext_params_mode_1 = {
	.pll_clk = 449,
	.data_rate = 898,
	.vfp_low_power = MODE_2_VFP, //50hz
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.phy_timcon = {
		.hs_trail = 0x0a,
	},
};

//50hz
static struct mtk_panel_params ext_params_mode_2 = {
	.pll_clk = 449,
	.data_rate = 898,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.phy_timcon = {
		.hs_trail = 0x0a,
	},
};

//48hz
static struct mtk_panel_params ext_params_mode_3 = {
	.pll_clk = 449,
	.data_rate = 898,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.phy_timcon = {
		.hs_trail = 0x0a,
	},
};

static struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
	unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;

	pr_info("[lcm]%s enter\n", __func__);

	list_for_each_entry(m, &connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}

	pr_info("[lcm]%s get mode fail\n", __func__);

	return NULL;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			 struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, mode);

	if (m == NULL) {
		pr_info("[lcm]%s:%d invalid display_mode\n", __func__, __LINE__);
		return -1;
	}
	if (drm_mode_vrefresh(m) == MODE_0_FPS)
		ext->params = &ext_params;
	else if (drm_mode_vrefresh(m) == MODE_1_FPS)
		ext->params = &ext_params_mode_1;
	else if (drm_mode_vrefresh(m) == MODE_2_FPS)
		ext->params = &ext_params_mode_2;
	else if (drm_mode_vrefresh(m) == MODE_3_FPS)
		ext->params = &ext_params_mode_3;
	else
		ret = 1;
	if (!ret)
		current_fps = drm_mode_vrefresh(m);
	return ret;
}


static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ext_param_set = mtk_panel_ext_param_set,
	.ata_check = panel_ata_check,
	.get_virtual_heigh = lcm_get_virtual_heigh,
	.get_virtual_width = lcm_get_virtual_width,
	.set_cabc_mode = lcm_set_cabc_mode,
	.get_cabc_mode = lcm_get_cabc_mode,
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
	struct drm_display_mode *mode;
	struct drm_display_mode *mode_1;
	struct drm_display_mode *mode_2;
	struct drm_display_mode *mode_3;
	int mode_count = 0;

	pr_info("%s+\n", __func__);
	if (need_fake_resolution)
		change_drm_disp_mode_params(&default_mode);

	pr_info("%s, default_mode.clock ==%d.\n", __func__, default_mode.clock);
	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);
	mode_count++;

	mode_1 = drm_mode_duplicate(connector->dev, &performance_mode_1);
	if (!mode_1) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			performance_mode_1.hdisplay, performance_mode_1.vdisplay,
			drm_mode_vrefresh(&performance_mode_1));
		return -ENOMEM;
	}

	drm_mode_set_name(mode_1);
	mode_1->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_1);
	mode_count++;

	mode_2 = drm_mode_duplicate(connector->dev, &performance_mode_2);
	if (!mode_2) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			performance_mode_2.hdisplay, performance_mode_2.vdisplay,
			drm_mode_vrefresh(&performance_mode_2));
		return -ENOMEM;
	}

	drm_mode_set_name(mode_2);
	mode_2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_2);
	mode_count++;

	mode_3 = drm_mode_duplicate(connector->dev, &performance_mode_3);
	if (!mode_3) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			performance_mode_3.hdisplay, performance_mode_3.vdisplay,
			drm_mode_vrefresh(&performance_mode_3));
		return -ENOMEM;
	}

	drm_mode_set_name(mode_3);
	mode_3->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_3);
	mode_count++;

	connector->display_info.width_mm = 113;
	connector->display_info.height_mm = 189;

	return mode_count;
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
	if (ret) {
		need_fake_resolution = false;
		return;
	}
	ret = of_property_read_u32(dev->of_node, "fake_width", &fake_width);
	if (ret) {
		need_fake_resolution = false;
		return;
	}
	if (fake_heigh > 0 && fake_heigh < VAC && fake_width > 0 && fake_width < HAC)
		need_fake_resolution = true;
	else
		need_fake_resolution = false;
}

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct lcm *ctx;
	struct device_node *backlight;
	unsigned int value;
	int ret;
	struct device_node *lcm_name;
	unsigned long size = 0;
	struct tag_videolfb *videolfb_tag = NULL;

	pr_info("%s+\n", __func__);

	pr_info("start to parse lcm name\n");
	lcm_name = of_find_node_by_path("/chosen");
	if (lcm_name) {
		videolfb_tag = (struct tag_videolfb *)
		of_get_property(lcm_name, "atag,videolfb", (int *)&size);

		if (!videolfb_tag)
			pr_info("Invalid lcm name\n");

		pr_info("read lcm name : %s\n", videolfb_tag->lcmname);
		if (strcmp("dsi_panel_n85_35_02_0a_90_vdo_lcm_drv",videolfb_tag->lcmname) == 0 ){
			pr_info(" find lcm: %s \n",videolfb_tag->lcmname);
		}
		else {
			pr_info("Can't find lcm %s\n",__func__);
			return -ENODEV;
		}
	} else {
		pr_info("Can't find node: chose in dts");
		return -ENODEV;
	}

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
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE
			| MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_NO_EOT_PACKET
			 | MIPI_DSI_CLOCK_NON_CONTINUOUS;

	ret = of_property_read_u32(dev->of_node, "gate-ic", &value);
	if (ret < 0)
		value = 0;
	else
		ctx->gate_ic = value;

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

	ctx->bias_pos = devm_gpiod_get(dev, "biasp", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos))
	{
		dev_info(dev, "cannot get biasp_gpio %ld\n", PTR_ERR(ctx->bias_pos));	
		return PTR_ERR(ctx->bias_pos);
	}
	devm_gpiod_put(dev, ctx->bias_pos);

	ctx->bias_neg = devm_gpiod_get(dev, "biasn", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg))
	{
		dev_info(dev, "cannot get biasn_gpio %ld\n", PTR_ERR(ctx->bias_neg));	
		return PTR_ERR(ctx->bias_neg);
	}
	devm_gpiod_put(dev, ctx->bias_neg);

	ctx->prepared = true;
	ctx->enabled = true;

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
	check_is_need_fake_resolution(dev);

	ctx->cabc_status = DDIC_CABC_OFF;

	bias_id = ktz8864_read_id();

	if(18 == bias_id) {
		reg_normal_max_brightness = REG_NORMAL_MAX_BRIGHTNESS_FOR_KTZ8864;
 		reg_hbm_max_brightness = REG_HBM_BRIGHTNESS_FOR_KTZ8864;

	} else {
		reg_normal_max_brightness = REG_NORMAL_MAX_BRIGHTNESS_FOR_AW9967;
		reg_hbm_max_brightness = REG_HBM_BRIGHTNESS_FOR_AW9967;
	}
	pr_info("[%s]:----bias_id = %d, reg_normal_max_brightness = %d-----\n", __func__, bias_id, reg_normal_max_brightness);

	pr_info("%s- boe,nt36525d,dphy,vdo,no,dsc\n", __func__);

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
	{ .compatible = "panel,lcm,video", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "dsi_panel_n85_35_02_0a_90_vdo",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Xiuhai Deng <xiuhai.deng@mediatek.com>");
MODULE_DESCRIPTION("nt36525d boe VDO LCD Panel Driver");
MODULE_LICENSE("GPL v2");
