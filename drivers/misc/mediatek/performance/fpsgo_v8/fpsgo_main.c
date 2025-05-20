// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/kthread.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include <linux/string.h>
#include <linux/unistd.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/cpufreq.h>
#include <linux/topology.h>
#include <linux/arch_topology.h>
#include <linux/cpumask.h>
#include <trace/events/power.h>
#include <linux/tracepoint.h>
#include <linux/kallsyms.h>
#include <uapi/linux/sched/types.h>
#include <trace/hooks/cpufreq.h>
#include "mtk_irq_mon.h"
#include "sugov/cpufreq.h"

#include "mt-plat/fpsgo_common.h"
#include "fpsgo_frame_info.h"
#include "fpsgo_base.h"
#include "fpsgo_sysfs.h"
#include "fpsgo_usedext.h"
#include "fpsgo_cpu_policy.h"
#include "fbt_cpu.h"
#include "fstb.h"
#include "fps_composer.h"
#include "xgf.h"
#include "mtk_drm_arr.h"
#include "fpsgo_frame_info.h"
#include "fpsgo_adpf.h"

#define CREATE_TRACE_POINTS
#define MAX_MAGT_TARGET_FPS_NUM 10
#define MAX_MAGT_DEP_LIST_NUM 10
#define TARGET_UNLIMITED_FPS 240

enum FPSGO_NOTIFIER_PUSH_TYPE {
	FPSGO_NOTIFIER_SWITCH_FPSGO			= 0x00,
	FPSGO_NOTIFIER_QUEUE_DEQUEUE		= 0x01,
	FPSGO_NOTIFIER_DFRC_FPS				= 0x03,
	FPSGO_NOTIFIER_VSYNC				= 0x05,
	FPSGO_NOTIFIER_SWAP_BUFFER          = 0x06,
	FPSGO_NOTIFIER_ACQUIRE              = 0x08,
	FPSGO_NOTIFIER_BUFFER_QUOTA         = 0x09,
	FPSGO_NOTIFIER_PRODUCER_INFO         = 0x0a,
	FPSGO_NOTIFIER_MAGT_TARGET_FPS      = 0x0b,
	FPSGO_NOTIFIER_MAGT_DEP_LIST        = 0x0c,
	FPSGO_NOTIFIER_VSYNC_PERIOD         = 0x0d,
};

struct fpsgo_magt_target_fps {
	int pid_arr[MAX_MAGT_TARGET_FPS_NUM];
	int tid_arr[MAX_MAGT_TARGET_FPS_NUM];
	int tfps_arr[MAX_MAGT_TARGET_FPS_NUM];
	int num;
};

struct fpsgo_magt_dep_list {
	int pid;
	struct task_info dep_task_arr[MAX_MAGT_DEP_LIST_NUM];
	int dep_task_num;
};

struct fpsgo_magt_l2q_time {
	int pid;
	unsigned int frameid;
	unsigned int type;
	unsigned int status;
	unsigned long long tv_ts;
};

/* TODO: use union*/
struct FPSGO_NOTIFIER_PUSH_TAG {
	enum FPSGO_NOTIFIER_PUSH_TYPE ePushType;

	int pid;
	unsigned long long cur_ts;

	int enable;

	int qudeq_cmd;
	unsigned int queue_arg;

	unsigned long long bufID;
	int connectedAPI;
	int queue_SF;
	unsigned long long buffer_id;
	int create;

	int dfrc_fps;
	int buffer_quota;

	int enhance;
	int rescue_type;
	long long frameID;
	long long frame_flags;
	unsigned long long rescue_target;
	struct list_head queue_list;

	int consumer_pid;
	int consumer_tid;
	int producer_pid;

	char name[16];
	unsigned long mask;
	char specific_name[1000];
	int num;
	int mode;

	unsigned long long sf_buf_id;
	unsigned int frameid;
	unsigned int type;
	unsigned int status;
	unsigned long long tv_ts;
	unsigned long long que_end_sys_time_ns;

	struct fpsgo_magt_target_fps *magt_tfps_hint;
	struct fpsgo_magt_dep_list *magt_dep_hint;
};

static struct mutex notify_lock;
static struct task_struct *kfpsgo_tsk;
static int fpsgo_enable;
static int fpsgo_force_onoff;

int powerhal_tid;

#if !IS_ENABLED(CONFIG_ARM64)
int cap_ready;
#endif


/* TODO: event register & dispatch */
int fpsgo_is_enable(void)
{
	int enable;

	mutex_lock(&notify_lock);
	enable = fpsgo_enable;
	mutex_unlock(&notify_lock);

	FPSGO_LOGI("[FPSGO_CTRL] isenable %d\n", enable);
	return enable;
}

static void fpsgo_notifier_wq_cb_vsync(unsigned long long ts)
{
	FPSGO_LOGI("[FPSGO_CB] vsync: %llu\n", ts);

	if (!fpsgo_is_enable())
		return;

	fpsgo_ctrl2fbt_vsync(ts);
}

static void fpsgo_notifier_wq_cb_vsync_period(unsigned long long period_ts)
{
	FPSGO_LOGI("[FPSGO_CB] vsync: %llu\n", period_ts);

	if (!fpsgo_is_enable())
		return;

	fpsgo_ctrl2fbt_vsync_period(period_ts);
}

static void fpsgo_notifier_wq_cb_swap_buffer(int pid)
{
	FPSGO_LOGI("[FPSGO_CB] swap_buffer: %d\n", pid);

	if (!fpsgo_is_enable())
		return;

	fpsgo_update_swap_buffer(pid);
}

