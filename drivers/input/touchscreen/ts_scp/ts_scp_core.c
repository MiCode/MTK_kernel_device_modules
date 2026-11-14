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
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
#include "scp_rv.h"
#endif
#if IS_ENABLED(CONFIG_MTK_SENSORHUB)
#include "timesync.h"
#endif
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 38)
#include <linux/input/mt.h>
#define INPUT_TYPE_B_PROTOCOL
#endif

struct ts_scp_option_cmd {
    unsigned int status;
    uint8_t cmd;
    uint8_t touch_type;
    uint8_t data[TOUCH_COMM_CTRL_DATA_MAX];
};

struct ts_scp_core_state {
    unsigned int tp_state[MAX_TS_TOUCH_TYPE];
    unsigned int scp_state;
    spinlock_t lock;
};

struct ts_scp_device {
    struct task_struct *task_cmd;
    struct task_struct *task_data;
    struct input_dev *input_dev;
#if IS_ENABLED(CONFIG_MTK_SENSORHUB)
    struct timesync_filter filter;
#endif
    int ts_scp_major;
    struct class *ts_scp_class;
    struct device *device;
    bool ts_scp_common_enable;
    struct tp_offload_scp *tp[MAX_TS_TOUCH_TYPE];
    struct ts_scp_core_state core_state;
    struct hrtimer after_scp_reset_timer;
    atomic_t scp_crash_num_from_boot;
    atomic_t scp_crash_num;
};

DEFINE_SPINLOCK(ts_scp_data_fifo_lock);
DECLARE_COMPLETION(ts_scp_data_done);
DEFINE_KFIFO(ts_scp_data_fifo, struct ts_scp_data, 16);

DEFINE_SPINLOCK(ts_scp_cmd_fifo_lock);
DECLARE_COMPLETION(ts_scp_cmd_done);
DEFINE_KFIFO(ts_scp_cmd_fifo, struct ts_scp_option_cmd, 8);

bool debug_log_flag = false;
struct ts_scp_node_ctrl ctrl_cmd;

struct ts_scp_device ts_scp_dev;
static DEFINE_MUTEX(tp_mutex);

static void ts_scp_set_tp_status(uint8_t touch_type, unsigned int state);
static void ts_scp_clear_tp_status(uint8_t touch_type, unsigned int state);
static int ts_scp_get_tp_status(uint8_t touch_type);
void ts_scp_cmd_handler_async(struct ts_scp_cmd *option_cmd);

int ts_scp_request_offload(struct tp_offload_scp *tp)
{
    int ret = 0;
    struct ts_scp_device *dev = &ts_scp_dev;
    uint8_t touch_type;

    if (!tp)
        return -EINVAL;

    touch_type = tp->touch_type;
    if (touch_type >= MAX_TS_TOUCH_TYPE || touch_type == TS_TYPE_INVALID)
        return -EINVAL;

    mutex_lock(&tp_mutex);
    if (dev->tp[touch_type]) {
        ts_scp_err("Repeat touch type[%u]tp offload requested old %s new %s",
            touch_type, dev->tp[touch_type]->name, tp->name);
        ret = -EFAULT;
        mutex_unlock(&tp_mutex);
        return ret;
    }
    /* init */
    dev->tp[touch_type] = tp;
    mutex_unlock(&tp_mutex);
    ts_scp_set_tp_status(touch_type, STAT_AP_TP_READY);

    return ret;
}
EXPORT_SYMBOL(ts_scp_request_offload);

