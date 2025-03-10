// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/types.h>

#include "pkvm_isp_hyp.h"
#include "seninf_auth.h"
#include "seninf_drv_csi_info.h"
#include "seninf_ta.h"
#include "seninf_tee_reg.h"
#include "seninf_util.h"
#include "sensor_cfg_sec.h"
#include "sys.h"

#define camsys_check_value 0x43
#define sensor_check_value 0x53

#if SENINF_CID_EN
#define CID_VALUE 14
// #define CID_VALUE 0
#define CID_VALUE_INIT 0
#define RDY_MASK_GROUP_ID 9
// #define RDY_MASK_GROUP_ID 0
#define RDY_MASK_GROUP_ID_INIT 0
#endif

// #define SENINF_UT
#ifdef SENINF_UT
#define SENINF_AUTH_CAMSV_MIN          SENINF_OUTMUX0
#define SENINF_AUTH_CAMSV_MAX          SENINF_OUTMUX_NUM
#define SENINF_AUTH_CAM_MIN            SENINF_OUTMUX0
#define SENINF_AUTH_CAM_MAX            SENINF_OUTMUX_NUM
#else
#define SENINF_AUTH_CAMSV_MIN          SENINF_OUTMUX0
#define SENINF_AUTH_CAMSV_MAX          SENINF_OUTMUX5
#define SENINF_AUTH_CAM_MIN            SENINF_OUTMUX6
#define SENINF_AUTH_CAM_MAX            SENINF_OUTMUX8
#endif

#define LOG_MORE 1
#define DUMP_MORE 0

#define SENINF_MUX_VR_FIRST 0
#define SENINF_MUX_VR_LAST 54
#define SENINF_VC_SPLIT_NONE_FOR_PDP_NUM 4
#define PDP_MUX_LAST SENINF_MUX14
#define PDP_CAM_MUX_LAST SENINF_CAM_MUX43
#define SENINF_VC_SPLIT_NONE_FOR_UISP_NUM 1
#define UISP_MUX_START SENINF_MUX15
#define UISP_MUX_LAST SENINF_MUX15
#define UISP_CAM_MUX_FIRST SENINF_CAM_MUX44
#define UISP_CAM_MUX_LAST SENINF_CAM_MUX44

#define SAT_MUX_FACTOR 8
#define SV_NORMAL_MUX_FACTOR 1
#define RAW_MUX_FACTOR 4
#define PDP_MUX_FACTOR 1

#define SAT_MUX_VR_FIRST SENINF_CAM_MUX0
#define SAT_MUX_VR_LAST SENINF_CAM_MUX31
#define SAT_MUX_FIRST SENINF_MUX1
#define SAT_MUX_LAST SENINF_MUX4
#define SAT_CAM_MUX_FIRST SENINF_CAM_MUX0
#define SAT_CAM_MUX_LAST SENINF_CAM_MUX31

#define SV_NORMAL_MUX_VR_FIRST SENINF_CAM_MUX32
#define SV_NORMAL_MUX_VR_LAST SENINF_CAM_MUX33
#define SV_NORMAL_MUX_FIRST SENINF_MUX5
#define SV_NORMAL_MUX_LAST SENINF_MUX6
#define SV_NORMAL_CAM_MUX_FIRST SENINF_CAM_MUX32
#define SV_NORMAL_CAM_MUX_LAST SENINF_CAM_MUX33

#define RAW_MUX_VR_FIRST SENINF_CAM_MUX34
#define RAW_MUX_FIRST SENINF_MUX7
#define RAW_MUX_LAST SENINF_MUX10
#define RAW_CAM_MUX_FIRST SENINF_CAM_MUX34
#define RAW_CAM_MUX_LAST SENINF_CAM_MUX39

#define PDP_MUX_FIRST SENINF_MUX11
#define PDP_CAM_MUX_FIRST SENINF_CAM_MUX40

SENINF_ASYNC_ENUM g_seninf;
SENINF_OUTMUX_ENUM g_seninf_outmux[SENINF_OUTMUX_NUM];
int g_seninf_lock_flag;
int g_multi_sensor_individual_flag;
int g_multi_sensor_current_port = -1;
uint32_t g_count;

#undef DAPC_EN
#ifndef DAPC_EN
SENINF_TEE_REG *g_preg_va;
#endif

#define P1_CCU_READY 1
#define SMPU_NO_VIOLATION 1

int ApiISPGetSecureState(uint64_t pa, SecMgr_CamInfo *sensor_check, int ret)
{
#if SMPU_NO_VIOLATION
	void *pfixmap = NULL;

	pfixmap = CALL_FROM_OPS(fixmap_map, pa);
	CALL_FROM_OPS(flush_dcache_to_poc, pfixmap, sizeof(SecMgr_CamInfo));

	sensor_check = (SecMgr_CamInfo *)pfixmap;
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "P1 secure status/tg value:");
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "Sec_status:");
	CALL_FROM_OPS(putx64, sensor_check->Sec_status);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "SecTG:");
	CALL_FROM_OPS(putx64, sensor_check->SecTG);
#if P1_CCU_READY
	if (sensor_check->Sec_status == camsys_check_value && ret == SENINF_RETURN_SUCCESS) {
		sensor_check->Sec_status = sensor_check_value;
		g_seninf_lock_flag = 1;
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "sensor return value");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "Sec_status:");
		CALL_FROM_OPS(putx64, sensor_check->Sec_status);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "lock_flag:");
		CALL_FROM_OPS(putx64, g_seninf_lock_flag);
	} else {
		sensor_check->Sec_status = -1;
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "secure status have error");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "lock_flag:");
		CALL_FROM_OPS(putx64, g_seninf_lock_flag);
	}
#else
	sensor_check->Sec_status = sensor_check_value;
	g_seninf_lock_flag = 1;
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "sensor return value");
	CALL_FROM_OPS(putx64, sensor_check->Sec_status);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "lock_flag:");
	CALL_FROM_OPS(putx64, g_seninf_lock_flag);
#endif
	CALL_FROM_OPS(flush_dcache_to_poc, pfixmap, sizeof(SecMgr_CamInfo));
	CALL_FROM_OPS(fixmap_unmap);
#else
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "bypass smpu violation");
	if (ret == SENINF_RETURN_SUCCESS) {
		g_seninf_lock_flag = 1;
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "sensor return value bypass");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "lock_flag:");
		CALL_FROM_OPS(putx64, g_seninf_lock_flag);
	} else {
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "secure status have error");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "ret:");
		CALL_FROM_OPS(putx64, ret);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "lock_flag:");
		CALL_FROM_OPS(putx64, g_seninf_lock_flag);
	}
#endif
	return 0;
}

int seninf_auth_is_outmux_en(SENINF_TEE_REG *preg, SENINF_OUTMUX_ENUM outmux)
{
	int ret = 0;
	uint32_t seninf_outmux_tag_en = 0;
	uint32_t seninf_outmux_tag_vcdt_read = 0;

	if (outmux >= SENINF_OUTMUX_NUM || outmux < SENINF_OUTMUX0)
		return SENINF_OUTMUX_ERROR;

	switch (outmux) {
	case SENINF_OUTMUX0:
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_0_TAG_VCDT_FILT_0],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_0_TAG_VCDT_FILT_0],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_0_TAG_VCDT_FILT_1],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_0_TAG_VCDT_FILT_1],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_0_TAG_VCDT_FILT_2],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_0_TAG_VCDT_FILT_2],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_0_TAG_VCDT_FILT_3],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_0_TAG_VCDT_FILT_3],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_0_TAG_VCDT_FILT_4],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_0_TAG_VCDT_FILT_4],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_0_TAG_VCDT_FILT_5],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_0_TAG_VCDT_FILT_5],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_0_TAG_VCDT_FILT_6],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_0_TAG_VCDT_FILT_6],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_0_TAG_VCDT_FILT_7],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_0_TAG_VCDT_FILT_7],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		break;
	case SENINF_OUTMUX1:
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_1_TAG_VCDT_FILT_0],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_1_TAG_VCDT_FILT_0],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_1_TAG_VCDT_FILT_1],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_1_TAG_VCDT_FILT_1],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_1_TAG_VCDT_FILT_2],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_1_TAG_VCDT_FILT_2],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_1_TAG_VCDT_FILT_3],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_1_TAG_VCDT_FILT_3],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_1_TAG_VCDT_FILT_4],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_1_TAG_VCDT_FILT_4],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_1_TAG_VCDT_FILT_5],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_1_TAG_VCDT_FILT_5],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_1_TAG_VCDT_FILT_6],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_1_TAG_VCDT_FILT_6],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_1_TAG_VCDT_FILT_7],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_1_TAG_VCDT_FILT_7],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		break;
	case SENINF_OUTMUX2:
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_2_TAG_VCDT_FILT_0],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_2_TAG_VCDT_FILT_0],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_2_TAG_VCDT_FILT_1],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_2_TAG_VCDT_FILT_1],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_2_TAG_VCDT_FILT_2],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_2_TAG_VCDT_FILT_2],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_2_TAG_VCDT_FILT_3],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_2_TAG_VCDT_FILT_3],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_2_TAG_VCDT_FILT_4],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_2_TAG_VCDT_FILT_4],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_2_TAG_VCDT_FILT_5],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_2_TAG_VCDT_FILT_5],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_2_TAG_VCDT_FILT_6],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_2_TAG_VCDT_FILT_6],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_2_TAG_VCDT_FILT_7],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_2_TAG_VCDT_FILT_7],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		break;
	case SENINF_OUTMUX3:
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_3_TAG_VCDT_FILT_0],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_3_TAG_VCDT_FILT_0],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_3_TAG_VCDT_FILT_1],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_3_TAG_VCDT_FILT_1],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_3_TAG_VCDT_FILT_2],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_3_TAG_VCDT_FILT_2],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_3_TAG_VCDT_FILT_3],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_3_TAG_VCDT_FILT_3],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_3_TAG_VCDT_FILT_4],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_3_TAG_VCDT_FILT_4],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_3_TAG_VCDT_FILT_5],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_3_TAG_VCDT_FILT_5],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_3_TAG_VCDT_FILT_6],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_3_TAG_VCDT_FILT_6],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_3_TAG_VCDT_FILT_7],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_3_TAG_VCDT_FILT_7],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		break;
	case SENINF_OUTMUX4:
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_4_TAG_VCDT_FILT_0],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_4_TAG_VCDT_FILT_0],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_4_TAG_VCDT_FILT_1],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_4_TAG_VCDT_FILT_1],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_4_TAG_VCDT_FILT_2],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_4_TAG_VCDT_FILT_2],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_4_TAG_VCDT_FILT_3],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_4_TAG_VCDT_FILT_3],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_4_TAG_VCDT_FILT_4],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_4_TAG_VCDT_FILT_4],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_4_TAG_VCDT_FILT_5],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_4_TAG_VCDT_FILT_5],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_4_TAG_VCDT_FILT_6],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_4_TAG_VCDT_FILT_6],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_4_TAG_VCDT_FILT_7],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_4_TAG_VCDT_FILT_7],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		break;
	case SENINF_OUTMUX5:
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_5_TAG_VCDT_FILT_0],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_5_TAG_VCDT_FILT_0],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_5_TAG_VCDT_FILT_1],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_5_TAG_VCDT_FILT_1],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_5_TAG_VCDT_FILT_2],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_5_TAG_VCDT_FILT_2],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_5_TAG_VCDT_FILT_3],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_5_TAG_VCDT_FILT_3],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_5_TAG_VCDT_FILT_4],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_5_TAG_VCDT_FILT_4],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_5_TAG_VCDT_FILT_5],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_5_TAG_VCDT_FILT_5],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_5_TAG_VCDT_FILT_6],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_5_TAG_VCDT_FILT_6],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_5_TAG_VCDT_FILT_7],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_5_TAG_VCDT_FILT_7],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		break;
	case SENINF_OUTMUX6:
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_6_TAG_VCDT_FILT_0],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_6_TAG_VCDT_FILT_0],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_6_TAG_VCDT_FILT_1],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_6_TAG_VCDT_FILT_1],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_6_TAG_VCDT_FILT_2],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_6_TAG_VCDT_FILT_2],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		break;
	case SENINF_OUTMUX7:
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_7_TAG_VCDT_FILT_0],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_7_TAG_VCDT_FILT_0],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_7_TAG_VCDT_FILT_1],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_7_TAG_VCDT_FILT_1],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_7_TAG_VCDT_FILT_2],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_7_TAG_VCDT_FILT_2],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		break;
	case SENINF_OUTMUX8:
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_8_TAG_VCDT_FILT_0],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_8_TAG_VCDT_FILT_0],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_8_TAG_VCDT_FILT_1],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_8_TAG_VCDT_FILT_1],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_8_TAG_VCDT_FILT_2],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_8_TAG_VCDT_FILT_2],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		break;
	case SENINF_OUTMUX9:
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_9_TAG_VCDT_FILT_0],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_9_TAG_VCDT_FILT_0],
			&seninf_outmux_tag_vcdt_read);
		seninf_outmux_tag_en |= seninf_outmux_tag_vcdt_read & 0x1;
		break;
	default:
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "invalid");
		break;
	}

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "SENINF_OUTMUX:");
	CALL_FROM_OPS(putx64, outmux);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "TAG_EN:");
	CALL_FROM_OPS(putx64, seninf_outmux_tag_en);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "ret:");
	CALL_FROM_OPS(putx64, ret);

	return seninf_outmux_tag_en;
}

