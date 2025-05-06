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

struct mmdvfs_res_mbrain_debug_ops *get_mmdvfs_mbrain_usr_dbg_ops(void)
{
	if (!ops.mmdvfs_mbrain_usr_fp) {
		pr_notice("[mmdvfs_dbg][dbg]%s:%d: without fp\n", __func__, __LINE__);
		return NULL;
	}

	return ops.mmdvfs_mbrain_usr_fp();
}
EXPORT_SYMBOL_GPL(get_mmdvfs_mbrain_usr_dbg_ops);

struct mmdvfs_res_mbrain_debug_ops *get_mmdvfs_mbrain_dbg_ops(void)
{
	if (!ops.mmdvfs_mbrain_fp) {
		pr_notice("[mmdvfs_dbg][dbg]%s:%d: without fp\n", __func__, __LINE__);
		return NULL;
	}

	return ops.mmdvfs_mbrain_fp();
}
EXPORT_SYMBOL_GPL(get_mmdvfs_mbrain_dbg_ops);

int mmdvfs_debug_status_dump(struct seq_file *file)
{
	if (!ops.status_dump_fp) {
		pr_notice("[mmdvfs_dbg][dbg]%s:%d: without fp\n", __func__, __LINE__);
		return -EINVAL;
	}

	return ops.status_dump_fp(file);
}
EXPORT_SYMBOL_GPL(mmdvfs_debug_status_dump);

int mmdvfs_stop_record(void)
{
	if (!ops.record_snapshot_fp) {
		pr_notice("[mmdvfs_dbg][dbg]%s:%d: without fp\n", __func__, __LINE__);
		return -EINVAL;
	}

	return ops.record_snapshot_fp();
}
EXPORT_SYMBOL_GPL(mmdvfs_stop_record);

static int mmdvfs_debug_set_force_step(const char *val, const struct kernel_param *kp)
{
	if (!ops.force_step_fp) {
		pr_notice("[mmdvfs_dbg][dbg]%s:%d: without fp\n", __func__, __LINE__);
		return -EINVAL;
	}

	return ops.force_step_fp(val, kp);
}

static const struct kernel_param_ops mmdvfs_debug_set_force_step_ops = {
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

static const struct kernel_param_ops mmdvfs_debug_set_vote_step_ops = {
	.set = mmdvfs_debug_set_vote_step,
};
module_param_cb(vote_step, &mmdvfs_debug_set_vote_step_ops, NULL, 0644);
MODULE_PARM_DESC(vote_step, "vote mmdvfs to specified step");

static int mmdvfs_debug_ap_set_rate(const char *val, const struct kernel_param *kp)
{
	if (!ops.ap_set_rate_fp) {
		pr_notice("[mmdvfs_dbg][dbg]%s:%d: without fp\n", __func__, __LINE__);
		return -EINVAL;
	}

	return ops.ap_set_rate_fp(val, kp);
}

static const struct kernel_param_ops mmdvfs_debug_ap_set_rate_ops = {
	.set = mmdvfs_debug_ap_set_rate,
};
module_param_cb(ap_set_rate, &mmdvfs_debug_ap_set_rate_ops, NULL, 0644);
MODULE_PARM_DESC(ap_set_rate, "set rate from dummy ap user");

int mtk_mmdvfs_debug_release_step0(void)
{
	if (!ops.release_step_fp) {
		pr_notice("[mmdvfs_dbg][dbg]%s:%d: without fp\n", __func__, __LINE__);
		return -EINVAL;
	}

	ops.release_step_fp();
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_mmdvfs_debug_release_step0);

static int mmdvfs_debug_set_ftrace(const char *val,
	const struct kernel_param *kp)
{
	if (!ops.mmdvfs_ftrace_fp) {
		pr_notice("[mmdvfs_dbg][dbg]%s:%d: without fp\n", __func__, __LINE__);
		return -EINVAL;
	}

	return ops.mmdvfs_ftrace_fp(val, kp);
}
static const struct kernel_param_ops mmdvfs_debug_set_ftrace_ops = {
	.set = mmdvfs_debug_set_ftrace,
};
module_param_cb(ftrace, &mmdvfs_debug_set_ftrace_ops, NULL, 0644);
MODULE_PARM_DESC(ftrace, "mmdvfs ftrace log");

MODULE_DESCRIPTION("MMDVFS Debug Driver");
MODULE_AUTHOR("Anthony Huang<anthony.huang@mediatek.com>");
MODULE_LICENSE("GPL");

