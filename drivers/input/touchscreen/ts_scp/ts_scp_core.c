// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#define pr_fmt(fmt) "[ts_scp]" fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kfifo.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/atomic.h>
#include <linux/kthread.h>
#include <uapi/linux/sched/types.h>
#include <linux/sched.h>
#include <linux/cpumask.h>
#include "ts_scp_core.h"
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/pinctrl/consumer.h>
#include <linux/notifier.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 38)
#include <linux/input/mt.h>
#define INPUT_TYPE_B_PROTOCOL
#endif

DEFINE_SPINLOCK(ts_scp_fifo_lock);
DECLARE_COMPLETION(ts_scp_data_done);
DEFINE_KFIFO(ts_scp_data_fifo, struct ts_scp_tp_multi_data, 8);

DECLARE_COMPLETION(ts_scp_ready_done);
DECLARE_COMPLETION(ts_scp_scene_done);

bool debug_log_flag = false;
uint8_t ctrl_cmd;
struct ts_scp_device ts_scp_dev;
struct touch_comm_notify_status ts_scp_notify_status;
void (*generic_report_func)(struct ts_scp_device *dev);
static bool touch_on_ap_has_finger_flag = false;
static int finger_on_mode = TS_FINIGER_NONE;
atomic_t scp_crash_num = ATOMIC_INIT(0);

static int ts_scp_cmd_scp_handle(void);
static int ts_scp_cmd_ap_handle(void);
static int ts_scp_cmd_query_scp_status(void);
static int ts_scp_cmd_suspend(void);
static int ts_scp_cmd_resume(void);
static int ts_scp_cmd_reinit(void);
static void ts_scp_touch_mode_scp(void);
static void ts_scp_touch_mode_ap(void);
static uint8_t ts_scp_get_touch_type(void);
static uint8_t ts_scp_get_touch_id(void);
static void ts_scp_force_switch_to_ap(void);
static void ts_scp_ready_process(void);
static void ts_scp_force_reset_crash_num(void);

void connect_report_func(void (*report_func)(struct ts_scp_device *dev))
{
	ts_scp_info("connect report func");
	generic_report_func = report_func;
}
EXPORT_SYMBOL_GPL(connect_report_func);

int generic_power_on_reinit(void)
{
	if (generic_power_on_reinit_func) {
		if (ts_scp_check_is_scp_enabled(&scene)) {
			ts_scp_info("reinit scp side");
			ts_scp_cmd_reinit();
			return 0;
		} else {
			ts_scp_info("reinit ap side");
			return generic_power_on_reinit_func();
		}
	} else {
		ts_scp_err("generic_power_on_reinit_func is NULL");
		return -ENOSYS;
	}
}
EXPORT_SYMBOL_GPL(generic_power_on_reinit);

/* debug log on/off show */
static ssize_t ts_scp_debug_log_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	int r = 0;

	r = sprintf(buf, "state:%s\n",
		    debug_log_flag ?
		    "enabled" : "disabled");
	if (r < 0)
		ts_scp_err("buf print fail");

	return r;
}

/* debug log on/off store */
static ssize_t ts_scp_debug_log_store(struct device *dev,
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

/* touch on scp or ap side show */
static ssize_t ts_scp_touch_stat_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	int r = 0;
	bool touch_stat;

	touch_stat = ts_scp_check_is_scp_enabled(&scene);
	r = sprintf(buf, "touch side:%s\n",
		    touch_stat ?
		    "scp" : "ap");
	if (r < 0)
		ts_scp_err("buf print fail");

	return r;
}

/* ctrl cmd show */
static ssize_t ts_scp_ctrl_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	int r = 0;

	r = sprintf(buf, "ctrl cmd: 0x%02x\n", ctrl_cmd);
	if (r < 0)
		ts_scp_err("buf print fail");

	return r;
}

