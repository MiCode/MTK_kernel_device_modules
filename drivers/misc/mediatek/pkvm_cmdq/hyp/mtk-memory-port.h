/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __MTK_MEMORY_PORT_H__
#define __MTK_MEMORY_PORT_H__

#define SMI_LARB_SEC_CONx(larb_port)	(0xf80 + ((larb_port) << 2))

#define F_BIT_VAL(val, bit)		((!!(val)) << (bit))
#define F_SMI_SEC_EN(sec)		F_BIT_VAL(sec, 1)

#define F_VAL(val, msb, lsb)		(((val) & ((1 << (msb - lsb + 1)) - 1)) << lsb)
#define F_MSK(msb, lsb)			F_VAL(0xffffffff, msb, lsb)
#define F_SMI_DOMN(domain)		F_VAL(domain, 8, 4)

/* below refer to mtk-memory-port.h in kernel */
#define TAB_ID				(0)

/* tab_id[23:20] + dom[19:16] + larb[10:5] + port[4:0] */
#define MTK_M4U_PORT_ID(tab, dom, larb, port)	(((tab & 0xf) << 20) | ((dom & 0xf) << 16) | \
						((larb & 0x3f) << 5) | (port & 0x1f))
#define MTK_M4U_DOM_ID(dom, larb, port)		MTK_M4U_PORT_ID(TAB_ID, dom, larb, port)
#define MTK_M4U_ID(larb, port)			(((larb) << 5) | (port))

#define MTK_M4U_TO_LARB(id)		(((id) >> 5) & 0x3f)
#define MTK_M4U_TO_PORT(id)		((id) & 0x1f)

#endif // __MTK_MEMORY_PORT_H__
