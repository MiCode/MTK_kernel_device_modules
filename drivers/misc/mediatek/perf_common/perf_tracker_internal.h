/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef _PERF_TRACKER_INTERNAL_H
#define _PERF_TRACKER_INTERNAL_H

#include <linux/types.h>
#include <linux/module.h>
#include <linux/cpufreq.h>

#define MAX_CLUSTER_NR  3

/* get OPP table */
struct ppm_data {
	struct cpufreq_frequency_table *dvfs_tbl;
	unsigned int opp_nr;
	bool init;
};

extern void __iomem *csram_base;
extern void __iomem *u_tcm_base;
extern void __iomem *pmu_tcm_base;

extern struct ppm_data cluster_ppm_info[MAX_CLUSTER_NR];
extern int cluster_nr;

#if IS_ENABLED(CONFIG_MTK_PERF_TRACKER)
/* copy from cpu_swpm_internal.h */
extern u32 CPU_L3DC_OFFSET;
extern u32 CPU_INST_SPEC_OFFSET;
extern u32 CPU_IDX_CYCLES_OFFSET;
extern u32 PERF_TRACKER_STATUS_OFFSET;
/*u a e*/
extern u32 U_AFFO;
extern u32 U_BMONIO;
extern u32 U_UFFO;
extern u32 U_UCFO;

/*
 *cluster's volt: start from 0x514
 *cluster's freq: start from 0x11e0
 */
/*2 cluster: B Vproc, >L Vproc, B Vsram, L Vsram */
extern u32 U_VOLT_2_CLUSTER;
/*2 cluster: L, B, >U */
extern u32 U_FREQ_2_CLUSTER;
/*3 cluster: B Vproc, M Vproc, >L Vproc, B Vsram, M Vsram, L Vsram */
extern u32 U_VOLT_3_CLUSTER;
/*3 cluster: L, M, B, >U */
extern u32 U_FREQ_3_CLUSTER;
extern u32 MCUPM_OFFSET_BASE;

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_TRACER)
#include <mt-plat/mtk_blocktag.h>
#else
struct mtk_btag_mictx_iostat_struct {
	__u64 duration;  /* duration time for below performance data (ns) */
	__u32 tp_req_r;  /* throughput (per-request): read  (KB/s) */
	__u32 tp_req_w;  /* throughput (per-request): write (KB/s) */
	__u32 tp_all_r;  /* throughput (overlapped) : read  (KB/s) */
	__u32 tp_all_w;  /* throughput (overlapped) : write (KB/s) */
	__u32 reqsize_r; /* request size : read  (Bytes) */
	__u32 reqsize_w; /* request size : write (Bytes) */
	__u32 reqcnt_r;  /* request count: read */
	__u32 reqcnt_w;  /* request count: write */
	__u16 wl;        /* storage device workload (%) */
	__u16 q_depth;   /* storage cmdq queue depth */
};
#endif
extern struct kobj_attribute perf_tracker_enable_attr;

extern void perf_tracker(u64 wallclock,
			 bool hit_long_check);
extern u64 get_cpu_pmu(int cpu, u32 offset);
extern bool perf_tracker_info_exist;
extern bool is_perf_tracker_info_exist(void);
extern u32 get_perf_tracker_info_from_dts(const char *property_name);

extern struct kobj_attribute perf_fuel_gauge_enable_attr;
extern struct kobj_attribute perf_fuel_gauge_period_attr;
#if IS_ENABLED(CONFIG_MTK_CHARGER)
extern struct kobj_attribute perf_charger_enable_attr;
extern struct kobj_attribute perf_charger_period_attr;
#endif
#if IS_ENABLED(CONFIG_MTK_GPU_SWPM_SUPPORT)
extern struct kobj_attribute perf_gpu_pmu_enable_attr;
extern struct kobj_attribute perf_gpu_pmu_period_attr;
extern void mtk_ltr_gpu_pmu_start(unsigned int interval_ns);
extern void mtk_ltr_gpu_pmu_stop(void);
#endif
/* perf_freq_tracker hook */
extern int insert_freq_qos_hook(void);
extern void remove_freq_qos_hook(void);
extern void init_perf_freq_tracker(void);
extern void exit_perf_freq_tracker(void);
extern struct kobj_attribute perf_mcupm_freq_enable_attr;
extern struct kobj_attribute perf_cpu_pmu_enable_attr;
#else
static inline void perf_tracker(u64 wallclock,
				bool hit_long_check) {}
#endif /* CONFIG_MTK_PERF_TRACKER */
#endif /* _PERF_TRACKER_INTERNAL_H */
