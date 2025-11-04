/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2025 MediaTek Inc.
 */

#ifndef __DEVAPC_MT6881_H__
#define __DEVAPC_MT6881_H__

#include "devapc-mtk-multi-ao.h"

/******************************************************************************
 * VARIABLE DEFINITION
 ******************************************************************************/
/* dbg status default setting */
#define PLAT_DBG_UT_DEFAULT		false
#define PLAT_DBG_KE_DEFAULT		true
#define PLAT_DBG_AEE_DEFAULT		true
#define PLAT_DBG_WARN_DEFAULT		true
#define PLAT_DBG_DAPC_DEFAULT		false

/* devapc status default setting */
#define ENABLE_DEVAPC_ADSP		0

/******************************************************************************
 * STRUCTURE DEFINITION
 ******************************************************************************/
enum DEVAPC_SLAVE_TYPE {
	SLAVE_TYPE_INFRA = 0,
	SLAVE_TYPE_INFRA1,
	SLAVE_TYPE_PERI_PAR,
	SLAVE_TYPE_VLP,
	SLAVE_TYPE_ADSP,
	SLAVE_TYPE_MMINFRA,
	SLAVE_TYPE_MMUP,
	SLAVE_TYPE_GPU,
	SLAVE_TYPE_GPU1,
	SLAVE_TYPE_NUM,
};

enum DEVAPC_VIO_MASK_STA_NUM {
	VIO_MASK_STA_NUM_INFRA = 8,
	VIO_MASK_STA_NUM_INFRA1 = 10,
	VIO_MASK_STA_NUM_PERI_PAR = 4,
	VIO_MASK_STA_NUM_VLP = 4,
	VIO_MASK_STA_NUM_ADSP = 0,
	VIO_MASK_STA_NUM_MMINFRA = 18,
	VIO_MASK_STA_NUM_MMUP = 2,
	VIO_MASK_STA_NUM_GPU = 1,
	VIO_MASK_STA_NUM_GPU1 = 1,
};

enum DEVAPC_PD_OFFSET {
	PD_VIO_MASK_OFFSET = 0x0,
	PD_VIO_STA_OFFSET = 0x400,
	PD_VIO_DBG0_OFFSET = 0x900,
	PD_VIO_DBG1_OFFSET = 0x904,
	PD_VIO_DBG2_OFFSET = 0x908,
	PD_APC_CON_OFFSET = 0xF00,
	PD_SHIFT_STA_OFFSET = 0xF20,
	PD_SHIFT_SEL_OFFSET = 0xF30,
	PD_SHIFT_CON_OFFSET = 0xF10,
	PD_VIO_DBG3_OFFSET = 0x90C,
};

#define SRAMROM_SLAVE_TYPE	SLAVE_TYPE_INFRA	/* Infra */
#define MM2ND_SLAVE_TYPE	SLAVE_TYPE_NUM		/* No MM2ND */

enum OTHER_TYPES_INDEX {
	SRAMROM_VIO_INDEX = 237,
	CONN_VIO_INDEX = 0,
	MDP_VIO_INDEX = 0,
	DISP2_VIO_INDEX = 0,
	MMSYS_VIO_INDEX = 0,
};

enum INFRACFG_MM2ND_VIO_NUM {
	INFRACFG_MM_VIO_STA_NUM = 4,
	INFRACFG_MDP_VIO_STA_NUM = 10,
	INFRACFG_DISP2_VIO_STA_NUM = 4,
};

enum INFRACFG_MM2ND_OFFSET {
	INFRACFG_MM_SEC_VIO0_OFFSET = 0xB30,
	INFRACFG_MDP_SEC_VIO0_OFFSET = 0xB40,
	INFRACFG_DISP2_SEC_VIO0_OFFSET = 0xB70,
};

enum BUSID_LENGTH {
	INFRAAXI_MI_BIT_LENGTH = 16,
	ADSPAXI_MI_BIT_LENGTH = 8,
	MMINFRAAXI_MI_BIT_LENGTH = 19,
};

struct INFRAAXI_ID_INFO {
	const char	*master;
	uint8_t		bit[INFRAAXI_MI_BIT_LENGTH];
};

struct ADSPAXI_ID_INFO {
	const char	*master;
	uint8_t		bit[ADSPAXI_MI_BIT_LENGTH];
};

struct MMINFRAAXI_ID_INFO {
	const char	*master;
	uint8_t		bit[MMINFRAAXI_MI_BIT_LENGTH];
};

enum DEVAPC_IRQ_TYPE {
	IRQ_TYPE_INFRA = 0,
	IRQ_TYPE_PERI,
	IRQ_TYPE_VLP,
	IRQ_TYPE_ADSP,
	IRQ_TYPE_MMINFRA,
	IRQ_TYPE_MMUP,
	IRQ_TYPE_GPU,
	IRQ_TYPE_NUM,
};

enum ADSP_MI_SELECT {
	ADSP_MI12 = 0,
	ADSP_MI17
};

/******************************************************************************
 * PLATFORM DEFINATION
 ******************************************************************************/

/* For Infra VIO_DBG */
#define INFRA_VIO_DBG_MSTID			0xFFFFFFFF
#define INFRA_VIO_DBG_MSTID_START_BIT		0
#define INFRA_VIO_DBG_DMNID			0x0000003F
#define INFRA_VIO_DBG_DMNID_START_BIT		0
#define INFRA_VIO_DBG_W_VIO			0x00000040
#define INFRA_VIO_DBG_W_VIO_START_BIT		6
#define INFRA_VIO_DBG_R_VIO			0x00000080
#define INFRA_VIO_DBG_R_VIO_START_BIT		7
#define INFRA_VIO_ADDR_HIGH			0xFFFFFFFF
#define INFRA_VIO_ADDR_HIGH_START_BIT		0

/* For SRAMROM VIO */
#define SRAMROM_SEC_VIO_ID_MASK			0x000FFFFF
#define SRAMROM_SEC_VIO_ID_SHIFT		0
#define SRAMROM_SEC_VIO_DOMAIN_MASK		0x0F000000
#define SRAMROM_SEC_VIO_DOMAIN_SHIFT		24
#define SRAMROM_SEC_VIO_RW_MASK			0x80000000
#define SRAMROM_SEC_VIO_RW_SHIFT		31

/* For MM 2nd VIO */
#define INFRACFG_MM2ND_VIO_DOMAIN_MASK		0x00000030
#define INFRACFG_MM2ND_VIO_DOMAIN_SHIFT		4
#define INFRACFG_MM2ND_VIO_ID_MASK		0x007FFF00
#define INFRACFG_MM2ND_VIO_ID_SHIFT		8
#define INFRACFG_MM2ND_VIO_RW_MASK		0x01000000
#define INFRACFG_MM2ND_VIO_RW_SHIFT		24

#define SRAM_START_ADDR				(0x100000)
#define SRAM_END_ADDR				(0x1FFFFF)

/* For VLP Bus Parser */
#define VLP_SCP_1_START				(0x1C400000)
#define VLP_SCP_1_END				(0x1CB9FFFF)
#define VLP_SCP_2_START				(0x1CCA0000)
#define VLP_SCP_2_END				(0x1CCFFFFF)
#define VLP_INFRA_START				(0x00000000)
#define VLP_INFRA_END				(0x1BFFFFFF)
#define VLP_INFRA_1_START			(0x1D000000)
#define VLP_INFRA_1_END				(0x1FFFFFFF)
#define VLP_INFRA_2_START			(0x20000000)
#define VLP_INFRA_2_END				(0x63FFFFFFF)

/* For MMINFRA Bus Parser */
#define IMG_START_ADDR				(0x15000000)
#define IMG_END_ADDR				(0x157FFFFF)
#define CAM_START_ADDR				(0x1a000000)
#define CAM_END_ADDR				(0x1BFFFFFF)
#define CODEC_START_ADDR			(0x16000000)
#define CODEC_END_ADDR				(0x17FFFFFF)
#define DISP_START_ADDR				(0x14000000)
#define DISP_END_ADDR				(0x143FFFFF)
#define MML_START_ADDR				(0x1F000000)
#define MML_END_ADDR				(0x1F7FFFFF)

/* For GPU Bus Parser */
#define GPU_PD_START				(0xC00000)
#define GPU_PD_END				(0xC620FF)

