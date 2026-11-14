// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

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

#define DISP_REG_OVL_DL_OUT_RELAY0_SIZE 0x26C

#define DISP_REG_RELAY_WIDTH	REG_FLD_MSB_LSB(13, 0)
#define DISP_REG_RELAY_HEIGHT	REG_FLD_MSB_LSB(29, 16)

enum disp_ovl_dlo_register {
	OVL_DL_OUT_RELAY0_SIZE,
	OVL_DL_OUT_RELAY1_SIZE,
	OVL_DLO_ASYNC0_STATUS0,
	OVL_DLO_ASYNC1_STATUS0,
	DISP_DLO_ASYNC0_SIZE,
	DISP_DLO_ASYNC0_STATUS0,
	DISP_DLO_ASYNC1_SIZE,
	DISP_DLO_ASYNC1_STATUS0,
	DISP_DLO_ASYNC16_SIZE,
	DISP_DLO_ASYNC16_STATUS0,
	DISP_DLO_ASYNC17_SIZE,
	DISP_DLO_ASYNC17_STATUS0,
	DISP_DLO_ASYNC31_SIZE,
	DISP_DLO_ASYNC31_STATUS0,
	OVL_DLO_REG_TOTAL
};

/* mt6991 */
static const u16 ovl_dlo_regs_mt6991[OVL_DLO_REG_TOTAL] = {
	[OVL_DL_OUT_RELAY0_SIZE]	= 0x288,
	[DISP_DLO_ASYNC0_SIZE]		= 0x264,
};

/* mt6993 */
static const u16 ovl_dlo_regs_mt6993[OVL_DLO_REG_TOTAL] = {
	[OVL_DL_OUT_RELAY0_SIZE]	= 0xc00,
	[OVL_DL_OUT_RELAY1_SIZE]	= 0xc08,
	[OVL_DLO_ASYNC0_STATUS0]	= 0xb8c,
	[OVL_DLO_ASYNC1_STATUS0]	= 0xb90,
	[DISP_DLO_ASYNC0_SIZE]		= 0xbd0,
	[DISP_DLO_ASYNC0_STATUS0]	= 0xb3c,
	[DISP_DLO_ASYNC1_SIZE]		= 0xbd4,
	[DISP_DLO_ASYNC1_STATUS0]	= 0xb40,
	[DISP_DLO_ASYNC16_SIZE]		= 0xc10,
	[DISP_DLO_ASYNC16_STATUS0]	= 0xb7c,
	[DISP_DLO_ASYNC17_SIZE]		= 0xc14,
	[DISP_DLO_ASYNC17_STATUS0]	= 0xb80,
	[DISP_DLO_ASYNC31_SIZE]		= 0xbe4,
	[DISP_DLO_ASYNC31_STATUS0]	= 0xb78,
};

struct dlo_async_data {
	const struct mtk_ddp_comp_funcs *funcs;
	const u16 *regs;
};

/**
 * struct mtk_disp_dlo_async - DISP_RSZ driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 */
struct mtk_disp_dlo_async {
	struct mtk_ddp_comp ddp_comp;
	const struct dlo_async_data *data;
};

static inline struct mtk_disp_dlo_async *comp_to_dlo_async(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_dlo_async, ddp_comp);
}

static void mtk_dlo_async_addon_config(struct mtk_ddp_comp *comp,
				 enum mtk_ddp_comp_id prev,
				 enum mtk_ddp_comp_id next,
				 union mtk_addon_config *addon_config,
				 struct cmdq_pkt *handle)
{
	DDPDBG("%s+\n", __func__);

	if (!addon_config)
		return;

	if ((addon_config->config_type.module == DISP_MML_IR_PQ_v2) ||
	    (addon_config->config_type.module == DISP_MML_IR_PQ_v2_1)) {
		u8 pipe = addon_config->addon_mml_config.pipe;

		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_OVL_DL_OUT_RELAY0_SIZE,
			       (addon_config->addon_mml_config.mml_src_roi[pipe].width |
				addon_config->addon_mml_config.mml_src_roi[pipe].height << 16),
			       ~0);
	}
}

void mtk_dlo_async_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}
	DDPDUMP("== DISP %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
	DDPDUMP("0x0F0: 0x%08x\n", readl(baddr + 0x0F0));
	DDPDUMP("0x27C: 0x%08x\n", readl(baddr + 0x27C));
	DDPDUMP("0x2A8: 0x%08x 0x%08x\n", readl(baddr + 0x2A8),
		readl(baddr + 0x2AC));
}

int mtk_dlo_async_analysis(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s\n", __func__);
	return 0;
}

static void mtk_dlo_async_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPDBG("%s+\n", __func__);
	// nothig to do
}

static void mtk_dlo_async_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPDBG("%s+\n", __func__);
	// nothig to do
}

static void mtk_dlo_async_prepare(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s\n", __func__);
	mtk_ddp_comp_clk_prepare(comp);
}

static void mtk_dlo_async_size_config(struct mtk_ddp_comp *comp, struct mtk_ddp_config *cfg,
		       struct cmdq_pkt *handle)
{
	struct mtk_disp_dlo_async *dlo = comp_to_dlo_async(comp);

	u32 dlo_in_relay_size, alias_id;
	unsigned int value = 0, mask = 0;

	alias_id = mtk_ddp_comp_get_alias(comp->id);