static void fpsgo_notifier_wq_cb_buffer_quota(unsigned long long curr_ts,
		int pid, int quota, unsigned long long id)
{
	unsigned long cb_mask = 0;

	FPSGO_LOGI("[FPSGO_CB] buffer_quota: ts %llu, pid %d,quota %d, id %llu\n",
		curr_ts, pid, quota, id);

	if (!fpsgo_is_enable())
		return;

	cb_mask = 1 << GET_FPSGO_BUFFER_TIME;

	fpsgo_ctrl2comp_buffer_count(pid, quota, id);

	// only no buffer count pass, remain origin design
	if (quota == 1) {
		fpsgo_notify_frame_info_callback(pid, cb_mask, id, NULL);
		fpsgo_ctrl2fbt_buffer_quota(curr_ts, pid, 0, id);
	}
}


static void fpsgo_notifier_wq_cb_dfrc_fps(int dfrc_fps)
{
	FPSGO_LOGI("[FPSGO_CB] dfrc_fps %d\n", dfrc_fps);

	fpsgo_ctrl2fstb_dfrc_fps(dfrc_fps);
}

static void fpsgo_notify_wq_cb_acquire(int consumer_pid, int consumer_tid,
	int producer_pid, int connectedAPI, unsigned long long buffer_id,
	unsigned long long ts)
{
	FPSGO_LOGI(
		"[FPSGO_CB] acquire: p_pid %d, c_pid:%d, c_tid:%d, api:%d, bufID:0x%llx\n",
		producer_pid, consumer_pid, consumer_tid, connectedAPI, buffer_id);

	fpsgo_ctrl2comp_acquire(producer_pid, consumer_pid, consumer_tid,
		connectedAPI, buffer_id, ts);
}

static void fpsgo_notify_wq_cb_producer_info(int ipc_tgid, int pid, int connectedAPI,
	int queue_SF, unsigned long long buffer_id)
{
	fpsgo_ctrl2comp_producer_info(ipc_tgid, pid, connectedAPI, queue_SF, buffer_id);
}

static void fpsgo_notifier_wq_cb_qudeq(int qudeq,
		unsigned int startend, int cur_pid,
		unsigned long long curr_ts, unsigned long long id,
		unsigned long long sf_buf_id)
{
	unsigned long cb_mask = 0;

	FPSGO_LOGI("[FPSGO_CB] qudeq: %d-%d, pid %d, ts %llu, id %llu\n",
		qudeq, startend, cur_pid, curr_ts, id);

	if (!fpsgo_is_enable())
		return;

	switch (qudeq) {
	case 1:
		if (startend) {
			FPSGO_LOGI("[FPSGO_CB] QUEUE Start: pid %d\n",
					cur_pid);
			cb_mask = 1 << GET_FPSGO_QUEUE_START;
			fpsgo_notify_frame_info_callback(cur_pid, cb_mask, id, NULL);
			fpsgo_ctrl2comp_enqueue_start(cur_pid,
					curr_ts, id);
		} else {
			FPSGO_LOGI("[FPSGO_CB] QUEUE End: pid %d\n",
					cur_pid);
			cb_mask = 1 << GET_FPSGO_QUEUE_END;
			fpsgo_notify_frame_info_callback(cur_pid, cb_mask, id, NULL);
			fpsgo_ctrl2comp_enqueue_end(cur_pid, curr_ts,
					id, sf_buf_id);
		}
		break;
	case 0:
		if (startend) {
			FPSGO_LOGI("[FPSGO_CB] DEQUEUE Start: pid %d\n",
					cur_pid);
			cb_mask = 1 << GET_FPSGO_DEQUEUE_START;
			fpsgo_notify_frame_info_callback(cur_pid, cb_mask, id, NULL);
			fpsgo_ctrl2comp_dequeue_start(cur_pid,
					curr_ts, id);
		} else {
			FPSGO_LOGI("[FPSGO_CB] DEQUEUE End: pid %d\n",
					cur_pid);
			cb_mask = 1 << GET_FPSGO_DEQUEUE_END;
			fpsgo_notify_frame_info_callback(cur_pid, cb_mask, id, NULL);
			fpsgo_ctrl2comp_dequeue_end(cur_pid,
					curr_ts, id, sf_buf_id);
		}
		break;
	default:
		break;
	}
}

static void fpsgo_notifier_wq_cb_enable(int enable)
{
	FPSGO_LOGI(
	"[FPSGO_CB] enable %d, fpsgo_enable %d, force_onoff %d\n",
	enable, fpsgo_enable, fpsgo_force_onoff);

	mutex_lock(&notify_lock);
	if (enable == fpsgo_enable) {
		mutex_unlock(&notify_lock);
		return;
	}

	if (fpsgo_force_onoff != FPSGO_FREE &&
			enable != fpsgo_force_onoff) {
		mutex_unlock(&notify_lock);
		return;
	}

	fpsgo_ctrl2fbt_switch_fbt(enable);
	fpsgo_ctrl2fstb_switch_fstb(enable);
	fpsgo_ctrl2xgf_switch_xgf(enable);

	fpsgo_enable = enable;

	if (!fpsgo_enable)
		fpsgo_clear();

	FPSGO_LOGI("[FPSGO_CB] fpsgo_enable %d\n",
			fpsgo_enable);
	mutex_unlock(&notify_lock);

	if (!enable)
		fpsgo_com_notify_fpsgo_is_boost(0);
}

/*
 * TODO(CHI): need to refactor in future
 *	MAGT need to implement setting FPSGO target fps by self via general API of FPSGO FSTB
 *	This part of code will phase out from FPSGO in future
 */
