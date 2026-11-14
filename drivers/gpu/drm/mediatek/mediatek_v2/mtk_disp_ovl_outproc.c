// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <drm/drm_blend.h>
#include <drm/drm_framebuffer.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#include "mtk_drm_drv.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_layering_rule_base.h"
#include "mtk_rect.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_graphics_base.h"
#include "mtk_drm_helper.h"
#include "mtk_drm_drv.h"
#include "platform/mtk_ovl_outproc_reg.h"
#include "mtk_disp_ovl_outproc.h"
#include "mtk_layer_layout_trace.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_gem.h"
#include "platform/mtk_drm_platform.h"

//To-do: It will be moved to ovlsys related define in the future

#define MT6991_OVL_OUTPROC0_L0_AID_SETTING	(0xB00UL)
#define MT6991_OVL_OUTPROC1_L0_AID_SETTING	(0xB10UL)
#define MT6991_OVL_OUTPROC2_L0_AID_SETTING	(0xB20UL)
#define MT6991_OVL_OUTPROC3_L0_AID_SETTING	(0xB30UL)
#define MT6991_OVL_OUTPROC4_L0_AID_SETTING	(0xB40UL)
#define MT6991_OVL_OUTPROC5_L0_AID_SETTING	(0xB50UL)

#define DISP_OVL_BYPASS_SHADOW_BIT			BIT(2)
#define DISP_OVL_OUTPROC_EN					BIT(0)
#define DISP_OVL_OUTPROC_RELAY_MODE_EN		BIT(5)


#define MAX_LAYER_NUM 1
struct mtk_ovl_backup_info {
	unsigned int layer;
	unsigned int layer_en;
	unsigned int con;
	dma_addr_t addr;
	unsigned int src_size;
	unsigned int src_pitch;
	unsigned int data_path_con;
};

struct mtk_disp_ovl_outproc {
	struct mtk_ddp_comp ddp_comp;
	const struct mtk_disp_ovl_outproc_data *data;
	unsigned int underflow_cnt;
	bool ovl_dis;
	int bg_w, bg_h;
	struct clk *fbdc_clk;
	struct mtk_ovl_backup_info backup_info[MAX_LAYER_NUM];
	unsigned int set_partial_update;
	unsigned int roi_height;
};

static inline struct mtk_disp_ovl_outproc *comp_to_ovl_outproc(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_ovl_outproc, ddp_comp);
}

int mtk_ovl_outproc_layer_num(struct mtk_ddp_comp *comp)
{
	return 1;
}

resource_size_t mtk_ovl_outproc_mmsys_mapping_MT6991(struct mtk_ddp_comp *comp)
{
	struct mtk_drm_private *priv = comp->mtk_crtc->base.dev->dev_private;

	switch (comp->id) {
	case DDP_COMPONENT_OVL0_OUTPROC0:
	case DDP_COMPONENT_OVL0_OUTPROC1:
	case DDP_COMPONENT_OVL0_OUTPROC2:
	case DDP_COMPONENT_OVL0_OUTPROC3:
	case DDP_COMPONENT_OVL0_OUTPROC4:
	case DDP_COMPONENT_OVL0_OUTPROC5:
		return priv->ovlsys0_regs_pa;
	case DDP_COMPONENT_OVL1_OUTPROC0:
	case DDP_COMPONENT_OVL1_OUTPROC1:
	case DDP_COMPONENT_OVL1_OUTPROC2:
	case DDP_COMPONENT_OVL1_OUTPROC3:
	case DDP_COMPONENT_OVL1_OUTPROC4:
	case DDP_COMPONENT_OVL1_OUTPROC5:
		return priv->ovlsys1_regs_pa;
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return 0;
	}
}

