// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2025 MediaTek Inc.

#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/interrupt.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/sched/clock.h>
#include <linux/sched/mm.h>
#include <linux/spmi.h>
#include <linux/irq.h>
#include "spmi-mtk.h"

#define PMRC_GIP_IRQDESC(name) { #name, pmrc_gip_##name##_irq_handler, -1}

struct pmrc_gip_irq_desc {
	const char *name;
	irq_handler_t irq_handler;
	int irq;
};

static const u32 mt6993_gip_regs[] = {
	[GIP_WRD_REQ_VIO_LOG_CLR] = 0x0034,
	[GIP_WRD_MPU_PMIC_VIO_LOG_CLR] = 0x030C,
	[GIP_IRQ_EVENT_EN_0] =	0x0390,
	[GIP_IRQ_EVENT_EN_1] =	0x0394,
	[GIP_IRQ_EVENT_EN_2] =	0x0398,
	[GIP_IRQ_EVENT_EN_3] =	0x039C,
	[GIP_IRQ_EVENT_EN_4] =  0x03A0,
	[GIP_IRQ_EVENT_EN_5] =  0x03A4,
	[GIP_WRD_IRQ_CLR_0] =   0x03A8,
	[GIP_WRD_IRQ_CLR_1] =   0x03AC,
	[GIP_WRD_IRQ_CLR_2] =   0x03B0,
	[GIP_WRD_IRQ_CLR_3] =   0x03B4,
	[GIP_WRD_IRQ_CLR_4] =   0x03B8,
	[GIP_WRD_IRQ_CLR_5] =   0x03BC,
	[GIP_IRQCTRL_DEBUG_IRQ_FLAG_0] = 0x0C74,
	[GIP_IRQCTRL_DEBUG_IRQ_FLAG_1] = 0x0C78,
	[GIP_IRQCTRL_DEBUG_IRQ_FLAG_2] = 0x0C7C,
	[GIP_IRQCTRL_DEBUG_IRQ_FLAG_3] = 0x0C80,
	[GIP_IRQCTRL_DEBUG_IRQ_FLAG_4] = 0x0C84,
	[GIP_IRQCTRL_DEBUG_IRQ_FLAG_5] = 0x0C88,
};

enum {
	/* 5 */
	IRQ_BIST_ERR_RISING = 4,
	IRQ_WAKEUP_TIMER_TICK = 3,
	IRQ_WDT_IRQ = 2,
	IRQ_HWINF_BYTECNT_VIO_15 = 1,
	IRQ_HWINF_BYTECNT_VIO_14 = 0,

	/* 4 */
	IRQ_HWINF_BYTECNT_VIO_13 = 31,
	IRQ_HWINF_BYTECNT_VIO_12 = 30,
	IRQ_HWINF_BYTECNT_VIO_11 = 29,
	IRQ_HWINF_BYTECNT_VIO_10 = 28,
	IRQ_HWINF_BYTECNT_VIO_9 = 27,
	IRQ_HWINF_BYTECNT_VIO_8 = 26,
	IRQ_HWINF_BYTECNT_VIO_7 = 25,
	IRQ_HWINF_BYTECNT_VIO_6 = 24,
	IRQ_HWINF_BYTECNT_VIO_5 = 23,
	IRQ_HWINF_BYTECNT_VIO_4 = 22,
	IRQ_HWINF_BYTECNT_VIO_3 = 21,
	IRQ_HWINF_BYTECNT_VIO_2 = 20,
	IRQ_HWINF_BYTECNT_VIO_1 = 19,
	IRQ_HWINF_BYTECNT_VIO_0 = 18,
	IRQ_HWINF_CMD_VIO_15 = 17,
	IRQ_HWINF_CMD_VIO_14 = 16,
	IRQ_HWINF_CMD_VIO_13 = 15,
	IRQ_HWINF_CMD_VIO_12 = 14,
	IRQ_HWINF_CMD_VIO_11 = 13,
	IRQ_HWINF_CMD_VIO_10 = 12,
	IRQ_HWINF_CMD_VIO_9 = 11,
	IRQ_HWINF_CMD_VIO_8 = 10,
	IRQ_HWINF_CMD_VIO_7 = 9,
	IRQ_HWINF_CMD_VIO_6 = 8,
	IRQ_HWINF_CMD_VIO_5 = 7,
	IRQ_HWINF_CMD_VIO_4 = 6,
	IRQ_HWINF_CMD_VIO_3 = 5,
	IRQ_HWINF_CMD_VIO_2 = 4,
	IRQ_HWINF_CMD_VIO_1 = 3,
	IRQ_HWINF_CMD_VIO_0 = 2,
	IRQ_HWINF_GIP_REQ_ERR_15 = 1,
	IRQ_HWINF_GIP_REQ_ERR_14 = 0,

