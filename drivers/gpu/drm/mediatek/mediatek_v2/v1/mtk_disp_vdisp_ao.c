// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

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

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_drm_drv.h"
#include "mtk_disp_vdisp_ao_reg.h"

/* VDISP_AO_COMMON */
#define DISP_REG_VDISP_AO_INTEN				(0x000UL)
	#define CPU_INTEN				BIT(0)
	#define SCP_INTEN				BIT(1)
	#define CPU_INT_MERGE			BIT(4)
	#define MODULE_INT_MERGE		BIT(5)	// mt6993
#define DISP_REG_VDISP_AO_MERGED_IRQB_STS	(0x0004)
#define DISP_REG_VDISP_AO_SELECTED_IRQB_STS	(0x0008)
#define DISP_REG_VDISP_AO_TOP_IRQB_STS		(0x000C)
#define DISP_REG_VDISP_AO_SHADOW_CTRL		(0x0010)
	#define FORCE_COMMIT			BIT(0)
	#define BYPASS_SHADOW			BIT(1)
	#define READ_WRK_REG			BIT(2)

static void __iomem *vdisp_ao_base;

static struct mtk_vdisp_ao *g_priv;

static const struct vdisp_ao_data vdisp_ao_data_mt6991 = {
	.ao_int_config = 1,		//	CPU_INT:1
};

static const struct vdisp_ao_data vdisp_ao_data_mt6993 = {
	.ao_int_config = 33,	// CPU_INT:1, MODULE_INIT_MODE:1
	.irq_count = 36,
	.irq_cfg = mt6993_irq_cfg,
};

static inline struct mtk_vdisp_ao *comp_to_vdisp_ao(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_vdisp_ao, ddp_comp);
}

static void vdisp_ao_dump_16_qos_info_MT6993(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	unsigned int val = 0;
	int subcom_num = 4;
	int i;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	DDPDUMP("16_lv_qos:(en,normal,preultra,ultra)\n");
	for (i = 0; i < subcom_num; i++) {
		val = readl(baddr + DISP_REG_VDISP_AO_MMQOS_SUBCOM0_MT6993 + i * 4);
		DDPDUMP("subcom%d:(r:%d,%d,%d,%d),(w:%d,%d,%d,%d)\n", i,
			REG_FLD_VAL_GET(FLD_MMQOS_EN_R, val),
			REG_FLD_VAL_GET(FLD_MMQOS_NORMAL_R, val),
			REG_FLD_VAL_GET(FLD_MMQOS_PREULTRA_R, val),
			REG_FLD_VAL_GET(FLD_MMQOS_ULTRA_R, val),
			REG_FLD_VAL_GET(FLD_MMQOS_EN_W, val),
			REG_FLD_VAL_GET(FLD_MMQOS_NORMAL_W, val),
			REG_FLD_VAL_GET(FLD_MMQOS_PREULTRA_W, val),
			REG_FLD_VAL_GET(FLD_MMQOS_ULTRA_W, val));
	}
}

void mtk_vdisp_ao_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_drm_private *priv = NULL;
	int i = 0;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	if (IS_ERR_OR_NULL(mtk_crtc))
		return;

	priv = mtk_crtc->base.dev->dev_private;

	DDPDUMP("== DISP %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
	/* INTEN, MERGED, SELECTED, TOP */
	mtk_serial_dump_reg(baddr, DISP_REG_VDISP_AO_INTEN, 4);

	if (priv->data->mmsys_id == MMSYS_MT6991) {
		mtk_serial_dump_reg(baddr, DISP_REG_VDISP_AO_INT_SEL_G0_MT6991, 4);
		mtk_serial_dump_reg(baddr, DISP_REG_VDISP_AO_INT_SEL_G4_MT6991, 3);

		mtk_serial_dump_reg(baddr, 0x100, 3);
		mtk_serial_dump_reg(baddr, 0x120, 3);
		mtk_serial_dump_reg(baddr, 0x140, 3);

		mtk_serial_dump_reg(baddr, 0x210, 2);
	} else {
		mtk_serial_dump_reg(baddr, 0x14, 4);
		mtk_serial_dump_reg(baddr, 0x24, 4);
		mtk_serial_dump_reg(baddr, 0x34, 4);
		mtk_serial_dump_reg(baddr, 0x44, 4);
		mtk_serial_dump_reg(baddr, 0x54, 4);
	}

	/* 16 lv qos */
	mtk_serial_dump_reg(baddr, 0x700, 4);
}

