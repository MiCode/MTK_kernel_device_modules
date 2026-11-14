// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/genalloc.h>
#include <linux/log2.h>
#include <linux/kernel.h>
#include <uapi/linux/dma-buf.h>
#include "mdw.h"
#include "mdw_cmn.h"
#include "mdw_mem_pool.h"
#include "mdw_trace.h"
#include "apummu_export.h"

#define mdw_mem_pool_show(m) \
	mdw_mem_debug("map pool(0x%llx/0x%llx)(0x%llx)(%p/0x%llx/%llu)(%d)(%u)\n", \
		(uint64_t)m->mpriv, (uint64_t)m, m->flags, m->vaddr, m->device_va, m->size, \
		kref_read(&m->ref), task_pid_nr(current))

/* allocate a memory chunk, and add it to pool */
static int mdw_mem_pool_chunk_add(struct mdw_mem_pool *pool, uint32_t size)
{
	int ret = 0, len = 0;
	struct mdw_mem_pool_chunk *chunk;
	char buf_name[DMA_BUF_NAME_LEN];

	mdw_trace_begin("apumdw:pool_chunk_add|size:%u", size);

	/* prepare buffer name */
	memset(buf_name, 0, sizeof(buf_name));
	len = snprintf(buf_name, sizeof(buf_name), "APU_CMDBUF_POOL:%u/%u",
		task_pid_nr(current), task_tgid_nr(current));
	if (len >= sizeof(buf_name) || len < 0) {
		mdw_drv_err("snprintf fail\n");
		ret = -EINVAL;
		goto out;
	}

	/* alloc */
	chunk = kzalloc(sizeof(*chunk), GFP_KERNEL);
	if (!chunk) {
		ret = -ENOMEM;
		goto out;
	}

	chunk->mem = mdw_mem_alloc(pool->mpriv, pool->mem_type,
		size, pool->align, pool->flags, buf_name);
	if (!chunk->mem) {
		mdw_drv_err("mem_pool(0x%llx) create allocate fail, size: %d\n",
			(uint64_t) pool->mpriv, size);
		ret = -ENOMEM;
		goto err_alloc;
	}

	chunk->map = mdw_mem_create_map(pool->mpriv, chunk->mem->dbuf, pool->buf_type, pool->flags, true);
	if (!chunk->map) {
		mdw_drv_err("mem_pool(0x%llx) create map fail, size: %d\n",
			(uint64_t)pool->mpriv, size);
		ret = -ENOMEM;
		goto err_map;
	}

	chunk->mpriv = pool->mpriv;
	chunk->map->pool = pool;

	mdw_mem_debug("mpriv: 0x%llx, pool: 0x%llx, new chunk mem: 0x%llx, kva: 0x%llx, iova: 0x%llx, size: %d",
		(uint64_t)pool->mpriv, (uint64_t)pool, (uint64_t)chunk,
		(uint64_t)chunk->map->vaddr, (uint64_t)chunk->map->device_va, size);

	ret = gen_pool_add_owner(pool->gp, (unsigned long)chunk->map->vaddr,
		(phys_addr_t)chunk->map->device_va, chunk->map->size, -1, chunk);

	if (ret) {
		mdw_drv_err("mem_pool(0x%llx) gen_pool add fail: %d\n",
			(uint64_t) pool->mpriv, ret);
		goto err_add;
	}
	list_add_tail(&chunk->pool_node, &pool->m_chunks);
	mdw_mem_debug("add chunk: pool: 0x%llx, chunk: 0x%llx, size: %d",
		(uint64_t)chunk->map->pool, (uint64_t)chunk, size);

	goto out;

err_add:
	mdw_mem_delete_map(pool->mpriv, chunk->map);
err_map:
	mdw_mem_free(pool->mpriv, chunk->mem);
err_alloc:
	kfree(chunk);
out:
	mdw_trace_end();

	return ret;
}

/* removes a memory chunk from pool, and free it */
static void mdw_mem_pool_chunk_del(struct mdw_mem_pool_chunk *chunk)
{
	mdw_trace_begin("apumdw:pool_chunk_del|size:%llu", chunk->mem->size);
	list_del(&chunk->pool_node);
	mdw_mem_debug("free chunk: pool: 0x%llx, chunk: 0x%llx",
		(uint64_t)chunk->map->pool, (uint64_t)chunk);
	mdw_mem_delete_map(chunk->mpriv, chunk->map);
	mdw_mem_free(chunk->mpriv, chunk->mem);
	kfree(chunk);
	mdw_trace_end();
}

