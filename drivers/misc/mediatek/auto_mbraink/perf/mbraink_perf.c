// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#include <linux/perf_event.h>
#include <linux/perf/arm_pmuv3.h>
#include "mbraink_perf.h"

static DEFINE_PER_CPU(struct perf_event *, inst_spec_events);
static DEFINE_PER_CPU(struct perf_event *, cycle_events);
static DEFINE_PER_CPU(struct perf_event *, l1dc_events);
static DEFINE_PER_CPU(struct perf_event *, l1dc_ref_events);
static DEFINE_PER_CPU(struct perf_event *, l2dc_events);
static DEFINE_PER_CPU(struct perf_event *, l2dc_ref_events);
static DEFINE_PER_CPU(struct perf_event *, l3dc_events);
static DEFINE_PER_CPU(struct perf_event *, l3dc_ref_events);

static struct perf_event_attr inst_spec_event_attr = {
	.type           = PERF_TYPE_RAW,
/*	.config         = 0x1B, */
	.config         = ARMV8_PMUV3_PERFCTR_INST_SPEC, /* 0x1B */
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
};
static struct perf_event_attr cycle_event_attr = {
	.type           = PERF_TYPE_HARDWARE,
	.config         = PERF_COUNT_HW_CPU_CYCLES,
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
};
static struct perf_event_attr l1dc_event_attr = {
	.type           = PERF_TYPE_RAW,
/*	.config         = 0x04, */
	.config         = ARMV8_PMUV3_PERFCTR_L1D_CACHE, /* 0x04 */
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
};
static struct perf_event_attr l1dc_ref_event_attr = {
	.type           = PERF_TYPE_RAW,
/*	.config         = 0x03, */
	.config         = ARMV8_PMUV3_PERFCTR_L1D_CACHE_REFILL, /* 0x03 */
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
};
static struct perf_event_attr l2dc_event_attr = {
	.type           = PERF_TYPE_RAW,
/*	.config         = 0x16, */
	.config         = ARMV8_PMUV3_PERFCTR_L2D_CACHE, /* 0x16 */
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
};
static struct perf_event_attr l2dc_ref_event_attr = {
	.type           = PERF_TYPE_RAW,
/*	.config         = 0x17, */
	.config         = ARMV8_PMUV3_PERFCTR_L2D_CACHE_REFILL, /* 0x17 */
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
};
static struct perf_event_attr l3dc_event_attr = {
	.type           = PERF_TYPE_RAW,
/*	.config         = 0x2B, */
	.config         = ARMV8_PMUV3_PERFCTR_L3D_CACHE, /* 0x2B */
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
};
static struct perf_event_attr l3dc_ref_event_attr = {
	.type           = PERF_TYPE_RAW,
/*	.config         = 0x2A, */
	.config         = ARMV8_PMUV3_PERFCTR_L3D_CACHE_REFILL, /* 0x2A */
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
};

static void mbraink_perf_start(int cpu)
{
	struct perf_event *i_event = per_cpu(inst_spec_events, cpu);
	struct perf_event *c_event = per_cpu(cycle_events, cpu);
	struct perf_event *l1_event = per_cpu(l1dc_events, cpu);
	struct perf_event *l1r_event = per_cpu(l1dc_ref_events, cpu);
	struct perf_event *l2_event = per_cpu(l2dc_events, cpu);
	struct perf_event *l2r_event = per_cpu(l2dc_ref_events, cpu);
	struct perf_event *l3_event = per_cpu(l3dc_events, cpu);
	struct perf_event *l3r_event = per_cpu(l3dc_ref_events, cpu);

	if (i_event)
		perf_event_enable(i_event);
	if (c_event)
		perf_event_enable(c_event);
	if (l1_event)
		perf_event_enable(l1_event);
	if (l1r_event)
		perf_event_enable(l1r_event);
	if (l2_event)
		perf_event_enable(l2_event);
	if (l2r_event)
		perf_event_enable(l2r_event);
	if (l3_event)
		perf_event_enable(l3_event);
	if (l3r_event)
		perf_event_enable(l3r_event);
}

