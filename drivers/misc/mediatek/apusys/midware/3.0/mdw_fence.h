/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_APU_MDW_FENCE_H__
#define __MTK_APU_MDW_FENCE_H__

int mdw_fence_init(struct mdw_cmd *c);
void mdw_fence_delete(struct mdw_cmd *c);

#endif