static void fpsgo_notifier_wq_cb_magt_target_fps(struct fpsgo_magt_target_fps *iter)
{
	int i, j;
	int max_render_num = 10;
	int cur_render_num = 0;
	int tmp_fps;
	struct render_fw_info *render_arr = NULL;

	if (!iter || !fpsgo_is_enable())
		return;

	render_arr = kcalloc(max_render_num, sizeof(struct render_fw_info), GFP_KERNEL);
	if (!render_arr)
		return;

	fpsgo_other2comp_get_render_fw_info(0, max_render_num, &cur_render_num, render_arr);
	if (cur_render_num <= 0)
		return;

	for (i = 0; i < iter->num; i++) {
		tmp_fps = iter->tfps_arr[i];
		if (iter->tid_arr[i] > 0) {
			for (j = 0; j < cur_render_num; j++)
				fpsgo_other2fstb_set_target(1, iter->tid_arr[i], tmp_fps > 0,
					1, tmp_fps, 0, render_arr[j].buffer_id);
		} else if (iter->pid_arr[i] > 0)
			fpsgo_other2fstb_set_target(0, iter->pid_arr[i], tmp_fps > 0, 1, tmp_fps, 0, 0);
	}

	kfree(render_arr);

	fpsgo_free(iter, sizeof(struct fpsgo_magt_target_fps));
}

/*
 * TODO(CHI): need to refactor in future
 *	MAGT need to implement setting FPSGO critical tasks by self via general API of FPSGO XGF
 *	This part of code will phase out from FPSGO in future
 */
static void fpsgo_notifier_wq_cb_magt_dep_list(struct fpsgo_magt_dep_list *iter)
{
	int i;
	int use;
	int num = 0;
	int max_render_num = 10;
	struct render_fw_info *render_arr = NULL;

	if (!iter || !fpsgo_is_enable())
		return;

	render_arr = kcalloc(max_render_num, sizeof(struct render_fw_info), GFP_KERNEL);
	if (!render_arr)
		return;

	fpsgo_other2comp_get_render_fw_info(0, max_render_num, &num, render_arr);
	if (num <= 0)
		return;

	use = iter->dep_task_num != 0;
	for (i = 0; i < num; i++) {
		if (render_arr[i].producer_tgid != iter->pid)
			continue;

		fpsgo_other2xgf_set_critical_tasks(render_arr[i].producer_pid, render_arr[i].buffer_id,
			iter->dep_task_arr, iter->dep_task_num, use);
	}
	kfree(render_arr);

	fpsgo_free(iter, sizeof(struct fpsgo_magt_dep_list));
}

static LIST_HEAD(head);
static int condition_notifier_wq;
static DEFINE_MUTEX(notifier_wq_lock);
static DECLARE_WAIT_QUEUE_HEAD(notifier_wq_queue);
static void fpsgo_queue_work(struct FPSGO_NOTIFIER_PUSH_TAG *vpPush)
{
	mutex_lock(&notifier_wq_lock);
	list_add_tail(&vpPush->queue_list, &head);
	condition_notifier_wq = 1;
	mutex_unlock(&notifier_wq_lock);

	wake_up_interruptible(&notifier_wq_queue);
}

static void fpsgo_notifier_wq_cb(void)
{
	struct FPSGO_NOTIFIER_PUSH_TAG *vpPush;

	wait_event_interruptible(notifier_wq_queue, condition_notifier_wq);
	mutex_lock(&notifier_wq_lock);

	if (!list_empty(&head)) {
		vpPush = list_first_entry(&head,
			struct FPSGO_NOTIFIER_PUSH_TAG, queue_list);
		list_del(&vpPush->queue_list);
		if (list_empty(&head))
			condition_notifier_wq = 0;
		mutex_unlock(&notifier_wq_lock);
	} else {
		condition_notifier_wq = 0;
		mutex_unlock(&notifier_wq_lock);
		return;
	}
	switch (vpPush->ePushType) {
	case FPSGO_NOTIFIER_SWITCH_FPSGO:
		fpsgo_notifier_wq_cb_enable(vpPush->enable);
		break;
	case FPSGO_NOTIFIER_QUEUE_DEQUEUE:
		fpsgo_notifier_wq_cb_qudeq(vpPush->qudeq_cmd,
				vpPush->queue_arg, vpPush->pid,
				vpPush->cur_ts, vpPush->buffer_id, vpPush->sf_buf_id);
		break;
	case FPSGO_NOTIFIER_DFRC_FPS:
		fpsgo_notifier_wq_cb_dfrc_fps(vpPush->dfrc_fps);
		break;
	case FPSGO_NOTIFIER_VSYNC:
		fpsgo_notifier_wq_cb_vsync(vpPush->cur_ts);
		break;
	case FPSGO_NOTIFIER_VSYNC_PERIOD:
		fpsgo_notifier_wq_cb_vsync_period(vpPush->cur_ts);
		break;
	case FPSGO_NOTIFIER_SWAP_BUFFER:
		fpsgo_notifier_wq_cb_swap_buffer(vpPush->pid);
		break;
	case FPSGO_NOTIFIER_ACQUIRE:
		fpsgo_notify_wq_cb_acquire(vpPush->consumer_pid,
			vpPush->consumer_tid, vpPush->producer_pid,
			vpPush->connectedAPI, vpPush->bufID, vpPush->cur_ts);
		break;
	case FPSGO_NOTIFIER_BUFFER_QUOTA:
		fpsgo_notifier_wq_cb_buffer_quota(vpPush->cur_ts,
				vpPush->pid,
				vpPush->buffer_quota,
				vpPush->buffer_id);
		break;
	case FPSGO_NOTIFIER_PRODUCER_INFO:
		fpsgo_notify_wq_cb_producer_info(vpPush->producer_pid, vpPush->pid,
			vpPush->connectedAPI, vpPush->queue_SF, vpPush->bufID);
		break;

	/*TODO(CHI): need to refactor in future*/
	case FPSGO_NOTIFIER_MAGT_TARGET_FPS:
		fpsgo_notifier_wq_cb_magt_target_fps(vpPush->magt_tfps_hint);
		break;
	case FPSGO_NOTIFIER_MAGT_DEP_LIST:
		fpsgo_notifier_wq_cb_magt_dep_list(vpPush->magt_dep_hint);
		break;

	default:
		FPSGO_LOGE("[FPSGO_CTRL] unhandled push type = %d\n",
				vpPush->ePushType);
		break;
	}
	fpsgo_free(vpPush, sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));
}

