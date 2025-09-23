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
#include "mtk_drm_ddp_comp_auto.h"
#include "mtk_dump.h"
#include "mtk_drm_drv.h"


#define DISP_REG_VDISP_AO_INTEN				(0x000UL)
	#define CPU_INTEN				BIT(0)
	#define SCP_INTEN				BIT(1)
	#define CPU_INT_MERGE			BIT(4)
#define DISP_REG_VDISP_AO_MERGED_IRQB_STS	(0x0004)
#define DISP_REG_VDISP_AO_SELECTED_IRQB_STS	(0x0008)
#define DISP_REG_VDISP_AO_TOP_IRQB_STS		(0x000C)
#define DISP_REG_VDISP_AO_SHADOW_CTRL		(0x0010)
	#define FORCE_COMMIT			BIT(0)
	#define BYPASS_SHADOW			BIT(1)
	#define READ_WRK_REG			BIT(2)
#define DISP_REG_VDISP_AO_INT_SEL_G0_MT6991		(0x0020)
	#define CPU_INTSEL_BIT_00		REG_FLD_MSB_LSB(7, 0)	//GIC:450
	#define CPU_INTSEL_BIT_01		REG_FLD_MSB_LSB(15, 8)
	#define CPU_INTSEL_BIT_02		REG_FLD_MSB_LSB(23, 16)
	#define CPU_INTSEL_BIT_03		REG_FLD_MSB_LSB(31, 24)
#define DISP_REG_VDISP_AO_INT_SEL_G1_MT6991		(0x0024)
	#define CPU_INTSEL_BIT_04		REG_FLD_MSB_LSB(7, 0)	//GIC:454
	#define CPU_INTSEL_BIT_05		REG_FLD_MSB_LSB(15, 8)
	#define CPU_INTSEL_BIT_06		REG_FLD_MSB_LSB(23, 16)
	#define CPU_INTSEL_BIT_07		REG_FLD_MSB_LSB(31, 24)
#define DISP_REG_VDISP_AO_INT_SEL_G2_MT6991		(0x0028)
	#define CPU_INTSEL_BIT_08		REG_FLD_MSB_LSB(7, 0)	//GIC:458
	#define CPU_INTSEL_BIT_09		REG_FLD_MSB_LSB(15, 8)
	#define CPU_INTSEL_BIT_10		REG_FLD_MSB_LSB(23, 16)
	#define CPU_INTSEL_BIT_11		REG_FLD_MSB_LSB(31, 24)
#define DISP_REG_VDISP_AO_INT_SEL_G3_MT6991		(0x002C)
	#define CPU_INTSEL_BIT_12		REG_FLD_MSB_LSB(7, 0)	//GIC:462
	#define CPU_INTSEL_BIT_13		REG_FLD_MSB_LSB(15, 8)
	#define CPU_INTSEL_BIT_14		REG_FLD_MSB_LSB(23, 16)
	#define CPU_INTSEL_BIT_15		REG_FLD_MSB_LSB(31, 24)
#define DISP_REG_VDISP_AO_INT_SEL_G4_MT6991		(0x0030)
	#define CPU_INTSEL_BIT_16		REG_FLD_MSB_LSB(7, 0)	//GIC:466
	#define CPU_INTSEL_BIT_17		REG_FLD_MSB_LSB(15, 8)
	#define CPU_INTSEL_BIT_18		REG_FLD_MSB_LSB(23, 16)
	#define CPU_INTSEL_BIT_19		REG_FLD_MSB_LSB(31, 24)
#define DISP_REG_VDISP_AO_INT_SEL_G5_MT6991		(0x0034)
	#define CPU_INTSEL_BIT_20		REG_FLD_MSB_LSB(7, 0)	//GIC:470
	#define CPU_INTSEL_BIT_21		REG_FLD_MSB_LSB(15, 8)
	#define CPU_INTSEL_BIT_22		REG_FLD_MSB_LSB(23, 16)
	#define CPU_INTSEL_BIT_23		REG_FLD_MSB_LSB(31, 24)
#define DISP_REG_VDISP_AO_INT_SEL_G6_MT6991		(0x0038)
	#define CPU_INTSEL_BIT_24		REG_FLD_MSB_LSB(7, 0)	//GIC:474
	#define CPU_INTSEL_BIT_25		REG_FLD_MSB_LSB(15, 8)

//#define VDISP_AO_SELECTED_DISP_DSI0				  BIT(0)	// 8'd53
//#define VDISP_AO_SELECTED_DISP1_MUTEX0			  BIT(1)	// 8'd41
//#define VDISP_AO_SELECTED_OVL0_EXDMA2			  BIT(2)	// 8'd148

