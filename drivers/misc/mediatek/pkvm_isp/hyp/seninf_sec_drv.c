// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include "pkvm_isp_hyp.h"
#include "seninf_ta.h"
#include "seninf_tee_reg.h"
#include "seninf_util.h"
#include "sys.h"
#include "trustzone.h"

SENINF_RETURN seninf_drv_sync_to_pa(void *args)
{
	SENINF_RETURN ret = SENINF_RETURN_SUCCESS;
	return ret;
}

SENINF_RETURN seninf_ta_drv_sync_to_va(void *args)
{
	uint32_t *preg_va = (uint32_t *)args;
	TZ_RESULT ret_err = TZ_RESULT_SUCCESS;

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "in sync to va");

	for (int i = 0;
		i < sizeof(SENINF_TEE_REG) / sizeof(uint32_t); i++ , preg_va++) {
		ret_err = SECIO_READ(
			gseninf_drv_reg_base[i], gseninf_drv_reg_addr[i], preg_va);
		if (ret_err != TZ_RESULT_SUCCESS) {
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "SECIO_READ failed");
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret_err:");
			CALL_FROM_OPS(putx64, ret_err);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "io_type:");
			CALL_FROM_OPS(putx64, gseninf_drv_reg_base[i]);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "offset:");
			CALL_FROM_OPS(putx64, gseninf_drv_reg_addr[i]);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "i:");
			CALL_FROM_OPS(putx64, i);
			break;
		}
	}

	return ret_err == TZ_RESULT_SUCCESS ?
		SENINF_RETURN_SUCCESS :SENINF_RETURN_ERROR;
}

SENINF_RETURN seninf_ta_drv_checkpipe(SENINF_RETURN auth_reuslt)
{
	TZ_RESULT ret_err = TZ_RESULT_SUCCESS;
	int value = 0;
	int i = 0;

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "enter");

	if (auth_reuslt != SENINF_RETURN_SUCCESS) {
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "check pipe fail,lock seninf mux!");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "auth_reuslt:");
		CALL_FROM_OPS(putx64, auth_reuslt);

		for (i = 0; i < sizeof(SENINF_TEE_REG)/sizeof(uint32_t); i++) {
			value = 0;
			ret_err |= SECIO_READ(gseninf_drv_reg_base[i], gseninf_drv_reg_addr[i], &value);
			if (ret_err != TZ_RESULT_SUCCESS) {
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "SECIO_READ failed");
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "ret_err:");
				CALL_FROM_OPS(putx64, ret_err);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "io_type:");
				CALL_FROM_OPS(putx64, gseninf_drv_reg_base[i]);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "offset:");
				CALL_FROM_OPS(putx64, gseninf_drv_reg_addr[i]);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "i:");
				CALL_FROM_OPS(putx64, i);
				return SENINF_RETURN_ERROR;
			} else {
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "start disable, read done");
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "i:");
				CALL_FROM_OPS(putx64, i);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "ret_err:");
				CALL_FROM_OPS(putx64, ret_err);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "value:");
				CALL_FROM_OPS(putx64, value);
			}
			ret_err |= SECIO_WRITE(gseninf_drv_reg_base[i], gseninf_drv_reg_addr[i], 0);
			if (ret_err != TZ_RESULT_SUCCESS) {
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "SECIO_WRITE failed");
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "ret_err:");
				CALL_FROM_OPS(putx64, ret_err);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "io_type:");
				CALL_FROM_OPS(putx64, gseninf_drv_reg_base[i]);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "offset:");
				CALL_FROM_OPS(putx64, gseninf_drv_reg_addr[i]);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "i:");
				CALL_FROM_OPS(putx64, i);
				return SENINF_RETURN_ERROR;
			} else {
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "disable done");
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "i:");
				CALL_FROM_OPS(putx64, i);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "ret_err:");
				CALL_FROM_OPS(putx64, ret_err);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "value:");
				CALL_FROM_OPS(putx64, value);
			}
		}
	} else {
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "check pipe pass");
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "auth_reuslt:");
		CALL_FROM_OPS(putx64, auth_reuslt);

		for (i = 0; i < sizeof(SENINF_TEE_REG)/sizeof(uint32_t); i++) {
			value = 0;
			ret_err |= SECIO_READ(gseninf_drv_reg_base[i], gseninf_drv_reg_addr[i], &value);
			if (ret_err != TZ_RESULT_SUCCESS) {
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "SECIO_READ failed");
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "ret_err:");
				CALL_FROM_OPS(putx64, ret_err);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "io_type:");
				CALL_FROM_OPS(putx64, gseninf_drv_reg_base[i]);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "offset:");
				CALL_FROM_OPS(putx64, gseninf_drv_reg_addr[i]);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "i:");
				CALL_FROM_OPS(putx64, i);
				return SENINF_RETURN_ERROR;
			} else {
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "read done");
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "i:");
				CALL_FROM_OPS(putx64, i);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "ret_err:");
				CALL_FROM_OPS(putx64, ret_err);
				CALL_FROM_OPS(puts, __func__);
				CALL_FROM_OPS(puts, PFX "value:");
				CALL_FROM_OPS(putx64, value);
			}
		}
	}
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "exit");
	return ret_err;
}

SENINF_RETURN seninf_ta_drv_free(void *args)
{
	TZ_RESULT ret_err = TZ_RESULT_SUCCESS;
	uint32_t value = 0;
	int i = 0;

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "enter");

	for (i = 0; i < sizeof(SENINF_TEE_REG)/sizeof(uint32_t); i++) {
		value = 0;
		ret_err |= SECIO_READ(gseninf_drv_reg_base[i], gseninf_drv_reg_addr[i], &value);
		if (ret_err != TZ_RESULT_SUCCESS) {
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "SECIO_READ failed");
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret_err:");
			CALL_FROM_OPS(putx64, ret_err);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "io_type:");
			CALL_FROM_OPS(putx64, gseninf_drv_reg_base[i]);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "offset:");
			CALL_FROM_OPS(putx64, gseninf_drv_reg_addr[i]);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "i:");
			CALL_FROM_OPS(putx64, i);
			return SENINF_RETURN_ERROR;
		} else {
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "read done");
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "i:");
			CALL_FROM_OPS(putx64, i);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "ret_err:");
			CALL_FROM_OPS(putx64, ret_err);
			CALL_FROM_OPS(puts, __func__);
			CALL_FROM_OPS(puts, PFX "value:");
			CALL_FROM_OPS(putx64, value);
		}
	}

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "exit");
	return ret_err;
}
