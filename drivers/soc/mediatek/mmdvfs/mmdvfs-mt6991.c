// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include "mmdvfs-mt6991.h"

int mmdvfs_mt6993_dfs_vote_by_xpu(const u8 user_id, const u8 level)
{
	u8 rc_id = mmdvfs_mux[mmdvfs_user[user_id].mux].rc;
	u8 pos = mmdvfs_mux[mmdvfs_user[user_id].mux].pos;
	u32 RC_XPUx_MUX_SEL_CLR = mmdvfs_user[user_id].xpu_ofs + (pos / 8) * 0xc;
	u32 RC_XPUx_MUX_SEL_PRE = RC_XPU0_MUX_SEL_PRE + mmdvfs_user[user_id].xpu * 0x10 + (pos / 8) * 0x4;
	u32 RC_XPUx_MUX_SEL = RC_XPU0_MUX_SEL + mmdvfs_user[user_id].xpu * 0x10 + (pos / 8) * 0x4;

	MMDVFS_DBG("[%s:%d] user_id:%d level:%d", __func__, __LINE__, user_id, level);
	return 0;

	//RC_XPUx_MUX_SEL_CLR: clear vote
	writel(0xf << (pos % 8) * 0x4, mmdvfs_rc[rc_id].rc_base + RC_XPUx_MUX_SEL_CLR);
	//RC_XPUx_MUX_SEL_SET: vote
	writel(level << (pos % 8) * 0x4, mmdvfs_rc[rc_id].rc_base + RC_XPUx_MUX_SEL_CLR + 0x4);
	//RC_XPUx_MUX_SEL_UPDATE: MUX_SEL_PRE update to VMM_DVFSRC_XPUx_0_7_MUX_SEL (RO)
	writel(0xf << (pos % 8) * 0x4, mmdvfs_rc[rc_id].rc_base + RC_XPUx_MUX_SEL_CLR + 0x8);

	MMDVFS_DBG("user_id:%d level:%d mux_sel_pre:%#010x mux_sel:%#010x",
		user_id, level, (readl(mmdvfs_rc[rc_id].rc_base + RC_XPUx_MUX_SEL_PRE) >> ((pos % 8) * 0x4)) & 0xf,
		(readl(mmdvfs_rc[rc_id].rc_base + RC_XPUx_MUX_SEL) >> ((pos % 8) * 0x4)) & 0xf);

	//Todo: polling dfs/dvs done

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
