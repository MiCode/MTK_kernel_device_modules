// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/ioctl.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/kfifo.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/slab.h>

#include "videogo_driver.h"
#include "videogo_public.h"
#include "videogo_param_config.h"
#include "videogo_utils.h"

#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
#include "eas/group.h"
#endif
#include "pf_ctrl.h"
#include "slbc_sdk.h"

static DECLARE_KFIFO(service_fifo, struct vgo_powerhal_info, 16);

static DECLARE_WAIT_QUEUE_HEAD(passive_wq);

static DECLARE_WAIT_QUEUE_HEAD(controller_wq);
static atomic_t controller_wq_ready;

struct workqueue_struct *delayed_wq;
static atomic_t runnable_boost_enable_cnt;
#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
static atomic_t cgrp_cnt;
#endif

static DECLARE_WAIT_QUEUE_HEAD(service_wq);

static struct task_struct *active_thread;

static LIST_HEAD(passive_workqueue);
//static struct list_head inst_list[MAX_CODEC_TYPE];
static struct list_head inst_list[MAX_CODEC_TYPE] = {
	LIST_HEAD_INIT(inst_list[VDEC]),
	LIST_HEAD_INIT(inst_list[VENC])
};
//static LIST_HEAD(transcoding_list);
static DEFINE_MUTEX(passive_workqueue_mutex);
static DEFINE_MUTEX(service_mutex);
struct mutex inst_list_mutex[MAX_CODEC_TYPE] = {
		__MUTEX_INITIALIZER(inst_list_mutex[VDEC]),
		__MUTEX_INITIALIZER(inst_list_mutex[VENC]) };
//static int inst_list_length[MAX_CODEC_TYPE] = {0};

static int major_num;
static struct class *videogo_class;
static struct device *videogo_device;
static struct cdev videogo_cdev;

// RUNNABLE_BOOST/MARGIN_CONTROL
static int set_runnable_boost_disable;
static int set_margin_ctrl;
static int set_uclamp_min_ta;
static int set_util_est_boost_disable;
static int set_rt_non_idle_preempt;
static int set_cpu_pf_ctrl;
static int set_slc_wce_ctrl;
static int set_ct_to_vip;
//static int set_cpu_freq_min;
static int set_gpu_freq_min;
static int target_fps_count[MAX_CODEC_TYPE] = {0};
static int alive_count[MAX_CODEC_TYPE] = {0};
static int isTranscoding;
//static struct task_struct *kvideogo_active;

static inline void videogo_read_and_set_bool(struct device_node *np, const char *prop_name, bool *var)
{
	u32 val;

	if (of_property_read_u32(np, prop_name, &val) == 0)
		*var = (val == 1);
	else
		mtk_vgo_info("Failed to read property %s\n", prop_name);
}

static inline void videogo_read_and_set_u32(struct device_node *np, const char *prop_name,
	u32 *var, bool *flag, int size)
{
	int ret;

	if (size > 1) {
		ret = of_property_read_u32_array(np, prop_name, var, size);
		if (ret < 0 || (!var[0] && !var[1] && !var[2])) {
			*flag = false;
			mtk_vgo_info("Failed to read property %s\n", prop_name);
		} else
			*flag = true;
	} else {
		ret = of_property_read_u32(np, prop_name, var);
		if (ret < 0) {
			*flag = false;
			mtk_vgo_info("Failed to read property %s\n", prop_name);
		} else
			*flag = true;
	}
}

static int videogo_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;

	if (!np) {
		mtk_vgo_err("Device node is NULL\n");
		return -ENODEV;
	}

	/* Create Instance*/
	videogo_read_and_set_bool(np, "cgroup-colocate", &mtk_vgo_cgroup_colocate);
	videogo_read_and_set_bool(np, "runnable-boost-enable", &mtk_vgo_runnable_boost_enable);

	/* VP Low Power */
	videogo_read_and_set_bool(np, "cpu-pf-ctrl", &mtk_vgo_cpu_pf_ctrl);
	videogo_read_and_set_bool(np, "rt-non-idle-preempt", &mtk_vgo_rt_non_idle_preempt);
	videogo_read_and_set_bool(np, "runnable-boost-disable", &mtk_vgo_runnable_boost_disable);
	videogo_read_and_set_bool(np, "slc-wce-ctrl", &mtk_vgo_slc_wce_ctrl);
	videogo_read_and_set_bool(np, "util-est-boost-disable", &mtk_vgo_util_est_boost_disable);
	videogo_read_and_set_u32(np, "margin-control", mtk_vgo_margin_ctrl_val, &mtk_vgo_margin_ctrl, 3);

	/* Transcoding */
	videogo_read_and_set_bool(np, "ct-to-vip", &mtk_vgo_ct_to_vip);
	videogo_read_and_set_u32(np, "gpu-freq-min-opp", &mtk_vgo_gpu_freq_min_opp, &mtk_vgo_gpu_freq_min, 1);
	videogo_read_and_set_u32(np, "uclamp-min-ta-val", &mtk_vgo_uclamp_min_ta_val, &mtk_vgo_uclamp_min_ta, 1);

	return 0;
}

