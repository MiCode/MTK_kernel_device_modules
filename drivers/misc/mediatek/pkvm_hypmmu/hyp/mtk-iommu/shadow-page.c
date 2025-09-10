// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <asm/kvm_pkvm_module.h>
#include <linux/list.h>
#include <nvhe/spinlock.h>

#include <mtk-iommu-defines.h>
#include "include/hypmmu.h"
#include "include/mpool.h"
#include "include/mtk-iommu.h"

#define DEBUG_SHADOW_PAGE 0

#define MAX_PAGE_POOL_SZ_MB (64UL)
#define MAX_PAGE_NODES ((MAX_PAGE_POOL_SZ_MB << 20) >> V7S_PAGE_TABLE_SHIFT)

#define TOTAL_PAGES() (SZ_32M >> V7S_PAGE_TABLE_SHIFT)
#define MPOOL_ALLOC_CONTIG(sz) mpool_alloc_contiguous(&iommu_mpool, sz, 1)

static u64 total_pages;
static u64 avail_pages;

struct v7s_page v7s_page_nodes[MAX_PAGE_NODES];

static struct share_region share_regions[MAX_TABLE_NUM][MAX_SHARE_REGION_NUM];

static struct list_head page_list;
static DEFINE_HYP_SPINLOCK(pgae_list_lock);

static phys_addr_t iommu_mpool_pa;
static void *iommu_mpool_va;
static size_t iommu_mpool_size;
static struct mpool iommu_mpool;
static struct list_head inuse_page_list;

static hyp_spinlock_t hw_table_access_lock[MAX_TABLE_NUM];
static hyp_spinlock_t prot_table_access_lock[MAX_TABLE_NUM];

#if IS_ENABLED(CONFIG_LIST_HARDENED)
bool __list_add_valid_or_report(struct list_head *new,
				struct list_head *prev,
				struct list_head *next)
{
	return mod_ops->list_add_valid_or_report( new, prev, next);
}

bool __list_del_entry_valid_or_report(struct list_head *entry)
{
	return mod_ops->list_del_entry_valid_or_report(entry);
}
#endif

void add_share_region(u64 iova, u64 size, u64 table_id)
{
	u32 i;
	struct share_region *region;

	region = &share_regions[table_id][0];
	for(i = 0; i < MAX_SHARE_REGION_NUM; i++) {
		if (region[i].size)
			continue;
		region[i].iova = iova;
		region[i].size = size;
		break;
	}
	WARN_ON(i >= MAX_SHARE_REGION_NUM);
}

static bool in_share_region(u64 iova, u64 tid)
{
	u64 start, end;
	struct share_region *region;

	region = &share_regions[tid][0];
	for(u32 i = 0; i < MAX_SHARE_REGION_NUM; i++) {
		if (!region[i].size)
			break;

		start = region[i].iova;
		end = region[i].iova + region[i].size;
		if (iova >= start && iova < end)
			return true;
	}

	return false;
}

static void iommu_assign_mpool(phys_addr_t pa, size_t size)
{
	iommu_mpool_pa = pa;
	iommu_mpool_size = size;
}

static void iommu_map_mpool(void *va)
{
	phys_addr_t pa = iommu_mpool_pa;
	size_t size = iommu_mpool_size;

	if (!pa || !size)
		return;

	iommu_mpool_va = va;
	mpool_init(&iommu_mpool, 1);
	mpool_enable_locks();
	mpool_add_chunk(&iommu_mpool, iommu_mpool_va, size);
}

static void create_lv2_pgtbl_info(void)
{
	uint32_t total_pages = TOTAL_PAGES();

	INIT_LIST_HEAD(&inuse_page_list);

	for (unsigned int i = 0; i < total_pages; i++) {
		v7s_page_nodes[i].lv2_page_index = i;
		v7s_page_nodes[i].lv2_va_start = MPOOL_ALLOC_CONTIG(V7S_PAGE_TABLE_SIZE);
		v7s_page_nodes[i].lv2_pa_start = mod_ops->hyp_pa(v7s_page_nodes[i].lv2_va_start);
	}
}

static struct v7s_page *v7s_phys_to_page(phys_addr_t pa)
{
	struct v7s_page *p;

