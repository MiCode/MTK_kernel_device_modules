/*
  * Goodix Touchscreen Driver
  * Copyright (C) 2020 - 2021 Goodix, Inc.
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation; either version 2 of the License, or
  * (at your option) any later version.
  *
  * This program is distributed in the hope that it will be a reference
  * to you, when you are integrating the GOODiX's CTP IC into your system,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  * General Public License for more details.
  *
  */
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
/*O6 code for HQ-391834 by liaoxianguo at 2023/06/24 start */
#include <linux/hqsysfs.h>
/*O6 code for HQ-391834 by liaoxianguo at 2023/06/24 end*/
/*O6 code for HQ-391123 by liaoxianguo at 2024/07/10 start*/
#include <linux/power_supply.h>
/*O6 code for HQ-391123 by liaoxianguo at 2024/07/10 end*/

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 38)
#include <linux/input/mt.h>
#define INPUT_TYPE_B_PROTOCOL
#endif

#include "goodix_ts_core.h"

/* O6 code for HQ-390162 by liuyupei at 2024/6/28 start */
#if IS_ENABLED(CONFIG_MI_DISP_NOTIFIER)
#include "mi_disp/mi_disp_notifier.h"
#endif
/* O6 code for HQ-390162 by liuyupei at 2024/6/28 end */

/* goodix fb test */
// #include "../../../video/fbdev/core/fb_firefly.h"

#define GOODIX_DEFAULT_CFG_NAME		"goodix_cfg_group.cfg"


/*N6 code for HQ-337468 by zhangzhijian5 at 2023/10/28 start*/
#define AOD_OPEN_STATUS				1
#define AOD_CLOSE_STATUS			0
/*N6 code for HQ-337468 by zhangzhijian5 at 2023/10/28 end*/

/*O6 code for HQ-392899 by liaoxianguo at 2024/07/11 start*/
#define CORNER_ZONE_TYPE 		0
#define EDGE_ZONE_TYPE			1
#define DEAD_ZONE_TYPE     		2
#define GTP_PARAMETER_NUM		8
#define EDGE_DATA_LENGTH		130
static int cornerzone = 0;
/*O6 code for HQ-392899 by liaoxianguo at 2024/07/11 end*/

/*O6 code for HQ-391996 by liuyupei at 2024/7/4 start*/
char *TS_DEFAULT_FIRMWARE;
char *TS_DEFAULT_CFG_BIN;
#define DISP_ID0_DET				(512+50)
#define DISP_ID1_DET				(512+52)
int tp_compatible_flag = 0;
/*O6 code for HQ-391996 by liuyupei at 2024/7/4 end*/
/*O6 code for HQ-391834 by liaoxianguo at 2023/06/24 start */
static char gt_hw_info[128] = " ";
/*O6 code for HQ-391834 by liaoxianguo at 2023/06/24 end */
struct goodix_module goodix_modules;
int core_module_prob_sate = CORE_MODULE_UNPROBED;
/*N6 code for HQ-305074 by dingying at 2023/09/27 start*/
struct goodix_ts_core *goodix_core_data;
/*N6 code for HQ-305074 by dingying at 2023/09/27 end*/

/*N6 code for HQ-340651 by huangshiquan at 2023/10/29 start*/
static bool goodix_ts_suspend_state = false;
/*N6 code for HQ-340651 by huangshiquan at 2023/10/29 end*/

/*O6 code for HQ-391123 by liaoxianguo at 2024/07/10 start*/
static int goodix_get_charging_status(void);
/*O6 code for HQ-391123 by liaoxianguo at 2024/07/10 end*/
/*N6 code for HQ-334519 by liaoxianguo at 2023/10/17 start*/
static void goodix_set_gesture_work(struct work_struct *work);
/*N6 code for HQ-334519 by liaoxianguo at 2023/10/17 end*/
static int goodix_send_ic_config(struct goodix_ts_core *cd, int type);
/*O6 code for HQ-391834 by liaoxianguo at 2023/06/24 start */
static int goodix_ts_hw_info(struct goodix_ts_core *core_data);
/*O6 code for HQ-391834 by liaoxianguo at 2023/06/24 end */
/*N6 code for HQ-346992 by zhangzhijian5 at 2023/12/04 start*/
static void goodix_sleep_to_gesture(struct goodix_ts_core *cd);
static void goodix_set_sleep_gesture_work(struct work_struct *work);
/*N6 code for HQ-346992 by zhangzhijian5 at 2023/12/04 end*/
/**
 * __do_register_ext_module - register external module
 * to register into touch core modules structure
 * return 0 on success, otherwise return < 0
 */
/*N6 code for HQ-346992 by zhangzhijian5 at 2023/12/04 start*/
static void goodix_sleep_to_gesture(struct goodix_ts_core *cd)
{
	int ret = 0;
	struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;

	ts_info("goodix_sleep_to_gesture in");
	ts_info("ic is in sleep mode, need to recover gesture mode");
	hw_ops->reset(cd, GOODIX_NORMAL_RESET_DELAY_MS);
	ret = hw_ops->gesture(cd, 0);
	if (ret)
		ts_err("failed enter gesture mode");
	else
		ts_info("enter gesture mode successfully");

	hw_ops->irq_enable(cd, true);
	enable_irq_wake(cd->irq);
	ts_info("goodix_sleep_to_gesture out");
}
/*N6 code for HQ-346992 by zhangzhijian5 at 2023/12/04 end*/

static int __do_register_ext_module(struct goodix_ext_module *module)
{
	struct goodix_ext_module *ext_module, *next;
	struct list_head *insert_point = &goodix_modules.head;

	/* prority level *must* be set */
	if (module->priority == EXTMOD_PRIO_RESERVED) {
		ts_err("Priority of module [%s] needs to be set",
		       module->name);
		return -EINVAL;
	}
	mutex_lock(&goodix_modules.mutex);
	/* find insert point for the specified priority */
	if (!list_empty(&goodix_modules.head)) {
		list_for_each_entry_safe(ext_module, next,
					 &goodix_modules.head, list) {
			if (ext_module == module) {
				ts_info("Module [%s] already exists",
					module->name);
				mutex_unlock(&goodix_modules.mutex);
				return 0;
			}
		}

		/* smaller priority value with higher priority level */
		list_for_each_entry_safe(ext_module, next,
					 &goodix_modules.head, list) {
			if (ext_module->priority >= module->priority) {
				insert_point = &ext_module->list;
				break;
			}
		}
	}

	if (module->funcs && module->funcs->init) {
		if (module->funcs->init(goodix_modules.core_data,
					module) < 0) {
			ts_err("Module [%s] init error",
			       module->name ? module->name : " ");
			mutex_unlock(&goodix_modules.mutex);
			return -EFAULT;
		}
	}

	list_add(&module->list, insert_point->prev);
	mutex_unlock(&goodix_modules.mutex);

	return 0;
}

static void goodix_register_ext_module_work(struct work_struct *work)
{
	struct goodix_ext_module *module =
			container_of(work, struct goodix_ext_module, work);

	ts_info("module register work IN");

	/* driver probe failed */
	if (core_module_prob_sate != CORE_MODULE_PROB_SUCCESS) {
		ts_err("Can't register ext_module core error");
		return;
	}

	if (__do_register_ext_module(module))
		ts_err("failed register module: %s", module->name);
	else
		ts_info("success register module: %s", module->name);
}

static void goodix_core_module_init(void)
{
	if (goodix_modules.initilized)
		return;
	goodix_modules.initilized = true;
	INIT_LIST_HEAD(&goodix_modules.head);
	mutex_init(&goodix_modules.mutex);
}

/**
 * goodix_register_ext_module - interface for register external module
 * to the core. This will create a workqueue to finish the real register
 * work and return immediately. The user need to check the final result
 * to make sure registe is success or fail.
 *
 * @module: pointer to external module to be register
 * return: 0 ok, <0 failed
 */
int goodix_register_ext_module(struct goodix_ext_module *module)
{
	if (!module)
		return -EINVAL;

	ts_info("IN");

	goodix_core_module_init();
	INIT_WORK(&module->work, goodix_register_ext_module_work);
	schedule_work(&module->work);

	ts_info("OUT");
	return 0;
}

/**
 * goodix_register_ext_module_no_wait
 * return: 0 ok, <0 failed
 */
int goodix_register_ext_module_no_wait(struct goodix_ext_module *module)
{
	if (!module)
		return -EINVAL;

	ts_info("IN");
	goodix_core_module_init();
	/* driver probe failed */
	if (core_module_prob_sate != CORE_MODULE_PROB_SUCCESS) {
		ts_err("Can't register ext_module core error");
		return -EINVAL;
	}
	return __do_register_ext_module(module);
}

/**
 * goodix_unregister_ext_module - interface for external module
 * to unregister external modules
 *
 * @module: pointer to external module
 * return: 0 ok, <0 failed
 */
int goodix_unregister_ext_module(struct goodix_ext_module *module)
{
	struct goodix_ext_module *ext_module, *next;
	bool found = false;

	if (!module)
		return -EINVAL;

	if (!goodix_modules.initilized)
		return -EINVAL;

	if (!goodix_modules.core_data)
		return -ENODEV;

	mutex_lock(&goodix_modules.mutex);
	if (!list_empty(&goodix_modules.head)) {
		list_for_each_entry_safe(ext_module, next,
					 &goodix_modules.head, list) {
			if (ext_module == module) {
				found = true;
				break;
			}
		}
	} else {
		mutex_unlock(&goodix_modules.mutex);
		return 0;
	}

	if (!found) {
		ts_debug("Module [%s] never registed",
				module->name);
		mutex_unlock(&goodix_modules.mutex);
		return 0;
	}

	list_del(&module->list);
	mutex_unlock(&goodix_modules.mutex);

	if (module->funcs && module->funcs->exit)
		module->funcs->exit(goodix_modules.core_data, module);

	ts_info("Moudle [%s] unregistered",
		module->name ? module->name : " ");
	return 0;
}

static void goodix_ext_sysfs_release(struct kobject *kobj)
{
	ts_info("Kobject released!");
}

#define to_ext_module(kobj)	container_of(kobj,\
				struct goodix_ext_module, kobj)
#define to_ext_attr(attr)	container_of(attr,\
				struct goodix_ext_attribute, attr)

static ssize_t goodix_ext_sysfs_show(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	struct goodix_ext_module *module = to_ext_module(kobj);
	struct goodix_ext_attribute *ext_attr = to_ext_attr(attr);

	if (ext_attr->show)
		return ext_attr->show(module, buf);

	return -EIO;
}

static ssize_t goodix_ext_sysfs_store(struct kobject *kobj,
		struct attribute *attr, const char *buf, size_t count)
{
	struct goodix_ext_module *module = to_ext_module(kobj);
	struct goodix_ext_attribute *ext_attr = to_ext_attr(attr);

	if (ext_attr->store)
		return ext_attr->store(module, buf, count);

	return -EIO;
}

static const struct sysfs_ops goodix_ext_ops = {
	.show = goodix_ext_sysfs_show,
	.store = goodix_ext_sysfs_store
};

static struct kobj_type goodix_ext_ktype = {
	.release = goodix_ext_sysfs_release,
	.sysfs_ops = &goodix_ext_ops,
};

struct kobj_type *goodix_get_default_ktype(void)
{
	return &goodix_ext_ktype;
}

struct kobject *goodix_get_default_kobj(void)
{
	struct kobject *kobj = NULL;

	if (goodix_modules.core_data &&
			goodix_modules.core_data->pdev)
		kobj = &goodix_modules.core_data->pdev->dev.kobj;
	return kobj;
}

/* show driver infomation */
static ssize_t driver_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "DriverVersion:%s\n",
			GOODIX_DRIVER_VERSION);
}

/* show chip infoamtion */
static ssize_t chip_info_show(struct device  *dev,
		struct device_attribute *attr, char *buf)
{
	struct goodix_ts_core *cd = dev_get_drvdata(dev);
	struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;
	struct goodix_fw_version chip_ver;
	struct goodix_ic_info ic_info;
	u8 temp_pid[8] = {0};
	int ret;
	int cnt = -EINVAL;

	if (hw_ops->read_version) {
		ret = hw_ops->read_version(cd, &chip_ver);
		if (!ret) {
			memcpy(temp_pid, chip_ver.rom_pid,
					sizeof(chip_ver.rom_pid));
			cnt = snprintf(&buf[0], PAGE_SIZE,
				"rom_pid:%s\nrom_vid:%02x%02x%02x\n",
				temp_pid, chip_ver.rom_vid[0],
				chip_ver.rom_vid[1], chip_ver.rom_vid[2]);
			cnt += snprintf(&buf[cnt], PAGE_SIZE,
				"patch_pid:%s\npatch_vid:%02x%02x%02x%02x\n",
				chip_ver.patch_pid, chip_ver.patch_vid[0],
				chip_ver.patch_vid[1], chip_ver.patch_vid[2],
				chip_ver.patch_vid[3]);
			cnt += snprintf(&buf[cnt], PAGE_SIZE,
				"sensorid:%d\n", chip_ver.sensor_id);
		}
	}

	if (hw_ops->get_ic_info) {
		ret = hw_ops->get_ic_info(cd, &ic_info);
		if (!ret) {
			cnt += snprintf(&buf[cnt], PAGE_SIZE,
					"config_id:%x\n",
					ic_info.version.config_id);
			cnt += snprintf(&buf[cnt], PAGE_SIZE,
					"config_version:%x\n",
					ic_info.version.config_version);
		}
	}

	return cnt;
}

/* reset chip */
static ssize_t goodix_ts_reset_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf,
				     size_t count)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;

	if (!buf || count <= 0)
		return -EINVAL;
	if (buf[0] != '0')
		hw_ops->reset(core_data, GOODIX_NORMAL_RESET_DELAY_MS);
	return count;
}

/* read config */
static ssize_t read_cfg_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	int ret;
	int i;
	int offset;
	char *cfg_buf = NULL;

	cfg_buf = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!cfg_buf)
		return -ENOMEM;

	if (hw_ops->read_config)
		ret = hw_ops->read_config(core_data, cfg_buf, PAGE_SIZE);
	else
		ret = -EINVAL;

	if (ret > 0) {
		offset = 0;
		for (i = 0; i < 200; i++) { // only print 200 bytes
			offset += snprintf(&buf[offset], PAGE_SIZE - offset,
					"%02x,", cfg_buf[i]);
			if ((i + 1) % 20 == 0)
				buf[offset++] = '\n';
		}
	}

	kfree(cfg_buf);
	if (ret <= 0)
		return ret;

	return offset;
}

static u8 ascii2hex(u8 a)
{
	s8 value = 0;

	if (a >= '0' && a <= '9')
		value = a - '0';
	else if (a >= 'A' && a <= 'F')
		value = a - 'A' + 0x0A;
	else if (a >= 'a' && a <= 'f')
		value = a - 'a' + 0x0A;
	else
		value = 0xff;

	return value;
}

static int goodix_ts_convert_0x_data(const u8 *buf, int buf_size,
				     u8 *out_buf, int *out_buf_len)
{
	int i, m_size = 0;
	int temp_index = 0;
	u8 high, low;

	for (i = 0; i < buf_size; i++) {
		if (buf[i] == 'x' || buf[i] == 'X')
			m_size++;
	}

	if (m_size <= 1) {
		ts_err("cfg file ERROR, valid data count:%d", m_size);
		return -EINVAL;
	}
	*out_buf_len = m_size;

	for (i = 0; i < buf_size; i++) {
		if (buf[i] != 'x' && buf[i] != 'X')
			continue;

		if (temp_index >= m_size) {
			ts_err("exchange cfg data error, overflow, temp_index:%d,m_size:%d",
					temp_index, m_size);
			return -EINVAL;
		}
		high = ascii2hex(buf[i + 1]);
		low = ascii2hex(buf[i + 2]);
		if (high == 0xff || low == 0xff) {
			ts_err("failed convert: 0x%x, 0x%x",
				buf[i + 1], buf[i + 2]);
			return -EINVAL;
		}
		out_buf[temp_index++] = (high << 4) + low;
	}
	return 0;
}

/* send config */
static ssize_t goodix_ts_send_cfg_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	struct goodix_ic_config *config = NULL;
	const struct firmware *cfg_img = NULL;
	int ret;

	if (buf[0] != '1')
		return -EINVAL;

	hw_ops->irq_enable(core_data, false);

	ret = request_firmware(&cfg_img, GOODIX_DEFAULT_CFG_NAME, dev);
	if (ret < 0) {
		ts_err("cfg file [%s] not available,errno:%d",
			GOODIX_DEFAULT_CFG_NAME, ret);
		goto exit;
	} else {
		ts_info("cfg file [%s] is ready", GOODIX_DEFAULT_CFG_NAME);
	}

	config = kzalloc(sizeof(*config), GFP_KERNEL);
	if (!config)
		goto exit;

	if (goodix_ts_convert_0x_data(cfg_img->data, cfg_img->size,
			config->data, &config->len)) {
		ts_err("convert config data FAILED");
		goto exit;
	}

	if (hw_ops->send_config) {
		ret = hw_ops->send_config(core_data, config->data, config->len);
		if (ret < 0)
			ts_err("send config failed");
	}

exit:
	hw_ops->irq_enable(core_data, true);
	kfree(config);
	if (cfg_img)
		release_firmware(cfg_img);

	return count;
}

/* reg read/write */
static u32 rw_addr;
static u32 rw_len;
static u8 rw_flag;
static u8 store_buf[32];
static u8 show_buf[PAGE_SIZE];
static ssize_t goodix_ts_reg_rw_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	int ret;

	if (!rw_addr || !rw_len) {
		ts_err("address(0x%x) and length(%d) can't be null",
			rw_addr, rw_len);
		return -EINVAL;
	}

	if (rw_flag != 1) {
		ts_err("invalid rw flag %d, only support [1/2]", rw_flag);
		return -EINVAL;
	}

	ret = hw_ops->read(core_data, rw_addr, show_buf, rw_len);
	if (ret < 0) {
		ts_err("failed read addr(%x) length(%d)", rw_addr, rw_len);
		return snprintf(buf, PAGE_SIZE,
			"failed read addr(%x), len(%d)\n",
			rw_addr, rw_len);
	}

	return snprintf(buf, PAGE_SIZE, "0x%x,%d {%*ph}\n",
		rw_addr, rw_len, rw_len, show_buf);
}

static ssize_t goodix_ts_reg_rw_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	char *pos = NULL;
	char *token = NULL;
	long result = 0;
	int ret;
	int i;

	if (!buf || !count) {
		ts_err("invalid parame");
		goto err_out;
	}

	if (buf[0] == 'r') {
		rw_flag = 1;
	} else if (buf[0] == 'w') {
		rw_flag = 2;
	} else {
		ts_err("string must start with 'r/w'");
		goto err_out;
	}

	/* get addr */
	pos = (char *)buf;
	pos += 2;
	token = strsep(&pos, ":");
	if (!token) {
		ts_err("invalid address info");
		goto err_out;
	} else {
		if (kstrtol(token, 16, &result)) {
			ts_err("failed get addr info");
			goto err_out;
		}
		rw_addr = (u32)result;
		ts_info("rw addr is 0x%x", rw_addr);
	}

	/* get length */
	token = strsep(&pos, ":");
	if (!token) {
		ts_err("invalid length info");
		goto err_out;
	} else {
		if (kstrtol(token, 0, &result)) {
			ts_err("failed get length info");
			goto err_out;
		}
		rw_len = (u32)result;
		ts_info("rw length info is %d", rw_len);
		if (rw_len > sizeof(store_buf)) {
			ts_err("data len > %lu", sizeof(store_buf));
			goto err_out;
		}
	}

	if (rw_flag == 1)
		return count;

	for (i = 0; i < rw_len; i++) {
		token = strsep(&pos, ":");
		if (!token) {
			ts_err("invalid data info");
			goto err_out;
		} else {
			if (kstrtol(token, 16, &result)) {
				ts_err("failed get data[%d] info", i);
				goto err_out;
			}
			store_buf[i] = (u8)result;
			ts_info("get data[%d]=0x%x", i, store_buf[i]);
		}
	}
	ret = hw_ops->write(core_data, rw_addr, store_buf, rw_len);
	if (ret < 0) {
		ts_err("failed write addr(%x) data %*ph", rw_addr,
			rw_len, store_buf);
		goto err_out;
	}

	ts_info("%s write to addr (%x) with data %*ph",
		"success", rw_addr, rw_len, store_buf);

	return count;
err_out:
	snprintf(show_buf, PAGE_SIZE, "%s\n",
		"invalid params, format{r/w:4100:length:[41:21:31]}");
	return -EINVAL;

}

/* show irq infomation */
static ssize_t goodix_ts_irq_info_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct irq_desc *desc;
	size_t offset = 0;
	int r;

	r = snprintf(&buf[offset], PAGE_SIZE, "irq:%u\n", core_data->irq);
	if (r < 0)
		return -EINVAL;

	offset += r;
	r = snprintf(&buf[offset], PAGE_SIZE - offset, "state:%s\n",
		     atomic_read(&core_data->irq_enabled) ?
		     "enabled" : "disabled");
	if (r < 0)
		return -EINVAL;

	desc = irq_to_desc(core_data->irq);
	offset += r;
	r = snprintf(&buf[offset], PAGE_SIZE - offset, "disable-depth:%d\n",
		     desc->depth);
	if (r < 0)
		return -EINVAL;

	offset += r;
	r = snprintf(&buf[offset], PAGE_SIZE - offset, "trigger-count:%zu\n",
		core_data->irq_trig_cnt);
	if (r < 0)
		return -EINVAL;

	offset += r;
	r = snprintf(&buf[offset], PAGE_SIZE - offset,
		     "echo 0/1 > irq_info to disable/enable irq\n");
	if (r < 0)
		return -EINVAL;

	offset += r;
	return offset;
}

