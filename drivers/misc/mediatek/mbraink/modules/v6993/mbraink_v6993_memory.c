// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>

#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <mbraink_modules_ops_def.h>
#include "mbraink_v6993_memory.h"

#include <swpm_module_psp.h>
#include <dvfsrc-mb.h>
#include <mtk_cm_mgr_mt6993.h>

#include <slbc_sdk.h>
#include <linux/kthread.h>
#include <linux/sched/task.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>

#include <dvfsrc-vsmr.h>

struct device *mbraink_v6993_device;
static struct task_struct *mbraink_slbc_thread;
static struct mbraink_v6993_slbc_info *slbc_info;
unsigned int slbc_period_ms = 10000;
unsigned int slbc_upload_cnt = 1000;
static DEFINE_MUTEX(mbraink_slbc_lock);

static int mbraink_v6993_memory_getDdrInfo(struct mbraink_memory_ddrInfo *pMemoryDdrInfo)
{
	int ret = 0;
	int i, j;
	int32_t ddr_freq_num = 0, ddr_bc_ip_num = 0;
	int32_t ddr_freq_check = 0, ddr_bc_ip_check = 0;

	struct ddr_act_times *ddr_act_times_ptr = NULL;
	struct ddr_sr_pd_times *ddr_sr_pd_times_ptr = NULL;
	struct ddr_ip_bc_stats *ddr_ip_stats_ptr = NULL;
	uint32_t record_cnt = 0;
	int32_t retV = 0;

	if (pMemoryDdrInfo == NULL) {
		pr_notice("pMemoryDdrInfo is NULL");
		ret = -1;
		goto End;
	}

	ddr_freq_num = get_ddr_freq_num();
	ddr_bc_ip_num = get_ddr_data_ip_num();

	ddr_act_times_ptr = kmalloc_array(ddr_freq_num, sizeof(struct ddr_act_times), GFP_KERNEL);
	if (!ddr_act_times_ptr) {
		ret = -1;
		goto End;
	}

	ddr_sr_pd_times_ptr = kmalloc(sizeof(struct ddr_sr_pd_times), GFP_KERNEL);
	if (!ddr_sr_pd_times_ptr) {
		ret = -1;
		goto End;
	}

	ddr_ip_stats_ptr = kmalloc_array(ddr_bc_ip_num,
								sizeof(struct ddr_ip_bc_stats),
								GFP_KERNEL);
	if (!ddr_ip_stats_ptr) {
		ret = -1;
		goto End;
	}

	for (i = 0; i < ddr_bc_ip_num; i++)
		ddr_ip_stats_ptr[i].bc_stats = kmalloc_array(ddr_freq_num,
								sizeof(struct ddr_bc_stats),
								GFP_KERNEL);
	for (i = 0; i < 2; i++) {
		retV = sync_latest_data();
		if (retV == SWPM_PSP_SUCCESS)
			break;
		pr_notice("%s(%d), (%d) retV(%d) sync latest data again\n",
			__func__, __LINE__, i, retV);
	}

	get_ddr_act_times(ddr_freq_num, ddr_act_times_ptr);
	get_ddr_sr_pd_times(ddr_sr_pd_times_ptr);
	get_ddr_freq_data_ip_stats(ddr_bc_ip_num, ddr_freq_num, ddr_ip_stats_ptr);
	get_data_record_number(&record_cnt);

	if (ddr_freq_num > MAX_DDR_FREQ_NUM) {
		pr_notice("ddr_freq_num over (%d)", MAX_DDR_FREQ_NUM);
		ddr_freq_check = MAX_DDR_FREQ_NUM;
	} else
		ddr_freq_check = ddr_freq_num;

	if (ddr_bc_ip_num > MAX_DDR_IP_NUM) {
		pr_notice("ddr_bc_ip_num over (%d)", MAX_DDR_IP_NUM);
		ddr_bc_ip_check = MAX_DDR_IP_NUM;
	} else
		ddr_bc_ip_check = ddr_bc_ip_num;

	pMemoryDdrInfo->srTimeInMs = ddr_sr_pd_times_ptr->sr_time;
	pMemoryDdrInfo->pdTimeInMs = ddr_sr_pd_times_ptr->pd_time;
	pMemoryDdrInfo->totalDdrFreqNum = ddr_freq_check;
	pMemoryDdrInfo->totalDdrIpNum = ddr_bc_ip_check;

	for (i = 0; i < ddr_freq_check; i++) {
		pMemoryDdrInfo->ddrActiveInfo[i].freqInMhz =
			ddr_act_times_ptr[i].freq;
		pMemoryDdrInfo->ddrActiveInfo[i].totalActiveTimeInMs =
			ddr_act_times_ptr[i].active_time;
		for (j = 0; j < ddr_bc_ip_check; j++) {
			pMemoryDdrInfo->ddrActiveInfo[i].totalIPActiveTimeInMs[j] =
				ddr_ip_stats_ptr[j].bc_stats[i].value;
		}
	}

	pMemoryDdrInfo->updateCnt = record_cnt;

End:
	if (ddr_act_times_ptr != NULL)
		kfree(ddr_act_times_ptr);

	if (ddr_sr_pd_times_ptr != NULL)
		kfree(ddr_sr_pd_times_ptr);

	if (ddr_ip_stats_ptr != NULL) {
		for (i = 0; i < ddr_bc_ip_num; i++) {
			if (ddr_ip_stats_ptr[i].bc_stats != NULL)
				kfree(ddr_ip_stats_ptr[i].bc_stats);
		}
		kfree(ddr_ip_stats_ptr);
	}

	return ret;
}

