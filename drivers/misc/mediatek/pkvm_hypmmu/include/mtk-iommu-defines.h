/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#ifndef __MTK_IOMMU_DEFINES_H__
#define __MTK_IOMMU_DEFINES_H__

#include <linux/bits.h>

#define DEBUG_IOMMU 0

#define MAX_TABLE_NUM 2
#define MAX_SHARE_REGION_NUM 16

#define MAX_PA_BITS (35)
#define MAX_PA_MASK ((1UL << MAX_PA_BITS) - 1)
#define MAX_IOMMU_DEVICES 8

#define ARM_V7S_PAGE_SHIFT (12UL)

/*
 * v2 ac table definition
 * max_entry = (1 << 35) / page_size
 * table size_bytes = (max_entry * 4-bits-attr) / 8
 */
#define AC_TABLE_MAX_ENTRIES ((1UL << MAX_PA_BITS) >> PAGE_SHIFT)
#define AC_TABLE_SIZE ((AC_TABLE_MAX_ENTRIES) >> 1)

#define NS_BANK 0
#define PROT_1_BANK 1
#define PROT_2_BANK 2
#define PROT_3_BANK 3
#define MAX_BANK 4

#define REG_MMU_BANK_OFFSET 0x1000

#define REG_MMU_PT_BASE_ADDR	0x000
#define REG_MMU_INVALIDATE	0x020
#define REG_MMU_INVLD_START_A	0x024
#define REG_MMU_INVLD_END_A	0x028
#define REG_MMU_INV_SEL_GEN2	0x02c
#define REG_MMU_CPE_DONE	0x12C

#define MMU_PT_BASE_ADDR_31_07_MASK	GENMASK(31, 7)
#define MMU_PT_BASE_ADDR_34_32_MASK	GENMASK(2, 0)
#define TTBR_LO_MASK			MMU_PT_BASE_ADDR_31_07_MASK
#define TTBR_HI_MASK			MMU_PT_BASE_ADDR_34_32_MASK

#define MMU_TLB_ADDR_31_12_MASK		GENMASK(31, 12)
#define MMU_TLB_ADDR_33_32_MASK		GENMASK(1, 0)
#define TLB_ADDR_LO_MASK		MMU_TLB_ADDR_31_12_MASK
#define TLB_ADDR_HI_MASK		MMU_TLB_ADDR_33_32_MASK

#define CONFIG_MTK_IOMMU_PGTABLE_EXT (34)
#define MTK_IOMMU_PGD_TABLE_SIZE \
	((1 << (CONFIG_MTK_IOMMU_PGTABLE_EXT - 20)) * sizeof(u32))

#define V7S_PAGE_TABLE_SIZE	(SZ_1K)
#define V7S_PAGE_TABLE_SHIFT	(10UL)
#define V7S_PAGE_SIZE		(SZ_4K)

#define ARM_V7S_CONT_PAGES		16
#define ARM_V7S_PTE_TYPE_TABLE		0x1
#define ARM_V7S_PTE_TYPE_PAGE		0x2
#define ARM_V7S_PTE_TYPE_CONT_PAGE	0x1

#define ARM_V7S_PTE_IS_VALID(pte)	(((pte) & 0x3) != 0)
#define ARM_V7S_PTE_IS_TABLE(pte, lvl) \
	((lvl) == 1 && (((pte) & 0x3) == ARM_V7S_PTE_TYPE_TABLE))

#define ARM_V7S_PTE_VALID_CMP(pte1, pte2)   (((pte1) & 0x3) != ((pte2) & 0x3))

#define ARM_V7S_TABLE_SHIFT		10
#define ARM_V7S_TABLE_MASK		((u32)(~0U << ARM_V7S_TABLE_SHIFT))

/* MediaTek extend the bits below for PA 32bit/33bit/34bit */
#define ARM_V7S_ATTR_MTK_PA_BIT32	BIT(9)
#define ARM_V7S_ATTR_MTK_PA_BIT33	BIT(4)
#define ARM_V7S_ATTR_MTK_PA_BIT34	BIT(5)

#define ARM_V7S_CONT_SECTION		BIT(18)

#define ARM_V7S_LVL_SHIFT(lvl)		((lvl) == 1 ? 20 : 12)
#define ARM_V7S_LVL_MASK(lvl)		((u32)(~0U << ARM_V7S_LVL_SHIFT(lvl)))

#define MTK_IOMMU_IAS	(34)
#define _ARM_V7S_LVL_BITS(lvl)	((lvl) == 1 ? (MTK_IOMMU_IAS - 20) : 8)
#define ARM_V7S_PTES_PER_LVL(lvl)	(1 << _ARM_V7S_LVL_BITS(lvl))
#define ARM_V7S_TABLE_SIZE(lvl)						\
	(ARM_V7S_PTES_PER_LVL(lvl) * sizeof(arm_v7s_iopte))

#define _ARM_V7S_IDX_MASK(lvl)	(ARM_V7S_PTES_PER_LVL(lvl) - 1)
#define ARM_V7S_LVL_IDX(addr, lvl)	({				\
	int _l = lvl;							\
	((addr) >> ARM_V7S_LVL_SHIFT(_l)) & _ARM_V7S_IDX_MASK(_l); \
})

#define MTK_IOMMU_SR_INFO_NSR_WITH_KP 1
#define MTK_IOMMU_SR_INFO_DENY 3
#define SR_INFO_NSR (MTK_IOMMU_SR_INFO_NSR_WITH_KP)
#define SR_INFO_DENY (MTK_IOMMU_SR_INFO_DENY )

#define MTK_V7S_L1_SR_INFO_MASK (0x700cUL)
#define MTK_V7S_L2_SR_INFO_MASK (0x1ccUL)

#define L1_SRINFO_MASK (0x700cUL)
#define L1_SRINFO_BIT0 BIT(2)
#define L1_SRINFO_BIT1 BIT(3)
#define L1_SRINFO_BIT2 BIT(12)
#define L1_SRINFO_BIT3 BIT(13)
#define L1_SRINFO_BIT4 BIT(14)

#define L2_SRINFO_MASK (0x1ccUL)
#define L2_SRINFO_BIT0 BIT(2)
#define L2_SRINFO_BIT1 BIT(3)
#define L2_SRINFO_BIT2 BIT(6)
#define L2_SRINFO_BIT3 BIT(7)
#define L2_SRINFO_BIT4 BIT(8)

#define F_ALL_INVLD     0x2
#define F_MMU_INV_RANGE 0x1

typedef u32 arm_v7s_iopte;

struct mtk_iommu_ctx {
	u32 ttbr;	/* pt_base_addr */
	u32 invld_sa;
	u32 invld_ea;
	u32 invld;
	u32 *pgd;
};

struct mtk_iommu_reg {
	u64 reg_base;
	u64 reg_size;
	/* mapped va to the reg_base */
	void *base_va;
};

struct mtk_iommu_device {
	struct mtk_iommu_reg bank_reg[MAX_BANK];
	struct mtk_iommu_ctx guest;
	u32 table_id;
	bool inited;
	/* PGD */
	u32 pgd_pa[MAX_BANK];
	void *pgd_va[MAX_BANK];
};

struct v7s_page {
	struct list_head node;
	struct list_head lv2_pgtbl_node;
	unsigned int lv2_page_index;
	phys_addr_t lv2_pa_start;
	void *lv2_va_start;
};

struct share_region {
	u64 iova;
	u64 size;
};

#endif
