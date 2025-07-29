/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __ARM64_KVM_NVHE_PKVM_HYPMMU_H__
#define __ARM64_KVM_NVHE_PKVM_HYPMMU_H__

#include <mtk-iommu-defines.h>

#include "include/mod_debug.h"

#ifdef memset
#undef memset
#endif

#ifdef memcpy
#undef memcpy
#endif

#define hyp_phys_to_virt(x)	mod_ops->hyp_va(x)

/* Refer to TRUSTED_MEM_REQ_TYPE in the kernel trusted_mem_api.h */
#define HYP_PMM_ATTR_SVP (9)
#define HYP_PMM_ATTR_PROT_MEM (10)
#define HYP_PMM_ATTR_WFD (11)
#define HYP_PMM_ATTR_SAPU_DATA_SHM (12)
#define HYP_PMM_ATTR_SAPU_PAGE (14)
#define HYP_PMM_ATTR_AP_MD_SHM (4)
#define HYP_PMM_ATTR_AP_SCP_SHM (5)

/* EL2 modules */
extern const struct pkvm_module_ops *mod_ops;

/* HYPMMU */
u8 hypmmu_get_srinfo(u8 attr);

/*
 * PMM-HAL-IMPL(s)
 */
void register_mtkiommu_pmm_hal(void);
void register_gpumpu_pmm_hal(void);
void register_inframpu_pmm_hal(void);

/* INFRA-MPU */
void infra_mpu_set_ipc_base(u64 pa, void *va);

#endif
