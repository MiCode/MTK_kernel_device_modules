/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef GPU_PDMA_MT6993_H
#define GPU_PDMA_MT6993_H

#define PDMA_RINGBUF_PA_NUM		8

enum CCMD_CACHE_MODE {
	SMART_CACHE_API = 0,
	COMPUTE_TLS,
	UNSUPPORTED_MODE
};

/**
 * struct pdma_device       Object representing an instance of PDMA device,
 *                          allocated from the probe method of pdma driver.
 *
 * @dev:                    Pointer to the kernel's representation of the
 *                          PDMA platform device.
 * @ctx_list:               A gloal list of existing ccmd_context instances.
 * @pdma_device_lock:       Protected critical section related to HW lock.
 * @ccmd_locked_ctx_id:     CCMD context ID which has been used. Bit-wise.
 * @reg_base:               Base address of CCMD register.
 * @reg_region:             Range of CCMD register.
 * @hw_sem_base:            PA of hw semaphore base.
 * @hw_sem_offset:          Offset from hw semaphore base.
 * @hw_sem_bit:             Bit shifter of HW semaphore. Set by dts.
 * @buffer_status_base:     Base address of CCMD buffer status.
 * @buffer_status_region:   Range of CCMD buffer status.
 * @ao_reg_base:            Base address of CCMD AO register.
 * @ao_hrptr_offset:        Offset to cid0  hrptr.
 * @hw_sem_base:            PA of hw semaphore base.
 * @pdma_sram_base:         PA of PDMA SRAM base
 * @pdma_reg_base_kva:      Kernel virtual address of CCMD base address.
 * @pdma_hw_sem_base_kva:   Kernel virtual address of CCMD HW semaohore.
 * @pdma_buffer_status_base_kva:   Kernel virtual address of CCMD buffer status.
 * @pdma_reg_base_ao_kva:   Kernel virtual address of CCMD AO register.
 * @pdma_sram_base_kva:     Kernel virtual address of SRAM base.
 * @page_order:             4k-based page_order. e.q. 3 for 8 4k-alinged pages.
 * @pdma_sram_base:         PA of PDMA SRAM base.
 * @config_mode:            Init CCMD by AP or mirco processor. Set by dts.
 * @dynamic_mode:           Backup and restore dynamic policy to SRAM.
 * @max_ctx_cnt:            Maximum supported CCMD context count.
 * @max_non_api_ctx_cnt:    Maximum supported count for non-Cache-API context.
 *                          Should be less than or equal to max_ctx_cnt.
 * @non_api_ctx_cnt:        Number of non-api contexts that have required HW lock
 * @sw_version:             To identify specific SW version.
 * @extended_pbha_bits:     Number of bits used for extended PBHA ID
 */

struct pdma_device {
	struct device *dev;
	struct list_head ctx_list;
	struct mutex pdma_device_lock;
	struct list_head extened_pbha_pool;
	u32 ccmd_locked_ctx_id;
	u64 reg_base;
	u64 reg_region;
	u64 hw_sem_base;
	u64 hw_sem_offset;
	u32 hw_sem_bit;
	u64 buffer_status_base;
	u64 buffer_status_region;
	u64 ao_reg_base;
	u64 ao_hrptr_offset;
	u64 pdma_sram_base;
	void __iomem *pdma_reg_base_kva;
	void __iomem *pdma_hw_sem_base_kva;
	void __iomem *pdma_buffer_status_base_kva;
	void __iomem *pdma_reg_base_ao_kva;
	struct pdma_sram *pdma_sram_base_kva;
	u32 page_order; /* g_page_order is 4k-based */
	u32 config_mode;
	s32 dynamic_mode;
	u8 max_ctx_cnt;
	u8 max_non_api_ctx_cnt;
	u8 non_api_ctx_cnt;
	u8 sw_version;
	u8 extended_pbha_bits;
};


