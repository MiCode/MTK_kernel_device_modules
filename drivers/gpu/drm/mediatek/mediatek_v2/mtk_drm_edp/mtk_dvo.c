// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Jie Qiu <jie.qiu@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/media-bus-format.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/types.h>

#include <video/videomode.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_bridge_connector.h>
#include <drm/drm_crtc.h>
#include <drm/drm_edid.h>
#include <drm/drm_of.h>
#include <drm/drm_simple_kms_helper.h>

#include "mtk_dvo_regs.h"
#include "../mtk_drm_drv.h"
#include "../mtk_drm_crtc.h"
#include "../mtk_drm_ddp_comp.h"


enum mtk_dvo_out_bit_num {
	MTK_DVO_OUT_BIT_NUM_8BITS,
	MTK_DVO_OUT_BIT_NUM_10BITS,
	MTK_DVO_OUT_BIT_NUM_12BITS,
	MTK_DVO_OUT_BIT_NUM_16BITS
};

enum mtk_dvo_out_yc_map {
	MTK_DVO_OUT_YC_MAP_RGB,
	MTK_DVO_OUT_YC_MAP_CYCY,
	MTK_DVO_OUT_YC_MAP_YCYC,
	MTK_DVO_OUT_YC_MAP_CY,
	MTK_DVO_OUT_YC_MAP_YC
};

enum mtk_dvo_out_channel_swap {
	MTK_DVO_OUT_CHANNEL_SWAP_RGB,
	MTK_DVO_OUT_CHANNEL_SWAP_GBR,
	MTK_DVO_OUT_CHANNEL_SWAP_BRG,
	MTK_DVO_OUT_CHANNEL_SWAP_RBG,
	MTK_DVO_OUT_CHANNEL_SWAP_GRB,
	MTK_DVO_OUT_CHANNEL_SWAP_BGR
};

enum mtk_dvo_out_color_format {
	MTK_DVO_COLOR_FORMAT_RGB,
	MTK_DVO_COLOR_FORMAT_YCBCR_422
};

enum TVDPLL_CLK {
	TVDPLL_PLL = 0,
	TVDPLL_D2 = 2,
	TVDPLL_D4 = 4,
	TVDPLL_D8 = 8,
	TVDPLL_D16 = 16,
};

enum mtk_dvo_golden_setting_level {
	MTK_DVO_FHD_60FPS_1920 = 0,
	MTK_DVO_FHD_60FPS_2180,
	MTK_DVO_FHD_60FPS_2400,
	MTK_DVO_FHD_60FPS_2520,
	MTK_DVO_FHD_90FPS,
	MTK_DVO_FHD_120FPS,
	MTK_DVO_WQHD_60FPS,
	MTK_DVO_WQHD_120FPS,
	MTK_DVO_8K_60FPS,
	MTK_DVO_GSL_MAX,
};

struct mtk_dvo_gs_info {
	u32 dvo_buf_sodi_high;
	u32 dvo_buf_sodi_low;
};

struct mtk_dvo {
	struct mtk_ddp_comp ddp_comp;
	struct drm_encoder encoder;
	struct drm_bridge bridge;
	struct drm_bridge *next_bridge;
	struct drm_connector *connector;
	void __iomem *regs;
	struct device *dev;
	struct device *mmsys_dev;
	struct clk *engine_clk;
	struct clk *pixel_clk;
	struct clk *tvd_clk;
	struct clk *dvo_clk;
	struct clk *pclk_src[5];
	int irq;
	struct drm_display_mode mode;
	const struct mtk_dvo_conf *conf;
	enum mtk_dvo_out_color_format color_format;
	enum mtk_dvo_out_yc_map yc_map;
	enum mtk_dvo_out_bit_num bit_num;
	enum mtk_dvo_out_channel_swap channel_swap;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_gpio;
	struct pinctrl_state *pins_dvo;
	u32 output_fmt;
	int refcount;
	enum mtk_dvo_golden_setting_level gs_level;
};

static inline struct mtk_dvo *bridge_to_dvo(struct drm_bridge *b)
{
	return container_of(b, struct mtk_dvo, bridge);
}

enum mtk_dvo_polarity {
	MTK_DVO_POLARITY_RISING,
	MTK_DVO_POLARITY_FALLING,
};

struct mtk_dvo_polarities {
	enum mtk_dvo_polarity de_pol;
	enum mtk_dvo_polarity ck_pol;
	enum mtk_dvo_polarity hsync_pol;
	enum mtk_dvo_polarity vsync_pol;
};

struct mtk_dvo_sync_param {
	u32 sync_width;
	u32 front_porch;
	u32 back_porch;
	bool shift_half_line;
};

struct mtk_dvo_yc_limit {
	u16 y_top;
	u16 y_bottom;
	u16 c_top;
	u16 c_bottom;
};

