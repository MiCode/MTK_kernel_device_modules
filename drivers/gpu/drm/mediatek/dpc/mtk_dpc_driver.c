// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/suspend.h>
#if IS_ENABLED(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#endif

#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <soc/mediatek/mmdvfs_v3.h>
#include "mtk-mmdvfs-v3-memory.h"

#include "mtk_dpc.h"
#include "mtk_dpc_mmp.h"
#include "mtk_dpc_internal.h"

#include "mtk_disp_vidle.h"
#include "mtk-mml-dpc.h"
#include "mdp_dpc.h"

int debug_mmp = 1;
module_param(debug_mmp, int, 0644);
int debug_dvfs;
module_param(debug_dvfs, int, 0644);
int debug_check_reg;
module_param(debug_check_reg, int, 0644);
int debug_check_rtff;
module_param(debug_check_rtff, int, 0644);
int debug_check_event;
module_param(debug_check_event, int, 0644);
int debug_mtcmos_off;
module_param(debug_mtcmos_off, int, 0644);
int debug_irq;
module_param(debug_irq, int, 0644);
int mminfra_ao;
module_param(mminfra_ao, int, 0644);
int mtcmos_ao;
module_param(mtcmos_ao, int, 0644);

/* TODO: move to mtk_dpc_test.c */
#define SPM_REQ_STA_4 0x85C	/* D1: BIT30 APSRC_REQ, DDRSRC_REQ */
#define SPM_REQ_STA_5 0x860	/* D2: BIT0 EMI_REQ, D3: BIT4 MAINPLL_REQ, D4: MMINFRA_REQ */
#define SPM_MMINFRA_PWR_CON 0xEA8
#define SPM_DISP_VCORE_PWR_CON 0xE8C
#define SPM_PWR_ACK BIT(30)	/* mt_spm_reg.h */

#define DPC_DEBUG_RTFF_CNT 10
static void __iomem *debug_rtff[DPC_DEBUG_RTFF_CNT];

#define DPC_SYS_REGS_CNT 7
static const char *reg_names[DPC_SYS_REGS_CNT] = {
	"DPC_BASE",
	"VLP_BASE",
	"SPM_BASE",
	"hw_vote_status",
	"vdisp_dvsrc_debug_sta_7",
	"dvfsrc_en",
	"dvfsrc_debug_sta_1",
};
/* TODO: move to mtk_dpc_test.c */

static void __iomem *dpc_base;

struct mtk_dpc {
	struct platform_device *pdev;
	struct device *dev;
	struct device *pd_dev;
	struct notifier_block pm_nb;
	int disp_irq;
	int mml_irq;
	resource_size_t dpc_pa;
	void __iomem *spm_base;
	void __iomem *vlp_base;
	void __iomem *mminfra_voter_check;
	void __iomem *mminfra_hfrp_pwr;
	void __iomem *vdisp_dvfsrc_check;
	void __iomem *vcore_dvfsrc_check;
	void __iomem *dvfsrc_en;
	struct cmdq_client *cmdq_client;
	atomic_t dpc_en_cnt;
	bool skip_force_power;
#if IS_ENABLED(CONFIG_DEBUG_FS)
	struct dentry *fs;
#endif
	struct mtk_dpc_dvfs_bw dvfs_bw;
	struct mutex bw_dvfs_mutex;
};
static struct mtk_dpc *g_priv;

/*
 * EOF                                                      TE
 *  | OFF0 |                                     | SAFEZONE | OFF1 |
 *  |      |         OVL OFF                     |          |      | OVL OFF |
 *  |      |<-100->| DISP1 OFF           |<-100->|          |      | <-100-> | DISP1 OFF
 *  |      |<-100->| MMINFRA OFF |<-800->|       |          |      | <-100-> | MMINFRA OFF
 *  |      |       |             |       |       |          |      |         |
 *  |      OFF     OFF           ON      ON      ON         |      OFF       OFF
 *         0       4,11          12      5       1                 3         7,13
 */

#define DT_TE_60 16000
#define DT_TE_120 8000
#define DT_TE_SAFEZONE 350
#define DT_OFF0 38000
#define DT_OFF1 500
#define DT_PRE_DISP1_OFF 100
#define DT_POST_DISP1_OFF 100
#define DT_PRE_MMINFRA_OFF 100
#define DT_POST_MMINFRA_OFF 800 /* infra267 + mminfra300 + margin */

#define DT_12 (DT_TE_120 - DT_TE_SAFEZONE - DT_POST_DISP1_OFF - DT_POST_MMINFRA_OFF)
#define DT_5  (DT_TE_120 - DT_TE_SAFEZONE - DT_POST_DISP1_OFF)
#define DT_1  (DT_TE_120 - DT_TE_SAFEZONE)
#define DT_6  (DT_TE_120 - DT_TE_SAFEZONE + 50)
#define DT_3  (DT_OFF1)
#define DT_7  (DT_OFF1 + DT_PRE_DISP1_OFF)
#define DT_13 (DT_OFF1 + DT_PRE_MMINFRA_OFF)

static struct mtk_dpc_dt_usage mt6989_disp_dt_usage[DPC_DISP_DT_CNT] = {
	/* OVL0/OVL1/DISP0 */
	{0, DPC_SP_FRAME_DONE,	40329,	DPC_DISP_VIDLE_MTCMOS},		/* OFF Time 0 */
	{1, DPC_SP_TE,		DT_1,	DPC_DISP_VIDLE_MTCMOS},		/* ON Time */
	{2, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_MTCMOS},		/* Pre-TE */
	{3, DPC_SP_TE,		DT_3,	DPC_DISP_VIDLE_MTCMOS},		/* OFF Time 1 */

	/* DISP1 */
	{4, DPC_SP_FRAME_DONE,	40329,	DPC_DISP_VIDLE_MTCMOS_DISP1},
	{5, DPC_SP_TE,		DT_5,	DPC_DISP_VIDLE_MTCMOS_DISP1},
	{6, DPC_SP_TE,		DT_6,	DPC_DISP_VIDLE_MTCMOS_DISP1},	/* DISP1-TE */
	{7, DPC_SP_TE,		DT_7,	DPC_DISP_VIDLE_MTCMOS_DISP1},

	/* VDISP DVFS, follow DISP1 by default, or HRT BW */
	{8, DPC_SP_FRAME_DONE,	40329,	DPC_DISP_VIDLE_VDISP_DVFS},
	{9, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_VDISP_DVFS},
	{10, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_VDISP_DVFS},

	/* HRT BW */
	{11, DPC_SP_FRAME_DONE,	40329,	DPC_DISP_VIDLE_HRT_BW},		/* OFF Time 0 */
	{12, DPC_SP_TE,		DT_12,	DPC_DISP_VIDLE_HRT_BW},		/* ON Time */
	{13, DPC_SP_TE,		DT_13,	DPC_DISP_VIDLE_HRT_BW},		/* OFF Time 1 */

	/* SRT BW, follow HRT BW by default, or follow DISP1 */
	{14, DPC_SP_FRAME_DONE,	40329,	DPC_DISP_VIDLE_SRT_BW},
	{15, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_SRT_BW},
	{16, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_SRT_BW},

	/* MMINFRA OFF, follow DISP1 by default, or HRT BW */
	{17, DPC_SP_FRAME_DONE,	40329,	DPC_DISP_VIDLE_MMINFRA_OFF},
	{18, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_MMINFRA_OFF},
	{19, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_MMINFRA_OFF},

	/* INFRA OFF, follow DISP1 by default, or HRT BW */
	{20, DPC_SP_FRAME_DONE,	40329,	DPC_DISP_VIDLE_INFRA_OFF},
	{21, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_INFRA_OFF},
	{22, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_INFRA_OFF},

	/* MAINPLL OFF, follow DISP1 by default, or HRT BW */
	{23, DPC_SP_FRAME_DONE,	40329,	DPC_DISP_VIDLE_MAINPLL_OFF},
	{24, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_MAINPLL_OFF},
	{25, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_MAINPLL_OFF},

	/* MSYNC 2.0 */
	{26, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_MSYNC2_0},
	{27, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_MSYNC2_0},
	{28, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_MSYNC2_0},

	/* RESERVED */
	{29, DPC_SP_FRAME_DONE,	3000,	DPC_DISP_VIDLE_RESERVED},
	{30, DPC_SP_TE,		16000,	DPC_DISP_VIDLE_RESERVED},
	{31, DPC_SP_TE,		40329,	DPC_DISP_VIDLE_RESERVED},
};

