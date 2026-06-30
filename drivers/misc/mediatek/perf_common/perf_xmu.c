// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2025 MediaTek Inc.
 */
#include <linux/percpu.h>
#include <asm/sysreg.h>

#include "perf_tracker_trace.h"
#include "perf_tracker_internal.h"


#if IS_ENABLED(CONFIG_ARM64_AMU_EXTN)
#define read_amu_core()	read_sysreg_s(SYS_AMEVCNTR0_CORE_EL0)
#define read_amu_inst()	read_sysreg_s(SYS_AMEVCNTR0_INST_RET_EL0)
#define read_amu_mem_stall() read_sysreg_s(SYS_AMEVCNTR0_MEM_STALL)
#else
#define read_amu_core()	(0UL)
#define read_amu_inst() (0UL)
#define read_amu_mem_stall() (0UL)
#endif

static inline u64 read_pmc_core(void)
{
	u64 curr;
#if IS_ENABLED(CONFIG_ARM64)
	asm volatile("mrs %0, pmccntr_el0" : "=r" (curr));
#else
	asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r" (curr));
#endif
	return curr;
}

static DEFINE_PER_CPU(u64,  pmc_core_prev);
static DEFINE_PER_CPU(u64,  amu_core_prev);
static DEFINE_PER_CPU(u64,  amu_inst_prev);
static DEFINE_PER_CPU(u64,  amu_be_stall_mem_prev);


void update_xmu_info(void)
{
	u64 pmc_core_old, amu_core_old, amu_inst_old, amu_be_stall_mem_old;
	u64 pmc_core_curr, amu_core_curr, amu_inst_curr, amu_be_stall_mem_curr;
	u32 pmc_core_diff, amu_core_diff, amu_inst_diff, amu_be_stall_mem_diff;

	pmc_core_old = this_cpu_read(pmc_core_prev);
	amu_core_old = this_cpu_read(amu_core_prev);
	amu_inst_old = this_cpu_read(amu_inst_prev);
	amu_be_stall_mem_old = this_cpu_read(amu_be_stall_mem_prev);

	pmc_core_curr = read_pmc_core();
	amu_core_curr = read_amu_core();
	amu_inst_curr = read_amu_inst();
	amu_be_stall_mem_curr = read_amu_mem_stall();

	this_cpu_write(pmc_core_prev, pmc_core_curr);
	this_cpu_write(amu_core_prev, amu_core_curr);
	this_cpu_write(amu_inst_prev, amu_inst_curr);
	this_cpu_write(amu_be_stall_mem_prev, amu_be_stall_mem_curr);

	//Ignore invalid data
	if (unlikely(amu_core_old == 0))
		return;

	pmc_core_diff = pmc_core_curr - pmc_core_old;
	amu_core_diff = amu_core_curr - amu_core_old;
	amu_inst_diff = amu_inst_curr - amu_inst_old;
	amu_be_stall_mem_diff = amu_be_stall_mem_curr - amu_be_stall_mem_old;

	trace_perf_index_xmu(pmc_core_diff, amu_core_diff, amu_inst_diff, amu_be_stall_mem_diff);
	trace_perf_index_xmu_debug(pmc_core_curr, amu_core_curr, amu_inst_curr, amu_be_stall_mem_curr);
}