static const struct mtk_device_info mt6881_devices_infra[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "APMIXEDSYS_APB_S", true},
	{0, 1, 1, "APMIXEDSYS_APB_S-1", true},
	{0, 2, 2, "RESERVED_APB_S", true},
	{0, 3, 3, "TOPCKGEN_APB_S", true},
	{0, 4, 4, "INFRACFG_AO_APB_S", true},
	{0, 5, 5, "GPIO_APB_S", true},
	{0, 6, 6, "DEVICE_APC_INFRA_AO_APB_S", true},
	{0, 7, 7, "BCRM_INFRA_AO_APB_S", true},
	{0, 8, 8, "DEBUG_CTRL_INFRA_AO_APB_S", true},
	{0, 9, 10, "TOP_MISC_APB_S", true},

	/* 10 */
	{0, 10, 11, "MBIST_AO_APB_S", true},
	{0, 11, 12, "DPMAIF_AO_APB_S", true},
	{0, 12, 13, "INFRA_SECURITY_AO_APB_S", true},
	{0, 13, 14, "TOPCKGEN_INFRA_CFG_APB_S", true},
	{0, 14, 15, "DRM_DEBUG_TOP_APB_S", true},
	{0, 15, 16, "SYS_CIRQ_APB_S", true},
	{0, 16, 17, "EFUSE_DEBUG_AO_INFRA_APB_S", true},
	{0, 17, 18, "PMSR_APB_S", true},
	{0, 18, 19, "INFRA2PERI_S", true},
	{0, 19, 20, "INFRA2PERI_S-1", true},

	/* 20 */
	{0, 20, 21, "INFRA2PERI_S-2", true},
	{0, 21, 22, "APU_S_S", true},
	{0, 22, 23, "APU_S_S-1", true},
	{0, 23, 72, "INFRA2MM_S", true},
	{0, 24, 73, "INFRA2MM_S-1", true},
	{0, 25, 74, "INFRA2MM_S-2", true},
	{0, 26, 75, "INFRA2MM_S-3", true},
	{0, 27, 76, "L3C_S", true},
	{0, 28, 77, "L3C_S-1", true},
	{0, 29, 78, "L3C_S-2", true},

	/* 30 */
	{0, 30, 79, "L3C_S-3", true},
	{0, 31, 80, "L3C_S-4", true},
	{0, 32, 81, "L3C_S-5", true},
	{0, 33, 82, "L3C_S-6", true},
	{0, 34, 83, "L3C_S-7", true},
	{0, 35, 84, "L3C_S-8", true},
	{0, 36, 85, "L3C_S-9", true},
	{0, 37, 86, "L3C_S-10", true},
	{0, 38, 87, "L3C_S-11", true},
	{0, 39, 88, "L3C_S-12", true},

	/* 40 */
	{0, 40, 89, "L3C_S-13", true},
	{0, 41, 90, "L3C_S-14", true},
	{0, 42, 91, "L3C_S-15", true},
	{0, 43, 92, "L3C_S-16", true},
	{0, 44, 93, "L3C_S-17", true},
	{0, 45, 94, "L3C_S-18", true},
	{0, 46, 95, "L3C_S-19", true},
	{0, 47, 96, "L3C_S-20", true},
	{0, 48, 97, "L3C_S-21", true},
	{0, 49, 98, "L3C_S-22", true},

	/* 50 */
	{0, 50, 99, "L3C_S-23", true},
	{0, 51, 100, "L3C_S-24", true},
	{0, 52, 101, "L3C_S-25", true},
	{0, 53, 102, "L3C_S-26", true},
	{0, 54, 103, "L3C_S-27", true},
	{0, 55, 104, "L3C_S-28", true},
	{0, 56, 105, "L3C_S-29", true},
	{0, 57, 106, "L3C_S-30", true},
	{0, 58, 110, "MFG_S_S", true},
	{0, 59, 113, "VLPSYS_S", true},

	/* 60 */
	{0, 60, 116, "CONN_S", true},
	{0, 61, 117, "SSR_APB_S", true},
	{0, 62, 119, "DEBUGSYS_APB_S", true},
	{0, 63, 120, "DPMAIF_PDN_APB_S", true},
	{0, 64, 121, "DPMAIF_PDN_APB_S-1", true},
	{0, 65, 122, "DPMAIF_PDN_APB_S-2", true},
	{0, 66, 123, "DPMAIF_PDN_APB_S-3", true},
	{0, 67, 124, "FDXCC_FAKE_ENG_APB_S", true},
	{0, 68, 125, "DEVICE_APC_INFRA_PDN_APB_S", true},
	{0, 69, 126, "DEBUG_TRACKER_APB_S", true},

	/* 70 */
	{0, 70, 127, "DEBUG_TRACKER_APB1_S", true},
	{0, 71, 128, "BCRM_INFRA_PDN_APB_S", true},
	{0, 72, 129, "IFR_MPU_APB_S", true},
	{0, 73, 130, "FMCU_FAKE_ENG_APB_S", true},
	{0, 74, 131, "PTP_THERM_CTRL_APB_S", true},
	{0, 75, 132, "PTP_THERM_CTRL2_APB_S", true},
	{0, 76, 133, "CCIF0_AP_APB_S", true},
	{0, 77, 134, "CCIF0_MD_APB_S", true},
	{0, 78, 135, "CCIF1_AP_APB_S", true},
	{0, 79, 136, "CCIF1_MD_APB_S", true},

	/* 80 */
	{0, 80, 137, "MBIST_PDN_APB_S", true},
	{0, 81, 138, "INFRACFG_PDN_APB_S", true},
	{0, 82, 139, "TRNG_APB_S", true},
	{0, 83, 140, "DX_CC_APB_S", true},
	{0, 84, 141, "CQ_DMA_APB_S", true},
	{0, 85, 142, "SRAMROM_APB_S", true},
	{0, 86, 143, "RESERVED_DVFS_PROC_APB_S", true},
	{0, 87, 145, "SYS_CIRQ2_APB_S", true},
	{0, 88, 146, "CCIF2_AP_APB_S", true},
	{0, 89, 147, "CCIF2_MD_APB_S", true},

	/* 90 */
	{0, 90, 148, "CCIF3_AP_APB_S", true},
	{0, 91, 149, "CCIF3_MD_APB_S", true},
	{0, 92, 150, "CCIF4_AP_APB_S", true},
	{0, 93, 151, "CCIF4_MD_APB_S", true},
	{0, 94, 152, "CCIF5_AP_APB_S", true},
	{0, 95, 153, "CCIF5_MD_APB_S", true},
	{0, 96, 154, "HWCCF_APB_S", true},
	{0, 97, 155, "INFRA_BUS_HRE_APB_S", true},
	{0, 98, 156, "IPI_APB_S", true},
	{1, 0, 24, "ADSPSYS_S", true},

	/* 100 */
	{2, 0, 25, "MD_AP_S", true},
	{2, 1, 26, "MD_AP_S-1", true},
	{2, 2, 27, "MD_AP_S-2", true},
	{2, 3, 28, "MD_AP_S-3", true},
	{2, 4, 29, "MD_AP_S-4", true},
	{2, 5, 30, "MD_AP_S-5", true},
	{2, 6, 31, "MD_AP_S-6", true},
	{2, 7, 32, "MD_AP_S-7", true},
	{2, 8, 33, "MD_AP_S-8", true},
	{2, 9, 34, "MD_AP_S-9", true},

	/* 110 */
	{2, 10, 35, "MD_AP_S-10", true},
	{2, 11, 36, "MD_AP_S-11", true},
	{2, 12, 37, "MD_AP_S-12", true},
	{2, 13, 38, "MD_AP_S-13", true},
	{2, 14, 39, "MD_AP_S-14", true},
	{2, 15, 40, "MD_AP_S-15", true},
	{2, 16, 41, "MD_AP_S-16", true},
	{2, 17, 42, "MD_AP_S-17", true},
	{2, 18, 43, "MD_AP_S-18", true},
	{2, 19, 44, "MD_AP_S-19", true},

	/* 120 */
	{2, 20, 45, "MD_AP_S-20", true},
	{2, 21, 46, "MD_AP_S-21", true},
	{2, 22, 47, "MD_AP_S-22", true},
	{2, 23, 48, "MD_AP_S-23", true},
	{2, 24, 49, "MD_AP_S-24", true},
	{2, 25, 50, "MD_AP_S-25", true},
	{2, 26, 51, "MD_AP_S-26", true},
	{2, 27, 52, "MD_AP_S-27", true},
	{2, 28, 53, "MD_AP_S-28", true},
	{2, 29, 54, "MD_AP_S-29", true},

	/* 130 */
	{2, 30, 55, "MD_AP_S-30", true},
	{2, 31, 56, "MD_AP_S-31", true},
	{2, 32, 57, "MD_AP_S-32", true},
	{2, 33, 58, "MD_AP_S-33", true},
	{2, 34, 59, "MD_AP_S-34", true},
	{2, 35, 60, "MD_AP_S-35", true},
	{2, 36, 61, "MD_AP_S-36", true},
	{2, 37, 62, "MD_AP_S-37", true},
	{2, 38, 63, "MD_AP_S-38", true},
	{2, 39, 64, "MD_AP_S-39", true},

	/* 140 */
	{2, 40, 65, "MD_AP_S-40", true},
	{2, 41, 66, "MD_AP_S-41", true},
	{2, 42, 67, "MD_AP_S-42", true},
	{2, 43, 68, "MD_AP_S-43", true},
	{2, 44, 69, "MD_AP_S-44", true},
	{2, 45, 70, "MD_AP_S-45", true},
	{2, 46, 71, "MD_AP_S-46", true},
	{-1, -1, 157, "OOB_way_en", true},
	{-1, -1, 158, "OOB_way_en", true},
	{-1, -1, 159, "OOB_way_en", true},

	{-1, -1, 160, "OOB_way_en", true},
	{-1, -1, 161, "OOB_way_en", true},
	{-1, -1, 162, "OOB_way_en", true},
	{-1, -1, 163, "OOB_way_en", true},
	{-1, -1, 164, "OOB_way_en", true},
	{-1, -1, 165, "OOB_way_en", true},
	{-1, -1, 166, "OOB_way_en", true},
	{-1, -1, 167, "OOB_way_en", true},
	{-1, -1, 168, "OOB_way_en", true},
	{-1, -1, 169, "OOB_way_en", true},

	{-1, -1, 170, "OOB_way_en", true},
	{-1, -1, 171, "OOB_way_en", true},
	{-1, -1, 172, "OOB_way_en", true},
	{-1, -1, 173, "OOB_way_en", true},
	{-1, -1, 174, "OOB_way_en", true},
	{-1, -1, 175, "OOB_way_en", true},
	{-1, -1, 176, "OOB_way_en", true},
	{-1, -1, 177, "OOB_way_en", true},
	{-1, -1, 178, "OOB_way_en", true},
	{-1, -1, 179, "OOB_way_en", true},

	{-1, -1, 180, "OOB_way_en", true},
	{-1, -1, 181, "OOB_way_en", true},
	{-1, -1, 182, "OOB_way_en", true},
	{-1, -1, 183, "OOB_way_en", true},
	{-1, -1, 184, "OOB_way_en", true},
	{-1, -1, 185, "OOB_way_en", true},
	{-1, -1, 186, "OOB_way_en", true},
	{-1, -1, 187, "OOB_way_en", true},
	{-1, -1, 188, "OOB_way_en", true},
	{-1, -1, 189, "OOB_way_en", true},

	{-1, -1, 190, "OOB_way_en", true},
	{-1, -1, 191, "OOB_way_en", true},
	{-1, -1, 192, "OOB_way_en", true},
	{-1, -1, 193, "OOB_way_en", true},
	{-1, -1, 194, "OOB_way_en", true},
	{-1, -1, 195, "OOB_way_en", true},
	{-1, -1, 196, "OOB_way_en", true},
	{-1, -1, 197, "OOB_way_en", true},
	{-1, -1, 198, "OOB_way_en", true},
	{-1, -1, 199, "OOB_way_en", true},

	{-1, -1, 200, "OOB_way_en", true},
	{-1, -1, 201, "OOB_way_en", true},
	{-1, -1, 202, "OOB_way_en", true},
	{-1, -1, 203, "OOB_way_en", true},
	{-1, -1, 204, "OOB_way_en", true},
	{-1, -1, 205, "OOB_way_en", true},
	{-1, -1, 206, "OOB_way_en", true},
	{-1, -1, 207, "OOB_way_en", true},
	{-1, -1, 208, "OOB_way_en", true},
	{-1, -1, 209, "OOB_way_en", true},

	{-1, -1, 210, "OOB_way_en", true},
	{-1, -1, 211, "OOB_way_en", true},
	{-1, -1, 212, "OOB_way_en", true},
	{-1, -1, 213, "OOB_way_en", true},
	{-1, -1, 214, "OOB_way_en", true},
	{-1, -1, 215, "OOB_way_en", true},
	{-1, -1, 216, "OOB_way_en", true},
	{-1, -1, 217, "OOB_way_en", true},
	{-1, -1, 218, "OOB_way_en", true},
	{-1, -1, 219, "OOB_way_en", true},

	{-1, -1, 220, "OOB_way_en", true},
	{-1, -1, 221, "OOB_way_en", true},
	{-1, -1, 222, "OOB_way_en", true},
	{-1, -1, 223, "OOB_way_en", true},
	{-1, -1, 224, "OOB_way_en", true},
	{-1, -1, 225, "OOB_way_en", true},
	{-1, -1, 226, "OOB_way_en", true},
	{-1, -1, 227, "OOB_way_en", true},
	{-1, -1, 228, "OOB_way_en", true},
	{-1, -1, 229, "OOB_way_en", true},

	{-1, -1, 230, "OOB_way_en", true},
	{-1, -1, 231, "Decode_error", true},
	{-1, -1, 232, "Decode_error", true},
	{-1, -1, 233, "Decode_error", true},
	{-1, -1, 234, "Decode_error", true},
	{-1, -1, 235, "Decode_error", true},
	{-1, -1, 236, "Decode_error", true},
	{-1, -1, 237, "SRAMROM", true},
	{-1, -1, 238, "Reserved", false},
	{-1, -1, 239, "Reserved", false},

	{-1, -1, 240, "Reserved", false},
	{-1, -1, 241, "CQ_DMA", false},
	{-1, -1, 242, "DEVICE_APC_INFRA_AO", false},
	{-1, -1, 243, "DEVICE_APC_INFRA_PDN", false},
};