SENINF_ASYNC_ENUM seninf_auth_get_async(SENINF_TEE_REG *preg, SENINF_OUTMUX_ENUM outmux)
{
	SENINF_TEE_REG_OUTMUX *preg_outmux = preg->seninf_outmux;

	if (outmux >= SENINF_OUTMUX_NUM || outmux < SENINF_OUTMUX0)
		return SENINF_ASYNC_ENUM_ERROR;

	preg_outmux += outmux;

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "outmux");
	CALL_FROM_OPS(putx64, outmux);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "outmux_src");
	CALL_FROM_OPS(putx64, ((preg_outmux->SENINF_TEE_OUTMUX_SRC_SEL) >> 4) & 0x7);

	return (SENINF_ASYNC_ENUM)(((preg_outmux->SENINF_TEE_OUTMUX_SRC_SEL) >> 4) & 0x7);
}

#if SENINF_CID_EN
int seninf_CID_handler(SENINF_ASYNC_ENUM seninf, SENINF_OUTMUX_ENUM outmux, int enable)
{
	int ret = SENINF_RETURN_SUCCESS;
	uint32_t cid_value = CID_VALUE;

	uint32_t seninf_outmux_csr_cfg_ctrl_value = 0;
	uint32_t seninf_outmux_csr_cfg_ctrl_value_read = 0;
	uint32_t root_sensor_cid_value = CID_VALUE_INIT;
	uint32_t root_sensor_cid_value_read = CID_VALUE_INIT;
	uint32_t root_tsrec_cid_value = CID_VALUE_INIT;
	uint32_t root_tsrec_cid_value_read = CID_VALUE_INIT;
	uint32_t root_outmux_cid_value = CID_VALUE_INIT;
	uint32_t root_outmux_cid_value_read = CID_VALUE_INIT;

	uint32_t root_rdy_mask_group_id = RDY_MASK_GROUP_ID;
	uint32_t root_rdy_mask_group_id_value = RDY_MASK_GROUP_ID_INIT;
	uint32_t root_rdy_mask_group_id_value_read = RDY_MASK_GROUP_ID_INIT;
	uint32_t root_rdy_mask_cid_value = CID_VALUE_INIT;
	uint32_t root_rdy_mask_cid_value_read = CID_VALUE_INIT;

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "enter");
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "enable:");
	CALL_FROM_OPS(putx64, enable);

	if (enable < 0) {
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "invalid parameter");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "enable:");
		CALL_FROM_OPS(putx64, enable);
		ret = SENINF_RETURN_ERROR;
		return ret;
	}

	if (seninf >= SENINF_ASYNC_NUM || seninf < SENINF_ASYNC_0)
		return SENINF_RETURN_ERROR;

	if (outmux >= SENINF_OUTMUX_NUM || outmux < SENINF_OUTMUX0)
		return SENINF_RETURN_ERROR;

	if (enable) {
		switch (outmux) {
		case SENINF_OUTMUX0:
			ret |= SECIO_READ(
				gseninf_drv_reg_base[SENINF_OUTMUX0_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX0_CSR_CFG_CTRL],
				&seninf_outmux_csr_cfg_ctrl_value_read);

			seninf_outmux_csr_cfg_ctrl_value = seninf_outmux_csr_cfg_ctrl_value_read & ~(0x1 << 12);

			ret |= SECIO_WRITE(
				gseninf_drv_reg_base[SENINF_OUTMUX0_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX0_CSR_CFG_CTRL],
				seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "csr_cfg_ctrl_value_read:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value_read);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "seninf_outmux_csr_cfg_ctrl_value:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret:");
			CALL_FROM_OPS(putx64, ret);
			break;
		case SENINF_OUTMUX1:
			ret |= SECIO_READ(
				gseninf_drv_reg_base[SENINF_OUTMUX1_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX1_CSR_CFG_CTRL],
				&seninf_outmux_csr_cfg_ctrl_value_read);

			seninf_outmux_csr_cfg_ctrl_value = seninf_outmux_csr_cfg_ctrl_value_read & ~(0x1 << 12);

			ret |= SECIO_WRITE(
				gseninf_drv_reg_base[SENINF_OUTMUX1_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX1_CSR_CFG_CTRL],
				seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "csr_cfg_ctrl_value_read:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value_read);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "seninf_outmux_csr_cfg_ctrl_value:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret:");
			CALL_FROM_OPS(putx64, ret);
			break;
		case SENINF_OUTMUX2:
			ret |= SECIO_READ(
				gseninf_drv_reg_base[SENINF_OUTMUX2_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX2_CSR_CFG_CTRL],
				&seninf_outmux_csr_cfg_ctrl_value_read);

			seninf_outmux_csr_cfg_ctrl_value = seninf_outmux_csr_cfg_ctrl_value_read & ~(0x1 << 12);

			ret |= SECIO_WRITE(
				gseninf_drv_reg_base[SENINF_OUTMUX2_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX2_CSR_CFG_CTRL],
				seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "csr_cfg_ctrl_value_read:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value_read);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "seninf_outmux_csr_cfg_ctrl_value:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret:");
			CALL_FROM_OPS(putx64, ret);
			break;
		case SENINF_OUTMUX3:
			ret |= SECIO_READ(
				gseninf_drv_reg_base[SENINF_OUTMUX3_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX3_CSR_CFG_CTRL],
				&seninf_outmux_csr_cfg_ctrl_value_read);

			seninf_outmux_csr_cfg_ctrl_value = seninf_outmux_csr_cfg_ctrl_value_read & ~(0x1 << 12);

			ret |= SECIO_WRITE(
				gseninf_drv_reg_base[SENINF_OUTMUX3_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX3_CSR_CFG_CTRL],
				seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "csr_cfg_ctrl_value_read:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value_read);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "seninf_outmux_csr_cfg_ctrl_value:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret:");
			CALL_FROM_OPS(putx64, ret);
			break;
		case SENINF_OUTMUX4:
			ret |= SECIO_READ(
				gseninf_drv_reg_base[SENINF_OUTMUX4_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX4_CSR_CFG_CTRL],
				&seninf_outmux_csr_cfg_ctrl_value_read);

			seninf_outmux_csr_cfg_ctrl_value = seninf_outmux_csr_cfg_ctrl_value_read & ~(0x1 << 12);

			ret |= SECIO_WRITE(
				gseninf_drv_reg_base[SENINF_OUTMUX4_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX4_CSR_CFG_CTRL],
				seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "csr_cfg_ctrl_value_read:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value_read);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "seninf_outmux_csr_cfg_ctrl_value:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret:");
			CALL_FROM_OPS(putx64, ret);
			break;
		case SENINF_OUTMUX5:
			ret |= SECIO_READ(
				gseninf_drv_reg_base[SENINF_OUTMUX5_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX5_CSR_CFG_CTRL],
				&seninf_outmux_csr_cfg_ctrl_value_read);

			seninf_outmux_csr_cfg_ctrl_value = seninf_outmux_csr_cfg_ctrl_value_read & ~(0x1 << 12);

			ret |= SECIO_WRITE(
				gseninf_drv_reg_base[SENINF_OUTMUX5_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX5_CSR_CFG_CTRL],
				seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "csr_cfg_ctrl_value_read:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value_read);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "seninf_outmux_csr_cfg_ctrl_value:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret:");
			CALL_FROM_OPS(putx64, ret);
			break;
		case SENINF_OUTMUX6:
			ret |= SECIO_READ(
				gseninf_drv_reg_base[SENINF_OUTMUX6_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX6_CSR_CFG_CTRL],
				&seninf_outmux_csr_cfg_ctrl_value_read);

			seninf_outmux_csr_cfg_ctrl_value = seninf_outmux_csr_cfg_ctrl_value_read & ~(0x1 << 12);

			ret |= SECIO_WRITE(
				gseninf_drv_reg_base[SENINF_OUTMUX6_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX6_CSR_CFG_CTRL],
				seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "csr_cfg_ctrl_value_read:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value_read);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "seninf_outmux_csr_cfg_ctrl_value:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret:");
			CALL_FROM_OPS(putx64, ret);
			break;
		case SENINF_OUTMUX7:
			ret |= SECIO_READ(
				gseninf_drv_reg_base[SENINF_OUTMUX7_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX7_CSR_CFG_CTRL],
				&seninf_outmux_csr_cfg_ctrl_value_read);

			seninf_outmux_csr_cfg_ctrl_value = seninf_outmux_csr_cfg_ctrl_value_read & ~(0x1 << 12);

			ret |= SECIO_WRITE(
				gseninf_drv_reg_base[SENINF_OUTMUX7_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX7_CSR_CFG_CTRL],
				seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "csr_cfg_ctrl_value_read:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value_read);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "seninf_outmux_csr_cfg_ctrl_value:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret:");
			CALL_FROM_OPS(putx64, ret);
			break;
		case SENINF_OUTMUX8:
			ret |= SECIO_READ(
				gseninf_drv_reg_base[SENINF_OUTMUX8_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX8_CSR_CFG_CTRL],
				&seninf_outmux_csr_cfg_ctrl_value_read);

			seninf_outmux_csr_cfg_ctrl_value = seninf_outmux_csr_cfg_ctrl_value_read & ~(0x1 << 12);

			ret |= SECIO_WRITE(
				gseninf_drv_reg_base[SENINF_OUTMUX8_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX8_CSR_CFG_CTRL],
				seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "csr_cfg_ctrl_value_read:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value_read);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "seninf_outmux_csr_cfg_ctrl_value:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret:");
			CALL_FROM_OPS(putx64, ret);
			break;
		case SENINF_OUTMUX9:
			ret |= SECIO_READ(
				gseninf_drv_reg_base[SENINF_OUTMUX9_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX9_CSR_CFG_CTRL],
				&seninf_outmux_csr_cfg_ctrl_value_read);

			seninf_outmux_csr_cfg_ctrl_value = seninf_outmux_csr_cfg_ctrl_value_read & ~(0x1 << 12);

			ret |= SECIO_WRITE(
				gseninf_drv_reg_base[SENINF_OUTMUX9_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX9_CSR_CFG_CTRL],
				seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "csr_cfg_ctrl_value_read:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value_read);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "seninf_outmux_csr_cfg_ctrl_value:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret:");
			CALL_FROM_OPS(putx64, ret);
			break;
		default:
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "invalid");
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux:");
			CALL_FROM_OPS(putx64, outmux);
			break;
		}
	}

	root_sensor_cid_value = (cid_value << 8) | cid_value;
	root_tsrec_cid_value = (((cid_value << 8) | cid_value) << ((seninf%2)*16));
	switch (seninf) {
	case SENINF_ASYNC_0:
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SENSOR_CID_0],
			gseninf_drv_reg_addr[SENINF_ROOT_SENSOR_CID_0], &root_sensor_cid_value_read);
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_TSREC_CID_0],
			gseninf_drv_reg_addr[SENINF_ROOT_TSREC_CID_0], &root_tsrec_cid_value_read);
		if (enable) {
			root_sensor_cid_value |= root_sensor_cid_value_read;
			root_tsrec_cid_value |= root_tsrec_cid_value_read;
		} else {
			root_sensor_cid_value = root_sensor_cid_value_read & ~root_sensor_cid_value;
			root_tsrec_cid_value = root_tsrec_cid_value_read & ~root_tsrec_cid_value;
		}
		ret |= SECIO_WRITE(
			gseninf_drv_reg_base[SENINF_ROOT_SENSOR_CID_0],
			gseninf_drv_reg_addr[SENINF_ROOT_SENSOR_CID_0], root_sensor_cid_value);
		ret |= SECIO_WRITE(
			gseninf_drv_reg_base[SENINF_ROOT_TSREC_CID_0],
			gseninf_drv_reg_addr[SENINF_ROOT_TSREC_CID_0], root_tsrec_cid_value);
		break;
	case SENINF_ASYNC_1:
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SENSOR_CID_1],
			gseninf_drv_reg_addr[SENINF_ROOT_SENSOR_CID_1], &root_sensor_cid_value_read);
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_TSREC_CID_0],
			gseninf_drv_reg_addr[SENINF_ROOT_TSREC_CID_0], &root_tsrec_cid_value_read);
		if (enable) {
			root_sensor_cid_value |= root_sensor_cid_value_read;
			root_tsrec_cid_value |= root_tsrec_cid_value_read;
		} else {
			root_sensor_cid_value = root_sensor_cid_value_read & ~root_sensor_cid_value;
			root_tsrec_cid_value = root_tsrec_cid_value_read & ~root_tsrec_cid_value;
		}
		ret |= SECIO_WRITE(
			gseninf_drv_reg_base[SENINF_ROOT_SENSOR_CID_1],
			gseninf_drv_reg_addr[SENINF_ROOT_SENSOR_CID_1], root_sensor_cid_value);
		ret |= SECIO_WRITE(
			gseninf_drv_reg_base[SENINF_ROOT_TSREC_CID_0],
			gseninf_drv_reg_addr[SENINF_ROOT_TSREC_CID_0], root_tsrec_cid_value);
		break;
	case SENINF_ASYNC_2:
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SENSOR_CID_2],
			gseninf_drv_reg_addr[SENINF_ROOT_SENSOR_CID_2], &root_sensor_cid_value_read);
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_TSREC_CID_1],
			gseninf_drv_reg_addr[SENINF_ROOT_TSREC_CID_1], &root_tsrec_cid_value_read);
		if (enable) {
			root_sensor_cid_value |= root_sensor_cid_value_read;
			root_tsrec_cid_value |= root_tsrec_cid_value_read;
		} else {
			root_sensor_cid_value = root_sensor_cid_value_read & ~root_sensor_cid_value;
			root_tsrec_cid_value = root_tsrec_cid_value_read & ~root_tsrec_cid_value;
		}
		ret |= SECIO_WRITE(
			gseninf_drv_reg_base[SENINF_ROOT_SENSOR_CID_2],
			gseninf_drv_reg_addr[SENINF_ROOT_SENSOR_CID_2], root_sensor_cid_value);
		ret |= SECIO_WRITE(
			gseninf_drv_reg_base[SENINF_ROOT_TSREC_CID_1],
			gseninf_drv_reg_addr[SENINF_ROOT_TSREC_CID_1], root_tsrec_cid_value);
		break;
	case SENINF_ASYNC_3:
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SENSOR_CID_3],
			gseninf_drv_reg_addr[SENINF_ROOT_SENSOR_CID_3], &root_sensor_cid_value_read);
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_TSREC_CID_1],
			gseninf_drv_reg_addr[SENINF_ROOT_TSREC_CID_1], &root_tsrec_cid_value_read);
		if (enable) {
			root_sensor_cid_value |= root_sensor_cid_value_read;
			root_tsrec_cid_value |= root_tsrec_cid_value_read;
		} else {
			root_sensor_cid_value = root_sensor_cid_value_read & ~root_sensor_cid_value;
			root_tsrec_cid_value = root_tsrec_cid_value_read & ~root_tsrec_cid_value;
		}
		ret |= SECIO_WRITE(
			gseninf_drv_reg_base[SENINF_ROOT_SENSOR_CID_3],
			gseninf_drv_reg_addr[SENINF_ROOT_SENSOR_CID_3], root_sensor_cid_value);
		ret |= SECIO_WRITE(
			gseninf_drv_reg_base[SENINF_ROOT_TSREC_CID_1],
			gseninf_drv_reg_addr[SENINF_ROOT_TSREC_CID_1], root_tsrec_cid_value);
		break;
	case SENINF_ASYNC_4:
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SENSOR_CID_4],
			gseninf_drv_reg_addr[SENINF_ROOT_SENSOR_CID_4], &root_sensor_cid_value_read);
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_TSREC_CID_1],
			gseninf_drv_reg_addr[SENINF_ROOT_TSREC_CID_1], &root_tsrec_cid_value_read);
		if (enable) {
			root_sensor_cid_value |= root_sensor_cid_value_read;
			root_tsrec_cid_value |= root_tsrec_cid_value_read;
		} else {
			root_sensor_cid_value = root_sensor_cid_value_read & ~root_sensor_cid_value;
			root_tsrec_cid_value = root_tsrec_cid_value_read & ~root_tsrec_cid_value;
		}
		ret |= SECIO_WRITE(
			gseninf_drv_reg_base[SENINF_ROOT_SENSOR_CID_4],
			gseninf_drv_reg_addr[SENINF_ROOT_SENSOR_CID_4], root_sensor_cid_value);
		ret |= SECIO_WRITE(
			gseninf_drv_reg_base[SENINF_ROOT_TSREC_CID_2],
			gseninf_drv_reg_addr[SENINF_ROOT_TSREC_CID_2], root_tsrec_cid_value);
		break;
	case SENINF_ASYNC_5:
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SENSOR_CID_5],
			gseninf_drv_reg_addr[SENINF_ROOT_SENSOR_CID_5], &root_sensor_cid_value_read);
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_TSREC_CID_2],
			gseninf_drv_reg_addr[SENINF_ROOT_TSREC_CID_2], &root_tsrec_cid_value_read);
		if (enable) {
			root_sensor_cid_value |= root_sensor_cid_value_read;
			root_tsrec_cid_value |= root_tsrec_cid_value_read;
		} else {
			root_sensor_cid_value = root_sensor_cid_value_read & ~root_sensor_cid_value;
			root_tsrec_cid_value = root_tsrec_cid_value_read & ~root_tsrec_cid_value;
		}
		ret |= SECIO_WRITE(
			gseninf_drv_reg_base[SENINF_ROOT_SENSOR_CID_5],
			gseninf_drv_reg_addr[SENINF_ROOT_SENSOR_CID_5], root_sensor_cid_value);
		ret |= SECIO_WRITE(
			gseninf_drv_reg_base[SENINF_ROOT_TSREC_CID_2],
			gseninf_drv_reg_addr[SENINF_ROOT_TSREC_CID_2], root_tsrec_cid_value);
		break;
	default:
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "SENINF_CID invalid");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "seninf:");
		CALL_FROM_OPS(putx64, seninf);
		break;
	}

