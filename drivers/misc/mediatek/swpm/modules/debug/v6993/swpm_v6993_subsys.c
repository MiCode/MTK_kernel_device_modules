// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/perf_event.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/workqueue.h>

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
#include <sspm_reservedmem.h>
#endif

#define CREATE_TRACE_POINTS
#include <swpm_tracker_trace.h>

#include <mtk_swpm_common_sysfs.h>
#include <mtk_swpm_sysfs.h>
#include <swpm_dbg_common_v1.h>
#include <swpm_module.h>
#include <swpm_v6993.h>
#include <swpm_v6993_subsys.h>

/****************************************************************************
 *  Global Variables
 ****************************************************************************/

/****************************************************************************
 *  Local Variables
 ****************************************************************************/
/* TODO: subsys index pointer */
/* static struct share_sub_index_ext *share_sub_idx_ref_ext; */

/****************************************************************************
 *  Static Function
 ****************************************************************************/
void swpm_v6993_sub_ext_update(void)
{
/* TODO: do something from share_sub_idx_ref_ext */
}

void swpm_v6993_sub_ext_init(void)
{
/* TODO: instance init of subsys index */
/*	if (wrap_d) {
 *		share_sub_idx_ref_ext =
 *		(struct share_sub_index_ext *)
 *		sspm_sbuf_get(wrap_d->share_index_sub_ext_addr);
 *	} else {
 *		share_sub_idx_ref_ext = NULL;
 *	}
 */
}