static struct mtk_dpc_dt_usage mt6989_mml_dt_usage[DPC_MML_DT_CNT] = {
	/* MML1 */
	{32, DPC_SP_RROT_DONE,	40329,	DPC_MML_VIDLE_MTCMOS},		/* OFF Time 0 */
	{33, DPC_SP_TE,		DT_1,	DPC_MML_VIDLE_MTCMOS},		/* ON Time */
	{34, DPC_SP_TE,		40329,	DPC_MML_VIDLE_MTCMOS},		/* MML-TE */
	{35, DPC_SP_TE,		DT_3,	DPC_MML_VIDLE_MTCMOS},		/* OFF Time 1 */

	/* VDISP DVFS, follow MML1 by default, or HRT BW */
	{36, DPC_SP_RROT_DONE,	40329,	DPC_MML_VIDLE_VDISP_DVFS},
	{37, DPC_SP_TE,		40329,	DPC_MML_VIDLE_VDISP_DVFS},
	{38, DPC_SP_TE,		40329,	DPC_MML_VIDLE_VDISP_DVFS},

	/* HRT BW */
	{39, DPC_SP_RROT_DONE,	40329,	DPC_MML_VIDLE_HRT_BW},		/* OFF Time 0 */
	{40, DPC_SP_TE,		DT_12,	DPC_MML_VIDLE_HRT_BW},		/* ON Time */
	{41, DPC_SP_TE,		DT_13,	DPC_MML_VIDLE_HRT_BW},		/* OFF Time 1 */

	/* SRT BW, follow HRT BW by default, or follow MML1 */
	{42, DPC_SP_RROT_DONE,	40329,	DPC_MML_VIDLE_SRT_BW},
	{43, DPC_SP_TE,		40329,	DPC_MML_VIDLE_SRT_BW},
	{44, DPC_SP_TE,		40329,	DPC_MML_VIDLE_SRT_BW},

	/* MMINFRA OFF, follow MML1 by default, or HRT BW */
	{45, DPC_SP_RROT_DONE,	40329,	DPC_MML_VIDLE_MMINFRA_OFF},
	{46, DPC_SP_TE,		40329,	DPC_MML_VIDLE_MMINFRA_OFF},
	{47, DPC_SP_TE,		40329,	DPC_MML_VIDLE_MMINFRA_OFF},

	/* INFRA OFF, follow MML1 by default, or HRT BW */
	{48, DPC_SP_RROT_DONE,	40329,	DPC_MML_VIDLE_INFRA_OFF},
	{49, DPC_SP_TE,		40329,	DPC_MML_VIDLE_INFRA_OFF},
	{50, DPC_SP_TE,		40329,	DPC_MML_VIDLE_INFRA_OFF},

	/* MAINPLL OFF, follow MML1 by default, or HRT BW */
	{51, DPC_SP_RROT_DONE,	40329,	DPC_MML_VIDLE_MAINPLL_OFF},
	{52, DPC_SP_TE,		40329,	DPC_MML_VIDLE_MAINPLL_OFF},
	{53, DPC_SP_TE,		40329,	DPC_MML_VIDLE_MAINPLL_OFF},

	/* RESERVED */
	{54, DPC_SP_RROT_DONE,	3000,	DPC_MML_VIDLE_RESERVED},
	{55, DPC_SP_TE,		16000,	DPC_MML_VIDLE_RESERVED},
	{56, DPC_SP_TE,		40329,	DPC_MML_VIDLE_RESERVED},
};

static inline int dpc_pm_ctrl(bool en)
{
	if (!g_priv->pd_dev)
		return 0;

	return en ? pm_runtime_resume_and_get(g_priv->pd_dev) : pm_runtime_put_sync(g_priv->pd_dev);
}

static inline bool dpc_pm_check_and_get(void)
{
	if (!g_priv->pd_dev)
		return false;

	return pm_runtime_get_if_in_use(g_priv->pd_dev) > 0 ? true : false;
}

static int mtk_disp_wait_pwr_ack(const enum mtk_dpc_subsys subsys)
{
	int ret = 0;
	u32 value = 0;
	u32 addr = 0;

	switch (subsys) {
	case DPC_SUBSYS_DISP0:
		addr = DISP_REG_DPC_DISP0_DEBUG1;
		break;
	case DPC_SUBSYS_DISP1:
		addr = DISP_REG_DPC_DISP1_DEBUG1;
		break;
	case DPC_SUBSYS_OVL0:
		addr = DISP_REG_DPC_OVL0_DEBUG1;
		break;
	case DPC_SUBSYS_OVL1:
		addr = DISP_REG_DPC_OVL1_DEBUG1;
		break;
	case DPC_SUBSYS_MML1:
		addr = DISP_REG_DPC_MML1_DEBUG1;
		break;
	default:
		/* unknown subsys type */
		return ret;
	}

	/* delay_us, timeout_us */
	ret = readl_poll_timeout_atomic(dpc_base + addr, value, 0xB, 1, 200);
	if (ret < 0)
		DPCERR("wait subsys(%d) power on timeout", subsys);

	return ret;
}

static void dpc_dt_enable(u16 dt, bool en)
{
	u32 value = 0;
	u32 addr = 0;

	if (dt < DPC_DISP_DT_CNT) {
		addr = DISP_REG_DPC_DISP_DT_EN;
	} else {
		addr = DISP_REG_DPC_MML_DT_EN;
		dt -= DPC_DISP_DT_CNT;
	}

	value = readl(dpc_base + addr);
	if (en)
		writel(BIT(dt) | value, dpc_base + addr);
	else
		writel(~BIT(dt) & value, dpc_base + addr);
}

static void dpc_dt_set(u16 dt, u32 us)
{
	u32 value = us * 26;	/* 26M base, 20 bits range, 38.46 ns ~ 38.46 ms*/

	writel(value, dpc_base + DISP_REG_DPC_DTx_COUNTER(dt));
}

static void dpc_dt_sw_trig(u16 dt)
{
	DPCFUNC("dt(%u)", dt);
	writel(1, dpc_base + DISP_REG_DPC_DTx_SW_TRIG(dt));
}

static void dpc_disp_group_enable(const enum mtk_dpc_disp_vidle group, bool en)
{
	int i;
	u32 value = 0;

	switch (group) {
	case DPC_DISP_VIDLE_MTCMOS:
		/* MTCMOS auto_on_off enable, both ack */
		value = en ? (BIT(0) | BIT(4)) : 0;
		writel(value, dpc_base + DISP_REG_DPC_DISP0_MTCMOS_CFG);
		writel(value, dpc_base + DISP_REG_DPC_OVL0_MTCMOS_CFG);
		writel(value, dpc_base + DISP_REG_DPC_OVL1_MTCMOS_CFG);
		break;
	case DPC_DISP_VIDLE_MTCMOS_DISP1:
		/* MTCMOS auto_on_off enable, both ack, pwr off dependency */
		value = en ? (BIT(0) | BIT(4) | BIT(6)) : 0;
		writel(value, dpc_base + DISP_REG_DPC_DISP1_MTCMOS_CFG);

		/* DDR_SRC and EMI_REQ DT is follow DISP1 */
		value = en ? 0x00010001 : 0x000D000D;
		writel(value, dpc_base + DISP_REG_DPC_DISP_DDRSRC_EMIREQ_CFG);
		break;
	case DPC_DISP_VIDLE_VDISP_DVFS:
		value = en ? 0 : 1;
		writel(value, dpc_base + DISP_REG_DPC_DISP_VDISP_DVFS_CFG);
		break;
	case DPC_DISP_VIDLE_HRT_BW:
	case DPC_DISP_VIDLE_SRT_BW:
		value = en ? 0 : 0x00010001;
		writel(value, dpc_base + DISP_REG_DPC_DISP_HRTBW_SRTBW_CFG);
		break;
	case DPC_DISP_VIDLE_MMINFRA_OFF:
	case DPC_DISP_VIDLE_INFRA_OFF:
	case DPC_DISP_VIDLE_MAINPLL_OFF:
		/* TODO: check SEL is 0b00 or 0b10 for ALL_PWR_ACK */
		value = en ? 0 : 0x181818;
		if (mminfra_ao)
			value = 0x181818;
		writel(value, dpc_base + DISP_REG_DPC_DISP_INFRA_PLL_OFF_CFG);
		break;
	default:
		break;
	}

	if (!en) {
		for (i = 0; i < DPC_DISP_DT_CNT; ++i) {
			if (group == mt6989_disp_dt_usage[i].group)
				dpc_dt_enable(mt6989_disp_dt_usage[i].index, false);
		}
		return;
	}
	for (i = 0; i < DPC_DISP_DT_CNT; ++i) {
		if (group == mt6989_disp_dt_usage[i].group) {
			dpc_dt_set(mt6989_disp_dt_usage[i].index, mt6989_disp_dt_usage[i].ep);
			// dpc_dt_enable(mt6989_disp_dt_usage[i].index, true);
		}
	}
}

