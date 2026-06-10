// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Kenny Liu <kenny.liu@mediatek.com>
 */

#include <linux/io.h>
#include <linux/module.h>
#include "mtk-mminfra-util.h"

#if IS_ENABLED(CONFIG_MTK_MMINFRA)
bool mtk_mminfra_is_on(u32 mm_pwr)
{
	return true;
}
EXPORT_SYMBOL_GPL(mtk_mminfra_is_on);

void mtk_mminfra_power_debug(u32 mm_pwr)
{

}
EXPORT_SYMBOL_GPL(mtk_mminfra_power_debug);

void mtk_mminfra_all_power_debug(void)
{

}
EXPORT_SYMBOL_GPL(mtk_mminfra_all_power_debug);

int mtk_mminfra_on_off(bool on_off, u32 mm_pwr, u32 mm_type)
{
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_mminfra_on_off);
#endif

static int __init mtk_mminfra_util_init(void)
{
	pr_notice("%s: dummy\n",__func__);

	return 0;
}

module_init(mtk_mminfra_util_init);
MODULE_DESCRIPTION("MTK MMInfra util driver");
MODULE_AUTHOR("Kenny Liu<kenny.liu@mediatek.com>");
MODULE_LICENSE("GPL");