static void mbraink_perf_stop(int cpu)
{
	struct perf_event *i_event = per_cpu(inst_spec_events, cpu);
	struct perf_event *c_event = per_cpu(cycle_events, cpu);
	struct perf_event *l1_event = per_cpu(l1dc_events, cpu);
	struct perf_event *l1r_event = per_cpu(l1dc_ref_events, cpu);
	struct perf_event *l2_event = per_cpu(l2dc_events, cpu);
	struct perf_event *l2r_event = per_cpu(l2dc_ref_events, cpu);
	struct perf_event *l3_event = per_cpu(l3dc_events, cpu);
	struct perf_event *l3r_event = per_cpu(l3dc_ref_events, cpu);

	if (i_event)
		perf_event_disable(i_event);
	if (c_event)
		perf_event_disable(c_event);
	if (l1_event)
		perf_event_disable(l1_event);
	if (l1r_event)
		perf_event_disable(l1r_event);
	if (l2_event)
		perf_event_disable(l2_event);
	if (l2r_event)
		perf_event_disable(l2r_event);
	if (l3_event)
		perf_event_disable(l3_event);
	if (l3r_event)
		perf_event_disable(l3r_event);
}

static int mbraink_perf_probe_cpu_enable(int cpu, int enable)
{
	struct perf_event *event = NULL;
	struct perf_event *i_event = per_cpu(inst_spec_events, cpu);
	struct perf_event *c_event = per_cpu(cycle_events, cpu);
	struct perf_event *l1_event = per_cpu(l1dc_events, cpu);
	struct perf_event *l1r_event = per_cpu(l1dc_ref_events, cpu);
	struct perf_event *l2_event = per_cpu(l2dc_events, cpu);
	struct perf_event *l2r_event = per_cpu(l2dc_ref_events, cpu);
	struct perf_event *l3_event = per_cpu(l3dc_events, cpu);
	struct perf_event *l3r_event = per_cpu(l3dc_ref_events, cpu);
	int ret = 0;

	if (enable) {
		if (!i_event) {
			event = perf_event_create_kernel_counter(
				&inst_spec_event_attr, cpu, NULL, NULL, NULL);
			if (IS_ERR(event)) {
				pr_notice("create (%d) inst_spec error (%d)\n", cpu, (int)PTR_ERR(event));
				goto FAIL;
			}
			per_cpu(inst_spec_events, cpu) = event;
		}
		if (!c_event) {
			event = perf_event_create_kernel_counter(
				&cycle_event_attr, cpu, NULL, NULL, NULL);
			if (IS_ERR(event)) {
				pr_notice("create (%d) cycle error (%d)\n", cpu, (int)PTR_ERR(event));
				goto FAIL;
			}
			per_cpu(cycle_events, cpu) = event;
		}
		if (!l1_event) {
			event = perf_event_create_kernel_counter(
				&l1dc_event_attr, cpu, NULL, NULL, NULL);
			if (IS_ERR(event)) {
				pr_notice("create (%d) l1dc counter error (%d)\n", cpu, (int)PTR_ERR(event));
				goto FAIL;
			}
			per_cpu(l1dc_events, cpu) = event;
		}
		if (!l1r_event) {
			event = perf_event_create_kernel_counter(
				&l1dc_ref_event_attr, cpu, NULL, NULL, NULL);
			if (IS_ERR(event)) {
				pr_notice("create (%d) l1dc refill counter error (%d)\n", cpu, (int)PTR_ERR(event));
				goto FAIL;
			}
			per_cpu(l1dc_ref_events, cpu) = event;
		}
		if (!l2_event) {
			event = perf_event_create_kernel_counter(
				&l2dc_event_attr, cpu, NULL, NULL, NULL);
			if (IS_ERR(event)) {
				pr_notice("create (%d) l2dc counter error (%d)\n", cpu, (int)PTR_ERR(event));
				goto FAIL;
			}
			per_cpu(l2dc_events, cpu) = event;
		}
		if (!l2r_event) {
			event = perf_event_create_kernel_counter(
				&l2dc_ref_event_attr, cpu, NULL, NULL, NULL);
			if (IS_ERR(event)) {
				pr_notice("create (%d) l2dc refill counter error (%d)\n", cpu, (int)PTR_ERR(event));
				goto FAIL;
			}
			per_cpu(l2dc_ref_events, cpu) = event;
		}
		if (!l3_event) {
			event = perf_event_create_kernel_counter(
				&l3dc_event_attr, cpu, NULL, NULL, NULL);
			if (IS_ERR(event)) {
				pr_notice("create (%d) l3dc counter error (%d)\n", cpu, (int)PTR_ERR(event));
				goto FAIL;
			}
			per_cpu(l3dc_events, cpu) = event;
		}
		if (!l3r_event) {
			event = perf_event_create_kernel_counter(
				&l3dc_ref_event_attr, cpu, NULL, NULL, NULL);
			if (IS_ERR(event)) {
				pr_notice("create (%d) l3dc refill counter error (%d)\n", cpu, (int)PTR_ERR(event));
				goto FAIL;
			}
			per_cpu(l3dc_ref_events, cpu) = event;
		}
		mbraink_perf_start(cpu);
	} else {
		mbraink_perf_stop(cpu);
		if (i_event) {
			perf_event_release_kernel(i_event);
			per_cpu(inst_spec_events, cpu) = NULL;
		}
		if (c_event) {
			perf_event_release_kernel(c_event);
			per_cpu(cycle_events, cpu) = NULL;
		}
		if (l1_event) {
			perf_event_release_kernel(l1_event);
			per_cpu(l1dc_events, cpu) = NULL;
		}
		if (l1r_event) {
			perf_event_release_kernel(l1r_event);
			per_cpu(l1dc_ref_events, cpu) = NULL;
		}
		if (l2_event) {
			perf_event_release_kernel(l2_event);
			per_cpu(l2dc_events, cpu) = NULL;
		}
		if (l2r_event) {
			perf_event_release_kernel(l2r_event);
			per_cpu(l2dc_ref_events, cpu) = NULL;
		}
		if (l3_event) {
			perf_event_release_kernel(l3_event);
			per_cpu(l3dc_events, cpu) = NULL;
		}
		if (l3r_event) {
			perf_event_release_kernel(l3r_event);
			per_cpu(l3dc_ref_events, cpu) = NULL;
		}
	}
FAIL:
	return ret;
}

