// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <drm/drm_vblank.h>

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include "mtk_drm_trace.h"

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_mmp.h"
#include "mtk_dsi_lpc.h"
#include <linux/module.h>

#define DSI_LPC_EN (0x0)
#define DSI_LPC_EN_BIT_FLD REG_FLD_MSB_LSB(0, 0)
#define DSI_LPC_EN_BIT BIT(0)
#define DSI_LPC_DBG_MON_EN REG_FLD_MSB_LSB(4, 4)
#define DSI_LPC_DBG_MON_RST REG_FLD_MSB_LSB(5, 5)

#define DSI_LPC_PHY_CON (0x04)
#define DSI_LPC_EVENT_TYPE (0x08)
#define DSI_LPC_GPIO_OUT_CON (0x0C)
#define DSI_LPC_SYS_TIMER_TIMESTAMP0 (0x80)
#define DSI_LPC_SYS_TIMER_TIMESTAMP1 (0x84)

#define DSI_LPC_CONFIG(n) (0x100 + 0x100 * (n))
#define DSI_LPC_UNIT_EN BIT(0)
#define DSI_LPC_UNIT_EN_FLD REG_FLD_MSB_LSB(0, 0)
#define DSI_LPC_UNIT_REST BIT(1)
#define DSI_LPC_HYNC_PWR_ON_SEQ_EN BIT(4)
#define DSI_LPC_DCM_DIS BIT(31)

#define DSI_LPC_INTEN(n) (0x104 + 0x100 * (n))
#define DSI_SOF_INT_EN BIT(0)
#define DSI_DONE_INT_EN BIT(1)
#define DSI_START_INT_EN BIT(2)
#define DSI_FRAME_DONE_INT_EN BIT(3)
#define DSI_GATE_EVENT_INT_EN BIT(4)
#define DSI_LPC_INTERVAL_START_INT_EN BIT(5)
#define IDLE_FRAME_PRD_START_INT_EN BIT(6)
#define GPIO_HSYNC_INT_EN BIT(7)
#define DBG_CALI_FAIL_INT_EN BIT(8)
#define HSYNC_PWR_ON_PLUSE_INT_EN BIT(9)
#define EXT_TE_PLUS_INT_EN BIT(10)
#define EVENT_TE_INT_EN BIT(11)
#define REPORTED_RESYNC_INT_EN BIT(12)
#define MIPI_ERROR_FLAG_INT_EN BIT(13)

#define DSI_LPC_INSTA(n) (0x108 + 0x100 * (n))
#define DSI_SOF_INT BIT(0)
#define DSI_DONE_INT BIT(1)
#define DSI_START_INT BIT(2)
#define DSI_FRAME_DONE_INT BIT(3)
#define DSI_GATE_EVENT_INT BIT(4)
#define DSI_LPC_INTERVAL_START_INT BIT(5)
#define IDLE_FRAME_PRD_START_INT BIT(6)
#define GPIO_HSYNC_INT BIT(7)
#define DBG_CALI_FAIL_INT BIT(8)
#define HSYNC_PWR_ON_PLUSE_INT BIT(9)
#define EXT_TE_PLUS_INT BIT(10)
#define EVENT_TE_INT BIT(11)
#define REPORTED_RESYNC_INT BIT(12)
#define MIPI_ERROR_FLAG_INT BIT(13)

#define DSI_LPC_UP_INTEN(n) (0x10C + 0x100 * (n))
#define DSI_LPC_UP_INSTA(n) (0x110 + 0x100 * (n))
#define DSI_LPC_SCP_INTEN(n) (0x114 + 0x100 * (n))
#define DSI_LPC_SCP_INSTA(n) (0x118 + 0x100 * (n))

#define DSI_LPC_FRAME_SIZE(n) (0x11C + 0x100 * (n))
#define DSI_LPC_EM_PRD_NUM(n) (0x120 + 0x100 * (n))
#define DSI_LPC_HSYNC_PRD_NUM(n) (0x124 + 0x100 * (n))

#define DSI_LPC_VIDLE_CON(n) (0x128 + 0x100 * (n))
#define DSI_LPC_DSI_VIDLE_EN BIT(0)
#define DSI_LPC_HSYNC_MODE_SEL REG_FLD_MSB_LSB(6, 4)
#define DSI_LPC_GPIO_HSYNC_EN BIT(7)
#define DSI_LPC_GPIO_HSYNC_TYPE BIT(8)

#define DSI_LPC_GPIO_HSYNC_CON0(n) (0x12C + 0x100 * (n))
#define DSI_LPC_GPIO_HSYNC_WIDTH_A REG_FLD_MSB_LSB(15, 0)
#define DSI_LPC_GPIO_HSYNC_WIDTH_B REG_FLD_MSB_LSB(31, 16)
#define DSI_LPC_GPIO_HSYNC_CON1(n) (0x130 + 0x100 * (n))
#define DSI_LPC_GPIO_HSYNC_EVENT_SHIFT_NUM REG_FLD_MSB_LSB(15, 0)
#define DSI_LPC_GPIO_HSYNC_MASK_TIMER REG_FLD_MSB_LSB(31, 16)

