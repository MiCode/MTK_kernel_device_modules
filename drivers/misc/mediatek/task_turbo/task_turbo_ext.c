// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2025 MediaTek Inc.
 */


#include <linux/arm-smccc.h>
#include <linux/module.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>

#include "../sda/sda.h"
//#include <task_turbo.h>

#define TAG "Task-Turbo"
static unsigned int mem_rename_opt;

void init_mem_rename_opt(void)
{
	struct arm_smccc_res res;

	mem_rename_opt = 1;

	arm_smccc_smc(MTK_SIP_SDA_CONTROL, SDA_ERRATA_3502731_CTRL, ERRATA_STATUS_GET,
		0, 0, 0, 0, 0, &res);
	if (res.a0 == ERRATA_DISABLE || res.a0 == ERRATA_ENABLE) {
		pr_info("%s: %s successfully: %lu\n", TAG, __func__, res.a0);
		mem_rename_opt = res.a0;
	}

}

static int set_mem_rename_opt(const char *buf,
				const struct kernel_param *kp)
{
	int ret;
	unsigned int val = NR_ERRATA_CTRL;
	struct arm_smccc_res res;

	ret = kstrtouint(buf, 0, &val);
	if (val >= NR_ERRATA_CTRL)
		ret = -EINVAL;

	if (!ret) {
		if(val == ERRATA_DISABLE) {
			arm_smccc_smc(MTK_SIP_SDA_CONTROL, SDA_ERRATA_3502731_CTRL, ERRATA_DISABLE,
				0, 0, 0, 0, 0, &res);
			if (res.a0) {
				pr_info("%s: %s ERRATA_DISABLE fail\n", TAG, __func__);
				return -EINVAL;
			}
		}
		if(val == ERRATA_ENABLE) {
			arm_smccc_smc(MTK_SIP_SDA_CONTROL, SDA_ERRATA_3502731_CTRL, ERRATA_ENABLE,
				0, 0, 0, 0, 0, &res);
			if (res.a0) {
				pr_info("%s: %s ERRATA_ENABLE fail\n", TAG, __func__);
				return -EINVAL;
			}
		}
		mem_rename_opt = val;
	}

	return ret;
}

static const struct kernel_param_ops mem_rename_opt_param_ops = {
	.set = set_mem_rename_opt,
	.get = param_get_uint,
};

param_check_uint(mem_rename_opt, &mem_rename_opt);
module_param_cb(mem_rename_opt, &mem_rename_opt_param_ops, &mem_rename_opt, 0644);
MODULE_PARM_DESC(mem_rename_opt, "enable mem_rename_opt features if needed");
