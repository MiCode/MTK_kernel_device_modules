// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/iommu.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <mtk-smmu-v3.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_mmp.h"
#include "mtk_dump.h"

#define DISP_REG_GDMA_EN		0x000
#define GDMA_ENABLE BIT(0)
#define HG_FDMA_CK_ON BIT(8)
#define HG_FSMI_CK_ON BIT(9)
#define HF_FDMA_CK_ON BIT(10)

#define DISP_REG_MDP_RDMA_RESET	0x004
#define GDMA_RESET BIT(0)

#define DISP_REG_GDMA_INT_ENABLE	0x008
#define DISP_REG_GDMA_INT_STATUS	0x00C
#define IF_END			BIT(0)
#define GDMA_FME_CPL	BIT(1)
#define GDMA_START		BIT(2)
#define ABNORMAL_SOF	BIT(4)
#define RDMA_FME_UND	BIT(8)
#define RDMA_EOF_ABNORMAL	BIT(11)
#define RDMA_SMI_UNDERFLOW	BIT(12)

#define DISP_REG_GDMA_CFG		0x020
#define RELAY_MODE		BIT(0)
#define DRAM_MODE		BIT(1)
#define BGCLR_IN_SEL	BIT(2)
#define OUTPUT_NO_RND	BIT(4)
#define OUTPUT_CLAMP	BIT(5)
#define GCLAST_EN		BIT(6)
#define STALL_CG_ON		BIT(8)
#define GDMA_8BIT_MODE	BIT(9)
#define PIXEL_REVERSE	BIT(14)
#define PIXEL_MODE	BIT(15)

#define DISP_REG_GDMA_SHADOW_CTRL	0x024
#define DISP_REG_GDMA_CRC_EN		0x05C
#define DISP_REG_GDMA_SODI			0x060
#define DISP_REG_GDMA_STATUS		0x0A0
#define DISP_REG_GDMA_CRC			0x0AC

#define DISP_REG_GDMA_DEBUG0		0x0B0
#define DISP_REG_GDMA_DEBUG1		0x0B4
#define DISP_REG_GDMA_DEBUG2		0x0B8
#define DISP_REG_GDMA_DEBUG3		0x0BC
#define DISP_REG_GDMA_DEBUG4		0x0C0
#define DISP_REG_GDMA_DEBUG5		0x0C4
#define DISP_REG_GDMA_DEBUG6		0x0C8
#define DISP_REG_GDMA_DEBUG7		0x0D0
#define DISP_REG_GDMA_DEBUG8		0x0D4
#define DISP_REG_GDMA_DEBUG9		0x0D8
#define DISP_REG_GDMA_DEBUG10		0x0E0
#define DISP_REG_GDMA_DEBUG11		0x0E4
#define DISP_REG_GDMA_DEBUG12		0x0E8
#define DISP_REG_GDMA_DEBUG13		0x0EC
#define DISP_REG_GDMA_DEBUG14		0x0F0
#define DISP_REG_GDMA_DEBUG15		0x0F4

#define DISP_GDMA_MEM_ADDR				0x100
#define DISP_GDMA_MEM_LENGTH			0x104
#define DISP_GDMA_RDMA_FIFO_CTRL		0x108
#define DISP_GDMA_RDMA_MEM_GMC_SETTING2	0x10C
#define DISP_GDMA_RDMA_PAUSE_REGION		0x110
#define DISP_GDMA_MEM_ADDR_MSB			0x114
#define DISP_GDMA_RDMA_BURST_CON0		0x120
#define DISP_GDMA_RDMA_BURST_CON1		0x124
#define DISP_GDMA_BANK_CON				0x128
#define DISP_GDMA_RDMA_GREQ_NUM			0x130
#define DISP_GDMA_RDMA_GREQ_URG_NUM		0x134
#define DISP_GDMA_RDMA_ULTRA_SRC		0x140
#define DISP_GDMA_RDMA_BUF_LOW_TH		0x144
#define DISP_GDMA_RDMA_BUF_HIGH_TH		0x148
#define DISP_GDMA_GDRDY_PRD				0x170
#define DISP_GDMA_GDRDY_PRD_NUM			0x174
#define DISP_GDMA_FUNC_DC				0x1F0
#define DISP_GDMA_FUNC_DCM1				0x1F4
#define DISP_GDMA_DEBUG_MON_SEL			0x1F8
#define DISP_GDMA_APB_SOF				0x1FC
#define DISP_GDMA_UP_INTEN				0x200
#define DISP_GDMA_UP_INTSTA				0x204
#define DISP_GDMA_DDREN_CTRL			0x208
#define DDREN_REQ_DISABLE	BIT(0)
#define USE_HRT_DDREN_REQ	BIT(1)
#define DREN_SW_MODE_EN		BIT(2)
#define SW_DDREN_REQ		BIT(3)

#define DISP_GDMA_ARSLC_CFG				0x20C
#define SLB			BIT(0)
#define CACHE		BIT(1)
#define ALLOCATE	BIT(2)
#define SPECULA		BIT(3)
#define EXCLUSIVE	BIT(4)

#define DISP_GDMA_SLC_ID				0x210