/**
 * struct mtk_dvo_conf - Configuration of mediatek dvo.
 * @cal_factor: Callback function to calculate factor value.
 * @reg_h_fre_con: Register address of frequency control.
 * @max_clock_khz: Max clock frequency supported for this SoCs in khz units.
 * @edge_sel_en: Enable of edge selection.
 * @output_fmts: Array of supported output formats.
 * @num_output_fmts: Quantity of supported output formats.
 * @is_ck_de_pol: Support CK/DE polarity.
 * @swap_input_support: Support input swap function.
 * @support_direct_pin: IP supports direct connection to dvo panels.
 * @input_2pixel: Input pixel of dp_intf is 2 pixel per round, so enable this
 *		  config to enable this feature.
 * @dimension_mask: Mask used for HWIDTH, HPORCH, VSYNC_WIDTH and VSYNC_PORCH
 *		    (no shift).
 * @hvsize_mask: Mask of HSIZE and VSIZE mask (no shift).
 * @channel_swap_shift: Shift value of channel swap.
 * @yuv422_en_bit: Enable bit of yuv422.
 * @csc_enable_bit: Enable bit of CSC.
 * @pixels_per_iter: Quantity of transferred pixels per iteration.
 * @edge_cfg_in_mmsys: If the edge configuration for DVO's output needs to be set in MMSYS.
 */
struct mtk_dvo_conf {
	unsigned int (*cal_factor)(int clock);
	u32 reg_h_fre_con;
	u32 max_clock_khz;
	bool edge_sel_en;
	const u32 *output_fmts;
	u32 num_output_fmts;
	bool is_ck_de_pol;
	bool swap_input_support;
	bool support_direct_pin;
	bool input_2pixel;
	u32 out_np_sel;
	u32 dimension_mask;
	u32 hvsize_mask;
	u32 channel_swap_shift;
	u32 yuv422_en_bit;
	u32 csc_enable_bit;
	u32 pixels_per_iter;
	bool edge_cfg_in_mmsys;
	bool has_commit;
};

static int irq_intsa;
static int irq_vdesa;
static int irq_underflowsa;
static int irq_tl;

static struct mtk_dvo_gs_info mtk_dvo_gs[MTK_DVO_GSL_MAX] = {
	[MTK_DVO_FHD_60FPS_1920] = {6880, 511},
};

static void mtk_dvo_mask(struct mtk_dvo *dvo, u32 offset, u32 val, u32 mask)
{
	u32 tmp = readl(dvo->regs + offset) & ~mask;

	tmp |= (val & mask);
	writel(tmp, dvo->regs + offset);
}

static void mtk_dvo_sw_reset(struct mtk_dvo *dvo, bool reset)
{
	mtk_dvo_mask(dvo, DVO_RET, reset ? SWRST : 0, SWRST);
}

static void mtk_dvo_enable(struct mtk_dvo *dvo)
{
	mtk_dvo_mask(dvo, DVO_EN, EN, EN);
}

static void mtk_dvo_disable(struct mtk_dvo *dvo)
{
	mtk_dvo_mask(dvo, DVO_EN, 0, EN);
}

static void mtk_dvo_irq_enable(struct mtk_dvo *dvo)
{
	mtk_dvo_mask(dvo, DVO_INTEN, INT_VDE_END_EN, INT_VDE_END_EN);
}

static void mtk_dvo_info_queue_start(struct mtk_dvo *dvo)
{
	mtk_dvo_mask(dvo, DVO_TGEN_INFOQ_LATENCY, 0, INFOQ_START_LATENCY_MASK | INFOQ_END_LATENCY_MASK);
}

static void mtk_dvo_buffer_ctrl(struct mtk_dvo *dvo)
{
	mtk_dvo_mask(dvo, DVO_BUF_CON0, DISP_BUF_EN, DISP_BUF_EN);
	mtk_dvo_mask(dvo, DVO_BUF_CON0, FIFO_UNDERFLOW_DONE_BLOCK, FIFO_UNDERFLOW_DONE_BLOCK);
}

static void mtk_dvo_pm_ctl(struct mtk_dvo *dvo, bool enable)
{
	u32 ret = 0;
	void *address = ioremap(0x31B50074, 0x1);

	if (enable) {
		ret = clk_prepare_enable(dvo->pixel_clk);
		if (ret)
			dev_info(dvo->dev, "[eDPTX]Failed to enable pixel clock: %d\n", ret);

		/* set DVO switch 26Mhz crystal */
		clk_set_parent(dvo->pixel_clk, dvo->dvo_clk);

		/* DISP_EDPTX_PWR_CON */
		pr_info("[eDPTX] DISP_EDPTX_PWR_CON enabled\n");
		writel(0xC2FC224D, address);
	} else {
		pr_info("[eDPTX] DISP_EDPTX_PWR_CON disabled\n");
		writel(0xC2FC2372, address);
	}

	if (address)
		iounmap(address);

}

static void mtk_dvo_trailing_blank_setting(struct mtk_dvo *dvo)
{
	//shuijing hard code, why 20
	mtk_dvo_mask(dvo, DVO_TGEN_V_LAST_TRAILING_BLANK, 0x20, V_LAST_TRAILING_BLANK_MASK);
	mtk_dvo_mask(dvo, DVO_TGEN_OUTPUT_DELAY_LINE, 0x20, EXT_TG_DLY_LINE_MASK);
}

static void mtk_dvo_get_gs_level(struct mtk_dvo *dvo)
{
	struct drm_display_mode *mode = &dvo->mode;
	enum mtk_dvo_golden_setting_level *gsl = &dvo->gs_level;

	if (mode->hdisplay == 1920 && mode->vdisplay == 1080)
		*gsl = MTK_DVO_FHD_60FPS_1920;
	else
		*gsl = MTK_DVO_FHD_60FPS_1920;

	pr_info("[eDPTX] %s gs_level %d\n",
		__func__, dvo->gs_level);
}

