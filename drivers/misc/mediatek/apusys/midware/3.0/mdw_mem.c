// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/dma-buf.h>

#include "apusys_device.h"
#include "mdw_cmn.h"
#include "mdw_mem_pool.h"
#include "mdw_trace.h"
#include "apu_sysmem.h"
#include "mdw_cmd.h"//for cmd invoke

#define mdw_mem_show(m) \
	mdw_mem_debug("mem(0x%llx/0x%llx/0x%llx)(%d)(%llu/%llu/0x%llx)(%p)(%d)(%u)\n", \
		(uint64_t)m->mpriv, (uint64_t)m, (uint64_t)APU_SYSMEM_GET_DMABUF(m->sysbuf), \
		m->mem_type, m->size, m->align, m->flags, m->vaddr, \
		kref_read(&m->ref), task_pid_nr(current))

#define mdw_map_show(m) \
	mdw_mem_debug("map(0x%llx/0x%llx/0x%llx)(%d/0x%llx)(%p/0x%llx/%llu)(%d)(%u)\n", \
		(uint64_t)m->mpriv, (uint64_t)m, (uint64_t)APU_SYSMEM_GET_DMABUF(m->sysmap), \
		m->buf_type, m->flags, m->vaddr, m->device_va, m->size, \
		kref_read(&m->ref), task_pid_nr(current))

static struct mdw_mem_map *mdw_mem_get_map_by_handle(struct mdw_fpriv *mpriv, int handle,
	uint64_t device_va)
{
	struct dma_buf *dbuf = NULL;
	struct mdw_mem_map *map = NULL;

	dbuf = dma_buf_get(handle);

	if (IS_ERR_OR_NULL(dbuf)) {
		mdw_drv_err("get dma_buf handle(%d) fail\n", handle);
		goto out;
	}

	hash_for_each_possible(mpriv->u_map_hash, map, fpriv_node, (uint64_t)dbuf) {
		if (map->dbuf == dbuf && map->device_va == device_va) {
			mdw_map_show(map);
			goto put_dbuf;
		} else {
			mdw_mem_debug("found same dbuf but diff params: dbuf(0x%llx/0x%llx) dva(0x%llx/0x%llx) find next\n",
				(uint64_t)dbuf, (uint64_t)APU_SYSMEM_GET_DMABUF(map->sysmap),
				device_va, map->device_va);
		}
	}

	mdw_drv_err("can't find map by handle(%d/0x%llx)\n", handle, device_va);
	map = NULL;

put_dbuf:
	dma_buf_put(dbuf);
out:
	return map;
}

struct mdw_mem *mdw_mem_get_mem_by_handle(struct mdw_fpriv *mpriv, int handle)
{
	struct dma_buf *dbuf = NULL;
	struct mdw_mem *m = NULL;

	dbuf = dma_buf_get(handle);
	if (IS_ERR_OR_NULL(dbuf)) {
		mdw_drv_err("get dma_buf handle(%d) fail\n", handle);
		goto out;
	}

	hash_for_each_possible(mpriv->u_mem_hash, m, fpriv_node, (uint64_t)dbuf) {
		if (m->dbuf == dbuf) {
			mdw_mem_show(m);
			goto put_dbuf;
		}
	}

	mdw_drv_err("can't find mem by handle(%d)\n", handle);
	m = NULL;

put_dbuf:
	dma_buf_put(dbuf);
out:
	return m;
}

struct mdw_mem *mdw_mem_get_mem_by_device_dbuf(struct dma_buf *dbuf)
{
	struct mdw_mem *m = NULL;

	mdw_mem_debug("dbuf 0x%llx\n", (uint64_t)dbuf);

	hash_for_each_possible(mdw_dev->u_mem_hash, m, device_node, (uint64_t)dbuf) {
		if (m->dbuf == dbuf) {
			mdw_mem_show(m);
			return m;
		}
	}

	return NULL;
}

