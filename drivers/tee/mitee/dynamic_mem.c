/*
 * Copyright (C) 2015 Google, Inc.
 * Copyright (C) 2024 XiaoMi, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/scatterlist.h>
#include <linux/mm_types.h>
#include <linux/gfp.h>
#include "dynamic_mem.h"

static struct mitee_dynamic_mem_queue memory_queue;

struct mitee_sgl_page {
	struct page *pages;
	uint32_t order;
};

struct mitee_page_frag_entry {
	struct mitee_sgl_page sgl_pages;
	struct list_head node;
};

struct mitee_page_fragment {
	uint32_t n_entries;
	struct list_head page_node;
};

static struct mitee_dynamic_mem_queue *get_mitee_dynamic_mem_queue(void)
{
	return &memory_queue;
}

int mitee_dynamic_mem_add_node(uint64_t mem_handle, struct sg_table *sgt, uint32_t size)
{
	struct mitee_dynamic_mem_queue *queue = get_mitee_dynamic_mem_queue();
	struct mem_desc *desc = NULL;

	desc = kmalloc(sizeof(*desc), GFP_KERNEL);
	if (!desc) {
		pr_err("mitee dynamic mem: failed to alloc memory for new dynamic node\n");
		return -ENOMEM;
	}

	desc->sgt = sgt;
	desc->global_id = mem_handle;
	desc->mem_size = size;
	mutex_lock(&queue->mem_mut);
	list_add_tail(&desc->node, &queue->mem_node);
	mutex_unlock(&queue->mem_mut);

	return 0;
}

void mitee_dynamic_mem_remove_node(uint64_t mem_handle)
{
	struct mitee_dynamic_mem_queue *queue = get_mitee_dynamic_mem_queue();
	struct mem_desc *desc = NULL;
	struct mem_desc *tmp = NULL;

	mutex_lock(&queue->mem_mut);
	list_for_each_entry_safe (desc, tmp, &queue->mem_node, node) {
		if (desc->global_id == mem_handle) {
			list_del(&desc->node);
			break;
		}
	}
	mutex_unlock(&queue->mem_mut);

	if (!desc) {
		pr_err("mitee dynamic mem: unknown pagelist 0x%llx\n", mem_handle);
	}

	kfree(desc);
	return;
}

struct mem_desc *mitee_dynamic_mem_find_node(uint64_t mem_handle)
{
	struct mitee_dynamic_mem_queue *queue = get_mitee_dynamic_mem_queue();
	struct mem_desc *desc = NULL;
	struct mem_desc *tmp = NULL;

	mutex_lock(&queue->mem_mut);
	list_for_each_entry_safe (desc, tmp, &queue->mem_node, node) {
		if (desc->global_id == mem_handle) {
			break;
		}
	}
	mutex_unlock(&queue->mem_mut);

	return desc;
}


void mitee_free_memory_sgt(uint32_t mem_size, struct sg_table *sgt) {
	struct scatterlist *sg = NULL;
	int i;
	uint32_t total_size = 0;
	for_each_sg(sgt->sgl, sg, sgt->orig_nents, i) {
		if (sg_page(sg)) {
			__free_pages(sg_page(sg), get_order(sg->length));
			total_size += sg->length;
		}
	}

	if (mem_size != total_size) {
		pr_err("mitee dynamic mem: error, sgt(0x%x) not compatible with target(0x%x)\n", total_size, mem_size);
	}

	sg_free_table(sgt);
	kfree(sgt);
}

int mitee_alloc_memory_sgt(uint32_t mem_size, struct sg_table **out_sgt)
{
	int index, rc;
	uint32_t left_size = mem_size;
	struct page *pages = NULL;
	struct mitee_page_fragment page_frag= {.n_entries = 0};
	struct mitee_page_frag_entry *tmp_page_frag_entry = NULL;
	struct mitee_page_frag_entry *tmp = NULL;
	struct sg_table *sgt = NULL;
	struct scatterlist *sg = NULL;
	INIT_LIST_HEAD(&page_frag.page_node);

	for (index = MAX_ORDER - 1; index >= 0; index--) {
		while (left_size >= (1 << index) * PAGE_SIZE) {
			pages = alloc_pages(GFP_KERNEL, index);
			if (!pages) {
				// go through next order
				break;
			}

			tmp_page_frag_entry = (struct mitee_page_frag_entry *)kvmalloc(sizeof(*tmp_page_frag_entry), GFP_KERNEL);
			if (!tmp_page_frag_entry) {
				pr_err("mitee dynamic mem: failed to alloc page_frag_entry\n");
				rc = -ENOMEM;
				goto out_free;
			}

			tmp_page_frag_entry->sgl_pages.pages = pages;
			tmp_page_frag_entry->sgl_pages.order = index;
			list_add_tail(&tmp_page_frag_entry->node, &page_frag.page_node);
			page_frag.n_entries++;
			left_size -= ((1 << index) * PAGE_SIZE);
		}
	}

	if (left_size > 0) {
		pr_err("mitee dynamic mem: system without enough memory, still need 0x%x\n", left_size);
		rc = -ENOMEM;
		goto out_free;
	}

	// transfer sgl_page_fragment to sgl_page_table
	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt) {
		pr_err("mitee dynamic mem: failed to alloc memory for sg_table\n");
		rc = -ENOMEM;
		goto out_free;
	}

	rc = sg_alloc_table(sgt, page_frag.n_entries, GFP_KERNEL);
	if (rc) {
		pr_err("mitee dynamic mem: failed to alloc sg_table with %d", rc);
		goto out_free;
	}

	sg = sgt->sgl;
	list_for_each_entry_safe (tmp_page_frag_entry, tmp, &page_frag.page_node, node) {
		sg_set_page(sg, tmp_page_frag_entry->sgl_pages.pages,
			     (1 << tmp_page_frag_entry->sgl_pages.order) * PAGE_SIZE, 0);
		sg = sg_next(sg);
		list_del(&tmp_page_frag_entry->node);
		kfree(tmp_page_frag_entry);
	}

	if (sg != NULL) {
		pr_err("mitee dynamic mem: error, sgt not compatible with page_fragment\n");
	}

	*out_sgt = sgt;
	return 0;

out_free:
	list_for_each_entry_safe (tmp_page_frag_entry, tmp, &page_frag.page_node, node) {
		list_del(&tmp_page_frag_entry->node);
		__free_pages(tmp_page_frag_entry->sgl_pages.pages, tmp_page_frag_entry->sgl_pages.order);
		kfree(tmp_page_frag_entry);
	}
	return rc;
}

void mitee_dynamic_mem_init(void)
{
	struct mitee_dynamic_mem_queue *queue = get_mitee_dynamic_mem_queue();
	mutex_init(&queue->mem_mut);
	INIT_LIST_HEAD(&queue->mem_node);
}

void mitee_dynamic_mem_deinit(void)
{
	struct mitee_dynamic_mem_queue *queue = get_mitee_dynamic_mem_queue();
	mutex_destroy(&queue->mem_mut);
}
