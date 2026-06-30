/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */
#ifndef VIDEOGO_POLICY_TABLE_H
#define VIDEOGO_POLICY_TABLE_H

#include <linux/jiffies.h>
#include "videogo_driver.h"
#include "videogo_param_config.h"
#include "videogo_service_utils.h"
#include "videogo_utils.h"
#include "pf_ctrl.h"
#include "slbc_sdk.h"

#define LOG_MSG_LEN 32

static struct task_tgid video_task;

typedef void (*cfg_set)(int, void *, const char *);
typedef void (*cfg_unset)(int, const char *);

static int enable = 1;
static int disable;
static int margin_default_val[3];

extern int isTranscoding;
extern int target_fps_count[MAX_CODEC_TYPE];
extern int alive_count[MAX_CODEC_TYPE];

struct vgo_policy {
	int cmd_id;
	const char *cmd_name;
	int *val[3];
	int val_cnt;
	bool set;
	bool *enable;
	cfg_set set_func;
	cfg_unset unset_func;
	atomic_t ref_count;
	enum vgo_policy_exec_mode exec_mode;
};

bool is_vcodec_perf(void)
{
	return mtk_vgo_sc_vcodec_perf;
}

bool is_vp(void)
{
	int total_vdec_target_fps = target_fps_count[VDEC];
	int total_vdec = alive_count[VDEC];
	int total_venc = alive_count[VENC];

	mtk_vgo_info("%d total_vdec:%d total_venc:%d, %d",
		 mtk_vgo_sc_vp_lp, total_vdec, total_venc, total_vdec_target_fps);
	return mtk_vgo_sc_vp_lp &&
		(total_vdec_target_fps > 0 && total_vdec_target_fps <= 2 &&
		!total_venc && total_vdec_target_fps == total_vdec);
}

static bool is_loom(void)
{
	return mtk_vgo_sc_vp_lp_loom && is_vp();
}

static bool is_trans(void)
{
	return mtk_vgo_sc_trans && isTranscoding;
}

static struct scenario_policy scenario_table[] = {
	{ VGO_VCODEC_PERF, "VGO_VCODEC_PERF",
		{ -1 }, is_vcodec_perf
	},
	{ VGO_VP_LOOM, "VGO_VP_LOOM",
		{ VGO_VDEC_TASK_TURBO_PER_TASK_VIP,
		VGO_CPUCORE_MIN_CLUSTER_0,
		VGO_CPU_BUSY_THRES_0,
		VGO_BTASK_UP_THRESH_CLUSTER_0,
		VGO_CPU_USAGE_THRES_0, -1 }, is_loom
	},
	{ VGO_VP, "VGO_VP",
		{ VGO_RUNNABLE_BOOST_DISABLE,
		VGO_RT_NON_IDLE_PREEMPT,
		VGO_MARGIN_CONTROL_0,
		VGO_CPU_PF_DISABLE_0,
		VGO_WLC_WCE_DISABLE, -1 }, is_vp
	},
	{ VGO_TRANS, "VGO_TRANS",
		{ VGO_UCLAMP_MIN_TA,
		VGO_GPU_FREQ_MIN,
		VGO_CT_TO_VIP, -1 }, is_trans
	}
};

static void set_service_info(int cmd_id, void *vals, const char *cmd_name)
{
	int ret;
	int *arr = (int *)vals;
	char log_msg[LOG_MSG_LEN];

	ret = snprintf(log_msg, LOG_MSG_LEN, "acq %s", cmd_name);
	if (ret < 0 || ret >= LOG_MSG_LEN) {
		strscpy(log_msg, "log error", LOG_MSG_LEN);
		log_msg[LOG_MSG_LEN - 1] = '\0';
	}
	send_service_info(log_msg, cmd_id, arr[0], arr[1], arr[2]);
}

static void set_cpu_pf_ctrl(int cmd_id, void *vals, const char *cmd_name)
{
	int ret;

	if (mtk_set_pf_ctrl_enable(true, PF_CTRL_USER_VP) != 0)
		mtk_vgo_err("Failed to mtk_set_pf_ctrl_enable");

	ret = mtk_get_pf_ctrl_enable();
	mtk_vgo_debug("acq %s %s: %d", cmd_name, ret ? "enable" : "disable", ret);
}