void ts_scp_release_offload(struct tp_offload_scp *tp)
{
    struct ts_scp_device *dev = &ts_scp_dev;
    uint8_t touch_type;

    if (!tp)
        return;

    touch_type = tp->touch_type;
    if (touch_type >= MAX_TS_TOUCH_TYPE || touch_type == TS_TYPE_INVALID)
        return;

    mutex_lock(&tp_mutex);
    if (!dev->tp[touch_type] || (dev->tp[touch_type] != tp))
        goto out;
    dev->tp[touch_type] = NULL;
    ts_scp_clear_tp_status(touch_type, STAT_AP_TP_READY);
out:
    mutex_unlock(&tp_mutex);
}
EXPORT_SYMBOL(ts_scp_release_offload);

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
static ssize_t ts_scp_touch_state_show(struct device *dev,
                       struct device_attribute *attr,
                       char *buf)
{
    int r = 0;
    bool touch_stat;
    struct ts_scp_device *tp_dev = &ts_scp_dev;

    for (uint8_t i = 0; i < MAX_TS_TOUCH_TYPE; i++) {
        if (tp_dev->tp[i] != NULL) {
            touch_stat = STAT_TP_WORK_MODE & ts_scp_get_tp_status(i);
            r = sprintf(buf, "Touch type:%u, touch side:%s\n", i, touch_stat ? "scp" : "ap");
            if (r < 0)
                ts_scp_err("buf print fail");
        }
    }

    return r;
}

/* ctrl cmd show */
static ssize_t ts_scp_ctrl_show(struct device *dev,
                       struct device_attribute *attr,
                       char *buf)
{
    int r = 0;

    r = sprintf(buf, "ctrl cmd: %u,%u\n", ctrl_cmd.touch_type, ctrl_cmd.cmd);
    if (r < 0)
        ts_scp_err("buf print fail");

    return r;
}

/* ctrl cmd store */
static ssize_t ts_scp_ctrl_store(struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    unsigned int touch_type = TS_TYPE_INVALID;
    unsigned int cmd = 0;
    int ret;
    struct ts_scp_cmd option_cmd;
    struct ts_scp_device *tp_dev = &ts_scp_dev;

    ret = sscanf(buf, "%u,%u", &touch_type, &cmd);
    if (ret != 2)
        return -EINVAL;
    ctrl_cmd.touch_type = touch_type;
    ctrl_cmd.cmd = cmd;
    ts_scp_info("ts_scp_ctrl_store Touch_type = %u, cmd = %u", touch_type, cmd);
	memset(&option_cmd, 0 , sizeof(struct ts_scp_cmd));

    switch (cmd) {
    case 0:
        option_cmd.command = TOUCH_COMM_CTRL_SCP_HANDLE_CMD;
        option_cmd.touch_type = touch_type;
        ts_scp_cmd_handler_async(&option_cmd);
        break;
    case 1:
        option_cmd.command = TOUCH_COMM_CTRL_AP_HANDLE_CMD;
        option_cmd.touch_type = touch_type;
        ts_scp_cmd_handler_async(&option_cmd);
        break;
    case 3:
        option_cmd.command = TOUCH_COMM_CTRL_SUSPEND_CMD;
        option_cmd.touch_type = touch_type;
        ts_scp_cmd_handler_async(&option_cmd);
        break;
    case 4:
        option_cmd.command = TOUCH_COMM_CTRL_RESUME_CMD;
        option_cmd.touch_type = touch_type;
        ts_scp_cmd_handler_async(&option_cmd);
        break;
    case 5:
        option_cmd.command = TOUCH_COMM_CTRL_REINIT_CMD;
        option_cmd.touch_type = touch_type;
        ts_scp_cmd_handler_async(&option_cmd);
        break;
    case 6:
        atomic_set(&tp_dev->scp_crash_num_from_boot, 0);
        break;
    default:
        ts_scp_err("ctrl node receive error cmd");
        break;
    }
    return count;
}

static DEVICE_ATTR(debug_log, 0660,
        ts_scp_debug_log_show, ts_scp_debug_log_store);
static DEVICE_ATTR(touch_stat, 0440,
        ts_scp_touch_state_show, NULL);
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
        if (size != sizeof(ctrl))
            return -EINVAL;
        if (copy_from_user(&ctrl, ubuf, size))
            return -EFAULT;

        ctrl_cmd.touch_type = ctrl.touch_type;
        ctrl_cmd.cmd = ctrl.cmd;
        break;
    default:
        ts_scp_err("Unknown command %u", cmd);
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

