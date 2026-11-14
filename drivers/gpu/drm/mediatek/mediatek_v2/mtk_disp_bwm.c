// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <drm/drm_blend.h>
#include <drm/drm_framebuffer.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_rect.h"
#include "mtk_drm_drv.h"
#include "mtk_dsi.h"
#include "mtk_disp_bwm.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_plane.h"
#include "mtk_drm_fb.h"

#define DISP_REG_BWM_STA					(0x000UL)
#define DISP_REG_BWM_INTEN					(0x004UL)
	#define FLD_BWM_REG_CMT_INTEN REG_FLD_MSB_LSB(0, 0)
	#define FLD_BWM_FME_CPL_INTEN REG_FLD_MSB_LSB(1, 1)
	#define FLD_BWM_FME_UND_INTEN REG_FLD_MSB_LSB(2, 2)
	#define FLD_BWM_FME_SWRST_DONE_INTEN REG_FLD_MSB_LSB(3, 3)
	#define FLD_BWM_FME_HWRST_DONE_INTEN REG_FLD_MSB_LSB(4, 4)
	#define FLD_RDMA0_EOF_ABNORMAL_INTEN REG_FLD_MSB_LSB(5, 5)
	#define FLD_RDMA0_SMI_UNDERFLOW_INTEN REG_FLD_MSB_LSB(6, 6)
	#define FLD_ABNORMAL_SOF_INTEN REG_FLD_MSB_LSB(13, 13)
	#define FLD_BWM_START_INTEN REG_FLD_MSB_LSB(14, 14)
	#define FLD_BWM_ROI_TIMING_0_INTEN REG_FLD_MSB_LSB(15, 15)
	#define FLD_BWM_ROI_TIMING_1_INTEN REG_FLD_MSB_LSB(16, 16)
	#define FLD_BWM_ROI_TIMING_2_INTEN REG_FLD_MSB_LSB(17, 17)
	#define FLD_BWM_ROI_TIMING_3_INTEN REG_FLD_MSB_LSB(18, 18)
	#define FLD_BWM_ROI_TIMING_4_INTEN REG_FLD_MSB_LSB(19, 19)
	#define FLD_BWM_ROI_TIMING_5_INTEN REG_FLD_MSB_LSB(20, 20)
	#define FLD_BWM_ROI_TIMING_6_INTEN REG_FLD_MSB_LSB(21, 21)
	#define FLD_BWM_ROI_TIMING_7_INTEN REG_FLD_MSB_LSB(22, 22)
#define DISP_REG_BWM_INTSTA					(0x008UL)
	#define FLD_BWM_FME_CPL_INTSTA REG_FLD_MSB_LSB(1, 1)
	#define BWM_FME_SWRST_DONE_INTSTA BIT(3)
	#define FLD_BWM_ROI_TIMING_INTSTA REG_FLD_MSB_LSB(22, 15)
#define DISP_REG_BWM_EN						(0x00CUL)
	#define FLD_BWM_EN REG_FLD_MSB_LSB(0, 0)
#define DISP_REG_BWM_TRIG					(0x010UL)
	#define FLD_BWM_SW_TRIG REG_FLD_MSB_LSB(0, 0)
#define DISP_REG_BWM_RST					(0x014UL)
#define DISP_REG_BWM_SRC_CON				(0x02CUL)
#define DISP_REG_BWM_L_CON(n)				(0x030UL + 0x8 * (n))
	#define FLD_BWM_L_CLRFMT REG_FLD_MSB_LSB(3, 0)
	#define FLD_BWM_L_2ND_SUBBUF BIT(4)
	#define OVL_CON_BYTE_SWAP BIT(24)
#define DISP_REG_BWM_L_SRC_SIZE(n)			(0x034UL + 0x8 * (n))
	#define FLD_BWM_L_SRC_W REG_FLD_MSB_LSB(12, 0)
	#define FLD_BWM_L_SRC_H REG_FLD_MSB_LSB(28, 16)
#define DISP_REG_BWM_RDMA0_CTRL				(0x070UL)
#define DISP_REG_BWM_RDMA_ULTRA_SRC			(0x0A0UL)
	#define FLD_PREULTRA_BUF_SRC REG_FLD_MSB_LSB(1, 0)
	#define FLD_PREULTRA_RDMA_SRC REG_FLD_MSB_LSB(7, 6)
	#define FLD_ULTRA_BUF_SRC REG_FLD_MSB_LSB(9, 8)
	#define FLD_ULTRA_RDMA_SRC REG_FLD_MSB_LSB(15, 14)
