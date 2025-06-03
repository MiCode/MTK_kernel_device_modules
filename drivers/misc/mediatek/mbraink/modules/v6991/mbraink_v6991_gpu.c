// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/sched/clock.h>
#include <linux/smp.h>
#include <linux/preempt.h>
#include <mbraink_modules_ops_def.h>
#include "mbraink_v6991_gpu.h"

#if IS_ENABLED(CONFIG_MTK_FPSGO_V8)
#include <fpsgo_frame_info.h>
#endif

#include <ged_dvfs.h>
#include <gpufreq_v2.h>

#if IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)
#include <ged_mali_event.h>
#define MAX_WORKER_META_DATA_NUM MAX_META_DATA_NUM
#else
#define MAX_WORKER_META_DATA_NUM 8
#endif

#define Q2QTIMEOUT 500000000 //500ms
#define Q2QTIMEOUT_HIST 70000000 //70ms
#define PERFINDEX_TIMEOUT 8000000000 //8sec
#define PERFINDEX_LIMIT 100
#define PERFINDEX_BUF 30
#define PERFINDEX_SLOT 20
#define PERFINDEX_LO_ALARM_COUNT 60
#define GPU_WORKEREVT_QUEUE_LIMIT 10
#define IS_GAME_MODE 0 //off

struct mbraink_gpu_perfidx_info {
	struct hlist_node hlist;

	int pid;
	unsigned long long bufid;
	int perf_idx[PERFINDEX_BUF];
	unsigned long long ts[PERFINDEX_BUF];
	int currentIdx;
	int current_perf_idx;
	int sbe_ctrl[PERFINDEX_BUF];
	unsigned long long err_ts;
	unsigned long long err_counter;
};

struct mbraink_gpu_workerevt_info {
	struct hlist_node hlist;

	unsigned long long timestamp;
	int pid;
	uint32_t workerType;
	unsigned long long exec_duration;
	uint32_t meta_data[MAX_WORKER_META_DATA_NUM];
};

static unsigned long long gq2qTimeoutInNs = Q2QTIMEOUT;
static int gisGameMode = IS_GAME_MODE;
unsigned int TimeoutCounter[10] = {0};
unsigned int TimeoutRange[10] = {70, 120, 170, 220, 270, 320, 370, 420, 470, 520};

static unsigned long long gperfIdxTimeoutInNs = PERFINDEX_TIMEOUT;
static int gperfIdxLimit = PERFINDEX_LIMIT;

static int gOpMode = mbraink_op_mode_normal;
static int gPerfLoAlarmCount = 1;

static int gWorkerEvtCount = 1;

static HLIST_HEAD(mbk_g_perfidx_list);
static DEFINE_MUTEX(mbk_g_perfidx_lock);

static HLIST_HEAD(mbk_g_workerevt_list);
static DEFINE_MUTEX(mbk_g_workerevt_lock);


static void calculateTimeoutCouter(unsigned long long q2qTimeInNS)
{
	if (q2qTimeInNS < 120000000) //70~120ms
		TimeoutCounter[0]++;
	else if (q2qTimeInNS < 170000000) //120~170ms
		TimeoutCounter[1]++;
	else if (q2qTimeInNS < 220000000) //170~220ms
		TimeoutCounter[2]++;
	else if (q2qTimeInNS < 270000000) //220~270ms
		TimeoutCounter[3]++;
	else if (q2qTimeInNS < 320000000) //270~320ms
		TimeoutCounter[4]++;
	else if (q2qTimeInNS < 370000000) //320~370ms
		TimeoutCounter[5]++;
	else if (q2qTimeInNS < 420000000) //370~420ms
		TimeoutCounter[6]++;
	else if (q2qTimeInNS < 470000000) //420~470ms
		TimeoutCounter[7]++;
	else if (q2qTimeInNS < 520000000) //470~520ms
		TimeoutCounter[8]++;
	else //>520ms
		TimeoutCounter[9]++;
}

