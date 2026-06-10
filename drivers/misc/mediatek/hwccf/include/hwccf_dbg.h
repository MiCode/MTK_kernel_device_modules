/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef _HWCCF_DBG_H
#define _HWCCF_DBG_H

#include <linux/types.h> /* Kernel only */
#include <linux/kernel.h> /* Kernel only */
#include <linux/time.h> /* Kernel only */
#include <linux/delay.h> /* Kernel only */
#include <linux/regmap.h> /* Kernel only */
#include <hwccf_provider.h>

struct hwccf_ops_data {
	uint32_t is_smc;
	uint32_t is_nowait;
	uint32_t is_irq_voter;
	uint32_t hwccf_type;
	uint32_t hwccf_resource_id;
	uint32_t hwccf_vote_type;
	uint32_t hwccf_vote_data;
};

struct regname {
	u32 ofs;
	const char *name;
};

int hwccf_proc_dbg_register(void);
uint32_t hwccf_read_wrapper(enum HWCCF_TYPE hwccf_type, uint32_t ofs);

#endif /* _HWCCF_DBG_H */