#define DSI_LPC_GATE_NL(n) (0x134 + 0x100 * (n))
#define DSI_LPC_GATE_NL_SIZE REG_FLD_MSB_LSB(19, 0)
#define DSI_LPC_AFTER_TE_GATE_EVENT_EN BIT(28)

#define DSI_LPC_TE_CON0(n) (0x160 + 0x100 * (n))
#define DSI_LPC_TE_TYPE REG_FLD_MSB_LSB(2, 0)
#define DSI_FIXED_TE (1)
#define DSI_MULTI_TE (2)
#define DSI_REQUEST_TE (3)
#define DSI_LEVEL_TE (4)
#define DSI_ARP_TE (5)
#define DSI_LPC_EVENT_TE_SEL BIT(4)
#define DSI_LPC_TE_NUM REG_FLD_MSB_LSB(17, 8)
#define DSI_LPC_HW_VSYNC_ON BIT(31)
#define DSI_LPC_HW_VSYNC_ON_FLD REG_FLD_MSB_LSB(31, 31)

#define DSI_LPC_TE_CON1(n) (0x164 + 0x100 * (n))
#define DSI_LPC_FAKE_TE_PRD REG_FLD_MSB_LSB(17, 0)

#define DSI_LPC_TE_CON2(n) (0x168 + 0x100 * (n))
#define DSI_LPC_FTE_MASK_EN BIT(0)
#define DSI_LPC_FTE_MASK_NUM REG_FLD_MSB_LSB(7, 4)
#define DSI_LPC_TE_DOWNSAMP_EN BIT(8)
#define DSI_LPC_TE_DOWNSAMP_NUM REG_FLD_MSB_LSB(21, 12)

#define DSI_LPC_SOF_TIMESTAMP_0(n) (0x180 + 0x100 * (n))
#define DSI_LPC_SOF_TIMESTAMP_1(n) (0x184 + 0x100 * (n))
#define DSI_LPC_FRAME_DONE_TIMESTAMP_0(n) (0x188 + 0x100 * (n))
#define DSI_LPC_FRAME_DONE_TIMESTAMP_1(n) (0x18C + 0x100 * (n))
#define DSI_LPC_EVENT_TE_TIMESTAMP_0(n) (0x190 + 0x100 * (n))
#define DSI_LPC_EVENT_TE_TIMESTAMP_1(n) (0x194 + 0x100 * (n))
#define DSI_LPC_RESYNC_TE_TIMESTAMP_0(n) (0x198 + 0x100 * (n))
#define DSI_LPC_RESYNC_TE_TIMESTAMP_1(n) (0x19C + 0x100 * (n))
#define DSI_LPC_GATE_EVENT_TIMESTAMP_0(n) (0x1A0 + 0x100 * (n))
#define DSI_LPC_GATE_EVENT_TIMESTAMP_1(n) (0x1A4 + 0x100 * (n))

#define DSI_LPC_DBG0(n) (0x1B0 + 0x100 * (n))
#define HSYNC_ACCU_HSYNC_NUM REG_FLD_MSB_LSB(11, 0)
#define EM_PRD_CNT REG_FLD_MSB_LSB(31, 12)
#define DSI_LPC_DBG1(n) (0x1B4 + 0x100 * (n))
#define DSI_LPC_DBG2(n) (0x1B8 + 0x100 * (n))
#define DSI_LPC_DBG3(n) (0x1BC + 0x100 * (n))
#define FRAME_LINE_CNT REG_FLD_MSB_LSB(19, 0)
#define GPIO_HSYNC_FSM_STATE REG_FLD_MSB_LSB(22, 20)
#define IDLE_FRAME_PRD BIT(23)
#define DSI_LPC_THREAD BIT(24)
#define DSI_LPC_TRIG BIT(25)
#define TE_TYPE_WRK REG_FLD_MSB_LSB(28, 26)

#define DSI_LPC_DBG4(n) (0x1C0 + 0x100 * (n))
#define FAKE_TE_CNT REG_FLD_MSB_LSB(9, 0)
#define FAKE_TE_PRD_CNT REG_FLD_MSB_LSB(27, 10)

#define DSI_LPC_DBG5(n) (0x1C4 + 0x100 * (n))
#define USED_FAKE_TE_PRD REG_FLD_MSB_LSB(17, 0)
#define DSI_LPC_GPI(n) (0x1FC + 0x100 * (n))
#define DSI_LPC_EXT_TE_SEL REG_FLD_MSB_LSB(1, 0)
#define DSI_LPC_MIPI_ERROR_FLAG_SEL REG_FLD_MSB_LSB(3, 2)