int delete_perfidx_info(int pid, unsigned long long bufID)
{
	int ret = 0;
	struct mbraink_gpu_perfidx_info *iter = NULL;
	struct hlist_node *h = NULL;

	mutex_lock(&mbk_g_perfidx_lock);

	hlist_for_each_entry_safe(iter, h, &mbk_g_perfidx_list, hlist) {
		if (iter->pid == pid && iter->bufid == bufID) {
			hlist_del(&iter->hlist);
			vfree(iter);
			ret = 1;
			break;
		}
	}

	mutex_unlock(&mbk_g_perfidx_lock);

	return ret;
}

int recycle_perfidx_list(void)
{
	int ret = 0;
	struct mbraink_gpu_perfidx_info *iter;
	struct hlist_node *h;

	mutex_lock(&mbk_g_perfidx_lock);

	if (hlist_empty(&mbk_g_perfidx_list)) {
		ret = 1;
		goto out;
	}

	hlist_for_each_entry_safe(iter, h, &mbk_g_perfidx_list, hlist) {
		hlist_del(&iter->hlist);
		vfree(iter);
		ret = 1;
	}

out:
	mutex_unlock(&mbk_g_perfidx_lock);
	return ret;
}

static void mbraink_v6991_gpu_setOpMode(int OpMode)
{
	gOpMode = OpMode;
}

static int mbraink_v6991_gpu_getOpMode(void)
{
	return gOpMode;
}

static ssize_t mbraink_v6991_gpu_getTimeoutCouterReport(char *pBuf)
{
	ssize_t size = 0;

	if (pBuf == NULL)
		return size;

	size += scnprintf(pBuf+size, 1024-size, "%d~%d:%d\n%d~%d:%d\n",
		TimeoutRange[0], TimeoutRange[1], TimeoutCounter[0],
		TimeoutRange[1], TimeoutRange[2], TimeoutCounter[1]);
	size += scnprintf(pBuf+size, 1024-size, "%d~%d:%d\n%d~%d:%d\n",
		TimeoutRange[2], TimeoutRange[3], TimeoutCounter[2],
		TimeoutRange[3], TimeoutRange[4], TimeoutCounter[3]);
	size += scnprintf(pBuf+size, 1024-size, "%d~%d:%d\n%d~%d:%d\n",
		TimeoutRange[4], TimeoutRange[5], TimeoutCounter[4],
		TimeoutRange[5], TimeoutRange[6], TimeoutCounter[5]);
	size += scnprintf(pBuf+size, 1024-size, "%d~%d:%d\n%d~%d:%d\n",
		TimeoutRange[6], TimeoutRange[7], TimeoutCounter[6],
		TimeoutRange[7], TimeoutRange[8], TimeoutCounter[7]);
	size += scnprintf(pBuf+size, 1024-size, "%d~%d:%d\n>%d:%d\n",
		TimeoutRange[8], TimeoutRange[9], TimeoutCounter[8],
		TimeoutRange[9], TimeoutCounter[9]);

	return size;
}

static unsigned long long fpsgo_get_time(void)
{
	unsigned long long temp;

	preempt_disable();
	temp = cpu_clock(smp_processor_id());
	preempt_enable();

	return temp;
}

void fpsgo2mbrain_hint_frameinfo(unsigned long cmd, struct render_frame_info *iter)
{
	char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};
	int n = 0;
	int pos = 0;
	int pid = 0;
	unsigned long long bufID = 0;
	unsigned long long time = 0;

	if (iter == NULL) {
		pr_info("q2q is null pointer\n");
		return;
	}

	if ((cmd & (1 << GET_FPSGO_Q2Q_TIME)) == 0) {
		pr_info("q2q cmd is not support (0x%lx)\n", cmd);
		return;
	}

	pid = iter->pid;
	bufID = iter->buffer_id;
	time = iter->q2q_time;

	if (time > Q2QTIMEOUT_HIST)
		calculateTimeoutCouter(time);

	if (time > gq2qTimeoutInNs && gisGameMode) {
		pr_info("q2q (%d) (%llu) (%llu) ns limit (%llu) ns\n",
			pid,
			bufID,
			time,
			gq2qTimeoutInNs);
		n = snprintf(netlink_buf + pos,
				NETLINK_EVENT_MESSAGE_SIZE - pos,
				"%s:%d:%llu:%llu",
				NETLINK_EVENT_Q2QTIMEOUT,
				pid,
				bufID,
				time);

		if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
			return;

		mbraink_netlink_send_msg(netlink_buf);
	}
}

