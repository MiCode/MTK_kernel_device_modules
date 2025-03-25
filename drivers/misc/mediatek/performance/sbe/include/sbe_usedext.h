/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __SBE_USEDEXT_H__
#define __SBE_USEDEXT_H__

enum SBE_NOTIFIER_PUSH_TYPE {
	SBE_NOTIFIER_DISPLAY_RATE,
	SBE_NOTIFIER_RESCUE,
	SBE_NOTIFIER_HWUI_FRAME_HINT,
	SBE_NOTIFIER_WEBVIEW_POLICY,
	SBE_NOTIFIER_SET_SBB,
};

enum SBE_ACTION_MASK {
	SBE_CPU_CONTROL,
	SBE_DISPLAY_TARGET_FPS,
	SBE_PAGE_WEBVIEW,
	SBE_PAGE_FLUTTER,
	SBE_PAGE_MULTI_WINDOW,
	SBE_PAGE_APPBRAND,
	SBE_RUNNING_CHECK,
	SBE_RUNNING_QUERY,
	SBE_NON_HWUI,
	SBE_HWUI,
	SBE_CLEAR_SCROLLING_INFO,
	SBE_MOVEING,
	SBE_FLING,
	SBE_SCROLLING,
	SBE_CLEAR_RENDERS,
};

struct SBE_NOTIFIER_PUSH_TAG {
	enum SBE_NOTIFIER_PUSH_TYPE ePushType;

	char name[16];
	char specific_name[1000];
	int pid;
	int start;
	int num;
	int mode;
	int display_rate;
	int enable;
	int enhance;
	int rescue_type;
	unsigned long mask;
	long long frameID;
	long long frame_flags;
	unsigned long long cur_ts;
	unsigned long long identifier;
	unsigned long long rescue_target;

	struct list_head queue_list;
};

extern int (*sbe_notify_smart_launch_algorithm_fp)(int feedback_time,
	int target_time, int pre_opp, int capabilty_ration);
extern int (*sbe_notify_webview_policy_fp)(int pid,  char *name,
	unsigned long mask, int start, char *specific_name, int num);
extern int (*sbe_notify_hwui_frame_hint_fp)(int qudeq,
		int pid, int frameID,
		unsigned long long id,
		int dep_mode, char *dep_name, int dep_num, long long frame_flags);
extern void (*sbe_notify_rescue_fp)(int pid, int start, int enhance,
		int rescue_type, unsigned long long rescue_target, unsigned long long frameID);
extern void (*sbe_consistency_policy_fp)(int start, int pid, int uclamp_min, int uclamp_max);
extern int (*sbe_set_sbb_fp)(int pid, int set, int active_ratio);

#endif