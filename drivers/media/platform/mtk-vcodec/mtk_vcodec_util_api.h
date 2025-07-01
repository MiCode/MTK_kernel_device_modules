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
/* cpufreq_sugov_ext.ko (use by vcodec_dvfs.c) */
#include "sched/sched.h"
#include "common.h"
#include "util/cpu_util.h"
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
	/* cpufreq_sugov_ext.ko (use by vcodec_dvfs.c) */
	int  (*__get_target_margin_low)(int cpu);
	int  (*__set_turn_point_freq)(int cpu, unsigned long freq);
	int  (*__set_target_margin)(int cpu, int margin);
	int  (*__set_target_margin_low)(int cpu, int margin);
	int  (*__unset_target_margin)(int cpu);
	int  (*__unset_target_margin_low)(int cpu);
	bool (*__is_runnable_boost_enable)(void);
	void (*__set_runnable_boost_enable)(int ctrl);
	void (*__unset_runnable_boost_enable)(void);
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

/* cpufreq_sugov_ext.ko (use by vcodec_dvfs.c) */
int mtk_vcodec_get_target_margin_low(int cpu);
int mtk_vcodec_set_turn_point_freq(int cpu, unsigned long freq);
int mtk_vcodec_set_target_margin(int cpu, int margin);
int mtk_vcodec_set_target_margin_low(int cpu, int margin);
int mtk_vcodec_unset_target_margin(int cpu);
int mtk_vcodec_unset_target_margin_low(int cpu);
bool mtk_vcodec_is_runnable_boost_enable(void);
void mtk_vcodec_set_runnable_boost_enable(int ctrl);
void mtk_vcodec_unset_runnable_boost_enable(void);

/* vip_engine.ko*/
int  mtk_vcodec_set_task_priority(struct task_struct *task, int prio);

/* emi-slb.ko */
int  mtk_vcodec_slb_violation_register_callback(mtk_slb_violation_callback_t fn, void *cb_data);

#endif /* _MTK_VCODEC_UTIL_API_H_ */