static void sendPerfTimeoutEvent(struct mbraink_gpu_perfidx_info *iter)
{
	char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};
	int n = 0;
	int pos = 0;
	int i = 0;

	if (iter == NULL)
		return;

	n = snprintf(netlink_buf + pos,
			NETLINK_EVENT_MESSAGE_SIZE - pos,
			"%s:%d:%llu:%llu:%llu",
			NETLINK_EVENT_PERFTIMEOUT,
			iter->pid,
			iter->bufid,
			iter->err_counter,
			iter->err_ts);

	if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
		return;

	pos += n;

	for (i = 0; i < PERFINDEX_BUF; i++) {
		if (pos+PERFINDEX_SLOT > NETLINK_EVENT_MESSAGE_SIZE) {
			pr_info("WG: over buffer size (%d) pos (%d), PERFINDEX_SLOT(%d)\n",
				NETLINK_EVENT_MESSAGE_SIZE,
				pos,
				PERFINDEX_SLOT);
			mbraink_netlink_send_msg(netlink_buf);

			//reset nl buffer.
			memset(netlink_buf, 0x00, sizeof(netlink_buf));
			pos = 0;
			n = 0;

			//create new buffer.
			n = snprintf(netlink_buf + pos,
					NETLINK_EVENT_MESSAGE_SIZE - pos,
					"%s:%d:%llu:%llu:%llu",
					NETLINK_EVENT_PERFTIMEOUT,
					iter->pid,
					iter->bufid,
					iter->err_counter,
					iter->err_ts);

			if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
				return;

			pos += n;
		}

		n = snprintf(netlink_buf + pos,
				NETLINK_EVENT_MESSAGE_SIZE - pos,
				":%d:%d:%llu",
				iter->perf_idx[i],
				iter->sbe_ctrl[i],
				iter->ts[i]);
		if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
			return;

		pos += n;
	}

	mbraink_netlink_send_msg(netlink_buf);

}

static void sendPerfLowoutEvent(struct mbraink_gpu_perfidx_info *iter, int opMode)
{
	char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};
	int n = 0;
	int pos = 0;

	if (iter == NULL)
		return;

	pos = 0;
	memset(netlink_buf, '\0', sizeof(netlink_buf));
	n = snprintf(netlink_buf + pos,
			NETLINK_EVENT_MESSAGE_SIZE - pos,
			"%s:%d:%llu:%d",
			NETLINK_EVENT_PERFLOWOUT,
			iter->pid,
			iter->bufid,
			opMode);

	if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
		return;

	pos += n;
	//pr_info("(%s)(%s)", __func__, netlink_buf);
	mbraink_netlink_send_msg(netlink_buf);
}

