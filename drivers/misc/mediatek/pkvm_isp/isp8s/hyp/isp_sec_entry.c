// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */
#include <asm/kvm_pkvm_module.h>
#include <linux/arm-smccc.h>
#include "pkvm_isp_hyp.h"
#include "isp_sec_api.h"
#include "isp_sec_entry.h"
#include "isp_sec_platform.h"

ISP_RETURN isp_config_sethsfcam(struct user_pt_regs *regs)
{
	ISP_RETURN ret = ISP_RETURN_ERROR;
	SecMgr_CamInfo *cam_info = NULL;
	uint64_t pa, enable_raw, En;
	void *pfixmap = NULL;

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "++");

	pa = (regs->regs[2] << 32) | regs->regs[1];

	CALL_FROM_OPS(puts, "pa");
	CALL_FROM_OPS(putx64, pa);

	enable_raw = regs->regs[3];
	En = regs->regs[4];

	pfixmap = CALL_FROM_OPS(fixmap_map, pa);
	CALL_FROM_OPS(flush_dcache_to_poc, pfixmap, sizeof(SecMgr_CamInfo));

	cam_info = (SecMgr_CamInfo *)pfixmap;
	CALL_FROM_OPS(puts, "P1 secure status/tg value:");

	CALL_FROM_OPS(putx64, cam_info->Sec_status);
	CALL_FROM_OPS(putx64, cam_info->SecTG);

	if (cam_info->Sec_status == 0x0) {
		ApiISPSetSecureState(cam_info, camsys_check_value);
		CALL_FROM_OPS(puts, "camsys return value:");
		CALL_FROM_OPS(putx64, cam_info->Sec_status);
	}

	CALL_FROM_OPS(flush_dcache_to_poc, pfixmap, sizeof(SecMgr_CamInfo));
	CALL_FROM_OPS(fixmap_unmap);

	isp_sec_configCam_platform(En, enable_raw);

	ret = ISP_RETURN_SUCCESS;
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "--");

	return ret;
}
ISP_RETURN isp_config_sethsfcamsv(struct user_pt_regs *regs)
{
	ISP_RETURN ret = ISP_RETURN_ERROR;
	uint64_t enable_raw, En;

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "++");

	enable_raw = regs->regs[2];
	En = regs->regs[3];

	CALL_FROM_OPS(puts, "enable_raw=");
	CALL_FROM_OPS(putx64, enable_raw);

	CALL_FROM_OPS(puts, "En =");
	CALL_FROM_OPS(putx64, En);

	isp_sec_configCamsv_platform(En, enable_raw);

	ret = ISP_RETURN_SUCCESS;
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "--");

	return ret;
}
ISP_RETURN isp_stream_ctrl(struct user_pt_regs *regs)
{
	ISP_RETURN ret = ISP_RETURN_ERROR;
	SecMgr_CamInfo *cam_info = NULL;
	uint64_t pa, enable_raw, without_tg;
	void *pfixmap = NULL;

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "++");

	pa = (regs->regs[2] << 32) | regs->regs[1];
	enable_raw = regs->regs[3];
	without_tg = regs->regs[4];

	CALL_FROM_OPS(puts, "pa");
	CALL_FROM_OPS(putx64, pa);

	pfixmap = CALL_FROM_OPS(fixmap_map, pa);
	CALL_FROM_OPS(flush_dcache_to_poc, pfixmap, sizeof(SecMgr_CamInfo));
	cam_info = (SecMgr_CamInfo *)pfixmap;

	CALL_FROM_OPS(puts, PFX "P1 secure status/tg value:");
	CALL_FROM_OPS(putx64, cam_info->Sec_status);
	CALL_FROM_OPS(putx64, cam_info->SecTG);

	if (cam_info->Sec_status == sensor_check_value) {
		ApiISPSetSecureState(cam_info, sensor_check_value);
		isp_sec_streamOn_platform(without_tg, enable_raw);
	}

	CALL_FROM_OPS(flush_dcache_to_poc, pfixmap, sizeof(SecMgr_CamInfo));
	CALL_FROM_OPS(fixmap_unmap);

	ret = ISP_RETURN_SUCCESS;
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "--");

	return ret;
}
