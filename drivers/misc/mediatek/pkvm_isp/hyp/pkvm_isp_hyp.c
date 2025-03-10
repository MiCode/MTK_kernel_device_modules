// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <asm/kvm_pkvm_module.h>
#include <linux/arm-smccc.h>

#include "pkvm_isp_hyp.h"
#include "seninf_auth.h"
#include "seninf_sec_drv.h"
#include "seninf_ta.h"
#include "seninf_tee_reg.h"
#include "sensor_cfg_sec.h"

#ifdef memset
#undef memset
#endif

#define LOG_MORE 1

const struct pkvm_module_ops *pkvm_isp_ops;

static SENINF_RETURN seninf_checkpipe(uint64_t pa);
static SENINF_RETURN seninf_free(void);

void *memset(void *dst, int c, size_t count)
{
	return CALL_FROM_OPS(memset, dst, c, count);
}

static SENINF_RETURN seninf_sync_to_va(void *preg)
{
	SENINF_RETURN ret = SENINF_RETURN_SUCCESS;

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "++");
	ret = seninf_ta_drv_sync_to_va(preg);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "ret =");
	CALL_FROM_OPS(putx64, ret);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "--");

	return ret;
}

static SENINF_RETURN seninf_checkpipe(uint64_t pa)
{
	SENINF_TEE_REG preg_va = {0};
	SENINF_RETURN ret = seninf_sync_to_va(&preg_va);

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "++");

#if LOG_MORE
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "seninf_sync_to_va done, ret:");
	CALL_FROM_OPS(putx64, ret);
#endif
	if(ret != SENINF_RETURN_SUCCESS) {
		CALL_FROM_OPS(puts, __func__);
		CALL_FROM_OPS(puts, PFX "seninf_sync_to_va error! status =");
		CALL_FROM_OPS(putx64, ret);
		return SENINF_RETURN_ERROR;
	}

#if defined(IS_SINGLE_SEC_PORT_SUPPORT)
	/* Configure secure csi port: Don't modify this code */
	ret = sensor_cfg_single_sec_port(Single_Secure_csi_port_front);
#if LOG_MORE
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "sensor_cfg_single_sec_port:");
	CALL_FROM_OPS(putx64, Single_Secure_csi_port_front);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "ret:");
	CALL_FROM_OPS(putx64, ret);
#endif
#elif defined(IS_MULTI_SEC_PORT_SUPPORT)
	/* Configure secure csi port: Don't modify this code */
	ret = sensor_cfg_multi_sec_port_front(Multi_Secure_csi_port_front);
#if LOG_MORE
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "sensor_cfg_multi_sec_port_front");
	CALL_FROM_OPS(putx64, Multi_Secure_csi_port_front);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "ret:");
	CALL_FROM_OPS(putx64, ret);
#endif

	ret = sensor_cfg_multi_sec_port_rear(Multi_Secure_csi_port_rear);
#if LOG_MORE
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "sensor_cfg_multi_sec_port_rear");
	CALL_FROM_OPS(putx64, Multi_Secure_csi_port_rear);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "ret:");
	CALL_FROM_OPS(putx64, ret);
#endif
#endif

	/*check authority by va*/
	ret = seninf_auth(&preg_va, pa);
#if LOG_MORE
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "seninf_auth done, ret:");
	CALL_FROM_OPS(putx64, ret);
#endif
	ret |= seninf_ta_drv_checkpipe(ret);
#if LOG_MORE
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "seninf_ta_drv_checkpipe done, ret:");
	CALL_FROM_OPS(putx64, ret);
#endif
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "--");

	return ret;
}

static SENINF_RETURN seninf_free(void)
{
	SENINF_RETURN ret = SENINF_RETURN_SUCCESS;

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "++, close secure seninf_mux first!");

	ret |= seninf_ta_drv_free(NULL);

#ifdef DAPC_EN
	/*unlock after seninf_mux_en = 0*/
	ret |= seninf_dapc_unlock();
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "-- , unlocked seninf devapc done, ret:");
	CALL_FROM_OPS(putx64, ret);
#else
	ret |= seninf_lock_exclude_disable();
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "-- seninf_lock_exclude_disable done, ret:");
	CALL_FROM_OPS(putx64, ret);
#endif

	return ret;
}

void isp_hyp_checkpipe(struct user_pt_regs *regs)
{
	SENINF_RETURN ret = 0;
	uint64_t pa;

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "++");

	regs->regs[0] = SMCCC_RET_SUCCESS;
	pa = (regs->regs[2] << 32) | regs->regs[1];
	ret = seninf_checkpipe(pa);
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "ret:");
	CALL_FROM_OPS(putx64, ret);

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "--");
}

void isp_hyp_free(struct user_pt_regs *regs)
{
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "++");

	regs->regs[0] = SMCCC_RET_SUCCESS;
	seninf_free();

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "--");
}

int isp_hyp_init(const struct pkvm_module_ops *ops)
{
	pkvm_isp_ops = ops;
	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "success");
	return 0;
}
