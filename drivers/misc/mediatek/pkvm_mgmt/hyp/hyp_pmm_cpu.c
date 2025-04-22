// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <asm/kvm_pkvm_module.h>
#include "include/export.h"

#define DEBUG_HAL	0

extern const struct pkvm_module_ops *pkvm_ops;

static int pmm_pre_init(void)
{
	return 0;
}

static int pmm_prepare(void)
{
#if (DEBUG_HAL == 1)
	pkvm_ops->puts("cpu: pmm_prepare");
#endif
	return 0;
}

static int pmm_secure(u64 paddr, u32 size, u8 pmm_attr)
{
	/*
	 * Deprecated to pKVM
	 * Page base v1 API was supported for legacy chipset which without pKVM
	 */
	return 0;
}

static int pmm_unsecure(u64 paddr, u32 size, u8 pmm_attr)
{
	/*
	 * Deprecated to pKVM
	 * Page base v1 API was supported for legacy chipset which without pKVM
	 */
	return 0;
}

static int pmm_secure_range(u64 paddr, u32 size, u8 pmm_attr)
{
	return 0;
}

static int pmm_unsecure_range(u64 paddr, u32 size, u8 pmm_attr)
{
	return 0;
}

static int pmm_secure_v2(u64 paddr, u8 order, u8 pmm_attr)
{
	int ret;
	u64 pfn = paddr >> PAGE_SHIFT;
	u64 nr_pages = 1UL << order;
	enum kvm_pgtable_prot prot = 0;

#if (DEBUG_HAL == 1)
	pkvm_ops->puts("cpu: pmm_secure_v2 {");
	pkvm_ops->putx64(paddr);
	pkvm_ops->putx64((u64)order);
	pkvm_ops->putx64((u64)pmm_attr);
	pkvm_ops->putx64((u64)nr_pages);
	pkvm_ops->puts("}");
#endif

	ret = pkvm_ops->host_stage2_mod_prot(pfn, prot, nr_pages, false);
	if (ret) {
		pkvm_ops->puts("pmm_secure_v2 failed {");
		pkvm_ops->putx64((u64)ret);
		pkvm_ops->putx64(paddr);
		pkvm_ops->putx64((u64)order);
		pkvm_ops->putx64((u64)pmm_attr);
		pkvm_ops->puts("}");
		WARN_ON(ret != 0);
	}

	return 0;
}

static int pmm_unsecure_v2(u64 paddr, u8 order, u8 pmm_attr)
{
	int ret;
	u64 pfn = paddr >> PAGE_SHIFT;
	u64 nr_pages = 1UL << order;
	/* Refer from default_host_prot() */
	enum kvm_pgtable_prot prot = PKVM_HOST_MEM_PROT;

#if (DEBUG_HAL == 1)
	pkvm_ops->puts("cpu: pmm_unsecure_v2 {");
	pkvm_ops->putx64(paddr);
	pkvm_ops->putx64((u64)order);
	pkvm_ops->putx64((u64)pmm_attr);
	pkvm_ops->puts("}");
#endif
	ret = pkvm_ops->host_stage2_mod_prot(pfn, prot, nr_pages, false);
	if (ret) {
		pkvm_ops->puts("pmm_unsecure_v2 failed {");
		pkvm_ops->putx64((u64)ret);
		pkvm_ops->putx64(paddr);
		pkvm_ops->putx64((u64)order);
		pkvm_ops->putx64((u64)pmm_attr);
		pkvm_ops->puts("}");
		WARN_ON(ret != 0);
	}

	return 0;
}

static int pmm_sync(void)
{
#if (DEBUG_HAL == 1)
	pkvm_ops->puts("cpu: sync");
#endif
	return 0;
}

static int pmm_defragment(void)
{
	pkvm_ops->puts("cpu: degragment");
	return 0;
}

struct pmm_hal pmm_ops = {
	.prepare		= pmm_prepare,
	.secure			= pmm_secure,
	.unsecure		= pmm_unsecure,
	.secure_v2		= pmm_secure_v2,
	.unsecure_v2		= pmm_unsecure_v2,
	.secure_range		= pmm_secure_range,
	.unsecure_range		= pmm_unsecure_range,
	.sync			= pmm_sync,
	.defragment		= pmm_defragment,
};

static const char *pmm_hal_name = "cpu";

int register_cpu_hal(void)
{
	pmm_ops.name = pmm_hal_name;

	pmm_pre_init();
	return hyp_pmm_hal_register(&pmm_ops);
}