int fpsgo_get_kfpsgo_tid(void)
{
	return kfpsgo_tsk ? kfpsgo_tsk->pid : 0;
}
EXPORT_SYMBOL(fpsgo_get_kfpsgo_tid);

static int kfpsgo(void *arg)
{
	struct sched_attr attr = {};

	attr.sched_policy = -1;
	attr.sched_flags =
		SCHED_FLAG_KEEP_ALL |
		SCHED_FLAG_UTIL_CLAMP |
		SCHED_FLAG_RESET_ON_FORK;
	attr.sched_util_min = 1;
	attr.sched_util_max = 1024;
	if (sched_setattr_nocheck(current, &attr) != 0)
		FPSGO_LOGE("[FPSGO_CTRL] %s set uclamp fail\n", __func__);

	set_user_nice(current, -20);

	while (!kthread_should_stop())
		fpsgo_notifier_wq_cb();

	return 0;
}
int fpsgo_notify_qudeq(int qudeq,
		unsigned int startend,
		int pid, unsigned long long id, unsigned long long sf_buf_id)
{
	unsigned long long cur_ts;
	struct FPSGO_NOTIFIER_PUSH_TAG *vpPush;

	FPSGO_LOGI("[FPSGO_CTRL] qudeq %d-%d, id %llu pid %d\n",
		qudeq, startend, id, pid);

	if (!fpsgo_is_enable())
		return 0;

	vpPush =
		(struct FPSGO_NOTIFIER_PUSH_TAG *)
		fpsgo_alloc_atomic(sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));
	if (!vpPush) {
		FPSGO_LOGE("[FPSGO_CTRL] OOM\n");
		return 0;
	}

	if (!kfpsgo_tsk) {
		FPSGO_LOGE("[FPSGO_CTRL] NULL WorkQueue\n");
		fpsgo_free(vpPush, sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));
		return 0;
	}

	cur_ts = fpsgo_get_time();

	vpPush->ePushType = FPSGO_NOTIFIER_QUEUE_DEQUEUE;
	vpPush->pid = pid;
	vpPush->cur_ts = cur_ts;
	vpPush->qudeq_cmd = qudeq;
	vpPush->queue_arg = startend;
	vpPush->buffer_id = id;
	vpPush->sf_buf_id = sf_buf_id;

	fpsgo_queue_work(vpPush);

#if !IS_ENABLED(CONFIG_ARM64)
	if (!cap_ready) {
		fbt_update_pwr_tbl();
		cap_ready = 1;
	}
#endif

	return FPSGO_VERSION_CODE;
}

void fpsgo_notify_acquire(int consumer_pid, int producer_pid,
	int connectedAPI, unsigned long long buffer_id)
{
	struct FPSGO_NOTIFIER_PUSH_TAG *vpPush;

	if (!fpsgo_is_enable())
		return;

	vpPush = (struct FPSGO_NOTIFIER_PUSH_TAG *)
		fpsgo_alloc_atomic(sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));

	if (!vpPush) {
		FPSGO_LOGE("[FPSGO_CTRL] OOM\n");
		return;
	}

	if (!kfpsgo_tsk) {
		FPSGO_LOGE("[FPSGO_CTRL] NULL WorkQueue\n");
		fpsgo_free(vpPush, sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));
		return;
	}

	vpPush->ePushType = FPSGO_NOTIFIER_ACQUIRE;
	vpPush->consumer_pid = consumer_pid;
	vpPush->consumer_tid = current->pid;
	vpPush->producer_pid = producer_pid;
	vpPush->connectedAPI = connectedAPI;
	vpPush->bufID = buffer_id;
	vpPush->cur_ts = fpsgo_get_time();

	fpsgo_queue_work(vpPush);
}

void fpsgo_notify_producer_info(int ipc_tgid, int pid, int connectedAPI,
	int queue_SF, unsigned long long buffer_id)
{
	struct FPSGO_NOTIFIER_PUSH_TAG *vpPush;

	if (!fpsgo_is_enable())
		return;

	vpPush = (struct FPSGO_NOTIFIER_PUSH_TAG *)
		fpsgo_alloc_atomic(sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));

	if (!vpPush)
		return;

	if (!kfpsgo_tsk) {
		fpsgo_free(vpPush, sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));
		return;
	}

	vpPush->ePushType = FPSGO_NOTIFIER_PRODUCER_INFO;
	vpPush->producer_pid = ipc_tgid;
	vpPush->pid = pid;
	vpPush->connectedAPI = connectedAPI;
	vpPush->queue_SF = queue_SF;
	vpPush->bufID = buffer_id;

	fpsgo_queue_work(vpPush);
}

