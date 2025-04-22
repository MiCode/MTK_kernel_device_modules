// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <asm/kvm_pkvm_module.h>
#include <linux/arm-smccc.h>
#include "include/hyp_pmm.h"

#define MAX_PMM_HALS 8
#define TRACE 0

#ifdef memset
#undef memset
#endif

extern const struct pkvm_module_ops *pkvm_ops;
static void *pmm_hals[MAX_PMM_HALS];

#undef cmpxchg64_release
#define cmpxchg64_release	__lse__cmpxchg_case_rel_64
//#define cmpxchg64_release	__ll_sc__cmpxchg_case_rel_64

#define DEBUG_HYP_PMM	0

/*
    PMM_MSG_ENTRY format
    page number = PA >> hyp_pmm_el1_page_size_shift()
     _______________________________________
    |  reserved  | page order | page number |
    |____________|____________|_____________|
    31         28 27        24 23          0
*/
#define PMM_MSG_ORDER_SHIFT (24UL)
#define PMM_MSG_PFN_MASK ((1UL << PMM_MSG_ORDER_SHIFT) - 1)
#define GET_PMM_ENTRY_ORDER(entry) ((entry >> PMM_MSG_ORDER_SHIFT) & 0xf)
#define GET_PMM_ENTRY_PFN(entry) ((u64)(entry & PMM_MSG_PFN_MASK))


#define for_each_pmm_hal(hal, i)				\
	for ((i) = 0, hal = (typeof(hal))__get_pmm_hal(0);	\
	     hal;						\
	     hal = (typeof(hal))__get_pmm_hal(++(i)))

static void *__get_pmm_hal(int i)
{
	if (i >= MAX_PMM_HALS)
		return NULL;

	i = array_index_nospec(i, MAX_PMM_HALS);

	return pmm_hals[i];
}

static int share_pmm_ipc(u64 pfn)
{
	int ret;
	void *va;

	ret = pkvm_ops->host_share_hyp(pfn);
	if (ret) {
		pkvm_ops->puts("share pmm_ipc failed");
		pkvm_ops->putx64(pfn);
		WARN_ON(ret != 0);
		return ret;
	}

	va = pkvm_ops->hyp_va(pfn << PAGE_SHIFT);
	ret = pkvm_ops->pin_shared_mem(va, va + PAGE_SIZE);
	if (ret) {
		pkvm_ops->puts("pin pmm_ipc failed");
		pkvm_ops->putx64(pfn);
		WARN_ON(ret != 0);
		return ret;
	}

	return 0;
}

static int unshare_pmm_ipc(u64 pfn)
{
	int ret;
	void *va = pkvm_ops->hyp_va(pfn << PAGE_SHIFT);

	pkvm_ops->unpin_shared_mem(va, va + PAGE_SIZE);

	ret = pkvm_ops->host_unshare_hyp(pfn);
	if (ret) {
		pkvm_ops->puts("unshare pmm_ipc failed");
		pkvm_ops->putx64(pfn);
		WARN_ON(ret != 0);
		return ret;
	}

	return 0;
}

static void pmm_poison_pages(u32 *ipc, u32 count)
{
	u32 entry, order, idx;
	u64 pa;
	void *va;
	u32 nr_pages;

	/* processing for each page from ipc */
	for (idx = 0; idx < count; idx++) {
		entry = ipc[idx];
		order = GET_PMM_ENTRY_ORDER(entry);
		pa = GET_PMM_ENTRY_PFN(entry) << PAGE_SHIFT;
		nr_pages = 1UL << order;

		while (nr_pages--) {
			va = pkvm_ops->fixmap_map(pa);
			pa += PAGE_SIZE;
			pkvm_ops->memset(va, 0, PAGE_SIZE);
			pkvm_ops->flush_dcache_to_poc(va, PAGE_SIZE);
			pkvm_ops->fixmap_unmap();
		}
	}
}

static int hal_secure_pages_v2(struct pmm_hal *hal,
			       u32 *ipc, u32 attr, u32 count, bool lock)
{
	u32 entry, order, idx;
	u64 pa;
	int ret = 0;

	/* secure_v2 and unsecure_v2 must be implemented */
	if (!hal->secure_v2 || !hal->unsecure_v2)
		return -EINVAL;

	if (hal->prepare)
		hal->prepare();

	/* processing for each page from ipc */
	for (idx = 0; idx < count; idx++) {
		entry = ipc[idx];
		order = GET_PMM_ENTRY_ORDER(entry);
		pa = GET_PMM_ENTRY_PFN(entry) << PAGE_SHIFT;
		if (lock)
			ret = hal->secure_v2(pa, order, attr);
		else
			ret = hal->unsecure_v2(pa, order, attr);

		if (ret) {
			pkvm_ops->puts("hal_secure_pages_v2 failed{");
			if (lock)
				pkvm_ops->puts("secure");
			else
				pkvm_ops->puts("unsecure");
			pkvm_ops->puts(hal->name);
			pkvm_ops->putx64(ret);
			pkvm_ops->putx64(pa);
			pkvm_ops->putx64((u64)order);
			pkvm_ops->putx64((u64)attr);
			pkvm_ops->puts("}");
			break;
		}
	}

	if (hal->sync)
		hal->sync();

	return ret;
}