static void videogo_remove(struct platform_device *pdev)
{
}

static const struct of_device_id videogo_of_match[] = {
	{ .compatible = "mediatek,videogo", },
	{}
};
MODULE_DEVICE_TABLE(of, videogo_of_match);

static struct platform_driver videogo_driver = {
	.probe = videogo_probe,
	.remove = videogo_remove,
	.driver = {
		.name = "videogo",
		.of_match_table = videogo_of_match,
	},
};

static void send_service_info(const char *log_msg, int service_type,
								int data0, int data1, int data2)
{
	struct vgo_powerhal_info service_info;

	mtk_vgo_debug("%s: %d %d %d", log_msg, data0, data1, data2);
	service_info.type = service_type;
	service_info.data[0] = data0;
	service_info.data[1] = data1;
	service_info.data[2] = data2;
	mutex_lock(&service_mutex);
	kfifo_in(&service_fifo, &service_info, 1);
	mutex_unlock(&service_mutex);

	wake_up_interruptible(&service_wq);
}

static void videogo_vcodec_send_fn(int iotype, void *data)
{
	struct data_entry *entry;

	entry = kmalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return;

	entry->type = iotype;

	switch(iotype) {
	case VGO_RECV_INSTANCE_INC:
	case VGO_RECV_INSTANCE_DEC:
		entry->data = kmalloc(sizeof(struct inst_init_data), GFP_KERNEL);
		if (!entry->data) {
			kfree(entry);
			return;
		}
		memcpy(entry->data, data, sizeof(struct inst_init_data));
		break;
	case VGO_RECV_RUNNING_UPDATE:
		entry->data = kmalloc(sizeof(struct inst_data), GFP_KERNEL);
		if (!entry->data) {
			kfree(entry);
			return;
		}
		memcpy(entry->data, data, sizeof(struct inst_data));
		break;
	case VGO_RECV_STATE_OPEN:
		entry->data = kmalloc(sizeof(struct oprate_data), GFP_KERNEL);
		if (!entry->data) {
			kfree(entry);
			return;
		}
		memcpy(entry->data, data, sizeof(struct oprate_data));
		break;
	default:
		mtk_vgo_err("type: %d not support", iotype);
		kfree(entry);
		return;
	}

	mutex_lock(&passive_workqueue_mutex);
	list_add_tail(&entry->list, &passive_workqueue);
	mutex_unlock(&passive_workqueue_mutex);

	wake_up_interruptible(&passive_wq);
}

static int is_transcoding(struct inst_node *info0, enum codec_type type)
{
	struct inst_node *info1, *tmp;
	enum codec_type list_type = abs(MAX_CODEC_TYPE - type - 1);
	int ret = 0;

	// Check has the same caller_pid of venc from vdec list
	// One VENC can be paired with multiple VDECs
	list_for_each_entry_safe(info1, tmp, &inst_list[list_type], list) {
		if (info1->caller_pid == info0->caller_pid) {
			// abnormal oprate, it means best effort mode
			if (info1->oprate > 960 || info0->oprate > 960)
				ret = 1;

			// oprate is 0, it means best effort mode
			// Need to avoid Vilte Scenario
			if (info1->oprate == 0 && info0->oprate == 0) {
				ret = 1;

				// Could be Vilte Scenario
				// TODO: Need to check by ViLTE Scenario Hints
				if (info0->width * info0->height <= FHD_SIZE &&
					info0->oprate_avdvfs < 35)
					ret = 0;
			}

			if (info1->oprate_avdvfs != 0 && info0->oprate_avdvfs != 0) {
				if ((info1->oprate != 0 || info0->oprate != 0) &&
					((abs(info1->oprate - info1->oprate_avdvfs) > 10) ||
					 (abs(info0->oprate - info0->oprate_avdvfs) > 10)))
					ret = 1;
			}
		}

		if (ret)
			break;
	}

	return ret;
}

