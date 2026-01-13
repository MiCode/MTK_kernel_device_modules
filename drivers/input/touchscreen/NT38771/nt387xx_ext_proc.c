/*
 * Copyright (C) 2024 Novatek, Inc.
 *
 * $Revision: 67976 $
 * $Date: 2020-08-27 16:49:50 +0800 (週四, 27 八月 2020) $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */


#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "nt387xx.h"

#if NVT_TOUCH_EXT_PROC
#define NVT_FW_VERSION "nvt_fw_version"
/* P16 code for HQFEAT-94217 by liaoxianguo at 2025/3/24 start */
#define NVT_IRQ "nvt_irq"
/* P16 code for HQFEAT-94217 by liaoxianguo at 2025/3/24 end */
/*P16 code for HQFEAT-89652 by liuyupei at 2025/4/8 start*/
#define NVT_TP_INFO "tp_info"
/*P16 code for HQFEAT-89652 by liuyupei at 2025/4/8 end*/
#define NVT_BASELINE "nvt_baseline"
#define NVT_RAW "nvt_raw"
#define NVT_DIFF "nvt_diff"
#define NVT_FW_DEBUG_INFO_SWITCH "nvt_fw_debug_info_switch"
/*P16 code for HQFEAT-89693 by xiongdejun at 2025/3/24 start*/
#define PROC_LOCKDOWN_INFO_FILE "tp_lockdown_info"
/*P16 code for HQFEAT-89693 by xiongdejun at 2025/3/24 end*/
/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 start*/
#define NVT_POCKET_PALM_SWITCH "nvt_pocket_palm_switch"
/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 end*/
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 start*/
#define NVT_GESTURE_SWITCH "nvt_gesture_switch"
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 end*/
/*P16 code for HQFEAT-88864 by xiongdejun at 2025/4/2 start*/
#define NVT_EDGE_REJECT_SWITCH "nvt_edge_reject_switch"
/*P16 code for HQFEAT-88864 by xiongdejun at 2025/4/2 end*/
/* P16 code for HQFEAT-90108 by liuyupei at 2025/4/1 start */
#define NVT_CHARGER_SWITCH "nvt_charger_switch"
/* P16 code for HQFEAT-90108 by liuyupei at 2025/4/1 end */
/*P16 bug fot BUGP16-11893 by xiongdejun at 2025/8/25 start*/
#define NONTHP_TOUCH_NODE "nonthp_touch_node"
extern char debug_info_buf[DEBUG_MAX_BUFFER_SIZE];
extern int is_full;
extern int nvt_read_pos;
extern int nvt_write_pos;
/*P16 bug fot BUGP16-11893 by xiongdejun at 2025/8/25 end*/
/*P16 code for HQFEAT-89698 by xiongdejun at 2025/4/23 start*/
#define NVT_TP_SELFTEST "tp_selftest"
#define SELF_TEST_INVAL 0
#define SELF_TEST_NG 1
#define SELF_TEST_OK 2
/*P16 code for HQFEAT-89698 by xiongdejun at 2025/4/23 end*/
/* P16 code for HQFEAT-89782 by p-zhangyundan at 2025/4/30 start */
#define TP_DATA_DUMP "tp_data_dump"
/* P16 code for HQFEAT-89782 by p-zhangyundan at 2025/4/30 end */
#define NORMAL_MODE 0x00
#define TEST_MODE_2 0x22
#define HANDSHAKING_HOST_READY 0xBB

#define XDATA_SECTOR_SIZE   256

static uint8_t xdata_tmp[5000] = {0};
static int32_t xdata[2500] = {0};

static struct proc_dir_entry *NVT_proc_fw_version_entry;
/* P16 code for HQFEAT-94217 by liaoxianguo at 2025/3/24 start */
static struct proc_dir_entry *NVT_proc_irq_entry;
/* P16 code for HQFEAT-94217 by liaoxianguo at 2025/3/24 end */
static struct proc_dir_entry *NVT_proc_baseline_entry;
static struct proc_dir_entry *NVT_proc_raw_entry;
static struct proc_dir_entry *NVT_proc_diff_entry;
static struct proc_dir_entry *NVT_proc_fw_debug_info_switch_entry;
/*P16 code for HQFEAT-89693 by xiongdejun at 2025/3/24 start*/
static struct proc_dir_entry *proc_tp_lockdown_info_entry = NULL;
/*P16 code for HQFEAT-89693 by xiongdejun at 2025/3/24 end*/

/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 start*/
static struct proc_dir_entry *NVT_proc_pocket_palm_switch_entry;
/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 end*/

/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 start*/
static struct proc_dir_entry *NVT_proc_gesture_switch_entry;
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 end*/
/*P16 code for HQFEAT-88864 by xiongdejun at 2025/4/2 start*/
static struct proc_dir_entry *NVT_proc_edge_reject_switch_entry;
extern uint8_t edge_orientation_store;
/*P16 code for HQFEAT-88864 by xiongdejun at 2025/4/2 end*/
/*P16 code for HQFEAT-89652 by liuyupei at 2025/4/8 start*/
static struct proc_dir_entry *NVT_proc_tp_info_entry;
/*P16 code for HQFEAT-89652 by liuyupei at 2025/4/8 end*/
/* P16 code for HQFEAT-90108 by liuyupei at 2025/4/1 start */
static struct proc_dir_entry *NVT_proc_charger_switch_entry;
/* P16 code for HQFEAT-90108 by liuyupei at 2025/4/1 end */
/*P16 code for HQFEAT-89698 by xiongdejun at 2025/4/23 start*/
static struct proc_dir_entry *nvt_proc_tp_selftest_entry = NULL;
/* P16 code for HQFEAT-89782 by p-zhangyundan at 2025/4/30 start */
static struct proc_dir_entry *tp_data_dump_entry =NULL;
/* P16 code for HQFEAT-89782 by p-zhangyundan at 2025/4/30 end */
/*P16 bug fot BUGP16-11893 by xiongdejun at 2025/8/25 start*/
static struct proc_dir_entry *NVT_debug_info_entry;
/*P16 bug fot BUGP16-11893 by xiongdejun at 2025/8/25 end*/
extern int nvt_factory_short_test(void);
extern int nvt_factory_open_test(void);
static int aftersale_selftest = 0;
/*P16 code for HQFEAT-89698 by xiongdejun at 2025/4/23 end*/
/*******************************************************
Description:
	Novatek touchscreen change mode function.

return:
	n.a.
*******************************************************/
void nvt_change_mode(uint8_t mode)
{
	uint8_t buf[8] = {0};

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---set mode---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = mode;
	CTP_SPI_WRITE(ts->client, buf, 2);

	if (mode == NORMAL_MODE) {
		usleep_range(20000, 20000);
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = HANDSHAKING_HOST_READY;
		CTP_SPI_WRITE(ts->client, buf, 2);
		usleep_range(20000, 20000);
	}
}