/* enable/disable irq */
static ssize_t goodix_ts_irq_info_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;

	if (!buf || count <= 0)
		return -EINVAL;

	if (buf[0] != '0')
		hw_ops->irq_enable(core_data, true);
	else
		hw_ops->irq_enable(core_data, false);
	return count;
}

/* show esd status */
static ssize_t goodix_ts_esd_info_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct goodix_ts_esd *ts_esd = &core_data->ts_esd;
	int r = 0;

	r = snprintf(buf, PAGE_SIZE, "state:%s\n",
		     atomic_read(&ts_esd->esd_on) ?
		     "enabled" : "disabled");

	return r;
}

/* enable/disable esd */
static ssize_t goodix_ts_esd_info_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	if (!buf || count <= 0)
		return -EINVAL;

	if (buf[0] != '0')
		goodix_ts_blocking_notify(NOTIFY_ESD_ON, NULL);
	else
		goodix_ts_blocking_notify(NOTIFY_ESD_OFF, NULL);
	return count;
}

/* debug level show */
static ssize_t goodix_ts_debug_log_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	int r = 0;

	r = snprintf(buf, PAGE_SIZE, "state:%s\n",
		    debug_log_flag ?
		    "enabled" : "disabled");

	return r;
}

/* debug level store */
static ssize_t goodix_ts_debug_log_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	if (!buf || count <= 0)
		return -EINVAL;

	if (buf[0] != '0')
		debug_log_flag = true;
	else
		debug_log_flag = false;
	return count;
}

/* show die package site and mcu fabs */
#define DIE_INFO_START_FLASH_ADDR 0x1F300
static ssize_t die_info_show(struct device  *dev,
		struct device_attribute *attr, char *buf)
{
	struct goodix_ts_core *cd = dev_get_drvdata(dev);
	struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;
	u8 temp_buf[21];
	u8 pkg_site;
	u8 mcu_fab;
	int ret;

	ret = hw_ops->read_flash(cd, DIE_INFO_START_FLASH_ADDR, temp_buf, sizeof(temp_buf));
	if (ret < 0) {
		ts_err("read flash failed");
		return 0;
	}

	ts_info("die info:%*ph", (int)sizeof(temp_buf), temp_buf);

	pkg_site = temp_buf[1];
	mcu_fab = temp_buf[20];
	ret = snprintf(buf, PAGE_SIZE, "package_id:0x%02X mcu_fab:0x%02X\n", pkg_site, mcu_fab);

	return ret;
}

/*O6 code for HQ-391123 by liaoxianguo at 2024/07/10 start*/

static ssize_t goodix_ts_charger_info_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret = 0;

	if (!dev || !attr || !buf) {
		ts_err("dev/attr/buf is null");
		return -EINVAL;
	}
	ret = snprintf(buf, PAGE_SIZE, "state:%s\n",
			goodix_core_data->charger_status ?
			"enabled" : "disabled");
	if (ret < 0) {
		ts_err("snprintf charger_status error");
	}
	return ret;
}

static ssize_t goodix_ts_charger_info_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	if (!dev || !attr || !buf || count <= 0)
		return -EINVAL;
	if (buf[0] != '0') {
		goodix_core_data->charger_status = 1;
		ts_info("charger usb in");
		goodix_core_data->hw_ops->charger_on(goodix_core_data, true);
	} else {
		goodix_core_data->charger_status = 0;
		ts_info("charger usb exit");
		goodix_core_data->hw_ops->charger_on(goodix_core_data, false);
	}
	return count;
}
/*O6 code for HQ-391123 by liaoxianguo at 2024/07/10 end*/

/*N6 code for HQ-337468 by zhangzhijian5 at 2023/10/28 start*/
static ssize_t goodix_ts_aod_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret = 0;

	if (!dev || !attr || !buf) {
		ts_err("dev/attr/buf is null");
		return -EINVAL;
	}
	ret = snprintf(buf, PAGE_SIZE, "state:%s\n",
			goodix_core_data->aod_status ?
			"enabled" : "disabled");
	if (ret < 0) {
		ts_err("snprintf aod_status error");
	}
	return ret;
}
/* aod gesture_store */
static ssize_t goodix_ts_aod_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	if (!dev || !attr || !buf || count <= 0)
		return -EINVAL;
	if (buf[0] != '0') {
		goodix_core_data->aod_status = AOD_OPEN_STATUS;
		queue_work(goodix_core_data->gesture_wq, &goodix_core_data->gesture_work);
	} else {
		goodix_core_data->aod_status = AOD_CLOSE_STATUS;
		queue_work(goodix_core_data->gesture_wq, &goodix_core_data->gesture_work);
	}
	return count;
}
/*N6 code for HQ-337468 by zhangzhijian5 at 2023/10/28 end*/

/*N6 code for HQ-334519 by liaoxianguo at 2023/10/17 start*/
/* fod gesture show */
static ssize_t goodix_ts_fod_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int r = 0;
	r = snprintf(buf, PAGE_SIZE, "state:%s\n",
			goodix_core_data->fod_status ?
			"enabled" : "disabled");
	return r;
}
/* fod gesture_store */
static ssize_t goodix_ts_fod_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	if (!buf || count <= 0)
		return -EINVAL;
	if (buf[0] != '0') {
		goodix_core_data->fod_status = 1;
		queue_work(goodix_core_data->gesture_wq, &goodix_core_data->gesture_work);
	} else {
		goodix_core_data->fod_status = 0;
		queue_work(goodix_core_data->gesture_wq, &goodix_core_data->gesture_work);
	}
	return count;
}
/*N6 code for HQ-334519 by liaoxianguo at 2023/10/17 end*/

static DEVICE_ATTR(driver_info, 0440,
		driver_info_show, NULL);
static DEVICE_ATTR(chip_info, 0440,
		chip_info_show, NULL);
static DEVICE_ATTR(reset, 0220,
		NULL, goodix_ts_reset_store);
static DEVICE_ATTR(send_cfg, 0220,
		NULL, goodix_ts_send_cfg_store);
static DEVICE_ATTR(read_cfg, 0440,
		read_cfg_show, NULL);
static DEVICE_ATTR(reg_rw, 0664,
		goodix_ts_reg_rw_show, goodix_ts_reg_rw_store);
static DEVICE_ATTR(irq_info, 0664,
		goodix_ts_irq_info_show, goodix_ts_irq_info_store);
static DEVICE_ATTR(esd_info, 0664,
		goodix_ts_esd_info_show, goodix_ts_esd_info_store);
static DEVICE_ATTR(debug_log, 0664,
		goodix_ts_debug_log_show, goodix_ts_debug_log_store);
static DEVICE_ATTR(die_info, 0440,
		die_info_show, NULL);
/*O6 code for HQ-391123 by liaoxianguo at 2024/07/10 start*/
static DEVICE_ATTR(charger_info, 0664,
		goodix_ts_charger_info_show, goodix_ts_charger_info_store);
/*O6 code for HQ-391123 by liaoxianguo at 2024/07/10 end*/
/*N6 code for HQ-337468 by zhangzhijian5 at 2023/10/28 start*/
static DEVICE_ATTR(aod, 0664,
		goodix_ts_aod_show, goodix_ts_aod_store);
/*N6 code for HQ-337468 by zhangzhijian5 at 2023/10/28 end*/
/*N6 code for HQ-334519 by liaoxianguo at 2023/10/17 start*/
static DEVICE_ATTR(fod, 0664,
		goodix_ts_fod_show, goodix_ts_fod_store);
/*N6 code for HQ-334519 by liaoxianguo at 2023/10/17 end*/

static struct attribute *sysfs_attrs[] = {
	&dev_attr_driver_info.attr,
	&dev_attr_chip_info.attr,
	&dev_attr_reset.attr,
	&dev_attr_send_cfg.attr,
	&dev_attr_read_cfg.attr,
	&dev_attr_reg_rw.attr,
	&dev_attr_irq_info.attr,
	&dev_attr_esd_info.attr,
	&dev_attr_debug_log.attr,
	&dev_attr_die_info.attr,
	/*O6 code for HQ-391123 by liaoxianguo at 2024/07/10 start*/
	&dev_attr_charger_info.attr,
	/*O6 code for HQ-391123 by liaoxianguo at 2024/07/10 end*/
	/*N6 code for HQ-337468 by zhangzhijian5 at 2023/10/28 start*/
	&dev_attr_aod.attr,
	/*N6 code for HQ-337468 by zhangzhijian5 at 2023/10/28 end*/
	/*N6 code for HQ-334519 by liaoxianguo at 2023/10/17 start*/
	&dev_attr_fod.attr,
	/*N6 code for HQ-334519 by liaoxianguo at 2023/10/17 end*/
	NULL,
};

static const struct attribute_group sysfs_group = {
	.attrs = sysfs_attrs,
};

static int goodix_ts_sysfs_init(struct goodix_ts_core *core_data)
{
	int ret;

	ret = sysfs_create_group(&core_data->pdev->dev.kobj, &sysfs_group);
	if (ret) {
		ts_err("failed create core sysfs group");
		return ret;
	}

	return ret;
}

static void goodix_ts_sysfs_exit(struct goodix_ts_core *core_data)
{
	sysfs_remove_group(&core_data->pdev->dev.kobj, &sysfs_group);
}

/* prosfs create */
static int rawdata_proc_show(struct seq_file *m, void *v)
{
	struct ts_rawdata_info *info;
	struct goodix_ts_core *cd;
	int tx;
	int rx;
	int ret;
	int i;
	int index;

	if (!m || !v)
		return -EIO;

	cd = m->private;

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	ret = cd->hw_ops->get_capacitance_data(cd, info);
	if (ret < 0) {
		ts_err("failed to get_capacitance_data, exit!");
		goto exit;
	}

	rx = info->buff[0];
	tx = info->buff[1];
	seq_printf(m, "TX:%d  RX:%d\n", tx, rx);
	seq_puts(m, "mutual_rawdata:\n");
	index = 2;
	for (i = 0; i < tx * rx; i++) {
		seq_printf(m, "%5d,", info->buff[index + i]);
		if ((i + 1) % tx == 0)
			seq_puts(m, "\n");
	}
	seq_puts(m, "mutual_diffdata:\n");
	index += tx * rx;
	for (i = 0; i < tx * rx; i++) {
		seq_printf(m, "%3d,", info->buff[index + i]);
		if ((i + 1) % tx == 0)
			seq_puts(m, "\n");
	}

exit:
	kfree(info);
	return ret;
}

static int rawdata_proc_open(struct inode *inode, struct file *file)
{
	return single_open_size(file, rawdata_proc_show,
			pde_data(inode), PAGE_SIZE * 10);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops rawdata_proc_fops = {
	.proc_open = rawdata_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
#else
static const struct file_operations rawdata_proc_fops = {
	.open = rawdata_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

/*N6 code for HQ-305079 by dingying at 2023/09/29 start*/
static void goodix_ts_procfs_init(struct goodix_ts_core *core_data)
{
	struct proc_dir_entry *proc_entry;

	if (!proc_mkdir("goodix_ts", NULL))
		return;
	proc_entry = proc_create_data("tp_data_dump",
			0664, NULL, &rawdata_proc_fops, core_data);
	if (!proc_entry)
		ts_err("failed to create proc entry");
}

static void goodix_ts_procfs_exit(struct goodix_ts_core *core_data)
{
	remove_proc_entry("tp_data_dump", NULL);
	remove_proc_entry("goodix_ts", NULL);
}
/*N6 code for HQ-305079 by dingying at 2023/09/29 start*/

/* event notifier */
static BLOCKING_NOTIFIER_HEAD(ts_notifier_list);
/**
 * goodix_ts_register_client - register a client notifier
 * @nb: notifier block to callback on events
 *  see enum ts_notify_event in goodix_ts_core.h
 */
int goodix_ts_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&ts_notifier_list, nb);
}

/**
 * goodix_ts_unregister_client - unregister a client notifier
 * @nb: notifier block to callback on events
 *	see enum ts_notify_event in goodix_ts_core.h
 */
int goodix_ts_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&ts_notifier_list, nb);
}

/**
 * fb_notifier_call_chain - notify clients of fb_events
 *	see enum ts_notify_event in goodix_ts_core.h
 */
int goodix_ts_blocking_notify(enum ts_notify_event evt, void *v)
{
	int ret;

	ret = blocking_notifier_call_chain(&ts_notifier_list,
			(unsigned long)evt, v);
	return ret;
}

#if IS_ENABLED(CONFIG_OF)
/**
 * goodix_parse_dt_resolution - parse resolution from dt
 * @node: devicetree node
 * @board_data: pointer to board data structure
 * return: 0 - no error, <0 error
 */
static int goodix_parse_dt_resolution(struct device_node *node,
		struct goodix_ts_board_data *board_data)
{
	int ret;

	ret = of_property_read_u32(node, "goodix,panel-max-x",
				 &board_data->panel_max_x);
	if (ret) {
		ts_err("failed get panel-max-x");
		return ret;
	}

	ret = of_property_read_u32(node, "goodix,panel-max-y",
				 &board_data->panel_max_y);
	if (ret) {
		ts_err("failed get panel-max-y");
		return ret;
	}

	ret = of_property_read_u32(node, "goodix,panel-max-w",
				 &board_data->panel_max_w);
	if (ret) {
		ts_err("failed get panel-max-w");
		return ret;
	}

	ret = of_property_read_u32(node, "goodix,panel-max-p",
				 &board_data->panel_max_p);
	if (ret) {
		ts_err("failed get panel-max-p, use default");
		board_data->panel_max_p = GOODIX_PEN_MAX_PRESSURE;
	}

	return 0;
}

/**
 * goodix_parse_dt - parse board data from dt
 * @dev: pointer to device
 * @board_data: pointer to board data structure
 * return: 0 - no error, <0 error
 */
static int goodix_parse_dt(struct device_node *node,
	struct goodix_ts_board_data *board_data)
{
	const char *name_tmp;
	int r;

	if (!board_data) {
		ts_err("invalid board data");
		return -EINVAL;
	}

	r = of_get_named_gpio(node, "goodix,avdd-gpio", 0);
	if (r < 0) {
		ts_info("can't find avdd-gpio, use other power supply");
		board_data->avdd_gpio = 0;
	} else {
		ts_info("get avdd-gpio[%d] from dt", r);
		board_data->avdd_gpio = r;
	}

	r = of_get_named_gpio(node, "goodix,iovdd-gpio", 0);
	if (r < 0) {
		ts_info("can't find iovdd-gpio, use other power supply");
		board_data->iovdd_gpio = 0;
	} else {
		ts_info("get iovdd-gpio[%d] from dt", r);
		board_data->iovdd_gpio = r;
	}

	r = of_get_named_gpio(node, "goodix,reset-gpio", 0);
	if (r < 0) {
		ts_err("invalid reset-gpio in dt: %d", r);
		return -EINVAL;
	}
	ts_info("get reset-gpio[%d] from dt", r);
	board_data->reset_gpio = r;

	r = of_get_named_gpio(node, "goodix,irq-gpio", 0);
	if (r < 0) {
		ts_err("invalid irq-gpio in dt: %d", r);
		return -EINVAL;
	}
	ts_info("get irq-gpio[%d] from dt", r);
	board_data->irq_gpio = r;

	r = of_property_read_u32(node, "goodix,irq-flags",
			&board_data->irq_flags);
	if (r) {
		ts_err("invalid irq-flags");
		return -EINVAL;
	}

	memset(board_data->avdd_name, 0, sizeof(board_data->avdd_name));
	r = of_property_read_string(node, "goodix,avdd-name", &name_tmp);
	if (!r) {
		ts_info("avdd name from dt: %s", name_tmp);
		if (strlen(name_tmp) < sizeof(board_data->avdd_name))
			strncpy(board_data->avdd_name,
				name_tmp, sizeof(board_data->avdd_name));
		else
			ts_info("invalied avdd name length: %ld > %ld",
				strlen(name_tmp),
				sizeof(board_data->avdd_name));
	}

	memset(board_data->iovdd_name, 0, sizeof(board_data->iovdd_name));
	r = of_property_read_string(node, "goodix,iovdd-name", &name_tmp);
	if (!r) {
		ts_info("iovdd name from dt: %s", name_tmp);
		if (strlen(name_tmp) < sizeof(board_data->iovdd_name))
			strncpy(board_data->iovdd_name,
				name_tmp, sizeof(board_data->iovdd_name));
		else
			ts_info("invalied iovdd name length: %ld > %ld",
				strlen(name_tmp),
				sizeof(board_data->iovdd_name));
	}

	/* get firmware file name */
	r = of_property_read_string(node, "goodix,firmware-name", &name_tmp);
	if (!r) {
		ts_info("firmware name from dt: %s", name_tmp);
		strncpy(board_data->fw_name,
				name_tmp, sizeof(board_data->fw_name));
	} else {
		ts_info("can't find firmware name, use default: %s",
				TS_DEFAULT_FIRMWARE);
		strncpy(board_data->fw_name,
				TS_DEFAULT_FIRMWARE,
				sizeof(board_data->fw_name));
	}

	/* get config file name */
	r = of_property_read_string(node, "goodix,config-name", &name_tmp);
	if (!r) {
		ts_info("config name from dt: %s", name_tmp);
		strncpy(board_data->cfg_bin_name, name_tmp,
				sizeof(board_data->cfg_bin_name));
	} else {
		ts_info("can't find config name, use default: %s",
				TS_DEFAULT_CFG_BIN);
		strncpy(board_data->cfg_bin_name,
				TS_DEFAULT_CFG_BIN,
				sizeof(board_data->cfg_bin_name));
	}

	/* get xyz resolutions */
	r = goodix_parse_dt_resolution(node, board_data);
	if (r) {
		ts_err("Failed to parse resolutions:%d", r);
		return r;
	}

	/*N6 code for HQ-305061 by zhangzhijian5 at 2023/10/30 start*/
	/* get expert mode parameter */
	r = of_property_count_u32_elems(node, "goodix,touch-expert-array");
	if (r == GAME_ARRAY_LEN * GAME_ARRAY_SIZE) {
		of_property_read_u32_array(node,
						"goodix,touch-expert-array",
						board_data->touch_expert_array,
						r);
	} else {
		ts_err("Failed to parse touch-expert-array:%d", r);
	}
	/*N6 code for HQ-305061 by zhangzhijian5 at 2023/10/30 end*/

	/* get sleep mode flag */
	board_data->sleep_enable = of_property_read_bool(node,
			"goodix,sleep-enable");

	/*get pen-enable switch and pen keys, must after "key map"*/
	board_data->pen_enable = of_property_read_bool(node,
			"goodix,pen-enable");

	ts_info("[DT]x:%d, y:%d, w:%d, p:%d sleep_enable:%d pen_enable:%d",
		board_data->panel_max_x, board_data->panel_max_y,
		board_data->panel_max_w, board_data->panel_max_p,
		board_data->sleep_enable, board_data->pen_enable);
	return 0;
}
#endif

static void goodix_ts_report_pen(struct input_dev *dev,
		struct goodix_pen_data *pen_data)
{
	int i;

	mutex_lock(&dev->mutex);

	if (pen_data->coords.status == TS_TOUCH) {
		input_report_key(dev, BTN_TOUCH, 1);
		input_report_key(dev, pen_data->coords.tool_type, 1);
		input_report_abs(dev, ABS_X, pen_data->coords.x);
		input_report_abs(dev, ABS_Y, pen_data->coords.y);
		input_report_abs(dev, ABS_PRESSURE, pen_data->coords.p);
		if (pen_data->coords.p == 0)
			input_report_abs(dev, ABS_DISTANCE, 1);
		else
			input_report_abs(dev, ABS_DISTANCE, 0);
		input_report_abs(dev, ABS_TILT_X, pen_data->coords.tilt_x);
		input_report_abs(dev, ABS_TILT_Y, pen_data->coords.tilt_y);
		ts_debug("pen_data:x %d, y %d, p %d, tilt_x %d tilt_y %d key[%d %d]",
				pen_data->coords.x, pen_data->coords.y,
				pen_data->coords.p, pen_data->coords.tilt_x,
				pen_data->coords.tilt_y,
				pen_data->keys[0].status == TS_TOUCH ? 1 : 0,
				pen_data->keys[1].status == TS_TOUCH ? 1 : 0);
	} else {
		input_report_key(dev, BTN_TOUCH, 0);
		input_report_key(dev, pen_data->coords.tool_type, 0);
	}
	/* report pen button */
	for (i = 0; i < GOODIX_MAX_PEN_KEY; i++) {
		if (pen_data->keys[i].status == TS_TOUCH)
			input_report_key(dev, pen_data->keys[i].code, 1);
		else
			input_report_key(dev, pen_data->keys[i].code, 0);
	}

	input_sync(dev);
	mutex_unlock(&dev->mutex);
}

