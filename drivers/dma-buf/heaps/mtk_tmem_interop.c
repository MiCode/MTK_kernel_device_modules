// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF mtk_sec heap exporter
 *
 * Copyright (C) 2024 MediaTek Inc.
 *
 */

#define pr_fmt(fmt) "dma_heap: mtk_tmem_interop " fmt
#define TMEM_PRIV static

#include "mtk_tmem_interop.h"

#define DEBUG_BITMAP 0

static bool is_pre_alloc_total_num(struct secure_heap_page *sec_heap)
{
	if ((sec_heap->tmem_type == TRUSTED_MEM_REQ_PROT_PAGE) &&
		(atomic64_read(&sec_heap->total_num) == get_pre_alloc_page_num()))
		return true;

	return false;
}

#if (ENABLE_PKVM_PMM == 1)
static int pkvm_mgmt_get_hcall(uint32_t smc_id)
{
	struct arm_smccc_res res;

	arm_smccc_1_1_smc(smc_id, 0, 0, 0, 0, 0, 0, &res);
	return (int)res.a1;
}
#endif

static int pmm_prot_via_pkvm(struct page *pmm_ipc_pg, uint32_t pmm_attr, uint32_t count, int lock)
{
	int ret = 0;
#if (ENABLE_PKVM_PMM == 1)
	static int assign_hcall;
	static int unassign_hcall;
	int id;

	if (!assign_hcall)
		assign_hcall = pkvm_mgmt_get_hcall(SMC_ID_MTK_PKVM_PMM_ASSIGN_BUFFER_V2);
	if (!unassign_hcall)
		unassign_hcall = pkvm_mgmt_get_hcall(SMC_ID_MTK_PKVM_PMM_UNASSIGN_BUFFER_V2);

	id = lock == 1 ? assign_hcall : unassign_hcall;
	ret = pkvm_el2_mod_call(id, page_to_pfn(pmm_ipc_pg), pmm_attr, count);
#else
	ret = pkvm_smmu_mapping(pmm_ipc_pg, pmm_attr, count, lock);
#endif
	return ret;
}

static int pmm_prot_via_mtee(struct page *pmm_ipc_pg, uint32_t pmm_attr, uint32_t count, int lock )
{
	struct arm_smccc_res smc_res;
	uint32_t smc_id;

	smc_id = lock == 1 ? HYP_PMM_ASSIGN_BUFFER_V2 : HYP_PMM_UNASSIGN_BUFFER_V2;
	arm_smccc_smc(smc_id, page_to_pfn(pmm_ipc_pg), pmm_attr, count, 0, 0, 0, 0, &smc_res);
	if (smc_res.a0 != 0) {
		pr_err("smc_id=%#x smc_res.a0=%#lx\n", smc_id, smc_res.a0);
		return -EINVAL;
	}

	return 0;
}

/* TMEM common functions */
TMEM_PRIV int pmm_common_buffer_v2(struct ssheap_buf_info *ssheap,
		u8 pmm_attr, int lock)
{
	struct page *pmm_page, *tmp_page;
	uint32_t tmp_count = 0;
	int count = 0;
	int ret;

	if (!ssheap || !ssheap->pmm_page) {
		pr_err("ssheap info not ready!\n");
		return -EINVAL;
	}
	count = ssheap->elems;
	pmm_page = ssheap->pmm_page;
	list_for_each_entry_safe(pmm_page, tmp_page, &ssheap->pmm_msg_list,
				  lru) {
		tmp_count =
			count >= PMM_MSG_ENTRIES_PER_PAGE ?
				(uint32_t)PMM_MSG_ENTRIES_PER_PAGE :
				(uint32_t)(count % PMM_MSG_ENTRIES_PER_PAGE);

		if (is_pkvm_enabled())
			ret = pmm_prot_via_pkvm(pmm_page, pmm_attr, tmp_count, lock);
		else
			ret = pmm_prot_via_mtee(pmm_page, pmm_attr, tmp_count, lock);
		if (ret)
			return ret;
		count -= PMM_MSG_ENTRIES_PER_PAGE;
	}
	return 0;
}

TMEM_PRIV int pmm_assign_buffer_v2(struct ssheap_buf_info *ssheap,
		u8 pmm_attr)
{
	return pmm_common_buffer_v2(ssheap, pmm_attr, 1);
}

TMEM_PRIV int pmm_unassign_buffer_v2(struct ssheap_buf_info *ssheap,
		u8 pmm_attr)
{
	return pmm_common_buffer_v2(ssheap, pmm_attr, 0);
}