#define DISP_REG_BWM_RDMA0_BUF_LOW			(0x0A4UL)
	#define FLD_PREULTRA_LOW_TH REG_FLD_MSB_LSB(11, 0)
	#define FLD_ULTRA_LOW_TH REG_FLD_MSB_LSB(23, 12)
#define DISP_REG_BWM_RDMA0_BUF_HIGH			(0x0A8UL)
	#define FLD_PREULTRA_HIGH_TH REG_FLD_MSB_LSB(23, 12)
#define DISP_REG_BWM_FUNC_DCM0				(0x0B8UL)
#define DISP_REG_BWM_L_BURST_ACC(n)			(0x0E0UL + 0x4 * (n))
#define DISP_REG_BWM_L_BURST_ACC_WIN_MAX(n)	(0x100UL + 0x4 * (n))
#define DISP_REG_BWM_BURST_MON_CFG			(0x120UL)
	#define FLD_BWM_BURST_ACC_EN REG_FLD_MSB_LSB(0, 0)
	#define FLD_BWM_BURST_ACC_FBDC REG_FLD_MSB_LSB(4, 4)
	#define FLD_BWM_BURST_ACC_WIN_SIZE REG_FLD_MSB_LSB(12, 8)
	#define FLD_BWM_BURST_ACC_ROT_EN REG_FLD_MSB_LSB(16, 16)
	#define FLD_BWM_BURST_ACC_ROT_WIN_SIZE REG_FLD_MSB_LSB(24, 20)
#define DISP_REG_BWM_L_HDR_ADDR(n)			(0x130UL + 0x10 * (n))
#define DISP_REG_BWM_L_HDR_PITCH(n)			(0x134UL + 0x10 * (n))
#define DISP_REG_BWM_L_ADDR_MSB(n)			(0x138UL + 0x10 * (n))
#define DISP_REG_BWM_L_BURST_ACC(n)			(0x0E0UL + 0x4 * (n))
#define DISP_REG_BWM_L_BURST_ACC_WIN_MAX(n)	(0x100UL + 0x4 * (n))

#define MT6991_OVL_BWM0_L0_AID_SETTING		(0xBB8UL)
#define DISP_REG_BWM_DDREN_CONFIG			(0x200UL)
	#define SW_DDREN_REQ BIT(2)
	#define DDREN_SW_MODE_EN BIT(3)
#define DISP_REG_BWM_DDREN_DEBUG			(0x204UL)

#define OVL_CON_CLRFMT_RGB (1UL)
#define OVL_CON_CLRFMT_RGBA8888 (2)
#define OVL_CON_CLRFMT_ARGB8888 (3)
#define OVL_CON_CLRFMT_RGB565(module)                                          \
	(((module)->data->fmt_rgb565_is_0 == true) ? 0UL : OVL_CON_CLRFMT_RGB)
#define OVL_CON_CLRFMT_RGB888(module)                                          \
	(((module)->data->fmt_rgb565_is_0 == true) ? OVL_CON_CLRFMT_RGB : 0UL)
#define OVL_CON_CLRFMT_UYVY(module) ((module)->data->fmt_uyvy)
#define OVL_CON_CLRFMT_YUYV(module) ((module)->data->fmt_yuyv)


struct mtk_disp_bwm {
	struct mtk_ddp_comp ddp_comp;
	const struct mtk_disp_bwm_data *data;
};

int active_layer_avg_info[OVL_LAYER_NR];
int active_layer_peak_info[OVL_LAYER_NR];
int enable_check;

void __iomem *mtk_bwm_mmsys_mapping_MT6991(struct mtk_ddp_comp *comp)
{
	struct mtk_drm_private *priv = comp->mtk_crtc->base.dev->dev_private;

	switch (comp->id) {
	case DDP_COMPONENT_BWM0:
		return priv->ovlsys0_regs;
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return 0;
	}
}

void __iomem *mtk_vdisp_ao_mapping_MT6993(struct mtk_ddp_comp *comp)
{
	struct mtk_ddp_comp *vdisp_ao_comp;

	vdisp_ao_comp = mtk_ddp_comp_find_by_id(&comp->mtk_crtc->base, DDP_COMPONENT_VDISP_AO);
	if (!vdisp_ao_comp) {
		DDPPR_ERR("%s failed to get vdisp_ao_comp\n", __func__);
		return 0;
	}

	switch (comp->id) {
	case DDP_COMPONENT_BWM0:
		return vdisp_ao_comp->regs;
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return 0;
	}
}

unsigned int mtk_ovl_bwm_aid_sel_MT6991(struct mtk_ddp_comp *comp)
{
	switch (comp->id) {
	case DDP_COMPONENT_BWM0:
		return MT6991_OVL_BWM0_L0_AID_SETTING;
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return 0;
	}
}

