// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <mod_debug.h>
#include <asm/kvm_pkvm_module.h>
#include <linux/arm-smccc.h>

#include "pkvm_seninf_hyp.h"
#include "seninf_auth.h"
#include "seninf_dapc.h"
#include "seninf_sec_drv.h"
#include "seninf_ta.h"
#include "seninf_tee_reg.h"
#include "sensor_cfg_sec.h"

#ifdef memset
#undef memset
#endif

const struct pkvm_module_ops *pkvm_seninf_ops;

static SENINF_RETURN seninf_checkpipe(void);
static SENINF_RETURN seninf_free(void);

void *memset(void *dst, int c, size_t count)
{
	return CALL_FROM_OPS(memset, dst, c, count);
}

static SENINF_RETURN seninf_sync_to_va(void *preg)
{
	MOD_PUTS(__func__);
	MOD_PUTS(PFX "++");

	SENINF_RETURN ret = SENINF_RETURN_SUCCESS;

	ret = seninf_ta_drv_sync_to_va(preg);

	MOD_PUTS(__func__);
	MOD_PUTS1(PFX "{ret} =", ret);
	MOD_PUTS(__func__);
	MOD_PUTS(PFX "--");

	return ret;
}

static SENINF_RETURN seninf_checkpipe(void)
{
	MOD_PUTS(__func__);
	MOD_PUTS(PFX "++");

	SENINF_TEE_REG preg_va = {0};
	SENINF_RETURN ret = seninf_sync_to_va(&preg_va);

	if(ret != SENINF_RETURN_SUCCESS) {
		MOD_PUTS(__func__);
		MOD_PUTS1(PFX "seninf_sync_to_va error! {status} =", ret);
		return SENINF_RETURN_ERROR;
	}

	/*check authority by va*/
	ret = seninf_auth(&preg_va);

	ret |= seninf_ta_drv_checkpipe(ret);

	MOD_PUTS(__func__);
	MOD_PUTS(PFX "--");

	return ret;
}

static SENINF_RETURN seninf_free(void)
{
	MOD_PUTS(__func__);
	MOD_PUTS(PFX "++, close secure seninf_mux first!");

	SENINF_RETURN ret = SENINF_RETURN_SUCCESS;

	ret |= seninf_ta_drv_free(NULL);

	/*unlock after seninf_mux_en = 0*/
	ret |= seninf_dapc_unlock();

	MOD_PUTS(__func__);
	MOD_PUTS1(PFX "-- , unlocked seninf devapc done, {ret} =", ret);

	return ret;
}

void seninf_hyp_checkpipe(struct user_pt_regs *regs)
{
	SENINF_RETURN ret = 0;

	MOD_PUTS(__func__);
	MOD_PUTS(PFX "++");

	regs->regs[0] = SMCCC_RET_SUCCESS;
	ret = seninf_checkpipe();

	MOD_PUTS(__func__);
	MOD_PUTS1(PFX "{ret} =", ret);

	MOD_PUTS(__func__);
	MOD_PUTS(PFX "--");
}

void seninf_hyp_free(struct user_pt_regs *regs)
{
	MOD_PUTS(__func__);
	MOD_PUTS(PFX "++");

	regs->regs[0] = SMCCC_RET_SUCCESS;
	seninf_free();

	MOD_PUTS(__func__);
	MOD_PUTS(PFX "--");
}

int seninf_hyp_init(const struct pkvm_module_ops *ops)
{
	pkvm_seninf_ops = ops;
	MOD_PUTS(__func__);
	MOD_PUTS(PFX "success");
	return 0;
}