/*******************************************************
Description:
	Novatek touchscreen get firmware pipe function.

return:
	Executive outcomes. 0---pipe 0. 1---pipe 1.
*******************************************************/
uint8_t nvt_get_fw_pipe(void)
{
	uint8_t buf[8]= {0};

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

	//---read fw status---
	buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
	buf[1] = 0x00;
	CTP_SPI_READ(ts->client, buf, 2);

	//NVT_LOG("FW pipe=%d, buf[1]=0x%02X\n", (buf[1]&0x01), buf[1]);

	return (buf[1] & 0x01);
}

/*******************************************************
Description:
	Novatek touchscreen read meta data function.

return:
	n.a.
*******************************************************/
void nvt_read_mdata(uint32_t xdata_addr)
{
	int32_t transfer_len = 0;
	int32_t i = 0;
	int32_t j = 0;
	int32_t k = 0;
	uint8_t buf[BUS_TRANSFER_LENGTH + 2] = {0};
	uint32_t head_addr = 0;
	int32_t dummy_len = 0;
	int32_t data_len = 0;
	int32_t residual_len = 0;

	if (BUS_TRANSFER_LENGTH <= XDATA_SECTOR_SIZE)
		transfer_len = BUS_TRANSFER_LENGTH;
	else
		transfer_len = XDATA_SECTOR_SIZE;

	//---set xdata sector address & length---
	head_addr = xdata_addr - (xdata_addr % XDATA_SECTOR_SIZE);
	dummy_len = xdata_addr - head_addr;
	data_len = ts->x_num * ts->y_num * 2;
	residual_len = (head_addr + dummy_len + data_len) % XDATA_SECTOR_SIZE;

	//printk("head_addr=0x%05X, dummy_len=0x%05X, data_len=0x%05X, residual_len=0x%05X\n", head_addr, dummy_len, data_len, residual_len);

	//read xdata : step 1
	for (i = 0; i < ((dummy_len + data_len) / XDATA_SECTOR_SIZE); i++) {
		//---change xdata index---
		nvt_set_page(head_addr + XDATA_SECTOR_SIZE * i);

		//---read xdata by transfer_len
		for (j = 0; j < (XDATA_SECTOR_SIZE / transfer_len); j++) {
			//---read data---
			buf[0] = transfer_len * j;
			CTP_SPI_READ(ts->client, buf, transfer_len + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < transfer_len; k++) {
				xdata_tmp[XDATA_SECTOR_SIZE * i + transfer_len * j + k] = buf[k + 1];
				//printk("0x%02X, 0x%04X\n", buf[k+1], (XDATA_SECTOR_SIZE*i + transfer_len*j + k));
			}
		}
		//printk("addr=0x%05X\n", (head_addr+XDATA_SECTOR_SIZE*i));
	}

	//read xdata : step2
	if (residual_len != 0) {
		//---change xdata index---
		nvt_set_page(xdata_addr + data_len - residual_len);

		//---read xdata by transfer_len
		for (j = 0; j < (residual_len / transfer_len + 1); j++) {
			//---read data---
			buf[0] = transfer_len * j;
			CTP_SPI_READ(ts->client, buf, transfer_len + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < transfer_len; k++) {
				xdata_tmp[(dummy_len + data_len - residual_len) + transfer_len * j + k] = buf[k + 1];
				//printk("0x%02X, 0x%04x\n", buf[k+1], ((dummy_len+data_len-residual_len) + transfer_len*j + k));
			}
		}
		//printk("addr=0x%05X\n", (xdata_addr+data_len-residual_len));
	}

	//---remove dummy data and 2bytes-to-1data---
	for (i = 0; i < (data_len / 2); i++) {
		xdata[i] = (int16_t)(xdata_tmp[dummy_len + i * 2] + 256 * xdata_tmp[dummy_len + i * 2 + 1]);
	}

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);
}
/* P16 code for HQFEAT-90108 by liuyupei at 2025/4/1 start */
int32_t nvt_set_charger_switch(uint8_t charger_switch)
{
	int32_t ret = 0;
	uint8_t buf[8] = {0};
	NVT_LOG("++\n");
	NVT_LOG("charger_switch: %d\n", charger_switch);
	if (charger_switch == 0) {
		// charger disable
		buf[0] = 0x51;
	} else if (charger_switch == 1) {
		// charger enable
		buf[0] = 0x53;
	} else {
		NVT_ERR("Invalid value! charger_switch = %d\n", charger_switch);
		ret = -EINVAL;
		goto out;
	}
	ret = nvt_set_custom_cmd(buf, 1);
	if (ret < 0) {
		NVT_ERR("nvt_set_custom_cmd fail! ret=%d\n", ret);
		goto out;
	}
out:
	NVT_LOG("--\n");
	return ret;
}
static ssize_t nvt_charger_switch_proc_write(struct file *filp,const char __user *buf, size_t count, loff_t *f_pos)
{
	int32_t ret;
	int32_t tmp;
	uint8_t charger_switch;
	char *tmp_buf;
	NVT_LOG("++\n");
	if (count == 0 || count > 2) {
		NVT_ERR("Invalid value!, count = %zu\n", count);
		ret = -EINVAL;
		goto out;
	}
	tmp_buf = kzalloc(count + 1, GFP_KERNEL);
	if (!tmp_buf) {
		NVT_ERR("Allocate tmp_buf fail!\n");
		ret = -ENOMEM;
		goto out;
	}
	if (copy_from_user(tmp_buf, buf, count)) {
		NVT_ERR("copy_from_user() error!\n");
		ret = -EFAULT;
		goto out;
	}
	ret = sscanf(tmp_buf, "%d", &tmp);
	if (ret != 1) {
		NVT_ERR("Invalid value!, ret = %d\n", ret);
		ret = -EINVAL;
		goto out;
	}
	charger_switch = (uint8_t)tmp;
	NVT_LOG("charger_switch = %d\n", charger_switch);
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}
#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */
	nvt_set_charger_switch(charger_switch);
	mutex_unlock(&ts->lock);
	ret = count;