TMEM_PRIV int paddr_cmp(void *priv, const struct list_head *a,
		const struct list_head *b)
{
	struct page *ia, *ib;

	ia = list_entry(a, struct page, lru);
	ib = list_entry(b, struct page, lru);

	if (page_to_phys(ia) > page_to_phys(ib))
		return 1;
	if (page_to_phys(ia) < page_to_phys(ib))
		return -1;

	return 0;
}

#if (ENABLE_PKVM_PMM == 0)
/* According to lock, map those scatter list page into mtk pkvm smmu page table with related
 * permission.
 */
TMEM_PRIV int pkvm_smmu_mapping(struct page *pmm_page, u8 pmm_attr,
		u32 tmp_count, int lock)
{
#if IS_ENABLED(CONFIG_MTK_PKVM_SMMU)
	static uint32_t hvc_id_map;
	static uint32_t hvc_id_unmap;
	struct arm_smccc_res res;
	uint32_t hvc_id;
	int ret;

	if (!hvc_id_map) {
		arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_SMMU_SEC_MAP,
			0, 0, 0, 0, 0, 0, &res);
		hvc_id_map = res.a1;
	}

	if (!hvc_id_unmap) {
		arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_SMMU_SEC_UNMAP,
			0, 0, 0, 0, 0, 0, &res);
		hvc_id_unmap = res.a1;
	}

	hvc_id = (lock == 1) ? hvc_id_map : hvc_id_unmap;
	if (hvc_id != 0) {
		ret = pkvm_el2_mod_call(hvc_id, page_to_pfn(pmm_page), pmm_attr,
					tmp_count);

		if (ret != 0)
			pr_info("hvc_id=%#x smmu_ret=%x\n", hvc_id, ret);
	} else
		pr_info("%s hvc is invalid\n", __func__);
#endif
	return 0;
}
#endif

void pkvm_pmm_defragment(void)
{
	static int defragment_hcall;

	if (!defragment_hcall)
		defragment_hcall = pkvm_mgmt_get_hcall(SMC_ID_MTK_PKVM_PMM_DEFRAGMENT);
	else
		pkvm_el2_mod_call(defragment_hcall);
}

#if (ENABLE_PKVM_PMM == 0)
/* Merge SMMU normal VM page table into large page, when exit secure feature */
TMEM_PRIV void pkvm_smmu_merge_ptable(void)
{
#if IS_ENABLED(CONFIG_MTK_PKVM_SMMU)
	struct arm_smccc_res res;
	static uint32_t hvc_id_merge_table;
	int ret = 0;

	if (!hvc_id_merge_table) {
		arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_SMMU_PAGE_TABLE_MERGE, 0, 0,
				  0, 0, 0, 0, &res);
		hvc_id_merge_table = res.a1;
	}

	if (hvc_id_merge_table != 0) {
		ret = pkvm_el2_mod_call(hvc_id_merge_table);

		if (ret != 0)
			pr_info("hvc_id=%#x smmu_ret=%x\n", hvc_id_merge_table,
				ret);
	} else
		pr_info("%s hvc is invalid\n", __func__);
#endif /* IS_ENABLED(CONFIG_MTK_PKVM_SMMU) */
}
#endif

int pmm_defragment(struct secure_heap_page *sec_heap)
{
	struct arm_smccc_res smc_res;

	if (is_pkvm_enabled()) {
#if (ENABLE_PKVM_PMM == 1)
		pkvm_pmm_defragment();
#else
		pkvm_smmu_merge_ptable();
#endif
	} else {
		arm_smccc_smc(HYP_PMM_MERGED_TABLE, page_to_pfn(sec_heap->bitmap),
				0, 0, 0, 0, 0, 0, &smc_res);
		if (smc_res.a0 != 0) {
			pr_err("smc_res.a0=%#lx\n", smc_res.a0);
			return -EINVAL;
		}
	}
	memset(page_address(sec_heap->bitmap), 0, PAGE_SIZE);

	return 0;
}

/*
 *	PMM_MSG_ENTRY format
 *	page number = PA >> PAGE_SHIFT
 *	 _______________________________________
 *	|  reserved  | page order | page number |
 *	|____________|____________|_____________|
 *	31         28 27        24 23          0
 */
TMEM_PRIV inline void pmm_set_msg_entry(u32 *pmm_msg, u32 index,
				     struct page *page)
{
	int order;
	phys_addr_t pa;

