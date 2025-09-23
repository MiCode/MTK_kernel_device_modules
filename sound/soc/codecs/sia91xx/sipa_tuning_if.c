/*
 * Copyright (C) 2022, SI-IN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <asm/current.h>
#include <linux/slab.h>
#include <linux/wait.h>

#include "sipa_tuning_if.h"
#include "sipa_common.h"

/*
 up:     pc/app -> kernel -> hal -> dsp
 down:   dsp -> hal -> kernel -> pc/app
*/

static void myCircularQueueCreate(sipa_sync_t* obj)
{
    int i;
    for (i=0; i < MAX_SIZE; i++) {
        memset(obj->q[i].data, 0, DATA_MAX_LEN);
        obj->q[i].flag = false;
        obj->q[i].len  = 0;
    }

    obj->head = 0;
    obj->tail = 0;
    obj->k = MAX_SIZE;
    mutex_unlock(&obj->lock);

    return;
}

//入队列
static bool myCircularQueueEnQueue(sipa_sync_t* obj, void __user * arg, uint32_t len)
{
    //如果队列已满，则无法插入
    if (obj->head == 0 && obj->tail + 1 == MAX_SIZE)
        return -1;
    else if((obj->tail + 1) % MAX_SIZE == obj->head)
        return -1;

    //插入数据
    if(copy_from_user(obj->q[obj->tail].data, arg, len)){
        return EFAULT;
    }
    obj->q[obj->tail].len  = len;
    obj->q[obj->tail].flag = true;
    obj->tail++;
    //循环下标判定，如果触碰边界，手动修正
    if (obj->tail == obj->k) {
        obj->tail = 0;
    }
    return 0;
}

//出队列
static bool myCircularQueueDeQueue(void __user * arg, sipa_sync_t* obj)
{
    mutex_lock(&obj->lock);
    //如果队列为空，则无法删除
    if (obj->head == obj->tail) {
        mutex_unlock(&obj->lock);
        return -1;
    }

    if(copy_to_user(arg, obj->q[obj->head].data, obj->q[obj->head].len)){
        mutex_unlock(&obj->lock);
        return EFAULT;
    }
    memset(obj->q[obj->head].data, 0, obj->q[obj->head].len);
    obj->q[obj->head].flag = false;
    obj->head++;

    if (obj->head == obj->k) {
        obj->head = 0;
    }
    mutex_unlock(&obj->lock);
    return 0;
}

static int sipa_tuning_write(void __user * arg, sipa_sync_t *pdata)
{
    uint8_t data[DATA_MAX_LEN];
    uint32_t len;
    struct dev_comm_data *msg = NULL;

    mutex_lock(&pdata->lock);
    if (copy_from_user(data, arg, sizeof(dev_comm_data_t))) {
        mutex_unlock(&pdata->lock);
        return -EFAULT;
    }

    msg = (struct dev_comm_data *)data;
    len = DEV_COMM_DATA_LEN(msg);
    if (myCircularQueueEnQueue(pdata, arg, len)) {
        pr_err("[ err] %s: enqueue fail\n", __func__);
        mutex_unlock(&pdata->lock);
        return -EFAULT;
    }
    wake_up_interruptible(&pdata->wq);
    mutex_unlock(&pdata->lock);
    pr_info("[ info] %s: datalen:%d payload len:%d\n", __func__, len, msg->payload_size);
    return 0;
}

static int sipa_tuning_read(void __user *arg, sipa_sync_t *pdata)
{
    int ret = 0;
    ret = wait_event_interruptible(pdata->wq, pdata->q[pdata->head].flag);
    if (ret) {
        pr_err("[ err] %s: wait_event return\n", __func__);
        return -ERESTART;
    }

    if (myCircularQueueDeQueue(arg, pdata)) {
        pr_err("[ err] %s: dequeue fail\n", __func__);
        return -EFAULT;
    }
    return 0;
}

static int sipa_tuning_read_unblock(void __user *arg, sipa_sync_t *pdata)
{
    int cnt = 0;

    while(pdata->q[pdata->head].flag == false) {
        cnt++;
        usleep_range(1000, 2000);
        if (cnt == 300) {
            pr_err("[  err] %s: wait reply event timeout !! \n",
				 __func__);
            return -1;
        }
    }

    if (myCircularQueueDeQueue(arg, pdata)) {
        return -EFAULT;
    }
    return 0;
}

static long sipa_tuning_comm_ioctl(unsigned int cmd, unsigned long arg, sipa_sync_t *up, sipa_sync_t *down)
{
    int ret = 0;

    switch (cmd) {
        case SIPA_TUNING_CTRL_WR_UP: {
                pr_info("[ info] %s: up write cmd\n", __func__);
                ret = sipa_tuning_write((void __user *)arg, up);
            }
            break;
        case SIPA_TUNING_CTRL_RD_UP: {
                ret = sipa_tuning_read((void __user *)arg, up);
                pr_info("[ info] %s: up read cmd, ret:%d \n", __func__, ret);
            }
            break;
        case SIPA_TUNING_CTRL_WR_DOWN: {
                pr_info("[ info] %s: down write cmd\n", __func__);
                ret = sipa_tuning_write((void __user *)arg, down);
            }
            break;
        case SIPA_TUNING_CTRL_RD_DOWN: {
                ret = sipa_tuning_read_unblock((void __user *)arg, down);
                pr_info("[ info] %s: down read cmd, ret:%d \n", __func__, ret );
            }
            break;
        default: {
            pr_info("[ info] %s: unsuport cmd:0x%x\n", __func__, cmd);
            ret = -EFAULT;
            break;
        }
    }

    return ret;
}