out:
	kfree(tmp_buf);
	NVT_LOG("--\n");
	return ret;
}
#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_charger_switch_fops = {
	.proc_write = nvt_charger_switch_proc_write,
};
#else
static const struct file_operations nvt_charger_switch_fops = {
	.owner = THIS_MODULE,
	.write = nvt_charger_switch_proc_write,
};
#endif
/* P16 code for HQFEAT-90108 by liuyupei at 2025/4/1 end */
/*******************************************************
Description:
    Novatek touchscreen get meta data function.

return:
    n.a.
*******************************************************/
void nvt_get_mdata(int32_t *buf, uint8_t *m_x_num, uint8_t *m_y_num)
{
    *m_x_num = ts->x_num;
    *m_y_num = ts->y_num;
    memcpy(buf, xdata, ((ts->x_num * ts->y_num) * sizeof(int32_t)));
}

/*******************************************************
Description:
	Novatek touchscreen read and get number of meta data function.

return:
	n.a.
*******************************************************/
void nvt_read_get_num_mdata(uint32_t xdata_addr, int32_t *buffer, uint32_t num)
{
	int32_t transfer_len = 0;
	int32_t i = 0;
	int32_t j = 0;
	int32_t k = 0;
	uint8_t buf[BUS_TRANSFER_LENGTH + 2] = {0};
	uint32_t head_addr = 0;
	int32_t dummy_len = 0;
	int32_t data_len = 0;
	int32_t residual_len = 0;

	if (BUS_TRANSFER_LENGTH <= XDATA_SECTOR_SIZE)
		transfer_len = BUS_TRANSFER_LENGTH;
	else
		transfer_len = XDATA_SECTOR_SIZE;

	//---set xdata sector address & length---
	head_addr = xdata_addr - (xdata_addr % XDATA_SECTOR_SIZE);
	dummy_len = xdata_addr - head_addr;
	data_len = num * 2;
	residual_len = (head_addr + dummy_len + data_len) % XDATA_SECTOR_SIZE;

	//printk("head_addr=0x%05X, dummy_len=0x%05X, data_len=0x%05X, residual_len=0x%05X\n", head_addr, dummy_len, data_len, residual_len);

	//read xdata : step 1
	for (i = 0; i < ((dummy_len + data_len) / XDATA_SECTOR_SIZE); i++) {
		//---change xdata index---
		nvt_set_page(head_addr + XDATA_SECTOR_SIZE * i);

		//---read xdata by transfer_len
		for (j = 0; j < (XDATA_SECTOR_SIZE / transfer_len); j++) {
			//---read data---
			buf[0] = transfer_len * j;
			CTP_SPI_READ(ts->client, buf, transfer_len + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < transfer_len; k++) {
				xdata_tmp[XDATA_SECTOR_SIZE * i + transfer_len * j + k] = buf[k + 1];
				//printk("0x%02X, 0x%04X\n", buf[k+1], (XDATA_SECTOR_SIZE*i + transfer_len*j + k));
			}
		}
		//printk("addr=0x%05X\n", (head_addr+XDATA_SECTOR_SIZE*i));
	}

	//read xdata : step2
	if (residual_len != 0) {
		//---change xdata index---
		nvt_set_page(xdata_addr + data_len - residual_len);

		//---read xdata by transfer_len
		for (j = 0; j < (residual_len / transfer_len + 1); j++) {
			//---read data---
			buf[0] = transfer_len * j;
			CTP_SPI_READ(ts->client, buf, transfer_len + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < transfer_len; k++) {
				xdata_tmp[(dummy_len + data_len - residual_len) + transfer_len * j + k] = buf[k + 1];
				//printk("0x%02X, 0x%04x\n", buf[k+1], ((dummy_len+data_len-residual_len) + transfer_len*j + k));
			}
		}
		//printk("addr=0x%05X\n", (xdata_addr+data_len-residual_len));
	}

	//---remove dummy data and 2bytes-to-1data---
	for (i = 0; i < (data_len / 2); i++) {
		buffer[i] = (int16_t)(xdata_tmp[dummy_len + i * 2] + 256 * xdata_tmp[dummy_len + i * 2 + 1]);
	}

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);
}

/*******************************************************
Description:
	Novatek touchscreen firmware version show function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t c_fw_version_show(struct seq_file *m, void *v)
{
	seq_printf(m, "fw_ver=%d, x_num=%d, y_num=%d\n", ts->fw_ver, ts->x_num, ts->y_num);
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen xdata sequence print show
	function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t c_show(struct seq_file *m, void *v)
{
	int32_t i = 0;
	int32_t j = 0;

	for (i = 0; i < ts->y_num; i++) {
		for (j = 0; j < ts->x_num; j++) {
			seq_printf(m, "%6d,", xdata[i * ts->x_num + j]);
		}
		seq_puts(m, "\n");
	}

	seq_printf(m, "\n\n");
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen xdata sequence print start
	function.

return:
	Executive outcomes. 1---call next function.
	NULL---not call next function and sequence loop
	stop.
*******************************************************/
static void *c_start(struct seq_file *m, loff_t *pos)
{
	return *pos < 1 ? (void *)1 : NULL;
}

/*******************************************************
Description:
	Novatek touchscreen xdata sequence print next
	function.

return:
	Executive outcomes. NULL---no next and call sequence
	stop function.
*******************************************************/
static void *c_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return NULL;
}

/*******************************************************
Description:
	Novatek touchscreen xdata sequence print stop
	function.

return:
	n.a.
*******************************************************/
static void c_stop(struct seq_file *m, void *v)
{
	return;
}

const struct seq_operations nvt_fw_version_seq_ops = {
	.start  = c_start,
	.next   = c_next,
	.stop   = c_stop,
	.show   = c_fw_version_show
};

const struct seq_operations nvt_seq_ops = {
	.start  = c_start,
	.next   = c_next,
	.stop   = c_stop,
	.show   = c_show
};

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_fw_version open
	function.

return:
	n.a.
*******************************************************/
static int32_t nvt_fw_version_open(struct inode *inode, struct file *file)
{
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	NVT_LOG("++\n");

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	return seq_open(file, &nvt_fw_version_seq_ops);
}

#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_fw_version_fops = {
	.proc_open = nvt_fw_version_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
