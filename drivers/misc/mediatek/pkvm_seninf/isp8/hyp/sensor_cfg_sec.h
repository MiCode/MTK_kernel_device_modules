/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __SENSOR_CFG_SEC_H__
#define __SENSOR_CFG_SEC_H__

#include "secure_port.h"
#include "seninf_drv_csi_info.h"

CUSTOM_CFG_SECURE sensor_cfg_sec_from_seninf(SENINF_ASYNC_ENUM seninf);

int sensor_cfg_multi_sec_port_front(CUSTOM_CFG_CSI_PORT secure_port);
int sensor_cfg_multi_sec_port_rear(CUSTOM_CFG_CSI_PORT secure_port);
int sensor_cfg_single_sec_port(CUSTOM_CFG_CSI_PORT secure_port);

#endif
