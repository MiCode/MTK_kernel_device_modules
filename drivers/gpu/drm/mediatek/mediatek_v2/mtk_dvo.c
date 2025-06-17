// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_drm_helper.h"
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_fb.h"
#include "mtk_dp.h"

#define DVO_EN						0x00
#define EN							BIT(0)

#define DVO_RST						0x04
#define SWRST						BIT(0)

#define DVO_INTEN					0x08
#define INT_VFP_START_EN			BIT(0)
#define INT_VSYNC_START_EN			BIT(1)
#define INT_VSYNC_END_EN			BIT(2)
#define INT_VDE_START_EN			BIT(3)
#define INT_VDE_END_EN				BIT(4)
#define INT_WR_INFOQ_REG_EN			BIT(5)
#define INT_TARGET_LINE0_EN			BIT(6)
#define INT_TARGET_LINE1_EN			BIT(7)
#define INT_TARGET_LINE2_EN			BIT(8)
#define INT_TARGET_LINE3_EN			BIT(9)
#define INT_WR_INFOQ_START_EN		BIT(10)
#define INT_WR_INFOQ_END_EN			BIT(11)
#define EXT_VSYNC_START_EN			BIT(12)
#define EXT_VSYNC_END_EN			BIT(13)
#define EXT_VDE_START_EN			BIT(14)
#define EXT_VDE_END_EN				BIT(15)
#define EXT_VBLANK_END_EN			BIT(16)
#define UNDERFLOW_EN				BIT(17)
#define INFOQ_ABORT_EN				BIT(18)

#define DVO_INTSTA					0x0C
#define INT_VFP_START_STA			BIT(0)
#define INT_VSYNC_START_STA			BIT(1)
#define INT_VSYNC_END_STA			BIT(2)
#define INT_VDE_START_STA			BIT(3)
#define INT_VDE_END_STA				BIT(4)
#define INT_WR_INFOQ_REG_STA		BIT(5)
#define INT_TARGET_LINE0_STA		BIT(6)
#define INT_TARGET_LINE1_STA		BIT(7)
#define INT_TARGET_LINE2_STA		BIT(8)
#define INT_TARGET_LINE3_STA		BIT(9)
#define INT_WR_INFOQ_START_STA		BIT(10)
#define INT_WR_INFOQ_END_STA		BIT(11)
#define EXT_VSYNC_START_STA			BIT(12)
#define EXT_VSYNC_END_STA			BIT(13)
#define EXT_VDE_START_STA			BIT(14)
#define EXT_VDE_END_STA				BIT(15)
#define EXT_VBLANK_END_STA			BIT(16)
#define INT_UNDERFLOW_STA			BIT(17)
#define INFOQ_ABORT_STA				BIT(18)

#define DVO_OUTPUT_SET				0x18
#define DVO_OUT_1T1P_SEL			0x0
#define DVO_OUT_1T2P_SEL			0x1
#define DVO_OUT_1T4P_SEL			0x2
#define OUT_NP_SEL_MASK				0x3

#define BIT_SWAP					BIT(4)
#define CH_SWAP_MASK				(0x7 << 5)
#define CH_SWAP_SHIFT				0x5
#define SWAP_RGB					0x00
#define SWAP_GBR					0x01
#define SWAP_BRG					0x02
#define SWAP_RBG					0x03
#define SWAP_GRB					0x04
#define SWAP_BGR					0x05
#define PXL_SWAP					BIT(8)
#define R_MASK						BIT(12)
#define G_MASK						BIT(13)
#define B_MASK						BIT(14)
#define DE_MASK						BIT(16)
#define HS_MASK						BIT(17)
#define VS_MASK						BIT(18)
#define HS_INV						BIT(19)
#define VS_INV						BIT(20)
#define DE_POL						BIT(19)
#define HSYNC_POL					BIT(20)
#define VSYNC_POL					BIT(21)
#define CK_POL						BIT(22)

#define DVO_SRC_SIZE				0x20
#define SRC_HSIZE					0
#define SRC_HSIZE_MASK				(0xFFFF << 0)
#define SRC_VSIZE					16
#define SRC_VSIZE_MASK				(0xFFFF << 16)

#define DVO_PIC_SIZE				0x24
#define PIC_HSIZE					0
#define PIC_HSIZE_MASK				(0xFFFF << 0)
#define PIC_VSIZE					16
#define PIC_VSIZE_MASK				(0xFFFF << 16)

#define DVO_OUT_HSIZE				0x28

#define DVO_TGEN_H0					0x50
#define HFP							0
#define HFP_MASK					(0xFFFF << 0)
#define HSYNC						16
#define HSYNC_MASK					(0xFFFF << 16)

#define DVO_TGEN_H1					0x54
#define HSYNC2ACT					0
#define HSYNC2ACT_MASK				(0xFFFF << 0)
#define HACT						16
#define HACT_MASK					(0xFFFF << 16)

#define DVO_TGEN_V0					0x58
#define VFP							0
#define VFP_MASK					(0xFFFF << 0)
#define VSYNC						16
#define VSYNC_MASK					(0xFFFF << 16)

#define DVO_TGEN_V1					0x5C
#define VSYNC2ACT					0
#define VSYNC2ACT_MASK				(0xFFFF << 0)
#define VACT						16
#define VACT_MASK					(0xFFFF << 16)

#define DVO_TGEN_INFOQ_LATENCY		0x80
#define INFOQ_START_LATENCY			0
#define INFOQ_START_LATENCY_MASK	(0xFFFF << 0)
#define INFOQ_END_LATENCY			16
#define INFOQ_END_LATENCY_MASK		(0xFFFF << 16)

#define DVO_MUTEX_VSYNC_SET			0x84
#define MUTEX_VFP					4
#define MUTEX_VSYNC_SEL				BIT(0)

#define DVO_MATRIX_SET				0x140
#define CSC_EN						BIT(0)
#define MATRIX_SEL_RGB_TO_JPEG      (0x0 << 4)
#define MATRIX_SEL_RGB_TO_BT601     (0x2 << 4)
#define MATRIX_SEL_RGB_TO_BT709     (0x3 << 4)
#define INT_MTX_SEL					(0x2 << 4)
#define INT_MTX_SEL_MASK            GENMASK(8, 4)

#define DVO_YUV422_SET				0x170
#define YUV422_EN					BIT(0)
#define VYU_MAP						BIT(8)

#define DVO_BUF_CON0				0x220
#define DISP_BUF_EN					BIT(0)
#define FIFO_UNDERFLOW_DONT_BLOCK	BIT(4)

#define DVO_TGEN_V_LAST_TRAILING_BLANK	0x6c
#define V_LAST_TRAILING_BLANK			0
#define V_LAST_TRAILING_BLANK_MASK		(0xFFFF << 0)

#define DVO_TGEN_OUTPUT_DELAY_LINE	0x7c
#define EXT_TG_DLY_LINE				0
#define EXT_TG_DLY_LINE_MASK		(0xFFFF << 0)