unsigned int mtk_ovl_outproc_sys_mapping_MT6991(struct mtk_ddp_comp *comp)
{
	switch (comp->id) {
	case DDP_COMPONENT_OVL0_OUTPROC0:
	case DDP_COMPONENT_OVL0_OUTPROC1:
	case DDP_COMPONENT_OVL0_OUTPROC2:
	case DDP_COMPONENT_OVL0_OUTPROC3:
	case DDP_COMPONENT_OVL0_OUTPROC4:
	case DDP_COMPONENT_OVL0_OUTPROC5:
		return 0;
	case DDP_COMPONENT_OVL1_OUTPROC0:
	case DDP_COMPONENT_OVL1_OUTPROC1:
	case DDP_COMPONENT_OVL1_OUTPROC2:
	case DDP_COMPONENT_OVL1_OUTPROC3:
	case DDP_COMPONENT_OVL1_OUTPROC4:
	case DDP_COMPONENT_OVL1_OUTPROC5:
		return 1;
	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return -1;
	}
}

unsigned int mtk_ovl_outproc_aid_sel_MT6991(struct mtk_ddp_comp *comp)
{
	switch (comp->id) {
	case DDP_COMPONENT_OVL0_OUTPROC0:
	case DDP_COMPONENT_OVL1_OUTPROC0:
		return MT6991_OVL_OUTPROC0_L0_AID_SETTING;
	case DDP_COMPONENT_OVL0_OUTPROC1:
	case DDP_COMPONENT_OVL1_OUTPROC1:
		return MT6991_OVL_OUTPROC1_L0_AID_SETTING;
	case DDP_COMPONENT_OVL0_OUTPROC2:
	case DDP_COMPONENT_OVL1_OUTPROC2:
		return MT6991_OVL_OUTPROC2_L0_AID_SETTING;
	case DDP_COMPONENT_OVL0_OUTPROC3:
	case DDP_COMPONENT_OVL1_OUTPROC3:
		return MT6991_OVL_OUTPROC3_L0_AID_SETTING;
	case DDP_COMPONENT_OVL0_OUTPROC4:
	case DDP_COMPONENT_OVL1_OUTPROC4:
		return MT6991_OVL_OUTPROC4_L0_AID_SETTING;
	case DDP_COMPONENT_OVL0_OUTPROC5:
	case DDP_COMPONENT_OVL1_OUTPROC5:
		return MT6991_OVL_OUTPROC5_L0_AID_SETTING;

	default:
		DDPPR_ERR("%s invalid ovl module=%d\n", __func__, comp->id);
		return 0;
	}
}

int mtk_ovl_outproc_aid_bit(struct mtk_ddp_comp *comp, bool is_ext, int id)
{
	if (is_ext)
		return mtk_ovl_outproc_layer_num(comp) + id;
	else
		return id;
}

static int mtk_ovl_outproc_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			  enum mtk_ddp_io_cmd io_cmd, void *params);

static void mtk_ovl_outproc_all_layer_off(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, int keep_first_layer)
{
	//struct mtk_disp_ovl_outproc *outproc = comp_to_ovl_outproc(comp);
	//const u16 *regs = outproc->data->regs;

	DDPINFO("%s+ %s\n", __func__, mtk_dump_comp_str(comp));

#ifdef IF_ZERO
	if (keep_first_layer) {
		return;
	}
#endif
}

static void mtk_ovl_outproc_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	unsigned int value = 0, mask = 0;
	struct mtk_drm_private *priv = comp->mtk_crtc->base.dev->dev_private;
	struct mtk_disp_ovl_outproc *outproc = comp_to_ovl_outproc(comp);
	const u16 *regs = outproc->data->regs;
	const u32 *reg_fld = outproc->data->reg_fld;

	DDPDBG("%s+ %s\n", __func__, mtk_dump_comp_str(comp));
	cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + regs[OVL_OUTPROC_RST], BIT(0), BIT(0));
	cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + regs[OVL_OUTPROC_RST], 0x0, BIT(0));
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_OUTPROC_INTSTA], 0, ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_OUTPROC_EN],
		       0x1, 0x21);

	value = 0;
	mask = 0;
	SET_VAL_MASK(value, mask, 1, reg_fld[FLD_OVL_OUTPROC_OUTPUT_CLAMP]);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + regs[OVL_OUTPROC_DATAPATH_CON],
		       value, mask);


	if (priv->data->mmsys_id == MMSYS_MT6989) {
		/* golden setting */
		value = 0;
		mask = 0;
		SET_VAL_MASK(value, mask, 1, reg_fld[FLD_OVL_OUTPROC_FUNC_DCM0]);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + regs[OVL_OUTPROC_FUNC_DCM0],
			       value, mask);
	} // need to check with DE

	DDPDBG("%s-\n", __func__);
}