static void videogo_work_handler(struct work_struct *work)
{
	struct vgo_delay_work *vgowork = container_of(to_delayed_work(work),
										struct vgo_delay_work, delayed_work);

	switch(vgowork->type) {
	case VGO_REL_RUNNABLE_BOOST_ENABLE:
		if (atomic_cmpxchg(&runnable_boost_enable_cnt, 1, 0) == 1)
			send_service_info("rel Runnable_boost_enable",
				VGO_RUNNABLE_BOOST_ENABLE, -1, 0, 0);
		else
			atomic_dec(&runnable_boost_enable_cnt);
		break;
	case VGO_REL_CGRP:
#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
		if (atomic_cmpxchg(&cgrp_cnt, 1, 0) == 1)
			group_set_cgroup_colocate(CGRP_FG, -1);
		else
			atomic_dec(&cgrp_cnt);
#endif
		break;
	}


	kvfree(vgowork);
}

static void videogo_send_delay_work(enum work_type type, unsigned int delay_ms, struct oprate_data *info)
{
	struct vgo_delay_work *work =
		kvzalloc(sizeof(struct vgo_delay_work), GFP_KERNEL);

	if (!work)
		return;

	work->type = type;
	work->inst_type = info->inst_type;
	work->ctx_id = info->ctx_id;

	INIT_DELAYED_WORK(&work->delayed_work, videogo_work_handler);

	if (!queue_delayed_work(delayed_wq, &work->delayed_work, msecs_to_jiffies(delay_ms)))
		kvfree(work);
}

