// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/perf_event.h>
#include <mbraink_modules_ops_def.h>

#include "mbraink_v6993_pmu.h"
#include "perf/mbraink_perf.h"

static DEFINE_PER_CPU(unsigned long long,  inst_spec_count);
static u64 spec_instructions[MAX_CPU_CORE_NUM] = {0};
static DEFINE_PER_CPU(unsigned long long,  cycles_count);
static u64 cpu_cycles[MAX_CPU_CORE_NUM] = {0};
static DEFINE_PER_CPU(unsigned long long,  l1dc_count);
static u64 cpu_l1dc[MAX_CPU_CORE_NUM] = {0};
static DEFINE_PER_CPU(unsigned long long,  l1dc_ref_count);
static u64 cpu_l1dc_ref[MAX_CPU_CORE_NUM] = {0};
static DEFINE_PER_CPU(unsigned long long,  l2dc_count);
static u64 cpu_l2dc[MAX_CPU_CORE_NUM] = {0};
static DEFINE_PER_CPU(unsigned long long,  l2dc_ref_count);
static u64 cpu_l2dc_ref[MAX_CPU_CORE_NUM] = {0};
static DEFINE_PER_CPU(unsigned long long,  l3dc_count);
static u64 cpu_l3dc[MAX_CPU_CORE_NUM] = {0};
static DEFINE_PER_CPU(unsigned long long,  l3dc_ref_count);
static u64 cpu_l3dc_ref[MAX_CPU_CORE_NUM] = {0};

static unsigned long long mbraink_pmu_get_inst_count(int cpu)
{
	unsigned long long new = 0;
	unsigned long long old = per_cpu(inst_spec_count, cpu);
	unsigned long long diff = 0;

	new = mbraink_perf_pmu_get_count(MBK_INST_SPEC_EVT, cpu);
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

	new = mbraink_perf_pmu_get_count(MBK_CYCLES_EVT, cpu);
	if (new > old && old > 0)
		diff = (unsigned long long)(new - old);

	per_cpu(cycles_count, cpu) = new;
	cpu_cycles[cpu] += diff;

	return diff;
}

static unsigned long long mbraink_pmu_get_l1dc(int cpu)
{
	unsigned long long new = 0;
	unsigned long long old = per_cpu(l1dc_count, cpu);
	unsigned long long diff = 0;

	new = mbraink_perf_pmu_get_count(MBK_L1DC_EVT, cpu);
	if (new > old && old > 0)
		diff = (unsigned long long)(new - old);

	per_cpu(l1dc_count, cpu) = new;
	cpu_l1dc[cpu] += diff;

	return diff;
}

static unsigned long long mbraink_pmu_get_l1dc_ref(int cpu)
{
	unsigned long long new = 0;
	unsigned long long old = per_cpu(l1dc_ref_count, cpu);
	unsigned long long diff = 0;

	new = mbraink_perf_pmu_get_count(MBK_L1DC_ERF_EVT, cpu);
	if (new > old && old > 0)
		diff = (unsigned long long)(new - old);

	per_cpu(l1dc_ref_count, cpu) = new;
	cpu_l1dc_ref[cpu] += diff;

	return diff;
}

static unsigned long long mbraink_pmu_get_l2dc(int cpu)
{
	unsigned long long new = 0;
	unsigned long long old = per_cpu(l2dc_count, cpu);
	unsigned long long diff = 0;

	new = mbraink_perf_pmu_get_count(MBK_L2DC_EVT, cpu);
	if (new > old && old > 0)
		diff = (unsigned long long)(new - old);

	per_cpu(l2dc_count, cpu) = new;
	cpu_l2dc[cpu] += diff;

	return diff;
}

static unsigned long long mbraink_pmu_get_l2dc_ref(int cpu)
{
	unsigned long long new = 0;
	unsigned long long old = per_cpu(l2dc_ref_count, cpu);
	unsigned long long diff = 0;

	new = mbraink_perf_pmu_get_count(MBK_L2DC_ERF_EVT, cpu);
	if (new > old && old > 0)
		diff = (unsigned long long)(new - old);

	per_cpu(l2dc_ref_count, cpu) = new;
	cpu_l2dc_ref[cpu] += diff;

	return diff;
}

static unsigned long long mbraink_pmu_get_l3dc(int cpu)
{
	unsigned long long new = 0;
	unsigned long long old = per_cpu(l3dc_count, cpu);
	unsigned long long diff = 0;

	new = mbraink_perf_pmu_get_count(MBK_L3DC_EVT, cpu);
	if (new > old && old > 0)
		diff = (unsigned long long)(new - old);

	per_cpu(l3dc_count, cpu) = new;
	cpu_l3dc[cpu] += diff;

	return diff;
}