static inline enum apu_sysmem_mem_type mdw_mem_type2sysmem(enum mdw_mem_type mem_type)
{
	enum apu_sysmem_mem_type sysmem_type = APU_SYSMEM_TYPE_NONE;

	switch (mem_type) {
	case MDW_MEM_TYPE_MAIN:
		sysmem_type = APU_SYSMEM_TYPE_DRAM;
		break;

	case MDW_MEM_TYPE_SYSTEM_ISP:
		sysmem_type = APU_SYSMEM_TYPE_SYSTEM_ISP;
		break;

	case MDW_MEM_TYPE_SYSTEM_APU:
		sysmem_type = APU_SYSMEM_TYPE_SYSTEM_NPU;
		break;

	case MDW_MEM_TYPE_VLM:
	case MDW_MEM_TYPE_LOCAL:
	case MDW_MEM_TYPE_SYSTEM:
	default:
		break;
	}

	return sysmem_type;
}

static inline uint64_t mdw_mem_flag2sysmem(uint64_t flags)
{
	uint64_t sysmem_flags = 0;

	if (flags & F_MDW_MEM_CACHEABLE)
		sysmem_flags |= F_APU_SYSMEM_FLAG_CACHEABLE;

	return sysmem_flags;
}

static inline uint64_t mdw_map_bitmask2sysmem(uint64_t flags)
{
	uint64_t map_bitmask = 0;

	/* addr part */
	if ((flags & F_MDW_MEM_HIGHADDR) && !(flags & F_MDW_MEM_32BIT)) {
		map_bitmask |= F_APU_SYSMEM_MAP_TYPE_DEVICE_LONGVA;
	} else {
		map_bitmask |= F_APU_SYSMEM_MAP_TYPE_DEVICE_VA;
	}

	/* attribute */
	if (flags & F_MDW_MEM_ATTR_SLC_DC)
		map_bitmask |= F_APU_SYSMEM_MAP_TYPE_SLC_DC;

	return map_bitmask;
}

static void mdw_mem_release(struct kref *ref)
{
	struct mdw_mem *m =
			container_of(ref, struct mdw_mem, ref);
	int ret = 0;

	mdw_mem_show(m);

	ret = m->mpriv->mem_allocator->free(m->mpriv->mem_allocator, m->sysbuf);
	if (ret) {
		mdw_drv_err("delete mem failed(%d) dbuf(0x%llx) size(%llu)\n", ret, (uint64_t)m->dbuf, m->size);
		mdw_exception("delete mem failed\n");
	}

	/* remove from dev hashtable */
	mutex_lock(&mdw_dev->mctl_mtx);
	hash_del(&m->device_node);
	mutex_unlock(&mdw_dev->mctl_mtx);

	kfree(m);
}

static void mdw_mem_get(struct mdw_mem *m)
{
	kref_get(&m->ref);
	mdw_mem_show(m);
}

static void mdw_mem_put(struct mdw_mem *m)
{
	mdw_mem_show(m);
	kref_put(&m->ref, mdw_mem_release);
}

struct mdw_mem *mdw_mem_alloc(struct mdw_fpriv *mpriv, enum mdw_mem_type mem_type,
	uint64_t size, uint32_t align, uint64_t flags, char *name)
{
	struct mdw_mem *m = NULL;
	enum apu_sysmem_mem_type sysmem_type = mdw_mem_type2sysmem(mem_type);

	/* check */
	if (sysmem_type == APU_SYSMEM_TYPE_NONE) {
		mdw_drv_err("invalid mem type(%d)\n", mem_type);
		goto out;
	}

	/* allocate mdw_mem */
	m = kzalloc(sizeof(*m), GFP_KERNEL);
	if (m == NULL) {
		mdw_drv_err("allocate mdw_mem(%s) failed\n", name);
		goto out;
	}

	/* allocate sys mem */
	m->sysbuf = mpriv->mem_allocator->alloc(mpriv->mem_allocator, mdw_mem_type2sysmem(mem_type),
		size, mdw_mem_flag2sysmem(flags), name);
	if (!m->sysbuf) {
		mdw_drv_err("allocate sysmem failed, type(%d) size(%llu) flags(0x%llx) name(%s)\n",
			mem_type, size, flags, name);
		goto free_mem;
	}

	/* init */
	m->mpriv = mpriv;
	m->mem_type = mem_type;
	m->size = size;
	m->align = align;
	m->flags = flags;
	m->vaddr = APU_SYSMEM_GET_KVA(m->sysbuf);
	m->dbuf = APU_SYSMEM_GET_DMABUF(m->sysbuf);
	kref_init(&m->ref);
	m->get = mdw_mem_get;
	m->put = mdw_mem_put;

	mdw_mem_show(m);

	goto out;

free_mem:
	kfree(m);
	m = NULL;
out:
	return m;
}