#if IS_ENABLED(CONFIG_MTK_SENSORHUB)
int64_t ts_scp_generate_timestamp(int64_t scp_timestamp, int64_t scp_archcounter)
{
    int64_t origin_boottime, origin_ktime, result_timestamp;
    int64_t bootime_offset_ns, ktime_ns;
    struct ts_scp_device *dev = &ts_scp_dev;

    timesync_filter_set(&dev->filter, scp_timestamp, scp_archcounter);
    origin_boottime = ktime_get_boottime_ns();
    origin_ktime = ktime_get_ns();
    bootime_offset_ns = origin_boottime - origin_ktime;
    result_timestamp = scp_timestamp + timesync_filter_get(&dev->filter);
    ktime_ns = result_timestamp - bootime_offset_ns;

    return ktime_ns;
}
#else
int64_t ts_scp_generate_timestamp(int64_t scp_timestamp, int64_t scp_archcounter)
{
    return 0;
}
#endif
EXPORT_SYMBOL(ts_scp_generate_timestamp);

static void ts_scp_data_notify_func(struct touch_comm_notify *n,
        void *private_data)
{
    struct ts_scp_data tp_data;
    unsigned long flags = 0;

    if (n->command != TOUCH_COMM_NOTIFY_DATA_CMD)
        return;

    tp_data.touch_type = n->touch_type;
    memcpy(tp_data.data, n->value,
        sizeof(int32_t)*TOUCH_COMM_DATA_MAX);

    spin_lock_irqsave(&ts_scp_data_fifo_lock, flags);
    if (kfifo_is_full(&ts_scp_data_fifo)) {
        ts_scp_err("ts_scp data fifo full");
        goto error;
    }
    kfifo_in(&ts_scp_data_fifo, &tp_data, 1);
    complete(&ts_scp_data_done);
error:
    spin_unlock_irqrestore(&ts_scp_data_fifo_lock, flags);
}

static void ts_scp_data_process(void)
{
    unsigned int ret = 0;
    struct ts_scp_data tp_data;
    unsigned long flags = 0;

    while (1) {
        spin_lock_irqsave(&ts_scp_data_fifo_lock, flags);
        ret = kfifo_out(&ts_scp_data_fifo, &tp_data, 1);
        spin_unlock_irqrestore(&ts_scp_data_fifo_lock, flags);
        if (!ret)
            break;

        mutex_lock(&tp_mutex);
        if (ts_scp_dev.tp[tp_data.touch_type]->report_func != NULL)
            ts_scp_dev.tp[tp_data.touch_type]->report_func(&tp_data);
        mutex_unlock(&tp_mutex);
    }
}

static int ts_scp_data_thread(void *data)
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

void ts_scp_cmd_handler_async(struct ts_scp_cmd *option_cmd)
{
    struct ts_scp_option_cmd option_data;
    unsigned long flags = 0;

	memset(&option_data, 0 , sizeof(struct ts_scp_option_cmd));
    option_data.cmd = option_cmd->command;
    option_data.touch_type = option_cmd->touch_type;
    memcpy(&option_data.data, &option_cmd->data, sizeof(option_data.data));

    spin_lock_irqsave(&ts_scp_cmd_fifo_lock, flags);
    if (kfifo_is_full(&ts_scp_cmd_fifo)) {
        ts_scp_err("ts_scp cmd fifo full, drop cmd-%u for touch type-%u", option_cmd->command, option_cmd->touch_type);
        goto error;
    }
    kfifo_in(&ts_scp_cmd_fifo, &option_data, 1);
    complete(&ts_scp_cmd_done);
error:
    spin_unlock_irqrestore(&ts_scp_cmd_fifo_lock, flags);
}
EXPORT_SYMBOL(ts_scp_cmd_handler_async);