static void mtk_ovl_outproc_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_disp_ovl_outproc *outproc = comp_to_ovl_outproc(comp);
	const u16 *regs = outproc->data->regs;

	DDPDBG("%s+ %s\n", __func__, mtk_dump_comp_str(comp));

	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_OUTPROC_INTEN], 0, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_OUTPROC_EN],
			   0x20, DISP_OVL_OUTPROC_RELAY_MODE_EN | DISP_OVL_OUTPROC_EN);

	mtk_ovl_outproc_all_layer_off(comp, handle, 0);

	comp->qos_bw = 0;
	comp->qos_bw_other = 0;
	comp->fbdc_bw = 0;
	DDPDBG("%s-\n", __func__);
}

static void mtk_ovl_outproc_reset(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_disp_ovl_outproc *outproc = comp_to_ovl_outproc(comp);
	const u16 *regs = outproc->data->regs;

	DDPDBG("%s+ %s\n", __func__, mtk_dump_comp_str(comp));
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_OUTPROC_RST], BIT(0), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_OUTPROC_RST], 0, ~0);
	DDPDBG("%s-\n", __func__);
}

static void _store_bg_roi(struct mtk_ddp_comp *comp, int h, int w)
{
	struct mtk_disp_ovl_outproc *outproc = comp_to_ovl_outproc(comp);

	outproc->bg_h = h;
	outproc->bg_w = w;
}

static int mtk_ovl_outproc_golden_setting(struct mtk_ddp_comp *comp,
				  struct mtk_ddp_config *cfg,
				  struct cmdq_pkt *handle);

static void mtk_ovl_outproc_config(struct mtk_ddp_comp *comp,
			   struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	unsigned int width, height;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc = &mtk_crtc->base;
	//struct mtk_drm_private *priv = crtc->dev->dev_private;
	//unsigned long crtc_idx = (unsigned long)drm_crtc_index(crtc);
	//int fps;
	struct mtk_disp_ovl_outproc *outproc = comp_to_ovl_outproc(comp);
	const u16 *regs = outproc->data->regs;

	DDPINFO("outproc_config:%s\n", mtk_dump_comp_str(comp));

	if (comp->mtk_crtc->is_dual_pipe) {
		if (cfg->tile_overhead.is_support) {
			if (outproc->data->is_right_ovl_comp && outproc->data->is_right_ovl_comp(comp))
				width = cfg->tile_overhead.right_in_width;
			else
				width = cfg->tile_overhead.left_in_width;
		} else
			width = cfg->w / 2;
		if (drm_crtc_index(crtc) == 2 && (width % 2)) {
			if (outproc->data->is_right_ovl_comp && outproc->data->is_right_ovl_comp(comp))
				width += 1;
			else
				width -= 1;
		}
	} else
		width = cfg->w;

	if (outproc->set_partial_update != MTK_PARTIAL_UPDATE_SISO)
		height = cfg->h;
	else
		height = outproc->roi_height;
	/*out_proc wa*/
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_OUTPROC_EN],
		       0x1, ~0);

	if (cfg->w != 0 && cfg->h != 0) {
		cmdq_pkt_write(handle, comp->cmdq_base,
				   comp->regs_pa + regs[OVL_OUTPROC_ROI_SIZE],
				   height << 16 | width, ~0);

		_store_bg_roi(comp, height, width);
	}

	mtk_ovl_outproc_golden_setting(comp, cfg, handle);

}

static void mtk_ovl_outproc_layer_on(struct mtk_ddp_comp *comp, unsigned int idx,
			     unsigned int ext_idx, struct cmdq_pkt *handle)
{
	DDPDBG("%s+ %s\n", __func__, mtk_dump_comp_str(comp));
}

static void mtk_ovl_outproc_layer_off(struct mtk_ddp_comp *comp, unsigned int idx,
			      unsigned int ext_idx, struct cmdq_pkt *handle)
{
	DDPDBG("%s+ %s\n", __func__, mtk_dump_comp_str(comp));
}

void mtk_ovl_outproc_swap(int *a, int *b)
{
	int tmp;

	tmp = *a;
	*a = *b;
	*b = tmp;
}

