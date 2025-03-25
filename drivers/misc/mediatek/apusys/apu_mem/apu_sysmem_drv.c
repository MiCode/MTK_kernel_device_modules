// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/of_device.h>
#include <linux/types.h>
#include <linux/dma-direct.h>
#include <linux/dma-buf.h>
#include <uapi/linux/dma-buf.h>
#include <linux/list.h>
#include <linux/hashtable.h>
#include <mtk_heap.h>
#include <mtk-smmu-v3.h>

#include "apusys_core.h"
#include "apu_sysmem.h"
#include "apu_sysmem_drv.h"
#include "apu_sysmem_dbg.h"

#include "apummu_mgt.h"
#include "apu_mem_export.h"

struct apusys_core_info *g_apusys_core;

/* mem domain type */
enum {
	APU_SYSMEM_DOMAIN_NONE,
	APU_SYSMEM_DOMAIN_CODE, // 32bit addr
	APU_SYSMEM_DOMAIN_DATA, // long addr
	APU_SYSMEM_DOMAIN_SHARE,// share addr
	APU_SYSMEM_DOMAIN_CODE_COHERENT, // COHERENT use 32bit addr
	APU_SYSMEM_DOMAIN_DATA_COHERENT, // COHERENT use long addr
	APU_SYSMEM_DOMAIN_SHARE_COHERENT, // COHERENT use long addr
	APU_SYSMEM_DOMAIN_MAX,
};

struct apu_sysmem_mgr {
	uint32_t domain_num;
	struct device *dev[APU_SYSMEM_DOMAIN_MAX];
	bool ssid_en[APU_SYSMEM_DOMAIN_MAX];
};

//---------------------------------------------------------
struct apu_sysmem_mgr g_apu_sysmem_mgr;

static inline enum AMMU_BUF_TYPE apu_sysmem_mapflag2ammu(uint64_t map_flags)
{
	enum AMMU_BUF_TYPE ret = AMMU_DATA_BUF;

	if (map_flags & F_APU_SYSMEM_MAP_TYPE_DEVICE_SHAREVA)
		ret = AMMU_CMD_BUF;

	return ret;
}

static int apu_sysmem_set_name(struct dma_buf *dbuf, const char *buf)
{
	char *name = NULL;

	name = kstrndup(buf, DMA_BUF_NAME_LEN, GFP_KERNEL);
	if (IS_ERR(name))
		return PTR_ERR(name);

	spin_lock(&dbuf->name_lock);
	kfree(dbuf->name);
	dbuf->name = name;
	spin_unlock(&dbuf->name_lock);

	return 0;
}

static struct device *apu_sysmem_get_mem_dev(uint64_t map_bitmask)
{
	struct device *mem_dev = NULL;

