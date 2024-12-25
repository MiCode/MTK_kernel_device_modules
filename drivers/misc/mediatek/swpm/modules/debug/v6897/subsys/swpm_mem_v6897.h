/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __SWPM_MEM_V6897_H__
#define __SWPM_MEM_V6897_H__

#define MAX_APHY_OTHERS_PWR			(16)
#define NR_DRAM_PWR_SAMPLE			(3)
/* sync emi in sspm */
#define MAX_EMI_NUM				(2)

enum ddr_freq {
	DDR_400,
	DDR_800,
	DDR_933,
	DDR_1066,
	DDR_1547,
	DDR_2133,
	DDR_2750,
	DDR_3200,
	DDR_3750,
	DDR_4266,

	NR_DDR_FREQ
};

/* dram voltage/freq index */
struct mem_swpm_vf_index {
	unsigned int ddr_freq_mhz;
	unsigned int ddr_cur_opp;
};

/* dram power index structure */
struct mem_swpm_index {
	unsigned int read_bw[MAX_EMI_NUM];
	unsigned int write_bw[MAX_EMI_NUM];
	unsigned int srr_pct;			/* (s1) self refresh rate */
	unsigned int ssr_pct;			/* (s0) sleep rate */
	unsigned int pdir_pct[MAX_EMI_NUM];	/* power-down idle rate */
	unsigned int phr_pct[MAX_EMI_NUM];	/* page-hit rate */
	unsigned int acc_util[MAX_EMI_NUM];	/* accumulate EMI utilization */
	unsigned int trans[MAX_EMI_NUM];	/* transaction count */
	unsigned int mr4;
	struct mem_swpm_vf_index vf;
};

struct mem_swpm_data {
	unsigned int read_bw[MAX_EMI_NUM];
	unsigned int write_bw[MAX_EMI_NUM];
	unsigned int ddr_opp_freq[NR_DDR_FREQ];
};

enum aphy_other_pwr_type {
	/* APHY_VCORE, independent */
	APHY_VDDQ,
	APHY_VM,
	APHY_VIO12,
	/* APHY_VIO_1P8V, */

	NR_APHY_OTHERS_PWR_TYPE
};

enum dram_pwr_type {
	DRAM_VDD1,
	DRAM_VDD2H,
	DRAM_VDD2L,
	DRAM_VDDQ,

	NR_DRAM_PWR_TYPE
};

extern spinlock_t mem_swpm_spinlock;
extern struct mem_swpm_data mem_idx_snap;
extern int swpm_mem_v6897_init(void);
extern void swpm_mem_v6897_exit(void);

#endif