#if IS_ENABLED(CONFIG_DEVICE_MODULES_SCSI_UFS_MEDIATEK)

int ufs2mbrain_event_notify(struct ufs_mbrain_event *event)
{
	char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};
	int n __maybe_unused = 0;

	if (!event) {
		pr_info("[%s] event is null\n", __func__);
		return -1;
	}

	n = snprintf(netlink_buf, NETLINK_EVENT_MESSAGE_SIZE,
		"%s:%d:%d:%llu:%d:%d:%d",
		NETLINK_EVENT_UFS_NOTIFY,
		(unsigned int)event->ver,
		(unsigned int)event->data->event,
		(unsigned long long)event->data->mb_ts,
		(unsigned int)event->data->val,
		(unsigned int)event->data->gear_rx,
		(unsigned int)event->data->gear_tx
	);

	if (n < 0 || n > NETLINK_EVENT_MESSAGE_SIZE)
		pr_info("%s : snprintf error n = %d\n", __func__, n);
	else
		mbraink_netlink_send_msg(netlink_buf);

	return 0;
}

#endif

static int mbraink_v6993_memory_getMdvInfo(struct mbraink_memory_mdvInfo  *pMemoryMdv)
{
	int ret = 0;
	int i = 0;
	struct mtk_dvfsrc_header srcHeader;

	if (pMemoryMdv == NULL) {
		ret = -1;
		goto End;
	}

	if (MAX_MDV_SZ != MAX_DATA_SIZE) {
		pr_notice("mdv data sz mis-match");
		ret = -1;
		goto End;
	}

	memset(&srcHeader, 0, sizeof(struct mtk_dvfsrc_header));
	dvfsrc_get_data(&srcHeader);
	pMemoryMdv->mid = srcHeader.module_id;
	pMemoryMdv->ver = srcHeader.version;
	pMemoryMdv->pos = srcHeader.data_offset;
	pMemoryMdv->size = srcHeader.data_length;
	for (i = 0; i < MAX_MDV_SZ; i++)
		pMemoryMdv->raw[i] = srcHeader.data[i];

End:
	return ret;
}

static int mbraink_v6993_get_ufs_info(struct mbraink_ufs_info  *ufs_info)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_DEVICE_MODULES_SCSI_UFS_MEDIATEK)
	struct ufs_mbrain_dev_info mb_dev_info;
	struct device_node *phy_node = NULL;
	struct platform_device *phy_pdev = NULL;
#endif

	if (ufs_info == NULL) {
		ret = -1;
		goto End;
	}