static void set_slc_wce_ctrl(int cmd_id, void *vals, const char *cmd_name)
{
	slbc_disable_dcc(true); // 1: disable WCE, 0: enable (default)
	mtk_vgo_debug("acq %s", cmd_name);
}

static void set_cpu_usage_monitor(int cmd_id, void *vals, const char *cmd_name)
{
	static unsigned long last_jiffies;
	unsigned long now = jiffies;
	int cpu_usage_0 = 0, i, last_val;

	if (last_jiffies && time_before(now, last_jiffies + msecs_to_jiffies(500)))
		return;

//	if (!margin_default_val[0])
//		memcpy(margin_default_val, mtk_vgo_margin_ctrl_val, sizeof(margin_default_val));

	last_jiffies = now;
	last_val = mtk_vgo_margin_ctrl_val[2];

	// sum cpu0~3 usage
	for (i = 0; i < 4; i++)
		cpu_usage_0 += get_cpu_usage(i);

	if (cpu_usage_0 > 50 && cpu_usage_0 < mtk_vgo_cpu_usage_thres_0_val &&
		mtk_vgo_margin_ctrl_val[2] > -54)
		mtk_vgo_margin_ctrl_val[2] -= 15;

	// lower_bound is -54, util: 100/(100-(-54)) = 65%
	if (mtk_vgo_margin_ctrl_val[2] < -54)
		mtk_vgo_margin_ctrl_val[2] = -54;

	if (mtk_vgo_margin_ctrl_val[2] != last_val)
		send_service_info("acq magin(cpu_usage)", VGO_MARGIN_CONTROL_0, mtk_vgo_margin_ctrl_val[0],
			mtk_vgo_margin_ctrl_val[1], mtk_vgo_margin_ctrl_val[2]);

	mtk_vgo_debug("%s: %d, %d", cmd_name, cpu_usage_0, mtk_vgo_margin_ctrl_val[2]);
	mtk_vgo_debug("%d %d %d/%d %d %d", mtk_vgo_margin_ctrl_val[0], mtk_vgo_margin_ctrl_val[1],
		mtk_vgo_margin_ctrl_val[2], margin_default_val[0], margin_default_val[1],
		margin_default_val[2]);
}

static void set_ct_to_vip(int cmd_id, void *vals, const char *cmd_name)
{
	int *arr = (int *)vals;

	enforce_ct_to_vip(arr[0], 3);
	mtk_vgo_debug("acq ct_to_vip");
}

static void unset_service_info(int cmd_id, const char *cmd_name)
{
	int ret;
	char log_msg[LOG_MSG_LEN];

	ret = snprintf(log_msg, LOG_MSG_LEN, "rel %s", cmd_name);
	if (ret < 0 || ret >= LOG_MSG_LEN) {
		strscpy(log_msg, "log error", LOG_MSG_LEN);
		log_msg[LOG_MSG_LEN - 1] = '\0';
	}
	send_service_info(log_msg, cmd_id, -1, 0, 0);
}

static void unset_cpu_pf_ctrl(int cmd_id, const char *cmd_name)
{
	int ret;

	mtk_set_pf_ctrl_enable(false, PF_CTRL_USER_VP);

	ret = mtk_get_pf_ctrl_enable();
	mtk_vgo_debug("rel %s %s: %d", cmd_name, ret ? "enable" : "disable", ret);
}

static void unset_slc_wce_ctrl(int cmd_id, const char *cmd_name)
{
	slbc_disable_dcc(false);
	mtk_vgo_debug("rel %s", cmd_name);
}

static void unset_cpu_usage_monitor(int cmd_id, const char *cmd_name)
{
	mtk_vgo_margin_ctrl_val[2] = 0;
//	memcpy(mtk_vgo_margin_ctrl_val, margin_default_val, sizeof(mtk_vgo_margin_ctrl_val));
//	memset(margin_default_val, 0, sizeof(margin_default_val));
	mtk_vgo_debug("%d %d %d/%d %d %d", mtk_vgo_margin_ctrl_val[0], mtk_vgo_margin_ctrl_val[1],
		mtk_vgo_margin_ctrl_val[2], margin_default_val[0], margin_default_val[1],
		margin_default_val[2]);
}

