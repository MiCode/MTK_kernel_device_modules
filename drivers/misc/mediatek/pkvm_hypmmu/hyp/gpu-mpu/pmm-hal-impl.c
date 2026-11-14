// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <asm/kvm_pkvm_module.h>

#include <include/export.h>
#include "include/hypmmu.h"

#define DEBUG_HAL 0

static void pmm_pre_init(void)
{
	MOD_PUTS("gpu-mpu pre_init");
}

static int pmm_prepare(void)
{
#if (DEBUG_HAL)
	MOD_PUTS("gpu-mpu prepare");
#endif
	return 0;
}

static int pmm_secure(u64 paddr, u32 size, u8 pmm_attr)
{
	/* legacy API. no need to implement */
	return 0;
}

static int pmm_unsecure(u64 paddr, u32 size, u8 pmm_attr)
{
	/* legacy API. no need to implement */
	return 0;
}

static int pmm_secure_v2(u64 paddr, u8 order, u8 pmm_attr)
{
#if (DEBUG_HAL)
	MOD_PUTS3("gpu-mpu secure_v2", paddr, order, pmm_attr);
#endif
	return 0;
}

static int pmm_unsecure_v2(u64 paddr, u8 order, u8 pmm_attr)
{
#if (DEBUG_HAL)
	MOD_PUTS3("gpu-mpu unsecure_v2", paddr, order, pmm_attr);
#endif
	return 0;
}

static int pmm_sync(void)
{
#if (DEBUG_HAL)
	MOD_PUTS("gpu-mpu sync");
#endif
	return 0;
}

static int pmm_defragment(void)
{
	return 0;
}

static struct pmm_hal pmm_ops = {
	.prepare		= pmm_prepare,
	.secure			= pmm_secure,
	.unsecure		= pmm_unsecure,
	.secure_v2		= pmm_secure_v2,
	.unsecure_v2		= pmm_unsecure_v2,
	.sync			= pmm_sync,
	.defragment		= pmm_defragment,
};

static const char *pmm_hal_name = "gpu-mpu";

void register_gpumpu_pmm_hal(void)
{
	MOD_PUTS("register_gpumpu_pmm_hal");

	pmm_ops.name = pmm_hal_name;

	pmm_pre_init();

	hyp_pmm_hal_register(&pmm_ops);
}
