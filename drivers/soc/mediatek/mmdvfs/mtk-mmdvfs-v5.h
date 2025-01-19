/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Anthony Huang <anthony.huang@mediatek.com>
 */

#ifndef __DRV_CLK_MMDVFS_V5_H
#define __DRV_CLK_MMDVFS_V5_H

#include <linux/clk-provider.h>

#define MAX_LEVEL 8

#define IPI_TIMEOUT_MS (200U)

#define MMDVFS_DBG(fmt, args...) \
	pr_notice("[mmdvfs][dbg]%s:%d: "fmt"\n", __func__, __LINE__, ##args)
#define MMDVFS_ERR(fmt, args...) \
	pr_notice("[mmdvfs][err]%s:%d: "fmt"\n", __func__, __LINE__, ##args)

enum {
	log_pwr,
	log_ipi,
	log_num
};

enum {
	FUNC_MMDVFS_INIT,
	FUNC_MMDVFS_SUSPEND,

	FUNC_NUM
};

struct mmdvfs_ipi_data {
	u8 func;
	u8 idx;
	u8 opp;
	u8 ack;
	u32 base;
};

struct mmdvfs_rc {
	const u8 id;
	const u32 pa;
	const u8 level_num;
};

struct mmdvfs_mux {
	const u8 id;
	const u8 rc;
	const u8 pos;
	const char *name;
	u64 freq[MAX_LEVEL];
};

struct mmdvfs_user {
	const u8 id;
	const char *name;
	const u8 mux;
	const u8 xpu;
	const u8 level;
	struct clk_hw clk_hw;
	struct clk *clk;
	int vcp_power;
};

struct mmdvfs_data {
	struct mmdvfs_rc *rc;
	const u8 rc_num;
	struct mmdvfs_mux *mux;
	const u8 mux_num;
	struct mmdvfs_user *user;
	const u8 user_num;
};

int mmdvfs_v5_probe(struct platform_device *pdev);

#endif /* __DRV_CLK_MMDVFS_V5_H */