/* ctrl cmd store */
static ssize_t ts_scp_ctrl_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int ret = 0;

	if (!buf || count <= 0)
		return -EINVAL;

	ctrl_cmd = buf[0];
	ts_scp_info("ts_scp_ctrl_store cmd=0x%02x", buf[0]);

	switch (buf[0]) {
	case '0':
		if (ts_scp_check_is_crash_too_much(&scene) == false) {
			if (ts_scp_check_is_finger_on_touch(&scene) == false) {
				generic_irq_enable(false);
				generic_esd_control(false);
				ts_scp_touch_mode_scp();
				ts_scp_set_scp_enable();
				ts_scp_clear_scenario_status(STAT_SCP_CRASH);
				ret = ts_scp_cmd_scp_handle();
				if ((ret == -EBADMSG) || (ret == -EILSEQ) || (ret == -EPROTO)
					|| (ret == -EREMOTEIO))
					goto err_ack;
			} else {
				touch_on_ap_has_finger_flag = true;
				finger_on_mode = TS_FINIGER_ENABLE_CMD;
				ts_scp_info("Touch work on AP, has touch on touchscreen enable");
			}
		} else {
			ts_scp_info("scp crash too much. cmd not works. cmd=0x%02x", buf[0]);
		}
		break;
	case '1':
		if (ts_scp_check_is_crash_too_much(&scene) == false) {
			generic_irq_enable(true);
			generic_esd_control(true);
			ts_scp_touch_mode_ap();
			ts_scp_set_scp_disable();
			ret = ts_scp_cmd_ap_handle();
			if ((ret == -EBADMSG) || (ret == -EILSEQ) || (ret == -EPROTO)
				|| (ret == -EREMOTEIO))
				goto err_ack;
		} else {
			ts_scp_info("scp crash too much. cmd not works. cmd=0x%02x", buf[0]);
		}
		break;
	case '2':
		if (ts_scp_check_is_crash_too_much(&scene) == false) {
			ts_scp_ready_process();
		} else {
			ts_scp_info("scp crash too much. cmd not works. cmd=0x%02x", buf[0]);
		}
		break;
	case '3':
		if (ts_scp_check_is_crash_too_much(&scene) == false) {
			ret = ts_scp_cmd_suspend();
			if ((ret == -EBADMSG) || (ret == -EILSEQ) || (ret == -EPROTO)
				|| (ret == -EREMOTEIO))
				goto err_ack;
		} else {
			ts_scp_info("scp crash too much. cmd not works. cmd=0x%02x", buf[0]);
		}
		break;
	case '4':
		if (ts_scp_check_is_crash_too_much(&scene) == false) {
			ret = ts_scp_cmd_resume();
			if ((ret == -EBADMSG) || (ret == -EILSEQ) || (ret == -EPROTO)
				|| (ret == -EREMOTEIO))
				goto err_ack;
		} else {
			ts_scp_info("scp crash too much. cmd not works. cmd=0x%02x", buf[0]);
		}
		break;
	case '5':
		if (ts_scp_check_is_crash_too_much(&scene) == false) {
			generic_power_on_reinit();
		} else {
			ts_scp_info("scp crash too much. cmd not works. cmd=0x%02x", buf[0]);
		}
		break;
	case '6':
		ts_scp_force_reset_crash_num();
		break;
	default:
		ts_scp_err("ctrl node receive error cmd");
		break;
	}
	return count;

err_ack:
	ts_scp_force_switch_to_ap();
	return count;
}

static DEVICE_ATTR(debug_log, 0660,
		ts_scp_debug_log_show, ts_scp_debug_log_store);
static DEVICE_ATTR(touch_stat, 0440,
		ts_scp_touch_stat_show, NULL);
static DEVICE_ATTR(ctrl, 0660,
		ts_scp_ctrl_show, ts_scp_ctrl_store);

static struct attribute *ts_scp_attrs[] = {
	&dev_attr_debug_log.attr,
	&dev_attr_touch_stat.attr,
	&dev_attr_ctrl.attr,
	NULL,
};

static struct attribute_group ts_scp_attrs_group = {
	.attrs = ts_scp_attrs,
};

const struct attribute_group *ts_scp_attrs_groups[] = {
	&ts_scp_attrs_group,
	NULL,
};