	/* 3 */
	IRQ_HWINF_GIP_REQ_ERR_13 = 31,
	IRQ_HWINF_GIP_REQ_ERR_12 = 30,
	IRQ_HWINF_GIP_REQ_ERR_11 = 29,
	IRQ_HWINF_GIP_REQ_ERR_10 = 28,
	IRQ_HWINF_GIP_REQ_ERR_9 = 27,
	IRQ_HWINF_GIP_REQ_ERR_8 = 26,
	IRQ_HWINF_GIP_REQ_ERR_7 = 25,
	IRQ_HWINF_GIP_REQ_ERR_6 = 24,
	IRQ_HWINF_GIP_REQ_ERR_5 = 23,
	IRQ_HWINF_GIP_REQ_ERR_4 = 22,
	IRQ_HWINF_GIP_REQ_ERR_3 = 21,
	IRQ_HWINF_GIP_REQ_ERR_2 = 20,
	IRQ_HWINF_GIP_REQ_ERR_1 = 19,
	IRQ_HWINF_GIP_REQ_ERR_0 = 18,
	IRQ_HWINF_GIP_REQ_MISS_15 = 17,
	IRQ_HWINF_GIP_REQ_MISS_14 = 16,
	IRQ_HWINF_GIP_REQ_MISS_13 = 15,
	IRQ_HWINF_GIP_REQ_MISS_12 = 14,
	IRQ_HWINF_GIP_REQ_MISS_11 = 13,
	IRQ_HWINF_GIP_REQ_MISS_10 = 12,
	IRQ_HWINF_GIP_REQ_MISS_9 = 11,
	IRQ_HWINF_GIP_REQ_MISS_8 = 10,
	IRQ_HWINF_GIP_REQ_MISS_7 = 9,
	IRQ_HWINF_GIP_REQ_MISS_6 = 8,
	IRQ_HWINF_GIP_REQ_MISS_5 = 7,
	IRQ_HWINF_GIP_REQ_MISS_4 = 6,
	IRQ_HWINF_GIP_REQ_MISS_3 = 5,
	IRQ_HWINF_GIP_REQ_MISS_2 = 4,
	IRQ_HWINF_GIP_REQ_MISS_1 = 3,
	IRQ_HWINF_GIP_REQ_MISS_0 = 2,
	IRQ_HWINF_ERR_15 = 1,
	IRQ_HWINF_ERR_14 = 0,

	/* 2 */
	IRQ_HWINF_ERR_13 = 31,
	IRQ_HWINF_ERR_12 = 30,
	IRQ_HWINF_ERR_11 = 29,
	IRQ_HWINF_ERR_10 = 28,
	IRQ_HWINF_ERR_9 = 27,
	IRQ_HWINF_ERR_8 = 26,
	IRQ_HWINF_ERR_7 = 25,
	IRQ_HWINF_ERR_6 = 24,
	IRQ_HWINF_ERR_5 = 23,
	IRQ_HWINF_ERR_4 = 22,
	IRQ_HWINF_ERR_3 = 21,
	IRQ_HWINF_ERR_2 = 20,
	IRQ_HWINF_ERR_1 = 19,
	IRQ_HWINF_ERR_0 = 18,
	IRQ_ARBITER_CMDISSUE_REQ_MISS_1 = 17,
	IRQ_ARBITER_CMDISSUE_REQ_MISS_0 = 16,
	IRQ_LAT_LIMIT_REACHED_IRQ_15 = 15,
	IRQ_LAT_LIMIT_REACHED_IRQ_14 = 14,
	IRQ_LAT_LIMIT_REACHED_IRQ_13 = 13,
	IRQ_LAT_LIMIT_REACHED_IRQ_12 = 12,
	IRQ_LAT_LIMIT_REACHED_IRQ_11 = 11,
	IRQ_LAT_LIMIT_REACHED_IRQ_10 = 10,
	IRQ_LAT_LIMIT_REACHED_IRQ_9 = 9,
	IRQ_LAT_LIMIT_REACHED_IRQ_8 = 8,
	IRQ_LAT_LIMIT_REACHED_IRQ_7 = 7,
	IRQ_LAT_LIMIT_REACHED_IRQ_6 = 6,
	IRQ_LAT_LIMIT_REACHED_IRQ_5 = 5,
	IRQ_LAT_LIMIT_REACHED_IRQ_4 = 4,
	IRQ_LAT_LIMIT_REACHED_IRQ_3 = 3,
	IRQ_LAT_LIMIT_REACHED_IRQ_2 = 2,
	IRQ_LAT_LIMIT_REACHED_IRQ_1 = 1,
	IRQ_LAT_LIMIT_REACHED_IRQ_0 = 0,