/* create memory pool */
int mdw_mem_pool_create(struct mdw_fpriv *mpriv, struct mdw_mem_pool *pool,
	enum mdw_mem_type mem_type, enum mdw_buf_type buf_type, uint32_t size,
	uint32_t align, uint64_t flags)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(mpriv) || IS_ERR_OR_NULL(pool))
		return -EINVAL;

	/* TODO: cacheable command buffer */
	if (flags & F_MDW_MEM_CACHEABLE) {
		mdw_drv_err("cacheable pool is unsupported: mpriv: 0x%llx",
			(uint64_t)mpriv);
		return -EINVAL;
	}

	mdw_trace_begin("apumdw:pool_create|size:%u align:%u", size, align);

	pool->mpriv = mpriv;
	pool->flags = flags;
	pool->mem_type = mem_type;
	pool->buf_type = buf_type;
	pool->align = align;
	pool->chunk_size = size;
	mutex_init(&pool->m_mtx);
	kref_init(&pool->m_ref);
	INIT_LIST_HEAD(&pool->m_chunks);
	INIT_LIST_HEAD(&pool->m_list);
	pool->gp = gen_pool_create(PAGE_SHIFT, -1 /* nid */);

	if (IS_ERR_OR_NULL(pool->gp)) {
		ret = PTR_ERR(pool->gp);
		mdw_drv_err("mem_pool(0x%llx) gen_pool init fail: %d\n",
			(uint64_t) mpriv, ret);
		goto out;
	}

	ret = mdw_mem_pool_chunk_add(pool, size);
	if (ret)
		goto err_add;

	mdw_mem_debug("success, mpriv: 0x%llx, pool: 0x%llx",
		(uint64_t)mpriv, (uint64_t)pool);

	goto out;

err_add:
	if (pool->gp)
		gen_pool_destroy(pool->gp);

out:
	mdw_trace_end();

	return ret;
}

/* the release function when pool reference count reaches zero */
static void mdw_mem_pool_release(struct kref *ref)
{
	struct mdw_mem_pool *pool;
	struct mdw_mem_pool_chunk *chunk, *chunk_tmp;
	struct mdw_mem_map *map, *map_tmp;

	pool = container_of(ref, struct mdw_mem_pool, m_ref);
	if (IS_ERR_OR_NULL(pool->mpriv))
		return;

	mdw_trace_begin("apumdw:pool_release|size:%u align:%u",
		pool->chunk_size, pool->align);

	/* release all allocated memories */
	list_for_each_entry_safe(map, map_tmp, &pool->m_list, pool_node) {
		/* This should not happen, when m_ref is zero */
		list_del(&map->pool_node);
		mdw_mem_debug("free mem: pool: 0x%llx, mem: 0x%llx",
			(uint64_t)pool, (uint64_t)map);
		gen_pool_free(pool->gp, (unsigned long)map->vaddr, map->size);
		kfree(map);
	}

	/* destroy gen pool */
	gen_pool_destroy(pool->gp);

	/* release all chunks */
	list_for_each_entry_safe(chunk, chunk_tmp, &pool->m_chunks, pool_node) {
		mdw_mem_pool_chunk_del(chunk);
	}

	mdw_trace_end();
}

/* destroy memory pool */
void mdw_mem_pool_destroy(struct mdw_mem_pool *pool)
{
	mdw_mem_debug("pool: 0x%llx", (uint64_t)pool);
	mutex_lock(&pool->m_mtx);
	kref_put(&pool->m_ref, mdw_mem_pool_release);
	mutex_unlock(&pool->m_mtx);
}

/* frees a mdw_mem struct */
static void mdw_mem_pool_ent_release(struct mdw_mem_map *m)
{
	mdw_mem_pool_show(m);
	kfree(m);
}

/* allocates a mdw_mem struct */
static struct mdw_mem_map *mdw_mem_pool_ent_create(struct mdw_mem_pool *pool)
{
	struct mdw_mem_map *m;

	m = kzalloc(sizeof(*m), GFP_KERNEL);
	if (!m)
		return NULL;

	m->pool = pool;
	m->mpriv = pool->mpriv;
	mdw_mem_pool_show(m);

	return m;
}

