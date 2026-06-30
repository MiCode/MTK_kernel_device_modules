// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include "sbe_ioctl.h"

#define TAG "SBE_IOCTL"

int (*sbe_notify_webview_policy_fp)(int pid,  char *name,
	unsigned long mask, int start, char *specific_name, int num);
EXPORT_SYMBOL_GPL(sbe_notify_webview_policy_fp);
int (*sbe_notify_hwui_frame_hint_fp)(int qudeq,
		int pid, int frameID,
		unsigned long long id,
		int dep_mode, char *dep_name, int dep_num, long long frame_flags);
EXPORT_SYMBOL_GPL(sbe_notify_hwui_frame_hint_fp);
void (*sbe_notify_rescue_fp)(int pid, int start, int enhance,
		int rescue_type, unsigned long long rescue_target, unsigned long long frameID);
EXPORT_SYMBOL_GPL(sbe_notify_rescue_fp);
void (*sbe_consistency_policy_fp)(int start, int pid, int uclamp_min, int uclamp_max);
EXPORT_SYMBOL_GPL(sbe_consistency_policy_fp);
int (*sbe_notify_smart_launch_algorithm_fp)(int feedback_time,
		int target_time, int pre_opp, int capabilty_ration);
EXPORT_SYMBOL_GPL(sbe_notify_smart_launch_algorithm_fp);
int (*sbe_set_sbb_fp)(int pid, int set, int active_ratio);
EXPORT_SYMBOL_GPL(sbe_set_sbb_fp);

static struct proc_dir_entry *perfmgr_root;

static unsigned long sbe_copy_from_user(void *pvTo,
		const void __user *pvFrom, unsigned long ulBytes)
{
	if (access_ok(pvFrom, ulBytes))
		return __copy_from_user(pvTo, pvFrom, ulBytes);

	return ulBytes;
}

static unsigned long sbe_copy_to_user(void __user *pvTo,
		const void *pvFrom, unsigned long ulBytes)
{
	if (access_ok(pvTo, ulBytes))
		return __copy_to_user(pvTo, pvFrom, ulBytes);

	return ulBytes;
}

static int device_show(struct seq_file *m, void *v)
{
	return 0;
}

static int device_open(struct inode *inode, struct file *file)
{
	return single_open(file, device_show, inode->i_private);
}

static long device_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	ssize_t ret = 0;
	struct _SBE_IOCTL_PACKAGE *msgKM_SBE = NULL, *msgUM_SBE = NULL;
	struct _SBE_IOCTL_PACKAGE smsgKM_SBE;
	struct _SMART_LAUNCH_PACKAGE *smart_launch_p;
	struct _SMART_LAUNCH_PACKAGE smart_launch;

	if (cmd == SMART_LAUNCH_ALGORITHM) {
		smart_launch_p = (struct _SMART_LAUNCH_PACKAGE *)arg;
		if (sbe_copy_from_user(&smart_launch, smart_launch_p,
					sizeof(struct _SMART_LAUNCH_PACKAGE))) {
			ret = -EFAULT;
			goto ret_ioctl;
		}

		if (sbe_notify_smart_launch_algorithm_fp)
			ret = sbe_notify_smart_launch_algorithm_fp(smart_launch.feedback_time,
				smart_launch.target_time, smart_launch.pre_opp, smart_launch.capabilty_ration);

		smart_launch.next_opp = ret;
		sbe_copy_to_user(smart_launch_p, &smart_launch, sizeof(struct _SMART_LAUNCH_PACKAGE));

		goto ret_ioctl;
	}

	msgUM_SBE = (struct _SBE_IOCTL_PACKAGE *)arg;
	msgKM_SBE = &smsgKM_SBE;
	if (sbe_copy_from_user(msgKM_SBE, msgUM_SBE,
				sizeof(struct _SBE_IOCTL_PACKAGE))) {
		ret = -EFAULT;
		goto ret_ioctl;
	}

	switch (cmd) {
	case SBE_SET_WEBVIEW_POLICY:
		if (sbe_notify_webview_policy_fp)
			ret = sbe_notify_webview_policy_fp(msgKM_SBE->pid, msgKM_SBE->name,
					msgKM_SBE->mask, msgKM_SBE->start,
					msgKM_SBE->specific_name, msgKM_SBE->num);
		break;
	case SBE_SET_HWUI_POLICY:
		if (sbe_notify_hwui_frame_hint_fp)
			msgKM_SBE->blc = sbe_notify_hwui_frame_hint_fp(msgKM_SBE->start,
				msgKM_SBE->rtid, msgKM_SBE->frame_id, msgKM_SBE->identifier,
				msgKM_SBE->mode, msgKM_SBE->specific_name, msgKM_SBE->num, msgKM_SBE->mask);
		sbe_copy_to_user(msgUM_SBE, msgKM_SBE,
			sizeof(struct _SBE_IOCTL_PACKAGE));
		break;
	case SBE_SET_RESCUE:
		if (sbe_notify_rescue_fp)
			sbe_notify_rescue_fp(msgKM_SBE->pid, msgKM_SBE->start, msgKM_SBE->floor,
				msgKM_SBE->identifier, msgKM_SBE->time, msgKM_SBE->frame_id);
		break;
	case SBE_CONSISTENCY:
		if (sbe_consistency_policy_fp) {
			sbe_consistency_policy_fp(msgKM_SBE->start, msgKM_SBE->pid,
				msgKM_SBE->uclamp_min, msgKM_SBE->uclamp_max);
		}
		break;
	case SBE_SET_SBB:
		if (sbe_set_sbb_fp)
			sbe_set_sbb_fp(msgKM_SBE->pid, msgKM_SBE->start, msgKM_SBE->mode);
		break;
	default:
		pr_debug(TAG "%s %d: unknown SBE cmd %x\n",
			__FILE__, __LINE__, cmd);
		ret = -1;
		break;
	}

ret_ioctl:
	return ret;
}

static const struct proc_ops sbe_Fops = {
#if IS_ENABLED(CONFIG_COMPAT)
	.proc_compat_ioctl = device_ioctl,
#endif
	.proc_ioctl = device_ioctl,
	.proc_open = device_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static void __exit exit_sbe_ioctl(void) {}

static int __init init_sbe_ioctl(void)
{
	struct proc_dir_entry *pe, *parent;
	int ret_val = 0;

	pr_debug(TAG"Start to init sbe_ioctl driver\n");

	parent = proc_mkdir("perfmgr_sbe", NULL);
	perfmgr_root = parent;

	pe = proc_create("sbe_ioctl", 0664, parent, &sbe_Fops);
	if (!pe) {
		pr_debug(TAG"%s failed with %d\n",
				"Creating file node ",
				ret_val);
		ret_val = -ENOMEM;
		goto out_wq;
	}

	pr_debug(TAG"init sbe_ioctl driver done\n");

	return 0;

out_wq:
	return ret_val;
}

module_init(init_sbe_ioctl);
module_exit(exit_sbe_ioctl);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek SBE ioctl");
MODULE_AUTHOR("MediaTek Inc.");
