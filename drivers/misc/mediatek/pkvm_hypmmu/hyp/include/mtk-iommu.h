/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __ARM64_KVM_NVHE_MTK_IOMMU_H__
#define __ARM64_KVM_NVHE_MTK_IOMMU_H__

#include <asm/kvm_pkvm_module.h>

#define DEBUG_IOVA_MAGIC_NUMBER 0xBBFD
#define IOMMU_DRIVER_MEM_PFN_MAX (100U)

extern u64 page_pool_base;
extern u64 page_pool_size;
extern bool is_iommu_pgtbl_page_memory(void);

struct iommu_info {
	u32 ent_sz; /* size of tag entry (including tag info) */
	u32 cnt;    /* total count of tags */
	u32 idx;    /* current tag index */
	char *tags; /* container of tags, size = ent_sz * cnt */
};

struct iova_info {
	u32 cur_sec;
	u32 cur_nsec;
	u64 iova_start;
	u64 iova_end;
	bool iova_map;
};

struct fmpt {
	u64 *smpt;
	u64 mem_order;
};

struct mpt {
	/* Memory used by IOMMU driver */
	struct fmpt fmpt[IOMMU_DRIVER_MEM_PFN_MAX];
	u32 mem_block_num;
};

bool mtkiommu_dabt_handler(struct user_pt_regs *regs, u64 esr, u64 addr);
bool mtk_iommu_smc_handler(struct user_pt_regs *regs);
int io_pgtable_handler(u64 iova_start, u64 iova_end, u64 tid, u32 sec, u32 nsec);
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
struct iova_info *query_iova_debug_info(u64 iova, bool iova_map);
void save_iova_debug_info(u32 sec, u32 nsec, u64 iova_start, u64 iova_end, bool iova_map);

#endif