static const struct mtk_device_info mt6881_devices_infra1[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "DRAMC_MD32_S0_APB_S", true},
	{0, 1, 1, "DRAMC_MD32_S0_APB_S-1", true},
	{0, 2, 2, "DRAMC_MD32_S1_APB_S", true},
	{0, 3, 3, "DRAMC_MD32_S1_APB_S-1", true},
	{0, 4, 4, "BND_EAST_APB0_S", true},
	{0, 5, 5, "BND_EAST_APB1_S", true},
	{0, 6, 6, "BND_EAST_APB2_S", true},
	{0, 7, 7, "BND_EAST_APB3_S", true},
	{0, 8, 8, "BND_EAST_APB4_S", true},
	{0, 9, 9, "BND_EAST_APB5_S", true},

	/* 10 */
	{0, 10, 10, "BND_EAST_APB6_S", true},
	{0, 11, 11, "BND_EAST_APB7_S", true},
	{0, 12, 12, "BND_EAST_APB8_S", true},
	{0, 13, 13, "BND_EAST_APB9_S", true},
	{0, 14, 14, "BND_EAST_APB10_S", true},
	{0, 15, 15, "BND_EAST_APB11_S", true},
	{0, 16, 16, "BND_EAST_APB12_S", true},
	{0, 17, 17, "BND_EAST_APB13_S", true},
	{0, 18, 18, "BND_EAST_APB14_S", true},
	{0, 19, 19, "BND_EAST_APB15_S", true},

	/* 20 */
	{0, 20, 20, "BND_WEST_SOUTH_APB0_S", true},
	{0, 21, 21, "BND_WEST_SOUTH_APB1_S", true},
	{0, 22, 22, "BND_WEST_SOUTH_APB2_S", true},
	{0, 23, 23, "BND_WEST_SOUTH_APB3_S", true},
	{0, 24, 24, "BND_WEST_SOUTH_APB4_S", true},
	{0, 25, 25, "BND_WEST_SOUTH_APB5_S", true},
	{0, 26, 26, "BND_WEST_SOUTH_APB6_S", true},
	{0, 27, 27, "BND_WEST_SOUTH_APB7_S", true},
	{0, 28, 28, "BND_WEST_APB0_S", true},
	{0, 29, 29, "BND_WEST_APB1_S", true},

	/* 30 */
	{0, 30, 30, "BND_WEST_APB2_S", true},
	{0, 31, 31, "BND_WEST_APB3_S", true},
	{0, 32, 32, "BND_WEST_APB4_S", true},
	{0, 33, 33, "BND_WEST_APB5_S", true},
	{0, 34, 34, "BND_WEST_APB6_S", true},
	{0, 35, 35, "BND_WEST_APB7_S", true},
	{0, 36, 36, "BND_NORTH_APB0_S", true},
	{0, 37, 37, "BND_NORTH_APB1_S", true},
	{0, 38, 38, "BND_NORTH_APB2_S", true},
	{0, 39, 39, "BND_NORTH_APB3_S", true},

	/* 40 */
	{0, 40, 40, "BND_NORTH_APB4_S", true},
	{0, 41, 41, "BND_NORTH_APB5_S", true},
	{0, 42, 42, "BND_NORTH_APB6_S", true},
	{0, 43, 43, "BND_NORTH_APB7_S", true},
	{0, 44, 44, "BND_NORTH_APB8_S", true},
	{0, 45, 45, "BND_NORTH_APB9_S", true},
	{0, 46, 46, "BND_NORTH_APB10_S", true},
	{0, 47, 47, "BND_NORTH_APB11_S", true},
	{0, 48, 48, "BND_NORTH_APB12_S", true},
	{0, 49, 49, "BND_NORTH_APB13_S", true},

	/* 50 */
	{0, 50, 50, "BND_NORTH_APB14_S", true},
	{0, 51, 51, "BND_NORTH_APB15_S", true},
	{0, 52, 52, "BND_SOUTH_APB0_S", true},
	{0, 53, 53, "BND_SOUTH_APB1_S", true},
	{0, 54, 54, "BND_SOUTH_APB2_S", true},
	{0, 55, 55, "BND_SOUTH_APB3_S", true},
	{0, 56, 56, "BND_SOUTH_APB4_S", true},
	{0, 57, 57, "BND_SOUTH_APB5_S", true},
	{0, 58, 58, "BND_SOUTH_APB6_S", true},
	{0, 59, 59, "BND_SOUTH_APB7_S", true},

	/* 60 */
	{0, 60, 60, "BND_SOUTH_APB8_S", true},
	{0, 61, 61, "BND_SOUTH_APB9_S", true},
	{0, 62, 62, "BND_SOUTH_APB10_S", true},
	{0, 63, 63, "BND_SOUTH_APB11_S", true},
	{0, 64, 64, "BND_SOUTH_APB12_S", true},
	{0, 65, 65, "BND_SOUTH_APB13_S", true},
	{0, 66, 66, "BND_SOUTH_APB14_S", true},
	{0, 67, 67, "BND_SOUTH_APB15_S", true},
	{0, 68, 68, "BND_EAST_NORTH_APB0_S", true},
	{0, 69, 69, "BND_EAST_NORTH_APB1_S", true},

	/* 70 */
	{0, 70, 70, "BND_EAST_NORTH_APB2_S", true},
	{0, 71, 71, "BND_EAST_NORTH_APB3_S", true},
	{0, 72, 72, "BND_EAST_NORTH_APB4_S", true},
	{0, 73, 73, "BND_EAST_NORTH_APB5_S", true},
	{0, 74, 74, "BND_EAST_NORTH_APB6_S", true},
	{0, 75, 75, "BND_EAST_NORTH_APB7_S", true},
	{0, 76, 76, "DRAMC_CH0_TOP0_APB_S", true},
	{0, 77, 77, "DRAMC_CH0_TOP1_APB_S", true},
	{0, 78, 78, "DRAMC_CH0_TOP2_APB_S", true},
	{0, 79, 79, "DRAMC_CH0_TOP3_APB_S", true},

	/* 80 */
	{0, 80, 80, "DRAMC_CH0_TOP4_APB_S", true},
	{0, 81, 81, "DRAMC_CH0_TOP5_APB_S", true},
	{0, 82, 82, "DRAMC_CH0_TOP6_APB_S", true},
	{0, 83, 83, "DRAMC_CH1_TOP0_APB_S", true},
	{0, 84, 84, "DRAMC_CH1_TOP1_APB_S", true},
	{0, 85, 85, "DRAMC_CH1_TOP2_APB_S", true},
	{0, 86, 86, "DRAMC_CH1_TOP3_APB_S", true},
	{0, 87, 87, "DRAMC_CH1_TOP4_APB_S", true},
	{0, 88, 88, "DRAMC_CH1_TOP5_APB_S", true},
	{0, 89, 89, "DRAMC_CH1_TOP6_APB_S", true},

	/* 90 */
	{0, 90, 92, "NTH_EMI_MBIST_PDN_APB_S", true},
	{0, 91, 93, "INFRACFG_MEM_APB_S", true},
	{0, 92, 94, "EMI_APB_S", true},
	{0, 93, 95, "EMI_MPU_APB_S", true},
	{0, 94, 96, "DEVICE_MPU_PDN_APB_S", true},
	{0, 95, 97, "BCRM_FMEM_PDN_APB_S", true},
	{0, 96, 98, "FAKE_ENGINE_1_S", true},
	{0, 97, 99, "FAKE_ENGINE_0_S", true},
	{0, 98, 100, "EMI_SUB_INFRA_APB_S", true},
	{0, 99, 101, "EMI_MPU_SUB_INFRA_APB_S", true},

	/* 100 */
	{0, 100, 102, "DEVICE_MPU_PDN_SUB_INFRA_APB_S", true},
	{0, 101, 103, "MBIST_PDN_SUB_INFRA_APB_S", true},
	{0, 102, 104, "INFRACFG_MEM_SUB_INFRA_APB_S", true},
	{0, 103, 105, "BCRM_SUB_INFRA_AO_APB_S", true},
	{0, 104, 106, "DEBUG_CTRL_SUB_INFRA_AO_APB_S", true},
	{0, 105, 107, "BCRM_SUB_INFRA_PDN_APB_S", true},
	{0, 106, 108, "SSC_SUB_INFRA_APB0_S", true},
	{0, 107, 109, "SSC_SUB_INFRA_APB1_S", true},
	{0, 108, 110, "SSC_SUB_INFRA_APB2_S", true},
	{0, 109, 111, "INFRACFG_AO_MEM_SUB_INFRA_APB_S", true},

	/* 110 */
	{0, 110, 112, "SUB_FAKE_ENGINE_MM_S", true},
	{0, 111, 113, "SUB_FAKE_ENGINE_MDP_S", true},
	{0, 112, 114, "SSC_INFRA_APB0_S", true},
	{0, 113, 115, "SSC_INFRA_APB1_S", true},
	{0, 114, 116, "SSC_INFRA_APB2_S", true},
	{0, 115, 117, "INFRACFG_AO_MEM_APB_S", true},
	{0, 116, 118, "DEBUG_CTRL_FMEM_AO_APB_S", true},
	{0, 117, 119, "BCRM_FMEM_AO_APB_S", true},
	{0, 118, 120, "NEMI_RSI_APB_S", true},
	{0, 119, 121, "DEVICE_MPU_ACP_APB_S", true},

	/* 120 */
	{0, 120, 122, "NEMI_HRE_EMI_APB_S", true},
	{0, 121, 123, "NEMI_HRE_EMI_MPU_APB_S", true},
	{0, 122, 124, "NEMI_SMPU0_APB_S", true},
	{0, 123, 125, "NEMI_SMPU1_APB_S", true},
	{0, 124, 126, "NEMI_SMPU2_APB_S", true},
	{0, 125, 127, "NEMI_HRE_SMPU_APB_S", true},
	{0, 126, 128, "EMI_2ND_AO_APB_S", true},
	{0, 127, 129, "BCRM_INFRA_PDN1_APB_S", true},
	{0, 128, 130, "DEVICE_APC_INFRA_PDN1_APB_S", true},
	{0, 129, 131, "BCRM_INFRA_AO1_APB_S", true},

	/* 130 */
	{0, 130, 132, "DEVICE_APC_INFRA_AO1_APB_S", true},
	{0, 131, 133, "DEBUG_CTRL_INFRA_AO1_APB_S", true},
	{-1, -1, 134, "OOB_way_en", true},
	{-1, -1, 135, "OOB_way_en", true},
	{-1, -1, 136, "OOB_way_en", true},
	{-1, -1, 137, "OOB_way_en", true},
	{-1, -1, 138, "OOB_way_en", true},
	{-1, -1, 139, "OOB_way_en", true},
	{-1, -1, 140, "OOB_way_en", true},
	{-1, -1, 141, "OOB_way_en", true},

	{-1, -1, 142, "OOB_way_en", true},
	{-1, -1, 143, "OOB_way_en", true},
	{-1, -1, 144, "OOB_way_en", true},
	{-1, -1, 145, "OOB_way_en", true},
	{-1, -1, 146, "OOB_way_en", true},
	{-1, -1, 147, "OOB_way_en", true},
	{-1, -1, 148, "OOB_way_en", true},
	{-1, -1, 149, "OOB_way_en", true},
	{-1, -1, 150, "OOB_way_en", true},
	{-1, -1, 151, "OOB_way_en", true},

	{-1, -1, 152, "OOB_way_en", true},
	{-1, -1, 153, "OOB_way_en", true},
	{-1, -1, 154, "OOB_way_en", true},
	{-1, -1, 155, "OOB_way_en", true},
	{-1, -1, 156, "OOB_way_en", true},
	{-1, -1, 157, "OOB_way_en", true},
	{-1, -1, 158, "OOB_way_en", true},
	{-1, -1, 159, "OOB_way_en", true},
	{-1, -1, 160, "OOB_way_en", true},
	{-1, -1, 161, "OOB_way_en", true},

	{-1, -1, 162, "OOB_way_en", true},
	{-1, -1, 163, "OOB_way_en", true},
	{-1, -1, 164, "OOB_way_en", true},
	{-1, -1, 165, "OOB_way_en", true},
	{-1, -1, 166, "OOB_way_en", true},
	{-1, -1, 167, "OOB_way_en", true},
	{-1, -1, 168, "OOB_way_en", true},
	{-1, -1, 169, "OOB_way_en", true},
	{-1, -1, 170, "OOB_way_en", true},
	{-1, -1, 171, "OOB_way_en", true},

	{-1, -1, 172, "OOB_way_en", true},
	{-1, -1, 173, "OOB_way_en", true},
	{-1, -1, 174, "OOB_way_en", true},
	{-1, -1, 175, "OOB_way_en", true},
	{-1, -1, 176, "OOB_way_en", true},
	{-1, -1, 177, "OOB_way_en", true},
	{-1, -1, 178, "OOB_way_en", true},
	{-1, -1, 179, "OOB_way_en", true},
	{-1, -1, 180, "OOB_way_en", true},
	{-1, -1, 181, "OOB_way_en", true},

	{-1, -1, 182, "OOB_way_en", true},
	{-1, -1, 183, "OOB_way_en", true},
	{-1, -1, 184, "OOB_way_en", true},
	{-1, -1, 185, "OOB_way_en", true},
	{-1, -1, 186, "OOB_way_en", true},
	{-1, -1, 187, "OOB_way_en", true},
	{-1, -1, 188, "OOB_way_en", true},
	{-1, -1, 189, "OOB_way_en", true},
	{-1, -1, 190, "OOB_way_en", true},
	{-1, -1, 191, "OOB_way_en", true},

	{-1, -1, 192, "OOB_way_en", true},
	{-1, -1, 193, "OOB_way_en", true},
	{-1, -1, 194, "OOB_way_en", true},
	{-1, -1, 195, "OOB_way_en", true},
	{-1, -1, 196, "OOB_way_en", true},
	{-1, -1, 197, "OOB_way_en", true},
	{-1, -1, 198, "OOB_way_en", true},
	{-1, -1, 199, "OOB_way_en", true},
	{-1, -1, 200, "OOB_way_en", true},
	{-1, -1, 201, "OOB_way_en", true},

	{-1, -1, 202, "OOB_way_en", true},
	{-1, -1, 203, "OOB_way_en", true},
	{-1, -1, 204, "OOB_way_en", true},
	{-1, -1, 205, "OOB_way_en", true},
	{-1, -1, 206, "OOB_way_en", true},
	{-1, -1, 207, "OOB_way_en", true},
	{-1, -1, 208, "OOB_way_en", true},
	{-1, -1, 209, "OOB_way_en", true},
	{-1, -1, 210, "OOB_way_en", true},
	{-1, -1, 211, "OOB_way_en", true},

	{-1, -1, 212, "OOB_way_en", true},
	{-1, -1, 213, "OOB_way_en", true},
	{-1, -1, 214, "OOB_way_en", true},
	{-1, -1, 215, "OOB_way_en", true},
	{-1, -1, 216, "OOB_way_en", true},
	{-1, -1, 217, "OOB_way_en", true},
	{-1, -1, 218, "OOB_way_en", true},
	{-1, -1, 219, "OOB_way_en", true},
	{-1, -1, 220, "OOB_way_en", true},
	{-1, -1, 221, "OOB_way_en", true},

	{-1, -1, 222, "OOB_way_en", true},
	{-1, -1, 223, "OOB_way_en", true},
	{-1, -1, 224, "OOB_way_en", true},
	{-1, -1, 225, "OOB_way_en", true},
	{-1, -1, 226, "OOB_way_en", true},
	{-1, -1, 227, "OOB_way_en", true},
	{-1, -1, 228, "OOB_way_en", true},
	{-1, -1, 229, "OOB_way_en", true},
	{-1, -1, 230, "OOB_way_en", true},
	{-1, -1, 231, "OOB_way_en", true},

	{-1, -1, 232, "OOB_way_en", true},
	{-1, -1, 233, "OOB_way_en", true},
	{-1, -1, 234, "OOB_way_en", true},
	{-1, -1, 235, "OOB_way_en", true},
	{-1, -1, 236, "OOB_way_en", true},
	{-1, -1, 237, "OOB_way_en", true},
	{-1, -1, 238, "OOB_way_en", true},
	{-1, -1, 239, "OOB_way_en", true},
	{-1, -1, 240, "OOB_way_en", true},
	{-1, -1, 241, "OOB_way_en", true},

	{-1, -1, 242, "OOB_way_en", true},
	{-1, -1, 243, "OOB_way_en", true},
	{-1, -1, 244, "OOB_way_en", true},
	{-1, -1, 245, "OOB_way_en", true},
	{-1, -1, 246, "OOB_way_en", true},
	{-1, -1, 247, "OOB_way_en", true},
	{-1, -1, 248, "OOB_way_en", true},
	{-1, -1, 249, "OOB_way_en", true},
	{-1, -1, 250, "OOB_way_en", true},
	{-1, -1, 251, "OOB_way_en", true},

	{-1, -1, 252, "OOB_way_en", true},
	{-1, -1, 253, "OOB_way_en", true},
	{-1, -1, 254, "OOB_way_en", true},
	{-1, -1, 255, "OOB_way_en", true},
	{-1, -1, 256, "OOB_way_en", true},
	{-1, -1, 257, "OOB_way_en", true},
	{-1, -1, 258, "OOB_way_en", true},
	{-1, -1, 259, "OOB_way_en", true},
	{-1, -1, 260, "OOB_way_en", true},
	{-1, -1, 261, "OOB_way_en", true},

	{-1, -1, 262, "OOB_way_en", true},
	{-1, -1, 263, "OOB_way_en", true},
	{-1, -1, 264, "OOB_way_en", true},
	{-1, -1, 265, "OOB_way_en", true},
	{-1, -1, 266, "OOB_way_en", true},
	{-1, -1, 267, "OOB_way_en", true},
	{-1, -1, 268, "OOB_way_en", true},
	{-1, -1, 269, "OOB_way_en", true},
	{-1, -1, 270, "OOB_way_en", true},
	{-1, -1, 271, "OOB_way_en", true},

	{-1, -1, 272, "OOB_way_en", true},
	{-1, -1, 273, "OOB_way_en", true},
	{-1, -1, 274, "Decode_error", true},
	{-1, -1, 275, "Decode_error", true},
	{-1, -1, 276, "Decode_error", true},
	{-1, -1, 277, "Decode_error", true},
	{-1, -1, 278, "Decode_error", true},
	{-1, -1, 279, "Decode_error", true},
	{-1, -1, 280, "Decode_error", true},
	{-1, -1, 281, "Decode_error", true},

	{-1, -1, 282, "Decode_error", true},
	{-1, -1, 283, "Decode_error", true},
	{-1, -1, 284, "North EMI", false},
	{-1, -1, 285, "Reserved", false},
	{-1, -1, 286, "Reserved", false},
	{-1, -1, 287, "Reserved", false},
	{-1, -1, 288, "DEVICE_APC_INFRA_AO1", false},
	{-1, -1, 289, "DEVICE_APC_INFRA_PDN1", false},
};

