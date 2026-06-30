/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2025 MediaTek Inc.
 */

#ifndef _MTK_VCODEC_UTIL_API_H_
#define _MTK_VCODEC_UTIL_API_H_

#include <linux/types.h>

/* include for enum, struct, define */
/* scheduler.ko */
#include "eas/group.h"
/* vip_engine header*/
#include <linux/sched.h>
/* emi-slb header */
#include "emi.h"


#define mtk_vcodec_api_err(fmt, args...)                \
	pr_notice("[MTK_V4L2_API][ERROR] %s:%d: " fmt "\n", __func__, __LINE__, ##args)


struct mtk_vcodec_sys_apis {
	/* scheduler.ko (use by mtk-vcodec-util.c) */
	void (*__set_top_grp_aware)(int val, int force_ctrl);
	void (*__set_grp_awr_min_opp_margin)(int gear_id, int group_id, int val);
	void (*__set_grp_awr_thr)(int gear_id, int group_id, int opp);
	/* vip_engine.ko*/
	int  (*__set_task_priority)(struct task_struct *task, int prio);
	/* emi-slb.ko */
	int  (*__mtk_slb_violation_register_callback)(mtk_slb_violation_callback_t fn, void *cb_data);
};


/* for assign dependency APIs' function pointers*/
void mtk_vcodec_set_sys_api_funcs(struct mtk_vcodec_sys_apis *fp);

/* scheduler.ko (use by mtk-vcodec-util.c) */
void mtk_vcodec_set_top_grp_aware(int val, int force_ctrl);
void mtk_vcodec_set_grp_awr_min_opp_margin(int gear_id, int group_id, int val);
void mtk_vcodec_set_grp_awr_thr(int gear_id, int group_id, int opp);

/* vip_engine.ko*/
int  mtk_vcodec_set_task_priority(struct task_struct *task, int prio);

/* emi-slb.ko */
int  mtk_vcodec_slb_violation_register_callback(mtk_slb_violation_callback_t fn, void *cb_data);

#endif /* _MTK_VCODEC_UTIL_API_H_ */
