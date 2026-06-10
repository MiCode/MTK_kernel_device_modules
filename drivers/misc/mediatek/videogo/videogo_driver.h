/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef MTK_VIDEOGO_H
#define MTK_VIDEOGO_H

#include <linux/list.h>

#define DEVICE_NAME "videogo"
#define CLASS_NAME "videogo_class"
#define VGO_IOCTL_SET_PROCTIME _IOWR('v', 1, struct inst_data_user)
#define VGO_IOCTL_GET _IOR('a', 'a', struct vgo_powerhal_info*)
#define MAX_POLICY 8


#define TARGET_FPS  (60ULL)
#define FHD_SIZE    (1920*1080)
#define TARGET_FPS_CHECKER(val0, val1, val2) \
	(((unsigned long long)(val0) * 100ULL) <= \
	((unsigned long long)(val1) * (100ULL + (unsigned long long)(val2))))

extern void mtk_vcodec_vgo_send(int type, void *data);
extern int videogo_active_fn(void *arg);
extern int enforce_ct_to_vip(int val, int caller_id);

enum codec_type {
	VDEC,
	VENC,
	MAX_CODEC_TYPE
};

enum scenario_type {
	VGO_IDLE = 0,
	VGO_TRANSCODING,
	VGO_VP_FEW_INST
};

enum work_type {
	VGO_REL_RUNNABLE_BOOST_ENABLE,
	VGO_REL_CGRP
};

enum vgo_policy_type {
	VGO_CPU_FREQ_MIN,
	VGO_GPU_FREQ_MIN,
	VGO_MARGIN_CONTROL_0,
	VGO_RUNNABLE_BOOST_DISABLE,
	VGO_RUNNABLE_BOOST_ENABLE,
	VGO_UCLAMP_MIN_TA,
	VGO_UTIL_EST_BOOST,
	VGO_RT_NON_IDLE_PREEMPT,
	VGO_VDEC_TASK_TURBO_PER_TASK_VIP,
	VGO_VENC_TASK_TURBO_PER_TASK_VIP,
	VGO_BTASK_UP_THRESH_CLUSTER_0,
	VGO_CPUCORE_MIN_CLUSTER_0,
	VGO_CPU_PF_DISABLE_0,
	VGO_WLC_WCE_DISABLE,
	VGO_CPU_BUSY_THRES_0,
	VGO_CPU_USAGE_THRES_0,
	VGO_CT_TO_VIP,
	VGO_POLICY_MAX
};

enum vgo_policy_exec_mode {
	VGO_POLICY_EXEC_ONCE,
	VGO_POLICY_EXEC_ALEAYS
};

enum vgo_scenario_type {
	VGO_VCODEC_PERF,
	VGO_VP_LOOM,
	VGO_VP,
	VGO_TRANS
};

struct vgo_delay_work {
	struct delayed_work delayed_work;
	enum work_type type;
	int inst_type;
	int ctx_id;
};

struct vgo_powerhal_info {
	enum vgo_policy_type type;
	int data[3];
};

struct data_entry {
	int type;
	void *data;
	struct list_head list;
};

struct inst_perf_info {
	int inst_type;
	int ctx_id;
	int avg_time;
	int max_time;
	int min_time;
	int count;
	int reserved;
};

struct inst_data_user {
	int inst_id;
	int module_id;
	int codec_id;
	int ctx_id;
	int avg_proc_time;
	int max_proc_time;
	int min_proc_time;
	int count;
};

// VGO_RECV_INSTANCE_UPDATE
struct inst_node {
	int inst_type;      /* VDEC/VENC */
	int ctx_id;
	int caller_pid;
	int fourcc;
	int oprate;         /* Set from Framework */
	int oprate_avdvfs;
	int oprate_vgo;
	int width;
	int height;
	int hw_proc_time[3];
	int post_proc_time;
	struct list_head list;
};

struct task_tgid {
	pid_t worker_tgid[2];
	pid_t ipi_recv_tgid[2];
	pid_t c2_tgid;
};

struct scenario_policy {
	enum vgo_scenario_type type_id;
	const char *name;
	int policy_ids[MAX_POLICY];	// MAX Policy 8
	bool (*check_func)(void);
};

static const char * const thermal_zones[] = {
	"cpu-little-core0",
	"cpu-little-core1",
	"cpu-little-core2",
	"cpu-little-core3",
	"cpu-medium-core4-0",
	"cpu-medium-core4-1",
	"cpu-medium-core5-0",
	"cpu-medium-core5-1",
	"cpu-medium-core6-0",
	"cpu-medium-core6-1",
	"cpu-big-core7-0",
	"cpu-big-core7-1"
};
#endif /* _MTK_VIDEOGO_H */
