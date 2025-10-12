// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */
#include <asm/kvm_pkvm_module.h>
#include <linux/arm-smccc.h>
#include "pkvm_isp_hyp.h"
#include "isp_sec_entry.h"

const struct pkvm_module_ops *pkvm_isp_ops;

void isp_hyp_sethsfcam(struct user_pt_regs *regs)
{
	ISP_RETURN ret = 0;

	CALL_FROM_OPS(puts, PFX "++");
	CALL_FROM_OPS(puts, __func__);

	regs->regs[0] = SMCCC_RET_SUCCESS;

	ret = isp_config_sethsfcam(regs);

	CALL_FROM_OPS(puts, "ret:");
	CALL_FROM_OPS(putx64, ret);

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "--");
}
void isp_hyp_sethsfcamsv(struct user_pt_regs *regs)
{
	ISP_RETURN ret = 0;

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "++");

	regs->regs[0] = SMCCC_RET_SUCCESS;

	ret = isp_config_sethsfcamsv(regs);

	CALL_FROM_OPS(puts, PFX "ret:");
	CALL_FROM_OPS(putx64, ret);

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "--");
}
void isp_hyp_streamon(struct user_pt_regs *regs)
{
	ISP_RETURN ret = 0;

	CALL_FROM_OPS(puts, __func__);
	CALL_FROM_OPS(puts, PFX "++");

	regs->regs[0] = SMCCC_RET_SUCCESS;
	ret = isp_stream_ctrl(regs);
	CALL_FROM_OPS(puts, PFX "ret:");

	CALL_FROM_OPS(putx64, ret);
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
