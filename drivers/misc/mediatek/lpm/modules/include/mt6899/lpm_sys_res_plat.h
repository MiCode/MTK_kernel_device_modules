/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __PLAT_SYS_RES_SIGNAL_H__
#define __PLAT_SYS_RES_SIGNAL_H__

#include <linux/types.h>

enum lpm_sys_res_record_plat_op_id {
	SYS_RES_DURATION = 0,
	SYS_RES_SUSPEND_TIME,
	SYS_RES_SIG_TIME,
	SYS_RES_SIG_ID,
	SYS_RES_SIG_GROUP_ID,
	SYS_RES_SIG_OVERALL_RATIO,
	SYS_RES_SIG_SUSPEND_RATIO,
	SYS_RES_SIG_ADDR,
};

enum lpm_sys_res_group_id {
	SYS_RES_DDREN_REQ,
	SYS_RES_APSRC_REQ,
	SYS_RES_EMI_REQ,
	SYS_RES_MAINPLL_REQ,
	SYS_RES_INFRA_REQ,
	SYS_RES_F26M_REQ,
	SYS_RES_PMIC_REQ,
	SYS_RES_VCORE_REQ,
	SYS_RES_RC_REQ,
	SYS_RES_PLL_EN,
	SYS_RES_PWR_OFF,
	SYS_RES_PWR_ACT,
	SYS_RES_SYS_STA,

	SYS_RES_GRP_NUM,
};

extern struct sys_res_group_info *group_info;

int lpm_sys_res_plat_init(void);
void lpm_sys_res_plat_deinit(void);

#endif