#define	IRQ_TABLE_DISP_DSI0_MT6991			(0x35)	//450
#define	IRQ_TABLE_DISP_DISP1_MUTEX0_MT6991	(0x29)	//451
#if !IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_GUEST)
#define	IRQ_TABLE_DISP_OVL0_EXDMA2_MT6991	(0x94)	//452
#endif
#define IRQ_TABLE_DISP_DISP1_WDMA1_MT6991	(0x45)  //453
#define IRQ_TABLE_DISP_OVL1_WDMA0_MT6991	(0xDE)  //454
#define	IRQ_TABLE_DISP_DSI1_MT6991		(0x36)	//455
#define	IRQ_TABLE_DISP_DSI2_MT6991		(0x37)	//456
#define	IRQ_TABLE_DISP_Y2R_MT6991			(36)	//457

#define	IRQ_TABLE_DISP_MDP_RDMA0			(27)	//458
#define IRQ_TABLE_DISP_OVL_WDMA0_MT6991	    (0xAE)  //459
#define	IRQ_TABLE_DISP_DP_INTF0_MT6991		(43)	//460
#define	IRQ_TABLE_DISP_DVO0_MT6991		(56)	//461

#define	IRQ_TABLE_DISP_DP_INTF1_MT6991		(44)	//464

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_GUEST)
#define	IRQ_TABLE_DISP_OVL0_EXDMA2_MT6991	(148)
#define	IRQ_TABLE_DISP_OVL0_EXDMA3_MT6991	(149)
#define	IRQ_TABLE_DISP_OVL0_EXDMA4_MT6991	(150)
#define	IRQ_TABLE_DISP_OVL0_EXDMA5_MT6991	(151)
#define	IRQ_TABLE_DISP_OVL0_EXDMA6_MT6991	(152)
#define	IRQ_TABLE_DISP_OVL0_EXDMA7_MT6991	(153)
#define	IRQ_TABLE_DISP_OVL0_EXDMA8_MT6991	(154)
#define	IRQ_TABLE_DISP_OVL0_EXDMA9_MT6991	(155)
#define	IRQ_TABLE_DISP_OVL0_INT_EXDMA(n)	(IRQ_TABLE_DISP_OVL0_EXDMA2_MT6991 + (n))
#define OVLSYS0_EXDMA_NUM			(8)

#define	IRQ_TABLE_DISP_OVL1_EXDMA2_MT6991	(196)
#define	IRQ_TABLE_DISP_OVL1_EXDMA3_MT6991	(197)
#define	IRQ_TABLE_DISP_OVL1_EXDMA4_MT6991	(198)
#define	IRQ_TABLE_DISP_OVL1_EXDMA5_MT6991	(199)
#define	IRQ_TABLE_DISP_OVL1_EXDMA6_MT6991	(200)
#define	IRQ_TABLE_DISP_OVL1_EXDMA7_MT6991	(201)
#define	IRQ_TABLE_DISP_OVL1_EXDMA8_MT6991	(202)
#define	IRQ_TABLE_DISP_OVL1_EXDMA9_MT6991	(203)

#define	IRQ_TABLE_DISP_OVL1_INT_EXDMA(n)	(IRQ_TABLE_DISP_OVL1_EXDMA2_MT6991 + (n))
#endif

#define IRQ_TABLE_DISP_DITHER2_MT6991          (39)    //469
#define IRQ_TABLE_DISP_DITHER1_MT6991          (22)    //470
#define IRQ_TABLE_DISP_DITHER0_MT6991          (21)    //471
#define IRQ_TABLE_DISP_CHIST1_MT6991           (18)    //472
#define IRQ_TABLE_DISP_CHIST0_MT6991           (17)    //473
#define IRQ_TABLE_DISP_AAL1_MT6991             (8)     //474
#define IRQ_TABLE_DISP_AAL0_MT6991             (7)     //475

static void __iomem *vdisp_ao_base;

struct mtk_vdisp_ao {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
};

static inline struct mtk_vdisp_ao *comp_to_vdisp_ao(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_vdisp_ao, ddp_comp);
}

static void mtk_vdisp_ao_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{

}

static void mtk_vdisp_ao_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{

}

