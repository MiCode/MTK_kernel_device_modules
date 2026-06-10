// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <mod_debug.h>
#include <pkvm_trustzone.h>

#include "MtkISPDrvTLApi.h"
#include "pkvm_seninf_hyp.h"
#include "seninf_reg.h"
#include "seninf_ta.h"
#include "sensor_cfg_sec.h"
#include "seninf_auth.h"
#include "seninf_dapc.h"

#define SENINF_AUTH_SENINF_MUX_MAX_NUM SENINF_MUX_NUM
#define SENINF_AUTH_SENINF_MAX_NUM	 SENINF_NUM

#define SENINF_AUTH_CAM_NUM			SENINF_CAM_MUX2
// #define SENINF_UT

#ifdef CMD_GETSECCAM
#define ISP_GET_SECURE_STATE CMD_GETSECCAM
#else
#define ISP_GET_SECURE_STATE 0XFF
#endif

#ifdef CMD_FILLPROTSTATUS
#define ISP_FILL_PROT_STATUS CMD_FILLPROTSTATUS
#else
#define ISP_FILL_PROT_STATUS 0xFF
#endif

int ApiISPGetSecureState(struct Sec_CamState *cam_state)
{
#ifndef SENINF_UT
	TZ_RESULT ret;

	MOD_PUTS(__func__);
	MOD_PUTS(PFX "start ipc to isp");
	// params[1].mem.buffer = (uint32_t*)cam_state;
	// params[1].mem.size = sizeof(struct Sec_CamState);
	// ret = UTEE_TeeServiceCall(handle, ISP_GET_SECURE_STATE, paramTypes, params);
	ret = 0;
	MOD_PUTS(__func__);
	MOD_PUTS1(PFX "end ipc to isp. {ret} =", ret);
	return ret;
#else
	memset(cam_state, 0, sizeof(struct Sec_CamState));
	return 0;
#endif
}

int ApiISPFillProtStatus(void)
{
#ifndef SENINF_UT

	MOD_PUTS(__func__);
	MOD_PUTS(PFX "start ipc to isp");

	TZ_RESULT ret;
	// ret = UTEE_TeeServiceCall(handle, ISP_FILL_PROT_STATUS, paramTypes, params);
	ret = 0;
	MOD_PUTS(__func__);
	MOD_PUTS1(PFX "end ipc to isp. {ret} =", ret);
	return ret;
#else
	return 0;
#endif
}


SENINF_MUX_ENUM seninf_auth_get_top_cam_mux(SENINF_TEE_REG *preg, SENINF_CAM_MUX_ENUM cam)
{
	uint8_t *pcam_mux = (uint8_t *)&preg->SENINF_CAM_MUX_CTRL_0;

	if (cam >= SENINF_CAM_MUX_NUM || cam < SENINF_CAM_MUX0)
		return SENINF_MUX_ERROR;

	pcam_mux += cam;

#ifdef SENINF_UT
	MOD_PUTS(__func__);
	MOD_PUTS2(PFX "{mux, cam} =", *pcam_mux, cam);
#endif
	return (*pcam_mux < SENINF_AUTH_SENINF_MUX_MAX_NUM) ? (SENINF_MUX_ENUM)*pcam_mux : SENINF_MUX_ERROR;
}

SENINF_ENUM seninf_auth_get_top_mux(SENINF_TEE_REG *preg, SENINF_MUX_ENUM mux)
{
	uint8_t *ptop_mux = (uint8_t *)&preg->SENINF_TOP_MUX_CTRL_0;

	if (mux >= SENINF_MUX_NUM || mux < SENINF_MUX1)
		return SENINF_ENUM_ERROR;

	ptop_mux += mux;
#ifdef SENINF_UT
	MOD_PUTS(__func__);
	MOD_PUTS2(PFX "{mux, seninf} =", mux, *ptop_mux);
#endif
	return (*ptop_mux < SENINF_AUTH_SENINF_MAX_NUM) ? (SENINF_ENUM)*ptop_mux : SENINF_ENUM_ERROR;
}

