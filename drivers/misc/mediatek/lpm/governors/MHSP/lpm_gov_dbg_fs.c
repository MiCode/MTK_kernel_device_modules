// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/cpuidle.h>
#include <linux/module.h>

#include <mtk_gov_sysfs.h>

#include "lpm-mhsp.h"

#if IS_ENABLED(CONFIG_MTK_CPU_RETENTION_SUPPORT)
static ssize_t gov_reten_read(char *ToUserBuf, size_t sz, void *priv)
{
	char *p = ToUserBuf;

	if (!p)
		return -EINVAL;

	mtk_dbg_gov_log("\nretention enable: %u\n\n", get_reten_status());

	return p - ToUserBuf;
}

static ssize_t gov_reten_write(char *FromUserBuf, size_t sz, void *priv)
{
	uint32_t val;

	if (!FromUserBuf)
		return -EINVAL;

	if (!kstrtouint(FromUserBuf, 10, &val)) {
		set_reten_status(val);
		return sz;
	}

	return -EINVAL;
}

static const struct mtk_lp_sysfs_op gov_reten_fops = {
	.fs_read = gov_reten_read,
	.fs_write = gov_reten_write,
};
#endif

int lpm_gov_fs_init(void)
{
#if IS_ENABLED(CONFIG_MTK_CPU_RETENTION_SUPPORT)
	mtk_gov_sysfs_root_entry_create();

	mtk_gov_sysfs_entry_node_add("reten_en", MTK_GOV_SYS_FS_MODE,
				     &gov_reten_fops, NULL);
#endif

	return 0;
}
