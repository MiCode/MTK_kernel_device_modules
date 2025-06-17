/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef _CORE_CTL_H
#define _CORE_CTL_H

struct _CORE_CTL_PACKAGE {
	union {
		__u32 cid;
		__u32 cpu;
	};
	union {
		__u32 min;
		__u32 is_pause;
		__u32 throttle_ms;
		__u32 not_preferred_cpus;
		__u32 boost;
		__u32 thres;
		__u32 enable_policy;
	};
	__u32 max;
};

struct core_ctl_notif_data {
	unsigned int cpu;
	unsigned int is_pause;
	unsigned int paused_mask;
	unsigned int online_mask;
};

enum{
	CLEARED_FORCE_PAUSE  		= 0,
	UNKNOWN_FORCE_PAUSE  		= 1,
	CLIENT_KERNEL_FORCE_PAUSE 	= 2,
	POWERHAL_FORCE_PAUSE 		= 4,
	POWER_THROTTLE_FORCE_PAUSE	= 8,
	THERMAL_FORCE_PAUSE  		= 16,
	MAX_FORCE_PAUSE_TYPE	= 32,
};

enum {
	SYSNODE = 0,
	POWERHAL,
	CAMERA,
	UX,
	GAME,
	MAX_DEMAND_REQUESTER
};

enum {
	DISABLE = 0,
	NORMAL_MODE,
	CAMERA_MODE,
	UX_MODE,
	GAME_MODE
};

#define CORE_CTL_FORCE_RESUME_CPU               _IOW('g', 1,  struct _CORE_CTL_PACKAGE)
#define CORE_CTL_FORCE_PAUSE_CPU                _IOW('g', 2,  struct _CORE_CTL_PACKAGE)
#define CORE_CTL_SET_OFFLINE_THROTTLE_MS        _IOW('g', 3,  struct _CORE_CTL_PACKAGE)
#define CORE_CTL_SET_LIMIT_CPUS                 _IOW('g', 4,  struct _CORE_CTL_PACKAGE)
#define CORE_CTL_SET_NOT_PREFERRED              _IOW('g', 5, struct _CORE_CTL_PACKAGE)
#define CORE_CTL_SET_BOOST                      _IOW('g', 6, struct _CORE_CTL_PACKAGE)
#define CORE_CTL_SET_UP_THRES                   _IOW('g', 7, struct _CORE_CTL_PACKAGE)
#define CORE_CTL_ENABLE_POLICY                  _IOW('g', 8, struct _CORE_CTL_PACKAGE)
#define CORE_CTL_SET_CPU_BUSY_THRES             _IOW('g', 9, struct _CORE_CTL_PACKAGE)
#define CORE_CTL_SET_CPU_NONBUSY_THRES          _IOW('g', 10, struct _CORE_CTL_PACKAGE)
#define CORE_CTL_SET_NR_TASK_THRES              _IOW('g', 11, struct _CORE_CTL_PACKAGE)
#define CORE_CTL_SET_FREQ_MIN_THRES             _IOW('g', 12, struct _CORE_CTL_PACKAGE)
#define CORE_CTL_SET_ACT_LOAD_THRES             _IOW('g', 13, struct _CORE_CTL_PACKAGE)
#define CORE_CTL_SET_RT_NR_TASK_THRES           _IOW('g', 14, struct _CORE_CTL_PACKAGE)

extern void core_ctl_notifier_register(struct notifier_block *n);
extern void core_ctl_notifier_unregister(struct notifier_block *n);
extern int core_ctl_get_min_cpus(unsigned int cid);
extern int core_ctl_set_min_cpus(unsigned int cid, unsigned int min, int requester, unsigned int have_demand);
extern int core_ctl_get_max_cpus(unsigned int cid);
extern int core_ctl_set_max_cpus(unsigned int cid, unsigned int max, int requester, unsigned int have_demand);
extern int pd_freq2opp(int cpu, int opp, int quant, int wl);
extern int core_ctl_consider_VIP(unsigned int enable);

#endif /* _CORE_CTL_H */