#else
static const struct file_operations nvt_fw_version_fops = {
	.owner = THIS_MODULE,
	.open = nvt_fw_version_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif

/* P16 code for HQFEAT-94217 by liaoxianguo at 2025/3/24 start */
static ssize_t nvt_irq_write(struct file *file, const char __user *buffer,size_t count, loff_t *data)
{
	char temp_buff[128] = { 0 };
	uint32_t pdata =1;
	if(!buffer){
		return -EINVAL;
	}
	if (copy_from_user(temp_buff, buffer, count - 1)) {
		NVT_ERR("copy from user error!\n");
		return -EFAULT;
	}
	if (sscanf(temp_buff, "%d", &pdata)) {
		if (pdata == 1) {
			nvt_irq_enable(true);
		} else {
			nvt_irq_enable(false);
		}
	}
	return count;
}
#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_irq_fops = {
	.proc_write = nvt_irq_write,
};
#else
static const struct file_operations nvt_irq_fops = {
	.owner = THIS_MODULE,
	.write = nvt_irq_write,
};
#endif
/* P16 code for HQFEAT-94217 by liaoxianguo at 2025/3/24 end */
/*P16 code for HQFEAT-89652 by liuyupei at 2025/4/8 start*/
static int32_t nvt_tp_info_show(struct seq_file *m, void *v)
{
	nvt_get_fw_info();
	/*P16 code for HQFEAT-102768 by p-zhaobeidou3 at 2025/4/16 start*/
	if (g_lcm_panel_id == TOUCH_SELECT_0A_CSOT) {
		seq_printf(m, "[Vendor]:Huaxing CSOT [TP-IC]:NT38771 [FW]:0x%02x\n", tp_fw_version);
	} else if (g_lcm_panel_id == TOUCH_SELECT_0B_TIANMA) {
		seq_printf(m, "[Vendor]:TianMa [TP-IC]:NT38771 [FW]:0x%02x\n", tp_fw_version);
	} else if (g_lcm_panel_id == TOUCH_SELECT_0C_VISIONOX) {
		seq_printf(m, "[Vendor]:Visionox [TP-IC]:NT38771 [FW]:0x%02x\n", tp_fw_version);
	} else {
		NVT_ERR("TP is not CORRECT!");
	}
	/*P16 code for HQFEAT-102768 by p-zhaobeidou3 at 2025/4/16 end*/
	return 0;
}
static int32_t nvt_tp_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, nvt_tp_info_show, NULL);
}
static const struct proc_ops nvt_tp_info_fops = {
	.proc_open = nvt_tp_info_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
/*P16 code for HQFEAT-89652 by liuyupei at 2025/4/8 end*/
/* P16 code for HQFEAT-89782 by p-zhangyundan at 2025/4/30 start */
static int32_t tp_data_dump_show(struct seq_file *m, void *v)
{
	int32_t i = 0;
	int32_t j = 0;

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */
	/*-------diff data-----*/
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}
	seq_printf(m, "diffdata\n");
	nvt_change_mode(TEST_MODE_2);
	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->DIFF_PIPE0_ADDR);
	else
		nvt_read_mdata(ts->mmap->DIFF_PIPE1_ADDR);
	nvt_change_mode(NORMAL_MODE);
	mutex_unlock(&ts->lock);
	for (i = 0; i < ts->y_num; i++) {
		for (j = 0; j < ts->x_num; j++) {
			seq_printf(m, "%5d, ", xdata[i * ts->x_num + j]);
		}
		seq_puts(m, "\n");
	}
	memset(xdata, 0, sizeof(xdata));
	/*-------raw data--------*/
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}
	seq_printf(m, "\nrawdata\n");
	nvt_change_mode(TEST_MODE_2);
	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->RAW_PIPE0_ADDR);
	else
		nvt_read_mdata(ts->mmap->RAW_PIPE1_ADDR);
	nvt_change_mode(NORMAL_MODE);
	mutex_unlock(&ts->lock);
	for (i = 0; i < ts->y_num; i++) {
		for (j = 0; j < ts->x_num; j++) {
			seq_printf(m, "%5d, ", xdata[i * ts->x_num + j]);
		}
		seq_puts(m, "\n");
	}
	return 0;
}
static int32_t tp_data_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, tp_data_dump_show, NULL);
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops tp_data_dump_fops = {
	.proc_open  = tp_data_dump_open,
	.proc_read  = seq_read,
};
#else
static const struct file_operations tp_data_dump_fops = {
	.open 	 = tp_data_dump_open,
	.read 	 = seq_read,
};
#endif
/* P16 code for HQFEAT-89782 by p-zhangyundan at 2025/4/30 end */
/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_baseline open function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nvt_baseline_open(struct inode *inode, struct file *file)
{
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	NVT_LOG("++\n");

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	if (nvt_clear_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_change_mode(TEST_MODE_2);

	if (nvt_check_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_read_mdata(ts->mmap->BASELINE_ADDR);

	nvt_change_mode(NORMAL_MODE);

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	return seq_open(file, &nvt_seq_ops);
}

#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_baseline_fops = {
	.proc_open = nvt_baseline_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
#else
static const struct file_operations nvt_baseline_fops = {
	.owner = THIS_MODULE,
	.open = nvt_baseline_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_raw open function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nvt_raw_open(struct inode *inode, struct file *file)
{
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	NVT_LOG("++\n");

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	if (nvt_clear_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_change_mode(TEST_MODE_2);

	if (nvt_check_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->RAW_PIPE0_ADDR);
	else
		nvt_read_mdata(ts->mmap->RAW_PIPE1_ADDR);

	nvt_change_mode(NORMAL_MODE);

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	return seq_open(file, &nvt_seq_ops);
}

#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_raw_fops = {
	.proc_open = nvt_raw_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
#else
static const struct file_operations nvt_raw_fops = {
	.owner = THIS_MODULE,
	.open = nvt_raw_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_diff open function.

return:
	Executive outcomes. 0---succeed. negative---failed.
*******************************************************/
static int32_t nvt_diff_open(struct inode *inode, struct file *file)
{
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	NVT_LOG("++\n");

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	if (nvt_clear_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_change_mode(TEST_MODE_2);

	if (nvt_check_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->DIFF_PIPE0_ADDR);
	else
		nvt_read_mdata(ts->mmap->DIFF_PIPE1_ADDR);

	nvt_change_mode(NORMAL_MODE);

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	return seq_open(file, &nvt_seq_ops);
}

#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_diff_fops = {
	.proc_open = nvt_diff_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
#else
static const struct file_operations nvt_diff_fops = {
	.owner = THIS_MODULE,
	.open = nvt_diff_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif
/*P16 code for BUGP16-8418 by xiongdejun at 2025/7/15 start*/
static ssize_t nvt_fw_debug_info_switch_proc_write(struct file *filp,const char __user *buf, size_t count, loff_t *f_pos)
{
	int32_t ret;
	int32_t tmp;
	char *tmp_buf;

	NVT_LOG("++\n");

	if (count == 0 || count > 2) {
		NVT_ERR("Invalid value!, count = %zu\n", count);
		ret = -EINVAL;
		goto out;
	}

	tmp_buf = kzalloc(count + 1, GFP_KERNEL);
	if (!tmp_buf) {
		NVT_ERR("Allocate tmp_buf fail!\n");
		ret = -ENOMEM;
		goto out;
	}
	if (copy_from_user(tmp_buf, buf, count)) {
		NVT_ERR("copy_from_user() error!\n");
		ret = -EFAULT;
		goto out;
	}

	ret = sscanf(tmp_buf, "%d", &tmp);
	if (ret != 1) {
		NVT_ERR("Invalid value!, ret = %d\n", ret);
		ret = -EINVAL;
		goto out;
	}
	ts->fw_debug_info_switch = !!tmp;
/*P16 bug fot BUGP16-11893 by xiongdejun at 2025/8/25 start*/
	if(ts->fw_debug_info_switch == 0){
		memset(debug_info_buf, 0, DEBUG_MAX_BUFFER_SIZE);
	}
/*P16 bug fot BUGP16-11893 by xiongdejun at 2025/8/25 end*/
	NVT_LOG("ts->fw_debug_info_switch = %d\n", ts->fw_debug_info_switch);

	ret = count;
out:
	kfree(tmp_buf);
	NVT_LOG("--\n");
	return ret;
}

static ssize_t nvt_fw_debug_info_switch_proc_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	NVT_LOG("++\n");
	char temp_buf[20] = {0};
  	int retval = 0;

	retval = ts->fw_debug_info_switch;
	snprintf(temp_buf, 20, "%d\n", retval);
	NVT_LOG("--\n");
	return simple_read_from_buffer(buf, count, f_pos, temp_buf, strlen(temp_buf));
}

#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_fw_debug_info_switch_fops = {
	.proc_read = nvt_fw_debug_info_switch_proc_read,
	.proc_write = nvt_fw_debug_info_switch_proc_write,
};
#else
static const struct file_operations nvt_fw_debug_info_switch_fops = {
	.owner = THIS_MODULE,
	.read = nvt_fw_debug_info_switch_proc_read,
	.write = nvt_fw_debug_info_switch_proc_write,
};
#endif
/*P16 code for BUGP16-8418 by xiongdejun at 2025/7/15 end*/
/*P16 code for HQFEAT-89693 by xiongdejun at 2025/3/24 start*/
static int tp_lockdown_info_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "%s\n", ts->lockdowninfo);
    return 0;
}
static int tp_lockdown_info_open(struct inode *inode, struct file *file)
{
    return single_open(file, tp_lockdown_info_proc_show, NULL);
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops proc_tp_lockdown_info_fops = {
    .proc_open = tp_lockdown_info_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = seq_release,
};
#else
static const struct file_operations proc_tp_lockdown_info_fops = {
    .open  = tp_lockdown_info_open,
    .read  = seq_read,
    .llseek = seq_lseek,
    .release = seq_release,
};
#endif
/*P16 code for HQFEAT-89693 by xiongdejun at 2025/3/24 end*/

/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 start*/
int32_t nvt_set_pocket_palm_switch(uint8_t pocket_palm_switch)
{
	int32_t ret = 0;
	uint8_t buf[8] = {0};

	NVT_LOG("++\n");
	NVT_LOG("pocket_palm_switch: %d\n", pocket_palm_switch);
	if (pocket_palm_switch == 0) {
		// pocket palm disable
		buf[0] = 0x74;
	} else if (pocket_palm_switch == 1) {
		// pocket palm enable
		buf[0] = 0x73;
	} else {
		NVT_ERR("Invalid value! pocket_palm_switch = %d\n", pocket_palm_switch);
		ret = -EINVAL;
			goto out;
	}
	ret = nvt_set_custom_cmd(buf, 1);
	if (ret < 0) {
		NVT_ERR("nvt_set_custom_cmd fail! ret=%d\n", ret);
		goto out;
	}

out:
	NVT_LOG("--\n");
	return ret;
}

static ssize_t nvt_pocket_palm_switch_proc_write(struct file *filp,const char __user *buf, size_t count, loff_t *f_pos)
{
	int32_t ret;
	int32_t tmp;
	uint8_t pocket_palm_switch;
	char *tmp_buf;

	NVT_LOG("++\n");

	if (count == 0 || count > 2) {
		NVT_ERR("Invalid value!, count = %zu\n", count);
		ret = -EINVAL;
		goto out;
	}

	tmp_buf = kzalloc(count + 1, GFP_KERNEL);
	if (!tmp_buf) {
		NVT_ERR("Allocate tmp_buf fail!\n");
		ret = -ENOMEM;
		goto out;
	}
	if (copy_from_user(tmp_buf, buf, count)) {
		NVT_ERR("copy_from_user() error!\n");
		ret = -EFAULT;
		goto out;
	}

	ret = sscanf(tmp_buf, "%d", &tmp);
	if (ret != 1) {
		NVT_ERR("Invalid value!, ret = %d\n", ret);
		ret = -EINVAL;
		goto out;
	}
	pocket_palm_switch = (uint8_t)tmp;
	NVT_LOG("pocket_palm_switch = %d\n", pocket_palm_switch);

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	nvt_set_pocket_palm_switch(pocket_palm_switch);

	mutex_unlock(&ts->lock);

	ret = count;
out:
	kfree(tmp_buf);
	NVT_LOG("--\n");
	return ret;
}

#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_pocket_palm_switch_fops = {
	.proc_write = nvt_pocket_palm_switch_proc_write,
};
#else
static const struct file_operations nvt_pocket_palm_switch_fops = {
	.owner = THIS_MODULE,
	.write = nvt_pocket_palm_switch_proc_write,
};
#endif
/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 end*/

int32_t nvt_set_gesture_switch(uint8_t gesture_switch)
{
	int32_t ret = 0;
	uint8_t buf[8] = {0};

	NVT_LOG("++\n");

	NVT_LOG("set gesture_switch: 0x%02X\n", gesture_switch);
	buf[0] = 0x7F;
	buf[1] = 0x01;
	buf[2] = gesture_switch;
	ret = nvt_set_custom_cmd(buf, 3);
	if (ret < 0) {
		NVT_ERR("nvt_set_custom_cmd fail! ret=%d\n", ret);
		goto out;
	}

out:
	NVT_LOG("--\n");
	return ret;
}

/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 start*/
static ssize_t nvt_gesture_switch_proc_write(struct file *filp,const char __user *buf, size_t count, loff_t *f_pos)
{
	int32_t ret;
	int32_t tmp;
	uint8_t gesture_switch;
	char *tmp_buf = NULL;

	NVT_LOG("++\n");

	if (count == 0 || count > 2) {
		NVT_ERR("Invalid value! count = %zu\n", count);
		ret = -EINVAL;
		goto out;
	}

	tmp_buf = kzalloc(count + 1, GFP_KERNEL);
	if (!tmp_buf) {
		NVT_ERR("Allocate tmp_buf fail!\n");
		ret = -ENOMEM;
		goto out;
	}
	if (copy_from_user(tmp_buf, buf, count)) {
		NVT_ERR("copy_from_user() error!\n");
		ret = -EFAULT;
		goto out;
	}

	ret = sscanf(tmp_buf, "%d", &tmp);
	if (ret != 1) {
		NVT_ERR("Invalid value! ret = %d\n", ret);
		ret = -EINVAL;
		goto out;
	}
	gesture_switch = (uint8_t)tmp;
	NVT_LOG("gesture_switch = %d\n", gesture_switch);
	nvt_set_gesture_switch(gesture_switch);

	ret = count;
out:
	if (tmp_buf)
		kfree(tmp_buf);
	NVT_LOG("--\n");
	return ret;
}

#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_gesture_switch_fops = {
    .proc_write = nvt_gesture_switch_proc_write,
};
#else
static const struct file_operations nvt_gesture_switch_fops = {
	.owner = THIS_MODULE,
	.write = nvt_gesture_switch_proc_write,
};
#endif
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 end*/
/*P16 code for HQFEAT-88864 by xiongdejun at 2025/4/2 start*/
int32_t nvt_set_edge_reject_switch(uint8_t edge_reject_switch)
{
	int32_t ret = 0;
	uint8_t buf[8] = {0};
	NVT_LOG("++\n");
	NVT_LOG("edge_reject_switch: %d\n", edge_reject_switch);
	edge_orientation_store = edge_reject_switch;
	if (edge_reject_switch == 1) {
		// vertical
		buf[0] = 0xBA;
	} else if (edge_reject_switch == 2) {
		// left up
		buf[0] = 0xBB;
	} else if (edge_reject_switch == 3) {
		// righ up
		buf[0] = 0xBC;
	} else {
		NVT_ERR("Invalid value! edge_reject_switch = %d\n", edge_reject_switch);
		ret = -EINVAL;
		goto out;
	}
	ret = nvt_set_custom_cmd(buf, 1);
	if (ret < 0) {
		NVT_ERR("nvt_set_custom_cmd fail! ret=%d\n", ret);
		goto out;
	}
out:
	NVT_LOG("--\n");
	return ret;
}

static ssize_t nvt_edge_reject_switch_proc_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	int32_t ret;
	int32_t tmp;
	uint8_t edge_reject_switch;
	char *tmp_buf = 0;
	NVT_LOG("++\n");
	if (count == 0 || count > 2) {
		NVT_ERR("Invalid value! count = %zu\n", count);
		ret = -EINVAL;
		goto out;
	}
	tmp_buf = kzalloc((count+1), GFP_KERNEL);
	if (!tmp_buf) {
		NVT_ERR("Allocate tmp_buf fail!\n");
		ret = -ENOMEM;
		goto out;
	}
	if (copy_from_user(tmp_buf, buf, count)) {
		NVT_ERR("copy_from_user() error!\n");
		ret =  -EFAULT;
		goto out;
	}
	ret = sscanf(tmp_buf, "%d", &tmp);
	if (ret != 1) {
		NVT_ERR("Invalid value! ret = %d\n", ret);
		ret = -EINVAL;
		goto out;
	}
	if (tmp < 1 || tmp > 4) {
		NVT_ERR("Invalid value! tmp = %d\n", tmp);
		ret = -EINVAL;
		goto out;
	}
	edge_reject_switch = (uint8_t)tmp;
	NVT_LOG("edge_reject_switch = %d\n", edge_reject_switch);
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}
#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */
	nvt_set_edge_reject_switch(edge_reject_switch);
	mutex_unlock(&ts->lock);
	ret = count;
out:
	if (tmp_buf)
		kfree(tmp_buf);
	NVT_LOG("--\n");
	return ret;
}
#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_edge_reject_switch_fops = {
	.proc_write = nvt_edge_reject_switch_proc_write,
};
#else
static const struct file_operations nvt_edge_reject_switch_fops = {
	.owner = THIS_MODULE,
	.write = nvt_edge_reject_switch_proc_write,
};
#endif
/*P16 code for HQFEAT-88864 by xiongdejun at 2025/4/2 end*/
/*P16 code for HQFEAT-89698 by xiongdejun at 2025/4/23 start*/
static int32_t nvt_spi_check(void)
{
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}
	NVT_LOG("++\n");