static int ts_scp_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &ts_scp_dev;
	return nonseekable_open(inode, filp);
}

static int ts_scp_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static long ts_scp_ioctl(struct file *filp,
			unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	unsigned int size = _IOC_SIZE(cmd);
	void __user *ubuf = (void __user *)arg;
	struct ts_scp_node_debug debug;
	struct ts_scp_node_ctrl ctrl;

	switch (cmd) {
	case TS_SCP_NODE_DEBUG:
		if (size != sizeof(debug))
			return -EINVAL;
		if (copy_from_user(&debug, ubuf, size))
			return -EFAULT;

		debug_log_flag = debug.debug_on;
		break;
	case TS_SCP_NODE_CTRL:
		if (size != sizeof(debug))
			return -EINVAL;
		if (copy_from_user(&debug, ubuf, size))
			return -EFAULT;

		ctrl_cmd = ctrl.cmd;
		break;
	default:
		ts_scp_err("Unknown command %u\n", cmd);
		return -EINVAL;
	}
	return ret;
}

static const struct file_operations ts_scp_fops = {
	.owner          = THIS_MODULE,
	.open           = ts_scp_open,
	.release        = ts_scp_release,
	.unlocked_ioctl = ts_scp_ioctl,
	.compat_ioctl   = ts_scp_ioctl,
};

static void ts_scp_data_notify_func(struct touch_comm_notify *n,
		void *private_data)
{
	if (n->command != TOUCH_COMM_NOTIFY_DATA_CMD)
		return;

	memcpy(&ts_scp_dev.ts_data->multi_data, n->value,
		sizeof(struct ts_scp_tp_multi_data));

	spin_lock(&ts_scp_fifo_lock);
	kfifo_in(&ts_scp_data_fifo, &ts_scp_dev.ts_data->multi_data, 1);
	complete(&ts_scp_data_done);
	spin_unlock(&ts_scp_fifo_lock);
}

static void ts_scp_ready_notify_func(struct touch_comm_notify *n,
		void *private_data)
{
	if (n->command != TOUCH_COMM_NOTIFY_READY_CMD)
		return;
	complete(&ts_scp_ready_done);
}

static void ts_scp_data_process(void)
{
	unsigned int ret = 0;
	struct ts_scp_tp_multi_data multi_data;
	unsigned long flags = 0;

	while (1) {
		spin_lock_irqsave(&ts_scp_fifo_lock, flags);
		ret = kfifo_out(&ts_scp_data_fifo, &multi_data, 1);
		spin_unlock_irqrestore(&ts_scp_fifo_lock, flags);
		if (!ret) {
			break;
		}

		ts_scp_dev.ts_data->multi_data = multi_data;
		generic_report_func(&ts_scp_dev);
	}
}

static int ts_scp_thread(void *data)
{
	int ret = 0;

	while (!kthread_should_stop()) {
		ret = wait_for_completion_interruptible(&ts_scp_data_done);
		if (ret)
			continue;

		ts_scp_data_process();
	}

	return 0;
}

static void ts_scp_ready_process(void)
{
	int ret = 0;

	if (ts_scp_check_is_crash_too_much(&scene) == false) {
		if (ts_scp_check_is_finger_on_touch(&scene) == false) {
			generic_irq_enable(false);
			generic_esd_control(false);
			ts_scp_touch_mode_scp();
			ts_scp_clear_scenario_status(STAT_SCP_CRASH);
			ret = ts_scp_cmd_query_scp_status();
			if ((ret == -EBADMSG) || (ret == -EILSEQ) || (ret == -EPROTO)
				|| (ret == -EREMOTEIO))
				ts_scp_force_switch_to_ap();
		} else {
			touch_on_ap_has_finger_flag = true;
			finger_on_mode = TS_FINIGER_READY_CMD;
			ts_scp_info("Touch work on AP, has touch on touchscreen");
		}
	} else {
		ts_scp_info("scp crash too much, ready process not works");
	}
}