/*N6 code for HQ-334519 by liaoxianguo at 2023/10/17 start*/
static void goodix_ts_report_finger(struct input_dev *dev,
		struct goodix_touch_data *touch_data)
{
	struct goodix_ts_core *cd = input_get_drvdata(dev);
	unsigned int touch_num = touch_data->touch_num;
	int i;
	int resolution_factor = 0;
	int report_x = 0;
	int report_y = 0;
	mutex_lock(&dev->mutex);
	for (i = 0; i < GOODIX_MAX_TOUCH; i++) {
		if (touch_data->coords[i].status == TS_TOUCH) {
			ts_debug("report: id %d, x %d, y %d, w %d", i,
				touch_data->coords[i].x, touch_data->coords[i].y,
				touch_data->coords[i].w);
			/*
			 *Make sure the Touch function works properly regardless of
			 *whether the TouchIC firmware supports the super-resolution
			 *scanning function
			 */
			if (cd->ic_info.other.screen_max_x > cd->board_data.panel_max_x) {
				/* if supported */
				resolution_factor = cd->ic_info.other.screen_max_x / cd->board_data.panel_max_x;
				report_x = touch_data->coords[i].x;
				report_y = touch_data->coords[i].y;
			} else {
				/* if not supported */
				resolution_factor = cd->board_data.panel_max_x / cd->ic_info.other.screen_max_x;
				report_x = touch_data->coords[i].x * resolution_factor;
				report_y = touch_data->coords[i].y * resolution_factor;
			}
			ts_debug("panel_max_x: %d, screen_max_x:%d",
			cd->board_data.panel_max_x, cd->ic_info.other.screen_max_x);
			ts_debug("report: id %d, x %d, y %d, w %d resolution_factor:%d",
					i, report_x, report_y, touch_data->coords[i].w, resolution_factor);
			input_mt_slot(dev, i);
			input_mt_report_slot_state(dev, MT_TOOL_FINGER, true);
			input_report_abs(dev, ABS_MT_POSITION_X,
					report_x);
			input_report_abs(dev, ABS_MT_POSITION_Y,
					report_y);
			/*O6 code for HQ-422640 by liuyupei at 2024/10/10 start*/
			if (goodix_core_data->fod_status && touch_data->fp_flag && (i == touch_data->fod_id)) {
				input_report_abs(dev, ABS_MT_WIDTH_MAJOR,
						touch_data->overlay);
				input_report_abs(dev, ABS_MT_WIDTH_MINOR,
						touch_data->overlay);
			} else {
				input_report_abs(dev, ABS_MT_WIDTH_MAJOR,
						touch_data->coords[i].w);
				input_report_abs(dev, ABS_MT_WIDTH_MINOR,
						touch_data->coords[i].w);
			}
		/*N6 code for HQ-305061 by zhangzhijian5 at 2023/10/30 start*/
			last_touch_events_collect(i, 1);
		} else {
			input_mt_slot(dev, i);
			input_mt_report_slot_state(dev, MT_TOOL_FINGER, false);
			last_touch_events_collect(i, 0);
		/*N6 code for HQ-305061 by zhangzhijian5 at 2023/10/30 end*/
		}
	}
	if (goodix_core_data->fod_status && touch_data->fp_flag) {
		goodix_core_data->fod_finger = true;
		input_report_key(dev, BTN_INFO, 1);
		update_fod_press_status(1);
	} else {
		goodix_core_data->fod_finger = false;
		input_report_key(dev, BTN_INFO, 0);
		update_fod_press_status(0);
	}
	input_report_key(dev, BTN_TOUCH, touch_num > 0 ? 1 : 0);
	input_report_key(dev, BTN_TOOL_FINGER, touch_num > 0 ? 1 : 0);
	/*O6 code for HQ-422640 by liuyupei at 2024/10/10 end*/
	input_sync(dev);
	mutex_unlock(&dev->mutex);
}
/*N6 code for HQ-334519 by liaoxianguo at 2023/10/17 end*/

static int goodix_ts_request_handle(struct goodix_ts_core *cd,
	struct goodix_ts_event *ts_event)
{
	struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;
	int ret = -1;

	if (ts_event->request_code == REQUEST_TYPE_CONFIG)
		ret = goodix_send_ic_config(cd, CONFIG_TYPE_NORMAL);
	else if (ts_event->request_code == REQUEST_TYPE_RESET)
		ret = hw_ops->reset(cd, GOODIX_NORMAL_RESET_DELAY_MS);
	else
		ts_info("can not handle request type 0x%x",
			  ts_event->request_code);
	if (ret)
		ts_err("failed handle request 0x%x",
			 ts_event->request_code);
	else
		ts_info("success handle ic request 0x%x",
			  ts_event->request_code);
	return ret;
}

/**
 * goodix_ts_threadirq_func - Bottom half of interrupt
 * This functions is excuted in thread context,
 * sleep in this function is permit.
 *
 * @data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static irqreturn_t goodix_ts_threadirq_func(int irq, void *data)
{
	struct goodix_ts_core *core_data = data;
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	struct goodix_ext_module *ext_module, *next;
	struct goodix_ts_event *ts_event = &core_data->ts_event;
	struct goodix_ts_esd *ts_esd = &core_data->ts_esd;
	int ret;

	disable_irq_nosync(core_data->irq);

	ts_esd->irq_status = true;
	core_data->irq_trig_cnt++;
	/* inform external module */
	mutex_lock(&goodix_modules.mutex);
	list_for_each_entry_safe(ext_module, next,
				 &goodix_modules.head, list) {
		if (!ext_module->funcs->irq_event)
			continue;
		ret = ext_module->funcs->irq_event(core_data, ext_module);
		if (ret == EVT_CANCEL_IRQEVT) {
			mutex_unlock(&goodix_modules.mutex);
			enable_irq(core_data->irq);
			return IRQ_HANDLED;
		}
	}
	mutex_unlock(&goodix_modules.mutex);

	/* read touch data from touch device */
	ret = hw_ops->event_handler(core_data, ts_event);
	if (likely(!ret)) {
		/*O6 code for HQ-422640 by liuyupei at 2024/10/10 start*/
		if (ts_event->event_type & EVENT_TOUCH) {
			/* report touch */
			goodix_ts_report_finger(core_data->input_dev,
					&ts_event->touch_data);
		}
		if (core_data->board_data.pen_enable &&
				ts_event->event_type & EVENT_PEN) {
			goodix_ts_report_pen(core_data->pen_dev,
					&ts_event->pen_data);
		}
		if (ts_event->event_type & EVENT_REQUEST)
		/*O6 code for HQ-422640 by liuyupei at 2024/10/10 end*/
			goodix_ts_request_handle(core_data, ts_event);
	}

	enable_irq(core_data->irq);
	return IRQ_HANDLED;
}

/**
 * goodix_ts_init_irq - Requset interrput line from system
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static int goodix_ts_irq_setup(struct goodix_ts_core *core_data)
{
	const struct goodix_ts_board_data *ts_bdata = board_data(core_data);
	int ret;

	/* if ts_bdata-> irq is invalid */
	core_data->irq = gpio_to_irq(ts_bdata->irq_gpio);
	if (core_data->irq < 0) {
		ts_err("failed get irq num %d", core_data->irq);
		return -EINVAL;
	}

	ts_info("IRQ:%u,flags:%d", core_data->irq, (int)ts_bdata->irq_flags);
	ret = devm_request_threaded_irq(&core_data->pdev->dev,
				      core_data->irq, NULL,
				      goodix_ts_threadirq_func,
				      ts_bdata->irq_flags | IRQF_ONESHOT,
				      GOODIX_CORE_DRIVER_NAME,
				      core_data);
	if (ret < 0)
		ts_err("Failed to requeset threaded irq:%d", ret);
	else
		atomic_set(&core_data->irq_enabled, 1);

	return ret;
}

/**
 * goodix_ts_power_init - Get regulator for touch device
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static int goodix_ts_power_init(struct goodix_ts_core *core_data)
{
	struct goodix_ts_board_data *ts_bdata = board_data(core_data);
	struct device *dev = core_data->bus->dev;
	int ret = 0;

	ts_info("Power init");
	if (strlen(ts_bdata->avdd_name)) {
		core_data->avdd = devm_regulator_get(dev,
				 ts_bdata->avdd_name);
		if (IS_ERR_OR_NULL(core_data->avdd)) {
			ret = PTR_ERR(core_data->avdd);
			ts_err("Failed to get regulator avdd:%d", ret);
			core_data->avdd = NULL;
			return ret;
		}
	} else {
		ts_info("Avdd name is NULL");
	}

	if (strlen(ts_bdata->iovdd_name)) {
		core_data->iovdd = devm_regulator_get(dev,
				 ts_bdata->iovdd_name);
		if (IS_ERR_OR_NULL(core_data->iovdd)) {
			ret = PTR_ERR(core_data->iovdd);
			ts_err("Failed to get regulator iovdd:%d", ret);
			core_data->iovdd = NULL;
		}
	} else {
		ts_info("iovdd name is NULL");
	}

	return ret;
}

/**
 * goodix_ts_power_on - Turn on power to the touch device
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
int goodix_ts_power_on(struct goodix_ts_core *cd)
{
	int ret = 0;

	ts_info("Device power on");
	if (cd->power_on)
		return 0;

	ret = cd->hw_ops->power_on(cd, true);
	if (!ret)
		cd->power_on = 1;
	else
		ts_err("failed power on, %d", ret);
	return ret;
}

/**
 * goodix_ts_power_off - Turn off power to the touch device
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
int goodix_ts_power_off(struct goodix_ts_core *cd)
{
	int ret;

	ts_info("Device power off");
	if (!cd->power_on)
		return 0;

	ret = cd->hw_ops->power_on(cd, false);
	if (!ret)
		cd->power_on = 0;
	else
		ts_err("failed power off, %d", ret);

	return ret;
}

/**
 * goodix_ts_gpio_setup - Request gpio resources from GPIO subsysten
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static int goodix_ts_gpio_setup(struct goodix_ts_core *core_data)
{
	struct goodix_ts_board_data *ts_bdata = board_data(core_data);
	int r = 0;

	ts_info("GPIO setup,reset-gpio:%d, irq-gpio:%d",
		ts_bdata->reset_gpio, ts_bdata->irq_gpio);
	/*
	 * after kenerl3.13, gpio_ api is deprecated, new
	 * driver should use gpiod_ api.
	 */
	r = devm_gpio_request_one(&core_data->pdev->dev,
			ts_bdata->reset_gpio,
			GPIOF_OUT_INIT_LOW, "ts_reset_gpio");
	if (r < 0) {
		ts_err("Failed to request reset gpio, r:%d", r);
		return r;
	}

	r = devm_gpio_request_one(&core_data->pdev->dev,
			ts_bdata->irq_gpio,
			GPIOF_IN, "ts_irq_gpio");
	if (r < 0) {
		ts_err("Failed to request irq gpio, r:%d", r);
		return r;
	}

	if (ts_bdata->avdd_gpio > 0) {
		r = devm_gpio_request_one(&core_data->pdev->dev,
				ts_bdata->avdd_gpio,
				GPIOF_OUT_INIT_LOW, "ts_avdd_gpio");
		if (r < 0) {
			ts_err("Failed to request avdd-gpio, r:%d", r);
			return r;
		}
	}

	if (ts_bdata->iovdd_gpio > 0) {
		r = devm_gpio_request_one(&core_data->pdev->dev,
				ts_bdata->iovdd_gpio,
				GPIOF_OUT_INIT_LOW, "ts_iovdd_gpio");
		if (r < 0) {
			ts_err("Failed to request iovdd-gpio, r:%d", r);
			return r;
		}
	}

	return 0;
}

/**
 * goodix_ts_input_dev_config - Requset and config a input device
 *  then register it to input sybsystem.
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static int goodix_ts_input_dev_config(struct goodix_ts_core *core_data)
{
	struct goodix_ts_board_data *ts_bdata = board_data(core_data);
	struct input_dev *input_dev = NULL;
	static char ts_phys[32];
	int r;

	input_dev = input_allocate_device();
	if (!input_dev) {
		ts_err("Failed to allocated input device");
		return -ENOMEM;
	}

	core_data->input_dev = input_dev;
	input_set_drvdata(input_dev, core_data);

	input_dev->name = GOODIX_CORE_DRIVER_NAME;
	sprintf(ts_phys, "%s/input0", input_dev->name);
	input_dev->phys = ts_phys;
	input_dev->id.bustype = core_data->bus->bus_type;
	input_dev->id.vendor = 0x27C6;
	input_dev->id.product = 0x0001;
	input_dev->id.version = 0x0100;

	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);
	set_bit(EV_ABS, input_dev->evbit);
	set_bit(BTN_TOUCH, input_dev->keybit);
	set_bit(BTN_TOOL_FINGER, input_dev->keybit);
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
	/*N6 code for HQ-334519 by liaoxianguo at 2023/10/17 start*/
	set_bit(BTN_INFO, input_dev->keybit);
	/*N6 code for HQ-334519 by liaoxianguo at 2023/10/17 end*/
	/* set input parameters */
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
			     0, ts_bdata->panel_max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
			     0, ts_bdata->panel_max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
			     0, ts_bdata->panel_max_w, 0, 0);
#ifdef INPUT_TYPE_B_PROTOCOL
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 7, 0)
	input_mt_init_slots(input_dev, GOODIX_MAX_TOUCH,
			    INPUT_MT_DIRECT);
#else
	input_mt_init_slots(input_dev, GOODIX_MAX_TOUCH);
#endif
#endif

	input_set_capability(input_dev, EV_KEY, KEY_POWER);
	input_set_capability(input_dev, EV_KEY, KEY_WAKEUP);
	input_set_capability(input_dev, EV_KEY, KEY_GOTO);
	/*N6 code for HQ-334519 by liaoxianguo at 2023/10/17 start*/
	input_set_capability(input_dev, EV_KEY, BTN_INFO);
	/*N6 code for HQ-334519 by liaoxianguo at 2023/10/17 end*/
	r = input_register_device(input_dev);
	if (r < 0) {
		ts_err("Unable to register input device");
		input_free_device(input_dev);
		return r;
	}

	return 0;
}

static int goodix_ts_pen_dev_config(struct goodix_ts_core *core_data)
{
	struct goodix_ts_board_data *ts_bdata = board_data(core_data);
	struct input_dev *pen_dev = NULL;
	static char ts_phys[32];
	int r;

	pen_dev = input_allocate_device();
	if (!pen_dev) {
		ts_err("Failed to allocated pen device");
		return -ENOMEM;
	}

	core_data->pen_dev = pen_dev;
	input_set_drvdata(pen_dev, core_data);

	pen_dev->name = GOODIX_PEN_DRIVER_NAME;
	sprintf(ts_phys, "%s/input0", pen_dev->name);
	pen_dev->phys = ts_phys;
	pen_dev->id.bustype = core_data->bus->bus_type;
	pen_dev->id.vendor = 0x27C6;
	pen_dev->id.product = 0x0002;
	pen_dev->id.version = 0x0100;

	pen_dev->evbit[0] |= BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	set_bit(ABS_X, pen_dev->absbit);
	set_bit(ABS_Y, pen_dev->absbit);
	set_bit(ABS_TILT_X, pen_dev->absbit);
	set_bit(ABS_TILT_Y, pen_dev->absbit);
	set_bit(BTN_STYLUS, pen_dev->keybit);
	set_bit(BTN_STYLUS2, pen_dev->keybit);
	set_bit(BTN_TOUCH, pen_dev->keybit);
	set_bit(BTN_TOOL_PEN, pen_dev->keybit);
	set_bit(INPUT_PROP_DIRECT, pen_dev->propbit);
	input_set_abs_params(pen_dev, ABS_X, 0, ts_bdata->panel_max_x, 0, 0);
	input_set_abs_params(pen_dev, ABS_Y, 0, ts_bdata->panel_max_y, 0, 0);
	input_set_abs_params(pen_dev, ABS_PRESSURE, 0,
			     ts_bdata->panel_max_p, 0, 0);
	input_set_abs_params(pen_dev, ABS_DISTANCE, 0, 255, 0, 0);
	input_set_abs_params(pen_dev, ABS_TILT_X,
			-GOODIX_PEN_MAX_TILT, GOODIX_PEN_MAX_TILT, 0, 0);
	input_set_abs_params(pen_dev, ABS_TILT_Y,
			-GOODIX_PEN_MAX_TILT, GOODIX_PEN_MAX_TILT, 0, 0);

	r = input_register_device(pen_dev);
	if (r < 0) {
		ts_err("Unable to register pen device");
		input_free_device(pen_dev);
		return r;
	}

	return 0;
}

void goodix_ts_input_dev_remove(struct goodix_ts_core *core_data)
{
	if (!core_data->input_dev)
		return;
	input_unregister_device(core_data->input_dev);
	input_free_device(core_data->input_dev);
	core_data->input_dev = NULL;
}

void goodix_ts_pen_dev_remove(struct goodix_ts_core *core_data)
{
	if (!core_data->pen_dev)
		return;
	input_unregister_device(core_data->pen_dev);
	input_free_device(core_data->pen_dev);
	core_data->pen_dev = NULL;
}

/**
 * goodix_ts_esd_work - check hardware status and recovery
 *  the hardware if needed.
 */
static void goodix_ts_esd_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct goodix_ts_esd *ts_esd = container_of(dwork,
			struct goodix_ts_esd, esd_work);
	struct goodix_ts_core *cd = container_of(ts_esd,
			struct goodix_ts_core, ts_esd);
	const struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;
	int ret = 0;

	if (ts_esd->irq_status)
		goto exit;

	if (!atomic_read(&ts_esd->esd_on))
		return;

	if (!hw_ops->esd_check)
		return;

	ret = hw_ops->esd_check(cd);
	if (ret) {
		ts_err("esd check failed");
		goodix_ts_power_off(cd);
		usleep_range(5000, 5100);
		goodix_ts_power_on(cd);
	}

exit:
	ts_esd->irq_status = false;
	if (atomic_read(&ts_esd->esd_on))
		schedule_delayed_work(&ts_esd->esd_work, 2 * HZ);
}

/**
 * goodix_ts_esd_on - turn on esd protection
 */
static void goodix_ts_esd_on(struct goodix_ts_core *cd)
{
	struct goodix_ic_info_misc *misc = &cd->ic_info.misc;
	struct goodix_ts_esd *ts_esd = &cd->ts_esd;

	if (!misc->esd_addr)
		return;

	if (atomic_read(&ts_esd->esd_on))
		return;

	atomic_set(&ts_esd->esd_on, 1);
	if (!schedule_delayed_work(&ts_esd->esd_work, 2 * HZ))
		ts_info("esd work already in workqueue");

	ts_info("esd on");
}

/**
 * goodix_ts_esd_off - turn off esd protection
 */
static void goodix_ts_esd_off(struct goodix_ts_core *cd)
{
	struct goodix_ts_esd *ts_esd = &cd->ts_esd;
	int ret;

	if (!atomic_read(&ts_esd->esd_on))
		return;

	atomic_set(&ts_esd->esd_on, 0);
	ret = cancel_delayed_work_sync(&ts_esd->esd_work);
	ts_info("Esd off, esd work state %d", ret);
}

/**
 * goodix_esd_notifier_callback - notification callback
 *  under certain condition, we need to turn off/on the esd
 *  protector, we use kernel notify call chain to achieve this.
 *
 *  for example: before firmware update we need to turn off the
 *  esd protector and after firmware update finished, we should
 *  turn on the esd protector.
 */
static int goodix_esd_notifier_callback(struct notifier_block *nb,
		unsigned long action, void *data)
{
	struct goodix_ts_esd *ts_esd = container_of(nb,
			struct goodix_ts_esd, esd_notifier);

	switch (action) {
	case NOTIFY_FWUPDATE_START:
	case NOTIFY_SUSPEND:
	case NOTIFY_ESD_OFF:
		goodix_ts_esd_off(ts_esd->ts_core);
		break;
	case NOTIFY_FWUPDATE_FAILED:
	case NOTIFY_FWUPDATE_SUCCESS:
	case NOTIFY_RESUME:
	case NOTIFY_ESD_ON:
		goodix_ts_esd_on(ts_esd->ts_core);
		break;
	default:
		break;
	}

	return 0;
}

/**
 * goodix_ts_esd_init - initialize esd protection
 */
int goodix_ts_esd_init(struct goodix_ts_core *cd)
{
	struct goodix_ic_info_misc *misc = &cd->ic_info.misc;
	struct goodix_ts_esd *ts_esd = &cd->ts_esd;

	if (!cd->hw_ops->esd_check || !misc->esd_addr) {
		ts_info("missing key info for esd check");
		return 0;
	}

	INIT_DELAYED_WORK(&ts_esd->esd_work, goodix_ts_esd_work);
	ts_esd->ts_core = cd;
	atomic_set(&ts_esd->esd_on, 0);
	ts_esd->esd_notifier.notifier_call = goodix_esd_notifier_callback;
	goodix_ts_register_notifier(&ts_esd->esd_notifier);
	goodix_ts_esd_on(cd);

	return 0;
}

