/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __SWPM_CORE_V6897_H__
#define __SWPM_CORE_V6897_H__

#include <swpm_mem_v6897.h>

#define MAX_APHY_CORE_PWR			(12)
#define NR_CORE_VOLT				(6)

/* infra power state for core power */
enum infra_power_state {
	INFRA_DATA_ACTIVE,
	INFRA_CMD_ACTIVE,
	INFRA_IDLE,
	INFRA_DCM,

	NR_INFRA_POWER_STATE
};

enum core_static_type {
	CORE_STATIC_MMINFRA, /* 4% */
	CORE_STATIC_MDPSYS,  /* 5% */
	CORE_STATIC_VDEC,    /* 6% */
	CORE_STATIC_MMSYS,   /* 7% */
	CORE_STATIC_VEND,    /* 9% */
	CORE_STATIC_DRAMC,   /* 10% */
	CORE_STATIC_TOP,     /* 11% */
	CORE_STATIC_INFRA,   /* 23% */
	CORE_STATIC_CONNSYS, /* 25% */

	NR_CORE_STATIC_TYPE
};

enum core_static_rec_type {
	CORE_STATIC_REC_INFRA,
	CORE_STATIC_REC_DRAMC,
	NR_CORE_STATIC_REC_TYPE
};

/* core voltage/freq index */
struct core_swpm_vf_index {
	unsigned int vcore_mv;
	unsigned int ddr_freq_mhz;
	unsigned int vcore_cur_opp;
	unsigned int ddr_cur_opp;
};

/* core power index structure */
struct core_swpm_index {
	unsigned int infra_state_ratio[NR_INFRA_POWER_STATE];
	unsigned int read_bw[MAX_EMI_NUM];
	unsigned int write_bw[MAX_EMI_NUM];
	unsigned int srr_pct;
	unsigned int ssr_pct;
	struct core_swpm_vf_index vf;
};

struct core_swpm_data {
	unsigned int core_volt_tbl[NR_CORE_VOLT];
	unsigned int core_static_pwr[NR_CORE_VOLT][NR_CORE_STATIC_TYPE];
	unsigned int thermal;
};

extern unsigned int swpm_core_static_data_get(void);
extern void swpm_core_static_replaced_data_set(unsigned int data);
extern void swpm_core_static_data_init(void);
extern int swpm_core_v6897_init(void);
extern void swpm_core_v6897_exit(void);

#endif