void mtk_ovl_outproc_transpose(int *matrix, int size)
{
	int i, j;

	for (i = 0; i < size; i++)
		for (j = 0; j < i; j++)
			if (i != j)
				mtk_ovl_outproc_swap(matrix + i * size + j, matrix + j * size + i);
}

static void mtk_ovl_outproc_layer_config(struct mtk_ddp_comp *comp, unsigned int idx,
				 struct mtk_plane_state *state,
				 struct cmdq_pkt *handle)
{
	DDPDBG("%s+ %s\n", __func__, mtk_dump_comp_str(comp));
}

//static void
//mtk_ovl_outproc_addon_rsz_config(struct mtk_ddp_comp *comp, enum mtk_ddp_comp_id prev,
//			 enum mtk_ddp_comp_id next, struct mtk_rect rsz_src_roi,
//			 struct mtk_rect rsz_dst_roi, struct cmdq_pkt *handle)
//{
//	struct mtk_disp_ovl_outproc *outproc = comp_to_ovl_outproc(comp);
//	const u16 *regs = outproc->data->regs;

//	if (prev == -1) {
//		cmdq_pkt_write(handle, comp->cmdq_base,
//			       comp->regs_pa + regs[OVL_OUTPROC_ROI_SIZE],
//			       rsz_src_roi.height << 16 | rsz_src_roi.width,
//			       ~0);
//		_store_bg_roi(comp, rsz_src_roi.height, rsz_src_roi.width);
//	}
//}

static void mtk_ovl_outproc_addon_config(struct mtk_ddp_comp *comp,
				 enum mtk_ddp_comp_id prev,
				 enum mtk_ddp_comp_id next,
				 union mtk_addon_config *addon_config,
				 struct cmdq_pkt *handle)
{
	unsigned int width, height;
	struct mtk_disp_ovl_outproc *outproc = comp_to_ovl_outproc(comp);
	const u16 *regs = outproc->data->regs;

	width = addon_config->addon_wdma_config.wdma_dst_roi.width;
	height = addon_config->addon_wdma_config.wdma_dst_roi.height;

	DDPINFO("%s,%s,w:%d, h:%d\n", __func__, mtk_dump_comp_str(comp), width, height);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + regs[OVL_OUTPROC_EN],
		       0x1, ~0);

	if (width != 0 && height != 0)
		cmdq_pkt_write(handle, comp->cmdq_base,
				   comp->regs_pa + regs[OVL_OUTPROC_ROI_SIZE],
				   height << 16 | width, ~0);

	return;
}

static int mtk_ovl_outproc_golden_setting(struct mtk_ddp_comp *comp,
				  struct mtk_ddp_config *cfg,
				  struct cmdq_pkt *handle)
{
	return 0;
}

