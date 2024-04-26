/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

// Here saves all MTK's smc call related to pKVM

// It is for el1 only
#define SMC_ID_MTK_PKVM_ADD_HVC 0xC400ff10

// smc result code
#define SMC_RET_MTK_PKVM_SMC_HANDLER_DUPLICATED_ID -1
