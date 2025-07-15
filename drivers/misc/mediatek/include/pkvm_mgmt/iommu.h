/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __ARM64_KVM_NVHE_IOMMU_H__
#define __ARM64_KVM_NVHE_IOMMU_H__

#include <asm/kvm_host.h>
#include <asm/kvm_pgtable.h>

#include <kvm/iommu.h>

struct kvm_iommu_ops {
	int (*init)(void);
	int (*alloc_domain)(struct kvm_hyp_iommu_domain *domain, int type);
	void (*free_domain)(struct kvm_hyp_iommu_domain *domain);
	struct kvm_hyp_iommu *(*get_iommu_by_id)(pkvm_handle_t iommu_id);
	int (*attach_dev)(struct kvm_hyp_iommu *iommu, struct kvm_hyp_iommu_domain *domain,
			  u32 endpoint_id, u32 pasid, u32 pasid_bits, unsigned long flags);
	int (*detach_dev)(struct kvm_hyp_iommu *iommu, struct kvm_hyp_iommu_domain *domain,
			  u32 endpoint_id, u32 pasid);
	int (*map_pages)(struct kvm_hyp_iommu_domain *domain, unsigned long iova,
			 phys_addr_t paddr, size_t pgsize,
			 size_t pgcount, int prot, size_t *total_mapped);
	size_t (*unmap_pages)(struct kvm_hyp_iommu_domain *domain, unsigned long iova,
			      size_t pgsize, size_t pgcount,
			      struct iommu_iotlb_gather *gather);
	phys_addr_t (*iova_to_phys)(struct kvm_hyp_iommu_domain *domain, unsigned long iova);
	void (*iotlb_sync)(struct kvm_hyp_iommu_domain *domain,
			   struct iommu_iotlb_gather *gather);
	bool (*dabt_handler)(struct user_pt_regs *regs, u64 esr, u64 addr);
	void (*host_stage2_idmap)(struct kvm_hyp_iommu_domain *domain,
				  phys_addr_t start, phys_addr_t end, int prot);
	void (*host_stage2_idmap_complete)(bool map);
	int (*suspend)(struct kvm_hyp_iommu *iommu);
	int (*resume)(struct kvm_hyp_iommu *iommu);
	int (*dev_block_dma)(struct kvm_hyp_iommu *iommu, u32 endpoint_id,
			     bool is_host_to_guest);
	int (*get_iommu_token_by_id)(pkvm_handle_t smmu_id, u64 *out_token);

	ANDROID_KABI_RESERVE(1);
	ANDROID_KABI_RESERVE(2);
	ANDROID_KABI_RESERVE(3);
	ANDROID_KABI_RESERVE(4);
	ANDROID_KABI_RESERVE(5);
	ANDROID_KABI_RESERVE(6);
	ANDROID_KABI_RESERVE(7);
	ANDROID_KABI_RESERVE(8);
};

#endif /* __ARM64_KVM_NVHE_IOMMU_H__ */
