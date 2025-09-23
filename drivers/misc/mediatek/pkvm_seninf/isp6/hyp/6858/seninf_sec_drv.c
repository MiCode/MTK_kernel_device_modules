// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <mod_debug.h>
#include <pkvm_sys.h>
#include <pkvm_trustzone.h>
#include <seninf_ta.h>

#include "pkvm_seninf_hyp.h"
#include "seninf_tee_reg.h"

#define CAM_MUX_ENABLE_INDEX 6

static const uint32_t gseninf_drv_reg_addr[] = {
	SENINF_TEE_REG_ADDR_SENINF_TOP_MUX_CTRL_0,
	SENINF_TEE_REG_ADDR_SENINF_TOP_MUX_CTRL_1,
	SENINF_TEE_REG_ADDR_SENINF_CAM_MUX_CTRL_0,
	SENINF_TEE_REG_ADDR_SENINF_CAM_MUX_CTRL_1,
	SENINF_TEE_REG_ADDR_SENINF_CAM_MUX_CTRL_2,
	SENINF_TEE_REG_ADDR_SENINF_CAM_MUX_CTRL_3,
	SENINF_TEE_REG_ADDR_SENINF_CAM_MUX_EN,
	SENINF_TEE_REG_ADDR_SENINF1_MUX_CTRL_0,
	SENINF_TEE_REG_ADDR_SENINF1_MUX_CTRL_1,
	SENINF_TEE_REG_ADDR_SENINF1_MUX_OPT,
	SENINF_TEE_REG_ADDR_SENINF2_MUX_CTRL_0,
	SENINF_TEE_REG_ADDR_SENINF2_MUX_CTRL_1,
	SENINF_TEE_REG_ADDR_SENINF2_MUX_OPT,
	SENINF_TEE_REG_ADDR_SENINF3_MUX_CTRL_0,
	SENINF_TEE_REG_ADDR_SENINF3_MUX_CTRL_1,
	SENINF_TEE_REG_ADDR_SENINF3_MUX_OPT,
	SENINF_TEE_REG_ADDR_SENINF4_MUX_CTRL_0,
	SENINF_TEE_REG_ADDR_SENINF4_MUX_CTRL_1,
	SENINF_TEE_REG_ADDR_SENINF4_MUX_OPT,
	SENINF_TEE_REG_ADDR_SENINF5_MUX_CTRL_0,
	SENINF_TEE_REG_ADDR_SENINF5_MUX_CTRL_1,
	SENINF_TEE_REG_ADDR_SENINF5_MUX_OPT,
	SENINF_TEE_REG_ADDR_SENINF6_MUX_CTRL_0,
	SENINF_TEE_REG_ADDR_SENINF6_MUX_CTRL_1,
	SENINF_TEE_REG_ADDR_SENINF6_MUX_OPT,
	SENINF_TEE_REG_ADDR_SENINF7_MUX_CTRL_0,
	SENINF_TEE_REG_ADDR_SENINF7_MUX_CTRL_1,
	SENINF_TEE_REG_ADDR_SENINF7_MUX_OPT,
	SENINF_TEE_REG_ADDR_SENINF8_MUX_CTRL_0,
	SENINF_TEE_REG_ADDR_SENINF8_MUX_CTRL_1,
	SENINF_TEE_REG_ADDR_SENINF8_MUX_OPT
};

#ifndef SECIO_SENINF_TOP
#define SECIO_SENINF_TOP SECIO_SENINF_MUX1
#define SECIO_SENINF_CAM_MUX SECIO_SENINF_MUX1

#endif
static const uint32_t gseninf_drv_reg_base[] = {
	SECIO_SENINF_TOP,
	SECIO_SENINF_TOP,
	SECIO_SENINF_CAM_MUX,
	SECIO_SENINF_CAM_MUX,
	SECIO_SENINF_CAM_MUX,
	SECIO_SENINF_CAM_MUX,
	SECIO_SENINF_CAM_MUX,
	SECIO_SENINF_MUX1,
	SECIO_SENINF_MUX1,
	SECIO_SENINF_MUX1,
	SECIO_SENINF_MUX2,
	SECIO_SENINF_MUX2,
	SECIO_SENINF_MUX2,
	SECIO_SENINF_MUX3,
	SECIO_SENINF_MUX3,
	SECIO_SENINF_MUX3,
	SECIO_SENINF_MUX4,
	SECIO_SENINF_MUX4,
	SECIO_SENINF_MUX4,
	SECIO_SENINF_MUX5,
	SECIO_SENINF_MUX5,
	SECIO_SENINF_MUX5,
	SECIO_SENINF_MUX6,
	SECIO_SENINF_MUX6,
	SECIO_SENINF_MUX6,
	SECIO_SENINF_MUX7,
	SECIO_SENINF_MUX7,
	SECIO_SENINF_MUX7,
	SECIO_SENINF_MUX8,
	SECIO_SENINF_MUX8,
	SECIO_SENINF_MUX8,
};