	/* 1 */
	IRQ_PMIC_VIO_IRQ_15 = 31,
	IRQ_PMIC_VIO_IRQ_14 = 30,
	IRQ_PMIC_VIO_IRQ_13 = 29,
	IRQ_PMIC_VIO_IRQ_12 = 28,
	IRQ_PMIC_VIO_IRQ_11 = 27,
	IRQ_PMIC_VIO_IRQ_10 = 26,
	IRQ_PMIC_VIO_IRQ_9 = 25,
	IRQ_PMIC_VIO_IRQ_8 = 24,
	IRQ_PMIC_VIO_IRQ_7 = 23,
	IRQ_PMIC_VIO_IRQ_6 = 22,
	IRQ_PMIC_VIO_IRQ_5 = 21,
	IRQ_PMIC_VIO_IRQ_4 = 20,
	IRQ_PMIC_VIO_IRQ_3 = 19,
	IRQ_PMIC_VIO_IRQ_2 = 18,
	IRQ_PMIC_VIO_IRQ_1 = 17,
	IRQ_PMIC_VIO_IRQ_0 = 16,
	IRQ_REQ_VIO_IRQ_15 = 15,
	IRQ_REQ_VIO_IRQ_14 = 14,
	IRQ_REQ_VIO_IRQ_13 = 13,
	IRQ_REQ_VIO_IRQ_12 = 12,
	IRQ_REQ_VIO_IRQ_11 = 11,
	IRQ_REQ_VIO_IRQ_10 = 10,
	IRQ_REQ_VIO_IRQ_9 = 9,
	IRQ_REQ_VIO_IRQ_8 = 8,
	IRQ_REQ_VIO_IRQ_7 = 7,
	IRQ_REQ_VIO_IRQ_6 = 6,
	IRQ_REQ_VIO_IRQ_5 = 5,
	IRQ_REQ_VIO_IRQ_4 = 4,
	IRQ_REQ_VIO_IRQ_3 = 3,
	IRQ_REQ_VIO_IRQ_2 = 2,
	IRQ_REQ_VIO_IRQ_1 = 1,
	IRQ_REQ_VIO_IRQ_0 = 0,

	/* 0 */
	IRQ_REQ_ACK_VIO_IRQ_15 = 31,
	IRQ_REQ_ACK_VIO_IRQ_14 = 30,
	IRQ_REQ_ACK_VIO_IRQ_13 = 29,
	IRQ_REQ_ACK_VIO_IRQ_12 = 28,
	IRQ_REQ_ACK_VIO_IRQ_11 = 27,
	IRQ_REQ_ACK_VIO_IRQ_10 = 26,
	IRQ_REQ_ACK_VIO_IRQ_9 = 25,
	IRQ_REQ_ACK_VIO_IRQ_8 = 24,
	IRQ_REQ_ACK_VIO_IRQ_7 = 23,
	IRQ_REQ_ACK_VIO_IRQ_6 = 22,
	IRQ_REQ_ACK_VIO_IRQ_5 = 21,
	IRQ_REQ_ACK_VIO_IRQ_4 = 20,
	IRQ_REQ_ACK_VIO_IRQ_3 = 19,
	IRQ_REQ_ACK_VIO_IRQ_2 = 18,
	IRQ_REQ_ACK_VIO_IRQ_1 = 17,
	IRQ_REQ_ACK_VIO_IRQ_0 = 16,
	IRQ_DIFF_CMD_VIO_IRQ_15 = 15,
	IRQ_DIFF_CMD_VIO_IRQ_14 = 14,
	IRQ_DIFF_CMD_VIO_IRQ_13 = 13,
	IRQ_DIFF_CMD_VIO_IRQ_12 = 12,
	IRQ_DIFF_CMD_VIO_IRQ_11 = 11,
	IRQ_DIFF_CMD_VIO_IRQ_10 = 10,
	IRQ_DIFF_CMD_VIO_IRQ_9 = 9,
	IRQ_DIFF_CMD_VIO_IRQ_8 = 8,
	IRQ_DIFF_CMD_VIO_IRQ_7 = 7,
	IRQ_DIFF_CMD_VIO_IRQ_6 = 6,
	IRQ_DIFF_CMD_VIO_IRQ_5 = 5,
	IRQ_DIFF_CMD_VIO_IRQ_4 = 4,
	IRQ_DIFF_CMD_VIO_IRQ_3 = 3,
	IRQ_DIFF_CMD_VIO_IRQ_2 = 2,
	IRQ_DIFF_CMD_VIO_IRQ_1 = 1,
	IRQ_DIFF_CMD_VIO_IRQ_0 = 0,
};

static u32 gip_readl(void __iomem *addr, struct pmrc_gip *arb, enum gip_regs reg)
{
	return readl(addr + arb->data->pmrcgip_regs[reg]);
}

static void gip_writel(void __iomem *addr, struct pmrc_gip *arb, u32 val, enum gip_regs reg)
{
	writel(val, addr + arb->data->pmrcgip_regs[reg]);
}

static struct platform_driver mtk_pmrc_gip_driver;

static struct pmrc_gip_data mt6xxx_pmrc_gip_arb[] = {
	{
		.pmrcgip_regs = mt6993_gip_regs,
	},
};

static void gip_hwinf_bytecnt_vio_irq_handler(int irq_m, int irq_p, void *data, int idx)
{
	struct pmrc_gip *arb = data;

	pr_notice("[PMRC_GIP] %s hwintf %d bytecnt vio irq_m/p 0x%x/0x%x\n", __func__,
		idx - ((arb->hwintf_bytecnt_vio_idx[0]) % 32), irq_m, irq_p);
}