#if IS_ENABLED(CONFIG_DEVICE_MODULES_SCSI_UFS_MEDIATEK)

	if (mbraink_v6993_device) {
		memset(&mb_dev_info, 0, sizeof(struct ufs_mbrain_dev_info));
		phy_node = of_parse_phandle(mbraink_v6993_device->of_node, "ufsnotify", 0);
		if (phy_node) {
			phy_pdev = of_find_device_by_node(phy_node);
			if (phy_pdev) {
				ret = ufs_mb_get_info(phy_pdev, &mb_dev_info);
				if (ret == 0) {
					if (mb_dev_info.model)
						memcpy(ufs_info->model,
						    mb_dev_info.model, SCSI_MODEL_LEN);

					if (mb_dev_info.rev)
						memcpy(ufs_info->rev,
						    mb_dev_info.rev, SCSI_REV_LEN);
				}
			}
		}
	}
#endif

End:
	return ret;
}

static int mbraink_v6993_memory_getEmiInfo(struct mbraink_memory_emiInfo *pMemoryEmiInfo)
{
	int i;
	int32_t emi_freq_num = 0;
	int ret = 0;
	struct freq_duration *emi_duration_ptr = NULL;
	uint32_t record_cnt = 0;
	int32_t retV = 0;
	int32_t emi_freq_check = 0;

	emi_freq_num = get_emi_freq_num();

	emi_duration_ptr = kmalloc_array(emi_freq_num, sizeof(struct freq_duration), GFP_KERNEL);
	if (!emi_duration_ptr) {
		ret = -1;
		goto End;
	}

	for (i = 0; i < 2; i++) {
		retV = sync_latest_data();
		if (retV == SWPM_PSP_SUCCESS)
			break;
		pr_notice("%s(%d), (%d) retV(%d) sync latest data again\n",
			__func__, __LINE__, i, retV);
	}

	get_emi_freq_duration(emi_freq_num, emi_duration_ptr);
	get_data_record_number(&record_cnt);

	if (emi_freq_num > MAX_EMI_FREQ_NUM) {
		pr_notice("emi_freq_num over (%d)", MAX_EMI_FREQ_NUM);
		emi_freq_check = MAX_EMI_FREQ_NUM;
	} else
		emi_freq_check = emi_freq_num;

	for (i = 0; i < emi_freq_check; i++) {
		pMemoryEmiInfo->emiActiveInfo[i].freqInMhz =
			emi_duration_ptr[i].freq;
		pMemoryEmiInfo->emiActiveInfo[i].totalActiveTimeInMs =
			emi_duration_ptr[i].duration;
	}

	pMemoryEmiInfo->totalEmiFreqNum = emi_freq_check;
	pMemoryEmiInfo->updateCnt = record_cnt;

End:

	if (emi_duration_ptr != NULL)
		kfree(emi_duration_ptr);

	return ret;
}

static int mbraink_v6993_memory_getCmProfileInfo
	(struct mbraink_memory_cmProfileInfo *pCmProfileInfo)
{
	int i;
	struct cm_profile_info cmProfInfo;
	int ret = 0;
	int32_t cm_profile_check = 0;

	if (pCmProfileInfo == NULL)	{
		pr_info("(%s)cm profile is null\n", __func__);
		return -1;
	}

	memset(&cmProfInfo, 0x00, sizeof(struct cm_profile_info));
	ret = cm_profile_get_bw(&cmProfInfo);
	if (ret != 0) {
		pr_info("(%s) get cm profile fail\n", __func__);
		return ret;
	}

	if (CM_WRAP_EMI_MAX > MAX_CM_WRAP_NUM)
		cm_profile_check = MAX_CM_WRAP_NUM;
	else
		cm_profile_check = CM_WRAP_EMI_MAX;

	for (i = 0; i < cm_profile_check; i++)
		pCmProfileInfo->info[i] = cmProfInfo.info[i];

	pCmProfileInfo->totalCmWrapNum = cm_profile_check;
	pCmProfileInfo->updateCnt = cmProfInfo.update_cnt;

	return ret;
}