static const struct mtk_device_info mt6881_devices_peri_par[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "UARTHUB_APB_S", true},
	{0, 1, 1, "UART0_APB_S", true},
	{0, 2, 2, "UART1_APB_S", true},
	{0, 3, 3, "UART2_APB_S", true},
	{0, 4, 4, "UART3_APB_S", true},
	{0, 5, 5, "UART4_APB_S", true},
	{0, 6, 6, "UART5_APB_S", true},
	{0, 7, 7, "PWM_PERI_APB_S", true},
	{0, 8, 8, "DISP_PWM0_APB_S", true},
	{0, 9, 9, "DISP_PWM1_APB_S", true},

	/* 10 */
	{0, 10, 10, "SPI0_APB_S", true},
	{0, 11, 11, "SPI1_APB_S", true},
	{0, 12, 12, "SPI2_APB_S", true},
	{0, 13, 13, "SPI3_APB_S", true},
	{0, 14, 14, "SPI4_APB_S", true},
	{0, 15, 15, "SPI5_APB_S", true},
	{0, 16, 16, "SPI6_APB_S", true},
	{0, 17, 17, "SPI7_APB_S", true},
	{0, 18, 18, "SPIS0_APB_S", true},
	{0, 19, 19, "SPIS1_APB_S", true},

	/* 20 */
	{0, 20, 20, "DEVICE_APC_PERI_PAR_PDN_APB_S", true},
	{0, 21, 21, "BCRM_PERI_PAR_PDN_APB_S", true},
	{0, 22, 22, "IIC_P2P_REMAP_APB_S", true},
	{0, 23, 23, "APDMA_APB_S", true},
	{0, 24, 24, "PERI_FAKE_ENG_MI10_APB_S", true},
	{0, 25, 25, "PERI_FAKE_ENG_MI11_APB_S", true},
	{0, 26, 26, "PERI_FAKE_ENG_MI20_APB_S", true},
	{0, 27, 27, "PERI_FAKE_ENG_MI21_APB_S", true},
	{0, 28, 28, "SSUSB_S", true},
	{0, 29, 29, "SSUSB_S-1", true},

	/* 30 */
	{0, 30, 30, "SSUSB_S-2", true},
	{0, 31, 31, "AUDIO_S-1", true},
	{0, 32, 32, "AUDIO_S-2", true},
	{0, 33, 33, "MSDC1_S-1", true},
	{0, 34, 34, "MSDC1_S-2", true},
	{0, 35, 35, "UFS0_S", true},
	{0, 36, 36, "UFS0_S-1", true},
	{0, 37, 37, "UFS0_S-2", true},
	{0, 38, 38, "UFS0_S-3", true},
	{0, 39, 39, "UFS0_S-4", true},

	/* 40 */
	{0, 40, 40, "UFS0_S-5", true},
	{0, 41, 41, "UFS0_S-6", true},
	{0, 42, 42, "UFS0_S-7", true},
	{0, 43, 43, "PCIE_S", true},
	{0, 44, 44, "PCIE_S-8", true},
	{0, 45, 45, "PCIE_S-11", true},
	{0, 46, 46, "PCIE_S-12", true},
	{0, 47, 47, "PCIE_S-13", true},
	{0, 48, 48, "PCIE_S-14", true},
	{0, 49, 49, "PCIE_S-16", true},

	/* 50 */
	{0, 50, 50, "PCIE_S-18", true},
	{0, 51, 51, "PCIE_S-19", true},
	{0, 52, 52, "PCIE_S-25", true},
	{0, 53, 53, "DEVICE_APC_PERI_PAR_AO_APB_S", true},
	{0, 54, 54, "PERI_MBIST_AO_APB_S", true},
	{0, 55, 55, "PERICFG_AO_APB_S", true},
	{0, 56, 56, "BCRM_PERI_PAR_AO_APB_S", true},
	{0, 57, 57, "DEBUG_CTRL_PERI_PAR_AO_APB_S", true},
	{-1, -1, 58, "OOB_way_en", true},
	{-1, -1, 59, "OOB_way_en", true},

	{-1, -1, 60, "OOB_way_en", true},
	{-1, -1, 61, "OOB_way_en", true},
	{-1, -1, 62, "OOB_way_en", true},
	{-1, -1, 63, "OOB_way_en", true},
	{-1, -1, 64, "OOB_way_en", true},
	{-1, -1, 65, "OOB_way_en", true},
	{-1, -1, 66, "OOB_way_en", true},
	{-1, -1, 67, "OOB_way_en", true},
	{-1, -1, 68, "OOB_way_en", true},
	{-1, -1, 69, "OOB_way_en", true},

	{-1, -1, 70, "OOB_way_en", true},
	{-1, -1, 71, "OOB_way_en", true},
	{-1, -1, 72, "OOB_way_en", true},
	{-1, -1, 73, "OOB_way_en", true},
	{-1, -1, 74, "OOB_way_en", true},
	{-1, -1, 75, "OOB_way_en", true},
	{-1, -1, 76, "OOB_way_en", true},
	{-1, -1, 77, "OOB_way_en", true},
	{-1, -1, 78, "OOB_way_en", true},
	{-1, -1, 79, "OOB_way_en", true},

	{-1, -1, 80, "OOB_way_en", true},
	{-1, -1, 81, "OOB_way_en", true},
	{-1, -1, 82, "OOB_way_en", true},
	{-1, -1, 83, "OOB_way_en", true},
	{-1, -1, 84, "OOB_way_en", true},
	{-1, -1, 85, "OOB_way_en", true},
	{-1, -1, 86, "OOB_way_en", true},
	{-1, -1, 87, "OOB_way_en", true},
	{-1, -1, 88, "OOB_way_en", true},
	{-1, -1, 89, "OOB_way_en", true},

	{-1, -1, 90, "OOB_way_en", true},
	{-1, -1, 91, "OOB_way_en", true},
	{-1, -1, 92, "OOB_way_en", true},
	{-1, -1, 93, "OOB_way_en", true},
	{-1, -1, 94, "Decode_error", true},
	{-1, -1, 95, "Decode_error", true},
	{-1, -1, 96, "APDMA", false},
	{-1, -1, 97, "IIC_P2P_REMAP", false},
	{-1, -1, 98, "DEVICE_APC_PERI_AO", false},
	{-1, -1, 99, "DEVICE_APC_PERI_PDN", false},
};

