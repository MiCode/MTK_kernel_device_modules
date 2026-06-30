/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Mediatek Watchdog Driver
 *
 * Copyright (C) 2025 MediaTek Inc
 *
 * Xiwen Shao <xiwen.shao@mediatek.com>
 */

#ifndef __MTK_WDT_HYPER_H__
#define __MTK_WDT_HYPER_H__

#define WDT_NONRST_REG		0x20
#define WDT_STATUS		0x0C
#define WDT_STATUS_SWWDT_RST	(1 << 30)
#define WDT_MAX_TIMEOUT		30
#define WDT_MIN_TIMEOUT		2

struct mtk_wdt_dev {
	struct watchdog_device wdt_dev;
	void __iomem *wdt_base;
};

extern int mtk_wdt_hyper_kick(void);
extern void mtk_wdt_hyper_suspend(void);
extern void mtk_wdt_hyper_resume(void);

#endif /* __MTK_WDT_HYPER_H__ */