static int mtk_ovl_outproc_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			  enum mtk_ddp_io_cmd io_cmd, void *params)
{
	int ret = 0;
	struct mtk_disp_ovl_outproc *outproc = comp_to_ovl_outproc(comp);
	const u16 *regs = outproc->data->regs;
	const u32 *reg_fld = outproc->data->reg_fld;
	struct mtk_drm_private *priv;

	if (!(comp->mtk_crtc && comp->mtk_crtc->base.dev)) {
		DDPINFO("%s %s %u has invalid CRTC or device\n",
			__func__, mtk_dump_comp_str(comp), io_cmd);
		return -INVALID;
	}

	priv = comp->mtk_crtc->base.dev->dev_private;
	switch (io_cmd) {
	case BACKUP_OVL_STATUS: {
		struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
		u32 idx = drm_crtc_index(&mtk_crtc->base);
		dma_addr_t slot = mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_OVL_STATUS(idx));

		cmdq_pkt_mem_move(handle, comp->cmdq_base,
			comp->regs_pa + regs[OVL_OUTPROC_STA],
			slot, CMDQ_THR_SPR_IDX3);
		break;
	}
	case IRQ_LEVEL_ALL: {
		unsigned int inten;

		inten = REG_FLD_VAL(reg_fld[FLD_OVL_OUTPROC_ABNORMAL_SOF_INTEN], 1) |
			REG_FLD_VAL(reg_fld[FLD_OVL_OUTPROC_FME_UND_INTEN], 1) |
			REG_FLD_VAL(reg_fld[FLD_OVL_OUTPROC_START_INTEN], 1);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + regs[OVL_OUTPROC_INTSTA], 0,
			       ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + regs[OVL_OUTPROC_INTEN], inten,
			       ~0);
		break;
	}
	case IRQ_LEVEL_NORMAL: {
		unsigned int inten;

		inten = REG_FLD_VAL(reg_fld[FLD_OVL_OUTPROC_FME_UND_INTEN], 1) |
			REG_FLD_VAL(reg_fld[FLD_OVL_OUTPROC_FME_CPL_INTEN], 1);

		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + regs[OVL_OUTPROC_INTSTA], 0,
			       ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + regs[OVL_OUTPROC_INTEN], inten,
			       ~0);
		break;
	}
	case IRQ_LEVEL_IDLE: {
		unsigned int inten;

		inten = REG_FLD_VAL(reg_fld[FLD_OVL_OUTPROC_REG_CMT_INTEN], 1) |
			REG_FLD_VAL(reg_fld[FLD_OVL_OUTPROC_FME_CPL_INTEN], 1) |
			REG_FLD_VAL(reg_fld[FLD_OVL_OUTPROC_START_INTEN], 1);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + regs[OVL_OUTPROC_INTEN], 0, inten);
		break;
	}
	default:
		break;
	}

	return ret;
}

#ifdef IF_ZERO
void mtk_ovl_outproc_dump_golden_setting(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	unsigned long rg0 = 0;
	unsigned int value;
	struct mtk_disp_ovl_outproc *outproc = comp_to_ovl_outproc(comp);
	const u16 *regs = outproc->data->regs;
	const u32 *reg_fld = outproc->data->reg_fld;

	rg0 = regs[OVL_OUTPROC_DATAPATH_CON];
	DDPDUMP("0x%03lx:0x%08x\n",
		rg0, readl(rg0 + baddr));

	value = readl(regs[OVL_OUTPROC_EN] + baddr);
	DDPDUMP("OVL_EN:%d\n", REG_FLD_VAL_GET(reg_fld[FLD_OVL_OUTPROC_EN], value));


	value = readl(regs[OVL_OUTPROC_DATAPATH_CON] + baddr);
	DDPDUMP("DATAPATH_CON\n");
	DDPDUMP("[0]:%u\n",
		REG_FLD_VAL_GET(reg_fld[FLD_OVL_OUTPROC_OUTPUT_CLAMP], value));

}
#endif

int mtk_ovl_outproc_dump(struct mtk_ddp_comp *comp)
{
	//struct mtk_disp_ovl_outproc *outproc = comp_to_ovl_outproc(comp);
	void __iomem *baddr = comp->regs;
	int i;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return 0;
	}

	if (comp->blank_mode)
		return 0;

	DDPDUMP("== %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);

	for (i = 0; i < 0x40; i += 0x10)
		mtk_serial_dump_reg(baddr, i, 4);
	for (i = 0x100; i < 0x160; i += 0x10)
		mtk_serial_dump_reg(baddr, i, 4);

	//mtk_ovl_outproc_dump_golden_setting(comp);

	return 0;
}


static void mtk_ovl_outproc_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_ovl_outproc *priv = dev_get_drvdata(comp->dev);
	int ret;
	struct mtk_disp_ovl_outproc *outproc = comp_to_ovl_outproc(comp);
	const u16 *regs = outproc->data->regs;
	//struct mtk_drm_private *dev_priv = NULL;

	mtk_ddp_comp_clk_prepare(comp);

	if (priv->fbdc_clk != NULL) {
		ret = clk_prepare_enable(priv->fbdc_clk);
		if (ret)
			DDPPR_ERR("clk prepare enable failed:%s\n",
				mtk_dump_comp_str(comp));
	}

	/* Bypass shadow register and read shadow register */
	if (outproc->data->need_bypass_shadow)
		mtk_ddp_write_mask_cpu(comp, DISP_OVL_BYPASS_SHADOW_BIT,
			regs[OVL_OUTPROC_SHADOW_CTRL], DISP_OVL_BYPASS_SHADOW_BIT);
	else
		mtk_ddp_write_mask_cpu(comp, 0,
			regs[OVL_OUTPROC_SHADOW_CTRL], DISP_OVL_BYPASS_SHADOW_BIT);

	/*
	 * if (comp->mtk_crtc && comp->mtk_crtc->base.dev) {
	 *	dev_priv = comp->mtk_crtc->base.dev->dev_private;
	 *	if (mtk_drm_helper_get_opt(dev_priv->helper_opt, MTK_DRM_OPT_LAYER_REC))
	 *		writel(0xffffffff, comp->regs + DISP_REG_OVL_OUTPROC_GDRDY_PRD);
	 * }
	 */ // nee to fix 20231222
}