	/* choose memory device by type */
	if ((map_bitmask & F_APU_SYSMEM_MAP_TYPE_DEVICE_VA)) {
		if (map_bitmask & F_APU_SYSMEM_MAP_TYPE_SHAREABLE) {
			if (g_apu_sysmem_mgr.dev[APU_SYSMEM_DOMAIN_CODE_COHERENT] == NULL)
				apu_sysmem_err("not support coherence 32bit device va\n");
			else
				mem_dev = g_apu_sysmem_mgr.dev[APU_SYSMEM_DOMAIN_CODE_COHERENT];
		} else {
		if (g_apu_sysmem_mgr.dev[APU_SYSMEM_DOMAIN_CODE] == NULL)
			apu_sysmem_err("not support 32bit device va\n");
		else
			mem_dev = g_apu_sysmem_mgr.dev[APU_SYSMEM_DOMAIN_CODE];
		}
	} else if (map_bitmask & F_APU_SYSMEM_MAP_TYPE_DEVICE_LONGVA) {
		if (map_bitmask & F_APU_SYSMEM_MAP_TYPE_SHAREABLE) {
			if (g_apu_sysmem_mgr.dev[APU_SYSMEM_DOMAIN_DATA_COHERENT] == NULL)
				apu_sysmem_err("not support coherence long addr device va\n");
			else
				mem_dev = g_apu_sysmem_mgr.dev[APU_SYSMEM_DOMAIN_DATA_COHERENT];
		} else {
		if (g_apu_sysmem_mgr.dev[APU_SYSMEM_DOMAIN_DATA] == NULL)
			apu_sysmem_err("not support long addr device va\n");
		else
			mem_dev = g_apu_sysmem_mgr.dev[APU_SYSMEM_DOMAIN_DATA];
		}
	} else if (map_bitmask & F_APU_SYSMEM_MAP_TYPE_DEVICE_SHAREVA) {
		if (map_bitmask & F_APU_SYSMEM_MAP_TYPE_SHAREABLE) {
			if (g_apu_sysmem_mgr.dev[APU_SYSMEM_DOMAIN_SHARE_COHERENT] == NULL) {
				apu_sysmem_info("not support coherence share addr device va\n");
				mem_dev = g_apu_sysmem_mgr.dev[APU_SYSMEM_DOMAIN_CODE_COHERENT];
			} else {
				mem_dev = g_apu_sysmem_mgr.dev[APU_SYSMEM_DOMAIN_SHARE_COHERENT];
			}
		} else {
		if (g_apu_sysmem_mgr.dev[APU_SYSMEM_DOMAIN_SHARE] == NULL) {
			apu_sysmem_info("not support share addr device va\n");
			mem_dev = g_apu_sysmem_mgr.dev[APU_SYSMEM_DOMAIN_CODE];
		} else {
			mem_dev = g_apu_sysmem_mgr.dev[APU_SYSMEM_DOMAIN_SHARE];
		}
		}
	} else {
		apu_sysmem_err("no va map bit\n");
	}

	return mem_dev;
}

static struct apu_sysmem_buffer *apu_sysmem_alloc(struct apu_sysmem_allocator *allocator,
	enum apu_sysmem_mem_type mem_type, uint64_t size, uint64_t flags, char *name)
{
	struct apu_sysmem_buffer *buf = NULL;

	apu_sysmem_debug("alloc memtype(%d) size(%llu) flags(0x%llx) name(%s)\n",
		mem_type, size, flags, name);

	/* new buffer */
	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf) {
		apu_sysmem_err("allocate buf failed\n");
		goto out;
	}

	/* alloc dmabuf */
	switch (mem_type) {
	case APU_SYSMEM_TYPE_DRAM:
		buf->dbuf = apu_sysmem_dmaheap_alloc(size, flags);
		break;
	case APU_SYSMEM_TYPE_SYSTEM_NPU:
	case APU_SYSMEM_TYPE_SYSTEM_ISP:
		buf->dbuf = apu_sysmem_aram_alloc(allocator->session_id, mem_type, size, flags);
		break;
	case APU_SYSMEM_TYPE_NONE:
	default:
		break;
	};
	/* check alloc status */
	if (!(buf->dbuf)) {
		apu_sysmem_err("allocate dmabuf failed, memtype(%d) size(%llu) flags(0x%llx)\n",
			mem_type, size, flags);
		goto free_buf;
	}

	/* set name */
	if (name) {
		if (apu_sysmem_set_name(buf->dbuf, name)) {
			apu_sysmem_warn("dmabuf set name failed, memtype(%d) size(%llu) flags(0x%llx) name(%s)\n",
				mem_type, size, flags, name);
		}
	}

	/* map kva */
	if (mem_type == APU_SYSMEM_TYPE_DRAM) {
		if (dma_buf_vmap_unlocked(buf->dbuf, &buf->map)) {
			apu_sysmem_err("vmap dmabuf failed, memtype(%d) size(%llu) flags(0x%llx)\n",
				mem_type, size, flags);
			goto free_dmabuf;
		}
	}

	buf->size = size;
	buf->mem_type = mem_type;
	buf->vaddr = buf->map.vaddr;
	buf->allocator = allocator;
	buf->flags = flags;
	/* add to allocator's buffer list */
	mutex_lock(&allocator->mtx);
	list_add_tail(&buf->a_node, &allocator->buffers);
	mutex_unlock(&allocator->mtx);

	apu_sysmem_debug("alloc sysbuf(0x%llx/0x%llx) done, memtype(%d) size(%llu) flags(0x%llx) name(%s)\n",
		(uint64_t)buf, (uint64_t)buf->dbuf, mem_type, size, flags, name);

	/* get dmabuf's ref */
	get_dma_buf(buf->dbuf);

	goto out;