#define DVO_PATTERN_CTRL			0x100
#define PRE_PAT_EN					BIT(0)
#define PRE_PAT_SEL_MASK			(0x7 << 4)
#define COLOR_BAR					(0x4<<4)
#define PRE_PAT_FORCE_ON			BIT(8)

#define DVO_PATTERN_COLOR			0x104
#define PAT_R						(0x3FF << 0)
#define PAT_G						(0x3FF << 10)
#define PAT_B						(0x3FF << 20)

#define DVO_SHADOW_CTRL				0x190
#define FORCE_COMMIT				BIT(0)
#define BYPASS_SHADOW				BIT(1)
#define READ_WRK_REG				BIT(2)

#define DVO_BUF_RW_TIMES			0x22C
#define DVO_BUF_SODI_HIGHT			0x230
#define DVO_BUF_SODI_LOW			0x234
#define DVO_BUF_VDE					0x258
#define DISP_BUF_VDE_BLOCK_URGENT			BIT(0)
#define DISP_BUF_NON_VDE_FORCE_PREULTRA		BIT(1)
#define DISP_BUF_VDE_BLOCK_ULTRA				BIT(2)
#define DVO_DISP_BUF_MASK			0xFFFFFFFF

#define DVO_STATUS					0xE00

#define DVO_CHECKSUM_EN				0xE10
#define CHKSUM_READY				BIT(4)
#define CHKSUM_EN					BIT(0)

#define DVO_CHECKSUM0				0xE14
#define DVO_CHECKSUM1				0xE18
#define DVO_CHECKSUM2				0xE1C
#define DVO_CHECKSUM3				0xE20
#define DVO_CHECKSUM4				0xE24
#define DVO_CHECKSUM5				0xE28
#define DVO_CHECKSUM6				0xE2C
#define DVO_CHECKSUM7				0xE30

#define dvo_dp_sel_lsb				24

#define DP_BUF_SODI_HIGH				0x0230
#define DP_BUF_SODI_LOW					0x0234
#define DP_BUF_PREULTRA_HIGH			0x0240
#define DP_BUF_PREULTRA_LOW				0x0244
#define DP_BUF_ULTRA_HIGH				0x0248
#define DP_BUF_ULTRA_LOW				0x024C
#define DP_BUF_URGENT_HIGH				0x0250
#define DP_BUF_URGENT_LOW				0x0254

static const struct of_device_id mtk_dvo_driver_dt_match[];
/**
 * struct mtk_dp_dvo - DP_dvo driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 */
struct mtk_dp_dvo {
	struct mtk_ddp_comp	 ddp_comp;
	struct device *dev;
	struct mtk_dp_dvo_driver_data *driver_data;
	struct drm_encoder encoder;
	struct drm_connector conn;
	void __iomem *regs;
	struct clk *hf_fmm_ck;
	struct clk *hf_fdp_ck;
	struct clk *hf_fdp_ck_src_mux;
	struct clk *hf_fdp_ck_src_pll;
	struct clk *hf_fdp_ck_src[10];
	int irq;
	struct drm_display_mode mode;
	struct mtk_drm_private *priv;
	int enable;
	int res;
	int config_time;
	int config_line;
};


// porting by clock table
enum  mt6993_hf_fdp_ck_src{
	mt6993_TCK_26M,
	mt6993_TVDPLL_D8,
	mt6993_TVDPLL_D4,
	mt6993_TVDPLL_D3,
	mt6993_TVDPLL_D2,
};

struct mtk_dp_dvo_driver_data {
	s32 (*poll_for_idle)(struct mtk_dp_dvo *dp_dvo,
		struct cmdq_pkt *handle);
	irqreturn_t (*irq_handler)(int irq, void *dev_id);
	void (*get_pll_clk)(struct mtk_dp_dvo *dp_dvo);
	void (*pll_mux_config)(struct mtk_dp_dvo *dp_dvo, unsigned int *clksrc, unsigned int *pll_rate);
};

static int irq_intsa;
static int irq_underflowsa;
static unsigned long long dp_dvo_bw;
static struct mtk_dp_dvo *g_dp_dvo;
static unsigned int checksum_array[8] = {0};

module_param_array(checksum_array, hexint, NULL, 0644);
MODULE_PARM_DESC(checksum_array, "dvo 8 checksum values");

module_param(irq_underflowsa, int, 0644);
MODULE_PARM_DESC(irq_underflowsa, "dvo irq underflow counter");

static inline struct mtk_dp_dvo *comp_to_dp_dvo(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_dp_dvo, ddp_comp);
}

static inline struct mtk_dp_dvo *encoder_to_dp_dvo(struct drm_encoder *e)
{
	return container_of(e, struct mtk_dp_dvo, encoder);
}

static inline struct mtk_dp_dvo *connector_to_dp_dvo(struct drm_connector *c)
{
	return container_of(c, struct mtk_dp_dvo, conn);
}

static void mtk_dp_dvo_mask(struct mtk_dp_dvo *dp_dvo, u32 offset,
	u32 mask, u32 data)
{
	u32 temp = readl(dp_dvo->regs + offset);

	writel((temp & ~mask) | (data & mask), dp_dvo->regs + offset);
}

void dp_dvo_dump_reg(void)
{
	u32 i, val[4], reg;

	for (i = 0x0; i < 0x100; i += 16) {
		reg = i;
		val[0] = readl(g_dp_dvo->regs + reg);
		val[1] = readl(g_dp_dvo->regs + reg + 4);
		val[2] = readl(g_dp_dvo->regs + reg + 8);
		val[3] = readl(g_dp_dvo->regs + reg + 12);
		DPTXMSG("dp_dvo reg[0x%x] = 0x%x 0x%x 0x%x 0x%x\n",
			reg, val[0], val[1], val[2], val[3]);
	}
}

static void mtk_dp_dvo_destroy_conn_enc(struct mtk_dp_dvo *dp_dvo)
{
	drm_encoder_cleanup(&dp_dvo->encoder);
	/* Skip connector cleanup if creation was delegated to the bridge */
	if (dp_dvo->conn.dev)
		drm_connector_cleanup(&dp_dvo->conn);
}

static void mtk_dp_dvo_start(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle)
{
	struct mtk_dp_dvo *dp_dvo = comp_to_dp_dvo(comp);

	DPTXFUNC();

	irq_intsa = 0;
	irq_underflowsa = 0;
	dp_dvo_bw = 0;

	mtk_ddp_write_mask(comp, 0,
		DVO_INTSTA, ~0, handle);
	mtk_ddp_write_mask(comp, 1,
		DVO_RST, SWRST, handle);
	mtk_ddp_write_mask(comp, 0,
		DVO_RST, SWRST, handle);
	mtk_ddp_write_mask(comp,
			(UNDERFLOW_EN |
			 INT_VDE_END_EN | INT_VSYNC_START_EN),
			DVO_INTEN,
			(UNDERFLOW_EN |
			 INT_VDE_END_EN | INT_VSYNC_START_EN), handle);

	//mtk_ddp_write_mask(comp, DP_PATTERN_EN,
	//	DP_PATTERN_CTRL0, DP_PATTERN_EN, handle);
	//mtk_ddp_write_mask(comp, DP_PATTERN_COLOR_BAR,
	//	DP_PATTERN_CTRL0, DP_PATTERN_COLOR_BAR, handle);


	//DVO function on
	mtk_ddp_write_mask(comp, EN,
		DVO_EN, EN, handle);

	dp_dvo->enable = 1;
}