/*N6 code for HQ-334519 by liaoxianguo at 2023/10/17 start*/
static void goodix_ts_release_connects(struct goodix_ts_core *core_data)
{
	struct input_dev *input_dev = core_data->input_dev;
	int i;

	mutex_lock(&input_dev->mutex);
	for (i = 0; i < GOODIX_MAX_TOUCH; i++) {
		input_mt_slot(input_dev, i);
		input_mt_report_slot_state(input_dev,
				MT_TOOL_FINGER,
				false);
		/*N6 code for HQ-305061 by zhangzhijian5 at 2023/10/30 start*/
		last_touch_events_collect(i, 0);
		/*N6 code for HQ-305061 by zhangzhijian5 at 2023/10/30 end*/
	}
	/*O6 code for HQ-422640 by liuyupei at 2024/10/10 start*/
	core_data->fod_finger = false;
	input_report_key(input_dev, BTN_INFO, 0);
	update_fod_press_status(0);
	input_report_key(input_dev, BTN_TOUCH, 0);
	input_report_key(input_dev, BTN_TOOL_FINGER, 0);
	/*O6 code for HQ-422640 by liuyupei at 2024/10/10 end*/
	input_sync(input_dev);

	mutex_unlock(&input_dev->mutex);
	if (core_data->gesture_type)
		core_data->hw_ops->after_event_handler(core_data);
}
/*N6 code for HQ-334519 by liaoxianguo at 2023/10/17 end*/

/**
 * goodix_ts_suspend - Touchscreen suspend function
 * Called by PM/FB/EARLYSUSPEN module to put the device to sleep
 */
static int goodix_ts_suspend(struct goodix_ts_core *core_data)
{
	struct goodix_ext_module *ext_module, *next;
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	int ret;
	/* N6 code for HQ-305073 by liaoxianguo at 2023/10/01 start */
	if (core_data->init_stage < CORE_INIT_STAGE2 ||
			atomic_read(&core_data->suspended)) {
		ts_err("Not start suspend");
		return 0;
	}
	/* N6 code for HQ-305073 by liaoxianguo at 2023/10/01 end */
	ts_info("Suspend start");
	atomic_set(&core_data->suspended, 1);
	/* disable irq */
	hw_ops->irq_enable(core_data, false);

	/*
	 * notify suspend event, inform the esd protector
	 * and charger detector to turn off the work
	 */
	goodix_ts_blocking_notify(NOTIFY_SUSPEND, NULL);


	/* inform external module */
	mutex_lock(&goodix_modules.mutex);
	if (!list_empty(&goodix_modules.head)) {
		list_for_each_entry_safe(ext_module, next,
					 &goodix_modules.head, list) {
			if (!ext_module->funcs->before_suspend)
				continue;

			ret = ext_module->funcs->before_suspend(core_data,
							      ext_module);
			if (ret == EVT_CANCEL_SUSPEND) {
				mutex_unlock(&goodix_modules.mutex);
				ts_info("Canceled by module:%s",
					ext_module->name);
				goto out;
			}
		}
	}
	mutex_unlock(&goodix_modules.mutex);

	/*N6 code for HQ-305064 by dingying at 2023/10/11 start*/
	core_data->work_status = TP_SLEEP;
	/*N6 code for HQ-305064 by dingying at 2023/10/11 end*/

	/* enter sleep mode or power off */
	if (core_data->board_data.sleep_enable)
		hw_ops->suspend(core_data);
	else
		goodix_ts_power_off(core_data);
	/*N6 code for HQ-340651 by huangshiquan at 2023/10/29 start*/
	goodix_ts_suspend_state = true;
	/*N6 code for HQ-340651 by huangshiquan at 2023/10/29 end*/
	/* inform exteranl modules */
	mutex_lock(&goodix_modules.mutex);
	if (!list_empty(&goodix_modules.head)) {
		list_for_each_entry_safe(ext_module, next,
					&goodix_modules.head, list) {
			if (!ext_module->funcs->after_suspend)
				continue;

			ret = ext_module->funcs->after_suspend(core_data,
							     ext_module);
			if (ret == EVT_CANCEL_SUSPEND) {
				mutex_unlock(&goodix_modules.mutex);
				ts_info("Canceled by module:%s",
					ext_module->name);
				goto out;
			}
		}
	}
	mutex_unlock(&goodix_modules.mutex);

out:
	goodix_ts_release_connects(core_data);
	ts_info("Suspend end");
	return 0;
}

/**
 * goodix_ts_resume - Touchscreen resume function
 * Called by PM/FB/EARLYSUSPEN module to wakeup device
 */
static int goodix_ts_resume(struct goodix_ts_core *core_data)
{
	struct goodix_ext_module *ext_module, *next;
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	int ret;

	if (core_data->init_stage < CORE_INIT_STAGE2 ||
			!atomic_read(&core_data->suspended))
		return 0;

	ts_info("Resume start");
	atomic_set(&core_data->suspended, 0);
	/*N6 code for HQ-305056 by zhangzhijian5 at 2023/10/26 start*/
	hw_ops->irq_enable(core_data, true);
	/*N6 code for HQ-305056 by zhangzhijian5 at 2023/10/26 end*/

	mutex_lock(&goodix_modules.mutex);
	if (!list_empty(&goodix_modules.head)) {
		list_for_each_entry_safe(ext_module, next,
					 &goodix_modules.head, list) {
			if (!ext_module->funcs->before_resume)
				continue;

			ret = ext_module->funcs->before_resume(core_data,
					ext_module);
			/*N6 code for HQ-340651 by huangshiquan at 2023/10/29 start*/
			if (ret == EVT_CANCEL_RESUME && goodix_ts_suspend_state == false) {
				mutex_unlock(&goodix_modules.mutex);
				ts_info("Canceled by module:%s",
					ext_module->name);
				goto out;
			}
			/*N6 code for HQ-340651 by huangshiquan at 2023/10/29 end*/
		}
	}
	mutex_unlock(&goodix_modules.mutex);

	/* reset device or power on*/
	/*N6 code for HQ-340134 by huangshiquan at 2023/10/30 start*/
	if (core_data->board_data.sleep_enable || goodix_ts_suspend_state == false)
		hw_ops->resume(core_data);
	else
		goodix_ts_power_on(core_data);
	/*N6 code for HQ-340134 by huangshiquan at 2023/10/30 end*/
	/*N6 code for HQ-305064 by dingying at 2023/10/11 start*/
	core_data->work_status = TP_NORMAL;
	/*N6 code for HQ-305064 by dingying at 2023/10/11 end*/
	mutex_lock(&goodix_modules.mutex);
	if (!list_empty(&goodix_modules.head)) {
		list_for_each_entry_safe(ext_module, next,
					 &goodix_modules.head, list) {
			if (!ext_module->funcs->after_resume)
				continue;

			ret = ext_module->funcs->after_resume(core_data,
							    ext_module);
			if (ret == EVT_CANCEL_RESUME) {
				mutex_unlock(&goodix_modules.mutex);
				ts_info("Canceled by module:%s",
					ext_module->name);
				goto out;
			}
		}
	}
	mutex_unlock(&goodix_modules.mutex);

out:
	core_data->work_status = TP_NORMAL;
	/*O6 code for HQ-391123 by liaoxianguo at 2024/07/10 start*/
	if (core_data->charger_status)
		hw_ops->charger_on(core_data, true);
	/*O6 code for HQ-391123 by liaoxianguo at 2024/07/10 end*/
	/*N6 code for HQ-305056 by zhangzhijian5 at 2023/10/26 start*/
	if (core_data->palm_status)
		ret = hw_ops->palm_on(core_data, core_data->palm_status);
	/*N6 code for HQ-305056 by zhangzhijian5 at 2023/10/26 end*/
	/* enable irq */
	hw_ops->irq_enable(core_data, true);
	/* open esd */
	goodix_ts_blocking_notify(NOTIFY_RESUME, NULL);
	/*N6 code for HQ-340651 by huangshiquan at 2023/10/29 start*/
	goodix_ts_suspend_state = false;
	/*N6 code for HQ-340651 by huangshiquan at 2023/10/29 end*/
	ts_info("Resume end");
	return 0;
}

/*N6 code for HQ-346992 by zhangzhijian5 at 2023/12/04 start*/
static void goodix_set_sleep_gesture_work(struct work_struct *work)
{
	struct goodix_ts_core *core_data =
		container_of(work, struct goodix_ts_core, sleep_gesture_work);
	goodix_sleep_to_gesture(core_data);
}
/*N6 code for HQ-346992 by zhangzhijian5 at 2023/12/04 end*/

/*O6 code for HQ-402304 by liaoxianguo at 2024/07/18 start*/
static void goodix_ts_suspend_work(struct work_struct *work)
{
	struct goodix_ts_core *core_data =
		container_of(work, struct goodix_ts_core, suspend_work);
	goodix_ts_suspend(core_data);
}
static void goodix_ts_resume_work(struct work_struct *work)
{
	struct goodix_ts_core *core_data =
		container_of(work, struct goodix_ts_core, resume_work);
	goodix_ts_resume(core_data);
}
/*O6 code for HQ-402304 by liaoxianguo at 2024/07/18 end*/

/* O6 code for HQ-390162 by liuyupei at 2024/6/28 start */
#if IS_ENABLED(CONFIG_MI_DISP_NOTIFIER)
/**
 * goodix_ts_fb_notifier_callback - Framebuffer notifier callback
 * Called by kernel during framebuffer blanck/unblank phrase
 */
static int goodix_ts_fb_notifier_callback(struct notifier_block *self,
	unsigned long event, void *data)
{
	struct goodix_ts_core *core_data =
		container_of(self, struct goodix_ts_core, fb_notifier);
	struct mi_disp_notifier *evdata = data;
	int blank = 0;
	ts_info("goodix_ts_fb_notifier_callback IN");
	if (!(event == MI_DISP_DPMS_EARLY_EVENT ||
		event == MI_DISP_DPMS_EVENT)) {
		ts_info("event(%lu) do not need process", event);
		return 0;
	}
	if (evdata && evdata->data && core_data) {
		blank = *(int *)(evdata->data);
		ts_info("notifier tp event:%lu, code:%d.", event, blank);
		/*O6 code for HQ-402304 by liaoxianguo at 2024/07/18 start*/
		if (event == MI_DISP_DPMS_EVENT
			&& (blank == MI_DISP_DPMS_POWERDOWN
			|| blank == MI_DISP_DPMS_LP1
			|| blank == MI_DISP_DPMS_LP2)) {
			ts_info("event:%lu,blank:%d", event, blank);
			flush_workqueue(core_data->event_wq);
			queue_work(core_data->event_wq, &core_data->suspend_work);
		} else if (event == MI_DISP_DPMS_EVENT && blank == MI_DISP_DPMS_ON) {
			ts_info("touchpanel resume, event:%lu,blank:%d", event, blank);
			flush_workqueue(core_data->event_wq);
			queue_work(core_data->event_wq, &core_data->resume_work);
		}
		/*O6 code for HQ-402304 by liaoxianguo at 2024/07/18 end*/
	}
	return 0;
}
#endif
/* O6 code for HQ-390162 by liuyupei at 2024/6/28 end */
/* O6 code for HQ-390162 by liuyupei at 2024/6/28 start */
#if IS_ENABLED(CONFIG_PM)
#if 0
/**
 * goodix_ts_pm_suspend - PM suspend function
 * Called by kernel during system suspend phrase
 */

static int goodix_ts_pm_suspend(struct device *dev)
{
	struct goodix_ts_core *core_data =
		dev_get_drvdata(dev);

	return goodix_ts_suspend(core_data);
}
/**
 * goodix_ts_pm_resume - PM resume function
 * Called by kernel during system wakeup
 */
static int goodix_ts_pm_resume(struct device *dev)
{
	struct goodix_ts_core *core_data =
		dev_get_drvdata(dev);

	return goodix_ts_resume(core_data);
}
#endif
#endif
/* O6 code for HQ-390162 by liuyupei at 2024/6/28  */
/**
 * goodix_generic_noti_callback - generic notifier callback
 *  for goodix touch notification event.
 */
static int goodix_generic_noti_callback(struct notifier_block *self,
		unsigned long action, void *data)
{
	struct goodix_ts_core *cd = container_of(self,
			struct goodix_ts_core, ts_notifier);
	const struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;

	if (cd->init_stage < CORE_INIT_STAGE2)
		return 0;

	ts_info("notify event type 0x%x", (unsigned int)action);
	switch (action) {
	case NOTIFY_FWUPDATE_START:
		hw_ops->irq_enable(cd, 0);
		break;
	case NOTIFY_FWUPDATE_SUCCESS:
	case NOTIFY_FWUPDATE_FAILED:
		if (hw_ops->read_version(cd, &cd->fw_version))
			ts_info("failed read fw version info[ignore]");
		hw_ops->irq_enable(cd, 1);
		break;
	default:
		break;
	}
	return 0;
}

/*O6 code for HQ-391123 by liaoxianguo at 2024/07/10 start*/
static int goodix_get_charging_status(void)
{
	struct power_supply *usb_psy = NULL;
	struct power_supply *dc_psy = NULL;
	union power_supply_propval val = {0};
	int rc = 0;
	int is_charging = 0;
	is_charging = !!power_supply_is_system_supplied();
	if (!is_charging)
		return 0;
	dc_psy = power_supply_get_by_name("wireless");
	if (dc_psy) {
		rc = power_supply_get_property(dc_psy, POWER_SUPPLY_PROP_ONLINE, &val);
		if (rc < 0)
			ts_err("Couldn't get DC online status, rc=%d\n", rc);
		else if (val.intval == 1)
			return 1;
	}
	usb_psy = power_supply_get_by_name("usb");
	if (usb_psy) {
		rc = power_supply_get_property(usb_psy, POWER_SUPPLY_PROP_ONLINE, &val);
		if (rc < 0)
			ts_err("Couldn't get usb online status, rc=%d\n", rc);
		else if (val.intval == 1)
			return 1;
	}
	return 0;
}
static void charger_power_supply_work(struct work_struct *work)
{
	struct goodix_ts_core *core_data = NULL;
	const struct goodix_ts_hw_ops *hw_ops = NULL;
	int charge_status = -1;

	if (!work) {
		ts_err("work is null");
	}
	core_data = container_of(work, struct goodix_ts_core, power_supply_work);
	hw_ops = core_data->hw_ops;

	if (core_data->init_stage < CORE_INIT_STAGE2 || atomic_read(&core_data->suspended)) {
		ts_debug("Init stage,forbid changing charger status");
		return;
	}
	charge_status = !!goodix_get_charging_status();
	ts_debug("power supply changed,Power_supply_event:%d", charge_status);
	if (charge_status != core_data->charger_status || core_data->charger_status < 0) {
		core_data->charger_status = charge_status;
		if (charge_status) {
			ts_info("charger usb in");
			hw_ops->charger_on(core_data, true);
		} else {
			ts_info("charger usb exit");
			hw_ops->charger_on(core_data, false);
		}
	}
}
static int charger_status_event_callback(struct notifier_block *nb, unsigned long event, void *ptr)
{
	struct goodix_ts_core *core_data = container_of(nb, struct goodix_ts_core, charger_notifier);
	if (!core_data)
		return 0;
	queue_work(core_data->event_wq, &core_data->power_supply_work);
	return 0;
}
/*O6 code for HQ-391123 by liaoxianguo at 2024/07/10 end*/

static void goodix_self_check(struct work_struct *work)
{
	struct goodix_ts_core *cd =
			container_of(work, struct goodix_ts_core, self_check_work);
	u32 fw_state_addr = cd->ic_info.misc.fw_state_addr;
	int update_flag = UPDATE_MODE_BLOCK | UPDATE_MODE_SRC_REQUEST | UPDATE_MODE_FORCE;
	u8 cur_cycle_cnt = 0;
	u8 pre_cycle_cnt = 0;
	int err_cnt = 0;
	int retry = 5;

	while (retry--) {
		cd->hw_ops->read(cd, fw_state_addr, &cur_cycle_cnt, 1);
		if (cur_cycle_cnt == pre_cycle_cnt)
			err_cnt++;
		pre_cycle_cnt = cur_cycle_cnt;
		msleep(20);
	}
	if (err_cnt > 1) {
		ts_err("Warning! The firmware maybe running abnormal, need upgrade.");
		goodix_do_fw_update(cd->ic_configs[CONFIG_TYPE_NORMAL],
				update_flag);
	}
}

int goodix_ts_stage2_init(struct goodix_ts_core *cd)
{
	int ret;
	mutex_init(&cd->report_mutex);
	/*O6 code for HQ-392899 by liaoxianguo at 2024/07/11 start*/
	mutex_init(&cd->edge_data_mutex);
	/*O6 code for HQ-392899 by liaoxianguo at 2024/07/11 start*/
	/* alloc/config/register input device */
	ret = goodix_ts_input_dev_config(cd);
	if (ret < 0) {
		ts_err("failed set input device");
		return ret;
	}

	if (cd->board_data.pen_enable) {
		ret = goodix_ts_pen_dev_config(cd);
		if (ret < 0) {
			ts_err("failed set pen device");
			goto err_finger;
		}
	}
	/* request irq line */
	ret = goodix_ts_irq_setup(cd);
	if (ret < 0) {
		ts_info("failed set irq");
		goto exit;
	}
	ts_info("success register irq");

/*O6 code for HQ-391123 by liaoxianguo at 2024/07/10 start*/
	cd->event_wq = alloc_workqueue("gtp-event-queue",
					WQ_UNBOUND | WQ_HIGHPRI | WQ_CPU_INTENSIVE, 1);
	if (!cd->event_wq) {
		ts_err("goodix cannot create event work thread");
		ret = -ENOMEM;
		goto exit;
	}
/*O6 code for HQ-391123 by liaoxianguo at 2024/07/10 end*/
/*N6 code for HQ-334519 by liaoxianguo at 2023/10/17 start*/
	cd->gesture_wq = alloc_workqueue("gtp-gesture-queue",
					WQ_UNBOUND | WQ_HIGHPRI | WQ_CPU_INTENSIVE, 1);
	if (!cd->gesture_wq) {
		ts_err("goodix cannot create gesture work thread");
		ret = -ENOMEM;
		goto exit;
	}
	INIT_WORK(&cd->gesture_work, goodix_set_gesture_work);
/*O6 code for HQ-402304 by liaoxianguo at 2024/07/18 start*/
	INIT_WORK(&cd->suspend_work, goodix_ts_suspend_work);
	INIT_WORK(&cd->resume_work, goodix_ts_resume_work);
/*O6 code for HQ-402304 by liaoxianguo at 2024/07/18 end*/
	/*N6 code for HQ-346992 by zhangzhijian5 at 2023/12/04 start*/
	INIT_WORK(&cd->sleep_gesture_work, goodix_set_sleep_gesture_work);
	/*N6 code for HQ-346992 by zhangzhijian5 at 2023/12/04 end*/
/*N6 code for HQ-334519 by liaoxianguo at 2023/10/17 end*/
/* O6 code for HQ-390162 by liuyupei at 2024/6/28 start */
#if IS_ENABLED(CONFIG_MI_DISP_NOTIFIER)
	cd->fb_notifier.notifier_call = goodix_ts_fb_notifier_callback;
	ret = mi_disp_register_client(&cd->fb_notifier);
	if (ret) {
		ts_err("[FB]Unable to register fb_notifier: %d", ret);
	}
#endif
/* O6 code for HQ-390162 by liuyupei at 2024/6/28 end */

/*O6 code for HQ-391123 by liaoxianguo at 2024/07/10 start*/
	INIT_WORK(&cd->power_supply_work, charger_power_supply_work);
	cd->charger_notifier.notifier_call = charger_status_event_callback;
	if (power_supply_reg_notifier(&cd->charger_notifier))
		ts_err("failed to register charger notifier client");
/*O6 code for HQ-391123 by liaoxianguo at 2024/07/10 end*/

	/*N6 code for HQ-305069 by liaoxianguo at 2023/09/29 start*/
	goodix_ts_get_lockdown_info(cd);
	/*N6 code for HQ-305069 by liaoxianguo at 2023/09/29 end*/

	/* create sysfs files */
	goodix_ts_sysfs_init(cd);

	/* create procfs files */
	goodix_ts_procfs_init(cd);

	/* esd protector */
	goodix_ts_esd_init(cd);

	/* gesture init */
	gesture_module_init();

	/* inspect init */
	inspect_module_init(cd);

	/* Do self check on first boot */
	INIT_WORK(&cd->self_check_work, goodix_self_check);
	schedule_work(&cd->self_check_work);

	return 0;
exit:
	goodix_ts_pen_dev_remove(cd);
err_finger:
	goodix_ts_input_dev_remove(cd);
	return ret;
}

/* try send the config specified with type */
static int goodix_send_ic_config(struct goodix_ts_core *cd, int type)
{
	u32 config_id;
	struct goodix_ic_config *cfg;

	if (type >= GOODIX_MAX_CONFIG_GROUP) {
		ts_err("unsupproted config type %d", type);
		return -EINVAL;
	}

	cfg = cd->ic_configs[type];
	if (!cfg || cfg->len <= 0) {
		ts_info("no valid normal config found");
		return -EINVAL;
	}

	config_id = goodix_get_file_config_id(cfg->data);
	if (cd->ic_info.version.config_id == config_id) {
		ts_info("config id is equal 0x%x, skiped", config_id);
		return 0;
	}

	ts_info("try send config, id=0x%x", config_id);
	return cd->hw_ops->send_config(cd, cfg->data, cfg->len);
}

/**
 * goodix_later_init_thread - init IC fw and config
 * @data: point to goodix_ts_core
 *
 * This function respond for get fw version and try upgrade fw and config.
 * Note: when init encounter error, need release all resource allocated here.
 */