int mdw_mem_free(struct mdw_fpriv *mpriv, struct mdw_mem *m)
{
	mdw_mem_show(m);
	m->put(m);

	return 0;
}

static void mdw_map_release(struct kref *ref)
{
	struct mdw_mem_map *map =
			container_of(ref, struct mdw_mem_map, ref);
	int ret = 0;

	mdw_map_show(map);

	/* remove from dev */
	mutex_lock(&mdw_dev->mctl_mtx);
	list_del(&map->device_node);
	mutex_unlock(&mdw_dev->mctl_mtx);

	ret = map->mpriv->mem_allocator->unmap(map->mpriv->mem_allocator, map->sysmap);
	if (ret) {
		mdw_drv_err("unmap failed(%d) dbuf(0x%llx) size(%llu)\n", ret, (uint64_t)map->dbuf, map->size);
		mdw_exception("unmap failed\n");
	}

	kfree(map);
}

static void mdw_map_get(struct mdw_mem_map *map)
{
	kref_get(&map->ref);
	mdw_map_show(map);
}

static void mdw_map_put(struct mdw_mem_map *map)
{
	mdw_map_show(map);
	kref_put(&map->ref, mdw_map_release);
}

struct mdw_mem_map *mdw_mem_create_map(struct mdw_fpriv *mpriv, struct dma_buf *dbuf,
	enum mdw_buf_type buf_type, uint64_t flags, bool share_region)
{
	struct mdw_mem_map *map = NULL;
	struct mdw_mem *m = NULL;
	enum mdw_mem_type mem_type = MDW_MEM_TYPE_MAIN;
	uint64_t map_flags = mdw_map_bitmask2sysmem(flags);

	/* alloc mdw_mem_map */
	map = kzalloc(sizeof(*map), GFP_KERNEL);
	if (!map) {
		mdw_drv_err("allocate map failed\n");
		goto out;
	}

	mutex_lock(&mdw_dev->mctl_mtx);
	m = mdw_mem_get_mem_by_device_dbuf(dbuf);
	if (m)
		mem_type = m->mem_type;
	mutex_unlock(&mdw_dev->mctl_mtx);

	/* append share region flag */
	if (share_region) {
		/* replace with shareva */
		map_flags &= ~(F_APU_SYSMEM_MAP_TYPE_DEVICE_LONGVA | F_APU_SYSMEM_MAP_TYPE_DEVICE_VA);
		map_flags |= F_APU_SYSMEM_MAP_TYPE_DEVICE_SHAREVA;
	}

	/* map */
	map->sysmap = mpriv->mem_allocator->map(mpriv->mem_allocator, dbuf,
		mdw_mem_type2sysmem(mem_type), map_flags);
	if (!map->sysmap) {
		mdw_drv_err("map failed\n");
		goto free_map;
	}

	map->dbuf = dbuf;
	map->mpriv = mpriv;
	map->flags = flags;
	map->buf_type = buf_type;
	map->device_va = map->sysmap->device_va;
	map->mem_type = mem_type;
	map->size = map->sysmap->size; //info from sysmap
	map->vaddr = map->sysmap->vaddr; //info from sysmap
	/* insert to dev */
	mutex_lock(&mdw_dev->mctl_mtx);
	list_add_tail(&map->device_node, &mdw_dev->maps);
	mutex_unlock(&mdw_dev->mctl_mtx);

	kref_init(&map->ref);
	map->get = mdw_map_get;
	map->put = mdw_map_put;

	mdw_map_show(map);

	goto out;

free_map:
	kfree(map);
	map = NULL;
out:
	return map;
}

int mdw_mem_delete_map(struct mdw_fpriv *mpriv, struct mdw_mem_map *map)
{
	mdw_map_show(map);
	map->put(map);

	return 0;
}