static void gip_hwinf_err_irq_handler(int irq_m, int irq_p, void *data, int idx)
{
	struct pmrc_gip *arb = data;

	pr_notice("[PMRC_GIP] %s hwintf %d err irq_m/p 0x%x/0x%x\n", __func__,
		idx - ((arb->hwintf_err_idx[0]) % 32), irq_m, irq_p);
}

static void gip_pmic_vio_irq_handler(int irq_m, int irq_p, void *data, int idx)
{
	struct pmrc_gip *arb = data;

	pr_notice("[PMRC_GIP] %s pmic_vio_irq[%d]  irq_m/p 0x%x/0x%x\n", __func__,
		idx - ((arb->pmic_vio_irq_idx[0]) % 32), irq_m, irq_p);

	if (irq_m) {
		gip_writel(arb->gip_sec_base[0], arb,
			(0x1 << (idx - ((arb->pmic_vio_irq_idx[0]) % 32))), GIP_WRD_MPU_PMIC_VIO_LOG_CLR);
	} else {
		gip_writel(arb->gip_sec_base[1], arb,
			(0x1 << (idx - ((arb->pmic_vio_irq_idx[0]) % 32))), GIP_WRD_MPU_PMIC_VIO_LOG_CLR);
	}
}

static void gip_req_vio_irq_handler(int irq_m, int irq_p, void *data, int idx)
{
	struct pmrc_gip *arb = data;

	pr_notice("[PMRC_GIP] %s req_vio_irq[%d]  irq_m/p 0x%x/0x%x\n", __func__,
		idx - (arb->req_vio_irq_idx[0]) % 32, irq_m, irq_p);

	if (irq_m) {
		gip_writel(arb->gip_sec_base[0], arb,
			(0x1 << (idx - ((arb->req_vio_irq_idx[0]) % 32))), GIP_WRD_REQ_VIO_LOG_CLR);
	} else {
		gip_writel(arb->gip_sec_base[1], arb,
			(0x1 << (idx - ((arb->req_vio_irq_idx[0]) % 32))), GIP_WRD_REQ_VIO_LOG_CLR);
	}
}

static bool check_irq_in_range(int idx, u32 start, u32 end)
{
	return (idx >= (start % 32)) && (idx <= (end % 32));
}

static irqreturn_t pmrc_gip_event_0_irq_handler(int irq, void *data)
{
	struct pmrc_gip *arb = data;
	int irq_f = 0, irq_f_p = 0, idx = 0;

	__pm_stay_awake(arb->pmrc_gip_m_Thread_lock);
	mutex_lock(&arb->pmrc_gip_m_mutex);

	irq_f = gip_readl(arb->gip_base[0], arb, GIP_IRQCTRL_DEBUG_IRQ_FLAG_0);
	if (!IS_ERR(arb->gip_base[1]))
		irq_f_p = gip_readl(arb->gip_base[1], arb, GIP_IRQCTRL_DEBUG_IRQ_FLAG_0);

	if ((irq_f == 0) && (irq_f_p == 0)) {
		mutex_unlock(&arb->pmrc_gip_m_mutex);
		__pm_relax(arb->pmrc_gip_m_Thread_lock);
		return IRQ_NONE;
	}

	for (idx = 0; idx < 32; idx++) {
		if (((irq_f & (0x1 << idx)) != 0) || ((irq_f_p & (0x1 << idx)) != 0)) {
			switch (idx) {
			default:
				pr_notice("%s IRQ[%d] triggered\n",
					__func__, idx);
			break;
			}
			if (irq_f)
				gip_writel(arb->gip_sec_base[0], arb, irq_f, GIP_WRD_IRQ_CLR_0);
			else if (irq_f_p)
				gip_writel(arb->gip_sec_base[1], arb, irq_f_p, GIP_WRD_IRQ_CLR_0);
			else
				pr_notice("%s IRQ[%d] is not cleared due to empty flags\n",
					__func__, idx);
			break;
		}
	}
	mutex_unlock(&arb->pmrc_gip_m_mutex);
	__pm_relax(arb->pmrc_gip_m_Thread_lock);

	return IRQ_HANDLED;
}