void fpsgo_notify_vsync(void)
{
	struct FPSGO_NOTIFIER_PUSH_TAG *vpPush;

	FPSGO_LOGI("[FPSGO_CTRL] vsync\n");

	if (!fpsgo_is_enable())
		return;

	vpPush = (struct FPSGO_NOTIFIER_PUSH_TAG *)
		fpsgo_alloc_atomic(sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));

	if (!vpPush) {
		FPSGO_LOGE("[FPSGO_CTRL] OOM\n");
		return;
	}

	if (!kfpsgo_tsk) {
		FPSGO_LOGE("[FPSGO_CTRL] NULL WorkQueue\n");
		fpsgo_free(vpPush, sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));
		return;
	}

	vpPush->ePushType = FPSGO_NOTIFIER_VSYNC;
	vpPush->cur_ts = fpsgo_get_time();

	fpsgo_queue_work(vpPush);
}

void fpsgo_notify_vsync_period(unsigned long long period)
{
	struct FPSGO_NOTIFIER_PUSH_TAG *vpPush;

	FPSGO_LOGI("[FPSGO_CTRL] vsync period\n");

	if (!fpsgo_is_enable())
		return;

	vpPush = (struct FPSGO_NOTIFIER_PUSH_TAG *)
		fpsgo_alloc_atomic(sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));

	if (!vpPush) {
		FPSGO_LOGE("[FPSGO_CTRL] OOM\n");
		return;
	}

	if (!kfpsgo_tsk) {
		FPSGO_LOGE("[FPSGO_CTRL] NULL WorkQueue\n");
		fpsgo_free(vpPush, sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));
		return;
	}

	vpPush->ePushType = FPSGO_NOTIFIER_VSYNC_PERIOD;
	vpPush->cur_ts = period;

	fpsgo_queue_work(vpPush);
}


void fpsgo_notify_swap_buffer(int pid)
{
	struct FPSGO_NOTIFIER_PUSH_TAG *vpPush;

	FPSGO_LOGI("[FPSGO_CTRL] swap_buffer\n");

	if (!fpsgo_is_enable())
		return;

	vpPush = (struct FPSGO_NOTIFIER_PUSH_TAG *)
		fpsgo_alloc_atomic(sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));

	if (!vpPush) {
		FPSGO_LOGE("[FPSGO_CTRL] OOM\n");
		return;
	}

	if (!kfpsgo_tsk) {
		FPSGO_LOGE("[FPSGO_CTRL] NULL WorkQueue\n");
		fpsgo_free(vpPush, sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));
		return;
	}

	vpPush->ePushType = FPSGO_NOTIFIER_SWAP_BUFFER;
	vpPush->pid = pid;

	fpsgo_queue_work(vpPush);
}

void power2fpsgo_get_fps(int *pid, int *fps)
{
	if (unlikely(powerhal_tid == 0))
		powerhal_tid = current->pid;

	fpsgo_ctrl2fstb_get_fps(pid, fps);

	FPSGO_LOGI("[FPSGO_CTRL] get_fps %d %d\n", *pid, *fps);
}

void fpsgo_get_cmd(int *cmd, int *value1, int *value2)
{
	int _cmd = -1, _value1 = -1, _value2 = -1;

	fpsgo_ctrl2base_get_pwr_cmd(&_cmd, &_value1, &_value2);


	FPSGO_LOGI("[FPSGO_CTRL] get_cmd %d %d %d\n", _cmd, _value1, _value2);
	*cmd = _cmd;
	*value1 = _value1;
	*value2 = _value2;

}

/* let Task Turbo get third camera status from ThirdCamPowerhalControl */
bool get_cam_status_for_task_turbo(void)
{
	return 0;
}
EXPORT_SYMBOL(get_cam_status_for_task_turbo);

void fpsgo_notify_buffer_quota(int pid, int quota, unsigned long long identifier)
{
	unsigned long long cur_ts;
	struct FPSGO_NOTIFIER_PUSH_TAG *vpPush;

	FPSGO_LOGI("[FPSGO_CTRL] buffer_quota %d, pid %d, id %llu\n",
		quota, pid, identifier);

	if (!fpsgo_is_enable())
		return;

	vpPush =
		(struct FPSGO_NOTIFIER_PUSH_TAG *)
		fpsgo_alloc_atomic(sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));
	if (!vpPush) {
		FPSGO_LOGE("[FPSGO_CTRL] OOM\n");
		return;
	}

	if (!kfpsgo_tsk) {
		FPSGO_LOGE("[FPSGO_CTRL] NULL WorkQueue\n");
		fpsgo_free(vpPush, sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));
		return;
	}

	cur_ts = fpsgo_get_time();

	vpPush->ePushType = FPSGO_NOTIFIER_BUFFER_QUOTA;
	vpPush->cur_ts = cur_ts;
	vpPush->pid = pid;
	vpPush->buffer_quota = quota;
	vpPush->buffer_id = identifier;

	fpsgo_queue_work(vpPush);
}

/*
 * TODO(CHI): need to refactor in future
 *	MAGT need to determine target fps and implement by self
 *	This part of code will phase out from FPSGO in future
 */