free_dmabuf:
	apu_sysmem_dmaheap_free(buf->dbuf);
free_buf:
	kfree(buf);
	buf = NULL;
out:
	return buf;
}

static int apu_sysmem_free(struct apu_sysmem_allocator *allocator,
	struct apu_sysmem_buffer *buf)
{
	struct dma_buf *dbuf = buf->dbuf;

	apu_sysmem_debug("free(0x%llx/0x%llx) memtype(%d) size(%llu) flags(0x%llx)\n",
		(uint64_t)buf, (uint64_t)buf->dbuf, buf->mem_type, buf->size, buf->flags);

	/* delete from allocator's buffer list */
	mutex_lock(&allocator->mtx);
	list_del(&buf->a_node);
	mutex_unlock(&allocator->mtx);

	/* unmap kva */
	if (buf->mem_type == APU_SYSMEM_TYPE_DRAM)
		dma_buf_vunmap_unlocked(buf->dbuf, &buf->map);

	/* release dmabuf */
	apu_sysmem_dmaheap_free(buf->dbuf);

	/* free buf */
	kfree(buf);

	dma_buf_put(dbuf);

	return 0;
}

static bool apu_sysmem_get_ssid_en(struct device *dev)
{
	bool ssid_en = false;
	uint32_t i;

	for (i = APU_SYSMEM_DOMAIN_NONE + 1; i < APU_SYSMEM_DOMAIN_MAX; i++) {
		if (dev == g_apu_sysmem_mgr.dev[i]) {
			ssid_en = g_apu_sysmem_mgr.ssid_en[i];
			break;
		}
	}

	return ssid_en;
}

static struct dma_buf_attachment *apu_sysmem_buf_attach(struct apu_sysmem_map *map)
{
	if (!map->ssid_en || !map->ssid || (map->map_bitmask & F_APU_SYSMEM_MAP_TYPE_SLC_DC))
		goto next;

	return dma_buf_attach_ssid(map->dbuf, map->mem_dev, map->ssid);

next:
	return dma_buf_attach(map->dbuf, map->mem_dev);
}

static struct apu_sysmem_map *apu_sysmem_map(struct apu_sysmem_allocator *allocator,
	struct dma_buf *dbuf, enum apu_sysmem_mem_type mem_type, uint64_t map_bitmask)
{
	struct apu_sysmem_map *map = NULL;
	struct scatterlist *sg = NULL;
	int ret = 0, i = 0;
	uint a_SLC_DC_EN;

	/* get dmabuf's ref */
	if (IS_ERR_OR_NULL(dbuf)) {
		apu_sysmem_err("dbuf is error or null 0x%llx", (uint64_t) dbuf);
		goto out;
	}
	/* get dmabuf's ref */
	get_dma_buf(dbuf);

	/* new map */
	map = kzalloc(sizeof(*map), GFP_KERNEL);
	if (!map) {
		apu_sysmem_err("allocate map failed\n");
		goto put_dma_buf;
	}

	/* check if dbuf is allocated from coherence heap */
	if (is_coherent_heap_dmabuf(dbuf)) {
		apu_sysmem_info("dmabuf is from coherent heap\n");
		map_bitmask |= F_APU_SYSMEM_MAP_TYPE_SHAREABLE;
	}

