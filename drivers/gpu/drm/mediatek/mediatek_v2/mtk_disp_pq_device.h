/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __MTK_DISP_PQ_DEVICE_H__
#define __MTK_DISP_PQ_DEVICE_H__

#include <linux/uaccess.h>
#include <uapi/drm/mediatek_drm.h>

// the two include files must to be same!
//     (kernel: mtk_disp_pq_device.h / hal: pq_device.h)
//Todo: should replace include the mediatek_drm.h

struct TDSHP_CLARITY_REG {
	uint32_t dirty_flg;
	struct DISP_TDSHP_REG disp_tdshp_regs;
	struct DISP_TDSHP_CLARITY_REG disp_clarity_regs;
};

enum TDSHP_CLARITY_DIRTY {
	DIRTY_NONE = 0x0,
	TDSHP_REG_DIRTY = 0x1,
	CLARITY_REG_DIRTY = 0x2,
};

#endif
