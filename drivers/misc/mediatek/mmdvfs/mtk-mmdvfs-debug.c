// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/printk.h>

#include "mtk-mmdvfs-debug.h"

struct mmdvfs_debug_ops ops;

void mmdvfs_debug_ops_set(struct mmdvfs_debug_ops *_ops)
{
	ops = *_ops;
}
EXPORT_SYMBOL_GPL(mmdvfs_debug_ops_set);

int mtk_mmdvfs_debug_force_vcore_notify(const u32 val)
{
	if (!ops.force_vcore_fp) {
		pr_notice("[mmdvfs_dbg][dbg]%s:%d: without fp\n", __func__, __LINE__);
		return -EINVAL;
	}

	return ops.force_vcore_fp(val);
}
EXPORT_SYMBOL_GPL(mtk_mmdvfs_debug_force_vcore_notify);

int mmdvfs_debug_status_dump(struct seq_file *file)
{
	if (!ops.status_dump_fp) {
		pr_notice("[mmdvfs_dbg][dbg]%s:%d: without fp\n", __func__, __LINE__);
		return -EINVAL;
	}

	return ops.status_dump_fp(file);
}
EXPORT_SYMBOL_GPL(mmdvfs_debug_status_dump);

static int mmdvfs_debug_set_force_step(const char *val, const struct kernel_param *kp)
{
	if (!ops.force_step_fp) {
		pr_notice("[mmdvfs_dbg][dbg]%s:%d: without fp\n", __func__, __LINE__);
		return -EINVAL;
	}

	return ops.force_step_fp(val, kp);
}

static struct kernel_param_ops mmdvfs_debug_set_force_step_ops = {
	.set = mmdvfs_debug_set_force_step,
};
module_param_cb(force_step, &mmdvfs_debug_set_force_step_ops, NULL, 0644);
MODULE_PARM_DESC(force_step, "force mmdvfs to specified step");

static int mmdvfs_debug_set_vote_step(const char *val, const struct kernel_param *kp)
{
	if (!ops.vote_step_fp) {
		pr_notice("[mmdvfs_dbg][dbg]%s:%d: without fp\n", __func__, __LINE__);
		return -EINVAL;
	}

	return ops.vote_step_fp(val, kp);
}

static struct kernel_param_ops mmdvfs_debug_set_vote_step_ops = {
	.set = mmdvfs_debug_set_vote_step,
};
module_param_cb(vote_step, &mmdvfs_debug_set_vote_step_ops, NULL, 0644);
MODULE_PARM_DESC(vote_step, "vote mmdvfs to specified step");

MODULE_DESCRIPTION("MMDVFS Debug Driver");
MODULE_AUTHOR("Anthony Huang<anthony.huang@mediatek.com>");
MODULE_LICENSE("GPL");