static unsigned int ovl_fmt_convert(struct mtk_disp_bwm *bwm, unsigned int fmt,
				    uint64_t modifier, unsigned int compress)
{
	switch (fmt) {
	default:
	case DRM_FORMAT_RGB565:
		return OVL_CON_CLRFMT_RGB565(bwm);
	case DRM_FORMAT_BGR565:
		return (unsigned int)OVL_CON_CLRFMT_RGB565(bwm);
	case DRM_FORMAT_RGB888:
		return OVL_CON_CLRFMT_RGB888(bwm);
	case DRM_FORMAT_BGR888:
		return (unsigned int)OVL_CON_CLRFMT_RGB888(bwm);
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_RGBA8888:
		if (modifier & MTK_FMT_PREMULTIPLIED)
			return OVL_CON_CLRFMT_ARGB8888;
		else
			return OVL_CON_CLRFMT_ARGB8888;
	case DRM_FORMAT_BGRX8888:
	case DRM_FORMAT_BGRA8888:
		if (modifier & MTK_FMT_PREMULTIPLIED)
			return OVL_CON_CLRFMT_ARGB8888;
		else
			return OVL_CON_CLRFMT_ARGB8888;
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
		if (modifier & MTK_FMT_PREMULTIPLIED)
			return OVL_CON_CLRFMT_ARGB8888;
		else
			return OVL_CON_CLRFMT_RGBA8888;
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_Y410:
		if (modifier & MTK_FMT_PREMULTIPLIED)
			return OVL_CON_CLRFMT_ARGB8888;
		else
			return OVL_CON_CLRFMT_RGBA8888;
	case DRM_FORMAT_UYVY:
		return OVL_CON_CLRFMT_UYVY(bwm);
	case DRM_FORMAT_YUYV:
		return OVL_CON_CLRFMT_YUYV(bwm);
	case DRM_FORMAT_ABGR2101010:
	case DRM_FORMAT_XBGR2101010:
		if (modifier & MTK_FMT_PREMULTIPLIED)
			return OVL_CON_CLRFMT_ARGB8888;
		return OVL_CON_CLRFMT_RGBA8888;
	case DRM_FORMAT_ABGR16161616F:
		if (modifier & MTK_FMT_PREMULTIPLIED)
			return OVL_CON_CLRFMT_ARGB8888;
		return OVL_CON_CLRFMT_RGBA8888;
	}
}

static inline struct mtk_disp_bwm *comp_to_bwm(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_bwm, ddp_comp);
}

int mtk_bwm_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	int i;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return 0;
	}

	DDPDUMP("== %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
	for (i = 0; i < 0x1e0; i += 0x10)
		mtk_serial_dump_reg(baddr, i, 4);
	mtk_cust_dump_reg(baddr, 0x1F0, 0x1F4, 0x1F8, 0x1FC);
	mtk_cust_dump_reg(baddr, 0x200, 0x204, -1, -1);

	return 0;
}

static void mtk_bwm_enable(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc = &mtk_crtc->base;
	int fps;
	u32 value = 0, mask = 0;
	unsigned int bw_monitor_config, line_time, h;

	DDPINFO("bwm_enable:%s\n", mtk_dump_comp_str(comp));
	enable_check = 0;

	SET_VAL_MASK(value, mask, 1, FLD_BWM_EN);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_REG_BWM_EN,
		       value, mask);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_BWM_RDMA0_CTRL, 0x1, 0x1);
	value = 0;
	mask = 0;
	SET_VAL_MASK(value, mask, 0, FLD_PREULTRA_BUF_SRC);
	SET_VAL_MASK(value, mask, 0, FLD_PREULTRA_RDMA_SRC);
	SET_VAL_MASK(value, mask, 0, FLD_ULTRA_BUF_SRC);
	SET_VAL_MASK(value, mask, 0, FLD_ULTRA_RDMA_SRC);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_BWM_RDMA_ULTRA_SRC, value, mask);
	value = 0;
	mask = 0;
	//bwm always preultra
	SET_VAL_MASK(value, mask, 0, FLD_ULTRA_LOW_TH);
	SET_VAL_MASK(value, mask, 32, FLD_PREULTRA_LOW_TH);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_BWM_RDMA0_BUF_LOW, value, mask);
	value = 0;
	mask = 0;
	SET_VAL_MASK(value, mask, 64, FLD_PREULTRA_HIGH_TH);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_BWM_RDMA0_BUF_HIGH,
			value, mask);

	/****************************************************************/
	/*BURST_ACC_FBDC: 1/0:fbdc size/actual BW(fbdc+sBCH)            */
	/*BURST_ACC_EN: 1: enable bw monitor 0: disable                 */
	/*BURST_ACC_WIN_SIZE:200us / (4AFBC line times(us) /1.2(Vblank))*/
	/****************************************************************/
	bw_monitor_config = REG_FLD_VAL(FLD_BWM_BURST_ACC_EN, 1);
	bw_monitor_config |= REG_FLD_VAL(FLD_BWM_BURST_ACC_FBDC, 0);

	if (mtk_crtc->panel_ext && mtk_crtc->panel_ext->params &&
		mtk_crtc->panel_ext->params->dyn_fps.vact_timing_fps != 0)
		fps = mtk_crtc->panel_ext->params->dyn_fps.vact_timing_fps;
	else
		fps = drm_mode_vrefresh(&crtc->state->adjusted_mode);
	h = crtc->state->adjusted_mode.vdisplay;
	line_time = 1000000 * 4 * 10 / fps / h / 12;

	ovl_win_size = (200 % line_time) ? (200 / line_time + 1) : (200 / line_time);
	bw_monitor_config |= REG_FLD_VAL(FLD_BWM_BURST_ACC_WIN_SIZE, ovl_win_size - 1);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_BWM_BURST_MON_CFG, bw_monitor_config, ~0);
}