static void dpc_mml_group_enable(const enum mtk_dpc_mml_vidle group, bool en)
{
	int i;
	u32 value = 0;

	switch (group) {
	case DPC_MML_VIDLE_MTCMOS:
		/* MTCMOS auto_on_off enable, both ack */
		value = en ? (BIT(0) | BIT(4)) : 0;
		writel(value, dpc_base + DISP_REG_DPC_MML1_MTCMOS_CFG);

		/* DDR_SRC and EMI_REQ DT is follow MML1 */
		value = en ? 0x00010001 : 0x000D000D;
		writel(value, dpc_base + DISP_REG_DPC_MML_DDRSRC_EMIREQ_CFG);
		break;
	case DPC_MML_VIDLE_VDISP_DVFS:
		value = en ? 0 : 1;
		writel(value, dpc_base + DISP_REG_DPC_MML_VDISP_DVFS_CFG);
		break;
	case DPC_MML_VIDLE_HRT_BW:
	case DPC_MML_VIDLE_SRT_BW:
		value = en ? 0 : 0x00010001;
		writel(value, dpc_base + DISP_REG_DPC_MML_HRTBW_SRTBW_CFG);
		break;
	case DPC_MML_VIDLE_MMINFRA_OFF:
	case DPC_MML_VIDLE_INFRA_OFF:
	case DPC_MML_VIDLE_MAINPLL_OFF:
		/* TODO: check SEL is 0b00 or 0b10 for ALL_PWR_ACK */
		value = en ? 0 : 0x181818;
		if (mminfra_ao)
			value = 0x181818;
		writel(value, dpc_base + DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG);
		break;
	default:
		break;
	}

	if (!en) {
		for (i = 0; i < DPC_MML_DT_CNT; ++i) {
			if (group == mt6989_mml_dt_usage[i].group)
				dpc_dt_enable(mt6989_mml_dt_usage[i].index, false);
		}
		return;
	}
	for (i = 0; i < DPC_MML_DT_CNT; ++i) {
		if (group == mt6989_mml_dt_usage[i].group) {
			dpc_dt_set(mt6989_mml_dt_usage[i].index, mt6989_mml_dt_usage[i].ep);
			// dpc_dt_enable(mt6989_mml_dt_usage[i].index, true);
		}
	}
}

void dpc_ddr_force_enable(const enum mtk_dpc_subsys subsys, const bool en)
{
	u32 addr = 0;
	u32 value = en ? 0x000D000D : 0x00050005;

	if (dpc_pm_ctrl(true))
		return;

	if (subsys == DPC_SUBSYS_DISP)
		addr = DISP_REG_DPC_DISP_DDRSRC_EMIREQ_CFG;
	else if (subsys == DPC_SUBSYS_MML)
		addr = DISP_REG_DPC_MML_DDRSRC_EMIREQ_CFG;

	writel(value, dpc_base + addr);

	dpc_pm_ctrl(false);
}
EXPORT_SYMBOL(dpc_ddr_force_enable);

void dpc_infra_force_enable(const enum mtk_dpc_subsys subsys, const bool en)
{
	u32 addr = 0;
	u32 value = en ? 0x00181818 : 0x00080808;

	if (dpc_pm_ctrl(true))
		return;

	if (subsys == DPC_SUBSYS_DISP)
		addr = DISP_REG_DPC_DISP_INFRA_PLL_OFF_CFG;
	else if (subsys == DPC_SUBSYS_MML)
		addr = DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG;

	if (mminfra_ao)
		value = 0x181818;
	writel(value, dpc_base + addr);

	dpc_pm_ctrl(false);
}
EXPORT_SYMBOL(dpc_infra_force_enable);

void dpc_dc_force_enable(const bool en)
{
	if (dpc_pm_ctrl(true))
		return;

	if (en) {
		writel(0x0, dpc_base + DISP_REG_DPC_MML_MASK_CFG);
		writel(0x00010001,
			dpc_base + DISP_REG_DPC_MML_DDRSRC_EMIREQ_CFG);
		writel(0x0, dpc_base + DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG);
		writel(0xFFFFFFFF, dpc_base + DISP_REG_DPC_MML_DT_EN);
		writel(0x3, dpc_base + DISP_REG_DPC_MML_DT_SW_TRIG_EN);
		writel(0x1, dpc_base + DISP_REG_DPC_DTx_SW_TRIG(33));
	} else {
		writel(0x1, dpc_base + DISP_REG_DPC_DTx_SW_TRIG(32));
		writel(0x0, dpc_base + DISP_REG_DPC_MML_DT_SW_TRIG_EN);
	}

	dpc_pm_ctrl(false);
}
EXPORT_SYMBOL(dpc_dc_force_enable);

void dpc_enable(bool en)
{
#ifdef IF_ZERO
	s32 cur_dpc_en_cnt;

	if (en) {
		cur_dpc_en_cnt = atomic_inc_return(&g_priv->dpc_en_cnt);
		if (cur_dpc_en_cnt > 1)
			return;
		else if (cur_dpc_en_cnt <= 0) {
			DPCERR("en %d cur_dpc_en_cnt %d", en, cur_dpc_en_cnt);
			return;
		}
	} else {
		cur_dpc_en_cnt = atomic_dec_return(&g_priv->dpc_en_cnt);
		if (cur_dpc_en_cnt > 0)
			return;
		else if (cur_dpc_en_cnt < 0) {
			DPCERR("en %d cur_dpc_en_cnt %d", en, cur_dpc_en_cnt);
			return;
		}
	}
#endif
	if (dpc_pm_ctrl(true))
		return;

	if (en) {
		/* DT enable only 1, 3, 5, 6, 7, 12, 13, 29, 30, 31 */
		writel(0xe00030ea, dpc_base + DISP_REG_DPC_DISP_DT_EN);

		/* DT enable only 1, 3, 8, 9 */
		writel(0x30a, dpc_base + DISP_REG_DPC_MML_DT_EN);

		writel(DISP_DPC_EN | DISP_DPC_DT_EN, dpc_base + DISP_REG_DPC_EN);
		dpc_mmp(config, MMPROFILE_FLAG_PULSE, U32_MAX, 1);
	} else {
		/* disable inten to avoid burst irq */
		writel(0, dpc_base + DISP_REG_DPC_DISP_INTEN);
		writel(0, dpc_base + DISP_REG_DPC_MML_INTEN);

		writel(0, dpc_base + DISP_REG_DPC_EN);

		/* reset dpc to clean counter start and value */
		writel(1, dpc_base + DISP_REG_DPC_RESET);
		writel(0, dpc_base + DISP_REG_DPC_RESET);
		dpc_mmp(config, MMPROFILE_FLAG_PULSE, U32_MAX, 0);
	}

	/* enable gce event */
	writel(en, dpc_base + DISP_REG_DPC_EVENT_EN);

	DPCFUNC("en(%u)", en);

	dpc_pm_ctrl(false);
}
EXPORT_SYMBOL(dpc_enable);

void dpc_hrt_bw_set(const enum mtk_dpc_subsys subsys, const u32 bw_in_mb, bool force)
{
	u32 addr1 = 0, addr2 = 0;

	if (dpc_pm_ctrl(true))
		return;

	if (subsys == DPC_SUBSYS_DISP) {
		addr1 = DISP_REG_DPC_DISP_HIGH_HRT_BW;
		addr2 = DISP_REG_DPC_DISP_HRTBW_SRTBW_CFG;
	} else if (subsys == DPC_SUBSYS_MML) {
		addr1 = DISP_REG_DPC_MML_SW_HRT_BW;
		addr2 = DISP_REG_DPC_MML_HRTBW_SRTBW_CFG;
	}
	writel(bw_in_mb / 30 + 1, dpc_base + addr1); /* 30MB unit */
	writel(force ? 0x00010001 : 0, dpc_base + addr2);

	if (unlikely(debug_dvfs))
		DPCFUNC("subsys(%u) hrt bw(%u)MB force(%u)", subsys, bw_in_mb, force);

	dpc_pm_ctrl(false);
}
EXPORT_SYMBOL(dpc_hrt_bw_set);

