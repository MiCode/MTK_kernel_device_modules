/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __SENINF_SEC_DRV_H__
#define __SENINF_SEC_DRV_H__

#include "seninf_ta.h"

typedef enum {
	SENINF_DRV_RETURN_SUCCESS,
	SENINF_DRV_RETURN_ERROR,
	SENINF_DRV_RETURN_MAP_USER_ERROR,
	SENINF_DRV_RETURN_MAP_REGION_ERROR
} SENINF_DRV_RETURN;

SENINF_RETURN seninf_drv_sync_to_pa(void *args);
SENINF_RETURN seninf_ta_drv_sync_to_va(void *args);
SENINF_RETURN seninf_ta_drv_free(void *args);
SENINF_RETURN seninf_ta_drv_checkpipe(SENINF_RETURN auth_reuslt);

#endif
