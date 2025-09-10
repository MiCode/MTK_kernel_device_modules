// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/sched/signal.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/tick.h>
#include <linux/types.h>
#include <linux/cpumask.h>

#include "videogo_utils.h"

struct cpu_stat {
	u64 last_idle;
	u64 last_total;
	int valid;
};

static struct cpu_stat *cpu_stats;
static int cpu_count;
static DEFINE_SPINLOCK(cpu_stats_lock);

void cpu_usage_init(void)
{
	cpu_count = num_possible_cpus();
	cpu_stats = kcalloc(cpu_count, sizeof(struct cpu_stat), GFP_KERNEL);
}

void cpu_usage_exit(void)
{
	kfree(cpu_stats);
}

int get_cpu_usage(int cpu)
{
	u64 idle, total = 0;
	int usage = 0;

	if (cpu < 0 || cpu >= cpu_count)
		return 0;

	idle = get_cpu_idle_time_us(cpu, &total);

	spin_lock(&cpu_stats_lock);

	if (!cpu_stats[cpu].valid) {
		cpu_stats[cpu].last_idle = idle;
		cpu_stats[cpu].last_total = total;
		cpu_stats[cpu].valid = 1;
		usage = 0;
	} else {
		u64 idle_diff = idle - cpu_stats[cpu].last_idle;
		u64 total_diff = total - cpu_stats[cpu].last_total;

		if (total_diff == 0)
			usage = 0;
		else {
			if (idle_diff > total_diff)
				idle_diff = total_diff;
			int tmp_usage = 100 - (int)(idle_diff * 100 / total_diff);

			if (tmp_usage < 0)
				usage = 0;
			else if (tmp_usage > 100)
				usage = 100;
			else
				usage = tmp_usage;
		}

		cpu_stats[cpu].last_idle = idle;
		cpu_stats[cpu].last_total = total;
	}

	spin_unlock(&cpu_stats_lock);

	return usage;
}