static int videogo_process_data(int iotype, void *data)
{
	struct inst_node *info0, *tmp;
	int ret = VGO_IDLE;

	if (iotype == VGO_RECV_INSTANCE_INC) {
		struct inst_init_data *init_data = (struct inst_init_data *)data;
		int type = init_data->inst_type;

		if (type >= 0 && type < MAX_CODEC_TYPE) {
			info0 = kmalloc(sizeof(*info0), GFP_KERNEL);
			if (!info0)
				return -ENOMEM;

			info0->inst_type = type;
			info0->ctx_id = init_data->ctx_id;
			info0->caller_pid = init_data->caller_pid;
			info0->fourcc = init_data->fourcc;
			info0->oprate = init_data->oprate;
			info0->oprate_vgo = 0;

			if ((init_data->width > 0 && init_data->width < INT_MAX) &&
				(init_data->height > 0 && init_data->height < INT_MAX)) {
				info0->width = init_data->width;
				info0->height = init_data->height;
			} else {
				kfree(info0);
				return -EINVAL;
			}

			memset(info0->hw_proc_time, 0, sizeof(info0->hw_proc_time));
			info0->post_proc_time = 0;
			INIT_LIST_HEAD(&info0->list);

			mutex_lock(&inst_list_mutex[type]);
			list_add(&info0->list, &inst_list[type]);

			alive_count[type]++;
			if (info0->oprate && info0->oprate <= TARGET_FPS)
				target_fps_count[type]++;
			mutex_unlock(&inst_list_mutex[type]);

			if (!isTranscoding)
				isTranscoding = is_transcoding(info0, type);

		} else {
			mtk_vgo_err("Invalid inst_type: [%d] %d", init_data->ctx_id, init_data->inst_type);
			return -EINVAL;
		}
	} else if (iotype == VGO_RECV_INSTANCE_DEC) {
		struct inst_init_data *inst_data = (struct inst_init_data *)data;
		int type = inst_data->inst_type;

		if (type >= 0 && type < MAX_CODEC_TYPE) {
			mutex_lock(&inst_list_mutex[type]);

			list_for_each_entry_safe(info0, tmp, &inst_list[type], list) {
				if (info0->ctx_id == inst_data->ctx_id &&
					info0->inst_type == type) {

					alive_count[type]--;
					if (TARGET_FPS_CHECKER(info0->oprate_avdvfs, TARGET_FPS, 20))
						target_fps_count[type]--;
					list_del(&info0->list);
					kfree(info0);
					break;
				}
			}
			mutex_unlock(&inst_list_mutex[type]);

			isTranscoding = alive_count[VENC] == 0 ? 0 : isTranscoding;
		} else {
			mtk_vgo_err("Invalid inst_type: [%d] %d", inst_data->ctx_id, inst_data->inst_type);
			return -EINVAL;
		}
	} else if (iotype == VGO_RECV_RUNNING_UPDATE) {
		struct inst_data *run_data = (struct inst_data *)data;
		struct inst_node *target_inst_info = NULL;
		int type = run_data->inst_type;

		if (type >= 0 && type < MAX_CODEC_TYPE) {
			mutex_lock(&inst_list_mutex[type]);
			target_fps_count[type] = 0;
			list_for_each_entry_safe(info0, tmp, &inst_list[type], list) {
				if (info0->ctx_id == run_data->ctx_id &&
					info0->inst_type == run_data->inst_type) {
					info0->oprate_avdvfs = run_data->oprate;
					memcpy(info0->hw_proc_time, run_data->hw_proc_time,
						   sizeof(info0->hw_proc_time));

					target_inst_info = info0;
				}

				if (TARGET_FPS_CHECKER(info0->oprate_avdvfs, TARGET_FPS, 20))
					target_fps_count[type]++;
			}
			mutex_unlock(&inst_list_mutex[type]);

		//struct oprate_data oprate_vgo;
		//oprate_vgo.inst_type = type;
		//oprate_vgo.ctx_id = info0->ctx_id;
		//oprate_vgo.oprate = info0->oprate_avdvfs;
		//mtk_vcodec_vgo_send(VGO_SEND_OPRATE, videogo_vcodec_send_fn);

			if (target_inst_info != NULL) {
				if (!isTranscoding && type == VENC)
					isTranscoding = is_transcoding(target_inst_info, type);

				mtk_vgo_info("[%d][%d] oprate_avdvfs:%d oprate:%d isTrans:%d",
					target_inst_info->ctx_id, target_inst_info->inst_type,
					target_inst_info->oprate_avdvfs, target_inst_info->oprate, isTranscoding);
			} else
				mtk_vgo_err("[%d][%d] Cannot find Inst", run_data->ctx_id, run_data->inst_type);
		} else {
			mtk_vgo_err("[%d][%d] Invalid inst_type", run_data->ctx_id, run_data->inst_type);
			return -EINVAL;
		}
	} else if (iotype == VGO_RECV_STATE_OPEN) {
		struct oprate_data *dev_data = (struct oprate_data *)data;

		if (mtk_vgo_runnable_boost_enable) {
			if (atomic_cmpxchg(&runnable_boost_enable_cnt, 0, 1) == 0)
				send_service_info("acq runnable_boost_enable",
					VGO_RUNNABLE_BOOST_ENABLE, 1, 0, 0);
			else
				atomic_inc(&runnable_boost_enable_cnt);
			videogo_send_delay_work(VGO_REL_RUNNABLE_BOOST_ENABLE, 40, dev_data);
		}

#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
		if (mtk_vgo_cgroup_colocate) {
			if (atomic_cmpxchg(&cgrp_cnt, 0, 1) == 0) {
				mtk_vgo_debug("acq cgroup_colocate");
				group_set_cgroup_colocate(CGRP_FG, 0);
			} else
				atomic_inc(&cgrp_cnt);
			videogo_send_delay_work(VGO_REL_CGRP, 200, dev_data);
		}
#endif
	}

	return ret;
}

static int videogo_passive_fn(void *arg)
{
	struct data_entry *entry;
	int ret = 0;

	while(!kthread_should_stop()) {
		ret = wait_event_interruptible(passive_wq, !list_empty(&passive_workqueue) ||
				kthread_should_stop());

		if (ret < 0) {
			mtk_vgo_info("wait event return=%d", ret);
			continue;
		}

		if (kthread_should_stop())
			break;

		mutex_lock(&passive_workqueue_mutex);
		while(!list_empty(&passive_workqueue)) {

			entry = list_first_entry(&passive_workqueue, struct data_entry, list);
			videogo_process_data(entry->type, entry->data);

			list_del(&entry->list);
			kfree(entry->data);
			kfree(entry);
		}
		mutex_unlock(&passive_workqueue_mutex);

		mtk_vgo_debug("Notify controller_wq");
		// if need to notify controller thread
		atomic_inc(&controller_wq_ready);
		wake_up_interruptible(&controller_wq);
	}

	return 0;
}