static long sipa_tuning_cmd_unlocked_ioctl(struct file *fp,
	unsigned int cmd, unsigned long arg)
{
    sipa_turning_t *priv = g_sipa_turning;
#if 0
    scene_data_t scene_data;
    box_name_t fwname;
    char *pstr = NULL;
#endif
    int ret = 0;

    pr_info("[ info] %s: enter cmd:0x%x\n", __func__, cmd);
    switch (cmd) {
        case SIPA_TUNING_CTRL_WR_UP:
        case SIPA_TUNING_CTRL_RD_UP:
        case SIPA_TUNING_CTRL_WR_DOWN:
        case SIPA_TUNING_CTRL_RD_DOWN: {
            ret = sipa_tuning_comm_ioctl(cmd, arg, &(priv->cmdup), &(priv->cmddown));
            break;
        }
#if 0
        case SIPA_IOCTL_LOAD_FIRMWARE: {
            if (copy_from_user(&fwname.len, (void __user *)arg, sizeof(uint8_t))) {
                pr_err("%s: power_mode copy from user failed\n", __func__);
                return -EFAULT;
            }
            if (fwname.len > sizeof(fwname.boxname)) {
                pr_err("%s: input too long, len:%d, maxlen:%d\n", __func__, fwname.len, sizeof(fwname.boxname));
                return -EFAULT;
            }
			pstr = (char*)arg + 1;
            if (copy_from_user(&fwname.boxname, (void __user *)pstr, sizeof(fwname.boxname))) {
                pr_err("%s: power_mode copy from user failed\n", __func__);
                return -EFAULT;
            }
            sipa_multi_channel_load_fw(fwname.boxname);
            break;
        }
        case SIPA_IOCTL_POWER_ON: {
            if (copy_from_user(&scene_data, (void __user *)arg, sizeof(scene_data_t))) {
                pr_err("%s: power_mode copy from user failed\n", __func__);
                return -EFAULT;
            }
            sipa_multi_channel_power_on_and_set_scene(scene_data.scene, scene_data.pa_idx);
            break;
        }
        case SIPA_IOCTL_POWER_OFF: {
            if (copy_from_user(&scene_data, (void __user *)arg, sizeof(scene_data_t))) {
                pr_err("%s: power_off copy from user failed\n", __func__);
                return -EFAULT;
            }
            sipa_multi_channel_power_off(scene_data.pa_idx);
            break;
        }
        case SIPA_IOCTL_GET_CHANNEL: {
            int channel_num = sipa_get_channels();
            if (copy_to_user((void __user *)arg, &channel_num, sizeof(int))) {
                pr_err("%s: copy channel_num to user failed\n", __func__);
                return -EFAULT;
            }
            break;
        }
        case SIPA_IOCTL_REG_DUMP: {
            sipa_multi_channel_reg_dump();
            break;
        }
#endif
        default: {
            pr_err("%s: not support ioctl:0x%x\n", __func__, cmd);
            break;
        }
    }
    return ret;
}

struct file_operations sipa_turning_cmd_fops = {
    .owner = THIS_MODULE,
	.unlocked_ioctl = sipa_tuning_cmd_unlocked_ioctl,
	.compat_ioctl = sipa_tuning_cmd_unlocked_ioctl,
};

static int sipa_tuning_tool_open(struct inode *inode, struct file *fp)
{
    sipa_turning_t *priv = g_sipa_turning;

    pr_info("[ info] %s: run !! \n", __func__);

    myCircularQueueCreate(&priv->toolup);
    myCircularQueueCreate(&priv->tooldown);
    myCircularQueueCreate(&priv->cmdup);
    myCircularQueueCreate(&priv->cmddown);

	return 0 ;
}

static long sipa_tuning_tool_unlocked_ioctl(struct file *fp,
	unsigned int cmd, unsigned long arg)
{
    sipa_turning_t *priv = g_sipa_turning;

	pr_info("[ info] %s: enter cmd:0x%x\n", __func__, cmd);
    return sipa_tuning_comm_ioctl(cmd, arg, &(priv->toolup), &(priv->tooldown));
}

struct file_operations sipa_turning_tool_fops = {
    .owner = THIS_MODULE,
    .open  = sipa_tuning_tool_open,
	.unlocked_ioctl = sipa_tuning_tool_unlocked_ioctl,
	.compat_ioctl = sipa_tuning_tool_unlocked_ioctl,
};
