// SPDX-License-Identifier: GPL-2.0
/*
 * mca_event.c
 *
 *mca process event driver
 *
 * Copyright (c) 2023-2023 Xiaomi Technologies Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/mca/common/mca_sysfs.h>
#include <linux/mca/common/mca_event.h>

struct mca_event_dev {
	struct device *dev;
	struct kobject *sysfs_ne;
	struct mutex block_notify_mutex;
};

enum mca_event_sysfs_list {
	MCA_EVENT_SYSFS_TRIGGER = 0,
};

static struct mca_event_dev *g_mca_event_dev;
static struct blocking_notifier_head g_mca_event_bnh[MCA_EVENT_TYPE_END];

int mca_event_block_notify_register(unsigned int type, struct notifier_block *nb)
{
	if (!g_mca_event_dev)
		return -1;

	if (type >= MCA_EVENT_TYPE_END)
		return 0;

	if (!nb)
		return 0;

	return blocking_notifier_chain_register(&g_mca_event_bnh[type], nb);
}
EXPORT_SYMBOL(mca_event_block_notify_register);

int mca_event_block_notify_unregister(unsigned int type, struct notifier_block *nb)
{
	if (!g_mca_event_dev)
		return -1;

	if (type >= MCA_EVENT_TYPE_END)
		return 0;

	if (!nb)
		return 0;

	return blocking_notifier_chain_unregister(&g_mca_event_bnh[type], nb);
}
EXPORT_SYMBOL(mca_event_block_notify_unregister);

void mca_event_block_notify(unsigned int type, unsigned long event, void *data)
{
	struct mca_event_dev *l_dev = g_mca_event_dev;

	if (type >= MCA_EVENT_TYPE_END || !g_mca_event_dev)
		return;

	/* make sure notfiy seq */
	mutex_lock(&l_dev->block_notify_mutex);
	pr_info("receive blocking event type=%u event=%lu\n", type, event);
	blocking_notifier_call_chain(&g_mca_event_bnh[type], event, data);
	mutex_unlock(&l_dev->block_notify_mutex);
}
EXPORT_SYMBOL(mca_event_block_notify);

void mca_event_report_uevent(const struct mca_event_notify_data *n_data)
{
	char uevent_buf[MCA_EVENT_NOTIFY_SIZE] = { 0 };
	char *envp[2] = { uevent_buf, NULL };
	int ret;
	struct mca_event_dev *l_dev = g_mca_event_dev;

	if (!l_dev || !l_dev->sysfs_ne) {
		pr_err("l_dev or sysfs_ne is null\n");
		return;
	}

	if (!n_data || !n_data->event) {
		pr_err("n_data or event is null\n");
		return;
	}

	if (n_data->event_len >= MCA_EVENT_NOTIFY_SIZE) {
		pr_err("event_len is invalid\n");
		return;
	}

	memcpy(uevent_buf, n_data->event, n_data->event_len);
	pr_info("receive uevent_buf %d,%s\n", n_data->event_len, uevent_buf);

	ret = kobject_uevent_env(l_dev->sysfs_ne, KOBJ_CHANGE, envp);
	if (ret < 0)
		pr_err("notify uevent fail, ret=%d\n", ret);

}
EXPORT_SYMBOL(mca_event_report_uevent);

#if defined(CONFIG_SYSFS)
static ssize_t mca_event_sysfs_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);

static struct mca_sysfs_attr_info mca_event_sysfs_field_tbl[] = {
	mca_sysfs_attr_wo(mca_event_sysfs, 0220, MCA_EVENT_SYSFS_TRIGGER, trigger),
};

#define MCA_EVENT_SYSFS_ATTRS_SIZE  ARRAY_SIZE(mca_event_sysfs_field_tbl)

static struct attribute *mca_event_sysfs_attrs[MCA_EVENT_SYSFS_ATTRS_SIZE + 1];

static const struct attribute_group mca_event_sysfs_attr_group = {
	.attrs = mca_event_sysfs_attrs,
};

static ssize_t mca_event_sysfs_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mca_sysfs_attr_info *info = NULL;
	struct mca_event_dev *l_dev = g_mca_event_dev;
	struct mca_event_notify_data n_data;

	if (!l_dev)
		return -1;

	info = mca_sysfs_lookup_attr(attr->attr.name,
		mca_event_sysfs_field_tbl, MCA_EVENT_SYSFS_ATTRS_SIZE);
	if (!info)
		return -1;

	switch (info->sysfs_attr_name) {
	case MCA_EVENT_SYSFS_TRIGGER:
		if (count >= MCA_EVENT_WR_BUF_SIZE)
			return -1;

		n_data.event = buf;
		n_data.event_len = count;
		mca_event_report_uevent(&n_data);
		break;
	default:
		break;
	}

	return count;
}

static struct device *mca_event_sysfs_create_group(void)
{
	mca_sysfs_init_attrs(mca_event_sysfs_attrs,
		mca_event_sysfs_field_tbl, MCA_EVENT_SYSFS_ATTRS_SIZE);
	return mca_sysfs_create_group("xm_power", "mca_event",
		&mca_event_sysfs_attr_group);
}

static void mca_event_sysfs_remove_group(struct device *dev)
{
	mca_sysfs_remove_group("xm_power", dev, &mca_event_sysfs_attr_group);
}
#else
static inline struct device *mca_event_sysfs_create_group(void)
{
	return NULL;
}

static inline void mca_event_sysfs_remove_group(struct device *dev)
{
}
#endif /* CONFIG_SYSFS */

static void mca_event_block_notify_init(void)
{
	int i;

	for (i = 0; i < MCA_EVENT_TYPE_END; i++)
		BLOCKING_INIT_NOTIFIER_HEAD(&g_mca_event_bnh[i]);
}

static int __init mca_event_init(void)
{
	struct mca_event_dev *l_dev = NULL;

	l_dev = kzalloc(sizeof(*l_dev), GFP_KERNEL);
	if (!l_dev)
		return -ENOMEM;

	mca_event_block_notify_init();
	mutex_init(&l_dev->block_notify_mutex);
	l_dev->dev = mca_event_sysfs_create_group();
	if (l_dev->dev)
		l_dev->sysfs_ne = &l_dev->dev->kobj;

	g_mca_event_dev = l_dev;

	return 0;
}

static void __exit mca_event_exit(void)
{
	struct mca_event_dev *l_dev = g_mca_event_dev;

	if (!l_dev)
		return;

	mca_event_sysfs_remove_group(l_dev->dev);
	kfree(l_dev);
	g_mca_event_dev = NULL;
}

module_init(mca_event_init);
module_exit(mca_event_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("mca event driver");
MODULE_AUTHOR("liyuze1@xiaomi.com");