static int videogo_controller_fn(void *arg)
{
	int ret = 0;

	while(!kthread_should_stop()) {
		ret = wait_event_interruptible(controller_wq,
				 atomic_read(&controller_wq_ready) > 0 || kthread_should_stop());

		if (ret < 0) {
			mtk_vgo_info("wait event return=%d", ret);
			continue;
		}

		if (kthread_should_stop())
			break;
		atomic_dec(&controller_wq_ready);

		int total_vdec_target_fps = target_fps_count[VDEC];
		int total_vdec = alive_count[VDEC];
		int total_venc = alive_count[VENC];

		if (total_vdec_target_fps > 0 && total_vdec_target_fps <= 2 &&
			!total_venc && total_vdec_target_fps == total_vdec) {
			if (!set_runnable_boost_disable && mtk_vgo_runnable_boost_disable) {
				send_service_info("acq runnable_boost_disable",
								VGO_RUNNABLE_BOOST_DISABLE, 1, 0, 0);
				set_runnable_boost_disable = 1;
			}
			if (!set_margin_ctrl && mtk_vgo_margin_ctrl) {
				send_service_info("acq margin_ctrl",
								VGO_MARGIN_CONTROL_0,
								mtk_vgo_margin_ctrl_val[0], mtk_vgo_margin_ctrl_val[1],
								mtk_vgo_margin_ctrl_val[2]);
				set_margin_ctrl = 1;
			}
			if (!set_util_est_boost_disable && mtk_vgo_util_est_boost_disable) {
				send_service_info("acq util_est_boost",
								VGO_UTIL_EST_BOOST, 0, 0, 0);
				set_util_est_boost_disable = 1;
			}
			if (!set_rt_non_idle_preempt && mtk_vgo_rt_non_idle_preempt) {
				send_service_info("acq rt_non_idle_preempt",
								VGO_RT_NON_IDLE_PREEMPT, 1, 0, 0);
				set_rt_non_idle_preempt = 1;
			}
			if (!set_cpu_pf_ctrl && mtk_vgo_cpu_pf_ctrl) {
				if (mtk_set_pf_ctrl_enable(true, PF_CTRL_USER_VP) == 0)
					set_cpu_pf_ctrl = 1;
				else
					mtk_vgo_err("Failed to mtk_set_pf_ctrl_enable");
				ret = mtk_get_pf_ctrl_enable();
				mtk_vgo_debug("acq cpu_pf_ctrl_%s: %d", ret ? "enable" : "disable", ret);
			}
			if (!set_slc_wce_ctrl && mtk_vgo_slc_wce_ctrl) {
				slbc_disable_dcc(true); // 1: disable WCE, 0: enable (default)
				mtk_vgo_debug("acq slc_wce_ctrl");
				set_slc_wce_ctrl = 1;
			}
			mtk_vgo_info("[VP] runnable_disable:%d margin_ctrl:%d util_boost_disable:%d rt_non_idle:%d pf_ctrl:%d slc_wce_ctrl:%d",
				set_runnable_boost_disable, set_margin_ctrl,
				set_util_est_boost_disable, set_rt_non_idle_preempt,
				set_cpu_pf_ctrl, set_slc_wce_ctrl);
		} else {
			if (set_runnable_boost_disable) {
				send_service_info("rel Runnable_boost_disable",
								VGO_RUNNABLE_BOOST_DISABLE, -1, 0, 0);
				set_runnable_boost_disable = 0;
			}
			if (set_margin_ctrl) {
				send_service_info("rel margin_ctrl",
								VGO_MARGIN_CONTROL_0, -1, 0, 0);
				set_margin_ctrl = 0;
			}
			if (set_util_est_boost_disable) {
				send_service_info("rel util_est_boost_disable",
								VGO_UTIL_EST_BOOST, -1, 0, 0);
				set_util_est_boost_disable = 0;
			}
			if (set_rt_non_idle_preempt) {
				send_service_info("rel rt_non_idle_preempt",
								VGO_RT_NON_IDLE_PREEMPT, -1, 0, 0);
				set_rt_non_idle_preempt = 0;
			}
			if (set_cpu_pf_ctrl) {
				mtk_set_pf_ctrl_enable(false, PF_CTRL_USER_VP);
				ret = mtk_get_pf_ctrl_enable();
				mtk_vgo_debug("rel cpu_pf_ctrl_%s: %d", ret ? "enable" : "disable", ret);
				set_cpu_pf_ctrl = 0;
			}
			if (set_slc_wce_ctrl) {
				slbc_disable_dcc(false);
				mtk_vgo_debug("rel slc_wce_ctrl");
				set_slc_wce_ctrl = 0;
			}
		}
		if (isTranscoding) {
			if (!set_uclamp_min_ta && mtk_vgo_uclamp_min_ta) {
				send_service_info("acq uclamp_min_ta",
					VGO_UCLAMP_MIN_TA, mtk_vgo_uclamp_min_ta_val, 0, 0);
				set_uclamp_min_ta = 1;
			}
			if (!set_gpu_freq_min && mtk_vgo_gpu_freq_min) {
				send_service_info("acq gpu_freq_min",
					VGO_GPU_FREQ_MIN, mtk_vgo_gpu_freq_min_opp, 0, 0);
				set_gpu_freq_min = 1;
			}
			if (!set_ct_to_vip && mtk_vgo_ct_to_vip) {
				mtk_vgo_debug("acq ct_to_vip");
				enforce_ct_to_vip(1, 3);
				set_ct_to_vip = 1;
			}
			mtk_vgo_info("[TRANS] uclamp_min:%d gpu_min:%d ta_vip:%d",
				set_uclamp_min_ta, set_gpu_freq_min, set_ct_to_vip);
		} else {
			if (set_uclamp_min_ta) {
				send_service_info("rel uclamp_min_ta",
								VGO_UCLAMP_MIN_TA, -1, 0, 0);
				set_uclamp_min_ta = 0;
			}
			if (set_gpu_freq_min) {
				send_service_info("rel gpu_freq_min",
								VGO_GPU_FREQ_MIN, -1, 0, 0);
				set_gpu_freq_min = 0;
			}
			if (set_ct_to_vip) {
				mtk_vgo_debug("rel ct_to_vip");
				enforce_ct_to_vip(0, 3);
				set_ct_to_vip = 0;
			}
		}

		mtk_vgo_debug("runnable_disable:%d margin_ctrl:%d util_boost_disable:%d rt_non_idle:%d uclamp_min:%d gpu_min:%d ta_vip:%d",
			set_runnable_boost_disable, set_margin_ctrl,
			set_util_est_boost_disable, set_rt_non_idle_preempt,
			set_uclamp_min_ta, set_gpu_freq_min, set_ct_to_vip);
	}
	return 0;
}