static void ts_scp_set_tp_status(uint8_t touch_type, unsigned int state)
{
    struct ts_scp_device *dev = &ts_scp_dev;
    unsigned long flags;
    struct ts_scp_cmd cmd_data;

    spin_lock_irqsave(&dev->core_state.lock, flags);
    dev->core_state.tp_state[touch_type] = dev->core_state.tp_state[touch_type] | state;
    ts_scp_info("Update core tp:%u state:%04x", touch_type, dev->core_state.tp_state[touch_type]);
    spin_unlock_irqrestore(&dev->core_state.lock, flags);

	memset(&cmd_data, 0 , sizeof(struct ts_scp_cmd));
    cmd_data.touch_type = touch_type;
    cmd_data.command = TOUCH_COMM_CTRL_STATUS_UPDATE;
    ts_scp_cmd_handler_async(&cmd_data);
}

static void ts_scp_clear_tp_status(uint8_t touch_type, unsigned int state)
{
    struct ts_scp_device *dev = &ts_scp_dev;
    unsigned long flags;
    struct ts_scp_cmd cmd_data;

    spin_lock_irqsave(&dev->core_state.lock, flags);
    dev->core_state.tp_state[touch_type] = dev->core_state.tp_state[touch_type] & (~(state));
    ts_scp_info("Update core tp:%u state:%04x", touch_type, dev->core_state.tp_state[touch_type]);
    spin_unlock_irqrestore(&dev->core_state.lock, flags);

	memset(&cmd_data, 0 , sizeof(struct ts_scp_cmd));
    cmd_data.touch_type = touch_type;
    cmd_data.command = TOUCH_COMM_CTRL_STATUS_UPDATE;
    ts_scp_cmd_handler_async(&cmd_data);
}

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
static void ts_scp_set_scp_status(unsigned int state)
{
    struct ts_scp_device *dev = &ts_scp_dev;
    unsigned long flags;
    struct ts_scp_cmd cmd_data;

    spin_lock_irqsave(&dev->core_state.lock, flags);
    dev->core_state.scp_state = dev->core_state.scp_state | state;
    ts_scp_info("Update core scp state:%04x", dev->core_state.scp_state);
    spin_unlock_irqrestore(&dev->core_state.lock, flags);

    for (uint8_t i = 0; i < MAX_TS_TOUCH_TYPE; i++) {
        if (dev->tp[i] != NULL) {
		memset(&cmd_data, 0 , sizeof(struct ts_scp_cmd));
            cmd_data.touch_type = i;
            cmd_data.command = TOUCH_COMM_CTRL_STATUS_UPDATE;
            ts_scp_cmd_handler_async(&cmd_data);
        }
    }
}
#endif

