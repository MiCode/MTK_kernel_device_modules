// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019-2022 MediaTek Inc.
 * Copyright (c) 2022 BayLibre
 */

#include <drm/display/drm_dp_aux_bus.h>
#include <drm/display/drm_dp.h>
#include <drm/display/drm_dp_helper.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_crtc.h>
#include <drm/drm_edid.h>
#include <drm/drm_of.h>
#include <drm/drm_panel.h>
#include <drm/drm_print.h>
#include <drm/drm_probe_helper.h>
#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/media-bus-format.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <sound/hdmi-codec.h>
#include <video/videomode.h>

#include "mtk_drm_edp_regs.h"
#include "../mtk_drm_crtc.h"

#define EDP_VIDEO_UNMUTE		0x22
#define MTK_SIP_DP_CONTROL \
	(0x82000523 | 0x40000000)
#define MTK_DP_SIP_CONTROL_AARCH32	MTK_SIP_SMC_CMD(0x523)
#define MTK_DP_SIP_ATF_EDP_VIDEO_UNMUTE	(BIT(0) | BIT(5))
#define MTK_DP_SIP_ATF_VIDEO_UNMUTE	BIT(5)

#define MTK_DP_THREAD_CABLE_STATE_CHG	BIT(0)
#define MTK_DP_THREAD_HPD_EVENT		BIT(1)

#define MTK_DP_4P1T 4
#define MTK_DP_HDE 2
#define MTK_DP_PIX_PER_ADDR 2
#define MTK_DP_AUX_WAIT_REPLY_COUNT 20
#define MTK_DP_TBC_BUF_READ_START_ADDR 0x8
#define MTK_DP_TRAIN_VOLTAGE_LEVEL_RETRY 5
#define MTK_DP_TRAIN_DOWNSCALE_RETRY 10
#define MTK_DP_VERSION 0x11
#define MTK_DP_SDP_AUI 0x4

#define MTK_EDP_MODE_EXTERNAL_MONITOR	"external-monitor"
#define MTK_EDP_MODE_USE_EDID			"use-edid"
#define MTK_EDP_MODE_USE_HPD			"use-hpd"

enum {
	MTK_DP_CAL_GLB_BIAS_TRIM = 0,
	MTK_DP_CAL_CLKTX_IMPSE,
	MTK_DP_CAL_LN_TX_IMPSEL_PMOS_0,
	MTK_DP_CAL_LN_TX_IMPSEL_PMOS_1,
	MTK_DP_CAL_LN_TX_IMPSEL_PMOS_2,
	MTK_DP_CAL_LN_TX_IMPSEL_PMOS_3,
	MTK_DP_CAL_LN_TX_IMPSEL_NMOS_0,
	MTK_DP_CAL_LN_TX_IMPSEL_NMOS_1,
	MTK_DP_CAL_LN_TX_IMPSEL_NMOS_2,
	MTK_DP_CAL_LN_TX_IMPSEL_NMOS_3,
	MTK_DP_CAL_MAX,
};

struct mtk_edp_enable {
	bool enable;
};

struct mtk_dp_train_info {
	bool sink_ssc;
	bool cable_plugged_in;
	/* link_rate is in multiple of 0.27Gbps */
	int link_rate;
	int lane_count;
	unsigned int channel_eq_pattern;
};

struct mtk_dp_audio_cfg {
	bool detect_monitor;
	int sad_count;
	int sample_rate;
	int word_length_bits;
	int channels;
};

struct mtk_dp_info {
	enum dp_pixelformat format;
	struct videomode vm;
	struct mtk_dp_audio_cfg audio_cur_cfg;
};

struct mtk_edp_efuse_fmt {
	unsigned short idx;
	unsigned short shift;
	unsigned short mask;
	unsigned short min_val;
	unsigned short max_val;
	unsigned short default_val;
};

struct mtk_edp {
	bool enabled;
	bool need_debounce;
	int irq;
	u8 max_lanes;
	u8 max_linkrate;
	u8 rx_cap[DP_RECEIVER_CAP_SIZE];
	u32 cal_data[MTK_DP_CAL_MAX];
	u32 irq_thread_handle;
	/* irq_thread_lock is used to protect irq_thread_handle */
	spinlock_t irq_thread_lock;

	struct device *dev;
	struct drm_bridge bridge;
	struct drm_bridge *next_bridge;
	struct drm_connector *conn;
	struct drm_device *drm_dev;
	struct drm_dp_aux aux;

	const struct mtk_edp_data *data;
	struct mtk_dp_info info;
	struct mtk_dp_train_info train_info;

	struct platform_device *phy_dev;
	struct phy *phy;
	struct regmap *regs;
	struct timer_list debounce_timer;

	/* For eDP attribute */
	unsigned int color_depth;
	bool use_hpd;
	bool use_edid;
	bool external_monitor;
	bool edp_ui_enable;
	bool has_fec;

	/* For audio */
	bool audio_enable;
	hdmi_codec_plugged_cb plugged_cb;
	struct platform_device *audio_pdev;

	struct device *codec_dev;
	/* protect the plugged_cb as it's used in both bridge ops and audio */
	struct mutex update_plugged_status_lock;
};

struct mtk_edp_data {
	int bridge_type;
	unsigned int smc_cmd;
	const struct mtk_edp_efuse_fmt *efuse_fmt;
	bool audio_supported;
	bool audio_pkt_in_hblank_area;
	u16 audio_m_div2_bit;
};

static const struct mtk_edp_efuse_fmt mt8678_edp_efuse_fmt[MTK_DP_CAL_MAX] = {
	[MTK_DP_CAL_GLB_BIAS_TRIM] = {
		.idx = 3,
		.shift = 27,
		.mask = 0x1f,
		.min_val = 1,
		.max_val = 0x1e,
		.default_val = 0xf,
	},
	[MTK_DP_CAL_CLKTX_IMPSE] = {
		.idx = 0,
		.shift = 9,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_PMOS_0] = {
		.idx = 2,
		.shift = 28,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_PMOS_1] = {
		.idx = 2,
		.shift = 20,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_PMOS_2] = {
		.idx = 2,
		.shift = 12,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_PMOS_3] = {
		.idx = 2,
		.shift = 4,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_NMOS_0] = {
		.idx = 2,
		.shift = 24,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_NMOS_1] = {
		.idx = 2,
		.shift = 16,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_NMOS_2] = {
		.idx = 2,
		.shift = 8,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
	[MTK_DP_CAL_LN_TX_IMPSEL_NMOS_3] = {
		.idx = 2,
		.shift = 0,
		.mask = 0xf,
		.min_val = 1,
		.max_val = 0xe,
		.default_val = 0x8,
	},
};

static struct regmap_config mtk_edp_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = SEC_OFFSET + 0x90,
	.name = "mtk-edp-registers",
};

enum DPTX_STATE {
	DPTX_STATE_NO_DEVICE,
	DPTX_STATE_ACTIVE,
};

struct notify_dev {
	const char *name;
	struct device *dev;
	int index;
	int state;
	int crtc;

	ssize_t (*print_name)(struct notify_dev *sdev, char *buf);
	ssize_t (*print_state)(struct notify_dev *sdev, char *buf);
	ssize_t (*print_crtc)(struct notify_dev *sdev, char *buf);
};


/* staitc global variable define */
static struct mtk_edp *g_mtk_edp;
struct notify_dev edptx_notify_data;
struct class *switch_edp_class;
static atomic_t device_count;

int edp_notify_uevent_user(struct notify_dev *sdev, int state)
{
	char *envp[4];
	char name_buf[120];
	char state_buf[120];
	char crtc_buf[16];
	struct mtk_edp *mtk_edp = g_mtk_edp;

	if (sdev == NULL)
		return -1;

	if (sdev->state != state)
		sdev->state = state;

	snprintf(name_buf, sizeof(name_buf), "SWITCH_NAME=%s", sdev->name);
	envp[0] = name_buf;
	snprintf(state_buf, sizeof(state_buf), "SWITCH_STATE=%d", sdev->state);
	envp[1] = state_buf;
	snprintf(crtc_buf, sizeof(crtc_buf), "CRTC=%d", sdev->crtc);
	envp[2] = crtc_buf;
	envp[3] = NULL;
	dev_info(mtk_edp->dev, "[eDPTX] uevent name:%s ,state:%s, dev:%s\n",
		envp[0], envp[1], envp[2]);

	kobject_uevent_env(&sdev->dev->kobj, KOBJ_CHANGE, envp);

	return 0;
}

static struct mtk_edp *mtk_edp_from_bridge(struct drm_bridge *b)
{
	return container_of(b, struct mtk_edp, bridge);
}

static u32 mtk_edp_read(struct mtk_edp *mtk_edp, u32 offset)
{
	u32 read_val;
	int ret;

	ret = regmap_read(mtk_edp->regs, offset, &read_val);
	if (ret) {
		dev_info(mtk_edp->dev, "Failed to read register 0x%x: %d\n",
			offset, ret);
		return 0;
	}

	return read_val;
}

static int mtk_edp_write(struct mtk_edp *mtk_edp, u32 offset, u32 val)
{
	int ret = regmap_write(mtk_edp->regs, offset, val);

	if (ret)
		dev_info(mtk_edp->dev,
			"Failed to write register 0x%8x with value 0x%x\n",
			offset, val);
	return ret;
}

static int mtk_edp_update_bits(struct mtk_edp *mtk_edp, u32 offset,
			      u32 val, u32 mask)
{
	int ret = regmap_update_bits(mtk_edp->regs, offset, mask, val);

	if (ret)
		dev_info(mtk_edp->dev,
			"Failed to update register 0x%04x with value 0x%08x, mask 0x%x\n",
			offset, val, mask);
	return ret;
}

static void mtk_edp_bulk_16bit_write(struct mtk_edp *mtk_edp, u32 offset, u8 *buf,
				    size_t length)
{
	int i;

	/* 2 bytes per register */
	for (i = 0; i < length; i += 2) {
		u32 val = buf[i] | (i + 1 < length ? buf[i + 1] << 8 : 0);

		if (mtk_edp_write(mtk_edp, offset + i * 2, val))
			return;
	}
}

/* reg operate function */
static bool mtk_edp_plug_state(struct mtk_edp *mtk_edp)
{
	return !!(mtk_edp_read(mtk_edp, REG_364C_AUX_TX_P0) &
		  HPD_STATUS_DP_AUX_TX_P0_MASK);
}

static void mtk_edp_hpd_sink_event(struct mtk_edp *mtk_edp)
{
	ssize_t ret;
	u8 sink_count;
	u8 link_status[DP_LINK_STATUS_SIZE] = {};
	u32 sink_count_reg;
	u32 link_status_reg;

	sink_count_reg = DP_SINK_COUNT_ESI;
	link_status_reg = DP_LANE0_1_STATUS;
	ret = drm_dp_dpcd_readb(&mtk_edp->aux, sink_count_reg, &sink_count);
	if (ret < 0) {
		dev_info(mtk_edp->dev,
			 "[eDPTX] Read sink count failed: %ld\n", ret);
		return;
	}

	ret = drm_dp_dpcd_readb(&mtk_edp->aux, DP_SINK_COUNT, &sink_count);
	if (ret < 0) {
		dev_info(mtk_edp->dev,
			 "[eDPTX] Read DP_SINK_COUNT_ESI failed: %ld\n", ret);
		return;
	}

	ret = drm_dp_dpcd_read(&mtk_edp->aux, link_status_reg, link_status,
			       sizeof(link_status));
	if (!ret) {
		drm_info(mtk_edp->drm_dev, "[eDPTX] Read link status failed: %ld\n",
			 ret);
		return;
	}

}


static void mtk_edp_msa_bypass_enable(struct mtk_edp *mtk_edp, bool enable)
{
	u32 mask = HTOTAL_SEL_DP_ENC0_P0 | VTOTAL_SEL_DP_ENC0_P0 |
		   HSTART_SEL_DP_ENC0_P0 | VSTART_SEL_DP_ENC0_P0 |
		   HWIDTH_SEL_DP_ENC0_P0 | VHEIGHT_SEL_DP_ENC0_P0 |
		   HSP_SEL_DP_ENC0_P0 | HSW_SEL_DP_ENC0_P0 |
		   VSP_SEL_DP_ENC0_P0 | VSW_SEL_DP_ENC0_P0;

	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_4P_3030, enable ? 0 : mask, mask);
}

static void mtk_edp_set_msa(struct mtk_edp *mtk_edp)
{
	struct drm_display_mode mode;
	struct videomode *vm = &mtk_edp->info.vm;

	drm_display_mode_from_videomode(vm, &mode);

	/* horizontal */
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_3010,
			   mode.htotal, HTOTAL_SW_DP_ENC0_P0_MASK);
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_3018,
			   vm->hsync_len + vm->hback_porch,
			   HSTART_SW_DP_ENC0_P0_MASK);
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_4P_3028,
			   vm->hsync_len, HSW_SW_DP_ENC0_P0_MASK);
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_4P_3028,
			   vm->hback_porch << 15, 0x8000);

	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_3020,
			   vm->hactive, HWIDTH_SW_DP_ENC0_P0_MASK);

	/* vertical */
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_3014,
			   mode.vtotal, VTOTAL_SW_DP_ENC0_P0_MASK);
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_301C,
			   vm->vsync_len + vm->vback_porch,
			   VSTART_SW_DP_ENC0_P0_MASK);
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_4P_302C,
			   vm->vsync_len, VSW_SW_DP_ENC0_P0_MASK);

	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_3024,
			   vm->vactive, VHEIGHT_SW_DP_ENC0_P0_MASK);

	/* horizontal */
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_3064,
			   vm->hactive, HDE_NUM_LAST_DP_ENC0_P0_MASK);
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_3154,
			   mode.htotal, PGEN_HTOTAL_DP_ENC0_P0_MASK);
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_3158,
			   vm->hfront_porch,
			   PGEN_HSYNC_RISING_DP_ENC0_P0_MASK);

	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_315C,
			   vm->hsync_len,
			   PGEN_HSYNC_PULSE_WIDTH_DP_ENC0_P0_MASK);

	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_3160,
			   vm->hback_porch + vm->hsync_len + vm->hfront_porch,
			   PGEN_HFDE_START_DP_ENC0_P0_MASK);
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_3164,
			   vm->hactive,
			   PGEN_HFDE_ACTIVE_WIDTH_DP_ENC0_P0_MASK);

	/* vertical */
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_3168,
			   mode.vtotal,
			   PGEN_VTOTAL_DP_ENC0_P0_MASK);
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_316C,
			   vm->vfront_porch,
			   PGEN_VSYNC_RISING_DP_ENC0_P0_MASK);
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_3170,
			   vm->vsync_len,
			   PGEN_VSYNC_PULSE_WIDTH_DP_ENC0_P0_MASK);
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_3174,
			   vm->vback_porch + vm->vsync_len +  vm->vfront_porch,
			   PGEN_VFDE_START_DP_ENC0_P0_MASK);
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_3178,
			   vm->vactive,
			   PGEN_VFDE_ACTIVE_WIDTH_DP_ENC0_P0_MASK);
}