static void mtk_dvo_golden_setting(struct mtk_dvo *dvo)
{
	struct mtk_dvo_gs_info *gs_info = NULL;

	if (dvo->gs_level >= MTK_DVO_GSL_MAX) {
		pr_info("[eDPTX] %s invalid gs_level %d\n",
			__func__, dvo->gs_level);
		return;
	}

	gs_info = &mtk_dvo_gs[dvo->gs_level];

	pr_info("[eDPTX] %s gs_level %d sodi %d %d\n",
		__func__, dvo->gs_level,
		gs_info->dvo_buf_sodi_high,
		gs_info->dvo_buf_sodi_low);

	mtk_dvo_mask(dvo, DVO_BUF_SODI_HIGHT, gs_info->dvo_buf_sodi_high, 0xffffffff);
	mtk_dvo_mask(dvo, DVO_BUF_SODI_LOW, gs_info->dvo_buf_sodi_low, 0xffffffff);
}

static void mtk_dvo_shadow_ctrl(struct mtk_dvo *dvo)
{
	mtk_dvo_mask(dvo, DVO_SHADOW_CTRL, 0, BYPASS_SHADOW);
	mtk_dvo_mask(dvo, DVO_SHADOW_CTRL, FORCE_COMMIT, FORCE_COMMIT);
}

static void mtk_dvo_config_hsync(struct mtk_dvo *dvo,
				 struct mtk_dvo_sync_param *sync)
{
	mtk_dvo_mask(dvo, DVO_TGEN_H0, sync->sync_width << HSYNC,
		     dvo->conf->dimension_mask << HSYNC);
	mtk_dvo_mask(dvo, DVO_TGEN_H0, sync->front_porch << HFP,
		     dvo->conf->dimension_mask << HFP);
	mtk_dvo_mask(dvo, DVO_TGEN_H1, (sync->back_porch + sync->sync_width) << HSYNC2ACT,
		 dvo->conf->dimension_mask << HSYNC2ACT);
}

static void mtk_dvo_config_vsync(struct mtk_dvo *dvo,
				 struct mtk_dvo_sync_param *sync,
				 u32 width_addr, u32 porch_addr)
{
	mtk_dvo_mask(dvo, width_addr,
		     sync->sync_width << VSYNC,
		     dvo->conf->dimension_mask << VSYNC);
	mtk_dvo_mask(dvo, width_addr,
		     sync->front_porch << VFP,
		     dvo->conf->dimension_mask << VFP);
	mtk_dvo_mask(dvo, porch_addr,
	     (sync->back_porch + sync->sync_width) << VSYNC2ACT,
	     dvo->conf->dimension_mask << VSYNC2ACT);
}

static void mtk_dvo_config_vsync_lodd(struct mtk_dvo *dvo,
				      struct mtk_dvo_sync_param *sync)
{
	mtk_dvo_config_vsync(dvo, sync, DVO_TGEN_V0, DVO_TGEN_V1);
}

static irqreturn_t mtk_dvo_irq_status(int irq, void *dev_id)
{
	struct mtk_dvo *dvo = dev_id;
	u32 status = 0;
	struct mtk_drm_crtc *mtk_crtc;

	status = readl(dvo->regs + DVO_INTSTA);

	status &= 0xFFFFFFFF;
	if (status) {
		mtk_dvo_mask(dvo, DVO_INTSTA, 0, status);
		if (status & INT_VSYNC_START_STA) {
			mtk_crtc = dvo->ddp_comp.mtk_crtc;
			if (mtk_crtc)
				mtk_crtc_vblank_irq(&mtk_crtc->base);
			irq_intsa++;
		}

		if (status & INT_VDE_START_STA)
			irq_vdesa++;

		if (status & INT_UNDERFLOW_STA)
			irq_underflowsa++;

		if (status & INT_TARGET_LINE0_STA)
			irq_tl++;
	}

	return IRQ_HANDLED;
}

static void mtk_dvo_config_interface(struct mtk_dvo *dvo, bool inter)
{
	mtk_dvo_mask(dvo, DVO_CON, inter ? INTL_EN : 0, INTL_EN);
}

static void mtk_dvo_config_fb_size(struct mtk_dvo *dvo, u32 width, u32 height)
{
	mtk_dvo_mask(dvo, DVO_SRC_SIZE, width << SRC_HSIZE,
		     dvo->conf->hvsize_mask << SRC_HSIZE);
	mtk_dvo_mask(dvo, DVO_SRC_SIZE, height << SRC_VSIZE,
		     dvo->conf->hvsize_mask << SRC_VSIZE);

	mtk_dvo_mask(dvo, DVO_PIC_SIZE, width << PIC_HSIZE,
		     dvo->conf->hvsize_mask << PIC_HSIZE);
	mtk_dvo_mask(dvo, DVO_PIC_SIZE, height << PIC_VSIZE,
		     dvo->conf->hvsize_mask << PIC_VSIZE);

	mtk_dvo_mask(dvo, DVO_TGEN_H1, (width/4) << HACT,
		     dvo->conf->hvsize_mask << HACT);
	mtk_dvo_mask(dvo, DVO_TGEN_V1, height << VACT,
		     dvo->conf->hvsize_mask << VACT);
}

