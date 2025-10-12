/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */
#ifndef __HA_M4U_API_H__
#define __HA_M4U_API_H__

#include <mtk-larb-port.h>

#define M4U_SEC_CONFIG(port, reg, mask, value) \
	do { \
		int larb_id, larb_port; \
		if ((unsigned int)port >= M4U_PORT_UNKNOWN) \
			break; \
		larb_id = MTK_M4U_TO_LARB(port); \
		larb_port = MTK_M4U_TO_PORT(port); \
		if (larb_id < SMI_LARB_NR) \
			reg =(smiLarbBasePA[larb_id] + SMI_LARB_SEC_CONx(larb_port)); \
		mask = F_SMI_SEC_EN(1); \
		value = F_SMI_SEC_EN(!!(value)); \
	} while(0)

/*
 * input dom_value is ~0, output dom_value is 0xc0/0x40
 * input dom_value is 0, output dom_value is 0x0
 *
 */
#define M4U_GET_DOMAIN(iommu_sec_id, port, reg, dom_value, dom_mask) \
	do { \
		int larb_id, larb_port, dom_id; \
		if ((unsigned int)port >= M4U_PORT_UNKNOWN) \
			break; \
		larb_id = MTK_M4U_TO_LARB(port); \
		larb_port = MTK_M4U_TO_PORT(port); \
		if (larb_id < SMI_LARB_NR) \
			reg =(smiLarbBasePA[larb_id] + SMI_LARB_SEC_CONx(larb_port)); \
		dom_id = 12; \
		if (dom_value) \
			dom_value = F_SMI_DOMN(dom_id); \
		else \
			dom_value = F_SMI_DOMN(0); \
		dom_mask = F_MSK(8, 4); \
		(void)iommu_sec_id; \
	} while(0)

#endif // __HA_M4U_API_H__