static void mtk_dp_dvo_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_dp_dvo *dp_dvo = comp_to_dp_dvo(comp);

	DPTXFUNC();
	//mtk_dp_video_trigger(video_mute<<16 | 0);
	mtk_ddp_write_mask(comp, 0, DVO_EN, EN, handle);
	mtk_ddp_write_mask(comp, 0, DVO_INTEN, ~0, handle);
	mtk_ddp_write_mask(comp, 0, DVO_INTSTA, ~0, handle);
	irq_intsa = 0;
	irq_underflowsa = 0;
	dp_dvo_bw = 0;
}

static void mtk_dp_dvo_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_dp_dvo *dp_dvo = NULL;
	int ret;

	DPTXFUNC();
	mtk_dp_poweron();

	dp_dvo = comp_to_dp_dvo(comp);

	/* Enable dp dvo clk */
	if (dp_dvo != NULL) {
		ret = clk_prepare_enable(dp_dvo->priv->pwr_clks[CLK_DPTX]);
		if (ret < 0)
			DPTXERR("Failed to enable dptx power: %d\n", ret);

		ret = clk_prepare_enable(dp_dvo->hf_fdp_ck_src_mux);
		if (ret < 0)
			DPTXERR("%s Failed to enable mux: %d\n",
				__func__, ret);

		ret = clk_prepare_enable(dp_dvo->hf_fmm_ck);
		if (ret < 0)
			DPTXERR("%s Failed to enable hf_fmm_ck clock: %d\n",
				__func__, ret);

		ret = clk_prepare_enable(dp_dvo->hf_fdp_ck);
		if (ret < 0)
			DPTXERR("%s Failed to enable hf_fdp_ck clock: %d\n",
				__func__, ret);

		ret = clk_prepare_enable(dp_dvo->hf_fdp_ck_src_pll);
		if (ret < 0)
			DPTXERR("%s Failed to enable pll clock: %d\n",
				__func__, ret);
	} else
		DPTXERR("Failed to enable dp_dvo clock\n");
}

void mtk_dp_dvo_PatternGenEn(int mode)
{
	if (mode == 1) {
		writel(0x141, g_dp_dvo->regs + DVO_PATTERN_CTRL);
		DPTXMSG("[DP Debug]dp dvo pg enable(color bar)\n");
	} else if (mode == 2) {
		writel(0x41, g_dp_dvo->regs + DVO_PATTERN_CTRL);
		DPTXMSG("[DP Debug]dp dvo forced pg enable(color bar)\n");
	} else if (mode == 3) {
		writel(0x131, g_dp_dvo->regs + DVO_PATTERN_CTRL);
		DPTXMSG("[DP Debug]dp dvo pg enable(horizontal gray)\n");
	} else if (mode == 4) {
		writel(0x31, g_dp_dvo->regs + DVO_PATTERN_CTRL);
		DPTXMSG("[DP Debug]dp dvo forced pg enable(horizontal gray)\n");
	} else {
		// pattern gen close
		writel(0x0, g_dp_dvo->regs + DVO_PATTERN_CTRL);
		DPTXMSG("[DP Debug]dp dvo pg disable\n");
	}
}
EXPORT_SYMBOL(mtk_dp_dvo_PatternGenEn);

void mtk_dp_dvo_ChecksumTrigger(void)
{
	uint32_t checksum_en;
	uint32_t ms_of_one_frame;
	uint32_t vrefresh;

	writel(CHKSUM_EN, g_dp_dvo->regs + DVO_CHECKSUM_EN);
	vrefresh = drm_mode_vrefresh(&g_dp_dvo->mode);
	ms_of_one_frame = 1000 / vrefresh;
	mdelay(ms_of_one_frame * 4);

	checksum_en = readl(g_dp_dvo->regs + DVO_CHECKSUM_EN);
	if (checksum_en & CHKSUM_READY) {
		checksum_array[0] = readl(g_dp_dvo->regs + DVO_CHECKSUM0);
		checksum_array[1] = readl(g_dp_dvo->regs + DVO_CHECKSUM1);
		checksum_array[2] = readl(g_dp_dvo->regs + DVO_CHECKSUM2);
		checksum_array[3] = readl(g_dp_dvo->regs + DVO_CHECKSUM3);
		checksum_array[4] = readl(g_dp_dvo->regs + DVO_CHECKSUM4);
		checksum_array[5] = readl(g_dp_dvo->regs + DVO_CHECKSUM5);
		checksum_array[6] = readl(g_dp_dvo->regs + DVO_CHECKSUM6);
		checksum_array[7] = readl(g_dp_dvo->regs + DVO_CHECKSUM7);
	} else {
		checksum_array[0] = 0;
		checksum_array[1] = 0;
		checksum_array[2] = 0;
		checksum_array[3] = 0;
		checksum_array[4] = 0;
		checksum_array[5] = 0;
		checksum_array[6] = 0;
		checksum_array[7] = 0;
		DPTXERR("DVO checksum not ready, return 0\n");
	}
}
EXPORT_SYMBOL(mtk_dp_dvo_ChecksumTrigger);

static void mtk_dp_dvo_unprepare(struct mtk_ddp_comp *comp)
{
	struct mtk_dp_dvo *dp_dvo = NULL;

	DPTXFUNC();
	dp_dvo = comp_to_dp_dvo(comp);

	/* disable dp dvo clk */
	if (dp_dvo != NULL) {
		clk_disable_unprepare(dp_dvo->hf_fdp_ck_src_pll);
		clk_disable_unprepare(dp_dvo->hf_fdp_ck);
		clk_disable_unprepare(dp_dvo->hf_fmm_ck);
		clk_set_rate(dp_dvo->hf_fdp_ck_src_pll, 594000000);
		clk_disable_unprepare(dp_dvo->hf_fdp_ck_src_mux);
		clk_disable_unprepare(dp_dvo->priv->pwr_clks[CLK_DPTX]);
	} else
		DPTXERR("Failed to disable dp_dvo clock\n");
}