static const struct mtk_device_info mt6881_devices_vlp[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "INFRA_S0_S", true},
	{0, 1, 1, "INFRA_S0_S-1", true},
	{0, 2, 2, "INFRA_S0_S-2", true},
	{0, 3, 3, "SCP_S", true},
	{0, 4, 4, "SCP_S-1", true},
	{0, 5, 5, "SCP_S-2", true},
	{0, 6, 6, "SCP_S-3", true},
	{0, 7, 7, "SCP_S-4", true},
	{0, 8, 8, "SCP_S-5", true},
	{0, 9, 9, "SCP_S-6", true},

	/* 10 */
	{0, 10, 10, "SPM_APB_S", true},
	{0, 11, 11, "SPM_APB_S-1", true},
	{0, 12, 12, "SPM_APB_S-2", true},
	{0, 13, 13, "SPM_APB_S-3", true},
	{0, 14, 14, "SPM_APB_S-4", true},
	{0, 15, 15, "SPM_APB_S-5", true},
	{0, 16, 16, "SPM_APB_S-6", true},
	{0, 17, 17, "SPM_APB_S-7", true},
	{0, 18, 18, "SPM_APB_S-8", true},
	{0, 19, 19, "SPM_APB_S-9", true},

	/* 20 */
	{0, 20, 20, "SPM_APB_S-10", true},
	{0, 21, 21, "SPM_APB_S-11", true},
	{0, 22, 22, "SPM_APB_S-12", true},
	{0, 23, 23, "SPM_APB_S-13", true},
	{0, 24, 24, "SYS_TIMER_APB_S", true},
	{0, 25, 25, "SYS_TIMER_APB_S-1", true},
	{0, 26, 26, "SYS_TIMER_APB_S-2", true},
	{0, 27, 27, "SYS_TIMER_APB_S-3", true},
	{0, 28, 28, "SYS_TIMER_APB_S-4", true},
	{0, 29, 29, "SYS_TIMER_APB_S-5", true},

	/* 30 */
	{0, 30, 30, "SYS_TIMER_APB_S-6", true},
	{0, 31, 31, "SYS_TIMER_APB_S-7", true},
	{0, 32, 32, "SYS_TIMER_APB_S-8", true},
	{0, 33, 33, "SYS_TIMER_APB_S-9", true},
	{0, 34, 34, "SYS_TIMER_APB_S-10", true},
	{0, 35, 35, "SYS_TIMER_APB_S-11", true},
	{0, 36, 36, "SYS_TIMER_APB_S-12", true},
	{0, 37, 37, "SYS_TIMER_APB_S-13", true},
	{0, 38, 38, "SYS_TIMER_APB_S-14", true},
	{0, 39, 39, "SYS_TIMER_APB_S-15", true},

	/* 40 */
	{0, 40, 40, "SSPM_S", true},
	{0, 41, 41, "SSPM_S-1", true},
	{0, 42, 42, "SSPM_S-2", true},
	{0, 43, 43, "SSPM_S-3", true},
	{0, 44, 44, "SSPM_S-4", true},
	{0, 45, 45, "SSPM_S-5", true},
	{0, 46, 46, "SSPM_S-6", true},
	{0, 47, 47, "SSPM_S-7", true},
	{0, 48, 48, "SSPM_S-8", true},
	{0, 49, 49, "SSPM_S-9", true},

	/* 50 */
	{0, 50, 50, "SSPM_S-10", true},
	{0, 51, 51, "SSPM_S-11", true},
	{0, 52, 52, "SSPM_S-12", true},
	{0, 53, 53, "VLPCFG_AO_APB_S", true},
	{0, 54, 55, "SRCLKEN_RC_APB_S", true},
	{0, 55, 56, "TOPRGU_APB_S", true},
	{0, 56, 57, "APXGPT_APB_S", true},
	{0, 57, 58, "KP_APB_S", true},
	{0, 58, 59, "DVFSRC_APB_S", true},
	{0, 59, 60, "MBIST_APB_S", true},

	/* 60 */
	{0, 60, 62, "VLP_CKSYS_APB_S", true},
	{0, 61, 63, "TIA_APB_S", true},
	{0, 62, 64, "PMIF1_APB_S", true},
	{0, 63, 65, "PMIF2_APB_S", true},
	{0, 64, 67, "PWM_VLP_APB_S", true},
	{0, 65, 68, "DEVICE_APC_VLP_AO_APB_S", true},
	{0, 66, 69, "DEVICE_APC_VLP_APB_S", true},
	{0, 67, 70, "BCRM_VLP_AO_APB_S", true},
	{0, 68, 71, "DEBUG_CTRL_VLP_AO_APB_S", true},
	{0, 69, 72, "VLPCFG_APB_S", true},

	/* 70 */
	{0, 70, 73, "SPMI_P_MST_APB_S", true},
	{0, 71, 74, "SPMI_M_MST_APB_S", true},
	{0, 72, 75, "EFUSE_DEBUG_AO_APB_S", true},
	{0, 73, 76, "AP_CIRQ_EINT_APB_S", true},
	{0, 74, 77, "DPMSR_APB_S", true},
	{-1, -1, 78, "OOB_way_en", true},
	{-1, -1, 79, "OOB_way_en", true},
	{-1, -1, 80, "OOB_way_en", true},
	{-1, -1, 81, "OOB_way_en", true},
	{-1, -1, 82, "OOB_way_en", true},

	{-1, -1, 83, "OOB_way_en", true},
	{-1, -1, 84, "OOB_way_en", true},
	{-1, -1, 85, "OOB_way_en", true},
	{-1, -1, 86, "OOB_way_en", true},
	{-1, -1, 87, "OOB_way_en", true},
	{-1, -1, 88, "OOB_way_en", true},
	{-1, -1, 89, "OOB_way_en", true},
	{-1, -1, 90, "OOB_way_en", true},
	{-1, -1, 91, "OOB_way_en", true},
	{-1, -1, 92, "OOB_way_en", true},

	{-1, -1, 93, "OOB_way_en", true},
	{-1, -1, 94, "OOB_way_en", true},
	{-1, -1, 95, "OOB_way_en", true},
	{-1, -1, 96, "OOB_way_en", true},
	{-1, -1, 97, "OOB_way_en", true},
	{-1, -1, 98, "OOB_way_en", true},
	{-1, -1, 99, "OOB_way_en", true},
	{-1, -1, 100, "OOB_way_en", true},
	{-1, -1, 101, "OOB_way_en", true},
	{-1, -1, 102, "OOB_way_en", true},

	{-1, -1, 103, "OOB_way_en", true},
	{-1, -1, 104, "OOB_way_en", true},
	{-1, -1, 105, "OOB_way_en", true},
	{-1, -1, 106, "Decode_error", true},
	{-1, -1, 107, "PMIF1", false},
	{-1, -1, 108, "PMIF2", false},
	{-1, -1, 109, "DEVICE_APC_VLP_AO", false},
	{-1, -1, 110, "DEVICE_APC_VLP_PDN", false},
};

#if ENABLE_DEVAPC_ADSP
static const struct mtk_device_info mt6881_devices_adsp[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
};
#endif

