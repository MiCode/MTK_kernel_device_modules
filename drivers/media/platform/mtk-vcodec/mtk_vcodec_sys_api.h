/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2025 MediaTek Inc.
 */

#ifndef _MTK_VCODEC_SYS_API_H_
#define _MTK_VCODEC_SYS_API_H_

#include <linux/types.h>
#include "mtk_vcodec_util_api.h"


#define mtk_vcodec_api_log(fmt, args...)                \
	pr_notice("[MTK_V4L2_API] %s:%d: " fmt "\n", __func__, __LINE__, ##args)


/* scheduler.ko (use by mtk-vcodec-util.c) */
#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
extern void set_top_grp_aware(int val, int force_ctrl);
extern void set_grp_awr_min_opp_margin(int gear_id, int group_id, int val);
extern void set_grp_awr_thr(int gear_id, int group_id, int opp);
#endif

/* vip_engine.ko*/
#if IS_ENABLED(CONFIG_MTK_VIP_ENGINE)
extern int set_task_priority(struct task_struct *task, int prio);
#endif

#endif /* _MTK_VCODEC_SYS_API_H_ */
