/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/
// MIUI ADD: Camera_CameraSkyNet
#define DEBUG
#define pr_fmt(fmt)     KBUILD_MODNAME ": " fmt
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/ktime.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#include <linux/poll.h>
#include <linux/kfifo.h>
#include "cam_log.h"
/*device name after register in charater*/
#define CAMLOG_DEV_NAME      "camlog"
#define CAMLOG_DEV_VERSION "2.0"
static struct camlog_dev  s_read_buf;
static struct camlog_dev  s_write_buf;

static struct mutex camlog_message_lock;
static  wait_queue_head_t camlog_is_not_empty;
static dev_t devt;
static  int debugLevel = 0;
static struct kfifo message_kfifo;

#define MY_IOCTL_TYPE 'M'

#define IOCTL_CMD_LOG_SWITCH _IOW(MY_IOCTL_TYPE, 1, int)
#define IOCTL_CMD_DUMP _IOW(MY_IOCTL_TYPE, 2, int)
#define IOCTL_CMD_SET_DEBUG_LEVEL _IOW(MY_IOCTL_TYPE, 3, int)
#define IOCTL_CMD_READ_KFIFO_CNT _IOR(MY_IOCTL_TYPE, 4, int)
#define IOCTL_CMD_READ_LAST_MSG _IOR(MY_IOCTL_TYPE, 5, char*)
#define IOCTL_CMD_SET_DEBUG_ON _IOW(MY_IOCTL_TYPE, 6, int)
#define IOCTL_CMD_SET_DEBUG_OFF _IOW(MY_IOCTL_TYPE, 7, int)

static uint8_t s_last_message[MESSAGE_MAX];//debug used only
static uid_t s_last_caller_uid  = 0;
static pid_t s_last_caller_pid = 0;

static uid_t s_last_reader_uid  = 0;
static pid_t s_last_reader_pid = 0;

static void dump(void) {
    struct camlog_dev  element;
    int message_cnt = 0;
    int i = 0;
    int ret = 0;
    pr_debug("----------------- DUMP CAMERALOG START------------------------");
    pr_debug("cameralog version %s", CAMLOG_DEV_VERSION);

    pr_debug(" last writer uid:%u  pid:%u", s_last_caller_uid, s_last_caller_pid);
    pr_debug(" last reader uid:%u  pid:%u", s_last_reader_uid, s_last_reader_pid);
    pr_debug(" last message:%s ",  s_last_message);

    message_cnt = kfifo_len(&message_kfifo);
    pr_debug(" kfifo len:%d ", message_cnt);
    if (message_cnt > 0) {
        pr_debug("-------fifo messages-------");
        pr_debug("UID\tPID\tMESSAGE");
        for (i = 0; i < message_cnt; i++) {
            ret = kfifo_out(&message_kfifo, &element, sizeof(struct camlog_dev));
            if (ret != sizeof(struct camlog_dev)) {
                break;
            }
            pr_debug("  %u\t%u\t %s\n", element.writer_uid, element.writer_pid,  element.m_camlog_message);
        }
        pr_debug("-------------------------------");
    }
    pr_debug("----------------- DUMP CAMERALOG END   ------------------------");
}

static long cameralog_ioctl(struct file *filp, unsigned int cmd,  unsigned long arg) {
    pr_debug(" cameralog_ioctl! %u  %lu\n", cmd,  arg);
    switch (cmd) {
        case IOCTL_CMD_LOG_SWITCH:
            {
                if (0 >= debugLevel) {
                    debugLevel = 1;
                } else {
                    debugLevel = 0;
                }
            }
            break;
        case IOCTL_CMD_SET_DEBUG_ON:
            {
                debugLevel = 1;
            }
            break;
        case IOCTL_CMD_SET_DEBUG_OFF:
            {
                debugLevel = 0;
            }
            break;
        case IOCTL_CMD_DUMP:
            {
                dump();
            }
            break;
        case IOCTL_CMD_SET_DEBUG_LEVEL:
            {
                int data;
                if (copy_from_user(&data, (int *)arg, sizeof(int))) {
                    return -EFAULT;
                }
                debugLevel = data;
            }
            break;
        case IOCTL_CMD_READ_KFIFO_CNT:
            {
                int len = kfifo_len(&message_kfifo);
                if (copy_to_user((int *)arg, &len, sizeof(int))) {
                    return -EFAULT;
                }
            }
            break;
        case IOCTL_CMD_READ_LAST_MSG:
            {
                if (copy_to_user((char *)arg, s_last_message, strlen(s_last_message) + 1)) {
                    return -EFAULT;
                }
            }
            break;
        default:
            return -ENOTTY;
    }

    return 0;
}

