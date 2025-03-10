// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>

#include "mtk-mml-dbgtp.h"
#include "mtk-mml-core.h"

int mml_dbgtp_msg;
module_param(mml_dbgtp_msg, int, 0644);

int mml_dbgtp_dump;
module_param(mml_dbgtp_dump, int, 0644);

#define dbgtp_msg(fmt, args...) \
do { \
	if (mml_dbgtp_msg) \
		_mml_log("[dbgtp]" fmt, ##args); \
	else \
		mml_msg("[dbgtp]" fmt, ##args); \
} while (0)

static struct dbgtp_funcs mml_dbgtp_funcs;

void mml_dbgtp_register(const struct dbgtp_funcs *funcs)
{
	mml_dbgtp_funcs = *funcs;
}
EXPORT_SYMBOL_GPL(mml_dbgtp_register);

void mml_dbgtp_config(struct cmdq_pkt *pkt, u32 sysid,
	phys_addr_t base_pa, void __iomem *base)
{
	if (!mml_dbgtp_funcs.dbgtp_mmlsys_config) {
		dbgtp_msg("%s dbgtp_mmlsys_config not exist", __func__);
		return;
	}

	dbgtp_msg("%s config mml-%d dbg by %s", __func__, sysid, pkt ? "gce" : "cpu");

	mml_dbgtp_funcs.dbgtp_mmlsys_config(pkt, NULL, sysid, base_pa, base);
}

void mml_dbgtp_config_dump(u32 sysid, void __iomem *base)
{
	if (!mml_dbgtp_funcs.dbgtp_mmlsys_config_dump) {
		dbgtp_msg("%s dbgtp_mmlsys_config_dump not exist", __func__);
		return;
	}

	mml_dbgtp_funcs.dbgtp_mmlsys_config_dump(base, sysid);
}

