// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/cpufreq.h>
#include <linux/notifier.h>
#include <linux/pm_qos.h>
#include <linux/slab.h>
#include <linux/minmax.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include <sugov/cpufreq.h>

MODULE_LICENSE("GPL");
/*
 *  max_freq_scale:
 *	max cpu frequency limit
 *	range: 0~SCHED_CAPACITY_SCALE
 *  min_freq:
 *	min cpu frequency limit
 *	unit: frequency
 */
DEFINE_PER_CPU(unsigned long, max_freq_scale) = SCHED_CAPACITY_SCALE;
DEFINE_PER_CPU(unsigned long, min_freq_scale) = 0;
DEFINE_PER_CPU(unsigned long, min_freq) = 0;

#if IS_ENABLED(CONFIG_MTK_EAS)
static struct notifier_block *freq_limit_max_notifier, *freq_limit_min_notifier;
static unsigned int nr_gears;

static int freq_limit_max_notifier_call(struct notifier_block *nb,
					 unsigned long freq_limit_max, void *ptr)
{
	int gear_idx = nb - freq_limit_max_notifier;
	unsigned int cpu;

	if (gear_idx < 0 || gear_idx >= nr_gears) {
		pr_info("freq_limit_max_notifier_call: gear_idx over-index\n");
		return -1;
	}

	for_each_possible_cpu(cpu) {
		if (topology_cluster_id(cpu) == gear_idx)
			WRITE_ONCE(per_cpu(max_freq_scale, cpu),
				pd_get_freq_util(cpu, freq_limit_max));
	}

	return 0;
}

static int freq_limit_min_notifier_call(struct notifier_block *nb,
					 unsigned long freq_limit_min, void *ptr)
{
	int gear_idx = nb - freq_limit_min_notifier;
	unsigned int cpu;

	if (gear_idx < 0 || gear_idx >= nr_gears) {
		pr_info("freq_limit_min_notifier_call: gear_idx over-index\n");
		return -1;
	}

	for_each_possible_cpu(cpu) {
		if (topology_cluster_id(cpu) == gear_idx) {
			per_cpu(min_freq, cpu) = freq_limit_min;
			WRITE_ONCE(per_cpu(min_freq_scale, cpu),
				pd_get_freq_util(cpu, freq_limit_min));
		}
	}

	return 0;
}

void mtk_freq_limit_notifier_register(void)
{
	struct cpufreq_policy *policy;
	int cpu, gear_idx = 0, ret;

	nr_gears = get_nr_gears();
	freq_limit_max_notifier = kcalloc(nr_gears, sizeof(struct notifier_block), GFP_KERNEL);
	freq_limit_min_notifier = kcalloc(nr_gears, sizeof(struct notifier_block), GFP_KERNEL);

	for_each_possible_cpu(cpu) {
		if (gear_idx >= nr_gears) {
			pr_info("mtk_freq_limit_notifier_register: gear_idx over-index\n");
			break;
		}
		policy = cpufreq_cpu_get(cpu);

		if (policy) {
			freq_limit_max_notifier[gear_idx].notifier_call
				= freq_limit_max_notifier_call;
			freq_limit_min_notifier[gear_idx].notifier_call
				= freq_limit_min_notifier_call;

			ret = freq_qos_add_notifier(&policy->constraints, FREQ_QOS_MAX,
				freq_limit_max_notifier + gear_idx);
			if (ret)
				pr_info("freq_qos_add_notifier freq_limit_max_notifier failed\n");

			ret = freq_qos_add_notifier(&policy->constraints, FREQ_QOS_MIN,
				freq_limit_min_notifier + gear_idx);
			if (ret)
				pr_info("freq_qos_add_notifier freq_limit_min_notifier failed\n");

			gear_idx++;
			cpu = cpumask_last(policy->related_cpus);
			cpufreq_cpu_put(policy);
		}
	}
}

void mtk_update_cpu_capacity(void *data, int cpu, unsigned long *capacity)
{
	unsigned long cap_ceiling;

	cap_ceiling = min_t(unsigned long, *capacity, get_cpu_gear_uclamp_max_capacity(cpu));
	*capacity = clamp_t(unsigned long, cap_ceiling,
		READ_ONCE(per_cpu(min_freq_scale, cpu)), READ_ONCE(per_cpu(max_freq_scale, cpu)));
}

