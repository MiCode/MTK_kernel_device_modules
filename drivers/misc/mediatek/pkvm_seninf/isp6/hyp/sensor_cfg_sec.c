// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include "sensor_cfg_sec.h"

extern SENINF_CSI_INFO seninfCSITypeInfo[CUSTOM_CFG_CSI_PORT_MAX_NUM];

CUSTOM_CFG_SECURE sensor_cfg_sec_from_seninf(SENINF_ENUM seninf)
{
	int i = 0;
	SENINF_CSI_INFO	*pcsi_info = NULL;
	CUSTOM_CFG	*pcust_cfg = getCustomConfig(IMGSENSOR_SENSOR_IDX_MIN_NUM);

	for (i = 0; i < CUSTOM_CFG_CSI_PORT_MAX_NUM; i++) {
		if(seninfCSITypeInfo[i].seninf == seninf) {
			pcsi_info = &seninfCSITypeInfo[i];
			break;
		}
	}

	if (pcsi_info == NULL || pcsi_info->port >= CUSTOM_CFG_CSI_PORT_MAX_NUM ||
	pcsi_info->port < CUSTOM_CFG_CSI_PORT_0)
		return CUSTOM_CFG_SECURE_NONE;

	if (pcust_cfg == NULL)
		return CUSTOM_CFG_SECURE_NONE;

	while (pcust_cfg->sensorIdx != IMGSENSOR_SENSOR_IDX_NONE && pcust_cfg->port != pcsi_info->port)
		pcust_cfg++;

	if (pcust_cfg->sensorIdx == IMGSENSOR_SENSOR_IDX_NONE)
		return CUSTOM_CFG_SECURE_NONE;

	return pcust_cfg->secure;
}

CUSTOM_CFG_SECURE sensor_cfg_sec_from_sensor_idx(IMGSENSOR_SENSOR_IDX sensor_idx)
{
	CUSTOM_CFG *pcust_cfg = getCustomConfig((IMGSENSOR_SENSOR_IDX)sensor_idx);

	if (pcust_cfg == NULL)
		return CUSTOM_CFG_SECURE_NONE;

	return pcust_cfg->secure;
}

