// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <pkvm_mgmt/pkvm_mgmt.h>
#include <pkvm_sys.h>
#include <pkvm_trustzone.h>

// tf-a/plat/mediatek/drivers/devapc/devapc_hal.h
enum devapc_sip_cmd {
	SIP_APC_MODULE_SET = 1,
	SIP_APC_MASTER_SET,
};

enum devapc_hyp_module_req_type {
	DEVAPC_HYP_MODULE_REQ_APU,
	DEVAPC_HYP_MODULE_REQ_IMGSENSOR,
	DEVAPC_HYP_MODULE_REQ_CAMERA_ISP,
	DEVAPC_HYP_MODULE_REQ_NUM,
};

// tf-a/plat/mediatek/include/drivers/devapc_public.h
enum devapc_protect_on_off {
	DEVAPC_PROTECT_DISABLE = 0,
	DEVAPC_PROTECT_ENABLE,
};

static inline TZ_RESULT devapc_request(enum devapc_sip_cmd cmd, enum devapc_hyp_module_req_type module,
	enum devapc_protect_on_off onoff, uint32_t param)
{
	struct arm_smccc_res res;

	arm_smccc_1_1_smc(MTK_SIP_HYP_DEVAPC_CTRL,
		cmd, module, onoff, param, 0, 0, 0, &res);

	return (TZ_RESULT)res.a0;
}

TZ_RESULT APC_ImgsensorProtEnable(void)
{
	return devapc_request(
		SIP_APC_MODULE_SET,
		DEVAPC_HYP_MODULE_REQ_IMGSENSOR,
		DEVAPC_PROTECT_ENABLE,
		0);
}

TZ_RESULT APC_ImgsensorProtDisable(void)
{
	return devapc_request(
		SIP_APC_MODULE_SET,
		DEVAPC_HYP_MODULE_REQ_IMGSENSOR,
		DEVAPC_PROTECT_DISABLE,
		0);
}

TZ_RESULT APC_CamIspProtEnable(uint32_t param)
{
	return devapc_request(
		SIP_APC_MODULE_SET,
		DEVAPC_HYP_MODULE_REQ_CAMERA_ISP,
		DEVAPC_PROTECT_ENABLE,
		param);
}

TZ_RESULT APC_CamIspProtDisable(uint32_t param)
{
	return devapc_request(
		SIP_APC_MODULE_SET,
		DEVAPC_HYP_MODULE_REQ_CAMERA_ISP,
		DEVAPC_PROTECT_DISABLE,
		param);
}
