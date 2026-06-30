/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __SENSOR_CFG_SEC_H__
#define __SENSOR_CFG_SEC_H__

#include "seninf_drv_csi_info.h"
#include "kd_camera_feature.h"

CUSTOM_CFG_SECURE sensor_cfg_sec_from_seninf(SENINF_ENUM seninf);
CUSTOM_CFG_SECURE sensor_cfg_sec_from_sensor_idx(IMGSENSOR_SENSOR_IDX sensor_idx);

#endif