static int mtk_edp_set_color_format(struct mtk_edp *mtk_edp,
				   enum dp_pixelformat color_format)
{
	u32 val = 0;
	u32 misc0 = 0;

	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_4P_3034,
			   BIT(3), BIT(3));

	switch (color_format) {
	case DP_PIXELFORMAT_RGB:
		misc0 = 0x0;
		val = PIXEL_ENCODE_FORMAT_DP_ENC0_P0_RGB;
		break;
	case DP_PIXELFORMAT_YUV444:
		misc0 = 0x2;
		val = PIXEL_ENCODE_FORMAT_DP_ENC0_P0_RGB;
		break;
	case DP_PIXELFORMAT_YUV422:
		misc0 = 0x1;
		val = PIXEL_ENCODE_FORMAT_DP_ENC0_P0_YCBCR422;
		break;
	case DP_PIXELFORMAT_YUV420:
		misc0 = 0x3;
		val = PIXEL_ENCODE_FORMAT_DP_ENC0_P0_YCBCR420;
		break;
	default:
		pr_info("[eDPTX] Not supported color format: %d\n", color_format);
		return -EINVAL;
	}

	/* update MISC0 for color format */
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_4P_3034,
			   misc0 << DP_TEST_COLOR_FORMAT_SHIFT,
			   DP_TEST_COLOR_FORMAT_MASK);

	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_4P_303C,
			   val, PIXEL_ENCODE_FORMAT_DP_ENC0_P0_MASK);

	return 0;
}

static void mtk_edp_set_color_depth(struct mtk_edp *mtk_edp)
{
	u32 val = 0;
	u32 misc0 = 0;

	switch (mtk_edp->color_depth) {
	case 6:
		misc0 = DP_MSA_MISC_6_BPC;
		val = VIDEO_COLOR_DEPTH_DP_ENC0_P0_6BIT;
		break;
	case 8:
		misc0 = DP_MSA_MISC_8_BPC;
		val = VIDEO_COLOR_DEPTH_DP_ENC0_P0_8BIT;

		/* set MISC0 BT709 */
		mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_4P_3034,
				MTK_DP_MISC0_BT709, MTK_DP_MISC0_BT_MASK);
		break;
	case 10:
		misc0 = DP_MSA_MISC_10_BPC;
		val = VIDEO_COLOR_DEPTH_DP_ENC0_P0_10BIT;

		/* set MISC0 BT601 */
		mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_4P_3034,
				MTK_DP_MISC0_BT601, MTK_DP_MISC0_BT_MASK);
		break;
	case 12:
		misc0 = DP_MSA_MISC_12_BPC;
		val = VIDEO_COLOR_DEPTH_DP_ENC0_P0_12BIT;
		break;
	case 16:
		misc0 = DP_MSA_MISC_16_BPC;
		val = VIDEO_COLOR_DEPTH_DP_ENC0_P0_16BIT;
		break;
	default:
		misc0 = DP_MSA_MISC_8_BPC;
		val = VIDEO_COLOR_DEPTH_DP_ENC0_P0_8BIT;

		/* set MISC0 BT709 */
		mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_4P_3034,
				MTK_DP_MISC0_BT709, MTK_DP_MISC0_BT_MASK);
		break;
	}

	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_4P_3034, misc0, DP_TEST_BIT_DEPTH_MASK);
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_4P_303C, val, VIDEO_COLOR_DEPTH_DP_ENC0_P0_MASK);
}

static void mtk_edp_config_mn_mode(struct mtk_edp *mtk_edp)
{
	/* 0: hw mode, 1: sw mode */
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_3004,
			   0, VIDEO_M_CODE_SEL_DP_ENC0_P0_MASK);
}

static void mtk_edp_set_sram_read_start(struct mtk_edp *mtk_edp, u32 val)
{
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_4P_303C,
			   val, SRAM_START_READ_THRD_DP_ENC0_P0_MASK);
}

static void mtk_edp_setup_encoder(struct mtk_edp *mtk_edp)
{
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_4P_303C,
			   VIDEO_MN_GEN_EN_DP_ENC0_P0,
			   VIDEO_MN_GEN_EN_DP_ENC0_P0);
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_3040,
			   SDP_DOWN_CNT_DP_ENC0_P0_VAL,
			   SDP_DOWN_CNT_INIT_DP_ENC0_P0_MASK);
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC1_P0_3364,
			   SDP_DOWN_CNT_IN_HBLANK_DP_ENC1_P0_VAL,
			   SDP_DOWN_CNT_INIT_IN_HBLANK_DP_ENC1_P0_MASK);
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC1_P0_3300,
			   VIDEO_AFIFO_RDY_SEL_DP_ENC1_P0_VAL << 8,
			   VIDEO_AFIFO_RDY_SEL_DP_ENC1_P0_MASK);
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC1_P0_3364,
			   FIFO_READ_START_POINT_DP_ENC1_P0_VAL << 12,
			   FIFO_READ_START_POINT_DP_ENC1_P0_MASK);
}

static void mtk_edp_pg_enable(struct mtk_edp *mtk_edp, bool enable)
{
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_3038,
			   enable ? VIDEO_SOURCE_SEL_DP_ENC0_P0_MASK : 0,
			   VIDEO_SOURCE_SEL_DP_ENC0_P0_MASK);
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_31B0,
			   PGEN_PATTERN_SEL_VAL << 4, PGEN_PATTERN_SEL_MASK);
}

static void mtk_dp_audio_setup_channels(struct mtk_edp *mtk_edp,
					struct mtk_dp_audio_cfg *cfg)
{
	u32 channel_enable_bits;

	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC1_P0_3324,
			   AUDIO_SOURCE_MUX_DP_ENC1_P0_DPRX,
			   AUDIO_SOURCE_MUX_DP_ENC1_P0_MASK);

	/* audio channel count change reset */
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC1_P0_33F4,
			   DP_ENC_DUMMY_RW_1, DP_ENC_DUMMY_RW_1);
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC1_P0_3304,
			   AU_PRTY_REGEN_DP_ENC1_P0_MASK |
			   AU_CH_STS_REGEN_DP_ENC1_P0_MASK |
			   AUDIO_SAMPLE_PRSENT_REGEN_DP_ENC1_P0_MASK,
			   AU_PRTY_REGEN_DP_ENC1_P0_MASK |
			   AU_CH_STS_REGEN_DP_ENC1_P0_MASK |
			   AUDIO_SAMPLE_PRSENT_REGEN_DP_ENC1_P0_MASK);

	switch (cfg->channels) {
	case 2:
		channel_enable_bits = AUDIO_2CH_SEL_DP_ENC0_P0_MASK |
				      AUDIO_2CH_EN_DP_ENC0_P0_MASK;
		break;
	case 8:
	default:
		channel_enable_bits = AUDIO_8CH_SEL_DP_ENC0_P0_MASK |
				      AUDIO_8CH_EN_DP_ENC0_P0_MASK;
		break;
	}
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_3088,
			   channel_enable_bits | AU_EN_DP_ENC0_P0,
			   AUDIO_2CH_SEL_DP_ENC0_P0_MASK |
			   AUDIO_2CH_EN_DP_ENC0_P0_MASK |
			   AUDIO_8CH_SEL_DP_ENC0_P0_MASK |
			   AUDIO_8CH_EN_DP_ENC0_P0_MASK |
			   AU_EN_DP_ENC0_P0);

	/* audio channel count change reset */
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC1_P0_33F4, 0, DP_ENC_DUMMY_RW_1);

	/* enable audio reset */
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC1_P0_33F4,
			   DP_ENC_DUMMY_RW_1_AUDIO_RST_EN,
			   DP_ENC_DUMMY_RW_1_AUDIO_RST_EN);
}

static void mtk_dp_audio_channel_status_set(struct mtk_edp *mtk_edp,
					    struct mtk_dp_audio_cfg *cfg)
{
	struct snd_aes_iec958 iec = { 0 };

	switch (cfg->sample_rate) {
	case 32000:
		iec.status[3] = IEC958_AES3_CON_FS_32000;
		break;
	case 44100:
		iec.status[3] = IEC958_AES3_CON_FS_44100;
		break;
	case 48000:
		iec.status[3] = IEC958_AES3_CON_FS_48000;
		break;
	case 88200:
		iec.status[3] = IEC958_AES3_CON_FS_88200;
		break;
	case 96000:
		iec.status[3] = IEC958_AES3_CON_FS_96000;
		break;
	case 192000:
		iec.status[3] = IEC958_AES3_CON_FS_192000;
		break;
	default:
		iec.status[3] = IEC958_AES3_CON_FS_NOTID;
		break;
	}

	switch (cfg->word_length_bits) {
	case 16:
		iec.status[4] = IEC958_AES4_CON_WORDLEN_20_16;
		break;
	case 20:
		iec.status[4] = IEC958_AES4_CON_WORDLEN_20_16 |
				IEC958_AES4_CON_MAX_WORDLEN_24;
		break;
	case 24:
		iec.status[4] = IEC958_AES4_CON_WORDLEN_24_20 |
				IEC958_AES4_CON_MAX_WORDLEN_24;
		break;
	default:
		iec.status[4] = IEC958_AES4_CON_WORDLEN_NOTID;
	}

	/* IEC 60958 consumer channel status bits */
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_308C,
			   0, CH_STATUS_0_DP_ENC0_P0_MASK);
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_3090,
			   iec.status[3] << 8, CH_STATUS_1_DP_ENC0_P0_MASK);
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_3094,
			   iec.status[4], CH_STATUS_2_DP_ENC0_P0_MASK);
}

static void mtk_dp_audio_sdp_asp_set_channels(struct mtk_edp *mtk_edp,
					      int channels)
{
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_312C,
			   (min(8, channels) - 1) << 8,
			   ASP_HB2_DP_ENC0_P0_MASK | ASP_HB3_DP_ENC0_P0_MASK);
}

static void mtk_dp_audio_set_divider(struct mtk_edp *mtk_edp)
{
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_30BC,
			   mtk_edp->data->audio_m_div2_bit,
			   AUDIO_M_CODE_MULT_DIV_SEL_DP_ENC0_P0_MASK);
}

static void mtk_dp_sdp_trigger_aui(struct mtk_edp *mtk_edp)
{
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC1_P0_3280,
			   MTK_DP_SDP_AUI, SDP_PACKET_TYPE_DP_ENC1_P0_MASK);
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC1_P0_3280,
			   SDP_PACKET_W_DP_ENC1_P0, SDP_PACKET_W_DP_ENC1_P0);
}

static void mtk_edp_sdp_set_data(struct mtk_edp *mtk_edp, u8 *data_bytes)
{
	mtk_edp_bulk_16bit_write(mtk_edp, MTK_DP_ENC1_P0_3200,
				data_bytes, 0x10);
}

static void mtk_dp_sdp_set_header_aui(struct mtk_edp *mtk_edp,
				      struct dp_sdp_header *header)
{
	u32 db_addr = MTK_DP_ENC0_P0_30D8 + (MTK_DP_SDP_AUI - 1) * 8;

	mtk_edp_bulk_16bit_write(mtk_edp, db_addr, (u8 *)header, 4);
}

static void mtk_dp_disable_sdp_aui(struct mtk_edp *mtk_edp)
{
	/* Disable periodic send */
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_30A8 & 0xfffc, 0,
			   0xff << ((MTK_DP_ENC0_P0_30A8 & 3) * 8));
}

static void mtk_dp_setup_sdp_aui(struct mtk_edp *mtk_edp,
				 struct dp_sdp *sdp)
{
	u32 shift;

	mtk_edp_sdp_set_data(mtk_edp, sdp->db);
	mtk_dp_sdp_set_header_aui(mtk_edp, &sdp->sdp_header);
	mtk_dp_disable_sdp_aui(mtk_edp);

	shift = (MTK_DP_ENC0_P0_30A8 & 3) * 8;

	mtk_dp_sdp_trigger_aui(mtk_edp);
	/* Enable periodic sending */
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_30A8 & 0xfffc,
			   0x05 << shift, 0xff << shift);
}

static void mtk_edp_aux_irq_clear(struct mtk_edp *mtk_edp)
{
	mtk_edp_write(mtk_edp, MTK_DP_AUX_P0_3640, DP_AUX_P0_3640_VAL);
}

static void mtk_edp_aux_set_cmd(struct mtk_edp *mtk_edp, u8 cmd, u32 addr)
{
	mtk_edp_update_bits(mtk_edp, MTK_DP_AUX_P0_3644,
			   cmd, MCU_REQUEST_COMMAND_AUX_TX_P0_MASK);
	mtk_edp_update_bits(mtk_edp, MTK_DP_AUX_P0_3648,
			   addr, MCU_REQUEST_ADDRESS_LSB_AUX_TX_P0_MASK);
	mtk_edp_update_bits(mtk_edp, MTK_DP_AUX_P0_364C,
			   addr >> 16, MCU_REQUEST_ADDRESS_MSB_AUX_TX_P0_MASK);
}

static void mtk_edp_aux_clear_fifo(struct mtk_edp *mtk_edp)
{
	mtk_edp_update_bits(mtk_edp, MTK_DP_AUX_P0_3650,
			   MCU_ACK_TRAN_COMPLETE_AUX_TX_P0,
			   MCU_ACK_TRAN_COMPLETE_AUX_TX_P0 |
			   PHY_FIFO_RST_AUX_TX_P0_MASK |
			   MCU_REQ_DATA_NUM_AUX_TX_P0_MASK);
}

static void mtk_edp_aux_request_ready(struct mtk_edp *mtk_edp)
{
	mtk_edp_update_bits(mtk_edp, MTK_DP_AUX_P0_3630,
			   AUX_TX_REQUEST_READY_AUX_TX_P0,
			   AUX_TX_REQUEST_READY_AUX_TX_P0);
}

static void mtk_edp_aux_fill_write_fifo(struct mtk_edp *mtk_edp, u8 *buf,
				       size_t length)
{
	mtk_edp_bulk_16bit_write(mtk_edp, MTK_DP_AUX_P0_3708, buf, length);
}

static void mtk_edp_aux_read_rx_fifo(struct mtk_edp *mtk_edp, u8 *buf,
				    size_t length, int read_delay)
{
	int read_pos;

	mtk_edp_update_bits(mtk_edp, MTK_DP_AUX_P0_3620,
			   0, AUX_RD_MODE_AUX_TX_P0_MASK);

	for (read_pos = 0; read_pos < length; read_pos++) {
		mtk_edp_update_bits(mtk_edp, MTK_DP_AUX_P0_3620,
				   AUX_RX_FIFO_READ_PULSE_TX_P0,
				   AUX_RX_FIFO_READ_PULSE_TX_P0);

		/* Hardware needs time to update the data */
		usleep_range(read_delay, read_delay * 2);
		buf[read_pos] = (u8)(mtk_edp_read(mtk_edp, MTK_DP_AUX_P0_3620) &
				     AUX_RX_FIFO_READ_DATA_AUX_TX_P0_MASK);
	}
}

static void mtk_edp_aux_set_length(struct mtk_edp *mtk_edp, size_t length)
{
	if (length > 0) {
		mtk_edp_update_bits(mtk_edp, MTK_DP_AUX_P0_3650,
				   (length - 1) << 12,
				   MCU_REQ_DATA_NUM_AUX_TX_P0_MASK);
		mtk_edp_update_bits(mtk_edp, MTK_DP_AUX_P0_362C,
				   0,
				   AUX_NO_LENGTH_AUX_TX_P0 |
				   AUX_TX_AUXTX_OV_EN_AUX_TX_P0_MASK |
				   AUX_RESERVED_RW_0_AUX_TX_P0_MASK);
	} else {
		mtk_edp_update_bits(mtk_edp, MTK_DP_AUX_P0_362C,
				   AUX_NO_LENGTH_AUX_TX_P0,
				   AUX_NO_LENGTH_AUX_TX_P0 |
				   AUX_TX_AUXTX_OV_EN_AUX_TX_P0_MASK |
				   AUX_RESERVED_RW_0_AUX_TX_P0_MASK);
	}
}