static void mtk_ovl_outproc_unprepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_ovl_outproc *priv = dev_get_drvdata(comp->dev);

	if (priv->fbdc_clk != NULL)
		clk_disable_unprepare(priv->fbdc_clk);

	mtk_ddp_comp_clk_unprepare(comp);
}

static void
mtk_ovl_outproc_config_trigger(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkt,
		       enum mtk_ddp_comp_trigger_flag flag)
{
	switch (flag) {
	case MTK_TRIG_FLAG_PRE_TRIGGER:
	{
	/* may cause side effect, need check
	 *	if (comp->blank_mode)
	 *		break;
	 *	DDPINFO("%s+ %s\n", __func__, mtk_dump_comp_str(comp));
	 *
	 *	cmdq_pkt_write(pkt, comp->cmdq_base,
	 *			comp->regs_pa +DISP_REG_OVL_OUTPROC_RST, 0x1, 0x1);
	 *	cmdq_pkt_write(pkt, comp->cmdq_base,
	 *			comp->regs_pa +DISP_REG_OVL_OUTPROC_RST, 0x0, 0x1);
	 */
		break;
	}
#ifdef IF_ZERO /* not ready for dummy register method */
	case MTK_TRIG_FLAG_LAYER_REC:
	{
		u32 offset = 0;
		struct cmdq_pkt_buffer *qbuf;

		int i = 0;
		const int lnr = mtk_ovl_outproc_layer_num(comp);
		u32 ln_con = 0, ln_size = 0;

		struct mtk_drm_private *priv = NULL;

		if (!comp->mtk_crtc)
			return;

		priv = comp->mtk_crtc->base.dev->dev_private;
		if (!mtk_drm_helper_get_opt(priv->helper_opt,
					   MTK_DRM_OPT_LAYER_REC))
			return;

		if (comp->id == DDP_COMPONENT_OVL0_2L)
			offset = DISP_SLOT_LAYER_REC_OVL0_2L;
		else if (comp->id == DDP_COMPONENT_OVL0)
			offset = DISP_SLOT_LAYER_REC_OVL0;
		else
			return;

		qbuf = &comp->mtk_crtc->gce_obj.buf;

		cmdq_pkt_mem_move(pkt, comp->cmdq_base,
				  comp->regs_pa +DISP_REG_OVL_OUTPROC_GDRDY_PRD_NUM,
				  qbuf->pa_base + offset,
				  CMDQ_THR_SPR_IDX3);

		/*
		 * offset += 4;
		 * cmdq_pkt_mem_move(pkt, comp->cmdq_base,
		 *		  comp->regs_pa +DISP_REG_OVL_OUTPROC_SRC_CON,
		 *		  qbuf->pa_base + offset,
		 *		  CMDQ_THR_SPR_IDX3);
		 */ //nee to fix 20231222
		offset += 4;
		cmdq_pkt_mem_move(pkt, comp->cmdq_base,
				  comp->regs_pa + DISP_REG_OVL_OUTPROC_DATAPATH_CON,
				  qbuf->pa_base + offset,
				  CMDQ_THR_SPR_IDX3);
		/*
		 * offset += 4;
		 * cmdq_pkt_mem_move(pkt, comp->cmdq_base,
		 *		  comp->regs_pa +DISP_REG_OVL_OUTPROC_DATAPATH_EXT_CON,
		 *		  qbuf->pa_base + offset,
		 *		  CMDQ_THR_SPR_IDX3);
		 */ //nee to fix 20231222

		for (i = 0; i < lnr + 3; i++) {
			if (i < lnr) {
				ln_con = DISP_REG_OVL_CON(i);
				ln_size = DISP_REG_OVL_SRC_SIZE(i);
			} else {
				ln_con = DISP_REG_OVL_EL_CON(i - lnr);
				ln_size = DISP_REG_OVL_EL_SRC_SIZE(i - lnr);
			}

			offset += 0x4;
			cmdq_pkt_mem_move(pkt, comp->cmdq_base,
					  comp->regs_pa + ln_con,
					  qbuf->pa_base + offset,
					  CMDQ_THR_SPR_IDX3);
			offset += 0x4;
			cmdq_pkt_mem_move(pkt, comp->cmdq_base,
					  comp->regs_pa + ln_size,
					  qbuf->pa_base + offset,
					  CMDQ_THR_SPR_IDX3);
		}

		if (comp->id == DDP_COMPONENT_OVL0_2L) {
			if (offset >= DISP_SLOT_LAYER_REC_OVL0)
				DDPMSG("%s:error:ovl0_2l:offset overflow:%u\n",
				       __func__, offset);
		} else if (comp->id == DDP_COMPONENT_OVL0) {
			if (offset >= DISP_SLOT_LAYER_REC_END)
				DDPMSG("%s:error:ovl0:offset overflow:%u\n",
				       __func__, offset);
		}

		break;
	}
#endif
	default:
		break;
	}
}