void dpc_srt_bw_set(const enum mtk_dpc_subsys subsys, const u32 bw_in_mb, bool force)
{
	u32 addr1 = 0, addr2 = 0;

	if (dpc_pm_ctrl(true))
		return;

	if (subsys == DPC_SUBSYS_DISP) {
		addr1 = DISP_REG_DPC_DISP_SW_SRT_BW;
		addr2 = DISP_REG_DPC_DISP_HRTBW_SRTBW_CFG;
	} else if (subsys == DPC_SUBSYS_MML) {
		addr1 = DISP_REG_DPC_MML_SW_SRT_BW;
		addr2 = DISP_REG_DPC_MML_HRTBW_SRTBW_CFG;
	}
	writel(bw_in_mb / 100 + 1, dpc_base + addr1); /* 100MB unit */
	writel(force ? 0x00010001 : 0, dpc_base + addr2);

	if (unlikely(debug_dvfs))
		DPCFUNC("subsys(%u) srt bw(%u)MB force(%u)", subsys, bw_in_mb, force);

	dpc_pm_ctrl(false);
}
EXPORT_SYMBOL(dpc_srt_bw_set);


static u8 dpc_max_dvfs_level(void)
{
	u8 max_level;

	max_level = max(g_priv->dvfs_bw.disp_dvfs_level,
		g_priv->dvfs_bw.mml_dvfs_level);
	max_level = max(max_level, g_priv->dvfs_bw.bw_level);

	return max_level;
}

void dpc_dvfs_set(const enum mtk_dpc_subsys subsys, const u8 level, bool force)
{
	u32 addr1 = 0, addr2 = 0;
	u8 max_level;

	/* support 575, 600, 650, 700, 750 mV */
	if (level > 4) {
		DPCERR("vdisp support only 5 levels");
		return;
	}

	if (dpc_pm_ctrl(true))
		return;

	mutex_lock(&g_priv->bw_dvfs_mutex);

	if (subsys == DPC_SUBSYS_DISP) {
		addr1 = DISP_REG_DPC_DISP_VDISP_DVFS_VAL;
		addr2 = DISP_REG_DPC_DISP_VDISP_DVFS_CFG;
		g_priv->dvfs_bw.disp_dvfs_level = level;
	} else if (subsys == DPC_SUBSYS_MML) {
		addr1 = DISP_REG_DPC_MML_VDISP_DVFS_VAL;
		addr2 = DISP_REG_DPC_MML_VDISP_DVFS_CFG;
		g_priv->dvfs_bw.mml_dvfs_level = level;
	}

	writel(level, dpc_base + addr1);
	writel(force ? 1 : 0, dpc_base + addr2);

	if (unlikely(debug_dvfs))
		DPCFUNC("subsys(%u) vdisp level(%u) force(%u)", subsys, level, force);

	max_level = dpc_max_dvfs_level();
	if (max_level > level)
		writel(max_level, dpc_base + DISP_REG_DPC_DISP_VDISP_DVFS_VAL);

	if (unlikely(debug_dvfs) && max_level > level)
		DPCFUNC("subsys(%u) vdisp level(%u) force(%u)", subsys, max_level, force);

	mutex_unlock(&g_priv->bw_dvfs_mutex);

	dpc_pm_ctrl(false);
}
EXPORT_SYMBOL(dpc_dvfs_set);

void dpc_dvfs_bw_set(const enum mtk_dpc_subsys subsys, const u32 bw_in_mb)
{
	u8 level = 0;
	u32 total_bw;
	u8 max_level;

	mutex_lock(&g_priv->bw_dvfs_mutex);

	if (subsys == DPC_SUBSYS_DISP)
		g_priv->dvfs_bw.disp_bw = bw_in_mb * 10 / 7;
	else if (subsys == DPC_SUBSYS_MML)
		g_priv->dvfs_bw.mml_bw = bw_in_mb * 10 / 7;

	total_bw = g_priv->dvfs_bw.disp_bw + g_priv->dvfs_bw.mml_bw;

	if (total_bw > 6988)
		level = 4;
	else if (total_bw > 5129)
		level = 3;
	else if (total_bw > 4076)
		level = 2;
	else if (total_bw > 3057)
		level = 1;
	else
		level = 0;

	g_priv->dvfs_bw.bw_level = level;

	if (dpc_pm_ctrl(true))
		return;

	max_level = dpc_max_dvfs_level();
	writel(max_level, dpc_base + DISP_REG_DPC_DISP_VDISP_DVFS_VAL);

	if (unlikely(debug_dvfs))
		DPCFUNC("subsys(%u) disp_bw(%u) mml_bw(%u) vdisp level(%u)",
			subsys, g_priv->dvfs_bw.disp_bw, g_priv->dvfs_bw.mml_bw, max_level);

	mutex_unlock(&g_priv->bw_dvfs_mutex);

	dpc_pm_ctrl(false);
}
EXPORT_SYMBOL(dpc_dvfs_bw_set);

void dpc_group_enable(const u16 group, bool en)
{
	if (dpc_pm_ctrl(true))
		return;

	if (group <= DPC_DISP_VIDLE_RESERVED)
		dpc_disp_group_enable((enum mtk_dpc_disp_vidle)group, en);
	else if (group <= DPC_MML_VIDLE_RESERVED)
		dpc_mml_group_enable((enum mtk_dpc_mml_vidle)group, en);
	else
		DPCERR("group(%u) is not defined", group);

	dpc_pm_ctrl(false);
}
EXPORT_SYMBOL(dpc_group_enable);

void dpc_config(const enum mtk_dpc_subsys subsys, bool en)
{
	if (dpc_pm_ctrl(true))
		return;

	if (!en) {
		if (subsys == DPC_SUBSYS_DISP) {
			dpc_mtcmos_vote(DPC_SUBSYS_DISP1, 6, 1);
			mtk_disp_wait_pwr_ack(DPC_SUBSYS_DISP1);
			dpc_mtcmos_vote(DPC_SUBSYS_DISP0, 6, 1);
			mtk_disp_wait_pwr_ack(DPC_SUBSYS_DISP0);
			dpc_mtcmos_vote(DPC_SUBSYS_OVL0, 6, 1);
			mtk_disp_wait_pwr_ack(DPC_SUBSYS_OVL0);
			dpc_mtcmos_vote(DPC_SUBSYS_OVL1, 6, 1);
			mtk_disp_wait_pwr_ack(DPC_SUBSYS_OVL1);
			if (readl(dpc_base + DISP_REG_DPC_MML1_MTCMOS_CFG)) {
				dpc_mtcmos_vote(DPC_SUBSYS_MML1, 6, 1);
				mtk_disp_wait_pwr_ack(DPC_SUBSYS_MML1);
			}
		} else {
			dpc_mtcmos_vote(DPC_SUBSYS_MML1, 6, 1);
			mtk_disp_wait_pwr_ack(DPC_SUBSYS_MML1);
		}
		udelay(30);
	}

	if (subsys == DPC_SUBSYS_DISP) {
		writel(en ? 0 : U32_MAX, dpc_base + DISP_REG_DPC_DISP_MASK_CFG);
		writel(0x1f, dpc_base + DISP_REG_DPC_DISP_EXT_INPUT_EN);
		writel(0x3ff, dpc_base + DISP_REG_DPC_DISP_DT_FOLLOW_CFG); /* all follow 11~13 */

		dpc_group_enable(DPC_DISP_VIDLE_MTCMOS, en);
		dpc_group_enable(DPC_DISP_VIDLE_MTCMOS_DISP1, en);
		dpc_group_enable(DPC_DISP_VIDLE_VDISP_DVFS, en);
		dpc_group_enable(DPC_DISP_VIDLE_HRT_BW, en);
		dpc_group_enable(DPC_DISP_VIDLE_MMINFRA_OFF, en);
	} else if (subsys == DPC_SUBSYS_MML) {
		writel(en ? 0 : U32_MAX, dpc_base + DISP_REG_DPC_MML_MASK_CFG);
		writel(0x3, dpc_base + DISP_REG_DPC_MML_EXT_INPUT_EN);
		writel(0x3ff, dpc_base + DISP_REG_DPC_MML_DT_FOLLOW_CFG); /* all follow 39~41 */

		dpc_group_enable(DPC_MML_VIDLE_MTCMOS, en);
		dpc_group_enable(DPC_MML_VIDLE_VDISP_DVFS, en);
		dpc_group_enable(DPC_MML_VIDLE_HRT_BW, en);
		dpc_group_enable(DPC_MML_VIDLE_MMINFRA_OFF, en);
	}

	if (en) {
		/* pwr on delay default 100 + 50 us, modify to 30 us */
		writel(0x30c, dpc_base + 0xa44);
		writel(0x30c, dpc_base + 0xb44);
		writel(0x30c, dpc_base + 0xc44);
		writel(0x30c, dpc_base + 0xd44);
		writel(0x30c, dpc_base + 0xe44);

		if (subsys == DPC_SUBSYS_DISP) {
			dpc_mtcmos_vote(DPC_SUBSYS_DISP1, 6, 0);
			dpc_mtcmos_vote(DPC_SUBSYS_DISP0, 6, 0);
			dpc_mtcmos_vote(DPC_SUBSYS_OVL0, 6, 0);
			dpc_mtcmos_vote(DPC_SUBSYS_OVL1, 6, 0);
			if (readl(dpc_base + DISP_REG_DPC_MML1_MTCMOS_CFG)) {
				dpc_mtcmos_vote(DPC_SUBSYS_MML1, 6, 0);
				mtk_disp_wait_pwr_ack(DPC_SUBSYS_MML1);
			}
		} else
			dpc_mtcmos_vote(DPC_SUBSYS_MML1, 6, 0);
	}

	writel(0, dpc_base + DISP_REG_DPC_MERGE_DISP_INT_CFG);
	writel(0, dpc_base + DISP_REG_DPC_MERGE_MML_INT_CFG);

	/* wla ddren ack */
	writel(1, dpc_base + DISP_REG_DPC_DDREN_ACK_SEL);

	if (debug_irq) {
		/* enable irq for DISP0 OVL0 OVL1 DISP1 */
		writel(DISP_DPC_INT_DISP1_ON | DISP_DPC_INT_DISP1_OFF |
			DISP_DPC_INT_OVL0_ON | DISP_DPC_INT_OVL0_OFF |
			DISP_DPC_INT_MMINFRA_OFF_END | DISP_DPC_INT_MMINFRA_OFF_START |
			DISP_DPC_INT_DT6 | DISP_DPC_INT_DT3,
			dpc_base + DISP_REG_DPC_DISP_INTEN);
		writel(0x33000, dpc_base + DISP_REG_DPC_MML_INTEN);
	}

	if (mtcmos_ao) {
		dpc_mtcmos_vote(DPC_SUBSYS_DISP1, 5, 1);
		dpc_mtcmos_vote(DPC_SUBSYS_DISP0, 5, 1);
		dpc_mtcmos_vote(DPC_SUBSYS_OVL0, 5, 1);
		dpc_mtcmos_vote(DPC_SUBSYS_OVL1, 5, 1);
		dpc_mtcmos_vote(DPC_SUBSYS_MML1, 5, 1);
	} else {
		dpc_mtcmos_vote(DPC_SUBSYS_DISP1, 5, 0);
		dpc_mtcmos_vote(DPC_SUBSYS_DISP0, 5, 0);
		dpc_mtcmos_vote(DPC_SUBSYS_OVL0, 5, 0);
		dpc_mtcmos_vote(DPC_SUBSYS_OVL1, 5, 0);
		dpc_mtcmos_vote(DPC_SUBSYS_MML1, 5, 0);
	}

	dpc_mmp(config, MMPROFILE_FLAG_PULSE, BIT(subsys), en);

	dpc_pm_ctrl(false);
}
EXPORT_SYMBOL(dpc_config);