	hyp_spin_lock(&pgae_list_lock);
	list_for_each_entry(p, &inuse_page_list, lv2_pgtbl_node) {
		if (pa >= p->lv2_pa_start && pa < p->lv2_pa_start + V7S_PAGE_TABLE_SIZE) {
			hyp_spin_unlock(&pgae_list_lock);
			return &v7s_page_nodes[p->lv2_page_index];
		}
	}
	hyp_spin_unlock(&pgae_list_lock);
	return NULL;
}

static phys_addr_t v7s_page_to_phys(struct v7s_page *page)
{
	return page->lv2_pa_start;
}

static void dump_usage(void)
{
#if (DEBUG_SHADOW_PAGE >= 2)
	MOD_PUTS2("dump-usage: avail total", avail_pages, total_pages);
#endif
}

static struct v7s_page *alloc_v7s_page(void)
{
	struct v7s_page *page = NULL;

	hyp_spin_lock(&pgae_list_lock);

	if (list_empty(&page_list))
		goto out;

	page = list_last_entry(&page_list, struct v7s_page, node);
	list_del(&page->node);
	avail_pages--;
	list_add_tail(&page->lv2_pgtbl_node, &inuse_page_list);

	dump_usage();
out:
	hyp_spin_unlock(&pgae_list_lock);

	return page;
}

static struct v7s_page *zalloc_v7s_page(void)
{
	struct v7s_page *page;
	phys_addr_t pa;

	page = alloc_v7s_page();
	if (page == NULL)
		return NULL;

	/* set page table memory to zero */
	pa = v7s_page_to_phys(page);
	mod_ops->memset(hyp_phys_to_virt(pa), 0, V7S_PAGE_TABLE_SIZE);

	return page;
}

static void list_remove_entry(struct v7s_page *page)
{
	struct v7s_page *p;

	list_for_each_entry(p, &inuse_page_list, lv2_pgtbl_node) {
		if (p->lv2_page_index == page->lv2_page_index) {
			list_del(&p->lv2_pgtbl_node);
			return;
		}
	}
}

static void free_v7s_page(struct v7s_page *page)
{
	WARN_ON(page == NULL);

	hyp_spin_lock(&pgae_list_lock);
	list_add_tail(&page->node, &page_list);
	avail_pages++;
	list_remove_entry(page);

	dump_usage();

	hyp_spin_unlock(&pgae_list_lock);
}

void create_v7s_pages(u64 rmem_pa, u64 rmem_size)
{
	u64 nr_pages = rmem_size / V7S_PAGE_TABLE_SIZE;
	struct v7s_page *pages = (struct v7s_page *)v7s_page_nodes;
	u32 idx;
	static int init_list;

#if (DEBUG_SHADOW_PAGE)
	MOD_PUTS2("max pool mb_size page_nodes", MAX_PAGE_POOL_SZ_MB, MAX_PAGE_NODES);
#endif

	if (init_list == 0) {
		mod_ops->puts("init list head");
		INIT_LIST_HEAD(&page_list);
		init_list = 1;
	}

	mod_ops->memset((void *)pages, 0, sizeof(struct v7s_page) * nr_pages);

	for (idx = 0; idx < nr_pages; idx++)
		list_add_tail(&pages[idx].node, &page_list);

	total_pages += nr_pages;
	avail_pages = total_pages;

#if (DEBUG_SHADOW_PAGE)
	MOD_PUTS6("create_v7s_pages", rmem_pa, rmem_size,
		nr_pages, total_pages, avail_pages, sizeof(struct v7s_page));
#endif

	iommu_assign_mpool(rmem_pa, rmem_size);
	iommu_map_mpool(mod_ops->hyp_va(rmem_pa));
	create_lv2_pgtbl_info();
}

static bool arm_v7s_pte_is_cont(arm_v7s_iopte pte, int lvl)
{
	if (lvl == 1 && !ARM_V7S_PTE_IS_TABLE(pte, lvl))
		return pte & ARM_V7S_CONT_SECTION;
	else if (lvl == 2)
		return !(pte & ARM_V7S_PTE_TYPE_PAGE);
	return false;
}