int mdw_mem_flush(struct mdw_fpriv *mpriv, struct mdw_mem_map *map)
{
	int ret = 0;

	if (map->pool)
		return mdw_mem_pool_flush(map);

	mdw_trace_begin("apumdw:mem_flush|size:%llu", map->size);
	ret = dma_buf_end_cpu_access(map->dbuf, DMA_TO_DEVICE);
	if (ret) {
		mdw_drv_err("Flush Fail\n");
		ret = -EINVAL;
		goto out;
	}

	mdw_mem_debug("flush(0x%llx/0x%llx/0x%llx) (%p/0x%llx/%llu)\n",
		(uint64_t)map->mpriv, (uint64_t)map, (uint64_t)map->dbuf,
		map->vaddr, map->device_va, map->size);

out:
	mdw_trace_end();
	return ret;
}

void mdw_mem_release_session(struct mdw_fpriv *mpriv)
{
	struct mdw_mem *m = NULL;
	struct mdw_mem_map *map = NULL;
	struct hlist_node *tmp = NULL;
	int i = 0;

	hash_for_each_safe(mpriv->u_map_hash, i, tmp, map, fpriv_node) {
		hash_del(&map->fpriv_node);
		mdw_map_show(map);
		mdw_mem_delete_map(mpriv, map);
	}

	tmp = NULL;
	hash_for_each_safe(mpriv->u_mem_hash, i, tmp, m, fpriv_node) {
		hash_del(&m->fpriv_node);
		mdw_mem_show(m);
		mdw_mem_free(mpriv, m);
	}
}

int mdw_mem_invalidate(struct mdw_fpriv *mpriv, struct mdw_mem_map *map)
{
	int ret = 0;

	if (map->pool)
		return mdw_mem_pool_invalidate(map);

	mdw_trace_begin("apumdw:mem_invalidate|size:%llu", map->size);

	ret = dma_buf_begin_cpu_access(map->dbuf, DMA_FROM_DEVICE);
	if (ret) {
		mdw_drv_err("Invalidate Fail\n");
		ret = -EINVAL;
		goto out;
	}

	mdw_mem_debug("flush(0x%llx/0x%llx/0x%llx) (%p/0x%llx/%llu)\n",
		(uint64_t)map->mpriv, (uint64_t)map, (uint64_t)map->dbuf,
		map->vaddr, map->device_va, map->size);

out:
	mdw_trace_end();
	return ret;
}

static int mdw_mem_gen_handle(struct mdw_mem *m)
{
	int fd = 0;

	if (IS_ERR_OR_NULL(m->dbuf))
		return -EINVAL;

	/* create fd from dma-buf */
	fd =  dma_buf_fd(m->dbuf,
		(O_RDWR | O_CLOEXEC) & ~O_ACCMODE);
	if (fd < 0)
		mdw_drv_err("create handle for dmabuf(0x%llx) fail\n", (uint64_t)m->dbuf);

	get_dma_buf(m->dbuf);

	return fd;
}

static int mdw_mem_ioctl_alloc(struct mdw_fpriv *mpriv,
	union mdw_mem_args *args)
{
	struct mdw_mem_in *in = (struct mdw_mem_in *)args;
	struct mdw_mem *m = NULL;
	int ret = 0, handle = 0;

	if (!in->alloc.size) {
		mdw_drv_err("invalid size(%llu)\n", in->alloc.size);
		return -EINVAL;
	}

	mutex_lock(&mpriv->mtx);

	m = mdw_mem_alloc(mpriv, in->alloc.type, in->alloc.size,
		in->alloc.align, in->alloc.flags, NULL);
	memset(args, 0, sizeof(*args));
	if (!m) {
		mdw_drv_err("mdw_mem_alloc fail\n");
		ret = -ENOMEM;
		goto out;
	}

	handle = mdw_mem_gen_handle(m);
	if (handle < 0) {
		ret = -ENOMEM;
		goto free_mem;
	}

	/* add to u_hash */
	hash_add(mpriv->u_mem_hash, &m->fpriv_node, (uint64_t)m->dbuf);
	mutex_lock(&mdw_dev->mctl_mtx);
	hash_add(mdw_dev->u_mem_hash, &m->device_node, (uint64_t)m->dbuf);
	mutex_unlock(&mdw_dev->mctl_mtx);

