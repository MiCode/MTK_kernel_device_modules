/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#ifndef _ASM_ARM64_ACRN_H
#define _ASM_ARM64_ACRN_H

#include <linux/arm-smccc.h>
#include <linux/hypervisor/hvcall.h>

void acrn_setup_intr_handler(void (*handler)(void));
void acrn_remove_intr_handler(void);

static inline long acrn_hypercall0(unsigned long hcall_id)
{
	struct arm_smccc_res res;
	ulong r7 = SMC_HYP_SECURE_ID << 16;

	arm_smccc_smc(SMC_SC_NBL_VHM_REQ, hcall_id, 0, 0, 0, 0, 0, r7, &res);
	return res.a0;
}

static inline long acrn_hypercall1(unsigned long hcall_id, unsigned long param1)
{
	struct arm_smccc_res res;
	ulong r7 = SMC_HYP_SECURE_ID << 16;

	arm_smccc_smc(SMC_SC_NBL_VHM_REQ, hcall_id, param1, 0, 0, 0, 0, r7, &res);
	return res.a0;
}

static inline long acrn_hypercall2(unsigned long hcall_id, unsigned long param1,
				   unsigned long param2)
{
	struct arm_smccc_res res;
	ulong r7 = SMC_HYP_SECURE_ID << 16;

	arm_smccc_smc(SMC_SC_NBL_VHM_REQ, hcall_id, param1, param2, 0, 0, 0, r7,
			&res);
	return res.a0;
}

static inline long acrn_hypercall3(unsigned long hcall_id, unsigned long param1,
				   unsigned long param2, unsigned long param3)
{
	struct arm_smccc_res res;
	ulong r7 = SMC_HYP_SECURE_ID << 16;

	arm_smccc_smc(SMC_SC_NBL_VHM_REQ, hcall_id, param1, param2, param3, 0,
		      0, r7, &res);
	return res.a0;
}

static inline long acrn_hypercall4(unsigned long hcall_id, unsigned long param1,
				   unsigned long param2, unsigned long param3,
				   unsigned long param4)
{
	struct arm_smccc_res res;
	ulong r7 = SMC_HYP_SECURE_ID << 16;

	arm_smccc_smc(SMC_SC_NBL_VHM_REQ, hcall_id, param1, param2, param3,
		      param4, 0, r7, &res);
	return res.a0;
}

static inline long acrn_hypercall5(unsigned long hcall_id, unsigned long param1,
				   unsigned long param2, unsigned long param3,
				   unsigned long param4, unsigned long param5)
{
	struct arm_smccc_res res;
	ulong r7 = SMC_HYP_SECURE_ID << 16;

	arm_smccc_smc(SMC_SC_NBL_VHM_REQ, hcall_id, param1, param2, param3,
		      param4, param5, r7, &res);
	return res.a0;
}

static inline long acrn_fastcall5(unsigned long fc_id, unsigned long param1,
				   unsigned long param2, unsigned long param3,
				   unsigned long param4, unsigned long param5)
{
	struct arm_smccc_res res;
	unsigned long r7 = SMC_HYP_SECURE_ID << 16;

	arm_smccc_smc(SMC_FC_NBL_VHM_REQ, fc_id, param1, param2, param3,
		      param4, param5, r7, &res);
	return res.a0;
}

static inline long acrn_fastcall0(unsigned long fc_id)
{
	return acrn_fastcall5(fc_id, 0, 0, 0, 0, 0);
}

static inline long acrn_fastcall1(unsigned long fc_id, unsigned long param1)
{
	return acrn_fastcall5(fc_id, param1, 0, 0, 0, 0);
}

static inline long acrn_fastcall2(unsigned long fc_id, unsigned long param1,
				   unsigned long param2)
{
	return acrn_fastcall5(fc_id, param1, param2, 0, 0, 0);
}

#endif /* _ASM_ARM64_ACRN_H */
