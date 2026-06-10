// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2025 MediaTek Inc.
 */

#include <linux/sched.h>
#include <linux/module.h>

#include "mtk_vcodec_util_api.h"

static struct mtk_vcodec_sys_apis *func_ptr;


void mtk_vcodec_set_sys_api_funcs(struct mtk_vcodec_sys_apis *fp)
{
	func_ptr = fp;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_set_sys_api_funcs);

/* scheduler.ko (use by mtk-vcodec-util.c) */
void mtk_vcodec_set_top_grp_aware(int val, int force_ctrl)
{
	if (!func_ptr || !func_ptr->__set_top_grp_aware) {
#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
		if (!func_ptr)
			mtk_vcodec_api_err("API function pointers not ready");
		else
			mtk_vcodec_api_err("API function not support");
#endif
		return;
	}
	func_ptr->__set_top_grp_aware(val, force_ctrl);
}
EXPORT_SYMBOL_GPL(mtk_vcodec_set_top_grp_aware);

void mtk_vcodec_set_grp_awr_min_opp_margin(int gear_id, int group_id, int val)
{
	if (!func_ptr || !func_ptr->__set_grp_awr_min_opp_margin) {
#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
		if (!func_ptr)
			mtk_vcodec_api_err("API function pointers not ready");
		else
			mtk_vcodec_api_err("API function not support");
#endif
		return;
	}
	func_ptr->__set_grp_awr_min_opp_margin(gear_id, group_id, val);
}
EXPORT_SYMBOL_GPL(mtk_vcodec_set_grp_awr_min_opp_margin);

void mtk_vcodec_set_grp_awr_thr(int gear_id, int group_id, int opp)
{
	if (!func_ptr || !func_ptr->__set_grp_awr_thr) {
#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
		if (!func_ptr)
			mtk_vcodec_api_err("API function pointers not ready");
		else
			mtk_vcodec_api_err("API function not support");
#endif
		return;
	}
	func_ptr->__set_grp_awr_thr(gear_id, group_id, opp);
}
EXPORT_SYMBOL_GPL(mtk_vcodec_set_grp_awr_thr);

/* vip_engine.ko*/
int mtk_vcodec_set_task_priority(struct task_struct *task, int prio)
{
	if (!func_ptr || !func_ptr->__set_task_priority) {
		struct sched_param param = {0};

		if (prio < 0 || prio >= MAX_PRIO) // >= 140
			return -EINVAL;

		if (!task)
			return -ESRCH;

		param.sched_priority = prio;
		if (prio < MAX_RT_PRIO) { // < 100 is RT
			if (sched_setscheduler_nocheck(task, SCHED_FIFO, &param) != 0)
				return -EINVAL;
		} else {
			if (sched_setscheduler_nocheck(task, SCHED_NORMAL, &param) != 0)
				return -EINVAL;
			set_user_nice(task, prio - DEFAULT_PRIO); // -120
		}
		return 0;
	}
	return func_ptr->__set_task_priority(task, prio);
}
EXPORT_SYMBOL_GPL(mtk_vcodec_set_task_priority);

/* emi-slb.ko */
int mtk_vcodec_slb_violation_register_callback(mtk_slb_violation_callback_t fn, void *cb_data)
{
	if (!func_ptr || !func_ptr->__mtk_slb_violation_register_callback) {
#if IS_ENABLED(CONFIG_MTK_EMI) && !IS_ENABLED(CONFIG_MTK_EMI_LEGACY)
		if (!func_ptr)
			mtk_vcodec_api_err("API function pointers not ready");
		else
			mtk_vcodec_api_err("API function not support");
#endif
		return -1;
	}
	return func_ptr->__mtk_slb_violation_register_callback(fn, cb_data);
}
EXPORT_SYMBOL_GPL(mtk_vcodec_slb_violation_register_callback);


MODULE_IMPORT_NS(DMA_BUF);
MODULE_LICENSE("GPL");
