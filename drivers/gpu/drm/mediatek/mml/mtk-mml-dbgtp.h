/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __MTK_MML_DBGTP_H__
#define __MTK_MML_DBGTP_H__

#include <linux/types.h>
#include <cmdq-util.h>

#include "mtk_disp_dbgtp.h"

extern int mml_dbgtp_dump;

/*
 * mml_dbgtp_register - register disp dbgtp driver functions.
 *
 * @funcs:	DISP DBGTP driver functions.
 */
void mml_dbgtp_register(const struct dbgtp_funcs *funcs);

void mml_dbgtp_config(struct cmdq_pkt *pkt, u32 sysid,
	phys_addr_t base_pa, void __iomem *base);
void mml_dbgtp_config_dump(u32 sysid, void __iomem *base);

#endif	/* __MTK_MML_DBGTP_H__ */