static int mtk_edp_aux_wait_for_completion(struct mtk_edp *mtk_edp, bool is_read)
{
	int wait_reply = MTK_DP_AUX_WAIT_REPLY_COUNT;

	while (--wait_reply) {
		u32 aux_irq_status;

		if (is_read) {
			u32 fifo_status = mtk_edp_read(mtk_edp, MTK_DP_AUX_P0_3618);

			if (fifo_status &
			    (AUX_RX_FIFO_WRITE_POINTER_AUX_TX_P0_MASK |
			     AUX_RX_FIFO_FULL_AUX_TX_P0_MASK)) {
				return 0;
			}
		}

		aux_irq_status = mtk_edp_read(mtk_edp, MTK_DP_AUX_P0_3640);
		if (aux_irq_status & AUX_RX_AUX_RECV_COMPLETE_IRQ_AUX_TX_P0)
			return 0;

		if (aux_irq_status & AUX_400US_TIMEOUT_IRQ_AUX_TX_P0)
			return -ETIMEDOUT;

		/* Give the hardware a chance to reach completion before retrying */
		usleep_range(100, 500);
	}

	return -ETIMEDOUT;
}

static int mtk_edp_aux_do_transfer(struct mtk_edp *mtk_edp, bool is_read, u8 cmd,
				  u32 addr, u8 *buf, size_t length, u8 *reply_cmd)
{
	int ret;

	if (is_read && (length > DP_AUX_MAX_PAYLOAD_BYTES ||
			(cmd == DP_AUX_NATIVE_READ && !length)))
		return -EINVAL;

	if (!is_read)
		mtk_edp_update_bits(mtk_edp, MTK_DP_AUX_P0_3704,
				   AUX_TX_FIFO_NEW_MODE_EN_AUX_TX_P0,
				   AUX_TX_FIFO_NEW_MODE_EN_AUX_TX_P0);

	/* We need to clear fifo and irq before sending commands to the sink device. */
	mtk_edp_aux_clear_fifo(mtk_edp);
	mtk_edp_aux_irq_clear(mtk_edp);

	mtk_edp_aux_set_cmd(mtk_edp, cmd, addr);
	mtk_edp_aux_set_length(mtk_edp, length);

	if (!is_read) {
		if (length)
			mtk_edp_aux_fill_write_fifo(mtk_edp, buf, length);

		mtk_edp_update_bits(mtk_edp, MTK_DP_AUX_P0_3704,
				   AUX_TX_FIFO_WDATA_NEW_MODE_T_AUX_TX_P0_MASK,
				   AUX_TX_FIFO_WDATA_NEW_MODE_T_AUX_TX_P0_MASK);
	}

	mtk_edp_aux_request_ready(mtk_edp);

	/* Wait for feedback from sink device. */
	ret = mtk_edp_aux_wait_for_completion(mtk_edp, is_read);

	*reply_cmd = mtk_edp_read(mtk_edp, MTK_DP_AUX_P0_3624) &
		     AUX_RX_REPLY_COMMAND_AUX_TX_P0_MASK;

	if (ret) {
		u32 phy_status = mtk_edp_read(mtk_edp, MTK_DP_AUX_P0_3628) &
				 AUX_RX_PHY_STATE_AUX_TX_P0_MASK;
		if (phy_status != AUX_RX_PHY_STATE_AUX_TX_P0_RX_IDLE) {
			dev_info(mtk_edp->dev,
				"[eDPTX] AUX Rx Aux hang, need SW reset\n");
			return -EIO;
		}

		return -ETIMEDOUT;
	}

	if (!length) {
		mtk_edp_update_bits(mtk_edp, MTK_DP_AUX_P0_362C,
				   0,
				   AUX_NO_LENGTH_AUX_TX_P0 |
				   AUX_TX_AUXTX_OV_EN_AUX_TX_P0_MASK |
				   AUX_RESERVED_RW_0_AUX_TX_P0_MASK);
	} else if (is_read) {
		int read_delay;

		if (cmd == (DP_AUX_I2C_READ | DP_AUX_I2C_MOT) ||
		    cmd == DP_AUX_I2C_READ)
			read_delay = 500;
		else
			read_delay = 100;
		mtk_edp_aux_read_rx_fifo(mtk_edp, buf, length, read_delay);
	}

	return 0;
}

static int mtk_edp_phy_configure(struct mtk_edp *mtk_edp,
				u32 link_rate, int lane_count,
				bool set_swing_pre, unsigned int *swing,
				unsigned int *pre_emphasis)
{
	int ret;
	union phy_configure_opts phy_opts = {
		.dp = {
			.link_rate = drm_dp_bw_code_to_link_rate(link_rate) / 100,
			.set_rate = 1,
			.lanes = lane_count,
			.set_lanes = 1,
			.ssc = mtk_edp->train_info.sink_ssc,
		}
	};

	if (set_swing_pre) {
		phy_opts.dp.set_rate = 0;
		phy_opts.dp.set_lanes = 0;
		memcpy(phy_opts.dp.voltage, swing, lane_count * sizeof(unsigned int));
		memcpy(phy_opts.dp.pre, pre_emphasis, lane_count * sizeof(unsigned int));
	}

	ret = phy_configure(mtk_edp->phy, &phy_opts);
	if (ret)
		return ret;

	mtk_edp_update_bits(mtk_edp, MTK_DP_TOP_PWR_STATE,
			   DP_PWR_STATE_BANDGAP_TPLL_LANE, DP_PWR_STATE_MASK);

	return 0;
}

static void mtk_edp_set_swing_pre_emphasis(struct mtk_edp *mtk_edp, int lane_count,
					  unsigned int *swing_val, unsigned int *preemphasis)
{

	int lane = 0;

	for (lane = 0; lane < lane_count; lane++) {

		dev_info(mtk_edp->dev,
			"[eDPTX] Link training lane%d: swing_val = 0x%x, pre-emphasis = 0x%x\n",
			lane, swing_val[lane], preemphasis[lane]);
	}

	mtk_edp_phy_configure(mtk_edp, 0, lane_count, TRUE, swing_val, preemphasis);
}

static void mtk_edp_reset_swing_pre_emphasis(struct mtk_edp *mtk_edp)
{
	mtk_edp_update_bits(mtk_edp, MTK_DP_TOP_SWING_EMP,
			   0,
			   DP_TX0_VOLT_SWING_MASK |
			   DP_TX1_VOLT_SWING_MASK |
			   DP_TX2_VOLT_SWING_MASK |
			   DP_TX3_VOLT_SWING_MASK |
			   DP_TX0_PRE_EMPH_MASK |
			   DP_TX1_PRE_EMPH_MASK |
			   DP_TX2_PRE_EMPH_MASK |
			   DP_TX3_PRE_EMPH_MASK);
}

static u32 mtk_edp_swirq_get_clear(struct mtk_edp *mtk_edp)
{
	u32 irq_status = mtk_edp_read(mtk_edp, MTK_DP_TRANS_4P_35D0) &
			 SW_IRQ_FINAL_STATUS_DP_TRANS_4P_MASK;

	if (irq_status) {
		mtk_edp_update_bits(mtk_edp, MTK_DP_TRANS_4P_35C8,
				   irq_status, SW_IRQ_CLR_DP_TRANS_4P_MASK);
		mtk_edp_update_bits(mtk_edp, MTK_DP_TRANS_4P_35C8,
				   0, SW_IRQ_CLR_DP_TRANS_4P_MASK);
	}

	return irq_status;
}

static u32 mtk_edp_hwirq_get_clear(struct mtk_edp *mtk_edp)
{
	u32 irq_status = (mtk_edp_read(mtk_edp, REG_3608_AUX_TX_P0));

	if (irq_status) {
		mtk_edp_update_bits(mtk_edp, REG_3668_AUX_TX_P0,
				   irq_status, irq_status);
		mtk_edp_update_bits(mtk_edp, REG_3668_AUX_TX_P0,
				   0, irq_status);
	}

	return irq_status;
}

static void mtk_edp_hwirq_enable(struct mtk_edp *mtk_edp, bool enable)
{
	if (enable)
		mtk_edp_update_bits(mtk_edp, REG_3660_AUX_TX_P0,
			  0x0, HPD_CONNECT_EVENT | HPD_INTERRUPT_EVENT | HPD_DISCONNECT_EVENT);
	else
		mtk_edp_update_bits(mtk_edp, REG_3660_AUX_TX_P0,
			  0xffff, 0xffff);

}

static void mtk_edp_initialize_settings(struct mtk_edp *mtk_edp)
{
	mtk_edp_update_bits(mtk_edp, MTK_DP_TRANS_4P_3540,
				FEC_CLOCK_EN_MODE_DP_TRANS_4P,
				FEC_CLOCK_EN_MODE_DP_TRANS_4P);
	mtk_edp_update_bits(mtk_edp, MTK_DP_TRANS_4P_342C,
						0x68, 0x68);
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_4P_31EC,
						AUDIO_CH_SRC_SEL_DP_ENC0_4P,
						AUDIO_CH_SRC_SEL_DP_ENC0_4P);
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_4P_304C,
						0, SDP_VSYNC_RISING_MASK_DP_ENC0_4P_MASK);
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_4P_304C,
						BIT(3), SDP_VSYNC_RISING_MASK_DP_ENC0_4P_MASK);

	/* default marker */
	mtk_edp_update_bits(mtk_edp, REG_3F04_DP_ENC_4P_3,
						0, 0xFFFF);
	mtk_edp_update_bits(mtk_edp, REG_3F08_DP_ENC_4P_3,
						BIT(3), BIT(3));
	mtk_edp_update_bits(mtk_edp, REG_3F0C_DP_ENC_4P_3,
						BIT(1), BIT(1));
	mtk_edp_update_bits(mtk_edp, REG_3F10_DP_ENC_4P_3,
						BIT(3), BIT(3));

	mtk_edp_update_bits(mtk_edp, REG_33C0_DP_ENCODER1_4P,
						0, 0xf000);
	mtk_edp_update_bits(mtk_edp, REG_33C0_DP_ENCODER1_4P,
						0x80, 0x80);
	mtk_edp_update_bits(mtk_edp, REG_33C4_DP_ENCODER1_P0,
						BIT(5), 0x60);
	mtk_edp_update_bits(mtk_edp, REG_3F28_DP_ENC_4P_3,
						10 << 2, 0x3C);
	mtk_edp_update_bits(mtk_edp, DP_TX_TOP_RESET_AND_PROBE,
						0x9100FF, 0x9100FF);

	mtk_edp_update_bits(mtk_edp, MTK_DP_TOP_IRQ_MASK,
						ENCODER_IRQ_MSK | TRANS_IRQ_MSK | BIT(3),
						ENCODER_IRQ_MSK | TRANS_IRQ_MSK | BIT(3));
}

static void mtk_edp_initialize_hpd_detect_settings(struct mtk_edp *mtk_edp)
{
	mtk_edp_update_bits(mtk_edp, REG_364C_AUX_TX_P0,
				HPD_INT_THD_FLDMASK_VAL << 4,
				HPD_INT_THD_FLDMASK);
}

static void mtk_edp_initialize_aux_settings(struct mtk_edp *mtk_edp)
{
	/* modify timeout threshold = 1595 [12 : 8] */
	mtk_edp_update_bits(mtk_edp, MTK_DP_AUX_4P_360C, 0x1D0C, 0x1fff);
	mtk_edp_update_bits(mtk_edp, MTK_DP_AUX_4P_3658, 0, BIT(0));

	/* [0]mtk_dp, REG_aux_tx_ov_en */
	mtk_edp_update_bits(mtk_edp, REG_36A0_AUX_TX_4P, 0xFFFC, 0xFFFC);

	/* 26M */
	mtk_edp_update_bits(mtk_edp, MTK_DP_AUX_4P_3634,
				AUX_TX_OVER_SAMPLE_RATE_FOR_26M << 8,
				AUX_TX_OVER_SAMPLE_RATE_AUX_TX_4P_MASK);

	mtk_edp_update_bits(mtk_edp, MTK_DP_AUX_4P_3614,
				AUX_RX_UI_CNT_THR_AUX_FOR_26M,
				AUX_RX_UI_CNT_THR_AUX_TX_P0_MASK);

	/* Modify, 13 for 26M */
	mtk_edp_update_bits(mtk_edp, MTK_DP_AUX_4P_37C8,
				MTK_ATOP_EN_AUX_TX_4P,
				MTK_ATOP_EN_AUX_TX_4P);

	/* disable aux sync_stop detect function */
	mtk_edp_update_bits(mtk_edp, MTK_DP_AUX_4P_3690,
				RX_REPLY_COMPLETE_MODE_AUX_TX_4P,
				RX_REPLY_COMPLETE_MODE_AUX_TX_4P);

	/* Con Thd = 1.5ms+Vx0.1ms */
	mtk_edp_update_bits(mtk_edp, REG_367C_AUX_TX_4P,
				HPD_CONN_THD_AUX_TX_P0_FLDMASK_POS << 8,
				HPD_CONN_THD_AUX_TX_P0_FLDMASK);

	/* DisCon Thd = 1.5ms+Vx0.1ms */
	mtk_edp_update_bits(mtk_edp, REG_37A0_AUX_TX_P0,
				HPD_DISC_THD_AUX_TX_P0_FLDMASK_POS << 4,
				HPD_DISC_THD_AUX_TX_P0_FLDMASK);

	mtk_edp_update_bits(mtk_edp, MTK_DP_AUX_4P_3690,
				RX_REPLY_COMPLETE_MODE_AUX_TX_4P,
				RX_REPLY_COMPLETE_MODE_AUX_TX_4P);

	mtk_edp_update_bits(mtk_edp, REG_3FF8_DP_ENC_4P_3,
				XTAL_FREQ_FOR_PSR_DP_ENC_4P_3_VALUE << 9,
				XTAL_FREQ_FOR_PSR_DP_ENC_4P_3_MASK);

	mtk_edp_update_bits(mtk_edp, REG_366C_AUX_TX_P0,
				XTAL_FREQ_DP_TX_AUX_366C_VALUE << 8,
				XTAL_FREQ_DP_TX_AUX_366C_MASK);

}

static void mtk_edp_initialize_digital_settings(struct mtk_edp *mtk_edp)
{
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_4P_304C,
						0, VBID_VIDEO_MUTE_DP_ENC0_4P_MASK);

	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC1_4P_3368,
						BS2BS_MODE_DP_ENC1_4P_VAL << 12,
						BS2BS_MODE_DP_ENC1_4P_MASK);

	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_4P_3030,
						BIT(11), BIT(11));

	/* dp I-mode enable */
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_3000,
						DP_I_MODE_ENABLE, DP_I_MODE_ENABLE);

	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_4P_3030,
						0x3ff, 0x3ff);

	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_4P_3028,
						0, BIT(15));

	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_4P_302C,
						0, BIT(15));

	/* set color format */
	mtk_edp_set_color_format(mtk_edp, DP_PIXELFORMAT_RGB);

	/* set color depth */
	mtk_edp_set_color_depth(mtk_edp);

	/* reg_bs_symbol_cnt_reset */
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_3000,
						REG_BS_SYMBOL_CNT_RESET,
						REG_BS_SYMBOL_CNT_RESET);

	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_3004,
						BIT(14), BIT(14));

	mtk_edp_update_bits(mtk_edp, REG_3368_DP_ENCODER1_P0,
						0x2, BIT(1) | BIT(2));

	mtk_edp_update_bits(mtk_edp, REG_3368_DP_ENCODER1_P0,
						BIT(15), BIT(15));

	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_4P_304C,
						0, BIT(8));

	/* [5:0]video sram start address */
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_4P_303C,
						0x8, 0x3F);

	/* reg_psr_patgen_avt_en patch */
	mtk_edp_update_bits(mtk_edp, REG_3F80_DP_ENC_4P_3,
						0, PSR_PATGEN_AVT_EN_FLDMASK);

	/* phy D enable */
	mtk_edp_update_bits(mtk_edp, REG_3FF8_DP_ENC_4P_3,
						PHY_STATE_W_1_FLDMASK,
						PHY_STATE_W_1_FLDMASK);

	/* reg_dvo_on_ow_en */
	mtk_edp_update_bits(mtk_edp, REG_3FF8_DP_ENC_4P_3,
						DVO_ON_W_1_FLDMASK,
						DVO_ON_W_1_FLDMASK);

	/* dp tx encoder reset all sw */
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_3004,
						DP_TX_ENCODER_4P_RESET_SW_DP_ENC0_P0,
						DP_TX_ENCODER_4P_RESET_SW_DP_ENC0_P0);

	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_3004,
						0, BIT(9));

	mtk_edp_update_bits(mtk_edp, REG_3FF8_DP_ENC_4P_3,
						0xff, 0xff);

	/* Wait for sw reset to complete */
	usleep_range(1000, 5000);

}

