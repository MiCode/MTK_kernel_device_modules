// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/ratelimit.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_fb.h"
#include "mtk_drm_drv.h"


#define RELAY5_WIDTH_FLD		REG_FLD_MSB_LSB(13, 0)
#define RELAY5_HEIGHT_FLD		REG_FLD_MSB_LSB(29, 16)

struct mtk_disp_relay_data {
	bool support_shadow;
	bool need_bypass_shadow;
};

struct mtk_disp_relay {
	struct mtk_ddp_comp	 ddp_comp;
	const struct mtk_disp_relay_data *data;
	uint32_t size_addr;
	uint32_t status_addr;
};

static inline struct mtk_disp_relay *comp_to_relay(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_relay, ddp_comp);
}

static void mtk_relay_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPINFO("%s start\n", mtk_dump_comp_str(comp));
}

static void mtk_relay_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPINFO("%s stop\n", mtk_dump_comp_str(comp));
}

static void mtk_relay_prepare(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s prepare\n", mtk_dump_comp_str(comp));

	mtk_ddp_comp_clk_prepare(comp);

	/* Bypass shadow register and read shadow register */
}

static void mtk_relay_unprepare(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s unprepare\n", mtk_dump_comp_str(comp));

	mtk_ddp_comp_clk_unprepare(comp);
}

static void mtk_relay_config(struct mtk_ddp_comp *comp,
				 struct mtk_ddp_config *cfg,
				 struct cmdq_pkt *handle)
{
	struct mtk_disp_relay *relay_data = comp_to_relay(comp);
	int value = 0, mask = 0;

	DDPINFO("%s config\n", mtk_dump_comp_str(comp));
	SET_VAL_MASK(value, mask, cfg->w, RELAY5_WIDTH_FLD);
	SET_VAL_MASK(value, mask, cfg->h, RELAY5_HEIGHT_FLD);
	if (relay_data->size_addr)
		cmdq_pkt_write(handle, NULL, comp->regs_pa + relay_data->size_addr, value, mask);
}

static const struct mtk_ddp_comp_funcs mtk_disp_relay_funcs = {
	.config = mtk_relay_config,
	.start = mtk_relay_start,
	.stop = mtk_relay_stop,
	.prepare = mtk_relay_prepare,
	.unprepare = mtk_relay_unprepare,
};

static int mtk_disp_relay_bind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_disp_relay *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	return 0;
}

static void mtk_disp_relay_unbind(struct device *dev, struct device *master,
				 void *data)
{
	struct mtk_disp_relay *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_relay_component_ops = {
	.bind = mtk_disp_relay_bind,
	.unbind = mtk_disp_relay_unbind,
};

static int mtk_disp_relay_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_relay *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	DDPMSG("%s+\n", __func__);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_RELAY);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_relay_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	if (of_property_read_u32(dev->of_node, "size-addr", &priv->size_addr)) {
		dev_err(dev, "fail to get size-addr\n");
		priv->size_addr = 0;
	}
	if (of_property_read_u32(dev->of_node, "status-addr", &priv->status_addr)) {
		dev_err(dev, "fail to get status-addr\n");
		priv->status_addr = 0;
	}

	priv->data = of_device_get_match_data(dev);
	platform_set_drvdata(pdev, priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_relay_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	DDPMSG("%s-\n", __func__);
	return ret;
}

static void mtk_disp_relay_remove(struct platform_device *pdev)
{
	struct mtk_disp_relay *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_relay_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);
}

static const struct mtk_disp_relay_data mt6993_relay_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct of_device_id mtk_disp_relay_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6993-disp-relay",
	  .data = &mt6993_relay_driver_data},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_disp_relay_driver_dt_match);

struct platform_driver mtk_disp_relay_driver = {
	.probe = mtk_disp_relay_probe,
	.remove = mtk_disp_relay_remove,
	.driver = {
		.name = "mediatek-disp-relay",
		.owner = THIS_MODULE,
		.of_match_table = mtk_disp_relay_driver_dt_match,
	},
};