#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */
	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}
	mutex_unlock(&ts->lock);
	NVT_LOG("--\n");
	return 0;
}
static ssize_t aftersale_selftest_read(struct file *file, char __user *buf,
			size_t count, loff_t *pos)
{
	int retval = 0;
	char temp_buf[256] = {0};
	if (aftersale_selftest == 1) {
		if (nvt_factory_short_test() < 0) {
			/*P16 code for HQFEAT-162581 by liuyupei at 2025/7/23 start*/
			NVT_LOG("short test fail,retry once!");
			if (nvt_factory_short_test() < 0) {
				retval = SELF_TEST_NG;
			} else {
				retval = SELF_TEST_OK;
			}
			/*P16 code for HQFEAT-162581 by liuyupei at 2025/7/23 end*/
		} else {
			retval = SELF_TEST_OK;
		}
	}else if(aftersale_selftest == 2){
		if (nvt_factory_open_test() < 0) {
			/*P16 code for HQFEAT-162581 by liuyupei at 2025/7/23 start*/
			NVT_LOG("open test fail,retry once!");
			if (nvt_factory_open_test() < 0) {
				retval = SELF_TEST_NG;
			} else {
				retval = SELF_TEST_OK;
			}
			/*P16 code for HQFEAT-162581 by liuyupei at 2025/7/23 end*/
		} else {
			retval = SELF_TEST_OK;
		}
	}else if (aftersale_selftest == 3) {
		retval = nvt_spi_check();
		NVT_LOG("RETVAL is %d\n", retval);
		if (!retval)
			retval = SELF_TEST_OK;
		else
			retval = SELF_TEST_NG;
	}else{
		retval = SELF_TEST_INVAL;
	}
	snprintf(temp_buf, 256, "%d\n", retval);
	return simple_read_from_buffer(buf, count, pos, temp_buf, strlen(temp_buf));
}
static ssize_t aftersale_selftest_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos)
{
	char temp_buf[20] = {0};
	if (copy_from_user(temp_buf, buf, count)) {
		return count;
	}
	if(!strncmp("short", temp_buf, 5)){
		aftersale_selftest = 1;
	} else if (!strncmp("spi", temp_buf, 3)) {
		aftersale_selftest = 3;
	}else if(!strncmp("open", temp_buf, 4)){
		aftersale_selftest = 2;
	}else {
		aftersale_selftest = 0;
		NVT_LOG("tp_selftest echo incorrect\n");
	}
	return count;
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops aftersale_test_ops = {
	.proc_read = aftersale_selftest_read,
	.proc_write = aftersale_selftest_write,
};
#else
static const struct file_operations aftersale_test_ops = {
	.read = aftersale_selftest_read,
	.write = aftersale_selftest_write,
};
#endif
/*P16 code for HQFEAT-89698 by xiongdejun at 2025/4/23 end*/
/*P16 bug fot BUGP16-11893 by xiongdejun at 2025/8/25 start*/
static int32_t nvt_debug_info_show(struct seq_file *m, void *v)
{
	if(is_full){
		seq_write(m, debug_info_buf + nvt_read_pos ,DEBUG_MAX_BUFFER_SIZE - nvt_read_pos);
		seq_write(m, debug_info_buf ,nvt_write_pos);
	}else{
		seq_write(m,debug_info_buf + nvt_read_pos ,nvt_write_pos - nvt_read_pos);
	}
	return 0;
}
static int32_t nvt_debug_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, nvt_debug_info_show, NULL);
}
static const struct proc_ops nvt_debug_info_fops = {
	.proc_open = nvt_debug_info_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
/*P16 bug fot BUGP16-11893 by xiongdejun at 2025/8/25 end*/
/*******************************************************
Description:
	Novatek touchscreen extra function proc. file node
	initial function.

return:
	Executive outcomes. 0---succeed. -12---failed.
*******************************************************/
int32_t nvt_extra_proc_init(void)
{
	NVT_proc_fw_version_entry = proc_create(NVT_FW_VERSION, 0444, NULL,&nvt_fw_version_fops);
	if (NVT_proc_fw_version_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_FW_VERSION);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_FW_VERSION);
	}