enum LPC_MMP_IDX {
	HW_VSYNC_ON_CONFIG,
	IRQ_DISABLE = 0xFF,
};
struct mtk_dsi_lpc_data {
	const unsigned int te_limit;
};
struct mtk_dsi_lpc {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	int lpc_te_type;
	int lpc_sof_status;
	bool dsi_lpc_en;
	bool dsi_lpc_te_irq_en;
	ktime_t ts_offset;	// arch_timer_get_cntfrq: MT6993 = 1,000,000,000
	const struct mtk_dsi_lpc_data *data;
};
static const struct mtk_dsi_lpc_data lpc_data_mt6993 = {
	.te_limit = 8333,
};
void mtk_dsi_lpc_for_debug_config(struct mtk_drm_crtc *mtk_crtc, struct cmdq_pkt *cmdq_handle)
{
	unsigned int value = 0;
	struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;
	unsigned int val = 0;
	unsigned int mask = 0;
	struct mtk_ddp_comp *comp = mtk_ddp_comp_request_output_lpc(mtk_crtc);

	DDPDBG("%s:%d dsi lpc mon en:%d\n", __func__, __LINE__, priv->mtk_dbgtp_sta.dsi_lpc_mon_en);

	if (!comp) {
		DDPPR_ERR("%s: request lpc comp fail\n", __func__);
		return;
	}

	if (priv && priv->mtk_dbgtp_sta.dsi_lpc_mon_en) {
		value = (REG_FLD_VAL((DSI_LPC_DBG_MON_EN),
			priv->mtk_dbgtp_sta.dsi_lpc_mon_en));
		mask = REG_FLD_MASK(DSI_LPC_DBG_MON_EN);

		DDPDBG("%s:%d value:%x mask:%x\n", __func__, __LINE__, value, mask);
		if (cmdq_handle == NULL) {
			val = readl(comp->regs + DSI_LPC_EN);
			writel((val & ~mask) | value, comp->regs + DSI_LPC_EN);
		} else
			cmdq_pkt_write(cmdq_handle, comp->cmdq_base,
					comp->regs_pa + DSI_LPC_EN, value, mask);

	} else {
		value = (REG_FLD_VAL((DSI_LPC_DBG_MON_RST), 0x1));
		mask = REG_FLD_MASK(DSI_LPC_DBG_MON_RST);

		if (cmdq_handle == NULL) {
			val = readl(comp->regs + DSI_LPC_EN);
			writel((val & ~mask) | value, comp->regs + DSI_LPC_EN);
		} else
			cmdq_pkt_write(cmdq_handle, comp->cmdq_base,
					comp->regs_pa + DSI_LPC_EN, value, mask);

		value = (REG_FLD_VAL((DSI_LPC_DBG_MON_RST), 0x0));

		if (cmdq_handle == NULL) {
			val = readl(comp->regs + DSI_LPC_EN);
			writel((val & ~mask) | value, comp->regs + DSI_LPC_EN);
		} else
			cmdq_pkt_write(cmdq_handle, comp->cmdq_base,
					comp->regs_pa + DSI_LPC_EN, value, mask);
	}
}

static inline struct mtk_dsi_lpc *comp_to_dsi_lpc(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_dsi_lpc, ddp_comp);
}
void mtk_set_dsi_lpc_en(struct mtk_ddp_comp *comp, bool en)
{
	struct mtk_dsi_lpc *lpc = comp_to_dsi_lpc(comp);

	lpc->dsi_lpc_en = en;
}
static int mtk_dsi_lpc_unit(struct mtk_drm_crtc *mtk_crtc)
{
	struct mtk_ddp_comp *output_comp = NULL;
	int index = -1;

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp && (mtk_ddp_comp_get_type(output_comp->id) == MTK_DSI)) {
		/* temp support dsi0 */
		if (output_comp->id == DDP_COMPONENT_DSI0)
			index = 0;
	}
	return index;
}
void mtk_dsi_lpc_te_irq_en(struct mtk_drm_crtc *mtk_crtc,
	struct mtk_ddp_comp *comp, bool en)
{
	int index = mtk_dsi_lpc_unit(mtk_crtc);
	unsigned int lpc_inten = 0;
	struct mtk_dsi_lpc *lpc = comp_to_dsi_lpc(comp);

	if (index < 0)
		return;

	lpc_inten = readl(comp->regs + DSI_LPC_INTEN(index));
	if (en)
		lpc_inten |= EVENT_TE_INT_EN;
	else
		lpc_inten &= ~EVENT_TE_INT_EN;

	/* for ddic error */
	lpc_inten |= MIPI_ERROR_FLAG_INT_EN;

	writel(0, comp->regs + DSI_LPC_INTEN(index));
	writel(lpc_inten, comp->regs + DSI_LPC_INTEN(index));

	lpc->dsi_lpc_te_irq_en = en;
}
void mtk_dsi_lpc_set_te_en(struct mtk_drm_crtc *mtk_crtc, bool en)
{
	struct mtk_ddp_comp *comp = mtk_ddp_comp_request_output_lpc(mtk_crtc);
	struct mtk_dsi_lpc *lpc = comp_to_dsi_lpc(comp);

	mtk_dsi_lpc_te_irq_en(mtk_crtc, comp, en);
}