void fpsgo2mbrain_hint_perfinfo(unsigned long cmd, struct render_frame_info *iter)
{
	struct mbraink_gpu_perfidx_info *PerfIdxIter = NULL;
	struct mbraink_gpu_perfidx_info *PerfIdxIter2 = NULL;
	int currentIdx = 0;
	int opMode = 0;
	bool bPass = false;
	int pid = 0;
	unsigned long long bufID = 0;
	int perf_idx = 0;
	unsigned long long ts = 0;
	int sbe_ctrl = 0;

	if (iter == NULL) {
		pr_info("perf notify is null pointer\n");
		return;
	}

	if ((cmd & (1 << GET_FPSGO_PERF_IDX)) == 0) {
		pr_info("perf notify cmd is not support (0x%lx)\n", cmd);
		return;
	}


	pid = iter->pid;
	bufID = iter->buffer_id;
	perf_idx = iter->blc;
	ts = fpsgo_get_time();

	opMode = mbraink_v6991_gpu_getOpMode();

	mutex_lock(&mbk_g_perfidx_lock);

	hlist_for_each_entry(PerfIdxIter, &mbk_g_perfidx_list, hlist) {
		if (PerfIdxIter->pid == pid && (PerfIdxIter->bufid == bufID ||
				PerfIdxIter->bufid == 0))
			break;
	}

	//add a new one.
	if (PerfIdxIter == NULL) {
		struct mbraink_gpu_perfidx_info *new_perfidx_info = NULL;

		new_perfidx_info = vmalloc(sizeof(struct mbraink_gpu_perfidx_info));
		if (new_perfidx_info == NULL)
			goto out;

		memset(new_perfidx_info, 0x00, sizeof(struct mbraink_gpu_perfidx_info));
		new_perfidx_info->pid = pid;
		new_perfidx_info->bufid = bufID;
		new_perfidx_info->sbe_ctrl[0] = 0;
		new_perfidx_info->err_ts = 0;
		new_perfidx_info->err_counter = 0;
		new_perfidx_info->currentIdx = 0;
		new_perfidx_info->perf_idx[0] = perf_idx;
		new_perfidx_info->ts[0] = ts;
		new_perfidx_info->current_perf_idx = perf_idx;

		PerfIdxIter = new_perfidx_info;
		hlist_add_head(&PerfIdxIter->hlist, &mbk_g_perfidx_list);
	}

	if (PerfIdxIter->bufid == 0)
		PerfIdxIter->bufid = bufID;

	//update info.
	currentIdx = (PerfIdxIter->currentIdx > PERFINDEX_BUF-1) ? 0 : PerfIdxIter->currentIdx;
	PerfIdxIter->perf_idx[currentIdx] = perf_idx;
	PerfIdxIter->ts[currentIdx] = ts;
	PerfIdxIter->sbe_ctrl[currentIdx] = sbe_ctrl;
	PerfIdxIter->currentIdx = (currentIdx > PERFINDEX_BUF-2) ? 0 : currentIdx+1;
	PerfIdxIter->current_perf_idx = perf_idx;

	//process logic here.
	if (perf_idx >= gperfIdxLimit) {
		if (PerfIdxIter->err_counter != 0) {
			PerfIdxIter->err_counter++;
			if ((ts - PerfIdxIter->err_ts) > gperfIdxTimeoutInNs) {
				sendPerfTimeoutEvent(PerfIdxIter);
				//reset
				PerfIdxIter->err_counter = 0;
				PerfIdxIter->err_ts = 0;
			}
		} else {
			PerfIdxIter->err_ts = ts;
			PerfIdxIter->err_counter++;
		}
	} else {
		PerfIdxIter->err_counter = 0;
		PerfIdxIter->err_ts = 0;
	}

	//check if all perf indx = 0
	if (opMode != mbraink_op_mode_normal) {
		hlist_for_each_entry(PerfIdxIter2, &mbk_g_perfidx_list, hlist) {
			if (PerfIdxIter2->current_perf_idx > 0) {
				bPass = true;
				break;
			}
		}

		if (bPass == false)
			gPerfLoAlarmCount++;
		else
			gPerfLoAlarmCount = 0;

		if (gPerfLoAlarmCount >= PERFINDEX_LO_ALARM_COUNT) {
			gPerfLoAlarmCount = 0;
			sendPerfLowoutEvent(PerfIdxIter, opMode);
		}
	}

out:
	mutex_unlock(&mbk_g_perfidx_lock);

}

