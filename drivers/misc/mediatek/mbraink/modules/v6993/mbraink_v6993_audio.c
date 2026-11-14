// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>

#include <mbraink_modules_ops_def.h>
#include "mbraink_v6993_audio.h"

#include <swpm_module_psp.h>

#include <adsp_platform_driver.h>

#define ADSPINDEX_SLOT 80
#define ADSP_KEY 0x7A7A

static int mbraink_v6993_audio_getIdleRatioInfo(
	struct mbraink_audio_idleRatioInfo *pmbrainkAudioIdleRatioInfo)
{
	int ret = 0;
	struct ddr_sr_pd_times *ddr_sr_pd_times_ptr = NULL;
	struct timespec64 tv = { 0 };

	ddr_sr_pd_times_ptr = kmalloc(sizeof(struct ddr_sr_pd_times), GFP_KERNEL);
	if (!ddr_sr_pd_times_ptr) {
		ret = -1;
		goto End;
	}

	sync_latest_data();
	ktime_get_real_ts64(&tv);

	get_ddr_sr_pd_times(ddr_sr_pd_times_ptr);

	pmbrainkAudioIdleRatioInfo->timestamp = (tv.tv_sec*1000)+(tv.tv_nsec/1000000);
	pmbrainkAudioIdleRatioInfo->adsp_active_time = 0;
	pmbrainkAudioIdleRatioInfo->adsp_wfi_time = 0;
	pmbrainkAudioIdleRatioInfo->adsp_pd_time = 0;
	pmbrainkAudioIdleRatioInfo->s0_time = ddr_sr_pd_times_ptr->pd_time;
	pmbrainkAudioIdleRatioInfo->s1_time = ddr_sr_pd_times_ptr->sr_time;
	pmbrainkAudioIdleRatioInfo->mcusys_active_time = 0;
	pmbrainkAudioIdleRatioInfo->mcusys_pd_time = 0;
	pmbrainkAudioIdleRatioInfo->cluster_active_time = 0;
	pmbrainkAudioIdleRatioInfo->cluster_idle_time = 0;
	pmbrainkAudioIdleRatioInfo->cluster_pd_time = 0;
	pmbrainkAudioIdleRatioInfo->audio_hw_time = 0;

End:

	if (ddr_sr_pd_times_ptr != NULL)
		kfree(ddr_sr_pd_times_ptr);

	return ret;
}

void sendAudioAdspNotifyEvent(const struct adsp_mbrain_t *data, uint32_t totalCount)
{
	char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};
	const struct adsp_mbrain_t *pdata = NULL;
	int n = 0;
	int pos = 0;
	int i = 0, j = 0;

	if (data == NULL)
		return;

	//reset nl buffer.
	memset(netlink_buf, 0x00, sizeof(netlink_buf));

	n = snprintf(netlink_buf + pos,
			NETLINK_EVENT_MESSAGE_SIZE - pos,
			"%s:",
			NETLINK_EVENT_AUDIOADSPNOTIFY);
	if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
		return;
	pos += n;

	for (i = 0; i < totalCount; i++) {
		if (pos+ADSPINDEX_SLOT > NETLINK_EVENT_MESSAGE_SIZE) {
			pr_info("WG: over buffer size (%d) pos (%d), ADSPINDEX_SLOT(%d)\n",
				NETLINK_EVENT_MESSAGE_SIZE,
				pos,
				ADSPINDEX_SLOT);
			return;
		}

		pdata = &data[i];

		if (pdata == NULL)
			return;
		else if (pdata->magic_num != ADSP_KEY) {
			pr_info("WG: key not match(%d)\n", pdata->magic_num);
			continue;
		} else if (pdata->data_size == 0) {
			pr_info("WG: data size is 0\n");
			continue;
		} else if (pdata->data_size > ADSP_MBRAIN_EVENT_DATA_SIZE) {
			pr_info("WG: data size (%d) over limit(%d)\n",
				pdata->data_size,
				ADSP_MBRAIN_EVENT_DATA_SIZE);
			return;
		}

		//userID:eventType:counter:version:timestamp:data_size:data1:data2:data3:...
		n = snprintf(netlink_buf + pos,
				NETLINK_EVENT_MESSAGE_SIZE - pos,
				"%d:%d:%d:%d:%lld:%d",
				pdata->user_id,
				pdata->event_type,
				pdata->event_counter,
				pdata->version,
				pdata->time_stamp,
				pdata->data_size);
		if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
			return;
		pos += n;

		for (j = 0; j < pdata->data_size; j++) {
			n = snprintf(netlink_buf + pos,
					NETLINK_EVENT_MESSAGE_SIZE - pos,
					":%u",
					pdata->data[j]);
			if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
				return;
			pos += n;
		}

		n = snprintf(netlink_buf + pos,
				NETLINK_EVENT_MESSAGE_SIZE - pos,
				"\n");
		if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
			return;
		pos += n;
	}

	pr_info("(%s) final nl: (%s)\n", __func__, netlink_buf);
	mbraink_netlink_send_msg(netlink_buf);

}

void audio2mbrain_hint_adspnotifyinfo(const void *info, const size_t size)
{
	const struct adsp_mbrain_t *pstAdspMbrain = (const struct adsp_mbrain_t *)info;
	uint32_t totalCount = (uint32_t)size;

	if (info == NULL)
		return;

	sendAudioAdspNotifyEvent(pstAdspMbrain, totalCount);
}

static struct mbraink_audio_ops mbraink_v6993_audio_ops = {
	.setUdmFeatureEn = NULL,
	.getIdleRatioInfo = mbraink_v6993_audio_getIdleRatioInfo,
};

int mbraink_v6993_audio_init(void)
{
	int ret = 0;

	ret = register_mbraink_audio_ops(&mbraink_v6993_audio_ops);
	if (ret != 0)
		pr_info("%s(%d) register audio ops fail\n", __func__, __LINE__);

	ret = adsp_mbrain_register_callback(audio2mbrain_hint_adspnotifyinfo);
	if (ret != 0)
		pr_info("%s(%d) register audio adsp cb fail\n", __func__, __LINE__);

	return ret;
}

int mbraink_v6993_audio_deinit(void)
{
	int ret = 0;

	ret = unregister_mbraink_audio_ops();
	if (ret != 0)
		pr_info("%s(%d) unregister audio ops fail\n", __func__, __LINE__);

	ret = adsp_mbrain_unregister_callback();
	if (ret != 0)
		pr_info("%s(%d) unregister audio adsp cb fail\n", __func__, __LINE__);

	return ret;
}