static void mtk_edp_digital_sw_reset(struct mtk_edp *mtk_edp)
{
	mtk_edp_update_bits(mtk_edp, MTK_DP_TRANS_P0_340C,
						DP_TX_TRANSMITTER_4P_RESET_SW_DP_TRANS_P0,
						DP_TX_TRANSMITTER_4P_RESET_SW_DP_TRANS_P0);

	/* Wait for sw reset to complete */
	usleep_range(1000, 5000);

	mtk_edp_update_bits(mtk_edp, MTK_DP_TRANS_P0_340C,
						0, DP_TX_TRANSMITTER_4P_RESET_SW_DP_TRANS_P0);
}

static void mtk_edp_set_lanes(struct mtk_edp *mtk_edp, int lanes)
{
	mtk_edp_update_bits(mtk_edp, MTK_DP_TRANS_P0_35F0,
			   lanes == 0 ? 0 : DP_TRANS_DUMMY_RW_0,
			   DP_TRANS_DUMMY_RW_0_MASK);
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_3000,
			   lanes, LANE_NUM_DP_ENC0_P0_MASK);
	mtk_edp_update_bits(mtk_edp, MTK_DP_TRANS_P0_34A4,
			   lanes << 2, LANE_NUM_DP_TRANS_P0_MASK);
}

static void mtk_edp_get_calibration_data(struct mtk_edp *mtk_edp)
{
	const struct mtk_edp_efuse_fmt *fmt = NULL;
	struct device *dev = mtk_edp->dev;
	struct nvmem_cell *cell = NULL;
	u32 *cal_data = mtk_edp->cal_data;
	u32 *buf = NULL;
	int i = 0;
	size_t len = 0;

	cell = nvmem_cell_get(dev, "dp_calibration_data");
	if (IS_ERR(cell)) {
		dev_info(dev, "Failed to get nvmem cell dp_calibration_data\n");
		goto use_default_val;
	}

	buf = (u32 *)nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (IS_ERR(buf) || ((len / sizeof(u32)) != 4)) {
		dev_info(dev, "Failed to read nvmem_cell_read\n");

		if (!IS_ERR(buf))
			kfree(buf);

		goto use_default_val;
	}

	for (i = 0; i < MTK_DP_CAL_MAX; i++) {
		fmt = &mtk_edp->data->efuse_fmt[i];
		cal_data[i] = (buf[fmt->idx] >> fmt->shift) & fmt->mask;

		if (cal_data[i] < fmt->min_val || cal_data[i] > fmt->max_val) {
			dev_info(mtk_edp->dev, "Invalid efuse data, idx = %d\n", i);
			kfree(buf);
			goto use_default_val;
		}
	}
	kfree(buf);

	return;

use_default_val:
	dev_info(mtk_edp->dev, "[eDPTX] Use default calibration data\n");
	for (i = 0; i < MTK_DP_CAL_MAX; i++)
		cal_data[i] = mtk_edp->data->efuse_fmt[i].default_val;
}

static void mtk_edp_set_idle_pattern(struct mtk_edp *mtk_edp, bool enable)
{
	u32 val = POST_MISC_DATA_LANE0_OV_DP_TRANS_4P_MASK |
		  POST_MISC_DATA_LANE1_OV_DP_TRANS_4P_MASK |
		  POST_MISC_DATA_LANE2_OV_DP_TRANS_4P_MASK |
		  POST_MISC_DATA_LANE3_OV_DP_TRANS_4P_MASK;

	mtk_edp_update_bits(mtk_edp, MTK_DP_TRANS_4P_3580,
			   enable ? val : 0, val);
}

static void mtk_edp_train_set_pattern(struct mtk_edp *mtk_edp, int pattern)
{
	/* TPS1 */
	if (pattern == 1)
		mtk_edp_set_idle_pattern(mtk_edp, false);

	mtk_edp_update_bits(mtk_edp,
			   MTK_DP_TRANS_P0_3400,
			   pattern ? BIT(pattern - 1) << 12 : 0,
			   PATTERN1_EN_DP_TRANS_P0_MASK |
			   PATTERN2_EN_DP_TRANS_P0_MASK |
			   PATTERN3_EN_DP_TRANS_P0_MASK |
			   PATTERN4_EN_DP_TRANS_P0_MASK);
}

static void mtk_edp_set_enhanced_frame_mode(struct mtk_edp *mtk_edp)
{
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_3000,
			   ENHANCED_FRAME_EN_DP_ENC0_P0,
			   ENHANCED_FRAME_EN_DP_ENC0_P0);
}

static void mtk_edp_training_set_scramble(struct mtk_edp *mtk_edp, bool enable)
{
	mtk_edp_update_bits(mtk_edp, MTK_DP_TRANS_P0_3404,
			   enable ? DP_SCR_EN_DP_TRANS_P0_MASK : 0,
			   DP_SCR_EN_DP_TRANS_P0_MASK);
}

static void mtk_edp_video_mute(struct mtk_edp *mtk_edp, bool enable)
{
	struct arm_smccc_res res;
	u32 x3 = (EDP_VIDEO_UNMUTE << 16) | enable;

/*  use secure mute and MTK_DP_ENC0_P0_3000 use default mute value
 *	u32 val = VIDEO_MUTE_SEL_DP_ENC0_P0 |
 *			(enable ? VIDEO_MUTE_SW_DP_ENC0_P0 : 0);
 *
 *	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_3000,
 *			   val,
 *			   VIDEO_MUTE_SEL_DP_ENC0_P0 |
 *		   VIDEO_MUTE_SW_DP_ENC0_P0);
 */

	arm_smccc_smc(MTK_SIP_DP_CONTROL,
		      EDP_VIDEO_UNMUTE, enable,
		      x3, 0xFEFD, 0, 0, 0, &res);

	dev_info(mtk_edp->dev, "[eDPTX] smc cmd: 0x%x, p1: %s, ret: 0x%lx-0x%lx\n",
		EDP_VIDEO_UNMUTE, enable ? "enable" : "disable", res.a0, res.a1);
}

static void mtk_dp_audio_mute(struct mtk_edp *mtk_edp, bool mute)
{
	u32 val[3];

	if (mute) {
		val[0] = VBID_AUDIO_MUTE_FLAG_SW_DP_ENC0_P0 |
			 VBID_AUDIO_MUTE_FLAG_SEL_DP_ENC0_P0;
		val[1] = 0;
		val[2] = 0;
	} else {
		val[0] = 0;
		val[1] = AU_EN_DP_ENC0_P0;
		/* Send one every two frames */
		val[2] = 0x0F;
	}

	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_4P_3030,
			   val[0],
			   VBID_AUDIO_MUTE_FLAG_SW_DP_ENC0_P0 |
			   VBID_AUDIO_MUTE_FLAG_SEL_DP_ENC0_P0);
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_3088,
			   val[1], AU_EN_DP_ENC0_P0);
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_30A4,
			   val[2], AU_TS_CFG_DP_ENC0_P0_MASK);
}

static void mtk_edp_aux_panel_poweron(struct mtk_edp *mtk_edp, bool pwron)
{
	if (pwron) {
		/* power on aux */
		mtk_edp_update_bits(mtk_edp, MTK_DP_TOP_PWR_STATE,
				   DP_PWR_STATE_BANDGAP_TPLL_LANE,
				   DP_PWR_STATE_MASK);

		/* power on panel */
		drm_dp_dpcd_writeb(&mtk_edp->aux, DP_SET_POWER, DP_SET_POWER_D0);
		usleep_range(2000, 5000);
	} else {
		/* power off panel */
		drm_dp_dpcd_writeb(&mtk_edp->aux, DP_SET_POWER, DP_SET_POWER_D3);
		usleep_range(2000, 3000);

		/* power off aux */
		mtk_edp_update_bits(mtk_edp, MTK_DP_TOP_PWR_STATE,
				   DP_PWR_STATE_BANDGAP_TPLL,
				   DP_PWR_STATE_MASK);
	}
}

static void mtk_edp_power_enable(struct mtk_edp *mtk_edp)
{
	pr_info("[eDPTX] %s+\n",__func__);

	mtk_edp_update_bits(mtk_edp, MTK_DP_TOP_RESET_AND_PROBE,
			   0, SW_RST_B_PHYD);

	/* Wait for power enable */
	usleep_range(10, 200);

	mtk_edp_update_bits(mtk_edp, MTK_DP_TOP_RESET_AND_PROBE,
			   SW_RST_B_PHYD, SW_RST_B_PHYD);

	pr_info("[eDPTX] %s-\n",__func__);
}

static void mtk_edp_power_disable(struct mtk_edp *mtk_edp)
{
	mtk_edp_write(mtk_edp, MTK_DP_TOP_PWR_STATE, 0);

	mtk_edp_update_bits(mtk_edp, MTK_DP_0034,
			   DA_CKM_CKTX0_EN_FORCE_EN, DA_CKM_CKTX0_EN_FORCE_EN);

	/* Disable RX */
	mtk_edp_write(mtk_edp, MTK_DP_1040, 0);
	mtk_edp_write(mtk_edp, MTK_DP_TOP_MEM_PD,
		     0x550 | FUSE_SEL | MEM_ISO_EN);
}

static void mtk_edp_initialize_priv_data(struct mtk_edp *mtk_edp)
{
	mtk_edp->train_info.link_rate = mtk_edp->max_linkrate;
	mtk_edp->train_info.lane_count = mtk_edp->max_lanes;
	if (mtk_edp_plug_state(mtk_edp))
		mtk_edp->train_info.cable_plugged_in = true;
	else
		mtk_edp->train_info.cable_plugged_in = false;

	pr_info("[eDPTX] cable_plugged_in = %d\n", mtk_edp->train_info.cable_plugged_in);
	mtk_edp->info.format = DP_PIXELFORMAT_RGB;
	mtk_edp->has_fec = false;
	memset(&mtk_edp->info.vm, 0, sizeof(struct videomode));
	mtk_edp->audio_enable = false;
}

static void mtk_edp_sdp_set_down_cnt_init(struct mtk_edp *mtk_edp,
					 u32 sram_read_start)
{
	u32 sdp_down_cnt_init = 0;
	struct drm_display_mode mode;
	struct videomode *vm = &mtk_edp->info.vm;

	drm_display_mode_from_videomode(vm, &mode);

	if (mode.clock > 0)
		sdp_down_cnt_init = sram_read_start *
				    mtk_edp->train_info.link_rate * 2700 * 8 /
				    (mode.clock * 4);

	switch (mtk_edp->train_info.lane_count) {
	case 1:
		sdp_down_cnt_init = max_t(u32, sdp_down_cnt_init, 0x1A);
		break;
	case 2:
		/* case for LowResolution && High Audio Sample Rate */
		sdp_down_cnt_init = max_t(u32, sdp_down_cnt_init, 0x10);
		sdp_down_cnt_init += mode.vtotal <= 525 ? 4 : 0;
		break;
	case 4:
	default:
		sdp_down_cnt_init = max_t(u32, sdp_down_cnt_init, 6);
		break;
	}

	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC0_P0_3040,
			   sdp_down_cnt_init,
			   SDP_DOWN_CNT_INIT_DP_ENC0_P0_MASK);
}

static void mtk_edp_sdp_set_down_cnt_init_in_hblank(struct mtk_edp *mtk_edp)
{
	int pix_clk_mhz;
	u32 dc_offset;
	u32 spd_down_cnt_init = 0;
	struct drm_display_mode mode;
	struct videomode *vm = &mtk_edp->info.vm;

	drm_display_mode_from_videomode(vm, &mode);

	pix_clk_mhz = mtk_edp->info.format == DP_PIXELFORMAT_YUV420 ?
		      mode.clock / 2000 : mode.clock / 1000;

	switch (mtk_edp->train_info.lane_count) {
	case 1:
		spd_down_cnt_init = 0x20;
		break;
	case 2:
		dc_offset = (mode.vtotal <= 525) ? 0x14 : 0x00;
		spd_down_cnt_init = 0x18 + dc_offset;
		break;
	case 4:
	default:
		dc_offset = (mode.vtotal <= 525) ? 0x08 : 0x00;
		if (pix_clk_mhz > mtk_edp->train_info.link_rate * 27)
			spd_down_cnt_init = 0x8;
		else
			spd_down_cnt_init = 0x10 + dc_offset;
		break;
	}

	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC1_P0_3364, spd_down_cnt_init,
			   SDP_DOWN_CNT_INIT_IN_HBLANK_DP_ENC1_P0_MASK);
}

static void mtk_dp_audio_sample_arrange_disable(struct mtk_edp *mtk_edp)
{
	/* arrange audio packets into the Hblanking and Vblanking area */
	if (!mtk_edp->data->audio_pkt_in_hblank_area)
		return;

	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC1_P0_3374, 0,
			   SDP_ASP_INSERT_IN_HBLANK_DP_ENC1_P0_MASK);
	mtk_edp_update_bits(mtk_edp, MTK_DP_ENC1_P0_3374, 0,
			   SDP_DOWN_ASP_CNT_INIT_DP_ENC1_P0_MASK);
}

static void mtk_edp_setup_tu(struct mtk_edp *mtk_edp)
{
	u32 sram_read_start = min_t(u32, MTK_DP_TBC_BUF_READ_START_ADDR,
				    mtk_edp->info.vm.hactive /
				    mtk_edp->train_info.lane_count /
				    MTK_DP_4P1T / MTK_DP_HDE /
				    MTK_DP_PIX_PER_ADDR);

	mtk_edp_set_sram_read_start(mtk_edp, sram_read_start);
	mtk_edp_setup_encoder(mtk_edp);
	mtk_dp_audio_sample_arrange_disable(mtk_edp);
	mtk_edp_sdp_set_down_cnt_init_in_hblank(mtk_edp);
	mtk_edp_sdp_set_down_cnt_init(mtk_edp, sram_read_start);
}

static void mtk_edp_set_tx_out(struct mtk_edp *mtk_edp)
{
	mtk_edp_setup_tu(mtk_edp);
}

static void mtk_edp_train_update_swing_pre(struct mtk_edp *mtk_edp, int lanes,
					  u8 dpcd_adjust_req[2])
{
	int lane;
	unsigned int swing[4]={};
	unsigned int preemphasis[4]={};

	for (lane = 0; lane < lanes; ++lane) {
		u8 val;
		int index = lane / 2;
		int shift = lane % 2 ? DP_ADJUST_VOLTAGE_SWING_LANE1_SHIFT : 0;

		swing[lane] = (dpcd_adjust_req[index] >> shift) &
			DP_ADJUST_VOLTAGE_SWING_LANE0_MASK;
		preemphasis[lane] = ((dpcd_adjust_req[index] >> shift) &
			       DP_ADJUST_PRE_EMPHASIS_LANE0_MASK) >>
			      DP_ADJUST_PRE_EMPHASIS_LANE0_SHIFT;
		val = swing[lane] << DP_TRAIN_VOLTAGE_SWING_SHIFT |
		      preemphasis[lane] << DP_TRAIN_PRE_EMPHASIS_SHIFT;

		if (swing[lane] == DP_TRAIN_VOLTAGE_SWING_LEVEL_3)
			val |= DP_TRAIN_MAX_SWING_REACHED;
		if (preemphasis[lane] == 3)
			val |= DP_TRAIN_MAX_PRE_EMPHASIS_REACHED;

		drm_dp_dpcd_writeb(&mtk_edp->aux, DP_TRAINING_LANE0_SET + lane,
				   val);
	}

	mtk_edp_set_swing_pre_emphasis(mtk_edp, lanes, swing, preemphasis);

}