int mtk_dsi_lpc_dump(struct mtk_ddp_comp *comp)
{
	int i = 0;
	void __iomem *baddr = comp->regs;
	struct mtk_dsi_lpc *lpc = NULL;
	bool lpc_en = false;

	lpc = comp_to_dsi_lpc(comp);
	if (!lpc)
		return 0;

	if (!lpc->dsi_lpc_en)
		return 0;

	if (!baddr)
		return 0;

	DDPDUMP("== LPC REGS:0x%pa ==\n", &comp->regs_pa);

	for (i = 0; i < 0x1FF; i += 16) {
		DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", i,
			readl(baddr + i),
			readl(baddr + i + 0x4),
			readl(baddr + i + 0x8),
			readl(baddr + i + 0xC));
	}

	return 0;
}
void mtk_dsi_lpc_analysis(struct mtk_ddp_comp *comp)
{
	unsigned int reg_val = 0;
	int i = 0;
	void __iomem *baddr = comp->regs;
	struct mtk_dsi_lpc *lpc = NULL;
	bool lpc_en = false;

	lpc = comp_to_dsi_lpc(comp);
	if (!lpc)
		return;

	if (!lpc->dsi_lpc_en)
		return;

	if (!baddr)
		return;

	DDPDUMP("== LPC ANALYSIS:0x%pa ==\n", &comp->regs_pa);
	reg_val = readl(baddr + DSI_LPC_EN);
	DDPDUMP("LPC_EN:%d\n", REG_FLD_VAL_GET(DSI_LPC_EN_BIT_FLD, reg_val));
	for (i = 0; i < 4; i++) {
		reg_val = readl(baddr + DSI_LPC_CONFIG(i));
		if ((reg_val & 0x1) == 0)
			break;
		DDPDUMP("LPC(%d) UNIT_EN:%d\n", i, REG_FLD_VAL_GET(DSI_LPC_UNIT_EN_FLD, reg_val));
		reg_val = readl(baddr + DSI_LPC_TE_CON0(i));
		DDPDUMP("TE_TYPE:%d,LPC_TE_NUM:%d,HW_VSYNC_ON:%d\n",
			REG_FLD_VAL_GET(DSI_LPC_TE_TYPE, reg_val),
			REG_FLD_VAL_GET(DSI_LPC_TE_NUM, reg_val),
			REG_FLD_VAL_GET(DSI_LPC_HW_VSYNC_ON_FLD, reg_val));
		reg_val = readl(baddr + DSI_LPC_TE_CON1(i));
		DDPDUMP("FAKE_TE_PRD:%d\n", REG_FLD_VAL_GET(DSI_LPC_FAKE_TE_PRD, reg_val));
	}
}
static void mtk_dsi_lpc_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	int i = 0;
	struct mtk_dsi_lpc *lpc = comp_to_dsi_lpc(comp);

	writel(0, comp->regs + DSI_LPC_EN);
	for (i = 0; i < 4; i++) {
		writel(0, comp->regs + DSI_LPC_INTEN(i));
		writel(0, comp->regs + DSI_LPC_INSTA(i));
	}

	lpc->lpc_sof_status = 0;
	drm_trace_tag_mark("lpc_stop");
	DRM_MMP_MARK(dsi_lpc, IRQ_DISABLE, 0xFFFF);
}
static void mtk_dsi_lpc_prepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_prepare(comp);
}
static void mtk_dsi_lpc_unprepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_unprepare(comp);
}
void set_pl_kernel_offset(struct mtk_ddp_comp *comp)
{
	struct mtk_dsi_lpc *lpc = comp_to_dsi_lpc(comp);
	ktime_t ts = ktime_get();

	lpc->ts_offset = arch_timer_read_counter() - ts;

	DDPINFO("%s,ker:%llu,offset:%llu\n", __func__, ts, lpc->ts_offset);
}
ktime_t pl_kernel_offset(struct mtk_ddp_comp *comp)
{
	struct mtk_dsi_lpc *lpc = comp_to_dsi_lpc(comp);

	return lpc->ts_offset;
}
void mtk_dsi_lpc_sof_ts(long long *sof_ts, struct mtk_drm_crtc *mtk_crtc, struct mtk_ddp_comp *comp)
{
	/* repot sof time */
	int index = 0;
	unsigned long ts0, ts1;
	struct mtk_dsi_lpc *lpc = comp_to_dsi_lpc(comp);

	index = mtk_dsi_lpc_unit(mtk_crtc);
	if (index < 0) {
		DDPMSG("%s lpc unit error\n", __func__);
		return;
	}

	ts0 = readl(comp->regs+ DSI_LPC_SOF_TIMESTAMP_0(index));
	ts1 = readl(comp->regs + DSI_LPC_SOF_TIMESTAMP_1(index));
	*sof_ts = (ts1 << 32 | ts0) << 7;
	*sof_ts -= pl_kernel_offset(comp);

	lpc->lpc_sof_status = 1;

	drm_trace_tag_value("lpc_sof_timestamp", *sof_ts);
}
void mtk_dsi_lpc_event_te_ts(unsigned long *event_te_ts_diff, struct mtk_drm_crtc *mtk_crtc,
	struct mtk_ddp_comp *comp)
{
	/* repot sof time */
	int index = 0;
	unsigned long ts0 = 0, ts1 = 0;
	static unsigned long pre_event_te_ts;
	unsigned long event_te_ts = 0;

	if (!comp || !mtk_crtc) {
		DDPMSG("%s NULL pointer\n", __func__);
		return;
	}

	index = mtk_dsi_lpc_unit(mtk_crtc);
	if (index < 0) {
		DDPMSG("%s lpc unit error\n", __func__);
		return;
	}

