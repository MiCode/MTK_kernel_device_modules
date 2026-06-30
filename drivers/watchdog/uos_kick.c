// SPDX-License-Identifier: GPL-2.0+
/*
 * Mediatek Watchdog Driver
 *
 * Copyright (C) 2025 MediaTek Inc
 *
 * Xiwen Shao <xiwen.shao@mediatek.com>
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>

static RAW_NOTIFIER_HEAD(vm_hangdet_kick_chain);

static int vm_hangdet_kick_event(void)
{
	return raw_notifier_call_chain(&vm_hangdet_kick_chain, 0, NULL);
}

int vm_hangdet_kick_notifier_register(struct notifier_block *nb)
{
	return raw_notifier_chain_register(&vm_hangdet_kick_chain, nb);
}
EXPORT_SYMBOL(vm_hangdet_kick_notifier_register);

int vm_hangdet_kick_notifier_unregister(struct notifier_block *nb)
{
	return raw_notifier_chain_unregister(&vm_hangdet_kick_chain, nb);
}
EXPORT_SYMBOL(vm_hangdet_kick_notifier_unregister);

int mtk_wdt_hyper_kick(void)
{
	int ret;

	ret = vm_hangdet_kick_event();
	pr_info("vm_wdt_kick_event return %d", ret);

	if (ret)
		return 0;

	return -1;
}
EXPORT_SYMBOL(mtk_wdt_hyper_kick);

void mtk_wdt_hyper_suspend(void)
{
	pr_info("%s:%d:suspend.\n", __func__, __LINE__);
}
EXPORT_SYMBOL(mtk_wdt_hyper_suspend);

void mtk_wdt_hyper_resume(void)
{
	pr_info("%s:%d:resume.\n", __func__, __LINE__);
}
EXPORT_SYMBOL(mtk_wdt_hyper_resume);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xiwen Shao <xiwen.shao@mediatek.com>");
MODULE_DESCRIPTION("Mediatek Virtual WatchDog Timer Driver");