static void mtk_edp_pattern(struct mtk_edp *mtk_edp, bool is_tps1)
{
	int pattern;
	unsigned int aux_offset;

	if (is_tps1) {
		pattern = 1;
		aux_offset = DP_LINK_SCRAMBLING_DISABLE | DP_TRAINING_PATTERN_1;
	} else {
		aux_offset = mtk_edp->train_info.channel_eq_pattern;

		switch (mtk_edp->train_info.channel_eq_pattern) {
		case DP_TRAINING_PATTERN_4:
			pattern = 4;
			break;
		case DP_TRAINING_PATTERN_3:
			pattern = 3;
			aux_offset |= DP_LINK_SCRAMBLING_DISABLE;
			break;
		case DP_TRAINING_PATTERN_2:
		default:
			pattern = 2;
			aux_offset |= DP_LINK_SCRAMBLING_DISABLE;
			break;
		}
	}

	mtk_edp_train_set_pattern(mtk_edp, pattern);
	drm_dp_dpcd_writeb(&mtk_edp->aux, DP_TRAINING_PATTERN_SET, aux_offset);
}

static int mtk_edp_train_setting(struct mtk_edp *mtk_edp, u8 target_link_rate,
				u8 target_lane_count)
{
	int ret;

	pr_info("[eDPTX] %s+\n", __func__);
	drm_dp_dpcd_writeb(&mtk_edp->aux, DP_LINK_BW_SET, target_link_rate);
	drm_dp_dpcd_writeb(&mtk_edp->aux, DP_LANE_COUNT_SET,
			   target_lane_count | DP_LANE_COUNT_ENHANCED_FRAME_EN);

	if (mtk_edp->train_info.sink_ssc)
		drm_dp_dpcd_writeb(&mtk_edp->aux, DP_DOWNSPREAD_CTRL,
				   DP_SPREAD_AMP_0_5);

	mtk_edp_set_lanes(mtk_edp, target_lane_count / 2);
	ret = mtk_edp_phy_configure(mtk_edp, target_link_rate, target_lane_count, FALSE,
								NULL, NULL);
	if (ret)
		return ret;

	dev_info(mtk_edp->dev,
		"Link train target_link_rate = 0x%x, target_lane_count = 0x%x\n",
		target_link_rate, target_lane_count);

	pr_info("[eDPTX] %s-\n", __func__);
	return 0;
}

static int mtk_edp_train_cr(struct mtk_edp *mtk_edp, u8 target_lane_count)
{
	u8 lane_adjust[2] = {};
	u8 link_status[DP_LINK_STATUS_SIZE] = {};
	u8 prev_lane_adjust = 0xff;
	int train_retries = 0;
	int voltage_retries = 0;

	mtk_edp_pattern(mtk_edp, true);

	/* In DP spec 1.4, the retry count of CR is defined as 10. */
	do {
		train_retries++;
		if (!mtk_edp->train_info.cable_plugged_in) {
			mtk_edp_train_set_pattern(mtk_edp, 0);
			return -ENODEV;
		}

		drm_dp_dpcd_read(&mtk_edp->aux, DP_ADJUST_REQUEST_LANE0_1,
				 lane_adjust, sizeof(lane_adjust));
		mtk_edp_train_update_swing_pre(mtk_edp, target_lane_count,
					      lane_adjust);

		drm_dp_link_train_clock_recovery_delay(&mtk_edp->aux,
						       mtk_edp->rx_cap);

		/* check link status from sink device */
		drm_dp_dpcd_read_link_status(&mtk_edp->aux, link_status);
		if (drm_dp_clock_recovery_ok(link_status,
					     target_lane_count)) {
			dev_dbg(mtk_edp->dev, "Link train CR pass\n");
			return 0;
		}

		/*
		 * In DP spec 1.4, if current voltage level is the same
		 * with previous voltage level, we need to retry 5 times.
		 */
		if (prev_lane_adjust == link_status[4]) {
			voltage_retries++;
			/*
			 * Condition of CR fail:
			 * 1. Failed to pass CR using the same voltage
			 *    level over five times.
			 * 2. Failed to pass CR when the current voltage
			 *    level is the same with previous voltage
			 *    level and reach max voltage level (3).
			 */
			if (voltage_retries > MTK_DP_TRAIN_VOLTAGE_LEVEL_RETRY ||
			    (prev_lane_adjust & DP_ADJUST_VOLTAGE_SWING_LANE0_MASK) == 3) {
				dev_dbg(mtk_edp->dev, "Link train CR fail\n");
				break;
			}
		} else {
			/*
			 * If the voltage level is changed, we need to
			 * re-calculate this retry count.
			 */
			voltage_retries = 0;
		}
		prev_lane_adjust = link_status[4];
	} while (train_retries < MTK_DP_TRAIN_DOWNSCALE_RETRY);

	/* Failed to train CR, and disable pattern. */
	drm_dp_dpcd_writeb(&mtk_edp->aux, DP_TRAINING_PATTERN_SET,
			   DP_TRAINING_PATTERN_DISABLE);
	mtk_edp_train_set_pattern(mtk_edp, 0);

	return -ETIMEDOUT;
}

static int mtk_edp_train_eq(struct mtk_edp *mtk_edp, u8 target_lane_count)
{
	u8 lane_adjust[2] = {};
	u8 link_status[DP_LINK_STATUS_SIZE] = {};
	int train_retries = 0;

	mtk_edp_pattern(mtk_edp, false);

	do {
		train_retries++;
		if (!mtk_edp->train_info.cable_plugged_in) {
			mtk_edp_train_set_pattern(mtk_edp, 0);
			return -ENODEV;
		}

		drm_dp_dpcd_read(&mtk_edp->aux, DP_ADJUST_REQUEST_LANE0_1,
				 lane_adjust, sizeof(lane_adjust));
		mtk_edp_train_update_swing_pre(mtk_edp, target_lane_count,
					      lane_adjust);

		drm_dp_link_train_channel_eq_delay(&mtk_edp->aux,
						   mtk_edp->rx_cap);

		/* check link status from sink device */
		drm_dp_dpcd_read_link_status(&mtk_edp->aux, link_status);
		if (drm_dp_channel_eq_ok(link_status, target_lane_count)) {
			dev_dbg(mtk_edp->dev, "Link train EQ pass\n");

			/* Training done, and disable pattern. */
			drm_dp_dpcd_writeb(&mtk_edp->aux, DP_TRAINING_PATTERN_SET,
					   DP_TRAINING_PATTERN_DISABLE);
			mtk_edp_train_set_pattern(mtk_edp, 0);
			return 0;
		}
		dev_dbg(mtk_edp->dev, "Link train EQ fail\n");
	} while (train_retries < MTK_DP_TRAIN_DOWNSCALE_RETRY);

	/* Failed to train EQ, and disable pattern. */
	drm_dp_dpcd_writeb(&mtk_edp->aux, DP_TRAINING_PATTERN_SET,
			   DP_TRAINING_PATTERN_DISABLE);
	mtk_edp_train_set_pattern(mtk_edp, 0);

	return -ETIMEDOUT;
}

static void mtk_edp_fec_set_capabilities(struct mtk_edp *mtk_edp)
{
	u8 fec_capabilities = 0x0;
	int ret = 0;

	ret = drm_dp_dpcd_readb(&mtk_edp->aux, DP_FEC_CAPABILITY, &fec_capabilities);
	if (ret < 0)
		pr_info("[eDPTX] read FEC capability failed\n");
	/* force disable fec
	 * mtk_edp->has_fec = !!(fec_capabilities & DP_FEC_CAPABLE);
	 * if (!mtk_edp->has_fec)
	 *      return;
	 * drm_dp_dpcd_writeb(&mtk_edp->aux, DP_FEC_CONFIGURATION,
	 *      DP_FEC_BIT_ERROR_COUNT | DP_FEC_READY);
	 */
	mtk_edp->has_fec = false;
	return ;
}

static int mtk_edp_parse_capabilities(struct mtk_edp *mtk_edp)
{
	u8 val;
	ssize_t ret;

	pr_info("[eDPTX] %s+\n", __func__);
	/*
	 * If we're eDP and capabilities were already parsed we can skip
	 * reading again because eDP panels aren't hotpluggable hence the
	 * caps and training information won't ever change in a boot life
	 */
	if (mtk_edp->bridge.type == DRM_MODE_CONNECTOR_eDP &&
	    mtk_edp->rx_cap[DP_MAX_LINK_RATE] &&
	    mtk_edp->train_info.sink_ssc)
		return 0;

	ret = drm_dp_read_dpcd_caps(&mtk_edp->aux, mtk_edp->rx_cap);
	if (ret < 0)
		return ret;

	if (drm_dp_tps4_supported(mtk_edp->rx_cap))
		mtk_edp->train_info.channel_eq_pattern = DP_TRAINING_PATTERN_4;
	else if (drm_dp_tps3_supported(mtk_edp->rx_cap))
		mtk_edp->train_info.channel_eq_pattern = DP_TRAINING_PATTERN_3;
	else
		mtk_edp->train_info.channel_eq_pattern = DP_TRAINING_PATTERN_2;

	mtk_edp->train_info.sink_ssc = drm_dp_max_downspread(mtk_edp->rx_cap);

	if (mtk_edp->rx_cap[DP_DPCD_REV] >= DP_DPCD_REV_14)
		mtk_edp_fec_set_capabilities(mtk_edp);

	ret = drm_dp_dpcd_readb(&mtk_edp->aux, DP_MSTM_CAP, &val);
	if (ret < 1) {
		drm_err(mtk_edp->drm_dev, "Read mstm cap failed\n");
		return ret == 0 ? -EIO : ret;
	}

	if (val & DP_MST_CAP) {
		/* Clear DP_DEVICE_SERVICE_IRQ_VECTOR_ESI0 */
		ret = drm_dp_dpcd_readb(&mtk_edp->aux,
					DP_DEVICE_SERVICE_IRQ_VECTOR_ESI0,
					&val);
		if (ret < 1) {
			drm_err(mtk_edp->drm_dev, "Read irq vector failed\n");
			return ret == 0 ? -EIO : ret;
		}

		if (val) {
			ret = drm_dp_dpcd_writeb(&mtk_edp->aux,
						 DP_DEVICE_SERVICE_IRQ_VECTOR_ESI0,
						 val);
			if (ret < 0)
				return ret;
		}
	}

	pr_info("[eDPTX] %s-\n",__func__);

	return 0;
}

static void mtk_edp_train_change_mode(struct mtk_edp *mtk_edp)
{
	phy_reset(mtk_edp->phy);
	mtk_edp_reset_swing_pre_emphasis(mtk_edp);
}

static int mtk_edp_training(struct mtk_edp *mtk_edp)
{
	int ret;
	u8 lane_count, link_rate, train_limit, max_link_rate;

	pr_info("[eDPTX] %s+\n", __func__);

	link_rate = min_t(u8, mtk_edp->max_linkrate,
			  mtk_edp->rx_cap[DP_MAX_LINK_RATE]);
	max_link_rate = link_rate;
	lane_count = min_t(u8, mtk_edp->max_lanes,
			   drm_dp_max_lane_count(mtk_edp->rx_cap));

	pr_info("[eDPTX] link rate= 0x%x, lane_count= 0x%x\n",link_rate, lane_count);
	/*
	 * TPS are generated by the hardware pattern generator. From the
	 * hardware setting we need to disable this scramble setting before
	 * use the TPS pattern generator.
	 */
	mtk_edp_training_set_scramble(mtk_edp, false);

	for (train_limit = 6; train_limit > 0; train_limit--) {
		mtk_edp_train_change_mode(mtk_edp);

		ret = mtk_edp_train_setting(mtk_edp, link_rate, lane_count);
		if (ret)
			return ret;

		ret = mtk_edp_train_cr(mtk_edp, lane_count);
		if (ret == -ENODEV) {
			return ret;
		} else if (ret) {
			/* reduce link rate */
			switch (link_rate) {
			case DP_LINK_BW_1_62:
				lane_count = lane_count / 2;
				link_rate = max_link_rate;
				if (lane_count == 0)
					return -EIO;
				break;
			case DP_LINK_BW_2_7:
				link_rate = DP_LINK_BW_1_62;
				break;
			case DP_LINK_BW_5_4:
				link_rate = DP_LINK_BW_2_7;
				break;
			case DP_LINK_BW_8_1:
				link_rate = DP_LINK_BW_5_4;
				break;
			default:
				return -EINVAL;
			}
			continue;
		}
		pr_info("[eDPTX] CR training pass\n");

		ret = mtk_edp_train_eq(mtk_edp, lane_count);
		if (ret == -ENODEV) {
			return ret;
		} else if (ret) {
			/* reduce lane count */
			if (lane_count == 0)
				return -EIO;
			lane_count /= 2;
			continue;
		}
		pr_info("[eDPTX] EQ training pass\n");
		/* if we can run to this, training is done. */
		break;
	}

	if (train_limit == 0)
		return -ETIMEDOUT;

	mtk_edp->train_info.link_rate = link_rate;
	mtk_edp->train_info.lane_count = lane_count;

	/*
	 * After training done, we need to output normal stream instead of TPS,
	 * so we need to enable scramble.
	 */
	mtk_edp_training_set_scramble(mtk_edp, true);
	mtk_edp_set_enhanced_frame_mode(mtk_edp);

	pr_info("[eDPTX] %s-\n", __func__);

	return 0;
}

static void mtk_edp_video_enable(struct mtk_edp *mtk_edp, bool enable)
{
	/* the mute sequence is different between enable and disable */
	if (enable) {
		mtk_edp_msa_bypass_enable(mtk_edp, false);
		//set_video_interlance
		mtk_edp_pg_enable(mtk_edp, false);
		mtk_edp_set_tx_out(mtk_edp);
		mtk_edp_video_mute(mtk_edp, false);
	} else {
		mtk_edp_video_mute(mtk_edp, true);
		mtk_edp_pg_enable(mtk_edp, true);
		mtk_edp_msa_bypass_enable(mtk_edp, true);
	}
}

static void mtk_dp_audio_sdp_setup(struct mtk_edp *mtk_edp,
				   struct mtk_dp_audio_cfg *cfg)
{
	struct dp_sdp sdp;
	struct hdmi_audio_infoframe frame;

	hdmi_audio_infoframe_init(&frame);
	frame.coding_type = HDMI_AUDIO_CODING_TYPE_PCM;
	frame.channels = cfg->channels;
	frame.sample_frequency = cfg->sample_rate;

	switch (cfg->word_length_bits) {
	case 16:
		frame.sample_size = HDMI_AUDIO_SAMPLE_SIZE_16;
		break;
	case 20:
		frame.sample_size = HDMI_AUDIO_SAMPLE_SIZE_20;
		break;
	case 24:
	default:
		frame.sample_size = HDMI_AUDIO_SAMPLE_SIZE_24;
		break;
	}

	hdmi_audio_infoframe_pack_for_dp(&frame, &sdp, MTK_DP_VERSION);

	mtk_dp_audio_sdp_asp_set_channels(mtk_edp, cfg->channels);
	mtk_dp_setup_sdp_aui(mtk_edp, &sdp);
}

static void mtk_dp_audio_setup(struct mtk_edp *mtk_edp,
			       struct mtk_dp_audio_cfg *cfg)
{
	mtk_dp_audio_sdp_setup(mtk_edp, cfg);
	mtk_dp_audio_channel_status_set(mtk_edp, cfg);

