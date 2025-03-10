/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __SENINF_AUTH_H__
#define __SENINF_AUTH_H__

#include <linux/types.h>

#include "seninf_drv_csi_info.h"
#include "seninf_ta.h"
#include "seninf_tee_reg.h"

SENINF_RETURN seninf_auth(SENINF_TEE_REG *preg, uint64_t pa);

int seninf_lock_exclude_enable(SENINF_TEE_REG *preg,
	SENINF_ASYNC_ENUM seninf, SENINF_OUTMUX_ENUM outmux);

int seninf_lock_exclude_disable(void);

typedef struct {
	uint32_t SecTG;
	uint32_t Sec_status;
} SecMgr_CamInfo;

#endif