void dpc_mtcmos_vote(const enum mtk_dpc_subsys subsys, const u8 thread, const bool en)
{
	u32 addr = 0;

	if (dpc_pm_ctrl(true))
		return;

	/* CLR : execute SW threads, disable auto MTCMOS */
	switch (subsys) {
	case DPC_SUBSYS_DISP0:
		addr = en ? DISP_REG_DPC_DISP0_THREADx_CLR(thread)
			  : DISP_REG_DPC_DISP0_THREADx_SET(thread);
		break;
	case DPC_SUBSYS_DISP1:
		addr = en ? DISP_REG_DPC_DISP1_THREADx_CLR(thread)
			  : DISP_REG_DPC_DISP1_THREADx_SET(thread);
		break;
	case DPC_SUBSYS_OVL0:
		addr = en ? DISP_REG_DPC_OVL0_THREADx_CLR(thread)
			  : DISP_REG_DPC_OVL0_THREADx_SET(thread);
		break;
	case DPC_SUBSYS_OVL1:
		addr = en ? DISP_REG_DPC_OVL1_THREADx_CLR(thread)
			  : DISP_REG_DPC_OVL1_THREADx_SET(thread);
		break;
	case DPC_SUBSYS_MML1:
		addr = en ? DISP_REG_DPC_MML1_THREADx_CLR(thread)
			  : DISP_REG_DPC_MML1_THREADx_SET(thread);
		break;
	default:
		break;
	}

	writel(1, dpc_base + addr);
	// DPCFUNC("subsys(%u:%u) addr(0x%x) vote %s", subsys, thread, addr, en ? "SET" : "CLR");
	dpc_pm_ctrl(false);
}
EXPORT_SYMBOL(dpc_mtcmos_vote);

irqreturn_t mtk_dpc_disp_irq_handler(int irq, void *dev_id)
{
	struct mtk_dpc *priv = dev_id;
	u32 status;

	if (IS_ERR_OR_NULL(priv))
		return IRQ_NONE;

	if (dpc_pm_ctrl(true)) {
		dpc_mmp(mminfra, MMPROFILE_FLAG_PULSE, U32_MAX, 0);
		return IRQ_NONE;
	}

	status = readl(dpc_base + DISP_REG_DPC_DISP_INTSTA);
	if (!status) {
		dpc_pm_ctrl(false);
		return IRQ_NONE;
	}

	writel(~status, dpc_base + DISP_REG_DPC_DISP_INTSTA);

	if (likely(debug_mmp)) {
		if (status & DISP_DPC_INT_DT6)
			dpc_mmp(prete, MMPROFILE_FLAG_PULSE, 0, 0);

		if (status & DISP_DPC_INT_DT3)
			dpc_mmp(idle_off, MMPROFILE_FLAG_PULSE, 0, 3);

		if (status & DISP_DPC_INT_MMINFRA_OFF_END)
			dpc_mmp(mminfra, MMPROFILE_FLAG_START, 0, 0);
		if (status & DISP_DPC_INT_MMINFRA_OFF_START)
			dpc_mmp(mminfra, MMPROFILE_FLAG_END, 0, 0);

		if (status & DISP_DPC_INT_DT1)
			dpc_mmp(mtcmos_ovl0, MMPROFILE_FLAG_PULSE, 0, 0);
		if (status & DISP_DPC_INT_OVL0_ON)
			dpc_mmp(mtcmos_ovl0, MMPROFILE_FLAG_START, 0, 0);
		if (status & DISP_DPC_INT_OVL0_OFF)
			dpc_mmp(mtcmos_ovl0, MMPROFILE_FLAG_END, 0, 0);

		if (status & DISP_DPC_INT_DT5)
			dpc_mmp(mtcmos_disp1, MMPROFILE_FLAG_PULSE, 0, 0);
		if (status & DISP_DPC_INT_DISP1_ON)
			dpc_mmp(mtcmos_disp1, MMPROFILE_FLAG_START, 0, 0);
		if (status & DISP_DPC_INT_DISP1_OFF)
			dpc_mmp(mtcmos_disp1, MMPROFILE_FLAG_END, 0, 0);
	}

	if (unlikely(debug_check_reg)) {
		if (status & DISP_DPC_INT_DT29) {	/* should be the last off irq */
			DPCFUNC("\tOFF MMINFRA(%u) VDISP(%u) SRT&HRT(%#x) D1(%u) D234(%u)",
				readl(priv->mminfra_voter_check) & BIT(6) ? 1 : 0,
				(readl(priv->vdisp_dvfsrc_check) & 0x1C) >> 2,
				readl(priv->vcore_dvfsrc_check) & 0xFFFFF,
				(readl(priv->spm_base + SPM_REQ_STA_4) & BIT(30)) ? 1 : 0,
				((readl(priv->spm_base + SPM_REQ_STA_5) & 0x13) == 0x13) ? 1 : 0);
		}
		if (status & DISP_DPC_INT_DT30) {	/* should be the last on irq */
			DPCFUNC("\tON MMINFRA(%u) VDISP(%u) SRT&HRT(%#x) D1(%u) D234(%u)",
				readl(priv->mminfra_voter_check) & BIT(6) ? 1 : 0,
				(readl(priv->vdisp_dvfsrc_check) & 0x1C) >> 2,
				readl(priv->vcore_dvfsrc_check) & 0xFFFFF,
				(readl(priv->spm_base + SPM_REQ_STA_4) & BIT(30)) ? 1 : 0,
				((readl(priv->spm_base + SPM_REQ_STA_5) & 0x13) == 0x13) ? 1 : 0);
		}
	}

	if (unlikely(debug_check_rtff && (status & 0x600000))) {
		int i, sum = 0;

		for (i = 0; i < DPC_DEBUG_RTFF_CNT; i++)
			sum += readl(debug_rtff[i]);

		if (status & DISP_DPC_INT_DT29)			/* should be the last off irq */
			DPCFUNC("\tOFF rtff(%#x)", sum);
		if (status & DISP_DPC_INT_DT30)			/* should be the last on irq */
			DPCFUNC("\tON rtff(%#x)", sum);
	}

	if (unlikely(debug_check_event)) {
		if (status & DISP_DPC_INT_DT29) {	/* should be the last off irq */
			DPCFUNC("\tOFF event(%#06x)", readl(dpc_base + DISP_REG_DPC_DUMMY0));
			writel(0, dpc_base + DISP_REG_DPC_DUMMY0);
		}
		if (status & DISP_DPC_INT_DT30) {	/* should be the last on irq */
			DPCFUNC("\tON event(%#06x)", readl(dpc_base + DISP_REG_DPC_DUMMY0));
			writel(0, dpc_base + DISP_REG_DPC_DUMMY0);
		}
	}

	if (unlikely(debug_mtcmos_off && (status & 0xFF000000))) {
		if (status & DISP_DPC_INT_OVL0_OFF)
			DPCDUMP("OVL0 OFF");
		if (status & DISP_DPC_INT_OVL1_OFF)
			DPCDUMP("OVL1 OFF");
		if (status & DISP_DPC_INT_DISP0_OFF)
			DPCDUMP("DISP0 OFF");
		if (status & DISP_DPC_INT_DISP1_OFF)
			DPCDUMP("DISP1 OFF");
		if (status & DISP_DPC_INT_OVL0_ON)
			DPCDUMP("OVL0 ON");
		if (status & DISP_DPC_INT_OVL1_ON)
			DPCDUMP("OVL1 ON");
		if (status & DISP_DPC_INT_DISP0_ON)
			DPCDUMP("DISP0 ON");
		if (status & DISP_DPC_INT_DISP1_ON)
			DPCDUMP("DISP1 ON");
	}

	dpc_pm_ctrl(false);

	return IRQ_HANDLED;
}