void mtk_vdisp_ao_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	DDPDUMP("== DISP %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
	/* INTEN, MERGED, SELECTED, TOP */
	mtk_serial_dump_reg(baddr, DISP_REG_VDISP_AO_INTEN, 4);

	mtk_serial_dump_reg(baddr, DISP_REG_VDISP_AO_INT_SEL_G0_MT6991, 4);
	mtk_serial_dump_reg(baddr, DISP_REG_VDISP_AO_INT_SEL_G4_MT6991, 3);

	mtk_serial_dump_reg(baddr, 0x100, 3);
	mtk_serial_dump_reg(baddr, 0x120, 3);
	mtk_serial_dump_reg(baddr, 0x140, 3);

	mtk_serial_dump_reg(baddr, 0x210, 2);
}

void mtk_vdisp_ao_analysis(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	unsigned int reg_val = 0, selectd_sts = 0;

	DDPDUMP("== %s ANALYSIS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
	reg_val = readl(baddr + DISP_REG_VDISP_AO_INTEN);
	selectd_sts = readl(baddr + DISP_REG_VDISP_AO_SELECTED_IRQB_STS);
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
	.start = mtk_vdisp_ao_start,
	.stop = mtk_vdisp_ao_stop,
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
#if !IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_GUEST)
	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_OVL0_EXDMA2_MT6991, CPU_INTSEL_BIT_02);
#endif
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

#if IS_ENABLED(CONFIG_MTK_VIRT_DISP_PATH)
static u32 mtk_vdisp_ao_get_first_exdma_interrupt_id(struct drm_crtc *crtc, u32 *int_id,
						     u32 *output_comp_id)
{
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct mtk_ddp_comp *output_comp = NULL;
	struct mtk_ddp_comp *first_exdma = NULL;
	u32 crtc_id = 0;
	u32 ovlsys_id = 0, exdma_phy_id = 0;

	if (!crtc) {
		DDPMSG("%s %d invalid crtc\n", __func__, __LINE__);
		goto err_ret;
	}

	crtc_id = drm_crtc_index(crtc);
	mtk_crtc = to_mtk_crtc(crtc);
	if (!mtk_crtc) {
		DDPMSG("%s %d invalid mtk_crtc\n", __func__, __LINE__);
		goto err_ret;
	}

	first_exdma = mtk_crtc->first_exdma;
	if (!first_exdma) {
		DDPMSG("%s %d crtc-%d invalid first_exdma\n",
			__func__, __LINE__, crtc_id);
		goto err_ret;
	}

	if (!mtk_ddp_comp_is_rdma(first_exdma)) {
		DDPMSG("%s %d crtc-%d first_exdma %s is not rdma\n",
			__func__, __LINE__, crtc_id, mtk_dump_comp_str(first_exdma));
		goto err_ret;
	}

	if (!first_exdma->funcs || !first_exdma->funcs->ovlsys_mapping ||
	    !first_exdma->funcs->ovl_phy_mapping) {
		DDPMSG("%s %d crtc-%d first_exdma %s missing mapping funcs\n",
			__func__, __LINE__, crtc_id, mtk_dump_comp_str(first_exdma));
		goto err_ret;
	}

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (!output_comp) {
		DDPMSG("%s %d crtc-%d invalid output_comp\n", __func__, __LINE__, crtc_id);
		goto err_ret;
	}
	*output_comp_id = output_comp->id;

	ovlsys_id = first_exdma->funcs->ovlsys_mapping(first_exdma);
	exdma_phy_id = first_exdma->funcs->ovl_phy_mapping(first_exdma);

	if (ovlsys_id == 0) {
		*int_id = IRQ_TABLE_DISP_OVL0_INT_EXDMA(exdma_phy_id);
	} else if (ovlsys_id == 1) {
		if (exdma_phy_id >= OVLSYS0_EXDMA_NUM) {
			exdma_phy_id -= OVLSYS0_EXDMA_NUM;
			*int_id = IRQ_TABLE_DISP_OVL1_INT_EXDMA(exdma_phy_id);
		} else {
			DDPMSG("%s %d crtc-%d ovlsys_id %d exdma_phy_id %d underflow\n",
				__func__, __LINE__, crtc_id, ovlsys_id, exdma_phy_id);
			goto err_ret;
		}
	} else {
		DDPMSG("%s %d crtc-%d ovlsys_id %d underflow\n",
			__func__, __LINE__, crtc_id, ovlsys_id);
		goto err_ret;
	}

	DDPMSG("%s %d crtc-%d first_exdma %s out %s ovlsys %d exdma_phy_id %d int_id %d\n",
		__func__, __LINE__, crtc_id,
		mtk_dump_comp_str(first_exdma), mtk_dump_comp_str(output_comp),
		ovlsys_id, exdma_phy_id, *int_id);

	return crtc_id;

err_ret:
	*int_id = IRQ_TABLE_DISP_OVL0_EXDMA2_MT6991;
	return crtc_id;
}
#endif

static void mtk_vdisp_ao_int_sel_g3_MT6991(struct drm_crtc *crtc)
{
	int value = 0, mask = 0;
	u32 tmp = 0;
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_GUEST)
	u32 int_id = 0, crtc_id;
	u32 output_comp_id = 0;
#endif

	tmp = readl(vdisp_ao_base + DISP_REG_VDISP_AO_INT_SEL_G3_MT6991);

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_GUEST)
	crtc_id = mtk_vdisp_ao_get_first_exdma_interrupt_id(crtc, &int_id, &output_comp_id);

	DDPMSG("%s %d crtc-%d int_id %d 0x%x output %s tmp 0x%x mask 0x%x\n",
		__func__, __LINE__, crtc_id, int_id, int_id,
		mtk_dump_comp_str_id(output_comp_id), tmp, mask);

	/* for dsi0 */
	if (crtc_id == 0)
		SET_VAL_MASK(value, mask, int_id, CPU_INTSEL_BIT_12); /* 462 */
	/* for dp0 */
	else if (crtc_id == 1)
		SET_VAL_MASK(value, mask, int_id, CPU_INTSEL_BIT_13); /* 463 */
	else if ((crtc_id == 3) || (crtc_id == 4))
		SET_VAL_MASK(value, mask, int_id, CPU_INTSEL_BIT_15); /* 465 */

