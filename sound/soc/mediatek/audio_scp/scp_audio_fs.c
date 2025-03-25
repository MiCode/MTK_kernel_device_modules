// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include "adsp_helper.h"
#include "scp_audio_ipi.h"

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
#include "scp.h"
#endif

/*==============================================================================
 *                     ioctl
 *==============================================================================
 */
#define AUDIO_DSP_IOC_MAGIC 'a'
#define AUDIO_DSP_IOCTL_ADSP_QUERY_STATUS \
	_IOR(AUDIO_DSP_IOC_MAGIC, 1, unsigned int)

union ioctl_param {
	struct {
		int16_t flag;
		uint16_t cid;
	} cmd1;
};

/* file operations */
static long adspscp_driver_ioctl(
	struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	union ioctl_param t;

	switch (cmd) {
	case AUDIO_DSP_IOCTL_ADSP_QUERY_STATUS: {
		if (copy_from_user(&t, (void *)arg, sizeof(t))) {
			ret = -EFAULT;
			break;
		}

		t.cmd1.flag = is_scp_ready(SCP_A_ID);
		pr_info("%s(), AUDIO_DSP_IOCTL_ADSP_QUERY_STATUS: %d\n", __func__, t.cmd1.flag);

		if (copy_to_user((void __user *)arg, &t, sizeof(t))) {
			ret = -EFAULT;
			break;
		}
		break;
	}
	default:
		pr_debug("%s(), invalid ioctl cmd\n", __func__);
	}

	if (ret < 0)
		pr_info("%s(), ioctl error %d\n", __func__, ret);

	return ret;
}

static long adspscp_driver_compat_ioctl(
	struct file *file, unsigned int cmd, unsigned long arg)
{
	if (!file->f_op || !file->f_op->unlocked_ioctl) {
		pr_notice("op null\n");
		return -ENOTTY;
	}
	return file->f_op->unlocked_ioctl(file, cmd, arg);
}

const struct file_operations adspscp_file_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.unlocked_ioctl = adspscp_driver_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl   = adspscp_driver_compat_ioctl,
#endif
};

struct miscdevice scp_audio_fs_mdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "adsp_in_scp",
	.fops = &adspscp_file_ops,
};


/*==============================================================================
 *                     debugfs
 *==============================================================================
 */
static ssize_t scp_audio_debug_read(struct file *filp, char __user *buf,
				    size_t count, loff_t *pos)
{
	char *buffer = NULL;
	size_t n = 0, max_size;

	buffer = adsp_get_reserve_mem_virt(ADSP_A_DEBUG_DUMP_MEM_ID);
	max_size = adsp_get_reserve_mem_size(ADSP_A_DEBUG_DUMP_MEM_ID);

	if (buffer)
		n = strnlen(buffer, max_size);

	return simple_read_from_buffer(buf, count, pos, buffer, n);
}

static ssize_t scp_audio_debug_write(struct file *filp, const char __user *buffer,
				     size_t count, loff_t *ppos)
{
	int ret = 0;
	char buf[64] = {0};

	if (copy_from_user(buf, buffer, min(count, sizeof(buf) - 1)))
		return -EFAULT;

	ret = scp_send_message(SCP_AUDIO_DBG_CMDS,
			       buf, strnlen(buf, sizeof(buf) - 1) + 1, 0, 0);
	pr_info("%s() send cmd: %s, ret: %d\n", __func__, buf, ret);

	return count;
}

const struct file_operations scp_audio_debug_ops = {
	.open = simple_open,
	.read = scp_audio_debug_read,
	.write = scp_audio_debug_write,
};