irqreturn_t mtk_dpc_mml_irq_handler(int irq, void *dev_id)
{
	struct mtk_dpc *priv = dev_id;
	u32 status;

	if (IS_ERR_OR_NULL(priv))
		return IRQ_NONE;

	if (dpc_pm_ctrl(true)) {
		dpc_mmp(mminfra, MMPROFILE_FLAG_PULSE, U32_MAX, 0);
		return IRQ_NONE;
	}

	status = readl(dpc_base + DISP_REG_DPC_MML_INTSTA);
	if (!status) {
		dpc_pm_ctrl(false);
		return IRQ_NONE;
	}

	writel(~status, dpc_base + DISP_REG_DPC_MML_INTSTA);

	if (likely(debug_mmp)) {
		if (status & BIT(16))
			dpc_mmp(mml_sof, MMPROFILE_FLAG_PULSE, 0, 0);
		if (status & BIT(17))
			dpc_mmp(mml_rrot_done, MMPROFILE_FLAG_PULSE, 0, 0);

		if (status & BIT(13))
			dpc_mmp(mtcmos_mml1, MMPROFILE_FLAG_START, 0, 0);
		if (status & BIT(12))
			dpc_mmp(mtcmos_mml1, MMPROFILE_FLAG_END, 0, 0);
	}

	if (debug_mtcmos_off) {
		if (status & BIT(13))
			DPCDUMP("MML1 ON");
		if (status & BIT(12))
			DPCDUMP("MML1 OFF");
	}

	dpc_pm_ctrl(false);

	return IRQ_HANDLED;
}

static int dpc_res_init(struct mtk_dpc *priv)
{
	int i;
	int ret = 0;
	struct resource *res;
	static void __iomem *sys_va[DPC_SYS_REGS_CNT];

	for (i = 0; i < DPC_SYS_REGS_CNT; i++) {
		res = platform_get_resource_byname(priv->pdev, IORESOURCE_MEM, reg_names[i]);
		if (res == NULL) {
			DPCERR("miss reg in node");
			return ret;
		}
		sys_va[i] = devm_ioremap_resource(priv->dev, res);

		if (!priv->dpc_pa)
			priv->dpc_pa = res->start;
	}

	dpc_base = sys_va[0];
	priv->vlp_base = sys_va[1];
	priv->spm_base = sys_va[2];
	priv->mminfra_voter_check = sys_va[3];
	priv->vdisp_dvfsrc_check = sys_va[4];
	priv->vcore_dvfsrc_check = sys_va[5];
	priv->dvfsrc_en = sys_va[6];

	/* FIXME: can't request region for resource */
	if (IS_ERR_OR_NULL(priv->spm_base))
		priv->spm_base = ioremap(0x1c001000, 0x1000);
	if (IS_ERR_OR_NULL(priv->vdisp_dvfsrc_check))
		priv->vdisp_dvfsrc_check = ioremap(0x1ec352b8, 0x4);
	if (IS_ERR_OR_NULL(priv->vcore_dvfsrc_check))
		priv->vcore_dvfsrc_check = ioremap(0x1c00f2a0, 0x4);
	if (IS_ERR_OR_NULL(priv->dvfsrc_en))
		priv->dvfsrc_en = ioremap(0x1c00f000, 0x4);

	priv->mminfra_hfrp_pwr = ioremap(0x1ec3eea8, 0x4);

	return ret;
}

static int dpc_irq_init(struct mtk_dpc *priv)
{
	int ret = 0;
	int num_irqs;

	num_irqs = platform_irq_count(priv->pdev);
	if (num_irqs <= 0) {
		DPCERR("unable to count IRQs");
		return -EPROBE_DEFER;
	} else if (num_irqs == 1) {
		priv->disp_irq = platform_get_irq(priv->pdev, 0);
	} else if (num_irqs == 2) {
		priv->disp_irq = platform_get_irq(priv->pdev, 0);
		priv->mml_irq = platform_get_irq(priv->pdev, 1);
	}

	if (priv->disp_irq > 0) {
		ret = devm_request_irq(priv->dev, priv->disp_irq, mtk_dpc_disp_irq_handler,
				       IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(priv->dev), priv);
		if (ret)
			DPCERR("devm_request_irq %d fail: %d", priv->disp_irq, ret);
	}
	if (priv->mml_irq > 0) {
		ret = devm_request_irq(priv->dev, priv->mml_irq, mtk_dpc_mml_irq_handler,
				       IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(priv->dev), priv);
		if (ret)
			DPCERR("devm_request_irq %d fail: %d", priv->mml_irq, ret);
	}
	DPCFUNC("disp irq %d, mml irq %d, ret %d", priv->disp_irq, priv->mml_irq, ret);

	if (dpc_pm_ctrl(true))
		return ret;

	/* disable merge irq */
	writel(0, dpc_base + DISP_REG_DPC_MERGE_DISP_INT_CFG);
	writel(0, dpc_base + DISP_REG_DPC_MERGE_DISP_INTSTA);
	writel(0, dpc_base + DISP_REG_DPC_MERGE_MML_INT_CFG);
	writel(0, dpc_base + DISP_REG_DPC_MERGE_MML_INTSTA);

	dpc_pm_ctrl(false);

	return ret;
}

static void dpc_debug_event(void)
{
	u16 event_ovl0_on, event_ovl0_off, event_disp1_on, event_disp1_off;
	struct cmdq_pkt *pkt;

	of_property_read_u16(g_priv->dev->of_node, "event-ovl0-off", &event_ovl0_off);
	of_property_read_u16(g_priv->dev->of_node, "event-ovl0-on", &event_ovl0_on);
	of_property_read_u16(g_priv->dev->of_node, "event-disp1-off", &event_disp1_off);
	of_property_read_u16(g_priv->dev->of_node, "event-disp1-on", &event_disp1_on);

	if (!event_ovl0_off || !event_ovl0_on || !event_disp1_off || !event_disp1_on) {
		DPCERR("read event fail");
		return;
	}

	g_priv->cmdq_client = cmdq_mbox_create(g_priv->dev, 0);
	if (!g_priv->cmdq_client) {
		DPCERR("cmdq_mbox_create fail");
		return;
	}

	cmdq_mbox_enable(g_priv->cmdq_client->chan);
	pkt = cmdq_pkt_create(g_priv->cmdq_client);
	if (!pkt) {
		DPCERR("cmdq_handle is NULL");
		return;
	}

	cmdq_pkt_wfe(pkt, event_ovl0_off);
	cmdq_pkt_write(pkt, NULL, g_priv->dpc_pa + DISP_REG_DPC_DUMMY0, 0x1000, 0x1000);
	cmdq_pkt_wfe(pkt, event_disp1_off);
	cmdq_pkt_write(pkt, NULL, g_priv->dpc_pa + DISP_REG_DPC_DUMMY0, 0x0100, 0x0100);

	/* DT29 done, off irq handler read and clear */

	cmdq_pkt_wfe(pkt, event_disp1_on);
	cmdq_pkt_write(pkt, NULL, g_priv->dpc_pa + DISP_REG_DPC_DUMMY0, 0x0010, 0x0010);
	cmdq_pkt_wfe(pkt, event_ovl0_on);
	cmdq_pkt_write(pkt, NULL, g_priv->dpc_pa + DISP_REG_DPC_DUMMY0, 0x0001, 0x0001);

	/* DT30 done, on irq handler read and clear */

	cmdq_pkt_finalize_loop(pkt);
	cmdq_pkt_flush_threaded(pkt, NULL, (void *)pkt);
}