/**
 * struct ccmd_context -    Object representing an instance of CCMD context,
 *                          allocated on user calls lock HW.
 *
 * @pdma_dev:               Pointer to global pdma device struct.
 * @entry:                  Link to the global ctx list from struct pdma_device
 * @pbha_list               Record PBHA ID requested by the ccmd context
 * @kctx_id:                kbase ctx (vk device) that lock the cid.
 * @cid:                    Unique CCMD context ID
 * @ringbuf_paddr:          PA of the ring buffer for the context
 * @ringbuf_paddr:          VA of the ring buffer for the context
 * @cid_reg_base:           Context view of CCMD register base
 * @mode:                   Use smart cache API or others. Refer to CCMD_CACHE_MODE.
 */

struct ccmd_context {
	struct pdma_device *pdma_dev;
	struct list_head entry;
	struct list_head pbha_list;
	u32 kctx_id;
	u32 cid;
	u64 ringbuf_paddr;
	u64 ringbuf_vaddr;
	u64 cid_reg_base;
	u32 mode;
};

/**
 * struct extended_pbha -   Object represnting an instance of an extended PBHA.
 *
 * @entry                   Link to the global PBHA pool from struct pdma_device
 * @id                      Extended PBHA ID.
 */

struct extended_pbha {
	struct list_head entry;
	u32 id;
};


/**
 * @in:                     Input parameters
 * @in.kctx_id:             Kbase context ID that locks HW.
 * @in.mode:                API mode or others (ex. Compute-TLS).
 * @out:                    Output parameters
 * @out.status: 0:          Fail to lock HW. 1: Success to lock HW.
 * @out.base:               PA of CCMD Reg base.
 * @out.region_size:        Size of CCMD register region
 * @out.ringbuf:            PA of ring buffer base
 * @out.size:               Size of ring buffer (bytes)
 * @out.hw_sem_base:        PA of hw semaphore base
 * @out.hw_sem_offset:      Offset from hw semaphore base.
 * @out.sw_ver:             Software version for specific HW configuration
 * @out.cid:                CCMD Context ID. Supported cid is from 0 to 3.
 * @out.debug_mode:         For debugging propose only.
 * @out.ao_region_base      PA of CCMD AO Reg base.
 * @out.ao_hrptr_offset     Offset of cidx hrptr.
 */

struct pdma_hw_lock {
	struct {
		unsigned int kctx_id;
		unsigned int mode;
	} in;
	struct {
		unsigned int status;
		unsigned long base;
		unsigned int region_size;
		unsigned long ringbuf;
		unsigned int size;
		unsigned long hw_sem_base;
		unsigned int hw_sem_offset;
		unsigned int sw_ver;
		unsigned int cid;
		bool debug_mode;
		unsigned long ao_region_base;
		unsigned long ao_hrptr_offset;
	} out;
};

/**
 * @in: Input parameters
 * @in.kctx_id: Context ID that locks HW.
 * @in.hwptr: Write pointer updated to HW.
 * @out: Output parameters
 * @out.hrptr: Read pointer read from HW.
 */

struct pdma_rw_ptr {
	struct {
		unsigned int kctx_id;
		unsigned int hwptr;
	} in;
	struct {
		unsigned int hrptr;
	} out;
};

struct pdma_sram {
	unsigned int ccmd_hw_reset;
	unsigned int interrupt_status;    /* [0]:enable irq & pmu[1]:interrupt status(from eb) */
	unsigned int ringbuf[PDMA_RINGBUF_PA_NUM];
	unsigned int pmu_status_df_0;
	unsigned int pmu_status_df_1;
};

/* IOCTL cmd */
#define PDMA_IOCTL_TYPE 0x80

#define GPU_PDMA_LOCKHW					_IOWR(PDMA_IOCTL_TYPE, 0x1, struct pdma_hw_lock)
#define GPU_PDMA_UNLOCKHW				_IOWR(PDMA_IOCTL_TYPE, 0x2, struct pdma_hw_lock)
#define GPU_PDMA_WRITE_HWPTR			_IOWR(PDMA_IOCTL_TYPE, 0x3, struct pdma_rw_ptr)
#define GPU_PDMA_READ_HRPTR			_IOWR(PDMA_IOCTL_TYPE, 0x4, struct pdma_rw_ptr)


#endif /* GPU_PDMA_MT6993_H */
