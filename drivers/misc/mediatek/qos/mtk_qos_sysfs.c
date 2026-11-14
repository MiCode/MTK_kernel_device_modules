// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/sysfs.h>

#include "mtk_qos_common.h"
#include "mtk_qos_share.h"
#include "mtk_qos_bound.h"

static ssize_t qos_bound_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", is_qos_bound_enabled());
}
static ssize_t qos_bound_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	qos_bound_enable(val);

	return count;
}
static DEVICE_ATTR_RW(qos_bound_enable);

static ssize_t qos_force_polling_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, " test\n");
}

static ssize_t qos_force_polling_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	qos_force_polling_mode(val, TRI_SW_PMQOS);

	return count;
}
static DEVICE_ATTR_RW(qos_force_polling);

static ssize_t qos_evt_tri_dbg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i = 0;
	char *ptr = buf;

	for (i=0; i<NR_TRI; i++) {
		evt_tri_dbg_tbl[i] = qos_share_sram_read_dbg(i * 4);
		ptr += snprintf(ptr, PAGE_SIZE - (ptr - buf), "evt_tri_dbg[%d] = %d\n", i, evt_tri_dbg_tbl[i]);
	}
	return ptr - buf;
}

static ssize_t qos_evt_tri_dbg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	qos_evt_tri_dbg_enable(val);
	return count;
}
static DEVICE_ATTR_RW(qos_evt_tri_dbg);

static ssize_t qos_bound_stress_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", is_qos_bound_stress_enabled());
}
static ssize_t qos_bound_stress_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	qos_bound_stress_enable(val);

	return count;
}
static DEVICE_ATTR_RW(qos_bound_stress_enable);

static ssize_t qos_bound_log_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", is_qos_bound_log_enabled());
}
static ssize_t qos_bound_log_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	qos_bound_log_enable(val);

	return count;
}
static DEVICE_ATTR_RW(qos_bound_log_enable);

static ssize_t qos_bound_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int qos_bound_count = get_qos_bound_count();
	unsigned int *qos_bound_buf = get_qos_bound_buf();

	return snprintf(buf, PAGE_SIZE, "count: %d, free: %d, cong: %d, full: %d\n",
			qos_bound_count, qos_bound_buf[0],
			qos_bound_buf[1], qos_bound_buf[2]);
}
static DEVICE_ATTR_RO(qos_bound_status);

static struct attribute *qos_attrs[] = {
	&dev_attr_qos_bound_enable.attr,
	&dev_attr_qos_bound_stress_enable.attr,
	&dev_attr_qos_bound_log_enable.attr,
	&dev_attr_qos_bound_status.attr,
	&dev_attr_qos_force_polling.attr,
	&dev_attr_qos_evt_tri_dbg.attr,
	NULL,
};

static struct attribute_group qos_attr_group = {
	.name = "qos",
	.attrs = qos_attrs,
};

int qos_add_interface(struct device *dev)
{
	int ret = sysfs_create_group(&dev->kobj, &qos_attr_group);

	pr_info("mtkqos: debug: %d\n", ret);
	return ret;
}

void qos_remove_interface(struct device *dev)
{
	sysfs_remove_group(&dev->kobj, &qos_attr_group);
}
