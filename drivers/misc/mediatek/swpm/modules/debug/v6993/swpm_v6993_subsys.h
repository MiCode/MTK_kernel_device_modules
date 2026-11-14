/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __SWPM_V6993_SUBSYS_H__
#define __SWPM_V6993_SUBSYS_H__

#define MAX_RECORD_CNT				(64)
#define DEFAULT_AVG_WINDOW		(50)
#define ALL_SWPM_TYPE				(0xFFFF)

#define for_each_pwr_mtr(i)    for (i = 0; i < NR_SWPM_TYPE; i++)

enum swpm_type {
	CPU_SWPM_TYPE,
	GPU_SWPM_TYPE,
	CORE_SWPM_TYPE,
	MEM_SWPM_TYPE,
	ISP_SWPM_TYPE,
	ME_SWPM_TYPE,
	APU_SWPM_TYPE,
	AUDIO_SWPM_TYPE,
	TSFDC_SWPM_TYPE,
	DISP_SWPM_TYPE,
	USB_SWPM_TYPE,
	PCIE_SWPM_TYPE,
	VDEC_SWPM_TYPE,
	UFS_SWPM_TYPE,
	SSPM_SWPM_TYPE,
	SCP_SWPM_TYPE,
	DUMMY_SWPM_TYPE,
	INFRA_SWPM_TYPE,
	CHINFRA_SWPM_TYPE,
	VCP_SWPM_TYPE,
	MML_SWPM_TYPE,
	MMINFRA_SWPM_TYPE,
	CLKMGR_SWPM_TYPE,
	VENC_SWPM_TYPE,
	ADSP_SWPM_TYPE,
	NR_SWPM_TYPE,
};

struct subsys_swpm_data {
	unsigned int mem_swpm_data_addr;
	unsigned int core_swpm_data_addr;
};

struct share_sub_index_ext {
};

extern void swpm_v6993_sub_ext_update(void);
extern void swpm_v6993_sub_ext_init(void);

#endif