static int ts_scp_ready_thread(void *data)
{
	int ret = 0;

	while (!kthread_should_stop()) {
		ret = wait_for_completion_interruptible(&ts_scp_ready_done);
		if (ret)
			continue;

		ts_scp_ready_process();
	}

	return 0;
}

void ts_scp_scene_process(struct ts_scp_tp_scenario *scene)
{
	int ret = 0;

	if (ts_scp_check_is_crash_too_much(scene) == false) {
		if ((scene->status & STAT_SCP_SUSPEND) && !(scene->status & STAT_SCP_RESUME)) {
			ret = ts_scp_cmd_suspend();
			if ((ret == -EBADMSG) || (ret == -EILSEQ) || (ret == -EPROTO)
				|| (ret == -EREMOTEIO))
				ts_scp_force_switch_to_ap();
			ts_scp_clear_scenario_status(STAT_SCP_SUSPEND);
		}
		if (!(scene->status & STAT_SCP_SUSPEND) && (scene->status & STAT_SCP_RESUME)) {
			ret = ts_scp_cmd_resume();
			if ((ret == -EBADMSG) || (ret == -EILSEQ) || (ret == -EPROTO)
				|| (ret == -EREMOTEIO))
				ts_scp_force_switch_to_ap();
			ts_scp_clear_scenario_status(STAT_SCP_RESUME);
		}
		if ((touch_on_ap_has_finger_flag == true) &&
			(ts_scp_check_is_finger_on_touch(scene) == false)) {
				if (finger_on_mode == TS_FINIGER_READY_CMD) {
					ts_scp_ready_process();
					touch_on_ap_has_finger_flag = false;
					ts_scp_info("AP no touch on touchscreen, auto ready");
				} else if (finger_on_mode == TS_FINIGER_ENABLE_CMD) {
					generic_irq_enable(false);
					generic_esd_control(false);
					ts_scp_touch_mode_scp();
					ts_scp_set_scp_enable();
					ts_scp_clear_scenario_status(STAT_SCP_CRASH);
					ret = ts_scp_cmd_scp_handle();
					if ((ret == -EBADMSG) || (ret == -EILSEQ) || (ret == -EPROTO)
						|| (ret == -EREMOTEIO))
						ts_scp_force_switch_to_ap();
					ts_scp_info("AP no touch on touchscreen, auto enable");
				}
		}
	} else {
		ts_scp_info("scp crash too much, scene process not works");
		if ((scene->status & STAT_SCP_SUSPEND) && !(scene->status & STAT_SCP_RESUME)) {
			ts_scp_clear_scenario_status(STAT_SCP_SUSPEND);
		}
		if (!(scene->status & STAT_SCP_SUSPEND) && (scene->status & STAT_SCP_RESUME)) {
			ts_scp_clear_scenario_status(STAT_SCP_RESUME);
		}
	}
}

static int ts_scp_scene_thread(void *data)
{
	int ret = 0;

	while (!kthread_should_stop()) {
		ret = wait_for_completion_interruptible(&ts_scp_scene_done);
		if (ret)
			continue;

		scene.status_changed = ts_scp_scene_flag_changed();
		if(scene.status_changed)
		{
			ts_scp_scene_process(&scene);
			scene.status_changed = 0;
		}
	}
	return 0;
}

static void ts_scp_force_reset_crash_num(void) {
	atomic_set(&scp_crash_num, 0);
	ts_scp_clear_scenario_status(STAT_SCP_CRASH_TOO_MUCH);
	ts_scp_info("force reset crash num=%d", atomic_read(&scp_crash_num));
}

static int ts_scp_comm_with(int cmd,
		void *data, uint8_t length)
{
	int ret = 0;
	struct touch_comm_ctrl *ctrl = NULL;
	uint32_t ctrl_size = 0;

	if (ts_scp_check_is_crash_too_much(&scene) == false) {
		ctrl_size = ipi_comm_size(sizeof(*ctrl) + length);
		ctrl = kzalloc(ctrl_size, GFP_KERNEL);
		ctrl->touch_id = ts_scp_get_touch_id();
		ctrl->command = cmd;
		ctrl->length = length;
		ts_scp_info("ts_scp_comm_with ctrl_size=%d, cmd=0x%02x, length=%d",
			ctrl_size, cmd, length);
		if (length)
			memcpy(ctrl->data, data, length);
		ret = touch_comm_ctrl_send(ctrl, ctrl_size);
		kfree(ctrl);
		return ret;
	} else {
		ts_scp_info("scp crash too much, not send cmd. cmd=0x%02x", cmd);
		return 0;
	}
}