static const struct mtk_device_info mt6881_devices_mminfra[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "MMUP_APB_S", true},
	{0, 1, 1, "MMUP_APB_S-1", true},
	{0, 2, 2, "GCE_M_APB_S", true},
	{0, 3, 3, "GCE_M_APB_S-1", true},
	{0, 4, 4, "GCE_M_APB_S-2", true},
	{0, 5, 5, "GCE_M_APB_S-3", true},
	{0, 6, 6, "GCE_M_APB_S-4", true},
	{0, 7, 7, "GCE_M_APB_S-5", true},
	{0, 8, 8, "GCE_M_APB_S-6", true},
	{0, 9, 9, "GCE_M_APB_S-7", true},

	/* 10 */
	{0, 10, 10, "GCE_M2_APB_S", true},
	{0, 11, 11, "GCE_M2_APB_S-1", true},
	{0, 12, 12, "GCE_M2_APB_S-2", true},
	{0, 13, 13, "GCE_M2_APB_S-3", true},
	{0, 14, 14, "GCE_M2_APB_S-4", true},
	{0, 15, 15, "GCE_M2_APB_S-5", true},
	{0, 16, 16, "GCE_M2_APB_S-6", true},
	{0, 17, 17, "GCE_M2_APB_S-7", true},
	{0, 18, 18, "GCE_D_APB_S", true},
	{0, 19, 19, "GCE_D_APB_S-1", true},

	/* 20 */
	{0, 20, 20, "GCE_D_APB_S-2", true},
	{0, 21, 21, "GCE_D_APB_S-3", true},
	{0, 22, 22, "GCE_D_APB_S-4", true},
	{0, 23, 23, "GCE_D_APB_S-5", true},
	{0, 24, 24, "GCE_D_APB_S-6", true},
	{0, 25, 25, "GCE_D_APB_S-7", true},
	{0, 26, 26, "MMINFRA_APB_S", true},
	{0, 27, 27, "MMINFRA_APB_S-1", true},
	{0, 28, 28, "MMINFRA_APB_S-2", true},
	{0, 29, 29, "MMINFRA_APB_S-3", true},

	/* 30 */
	{0, 30, 30, "MMINFRA_APB_S-4", true},
	{0, 31, 31, "MMINFRA_APB_S-5", true},
	{0, 32, 32, "MMINFRA_APB_S-6", true},
	{0, 33, 33, "MMINFRA_APB_S-7", true},
	{0, 34, 34, "MMINFRA_APB_S-8", true},
	{0, 35, 35, "MMINFRA_APB_S-9", true},
	{0, 36, 36, "MMINFRA_APB_S-10", true},
	{0, 37, 37, "MMINFRA_APB_S-11", true},
	{0, 38, 38, "MMINFRA_APB_S-12", true},
	{0, 39, 39, "MMINFRA_APB_S-13", true},

	/* 40 */
	{0, 40, 40, "MMINFRA_APB_S-14", true},
	{0, 41, 41, "MMINFRA_APB_S-15", true},
	{0, 42, 42, "MMINFRA_APB_S-16", true},
	{0, 43, 43, "MMINFRA_APB_S-17", true},
	{0, 44, 44, "MMINFRA_APB_S-18", true},
	{0, 45, 45, "MMINFRA_APB_S-19", true},
	{0, 46, 46, "MMINFRA_APB_S-20", true},
	{0, 47, 47, "MMINFRA_APB_S-21", true},
	{0, 48, 48, "MMINFRA_APB_S-22", true},
	{0, 49, 49, "MMINFRA_APB_S-23", true},

	/* 50 */
	{0, 50, 50, "MMINFRA_APB_S-24", true},
	{0, 51, 51, "MMINFRA_APB_S-37", true},
	{0, 52, 52, "DAPC_PDN_S", true},
	{0, 53, 53, "BCRM_PDN_S", true},
	{0, 54, 54, "VENC_APB_S-0", true},
	{0, 55, 55, "VENC_APB_S-1", true},
	{0, 56, 56, "VENC_APB_S-2", true},
	{0, 57, 57, "VENC_APB_S-3", true},
	{0, 58, 58, "VENC_APB_S-4", true},
	{0, 59, 59, "VENC_APB_S-5", true},

	/* 60 */
	{0, 60, 60, "VENC_APB_S-6", true},
	{0, 61, 61, "VENC_APB_S-7", true},
	{0, 62, 62, "VDEC_APB_S-0", true},
	{0, 63, 63, "VDEC_APB_S-1", true},
	{0, 64, 64, "VDEC_APB_S-2", true},
	{0, 65, 65, "VDEC_APB_S-3", true},
	{0, 66, 66, "VDEC_APB_S-4", true},
	{0, 67, 67, "VDEC_APB_S-5", true},
	{0, 68, 68, "VDEC_APB_S-6", true},
	{0, 69, 69, "VDEC_APB_S-7", true},

	/* 70 */
	{0, 70, 70, "VDEC_APB_S-8", true},
	{0, 71, 71, "VDEC_APB_S-9", true},
	{0, 72, 72, "VDEC_APB_S-10", true},
	{0, 73, 73, "VDEC_APB_S-11", true},
	{0, 74, 74, "VDEC_APB_S-12", true},
	{0, 75, 75, "CCU_APB_S-0", true},
	{0, 76, 76, "CCU_APB_S-1", true},
	{0, 77, 77, "CCU_APB_S-2", true},
	{0, 78, 78, "CCU_APB_S-3", true},
	{0, 79, 79, "CCU_APB_S-4", true},

	/* 80 */
	{0, 80, 80, "CCU_APB_S-5", true},
	{0, 81, 81, "CCU_APB_S-6", true},
	{0, 82, 82, "CCU_APB_S-7", true},
	{0, 83, 83, "CCU_APB_S-8", true},
	{0, 84, 84, "CCU_APB_S-9", true},
	{0, 85, 85, "CCU_APB_S-10", true},
	{0, 86, 86, "CCU_APB_S-11", true},
	{0, 87, 87, "CCU_APB_S-12", true},
	{0, 88, 88, "CCU_APB_S-13", true},
	{0, 89, 89, "CCU_APB_S-14", true},

	/* 90 */
	{0, 90, 90, "CCU_APB_S-15", true},
	{0, 91, 91, "CCU_APB_S-16", true},
	{0, 92, 92, "CCU_APB_S-17", true},
	{0, 93, 93, "CCU_APB_S-18", true},
	{0, 94, 94, "CCU_APB_S-19", true},
	{0, 95, 95, "CCU_APB_S-20", true},
	{0, 96, 96, "CCU_APB_S-21", true},
	{0, 97, 97, "CCU_APB_S-22", true},
	{0, 98, 98, "CCU_APB_S-23", true},
	{0, 99, 99, "CCU_APB_S-24", true},

	/* 100 */
	{0, 100, 100, "CCU_APB_S-25", true},
	{0, 101, 101, "CCU_APB_S-26", true},
	{0, 102, 102, "CCU_APB_S-27", true},
	{0, 103, 103, "CCU_APB_S-28", true},
	{0, 104, 104, "CCU_APB_S-29", true},
	{0, 105, 105, "CCU_APB_S-30", true},
	{0, 106, 106, "CCU_APB_S-31", true},
	{0, 107, 107, "CCU_APB_S-32", true},
	{0, 108, 108, "CCU_APB_S-33", true},
	{0, 109, 109, "CCU_APB_S-34", true},

	/* 110 */
	{0, 110, 110, "CCU_APB_S-35", true},
	{0, 111, 111, "CCU_APB_S-36", true},
	{0, 112, 112, "CCU_APB_S-37", true},
	{0, 113, 113, "CCU_APB_S-38", true},
	{0, 114, 114, "CCU_APB_S-39", true},
	{0, 115, 115, "CCU_APB_S-40", true},
	{0, 116, 116, "CCU_APB_S-41", true},
	{0, 117, 117, "CCU_APB_S-42", true},
	{0, 118, 118, "CCU_APB_S-43", true},
	{0, 119, 119, "CCU_APB_S-44", true},

	/* 120 */
	{0, 120, 120, "CCU_APB_S-45", true},
	{0, 121, 121, "CCU_APB_S-46", true},
	{0, 122, 122, "CCU_APB_S-47", true},
	{0, 123, 123, "CCU_APB_S-48", true},
	{0, 124, 124, "CCU_APB_S-49", true},
	{0, 125, 125, "CCU_APB_S-50", true},
	{0, 126, 126, "CCU_APB_S-51", true},
	{0, 127, 127, "CCU_APB_S-52", true},
	{0, 128, 128, "CCU_APB_S-53", true},
	{0, 129, 129, "CCU_APB_S-54", true},

	/* 130 */
	{0, 130, 130, "CCU_APB_S-55", true},
	{0, 131, 131, "CCU_APB_S-56", true},
	{0, 132, 132, "CCU_APB_S-57", true},
	{0, 133, 133, "CCU_APB_S-58", true},
	{0, 134, 134, "CAM_APB_S-0", true},
	{0, 135, 135, "CAM_APB_S-1", true},
	{0, 136, 136, "CAM_APB_S-2", true},
	{0, 137, 137, "CAM_APB_S-3", true},
	{0, 138, 138, "CAM_APB_S-4", true},
	{0, 139, 139, "CAM_APB_S-5", true},

	/* 140 */
	{0, 140, 140, "CAM_APB_S-6", true},
	{0, 141, 141, "CAM_APB_S-7", true},
	{0, 142, 142, "CAM_APB_S-8", true},
	{0, 143, 143, "CAM_APB_S-9", true},
	{0, 144, 144, "CAM_APB_S-10", true},
	{0, 145, 145, "CAM_APB_S-11", true},
	{0, 146, 146, "CAM_APB_S-12", true},
	{0, 147, 147, "CAM_APB_S-13", true},
	{0, 148, 148, "CAM_APB_S-14", true},
	{0, 149, 149, "CAM_APB_S-15", true},

	/* 150 */
	{0, 150, 150, "CAM_APB_S-16", true},
	{0, 151, 151, "CAM_APB_S-17", true},
	{0, 152, 152, "CAM_APB_S-18", true},
	{0, 153, 153, "CAM_APB_S-19", true},
	{0, 154, 154, "CAM_APB_S-20", true},
	{0, 155, 155, "CAM_APB_S-21", true},
	{0, 156, 156, "CAM_APB_S-22", true},
	{0, 157, 157, "CAM_APB_S-23", true},
	{0, 158, 158, "CAM_APB_S-24", true},
	{0, 159, 159, "CAM_APB_S-25", true},

	/* 160 */
	{0, 160, 160, "CAM_APB_S-26", true},
	{0, 161, 161, "CAM_APB_S-27", true},
	{0, 162, 162, "CAM_APB_S-28", true},
	{0, 163, 163, "CAM_APB_S-29", true},
	{0, 164, 164, "CAM_APB_S-30", true},
	{0, 165, 165, "CAM_APB_S-31", true},
	{0, 166, 166, "CAM_APB_S-32", true},
	{0, 167, 167, "CAM_APB_S-33", true},
	{0, 168, 168, "CAM_APB_S-34", true},
	{0, 169, 169, "CAM_APB_S-35", true},

	/* 170 */
	{0, 170, 170, "CAM_APB_S-36", true},
	{0, 171, 171, "CAM_APB_S-37", true},
	{0, 172, 172, "CAM_APB_S-38", true},
	{0, 173, 173, "CAM_APB_S-39", true},
	{0, 174, 174, "CAM_APB_S-40", true},
	{0, 175, 175, "CAM_APB_S-41", true},
	{0, 176, 176, "CAM_APB_S-42", true},
	{0, 177, 177, "CAM_APB_S-43", true},
	{0, 178, 178, "CAM_APB_S-44", true},
	{0, 179, 179, "CAM_APB_S-45", true},

	/* 180 */
	{0, 180, 180, "CAM_APB_S-46", true},
	{0, 181, 181, "CAM_APB_S-47", true},
	{0, 182, 182, "CAM_APB_S-48", true},
	{0, 183, 183, "CAM_APB_S-49", true},
	{0, 184, 184, "CAM_APB_S-50", true},
	{0, 185, 185, "CAM_APB_S-51", true},
	{0, 186, 186, "CAM_APB_S-52", true},
	{0, 187, 187, "CAM_APB_S-53", true},
	{0, 188, 188, "CAM_APB_S-54", true},
	{0, 189, 189, "CAM_APB_S-55", true},

	/* 190 */
	{0, 190, 190, "CAM_APB_S-56", true},
	{0, 191, 191, "CAM_APB_S-57", true},
	{0, 192, 192, "CAM_APB_S-58", true},
	{0, 193, 193, "CAM_APB_S-59", true},
	{0, 194, 194, "CAM_APB_S-60", true},
	{0, 195, 195, "CAM_APB_S-61", true},
	{0, 196, 196, "CAM_APB_S-62", true},
	{0, 197, 197, "CAM_APB_S-63", true},
	{0, 198, 198, "CAM_APB_S-64", true},
	{0, 199, 199, "CAM_APB_S-65", true},

	/* 200 */
	{0, 200, 200, "CAM_APB_S-66", true},
	{0, 201, 201, "CAM_APB_S-67", true},
	{0, 202, 202, "CAM_APB_S-68", true},
	{0, 203, 203, "CAM_APB_S-69", true},
	{0, 204, 204, "CAM_APB_S-70", true},
	{0, 205, 205, "CAM_APB_S-71", true},
	{0, 206, 206, "CAM_APB_S-72", true},
	{0, 207, 207, "CAM_APB_S-73", true},
	{0, 208, 208, "CAM_APB_S-74", true},
	{0, 209, 209, "CAM_APB_S-75", true},

	/* 210 */
	{0, 210, 210, "CAM_APB_S-76", true},
	{0, 211, 211, "CAM_APB_S-77", true},
	{0, 212, 212, "CAM_APB_S-78", true},
	{0, 213, 213, "CAM_APB_S-79", true},
	{0, 214, 214, "CAM_APB_S-80", true},
	{0, 215, 215, "CAM_APB_S-81", true},
	{0, 216, 216, "CAM_APB_S-82", true},
	{0, 217, 217, "CAM_APB_S-83", true},
	{0, 218, 218, "CAM_APB_S-84", true},
	{0, 219, 219, "CAM_APB_S-85", true},

	/* 220 */
	{0, 220, 220, "CAM_APB_S-86", true},
	{0, 221, 221, "CAM_APB_S-87", true},
	{0, 222, 222, "CAM_APB_S-88", true},
	{0, 223, 223, "CAM_APB_S-89", true},
	{0, 224, 224, "CAM_APB_S-90", true},
	{0, 225, 225, "CAM_APB_S-91", true},
	{0, 226, 226, "CAM_APB_S-92", true},
	{0, 227, 227, "CAM_APB_S-93", true},
	{0, 228, 228, "CAM_APB_S-94", true},
	{0, 229, 229, "CAM_APB_S-95", true},

	/* 230 */
	{0, 230, 230, "CAM_APB_S-96", true},
	{0, 231, 231, "CAM_APB_S-97", true},
	{0, 232, 232, "CAM_APB_S-98", true},
	{0, 233, 233, "CAM_APB_S-99", true},
	{0, 234, 234, "CAM_APB_S-100", true},
	{0, 235, 235, "CAM_APB_S-101", true},
	{0, 236, 236, "CAM_APB_S-102", true},
	{0, 237, 237, "CAM_APB_S-103", true},
	{0, 238, 238, "CAM_APB_S-104", true},
	{0, 239, 239, "CAM_APB_S-105", true},

	/* 240 */
	{0, 240, 240, "CAM_APB_S-106", true},
	{0, 241, 241, "CAM_APB_S-107", true},
	{0, 242, 242, "CAM_APB_S-108", true},
	{0, 243, 243, "CAM_APB_S-109", true},
	{0, 244, 244, "CAM_APB_S-110", true},
	{0, 245, 245, "CAM_APB_S-111", true},
	{0, 246, 246, "CAM_APB_S-112", true},
	{0, 247, 247, "CAM_APB_S-113", true},
	{0, 248, 248, "CAM_APB_S-114", true},
	{0, 249, 249, "CAM_APB_S-115", true},

	/* 250 */
	{0, 250, 250, "CAM_APB_S-116", true},
	{0, 251, 251, "CAM_APB_S-117", true},
	{0, 252, 252, "CAM_APB_S-118", true},
	{0, 253, 253, "CAM_APB_S-119", true},
	{0, 254, 254, "CAM_APB_S-120", true},
	{0, 255, 255, "CAM_APB_S-121", true},
	{1, 0, 256, "CAM_APB_S-122", true},
	{1, 1, 257, "CAM_APB_S-123", true},
	{1, 2, 258, "CAM_APB_S-124", true},
	{1, 3, 259, "CAM_APB_S-125", true},

	/* 260 */
	{1, 4, 260, "CAM_APB_S-126", true},
	{1, 5, 261, "CAM_APB_S-127", true},
	{1, 6, 262, "CAM_APB_S-128", true},
	{1, 7, 263, "CAM_APB_S-129", true},
	{1, 8, 264, "CAM_APB_S-130", true},
	{1, 9, 265, "CAM_APB_S-131", true},
	{1, 10, 266, "CAM_APB_S-132", true},
	{1, 11, 267, "CAM_APB_S-133", true},
	{1, 12, 268, "CAM_APB_S-134", true},
	{1, 13, 269, "CAM_APB_S-135", true},

	/* 270 */
	{1, 14, 270, "CAM_APB_S-136", true},
	{1, 15, 271, "CAM_APB_S-137", true},
	{1, 16, 272, "CAM_APB_S-138", true},
	{1, 17, 273, "CAM_APB_S-139", true},
	{1, 18, 274, "CAM_APB_S-140", true},
	{1, 19, 275, "CAM_APB_S-141", true},
	{1, 20, 276, "IMG1_APB_S-0", true},
	{1, 21, 277, "IMG1_APB_S-1", true},
	{1, 22, 278, "IMG1_APB_S-2", true},
	{1, 23, 279, "IMG1_APB_S-3", true},

	/* 280 */
	{1, 24, 280, "IMG1_APB_S-4", true},
	{1, 25, 281, "IMG1_APB_S-5", true},
	{1, 26, 282, "IMG1_APB_S-6", true},
	{1, 27, 283, "IMG1_APB_S-7", true},
	{1, 28, 284, "IMG1_APB_S-8", true},
	{1, 29, 285, "IMG1_APB_S-9", true},
	{1, 30, 286, "IMG1_APB_S-10", true},
	{1, 31, 287, "IMG1_APB_S-11", true},
	{1, 32, 288, "IMG1_APB_S-12", true},
	{1, 33, 289, "IMG1_APB_S-13", true},

	/* 290 */
	{1, 34, 290, "IMG1_APB_S-14", true},
	{1, 35, 291, "IMG1_APB_S-15", true},
	{1, 36, 292, "IMG1_APB_S-16", true},
	{1, 37, 293, "IMG1_APB_S-17", true},
	{1, 38, 294, "IMG1_APB_S-18", true},
	{1, 39, 295, "IMG1_APB_S-19", true},
	{1, 40, 296, "IMG1_APB_S-20", true},
	{1, 41, 297, "IMG1_APB_S-21", true},
	{1, 42, 298, "IMG1_APB_S-22", true},
	{1, 43, 299, "IMG1_APB_S-23", true},

	/* 300 */
	{1, 44, 300, "IMG1_APB_S-24", true},
	{1, 45, 301, "IMG1_APB_S-25", true},
	{1, 46, 302, "IMG1_APB_S-26", true},
	{1, 47, 303, "IMG1_APB_S-27", true},
	{1, 48, 304, "IMG1_APB_S-28", true},
	{1, 49, 305, "IMG1_APB_S-29", true},
	{1, 50, 306, "IMG1_APB_S-30", true},
	{1, 51, 307, "IMG1_APB_S-31", true},
	{1, 52, 308, "IMG1_APB_S-32", true},
	{1, 53, 309, "IMG1_APB_S-33", true},

	/* 310 */
	{1, 54, 310, "IMG1_APB_S-34", true},
	{1, 55, 311, "IMG1_APB_S-35", true},
	{1, 56, 312, "IMG1_APB_S-36", true},
	{1, 57, 313, "IMG1_APB_S-37", true},
	{1, 58, 314, "IMG1_APB_S-38", true},
	{1, 59, 315, "IMG1_APB_S-39", true},
	{1, 60, 316, "IMG1_APB_S-40", true},
	{1, 61, 317, "IMG1_APB_S-41", true},
	{1, 62, 318, "IMG1_APB_S-42", true},
	{1, 63, 319, "IMG1_APB_S-43", true},

	/* 320 */
	{1, 64, 320, "IMG1_APB_S-44", true},
	{1, 65, 321, "IMG1_APB_S-45", true},
	{1, 66, 322, "IMG1_APB_S-46", true},
	{1, 67, 323, "IMG1_APB_S-47", true},
	{1, 68, 324, "IMG1_APB_S-48", true},
	{1, 69, 325, "IMG1_APB_S-49", true},
	{1, 70, 326, "IMG1_APB_S-50", true},
	{1, 71, 327, "IMG1_APB_S-51", true},
	{1, 72, 328, "IMG1_APB_S-52", true},
	{1, 73, 329, "IMG1_APB_S-53", true},

	/* 330 */
	{1, 74, 330, "IMG1_APB_S-54", true},
	{1, 75, 331, "IMG1_APB_S-55", true},
	{1, 76, 332, "IMG1_APB_S-56", true},
	{1, 77, 333, "IMG1_APB_S-57", true},
	{1, 78, 334, "IMG1_APB_S-58", true},
	{1, 79, 335, "IMG1_APB_S-59", true},
	{1, 80, 336, "IMG1_APB_S-60", true},
	{1, 81, 337, "IMG1_APB_S-61", true},
	{1, 82, 338, "IMG1_APB_S-62", true},
	{1, 83, 339, "IMG1_APB_S-63", true},

	/* 340 */
	{1, 84, 340, "IMG1_APB_S-64", true},
	{1, 85, 341, "IMG1_APB_S-65", true},
	{1, 86, 342, "IMG1_APB_S-66", true},
	{1, 87, 343, "IMG1_APB_S-67", true},
	{1, 88, 344, "IMG1_APB_S-68", true},
	{1, 89, 345, "IMG1_APB_S-69", true},
	{1, 90, 346, "IMG1_APB_S-70", true},
	{1, 91, 347, "IMG1_APB_S-71", true},
	{1, 92, 348, "IMG1_APB_S-72", true},
	{1, 93, 349, "IMG1_APB_S-73", true},

	/* 350 */
	{1, 94, 350, "IMG1_APB_S-74", true},
	{1, 95, 351, "IMG1_APB_S-75", true},
	{1, 96, 352, "IMG1_APB_S-76", true},
	{1, 97, 353, "IMG1_APB_S-77", true},
	{1, 98, 354, "IMG1_APB_S-78", true},
	{1, 99, 355, "IMG1_APB_S-79", true},
	{1, 100, 356, "IMG1_APB_S-80", true},
	{1, 101, 357, "IMG1_APB_S-81", true},
	{1, 102, 358, "IMG1_APB_S-82", true},
	{1, 103, 359, "IMG1_APB_S-83", true},

	/* 360 */
	{1, 104, 360, "IMG1_APB_S-84", true},
	{1, 105, 361, "IMG1_APB_S-85", true},
	{1, 106, 362, "IMG1_APB_S-86", true},
	{1, 107, 363, "IMG1_APB_S-87", true},
	{1, 108, 364, "IMG1_APB_S-88", true},
	{1, 109, 365, "IMG1_APB_S-89", true},
	{1, 110, 366, "IMG1_APB_S-90", true},
	{1, 111, 367, "IMG1_APB_S-91", true},
	{1, 112, 368, "IMG1_APB_S-92", true},
	{1, 113, 369, "IMG1_APB_S-93", true},

	/* 370 */
	{1, 114, 370, "IMG1_APB_S-94", true},
	{1, 115, 371, "IMG1_APB_S-95", true},
	{1, 116, 372, "IMG1_APB_S-96", true},
	{1, 117, 373, "IMG1_APB_S-97", true},
	{1, 118, 374, "IMG1_APB_S-98", true},
	{1, 119, 375, "IMG1_APB_S-99", true},
	{1, 120, 376, "IMG1_APB_S-100", true},
	{1, 121, 377, "IMG1_APB_S-101", true},
	{1, 122, 378, "IMG1_APB_S-102", true},
	{1, 123, 379, "IMG1_APB_S-103", true},

	/* 380 */
	{1, 124, 380, "IMG1_APB_S-104", true},
	{1, 125, 381, "IMG1_APB_S-105", true},
	{1, 126, 382, "IMG1_APB_S-106", true},
	{1, 127, 383, "IMG1_APB_S-107", true},
	{1, 128, 384, "IMG1_APB_S-108", true},
	{1, 129, 385, "IMG1_APB_S-109", true},
	{1, 130, 386, "IMG1_APB_S-110", true},
	{1, 131, 387, "IMG1_APB_S-111", true},
	{1, 132, 388, "IMG1_APB_S-112", true},
	{1, 133, 389, "IMG1_APB_S-113", true},

	/* 390 */
	{1, 134, 390, "IMG1_APB_S-114", true},
	{1, 135, 391, "IMG1_APB_S-115", true},
	{1, 136, 392, "IMG1_APB_S-116", true},
	{1, 137, 393, "IMG1_APB_S-117", true},
	{1, 138, 394, "IMG1_APB_S-118", true},
	{1, 139, 395, "IMG1_APB_S-119", true},
	{1, 140, 396, "IMG1_APB_S-120", true},
	{1, 141, 397, "IMG1_APB_S-121", true},
	{1, 142, 398, "IMG1_APB_S-122", true},
	{1, 143, 399, "IMG1_APB_S-123", true},

	/* 400 */
	{1, 144, 400, "IMG1_APB_S-124", true},
	{1, 145, 401, "IMG1_APB_S-125", true},
	{1, 146, 402, "IMG1_APB_S-126", true},
	{1, 147, 403, "IMG1_APB_S-127", true},
	{1, 148, 404, "IMG1_APB_S-128", true},
	{1, 149, 405, "IMG1_APB_S-129", true},
	{1, 150, 406, "IMG1_APB_S-130", true},
	{1, 151, 407, "IMG1_APB_S-131", true},
	{1, 152, 408, "IMG1_APB_S-132", true},
	{1, 153, 409, "IMG1_APB_S-133", true},

	/* 410 */
	{1, 154, 410, "IMG1_APB_S-134", true},
	{1, 155, 411, "IMG1_APB_S-135", true},
	{1, 156, 412, "IMG1_APB_S-136", true},
	{1, 157, 413, "IMG1_APB_S-137", true},
	{1, 158, 414, "IMG1_APB_S-138", true},
	{1, 159, 415, "IMG1_APB_S-139", true},
	{1, 160, 416, "IMG1_APB_S-140", true},
	{1, 161, 417, "IMG1_APB_S-141", true},
	{1, 162, 418, "IMG1_APB_S-142", true},
	{1, 163, 419, "IMG1_APB_S-143", true},

	/* 420 */
	{1, 164, 420, "IMG1_APB_S-144", true},
	{1, 165, 421, "IMG1_APB_S-145", true},
	{1, 166, 422, "IMG1_APB_S-146", true},
	{1, 167, 423, "IMG1_APB_S-147", true},
	{1, 168, 424, "IMG1_APB_S-148", true},
	{1, 169, 425, "IMG1_APB_S-149", true},
	{1, 170, 426, "IMG1_APB_S-150", true},
	{1, 171, 427, "IMG1_APB_S-151", true},
	{1, 172, 428, "IMG1_APB_S-152", true},
	{1, 173, 429, "IMG1_APB_S-153", true},

	/* 430 */
	{1, 174, 430, "IMG1_APB_S-154", true},
	{1, 175, 431, "IMG1_APB_S-155", true},
	{1, 176, 432, "IMG1_APB_S-156", true},
	{1, 177, 433, "IMG1_APB_S-157", true},
	{1, 178, 434, "IMG1_APB_S-158", true},
	{1, 179, 435, "IMG1_APB_S-159", true},
	{1, 180, 436, "IMG1_APB_S-160", true},
	{1, 181, 437, "IMG1_APB_S-161", true},
	{1, 182, 438, "IMG1_APB_S-162", true},
	{1, 183, 439, "IMG1_APB_S-163", true},

	/* 440 */
	{1, 184, 440, "IMG1_APB_S-164", true},
	{1, 185, 441, "IMG1_APB_S-165", true},
	{1, 186, 442, "IMG1_APB_S-166", true},
	{1, 187, 443, "IMG1_APB_S-167", true},
	{1, 188, 444, "IMG1_APB_S-168", true},
	{1, 189, 445, "IMG1_APB_S-169", true},
	{1, 190, 446, "IMG1_APB_S-170", true},
	{1, 191, 447, "IMG1_APB_S-171", true},
	{1, 192, 448, "IMG1_APB_S-172", true},
	{1, 193, 449, "IMG1_APB_S-173", true},

	/* 450 */
	{1, 194, 450, "IMG1_APB_S-174", true},
	{1, 195, 451, "IMG1_APB_S-175", true},
	{1, 196, 452, "IMG1_APB_S-176", true},
	{1, 197, 453, "IMG1_APB_S-177", true},
	{1, 198, 454, "IMG1_APB_S-178", true},
	{1, 199, 455, "IMG1_APB_S-179", true},
	{1, 200, 456, "IMG1_APB_S-180", true},
	{1, 201, 457, "IMG1_APB_S-181", true},
	{1, 202, 458, "IMG1_APB_S-182", true},
	{1, 203, 459, "IMG1_APB_S-183", true},

	/* 460 */
	{1, 204, 460, "IMG1_APB_S-184", true},
	{1, 205, 461, "IMG1_APB_S-185", true},
	{1, 206, 462, "IMG1_APB_S-186", true},
	{1, 207, 463, "DISP_APB_S-0", true},
	{1, 208, 464, "DISP_APB_S-1", true},
	{1, 209, 465, "DISP_APB_S-2", true},
	{1, 210, 466, "DISP_APB_S-3", true},
	{1, 211, 467, "DISP_APB_S-4", true},
	{1, 212, 468, "DISP_APB_S-5", true},
	{1, 213, 469, "DISP_APB_S-6", true},

	/* 470 */
	{1, 214, 470, "DISP_APB_S-7", true},
	{1, 215, 471, "DISP_APB_S-8", true},
	{1, 216, 472, "DISP_APB_S-9", true},
	{1, 217, 473, "DISP_APB_S-10", true},
	{1, 218, 474, "DISP_APB_S-11", true},
	{1, 219, 475, "DISP_APB_S-12", true},
	{1, 220, 476, "DISP_APB_S-13", true},
	{1, 221, 477, "DISP_APB_S-14", true},
	{1, 222, 478, "DISP_APB_S-15", true},
	{1, 223, 479, "DISP_APB_S-16", true},

	/* 480 */
	{1, 224, 480, "DISP_APB_S-17", true},
	{1, 225, 481, "DISP_APB_S-18", true},
	{1, 226, 482, "DISP_APB_S-19", true},
	{1, 227, 483, "DISP_APB_S-20", true},
	{1, 228, 484, "DISP_APB_S-21", true},
	{1, 229, 485, "DISP_APB_S-22", true},
	{1, 230, 486, "DISP_APB_S-23", true},
	{1, 231, 487, "DISP_APB_S-24", true},
	{1, 232, 488, "DISP_APB_S-25", true},
	{1, 233, 489, "DISP_APB_S-26", true},

	/* 490 */
	{1, 234, 490, "DISP_APB_S-27", true},
	{1, 235, 491, "DISP_APB_S-28", true},
	{1, 236, 492, "DISP_APB_S-29", true},
	{1, 237, 493, "DISP_APB_S-30", true},
	{1, 238, 494, "DISP_APB_S-31", true},
	{1, 239, 495, "DISP_APB_S-32", true},
	{1, 240, 496, "DISP_APB_S-33", true},
	{1, 241, 497, "DISP_APB_S-34", true},
	{1, 242, 498, "DISP_APB_S-35", true},
	{1, 243, 499, "DISP_APB_S-36", true},

	/* 500 */
	{1, 244, 500, "DISP_APB_S-37", true},
	{1, 245, 501, "DISP_APB_S-38", true},
	{1, 246, 502, "DISP_APB_S-39", true},
	{1, 247, 503, "DISP_APB_S-40", true},
	{1, 248, 504, "DISP_APB_S-41", true},
	{1, 249, 505, "DISP_APB_S-42", true},
	{1, 250, 506, "MDP_APB_S-0", true},
	{1, 251, 507, "MDP_APB_S-1", true},
	{1, 252, 508, "MDP_APB_S-2", true},
	{1, 253, 509, "MDP_APB_S-3", true},

	/* 510 */
	{1, 254, 510, "MDP_APB_S-4", true},
	{1, 255, 511, "MDP_APB_S-5", true},
	{2, 0, 512, "MDP_APB_S-6", true},
	{2, 1, 513, "MDP_APB_S-7", true},
	{2, 2, 514, "MDP_APB_S-8", true},
	{2, 3, 515, "MDP_APB_S-9", true},
	{2, 4, 516, "MDP_APB_S-10", true},
	{2, 5, 517, "MDP_APB_S-11", true},
	{2, 6, 518, "MDP_APB_S-12", true},
	{2, 7, 519, "MDP_APB_S-13", true},

	/* 520 */
	{2, 8, 520, "MDP_APB_S-14", true},
	{2, 9, 521, "MDP_APB_S-15", true},
	{2, 10, 522, "MDP_APB_S-16", true},
	{2, 11, 523, "MDP_APB_S-17", true},
	{2, 12, 524, "MDP_APB_S-18", true},
	{2, 13, 525, "MDP_APB_S-19", true},
	{2, 14, 526, "MDP_APB_S-20", true},
	{2, 15, 527, "MDP_APB_S-21", true},
	{2, 16, 528, "MDP_APB_S-22", true},
	{2, 17, 529, "MDP_APB_S-23", true},

	/* 530 */
	{2, 18, 530, "MDP_APB_S-24", true},
	{2, 19, 532, "HRE_APB_S", true},
	{2, 20, 538, "DAPC_AO_S", true},
	{2, 21, 539, "BCRM_AO_S", true},
	{2, 22, 540, "DEBUG_CTL_AO_S", true},
	{-1, -1, 541, "OOB_way_en", true},
	{-1, -1, 542, "OOB_way_en", true},
	{-1, -1, 543, "OOB_way_en", true},
	{-1, -1, 544, "OOB_way_en", true},
	{-1, -1, 545, "OOB_way_en", true},

	{-1, -1, 546, "OOB_way_en", true},
	{-1, -1, 547, "OOB_way_en", true},
	{-1, -1, 548, "OOB_way_en", true},
	{-1, -1, 549, "OOB_way_en", true},
	{-1, -1, 550, "OOB_way_en", true},
	{-1, -1, 551, "OOB_way_en", true},
	{-1, -1, 552, "OOB_way_en", true},
	{-1, -1, 553, "OOB_way_en", true},
	{-1, -1, 554, "OOB_way_en", true},
	{-1, -1, 555, "OOB_way_en", true},

	{-1, -1, 556, "OOB_way_en", true},
	{-1, -1, 557, "Decode_error", true},
	{-1, -1, 558, "Decode_error", true},
	{-1, -1, 559, "Decode_error", true},
	{-1, -1, 560, "Decode_error", true},
	{-1, -1, 561, "GCE_D_APB_S", false},
	{-1, -1, 562, "GCE_M_APB_S", false},
	{-1, -1, 563, "GCE_M2_APB_S", false},
	{-1, -1, 564, "DEVICE_APC_MM_AO", false},
	{-1, -1, 565, "DEVICE_APC_MM_PDN", false},
};