static phys_addr_t iopte_to_paddr(arm_v7s_iopte pte, int lvl)
{
	arm_v7s_iopte mask;
	phys_addr_t paddr;

	if (ARM_V7S_PTE_IS_TABLE(pte, lvl))
		mask = ARM_V7S_TABLE_MASK;
	else if (arm_v7s_pte_is_cont(pte, lvl))
		mask = ARM_V7S_LVL_MASK(lvl) * ARM_V7S_CONT_PAGES;
	else
		mask = ARM_V7S_LVL_MASK(lvl);

	paddr = pte & mask;

	if (pte & ARM_V7S_ATTR_MTK_PA_BIT32)
		paddr |= BIT_ULL(32);
	if (pte & ARM_V7S_ATTR_MTK_PA_BIT33)
		paddr |= BIT_ULL(33);
	if (pte & ARM_V7S_ATTR_MTK_PA_BIT34)
		paddr |= BIT_ULL(34);

	return paddr;
}

/* only used for donated or shared hypvisor memory */
static arm_v7s_iopte *iopte_deref(arm_v7s_iopte pte, int lvl)
{
	return hyp_phys_to_virt(iopte_to_paddr(pte, lvl));
}

static u64 v7s_pgd_index(u64 iova)
{
	/* pgd index, bits[33:20] */
	return iova >> 20;
}

static void flush_pgd(u32 *pgd, u64 iova_start, u64 iova_end)
{
	u32 start_idx = v7s_pgd_index(iova_start);
	u32 end_idx = v7s_pgd_index(iova_end - 1);
	u32 count = end_idx - start_idx + 1;
	void *ptr;

	ptr = &pgd[start_idx];
	mod_ops->flush_dcache_to_poc(ptr, count * sizeof(u32));
}

static void v7s_unmap(u32 *pte)
{
	phys_addr_t pa;
	struct v7s_page *page;
	u32 pte_old = *pte;

	if (ARM_V7S_PTE_IS_TABLE(pte_old, 1)) {
		/* free page table */
		pa = iopte_to_paddr(pte_old, 1);
		page = v7s_phys_to_page(pa);
		free_v7s_page(page);
	}
	/* replace entry with 0 */
	*pte = 0;
}

static void lock_hw_table_access(u64 tid)
{
	hyp_spin_lock(&hw_table_access_lock[tid]);
}

static void unlock_hw_table_access(u64 tid)
{
	hyp_spin_unlock(&hw_table_access_lock[tid]);
}

static void lock_prot_table_access(u64 tid)
{
	hyp_spin_lock(&prot_table_access_lock[tid]);
}

static void unlock_prot_table_access(u64 tid)
{
	hyp_spin_unlock(&prot_table_access_lock[tid]);
}

int io_pgtable_unmap(u64 tid, u32 *h_pte, u32 *p_pte)
{
	lock_hw_table_access(tid);
	v7s_unmap(h_pte);
	unlock_hw_table_access(tid);

	lock_prot_table_access(tid);
	v7s_unmap(p_pte);
	unlock_prot_table_access(tid);

	return 0;
}

static u32 l1_get_srinfo(arm_v7s_iopte pte)
{
	u32 sr_info = 0;

	if (pte & L1_SRINFO_BIT0)
		sr_info |= BIT(0);
	if (pte & L1_SRINFO_BIT1)
		sr_info |= BIT(1);
	if (pte & L1_SRINFO_BIT2)
		sr_info |= BIT(2);
	if (pte & L1_SRINFO_BIT3)
		sr_info |= BIT(3);
	if (pte & L1_SRINFO_BIT4)
		sr_info |= BIT(4);

	return sr_info;
}

static u32 l2_get_srinfo(arm_v7s_iopte pte)
{
	u32 sr_info = 0;

	if (pte & L2_SRINFO_BIT0)
		sr_info |= BIT(0);
	if (pte & L2_SRINFO_BIT1)
		sr_info |= BIT(1);
	if (pte & L2_SRINFO_BIT2)
		sr_info |= BIT(2);
	if (pte & L2_SRINFO_BIT3)
		sr_info |= BIT(3);
	if (pte & L2_SRINFO_BIT4)
		sr_info |= BIT(4);

	return sr_info;
}