#endif
	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_DP_INTF1_MT6991, CPU_INTSEL_BIT_14); /* 464 */

	value = (tmp & ~mask) | (value & mask);

	writel(value, vdisp_ao_base + DISP_REG_VDISP_AO_INT_SEL_G3_MT6991);

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_GUEST)
	DDPMSG("%s %d crtc-%d int_id %d 0x%x output %s value 0x%x mask 0x%x\n",
		__func__, __LINE__, crtc_id, int_id, int_id,
		mtk_dump_comp_str_id(output_comp_id), value, mask);
#else
	DDPMSG("%s %d value 0x%x\n", __func__, __LINE__, value);
#endif
}

static void mtk_vdisp_ao_int_sel_g4_MT6991(struct drm_crtc *crtc)
{
	int value = 0, mask = 0;
	u32 tmp = 0;
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_GUEST)
	u32 int_id = 0;
	u32 output_comp_id = 0;
	u32 crtc_id = 0;
#endif

	tmp = readl(vdisp_ao_base + DISP_REG_VDISP_AO_INT_SEL_G4_MT6991);

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_GUEST)
	crtc_id = mtk_vdisp_ao_get_first_exdma_interrupt_id(crtc, &int_id, &output_comp_id);

	/* for dsi2_1 */
	if (crtc_id == 5)
		SET_VAL_MASK(value, mask, int_id, CPU_INTSEL_BIT_16); /* 466 */
#endif
	SET_VAL_MASK(value, mask, IRQ_TABLE_DISP_DITHER2_MT6991, CPU_INTSEL_BIT_19);

	value = (tmp & ~mask) | (value & mask);

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

void mtk_vdisp_ao_irq_config_MT6991(struct drm_crtc *crtc)
{
	DDPINFO("%s:%d\n", __func__, __LINE__);
	writel(1, vdisp_ao_base + DISP_REG_VDISP_AO_INTEN);	// disable merge irq
	mtk_vdisp_ao_int_sel_g0_MT6991();
	mtk_vdisp_ao_int_sel_g1_MT6991();
	mtk_vdisp_ao_int_sel_g2_MT6991();
	mtk_vdisp_ao_int_sel_g3_MT6991(crtc);
	mtk_vdisp_ao_int_sel_g4_MT6991(crtc);
	mtk_vdisp_ao_int_sel_g5_MT6991();
	mtk_vdisp_ao_int_sel_g6_MT6991();
}

static int mtk_vdisp_ao_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_vdisp_ao *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	DDPFUNC("+");
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_VDISP_AO);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_vdisp_ao_funcs);
	vdisp_ao_base = priv->ddp_comp.regs;
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, priv);

	writel(1, vdisp_ao_base + DISP_REG_VDISP_AO_INTEN);	// disable merge irq

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_vdisp_ao_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}
	DDPFUNC("-");

	return ret;
}

static int mtk_vdisp_ao_remove(struct platform_device *pdev)
{
	struct mtk_vdisp_ao *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_vdisp_ao_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

static const struct of_device_id mtk_vdisp_ao_driver_dt_match[] = {
	{.compatible = "mediatek,mt6991-vdisp-ao", },
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