	/* get memory device */
	map->mem_dev = apu_sysmem_get_mem_dev(map_bitmask);
	if (map->mem_dev == NULL) {
		apu_sysmem_err("get memory device failed, bitmask(0x%llx)\n", map_bitmask);
		goto free_map;
	}

	/* get ssid enable info */
	map->ssid_en = apu_sysmem_get_ssid_en(map->mem_dev);

	map->dbuf = dbuf;
	map->ssid = allocator->smmu_ssid;
	map->mem_type = mem_type;

	/* map vaddr */
	if (mem_type == APU_SYSMEM_TYPE_DRAM) {
		if (dma_buf_vmap_unlocked(map->dbuf, &map->map)) {
			apu_sysmem_err("vmap dmabuf failed, dbuf(0x%llx) map_bitmask(0x%llx)\n",
				(uint64_t)dbuf, map_bitmask);
			goto free_map;
		}
	}

	if (map_bitmask & F_APU_SYSMEM_MAP_TYPE_SLC_DC)
		a_SLC_DC_EN = 1;
	else
		a_SLC_DC_EN = 0;

	/* map by iommu/smmu */
	map->attach = apu_sysmem_buf_attach(map);
	if (IS_ERR(map->attach)) {
		ret = PTR_ERR(map->attach);
		apu_sysmem_err("dma_buf_attach failed: %d\n", ret);
		goto unmap_vaddr;
	}
	map->sgt = dma_buf_map_attachment_unlocked(map->attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(map->sgt)) {
		ret = PTR_ERR(map->sgt);
		apu_sysmem_err("dma_buf_map_attachment_unlocked failed: %d\n", ret);
		goto detach_dbuf;
	}

	/* get start addr and size */
	map->device_iova = sg_dma_address(map->sgt->sgl);
	for_each_sgtable_dma_sg(map->sgt, sg, i) {
		if (!sg)
			break;
		map->size += sg_dma_len(sg);
	}

	/* map eva by apummu */
	ret = apu_mem_map_iova(apu_sysmem_mapflag2ammu(map_bitmask), allocator->session_id,
		map->device_iova, map->size, &map->device_va, a_SLC_DC_EN);
	if (ret) {
		apu_sysmem_err("map dmabuf eva failed(%d), dbuf(0x%llx) map_bitmask(0x%llx)\n",
			ret, (uint64_t)dbuf, map_bitmask);
		goto unmap_dbuf;
	}

	map->allocator = allocator;
	map->map_bitmask = map_bitmask;
	map->vaddr = map->map.vaddr;

	/* add to allocator's map list */
	mutex_lock(&allocator->mtx);
	list_add_tail(&map->a_node, &allocator->maps);
	mutex_unlock(&allocator->mtx);

	apu_sysmem_debug("map(0x%llx/0x%llx) map_bitmask(0x%llx) dva(0x%llx) iova(0x%llx) size(%llu)\n",
		(uint64_t)map, (uint64_t)map->dbuf, map->map_bitmask, map->device_va, map->device_iova, map->size);

	goto out;

unmap_dbuf:
	dma_buf_unmap_attachment_unlocked(map->attach, map->sgt, DMA_BIDIRECTIONAL);
detach_dbuf:
	dma_buf_detach(map->dbuf, map->attach);
unmap_vaddr:
	if (mem_type == APU_SYSMEM_TYPE_DRAM)
		dma_buf_vunmap_unlocked(map->dbuf, &map->map);
free_map:
	kfree(map);
put_dma_buf:
	dma_buf_put(dbuf);
	map = NULL;
out:
	return map;
}

static int apu_sysmem_unmap(struct apu_sysmem_allocator *allocator,
		struct apu_sysmem_map *map)
{
	struct dma_buf *dbuf = map->dbuf;