/* alloc memory from pool, and alloc/add its mdw_mem struct to pool->list */
struct mdw_mem_map *mdw_mem_pool_alloc(struct mdw_mem_pool *pool, uint64_t size,
	uint32_t align)
{
	struct mdw_mem_map *m = NULL;
	dma_addr_t dma;
	bool retried = false;
	unsigned long chunk_size;
	int ret = 0;

	if (!pool || !size)
		return NULL;

	mdw_trace_begin("apumdw:pool_alloc|size:%llu align:%u",
		size, align);

	/* create mem struct */
	m = mdw_mem_pool_ent_create(pool);
	if (!m)
		goto out;

	/* set param */
	m->size = size;

	/* alloc mem */
	mutex_lock(&pool->m_mtx);
retry:

	m->vaddr = gen_pool_dma_alloc_align(pool->gp, size, &dma, align);
	if (m->vaddr) {
		list_add_tail(&m->pool_node, &pool->m_list);
		kref_get(&pool->m_ref);
	} else {
		/* try to add a new chunk to pool, and retry again */
		chunk_size = max(PAGE_SIZE, __roundup_pow_of_two(size));
		ret = mdw_mem_pool_chunk_add(pool, chunk_size);
		if (!ret && !retried) {
			retried = true;
			goto retry;
		}
	}
	mutex_unlock(&pool->m_mtx);

	if (!m->vaddr) {
		mdw_drv_err("alloc (%pK,%d,%llu,%d) fail\n",
			pool, pool->mem_type, size, align);
		goto err_alloc;
	}

	m->device_va = dma;
	m->size = size;

	/* zero out the allocated buffer */
	memset(m->vaddr, 0, size);

	mdw_mem_pool_show(m);
	goto out;

err_alloc:
	kfree(m);
	m = NULL;
out:
	if (m) {
		mdw_mem_debug("pool: 0x%llx, map: 0x%llx, size: %llu, align: %d, kva: 0x%llx, iova: 0x%llx",
			(uint64_t)pool, (uint64_t)m, size, align,
			(uint64_t)m->vaddr, (uint64_t)m->device_va);
	}

	mdw_trace_end();
	return m;
}

/* free memory from pool, and free/delete its mdw_mem struct from pool->list */
void mdw_mem_pool_free(struct mdw_mem_map *m)
{
	struct mdw_mem_pool *pool;
	uint32_t size = 0;

	if (!m)
		return;

	mdw_mem_pool_show(m);
	pool = m->pool;

	if (!pool || !pool->gp)
		return;

	size = m->size;

	mdw_trace_begin("apumdw:pool_free|size:%u", size);

	mdw_mem_debug("pool: 0x%llx, map: 0x%llx, size: %llu, kva: 0x%llx, iova: 0x%llx",
		(uint64_t)pool, (uint64_t)m, m->size,
		(uint64_t)m->vaddr, (uint64_t)m->device_va);


	mutex_lock(&pool->m_mtx);
	list_del(&m->pool_node);
	gen_pool_free(m->pool->gp, (unsigned long)m->vaddr, m->size);
	kref_put(&pool->m_ref, mdw_mem_pool_release);
	mutex_unlock(&pool->m_mtx);

	mdw_mem_pool_ent_release(m);

	mdw_trace_end();
}

/* flush a memory, do nothing, if it's non-cacheable */
int mdw_mem_pool_flush(struct mdw_mem_map *m)
{
	if (!m)
		return 0;

	if (m->flags ^ F_MDW_MEM_CACHEABLE)
		return 0;

	mdw_trace_begin("apumdw:pool_flush|size:%llu", m->size);
	/* TODO: cacheable command buffer */
	mdw_drv_err("cacheable buffer: pool: 0x%llx, map: 0x%llx",
		(uint64_t)m->pool, (uint64_t)m);
	mdw_trace_end();

	return -EINVAL;
}

/* invalidate a memory, do nothing, if it's non-cacheable */
int mdw_mem_pool_invalidate(struct mdw_mem_map *m)
{
	if (!m)
		return 0;

	if (m->flags ^ F_MDW_MEM_CACHEABLE)
		return 0;

	mdw_trace_begin("apumdw:pool_invalidate|size:%llu", m->size);
	/* TODO: cacheable command buffer */
	mdw_drv_err("cacheable buffer: pool: 0x%llx, map: 0x%llx",
		(uint64_t)m->pool, (uint64_t)m);
	mdw_trace_end();

	return -EINVAL;
}