static void ts_scp_clear_scp_status(unsigned int state)
{
    struct ts_scp_device *dev = &ts_scp_dev;
    unsigned long flags;
    struct ts_scp_cmd cmd_data;

    spin_lock_irqsave(&dev->core_state.lock, flags);
    dev->core_state.scp_state = dev->core_state.scp_state & (~(state));
    ts_scp_info("Update core scp state:%04x", dev->core_state.scp_state);
    spin_unlock_irqrestore(&dev->core_state.lock, flags);

    for (uint8_t i = 0; i < MAX_TS_TOUCH_TYPE; i++) {
        if (dev->tp[i] != NULL) {
		memset(&cmd_data, 0 , sizeof(struct ts_scp_cmd));
            cmd_data.touch_type = i;
            cmd_data.command = TOUCH_COMM_CTRL_STATUS_UPDATE;
            ts_scp_cmd_handler_async(&cmd_data);
        }
    }
}
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
static void ts_scp_reset_update_status(void)
{
    struct ts_scp_device *dev = &ts_scp_dev;
    unsigned long flags;
    struct ts_scp_cmd cmd_data;

    spin_lock_irqsave(&dev->core_state.lock, flags);
    dev->core_state.scp_state = dev->core_state.scp_state & (~STAT_SCP_STATE_READY);
    dev->core_state.scp_state = dev->core_state.scp_state | STAT_SCP_STATE_CRASH_DURATION;
    ts_scp_info("Update core scp state:%04x", dev->core_state.scp_state);
    for (uint8_t i = 0; i < MAX_TS_TOUCH_TYPE; i++) {
        if (dev->tp[i] != NULL) {
            dev->core_state.tp_state[i]  = dev->core_state.tp_state[i] & (~(STAT_SCP_TP_INIT_READY | STAT_SCP_TP_WORK_STATE));
            ts_scp_info("Update core tp:%u state:%04x", i, dev->core_state.tp_state[i]);
        }
    }
    spin_unlock_irqrestore(&dev->core_state.lock, flags);

    for (uint8_t i = 0; i < MAX_TS_TOUCH_TYPE; i++) {
        if (dev->tp[i] != NULL) {
		memset(&cmd_data, 0 , sizeof(struct ts_scp_cmd));
            cmd_data.touch_type = i;
            cmd_data.command = TOUCH_COMM_CTRL_STATUS_UPDATE;
            ts_scp_cmd_handler_async(&cmd_data);
        }
    }
}
#endif
static int ts_scp_get_tp_status(uint8_t touch_type)
{
    struct ts_scp_device *dev = &ts_scp_dev;
    unsigned long flags;
    unsigned int current_tp_status;

    spin_lock_irqsave(&dev->core_state.lock, flags);
    current_tp_status = dev->core_state.tp_state[touch_type];
    spin_unlock_irqrestore(&dev->core_state.lock, flags);

    return current_tp_status;
}
/*
static int ts_scp_get_scp_status(void)
{
    struct ts_scp_device *dev = &ts_scp_dev;
    unsigned long flags;
    unsigned int current_scp_status;

    spin_lock_irqsave(&dev->core_state.lock, flags);
    current_scp_status = dev->core_state.scp_state;
    spin_unlock_irqrestore(&dev->core_state.lock, flags);

    return current_scp_status;
}
*/
int ts_scp_offload_check_status(uint8_t touch_type)
{
    bool touch_state;

    touch_state = STAT_TP_WORK_MODE & ts_scp_get_tp_status(touch_type);
    return touch_state ? 0 : -1;
}
EXPORT_SYMBOL(ts_scp_offload_check_status);

int ts_scp_cmd_handler_sync(struct ts_scp_cmd *option_cmd)
{
    int ret = -1;
    struct ts_scp_device *dev = &ts_scp_dev;
    uint8_t touch_type = option_cmd->touch_type;
    uint8_t current_cmd = option_cmd->command;

    switch (current_cmd) {
        case TOUCH_COMM_CTRL_RESUME_CMD:
        case TOUCH_COMM_CTRL_SUSPEND_CMD:
        case TOUCH_COMM_CTRL_REINIT_CMD:
        case TOUCH_COMM_CTRL_CHANGE_REPORT_RATE_CMD:
            if (!ts_scp_offload_check_status(touch_type)) {
                ret = touch_comm_cmd_send(touch_type, current_cmd, option_cmd->data, sizeof(option_cmd->data));
                if (ret) {
                    ts_scp_info("cmd-%u failed! force change to ap", current_cmd);
                    mutex_lock(&tp_mutex);
                    if (dev->tp[touch_type]->offload_scp != NULL)
                        dev->tp[touch_type]->offload_scp(dev->tp[touch_type], false);
                    mutex_unlock(&tp_mutex);
                    ts_scp_clear_tp_status(touch_type, STAT_TP_WORK_MODE);
                }
            } else {
                ts_scp_info("tp work not scp");
            }
            break;
        case TOUCH_COMM_CTRL_SCP_HANDLE_CMD:
            if (ts_scp_offload_check_status(touch_type)) {
                mutex_lock(&tp_mutex);
                if (dev->tp[touch_type]->offload_scp != NULL) {
                    ret = dev->tp[touch_type]->offload_scp(dev->tp[touch_type], true);
                    if (!ret) {
                        ret = touch_comm_cmd_send(touch_type, TOUCH_COMM_CTRL_SCP_HANDLE_CMD, option_cmd->data, 0);
                        if (!ret) {
                            ts_scp_info("tp work mode change to scp");
                            ts_scp_set_tp_status(touch_type, STAT_TP_WORK_MODE);
                        } else {
                            dev->tp[touch_type]->offload_scp(dev->tp[touch_type], false);
                        }
                    } else {
                        dev->tp[touch_type]->offload_scp(dev->tp[touch_type], false);
                    }
                }
                mutex_unlock(&tp_mutex);
            } else {
                ts_scp_info("tp work alread in scp not to do cmd: TOUCH_COMM_CTRL_SCP_HANDLE_CMD");
            }
            break;
        case TOUCH_COMM_CTRL_AP_HANDLE_CMD:
            if (!ts_scp_offload_check_status(touch_type)) {
                mutex_lock(&tp_mutex);
                ret = touch_comm_cmd_send(touch_type, TOUCH_COMM_CTRL_AP_HANDLE_CMD, option_cmd->data, 0);
                if (!ret) {
                    if (dev->tp[touch_type]->offload_scp != NULL)
                        dev->tp[touch_type]->offload_scp(dev->tp[touch_type], false);
                    ts_scp_info("tp work mode change to AP");
                } else {
                    ts_scp_info("cmd of tp work mode change to AP failed! force change to ap");
                    if (dev->tp[touch_type]->offload_scp != NULL)
                        dev->tp[touch_type]->offload_scp(dev->tp[touch_type], false);
                }
                ts_scp_clear_tp_status(touch_type, STAT_TP_WORK_MODE);
                mutex_unlock(&tp_mutex);
            } else {
                ts_scp_info("tp work alread in ap not to da cmd: TOUCH_COMM_CTRL_AP_HANDLE_CMD");
            }
            break;
        default:
            break;
    }

    return ret;
}
EXPORT_SYMBOL(ts_scp_cmd_handler_sync);

