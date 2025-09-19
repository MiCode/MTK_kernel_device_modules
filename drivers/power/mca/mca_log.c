// SPDX-License-Identifier: GPL-2.0
/*
 *mca_log.c
 *
 * mca log buffer driver
 *
 * Copyright (c) 2024-2024 Xiaomi Technologies Co., Ltd.
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
#include <linux/module.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/stdarg.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include <linux/time64.h>
#include <linux/timekeeping.h>
#include <linux/mca/common/mca_sysfs.h>
#include <linux/mca/common/mca_event.h>
#include <linux/mca/common/mca_log.h>

#define MCA_LOG_SINGLE_LOG_MAX_LEN 256
#define MCA_LOG_BUFF_COUNT 20
#define MCA_LOG_BUFF_DEFAULT_COUNT 4

static uint charge_boot_mode;

module_param(charge_boot_mode, uint, 0444);
MODULE_PARM_DESC(charge_boot_mode, "charge_boot_mode");

enum mca_log_level {
	MCA_LOG_LEVEL_ERROR = 0,
	MCA_LOG_LEVEL_INFO,
	MCA_LOG_LEVEL_DEBUG,
	MCA_LOG_MAX_LEVEL,
};

struct mca_log_buf_info {
	struct device *dev;
	int init_flag;
	int log_level;
	int console_level;
	int enable;
	int index;
	int count;
	int lastest_cache;
	int cache_num;
	int max_cache_num;
	char *log_buff_cache[MCA_LOG_BUFF_COUNT];
	char *log_buff;
};

struct mca_cur_time {
	struct rtc_time tm;
	long cur_ms;
};

struct mca_charge_log_ops_data {
	struct mca_log_charge_log_ops *ops;
	void *data;
};

enum mca_log_sysfs_type {
	MCA_LOG_ATTR_LOG_LEVEL = 0,
	MCA_LOG_ATTR_LOG_ENABLE,
	MCA_LOG_ATTR_LOG_INDEX,
	MCA_LOG_ATTR_LOG_CUR_BUFF,
	MCA_LOG_ATTR_LOG_MAX_BUFF_NUM,
	MCA_LOG_ATTR_DUMP_BUFFER_LOG,
	MCA_LOG_ATTR_DUMP_CHARGE_LOG_HEAD,
	MCA_LOG_ATTR_DUMP_CHARGE_LOG_INFO,
	MCA_LOG_ATTR_CONSOLE_LOG_LEVEL,
};

static struct mca_log_buf_info g_mca_log_info;
static DEFINE_SPINLOCK(g_mca_log_lock);


static struct mca_charge_log_ops_data g_mca_charge_log_data[MCA_CHARGE_LOG_ID_MAX];

static void mca_log_get_cur_time(struct mca_cur_time *tm)
{
	struct timespec64 now = { 0 };

	ktime_get_real_ts64(&now);
	now.tv_sec -= sys_tz.tz_minuteswest * 60;
	rtc_time64_to_tm(now.tv_sec, &tm->tm);
	tm->cur_ms = now.tv_nsec / 1000000;
}

static void mca_log_proc_log_info(char *buff, int len,
	struct mca_log_buf_info *info)
{
	int flag = 0;
	struct mca_event_notify_data n_data = { 0 };
	static int full_num;
	int send_uevent = 0;
	unsigned long flags;

	if (buff[len - 1] != '\n' && len < MCA_LOG_SINGLE_LOG_MAX_LEN - 1) {
		buff[len] = '\n';
		len += 1;
	}

	spin_lock_irqsave(&g_mca_log_lock, flags);
	if (!info->log_buff) {
		info->log_buff = kzalloc(PAGE_SIZE, GFP_ATOMIC);
		if (!info->log_buff) {
			spin_unlock_irqrestore(&g_mca_log_lock, flags);
			return;
		}
	}
	if (len + info->count >= PAGE_SIZE - 1) {
		flag = 1;
		info->cache_num++;
		full_num++;
		if (info->cache_num >= info->max_cache_num)
			info->cache_num = info->max_cache_num;
		kfree(info->log_buff_cache[info->lastest_cache]);
		info->log_buff_cache[info->lastest_cache] = NULL;
		info->log_buff_cache[info->lastest_cache] = info->log_buff;
		info->log_buff = NULL;
		info->lastest_cache++;
		if (info->lastest_cache == info->max_cache_num) {
			info->lastest_cache = 0;
			kfree(info->log_buff_cache[0]);
			info->log_buff_cache[0] = NULL;
		}
	}

	if (flag) {
		info->count = 0;
		info->log_buff = kzalloc(PAGE_SIZE, GFP_ATOMIC);
		if (!info->log_buff) {
			spin_unlock_irqrestore(&g_mca_log_lock, flags);
			return;
		}
	}
	memcpy(info->log_buff + info->count, buff, len);
	info->count += len;
	if (full_num >= MCA_LOG_BUFF_DEFAULT_COUNT)
		send_uevent = 1;

	spin_unlock_irqrestore(&g_mca_log_lock, flags);
	if (send_uevent && !in_interrupt()) {
		full_num = 0;
		if (info->enable) {
			n_data.event = "MCA_LOG_FULL_EVENT";
			n_data.event_len = 18;
			mca_event_report_uevent(&n_data);
		}
	}
}

int mca_log_get_charge_boot_mode(void)
{
	return charge_boot_mode;
}
EXPORT_SYMBOL(mca_log_get_charge_boot_mode);

void __mca_log_err(const char *format, ...)
{
	struct mca_cur_time cur_time = { 0 };
	va_list args;
	int len;
	char buff[MCA_LOG_SINGLE_LOG_MAX_LEN] = { 0 };
	struct mca_log_buf_info *info = &g_mca_log_info;

	if (!info->init_flag) {
		va_start(args, format);
		len = vsnprintf(buff, MCA_LOG_SINGLE_LOG_MAX_LEN - 1, format, args);
		va_end(args);
		buff[MCA_LOG_SINGLE_LOG_MAX_LEN - 1] = '\0';
		pr_err("%s\n", buff);
		return;
	}

	mca_log_get_cur_time(&cur_time);
	len = snprintf(buff, MCA_LOG_SINGLE_LOG_MAX_LEN - 1, "[%02d:%02d:%02d:%03ld-E]",
		cur_time.tm.tm_hour, cur_time.tm.tm_min, cur_time.tm.tm_sec, cur_time.cur_ms);
	va_start(args, format);
	len += vsnprintf(buff + len, MCA_LOG_SINGLE_LOG_MAX_LEN - 1 - len, format, args);
	va_end(args);
	pr_err("%s", buff);
	mca_log_proc_log_info(buff, len, info);
}
EXPORT_SYMBOL(__mca_log_err);

void __mca_log_info(const char *format, ...)
{
	struct mca_cur_time cur_time = { 0 };
	va_list args;
	int len;
	char buff[MCA_LOG_SINGLE_LOG_MAX_LEN] = { 0 };
	struct mca_log_buf_info *info = &g_mca_log_info;

	if (!info->init_flag)
		return;

	mca_log_get_cur_time(&cur_time);
	len = snprintf(buff, MCA_LOG_SINGLE_LOG_MAX_LEN - 1, "[%02d:%02d:%02d:%03ld-I]",
		cur_time.tm.tm_hour, cur_time.tm.tm_min, cur_time.tm.tm_sec, cur_time.cur_ms);
	va_start(args, format);
	len += vsnprintf(buff + len, MCA_LOG_SINGLE_LOG_MAX_LEN - 1 - len, format, args);
	va_end(args);

	if (info->console_level >= MCA_LOG_LEVEL_INFO || charge_boot_mode)
		pr_info("%s", buff);

	if (info->log_level < MCA_LOG_LEVEL_INFO)
		return;

	mca_log_proc_log_info(buff, len, info);
}
EXPORT_SYMBOL(__mca_log_info);

void __mca_log_debug(const char *format, ...)
{
	struct mca_cur_time cur_time = { 0 };
	va_list args;
	int len;
	char buff[MCA_LOG_SINGLE_LOG_MAX_LEN] = { 0 };
	struct mca_log_buf_info *info = &g_mca_log_info;

	if (!info->init_flag || info->log_level < MCA_LOG_LEVEL_DEBUG)
		return;

	mca_log_get_cur_time(&cur_time);
	len = snprintf(buff, MCA_LOG_SINGLE_LOG_MAX_LEN - 1, "[%02d:%02d:%02d:%03ld-D]",
		cur_time.tm.tm_hour, cur_time.tm.tm_min, cur_time.tm.tm_sec, cur_time.cur_ms);
	va_start(args, format);
	len += vsnprintf(buff + len, MCA_LOG_SINGLE_LOG_MAX_LEN - 1 - len, format, args);
	va_end(args);
	if (info->console_level == MCA_LOG_LEVEL_DEBUG)
		pr_info("%s", buff);

	if (info->log_level < MCA_LOG_LEVEL_DEBUG)
		return;

	mca_log_proc_log_info(buff, len, info);
}
EXPORT_SYMBOL(__mca_log_debug);

void mca_log_charge_log_register(enum mca_charge_log_id_ele type,
	struct mca_log_charge_log_ops *ops, void *data)
{
	if (type >= MCA_CHARGE_LOG_ID_MAX)
		return;

	g_mca_charge_log_data[type].ops = ops;
	g_mca_charge_log_data[type].data = data;
}
EXPORT_SYMBOL(mca_log_charge_log_register);

#ifdef CONFIG_SYSFS
static ssize_t mca_log_sysfs_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t mca_log_sysfs_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);

static struct mca_sysfs_attr_info mca_log_sysfs_field_tbl[] = {
	mca_sysfs_attr_rw(mca_log_sysfs, 0660, MCA_LOG_ATTR_LOG_LEVEL, log_level),
	mca_sysfs_attr_rw(mca_log_sysfs, 0660, MCA_LOG_ATTR_LOG_ENABLE, log_enable),
	mca_sysfs_attr_wo(mca_log_sysfs, 0220, MCA_LOG_ATTR_LOG_INDEX, log_index),
	mca_sysfs_attr_ro(mca_log_sysfs, 0440, MCA_LOG_ATTR_LOG_CUR_BUFF, cur_buff),
	mca_sysfs_attr_ro(mca_log_sysfs, 0440, MCA_LOG_ATTR_LOG_MAX_BUFF_NUM, max_buffer_num),
	mca_sysfs_attr_ro(mca_log_sysfs, 0440, MCA_LOG_ATTR_DUMP_BUFFER_LOG, dump_log_buff),
	mca_sysfs_attr_ro(mca_log_sysfs, 0440, MCA_LOG_ATTR_DUMP_CHARGE_LOG_HEAD, charge_log_head),
	mca_sysfs_attr_ro(mca_log_sysfs, 0440, MCA_LOG_ATTR_DUMP_CHARGE_LOG_INFO, charge_log_info),
	mca_sysfs_attr_rw(mca_log_sysfs, 0660, MCA_LOG_ATTR_CONSOLE_LOG_LEVEL, console_log_level),
};

#define MCA_LOG_SYSFS_ATTRS_SIZE  ARRAY_SIZE(mca_log_sysfs_field_tbl)

static struct attribute *mca_log_sysfs_attrs[MCA_LOG_SYSFS_ATTRS_SIZE + 1];

static const struct attribute_group mca_log_sysfs_attr_group = {
	.attrs = mca_log_sysfs_attrs,
};

static inline struct mca_charge_log_ops_data *mca_log_get_charge_log_ops(unsigned int type)
{
	return &g_mca_charge_log_data[type];
}

static int mca_log_charge_log_dump_log_head(char *buf)
{
	int i;
	int len = 0;
	struct mca_charge_log_ops_data *ops_data;

	for (i = 0; i < MCA_CHARGE_LOG_ID_MAX; i++) {
		ops_data = mca_log_get_charge_log_ops(i);
		if (!ops_data->ops || !ops_data->ops->dump_log_head)
			continue;

		len += ops_data->ops->dump_log_head(ops_data->data, buf + len, PAGE_SIZE - len);
	}

	if (len < PAGE_SIZE - 1)
		buf[len] = '\n';
	else
		buf[PAGE_SIZE - 1] = '\n';

	return len;
}

static int mca_log_charge_log_dump_log_context(char *buf)
{
	int i;
	int len = 0;
	struct mca_charge_log_ops_data *ops_data;

	for (i = 0; i < MCA_CHARGE_LOG_ID_MAX; i++) {
		ops_data = mca_log_get_charge_log_ops(i);
		if (!ops_data->ops || !ops_data->ops->dump_log_head)
			continue;

		len += ops_data->ops->dump_log_context(ops_data->data, buf + len, PAGE_SIZE - len);
	}

	if (len < PAGE_SIZE - 1)
		buf[len] = '\n';
	else
		buf[PAGE_SIZE - 1] = '\n';

	return len;
}

static ssize_t mca_log_sysfs_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mca_sysfs_attr_info *info = NULL;
	struct mca_log_buf_info *log_info = &g_mca_log_info;
	int value = 0;

	info = mca_sysfs_lookup_attr(attr->attr.name,
		mca_log_sysfs_field_tbl, MCA_LOG_SYSFS_ATTRS_SIZE);

	if (!info)
		return -EINVAL;

	(void)kstrtoint(buf, 0, &value);

	switch (info->sysfs_attr_name) {
	case MCA_LOG_ATTR_LOG_LEVEL:
		log_info->log_level = value;
		break;
	case MCA_LOG_ATTR_LOG_ENABLE:
		log_info->enable = value;
		log_info->max_cache_num = MCA_LOG_BUFF_DEFAULT_COUNT;
		break;
	case MCA_LOG_ATTR_LOG_INDEX:
		if (value > MCA_LOG_BUFF_COUNT || value < 0)
			return -1;
		log_info->index = value;
		break;
	case MCA_LOG_ATTR_CONSOLE_LOG_LEVEL:
		log_info->console_level = value;
		pr_err("console level is %d\n", log_info->console_level);
		break;
	default:
		break;
	}

	return count;
}

static ssize_t mca_log_sysfs_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mca_sysfs_attr_info *info = NULL;
	struct mca_log_buf_info *log_info = &g_mca_log_info;
	int len = 0;
	unsigned long flags;

	info = mca_sysfs_lookup_attr(attr->attr.name,
		mca_log_sysfs_field_tbl, MCA_LOG_SYSFS_ATTRS_SIZE);

	if (!info)
		return -EINVAL;

	switch (info->sysfs_attr_name) {
	case MCA_LOG_ATTR_LOG_LEVEL:
		len = snprintf(buf, PAGE_SIZE, "%d\n", log_info->log_level);
		break;
	case MCA_LOG_ATTR_LOG_ENABLE:
		len = snprintf(buf, PAGE_SIZE, "%d\n", log_info->enable);
		break;
	case MCA_LOG_ATTR_LOG_CUR_BUFF:
		len = snprintf(buf, PAGE_SIZE, "%d\n", log_info->lastest_cache);
		break;
	case MCA_LOG_ATTR_LOG_MAX_BUFF_NUM:
		len = snprintf(buf, PAGE_SIZE, "%d\n", log_info->max_cache_num);
		break;
	case MCA_LOG_ATTR_DUMP_BUFFER_LOG:
		spin_lock_irqsave(&g_mca_log_lock, flags);
		if (log_info->index == MCA_LOG_BUFF_COUNT) {
			memcpy(buf, log_info->log_buff, PAGE_SIZE - 1);
			len = strlen(buf);
			memset(log_info->log_buff, 0, PAGE_SIZE);
			log_info->count = 0;
			spin_unlock_irqrestore(&g_mca_log_lock, flags);
			return len;
		}
		if (!log_info->log_buff_cache[log_info->index]) {
			spin_unlock_irqrestore(&g_mca_log_lock, flags);
			return 0;
		}
		log_info->cache_num--;
		if (log_info->cache_num < 0)
			log_info->cache_num = 0;
		memcpy(buf, log_info->log_buff_cache[log_info->index], PAGE_SIZE - 1);
		len = strlen(buf);
		kfree(log_info->log_buff_cache[log_info->index]);
		log_info->log_buff_cache[log_info->index] = NULL;
		spin_unlock_irqrestore(&g_mca_log_lock, flags);
		break;
	case MCA_LOG_ATTR_DUMP_CHARGE_LOG_HEAD:
		len = mca_log_charge_log_dump_log_head(buf);
		break;
	case MCA_LOG_ATTR_DUMP_CHARGE_LOG_INFO:
		len = mca_log_charge_log_dump_log_context(buf);
		break;
	case MCA_LOG_ATTR_CONSOLE_LOG_LEVEL:
		len = snprintf(buf, PAGE_SIZE, "%d\n", log_info->console_level);
		break;
	default:
		break;
	}

	return len;
}

static int mca_log_sysfs_create_group(struct mca_log_buf_info *info)
{
	mca_sysfs_init_attrs(mca_log_sysfs_attrs, mca_log_sysfs_field_tbl,
		MCA_LOG_SYSFS_ATTRS_SIZE);

	info->dev = mca_sysfs_create_group("xm_power", "charge_log",
		&mca_log_sysfs_attr_group);

	return 0;
}

static void mca_log_sysfs_remove_group(struct mca_log_buf_info *info)
{
	mca_sysfs_remove_group("xm_power", info->dev, &mca_log_sysfs_attr_group);
}

#else
static int mca_log_sysfs_create_group(struct mca_log_buf_info *info)
{
	return 0;
}

static void mca_log_sysfs_remove_group(struct mca_log_buf_info *info)
{
}
#endif /* CONFIG_SYSFS */

static int __init mca_log_init(void)
{
	struct mca_log_buf_info *info = &g_mca_log_info;

	info->log_buff = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!info->log_buff)
		return -ENOMEM;

	if (mca_log_sysfs_create_group(&g_mca_log_info)) {
		pr_err("mca_log create group failed\n");
		kfree(info->log_buff);
		return -1;
	}

	info->max_cache_num = MCA_LOG_BUFF_COUNT;
	info->enable = 1;
	info->log_level = MCA_LOG_LEVEL_INFO;
	info->console_level = MCA_LOG_LEVEL_ERROR;
	info->init_flag = 1;
	pr_err("charge_boot_mode %d\n", charge_boot_mode);

	return 0;
}
module_init(mca_log_init);

static void __exit mca_log_exit(void)
{
	mca_log_sysfs_remove_group(&g_mca_log_info);
}
module_exit(mca_log_exit);


MODULE_DESCRIPTION("strategy buckchg class");
MODULE_AUTHOR("liyuze1@xiaomi.com");
MODULE_LICENSE("GPL v2");

