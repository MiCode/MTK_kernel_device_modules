/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_VDISP_H__
#define __MTK_VDISP_H__

#include "mtk_dpc.h"

void mtk_vdisp_dpc_register(const struct dpc_funcs *funcs);

#endif
