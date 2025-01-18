// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

/**
 * @file    ghpm.c
 * @brief   GHPM API implementation
 */

/**
 * ===============================================
 * SECTION : Include files
 * ===============================================
 */

#include "gpueb_debug.h"
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/sched/clock.h>

#include <linux/soc/mediatek/mtk_tinysys_ipi.h>

#include "gpueb_debug.h"
#include "gpueb_helper.h"
#include "gpueb_ipi.h"
#include "ghpm.h"


static bool first_on_after_bootup;
static int g_ipi_channel;
static int g_gpueb_slot_size;
static int g_power_count;  /* power count for ghpm control mfg1 on/off */
static void __iomem *g_gpueb_gpr_base;
static void __iomem *g_mfg_rpc_base;
static struct gpueb_slp_ipi_data msgbuf;
static unsigned long g_pwr_irq_flags;
static raw_spinlock_t ghpm_lock;

static void ghpm_abort(void);

int ghpm_init(struct platform_device *pdev)
{
	struct device *gpueb_dev = &pdev->dev;
	struct resource *res = NULL;
	int ret;

	if (unlikely(!gpueb_dev)) {
		gpueb_pr_err(GHPM_TAG, "fail to find gpueb device");
		return GHPM_ERR;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_rpc");
	if (unlikely(!res)) {
		gpueb_pr_err(GHPM_TAG, "fail to get resource MFG_RPC");
		return GHPM_ERR;
	}
	g_mfg_rpc_base = devm_ioremap(gpueb_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_rpc_base)) {
		gpueb_pr_err(GHPM_TAG, "fail to ioremap MFG_RPC: 0x%llx",
			(unsigned long long) res->start);
		return GHPM_ERR;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "gpueb_gpr_base");
	if (unlikely(!res)) {
		gpueb_pr_err(GPUEB_TAG, "fail to get resource GPUEB_GPR_BASE");
		return GHPM_ERR;
	}
	g_gpueb_gpr_base = devm_ioremap(gpueb_dev, res->start, resource_size(res));
	if (unlikely(!g_gpueb_gpr_base)) {
		gpueb_pr_err(GPUEB_TAG, "fail to ioremap gpr base: 0x%llx",
			(unsigned long long) res->start);
		return GHPM_ERR;
	}

	g_ipi_channel = gpueb_get_send_PIN_ID_by_name("IPI_ID_SLEEP");
	if (unlikely(g_ipi_channel < 0)) {
		gpueb_pr_err(GHPM_TAG, "fail to get IPI_ID_SLEEP id");
		return GHPM_ERR;
	}

	ret = mtk_ipi_register(get_gpueb_ipidev(), g_ipi_channel, NULL, NULL, &msgbuf);
	if (ret != IPI_ACTION_DONE) {
		gpueb_pr_err(GHPM_TAG, "ipi register fail: id=%d, ret=%d", g_ipi_channel, ret);
		return GHPM_ERR;
	}

	g_gpueb_slot_size = get_gpueb_slot_size();

	g_power_count = 0;

	first_on_after_bootup = false;

	raw_spin_lock_init(&ghpm_lock);

	return GHPM_SUCCESS;
}

enum mfg0_pwr_sta mfg0_pwr_sta(void)
{
	return ((readl(MFG_RPC_MFG0_PWR_CON) & MFG0_PWR_ACK_BIT) == MFG0_PWR_ACK_BIT)?
		MFGO_PWR_ON : MFG0_PWR_OFF;
}
EXPORT_SYMBOL(mfg0_pwr_sta);