void fpsgo2mbrain_hint_deleteperfinfo(unsigned long cmd, struct render_frame_info *iter)
{
	struct mbraink_gpu_perfidx_info *PerfIndxIter = NULL;
	int pid = 0;
	unsigned long long bufID = 0;

	if (iter == NULL) {
		pr_info("perf notify is null pointer\n");
		return;
	}

	if ((cmd & (1 << GET_FPSGO_DELETE_INFO)) == 0) {
		pr_info("perf notify cmd is not support (0x%lx)\n", cmd);
		return;
	}

	pid = iter->pid;
	bufID = iter->buffer_id;

	mutex_lock(&mbk_g_perfidx_lock);
	hlist_for_each_entry(PerfIndxIter, &mbk_g_perfidx_list, hlist) {
		if ((PerfIndxIter->pid == pid) && (PerfIndxIter->bufid == bufID)) {
			hlist_del(&PerfIndxIter->hlist);
			vfree(PerfIndxIter);
			break;
		}
	}
	mutex_unlock(&mbk_g_perfidx_lock);

}

void game_mode_notify(int is_game_mode)
{
	char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};
	int n = 0;
	int pos = 0;

	n = snprintf(netlink_buf + pos,
			NETLINK_EVENT_MESSAGE_SIZE - pos,
			"%s:%d",
			NETLINK_EVENT_GAME_MODE_NOTIFY,
			is_game_mode);

	if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
		return;
	mbraink_netlink_send_msg(netlink_buf);
}

#if IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)
int recycle_workerevt_list(void)
{
	int ret = 0;
	struct mbraink_gpu_workerevt_info *iter;
	struct hlist_node *h;

	mutex_lock(&mbk_g_workerevt_lock);

	if (hlist_empty(&mbk_g_workerevt_list)) {
		gWorkerEvtCount = 0;
		ret = 1;
		goto out;
	}

	hlist_for_each_entry_safe(iter, h, &mbk_g_workerevt_list, hlist) {
		hlist_del(&iter->hlist);
		vfree(iter);
		ret = 1;
	}

	gWorkerEvtCount = 0;

out:
	mutex_unlock(&mbk_g_workerevt_lock);
	return ret;
}

static void sendGpuFenceTimeoutEvent(int pid, void *data, unsigned long long time)
{
	char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};
	struct ged_mali_event_info *pGpuInfo = NULL;
	int n = 0;
	int pos = 0;
	int i = 0;

	if (data == NULL)
		return;

	pGpuInfo = (struct ged_mali_event_info *)data;

	pr_info("pid(%d), time(%llu), fenceType(%d), pmodeFlag(%d), fenceTimeoutSec(%d)\n",
			pid,
			time,
			pGpuInfo->fenceType,
			pGpuInfo->pmode_flag,
			pGpuInfo->fenceTimeoutSec);

	n = snprintf(netlink_buf + pos,
			NETLINK_EVENT_MESSAGE_SIZE - pos,
			"%s:%d:%llu:%d:%d:%d",
			NETLINK_EVENT_GPUFENCETIMOEUT,
			pid,
			time,
			pGpuInfo->fenceType,
			pGpuInfo->pmode_flag,
			pGpuInfo->fenceTimeoutSec);

	if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
		return;

	pos += n;

	for (i = 0; i < MAX_RECORD_DATA; i++) {
		n = snprintf(netlink_buf + pos,
				NETLINK_EVENT_MESSAGE_SIZE - pos,
				":%d:%llu:%d:%d:%d:%d:%d:%llu",
				pGpuInfo->cs_error_info_array[i].pid,
				pGpuInfo->cs_error_info_array[i].ts,
				pGpuInfo->cs_error_info_array[i].group_handle,
				pGpuInfo->cs_error_info_array[i].csg_nr,
				pGpuInfo->cs_error_info_array[i].csi_index,
				pGpuInfo->cs_error_info_array[i].cs_fatal_type,
				pGpuInfo->cs_error_info_array[i].cs_fatal_data,
				pGpuInfo->cs_error_info_array[i].cs_fatal_info_data);

		if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
			return;

		pos += n;
	}

	for (i = 0; i < MAX_RECORD_DATA; i++) {
		n = snprintf(netlink_buf + pos,
				NETLINK_EVENT_MESSAGE_SIZE - pos,
				":%llu:%d",
				pGpuInfo->device_lost_info_array[i].ts,
				pGpuInfo->device_lost_info_array[i].reason);

		if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
			return;

		pos += n;
	}

	for (i = 0; i < MAX_RECORD_DATA; i++) {
		n = snprintf(netlink_buf + pos,
				NETLINK_EVENT_MESSAGE_SIZE - pos,
				":%llu:%d",
				pGpuInfo->gpu_reset_info_array[i].ts,
				pGpuInfo->gpu_reset_info_array[i].reason);

		if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
			return;

		pos += n;
	}

	//pr_info("NL:(%s)\n", netlink_buf);
	mbraink_netlink_send_msg(netlink_buf);

}