static const struct mtk_device_info mt6881_devices_mmup[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "p_main_BCRM", true},
	{0, 1, 1, "p_main_DEBUG", true},
	{0, 2, 2, "p_main_DEVAPCAO", true},
	{0, 3, 3, "p_main_DEVAPC", true},
	{0, 4, 4, "p_par_top", true},
	{0, 5, 5, "pslv_clk_ctrl", true},
	{0, 6, 6, "pslv_cfgreg", true},
	{0, 7, 7, "pslv_uart", true},
	{0, 8, 8, "pslv_uart1", true},
	{0, 9, 9, "pslv_cfg_core0", true},

	/* 10 */
	{0, 10, 10, "pslv_dma_core0", true},
	{0, 11, 11, "pslv_irq_core0", true},
	{0, 12, 12, "pslv_tmr_core0", true},
	{0, 13, 13, "pslv_dbg_core0", true},
	{0, 14, 14, "pbus_tracker", true},
	{0, 15, 15, "pCACHE0", true},
	{0, 16, 16, "pslv_cfgreg_sec", true},
	{0, 17, 17, "p_mbox0", true},
	{0, 18, 18, "p_mbox1", true},
	{0, 19, 19, "p_mbox2", true},

	/* 20 */
	{0, 20, 20, "p_mbox3", true},
	{0, 21, 21, "p_mbox4", true},
	{0, 22, 22, "pslv_vmm_dvfsrc_top", true},
	{0, 23, 23, "pslv_vmm_DPSW_top", true},
	{0, 24, 24, "pslv_vdisp_dvfsrc_top", true},
	{0, 25, 25, "pslv_vdisp_DPSW_top", true},
	{0, 26, 26, "pslv_hfrp_pwr_ctrl", true},
	{0, 27, 27, "p_main_BCRM_PDN", true},
	{-1, -1, 28, "OOB_way_en", true},
	{-1, -1, 29, "OOB_way_en", true},

	{-1, -1, 30, "OOB_way_en", true},
	{-1, -1, 31, "OOB_way_en", true},
	{-1, -1, 32, "OOB_way_en", true},
	{-1, -1, 33, "OOB_way_en", true},
	{-1, -1, 34, "OOB_way_en", true},
	{-1, -1, 35, "OOB_way_en", true},
	{-1, -1, 36, "OOB_way_en", true},
	{-1, -1, 37, "OOB_way_en", true},
	{-1, -1, 38, "OOB_way_en", true},
	{-1, -1, 39, "OOB_way_en", true},

	{-1, -1, 40, "OOB_way_en", true},
	{-1, -1, 41, "OOB_way_en", true},
	{-1, -1, 42, "OOB_way_en", true},
	{-1, -1, 43, "OOB_way_en", true},
	{-1, -1, 44, "OOB_way_en", true},
	{-1, -1, 45, "OOB_way_en", true},
	{-1, -1, 46, "OOB_way_en", true},
	{-1, -1, 47, "OOB_way_en", true},
	{-1, -1, 48, "OOB_way_en", true},
	{-1, -1, 49, "OOB_way_en", true},

	{-1, -1, 50, "OOB_way_en", true},
	{-1, -1, 51, "OOB_way_en", true},
	{-1, -1, 52, "OOB_way_en", true},
	{-1, -1, 53, "OOB_way_en", true},
	{-1, -1, 54, "OOB_way_en", true},
	{-1, -1, 55, "OOB_way_en", true},
	{-1, -1, 56, "OOB_way_en", true},
	{-1, -1, 57, "OOB_way_en", true},
	{-1, -1, 58, "Decode_error", true},
	{-1, -1, 59, "Decode_error", true},

	{-1, -1, 60, "DEVICE_APC_MMUP_AO", false},
	{-1, -1, 61, "DEVICE_APC_MMUP_PDN", false},
};

