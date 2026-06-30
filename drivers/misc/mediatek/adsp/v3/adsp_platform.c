// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/delay.h>
#include "adsp_reg.h"
#include "adsp_platform_driver.h"
#include "adsp_platform.h"

#ifdef ADSP_BASE
#undef ADSP_BASE
#endif
#ifdef ADSP_BASE_CFG2
#undef ADSP_BASE_CFG2
#endif

#define ADSP_BASE                  mt_base
#define ADSP_BASE_CFG2             mt_base_cfg2

#define SET_BITS(addr, mask) writel(readl(addr) | (mask), addr)
#define CLR_BITS(addr, mask) writel(readl(addr) & ~(mask), addr)
#define READ_BITS(addr, mask) (readl(addr) & (mask))

static void __iomem *mt_base;
static void __iomem *mt_base_cfg2;
static u32 axibus_idle_val;

/* below access adsp register necessary */
void adsp_mt_set_swirq(u32 cid)
{
	if (cid == ADSP_A_ID)
		writel(ADSP_A_SW_INT, ADSP_SW_INT_SET);
	else
		writel(ADSP_B_SW_INT, ADSP_SW_INT_SET);
}

u32 adsp_mt_check_swirq(u32 cid)
{
	if (cid == ADSP_A_ID)
		return readl(ADSP_SW_INT_SET) & ADSP_A_SW_INT;
	else
		return readl(ADSP_SW_INT_SET) & ADSP_B_SW_INT;
}

void adsp_mt_clr_sysirq(u32 cid)
{
	if (cid == ADSP_A_ID)
		writel(ADSP_A_2HOST_IRQ_BIT, ADSP_GENERAL_IRQ_CLR);
	else
		writel(ADSP_B_2HOST_IRQ_BIT, ADSP_GENERAL_IRQ_CLR);
}

void adsp_mt_clr_auidoirq(u32 cid)
{
	/* just clear correct bits*/
	if (cid == ADSP_A_ID)
		writel(ADSP_A_AFE2HOST_IRQ_BIT, ADSP_GENERAL_IRQ_CLR);
	else
		writel(ADSP_B_AFE2HOST_IRQ_BIT, ADSP_GENERAL_IRQ_CLR);
}

void adsp_mt_disable_wdt(u32 cid)
{
	if (cid == ADSP_A_ID)
		CLR_BITS(ADSP_A_WDT_REG, WDT_EN_BIT);
	else
		CLR_BITS(ADSP_B_WDT_REG, WDT_EN_BIT);
}

void adsp_mt_clr_spm(u32 cid)
{
	if (cid == ADSP_A_ID)
		CLR_BITS(ADSP_A_SPM_WAKEUPSRC, ADSP_WAKEUP_SPM);
	else
		CLR_BITS(ADSP_B_SPM_WAKEUPSRC, ADSP_WAKEUP_SPM);
}

bool adsp_mt_check_hifi_status(u32 mask)
{
	return !!(readl(ADSP_SLEEP_STATUS_REG) & mask);
}

u32 adsp_mt_read_adsp_sys_status(u32 cid)
{
	if (cid == ADSP_A_ID)
		return readl(ADSP_CFGREG_RSV_RW_REG0);
	else
		return readl(ADSP_CFGREG_RSV_RW_REG1);
}

bool adsp_mt_is_adsp_axibus_idle(u32 *backup)
{
	u32 value = readl(ADSP_DBG_PEND_CNT);

	if (backup)
		*backup = value;

	return (value == axibus_idle_val);
}

void adsp_mt_toggle_semaphore(u32 bit)
{
	writel((1 << bit), ADSP_SEMAPHORE);
}

u32 adsp_mt_get_semaphore(u32 bit)
{
	return (readl(ADSP_SEMAPHORE) >> bit) & 0x1;
}

ssize_t adsp_mt_get_dpsw_status(char *buf, u32 buf_size)
{
	int n = 0;
	u32 req, ack, vcore_ack, vlp_ack;

	if (!buf)
		return n;

	req = readl(ADSP_DPSW_REQ) & ADSP_DPSW_REQ_MASK;
	ack = readl(ADSP_DPSW_ACK) & ADSP_DPSW_ACK_MASK;
	vcore_ack = readl(DPSW_AD_VLOGIC_ON_ACK_STATUS);
	vlp_ack = readl(DPSW_AD_SRAM_ON_ACK_STATUS);

	n += scnprintf(buf + n, buf_size - n,
			"dpsw req = 0x%x, ack = 0x%x\n",
			req, ack);
	n += scnprintf(buf + n, buf_size - n,
			"vcore_ack = 0x%x, vlp_ack = 0x%x\n",
			vcore_ack, vlp_ack);
	return n;
}

void adsp_hardware_init(struct adspsys_priv *adspsys)
{
	struct adsp_hardware_operations *hw_ops;

	if (unlikely(!adspsys))
		return;

	mt_base = adspsys->cfg;
	mt_base_cfg2 = adspsys->cfg2;
	axibus_idle_val = adspsys->desc->axibus_idle_val;

	/* platform operation initialization */
	hw_ops = &adspsys->hw_ops;
	hw_ops->set_swirq = adsp_mt_set_swirq;
	hw_ops->check_swirq = adsp_mt_check_swirq;
	hw_ops->clr_spm = adsp_mt_clr_spm;
	hw_ops->toggle_semaphore = adsp_mt_toggle_semaphore;
	hw_ops->get_semaphore = adsp_mt_get_semaphore;
	hw_ops->check_hifi_status = adsp_mt_check_hifi_status;
	hw_ops->read_adsp_sys_status = adsp_mt_read_adsp_sys_status;
	hw_ops->is_adsp_axibus_idle = adsp_mt_is_adsp_axibus_idle;
	hw_ops->get_dpsw_status = adsp_mt_get_dpsw_status;
}