static void mtk_disp_vlp_vote(unsigned int vote_set, unsigned int thread)
{
	u32 addr = vote_set ? VLP_DISP_SW_VOTE_SET : VLP_DISP_SW_VOTE_CLR;
	u32 ack = vote_set ? BIT(thread) : 0;
	u32 val = 0;
	u16 i = 0;

	writel_relaxed(BIT(thread), g_priv->vlp_base + addr);
	do {
		writel_relaxed(BIT(thread), g_priv->vlp_base + addr);
		val = readl(g_priv->vlp_base + VLP_DISP_SW_VOTE_CON);
		if ((val & BIT(thread)) == ack)
			break;

		if (i > 2500) {
			DPCERR("vlp vote bit(%u) timeout", thread);
			return;
		}

		udelay(2);
		i++;
	} while (1);

	/* check voter only, later will use another API to power on mminfra */

	dpc_mmp(vlp_vote, MMPROFILE_FLAG_PULSE, BIT(thread) | vote_set, val);
}

int dpc_vidle_power_keep(const enum mtk_vidle_voter_user user)
{
	if (unlikely(g_priv->skip_force_power)) {
		DPCFUNC("user %u skip force power", user);
		return 0;
	}

	if (dpc_pm_ctrl(true))
		return -1;

	mtk_disp_vlp_vote(VOTE_SET, user);
	udelay(50);

	return 0;
}
EXPORT_SYMBOL(dpc_vidle_power_keep);

void dpc_vidle_power_release(const enum mtk_vidle_voter_user user)
{
	if (unlikely(g_priv->skip_force_power)) {
		DPCFUNC("user %u skip force power", user);
		return;
	}

	mtk_disp_vlp_vote(VOTE_CLR, user);

	dpc_pm_ctrl(false);
}
EXPORT_SYMBOL(dpc_vidle_power_release);

static void dpc_analysis(void)
{
	if (0 == (readl(g_priv->spm_base + SPM_PWR_STATUS_MSB) & BIT(3))) {
		DPCFUNC("disp vcore is not power on");
		return;
	}

	if (dpc_pm_ctrl(true))
		return;

	DPCDUMP("ddremi mminfra: (%#010x %#08x)(%#010x %#08x)",
		readl(dpc_base + DISP_REG_DPC_DISP_DDRSRC_EMIREQ_CFG),
		readl(dpc_base + DISP_REG_DPC_DISP_INFRA_PLL_OFF_CFG),
		readl(dpc_base + DISP_REG_DPC_MML_DDRSRC_EMIREQ_CFG),
		readl(dpc_base + DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG));
	DPCDUMP("hrt cfg val: (%#010x %#06x)(%#010x %#06x)",
		readl(dpc_base + DISP_REG_DPC_DISP_HRTBW_SRTBW_CFG),
		readl(dpc_base + DISP_REG_DPC_DISP_HIGH_HRT_BW),
		readl(dpc_base + DISP_REG_DPC_MML_HRTBW_SRTBW_CFG),
		readl(dpc_base + DISP_REG_DPC_MML_SW_HRT_BW));
	DPCDUMP("vdisp cfg val: (%#04x %#04x)(%#04x %#04x)",
		readl(dpc_base + DISP_REG_DPC_DISP_VDISP_DVFS_CFG),
		readl(dpc_base + DISP_REG_DPC_DISP_VDISP_DVFS_VAL),
		readl(dpc_base + DISP_REG_DPC_MML_VDISP_DVFS_CFG),
		readl(dpc_base + DISP_REG_DPC_MML_VDISP_DVFS_VAL));
	DPCDUMP("mtcmos: (%#04x %#04x %#04x %#04x %#04x)",
		readl(dpc_base + DISP_REG_DPC_DISP0_MTCMOS_CFG),
		readl(dpc_base + DISP_REG_DPC_DISP1_MTCMOS_CFG),
		readl(dpc_base + DISP_REG_DPC_OVL0_MTCMOS_CFG),
		readl(dpc_base + DISP_REG_DPC_OVL1_MTCMOS_CFG),
		readl(dpc_base + DISP_REG_DPC_MML1_MTCMOS_CFG));

	dpc_pm_ctrl(false);
}