static int mtk_ovl_outproc_set_partial_update(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *handle, struct mtk_rect partial_roi, unsigned int enable)
{
	struct mtk_disp_ovl_outproc *outproc = comp_to_ovl_outproc(comp);
	const u16 *regs = outproc->data->regs;
	unsigned int full_height = mtk_crtc_get_height_by_comp(__func__,
						&comp->mtk_crtc->base, comp, false);
	struct total_tile_overhead_v to_v_info;
	unsigned int top_overhead_v, bot_overhead_v;

	DDPDBG("%s, %s set partial update, height:%d, enable:%d\n",
			__func__, mtk_dump_comp_str(comp), partial_roi.height, enable);

	outproc->set_partial_update = enable;

	to_v_info = mtk_crtc_get_total_overhead_v(comp->mtk_crtc);
	top_overhead_v = to_v_info.top_overhead_v;
	bot_overhead_v = to_v_info.bot_overhead_v;

	if (comp->mtk_crtc->res_switch == RES_SWITCH_ON_AP &&
		comp->mtk_crtc->scaling_ctx.scaling_en)
		outproc->roi_height = to_v_info.in_height;
	else
		outproc->roi_height = partial_roi.height + (top_overhead_v + bot_overhead_v);

	DDPDBG("%s, %s overhead_v T:%d overhead_v B:%d, roi_height:%d\n",
			__func__, mtk_dump_comp_str(comp),
			top_overhead_v, bot_overhead_v, outproc->roi_height);

	if (outproc->set_partial_update == MTK_PARTIAL_UPDATE_SISO) {
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + regs[OVL_OUTPROC_ROI_SIZE],
				outproc->roi_height << 16, 0x1fff << 16);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + regs[OVL_OUTPROC_ROI_SIZE],
				full_height << 16, 0x1fff << 16);
	}

	return 0;
}


static const struct mtk_ddp_comp_funcs mtk_disp_ovl_outproc_funcs = {
	.config = mtk_ovl_outproc_config,
	.first_cfg = mtk_ovl_outproc_config,
	.start = mtk_ovl_outproc_start,
	.stop = mtk_ovl_outproc_stop,
	.reset = mtk_ovl_outproc_reset,
	.layer_on = mtk_ovl_outproc_layer_on,
	.layer_off = mtk_ovl_outproc_layer_off,
	.layer_config = mtk_ovl_outproc_layer_config,
	.addon_config = mtk_ovl_outproc_addon_config,
	.io_cmd = mtk_ovl_outproc_io_cmd,
	.prepare = mtk_ovl_outproc_prepare,
	.unprepare = mtk_ovl_outproc_unprepare,
	.config_trigger = mtk_ovl_outproc_config_trigger,
	.partial_update = mtk_ovl_outproc_set_partial_update,
};

