// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/perf_event.h>
#include <swpm_perf_arm_pmu.h>
#include "mbraink_pmu.h"

static DEFINE_PER_CPU(unsigned long long,  inst_spec_count);
static u64 spec_instructions[MAX_CPU_CORE_NUM] = {0};
static DEFINE_PER_CPU(unsigned long long,  cycles_count);
static u64 cpu_cycles[MAX_CPU_CORE_NUM] = {0};

static unsigned long long mbraink_pmu_get_inst_count(int cpu)
{
	unsigned long long new = 0;
	unsigned long long old = per_cpu(inst_spec_count, cpu);
	unsigned long long diff = 0;

	new = swpm_pmu_get_count(INST_SPEC_EVT, cpu);
	if (new > old && old > 0)
		diff = (unsigned long long)(new - old);

	per_cpu(inst_spec_count, cpu) = new;
	spec_instructions[cpu] += diff;

	return diff;
}

static unsigned long long mbraink_pmu_get_cpu_cycles(int cpu)
{
	unsigned long long new = 0;
	unsigned long long old = per_cpu(cycles_count, cpu);
	unsigned long long diff = 0;

	new = swpm_pmu_get_count(CYCLES_EVT, cpu);
	if (new > old && old > 0)
		diff = (unsigned long long)(new - old);

	per_cpu(cycles_count, cpu) = new;
	cpu_cycles[cpu] += diff;

	return diff;
}

static void _mbraink_gen_pmu_count(void *val)
{
	int cpu;
	bool set_inst_spec = 0;
	bool set_cpu_cycles = 0;
	struct mbraink_pmu_info *pmuInfo = (struct mbraink_pmu_info *)(val);
	unsigned long options = pmuInfo->pmu_options;

	set_inst_spec = options & (1UL << E_PMU_INST_SPEC);
	set_cpu_cycles = options & (1UL << E_PMU_CPU_CYCLES);

	cpu = smp_processor_id();
	if (cpu >=0 && cpu < MAX_CPU_CORE_NUM) {
		if (set_inst_spec)
			mbraink_pmu_get_inst_count(cpu);
		if (set_cpu_cycles)
			mbraink_pmu_get_cpu_cycles(cpu);
	}
}

int mbraink_enable_pmu_inst_spec(bool enable)
{
	pr_info("mbrain enable pmu inst spec, enable: %d", enable);
	if (enable) {
		/* read perf event from swpm, func enable reset value only */
		spec_instructions[0] = 0;
		spec_instructions[1] = 0;
		spec_instructions[2] = 0;
		spec_instructions[3] = 0;
		spec_instructions[4] = 0;
		spec_instructions[5] = 0;
		spec_instructions[6] = 0;
		spec_instructions[7] = 0;
	}
	return 0;
}

int mbraink_get_pmu_info(struct mbraink_pmu_info *pmuInfo)
{
	int cpu;
	int i = 0;

	for_each_online_cpu(cpu) {
		smp_call_function_single(cpu, _mbraink_gen_pmu_count, pmuInfo, 1);
	}

	for (i = 0; i < MAX_CPU_CORE_NUM; i++) {
		pmuInfo->pmu_data_inst_spec[i] = spec_instructions[i];
		pmuInfo->pmu_data_cpu_cycles[i] = cpu_cycles[i];
	}

	pr_notice("%s: get inst spec: %llu %llu %llu %llu %llu %llu %llu %llu", __func__,
	spec_instructions[0], spec_instructions[1],
	spec_instructions[2], spec_instructions[3],
	spec_instructions[4], spec_instructions[5],
	spec_instructions[6], spec_instructions[7]);

	pr_notice("%s: get cpu cycles: %llu %llu %llu %llu %llu %llu %llu %llu", __func__,
	cpu_cycles[0], cpu_cycles[1],
	cpu_cycles[2], cpu_cycles[3],
	cpu_cycles[4], cpu_cycles[5],
	cpu_cycles[6], cpu_cycles[7]);

	return 0;
}
