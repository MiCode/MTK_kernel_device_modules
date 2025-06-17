// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/types.h>
#include <linux/rtc.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_log.h"
#include "mtk_dump.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_gem.h"
#include "mtk_disp_dbgtp.h"
#include "platform/mtk_drm_platform.h"
#include "mtk_disp_vidle.h"
#include "mtk_dsi_lpc.h"
#include "mtk_dsi.h"
#include "mtk-mml-dbgtp.h"
#include "mtk_disp_vdisp_ao.h"

/* Display Debug Top Regs */
//DISP_DEBUG_TOP Base address: (+0x3EE8_0000)
#define DISP_DBG_TOP_EN                    0x00
#define DISP_DBG_TOP_RST                   0x04
#define DISP_DBG_TOP_CON                   0x08
#define DISP_TRIGGER_PRD                   REG_FLD_MSB_LSB(17, 0)
#define DISP_PERIODIC_DUMP_EN              REG_FLD_MSB_LSB(20, 20)
#define DISP_STOP_PERIOD_DUMP              REG_FLD_MSB_LSB(24, 24)
#define DISP_STOP_PERIOD_ONCE              REG_FLD_MSB_LSB(25, 25)

#define DISP_DBG_TOP_CON2                  0x0C
#define DISP_DBG_TOP_SWITCH                0x10
#define DISP_DBG_TOP_TIMEOUT               0x14
#define DISP_DBG_TOP_TIMEOUT_EN            REG_FLD_MSB_LSB(0, 0)
#define DISP_DBG_TOP_TIMEOUT_PRD           REG_FLD_MSB_LSB(20, 20)

#define DISP_DBG_TOP_ATID                  0x18
#define DISP_DBG_TOP_ATB_CFG               0x1C
#define DISP_DBG_TOP_ATB_RG1               REG_FLD_MSB_LSB(11, 10)
#define DISP_DBG_TOP_ATB_RG2               REG_FLD_MSB_LSB(9, 9)
#define DISP_DBG_TOP_ATB_RG3               REG_FLD_MSB_LSB(8, 8)
#define DISP_DBG_TOP_ATB_RG4               REG_FLD_MSB_LSB(6, 2)
#define DISP_DBG_TOP_ATB_RG5               REG_FLD_MSB_LSB(1, 1)
#define DISP_DBG_TOP_ATB_RG6               REG_FLD_MSB_LSB(0, 0)

#define DISP_DBG_FIFO_MON_CFG0             0x20
#define DISP_FIFO_MON_EN                   REG_FLD_MSB_LSB(0, 0)
#define DISP_FIFO_MON_INT_THRESHOLD        REG_FLD_MSB_LSB(6, 1)
#define DISP_FIFO_MON_TRIG_THRESHOLD       REG_FLD_MSB_LSB(12, 7)

#define DISP_DBG_FIFO_MON_CFG1             0x24
#define DISP_DBG_FIFO_MON_CFG2             0x28
#define DISP_DBG_FIFO_MON_CFG3             0x2C

#define DISP_DBG_FIFO_MON_RST              0x30
#define DISP_DBG_FIFO_MON_INTEN            0x34
#define DISP_FIFO_MON3_INT_EN              REG_FLD_MSB_LSB(3, 3)
#define DISP_FIFO_MON2_INT_EN              REG_FLD_MSB_LSB(2, 2)
#define DISP_FIFO_MON1_INT_EN              REG_FLD_MSB_LSB(1, 1)
#define DISP_FIFO_MON0_INT_EN              REG_FLD_MSB_LSB(0, 0)

#define DISP_DBG_FIFO_MON_INTSTA           0x38
#define DISP_DBG_FIFO_MON_INT_CLR          0x3C
#define DISP_FIFO_MON3_INT_CLR             REG_FLD_MSB_LSB(3, 3)
#define DISP_FIFO_MON2_INT_CLR             REG_FLD_MSB_LSB(2, 2)
#define DISP_FIFO_MON1_INT_CLR             REG_FLD_MSB_LSB(1, 1)
#define DISP_FIFO_MON0_INT_CLR             REG_FLD_MSB_LSB(0, 0)

#define DISP_DBG_FIFO_MON_UP_INTEN         0x40
#define DISP_FIFO_MON3_UP_INT_EN           REG_FLD_MSB_LSB(3, 3)
#define DISP_FIFO_MON2_UP_INT_EN           REG_FLD_MSB_LSB(2, 2)
#define DISP_FIFO_MON1_UP_INT_EN           REG_FLD_MSB_LSB(1, 1)
#define DISP_FIFO_MON0_UP_INT_EN           REG_FLD_MSB_LSB(0, 0)

#define DISP_DBG_FIFO_MON_UP_INTSTA        0x44
#define DISP_DBG_FIFO_MON_UP_INT_CLR       0x48
#define DISP_FIFO_MON3_UP_INT_CLR          REG_FLD_MSB_LSB(3, 3)
#define DISP_FIFO_MON2_UP_INT_CLR          REG_FLD_MSB_LSB(2, 2)
#define DISP_FIFO_MON1_UP_INT_CLR          REG_FLD_MSB_LSB(1, 1)
#define DISP_FIFO_MON0_UP_INT_CLR          REG_FLD_MSB_LSB(0, 0)


/* DISPSYS */
//Module name: DISPSYS_CONFIG Base address: (+0x3E300000)
//Module name: DISP0B_DISPSYS_CONFIG Base address: (+0x3E500000)
#define DISPSYS_DEBUG_SUBSYS                              0xA10
#define SUBSYS_SMI_TRIG_EN                                REG_FLD_MSB_LSB(17, 17)
#define SUBSYS_DSI_TRIG_EN                                REG_FLD_MSB_LSB(18, 18)
#define SUBSYS_INLINEROTATE_INFO_EN                       REG_FLD_MSB_LSB(19, 19)
#define SUBSYS_CROSSBAR_INFO_EN                           REG_FLD_MSB_LSB(20, 20)
#define SUBSYS_MON_INFO_EN                                REG_FLD_MSB_LSB(21, 21)
#define SUBSYS_MON_ENGINE_EN                              REG_FLD_MSB_LSB(22, 22)
#define SUBSYS_MON_ENGINE_RST                             REG_FLD_MSB_LSB(23, 23)
//smi
#define DISPSYS_DISP_SMI_DBG_MON_LARB0_CON0               0xA14
#define SMI_MON_SLICE_TIME                                REG_FLD_MSB_LSB(11, 0)
#define SMI_MON_DUMP_SEL                                  REG_FLD_MSB_LSB(17, 16)
#define SMI_MON_RST_BY_FRAME                              REG_FLD_MSB_LSB(20, 20)
#define SMI_MON_ENABLE                                    REG_FLD_MSB_LSB(22, 22)
#define DISPSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR          0xA18
#define DISPSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL           0xA1C
#define SMI_MON_PORT_ID                                   REG_FLD_MSB_LSB(4, 0)
#define SMI_MON_PORT_CG_CTL                               REG_FLD_MSB_LSB(5, 5)
#define SMI_MON_PORT_RST                                  REG_FLD_MSB_LSB(6, 6)
#define DISPSYS_DISP_SMI_DBG_MON_LARB0_MON1_ADDR          0xA20
#define DISPSYS_DISP_SMI_DBG_MON_LARB0_MON1_SEL           0xA24
#define DISPSYS_DISP_SMI_DBG_MON_LARB0_MON2_ADDR          0xA28
#define DISPSYS_DISP_SMI_DBG_MON_LARB0_MON2_SEL           0xA2C
#define DISPSYS_DISP_SMI_DBG_MON_LARB0_MON3_ADDR          0xA30
#define DISPSYS_DISP_SMI_DBG_MON_LARB0_MON3_SEL           0xA34
//crossbar
#define PQ_OUT_CROSSBAR_DEBUG_MONITOR_PTR                 0xEBC
#define PQ_IN_CROSSBAR_DEBUG_MONITOR_PTR                  0xEC0
#define PC_OUT_CROSSBAR_DEBUG_MONITOR_PTR                 0xEC4
#define PC_IN_CROSSBAR_DEBUG_MONITOR_PTR                  0xEC8

//Module name: DISPSYS1_CONFIG Base address: (+0x3E700000)
//Module name: DISP1B_DISPSYS1_CONFIG Base address: (+0x3E900000)
#define DISPSYS1_DEBUG_SUBSYS                            0xA14
#define DISPSYS1_DISP_SMI_DBG_MON_LARB0_CON0             0xA18
#define DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON0_ADDR        0xA1C
#define DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON0_SEL         0xA20
#define DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON1_ADDR        0xA24
#define DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON1_SEL         0xA28
#define DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON2_ADDR        0xA2C
#define DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON2_SEL         0xA30
#define DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON3_ADDR        0xA34
#define DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON3_SEL         0xA38
#define SPLITTER_OUT_CROSSBAR_DEBUG_MONITOR_PTR          0xDB0
#define SPLITTER_IN_CROSSBAR_DEBUG_MONITOR_PTR           0xDB4
#define MERGE_OUT_CROSSBAR_DEBUG_MONITOR_PTR             0xDB8
#define INSIDE_PC_CROSSBAR_DEBUG_MONITOR_PTR             0xDBC
#define COMP_OUT_CROSSBAR_DEBUG_MONITOR_PTR              0xDC0


/* ovlsys */
//Module name: OVLSYS_CONFIG Base address: (+0x32900000)
//Module name: OVL1_OVLSYS_CONFIG Base address: (+0x32C00000)
//Module name: OVL2_OVLSYS_CONFIG Base address: (+0x3E000000)
#define OVLSYS_DEBUG_SUBSYS                                   0xA14
#define OVLSYS_DISP_SMI_DBG_MON_LARB0_CON0                    0xA18
#define OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR               0xA1C
#define OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL                0xA20
#define OVLSYS_DISP_SMI_DBG_MON_LARB0_MON1_ADDR               0xA24
#define OVLSYS_DISP_SMI_DBG_MON_LARB0_MON1_SEL                0xA28
#define OVLSYS_DISP_SMI_DBG_MON_LARB0_MON2_ADDR               0xA2C
#define OVLSYS_DISP_SMI_DBG_MON_LARB0_MON2_SEL                0xA30
#define OVLSYS_DISP_SMI_DBG_MON_LARB0_MON3_ADDR               0xA34
#define OVLSYS_DISP_SMI_DBG_MON_LARB0_MON3_SEL                0xA38
#define OVLSYS_DISP_SMI_DBG_MON_LARB1_CON0                    0xA3C
#define OVLSYS_DISP_SMI_DBG_MON_LARB1_MON0_ADDR               0xA40
#define OVLSYS_DISP_SMI_DBG_MON_LARB1_MON0_SEL                0xA44
#define OVLSYS_DISP_SMI_DBG_MON_LARB1_MON1_ADDR               0xA48
#define OVLSYS_DISP_SMI_DBG_MON_LARB1_MON1_SEL                0xA4C
#define OVLSYS_DISP_SMI_DBG_MON_LARB1_MON2_ADDR               0xA50
#define OVLSYS_DISP_SMI_DBG_MON_LARB1_MON2_SEL                0xA54
#define OVLSYS_DISP_SMI_DBG_MON_LARB1_MON3_ADDR               0xA58
#define OVLSYS_DISP_SMI_DBG_MON_LARB1_MON3_SEL                0xA5C
#define OVLSYS_DISP_SMI_DBG_MON_LARB20_CON0                   0xA60
#define OVLSYS_DISP_SMI_DBG_MON_LARB20_MON0_ADDR              0xA64
#define OVLSYS_DISP_SMI_DBG_MON_LARB20_MON0_SEL               0xA68
#define OVLSYS_DISP_SMI_DBG_MON_LARB20_MON1_ADDR              0xA6C
#define OVLSYS_DISP_SMI_DBG_MON_LARB20_MON1_SEL               0xA70
#define OVLSYS_DISP_SMI_DBG_MON_LARB20_MON2_ADDR              0xA74
#define OVLSYS_DISP_SMI_DBG_MON_LARB20_MON2_SEL               0xA78
#define OVLSYS_DISP_SMI_DBG_MON_LARB20_MON3_ADDR              0xA7C
#define OVLSYS_DISP_SMI_DBG_MON_LARB20_MON3_SEL               0xA80
#define OVLSYS_DISP_SMI_DBG_MON_LARB21_CON0                   0xA84
#define OVLSYS_DISP_SMI_DBG_MON_LARB21_MON0_ADDR              0xA88
#define OVLSYS_DISP_SMI_DBG_MON_LARB21_MON0_SEL               0xA8C
#define OVLSYS_DISP_SMI_DBG_MON_LARB21_MON1_ADDR              0xA90
#define OVLSYS_DISP_SMI_DBG_MON_LARB21_MON1_SEL               0xA94
#define OVLSYS_DISP_SMI_DBG_MON_LARB21_MON2_ADDR              0xA98
#define OVLSYS_DISP_SMI_DBG_MON_LARB21_MON2_SEL               0xA9C
#define OVLSYS_DISP_SMI_DBG_MON_LARB21_MON3_ADDR              0xAA0
#define OVLSYS_DISP_SMI_DBG_MON_LARB21_MON3_SEL               0xAA4
#define OVL_RSZ_IN_CROSSBAR_DEBUG_MONITOR_PTR                 0xE8C
#define OVL_PQ_IN_CROSSBAR_DEBUG_MONITOR_PTR                  0xE90
#define OVL_OUTPROC_OUT_CROSSBAR_DEBUG_MONITOR_PTR            0xE94
#define OVL_EXDMA_OUT_CROSSBAR_DEBUG_MONITOR_PTR              0xE98
#define OVL_BLENDER_OUT_CROSSBAR_DEBUG_MONITOR_PTR            0xE9C


/* mmlsys */
//Module name: MMLSYS_CONFIG Base address: (+0x3200_0000)
//Module name: MML1_MMLSYS_CONFIG Base address: (+0x3230_0000)
//Module name: MML2_MMLSYS_CONFIG Base address: (+0x3260_0000)
#define MMLSYS_DEBUG_SUBSYS                                    0xA0C
#define MMLSYS_DISP_SMI_DBG_MON_LARB0_CON0                     0xA10
#define MMLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR                0xA14
#define MMLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL                 0xA18
#define MMLSYS_DISP_SMI_DBG_MON_LARB0_MON1_ADDR                0xA1C
#define MMLSYS_DISP_SMI_DBG_MON_LARB0_MON1_SEL                 0xA20
#define MMLSYS_DISP_SMI_DBG_MON_LARB0_MON2_ADDR                0xA24
#define MMLSYS_DISP_SMI_DBG_MON_LARB0_MON2_SEL                 0xA28
#define MMLSYS_DISP_SMI_DBG_MON_LARB0_MON3_ADDR                0xA2C
#define MMLSYS_DISP_SMI_DBG_MON_LARB0_MON3_SEL                 0xA30
#define MMLSYS_DISP_SMI_DBG_MON_LARB1_CON0                     0xA34
#define MMLSYS_DISP_SMI_DBG_MON_LARB1_MON0_ADDR                0xA38
#define MMLSYS_DISP_SMI_DBG_MON_LARB1_MON0_SEL                 0xA3C
#define MMLSYS_DISP_SMI_DBG_MON_LARB1_MON1_ADDR                0xA40
#define MMLSYS_DISP_SMI_DBG_MON_LARB1_MON1_SEL                 0xA44
#define MMLSYS_DISP_SMI_DBG_MON_LARB1_MON2_ADDR                0xA48
#define MMLSYS_DISP_SMI_DBG_MON_LARB1_MON2_SEL                 0xA4C
#define MMLSYS_DISP_SMI_DBG_MON_LARB1_MON3_ADDR                0xA50
#define MMLSYS_DISP_SMI_DBG_MON_LARB1_MON3_SEL                 0xA54
#define MML_PQ_OUT_CROSSBAR_DEBUG_MONITOR_PTR                  0xF5C
#define MML_PQ_IN_CROSSBAR_DEBUG_MONITOR_PTR                   0xF60


/* DSI mon config */
#define DSI_MON_EN                               REG_FLD_MSB_LSB(0, 0)
#define DSI_MON_RST_BYF                          REG_FLD_MSB_LSB(4, 4)
#define DSI_MON_SEL_DBG                          REG_FLD_MSB_LSB(15, 0)
#define DSI_MON_SEL_BUF                          REG_FLD_MSB_LSB(31, 16)
#define DSI_MON_TGT_PIX                          REG_FLD_MSB_LSB(14, 0)

struct mtk_disp_dbgtp_data {
	bool is_support_34bits;
	bool need_bypass_shadow;
	unsigned int mminfra_funnel_addr;
	unsigned int trace_top_funnel_addr;
};

struct mtk_disp_dbgtp {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	unsigned int underflow_cnt;
	unsigned int abnormal_cnt;
	void __iomem *mminfra_funnel;
	void __iomem *trace_top_funnel;
	const struct mtk_disp_dbgtp_data *data;
};


struct mtk_ddp_comp *dbgtp_comp;
struct mtk_drm_private *private_ptr;

static inline struct mtk_disp_dbgtp *comp_to_dbgtp(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_dbgtp, ddp_comp);
}

/* For CAM ELA affect DISP HRT fail no DISP ELA */
void mtk_dbgtp_set_mminfra_funnel(bool en)
{
	struct mtk_disp_dbgtp *priv = comp_to_dbgtp(dbgtp_comp);

	if (priv && priv->mminfra_funnel) {
		DDPMSG("%s: %s mminfra funnel\n", __func__, en ? "enalbe" : "disable");
		if (en)
			writel(0xFF, priv->mminfra_funnel);
		else
			writel(0x0, priv->mminfra_funnel);
	}
}

void mtk_dbgtp_set_trace_top_funnel(bool en)
{
	struct mtk_disp_dbgtp *priv = comp_to_dbgtp(dbgtp_comp);
	unsigned int tmp  = 0;
	static unsigned int funnel_port;

	if (priv && priv->trace_top_funnel) {
		DDPMSG("%s: %s trace top funnel\n", __func__, en ? "enalbe" : "disable");
		if (en)
			writel(funnel_port, priv->trace_top_funnel);
		else {
			tmp = readl(priv->trace_top_funnel);
			if (tmp) {
				funnel_port = readl(priv->trace_top_funnel);
				writel(0x0, priv->trace_top_funnel);
			}
		}
	}
}

void mtk_dbgtp_dump_mminfra_funnel(void)
{
	struct mtk_disp_dbgtp *priv = comp_to_dbgtp(dbgtp_comp);

	if (priv && priv->mminfra_funnel)
		DDPMSG("%s: 0x%08x\n" , __func__, readl(priv->mminfra_funnel));
}

void mtk_dbgtp_update(struct mtk_drm_private *priv)
{
	int i = 0;

	if (priv->mtk_dbgtp_sta.dbgtp_en) {
		for (i = 0; i < DISPSYS_NUM; i++) {
			if (priv->mtk_dbgtp_sta.dispsys[i].subsys_mon_en) {
				priv->mtk_dbgtp_sta.dispsys[i].need_update = true;
				DDPDBG("%s: dispsys%d need update\n", __func__, i);
			}
		}

		for (i = 0; i < OVLSYS_NUM; i++) {
			if (priv->mtk_dbgtp_sta.ovlsys[i].subsys_mon_en) {
				priv->mtk_dbgtp_sta.ovlsys[i].need_update = true;
				DDPDBG("%s: ovlsys%d need update\n", __func__, i);
			}
		}

		for (i = 0; i < MMLSYS_NUM; i++) {
			if (priv->mtk_dbgtp_sta.mmlsys[i].subsys_mon_en) {
				priv->mtk_dbgtp_sta.mmlsys[i].need_update = true;
				DDPDBG("%s: mmlsys%d need update\n", __func__, i);
			}
		}

		priv->mtk_dbgtp_sta.need_update = true;
	}
}

/* Just for mt6993*/
void mtk_dbgtp_dsi_gce_event_config(struct mtk_drm_crtc *mtk_crtc, struct cmdq_pkt *cmdq_handle)
{
	struct mtk_ddp_comp *output_comp = NULL;

	output_comp =
		mtk_ddp_comp_request_output(mtk_crtc);
	mtk_ddp_comp_io_cmd(output_comp, cmdq_handle, DSI_GCE_EVENT_CFG, NULL);
}

/* Just for mt6993*/
void mtk_dbgtp_fifo_mon_config(struct mtk_drm_crtc *mtk_crtc, struct cmdq_pkt *cmdq_handle)
{
	unsigned int value = 0;
	unsigned int mask = 0;
	unsigned int i = 0;
	struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;
	struct mtk_ddp_comp *output_comp = NULL;

	if (cmdq_handle == NULL) {
		writel(0, dbgtp_comp->regs + DISP_DBG_FIFO_MON_CFG0);
		DDPDBG("%s:%d disable fifo mon when leave hs idle\n", __func__, __LINE__);
		return;
	}

	output_comp =
		mtk_ddp_comp_request_output(mtk_crtc);
	mtk_ddp_comp_io_cmd(output_comp, cmdq_handle, DSI_GCE_EVENT_CFG, NULL);

	for (i = 0; i < FIFO_MON_NUM; i++) {
		/* dbg top fifo mon0 */
		value = (REG_FLD_VAL((DISP_FIFO_MON_EN), priv->mtk_dbgtp_sta.fifo_mon_en[i]));
		mask = REG_FLD_MASK(DISP_FIFO_MON_EN);
		/*DDPMSG("%s:%d value:%x mask:%x\n", __func__, __LINE__, value, mask);*/
		cmdq_pkt_write(cmdq_handle, dbgtp_comp->cmdq_base,
			dbgtp_comp->regs_pa + DISP_DBG_FIFO_MON_CFG0 + i * 0x4, value, mask);

		/* dbg top fifo mon0 tigger threshold : dsi ungent high */
		value = (REG_FLD_VAL((DISP_FIFO_MON_TRIG_THRESHOLD), 0x0) |
			REG_FLD_VAL((DISP_FIFO_MON_INT_THRESHOLD), 0x0));
		mask = REG_FLD_MASK(DISP_FIFO_MON_TRIG_THRESHOLD) |
		REG_FLD_MASK(DISP_FIFO_MON_INT_THRESHOLD);
		/*DDPMSG("%s:%d value:%x mask:%x\n", __func__, __LINE__, value, mask);*/
		cmdq_pkt_write(cmdq_handle, dbgtp_comp->cmdq_base,
			dbgtp_comp->regs_pa + DISP_DBG_FIFO_MON_CFG0 + i * 0x4, value, mask);
	}

	value = (REG_FLD_VAL((DISP_FIFO_MON0_INT_EN), priv->mtk_dbgtp_sta.fifo_mon_en[0]) |
			REG_FLD_VAL((DISP_FIFO_MON1_INT_EN), priv->mtk_dbgtp_sta.fifo_mon_en[1]) |
			REG_FLD_VAL((DISP_FIFO_MON2_INT_EN), priv->mtk_dbgtp_sta.fifo_mon_en[2]) |
			REG_FLD_VAL((DISP_FIFO_MON3_INT_EN), priv->mtk_dbgtp_sta.fifo_mon_en[3]));
	cmdq_pkt_write(cmdq_handle, dbgtp_comp->cmdq_base,
			dbgtp_comp->regs_pa + DISP_DBG_FIFO_MON_INTEN, value, ~0x0);
}