/* P16 code for HQFEAT-94217 by liaoxianguo at 2025/3/24 start */
	NVT_proc_irq_entry = proc_create(NVT_IRQ, 0222, NULL,&nvt_irq_fops);
	if (NVT_proc_irq_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_IRQ);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_IRQ);
	}
/* P16 code for HQFEAT-94217 by liaoxianguo at 2025/3/24 end */

	NVT_proc_baseline_entry = proc_create(NVT_BASELINE, 0444, NULL,&nvt_baseline_fops);
	if (NVT_proc_baseline_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_BASELINE);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_BASELINE);
	}

	NVT_proc_raw_entry = proc_create(NVT_RAW, 0444, NULL,&nvt_raw_fops);
	if (NVT_proc_raw_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_RAW);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_RAW);
	}

	NVT_proc_diff_entry = proc_create(NVT_DIFF, 0444, NULL,&nvt_diff_fops);
	if (NVT_proc_diff_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_DIFF);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_DIFF);
	}
/*P16 code for BUGP16-8418 by xiongdejun at 2025/7/15 start*/
	NVT_proc_fw_debug_info_switch_entry = proc_create(NVT_FW_DEBUG_INFO_SWITCH, 0666, NULL, &nvt_fw_debug_info_switch_fops);
	if (NVT_proc_fw_debug_info_switch_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_FW_DEBUG_INFO_SWITCH);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_FW_DEBUG_INFO_SWITCH);
	}