void mtk_bwm_trigger(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	s32 reg_val1,reg_val2;

	reg_val1 = readl(comp->regs + DISP_REG_BWM_INTSTA);
	reg_val2 = readl(comp->regs + DISP_REG_BWM_DDREN_DEBUG);
	writel(0xc, comp->regs + DISP_REG_BWM_DDREN_CONFIG);

	writel(0x1, comp->regs + DISP_REG_BWM_TRIG);
	CRTC_MMP_MARK(0, bwm20, (unsigned long)(0x30000 | reg_val2), (unsigned long)reg_val1);
	enable_check = 0;
}

void mtk_bwm_calc_ratio(struct mtk_ddp_comp *comp)
{
	unsigned int avail_layer, i, j = 0;
	static bool aee_trigger = true;
	s32 avg_val, peak_val, int_val, tmp, ddren;
	int_val = readl(comp->regs + DISP_REG_BWM_INTSTA);
	avail_layer = __builtin_popcount(REG_FLD_VAL_GET(FLD_BWM_ROI_TIMING_INTSTA, int_val));

	for (i = 0; i < MAX_LAYER_RATIO_NUMBER; i++) {
		if (all_layer_compress_ratio_table[i].active) {
			if (j >= avail_layer) {
				all_layer_compress_ratio_table[i].average_ratio = 1000;
				all_layer_compress_ratio_table[i].peak_ratio = 1000;
			} else {
				avg_val = readl(comp->regs + DISP_REG_BWM_L_BURST_ACC(j));
				peak_val = readl(comp->regs + DISP_REG_BWM_L_BURST_ACC_WIN_MAX(j));
				all_layer_compress_ratio_table[i].average_ratio =
					(avg_val * active_layer_avg_info[j]) >> 14;
				all_layer_compress_ratio_table[i].peak_ratio =
					(peak_val* active_layer_peak_info[j]) >> 14;
				all_layer_compress_ratio_table[i].peak_ratio =
						all_layer_compress_ratio_table[i].peak_ratio >
						all_layer_compress_ratio_table[i].average_ratio ?
						all_layer_compress_ratio_table[i].peak_ratio :
						all_layer_compress_ratio_table[i].average_ratio;
				if ((all_layer_compress_ratio_table[i].peak_ratio != 0) &&
					(all_layer_compress_ratio_table[i].average_ratio == 0))
					all_layer_compress_ratio_table[i].average_ratio = 10;
				if (all_layer_compress_ratio_table[i].average_ratio > 1024 ||
					all_layer_compress_ratio_table[i].peak_ratio > 1024) {
					if (aee_trigger) {
						DDPPR_ERR("BWM20 layer%d ratio error,avg%d peak%d ar %d pr %d\n",
								i,  avg_val, peak_val,
								all_layer_compress_ratio_table[i].average_ratio,
								all_layer_compress_ratio_table[i].peak_ratio);
						aee_trigger = false;
						mtk_bwm_dump(comp);
						DDPINFO("i %d j %d avl %d avg %d peak %d ar %d pr %d key %llu\n",
							i, j, avail_layer, avg_val, peak_val,
							all_layer_compress_ratio_table[i].average_ratio,
							all_layer_compress_ratio_table[i].peak_ratio,
							all_layer_compress_ratio_table[i].key_value);
					}
					all_layer_compress_ratio_table[i].average_ratio = 1000;
					all_layer_compress_ratio_table[i].peak_ratio = 1000;
				}
				DDPDBG_BWM("%s i:%d j%d avl%d avg %d peak %d ar %d pr %d int 0x%x\n",
					__func__, i, j, avail_layer, avg_val, peak_val,
					all_layer_compress_ratio_table[i].average_ratio,
					all_layer_compress_ratio_table[i].peak_ratio, int_val);
				j++;
			}
		}
	}
	memset(active_layer_avg_info, 0, sizeof(active_layer_avg_info));
	memset(active_layer_peak_info, 0, sizeof(active_layer_peak_info));
	writel(0x0, comp->regs + DISP_REG_BWM_INTSTA);
	writel(0x1, comp->regs + DISP_REG_BWM_RST);
	writel(0x0, comp->regs + DISP_REG_BWM_RST);
	if (enable_check == 0)
		enable_check = 1;
	//add memory barrior to avoid bwm work after atomic commit
	wmb();
	ddren = readl(comp->regs + DISP_REG_BWM_DDREN_DEBUG);
	tmp = readl(comp->regs + DISP_REG_BWM_INTSTA);
	CRTC_MMP_MARK(0, bwm20, ddren, tmp);
	writel(0x0, comp->regs + DISP_REG_BWM_DDREN_CONFIG);
}

