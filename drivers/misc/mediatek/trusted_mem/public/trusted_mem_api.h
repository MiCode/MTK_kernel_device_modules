/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef TRUSTED_MEM_API_H
#define TRUSTED_MEM_API_H

enum TRUSTED_MEM_REQ_TYPE {
	TRUSTED_MEM_REQ_SVP_REGION = 0,
	TRUSTED_MEM_REQ_PROT_REGION = 1,
	TRUSTED_MEM_REQ_WFD_REGION = 2,
	TRUSTED_MEM_REQ_HAPP = 3,
	TRUSTED_MEM_REQ_HAPP_EXTRA = 4,
	TRUSTED_MEM_REQ_SDSP = 5,
	TRUSTED_MEM_REQ_SDSP_SHARED = 6,
	TRUSTED_MEM_REQ_2D_FR = 7,
	TRUSTED_MEM_REQ_TUI_REGION = 8,
	TRUSTED_MEM_REQ_SVP_PAGE = 9,
	TRUSTED_MEM_REQ_PROT_PAGE = 10,
	TRUSTED_MEM_REQ_WFD_PAGE = 11,
	TRUSTED_MEM_REQ_SAPU_DATA_SHM = 12,
	TRUSTED_MEM_REQ_SAPU_ENGINE_SHM = 13,
	TRUSTED_MEM_REQ_AP_MD_SHM = 14,
	TRUSTED_MEM_REQ_AP_SCP_SHM = 15,
	TRUSTED_MEM_REQ_SAPU_PAGE = 16,
};

struct ssheap_buf_info {
	struct sg_table *table;
	unsigned long alignment;
	struct list_head block_list;
	unsigned long req_size;
	unsigned long aligned_req_size;
	unsigned long allocated_size;
	unsigned long elems;
	struct page *pmm_msg_page;
	u8 mem_type;
	u64 ffa_handle;
#if IS_ENABLED(CONFIG_MTK_PROT_MEM_SSHEAP_V2)
	struct list_head pmm_msg_list;
	struct page *pmm_page;
#endif
};


/**********************************************************/
/**** Trusted Memory Common APIs for ION kernel driver ****/
/**********************************************************/
#if IS_ENABLED(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM)
int trusted_mem_api_alloc(enum TRUSTED_MEM_REQ_TYPE mem_type, u32 alignment,
			  u32 *size, u32 *refcount, u64 *sec_handle,
			  uint8_t *owner, uint32_t id, struct ssheap_buf_info **buf_info);
int trusted_mem_api_alloc_zero(enum TRUSTED_MEM_REQ_TYPE mem_type,
			       u32 alignment, u32 size, u32 *refcount,
			       u64 *sec_handle, uint8_t *owner, uint32_t id);
int trusted_mem_api_unref(enum TRUSTED_MEM_REQ_TYPE mem_type, u64 sec_handle,
			  uint8_t *owner, uint32_t id, struct ssheap_buf_info *buf_info);
int trusted_mem_api_query_pa(enum TRUSTED_MEM_REQ_TYPE mem_type, u32 alignment,
			      u32 size, u32 *refcount, u64 *handle,
			      u8 *owner, u32 id, u32 clean, uint64_t *phy_addr);
bool trusted_mem_api_get_region_info(enum TRUSTED_MEM_REQ_TYPE mem_type,
				     u64 *pa, u32 *size);
enum TRUSTED_MEM_REQ_TYPE trusted_mem_api_get_page_replace(
				  enum TRUSTED_MEM_REQ_TYPE mem_type);
int trusted_mem_ffa_query_pa(u64 *handle, uint64_t *phy_addr);
bool trusted_mem_is_ffa_enabled(void);
bool trusted_mem_is_page_v2_enabled(void);

int trusted_mem_page_based_alloc(enum TRUSTED_MEM_REQ_TYPE req_mem_type,
				struct sg_table *sg_tbl, u64 *handle, u32 size);
int trusted_mem_page_based_free(enum TRUSTED_MEM_REQ_TYPE req_mem_type, u64 handle);
void trusted_mem_enable_high_freq(void);
void trusted_mem_disable_high_freq(void);
#endif

#endif /* end of TRUSTED_MEM_API_H */