int ghpm_ctrl(enum ghpm_state power, enum mfg0_off_state off_state)
{
	int ret = GHPM_ERR;
	struct gpueb_slp_ipi_data data;

	raw_spin_lock_irqsave(&ghpm_lock, g_pwr_irq_flags);
	gpueb_pr_debug(GHPM_TAG, "Entry");

	if (power == GHPM_ON)
		++g_power_count;
	else
		--g_power_count;

	if (power == GHPM_ON && g_power_count == 1) {
		if (mfg0_pwr_sta() == MFGO_PWR_ON) {
			if (first_on_after_bootup == false) {
				/*
				 * MFG0 shutdown on from TFA after bootup, ghpm don't need to
				 * power on mfg0 again when ghpm on first time coming
				 */
				first_on_after_bootup = true;
			} else {
				gpueb_pr_err(GHPM_TAG, "MFG0 already on but receive GHPM on");
				ret = GHPM_DUPLICATE_ON_ERR;
				goto done_unlock;
			}
		} else {
			/* trigger ghpm on -> reset gpueb -> warm boot -> gpueb resume */
			gpueb_pr_debug(GHPM_TAG, "ghpm on");

			/* Check GHPM IDLE state MFG_GHPM_RO0_CON [7:0] = 8'b0*/
			if ((readl(MFG_GHPM_RO0_CON) & GHPM_STATE) != 0x0) {
				gpueb_pr_err(GHPM_TAG, "GHPM ON, check ghpm_state=idle failed");
				ret = GHPM_STATE_ERR;
				goto done_unlock;
			}

			/* Check GHPM_PWR_STATE MFG_GHPM_RO0_CON [16] = 1'b0*/
			if ((readl(MFG_GHPM_RO0_CON) & GHPM_PWR_STATE) == GHPM_PWR_STATE) {
				gpueb_pr_err(GHPM_TAG, "MFG0 shutdown off not by GHPM last time");
				ret = GHPM_PWR_STATE_ERR;
				goto done_unlock;
			}

			/* Trigger GHPM on sequence */
			writel(readl(MFG_GHPM_CFG0_CON) & ~ON_SEQ_TRI, MFG_GHPM_CFG0_CON);
			writel(readl(MFG_GHPM_CFG0_CON) | ON_SEQ_TRI, MFG_GHPM_CFG0_CON);
		}

		ret = GHPM_SUCCESS;
	} else if (power == GHPM_OFF && g_power_count == 0) {
		gpueb_pr_debug(GHPM_TAG, "ghpm off");

		/* IPI to gpueb for suspend flow and then trigger ghpm off */
		data.event = SUSPEND_POWER_OFF;
		data.off_state = off_state;
		data.reserve = 0; /* dummy (reserve input) */
		ret = mtk_ipi_send(
			get_gpueb_ipidev(),
			g_ipi_channel,
			IPI_SEND_POLLING,
			(void *)&data,
			(sizeof(data) / g_gpueb_slot_size),
			GHPM_IPI_TIMEOUT);

		if (unlikely(ret != IPI_ACTION_DONE)) {
			gpueb_pr_err(GHPM_TAG, "[ABORT] fail to send gpueb off IPI, ret=%d", ret);
			ret = GHPM_OFF_EBB_IPI_ERR;
			goto done_unlock;
		}

		ret = GHPM_SUCCESS;
	} else {
		gpueb_pr_debug(GHPM_TAG, "power=%d, g_power_count=%d", power, g_power_count);
		ret = GHPM_PWRCNT_ERR;
	}

done_unlock:
	gpueb_pr_debug(GHPM_TAG, "EXIT");
	raw_spin_unlock_irqrestore(&ghpm_lock, g_pwr_irq_flags);

	if (ret != GHPM_SUCCESS)
		ghpm_abort();

	return ret;
}
EXPORT_SYMBOL(ghpm_ctrl);

int wait_gpueb(enum gpueb_low_power_event event, int timeout_us)
{
	unsigned int i;

	gpueb_pr_debug(GHPM_TAG, "Entry");

	i = 0;
	if (event == SUSPEND_POWER_ON) {
		while ((readl(GPUEB_SRAM_GPR10) != GPUEB_ON_RESUME)) {
			udelay(1);
			if (++i > GPUEB_WAIT_TIMEOUT)
				goto timeout;
		}
		gpueb_pr_debug(GHPM_TAG, "GPUEB resume done, i=%u", i);
	} else if (event == SUSPEND_POWER_OFF) {
		while (((readl(MFG_GHPM_RO0_CON) & GHPM_STATE) != 0x0) ||
			((readl(MFG_GHPM_RO0_CON) & GHPM_PWR_STATE) == GHPM_PWR_STATE) ||
			(mfg0_pwr_sta() == MFGO_PWR_ON)) {
			udelay(1);
			if (++i > GPUEB_WAIT_TIMEOUT)
				goto timeout;
		}
		gpueb_pr_debug(GHPM_TAG, "GPUEB suspend done, i=%u", i);
	} else {
		gpueb_pr_err(GHPM_TAG, "Wrong event=%d", event);
		return WAIT_INPUT_ERROR;
	}

	gpueb_pr_debug(GHPM_TAG, "End");

	return WAIT_DONE;

timeout:
	gpueb_pr_err(GHPM_TAG, "Wait GPUEB timeout, event=%d", event);
	ghpm_abort();
	return WAIT_TIMEOUT;
}
EXPORT_SYMBOL(wait_gpueb);

void dump_ghpm_info(void)
{
	gpueb_pr_err(GHPM_TAG, "MFG_GHPM_RO0_CON=0x%x", readl(MFG_GHPM_RO0_CON));
	gpueb_pr_err(GHPM_TAG, "MFG_GHPM_RO1_CON=0x%x", readl(MFG_GHPM_RO1_CON));
	gpueb_pr_err(GHPM_TAG, "MFG_RPC_MFG0_PWR_CON=0x%x", readl(MFG_RPC_MFG0_PWR_CON));
}
EXPORT_SYMBOL(dump_ghpm_info);

static void ghpm_abort(void)
{
	dump_ghpm_info();
	gpueb_dump_status(NULL, NULL, 0);
	WARN_ON(1);
}
