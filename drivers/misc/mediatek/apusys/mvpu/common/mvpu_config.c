// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "mvpu_plat.h"
#include "mvpu_config.h"
#include "mvpu2_preempt.h"
#include "mvpu3_preempt.h"

int mvpu_config_init(struct mtk_apu *apu)
{
	int ret = 0;

	pr_info("%s +\n", __func__);

	if (g_mvpu_platdata->sw_ver == MVPU_SW_VER_MVPU20 || g_mvpu_platdata->sw_ver == MVPU_SW_VER_MVPU25 ||
		g_mvpu_platdata->sw_ver == MVPU_SW_VER_MVPU25a|| g_mvpu_platdata->sw_ver == MVPU_SW_VER_MVPU25b) {
		ret = mvpu2_preempt_dram_init(apu);
	} else if (g_mvpu_platdata->sw_ver == MVPU_SW_VER_MVPU30) {
		ret = mvpu3_preempt_dram_init(apu);
	} else {
		pr_info("sw_ver error!");
		ret = -1;
	}

	return ret;
}

int mvpu_config_remove(struct mtk_apu *apu)
{
	int ret = 0;

	if (g_mvpu_platdata->sw_ver == MVPU_SW_VER_MVPU20 || g_mvpu_platdata->sw_ver == MVPU_SW_VER_MVPU25 ||
		g_mvpu_platdata->sw_ver == MVPU_SW_VER_MVPU25a|| g_mvpu_platdata->sw_ver == MVPU_SW_VER_MVPU25b) {
		ret = mvpu2_preempt_dram_deinit(apu);
	} else if (g_mvpu_platdata->sw_ver == MVPU_SW_VER_MVPU30) {
		ret = mvpu3_preempt_dram_deinit(apu);
	} else {
		pr_info("sw_ver error!");
		ret = -1;
	}

	return ret;
}
