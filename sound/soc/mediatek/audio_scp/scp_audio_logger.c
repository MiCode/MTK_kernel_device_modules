// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/of.h>
#include <linux/io.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include "adsp_helper.h"
#include "scp_audio_ipi.h"
#include "scp_audio_logger.h"

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
#include "scp_helper.h"
#endif

#define LOGGER_DEBUG 0
#if (LOGGER_DEBUG == 1)
#define LOGGER_D(x...)		pr_debug(x)
#else
#define LOGGER_D(x...)
#endif

#define ROUNDUP(a, b)		(((a) + ((b)-1)) & ~((b)-1))
#define PLT_LOG_ENABLE		0x504C5402 /*magic*/
static unsigned int magic_warn_flag = 1;

static unsigned int logger_inited;
static struct mutex logger_lock;
static struct log_ctrl_s *logger_ctrl;
static struct buffer_info_s *buf_info;

static unsigned int logger_enable;
static unsigned int logger_buffer_size; // store size info in global rather than shared DRAM

static unsigned int w_pos_debug;
static unsigned int r_pos_debug;

static void dump_dram_ctrl_mem(void)
{
	unsigned int i, size;
	unsigned int *addr;
	uintptr_t address;

	if (!logger_ctrl) {
		pr_err("%s(), logger_ctrl is NULL!\n", __func__);
		return;
	}

	pr_info(" ===== log_ctrl_s =====\n");
	addr = (unsigned int *)logger_ctrl;
	address = (uintptr_t)addr;
	size = sizeof(struct log_ctrl_s) / sizeof(unsigned int);
	for (i = 0; i < size; i += 4)
		pr_info("0x%08x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
			((unsigned int)address)+i, addr[i], addr[i+1], addr[i+2], addr[i+3]);

	pr_info(" ===== buffer_info_s =====\n");
	addr = (unsigned int *)buf_info;
	address = (uintptr_t)addr;
	size = sizeof(struct buffer_info_s) / sizeof(unsigned int);
	for (i = 0; i < size; i += 4)
		pr_info("0x%08x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
			((unsigned int)address)+i, addr[i], addr[i+1], addr[i+2], addr[i+3]);
}

static ssize_t scp_audio_logger_read(struct file *file, char __user *data,
				     size_t len, loff_t *ppos)
{
	unsigned int w_pos, r_pos, datalen = 0;
	void *buf;

	LOGGER_D("[SCP AUDIO] %s()\n", __func__);

	if (!logger_inited)
		return 0;

	mutex_lock(&logger_lock);

	if (logger_ctrl->base != PLT_LOG_ENABLE) {
		pr_info_ratelimited("%s, logger magic 0x%x error\n", __func__, logger_ctrl->base);
		dump_dram_ctrl_mem();
		WARN_ON(magic_warn_flag);
		magic_warn_flag = 0; // warn once
		goto EXIT;
	}

	memcpy_fromio(&r_pos, &buf_info->r_pos, sizeof(r_pos));
	memcpy_fromio(&w_pos, &buf_info->w_pos, sizeof(w_pos));

	if (r_pos == w_pos)
		goto EXIT;
	else if (r_pos > w_pos)
		datalen = logger_buffer_size - r_pos;
	else
		datalen = w_pos - r_pos;

	if (datalen > len)
		datalen = len;

	r_pos_debug = r_pos;
	w_pos_debug = w_pos;
	if ((r_pos >= logger_buffer_size) || (w_pos >= logger_buffer_size)) {
		pr_err("[SCP AUDIO] %s() r_pos 0x%x or w_pos 0x%x >= buff_size 0x%x\n",
		       __func__, r_pos, w_pos, logger_buffer_size);
		datalen = 0;

		// reset r_pos and w_pos
		r_pos = 0;
		w_pos = 0;
		memcpy_toio(&buf_info->r_pos, &r_pos, sizeof(r_pos));
		memcpy_toio(&buf_info->w_pos, &w_pos, sizeof(w_pos));
		goto EXIT;
	}

	buf = ((char *)logger_ctrl) + logger_ctrl->buff_ofs + r_pos;
	if (copy_to_user(data, buf, datalen))
		pr_err("[SCP AUDIO] copy to user buf failed!\n");

	r_pos += datalen;
	if (r_pos >= logger_buffer_size)
		r_pos -= logger_buffer_size;
	memcpy_toio(&buf_info->r_pos, &r_pos, sizeof(r_pos));

EXIT:
	LOGGER_D("[SCP AUDIO] %s() r_pos 0x%x w_pos 0x%x datalen 0x%x\n",
		 __func__, r_pos, w_pos, datalen);
	mutex_unlock(&logger_lock);
	return datalen;
}

static int scp_audio_logger_open(struct inode *inode, struct file *file)
{
	LOGGER_D("[SCP AUDIO] %s()\n", __func__);
	return nonseekable_open(inode, file);
}

static unsigned int scp_audio_logger_poll(struct file *file,
					  struct poll_table_struct *poll)
{
	LOGGER_D("[SCP AUDIO] %s()\n", __func__);

	if (!logger_inited || !(file->f_mode & FMODE_READ))
		return 0;

	if (!logger_ctrl || !buf_info)
		return POLLERR;

	if (buf_info->r_pos >= logger_ctrl->buff_size ||
	    buf_info->w_pos >= logger_ctrl->buff_size)
		return POLLERR;

	if (buf_info->r_pos != buf_info->w_pos)
		return POLLIN | POLLRDNORM;

	return 0;
}

