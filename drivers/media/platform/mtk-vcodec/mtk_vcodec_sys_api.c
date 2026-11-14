// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2025 MediaTek Inc.
 */

#include <linux/module.h>

#include "mtk_vcodec_sys_api.h"

/* emi-slb.ko */
#if IS_ENABLED(CONFIG_MTK_EMI) && !IS_ENABLED(CONFIG_MTK_EMI_LEGACY)
#include "emi.h"
#endif


static struct mtk_vcodec_sys_apis funcs = {
#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
	/* scheduler.ko (use by mtk-vcodec-util.c) */
	.__set_top_grp_aware		= set_top_grp_aware,
	.__set_grp_awr_min_opp_margin	= set_grp_awr_min_opp_margin,
	.__set_grp_awr_thr		= set_grp_awr_thr,
#endif
#if IS_ENABLED(CONFIG_MTK_VIP_ENGINE)
	/* vip_engine.ko*/
	.__set_task_priority		= set_task_priority,
#endif
#if IS_ENABLED(CONFIG_MTK_EMI) && !IS_ENABLED(CONFIG_MTK_EMI_LEGACY)
	/* emi-slb.ko */
	.__mtk_slb_violation_register_callback = mtk_slb_violation_register_callback,
#endif
};

static int __init mtk_vcodec_sys_api_init(void)
{
	mtk_vcodec_set_sys_api_funcs(&funcs);
	mtk_vcodec_api_log("sys APIs init done");
	return 0;
}

static void __exit mtk_vcodec_sys_api_exit(void)
{
	mtk_vcodec_set_sys_api_funcs(NULL);
	mtk_vcodec_api_log("sys APIs remove done");
}

module_init(mtk_vcodec_sys_api_init);
module_exit(mtk_vcodec_sys_api_exit);

MODULE_IMPORT_NS(DMA_BUF);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mediatek video codec dependency utility");