static int ts_scp_cmd_scp_handle(void)
{
	return ts_scp_comm_with(
		TOUCH_COMM_CTRL_SCP_HANDLE_CMD, NULL, 0);
}

static int ts_scp_cmd_ap_handle(void)
{
	return ts_scp_comm_with(
		TOUCH_COMM_CTRL_AP_HANDLE_CMD, NULL, 0);
}

static int ts_scp_cmd_query_scp_status(void)
{
	return ts_scp_comm_with(
		TOUCH_COMM_CTRL_QUERY_SCP_STATUS_CMD, NULL, 0);
}

static int ts_scp_cmd_suspend(void)
{
	return ts_scp_comm_with(
		TOUCH_COMM_CTRL_SUSPEND_CMD, NULL, 0);
}

static int ts_scp_cmd_resume(void)
{
	return ts_scp_comm_with(
		TOUCH_COMM_CTRL_RESUME_CMD, NULL, 0);
}

static int ts_scp_cmd_reinit(void)
{
	return ts_scp_comm_with(
		TOUCH_COMM_CTRL_REINIT_CMD, NULL, 0);
}

static void ts_scp_touch_mode_ap(void)
{
	int ret;

	ret = pinctrl_select_state(generic_pinctrl->pinctrl,
				generic_pinctrl->touch_mode_ap);
	if (ret < 0)
		ts_scp_err("Failed to select default pinstate, ret:%d", ret);
}

static void ts_scp_touch_mode_scp(void)
{
	int ret;

	ret = pinctrl_select_state(generic_pinctrl->pinctrl,
				generic_pinctrl->touch_mode_scp);
	if (ret < 0)
		ts_scp_err("Failed to select default pinstate, ret:%d", ret);
}

static void ts_scp_force_switch_to_ap(void)
{
	ts_scp_info("ts_scp_force_switch_to_ap");
	ts_scp_touch_mode_ap();
	generic_irq_enable(true);
	generic_esd_control(true);
	ts_scp_set_scp_disable();
}

static int scp_platform_ready_notifier_call(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	if (event == SCP_EVENT_STOP) {
		atomic_inc(&scp_crash_num) ;
		ts_scp_info("scp crash! SCP_EVENT_STOP, crash num=%d", atomic_read(&scp_crash_num));
		if (ts_scp_check_is_crash_too_much(&scene) == false) {
			ts_scp_set_scenario_status(STAT_SCP_CRASH);
			ts_scp_force_switch_to_ap();
			if (atomic_read(&scp_crash_num) > MAX_CRASH_NUM - 1) {
				ts_scp_set_scenario_status(STAT_SCP_CRASH_TOO_MUCH);
			}
		}
	} else if (event == SCP_EVENT_READY) {
		if (ts_scp_check_is_crash_too_much(&scene) == false) {
			ts_scp_info("scp ready! SCP_EVENT_READY");
			ts_scp_set_scenario_status(STAT_SCP_SCP_READY);
			ts_scp_is_scp_touch_need_probe();
		} else {
			ts_scp_clear_scenario_status(STAT_SCP_SCP_READY);
			ts_scp_info("scp crash too much, scp ready status not set");
		}
	}

	return NOTIFY_DONE;
}

static struct notifier_block scp_platform_ready_notifier = {
	.notifier_call = scp_platform_ready_notifier_call,
};

