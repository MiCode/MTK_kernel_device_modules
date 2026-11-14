// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/highmem.h>

#include "apu_sysmem_drv.h"
#include "apummu_mem_def.h"
#include "apu_mem_export.h"

struct apu_sysmem_aram_info {
	enum apu_sysmem_mem_type type;
	uint64_t session_id;

	uint64_t addr;
	uint64_t size;
	uint32_t sid; //from ammu

	struct list_head attachments; //for apu_sysmem_aram_attachment
	struct mutex mtx;
};

struct apu_sysmem_aram_attachment {
	struct apu_sysmem_aram_info *info;
	struct sg_table sgt;
	struct list_head node; //to apu_sysmem_aram_info
};

static inline int apu_sysmem_slb2ammu(enum apu_sysmem_mem_type type)
{
	int ret = -1; //error

	/* convert sysmem type to ammu type */
	switch (type) {
	case APU_SYSMEM_TYPE_SYSTEM_NPU:
		ret = APUMMU_MEM_TYPE_RSV_S;
		break;
	case APU_SYSMEM_TYPE_SYSTEM_ISP:
		ret = APUMMU_MEM_TYPE_EXT;
		break;
	default:
		apu_sysmem_err("unknown type(%d)\n", type);
		break;
	}

	return ret;
}

static int apu_sysmem_aram_attach(struct dma_buf *dbuf,
	struct dma_buf_attachment *attach)
{
	struct apu_sysmem_aram_info *info = dbuf->priv;
	struct apu_sysmem_aram_attachment *asla = NULL;
	int ret = 0;

	asla = kzalloc(sizeof(*asla), GFP_KERNEL);
	if (IS_ERR_OR_NULL(asla)) {
		apu_sysmem_err("alloc npu ram attachment failed\n");
		return -ENOMEM;
	}

	/* alloc sgtable */
	ret = sg_alloc_table(&asla->sgt, 1, GFP_KERNEL);
	if (ret)
		goto free_attach;

	attach->priv = asla;
	asla->info = info;

	/* add list */
	mutex_lock(&info->mtx);
	list_add_tail(&asla->node, &info->attachments);
	mutex_unlock(&info->mtx);

	apu_sysmem_debug("dbuf(0x%llx)\n", (uint64_t)dbuf);

	goto out;

free_attach:
	kfree(asla);
out:
	return ret;
}

static void apu_sysmem_aram_detach(struct dma_buf *dbuf,
	struct dma_buf_attachment *attach)
{
	struct apu_sysmem_aram_info *info = dbuf->priv;
	struct apu_sysmem_aram_attachment *asla = attach->priv;

	apu_sysmem_debug("\n");
	mutex_lock(&info->mtx);
	list_del(&asla->node);
	mutex_unlock(&info->mtx);

	sg_free_table(&asla->sgt);
	kfree(asla);
}

static struct sg_table *apu_sysmem_aram_map_attachment(struct dma_buf_attachment *attach,
	enum dma_data_direction dir)
{
	struct apu_sysmem_aram_attachment *asla = attach->priv;
	struct apu_sysmem_aram_info *info = asla->info;
	dma_addr_t vlm_addr = 0;
	int ret = 0;

	apu_sysmem_debug("type(%d) size(%llu) sid(%u)\n", info->type, info->size, info->sid);

	ret = apu_mem_map(info->session_id, info->sid, &vlm_addr);
	if (ret) {
		apu_sysmem_err("map vlm fail(%d)\n", ret);
		return ERR_PTR(-ENOMEM);
	}

	sg_dma_address(asla->sgt.sgl) = vlm_addr;
	sg_dma_len(asla->sgt.sgl) = info->size;

	return &asla->sgt;
}

static void apu_sysmem_aram_unmap_attachment(struct dma_buf_attachment *attach,
	struct sg_table *sgt, enum dma_data_direction dir)
{
	struct apu_sysmem_aram_attachment *asla = attach->priv;
	struct apu_sysmem_aram_info *info = asla->info;
	int ret = 0;

	apu_sysmem_debug("type(%d) size(%llu) sid(%u)\n", info->type, info->size, info->sid);

	ret = apu_mem_unmap(info->session_id, info->sid);
	if (ret)
		apu_sysmem_err("map vlm fail(%d)\n", ret);
}

static void apu_sysmem_aram_release(struct dma_buf *dbuf)
{
	struct apu_sysmem_aram_info *info = dbuf->priv;

	apu_sysmem_debug("size(%llu) sid(%d)\n", info->size, info->sid);

	if (apu_mem_free(info->sid)) {
		apu_sysmem_err("free apu_mem failed, type(%d) size(%llu) sid(%u)\n",
			info->type, info->size, info->sid);
	}

	kfree(info);
}

static struct dma_buf_ops apu_sysmem_aram_dbuf_ops = {
	.attach = apu_sysmem_aram_attach,
	.detach = apu_sysmem_aram_detach,
	.map_dma_buf = apu_sysmem_aram_map_attachment,
	.unmap_dma_buf = apu_sysmem_aram_unmap_attachment,
	.release = apu_sysmem_aram_release,
};

struct dma_buf *apu_sysmem_aram_alloc(uint64_t session_id, enum apu_sysmem_mem_type type, uint64_t size, uint64_t flags)
{
	struct dma_buf *dbuf = NULL;
	int ammu_mapped_type = apu_sysmem_slb2ammu(type), ret = 0;
	struct apu_sysmem_aram_info *info = NULL;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

	/* check type */
	if (ammu_mapped_type < 0) {
		apu_sysmem_err("invalid type(%d)\n", type);
		goto out;
	}

	/* check size */
	if (size == 0) {
		apu_sysmem_err("invalid size(%llu)\n", size);
		goto out;
	}

	/* alloc info */
	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		apu_sysmem_err("allocate info failed\n");
		goto out;
	}

	/* alloc mem */
	ret = apu_mem_alloc(ammu_mapped_type, size, &info->addr, &info->sid);
	if (ret) {
		apu_sysmem_err("allocate info failed(%d)\n", ret);
		goto free_info;
	}

	/* assign value */
	info->session_id = session_id;
	info->type = type;
	info->size = size;
	INIT_LIST_HEAD(&info->attachments);
	mutex_init(&info->mtx);

	/* export as dma_buf */
	exp_info.ops = &apu_sysmem_aram_dbuf_ops;
	exp_info.size = info->size;
	exp_info.flags = O_RDWR | O_CLOEXEC;
	exp_info.priv = info;
	dbuf = dma_buf_export(&exp_info);
	if (IS_ERR_OR_NULL(dbuf)) {
		apu_sysmem_err("dma_buf_export Fail\n");
		goto free_mem;
	}

	goto out;

free_mem:
	if (apu_mem_free(info->sid)) {
		apu_sysmem_err("free apu_mem failed, type(%d) size(%llu) sid(%u)\n",
			info->type, info->size, info->sid);
	}
free_info:
	kfree(info);
	dbuf = NULL;
out:
	return dbuf;
}

int apu_sysmem_aram_free(struct dma_buf *dbuf)
{
	if (IS_ERR_OR_NULL(dbuf))
		return -EINVAL;

	dma_buf_put(dbuf);

	return 0;
}