#if LOG_MORE
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "check");
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "ret:");
	CALL_FROM_OPS(putx64, ret);
#endif

	root_outmux_cid_value = cid_value << ((outmux % 4) * 8);
	switch (outmux) {
	case SENINF_OUTMUX0:
	case SENINF_OUTMUX1:
	case SENINF_OUTMUX2:
	case SENINF_OUTMUX3:
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_OUTMUX_CID_0],
			gseninf_drv_reg_addr[SENINF_ROOT_OUTMUX_CID_0], &root_outmux_cid_value_read);
		if (enable)
			root_outmux_cid_value |= root_outmux_cid_value_read;
		else
			root_outmux_cid_value = root_outmux_cid_value_read & ~root_outmux_cid_value;
		ret |= SECIO_WRITE(
			gseninf_drv_reg_base[SENINF_ROOT_OUTMUX_CID_0],
			gseninf_drv_reg_addr[SENINF_ROOT_OUTMUX_CID_0], root_outmux_cid_value);
		break;
	case SENINF_OUTMUX4:
	case SENINF_OUTMUX5:
	case SENINF_OUTMUX6:
	case SENINF_OUTMUX7:
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_OUTMUX_CID_1],
			gseninf_drv_reg_addr[SENINF_ROOT_OUTMUX_CID_1], &root_outmux_cid_value_read);
		if (enable)
			root_outmux_cid_value |= root_outmux_cid_value_read;
		else
			root_outmux_cid_value = root_outmux_cid_value_read & ~root_outmux_cid_value;
		ret |= SECIO_WRITE(
			gseninf_drv_reg_base[SENINF_ROOT_OUTMUX_CID_1],
			gseninf_drv_reg_addr[SENINF_ROOT_OUTMUX_CID_1], root_outmux_cid_value);
		break;
	case SENINF_OUTMUX8:
	case SENINF_OUTMUX9:
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_OUTMUX_CID_2],
			gseninf_drv_reg_addr[SENINF_ROOT_OUTMUX_CID_2], &root_outmux_cid_value_read);
		if (enable)
			root_outmux_cid_value |= root_outmux_cid_value_read;
		else
			root_outmux_cid_value = root_outmux_cid_value_read & ~root_outmux_cid_value;
		ret |= SECIO_WRITE(
			gseninf_drv_reg_base[SENINF_ROOT_OUTMUX_CID_2],
			gseninf_drv_reg_addr[SENINF_ROOT_OUTMUX_CID_2], root_outmux_cid_value);
		break;
	default:
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "SENINF_CID invalid");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "outmux:");
		CALL_FROM_OPS(putx64, outmux);
		break;
	}

#if LOG_MORE
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "check");
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "ret:");
	CALL_FROM_OPS(putx64, ret);
#endif

	switch (outmux) {
	case SENINF_OUTMUX0:
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_OUTMUX0_CAM_RDY_GRP_SEL],
			gseninf_drv_reg_addr[SENINF_OUTMUX0_CAM_RDY_GRP_SEL],
			&root_rdy_mask_group_id_value_read);
#if LOG_MORE
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "check");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "ret:");
		CALL_FROM_OPS(putx64, ret);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "outmux:");
		CALL_FROM_OPS(putx64, outmux);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "root_rdy_mask_group_id_value_read:");
		CALL_FROM_OPS(putx64, root_rdy_mask_group_id_value_read);
#endif
		if (enable) {
			root_rdy_mask_group_id_value = root_rdy_mask_group_id;
		} else {
			if (root_rdy_mask_group_id_value_read != root_rdy_mask_group_id) {
				ret = SENINF_RETURN_ERROR;
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "Error");
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "outmux:");
				CALL_FROM_OPS(putx64, outmux);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "root_rdy_mask_group_id_value_read:");
				CALL_FROM_OPS(putx64, root_rdy_mask_group_id_value_read);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "root_rdy_mask_group_id:");
				CALL_FROM_OPS(putx64, root_rdy_mask_group_id);
				return ret;
			}
			root_rdy_mask_group_id_value = 0;
		}
		ret |= SECIO_WRITE(
			gseninf_drv_reg_base[SENINF_OUTMUX0_CAM_RDY_GRP_SEL],
			gseninf_drv_reg_addr[SENINF_OUTMUX0_CAM_RDY_GRP_SEL],
			root_rdy_mask_group_id_value);
		break;
	case SENINF_OUTMUX1:
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_OUTMUX1_CAM_RDY_GRP_SEL],
			gseninf_drv_reg_addr[SENINF_OUTMUX1_CAM_RDY_GRP_SEL],
			&root_rdy_mask_group_id_value_read);
