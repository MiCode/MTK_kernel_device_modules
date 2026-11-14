/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#ifndef _MTK_CPU_POWER_THROTTLING_H_
#define _MTK_CPU_POWER_THROTTLING_H_
#define PT_OP_SET_MPMM    0

enum cpu_pt_type {
	LBAT_POWER_THROTTLING,
	OC_POWER_THROTTLING,
	SOC_POWER_THROTTLING,
	POWER_THROTTLING_TYPE_MAX
};

struct cpu_pt_policy {
	enum cpu_pt_type           pt_type;
	unsigned int               pt_max_lv;
	unsigned int               cpu;
	s32                        *freq_limit;
	struct freq_qos_request    qos_req;
	struct cpufreq_policy      *policy;
	struct list_head           cpu_pt_list;
};

struct cpu_bootup_pt_policy {
	unsigned int               pt_max_lv;
	unsigned int               cpu;
	s32                        *freq_limit_booting;
	struct freq_qos_request    qos_req;
	struct cpufreq_policy      *policy;
	struct list_head           cpu_bootup_pt_list;
};

typedef int (*cpu_isolate_cb)(unsigned int cpu, bool is_pause, unsigned int request_mask);
extern int register_pt_isolate_cb(cpu_isolate_cb cb_func);

#endif
