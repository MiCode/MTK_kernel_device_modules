/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Yc Li <yc.li@mediatek.com>
 */
#ifndef MTK_SMI_HWCCF_H
#define MTK_SMI_HWCCF_H

#define MTK_SMI_PD_CTRL(hwccf_dom, subsys_id)	(((hwccf_dom & 0x3f) << 8) | ((subsys_id & 0xff)))
#define MTK_SMI_SUBSYS_ID_NR_MAX        (32)

/* HWCCF domain */
#define MTK_SMI_HWCCF_DOM0              (0)
#define MTK_SMI_HWCCF_DOM1              (1)
#define MTK_SMI_HWCCF_DOM2              (2)
#define MTK_SMI_HWCCF_DOM_NR            (3)
#define MTK_SMI_HWCCF_DOM_SKIP          (0x3f)

#define MTK_SMI_ID2HWCCF_DOM(id)        (((id) >> 8) & 0x3f)
#define MTK_SMI_ID2SUBSYS_ID(id)        ((id) & 0xff)

#endif