void mtk_dvo_video_clock(struct mtk_dp_dvo *dp_dvo)
{
	unsigned int clksrc;
	unsigned int pll_rate;
	int ret = 0;

	DPTXFUNC();
	if (dp_dvo == NULL) {
		DPTXERR("%s:input error\n", __func__);
		return;
	}

	dp_dvo->driver_data->pll_mux_config(dp_dvo, &clksrc, &pll_rate);

	ret = clk_set_rate(dp_dvo->hf_fdp_ck_src_pll, pll_rate);
	if (ret)
		DPTXERR("%s clk_set_rate dp_dvo->hf_fdp_ck_src_pll: err=%d\n", __func__, ret);

	ret = clk_set_parent(dp_dvo->hf_fdp_ck_src_mux, dp_dvo->hf_fdp_ck_src[clksrc]);
	if (ret)
		DPTXERR("%s clk_set_parent dp_dvo->hf_fdp_ck_src_mux: err=%d\n",
			__func__, ret);

	// print clk rates
	DPTXMSG("set clksrc and src and pll rate %d,%d\n", clksrc, pll_rate);
	DPTXMSG("%s dp dvo->hf_fdp_ck_src_mux =  %ld\n",
		__func__, clk_get_rate(g_dp_dvo->hf_fdp_ck_src_mux));
	DPTXMSG("%s dp dvo->hf_fmm_ck =  %ld\n",
		__func__, clk_get_rate(g_dp_dvo->hf_fmm_ck));
	DPTXMSG("%s dp dvo->hf_fdp_ck =  %ld\n",
		__func__, clk_get_rate(g_dp_dvo->hf_fdp_ck));
	DPTXMSG("%s dp dvo->hf_fdp_ck_src_pll =  %ld\n",
		__func__, clk_get_rate(g_dp_dvo->hf_fdp_ck_src_pll));
}

static void mtk_dp_dvo_golden_setting(struct mtk_ddp_comp *comp,
					    struct cmdq_pkt *handle)
{
	struct mtk_dp_dvo *dp_dvo = comp_to_dp_dvo(comp);
	unsigned int pixel_clk_rate, threshold_unit, bpp, consume_rate;
	unsigned int fifo_size = 5569;
	unsigned int ultra_high_fifo_us, ultra_low_fifo_us;
	unsigned int urgent_high_fifo_us, urgent_low_fifo_us;
	unsigned int dp_buf_sodi_high, dp_buf_sodi_low;
	unsigned int dp_buf_preultra_high, dp_buf_preultra_low;
	unsigned int dp_buf_ultra_high, dp_buf_ultra_low;
	unsigned int dp_buf_urgent_high, dp_buf_urgent_low;

	DPTXFUNC();

	//parameter setting
	//htt * vtt * fps = dp_ck_rate * 4 (output: 1t4p)
	pixel_clk_rate = clk_get_rate(dp_dvo->hf_fdp_ck) * 4;
	threshold_unit = 32;
	bpp = 30;
	//Use 1000 times consume_rate to avoid truncation errors
	consume_rate = (uint32_t) ((uint64_t)pixel_clk_rate * bpp / (8 * threshold_unit) / 1000);

	//ultra low cal
	//fifo_size * 0.8 / (consume_rate / 1000)
	ultra_low_fifo_us = (fifo_size * 1000 * 8) / (10 * consume_rate);
	ultra_low_fifo_us = (ultra_low_fifo_us >= 35) ? ultra_low_fifo_us : 35;
	ultra_high_fifo_us = ultra_low_fifo_us + 1;

	//urgent low is 60%, urgent high is 80%
	urgent_low_fifo_us = (fifo_size * 1000 * 6) / (10 * consume_rate);
	urgent_high_fifo_us = (fifo_size * 1000 * 8) / (10 * consume_rate);

	//final result
	dp_buf_sodi_low = fifo_size;
	dp_buf_sodi_high = fifo_size + 1;

	dp_buf_preultra_low = fifo_size;
	dp_buf_preultra_high = fifo_size + 1;

	dp_buf_ultra_low = ultra_low_fifo_us * consume_rate / 1000;
	dp_buf_ultra_high = ultra_high_fifo_us * consume_rate / 1000;

	dp_buf_urgent_low = urgent_low_fifo_us * consume_rate / 1000;
	dp_buf_urgent_high = urgent_high_fifo_us * consume_rate / 1000;

	//print
	DDPMSG("consume_rate = %u\n", consume_rate);
	DDPMSG("ultra_low_fifo_us = %u, ultra_high_fifo_us = %u\n", ultra_low_fifo_us, ultra_high_fifo_us);
	DDPMSG("urgent_high_fifo_us = %u, urgent_low_fifo_us = %u\n", urgent_high_fifo_us, urgent_low_fifo_us);
	DDPMSG("dp_buf_sodi_high = %u, dp_buf_sodi_low = %u, dp_buf_preultra_high = %u, dp_buf_preultra_low = %u\n",
			dp_buf_sodi_high, dp_buf_sodi_low, dp_buf_preultra_high, dp_buf_preultra_low);
	DDPMSG("dp_buf_ultra_high = %u, dp_buf_ultra_low = %u, dp_buf_urgent_high = %u, dp_buf_urgent_low = %u\n",
			dp_buf_ultra_high, dp_buf_ultra_low, dp_buf_urgent_high, dp_buf_urgent_low);

	//RG setting
	mtk_ddp_write_relaxed(comp, dp_buf_sodi_low, DP_BUF_SODI_LOW, handle);
	mtk_ddp_write_relaxed(comp, dp_buf_sodi_high, DP_BUF_SODI_HIGH, handle);

	mtk_ddp_write_relaxed(comp, dp_buf_preultra_low, DP_BUF_PREULTRA_LOW, handle);
	mtk_ddp_write_relaxed(comp, dp_buf_preultra_high, DP_BUF_PREULTRA_HIGH, handle);

	mtk_ddp_write_relaxed(comp, dp_buf_ultra_low, DP_BUF_ULTRA_LOW, handle);
	mtk_ddp_write_relaxed(comp, dp_buf_ultra_high, DP_BUF_ULTRA_HIGH, handle);

	mtk_ddp_write_relaxed(comp, dp_buf_urgent_low, DP_BUF_URGENT_LOW, handle);
	mtk_ddp_write_relaxed(comp, dp_buf_urgent_high, DP_BUF_URGENT_HIGH, handle);
}

void mhal_DVO_VideoClock(bool enable, int resolution)
{
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_drm_private *priv;

	mtk_crtc = g_dp_dvo->ddp_comp.mtk_crtc;
	priv = mtk_crtc->base.dev->dev_private;
	DPTXFUNC();
	if (enable) {
		mtk_dvo_video_clock(g_dp_dvo);
	} else {
		clk_set_rate(g_dp_dvo->hf_fdp_ck_src_pll, 594000000);
		clk_disable_unprepare(g_dp_dvo->hf_fdp_ck_src_mux);
	}
}

void mhal_DPTx_ModeCopy(struct drm_display_mode *mode)
{
	drm_mode_copy(&g_dp_dvo->mode, mode);
	DDPMSG("[DPTX] %s Htt=%d Vtt=%d Ha=%d Va=%d\n", __func__, g_dp_dvo->mode.htotal,
		g_dp_dvo->mode.vtotal, g_dp_dvo->mode.hdisplay, g_dp_dvo->mode.vdisplay);
}