static unsigned long long mbraink_pmu_get_l3dc_ref(int cpu)
{
	unsigned long long new = 0;
	unsigned long long old = per_cpu(l3dc_ref_count, cpu);
	unsigned long long diff = 0;

	new = mbraink_perf_pmu_get_count(MBK_L3DC_ERF_EVT, cpu);
	if (new > old && old > 0)
		diff = (unsigned long long)(new - old);

	per_cpu(l3dc_ref_count, cpu) = new;
	cpu_l3dc_ref[cpu] += diff;

	return diff;
}

static void _mbraink_gen_pmu_count(void *val)
{
	int cpu;
	bool set_inst_spec = 0;
	bool set_cpu_cycles = 0;
	bool set_cpu_l1_cache_access = 0;
	bool set_cpu_l1_cache_refill = 0;
	bool set_cpu_l2_cache_access = 0;
	bool set_cpu_l2_cache_refill = 0;
	bool set_cpu_l3_cache_access = 0;
	bool set_cpu_l3_cache_refill = 0;
	struct mbraink_pmu_info *pmuInfo = (struct mbraink_pmu_info *)(val);
	unsigned long options = pmuInfo->pmu_options;

	set_inst_spec = options & (1UL << E_PMU_INST_SPEC);
	set_cpu_cycles = options & (1UL << E_PMU_CPU_CYCLES);
	set_cpu_l1_cache_access = options & (1UL << E_PMU_CPU_L1_CACHE_ACCESS);
	set_cpu_l1_cache_refill = options & (1UL << E_PMU_CPU_L1_CACHE_REFILL);
	set_cpu_l2_cache_access = options & (1UL << E_PMU_CPU_L2_CACHE_ACCESS);
	set_cpu_l2_cache_refill = options & (1UL << E_PMU_CPU_L2_CACHE_REFILL);
	set_cpu_l3_cache_access = options & (1UL << E_PMU_CPU_L3_CACHE_ACCESS);
	set_cpu_l3_cache_refill = options & (1UL << E_PMU_CPU_L3_CACHE_REFILL);

	cpu = smp_processor_id();
	if (cpu >=0 && cpu < MAX_CPU_CORE_NUM) {
		if (set_inst_spec)
			mbraink_pmu_get_inst_count(cpu);
		if (set_cpu_cycles)
			mbraink_pmu_get_cpu_cycles(cpu);
		if (set_cpu_l1_cache_access)
			mbraink_pmu_get_l1dc(cpu);
		if (set_cpu_l1_cache_refill)
			mbraink_pmu_get_l1dc_ref(cpu);
		if (set_cpu_l2_cache_access)
			mbraink_pmu_get_l2dc(cpu);
		if (set_cpu_l2_cache_refill)
			mbraink_pmu_get_l2dc_ref(cpu);
		if (set_cpu_l3_cache_access)
			mbraink_pmu_get_l3dc(cpu);
		if (set_cpu_l3_cache_refill)
			mbraink_pmu_get_l3dc_ref(cpu);
	}
}

int mbraink_v6993_set_pmu_enable(bool enable)
{
	int ret = 0;

	ret = mbraink_perf_probe_enable(enable);
	return ret;
}

int mbraink_v6993_get_pmu_info(struct mbraink_pmu_info *pmuInfo)
{
	int cpu;
	int i = 0;

	for_each_online_cpu(cpu) {
		smp_call_function_single(cpu, _mbraink_gen_pmu_count, pmuInfo, 1);
	}

	for (i = 0; i < MAX_CPU_CORE_NUM; i++) {
		pmuInfo->pmu_data_inst_spec[i] = spec_instructions[i];
		pmuInfo->pmu_data_cpu_cycles[i] = cpu_cycles[i];
		pmuInfo->pmu_data_cpu_l1_cache_access[i] = cpu_l1dc[i];
		pmuInfo->pmu_data_cpu_l1_cache_refill[i] = cpu_l1dc_ref[i];
		pmuInfo->pmu_data_cpu_l2_cache_access[i] = cpu_l2dc[i];
		pmuInfo->pmu_data_cpu_l2_cache_refill[i] = cpu_l2dc_ref[i];
		pmuInfo->pmu_data_cpu_l3_cache_access[i] = cpu_l3dc[i];
		pmuInfo->pmu_data_cpu_l3_cache_refill[i] = cpu_l3dc_ref[i];
	}

	return 0;
}

static struct mbraink_pmu_ops mbraink_v6993_pmu_ops = {
	.set_pmu_enable = mbraink_v6993_set_pmu_enable,
	.get_pmu_info = mbraink_v6993_get_pmu_info,
};

int mbraink_v6993_pmu_init(void)
{
	int ret = 0;

	ret = register_mbraink_pmu_ops(&mbraink_v6993_pmu_ops);
	return ret;
}

int mbraink_v6993_pmu_deinit(void)
{
	int ret = 0;

	ret = unregister_mbraink_pmu_ops();
	return ret;
}
