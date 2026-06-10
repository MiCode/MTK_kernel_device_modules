/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __PELT_HINT_USEDEXT_H__
#define __PELT_HINT_USEDEXT_H__

#define CHI_SYSFS_READ(name, show, variable); \
static ssize_t name##_show(struct kobject *kobj, \
		struct kobj_attribute *attr, \
		char *buf) \
{ \
	if ((show)) \
		return scnprintf(buf, PAGE_SIZE, "%d\n", (variable)); \
	else \
		return 0; \
}

#define CHI_SYSFS_WRITE_VALUE(name, variable, min, max); \
static ssize_t name##_store(struct kobject *kobj, \
		struct kobj_attribute *attr, \
		const char *buf, size_t count) \
{ \
	char *acBuffer = NULL; \
	int arg; \
\
	acBuffer = kcalloc(PELT_HINT_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL); \
	if (!acBuffer) \
		goto out; \
\
	if ((count > 0) && (count < PELT_HINT_SYSFS_MAX_BUFF_SIZE)) { \
		if (scnprintf(acBuffer, PELT_HINT_SYSFS_MAX_BUFF_SIZE, "%s", buf)) { \
			if (kstrtoint(acBuffer, 0, &arg) == 0) { \
				if (arg >= (min) && arg <= (max)) \
					(variable) = arg; \
			} \
		} \
	} \
\
out: \
	kfree(acBuffer); \
	return count; \
}

struct pelt_hint_oem_task_data {
	int gai_task_flag;
};

struct gai_thread_info {
	int pid;
	struct hlist_node hlist;
};

#endif