unsigned long cpu_cap_ceiling(int cpu)
{
	unsigned long cap_ceiling;

	cap_ceiling = min_t(unsigned long, capacity_orig_of(cpu),
		get_cpu_gear_uclamp_max_capacity(cpu));
	return clamp_t(unsigned long, cap_ceiling,
		READ_ONCE(per_cpu(min_freq_scale, cpu)), READ_ONCE(per_cpu(max_freq_scale, cpu)));
}

#if !IS_ENABLED(CONFIG_ARM64)
static int __init get_cpu_for_node(struct device_node *node)
{
	struct device_node *cpu_node;
	int cpu;

	cpu_node = of_parse_phandle(node, "cpu", 0);
	if (!cpu_node)
		return -1;

	for_each_possible_cpu(cpu) {
		if (of_get_cpu_node(cpu, NULL) == cpu_node) {
			topology_parse_cpu_capacity(cpu_node, cpu);
			of_node_put(cpu_node);
			return cpu;
		}
	}

	pr_crit("Unable to find CPU node for %pOF\n", cpu_node);

	of_node_put(cpu_node);
	return -1;
}

static int __init parse_core(struct device_node *core, int cluster_id,
				int core_id)
{
	char name[10];
	bool leaf = true;
	int i = 0;
	int cpu;
	struct device_node *t;

	do {
		snprintf(name, sizeof(name), "thread%d", i);
		t = of_get_child_by_name(core, name);
		if (t) {
			leaf = false;
			cpu = get_cpu_for_node(t);
			if (cpu >= 0) {
				cpu_topology[cpu].package_id = cluster_id;
				cpu_topology[cpu].cluster_id = cluster_id;
				cpu_topology[cpu].core_id = core_id;
				cpu_topology[cpu].thread_id = i;
			} else {
				pr_err("%pOF: Can't get CPU for thread\n",
					t);
				of_node_put(t);
				return -EINVAL;
			}
			of_node_put(t);
		}
		i++;
	} while (t);

	cpu = get_cpu_for_node(core);
	if (cpu >= 0) {
		if (!leaf) {
			pr_err("%pOF: Core has both threads and CPU\n",
			core);
			return -EINVAL;
		}

		cpu_topology[cpu].package_id = cluster_id;
		cpu_topology[cpu].cluster_id = cluster_id;
		cpu_topology[cpu].core_id = core_id;
	} else if (leaf) {
		pr_err("%pOF: Can't get CPU for leaf core\n", core);
		return -EINVAL;
	}

	return 0;
}

static int __init parse_cluster(struct device_node *cluster, int depth)
{
	char name[10];
	bool leaf = true;
	bool has_cores = false;
	struct device_node *c;

	static int cluster_id __initdata;

	int core_id = 0;
	int i, ret;

	i = 0;
	do {
		snprintf(name, sizeof(name), "cluster%d", i);
		c = of_get_child_by_name(cluster, name);
		if (c) {
			leaf = false;
			ret = parse_cluster(c, depth + 1);
			of_node_put(c);
			if (ret != 0)
				return ret;
		}
		i++;
	} while (c);

	i = 0;
	do {
		snprintf(name, sizeof(name), "core%d", i);
		c = of_get_child_by_name(cluster, name);
		if (c) {
			has_cores = true;

			if (depth == 0) {
				pr_err("%pOF: cpu-map children should be clusters\n",
					c);
				of_node_put(c);
				return -EINVAL;
			}

			if (leaf) {
				ret = parse_core(c, cluster_id, core_id++);
			} else {
				pr_err("%pOF: Non-leaf cluster with core %s\n",
					cluster, name);
				ret = -EINVAL;
			}

			of_node_put(c);
			if (ret != 0)
				return ret;
		}
		i++;
	} while (c);

	if (leaf && !has_cores)
		pr_warn("%pOF: empty cluster\n", cluster);

	if (leaf)
		cluster_id++;

	return 0;
}

int __init parse_dt_topology_arm(void)
{
	struct device_node *cn_cpus = NULL;
	struct device_node *map;
	int ret;

	pr_info("parse_dt_topology\n");
	cn_cpus = of_find_node_by_path("/cpus");
	if (!cn_cpus) {
		pr_err("No CPU information found in DT\n");
		return -EINVAL;
	}

	map = of_get_child_by_name(cn_cpus, "cpu-map");
	if (!map) {
		pr_err("No cpu-map information found in DT\n");
		return -EINVAL;
	}

	ret = parse_cluster(map, 0);
	of_node_put(map);

	return ret;
}


#endif

#endif