static int mtk_disp_ovl_outproc_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct mtk_disp_ovl_outproc *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	unsigned int bg_h, bg_w;
	void __iomem *baddr;
	//struct mtk_drm_private *private = drm_dev->dev_private;
	int ret;

	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	baddr = priv->ddp_comp.regs;
	bg_w = readl(priv->data->regs[OVL_OUTPROC_ROI_SIZE] + baddr) & 0xfff,
	bg_h = (readl(priv->data->regs[OVL_OUTPROC_ROI_SIZE] + baddr) >> 16) & 0xfff,
	_store_bg_roi(&priv->ddp_comp, bg_h, bg_w);

	return 0;
}

static void mtk_disp_ovl_outproc_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_disp_ovl_outproc *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_ovl_outproc_component_ops = {
	.bind = mtk_disp_ovl_outproc_bind, .unbind = mtk_disp_ovl_outproc_unbind,
};

static int mtk_disp_ovl_outproc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_ovl_outproc *priv;
	enum mtk_ddp_comp_id comp_id;
	//int irq, num_irqs;
	int ret, len;
	const __be32 *ranges = NULL;

	DDPDBG("%s+\n", __func__);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;


	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_OVL_OUTPROC);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}


	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_ovl_outproc_funcs);

	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}
	priv->fbdc_clk = of_clk_get(dev->of_node, 1);
	if (IS_ERR(priv->fbdc_clk))
		priv->fbdc_clk = NULL;

	priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	ranges = of_get_property(dev->of_node, "dma-ranges", &len);
	if (ranges && priv->data && priv->data->is_support_34bits)
		ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34));

	writel(0, priv->ddp_comp.regs + priv->data->regs[OVL_OUTPROC_INTSTA]);
	writel(0, priv->ddp_comp.regs + priv->data->regs[OVL_OUTPROC_INTEN]);

	//num_irqs = platform_irq_count(pdev);
	/*
	 * if (num_irqs) {
	 *	irq = platform_get_irq(pdev, 0);
	 *
	 *	if (irq < 0)
	 *		return irq;
	 *
	 *	ret = devm_request_irq(dev, irq, mtk_disp_ovl_outproc_irq_handler,
	 *					   IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(dev),
	 *					   priv);
	 * }
	 */

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_ovl_outproc_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	DDPDBG("%s-\n", __func__);
	return ret;
}

static void mtk_disp_ovl_outproc_remove(struct platform_device *pdev)
{
	struct mtk_disp_ovl_outproc *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_ovl_outproc_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);
}

static const struct mtk_disp_ovl_outproc_data mt6991_ovl_driver_data = {
	.aid_sel_mapping = &mtk_ovl_outproc_aid_sel_MT6991,
	.aid_per_layer_setting = true,
	.mmsys_mapping = &mtk_ovl_outproc_mmsys_mapping_MT6991,
	.regs = ovl_outproc_regs_mt6991,
	.reg_fld = ovl_outproc_fld_mt6991,
};

static const struct mtk_disp_ovl_outproc_data mt6993_ovl_driver_data = {
	//.aid_sel_mapping = &mtk_ovl_outproc_aid_sel_MT6991,
	.aid_per_layer_setting = true,
	//.mmsys_mapping = &mtk_ovl_outproc_mmsys_mapping_MT6991,
	.regs = ovl_outproc_regs_mt6991,
	.reg_fld = ovl_outproc_fld_mt6991,
};


static const struct of_device_id mtk_disp_ovl_outproc_driver_dt_match[] = {
	{.compatible = "mediatek,mt6991-disp-ovl-outproc",
	 .data = &mt6991_ovl_driver_data},
	{.compatible = "mediatek,mt6993-disp-ovl-outproc",
	 .data = &mt6993_ovl_driver_data},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_ovl_outproc_driver_dt_match);

struct platform_driver mtk_ovl_outproc_driver = {
	.probe = mtk_disp_ovl_outproc_probe,
	.remove = mtk_disp_ovl_outproc_remove,
	.driver = {

			.name = "mediatek-disp-ovl-outproc",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_ovl_outproc_driver_dt_match,
		},
};
