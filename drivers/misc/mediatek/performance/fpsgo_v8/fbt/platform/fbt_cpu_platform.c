// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/cpumask.h>
#include <linux/interconnect.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/mod_devicetable.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include <uapi/linux/sched/types.h>
#include <mt-plat/fpsgo_common.h>
#include "dvfsrc-exp.h"
#include "fpsgo_frame_info.h"
#include "fpsgo_base.h"
#include "fbt_cpu_platform.h"
#include <common.h>

static int mask_int[FPSGO_PREFER_TOTAL];
static struct cpumask mask[FPSGO_PREFER_TOTAL];
static int mask_done;
static struct device_node *node;
static int plat_cpu_limit;

static int generate_cpu_mask(void);

static int platform_fpsgo_probe(struct platform_device *pdev)
{
	int ret = 0, retval = 0;

	node = pdev->dev.of_node;

	FPSGO_LOGE("%s\n", __func__);

	ret = of_property_read_u32(node,
			 "cpu-limit", &retval);
	if (!ret)
		plat_cpu_limit = retval;

	generate_cpu_mask();

	return 0;
}

static void platform_fpsgo_remove(struct platform_device *pdev)
{ }

static const struct of_device_id platform_fpsgo_of_match[] = {
	{ .compatible = "mediatek,fpsgo", },
	{},
};

static const struct platform_device_id platform_fpsgo_id_table[] = {
	{ "fpsgo", 0},
	{ },
};

static struct platform_driver mtk_platform_fpsgo_driver = {
	.probe = platform_fpsgo_probe,
	.remove	= platform_fpsgo_remove,
	.driver = {
		.name = "fpsgo",
		.owner = THIS_MODULE,
		.of_match_table = platform_fpsgo_of_match,
	},
	.id_table = platform_fpsgo_id_table,
};

void init_fbt_platform(void)
{
	FPSGO_LOGE("%s\n", __func__);
	platform_driver_register(&mtk_platform_fpsgo_driver);
}

void exit_fbt_platform(void)
{
	platform_driver_unregister(&mtk_platform_fpsgo_driver);
}

void fbt_set_boost_value(unsigned int base_blc)
{
	base_blc = clamp(base_blc, 1U, 100U);
	fpsgo_sentcmd(FPSGO_SET_BOOST_TA, base_blc, -1);
	fpsgo_systrace_c_fbt_debug(-100, 0, base_blc, "TA_cap");
}

void fbt_clear_boost_value(void)
{
	fpsgo_sentcmd(FPSGO_SET_BOOST_TA, -1, -1);
	fpsgo_systrace_c_fbt_debug(-100, 0, 0, "TA_cap");
}

void fbt_set_per_task_cap(int pid, unsigned int min_blc,
			unsigned int max_blc, unsigned int max_util)
{
	int ret = -1;
	unsigned int min_blc_1024;
	unsigned int max_blc_1024;
	struct task_struct *p;
	struct sched_attr attr = {};
	unsigned long cur_min = 0, cur_max = 0;

	if (!pid)
		return;

	max_blc_1024 = (max_util != 1024U) ? max_util : ((max_blc << 10) / 100U);
	max_blc_1024 = clamp(max_blc_1024, 1U, 1024U);

	min_blc_1024 = (min_blc << 10) / 100U;
	min_blc_1024 = clamp(min_blc_1024, 1U, max_blc_1024);

	attr.sched_policy = -1;
	attr.sched_flags =
		SCHED_FLAG_KEEP_ALL |
		SCHED_FLAG_UTIL_CLAMP |
		SCHED_FLAG_RESET_ON_FORK;

	if (min_blc == 0 && max_blc == 100) {
		attr.sched_util_min = -1;
		attr.sched_util_max = -1;
	} else {
		attr.sched_util_min = min_blc_1024;
		attr.sched_util_max = max_blc_1024;
	}

	if (pid < 0)
		goto out;

	rcu_read_lock();
	p = find_task_by_vpid(pid);

	if (likely(p))
		get_task_struct(p);

	rcu_read_unlock();

	if (likely(p)) {
		cur_min = uclamp_eff_value(p, UCLAMP_MIN);
		cur_max = uclamp_eff_value(p, UCLAMP_MAX);
		if (cur_min != attr.sched_util_min || cur_max != attr.sched_util_max) {
			attr.sched_policy = p->policy;
		if (rt_policy(p->policy))
			attr.sched_priority = p->rt_priority;
		ret = sched_setattr_nocheck(p, &attr);
		}
		put_task_struct(p);
	}

out:
	if (ret != 0) {
		fpsgo_systrace_c_fbt(pid, 0, ret, "uclamp fail");
		fpsgo_systrace_c_fbt(pid, 0, 0, "uclamp fail");
		return;
	}

	fpsgo_systrace_c_fbt_debug(pid, 0, attr.sched_util_min, "min_cap");
	fpsgo_systrace_c_fbt_debug(pid, 0, attr.sched_util_max, "max_cap");
}