int fpsgo_notify_magt_target_fps(int *pid_arr, int *tid_arr,
	int *tfps_arr, int num)
{
	struct FPSGO_NOTIFIER_PUSH_TAG *vpPush = NULL;

	if (!pid_arr|| !tid_arr|| num < 0 ||
		num > MAX_MAGT_TARGET_FPS_NUM)
		return -EINVAL;

	if (!kfpsgo_tsk)
		return -ENOMEM;

	vpPush = fpsgo_alloc_atomic(sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));
	if (!vpPush)
		return -ENOMEM;

	vpPush->magt_tfps_hint = NULL;
	vpPush->magt_tfps_hint = fpsgo_alloc_atomic(sizeof(struct fpsgo_magt_target_fps));
	if (!vpPush->magt_tfps_hint) {
		fpsgo_free(vpPush, sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));
		return -ENOMEM;
	}

	memset(vpPush->magt_tfps_hint->pid_arr, 0, MAX_MAGT_TARGET_FPS_NUM * sizeof(int));
	memcpy(vpPush->magt_tfps_hint->pid_arr, pid_arr, num * sizeof(int));
	memset(vpPush->magt_tfps_hint->tid_arr, 0, MAX_MAGT_TARGET_FPS_NUM * sizeof(int));
	memcpy(vpPush->magt_tfps_hint->tid_arr, tid_arr, num * sizeof(int));
	memset(vpPush->magt_tfps_hint->tfps_arr, 0, MAX_MAGT_TARGET_FPS_NUM * sizeof(int));
	memcpy(vpPush->magt_tfps_hint->tfps_arr, tfps_arr, num * sizeof(int));
	vpPush->magt_tfps_hint->num = num;
	vpPush->ePushType = FPSGO_NOTIFIER_MAGT_TARGET_FPS;

	fpsgo_queue_work(vpPush);

	return 0;
}

/*
 * TODO(CHI): need to refactor in future
 *	MAGT need to determine action of specific tasks and implement by self
 *	This part of code will phase out from FPSGO in future
 */
struct dep_and_prio {
	int32_t pid;
	int32_t prio;
	int32_t timeout;
};
int fpsgo_notify_magt_dep_list(int pid, void *dep_task_arr, int dep_task_num)
{
	struct dep_and_prio *param = (struct dep_and_prio *)dep_task_arr;
	struct FPSGO_NOTIFIER_PUSH_TAG *vpPush = NULL;
	int i;

	if (dep_task_num < 0 || dep_task_num > MAX_MAGT_DEP_LIST_NUM ||
		(!param && dep_task_num != 0))
		return -EINVAL;

	if (!kfpsgo_tsk)
		return -ENOMEM;

	vpPush = fpsgo_alloc_atomic(sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));
	if (!vpPush)
		return -ENOMEM;

	vpPush->magt_dep_hint = NULL;
	vpPush->magt_dep_hint = fpsgo_alloc_atomic(sizeof(struct fpsgo_magt_dep_list));
	if (!vpPush->magt_dep_hint) {
		fpsgo_free(vpPush, sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));
		return -ENOMEM;
	}

	vpPush->magt_dep_hint->pid = pid;
	memset(vpPush->magt_dep_hint->dep_task_arr, 0,
		MAX_MAGT_DEP_LIST_NUM * sizeof(struct task_info));
	if (param) {
		for (i = 0; i < dep_task_num; i++) {
			vpPush->magt_dep_hint->dep_task_arr[i].pid = param[i].pid;
			vpPush->magt_dep_hint->dep_task_arr[i].action = XGF_ADD_DEP;
			vpPush->magt_dep_hint->dep_task_arr[i].vip_prio = param[i].prio;
			vpPush->magt_dep_hint->dep_task_arr[i].vip_timeout = param[i].timeout;
		}
	}

	vpPush->magt_dep_hint->dep_task_num = dep_task_num;
	vpPush->ePushType = FPSGO_NOTIFIER_MAGT_DEP_LIST;

	fpsgo_queue_work(vpPush);

	return 0;
}

int fpsgo_get_enable_signal(int tgid, int wait, int *ret) {
	// consider all open or by process open
	return wait ? fpsgo_ctrl2comp_wait_receive_fw_info_enable(tgid, ret) :
				fpsgo_ctrl2comp_get_receive_fw_info_enable();
}

int get_fpsgo_frame_info(int max_num, unsigned long mask,
	int filter_bypass, int tgid, struct render_frame_info *frame_info_arr)
{
	int ret = 0;

	if (max_num <= 0 || mask == 0 ||
		mask >= 1 << FPSGO_FRAME_INFO_MAX_NUM || !frame_info_arr)
		return -EINVAL;

	ret = fpsgo_ctrl2base_get_render_frame_info(max_num, mask,
			filter_bypass, tgid, frame_info_arr);

	return ret;
}
EXPORT_SYMBOL(get_fpsgo_frame_info);

void dfrc_fps_limit_cb(unsigned int fps_limit)
{
	unsigned int vTmp = TARGET_UNLIMITED_FPS;
	struct FPSGO_NOTIFIER_PUSH_TAG *vpPush;

	if (fps_limit > 0 && fps_limit <= TARGET_UNLIMITED_FPS)
		vTmp = fps_limit;

	FPSGO_LOGI("[FPSGO_CTRL] dfrc_fps %d\n", vTmp);

	vpPush =
		(struct FPSGO_NOTIFIER_PUSH_TAG *)
		fpsgo_alloc_atomic(sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));

	if (!vpPush) {
		FPSGO_LOGE("[FPSGO_CTRL] OOM\n");
		return;
	}

	if (!kfpsgo_tsk) {
		FPSGO_LOGE("[FPSGO_CTRL] NULL WorkQueue\n");
		fpsgo_free(vpPush, sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));
		return;
	}

	vpPush->ePushType = FPSGO_NOTIFIER_DFRC_FPS;
	vpPush->dfrc_fps = vTmp;

	fpsgo_queue_work(vpPush);
}

