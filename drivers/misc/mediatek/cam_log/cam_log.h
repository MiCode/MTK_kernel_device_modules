/* SPDX-License-Identifier: GPL-2.0-only */
/*
* Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
*/
// MIUI ADD: Camera_CameraSkyNet
#ifndef _CAM_LOG_H_
#define _CAM_LOG_H_
#include <linux/types.h>
#include <linux/kfifo.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>

#define MESSAGE_MAX 1024
#define KFIFO_COUNT 6
struct camlog_dev {
    uid_t writer_uid;
    pid_t writer_pid;
    uint8_t m_camlog_message[MESSAGE_MAX];
};
void camlog_send_message(void);
#endif /* _CAM_LOG_H_ */
// END Camera_CameraSkyNet

