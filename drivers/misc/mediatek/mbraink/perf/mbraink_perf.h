/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef MBRAINK_PERF_H
#define MBRAINK_PERF_H

enum mbraink_perf_evt_id {
	MBK_INST_SPEC_EVT,
	MBK_CYCLES_EVT,
	MBK_L1DC_EVT,
	MBK_L1DC_ERF_EVT,
	MBK_L2DC_EVT,
	MBK_L2DC_ERF_EVT,
	MBK_L3DC_EVT,
	MBK_L3DC_ERF_EVT,
};

extern int mbraink_perf_probe_enable(int enable);
extern unsigned long long mbraink_perf_pmu_get_count(unsigned int evt_id, unsigned int cpu);

#endif /*end of MBRAINK_PERF_H*/
