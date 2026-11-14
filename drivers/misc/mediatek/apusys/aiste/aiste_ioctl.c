// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include "aiste_ioctl.h"
#include "aiste_qos.h"
#include "aiste_debug.h"

#define MAX_BOOST_VALUE 100

static long aiste_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	switch (cmd) {
	case AISTE_IOCTL_CREATE_QOS: {
		struct aiste_ioctl_id id;

		id.qos_id = aiste_create_qos();
		ret = (id.qos_id == 0) ? -ENOMEM : 0;
		if (ret == 0 && copy_to_user((void *)arg, &id, sizeof(id)))
			ret = -EINVAL;
		break;
	}
	case AISTE_IOCTL_REQUEST_QOS: {
		struct aiste_ioctl_request request;
		struct qos_entry *entry = NULL;

		memset(&request, 0, sizeof(request));
		if (copy_from_user(&request, (void *)arg, sizeof(request))) {
			ret = -EINVAL;
			break;
		}

		entry = validate_and_get_qos_entry(request.qos_id);
		if (!entry)
			return -EINVAL;

		if (request.qos.ddr_boost_value > MAX_BOOST_VALUE) {
			aiste_err("%s: invalid DDR boost value: %d.\n", __func__, request.qos.ddr_boost_value);
			ret = -EINVAL;
			break;
		}
		if (request.qos.cpu_boost_value > MAX_BOOST_VALUE) {
			aiste_err("%s: invalid CPU boost value: %d.\n", __func__, request.qos.cpu_boost_value);
			ret = -EINVAL;
			break;
		}
		if (request.qos.cpu_boost_value > 0 && (request.qos.thread_count == 0)) {
			aiste_err("%s: set CPU boost without threads.\n", __func__);
			ret = -EINVAL;
			break;
		}
		if (request.qos.thread_count > MAX_THREAD_NUM_PER_QOS) {
			aiste_err("%s: thread count exceeds the limit: %d.\n", __func__, request.qos.thread_count);
			ret = -EINVAL;
			break;
		}

		ret = aiste_request_qos(
				entry,
				request.qos.ddr_boost_value,
				request.qos.cpu_boost_value,
				request.qos.thread_count,
				request.qos.thread_set);
		break;
	}
	case AISTE_IOCTL_DELETE_QOS: {
		struct aiste_ioctl_id id;
		struct qos_entry *entry = NULL;

		if (copy_from_user(&id, (void *)arg, sizeof(id))) {
			ret = -EINVAL;
			break;
		}
		entry = validate_and_get_qos_entry(id.qos_id);
		if (!entry)
			return -EINVAL;
		ret = aiste_delete_qos(entry);
		break;
	}
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int aiste_open(struct inode *inodep, struct file *filep)
{
	aiste_drv_debug("%s(): Device has been opened\n", __func__);
	return 0;
}

static int aiste_release(struct inode *inodep, struct file *filep)
{
	aiste_drv_debug("%s(): Device successfully closed\n", __func__);
	return 0;
}

static const struct file_operations aiste_fops = {
	.owner = THIS_MODULE,
	.open = aiste_open,
	.unlocked_ioctl = aiste_ioctl,
	.compat_ioctl = aiste_ioctl,
	.release = aiste_release,
};

static struct miscdevice aiste_misc_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = AISTE_NAME,
	.fops = &aiste_fops,
};

struct miscdevice *aiste_get_misc_dev(void)
{
	return &aiste_misc_dev;
}