static const uint32_t gseninf_drv_reg_base_addr[] = {
	0x1a004000,
	0x1a004000,
	0x1a004000,
	0x1a004000,
	0x1a004000,
	0x1a004000,
	0x1a004000,

	0x1a004000,
	0x1a004000,
	0x1a004000,
	0x1a005000,
	0x1a005000,
	0x1a005000,
	0x1a006000,
	0x1a006000,
	0x1a006000,
	0x1a007000,
	0x1a007000,
	0x1a007000,
	0x1a008000,
	0x1a008000,
	0x1a008000,
	0x1a009000,
	0x1a009000,
	0x1a009000,
	0x1a00a000,
	0x1a00a000,
	0x1a00a000,
	0x1a00b000,
	0x1a00b000,
	0x1a00b000,
};


SENINF_RETURN seninf_drv_sync_to_pa(void *args)
{
	SENINF_RETURN ret = SENINF_RETURN_SUCCESS;
	return ret;
}

SENINF_RETURN seninf_ta_drv_sync_to_va(void *args)
{
	uint32_t *preg_va = (uint32_t *)args;
	TZ_RESULT ret_err = TZ_RESULT_SUCCESS;

	for (int i = 0; i < sizeof(SENINF_TEE_REG) / sizeof(uint32_t); i++ ,preg_va++) {
		ret_err |= SECIO_READ(gseninf_drv_reg_base[i], gseninf_drv_reg_addr[i], preg_va);
		if (ret_err != TZ_RESULT_SUCCESS) {
			MOD_PUTS(__func__);
			MOD_PUTS4(PFX "SECIO_READ failed. {ret_err, io_type, offset, i} =",
				ret_err, gseninf_drv_reg_base[i], gseninf_drv_reg_addr[i], i);
			break;
		}
	}
	return ret_err == TZ_RESULT_SUCCESS ?SENINF_RETURN_SUCCESS :SENINF_RETURN_ERROR;
}