static void mtk_dvo_power_off(struct mtk_dvo *dvo)
{
	if (WARN_ON(dvo->refcount == 0))
		return;

	if (--dvo->refcount != 0)
		return;

	mtk_dvo_disable(dvo);
	clk_disable_unprepare(dvo->tvd_clk);
	clk_disable_unprepare(dvo->pixel_clk);
	clk_disable_unprepare(dvo->engine_clk);
}

static int mtk_dvo_power_on(struct mtk_dvo *dvo)
{
	int ret;

	if (++dvo->refcount != 1)
		return 0;

	ret = clk_prepare_enable(dvo->tvd_clk);
	if (ret) {
		dev_info(dvo->dev, "[eDPTX]Failed to enable tvd_clk clock: %d\n", ret);
		goto err_refcount;
	}

	ret = clk_prepare_enable(dvo->engine_clk);
	if (ret) {
		dev_info(dvo->dev, "Failed to enable engine clock: %d\n", ret);
		goto err_refcount;
	}

	ret = clk_prepare_enable(dvo->pixel_clk);
	if (ret) {
		dev_info(dvo->dev, "Failed to enable pixel clock: %d\n", ret);
		goto err_pixel;
	}

	return 0;

err_pixel:
	clk_disable_unprepare(dvo->engine_clk);
err_refcount:
	dvo->refcount--;
	return ret;
}

static int mtk_dvo_set_display_mode(struct mtk_dvo *dvo,
				    struct drm_display_mode *mode)
{
	struct mtk_dvo_sync_param hsync;
	struct mtk_dvo_sync_param vsync_lodd = { 0 };
	struct videomode vm = { 0 };
	unsigned long pll_rate;
	unsigned int factor;

	pr_info("[eDPTX] %s+\n", __func__);

	/* let pll_rate can fix the valid range of tvdpll (1G~2GHz) */
	factor = dvo->conf->cal_factor(mode->clock);
	drm_display_mode_to_videomode(mode, &vm);

	pr_notice("[eDPTX] vm.pixelclock=%lu vm.hactive=%d vm.hfront_porch=%d",
				vm.pixelclock, vm.hactive, vm.hfront_porch);
	pr_notice(" vm.hback_porch=%d vm.vsync_len=%d\n",
				vm.hback_porch, vm.hsync_len);

	pr_notice("[eDPTX]  vm.vactive=%d vm.vfront_porch=%d",
				vm.vactive, vm.vfront_porch);

	pr_notice("[eDPTX] vm.vback_porch=%d vm.vsync_len=%d\n",
				vm.vback_porch, vm.vsync_len);

	pll_rate = vm.pixelclock * factor;

	pr_info("[eDPTX] Want PLL %lu Hz, pixel clock %lu Hz\n",
		pll_rate, vm.pixelclock);

	clk_set_rate(dvo->tvd_clk, pll_rate);
	pll_rate = clk_get_rate(dvo->tvd_clk);

	/*
	 * Depending on the IP version, we may output a different amount of
	 * pixels for each iteration: divide the clock by this number and
	 * adjust the display porches accordingly.
	 */
	vm.pixelclock = pll_rate / factor;
	vm.pixelclock /= dvo->conf->pixels_per_iter;

	if (factor == 1)
		clk_set_parent(dvo->pixel_clk, dvo->pclk_src[2]);
	else if (factor == 2)
		clk_set_parent(dvo->pixel_clk, dvo->pclk_src[3]);
	else if (factor == 4)
		clk_set_parent(dvo->pixel_clk, dvo->pclk_src[4]);
	else
		clk_set_parent(dvo->pixel_clk, dvo->pclk_src[2]);

	vm.pixelclock = clk_get_rate(dvo->pixel_clk);

	pr_info("[eDPTX] Got  PLL %lu Hz, pixel clock %lu Hz\n",
		pll_rate, vm.pixelclock);

	/*
	 * Depending on the IP version, we may output a different amount of
	 * pixels for each iteration: divide the clock by this number and
	 * adjust the display porches accordingly.
	 */
	hsync.sync_width = vm.hsync_len / dvo->conf->pixels_per_iter;
	hsync.back_porch = vm.hback_porch / dvo->conf->pixels_per_iter;
	hsync.front_porch = vm.hfront_porch / dvo->conf->pixels_per_iter;

	hsync.shift_half_line = false;
	vsync_lodd.sync_width = vm.vsync_len;
	vsync_lodd.back_porch = vm.vback_porch;
	vsync_lodd.front_porch = vm.vfront_porch;
	vsync_lodd.shift_half_line = false;

	mtk_dvo_sw_reset(dvo, true);
	pr_info("[eDPTX] %s-\n", __func__);
	mtk_dvo_irq_enable(dvo);

	mtk_dvo_config_hsync(dvo, &hsync);
	mtk_dvo_config_vsync_lodd(dvo, &vsync_lodd);

	mtk_dvo_config_interface(dvo, !!(vm.flags &
					 DISPLAY_FLAGS_INTERLACED));