	order = compound_order(page);
	pa = page_to_phys(page);

	pmm_msg[index] = PMM_MSG_ENTRY(pa, order);
}

TMEM_PRIV struct page *pmm_alloc_msg_v2(struct sg_table *table,
		struct ssheap_buf_info *ssheap, unsigned int max_order)
{
	struct page *pmm_page, *tmp_page, *head_page;
	struct scatterlist *sg;
	struct list_head *pmm_msg_list;
	unsigned long size_remaining = 0;
	void *pmm_msg;
	int i = 0, pmepp = PMM_MSG_ENTRIES_PER_PAGE;

	pmm_msg_list = &ssheap->pmm_msg_list;
	INIT_LIST_HEAD(pmm_msg_list);

	size_remaining =
		ROUNDUP(table->orig_nents * sizeof(uint32_t), PAGE_SIZE);
	pr_debug("%s: size_remaining=%#lx\n", __func__, size_remaining);

	while (size_remaining > 0) {
		pmm_page = alloc_pages(GFP_KERNEL | __GFP_ZERO, max_order);
		if (!pmm_page)
			goto free_buffer;
		list_add_tail(&pmm_page->lru, pmm_msg_list);
		size_remaining -= page_size(pmm_page);
		if (i == 0)
			head_page = pmm_page;
		i++;
	}
	pr_debug("%s: pmm_msg alloc %d pages\n", __func__, i);
	pmm_msg = page_address(head_page); // first page's addr
	tmp_page = head_page; // FOR TRIVERS

	sg = table->sgl;
	for (i = 0; i < table->orig_nents; i++) {
		if (i != 0 && (i % pmepp == 0)) {
			tmp_page = list_next_entry(tmp_page, lru);
			pmm_msg = page_address(tmp_page);
		}
		if (!sg) {
			pr_info("%s err, sg null at %d\n", __func__, i);
			goto free_buffer;
		}
		pmm_set_msg_entry(pmm_msg, i % pmepp, sg_page(sg));
		sg = sg_next(sg);
	}

	return head_page;

free_buffer:
	list_for_each_entry_safe(pmm_page, tmp_page, pmm_msg_list, lru) {
		max_order = compound_order(pmm_page);
		if (max_order < NR_PAGE_ORDERS)
			__free_pages(pmm_page, max_order);
	}
	return NULL;
}
TMEM_PRIV struct ssheap_buf_info *ssheap_create_by_sgtable(
		struct sg_table *table, ulong req_sz)
{
	struct ssheap_buf_info *buf = NULL;

	buf = kzalloc(sizeof(struct ssheap_buf_info), GFP_KERNEL);
	if (buf == NULL)
		return NULL;

	buf->table = table;
	buf->aligned_req_size = req_sz;
	buf->alignment = PAGE_SIZE;
	return buf;
}

int tmem_api_ver(void)
{
	static int api_ver = INT_MIN;

	if (api_ver < 0)
		api_ver = trusted_mem_is_page_v2_enabled() ? 2 : 1;

	return api_ver;
}

/* TMEM page-based functions */

TMEM_PRIV struct page *page_alloc_largest_available(ulong size, uint max_order)
{
	struct page *page;
	int i;

	for (i = 0; i < SEC_NUM_ORDERS; i++) {
		if (size < (PAGE_SIZE << sec_orders[i]) &&
				i < SEC_NUM_ORDERS - 1)
			continue;
		if (max_order < sec_orders[i])
			continue;
		page = dmabuf_page_pool_alloc(sec_pools[i]);
		if (!page)
			continue;
		return page;
	}
	return NULL;
}

TMEM_PRIV int page_alloc(struct secure_heap_page *sec_heap,
		struct mtk_sec_heap_buffer *buffer, ulong req_sz)
{
	int ret;
	u64 sec_handle = 0;
	u32 refcount = 0; /* tmem refcount */
	struct ssheap_buf_info *ssheap = NULL;
	struct sg_table *table = &buffer->sg_table;

	if (buffer->len > UINT_MAX) {
		pr_err("%s error. len more than UINT_MAX\n", __func__);
		return -EINVAL;
	}
	ret = trusted_mem_api_alloc(
			sec_heap->tmem_type, 0, &buffer->len, &refcount,
			&sec_handle,
			(uint8_t *)dma_heap_get_name(sec_heap->heap), 0,
			&ssheap);
	if (!ssheap) {
		pr_err("%s error, alloc page base failed\n", __func__);
		return -ENOMEM;
	}

