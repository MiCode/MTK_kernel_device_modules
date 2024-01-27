// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include <linux/of.h>
#include "common.h"

MODULE_LICENSE("GPL");


#ifdef CONFIG_UCLAMP_TASK
/**
 * uclamp_rq_util_with - clamp @util with @rq and @p effective uclamp values.
 * @rq:		The rq to clamp against. Must not be NULL.
 * @util:	The util value to clamp.
 * @p:		The task to clamp against. Can be NULL if you want to clamp
 *		against @rq only.
 *
 * Clamps the passed @util to the max(@rq, @p) effective uclamp values.
 *
 * If sched_uclamp_used static key is disabled, then just return the util
 * without any clamping since uclamp aggregation at the rq level in the fast
 * path is disabled, rendering this operation a NOP.
 *
 * Use uclamp_eff_value() if you don't care about uclamp values at rq level. It
 * will return the correct effective uclamp value of the task even if the
 * static key is disabled.
 */
__always_inline
unsigned long mtk_uclamp_rq_util_with(struct rq *rq, unsigned long util,
				  struct task_struct *p,
				  unsigned long min_cap, unsigned long max_cap)
{
	unsigned long min_util;
	unsigned long max_util;

	if (!static_branch_likely(&sched_uclamp_used))
		return util;

	min_util = READ_ONCE(rq->uclamp[UCLAMP_MIN].value);
	max_util = READ_ONCE(rq->uclamp[UCLAMP_MAX].value);

	if (p) {
		min_util = max(min_util, min_cap);
#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
		max_util = (!rq->nr_running ? max_cap : max(max_util, max_cap));
#else
		max_util = max(max_util, max_cap);
#endif
	}

	/*
	 * Since CPU's {min,max}_util clamps are MAX aggregated considering
	 * RUNNABLE tasks with _different_ clamps, we can end up with an
	 * inversion. Fix it now when the clamps are applied.
	 */
	if (unlikely(min_util >= max_util))
		return min_util;

	return clamp(util, min_util, max_util);
}
#else /* CONFIG_UCLAMP_TASK */
__always_inline
unsigned long mtk_uclamp_rq_util_with(struct rq *rq, unsigned long util,
				  struct task_struct *p,
				  unsigned long min_cap, unsigned long max_cap)
{
	return util;
}
#endif /* CONFIG_UCLAMP_TASK */

void parse_eas_data(struct eas_info *info)
{
	struct device_node *dn = NULL;
	int ret;

	dn = of_find_node_by_name(NULL, EAS_NODE_NAME);
	if (dn) {
		ret = of_property_read_u32(dn, EAS_PROP_CSRAM, &info->csram_base);
		ret |= of_property_read_u32(dn, EAS_PROP_OFFS_CAP, &info->offs_cap);
		ret |= of_property_read_u32_index(dn, EAS_PROP_OFFS_THERMAL_S, 0,
					&info->offs_thermal_limit_s);
		info->available = !ret;
		pr_info("Get info: base_addr=0x%x, cap=0x%x, thermal=0x%x, avai=%d\n",
					info->csram_base, info->offs_cap,
					info->offs_thermal_limit_s, info->available);
	} else {
		pr_info("No EAS node!\n");
		info->available = false;
	}
}