static int mbraink_v6993_memory_getVsmrInfo(struct mbraink_memory_vsmrInfo  *pMemoryVsmr)
{
	int ret = 0;
	int i = 0;
	struct mtk_vsmr_header vsmrHeader;

	if (pMemoryVsmr == NULL) {
		ret = -1;
		goto End;
	}

	if (MAX_VSMR_SZ != MAX_VSMR_DATA_SIZE) {
		pr_notice("vsmr data sz mis-match");
		ret = -1;
		goto End;
	}

	memset(&vsmrHeader, 0, sizeof(struct mtk_vsmr_header));
	vsmr_get_data(&vsmrHeader);
	pMemoryVsmr->mid = vsmrHeader.module_id;
	pMemoryVsmr->ver = vsmrHeader.version;
	pMemoryVsmr->pos = vsmrHeader.data_offset;
	pMemoryVsmr->size = vsmrHeader.data_length;
	pMemoryVsmr->timer = vsmrHeader.timer;
	pMemoryVsmr->vsmr_support = vsmrHeader.vsmr_support;
	pMemoryVsmr->total_size = MAX_VSMR_DATA_SIZE;
	pMemoryVsmr->level_size = 16;
	pMemoryVsmr->vt_size = MAX_VSMR_DATA_SIZE/16;

	for (i = 0; i < MAX_VSMR_SZ; i++)
		pMemoryVsmr->raw[i] = vsmrHeader.last_data[i];

End:
	return ret;
}

static struct mbraink_memory_ops mbraink_v6993_memory_ops = {
	.getDdrInfo = mbraink_v6993_memory_getDdrInfo,
	.getMdvInfo = mbraink_v6993_memory_getMdvInfo,
	.get_ufs_info = mbraink_v6993_get_ufs_info,
	.getEmiInfo = mbraink_v6993_memory_getEmiInfo,
	.getCmProfileInfo = mbraink_v6993_memory_getCmProfileInfo,
	.getVsmrInfo = mbraink_v6993_memory_getVsmrInfo,
};

/*This function must be called in mutex*/
static void mbraink_slbc_data_send(unsigned int cnt)
{
	char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};
	int n = 0;
	int pos = 0;
	int idx = 0;

	for (idx = 0; idx < cnt; idx++) {
		if (idx % 16 == 0) {
			pos = 0;
			n = snprintf(netlink_buf,
				NETLINK_EVENT_MESSAGE_SIZE, "%s ",
				NETLINK_EVENT_SYSSLBC);
			if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE)
				break;
			pos += n;
		}
		n = snprintf(netlink_buf + pos, NETLINK_EVENT_MESSAGE_SIZE - pos,
			"%llu:%llu:%llu:%u ",
			slbc_info[idx].start,
			slbc_info[idx].end,
			slbc_info[idx].slbc_gpu_wb,
			slbc_info[idx].count);
		if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
			break;
		pos += n;

		if (idx % 16 == 15 || (idx == (cnt - 1) && idx % 16 != 0)) {
			mbraink_netlink_send_msg(netlink_buf);
			memset(netlink_buf, 0, NETLINK_EVENT_MESSAGE_SIZE);
		}
	}
}

static int mbraink_slbc_thread_func(void *data)
{
	long long timestamp = 0;
	struct timespec64 tv = { 0 };
	unsigned int val = 0;
	unsigned long long sum = 0;
	unsigned int idx = 0;
	unsigned int count = 0;

	while (!kthread_should_stop()) {
		slbc_get_gpu_wb(&val);

		mutex_lock(&mbraink_slbc_lock);

		if (slbc_info) {
			if (count == 0) {
				ktime_get_real_ts64(&tv);
				timestamp = (tv.tv_sec*1000)+(tv.tv_nsec/1000000);
				slbc_info[idx].start = timestamp;
			}

			if (count < slbc_period_ms) {
				count++;
				sum += (unsigned long long)(val);
			} else {
				ktime_get_real_ts64(&tv);
				timestamp = (tv.tv_sec*1000)+(tv.tv_nsec/1000000);
				slbc_info[idx].end = timestamp;
				slbc_info[idx].count = count;
				slbc_info[idx].slbc_gpu_wb = sum;
				idx++;
				sum = 0;
				count = 0;
			}
			if (idx == slbc_upload_cnt) {
				idx = 0;
				mbraink_slbc_data_send(slbc_upload_cnt);
				memset(slbc_info, 0,
					sizeof(struct mbraink_v6993_slbc_info) * slbc_upload_cnt);
			}
		}
		mutex_unlock(&mbraink_slbc_lock);
		usleep_range(530, 550);
	}
	return 0;
}

static ssize_t mbraink_platform_slbc_info_show(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	return snprintf(buf, PAGE_SIZE, "slbc_period_ms = %u, slbc_upload_cnt=%u...\n"
			, slbc_period_ms, slbc_upload_cnt);
}