int mtk_bwm_idle_check(struct mtk_ddp_comp *comp)
{
	s32 tmp, reg_val;
	unsigned int loop_cnt;

	reg_val = readl(comp->regs + DISP_REG_BWM_INTSTA);
	CRTC_MMP_MARK(0, bwm20, 0xeeee, (unsigned long)reg_val);
	if (!(reg_val & 0x8) && enable_check == 1) {
		writel(0x1, comp->regs + DISP_REG_BWM_RST);
		writel(0x0, comp->regs + DISP_REG_BWM_RST);
		while (loop_cnt < 50) {
			tmp = readl(comp->regs + DISP_REG_BWM_INTSTA);
			if (tmp & 0x8)
				break;
			loop_cnt++;
			udelay(1);
		}
		CRTC_MMP_MARK(0, bwm20, 0xffff, loop_cnt);
		if (loop_cnt == 50)
			return 0;
		else
			return 1;
	} else
		return 1;
}


static int mtk_bwm_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			  enum mtk_ddp_io_cmd io_cmd, void *params)
{
	int ret = 0;

	if (!(comp->mtk_crtc && comp->mtk_crtc->base.dev)) {
		DDPINFO("%s %s %u has invalid CRTC or device\n",
			__func__, mtk_dump_comp_str(comp), io_cmd);
		return -INVALID;
	}

	switch (io_cmd) {
		case MTK_IO_CMD_BWM_TRIG: {
			mtk_bwm_trigger(comp, handle);
			break;
		}
		case MTK_IO_CMD_BWM_CALC_RATIO: {
			mtk_bwm_calc_ratio(comp);
			break;
		}
		case MTK_IO_CMD_BWM_ENABLE: {
			mtk_bwm_enable(comp, handle);
			break;
		}
		case MTK_IO_CMD_BWM_IDLE_CHECK: {
			ret = mtk_bwm_idle_check(comp);
			break;
		}
		default:
			break;
	}

	return ret;
}

bool bwm_compr_l_config_AFBC_V1_2(struct mtk_ddp_comp *comp,
			unsigned int idx, struct mtk_plane_state *state,
			struct cmdq_pkt *handle)
{
	struct mtk_disp_bwm *bwm = comp_to_bwm(comp);
	unsigned int tile_w = AFBC_V1_2_TILE_W;
	unsigned int tile_h = AFBC_V1_2_TILE_H;
	struct drm_plane_state *drm_state = &state->base;
	struct drm_framebuffer *fb = drm_state->fb;
	unsigned int fmt = fb->format->format;
	unsigned int val = 0, tmp_bw;
	unsigned int pitch = fb->pitches[0];
	unsigned int Bpp = mtk_drm_format_plane_cpp(fmt, 0);
	unsigned int lx_hdr_pitch, lx_src_size, tile_offset;
	unsigned int src_x = drm_state->src.x1 >> 16;
	unsigned int src_y = drm_state->src.y1 >> 16;
	unsigned int src_w = drm_rect_width(&drm_state->src) >> 16;
	unsigned int src_h = drm_rect_height(&drm_state->src) >> 16;
	unsigned int src_x_align, src_w_align;
	unsigned int src_y_align, src_y_half_align;
	unsigned int src_y_end_align, src_y_end_half_align;
	unsigned int src_h_align = 0, src_h_half_align = 0, lx_2nd_subbuf = 0;
	dma_addr_t lx_hdr_addr, addr = mtk_fb_get_dma(fb), lye_addr, addr_msb;
	int rotate = 0;
	struct mtk_panel_params *params = NULL;
	unsigned int expand = 1 << 24;
	unsigned int pixel_blend_mode = DRM_MODE_BLEND_PIXEL_NONE;
	unsigned int modifier = 0;
	unsigned long record0 = 0;
	unsigned long record1 = 0;
	s32 reg_val;
	void __iomem *aid_sel_baddr = 0;
	unsigned int aid_sel_offset = 0;