static int cameralog_open(struct inode *inode, struct file *filp) {
    pr_debug(" cameralog_open!\n");
    return 0;
}

/**
*
* @return Number of bytes read.
*/
static ssize_t cameralog_read(struct file *fp, char __user *buff,  size_t length, loff_t *ppos) {
    ssize_t bytes_read = 0;
    int ret = 0;
    int message_len = 0;
    struct task_struct *caller_task = current;

    if (debugLevel > 0) {
        pr_debug("cameralog_read .");
    }

    mutex_lock(&camlog_message_lock);
    memset(&s_read_buf, 0, sizeof(struct camlog_dev ));
    if (debugLevel > 0) {
        pr_debug("cameralog_read  kfifo len:%d",  kfifo_len(&message_kfifo) / ((int)sizeof(struct camlog_dev)));
    }

    ret = kfifo_out(&message_kfifo, &s_read_buf, sizeof(struct camlog_dev ));
    if (ret != sizeof(struct camlog_dev ) ) {
        if (debugLevel > 0) {
            pr_debug("cameralog_read: not read anything from kfifo %d  expect:%d\n", ret, (int)sizeof(struct camlog_dev));
        }
        mutex_unlock(&camlog_message_lock);
        return 0;
    }

    message_len =  strlen(s_read_buf.m_camlog_message);
    if (message_len > 0 )  {
        s_read_buf.m_camlog_message[message_len - 1] = '\n';
    } else {
        mutex_unlock(&camlog_message_lock);
        return 0;
    }
    if (debugLevel > 0) {
        pr_debug("cameralog_read message to user:%s   len:%d", (uint8_t *)s_read_buf.m_camlog_message, message_len);
    }

    ret = copy_to_user(buff, s_read_buf.m_camlog_message,  message_len);

    if (ret == 0) {
        bytes_read = message_len;
    }
    if (debugLevel > 0) {
        pr_debug("cameralog_read return len:%d", (int)bytes_read);
    }
    s_last_reader_uid = from_kuid(&init_user_ns, caller_task->cred->uid);
    s_last_reader_pid = task_pid_nr(caller_task);

    mutex_unlock(&camlog_message_lock);
    return bytes_read;
}

void camlog_send_message(void){
    pr_debug (" camlog_send_message ss  \n");
}

