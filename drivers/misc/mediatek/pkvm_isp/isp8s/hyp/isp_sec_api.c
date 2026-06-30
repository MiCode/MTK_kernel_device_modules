// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */
#include <linux/types.h>
#include "pkvm_isp_hyp.h"
#include "isp_sec_function.h"
#include <pkvm_sys.h>

int ApiISPSetSecureState(SecMgr_CamInfo *cam_info, int val)
{
	int ret = 0;

	CALL_FROM_OPS(puts, "+ SecTG:");
	CALL_FROM_OPS(putx64, cam_info->SecTG);

	CALL_FROM_OPS(puts, " Sec_status:");
	CALL_FROM_OPS(putx64, cam_info->Sec_status);

	CALL_FROM_OPS(puts, " val:");
	CALL_FROM_OPS(putx64, val);

	ret = SecDrvSetSecureState(cam_info, val);

	if (ret < 0) {
		CALL_FROM_OPS(puts, "ret:");
		CALL_FROM_OPS(putx64, ret);
	}

	return ret;
}