static void mtk_dp_dvo_config(struct mtk_ddp_comp *comp,
				 struct mtk_ddp_config *cfg,
				 struct cmdq_pkt *handle)
{
	/*u32 reg_val;*/
	struct mtk_dp_dvo *dp_dvo = comp_to_dp_dvo(comp);
	unsigned int hsize = 0, vsize = 0;
	unsigned int hpw = 0;
	unsigned int hfp = 0, hbp = 0;
	unsigned int vpw = 0;
	unsigned int vfp = 0, vbp = 0;
	unsigned int vtotal = 0;
	unsigned int rw_times = 0;
	unsigned int vrefresh = 0;
	unsigned int vblank_time = 0, prefetch_time = 0, config_time = 0;
	unsigned int pixel_per_tick = 4;
	u32 val = 0, line_time = 0;

	hsize = dp_dvo->mode.hdisplay;
	vsize = dp_dvo->mode.vdisplay;
	hpw = (dp_dvo->mode.hsync_end - dp_dvo->mode.hsync_start) / pixel_per_tick;
	hfp = (dp_dvo->mode.hsync_start - dp_dvo->mode.hdisplay) / pixel_per_tick;
	hbp = (dp_dvo->mode.htotal - dp_dvo->mode.hsync_end) / pixel_per_tick;
	vpw = dp_dvo->mode.vsync_end - dp_dvo->mode.vsync_start;
	vfp = dp_dvo->mode.vsync_start - dp_dvo->mode.vdisplay;
	vbp = dp_dvo->mode.vtotal - dp_dvo->mode.vsync_end;
	vrefresh = drm_mode_vrefresh(&dp_dvo->mode);

	DDPMSG("%s Htt=%d Hact=%d Hblk=%d Hfp=%d Hpw=%d Hbp=%d\n", __func__, hsize+(hpw+hfp+hbp)*pixel_per_tick,
		hsize,(hpw+hfp+hbp)*pixel_per_tick, hfp*pixel_per_tick, hpw*pixel_per_tick, hbp*pixel_per_tick);
	DDPMSG("%s Vtt=%d Vact=%d Vblk=%d Vfp=%d Vpw=%d Vbp=%d\n", __func__, vsize+vpw+vfp+vbp,
		vsize, vpw+vfp+vbp, vfp, vpw, vbp);
	DDPMSG("%s vrefresh=%d, clock=%dkhz\n", __func__, vrefresh, dp_dvo->mode.clock);

	mtk_dvo_video_clock(dp_dvo);

	//video config setting
	mtk_ddp_write_relaxed(comp, (vsize << SRC_VSIZE) | hsize, DVO_SRC_SIZE, handle);
	mtk_ddp_write_relaxed(comp, (vsize << PIC_VSIZE) | hsize, DVO_PIC_SIZE, handle);
	mtk_ddp_write_relaxed(comp, hsize, DVO_OUT_HSIZE, handle);
	if (hsize == 640 && vsize == 480) {
		mtk_ddp_write_relaxed(comp, ((hpw + 296) << HSYNC) | hfp, DVO_TGEN_H0, handle);
		mtk_ddp_write_relaxed(comp, ((hsize / 4) << HACT) | (hbp + hpw + 296), DVO_TGEN_H1, handle);
	} else {
		mtk_ddp_write_relaxed(comp, (hpw << HSYNC) | hfp, DVO_TGEN_H0, handle);
		mtk_ddp_write_relaxed(comp, ((hsize / 4) << HACT) | (hbp + hpw), DVO_TGEN_H1, handle);
	}
	mtk_ddp_write_relaxed(comp, (vpw << VSYNC) | vfp, DVO_TGEN_V0, handle);
	mtk_ddp_write_relaxed(comp, (vsize << VACT) | (vbp + vpw), DVO_TGEN_V1, handle);

/*
 *#ifdef IF_ZERO
 *	mtk_ddp_write_mask(comp, DSC_UFOE_SEL,
 *			DISP_REG_DSC_CON, DSC_UFOE_SEL, handle);
 *	mtk_ddp_write_relaxed(comp,
 *			(slice_group_width - 1) << 16 | slice_width,
 *			DISP_REG_DSC_SLICE_W, handle);
 *	mtk_ddp_write(comp, 0x20000c03, DISP_REG_DSC_PPS6, handle);
 *#endif
 */

	if (hsize & 0x3)
		rw_times = ((hsize >> 2) + 1) * vsize;
	else
		rw_times = (hsize >> 2) * vsize;

	mtk_ddp_write_relaxed(comp, rw_times,
			DVO_BUF_RW_TIMES, handle);
	mtk_ddp_write_mask(comp, DISP_BUF_EN,
			DVO_BUF_CON0, DISP_BUF_EN, handle);
	mtk_ddp_write_mask(comp, FIFO_UNDERFLOW_DONT_BLOCK,
			DVO_BUF_CON0, FIFO_UNDERFLOW_DONT_BLOCK, handle);
	//DVO 1T4P select
	mtk_ddp_write_relaxed(comp, DVO_OUT_1T4P_SEL,
		DVO_OUTPUT_SET, handle);

	//Golden setting need to check
	mtk_dp_dvo_golden_setting(comp, handle);
	val = DISP_BUF_VDE_BLOCK_URGENT | DISP_BUF_NON_VDE_FORCE_PREULTRA | DISP_BUF_VDE_BLOCK_ULTRA;
	mtk_ddp_write_relaxed(comp, val, DVO_BUF_VDE, handle);

	/* fix prefetch time at 133us as DE suggests, *100 for integer calculation,
	 * and also use ceiling function for value (unit: line) written into register
	 */
	vtotal = vfp + vpw + vbp + vsize;
	line_time = (vtotal * vrefresh) > 0 ? 1000000 * 100 / (vtotal * vrefresh) : 1400;
	vblank_time = line_time * (vfp + vpw + vbp);
	prefetch_time = 13300;
	config_time = vblank_time - prefetch_time;
	val = line_time > 0 ? (config_time + line_time - 100) / line_time : vfp;

	dp_dvo->config_time = config_time/100;
	dp_dvo->config_line = val;
	DPTXMSG("line time: %dus, vblank time: %dus, prefetch time: %dus, config time: %dus\n",
		line_time/100, vblank_time/100, prefetch_time/100, config_time/100);
	DPTXMSG("vblank line: %d, mutex_vfp= %d line, prefetch= %d line\n",
		(vfp + vpw + vbp), val, (vfp + vpw + vbp - val));

	val = (val << MUTEX_VFP) | MUTEX_VSYNC_SEL;
	mtk_ddp_write_relaxed(comp, val, DVO_MUTEX_VSYNC_SET, handle);
	DPTXMSG("vsync_time=%x\n",val);

	DPTXMSG("%s config done\n",
			mtk_dump_comp_str(comp));