static arm_v7s_iopte l1_set_srinfo(arm_v7s_iopte pte, u32 sr_info)
{
	pte &= (u32)~L1_SRINFO_MASK;

	if (sr_info & BIT(0))
		pte |= L1_SRINFO_BIT0;
	if (sr_info & BIT(1))
		pte |= L1_SRINFO_BIT1;
	if (sr_info & BIT(2))
		pte |= L1_SRINFO_BIT2;
	if (sr_info & BIT(3))
		pte |= L1_SRINFO_BIT3;
	if (sr_info & BIT(4))
		pte |= L1_SRINFO_BIT4;

	return pte;
}

static arm_v7s_iopte l2_set_srinfo(arm_v7s_iopte pte, u32 sr_info)
{
	pte &= (u32)~L2_SRINFO_MASK;

	if (sr_info & BIT(0))
		pte |= L2_SRINFO_BIT0;
	if (sr_info & BIT(1))
		pte |= L2_SRINFO_BIT1;
	if (sr_info & BIT(2))
		pte |= L2_SRINFO_BIT2;
	if (sr_info & BIT(3))
		pte |= L2_SRINFO_BIT3;
	if (sr_info & BIT(4))
		pte |= L2_SRINFO_BIT4;

	return pte;
}

static arm_v7s_iopte to_mtk_iopte(phys_addr_t paddr, arm_v7s_iopte pte)
{
	if (paddr & BIT_ULL(32))
		pte |= ARM_V7S_ATTR_MTK_PA_BIT32;
	if (paddr & BIT_ULL(33))
		pte |= ARM_V7S_ATTR_MTK_PA_BIT33;
	if (paddr & BIT_ULL(34))
		pte |= ARM_V7S_ATTR_MTK_PA_BIT34;
	return pte;
}

u32 debug_io_get_pte(u32 *pgd, u64 iova, u32 *out_pte, u64 table_id)
{
	u32 *tablep;
	u32 idx = ARM_V7S_LVL_IDX(iova, 1);
	u32 pte = pgd[idx];
	u32 lvl;

	if (ARM_V7S_PTE_IS_TABLE(pte, 1)) {
		tablep = iopte_deref(pte, 1);
		pte = tablep[ARM_V7S_LVL_IDX(iova, 2)];
		lvl = 2;
	} else
		lvl = 1;

	*out_pte = pte;
	return lvl;
}

u32 debug_make_result(u64 iova, u32 pte, u32 lvl)
{
	phys_addr_t pa;
	u32 sr_info;
	u32 type;
	u32 ret;
	u32 mask = GENMASK(31, 12);

	if (!ARM_V7S_PTE_IS_VALID(pte))
		return 0;

	pa = iopte_to_paddr(pte, lvl);
	sr_info = (lvl == 1) ? l1_get_srinfo(pte) : l2_get_srinfo(pte);
	type = pte & 0x3;

	/*
	 * bits[31:12] = pa[31:12];
	 * bits[11:10] = lvl
	 * bits[9:8]: type
	 * bits[7:3]: sr_info
	 * bits[2:0]: pa[34:32]
	 */
	ret = (pa & mask) | ((u32)(pa >> 32));
	ret |= lvl << 10;
	ret |= type << 8;
	ret |= sr_info << 3;

	MOD_PUTS5("tf: make_result: iova pa lvl srinfo pte", iova, pa,
		lvl, sr_info, pte);

	return ret;
}

/*
 * allocate page table when it's empty. return existing table if it's not empty
 */
arm_v7s_iopte install_table(arm_v7s_iopte *pte)
{
	struct v7s_page *page;
	arm_v7s_iopte new;
	u64 pa;

	/* had table installed? */
	if (ARM_V7S_PTE_IS_TABLE(*pte, 1))
		/* yes, return existed table */
		return *pte;

	/* no, allocate new one */
	page = zalloc_v7s_page();
	if (!page) {
		mod_ops->puts("out of v7s pages");
		WARN_ON(1);
	}
	pa = v7s_page_to_phys(page);
	new = (arm_v7s_iopte)(pa | ARM_V7S_PTE_TYPE_TABLE);
	new = to_mtk_iopte(pa, new);
	*pte = new;

	return new;
}

