/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_APU_MDW_SANITY_H__
#define __MTK_APU_MDW_SANITY_H__

#include "mdw.h"

int mdw_sanity_einfo_check(struct mdw_cmd *c);
int mdw_sanity_adj_check(struct mdw_cmd *c);
int mdw_sanity_link_check(struct mdw_cmd *c);
int mdw_sanity_order_check(struct mdw_cmd *c);

#endif