static void unset_ct_to_vip(int cmd_id, const char *cmd_name)
{
	enforce_ct_to_vip(0, 3);
	mtk_vgo_debug("rel ct_to_vip");
}

static struct vgo_policy vgo_table[] = {
	{
		.cmd_id = VGO_CPU_FREQ_MIN,
		.cmd_name = "cpu_freq_min",
		.val = { NULL, NULL, NULL },
		.val_cnt = 0,
		.set = false,
		.enable = NULL,
		.set_func = NULL,
		.unset_func = NULL,
		.exec_mode = VGO_POLICY_EXEC_ONCE
	},
	{
		.cmd_id = VGO_GPU_FREQ_MIN,
		.cmd_name = "gpu_ferq_min_opp",
		.val = { &mtk_vgo_gpu_freq_min_opp, NULL, NULL },
		.val_cnt = 1,
		.set = false,
		.enable = &mtk_vgo_gpu_freq_min,
		.set_func = set_service_info,
		.unset_func = unset_service_info,
		.exec_mode = VGO_POLICY_EXEC_ONCE
	},
	{
		.cmd_id = VGO_MARGIN_CONTROL_0,
		.cmd_name = "margin_ctrl(headroom)",
		.val = { &mtk_vgo_margin_ctrl_val[0], &mtk_vgo_margin_ctrl_val[1],
			&mtk_vgo_margin_ctrl_val[2] },
		.val_cnt = 3,
		.set = false,
		.enable = &mtk_vgo_margin_ctrl,
		.set_func = set_service_info,
		.unset_func = unset_service_info,
		.exec_mode = VGO_POLICY_EXEC_ONCE
	},
	{
		.cmd_id = VGO_RUNNABLE_BOOST_DISABLE,
		.cmd_name = "runnable_boost_disable",
		.val = { &disable, NULL, NULL },
		.val_cnt = 1,
		.set = false,
		.enable = &mtk_vgo_runnable_boost_disable,
		.set_func = set_service_info,
		.unset_func = unset_service_info,
		.exec_mode = VGO_POLICY_EXEC_ONCE
	},
	{
		.cmd_id = VGO_RUNNABLE_BOOST_ENABLE,
		.cmd_name = "runnable_boost_enable",
		.val = { &enable, NULL, NULL },
		.val_cnt = 1,
		.set = false,
		.enable = &mtk_vgo_runnable_boost_enable,
		.set_func = set_service_info,
		.unset_func = unset_service_info,
		.exec_mode = VGO_POLICY_EXEC_ONCE
	},
	{
		.cmd_id = VGO_UCLAMP_MIN_TA,
		.cmd_name = "uclamp_min_ta",
		.val = { &mtk_vgo_uclamp_min_ta_val, NULL, NULL },
		.val_cnt = 1,
		.set = false,
		.enable = &mtk_vgo_uclamp_min_ta,
		.set_func = set_service_info,
		.unset_func = unset_service_info,
		.exec_mode = VGO_POLICY_EXEC_ONCE
	},
	{
		.cmd_id = VGO_UTIL_EST_BOOST,
		.cmd_name = "util_est_boost",
		.val = { &disable, NULL, NULL },
		.val_cnt = 1,
		.set = false,
		.enable = &mtk_vgo_util_est_boost_disable,
		.set_func = set_service_info,
		.unset_func = unset_service_info,
		.exec_mode = VGO_POLICY_EXEC_ONCE
	},
	{
		.cmd_id = VGO_RT_NON_IDLE_PREEMPT,
		.cmd_name = "rt_non_idle_preempt",
		.val = { &enable, NULL, NULL },
		.val_cnt = 1,
		.set = false,
		.enable = &mtk_vgo_rt_non_idle_preempt,
		.set_func = set_service_info,
		.unset_func = unset_service_info,
		.exec_mode = VGO_POLICY_EXEC_ONCE
	},
	{
		.cmd_id = VGO_VDEC_TASK_TURBO_PER_TASK_VIP,
		.cmd_name = "task_turbo VDEC",
		.val = { &video_task.worker_tgid[VDEC], &video_task.ipi_recv_tgid[VDEC], &video_task.c2_tgid },
		.val_cnt = 3,
		.set = false,
		.enable = &mtk_vgo_vdec_task_turbo,
		.set_func = set_service_info,
		.unset_func = unset_service_info,
		.exec_mode = VGO_POLICY_EXEC_ONCE
	},
	{
		.cmd_id = VGO_VENC_TASK_TURBO_PER_TASK_VIP,
		.cmd_name = "task_turbo VENC",
		.val = { &video_task.worker_tgid[VENC], &video_task.ipi_recv_tgid[VENC], &video_task.c2_tgid },
		.val_cnt = 3,
		.set = false,
		.enable = &mtk_vgo_venc_task_turbo,
		.set_func = set_service_info,
		.unset_func = unset_service_info,
		.exec_mode = VGO_POLICY_EXEC_ONCE
	},
	{
		.cmd_id = VGO_BTASK_UP_THRESH_CLUSTER_0,
		.cmd_name = "cpu_btask_up_thres_0",
		.val = { &mtk_vgo_cpu_btask_up_thres_0_val, NULL, NULL },
		.val_cnt = 1,
		.set = false,
		.enable = &mtk_vgo_cpu_btask_up_thres_0,
		.set_func = set_service_info,
		.unset_func = unset_service_info,
		.exec_mode = VGO_POLICY_EXEC_ONCE
	},
	{
		.cmd_id = VGO_CPUCORE_MIN_CLUSTER_0,
		.cmd_name = "cpucore_min_cluster_0",
		.val = { &mtk_vgo_cpucore_min_cluster_0_val, NULL, NULL },
		.val_cnt = 1,
		.set = false,
		.enable = &mtk_vgo_cpucore_min_cluster_0,
		.set_func = set_service_info,
		.unset_func = unset_service_info,
		.exec_mode = VGO_POLICY_EXEC_ONCE
	},
	{
		.cmd_id = VGO_CPU_PF_DISABLE_0,
		.cmd_name = "cpu_pf_disable_0",
		.val = { NULL, NULL, NULL },
		.val_cnt = 0,
		.set = false,
		.enable = &mtk_vgo_cpu_pf_ctrl,
		.set_func = set_cpu_pf_ctrl,
		.unset_func = unset_cpu_pf_ctrl,
		.exec_mode = VGO_POLICY_EXEC_ONCE
	},
	{
		.cmd_id = VGO_WLC_WCE_DISABLE,
		.cmd_name = "slc_wce_disable",
		.val = { NULL, NULL, NULL },
		.val_cnt = 0,
		.set = false,
		.enable = &mtk_vgo_slc_wce_disable,
		.set_func = set_slc_wce_ctrl,
		.unset_func = unset_slc_wce_ctrl,
		.exec_mode = VGO_POLICY_EXEC_ONCE
	},
	{
		.cmd_id = VGO_CPU_BUSY_THRES_0,
		.cmd_name = "cpu_busy_thres_0",
		.val = { &mtk_vgo_cpu_busy_thres_0_val, NULL, NULL },
		.val_cnt = 1,
		.set = false,
		.enable = &mtk_vgo_cpu_busy_thres_0,
		.set_func = set_service_info,
		.unset_func = unset_service_info,
		.exec_mode = VGO_POLICY_EXEC_ONCE
	},
	{
		.cmd_id = VGO_CPU_USAGE_THRES_0,
		.cmd_name = "cpu_usage_thres_0",
		.val = { &mtk_vgo_cpu_usage_thres_0_val, NULL, NULL },
		.val_cnt = 1,
		.set = false,
		.enable = &mtk_vgo_cpu_usage_thres_0,
		.set_func = set_cpu_usage_monitor,
		.unset_func = unset_cpu_usage_monitor,
		.exec_mode = VGO_POLICY_EXEC_ALEAYS
	},
	{
		.cmd_id = VGO_CT_TO_VIP,
		.cmd_name = "ct_to_vip",
		.val = { &enable, NULL, NULL },
		.val_cnt = 1,
		.set = false,
		.enable = &mtk_vgo_ct_to_vip,
		.set_func = set_ct_to_vip,
		.unset_func = unset_ct_to_vip,
		.exec_mode = VGO_POLICY_EXEC_ONCE
	}
};