/*P16 code for BUGP16-8418 by xiongdejun at 2025/7/15 end*/
/*P16 code for HQFEAT-89693 by xiongdejun at 2025/3/24 start*/
	proc_tp_lockdown_info_entry = proc_create(PROC_LOCKDOWN_INFO_FILE, 0444, NULL, &proc_tp_lockdown_info_fops);
	if (proc_tp_lockdown_info_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", PROC_LOCKDOWN_INFO_FILE);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", PROC_LOCKDOWN_INFO_FILE);
	}
/*P16 code for HQFEAT-89693 by xiongdejun at 2025/3/24 end*/
/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 start*/
NVT_proc_pocket_palm_switch_entry = proc_create(NVT_POCKET_PALM_SWITCH, 0666, NULL,&nvt_pocket_palm_switch_fops);
	if (NVT_proc_pocket_palm_switch_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_POCKET_PALM_SWITCH);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_POCKET_PALM_SWITCH);
	}
/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 end*/
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 start*/
	NVT_proc_gesture_switch_entry = proc_create(NVT_GESTURE_SWITCH, 0666, NULL, &nvt_gesture_switch_fops);
	if (NVT_proc_gesture_switch_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_GESTURE_SWITCH);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_GESTURE_SWITCH);
	}
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 end*/
/*P16 code for HQFEAT-88864 by xiongdejun at 2025/4/2 start*/
	NVT_proc_edge_reject_switch_entry = proc_create(NVT_EDGE_REJECT_SWITCH, 0666, NULL, &nvt_edge_reject_switch_fops);
	if (NVT_proc_edge_reject_switch_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_EDGE_REJECT_SWITCH);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_EDGE_REJECT_SWITCH);
	}
/*P16 code for HQFEAT-88864 by xiongdejun at 2025/4/2 end*/
/*P16 code for HQFEAT-89652 by liuyupei at 2025/4/8 start*/
	NVT_proc_tp_info_entry = proc_create(NVT_TP_INFO, 0444, NULL, &nvt_tp_info_fops);
	if (NVT_proc_tp_info_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_TP_INFO);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_TP_INFO);
	}
/*P16 code for HQFEAT-89652 by liuyupei at 2025/4/8 end*/
/* P16 code for HQFEAT-90108 by liuyupei at 2025/4/1 start */
	NVT_proc_charger_switch_entry = proc_create(NVT_CHARGER_SWITCH, 0666, NULL, &nvt_charger_switch_fops);
	if (NVT_proc_charger_switch_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_CHARGER_SWITCH);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_CHARGER_SWITCH);
	}
/* P16 code for HQFEAT-90108 by liuyupei at 2025/4/1 end */
/*P16 code for HQFEAT-89698 by xiongdejun at 2025/4/23 start*/
	nvt_proc_tp_selftest_entry = proc_create(NVT_TP_SELFTEST, (S_IWUSR | S_IRUGO), NULL, &aftersale_test_ops);
	if (nvt_proc_tp_selftest_entry == NULL){
		NVT_ERR("proc/%s create failed!\n", NVT_TP_SELFTEST);
	} else {
		NVT_LOG("proc/%s create success!\n",NVT_TP_SELFTEST);
	}