EXPORT_SYMBOL(camlog_send_message);
/**
*
* @return number of bytes actually written
*/
static ssize_t cameralog_write(struct file *fp, const char *buff,   size_t length, loff_t *ppos)
{
    int ret = 0;
    ssize_t bytes_wirte = 0;
    int valid_len = 0;
    struct task_struct *caller_task = current;

    if (debugLevel > 0) {
        pr_debug("cameralog_write .");
    }

    mutex_lock(&camlog_message_lock);
    memset(&s_write_buf, 0 , sizeof(struct camlog_dev));

    valid_len =  length >= MESSAGE_MAX?MESSAGE_MAX - 1:length;
    ret = copy_from_user(s_write_buf.m_camlog_message, buff,  valid_len);
    if (0 == ret)
    {
        bytes_wirte = strlen(s_write_buf.m_camlog_message);
    }
    if (debugLevel > 0) {
        pr_debug("cameralog_write  valid_len:%d  strlen:%d.", valid_len, (int)bytes_wirte);
    }
    if (0 == bytes_wirte)
    {
        //empty string!
        mutex_unlock(&camlog_message_lock);
        return 0;
    }
    if (debugLevel > 0) {
        pr_debug("cameralog_write  input  msg : %s", s_write_buf.m_camlog_message);
    }
    //pr_debug("cameralog_write avail:%d", kfifo_avail(&message_kfifo) );
    while (kfifo_avail(&message_kfifo) < sizeof(struct camlog_dev)) {
        ret = kfifo_out(&message_kfifo, &s_read_buf, sizeof(struct camlog_dev));
        if (ret != sizeof(struct camlog_dev )) {
            kfifo_skip(&message_kfifo);
        }
        //pr_debug("avail:%d", kfifo_avail(&message_kfifo) );
    }
    ret = kfifo_in(&message_kfifo, &s_write_buf, sizeof(struct camlog_dev));
    if (ret != sizeof(struct camlog_dev )) {
        if (debugLevel > 0) {
            pr_debug("cameralog: Failed to write to kfifo %d -  %d\n", ret, (int)sizeof(struct camlog_dev));
        }
        mutex_unlock(&camlog_message_lock);
        return 0;
    }
    s_last_caller_uid = from_kuid(&init_user_ns, caller_task->cred->uid);
    s_last_caller_pid = task_pid_nr(caller_task);
    strncpy(s_last_message, s_write_buf.m_camlog_message, bytes_wirte);
    if (debugLevel > 0) {
        pr_debug("cameralog message kfifo   len:%d" , kfifo_len(&message_kfifo));
    }
    mutex_unlock(&camlog_message_lock);
    wake_up_interruptible(&camlog_is_not_empty);
    return bytes_wirte;
}

static int cameralog_release(struct inode *inode, struct file *filp)
{
    pr_debug(" cameralog_release!\n");
    wake_up_interruptible(&camlog_is_not_empty);
    return 0;
}

static unsigned int cameralog_poll(struct file *file,
                                          struct poll_table_struct *poll_table)
{
    unsigned int mask = 0;
    poll_wait(file, &camlog_is_not_empty, poll_table);
    if (kfifo_len(&message_kfifo) > 0)
    {
        mask = POLLIN | POLLRDNORM;
    }
    return mask;
}

static const struct file_operations cameralog_fops =
{
    .owner = THIS_MODULE,
    .unlocked_ioctl = cameralog_ioctl,
    .open = cameralog_open,
    .release = cameralog_release,
    .poll = cameralog_poll,
    .read = cameralog_read,
    .write=cameralog_write,
};

static struct class *camlog_class;
static unsigned int camlog_major;
static int __init cameralog_init(void)
{
    struct device *device;
    int ret = 0;
    ret = kfifo_alloc(&message_kfifo, KFIFO_COUNT*sizeof(struct camlog_dev), GFP_KERNEL);
    if (ret) {
        printk(KERN_ERR "cameralog message kfifo: Failed to allocate kfifo\n");
        return ret;
    }
    camlog_major = register_chrdev(0, CAMLOG_DEV_NAME, &cameralog_fops);
    camlog_class=class_create(CAMLOG_DEV_NAME);
    if (IS_ERR(camlog_class)) {
        unregister_chrdev(camlog_major,  CAMLOG_DEV_NAME);
        pr_warn("Failed to create class.\n");
        return PTR_ERR(camlog_class);
    }
    devt = MKDEV(camlog_major, 0);
    device = device_create(camlog_class, NULL, devt,
                          NULL, CAMLOG_DEV_NAME);
    if (IS_ERR(device)) {
        pr_err("error while trying to create %s\n",CAMLOG_DEV_NAME);
        return -EINVAL;
    }
    mutex_init(&camlog_message_lock);
    init_waitqueue_head(&camlog_is_not_empty);
    return 0;
}
module_init(cameralog_init);
static void __exit cameralog_exit(void)
{
    device_destroy(camlog_class, devt);
    class_destroy(camlog_class);
    unregister_chrdev(camlog_major, CAMLOG_DEV_NAME);
    kfifo_free(&message_kfifo);
}
module_exit(cameralog_exit);
MODULE_DESCRIPTION("camera log device driver");
MODULE_LICENSE("GPL v2");
// END Camera_CameraSkyNet