	if (comp->mtk_crtc)
		params = mtk_drm_get_lcm_ext_params(&comp->mtk_crtc->base);
	if (params && params->rotate == MTK_PANEL_ROTATE_180)
		if (drm_crtc_index(&comp->mtk_crtc->base) == 0)
			rotate = 1;

	if (state->base.fb) {
		if (state->base.fb->format->has_alpha)
			pixel_blend_mode = state->base.pixel_blend_mode;

		DDPDBG("Blending: has_alpha %d pixel_blend_mode=0x%x fmt=0x%x\n",
			state->base.fb->format->has_alpha, state->base.pixel_blend_mode, fmt);
	}
	DDPDBG("%s:%d, addr:0x%lx, pitch:%d\n",
		__func__, __LINE__, (unsigned long)addr,
		pitch);
	DDPDBG("src:(%d,%d,%d,%d), fmt:%d, Bpp:%d\n",
		src_x, src_y,
		src_w, src_h,
		fmt, Bpp);

	if (pixel_blend_mode == DRM_MODE_BLEND_PREMULTI)
		modifier |= MTK_FMT_PREMULTIPLIED;

	val |= ovl_fmt_convert(bwm, fmt, modifier, 1);

	if (idx == 0)
		writel(0x0, comp->regs + DISP_REG_BWM_SRC_CON);

	reg_val = readl(comp->regs + DISP_REG_BWM_SRC_CON);
	reg_val |= 1 << idx;
	writel(reg_val, comp->regs + DISP_REG_BWM_SRC_CON);

	/* calculate for alignment */
	src_x_align = (src_x / tile_w) * tile_w;
	src_w_align = (1 + (src_x + src_w - 1) / tile_w) * tile_w - src_x_align;

	/* src_y_half_align, src_y_end_half_align,
	 * the start y offset and  stop y offset if half tile align
	 * such as 0 and 3, then the src_h_align is 4
	 */
	src_y_align = (src_y / tile_h) * tile_h;
	src_y_end_align = (1 + (src_y + src_h - 1) / tile_h) * tile_h - 1;
	src_h_align = src_y_end_align - src_y_align + 1;

	src_y_half_align = (src_y / (tile_h >> 1)) * (tile_h >> 1);
	src_y_end_half_align =
		(1 + (src_y + src_h - 1) / (tile_h >> 1)) * (tile_h >> 1) - 1;
	src_h_half_align = src_y_end_half_align - src_y_half_align + 1;

	if (rotate) {
		tile_offset = (src_x_align + src_w_align - tile_w) / tile_w +
			(pitch / tile_w / Bpp) *
			(src_y_align + src_h_align - tile_h) /
			tile_h;
		if (src_y_end_align == src_y_end_half_align)
			lx_2nd_subbuf = 1;
	} else {
		tile_offset = src_x_align / tile_w +
			(pitch / tile_w / Bpp) * src_y_align / tile_h;
		if (src_y_align != src_y_half_align)
			lx_2nd_subbuf = 1;
	}

	val |= REG_FLD_VAL(FLD_BWM_L_2ND_SUBBUF, lx_2nd_subbuf);
	writel(val, comp->regs + DISP_REG_BWM_L_CON(idx));

	if (fmt != DRM_FORMAT_RGB565 && fmt != DRM_FORMAT_BGR565) {
		src_h_align = src_h_half_align;
		src_y_align = src_y_half_align;
	}
	lx_src_size = (src_h_align << 16) | src_w_align;
	writel(lx_src_size, comp->regs + DISP_REG_BWM_L_SRC_SIZE(idx));

	if (src_w_align * src_h_align * Bpp)
		active_layer_avg_info[idx] = (16 * expand)/(src_w_align * src_h_align * Bpp);
	else {
		DDPPR_ERR("%s BWM: division by zero, src_w:%u src_h:%u\n", __func__,
				src_w_align, src_h_align);
	}
	if (src_w_align * ovl_win_size * Bpp) {
		if ((fmt == DRM_FORMAT_RGB565) ||
			(fmt == DRM_FORMAT_BGR565))
			active_layer_peak_info[idx] = (16 * expand) /
				(src_w_align * ovl_win_size * 8 * Bpp);
		else
			active_layer_peak_info[idx] = (16 * expand) /
				(src_w_align * ovl_win_size * 4 * Bpp);
	} else {
		DDPPR_ERR("%s BWM: division by zero, src_w:%u ovl_win_size:%u\n",
				__func__, src_w_align, ovl_win_size);
	}