	ret = sg_alloc_table(table, ssheap->table->orig_nents, GFP_KERNEL);
	if (ret) {
		pr_err("%s error. sg_alloc_table failed\n", __func__);
		goto free_tmem;
	}

	ret = copy_sec_sg_table(ssheap->table, table);
	if (ret) {
		pr_err("%s error. copy_sec_sg_table failed\n", __func__);
		goto free_table;
	}
	buffer->len = ssheap->aligned_req_size;
	buffer->ssheap = ssheap;
	atomic64_add(buffer->len, &sec_heap->total_size);

	pr_debug(
		"%s done: [%s], req_size:%#lx(%#lx), align_sz:%#x, nent:%u--%lu, align:%#lx, total_sz:%#llx\n",
		__func__, dma_heap_get_name(sec_heap->heap),
		buffer->ssheap->req_size, req_sz, buffer->len,
		buffer->ssheap->table->orig_nents, buffer->ssheap->elems,
		buffer->ssheap->alignment,
		atomic64_read(&sec_heap->total_size));

	return 0;

free_table:
	sg_free_table(table);
free_tmem:
	trusted_mem_api_unref(sec_heap->tmem_type, sec_handle,
			      (uint8_t *)dma_heap_get_name(buffer->heap), 0,
			      ssheap);

	return ret;
}

TMEM_PRIV int page_alloc_v2(struct secure_heap_page *sec_heap,
		struct mtk_sec_heap_buffer *buffer, ulong req_sz)
{
	// check input parameters
	// if (!is_page_based_heap_type(sec_heap->tmem_type))
	//	return -EINVAL;
	unsigned long size_remaining = 0;
	unsigned int max_order = sec_orders[0];
	struct sg_table *table;
	struct scatterlist *sg;
	struct list_head pages;
	struct page *page, *tmp_page, *pmm_page;
	int i = 0, smc_ret = 0;

	size_remaining = ROUNDUP(req_sz, BASE_SEC_HEAP_SZ);
	pr_debug("%s: req_sz=%#lx, size_remaining=%#lx\n", __func__, req_sz,
		 size_remaining);
	// allocage pages by dma-poll
	INIT_LIST_HEAD(&pages);

	while (size_remaining > 0) {
		page = page_alloc_largest_available(size_remaining, max_order);
		if (!page) {
			pr_err("%s: Failed to alloc pages!!\n", __func__);
			goto free_pages;
		}
		list_add_tail(&page->lru, &pages);
		size_remaining -= page_size(page);
		max_order = compound_order(page);
		i++;
	}
	// sort pages by pa
	list_sort(NULL, &pages, paddr_cmp);

	table = &buffer->sg_table;
	if (sg_alloc_table(table, i, GFP_KERNEL)) {
		pr_err("%s: sg_alloc_table failed\n", __func__);
		goto free_pages;
	}

	sg = table->sgl;
	list_for_each_entry_safe(page, tmp_page, &pages, lru) {
		sg_set_page(sg, page, page_size(page), 0);
		sg = sg_next(sg);
	}

	/*
	 * For uncached buffers, we need to initially invalid cpu cache, since
	 * the __GFP_ZERO on the allocation means the zeroing was done by the
	 * cpu and thus it is likely cached. Map (and implicitly flush) and
	 * unmap it now so we don't get corruption later on.
	 */
	dma_map_sgtable(dma_heap_get_dev(buffer->heap), table,
			DMA_BIDIRECTIONAL, 0);
	dma_unmap_sgtable(dma_heap_get_dev(buffer->heap), table,
			  DMA_BIDIRECTIONAL, 0);

	//TODO naming change to assign sg to ssheap
	buffer->ssheap = ssheap_create_by_sgtable(table, req_sz);
	if (buffer->ssheap == NULL) {
		pr_err("%s: ssheap_create_by_sgtable failed\n", __func__);
		goto free_sg_table;
	}

	pmm_page =
		pmm_alloc_msg_v2(table, buffer->ssheap, get_order(PAGE_SIZE));
	if (!pmm_page) {
		pr_err("%s: pmm_alloc_msg_v2 failed\n", __func__);
		kfree(buffer->ssheap);
		goto free_sg_table;
	}

	buffer->ssheap->pmm_page = pmm_page;
	buffer->ssheap->elems = table->orig_nents;
	buffer->len = buffer->ssheap->aligned_req_size;
	atomic64_add(buffer->len, &sec_heap->total_size);
	atomic64_add(1, &sec_heap->total_num);