void mtk_vdisp_ao_analysis(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	unsigned int reg_val = 0, selectd_sts = 0;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_drm_private *priv = NULL;

	DDPDUMP("== %s ANALYSIS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
	reg_val = readl(baddr + DISP_REG_VDISP_AO_INTEN);
	selectd_sts = readl(baddr + DISP_REG_VDISP_AO_SELECTED_IRQB_STS);

	if (IS_ERR_OR_NULL(mtk_crtc))
		return;

	priv = mtk_crtc->base.dev->dev_private;
	if (priv->data->mmsys_id == MMSYS_MT6993)
		vdisp_ao_dump_16_qos_info_MT6993(comp);
}

static void mtk_vdisp_ao_prepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_prepare(comp);
}

static void mtk_vdisp_ao_unprepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_unprepare(comp);
}

static const struct mtk_ddp_comp_funcs mtk_vdisp_ao_funcs = {
	//.start = mtk_vdisp_ao_start,
	//.stop = mtk_vdisp_ao_stop,
	.prepare = mtk_vdisp_ao_prepare,
	.unprepare = mtk_vdisp_ao_unprepare,
};

static int mtk_vdisp_ao_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct mtk_vdisp_ao *priv = dev_get_drvdata(dev);
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

static void mtk_vdisp_ao_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_vdisp_ao *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_vdisp_ao_component_ops = {
	.bind = mtk_vdisp_ao_bind, .unbind = mtk_vdisp_ao_unbind,
};

static void mtk_vdisp_ao_int_sel_g0_MT6991(void)
{
	int value = 0, mask = 0;

	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_DSI0_MT6991, CPU_INTSEL_BIT_00);
	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_DISP1_MUTEX0_MT6991, CPU_INTSEL_BIT_01);
	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_OVL0_BWM0_MT6991, CPU_INTSEL_BIT_02);
	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_DISP1_WDMA1_MT6991, CPU_INTSEL_BIT_03);

	writel(value, vdisp_ao_base + DISP_REG_VDISP_AO_INT_SEL_G0_MT6991);

	DDPINFO("%s,%d,value:0x%x\n", __func__, __LINE__, value);
}

static void mtk_vdisp_ao_int_sel_g1_MT6991(void)
{
	int value = 0, mask = 0;

	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_OVL1_WDMA0_MT6991, CPU_INTSEL_BIT_04);
	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_DSI1_MT6991, CPU_INTSEL_BIT_05);
	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_DSI2_MT6991, CPU_INTSEL_BIT_06);
	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_Y2R_MT6991, CPU_INTSEL_BIT_07);

	writel(value, vdisp_ao_base + DISP_REG_VDISP_AO_INT_SEL_G1_MT6991);

	DDPINFO("%s,%d,value:0x%x\n", __func__, __LINE__, value);
}

static void mtk_vdisp_ao_int_sel_g2_MT6991(void)
{
	int value = 0, mask = 0;

	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_MDP_RDMA0, CPU_INTSEL_BIT_08);
	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_OVL_WDMA0_MT6991, CPU_INTSEL_BIT_09);
	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_DP_INTF0_MT6991, CPU_INTSEL_BIT_10);
	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_DVO0_MT6991, CPU_INTSEL_BIT_11);

	writel(value, vdisp_ao_base + DISP_REG_VDISP_AO_INT_SEL_G2_MT6991);

	DDPINFO("%s,%d,value:0x%x\n", __func__, __LINE__, value);
}

static void mtk_vdisp_ao_int_sel_g3_MT6991(void)
{
	int value = 0, mask = 0;

	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_DISP1_WDMA4_MT6991, CPU_INTSEL_BIT_12);
	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_DP_INTF1_MT6991, CPU_INTSEL_BIT_14);
	SET_VAL_MASK(value, mask, IRQ_TABLE_MDP0_MUTEX0_MT6991, CPU_INTSEL_BIT_15);

	writel(value, vdisp_ao_base + DISP_REG_VDISP_AO_INT_SEL_G3_MT6991);

	DDPINFO("%s,%d,value:0x%x\n", __func__, __LINE__, value);
}

