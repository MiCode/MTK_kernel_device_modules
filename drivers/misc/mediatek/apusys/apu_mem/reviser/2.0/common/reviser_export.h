/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */


#ifndef __APUSYS_REVISER_EXPORT_H__
#define __APUSYS_REVISER_EXPORT_H__
#include <linux/types.h>

extern struct reviser_dev_info *g_rdv;

int reviser_get_vlm(uint32_t request_size, bool force,
		unsigned long *ctx, uint32_t *tcm_size);
int reviser_free_vlm(uint32_t ctx);
int reviser_set_context(int type,
		int index, uint8_t ctx);
int reviser_get_resource_vlm(uint32_t *addr, uint32_t *size);
int reviser_get_pool_size(uint32_t type, uint32_t *size);
int reviser_alloc_mem(uint32_t type, uint32_t size, uint64_t *addr, uint32_t *sid);
int reviser_free_mem(uint32_t sid);
int reviser_import_mem(uint64_t session, uint32_t sid);
int reviser_unimport_mem(uint64_t session, uint32_t sid);
int reviser_map_mem(uint64_t session, uint32_t sid, uint64_t *addr);
int reviser_unmap_mem(uint64_t session, uint32_t sid);
int reviser_map_iova(uint32_t type, uint64_t session, uint64_t device_va,
		uint64_t buf_size, uint64_t *eva, uint SLC_DC_EN);
int reviser_unmap_iova(uint64_t session, uint64_t device_va, uint64_t buf_size);
int reviser_table_alloc(uint64_t session);
int reviser_table_free(uint64_t session);
int reviser_ssid_get(uint64_t session, uint32_t *ssid);
int reviser_export_API_init(void);
#endif