	apu_sysmem_debug("unmap(0x%llx/0x%llx) map_bitmask(0x%llx) dva(0x%llx) iova(0x%llx) size(%llu)\n",
		(uint64_t)map, (uint64_t)map->dbuf, map->map_bitmask, map->device_va,
		map->device_iova, map->size);

	/* delete from allocator's buffer list */
	mutex_lock(&allocator->mtx);
	list_del(&map->a_node);
	mutex_unlock(&allocator->mtx);

	/* unmap eva */
	if (apu_mem_unmap_iova(allocator->session_id, map->device_iova, map->size))
		apu_sysmem_err("unmap eva(0x%llx) failed\n", map->device_iova);

	/* unmap iova */
	dma_buf_unmap_attachment_unlocked(map->attach, map->sgt, DMA_BIDIRECTIONAL);
	dma_buf_detach(map->dbuf, map->attach);

	/* unmap vaddr */
	if (map->mem_type == APU_SYSMEM_TYPE_DRAM)
		dma_buf_vunmap_unlocked(map->dbuf, &map->map);

	/* free buf */
	kfree(map);

	/* put dmabuf's ref */
	dma_buf_put(dbuf);

	return 0;
}

static void apu_sysmem_dump(struct apu_sysmem_allocator *allocator)
{
	struct apu_sysmem_map *map = NULL;
	struct apu_sysmem_buffer *buf = NULL;
	uint32_t idx = 0;

	apu_sysmem_info("allocator(0x%llx/0x%llx) dump:\n", (uint64_t)allocator, allocator->session_id);

	mutex_lock(&allocator->mtx);

	idx = 0;
	list_for_each_entry(buf, &allocator->buffers, a_node) {
		apu_sysmem_info(" buf[%u]: name(%s/%s) dbuf(0x%llx) size(%llu) mem_type(%d) vaddr(%p)\n",
			idx, buf->dbuf->exp_name, buf->dbuf->name,
			(uint64_t)buf->dbuf, buf->size, buf->mem_type, buf->vaddr);
		idx++;
	}

	idx = 0;
	list_for_each_entry(map, &allocator->maps, a_node) {
		apu_sysmem_info(" map[%u]: name(%s/%s) dbuf(0x%llx) device_va(0x%llx) device_iova(0x%llx) size(%llu) map_bitmask(0x%llx)\n",
			idx, map->dbuf->exp_name, map->dbuf->name,
			(uint64_t)map->dbuf, map->device_va, map->device_iova,
			map->size, map->map_bitmask);
		idx++;
	}

	mutex_unlock(&allocator->mtx);
}

struct apu_sysmem_allocator *apu_sysmem_create_allocator(uint64_t session_id)
{
	struct apu_sysmem_allocator *allocator = NULL;
	uint32_t ssid;
	int ret;

	apu_sysmem_debug("session_id(0x%llx)\n", session_id);

	ret = apu_mem_table_alloc(session_id);
	if (ret) {
		apu_sysmem_err("Failed to alloc NPUMMU table: %d\n", ret);
		goto out;
	}


	/* ssid */
	ret = apu_mem_ssid_get(session_id, &ssid);
	if (ret) {
		apu_sysmem_err("Failed to get ssid: %d\n", ret);
		goto out;
	}

	/* new allocator */
	allocator = kzalloc(sizeof(*allocator), GFP_KERNEL);
	if (!allocator) {
		apu_sysmem_err("allocate allocator failed\n");
		goto out;
	}

	/* init allocator */
	INIT_LIST_HEAD(&allocator->buffers);
	INIT_LIST_HEAD(&allocator->maps);
	mutex_init(&allocator->mtx);
	allocator->session_id = session_id;
	allocator->smmu_ssid = ssid;
	allocator->alloc = apu_sysmem_alloc;
	allocator->free = apu_sysmem_free;
	allocator->map = apu_sysmem_map;
	allocator->unmap = apu_sysmem_unmap;
	allocator->dump = apu_sysmem_dump;

out:
	return allocator;
}