/* Just for mt6993*/
void mtk_dbgtp_fifo_mon_set_trig_threshold(struct mtk_drm_crtc *mtk_crtc, struct cmdq_pkt *cmdq_handle)
{
	unsigned int value = 0;
	unsigned int mask = 0;
	unsigned int i = 0;
	struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;

	for (i = 0; i < FIFO_MON_NUM; i++) {
		/* dbg top fifo mon0 */
		value = (REG_FLD_VAL((DISP_FIFO_MON_EN), priv->mtk_dbgtp_sta.fifo_mon_en[i]));
		mask = REG_FLD_MASK(DISP_FIFO_MON_EN);
		cmdq_pkt_write(cmdq_handle, dbgtp_comp->cmdq_base,
			dbgtp_comp->regs_pa + DISP_DBG_FIFO_MON_CFG0 + i * 0x4, value, mask);

		/* dbg top fifo mon0 tigger threshold : dsi ungent high */
		value = (REG_FLD_VAL((DISP_FIFO_MON_TRIG_THRESHOLD),
			priv->mtk_dbgtp_sta.fifo_mon_trig_thrd[i]) |
			REG_FLD_VAL((DISP_FIFO_MON_INT_THRESHOLD),
			priv->mtk_dbgtp_sta.fifo_mon_trig_thrd[i]));
		mask = REG_FLD_MASK(DISP_FIFO_MON_TRIG_THRESHOLD) |
		REG_FLD_MASK(DISP_FIFO_MON_INT_THRESHOLD);
		cmdq_pkt_write(cmdq_handle, dbgtp_comp->cmdq_base,
			dbgtp_comp->regs_pa + DISP_DBG_FIFO_MON_CFG0 + i * 0x4, value, mask);
	}

	value = (REG_FLD_VAL((DISP_FIFO_MON0_INT_EN), priv->mtk_dbgtp_sta.fifo_mon_en[0]) |
			REG_FLD_VAL((DISP_FIFO_MON1_INT_EN), priv->mtk_dbgtp_sta.fifo_mon_en[1]) |
			REG_FLD_VAL((DISP_FIFO_MON2_INT_EN), priv->mtk_dbgtp_sta.fifo_mon_en[2]) |
			REG_FLD_VAL((DISP_FIFO_MON3_INT_EN), priv->mtk_dbgtp_sta.fifo_mon_en[3]));
	cmdq_pkt_write(cmdq_handle, dbgtp_comp->cmdq_base,
			dbgtp_comp->regs_pa + DISP_DBG_FIFO_MON_INTEN, value, ~0x0);
}

void mtk_dbgtp_all_setting_dump(struct mtk_drm_private *priv)
{
	unsigned int i = 0;
	unsigned int j = 0;
	unsigned int k = 0;

	DDPMSG("================ DBGTP SETTING ================\n");
	DDPMSG("dbgtp en: %d  dbgtp_switch:0x%x\n",
		priv->mtk_dbgtp_sta.dbgtp_en,
		priv->mtk_dbgtp_sta.dbgtp_switch);
	DDPMSG("dbgtp_prd_trig_en: %d  dbgtp_trig_prd:%d\n",
		priv->mtk_dbgtp_sta.dbgtp_prd_trig_en,
		priv->mtk_dbgtp_sta.dbgtp_trig_prd);
	DDPMSG("dbgtp_timeout_en: %d  dbgtp_timeout_prd:%d\n",
		priv->mtk_dbgtp_sta.dbgtp_timeout_en,
		priv->mtk_dbgtp_sta.dbgtp_timeout_prd);
	DDPMSG("dsi_lpc_mon_en: %d dbgtp_dpc_mon_cfg:0x%x\n",
			priv->mtk_dbgtp_sta.dsi_lpc_mon_en,
			priv->mtk_dbgtp_sta.dbgtp_dpc_mon_cfg);
	DDPMSG("fifo_mon_en: %d,%d,%d,%d fifo_mon_threshold:%d,%d,%d,%d\n",
		priv->mtk_dbgtp_sta.fifo_mon_en[0],
		priv->mtk_dbgtp_sta.fifo_mon_en[1],
		priv->mtk_dbgtp_sta.fifo_mon_en[2],
		priv->mtk_dbgtp_sta.fifo_mon_en[3],
		priv->mtk_dbgtp_sta.fifo_mon_trig_thrd[0],
		priv->mtk_dbgtp_sta.fifo_mon_trig_thrd[1],
		priv->mtk_dbgtp_sta.fifo_mon_trig_thrd[2],
		priv->mtk_dbgtp_sta.fifo_mon_trig_thrd[3]);
	DDPMSG("fifo_mon_sel: %d disp_bwr_sel:%d\n",
		priv->mtk_dbgtp_sta.fifo_mon_sel,
		priv->mtk_dbgtp_sta.disp_bwr_sel);
	DDPMSG("validation mode: %d\n",
		priv->mtk_dbgtp_sta.is_validation_mode);

	DDPMSG(">>>>>>>>>>>>>>>> dispsys   <<<<<<<<<<<<<<<<\n");
	for (i = 0; i < DISPSYS_NUM; i++) {
		DDPMSG("-------- dispsys%d 0A,1A,0B,1B --------\n", i);
		DDPMSG("subsys_mon_en: %d  subsys_smi_trig_en:0x%x\n",
			priv->mtk_dbgtp_sta.dispsys[i].subsys_mon_en,
			priv->mtk_dbgtp_sta.dispsys[i].subsys_smi_trig_en);
		DDPMSG("subsys_dsi_trig_en: %d  subsys_crossbar_info_en:0x%x\n",
			priv->mtk_dbgtp_sta.dispsys[i].subsys_dsi_trig_en,
			priv->mtk_dbgtp_sta.dispsys[i].subsys_crossbar_info_en);
		DDPMSG("subsys_inlinerotate_info_en: %d  subsys_mon_info_en:0x%x\n",
			priv->mtk_dbgtp_sta.dispsys[i].subsys_inlinerotate_info_en,
			priv->mtk_dbgtp_sta.dispsys[i].subsys_mon_info_en);
		DDPMSG("crossbar_mon_cfg0:0x%x\n",
			priv->mtk_dbgtp_sta.dispsys[i].crossbar_mon_cfg0);
		DDPMSG("crossbar_mon_cfg1: %d  crossbar_mon_cfg2:0x%x\n",
			priv->mtk_dbgtp_sta.dispsys[i].crossbar_mon_cfg1,
			priv->mtk_dbgtp_sta.dispsys[i].crossbar_mon_cfg2);
		DDPMSG("crossbar_mon_cfg3: %d  crossbar_mon_cfg4:0x%x\n",
			priv->mtk_dbgtp_sta.dispsys[i].crossbar_mon_cfg3,
			priv->mtk_dbgtp_sta.dispsys[i].crossbar_mon_cfg4);
		if ((i == 1) || (i == 3)) {
			DDPMSG("dsi_mon_en:%d\n",
			priv->mtk_dbgtp_sta.dispsys[i].dsi_mon.dsi_mon_en);
			DDPMSG("dsi_mon_reset_byf:%d  dsi_mon_sel:%d\n",
			priv->mtk_dbgtp_sta.dispsys[i].dsi_mon.dsi_mon_reset_byf,
			priv->mtk_dbgtp_sta.dispsys[i].dsi_mon.dsi_mon_sel);
			DDPMSG("dsi_buf_sel:%d  dsi_tgt_pix:%d\n",
			priv->mtk_dbgtp_sta.dispsys[i].dsi_mon.dsi_buf_sel,
			priv->mtk_dbgtp_sta.dispsys[i].dsi_mon.dsi_tgt_pix);
		}

		for (j = 0; j < MAX_SMI_MON_NUM; j++) {
			DDPMSG("smi%d: smi_mon_en:%d rst_by_frame:%d slice_time:%x dump_sel:%d\n",
				j, priv->mtk_dbgtp_sta.dispsys[i].smi_mon[j].smi_mon_en,
				priv->mtk_dbgtp_sta.dispsys[i].smi_mon[j].rst_by_frame,
				priv->mtk_dbgtp_sta.dispsys[i].smi_mon[j].slice_time,
				priv->mtk_dbgtp_sta.dispsys[i].smi_mon[j].smi_mon_dump_sel);
			for (k = 0; k < SMI_MON_PORT_NUM; k++)
				DDPMSG("smi%d:port%d: addr:%x portid:%d cg_ctl:%d\n", j, k,
					priv->mtk_dbgtp_sta.dispsys[i].smi_mon[j].smi_mon_addr[k],
					priv->mtk_dbgtp_sta.dispsys[i].smi_mon[j].smi_mon_portid[k],
					priv->mtk_dbgtp_sta.dispsys[i].smi_mon[j].smi_mon_cg_ctl[k]);
		}
	}

	DDPMSG(">>>>>>>>>>>>>>>> ovlsys   <<<<<<<<<<<<<<<<\n");
	for (i = 0; i < OVLSYS_NUM; i++) {
		DDPMSG("-------- ovlsys%d --------\n", i);
		DDPMSG("subsys_mon_en: %d  subsys_smi_trig_en:0x%x\n",
			priv->mtk_dbgtp_sta.ovlsys[i].subsys_mon_en,
			priv->mtk_dbgtp_sta.ovlsys[i].subsys_smi_trig_en);
		DDPMSG("subsys_dsi_trig_en: %d  subsys_crossbar_info_en:0x%x\n",
			priv->mtk_dbgtp_sta.ovlsys[i].subsys_dsi_trig_en,
			priv->mtk_dbgtp_sta.ovlsys[i].subsys_crossbar_info_en);
		DDPMSG("subsys_inlinerotate_info_en: %d  subsys_mon_info_en:0x%x\n",
			priv->mtk_dbgtp_sta.ovlsys[i].subsys_inlinerotate_info_en,
			priv->mtk_dbgtp_sta.ovlsys[i].subsys_mon_info_en);
		DDPMSG("crossbar_mon_cfg0:0x%x\n",
			priv->mtk_dbgtp_sta.ovlsys[i].crossbar_mon_cfg0);
		DDPMSG("crossbar_mon_cfg1: %d  crossbar_mon_cfg2:0x%x\n",
			priv->mtk_dbgtp_sta.ovlsys[i].crossbar_mon_cfg1,
			priv->mtk_dbgtp_sta.ovlsys[i].crossbar_mon_cfg2);
		DDPMSG("crossbar_mon_cfg3: %d  crossbar_mon_cfg4:0x%x\n",
			priv->mtk_dbgtp_sta.ovlsys[i].crossbar_mon_cfg3,
			priv->mtk_dbgtp_sta.ovlsys[i].crossbar_mon_cfg4);

		for (j = 0; j < MAX_SMI_MON_NUM; j++) {
			DDPMSG("smi%d: smi_mon_en:%d rst_by_frame:%d slice_time:%x dump_sel:%d\n",
				j, priv->mtk_dbgtp_sta.ovlsys[i].smi_mon[j].smi_mon_en,
				priv->mtk_dbgtp_sta.ovlsys[i].smi_mon[j].rst_by_frame,
				priv->mtk_dbgtp_sta.ovlsys[i].smi_mon[j].slice_time,
				priv->mtk_dbgtp_sta.ovlsys[i].smi_mon[j].smi_mon_dump_sel);
			for (k = 0; k < SMI_MON_PORT_NUM; k++)
				DDPMSG("smi%d:port%d: addr:%x portid:%d cg_ctl:%d\n", j, k,
					priv->mtk_dbgtp_sta.ovlsys[i].smi_mon[j].smi_mon_addr[k],
					priv->mtk_dbgtp_sta.ovlsys[i].smi_mon[j].smi_mon_portid[k],
					priv->mtk_dbgtp_sta.ovlsys[i].smi_mon[j].smi_mon_cg_ctl[k]);
		}
	}

	DDPMSG(">>>>>>>>>>>>>>>> mmlsys   <<<<<<<<<<<<<<<<\n");
	for (i = 0; i < MMLSYS_NUM; i++) {
		DDPMSG("-------- mmlsys%d --------\n", i);
		DDPMSG("subsys_mon_en: %d  subsys_smi_trig_en:0x%x\n",
			priv->mtk_dbgtp_sta.mmlsys[i].subsys_mon_en,
			priv->mtk_dbgtp_sta.mmlsys[i].subsys_smi_trig_en);
		DDPMSG("subsys_dsi_trig_en: %d  subsys_crossbar_info_en:0x%x\n",
			priv->mtk_dbgtp_sta.mmlsys[i].subsys_dsi_trig_en,
			priv->mtk_dbgtp_sta.mmlsys[i].subsys_crossbar_info_en);
		DDPMSG("subsys_inlinerotate_info_en: %d  subsys_mon_info_en:0x%x\n",
			priv->mtk_dbgtp_sta.mmlsys[i].subsys_inlinerotate_info_en,
			priv->mtk_dbgtp_sta.mmlsys[i].subsys_mon_info_en);
		DDPMSG("crossbar_mon_cfg0:0x%x\n",
			priv->mtk_dbgtp_sta.mmlsys[i].crossbar_mon_cfg0);
		DDPMSG("crossbar_mon_cfg1: %d  crossbar_mon_cfg2:0x%x\n",
			priv->mtk_dbgtp_sta.mmlsys[i].crossbar_mon_cfg1,
			priv->mtk_dbgtp_sta.mmlsys[i].crossbar_mon_cfg2);
		DDPMSG("crossbar_mon_cfg3: %d  crossbar_mon_cfg4:0x%x\n",
			priv->mtk_dbgtp_sta.mmlsys[i].crossbar_mon_cfg3,
			priv->mtk_dbgtp_sta.mmlsys[i].crossbar_mon_cfg4);
		for (j = 0; j < MAX_SMI_MON_NUM; j++) {
			DDPMSG("smi%d: smi_mon_en:%d rst_by_frame:%d slice_time:%x dump_sel:%d\n",
				j, priv->mtk_dbgtp_sta.mmlsys[i].smi_mon[j].smi_mon_en,
				priv->mtk_dbgtp_sta.mmlsys[i].smi_mon[j].rst_by_frame,
				priv->mtk_dbgtp_sta.mmlsys[i].smi_mon[j].slice_time,
				priv->mtk_dbgtp_sta.mmlsys[i].smi_mon[j].smi_mon_dump_sel);
			for (k = 0; k < SMI_MON_PORT_NUM; k++)
				DDPMSG("smi%d:port%d: addr:%x portid:%d cg_ctl:%d\n", j, k,
					priv->mtk_dbgtp_sta.mmlsys[i].smi_mon[j].smi_mon_addr[k],
					priv->mtk_dbgtp_sta.mmlsys[i].smi_mon[j].smi_mon_portid[k],
					priv->mtk_dbgtp_sta.mmlsys[i].smi_mon[j].smi_mon_cg_ctl[k]);
		}
	}
	DDPMSG("================ DEBUG TOP END ============\n");

	mtk_dbgtp_all_regs_dump(priv);
}