static int goodix_later_init_thread(void *data)
{
	int ret, i;
	int update_flag = UPDATE_MODE_BLOCK | UPDATE_MODE_SRC_REQUEST;
	struct goodix_ts_core *cd = data;
	struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;

	/* step 1: read version */
	ret = hw_ops->read_version(cd, &cd->fw_version);
	if (ret < 0) {
		ts_err("failed to get version info, try to upgrade");
		update_flag |= UPDATE_MODE_FORCE;
	}

	/* step 2: read ic info */
	ret = hw_ops->get_ic_info(cd, &cd->ic_info);
	if (ret < 0) {
/*o6 code for HQFEAT-118700 by p-zhoujun33 at 2025/8/19 start*/
#if IS_ENABLED(CONFIG_MIEV)
		xiaomi_touch_mievent_report_int(TOUCH_EVENT_TRANSFER_ERR, 0, "TpTransferErr", "goodix",ret);
#endif
/*o6 code for HQFEAT-118700 by p-zhoujun33 at 2025/8/19 end*/
		ts_err("failed to get ic info, try to upgrade");
		update_flag |= UPDATE_MODE_FORCE;
	}

	/* step 3: get config data from config bin */
	ret = goodix_get_config_proc(cd);
	if (ret)
		ts_info("no valid ic config found");
	else
		ts_info("success get valid ic config");

	/* step 4: init fw struct add try do fw upgrade */
	ret = goodix_fw_update_init(cd);
	if (ret) {
		ts_err("failed init fw update module");
		goto err_out;
	}

	/* step 5: do upgrade */
	ts_info("update flag: 0x%X", update_flag);
	ret = goodix_do_fw_update(cd->ic_configs[CONFIG_TYPE_NORMAL],
			update_flag);
	if (ret)
		ts_err("failed do fw update");

	print_ic_info(&cd->ic_info);

	/*O6 code for HQ-391834 by liaoxianguo at 2023/06/24 start */
	goodix_ts_hw_info(cd);
	/*O6 code for HQ-391834 by liaoxianguo at 2023/06/24 end */

	/* the recomend way to update ic config is throuth ISP,
	 * if not we will send config with interactive mode
	 */
	goodix_send_ic_config(cd, CONFIG_TYPE_NORMAL);

	/* init other resources */
	ret = goodix_ts_stage2_init(cd);
	if (ret) {
		ts_err("stage2 init failed");
		goto uninit_fw;
	}
	cd->init_stage = CORE_INIT_STAGE2;

	return 0;

uninit_fw:
	goodix_fw_update_uninit();
err_out:
	ts_err("stage2 init failed");
	cd->init_stage = CORE_INIT_FAIL;
	for (i = 0; i < GOODIX_MAX_CONFIG_GROUP; i++) {
		kfree(cd->ic_configs[i]);
		cd->ic_configs[i] = NULL;
	}
	return ret;
}

static int goodix_start_later_init(struct goodix_ts_core *ts_core)
{
	struct task_struct *init_thrd;
	/* create and run update thread */
	init_thrd = kthread_run(goodix_later_init_thread,
				ts_core, "goodix_init_thread");
	if (IS_ERR_OR_NULL(init_thrd)) {
		ts_err("Failed to create update thread:%ld",
		       PTR_ERR(init_thrd));
		return -EFAULT;
	}
	return 0;
}

/*O6 code for HQ-391996 by liuyupei at 2024/7/4 start*/
/*O6 code for HQ-391834 by liaoxianguo at 2023/06/24 start */
int goodix_ts_hw_info(struct goodix_ts_core *core_data)
{
	if (!core_data)
		return -EINVAL;
	if (tp_compatible_flag == TOUCH_SELECT_HUAXING) {
		snprintf(gt_hw_info, sizeof(gt_hw_info),
			"[Vendor]:Huaxing [TP-IC]:GT%s [FW]:%02x%02x%02x%02x [Config]:%x\n",
		core_data->fw_version.patch_pid,
		core_data->fw_version.patch_vid[0], core_data->fw_version.patch_vid[1],
		core_data->fw_version.patch_vid[2], core_data->fw_version.patch_vid[3],
		core_data->ic_info.version.config_version);
	} else if (tp_compatible_flag == TOUCH_SELECT_VISIONOX) {
		snprintf(gt_hw_info, sizeof(gt_hw_info),
			"[Vendor]:Visionox [TP-IC]:GT%s [FW]:%02x%02x%02x%02x [Config]:%x\n",
		core_data->fw_version.patch_pid,
		core_data->fw_version.patch_vid[0], core_data->fw_version.patch_vid[1],
		core_data->fw_version.patch_vid[2], core_data->fw_version.patch_vid[3],
		core_data->ic_info.version.config_version);
	} else {
		ts_err("TP_HW_INFO IS NOT CORRECT!");
	}
	hq_regiser_hw_info(HWID_CTP, gt_hw_info);
	return 0;
}
/*O6 code for HQ-391834 by liaoxianguo at 2023/06/24 end */
/*O6 code for HQ-391996 by liuyupei at 2024/7/4 end*/
/* goodix fb test */
// static void test_suspend(void)
// {
// 	goodix_ts_suspend(goodix_modules.core_data);
// }

// static void test_resume(void)
// {
// 	goodix_ts_resume(goodix_modules.core_data);
// }

/*N6 code for HQ-305074 by dingying at 2023/09/27 start*/
static ssize_t goodix_fw_version_info_read(struct file *file, char __user *buf,
		size_t count, loff_t *pos)
{
	struct goodix_ts_hw_ops *hw_ops = goodix_core_data->hw_ops;
	struct goodix_fw_version chip_ver = {0};
	char k_buf[100] = {0};
	int ret = 0;
	int cnt = -EINVAL;
	ts_info("%s",  __func__);
	if (*pos != 0 || !hw_ops)
		return 0;
	if (hw_ops->read_version) {
		ret = hw_ops->read_version(goodix_core_data, &chip_ver);
		if (!ret) {
			/*O6 code for HQ-391996 by liuyupei at 2024/7/4 start*/
			if (tp_compatible_flag == TOUCH_SELECT_HUAXING) {
				cnt = snprintf(&k_buf[0], sizeof(k_buf), "vendor:huaxing\n");
			} else if (tp_compatible_flag == TOUCH_SELECT_VISIONOX) {
				cnt = snprintf(&k_buf[0], sizeof(k_buf), "vendor:visionox\n");
			} else {
				cnt = snprintf(&k_buf[0], sizeof(k_buf), "vendor:wrong\n");
				ts_err("TP_INFO IS NOT CORRECT!");
			}
			cnt += snprintf(&k_buf[cnt], sizeof(k_buf),
					"patch_pid:%s\n",
					chip_ver.patch_pid);
			cnt += snprintf(&k_buf[cnt], sizeof(k_buf),
					"patch_vid:%02x%02x%02x%02x\n",
					chip_ver.patch_vid[0], chip_ver.patch_vid[1],
					chip_ver.patch_vid[2], chip_ver.patch_vid[3]);
			/*O6 code for HQ-391996 by liuyupei at 2024/7/4 end*/
		}
	}
	if (hw_ops->get_ic_info) {
		ret = hw_ops->get_ic_info(goodix_core_data, &goodix_core_data->ic_info);
		if (!ret) {
			cnt += snprintf(&k_buf[cnt], sizeof(k_buf),
					"config_version:%x\n", goodix_core_data->ic_info.version.config_version);
		}
	}
	cnt = cnt > count ? count : cnt;
	ret = copy_to_user(buf, k_buf, cnt);
	*pos += cnt;
	if (ret != 0)
		return 0;
	else
		return cnt;
}
static const struct proc_ops goodix_fw_version_info_ops = {
	.proc_read = goodix_fw_version_info_read,
};

/*N6 code for HQ-305074 by dingying at 2023/09/27 end*/
/*N6 code for HQ-305069 by liaoxianguo at 2023/09/29 start*/
static ssize_t goodix_lockdown_info_read(struct file *file, char __user *buf,
		size_t count, loff_t *pos)
{
	int cnt = 0, ret = 0;
	char tmp[TP_INFO_MAX_LENGTH] = {0};
	if (*pos != 0 || !goodix_core_data)
		return 0;
	cnt = snprintf(tmp, TP_INFO_MAX_LENGTH,
			"%02x%02x%02x%02x%02x%02x%02x%02x\n",
			goodix_core_data->lockdown_info[0], goodix_core_data->lockdown_info[1],
			goodix_core_data->lockdown_info[2], goodix_core_data->lockdown_info[3],
			goodix_core_data->lockdown_info[4], goodix_core_data->lockdown_info[5],
			goodix_core_data->lockdown_info[6], goodix_core_data->lockdown_info[7]);
	ret = copy_to_user(buf, tmp, cnt);
	*pos += cnt;
	if (ret != 0)
		return 0;
	else
		return cnt;
}
static const struct proc_ops goodix_lockdown_info_ops = {
	.proc_read = goodix_lockdown_info_read,
};
int goodix_ts_get_lockdown_info(struct goodix_ts_core *cd)
{
	int ret = 0;
	struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;
	ret = hw_ops->read(cd, TS_LOCKDOWN_REG,
			cd->lockdown_info, GOODIX_LOCKDOWN_SIZE);
	if (ret) {
		ts_err("can't get lockdown");
		return -EINVAL;
	}
	ts_info("lockdown is:%02x%02x%02x%02x%02x%02x%02x%02x",
			cd->lockdown_info[0], cd->lockdown_info[1],
			cd->lockdown_info[2], cd->lockdown_info[3],
			cd->lockdown_info[4], cd->lockdown_info[5],
			cd->lockdown_info[6], cd->lockdown_info[7]);
	return 0;
}
/*N6 code for HQ-305069 by liaoxianguo at 2023/09/29 end*/

/*N6 code for HQ-343656 by huangshiquan at 2023/11/08 start*/
static int goodix_fod_test_store(int value)
{
	struct input_dev *input_dev = NULL;

	if((!goodix_core_data) || (!goodix_core_data->input_dev)) {
        	ts_err("goodix core data or goodix_core_data->input_dev is NULL!");
        	return -EINVAL;
        }
	input_dev = goodix_core_data->input_dev;

	if (value) {
		input_report_key(input_dev, BTN_INFO, 1);
		update_fod_press_status(1);
		input_sync(input_dev);
		input_mt_slot(input_dev, 0);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 1);
		input_report_key(input_dev, BTN_TOUCH, 1);
		input_report_key(input_dev, BTN_TOOL_FINGER, 1);
		input_report_abs(input_dev, ABS_MT_TRACKING_ID, 0);
		input_report_abs(input_dev, ABS_MT_WIDTH_MINOR, 1);
		input_report_abs(input_dev, ABS_MT_POSITION_X, FOD_TEST_POSITION_X);
		input_report_abs(input_dev, ABS_MT_POSITION_Y, FOD_TEST_POSITION_Y);
		input_sync(input_dev);
	} else {
		input_mt_slot(input_dev, 0);
		input_report_abs(input_dev, ABS_MT_WIDTH_MINOR, 0);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
		input_report_abs(input_dev, ABS_MT_TRACKING_ID, FOD_TEST_TRACKING_INVALID_ID);
		input_report_key(input_dev, BTN_INFO, 0);
		update_fod_press_status(0);
		/* mi_disp_set_fod_queue_work(0, true); */
		input_sync(input_dev);
	}
	return 0;
}
/*N6 code for HQ-343656 by huangshiquan at 2023/11/08 end*/

/*N6 code for HQ-334519 by liaoxianguo at 2023/10/17 start*/
static struct xiaomi_touch_interface xiaomi_touch_interfaces;

/*N6 code for HQ-337468 by zhangzhijian5 at 2023/10/28 start*/
static void goodix_set_gesture_work(struct work_struct *work)
{
	struct goodix_ts_core *core_data =
		container_of(work, struct goodix_ts_core, gesture_work);
	/*O6 code for HQ-402304 by liaoxianguo at 2024/07/18 start*/
	ts_debug("double is 0x%x", core_data->double_wakeup);
	ts_debug("fod is 0x%x", core_data->fod_status);
	ts_debug("aod is 0x%x", core_data->aod_status);
	ts_debug("tybe is 0x%x", core_data->gesture_type);
	if (core_data->fod_status != -1 && core_data->fod_status != 0)
		core_data->gesture_type |= GESTURE_FOD_PRESS;
	else
		core_data->gesture_type &= ~GESTURE_FOD_PRESS;
	/*N6 code for HQ-342824 by zhangzhijian5 at 2023/11/7 start*/
	if ((core_data->aod_status) || (core_data->fod_status != -1 && core_data->fod_status != 0))
		core_data->gesture_type |= GESTURE_SINGLE_TAP;
	else
		core_data->gesture_type &= ~GESTURE_SINGLE_TAP;
	/*N6 code for HQ-342824 by zhangzhijian5 at 2023/11/7 end*/
	if (core_data->double_wakeup){
		core_data->gesture_type |= GESTURE_DOUBLE_TAP;
	} else {
		core_data->gesture_type &= ~GESTURE_DOUBLE_TAP;
	}
	/*O6 code for HQ-402304 by liaoxianguo at 2024/07/18 end*/
	ts_info("set gesture_type:%d", core_data->gesture_type);
	if (core_data->gesture_type != 0)
	goodix_gesture_enable(1);
}
/*N6 code for HQ-337468 by zhangzhijian5 at 2023/10/28 end*/

/*O6 code for HQ-392899 by liaoxianguo at 2024/07/11 start*/
static void goodix_set_game_work(struct work_struct *work)
{
	struct goodix_ts_hw_ops *hw_ops;
	u8 data0 = 0;
	u8 data1 = 0;

	static bool game_edge_update_falg = false;
	u8 temp_value = 0;
	int ret = 0;
	int i = 0;
	bool update = false;
	static bool expert_mode = false;
	int edge_filter_corner_size = 180;/*case1 default is 170*/

	if (!goodix_core_data || !goodix_core_data->hw_ops) {
		ts_err("core_data or hw_ops is NULL");
		return;
	}

	hw_ops = goodix_core_data->hw_ops;

	if (goodix_core_data->work_status != TP_NORMAL) {
		ts_info("suspended or gesture, skip");
		return;
	}
	mutex_lock(&goodix_core_data->core_mutex);
	for (i = 0; i <= Touch_Panel_Orientation; i++) {
		if (xiaomi_touch_interfaces.touch_mode[i][GET_CUR_VALUE] !=
			xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE]) {
			update = true;
			if (Touch_Expert_Mode == i) {
				expert_mode = true;
				ts_info("expert mode set");
			}
			else if ((i == Touch_Tolerance)
				|| (i == Touch_UP_THRESHOLD)
				|| (i == Touch_Aim_Sensitivity)
				|| (i == Touch_Tap_Stability))
				expert_mode = false;
			if (((i == Touch_Game_Mode) && (goodix_core_data->gamemode_enabled) )
				|| (i == Touch_Panel_Orientation)
				|| (i == Touch_Edge_Filter)) {
				game_edge_update_falg = true;
				ts_info("edge data may be updated because %d", i);
			}
			xiaomi_touch_interfaces.touch_mode[i][GET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE];
		}
	}
	if (!update) {
		ts_info("no need update mode value");
		mutex_unlock(&goodix_core_data->core_mutex);
		return;
	}
	ts_info("enter set_game_work in core.c");
	for (i = 0; i <= Touch_Panel_Orientation; i++) {
		switch (i) {
		case Touch_Game_Mode:
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][SET_CUR_VALUE];
			break;
		case Touch_Active_MODE:
			break;
		case Touch_UP_THRESHOLD:
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][SET_CUR_VALUE];
			data0 &= 0xF8;
			data0 |= temp_value;
			break;
		case Touch_Tolerance:
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][SET_CUR_VALUE];
			data0 &= 0xC7;
			data0 |= (temp_value << 3);
			break;
		case Touch_Panel_Orientation:
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][SET_CUR_VALUE];
			if (temp_value == PANEL_ORIENTATION_DEGREE_90)
				temp_value = 1;
			else if (temp_value == PANEL_ORIENTATION_DEGREE_270)
				temp_value = 2;
			else
				temp_value = 0;
			data0 &= 0x3F;
			data0 |= (temp_value << 6);
			break;
		case Touch_Aim_Sensitivity:
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][SET_CUR_VALUE];
			data1 &= 0xC7;
			data1 |= (temp_value << 3);
			break;
		case Touch_Tap_Stability:
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][SET_CUR_VALUE];
			data1 &= 0xF8;
			data1 |= temp_value;
			break;
		case Touch_Edge_Filter:
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][SET_CUR_VALUE];
			/* set edge_filter_corner_size */
			if(0 == temp_value){
				edge_filter_corner_size = GAME_CORNER_SUPPRESSION_NONE;
			}else if(1 == temp_value){
				edge_filter_corner_size = GAME_CORNER_SUPPRESSION_SMALL;
			}else if(2 == temp_value){
				edge_filter_corner_size = GAME_CORNER_SUPPRESSION_MEDIUM;
			}else if(3 == temp_value){
				edge_filter_corner_size = GAME_CORNER_SUPPRESSION_LARGE;
			}
			data1 &= 0x3F;
			data1 |= (temp_value << 6);
			break;
		case Touch_Expert_Mode:
			temp_value = xiaomi_touch_interfaces.touch_mode[Touch_Expert_Mode][SET_CUR_VALUE];
			temp_value = temp_value - 1;
			if (expert_mode) {
				data0 &= 0xF8;
				data0 |= (u8)goodix_core_data->board_data.touch_expert_array[temp_value * GAME_ARRAY_LEN + 1];
				data0 &= 0xC7;
				data0 |= (u8)(goodix_core_data->board_data.touch_expert_array[temp_value * GAME_ARRAY_LEN] << 3);
				data1 &= 0xC7;
				data1 |= (u8)(goodix_core_data->board_data.touch_expert_array[temp_value * GAME_ARRAY_LEN + 2] << 3);
				data1 &= 0xF8;
				data1 |= (u8)goodix_core_data->board_data.touch_expert_array[temp_value * GAME_ARRAY_LEN + 3];
			}
			break;
		default:
			/* Don't support */
			break;
		};
	}
	if (false == atomic_read(&goodix_core_data->suspended)) {
		if (goodix_core_data->gamemode_enabled) {
			if (game_edge_update_falg) {
				/* Computing and Deliver Game Mode Edge Suppression Parameters */
				/*ts_info("Update edge suppression data[] in game mode");*/
				goodix_set_edge_filter_game(edge_filter_corner_size);
			}
				ret = hw_ops->game(goodix_core_data, data0, data1);
				game_edge_update_falg = false;
				if (ret < 0) {
					ts_err("send game mode fail");
				}
		} else {
			ts_info("Game mode off, do not update edge suppression data in gamemode, gamemode_enabled = %d", goodix_core_data->gamemode_enabled);
		}
	} else {
		ts_info("Touch suspended, do not update game edge suppression parmas");
	}
	mutex_unlock(&goodix_core_data->core_mutex);
	return;
}
/*O6 code for HQ-392899 by liaoxianguo at 2024/07/11 start*/

