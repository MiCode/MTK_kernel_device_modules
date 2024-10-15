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
#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_log.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int bpc;

	/**
	 * @width_mm: width of the panel's active display area
	 * @height_mm: height of the panel's active display area
	 */
	struct {
		unsigned int width_mm;
		unsigned int height_mm;
	} size;

	unsigned long mode_flags;
	enum mipi_dsi_pixel_format format;
	const struct panel_init_cmd *init_cmds;
	unsigned int lanes;
};

struct jd9635da {
	struct device *dev;
	struct mipi_dsi_device *dsi;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *vdd33_gpio;
	struct regulator *reg;
	bool prepared;
	bool enabled;
	int error;
};

#define jd9635da_dcs_write_seq(ctx, seq...)                                     \
	({                                                                     \
		const u8 d[] = {seq};                                          \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		jd9635da_dcs_write(ctx, d, ARRAY_SIZE(d));                      \
	})
#define jd9635da_dcs_write_seq_static(ctx, seq...)                              \
	({                                                                     \
		static const u8 d[] = {seq};                                   \
		jd9635da_dcs_write(ctx, d, ARRAY_SIZE(d));                      \
	})

static inline struct jd9635da *panel_to_jd9635da(struct drm_panel *panel)
{
	return container_of(panel, struct jd9635da, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int jd9635da_dcs_read(struct jd9635da *ctx, u8 cmd, void *data, size_t len)
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

static void jd9635da_panel_get_data(struct jd9635da *ctx)
{
	u8 buffer[3] = {0};
	static int ret;

	if (ret == 0) {
		ret = jd9635da_dcs_read(ctx, 0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void jd9635da_dcs_write(struct jd9635da *ctx, const void *data, size_t len)
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

static void jd9635da_panel_init(struct jd9635da *ctx)
{
	pr_info("%s +\n", __func__);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10000, 12000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(5000, 6000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(5000, 6000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	jd9635da_dcs_write_seq_static(ctx, 0xE0, 0x00);

	jd9635da_dcs_write_seq_static(ctx, 0xE1, 0x93);
	jd9635da_dcs_write_seq_static(ctx, 0xE2, 0x65);
	jd9635da_dcs_write_seq_static(ctx, 0xE3, 0xF8);
	jd9635da_dcs_write_seq_static(ctx, 0x80, 0x03);

	jd9635da_dcs_write_seq_static(ctx, 0xE0, 0x01);

	jd9635da_dcs_write_seq_static(ctx, 0x03, 0x00);
	jd9635da_dcs_write_seq_static(ctx, 0x04, 0x3C);

	jd9635da_dcs_write_seq_static(ctx, 0x0C, 0x74);
	jd9635da_dcs_write_seq_static(ctx, 0x17, 0x00);
	jd9635da_dcs_write_seq_static(ctx, 0x18, 0xF7);
	jd9635da_dcs_write_seq_static(ctx, 0x19, 0x01);
	jd9635da_dcs_write_seq_static(ctx, 0x1A, 0x00);
	jd9635da_dcs_write_seq_static(ctx, 0x1B, 0xF7);
	jd9635da_dcs_write_seq_static(ctx, 0x1C, 0x01);

	jd9635da_dcs_write_seq_static(ctx, 0x24, 0xFE);

	jd9635da_dcs_write_seq_static(ctx, 0x35, 0x23);

	jd9635da_dcs_write_seq_static(ctx, 0x37, 0x09);

	jd9635da_dcs_write_seq_static(ctx, 0x38, 0x04);
	jd9635da_dcs_write_seq_static(ctx, 0x39, 0x00);
	jd9635da_dcs_write_seq_static(ctx, 0x3A, 0x01);
	jd9635da_dcs_write_seq_static(ctx, 0x3C, 0x70);
	jd9635da_dcs_write_seq_static(ctx, 0x3D, 0xFF);
	jd9635da_dcs_write_seq_static(ctx, 0x3E, 0xFF);
	jd9635da_dcs_write_seq_static(ctx, 0x3F, 0x7F);

	jd9635da_dcs_write_seq_static(ctx, 0x40, 0x06);
	jd9635da_dcs_write_seq_static(ctx, 0x41, 0xA0);
	jd9635da_dcs_write_seq_static(ctx, 0x43, 0x1E);
	jd9635da_dcs_write_seq_static(ctx, 0x44, 0x0B);
	jd9635da_dcs_write_seq_static(ctx, 0x45, 0x28);

	jd9635da_dcs_write_seq_static(ctx, 0x55, 0x02);
	jd9635da_dcs_write_seq_static(ctx, 0x57, 0x69);
	jd9635da_dcs_write_seq_static(ctx, 0x59, 0x0A);
	jd9635da_dcs_write_seq_static(ctx, 0x5A, 0x2D);
	jd9635da_dcs_write_seq_static(ctx, 0x5B, 0x1A);
	jd9635da_dcs_write_seq_static(ctx, 0x5C, 0x15);

	jd9635da_dcs_write_seq_static(ctx, 0x5D, 0x7F);
	jd9635da_dcs_write_seq_static(ctx, 0x5E, 0x6E);
	jd9635da_dcs_write_seq_static(ctx, 0x5F, 0x5F);
	jd9635da_dcs_write_seq_static(ctx, 0x60, 0x54);
	jd9635da_dcs_write_seq_static(ctx, 0x61, 0x50);
	jd9635da_dcs_write_seq_static(ctx, 0x62, 0x42);
	jd9635da_dcs_write_seq_static(ctx, 0x63, 0x46);
	jd9635da_dcs_write_seq_static(ctx, 0x64, 0x30);
	jd9635da_dcs_write_seq_static(ctx, 0x65, 0x4A);
	jd9635da_dcs_write_seq_static(ctx, 0x66, 0x48);
	jd9635da_dcs_write_seq_static(ctx, 0x67, 0x47);
	jd9635da_dcs_write_seq_static(ctx, 0x68, 0x62);
	jd9635da_dcs_write_seq_static(ctx, 0x69, 0x4C);
	jd9635da_dcs_write_seq_static(ctx, 0x6A, 0x4E);
	jd9635da_dcs_write_seq_static(ctx, 0x6B, 0x3F);
	jd9635da_dcs_write_seq_static(ctx, 0x6C, 0x37);
	jd9635da_dcs_write_seq_static(ctx, 0x6D, 0x29);
	jd9635da_dcs_write_seq_static(ctx, 0x6E, 0x16);
	jd9635da_dcs_write_seq_static(ctx, 0x6F, 0x02);
	jd9635da_dcs_write_seq_static(ctx, 0x70, 0x7F);
	jd9635da_dcs_write_seq_static(ctx, 0x71, 0x6E);
	jd9635da_dcs_write_seq_static(ctx, 0x72, 0x5F);
	jd9635da_dcs_write_seq_static(ctx, 0x73, 0x54);
	jd9635da_dcs_write_seq_static(ctx, 0x74, 0x50);
	jd9635da_dcs_write_seq_static(ctx, 0x75, 0x42);
	jd9635da_dcs_write_seq_static(ctx, 0x76, 0x46);
	jd9635da_dcs_write_seq_static(ctx, 0x77, 0x30);
	jd9635da_dcs_write_seq_static(ctx, 0x78, 0x4A);
	jd9635da_dcs_write_seq_static(ctx, 0x79, 0x48);
	jd9635da_dcs_write_seq_static(ctx, 0x7A, 0x47);
	jd9635da_dcs_write_seq_static(ctx, 0x7B, 0x62);
	jd9635da_dcs_write_seq_static(ctx, 0x7C, 0x4C);
	jd9635da_dcs_write_seq_static(ctx, 0x7D, 0x4E);
	jd9635da_dcs_write_seq_static(ctx, 0x7E, 0x3F);
	jd9635da_dcs_write_seq_static(ctx, 0x7F, 0x37);
	jd9635da_dcs_write_seq_static(ctx, 0x80, 0x29);
	jd9635da_dcs_write_seq_static(ctx, 0x81, 0x16);
	jd9635da_dcs_write_seq_static(ctx, 0x82, 0x02);

	jd9635da_dcs_write_seq_static(ctx, 0xE0, 0x02);

	jd9635da_dcs_write_seq_static(ctx, 0x00, 0x50);
	jd9635da_dcs_write_seq_static(ctx, 0x01, 0x5F);
	jd9635da_dcs_write_seq_static(ctx, 0x02, 0x5F);
	jd9635da_dcs_write_seq_static(ctx, 0x03, 0x52);
	jd9635da_dcs_write_seq_static(ctx, 0x04, 0x77);
	jd9635da_dcs_write_seq_static(ctx, 0x05, 0x57);
	jd9635da_dcs_write_seq_static(ctx, 0x06, 0x5F);
	jd9635da_dcs_write_seq_static(ctx, 0x07, 0x4E);
	jd9635da_dcs_write_seq_static(ctx, 0x08, 0x4C);
	jd9635da_dcs_write_seq_static(ctx, 0x09, 0x5F);
	jd9635da_dcs_write_seq_static(ctx, 0x0A, 0x4A);
	jd9635da_dcs_write_seq_static(ctx, 0x0B, 0x48);
	jd9635da_dcs_write_seq_static(ctx, 0x0C, 0x5F);
	jd9635da_dcs_write_seq_static(ctx, 0x0D, 0x46);
	jd9635da_dcs_write_seq_static(ctx, 0x0E, 0x44);
	jd9635da_dcs_write_seq_static(ctx, 0x0F, 0x40);
	jd9635da_dcs_write_seq_static(ctx, 0x10, 0x5F);
	jd9635da_dcs_write_seq_static(ctx, 0x11, 0x5F);
	jd9635da_dcs_write_seq_static(ctx, 0x12, 0x5F);
	jd9635da_dcs_write_seq_static(ctx, 0x13, 0x5F);
	jd9635da_dcs_write_seq_static(ctx, 0x14, 0x5F);
	jd9635da_dcs_write_seq_static(ctx, 0x15, 0x5F);

	jd9635da_dcs_write_seq_static(ctx, 0x16, 0x51);
	jd9635da_dcs_write_seq_static(ctx, 0x17, 0x5F);
	jd9635da_dcs_write_seq_static(ctx, 0x18, 0x5F);
	jd9635da_dcs_write_seq_static(ctx, 0x19, 0x53);
	jd9635da_dcs_write_seq_static(ctx, 0x1A, 0x77);
	jd9635da_dcs_write_seq_static(ctx, 0x1B, 0x57);
	jd9635da_dcs_write_seq_static(ctx, 0x1C, 0x5F);
	jd9635da_dcs_write_seq_static(ctx, 0x1D, 0x4F);
	jd9635da_dcs_write_seq_static(ctx, 0x1E, 0x4D);
	jd9635da_dcs_write_seq_static(ctx, 0x1F, 0x5F);
	jd9635da_dcs_write_seq_static(ctx, 0x20, 0x4B);
	jd9635da_dcs_write_seq_static(ctx, 0x21, 0x49);
	jd9635da_dcs_write_seq_static(ctx, 0x22, 0x5F);
	jd9635da_dcs_write_seq_static(ctx, 0x23, 0x47);
	jd9635da_dcs_write_seq_static(ctx, 0x24, 0x45);
	jd9635da_dcs_write_seq_static(ctx, 0x25, 0x41);
	jd9635da_dcs_write_seq_static(ctx, 0x26, 0x5F);
	jd9635da_dcs_write_seq_static(ctx, 0x27, 0x5F);
	jd9635da_dcs_write_seq_static(ctx, 0x28, 0x5F);
	jd9635da_dcs_write_seq_static(ctx, 0x29, 0x5F);
	jd9635da_dcs_write_seq_static(ctx, 0x2A, 0x5F);
	jd9635da_dcs_write_seq_static(ctx, 0x2B, 0x5F);

	jd9635da_dcs_write_seq_static(ctx, 0x2C, 0x01);
	jd9635da_dcs_write_seq_static(ctx, 0x2D, 0x1F);
	jd9635da_dcs_write_seq_static(ctx, 0x2E, 0x1F);
	jd9635da_dcs_write_seq_static(ctx, 0x2F, 0x13);
	jd9635da_dcs_write_seq_static(ctx, 0x30, 0x17);
	jd9635da_dcs_write_seq_static(ctx, 0x31, 0x17);
	jd9635da_dcs_write_seq_static(ctx, 0x32, 0x1F);
	jd9635da_dcs_write_seq_static(ctx, 0x33, 0x0D);
	jd9635da_dcs_write_seq_static(ctx, 0x34, 0x0F);
	jd9635da_dcs_write_seq_static(ctx, 0x35, 0x1F);
	jd9635da_dcs_write_seq_static(ctx, 0x36, 0x05);
	jd9635da_dcs_write_seq_static(ctx, 0x37, 0x07);
	jd9635da_dcs_write_seq_static(ctx, 0x38, 0x1F);
	jd9635da_dcs_write_seq_static(ctx, 0x39, 0x09);
	jd9635da_dcs_write_seq_static(ctx, 0x3A, 0x0B);
	jd9635da_dcs_write_seq_static(ctx, 0x3B, 0x11);
	jd9635da_dcs_write_seq_static(ctx, 0x3C, 0x1F);
	jd9635da_dcs_write_seq_static(ctx, 0x3D, 0x1F);
	jd9635da_dcs_write_seq_static(ctx, 0x3E, 0x1F);
	jd9635da_dcs_write_seq_static(ctx, 0x3F, 0x1F);
	jd9635da_dcs_write_seq_static(ctx, 0x40, 0x1F);
	jd9635da_dcs_write_seq_static(ctx, 0x41, 0x1F);

	jd9635da_dcs_write_seq_static(ctx, 0x42, 0x00);
	jd9635da_dcs_write_seq_static(ctx, 0x43, 0x1F);
	jd9635da_dcs_write_seq_static(ctx, 0x44, 0x1F);
	jd9635da_dcs_write_seq_static(ctx, 0x45, 0x12);
	jd9635da_dcs_write_seq_static(ctx, 0x46, 0x17);
	jd9635da_dcs_write_seq_static(ctx, 0x47, 0x17);
	jd9635da_dcs_write_seq_static(ctx, 0x48, 0x1F);
	jd9635da_dcs_write_seq_static(ctx, 0x49, 0x0C);
	jd9635da_dcs_write_seq_static(ctx, 0x4A, 0x0E);
	jd9635da_dcs_write_seq_static(ctx, 0x4B, 0x1F);
	jd9635da_dcs_write_seq_static(ctx, 0x4C, 0x04);
	jd9635da_dcs_write_seq_static(ctx, 0x4D, 0x06);
	jd9635da_dcs_write_seq_static(ctx, 0x4E, 0x1F);
	jd9635da_dcs_write_seq_static(ctx, 0x4F, 0x08);
	jd9635da_dcs_write_seq_static(ctx, 0x50, 0x0A);
	jd9635da_dcs_write_seq_static(ctx, 0x51, 0x10);
	jd9635da_dcs_write_seq_static(ctx, 0x52, 0x1F);
	jd9635da_dcs_write_seq_static(ctx, 0x53, 0x1F);
	jd9635da_dcs_write_seq_static(ctx, 0x54, 0x1F);
	jd9635da_dcs_write_seq_static(ctx, 0x55, 0x1F);
	jd9635da_dcs_write_seq_static(ctx, 0x56, 0x1F);
	jd9635da_dcs_write_seq_static(ctx, 0x57, 0x1F);

	jd9635da_dcs_write_seq_static(ctx, 0x58, 0x40);
	jd9635da_dcs_write_seq_static(ctx, 0x5B, 0x10);
	jd9635da_dcs_write_seq_static(ctx, 0x5C, 0x06);
	jd9635da_dcs_write_seq_static(ctx, 0x5D, 0x40);
	jd9635da_dcs_write_seq_static(ctx, 0x5E, 0x00);
	jd9635da_dcs_write_seq_static(ctx, 0x5F, 0x00);
	jd9635da_dcs_write_seq_static(ctx, 0x60, 0x40);
	jd9635da_dcs_write_seq_static(ctx, 0x61, 0x03);
	jd9635da_dcs_write_seq_static(ctx, 0x62, 0x04);
	jd9635da_dcs_write_seq_static(ctx, 0x63, 0x6C);
	jd9635da_dcs_write_seq_static(ctx, 0x64, 0x6C);
	jd9635da_dcs_write_seq_static(ctx, 0x65, 0x75);
	jd9635da_dcs_write_seq_static(ctx, 0x66, 0x08);
	jd9635da_dcs_write_seq_static(ctx, 0x67, 0xB4);
	jd9635da_dcs_write_seq_static(ctx, 0x68, 0x08);
	jd9635da_dcs_write_seq_static(ctx, 0x69, 0x6C);
	jd9635da_dcs_write_seq_static(ctx, 0x6A, 0x6C);
	jd9635da_dcs_write_seq_static(ctx, 0x6B, 0x0C);
	jd9635da_dcs_write_seq_static(ctx, 0x6D, 0x00);
	jd9635da_dcs_write_seq_static(ctx, 0x6E, 0x00);
	jd9635da_dcs_write_seq_static(ctx, 0x6F, 0x88);
	jd9635da_dcs_write_seq_static(ctx, 0x75, 0xBB);
	jd9635da_dcs_write_seq_static(ctx, 0x76, 0x00);
	jd9635da_dcs_write_seq_static(ctx, 0x77, 0x05);
	jd9635da_dcs_write_seq_static(ctx, 0x78, 0x2A);

	jd9635da_dcs_write_seq_static(ctx, 0xE0, 0x04);
	jd9635da_dcs_write_seq_static(ctx, 0x00, 0x0E);
	jd9635da_dcs_write_seq_static(ctx, 0x02, 0xB3);
	jd9635da_dcs_write_seq_static(ctx, 0x09, 0x60);
	jd9635da_dcs_write_seq_static(ctx, 0x0E, 0x48);

	jd9635da_dcs_write_seq_static(ctx, 0xE0, 0x00);
	jd9635da_dcs_write_seq_static(ctx, 0x11);
	msleep(120);
	jd9635da_dcs_write_seq_static(ctx, 0x29);
	msleep(50);
	jd9635da_dcs_write_seq_static(ctx, 0x35, 0x00);

	pr_info("%s -\n", __func__);
}

static int jd9635da_disable(struct drm_panel *panel)
{
	struct jd9635da *ctx = panel_to_jd9635da(panel);

	pr_info("%s+++\n", __func__);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	pr_info("%s---\n", __func__);

	return 0;
}

static int jd9635da_unprepare(struct drm_panel *panel)
{
	struct jd9635da *ctx = panel_to_jd9635da(panel);

	pr_info("%s+++\n", __func__);

	if (!ctx->prepared)
		return 0;

	jd9635da_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	msleep(50);
	jd9635da_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	msleep(120);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(5000, 6000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	ctx->vdd33_gpio = devm_gpiod_get(ctx->dev, "vdd33", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->vdd33_gpio, 0);
	usleep_range(2000, 3000);
	devm_gpiod_put(ctx->dev, ctx->vdd33_gpio);

	ctx->error = 0;
	ctx->prepared = false;

	pr_info("%s---\n", __func__);

	return 0;
}
static int jd9635da_prepare(struct drm_panel *panel)
{
	struct jd9635da *ctx = panel_to_jd9635da(panel);
	int ret;

	pr_info("%s+++\n", __func__);

	if (ctx->prepared)
		return 0;

	ctx->vdd33_gpio = devm_gpiod_get(ctx->dev, "vdd33", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->vdd33_gpio, 1);
	usleep_range(5000, 6000);
	devm_gpiod_put(ctx->dev, ctx->vdd33_gpio);

	jd9635da_panel_init(ctx);
	ret = ctx->error;
	if (ret < 0) {
		pr_info("Send initial code error!\n");
		jd9635da_unprepare(panel);
	}

	ctx->prepared = true;

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif

#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

	pr_info("%s---\n", __func__);

	return ret;
}

static int jd9635da_enable(struct drm_panel *panel)
{
	struct jd9635da *ctx = panel_to_jd9635da(panel);

	pr_info("%s+++\n", __func__);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	pr_info("%s---\n", __func__);

	return 0;
}

static const struct drm_display_mode default_mode = {
	.clock = 68215,
	.hdisplay = 800,
	.hsync_start = 800 + 20,
	.hsync_end = 800 + 20 + 20,
	.htotal = 800 + 20 + 20 + 20,
	.vdisplay = 1280,
	.vsync_start = 1280 + 30,
	.vsync_end = 1280 + 30 + 4,
	.vtotal = 1280 + 30 + 4 + 8,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.pll_clk = 210,
	.physical_width_um = 108000,
	.physical_height_um = 173000,
};

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct jd9635da *ctx = panel_to_jd9635da(panel);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
};
#endif

static int jd9635da_get_modes(struct drm_panel *panel,
					struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	pr_info("%s+++\n", __func__);

	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		pr_info("failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}
	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = 108;
	connector->display_info.height_mm = 173;

	return 1;
}

static const struct drm_panel_funcs jd9635da_drm_funcs = {
	.disable = jd9635da_disable,
	.unprepare = jd9635da_unprepare,
	.prepare = jd9635da_prepare,
	.enable = jd9635da_enable,
	.get_modes = jd9635da_get_modes,
};

static int jd9635da_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *backlight;
	struct jd9635da *ctx;
	int ret;

	pr_info("%s+++\n", __func__);

	ctx = devm_kzalloc(dev, sizeof(struct jd9635da), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

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

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "cannot get reset-gpios %ld\n",
			PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->vdd33_gpio = devm_gpiod_get(dev, "vdd33", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vdd33_gpio)) {
		dev_err(dev, "cannot get vdd33-gpios %ld\n",
			PTR_ERR(ctx->vdd33_gpio));
		return PTR_ERR(ctx->vdd33_gpio);
	}
	devm_gpiod_put(dev, ctx->vdd33_gpio);

	ctx->prepared = true;
	ctx->enabled = true;
	drm_panel_init(&ctx->panel, dev, &jd9635da_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	ctx->panel.dev = dev;
	ctx->panel.funcs = &jd9635da_drm_funcs;

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		drm_panel_remove(&ctx->panel);
		dev_err(dev, "mipi_dsi_attach fail, ret=%d\n", ret);
		return -EPROBE_DEFER;
	}

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif

	pr_info("%s-\n", __func__);

	return ret;
}
static void jd9635da_remove(struct mipi_dsi_device *dsi)
{
	struct jd9635da *ctx = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	if (ext_ctx != NULL) {
		mtk_panel_detach(ext_ctx);
		mtk_panel_remove(ext_ctx);
	}
#endif
}
static const struct of_device_id jd9635da_of_match[] = {
	{
		.compatible = "jd9635da,gq31093gq",
	},
	{}
};
MODULE_DEVICE_TABLE(of, jd9635da_of_match);
static struct mipi_dsi_driver jd9635da_driver = {
	.probe = jd9635da_probe,
	.remove = jd9635da_remove,
	.driver = {
			.name = "panel-jd9635da-ts127qfmll1dkp0",
			.owner = THIS_MODULE,
			.of_match_table = jd9635da_of_match,
		},
};
module_mipi_dsi_driver(jd9635da_driver);
MODULE_AUTHOR("Huijuan Xie <huijuan.xie@mediatek.com>");
MODULE_DESCRIPTION("jd9635da gq31093gq wxga Panel Driver");
MODULE_LICENSE("GPL");

