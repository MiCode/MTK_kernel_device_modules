/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: Anthony Huang <anthony.huang@mediatek.com>
 */

#ifndef __MMDVFS_V5_H__
#define __MMDVFS_V5_H__

#include <linux/clk-provider.h>

#define MAX_LEVEL 8

#define IPI_TIMEOUT_MS (500U)

#define MMDVFS_DBG(fmt, args...) \
	pr_notice("[mmdvfs][dbg]%s:%d: "fmt"\n", __func__, __LINE__, ##args)
#define MMDVFS_ERR(fmt, args...) \
	pr_notice("[mmdvfs][err]%s:%d: "fmt"\n", __func__, __LINE__, ##args)

#define OPP2LEVEL(idx, opp) \
	(((opp) < 0 || (opp) >= mmdvfs_data->rc[idx].level_num) ? 0 : (mmdvfs_data->rc[idx].level_num - (opp) - 1))
enum {
	log_pwr,
	log_ipi,
	log_num
};

enum {
	FUNC_MMDVFS_INIT,
	FUNC_MMDVFS_SUSPEND,

	// mmup
	FUNC_MMDVFS_FORCE_CLOCK,
	FUNC_MMDVFS_FORCE_CLOCK_RC,

	// vcp
	FUNC_MMDVFS_VCP_SET_RATE,

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
	const u8 *force_user;
	const u8 force_user_num;
	void __iomem *rc_base;
};

struct mmdvfs_mux {
	const u8 id;
	const u8 rc;
	const u8 pos;
	const char *name;
	u64 freq[MAX_LEVEL];
	struct mutex lock;
};

struct mmdvfs_user {
	const u8 id;
	const char *name;
	const u8 mux;
	const u8 xpu;
	const u32 xpu_ofs;
	u8 level;
	struct clk_hw clk_hw;
	struct clk *clk;
	int vcp_power;
};

struct mmdvfs_debug_user {
	u8 id;
	u8 rc;
	const char *name;
	struct clk *clk;
	s8 force_opp;
	s8 vote_opp;
};

struct mmdvfs_ops {
	int (*dfs_vote_by_xpu)(const u8 user_id, const u8 level);
	int (*dvfsrc_rg_dump)(void);
	int (*dvfsrc_record_dump)(void);
	u32 (*get_mmup_sram_offset)(void);
};

struct mmdvfs_data {
	struct mmdvfs_rc *rc;
	const u8 rc_num;
	struct mmdvfs_mux *mux;
	const u8 mux_num;
	struct mmdvfs_user *user;
	const u8 user_num;
	struct mmdvfs_ops *ops;
};

int mmdvfs_hfrp_ipi_send(const u8 func, const u8 idx, const u8 opp, u32 *data, const bool vcp);
int mmdvfs_mux_probe(struct platform_device *pdev);

inline bool mmdvfs_mmup_cb_ready_get(void);
inline void mmdvfs_mmup_cb_mutex_lock(void);
inline void mmdvfs_mmup_cb_mutex_unlock(void);

inline u8 mmdvfs_user_get_rc(const u8 idx);
inline u64 mmdvfs_user_get_freq_by_opp(const u8 idx, const s8 opp);
inline s8 mmdvfs_get_level_to_opp(const u8 rc, const s8 lvl);
int mmdvfs_force_vcore_notify(const u32 val);
int mmdvfs_force_step(const u8 idx, const s8 opp);
int mmdvfs_dump_dvfsrc_rg(void);
int mmdvfs_dump_dvfsrc_record(void);
void mmdvfs_record_cmd_user(const u8 usr, const u8 idx, const u8 lvl);
#endif