static int videogo_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int videogo_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long videogo_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;
	struct inst_data_user data;
	struct vgo_powerhal_info service_info;

	switch (cmd) {
	case VGO_IOCTL_SET_PROCTIME:
		if (copy_from_user(&data, (struct inst_data_user __user *)arg, sizeof(data))) {
			mtk_vgo_err("VGO_IOCTL_SET_PROCTIME: copy_from_user is fail");
			return -EFAULT;
		}
		mtk_vgo_debug("Recv data: ctx_id=%d, avg=%d, max=%d, min=%d, count=%d",
				data.ctx_id, data.avg_proc_time, data.max_proc_time,
				data.min_proc_time, data.count);
		break;
	case VGO_IOCTL_GET:
		wait_event_interruptible(service_wq, !kfifo_is_empty(&service_fifo));
		mutex_lock(&service_mutex);
		ret = kfifo_out(&service_fifo, &service_info, 1);
		mutex_unlock(&service_mutex);
		if (ret != 1) {
			mtk_vgo_err("kfifo_out is abnormal: %d", ret);
			return -EFAULT;
		}
		if (copy_to_user((struct vgo_powerhal_info __user *)arg,
					 &service_info, sizeof(service_info))) {
			return -EFAULT;
		}
		break;
	default:
		return -EFAULT;
	}

	return 0;
}

static const struct file_operations fops = {
	.open = videogo_open,
	.release = videogo_release,
	.unlocked_ioctl = videogo_ioctl,
};

/*
 * driver initialization entry point
 */
