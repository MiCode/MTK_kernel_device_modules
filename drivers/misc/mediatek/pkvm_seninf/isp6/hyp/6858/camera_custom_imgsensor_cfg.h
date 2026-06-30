/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef _CAMERA_CUSTOM_IMGSENSOR_CFG_
#define _CAMERA_CUSTOM_IMGSENSOR_CFG_

#include <linux/stddef.h>
#include "kd_camera_feature.h"

/*******************************************************************************
 * Sensor mclk usage
 *******************************************************************************/
typedef enum {
	CUSTOM_CFG_MCLK_1 = 0x0,	//mclk1
	CUSTOM_CFG_MCLK_2,		//mclk2
	CUSTOM_CFG_MCLK_3,		//mclk3
	CUSTOM_CFG_MCLK_4,
	CUSTOM_CFG_MCLK_5,
	CUSTOM_CFG_MCLK_MAX_NUM,
	CUSTOM_CFG_MCLK_NONE
} CUSTOM_CFG_MCLK;

/*******************************************************************************
 * MIPI sensor pad usage
 *******************************************************************************/
typedef enum {
	CUSTOM_CFG_CSI_PORT_0 = 0x0,	// 4D1C
	CUSTOM_CFG_CSI_PORT_1,		// 4D1C
	CUSTOM_CFG_CSI_PORT_2,		// 4D1C
	CUSTOM_CFG_CSI_PORT_3,		// 4D1C
	CUSTOM_CFG_CSI_PORT_0A,		// 2D1C or 2Trio
	CUSTOM_CFG_CSI_PORT_0B,		// 2D1C or 2Trio
	CUSTOM_CFG_CSI_PORT_1A,		// 2D1C or 2Trio
	CUSTOM_CFG_CSI_PORT_1B,		// 2D1C or 2Trio
	CUSTOM_CFG_CSI_PORT_2A,
	CUSTOM_CFG_CSI_PORT_2B,
	CUSTOM_CFG_CSI_PORT_3A,
	CUSTOM_CFG_CSI_PORT_3B,
	CUSTOM_CFG_CSI_PORT_MAX_NUM,
	CUSTOM_CFG_CSI_PORT_NONE	//for non-MIPI sensor
} CUSTOM_CFG_CSI_PORT;

/*******************************************************************************
 * Sensor Placement Facing Direction
 *
 *   CUSTOM_CFG_DIR_REAR  : Back side
 *   CUSTOM_CFG_DIR_FRONT : Front side (LCD side)
 *******************************************************************************/
typedef enum {
	CUSTOM_CFG_DIR_REAR,
	CUSTOM_CFG_DIR_FRONT,
	CUSTOM_CFG_DIR_MAX_NUM,
	CUSTOM_CFG_DIR_NONE
} CUSTOM_CFG_DIR;

/*******************************************************************************
 * Sensor Input Data Bit Order
 *
 *   CUSTOM_CFG_BITORDER_9_2 : raw data input [9:2]
 *   CUSTOM_CFG_BITORDER_7_0 : raw data input [7:0]
 *******************************************************************************/
typedef enum {
	CUSTOM_CFG_BITORDER_9_2,
	CUSTOM_CFG_BITORDER_7_0,
	CUSTOM_CFG_BITORDER_MAX_NUM,
	CUSTOM_CFG_BITORDER_NONE
} CUSTOM_CFG_BITORDER;

typedef enum {
	CUSTOM_CFG_SECURE_NONE,
	CUSTOM_CFG_SECURE_M0
} CUSTOM_CFG_SECURE;

typedef struct {
	IMGSENSOR_SENSOR_IDX	sensorIdx;
	CUSTOM_CFG_MCLK		mclk;
	CUSTOM_CFG_CSI_PORT	port;
	CUSTOM_CFG_DIR		dir;
	CUSTOM_CFG_BITORDER	bitOrder;
	unsigned int		orientation;
	unsigned int		horizontalFov;
	unsigned int		verticalFov;
	CUSTOM_CFG_SECURE	secure;
} CUSTOM_CFG;

/*******************************************************************************
 * Get Custom Configuration
 *******************************************************************************/
CUSTOM_CFG *getCustomConfig(IMGSENSOR_SENSOR_IDX const sensorIdx);

#endif  //  _CAMERA_CUSTOM_IMGSENSOR_CFG_

