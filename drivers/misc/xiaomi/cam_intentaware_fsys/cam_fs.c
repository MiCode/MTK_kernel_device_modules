// SPDX-License-Identifier: GPL-2.0-or-later
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
// MIUI ADD: Camera_CameraOpt
#include "cam_fs.h"
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/path.h>
#include <linux/dcache.h>
#include <linux/namei.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/wait.h>
#include <linux/preempt.h>
#include <linux/printk.h>
#include <linux/cgroup.h>
#include <linux/kernfs.h>
#include <linux/cred.h>
#include <linux/sched/task.h>
#include <trace/hooks/fs.h>
#include <trace/hooks/fuse.h>
#include <trace/hooks/syscall_check.h>
#include <linux/pid.h>
#include <linux/poll.h>
#include <linux/string_helpers.h>
#include <linux/mm.h>      // access_process_vm
#include <linux/types.h>
#include <linux/kfifo.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/input.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/fb.h>


#define CAM_FS_ENABLE  1
#define CAM_FS_DISABLE 0

#define CAM_FS_FLAG_WAIT_CONTINUE 1
#define CAM_FS_FLAG_WAIT_RELEASE  0

#define CAM_FS_DEFAULT_VALUE -1
#define MAX_CONTROL_NUM_OF_THREADS 2
#define CAM_FS_TOP_APP_DEFAULT_VALUE -1
#define CAM_FS_MEDIA_PROVIDER_PID_DEVAULT -1

#define WRITE_ERROR_NO_PERMISSION -1018
#define READER_ERROR_COPY_TO_USER_FAILED -1017
#define WRITE_ERR_FORMAT_INVALID -6002

static atomic_t cam_fs_debug  = ATOMIC_INIT(0);
static atomic_t cam_fs_enable = ATOMIC_INIT(0);
static atomic_t cam_fs_f_wait = ATOMIC_INIT(CAM_FS_FLAG_WAIT_RELEASE);
static atomic_t cam_fs_lock_icount      = ATOMIC_INIT(2);
static atomic_t cam_fs_top_app_pid      = ATOMIC_INIT(CAM_FS_TOP_APP_DEFAULT_VALUE);
static atomic_t cam_fs_waited_threads   = ATOMIC_INIT(0);
static atomic_t cam_fs_lock_max_time_ms = ATOMIC_INIT(1000);
static atomic_t cam_fs_max_ctrl_threads = ATOMIC_INIT(MAX_CONTROL_NUM_OF_THREADS);
static atomic_t cam_fs_open_drv_threads = ATOMIC_INIT(0);
static atomic_t cam_fs_btransc_rec_tid  = ATOMIC_INIT(CAM_FS_MEDIA_PROVIDER_PID_DEVAULT);

static struct cam_fs_data __rcu *camfsdata;
static struct cam_fs_device dev_info;
static DEFINE_MUTEX(update_lock);

static struct binder_data *b_data;
static DEFINE_RWLOCK(b_data_rwlock);

#define MP_HASH_BITS 8
DECLARE_HASHTABLE(mprovider_pids_hash, MP_HASH_BITS);
static DEFINE_SPINLOCK(mprovider_pids_lock);

