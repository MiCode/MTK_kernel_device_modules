// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include "camera_custom_imgsensor_cfg.h"
#include "seninf_drv_csi_info.h"

#define SENINF_CSI_TYPE_MIPI(port, seninf)	{port, seninf, MIPI_SENSOR}
#define SENINF_CSI_TYPE_SERIAL(port, seninf)	{port, seninf, SERIAL_SENSOR}
#define SENINF_CSI_TYPE_PARALLEL(port, seninf)	{port, seninf, PARALLEL_SENSOR}

SENINF_CSI_INFO seninfCSITypeInfo[CUSTOM_CFG_CSI_PORT_MAX_NUM] = {
	SENINF_CSI_TYPE_MIPI(CUSTOM_CFG_CSI_PORT_0,  SENINF_1),
	SENINF_CSI_TYPE_MIPI(CUSTOM_CFG_CSI_PORT_1,  SENINF_3),
	SENINF_CSI_TYPE_MIPI(CUSTOM_CFG_CSI_PORT_2,  SENINF_5),
	SENINF_CSI_TYPE_MIPI(CUSTOM_CFG_CSI_PORT_0A, SENINF_1),
	SENINF_CSI_TYPE_MIPI(CUSTOM_CFG_CSI_PORT_0B, SENINF_2),
};

