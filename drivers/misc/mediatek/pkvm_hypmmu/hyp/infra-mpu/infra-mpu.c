// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <asm/kvm_pkvm_module.h>
#include "include/hypmmu.h"
#include "include/infra-mpu.h"

struct infra_buf infra_shared_buf[CORE_NUM];

void infra_mpu_set_ipc_base(u64 pa, void *va)
{
	u64 pa_percpu;
	struct infra_buf *p;

	MOD_PUTS2("infra-mpu ipc pa va", pa, va);

	/* infra-mpu shared buffer for TFA */
	for (u32 i = 0; i < CORE_NUM; i++) {
		p = &infra_shared_buf[i];
		pa_percpu = pa + i * SZ_32K;

		p->buf_paddr = pa_percpu;
		p->buf_size = SZ_32K;
		p->buf_ptr = hyp_phys_to_virt(pa_percpu);
		p->counter = 0;
	}
}

void infra_mpu_hyp_init(struct user_pt_regs *regs)
{
	register_inframpu_pmm_hal();

	regs->regs[0] = SMCCC_RET_SUCCESS;
	regs->regs[1] = 0;
}