static irqreturn_t pmrc_gip_event_1_irq_handler(int irq, void *data)
{
	struct pmrc_gip *arb = data;
	int irq_f = 0, irq_f_p = 0, idx = 0;

	__pm_stay_awake(arb->pmrc_gip_m_Thread_lock);
	mutex_lock(&arb->pmrc_gip_m_mutex);

	irq_f = gip_readl(arb->gip_base[0], arb, GIP_IRQCTRL_DEBUG_IRQ_FLAG_1);
	if (!IS_ERR(arb->gip_base[1]))
		irq_f_p = gip_readl(arb->gip_base[1], arb, GIP_IRQCTRL_DEBUG_IRQ_FLAG_1);

	if ((irq_f == 0) && (irq_f_p == 0)) {
		mutex_unlock(&arb->pmrc_gip_m_mutex);
		__pm_relax(arb->pmrc_gip_m_Thread_lock);
		return IRQ_NONE;
	}

	for (idx = 0; idx < 32; idx++) {
		if (((irq_f & (0x1 << idx)) != 0) || ((irq_f_p & (0x1 << idx)) != 0)) {
			if (check_irq_in_range(idx, arb->req_vio_irq_idx[0], arb->req_vio_irq_idx[1]))
				gip_req_vio_irq_handler(irq_f, irq_f_p, data, idx);
			if (check_irq_in_range(idx, arb->pmic_vio_irq_idx[0], arb->pmic_vio_irq_idx[1]))
				gip_pmic_vio_irq_handler(irq_f, irq_f_p, data, idx);

			pr_notice("%s IRQ[%d] triggered\n",
				__func__, idx);

			if (irq_f)
				gip_writel(arb->gip_sec_base[0], arb, irq_f, GIP_WRD_IRQ_CLR_1);
			else if (irq_f_p)
				gip_writel(arb->gip_sec_base[1], arb, irq_f_p, GIP_WRD_IRQ_CLR_1);
			else
				pr_notice("%s IRQ[%d] is not cleared due to empty flags\n",
					__func__, idx);
			break;
		}
	}
	mutex_unlock(&arb->pmrc_gip_m_mutex);
	__pm_relax(arb->pmrc_gip_m_Thread_lock);

	return IRQ_HANDLED;
}

static irqreturn_t pmrc_gip_event_2_irq_handler(int irq, void *data)
{
	struct pmrc_gip *arb = data;
	int irq_f = 0, irq_f_p = 0, idx = 0;

	__pm_stay_awake(arb->pmrc_gip_m_Thread_lock);
	mutex_lock(&arb->pmrc_gip_m_mutex);

	irq_f = gip_readl(arb->gip_base[0], arb, GIP_IRQCTRL_DEBUG_IRQ_FLAG_2);
	if (!IS_ERR(arb->gip_base[1]))
		irq_f_p = gip_readl(arb->gip_base[1], arb, GIP_IRQCTRL_DEBUG_IRQ_FLAG_2);

	if ((irq_f == 0) && (irq_f_p == 0)) {
		mutex_unlock(&arb->pmrc_gip_m_mutex);
		__pm_relax(arb->pmrc_gip_m_Thread_lock);
		return IRQ_NONE;
	}

	for (idx = 0; idx < 32; idx++) {
		if (((irq_f & (0x1 << idx)) != 0) || ((irq_f_p & (0x1 << idx)) != 0)) {
			if (check_irq_in_range(idx, arb->hwintf_err_idx[0], arb->hwintf_err_idx[1]))
				gip_hwinf_err_irq_handler(irq_f, irq_f_p, data, idx);

			pr_notice("%s IRQ[%d] triggered\n",
				__func__, idx);

			if (irq_f)
				gip_writel(arb->gip_sec_base[0], arb, irq_f, GIP_WRD_IRQ_CLR_2);
			else if (irq_f_p)
				gip_writel(arb->gip_sec_base[1], arb, irq_f_p, GIP_WRD_IRQ_CLR_2);
			else
				pr_notice("%s IRQ[%d] is not cleared due to empty flags\n",
					__func__, idx);
			break;
		}
	}
	mutex_unlock(&arb->pmrc_gip_m_mutex);
	__pm_relax(arb->pmrc_gip_m_Thread_lock);

	return IRQ_HANDLED;
}

static irqreturn_t pmrc_gip_event_3_irq_handler(int irq, void *data)
{
	struct pmrc_gip *arb = data;
	int irq_f = 0, irq_f_p = 0, idx = 0;

	__pm_stay_awake(arb->pmrc_gip_m_Thread_lock);
	mutex_lock(&arb->pmrc_gip_m_mutex);

	irq_f = gip_readl(arb->gip_base[0], arb, GIP_IRQCTRL_DEBUG_IRQ_FLAG_3);
	if (!IS_ERR(arb->gip_base[1]))
		irq_f_p = gip_readl(arb->gip_base[1], arb, GIP_IRQCTRL_DEBUG_IRQ_FLAG_3);

	if ((irq_f == 0) && (irq_f_p == 0)) {
		mutex_unlock(&arb->pmrc_gip_m_mutex);
		__pm_relax(arb->pmrc_gip_m_Thread_lock);
		return IRQ_NONE;
	}

	for (idx = 0; idx < 32; idx++) {
		if (((irq_f & (0x1 << idx)) != 0) || ((irq_f_p & (0x1 << idx)) != 0)) {
			if (check_irq_in_range(idx, arb->hwintf_err_idx[0], arb->hwintf_err_idx[1]))
				gip_hwinf_err_irq_handler(irq_f, irq_f_p, data, idx);

			pr_notice("%s IRQ[%d] triggered\n",
				__func__, idx);

			if (irq_f)
				gip_writel(arb->gip_sec_base[0], arb, irq_f, GIP_WRD_IRQ_CLR_3);
			else if (irq_f_p)
				gip_writel(arb->gip_sec_base[1], arb, irq_f_p, GIP_WRD_IRQ_CLR_3);
			else
				pr_notice("%s IRQ[%d] is not cleared due to empty flags\n",
					__func__, idx);
			break;
		}
	}
	mutex_unlock(&arb->pmrc_gip_m_mutex);
	__pm_relax(arb->pmrc_gip_m_Thread_lock);

	return IRQ_HANDLED;
}