	mtk_dp_audio_setup_channels(mtk_edp, cfg);
	mtk_dp_audio_set_divider(mtk_edp);
}

static int mtk_edp_video_config(struct mtk_edp *mtk_edp)
{
	mtk_edp_config_mn_mode(mtk_edp);
	mtk_edp_set_msa(mtk_edp);
	mtk_edp_set_color_depth(mtk_edp);
	return mtk_edp_set_color_format(mtk_edp, mtk_edp->info.format);
}

static void mtk_edp_init_port(struct mtk_edp *mtk_edp)
{
	mtk_edp_set_idle_pattern(mtk_edp, true);
	mtk_edp_initialize_priv_data(mtk_edp);

	mtk_edp_initialize_settings(mtk_edp);
	mtk_edp_initialize_aux_settings(mtk_edp);
	mtk_edp_initialize_digital_settings(mtk_edp);
	mtk_edp_initialize_hpd_detect_settings(mtk_edp);

	mtk_edp_digital_sw_reset(mtk_edp);

	mtk_edp_update_bits(mtk_edp, EDP_TX_TOP_CLKGEN_0,
			  0x0000000f, 0x0000000f);
}

static irqreturn_t mtk_edp_hpd_event_thread(int hpd, void *dev)
{
	struct mtk_edp *mtk_edp = dev;
	unsigned long flags;
	u32 status;
	int ret = 0;

	pr_info("[eDPTX] %s+\n", __func__);
	if (mtk_edp->need_debounce && mtk_edp->train_info.cable_plugged_in)
		msleep(100);

	spin_lock_irqsave(&mtk_edp->irq_thread_lock, flags);
	status = mtk_edp->irq_thread_handle;
	mtk_edp->irq_thread_handle = 0;
	spin_unlock_irqrestore(&mtk_edp->irq_thread_lock, flags);

	if (status & MTK_DP_THREAD_HPD_EVENT) {
		dev_info(mtk_edp->dev, "[eDPTX] Receive IRQ from sink devices\n");
		// mtk_edp_hpd_sink_event(mtk_edp);
		ret = mtk_edp_training(mtk_edp);
		if (ret)
			pr_info("[eDPTX] link trainning failed %d\n", ret);

		pr_info("[eDPTX] %s-\n", __func__);
		return IRQ_HANDLED;
	}

	mtk_edp->edp_ui_enable = true;
	if (mtk_edp->edp_ui_enable) {
		if (mtk_edp->external_monitor) {
			if (mtk_edp->train_info.cable_plugged_in)
				edp_notify_uevent_user(&edptx_notify_data,
					DPTX_STATE_ACTIVE);
			else
				edp_notify_uevent_user(&edptx_notify_data,
					DPTX_STATE_NO_DEVICE);
		} else {
			if (mtk_edp->use_hpd) {
				if (mtk_edp->train_info.cable_plugged_in)
					edp_notify_uevent_user(&edptx_notify_data,
						DPTX_STATE_ACTIVE);
				else
					edp_notify_uevent_user(&edptx_notify_data,
						DPTX_STATE_NO_DEVICE);
			} else {
				edp_notify_uevent_user(&edptx_notify_data,
					DPTX_STATE_ACTIVE);
			}
		}
	}

	if (status & MTK_DP_THREAD_CABLE_STATE_CHG) {
		if (mtk_edp->bridge.dev)
			drm_helper_hpd_irq_event(mtk_edp->bridge.dev);

		if (!mtk_edp->train_info.cable_plugged_in) {
			dev_info(mtk_edp->dev, "[eDPTX] MTK_DP_HPD_DISCONNECT\n");
			if (mtk_edp->data->bridge_type != DRM_MODE_CONNECTOR_eDP) {
				mtk_dp_disable_sdp_aui(mtk_edp);
				memset(&mtk_edp->info.audio_cur_cfg, 0,
					sizeof(mtk_edp->info.audio_cur_cfg));
			}
			mtk_edp_video_mute(mtk_edp, true);
			mtk_edp_set_idle_pattern(mtk_edp, true);
			mtk_edp_update_bits(mtk_edp, MTK_DP_TOP_PWR_STATE,
					DP_PWR_STATE_BANDGAP_TPLL,
					DP_PWR_STATE_MASK);
			mtk_edp->need_debounce = false;
			mod_timer(&mtk_edp->debounce_timer,
				  jiffies + msecs_to_jiffies(100) - 1);
		} else {
			dev_info(mtk_edp->dev, "[eDPTX] MTK_DP_HPD_CONNECT\n");
		}
	}

	pr_info("[eDPTX] %s-\n", __func__);

	return IRQ_HANDLED;
}

static irqreturn_t mtk_edp_hpd_event(int hpd, void *dev)
{
	struct mtk_edp *mtk_edp = dev;
	bool cable_sta_chg = false;
	unsigned long flags;
	u32 irq_status = mtk_edp_swirq_get_clear(mtk_edp) |
			 mtk_edp_hwirq_get_clear(mtk_edp);

	if (!irq_status)
		return IRQ_HANDLED;

	spin_lock_irqsave(&mtk_edp->irq_thread_lock, flags);

	if (irq_status & MTK_DP_HPD_INTERRUPT) {
		mtk_edp->irq_thread_handle |= MTK_DP_THREAD_HPD_EVENT;
		spin_unlock_irqrestore(&mtk_edp->irq_thread_lock, flags);
		pr_info("[eDPTX] HPD INTERRUPT.\n");
		return IRQ_WAKE_THREAD;
	}
	/* Cable state is changed */
	if (irq_status != MTK_DP_HPD_INTERRUPT) {
		mtk_edp->irq_thread_handle |= MTK_DP_THREAD_CABLE_STATE_CHG;
		cable_sta_chg = true;
	}

	spin_unlock_irqrestore(&mtk_edp->irq_thread_lock, flags);

	if (cable_sta_chg) {
		if (!!(mtk_edp_plug_state(mtk_edp))) {
			mtk_edp->train_info.cable_plugged_in = true;
			pr_info("[eDPTX] HPD CONNECT.\n");
		} else {
			mtk_edp->train_info.cable_plugged_in = false;
			pr_info("[eDPTX] HPD DISCONNECT.\n");
		}
	}

	return IRQ_WAKE_THREAD;
}

static int mtk_edp_wait_hpd_asserted(struct drm_dp_aux *mtk_aux, unsigned long wait_us)
{
	struct mtk_edp *mtk_edp = container_of(mtk_aux, struct mtk_edp, aux);
	u32 val;
	int ret;

	pr_info("[eDPTX] %s+\n", __func__);

	ret = regmap_read_poll_timeout(mtk_edp->regs, REG_364C_AUX_TX_P0,
				       val, !!(val & HPD_STATUS_DP_AUX_TX_P0_MASK),
				       wait_us / 100, wait_us);
	if (ret) {
		mtk_edp->train_info.cable_plugged_in = false;
		return ret;
	}

	mtk_edp->train_info.cable_plugged_in = true;

	ret = mtk_edp_parse_capabilities(mtk_edp);
	if (ret) {
		drm_info(mtk_edp->drm_dev, "Can't parse capabilities\n");
		return ret;
	}

	/* Training */
	ret = mtk_edp_training(mtk_edp);
	if (ret) {
		dev_info(mtk_edp->dev, "[eDPTX] Training failed, %d\n", ret);
		return ret;
	}

	pr_info("[eDPTX] %s+\n", __func__);

	return 0;
}

static int mtk_edp_dt_parse(struct mtk_edp *mtk_edp,
			   struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret;
	void __iomem *base;
	u32 linkrate;
	u32 lane_count;
	u32 read_value;

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	mtk_edp->regs = devm_regmap_init_mmio(dev, base, &mtk_edp_regmap_config);
	if (IS_ERR(mtk_edp->regs))
		return PTR_ERR(mtk_edp->regs);

	ret = device_property_read_u32(dev, "max-lane-count", &lane_count);

	if (lane_count == 0 || lane_count == 3 || lane_count > 4) {
		dev_info(dev, "[eDPTX] Invalid data lane size: %d\n", lane_count);
		return -EINVAL;
	}

	mtk_edp->max_lanes = lane_count;

	/* example:  max-linkrate-mhz = 2700 == 2.7Gbps*/
	ret = device_property_read_u32(dev, "max-linkrate-mhz", &linkrate);
	if (ret) {
		dev_info(dev, "[eDPTX] Failed to read max linkrate: %d\n", ret);
		return ret;
	}

	mtk_edp->max_linkrate = drm_dp_link_rate_to_bw_code(linkrate * 100);

	dev_info(dev, "[eDPTX] SoC support max_lanes:%d, max_linkrate:0x%x\n",
			mtk_edp->max_lanes, mtk_edp->max_linkrate);

	ret = of_property_read_u32(dev->of_node, MTK_EDP_MODE_EXTERNAL_MONITOR, &read_value);
	mtk_edp->external_monitor = (!ret) ? !!read_value : false;

	ret = of_property_read_u32(dev->of_node, MTK_EDP_MODE_USE_EDID, &read_value);
	mtk_edp->use_edid = (!ret) ? !!read_value : false;

	ret = of_property_read_u32(dev->of_node, MTK_EDP_MODE_USE_HPD, &read_value);
	mtk_edp->use_hpd = (!ret) ? !!read_value : false;

	dev_info(dev, "[eDPTX] use external monitor:%d, use edid:%d use hpd:%d\n",
			mtk_edp->external_monitor,
			mtk_edp->use_edid,
			mtk_edp->use_hpd);

	return 0;
}

static void mtk_edp_update_plugged_status(struct mtk_edp *mtk_edp)
{
	if (!mtk_edp->data->audio_supported || !mtk_edp->audio_enable)
		return;

	mutex_lock(&mtk_edp->update_plugged_status_lock);
	if (mtk_edp->plugged_cb && mtk_edp->codec_dev)
		mtk_edp->plugged_cb(mtk_edp->codec_dev,
				   mtk_edp->enabled &
				   mtk_edp->info.audio_cur_cfg.detect_monitor);
	mutex_unlock(&mtk_edp->update_plugged_status_lock);
}

static enum drm_connector_status mtk_edp_bdg_detect(struct drm_bridge *bridge)
{

	struct mtk_edp *mtk_edp = mtk_edp_from_bridge(bridge);
	enum drm_connector_status ret = connector_status_disconnected;
	bool enabled = mtk_edp->enabled;
	u8 sink_count = 0;
	int ret_value = 0;

	pr_info("[eDPTX] %s\n", __func__);


	if (!mtk_edp->train_info.cable_plugged_in)
		return ret;

	if (mtk_edp->next_bridge)
		return connector_status_connected;

	if (!enabled)
		mtk_edp_aux_panel_poweron(mtk_edp, true);

	/*
	 * Some dongles still source HPD when they do not connect to any
	 * sink device. To avoid this, we need to read the sink count
	 * to make sure we do connect to sink devices. After this detect
	 * function, we just need to check the HPD connection to check
	 * whether we connect to a sink device.
	 */
	ret_value = drm_dp_dpcd_readb(&mtk_edp->aux, DP_SINK_COUNT, &sink_count);
	if (ret_value < 0) {
		pr_info("[eDPTX] Failed to read sink count 0x%x\n", ret);
		return ret;
	}

	if (DP_GET_SINK_COUNT(sink_count))
		ret = connector_status_connected;

	if (!enabled)
		mtk_edp_aux_panel_poweron(mtk_edp, false);

	return ret;
}

static struct edid *mtk_edp_get_edid(struct drm_bridge *bridge,
				    struct drm_connector *connector)
{

	struct mtk_edp *mtk_edp = mtk_edp_from_bridge(bridge);
	bool enabled = mtk_edp->enabled;
	struct edid *new_edid = NULL;
	struct mtk_dp_audio_cfg *audio_caps = &mtk_edp->info.audio_cur_cfg;

	pr_info("[eDPTX] %s+\n", __func__);

	if (!enabled) {
		drm_atomic_bridge_chain_pre_enable(bridge, connector->state->state);
		mtk_edp_aux_panel_poweron(mtk_edp, true);
	}

	new_edid = drm_get_edid(connector, &mtk_edp->aux.ddc);

	/*
	 * Parse capability here to let atomic_get_input_bus_fmts and
	 * mode_valid use the capability to calculate sink bitrates.
	 */
	if (mtk_edp_parse_capabilities(mtk_edp)) {
		drm_err(mtk_edp->drm_dev, "Can't parse capabilities\n");
		kfree(new_edid);
		new_edid = NULL;
	}

	if (new_edid) {
		struct cea_sad *sads;

		audio_caps->sad_count = drm_edid_to_sad(new_edid, &sads);
		kfree(sads);

		audio_caps->detect_monitor = drm_detect_monitor_audio(new_edid);
	}

	if (!enabled) {
		mtk_edp_aux_panel_poweron(mtk_edp, false);
		drm_atomic_bridge_chain_post_disable(bridge, connector->state->state);
	}

	pr_info("[eDPTX] EDID raw data:\n");
	print_hex_dump(KERN_NOTICE, "\t", DUMP_PREFIX_NONE, 16, 1,
					new_edid, EDID_LENGTH * (new_edid->extensions + 1), false);

	pr_info("[eDPTX] %s-\n", __func__);
	return new_edid;
}

static ssize_t mtk_edp_aux_transfer(struct drm_dp_aux *mtk_aux,
				   struct drm_dp_aux_msg *msg)
{
	struct mtk_edp *mtk_edp = container_of(mtk_aux, struct mtk_edp, aux);
	bool is_read;
	u8 request;
	size_t accessed_bytes = 0;
	int ret = 0;

	if (!mtk_edp->train_info.cable_plugged_in) {
		ret = -EAGAIN;
		goto err;
	}

	if (msg == NULL) {
		pr_info("[eDPTX] msg is null.\n");
		return -EINVAL;
	}

	switch (msg->request) {
	case DP_AUX_I2C_MOT:
	case DP_AUX_I2C_WRITE:
	case DP_AUX_NATIVE_WRITE:
	case DP_AUX_I2C_WRITE_STATUS_UPDATE:
	case DP_AUX_I2C_WRITE_STATUS_UPDATE | DP_AUX_I2C_MOT:
		request = msg->request & ~DP_AUX_I2C_WRITE_STATUS_UPDATE;
		is_read = false;
		break;
	case DP_AUX_I2C_READ:
	case DP_AUX_NATIVE_READ:
	case DP_AUX_I2C_READ | DP_AUX_I2C_MOT:
		request = msg->request;
		is_read = true;
		break;
	default:
		dev_info(mtk_edp->dev, "invalid aux cmd = %d\n",
			msg->request);
		ret = -EINVAL;
		goto err;
	}

	do {
		size_t to_access = min_t(size_t, DP_AUX_MAX_PAYLOAD_BYTES,
					 msg->size - accessed_bytes);

		ret = mtk_edp_aux_do_transfer(mtk_edp, is_read, request,
					     msg->address + accessed_bytes,
					     msg->buffer + accessed_bytes,
					     to_access, &msg->reply);

		if (ret) {
			dev_info(mtk_edp->dev,
				 "[eDPTX] Failed to do AUX transfer: %d\n", ret);
			goto err;
		}
		accessed_bytes += to_access;
	} while (accessed_bytes < msg->size);

	return msg->size;
err:
	msg->reply = DP_AUX_NATIVE_REPLY_NACK | DP_AUX_I2C_REPLY_NACK;
	return ret;
}