SENINF_RETURN seninf_ta_drv_checkpipe(SENINF_RETURN auth_reuslt)
{
	uint32_t value = 0;
	TZ_RESULT ret_err = TZ_RESULT_SUCCESS;

	MOD_PUTS(__func__);
	MOD_PUTS(PFX "enter");

	if(auth_reuslt != SENINF_RETURN_SUCCESS) {
		MOD_PUTS(__func__);
		MOD_PUTS1(PFX "check pipe fail,lock seninf mux! {auth_reuslt} =", auth_reuslt);

		/* disable seninf_mux_en*/
		/* seninf_mux_en X : disable all seninf_mux_en*/
		/* FIELD  SENINF_MUX_EN  :  1; 31..31, 0x80000000 */

		ret_err |= SECIO_READ(gseninf_drv_reg_base[CAM_MUX_ENABLE_INDEX],
			gseninf_drv_reg_addr[CAM_MUX_ENABLE_INDEX], &value);
		if (ret_err != TZ_RESULT_SUCCESS) {
			MOD_PUTS(__func__);
			MOD_PUTS4(PFX "SECIO_READ failed. {ret_err, io_type, offset, j} =",
				ret_err, gseninf_drv_reg_base[CAM_MUX_ENABLE_INDEX],
				gseninf_drv_reg_addr[CAM_MUX_ENABLE_INDEX], CAM_MUX_ENABLE_INDEX);
			return SENINF_RETURN_ERROR;
		}
		ret_err |= SECIO_WRITE(gseninf_drv_reg_base[CAM_MUX_ENABLE_INDEX],
			gseninf_drv_reg_addr[CAM_MUX_ENABLE_INDEX], value & (0xfffffE00));
		if (ret_err != TZ_RESULT_SUCCESS) {
			MOD_PUTS(__func__);
			MOD_PUTS5(PFX "SECIO_WRITE failed. {ret_err, io_type, offset, j, value} =",
				ret_err, gseninf_drv_reg_base[CAM_MUX_ENABLE_INDEX],
				gseninf_drv_reg_addr[CAM_MUX_ENABLE_INDEX], CAM_MUX_ENABLE_INDEX, value);
			return SENINF_RETURN_ERROR;
		}
		MOD_PUTS(__func__);
		MOD_PUTS2(PFX "{reg addr, value} =",
			gseninf_drv_reg_base_addr[CAM_MUX_ENABLE_INDEX] +
			gseninf_drv_reg_addr[CAM_MUX_ENABLE_INDEX],
			value);
	} else {
		/* seninf_mux_en X : enable all seninf_mux_en*/
		MOD_PUTS(__func__);
		MOD_PUTS1(PFX "check pipe pass. {auth_reuslt} =", auth_reuslt);

		ret_err |= SECIO_READ(gseninf_drv_reg_base[CAM_MUX_ENABLE_INDEX],
			gseninf_drv_reg_addr[CAM_MUX_ENABLE_INDEX], &value);
		if (ret_err != TZ_RESULT_SUCCESS) {
			MOD_PUTS(__func__);
			MOD_PUTS4(PFX "SECIO_READ failed. {ret_err, io_type, offset, j} =",
				ret_err, gseninf_drv_reg_base[CAM_MUX_ENABLE_INDEX],
				gseninf_drv_reg_addr[CAM_MUX_ENABLE_INDEX], CAM_MUX_ENABLE_INDEX);
			return SENINF_RETURN_ERROR;
		}
		ret_err |= SECIO_WRITE(gseninf_drv_reg_base[CAM_MUX_ENABLE_INDEX],
			gseninf_drv_reg_addr[CAM_MUX_ENABLE_INDEX], value | 0x1FFF);
		if (ret_err != TZ_RESULT_SUCCESS) {
			MOD_PUTS(__func__);
			MOD_PUTS5(PFX "SECIO_WRITE failed. {ret_err, io_type, offset, j, value} =",
				ret_err, gseninf_drv_reg_base[CAM_MUX_ENABLE_INDEX],
				gseninf_drv_reg_addr[CAM_MUX_ENABLE_INDEX], CAM_MUX_ENABLE_INDEX, value);
			return SENINF_RETURN_ERROR;
		}
		MOD_PUTS(__func__);
		MOD_PUTS2(PFX "{reg addr, value} =",
			gseninf_drv_reg_base_addr[CAM_MUX_ENABLE_INDEX] +
			gseninf_drv_reg_addr[CAM_MUX_ENABLE_INDEX],
			value);
	}
	MOD_PUTS(__func__);
	MOD_PUTS(PFX "exit");
	return SENINF_RETURN_SUCCESS;
}



SENINF_RETURN seninf_ta_drv_free(void *args)
{
	uint32_t value = 0;
	TZ_RESULT ret_err = TZ_RESULT_SUCCESS;

	MOD_PUTS(__func__);
	MOD_PUTS(PFX "enter");

	ret_err |= SECIO_READ(gseninf_drv_reg_base[CAM_MUX_ENABLE_INDEX],
		gseninf_drv_reg_addr[CAM_MUX_ENABLE_INDEX], &value);
	if (ret_err != TZ_RESULT_SUCCESS) {
		MOD_PUTS(__func__);
		MOD_PUTS4(PFX "SECIO_READ failed. {ret_err, io_type, offset, j} =",
			ret_err, gseninf_drv_reg_base[CAM_MUX_ENABLE_INDEX],
			gseninf_drv_reg_addr[CAM_MUX_ENABLE_INDEX], CAM_MUX_ENABLE_INDEX);
		return SENINF_RETURN_ERROR;
	}
	ret_err |= SECIO_WRITE(gseninf_drv_reg_base[CAM_MUX_ENABLE_INDEX],
		gseninf_drv_reg_addr[CAM_MUX_ENABLE_INDEX], value & (0xfffffE00));
	if (ret_err != TZ_RESULT_SUCCESS) {
		MOD_PUTS(__func__);
		MOD_PUTS5(PFX "SECIO_WRITE failed. {ret_err, io_type, offset, j, value} =",
			ret_err, gseninf_drv_reg_base[CAM_MUX_ENABLE_INDEX],
			gseninf_drv_reg_addr[CAM_MUX_ENABLE_INDEX], CAM_MUX_ENABLE_INDEX, value);
		return SENINF_RETURN_ERROR;
	}
	MOD_PUTS2(PFX "{reg addr, value} =",
		gseninf_drv_reg_base_addr[CAM_MUX_ENABLE_INDEX] +
		gseninf_drv_reg_addr[CAM_MUX_ENABLE_INDEX],
		value);

	MOD_PUTS(__func__);
	MOD_PUTS(PFX "exit");
	return SENINF_RETURN_SUCCESS;
}