/*P16 code for HQFEAT-89698 by xiongdejun at 2025/4/23 end*/
/* P16 code for HQFEAT-89782 by p-zhangyundan at 2025/4/30 start */
	tp_data_dump_entry = proc_create(TP_DATA_DUMP, (S_IWUSR | S_IRUGO), NULL, &tp_data_dump_fops);
	if (tp_data_dump_entry == NULL) {
		printk(KERN_ERR "create proc/%s Failed!\n", TP_DATA_DUMP);
	} else {
		printk("create proc/%s Succeeded!\n", TP_DATA_DUMP);
	}
/* P16 code for HQFEAT-89782 by p-zhangyundan at 2025/4/30 end */
/*P16 bug fot BUGP16-11893 by xiongdejun at 2025/8/25 start*/
	NVT_debug_info_entry = proc_create(NONTHP_TOUCH_NODE, 0666, NULL, &nvt_debug_info_fops);
	if (NVT_debug_info_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NONTHP_TOUCH_NODE);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NONTHP_TOUCH_NODE);
	}
/*P16 bug fot BUGP16-11893 by xiongdejun at 2025/8/25 end*/
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen extra function proc. file node
	deinitial function.

return:
	n.a.
*******************************************************/
void nvt_extra_proc_deinit(void)
{
	if (NVT_proc_fw_version_entry != NULL) {
		remove_proc_entry(NVT_FW_VERSION, NULL);
		NVT_proc_fw_version_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_FW_VERSION);
	}

/* P16 code for HQFEAT-94217 by liaoxianguo at 2025/3/24 start */
	if (NVT_proc_irq_entry != NULL) {
		remove_proc_entry(NVT_IRQ, NULL);
		NVT_proc_irq_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_IRQ);
	}
/* P16 code for HQFEAT-94217 by liaoxianguo at 2025/3/24 end */

	if (NVT_proc_baseline_entry != NULL) {
		remove_proc_entry(NVT_BASELINE, NULL);
		NVT_proc_baseline_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_BASELINE);
	}

	if (NVT_proc_raw_entry != NULL) {
		remove_proc_entry(NVT_RAW, NULL);
		NVT_proc_raw_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_RAW);
	}

	if (NVT_proc_diff_entry != NULL) {
		remove_proc_entry(NVT_DIFF, NULL);
		NVT_proc_diff_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_DIFF);
	}
/*P16 code for BUGP16-8418 by xiongdejun at 2025/7/15 start*/
	if (NVT_proc_fw_debug_info_switch_entry != NULL) {
		remove_proc_entry(NVT_FW_DEBUG_INFO_SWITCH, NULL);
		NVT_proc_fw_debug_info_switch_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_FW_DEBUG_INFO_SWITCH);
	}
/*P16 code for BUGP16-8418 by xiongdejun at 2025/7/15 end*/
/*P16 code for HQFEAT-89693 by xiongdejun at 2025/3/24 start*/
	if (proc_tp_lockdown_info_entry != NULL) {
		remove_proc_entry(PROC_LOCKDOWN_INFO_FILE, NULL);
		proc_tp_lockdown_info_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", PROC_LOCKDOWN_INFO_FILE);
	}
/*P16 code for HQFEAT-89693 by xiongdejun at 2025/3/24 end*/
/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 start*/
	if (NVT_proc_pocket_palm_switch_entry != NULL) {
		remove_proc_entry(NVT_POCKET_PALM_SWITCH, NULL);
		NVT_proc_pocket_palm_switch_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_POCKET_PALM_SWITCH);
	}
/*P16 code for HQFEAT-89815 by liaoxianguo at 2025/4/1 end*/
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 start*/
	if (NVT_proc_gesture_switch_entry != NULL) {
		remove_proc_entry(NVT_GESTURE_SWITCH, NULL);
		NVT_proc_gesture_switch_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_GESTURE_SWITCH);
	}
/*P16 code for HQFEAT-94432 by liaoxianguo at 2025/3/27 end*/
/*P16 code for HQFEAT-88864 by xiongdejun at 2025/4/2 start*/
	if (NVT_proc_edge_reject_switch_entry != NULL) {
		remove_proc_entry(NVT_EDGE_REJECT_SWITCH, NULL);
		NVT_proc_edge_reject_switch_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_EDGE_REJECT_SWITCH);
	}
/*P16 code for HQFEAT-88864 by xiongdejun at 2025/4/2 end*/
/*P16 code for HQFEAT-89652 by liuyupei at 2025/4/8 start*/
	if (NVT_proc_tp_info_entry != NULL) {
		remove_proc_entry(NVT_TP_INFO, NULL);
		NVT_proc_tp_info_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_TP_INFO);
	}
/*P16 code for HQFEAT-89652 by liuyupei at 2025/4/8 end*/
/* P16 code for HQFEAT-90108 by liuyupei at 2025/4/1 start */
	if (NVT_proc_charger_switch_entry != NULL) {
		remove_proc_entry(NVT_CHARGER_SWITCH, NULL);
		NVT_proc_charger_switch_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_CHARGER_SWITCH);
	}
/* P16 code for HQFEAT-90108 by liuyupei at 2025/4/1 end */
/*P16 code for HQFEAT-89698 by xiongdejun at 2025/4/23 start*/
	if (nvt_proc_tp_selftest_entry != NULL) {
		remove_proc_entry(NVT_TP_SELFTEST, NULL);
		nvt_proc_tp_selftest_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_TP_SELFTEST);
	}
/*P16 code for HQFEAT-89698 by xiongdejun at 2025/4/23 end*/
/* P16 code for HQFEAT-89782 by p-zhangyundan at 2025/4/30 start */
	if (tp_data_dump_entry != NULL) {
		remove_proc_entry(TP_DATA_DUMP, NULL);
		tp_data_dump_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", TP_DATA_DUMP);
	}
/* P16 code for HQFEAT-89782 by p-zhangyundan at 2025/4/30 end */
/*P16 bug fot BUGP16-11893 by xiongdejun at 2025/8/25 start*/
	if (NVT_debug_info_entry != NULL) {
		remove_proc_entry(NONTHP_TOUCH_NODE, NULL);
		NVT_debug_info_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NONTHP_TOUCH_NODE);
	}
/*P16 bug fot BUGP16-11893 by xiongdejun at 2025/8/25 end*/
}
#endif