	ts0 = readl(comp->regs + DSI_LPC_EVENT_TE_TIMESTAMP_0(index));
	ts1 = readl(comp->regs + DSI_LPC_EVENT_TE_TIMESTAMP_1(index));
	event_te_ts = (ts1 << 32 | ts0) << 7;
	if (event_te_ts > pre_event_te_ts) {
		*event_te_ts_diff = event_te_ts - pre_event_te_ts;
		pre_event_te_ts = event_te_ts;
	}
}
void mtk_dsi_lpc_resync_ts(long long *resync_ts, struct mtk_drm_crtc *mtk_crtc,
	struct mtk_ddp_comp *comp)
{
	/* repot resync time */
	int index = 0;
	unsigned long ts0, ts1;

	index = mtk_dsi_lpc_unit(mtk_crtc);
	if (index < 0) {
		DDPMSG("%s lpc unit error\n", __func__);
		return;
	}

	ts0 = readl(comp->regs + DSI_LPC_RESYNC_TE_TIMESTAMP_0(index));
	ts1 = readl(comp->regs + DSI_LPC_RESYNC_TE_TIMESTAMP_1(index));
	*resync_ts = (ts1 << 32 | ts0) << 7;
	*resync_ts -= pl_kernel_offset(comp);

	drm_trace_tag_value("lpc_resync_timestamp", *resync_ts);
}
int mtk_dsi_lpc_interrupt_enable(struct mtk_drm_crtc *mtk_crtc,
	struct mtk_ddp_comp *comp, bool en)
{
	struct mtk_dsi_lpc *lpc = comp_to_dsi_lpc(comp);
	int index = 0;
	unsigned int lpc_te_con0_val = 0;
	unsigned int inten = 0;
	int sof_status;

	index = mtk_dsi_lpc_unit(mtk_crtc);
	if (index < 0) {
		DDPMSG("%s lpc unit error\n", __func__);
		return -EINVAL;
	}

	DDP_MUTEX_LOCK_CONDITION(&mtk_crtc->lock, __func__, __LINE__, false);

	if (!mtk_crtc->enabled) {
		DDPMSG("%s crtc is disable\n", __func__);
		DDP_MUTEX_UNLOCK_CONDITION(&mtk_crtc->lock, __func__, __LINE__, false);
		return -EINVAL;
	}

	if (lpc->lpc_te_type == DSI_FIXED_TE)
		sof_status = 1;
	else if (lpc->lpc_te_type == DSI_MULTI_TE)
		sof_status = lpc->lpc_sof_status;
	else
		sof_status = 0;

	if (sof_status == 0) {
		drm_trace_tag_mark("lpc_hwvsync_on_ioctl_no_sof");
		DDP_MUTEX_UNLOCK_CONDITION(&mtk_crtc->lock, __func__, __LINE__, false);
		return -EPERM;
	}

	mtk_drm_idlemgr_kick(__func__, &mtk_crtc->base, 0);
	mtk_vidle_user_power_keep(DISP_VIDLE_USER_CRTC);

	lpc_te_con0_val = readl(comp->regs + DSI_LPC_TE_CON0(index));

	if (en) {
		inten |= REPORTED_RESYNC_INT_EN;
		lpc_te_con0_val |= DSI_LPC_HW_VSYNC_ON;
	} else {
		inten &= ~REPORTED_RESYNC_INT_EN;
		lpc_te_con0_val &= ~DSI_LPC_HW_VSYNC_ON;
	}

	if (lpc->dsi_lpc_te_irq_en)
		inten |= EVENT_TE_INT_EN;

	/* for ddic error */
	inten |= MIPI_ERROR_FLAG_INT_EN;

	writel(lpc_te_con0_val, comp->regs + DSI_LPC_TE_CON0(index));
	writel(0, comp->regs + DSI_LPC_INTEN(index));
	writel(inten, comp->regs + DSI_LPC_INTEN(index));

	mtk_vidle_user_power_release(DISP_VIDLE_USER_CRTC);
	DDP_MUTEX_UNLOCK_CONDITION(&mtk_crtc->lock, __func__, __LINE__, false);

	if (en) {
		DDPINFO("%s,[%d]inten:0x%x,lpc_te_con0_val:0x%x\n", __func__, index, inten, lpc_te_con0_val);
		DRM_MMP_MARK(dsi_lpc, HW_VSYNC_ON_CONFIG, lpc_te_con0_val);
		drm_trace_tag_value("lpc_hwvsync_on", lpc_te_con0_val);
	} else {
		drm_trace_tag_mark("lpc_resync_irq_disable");
		DRM_MMP_MARK(dsi_lpc, IRQ_DISABLE, 0);
	}