void gpu2mbrain_hint_fenceTimeoutNotify(int pid, void *data, unsigned long long time)
{
	sendGpuFenceTimeoutEvent(pid, data, time);
}

static void sendGpuResetDoneEvent(unsigned long long time)
{
	char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};
	int n = 0;
	int pos = 0;

	n = snprintf(netlink_buf + pos,
			NETLINK_EVENT_MESSAGE_SIZE - pos,
			"%s:%llu",
			NETLINK_EVENT_GPURESETDONE,
			time);

	if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
		return;

	//pr_info("GPU Rest Done NL:(%s)\n", netlink_buf);
	mbraink_netlink_send_msg(netlink_buf);
}

void gpu2mbrain_hint_GpuResetDoneNotify(unsigned long long time)
{
	sendGpuResetDoneEvent(time);
}

static void sendGpuWorkerEventNotify(void)
{
	char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};
	int n = 0;
	int pos = 0;
	struct mbraink_gpu_workerevt_info *iter;
	struct hlist_node *h;

	mutex_lock(&mbk_g_workerevt_lock);
	//prepare header.
	n = snprintf(netlink_buf + pos,
			NETLINK_EVENT_MESSAGE_SIZE - pos,
			"%s:%d",
			NETLINK_EVENT_GPUWORKERNOTIFY,
			gWorkerEvtCount);
	if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos) {
		pr_info("%s(%d): n(%d), pos(%d), over nl size\n",
			__func__, __LINE__, n, pos);
		goto out;
	}
	pos += n;

	hlist_for_each_entry_safe(iter, h, &mbk_g_workerevt_list, hlist) {
		n = snprintf(netlink_buf + pos,
				NETLINK_EVENT_MESSAGE_SIZE - pos,
				":%llu:%d:%d:%llu:%d:%d:%d:%d:%d:%d:%d:%d",
				iter->timestamp,
				iter->pid,
				iter->workerType,
				iter->exec_duration,
				iter->meta_data[0],
				iter->meta_data[1],
				iter->meta_data[2],
				iter->meta_data[3],
				iter->meta_data[4],
				iter->meta_data[5],
				iter->meta_data[6],
				iter->meta_data[7]);

		if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos) {
			pr_info("%s(%d): n(%d), pos(%d), over nl size\n",
				__func__, __LINE__, n, pos);
			goto out;
		}
		pos += n;
	}
	mbraink_netlink_send_msg(netlink_buf);

out:
	mutex_unlock(&mbk_g_workerevt_lock);
}

void gpu2mbrain_hint_GpuWorkerEventNotify(void *data, unsigned long long time)
{
	struct mbraink_gpu_workerevt_info *new_workerevt_info = NULL;
	struct ged_mali_worker_event_info *pGedMaliWorkerEventInfo = NULL;
	int i = 0;

	mutex_lock(&mbk_g_workerevt_lock);

	new_workerevt_info = vmalloc(sizeof(struct mbraink_gpu_workerevt_info));
	if (new_workerevt_info == NULL)
		goto out;

	pGedMaliWorkerEventInfo = (struct ged_mali_worker_event_info *)data;
	memset(new_workerevt_info, 0x00, sizeof(struct mbraink_gpu_workerevt_info));

	new_workerevt_info->timestamp = time;
	new_workerevt_info->pid = pGedMaliWorkerEventInfo->pid;
	new_workerevt_info->workerType = pGedMaliWorkerEventInfo->workerType;
	new_workerevt_info->exec_duration = pGedMaliWorkerEventInfo->exec_duration;
	for (i = 0; i < MAX_WORKER_META_DATA_NUM; i++)
		new_workerevt_info->meta_data[i] = pGedMaliWorkerEventInfo->meta_data[i];
	hlist_add_head(&new_workerevt_info->hlist, &mbk_g_workerevt_list);
	gWorkerEvtCount++;

out:
	mutex_unlock(&mbk_g_workerevt_lock);

	if (gWorkerEvtCount >= GPU_WORKEREVT_QUEUE_LIMIT) {
		pr_info("%s(%d): over limit send nl worker event\n", __func__, __LINE__);
		sendGpuWorkerEventNotify();
		recycle_workerevt_list();
	}

}

