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

#define ADSP_BASE                  mt_base
#define INFRA_RSV_BASE             mt_infra_rsv

#define SET_BITS(addr, mask) writel(readl(addr) | (mask), addr)
#define CLR_BITS(addr, mask) writel(readl(addr) & ~(mask), addr)
#define READ_BITS(addr, mask) (readl(addr) & (mask))

static void __iomem *mt_base;
static void __iomem *mt_infra_rsv;
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

bool adsp_mt_check_core_active(u32 cid)
{
	if (cid == ADSP_A_ID)
		return READ_BITS(ADSP_A_CORE_AWAKE_REG, ADSP_A_PRELOCK_MASK) != 0;
	else
		return READ_BITS(ADSP_B_CORE_AWAKE_REG, ADSP_B_PRELOCK_MASK) != 0;
}
int adsp_mt_pre_lock(u32 cid, bool is_lock)
{
	void __iomem *reg;
	u32 mask;

	if (cid == ADSP_A_ID) {
		reg = ADSP_A_PRELOCK_REG;
		mask = ADSP_A_PRELOCK_MASK;
	} else {
		reg = ADSP_B_PRELOCK_REG;
		mask = ADSP_B_PRELOCK_MASK;
	}

	if (is_lock)
		SET_BITS(reg, mask);
	else if (READ_BITS(reg, mask)) /* unlock */
		CLR_BITS(reg, mask);

	return (int)!!READ_BITS(reg, mask);
}

void adsp_hardware_init(struct adspsys_priv *adspsys)
{
	struct adsp_hardware_operations *hw_ops;

	if (unlikely(!adspsys))
		return;

	mt_base = adspsys->cfg;
	mt_infra_rsv = adspsys->infracfg_rsv;
	axibus_idle_val = adspsys->desc->axibus_idle_val;

	if (mt_infra_rsv != NULL) {
		writel(0x0, ADSP_A_PRELOCK_REG);
		writel(0x0, ADSP_B_PRELOCK_REG);
	}

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
	hw_ops->check_core_active = adsp_mt_check_core_active;
	hw_ops->pre_lock = adsp_mt_pre_lock;
}
