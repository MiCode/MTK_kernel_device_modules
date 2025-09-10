// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <asm/kvm_pkvm_module.h>
#include "include/hypmmu.h"

void gpu_mpu_hyp_init(struct user_pt_regs *regs)
{
	register_gpumpu_pmm_hal(regs);

	regs->regs[0] = SMCCC_RET_SUCCESS;
	regs->regs[1] = 0;
}
