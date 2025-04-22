// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/sysfs.h>
#include "hds.h"


static ssize_t klog_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = snprintf(buf, 32, "%u\n", g_hds_klog);
	if (ret < 0)
		apu_hds_err("show klog fail(%d)\n", g_hds_klog);

	return ret;
}

static ssize_t klog_store(struct device *dev,
	struct device_attribute *attr, const char *buf,
	size_t count)
{
	uint32_t val = 0;

	if (!kstrtouint(buf, 0, &val)) {
		apu_hds_info("set klog(%u)\n", val);
		g_hds_klog = val;
	}

	return count;
}
static DEVICE_ATTR_RW(klog);

static ssize_t plog_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = snprintf(buf, 32, "%u\n", g_hdev->pmu_lv);
	if (ret < 0)
		apu_hds_err("show plog fail(%u)\n", g_hdev->pmu_lv);

	return ret;
}

static ssize_t plog_store(struct device *dev,
	struct device_attribute *attr, const char *buf,
	size_t count)
{
	uint32_t val = 0;

	if (!kstrtouint(buf, 0, &val)) {
		apu_hds_info("set plog(%u)\n", val);
		g_hdev->pmu_lv = val;
	}

	return count;
}
static DEVICE_ATTR_RW(plog);

static ssize_t ptag_en_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = snprintf(buf, 32, "%u\n", g_hdev->pmu_tag_en);
	if (ret < 0)
		apu_hds_err("show plog fail(%u)\n", g_hdev->pmu_tag_en);

	return ret;
}

static ssize_t ptag_en_store(struct device *dev,
	struct device_attribute *attr, const char *buf,
	size_t count)
{
	uint32_t val = 0;

	if (!kstrtouint(buf, 0, &val)) {
		apu_hds_info("set ptag_en(%u)\n", val);
		g_hdev->pmu_tag_en = val;
	}

	return count;
}
static DEVICE_ATTR_RW(ptag_en);

static struct attribute *hds_log_attrs[] = {
	&dev_attr_klog.attr,
	&dev_attr_plog.attr,
	&dev_attr_ptag_en.attr,
	NULL,
};

static struct attribute_group apu_hds_log_attr_group = {
	.name	= "log",
	.attrs	= hds_log_attrs,
};

int apu_hds_sysfs_init(void)
{
	int ret = 0;

	ret = sysfs_create_group(&g_hdev->rpdev->dev.kobj, &apu_hds_log_attr_group);
	if (ret)
		apu_hds_err("create apu hds log attr fail, ret %d\n", ret);

	return ret;
}

void apu_hds_sysfs_deinit(void)
{
	sysfs_remove_group(&g_hdev->rpdev->dev.kobj, &apu_hds_log_attr_group);
}