static void ts_scp_cmd_onceprocess(struct ts_scp_option_cmd *option_data)
{
    uint8_t current_cmd = option_data->cmd;
    uint8_t touch_type = option_data->touch_type;
    unsigned int current_scp_status, current_tp_status;
    unsigned long flags;
    struct ts_scp_device *dev = &ts_scp_dev;
    struct ts_scp_cmd cmd_data;
    int ret;

    spin_lock_irqsave(&dev->core_state.lock, flags);
    current_scp_status = dev->core_state.scp_state;
    current_tp_status = dev->core_state.tp_state[touch_type];
    spin_unlock_irqrestore(&dev->core_state.lock, flags);

    switch (current_cmd) {
        case TOUCH_COMM_CTRL_STATUS_UPDATE:
            if ((STAT_SCP_STATE_READY & current_scp_status)
            && (STAT_AP_TP_READY & current_tp_status)
            && (!(STAT_SCP_STATE_CRASH_TOO_MUCH & current_scp_status))
            && (!(STAT_SCP_TP_INIT_READY & current_tp_status))
            && (!(STAT_SCP_TP_WORK_STATE & current_tp_status))) {
                touch_comm_data_notify(touch_type, dev->tp[touch_type]->touch_id, TOUCH_COMM_NOTIFY_READY_CMD, NULL, 0);
		} else if ((STAT_SCP_STATE_READY & current_scp_status)
            && (STAT_AP_TP_READY & current_tp_status)
            && (!(STAT_SCP_STATE_CRASH_TOO_MUCH & current_scp_status))
            && (STAT_SCP_TP_INIT_READY & current_tp_status)
            && (!(STAT_SCP_TP_WORK_STATE & current_tp_status))
            && (!(STAT_SCP_STATE_CRASH_DURATION & current_scp_status))) {
                mutex_lock(&tp_mutex);
                if (dev->tp[touch_type]->offload_scp != NULL) {
                    ret = dev->tp[touch_type]->offload_scp(dev->tp[touch_type], true);
                    if (!ret) {
                        ret = touch_comm_cmd_send(touch_type, TOUCH_COMM_CTRL_QUERY_SCP_STATUS_CMD, cmd_data.data, 0);
                        if (!ret) {
                            ts_scp_set_tp_status(touch_type, STAT_SCP_TP_WORK_STATE);
                            ts_scp_set_tp_status(touch_type, STAT_TP_WORK_MODE);
                        } else {
                            ts_scp_err("query scp tp work failed");
                            dev->tp[touch_type]->offload_scp(dev->tp[touch_type], false);
                        }
                    } else {
                        dev->tp[touch_type]->offload_scp(dev->tp[touch_type], false);
                    }
                }
                mutex_unlock(&tp_mutex);
            } else if(STAT_SCP_STATE_CRASH_TOO_MUCH & current_scp_status) {
                if (!ts_scp_offload_check_status(touch_type)) {
                    mutex_lock(&tp_mutex);
                    if (dev->tp[touch_type]->offload_scp != NULL)
                        dev->tp[touch_type]->offload_scp(dev->tp[touch_type], false);
                    mutex_unlock(&tp_mutex);
                    ts_scp_clear_tp_status(touch_type, STAT_TP_WORK_MODE);
                }
            } else if(! (STAT_SCP_STATE_READY & current_scp_status)) {
                if (!ts_scp_offload_check_status(touch_type)) {
                    mutex_lock(&tp_mutex);
                    if (dev->tp[touch_type]->offload_scp != NULL)
                        dev->tp[touch_type]->offload_scp(dev->tp[touch_type], false);
                    mutex_unlock(&tp_mutex);
                    ts_scp_clear_tp_status(touch_type, STAT_TP_WORK_MODE);
                }
            }
            break;
        case TOUCH_COMM_CTRL_SCP_HANDLE_CMD:
        case TOUCH_COMM_CTRL_AP_HANDLE_CMD:
        case TOUCH_COMM_CTRL_SUSPEND_CMD:
        case TOUCH_COMM_CTRL_RESUME_CMD:
        case TOUCH_COMM_CTRL_REINIT_CMD:
        case TOUCH_COMM_CTRL_CHANGE_REPORT_RATE_CMD:
            cmd_data.command = current_cmd;
            cmd_data.touch_type = touch_type;
            memcpy(&cmd_data.data, &option_data->data, sizeof(cmd_data.data));
            ts_scp_cmd_handler_sync(&cmd_data);
            break;
        default:
            break;
    }
    return;
}