static int goodix_set_cur_value(int gtp_mode, int gtp_value)
{
	int ret = 0;
/*O6 code for HQ-392899 by liaoxianguo at 2024/07/11 start*/
	struct goodix_ts_core *cd;
	if (!goodix_core_data) {
		ts_err("core_data is NULL");
		return -EINVAL;
	}
	cd = goodix_core_data;
	cd->gtp_mode = gtp_mode;
	cd->gtp_value = gtp_value;
	if(cd->gtp_mode == Touch_Panel_Orientation){
	    cd->gtp_direction_value = gtp_value;
	}
    /*解决屏幕方向在设置双击唤醒AOD等功能后灭屏再亮屏下发不对的问题*/
/*O6 code for HQ-392899 by liaoxianguo at 2024/07/11 end*/
	ts_info("mode:%d, value:%d", gtp_mode, gtp_value);
	if (!goodix_core_data || goodix_core_data->init_stage != CORE_INIT_STAGE2) {
		ts_err("initialization not completed, return");
		return 0;
	}
	/*O6 code for HQ-402304 by liaoxianguo at 2024/07/18 start*/
	if (gtp_mode == Touch_Doubletap_Mode && goodix_core_data && gtp_value >= 0) {
		goodix_core_data->double_wakeup = gtp_value;
		queue_work(goodix_core_data->gesture_wq, &goodix_core_data->gesture_work);
		return 0;
	}
	/*if (gtp_mode == Touch_Power_Status && goodix_core_data && gtp_value >= 0){
		goodix_core_data->power_status = gtp_value;
		flush_workqueue(goodix_core_data->event_wq);
		ts_info("%s: set power status:%d\n", __func__, gtp_value);
		if (goodix_core_data->power_status) {
			queue_work(goodix_core_data->event_wq, &goodix_core_data->resume_work);
		} else if (!goodix_core_data->power_status) {
			queue_work(goodix_core_data->event_wq, &goodix_core_data->suspend_work);
		}
		return 0;
	}*/
	/*O6 code for HQ-402304 by liaoxianguo at 2024/07/18 end*/
	if (gtp_mode ==  Touch_Fod_Enable && goodix_core_data && gtp_value >= 0) {
		goodix_core_data->fod_status = gtp_value;
		ts_info("Touch_Fod_Enable value [%d]\n", gtp_value);
		queue_work(goodix_core_data->gesture_wq, &goodix_core_data->gesture_work);
		/*N6 code for HQ-346992 by zhangzhijian5 at 2023/12/04 start*/
		if ((goodix_core_data->fod_status != -1 && goodix_core_data->fod_status != 0) &&
				atomic_read(&goodix_core_data->suspended)) {
			flush_workqueue(goodix_core_data->gesture_wq);
			flush_workqueue(goodix_core_data->event_wq);
			queue_work(goodix_core_data->event_wq,
				&goodix_core_data->sleep_gesture_work);
		}
		/*N6 code for HQ-346992 by zhangzhijian5 at 2023/12/04 end*/
		return 0;
	}

	if (gtp_mode == Touch_FodIcon_Enable && goodix_core_data && gtp_value >= 0) {
		goodix_core_data->fod_icon_status = gtp_value;
		ts_info("Touch_FodIcon_Enable value [%d]\n", gtp_value);
		queue_work(goodix_core_data->gesture_wq, &goodix_core_data->gesture_work);
		return 0;
	}

	/*N6 code for HQ-337468 by zhangzhijian5 at 2023/10/28 start*/
	if (gtp_mode == Touch_Aod_Enable && goodix_core_data && gtp_value >= 0) {
		goodix_core_data->aod_status = gtp_value;
		queue_work(goodix_core_data->gesture_wq, &goodix_core_data->gesture_work);
		return 0;
	}
	/*N6 code for HQ-337468 by zhangzhijian5 at 2023/10/28 end*/

	/*N6 code for HQ-305061 by zhangzhijian5 at 2023/10/30 start*/
	if (gtp_mode >= Touch_Mode_NUM) {
		ts_err("gtp mode is error:%d", gtp_mode);
		return -EINVAL;
	}

	xiaomi_touch_interfaces.touch_mode[gtp_mode][SET_CUR_VALUE] = gtp_value;
	/*N6 code for HQ-305061 by zhangzhijian5 at 2023/10/30 end*/
	if (xiaomi_touch_interfaces.touch_mode[gtp_mode][SET_CUR_VALUE] >
			xiaomi_touch_interfaces.touch_mode[gtp_mode][GET_MAX_VALUE]) {
		xiaomi_touch_interfaces.touch_mode[gtp_mode][SET_CUR_VALUE] =
			xiaomi_touch_interfaces.touch_mode[gtp_mode][GET_MAX_VALUE];
	} else if (xiaomi_touch_interfaces.touch_mode[gtp_mode][SET_CUR_VALUE] <
			xiaomi_touch_interfaces.touch_mode[gtp_mode][GET_MIN_VALUE]) {
		xiaomi_touch_interfaces.touch_mode[gtp_mode][SET_CUR_VALUE] =
			xiaomi_touch_interfaces.touch_mode[gtp_mode][GET_MIN_VALUE];
	}

/*O6 code for HQ-392899 by liaoxianguo at 2024/07/11 start*/
	if (gtp_mode <= Touch_Panel_Orientation) {/*power state no need call game work*/
		if (gtp_mode ==  Touch_Game_Mode && goodix_core_data && gtp_value >= 0) {
			ts_info("Touch_Game_Mode value [%d]\n",gtp_value );
			goodix_core_data->gamemode_enabled = gtp_value > 0 ? true : false;
		}
		queue_work(goodix_core_data->game_wq, &goodix_core_data->game_work);
	} else {
		xiaomi_touch_interfaces.touch_mode[gtp_mode][GET_CUR_VALUE] =
			xiaomi_touch_interfaces.touch_mode[gtp_mode][SET_CUR_VALUE];
	}
	return ret;
}

static int goodix_set_mode_long_value(int mode, int len, int *buf)
{
	int i = 0;
	struct goodix_ts_core *cd;
	if (!goodix_core_data || !buf) {
		ts_err("core_data or buf is NULL");
		return -EINVAL;
	}
	cd = goodix_core_data;
	if (len <= 0)
		return -EIO;
	ts_info("enter set_mode_long : %s, mode: %d, len: %d\n",__func__, mode, len);
	if (!goodix_core_data || goodix_core_data->init_stage != CORE_INIT_STAGE2) {
		ts_err("initialization not completed, return");
		return 0;
	}
	mutex_lock(&goodix_modules.mutex);
	xiaomi_touch_interfaces.long_mode_len = len;
	for (i = 0; i < len; i++) {
		xiaomi_touch_interfaces.long_mode_value[i] = buf[i];
	}
#ifdef GRIP_MODE_DEBUG
	for (i = 0; i < len; i = i + 8) {
		ts_info("long_mode_value[0~7] = %d, %d, %d, %d, %d, %d, %d, %d\n",
				xiaomi_touch_interfaces.long_mode_value[i], xiaomi_touch_interfaces.long_mode_value[i + 1], xiaomi_touch_interfaces.long_mode_value[i + 2],
				xiaomi_touch_interfaces.long_mode_value[i + 3], xiaomi_touch_interfaces.long_mode_value[i + 4], xiaomi_touch_interfaces.long_mode_value[i + 5],
				xiaomi_touch_interfaces.long_mode_value[i + 6], xiaomi_touch_interfaces.long_mode_value[i + 7]);
	}
#endif
	mutex_unlock(&goodix_modules.mutex);
	if (mode == Touch_Grip_Mode) {
		if(true == atomic_read(&cd->suspended)){
			msleep(35);
			/* If suspended, shield the upper layer to set the normal mode edge suppression function */
			if(false == atomic_read(&cd->suspended))
				goto normal_send;
			else
				ts_info("Touch suspended, do not update edge suppression parmas");
				goto out;
		}
		else
			goto normal_send;
	}
	else
		goto out;
normal_send:
	if (cd->gamemode_enabled) {
		ts_info("%s in gamemode, can't write parameters to touch ic\n",__func__);
		goto out;
	} else {
		if(cd->work_status != TP_NORMAL)
		msleep(55);
		/*ts_info("send normal edge data in set_mode_long_value");*/
		goodix_set_edge_filter_normal();
		brl_Edge_suppression(cd);
	}
out:
	return 0;
}

void goodix_set_grip_filter(int *source, int *sum)
{
	struct goodix_ts_core *cd;
	int *buf;
	int i = 0, type = 0, pos = 0, x_start = 0, y_start = 0, x_end = 0, y_end = 0;
	int sum_type_pos = 0;
	/*for grip mode, the format from framework is :
	* len:the num of the commond, rect_num * parameters_num_for_each_rect
	 * type:dead grip, or edge grip or cornero grip
	 * pos: which corner or which edge
	 * sum_type_pos: set type to high pos to low
	 * x start
	 * y start
	 * x end
	 * y end
	 * time
	 * sum: the sum of below
	 * node num
	  */
	if (!goodix_core_data || !source || !sum) {
		ts_err("grip filter data invalid pointer detected");
		return;
	}
	cd = goodix_core_data;
	cd->edge_data.Length[0] = EDGE_DATA_LENGTH;
	buf= source;
	*sum = 0;

	for (i = 0; i < 4; i++) {
		buf = source + GTP_PARAMETER_NUM * i;
		type = *buf;
		pos = *(buf + 1);
		sum_type_pos = type << 8 | pos;
		x_start = *(buf + 2);
		y_start = *(buf + 3);
		x_end = *(buf + 4);
		y_end = *(buf + 5);
		ts_info("grip_type: %d, grip_pos: %d, x_start: %d, y_start: %d, x_end: %d, y_end: %d\n",
		type, pos, x_start, y_start, x_end, y_end);
		if (type == DEAD_ZONE_TYPE) {
			/* Calculate data based on dead zone suppression parameters, i:pos*/
			if (i == 0) {
				cd->edge_data.Top_DeadArea_MinX[0] = (x_start & 0xff);
				cd->edge_data.Top_DeadArea_MinX[1] = ((x_start >> 8) & 0xff);
				cd->edge_data.Top_DeadArea_MinY[0] = (y_start & 0xff);
				cd->edge_data.Top_DeadArea_MinY[1] = ((y_start >> 8) & 0xff);
				cd->edge_data.Top_DeadArea_MaxX[0] = (x_end & 0xff);
				cd->edge_data.Top_DeadArea_MaxX[1] = ((x_end >> 8) & 0xff);
				cd->edge_data.Top_DeadArea_MaxY[0] = (y_end & 0xff);
				cd->edge_data.Top_DeadArea_MaxY[1] = ((y_end >> 8) & 0xff);
			} else if(i == 1) {
				cd->edge_data.Bot_DeadArea_MinX[0] = (x_start & 0xff);
				cd->edge_data.Bot_DeadArea_MinX[1] = ((x_start >> 8) & 0xff);
				cd->edge_data.Bot_DeadArea_MinY[0] = (y_start & 0xff);
				cd->edge_data.Bot_DeadArea_MinY[1] = ((y_start >> 8) & 0xff);
				cd->edge_data.Bot_DeadArea_MaxX[0] = (x_end & 0xff);
				cd->edge_data.Bot_DeadArea_MaxX[1] = ((x_end >> 8) & 0xff);
				cd->edge_data.Bot_DeadArea_MaxY[0] = (y_end & 0xff);
				cd->edge_data.Bot_DeadArea_MaxY[1] = ((y_end >> 8) & 0xff);
			} else if(i == 2) {
				cd->edge_data.Left_DeadArea_MinX[0] = (x_start & 0xff);
				cd->edge_data.Left_DeadArea_MinX[1] = ((x_start >> 8) & 0xff);
				cd->edge_data.Left_DeadArea_MinY[0] = (y_start & 0xff);
				cd->edge_data.Left_DeadArea_MinY[1] = ((y_start >> 8) & 0xff);
				cd->edge_data.Left_DeadArea_MaxX[0] = (x_end & 0xff);
				cd->edge_data.Left_DeadArea_MaxX[1] = ((x_end >> 8) & 0xff);
				cd->edge_data.Left_DeadArea_MaxY[0] = (y_end & 0xff);
				cd->edge_data.Left_DeadArea_MaxY[1] = ((y_end >> 8) & 0xff);
			} else if(i == 3) {
				cd->edge_data.Right_DeadArea_MinX[0] = (x_start & 0xff);
				cd->edge_data.Right_DeadArea_MinX[1] = ((x_start >> 8) & 0xff);
				cd->edge_data.Right_DeadArea_MinY[0] = (y_start & 0xff);
				cd->edge_data.Right_DeadArea_MinY[1] = ((y_start >> 8) & 0xff);
				cd->edge_data.Right_DeadArea_MaxX[0] = (x_end & 0xff);
				cd->edge_data.Right_DeadArea_MaxX[1] = ((x_end >> 8) & 0xff);
				cd->edge_data.Right_DeadArea_MaxY[0] = (y_end & 0xff);
				cd->edge_data.Right_DeadArea_MaxY[1] = ((y_end >> 8) & 0xff);
			}
		} else if (type == EDGE_ZONE_TYPE) {
			/* Calculate data based on edge zone suppression parameters, i:pos*/
			if (i == 0) {
				cd->edge_data.Top_ClickArea_MaxX[0] = (x_start & 0xff);
				cd->edge_data.Top_ClickArea_MinX[1] = ((x_start >> 8) & 0xff);
				cd->edge_data.Top_ClickArea_MinY[0] = (y_start & 0xff);
				cd->edge_data.Top_ClickArea_MinY[1] = ((y_start >> 8) & 0xff);
				cd->edge_data.Top_ClickArea_MaxX[0] = (x_end & 0xff);
				cd->edge_data.Top_ClickArea_MaxX[1] = ((x_end >> 8) & 0xff);
				cd->edge_data.Top_ClickArea_MaxY[0] = (y_end & 0xff);
				cd->edge_data.Top_ClickArea_MaxY[1] = ((y_end >> 8) & 0xff);
			} else if(i == 1) {
				cd->edge_data.Bot_ClickArea_MinX[0] = (x_start & 0xff);
				cd->edge_data.Bot_ClickArea_MinX[1] = ((x_start >> 8) & 0xff);
				cd->edge_data.Bot_ClickArea_MinY[0] = (y_start & 0xff);
				cd->edge_data.Bot_ClickArea_MinY[1] = ((y_start >> 8) & 0xff);
				cd->edge_data.Bot_ClickArea_MaxX[0] = (x_end & 0xff);
				cd->edge_data.Bot_ClickArea_MaxX[1] = ((x_end >> 8) & 0xff);
				cd->edge_data.Bot_ClickArea_MaxY[0] = (y_end & 0xff);
				cd->edge_data.Bot_ClickArea_MaxY[1] = ((y_end >> 8) & 0xff);
			} else if(i == 2) {
				cd->edge_data.Left_ClickArea_MinX[0] = (x_start & 0xff);
				cd->edge_data.Left_ClickArea_MinX[1] = ((x_start >> 8) & 0xff);
				cd->edge_data.Left_ClickArea_MinY[0] = (y_start & 0xff);
				cd->edge_data.Left_ClickArea_MinY[1] = ((y_start >> 8) & 0xff);
				cd->edge_data.Left_ClickArea_MaxX[0] = (x_end & 0xff);
				cd->edge_data.Left_ClickArea_MaxX[1] = ((x_end >> 8) & 0xff);
				cd->edge_data.Left_ClickArea_MaxY[0] = (y_end & 0xff);
				cd->edge_data.Left_ClickArea_MaxY[1] = ((y_end >> 8) & 0xff);
			} else if(i == 3) {
				cd->edge_data.Right_ClickArea_MinX[0] = (x_start & 0xff);
				cd->edge_data.Right_ClickArea_MinX[1] = ((x_start >> 8) & 0xff);
				cd->edge_data.Right_ClickArea_MinY[0] = (y_start & 0xff);
				cd->edge_data.Right_ClickArea_MinY[1] = ((y_start >> 8) & 0xff);
				cd->edge_data.Right_ClickArea_MaxX[0] = (x_end & 0xff);
				cd->edge_data.Right_ClickArea_MaxX[1] = ((x_end >> 8) & 0xff);
				cd->edge_data.Right_ClickArea_MaxY[0] = (y_end & 0xff);
				cd->edge_data.Right_ClickArea_MaxY[1] = ((y_end >> 8) & 0xff);
			}
		} else if (type == CORNER_ZONE_TYPE && cornerzone == 1) {
			/* Calculate data based on corner zone suppression case1 parameters, i:pos*/
			if (i == 0) {
				cd->edge_data.Top_CornerArea_MaxX[0] = (x_start & 0xff);
				cd->edge_data.Top_CornerArea_MinX[1] = ((x_start >> 8) & 0xff);
				cd->edge_data.Top_CornerArea_MinY[0] = (y_start & 0xff);
				cd->edge_data.Top_CornerArea_MinY[1] = ((y_start >> 8) & 0xff);
				cd->edge_data.Top_CornerArea_MaxX[0] = (x_end & 0xff);
				cd->edge_data.Top_CornerArea_MaxX[1] = ((x_end >> 8) & 0xff);
				cd->edge_data.Top_CornerArea_MaxY[0] = (y_end & 0xff);
				cd->edge_data.Top_CornerArea_MaxY[1] = ((y_end >> 8) & 0xff);
			} else if(i == 1) {
				cd->edge_data.Bot_CornerArea_MinX[0] = (x_start & 0xff);
				cd->edge_data.Bot_CornerArea_MinX[1] = ((x_start >> 8) & 0xff);
				cd->edge_data.Bot_CornerArea_MinY[0] = (y_start & 0xff);
				cd->edge_data.Bot_CornerArea_MinY[1] = ((y_start >> 8) & 0xff);
				cd->edge_data.Bot_CornerArea_MaxX[0] = (x_end & 0xff);
				cd->edge_data.Bot_CornerArea_MaxX[1] = ((x_end >> 8) & 0xff);
				cd->edge_data.Bot_CornerArea_MaxY[0] = (y_end & 0xff);
				cd->edge_data.Bot_CornerArea_MaxY[1] = ((y_end >> 8) & 0xff);
			} else if(i == 2) {
				cd->edge_data.Left_CornerArea_MinX[0] = (x_start & 0xff);
				cd->edge_data.Left_CornerArea_MinX[1] = ((x_start >> 8) & 0xff);
				cd->edge_data.Left_CornerArea_MinY[0] = (y_start & 0xff);
				cd->edge_data.Left_CornerArea_MinY[1] = ((y_start >> 8) & 0xff);
				cd->edge_data.Left_CornerArea_MaxX[0] = (x_end & 0xff);
				cd->edge_data.Left_CornerArea_MaxX[1] = ((x_end >> 8) & 0xff);
				cd->edge_data.Left_CornerArea_MaxY[0] = (y_end & 0xff);
				cd->edge_data.Left_CornerArea_MaxY[1] = ((y_end >> 8) & 0xff);
			} else if(i == 3) {
				cd->edge_data.Right_CornerArea_MinX[0] = (x_start & 0xff);
				cd->edge_data.Right_CornerArea_MinX[1] = ((x_start >> 8) & 0xff);
				cd->edge_data.Right_CornerArea_MinY[0] = (y_start & 0xff);
				cd->edge_data.Right_CornerArea_MinY[1] = ((y_start >> 8) & 0xff);
				cd->edge_data.Right_CornerArea_MaxX[0] = (x_end & 0xff);
				cd->edge_data.Right_CornerArea_MaxX[1] = ((x_end >> 8) & 0xff);
				cd->edge_data.Right_CornerArea_MaxY[0] = (y_end & 0xff);
				cd->edge_data.Right_CornerArea_MaxY[1] = ((y_end >> 8) & 0xff);
			}
		} else if (type == CORNER_ZONE_TYPE && cornerzone == 2) {
			/* Calculate data based on corner zone suppression case2 parameters, i:pos*/
			if (i == 0) {
				cd->edge_data.Top_GameCornerArea_MaxX[0] = (x_start & 0xff);
				cd->edge_data.Top_GameCornerArea_MinX[1] = ((x_start >> 8) & 0xff);
				cd->edge_data.Top_GameCornerArea_MinY[0] = (y_start & 0xff);
				cd->edge_data.Top_GameCornerArea_MinY[1] = ((y_start >> 8) & 0xff);
				cd->edge_data.Top_GameCornerArea_MaxX[0] = (x_end & 0xff);
				cd->edge_data.Top_GameCornerArea_MaxX[1] = ((x_end >> 8) & 0xff);
				cd->edge_data.Top_GameCornerArea_MaxY[0] = (y_end & 0xff);
				cd->edge_data.Top_GameCornerArea_MaxY[1] = ((y_end >> 8) & 0xff);
			} else if(i == 1) {
				cd->edge_data.Bot_GameCornerArea_MinX[0] = (x_start & 0xff);
				cd->edge_data.Bot_GameCornerArea_MinX[1] = ((x_start >> 8) & 0xff);
				cd->edge_data.Bot_GameCornerArea_MinY[0] = (y_start & 0xff);
				cd->edge_data.Bot_GameCornerArea_MinY[1] = ((y_start >> 8) & 0xff);
				cd->edge_data.Bot_GameCornerArea_MaxX[0] = (x_end & 0xff);
				cd->edge_data.Bot_GameCornerArea_MaxX[1] = ((x_end >> 8) & 0xff);
				cd->edge_data.Bot_GameCornerArea_MaxY[0] = (y_end & 0xff);
				cd->edge_data.Bot_GameCornerArea_MaxY[1] = ((y_end >> 8) & 0xff);
			} else if(i == 2) {
				cd->edge_data.Left_GameCornerArea_MinX[0] = (x_start & 0xff);
				cd->edge_data.Left_GameCornerArea_MinX[1] = ((x_start >> 8) & 0xff);
				cd->edge_data.Left_GameCornerArea_MinY[0] = (y_start & 0xff);
				cd->edge_data.Left_GameCornerArea_MinY[1] = ((y_start >> 8) & 0xff);
				cd->edge_data.Left_GameCornerArea_MaxX[0] = (x_end & 0xff);
				cd->edge_data.Left_GameCornerArea_MaxX[1] = ((x_end >> 8) & 0xff);
				cd->edge_data.Left_GameCornerArea_MaxY[0] = (y_end & 0xff);
				cd->edge_data.Left_GameCornerArea_MaxY[1] = ((y_end >> 8) & 0xff);
			} else if(i == 3) {
				cd->edge_data.Right_GameCornerArea_MinX[0] = (x_start & 0xff);
				cd->edge_data.Right_GameCornerArea_MinX[1] = ((x_start >> 8) & 0xff);
				cd->edge_data.Right_GameCornerArea_MinY[0] = (y_start & 0xff);
				cd->edge_data.Right_GameCornerArea_MinY[1] = ((y_start >> 8) & 0xff);
				cd->edge_data.Right_GameCornerArea_MaxX[0] = (x_end & 0xff);
				cd->edge_data.Right_GameCornerArea_MaxX[1] = ((x_end >> 8) & 0xff);
				cd->edge_data.Right_GameCornerArea_MaxY[0] = (y_end & 0xff);
				cd->edge_data.Right_GameCornerArea_MaxY[1] = ((y_end >> 8) & 0xff);
			}
		}
		*sum += sum_type_pos + x_start + y_start + x_end + y_end;
	}
	cornerzone = 0;
	/*ts_info("sum = %d",*sum);*/
}
/**
* @description:	Game mode edge suppression parameters & data calculation,
*				Tips:Edge suppression parameter will not be affected by the super-resolution feature
* @param edge_filter_corner_size -	GAME_CORNER_SUPPRESSION_NONE	0
*									GAME_CORNER_SUPPRESSION_SMALL 	100
*									GAME_CORNER_SUPPRESSION_MEDIUM 	170
*									GAME_CORNER_SUPPRESSION_LARGE 	250
*/
void goodix_set_edge_filter_game(int edge_filter_corner_size)
{
	int sum_corner = 0, sum_cornergame = 0, sum_edge = 0, sum_dead = 0;
	/* PANEL_ORIENTATION_DEGREE_0 PANEL_ORIENTATION_DEGREE_90 PANEL_ORIENTATION_DEGREE_180 PANEL_ORIENTATION_DEGREE_270 */
	int direction = xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_CUR_VALUE];
	int deadzone_filter[4 * GTP_PARAMETER_NUM] 	 = {0};
	int edgezone_filter[4 * GTP_PARAMETER_NUM] 	 = {0};
	int cornerzone_filter[4 * GTP_PARAMETER_NUM] = {0};
	int i = 0;
	ts_info("deadzone");
	/* zero */
	for (i = 0; i < 4; i ++) {/* grip_type & grip_pos */
		deadzone_filter[i * GTP_PARAMETER_NUM] = DEAD_ZONE_TYPE;
		deadzone_filter[1 + i * GTP_PARAMETER_NUM] = i;
	}
	goodix_set_grip_filter((int*)&(deadzone_filter[0]), &sum_dead);
	ts_info("edgezone");
	for (i = 0; i < 4; i ++) {/* grip_type & grip_pos */
		edgezone_filter[i * GTP_PARAMETER_NUM] = EDGE_ZONE_TYPE;
		edgezone_filter[1 + i * GTP_PARAMETER_NUM] = i;
	}
	if ((PANEL_ORIENTATION_DEGREE_90 == direction) || (PANEL_ORIENTATION_DEGREE_270 == direction)) {
		ts_info("direction: %d, edge long side: %d, edge short side: %d",
					direction, GAME_EDGE_SUPPRESSION_LONGSIDE_90_270, GAME_EDGE_SUPPRESSION_SHORTSIDE_90_270);
		/* pos 0 Top*/
		edgezone_filter[2] = 0;
		edgezone_filter[3] = 0;
		edgezone_filter[4] = PANEL_MAX_X;
		edgezone_filter[5] = GAME_EDGE_SUPPRESSION_SHORTSIDE_90_270;
		/* pos 1 Bottom*/
		edgezone_filter[2 + 1 * GTP_PARAMETER_NUM] = 0;
		edgezone_filter[3 + 1 * GTP_PARAMETER_NUM] = PANEL_MAX_Y - GAME_EDGE_SUPPRESSION_SHORTSIDE_90_270;
		edgezone_filter[4 + 1 * GTP_PARAMETER_NUM] = PANEL_MAX_X;
		edgezone_filter[5 + 1 * GTP_PARAMETER_NUM] = PANEL_MAX_Y;
		/* pos 2 Left*/
		edgezone_filter[2 + 2 * GTP_PARAMETER_NUM] = 0;
		edgezone_filter[3 + 2 * GTP_PARAMETER_NUM] = 0;
		edgezone_filter[4 + 2 * GTP_PARAMETER_NUM] = GAME_EDGE_SUPPRESSION_LONGSIDE_90_270;
		edgezone_filter[5 + 2 * GTP_PARAMETER_NUM] = PANEL_MAX_Y;
		/* pos 3 Right*/
		edgezone_filter[2 + 3 * GTP_PARAMETER_NUM] = PANEL_MAX_X - GAME_EDGE_SUPPRESSION_LONGSIDE_90_270;
		edgezone_filter[3 + 3 * GTP_PARAMETER_NUM] = 0;
		edgezone_filter[4 + 3 * GTP_PARAMETER_NUM] = PANEL_MAX_X;
		edgezone_filter[5 + 3 * GTP_PARAMETER_NUM] = PANEL_MAX_Y;
	} else if ((PANEL_ORIENTATION_DEGREE_0 == direction) || (PANEL_ORIENTATION_DEGREE_180 == direction)) {
		ts_info("direction: %d, edge long side: %d, edge short side: %d",
					direction, GAME_EDGE_SUPPRESSION_LONGSIDE_0_180, GAME_EDGE_SUPPRESSION_SHORTSIDE_0_180);
		/* pos 0 Top*/
		edgezone_filter[2] = 0;
		edgezone_filter[3] = 0;
		edgezone_filter[4] = PANEL_MAX_X;
		edgezone_filter[5] = GAME_EDGE_SUPPRESSION_SHORTSIDE_0_180;
		/* pos 1 Bottom*/
		edgezone_filter[2 + 1 * GTP_PARAMETER_NUM] = 0;
		edgezone_filter[3 + 1 * GTP_PARAMETER_NUM] = PANEL_MAX_Y - GAME_EDGE_SUPPRESSION_SHORTSIDE_0_180;
		edgezone_filter[4 + 1 * GTP_PARAMETER_NUM] = PANEL_MAX_X;
		edgezone_filter[5 + 1 * GTP_PARAMETER_NUM] = PANEL_MAX_Y;
		/* pos 2 Left*/
		edgezone_filter[2 + 2 * GTP_PARAMETER_NUM] = 0;
		edgezone_filter[3 + 2 * GTP_PARAMETER_NUM] = 0;
		edgezone_filter[4 + 2 * GTP_PARAMETER_NUM] = GAME_EDGE_SUPPRESSION_LONGSIDE_0_180;
		edgezone_filter[5 + 2 * GTP_PARAMETER_NUM] = PANEL_MAX_Y;
		/* pos 3 Right*/
		edgezone_filter[2 + 3 * GTP_PARAMETER_NUM] = PANEL_MAX_X - GAME_EDGE_SUPPRESSION_LONGSIDE_0_180;
		edgezone_filter[3 + 3 * GTP_PARAMETER_NUM] = 0;
		edgezone_filter[4 + 3 * GTP_PARAMETER_NUM] = PANEL_MAX_X;
		edgezone_filter[5 + 3 * GTP_PARAMETER_NUM] = PANEL_MAX_Y;
	}
	goodix_set_grip_filter((int*)&(edgezone_filter[0]), &sum_edge);
	ts_info("cornerzone case 1");
	cornerzone = 1;
	/*memset(cornerzone_filter,0,sizeof(cornerzone_filter));*/
	for (i = 0; i < 4; i ++) {/* grip_type & grip_pos */
		cornerzone_filter[i * GTP_PARAMETER_NUM] = CORNER_ZONE_TYPE;
		cornerzone_filter[1 + i * GTP_PARAMETER_NUM] = i;
	}
	if ((edge_filter_corner_size != 0) || (PANEL_ORIENTATION_DEGREE_0 == direction) || (PANEL_ORIENTATION_DEGREE_180 == direction)) {
		if (PANEL_ORIENTATION_DEGREE_90 == direction) {
			ts_info("direction: %d, edge_filter_corner_size: %d", direction, edge_filter_corner_size);
			/* pos 0 */
			cornerzone_filter[2] = 0;
			cornerzone_filter[3] = 0;
			cornerzone_filter[4] = edge_filter_corner_size;
			cornerzone_filter[5] = edge_filter_corner_size;
			/* pos 2 */
			cornerzone_filter[2 + 2 * GTP_PARAMETER_NUM] = 0;
			cornerzone_filter[3 + 2 * GTP_PARAMETER_NUM] = PANEL_MAX_Y - edge_filter_corner_size;
			cornerzone_filter[4 + 2 * GTP_PARAMETER_NUM] = edge_filter_corner_size;
			cornerzone_filter[5 + 2 * GTP_PARAMETER_NUM] = PANEL_MAX_Y;
		} else if(PANEL_ORIENTATION_DEGREE_270 == direction) {
			ts_info("direction: %d, edge_filter_corner_size: %d", direction, edge_filter_corner_size);
			/* pos 1 */
			cornerzone_filter[2 + 1 * GTP_PARAMETER_NUM] = PANEL_MAX_X - edge_filter_corner_size;
			cornerzone_filter[3 + 1 * GTP_PARAMETER_NUM] = 0;
			cornerzone_filter[4 + 1 * GTP_PARAMETER_NUM] = PANEL_MAX_X;
			cornerzone_filter[5 + 1 * GTP_PARAMETER_NUM] = edge_filter_corner_size;
			/* pos 3 */
			cornerzone_filter[2 + 3 * GTP_PARAMETER_NUM] = PANEL_MAX_X - edge_filter_corner_size;
			cornerzone_filter[3 + 3 * GTP_PARAMETER_NUM] = PANEL_MAX_Y - edge_filter_corner_size;
			cornerzone_filter[4 + 3 * GTP_PARAMETER_NUM] = PANEL_MAX_X;
			cornerzone_filter[5 + 3 * GTP_PARAMETER_NUM] = PANEL_MAX_Y;
		} else if(PANEL_ORIENTATION_DEGREE_0 == direction) {
			ts_info("direction: %d, edge_filter_corner_size_shuping: 150*300", direction);
			/* pos 2 */
			cornerzone_filter[2 + 2 * GTP_PARAMETER_NUM] = 0;
			cornerzone_filter[3 + 2 * GTP_PARAMETER_NUM] = PANEL_MAX_Y - VERTICAL_GAME_CORNER_SUPPRESSION_Y;
			cornerzone_filter[4 + 2 * GTP_PARAMETER_NUM] = VERTICAL_GAME_CORNER_SUPPRESSION_X;
			cornerzone_filter[5 + 2 * GTP_PARAMETER_NUM] = PANEL_MAX_Y;
			/* pos 3 */
			cornerzone_filter[2 + 3 * GTP_PARAMETER_NUM] = PANEL_MAX_X - VERTICAL_GAME_CORNER_SUPPRESSION_X;
			cornerzone_filter[3 + 3 * GTP_PARAMETER_NUM] = PANEL_MAX_Y - VERTICAL_GAME_CORNER_SUPPRESSION_Y;
			cornerzone_filter[4 + 3 * GTP_PARAMETER_NUM] = PANEL_MAX_X;
			cornerzone_filter[5 + 3 * GTP_PARAMETER_NUM] = PANEL_MAX_Y;
		} else if(PANEL_ORIENTATION_DEGREE_180 == direction) {
			ts_info("direction: %d, edge_filter_corner_size_shuping: 150*300", direction);
			/* pos 0 */
			cornerzone_filter[2] = 0;
			cornerzone_filter[3] = 0;
			cornerzone_filter[4] = VERTICAL_GAME_CORNER_SUPPRESSION_X;
			cornerzone_filter[5] = VERTICAL_GAME_CORNER_SUPPRESSION_Y;
			/* pos 1 */
			cornerzone_filter[2 + 1 * GTP_PARAMETER_NUM] = PANEL_MAX_X - VERTICAL_GAME_CORNER_SUPPRESSION_X;
			cornerzone_filter[3 + 1 * GTP_PARAMETER_NUM] = 0;
			cornerzone_filter[4 + 1 * GTP_PARAMETER_NUM] = PANEL_MAX_X;
			cornerzone_filter[5 + 1 * GTP_PARAMETER_NUM] = VERTICAL_GAME_CORNER_SUPPRESSION_Y;
		}
	}
	goodix_set_grip_filter((int*)&(cornerzone_filter[0]), &sum_corner);
	ts_info("cornerzone case 2");
	cornerzone = 2;
	if ((PANEL_ORIENTATION_DEGREE_0 == direction) || (PANEL_ORIENTATION_DEGREE_180 == direction))
		memset(cornerzone_filter,0,sizeof(cornerzone_filter));
		/* case1 and case2 use the same array, when set the case2 must memset case1，prevent affecting the vertical game mode*/
	for (i = 0; i < 4; i ++) {/* grip_type & grip_pos */
		cornerzone_filter[i * GTP_PARAMETER_NUM] = CORNER_ZONE_TYPE;
		cornerzone_filter[1 + i * GTP_PARAMETER_NUM] = i;
	}
	if (edge_filter_corner_size != 0) {
		if (PANEL_ORIENTATION_DEGREE_90 == direction) {
			ts_info("direction: %d, edge_filter_corner_size: %d", direction, GAME_CORNER_SUPPRESSION_HOR);
			/* pos 0 */
			cornerzone_filter[2] = 0;
			cornerzone_filter[3] = 0;
			cornerzone_filter[4] = GAME_CORNER_SUPPRESSION_HOR;
			cornerzone_filter[5] = GAME_CORNER_SUPPRESSION_VER;
			/* pos 2 */
			cornerzone_filter[2 + 2 * GTP_PARAMETER_NUM] = 0;
			cornerzone_filter[3 + 2 * GTP_PARAMETER_NUM] = PANEL_MAX_Y - GAME_CORNER_SUPPRESSION_VER;
			cornerzone_filter[4 + 2 * GTP_PARAMETER_NUM] = GAME_CORNER_SUPPRESSION_HOR;
			cornerzone_filter[5 + 2 * GTP_PARAMETER_NUM] = PANEL_MAX_Y;
		} else if(PANEL_ORIENTATION_DEGREE_270 == direction) {
			ts_info("direction: %d, edge_filter_corner_size: %d", direction, GAME_CORNER_SUPPRESSION_HOR);
			/* pos 1 */
			cornerzone_filter[2 + 1 * GTP_PARAMETER_NUM] = PANEL_MAX_X - GAME_CORNER_SUPPRESSION_HOR;
			cornerzone_filter[3 + 1 * GTP_PARAMETER_NUM] = 0;
			cornerzone_filter[4 + 1 * GTP_PARAMETER_NUM] = PANEL_MAX_X;
			cornerzone_filter[5 + 1 * GTP_PARAMETER_NUM] = GAME_CORNER_SUPPRESSION_VER;
			/* pos 3 */
			cornerzone_filter[2 + 3 * GTP_PARAMETER_NUM] = PANEL_MAX_X - GAME_CORNER_SUPPRESSION_VER;
			cornerzone_filter[3 + 3 * GTP_PARAMETER_NUM] = PANEL_MAX_Y - GAME_CORNER_SUPPRESSION_HOR;
			cornerzone_filter[4 + 3 * GTP_PARAMETER_NUM] = PANEL_MAX_X;
			cornerzone_filter[5 + 3 * GTP_PARAMETER_NUM] = PANEL_MAX_Y;
		}
	}
	goodix_set_grip_filter((int*)&(cornerzone_filter[0]), &sum_cornergame);
}
void goodix_set_edge_filter_normal(void)
{
	int sum_corner = 0, sum_edge = 0, sum_dead = 0, sum_cornergame = 0;
	struct goodix_xiaomi_board_data *bdata = &goodix_core_data->goodix_xiaomi_board_data;
	ts_info("deadzone");
	goodix_set_grip_filter((int*)&(xiaomi_touch_interfaces.long_mode_value[0]), &sum_dead);
	ts_info("edgezone");
	goodix_set_grip_filter((int*)&(xiaomi_touch_interfaces.long_mode_value[4 * GTP_PARAMETER_NUM]), &sum_edge);
	ts_info("cornerzone");
	cornerzone = 1;
	goodix_set_grip_filter((int*)&(xiaomi_touch_interfaces.long_mode_value[2 * 4 * GTP_PARAMETER_NUM]), &sum_corner);
	ts_info("cornerzonecase2 reset");
	cornerzone = 2;
	goodix_set_grip_filter((int*)&(xiaomi_touch_interfaces.long_mode_value[3 * 4 * GTP_PARAMETER_NUM]), &sum_cornergame);
	bdata->check_sum = - (sum_corner + sum_edge + sum_dead + sum_cornergame);
}
/*O6 code for HQ-392899 by liaoxianguo at 2024/07/11 end*/