const struct file_operations scp_audio_logger_file_ops = {
	.owner = THIS_MODULE,
	.read = scp_audio_logger_read,
	.open = scp_audio_logger_open,
	.poll = scp_audio_logger_poll,
};

static struct miscdevice mdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "adsp_0", // reuse adsp_0 node
	.fops = &scp_audio_logger_file_ops,
};

static void scp_audio_logger_init_handler(int id, void *data, unsigned int len)
{
	LOGGER_D("[SCP AUDIO] %s()\n", __func__);
	logger_inited = 1;
}

void reset_adsp_logger_status(void)
{
	logger_inited = 0;
}

int scp_audio_logger_init_message(void)
{
	int ret;
	unsigned int val = 0;
	unsigned int retry_count = 10;

	LOGGER_D("[SCP AUDIO] %s() logger_inited %d\n", __func__, logger_inited);

	if (logger_inited)
		return 0;

	do {
		retry_count--;
		ret = scp_push_message(SCP_AUDIO_IPI_LOGGER_INIT,
				       &val, sizeof(val), 20, 0);
		if (ret != ADSP_IPI_DONE)
			usleep_range(1000, 1500);
	} while ((retry_count > 0) && (ret != ADSP_IPI_DONE));

	if (ret != ADSP_IPI_DONE)
		pr_err("[SCP AUDIO] %s() ret %d\n", __func__, ret);
	else
		pr_info("[SCP AUDIO] %s() success\n", __func__);

	return ret;
}

int scp_audio_logger_init(struct platform_device *pdev)
{

	int ret = -1;
	int last_ofs = 0;
	void *addr = NULL;
	uint64_t size = 0;

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)

	mutex_init(&logger_lock);

	/* setup scp audio logger */
	addr = adsp_get_reserve_mem_virt(ADSP_A_LOGGER_MEM_ID);
	size = adsp_get_reserve_mem_size(ADSP_A_LOGGER_MEM_ID);
	if (!addr || size == 0) {
		pr_err("%s, get reserved memory failed, addr 0x%llx size 0x%llx\n",
		       __func__, (uint64_t)addr, size);
		ret = -ENOMEM;
		goto ERROR;
	}

	logger_ctrl = (struct log_ctrl_s *)addr;
	logger_ctrl->base = PLT_LOG_ENABLE;
	logger_ctrl->enable = 0;
	logger_ctrl->size = sizeof(*logger_ctrl);

	last_ofs += ALIGN(logger_ctrl->size, 128);
	logger_ctrl->info_ofs = last_ofs;

	last_ofs += sizeof(*buf_info);
	last_ofs = ALIGN(last_ofs, 128);
	logger_ctrl->buff_ofs = last_ofs;
	logger_ctrl->buff_size = size - last_ofs;
	logger_buffer_size = logger_ctrl->buff_size;

	buf_info = (struct buffer_info_s *)
		(((unsigned char *) logger_ctrl) + logger_ctrl->info_ofs);
	buf_info->r_pos = 0;
	buf_info->w_pos = 0;

	last_ofs += logger_buffer_size;
	if (last_ofs > size) {
		pr_err("%s fail! last_ofs:0x%x, size:0x%llx\n", __func__, last_ofs, size);
		ret = -EINVAL;
		goto ERROR;
	}

	/* register logger IPI */
	scp_audio_ipi_registration(SCP_AUDIO_IPI_LOGGER_INIT,
				   scp_audio_logger_init_handler,
				   "logger init");

	if (is_scp_ready(SCP_A_ID))
		scp_audio_logger_init_message();

	ret = misc_register(&mdev);
	if (ret) {
		pr_err("%s, cannot register misc device\n", __func__);
		goto ERROR;
	}

	ret = device_create_file(mdev.this_device, &dev_attr_log_enable);
	if (ret) {
		pr_err("%s, cannot create dev_attr_log_enable\n", __func__);
		goto ERROR;
	}

	pr_info("%s, done ret:%d", __func__, ret);
	return ret;

#endif /* CONFIG_MTK_TINYSYS_SCP_SUPPORT */

ERROR:
	logger_inited = 0;
	logger_ctrl = NULL;
	buf_info = NULL;
	pr_err("%s, failed ret:%d", __func__, ret);
	return ret;
}

static inline ssize_t log_enable_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	unsigned int n = 0;
	unsigned int status;

	status = logger_inited && logger_enable;
	n = snprintf(buf, 128, "[SCP AUDIO] mobile log is %s, logger_inited %d logger_enable %d\n",
		     (status == 0x1) ? "enabled" : "disabled", logger_inited, logger_enable);
	if (n > 128)
		n = 128;
	return n;
}

static inline ssize_t log_enable_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	int ret = -1;
	unsigned int enable = 0;
	unsigned int retry_count = 10;

	if (!logger_inited)
		return -EINVAL;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	mutex_lock(&logger_lock);
	do {
		retry_count--;
		ret = scp_push_message(SCP_AUDIO_IPI_LOGGER_ENABLE,
				       &enable, sizeof(enable), 20, 0);
		if (ret != ADSP_IPI_DONE)
			usleep_range(1000, 1500);
	} while ((retry_count > 0) && (ret != ADSP_IPI_DONE));

	if (ret == ADSP_IPI_DONE) {
		logger_enable = enable;
		logger_ctrl->enable = enable;
		pr_info("%s, [SCP AUDIO] enable = %d\n", __func__, enable);
	} else
		pr_err("%s scp_push_message failed, ret = %d\n", __func__, ret);

	mutex_unlock(&logger_lock);

	return count;
}
DEVICE_ATTR_RW(log_enable);