	dp_dvo->enable = true;
}


int mtk_dp_dvo_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = NULL;

	if (IS_ERR_OR_NULL(comp) || IS_ERR_OR_NULL(comp->regs)) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return 0;
	}

	baddr = comp->regs;

	DDPDUMP("== %s REGS ==\n", mtk_dump_comp_str(comp));
	DDPDUMP("(0x0000) DVO_EN              =0x%x\n",
			readl(baddr + DVO_EN));
	DDPDUMP("(0x0004) DVO_RST             =0x%x\n",
			readl(baddr + DVO_RST));
	DDPDUMP("(0x0008) DVO_INTEN           =0x%x\n",
			readl(baddr + DVO_INTEN));
	DDPDUMP("(0x000C) DVO_INTSTA          =0x%x\n",
			readl(baddr + DVO_INTSTA));
	DDPDUMP("(0x0018) DVO_OUTPUT_SET      =0x%x\n",
			readl(baddr + DVO_OUTPUT_SET));
	DDPDUMP("(0x0020) DVO_SRC_SIZE        =0x%x\n",
			readl(baddr + DVO_SRC_SIZE));
	DDPDUMP("(0x0024) DVO_PIC_SIZE        =0x%x\n",
			readl(baddr + DVO_PIC_SIZE));
	DDPDUMP("(0x0028) DVO_OUT_HSIZE       =0x%x\n",
			readl(baddr + DVO_OUT_HSIZE));
	DDPDUMP("(0x0050) DVO_TGEN_H0         =0x%x\n",
			readl(baddr + DVO_TGEN_H0));
	DDPDUMP("(0x0054) DVO_TGEN_H1         =0x%x\n",
			readl(baddr + DVO_TGEN_H1));
	DDPDUMP("(0x0058) DVO_TGEN_V0         =0x%x\n",
			readl(baddr + DVO_TGEN_V0));
	DDPDUMP("(0x005C) DVO_TGEN_V1         =0x%x\n",
			readl(baddr + DVO_TGEN_V1));
	DDPDUMP("(0x0084) DVO_MUTEX_VSYNC_SET =0x%x\n",
			readl(baddr + DVO_MUTEX_VSYNC_SET));
	DDPDUMP("(0x0220) DVO_BUF_CON0        =0x%x\n",
			readl(baddr + DVO_BUF_CON0));
	DDPDUMP("(0x022C) DVO_BUF_RW_TIMES    =0x%x\n",
			readl(baddr + DVO_BUF_RW_TIMES));
	DDPDUMP("(0x0258) DVO_BUF_VDE         =0x%x\n",
			readl(baddr + DVO_BUF_VDE));
	return 0;
}

 /*
 *int mtk_dp_intf_analysis(struct mtk_ddp_comp *comp)
 *{
 *	void __iomem *baddr = comp->regs;
 *
 *	if (!baddr) {
 *		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
 *		return 0;
 *	}
 *
 *	DDPDUMP("== %s ANALYSIS ==\n", mtk_dump_comp_str(comp));
 *	DDPDUMP("en=%d, rst_sel=%d, rst=%d, bg_en=%d, intl_en=%d\n",
 *	DISP_REG_GET_FIELD(CON_FLD_DVO_EN, baddr + DVO_EN),
 *	DISP_REG_GET_FIELD(CON_FLD_DVO_RET_SEL, baddr + DVO_RET),
 *	DISP_REG_GET_FIELD(CON_FLD_DVO_RET, baddr + DVO_RET),
 *	DISP_REG_GET_FIELD(CON_FLD_DP_BG_EN, baddr + DP_CON),
 *	DISP_REG_GET_FIELD(CON_FLD_DP_INTL_EN, baddr + DP_CON));
 *	DDPDUMP("== End %s ANALYSIS ==\n", mtk_dump_comp_str(comp));
 *
 *	return 0;
 *}
 */

unsigned long long mtk_dpdvo_get_frame_hrt_bw_base(
		struct mtk_drm_crtc *mtk_crtc, struct mtk_dp_dvo *dp_dvo)
{
	unsigned long long base_bw;
	unsigned int vtotal, htotal;
	int vrefresh;
	u32 bpp = 4;

	/* for the case dpdvo not initialize yet, return 1 avoid treat as error */
	if (!(mtk_crtc && mtk_crtc->base.state))
		return 1;

	htotal = mtk_crtc->base.state->adjusted_mode.htotal;
	vtotal = mtk_crtc->base.state->adjusted_mode.vtotal;
	vrefresh = drm_mode_vrefresh(&mtk_crtc->base.state->adjusted_mode);
	base_bw = div_u64((unsigned long long)vtotal * htotal * vrefresh * bpp, 1000000);

	if (dp_dvo_bw != base_bw) {
		dp_dvo_bw = base_bw;
		DPTXMSG("%s Frame Bw:%llu, htotal:%d, vtotal:%d, vrefresh:%d\n",
			__func__, base_bw, htotal, vtotal, vrefresh);
	}

	return base_bw;
}

static unsigned long long mtk_dpdvo_get_frame_hrt_bw_base_by_mode(
		struct mtk_drm_crtc *mtk_crtc, struct mtk_dp_dvo *dp_dvo)
{
	unsigned long long base_bw;
	unsigned int vtotal, htotal;
	unsigned int vrefresh;
	u32 bpp = 4;

	/* for the case dpdvo not initialize yet, return 1 avoid treat as error */
	if (!(mtk_crtc && mtk_crtc->avail_modes))
		return 1;

	htotal = mtk_crtc->avail_modes->htotal ;
	vtotal = mtk_crtc->avail_modes->vtotal;
	vrefresh = drm_mode_vrefresh(mtk_crtc->avail_modes);
	base_bw = div_u64((unsigned long long)vtotal * htotal * vrefresh * bpp, 1000000);

	if (dp_dvo_bw != base_bw) {
		dp_dvo_bw = base_bw;
		DPTXMSG("%s Frame Bw:%llu, htotal:%d, vtotal:%d, vrefresh:%d\n",
			__func__, base_bw, htotal, vtotal, vrefresh);
	}

	return base_bw;
}

static int mtk_dpdvo_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			  enum mtk_ddp_io_cmd cmd, void *params)
{
	struct mtk_dp_dvo *dp_dvo = comp_to_dp_dvo(comp);

	switch (cmd) {
	case GET_FRAME_HRT_BW_BY_DATARATE:
	{
		struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
		unsigned long long *base_bw =
			(unsigned long long *)params;

		*base_bw = mtk_dpdvo_get_frame_hrt_bw_base(mtk_crtc, dp_dvo);
	}
		break;
	case GET_FRAME_HRT_BW_BY_MODE:
	{
		struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
		unsigned long long *base_bw =
			(unsigned long long *)params;

		*base_bw = mtk_dpdvo_get_frame_hrt_bw_base_by_mode(mtk_crtc, dp_dvo);
	}
		break;
	case DVO_GET_MUTEX_VSYNC_CONFIG_TIME:
	{
		int *config_time = (int *)params;
		*config_time = dp_dvo->config_time;
	}
		break;
	case DVO_GET_MUTEX_VSYNC_CONFIG_LINE:
	{
		int *config_line = (int *)params;
		*config_line = dp_dvo->config_line;
	}
		break;
	default:
		break;
	}

	return 0;
}