	tmp_bw = src_h_align * src_w_align / 32 / 8 * 16 / 500;
	if (tmp_bw == 0)
		tmp_bw = 1;
	comp->qos_bw += tmp_bw;

	record0 = ((0xFFFF & src_h_align) << 16) | (0xFFFF & src_w_align);
	record1 = ((0xFFFF & active_layer_avg_info[idx]) << 16) |
			(0xFFFF & active_layer_peak_info[idx]);
	CRTC_MMP_MARK(0, bwm20, record0, record1);

	lx_hdr_addr = addr + tile_offset *
	    AFBC_V1_2_HEADER_SIZE_PER_TILE_BYTES;

	writel(lx_hdr_addr, comp->regs + DISP_REG_BWM_L_HDR_ADDR(idx));

	if (bwm->data->is_support_34bits)
		writel(lx_hdr_addr >> 32, comp->regs + DISP_REG_BWM_L_ADDR_MSB(idx));
	lye_addr = readl(comp->regs + DISP_REG_BWM_L_HDR_ADDR(idx));
	addr_msb = readl(comp->regs + DISP_REG_BWM_L_ADDR_MSB(idx));
	CRTC_MMP_MARK(0, bwm20, (unsigned long)(lye_addr >> 2 | addr_msb << 30),
		(unsigned long)(lx_hdr_addr >> 2));
	if ((addr_msb << 32 | lye_addr) != lx_hdr_addr) {
		DDPPR_ERR("%s lye idx%u addr0x%llx reg addr0x%llx", __func__, idx, lx_hdr_addr,
			addr_msb << 32 | lye_addr);
		mtk_bwm_dump(comp);
	}

	lx_hdr_pitch = pitch / tile_w / Bpp *
		AFBC_V1_2_HEADER_SIZE_PER_TILE_BYTES;

	writel(lx_hdr_pitch, comp->regs + DISP_REG_BWM_L_HDR_PITCH(idx));

	if (bwm->data->aid_sel_baddr_mapping )
		aid_sel_baddr = bwm->data->aid_sel_baddr_mapping (comp);

	if (bwm->data->aid_sel_mapping)
		aid_sel_offset = bwm->data->aid_sel_mapping(comp);

	if (aid_sel_baddr && aid_sel_offset) {
		bool is_sec = mtk_drm_fb_is_secure(fb);

		if (is_sec && addr) {
			writel(BIT(0), aid_sel_baddr + aid_sel_offset
				+ bwm->data->aid_lye_ofs * idx);
		} else {
			writel(0x0, aid_sel_baddr + aid_sel_offset
				+ bwm->data->aid_lye_ofs * idx);
		}
	}

	return 0;
}

static void mtk_bwm_layer_config(struct mtk_ddp_comp *comp, unsigned int idx,
				 struct mtk_plane_state *state,
				 struct cmdq_pkt *handle)
{
	struct mtk_disp_bwm *bwm = comp_to_bwm(comp);

	if (!comp->mtk_crtc) {
		DDPINFO("%s %d no crtc\n", __func__, __LINE__);
		return;
	}

	if (bwm->data->compr_info && bwm->data->compr_info->l_config) {
		if (bwm->data->compr_info->l_config(comp,
			    idx, state, handle)) {
			DDPPR_ERR("wrong fbdc input config\n");
			return;
		}
	} else {
		DDPMSG("%s no compress info\n", __func__);
	}
}

static void mtk_bwm_prepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_prepare(comp);
}

static void mtk_bwm_unprepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_unprepare(comp);
}

static const struct mtk_ddp_comp_funcs mtk_disp_bwm_funcs = {
	.layer_config = mtk_bwm_layer_config,
	.io_cmd = mtk_bwm_io_cmd,
	.prepare = mtk_bwm_prepare,
	.unprepare = mtk_bwm_unprepare,
};

