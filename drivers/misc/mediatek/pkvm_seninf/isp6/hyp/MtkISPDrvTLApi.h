/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef ISPCAMERA_TA_API_H_
#define ISPCAMERA_TA_API_H_

#define CAM_NUM 3

typedef enum {
	TG_A,
	TG_B,
	TG_C,
	TG_UNKNOWN = 0xff,
} SECCAM_TG;

typedef struct {
	uint32_t CamModule;
	uint32_t TwinCamModule;
	uint32_t SecTG;
	uint32_t DevID;
	uint64_t chk_handle_pa;
} SecMgr_CamInfo;

struct Sec_CamState {
	SecMgr_CamInfo cam_info[CAM_NUM];
};
#endif