static const struct mtk_ddp_comp_funcs mtk_dp_dvo_funcs = {
	.config = mtk_dp_dvo_config,
	.start = mtk_dp_dvo_start,
	.stop = mtk_dp_dvo_stop,
	.prepare = mtk_dp_dvo_prepare,
	.unprepare = mtk_dp_dvo_unprepare,
	.io_cmd = mtk_dpdvo_io_cmd,
};

static int mtk_dvo_bind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_dp_dvo *dp_dvo = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	DPTXMSG("%s+\n", __func__);
	dp_dvo->priv = drm_dev->dev_private;
	ret = mtk_ddp_comp_register(drm_dev, &dp_dvo->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	DPTXMSG("%s-\n", __func__);
	return 0;
}

static void mtk_dvo_unbind(struct device *dev, struct device *master,
				 void *data)
{
	struct mtk_dp_dvo *dp_dvo = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_dp_dvo_destroy_conn_enc(dp_dvo);
	mtk_ddp_comp_unregister(drm_dev, &dp_dvo->ddp_comp);
}

static const struct component_ops mtk_dvo_component_ops = {
	.bind = mtk_dvo_bind,
	.unbind = mtk_dvo_unbind,
};

static int mtk_dvo_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_dp_dvo *dp_dvo;
	enum mtk_ddp_comp_id comp_id;
	const struct of_device_id *of_id;
	struct resource *mem;
	int ret;

	DPTXMSG("%s+\n", __func__);
	dp_dvo = devm_kzalloc(dev, sizeof(*dp_dvo), GFP_KERNEL);
	if (!dp_dvo)
		return -ENOMEM;
	dp_dvo->dev = dev;

	of_id = of_match_device(mtk_dvo_driver_dt_match, &pdev->dev);
	if (!of_id) {
		dev_err(dev, "DP_dvo device match failed\n");
		return -ENODEV;
	}
	dp_dvo->driver_data = (struct mtk_dp_dvo_driver_data *)of_id->data;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dp_dvo->regs = devm_ioremap_resource(dev, mem);
	if (IS_ERR(dp_dvo->regs)) {
		ret = PTR_ERR(dp_dvo->regs);
		dev_err(dev, "Failed to ioremap mem resource: %d\n", ret);
		return ret;
	}

	/* Get dp dvo clk
	 * Input pixel clock(hf_fmm_ck) frequency needs to be > hf_fdp_ck * 4
	 * Otherwise FIFO will underflow
	 */
	dp_dvo->hf_fmm_ck = devm_clk_get(dev, "hf_fmm_ck");
	if (IS_ERR(dp_dvo->hf_fmm_ck)) {
		ret = PTR_ERR(dp_dvo->hf_fmm_ck);
		dev_err(dev, "Failed to get hf_fmm_ck clock: %d\n", ret);
		return ret;
	}
	dp_dvo->hf_fdp_ck = devm_clk_get(dev, "hf_fdp_ck");
	if (IS_ERR(dp_dvo->hf_fdp_ck)) {
		ret = PTR_ERR(dp_dvo->hf_fdp_ck);
		dev_err(dev, "Failed to get hf_fdp_ck clock: %d\n", ret);
		return ret;
	}
	dp_dvo->hf_fdp_ck_src_mux = devm_clk_get(dev, "MUX_DVO");
	if (IS_ERR(dp_dvo->hf_fdp_ck_src_mux)) {
		ret = PTR_ERR(dp_dvo->hf_fdp_ck_src_mux);
		dev_err(dev, "Failed to get mux: %d\n", ret);
		return ret;
	}
	dp_dvo->hf_fdp_ck_src_pll = devm_clk_get(dev, "TVDPLL_PLL");
	if (IS_ERR(dp_dvo->hf_fdp_ck_src_pll)) {
		ret = PTR_ERR(dp_dvo->hf_fdp_ck_src_pll);
		dev_err(dev, "Failed to pll clk: %d\n", ret);
		return ret;
	}

	// clk source include
	dp_dvo->driver_data->get_pll_clk(dp_dvo);

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_DVO);
	DPTXMSG("comp_id = %d\n",comp_id);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &dp_dvo->ddp_comp, comp_id,
				&mtk_dp_dvo_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	/* Get dp dvo irq num and request irq */
	dp_dvo->irq = platform_get_irq(pdev, 0);
	if (dp_dvo->irq <= 0) {
		dev_err(dev, "Failed to get irq: %d\n", dp_dvo->irq);
		return -EINVAL;
	}

	irq_set_status_flags(dp_dvo->irq, IRQ_TYPE_LEVEL_HIGH);
	ret = devm_request_irq(
		&pdev->dev, dp_dvo->irq, dp_dvo->driver_data->irq_handler,
		IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(&pdev->dev), dp_dvo);
	if (ret) {
		dev_err(&pdev->dev, "failed to request mediatek dp dvo irq\n");
		ret = -EPROBE_DEFER;
		return ret;
	}

	platform_set_drvdata(pdev, dp_dvo);
	pm_runtime_enable(dev);

	ret = component_add(dev, &mtk_dvo_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		pm_runtime_disable(dev);
	}

	g_dp_dvo = dp_dvo;
	DPTXMSG("%s-\n", __func__);
	return ret;
}

static void mtk_dvo_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mtk_dvo_component_ops);

	pm_runtime_disable(&pdev->dev);
}

static s32 mtk_dp_dvo_poll_for_idle(struct mtk_dp_dvo *dp_dvo,
	struct cmdq_pkt *handle)
{
	return 0;
}