static irqreturn_t pmrc_gip_event_4_irq_handler(int irq, void *data)
{
	struct pmrc_gip *arb = data;
	int irq_f = 0, irq_f_p = 0, idx = 0;

	__pm_stay_awake(arb->pmrc_gip_m_Thread_lock);
	mutex_lock(&arb->pmrc_gip_m_mutex);

	irq_f = gip_readl(arb->gip_base[0], arb, GIP_IRQCTRL_DEBUG_IRQ_FLAG_4);
	if (!IS_ERR(arb->gip_base[1]))
		irq_f_p = gip_readl(arb->gip_base[1], arb, GIP_IRQCTRL_DEBUG_IRQ_FLAG_4);

	if ((irq_f == 0) && (irq_f_p == 0)) {
		mutex_unlock(&arb->pmrc_gip_m_mutex);
		__pm_relax(arb->pmrc_gip_m_Thread_lock);
		return IRQ_NONE;
	}

	for (idx = 0; idx < 32; idx++) {
		if (((irq_f & (0x1 << idx)) != 0) || ((irq_f_p & (0x1 << idx)) != 0)) {
			if (check_irq_in_range(idx, arb->hwintf_bytecnt_vio_idx[0], arb->hwintf_bytecnt_vio_idx[1]))
				gip_hwinf_bytecnt_vio_irq_handler(irq_f, irq_f_p, data, idx);

			pr_notice("%s IRQ[%d] triggered\n",
				__func__, idx);

			if (irq_f)
				gip_writel(arb->gip_sec_base[0], arb, irq_f, GIP_WRD_IRQ_CLR_4);
			else if (irq_f_p)
				gip_writel(arb->gip_sec_base[1], arb, irq_f_p, GIP_WRD_IRQ_CLR_4);
			else
				pr_notice("%s IRQ[%d] is not cleared due to empty flags\n",
					__func__, idx);
			break;
		}
	}
	mutex_unlock(&arb->pmrc_gip_m_mutex);
	__pm_relax(arb->pmrc_gip_m_Thread_lock);

	return IRQ_HANDLED;
}

static irqreturn_t pmrc_gip_event_5_irq_handler(int irq, void *data)
{
	struct pmrc_gip *arb = data;
	int irq_f = 0, irq_f_p = 0, idx = 0;

	__pm_stay_awake(arb->pmrc_gip_m_Thread_lock);
	mutex_lock(&arb->pmrc_gip_m_mutex);

	irq_f = gip_readl(arb->gip_base[0], arb, GIP_IRQCTRL_DEBUG_IRQ_FLAG_5);
	if (!IS_ERR(arb->gip_base[1]))
		irq_f_p = gip_readl(arb->gip_base[1], arb, GIP_IRQCTRL_DEBUG_IRQ_FLAG_5);

	if ((irq_f == 0) && (irq_f_p == 0)) {
		mutex_unlock(&arb->pmrc_gip_m_mutex);
		__pm_relax(arb->pmrc_gip_m_Thread_lock);
		return IRQ_NONE;
	}

	for (idx = 0; idx < 32; idx++) {
		if (((irq_f & (0x1 << idx)) != 0) || ((irq_f_p & (0x1 << idx)) != 0)) {
			if (check_irq_in_range(idx, arb->hwintf_bytecnt_vio_idx[0], arb->hwintf_bytecnt_vio_idx[1]))
				gip_hwinf_bytecnt_vio_irq_handler(irq_f, irq_f_p, data, idx);

			pr_notice("%s IRQ[%d] triggered\n",
				__func__, idx);

			if (irq_f)
				gip_writel(arb->gip_sec_base[0], arb, irq_f, GIP_WRD_IRQ_CLR_5);
			else if (irq_f_p)
				gip_writel(arb->gip_sec_base[1], arb, irq_f_p, GIP_WRD_IRQ_CLR_5);
			else
				pr_notice("%s IRQ[%d] is not cleared due to empty flags\n",
					__func__, idx);
			break;
		}
	}
	mutex_unlock(&arb->pmrc_gip_m_mutex);
	__pm_relax(arb->pmrc_gip_m_Thread_lock);

	return IRQ_HANDLED;
}

static struct pmrc_gip_irq_desc pmrc_gip_event_irq[] = {
	PMRC_GIP_IRQDESC(event_0),
	PMRC_GIP_IRQDESC(event_1),
	PMRC_GIP_IRQDESC(event_2),
	PMRC_GIP_IRQDESC(event_3),
	PMRC_GIP_IRQDESC(event_4),
	PMRC_GIP_IRQDESC(event_5),
};

