// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <asm/alternative-macros.h>
#include <asm/kvm_pkvm_module.h>

#include "../../include/pkvm_mtk_smc_handler/pkvm_mtk_smc_handler.h"

// Here saves all el2 smc call ids for blocking any invlid el1 smc
// call to use el2 smc call ids
static const uint64_t el2_smc_id_list[] = {
};

bool mtk_smc_handler(struct user_pt_regs *ctxt)
{
	int i = 0;
	uint64_t func_id = ctxt->regs[0] & ~ARM_SMCCC_CALL_HINTS;

	for (i = 0; i < ARRAY_SIZE(el2_smc_id_list); i++) {
		if (func_id == el2_smc_id_list[i])
			return true;
	}
	return false;
}

int mtk_smc_handler_hyp_init(const struct pkvm_module_ops *ops)
{
	return ops->register_host_smc_handler(mtk_smc_handler);
}