	if (vm.flags & DISPLAY_FLAGS_INTERLACED)
		mtk_dvo_config_fb_size(dvo, vm.hactive, vm.vactive >> 1);
	else
		mtk_dvo_config_fb_size(dvo, vm.hactive, vm.vactive);

	mtk_dvo_info_queue_start(dvo);
	mtk_dvo_buffer_ctrl(dvo);
	mtk_dvo_trailing_blank_setting(dvo);

	mtk_dvo_get_gs_level(dvo);
	mtk_dvo_golden_setting(dvo);

	if (dvo->conf->pixels_per_iter)
		mtk_dvo_shadow_ctrl(dvo);

	if (dvo->conf->out_np_sel) {
		mtk_dvo_mask(dvo, DVO_OUTPUT_SET, dvo->conf->out_np_sel,
			     OUT_NP_SEL);
	}
	mtk_dvo_sw_reset(dvo, false);

	pr_info("[eDPTX] %s-\n", __func__);

	return 0;
}

static u32 *mtk_dvo_bridge_atomic_get_output_bus_fmts(struct drm_bridge *bridge,
						      struct drm_bridge_state *bridge_state,
						      struct drm_crtc_state *crtc_state,
						      struct drm_connector_state *conn_state,
						      unsigned int *num_output_fmts)
{
	struct mtk_dvo *dvo = bridge_to_dvo(bridge);
	u32 *output_fmts;

	*num_output_fmts = 0;

	pr_info("[eDPTX] %s\n", __func__);
	if (!dvo->conf->output_fmts) {
		dev_info(dvo->dev, "output_fmts should not be null\n");
		return NULL;
	}

	output_fmts = kcalloc(dvo->conf->num_output_fmts, sizeof(*output_fmts),
			     GFP_KERNEL);
	if (!output_fmts)
		return NULL;

	*num_output_fmts = dvo->conf->num_output_fmts;

	memcpy(output_fmts, dvo->conf->output_fmts,
	       sizeof(*output_fmts) * dvo->conf->num_output_fmts);

	return output_fmts;
}

static u32 *mtk_dvo_bridge_atomic_get_input_bus_fmts(struct drm_bridge *bridge,
						     struct drm_bridge_state *bridge_state,
						     struct drm_crtc_state *crtc_state,
						     struct drm_connector_state *conn_state,
						     u32 output_fmt,
						     unsigned int *num_input_fmts)
{
	u32 *input_fmts;

	*num_input_fmts = 0;

	pr_info("[eDPTX] %s\n", __func__);
	input_fmts = kcalloc(1, sizeof(*input_fmts),
			     GFP_KERNEL);
	if (!input_fmts)
		return NULL;

	*num_input_fmts = 1;
	input_fmts[0] = MEDIA_BUS_FMT_RGB888_1X24;

	return input_fmts;
}

static int mtk_dvo_bridge_atomic_check(struct drm_bridge *bridge,
				       struct drm_bridge_state *bridge_state,
				       struct drm_crtc_state *crtc_state,
				       struct drm_connector_state *conn_state)
{
	struct mtk_dvo *dvo = bridge_to_dvo(bridge);
	unsigned int out_bus_format;

	out_bus_format = bridge_state->output_bus_cfg.format;

	pr_info("[eDPTX] %s\n", __func__);
	if (out_bus_format == MEDIA_BUS_FMT_FIXED)
		if (dvo->conf->num_output_fmts)
			out_bus_format = dvo->conf->output_fmts[0];

	dev_dbg(dvo->dev, "input format 0x%04x, output format 0x%04x\n",
		bridge_state->input_bus_cfg.format,
		bridge_state->output_bus_cfg.format);

	dvo->output_fmt = out_bus_format;
	dvo->bit_num = MTK_DVO_OUT_BIT_NUM_8BITS;
	dvo->channel_swap = MTK_DVO_OUT_CHANNEL_SWAP_RGB;
	dvo->yc_map = MTK_DVO_OUT_YC_MAP_RGB;
	if (out_bus_format == MEDIA_BUS_FMT_YUYV8_1X16)
		dvo->color_format = MTK_DVO_COLOR_FORMAT_YCBCR_422;
	else
		dvo->color_format = MTK_DVO_COLOR_FORMAT_RGB;

	return 0;
}

static int mtk_dvo_bridge_attach(struct drm_bridge *bridge,
				 enum drm_bridge_attach_flags flags)
{
	struct mtk_dvo *dvo = bridge_to_dvo(bridge);
	int ret = 0;

	ret = drm_bridge_attach(bridge->encoder, dvo->next_bridge,
				 &dvo->bridge, flags);
	if (ret)
		dev_info(dvo->dev, "[eDPTX] Failed to call attach ret = %d\n", ret);

	return ret;
}

static void mtk_dvo_bridge_mode_set(struct drm_bridge *bridge,
				const struct drm_display_mode *mode,
				const struct drm_display_mode *adjusted_mode)
{
	struct mtk_dvo *dvo = bridge_to_dvo(bridge);

	pr_info("[eDPTX] %s\n", __func__);
	drm_mode_copy(&dvo->mode, adjusted_mode);
}