void ts_scp_is_scp_touch_need_probe(void)
{
	int ret = 0;

	if(ts_scp_check_is_ready_probe(&scene))
	{
		ts_scp_notify_status.touch_id = ts_scp_get_touch_id();
		ts_scp_notify_status.touch_type = ts_scp_get_touch_type();
		ts_scp_notify_status.command = TOUCH_COMM_NOTIFY_READY_CMD;
		ts_scp_notify_status.length = 0;
		ret = touch_comm_notify_status_bypass(&ts_scp_notify_status);
		if (ret < 0)
			ts_scp_err("notify ready to scp fail %d", ret);
		else
			ts_scp_info("need scp touch probe");
	}
}
EXPORT_SYMBOL_GPL(ts_scp_is_scp_touch_need_probe);

void ts_scp_set_touch_type_id(uint8_t type, uint8_t id)
{
	ts_scp_notify_status.touch_type = type;
	ts_scp_notify_status.touch_id = id;
	ts_scp_info("set touch type=%d, id=%d", type, id);
}
EXPORT_SYMBOL_GPL(ts_scp_set_touch_type_id);

static uint8_t ts_scp_get_touch_type(void)
{
	ts_scp_info("get touch type, type=%d", ts_scp_notify_status.touch_type);
	return ts_scp_notify_status.touch_type;
}

static uint8_t ts_scp_get_touch_id(void)
{
	ts_scp_info("get touch id, id=%d", ts_scp_notify_status.touch_id);
	return ts_scp_notify_status.touch_id;
}

int64_t ts_scp_timesync(struct ts_scp_device *device)
{
	int64_t origin_boottime, origin_ktime, result_timestamp;
	int64_t bootime_offset_ns, ktime_ns;

	timesync_filter_set(&device->filter,
		device->ts_data->multi_data.scp_timestamp,
		device->ts_data->multi_data.scp_archcounter);
	origin_boottime = ktime_get_boottime_ns();
	origin_ktime = ktime_get_ns();
	bootime_offset_ns = origin_boottime - origin_ktime;
	result_timestamp = device->ts_data->multi_data.scp_timestamp + timesync_filter_get(&device->filter);
	ktime_ns = result_timestamp - bootime_offset_ns;
	//ts_scp_info("boottime=%lld, result=%lld", origin_boottime, result_timestamp);
	//ts_scp_info("result_ktime_ns=%lld, ori_ktime_ns=%lld, bootime_offset_ns=%lld",
	//	ktime_ns, origin_ktime, bootime_offset_ns);

	return ktime_ns;
}
EXPORT_SYMBOL_GPL(ts_scp_timesync);

static int ts_scp_parse_dts(struct ts_scp_device *dev, struct device_node *node)
{
	dev->ts_scp_common_enable = of_property_read_bool(node, "tp-offload-scp-common-enable");

	return 0;
}