int mbraink_perf_probe_enable(int enable)
{
	int i, ret = 0;

	for (i = 0; i < num_possible_cpus(); i++)
		ret |= mbraink_perf_probe_cpu_enable(i, !!enable);

	return ret;
}
EXPORT_SYMBOL(mbraink_perf_probe_enable);

unsigned long long mbraink_perf_pmu_get_count(unsigned int evt_id, unsigned int cpu)
{
	struct perf_event *event = NULL;
	unsigned long long val = 0;

	if (cpu >= num_possible_cpus())
		return 0;

	switch (evt_id) {
	case MBK_INST_SPEC_EVT:
		event = per_cpu(inst_spec_events, cpu);
		break;
	case MBK_CYCLES_EVT:
		event = per_cpu(cycle_events, cpu);
		break;
	case MBK_L1DC_EVT:
		event = per_cpu(l1dc_events, cpu);
		break;
	case MBK_L1DC_ERF_EVT:
		event = per_cpu(l1dc_ref_events, cpu);
		break;
	case MBK_L2DC_EVT:
		event = per_cpu(l2dc_events, cpu);
		break;
	case MBK_L2DC_ERF_EVT:
		event = per_cpu(l2dc_ref_events, cpu);
		break;
	case MBK_L3DC_EVT:
		event = per_cpu(l3dc_events, cpu);
		break;
	case MBK_L3DC_ERF_EVT:
		event = per_cpu(l3dc_ref_events, cpu);
		break;
	default:
		return 0;
	};

	if (event && event->state == PERF_EVENT_STATE_ACTIVE && cpu_online(cpu))
		perf_event_read_local(event, &val, NULL, NULL);

	return val;
}
EXPORT_SYMBOL(mbraink_perf_pmu_get_count);

int mbraink_perf_init(void)
{
	int ret = 0;

	ret = mbraink_perf_probe_enable(1);
	return ret;
}

void __exit mbraink_perf_exit(void)
{
	mbraink_perf_probe_enable(0);
}

module_init(mbraink_perf_init);
module_exit(mbraink_perf_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("<yu-jui.lin@mediatek.com>");
MODULE_DESCRIPTION("MBrainK Perf Driver");
MODULE_VERSION("1.0");