#if LOG_MORE
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "check");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "ret:");
		CALL_FROM_OPS(putx64, ret);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "outmux:");
		CALL_FROM_OPS(putx64, outmux);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "root_rdy_mask_group_id_value_read:");
		CALL_FROM_OPS(putx64, root_rdy_mask_group_id_value_read);
#endif
		if (enable) {
			root_rdy_mask_group_id_value = root_rdy_mask_group_id;
		} else {
			if (root_rdy_mask_group_id_value_read != root_rdy_mask_group_id) {
				ret = SENINF_RETURN_ERROR;
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "Error");
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "outmux:");
				CALL_FROM_OPS(putx64, outmux);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "root_rdy_mask_group_id_value_read:");
				CALL_FROM_OPS(putx64, root_rdy_mask_group_id_value_read);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "root_rdy_mask_group_id:");
				CALL_FROM_OPS(putx64, root_rdy_mask_group_id);
				return ret;
			}
			root_rdy_mask_group_id_value = 0;
		}
		ret |= SECIO_WRITE(
			gseninf_drv_reg_base[SENINF_OUTMUX1_CAM_RDY_GRP_SEL],
			gseninf_drv_reg_addr[SENINF_OUTMUX1_CAM_RDY_GRP_SEL],
			root_rdy_mask_group_id_value);
		break;
	case SENINF_OUTMUX2:
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_OUTMUX2_CAM_RDY_GRP_SEL],
			gseninf_drv_reg_addr[SENINF_OUTMUX2_CAM_RDY_GRP_SEL],
			&root_rdy_mask_group_id_value_read);
#if LOG_MORE
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "check");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "ret:");
		CALL_FROM_OPS(putx64, ret);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "outmux:");
		CALL_FROM_OPS(putx64, outmux);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "root_rdy_mask_group_id_value_read:");
		CALL_FROM_OPS(putx64, root_rdy_mask_group_id_value_read);
#endif
		if (enable) {
			root_rdy_mask_group_id_value = root_rdy_mask_group_id;
		} else {
			if (root_rdy_mask_group_id_value_read != root_rdy_mask_group_id) {
				ret = SENINF_RETURN_ERROR;
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "Error");
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "outmux:");
				CALL_FROM_OPS(putx64, outmux);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "root_rdy_mask_group_id_value_read:");
				CALL_FROM_OPS(putx64, root_rdy_mask_group_id_value_read);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "root_rdy_mask_group_id:");
				CALL_FROM_OPS(putx64, root_rdy_mask_group_id);
				return ret;
			}
			root_rdy_mask_group_id_value = 0;
		}
		ret |= SECIO_WRITE(
			gseninf_drv_reg_base[SENINF_OUTMUX2_CAM_RDY_GRP_SEL],
			gseninf_drv_reg_addr[SENINF_OUTMUX2_CAM_RDY_GRP_SEL],
			root_rdy_mask_group_id_value);
		break;
	case SENINF_OUTMUX3:
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_OUTMUX3_CAM_RDY_GRP_SEL],
			gseninf_drv_reg_addr[SENINF_OUTMUX3_CAM_RDY_GRP_SEL],
			&root_rdy_mask_group_id_value_read);
#if LOG_MORE
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "check");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "ret:");
		CALL_FROM_OPS(putx64, ret);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "outmux:");
		CALL_FROM_OPS(putx64, outmux);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "root_rdy_mask_group_id_value_read:");
		CALL_FROM_OPS(putx64, root_rdy_mask_group_id_value_read);
#endif
		if (enable) {
			root_rdy_mask_group_id_value = root_rdy_mask_group_id;
		} else {
			if (root_rdy_mask_group_id_value_read != root_rdy_mask_group_id) {
				ret = SENINF_RETURN_ERROR;
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "Error");
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "outmux:");
				CALL_FROM_OPS(putx64, outmux);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "root_rdy_mask_group_id_value_read:");
				CALL_FROM_OPS(putx64, root_rdy_mask_group_id_value_read);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "root_rdy_mask_group_id:");
				CALL_FROM_OPS(putx64, root_rdy_mask_group_id);
				return ret;
			}
			root_rdy_mask_group_id_value = 0;
		}
		ret |= SECIO_WRITE(
			gseninf_drv_reg_base[SENINF_OUTMUX3_CAM_RDY_GRP_SEL],
			gseninf_drv_reg_addr[SENINF_OUTMUX3_CAM_RDY_GRP_SEL],
			root_rdy_mask_group_id_value);
		break;
	case SENINF_OUTMUX4:
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_OUTMUX4_CAM_RDY_GRP_SEL],
			gseninf_drv_reg_addr[SENINF_OUTMUX4_CAM_RDY_GRP_SEL],
			&root_rdy_mask_group_id_value_read);
#if LOG_MORE
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "check");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "ret:");
		CALL_FROM_OPS(putx64, ret);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "outmux:");
		CALL_FROM_OPS(putx64, outmux);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "root_rdy_mask_group_id_value_read:");
		CALL_FROM_OPS(putx64, root_rdy_mask_group_id_value_read);
#endif
		if (enable) {
			root_rdy_mask_group_id_value = root_rdy_mask_group_id;
		} else {
			if (root_rdy_mask_group_id_value_read != root_rdy_mask_group_id) {
				ret = SENINF_RETURN_ERROR;
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "Error");
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "outmux:");
				CALL_FROM_OPS(putx64, outmux);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "root_rdy_mask_group_id_value_read:");
				CALL_FROM_OPS(putx64, root_rdy_mask_group_id_value_read);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "root_rdy_mask_group_id:");
				CALL_FROM_OPS(putx64, root_rdy_mask_group_id);
				return ret;
			}
			root_rdy_mask_group_id_value = 0;
		}
		ret |= SECIO_WRITE(
			gseninf_drv_reg_base[SENINF_OUTMUX4_CAM_RDY_GRP_SEL],
			gseninf_drv_reg_addr[SENINF_OUTMUX4_CAM_RDY_GRP_SEL],
			root_rdy_mask_group_id_value);
		break;
	case SENINF_OUTMUX5:
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_OUTMUX5_CAM_RDY_GRP_SEL],
			gseninf_drv_reg_addr[SENINF_OUTMUX5_CAM_RDY_GRP_SEL],
			&root_rdy_mask_group_id_value_read);
#if LOG_MORE
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "check");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "ret:");
		CALL_FROM_OPS(putx64, ret);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "outmux:");
		CALL_FROM_OPS(putx64, outmux);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "root_rdy_mask_group_id_value_read:");
		CALL_FROM_OPS(putx64, root_rdy_mask_group_id_value_read);
#endif
		if (enable) {
			root_rdy_mask_group_id_value = root_rdy_mask_group_id;
		} else {
			if (root_rdy_mask_group_id_value_read != root_rdy_mask_group_id) {
				ret = SENINF_RETURN_ERROR;
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "Error");
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "outmux:");
				CALL_FROM_OPS(putx64, outmux);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "root_rdy_mask_group_id_value_read:");
				CALL_FROM_OPS(putx64, root_rdy_mask_group_id_value_read);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "root_rdy_mask_group_id:");
				CALL_FROM_OPS(putx64, root_rdy_mask_group_id);
				return ret;
			}
			root_rdy_mask_group_id_value = 0;
		}
		ret |= SECIO_WRITE(
			gseninf_drv_reg_base[SENINF_OUTMUX5_CAM_RDY_GRP_SEL],
			gseninf_drv_reg_addr[SENINF_OUTMUX5_CAM_RDY_GRP_SEL],
			root_rdy_mask_group_id_value);
		break;
	case SENINF_OUTMUX6:
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_OUTMUX6_CAM_RDY_GRP_SEL],
			gseninf_drv_reg_addr[SENINF_OUTMUX6_CAM_RDY_GRP_SEL],
			&root_rdy_mask_group_id_value_read);
#if LOG_MORE
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "check");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "ret:");
		CALL_FROM_OPS(putx64, ret);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "outmux:");
		CALL_FROM_OPS(putx64, outmux);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "root_rdy_mask_group_id_value_read:");
		CALL_FROM_OPS(putx64, root_rdy_mask_group_id_value_read);
#endif
		if (enable) {
			root_rdy_mask_group_id_value = root_rdy_mask_group_id;
		} else {
			if (root_rdy_mask_group_id_value_read != root_rdy_mask_group_id) {
				ret = SENINF_RETURN_ERROR;
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "Error");
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "outmux:");
				CALL_FROM_OPS(putx64, outmux);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "root_rdy_mask_group_id_value_read:");
				CALL_FROM_OPS(putx64, root_rdy_mask_group_id_value_read);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "root_rdy_mask_group_id:");
				CALL_FROM_OPS(putx64, root_rdy_mask_group_id);
				return ret;
			}
			root_rdy_mask_group_id_value = 0;
		}
		ret |= SECIO_WRITE(
			gseninf_drv_reg_base[SENINF_OUTMUX6_CAM_RDY_GRP_SEL],
			gseninf_drv_reg_addr[SENINF_OUTMUX6_CAM_RDY_GRP_SEL],
			root_rdy_mask_group_id_value);
		break;
	case SENINF_OUTMUX7:
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_OUTMUX7_CAM_RDY_GRP_SEL],
			gseninf_drv_reg_addr[SENINF_OUTMUX7_CAM_RDY_GRP_SEL],
			&root_rdy_mask_group_id_value_read);
#if LOG_MORE
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "check");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "ret:");
		CALL_FROM_OPS(putx64, ret);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "outmux:");
		CALL_FROM_OPS(putx64, outmux);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "root_rdy_mask_group_id_value_read:");
		CALL_FROM_OPS(putx64, root_rdy_mask_group_id_value_read);
#endif
		if (enable) {
			root_rdy_mask_group_id_value = root_rdy_mask_group_id;
		} else {
			if (root_rdy_mask_group_id_value_read != root_rdy_mask_group_id) {
				ret = SENINF_RETURN_ERROR;
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "Error");
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "outmux:");
				CALL_FROM_OPS(putx64, outmux);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "root_rdy_mask_group_id_value_read:");
				CALL_FROM_OPS(putx64, root_rdy_mask_group_id_value_read);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "root_rdy_mask_group_id:");
				CALL_FROM_OPS(putx64, root_rdy_mask_group_id);
				return ret;
			}
			root_rdy_mask_group_id_value = 0;
		}
		ret |= SECIO_WRITE(
			gseninf_drv_reg_base[SENINF_OUTMUX7_CAM_RDY_GRP_SEL],
			gseninf_drv_reg_addr[SENINF_OUTMUX7_CAM_RDY_GRP_SEL],
			root_rdy_mask_group_id_value);
		break;
	case SENINF_OUTMUX8:
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_OUTMUX8_CAM_RDY_GRP_SEL],
			gseninf_drv_reg_addr[SENINF_OUTMUX8_CAM_RDY_GRP_SEL],
			&root_rdy_mask_group_id_value_read);
#if LOG_MORE
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "check");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "ret:");
		CALL_FROM_OPS(putx64, ret);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "outmux:");
		CALL_FROM_OPS(putx64, outmux);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "root_rdy_mask_group_id_value_read:");
		CALL_FROM_OPS(putx64, root_rdy_mask_group_id_value_read);
