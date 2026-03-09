// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_dump.h"

#define DISP_REG_SPLITTER_CTL 0x00
#define SPLITTER_ENABLE BIT(0)
#define SPLITTER_OUT1_DIS BIT(12)
#define SPLITTER_CONFIG_OUT_TH BIT(16)

#define DISP_REG_SPLITTER_SRC_SIZE 0x04
#define DISP_REG_SPLITTER_OUT0_OFFSET 0x10
#define DISP_REG_SPLITTER_OUT0_SIZE 0x14
#define DISP_REG_SPLITTER_OUT1_OFFSET 0x18
#define DISP_REG_SPLITTER_OUT1_SIZE 0x1C

#define DISP_REG_SPLITTER_SHADOW_CTL 0x4C
#define REG_SPLITTER_FORCE_COMMIT REG_FLD_MSB_LSB(0, 0)
#define REG_SPLITTER_BYPASS_SHADOW REG_FLD_MSB_LSB(1, 1)

struct mtk_disp_splitter {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
};

static void mtk_splitter_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPFUNC();

	if (!comp) {
		DDPPR_ERR("find comp fail\n");
		return;
	}
	mtk_ddp_write_mask(comp, 0x1, DISP_REG_SPLITTER_CTL, 0x1,  handle);
}

static void mtk_splitter_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPFUNC();

	if (!comp) {
		DDPPR_ERR("find comp fail\n");
		return;
	}
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_SPLITTER_CTL, 0x0, 0x1);
}
void mtk_splitter_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	int i;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}
	DDPDUMP("== DISP %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);

	for (i = 0x0; i < 0x60; i += 0x10)
		mtk_cust_dump_reg(baddr, i, i + 0x4, i + 0x8, i + 0xc);
}

static void
mtk_splitter_config(struct mtk_ddp_comp *comp, struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	DDPFUNC();

	cmdq_pkt_write(handle, comp->cmdq_base,	comp->regs_pa + DISP_REG_SPLITTER_SHADOW_CTL,
		       0x1, 0x1);
	cmdq_pkt_write(handle, comp->cmdq_base,	comp->regs_pa + DISP_REG_SPLITTER_SRC_SIZE,
		       cfg->w | (cfg->h << 16), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,	comp->regs_pa + DISP_REG_SPLITTER_OUT0_OFFSET,
		       0, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,	comp->regs_pa + DISP_REG_SPLITTER_OUT0_SIZE,
		       (cfg->w / 2) | (cfg->h << 16), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,	comp->regs_pa + DISP_REG_SPLITTER_OUT1_OFFSET,
		       cfg->w / 2, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,	comp->regs_pa + DISP_REG_SPLITTER_OUT1_SIZE,
		       (cfg->w / 2) | (cfg->h << 16), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,	comp->regs_pa + DISP_REG_SPLITTER_CTL,
		       0x10000, ~0);
}
static void
mtk_splitter_bif_write_config(struct mtk_ddp_comp *comp, struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	int width = cfg->w;
	int height = cfg->h;
	int value = 0;
	int offset = comp->mtk_crtc->bif_info->wdma_offset;

	DDPINFO("%s,comp:%s,(%dx%d)offset:%d\n", __func__, mtk_dump_comp_str(comp), width, height, offset);

	value = SPLITTER_ENABLE | SPLITTER_CONFIG_OUT_TH;
	mtk_ddp_write_mask(comp, value, DISP_REG_SPLITTER_CTL, value,  handle);

	value = (height << 16) | width;
	mtk_ddp_write_relaxed(comp, value, DISP_REG_SPLITTER_SRC_SIZE, handle);
	mtk_ddp_write_relaxed(comp, value, DISP_REG_SPLITTER_OUT0_SIZE, handle);
	mtk_ddp_write_relaxed(comp, 0, DISP_REG_SPLITTER_OUT0_OFFSET, handle);
	value = width - offset;
	mtk_ddp_write_relaxed(comp, value, DISP_REG_SPLITTER_OUT1_OFFSET, handle);
	value = (height << 16) | offset;
	mtk_ddp_write_relaxed(comp, value, DISP_REG_SPLITTER_OUT1_SIZE, handle);
}
static void
mtk_splitter_bif_read_config(struct mtk_ddp_comp *comp, struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	int width = cfg->w/3;
	int height = cfg->h;
	int offset = comp->mtk_crtc->bif_info->wdma_offset;

	DDPINFO("%s,comp:%s,(%dx%d)offset:%d\n", __func__, mtk_dump_comp_str(comp), width, height, offset);

	mtk_ddp_write_mask(comp,SPLITTER_ENABLE | SPLITTER_OUT1_DIS, DISP_REG_SPLITTER_CTL,
			SPLITTER_ENABLE | SPLITTER_OUT1_DIS,  handle);
	mtk_ddp_write_relaxed(comp, (height << 16) | (width + offset), DISP_REG_SPLITTER_SRC_SIZE, handle);
	mtk_ddp_write_relaxed(comp, 0, DISP_REG_SPLITTER_OUT0_OFFSET, handle);
	mtk_ddp_write_relaxed(comp, (height << 16) | width, DISP_REG_SPLITTER_OUT0_SIZE, handle);
}

static void mtk_splitter_prepare(struct mtk_ddp_comp *comp)
{
	DDPFUNC();
	mtk_ddp_comp_clk_prepare(comp);
}

static void mtk_splitter_unprepare(struct mtk_ddp_comp *comp)
{
	DDPFUNC();
	mtk_ddp_comp_clk_unprepare(comp);
}

static const struct mtk_ddp_comp_funcs mtk_disp_splitter_funcs = {
	.start = mtk_splitter_start,
	.stop = mtk_splitter_stop,
	.config = mtk_splitter_config,
	.bif_write_config = mtk_splitter_bif_write_config,
	.bif_read_config = mtk_splitter_bif_read_config,
	.prepare = mtk_splitter_prepare,
	.unprepare = mtk_splitter_unprepare,
};

static int mtk_disp_splitter_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct mtk_disp_splitter *priv = dev_get_drvdata(dev);
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

static void mtk_disp_splitter_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_disp_splitter *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_splitter_component_ops = {
	.bind = mtk_disp_splitter_bind, .unbind = mtk_disp_splitter_unbind,
};

static int mtk_disp_splitter_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_splitter *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	DDPFUNC("+");
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_SPLITTER);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_splitter_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_splitter_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}
	DDPFUNC("-");

	return ret;
}

static int mtk_disp_splitter_remove(struct platform_device *pdev)
{
	struct mtk_disp_splitter *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_splitter_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

static const struct of_device_id mtk_disp_splitter_driver_dt_match[] = {
	{.compatible = "mediatek,mt6991-disp-splitter", },
	{.compatible = "mediatek,mt6899-disp-splitter", },
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_splitter_driver_dt_match);

struct platform_driver mtk_disp_splitter_driver = {
	.probe = mtk_disp_splitter_probe,
	.remove = mtk_disp_splitter_remove,
	.driver = {

			.name = "mediatek-disp-splitter",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_splitter_driver_dt_match,
		},
};
