// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>

#include "apu_dbg.h"
#include "apu_power_table.h"

#define APUSYS_VPU_NUM	(3)
#define APUSYS_MDLA_NUM	(2)

/* opp, mW */
struct apu_opp_info vpu_power_table[APU_OPP_NUM] = {
	{APU_OPP_0, 212},
	{APU_OPP_1, 176},
	{APU_OPP_2, 133},
	{APU_OPP_3, 98},
	{APU_OPP_4, 44},
};
EXPORT_SYMBOL(vpu_power_table);

/* opp, mW */
struct apu_opp_info mdla_power_table[APU_OPP_NUM] = {
	{APU_OPP_0, 161},
	{APU_OPP_1, 161},
	{APU_OPP_2, 120},
	{APU_OPP_3, 86},
	{APU_OPP_4, 44},
};
EXPORT_SYMBOL(mdla_power_table);

static int _thermal_throttle(enum DVFS_USER limit_user, enum APU_OPP_INDEX opp)
{
	enum DVFS_USER user;

	switch (limit_user) {
	case VPU0:
		for (user = VPU0 ; user < VPU0 + APUSYS_VPU_NUM ; user++)
			apupw_thermal_limit(user, opp);
	break;
	case MDLA0:
		for (user = MDLA0 ; user < MDLA0 + APUSYS_MDLA_NUM ; user++)
			apupw_thermal_limit(user, opp);
	break;
	default:
		pr_info("%s invalid limit_user : %d\n", __func__, limit_user);
		return -1;
	}

	return 0;
}

int32_t apusys_thermal_en_throttle_cb(enum DVFS_USER limit_user,
		enum APU_OPP_INDEX opp)
{
	int ret = 0;

	pr_info("%s limit_user : %d, limit_opp : %d\n",
			__func__, limit_user, opp);

	ret = _thermal_throttle(limit_user, opp);

	return ret;
}
EXPORT_SYMBOL(apusys_thermal_en_throttle_cb);

int32_t apusys_thermal_dis_throttle_cb(enum DVFS_USER limit_user)
{
	int ret = 0;

	pr_info("%s limit_user : %d\n", __func__, limit_user);

	ret = _thermal_throttle(limit_user, 0);

	return ret;
}
EXPORT_SYMBOL(apusys_thermal_dis_throttle_cb);