	trusted_mem_enable_high_freq();
	// pmm_assign assign pa to protected pa
	smc_ret = pmm_assign_buffer_v2(buffer->ssheap, sec_heap->tmem_type);
	trusted_mem_disable_high_freq();

	if (smc_ret != 0) {
		pr_err("%s:pmm_assign_buffer_v2 smc_ret=%d\n", __func__,
		       smc_ret);
		kfree(buffer->ssheap);
		goto free_pmm_page;
	}
	return 0;

free_pmm_page:
	list_for_each_entry_safe(pmm_page, tmp_page,
				  &buffer->ssheap->pmm_msg_list, lru) {
		max_order = compound_order(pmm_page);
		if (max_order < NR_PAGE_ORDERS)
			__free_pages(pmm_page, max_order);
	}
free_sg_table:
	sg_free_table(table);
free_pages:
	list_for_each_entry_safe(page, tmp_page, &pages, lru) {
		max_order = compound_order(page);
		if (max_order < NR_PAGE_ORDERS)
			__free_pages(page, max_order);
	}

	return -ENOMEM;
}

struct dma_buf *tmem_page_alloc(struct dma_heap *heap, ulong len, u32 fd_flags,
		u64 heap_flags)
{
	int ret = -ENOMEM;
	struct dma_buf *dmabuf;
	struct mtk_sec_heap_buffer *buffer;
	struct secure_heap_page *sec_heap = sec_heap_page_get(heap);
	u64 sec_handle = 0;

	if (!sec_heap) {
		pr_err("%s, %s can not find secure heap!!\n", __func__,
		       heap ? dma_heap_get_name(heap) : "null ptr");
		return ERR_PTR(-EINVAL);
	}

	if (len / PAGE_SIZE > totalram_pages()) {
		pr_err("%s, len %ld is more than %ld\n", __func__, len,
			totalram_pages() * PAGE_SIZE);
		return ERR_PTR(-ENOMEM);
	}

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	buffer->len = len;
	buffer->heap = heap;
	/* all page base memory set as noncached buffer */
	buffer->uncached = true;
	buffer->show = sec_buf_priv_dump;
	buffer->gid = -1;

	if (tmem_api_ver() == 2)
		ret = page_alloc_v2(sec_heap, buffer, len);
	else
		ret = page_alloc(sec_heap, buffer, len);

	if (ret) {
		pr_err("%s: page_alloc%s ret = %d\n", __func__,
		       tmem_api_ver() == 2 ? "_v2" : "", ret);
		goto free_buffer;
	}

	// TODO: need fix cause didn't call free in free
	ret = trusted_mem_page_based_alloc(sec_heap->tmem_type, &buffer->sg_table,
				     &sec_handle, len);
	/* store seucre handle */
	buffer->sec_handle = sec_handle;

	dmabuf = alloc_dmabuf(heap, buffer, &sec_buf_page_ops, fd_flags);
	if (IS_ERR(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		pr_err("%s alloc_dmabuf fail\n", __func__);
		goto free_tmem;
	}

	init_buffer_info(heap, buffer);

	return dmabuf;

free_tmem:
	if (tmem_api_ver() == 2) {
		page_free_v2(sec_heap, buffer);
	} else {
		trusted_mem_api_unref(
			sec_heap->tmem_type, buffer->sec_handle,
			(uint8_t *)dma_heap_get_name(buffer->heap), 0,
			buffer->ssheap);
	}
	sg_free_table(&buffer->sg_table);
free_buffer:
	kfree(buffer);
	return ERR_PTR(ret);
}

TMEM_PRIV int page_free(struct secure_heap_page *sec_heap,
		struct mtk_sec_heap_buffer *buffer)
{
	int ret;

	ret = trusted_mem_api_unref(sec_heap->tmem_type, buffer->sec_handle,
				    (uint8_t *)dma_heap_get_name(buffer->heap),
				    0, buffer->ssheap);
	if (ret) {
		pr_err("%s error, trusted_mem_api_unref failed, heap:%s, ret:%d\n",
		       __func__, dma_heap_get_name(buffer->heap), ret);
		return ret;
	}

	if (atomic64_sub_return(buffer->len, &sec_heap->total_size) < 0)
		pr_warn("%s, total memory overflow, 0x%llx!!\n", __func__,
			atomic64_read(&sec_heap->total_size));

	pr_debug("%s done, [%s] size:%#x, total_size:%#llx\n", __func__,
		 dma_heap_get_name(buffer->heap), buffer->len,
		 atomic64_read(&sec_heap->total_size));

	return ret;
}

TMEM_PRIV int page_free_v2(struct secure_heap_page *sec_heap,
		struct mtk_sec_heap_buffer *buffer)
{
	struct page *pmm_page, *tmp_page;
	struct list_head *pmm_msg_list;
	struct sg_table *table = NULL;
	struct scatterlist *sg = NULL;
	int smc_ret, i, j, page_count = 0;
	uint32_t *bitmap = page_address(sec_heap->bitmap);
	int ret;

	smc_ret = pmm_unassign_buffer_v2(buffer->ssheap, sec_heap->tmem_type);
	if (smc_ret) {
		pr_err("%s: error, unassign buffer smc_ret=%d\n", __func__,
		       smc_ret);
		return -EINVAL;
	}

	/* free the pmm_msg_pages */
	pmm_page = buffer->ssheap->pmm_page;
	pmm_msg_list = &buffer->ssheap->pmm_msg_list;
	list_for_each_entry_safe(pmm_page, tmp_page, pmm_msg_list, lru) {
		uint32_t *pmm_msg = page_address(pmm_page);
		uint32_t idx_2m, idx, offset;
		uint32_t entry_num = page_size(pmm_page)/sizeof(uint32_t);

		// pr_debug("list_for_each_entry: entry_num%#x\n", entry_num);
		for (i = 0; i < entry_num && pmm_msg[i]; i++) {
			idx_2m = (pmm_msg[i] & 0xffffff) >> 9; // 2MB -> 21bit = 12bit(page) + 9bit
			idx = idx_2m / 32; // bitmap layout: 0 ~ 31bit, one bit -> one 2MB block
			offset = idx_2m % 32; // offset is the 2MB block which this page included.
			bitmap[idx] |= (1 << offset);
			// pr_debug("bitmap[%#x]:%#x, offset:%#x\n", idx, bitmap[idx], offset);
			++page_count;
		}
		memset(page_address(pmm_page), 0, page_size(pmm_page));
		__free_pages(pmm_page, get_order(PAGE_SIZE));
	}
	kfree(buffer->ssheap);

	/* Zero the buffer pages before adding back to the pool */
	/* clean buffer in el2 */

	table = &buffer->sg_table;
	for_each_sgtable_sg(table, sg, i) {
		struct page *page;

		if (!sg) {
			pr_info("%s err, sg null at %d\n", __func__, i);
			return -EINVAL;
		}
		page = sg_page(sg);

		for (j = 0; j < SEC_NUM_ORDERS; j++) {
			if (compound_order(page) == sec_orders[j])
				break;
		}
		if (j < SEC_NUM_ORDERS)
			dmabuf_page_pool_free(sec_pools[j], page);
		else
			pr_err("%s error: order %u\n", __func__,
			       compound_order(page));
	}

	atomic64_sub(1, &sec_heap->total_num);
	if (atomic64_sub_return(buffer->len, &sec_heap->total_size) < 0)
		pr_warn("%s, total memory overflow, %#llx!!\n", __func__,
			atomic64_read(&sec_heap->total_size));

	/* Defragment once the total size of sec heap is zero */
	if (atomic64_read(&sec_heap->total_size) == 0 ||
		is_pre_alloc_total_num(sec_heap)) {
		pr_info("%s: page count:%d, merge the cpu and infra-mpu pgtbl\n",
				__func__, page_count);
#if (DEBUG_BITMAP == 1)
		pr_debug("bitmap:\n");
		for (int i = 0; i < 1024; ++i) {
			if (bitmap[i] != 0)
				pr_debug("%#4x: %#32x ", i, bitmap[i]);
		}
#endif
		ret = pmm_defragment(sec_heap);
		if (ret)
			return ret;
	}

	pr_debug("%s done, [%s] size:%#x, total_size:%#llx\n", __func__,
		 dma_heap_get_name(buffer->heap), buffer->len,
		 atomic64_read(&sec_heap->total_size));
	return 0;
}

void tmem_page_free(struct dma_buf *dmabuf)
{
	int ret = -EINVAL;
	struct secure_heap_page *sec_heap;
	struct mtk_sec_heap_buffer *buffer = NULL;

	dmabuf_release_check(dmabuf);

	buffer = dmabuf->priv;
	sec_heap = sec_heap_page_get(buffer->heap);
	if (!sec_heap) {
		pr_err("%s, %s can not find secure heap!!\n", __func__,
		       buffer->heap ? dma_heap_get_name(buffer->heap) :
				      "null ptr");
		return;
	}

	/* remove all domains' sgtable */
	free_iova_cache(buffer, NULL, 0);

	trusted_mem_page_based_free(sec_heap->tmem_type, buffer->sec_handle);

	if (tmem_api_ver() == 2)
		ret = page_free_v2(sec_heap, buffer);
	else
		ret = page_free(sec_heap, buffer);

	if (ret) {
		pr_err("%s fail, heap:%u\n", __func__, sec_heap->heap_type);
		return;
	}

	sg_free_table(&buffer->sg_table);
	kfree(buffer);
}

/* TMEM region-based functions */

/* region base size is 4K alignment */
TMEM_PRIV int region_alloc(struct secure_heap_region *sec_heap,
		struct mtk_sec_heap_buffer *buffer, ulong req_sz,
		bool aligned)
{
	int ret;
	u64 sec_handle = 0;
	u32 refcount = 0; /* tmem refcount */
	u32 alignment = aligned ? SZ_1M : 0;
	uint64_t phy_addr = 0;
	struct sg_table *table;

	if (buffer->len > UINT_MAX) {
		pr_err("%s error. len more than UINT_MAX\n", __func__);
		return -EINVAL;
	}
	ret = trusted_mem_api_alloc(sec_heap->tmem_type, alignment,
			&buffer->len, &refcount, &sec_handle,
			(uint8_t *)dma_heap_get_name(buffer->heap),
			0, NULL);
	if (ret == -ENOMEM) {
		pr_err("%s error: security out of memory!! heap:%s\n", __func__,
		       dma_heap_get_name(buffer->heap));
		return -ENOMEM;
	}
	if (!sec_handle) {
		pr_err("%s alloc security memory failed, req_size:%#lx, total_size %#llx\n",
		       __func__, req_sz, atomic64_read(&sec_heap->total_size));
		return -ENOMEM;
	}

	table = &buffer->sg_table;
	/* region base PA is continuous, so nent = 1 */
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret) {
		/* free buffer */
		pr_err("%s#%d Error. Allocate mem failed.\n", __func__,
		       __LINE__);
		goto free_buffer;
	}
#if IS_ENABLED(CONFIG_ARM_FFA_TRANSPORT)
	if (trusted_mem_is_ffa_enabled()) {
		ret = trusted_mem_ffa_query_pa(&sec_handle, &phy_addr);
	} else {
		ret = trusted_mem_api_query_pa(sec_heap->tmem_type, 0, buffer->len,
				       &refcount, &sec_handle,
				       (u8 *)dma_heap_get_name(buffer->heap), 0,
				       0, &phy_addr);
	}
#else
	ret = trusted_mem_api_query_pa(sec_heap->tmem_type, 0, buffer->len,
				       &refcount, &sec_handle,
				       (u8 *)dma_heap_get_name(buffer->heap), 0,
				       0, &phy_addr);
#endif /* IS_ENABLED(CONFIG_ARM_FFA_TRANSPORT) */
	if (ret) {
		/* free buffer */
		pr_err("%s#%d Error. query pa failed.\n", __func__, __LINE__);
		goto free_buffer_struct;
	}
	sg_set_page(table->sgl, phys_to_page(phy_addr), buffer->len, 0);
	/* store seucre handle */
	buffer->sec_handle = sec_handle;

	ret = fill_heap_sgtable(sec_heap, buffer);
	if (ret) {
		pr_err("%s#%d Error. fill_heap_sgtable failed.\n", __func__,
		       __LINE__);
		goto free_buffer_struct;
	}

	atomic64_add(buffer->len, &sec_heap->total_size);

	pr_debug(
		"%s done: [%s], req_size:%#lx, align_sz:%#x, handle:%#llx, pa:%#llx, total_sz:%#llx\n",
		__func__, dma_heap_get_name(buffer->heap), req_sz, buffer->len,
		buffer->sec_handle, phy_addr,
		atomic64_read(&sec_heap->total_size));

	return 0;

free_buffer_struct:
	sg_free_table(table);
free_buffer:
	/* free secure handle */
	trusted_mem_api_unref(sec_heap->tmem_type, sec_handle,
			      (uint8_t *)dma_heap_get_name(buffer->heap), 0,
			      NULL);

	return ret;
}