static void mtk_vdisp_ao_int_sel_g4_MT6991(void)
{
	int value = 0, mask = 0;

	SET_VAL_MASK(value, mask, IRQ_TABLE_MDP0_RROT0_MT6991, CPU_INTSEL_BIT_16);
	SET_VAL_MASK(value, mask, IRQ_TABLE_MDP1_MUTEX0_MT6991, CPU_INTSEL_BIT_17);
	SET_VAL_MASK(value, mask, IRQ_TABLE_MDP1_RROT0_MT6991, CPU_INTSEL_BIT_18);

	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_DITHER2_MT6991, CPU_INTSEL_BIT_19);

	writel(value, vdisp_ao_base + DISP_REG_VDISP_AO_INT_SEL_G4_MT6991);

	DDPINFO("%s,%d,value:0x%x\n", __func__, __LINE__, value);
}

static void mtk_vdisp_ao_int_sel_g5_MT6991(void)
{
	int value = 0, mask = 0;

	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_DITHER1_MT6991, CPU_INTSEL_BIT_20);
	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_DITHER0_MT6991, CPU_INTSEL_BIT_21);
	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_CHIST1_MT6991, CPU_INTSEL_BIT_22);
	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_CHIST0_MT6991, CPU_INTSEL_BIT_23);

	writel(value, vdisp_ao_base + DISP_REG_VDISP_AO_INT_SEL_G5_MT6991);

	DDPINFO("%s,%d,value:0x%x\n", __func__, __LINE__, value);
}

static void mtk_vdisp_ao_int_sel_g6_MT6991(void)
{
	int value = 0, mask = 0;

	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_AAL1_MT6991, CPU_INTSEL_BIT_24);
	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_AAL0_MT6991, CPU_INTSEL_BIT_25);

	writel(value, vdisp_ao_base + DISP_REG_VDISP_AO_INT_SEL_G6_MT6991);

	DDPINFO("%s,%d,value:0x%x\n", __func__, __LINE__, value);
}

void mtk_vdisp_ao_irq_config_MT6991(struct drm_device *drm)
{
	DDPINFO("%s:%d\n", __func__, __LINE__);
	writel(1, vdisp_ao_base + DISP_REG_VDISP_AO_INTEN);	// disable merge irq
	mtk_vdisp_ao_int_sel_g0_MT6991();
	mtk_vdisp_ao_int_sel_g1_MT6991();
	mtk_vdisp_ao_int_sel_g2_MT6991();
	mtk_vdisp_ao_int_sel_g3_MT6991();
	mtk_vdisp_ao_int_sel_g4_MT6991();
	mtk_vdisp_ao_int_sel_g5_MT6991();
	mtk_vdisp_ao_int_sel_g6_MT6991();
}
void mtk_vdisp_ao_irq_config_MT6993(struct drm_device *drm)
{
	int i = 0, value = 0, mask = 0;

	if (!g_priv) {
		DDPMSG("%s, g_priv is null\n", __func__);
		return;
	}

	for (i = 0; i < g_priv->data->irq_count; i++) {
		if (g_priv->data->irq_cfg[i].value == 0)
			continue;

		if (g_priv->data->irq_cfg[i].shift == 0) {
			value = 0;
			mask = 0;
			SET_VAL_MASK(value, mask, g_priv->data->irq_cfg[i].value, CPU_INTSEL_BIT_MT6993_L);
		} else
			SET_VAL_MASK(value, mask, g_priv->data->irq_cfg[i].value, CPU_INTSEL_BIT_MT6993_H);

		DDPINFO("%s,%d:offset:0x%x,value:%d\n", __func__, i,
			g_priv->data->irq_cfg[i].offset, g_priv->data->irq_cfg[i].value, value);

		writel(value, vdisp_ao_base + g_priv->data->irq_cfg[i].offset);
	}
}