int get_fbt_cpu_mask(int prefer_type, int *get_mask)
{
	int ret = 0;

	if (!mask_done)
		return -100;
	if (prefer_type < 0 || prefer_type >= FPSGO_PREFER_TOTAL)
		return -EINVAL;

	*get_mask = mask_int[prefer_type];

	return ret;
}

static int generate_cpu_mask(void)
{
	int i, ret, cpu;
	int temp_mask = 0;

	ret = of_property_read_u32_array(node, "fbt-cpu-mask",
			mask_int, FPSGO_PREFER_TOTAL);

	for (i = 0; i < FPSGO_PREFER_TOTAL; i++) {
		cpumask_clear(&mask[i]);
		temp_mask = mask_int[i];
		for_each_possible_cpu(cpu) {
			if (temp_mask & (1 << cpu))
				cpumask_set_cpu(cpu, &mask[i]);
		}
		FPSGO_LOGE("%s i:%d mask:%d %*pbl\n",
			__func__, i, mask_int[i], cpumask_pr_args(&mask[i]));
	}

	if (!ret)
		mask_done = 1;

	return ret;
}

struct cpumask fbt_generate_user_cpu_mask(int mask_int)
{
	unsigned long cpumask_ulval = mask_int;
	struct cpumask cpumask_setting;
	int cpu;

	cpumask_clear(&cpumask_setting);
	for_each_possible_cpu(cpu) {
		if (cpumask_ulval & (1 << cpu))
			cpumask_set_cpu(cpu, &cpumask_setting);
	}
	return cpumask_setting;
}

int fbt_check_ls(int pid)
{
	int ls = 0;

#if IS_ENABLED(CONFIG_MTK_SCHEDULER)
	struct task_struct *tsk;
	rcu_read_lock();
	tsk = find_task_by_vpid(pid);
	if (!tsk)
		goto EXIT;
	get_task_struct(tsk);
	ls = is_task_latency_sensitive(tsk);
	put_task_struct(tsk);

EXIT:
	rcu_read_unlock();
#endif

	return ls;
}

int fbt_set_soft_affinity(int pid, int set, unsigned int prefer_type)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_MTK_SCHEDULER)
	if (!mask_done) {
		ret = -100;
		goto out;
	}

	if (set)
		set_task_ls_prefer_cpus(pid, mask_int[prefer_type]);
	else
		unset_task_ls_prefer_cpus(pid);

out:
	if (ret != 0) {
		fpsgo_systrace_c_fbt(pid, 0, ret, "softaffinity fail");
		fpsgo_systrace_c_fbt(pid, 0, 0, "softaffinity fail");
		return ret;
	}
	fpsgo_systrace_c_fbt(pid, 0, prefer_type, "soft_affinity");
#endif
	return ret;
}

int fbt_set_affinity(pid_t pid, unsigned int prefer_type)
{
	int ret = 0;

	if (!mask_done) {
		ret = -100;
		goto out;
	}

	ret = fpsgo_sched_setaffinity(pid, &mask[prefer_type]);

out:
	if (ret != 0) {
		fpsgo_systrace_c_fbt(pid, 0, ret, "setaffinity fail");
		fpsgo_systrace_c_fbt(pid, 0, 0, "setaffinity fail");
		return ret;
	}
	fpsgo_systrace_c_fbt(pid, 0, prefer_type, "set_affinity");
	return ret;
}

int fbt_get_cluster_limit(int *cluster, int *freq, int *r_freq, int *cpu)
{
/*
 * Use return value to specify limit on frequency or core
 * 1. when return value is FPSGO_LIMIT_NO_LIMIT -> no limit
 * 2. when return value is FPSGO_LIMIT_FREQ -> frequency limit
 * 2.1 when cluster is set and freq is set -> ceiling limit
 * 2.2 when cluster is set and r_freq is set -> rescue ceiling limit
 * 3. when return value is FPSGO_LIMIT_CPU -> cpu limit
 * 3.1 when cpu is set valid -> cpu isolation
 */
	int limit = plat_cpu_limit;

	if (limit == FPSGO_LIMIT_CPU)
		*cpu = 7;
	else if (limit == FPSGO_LIMIT_FREQ) {
		*cluster = 2;
		*freq = 2600000;
	}

	return limit;
}