struct mtk_disp_gdma {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
};
static void mtk_disp_gdma_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	int val = 0;

	if (!mtk_crtc) {
		DDPPR_ERR("%s mtk_crtc assign fail\n", __func__);
		return;
	}
	if (!comp) {
		DDPPR_ERR("find comp fail\n");
		return;
	}
	DDPINFO("%s,\n", __func__);

	if (bif_enabled(&mtk_crtc->base) && mtk_crtc->bif_info->read_comp)
		mtk_ddp_write_mask(comp, DDREN_REQ_DISABLE, DISP_GDMA_DDREN_CTRL, DDREN_REQ_DISABLE, handle);

	mtk_ddp_write_mask(comp, RDMA_FME_UND, DISP_REG_GDMA_INT_ENABLE, RDMA_FME_UND, handle);
	mtk_ddp_write_mask(comp, PIXEL_MODE, DISP_REG_GDMA_CFG, PIXEL_MODE, handle);
	val = (GDMA_ENABLE | HG_FDMA_CK_ON | HG_FSMI_CK_ON | HF_FDMA_CK_ON);
	mtk_ddp_write_mask(comp, val, DISP_REG_GDMA_EN, val, handle);
}
static void mtk_disp_gdma_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct iommu_domain *domain;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;
	struct mtk_bif_info *bif_info = mtk_crtc->bif_info;
	dma_addr_t addr = 0;
	int ret = 0;

	if (!comp) {
		DDPPR_ERR("find comp fail\n");
		return;
	}

	mtk_ddp_write_mask(comp, 0, DISP_REG_GDMA_EN, ~0, handle);
	mtk_ddp_write_mask(comp, 0, DISP_REG_GDMA_INT_ENABLE, ~0, handle);
}
void mtk_disp_gdma_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	int i;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}
	DDPDUMP("== DISP %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);

	for (i = 0x0; i <= 0x210; i += 0x10)
		mtk_cust_dump_reg(baddr, i, i + 0x4, i + 0x8, i + 0xc);
}
static void
mtk_disp_gdma_config(struct mtk_ddp_comp *comp, struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	DDPFUNC();
}
static void
mtk_disp_gdma_bif_read_config(struct mtk_ddp_comp *comp, struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;
	struct mtk_bif_info *bif_info = mtk_crtc->bif_info;
	dma_addr_t addr = bif_info->sram_pa;
	int width = bif_info->src_roi.width;
	int height = bif_info->src_roi.height;
	unsigned int fmt = DRM_FORMAT_RGB888;
	unsigned int hact = 0, vtotal = 0, vact = 0, vrefresh = 0;
	unsigned int bpp = mtk_get_format_bpp(fmt);
	unsigned int mem_len = height*width*bpp;
	unsigned long long bw_base = 0;
	struct iommu_domain *domain;
	int ret = 0;

	/* bw setting*/
	hact = mtk_crtc->base.state->adjusted_mode.hdisplay;
	vtotal = mtk_crtc->base.state->adjusted_mode.vtotal;
	vact = mtk_crtc->base.state->adjusted_mode.vdisplay;
	vrefresh = drm_mode_vrefresh(&mtk_crtc->base.state->adjusted_mode);
	bw_base = div_u64((unsigned long long)vact * hact * vrefresh * bpp, 1000);
	bw_base = div_u64(bw_base, 1000) * 2;
	mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_SET_HRT_BW, &bw_base);

	//cmdq_pkt_write(handle, comp->cmdq_base,
		//comp->larb_cons[0], GENMASK(19, 16), GENMASK(19, 16));

	mtk_ddp_write_relaxed(comp, addr, DISP_GDMA_MEM_ADDR, handle);
	mtk_ddp_write_relaxed(comp, mem_len, DISP_GDMA_MEM_LENGTH, handle);

	DDPBIF("%s,addr:0x%lx,cfg(%d,%d),roi(%u,%u),mem_len:%u,larb_cons:0x%pa\n", __func__,
			(unsigned long)addr, cfg->w, cfg->h, width, height, mem_len, comp->larb_cons);

}
static int mtk_disp_gdma_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			  enum mtk_ddp_io_cmd cmd, void *params)
{
	int ret = 0;
	struct mtk_drm_private *priv =
		comp->mtk_crtc->base.dev->dev_private;

	switch (cmd) {
	case PMQOS_SET_HRT_BW:
	case PMQOS_SET_HRT_BW_DELAY:
	{
		u32 bw_val = *(unsigned int *)params;
		u32 stash_bw_val  = 0;

		if (!priv) {
			DDPPR_ERR("%s priv error\n", __func__);
			break;
		}

		if (!mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_MMQOS_SUPPORT))
			break;

		__mtk_disp_set_module_hrt(comp->hrt_qos_req, comp->id, bw_val,
			priv->data->respective_ostdl);

		if (!IS_ERR(comp->stash_qos_req)) {
			if (bw_val) {
				stash_bw_val = bw_val / 256;

				stash_bw_val = stash_bw_val > 17 ? stash_bw_val : 17; //set low bound
			}
			__mtk_disp_set_module_hrt(comp->stash_qos_req, comp->id, stash_bw_val,
				priv->data->respective_ostdl);
		}
		ret = GDMA_REQ_HRT;
	}
		break;
	case PMQOS_GET_LARB_PORT_HRT_BW:
	{
		struct mtk_larb_port_bw *data = (struct mtk_larb_port_bw *)params;
		struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
		struct drm_crtc *crtc = &mtk_crtc->base;
		unsigned int bw_base = data->bw_base;

		if (!mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_MMQOS_SUPPORT))
			break;

		data->larb_id = -1;
		data->bw = 0;
		if (data->type != CHANNEL_HRT_READ)
			break;

		if (IS_ERR_OR_NULL(comp->larb_ids))
			data->larb_id = comp->larb_id;
		else
			data->larb_id = comp->larb_ids[0];

		if (data->larb_id < 0)
			break;

		if (!data->bw_base)
			bw_base = mtk_drm_primary_frame_bw(crtc);

		data->bw = bw_base;
		DDPQOS("%s, gdma comp:%d, larb:%d, type:%d, bw:%d\n",
			__func__, comp->id, data->larb_id, data->type, data->bw);
	}
		break;
	default:
		break;
	}

	return ret;
}
static irqreturn_t mtk_disp_gdma_irq_handler(int irq, void *dev_id)
{
	struct mtk_disp_gdma *priv = dev_id;
	struct mtk_ddp_comp *gdma = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	unsigned int val = 0;
	unsigned int ret = 0;

	if (IS_ERR_OR_NULL(priv))
		return IRQ_NONE;

	gdma = &priv->ddp_comp;
	if (IS_ERR_OR_NULL(gdma))
		return IRQ_NONE;

	if (mtk_drm_top_clk_isr_get(gdma) == false) {
		DDPIRQ("%s, top clk off\n", __func__);
		return IRQ_NONE;
	}

	val = readl(gdma->regs + DISP_REG_GDMA_INT_STATUS);
	if (!val) {
		ret = IRQ_NONE;
		goto out;
	}

	DRM_MMP_MARK(gdma, gdma->regs_pa, val);

	mtk_crtc = gdma->mtk_crtc;

	DDPIRQ("%s irq, val:0x%x\n", mtk_dump_comp_str(gdma), val);

	writel(~val, gdma->regs + DISP_REG_GDMA_INT_STATUS);

	if (val & GDMA_START)
		DDPIRQ("[IRQ] %s: GDMA_START!\n", mtk_dump_comp_str(gdma));

	if (val & RDMA_FME_UND)
		DDPPR_ERR("[IRQ] %s, error, underrun\n", mtk_dump_comp_str(gdma));

	ret = IRQ_HANDLED;

out:
	mtk_drm_top_clk_isr_put(gdma);

	return ret;
}