int apu_sysmem_delete_allocator(struct apu_sysmem_allocator *allocator)
{
	int ret = -EEXIST;

	/* check buffers empty */
	if (!list_empty(&allocator->buffers)) {
		apu_sysmem_err("some buffers in allocator, delete failed\n");
		allocator->dump(allocator);
		goto out;
	}

	/* check maps empty */
	if (!list_empty(&allocator->maps)) {
		apu_sysmem_err("some maps in allocator, delete failed\n");
		allocator->dump(allocator);
		goto out;
	}

	apu_mem_table_free(allocator->session_id);

	apu_sysmem_debug("delete allocator(0x%llx/0x%llx)\n", (uint64_t)allocator, allocator->session_id);

	kfree(allocator);
	ret = 0;

out:
	return ret;
}

static void of_find_ssid_range(struct device_node *np, uint32_t *ssid_range)
{
#define SSID_MAX	255

	struct device_node *ssids_node;
	int ret;

	ssids_node = of_get_child_by_name(np, "mtk,smmu-ssids");
	if (!ssids_node) {
		apu_sysmem_info("No node name mtk,smmu-ssids\n");
		goto exit;
	}

	ret = of_property_read_u32_array(ssids_node, "mtk,smmu-ssid-range", ssid_range, 2);
	if (ret) {
		apu_sysmem_err("No node name mtk,smmu-ssid-range\n");
		goto exit;
	}

	if (ssid_range[0] == 0 ||
	    ssid_range[0] > ssid_range[1] ||
	    ssid_range[1] > SSID_MAX) {
		apu_sysmem_err("Invalid ssid range: %u-%u\n",
			ssid_range[0], ssid_range[1]);
		ssid_range[0] = 0;
		ssid_range[1] = 0;
		goto exit;
	}
exit:
	return;
}

static int apu_sysmem_enable_ssid(struct device *dev, uint32_t min, uint32_t max)
{
	uint32_t i;
	int ret;

	if (!min)
		goto exit;

	for (i = min; i <= max; i++) {
		ret = mtk_smmu_enable_ssid(dev, i);
		if (ret) {
			apu_sysmem_err("Failed to enable ssid (%u): %d\n", i, ret);
			goto err;
		}
	}
exit:
	return 0;

err:
	for (i = i - 1; i >= min; i--)
		mtk_smmu_disable_ssid(dev, i);
	return ret;
}

static void apu_sysmem_disable_ssid(struct device *dev, uint32_t min, uint32_t max)
{
	uint32_t i;

	if (!min)
		goto exit;

	for (i = min; i <= max; i++)
		mtk_smmu_disable_ssid(dev, i);
exit:
	return;
}