#define log(fmt, args...) \
do { \
	if (atomic_read(&cam_fs_debug)) { \
		pr_info(fmt, ##args); \
	} \
} while (0)

bool is_media_provider(pid_t pid)
{
	struct pid_entry *entry;
	bool found = false;

	rcu_read_lock();
	hash_for_each_possible_rcu(mprovider_pids_hash, entry, node, pid) {
		if (entry->pid == pid) {
			found = true;
			break;
		}
	}
	rcu_read_unlock();
	return found;
}

#define PID_ARRAY_SIZE 10

int find_pid_cmdline(char *cmdline)
{
	struct task_struct *task = NULL;

	int ret_pid = -1;
	int idx = 0;
	int cmdline_len = 0;
	int pids[PID_ARRAY_SIZE] = {-1};
	char *tmp_cmdline = NULL;

	if (cmdline == NULL)
		return ret_pid;

	cmdline_len = strnlen(cmdline, CMDLINE_MAX_LEN)-1;
	if (cmdline_len <= 0)
		return ret_pid;

	log("[%s] cmdline:%s cmdline_len:%d\n", __func__, cmdline, cmdline_len);
	if (tasklist_empty())
		return ret_pid;
	rcu_read_lock();
	for_each_process(task) {
		if (task == NULL || task->tasks.next == NULL || list_empty(&task->tasks))
			break;
		if (task->pid != task->tgid)
			continue;
		if (cmdline_len < TASK_COMM_LEN && strnlen(task->comm, TASK_COMM_LEN) > cmdline_len)
			continue;

		if (idx >= PID_ARRAY_SIZE)
			break;

		if (strstr(cmdline, task->comm) != NULL) {
			pids[idx++] = task->tgid;
			log("[%s] cmdline:%s idx:%d pid:%d\n", __func__, task->comm, (idx-1), pids[idx-1]);
		}
	}
	rcu_read_unlock();
	log("[%s] cmdline_len:%d idx:%d\n", __func__, cmdline_len, idx);
	for (idx = 0; idx < PID_ARRAY_SIZE; idx++) {
		if (pids[idx] > 0) {
			task = get_pid_task(find_vpid(pids[idx]), PIDTYPE_PID);
			if (!task)
				continue;
			tmp_cmdline = kstrdup_quotable_cmdline(task, GFP_KERNEL);
			log("[%s] tm_cmdline:%s cmdline:%s camdline_len:%d strncmp:%d\n", __func__, tmp_cmdline, cmdline, cmdline_len, strncmp(tmp_cmdline, cmdline, cmdline_len));
			if (strncmp(tmp_cmdline, cmdline, cmdline_len) == 0) {
				ret_pid = task->tgid;
				kfree(tmp_cmdline);
				put_task_struct(task);
				break;
			}
			kfree(tmp_cmdline);
			put_task_struct(task);
		}
	}

	return ret_pid;
}


void check_mprovider_pid(bool camfs_enable)
{
	char mp_cmdline[] = "com.android.providers.media.module";
	char gl_mp_cmdline[] = "com.google.android.providers.media.module";
	struct task_struct *task = NULL;
	struct pid_entry *entry = NULL;
	struct hlist_node *tmp_node = NULL, *tmp = NULL;
	struct hlist_head temp_hash;
	char *tmp_cmdline = NULL;
	int i = 0;

	INIT_HLIST_HEAD(&temp_hash);

	if (camfs_enable) {
		for_each_process(task) {
			if (task->mm == NULL)
				continue;
			tmp_cmdline = kstrdup_quotable_cmdline(task, GFP_KERNEL);
			if (!tmp_cmdline)
				continue;

			if (strstr(tmp_cmdline, mp_cmdline) || strstr(tmp_cmdline, gl_mp_cmdline)) {
				entry = kmalloc(sizeof(*entry), GFP_KERNEL);
				if (entry) {
					entry->pid = task->pid;
					hlist_add_head_rcu(&entry->node, &temp_hash);
					log("[%s] Found media provider pid:%d\n", __func__, task->pid);
				} else {
					log("[%s] pid_entry kmalloc failed.\n", __func__);
				}
			}
			kfree(tmp_cmdline);
		}
	}
	spin_lock(&mprovider_pids_lock);

	hash_for_each_safe(mprovider_pids_hash, i, tmp_node, entry, node) {
		hash_del_rcu(&entry->node);
		kfree_rcu(entry, rcu);
	}

	hlist_for_each_entry_safe(entry, tmp, &temp_hash, node) {
		hash_add_rcu(mprovider_pids_hash, &entry->node, entry->pid);
	}

	spin_unlock(&mprovider_pids_lock);
}

void clear_cam_fs_data(struct cam_fs_data *item)
{
	if (item == NULL)
		return;
	item->i_ino     = CAM_FS_DEFAULT_VALUE;
	item->lock_tgid = CAM_FS_DEFAULT_VALUE;
	memset(&(item->fullPath), 0, sizeof(char) * PATH_NAME_MAX_LEN);
}

static void cam_fs_android_vh_binder_proc_transaction(void *unused, struct task_struct *caller_task, struct task_struct *binder_proc_task,
			struct task_struct *binder_th_task, int node_debug_id,
			struct binder_transaction *t, bool pending_async)
{
	bool should_record_binderinfo = false;
	struct cam_fs_data *local = NULL;

	if (!atomic_read(&cam_fs_enable))
		return;

	if (binder_proc_task && is_media_provider(binder_proc_task->pid)) {
		rcu_read_lock();
		local = rcu_dereference(camfsdata);
		if (local && READ_ONCE(local->i_ino) != CAM_FS_DEFAULT_VALUE
			&& READ_ONCE(local->lock_tgid) == caller_task->tgid) {
			should_record_binderinfo = true;
		}
		rcu_read_unlock();
		if (binder_th_task && should_record_binderinfo) {
			write_lock(&b_data_rwlock);
			if (b_data) {
				b_data->caller_pid    = caller_task->tgid;
				b_data->binder_th_tid = binder_th_task->pid;
				b_data->t = t;
				b_data->transaction_received = false;
			}
			write_unlock(&b_data_rwlock);
			log("[%s] camera app caller pid:%d binder_proc:%d binder_th:%d transaction:%p\n", __func__, caller_task->tgid, binder_th_task->tgid, binder_th_task->pid, t);
		} else {
			log("[%s] other app caller pid:%d binder_proc:%d transaction:%p\n", __func__, caller_task->tgid, binder_proc_task->pid, t);
		}
	}
}

#define CMD_TO_OPEN_FILE_1 2151707138
#define CMD_TO_OPEN_FILE_2 2151707139
static void cam_fs_android_vh_binder_transaction_received(void *unused,
			struct binder_transaction *t, struct binder_proc *proc,
			struct binder_thread *thread, uint32_t cmd)
{
	bool is_mprovider         = false;
	bool is_target_transact   = false;
	struct cam_fs_data *local = NULL;

	if (!atomic_read(&cam_fs_enable))
		return;

	rcu_read_lock();
	local = rcu_dereference(camfsdata);
	if (local && READ_ONCE(local->i_ino) != CAM_FS_DEFAULT_VALUE && is_media_provider(current->tgid)) {
		is_mprovider = true;
	}
	rcu_read_unlock();
	if (is_mprovider && cmd == CMD_TO_OPEN_FILE_1) {
		read_lock(&b_data_rwlock);
		if (b_data) {
			if (b_data->t == t && b_data->binder_th_tid == current->pid) {
				is_target_transact = true;
				log("[%s] caller current_pid:%d current_tid:%d cmd:%u transaction:%p is target.\n", __func__, current->tgid, current->pid, cmd, t);
			} else {
				is_target_transact = false;
				log("[%s] caller current_pid:%d current_tid:%d cmd:%u transaction:%p not target\n", __func__, current->tgid, current->pid, cmd, t);
			}
		}
		read_unlock(&b_data_rwlock);
		write_lock(&b_data_rwlock);
		if (b_data)
			b_data->transaction_received = is_target_transact;
		write_unlock(&b_data_rwlock);
		atomic_set(&cam_fs_btransc_rec_tid, current->pid);
	}
}

#define NORMAL_APP_UID 10000
static void cam_fs_android_vh_f2fs_file_open(void *unused, struct inode *f_inode, struct file *filp)
//static void cam_fs_android_vh_f2fs_file_open(void *unused, const struct file *filp)
{
	bool should_wait = false;
	struct inode       *inode = NULL;
	struct cam_fs_data *local = NULL;
	unsigned long timeout_start     = -1;
	unsigned long timeout_jiffies   = -1;
	unsigned long timeout_remaining = -1;

	if (!in_task()) {
		log("[%s] hook point called in atomic context! cannot sleep.\n", __func__);
		return;
	}
	if (!atomic_read(&cam_fs_enable))
		return;

	inode = filp->f_inode;
	if (!inode) {
		log("[%s] d_real_inode func get inode failed.\n", __func__);
		return;
	}

	rcu_read_lock();
	local = rcu_dereference(camfsdata);
	if (local && READ_ONCE(local->i_ino) != CAM_FS_DEFAULT_VALUE //control thread set control info.
		&& inode->i_ino == READ_ONCE(local->i_ino)      //curr thread open target file.
		&& READ_ONCE(local->lock_tgid) != current->tgid //curr thread can not be control thread
		&& __kuid_val(current_uid()) > NORMAL_APP_UID) {//curr thread uid must upper 10000

		log("[%s] comm:%s pid:%d tid:%d try to open file.\n", __func__, current->comm, current->tgid, current->pid);

		if (!is_media_provider(current->tgid)) { //app open file not with mediaprovider
			log("[%s] comm:%s pid:%d tid:%d open file not with mediaprovider\n", __func__, current->comm, current->tgid, current->pid);
			rcu_read_unlock();
			goto __t_udelay__;
		}

		if (atomic_read(&cam_fs_btransc_rec_tid) == current->pid) // curr thread is binder thread, just do it with mediaprovider binder thread not other threads
			should_wait = true;
	}
	rcu_read_unlock();
	// is Camera App？
	if (should_wait) {
		read_lock(&b_data_rwlock);
		if (b_data && (b_data->transaction_received)
			&& b_data->binder_th_tid == current->pid) {
			should_wait = false;
			log("[%s] open may be camera app invoked! ignore it. should_wait:%d\n", __func__, should_wait);
			read_unlock(&b_data_rwlock);
			return;
		}
		read_unlock(&b_data_rwlock);
	}
	if (should_wait) {

__t_udelay__:
		//if is white list app, skip it.
		if (atomic_read(&cam_fs_top_app_pid) == current->tgid)
			return;

		if (atomic_inc_return(&cam_fs_waited_threads) >= atomic_read(&cam_fs_max_ctrl_threads)) {
			log("[%s] waitting threads number up control: %d\n", __func__, atomic_read(&cam_fs_max_ctrl_threads));
			atomic_dec(&cam_fs_waited_threads);
			return;
		}
		log("[%s] try to block comm:%s pid:%d tid:%d to open should_wait:%d\n", __func__, current->comm, current->tgid, current->pid, should_wait);
		timeout_start     = jiffies;
		timeout_jiffies   = msecs_to_jiffies(atomic_read(&cam_fs_lock_max_time_ms));
		timeout_remaining = timeout_jiffies;
		while (timeout_remaining > 0 && (timeout_remaining <= timeout_jiffies)) {
			if (atomic_read(&cam_fs_f_wait) == CAM_FS_FLAG_WAIT_RELEASE) {
				log("[%s] try to block comm:%s pid:%d tid:%d to open released early by user\n", __func__, current->comm, current->tgid, current->pid);
				break;
			}
			udelay(1000);
			timeout_remaining = timeout_jiffies - (jiffies - timeout_start);
		}
		log("[%s] comm:%s pid:%d tid:%d wake up from wait.\n", __func__, current->comm, current->tgid, current->pid);
		atomic_dec_if_positive(&cam_fs_waited_threads);
	}
}

static ssize_t cam_fs_unlock_handle(char *buf, size_t count)
{
	int ret = 0;
	char *kern_path_buf = NULL;
	size_t actual_len      = count;
	size_t origin_data_len = 0;
	struct cam_fs_data *camfsdata_new = NULL;
	struct cam_fs_data *camfsdata_old = NULL;

	kern_path_buf = kzalloc(actual_len + 1, GFP_KERNEL);
	if (!kern_path_buf) {
		ret = -ENOMEM;
		goto out;
	}

	strscpy(kern_path_buf, buf, actual_len + 1);

	while (actual_len > 0 && (kern_path_buf[actual_len - 1] == '\n' ||
		kern_path_buf[actual_len - 1] == '\r' || isspace(kern_path_buf[actual_len - 1]))) {
		kern_path_buf[actual_len - 1] = '\0';
		actual_len--;
	}

	log("[%s] Received path: %s\n", __func__, kern_path_buf);

	mutex_lock(&update_lock);
	camfsdata_new = kmalloc(sizeof(struct cam_fs_data), GFP_KERNEL);
	if (camfsdata_new == NULL) {
		log("[%s] camfsdata_new kmalloc failed.\n", __func__);
		ret = -EINVAL;
	} else {
		clear_cam_fs_data(camfsdata_new);
		camfsdata_old   = rcu_dereference_protected(camfsdata, lockdep_is_held(&update_lock));
		origin_data_len = strnlen(&(camfsdata_old->fullPath[0]), actual_len);

		if (!camfsdata_old) {
			log("[%s] camfsdata_old is nullptr.\n", __func__);
			kfree(camfsdata_new);
			ret = -EINVAL;
		} else {
			log("[%s] origin path: %s origin_data_len:%zu actual_len:%zu cmp:%d\n", __func__, &(camfsdata_old->fullPath[0]), origin_data_len, actual_len,
							strncmp(&(camfsdata_old->fullPath[0]), kern_path_buf, min(origin_data_len, actual_len)));
			if (READ_ONCE(camfsdata_old->i_ino) == CAM_FS_DEFAULT_VALUE) {
				log("[%s] user never set file lock before!.\n", __func__);
				kfree(camfsdata_new);
				ret = -EINVAL;
			} else if (strncmp(&(camfsdata_old->fullPath[0]), kern_path_buf, min(origin_data_len, actual_len)) == 0) {
				rcu_assign_pointer(camfsdata, camfsdata_new);
				synchronize_rcu();
				kfree(camfsdata_old);
				atomic_set(&cam_fs_f_wait, CAM_FS_FLAG_WAIT_RELEASE);
				log("[%s] camfsdata replace finish.\n", __func__);
			} else {
				log("[%s] unlock the wrong file: %s.\n", __func__, kern_path_buf);
				kfree(camfsdata_new);
				ret = -EINVAL;
			}
		}
	}
	mutex_unlock(&update_lock);

	kfree(kern_path_buf);
out:
	if (ret < 0)
		return ret;

	return count;
}

static ssize_t cam_fs_try_lock_handle(char *buf, size_t count)
{
	int ret = 0;
	struct path path;
	char *kern_path_buf = NULL;
	struct inode *inode = NULL;
	size_t actual_len   = count;
	struct cam_fs_data *camfsdata_new = NULL;
	struct cam_fs_data *camfsdata_old = NULL;

	kern_path_buf = kzalloc(actual_len + 1, GFP_KERNEL);
	if (!kern_path_buf) {
		ret = -ENOMEM;
		goto out;
	}

	strscpy(kern_path_buf, buf, actual_len + 1);

	while (actual_len > 0 && (kern_path_buf[actual_len - 1] == '\n' ||
		kern_path_buf[actual_len - 1] == '\r' || isspace(kern_path_buf[actual_len - 1]))) {
		kern_path_buf[actual_len - 1] = '\0';
		actual_len--;
	}

	log("[%s] Received path: %s.\n", __func__, kern_path_buf);

	ret = kern_path(kern_path_buf, LOOKUP_FOLLOW, &path);
	if (ret) {
		log("[%s] Path lookup failed for: %s., error: %d\n", __func__, kern_path_buf, ret);
		ret = -ENOENT;
		goto out_free;
	}

	inode = d_real_inode(path.dentry);
	if (!inode) {
		log("[%s] Failed to get inode for: %s.\n", __func__, kern_path_buf);
		ret = -EINVAL;
		goto out_path_put;
	}

	log("[%s] Found inode: %lu for path: %s.\n", __func__, inode->i_ino, kern_path_buf);

	if (atomic_read(&inode->i_writecount) > 0) {
		log("[%s] File is open for writing (writecount: %d)\n", __func__, atomic_read(&inode->i_writecount));
		ret = -EINVAL;
	} else {
		if (atomic_read(&(inode->i_count)) >= atomic_read(&cam_fs_lock_icount)) {
			log("[%s] File is opened icount: %d\n", __func__, atomic_read(&inode->i_count));
			ret = -EINVAL;
		} else {
			log("[%s] pid:%d add INode(real_ino):%lu to Container\n", __func__, current->tgid, inode->i_ino);
			mutex_lock(&update_lock);
			camfsdata_new = kmalloc(sizeof(struct cam_fs_data), GFP_KERNEL);
			if (camfsdata_new == NULL) {
				log("[%s] camfsdata_new kmalloc failed.\n", __func__);
				ret = -EINVAL;
			} else {
				if (actual_len > PATH_NAME_MAX_LEN)
					actual_len = PATH_NAME_MAX_LEN;

				clear_cam_fs_data(camfsdata_new);
				camfsdata_old = rcu_dereference_protected(camfsdata, lockdep_is_held(&update_lock));
				if (!camfsdata_old) {
					log("[%s] camfsdata_old is nullptr.\n", __func__);
					kfree(camfsdata_new);
					ret = -EINVAL;
				} else {
					memcpy(camfsdata_new, camfsdata_old, sizeof(struct cam_fs_data));
					if (READ_ONCE(camfsdata_old->i_ino) != CAM_FS_DEFAULT_VALUE) {
						log("[%s] user set file lock before!.\n", __func__);
						kfree(camfsdata_new);
						ret = -EINVAL;
					} else {
						atomic_set(&cam_fs_f_wait, CAM_FS_FLAG_WAIT_CONTINUE);
						WRITE_ONCE(camfsdata_new->i_ino,     inode->i_ino);
						WRITE_ONCE(camfsdata_new->lock_tgid, current->tgid);
						strscpy(&(camfsdata_new->fullPath[0]), kern_path_buf, actual_len);
						rcu_assign_pointer(camfsdata, camfsdata_new);
						synchronize_rcu();
						kfree(camfsdata_old);
						log("[%s] camfsdata replace finish.\n", __func__);
						check_mprovider_pid(true);
					}
				}
			}
			mutex_unlock(&update_lock);
		}
	}
out_path_put:
	path_put(&path);
out_free:
	kfree(kern_path_buf);
out:
	if (ret < 0)
		return ret;

	return count;
}


static ssize_t cam_fs_enable_handle(char *buf, size_t count)
{
	int ret;
	int enable = 0;
	struct cam_fs_data *camfsdata_new = NULL;
	struct cam_fs_data *camfsdata_old = NULL;

	ret = kstrtoint(buf, 10, &enable);
	if (ret < 0)
		return ret;

	if (enable != CAM_FS_DISABLE && enable != CAM_FS_ENABLE) {
		atomic_set(&cam_fs_enable, CAM_FS_DISABLE);
		goto mybe_release_all_lock;
	}
	atomic_set(&cam_fs_enable, enable);
	if (enable == CAM_FS_DISABLE)
		check_mprovider_pid(false);

	if (enable == CAM_FS_ENABLE)
		check_mprovider_pid(true);

	log("[%s] cam_fs_enable set to %d\n", __func__, enable);
mybe_release_all_lock:
	mutex_lock(&update_lock);
	camfsdata_new = kmalloc(sizeof(struct cam_fs_data), GFP_KERNEL);
	if (camfsdata_new == NULL) {
		log("[%s] camfsdata_new kmalloc failed.\n", __func__);
		ret = -EINVAL;
	} else {
		clear_cam_fs_data(camfsdata_new);
		camfsdata_old = rcu_dereference_protected(camfsdata, lockdep_is_held(&update_lock));
		if (!camfsdata_old) {
			log("[%s] camfsdata_old is nullptr.\n", __func__);
			kfree(camfsdata_new);
			ret = -EINVAL;
		} else {
			if (READ_ONCE(camfsdata_old->i_ino) == CAM_FS_DEFAULT_VALUE) {
				log("[%s] user never set file lock before!.\n", __func__);
				kfree(camfsdata_new);
				ret = -EINVAL;
			} else {
				rcu_assign_pointer(camfsdata, camfsdata_new);
				synchronize_rcu();
				kfree(camfsdata_old);
				atomic_set(&cam_fs_f_wait, CAM_FS_FLAG_WAIT_RELEASE);
				log("[%s] camfsdata replace finish.\n", __func__);
			}
		}
	}
	mutex_unlock(&update_lock);

	return count;
}

static ssize_t cam_fs_drv_read(struct file *fp, char __user *buff,  size_t length, loff_t *ppos)
{
	int ret         = 0;
	int content_len = 0;
	int lock_tgid   = -1;
	unsigned long i_ino = -1;
	char content[1024]  = {0};
	struct cam_fs_data *local = NULL;

	if (*ppos >= sizeof(content))
		return 0;

	rcu_read_lock();
	local = rcu_dereference(camfsdata);
	if (local) {
		lock_tgid = READ_ONCE(local->lock_tgid);
		i_ino     = READ_ONCE(local->i_ino);
	}
	rcu_read_unlock();

	content_len = snprintf(content, sizeof(content), "Lock Inode tgid:%d i_ino:%lu icount:%d ctrl_thread:%d Lock_max_time:%d debug:%d app_pid:%d\n"
					, lock_tgid, i_ino, atomic_read(&cam_fs_lock_icount), atomic_read(&cam_fs_max_ctrl_threads)
					, atomic_read(&cam_fs_lock_max_time_ms), atomic_read(&cam_fs_debug)
					, atomic_read(&cam_fs_top_app_pid));

	ret = min_t(size_t, content_len - *ppos, length);
	if (ret <= 0)
		return 0;

	if (copy_to_user(buff, content + *ppos, ret))
		ret = -EFAULT;
	else
		*ppos += ret;

	return ret;
}

#define WRITE_OPT_TYPE_UNLOCK   1
#define WRITE_OPT_TYPE_DEBUG    7
#define WRITE_OPT_TYPE_ENABLE   2
#define WRITE_OPT_TYPE_TRY_LOCK 3
#define WRITE_OPT_TYPE_TOP_APP  8
#define WRITE_OPT_TYPE_LOCK_ICOUNT 4
#define WRITE_OPT_TYPE_LOCK_MAX_TIME_MS 5
#define WRITE_OPT_TYPE_MAX_CTRL_THREADS 6
static ssize_t cam_fs_drv_write(struct file *fp, const char *buff,  size_t length, loff_t *ppos)
{
	int idx     = 0;
	int i_val   = -1;
	int cpy_len = -1;
	char *rest	= NULL;
	char *token = NULL;
	int write_opt_type = -1;
	int retv = WRITE_ERROR_UNSPORT_MSG_TYPE;

	log("%s len:%zu", __func__, length);
	if (length <= 6)
		return WRITE_ERROR_LEN_INVALID;

	if (buff == NULL)
		return WRITE_ERROR_BUF_ADDR_INVALID;

	log("%s current thread tid:%d uid:%d\n", __func__, current->pid, from_kuid(current_user_ns(), current_uid()));

	//if (from_kuid(current_user_ns(), current_uid()) >= NORMAL_APP_UID)
	//	return WRITE_ERROR_NO_PERMISSION;

	mutex_lock(&dev_info.g_lock);
	memset(&(dev_info.d_cache), 0, sizeof(char) * CAM_FS_CMD_MAX_LEN);
	cpy_len = copy_from_user(&(dev_info.d_cache[0]), buff, length);
	if (cpy_len) {
		log("%s: copy_from_user func exec error:%d", __func__, cpy_len);
		retv = WRITE_ERR_FORMAT_INVALID;
		goto __func_done;
	}
	rest = &(dev_info.d_cache[0]);
	if (strstr(rest, SPLIT_DELIM) == NULL) {
		log("%s: copy rest:%s cpy_len:%d", __func__, rest, cpy_len);
		retv = WRITE_ERR_FORMAT_INVALID;
		goto __func_done;
	}

	log("%s: copy after:%s len:%zu cpy_len:%d", __func__, rest, length, cpy_len);

	if (strncmp(rest, "unlock", 6) == 0)
		write_opt_type = WRITE_OPT_TYPE_UNLOCK;
	else if (strncmp(rest, "enable", 6) == 0)
		write_opt_type = WRITE_OPT_TYPE_ENABLE;
	else if (strncmp(rest, "debug", 5) == 0)
		write_opt_type = WRITE_OPT_TYPE_DEBUG;
	else if (strncmp(rest, "tryLock", 7) == 0)
		write_opt_type = WRITE_OPT_TYPE_TRY_LOCK;
	else if (strncmp(rest, "topApp", 6) == 0)
		write_opt_type = WRITE_OPT_TYPE_TOP_APP;
	else if (strncmp(rest, "icount", 6) == 0)
		write_opt_type = WRITE_OPT_TYPE_LOCK_ICOUNT;
	else if (strncmp(rest, "lockMaxTime", 11) == 0)
		write_opt_type = WRITE_OPT_TYPE_LOCK_MAX_TIME_MS;
	else if (strncmp(rest, "maxCtrlThreads", 14) == 0)
		write_opt_type = WRITE_OPT_TYPE_MAX_CTRL_THREADS;

	log("%s: write opt type:%d ", __func__, write_opt_type);
	if (write_opt_type == -1) {
		retv = WRITE_ERROR_UNSPORT_MSG_TYPE;
		goto __func_done;
	}

	while ((token = strsep(&rest, SPLIT_DELIM)) != NULL) {
		if (idx > 2)
			break;
		log("%s: token: %s idx: %d write_opt_type: %d", __func__, token, idx, write_opt_type);
		if (idx == 1) {
			switch (write_opt_type) {
			case WRITE_OPT_TYPE_UNLOCK:
				if (cam_fs_unlock_handle(token, strnlen(token, CAM_FS_CMD_MAX_LEN)) >= 0)
					retv = length;
				break;
			case WRITE_OPT_TYPE_ENABLE:
				if (cam_fs_enable_handle(token, strnlen(token, CAM_FS_CMD_MAX_LEN)) >= 0)
					retv = length;
				break;
			case WRITE_OPT_TYPE_TRY_LOCK:
				if (cam_fs_try_lock_handle(token, strnlen(token, CAM_FS_CMD_MAX_LEN)) >= 0)
					retv = length;
				break;
			case WRITE_OPT_TYPE_LOCK_ICOUNT:
			case WRITE_OPT_TYPE_LOCK_MAX_TIME_MS:
			case WRITE_OPT_TYPE_MAX_CTRL_THREADS:
			case WRITE_OPT_TYPE_TOP_APP:
			case WRITE_OPT_TYPE_DEBUG:
				retv = kstrtoint(token, 10, &i_val);
				log("%s: token: %s idx: %d write_opt_type: %d retv: %d i_val: %d", __func__, token, idx, write_opt_type, retv, i_val);
				if (retv < 0)
					goto __func_done;

				if (i_val < 0)
					i_val = 0;

				if (write_opt_type == WRITE_OPT_TYPE_LOCK_ICOUNT)
					atomic_set(&cam_fs_lock_icount, i_val);
				else if (write_opt_type == WRITE_OPT_TYPE_LOCK_MAX_TIME_MS)
					atomic_set(&cam_fs_lock_max_time_ms, i_val);
				else if (write_opt_type == WRITE_OPT_TYPE_MAX_CTRL_THREADS)
					atomic_set(&cam_fs_max_ctrl_threads, i_val);
				else if (write_opt_type == WRITE_OPT_TYPE_DEBUG)
					atomic_set(&cam_fs_debug, i_val);
				else if (write_opt_type == WRITE_OPT_TYPE_TOP_APP)
					atomic_set(&cam_fs_top_app_pid, i_val);

				log("[%s] write opt_type: %d , store value %d\n", __func__, write_opt_type, i_val);
				retv = length;
				break;
			default:
				log("%s: write opt type:%d ", __func__, write_opt_type);
				break;
			}
		}
		idx++;
	}

__func_done:
	mutex_unlock(&dev_info.g_lock);
	return retv;
}
static int cam_fs_drv_release(struct inode *inode, struct file *filp)
{
	struct cam_fs_data *camfsdata_new = NULL;
	struct cam_fs_data *camfsdata_old = NULL;

	atomic_dec_if_positive(&cam_fs_open_drv_threads);

	if (atomic_read(&cam_fs_open_drv_threads) == 0) {
		mutex_lock(&update_lock);
		camfsdata_new = kmalloc(sizeof(struct cam_fs_data), GFP_KERNEL);
		if (camfsdata_new == NULL) {
			log("[%s] camfsdata_new kmalloc failed.\n", __func__);
		} else {
			clear_cam_fs_data(camfsdata_new);
			camfsdata_old = rcu_dereference_protected(camfsdata, lockdep_is_held(&update_lock));
			if (!camfsdata_old) {
				log("[%s] camfsdata_old is nullptr.\n", __func__);
				kfree(camfsdata_new);
			} else {
				if (READ_ONCE(camfsdata_old->i_ino) == CAM_FS_DEFAULT_VALUE) {
					log("[%s] user never set file lock before!.\n", __func__);
					kfree(camfsdata_new);
				} else {
					rcu_assign_pointer(camfsdata, camfsdata_new);
					synchronize_rcu();
					kfree(camfsdata_old);
					atomic_set(&cam_fs_f_wait, CAM_FS_FLAG_WAIT_RELEASE);
					log("[%s] camfsdata replace finish.\n", __func__);
				}
			}
		}
		mutex_unlock(&update_lock);
	}
	return 0;
}

static long cam_fs_drv_ioctl(struct file *filp, unsigned int cmd,  unsigned long arg)
{
	log("[%s] finish.\n", __func__);
	return 0;
}

static int cam_fs_drv_open(struct inode *inode, struct file *filp)
{
	log("%s, thread:%d file:%p\n", __func__, current->pid, filp);
	atomic_inc(&cam_fs_open_drv_threads);
	return 0;
}

static unsigned int cam_fs_drv_poll(struct file *file, struct poll_table_struct *poll_table)
{
	return POLLIN | POLLRDNORM;
}
static const struct file_operations dev_fops = {
	.owner =	THIS_MODULE,
	.open  = cam_fs_drv_open,
	.poll  = cam_fs_drv_poll,
	.read  = cam_fs_drv_read,
	.write = cam_fs_drv_write,
	.release = cam_fs_drv_release,
	.unlocked_ioctl = cam_fs_drv_ioctl,
};

static int __init cam_fs_init(void)
{
	int ret = 0;
	log("[%s] enter.\n", __func__);
	struct cam_fs_data *newone     = NULL;
	struct binder_data *b_data_new = NULL;

	newone = kmalloc(sizeof(struct cam_fs_data), GFP_KERNEL);
	if (newone == NULL) {
		log("[%s] cam_fs_data kmalloc failed.\n", __func__);
		return ret;
	}
	clear_cam_fs_data(newone);
	rcu_assign_pointer(camfsdata, newone);

	b_data_new = kmalloc(sizeof(struct binder_data), GFP_KERNEL);
	if (!b_data_new) {
		log("[%s] binder_data kmalloc failed.\n", __func__);
		return ret;
	}

	b_data_new->caller_pid    = -1;
	b_data_new->binder_th_tid = -1;
	b_data_new->t = NULL;
	b_data_new->transaction_received = false;
	write_lock(&b_data_rwlock);
	b_data = b_data_new;
	write_unlock(&b_data_rwlock);

	register_trace_android_vh_f2fs_file_open(cam_fs_android_vh_f2fs_file_open, NULL);
	//register_trace_android_vh_check_file_open(cam_fs_android_vh_f2fs_file_open, NULL);
	register_trace_android_vh_binder_proc_transaction(cam_fs_android_vh_binder_proc_transaction, NULL);
	register_trace_android_vh_binder_transaction_received(cam_fs_android_vh_binder_transaction_received, NULL);

	//create cam_fs_devices
	log("[%s] create cam_fs_devices.\n", __func__);

	strscpy(dev_info.name,    "cam_intentaware_fsys", 21);
	strscpy(dev_info.version, "2.0", 4);

	dev_info.cam_fs_major = register_chrdev(0, dev_info.name, &dev_fops);
	dev_info.cam_fs_class = class_create(dev_info.name);

	if (IS_ERR(dev_info.cam_fs_class)) {
		unregister_chrdev(dev_info.cam_fs_major, dev_info.name);
		unregister_trace_android_vh_f2fs_file_open(cam_fs_android_vh_f2fs_file_open, NULL);
		unregister_trace_android_vh_binder_proc_transaction(cam_fs_android_vh_binder_proc_transaction, NULL);
		unregister_trace_android_vh_binder_transaction_received(cam_fs_android_vh_binder_transaction_received, NULL);
		log("[%s] Failed to create class.\n", __func__);
		return ret;
	}

	log("[%s] MKDEV.\n", __func__);
	dev_info.devt   = MKDEV(dev_info.cam_fs_major, 0);
	dev_info.device = device_create(dev_info.cam_fs_class, NULL, dev_info.devt,
				NULL, "%s", dev_info.name);

	if (IS_ERR(dev_info.device)) {
		log("[%s] error while trying to create %s\n", __func__, dev_info.name);
		class_destroy(dev_info.cam_fs_class);
		unregister_chrdev(dev_info.cam_fs_major, dev_info.name);
		unregister_trace_android_vh_f2fs_file_open(cam_fs_android_vh_f2fs_file_open, NULL);
		unregister_trace_android_vh_binder_proc_transaction(cam_fs_android_vh_binder_proc_transaction, NULL);
		unregister_trace_android_vh_binder_transaction_received(cam_fs_android_vh_binder_transaction_received, NULL);
		return ret;
	}

	hash_init(mprovider_pids_hash);

	log("[%s] mutex_init.\n", __func__);
	mutex_init(&dev_info.g_lock);
	memset(&(dev_info.d_cache), 0, sizeof(char) * CAM_FS_CMD_MAX_LEN);

	log("[%s] finish.\n", __func__);
	return 1;
}

module_init(cam_fs_init);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("camera intentaware driver");
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
// END Camera_CameraOpt