static void mtk_dvo_bridge_disable(struct drm_bridge *bridge)
{
	struct mtk_dvo *dvo = bridge_to_dvo(bridge);

	pr_info("[eDPTX] %s\n", __func__);

	clk_set_parent(dvo->pixel_clk, dvo->dvo_clk);
	mtk_dvo_power_off(dvo);

	if (dvo->pinctrl && dvo->pins_gpio)
		pinctrl_select_state(dvo->pinctrl, dvo->pins_gpio);
}

static void mtk_dvo_bridge_enable(struct drm_bridge *bridge)
{
	struct mtk_dvo *dvo = bridge_to_dvo(bridge);

	dev_info(dvo->dev, "[eDPTX] %s+\n", __func__);
	if (dvo->pinctrl && dvo->pins_dvo)
		pinctrl_select_state(dvo->pinctrl, dvo->pins_dvo);

	mtk_dvo_power_on(dvo);
	mtk_dvo_set_display_mode(dvo, &dvo->mode);
	mtk_dvo_enable(dvo);

	dev_info(dvo->dev, "[eDPTX] %s-\n", __func__);
}

static enum drm_mode_status
mtk_dvo_bridge_mode_valid(struct drm_bridge *bridge,
			  const struct drm_display_info *info,
			  const struct drm_display_mode *mode)
{
	struct mtk_dvo *dvo = bridge_to_dvo(bridge);

	pr_info("[eDPTX] %s\n", __func__);
	if (mode->clock > dvo->conf->max_clock_khz) {
		pr_info("[eDPTX] Invalid mode mode->clock= %d\n", mode->clock);
		return MODE_CLOCK_HIGH;
	}

	pr_info("[eDPTX] Valid mode mode->clock=%d\n", mode->clock);
	return MODE_OK;
}

static const struct drm_bridge_funcs mtk_dvo_bridge_funcs = {
	.attach = mtk_dvo_bridge_attach,
	.mode_set = mtk_dvo_bridge_mode_set,
	.mode_valid = mtk_dvo_bridge_mode_valid,
	.disable = mtk_dvo_bridge_disable,
	.enable = mtk_dvo_bridge_enable,
	.atomic_check = mtk_dvo_bridge_atomic_check,
	.atomic_get_output_bus_fmts = mtk_dvo_bridge_atomic_get_output_bus_fmts,
	.atomic_get_input_bus_fmts = mtk_dvo_bridge_atomic_get_input_bus_fmts,
	.atomic_duplicate_state = drm_atomic_helper_bridge_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_bridge_destroy_state,
	.atomic_reset = drm_atomic_helper_bridge_reset,
};

void mtk_dvo_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_dvo *dvo = container_of(comp, struct mtk_dvo, ddp_comp);

	dev_info(dvo->dev, "[eDPTX] %s\n", __func__);
}

void mtk_dvo_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_dvo *dvo = container_of(comp, struct mtk_dvo, ddp_comp);

	dev_info(dvo->dev, "[eDPTX] %s\n", __func__);

	//mtk_dvo_power_off(dvo);
}

unsigned long long mtk_dvo_get_frame_hrt_bw_base_by_datarate(
		struct mtk_drm_crtc *mtk_crtc,
		struct mtk_dvo *dvo)
{
	static unsigned long long bw_base;
	int hact, vtotal, vact, vrefresh;

	if (!mtk_crtc) {
		DDPDBG("%s mtk_crtc is null\n", __func__);
		return 0;
	}

	hact = mtk_crtc->base.state->adjusted_mode.hdisplay;
	vtotal = mtk_crtc->base.state->adjusted_mode.vtotal;
	vact = mtk_crtc->base.state->adjusted_mode.vdisplay;
	vrefresh = drm_mode_vrefresh(&mtk_crtc->base.state->adjusted_mode);

	bw_base = (unsigned long long)vact * (unsigned long long)hact * vrefresh * 4 / 1000;
	bw_base = bw_base * vtotal / vact;
	bw_base = bw_base / 1000;

	DDPDBG("Frame Bw:%llu",	bw_base);
	return bw_base;
}