static struct vgo_policy vp_lp_table[] = {
	{
		.cmd_id = VGO_RUNNABLE_BOOST_DISABLE,
		.cmd_name = "runnable_boost_disable",
		.val = { &enable, NULL, NULL },
		.val_cnt = 1,
		.set = false,
		.enable = &mtk_vgo_runnable_boost_disable,
		.set_func = set_service_info,
		.unset_func = unset_service_info
	},
	{
		.cmd_id = VGO_RT_NON_IDLE_PREEMPT,
		.cmd_name = "rt_non_idle_preempt",
		.val = { &enable, NULL, NULL },
		.val_cnt = 1,
		.set = false,
		.enable = &mtk_vgo_rt_non_idle_preempt,
		.set_func = set_service_info,
		.unset_func = unset_service_info
	},
	{
		.cmd_id = VGO_MARGIN_CONTROL_0,
		.cmd_name = "margin_ctrl(headroom)",
		.val = { &mtk_vgo_margin_ctrl_val[0], &mtk_vgo_margin_ctrl_val[1],
				&mtk_vgo_margin_ctrl_val[2] },
		.val_cnt = 3,
		.set = false,
		.enable = &mtk_vgo_margin_ctrl,
		.set_func = set_service_info,
		.unset_func = unset_service_info
	},
	{
		.cmd_id = VGO_UTIL_EST_BOOST,
		.cmd_name = "util_est_boost",
		.val = { &disable, NULL, NULL },
		.val_cnt = 1,
		.set = false,
		.enable = &mtk_vgo_util_est_boost_disable,
		.set_func = set_service_info,
		.unset_func = unset_service_info
	},
	{
		.cmd_id = VGO_CPU_PF_DISABLE_0,
		.cmd_name = "cpu_pf_disable_0",
		.val = { NULL, NULL, NULL },
		.val_cnt = 0,
		.set = false,
		.enable = &mtk_vgo_cpu_pf_ctrl,
		.set_func = set_cpu_pf_ctrl,
		.unset_func = unset_cpu_pf_ctrl
	},
	{
		.cmd_id = VGO_WLC_WCE_DISABLE,
		.cmd_name = "slc_wce_disable",
		.val = { NULL, NULL, NULL },
		.val_cnt = 0,
		.set = false,
		.enable = &mtk_vgo_slc_wce_disable,
		.set_func = set_slc_wce_ctrl,
		.unset_func = unset_slc_wce_ctrl
	},
};

