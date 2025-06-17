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
	AISTE_INIT,
	AISTE_PERFORMANCE_L1_ON,
	AISTE_PERFORMANCE_L2_ON,
	AISTE_PERFORMANCE_L3_ON,
	AISTE_PERFORMANCE_OFF,
	AISTE_MAX,
};

struct DdrBoostConfig {
	int threshold;
	int performance_level;
};

void aiste_scmi_init(unsigned int g_aiste_addr, unsigned int g_aiste_size);
int aiste_scmi_set(uint16_t ddr_boost_val);

#endif /* __AISTE_SCMI_H__ */
