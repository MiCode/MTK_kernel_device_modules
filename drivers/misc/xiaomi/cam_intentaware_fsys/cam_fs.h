/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */
// MIUI ADD: Camera_CameraOpt

#ifndef _CAM_MSG_FS_H_
#define _CAM_MSG_FS_H_

#include <linux/cpumask.h>
#include <linux/android/binder.h>
#include <trace/hooks/binder.h>
#include <linux/rwlock.h>
#include <linux/fs.h>
#include <linux/hashtable.h>
#include <linux/rculist.h>

#define CAM_FS_CMD_MAX_LEN 2048
#define PATH_NAME_MAX_LEN 1024
#define DEV_NAME_LEN_MAX 25
#define CMDLINE_MAX_LEN  60
#define SPLIT_DELIM "|"

#define WRITE_ERROR_UNSPORT_MSG_TYPE -1008
#define WRITE_ERROR_BUF_ADDR_INVALID -1009
#define WRITE_ERROR_FORMAT_INVALID -1010
#define WRITE_ERROR_NO_RECEVER -1011
#define WRITE_ERROR_NO_MSG_SPACE -1012
#define WRITE_ERROR_LEN_INVALID -1013
#define WRITE_ERROR_CPY_FROM_USER -1014
#define WRITE_ERROR_CAM_DPM_DISABLE -2002

struct pid_entry {
	pid_t pid;
	struct rcu_head rcu;
	struct hlist_node node;
};

struct binder_data {
	int caller_pid;
	int binder_th_tid;
	struct binder_transaction *t;
	bool transaction_received;
};

struct cam_fs_data {
	int lock_tgid;
	unsigned long i_ino;
	struct rcu_head rcu;
	char  fullPath[PATH_NAME_MAX_LEN];
};

struct cam_fs_device {
	char	name[DEV_NAME_LEN_MAX];
	char version[DEV_NAME_LEN_MAX];
	struct class  *cam_fs_class;
	unsigned int   cam_fs_major;

	dev_t devt;
	struct device *device;

	struct mutex g_lock;      //global Lock
	char d_cache[CAM_FS_CMD_MAX_LEN];
};
#endif /* _CAM_MSG_FS_H_ */
// END Camera_CameraOpt