static ssize_t mbraink_platform_slbc_info_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf,
						size_t count)
{
	unsigned int command;
	int retSize = 0;
	unsigned int upload_cnt = 0;

	retSize = sscanf(buf, "%d %u %u", &command, &slbc_period_ms, &upload_cnt);
	if (retSize == -1)
		return 0;
	if (slbc_period_ms < 400)
		slbc_period_ms = 400;
	if (slbc_period_ms > 1000)
		slbc_period_ms = 1000;
	if (upload_cnt < 25)
		upload_cnt = 25;
	if (upload_cnt > 180)
		upload_cnt = 180;

	pr_info("%s: Get Command (%d), slbc_period_ms (%u)\n",
		__func__,
		command,
		slbc_period_ms);

	if (command == 1) {
		mutex_lock(&mbraink_slbc_lock);

		if (!slbc_info) {
			slbc_upload_cnt = upload_cnt;
			pr_info("%s: Get size(%d)\n",
				__func__,
				slbc_upload_cnt);
			slbc_info =
				vmalloc(sizeof(struct mbraink_v6993_slbc_info) * slbc_upload_cnt);
			if (!slbc_info)
				pr_err("%s: slbc_info buffer create failed!\n", __func__);
		} else
			pr_info("slbc_info buffer has already existed!\n");

		mutex_unlock(&mbraink_slbc_lock);

		if (!mbraink_slbc_thread) {
			mbraink_slbc_thread =
				kthread_run(mbraink_slbc_thread_func,
					NULL,
					"mbraink_v6993_slbc_thread");
			if (!mbraink_slbc_thread)
				pr_info("%s: mbraink_v6993_slbc_thread create fail!\n",
					__func__);
		} else
			pr_info("mbraink_v6993_slbc_thread has already existed!\n");
	} else {
		if (mbraink_slbc_thread) {
			kthread_stop(mbraink_slbc_thread);
			mbraink_slbc_thread = NULL;
		} else
			pr_notice("mbraink_v6993_slbc_thread does not exist!\n");

		mutex_lock(&mbraink_slbc_lock);

		if (slbc_info) {
			vfree(slbc_info);
			slbc_info = NULL;
		} else
			pr_notice("slbc_info buffer does not exist!\n");

		mutex_unlock(&mbraink_slbc_lock);
	}

	return count;
}
static DEVICE_ATTR_RW(mbraink_platform_slbc_info);

int mbraink_v6993_memory_init(struct device *dev)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_DEVICE_MODULES_SCSI_UFS_MEDIATEK)
	struct device_node *phy_node = NULL;
	struct platform_device *phy_pdev = NULL;
#endif

	if (dev)
		ret = device_create_file(dev, &dev_attr_mbraink_platform_slbc_info);

	ret = register_mbraink_memory_ops(&mbraink_v6993_memory_ops);

	if (dev) {
		mbraink_v6993_device = dev;

#if IS_ENABLED(CONFIG_DEVICE_MODULES_SCSI_UFS_MEDIATEK)
		phy_node = of_parse_phandle(dev->of_node, "ufsnotify", 0);
		if (phy_node) {
			phy_pdev = of_find_device_by_node(phy_node);
			if (phy_pdev)
				ufs_mb_register(phy_pdev, ufs2mbrain_event_notify);
		}
#endif
	} else
		pr_notice("memory init dev is NULL");

	return ret;
}

int mbraink_v6993_memory_deinit(struct device *dev)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_DEVICE_MODULES_SCSI_UFS_MEDIATEK)
	struct device_node *phy_node = NULL;
	struct platform_device *phy_pdev = NULL;
#endif

	if (dev)
		device_remove_file(dev, &dev_attr_mbraink_platform_slbc_info);
	ret = unregister_mbraink_memory_ops();

#if IS_ENABLED(CONFIG_DEVICE_MODULES_SCSI_UFS_MEDIATEK)
	if (dev) {
		phy_node = of_parse_phandle(dev->of_node, "ufsnotify", 0);
		if (phy_node) {
			phy_pdev = of_find_device_by_node(phy_node);
			if (phy_pdev)
				ufs_mb_unregister(phy_pdev);
		}
	}
#endif
	mbraink_v6993_device = NULL;

	return ret;
}