static void mtk_edp_aux_init(struct mtk_edp *mtk_edp)
{
	mtk_edp->aux.name = "aux_mtk_edp";
	mtk_edp->aux.dev = mtk_edp->dev;
	mtk_edp->aux.drm_dev = mtk_edp->drm_dev;
	mtk_edp->aux.transfer = mtk_edp_aux_transfer;
	drm_dp_aux_init(&mtk_edp->aux);
}

static int mtk_edp_poweron(struct mtk_edp *mtk_edp)
{
	int ret;

	pr_info("[eDPTX] %s+\n",__func__);

	ret = phy_init(mtk_edp->phy);
	if (ret) {
		dev_info(mtk_edp->dev, "[eDPTX] Failed to initialize phy: %d\n", ret);
		return ret;
	}

	ret = mtk_edp_phy_configure(mtk_edp, 0x6, 1, false, NULL, NULL);
	if (ret) {
		dev_info(mtk_edp->dev, "[eDPTX] Failed to configure phy: %d\n", ret);
		goto err_phy_config;
	}


	mtk_edp_init_port(mtk_edp);
	mtk_edp_power_enable(mtk_edp);

	pr_info("[eDPTX] %s-\n",__func__);

err_phy_config:
		phy_exit(mtk_edp->phy);

	return ret;
}

static void mtk_edp_poweroff(struct mtk_edp *mtk_edp)
{
	mtk_edp_power_disable(mtk_edp);
	phy_exit(mtk_edp->phy);
}

static int mtk_edp_bridge_attach(struct drm_bridge *bridge,
				enum drm_bridge_attach_flags flags)
{
	struct mtk_edp *mtk_edp = mtk_edp_from_bridge(bridge);
	int ret;

	dev_info(mtk_edp->dev, "[eDPTX] %s+\n", __func__);

	if (!(flags & DRM_BRIDGE_ATTACH_NO_CONNECTOR)) {
		dev_info(mtk_edp->dev, "[eDPTX] Driver does not provide a connector!");
		return -EINVAL;
	}

	mtk_edp->drm_dev = bridge->dev;

	mtk_edp_aux_init(mtk_edp);
	ret = drm_dp_aux_register(&mtk_edp->aux);
	if (ret) {
		dev_info(mtk_edp->dev,
			"[eDPTX] Failed to register DP AUX channel: %d\n", ret);
		return ret;
	}

	ret = mtk_edp_poweron(mtk_edp);
	if (ret)
		goto err_aux_register;

	if (mtk_edp->next_bridge) {
		ret = drm_bridge_attach(bridge->encoder, mtk_edp->next_bridge,
					&mtk_edp->bridge, flags);
		if (ret) {
			drm_warn(mtk_edp->drm_dev,
				 "Failed to attach external bridge: %d\n", ret);
			goto err_bridge_attach;
		}
	}

	mtk_edp->drm_dev = bridge->dev;
	mtk_edp->aux.drm_dev = bridge->dev;

	mtk_edp_hwirq_get_clear(mtk_edp);

	if (mtk_edp->use_hpd) {
		irq_clear_status_flags(mtk_edp->irq, IRQ_NOAUTOEN);
		enable_irq(mtk_edp->irq);
		mtk_edp_hwirq_enable(mtk_edp, true);
	}

	for (ret= 0; ret < MAX_CRTC; ret++)
		if ((bridge->encoder->possible_crtcs >> ret) & 0x1)
			edptx_notify_data.crtc = ret;

	dev_info(mtk_edp->dev, "[eDPTX] %s-\n", __func__);

	return 0;

err_bridge_attach:
	mtk_edp_poweroff(mtk_edp);
err_aux_register:
	drm_dp_aux_unregister(&mtk_edp->aux);
	return ret;
}

static void mtk_edp_bridge_detach(struct drm_bridge *bridge)
{
	struct mtk_edp *mtk_edp = mtk_edp_from_bridge(bridge);

	if (mtk_edp->bridge.type != DRM_MODE_CONNECTOR_eDP) {
		mtk_edp_hwirq_enable(mtk_edp, false);
		disable_irq(mtk_edp->irq);
	}
	mtk_edp->drm_dev = NULL;
	mtk_edp_poweroff(mtk_edp);
	drm_dp_aux_unregister(&mtk_edp->aux);
}

static void mtk_edp_bridge_atomic_enable(struct drm_bridge *bridge,
					struct drm_bridge_state *old_state)
{
	struct mtk_edp *mtk_edp = mtk_edp_from_bridge(bridge);
	int ret;

	dev_info(mtk_edp->dev, "[eDPTX] %s+\n", __func__);
	mtk_edp->conn = drm_atomic_get_new_connector_for_encoder(old_state->base.state,
								bridge->encoder);
	if (!mtk_edp->conn) {
		drm_err(mtk_edp->drm_dev,
			"Can't enable bridge as connector is missing\n");
		return;
	}

	mtk_edp_aux_panel_poweron(mtk_edp, true);

	/* Training */

	mtk_edp_parse_capabilities(mtk_edp);

	ret = mtk_edp_training(mtk_edp);
	if (ret) {
		drm_err(mtk_edp->drm_dev, "[eDPTX] Training failed, %d\n", ret);
		goto power_off_aux;
	}

	ret = mtk_edp_video_config(mtk_edp);
	if (ret)
		goto power_off_aux;

	//eDPTX color bar frame

	mtk_edp_video_enable(mtk_edp, true);

	mtk_edp->audio_enable = false;
	if (mtk_edp->audio_enable) {
		mtk_dp_audio_setup(mtk_edp, &mtk_edp->info.audio_cur_cfg);
		mtk_dp_audio_mute(mtk_edp, false);
	} else {
		memset(&mtk_edp->info.audio_cur_cfg, 0,
		       sizeof(mtk_edp->info.audio_cur_cfg));
	}

	mtk_edp->enabled = true;
	mtk_edp_update_plugged_status(mtk_edp);

	dev_info(mtk_edp->dev, "[eDPTX] %s-\n", __func__);

	return;
power_off_aux:
	mtk_edp_update_bits(mtk_edp, MTK_DP_TOP_PWR_STATE,
			   DP_PWR_STATE_BANDGAP_TPLL,
			   DP_PWR_STATE_MASK);
}

static void mtk_edp_bridge_atomic_disable(struct drm_bridge *bridge,
					 struct drm_bridge_state *old_state)
{
	struct mtk_edp *mtk_edp = mtk_edp_from_bridge(bridge);

	pr_info("[eDPTX] %s+\n", __func__);
	mtk_edp->enabled = false;
	mtk_edp_update_plugged_status(mtk_edp);
	mtk_edp_video_enable(mtk_edp, false);
	mtk_dp_audio_mute(mtk_edp, true);

	if (mtk_edp->train_info.cable_plugged_in) {
		drm_dp_dpcd_writeb(&mtk_edp->aux, DP_SET_POWER, DP_SET_POWER_D3);
		usleep_range(2000, 3000);
	}

	/* power off aux */
	mtk_edp_update_bits(mtk_edp, MTK_DP_TOP_PWR_STATE,
			   DP_PWR_STATE_BANDGAP_TPLL,
			   DP_PWR_STATE_MASK);

	pr_info("[eDPTX] %s-\n", __func__);
	/* Ensure the sink is muted */
	msleep(20);
}

static enum drm_mode_status
mtk_edp_bridge_mode_valid(struct drm_bridge *bridge,
			 const struct drm_display_info *info,
			 const struct drm_display_mode *mode)
{
	struct mtk_edp *mtk_edp = mtk_edp_from_bridge(bridge);
	u32 bpp = info->color_formats & DRM_COLOR_FORMAT_YCBCR422 ? 16 : 24;
	u32 rate = mtk_edp->train_info.link_rate * 27000 *
				mtk_edp->train_info.lane_count;

	pr_info("[eDPTX] %s\n", __func__);

	if (rate < mode->clock * bpp / 8) {
		pr_info("[eDPTX] mode invalid rate = %d mode->clock * bpp/8 = %d\n",
				rate, (mode->clock * bpp / 8));
		return MODE_CLOCK_HIGH;
	}

	pr_info("[eDPTX] mode valid rate = %d\n", rate);
	return MODE_OK;
}

static u32 *mtk_edp_bridge_atomic_get_output_bus_fmts(struct drm_bridge *bridge,
						     struct drm_bridge_state *bridge_state,
						     struct drm_crtc_state *crtc_state,
						     struct drm_connector_state *conn_state,
						     unsigned int *num_output_fmts)
{
	u32 *output_fmts;

	*num_output_fmts = 0;
	output_fmts = kmalloc(sizeof(*output_fmts), GFP_KERNEL);
	if (!output_fmts)
		return NULL;

	*num_output_fmts = 1;
	output_fmts[0] = MEDIA_BUS_FMT_FIXED;

	pr_info("[eDPTX] %s num_output_fmts:%u output_fmts:0x%04x\n",
			__func__, *num_output_fmts, output_fmts[0]);

	return output_fmts;
}

static const u32 mt8678_input_fmts[] = {
	MEDIA_BUS_FMT_RGB888_1X24,
	MEDIA_BUS_FMT_YUV8_1X24,
	MEDIA_BUS_FMT_YUYV8_1X16,
};

static u32 *mtk_edp_bridge_atomic_get_input_bus_fmts(struct drm_bridge *bridge,
						    struct drm_bridge_state *bridge_state,
						    struct drm_crtc_state *crtc_state,
						    struct drm_connector_state *conn_state,
						    u32 output_fmt,
						    unsigned int *num_input_fmts)
{
	u32 *input_fmts;
	struct mtk_edp *mtk_edp = mtk_edp_from_bridge(bridge);
	struct drm_display_mode *mode = &crtc_state->adjusted_mode;
	struct drm_display_info *display_info =
		&conn_state->connector->display_info;
	u32 rate = mtk_edp->train_info.link_rate *
				mtk_edp->train_info.lane_count;

	pr_info("[eDPTX] %s+\n", __func__);
	*num_input_fmts = 0;

	/*
	 * If the linkrate is smaller than datarate of RGB888, larger than
	 * datarate of YUV422 and sink device supports YUV422, we output YUV422
	 * format. Use this condition, we can support more resolution.
	 */
	if ((rate < (mode->clock * 24 / 8)) &&
	    (rate > (mode->clock * 16 / 8)) &&
	    (display_info->color_formats & DRM_COLOR_FORMAT_YCBCR422)) {
		input_fmts = kcalloc(1, sizeof(*input_fmts), GFP_KERNEL);
		if (!input_fmts)
			return NULL;
		*num_input_fmts = 1;
		input_fmts[0] = MEDIA_BUS_FMT_YUYV8_1X16;
	} else {
		input_fmts = kcalloc(ARRAY_SIZE(mt8678_input_fmts),
				     sizeof(*input_fmts),
				     GFP_KERNEL);
		if (!input_fmts) {
			*num_input_fmts = 0;
			return NULL;
		}

		*num_input_fmts = ARRAY_SIZE(mt8678_input_fmts);
		memcpy(input_fmts, mt8678_input_fmts, sizeof(mt8678_input_fmts));
	}

	pr_info("[eDPTX] input_fmts=0x%04x\n", input_fmts[0]);

	return input_fmts;
}

static int mtk_edp_bridge_atomic_check(struct drm_bridge *bridge,
				      struct drm_bridge_state *bridge_state,
				      struct drm_crtc_state *crtc_state,
				      struct drm_connector_state *conn_state)
{
	struct mtk_edp *mtk_edp = mtk_edp_from_bridge(bridge);
	struct drm_display_info *display_info =
		&conn_state->connector->display_info;
	struct drm_crtc *crtc = conn_state->crtc;
	unsigned int input_bus_format;

	pr_info("[eDPTX] %s+\n", __func__);
	input_bus_format = bridge_state->input_bus_cfg.format;

	dev_info(mtk_edp->dev, "[eDPTX] input format 0x%04x, output format 0x%04x\n",
		bridge_state->input_bus_cfg.format,
		 bridge_state->output_bus_cfg.format);

	/* set edp output color depth */
	if (display_info->bpc)
		mtk_edp->color_depth = display_info->bpc;
	else
		mtk_edp->color_depth = 8;

	switch (input_bus_format) {
	case MEDIA_BUS_FMT_YUYV8_1X16:
		mtk_edp->info.format = DP_PIXELFORMAT_YUV422;
		break;
	case MEDIA_BUS_FMT_AYUV8_1X32:
		mtk_edp->info.format = DP_PIXELFORMAT_YUV420;
		break;
	default:
		mtk_edp->info.format = DP_PIXELFORMAT_RGB;
		break;
	}

	dev_info(mtk_edp->dev, "[eDPTX] color depth:%u, color format:%d\n",
		mtk_edp->color_depth, mtk_edp->info.format);

	if (!crtc) {
		drm_err(mtk_edp->drm_dev,
			"Can't enable bridge as connector state doesn't have a crtc\n");
		return -EINVAL;
	}

	drm_display_mode_to_videomode(&crtc_state->adjusted_mode, &mtk_edp->info.vm);
	pr_info("[eDPTX] %s-\n", __func__);

	return 0;
}

static const struct drm_bridge_funcs mtk_edp_bridge_funcs = {
	.atomic_check = mtk_edp_bridge_atomic_check,
	.atomic_duplicate_state = drm_atomic_helper_bridge_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_bridge_destroy_state,
	.atomic_get_output_bus_fmts = mtk_edp_bridge_atomic_get_output_bus_fmts,
	.atomic_get_input_bus_fmts = mtk_edp_bridge_atomic_get_input_bus_fmts,
	.atomic_reset = drm_atomic_helper_bridge_reset,
	.attach = mtk_edp_bridge_attach,
	.detach = mtk_edp_bridge_detach,
	.atomic_enable = mtk_edp_bridge_atomic_enable,
	.atomic_disable = mtk_edp_bridge_atomic_disable,
	.mode_valid = mtk_edp_bridge_mode_valid,
	.get_edid = mtk_edp_get_edid,
	.detect = mtk_edp_bdg_detect,
};

static void mtk_edp_debounce_timer(struct timer_list *t)
{
	struct mtk_edp *mtk_edp = from_timer(mtk_edp, t, debounce_timer);

	mtk_edp->need_debounce = true;
}

/*
 * HDMI audio codec callbacks
 */

static int mtk_dp_audio_hw_params(struct device *dev, void *data,
				  struct hdmi_codec_daifmt *daifmt,
				  struct hdmi_codec_params *params)
{
	struct mtk_edp *mtk_edp = dev_get_drvdata(dev);

	if (!mtk_edp->enabled) {
		dev_info(mtk_edp->dev, "%s, DP is not ready!\n", __func__);
		return -ENODEV;
	}

	mtk_edp->info.audio_cur_cfg.channels = params->cea.channels;
	mtk_edp->info.audio_cur_cfg.sample_rate = params->sample_rate;

	mtk_dp_audio_setup(mtk_edp, &mtk_edp->info.audio_cur_cfg);

	return 0;
}

static int mtk_dp_audio_startup(struct device *dev, void *data)
{
	struct mtk_edp *mtk_edp = dev_get_drvdata(dev);

	mtk_dp_audio_mute(mtk_edp, false);

	return 0;
}

static void mtk_dp_audio_shutdown(struct device *dev, void *data)
{
	struct mtk_edp *mtk_edp = dev_get_drvdata(dev);

	mtk_dp_audio_mute(mtk_edp, true);
}

static int mtk_dp_audio_get_eld(struct device *dev, void *data, uint8_t *buf,
				size_t len)
{
	struct mtk_edp *mtk_edp = dev_get_drvdata(dev);

	if (mtk_edp->enabled)
		memcpy(buf, mtk_edp->conn->eld, len);
	else
		memset(buf, 0, len);

	return 0;
}

static int mtk_dp_audio_hook_plugged_cb(struct device *dev, void *data,
					hdmi_codec_plugged_cb fn,
					struct device *codec_dev)
{
	struct mtk_edp *mtk_edp = data;

