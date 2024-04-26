// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/module.h>
#include <soc/mediatek/dramc.h>

#define MTK_SIP_VCOREFS_SET_FREQ 16
#define NUM_DRAM_OPP 6

int ddr_level_to_step(int opp)
{
	unsigned int step[] = {0, 1, 3, 5, 7, 9};
	return step[opp];
}

static int __init dvfsrc_opp_init(void)
{
#if IS_ENABLED(CONFIG_MTK_DRAMC)
	int i;
	struct arm_smccc_res ares;

	for (i = 0; i < NUM_DRAM_OPP; i++) {
		arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL,
			MTK_SIP_VCOREFS_SET_FREQ,
			i, mtk_dramc_get_steps_freq(ddr_level_to_step(i)), 0, 0, 0, 0,
			&ares);
	}
#endif
	return 0;
}

#if IS_BUILTIN(CONFIG_MTK_PLAT_POWER_6781)
fs_initcall(dvfsrc_opp_init);
#else
subsys_initcall(dvfsrc_opp_init);
#endif

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DVFSRC SPMFW INIT");