static const struct mtk_device_info mt6881_devices_gpu[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "MFG_S_G3D_CONFIG", true},
	{0, 1, 1, "MFG_S_G3D_SECURE_CONFIG", true},
	{0, 2, 2, "MFG_S_IP", true},
	{0, 3, 3, "MFG_S_MALI_AXUSER", true},
	{0, 4, 4, "MFG_S_SMMU_CONFIG", true},
	{0, 5, 5, "MFG_S_DVFS_HINT", true},
	{0, 6, 6, "MFG_S_MPU", true},
	{-1, -1, 7, "DEVICE_APC_GPU_AO", false},
	{-1, -1, 8, "DEVICE_APC_GPU_PDN", false},
};

static const struct mtk_device_info mt6881_devices_gpu1[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "MFG_S_RPC", true},
	{0, 1, 1, "MFG_S_DFD_V3P6", true},
	{0, 2, 2, "MFG_S_VGPU_DEVAPC_AO_WRAPPER", true},
	{0, 3, 3, "MFG_S_VCORE_DEVAPC_AO_WRAPPER", true},
	{0, 4, 4, "MFG_S_VCORE_AO_DBG_TRACKER_SECU", true},
	{0, 5, 5, "MFG_S_VCORE_AO_DBG_TRACKER_NON_SECU", true},
	{0, 6, 6, "MFG_S_PLL", true},
	{0, 7, 7, "MFG_S_VCORE_AO_CONFIG", true},
	{0, 8, 8, "MFG_S_VGPU_DEVAPC_WRAPPER", true},
	{0, 9, 9, "MFG_S_VCORE_DEVAPC_WRAPPER", true},

	/* 10 */
	{0, 10, 10, "MFG_S_GPUEB_TCM_MPOOL", true},
	{0, 11, 11, "MFG_S_GPUEB_CKCTRL", true},
	{0, 12, 12, "MFG_S_GPUEB_INTC", true},
	{0, 13, 13, "MFG_S_GPUEB_DMA", true},
	{0, 14, 14, "MFG_S_GPUEB_CFG", true},
	{0, 15, 15, "MFG_S_VCORE_DEVAPC_GRP0", true},
	{0, 16, 16, "MFG_S_GPUEB_OCD_BIU", true},
	{0, 17, 17, "MFG_S_GPUEB_MBOX", true},
	{0, 18, 18, "MFG_S_TSFDC_CTRL", true},
	{0, 19, 19, "MFG_S_GPUEB_DBG_TRACKER_SECU", true},

	/* 20 */
	{0, 20, 20, "MFG_S_GPUEB_DBG_TRACKER_NON_SECU", true},
	{0, 21, 21, "MFG_S_VCORE_ASYNC_DBG_TRACKER_SECU", true},
	{0, 22, 22, "MFG_S_VCORE_ASYNC_DBG_TRACKER_NON_SECU", true},
	{-1, -1, 23, "DEVICE_APC_GPU_AO1", false},
	{-1, -1, 24, "DEVICE_APC_GPU_PDN1", false},
};

enum DEVAPC_VIO_SLAVE_NUM {
	VIO_SLAVE_NUM_INFRA = ARRAY_SIZE(mt6881_devices_infra),
	VIO_SLAVE_NUM_INFRA1 = ARRAY_SIZE(mt6881_devices_infra1),
	VIO_SLAVE_NUM_PERI_PAR = ARRAY_SIZE(mt6881_devices_peri_par),
	VIO_SLAVE_NUM_VLP = ARRAY_SIZE(mt6881_devices_vlp),
#if ENABLE_DEVAPC_ADSP
	VIO_SLAVE_NUM_ADSP = ARRAY_SIZE(mt6881_devices_adsp),
#endif
	VIO_SLAVE_NUM_MMINFRA = ARRAY_SIZE(mt6881_devices_mminfra),
	VIO_SLAVE_NUM_MMUP = ARRAY_SIZE(mt6881_devices_mmup),
	VIO_SLAVE_NUM_GPU = ARRAY_SIZE(mt6881_devices_gpu),
	VIO_SLAVE_NUM_GPU1 = ARRAY_SIZE(mt6881_devices_gpu1),
};

#endif /* __DEVAPC_MT6881_H__ */