#endif
		if (enable) {
			root_rdy_mask_group_id_value = root_rdy_mask_group_id;
		} else {
			if (root_rdy_mask_group_id_value_read != root_rdy_mask_group_id) {
				ret = SENINF_RETURN_ERROR;
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "Error");
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "outmux:");
				CALL_FROM_OPS(putx64, outmux);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "root_rdy_mask_group_id_value_read:");
				CALL_FROM_OPS(putx64, root_rdy_mask_group_id_value_read);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "root_rdy_mask_group_id:");
				CALL_FROM_OPS(putx64, root_rdy_mask_group_id);
				return ret;
			}
			root_rdy_mask_group_id_value = 0;
		}
		ret |= SECIO_WRITE(
			gseninf_drv_reg_base[SENINF_OUTMUX8_CAM_RDY_GRP_SEL],
			gseninf_drv_reg_addr[SENINF_OUTMUX8_CAM_RDY_GRP_SEL],
			root_rdy_mask_group_id_value);
		break;
	case SENINF_OUTMUX9:
		ret |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_OUTMUX9_CAM_RDY_GRP_SEL],
			gseninf_drv_reg_addr[SENINF_OUTMUX9_CAM_RDY_GRP_SEL],
			&root_rdy_mask_group_id_value_read);
#if LOG_MORE
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "check");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "ret:");
		CALL_FROM_OPS(putx64, ret);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "outmux:");
		CALL_FROM_OPS(putx64, outmux);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "root_rdy_mask_group_id_value_read:");
		CALL_FROM_OPS(putx64, root_rdy_mask_group_id_value_read);
#endif
		if (enable) {
			root_rdy_mask_group_id_value = root_rdy_mask_group_id;
		} else {
			if (root_rdy_mask_group_id_value_read != root_rdy_mask_group_id) {
				ret = SENINF_RETURN_ERROR;
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "Error");
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "outmux:");
				CALL_FROM_OPS(putx64, outmux);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "root_rdy_mask_group_id_value_read:");
				CALL_FROM_OPS(putx64, root_rdy_mask_group_id_value_read);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "root_rdy_mask_group_id:");
				CALL_FROM_OPS(putx64, root_rdy_mask_group_id);
				return ret;
			}
			root_rdy_mask_group_id_value = 0;
		}
		ret |= SECIO_WRITE(
			gseninf_drv_reg_base[SENINF_OUTMUX9_CAM_RDY_GRP_SEL],
			gseninf_drv_reg_addr[SENINF_OUTMUX9_CAM_RDY_GRP_SEL],
			root_rdy_mask_group_id_value);
		break;
	default:
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "SENINF_CID invalid");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "outmux:");
		CALL_FROM_OPS(putx64, outmux);
		break;
	}

	root_rdy_mask_cid_value = cid_value << ((root_rdy_mask_group_id % 4) * 8);

#if LOG_MORE
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "check");
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "ret:");
	CALL_FROM_OPS(putx64, ret);
#endif

	/* config group 9 */
	ret |= SECIO_READ(
		gseninf_drv_reg_base[SENINF_ROOT_RDY_MASK_CID_2],
		gseninf_drv_reg_addr[SENINF_ROOT_RDY_MASK_CID_2], &root_rdy_mask_cid_value_read);
	if (enable)
		root_rdy_mask_cid_value |= root_rdy_mask_cid_value_read;
	else
		root_rdy_mask_cid_value = root_rdy_mask_cid_value_read & ~root_rdy_mask_cid_value;
	ret |= SECIO_WRITE(
		gseninf_drv_reg_base[SENINF_ROOT_RDY_MASK_CID_2],
		gseninf_drv_reg_addr[SENINF_ROOT_RDY_MASK_CID_2], root_rdy_mask_cid_value);

	if (enable) {
		switch (outmux) {
		case SENINF_OUTMUX0:
			ret |= SECIO_READ(
				gseninf_drv_reg_base[SENINF_OUTMUX0_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX0_CSR_CFG_CTRL],
				&seninf_outmux_csr_cfg_ctrl_value_read);

			seninf_outmux_csr_cfg_ctrl_value = seninf_outmux_csr_cfg_ctrl_value_read | (0x1 << 12);

			ret |= SECIO_WRITE(
				gseninf_drv_reg_base[SENINF_OUTMUX0_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX0_CSR_CFG_CTRL],
				seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "csr_cfg_ctrl_value_read:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value_read);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "seninf_outmux_csr_cfg_ctrl_value:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret:");
			CALL_FROM_OPS(putx64, ret);
			break;
		case SENINF_OUTMUX1:
			ret |= SECIO_READ(
				gseninf_drv_reg_base[SENINF_OUTMUX1_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX1_CSR_CFG_CTRL],
				&seninf_outmux_csr_cfg_ctrl_value_read);

			seninf_outmux_csr_cfg_ctrl_value = seninf_outmux_csr_cfg_ctrl_value_read | (0x1 << 12);

			ret |= SECIO_WRITE(
				gseninf_drv_reg_base[SENINF_OUTMUX1_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX1_CSR_CFG_CTRL],
				seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "csr_cfg_ctrl_value_read:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value_read);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "seninf_outmux_csr_cfg_ctrl_value:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret:");
			CALL_FROM_OPS(putx64, ret);
			break;
		case SENINF_OUTMUX2:
			ret |= SECIO_READ(
				gseninf_drv_reg_base[SENINF_OUTMUX2_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX2_CSR_CFG_CTRL],
				&seninf_outmux_csr_cfg_ctrl_value_read);

			seninf_outmux_csr_cfg_ctrl_value = seninf_outmux_csr_cfg_ctrl_value_read | (0x1 << 12);

			ret |= SECIO_WRITE(
				gseninf_drv_reg_base[SENINF_OUTMUX2_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX2_CSR_CFG_CTRL],
				seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "csr_cfg_ctrl_value_read:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value_read);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "seninf_outmux_csr_cfg_ctrl_value:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret:");
			CALL_FROM_OPS(putx64, ret);
			break;
		case SENINF_OUTMUX3:
			ret |= SECIO_READ(
				gseninf_drv_reg_base[SENINF_OUTMUX3_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX3_CSR_CFG_CTRL],
				&seninf_outmux_csr_cfg_ctrl_value_read);

			seninf_outmux_csr_cfg_ctrl_value = seninf_outmux_csr_cfg_ctrl_value_read | (0x1 << 12);

			ret |= SECIO_WRITE(
				gseninf_drv_reg_base[SENINF_OUTMUX3_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX3_CSR_CFG_CTRL],
				seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "csr_cfg_ctrl_value_read:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value_read);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "seninf_outmux_csr_cfg_ctrl_value:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret:");
			CALL_FROM_OPS(putx64, ret);
			break;
		case SENINF_OUTMUX4:
			ret |= SECIO_READ(
				gseninf_drv_reg_base[SENINF_OUTMUX4_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX4_CSR_CFG_CTRL],
				&seninf_outmux_csr_cfg_ctrl_value_read);

			seninf_outmux_csr_cfg_ctrl_value = seninf_outmux_csr_cfg_ctrl_value_read | (0x1 << 12);

			ret |= SECIO_WRITE(
				gseninf_drv_reg_base[SENINF_OUTMUX4_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX4_CSR_CFG_CTRL],
				seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "csr_cfg_ctrl_value_read:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value_read);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "seninf_outmux_csr_cfg_ctrl_value:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret:");
			CALL_FROM_OPS(putx64, ret);
			break;
		case SENINF_OUTMUX5:
			ret |= SECIO_READ(
				gseninf_drv_reg_base[SENINF_OUTMUX5_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX5_CSR_CFG_CTRL],
				&seninf_outmux_csr_cfg_ctrl_value_read);

			seninf_outmux_csr_cfg_ctrl_value = seninf_outmux_csr_cfg_ctrl_value_read | (0x1 << 12);

			ret |= SECIO_WRITE(
				gseninf_drv_reg_base[SENINF_OUTMUX5_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX5_CSR_CFG_CTRL],
				seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "csr_cfg_ctrl_value_read:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value_read);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "seninf_outmux_csr_cfg_ctrl_value:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret:");
			CALL_FROM_OPS(putx64, ret);
			break;
		case SENINF_OUTMUX6:
			ret |= SECIO_READ(
				gseninf_drv_reg_base[SENINF_OUTMUX6_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX6_CSR_CFG_CTRL],
				&seninf_outmux_csr_cfg_ctrl_value_read);

			seninf_outmux_csr_cfg_ctrl_value = seninf_outmux_csr_cfg_ctrl_value_read | (0x1 << 12);

			ret |= SECIO_WRITE(
				gseninf_drv_reg_base[SENINF_OUTMUX6_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX6_CSR_CFG_CTRL],
				seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "csr_cfg_ctrl_value_read:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value_read);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "seninf_outmux_csr_cfg_ctrl_value:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret:");
			CALL_FROM_OPS(putx64, ret);
			break;
		case SENINF_OUTMUX7:
			ret |= SECIO_READ(
				gseninf_drv_reg_base[SENINF_OUTMUX7_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX7_CSR_CFG_CTRL],
				&seninf_outmux_csr_cfg_ctrl_value_read);

			seninf_outmux_csr_cfg_ctrl_value = seninf_outmux_csr_cfg_ctrl_value_read | (0x1 << 12);

			ret |= SECIO_WRITE(
				gseninf_drv_reg_base[SENINF_OUTMUX7_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX7_CSR_CFG_CTRL],
				seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "csr_cfg_ctrl_value_read:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value_read);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "seninf_outmux_csr_cfg_ctrl_value:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret:");
			CALL_FROM_OPS(putx64, ret);
			break;
		case SENINF_OUTMUX8:
			ret |= SECIO_READ(
				gseninf_drv_reg_base[SENINF_OUTMUX8_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX8_CSR_CFG_CTRL],
				&seninf_outmux_csr_cfg_ctrl_value_read);

			seninf_outmux_csr_cfg_ctrl_value = seninf_outmux_csr_cfg_ctrl_value_read | (0x1 << 12);

			ret |= SECIO_WRITE(
				gseninf_drv_reg_base[SENINF_OUTMUX8_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX8_CSR_CFG_CTRL],
				seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "csr_cfg_ctrl_value_read:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value_read);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "seninf_outmux_csr_cfg_ctrl_value:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret:");
			CALL_FROM_OPS(putx64, ret);
			break;
		case SENINF_OUTMUX9:
			ret |= SECIO_READ(
				gseninf_drv_reg_base[SENINF_OUTMUX9_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX9_CSR_CFG_CTRL],
				&seninf_outmux_csr_cfg_ctrl_value_read);

			seninf_outmux_csr_cfg_ctrl_value = seninf_outmux_csr_cfg_ctrl_value_read | (0x1 << 12);

			ret |= SECIO_WRITE(
				gseninf_drv_reg_base[SENINF_OUTMUX9_CSR_CFG_CTRL],
				gseninf_drv_reg_addr[SENINF_OUTMUX9_CSR_CFG_CTRL],
				seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "csr_cfg_ctrl_value_read:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value_read);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "seninf_outmux_csr_cfg_ctrl_value:");
			CALL_FROM_OPS(putx64, seninf_outmux_csr_cfg_ctrl_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret:");
			CALL_FROM_OPS(putx64, ret);
			break;
		default:
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "invalid");
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux:");
			CALL_FROM_OPS(putx64, outmux);
			break;
		}
	}

#if LOG_MORE
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "check");
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "ret:");
	CALL_FROM_OPS(putx64, ret);
	cid_value = 0;
	ret |= SECIO_READ(
		gseninf_drv_reg_base[SENINF_ROOT_SENSOR_CID_0],
		gseninf_drv_reg_addr[SENINF_ROOT_SENSOR_CID_0], &cid_value);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "SENINF_ROOT_SENSOR_CID_0 read done");
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "ret:");
	CALL_FROM_OPS(putx64, ret);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "cid_value:");
	CALL_FROM_OPS(putx64, cid_value);
	cid_value = 0;
	ret |= SECIO_READ(
		gseninf_drv_reg_base[SENINF_ROOT_SENSOR_CID_1],
		gseninf_drv_reg_addr[SENINF_ROOT_SENSOR_CID_1], &cid_value);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "SENINF_ROOT_SENSOR_CID_1 read done");
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "ret:");
	CALL_FROM_OPS(putx64, ret);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "cid_value:");
	CALL_FROM_OPS(putx64, cid_value);
	cid_value = 0;
	ret |= SECIO_READ(
		gseninf_drv_reg_base[SENINF_ROOT_SENSOR_CID_2],
		gseninf_drv_reg_addr[SENINF_ROOT_SENSOR_CID_2], &cid_value);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "SENINF_ROOT_SENSOR_CID_2 read done");
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "ret:");
	CALL_FROM_OPS(putx64, ret);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "cid_value:");
	CALL_FROM_OPS(putx64, cid_value);
	cid_value = 0;
	ret |= SECIO_READ(
		gseninf_drv_reg_base[SENINF_ROOT_SENSOR_CID_3],
		gseninf_drv_reg_addr[SENINF_ROOT_SENSOR_CID_3], &cid_value);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "SENINF_ROOT_SENSOR_CID_3 read done");
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "ret:");
	CALL_FROM_OPS(putx64, ret);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "cid_value:");
	CALL_FROM_OPS(putx64, cid_value);
	cid_value = 0;
	ret |= SECIO_READ(
		gseninf_drv_reg_base[SENINF_ROOT_SENSOR_CID_4],
		gseninf_drv_reg_addr[SENINF_ROOT_SENSOR_CID_4], &cid_value);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "SENINF_ROOT_SENSOR_CID_4 read done");
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "ret:");
	CALL_FROM_OPS(putx64, ret);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "cid_value:");
	CALL_FROM_OPS(putx64, cid_value);
	cid_value = 0;
	ret |= SECIO_READ(
		gseninf_drv_reg_base[SENINF_ROOT_SENSOR_CID_5],
		gseninf_drv_reg_addr[SENINF_ROOT_SENSOR_CID_5], &cid_value);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "SENINF_ROOT_SENSOR_CID_5 read done");
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "ret:");
	CALL_FROM_OPS(putx64, ret);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "cid_value:");
	CALL_FROM_OPS(putx64, cid_value);

	cid_value = 0;
	ret |= SECIO_READ(
		gseninf_drv_reg_base[SENINF_ROOT_OUTMUX_CID_0],
		gseninf_drv_reg_addr[SENINF_ROOT_OUTMUX_CID_0], &cid_value);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "SENINF_ROOT_CID_0 read done");
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "ret:");
	CALL_FROM_OPS(putx64, ret);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "cid_value:");
	CALL_FROM_OPS(putx64, cid_value);
	cid_value = 0;
	ret |= SECIO_READ(
		gseninf_drv_reg_base[SENINF_ROOT_OUTMUX_CID_1],
		gseninf_drv_reg_addr[SENINF_ROOT_OUTMUX_CID_1], &cid_value);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "SENINF_ROOT_CID_1 read done");
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "ret:");
	CALL_FROM_OPS(putx64, ret);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "cid_value:");
	CALL_FROM_OPS(putx64, cid_value);
	cid_value = 0;
	ret |= SECIO_READ(
		gseninf_drv_reg_base[SENINF_ROOT_OUTMUX_CID_2],
		gseninf_drv_reg_addr[SENINF_ROOT_OUTMUX_CID_2], &cid_value);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "SENINF_ROOT_CID_2 read done");
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "ret:");
	CALL_FROM_OPS(putx64, ret);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "cid_value:");
	CALL_FROM_OPS(putx64, cid_value);
	cid_value = 0;
	ret |= SECIO_READ(
		gseninf_drv_reg_base[SENINF_ROOT_RDY_MASK_CID_0],
		gseninf_drv_reg_addr[SENINF_ROOT_RDY_MASK_CID_0], &cid_value);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "SENINF_ROOT_RDY_MASK_CID_0 read done");
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "ret:");
	CALL_FROM_OPS(putx64, ret);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "cid_value:");
	CALL_FROM_OPS(putx64, cid_value);
	cid_value = 0;
	ret |= SECIO_READ(
		gseninf_drv_reg_base[SENINF_ROOT_RDY_MASK_CID_1],
		gseninf_drv_reg_addr[SENINF_ROOT_RDY_MASK_CID_1], &cid_value);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "SENINF_ROOT_RDY_MASK_CID_1 read done");
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "ret:");
	CALL_FROM_OPS(putx64, ret);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "cid_value:");
	CALL_FROM_OPS(putx64, cid_value);
	cid_value = 0;
	ret |= SECIO_READ(
		gseninf_drv_reg_base[SENINF_ROOT_RDY_MASK_CID_2],
		gseninf_drv_reg_addr[SENINF_ROOT_RDY_MASK_CID_2], &cid_value);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "SENINF_ROOT_RDY_MASK_CID_2 read done");
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "ret:");
	CALL_FROM_OPS(putx64, ret);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "cid_value:");
	CALL_FROM_OPS(putx64, cid_value);