static void mt6993_dvo_pll_mux_config(struct mtk_dp_dvo *dp_dvo, unsigned int *clksrc, unsigned int *pll_rate)
{
	// pll_min = 125M
	// pll_min / 8(d8) * 4(1T4P) = 62.5M(pixel clk min d8 support)
	// pll_min / 4(d4) * 4(1T4P) =  125M(pixel clk min d4 support)
	// pll_min / 2(d2) * 4(1T4P) =  250M(pixel clk min d2 support)
	// spec min pixel clk (640*480) = 800 * 525 * 60 = 25200000

	if (dp_dvo->mode.clock * 1000 > 250000000) {
		*clksrc = mt6993_TVDPLL_D2;
		*pll_rate = dp_dvo->mode.clock * 1000 / 4 * 2;
		DPTXMSG("Set TVDPLL_D2");
	} else if (dp_dvo->mode.clock * 1000 > 125000000 && dp_dvo->mode.clock * 1000 <= 250000000) {
		*clksrc = mt6993_TVDPLL_D4;
		*pll_rate = dp_dvo->mode.clock * 1000 / 4 * 4;
		DPTXMSG("Set TVDPLL_D4");
	} else if (dp_dvo->mode.clock * 1000 > 62500000 && dp_dvo->mode.clock * 1000 <= 125000000) {
		*clksrc = mt6993_TVDPLL_D8;
		*pll_rate = dp_dvo->mode.clock * 1000 / 4 * 8;
		DPTXMSG("Set TVDPLL_D8");
	} else if (dp_dvo->mode.clock * 1000 >= 25200000 && dp_dvo->mode.clock * 1000 <= 62500000
			|| (dp_dvo->mode.hdisplay == 640 && dp_dvo->mode.vdisplay == 480)) {
		*clksrc = mt6993_TVDPLL_D8;
		*pll_rate = 125000000;
		DPTXMSG("Set TVDPLL_D8");
	} else {
		// default setting
		DPTXERR("The pixel clock requirement is too low and not supported, using default value");
		*clksrc = mt6993_TVDPLL_D2;
		*pll_rate = 594000000;
		DPTXMSG("Set TVDPLL_D2");
	}
}


static void mt6993_dvo_get_pll_clk(struct mtk_dp_dvo *dp_dvo)
{
	dp_dvo->hf_fdp_ck_src[mt6993_TCK_26M] = devm_clk_get(dp_dvo->dev, "TCK_26M");
	dp_dvo->hf_fdp_ck_src[mt6993_TVDPLL_D2] = devm_clk_get(dp_dvo->dev, "TVDPLL_D2");
	dp_dvo->hf_fdp_ck_src[mt6993_TVDPLL_D3] = devm_clk_get(dp_dvo->dev, "TVDPLL_D3");
	dp_dvo->hf_fdp_ck_src[mt6993_TVDPLL_D4] = devm_clk_get(dp_dvo->dev, "TVDPLL_D4");
	dp_dvo->hf_fdp_ck_src[mt6993_TVDPLL_D8] = devm_clk_get(dp_dvo->dev, "TVDPLL_D8");

	if (IS_ERR(dp_dvo->hf_fdp_ck_src[mt6993_TCK_26M])
		|| IS_ERR(dp_dvo->hf_fdp_ck_src[mt6993_TVDPLL_D2])
		|| IS_ERR(dp_dvo->hf_fdp_ck_src[mt6993_TVDPLL_D3])
		|| IS_ERR(dp_dvo->hf_fdp_ck_src[mt6993_TVDPLL_D4])
		|| IS_ERR(dp_dvo->hf_fdp_ck_src[mt6993_TVDPLL_D8]))
		DPTXERR("Failed to get pclk andr src clock, -%d-%d-%d-%d-%d-\n",
			IS_ERR(dp_dvo->hf_fdp_ck_src[mt6993_TCK_26M]),
			IS_ERR(dp_dvo->hf_fdp_ck_src[mt6993_TVDPLL_D2]),
			IS_ERR(dp_dvo->hf_fdp_ck_src[mt6993_TVDPLL_D3]),
			IS_ERR(dp_dvo->hf_fdp_ck_src[mt6993_TVDPLL_D4]),
			IS_ERR(dp_dvo->hf_fdp_ck_src[mt6993_TVDPLL_D8]));
}

static irqreturn_t mtk_dp_dvo_irq_status(int irq, void *dev_id)
{
	struct mtk_dp_dvo *dp_dvo = dev_id;
	u32 status = 0;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_drm_private *priv = NULL;
	struct mtk_ddp_comp *comp = NULL;
	int dpdvo_opt = 0;
	irqreturn_t ret = IRQ_NONE;
	int irq_mmp_data = 0;

	mtk_crtc = dp_dvo->ddp_comp.mtk_crtc;
	priv = mtk_crtc->base.dev->dev_private;
	dpdvo_opt = mtk_drm_helper_get_opt(priv->helper_opt,
		MTK_DRM_OPT_DPINTF_UNDERFLOW_AEE);

	comp = &dp_dvo->ddp_comp;
	if (mtk_drm_top_clk_isr_get(comp) == false) {
		DPTXMSG("%s, top clk off\n", __func__);
		return IRQ_NONE;
	}

	mtk_crtc = dp_dvo->ddp_comp.mtk_crtc;
	if (!mtk_crtc) {
		DPTXMSG("%s mtk_crtc is NULL\n", __func__);
		goto out;
	}
	status = readl(dp_dvo->regs + DVO_INTSTA);
	if (!status)
		goto out;

	status &= 0xfffff;
	if (status) {
		mtk_dp_dvo_mask(dp_dvo, DVO_INTSTA, status, 0);
		if (status & INT_VSYNC_START_STA) {
			mtk_crtc_vblank_irq(&mtk_crtc->base);
			irq_intsa++;
			// TODO: After porting all timing functionality, dp_dvo->res will not
			// be used anymore. However, since the 6991 project has not ported
			// all timing yet, we will temporarily set dp_dvo->res to 0.
			dp_dvo->res = 0;
			if (irq_intsa == 3)
				mtk_dp_video_trigger(video_unmute << 16 | dp_dvo->res);
			irq_mmp_data += 1;
		}

		if (status & INT_UNDERFLOW_STA) {
			DPTXMSG("%s dpdvo_underflow!\n", __func__);
			irq_underflowsa++;
			irq_mmp_data += 2;
		}
	}

	DRM_MMP_MARK(dp_intf0, status, irq_mmp_data);

	if (dpdvo_opt && (status & INT_UNDERFLOW_STA) && (irq_underflowsa == 1)) {
#if IS_ENABLED(CONFIG_ARM64)
		DDPAEE("DVO underflow 0x%x. TS: 0x%08llx\n",
			status, arch_timer_read_counter());
#else
		DDPAEE("DVO  0x%x\n",
			status);
#endif
		mtk_drm_crtc_analysis(&(mtk_crtc->base));
		mtk_drm_crtc_dump(&(mtk_crtc->base));
	}

	ret = IRQ_HANDLED;
out:
	mtk_drm_top_clk_isr_put(comp);
	return ret;
}

static const struct mtk_dp_dvo_driver_data dp_dvo_driver_data = {
	.poll_for_idle = mtk_dp_dvo_poll_for_idle,
	.irq_handler = mtk_dp_dvo_irq_status,
	.get_pll_clk = mt6993_dvo_get_pll_clk,
	.pll_mux_config = mt6993_dvo_pll_mux_config,
};

static const struct of_device_id mtk_dvo_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6993-dvo",
	.data = &dp_dvo_driver_data},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_dvo_driver_dt_match);

struct platform_driver mtk_dvo_driver = {
	.probe = mtk_dvo_probe,
	.remove = mtk_dvo_remove,
	.driver = {
		.name = "mediatek-dvo",
		.owner = THIS_MODULE,
		.of_match_table = mtk_dvo_driver_dt_match,
	},
};