static void *guest_fixmap_map(phys_addr_t pa)
{
	phys_addr_t rd_pa;
	u32 offset;
	void *va;

	rd_pa = round_down(pa, PAGE_SIZE);
	offset = (u32)(pa - rd_pa);
	va = mod_ops->fixmap_map(rd_pa);

	return va + offset;
}

static void guest_fixmap_unmap(void)
{
	mod_ops->fixmap_unmap();

}

static void handle_hw_table(u32 *h_pte, u32 *g_tablep)
{
	arm_v7s_iopte pte, pte_nsr;
	arm_v7s_iopte *h_tablep;
	phys_addr_t pa;
	u32 zero_counter = 0;
	u32 srinfo;

	/* install hw page table */
	pte = install_table(h_pte);
	pa = iopte_to_paddr(pte, 1);
	h_tablep = iopte_deref(pte, 1);

	if (!h_tablep) {
		mod_ops->puts("h_tablep is NULL");
		WARN_ON(h_tablep == NULL);
		return;
	}

	for(u32 i = 0; i < 256; i++) {
		pte = g_tablep[i];
		srinfo = query_ac_srinfo(iopte_to_paddr(pte, 2));

		if (ARM_V7S_PTE_IS_VALID(pte)) {
			if (srinfo == SR_INFO_NSR) {
				pte_nsr = l2_set_srinfo(pte, SR_INFO_NSR);
				h_tablep[i] = pte_nsr;
			} else
				/* protected PA, never map in HW */
				h_tablep[i] = 0;
		} else
			/* unmap */
			h_tablep[i] = 0;

		if (h_tablep[i] == 0)
			zero_counter++;
	}

	/* flush page table */
	mod_ops->flush_dcache_to_poc(h_tablep, V7S_PAGE_TABLE_SIZE);

	if (zero_counter == 256)
		/* remove hw page table */
		v7s_unmap(h_pte);
}

static void handle_prot_table(u64 iova, u64 tid, u32 *p_pte, u32 *g_tablep)
{
	arm_v7s_iopte pte, pte_nsr;
	arm_v7s_iopte *p_tablep;
	u32 srinfo;
	bool share = in_share_region(iova, tid);
	u32 zero_counter = 0;

#if (DEBUG_SHADOW_PAGE >= 2)
	if (share)
		MOD_PUTS1("handle-prot-table: in share region: true", iova);
#endif

	pte = install_table(p_pte);
	p_tablep = iopte_deref(pte, 1);

	for (u32 i = 0; i < 256; i++) {
		pte = g_tablep[i];
		if (!ARM_V7S_PTE_IS_VALID(pte)) {
			/* unmap */
			p_tablep[i] = 0;
			zero_counter++;
			continue;
		}
		pte_nsr = l2_set_srinfo(pte, SR_INFO_NSR);
		srinfo = query_ac_srinfo(iopte_to_paddr(pte, 2));

		if ((srinfo != SR_INFO_NSR)  && share)
			mod_ops->puts("WARN: share and non-nsr can't be used in the same time");

		/* has srinfo? */
		if (srinfo == SR_INFO_NSR) {
			/* normal pa. iova in share region? */
			if (share)
				/* map prot with nsr if iova in share region */
				p_tablep[i] = pte_nsr;
		} else {
			/* protected pa, exclusive map to prot with srinfo */
			pte = l2_set_srinfo(pte, srinfo);
			p_tablep[i] = pte;
			continue;
		}
	}

	/* flush page table */
	mod_ops->flush_dcache_to_poc(p_tablep, V7S_PAGE_TABLE_SIZE);

	if (zero_counter == 256)
		/* remove hw page table */
		v7s_unmap(p_pte);
}

int guest_to_io_table(u64 iova, u64 tid, u32 guest_pte, u32 *h_pte, u32 *p_pte)
{
	arm_v7s_iopte *g_tablep;
	phys_addr_t pa;

	/* get guest table pa */
	pa = iopte_to_paddr(guest_pte, 1);

	/* fast map guest table buffer */
	g_tablep = guest_fixmap_map(pa);

	lock_hw_table_access(tid);
	handle_hw_table(h_pte, g_tablep);
	unlock_hw_table_access(tid);

	lock_prot_table_access(tid);
	handle_prot_table(iova, tid, p_pte, g_tablep);
	unlock_prot_table_access(tid);

	/* unmap guest table buffer*/
	guest_fixmap_unmap();

	return 0;
}

