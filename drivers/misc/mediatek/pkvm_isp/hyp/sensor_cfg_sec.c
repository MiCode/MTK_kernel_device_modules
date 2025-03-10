// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include "pkvm_isp_hyp.h"
#include "secure_port.h"
#include "seninf_drv_csi_info.h"
#include "seninf_tee_reg.h"
#include "sensor_cfg_sec.h"

extern SENINF_CSI_INFO seninfCSITypeInfo[CUSTOM_CFG_CSI_PORT_MAX_NUM];

unsigned int g_multi_secure_csi_port_front = CUSTOM_CFG_CSI_PORT_NONE;
unsigned int g_multi_secure_csi_port_rear = CUSTOM_CFG_CSI_PORT_NONE;
unsigned int g_single_secure_csi_port = CUSTOM_CFG_CSI_PORT_NONE;

int sensor_cfg_multi_sec_port_front(CUSTOM_CFG_CSI_PORT secure_port)
{
	if (secure_port >= CUSTOM_CFG_CSI_PORT_MAX_NUM) {
		g_multi_secure_csi_port_front = CUSTOM_CFG_CSI_PORT_NONE;
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "[ERROR] input");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "secure_port:");
		CALL_FROM_OPS(putx64, secure_port);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "g_multi_secure_csi_port_front:");
		CALL_FROM_OPS(putx64, g_multi_secure_csi_port_front);
		return -1;
	}
	g_multi_secure_csi_port_front = secure_port;
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "input");
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "secure_port:");
	CALL_FROM_OPS(putx64, secure_port);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "g_multi_secure_csi_port_front:");
	CALL_FROM_OPS(putx64, g_multi_secure_csi_port_front);

	return 0;
}

int sensor_cfg_multi_sec_port_rear(CUSTOM_CFG_CSI_PORT secure_port)
{
	if (secure_port >= CUSTOM_CFG_CSI_PORT_MAX_NUM) {
		g_multi_secure_csi_port_rear = CUSTOM_CFG_CSI_PORT_NONE;
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "[ERROR] input");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "secure_port:");
		CALL_FROM_OPS(putx64, secure_port);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "g_multi_secure_csi_port_rear:");
		CALL_FROM_OPS(putx64, g_multi_secure_csi_port_rear);
		return -1;
	}
	g_multi_secure_csi_port_rear = secure_port;
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "input");
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "secure_port:");
	CALL_FROM_OPS(putx64, secure_port);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "g_multi_secure_csi_port_rear:");
	CALL_FROM_OPS(putx64, g_multi_secure_csi_port_rear);

	return 0;
}

int sensor_cfg_single_sec_port(CUSTOM_CFG_CSI_PORT secure_port)
{
	if (secure_port >= CUSTOM_CFG_CSI_PORT_MAX_NUM) {
		g_single_secure_csi_port = CUSTOM_CFG_CSI_PORT_NONE;
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "[ERROR] input");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "secure_port:");
		CALL_FROM_OPS(putx64, secure_port);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "g_single_secure_csi_port:");
		CALL_FROM_OPS(putx64, g_single_secure_csi_port);
		return -1;
	}
	g_single_secure_csi_port = secure_port;
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "input");
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "secure_port:");
	CALL_FROM_OPS(putx64, secure_port);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "g_single_secure_csi_port:");
	CALL_FROM_OPS(putx64, g_single_secure_csi_port);

	return 0;
}

CUSTOM_CFG_SECURE sensor_cfg_sec_from_seninf(SENINF_ASYNC_ENUM seninf)
{
	int i = 0;
	SENINF_CSI_INFO *pcsi_info = NULL;

	for (i = 0; i < CUSTOM_CFG_CSI_PORT_MAX_NUM; i++) {
		if (seninfCSITypeInfo[i].seninf == seninf) {
			pcsi_info = &seninfCSITypeInfo[i];
			break;
		}
	}
	if (pcsi_info == NULL || pcsi_info->port >= CUSTOM_CFG_CSI_PORT_MAX_NUM ||
		pcsi_info->port < CUSTOM_CFG_CSI_PORT_0)
		return CUSTOM_CFG_SECURE_NONE;

#ifdef MULTI_SENSOR_INDIVIDUAL_SUPPORT
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "ISP8 MULTI");
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "pcsi_info->port:");
	CALL_FROM_OPS(putx64, pcsi_info->port);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "g_multi_secure_csi_port_front:");
	CALL_FROM_OPS(putx64, g_multi_secure_csi_port_front);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "g_multi_secure_csi_port_rear:");
	CALL_FROM_OPS(putx64, g_multi_secure_csi_port_rear);

	if (g_multi_sensor_individual_flag &&
		(g_multi_sensor_current_port != pcsi_info->port)) {
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "pcsi_info->port:");
		CALL_FROM_OPS(putx64, pcsi_info->port);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "g_multi_sensor_individual_flag:");
		CALL_FROM_OPS(putx64, g_multi_sensor_individual_flag);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "g_multi_sensor_current_port:");
		CALL_FROM_OPS(putx64, g_multi_sensor_current_port);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "ERROR support one secure cam at the same time");
		return CUSTOM_CFG_SECURE_NONE;
	}

	if (pcsi_info->port == g_multi_secure_csi_port_front ||
		pcsi_info->port == g_multi_secure_csi_port_rear) {
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "pcsi_info->port:");
		CALL_FROM_OPS(putx64, pcsi_info->port);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "g_multi_sensor_individual_flag:");
		CALL_FROM_OPS(putx64, g_multi_sensor_individual_flag);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "g_multi_sensor_current_port:");
		CALL_FROM_OPS(putx64, g_multi_sensor_current_port);
		g_multi_sensor_individual_flag = 1;
		g_multi_sensor_current_port = pcsi_info->port;
		return CUSTOM_CFG_SECURE_M0;
	}
#else
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "ISP8 SINGLE");
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "pcsi_info->port:");
	CALL_FROM_OPS(putx64, pcsi_info->port);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "g_single_secure_csi_port:");
	CALL_FROM_OPS(putx64, g_single_secure_csi_port);

	if (pcsi_info->port == g_single_secure_csi_port)
		return CUSTOM_CFG_SECURE_M0;
#endif

	return CUSTOM_CFG_SECURE_NONE;
}