static irqreturn_t mtk_disp_bwm_irq_handler(int irq, void *dev_id)
{
	struct mtk_disp_bwm *priv = dev_id;
	struct mtk_ddp_comp *bwm = NULL;
	unsigned int val = 0;
	unsigned int ret = 0;

	if (IS_ERR_OR_NULL(priv))
		return IRQ_NONE;

	bwm = &priv->ddp_comp;
	if (IS_ERR_OR_NULL(bwm))
		return IRQ_NONE;

	if (mtk_drm_top_clk_isr_get(bwm) == false) {
		DDPIRQ("%s, top clk off\n", __func__);
		return IRQ_NONE;
	}

	val = readl(bwm->regs + DISP_REG_BWM_INTSTA);

	if (!val) {
		ret = IRQ_NONE;
		goto out;
	}
	DRM_MMP_MARK(IRQ, bwm->regs_pa, val);

	DDPIRQ("%s irq, val:0x%x\n", mtk_dump_comp_str(bwm), val);

	writel(~val, bwm->regs + DISP_REG_BWM_INTSTA);

	if (val & (1 << 1)) {
		DDPMSG("[IRQ] %s: frame done!\n", mtk_dump_comp_str(bwm));
	}

	ret = IRQ_HANDLED;

out:
	mtk_drm_top_clk_isr_put(bwm);

	return ret;
}

static int mtk_disp_bwm_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct mtk_disp_bwm *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	struct mtk_drm_private *private = drm_dev->dev_private;
	int ret;
	char buf[50];

	DDPINFO("%s+\n", __func__);

	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	if (mtk_drm_helper_get_opt(private->helper_opt,
			MTK_DRM_OPT_MMQOS_SUPPORT)) {
		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&priv->ddp_comp, "qos");
		priv->ddp_comp.qos_req = of_mtk_icc_get(dev, buf);
		if (!IS_ERR(priv->ddp_comp.qos_req))
			DDPMSG("%s, %s create success, dev:%s\n", __func__, buf, dev_name(dev));
	}

	DDPINFO("%s+\n", __func__);

	return 0;
}

static void mtk_disp_bwm_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_disp_bwm *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_bwm_component_ops = {
	.bind = mtk_disp_bwm_bind, .unbind = mtk_disp_bwm_unbind,
};

static int mtk_disp_bwm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_bwm *priv;
	enum mtk_ddp_comp_id comp_id;
	int irq, num_irqs;
	int ret, len;
	const __be32 *ranges = NULL;

	DDPINFO("%s+\n", __func__);
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_BWM);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	DDPINFO("%s comp_id:%d\n", __func__, comp_id);

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_bwm_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	ranges = of_get_property(dev->of_node, "dma-ranges", &len);
	if (ranges && priv->data && priv->data->is_support_34bits)
		ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34));

	num_irqs = platform_irq_count(pdev);
	if (num_irqs) {
		irq = platform_get_irq(pdev, 0);

		if (irq < 0)
			return irq;

		ret = devm_request_irq(dev, irq, mtk_disp_bwm_irq_handler,
						   IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(dev),
						   priv);
	}

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_bwm_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	DDPINFO("%s-\n", __func__);
	return ret;
}

static void mtk_disp_bwm_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mtk_disp_bwm_component_ops);
}

static const struct compress_info bwm_compr_info_mt6991 = {
	.name = "AFBC_V1_2_MTK_1",
	.l_config = &bwm_compr_l_config_AFBC_V1_2,
};

static const struct mtk_disp_bwm_data mt6991_bwm_driver_data = {
	.is_support_34bits = true,
	.compr_info = &bwm_compr_info_mt6991,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U,
	.fmt_yuyv = 5U,
	.aid_sel_mapping = &mtk_ovl_bwm_aid_sel_MT6991,
	.aid_sel_baddr_mapping = &mtk_bwm_mmsys_mapping_MT6991,
	.aid_lye_ofs = 0x4,
};

static const struct mtk_disp_bwm_data mt6993_bwm_driver_data = {
	.is_support_34bits = true,
	.compr_info = &bwm_compr_info_mt6991,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U,
	.fmt_yuyv = 5U,
	.aid_sel_mapping = &mtk_ovl_bwm_aid_sel_MT6991,
	.aid_sel_baddr_mapping = &mtk_vdisp_ao_mapping_MT6993,
	.aid_lye_ofs = 0x4,
};

static const struct of_device_id mtk_disp_bwm_driver_dt_match[] = {
	{.compatible = "mediatek,mt6991-disp-bwm",
	 .data = &mt6991_bwm_driver_data},
	{.compatible = "mediatek,mt6993-disp-bwm",
	 .data = &mt6993_bwm_driver_data},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_bwm_driver_dt_match);

struct platform_driver mtk_disp_bwm_driver = {
	.probe = mtk_disp_bwm_probe,
	.remove = mtk_disp_bwm_remove,
	.driver = {

			.name = "mediatek-disp-bwm",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_bwm_driver_dt_match,
		},
};


