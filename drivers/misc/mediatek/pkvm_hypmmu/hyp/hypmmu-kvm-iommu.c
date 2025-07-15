// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <asm/kvm_mmu.h>
#include <asm/kvm_pkvm_module.h>
#include <kvm/iommu.h>
#include <linux/arm-smccc.h>
#include <pkvm_mgmt/iommu.h>

#include "include/hypmmu.h"
#include "include/mtk-iommu.h"

static bool snapshot_done;

static int _init(void)
{
	return 0;
}

static struct kvm_hyp_iommu *_id_to_iommu(pkvm_handle_t smmu_id)
{
	return 0;
}

int _alloc_domain(struct kvm_hyp_iommu_domain *domain, int type)
{
	return 0;
}

void _free_domain(struct kvm_hyp_iommu_domain *domain)
{
}

static int _attach_dev(struct kvm_hyp_iommu *iommu, struct kvm_hyp_iommu_domain *domain,
			    u32 endpoint_id, u32 pasid, u32 pasid_bits, unsigned long flags)
{
	return 0;
}

static int _detach_dev(struct kvm_hyp_iommu *iommu, struct kvm_hyp_iommu_domain *domain,
			    u32 sid, u32 pasid)
{
	return 0;
}

bool _dabt_handler(struct user_pt_regs *regs, u64 esr, u64 addr)
{
	return mtkiommu_dabt_handler(regs, esr, addr);
}

int _suspend(struct kvm_hyp_iommu *iommu)
{
	return 0;
}

int _resume(struct kvm_hyp_iommu *iommu)
{
	return 0;
}

static void _iotlb_sync(struct kvm_hyp_iommu_domain *domain,
			     struct iommu_iotlb_gather *gather)
{
}

static void _host_stage2_idmap(struct kvm_hyp_iommu_domain *domain,
				    phys_addr_t start, phys_addr_t end,
				    int prot)
{
}

static void _host_stage2_idmap_complete(bool map)
{
}

struct kvm_iommu_ops hypmmu_ops = {
	.init				= _init,
	.get_iommu_by_id		= _id_to_iommu,
	.alloc_domain			= _alloc_domain,
	.free_domain			= _free_domain,
	.attach_dev			= _attach_dev,
	.detach_dev			= _detach_dev,
	.dabt_handler			= _dabt_handler,
	.suspend			= _suspend,
	.resume				= _resume,
	.iotlb_sync			= _iotlb_sync,
	.host_stage2_idmap		= _host_stage2_idmap,
	.host_stage2_idmap_complete     = _host_stage2_idmap_complete,
};

void iommu_finalise(struct user_pt_regs *regs)
{
	int ret;

	ret = mod_ops->iommu_snapshot_host_stage2(NULL);
	regs->regs[0] = SMCCC_RET_SUCCESS;
	snapshot_done = true;
}
