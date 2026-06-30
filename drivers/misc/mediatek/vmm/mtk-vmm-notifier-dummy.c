// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: Eric Chien <eric.chien@mediatek.com>
 */


#include <linux/module.h>
#include <linux/init.h>
#include <linux/of_platform.h>
#include <mt-plat/mtk-vmm-notifier.h>

int vmm_isp_ctrl_notify(int openIsp)
{
	return 0;
}
EXPORT_SYMBOL_GPL(vmm_isp_ctrl_notify);

int vmm_enable_cvfs(enum VMM_CVFS_USR_ID user_id, enum VMM_CVFS_SEL_ID vmm_cvfs_sel_id)
{
	return 0;
}
EXPORT_SYMBOL_GPL(vmm_enable_cvfs);

int vmm_disable_cvfs(enum VMM_CVFS_USR_ID user_id, enum VMM_CVFS_SEL_ID vmm_cvfs_sel_id)
{
	return 0;
}
EXPORT_SYMBOL_GPL(vmm_disable_cvfs);

int vmm_cvfs_dump(void)
{
	return 0;
}
EXPORT_SYMBOL_GPL(vmm_cvfs_dump);

int mtk_vmm_ctrl_dbg_use(bool enable)
{
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_vmm_ctrl_dbg_use);

static int __init mtk_vmm_notifier_dummy_init(void)
{
	pr_info("mtk_vmm_notifier_dummy loaded\n");
	return 0;
}

static void __exit mtk_vmm_notifier_dummy_exit(void)
{
	pr_info("mtk_vmm_notifier_dummy unloaded\n");
}

module_init(mtk_vmm_notifier_dummy_init);
module_exit(mtk_vmm_notifier_dummy_exit);
MODULE_DESCRIPTION("MTK VMM notifier driver");
MODULE_AUTHOR("Eric Chien <eric.chien@mediatek.com>");
MODULE_LICENSE("GPL");
