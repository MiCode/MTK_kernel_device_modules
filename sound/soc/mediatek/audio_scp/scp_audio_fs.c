// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */


#include <linux/poll.h>
#include <linux/delay.h>
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
#define AUDIO_DSP_IOCTL_ADSP_HRT_BW \
	_IOR(AUDIO_DSP_IOC_MAGIC, 3, unsigned int)

static unsigned int dbg_inited;

union ioctl_param {
	struct {
		int16_t flag;
		uint16_t cid;
	} cmd1;
	struct {
		uint16_t set;
		uint16_t scene;
	} cmd2;
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

		if (copy_to_user((void __user *)arg, &t, sizeof(t))) {
			ret = -EFAULT;
			break;
		}
		break;
	}
	case AUDIO_DSP_IOCTL_ADSP_HRT_BW: {
		if (copy_from_user(&t, (void *)arg, sizeof(t))) {
			ret = -EFAULT;
			break;
		}

		ret = adsp_icc_bw_req(t.cmd2.scene, t.cmd2.set);
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

static void scp_audio_dbg_init_handler(int id, void *data, unsigned int len)
{
	dbg_inited = 1;
}

static int scp_audio_debug_cmds_init_message(void)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
	unsigned int retry_count = 10;
	uint64_t mem_info[2] = {0};

	/*just init once*/
	if (dbg_inited)
		return 0;

	/* setup scp audio debug cmds */
	mem_info[0] = adsp_get_reserve_mem_phys(ADSP_A_DEBUG_DUMP_MEM_ID);
	mem_info[1] = adsp_get_reserve_mem_size(ADSP_A_DEBUG_DUMP_MEM_ID);
	if (!mem_info[0] || mem_info[1] == 0) {
		pr_err("%s, get reserved memory failed, addr 0x%llx size 0x%llx\n",
		       __func__, mem_info[0], mem_info[1]);
		ret = -ENOMEM;
		return ret;
	}

	do {
		ret = scp_send_message_with_wakelock(SCP_AUDIO_IPI_DBG_INIT,
			mem_info, sizeof(mem_info), 0, 0);
		if (ret != ADSP_IPI_DONE)
			usleep_range(1000, 1500);
	} while ((retry_count > 0) && (ret != ADSP_IPI_DONE));

	if (ret != ADSP_IPI_DONE)
		pr_err("[SCP AUDIO] dbg inif failed %s() ret %d\n", __func__, ret);
	else
		pr_info("mem_info[0] = 0x%llx, mem_info[1] = 0x%llx, ret = %d\n", mem_info[0], mem_info[1], ret);
#endif
	return ret;
}

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
/* SCP reboot */
static int audio_dbg_ctrl_event_receive_scp(
	struct notifier_block *this,
	unsigned long event,
	void *ptr)
{
	switch (event) {
	case SCP_EVENT_STOP:
		dbg_inited = 0;
		break;
	case SCP_EVENT_READY:
		scp_audio_debug_cmds_init_message();
		break;
	default:
		pr_info("event %lu err", event);
	}
	pr_info("%s() event %lu", __func__, event);
	return 0;
}

static struct notifier_block audio_dbg_ctrl_notifier_scp = {
	.notifier_call = audio_dbg_ctrl_event_receive_scp,
};
#endif

int scp_audio_debug_cmds_init(void)
{
	int ret = 0;
	/* register dbg IPI */
	scp_audio_ipi_registration(SCP_AUDIO_IPI_DBG_INIT,
				   scp_audio_dbg_init_handler,
				   "dbg init");

	scp_A_register_notify(&audio_dbg_ctrl_notifier_scp);

	if (is_scp_ready(SCP_A_ID))
		ret = scp_audio_debug_cmds_init_message();

	pr_info("%s, done ret:%d", __func__, ret);
	return ret;
}

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
	buf[sizeof(buf) - 1] = '\0';

	ret = scp_send_message_with_wakelock(SCP_AUDIO_IPI_DBG_CMDS,
			       buf, strnlen(buf, sizeof(buf) - 1) + 1, 0, 0);
	pr_info("%s() send cmd: %s, ret: %d\n", __func__, buf, ret);

	return count;
}

const struct file_operations scp_audio_debug_ops = {
	.open = simple_open,
	.read = scp_audio_debug_read,
	.write = scp_audio_debug_write,
};