#endif

static int mbraink_v6991_gpu_setFeatureEnable(bool bEnable)
{
	if (bEnable == true) {
#if IS_ENABLED(CONFIG_MTK_FPSGO_V8) || IS_ENABLED(CONFIG_MTK_FPSGO)
		register_fpsgo_frame_info_callback(1 << GET_FPSGO_Q2Q_TIME,
			fpsgo2mbrain_hint_frameinfo);

		register_fpsgo_frame_info_callback(1 << GET_FPSGO_PERF_IDX,
			fpsgo2mbrain_hint_perfinfo);

		register_fpsgo_frame_info_callback(1 << GET_FPSGO_DELETE_INFO,
			fpsgo2mbrain_hint_deleteperfinfo);

		gPerfLoAlarmCount = 0;
#endif
#if IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)
		ged_mali_event_register_fence_timeout_callback(
			gpu2mbrain_hint_fenceTimeoutNotify);
		ged_mali_event_register_gpu_reset_done_callback(
			gpu2mbrain_hint_GpuResetDoneNotify);
		ged_mali_worker_event_register_callback(
			gpu2mbrain_hint_GpuWorkerEventNotify);
		gWorkerEvtCount = 0;
#endif
	} else {
#if IS_ENABLED(CONFIG_MTK_FPSGO_V8) || IS_ENABLED(CONFIG_MTK_FPSGO)
		unregister_fpsgo_frame_info_callback(fpsgo2mbrain_hint_frameinfo);

		unregister_fpsgo_frame_info_callback(fpsgo2mbrain_hint_perfinfo);

		unregister_fpsgo_frame_info_callback(fpsgo2mbrain_hint_deleteperfinfo);

		gPerfLoAlarmCount = 0;
#endif

#if IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)
		ged_mali_event_unregister_fence_timeout_callback(
			gpu2mbrain_hint_fenceTimeoutNotify);
		ged_mali_event_unregister_gpu_reset_done_callback(
			gpu2mbrain_hint_GpuResetDoneNotify);
		ged_mali_worker_event_unregister_callback(
			gpu2mbrain_hint_GpuWorkerEventNotify);
		gWorkerEvtCount = 0;
#endif

	}
	return 0;
}

void mbraink_v6991_gpu_setQ2QTimeoutInNS(unsigned long long q2qTimeoutInNS)
{
	gq2qTimeoutInNs = q2qTimeoutInNS;
}

unsigned long long mbraink_v6991_gpu_getQ2QTimeoutInNS(void)
{
	return gq2qTimeoutInNs;
}

void mbraink_v6991_gpu_fpsgoSetGameMode(int isGameMode)
{
	gisGameMode= isGameMode;
	game_mode_notify(gisGameMode);
}

