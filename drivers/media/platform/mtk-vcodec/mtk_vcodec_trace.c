// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2025 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/trace_events.h>

#include "mtk_vcodec_trace.h"

static void mtk_vcodec_trace_puts(char *buf)
{
	trace_puts(buf);
}

static int __init mtk_vcodec_trace_init(void)
{
	pr_notice("[MTK_V4L2] %s(),%d: trace init\n", __func__, __LINE__);
	mtk_vcodec_register_trace(mtk_vcodec_trace_puts);
	return 0;
}

static void __exit mtk_vcodec_trace_exit(void)
{
	pr_notice("[MTK_V4L2] %s(),%d: trace remove\n", __func__, __LINE__);
	mtk_vcodec_register_trace(NULL);
}

module_init(mtk_vcodec_trace_init);
module_exit(mtk_vcodec_trace_exit);

MODULE_IMPORT_NS(DMA_BUF);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mediatek video codec debug trace utility");