	mutex_lock(&mtk_edp->update_plugged_status_lock);
	mtk_edp->plugged_cb = fn;
	mtk_edp->codec_dev = codec_dev;
	mutex_unlock(&mtk_edp->update_plugged_status_lock);

	mtk_edp_update_plugged_status(mtk_edp);

	return 0;
}

static const struct hdmi_codec_ops mtk_dp_audio_codec_ops = {
	.hw_params = mtk_dp_audio_hw_params,
	.audio_startup = mtk_dp_audio_startup,
	.audio_shutdown = mtk_dp_audio_shutdown,
	.get_eld = mtk_dp_audio_get_eld,
	.hook_plugged_cb = mtk_dp_audio_hook_plugged_cb,
	.no_capture_mute = 1,
};

static int mtk_dp_register_audio_driver(struct device *dev)
{
	struct mtk_edp *mtk_edp = dev_get_drvdata(dev);
	struct hdmi_codec_pdata codec_data = {
		.ops = &mtk_dp_audio_codec_ops,
		.max_i2s_channels = 8,
		.i2s = 1,
		.data = mtk_edp,
	};

	mtk_edp->audio_pdev = platform_device_register_data(dev,
							   HDMI_CODEC_DRV_NAME,
							   PLATFORM_DEVID_AUTO,
							   &codec_data,
							   sizeof(codec_data));
	return PTR_ERR_OR_ZERO(mtk_edp->audio_pdev);
}

static int mtk_edp_register_phy(struct mtk_edp *mtk_edp)
{
	struct device *dev = mtk_edp->dev;

	mtk_edp->phy_dev = platform_device_register_data(dev, "mediatek-edp-phy",
							PLATFORM_DEVID_AUTO,
							&mtk_edp->regs,
							sizeof(struct regmap *));
	if (IS_ERR(mtk_edp->phy_dev)) {
		pr_info("[eDPTX] Failed to create device mediatek-dp-phy\n");
		return PTR_ERR(mtk_edp->phy_dev);
	}

	mtk_edp_get_calibration_data(mtk_edp);

	mtk_edp->phy = devm_phy_get(&mtk_edp->phy_dev->dev, "edp");
	if (IS_ERR(mtk_edp->phy)) {
		platform_device_unregister(mtk_edp->phy_dev);
		pr_info("[eDPTX] Failed to get phy\n");
		return PTR_ERR(mtk_edp->phy);
	}

	return 0;
}

static ssize_t state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;
	struct notify_dev *sdev = (struct notify_dev *)
		dev_get_drvdata(dev);

	if (sdev->print_state) {
		ret = sdev->print_state(sdev, buf);
		if (ret >= 0)
			return ret;
	}
	return sprintf(buf, "%d\n", sdev->state);
}

static ssize_t name_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int ret;
	struct notify_dev *sdev = (struct notify_dev *)
		dev_get_drvdata(dev);

	if (sdev->print_name) {
		ret = sdev->print_name(sdev, buf);
		if (ret >= 0)
			return ret;
	}
	return sprintf(buf, "%s\n", sdev->name);
}

static ssize_t crtc_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int ret;
	struct notify_dev *sdev = (struct notify_dev *)
		dev_get_drvdata(dev);

	if (sdev->print_crtc) {
		ret = sdev->print_crtc(sdev, buf);
		if (ret >= 0)
			return ret;
	}
	return sprintf(buf, "%d\n", sdev->crtc);
}

static DEVICE_ATTR_RO(state);
static DEVICE_ATTR_RO(name);
static DEVICE_ATTR_RO(crtc);

static int create_switch_class(void)
{
	if (!switch_edp_class) {
		switch_edp_class = class_create("edpswitch");
		if (IS_ERR(switch_edp_class))
			return PTR_ERR(switch_edp_class);
		atomic_set(&device_count, 0);
	}
	return 0;
}

int edptx_uevent_dev_register(struct notify_dev *sdev)
{
	int ret;

	if (!switch_edp_class) {
		ret = create_switch_class();

		if (ret == 0)
			pr_info("create_switch_class susesess\n");
		else {
			pr_info("create_switch_class fail\n");
			return ret;
		}
	}

	sdev->index = atomic_inc_return(&device_count);
	sdev->dev = device_create(switch_edp_class, NULL,
			MKDEV(0, sdev->index), NULL, sdev->name);

	if (sdev->dev != NULL) {
		pr_info("device create ok,index:0x%x\n", sdev->index);
		ret = 0;
	} else {
		pr_info("device create fail,index:0x%x\n", sdev->index);
		return -1;
	}

	ret = device_create_file(sdev->dev, &dev_attr_state);
	if (ret < 0) {
		device_destroy(switch_edp_class, MKDEV(0, sdev->index));
		pr_info("switch: Failed to register driver %s\n",
				sdev->name);
	}

	ret = device_create_file(sdev->dev, &dev_attr_name);
	if (ret < 0) {
		device_remove_file(sdev->dev, &dev_attr_state);
		pr_info("switch: Failed to register driver %s\n",
				sdev->name);
	}

	ret = device_create_file(sdev->dev, &dev_attr_crtc);
	if (ret < 0) {
		device_remove_file(sdev->dev, &dev_attr_state);
		device_remove_file(sdev->dev, &dev_attr_name);
		pr_info("switch: Failed to register driver %s\n",
				sdev->name);
	}

	dev_set_drvdata(sdev->dev, sdev);
	sdev->state = 0;

	return ret;
}

static const char LK_TAG_NAME[] = "mediatek_drm.lkdisplay=";

bool mtk_edp_get_lk_display(void)
{
	struct device_node *of_chosen;
	char *bootargs;
	char *ptr;
	char *display_token = NULL;
	char cmd[128];

	of_chosen = of_find_node_by_path("/chosen");
	if (!of_chosen)
		return false;

	bootargs = (char *)of_get_property(of_chosen,
		"bootargs", NULL);
	if (!bootargs)
		return false;

	ptr = strstr(bootargs, LK_TAG_NAME);
	if (!ptr)
		return false;

	strscpy(cmd, ptr, sizeof(cmd));
	cmd[sizeof(cmd) - 1UL] = '\0';

	ptr = cmd;
	display_token = strsep(&ptr, " ");

	if (!display_token)
		return false;

#ifdef CONFIG_MTK_DISP_NO_LK
	return false;
#else
	if (strnstr(display_token, "dp_intf0", strlen(display_token)))
		return true;
	return false;
#endif
}

static int mtk_edp_probe(struct platform_device *pdev)
{
	struct mtk_edp *mtk_edp;
	struct device *dev = &pdev->dev;
	struct drm_panel *panel = NULL;
	int ret;

	dev_info(dev, "[eDPTX] %s+\n",__func__);
	mtk_edp = devm_kzalloc(dev, sizeof(*mtk_edp), GFP_KERNEL);
	if (!mtk_edp)
		return -ENOMEM;

	g_mtk_edp = mtk_edp;
	mtk_edp->dev = dev;
	mtk_edp->data = (struct mtk_edp_data *)of_device_get_match_data(dev);

	ret = mtk_edp_dt_parse(mtk_edp, pdev);
	if (ret) {
		pr_info("[eDPTX] Failed to parse dt\n");
		return ret;
	}

	/*
	 * Request the interrupt and install service routine only if we are
	 * on full DisplayPort.
	 * For eDP, polling the HPD instead is more convenient because we
	 * don't expect any (un)plug events during runtime, hence we can
	 * avoid some locking.
	 */
	if (mtk_edp->use_hpd) {
		mtk_edp->irq = platform_get_irq(pdev, 0);
		if (mtk_edp->irq < 0) {
			pr_info("[eDPTX] Failed to request dp irq resource\n");
			return -EPROBE_DEFER;
		}

		spin_lock_init(&mtk_edp->irq_thread_lock);
		irq_set_status_flags(mtk_edp->irq, IRQ_NOAUTOEN);
		ret = devm_request_threaded_irq(dev, mtk_edp->irq, mtk_edp_hpd_event,
						mtk_edp_hpd_event_thread,
						IRQ_TYPE_LEVEL_HIGH, dev_name(dev),
						mtk_edp);
		if (ret) {
			pr_info("[eDPTX] Failed to request mediatek dptx irq\n");
			return ret;
		}


		mtk_edp->need_debounce = true;
		timer_setup(&mtk_edp->debounce_timer, mtk_edp_debounce_timer, 0);
	}else {
		mtk_edp->aux.wait_hpd_asserted = mtk_edp_wait_hpd_asserted;
	}

	if (!mtk_edp->external_monitor) {
		ret = drm_of_find_panel_or_bridge(dev->of_node, 1, 0,
					  &panel, &mtk_edp->next_bridge);
		if (ret == -ENODEV) {
			dev_info(dev, "[eDPTX] No panel connected in devicetree, continuing as external DP\n");
			mtk_edp->next_bridge = NULL;
		} else if (ret) {
			dev_info(dev, "[eDPTX] Failed to find panel or bridge: %d\n",
				ret);
			return ret;
		}

		if (mtk_edp->next_bridge)
			dev_info(dev, "[eDPTX] Found bridge node: %pOF\n", mtk_edp->next_bridge->of_node);

		if (panel) {
			dev_info(dev, "[eDPTX] Found panel node: %pOF\n", panel->dev->of_node);
			mtk_edp->next_bridge =
				devm_drm_panel_bridge_add(dev, panel);
			if (IS_ERR(mtk_edp->next_bridge)) {
				ret = PTR_ERR(mtk_edp->next_bridge);
				pr_info("[eDPTX] Failed to create bridge: %d\n", ret);
				return -EPROBE_DEFER;
			}
		}
	}

	/* notify HWC */
	edptx_notify_data.name = "edptx";
	edptx_notify_data.index = 0;
	edptx_notify_data.state = DPTX_STATE_NO_DEVICE;
	edptx_notify_data.crtc = -1;
	ret = edptx_uevent_dev_register(&edptx_notify_data);
	if (ret)
		dev_info(dev, "switch_dev_register failed, returned:%d!\n", ret);

	platform_set_drvdata(pdev, mtk_edp);

	if (mtk_edp->data->audio_supported) {
		mutex_init(&mtk_edp->update_plugged_status_lock);

		ret = mtk_dp_register_audio_driver(dev);
		if (ret) {
			dev_info(dev, "Failed to register audio driver: %d\n",
				ret);
			return ret;
		}
	}

	ret = mtk_edp_register_phy(mtk_edp);
	if (ret)
		return ret;

	mtk_edp->bridge.funcs = &mtk_edp_bridge_funcs;
	mtk_edp->bridge.of_node = dev->of_node;
	mtk_edp->bridge.type = mtk_edp->data->bridge_type;
	mtk_edp->bridge.ops = DRM_BRIDGE_OP_DETECT;
	if (mtk_edp->use_edid)
		mtk_edp->bridge.ops |= DRM_BRIDGE_OP_EDID;

	if (mtk_edp->use_hpd)
		mtk_edp->bridge.ops |= DRM_BRIDGE_OP_HPD;

	ret = devm_drm_bridge_add(dev, &mtk_edp->bridge);
	if (ret) {
		pr_info("[eDPTX] Failed to add bridge\n");
		return ret;
	}

	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);

	dev_info(dev, "[eDPTX] %s-\n",__func__);
	return 0;
}

static void mtk_edp_remove(struct platform_device *pdev)
{
	struct mtk_edp *mtk_edp = platform_get_drvdata(pdev);

	pm_runtime_put(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	if (mtk_edp->data->bridge_type != DRM_MODE_CONNECTOR_eDP)
		del_timer_sync(&mtk_edp->debounce_timer);
	platform_device_unregister(mtk_edp->phy_dev);
	if (mtk_edp->audio_pdev)
		platform_device_unregister(mtk_edp->audio_pdev);
}

int mtk_drm_ioctl_enable_edp(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct mtk_edp_enable *edp_enable = data;
	struct mtk_edp *mtk_edp = g_mtk_edp;
	int event;

	if ((edp_enable == NULL) || (mtk_edp == NULL)) {
		pr_info("[eDPTX] IOCTL: ERROR!\n");
		return -EFAULT;
	}

	dev_info(mtk_edp->dev, "[eDPTX] %s enable=%d\n", __func__, edp_enable->enable);
	event = mtk_edp_plug_state(mtk_edp) ? connector_status_connected :
		connector_status_disconnected;
	if (edp_enable->enable == true) {
		if (mtk_edp->edp_ui_enable == true) {
			dev_info(mtk_edp->dev, "[eDPTX] eDP has already been enabled, return\n");
			return 0;
		}
		mtk_edp->edp_ui_enable = true;
		if (event == connector_status_connected) {
			edp_notify_uevent_user(&edptx_notify_data,
				DPTX_STATE_ACTIVE);
		} else {
			edp_notify_uevent_user(&edptx_notify_data,
				DPTX_STATE_NO_DEVICE);
		}
	} else {
		if (mtk_edp->edp_ui_enable == false) {
			dev_info(mtk_edp->dev, "[eDPTX] eDP has already been disabled, return\n");
			return 0;
		}
		mtk_edp->edp_ui_enable = false;
		edp_notify_uevent_user(&edptx_notify_data,
			DPTX_STATE_NO_DEVICE);
	}
	return 0;
}
EXPORT_SYMBOL(mtk_drm_ioctl_enable_edp);

#ifdef CONFIG_PM_SLEEP
static int mtk_edp_suspend(struct device *dev)
{
	struct mtk_edp *mtk_edp = dev_get_drvdata(dev);

	dev_info(mtk_edp->dev, "[eDPTX] %s+\n", __func__);

	mtk_edp_power_disable(mtk_edp);
	if (mtk_edp->use_hpd)
		mtk_edp_hwirq_enable(mtk_edp, false);
	pm_runtime_put_sync(dev);

	dev_info(mtk_edp->dev, "[eDPTX] %s-\n", __func__);

	return 0;
}

static int mtk_edp_resume(struct device *dev)
{
	struct mtk_edp *mtk_edp = dev_get_drvdata(dev);

	dev_info(mtk_edp->dev, "[eDPTX] %s+\n", __func__);

	pm_runtime_get_sync(dev);
	mtk_edp_init_port(mtk_edp);
	if (mtk_edp->use_hpd)
		mtk_edp_hwirq_enable(mtk_edp, true);
	mtk_edp_power_enable(mtk_edp);

	dev_info(mtk_edp->dev, "[eDPTX] %s-\n", __func__);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(mtk_edp_pm_ops, mtk_edp_suspend, mtk_edp_resume);

static const struct mtk_edp_data mt8678_edp_data = {
	.bridge_type = DRM_MODE_CONNECTOR_eDP,
	.smc_cmd = MTK_DP_SIP_ATF_EDP_VIDEO_UNMUTE,
	.efuse_fmt = mt8678_edp_efuse_fmt,
	.audio_supported = false,
	.audio_m_div2_bit = MT8195_AUDIO_M_CODE_MULT_DIV_SEL_DP_ENC0_P0_DIV_2,
};

static const struct of_device_id mtk_edp_of_match[] = {
	{
		.compatible = "mediatek,mt8678-edp-tx",
		.data = &mt8678_edp_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_edp_of_match);

static struct platform_driver mtk_edp_driver = {
	.probe = mtk_edp_probe,
	.remove_new = mtk_edp_remove,
	.driver = {
		.name = "mediatek-drm-edp",
		.of_match_table = mtk_edp_of_match,
		.pm = &mtk_edp_pm_ops,
	},
};

module_platform_driver(mtk_edp_driver);

MODULE_AUTHOR("Jie-h.Hu <jie-h.hu@mediatek.com>");
MODULE_DESCRIPTION("MediaTek DisplayPort Driver");
MODULE_LICENSE("GPL");