static void mtk_disp_gdma_prepare(struct mtk_ddp_comp *comp)
{
	DDPFUNC();
	mtk_ddp_comp_clk_prepare(comp);
}
static void mtk_disp_gdma_unprepare(struct mtk_ddp_comp *comp)
{
	DDPFUNC();
	mtk_ddp_comp_clk_unprepare(comp);
}
static const struct mtk_ddp_comp_funcs mtk_disp_gdma_funcs = {
	.start = mtk_disp_gdma_start,
	.stop = mtk_disp_gdma_stop,
	.prepare = mtk_disp_gdma_prepare,
	.unprepare = mtk_disp_gdma_unprepare,
	.config = mtk_disp_gdma_config,
	.io_cmd = mtk_disp_gdma_io_cmd,
	.bif_read_config = mtk_disp_gdma_bif_read_config,
};
static int mtk_disp_gdma_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct mtk_disp_gdma *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	DDPFUNC("+");
	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}
	DDPFUNC("-");
	return 0;
}
static void mtk_disp_gdma_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_disp_gdma *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}
static const struct component_ops mtk_disp_gdma_component_ops = {
	.bind = mtk_disp_gdma_bind, .unbind = mtk_disp_gdma_unbind,
};
static int mtk_disp_gdma_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_gdma *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret, irq;

	DDPFUNC("+");
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_GDMA);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_gdma_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	ret = devm_request_irq(dev, irq, mtk_disp_gdma_irq_handler,
		IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(dev), priv);

	if (ret < 0) {
		DDPAEE("%s:%d, failed to request irq:%d ret:%d comp_id:%d\n",
		__func__, __LINE__,irq, ret, comp_id);
		return ret;
	}

	platform_set_drvdata(pdev, priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_gdma_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}
	DDPFUNC("-");

	return ret;
}
static void mtk_disp_gdma_remove(struct platform_device *pdev)
{
	struct mtk_disp_gdma *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_gdma_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);
}
static const struct of_device_id mtk_disp_gdma_driver_dt_match[] = {
	{.compatible = "mediatek,mt6993-disp-gdma", },
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_gdma_driver_dt_match);

struct platform_driver mtk_disp_gdma_driver = {
	.probe = mtk_disp_gdma_probe,
	.remove = mtk_disp_gdma_remove,
	.driver = {

			.name = "mediatek-disp-gdma",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_gdma_driver_dt_match,
		},
};