static int goodix_get_mode_value(int mode, int value_type)
{
	int value = -1;
	if (mode < Touch_Mode_NUM && mode >= 0)
		value = xiaomi_touch_interfaces.touch_mode[mode][value_type];
	else
		ts_err("don't support");
	return value;
}
static int goodix_get_mode_all(int mode, int *value)
{
	if (mode < Touch_Mode_NUM && mode >= 0) {
		value[0] = xiaomi_touch_interfaces.touch_mode[mode][GET_CUR_VALUE];
		value[1] = xiaomi_touch_interfaces.touch_mode[mode][GET_DEF_VALUE];
		value[2] = xiaomi_touch_interfaces.touch_mode[mode][GET_MIN_VALUE];
		value[3] = xiaomi_touch_interfaces.touch_mode[mode][GET_MAX_VALUE];
	} else {
		ts_err("don't support");
	}
	ts_info("mode:%d, value:%d:%d:%d:%d", mode, value[0], value[1], value[2], value[3]);
	return 0;
}
static int goodix_reset_mode(int mode)
{
	int i = 0;
	if (!goodix_core_data) {
		ts_err("goodix_core_data is null");
		return -EINVAL;
	}
	ts_info("mode:%d", mode);
	if (mode < Touch_Mode_NUM && mode > 0) {
		xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] =
			xiaomi_touch_interfaces.touch_mode[mode][GET_DEF_VALUE];
		/*N6 code for HQ-305061 by zhangzhijian5 at 2023/10/30 start*/
		queue_work(goodix_core_data->game_wq, &goodix_core_data->game_work);
		/*N6 code for HQ-305061 by zhangzhijian5 at 2023/10/30 end*/
	} else if (mode == 0) {
		/*O6 code for HQ-392899 by liaoxianguo at 2024/07/11 start*/
		if (goodix_core_data) {
			/*进退游戏模式除了在set_cur_value中还在reset_mode中*/
			ts_info("Touch_Game_Mode value by Reset mode is 0\n");
			goodix_core_data->gamemode_enabled = false;
		}
		/*O6 code for HQ-392899 by liaoxianguo at 2024/07/11 end*/
		for (i = 0; i <= Touch_Panel_Orientation; i++) {
			if (i == Touch_Panel_Orientation)
				xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE] =
					xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE];
			else {
				xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE] =
					xiaomi_touch_interfaces.touch_mode[i][GET_DEF_VALUE];
			}
		}
		/*N6 code for HQ-305061 by zhangzhijian5 at 2023/10/30 start*/
		queue_work(goodix_core_data->game_wq, &goodix_core_data->game_work);
		/*N6 code for HQ-305061 by zhangzhijian5 at 2023/10/30 end*/
	} else {
		ts_err("don't support");
	}
	return 0;
}
static void goodix_init_touchmode_data(void)
{
	int i;
	/* Touch Game Mode Switch */
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_MAX_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_CUR_VALUE] = 0;
	/* Acitve Mode */
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_MAX_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_CUR_VALUE] = 0;
	/* tap sensitivity */
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_MAX_VALUE] = 4;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_DEF_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][SET_CUR_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_CUR_VALUE] = 3;
	/* latency */
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_MAX_VALUE] = 4;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_DEF_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][SET_CUR_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_CUR_VALUE] = 2;
	/* aim sensitivity */
	xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][GET_MAX_VALUE] = 4;
	xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][GET_DEF_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][SET_CUR_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Aim_Sensitivity][GET_CUR_VALUE] = 2;
	/* tap stability */
	xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][GET_MAX_VALUE] = 4;
	xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][GET_DEF_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][SET_CUR_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Tap_Stability][GET_CUR_VALUE] = 2;
	/* edge filter */
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_MAX_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_DEF_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][SET_CUR_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_CUR_VALUE] = 2;
	/*Orientation */
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_MAX_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_CUR_VALUE] = 0;
	for (i = 0; i < Touch_Mode_NUM; i++) {
		ts_info("mode:%d, set cur:%d, get cur:%d, def:%d min:%d max:%d\n",
				i,
				xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE],
				xiaomi_touch_interfaces.touch_mode[i][GET_CUR_VALUE],
				xiaomi_touch_interfaces.touch_mode[i][GET_DEF_VALUE],
				xiaomi_touch_interfaces.touch_mode[i][GET_MIN_VALUE],
				xiaomi_touch_interfaces.touch_mode[i][GET_MAX_VALUE]);
	}
}
/*N6 code for HQ-334519 by liaoxianguo at 2023/10/17 start*/

/*N6 code for HQ-307922 by zhangzhijian5 at 2023/11/2 start*/
static u8 goodix_panel_color_read(void)
{
	if (!goodix_core_data)
		return 0;

	return goodix_core_data->lockdown_info[2];
}

static u8 goodix_panel_vendor_read(void)
{
	if (!goodix_core_data)
		return 0;

	return goodix_core_data->lockdown_info[0];
}

static u8 goodix_panel_display_read(void)
{
	if (!goodix_core_data)
		return 0;

	return goodix_core_data->lockdown_info[1];
}

/*
 * goodix_touch_vendor_read - read vendor ic name
 *
 * return: 1 is st, 2 is goodix, 3 is focal, 4 is nvt and 5 is synaptics
 */
static char goodix_touch_vendor_read(void)
{
	return '2';
}
/*N6 code for HQ-307922 by zhangzhijian5 at 2023/11/2 end*/

/*N6 code for HQ-305056 by zhangzhijian5 at 2023/10/26 start*/
static int goodix_palm_sensor_write(int value)
{
	/*N6 code for HQ-342530 by zhangzhijian5 at 2023/11/2 start*/
	struct goodix_ts_hw_ops *hw_ops = NULL;
	int ret = 0;

	ts_info("palm sensor value : %d", value);
	if ((!goodix_core_data) || (!goodix_core_data->hw_ops)) {
		ts_err("goodix core data os NULL or hw_ops is NULL");
		return -EINVAL;
	}

	hw_ops = goodix_core_data->hw_ops;
	goodix_core_data->palm_status = value;
	if (goodix_core_data->work_status == TP_NORMAL) {
		if (!hw_ops->palm_on) {
			ts_err("palm_on is NULL");
			return -EINVAL;
		}
		ret = hw_ops->palm_on(goodix_core_data, !!value);
        }
	/*N6 code for HQ-342530 by zhangzhijian5 at 2023/11/2 end*/
	return ret;
}
/*N6 code for HQ-305056 by zhangzhijian5 at 2023/10/26 end*/