	return 0;
}
void mtk_dsi_set_lpc_en(bool en, struct mtk_ddp_comp *comp)
{
	writel(en, comp->regs + DSI_LPC_EN);
}
void mtk_dsi_lpc_unit_en(bool en, int index, struct mtk_ddp_comp *comp)
{
	writel(en, comp->regs + DSI_LPC_CONFIG(index));
}
void mtk_dsi_lpc_update_fake_te_info(struct mtk_dsi_lpc *lpc, unsigned int real_te_duration,
	unsigned int *te_type, unsigned long *dsi_lpc_fake_te_prd)
{
	/*
	 * enable mte and fake te when te_duration <= te_limit,
	 * due to LPC_FAKE_TE_PRD REG_FLD_MSB_LSB(17, 0) limit.
	 * real_te_duration (us), FAKE_TE_PRD (26M clock cycle)
	 */
	if (lpc->data->te_limit) {
		if (real_te_duration <= lpc->data->te_limit) {
			*dsi_lpc_fake_te_prd = real_te_duration * 26;
			*te_type = DSI_MULTI_TE;
		} else {
			*dsi_lpc_fake_te_prd = 0;
			*te_type = DSI_FIXED_TE;
		}

	} else {
		*dsi_lpc_fake_te_prd = real_te_duration * 26;
		*te_type = DSI_MULTI_TE;
	}
	lpc->lpc_te_type = *te_type;
}
void mtk_dsi_lpc_update_panel_params(struct mtk_drm_crtc *mtk_crtc,
	struct mtk_ddp_comp *comp, struct cmdq_pkt *cmdq_handle,
	struct mtk_panel_params *params)
{
	struct mtk_dsi_lpc *lpc = comp_to_dsi_lpc(comp);
	int index = 0;
	unsigned int lpc_te_con0_val = 0;
	unsigned long dsi_lpc_fake_te_prd = 0;
	unsigned int real_te_duration = 0;
	unsigned int skip_vblank = 0;
	unsigned int te_type = 0;
	int reg_val = 0;

	index = mtk_dsi_lpc_unit(mtk_crtc);
	if (index < 0) {
		DDPMSG("%s lpc unit error\n", __func__);
		return;
	}

	if (!params) {
		DDPMSG("%s params error\n", __func__);
		return;
	}

	if (cmdq_handle) {
		skip_vblank = params->skip_vblank;
		real_te_duration = params->real_te_duration;
	} else {
		skip_vblank = params->cur_skip_vblank;
		real_te_duration = params->cur_te_duration;
	}
	mtk_dsi_lpc_update_fake_te_info(lpc, real_te_duration, &te_type, &dsi_lpc_fake_te_prd);

	lpc_te_con0_val = (skip_vblank << 8) | te_type;

	if (cmdq_handle) {
		reg_val = DSI_LPC_UNIT_EN;

		cmdq_pkt_write(cmdq_handle, comp->cmdq_base,
			comp->regs_pa+ DSI_LPC_TE_CON0(index), lpc_te_con0_val, ~0);
		cmdq_pkt_write(cmdq_handle, comp->cmdq_base,
			comp->regs_pa+ DSI_LPC_TE_CON1(index), dsi_lpc_fake_te_prd, ~0);

		reg_val |= DSI_LPC_UNIT_REST;
		cmdq_pkt_write(cmdq_handle, comp->cmdq_base,
			comp->regs_pa+ DSI_LPC_CONFIG(index), reg_val, ~0);

		reg_val &= ~DSI_LPC_UNIT_REST;
		cmdq_pkt_write(cmdq_handle, comp->cmdq_base,
			comp->regs_pa+ DSI_LPC_CONFIG(index), reg_val, ~0);

		cmdq_pkt_write(cmdq_handle, comp->cmdq_base,
			comp->regs_pa+ DSI_LPC_TE_CON0(index), lpc_te_con0_val | DSI_LPC_HW_VSYNC_ON, ~0);
	} else {
		reg_val = DSI_LPC_UNIT_EN;

		writel(lpc_te_con0_val, comp->regs + DSI_LPC_TE_CON0(index));
		writel(dsi_lpc_fake_te_prd, comp->regs+ DSI_LPC_TE_CON1(index));

		reg_val |= DSI_LPC_UNIT_REST;
		writel(reg_val , comp->regs + DSI_LPC_CONFIG(index));

		reg_val &= ~DSI_LPC_UNIT_REST;
		writel(reg_val , comp->regs + DSI_LPC_CONFIG(index));

		writel(lpc_te_con0_val | DSI_LPC_HW_VSYNC_ON, comp->regs + DSI_LPC_TE_CON0(index));
	}