	switch(mtk_ddp_comp_get_type(comp->id)) {
	case MTK_OVL_0_DLO_ASYNC:
	case MTK_OVL_1_DLO_ASYNC:
	case MTK_OVL_2_DLO_ASYNC:
		dlo_in_relay_size = dlo->data->regs[OVL_DL_OUT_RELAY0_SIZE] + alias_id * 4;
		break;
	case MTK_DISP_DLO_ASYNC:
	case MTK_DISP_B_DLO_ASYNC:
		if (alias_id < 31)
			dlo_in_relay_size = dlo->data->regs[DISP_DLO_ASYNC0_SIZE] + alias_id * 4;
		else
			dlo_in_relay_size = dlo->data->regs[DISP_DLO_ASYNC31_SIZE] +
									((alias_id - 31) * 4);
		break;
	default:
		DDPMSG("Not dlo async module\n");
		return;
	}

	SET_VAL_MASK(value, mask, cfg->w, DISP_REG_RELAY_WIDTH);
	SET_VAL_MASK(value, mask, cfg->h, DISP_REG_RELAY_HEIGHT);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + dlo_in_relay_size,
					value, mask);

	DDPINFO("%s comp->id %d alias_id %d value 0x%x\n", __func__, comp->id, alias_id, value);
}

static void mtk_dlo_async_unprepare(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s\n", __func__);
	mtk_ddp_comp_clk_unprepare(comp);
}

static const struct mtk_ddp_comp_funcs mtk_disp_dlo_async_funcs = {
	.start = mtk_dlo_async_start,
	.stop = mtk_dlo_async_stop,
	.addon_config = mtk_dlo_async_addon_config,
	.prepare = mtk_dlo_async_prepare,
	.unprepare = mtk_dlo_async_unprepare,
	.config = mtk_dlo_async_size_config,
};

static const struct dlo_async_data dlo_data_mt6991 = {
	.funcs = &mtk_disp_dlo_async_funcs,
	.regs = ovl_dlo_regs_mt6991,
};

static const struct dlo_async_data dlo_data_mt6993 = {
	.funcs = &mtk_disp_dlo_async_funcs,
	.regs = ovl_dlo_regs_mt6993,
};

static int mtk_disp_dlo_async_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct mtk_disp_dlo_async *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	DDPINFO("%s &priv->ddp_comp:0x%lx\n", __func__, (unsigned long)&priv->ddp_comp);
	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	return 0;
}

static void mtk_disp_dlo_async_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_disp_dlo_async *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	DDPINFO("%s\n", __func__);
	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_dlo_async_component_ops = {
	.bind = mtk_disp_dlo_async_bind, .unbind = mtk_disp_dlo_async_unbind,
};

static int mtk_disp_dlo_async_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_dlo_async *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret;
	const struct dlo_async_data *dlo_data;

	DDPINFO("%s+\n", __func__);
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	if (of_device_is_compatible(dev->of_node, "mediatek,ovl0_dlo_async"))
		comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_OVL_0_DLO_ASYNC);
	else if (of_device_is_compatible(dev->of_node, "mediatek,ovl1_dlo_async"))
		comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_OVL_1_DLO_ASYNC);
	else if (of_device_is_compatible(dev->of_node, "mediatek,ovl2_dlo_async"))
		comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_OVL_2_DLO_ASYNC);
	else if (of_device_is_compatible(dev->of_node, "mediatek,dlo_async"))
		comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_DLO_ASYNC);
	else if (of_device_is_compatible(dev->of_node, "mediatek,dlo_async_b"))
		comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_B_DLO_ASYNC);
	else
		comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_DLO_ASYNC);

	DDPMSG("comp_id:%d", comp_id);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	dlo_data = (const struct dlo_async_data *)of_device_get_match_data(dev);
	if (!dlo_data)
		dlo_data = &dlo_data_mt6991;
	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id, dlo_data->funcs);

	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	priv->data = dlo_data;

	//priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	DDPINFO("&priv->ddp_comp:0x%lx", (unsigned long)&priv->ddp_comp);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_dlo_async_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	DDPINFO("%s-\n", __func__);

	return ret;
}

static void mtk_disp_dlo_async_remove(struct platform_device *pdev)
{
	struct mtk_disp_dlo_async *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_dlo_async_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);
}

static const struct of_device_id mtk_disp_dlo_async_driver_dt_match[] = {
	{.compatible = "mediatek,mt6983-disp-dlo-async3",},
	{.compatible = "mediatek,mt6895-disp-dlo-async3",},
	{.compatible = "mediatek,mt6985-disp-dlo-async",},
	{.compatible = "mediatek,mt6989-disp-dlo-async",},
	{.compatible = "mediatek,mt6991-disp-dlo-async",},
	{.compatible = "mediatek,mt6897-disp-dlo-async",},
	{.compatible = "mediatek,mt6886-disp-dlo-async3",},
	{.compatible = "mediatek,mt6991-ovl-0-dlo-async",},
	{.compatible = "mediatek,mt6991-ovl-1-dlo-async",},
	{.compatible = "mediatek,mt6993-ovl-0-dlo-async",
	 .data = (void *)&dlo_data_mt6993,},
	{.compatible = "mediatek,mt6993-ovl-1-dlo-async",
	 .data = (void *)&dlo_data_mt6993,},
	{.compatible = "mediatek,mt6993-ovl-2-dlo-async",
	 .data = (void *)&dlo_data_mt6993,},
	{.compatible = "mediatek,mt6993-disp-dlo-async",
	 .data = (void *)&dlo_data_mt6993,},
	{.compatible = "mediatek,mt6993-disp-b-dlo-async",
	 .data = (void *)&dlo_data_mt6993,},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_dlo_async_driver_dt_match);

struct platform_driver mtk_disp_dlo_async_driver = {
	.probe = mtk_disp_dlo_async_probe,
	.remove = mtk_disp_dlo_async_remove,
	.driver = {
		.name = "mediatek-disp-dlo-async",
		.owner = THIS_MODULE,
		.of_match_table = mtk_disp_dlo_async_driver_dt_match,
	},
};