int seninf_auth_is_mux_used(SENINF_TEE_REG *preg, SENINF_MUX_ENUM mux)
{
	SENINF_TEE_REG_MUX *preg_mux = preg->seninf_mux;

	if (mux >= SENINF_MUX_NUM || mux < SENINF_MUX1)
		return 0;

	preg_mux += mux;
#ifdef SENINF_UT
	MOD_PUTS(__func__);
	MOD_PUTS2(PFX "{mux, enabled} =", mux, SENINF_BITS(preg_mux, SENINF_MUX_CTRL_0, seninf_mux_en));
#endif
	return SENINF_BITS(preg_mux, SENINF_MUX_CTRL_0, seninf_mux_en);
}

int seninf_auth_is_cam_mux_en(SENINF_TEE_REG *preg, SENINF_CAM_MUX_ENUM cam)
{
	return preg->SENINF_CAM_MUX_EN.Raw & (1<<cam);
}

SENINF_RETURN seninf_auth(SENINF_TEE_REG *preg)
{
	SENINF_RETURN  ret = SENINF_RETURN_SUCCESS;
	bool has_secured_path = false;
	struct Sec_CamState cam_state;

	memset(&cam_state, TG_UNKNOWN, sizeof(struct Sec_CamState));
	MOD_PUTS(__func__);
	MOD_PUTS(PFX "enter");

	ret = (SENINF_RETURN)seninf_dapc_lock();

	if (ret != SENINF_RETURN_SUCCESS) {
		MOD_PUTS(__func__);
		MOD_PUTS1(PFX "seninf dapc lock error! {ret} =", ret);
		return SENINF_RETURN_ERROR;
	}
	ApiISPGetSecureState(&cam_state);

#ifdef SENINF_UT //def SENINF_UT
	// uint32_t *pregister = (uint32_t *)preg;

	// for (int i = 0; i < sizeof(SENINF_TEE_REG) / sizeof(uint32_t); i++)
	//	MOD_PUTS2(PFX "{i, reg} =", i, pregister[i]);

	for (int i = 0; i < SENINF_AUTH_CAM_NUM; i++)
		MOD_PUTS2(PFX "{i, SecTG} =", i, cam_state.cam_info[i].SecTG);
#endif

	for (int i = 0; i < SENINF_CAM_MUX_NUM && ret == SENINF_RETURN_SUCCESS; i++) {
		if (!seninf_auth_is_cam_mux_en(preg, (SENINF_CAM_MUX_ENUM)i))
			continue;

		SENINF_MUX_ENUM seninf_mux = seninf_auth_get_top_cam_mux(preg, (SENINF_CAM_MUX_ENUM)i);

		if (seninf_mux == SENINF_MUX_ERROR || !seninf_auth_is_mux_used(preg, (SENINF_MUX_ENUM)seninf_mux))
			continue;

		CUSTOM_CFG_SECURE secure = sensor_cfg_sec_from_seninf(seninf_auth_get_top_mux(preg, seninf_mux));

		if (secure == CUSTOM_CFG_SECURE_NONE)
			continue;

		if (i < SENINF_AUTH_CAM_NUM) {
			if (cam_state.cam_info[i].SecTG == TG_UNKNOWN) {
				MOD_PUTS3(PFX "Authentication fail. {mux, secure, SecTG} =",
					seninf_mux, secure, cam_state.cam_info[i].SecTG);
				ret = SENINF_RETURN_ERROR;
			} else
				has_secured_path= true;
		} else {
			/* CamSV path */
			MOD_PUTS3(PFX "{i, secure, seninf_mux} =", i, secure, seninf_mux);
			ret = SENINF_RETURN_ERROR;
		}
	}

	ret = (ret == SENINF_RETURN_SUCCESS && has_secured_path && !ApiISPFillProtStatus()) ? ret : SENINF_RETURN_ERROR;
	MOD_PUTS(__func__);
	MOD_PUTS2(PFX "exit. {has_secured_path, ret} =", has_secured_path, ret);

	return ret;
}