#endif

	return ret;
}
#else

#endif /* SENINF_CID_EN */

int seninf_lock_exclude_enable(SENINF_TEE_REG *preg,
	SENINF_ASYNC_ENUM seninf, SENINF_OUTMUX_ENUM outmux)
{
	int ret = SENINF_RETURN_SUCCESS;

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "enter");

	if (g_preg_va == NULL) {
		ret = SENINF_RETURN_ERROR_MAPPING_FAIL;
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "invalid address");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "ret:");
		CALL_FROM_OPS(putx64, ret);
		return ret;
	}

	if (seninf >= SENINF_ASYNC_NUM || seninf < SENINF_ASYNC_0)
		return SENINF_RETURN_ERROR;

	if (outmux >= SENINF_OUTMUX_NUM || outmux < SENINF_OUTMUX0)
		return SENINF_RETURN_ERROR;

	g_seninf = seninf;
	g_seninf_outmux[g_count] = outmux;
#if SENINF_CID_EN
	ret |= seninf_CID_handler(seninf, outmux, 1);
#else

#endif /* SENINF_CID_EN */
	return ret;
}

int seninf_lock_exclude_disable(void)
{
	int ret = SENINF_RETURN_SUCCESS;

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "enter");

	if (g_preg_va == NULL) {
		ret = SENINF_RETURN_ERROR_MAPPING_FAIL;
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "invalid address");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "ret:");
		CALL_FROM_OPS(putx64, ret);
		return ret;
	}

	if (g_count > 0) {
		while (g_count) {
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "disable start");
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux:");
			CALL_FROM_OPS(putx64, g_seninf_outmux[g_count]);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "g_count:");
			CALL_FROM_OPS(putx64, g_count);
			ret |= seninf_CID_handler(g_seninf, g_seninf_outmux[g_count], 0);
			g_seninf_outmux[g_count] = SENINF_OUTMUX_ERROR;
			g_count--;
		}
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "disable done");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "g_count:");
		CALL_FROM_OPS(putx64, g_count);
	}

	g_preg_va = 0;
	g_seninf = SENINF_ASYNC_ENUM_ERROR;
	g_seninf_lock_flag = 0;
	g_multi_sensor_individual_flag = 0;
	g_multi_sensor_current_port = -1;
	g_multi_secure_csi_port_front = CUSTOM_CFG_CSI_PORT_NONE;
	g_multi_secure_csi_port_rear = CUSTOM_CFG_CSI_PORT_NONE;
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "g_multi_sensor_individual_flag:");
	CALL_FROM_OPS(putx64, g_multi_sensor_individual_flag);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "g_multi_sensor_current_port:");
	CALL_FROM_OPS(putx64, g_multi_sensor_current_port);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "g_multi_secure_csi_port_front:");
	CALL_FROM_OPS(putx64, g_multi_secure_csi_port_front);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "g_multi_secure_csi_port_rear:");
	CALL_FROM_OPS(putx64, g_multi_secure_csi_port_rear);

	return ret;
}

#define CMASV_TAG_MIN 0
#define CMASV_TAG_MAX 7
#define CMA_TAG_MIN 0
#define CMA_TAG_MAX 2
SENINF_RETURN seninf_check_outmux_dt(SENINF_OUTMUX_ENUM outmux)
{
	SENINF_RETURN ret = SENINF_RETURN_ERROR;
	int ret_err = SENINF_RETURN_SUCCESS;
	uint32_t outmux_tag_vcdt_value = 0;
	uint32_t outmux_tag_en = 0;
	uint32_t outmux_tag_dt_value = 0;
	int tag = 0;

	switch (outmux) {
	case SENINF_OUTMUX0:
		for (tag = CMASV_TAG_MIN; tag <= CMASV_TAG_MAX; tag++) {
			switch(tag) {
			case 0:
				ret_err |= SECIO_READ(
					gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_0_TAG_VCDT_FILT_0],
					gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_0_TAG_VCDT_FILT_0],
					&outmux_tag_vcdt_value);
				break;
			case 1:
				ret_err |= SECIO_READ(
					gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_0_TAG_VCDT_FILT_1],
					gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_0_TAG_VCDT_FILT_1],
					&outmux_tag_vcdt_value);
				break;
			case 2:
				ret_err |= SECIO_READ(
					gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_0_TAG_VCDT_FILT_2],
					gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_0_TAG_VCDT_FILT_2],
					&outmux_tag_vcdt_value);
				break;
			}

			if (ret_err != SENINF_RETURN_SUCCESS) {
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "REG fail");
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "tag:");
				CALL_FROM_OPS(putx64, tag);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "ret_err:");
				CALL_FROM_OPS(putx64, ret_err);
				return SENINF_RETURN_ERROR;
			}

			outmux_tag_en = outmux_tag_vcdt_value & 0x1;
			outmux_tag_dt_value = (outmux_tag_vcdt_value >> 16) & 0x3F;
			if (outmux_tag_en &&
				((outmux_tag_dt_value >= 0x28) && (outmux_tag_dt_value <= 0x2F))) {
				ret = SENINF_RETURN_SUCCESS;
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "check");
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "ret:");
				CALL_FROM_OPS(putx64, ret);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "outmux:");
				CALL_FROM_OPS(putx64, outmux);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "tag:");
				CALL_FROM_OPS(putx64, tag);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "outmux_tag_vcdt_value:");
				CALL_FROM_OPS(putx64, outmux_tag_vcdt_value);
				break;
			}
#if LOG_MORE
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "check");
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret:");
			CALL_FROM_OPS(putx64, ret);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux:");
			CALL_FROM_OPS(putx64, outmux);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "tag:");
			CALL_FROM_OPS(putx64, tag);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux_tag_vcdt_value:");
			CALL_FROM_OPS(putx64, outmux_tag_vcdt_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux_tag_en:");
			CALL_FROM_OPS(putx64, outmux_tag_en);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux_tag_dt_value:");
			CALL_FROM_OPS(putx64, outmux_tag_dt_value);
#endif
		}
		break;
	case SENINF_OUTMUX1:
		for (tag = CMASV_TAG_MIN; tag <= CMASV_TAG_MAX; tag++) {
			switch(tag) {
			case 0:
				ret_err |= SECIO_READ(
					gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_1_TAG_VCDT_FILT_0],
					gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_1_TAG_VCDT_FILT_0],
					&outmux_tag_vcdt_value);
				break;
			case 1:
				ret_err |= SECIO_READ(
					gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_1_TAG_VCDT_FILT_1],
					gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_1_TAG_VCDT_FILT_1],
					&outmux_tag_vcdt_value);
				break;
			case 2:
				ret_err |= SECIO_READ(
					gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_1_TAG_VCDT_FILT_2],
					gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_1_TAG_VCDT_FILT_2],
					&outmux_tag_vcdt_value);
				break;
			}

			if (ret_err != SENINF_RETURN_SUCCESS) {
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "REG fail");
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "tag:");
				CALL_FROM_OPS(putx64, tag);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "ret_err:");
				CALL_FROM_OPS(putx64, ret_err);
				return SENINF_RETURN_ERROR;
			}

			outmux_tag_en = outmux_tag_vcdt_value & 0x1;
			outmux_tag_dt_value = (outmux_tag_vcdt_value >> 16) & 0x3F;
			if (outmux_tag_en &&
				((outmux_tag_dt_value >= 0x28) && (outmux_tag_dt_value <= 0x2F))) {
				ret = SENINF_RETURN_SUCCESS;
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "check");
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "ret:");
				CALL_FROM_OPS(putx64, ret);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "outmux:");
				CALL_FROM_OPS(putx64, outmux);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "tag:");
				CALL_FROM_OPS(putx64, tag);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "outmux_tag_vcdt_value:");
				CALL_FROM_OPS(putx64, outmux_tag_vcdt_value);
				break;
			}