	DDPINFO("%s,[%d]lpc_te_con0_val:0x%x,real_te_duration:%d,cmdq:%p\n", __func__,
		index, lpc_te_con0_val, real_te_duration, cmdq_handle);
	DRM_MMP_MARK(dsi_lpc, lpc_te_con0_val,real_te_duration);
	drm_trace_tag_value("lpc_te_con0_val", lpc_te_con0_val);
	drm_trace_tag_value("real_te_duration", real_te_duration);
}
void mtk_dsi_lpc_init_config(struct drm_crtc *crtc, struct mtk_ddp_comp *comp)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_panel_params *params = NULL;
	struct mtk_drm_private *priv = NULL;
	struct mtk_dsi_lpc *lpc = comp_to_dsi_lpc(comp);
	int index = mtk_dsi_lpc_unit(mtk_crtc);
	unsigned int dsi_lpc_te_con = 0;
	unsigned long dsi_lpc_fake_te_prd = 0;
	bool lpc_en = true;
	bool te_en = false;

	drm_trace_tag_start("lpc_init_config");

	set_pl_kernel_offset(comp);

	params = mtk_drm_get_lcm_ext_params(crtc);
	if (!params)
		lpc_en = false;

	if (!mtk_drm_lcm_is_connect(mtk_crtc))
		lpc_en = false;

	if (!mtk_crtc_is_frame_trigger_mode(&mtk_crtc->base))
		lpc_en = false;

	priv = mtk_crtc->base.dev->dev_private;
	if (!priv)
		lpc_en = false;

	if (!priv->data->support_lpc)
		lpc_en = false;

	if (!mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_DSI_LPC_EN))
		lpc_en = false;

	mtk_set_dsi_lpc_en(comp, lpc_en);

	if (lpc_en) {
		mtk_dsi_lpc_update_fake_te_info(lpc, params->real_te_duration,
			&dsi_lpc_te_con, &dsi_lpc_fake_te_prd);

		writel(dsi_lpc_fake_te_prd, comp->regs + DSI_LPC_TE_CON1(index));

		if (params->skip_vblank)
			dsi_lpc_te_con |= params->skip_vblank << 8;
		writel(dsi_lpc_te_con, comp->regs+ DSI_LPC_TE_CON0(index));

		DDPINFO("%s, lpc_en:%d, dsi_lpc_te_con:0x%x, fake_te_prd:%lu\n", __func__,
			lpc_en, dsi_lpc_te_con, dsi_lpc_fake_te_prd);

		te_en = lpc->dsi_lpc_te_irq_en | mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_HRT_DEBUG);
		mtk_dsi_lpc_set_te_en(mtk_crtc, te_en);
	}

	mtk_dsi_set_lpc_en(lpc_en, comp);
	mtk_dsi_lpc_unit_en(lpc_en, index, comp);

	drm_trace_tag_end("lpc_init_config");
}
static int mtk_dsi_lpc_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			  enum mtk_ddp_io_cmd cmd, void *params)
{
	if (!comp) {
		DDPMSG("%s: error, comp=NULL!\n", __func__);
		return -INVALID;
	}

	if(mtk_ddp_comp_get_type(comp->id) != MTK_DSI_LPC) {
		DDPMSG("%s: comp = %s, return!\n", __func__, mtk_dump_comp_str(comp));
		return -INVALID;
	}

	if (!(comp->mtk_crtc && comp->mtk_crtc->base.dev)) {
		DDPMSG("%s %s %u has invalid CRTC or device\n",
			__func__, mtk_dump_comp_str(comp), cmd);
		return -INVALID;
	}
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;

	switch(cmd) {
	case DSI_LPC_INIT_CONFIG:
	{
		mtk_dsi_lpc_init_config(&mtk_crtc->base, comp);
	}
		break;
	case DSI_LPC_GET_SOF_TS:
	{
		long long *ts = (long long *)params;

		mtk_dsi_lpc_sof_ts(ts,mtk_crtc, comp);
	}
		break;
	case DSI_LPC_GET_RESYNC_TS:
	{
		long long *ts = (long long *)params;

		mtk_dsi_lpc_resync_ts(ts,mtk_crtc, comp);
	}
		break;
	case DSI_LPC_IRQ_EN:
	{
		bool en = (bool *)params;

		return mtk_dsi_lpc_interrupt_enable(mtk_crtc, comp, en);
	}
		break;
	case DSI_LPC_PANEL_PARAMS:
	{
		struct mtk_panel_params *panel_params = (struct mtk_panel_params *)params;

		mtk_dsi_lpc_update_panel_params(mtk_crtc, comp, handle, panel_params);
	}
		break;
	case DSI_LPC_GET_SOF_STATUS:
	{
		int *sof_status = (int *)params;
		struct mtk_dsi_lpc *lpc = comp_to_dsi_lpc(comp);

		if (lpc->lpc_te_type == DSI_FIXED_TE)
			*sof_status = 1;
		else if (lpc->lpc_te_type == DSI_MULTI_TE)
			*sof_status = lpc->lpc_sof_status;
		else
			*sof_status = 0;
	}
		break;
	case DSI_LPC_GET_EN:
	{
		bool *lpc_en = (bool *)params;
		struct mtk_dsi_lpc *lpc = comp_to_dsi_lpc(comp);
		int index = mtk_dsi_lpc_unit(comp->mtk_crtc);

		if (index != 0) {
			DDPDBG("%s: only support dsi0\n", __func__);
			*lpc_en = false;
		} else
			*lpc_en = lpc->dsi_lpc_en;
	}
		break;
	default:
		break;
	}

	return 0;
}
static const struct mtk_ddp_comp_funcs mtk_dsi_lpc_funcs = {
	//.start = mtk_dsi_lpc_start,
	.stop = mtk_dsi_lpc_stop,
	.prepare = mtk_dsi_lpc_prepare,
	.unprepare = mtk_dsi_lpc_unprepare,
	//.config = mtk_dsi_lpc_config,,
	.io_cmd = mtk_dsi_lpc_io_cmd,
};
static int mtk_dsi_lpc_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct mtk_dsi_lpc *priv = dev_get_drvdata(dev);
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
static void mtk_dsi_lpc_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_dsi_lpc  *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}
static const struct component_ops mtk_dsi_lpc_component_ops = {
	.bind = mtk_dsi_lpc_bind, .unbind = mtk_dsi_lpc_unbind,
};
static irqreturn_t mtk_dsi_lpc_irq_handler(int irq, void *dev_id)
{
	struct mtk_dsi_lpc *dsi_lpc = dev_id;
	struct mtk_ddp_comp *comp = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	unsigned int status = 0;
	unsigned int ret = 0;
	int index = 0;

	comp = &dsi_lpc->ddp_comp;
	if (IS_ERR_OR_NULL(comp))
		return IRQ_NONE;

	if (mtk_drm_top_clk_isr_get(comp) == false) {
		drm_trace_tag_mark("lpc_irq_handler error 1");
		DDPIRQ("%s, top clk off\n", __func__);
		return IRQ_NONE;
	}
	mtk_crtc = comp->mtk_crtc;
	if (!mtk_crtc) {
		drm_trace_tag_mark("lpc_irq_handler error 2");
		DDPPR_ERR("%s mtk_crtc is NULL\n", __func__);
		ret = IRQ_NONE;
		goto out;
	}

	if (!mtk_crtc->base.dev) {
		drm_trace_tag_mark("lpc_irq_handler error 3");
		DDPPR_ERR("%s mtk_crtc->base.dev is NULL\n", __func__);
		ret = IRQ_NONE;
		goto out;
	}

	index = drm_crtc_index(&mtk_crtc->base);

	index = mtk_dsi_lpc_unit(mtk_crtc);
	status = readl(comp->regs + DSI_LPC_INSTA(index));
	if (!status) {
		drm_trace_tag_mark("lpc_irq_handler error 4");
		ret = IRQ_NONE;
		goto out;
	}

	if (!dsi_lpc->dsi_lpc_te_irq_en) {
		drm_trace_tag_mark("lpc_irq_handler error 5");
		status &= ~EVENT_TE_INT;
	}

	DDPIRQ("%s irq, val:0x%x\n", mtk_dump_comp_str(comp), status);

	if (status) {
		unsigned long event_te_ts_diff = 0;

		mtk_dsi_lpc_event_te_ts(&event_te_ts_diff, mtk_crtc, comp);
		drm_trace_tag_value("lpc_te_ts", event_te_ts_diff);
		writel(~status, comp->regs + DSI_LPC_INSTA(index));

		if (status & REPORTED_RESYNC_INT) {
			if (index == 0)
				DRM_MMP_MARK(dsi_lpc0, status, 0);

			drm_trace_tag_mark("lpc_resync_irq");
			mtk_crtc_vblank_irq_for_lpc_resync(&mtk_crtc->base);
		}

		if (status & EVENT_TE_INT) {
			DRM_MMP_MARK(dsi_lpc0_te, status, event_te_ts_diff);
			drm_trace_tag_mark("lpc_te_irq");
		}

		if (status & MIPI_ERROR_FLAG_INT) {
			DDPPR_ERR("DSI LPC DDIC ERROR\n");
			DRM_MMP_MARK(dsi_lpc0, status, 0xFFFF);
			drm_trace_tag_mark("lpc_ddic_error_irq");
		}
	}

	ret = IRQ_HANDLED;
out:
	mtk_drm_top_clk_isr_put(comp);

	return ret;
}