int mtk_dbgtp_dump(void)
{
	void __iomem *baddr = NULL;

	if (!dbgtp_comp) {
		DDPPR_ERR("%s, %s is NULL!\n", __func__, "dbgtp comp");
		return 0;
	}

	baddr = dbgtp_comp->regs;
	if (!baddr) {
		DDPPR_ERR("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(dbgtp_comp));
		return 0;
	}

	if (!dbgtp_comp->regs_pa) {
		DDPPR_ERR("%s, %s is NULL!\n", __func__, "dbgtp regs_pa");
		return 0;
	}

	DDPINFO("== %s REGS:0x%pa ==\n", mtk_dump_comp_str(dbgtp_comp), &dbgtp_comp->regs_pa);
	DDPINFO("dbgtp: 0x%x : 0x%x 0x%x 0x%x 0x%x\n", 0x0, readl(baddr + 0x0),
		readl(baddr + 0x4), readl(baddr + 0x8), readl(baddr + 0xC));
	DDPINFO("dbgtp: 0x%x : 0x%x 0x%x 0x%x 0x%x\n", 0x10, readl(baddr + 0x10),
		readl(baddr + 0x14), readl(baddr + 0x18), readl(baddr + 0x1C));
	DDPINFO("dbgtp: 0x%x : 0x%x 0x%x 0x%x 0x%x\n", 0x20, readl(baddr + 0x20),
		readl(baddr + 0x24), readl(baddr + 0x28), readl(baddr + 0x2C));
	DDPINFO("dbgtp: 0x%x : 0x%x 0x%x 0x%x 0x%x\n", 0x30, readl(baddr + 0x30),
		readl(baddr + 0x34), readl(baddr + 0x38), readl(baddr + 0x3C));
	DDPINFO("dbgtp: 0x%x : 0x%x 0x%x 0x%x 0x%x\n", 0x40, readl(baddr + 0x40),
		readl(baddr + 0x44), readl(baddr + 0x48), readl(baddr + 0x4C));

	return 0;
}

void mtk_dbgtp_all_regs_dump(struct mtk_drm_private *priv)
{
	void __iomem *regs = NULL;

	mtk_dbgtp_dump();
	DDPDUMP(">>>>>>>>>>>>>>>> dispsys   <<<<<<<<<<<<<<<<\n");
	regs = priv->config_regs;
	if (regs != NULL) {
		DDPDUMP("dispsys0a: 0x%x : 0x%x\n", DISPSYS_DEBUG_SUBSYS,
			readl(regs + DISPSYS_DEBUG_SUBSYS));
		DDPDUMP("dispsys0a: 0x%x : 0x%x\n", DISPSYS_DISP_SMI_DBG_MON_LARB0_CON0,
			readl(regs + DISPSYS_DISP_SMI_DBG_MON_LARB0_CON0));
		DDPDUMP("dispsys0a: 0x%x : 0x%x\n", DISPSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR,
			readl(regs + DISPSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR));
		DDPDUMP("dispsys0a: 0x%x : 0x%x\n", DISPSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL,
			readl(regs + DISPSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL));
		DDPDUMP("dispsys0a: 0x%x : 0x%x\n", DISPSYS_DISP_SMI_DBG_MON_LARB0_MON1_ADDR,
			readl(regs + DISPSYS_DISP_SMI_DBG_MON_LARB0_MON1_ADDR));
		DDPDUMP("dispsys0a: 0x%x : 0x%x\n", DISPSYS_DISP_SMI_DBG_MON_LARB0_MON1_SEL,
			readl(regs + DISPSYS_DISP_SMI_DBG_MON_LARB0_MON1_SEL));
		DDPDUMP("dispsys0a: 0x%x : 0x%x\n", DISPSYS_DISP_SMI_DBG_MON_LARB0_MON2_ADDR,
			readl(regs + DISPSYS_DISP_SMI_DBG_MON_LARB0_MON2_ADDR));
		DDPDUMP("dispsys0a: 0x%x : 0x%x\n", DISPSYS_DISP_SMI_DBG_MON_LARB0_MON2_SEL,
			readl(regs + DISPSYS_DISP_SMI_DBG_MON_LARB0_MON2_SEL));
		DDPDUMP("dispsys0a: 0x%x : 0x%x\n", DISPSYS_DISP_SMI_DBG_MON_LARB0_MON3_ADDR,
			readl(regs + DISPSYS_DISP_SMI_DBG_MON_LARB0_MON3_ADDR));
		DDPDUMP("dispsys0a: 0x%x : 0x%x\n", DISPSYS_DISP_SMI_DBG_MON_LARB0_MON3_SEL,
			readl(regs + DISPSYS_DISP_SMI_DBG_MON_LARB0_MON3_SEL));
		DDPDUMP("dispsys0a: 0x%x : 0x%x\n", PQ_OUT_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + PQ_OUT_CROSSBAR_DEBUG_MONITOR_PTR));
		DDPDUMP("dispsys0a: 0x%x : 0x%x\n", PQ_IN_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + PQ_IN_CROSSBAR_DEBUG_MONITOR_PTR));
		DDPDUMP("dispsys0a: 0x%x : 0x%x\n", PC_OUT_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + PC_OUT_CROSSBAR_DEBUG_MONITOR_PTR));
		DDPDUMP("dispsys0a: 0x%x : 0x%x\n", PC_IN_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + PC_IN_CROSSBAR_DEBUG_MONITOR_PTR));
	}
	regs = priv->side_config_regs;
	if (regs != NULL) {
		DDPDUMP("dispsys1a: 0x%x : 0x%x\n", DISPSYS1_DEBUG_SUBSYS,
			readl(regs + DISPSYS1_DEBUG_SUBSYS));
		DDPDUMP("dispsys1a: 0x%x : 0x%x\n", DISPSYS1_DISP_SMI_DBG_MON_LARB0_CON0,
			readl(regs + DISPSYS1_DISP_SMI_DBG_MON_LARB0_CON0));
		DDPDUMP("dispsys1a: 0x%x : 0x%x\n", DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON0_ADDR,
			readl(regs + DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON0_ADDR));
		DDPDUMP("dispsys1a: 0x%x : 0x%x\n", DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON0_SEL,
			readl(regs + DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON0_SEL));
		DDPDUMP("dispsys1a: 0x%x : 0x%x\n", DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON1_ADDR,
			readl(regs + DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON1_ADDR));
		DDPDUMP("dispsys1a: 0x%x : 0x%x\n", DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON1_SEL,
			readl(regs + DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON1_SEL));
		DDPDUMP("dispsys1a: 0x%x : 0x%x\n", DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON2_ADDR,
			readl(regs + DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON2_ADDR));
		DDPDUMP("dispsys1a: 0x%x : 0x%x\n", DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON2_SEL,
			readl(regs + DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON2_SEL));
		DDPDUMP("dispsys1a: 0x%x : 0x%x\n", DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON3_ADDR,
			readl(regs + DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON3_ADDR));
		DDPDUMP("dispsys1a: 0x%x : 0x%x\n", DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON3_SEL,
			readl(regs + DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON3_SEL));
		DDPDUMP("dispsys1a: 0x%x : 0x%x\n", SPLITTER_OUT_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + SPLITTER_OUT_CROSSBAR_DEBUG_MONITOR_PTR));
		DDPDUMP("dispsys1a: 0x%x : 0x%x\n", SPLITTER_IN_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + SPLITTER_IN_CROSSBAR_DEBUG_MONITOR_PTR));
		DDPDUMP("dispsys1a: 0x%x : 0x%x\n", MERGE_OUT_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + MERGE_OUT_CROSSBAR_DEBUG_MONITOR_PTR));
		DDPDUMP("dispsys1a: 0x%x : 0x%x\n", INSIDE_PC_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + INSIDE_PC_CROSSBAR_DEBUG_MONITOR_PTR));
		DDPDUMP("dispsys1a: 0x%x : 0x%x\n", COMP_OUT_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + COMP_OUT_CROSSBAR_DEBUG_MONITOR_PTR));
	}
	regs = priv->sys_b_config_regs;
	if (regs != NULL) {
		DDPDUMP("dispsys0b: 0x%x : 0x%x\n", DISPSYS_DEBUG_SUBSYS,
			readl(regs + DISPSYS_DEBUG_SUBSYS));
		DDPDUMP("dispsys0b: 0x%x : 0x%x\n", DISPSYS_DISP_SMI_DBG_MON_LARB0_CON0,
			readl(regs + DISPSYS_DISP_SMI_DBG_MON_LARB0_CON0));
		DDPDUMP("dispsys0b: 0x%x : 0x%x\n", DISPSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR,
			readl(regs + DISPSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR));
		DDPDUMP("dispsys0b: 0x%x : 0x%x\n", DISPSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL,
			readl(regs + DISPSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL));
		DDPDUMP("dispsys0b: 0x%x : 0x%x\n", DISPSYS_DISP_SMI_DBG_MON_LARB0_MON1_ADDR,
			readl(regs + DISPSYS_DISP_SMI_DBG_MON_LARB0_MON1_ADDR));
		DDPDUMP("dispsys0b: 0x%x : 0x%x\n", DISPSYS_DISP_SMI_DBG_MON_LARB0_MON1_SEL,
			readl(regs + DISPSYS_DISP_SMI_DBG_MON_LARB0_MON1_SEL));
		DDPDUMP("dispsys0b: 0x%x : 0x%x\n", DISPSYS_DISP_SMI_DBG_MON_LARB0_MON2_ADDR,
			readl(regs + DISPSYS_DISP_SMI_DBG_MON_LARB0_MON2_ADDR));
		DDPDUMP("dispsys0b: 0x%x : 0x%x\n", DISPSYS_DISP_SMI_DBG_MON_LARB0_MON2_SEL,
			readl(regs + DISPSYS_DISP_SMI_DBG_MON_LARB0_MON2_SEL));
		DDPDUMP("dispsys0b: 0x%x : 0x%x\n", DISPSYS_DISP_SMI_DBG_MON_LARB0_MON3_ADDR,
			readl(regs + DISPSYS_DISP_SMI_DBG_MON_LARB0_MON3_ADDR));
		DDPDUMP("dispsys0b: 0x%x : 0x%x\n", DISPSYS_DISP_SMI_DBG_MON_LARB0_MON3_SEL,
			readl(regs + DISPSYS_DISP_SMI_DBG_MON_LARB0_MON3_SEL));
		DDPDUMP("dispsys0b: 0x%x : 0x%x\n", PQ_OUT_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + PQ_OUT_CROSSBAR_DEBUG_MONITOR_PTR));
		DDPDUMP("dispsys0b: 0x%x : 0x%x\n", PQ_IN_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + PQ_IN_CROSSBAR_DEBUG_MONITOR_PTR));
		DDPDUMP("dispsys0b: 0x%x : 0x%x\n", PC_OUT_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + PC_OUT_CROSSBAR_DEBUG_MONITOR_PTR));
		DDPDUMP("dispsys0b: 0x%x : 0x%x\n", PC_IN_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + PC_IN_CROSSBAR_DEBUG_MONITOR_PTR));
	}
	regs = priv->sys_b_side_config_regs;
	if (regs != NULL) {
		DDPDUMP("dispsys1b: 0x%x : 0x%x\n", DISPSYS1_DEBUG_SUBSYS,
			readl(regs + DISPSYS1_DEBUG_SUBSYS));
		DDPDUMP("dispsys1b: 0x%x : 0x%x\n", DISPSYS1_DISP_SMI_DBG_MON_LARB0_CON0,
			readl(regs + DISPSYS1_DISP_SMI_DBG_MON_LARB0_CON0));
		DDPDUMP("dispsys1b: 0x%x : 0x%x\n", DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON0_ADDR,
			readl(regs + DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON0_ADDR));
		DDPDUMP("dispsys1b: 0x%x : 0x%x\n", DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON0_SEL,
			readl(regs + DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON0_SEL));
		DDPDUMP("dispsys1b: 0x%x : 0x%x\n", DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON1_ADDR,
			readl(regs + DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON1_ADDR));
		DDPDUMP("dispsys1b: 0x%x : 0x%x\n", DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON1_SEL,
			readl(regs + DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON1_SEL));
		DDPDUMP("dispsys1b: 0x%x : 0x%x\n", DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON2_ADDR,
			readl(regs + DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON2_ADDR));
		DDPDUMP("dispsys1b: 0x%x : 0x%x\n", DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON2_SEL,
			readl(regs + DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON2_SEL));
		DDPDUMP("dispsys1b: 0x%x : 0x%x\n", DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON3_ADDR,
			readl(regs + DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON3_ADDR));
		DDPDUMP("dispsys1b: 0x%x : 0x%x\n", DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON3_SEL,
			readl(regs + DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON3_SEL));
		DDPDUMP("dispsys1b: 0x%x : 0x%x\n", SPLITTER_OUT_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + SPLITTER_OUT_CROSSBAR_DEBUG_MONITOR_PTR));
		DDPDUMP("dispsys1b: 0x%x : 0x%x\n", SPLITTER_IN_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + SPLITTER_IN_CROSSBAR_DEBUG_MONITOR_PTR));
		DDPDUMP("dispsys1b: 0x%x : 0x%x\n", MERGE_OUT_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + MERGE_OUT_CROSSBAR_DEBUG_MONITOR_PTR));
		DDPDUMP("dispsys1b: 0x%x : 0x%x\n", INSIDE_PC_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + INSIDE_PC_CROSSBAR_DEBUG_MONITOR_PTR));
		DDPDUMP("dispsys1b: 0x%x : 0x%x\n", COMP_OUT_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + COMP_OUT_CROSSBAR_DEBUG_MONITOR_PTR));
	}
	DDPDUMP(">>>>>>>>>>>>>>>> ovlsys   <<<<<<<<<<<<<<<<\n");
	regs = priv->ovlsys0_regs;
	if (regs != NULL) {
		DDPDUMP("ovlsys0: 0x%x : 0x%x\n", OVLSYS_DEBUG_SUBSYS,
			readl(regs + OVLSYS_DEBUG_SUBSYS));
		DDPDUMP("ovlsys0: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_CON0,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_CON0));
		DDPDUMP("ovlsys0: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR));
		DDPDUMP("ovlsys0: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL));
		DDPDUMP("ovlsys0: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_CON0 + 0x24,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_CON0 + 0x24));
		DDPDUMP("ovlsys0: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR + 0x24,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR + 0x24));
		DDPDUMP("ovlsys0: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL + 0x24,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL + 0x24));
		DDPDUMP("ovlsys0: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_CON0 + 0x48,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_CON0 + 0x48));
		DDPDUMP("ovlsys0: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR + 0x48,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR + 0x48));
		DDPDUMP("ovlsys0: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL + 0x48,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL + 0x48));
		DDPDUMP("ovlsys0: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_CON0 + 0x6C,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_CON0 + 0x6C));
		DDPDUMP("ovlsys0: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR + 0x6C,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR + 0x6C));
		DDPDUMP("ovlsys0: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL + 0x6C,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL + 0x6C));

		DDPDUMP("ovlsys0: 0x%x : 0x%x\n", OVL_RSZ_IN_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + OVL_RSZ_IN_CROSSBAR_DEBUG_MONITOR_PTR));
		DDPDUMP("ovlsys0: 0x%x : 0x%x\n", OVL_PQ_IN_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + OVL_PQ_IN_CROSSBAR_DEBUG_MONITOR_PTR));
		DDPDUMP("ovlsys0: 0x%x : 0x%x\n", OVL_OUTPROC_OUT_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + OVL_OUTPROC_OUT_CROSSBAR_DEBUG_MONITOR_PTR));
		DDPDUMP("ovlsys0: 0x%x : 0x%x\n", OVL_EXDMA_OUT_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + OVL_EXDMA_OUT_CROSSBAR_DEBUG_MONITOR_PTR));
		DDPDUMP("ovlsys0: 0x%x : 0x%x\n", OVL_BLENDER_OUT_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + OVL_BLENDER_OUT_CROSSBAR_DEBUG_MONITOR_PTR));
	}
	regs = priv->ovlsys1_regs;
	if (regs != NULL) {
		DDPDUMP("ovlsys1: 0x%x : 0x%x\n", OVLSYS_DEBUG_SUBSYS,
			readl(regs + OVLSYS_DEBUG_SUBSYS));
		DDPDUMP("ovlsys1: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_CON0,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_CON0));
		DDPDUMP("ovlsys1: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR));
		DDPDUMP("ovlsys1: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL));
		DDPDUMP("ovlsys1: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_CON0 + 0x24,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_CON0 + 0x24));
		DDPDUMP("ovlsys1: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR + 0x24,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR + 0x24));
		DDPDUMP("ovlsys1: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL + 0x24,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL + 0x24));
		DDPDUMP("ovlsys1: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_CON0 + 0x48,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_CON0 + 0x48));
		DDPDUMP("ovlsys1: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR + 0x48,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR + 0x48));
		DDPDUMP("ovlsys1: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL + 0x48,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL + 0x48));
		DDPDUMP("ovlsys1: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_CON0 + 0x6C,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_CON0 + 0x6C));
		DDPDUMP("ovlsys1: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR + 0x6C,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR + 0x6C));
		DDPDUMP("ovlsys1: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL + 0x6C,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL + 0x6C));

		DDPDUMP("ovlsys1: 0x%x : 0x%x\n", OVL_RSZ_IN_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + OVL_RSZ_IN_CROSSBAR_DEBUG_MONITOR_PTR));
		DDPDUMP("ovlsys1: 0x%x : 0x%x\n", OVL_PQ_IN_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + OVL_PQ_IN_CROSSBAR_DEBUG_MONITOR_PTR));
		DDPDUMP("ovlsys1: 0x%x : 0x%x\n", OVL_OUTPROC_OUT_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + OVL_OUTPROC_OUT_CROSSBAR_DEBUG_MONITOR_PTR));
		DDPDUMP("ovlsys1: 0x%x : 0x%x\n", OVL_EXDMA_OUT_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + OVL_EXDMA_OUT_CROSSBAR_DEBUG_MONITOR_PTR));
		DDPDUMP("ovlsys1: 0x%x : 0x%x\n", OVL_BLENDER_OUT_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + OVL_BLENDER_OUT_CROSSBAR_DEBUG_MONITOR_PTR));
	}
	regs = priv->ovlsys2_regs;
	if (regs != NULL) {
		DDPDUMP("ovlsys2: 0x%x : 0x%x\n", OVLSYS_DEBUG_SUBSYS,
			readl(regs + OVLSYS_DEBUG_SUBSYS));
		DDPDUMP("ovlsys2: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_CON0,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_CON0));
		DDPDUMP("ovlsys2: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR));
		DDPDUMP("ovlsys2: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL));
		DDPDUMP("ovlsys2: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_CON0 + 0x24,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_CON0 + 0x24));
		DDPDUMP("ovlsys2: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR + 0x24,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR + 0x24));
		DDPDUMP("ovlsys2: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL + 0x24,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL + 0x24));
		DDPDUMP("ovlsys2: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_CON0 + 0x48,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_CON0 + 0x48));
		DDPDUMP("ovlsys2: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR + 0x48,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR + 0x48));
		DDPDUMP("ovlsys2: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL + 0x48,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL + 0x48));
		DDPDUMP("ovlsys2: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_CON0 + 0x6C,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_CON0 + 0x6C));
		DDPDUMP("ovlsys2: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR + 0x6C,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR + 0x6C));
		DDPDUMP("ovlsys2: 0x%x : 0x%x\n", OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL + 0x6C,
			readl(regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL + 0x6C));

		DDPDUMP("ovlsys2: 0x%x : 0x%x\n", OVL_RSZ_IN_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + OVL_RSZ_IN_CROSSBAR_DEBUG_MONITOR_PTR));
		DDPDUMP("ovlsys2: 0x%x : 0x%x\n", OVL_PQ_IN_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + OVL_PQ_IN_CROSSBAR_DEBUG_MONITOR_PTR));
		DDPDUMP("ovlsys2: 0x%x : 0x%x\n", OVL_OUTPROC_OUT_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + OVL_OUTPROC_OUT_CROSSBAR_DEBUG_MONITOR_PTR));
		DDPDUMP("ovlsys2: 0x%x : 0x%x\n", OVL_EXDMA_OUT_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + OVL_EXDMA_OUT_CROSSBAR_DEBUG_MONITOR_PTR));
		DDPDUMP("ovlsys2: 0x%x : 0x%x\n", OVL_BLENDER_OUT_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + OVL_BLENDER_OUT_CROSSBAR_DEBUG_MONITOR_PTR));
	}

}

void mtk_dbgtp_dump_mmlsys_regs(void __iomem *config_regs, unsigned int mmlsys_id)
{
	void __iomem *regs = NULL;

	DDPDUMP(">>>>>>>>>>>>>>>> mmlsys   <<<<<<<<<<<<<<<<\n");
	regs = config_regs;
	if ((regs != NULL) && (mmlsys_id == 0)) {
		DDPDUMP("mmlsys0: 0x%x : 0x%x\n", MMLSYS_DEBUG_SUBSYS,
			readl(regs + MMLSYS_DEBUG_SUBSYS));
		DDPDUMP("mmlsys0: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB0_CON0,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_CON0));
		DDPDUMP("mmlsys0: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR));
		DDPDUMP("mmlsys0: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL));
		DDPDUMP("mmlsys0: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB0_MON1_ADDR,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_MON1_ADDR));
		DDPDUMP("mmlsys0: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB0_MON1_SEL,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_MON1_SEL));
		DDPDUMP("mmlsys0: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB0_MON2_ADDR,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_MON2_ADDR));
		DDPDUMP("mmlsys0: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB0_MON2_SEL,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_MON2_SEL));
		DDPDUMP("mmlsys0: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB0_MON3_ADDR,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_MON3_ADDR));
		DDPDUMP("mmlsys0: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB0_MON3_SEL,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_MON3_SEL));
		DDPDUMP("mmlsys0: 0x%x : 0x%x\n", MML_PQ_OUT_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + MML_PQ_OUT_CROSSBAR_DEBUG_MONITOR_PTR));
		DDPDUMP("mmlsys0: 0x%x : 0x%x\n", MML_PQ_IN_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + MML_PQ_IN_CROSSBAR_DEBUG_MONITOR_PTR));
	}
	if ((regs != NULL) && (mmlsys_id == 1)) {
		DDPDUMP("mmlsys1: 0x%x : 0x%x\n", MMLSYS_DEBUG_SUBSYS,
			readl(regs + MMLSYS_DEBUG_SUBSYS));
		DDPDUMP("mmlsys1: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB0_CON0,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_CON0));
		DDPDUMP("mmlsys1: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR));
		DDPDUMP("mmlsys1: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL));
		DDPDUMP("mmlsys1: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB0_MON1_ADDR,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_MON1_ADDR));
		DDPDUMP("mmlsys1: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB0_MON1_SEL,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_MON1_SEL));
		DDPDUMP("mmlsys1: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB0_MON2_ADDR,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_MON2_ADDR));
		DDPDUMP("mmlsys1: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB0_MON2_SEL,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_MON2_SEL));
		DDPDUMP("mmlsys1: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB0_MON3_ADDR,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_MON3_ADDR));
		DDPDUMP("mmlsys1: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB0_MON3_SEL,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_MON3_SEL));
		DDPDUMP("mmlsys1: 0x%x : 0x%x\n", MML_PQ_OUT_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + MML_PQ_OUT_CROSSBAR_DEBUG_MONITOR_PTR));
		DDPDUMP("mmlsys1: 0x%x : 0x%x\n", MML_PQ_IN_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + MML_PQ_IN_CROSSBAR_DEBUG_MONITOR_PTR));
	}
	if ((regs != NULL) && (mmlsys_id == 2)) {
		DDPDUMP("mmlsys2: 0x%x : 0x%x\n", MMLSYS_DEBUG_SUBSYS,
			readl(regs + MMLSYS_DEBUG_SUBSYS));
		DDPDUMP("mmlsys2: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB0_CON0,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_CON0));
		DDPDUMP("mmlsys2: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_MON0_ADDR));
		DDPDUMP("mmlsys2: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL));
		DDPDUMP("mmlsys2: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB0_MON1_ADDR,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_MON1_ADDR));
		DDPDUMP("mmlsys2: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB0_MON1_SEL,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_MON1_SEL));
		DDPDUMP("mmlsys2: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB0_MON2_ADDR,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_MON2_ADDR));
		DDPDUMP("mmlsys2: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB0_MON2_SEL,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_MON2_SEL));
		DDPDUMP("mmlsys2: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB0_MON3_ADDR,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_MON3_ADDR));
		DDPDUMP("mmlsys2: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB0_MON3_SEL,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_MON3_SEL));
		DDPDUMP("mmlsys2: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB1_CON0,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB1_CON0));
		DDPDUMP("mmlsys2: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB1_MON0_ADDR,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB1_MON0_ADDR));
		DDPDUMP("mmlsys2: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB1_MON0_SEL,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB1_MON0_SEL));
		DDPDUMP("mmlsys2: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB1_MON1_ADDR,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB1_MON1_ADDR));
		DDPDUMP("mmlsys2: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB1_MON1_SEL,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB1_MON1_SEL));
		DDPDUMP("mmlsys2: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB1_MON2_ADDR,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB1_MON2_ADDR));
		DDPDUMP("mmlsys2: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB1_MON2_SEL,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB1_MON2_SEL));
		DDPDUMP("mmlsys2: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB1_MON3_ADDR,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB1_MON3_ADDR));
		DDPDUMP("mmlsys2: 0x%x : 0x%x\n", MMLSYS_DISP_SMI_DBG_MON_LARB1_MON3_SEL,
			readl(regs + MMLSYS_DISP_SMI_DBG_MON_LARB1_MON3_SEL));
		DDPDUMP("mmlsys2: 0x%x : 0x%x\n", MML_PQ_OUT_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + MML_PQ_OUT_CROSSBAR_DEBUG_MONITOR_PTR));
		DDPDUMP("mmlsys2: 0x%x : 0x%x\n", MML_PQ_IN_CROSSBAR_DEBUG_MONITOR_PTR,
			readl(regs + MML_PQ_IN_CROSSBAR_DEBUG_MONITOR_PTR));
	}
}

void mtk_dbgtp_default_cfg_load(struct mtk_drm_private *priv)
{
	DDPMSG("%s:%d +\n", __func__, __LINE__);

	/* reset all setting */
	memset(&priv->mtk_dbgtp_sta, 0, sizeof(priv->mtk_dbgtp_sta));

	/* debug top default setting */
	priv->mtk_dbgtp_sta.dbgtp_en = true;
	priv->mtk_dbgtp_sta.dbgtp_switch = 0x1795;
	priv->mtk_dbgtp_sta.dbgtp_prd_trig_en = true;
	priv->mtk_dbgtp_sta.dbgtp_trig_prd = 260;
	priv->mtk_dbgtp_sta.dbgtp_timeout_en = 0x0;
	priv->mtk_dbgtp_sta.dsi_lpc_mon_en = false;
	priv->mtk_dbgtp_sta.is_validation_mode = false;

	/* dpc default setting */
	priv->mtk_dbgtp_sta.dbgtp_dpc_mon_cfg = 0x00FFE;

	/* fifo mon default setting */
	priv->mtk_dbgtp_sta.fifo_mon_en[0] = 1;
	priv->mtk_dbgtp_sta.fifo_mon_trig_thrd[0] = 30;

	/* dispsys default setting */
	/* dispsys0A */
	priv->mtk_dbgtp_sta.dispsys[0].subsys_mon_en = false;
	priv->mtk_dbgtp_sta.dispsys[0].subsys_smi_trig_en = true;
	priv->mtk_dbgtp_sta.dispsys[0].subsys_dsi_trig_en = false;
	priv->mtk_dbgtp_sta.dispsys[0].subsys_crossbar_info_en = false;
	priv->mtk_dbgtp_sta.dispsys[0].subsys_mon_info_en = true;
	priv->mtk_dbgtp_sta.dispsys[0].smi_mon[0].smi_mon_en = true;
	priv->mtk_dbgtp_sta.dispsys[0].smi_mon[0].rst_by_frame = true;
	priv->mtk_dbgtp_sta.dispsys[0].smi_mon[0].slice_time = 0x104;
	priv->mtk_dbgtp_sta.dispsys[0].smi_mon[0].smi_mon_dump_sel = 0x1;
	priv->mtk_dbgtp_sta.dispsys[0].smi_mon[0].smi_mon_portid[0] = 2;
	priv->mtk_dbgtp_sta.dispsys[0].smi_mon[0].smi_mon_portid[1] = 3;
	priv->mtk_dbgtp_sta.dispsys[0].smi_mon[0].smi_mon_portid[2] = 4;
	priv->mtk_dbgtp_sta.dispsys[0].smi_mon[0].smi_mon_portid[3] = 5;
	priv->mtk_dbgtp_sta.dispsys[0].crossbar_mon_cfg0 = 0x00000006;//PQ_OUT
	priv->mtk_dbgtp_sta.dispsys[0].crossbar_mon_cfg1 = 0x00000000;//PQ_IN
	priv->mtk_dbgtp_sta.dispsys[0].crossbar_mon_cfg2 = 0x00000002;//PC_OUT
	priv->mtk_dbgtp_sta.dispsys[0].crossbar_mon_cfg3 = 0x00010000;//PC_IN
	/* dispsys1A */
	priv->mtk_dbgtp_sta.dispsys[1].subsys_mon_en = true;
	priv->mtk_dbgtp_sta.dispsys[1].subsys_smi_trig_en = false;
	priv->mtk_dbgtp_sta.dispsys[1].subsys_dsi_trig_en = true;
	priv->mtk_dbgtp_sta.dispsys[1].subsys_crossbar_info_en = false;
	priv->mtk_dbgtp_sta.dispsys[1].subsys_mon_info_en = false;
	priv->mtk_dbgtp_sta.dispsys[1].smi_mon[0].smi_mon_en = false;
	priv->mtk_dbgtp_sta.dispsys[1].smi_mon[0].rst_by_frame = true;
	priv->mtk_dbgtp_sta.dispsys[1].smi_mon[0].slice_time = 0x104;
	priv->mtk_dbgtp_sta.dispsys[1].smi_mon[0].smi_mon_dump_sel = 0x1;
	priv->mtk_dbgtp_sta.dispsys[1].smi_mon[0].smi_mon_portid[0] = 0;
	priv->mtk_dbgtp_sta.dispsys[1].smi_mon[0].smi_mon_portid[1] = 1;
	priv->mtk_dbgtp_sta.dispsys[1].smi_mon[0].smi_mon_portid[2] = 5;
	priv->mtk_dbgtp_sta.dispsys[1].smi_mon[0].smi_mon_portid[3] = 6;
	priv->mtk_dbgtp_sta.dispsys[1].dsi_mon.dsi_mon_en = true;
	priv->mtk_dbgtp_sta.dispsys[1].dsi_mon.dsi_mon_sel = 0x0200;
	priv->mtk_dbgtp_sta.dispsys[1].dsi_mon.dsi_buf_sel = 0x0316;
	priv->mtk_dbgtp_sta.dispsys[1].crossbar_mon_cfg0 = 0x00090000;//SPLITTER_OUT
	priv->mtk_dbgtp_sta.dispsys[1].crossbar_mon_cfg1 = 0x00000000;//SPLITTER_IN
	priv->mtk_dbgtp_sta.dispsys[1].crossbar_mon_cfg2 = 0x00000000;//MERGE_OUT
	priv->mtk_dbgtp_sta.dispsys[1].crossbar_mon_cfg3 = 0x00000000;//INSIDE_PC
	priv->mtk_dbgtp_sta.dispsys[1].crossbar_mon_cfg4 = 0x00000000;//COMP_OUT
	/* dispsys0B */
	priv->mtk_dbgtp_sta.dispsys[2].subsys_mon_en = false;
	priv->mtk_dbgtp_sta.dispsys[2].subsys_smi_trig_en = true;
	priv->mtk_dbgtp_sta.dispsys[2].subsys_dsi_trig_en = true;
	priv->mtk_dbgtp_sta.dispsys[2].subsys_crossbar_info_en = false;
	priv->mtk_dbgtp_sta.dispsys[2].subsys_mon_info_en = true;
	priv->mtk_dbgtp_sta.dispsys[2].smi_mon[0].smi_mon_en = true;
	priv->mtk_dbgtp_sta.dispsys[2].smi_mon[0].rst_by_frame = true;
	priv->mtk_dbgtp_sta.dispsys[2].smi_mon[0].slice_time = 0x104;
	priv->mtk_dbgtp_sta.dispsys[2].smi_mon[0].smi_mon_portid[0] = 1;
	priv->mtk_dbgtp_sta.dispsys[2].smi_mon[0].smi_mon_portid[1] = 3;
	priv->mtk_dbgtp_sta.dispsys[2].smi_mon[0].smi_mon_portid[2] = 6;
	priv->mtk_dbgtp_sta.dispsys[2].smi_mon[0].smi_mon_portid[3] = 8;
	priv->mtk_dbgtp_sta.dispsys[2].crossbar_mon_cfg0 = 0x00000006;//PQ_OUT
	priv->mtk_dbgtp_sta.dispsys[2].crossbar_mon_cfg1 = 0x00000000;//PQ_IN
	priv->mtk_dbgtp_sta.dispsys[2].crossbar_mon_cfg2 = 0x00000002;//PC_OUT
	priv->mtk_dbgtp_sta.dispsys[2].crossbar_mon_cfg3 = 0x00010000;//PC_IN
	/* dispsys1B */
	priv->mtk_dbgtp_sta.dispsys[3].subsys_mon_en = false;
	priv->mtk_dbgtp_sta.dispsys[3].subsys_smi_trig_en = true;
	priv->mtk_dbgtp_sta.dispsys[3].subsys_dsi_trig_en = true;
	priv->mtk_dbgtp_sta.dispsys[3].subsys_crossbar_info_en = false;
	priv->mtk_dbgtp_sta.dispsys[3].subsys_mon_info_en = true;
	priv->mtk_dbgtp_sta.dispsys[3].smi_mon[0].smi_mon_en = true;
	priv->mtk_dbgtp_sta.dispsys[3].smi_mon[0].rst_by_frame = true;
	priv->mtk_dbgtp_sta.dispsys[3].smi_mon[0].slice_time = 0x104;
	priv->mtk_dbgtp_sta.dispsys[3].smi_mon[0].smi_mon_portid[0] = 0;
	priv->mtk_dbgtp_sta.dispsys[3].smi_mon[0].smi_mon_portid[1] = 1;
	priv->mtk_dbgtp_sta.dispsys[3].smi_mon[0].smi_mon_portid[2] = 5;
	priv->mtk_dbgtp_sta.dispsys[3].smi_mon[0].smi_mon_portid[3] = 6;
	priv->mtk_dbgtp_sta.dispsys[3].dsi_mon.dsi_mon_en = true;
	priv->mtk_dbgtp_sta.dispsys[3].dsi_mon.dsi_mon_sel = 0xFFFF;
	priv->mtk_dbgtp_sta.dispsys[3].dsi_mon.dsi_buf_sel = 0xFFFF;
	priv->mtk_dbgtp_sta.dispsys[3].crossbar_mon_cfg0 = 0x00090000;//SPLITTER_OUT
	priv->mtk_dbgtp_sta.dispsys[3].crossbar_mon_cfg1 = 0x00000000;//SPLITTER_IN
	priv->mtk_dbgtp_sta.dispsys[3].crossbar_mon_cfg2 = 0x00000002;//MERGE_OUT
	priv->mtk_dbgtp_sta.dispsys[3].crossbar_mon_cfg3 = 0x00000000;//INSIDE_PC
	priv->mtk_dbgtp_sta.dispsys[3].crossbar_mon_cfg4 = 0x00000000;//COMP_OUT

	/* ovlsys default setting */
	/* ovlsys0 */
	priv->mtk_dbgtp_sta.ovlsys[0].subsys_mon_en = true;
	priv->mtk_dbgtp_sta.ovlsys[0].subsys_smi_trig_en = true;
	priv->mtk_dbgtp_sta.ovlsys[0].subsys_crossbar_info_en = false;
	priv->mtk_dbgtp_sta.ovlsys[0].subsys_inlinerotate_info_en = false;
	priv->mtk_dbgtp_sta.ovlsys[0].subsys_mon_info_en = false;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[0].smi_mon_en = true;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[1].smi_mon_en = true;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[2].smi_mon_en = false;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[3].smi_mon_en = false;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[0].rst_by_frame = true;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[1].rst_by_frame = true;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[2].rst_by_frame = true;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[3].rst_by_frame = true;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[0].slice_time = 0x104;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[1].slice_time = 0x104;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[2].slice_time = 0x104;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[3].slice_time = 0x104;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[0].smi_mon_dump_sel = 0x1;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[1].smi_mon_dump_sel = 0x1;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[2].smi_mon_dump_sel = 0x1;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[3].smi_mon_dump_sel = 0x1;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[0].smi_mon_portid[0] = 2;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[0].smi_mon_cg_ctl[1] = 1;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[0].smi_mon_cg_ctl[2] = 1;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[0].smi_mon_cg_ctl[3] = 1;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[1].smi_mon_portid[0] = 2;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[1].smi_mon_cg_ctl[1] = 1;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[1].smi_mon_cg_ctl[2] = 1;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[1].smi_mon_cg_ctl[3] = 1;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[2].smi_mon_portid[0] = 2;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[2].smi_mon_cg_ctl[1] = 1;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[2].smi_mon_cg_ctl[2] = 1;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[2].smi_mon_cg_ctl[3] = 1;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[3].smi_mon_portid[0] = 2;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[3].smi_mon_cg_ctl[1] = 1;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[3].smi_mon_cg_ctl[2] = 1;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[3].smi_mon_cg_ctl[3] = 1;
	priv->mtk_dbgtp_sta.ovlsys[0].crossbar_mon_cfg0 = 0x00000002;//OVL_RSZ_IN
	priv->mtk_dbgtp_sta.ovlsys[0].crossbar_mon_cfg1 = 0x00000000;//OVL_PQ_IN
	priv->mtk_dbgtp_sta.ovlsys[0].crossbar_mon_cfg2 = 0x00000000;//OVL_OUTPROC_OUT
	priv->mtk_dbgtp_sta.ovlsys[0].crossbar_mon_cfg3 = 0x00060003;//OVL_EXDMA_OUT
	priv->mtk_dbgtp_sta.ovlsys[0].crossbar_mon_cfg4 = 0x00000006;//OVL_BLENDER_OUT
	/* ovlsys1 */
	priv->mtk_dbgtp_sta.ovlsys[1].subsys_mon_en = false;
	priv->mtk_dbgtp_sta.ovlsys[1].subsys_smi_trig_en = true;
	priv->mtk_dbgtp_sta.ovlsys[1].subsys_crossbar_info_en = false;
	priv->mtk_dbgtp_sta.ovlsys[1].subsys_inlinerotate_info_en = false;
	priv->mtk_dbgtp_sta.ovlsys[1].subsys_mon_info_en = true;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[0].smi_mon_en = true;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[1].smi_mon_en = true;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[2].smi_mon_en = true;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[3].smi_mon_en = true;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[0].rst_by_frame = true;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[1].rst_by_frame = true;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[2].rst_by_frame = true;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[3].rst_by_frame = true;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[0].slice_time = 0x104;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[1].slice_time = 0x104;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[2].slice_time = 0x104;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[3].slice_time = 0x104;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[0].smi_mon_portid[0] = 2;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[0].smi_mon_cg_ctl[1] = 1;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[0].smi_mon_cg_ctl[2] = 1;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[0].smi_mon_cg_ctl[3] = 1;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[1].smi_mon_portid[0] = 2;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[1].smi_mon_cg_ctl[1] = 1;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[1].smi_mon_cg_ctl[2] = 1;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[1].smi_mon_cg_ctl[3] = 1;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[2].smi_mon_portid[0] = 2;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[2].smi_mon_cg_ctl[1] = 1;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[2].smi_mon_cg_ctl[2] = 1;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[2].smi_mon_cg_ctl[3] = 1;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[3].smi_mon_portid[0] = 2;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[3].smi_mon_cg_ctl[1] = 1;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[3].smi_mon_cg_ctl[2] = 1;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[3].smi_mon_cg_ctl[3] = 1;
	priv->mtk_dbgtp_sta.ovlsys[1].crossbar_mon_cfg0 = 0x00000002;//OVL_RSZ_IN
	priv->mtk_dbgtp_sta.ovlsys[1].crossbar_mon_cfg1 = 0x00000003;//OVL_PQ_IN
	priv->mtk_dbgtp_sta.ovlsys[1].crossbar_mon_cfg2 = 0x00000000;//OVL_OUTPROC_OUT
	priv->mtk_dbgtp_sta.ovlsys[1].crossbar_mon_cfg3 = 0x00060003;//OVL_EXDMA_OUT
	priv->mtk_dbgtp_sta.ovlsys[1].crossbar_mon_cfg4 = 0x00000006;//OVL_BLENDER_OUT
	/* ovlsys2 */
	priv->mtk_dbgtp_sta.ovlsys[2].subsys_mon_en = false;
	priv->mtk_dbgtp_sta.ovlsys[2].subsys_smi_trig_en = true;
	priv->mtk_dbgtp_sta.ovlsys[2].subsys_crossbar_info_en = false;
	priv->mtk_dbgtp_sta.ovlsys[2].subsys_inlinerotate_info_en = false;
	priv->mtk_dbgtp_sta.ovlsys[2].subsys_mon_info_en = true;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[0].smi_mon_en = true;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[1].smi_mon_en = true;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[2].smi_mon_en = true;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[3].smi_mon_en = true;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[0].rst_by_frame = true;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[1].rst_by_frame = true;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[2].rst_by_frame = true;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[3].rst_by_frame = true;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[0].slice_time = 0x104;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[1].slice_time = 0x104;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[2].slice_time = 0x104;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[3].slice_time = 0x104;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[0].smi_mon_portid[0] = 2;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[0].smi_mon_cg_ctl[1] = 1;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[0].smi_mon_cg_ctl[2] = 1;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[0].smi_mon_cg_ctl[3] = 1;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[1].smi_mon_portid[0] = 2;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[1].smi_mon_cg_ctl[1] = 1;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[1].smi_mon_cg_ctl[2] = 1;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[1].smi_mon_cg_ctl[3] = 1;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[2].smi_mon_portid[0] = 2;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[2].smi_mon_cg_ctl[1] = 1;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[2].smi_mon_cg_ctl[2] = 1;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[2].smi_mon_cg_ctl[3] = 1;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[3].smi_mon_portid[0] = 2;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[3].smi_mon_cg_ctl[1] = 1;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[3].smi_mon_cg_ctl[2] = 1;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[3].smi_mon_cg_ctl[3] = 1;
	priv->mtk_dbgtp_sta.ovlsys[2].crossbar_mon_cfg0 = 0x00000002;//OVL_RSZ_IN
	priv->mtk_dbgtp_sta.ovlsys[2].crossbar_mon_cfg1 = 0x00000003;//OVL_PQ_IN
	priv->mtk_dbgtp_sta.ovlsys[2].crossbar_mon_cfg2 = 0x00000000;//OVL_OUTPROC_OUT
	priv->mtk_dbgtp_sta.ovlsys[2].crossbar_mon_cfg3 = 0x00060003;//OVL_EXDMA_OUT
	priv->mtk_dbgtp_sta.ovlsys[2].crossbar_mon_cfg4 = 0x00000006;//OVL_BLENDER_OUT

	/* mmlsys default setting */
	/* mmlsys0 */
	priv->mtk_dbgtp_sta.mmlsys[0].subsys_mon_en = false;
	priv->mtk_dbgtp_sta.mmlsys[0].subsys_smi_trig_en = true;
	priv->mtk_dbgtp_sta.mmlsys[0].subsys_crossbar_info_en = true;
	priv->mtk_dbgtp_sta.mmlsys[0].subsys_inlinerotate_info_en = true;
	priv->mtk_dbgtp_sta.mmlsys[0].subsys_mon_info_en = true;
	priv->mtk_dbgtp_sta.mmlsys[0].smi_mon[0].smi_mon_en = true;
	priv->mtk_dbgtp_sta.mmlsys[0].smi_mon[0].rst_by_frame = true;
	priv->mtk_dbgtp_sta.mmlsys[0].smi_mon[0].slice_time = 0x104;
	priv->mtk_dbgtp_sta.mmlsys[0].smi_mon[0].smi_mon_portid[0] = 4;
	priv->mtk_dbgtp_sta.mmlsys[0].smi_mon[0].smi_mon_portid[1] = 5;
	priv->mtk_dbgtp_sta.mmlsys[0].smi_mon[0].smi_mon_portid[2] = 11;
	priv->mtk_dbgtp_sta.mmlsys[0].smi_mon[0].smi_mon_portid[3] = 12;
	priv->mtk_dbgtp_sta.mmlsys[0].crossbar_mon_cfg0 = 0x00000000;//MML_PQ_OUT
	priv->mtk_dbgtp_sta.mmlsys[0].crossbar_mon_cfg1 = 0x00000000;//MML_PQ_IN
	/* mmlsys1 */
	priv->mtk_dbgtp_sta.mmlsys[1].subsys_mon_en = false;
	priv->mtk_dbgtp_sta.mmlsys[1].subsys_smi_trig_en = true;
	priv->mtk_dbgtp_sta.mmlsys[1].subsys_crossbar_info_en = true;
	priv->mtk_dbgtp_sta.mmlsys[1].subsys_inlinerotate_info_en = true;
	priv->mtk_dbgtp_sta.mmlsys[1].subsys_mon_info_en = true;
	priv->mtk_dbgtp_sta.mmlsys[1].smi_mon[0].smi_mon_en = true;
	priv->mtk_dbgtp_sta.mmlsys[1].smi_mon[0].rst_by_frame = true;
	priv->mtk_dbgtp_sta.mmlsys[1].smi_mon[0].slice_time = 0x104;
	priv->mtk_dbgtp_sta.mmlsys[1].smi_mon[0].smi_mon_portid[0] = 4;
	priv->mtk_dbgtp_sta.mmlsys[1].smi_mon[0].smi_mon_portid[1] = 5;
	priv->mtk_dbgtp_sta.mmlsys[1].smi_mon[0].smi_mon_portid[2] = 11;
	priv->mtk_dbgtp_sta.mmlsys[1].smi_mon[0].smi_mon_portid[3] = 12;
	priv->mtk_dbgtp_sta.mmlsys[1].crossbar_mon_cfg0 = 0x00000000;//MML_PQ_OUT
	priv->mtk_dbgtp_sta.mmlsys[1].crossbar_mon_cfg1 = 0x00000000;//MML_PQ_IN
	/* mmlsys2 */
	priv->mtk_dbgtp_sta.mmlsys[2].subsys_mon_en = true;
	priv->mtk_dbgtp_sta.mmlsys[2].subsys_smi_trig_en = true;
	priv->mtk_dbgtp_sta.mmlsys[2].subsys_crossbar_info_en = true;
	priv->mtk_dbgtp_sta.mmlsys[2].subsys_inlinerotate_info_en = false;
	priv->mtk_dbgtp_sta.mmlsys[2].subsys_mon_info_en = true;
	priv->mtk_dbgtp_sta.mmlsys[2].smi_mon[0].smi_mon_en = true;
	priv->mtk_dbgtp_sta.mmlsys[2].smi_mon[1].smi_mon_en = true;
	priv->mtk_dbgtp_sta.mmlsys[2].smi_mon[0].rst_by_frame = true;
	priv->mtk_dbgtp_sta.mmlsys[2].smi_mon[1].rst_by_frame = true;
	priv->mtk_dbgtp_sta.mmlsys[2].smi_mon[0].slice_time = 0x104;
	priv->mtk_dbgtp_sta.mmlsys[2].smi_mon[1].slice_time = 0x104;
	priv->mtk_dbgtp_sta.mmlsys[2].smi_mon[0].smi_mon_dump_sel = 0x1;
	priv->mtk_dbgtp_sta.mmlsys[2].smi_mon[1].smi_mon_dump_sel = 0x1;
	priv->mtk_dbgtp_sta.mmlsys[2].smi_mon[0].smi_mon_portid[0] = 0;
	priv->mtk_dbgtp_sta.mmlsys[2].smi_mon[0].smi_mon_portid[1] = 1;
	priv->mtk_dbgtp_sta.mmlsys[2].smi_mon[0].smi_mon_portid[2] = 2;
	priv->mtk_dbgtp_sta.mmlsys[2].smi_mon[0].smi_mon_portid[3] = 3;
	priv->mtk_dbgtp_sta.mmlsys[2].smi_mon[1].smi_mon_portid[0] = 0;
	priv->mtk_dbgtp_sta.mmlsys[2].smi_mon[1].smi_mon_portid[1] = 1;
	priv->mtk_dbgtp_sta.mmlsys[2].smi_mon[1].smi_mon_portid[2] = 2;
	priv->mtk_dbgtp_sta.mmlsys[2].smi_mon[1].smi_mon_portid[3] = 3;
	priv->mtk_dbgtp_sta.mmlsys[2].crossbar_mon_cfg0 = 0x00000000;//MML_PQ_OUT
	priv->mtk_dbgtp_sta.mmlsys[2].crossbar_mon_cfg1 = 0x00000000;//MML_PQ_IN

	DDPMSG("%s:%d -\n", __func__, __LINE__);
}

void mtk_dbgtp_load_all_open_setting(struct mtk_drm_private *priv)
{
	DDPMSG("%s:%d +\n", __func__, __LINE__);

	/* reset all setting */
	memset(&priv->mtk_dbgtp_sta, 0, sizeof(priv->mtk_dbgtp_sta));

	/* debug top default setting */
	priv->mtk_dbgtp_sta.dbgtp_en = true;
	priv->mtk_dbgtp_sta.dbgtp_switch = 0x1FFF;
	priv->mtk_dbgtp_sta.dbgtp_prd_trig_en = true;
	priv->mtk_dbgtp_sta.dbgtp_trig_prd = 2600;
	priv->mtk_dbgtp_sta.dbgtp_timeout_en = 0x0;
	priv->mtk_dbgtp_sta.dsi_lpc_mon_en = true;
	priv->mtk_dbgtp_sta.is_validation_mode = false;

	/* dpc default setting */
	priv->mtk_dbgtp_sta.dbgtp_dpc_mon_cfg = 0x10FFE;

	/* fifo mon default setting */
	priv->mtk_dbgtp_sta.fifo_mon_en[0] = 1;
	priv->mtk_dbgtp_sta.fifo_mon_trig_thrd[0] = 20;

	/* dispsys default setting */
	/* dispsys0A */
	priv->mtk_dbgtp_sta.dispsys[0].subsys_mon_en = true;
	priv->mtk_dbgtp_sta.dispsys[0].subsys_smi_trig_en = true;
	priv->mtk_dbgtp_sta.dispsys[0].subsys_dsi_trig_en = true;
	priv->mtk_dbgtp_sta.dispsys[0].subsys_crossbar_info_en = true;
	priv->mtk_dbgtp_sta.dispsys[0].subsys_mon_info_en = true;
	priv->mtk_dbgtp_sta.dispsys[0].smi_mon[0].smi_mon_en = true;
	priv->mtk_dbgtp_sta.dispsys[0].smi_mon[0].rst_by_frame = true;
	priv->mtk_dbgtp_sta.dispsys[0].smi_mon[0].slice_time = 0x514;
	priv->mtk_dbgtp_sta.dispsys[0].smi_mon[0].smi_mon_portid[0] = 2;
	priv->mtk_dbgtp_sta.dispsys[0].smi_mon[0].smi_mon_portid[1] = 3;
	priv->mtk_dbgtp_sta.dispsys[0].smi_mon[0].smi_mon_portid[2] = 4;
	priv->mtk_dbgtp_sta.dispsys[0].smi_mon[0].smi_mon_portid[3] = 5;
	priv->mtk_dbgtp_sta.dispsys[0].crossbar_mon_cfg0 = 0x00000006;//PQ_OUT
	priv->mtk_dbgtp_sta.dispsys[0].crossbar_mon_cfg1 = 0x00000000;//PQ_IN
	priv->mtk_dbgtp_sta.dispsys[0].crossbar_mon_cfg2 = 0x00000002;//PC_OUT
	priv->mtk_dbgtp_sta.dispsys[0].crossbar_mon_cfg3 = 0x00010000;//PC_IN
	/* dispsys1A */
	priv->mtk_dbgtp_sta.dispsys[1].subsys_mon_en = true;
	priv->mtk_dbgtp_sta.dispsys[1].subsys_smi_trig_en = true;
	priv->mtk_dbgtp_sta.dispsys[1].subsys_dsi_trig_en = true;
	priv->mtk_dbgtp_sta.dispsys[1].subsys_crossbar_info_en = true;
	priv->mtk_dbgtp_sta.dispsys[1].subsys_mon_info_en = true;
	priv->mtk_dbgtp_sta.dispsys[1].smi_mon[0].smi_mon_en = true;
	priv->mtk_dbgtp_sta.dispsys[1].smi_mon[0].rst_by_frame = true;
	priv->mtk_dbgtp_sta.dispsys[1].smi_mon[0].slice_time = 0x514;
	priv->mtk_dbgtp_sta.dispsys[1].smi_mon[0].smi_mon_portid[0] = 0;
	priv->mtk_dbgtp_sta.dispsys[1].smi_mon[0].smi_mon_portid[1] = 1;
	priv->mtk_dbgtp_sta.dispsys[1].smi_mon[0].smi_mon_portid[2] = 5;
	priv->mtk_dbgtp_sta.dispsys[1].smi_mon[0].smi_mon_portid[3] = 6;
	priv->mtk_dbgtp_sta.dispsys[1].dsi_mon.dsi_mon_en = true;
	priv->mtk_dbgtp_sta.dispsys[1].dsi_mon.dsi_mon_sel = 0xFFFF;
	priv->mtk_dbgtp_sta.dispsys[1].dsi_mon.dsi_buf_sel = 0xFFFF;
	priv->mtk_dbgtp_sta.dispsys[1].crossbar_mon_cfg0 = 0x00090000;//SPLITTER_OUT
	priv->mtk_dbgtp_sta.dispsys[1].crossbar_mon_cfg1 = 0x00000000;//SPLITTER_IN
	priv->mtk_dbgtp_sta.dispsys[1].crossbar_mon_cfg2 = 0x00000000;//MERGE_OUT
	priv->mtk_dbgtp_sta.dispsys[1].crossbar_mon_cfg3 = 0x00000000;//INSIDE_PC
	priv->mtk_dbgtp_sta.dispsys[1].crossbar_mon_cfg4 = 0x00000000;//COMP_OUT
	/* dispsys0B */
	priv->mtk_dbgtp_sta.dispsys[2].subsys_mon_en = true;
	priv->mtk_dbgtp_sta.dispsys[2].subsys_smi_trig_en = true;
	priv->mtk_dbgtp_sta.dispsys[2].subsys_dsi_trig_en = true;
	priv->mtk_dbgtp_sta.dispsys[2].subsys_crossbar_info_en = true;
	priv->mtk_dbgtp_sta.dispsys[2].subsys_mon_info_en = true;
	priv->mtk_dbgtp_sta.dispsys[2].smi_mon[0].smi_mon_en = true;
	priv->mtk_dbgtp_sta.dispsys[2].smi_mon[0].rst_by_frame = true;
	priv->mtk_dbgtp_sta.dispsys[2].smi_mon[0].slice_time = 0x514;
	priv->mtk_dbgtp_sta.dispsys[2].smi_mon[0].smi_mon_portid[0] = 1;
	priv->mtk_dbgtp_sta.dispsys[2].smi_mon[0].smi_mon_portid[1] = 3;
	priv->mtk_dbgtp_sta.dispsys[2].smi_mon[0].smi_mon_portid[2] = 6;
	priv->mtk_dbgtp_sta.dispsys[2].smi_mon[0].smi_mon_portid[3] = 8;
	priv->mtk_dbgtp_sta.dispsys[2].crossbar_mon_cfg0 = 0x00000006;//PQ_OUT
	priv->mtk_dbgtp_sta.dispsys[2].crossbar_mon_cfg1 = 0x00000000;//PQ_IN
	priv->mtk_dbgtp_sta.dispsys[2].crossbar_mon_cfg2 = 0x00000002;//PC_OUT
	priv->mtk_dbgtp_sta.dispsys[2].crossbar_mon_cfg3 = 0x00010000;//PC_IN
	/* dispsys1B */
	priv->mtk_dbgtp_sta.dispsys[3].subsys_mon_en = true;
	priv->mtk_dbgtp_sta.dispsys[3].subsys_smi_trig_en = true;
	priv->mtk_dbgtp_sta.dispsys[3].subsys_dsi_trig_en = true;
	priv->mtk_dbgtp_sta.dispsys[3].subsys_crossbar_info_en = true;
	priv->mtk_dbgtp_sta.dispsys[3].subsys_mon_info_en = true;
	priv->mtk_dbgtp_sta.dispsys[3].smi_mon[0].smi_mon_en = true;
	priv->mtk_dbgtp_sta.dispsys[3].smi_mon[0].rst_by_frame = true;
	priv->mtk_dbgtp_sta.dispsys[3].smi_mon[0].slice_time = 0x514;
	priv->mtk_dbgtp_sta.dispsys[3].smi_mon[0].smi_mon_portid[0] = 0;
	priv->mtk_dbgtp_sta.dispsys[3].smi_mon[0].smi_mon_portid[1] = 1;
	priv->mtk_dbgtp_sta.dispsys[3].smi_mon[0].smi_mon_portid[2] = 5;
	priv->mtk_dbgtp_sta.dispsys[3].smi_mon[0].smi_mon_portid[3] = 6;
	priv->mtk_dbgtp_sta.dispsys[3].dsi_mon.dsi_mon_en = true;
	priv->mtk_dbgtp_sta.dispsys[3].dsi_mon.dsi_mon_sel = 0xFFFF;
	priv->mtk_dbgtp_sta.dispsys[3].dsi_mon.dsi_buf_sel = 0xFFFF;
	priv->mtk_dbgtp_sta.dispsys[3].crossbar_mon_cfg0 = 0x00090000;//SPLITTER_OUT
	priv->mtk_dbgtp_sta.dispsys[3].crossbar_mon_cfg1 = 0x00000000;//SPLITTER_IN
	priv->mtk_dbgtp_sta.dispsys[3].crossbar_mon_cfg2 = 0x00000002;//MERGE_OUT
	priv->mtk_dbgtp_sta.dispsys[3].crossbar_mon_cfg3 = 0x00000000;//INSIDE_PC
	priv->mtk_dbgtp_sta.dispsys[3].crossbar_mon_cfg4 = 0x00000000;//COMP_OUT

	/* ovlsys default setting */
	/* ovlsys0 */
	priv->mtk_dbgtp_sta.ovlsys[0].subsys_mon_en = true;
	priv->mtk_dbgtp_sta.ovlsys[0].subsys_smi_trig_en = true;
	priv->mtk_dbgtp_sta.ovlsys[0].subsys_crossbar_info_en = true;
	priv->mtk_dbgtp_sta.ovlsys[0].subsys_inlinerotate_info_en = true;
	priv->mtk_dbgtp_sta.ovlsys[0].subsys_mon_info_en = true;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[0].smi_mon_en = true;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[1].smi_mon_en = true;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[2].smi_mon_en = true;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[3].smi_mon_en = true;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[0].rst_by_frame = true;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[1].rst_by_frame = true;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[2].rst_by_frame = true;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[3].rst_by_frame = true;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[0].slice_time = 0x514;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[1].slice_time = 0x514;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[2].slice_time = 0x514;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[3].slice_time = 0x514;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[0].smi_mon_portid[0] = 2;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[0].smi_mon_cg_ctl[1] = 1;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[0].smi_mon_cg_ctl[2] = 1;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[0].smi_mon_cg_ctl[3] = 1;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[1].smi_mon_portid[0] = 2;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[1].smi_mon_cg_ctl[1] = 1;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[1].smi_mon_cg_ctl[2] = 1;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[1].smi_mon_cg_ctl[3] = 1;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[2].smi_mon_portid[0] = 2;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[2].smi_mon_cg_ctl[1] = 1;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[2].smi_mon_cg_ctl[2] = 1;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[2].smi_mon_cg_ctl[3] = 1;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[3].smi_mon_portid[0] = 2;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[3].smi_mon_cg_ctl[1] = 1;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[3].smi_mon_cg_ctl[2] = 1;
	priv->mtk_dbgtp_sta.ovlsys[0].smi_mon[3].smi_mon_cg_ctl[3] = 1;
	priv->mtk_dbgtp_sta.ovlsys[0].crossbar_mon_cfg0 = 0x00000002;//OVL_RSZ_IN
	priv->mtk_dbgtp_sta.ovlsys[0].crossbar_mon_cfg1 = 0x00000000;//OVL_PQ_IN
	priv->mtk_dbgtp_sta.ovlsys[0].crossbar_mon_cfg2 = 0x00000000;//OVL_OUTPROC_OUT
	priv->mtk_dbgtp_sta.ovlsys[0].crossbar_mon_cfg3 = 0x00060003;//OVL_EXDMA_OUT
	priv->mtk_dbgtp_sta.ovlsys[0].crossbar_mon_cfg4 = 0x00000006;//OVL_BLENDER_OUT
	/* ovlsys1 */
	priv->mtk_dbgtp_sta.ovlsys[1].subsys_mon_en = true;
	priv->mtk_dbgtp_sta.ovlsys[1].subsys_smi_trig_en = true;
	priv->mtk_dbgtp_sta.ovlsys[1].subsys_crossbar_info_en = true;
	priv->mtk_dbgtp_sta.ovlsys[1].subsys_inlinerotate_info_en = true;
	priv->mtk_dbgtp_sta.ovlsys[1].subsys_mon_info_en = true;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[0].smi_mon_en = true;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[1].smi_mon_en = true;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[2].smi_mon_en = true;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[3].smi_mon_en = true;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[0].rst_by_frame = true;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[1].rst_by_frame = true;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[2].rst_by_frame = true;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[3].rst_by_frame = true;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[0].slice_time = 0x514;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[1].slice_time = 0x514;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[2].slice_time = 0x514;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[3].slice_time = 0x514;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[0].smi_mon_portid[0] = 2;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[0].smi_mon_cg_ctl[1] = 1;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[0].smi_mon_cg_ctl[2] = 1;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[0].smi_mon_cg_ctl[3] = 1;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[1].smi_mon_portid[0] = 2;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[1].smi_mon_cg_ctl[1] = 1;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[1].smi_mon_cg_ctl[2] = 1;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[1].smi_mon_cg_ctl[3] = 1;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[2].smi_mon_portid[0] = 2;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[2].smi_mon_cg_ctl[1] = 1;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[2].smi_mon_cg_ctl[2] = 1;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[2].smi_mon_cg_ctl[3] = 1;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[3].smi_mon_portid[0] = 2;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[3].smi_mon_cg_ctl[1] = 1;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[3].smi_mon_cg_ctl[2] = 1;
	priv->mtk_dbgtp_sta.ovlsys[1].smi_mon[3].smi_mon_cg_ctl[3] = 1;
	priv->mtk_dbgtp_sta.ovlsys[1].crossbar_mon_cfg0 = 0x00000002;//OVL_RSZ_IN
	priv->mtk_dbgtp_sta.ovlsys[1].crossbar_mon_cfg1 = 0x00000003;//OVL_PQ_IN
	priv->mtk_dbgtp_sta.ovlsys[1].crossbar_mon_cfg2 = 0x00000000;//OVL_OUTPROC_OUT
	priv->mtk_dbgtp_sta.ovlsys[1].crossbar_mon_cfg3 = 0x00060003;//OVL_EXDMA_OUT
	priv->mtk_dbgtp_sta.ovlsys[1].crossbar_mon_cfg4 = 0x00000006;//OVL_BLENDER_OUT
	/* ovlsys2 */
	priv->mtk_dbgtp_sta.ovlsys[2].subsys_mon_en = true;
	priv->mtk_dbgtp_sta.ovlsys[2].subsys_smi_trig_en = true;
	priv->mtk_dbgtp_sta.ovlsys[2].subsys_crossbar_info_en = true;
	priv->mtk_dbgtp_sta.ovlsys[2].subsys_inlinerotate_info_en = true;
	priv->mtk_dbgtp_sta.ovlsys[2].subsys_mon_info_en = true;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[0].smi_mon_en = true;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[1].smi_mon_en = true;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[2].smi_mon_en = true;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[3].smi_mon_en = true;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[0].rst_by_frame = true;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[1].rst_by_frame = true;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[2].rst_by_frame = true;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[3].rst_by_frame = true;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[0].slice_time = 0x514;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[1].slice_time = 0x514;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[2].slice_time = 0x514;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[3].slice_time = 0x514;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[0].smi_mon_portid[0] = 2;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[0].smi_mon_cg_ctl[1] = 1;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[0].smi_mon_cg_ctl[2] = 1;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[0].smi_mon_cg_ctl[3] = 1;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[1].smi_mon_portid[0] = 2;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[1].smi_mon_cg_ctl[1] = 1;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[1].smi_mon_cg_ctl[2] = 1;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[1].smi_mon_cg_ctl[3] = 1;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[2].smi_mon_portid[0] = 2;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[2].smi_mon_cg_ctl[1] = 1;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[2].smi_mon_cg_ctl[2] = 1;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[2].smi_mon_cg_ctl[3] = 1;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[3].smi_mon_portid[0] = 2;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[3].smi_mon_cg_ctl[1] = 1;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[3].smi_mon_cg_ctl[2] = 1;
	priv->mtk_dbgtp_sta.ovlsys[2].smi_mon[3].smi_mon_cg_ctl[3] = 1;
	priv->mtk_dbgtp_sta.ovlsys[2].crossbar_mon_cfg0 = 0x00000002;//OVL_RSZ_IN
	priv->mtk_dbgtp_sta.ovlsys[2].crossbar_mon_cfg1 = 0x00000003;//OVL_PQ_IN
	priv->mtk_dbgtp_sta.ovlsys[2].crossbar_mon_cfg2 = 0x00000000;//OVL_OUTPROC_OUT
	priv->mtk_dbgtp_sta.ovlsys[2].crossbar_mon_cfg3 = 0x00060003;//OVL_EXDMA_OUT
	priv->mtk_dbgtp_sta.ovlsys[2].crossbar_mon_cfg4 = 0x00000006;//OVL_BLENDER_OUT

	/* mmlsys default setting */
	/* mmlsys0 */
	priv->mtk_dbgtp_sta.mmlsys[0].subsys_mon_en = true;
	priv->mtk_dbgtp_sta.mmlsys[0].subsys_smi_trig_en = true;
	priv->mtk_dbgtp_sta.mmlsys[0].subsys_crossbar_info_en = true;
	priv->mtk_dbgtp_sta.mmlsys[0].subsys_inlinerotate_info_en = true;
	priv->mtk_dbgtp_sta.mmlsys[0].subsys_mon_info_en = true;
	priv->mtk_dbgtp_sta.mmlsys[0].smi_mon[0].smi_mon_en = true;
	priv->mtk_dbgtp_sta.mmlsys[0].smi_mon[0].rst_by_frame = true;
	priv->mtk_dbgtp_sta.mmlsys[0].smi_mon[0].slice_time = 0x514;
	priv->mtk_dbgtp_sta.mmlsys[0].smi_mon[0].smi_mon_portid[0] = 4;
	priv->mtk_dbgtp_sta.mmlsys[0].smi_mon[0].smi_mon_portid[1] = 5;
	priv->mtk_dbgtp_sta.mmlsys[0].smi_mon[0].smi_mon_portid[2] = 11;
	priv->mtk_dbgtp_sta.mmlsys[0].smi_mon[0].smi_mon_portid[3] = 12;
	priv->mtk_dbgtp_sta.mmlsys[0].crossbar_mon_cfg0 = 0x00000000;//MML_PQ_OUT
	priv->mtk_dbgtp_sta.mmlsys[0].crossbar_mon_cfg1 = 0x00000000;//MML_PQ_IN
	/* mmlsys1 */
	priv->mtk_dbgtp_sta.mmlsys[1].subsys_mon_en = true;
	priv->mtk_dbgtp_sta.mmlsys[1].subsys_smi_trig_en = true;
	priv->mtk_dbgtp_sta.mmlsys[1].subsys_crossbar_info_en = true;
	priv->mtk_dbgtp_sta.mmlsys[1].subsys_inlinerotate_info_en = true;
	priv->mtk_dbgtp_sta.mmlsys[1].subsys_mon_info_en = true;
	priv->mtk_dbgtp_sta.mmlsys[1].smi_mon[0].smi_mon_en = true;
	priv->mtk_dbgtp_sta.mmlsys[1].smi_mon[0].rst_by_frame = true;
	priv->mtk_dbgtp_sta.mmlsys[1].smi_mon[0].slice_time = 0x514;
	priv->mtk_dbgtp_sta.mmlsys[1].smi_mon[0].smi_mon_portid[0] = 4;
	priv->mtk_dbgtp_sta.mmlsys[1].smi_mon[0].smi_mon_portid[1] = 5;
	priv->mtk_dbgtp_sta.mmlsys[1].smi_mon[0].smi_mon_portid[2] = 11;
	priv->mtk_dbgtp_sta.mmlsys[1].smi_mon[0].smi_mon_portid[3] = 12;
	priv->mtk_dbgtp_sta.mmlsys[1].crossbar_mon_cfg0 = 0x00000000;//MML_PQ_OUT
	priv->mtk_dbgtp_sta.mmlsys[1].crossbar_mon_cfg1 = 0x00000000;//MML_PQ_IN
	/* mmlsys2 */
	priv->mtk_dbgtp_sta.mmlsys[2].subsys_mon_en = true;
	priv->mtk_dbgtp_sta.mmlsys[2].subsys_smi_trig_en = true;
	priv->mtk_dbgtp_sta.mmlsys[2].subsys_crossbar_info_en = true;
	priv->mtk_dbgtp_sta.mmlsys[2].subsys_inlinerotate_info_en = true;
	priv->mtk_dbgtp_sta.mmlsys[2].subsys_mon_info_en = true;
	priv->mtk_dbgtp_sta.mmlsys[2].smi_mon[0].smi_mon_en = true;
	priv->mtk_dbgtp_sta.mmlsys[2].smi_mon[1].smi_mon_en = true;
	priv->mtk_dbgtp_sta.mmlsys[2].smi_mon[0].rst_by_frame = true;
	priv->mtk_dbgtp_sta.mmlsys[2].smi_mon[1].rst_by_frame = true;
	priv->mtk_dbgtp_sta.mmlsys[2].smi_mon[0].slice_time = 0x514;
	priv->mtk_dbgtp_sta.mmlsys[2].smi_mon[1].slice_time = 0x514;
	priv->mtk_dbgtp_sta.mmlsys[2].smi_mon[0].smi_mon_portid[0] = 0;
	priv->mtk_dbgtp_sta.mmlsys[2].smi_mon[0].smi_mon_portid[1] = 1;
	priv->mtk_dbgtp_sta.mmlsys[2].smi_mon[0].smi_mon_portid[2] = 2;
	priv->mtk_dbgtp_sta.mmlsys[2].smi_mon[0].smi_mon_portid[3] = 3;
	priv->mtk_dbgtp_sta.mmlsys[2].smi_mon[1].smi_mon_portid[0] = 0;
	priv->mtk_dbgtp_sta.mmlsys[2].smi_mon[1].smi_mon_portid[1] = 1;
	priv->mtk_dbgtp_sta.mmlsys[2].smi_mon[1].smi_mon_portid[2] = 2;
	priv->mtk_dbgtp_sta.mmlsys[2].smi_mon[1].smi_mon_portid[3] = 3;
	priv->mtk_dbgtp_sta.mmlsys[2].crossbar_mon_cfg0 = 0x00000000;//MML_PQ_OUT
	priv->mtk_dbgtp_sta.mmlsys[2].crossbar_mon_cfg1 = 0x00000000;//MML_PQ_IN

	DDPMSG("%s:%d -\n", __func__, __LINE__);
}

void mtk_dbgtp_dispsys_smi_mon_config(struct mtk_drm_crtc *mtk_crtc, struct cmdq_pkt *cmdq_handle,
		unsigned int dispsys_num, resource_size_t config_regs_pa, void __iomem *config_regs)
{
	unsigned int value = 0;
	unsigned int j = 0;
	bool smi_mon_en = 0;
	bool rst_by_frame = 0;
	unsigned int slice_time = 0;
	unsigned int smi_mon_dump_sel = 0;
	unsigned int smi_mon_portid = 0;
	unsigned int smi_mon_cg_ctl = 0;
	unsigned int mon_con_addr = 0;
	unsigned int mon_sel_addr = 0;
	struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;
	int i = dispsys_num;
	unsigned int val = 0;
	unsigned int mask = 0;

	if ((i == 0) || (i == 3)) {
		mon_con_addr = DISPSYS_DISP_SMI_DBG_MON_LARB0_CON0;
		mon_sel_addr = DISPSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL;
	} else {
		mon_con_addr = DISPSYS1_DISP_SMI_DBG_MON_LARB0_CON0;
		mon_sel_addr = DISPSYS1_DISP_SMI_DBG_MON_LARB0_MON0_SEL;
	}

	/* Enable & Config smi mon */
	smi_mon_en = priv->mtk_dbgtp_sta.dispsys[i].smi_mon[0].smi_mon_en;
	rst_by_frame = priv->mtk_dbgtp_sta.dispsys[i].smi_mon[0].rst_by_frame;
	slice_time = priv->mtk_dbgtp_sta.dispsys[i].smi_mon[0].slice_time;
	smi_mon_dump_sel = priv->mtk_dbgtp_sta.dispsys[i].smi_mon[0].smi_mon_dump_sel;
	if (smi_mon_en) {
		value = (REG_FLD_VAL((SMI_MON_DUMP_SEL), smi_mon_dump_sel) |
			REG_FLD_VAL((SMI_MON_RST_BY_FRAME), rst_by_frame) |
			REG_FLD_VAL((SMI_MON_SLICE_TIME), slice_time) |
			REG_FLD_VAL((SMI_MON_ENABLE), smi_mon_en));
		mask = REG_FLD_MASK(SMI_MON_DUMP_SEL) |
			REG_FLD_MASK(SMI_MON_RST_BY_FRAME) |
			REG_FLD_MASK(SMI_MON_SLICE_TIME) |
			REG_FLD_MASK(SMI_MON_ENABLE);
		if (cmdq_handle == NULL) {
			val = readl(config_regs + mon_con_addr);
			writel((val & ~mask) | value, config_regs + mon_con_addr);
		} else
			cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
				config_regs_pa + mon_con_addr, value, mask);

		for (j = 0; j < SMI_MON_PORT_NUM; j++) {
			smi_mon_portid = priv->mtk_dbgtp_sta.dispsys[i].smi_mon[0].smi_mon_portid[j];
			smi_mon_cg_ctl = priv->mtk_dbgtp_sta.dispsys[i].smi_mon[0].smi_mon_cg_ctl[j];
			value = (REG_FLD_VAL((SMI_MON_PORT_ID), smi_mon_portid) |
				REG_FLD_VAL((SMI_MON_PORT_CG_CTL), smi_mon_cg_ctl));
			mask = REG_FLD_MASK(SMI_MON_PORT_ID) |
				REG_FLD_MASK(SMI_MON_PORT_CG_CTL);
			if (cmdq_handle == NULL) {
				val = readl(config_regs + mon_sel_addr + j * 0x8);
				writel((val & ~mask) | value, config_regs + mon_sel_addr + j * 0x8);
			} else
				cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
					config_regs_pa + mon_sel_addr +
					j * 0x8, value, mask);
		}
	} else {
		for (j = 0; j < SMI_MON_PORT_NUM; j++) {
			/* reset smi port monitor, set 1 set 0 */
			value = (REG_FLD_VAL((SMI_MON_PORT_RST), 1));
			mask = REG_FLD_MASK(SMI_MON_PORT_RST);
			if (cmdq_handle == NULL) {
				val = readl(config_regs + mon_sel_addr + j * 0x8);
				writel((val & ~mask) | value, config_regs + mon_sel_addr + j * 0x8);
			} else
				cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
					config_regs_pa + mon_sel_addr +
					j * 0x8, value, mask);
			value = (REG_FLD_VAL((SMI_MON_PORT_RST), 0));
			if (cmdq_handle == NULL) {
				val = readl(config_regs + mon_sel_addr + j * 0x8);
				writel((val & ~mask) | value, config_regs + mon_sel_addr + j * 0x8);
			} else
				cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
					config_regs_pa + mon_sel_addr +
					j * 0x8, value, mask);
		}
		/* Disable SMI mon */
		value = (REG_FLD_VAL((SMI_MON_ENABLE), 0));
		mask = REG_FLD_MASK(SMI_MON_ENABLE);
		if (cmdq_handle == NULL) {
			val = readl(config_regs + mon_con_addr);
			writel((val & ~mask) | value, config_regs + mon_con_addr);
		} else
			cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
				config_regs_pa + mon_con_addr, value, mask);
	}
}

void mtk_dbgtp_dispsys_config(struct mtk_drm_crtc *mtk_crtc, struct cmdq_pkt *cmdq_handle)
{
	unsigned int value = 0;
	unsigned int i = 0;
	bool dbgtp_dispsys_en = false;
	bool dbgtp_dispsys_update = false;
	resource_size_t config_regs_pa = 0;
	void __iomem *config_regs = NULL;
	bool subsys_smi_trig_en = 0;
	bool subsys_dsi_trig_en = 0;
	bool subsys_inlinerotate_info_en = 0;
	bool subsys_crossbar_info_en = 0;
	bool subsys_mon_info_en = 0;
	unsigned int crossbar_mon_cfg0 = 0;
	unsigned int crossbar_mon_cfg1 = 0;
	unsigned int crossbar_mon_cfg2 = 0;
	unsigned int crossbar_mon_cfg3 = 0;
	unsigned int crossbar_mon_cfg4 = 0;
	struct mtk_ddp_comp *output_comp = NULL;
	bool dsi_mon_en = 0;
	bool dsi_mon_reset_byf = 0;
	unsigned int dsi_mon_sel = 0;
	unsigned int dsi_buf_sel = 0;
	unsigned int dsi_tgt_pix = 0;
	struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;
	unsigned int val = 0;
	unsigned int mask = 0;

	for (i = 0; i < DISPSYS_NUM; i++) {
		dbgtp_dispsys_en = priv->mtk_dbgtp_sta.dispsys[i].subsys_mon_en;
		dbgtp_dispsys_update = priv->mtk_dbgtp_sta.dispsys[i].need_update;
		subsys_smi_trig_en = priv->mtk_dbgtp_sta.dispsys[i].subsys_smi_trig_en;
		subsys_dsi_trig_en = priv->mtk_dbgtp_sta.dispsys[i].subsys_dsi_trig_en;
		subsys_inlinerotate_info_en = priv->mtk_dbgtp_sta.dispsys[i].subsys_inlinerotate_info_en;
		subsys_crossbar_info_en = priv->mtk_dbgtp_sta.dispsys[i].subsys_crossbar_info_en;
		subsys_mon_info_en = priv->mtk_dbgtp_sta.dispsys[i].subsys_mon_info_en;
		crossbar_mon_cfg0 = priv->mtk_dbgtp_sta.dispsys[i].crossbar_mon_cfg0;
		crossbar_mon_cfg1 = priv->mtk_dbgtp_sta.dispsys[i].crossbar_mon_cfg1;
		crossbar_mon_cfg2 = priv->mtk_dbgtp_sta.dispsys[i].crossbar_mon_cfg2;
		crossbar_mon_cfg3 = priv->mtk_dbgtp_sta.dispsys[i].crossbar_mon_cfg3;
		crossbar_mon_cfg4 = priv->mtk_dbgtp_sta.dispsys[i].crossbar_mon_cfg4;
		dsi_mon_en = priv->mtk_dbgtp_sta.dispsys[i].dsi_mon.dsi_mon_en;
		dsi_mon_reset_byf = priv->mtk_dbgtp_sta.dispsys[i].dsi_mon.dsi_mon_reset_byf;
		dsi_mon_sel = priv->mtk_dbgtp_sta.dispsys[i].dsi_mon.dsi_mon_sel;
		dsi_buf_sel = priv->mtk_dbgtp_sta.dispsys[i].dsi_mon.dsi_buf_sel;
		dsi_tgt_pix = priv->mtk_dbgtp_sta.dispsys[i].dsi_mon.dsi_tgt_pix;

		if (i == 0) {
			config_regs_pa = priv->config_regs_pa;
			config_regs = priv->config_regs;
		}
		if (i == 1) {
			config_regs_pa = priv->side_config_regs_pa;
			config_regs = priv->side_config_regs;
		}
		if (i == 2) {
			config_regs_pa = priv->sys_b_config_regs_pa;
			config_regs = priv->sys_b_config_regs;
		}
		if (i == 3) {
			config_regs_pa = priv->sys_b_side_config_regs_pa;
			config_regs = priv->sys_b_side_config_regs;
		}

		if ((config_regs_pa <= 0) || config_regs == NULL)
			continue;

		if (dbgtp_dispsys_update && ((i == 0) || (i == 2))) {
			if (dbgtp_dispsys_en) {
				DDPDBG("%s:%d dispsys%d enable\n", __func__, __LINE__, i);

				/* Enable & config subsys - dispsys 0A-0B */
				value = (REG_FLD_VAL((SUBSYS_SMI_TRIG_EN), subsys_smi_trig_en) |
					REG_FLD_VAL((SUBSYS_DSI_TRIG_EN), subsys_dsi_trig_en) |
					REG_FLD_VAL((SUBSYS_INLINEROTATE_INFO_EN), subsys_inlinerotate_info_en) |
					REG_FLD_VAL((SUBSYS_CROSSBAR_INFO_EN), subsys_crossbar_info_en) |
					REG_FLD_VAL((SUBSYS_MON_INFO_EN), subsys_mon_info_en) |
					REG_FLD_VAL((SUBSYS_MON_ENGINE_EN), dbgtp_dispsys_en));
				mask = REG_FLD_MASK(SUBSYS_SMI_TRIG_EN) |
					REG_FLD_MASK(SUBSYS_DSI_TRIG_EN) |
					REG_FLD_MASK(SUBSYS_INLINEROTATE_INFO_EN) |
					REG_FLD_MASK(SUBSYS_CROSSBAR_INFO_EN) |
					REG_FLD_MASK(SUBSYS_MON_INFO_EN) |
					REG_FLD_MASK(SUBSYS_MON_ENGINE_EN);
				if (cmdq_handle == NULL) {
					val = readl(config_regs + DISPSYS_DEBUG_SUBSYS);
					writel((val & ~mask) | value, config_regs + DISPSYS_DEBUG_SUBSYS);
				} else
					cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						config_regs_pa + DISPSYS_DEBUG_SUBSYS, value, mask);

				/* Config crossbar mon */
				if (cmdq_handle == NULL) {
					writel(crossbar_mon_cfg0, config_regs + PQ_OUT_CROSSBAR_DEBUG_MONITOR_PTR);
					writel(crossbar_mon_cfg1, config_regs + PQ_IN_CROSSBAR_DEBUG_MONITOR_PTR);
					writel(crossbar_mon_cfg2, config_regs + PC_OUT_CROSSBAR_DEBUG_MONITOR_PTR);
					writel(crossbar_mon_cfg3, config_regs + PC_IN_CROSSBAR_DEBUG_MONITOR_PTR);
				} else {
					cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						config_regs_pa + PQ_OUT_CROSSBAR_DEBUG_MONITOR_PTR,
						crossbar_mon_cfg0, ~0);
					cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						config_regs_pa + PQ_IN_CROSSBAR_DEBUG_MONITOR_PTR,
						crossbar_mon_cfg1, ~0);
					cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						config_regs_pa + PC_OUT_CROSSBAR_DEBUG_MONITOR_PTR,
						crossbar_mon_cfg2, ~0);
					cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						config_regs_pa + PC_IN_CROSSBAR_DEBUG_MONITOR_PTR,
						crossbar_mon_cfg3, ~0);
				}

				/* Enable & config SMI monitor */
				mtk_dbgtp_dispsys_smi_mon_config(mtk_crtc, cmdq_handle, i, config_regs_pa, config_regs);

				/* Enable & config DSI monitor */
				output_comp =
					mtk_ddp_comp_request_output(mtk_crtc);
				value = (REG_FLD_VAL((DSI_MON_EN), dsi_mon_en) |
					REG_FLD_VAL((DSI_MON_RST_BYF), dsi_mon_reset_byf));
				mtk_ddp_comp_io_cmd(output_comp, cmdq_handle, DSI_MON_CFG0, &value);
				value = (REG_FLD_VAL((DSI_MON_SEL_DBG), dsi_mon_sel) |
					REG_FLD_VAL((DSI_MON_SEL_BUF), dsi_buf_sel));
				mtk_ddp_comp_io_cmd(output_comp, cmdq_handle, DSI_MON_CFG1, &value);
				value = (REG_FLD_VAL((DSI_MON_TGT_PIX), dsi_tgt_pix));
				mtk_ddp_comp_io_cmd(output_comp, cmdq_handle, DSI_MON_CFG2, &value);

				priv->mtk_dbgtp_sta.dispsys[i].need_update = false;
			} else {
				DDPDBG("%s:%d dispsys%d disable\n", __func__, __LINE__, i);

				if (cmdq_handle == NULL) {
					value = (REG_FLD_VAL((SUBSYS_MON_ENGINE_RST), 0x1));
					mask = REG_FLD_MASK(SUBSYS_MON_ENGINE_RST);
					val = readl(config_regs +  DISPSYS_DEBUG_SUBSYS);
					writel((val & ~mask) | value, config_regs + DISPSYS_DEBUG_SUBSYS);

					value = (REG_FLD_VAL((SUBSYS_MON_ENGINE_RST), 0x0));
					val = readl(config_regs +  DISPSYS_DEBUG_SUBSYS);
					writel((val & ~mask) | value, config_regs + DISPSYS_DEBUG_SUBSYS);

					value = (REG_FLD_VAL((SUBSYS_MON_ENGINE_EN), 0x0));
					mask = REG_FLD_MASK(SUBSYS_MON_ENGINE_EN);
					val = readl(config_regs +  DISPSYS_DEBUG_SUBSYS);
					writel((val & ~mask) | value, config_regs + DISPSYS_DEBUG_SUBSYS);
				} else {
					/* Reset subsys */
					value = (REG_FLD_VAL((SUBSYS_MON_ENGINE_RST), 0x1));
					cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						config_regs_pa + DISPSYS_DEBUG_SUBSYS, value,
						REG_FLD_MASK(SUBSYS_MON_ENGINE_RST));
					value = (REG_FLD_VAL((SUBSYS_MON_ENGINE_RST), 0x0));
					cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						config_regs_pa + DISPSYS_DEBUG_SUBSYS, value,
						REG_FLD_MASK(SUBSYS_MON_ENGINE_RST));
					/* Disable subsys engine */
					value = (REG_FLD_VAL((SUBSYS_MON_ENGINE_EN), 0x0));
					cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						config_regs_pa + DISPSYS_DEBUG_SUBSYS, value,
						REG_FLD_MASK(SUBSYS_MON_ENGINE_EN));
				}

				/* Disable SMI monitor */
				mtk_dbgtp_dispsys_smi_mon_config(mtk_crtc, cmdq_handle,
					i, config_regs_pa, config_regs);

				/* Disable DSI monitor */
				output_comp =
					mtk_ddp_comp_request_output(mtk_crtc);
				value = 0;
				mtk_ddp_comp_io_cmd(output_comp, cmdq_handle, DSI_MON_DSI_RST, &value);

				priv->mtk_dbgtp_sta.dispsys[i].need_update = false;
			}
		}

		if (dbgtp_dispsys_update && ((i == 1) || (i == 3))) {
			if (dbgtp_dispsys_en) {
				DDPDBG("%s:%d dispsys%d enable\n", __func__, __LINE__, i);

				/* Enable & config subsys - dispsys 1A-1B */
				value = (REG_FLD_VAL((SUBSYS_SMI_TRIG_EN), subsys_smi_trig_en) |
					REG_FLD_VAL((SUBSYS_DSI_TRIG_EN), subsys_dsi_trig_en) |
					REG_FLD_VAL((SUBSYS_INLINEROTATE_INFO_EN), subsys_inlinerotate_info_en) |
					REG_FLD_VAL((SUBSYS_CROSSBAR_INFO_EN), subsys_crossbar_info_en) |
					REG_FLD_VAL((SUBSYS_MON_INFO_EN), subsys_mon_info_en) |
					REG_FLD_VAL((SUBSYS_MON_ENGINE_EN), dbgtp_dispsys_en));
				mask = REG_FLD_MASK(SUBSYS_SMI_TRIG_EN) |
					REG_FLD_MASK(SUBSYS_DSI_TRIG_EN) |
					REG_FLD_MASK(SUBSYS_INLINEROTATE_INFO_EN) |
					REG_FLD_MASK(SUBSYS_CROSSBAR_INFO_EN) |
					REG_FLD_MASK(SUBSYS_MON_INFO_EN) |
					REG_FLD_MASK(SUBSYS_MON_ENGINE_EN);
				if (cmdq_handle == NULL) {
					val = readl(config_regs + DISPSYS1_DEBUG_SUBSYS);
					writel((val & ~mask) | value, config_regs + DISPSYS1_DEBUG_SUBSYS);
				} else
					cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						config_regs_pa + DISPSYS1_DEBUG_SUBSYS, value, mask);

				/* Config crossbar mon */
				if (cmdq_handle == NULL) {
					writel(crossbar_mon_cfg0, config_regs +
						SPLITTER_OUT_CROSSBAR_DEBUG_MONITOR_PTR);
					writel(crossbar_mon_cfg1, config_regs +
						SPLITTER_IN_CROSSBAR_DEBUG_MONITOR_PTR);
					writel(crossbar_mon_cfg2, config_regs +
						MERGE_OUT_CROSSBAR_DEBUG_MONITOR_PTR);
					writel(crossbar_mon_cfg3, config_regs +
						INSIDE_PC_CROSSBAR_DEBUG_MONITOR_PTR);
					writel(crossbar_mon_cfg4, config_regs +
						COMP_OUT_CROSSBAR_DEBUG_MONITOR_PTR);
				} else {
					cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						config_regs_pa + SPLITTER_OUT_CROSSBAR_DEBUG_MONITOR_PTR,
						crossbar_mon_cfg0, ~0);
					cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						config_regs_pa + SPLITTER_IN_CROSSBAR_DEBUG_MONITOR_PTR,
						crossbar_mon_cfg1, ~0);
					cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						config_regs_pa + MERGE_OUT_CROSSBAR_DEBUG_MONITOR_PTR,
						crossbar_mon_cfg2, ~0);
					cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						config_regs_pa + INSIDE_PC_CROSSBAR_DEBUG_MONITOR_PTR,
						crossbar_mon_cfg3, ~0);
					cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						config_regs_pa + COMP_OUT_CROSSBAR_DEBUG_MONITOR_PTR,
						crossbar_mon_cfg4, ~0);
				}

				/* Enable & config SMI monitor */
				mtk_dbgtp_dispsys_smi_mon_config(mtk_crtc, cmdq_handle,
					i, config_regs_pa, config_regs);

				/* Enable & config DSI monitor */
				output_comp =
					mtk_ddp_comp_request_output(mtk_crtc);
				value = (REG_FLD_VAL((DSI_MON_EN), dsi_mon_en) |
					REG_FLD_VAL((DSI_MON_RST_BYF), dsi_mon_reset_byf));
				mtk_ddp_comp_io_cmd(output_comp, cmdq_handle, DSI_MON_CFG0, &value);
				value = (REG_FLD_VAL((DSI_MON_SEL_DBG), dsi_mon_sel) |
					REG_FLD_VAL((DSI_MON_SEL_BUF), dsi_buf_sel));
				mtk_ddp_comp_io_cmd(output_comp, cmdq_handle, DSI_MON_CFG1, &value);
				value = (REG_FLD_VAL((DSI_MON_TGT_PIX), dsi_tgt_pix));
				mtk_ddp_comp_io_cmd(output_comp, cmdq_handle, DSI_MON_CFG2, &value);

				priv->mtk_dbgtp_sta.dispsys[i].need_update = false;
			} else {
				DDPDBG("%s:%d dispsys%d disable\n", __func__, __LINE__, i);

				if (cmdq_handle == NULL) {
					value = (REG_FLD_VAL((SUBSYS_MON_ENGINE_RST), 0x1));
					mask = REG_FLD_MASK(SUBSYS_MON_ENGINE_RST);
					val = readl(config_regs +  DISPSYS1_DEBUG_SUBSYS);
					writel((val & ~mask) | value, config_regs + DISPSYS1_DEBUG_SUBSYS);

					value = (REG_FLD_VAL((SUBSYS_MON_ENGINE_RST), 0x0));
					val = readl(config_regs +  DISPSYS1_DEBUG_SUBSYS);
					writel((val & ~mask) | value, config_regs + DISPSYS1_DEBUG_SUBSYS);

					value = (REG_FLD_VAL((SUBSYS_MON_ENGINE_EN), 0x0));
					mask = REG_FLD_MASK(SUBSYS_MON_ENGINE_EN);
					val = readl(config_regs +  DISPSYS1_DEBUG_SUBSYS);
					writel((val & ~mask) | value, config_regs + DISPSYS1_DEBUG_SUBSYS);
				} else {
					/* Reset subsys */
					value = (REG_FLD_VAL((SUBSYS_MON_ENGINE_RST), 0x1));
					cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						config_regs_pa + DISPSYS1_DEBUG_SUBSYS, value,
						REG_FLD_MASK(SUBSYS_MON_ENGINE_RST));
					value = (REG_FLD_VAL((SUBSYS_MON_ENGINE_RST), 0x0));
					cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						config_regs_pa + DISPSYS1_DEBUG_SUBSYS, value,
						REG_FLD_MASK(SUBSYS_MON_ENGINE_RST));
					/* Disable subsys engine */
					value = (REG_FLD_VAL((SUBSYS_MON_ENGINE_EN), 0x0));
					cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						config_regs_pa + DISPSYS1_DEBUG_SUBSYS, value,
						REG_FLD_MASK(SUBSYS_MON_ENGINE_EN));
				}

				/* Disable SMI monitor */
				mtk_dbgtp_dispsys_smi_mon_config(mtk_crtc, cmdq_handle,
					i, config_regs_pa, config_regs);

				/* Disable DSI monitor */
				output_comp =
					mtk_ddp_comp_request_output(mtk_crtc);
				value = 0;
				mtk_ddp_comp_io_cmd(output_comp, cmdq_handle, DSI_MON_DSI_RST, &value);

				priv->mtk_dbgtp_sta.dispsys[i].need_update = false;
			}
		}
	}
}

void mtk_dbgtp_ovlsys_smi_mon_config(struct mtk_drm_crtc *mtk_crtc, struct cmdq_pkt *cmdq_handle,
	struct dbgtp_subsys *ovlsys, resource_size_t config_regs_pa, void __iomem *config_regs)
{
	unsigned int value = 0;
	unsigned int i = 0;
	unsigned int j = 0;
	bool smi_mon_en = 0;
	bool rst_by_frame = 0;
	unsigned int slice_time = 0;
	unsigned int smi_mon_dump_sel = 0;
	unsigned int smi_mon_portid = 0;
	unsigned int smi_mon_cg_ctl = 0;
	unsigned int val = 0;
	unsigned int mask = 0;

	for (i = 0; i < MAX_SMI_MON_NUM; i++) {
		smi_mon_en = ovlsys->smi_mon[i].smi_mon_en;
		rst_by_frame = ovlsys->smi_mon[i].rst_by_frame;
		slice_time = ovlsys->smi_mon[i].slice_time;
		smi_mon_dump_sel = ovlsys->smi_mon[i].smi_mon_dump_sel;
		if (smi_mon_en) {
			/* Enable & Config smi mon */
			value = (REG_FLD_VAL((SMI_MON_DUMP_SEL), smi_mon_dump_sel) |
				REG_FLD_VAL((SMI_MON_RST_BY_FRAME), rst_by_frame) |
				REG_FLD_VAL((SMI_MON_SLICE_TIME), slice_time) |
				REG_FLD_VAL((SMI_MON_ENABLE), smi_mon_en));
			mask = REG_FLD_MASK(SMI_MON_DUMP_SEL) |
				REG_FLD_MASK(SMI_MON_RST_BY_FRAME) |
				REG_FLD_MASK(SMI_MON_SLICE_TIME) |
				REG_FLD_MASK(SMI_MON_ENABLE);
			if (cmdq_handle == NULL) {
				val = readl(config_regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_CON0 + i * 0x24);
				writel((val & ~mask) | value,
					config_regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_CON0 + i * 0x24);
			} else
				cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
					config_regs_pa + OVLSYS_DISP_SMI_DBG_MON_LARB0_CON0 + i * 0x24 , value, mask);

			for (j = 0; j < SMI_MON_PORT_NUM; j++) {
				smi_mon_portid = ovlsys->smi_mon[i].smi_mon_portid[j];
				smi_mon_cg_ctl = ovlsys->smi_mon[i].smi_mon_cg_ctl[j];
				value = (REG_FLD_VAL((SMI_MON_PORT_ID), smi_mon_portid) |
					REG_FLD_VAL((SMI_MON_PORT_CG_CTL), smi_mon_cg_ctl));
				mask = REG_FLD_MASK(SMI_MON_PORT_ID) |
					REG_FLD_MASK(SMI_MON_PORT_CG_CTL);
				if (cmdq_handle == NULL) {
					val = readl(config_regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL +
						j * 0x8 + i * 0x24);
					writel((val & ~mask) | value, config_regs +
						OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL +
						j * 0x8 + i * 0x24);
				} else
					cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						config_regs_pa + OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL +
						j * 0x8 + i * 0x24, value, mask);
			}
		} else {
			for (j = 0; j < SMI_MON_PORT_NUM; j++) {
				/* reset smi port monitor, set 1 set 0 */
				value = (REG_FLD_VAL((SMI_MON_PORT_RST), 1));
				mask = REG_FLD_MASK(SMI_MON_PORT_RST);
				if (cmdq_handle == NULL) {
					val = readl(config_regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL +
						j * 0x8 + i * 0x24);
					writel((val & ~mask) | value, config_regs +
						OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL +
						j * 0x8 + i * 0x24);
				} else
					cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						config_regs_pa + OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL +
						j * 0x8 + i * 0x24, value, mask);

				value = (REG_FLD_VAL((SMI_MON_PORT_RST), 0));
				if (cmdq_handle == NULL) {
					val = readl(config_regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL +
						j * 0x8 + i * 0x24);
					writel((val & ~mask) | value, config_regs +
						OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL +
						j * 0x8 + i * 0x24);
				} else
					cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						config_regs_pa + OVLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL +
						j * 0x8 + i * 0x24, value, mask);
			}
			/* Disable SMI mon */
			value = (REG_FLD_VAL((SMI_MON_ENABLE), 0));
			mask = REG_FLD_MASK(SMI_MON_ENABLE);
			if (cmdq_handle == NULL) {
				val = readl(config_regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_CON0 + i * 0x24);
				writel((val & ~mask) | value,
						config_regs + OVLSYS_DISP_SMI_DBG_MON_LARB0_CON0 + i * 0x24);
			} else
				cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
					config_regs_pa + OVLSYS_DISP_SMI_DBG_MON_LARB0_CON0 +
					i * 0x24, value, mask);
		}
	}
}


void mtk_dbgtp_ovlsys_config(struct mtk_drm_crtc *mtk_crtc, struct cmdq_pkt *cmdq_handle)
{
	unsigned int value = 0;
	unsigned int i = 0;
	bool dbgtp_ovlsys_en = false;
	bool dbgtp_ovlsys_update = false;
	resource_size_t config_regs_pa = 0;
	void __iomem *config_regs = NULL;
	bool subsys_smi_trig_en = 0;
	bool subsys_dsi_trig_en = 0;
	bool subsys_inlinerotate_info_en = 0;
	bool subsys_crossbar_info_en = 0;
	bool subsys_mon_info_en = 0;
	unsigned int crossbar_mon_cfg0 = 0;
	unsigned int crossbar_mon_cfg1 = 0;
	unsigned int crossbar_mon_cfg2 = 0;
	unsigned int crossbar_mon_cfg3 = 0;
	unsigned int crossbar_mon_cfg4 = 0;
	struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;
	unsigned int val = 0;
	unsigned int mask = 0;

	for (i = 0; i < OVLSYS_NUM; i++) {
		dbgtp_ovlsys_en = priv->mtk_dbgtp_sta.ovlsys[i].subsys_mon_en;
		dbgtp_ovlsys_update = priv->mtk_dbgtp_sta.ovlsys[i].need_update;
		subsys_smi_trig_en = priv->mtk_dbgtp_sta.ovlsys[i].subsys_smi_trig_en;
		subsys_dsi_trig_en = priv->mtk_dbgtp_sta.ovlsys[i].subsys_dsi_trig_en;
		subsys_inlinerotate_info_en = priv->mtk_dbgtp_sta.ovlsys[i].subsys_inlinerotate_info_en;
		subsys_crossbar_info_en = priv->mtk_dbgtp_sta.ovlsys[i].subsys_crossbar_info_en;
		subsys_mon_info_en = priv->mtk_dbgtp_sta.ovlsys[i].subsys_mon_info_en;
		crossbar_mon_cfg0 = priv->mtk_dbgtp_sta.ovlsys[i].crossbar_mon_cfg0;
		crossbar_mon_cfg1 = priv->mtk_dbgtp_sta.ovlsys[i].crossbar_mon_cfg1;
		crossbar_mon_cfg2 = priv->mtk_dbgtp_sta.ovlsys[i].crossbar_mon_cfg2;
		crossbar_mon_cfg3 = priv->mtk_dbgtp_sta.ovlsys[i].crossbar_mon_cfg3;
		crossbar_mon_cfg4 = priv->mtk_dbgtp_sta.ovlsys[i].crossbar_mon_cfg4;

		if (i == 0) {
			config_regs_pa = priv->ovlsys0_regs_pa;
			config_regs = priv->ovlsys0_regs;
		}
		if (i == 1) {
			config_regs_pa = priv->ovlsys1_regs_pa;
			config_regs = priv->ovlsys1_regs;
		}
		if (i == 2) {
			config_regs_pa = priv->ovlsys2_regs_pa;
			config_regs = priv->ovlsys2_regs;
		}

		if ((config_regs_pa <= 0) || config_regs == NULL)
			continue;

		if (dbgtp_ovlsys_update) {
			if (dbgtp_ovlsys_en) {
				/* Enable & config subsys - ovlsys 0,1,2 */
				value = (REG_FLD_VAL((SUBSYS_SMI_TRIG_EN), subsys_smi_trig_en) |
					REG_FLD_VAL((SUBSYS_DSI_TRIG_EN), subsys_dsi_trig_en) |
					REG_FLD_VAL((SUBSYS_INLINEROTATE_INFO_EN), subsys_inlinerotate_info_en) |
					REG_FLD_VAL((SUBSYS_CROSSBAR_INFO_EN), subsys_crossbar_info_en) |
					REG_FLD_VAL((SUBSYS_MON_INFO_EN), subsys_mon_info_en) |
					REG_FLD_VAL((SUBSYS_MON_ENGINE_EN), dbgtp_ovlsys_en));
				mask = REG_FLD_MASK(SUBSYS_SMI_TRIG_EN) |
					REG_FLD_MASK(SUBSYS_DSI_TRIG_EN) |
					REG_FLD_MASK(SUBSYS_INLINEROTATE_INFO_EN) |
					REG_FLD_MASK(SUBSYS_CROSSBAR_INFO_EN) |
					REG_FLD_MASK(SUBSYS_MON_INFO_EN) |
					REG_FLD_MASK(SUBSYS_MON_ENGINE_EN);
				if (cmdq_handle == NULL) {
					val = readl(config_regs + OVLSYS_DEBUG_SUBSYS);
					writel((val & ~mask) | value, config_regs + OVLSYS_DEBUG_SUBSYS);
				} else
					cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						config_regs_pa + OVLSYS_DEBUG_SUBSYS, value, mask);

				/* Config crossbar mon */
				if (cmdq_handle == NULL) {
					writel(crossbar_mon_cfg0, config_regs +
						OVL_RSZ_IN_CROSSBAR_DEBUG_MONITOR_PTR);
					writel(crossbar_mon_cfg1, config_regs +
						OVL_PQ_IN_CROSSBAR_DEBUG_MONITOR_PTR);
					writel(crossbar_mon_cfg2, config_regs +
						OVL_OUTPROC_OUT_CROSSBAR_DEBUG_MONITOR_PTR);
					writel(crossbar_mon_cfg3, config_regs +
						OVL_EXDMA_OUT_CROSSBAR_DEBUG_MONITOR_PTR);
					writel(crossbar_mon_cfg4, config_regs +
						OVL_BLENDER_OUT_CROSSBAR_DEBUG_MONITOR_PTR);
				} else {
					cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						config_regs_pa + OVL_RSZ_IN_CROSSBAR_DEBUG_MONITOR_PTR,
						crossbar_mon_cfg0, ~0);
					cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						config_regs_pa + OVL_PQ_IN_CROSSBAR_DEBUG_MONITOR_PTR,
						crossbar_mon_cfg1, ~0);
					cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						config_regs_pa + OVL_OUTPROC_OUT_CROSSBAR_DEBUG_MONITOR_PTR,
						crossbar_mon_cfg2, ~0);
					cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						config_regs_pa + OVL_EXDMA_OUT_CROSSBAR_DEBUG_MONITOR_PTR,
						crossbar_mon_cfg3, ~0);
					cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						config_regs_pa + OVL_BLENDER_OUT_CROSSBAR_DEBUG_MONITOR_PTR,
						crossbar_mon_cfg4, ~0);
				}

				/* Enable & config smi monitor */
				mtk_dbgtp_ovlsys_smi_mon_config(mtk_crtc, cmdq_handle,
					&priv->mtk_dbgtp_sta.ovlsys[i], config_regs_pa, config_regs);

				priv->mtk_dbgtp_sta.ovlsys[i].need_update = false;
			} else {
				if (cmdq_handle == NULL) {
					value = (REG_FLD_VAL((SUBSYS_MON_ENGINE_RST), 0x1));
					mask = REG_FLD_MASK(SUBSYS_MON_ENGINE_RST);
					val = readl(config_regs +  OVLSYS_DEBUG_SUBSYS);
					writel((val & ~mask) | value, config_regs + OVLSYS_DEBUG_SUBSYS);

					value = (REG_FLD_VAL((SUBSYS_MON_ENGINE_RST), 0x0));
					val = readl(config_regs +  OVLSYS_DEBUG_SUBSYS);
					writel((val & ~mask) | value, config_regs + OVLSYS_DEBUG_SUBSYS);

					value = (REG_FLD_VAL((SUBSYS_MON_ENGINE_EN), 0x0));
					mask = REG_FLD_MASK(SUBSYS_MON_ENGINE_EN);
					val = readl(config_regs +  OVLSYS_DEBUG_SUBSYS);
					writel((val & ~mask) | value, config_regs + OVLSYS_DEBUG_SUBSYS);
				} else {
					/* Reset subsys */
					cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						config_regs_pa + OVLSYS_DEBUG_SUBSYS,
						1, REG_FLD_MASK(SUBSYS_MON_ENGINE_RST));
					cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						config_regs_pa + OVLSYS_DEBUG_SUBSYS,
						0, REG_FLD_MASK(SUBSYS_MON_ENGINE_RST));
					/* Disable subsys engine */
					cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
						config_regs_pa + OVLSYS_DEBUG_SUBSYS,
						0, REG_FLD_MASK(SUBSYS_MON_ENGINE_EN));
				}

				/* Disable smi monitor */
				mtk_dbgtp_ovlsys_smi_mon_config(mtk_crtc, cmdq_handle,
					&priv->mtk_dbgtp_sta.ovlsys[i], config_regs_pa, config_regs);

				priv->mtk_dbgtp_sta.ovlsys[i].need_update = false;
			}
		}
	}
}

void mtk_dbgtp_mmlsys_smi_mon_config(struct cmdq_pkt *cmdq_handle, struct cmdq_base *clt_base,
	struct dbgtp_subsys *mmlsys, resource_size_t config_regs_pa, void __iomem *config_regs)
{
	unsigned int value = 0;
	unsigned int i = 0;
	unsigned int j = 0;
	bool smi_mon_en = 0;
	bool rst_by_frame = 0;
	unsigned int slice_time = 0;
	unsigned int smi_mon_dump_sel = 0;
	unsigned int smi_mon_portid = 0;
	unsigned int smi_mon_cg_ctl = 0;
	unsigned int val = 0;
	unsigned int mask = 0;

	for (i = 0; i < MAX_SMI_MON_NUM / 2; i++) {
		smi_mon_en = mmlsys->smi_mon[i].smi_mon_en;
		rst_by_frame = mmlsys->smi_mon[i].rst_by_frame;
		slice_time = mmlsys->smi_mon[i].slice_time;
		smi_mon_dump_sel = mmlsys->smi_mon[i].smi_mon_dump_sel;
		if (smi_mon_en) {
			/* Enable & Config smi mon */
			value = (REG_FLD_VAL((SMI_MON_DUMP_SEL), smi_mon_dump_sel) |
				REG_FLD_VAL((SMI_MON_RST_BY_FRAME), rst_by_frame) |
				REG_FLD_VAL((SMI_MON_SLICE_TIME), slice_time) |
				REG_FLD_VAL((SMI_MON_ENABLE), smi_mon_en));
			mask = REG_FLD_MASK(SMI_MON_DUMP_SEL) |
				REG_FLD_MASK(SMI_MON_RST_BY_FRAME) |
				REG_FLD_MASK(SMI_MON_SLICE_TIME) |
				REG_FLD_MASK(SMI_MON_ENABLE);
			DDPDBG("%s:%d value:%x mask:%x\n", __func__, __LINE__, value, mask);
			if (cmdq_handle == NULL) {
				val = readl(config_regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_CON0 + i * 0x24);
				writel((val & ~mask) | value,
					config_regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_CON0 + i * 0x24);
			} else
				cmdq_pkt_write(cmdq_handle, clt_base,
					config_regs_pa + MMLSYS_DISP_SMI_DBG_MON_LARB0_CON0 + i * 0x24 , value, mask);
			for (j = 0; j < SMI_MON_PORT_NUM; j++) {
				smi_mon_portid = mmlsys->smi_mon[i].smi_mon_portid[j];
				smi_mon_cg_ctl = mmlsys->smi_mon[i].smi_mon_cg_ctl[j];
				value = (REG_FLD_VAL((SMI_MON_PORT_ID), smi_mon_portid) |
					REG_FLD_VAL((SMI_MON_PORT_CG_CTL), smi_mon_cg_ctl));
				mask = REG_FLD_MASK(SMI_MON_PORT_ID) |
					REG_FLD_MASK(SMI_MON_PORT_CG_CTL);
				DDPDBG("%s:%d value:%x mask:%x\n", __func__, __LINE__, value, mask);
				if (cmdq_handle == NULL) {
					val = readl(config_regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL +
						j * 0x8 + i * 0x24);
					writel((val & ~mask) | value, config_regs +
						MMLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL +
						j * 0x8 + i * 0x24);
				} else
					cmdq_pkt_write(cmdq_handle, clt_base,
						config_regs_pa + MMLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL +
						j * 0x8 + i * 0x24, value, mask);
			}
		} else {
			for (j = 0; j < SMI_MON_PORT_NUM; j++) {
				/* reset smi port monitor, set 1 set 0 */
				value = (REG_FLD_VAL((SMI_MON_PORT_RST), 1));
				mask = REG_FLD_MASK(SMI_MON_PORT_RST);
				if (cmdq_handle == NULL) {
					val = readl(config_regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL +
						j * 0x8 + i * 0x24);
					writel((val & ~mask) | value, config_regs +
						MMLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL +
						j * 0x8 + i * 0x24);
				} else
					cmdq_pkt_write(cmdq_handle, clt_base,
						config_regs_pa + MMLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL +
						j * 0x8 + i * 0x24, value, mask);

				value = (REG_FLD_VAL((SMI_MON_PORT_RST), 0));
				if (cmdq_handle == NULL) {
					val = readl(config_regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL +
						j * 0x8 + i * 0x24);
					writel((val & ~mask) | value, config_regs +
						MMLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL +
						j * 0x8 + i * 0x24);
				} else
					cmdq_pkt_write(cmdq_handle, clt_base,
						config_regs_pa + MMLSYS_DISP_SMI_DBG_MON_LARB0_MON0_SEL +
						j * 0x8 + i * 0x24, value, mask);
			}
			/* Disable SMI mon */
			value = (REG_FLD_VAL((SMI_MON_ENABLE), 0));
			mask = REG_FLD_MASK(SMI_MON_ENABLE);
			if (cmdq_handle == NULL) {
				val = readl(config_regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_CON0 + i * 0x24);
				writel((val & ~mask) | value,
					config_regs + MMLSYS_DISP_SMI_DBG_MON_LARB0_CON0 + i * 0x24);
			} else
				cmdq_pkt_write(cmdq_handle, clt_base,
					config_regs_pa + MMLSYS_DISP_SMI_DBG_MON_LARB0_CON0 + i * 0x24, value, mask);
		}
	}
}

void mtk_dbgtp_mmlsys_config(struct cmdq_pkt *cmdq_handle, struct cmdq_base *clt_base, unsigned int mmlsys_id,
		resource_size_t config_regs_pa, void __iomem *config_regs)
{
	unsigned int value = 0;
	bool dbgtp_mmlsys_en = false;
	bool dbgtp_mmlsys_update = false;
	bool subsys_smi_trig_en = 0;
	bool subsys_dsi_trig_en = 0;
	bool subsys_inlinerotate_info_en = 0;
	bool subsys_crossbar_info_en = 0;
	bool subsys_mon_info_en = 0;
	unsigned int crossbar_mon_cfg0 = 0;
	unsigned int crossbar_mon_cfg1 = 0;
	unsigned int crossbar_mon_cfg2 = 0;
	unsigned int crossbar_mon_cfg3 = 0;
	unsigned int crossbar_mon_cfg4 = 0;
	struct mtk_drm_private *priv = private_ptr;
	unsigned int val = 0;
	unsigned int mask = 0;


	DDPDBG("%s:%d for mmlsys:%d\n", __func__, __LINE__, mmlsys_id);

	dbgtp_mmlsys_en = priv->mtk_dbgtp_sta.mmlsys[mmlsys_id].subsys_mon_en;
	dbgtp_mmlsys_update = priv->mtk_dbgtp_sta.mmlsys[mmlsys_id].need_update;
	subsys_smi_trig_en = priv->mtk_dbgtp_sta.mmlsys[mmlsys_id].subsys_smi_trig_en;
	subsys_dsi_trig_en = priv->mtk_dbgtp_sta.mmlsys[mmlsys_id].subsys_dsi_trig_en;
	subsys_inlinerotate_info_en = priv->mtk_dbgtp_sta.mmlsys[mmlsys_id].subsys_inlinerotate_info_en;
	subsys_crossbar_info_en = priv->mtk_dbgtp_sta.mmlsys[mmlsys_id].subsys_crossbar_info_en;
	subsys_mon_info_en = priv->mtk_dbgtp_sta.mmlsys[mmlsys_id].subsys_mon_info_en;
	crossbar_mon_cfg0 = priv->mtk_dbgtp_sta.mmlsys[mmlsys_id].crossbar_mon_cfg0;
	crossbar_mon_cfg1 = priv->mtk_dbgtp_sta.mmlsys[mmlsys_id].crossbar_mon_cfg1;
	crossbar_mon_cfg2 = priv->mtk_dbgtp_sta.mmlsys[mmlsys_id].crossbar_mon_cfg2;
	crossbar_mon_cfg3 = priv->mtk_dbgtp_sta.mmlsys[mmlsys_id].crossbar_mon_cfg3;
	crossbar_mon_cfg4 = priv->mtk_dbgtp_sta.mmlsys[mmlsys_id].crossbar_mon_cfg4;

	if (((cmdq_handle != NULL) && (config_regs_pa <= 0)) ||
		((cmdq_handle == NULL) && (config_regs == NULL))) {
		DDPPR_ERR("%s:%d cmdq handle or addr error\n", __func__, __LINE__);
		return;
	}

	if (dbgtp_mmlsys_update) {
		if (dbgtp_mmlsys_en) {
			DDPDBG("%s:%d mmlsys%d dbgtp enable\n", __func__, __LINE__, mmlsys_id);

			/* Enable & config subsys - mmlsys 0,1,2 */
			value = (REG_FLD_VAL((SUBSYS_SMI_TRIG_EN), subsys_smi_trig_en) |
					REG_FLD_VAL((SUBSYS_DSI_TRIG_EN), subsys_dsi_trig_en) |
					REG_FLD_VAL((SUBSYS_INLINEROTATE_INFO_EN), subsys_inlinerotate_info_en) |
					REG_FLD_VAL((SUBSYS_CROSSBAR_INFO_EN), subsys_crossbar_info_en) |
					REG_FLD_VAL((SUBSYS_MON_INFO_EN), subsys_mon_info_en) |
					REG_FLD_VAL((SUBSYS_MON_ENGINE_EN), dbgtp_mmlsys_en));
			mask = REG_FLD_MASK(SUBSYS_SMI_TRIG_EN) |
				REG_FLD_MASK(SUBSYS_DSI_TRIG_EN) |
				REG_FLD_MASK(SUBSYS_INLINEROTATE_INFO_EN) |
				REG_FLD_MASK(SUBSYS_CROSSBAR_INFO_EN) |
				REG_FLD_MASK(SUBSYS_MON_INFO_EN) |
				REG_FLD_MASK(SUBSYS_MON_ENGINE_EN);
			DDPDBG("%s:%d value:%x mask:%x\n", __func__, __LINE__, value, mask);
			if (cmdq_handle == NULL) {
				val = readl(config_regs + MMLSYS_DEBUG_SUBSYS);
				writel((val & ~mask) | value, config_regs + MMLSYS_DEBUG_SUBSYS);
			} else
				cmdq_pkt_write(cmdq_handle, clt_base,
						config_regs_pa + MMLSYS_DEBUG_SUBSYS, value, mask);

			/* Config crossbar mon */
			if (cmdq_handle == NULL) {
				writel(crossbar_mon_cfg0, config_regs + MML_PQ_OUT_CROSSBAR_DEBUG_MONITOR_PTR);
				writel(crossbar_mon_cfg1, config_regs + MML_PQ_IN_CROSSBAR_DEBUG_MONITOR_PTR);
			} else {
				cmdq_pkt_write(cmdq_handle, clt_base,
						config_regs_pa + MML_PQ_OUT_CROSSBAR_DEBUG_MONITOR_PTR,
						crossbar_mon_cfg0, ~0);
				cmdq_pkt_write(cmdq_handle, clt_base,
						config_regs_pa + MML_PQ_IN_CROSSBAR_DEBUG_MONITOR_PTR,
						crossbar_mon_cfg1, ~0);
			}

			/* Enable & config smi monitor */
			mtk_dbgtp_mmlsys_smi_mon_config(cmdq_handle, clt_base,
					&priv->mtk_dbgtp_sta.mmlsys[mmlsys_id], config_regs_pa, config_regs);

			priv->mtk_dbgtp_sta.mmlsys[mmlsys_id].need_update = false;
		} else {
			DDPDBG("%s:%d mmlsys%d dbgtp disable\n", __func__, __LINE__, mmlsys_id);

			if (cmdq_handle == NULL) {
				value = (REG_FLD_VAL((SUBSYS_MON_ENGINE_RST), 0x1));
				mask = REG_FLD_MASK(SUBSYS_MON_ENGINE_RST);
				val = readl(config_regs +  MMLSYS_DEBUG_SUBSYS);
				writel((val & ~mask) | value, config_regs + MMLSYS_DEBUG_SUBSYS);

				value = (REG_FLD_VAL((SUBSYS_MON_ENGINE_RST), 0x0));
				val = readl(config_regs +  MMLSYS_DEBUG_SUBSYS);
				writel((val & ~mask) | value, config_regs + MMLSYS_DEBUG_SUBSYS);

				value = (REG_FLD_VAL((SUBSYS_MON_ENGINE_EN), 0x0));
				mask = REG_FLD_MASK(SUBSYS_MON_ENGINE_EN);
				val = readl(config_regs +  MMLSYS_DEBUG_SUBSYS);
				writel((val & ~mask) | value, config_regs + MMLSYS_DEBUG_SUBSYS);
			} else {
				/* Reset subsys */
				cmdq_pkt_write(cmdq_handle, clt_base,
						config_regs_pa + MMLSYS_DEBUG_SUBSYS,
						1, REG_FLD_MASK(SUBSYS_MON_ENGINE_RST));
				cmdq_pkt_write(cmdq_handle, clt_base,
						config_regs_pa + MMLSYS_DEBUG_SUBSYS,
						0, REG_FLD_MASK(SUBSYS_MON_ENGINE_RST));
				/* Disable subsys engine */
				cmdq_pkt_write(cmdq_handle, clt_base,
						config_regs_pa + MMLSYS_DEBUG_SUBSYS,
						0, REG_FLD_MASK(SUBSYS_MON_ENGINE_EN));
			}

			/* Disable smi monitor */
			mtk_dbgtp_mmlsys_smi_mon_config(cmdq_handle, clt_base,
					&priv->mtk_dbgtp_sta.mmlsys[mmlsys_id], config_regs_pa, config_regs);

			priv->mtk_dbgtp_sta.mmlsys[mmlsys_id].need_update = false;
		}
	}
}

void mtk_dbgtp_switch(struct mtk_drm_crtc *mtk_crtc, struct cmdq_pkt *cmdq_handle, bool en)
{
	if (cmdq_handle == NULL) {
		writel(en, dbgtp_comp->regs + DISP_DBG_TOP_EN);
		return;
	}

	/* Enable/Disable dbg top */
	cmdq_pkt_write(cmdq_handle, dbgtp_comp->cmdq_base,
			dbgtp_comp->regs_pa + DISP_DBG_TOP_EN, en, 0x1);
}

dma_addr_t mtk_get_dbgtp_comp_pa(void)
{
	return dbgtp_comp->regs_pa;
}

void mtk_dbgtp_config(struct mtk_drm_crtc *mtk_crtc, struct cmdq_pkt *cmdq_handle)
{
	unsigned int value = 0;
	struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;
	unsigned int val = 0;
	unsigned int mask = 0;
	unsigned int i = 0;

	if (priv->mtk_dbgtp_sta.dbgtp_en && !(readl(priv->config_regs + DISPSYS_DEBUG_SUBSYS) & 0x1)) {
		DDPDBG("%s:%d dbgtp regs not match setting need update\n", __func__, __LINE__);
		mtk_dbgtp_update(priv);
	}

	if (priv->mtk_dbgtp_sta.need_update) {
		if (priv->mtk_dbgtp_sta.dbgtp_en) {
			DDPDBG("%s:%d debug top enable\n", __func__, __LINE__);

			/* dbg top switch */
			value = priv->mtk_dbgtp_sta.dbgtp_switch;
			if (cmdq_handle == NULL)
				writel(value, dbgtp_comp->regs + DISP_DBG_TOP_SWITCH);
			else
				cmdq_pkt_write(cmdq_handle, dbgtp_comp->cmdq_base,
					dbgtp_comp->regs_pa + DISP_DBG_TOP_SWITCH, value, ~0);

			/* dbg top fifo mon config */
			for (i = 0; i < FIFO_MON_NUM; i++) {
				value = REG_FLD_VAL((DISP_FIFO_MON_EN),
					priv->mtk_dbgtp_sta.fifo_mon_en[i]);
				mask = REG_FLD_MASK(DISP_FIFO_MON_EN);
				if (cmdq_handle == NULL) {
					val = readl(dbgtp_comp->regs + DISP_DBG_FIFO_MON_CFG0 + i * 0x4);
					writel(value, dbgtp_comp->regs + DISP_DBG_FIFO_MON_CFG0 + i * 0x4);
				} else {
					cmdq_pkt_write(cmdq_handle, dbgtp_comp->cmdq_base,
						dbgtp_comp->regs_pa + DISP_DBG_FIFO_MON_CFG0 +
						i * 0x4, value, mask);
				}
			}

			/* dbg top periodic dump cfg */
			if (priv->mtk_dbgtp_sta.dbgtp_trig_prd) {
				value = (REG_FLD_VAL((DISP_TRIGGER_PRD),
					priv->mtk_dbgtp_sta.dbgtp_trig_prd) |
					REG_FLD_VAL((DISP_PERIODIC_DUMP_EN),
					priv->mtk_dbgtp_sta.dbgtp_prd_trig_en));
				mask = REG_FLD_MASK(DISP_TRIGGER_PRD) |
					REG_FLD_MASK(DISP_PERIODIC_DUMP_EN);
			} else {
				value = REG_FLD_VAL((DISP_PERIODIC_DUMP_EN),
					priv->mtk_dbgtp_sta.dbgtp_prd_trig_en);
				mask = REG_FLD_MASK(DISP_PERIODIC_DUMP_EN);
			}
			if (cmdq_handle == NULL) {
				val = readl(dbgtp_comp->regs + DISP_DBG_TOP_CON);
				writel((val & ~mask) | value, dbgtp_comp->regs + DISP_DBG_TOP_CON);
			} else
				cmdq_pkt_write(cmdq_handle, dbgtp_comp->cmdq_base,
					dbgtp_comp->regs_pa + DISP_DBG_TOP_CON, value, mask);

			/* dbg top timeout cfg */
			value = (REG_FLD_VAL((DISP_DBG_TOP_TIMEOUT_EN),
				priv->mtk_dbgtp_sta.dbgtp_timeout_en) |
					REG_FLD_VAL((DISP_DBG_TOP_TIMEOUT_PRD),
				priv->mtk_dbgtp_sta.dbgtp_timeout_prd));
			mask = REG_FLD_MASK(DISP_DBG_TOP_TIMEOUT_EN) |
				REG_FLD_MASK(DISP_DBG_TOP_TIMEOUT_PRD);
			if (cmdq_handle == NULL) {
				val = readl(dbgtp_comp->regs + DISP_DBG_TOP_TIMEOUT);
				writel((val & ~mask) | value, dbgtp_comp->regs + DISP_DBG_TOP_CON);
			} else
				cmdq_pkt_write(cmdq_handle, dbgtp_comp->cmdq_base,
					dbgtp_comp->regs_pa + DISP_DBG_TOP_TIMEOUT, value, mask);

			/* dbg top atb atid cfg : Default Non-zero Value */
			if (cmdq_handle == NULL)
				writel(0x1F, dbgtp_comp->regs + DISP_DBG_TOP_ATID);
			else
				cmdq_pkt_write(cmdq_handle, dbgtp_comp->cmdq_base,
					dbgtp_comp->regs_pa + DISP_DBG_TOP_ATID, 0x1F, 0x3F);

			/* Config & Enable subsys monitor */
			/* subsys - dispsys + SMI + DSI + DSI LPC + crossbar */
			mtk_dbgtp_dispsys_config(mtk_crtc, cmdq_handle);
			/* subsys - ovlsys + SMI + Crossbar */
			mtk_dbgtp_ovlsys_config(mtk_crtc, cmdq_handle);
			/* subsys - mmlsys + SMI + Crossbar */
			//mtk_dbgtp_mmlsys_config(mtk_crtc, cmdq_handle);

			/* Enable DPC monitor */
			value = priv->mtk_dbgtp_sta.dbgtp_dpc_mon_cfg;
			mtk_dpc_monitor_config(cmdq_handle, value);

			/* Enable LPC monitor */
			mtk_dsi_lpc_for_debug_config(mtk_crtc, cmdq_handle);

			/* Enable dbg top */
			if (cmdq_handle == NULL) {
				val = readl(dbgtp_comp->regs + DISP_DBG_TOP_EN);
				writel((val & ~mask) | value, dbgtp_comp->regs + DISP_DBG_TOP_EN);
			}

			/* Config AO FIFO mon */
			mtk_vdisp_ao_for_debug_config(mtk_crtc, cmdq_handle);

			/* No need config per frame */
			priv->mtk_dbgtp_sta.need_update = false;
		} else {
			DDPDBG("%s:%d debug top disable\n", __func__, __LINE__);

			/* Disable & Reset subsys monitor */
			/* subsys - dispsys + SMI + DSI + DSI LPC + crossbar */
			mtk_dbgtp_dispsys_config(mtk_crtc, cmdq_handle);
			/* subsys - ovlsys + SMI + Crossbar */
			mtk_dbgtp_ovlsys_config(mtk_crtc, cmdq_handle);
			/* subsys - mmlsys + SMI + Crossbar */
			//mtk_dbgtp_mmlsys_config(mtk_crtc, cmdq_handle);

			/* Disable DPC monitor */
			value = 0;
			mtk_dpc_monitor_config(cmdq_handle, value);

			if (cmdq_handle == NULL) {
				/* reset dbg top */
				writel(0x1, dbgtp_comp->regs + DISP_DBG_TOP_RST);
				writel(0x0, dbgtp_comp->regs + DISP_DBG_TOP_RST);
				/* disable dbg top */
				writel(0x0, dbgtp_comp->regs + DISP_DBG_TOP_EN);
			} else {
				/* reset dbg top */
				cmdq_pkt_write(cmdq_handle, dbgtp_comp->cmdq_base,
					dbgtp_comp->regs_pa + DISP_DBG_TOP_RST, 0x1, 0x1);
				cmdq_pkt_write(cmdq_handle, dbgtp_comp->cmdq_base,
					dbgtp_comp->regs_pa + DISP_DBG_TOP_RST, 0x0, 0x1);
				/* disable dbg top */
				cmdq_pkt_write(cmdq_handle, dbgtp_comp->cmdq_base,
					dbgtp_comp->regs_pa + DISP_DBG_TOP_EN, 0x0, 0x1);
			}

			/* reset LPC monitor */
			priv->mtk_dbgtp_sta.dsi_lpc_mon_en = false;
			mtk_dsi_lpc_for_debug_config(mtk_crtc, cmdq_handle);

			/* No need config per frame */
			priv->mtk_dbgtp_sta.need_update = false;
		}
	}
}

int mtk_dbgtp_analysis(struct mtk_ddp_comp *comp)
{

	return 0;
}

static int mtk_disp_dbgtp_bind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_disp_dbgtp *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	struct mtk_drm_private *private = drm_dev->dev_private;
	int ret;

	DDPMSG("%s:%d\n", __func__, __LINE__);

	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}
	private_ptr = private;

	return 0;
}

static void mtk_disp_dbgtp_unbind(struct device *dev, struct device *master,
				     void *data)
{
	struct mtk_disp_dbgtp *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static void mtk_dbgtp_prepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_prepare(comp);
}

static void mtk_dbgtp_unprepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_unprepare(comp);
}

static const struct mtk_ddp_comp_funcs mtk_disp_dbgtp_funcs = {
	.prepare = mtk_dbgtp_prepare,
	.unprepare = mtk_dbgtp_unprepare,
};

static const struct component_ops mtk_disp_dbgtp_component_ops = {
	.bind = mtk_disp_dbgtp_bind,
	.unbind = mtk_disp_dbgtp_unbind,
};

static const struct dbgtp_funcs dbgtp_mml_funcs = {
	.dbgtp_mmlsys_config = mtk_dbgtp_mmlsys_config,
	.dbgtp_mmlsys_config_dump = mtk_dbgtp_dump_mmlsys_regs,
};

static irqreturn_t mtk_disp_dbgtp_irq_handler(int irq, void *dev_id)
{
	struct mtk_ddp_comp *dbgtp = NULL;
	unsigned int val = 0;
	unsigned int val1 = 0;
	unsigned int ret = 0;

	dbgtp = dbgtp_comp;
	if (IS_ERR_OR_NULL(dbgtp))
		return IRQ_NONE;

	if (mtk_drm_top_clk_isr_get(dbgtp) == false) {
		DDPIRQ("%s, top clk off\n", __func__);
		return IRQ_NONE;
	}

	val = readl(dbgtp->regs + DISP_DBG_FIFO_MON_INTSTA);
	val1 = readl(dbgtp->regs + DISP_DBG_FIFO_MON_CFG0);
	if (!val) {
		ret = IRQ_NONE;
		goto out;
	}

	DRM_MMP_MARK(IRQ, dbgtp->regs_pa, val);
	DDPDBG("%s irq, val:0x%x\n", mtk_dump_comp_str(dbgtp), val);

	if (val & (1 << 0)) {
		DRM_MMP_MARK(dbgtp, val, val1);
		DDPDBG("[IRQ] %s: 0 trigger start\n", mtk_dump_comp_str(dbgtp));
		DDP_DBGTP("%s: 0 trigger start\n", mtk_dump_comp_str(dbgtp));
		mtk_dsi_fifo_mon_trigger_start_set(true);
		writel(0xf, dbgtp->regs + DISP_DBG_FIFO_MON_INT_CLR);
		writel(0, dbgtp->regs + DISP_DBG_FIFO_MON_INT_CLR);
	}

	if (val & (1 << 1)) {
		DRM_MMP_MARK(dbgtp, val, 1);
		DDPDBG("[IRQ] %s: 1 trigger start\n", mtk_dump_comp_str(dbgtp));
		writel(0xf, dbgtp->regs + DISP_DBG_FIFO_MON_INT_CLR);
		writel(0, dbgtp->regs + DISP_DBG_FIFO_MON_INT_CLR);
	}

	if (val & (1 << 2)) {
		DRM_MMP_MARK(dbgtp, val, 2);
		DDPDBG("[IRQ] %s: 2 trigger start\n", mtk_dump_comp_str(dbgtp));
		writel(0xf, dbgtp->regs + DISP_DBG_FIFO_MON_INT_CLR);
		writel(0, dbgtp->regs + DISP_DBG_FIFO_MON_INT_CLR);
	}

	if (val & (1 << 3)) {
		DRM_MMP_MARK(dbgtp, val, 3);
		DDPDBG("[IRQ] %s: 3 trigger start\n", mtk_dump_comp_str(dbgtp));
		writel(0xf, dbgtp->regs + DISP_DBG_FIFO_MON_INT_CLR);
		writel(0, dbgtp->regs + DISP_DBG_FIFO_MON_INT_CLR);
	}

	ret = IRQ_HANDLED;

out:
	mtk_drm_top_clk_isr_put(dbgtp);

	return ret;
}

static int mtk_disp_dbgtp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_dbgtp *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret = 0;
	int irq = 0;

	DDPMSG("%s+\n", __func__);


	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "failed to request dbgtp irq resource\n");
		ret = -EPROBE_DEFER;
		return ret;
	}

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_DBGTP);
	if ((int)comp_id < 0) {
		DDPPR_ERR("Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_dbgtp_funcs);
	if (ret != 0) {
		DDPPR_ERR("Failed to initialize component: %d\n", ret);
		return ret;
	}

	priv->data = of_device_get_match_data(dev);

	/* For Camera ELA affect display ELA issue */
	if (priv->data->mminfra_funnel_addr)
		priv->mminfra_funnel = ioremap(priv->data->mminfra_funnel_addr, 0x4);
	if (priv->data->trace_top_funnel_addr)
		priv->trace_top_funnel = ioremap(priv->data->trace_top_funnel_addr, 0x4);

	platform_set_drvdata(pdev, priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);
	dbgtp_comp = &priv->ddp_comp;

	ret = devm_request_irq(dev, irq, mtk_disp_dbgtp_irq_handler,
			       IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(dev),
			       priv);
	if (ret < 0) {
		DDPAEE("%s:%d, failed to request irq:%d ret:%d comp_id:%d\n",
				__func__, __LINE__,
				irq, ret, comp_id);
		return ret;
	}

	ret = component_add(dev, &mtk_disp_dbgtp_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
		return ret;
	}

	mml_dbgtp_register(&dbgtp_mml_funcs);

	DDPMSG("%s-\n", __func__);
	return ret;
}

static void mtk_disp_dbgtp_remove(struct platform_device *pdev)
{
	struct mtk_disp_dbgtp *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_dbgtp_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);
}

static const struct mtk_disp_dbgtp_data mt6993_dbgtp_driver_data = {
	.is_support_34bits = true,
	.need_bypass_shadow = true,
	.mminfra_funnel_addr = 0x30a2f000,
	.trace_top_funnel_addr = 0x0d070000,
};

static const struct of_device_id mtk_disp_dbgtp_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6993-disp-dbgtp",
	  .data = &mt6993_dbgtp_driver_data},
	{},
};

struct platform_driver mtk_disp_dbgtp_driver = {
	.probe = mtk_disp_dbgtp_probe,
	.remove = mtk_disp_dbgtp_remove,
	.driver = {
			.name = "mediatek-disp-dbgtp",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_dbgtp_driver_dt_match,
		},
};
