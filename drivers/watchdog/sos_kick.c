// SPDX-License-Identifier: GPL-2.0+
/*
 * Mediatek Watchdog Driver
 *
 * Copyright (C) 2025 MediaTek Inc
 *
 * Xiwen Shao <xiwen.shao@mediatek.com>
 */

#include <linux/arm-smccc.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/hypervisor/hvcall.h>

#define HC_GUEST_WDTK_KICK		0xB0000008
#define HC_GUEST_WDTK_SUSPEND		0xB000000A
#define HC_GUEST_WDTK_RESUME		0xB000000B

static inline long acrn_hypercall0(unsigned long hcall_id)
{
	struct arm_smccc_res res;
	unsigned long r7 = 1 << 16;

	arm_smccc_smc(SMC_SC_NBL_VHM_REQ, hcall_id, 0, 0, 0, 0, 0, r7, &res);
	return res.a0;
}

int mtk_wdt_hyper_kick(void)
{
	int ret;

	ret = acrn_hypercall0(HC_GUEST_WDTK_KICK);
	pr_info("%s: arcn_hypercall0 return %d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(mtk_wdt_hyper_kick);

void mtk_wdt_hyper_suspend(void)
{
	acrn_hypercall0(HC_GUEST_WDTK_SUSPEND);
	pr_info("%s: arcn_hypercall0.\n", __func__);
}
EXPORT_SYMBOL(mtk_wdt_hyper_suspend);

void mtk_wdt_hyper_resume(void)
{
	acrn_hypercall0(HC_GUEST_WDTK_RESUME);
	pr_info("%s: arcn_hypercall0.\n", __func__);
}
EXPORT_SYMBOL(mtk_wdt_hyper_resume);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xiwen Shao <xiwen.shao@mediatek.com>");
MODULE_DESCRIPTION("Mediatek Virtual WatchDog Timer Driver");