static int mtk_dvo_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			  enum mtk_ddp_io_cmd cmd, void *params)
{
	struct mtk_dvo *dvo = container_of(comp, struct mtk_dvo, ddp_comp);
	struct videomode vm = { 0 };
	struct drm_display_mode **mode;
	unsigned int vfporch, vbporch, vsync, hfporch, hbporch, hsync;

	drm_display_mode_to_videomode(&dvo->mode, &vm);

	switch (cmd) {
	case DSI_GET_TIMING:
	{
		mode = (struct drm_display_mode **)params;

		dvo->mode.hdisplay = readl(dvo->regs + DVO_SIZE) & 0xFFFF;
		dvo->mode.vdisplay = readl(dvo->regs + DVO_SIZE) >> 16;
		vfporch = readl(dvo->regs + DVO_TGEN_VPORCH) >> 16;
		vbporch = readl(dvo->regs + DVO_TGEN_VPORCH) & 0xFFFF;
		vsync = readl(dvo->regs + DVO_TGEN_VWIDTH) & 0xFFFF;
		dvo->mode.vtotal = dvo->mode.vdisplay + vfporch + vbporch + vsync;
		hfporch = readl(dvo->regs + DVO_TGEN_HPORCH) >> 16;
		hbporch = readl(dvo->regs + DVO_TGEN_HPORCH) & 0xFFFF;
		hsync = readl(dvo->regs + DVO_TGEN_HWIDTH) & 0xFFFF;
		dvo->mode.htotal = dvo->mode.hdisplay + hfporch + hbporch + hsync;
		dvo->mode.clock = clk_get_rate(dvo->pixel_clk) * 4 / 1000;
		dev_info(dvo->dev, "[eDPTX] %s cmd:%d\n", __func__, cmd);
		dev_info(dvo->dev, "[eDPTX] dvo->hdisplay = %d\n", dvo->mode.hdisplay);
		dev_info(dvo->dev, "[eDPTX] dvo->vdisplay = %d\n", dvo->mode.vdisplay);
		dev_info(dvo->dev, "[eDPTX] dvo->clock = %d, fps: %d\n",
			dvo->mode.clock, drm_mode_vrefresh(&dvo->mode));
		*mode = &dvo->mode;
	}
		break;
	case SET_MMCLK_BY_DATARATE:
	{
#ifndef CONFIG_FPGA_EARLY_PORTING
		struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
		unsigned int pixelclock = vm.pixelclock / 1000000;

		mtk_drm_set_mmclk_by_pixclk(&(mtk_crtc->base),
			pixelclock, __func__);
#endif
	}
		break;
	case GET_FRAME_HRT_BW_BY_DATARATE:
	{
		struct mtk_drm_crtc *crtc = comp->mtk_crtc;
		unsigned long long *base_bw =
			(unsigned long long *)params;

		*base_bw = mtk_dvo_get_frame_hrt_bw_base_by_datarate(crtc, dvo);
	}
		break;
	default:
		break;
	}
	return 0;
}

static const struct mtk_ddp_comp_funcs mtk_dvo_funcs = {
	.start = mtk_dvo_start,
	.stop = mtk_dvo_stop,
	.io_cmd = mtk_dvo_io_cmd,
};