static int mtk_dsi_lpc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_dsi_lpc *dsi_lpc;
	const struct mtk_dsi_lpc_data *lpc_data;
	enum mtk_ddp_comp_id comp_id;
	int ret;
	int irq, num_irqs;

	DDPFUNC("+");
	dsi_lpc = devm_kzalloc(dev, sizeof(*dsi_lpc), GFP_KERNEL);
	if (!dsi_lpc)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DSI_LPC);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	lpc_data = (const struct mtk_dsi_lpc_data *)of_device_get_match_data(dev);
	ret = mtk_ddp_comp_init(dev, dev->of_node, &dsi_lpc->ddp_comp, comp_id,
				&mtk_dsi_lpc_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}
	dsi_lpc->data = lpc_data;

	platform_set_drvdata(pdev, dsi_lpc);

	num_irqs = platform_irq_count(pdev);

	if (num_irqs) {

		irq = platform_get_irq(pdev, 0);
		if (irq < 0)
			return irq;

		ret = devm_request_irq(dev, irq, mtk_dsi_lpc_irq_handler,
					   IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(dev),
					   dsi_lpc);
		if (ret < 0) {
			DDPAEE("%s:%d, failed to request irq:%d ret:%d comp_id:%d\n",
					__func__, __LINE__,
					irq, ret, comp_id);
			return ret;
		}
	}
	dsi_lpc->dsi_lpc_en = true;

	mtk_ddp_comp_pm_enable(&dsi_lpc->ddp_comp);

	ret = component_add(dev, &mtk_dsi_lpc_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&dsi_lpc->ddp_comp);
	}
	set_pl_kernel_offset(&dsi_lpc->ddp_comp);
	DDPFUNC("-");

	return ret;
}
static void mtk_dsi_lpc_remove(struct platform_device *pdev)
{
	struct mtk_dsi_lpc *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_dsi_lpc_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);
}
static const struct of_device_id mtk_dsi_lpc_driver_dt_match[] = {
	{.compatible = "mediatek,mt6993-dsi-lpc",
		.data = (void *)&lpc_data_mt6993,},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_dsi_lpc_driver_dt_match);

struct platform_driver mtk_dsi_lpc_driver = {
	.probe = mtk_dsi_lpc_probe,
	.remove = mtk_dsi_lpc_remove,
	.driver = {
			.name = "mediatek-dsi-lpc",
			.owner = THIS_MODULE,
			.of_match_table = mtk_dsi_lpc_driver_dt_match,
		},
};