static int pmrc_gip_irq_register(struct platform_device *pdev,
		struct pmrc_gip *arb, int irq)
{
	int i = 0, ret = 0;
	u32 gip_irq_event_en[6] = {0};
	u32 gip_irq_event_en_p[6] = {0};

	for (i = 0; i < ARRAY_SIZE(pmrc_gip_event_irq); i++) {
		if (!pmrc_gip_event_irq[i].name)
			continue;
		ret = devm_request_threaded_irq(&pdev->dev, irq, NULL,
				pmrc_gip_event_irq[i].irq_handler,
				IRQF_TRIGGER_HIGH | IRQF_ONESHOT | IRQF_SHARED,
				pmrc_gip_event_irq[i].name, arb);
		if (ret < 0) {
			dev_notice(&pdev->dev, "request %s irq fail\n",
				pmrc_gip_event_irq[i].name);
			continue;
		}
		pmrc_gip_event_irq[i].irq = irq;
	}

	ret = of_property_read_u32_array(pdev->dev.of_node, "gip-irq-event-en",
		gip_irq_event_en, ARRAY_SIZE(gip_irq_event_en));

	ret |= of_property_read_u32_array(pdev->dev.of_node, "gip-irq-event-en-p",
		gip_irq_event_en_p, ARRAY_SIZE(gip_irq_event_en_p));

	gip_writel(arb->gip_sec_base[0], arb, gip_irq_event_en[0] | gip_readl(arb->gip_base[0],
		arb, GIP_IRQ_EVENT_EN_0), GIP_IRQ_EVENT_EN_0);
	gip_writel(arb->gip_sec_base[0], arb, gip_irq_event_en[1] | gip_readl(arb->gip_base[0],
		arb, GIP_IRQ_EVENT_EN_1), GIP_IRQ_EVENT_EN_1);
	gip_writel(arb->gip_sec_base[0], arb, gip_irq_event_en[2] | gip_readl(arb->gip_base[0],
		arb, GIP_IRQ_EVENT_EN_2), GIP_IRQ_EVENT_EN_2);
	gip_writel(arb->gip_sec_base[0], arb, gip_irq_event_en[3] | gip_readl(arb->gip_base[0],
		arb, GIP_IRQ_EVENT_EN_3), GIP_IRQ_EVENT_EN_3);
	gip_writel(arb->gip_sec_base[0], arb, gip_irq_event_en[4] | gip_readl(arb->gip_base[0],
		arb, GIP_IRQ_EVENT_EN_4), GIP_IRQ_EVENT_EN_4);
	gip_writel(arb->gip_sec_base[0], arb, gip_irq_event_en[5] | gip_readl(arb->gip_base[0],
		arb, GIP_IRQ_EVENT_EN_5), GIP_IRQ_EVENT_EN_5);

	if (!IS_ERR(arb->gip_base[1])) {
		gip_writel(arb->gip_sec_base[1], arb, gip_irq_event_en_p[0] | gip_readl(arb->gip_base[1],
			arb, GIP_IRQ_EVENT_EN_0), GIP_IRQ_EVENT_EN_0);
		gip_writel(arb->gip_sec_base[1], arb, gip_irq_event_en_p[1] | gip_readl(arb->gip_base[1],
			arb, GIP_IRQ_EVENT_EN_1), GIP_IRQ_EVENT_EN_1);
		gip_writel(arb->gip_sec_base[1], arb, gip_irq_event_en_p[2] | gip_readl(arb->gip_base[1],
			arb, GIP_IRQ_EVENT_EN_2), GIP_IRQ_EVENT_EN_2);
		gip_writel(arb->gip_sec_base[1], arb, gip_irq_event_en_p[3] | gip_readl(arb->gip_base[1],
			arb, GIP_IRQ_EVENT_EN_3), GIP_IRQ_EVENT_EN_3);
		gip_writel(arb->gip_sec_base[1], arb, gip_irq_event_en_p[4] | gip_readl(arb->gip_base[1],
			arb, GIP_IRQ_EVENT_EN_4), GIP_IRQ_EVENT_EN_4);
		gip_writel(arb->gip_sec_base[1], arb, gip_irq_event_en_p[5] | gip_readl(arb->gip_base[1],
			arb, GIP_IRQ_EVENT_EN_5), GIP_IRQ_EVENT_EN_5);
	}

	return ret;
}