//---------------------------------------------------------
static int apu_sysmem_probe(struct platform_device *pdev)
{
	int ret = 0;
	uint32_t type = 0;
	uint64_t mask = 0;
	uint32_t ssid_range[2] = {0, 0};
	struct device *dev = &pdev->dev;

	of_property_read_u64(pdev->dev.of_node, "mask", &mask);
	of_property_read_u32(pdev->dev.of_node, "type", &type);
	of_find_ssid_range(pdev->dev.of_node, ssid_range);

	apu_sysmem_info("%s mask 0x%llx type %u\n", __func__, mask, type);

	if (type >= APU_SYSMEM_DOMAIN_MAX || type == APU_SYSMEM_DOMAIN_NONE) {
		apu_sysmem_err("unknown mem domain(%u), probe failed\n", type);
		ret = -EINVAL;
		goto out;
	}
	if (g_apu_sysmem_mgr.dev[type] != NULL) {
		apu_sysmem_err("domain %d already exist\n", type);
		ret = -EINVAL;
		goto out;
	}

	/* set mask */
	ret = dma_set_mask_and_coherent(dev, mask);
	if (ret) {
		apu_sysmem_err("unable to set DMA mask coherent: %d\n", ret);
		goto out;
	}
	apu_sysmem_info("%s dma_set_mask_and_coherent 0x%llx type %u\n", __func__, mask, type);

	if (!pdev->dev.dma_parms) {
		pdev->dev.dma_parms =
			devm_kzalloc(dev, sizeof(*pdev->dev.dma_parms), GFP_KERNEL);
	}
	if (pdev->dev.dma_parms)
		dma_set_max_seg_size(dev, mask);

	ret = apu_sysmem_enable_ssid(&pdev->dev, ssid_range[0], ssid_range[1]);
	if (ret) {
		apu_sysmem_err("Failed to enable ssid: %d\n", ret);
		goto free_dma_params;
	}

	/* setup mem domain */
	g_apu_sysmem_mgr.domain_num++;
	g_apu_sysmem_mgr.dev[type] = &pdev->dev;
	g_apu_sysmem_mgr.ssid_en[type] = ssid_range[0] ? true : false;

	goto out;

free_dma_params:
	if (pdev->dev.dma_parms)
		devm_kfree(dev, pdev->dev.dma_parms);
	pdev->dev.dma_parms = NULL;
out:
	return ret;
}

static void apu_sysmem_remove(struct platform_device *pdev)
{
	int type = 0;
	uint32_t ssid_range[2] = {0, 0};
	struct device *dev = &pdev->dev;

	apu_sysmem_info("+\n");

	/* disable ssid */
	of_find_ssid_range(pdev->dev.of_node, ssid_range);
	apu_sysmem_disable_ssid(&pdev->dev, ssid_range[0], ssid_range[1]);

	/* free dma_params */
	if (pdev->dev.dma_parms)
		devm_kfree(dev, pdev->dev.dma_parms);

	/* clear domain infos */
	of_property_read_u32(pdev->dev.of_node, "type", &type);
	g_apu_sysmem_mgr.ssid_en[type] = false;
	g_apu_sysmem_mgr.dev[type] = NULL;
	g_apu_sysmem_mgr.domain_num--;

	apu_sysmem_info("-, domain_num(%u)\n", g_apu_sysmem_mgr.domain_num);
}

static const struct of_device_id mem_of_match[] = {
	{.compatible = "mediatek, apu_mem_code"},
	{.compatible = "mediatek, apu_mem_data"},
	{.compatible = "mediatek, apu_mem_share"},
	{.compatible = "mediatek, apu_mem_code_coherent"},
	{.compatible = "mediatek, apu_mem_data_coherent"},
	{.compatible = "mediatek, apu_mem_share_coherent"},
	{},
};

static struct platform_driver apu_sysmem_driver = {
	.driver = {
		.name = "apu_sysmem_driver",
		.owner = THIS_MODULE,
		.of_match_table = mem_of_match,
	},
	.probe = apu_sysmem_probe,
	.remove = apu_sysmem_remove,
};

int apu_sysmem_init(struct apusys_core_info *info)
{
	int ret = 0;

	apu_sysmem_info("+\n");

	g_apusys_core = info;
	memset(&g_apu_sysmem_mgr, 0, sizeof(g_apu_sysmem_mgr));

	ret =  platform_driver_register(&apu_sysmem_driver);
	if (ret) {
		apu_sysmem_err("failed to register apu sysmem driver\n");
		goto out;
	}

	if (apu_sysmem_dbg_init(info->dbg_root))
		apu_sysmem_err("apu_sysmem_dbg_init fail\n");

	apu_sysmem_info("-\n");

	goto out;

out:
	return ret;
}

void apu_sysmem_exit(void)
{
	apu_sysmem_info("+\n");
	platform_driver_unregister(&apu_sysmem_driver);

	g_apusys_core = NULL;
	apu_sysmem_info("-\n");
}