static int mtk_dvo_bind(struct device *dev, struct device *master, void *data)
{
	struct mtk_dvo *dvo = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	struct mtk_drm_private *priv = drm_dev->dev_private;
	int ret;

	dev_info(dev, "[eDPTX] %s+\n", __func__);

	dvo->mmsys_dev = priv->mmsys_dev;
	ret = mtk_ddp_comp_register(drm_dev, &dvo->ddp_comp);
	if (ret < 0) {
		dev_info(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}
	ret = drm_simple_encoder_init(drm_dev, &dvo->encoder,
				      DRM_MODE_ENCODER_TMDS);
	if (ret) {
		dev_info(dev, "Failed to initialize decoder: %d\n", ret);
		return ret;
	}

	dvo->encoder.possible_crtcs = mtk_drm_find_possible_crtc_by_comp(drm_dev, dvo->ddp_comp);

	ret = drm_bridge_attach(&dvo->encoder, &dvo->bridge, NULL,
				DRM_BRIDGE_ATTACH_NO_CONNECTOR);
	if (ret)
		goto err_cleanup;

	dvo->connector = drm_bridge_connector_init(drm_dev, &dvo->encoder);
	if (IS_ERR(dvo->connector)) {
		dev_info(dev, "Unable to create bridge connector\n");
		ret = PTR_ERR(dvo->connector);
		goto err_cleanup;
	}
	drm_connector_attach_encoder(dvo->connector, &dvo->encoder);

	dev_info(dev, "[eDPTX] %s-\n", __func__);

	return 0;

err_cleanup:
	drm_encoder_cleanup(&dvo->encoder);
	return ret;
}

static void mtk_dvo_unbind(struct device *dev, struct device *master,
			   void *data)
{
	struct mtk_dvo *dvo = dev_get_drvdata(dev);

	drm_encoder_cleanup(&dvo->encoder);
}

static const struct component_ops mtk_dvo_component_ops = {
	.bind = mtk_dvo_bind,
	.unbind = mtk_dvo_unbind,
};

static unsigned int mt6991_calculate_factor(int clock)
{
	if (clock < 70000)
		return 4;
	else if (clock < 200000)
		return 2;
	else
		return 1;

}

static const u32 mt6991_output_fmts[] = {
	MEDIA_BUS_FMT_RGB888_1X24,
	MEDIA_BUS_FMT_YUYV8_1X16,
};

static const struct mtk_dvo_conf mt6991_conf = {
	.cal_factor = mt6991_calculate_factor,
	.out_np_sel = 0X2,
	.reg_h_fre_con = 0xb0,
	.max_clock_khz = 600000,
	.output_fmts = mt6991_output_fmts,
	.num_output_fmts = ARRAY_SIZE(mt6991_output_fmts),
	.pixels_per_iter = 4,
	.has_commit = true,
	.dimension_mask = HFP_MASK,
	.hvsize_mask = PIC_HSIZE_MASK,
};

static int mtk_dvo_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_dvo *dvo;
	int comp_id;
	int ret;

	dev_info(dev, "[eDPTX] %s+\n", __func__);
	dvo = devm_kzalloc(dev, sizeof(*dvo), GFP_KERNEL);
	if (!dvo)
		return -ENOMEM;

	dvo->dev = dev;
	dvo->conf = (struct mtk_dvo_conf *)of_device_get_match_data(dev);
	dvo->output_fmt = MEDIA_BUS_FMT_RGB888_1X24;

	dvo->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(dvo->pinctrl)) {
		dvo->pinctrl = NULL;
		dev_dbg(&pdev->dev, "Cannot find pinctrl!\n");
	}
	if (dvo->pinctrl) {
		dvo->pins_gpio = pinctrl_lookup_state(dvo->pinctrl, "sleep");
		if (IS_ERR(dvo->pins_gpio)) {
			dvo->pins_gpio = NULL;
			dev_dbg(&pdev->dev, "Cannot find pinctrl idle!\n");
		}
		if (dvo->pins_gpio)
			pinctrl_select_state(dvo->pinctrl, dvo->pins_gpio);

		dvo->pins_dvo = pinctrl_lookup_state(dvo->pinctrl, "default");
		if (IS_ERR(dvo->pins_dvo)) {
			dvo->pins_dvo = NULL;
			dev_dbg(&pdev->dev, "Cannot find pinctrl active!\n");
		}
	}
	dvo->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(dvo->regs)) {
		pr_info("[eDPTX ] Failed to ioremap mem resource\n");
		return PTR_ERR(dvo->regs);
	}

	dvo->engine_clk = devm_clk_get(dev, "engine");
	if (IS_ERR(dvo->engine_clk)) {
		pr_info("[eDPTX ] Failed to get engine clock\n");
		return PTR_ERR(dvo->engine_clk);
	}

	dvo->pixel_clk = devm_clk_get(dev, "pixel");
	if (IS_ERR(dvo->pixel_clk)) {
		pr_info("[eDPTX ] Failed to get pixel clock\n");
		return PTR_ERR(dvo->pixel_clk);
	}

	dvo->tvd_clk = devm_clk_get(dev, "pll");
	if (IS_ERR(dvo->tvd_clk)) {
		pr_info("[eDPTX ] Failed to get tvdpll clock\n");
		return PTR_ERR(dvo->tvd_clk);
	}

	dvo->dvo_clk = devm_clk_get(dev, "dvo_clk");
	if (IS_ERR(dvo->dvo_clk)) {
		pr_info("[eDPTX ] Failed to get tck26_clk clock\n");
		return PTR_ERR(dvo->dvo_clk);
	}

	dvo->pclk_src[1] = devm_clk_get(dev, "TVDPLL_D2");
	dvo->pclk_src[2] = devm_clk_get(dev, "TVDPLL_D4");
	dvo->pclk_src[3] = devm_clk_get(dev, "TVDPLL_D8");
	dvo->pclk_src[4] = devm_clk_get(dev, "TVDPLL_D16");

	dvo->irq = platform_get_irq(pdev, 0);
	if (dvo->irq < 0) {
		pr_info("[eDPTX] failed to get edp dvo irq num\n");
		return dvo->irq;
	}

	irq_set_status_flags(dvo->irq, IRQ_TYPE_LEVEL_HIGH);
	ret = devm_request_irq(
		&pdev->dev, dvo->irq, mtk_dvo_irq_status,
		IRQ_TYPE_LEVEL_HIGH, dev_name(&pdev->dev), dvo);
	if (ret) {
		dev_info(&pdev->dev, "failed to request mediatek edp dvo irq\n");
		return -EPROBE_DEFER;
	}


	dvo->next_bridge = devm_drm_of_get_bridge(dev, dev->of_node, 0, 0);
	if (IS_ERR(dvo->next_bridge)) {
		pr_info("[eDPTX] Failed to get bridge\n");
		return PTR_ERR(dvo->next_bridge);
	}

	dev_info(dev, "[eDPTX] Found bridge node: %pOF\n", dvo->next_bridge->of_node);

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_DVO);
	if (comp_id < 0) {
		dev_info(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &dvo->ddp_comp, comp_id,
				&mtk_dvo_funcs);
	if (ret) {
		dev_info(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, dvo);

	dvo->bridge.funcs = &mtk_dvo_bridge_funcs;
	dvo->bridge.of_node = dev->of_node;
	dvo->bridge.type = DRM_MODE_CONNECTOR_DPI;

	ret = devm_drm_bridge_add(dev, &dvo->bridge);
	if (ret)
		return ret;

	mtk_dvo_pm_ctl(dvo, true);
	pm_runtime_enable(dev);

	ret = component_add(dev, &mtk_dvo_component_ops);
	if (ret) {
		pr_info("[eDPTX] Failed to add component\n");
		return ret;
	}

	dev_info(dev, "[eDPTX] %s-\n", __func__);

	return 0;
}

static void mtk_dvo_remove(struct platform_device *pdev)
{
	struct mtk_dvo *dvo = platform_get_drvdata(pdev);

	mtk_dvo_pm_ctl(dvo, false);
	pm_runtime_disable(&pdev->dev);
	component_del(&pdev->dev, &mtk_dvo_component_ops);
}

static const struct of_device_id mtk_dvo_of_ids[] = {
	{ .compatible = "mediatek,mt6991-edp-dvo", .data = &mt6991_conf },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, mtk_dvo_of_ids);

struct platform_driver mtk_dvo_driver = {
	.probe = mtk_dvo_probe,
	.remove_new = mtk_dvo_remove,
	.driver = {
		.name = "mediatek-dvo",
		.of_match_table = mtk_dvo_of_ids,
	},
};