	args->out.alloc.handle = handle;
	mdw_mem_debug("alloc mem handle(%d)\n", handle);
	mdw_mem_show(m);

	goto out;

free_mem:
	mdw_mem_free(mpriv, m);
out:
	mutex_unlock(&mpriv->mtx);
	return ret;
}

static int mdw_mem_ioctl_free(struct mdw_fpriv *mpriv,
	union mdw_mem_args *args)
{
	struct dma_buf *dbuf = NULL;
	struct mdw_mem *m = NULL;
	int ret = 0;

	dbuf = dma_buf_get(args->in.free.handle);
	if (!dbuf) {
		mdw_drv_err("invalid handle(%llu) to get dbuf\n", args->in.free.handle);
		return -EINVAL;
	}

	mutex_lock(&mpriv->mtx);

	m = mdw_mem_get_mem_by_handle(mpriv, args->in.free.handle);
	if (!m) {
		mdw_drv_err("can't find mem by handle(%llu), maybe free already\n",
			args->in.free.handle);
		ret = -EINVAL;
		goto unlock;
	}

	mdw_mem_show(m);

	hash_del(&m->fpriv_node);
	mutex_lock(&mdw_dev->mctl_mtx);
	hash_del(&m->device_node);
	mutex_unlock(&mdw_dev->mctl_mtx);

	mdw_mem_free(mpriv, m);

unlock:
	mutex_unlock(&mpriv->mtx);
	dma_buf_put(dbuf);
	return ret;
}

static int mdw_mem_ioctl_alloc_fb(struct mdw_fpriv *mpriv,
	union mdw_mem_args *args)
{
	struct mdw_mem_in *in = (struct mdw_mem_in *)args;
	int ret = 0;

	if (!in->alloc_fb.total_vlm_size) {
		mdw_drv_warn("invalid size(%u)\n", in->alloc_fb.total_vlm_size);
		return -EINVAL;
	}

	mdw_mem_debug("size(%u) num_subcmds(%u)\n",
		in->alloc_fb.total_vlm_size, in->alloc_fb.num_subcmds);
	mdw_trace_begin("apummu:alloc dram fb|size:%u",
		in->alloc_fb.total_vlm_size);
	ret = mdw_apu_mem_fallback_alloc((uint64_t)mpriv, in->alloc_fb.total_vlm_size,
		in->alloc_fb.num_subcmds);
	mdw_trace_end();

	if (ret)
		mdw_drv_err("apummu: alloc fb size(%u) fail(%d) num_subcmds(%u)\n",
			in->alloc_fb.total_vlm_size, ret,
			in->alloc_fb.num_subcmds);

	return ret;
}

static int mdw_mem_ioctl_map(struct mdw_fpriv *mpriv,
	union mdw_mem_args *args)
{
	int ret = 0;
	struct dma_buf *dbuf = NULL;
	struct mdw_mem_map *map = NULL;

	dbuf = dma_buf_get(args->in.map.handle);
	if (IS_ERR_OR_NULL(dbuf)) {
		mdw_drv_err("can't get dbuf by handle(%llu)\n", args->in.map.handle);
		return -EINVAL;
	}

	mutex_lock(&mpriv->mtx);

	map = mdw_mem_create_map(mpriv, dbuf, MDW_BUF_TYPE_DATA, args->in.map.flags, false);
	if (!map) {
		mdw_drv_err("create map failed\n");
		ret = -EINVAL;
		goto unlock;
	}

	/* add to u_hash */
	hash_add(mpriv->u_map_hash, &map->fpriv_node, (uint64_t)map->dbuf);
	memset(args, 0, sizeof(*args));
	args->out.map.device_va = map->device_va;
	args->out.map.type = map->mem_type;

	mdw_map_show(map);

unlock:
	mutex_unlock(&mpriv->mtx);
	dma_buf_put(dbuf);
	return ret;
}

