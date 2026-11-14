/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_NPU_MEM_H__
#define __MTK_NPU_MEM_H__

#include <linux/dma-heap.h>
#include <linux/types.h>

enum apu_sysmem_flag {
	APU_SYSMEM_FLAG_CACHEABLE,
};
#define F_APU_SYSMEM_FLAG_CACHEABLE (1ULL << APU_SYSMEM_FLAG_CACHEABLE)

enum apu_sysmem_mem_type {
	APU_SYSMEM_TYPE_NONE, //none
	APU_SYSMEM_TYPE_DRAM, //main memory
	APU_SYSMEM_TYPE_SYSTEM_NPU, //extern vlm
	APU_SYSMEM_TYPE_SYSTEM_ISP, //dc slb
};

enum apu_sysmem_map_type {
	APU_SYSMEM_MAP_TYPE_DEVICE_VA, //32bit device va
	APU_SYSMEM_MAP_TYPE_DEVICE_LONGVA, //long device va
	APU_SYSMEM_MAP_TYPE_DEVICE_SHAREVA,//share device va
	APU_SYSMEM_MAP_TYPE_SLC_DC, //for slc dc
	APU_SYSMEM_MAP_TYPE_SHAREABLE,
};
#define F_APU_SYSMEM_MAP_TYPE_DEVICE_VA (1ULL << APU_SYSMEM_MAP_TYPE_DEVICE_VA)
#define F_APU_SYSMEM_MAP_TYPE_DEVICE_LONGVA (1ULL << APU_SYSMEM_MAP_TYPE_DEVICE_LONGVA)
#define F_APU_SYSMEM_MAP_TYPE_DEVICE_SHAREVA (1ULL << APU_SYSMEM_MAP_TYPE_DEVICE_SHAREVA)
#define F_APU_SYSMEM_MAP_TYPE_SLC_DC (1ULL << APU_SYSMEM_MAP_TYPE_SLC_DC)
#define F_APU_SYSMEM_MAP_TYPE_SHAREABLE (1ULL << APU_SYSMEM_MAP_TYPE_SHAREABLE)

struct apu_sysmem_map {
	struct dma_buf *dbuf;
	struct sg_table *sgt;
	struct dma_buf_attachment *attach;
	struct device *mem_dev; //mem device
	uint64_t device_va; //map by apummu
	uint64_t device_iova; //map by mmu
	uint64_t size; //query from dmabuf
	uint64_t map_bitmask; //bitmask apu_sysmem_map_type
	uint32_t ssid;
	bool ssid_en;
	enum apu_sysmem_mem_type mem_type;

	struct iosys_map map;
	void *vaddr;

	struct list_head a_node; //to allocator
	struct apu_sysmem_allocator *allocator; //parent
};
#define APU_SYSMEM_GET_DEVICE_VA(map) (map->device_va)
#define APU_SYSMEM_GET_KVA(buf) (buf->vaddr)
#define APU_SYSMEM_GET_DEVICE_VA_SIZE(map) (map->device_iova_size)
#define APU_SYSMEM_GET_DMABUF(x) (x->dbuf)

struct apu_sysmem_buffer {
	uint64_t size;
	enum apu_sysmem_mem_type mem_type;
	uint64_t flags;

	struct dma_buf *dbuf;
	struct iosys_map map;
	void *vaddr;

	struct list_head a_node; //to allocator
	struct apu_sysmem_allocator *allocator; //parent
};

struct apu_sysmem_allocator {
	struct apu_sysmem_buffer *(*alloc)(
		struct apu_sysmem_allocator *allocator,
		enum apu_sysmem_mem_type mem_type,
		uint64_t size,
		uint64_t flags, //apu_sysmem_flag
		char *name);
	int (*free)(
		struct apu_sysmem_allocator *allocator,
		struct apu_sysmem_buffer *buf);
	struct apu_sysmem_map *(*map)(
		struct apu_sysmem_allocator *allocator,
		struct dma_buf *dbuf,
		enum apu_sysmem_mem_type mem_type,
		uint64_t map_bitmask); //apu_sysmem_map_type
	int (*unmap)(
		struct apu_sysmem_allocator *allocator,
		struct apu_sysmem_map *map);
	void (*dump)(struct apu_sysmem_allocator *allocator);

	uint64_t session_id;
	uint32_t smmu_ssid;
	struct list_head buffers; //for apu_sysmem_allocator
	struct list_head maps; //for apu_sysmem_map
	struct mutex mtx;
};

struct apu_sysmem_allocator *apu_sysmem_create_allocator(uint64_t session_id);
int apu_sysmem_delete_allocator(struct apu_sysmem_allocator *allocator);

#endif //__MTK_APU_MEM_H__