static int __mtk_vdisp_ao_qos_config_MT6993(bool hrt_read, bool hrt_write)
{
	int value = 0, mask = 0;
	// 16 level qos config, config value see 16 level qos table
	int hrt_normal_lv = 6, hrt_preultra_lv = 6, hrt_ultra_lv = 14;
	int srt_normal_lv = 1, srt_preultra_lv = 9, srt_ultra_lv = 9;
	int normal_lv = srt_normal_lv;
	int preultra_lv = srt_preultra_lv;
	int ultra_lv = srt_ultra_lv;

	SET_VAL_MASK(value, mask, 1, FLD_MMQOS_EN_W);
	SET_VAL_MASK(value, mask, 1, FLD_MMQOS_EN_R);
	if (hrt_read) {
		normal_lv = hrt_normal_lv;
		preultra_lv = hrt_preultra_lv;
		ultra_lv = hrt_ultra_lv;
	}
	SET_VAL_MASK(value, mask, normal_lv, FLD_MMQOS_NORMAL_R);
	SET_VAL_MASK(value, mask, preultra_lv, FLD_MMQOS_PREULTRA_R);
	SET_VAL_MASK(value, mask, ultra_lv, FLD_MMQOS_ULTRA_R);

	normal_lv = srt_normal_lv;
	preultra_lv = srt_preultra_lv;
	ultra_lv = srt_ultra_lv;
	if (hrt_write) {
		normal_lv = hrt_normal_lv;
		preultra_lv = hrt_preultra_lv;
		ultra_lv = hrt_ultra_lv;
	}
	SET_VAL_MASK(value, mask, normal_lv, FLD_MMQOS_NORMAL_W);
	SET_VAL_MASK(value, mask, preultra_lv, FLD_MMQOS_PREULTRA_W);
	SET_VAL_MASK(value, mask, ultra_lv, FLD_MMQOS_ULTRA_W);

	return value;
}

void mtk_vdisp_ao_qos_config_MT6993(struct drm_device *drm)
{
	int hrt_r_hrt_w_value;
	int hrt_r_srt_w_value;
	int value;

	DDPINFO("%s:%d\n", __func__, __LINE__);

	hrt_r_hrt_w_value = __mtk_vdisp_ao_qos_config_MT6993(true, true);
	hrt_r_srt_w_value = __mtk_vdisp_ao_qos_config_MT6993(true, false);

	// 16 level qos config, config value see 16 level qos table
	//cwb need hrt_write
	value = hrt_r_hrt_w_value;
	writel(value, vdisp_ao_base + DISP_REG_VDISP_AO_MMQOS_SUBCOM0_MT6993);
	DDPMSG("set subcom0 hrt_r, hrt_w, value=0x%llx\n", value);

	//only when od on can chg srt_write to hrt_write
	value = hrt_r_srt_w_value;
	writel(value, vdisp_ao_base + DISP_REG_VDISP_AO_MMQOS_SUBCOM1_MT6993);
	DDPMSG("set subcom1 hrt_r, srt_w, value=0x%llx\n", value);

	value = hrt_r_srt_w_value;
	writel(value, vdisp_ao_base + DISP_REG_VDISP_AO_MMQOS_SUBCOM2_MT6993);
	DDPMSG("set subcom2 hrt_r, srt_w, value=0x%llx\n", value);

	//cwb need hrt_write
	value = hrt_r_hrt_w_value;
	writel(value, vdisp_ao_base + DISP_REG_VDISP_AO_MMQOS_SUBCOM3_MT6993);
	DDPMSG("set subcom3 hrt_r, hrt_w, value=0x%llx\n", value);
}

static int mtk_vdisp_ao_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_vdisp_ao *priv;
	enum mtk_ddp_comp_id comp_id;
	const struct vdisp_ao_data *ao_data;
	int ret;

	DDPFUNC("+");
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	g_priv = priv;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_VDISP_AO);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}
	ao_data = (const struct vdisp_ao_data *)of_device_get_match_data(dev);
	if (!ao_data) {
		dev_err(dev, "please assign data\n");
		return -ENOMEM;
	}

	priv->data = ao_data;

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_vdisp_ao_funcs);
	vdisp_ao_base = priv->ddp_comp.regs;
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, priv);

	writel(ao_data->ao_int_config, vdisp_ao_base + DISP_REG_VDISP_AO_INTEN);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_vdisp_ao_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}
	DDPFUNC("-");

	return ret;
}

static void mtk_vdisp_ao_remove(struct platform_device *pdev)
{
	struct mtk_vdisp_ao *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_vdisp_ao_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);
}

static const struct of_device_id mtk_vdisp_ao_driver_dt_match[] = {
	{.compatible = "mediatek,mt6991-vdisp-ao",
	.data = (void *)&vdisp_ao_data_mt6991,},
	{.compatible = "mediatek,mt6993-vdisp-ao",
	.data = (void *)&vdisp_ao_data_mt6993,},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_vdisp_ao_driver_dt_match);

struct platform_driver mtk_vdisp_ao_driver = {
	.probe = mtk_vdisp_ao_probe,
	.remove = mtk_vdisp_ao_remove,
	.driver = {

			.name = "mediatek-vdisp-ao",
			.owner = THIS_MODULE,
			.of_match_table = mtk_vdisp_ao_driver_dt_match,
		},
};
