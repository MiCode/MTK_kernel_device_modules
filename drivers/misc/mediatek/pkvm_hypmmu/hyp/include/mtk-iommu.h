/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __ARM64_KVM_NVHE_MTK_IOMMU_H__
#define __ARM64_KVM_NVHE_MTK_IOMMU_H__

#include <asm/kvm_pkvm_module.h>

extern u64 page_pool_base;
extern u64 page_pool_size;

bool mtkiommu_dabt_handler(struct user_pt_regs *regs, u64 esr, u64 addr);
bool mtk_iommu_smc_handler(struct user_pt_regs *regs);
int io_pgtable_handler(u64 iova_start, u64 iova_end, u64 tid);
u64 get_page_pool_base(void);
u64 get_page_pool_size(void);
void create_v7s_pages(u64 rmem_pa, u64 rmem_size);
struct mtk_iommu_device *mtk_iommu_lookup_dev(u64 addr);
bool is_enable_jpeg_prot_2(void);
void set_guest_pgd(u64 pa, u32 table_id);
u64 query_guest_pgd(u32 table_id);
u64 query_hw_pgd(u32 table_id);
u64 query_prot_pgd(u32 table_id);
void add_share_region(u64 iova, u64 size, u64 table_id);
u32 debug_io_get_pte(u32 *pgd, u64 iova, u32 *out_pte, u64 table_id);
u32 debug_make_result(u64 iova, u32 pte, u32 lvl);
u32 query_ac_srinfo(u64 pa);
void set_ac_attr(u64 pa, u64 size, u8 attr);

#endif
