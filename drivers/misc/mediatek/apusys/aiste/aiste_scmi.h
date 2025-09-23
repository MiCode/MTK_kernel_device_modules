/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __AISTE_SCMI_H__
#define __AISTE_SCMI_H__

#define NONE DEL_NONE
#include <linux/scmi_protocol.h>
#undef NONE
#include "tinysys-scmi.h"

enum {
	AISTE_INIT = 0,
	AISTE_OFF,
	AISTE_Power_Saving,
	AISTE_Perf_L1,
	AISTE_Bench_Perf,
	AISTE_LLM_Qwen_25_3B_Perf,
	AISTE_Perf_L2,
	AISTE_Perf_L3,
	AISTE_Low_Power_High,
	AISTE_Low_Power,
	AISTE_MAX,
};

struct DdrBoostConfig {
	int threshold;
	int performance_level;
};

void aiste_scmi_init(unsigned int g_aiste_addr, unsigned int g_aiste_size);
int aiste_scmi_set(uint16_t ddr_boost_val);

#endif /* __AISTE_SCMI_H__ */