static struct vgo_policy loom_table[] = {
	{
		.cmd_id = VGO_VDEC_TASK_TURBO_PER_TASK_VIP,
		.cmd_name = "task_turbo VDEC",
		.val = { &video_task.worker_tgid[VDEC], &video_task.ipi_recv_tgid[VDEC], &video_task.c2_tgid },
		.val_cnt = 3,
		.set = false,
		.enable = &mtk_vgo_sc_vp_lp_loom,
		.set_func = set_service_info,
		.unset_func = unset_service_info
	},
	{
		.cmd_id = VGO_CPUCORE_MIN_CLUSTER_0,
		.cmd_name = "cpucore_min_cluster_0",
		.val = { &mtk_vgo_cpucore_min_cluster_0_val, NULL, NULL },
		.val_cnt = 1,
		.set = false,
		.enable = &mtk_vgo_cpucore_min_cluster_0,
		.set_func = set_service_info,
		.unset_func = unset_service_info
	},
	{
		.cmd_id = VGO_CPU_BUSY_THRES_0,
		.cmd_name = "cpu_busy_thres_0",
		.val = { &mtk_vgo_cpu_busy_thres_0_val, NULL, NULL },
		.val_cnt = 1,
		.set = false,
		.enable = &mtk_vgo_cpu_busy_thres_0,
		.set_func = set_service_info,
		.unset_func = unset_service_info
	},
	{
		.cmd_id = VGO_BTASK_UP_THRESH_CLUSTER_0,
		.cmd_name = "cpu_btask_up_thres_0",
		.val = { &mtk_vgo_cpu_btask_up_thres_0_val, NULL, NULL },
		.val_cnt = 1,
		.set = false,
		.enable = &mtk_vgo_cpu_btask_up_thres_0,
		.set_func = set_service_info,
		.unset_func = unset_service_info
	}
};

#endif //VIDEOGO_POLICY_TABLE_H