static void ts_scp_cmd_process(void)
{
    unsigned int ret = 0;
    struct ts_scp_option_cmd option_data;
    unsigned long flags = 0;

    while (1) {
        spin_lock_irqsave(&ts_scp_cmd_fifo_lock, flags);
        ret = kfifo_out(&ts_scp_cmd_fifo, &option_data, 1);
        spin_unlock_irqrestore(&ts_scp_cmd_fifo_lock, flags);
        if (!ret) {
            break;
        }
        ts_scp_cmd_onceprocess(&option_data);
    }
}

static int ts_scp_cmd_thread(void *data)
{
    int ret = 0;

    while (!kthread_should_stop()) {
        ret = wait_for_completion_interruptible(&ts_scp_cmd_done);
        if (ret)
            continue;

        ts_scp_cmd_process();
    }
    return 0;
}

static void ts_scp_ready_notify_func(struct touch_comm_notify *n,
        void *private_data)
{
    if (n->command != TOUCH_COMM_NOTIFY_READY_CMD)
        return;
    ts_scp_set_tp_status(n->touch_type, STAT_SCP_TP_INIT_READY);
}

static enum hrtimer_restart ts_after_scp_reset_timer_func(struct hrtimer *timer)
{
    struct ts_scp_device *dev = &ts_scp_dev;

