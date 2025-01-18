/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

// Here saves all MTK's smc call related to pKVM

// It is for el1 only
#define SMC_ID_MTK_PKVM_ADD_HVC 0xC400ff10
#define SMC_ID_MTK_PKVM_TMEM_REGION_PROTECT		0XBB00FFA7
#define SMC_ID_MTK_PKVM_TMEM_REGION_UNPROTECT	0XBB00FFA8
#define SMC_ID_MTK_PKVM_TMEM_PAGE_PROTECT		0XBB00FFA9
#define SMC_ID_MTK_PKVM_TMEM_PAGE_UNPROTECT		0XBB00FFAA

// smc result code
#define SMC_RET_MTK_PKVM_SMC_HANDLER_DUPLICATED_ID -1