#if LOG_MORE
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "check");
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret:");
			CALL_FROM_OPS(putx64, ret);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux:");
			CALL_FROM_OPS(putx64, outmux);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "tag:");
			CALL_FROM_OPS(putx64, tag);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux_tag_vcdt_value:");
			CALL_FROM_OPS(putx64, outmux_tag_vcdt_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux_tag_en:");
			CALL_FROM_OPS(putx64, outmux_tag_en);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux_tag_dt_value:");
			CALL_FROM_OPS(putx64, outmux_tag_dt_value);
#endif
		}
		break;
	case SENINF_OUTMUX2:
		for (tag = CMASV_TAG_MIN; tag <= CMASV_TAG_MAX; tag++) {
			switch(tag) {
			case 0:
				ret_err |= SECIO_READ(
					gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_2_TAG_VCDT_FILT_0],
					gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_2_TAG_VCDT_FILT_0],
					&outmux_tag_vcdt_value);
				break;
			case 1:
				ret_err |= SECIO_READ(
					gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_2_TAG_VCDT_FILT_1],
					gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_2_TAG_VCDT_FILT_1],
					&outmux_tag_vcdt_value);
				break;
			case 2:
				ret_err |= SECIO_READ(
					gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_2_TAG_VCDT_FILT_2],
					gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_2_TAG_VCDT_FILT_2],
					&outmux_tag_vcdt_value);
				break;
			}

			if (ret_err != SENINF_RETURN_SUCCESS) {
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "REG fail");
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "tag:");
				CALL_FROM_OPS(putx64, tag);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "ret_err:");
				CALL_FROM_OPS(putx64, ret_err);
				return SENINF_RETURN_ERROR;
			}

			outmux_tag_en = outmux_tag_vcdt_value & 0x1;
			outmux_tag_dt_value = (outmux_tag_vcdt_value >> 16) & 0x3F;
			if (outmux_tag_en &&
				((outmux_tag_dt_value >= 0x28) && (outmux_tag_dt_value <= 0x2F))) {
				ret = SENINF_RETURN_SUCCESS;
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "check");
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "ret:");
				CALL_FROM_OPS(putx64, ret);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "outmux:");
				CALL_FROM_OPS(putx64, outmux);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "tag:");
				CALL_FROM_OPS(putx64, tag);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "outmux_tag_vcdt_value:");
				CALL_FROM_OPS(putx64, outmux_tag_vcdt_value);
				break;
			}
#if LOG_MORE
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "check");
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret:");
			CALL_FROM_OPS(putx64, ret);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux:");
			CALL_FROM_OPS(putx64, outmux);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "tag:");
			CALL_FROM_OPS(putx64, tag);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux_tag_vcdt_value:");
			CALL_FROM_OPS(putx64, outmux_tag_vcdt_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux_tag_en:");
			CALL_FROM_OPS(putx64, outmux_tag_en);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux_tag_dt_value:");
			CALL_FROM_OPS(putx64, outmux_tag_dt_value);
#endif
		}
		break;
	case SENINF_OUTMUX3:
		for (tag = CMASV_TAG_MIN; tag <= CMASV_TAG_MAX; tag++) {
			switch(tag) {
			case 0:
				ret_err |= SECIO_READ(
					gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_3_TAG_VCDT_FILT_0],
					gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_3_TAG_VCDT_FILT_0],
					&outmux_tag_vcdt_value);
				break;
			case 1:
				ret_err |= SECIO_READ(
					gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_3_TAG_VCDT_FILT_1],
					gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_3_TAG_VCDT_FILT_1],
					&outmux_tag_vcdt_value);
				break;
			case 2:
				ret_err |= SECIO_READ(
					gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_3_TAG_VCDT_FILT_2],
					gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_3_TAG_VCDT_FILT_2],
					&outmux_tag_vcdt_value);
				break;
			}

			if (ret_err != SENINF_RETURN_SUCCESS) {
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "REG fail");
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "tag:");
				CALL_FROM_OPS(putx64, tag);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "ret_err:");
				CALL_FROM_OPS(putx64, ret_err);
				return SENINF_RETURN_ERROR;
			}

			outmux_tag_en = outmux_tag_vcdt_value & 0x1;
			outmux_tag_dt_value = (outmux_tag_vcdt_value >> 16) & 0x3F;
			if (outmux_tag_en &&
				((outmux_tag_dt_value >= 0x28) && (outmux_tag_dt_value <= 0x2F))) {
				ret = SENINF_RETURN_SUCCESS;
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "check");
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "ret:");
				CALL_FROM_OPS(putx64, ret);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "outmux:");
				CALL_FROM_OPS(putx64, outmux);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "tag:");
				CALL_FROM_OPS(putx64, tag);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "outmux_tag_vcdt_value:");
				CALL_FROM_OPS(putx64, outmux_tag_vcdt_value);
				break;
			}
#if LOG_MORE
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "check");
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret:");
			CALL_FROM_OPS(putx64, ret);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux:");
			CALL_FROM_OPS(putx64, outmux);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "tag:");
			CALL_FROM_OPS(putx64, tag);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux_tag_vcdt_value:");
			CALL_FROM_OPS(putx64, outmux_tag_vcdt_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux_tag_en:");
			CALL_FROM_OPS(putx64, outmux_tag_en);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux_tag_dt_value:");
			CALL_FROM_OPS(putx64, outmux_tag_dt_value);
#endif
		}
		break;
	case SENINF_OUTMUX4:
		for (tag = CMASV_TAG_MIN; tag <= CMASV_TAG_MAX; tag++) {
			switch(tag) {
			case 0:
				ret_err |= SECIO_READ(
					gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_4_TAG_VCDT_FILT_0],
					gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_4_TAG_VCDT_FILT_0],
					&outmux_tag_vcdt_value);
				break;
			case 1:
				ret_err |= SECIO_READ(
					gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_4_TAG_VCDT_FILT_1],
					gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_4_TAG_VCDT_FILT_1],
					&outmux_tag_vcdt_value);
				break;
			case 2:
				ret_err |= SECIO_READ(
					gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_4_TAG_VCDT_FILT_2],
					gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_4_TAG_VCDT_FILT_2],
					&outmux_tag_vcdt_value);
				break;
			}

			if (ret_err != SENINF_RETURN_SUCCESS) {
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "REG fail");
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "tag:");
				CALL_FROM_OPS(putx64, tag);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "ret_err:");
				CALL_FROM_OPS(putx64, ret_err);
				return SENINF_RETURN_ERROR;
			}

			outmux_tag_en = outmux_tag_vcdt_value & 0x1;
			outmux_tag_dt_value = (outmux_tag_vcdt_value >> 16) & 0x3F;
			if (outmux_tag_en &&
				((outmux_tag_dt_value >= 0x28) && (outmux_tag_dt_value <= 0x2F))) {
				ret = SENINF_RETURN_SUCCESS;
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "check");
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "ret:");
				CALL_FROM_OPS(putx64, ret);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "outmux:");
				CALL_FROM_OPS(putx64, outmux);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "tag:");
				CALL_FROM_OPS(putx64, tag);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "outmux_tag_vcdt_value:");
				CALL_FROM_OPS(putx64, outmux_tag_vcdt_value);
				break;
			}
#if LOG_MORE
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "check");
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret:");
			CALL_FROM_OPS(putx64, ret);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux:");
			CALL_FROM_OPS(putx64, outmux);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "tag:");
			CALL_FROM_OPS(putx64, tag);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux_tag_vcdt_value:");
			CALL_FROM_OPS(putx64, outmux_tag_vcdt_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux_tag_en:");
			CALL_FROM_OPS(putx64, outmux_tag_en);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux_tag_dt_value:");
			CALL_FROM_OPS(putx64, outmux_tag_dt_value);
#endif
		}
		break;
	case SENINF_OUTMUX5:
		for (tag = CMASV_TAG_MIN; tag <= CMASV_TAG_MAX; tag++) {
			switch(tag) {
			case 0:
				ret_err |= SECIO_READ(
					gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_5_TAG_VCDT_FILT_0],
					gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_5_TAG_VCDT_FILT_0],
					&outmux_tag_vcdt_value);
				break;
			case 1:
				ret_err |= SECIO_READ(
					gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_5_TAG_VCDT_FILT_1],
					gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_5_TAG_VCDT_FILT_1],
					&outmux_tag_vcdt_value);
				break;
			case 2:
				ret_err |= SECIO_READ(
					gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_5_TAG_VCDT_FILT_2],
					gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_5_TAG_VCDT_FILT_2],
					&outmux_tag_vcdt_value);
				break;
			}

			if (ret_err != SENINF_RETURN_SUCCESS) {
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "REG fail");
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "tag:");
				CALL_FROM_OPS(putx64, tag);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "ret_err:");
				CALL_FROM_OPS(putx64, ret_err);
				return SENINF_RETURN_ERROR;
			}

			outmux_tag_en = outmux_tag_vcdt_value & 0x1;
			outmux_tag_dt_value = (outmux_tag_vcdt_value >> 16) & 0x3F;
			if (outmux_tag_en &&
				((outmux_tag_dt_value >= 0x28) && (outmux_tag_dt_value <= 0x2F))) {
				ret = SENINF_RETURN_SUCCESS;
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "check");
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "ret:");
				CALL_FROM_OPS(putx64, ret);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "outmux:");
				CALL_FROM_OPS(putx64, outmux);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "tag:");
				CALL_FROM_OPS(putx64, tag);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "outmux_tag_vcdt_value:");
				CALL_FROM_OPS(putx64, outmux_tag_vcdt_value);
				break;
			}
#if LOG_MORE
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "check");
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret:");
			CALL_FROM_OPS(putx64, ret);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux:");
			CALL_FROM_OPS(putx64, outmux);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "tag:");
			CALL_FROM_OPS(putx64, tag);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux_tag_vcdt_value:");
			CALL_FROM_OPS(putx64, outmux_tag_vcdt_value);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux_tag_en:");
			CALL_FROM_OPS(putx64, outmux_tag_en);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux_tag_dt_value:");
			CALL_FROM_OPS(putx64, outmux_tag_dt_value);
#endif
		}
		break;
	case SENINF_OUTMUX6:
		ret_err |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_6_TAG_VCDT_FILT_0],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_6_TAG_VCDT_FILT_0],
			&outmux_tag_vcdt_value);

		if (ret_err != SENINF_RETURN_SUCCESS) {
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "REG fail");
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret_err:");
			CALL_FROM_OPS(putx64, ret_err);
			return SENINF_RETURN_ERROR;
		}
		outmux_tag_en = outmux_tag_vcdt_value & 0x1;
		outmux_tag_dt_value = (outmux_tag_vcdt_value >> 16) & 0x3F;
		if (outmux_tag_en &&
			((outmux_tag_dt_value >= 0x28) && (outmux_tag_dt_value <= 0x2F))) {
			ret = SENINF_RETURN_SUCCESS;
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "check");
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret:");
			CALL_FROM_OPS(putx64, ret);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux:");
			CALL_FROM_OPS(putx64, outmux);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "tag:");
			CALL_FROM_OPS(putx64, tag);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux_tag_vcdt_value:");
			CALL_FROM_OPS(putx64, outmux_tag_vcdt_value);
			break;
		}