static int ts_scp_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *node_dev;
	struct ts_scp_device *dev = &ts_scp_dev;
	struct ts_scp_device *device;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO / 2 };

	ts_scp_info("ts_scp_init in");

	device = devm_kzalloc(&pdev->dev, sizeof(*device), GFP_KERNEL);
	if (!device)
		return -ENOMEM;

	ts_scp_parse_dts(device, pdev->dev.of_node);

	if(device->ts_scp_common_enable == true) {
		ts_scp_info("ts_scp_common_enable true");

		dev->ts_data = kzalloc(sizeof(struct ts_scp_data), GFP_KERNEL);
		if (!dev->ts_data) {
			ts_scp_err("Failed to allocate memory for ts_scp_data\n");
			return -1;
		}

		/* init device node */
		dev->ts_scp_major = register_chrdev(0, "ts_scp", &ts_scp_fops);
		if (dev->ts_scp_major < 0) {
			ts_scp_err("Unable to get major");
			ret = dev->ts_scp_major;
			goto err_exit;
		}
		dev->ts_scp_class = class_create("ts_scp");
		if (IS_ERR(dev->ts_scp_class)) {
			ts_scp_err("Failed to create class");
			ret = PTR_ERR(dev->ts_scp_class);
			goto err_class;
		}
		node_dev = device_create_with_groups(dev->ts_scp_class, NULL,
			MKDEV(dev->ts_scp_major, 0), dev,
			ts_scp_attrs_groups, "ts_scp");
		if (IS_ERR(node_dev)) {
			ts_scp_err("Failed to create device");
			ret = PTR_ERR(node_dev);
			goto err_dev;
		}

		//ts_scp_sysfs_init(dev);
		ts_scp_info("ts_scp_init node create");

		ts_scp_dev.filter.max_diff = 10000000000LL;
		ts_scp_dev.filter.min_diff = 10000000LL;
		ts_scp_dev.filter.bufsize = 16;
		ts_scp_dev.filter.name = "ts_scp";
		ret = timesync_filter_init(&ts_scp_dev.filter);
		if (ret < 0) {
			ts_scp_err("timesync filter init fail %d", ret);
			goto err_task;
		}

		ret = touch_comm_init();
		if (ret < 0) {
			ts_scp_err("touch comm init fail ret = %d", ret);
			goto err_task;
		}

		dev->task = kthread_run(ts_scp_thread, dev, "ts_scp");
		if (IS_ERR(dev->task)) {
			ret = -ENOMEM;
			ts_scp_err("create thread fail %d", ret);
			goto err_task;
		}
		sched_setscheduler_nocheck(dev->task, SCHED_FIFO, &param);

		dev->task_ready = kthread_run(ts_scp_ready_thread, dev, "ts_scp_ready");
		if (IS_ERR(dev->task_ready)) {
			ret = -ENOMEM;
			ts_scp_err("create thread fail %d", ret);
			goto err_task;
		}

		dev->task_scene = kthread_run(ts_scp_scene_thread, dev, "ts_scp_scene");
		if (IS_ERR(dev->task_scene)) {
			ret = -ENOMEM;
			ts_scp_err("create scene thread fail %d", ret);
			goto err_task;
		}

		touch_comm_notify_handler_register(TOUCH_COMM_NOTIFY_DATA_CMD,
			ts_scp_data_notify_func, dev);

		touch_comm_notify_handler_register(TOUCH_COMM_NOTIFY_READY_CMD,
			ts_scp_ready_notify_func, dev);

		scp_A_register_notify(&scp_platform_ready_notifier);

		ts_scp_info("ts_scp_init success");
		return 0;

	err_task:
		device_destroy(dev->ts_scp_class, MKDEV(dev->ts_scp_major, 0));
	err_dev:
		class_destroy(dev->ts_scp_class);
	err_class:
		unregister_chrdev(dev->ts_scp_major, "ts_scp");
	err_exit:
		return ret;
	} else {
		ts_scp_info("ts_scp_common_enable false");
		return 0;
	}
}

static void ts_scp_remove(struct platform_device *pdev)
{
	struct ts_scp_device *dev = &ts_scp_dev;
	//struct ts_scp_device *dev = platform_get_drvdata(pdev);

	scp_A_unregister_notify(&scp_platform_ready_notifier);
	device_destroy(dev->ts_scp_class, MKDEV(dev->ts_scp_major, 0));
	class_destroy(dev->ts_scp_class);
	unregister_chrdev(dev->ts_scp_major, "ts_scp");

	//ts_scp_sysfs_exit(dev);

	touch_comm_notify_handler_unregister(TOUCH_COMM_NOTIFY_DATA_CMD);
	touch_comm_notify_handler_unregister(TOUCH_COMM_NOTIFY_READY_CMD);
	if (!IS_ERR_OR_NULL(dev->task))
		kthread_stop(dev->task);
}

static const struct of_device_id ts_scp_of_match[] = {
	{ .compatible = "mediatek,tp-offload-scp" },
	{},
};
MODULE_DEVICE_TABLE(of, ts_scp_of_match);

static struct platform_driver ts_scp_driver = {
	.probe = ts_scp_probe,
	.remove = ts_scp_remove,
	.driver = {
		.name = "ts-scp",
		.of_match_table = ts_scp_of_match,
	},
};

module_platform_driver(ts_scp_driver);

MODULE_DESCRIPTION("touch scp");
MODULE_AUTHOR("Mediatek");
MODULE_LICENSE("GPL v2");
