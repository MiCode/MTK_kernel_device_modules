// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include "mtk_log.h"
#include "mtk_dump.h"
#include "mtk_drm_ddp_comp.h"

#define MT6991_PQ_PATH_SEL 0x34

int mtk_ddp_pq_path_sel_MT6991(enum mtk_ddp_comp_id cur,
				      enum mtk_ddp_comp_id next,
				      unsigned int *addr)
{
	int value = -1;

	switch (cur) {
	case DDP_COMPONENT_MDP_RSZ0:
	case DDP_COMPONENT_MDP_RSZ1:
		*addr = MT6991_PQ_PATH_SEL;
		break;
	default:
		value = -1;
		return value;
	}

	/* set value according to dst comp */
	switch (next) {
	case DDP_COMPONENT_TDSHP0:
		value = 16;
		break;
	case DDP_COMPONENT_TDSHP1:
		value = (16 << 8);
		break;
	case DDP_COMPONENT_ID_MAX:
		value = 0;
		break;

	default:
		value = -1;
		return value;
	}
	DDPDBG("%s, cur=%s->next=%s, addr:0x%x, value:0x%x\n",
		__func__,
		mtk_dump_comp_str_id(cur),
		mtk_dump_comp_str_id(next),
		*addr,
		value);

	return value;
}