/*N6 code for HQ-346302 by huangshiquan at 2023/11/09 start*/
static int goodix_touch_hdle_mode_set(bool value)
{
	struct goodix_ts_hw_ops *hw_ops = NULL;
	int ret = 0;

	ts_info("hdle value : %d", value);

	if ((!goodix_core_data) || (!goodix_core_data->hw_ops)) {
		ts_err("goodix_core_data or hw_ops is NULL");
		return -EINVAL;
	}

	hw_ops = goodix_core_data->hw_ops;
	if (!hw_ops->hdle_mode_set) {
        	ts_err("hw_ops->hdle_mode_set is NULL\n");
		return -EINVAL;
        }
	ret = hw_ops->hdle_mode_set(goodix_core_data, value);

	return ret;
}
/*N6 code for HQ-346302 by huangshiquan at 2023/11/09 end*/

/*N6 code for HQ-304306 by liaoxianguo at 2023/10/11 start*/
static ssize_t goodix_selftest_read(struct file *file, char __user *buf,
		size_t count, loff_t *pos)
{
	char tmp[GTP_TP_SELFTEST_TMP_LEN] = { 0 };
	int cnt = 0;

	if (*pos != 0 || !goodix_core_data)
		return 0;

	cnt = snprintf(tmp, sizeof(goodix_core_data->result_type), "%d\n",
			goodix_core_data->result_type);
	if (copy_to_user(buf, tmp, strlen(tmp))) {
		return -EFAULT;
	}

	*pos += cnt;
	return cnt;
}

static int goodix_short_open_test(void)
{
	struct ts_rawdata_info *info = NULL;
	int test_result = 0;

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		ts_err("Failed to alloc rawdata info memory");
		return GTP_RESULT_INVALID;
	}

	if (goodix_get_rawdata(&goodix_core_data->pdev->dev, info)) {
		ts_err("Factory_test FAIL");
		test_result = GTP_RESULT_INVALID;
		goto exit;
	}

	if (TP_SELFTEST_RESULT_LEN == (*(info->result + 1))) {
		ts_info("test PASS!");
		test_result = GTP_RESULT_PASS;
	} else {
		ts_err("test FAILED!");
		test_result = GTP_RESULT_FAIL;
	}

exit:
	ts_info("resultInfo: %s", info->result);
	/* ret = snprintf(buf, PAGE_SIZE, "resultInfo: %s", info->result); */
	kfree(info);
	return test_result;
}

static ssize_t goodix_selftest_write(struct file *file, const char __user *buf,
		size_t count, loff_t *pos)
{
	struct goodix_fw_version chip_ver = { 0 };
	struct goodix_ts_hw_ops *hw_ops = NULL;
	int retval = 0;
	char tmp[6] = { 0 };

	if (copy_from_user(tmp, buf, count)) {
		retval = -EFAULT;
		goto out;
	}

	if (!goodix_core_data) {
		return GTP_RESULT_INVALID;
	} else {
		hw_ops = goodix_core_data->hw_ops;
		if (!hw_ops) {
			return GTP_RESULT_INVALID;
		}
	}

	if (!strncmp("short", tmp, SHORT_TEST_LEN) ||
		!strncmp("open", tmp, OPEN_TEST_LEN)) {
		retval = goodix_short_open_test();
	} else if (!strncmp("i2c", tmp, IIC_TEST_LEN)) {
		hw_ops->read_version(goodix_core_data, &chip_ver);
		if (chip_ver.sensor_id == 255)
			retval = GTP_RESULT_PASS;
		else
			retval = GTP_RESULT_FAIL;
	}
	goodix_core_data->result_type = retval;

out:
	if (retval >= 0)
		retval = count;
	return retval;
}

static const struct proc_ops goodix_selftest_ops = {
	.proc_read = goodix_selftest_read,
	.proc_write = goodix_selftest_write,
	.proc_lseek = default_llseek,
};
/*N6 code for HQ-304306 by liaoxianguo at 2023/10/11 end*/
/*P6 code for HQFEAT-118700 by p-zhaobeidou3 at 2025/6/27 start*/
#if IS_ENABLED(CONFIG_MIEV)
static void gtp_touch_dfs_test(int value){
	switch(value){
		case TOUCH_EVENT_TRANSFER_ERR:
			xiaomi_touch_mievent_report_int(TOUCH_EVENT_TRANSFER_ERR, 0, "TpTransferErr", "goodix", -1);
			break;
		case TOUCH_EVENT_FWLOAD_ERR:
			xiaomi_touch_mievent_report_int(TOUCH_EVENT_FWLOAD_ERR, 0, "TpFirmwareLoadFail", "goodix", -1);
			break;
		case TOUCH_EVENT_PARAM_ERR:
			xiaomi_touch_mievent_report_int(TOUCH_EVENT_PARAM_ERR, 0, "TpParamParseFail", "goodix", ERROR_DTS_PARSE);
			xiaomi_touch_mievent_report_int(TOUCH_EVENT_PARAM_ERR, 0, "TpParamParseFail", "goodix", ERROR_REGULATOR_INIT);
			xiaomi_touch_mievent_report_int(TOUCH_EVENT_PARAM_ERR, 0, "TpParamParseFail", "goodix", ERROR_GPIO_REQUEST);
			break;
		case TOUCH_EVENT_OPENTEST_FAIL:
			xiaomi_touch_mievent_report_int(TOUCH_EVENT_OPENTEST_FAIL, 0, "TpOpenTestFail", "goodix", -1);
			break;
		case TOUCH_EVENT_SHORTTEST_FAIL:
			xiaomi_touch_mievent_report_int(TOUCH_EVENT_SHORTTEST_FAIL, 0, "TpShortTestFail", "goodix", -1);
			break;
		default:
			ts_err("don't support touch dfs test");
			break;
	}
}
#endif
/*P6 code for HQFEAT-118700 by p-zhaobeidou3 at 2025/6/27 end*/
/*O6 code for HQ-391996 by liuyupei at 2024/7/4 start*/
int tp_compare_ic(void)
{
	int gpio_50 = 0;
	int gpio_52 = 0;
	ts_info("tp_compare_ic in!!");
	gpio_direction_input(DISP_ID0_DET);
	gpio_50 = gpio_get_value(DISP_ID0_DET);
	gpio_direction_input(DISP_ID1_DET);
	gpio_52 = gpio_get_value(DISP_ID1_DET);
	ts_info("gpio_50 = %d, gpio_52 = %d\n", gpio_50, gpio_52);
	tp_compatible_flag = gpio_50 << 1 | gpio_52;
	if (tp_compatible_flag == TOUCH_SELECT_HUAXING){
		    TS_DEFAULT_FIRMWARE = "gt9916_ts_fw_csot.bin";
		    TS_DEFAULT_CFG_BIN = "goodix_cfg_group_csot.bin";
		    ts_info("TP is HUAXING");
	}   else if (tp_compatible_flag == TOUCH_SELECT_VISIONOX) {
		    TS_DEFAULT_FIRMWARE = "gt9916_ts_fw_vsn.bin";
		    TS_DEFAULT_CFG_BIN = "goodix_cfg_group_vsn.bin";
		    ts_info("TP is VISIONOX");
	}   else {
		    ts_err("TP is not CORRECT!");
		    return -ENODEV;
	}
	return 0;
}
/*O6 code for HQ-391996 by liuyupei at 2024/7/4 end*/
/**
 * goodix_ts_probe - called by kernel when Goodix touch
 *  platform driver is added.
 */
static int goodix_ts_probe(struct platform_device *pdev)
{
	struct goodix_ts_core *core_data = NULL;
	struct goodix_bus_interface *bus_interface;
	/*O6 code for HQ-391996 by liaoxianguo at 2024/7/15 start*/
	int ret = 0;
	ret = tp_compare_ic();
	if (ret < 0) {
		ts_err("TP_COMPATIBLE IS NOT CORRECT!");
		return -ENODEV;
	}
	/*O6 code for HQ-391996 by liaoxianguo at 2024/7/15 end*/
	ts_info("tp_compare_ic_in_probe");
	/*O6 code for HQ-391996 by liuyupei at 2024/7/4 end*/
	ts_info("goodix_ts_probe IN");
	bus_interface = pdev->dev.platform_data;
	if (!bus_interface) {
		ts_err("Invalid touch device");
		core_module_prob_sate = CORE_MODULE_PROB_FAILED;
		return -ENODEV;
	}

	core_data = devm_kzalloc(&pdev->dev,
			sizeof(struct goodix_ts_core), GFP_KERNEL);
	if (!core_data) {
		core_module_prob_sate = CORE_MODULE_PROB_FAILED;
		return -ENOMEM;
	}
	/*N6 code for HQ-305074 by dingying at 2023/09/27 start*/
	goodix_core_data = core_data;
	/*N6 code for HQ-305074 by dingying at 2023/09/27 end*/

	if (IS_ENABLED(CONFIG_OF) && bus_interface->dev->of_node) {
		/* parse devicetree property */
		ret = goodix_parse_dt(bus_interface->dev->of_node,
					&core_data->board_data);
		if (ret) {
/*o6 code for HQFEAT-118700 by p-zhoujun33 at 2025/8/19 start*/
#if IS_ENABLED(CONFIG_MIEV)
			xiaomi_touch_mievent_report_int(TOUCH_EVENT_PARAM_ERR, 0, "TpParamParseFail", "goodix", ERROR_DTS_PARSE);
#endif
/*o6 code for HQFEAT-118700 by p-zhoujun33 at 2025/8/19 end*/
			ts_err("failed parse device info form dts, %d", ret);
			return -EINVAL;
		}
	} else {
		ts_err("no valid device tree node found");
		return -ENODEV;
	}

	core_data->hw_ops = goodix_get_hw_ops();
	if (!core_data->hw_ops) {
		ts_err("hw ops is NULL");
		core_module_prob_sate = CORE_MODULE_PROB_FAILED;
		return -EINVAL;
	}
	goodix_core_module_init();
	/* touch core layer is a platform driver */
	core_data->pdev = pdev;
	core_data->bus = bus_interface;
	platform_set_drvdata(pdev, core_data);

	/* get GPIO resource */
	ret = goodix_ts_gpio_setup(core_data);
	if (ret) {
/*o6 code for HQFEAT-118700 by p-zhoujun33 at 2025/8/19 start*/
#if IS_ENABLED(CONFIG_MIEV)
			xiaomi_touch_mievent_report_int(TOUCH_EVENT_PARAM_ERR, 0, "TpParamParseFail", "goodix", ERROR_DTS_PARSE);
#endif
/*o6 code for HQFEAT-118700 by p-zhoujun33 at 2025/8/19 end*/
		ts_err("failed init gpio");
		goto err_out;
	}
	ret = goodix_ts_power_init(core_data);
	if (ret) {
/*o6 code for HQFEAT-118700 by p-zhoujun33 at 2025/8/19 start*/
#if IS_ENABLED(CONFIG_MIEV)
			xiaomi_touch_mievent_report_int(TOUCH_EVENT_PARAM_ERR, 0, "TpParamParseFail", "goodix", ERROR_DTS_PARSE);
#endif
/*o6 code for HQFEAT-118700 by p-zhoujun33 at 2025/8/19 end*/
		ts_err("failed init power");
		goto err_out;
	}
	ret = goodix_ts_power_on(core_data);
	if (ret) {
/*o6 code for HQFEAT-118700 by p-zhoujun33 at 2025/8/19 start*/
#if IS_ENABLED(CONFIG_MIEV)
			xiaomi_touch_mievent_report_int(TOUCH_EVENT_PARAM_ERR, 0, "TpParamParseFail", "goodix", ERROR_DTS_PARSE);
#endif
/*o6 code for HQFEAT-118700 by p-zhoujun33 at 2025/8/19 end*/
		ts_err("failed power on");
		goto err_out;
	}

	/* generic notifier callback */
	core_data->ts_notifier.notifier_call = goodix_generic_noti_callback;
	goodix_ts_register_notifier(&core_data->ts_notifier);

	/*N6 code for HQ-305074 by dingying at 2023/09/27 start*/

	device_init_wakeup(core_data->bus->dev, 1);
	core_data->tp_fw_version_proc =
		proc_create("tp_info", 0664, NULL, &goodix_fw_version_info_ops);
	/*N6 code for HQ-305074 by dingying at 2023/09/27 end*/
	/*N6 code for HQ-305069 by liaoxianguo at 2023/09/29 start*/
	core_data->tp_lockdown_info_proc =
		proc_create("tp_lockdown_info", 0664, NULL, &goodix_lockdown_info_ops);
	/*N6 code for HQ-305069 by liaoxianguo at 2023/09/29 end*/
	/*N6 code for HQ-304306 by liaoxianguo at 2023/10/11 start*/
	core_data->tp_selftest_proc =
		proc_create("tp_selftest", 0664, NULL, &goodix_selftest_ops);
	/*N6 code for HQ-304306 by liaoxianguo at 2023/10/11 end*/

	/*N6 code for HQ-305061 by zhangzhijian5 at 2023/10/30 start*/
	if (core_data->goodix_tp_class == NULL) {
		core_data->goodix_tp_class = get_xiaomi_touch_class();
		if (core_data->goodix_tp_class) {
			core_data->goodix_touch_dev =
				device_create(core_data->goodix_tp_class, NULL,
					      0x38, core_data, "tp_dev");
			if (IS_ERR(core_data->goodix_touch_dev)) {
				ts_err("Failed to create device !\n");
				goto err_class_create;
			}
			dev_set_drvdata(core_data->goodix_touch_dev, core_data);
		}
	}

	core_data->game_wq = alloc_workqueue("gtp-game-queue",
				WQ_UNBOUND | WQ_HIGHPRI | WQ_CPU_INTENSIVE, 1);
	if (!core_data->game_wq)
		ts_err("goodix cannot create game work thread");
	INIT_WORK(&core_data->game_work, goodix_set_game_work);
	/*N6 code for HQ-305061 by zhangzhijian5 at 2023/10/30 end*/

	/*N6 code for HQ-334519 by liaoxianguo at 2023/10/17 start*/
	memset(&xiaomi_touch_interfaces, 0x00, sizeof(struct xiaomi_touch_interface));
	xiaomi_touch_interfaces.setModeValue = goodix_set_cur_value;
	/*O6 code for HQ-392899 by liaoxianguo at 2024/07/11 start*/
	xiaomi_touch_interfaces.setModeLongValue = goodix_set_mode_long_value;
	/*O6 code for HQ-392899 by liaoxianguo at 2024/07/11 end*/
	xiaomi_touch_interfaces.getModeValue = goodix_get_mode_value;
	xiaomi_touch_interfaces.resetMode = goodix_reset_mode;
	xiaomi_touch_interfaces.getModeAll = goodix_get_mode_all;
	/*N6 code for HQ-307922 by zhangzhijian5 at 2023/11/2 start*/
	xiaomi_touch_interfaces.panel_display_read = goodix_panel_display_read;
	xiaomi_touch_interfaces.panel_vendor_read = goodix_panel_vendor_read;
	xiaomi_touch_interfaces.panel_color_read = goodix_panel_color_read;
	xiaomi_touch_interfaces.touch_vendor_read = goodix_touch_vendor_read;
	/*N6 code for HQ-307922 by zhangzhijian5 at 2023/11/2 end*/
	/*N6 code for HQ-305056 by zhangzhijian5 at 2023/10/26 start*/
	xiaomi_touch_interfaces.palm_sensor_write = goodix_palm_sensor_write;
	/*N6 code for HQ-305056 by zhangzhijian5 at 2023/10/26 end*/
	/*N6 code for HQ-343656 by huangshiquan at 2023/11/08 start*/
	xiaomi_touch_interfaces.fod_test_store = goodix_fod_test_store;
	/*N6 code for HQ-343656 by huangshiquan at 2023/11/08 end*/
	/*N6 code for HQ-346302 by huangshiquan at 2023/11/09 start*/
	xiaomi_touch_interfaces.touch_hdle_mode_set = goodix_touch_hdle_mode_set;
	/*N6 code for HQ-346302 by huangshiquan at 2023/11/09 end*/
/*o6 code for HQFEAT-118700 by p-zhoujun33 at 2025/8/19 start*/
#if IS_ENABLED(CONFIG_MIEV)
    xiaomi_touch_interfaces.touch_dfs_test = gtp_touch_dfs_test;
#endif
/*o6 code for HQFEAT-118700 by p-zhoujun33 at 2025/8/19 end*/
	xiaomitouch_register_modedata(0, &xiaomi_touch_interfaces);
	goodix_init_touchmode_data();

	/* debug node init */
	goodix_tools_init();
	#ifdef KERNEL_FACTORY_BUILD
	core_data->fod_status = 1;
	#else
	core_data->fod_status = -1;
	#endif
	/*N6 code for HQ-334519 by liaoxianguo at 2023/10/17 end*/
	/* goodix fb test */
	// fb_firefly_register(test_suspend, test_resume);

	core_data->init_stage = CORE_INIT_STAGE1;
	goodix_modules.core_data = core_data;
	core_module_prob_sate = CORE_MODULE_PROB_SUCCESS;

	/* Try start a thread to get config-bin info */
/*N6 code for HQ-339809 by zhangzhijian5 at 2023/10/27 start*/
	goodix_start_later_init(core_data);
/*N6 code for HQ-339809 by zhangzhijian5 at 2023/10/27 end*/

	ts_info("goodix_ts_core probe success");
	return 0;

/*N6 code for HQ-305061 by zhangzhijian5 at 2023/10/30 start*/
err_class_create:
	class_destroy(core_data->goodix_tp_class);
	core_data->goodix_tp_class = NULL;
/*N6 code for HQ-305061 by zhangzhijian5 at 2023/10/30 end*/

err_out:
	core_data->init_stage = CORE_INIT_FAIL;
	core_module_prob_sate = CORE_MODULE_PROB_FAILED;
	ts_err("goodix_ts_core failed, ret:%d", ret);
	return ret;
}

static void goodix_ts_remove(struct platform_device *pdev)
{
	struct goodix_ts_core *core_data = platform_get_drvdata(pdev);
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	struct goodix_ts_esd *ts_esd = &core_data->ts_esd;

	goodix_ts_unregister_notifier(&core_data->ts_notifier);
	goodix_tools_exit();

	if (core_data->init_stage >= CORE_INIT_STAGE2) {
		gesture_module_exit();
		inspect_module_exit();
		hw_ops->irq_enable(core_data, false);
	/* O6 code for HQ-390162 by liuyupei at 2024/6/28 start */	
	#if IS_ENABLED(CONFIG_MI_DISP_NOTIFIER)
		mi_disp_unregister_client(&core_data->fb_notifier);
	#endif
    /* O6 code for HQ-390162 by liuyupei at 2024/6/28 end */

		core_module_prob_sate = CORE_MODULE_REMOVED;
		if (atomic_read(&core_data->ts_esd.esd_on))
			goodix_ts_esd_off(core_data);
		goodix_ts_unregister_notifier(&ts_esd->esd_notifier);

		goodix_fw_update_uninit();
		goodix_ts_input_dev_remove(core_data);
		goodix_ts_pen_dev_remove(core_data);
		goodix_ts_sysfs_exit(core_data);
		goodix_ts_procfs_exit(core_data);
		goodix_ts_power_off(core_data);
	}

	return ;
}

/* O6 code for HQ-390162 by liuyupei at 2024/6/28 start */
#if IS_ENABLED(CONFIG_PM)
static const struct dev_pm_ops dev_pm_ops = {
#if 0
	.suspend = goodix_ts_pm_suspend,
	.resume = goodix_ts_pm_resume,
#endif
};
#endif
/* O6 code for HQ-390162 by liuyupei at 2024/6/28 end */

static const struct platform_device_id ts_core_ids[] = {
	{.name = GOODIX_CORE_DRIVER_NAME},
	{}
};
MODULE_DEVICE_TABLE(platform, ts_core_ids);

static struct platform_driver goodix_ts_driver = {
	.driver = {
		.name = GOODIX_CORE_DRIVER_NAME,
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_PM)
		.pm = &dev_pm_ops,
#endif
	},
	.probe = goodix_ts_probe,
	.remove = goodix_ts_remove,
	.id_table = ts_core_ids,
};

static int __init goodix_ts_core_init(void)
{
	int ret = 0;
	ts_info("Core layer init:%s", GOODIX_DRIVER_VERSION);
#ifdef CONFIG_TOUCHSCREEN_GOODIX_BRL_SPI
	ret = goodix_spi_bus_init();
#else
	ret = goodix_i2c_bus_init();
#endif
	if (ret) {
		ts_err("failed add bus driver");
		return ret;
	}
	return platform_driver_register(&goodix_ts_driver);
}

static void __exit goodix_ts_core_exit(void)
{
	ts_info("Core layer exit");
	platform_driver_unregister(&goodix_ts_driver);
#ifdef CONFIG_TOUCHSCREEN_GOODIX_BRL_SPI
	goodix_spi_bus_exit();
#else
	goodix_i2c_bus_exit();
#endif
}

late_initcall(goodix_ts_core_init);
module_exit(goodix_ts_core_exit);

/*N6 code for HQ-332301 by dingying at 2023/09/27 start*/
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
/*N6 code for HQ-332301 by dingying at 2023/09/27 end*/
MODULE_DESCRIPTION("Goodix Touchscreen Core Module");
MODULE_AUTHOR("Goodix, Inc.");
MODULE_LICENSE("GPL v2");