static struct task_struct *passive_thread;
static struct task_struct *controller_thread;

static int __init videogo_init(void)
{
	int ret;
	dev_t dev;

	INIT_KFIFO(service_fifo);
	init_waitqueue_head(&passive_wq);
	init_waitqueue_head(&controller_wq);
	mutex_init(&passive_workqueue_mutex);
	mutex_init(&service_mutex);

	mtk_vgo_info("videogo: _init");

	ret = platform_driver_register(&videogo_driver);
	if (ret)
		mtk_vgo_err("Filed to register videogo driver: %d", ret);

	ret = register_chrdev(0, DEVICE_NAME, &fops);
	if (ret < 0) {
		mtk_vgo_err("Failed to register character device:%s", DEVICE_NAME);
		return ret;
	}
	major_num = ret;

	cdev_init(&videogo_cdev, &fops);
	videogo_cdev.owner = THIS_MODULE;

	dev = MKDEV(major_num, 0);
	ret = cdev_add(&videogo_cdev, dev, 1);
	if (ret <0) {
		unregister_chrdev(major_num, DEVICE_NAME);
		mtk_vgo_err("Adding cdev failed with:%d", ret);
		return ret;
	}

	videogo_class = class_create(CLASS_NAME);
	if (IS_ERR(videogo_class)) {
		unregister_chrdev(major_num, DEVICE_NAME);
		mtk_vgo_err("videogo: Failed to register device class:%s", CLASS_NAME);
		return PTR_ERR(videogo_class);
	}

	videogo_device = device_create(videogo_class, NULL,
						 MKDEV(major_num, 0), NULL, DEVICE_NAME);
	if (IS_ERR(videogo_device)) {
		class_destroy(videogo_class);
		unregister_chrdev(major_num, DEVICE_NAME);
		mtk_vgo_err("Failed to create the device:%s", DEVICE_NAME);
		return PTR_ERR(videogo_device);
	}

	passive_thread = kthread_run(videogo_passive_fn, NULL, "videogo_passive");
	if (IS_ERR(passive_thread)) {
		mtk_vgo_err("Failed to create videogo_passive thread");
		return PTR_ERR(passive_thread);
	}

	controller_thread = kthread_run(videogo_controller_fn, NULL, "videogo_controller");
	if (IS_ERR(controller_thread)) {
		mtk_vgo_err("Failed to create videogo_controller thread");
		kthread_stop(passive_thread);
		return PTR_ERR(controller_thread);
	}

	mtk_vcodec_vgo_send(VGO_SEND_UPDATE_FN, videogo_vcodec_send_fn);

	delayed_wq = create_workqueue("videogo_delayed_wq");

	mtk_vgo_info("videogo: module loaded");

	return 0;
}

/*
 * driver exit point
 */
static void videogo_exit(void)
{
	struct inst_node *info, *tmp;
	int i;

	kthread_stop(passive_thread);
	kthread_stop(controller_thread);

	if (active_thread != NULL)
		kthread_stop(active_thread);

    // Clean instance list
	for (i = 0; i < MAX_CODEC_TYPE; i++) {
		mutex_lock(&inst_list_mutex[i]);
		list_for_each_entry_safe(info, tmp, &inst_list[i], list) {
			list_del(&info->list);
			kfree(info);
		}
		mutex_unlock(&inst_list_mutex[i]);
	}

	// Clean passive_workqueue
	mutex_lock(&passive_workqueue_mutex);
	while(!list_empty(&passive_workqueue)) {
		struct data_entry *entry;

		entry = list_first_entry(&passive_workqueue, struct data_entry, list);

		list_del(&entry->list);
		kfree(entry->data);
		kfree(entry);
	}
	mutex_unlock(&passive_workqueue_mutex);

	destroy_workqueue(delayed_wq);

	device_destroy(videogo_class, MKDEV(major_num, 0));
	class_unregister(videogo_class);
	class_destroy(videogo_class);
	cdev_del(&videogo_cdev);
	unregister_chrdev(major_num, DEVICE_NAME);

	platform_driver_unregister(&videogo_driver);
}

module_init(videogo_init);
module_exit(videogo_exit);

MODULE_DESCRIPTION("MEDIATEK Module VIDEOGO driver");
MODULE_AUTHOR("Mediatek");
MODULE_LICENSE("GPL");
