/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */
#ifndef __ISP_HYP_DEF_H__
#define __ISP_HYP_DEF_H__

#define camsys_check_value 0x43
#define sensor_check_value 0x53

typedef enum {
	ISP_RETURN_SUCCESS = 0,
	ISP_RETURN_ERROR = -1,
	ISP_RETURN_ERROR_MAPPING_FAIL = -2,
} ISP_RETURN;

typedef enum {
	CAMA = 0,
	CAMB,
	CAMC,
	CAM_UNKNOWN
} SECCAM_HWMODULE;

typedef enum{
	SEC_CAM_A   = 0,
	SEC_CAM_B,
	SEC_CAM_C,
	SEC_CAM_MAX,  //3
} SEC_ISP_HW_MODULE;

typedef struct {
	uint32_t SecTG;
	uint32_t Sec_status;
} SecMgr_CamInfo;

#endif