static int mdw_mem_ioctl_unmap(struct mdw_fpriv *mpriv,
	union mdw_mem_args *args)
{
	struct dma_buf *dbuf = NULL;
	struct mdw_mem_map *map = NULL;
	int ret = 0;

	dbuf = dma_buf_get(args->in.map.handle);
	if (!dbuf) {
		mdw_drv_err("invalid handle(%llu) to get dbuf\n", args->in.unmap.handle);
		return -EINVAL;
	}

	mutex_lock(&mpriv->mtx);

	map = mdw_mem_get_map_by_handle(mpriv, args->in.unmap.handle, args->in.unmap.device_va);
	if (!map) {
		mdw_drv_err("can't find map by handle(%llu)\n", args->in.unmap.handle);
		ret = -EINVAL;
		goto unlock;
	}

	mdw_map_show(map);

	hash_del(&map->fpriv_node);
	mdw_mem_delete_map(mpriv, map);

unlock:
	mutex_unlock(&mpriv->mtx);
	dma_buf_put(dbuf);
	return ret;
}

int mdw_mem_ioctl(struct mdw_fpriv *mpriv, void *data)
{
	union mdw_mem_args *args = (union mdw_mem_args *)data;
	int ret = 0;

	mdw_flw_debug("s(0x%llx) op::%d\n", (uint64_t)mpriv, args->in.op);
	switch (args->in.op) {
	case MDW_MEM_IOCTL_ALLOC:
		ret = mdw_mem_ioctl_alloc(mpriv, args);
		break;

	case MDW_MEM_IOCTL_MAP:
		ret = mdw_mem_ioctl_map(mpriv, args);
		break;

	case MDW_MEM_IOCTL_UNMAP:
		ret = mdw_mem_ioctl_unmap(mpriv, args);
		break;

	case MDW_MEM_IOCTL_ALLOC_FB:
		ret = mdw_mem_ioctl_alloc_fb(mpriv, args);
		break;

	case MDW_MEM_IOCTL_FREE:
		ret = mdw_mem_ioctl_free(mpriv, args);
		break;

	case MDW_MEM_IOCTL_FLUSH:
	case MDW_MEM_IOCTL_INVALIDATE:
	default:
		mdw_drv_warn("not support memory op(%d)\n", args->in.op);
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* for validation */
int apusys_mem_validate_by_cmd(void *session, void *cmd, uint64_t eva, uint32_t size)
{
	struct mdw_fpriv *mpriv = NULL;
	struct mdw_cmd *c = NULL;
	struct mdw_device *mdev = NULL;
	struct mdw_mem_map *map = NULL;
	struct hlist_node *tmp = NULL;
	int i = 0, ret = -ENOMEM;

	/* check arguments */
	if (!session || !cmd)
		return -EINVAL;

	mpriv = (struct mdw_fpriv *)session;
	c = (struct mdw_cmd *)cmd;
	mdev = mpriv->mdev;

	hash_for_each_safe(mpriv->u_map_hash, i, tmp, map, fpriv_node) {
		mdw_vld_debug("check target(0x%llx/%u) map(0x%llx/%llu)...\n",
			eva, size, map->device_va, map->size);

		if (eva < map->device_va || eva + size > map->device_va + map->size)
			continue;

		mdw_vld_debug("check target(0x%llx/%u) map(0x%llx/%llu) matched!\n",
			eva, size, map->device_va, map->size);

		ret = mdw_cmd_invoke_map(c, map);
		if (ret) {
			mdw_drv_err("s(0x%llx)c(0x%llx)m(0x%llx/%u)get map fail(%d)\n",
				(uint64_t)session, (uint64_t)cmd, eva, size, ret);
		}

		return ret;
	}

	/* check vlm */
	if (test_bit(MDW_MEM_TYPE_VLM, mdev->mem_mask) &&
		size &&
		eva >= mdev->minfos[MDW_MEM_TYPE_VLM].device_va &&
		eva + size <= mdev->minfos[MDW_MEM_TYPE_VLM].device_va +
		mdev->minfos[MDW_MEM_TYPE_VLM].size) {
		mdw_vld_debug("m(0x%llx/%u) in vlm range(0x%llx/%llu)\n",
			eva, size,
			mdev->minfos[MDW_MEM_TYPE_VLM].device_va,
			mdev->minfos[MDW_MEM_TYPE_VLM].size);
			return 0;
	}

	return -EINVAL;
}

void *apusys_mem_query_kva_by_sess(void *session, uint64_t eva)
{
	struct mdw_fpriv *mpriv = NULL;
	struct mdw_mem_map *map = NULL;
	struct hlist_node *tmp = NULL;
	int i = 0;

	/* check arguments */
	if (!session)
		return NULL;

	mpriv = (struct mdw_fpriv *)session;

	hash_for_each_safe(mpriv->u_map_hash, i, tmp, map, fpriv_node) {
		if (eva >= map->device_va &&
			eva < map->device_va + map->size &&
			map->vaddr)
			return map->vaddr + (eva - map->device_va);
	}

	return NULL;
}

/* for addr cache operation */
static struct mdw_mem_map *mdw_mem_query_map_by_device_kva(uint64_t kva)
{
	struct mdw_mem_map *map = NULL, *tmp = NULL;

	mutex_lock(&mdw_dev->mctl_mtx);
	list_for_each_entry_safe(map, tmp, &mdw_dev->maps, device_node) {
		if (kva >= (uint64_t)map->vaddr &&
			kva < (uint64_t)map->vaddr + map->size) {

			mdw_mem_debug("query iova (0x%llx->0x%llx)\n",
				kva, (uint64_t)map);
			mutex_unlock(&mdw_dev->mctl_mtx);
			return map;
		}
	}
	mutex_unlock(&mdw_dev->mctl_mtx);

	return NULL;
}

int apusys_mem_flush_kva(void *kva, uint32_t size)
{
	struct mdw_mem_map *map = NULL;
	int ret = 0;

	map = mdw_mem_query_map_by_device_kva((uint64_t)kva);
	if (!map) {
		mdw_drv_err("no map\n");
		ret = -ENOMEM;
		goto out;
	}

	ret = mdw_mem_flush(map->mpriv, map);
	mdw_mem_debug("flush kva 0x%llx\n", (uint64_t)kva);

out:
	return ret;
}

int apusys_mem_invalidate_kva(void *kva, uint32_t size)
{
	struct mdw_mem_map *map = NULL;
	int ret = 0;

	map = mdw_mem_query_map_by_device_kva((uint64_t)kva);
	if (!map) {
		mdw_drv_err("no map\n");
		ret = -ENOMEM;
		goto out;
	}

	ret = mdw_mem_invalidate(map->mpriv, map);

	mdw_mem_debug("invalidate kva 0x%llx\n", (uint64_t)kva);
out:
	return ret;
}

uint64_t apusys_mem_query_kva(uint64_t device_va)
{
	struct mdw_mem_map *map = NULL, *tmp = NULL;
	uint64_t kva = 0;

	mutex_lock(&mdw_dev->mctl_mtx);
	list_for_each_entry_safe(map, tmp, &mdw_dev->maps, device_node) {
		if (device_va >= map->device_va &&
			device_va < map->device_va + map->size) {
			if (map->vaddr == NULL)
				break;

			kva = (uint64_t)map->vaddr + (device_va - map->device_va);
			mdw_mem_debug("query kva (0x%llx->0x%llx)\n",
				device_va, kva);
		}
	}
	mutex_unlock(&mdw_dev->mctl_mtx);

	return kva;
}

uint64_t apusys_mem_query_iova(uint64_t kva)
{
	struct mdw_mem_map *map = NULL, *tmp = NULL;
	uint64_t eva = 0;

	mutex_lock(&mdw_dev->mctl_mtx);
	list_for_each_entry_safe(map, tmp, &mdw_dev->maps, device_node) {
		if (kva >= (uint64_t)map->vaddr &&
			kva < (uint64_t)map->vaddr + map->size) {
			if (!map->device_va)
				break;

			eva = map->device_va + (kva - (uint64_t)map->vaddr);
			mdw_mem_debug("query eva (0x%llx->0x%llx)\n",
				kva, eva);
		}
	}
	mutex_unlock(&mdw_dev->mctl_mtx);

	return eva;
}

//MODULE_IMPORT_NS(DMA_BUF);