#if LOG_MORE
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "check");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "ret:");
		CALL_FROM_OPS(putx64, ret);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "outmux:");
		CALL_FROM_OPS(putx64, outmux);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "tag:");
		CALL_FROM_OPS(putx64, tag);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "outmux_tag_vcdt_value:");
		CALL_FROM_OPS(putx64, outmux_tag_vcdt_value);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "outmux_tag_en:");
		CALL_FROM_OPS(putx64, outmux_tag_en);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "outmux_tag_dt_value:");
		CALL_FROM_OPS(putx64, outmux_tag_dt_value);
#endif
		break;
	case SENINF_OUTMUX7:
		ret_err |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_7_TAG_VCDT_FILT_0],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_7_TAG_VCDT_FILT_0],
			&outmux_tag_vcdt_value);

		if (ret_err != SENINF_RETURN_SUCCESS) {
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "REG fail");
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret_err:");
			CALL_FROM_OPS(putx64, ret_err);
			return SENINF_RETURN_ERROR;
		}
		outmux_tag_en = outmux_tag_vcdt_value & 0x1;
		outmux_tag_dt_value = (outmux_tag_vcdt_value >> 16) & 0x3F;
		if (outmux_tag_en &&
			((outmux_tag_dt_value >= 0x28) && (outmux_tag_dt_value <= 0x2F))) {
			ret = SENINF_RETURN_SUCCESS;
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "check");
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret:");
			CALL_FROM_OPS(putx64, ret);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux:");
			CALL_FROM_OPS(putx64, outmux);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "tag:");
			CALL_FROM_OPS(putx64, tag);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux_tag_vcdt_value:");
			CALL_FROM_OPS(putx64, outmux_tag_vcdt_value);
			break;
		}
#if LOG_MORE
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "check");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "ret:");
		CALL_FROM_OPS(putx64, ret);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "outmux:");
		CALL_FROM_OPS(putx64, outmux);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "tag:");
		CALL_FROM_OPS(putx64, tag);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "outmux_tag_vcdt_value:");
		CALL_FROM_OPS(putx64, outmux_tag_vcdt_value);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "outmux_tag_en:");
		CALL_FROM_OPS(putx64, outmux_tag_en);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "outmux_tag_dt_value:");
		CALL_FROM_OPS(putx64, outmux_tag_dt_value);
#endif
		break;
	case SENINF_OUTMUX8:
		ret_err |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_8_TAG_VCDT_FILT_0],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_8_TAG_VCDT_FILT_0],
			&outmux_tag_vcdt_value);

		if (ret_err != SENINF_RETURN_SUCCESS) {
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "REG fail");
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret_err:");
			CALL_FROM_OPS(putx64, ret_err);
			return SENINF_RETURN_ERROR;
		}
		outmux_tag_en = outmux_tag_vcdt_value & 0x1;
		outmux_tag_dt_value = (outmux_tag_vcdt_value >> 16) & 0x3F;
		if (outmux_tag_en &&
			((outmux_tag_dt_value >= 0x28) && (outmux_tag_dt_value <= 0x2F))) {
			ret = SENINF_RETURN_SUCCESS;
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "check");
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret:");
			CALL_FROM_OPS(putx64, ret);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux:");
			CALL_FROM_OPS(putx64, outmux);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "tag:");
			CALL_FROM_OPS(putx64, tag);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux_tag_vcdt_value:");
			CALL_FROM_OPS(putx64, outmux_tag_vcdt_value);
			break;
		}
#if LOG_MORE
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "check");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "ret:");
		CALL_FROM_OPS(putx64, ret);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "outmux:");
		CALL_FROM_OPS(putx64, outmux);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "tag:");
		CALL_FROM_OPS(putx64, tag);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "outmux_tag_vcdt_value:");
		CALL_FROM_OPS(putx64, outmux_tag_vcdt_value);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "outmux_tag_en:");
		CALL_FROM_OPS(putx64, outmux_tag_en);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "outmux_tag_dt_value:");
		CALL_FROM_OPS(putx64, outmux_tag_dt_value);
#endif
		break;
	case SENINF_OUTMUX9:
		ret_err |= SECIO_READ(
			gseninf_drv_reg_base[SENINF_ROOT_SECURE_OUTMUX_9_TAG_VCDT_FILT_0],
			gseninf_drv_reg_addr[SENINF_ROOT_SECURE_OUTMUX_9_TAG_VCDT_FILT_0],
			&outmux_tag_vcdt_value);

		if (ret_err != SENINF_RETURN_SUCCESS) {
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "REG fail");
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret_err:");
			CALL_FROM_OPS(putx64, ret_err);
			return SENINF_RETURN_ERROR;
		}
		outmux_tag_en = outmux_tag_vcdt_value & 0x1;
		outmux_tag_dt_value = (outmux_tag_vcdt_value >> 16) & 0x3F;
		if (outmux_tag_en &&
			((outmux_tag_dt_value >= 0x28) && (outmux_tag_dt_value <= 0x2F))) {
			ret = SENINF_RETURN_SUCCESS;
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "check");
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret:");
			CALL_FROM_OPS(putx64, ret);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux:");
			CALL_FROM_OPS(putx64, outmux);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "tag:");
			CALL_FROM_OPS(putx64, tag);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "outmux_tag_vcdt_value:");
			CALL_FROM_OPS(putx64, outmux_tag_vcdt_value);
			break;
		}
#if LOG_MORE
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "check");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "ret:");
		CALL_FROM_OPS(putx64, ret);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "outmux:");
		CALL_FROM_OPS(putx64, outmux);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "tag:");
		CALL_FROM_OPS(putx64, tag);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "outmux_tag_vcdt_value:");
		CALL_FROM_OPS(putx64, outmux_tag_vcdt_value);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "outmux_tag_en:");
		CALL_FROM_OPS(putx64, outmux_tag_en);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "outmux_tag_dt_value:");
		CALL_FROM_OPS(putx64, outmux_tag_dt_value);
#endif
		break;
	default:
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "invalid");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "outmux:");
		CALL_FROM_OPS(putx64, outmux);
		break;
	}

	return ret;
}

SENINF_RETURN seninf_auth(SENINF_TEE_REG *preg, uint64_t pa)
{
	SENINF_RETURN ret = SENINF_RETURN_SUCCESS;
	bool has_secured_path = false;
	SecMgr_CamInfo *sensor_check = NULL;
	uint32_t target_outmux = 0;
#ifdef SENINF_UT
	uint32_t seninf_ut_outmux;
#endif
	void *pfixmap = NULL;

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "enter");
#ifdef DAPC_EN
	ret = (SENINF_RETURN)seninf_dapc_lock();

	if (ret != SENINF_RETURN_SUCCESS) {
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "seninf dapc lock error!");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "ret:");
		CALL_FROM_OPS(putx64, ret);
		return SENINF_RETURN_ERROR;
	}
#else
	if (g_seninf_lock_flag) {
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "has secure already");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "g_seninf_lock_flag:");
		CALL_FROM_OPS(putx64, g_seninf_lock_flag);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "ret:");
		CALL_FROM_OPS(putx64, ret);
		return ret;
	}
#endif

#if LOG_MORE  // def SENINF_UT
	uint32_t *pregister = (uint32_t *)preg;

	for (uint32_t k = 0; k < sizeof(SENINF_TEE_REG) / sizeof(uint32_t); k++) {
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "k:");
		CALL_FROM_OPS(putx64, k);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "reg");
		CALL_FROM_OPS(putx64, pregister[k]);
	}

#if DUMP_MORE
	for (uint32_t j = 0; j < SENINF_OUTMUX_NUM; j++) {
		SENINF_TEE_REG_OUTMUX *preg_outmux = preg->seninf_outmux;

		preg_outmux += j;
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "j:");
		CALL_FROM_OPS(putx64, j);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "preg_outmux");
		CALL_FROM_OPS(putx64, preg_outmux);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "preg_outmux->SENINF_TEE_OUTMUX_SRC_SEL");
		CALL_FROM_OPS(putx64, preg_outmux->SENINF_TEE_OUTMUX_SRC_SEL);
	}
#endif
#endif

	for (int i = 0; i < SENINF_OUTMUX_NUM; i++) {
		if (!seninf_auth_is_outmux_en(preg, (SENINF_OUTMUX_ENUM)i))
			continue;

		SENINF_ASYNC_ENUM seninf = seninf_auth_get_async(preg, (SENINF_OUTMUX_ENUM)i);
		CUSTOM_CFG_SECURE secure = sensor_cfg_sec_from_seninf(seninf);

		if (secure == CUSTOM_CFG_SECURE_NONE)
			continue;

		ret = seninf_check_outmux_dt((SENINF_OUTMUX_ENUM)i);//input outmux id

		if (ret) {//check fail
			/* Non RAW path */
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "dt mismatch");
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "i:");
			CALL_FROM_OPS(putx64, i);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "seninf_outmux:");
			CALL_FROM_OPS(putx64, (SENINF_OUTMUX_ENUM)i);
		} else {//check pass
			g_count++;
#ifdef SENINF_UT
			seninf_ut_outmux = i;
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "seninf_ut_outmux:");
			CALL_FROM_OPS(putx64, seninf_ut_outmux);
#endif
			has_secured_path = true;
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "dt match");
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "g_count:");
			CALL_FROM_OPS(putx64, g_count);
#ifndef DAPC_EN
			g_preg_va = preg;
			ret = (SENINF_RETURN)seninf_lock_exclude_enable(
				preg, seninf, (SENINF_OUTMUX_ENUM)i);
#endif
		}
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "i:");
		CALL_FROM_OPS(putx64, i);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "secure:");
		CALL_FROM_OPS(putx64, secure);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "seninf_outmux:");
		CALL_FROM_OPS(putx64, (SENINF_OUTMUX_ENUM)i);
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "ret:");
		CALL_FROM_OPS(putx64, ret);
	}
#ifndef SENINF_UT
	pfixmap = CALL_FROM_OPS(fixmap_map, pa);
	CALL_FROM_OPS(flush_dcache_to_poc, pfixmap, sizeof(SecMgr_CamInfo));

	sensor_check = (SecMgr_CamInfo *)pfixmap;
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "P1 secure status/tg");
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "Sec_status:");
	CALL_FROM_OPS(putx64, sensor_check->Sec_status);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "SecTG:");
	CALL_FROM_OPS(putx64, sensor_check->SecTG);
	target_outmux = sensor_check->SecTG + SENINF_AUTH_CAM_MIN;
	CALL_FROM_OPS(fixmap_unmap);
#else
	target_outmux = seninf_ut_outmux;
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "target_outmux:");
	CALL_FROM_OPS(putx64, target_outmux);
#endif

	ret = SENINF_RETURN_ERROR;

	if ((g_count == 2) && ((target_outmux >= SENINF_AUTH_CAM_MIN) &&
		(target_outmux <= SENINF_AUTH_CAM_MAX))) {
		ret = SENINF_RETURN_SUCCESS;
	}

	if ((g_count == 1) && ((target_outmux > SENINF_OUTMUX0) &&
		(target_outmux <= SENINF_OUTMUX_NUM))) {
		ret = SENINF_RETURN_SUCCESS;
	}

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "g_count:");
	CALL_FROM_OPS(putx64, g_count);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "target_outmux:");
	CALL_FROM_OPS(putx64, target_outmux);
	ret = (ret == SENINF_RETURN_SUCCESS && has_secured_path) ?
		ret : SENINF_RETURN_ERROR;
#ifndef SENINF_UT
	ApiISPGetSecureState(pa, sensor_check, ret);
#else
	if (chunk_hsfhandle != sensor_check_value) {
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "chunk_hsfhandle error!");
		ret = SENINF_RETURN_ERROR;
	}
#endif
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "exit");
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "has_secured_path:");
	CALL_FROM_OPS(putx64, has_secured_path);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "ret:");
	CALL_FROM_OPS(putx64, ret);

	return ret;
}