    if (atomic_read(&dev->scp_crash_num) < (MAX_CRASH_NUM_ONE_DURATION + 1))
        ts_scp_clear_scp_status(STAT_SCP_STATE_CRASH_DURATION);
    ts_scp_info("scp crash after timer expire");
    atomic_set(&dev->scp_crash_num, 0);
    return HRTIMER_NORESTART;
}
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
static int scp_platform_ready_notifier_call(struct notifier_block *this,
        unsigned long event, void *ptr)
{
    struct ts_scp_device *dev = &ts_scp_dev;

    if (event == SCP_EVENT_STOP) {
        atomic_inc(&dev->scp_crash_num_from_boot);
        atomic_inc(&dev->scp_crash_num);
        ts_scp_info("scp crash! SCP_EVENT_STOP, crash num=%d", atomic_read(&dev->scp_crash_num_from_boot));
        ts_scp_reset_update_status();
        if (!hrtimer_active(&dev->after_scp_reset_timer))
            hrtimer_start(&dev->after_scp_reset_timer, ns_to_ktime(TS_AFTER_SCP_REST_TIME), HRTIMER_MODE_REL);

        if (atomic_read(&dev->scp_crash_num_from_boot) > MAX_CRASH_NUM - 1) {
            ts_scp_set_scp_status(STAT_SCP_STATE_CRASH_TOO_MUCH);
        }
    } else if (event == SCP_EVENT_READY) {
        ts_scp_info("scp ready! SCP_EVENT_READY");
        ts_scp_set_scp_status(STAT_SCP_STATE_READY);
    }

    return NOTIFY_DONE;
}

static struct notifier_block scp_platform_ready_notifier = {
    .notifier_call = scp_platform_ready_notifier_call,
};
#endif
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

        ts_scp_info("ts_scp_init node create");
#if IS_ENABLED(CONFIG_MTK_SENSORHUB)
        ts_scp_dev.filter.max_diff = 10000000000LL;
        ts_scp_dev.filter.min_diff = 10000000LL;
        ts_scp_dev.filter.bufsize = 16;
        ts_scp_dev.filter.name = "ts_scp";
        ret = timesync_filter_init(&ts_scp_dev.filter);
        if (ret < 0) {
            ts_scp_err("timesync filter init fail %d", ret);
            goto err_task;
        }
#endif
        spin_lock_init(&dev->core_state.lock);
        dev->core_state.scp_state = STAT_NONE;
        ret = touch_comm_init();
        if (ret < 0) {
            ts_scp_err("touch comm init fail ret = %d", ret);
            goto err_task;
        }

        dev->task_data = kthread_run(ts_scp_data_thread, dev, "ts_scp_data");
        if (IS_ERR(dev->task_data)) {
            ret = -ENOMEM;
            ts_scp_err("create thread fail %d", ret);
            goto err_task;
        }
        sched_setscheduler_nocheck(dev->task_data, SCHED_FIFO, &param);

        dev->task_cmd = kthread_run(ts_scp_cmd_thread, dev, "ts_scp_cmd");
        if (IS_ERR(dev->task_cmd)) {
            ret = -ENOMEM;
            ts_scp_err("create thread fail %d", ret);
            goto err_task;
        }
        sched_setscheduler_nocheck(dev->task_cmd, SCHED_FIFO, &param);

        /* init after scp reset timer */
        hrtimer_init(&dev->after_scp_reset_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
        dev->after_scp_reset_timer.function = ts_after_scp_reset_timer_func;

        atomic_set(&dev->scp_crash_num_from_boot, 0);
        atomic_set(&dev->scp_crash_num, 0);

        touch_comm_notify_handler_register(TOUCH_COMM_NOTIFY_DATA_CMD,
            ts_scp_data_notify_func, dev);

        touch_comm_notify_handler_register(TOUCH_COMM_NOTIFY_READY_CMD,
            ts_scp_ready_notify_func, dev);
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
        scp_A_register_notify(&scp_platform_ready_notifier);
#endif
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
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
    scp_A_unregister_notify(&scp_platform_ready_notifier);
#endif
    device_destroy(dev->ts_scp_class, MKDEV(dev->ts_scp_major, 0));
    class_destroy(dev->ts_scp_class);
    unregister_chrdev(dev->ts_scp_major, "ts_scp");

    touch_comm_notify_handler_unregister(TOUCH_COMM_NOTIFY_DATA_CMD);
    touch_comm_notify_handler_unregister(TOUCH_COMM_NOTIFY_READY_CMD);
    if (!IS_ERR_OR_NULL(dev->task_cmd))
        kthread_stop(dev->task_cmd);
    if (!IS_ERR_OR_NULL(dev->task_data))
        kthread_stop(dev->task_data);
    hrtimer_cancel(&dev->after_scp_reset_timer);
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