/* FPSGO control */
void fpsgo_switch_enable(int enable)
{
	struct FPSGO_NOTIFIER_PUSH_TAG *vpPush = NULL;

	if (!kfpsgo_tsk) {
		FPSGO_LOGE("[FPSGO_CTRL] NULL WorkQueue\n");
		return;
	}

	if (fpsgo_base2fbt_get_cluster_num() <= 0) {
		FPSGO_LOGE("[%s] DON'T ENABLE FPSGO: nr_cluster <= 0", __func__);
		return;
	}

	FPSGO_LOGI("[FPSGO_CTRL] switch enable %d\n", enable);

	if (fpsgo_is_force_enable() !=
			FPSGO_FREE && enable !=
			fpsgo_is_force_enable())
		return;

	vpPush =
		(struct FPSGO_NOTIFIER_PUSH_TAG *)
		fpsgo_alloc_atomic(sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));

	if (!vpPush) {
		FPSGO_LOGE("[FPSGO_CTRL] OOM\n");
		return;
	}

	vpPush->ePushType = FPSGO_NOTIFIER_SWITCH_FPSGO;
	vpPush->enable = enable;

	fpsgo_queue_work(vpPush);
}

int fpsgo_is_force_enable(void)
{
	int temp_onoff;

	mutex_lock(&notify_lock);
	temp_onoff = fpsgo_force_onoff;
	mutex_unlock(&notify_lock);

	return temp_onoff;
}

void fpsgo_force_switch_enable(int enable)
{
	mutex_lock(&notify_lock);
	fpsgo_force_onoff = enable;
	mutex_unlock(&notify_lock);

	fpsgo_switch_enable(enable?1:0);
}

static void fpsgo_notify_cpufreq_cap(unsigned int first_cpu_id, unsigned int last_cpu_id)
{
	int cluster = 0;
	int max_capacity = 0, tmp_capacity = 0;
	unsigned int cpu;
	struct cpufreq_policy *policy = NULL;

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			break;
		cpu = cpumask_first(policy->related_cpus);
		if (cpu == first_cpu_id) {
			cpufreq_cpu_put(policy);
			break;
		}
		cpu = cpumask_last(policy->related_cpus);
		cluster++;
		cpufreq_cpu_put(policy);
	}

	for (cpu = first_cpu_id; cpu <= last_cpu_id; cpu++) {
#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
		tmp_capacity = get_curr_cap(cpu);
#endif
		if (tmp_capacity > max_capacity)
			max_capacity = tmp_capacity;
	}

	if (max_capacity > 0)
		fpsgo_ctrl2fbt_cpufreq_cb_cap(cluster, max_capacity);
}

#if FPSGO_DYNAMIC_WL
static void fpsgo_cpu_frequency_cap_tracer(void *ignore, struct cpufreq_policy *policy)
{
	int first_cpu_id = 0, last_cpu_id = 0;

	irq_log_store();

	if (!fpsgo_enable || !policy)
		goto out;

	first_cpu_id = cpumask_first(policy->related_cpus);
	last_cpu_id = cpumask_last(policy->related_cpus);
	fpsgo_notify_cpufreq_cap(first_cpu_id, last_cpu_id);

out:
	irq_log_store();
}

void register_fpsgo_android_cpufreq_transition_hook(void)
{
	int ret = 0;

	ret = register_trace_android_rvh_cpufreq_transition(fpsgo_cpu_frequency_cap_tracer, NULL);
	if (ret)
		pr_info("register android_rvh_cpufreq_transition hooks failed, returned %d\n", ret);
}

#else
struct tracepoints_table {
	const char *name;
	void *func;
	struct tracepoint *tp;
	bool registered;
};

#define MAX_NR_CPUS CONFIG_MAX_NR_CPUS
static void fpsgo_cpu_frequency_tracer(void *ignore, unsigned int frequency, unsigned int cpu_id)
{
	unsigned int first_cpu_id, last_cpu_id;
	struct cpufreq_policy *policy = NULL;

	irq_log_store();

	if (!fpsgo_enable)
		return;

	policy = cpufreq_cpu_get(cpu_id);
	if (!policy)
		return;
	if (cpu_id != cpumask_first(policy->related_cpus)) {
		cpufreq_cpu_put(policy);
		return;
	}

	first_cpu_id = cpumask_first(policy->related_cpus);
	last_cpu_id = cpumask_last(policy->related_cpus);
	first_cpu_id = clamp(first_cpu_id, 0, MAX_NR_CPUS - 1);
	last_cpu_id = clamp(last_cpu_id, 0, MAX_NR_CPUS - 1);
	cpufreq_cpu_put(policy);
	fpsgo_notify_cpufreq_cap(first_cpu_id, last_cpu_id);

	irq_log_store();
}

struct tracepoints_table fpsgo_tracepoints[] = {
	{.name = "cpu_frequency", .func = fpsgo_cpu_frequency_tracer},
};

#define FOR_EACH_INTEREST(i) \
	for (i = 0; i < sizeof(fpsgo_tracepoints) / sizeof(struct tracepoints_table); i++)

static void lookup_tracepoints(struct tracepoint *tp, void *ignore)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (strcmp(fpsgo_tracepoints[i].name, tp->name) == 0)
			fpsgo_tracepoints[i].tp = tp;
	}
}

