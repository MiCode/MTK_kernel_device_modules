// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include "smmu_mgmt.h"
#include "hyp_spinlock.h"

#include <asm/kvm_pkvm_module.h>
//#include <nvhe/spinlock.h>

#include "../../pkvm_mgmt/hyp/include/export.h"

#define DEBUG_HAL	0

extern void mtk_smmu_sync(void);
extern const struct pkvm_module_ops *pkvm_smmu_ops;
extern hyp_spinlock_t smmu_all_vm_lock;

static int pmm_pre_init(void)
{
	return 0;
}

static int pmm_prepare(void)
{
#if (DEBUG_HAL == 1)
	pkvm_smmu_ops->puts("smmu: pmm_prepare");
#endif
	return 0;
}

static int pmm_secure(u64 paddr, u32 size, u8 pmm_attr)
{
	return 0;
}

static int pmm_unsecure(u64 paddr, u32 size, u8 pmm_attr)
{
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
	u32 size = PAGE_SIZE << order;
	u8 mode_rwx = MM_MODE_R | MM_MODE_W | MM_MODE_X;

#if (DEBUG_HAL == 1)
	pkvm_smmu_ops->puts("smmu: pmm_secure_v2 {");
	pkvm_smmu_ops->putx64(paddr);
	pkvm_smmu_ops->putx64((u64)order);
	pkvm_smmu_ops->putx64((u64)pmm_attr);
	pkvm_smmu_ops->puts("}");
#endif

	/*   isolate the page from Linux, and let it be accessable in protected VM
	 *    ___________________________
	 *   | Linux        | Protected  |
	 *   | VM 0 (unmap) | VM 1 (rwx) |
	 *   |___________________________|
	 */
	hyp_spin_lock(&smmu_all_vm_lock);
	smmu_lazy_free();
	smmu_vm_unmap(0, paddr, size);
	smmu_vm_map(1, paddr, size, mode_rwx);
	hyp_spin_unlock(&smmu_all_vm_lock);
	return 0;
}

static int pmm_unsecure_v2(u64 paddr, u8 order, u8 pmm_attr)
{
	u32 size = PAGE_SIZE << order;
	u8 vm0_default_mode = MM_MODE_R | MM_MODE_W | MM_MODE_X;
	u8 vm1_default_mode = MM_MODE_R;

#if (DEBUG_HAL == 1)
	pkvm_smmu_ops->puts("smmu: pmm_unsecure_v2 {");
	pkvm_smmu_ops->putx64(paddr);
	pkvm_smmu_ops->putx64((u64)order);
	pkvm_smmu_ops->putx64((u64)pmm_attr);
	pkvm_smmu_ops->puts("}");
#endif
	/* VM0,VM1 map to default mode */
	/*   retrive the page from protected VM back to linux VM
	 *    ___________________________
	 *   | Linux        | Protected  |
	 *   | VM 0 (rwx)   | VM 1 (ro)  |
	 *   |___________________________|
	 */
	hyp_spin_lock(&smmu_all_vm_lock);
	smmu_lazy_free();
	smmu_vm_map(0, paddr, size, vm0_default_mode);
	smmu_vm_map(1, paddr, size, vm1_default_mode);
	hyp_spin_unlock(&smmu_all_vm_lock);
	return 0;
}

static int pmm_sync(void)
{
#if (DEBUG_HAL == 1)
	pkvm_smmu_ops->puts("smmu: sync");
#endif
	mtk_smmu_sync();
	return 0;
}

static int pmm_defragment(void)
{
	pkvm_smmu_ops->puts("smmu: degragment");

	hyp_spin_lock(&smmu_all_vm_lock);
	smmu_vm_defragment(0);
	/* No need to defrag protect vm-1
	 * because protect vm map memory qranulity will be
	 * the biggest one each time. e.g. granule=2MB while map 2MB
	 */
	mtk_smmu_sync();
	hyp_spin_unlock(&smmu_all_vm_lock);

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

static const char *pmm_hal_name = "mtk-smmu";

int register_hyp_pmm_hal(void)
{
	pmm_ops.name = pmm_hal_name;

	pmm_pre_init();
	return hyp_pmm_hal_register(&pmm_ops);
}
