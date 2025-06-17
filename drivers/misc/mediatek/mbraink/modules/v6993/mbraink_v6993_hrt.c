// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <bridge/mbraink_bridge_hrt.h>

#include "mbraink_v6993_hrt.h"

void mbraink_v6993_display_hrt_notify(int threshold)
{
	char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};
	int n = 0;
	int pos = 0;
	struct timespec64 tv = { 0 };
	long long timestamp = 0;

	ktime_get_real_ts64(&tv);
	timestamp = (tv.tv_sec*1000)+(tv.tv_nsec/1000000);

	pr_info("%s : timestamp = %llu, thredhold = %d\n", __func__, timestamp, threshold);
	n = snprintf(netlink_buf + pos,
			NETLINK_EVENT_MESSAGE_SIZE - pos,
			"%s:%llu:%d",
			NETLINK_EVENT_DISPLAY_HRT,
			timestamp,
			threshold);

	if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
		return;
	mbraink_netlink_send_msg(netlink_buf);
}

void mbraink_v6993_isp_hrt_notify(int threshold)
{
	char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};
	int n = 0;
	int pos = 0;
	struct timespec64 tv = { 0 };
	long long timestamp = 0;

	ktime_get_real_ts64(&tv);
	timestamp = (tv.tv_sec*1000)+(tv.tv_nsec/1000000);

	pr_info("%s : timestamp = %llu, thredhold = %d\n", __func__, timestamp, threshold);
	n = snprintf(netlink_buf + pos,
			NETLINK_EVENT_MESSAGE_SIZE - pos,
			"%s:%llu:%d",
			NETLINK_EVENT_ISP_HRT,
			timestamp,
			threshold);

	if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
		return;
	mbraink_netlink_send_msg(netlink_buf);
}

static struct mbraink2hrt_ops mbraink_v6993_isp_hrt_ops = {
	.isp_hrt_notify = mbraink_v6993_isp_hrt_notify,
};

static ssize_t mbraink_platform_hrt_info_show(struct device *dev,
					      struct device_attribute *attr,
					      char *buf)
{
	char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};
	int n = 0;
	int pos = 0;
	struct timespec64 tv = { 0 };
	long long timestamp = 0;

	ktime_get_real_ts64(&tv);
	timestamp = (tv.tv_sec*1000)+(tv.tv_nsec/1000000);

	n = snprintf(netlink_buf + pos,
			NETLINK_EVENT_MESSAGE_SIZE - pos,
			"%s:%llu",
			NETLINK_EVENT_HRT,
			timestamp);

	if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
		return snprintf(buf, PAGE_SIZE, "hrt notified failed due to snprintf\n");
	mbraink_netlink_send_msg(netlink_buf);

	return snprintf(buf, PAGE_SIZE, "hrt notified, timestamp = %llu\n", timestamp);
}

static ssize_t mbraink_platform_hrt_info_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf,
						size_t count)
{
	return count;
}
static DEVICE_ATTR_RW(mbraink_platform_hrt_info);

int mbraink_v6993_hrt_init(struct device *dev)
{
	int ret = 0;

	if (dev)
		ret = device_create_file(dev, &dev_attr_mbraink_platform_hrt_info);

	ret = mtk_mbrain2disp_register_hrt_cb(mbraink_v6993_display_hrt_notify);
	if (ret)
		pr_info("%s: register display hrt callback failed\n", __func__);

	ret = mtk_mbrain2isp_register_ops(&mbraink_v6993_isp_hrt_ops);
	if (ret)
		pr_info("%s: register isp hrt callback failed\n", __func__);

	return ret;
}

int mbraink_v6993_hrt_deinit(struct device *dev)
{
	int ret = 0;

	if (dev)
		device_remove_file(dev, &dev_attr_mbraink_platform_hrt_info);

	ret = mtk_mbrain2disp_unregister_hrt_cb(mbraink_v6993_display_hrt_notify);
	if (ret)
		pr_info("%s: unregister display hrt callback failed\n", __func__);

	ret = mtk_mbrain2isp_unregister_ops();
	if (ret)
		pr_info("%s: unregister isp hrt callback failed\n", __func__);
	return ret;
}
