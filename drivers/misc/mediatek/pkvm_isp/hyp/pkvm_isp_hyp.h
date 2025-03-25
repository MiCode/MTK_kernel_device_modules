/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <asm/kvm_pkvm_module.h>

#define PFX "[pkvm_isp] "
#define CALL_FROM_OPS(fn, ...) pkvm_isp_ops->fn(__VA_ARGS__)

#define IS_MULTI_SEC_PORT_SUPPORT
#define MULTI_SENSOR_INDIVIDUAL_SUPPORT
#define Single_Secure_csi_port_front 0
#define Multi_Secure_csi_port_front  0
#define Multi_Secure_csi_port_rear   3

extern const struct pkvm_module_ops *pkvm_isp_ops;