static int secure_pages_v2(u64 ipc_pfn, u32 attr, u32 count, bool lock)
{
	struct pmm_hal *hal;
	void *ipc_va;
	int i, ret = 0;

#if (DEBUG_HYP_PMM == 1)
	if (lock == true)
		pkvm_ops->puts("HYP_PMM: secure_pages_v2{");
	else
		pkvm_ops->puts("HYP_PMM: unsecure_pages_v2{");
	pkvm_ops->putx64(ipc_pfn);
	pkvm_ops->putx64((u64)attr);
	pkvm_ops->putx64((u64)count);
	pkvm_ops->puts("}");
#endif
	if (!ipc_pfn)
		return -EINVAL;

	if (!count)
		return -EINVAL;

	ret = share_pmm_ipc(ipc_pfn);
	if (ret)
		goto out;
	ipc_va = pkvm_ops->hyp_va(ipc_pfn << PAGE_SHIFT);

	/* Poison(clean) pages before unlock */
	if (lock == false)
		pmm_poison_pages(ipc_va, count);

	for_each_pmm_hal(hal, i) {
		ret = hal_secure_pages_v2(hal, ipc_va, attr, count, lock);
		if (ret) {
			pkvm_ops->puts("hal_secure_pages_v2 failed");
			pkvm_ops->puts(hal->name);
			pkvm_ops->putx64(ret);
			WARN_ON(ret != 0);
			break;
		}
	}

	ret = unshare_pmm_ipc(ipc_pfn);
out:
	return ret;
}

void hyp_pmm_assign_buffer_v2(struct user_pt_regs *regs)
{
	u64 ipc_pfn = regs->regs[1];
	u32 attr = regs->regs[2];
	u32 count = regs->regs[3];
	int ret;

#if (DEBUG_HYP_PMM == 1)
	pkvm_ops->puts("HYP_PMM: assign buffer");
	pkvm_ops->putx64(ipc_pfn);
#endif
	ret = secure_pages_v2(ipc_pfn, attr, count, true);
	if (ret) {
		pkvm_ops->puts("hyp_pmm_assign_buffer_v2 failed");
		regs->regs[0] = ret;
		WARN_ON(ret != 0);
		return;
	}

	regs->regs[0] = SMCCC_RET_SUCCESS;
	regs->regs[1] = 0;
}

void hyp_pmm_unassign_buffer_v2(struct user_pt_regs *regs)
{
	u64 ipc_pfn = regs->regs[1];
	u32 attr = regs->regs[2];
	u32 count = regs->regs[3];
	int ret;

#if (DEBUG_HYP_PMM == 1)
	pkvm_ops->puts("HYP_PMM: unassign buffer");
	pkvm_ops->putx64(ipc_pfn);
#endif
	ret = secure_pages_v2(ipc_pfn, attr, count, false);
	if (ret) {
		pkvm_ops->puts("hyp_pmm_unassign_buffer_v2 failed");
		regs->regs[0] = ret;
		WARN_ON(ret != 0);
		return;
	}

	regs->regs[0] = SMCCC_RET_SUCCESS;
	regs->regs[1] = 0;
}

void hyp_pmm_defragment(struct user_pt_regs *regs)
{
	struct pmm_hal *hal;
	int i;

#if (DEBUG_HYP_PMM == 1)
	pkvm_ops->puts("defragment{}");
#endif

	for_each_pmm_hal(hal, i) {
		if (hal->defragment)
			hal->defragment();
	}
	regs->regs[0] = SMCCC_RET_SUCCESS;
	regs->regs[1] = 0;
}

static int hal_register(void *hal)
{
	int i;

	for (i = 0; i < MAX_PMM_HALS ; i++) {
		if (!cmpxchg64_release((void *)&pmm_hals[i], 0, (u64)hal)) {
			pmm_hals[i] = hal;
			return 0;
		}
	}
	pkvm_ops->puts("hal_register failed");
	WARN_ON(1);
	return -EBUSY;
}

int hyp_pmm_init(void)
{
	return 0;
}


int hyp_pmm_hal_register(struct pmm_hal *hal)
{
	hal_register(hal);
	pkvm_ops->puts("HYP_PMM: hal register");
	pkvm_ops->puts(hal->name);
	return 0;
}
