// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include "mmdvfs-mt6991.h"

int mmdvfs_mt6993_dvfsrc_rg_dump(void)
{
	int i, ret = 0;
	u8 rc_level[MMDVFS_V5_PWR_NUM], mux_level[MMDVFS_V5_MUX_NUM];
	u32 mux_curr;

	//rc current level
	for (i = 0; i < MMDVFS_V5_PWR_NUM; i++) {
		if (!mmdvfs_rc[i].rc_base) {
			MMDVFS_ERR("get rc_base failed, rc_id:%hhu", i);
			ret = -1;
			continue;
		}
		rc_level[i] = readl(mmdvfs_rc[i].rc_base + RC_CURRENT_LEVEL);
	}

	//mux current level
	for (i = 0; i < MMDVFS_V5_MUX_NUM; i++) {
		if (!mmdvfs_rc[mmdvfs_mux[i].rc].rc_base) {
			MMDVFS_ERR("get rc_base failed, rc_id:%hhu", mmdvfs_mux[i].rc);
			ret = -1;
			continue;
		}

		mux_curr = readl(mmdvfs_rc[mmdvfs_mux[i].rc].rc_base + (mmdvfs_mux[i].pos / 8) * 0x4 + RC_MUX_SEL_CURR_ENABLE);
		mux_level[i] = (mux_curr >> ((mmdvfs_mux[i].pos % 8) * 0x4)) & 0xF;
	}

	//user vote level
	for (i = 0; i < MMDVFS_V5_USER_NUM; i++) {
		u8 rc_id = mmdvfs_mux[mmdvfs_user[i].mux].rc;
		u8 pos = mmdvfs_mux[mmdvfs_user[i].mux].pos;
		u32 RC_XPUx_MUX_SEL = RC_XPU0_MUX_SEL + mmdvfs_user[i].xpu * 0x10 + (pos / 8) * 0x4;

		if (!mmdvfs_rc[rc_id].rc_base) {
			MMDVFS_ERR("get rc_base failed, rc_id:%hhu", i);
			ret = -1;
			continue;
		}

		MMDVFS_DBG("user_id:%d level:%d mux_id:%d level:%d rc_id:%d level:%d",
			i, (readl(mmdvfs_rc[rc_id].rc_base + RC_XPUx_MUX_SEL) >> ((pos % 8) * 0x4)) & 0xf,
			mmdvfs_user[i].mux, mux_level[mmdvfs_user[i].mux],
			rc_id, rc_level[rc_id]);
	}

	return ret;
}

int mmdvfs_mt6993_dfs_vote_by_xpu(const u8 user_id, const u8 level)
{
	u8 rc_id = mmdvfs_mux[mmdvfs_user[user_id].mux].rc;
	u8 pos = mmdvfs_mux[mmdvfs_user[user_id].mux].pos;
	u32 RC_XPUx_MUX_SEL_CLR = mmdvfs_user[user_id].xpu_ofs + (pos / 8) * 0xc;
	u32 RC_XPUx_MUX_SEL_PRE = RC_XPU0_MUX_SEL_PRE + mmdvfs_user[user_id].xpu * 0x10 + (pos / 8) * 0x4;
	u32 RC_XPUx_MUX_SEL = RC_XPU0_MUX_SEL + mmdvfs_user[user_id].xpu * 0x10 + (pos / 8) * 0x4;
	u8 irq, vote, targ, curr;

	MMDVFS_DBG("[%s:%d] user_id:%d level:%d", __func__, __LINE__, user_id, level);

	//RC_XPUx_MUX_SEL_CLR: clear vote
	writel(0xf << (pos % 8) * 0x4, mmdvfs_rc[rc_id].rc_base + RC_XPUx_MUX_SEL_CLR);
	//RC_XPUx_MUX_SEL_SET: vote
	writel(level << (pos % 8) * 0x4, mmdvfs_rc[rc_id].rc_base + RC_XPUx_MUX_SEL_CLR + 0x4);
	//RC_XPUx_MUX_SEL_UPDATE: MUX_SEL_PRE update to VMM_DVFSRC_XPUx_0_7_MUX_SEL (RO)
	writel(0xf << (pos % 8) * 0x4, mmdvfs_rc[rc_id].rc_base + RC_XPUx_MUX_SEL_CLR + 0x8);

	do {
		irq = (readl(mmdvfs_rc[rc_id].rc_base + RC_LEVEL_HEX) >> 15) & 0x1;
		vote = (readl(mmdvfs_rc[rc_id].rc_base + RC_XPUx_MUX_SEL) >> ((pos % 8) * 0x4)) & 0xf;
		targ = (readl(mmdvfs_rc[rc_id].rc_base + (pos / 8) * 0x4 + RC_MUX_SEL_ENABLE) >> ((pos % 8) * 0x4)) & 0xf;
		curr = (readl(mmdvfs_rc[rc_id].rc_base + (pos / 8) * 0x4 + RC_MUX_SEL_CURR_ENABLE) >> ((pos % 8) * 0x4)) & 0xf;
	} while (!(vote <= curr && (vote <= targ || ~irq)));

	MMDVFS_DBG("user_id:%d level:%d mux_sel_pre:%#010x mux_sel:%#010x irq:%hhu vote:%hhu targ:%hhu curr:%hhu",
		user_id, level, (readl(mmdvfs_rc[rc_id].rc_base + RC_XPUx_MUX_SEL_PRE) >> ((pos % 8) * 0x4)) & 0xf,
		(readl(mmdvfs_rc[rc_id].rc_base + RC_XPUx_MUX_SEL) >> ((pos % 8) * 0x4)) & 0xf,
		irq, vote, targ, curr);

	return 0;
}

static const struct of_device_id of_match_mmdvfs_mt6993_mux[] = {
	{
		.compatible = "mediatek,mtk-mmdvfs-v5-mux",
		.data = &mmdvfs_data_mt6993,
	}, {}
};

static struct platform_driver mmdvfs_mt6993_mux_drv = {
	.probe = mmdvfs_v5_mux_probe,
	.driver = {
		.name = "mtk-mmdvfs-mt6993-mux",
		.of_match_table = of_match_mmdvfs_mt6993_mux,
	},
};

static int __init mmdvfs_init(void)
{
	return platform_driver_register(&mmdvfs_mt6993_mux_drv);
}

static void __exit mmdvfs_exit(void)
{
	platform_driver_unregister(&mmdvfs_mt6993_mux_drv);
}

module_init(mmdvfs_init);
module_exit(mmdvfs_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek MMDVFS MT6993");
MODULE_AUTHOR("MediaTek Inc.");