struct dma_buf *tmem_region_alloc(struct dma_heap *heap, ulong len,
		u32 fd_flags, u64 heap_flags)
{
	int ret = -ENOMEM;
	struct dma_buf *dmabuf;
	struct mtk_sec_heap_buffer *buffer;
	bool aligned = region_heap_is_aligned(heap);
	struct secure_heap_region *sec_heap = sec_heap_region_get(heap);

	if (!sec_heap) {
		pr_err("%s, can not find secure heap(%s)!!\n", __func__,
		       (heap ? dma_heap_get_name(heap) : "null ptr"));
		return ERR_PTR(-EINVAL);
	}

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	buffer->len = len;
	buffer->heap = heap;
	buffer->show = sec_buf_priv_dump;
	buffer->gid = -1;

	ret = region_alloc(sec_heap, buffer, len, aligned);
	if (ret)
		goto free_buffer;

	dmabuf = alloc_dmabuf(heap, buffer, &sec_buf_region_ops, fd_flags);
	if (IS_ERR(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		pr_err("%s alloc_dmabuf fail\n", __func__);
		goto free_tmem;
	}
	init_buffer_info(heap, buffer);

	return dmabuf;

free_tmem:
	trusted_mem_api_unref(sec_heap->tmem_type, buffer->sec_handle,
			      (uint8_t *)dma_heap_get_name(buffer->heap), 0,
			      NULL);
	sg_free_table(&buffer->sg_table);
free_buffer:
	kfree(buffer);
	return ERR_PTR(ret);
}

TMEM_PRIV int region_free(struct secure_heap_region *sec_heap,
		struct mtk_sec_heap_buffer *buffer)
{
	struct device *iommu_dev;
	u64 sec_handle = 0;
	int ret = 0;

	iommu_dev = sec_heap->iommu_dev;
	/* remove all domains' sgtable */
	free_iova_cache(buffer, iommu_dev, 1);

	sec_handle = buffer->sec_handle;
	ret = trusted_mem_api_unref(sec_heap->tmem_type, sec_handle,
				    (uint8_t *)dma_heap_get_name(buffer->heap),
				    0, NULL);
	if (ret) {
		pr_err("%s error, trusted_mem_api_unref failed, heap:%s, ret:%d\n",
		       __func__, dma_heap_get_name(buffer->heap), ret);
		return ret;
	}

	if (atomic64_sub_return(buffer->len, &sec_heap->total_size) < 0)
		pr_warn("%s warn!, total memory overflow, 0x%llx!!\n", __func__,
			atomic64_read(&sec_heap->total_size));

	if (!atomic64_read(&sec_heap->total_size)) {
		mutex_lock(&sec_heap->heap_lock);
		if (sec_heap->heap_filled && sec_heap->region_table) {
			if (sec_heap->heap_mapped) {
				dma_unmap_sgtable(iommu_dev,
						  sec_heap->region_table,
						  DMA_BIDIRECTIONAL,
						  DMA_ATTR_SKIP_CPU_SYNC);
				sec_heap->heap_mapped = false;
			}
			sg_free_table(sec_heap->region_table);
			kfree(sec_heap->region_table);
			sec_heap->region_table = NULL;
			sec_heap->heap_filled = false;
			pr_debug(
				"%s: all secure memory already free, unmap heap_region iova\n",
				__func__);
		}
		mutex_unlock(&sec_heap->heap_lock);
	}

	pr_debug("%s done, [%s] size:%#x, total_size:%#llx\n", __func__,
		 dma_heap_get_name(buffer->heap), buffer->len,
		 atomic64_read(&sec_heap->total_size));
	return ret;
}

void tmem_region_free(struct dma_buf *dmabuf)
{
	int ret = -EINVAL;
	struct secure_heap_region *sec_heap;
	struct mtk_sec_heap_buffer *buffer = NULL;

	dmabuf_release_check(dmabuf);

	buffer = dmabuf->priv;
	sec_heap = sec_heap_region_get(buffer->heap);
	if (!sec_heap) {
		pr_err("%s, %s can not find secure heap!!\n", __func__,
		       buffer->heap ? dma_heap_get_name(buffer->heap) :
				      "null ptr");
		return;
	}

	ret = region_free(sec_heap, buffer);
	if (ret) {
		pr_err("%s fail, heap:%u\n", __func__, sec_heap->heap_type);
		return;
	}
	sg_free_table(&buffer->sg_table);
	kfree(buffer);
}

MODULE_LICENSE("GPL v2");