int guest_to_io_section(u64 iova, u64 tid, u32 guest_pte, u32 *h_pte, u32 *p_pte)
{
	u32 pte, pte_nsr, srinfo;
	u64 pa;
	bool share = in_share_region(iova, tid);

#if (DEBUG_SHADOW_PAGE >= 2)
	if (share)
		MOD_PUTS1("guest-to-io-section: in share region: true", iova);
#endif

	pte_nsr = l1_set_srinfo(guest_pte, SR_INFO_NSR);
	pa = iopte_to_paddr(guest_pte, 1);
	srinfo = query_ac_srinfo(pa);

	lock_hw_table_access(tid);
	lock_prot_table_access(tid);

	/* unmap existed entry */
	v7s_unmap(h_pte);
	v7s_unmap(p_pte);

	/* then write new entry value */
	if (srinfo == SR_INFO_NSR) {
		/* normal pa, map to hw with nsr */
		*h_pte = pte_nsr;
		/* map prot with nsr if iova in share region */
		if (share)
			*p_pte = pte_nsr;
	} else {
		/* protected pa, exclusive map to prot with srinfo*/
		*h_pte = 0;	/* unmap to hw */
		pte = l1_set_srinfo(guest_pte, srinfo);
		*p_pte = pte;	/* map to prot with srinfo */
	}

	unlock_prot_table_access(tid);
	unlock_hw_table_access(tid);

	return 0;
}

/*
 * guest map -> check pa in ac_table +-> hw map with nsr (NS_BANK)
 *                                   |
 *                                   +-> prot map with sr_info (PROT_1_BANK)
 */
int io_pgtable_map(u64 iova, u64 tid, u32 guest_pte, u32 *h_pte, u32 *p_pte)
{
	int ret;

	if (ARM_V7S_PTE_IS_TABLE(guest_pte, 1))
		ret = guest_to_io_table(iova, tid, guest_pte, h_pte, p_pte);
	else
		ret = guest_to_io_section(iova, tid, guest_pte, h_pte, p_pte);

	return ret;
}

/*
 * IO pgtable handler.
 * Syn guest page table into HW(IO) page table.
 * Base on range of iova.
 */
int io_pgtable_handler(u64 iova_start, u64 iova_size, u64 tid)
{
	u64 cur_iova = iova_start;
	u64 iova_end = iova_start + iova_size;
	u64 g_pgd_pa = query_guest_pgd(tid);
	u64 h_pgd_pa = query_hw_pgd(tid);
	u64 p_pgd_pa = query_prot_pgd(tid);
	u32 *g_pgd, *h_pgd, *p_pgd;
	u32 guest_pte, i;
	int ret = 0;

	if (!g_pgd_pa || !h_pgd_pa) {
		MOD_PUTS2("io-pgtable-handler: invalid params", g_pgd_pa, h_pgd_pa);
		return -EINVAL;
	}

	h_pgd = (u32 *)hyp_phys_to_virt(h_pgd_pa);
	g_pgd = (u32 *)hyp_phys_to_virt(g_pgd_pa);
	p_pgd = (u32 *)hyp_phys_to_virt(p_pgd_pa);

	do {
		i = v7s_pgd_index(cur_iova);

		/* snapshot current guest pte value */
		guest_pte = g_pgd[i];

		if (!ARM_V7S_PTE_IS_VALID(guest_pte))
			ret = io_pgtable_unmap(tid, &h_pgd[i], &p_pgd[i]);
		else
			ret = io_pgtable_map(cur_iova, tid, guest_pte, &h_pgd[i], &p_pgd[i]);
		if (ret)
			break;

		cur_iova += SZ_1M;
	} while(cur_iova < iova_end);

	/* flush pgd */
	flush_pgd(h_pgd, iova_start, iova_end);
	flush_pgd(p_pgd, iova_start, iova_end);

	return ret;
}