static void tracepoint_cleanup(void)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (fpsgo_tracepoints[i].registered) {
			tracepoint_probe_unregister(
				fpsgo_tracepoints[i].tp,
				fpsgo_tracepoints[i].func, NULL);
			fpsgo_tracepoints[i].registered = false;
		}
	}
}

void register_fpsgo_cpufreq_transition_hook(void)
{
	int i, ret = 0;

	for_each_kernel_tracepoint(lookup_tracepoints, NULL);

	FOR_EACH_INTEREST(i) {
		if (fpsgo_tracepoints[i].tp == NULL) {
			FPSGO_LOGE("FPSGO Error, %s not found\n", fpsgo_tracepoints[i].name);
			tracepoint_cleanup();
			return;
		}
	}
	ret = tracepoint_probe_register(fpsgo_tracepoints[0].tp, fpsgo_tracepoints[0].func,  NULL);
	if (ret) {
		FPSGO_LOGE("cpu_frequency: Couldn't activate tracepoint\n");
		return;
	}
	fpsgo_tracepoints[0].registered = true;
}
#endif  // FPSGO_DYNAMIC_WL

static void __exit fpsgo_exit(void)
{
	fpsgo_notifier_wq_cb_enable(0);

	if (kfpsgo_tsk)
		kthread_stop(kfpsgo_tsk);

#if IS_ENABLED(CONFIG_DEVICE_MODULES_DRM_MEDIATEK)
	drm_unregister_fps_chg_callback(dfrc_fps_limit_cb);
#endif
	fbt_cpu_exit();
	mtk_fstb_exit();
	fpsgo_composer_exit();
	fpsgo_sysfs_exit();
}

static int __init fpsgo_init(void)
{
#if !IS_ENABLED(CONFIG_ARM64)
	cap_ready = 0;
#endif

	fpsgo_cpu_policy_init();

	fpsgo_sysfs_init();

	kfpsgo_tsk = kthread_create(kfpsgo, NULL, "kfps");
	if (kfpsgo_tsk == NULL)
		return -EFAULT;
	wake_up_process(kfpsgo_tsk);

#if FPSGO_DYNAMIC_WL
	register_fpsgo_android_cpufreq_transition_hook();
#else  // FPSGO_DYNAMIC_WL
	register_fpsgo_cpufreq_transition_hook();
#endif  // FPSGO_DYNAMIC_WL

	mutex_init(&notify_lock);

	fpsgo_force_onoff = FPSGO_FREE;

	init_fpsgo_common();
	mtk_fstb_init();
	fpsgo_composer_init();
	fbt_cpu_init();
	fpsgo_adpf_init();

	if (fpsgo_arch_nr_clusters() > 0)
		fpsgo_switch_enable(1);

	fpsgo_notify_vsync_fp = fpsgo_notify_vsync;
	fpsgo_notify_vsync_period_fp = fpsgo_notify_vsync_period;

	fpsgo_notify_qudeq_fp = fpsgo_notify_qudeq;
	fpsgo_notify_acquire_fp = fpsgo_notify_acquire;
	fpsgo_notify_producer_info_fp = fpsgo_notify_producer_info;

	fpsgo_notify_swap_buffer_fp = fpsgo_notify_swap_buffer;

	power2fpsgo_get_fps_fp = power2fpsgo_get_fps;
	fpsgo_get_cmd_fp = fpsgo_get_cmd;
	fpsgo_notify_buffer_quota_fp = fpsgo_notify_buffer_quota;
	/*General COMP API*/
	fpsgo_other2comp_get_render_fw_info_fp = fpsgo_other2comp_get_render_fw_info;
	fpsgo_other2comp_flush_acquire_table_fp = fpsgo_other2comp_flush_acquire_table;
	/*General FSTB API*/
	fpsgo_other2fstb_get_app_self_ctrl_time_fp = fpsgo_other2fstb_get_app_self_ctrl_time;
	fpsgo_other2fstb_get_fps_info_fp = fpsgo_other2fstb_get_fps_info;
	/*General XGF API*/
	fpsgo_other2xgf_get_critical_tasks_fp = fpsgo_other2xgf_get_critical_tasks;

	powerhal2fpsgo_get_fpsgo_frame_info_fp = get_fpsgo_frame_info;

#if IS_ENABLED(CONFIG_MTK_PERF_IOCTL_MAGT)
	magt2fpsgo_notify_target_fps_fp = fpsgo_notify_magt_target_fps;
	magt2fpsgo_notify_dep_list_fp = fpsgo_notify_magt_dep_list;
	magt2fpsgo_get_fpsgo_frame_info = get_fpsgo_frame_info;
#endif
	fpsgo_get_lr_pair_fp = fpsgo_get_lr_pair;
	fpsgo_set_rl_l2q_enable_fp = fpsgo_set_rl_l2q_enable;
	fpsgo_set_rl_expected_l2q_us_fp = fpsgo_set_expected_l2q_us;
	fpsgo_get_now_logic_head_fp = fpsgo_get_now_logic_head;

	fpsgo_get_enable_signal_fp = fpsgo_get_enable_signal;

#if IS_ENABLED(CONFIG_DEVICE_MODULES_DRM_MEDIATEK)
	drm_register_fps_chg_callback(dfrc_fps_limit_cb);
#endif

	return 0;
}

#if !IS_ENABLED(CONFIG_ARM64)
late_initcall_sync(fpsgo_init);
#else
module_init(fpsgo_init);
#endif
module_exit(fpsgo_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek FPSGO");
MODULE_AUTHOR("MediaTek Inc.");
MODULE_VERSION(FPSGO_VERSION_MODULE);
