/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Iris-SC Yang <iris-sc.yang@mediatek.com>
 */

#ifndef __MTK_MML_V4L2_H__
#define __MTK_MML_V4L2_H__

#include <linux/types.h>
#include <linux/v4l2-controls.h>

#include "mtk-mml.h"

#define MML_M2M_MODULE_NAME	"mtk-mml-m2m"
#define MML_M2M_DEVICE_NAME	"MediaTek mml-m2m"
#define V4L2_CID_MTK_MML_BASE	(V4L2_CTRL_CLASS_USER | 0x2000)

enum {
	MML_M2M_CID_PQPARAM = V4L2_CID_MTK_MML_BASE,
	MML_M2M_CID_SECURE,
};

struct v4l2_pq_submit {
	uint32_t id;
	struct mml_pq_config pq_config;
	struct mml_pq_param pq_param;
};

#endif	/* __MTK_MML_V4L2_H__ */