static int mtk_pmrc_gip_probe(struct platform_device *pdev)
{
	struct pmrc_gip *arb = NULL;
	struct resource *res;
	int err = 0;

	arb = devm_kzalloc(&pdev->dev, sizeof(*arb), GFP_KERNEL);
	if (!arb)
		return -ENOMEM;

	arb->data = of_device_get_match_data(&pdev->dev);
	if (!arb->data) {
		dev_info(&pdev->dev, "cannot get drv_data\n");
		return -EINVAL;
	}

	arb->pmrc_gip_m_Thread_lock =
		wakeup_source_register(NULL, "pmrc_gip_m wakelock");
	arb->pmrc_gip_p_Thread_lock =
		wakeup_source_register(NULL, "pmrc_gip_p wakelock");
	mutex_init(&arb->pmrc_gip_m_mutex);
	mutex_init(&arb->pmrc_gip_p_mutex);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "gip");
	arb->gip_base[0] = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(arb->gip_base[0])) {
		dev_notice(&pdev->dev, "[PMRC_GIP]:no gip found\n");
		err = PTR_ERR(arb->gip_base[0]);
		return err;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "gip-p");
	arb->gip_base[1] = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(arb->gip_base[1])) {
		dev_notice(&pdev->dev, "[PMRC_GIP]:no gip-p found\n");
		err = PTR_ERR(arb->gip_base[1]);
		return err;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "gip-sec");
	arb->gip_sec_base[0] = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(arb->gip_sec_base[0])) {
		dev_notice(&pdev->dev, "[PMRC_GIP]:no gip-sec found\n");
		err = PTR_ERR(arb->gip_sec_base[0]);
		return err;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "gip-p-sec");
	arb->gip_sec_base[1] = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(arb->gip_sec_base[1])) {
		dev_notice(&pdev->dev, "[PMRC_GIP]:no gip-p-sec found\n");
		err = PTR_ERR(arb->gip_sec_base[1]);
		return err;
	}

	platform_set_drvdata(pdev, arb);

	arb->pmrc_gip_irq = platform_get_irq_byname(pdev, "pmrc_gip_irq");
	if (arb->pmrc_gip_irq < 0) {
		dev_notice(&pdev->dev,
			"Failed to get pmrc_gip_irq, ret = %d\n", arb->pmrc_gip_irq);
	} else {
		err = pmrc_gip_irq_register(pdev, arb, arb->pmrc_gip_irq);
		if (err)
			dev_notice(&pdev->dev,
				"Failed to register pmrc_gip_irq, ret = %d\n",
						arb->pmrc_gip_irq);
	}

	arb->pmrc_gip_irq_p = platform_get_irq_byname(pdev, "pmrc_gip_p_irq");
	if (arb->pmrc_gip_irq_p < 0) {
		dev_notice(&pdev->dev,
			"Failed to get pmrc_gip_p_irq, ret = %d\n", arb->pmrc_gip_irq_p);
	} else {
		err = pmrc_gip_irq_register(pdev, arb, arb->pmrc_gip_irq_p);
		if (err)
			dev_notice(&pdev->dev,
				"Failed to register pmrc_gip_p_irq, ret = %d\n",
						arb->pmrc_gip_irq_p);
	}

	err = of_property_read_u32_array(pdev->dev.of_node, "hwinf-bytecnt-vio-irq-idx",
		arb->hwintf_bytecnt_vio_idx, ARRAY_SIZE(arb->hwintf_bytecnt_vio_idx));

	if (err)
		dev_info(&pdev->dev, "[PMIF]: No hwinf-bytecnt-vio-irq-idx found\n");

	err = of_property_read_u32_array(pdev->dev.of_node, "hwinf-err-irq-idx",
		arb->hwintf_err_idx, ARRAY_SIZE(arb->hwintf_err_idx));

	if (err)
		dev_info(&pdev->dev, "[PMIF]: No hwinf-err-irq-idx found\n");

	err = of_property_read_u32_array(pdev->dev.of_node, "pmic-vio-irq-idx",
		arb->pmic_vio_irq_idx, ARRAY_SIZE(arb->pmic_vio_irq_idx));

	if (err)
		dev_info(&pdev->dev, "[PMIF]: No pmic-vio-irq-idx found\n");

	err = of_property_read_u32_array(pdev->dev.of_node, "req-vio-irq-idx",
		arb->req_vio_irq_idx, ARRAY_SIZE(arb->req_vio_irq_idx));

	if (err)
		dev_info(&pdev->dev, "[PMIF]: No req-vio-irq-idx found\n");

	return 0;
}

static void mtk_pmrc_gip_remove(struct platform_device *pdev)
{

}

static const struct of_device_id mtk_pmrc_gip_match_table[] = {
	{
		.compatible = "mediatek,mt6993-pmrc-gip",
		.data = &mt6xxx_pmrc_gip_arb,
	}, {
		/* sentinel */
	},
};
MODULE_DEVICE_TABLE(of, mtk_pmrc_gip_match_table);

static struct platform_driver mtk_pmrc_gip_driver = {
	.driver		= {
		.name	= "pmrc-gip-mtk",
		.of_match_table = of_match_ptr(mtk_pmrc_gip_match_table),
	},
	.probe		= mtk_pmrc_gip_probe,
	.remove		= mtk_pmrc_gip_remove,
};
module_platform_driver(mtk_pmrc_gip_driver);

MODULE_AUTHOR("Roy-fp Wu <Roy-fp.wu@mediatek.com>");
MODULE_DESCRIPTION("MediaTek PMRC_GIP Driver");
MODULE_LICENSE("GPL");
