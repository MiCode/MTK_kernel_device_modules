/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_LPM_SYS_RES__
#define __MTK_LPM_SYS_RES__


#include <linux/types.h>
#include <linux/spinlock.h>
#include <swpm_module_psp.h>

#define SYS_RES_SYS_RESOURCE_NUM (8)
#define NON_RES_SIG_GROUP (0xFFFFFFFF)

enum _sys_main_res {
	SYS_MAIN_RES_DDREN,
	SYS_MAIN_RES_APSRC,
	SYS_MAIN_RES_EMI,
	SYS_MAIN_RES_MAINPLL,
	SYS_MAIN_RES_INFRA,
	SYS_MAIN_RES_26M,
	SYS_MAIN_RES_PMIC,
	SYS_MAIN_RES_VCORE,
	SYS_MAIN_RES_RC_REQ,
	SYS_MAIN_RES_PLL_EN,
	SYS_MAIN_RES_PWR_OFF,
	SYS_MAIN_RES_PWR_ACT,
	SYS_MAIN_RES_SYS_STA,
	SYS_MAIN_RES_NUM,
};

enum _sys_res_scene{
	SYS_RES_SCENE_COMMON = 0,
	SYS_RES_SCENE_SUSPEND,
	SYS_RES_SCENE_LAST_SUSPEND_DIFF,
	SYS_RES_SCENE_LAST_DIFF,
	SYS_RES_SCENE_LAST_SYNC,
	SYS_RES_SCENE_TEMP,
	SYS_RES_SCENE_NUM,
};

enum _sys_res_get_scene{
	SYS_RES_COMMON = 0,
	SYS_RES_SUSPEND,
	SYS_RES_LAST_SUSPEND,
	SYS_RES_LAST,
	SYS_RES_GET_SCENE_NUM,
};

enum _sys_res_op_id {
	SYS_RES_OP_DURATION = 0,
	SYS_RES_OP_SUSPEND_TIME,
	SYS_RES_OP_SIG_TIME,
	SYS_RES_OP_SIG_ID,
	SYS_RES_OP_SIG_GROUP_ID,
	SYS_RES_OP_SIG_OVERALL_RATIO,
	SYS_RES_OP_SIG_SUSPEND_RATIO,
	SYS_RES_OP_SIG_ADDR,
};



struct sys_res_record {
	struct res_sig_stats *spm_res_sig_stats_ptr;
};

#define SYS_RES_SYS_NAME_LEN (10)
struct sys_res_group_info {
	unsigned int sys_index;
	unsigned int sig_table_index;
	unsigned int group_num;
	unsigned int threshold;
};

#define SYS_RES_NAME_LEN (10)
struct sys_res_mapping {
	unsigned int id;
	char name[SYS_RES_NAME_LEN];
};

struct lpm_sys_res_ops {
	struct sys_res_record* (*get)(unsigned int scene);
	int (*update)(void);
	uint64_t (*get_detail)(struct sys_res_record *record, int op, unsigned int val);
	unsigned int (*get_threshold)(void);
	void (*set_threshold)(unsigned int val);
	void (*enable_common_log)(int en);
	int (*get_log_enable)(void);
	void (*log)(unsigned int scene);
	spinlock_t *lock;
	int (*get_id_name)(struct sys_res_mapping **map, unsigned int *size);
};

int lpm_sys_res_init(void);
void lpm_sys_res_exit(void);

int register_lpm_sys_res_ops(struct lpm_sys_res_ops *ops);
void unregister_lpm_sys_res_ops(void);
struct lpm_sys_res_ops *get_lpm_sys_res_ops(void);
int lpm_sys_res_stat_init(void);
void lpm_sys_res_stat_deinit(void);

#endif