static int mbraink_v6991_gpu_getOppInfo(struct mbraink_gpu_opp_info *gOppInfo)
{
	int ret = 0;
	int i = 0;
	unsigned int u32Count = 0;
	unsigned int u32Level = 0;
	struct GED_DVFS_OPP_STAT *report = NULL;
	u64 u64ts = 0;

	if (gOppInfo == NULL) {
		pr_info("Null gOppInfo\n");
		return -1;
	}

	u32Count = ged_dvfs_get_real_oppfreq_num();

	if (u32Count)
		report = vmalloc(sizeof(struct GED_DVFS_OPP_STAT) * u32Count);

	if ((report != NULL) &&
		ged_dvfs_query_opp_cost(report, u32Count, false, &u64ts) == 0) {
		gOppInfo->data1 = u64ts;
		for (i = 0; i < u32Count; i++) {
			u32Level = gpufreq_get_freq_by_idx(TARGET_DEFAULT, i)/1000;
			if (i < MAX_GPU_OPP_INFO_SZ) {
				gOppInfo->raw[i].data1 = u32Level;
				gOppInfo->raw[i].data2 = report[i].ui64Active;
				gOppInfo->raw[i].data3 = report[i].ui64Idle;
			}
		}
	} else {
		pr_info("can't allocate ged dvfs opp stat memory\n");
		ret = -1;
	}

	if (report != NULL)
		vfree(report);

	return ret;
}

static int mbraink_v6991_gpu_getStateInfo(struct mbraink_gpu_state_info *gStateInfo)
{
	int ret = 0;

	if (gStateInfo != NULL) {
		ret = ged_dvfs_query_power_state_time(&gStateInfo->data1,
							&gStateInfo->data2,
							&gStateInfo->data3,
							&gStateInfo->data4);
	} else {
		pr_info("gStateInfo is Null\n");
		ret = -1;
	}
	return ret;
}

static int mbraink_v6991_gpu_getLoadingInfo(struct mbraink_gpu_loading_info *gLoadingInfo)
{
	int ret = 0;

	if (gLoadingInfo != NULL) {
		ret = ged_dvfs_query_loading(&gLoadingInfo->data1,
						&gLoadingInfo->data2);
	} else {
		pr_info("gLoadingInfo is Null\n");
		ret = -1;
	}
	return ret;
}

void mbraink_v6991_gpu_setPerfIdxTimeoutInNS(unsigned long long perfIdxTimeoutInNS)
{
	gperfIdxTimeoutInNs = perfIdxTimeoutInNS;
}

void mbraink_v6991_gpu_setPerfIdxLimit(int perfIdxLimit)
{
	if (perfIdxLimit <= 100)
		gperfIdxLimit = perfIdxLimit;
}

void mbraink_v6991_gpu_dumpPerfIdxList(void)
{
	struct mbraink_gpu_perfidx_info *iter;
	int n = 0;

	mutex_lock(&mbk_g_perfidx_lock);
	hlist_for_each_entry(iter, &mbk_g_perfidx_list, hlist) {
		pr_info("perf info pid(%d) bid(%llu) cIdx (%d) last %d record\n",
			iter->pid,
			iter->bufid,
			iter->currentIdx,
			PERFINDEX_BUF);

		for (n = 0; n < PERFINDEX_BUF; n++) {
			pr_info("perf info (%d):  perf(%d) sbe(%d) ts(%llu)\n",
				n,
				iter->perf_idx[n],
				iter->sbe_ctrl[n],
				iter->ts[n]);
		}
	}
	mutex_unlock(&mbk_g_perfidx_lock);

}

static struct mbraink_gpu_ops mbraink_v6991_gpu_ops = {
	.setFeatureEnable = mbraink_v6991_gpu_setFeatureEnable,
	.getTimeoutCounterReport = mbraink_v6991_gpu_getTimeoutCouterReport,
	.getOppInfo = mbraink_v6991_gpu_getOppInfo,
	.getStateInfo = mbraink_v6991_gpu_getStateInfo,
	.getLoadingInfo = mbraink_v6991_gpu_getLoadingInfo,
	.setOpMode = mbraink_v6991_gpu_setOpMode,
};

int mbraink_v6991_gpu_init(void)
{
	int ret = 0;

	ret = register_mbraink_gpu_ops(&mbraink_v6991_gpu_ops);
	return ret;
}

int mbraink_v6991_gpu_deinit(void)
{
	int ret = 0;

	ret = unregister_mbraink_gpu_ops();
	ret = mbraink_v6991_gpu_setFeatureEnable(false);
	return ret;
}


