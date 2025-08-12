/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#ifndef __GZVM_HOOK_H_
#define __GZVM_HOOK_H_

typedef void (*gzvm_iommu_sync_t)(void);
void gzvm_register_iommu_sync_cb(gzvm_iommu_sync_t func_ptr);

#endif // !__GZVM_HOOK_H_
