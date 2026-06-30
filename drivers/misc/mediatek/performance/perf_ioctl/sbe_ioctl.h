/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef SBE_IOCTL_H
#define SBE_IOCTL_H
#include <linux/jiffies.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/utsname.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/uaccess.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/notifier.h>
#include <linux/suspend.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/hrtimer.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/ioctl.h>

struct _SBE_IOCTL_PACKAGE {
	__s32 num;
	__u32 pid;
	__u32 rtid;
	__u32 start;
	__u32 blc;
	__u32 mode;
	__u32 floor;
	__u64 frame_id;
	__u64 identifier;
	__u64 mask;
	__u64 time;
	__u8 name[16];
	__u8 specific_name[1000];
	__u32 uclamp_min;
	__u32 uclamp_max;
};

struct _SMART_LAUNCH_PACKAGE {
	int target_time;
	int feedback_time;
	int pre_opp;
	int next_opp;
	int capabilty_ration;
};

#define SBE_SET_WEBVIEW_POLICY     _IOW('g', 1, struct _SBE_IOCTL_PACKAGE)
#define SBE_SET_HWUI_POLICY        _IOW('g', 2, struct _SBE_IOCTL_PACKAGE)
#define SBE_SET_RESCUE             _IOW('g', 3, struct _SBE_IOCTL_PACKAGE)
#define SBE_CONSISTENCY            _IOW('g', 4, struct _SBE_IOCTL_PACKAGE)
#define SBE_SET_SBB                _IOW('g', 5, struct _SBE_IOCTL_PACKAGE)
#define SMART_LAUNCH_ALGORITHM     _IOW('g', 1, struct _SMART_LAUNCH_PACKAGE)

#endif