static int dpc_pm_notifier(struct notifier_block *notifier, unsigned long pm_event, void *unused)
{
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		g_priv->skip_force_power = true;
		dpc_mmp(vlp_vote, MMPROFILE_FLAG_PULSE, U32_MAX, 0);
		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		g_priv->skip_force_power = false;
		dpc_mmp(vlp_vote, MMPROFILE_FLAG_PULSE, U32_MAX, 1);
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
static void process_dbg_opt(const char *opt)
{
	int ret = 0;
	u32 val = 0, v1 = 0, v2 = 0;

	if (0 == (readl(g_priv->spm_base + SPM_PWR_STATUS_MSB) & BIT(3))) {
		DPCFUNC("disp vcore is not power on");
		return;
	}

	if (dpc_pm_ctrl(true))
		return;

	if (strncmp(opt, "en:", 3) == 0) {
		ret = sscanf(opt, "en:%u\n", &val);
		if (ret != 1)
			goto err;
		dpc_enable((bool)val);
	} else if (strncmp(opt, "cfg:", 4) == 0) {
		ret = sscanf(opt, "cfg:%u\n", &val);
		if (ret != 1)
			goto err;
		if (val == 1) {
			const u32 dummy_regs[DPC_DEBUG_RTFF_CNT] = {
				0x14000400,
				0x1402040c,
				0x14200400,
				0x14206220,
				0x14402200,
				0x14403200,
				0x14602200,
				0x14603200,
				0x1f800400,
				0x1f81a100,
			};

			for (val = 0; val < DPC_DEBUG_RTFF_CNT; val++)
				debug_rtff[val] = ioremap(dummy_regs[val], 0x4);

			/* TODO: remove me after vcore dvfsrc ready */
			writel(0x1, g_priv->dvfsrc_en);

			/* default value for HRT and SRT */
			writel(0x66, dpc_base + DISP_REG_DPC_DISP_HIGH_HRT_BW);
			writel(0x154, dpc_base + DISP_REG_DPC_DISP_SW_SRT_BW);

			/* debug only, skip RROT Read done */
			writel(BIT(4), dpc_base + DISP_REG_DPC_MML_DT_CFG);

			dpc_config(DPC_SUBSYS_DISP, true);
			dpc_group_enable(DPC_DISP_VIDLE_RESERVED, true);
		} else {
			dpc_config(DPC_SUBSYS_DISP, false);
		}
	} else if (strncmp(opt, "mmlmutex", 8) == 0) {
		const u32 dummy_regs[6] = {
			0x1f801040,
			0x1f801044,
			0x1f801048,
			0x1f80104C,
			0x1f801050,
			0x1f801054,
		};
		for (val = 0; val < 6; val++)
			debug_rtff[val] = ioremap(dummy_regs[val], 0x4);
	} else if (strncmp(opt, "mmlcfg:", 7) == 0) {
		ret = sscanf(opt, "mmlcfg:%u\n", &val);
		if (ret != 1)
			goto err;
		dpc_config(DPC_SUBSYS_MML, (bool)val);
	} else if (strncmp(opt, "event", 5) == 0) {
		dpc_debug_event();
	} else if (strncmp(opt, "irq", 3) == 0) {
		dpc_irq_init(g_priv);
	} else if (strncmp(opt, "dump", 4) == 0) {
		dpc_analysis();
	} else if (strncmp(opt, "swmode:", 7) == 0) {
		ret = sscanf(opt, "swmode:%u\n", &val);
		if (ret != 1)
			goto err;
		if (val) {
			writel(0xFFFFFFFF, dpc_base + DISP_REG_DPC_DISP_DT_EN);
			writel(0xFFFFFFFF, dpc_base + DISP_REG_DPC_DISP_DT_SW_TRIG_EN);
		} else
			writel(0, dpc_base + DISP_REG_DPC_DISP_DT_SW_TRIG_EN);
	} else if (strncmp(opt, "trig:", 5) == 0) {
		ret = sscanf(opt, "trig:%u\n", &val);
		if (ret != 1)
			goto err;
		dpc_dt_sw_trig(val);
	} else if (strncmp(opt, "vdisp:", 6) == 0) {
		ret = sscanf(opt, "vdisp:%u\n", &val);
		if (ret != 1)
			goto err;
		dpc_dvfs_set(DPC_SUBSYS_DISP, val, true);
	} else if (strncmp(opt, "dt:", 3) == 0) {
		ret = sscanf(opt, "dt:%u,%u\n", &v1, &v2);
		if (ret != 2)
			goto err;
		dpc_dt_set((u16)v1, v2);
		if (v1 < DPC_DISP_DT_CNT) {
			DPCDUMP("disp dt(%u, %u->%u)", v1, mt6989_disp_dt_usage[v1].ep, v2);
			mt6989_disp_dt_usage[v1].ep = v2;
		} else {
			v1 -= DPC_DISP_DT_CNT;
			DPCDUMP("mml dt(%u, %u->%u)", v1, mt6989_mml_dt_usage[v1].ep, v2);
			mt6989_mml_dt_usage[v1].ep = v2;
		}
	} else if (strncmp(opt, "vote:", 5) == 0) {
		ret = sscanf(opt, "vote:%u,%u\n", &v1, &v2);
		if (ret != 2)
			goto err;
		dpc_mtcmos_vote(v1, 7, (bool)v2);
	} else if (strncmp(opt, "wr:", 3) == 0) {
		ret = sscanf(opt, "wr:0x%x=0x%x\n", &v1, &v2);
		if (ret != 2)
			goto err;
		DPCFUNC("(%#llx)=(%x)", (u64)(dpc_base + v1), v2);
		writel(v2, dpc_base + v1);
	} else if (strncmp(opt, "avs:", 4) == 0) {
		ret = sscanf(opt, "avs:%u,%u\n", &v1, &v2);
		if (ret != 2)
			goto err;
		writel(v2, MEM_VDISP_AVS_STEP(v1));
		mmdvfs_force_step_by_vcp(2, 4 - v1);
	} else if (strncmp(opt, "vdo", 3) == 0) {
		writel(DISP_DPC_EN|DISP_DPC_DT_EN|DISP_DPC_VDO_MODE, dpc_base + DISP_REG_DPC_EN);
	}

	dpc_pm_ctrl(false);

	return;
err:
	DPCERR();
}
static ssize_t fs_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos)
{
	const u32 debug_bufmax = 512 - 1;
	size_t ret;
	char cmd_buffer[512] = {0};
	char *tok, *buf;

	ret = count;

	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(&cmd_buffer, ubuf, count))
		return -EFAULT;

	cmd_buffer[count] = 0;
	buf = cmd_buffer;
	DPCFUNC("%s", cmd_buffer);

	while ((tok = strsep(&buf, " ")) != NULL)
		process_dbg_opt(tok);

	return ret;
}

static const struct file_operations debug_fops = {
	.write = fs_write,
};
#endif

static const struct dpc_funcs funcs = {
	.dpc_enable = dpc_enable,
	.dpc_ddr_force_enable = dpc_ddr_force_enable,
	.dpc_infra_force_enable = dpc_infra_force_enable,
	.dpc_dc_force_enable = dpc_dc_force_enable,
	.dpc_group_enable = dpc_group_enable,
	.dpc_config = dpc_config,
	.dpc_mtcmos_vote = dpc_mtcmos_vote,
	.dpc_vidle_power_keep = dpc_vidle_power_keep,
	.dpc_vidle_power_release = dpc_vidle_power_release,
	.dpc_hrt_bw_set = dpc_hrt_bw_set,
	.dpc_srt_bw_set = dpc_srt_bw_set,
	.dpc_dvfs_set = dpc_dvfs_set,
	.dpc_dvfs_bw_set = dpc_dvfs_bw_set,
	.dpc_analysis = dpc_analysis,
};

static int mtk_dpc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_dpc *priv;
	int ret = 0;

	DPCFUNC("+");
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	g_priv = priv;

	platform_set_drvdata(pdev, priv);
	priv->pdev = pdev;
	priv->dev = dev;

	if (of_find_property(dev->of_node, "power-domains", NULL)) {
		priv->pd_dev = dev;
		if (!pm_runtime_enabled(priv->pd_dev))
			pm_runtime_enable(priv->pd_dev);
		pm_runtime_irq_safe(priv->pd_dev);
	}

	ret = dpc_res_init(priv);
	if (ret)
		return ret;

	ret = dpc_irq_init(priv);
	if (ret)
		return ret;

	priv->pm_nb.notifier_call = dpc_pm_notifier;
	ret = register_pm_notifier(&priv->pm_nb);
	if (ret) {
		DPCERR("register_pm_notifier failed %d", ret);
		return ret;
	}

	/* enable external signal from DSI and TE */
	writel(0x1F, dpc_base + DISP_REG_DPC_DISP_EXT_INPUT_EN);
	writel(0x3, dpc_base + DISP_REG_DPC_MML_EXT_INPUT_EN);

	/* SW_CTRL and SW_VAL=1 */
	dpc_ddr_force_enable(DPC_SUBSYS_DISP, true);
	dpc_ddr_force_enable(DPC_SUBSYS_MML, true);
	writel(0x181818, dpc_base + DISP_REG_DPC_DISP_INFRA_PLL_OFF_CFG);
	writel(0x181818, dpc_base + DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG);

	/* keep vdisp opp */
	writel(0x1, dpc_base + DISP_REG_DPC_DISP_VDISP_DVFS_CFG);
	writel(0x1, dpc_base + DISP_REG_DPC_MML_VDISP_DVFS_CFG);

	/* keep HRT and SRT BW */
	writel(0x00010001, dpc_base + DISP_REG_DPC_DISP_HRTBW_SRTBW_CFG);
	writel(0x00010001, dpc_base + DISP_REG_DPC_MML_HRTBW_SRTBW_CFG);

#if IS_ENABLED(CONFIG_DEBUG_FS)
	priv->fs = debugfs_create_file("dpc_ctrl", S_IFREG | 0440, NULL, NULL, &debug_fops);
	if (IS_ERR(priv->fs))
		DPCERR("debugfs_create_file failed:%ld", PTR_ERR(priv->fs));
#endif

	dpc_mmp_init();

	mutex_init(&g_priv->bw_dvfs_mutex);

	mtk_vidle_register(&funcs);
	mml_dpc_register(&funcs);
	mdp_dpc_register(&funcs);

	DPCFUNC("-");
	return ret;
}

static int mtk_dpc_remove(struct platform_device *pdev)
{
	DPCFUNC();
	return 0;
}

static void mtk_dpc_shutdown(struct platform_device *pdev)
{
	struct mtk_dpc *priv = platform_get_drvdata(pdev);

	priv->skip_force_power = true;
}

static const struct of_device_id mtk_dpc_driver_dt_match[] = {
	{.compatible = "mediatek,mt6989-disp-dpc"},
	{.compatible = "mediatek,mt6985-disp-dpc"},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_dpc_driver_dt_match);

struct platform_driver mtk_dpc_driver = {
	.probe = mtk_dpc_probe,
	.remove = mtk_dpc_remove,
	.shutdown = mtk_dpc_shutdown,
	.driver = {
		.name = "mediatek-disp-dpc",
		.owner = THIS_MODULE,
		.of_match_table = mtk_dpc_driver_dt_match,
	},
};

static int __init mtk_dpc_init(void)
{
	DPCFUNC("+");
	platform_driver_register(&mtk_dpc_driver);
	DPCFUNC("-");
	return 0;
}

static void __exit mtk_dpc_exit(void)
{
	DPCFUNC();
}

module_init(mtk_dpc_init);
module_exit(mtk_dpc_exit);

MODULE_AUTHOR("William Yang <William-tw.Yang@mediatek.com>");
MODULE_DESCRIPTION("MTK Display Power Controller");
MODULE_LICENSE("GPL");
