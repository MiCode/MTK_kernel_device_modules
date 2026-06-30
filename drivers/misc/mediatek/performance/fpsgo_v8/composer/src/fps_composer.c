// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/slab.h>
#include <linux/device.h>
#include <linux/seq_file.h>
#include <linux/security.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/sched/task.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/irq_work.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include "sugov/cpufreq.h"

#include <mt-plat/fpsgo_common.h>
#include "fpsgo_frame_info.h"
#include "fpsgo_base.h"
#include "fpsgo_sysfs.h"
#include "fpsgo_usedext.h"
#include "fps_composer.h"
#include "fbt_cpu.h"
#include "fstb.h"
#include "xgf.h"
#include "mini_top.h"
#include "fbt_cpu_platform.h"

#define MAX_FPSGO_CB_NUM 5

static struct kobject *comp_kobj;
static struct workqueue_struct *composer_wq;
static struct hrtimer recycle_hrt;
static struct rb_root fpsgo_com_policy_cmd_tree;
static int receive_fw_info_enable;
static int recycle_idle_cnt;
static int recycle_active = 1;
static int total_fpsgo_com_policy_cmd_num;
static int fpsgo_is_boosting;
static int jank_detection_is_ready;

// touch latency
static int fpsgo_touch_latency_ko_ready;

static void fpsgo_com_notify_to_do_recycle(struct work_struct *work);
static DECLARE_WORK(do_recycle_work, fpsgo_com_notify_to_do_recycle);
static DEFINE_MUTEX(recycle_lock);
static DEFINE_MUTEX(fpsgo_com_policy_cmd_lock);
static DEFINE_MUTEX(fpsgo_boost_lock);
static DEFINE_MUTEX(user_hint_lock);
static DEFINE_MUTEX(fpsgo_frame_info_cb_lock);
static DEFINE_MUTEX(wait_enable_lock);
static DEFINE_MUTEX(cur_l2q_info_lock);

static struct render_frame_info_cb fpsgo_frame_info_cb_list[FPSGO_MAX_CALLBACK_NUM];
static struct FSTB_FRAME_L2Q_INFO cur_l2q_info[MAX_SF_BUFFER_SIZE];
static int cur_l2q_info_index;


/*
 * TODO(CHI): need to remove, jank detection need to notify FPSGO specific task no boost
 *            via fpsgo_get_no_boost_info() and fpsgo_delete_no_boost_info(),
 *            and then boost specific task by itself
 */
typedef void (*heavy_fp)(int jank, int pid);
int (*fpsgo2jank_detection_register_callback_fp)(heavy_fp cb);
EXPORT_SYMBOL(fpsgo2jank_detection_register_callback_fp);
int (*fpsgo2jank_detection_unregister_callback_fp)(heavy_fp cb);
EXPORT_SYMBOL(fpsgo2jank_detection_unregister_callback_fp);

static enum hrtimer_restart prepare_do_recycle(struct hrtimer *timer)
{
	if (composer_wq)
		queue_work(composer_wq, &do_recycle_work);
	else
		schedule_work(&do_recycle_work);

	return HRTIMER_NORESTART;
}

static void fpsgo_com_prepare_to_do_recycle(void)
{
	mutex_lock(&recycle_lock);
	if (recycle_idle_cnt) {
		recycle_idle_cnt = 0;
		if (!recycle_active) {
			recycle_active = 1;
			hrtimer_start(&recycle_hrt, ktime_set(0, NSEC_PER_SEC), HRTIMER_MODE_REL);
		}
	}
	mutex_unlock(&recycle_lock);
}

void fpsgo_com_notify_fpsgo_is_boost(int enable)
{
	mutex_lock(&fpsgo_boost_lock);
	if (!fpsgo_is_boosting && enable) {
		fpsgo_is_boosting = 1;
#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
		if (fpsgo_notify_fbt_is_boost_fp)
			fpsgo_notify_fbt_is_boost_fp(1);
#endif
	} else if (fpsgo_is_boosting && !enable) {
		fpsgo_is_boosting = 0;
#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
		if (fpsgo_notify_fbt_is_boost_fp)
			fpsgo_notify_fbt_is_boost_fp(0);
#endif
	}
	mutex_unlock(&fpsgo_boost_lock);
}

int register_fpsgo_frame_info_callback(unsigned long mask, fpsgo_frame_info_callback cb)
{
	int i = 0;
	int ret = -ENOMEM;
	struct render_frame_info_cb *iter = NULL;

	mutex_lock(&fpsgo_frame_info_cb_lock);
	for (i = 0; i < FPSGO_MAX_CALLBACK_NUM; i++) {
		if (fpsgo_frame_info_cb_list[i].func_cb == NULL)
			break;
	}

	if (i >= FPSGO_MAX_CALLBACK_NUM || i < 0)
		goto out;

	iter = &fpsgo_frame_info_cb_list[i];
	iter->mask = mask;
	iter->func_cb = cb;
	memset(&iter->info_iter, 0, sizeof(struct render_frame_info));
	ret = i;

out:
	mutex_unlock(&fpsgo_frame_info_cb_lock);
	return ret;
}
EXPORT_SYMBOL(register_fpsgo_frame_info_callback);

int unregister_fpsgo_frame_info_callback(fpsgo_frame_info_callback cb)
{
	int i = 0;
	int ret = -ESPIPE;
	struct render_frame_info_cb *iter = NULL;

	mutex_lock(&fpsgo_frame_info_cb_lock);
	for (i = 0; i < FPSGO_MAX_CALLBACK_NUM; i++) {
		iter = &fpsgo_frame_info_cb_list[i];
		if (iter->func_cb == cb) {
			iter->mask = 0;
			iter->func_cb = NULL;
			memset(&iter->info_iter, 0, sizeof(struct render_frame_info));
			ret = i;
		}
	}
	mutex_unlock(&fpsgo_frame_info_cb_lock);

	return ret;
}
EXPORT_SYMBOL(unregister_fpsgo_frame_info_callback);

/* This function should not be wrapped in any locks*/
void fpsgo_notify_frame_info_callback(int pid, unsigned long cmd,
	unsigned long long buffer_id, struct render_frame_info *r_iter)
{
	int i;
	struct render_frame_info_cb *iter = NULL;

	mutex_lock(&fpsgo_frame_info_cb_lock);
	for (i = 0; i < FPSGO_MAX_CALLBACK_NUM; i++) {
		iter = &fpsgo_frame_info_cb_list[i];

		if (iter->func_cb && cmd & iter->mask) {
			if (r_iter)
				memcpy(&iter->info_iter, r_iter, sizeof(struct render_frame_info));
			iter->info_iter.tgid = fpsgo_get_tgid(pid);
			iter->info_iter.pid = pid;
			iter->info_iter.buffer_id = buffer_id;

			iter->func_cb(cmd, &iter->info_iter);
		}
	}
	mutex_unlock(&fpsgo_frame_info_cb_lock);
}

static int fpsgo_comp_make_pair_l2q(struct render_info *f_render,
	unsigned long long cur_queue_end, unsigned long long logic_head_ts, int has_logic_head,
	int is_logic_valid, unsigned long long sf_buf_id)
{
	struct FSTB_FRAME_L2Q_INFO *prev_l2q_info, *cur_l2q_info;
	int prev_l2q_index = -1, cur_l2q_index = -1;
	int ret = 0;
	unsigned long long expected_fps = 0, expected_time = 0;

	prev_l2q_index = f_render->l2q_index;
	cur_l2q_index = (f_render->l2q_index + 1) % MAX_SF_BUFFER_SIZE;
	prev_l2q_info = &(f_render->l2q_info[prev_l2q_index]);
	cur_l2q_info = &(f_render->l2q_info[cur_l2q_index]);
	f_render->l2q_index = cur_l2q_index;
	if (!prev_l2q_info || !cur_l2q_info) {
		ret = -EINVAL;
		goto out;
	}

	expected_fps = (unsigned long long) f_render->boost_info.target_fps;
	expected_time = div64_u64(NSEC_PER_SEC, expected_fps);
	cur_l2q_info->sf_buf_id = sf_buf_id;
	cur_l2q_info->queue_end_ns = cur_queue_end;

	if (logic_head_ts && cur_queue_end > logic_head_ts) {
		if (is_logic_valid != 0) {
			fpsgo_main_trace("[%s]pid=%d, cur_que_end=%llu, logic_head_ts=%llu", __func__,
				f_render->pid, cur_queue_end, logic_head_ts);
			ret = 1;
		}
		cur_l2q_info->logic_head_fixed_ts = logic_head_ts;
		cur_l2q_info->logic_head_ts = logic_head_ts;
	} else {
		cur_l2q_info->logic_head_fixed_ts = prev_l2q_info->logic_head_fixed_ts +
			expected_time;
		cur_l2q_info->logic_head_ts = 0;
	}
	cur_l2q_info->is_logic_head_alive = has_logic_head;

	if (cur_queue_end > cur_l2q_info->logic_head_fixed_ts)
		cur_l2q_info->l2q_ts = cur_queue_end - cur_l2q_info->logic_head_fixed_ts;
	else  // Error handling 拿前一框L2Q
		cur_l2q_info->l2q_ts = prev_l2q_info->l2q_ts;

	fpsgo_main_trace("[fstb_logical][%d]l_ts=%llu,l_fixed_ts=%llu,q=%llu,l2q_ts=%llu,exp_t=%llu,has=%d",
		f_render->pid, cur_l2q_info->logic_head_ts, cur_l2q_info->logic_head_fixed_ts,
		cur_queue_end, cur_l2q_info->l2q_ts, expected_time,
		cur_l2q_info->is_logic_head_alive);
	fpsgo_systrace_c_fstb_man(f_render->pid, f_render->buffer_id,
			div64_u64(cur_l2q_info->logic_head_fixed_ts, 1000000), "L_fixed");
	fpsgo_systrace_c_fstb_man(f_render->pid, f_render->buffer_id, cur_l2q_info->sf_buf_id,
		"L2Q_sf_buf_id");
	fpsgo_systrace_c_fstb_man(f_render->pid, f_render->buffer_id, cur_l2q_info->l2q_ts,
		"L2Q_ts");
	fpsgo_systrace_c_fstb_man(f_render->pid, f_render->buffer_id,
		div64_u64(cur_queue_end, 1000000), "L2Q_q_end_ts");
	fpsgo_systrace_c_fstb_man(f_render->pid, f_render->buffer_id,
		div64_u64(logic_head_ts, 1000000), "Logical ts");
out:
	return ret;
}

/*
 * TODO(CHI): need to remove, jank detection need to notify FPSGO specific task no boost
 *            via fpsgo_get_no_boost_info() and fpsgo_delete_no_boost_info(),
 *            and then boost specific task by itself
 */
static void fpsgo_com_do_jank_detection_hint(struct work_struct *psWork)
{
	int local_jank;
	int local_pid;
	struct jank_detection_hint *hint = NULL;

	hint = container_of(psWork, struct jank_detection_hint, sWork);
	local_jank = hint->jank;
	local_pid = hint->pid;

	fpsgo_render_tree_lock(__func__);
	fpsgo_systrace_c_fbt(local_pid, 0, local_jank, "jank_detection_hint");
	fpsgo_comp2fbt_jank_thread_boost(local_jank, local_pid);
	fpsgo_render_tree_unlock(__func__);

	kfree(hint);
}

static void fpsgo_com_receive_jank_detection(int jank, int pid)
{
	struct jank_detection_hint *hint = NULL;

	fpsgo_systrace_c_fbt(pid, 0, jank, "jank_detection_hint_ori");

	if (!composer_wq)
		return;

	hint = kmalloc(sizeof(struct jank_detection_hint), GFP_ATOMIC);
	if (!hint)
		return;

	hint->jank = jank;
	hint->pid = pid;

	INIT_WORK(&hint->sWork, fpsgo_com_do_jank_detection_hint);
	queue_work(composer_wq, &hint->sWork);
}

static void fpsgo_com_get_l2q_time(int pid, unsigned long long buf_id, int tgid,
		unsigned long long enqueue_end_time, unsigned long long prev_queue_end_ts,
		unsigned long long pprev_queue_end_ts, unsigned long long dequeue_start_ts,
		unsigned long long sf_buf_id, struct render_info *f_render)
{
		int l2q_enable_pid_final = 0;

		if (!f_render)
			return;

		l2q_enable_pid_final = f_render->attr.l2q_enable_by_pid;
		if (fpsgo_touch_latency_ko_ready && l2q_enable_pid_final) {
			fpsgo_comp_make_pair_l2q(f_render, enqueue_end_time, f_render->logic_head_ts,
				f_render->has_logic_head, f_render->is_logic_valid, sf_buf_id);
		}
}

static void fpsgo_comp_set_logic_head(struct render_info *f_render,
	unsigned long long sf_buf_id, unsigned long long logic_head_ts,
	int logic_head_is_valid)
{
	unsigned long long expected_fps = 0, expected_time = 0;
	int prev_l2q_index = 0;

	if (!f_render)
		return;

	mutex_lock(&cur_l2q_info_lock);
	cur_l2q_info[cur_l2q_info_index].pid = f_render->pid;
	cur_l2q_info[cur_l2q_info_index].buf_id = f_render->buffer_id;
	cur_l2q_info[cur_l2q_info_index].sf_buf_id = sf_buf_id;

	expected_fps = (unsigned long long) f_render->boost_info.target_fps;
	expected_time = div64_u64(NSEC_PER_SEC, expected_fps);

	if (logic_head_ts) {
		cur_l2q_info[cur_l2q_info_index].logic_head_fixed_ts = logic_head_ts;
		cur_l2q_info[cur_l2q_info_index].logic_head_ts = logic_head_ts;
	} else {
		cur_l2q_info[cur_l2q_info_index].logic_head_fixed_ts = 0;
		cur_l2q_info[cur_l2q_info_index].logic_head_ts = 0;
		if (cur_l2q_info_index == 0)
			prev_l2q_index = MAX_SF_BUFFER_SIZE - 1;
		else
			prev_l2q_index = cur_l2q_info_index - 1;
		while (prev_l2q_index != cur_l2q_info_index) {
			if (cur_l2q_info[prev_l2q_index].pid == cur_l2q_info[cur_l2q_info_index].pid) {
				cur_l2q_info[cur_l2q_info_index].logic_head_fixed_ts =
					cur_l2q_info[prev_l2q_index].logic_head_fixed_ts + expected_time;
				break;
			}
			if (prev_l2q_index == 0)
				prev_l2q_index = MAX_SF_BUFFER_SIZE - 1;
			else
				prev_l2q_index--;
		}
	}
	cur_l2q_info[cur_l2q_info_index].is_logic_head_alive = logic_head_is_valid;

	cur_l2q_info_index = (cur_l2q_info_index + 1) % MAX_SF_BUFFER_SIZE;

	mutex_unlock(&cur_l2q_info_lock);
}

int fpsgo_get_now_logic_head(unsigned long long sf_buffer_id,
	int *pid, unsigned long long *logic_head_ts, unsigned int *is_logic_head_alive,
	unsigned long long *now_ts)
{
	int ret = 0, i = 0;
	unsigned long long now_ktime_ns = 0;
	bool found = false;

	now_ktime_ns = fpsgo_get_time();

	mutex_lock(&cur_l2q_info_lock);

	if (cur_l2q_info_index == 0)
		i = MAX_SF_BUFFER_SIZE - 1;
	else
		i = cur_l2q_info_index - 1;

	while(i != cur_l2q_info_index) {
		if (cur_l2q_info[i].sf_buf_id == sf_buffer_id) {
			found = true;
			break;
		}
		if (i == 0)
			i = MAX_SF_BUFFER_SIZE - 1;
		else
			i--;
	}
	if (!found)
		goto out;

	if (*pid)
		*pid = cur_l2q_info[i].pid;
	if (logic_head_ts)
		*logic_head_ts = cur_l2q_info[i].logic_head_fixed_ts;
	if (is_logic_head_alive)
		*is_logic_head_alive = cur_l2q_info[i].is_logic_head_alive;
	if (now_ts)
		*now_ts = now_ktime_ns;

	fpsgo_main_trace("[%s] sf_buf_id=%llu, pid=%d, logic_ts=%llu, is_logic_alive=%d, now_ts=%llu",
		__func__, sf_buffer_id, cur_l2q_info[i].pid, cur_l2q_info[i].logic_head_fixed_ts,
		cur_l2q_info[i].is_logic_head_alive, now_ktime_ns);
out:
	mutex_unlock(&cur_l2q_info_lock);
	return ret;
}


static void fpsgo_com_delete_policy_cmd(struct fpsgo_com_policy_cmd *iter)
{
	unsigned long long min_ts = ULLONG_MAX;
	struct fpsgo_com_policy_cmd *tmp_iter = NULL, *min_iter = NULL;
	struct rb_node *rbn = NULL;

	if (iter) {
		if (iter->bypass_non_SF_by_pid == BY_PID_DEFAULT_VAL &&
			iter->control_api_mask_by_pid == BY_PID_DEFAULT_VAL &&
			iter->dep_loading_thr_by_pid == BY_PID_DEFAULT_VAL &&
			iter->cam_bypass_window_ms_by_pid == BY_PID_DEFAULT_VAL) {
			min_iter = iter;
			goto delete;
		} else
			return;
	}

	if (RB_EMPTY_ROOT(&fpsgo_com_policy_cmd_tree))
		return;

	rbn = rb_first(&fpsgo_com_policy_cmd_tree);
	while (rbn) {
		tmp_iter = rb_entry(rbn, struct fpsgo_com_policy_cmd, rb_node);
		if (tmp_iter->ts < min_ts) {
			min_ts = tmp_iter->ts;
			min_iter = tmp_iter;
		}
		rbn = rb_next(rbn);
	}

	if (!min_iter)
		return;

delete:
	rb_erase(&min_iter->rb_node, &fpsgo_com_policy_cmd_tree);
	kfree(min_iter);
	total_fpsgo_com_policy_cmd_num--;
}

static struct fpsgo_com_policy_cmd *fpsgo_com_get_policy_cmd(int tgid,
	unsigned long long ts, int create)
{
	struct rb_node **p = &fpsgo_com_policy_cmd_tree.rb_node;
	struct rb_node *parent = NULL;
	struct fpsgo_com_policy_cmd *iter = NULL;

	while (*p) {
		parent = *p;
		iter = rb_entry(parent, struct fpsgo_com_policy_cmd, rb_node);

		if (tgid < iter->tgid)
			p = &(*p)->rb_left;
		else if (tgid > iter->tgid)
			p = &(*p)->rb_right;
		else
			return iter;
	}

	if (!create)
		return NULL;

	iter = kzalloc(sizeof(struct fpsgo_com_policy_cmd), GFP_KERNEL);
	if (!iter)
		return NULL;

	iter->tgid = tgid;
	iter->bypass_non_SF_by_pid = BY_PID_DEFAULT_VAL;
	iter->control_api_mask_by_pid = BY_PID_DEFAULT_VAL;
	iter->dep_loading_thr_by_pid = BY_PID_DEFAULT_VAL;
	iter->cam_bypass_window_ms_by_pid = BY_PID_DEFAULT_VAL;
	iter->ts = ts;

	rb_link_node(&iter->rb_node, parent, p);
	rb_insert_color(&iter->rb_node, &fpsgo_com_policy_cmd_tree);
	total_fpsgo_com_policy_cmd_num++;

	if (total_fpsgo_com_policy_cmd_num > FPSGO_MAX_TREE_SIZE)
		fpsgo_com_delete_policy_cmd(NULL);

	return iter;
}

static void fpsgo_com_set_policy_cmd(int cmd, int value, int tgid,
	unsigned long long ts, int op)
{
	struct fpsgo_com_policy_cmd *iter = NULL;

	iter = fpsgo_com_get_policy_cmd(tgid, ts, op);
	if (iter) {
		if (cmd == 0)
			iter->bypass_non_SF_by_pid = value;
		else if (cmd == 1)
			iter->control_api_mask_by_pid = value;
		else if (cmd == 2)
			iter->dep_loading_thr_by_pid = value;
		else if (cmd == 3)
			iter->cam_bypass_window_ms_by_pid = value;

		if (!op)
			fpsgo_com_delete_policy_cmd(iter);
	}
}

static int fpsgo_com_check_frame_type(int tgid, int queue_SF, int api, int hwui)
{
	int local_bypass_non_SF = -1;
	int local_control_api_mask = 0;
	struct fpsgo_com_policy_cmd *iter = NULL;

	mutex_lock(&fpsgo_com_policy_cmd_lock);
	iter = fpsgo_com_get_policy_cmd(tgid, 0, 0);
	if (iter) {
		if (iter->bypass_non_SF_by_pid != BY_PID_DEFAULT_VAL)
			local_bypass_non_SF = iter->bypass_non_SF_by_pid;
		if (iter->control_api_mask_by_pid != BY_PID_DEFAULT_VAL)
			local_control_api_mask = iter->control_api_mask_by_pid;
	}
	mutex_unlock(&fpsgo_com_policy_cmd_lock);

	if (hwui == RENDER_INFO_HWUI_NONE && (local_control_api_mask & (1 << api)) &&
		(local_bypass_non_SF == 0 || (local_bypass_non_SF == 1 && queue_SF)))
		return NON_VSYNC_ALIGNED_TYPE;
	return BY_PASS_TYPE;
}

static void fpsgo_com_check_bypass_closed_loop(struct render_info *iter)
{
	int local_loading = 0;
	int loading_thr;
	int window_ms;
	struct fpsgo_com_policy_cmd *policy_iter = NULL;

	mutex_lock(&fpsgo_com_policy_cmd_lock);
	policy_iter = fpsgo_com_get_policy_cmd(iter->tgid, 0, 0);
	loading_thr = policy_iter ? policy_iter->dep_loading_thr_by_pid : -1;
	window_ms = policy_iter ? policy_iter->cam_bypass_window_ms_by_pid : -1;
	mutex_unlock(&fpsgo_com_policy_cmd_lock);

	if (loading_thr < 0 || window_ms < 0)
		return;

	if (iter->running_time > 0 && iter->Q2Q_time > 0) {
		iter->sum_cpu_time_us += iter->running_time >> 10;
		iter->sum_q2q_time_us += iter->Q2Q_time >> 10;
		if (!iter->sum_reset_ts)
			iter->sum_reset_ts = iter->t_enqueue_end;

		if (iter->t_enqueue_end - iter->sum_reset_ts >= window_ms * NSEC_PER_MSEC) {
			if (iter->sum_cpu_time_us > 0 && iter->sum_q2q_time_us > 0)
				local_loading = (int)div64_u64(iter->sum_cpu_time_us * 100, iter->sum_q2q_time_us);
			iter->bypass_closed_loop = local_loading < loading_thr;
			iter->sum_cpu_time_us = 0;
			iter->sum_q2q_time_us = 0;
			iter->sum_reset_ts = iter->t_enqueue_end;
		}
	}

	fpsgo_systrace_c_fbt(iter->pid, iter->buffer_id,
		iter->bypass_closed_loop, "bypass_closed_loop");
	fpsgo_main_trace("[comp][%d][0x%llx] run:%llu(%llu) q2q:%llu(%llu) thr:%d(%d) bypass:%d ts:%llu",
		iter->pid, iter->buffer_id,
		iter->running_time, iter->sum_cpu_time_us, iter->Q2Q_time, iter->sum_q2q_time_us,
		loading_thr, window_ms, iter->bypass_closed_loop, iter->sum_reset_ts);
}

int fpsgo_ctrl2comp_get_receive_fw_info_enable(void)
{
	// define in perfctl.h
	// QUEUE_BEG      = 0
	// QUEUE_END      = 1
	// DEQUEUE_BEG    = 2
	// DEQUEUE_END    = 3
	// ACQUIRE        = 4
	// BUFFER_QUOTA   = 5
	// VSYNC          = 6
	// VSYNC_PERIOD   = 7
	// PRODUCER_INFO  = 8
	return receive_fw_info_enable;
}

int fpsgo_ctrl2comp_wait_receive_fw_info_enable(int tgid, int *ret)
{
	struct wait_enable_info *iter;

	*ret = 0;

	mutex_lock(&wait_enable_lock);
	iter = fpsgo_search_and_add_wait_enable_info(current->pid, 1);
	if (!iter) {
		mutex_unlock(&wait_enable_lock);
		*ret = -ENOMEM;
		FPSGO_LOGE("[comp] no memory to wait for tgid:%d pid:%d\n", tgid, current->pid);
		goto out;
	}

	fpsgo_main_trace("[comp] from userspace request tgid:%d pid:%d enable:%d cond:%d\n",
		tgid, current->pid, receive_fw_info_enable, iter->wait_cond);
	mutex_unlock(&wait_enable_lock);

	/*
	 * wait_event_interruptible
	 *   --> __wait_event_interruptible
	 *     --> ___wait_event
	 *       --> prepare_to_wait_event (hold wq_head->lock)
	 *       --> ___wait_is_interruptible
	 *       --> finish_wait
	 */
	wait_event_interruptible(iter->wait_q, iter->wait_cond);

	fpsgo_main_trace("[comp] wakeup tgid:%d pid:%d enable:%d cond:%d\n",
		tgid, current->pid, receive_fw_info_enable, iter->wait_cond);

	mutex_lock(&wait_enable_lock);
	iter->wait_cond = 0;
	mutex_unlock(&wait_enable_lock);

out:
	return receive_fw_info_enable;
}

static int fpsgo_com_update_buffer_info(int pid, int api, int queue_SF,
	unsigned long long buffer_id)
{
	struct render_info *f_render;

	f_render = fpsgo_search_and_add_render_info(pid, buffer_id, 0);
	if (!f_render)
		return -ENOMEM;

	fpsgo_thread_lock(&f_render->thr_mlock);
	f_render->queue_SF = queue_SF;
	f_render->api = api;
	if (!f_render->p_blc && !fpsgo_base2fbt_node_init(f_render))
		f_render->producer_info_ready = 1;
	fpsgo_thread_unlock(&f_render->thr_mlock);

	return 0;
}

void fpsgo_ctrl2comp_producer_info(int ipc_tgid, int pid, int api,
	int queue_SF, unsigned long long buffer_id)
{
	int i, num = 0;
	int *pid_arr = NULL;
	unsigned long long *bufID_arr = NULL;

	fpsgo_render_tree_lock(__func__);
	if (fpsgo_com_update_buffer_info(pid, api, queue_SF, buffer_id)) {
		pid_arr = kcalloc(FPSGO_MAX_RENDER_INFO_SIZE, sizeof(int), GFP_KERNEL);
		bufID_arr = kcalloc(FPSGO_MAX_RENDER_INFO_SIZE, sizeof(unsigned long long), GFP_KERNEL);
		if (!pid_arr || !bufID_arr)
			goto free;
		num = fpsgo_get_render_info_by_tgid(ipc_tgid, pid_arr, bufID_arr,
			FPSGO_MAX_RENDER_INFO_SIZE);
		for (i = 0; i < num; i++) {
			if (bufID_arr[i] != buffer_id)
				continue;
			fpsgo_com_update_buffer_info(pid_arr[i], api, queue_SF, bufID_arr[i]);
		}
free:
		kfree(pid_arr);
		kfree(bufID_arr);
	}
	fpsgo_render_tree_unlock(__func__);
}

void fpsgo_ctrl2comp_enqueue_start(int pid,
	unsigned long long enqueue_start_time,
	unsigned long long buffer_id)
{
	struct render_info *f_render;

	fpsgo_render_tree_lock(__func__);

	f_render = fpsgo_search_and_add_render_info(pid, buffer_id, 1);
	if (!f_render) {
		fpsgo_render_tree_unlock(__func__);
		return;
	}

	fpsgo_thread_lock(&f_render->thr_mlock);

	if (!f_render->producer_info_ready)
		goto exit;

	f_render->hwui = fpsgo_search_and_add_hwui_info(f_render->pid, 0) ?
			RENDER_INFO_HWUI_TYPE : RENDER_INFO_HWUI_NONE;
	f_render->frame_type = fpsgo_com_check_frame_type(f_render->tgid,
		f_render->queue_SF, f_render->api, f_render->hwui);
	f_render->target_render_flag =
		fpsgo_search_and_add_fps_control_info(1, f_render->pid, f_render->buffer_id, 0)
			|| (f_render->frame_type == NON_VSYNC_ALIGNED_TYPE &&
				fpsgo_search_and_add_fps_control_info(0, f_render->tgid, 0, 0));
	f_render->t_enqueue_start = enqueue_start_time;

	if (f_render->target_render_flag)
		fpsgo_com_notify_fpsgo_is_boost(1);
	else {
		fpsgo_systrace_c_fbt(pid, f_render->buffer_id,
			f_render->queue_SF, "bypass_sf");
		fpsgo_systrace_c_fbt(pid, f_render->buffer_id,
			f_render->api, "bypass_api");
		fpsgo_systrace_c_fbt(pid, f_render->buffer_id,
			f_render->hwui, "bypass_hwui");
	}

exit:
	fpsgo_thread_unlock(&f_render->thr_mlock);
	fpsgo_render_tree_unlock(__func__);

	fpsgo_com_prepare_to_do_recycle();

	if (!jank_detection_is_ready &&
		fpsgo2jank_detection_register_callback_fp) {
		fpsgo2jank_detection_register_callback_fp(fpsgo_com_receive_jank_detection);
		jank_detection_is_ready = 1;
		FPSGO_LOGE("fpsgo2jank_detection_register_callback_fp finish\n");
	}
}

void fpsgo_ctrl2comp_enqueue_end(int pid,
	unsigned long long enqueue_end_time,
	unsigned long long buffer_id,
	unsigned long long sf_buf_id)
{
	unsigned long cb_mask = 0;
	unsigned long long raw_runtime = 0;
	unsigned long long running_time = 0;
	unsigned long long enq_running_time = 0;
	unsigned long long pprev_enqueue_end = 0, prev_enqueue_end = 0;
	struct render_info *f_render = NULL;
	struct render_frame_info *info = NULL;

	fpsgo_render_tree_lock(__func__);

	f_render = fpsgo_search_and_add_render_info(pid, buffer_id, 0);
	if (!f_render) {
		fpsgo_render_tree_unlock(__func__);
		return;
	}

	fpsgo_thread_lock(&f_render->thr_mlock);

	if (!f_render->producer_info_ready)
		goto exit;

	pprev_enqueue_end = f_render->prev_t_enqueue_end;
	prev_enqueue_end = f_render->t_enqueue_end;
	if (f_render->t_enqueue_end)
		f_render->Q2Q_time = enqueue_end_time - f_render->t_enqueue_end;
	f_render->prev_t_enqueue_end = f_render->t_enqueue_end;
	f_render->t_enqueue_end = enqueue_end_time;
	f_render->enqueue_length = enqueue_end_time - f_render->t_enqueue_start;
	f_render->enqueue_length_real = f_render->enqueue_length;

	if (f_render->target_render_flag) {
		fpsgo_comp2fstb_prepare_calculate_target_fps(pid, f_render->buffer_id,
			enqueue_end_time);

		if (is_xgf_calculate_dep_enable(f_render->tgid, pid, f_render->buffer_id))
			fpsgo_other2xgf_calculate_dep(pid, f_render->buffer_id,
				&raw_runtime, &running_time, &enq_running_time,
				0, f_render->t_enqueue_end,
				f_render->t_dequeue_start, f_render->t_dequeue_end,
				f_render->t_enqueue_start, f_render->t_enqueue_end, 0);
		f_render->enqueue_length_real = f_render->enqueue_length > enq_running_time ?
						f_render->enqueue_length - enq_running_time : 0;
		fpsgo_systrace_c_fbt_debug(pid, f_render->buffer_id,
			f_render->enqueue_length_real, "enq_length_real");
		f_render->raw_runtime = raw_runtime;
		if (running_time != 0)
			f_render->running_time = running_time;
		fpsgo_com_check_bypass_closed_loop(f_render);

		fpsgo_check_jank_detection_info_status();

		fpsgo_com_get_l2q_time(pid, f_render->buffer_id, f_render->tgid,
			enqueue_end_time, prev_enqueue_end, pprev_enqueue_end,
			f_render->t_dequeue_start, sf_buf_id, f_render);

		fpsgo_comp2fbt_frame_start(f_render,
				enqueue_end_time);

		fpsgo_comp2fstb_queue_time_update(pid,
			f_render->buffer_id,
			enqueue_end_time,
			f_render->hwui);
		fpsgo_comp2fstb_notify_info(pid, f_render->buffer_id,
			f_render->Q2Q_time, f_render->enqueue_length, f_render->dequeue_length);
		fpsgo_comp2minitop_queue_update(enqueue_end_time);

		fpsgo_systrace_c_fbt_debug(-300, 0, f_render->enqueue_length,
			"%d_%d-enqueue_length", pid, f_render->frame_type);
	} else {
		fpsgo_other2xgf_calculate_dep(pid, f_render->buffer_id,
			&raw_runtime, &running_time, &enq_running_time,
			0, f_render->t_enqueue_end,
			f_render->t_dequeue_start, f_render->t_dequeue_end,
			f_render->t_enqueue_start, f_render->t_enqueue_end, 1);
		fpsgo_stop_boost_by_render(f_render);
		fbt_set_render_last_cb(f_render, enqueue_end_time);
		fpsgo_comp2fstb_queue_time_update(pid,
			f_render->buffer_id,
			enqueue_end_time,
			f_render->hwui);
	}

	info = kzalloc(sizeof(struct render_frame_info), GFP_KERNEL);
	if (info) {
		info->q2q_time = f_render->Q2Q_time;
		info->blc = f_render->boost_info.last_blc;
	}

exit:
	fpsgo_thread_unlock(&f_render->thr_mlock);
	fpsgo_render_tree_unlock(__func__);

	if (info) {
		cb_mask = 1 << GET_FPSGO_Q2Q_TIME | 1 << GET_FPSGO_PERF_IDX;
		fpsgo_notify_frame_info_callback(pid, cb_mask, buffer_id, info);
		kfree(info);
	}

}

void fpsgo_ctrl2comp_dequeue_start(int pid,
	unsigned long long dequeue_start_time,
	unsigned long long buffer_id)
{
	struct render_info *f_render;

	fpsgo_render_tree_lock(__func__);

	f_render = fpsgo_search_and_add_render_info(pid, buffer_id, 0);
	if (!f_render) {
		fpsgo_render_tree_unlock(__func__);
		return;
	}

	fpsgo_thread_lock(&f_render->thr_mlock);

	if (!f_render->producer_info_ready)
		goto exit;

	f_render->t_dequeue_start = dequeue_start_time;

exit:
	fpsgo_thread_unlock(&f_render->thr_mlock);
	fpsgo_render_tree_unlock(__func__);
}

void fpsgo_ctrl2comp_dequeue_end(int pid,
	unsigned long long dequeue_end_time,
	unsigned long long buffer_id,
	unsigned long long sf_buf_id)
{
	struct render_info *f_render;
	unsigned long long pprev_enqueue_end = 0, prev_enqueue_end = 0,
		dequeue_start_ts = 0;
	int l2q_enable_pid_final = 0;

	fpsgo_render_tree_lock(__func__);

	f_render = fpsgo_search_and_add_render_info(pid, buffer_id, 0);
	if (!f_render) {
		fpsgo_render_tree_unlock(__func__);
		return;
	}

	fpsgo_thread_lock(&f_render->thr_mlock);

	if (!f_render->producer_info_ready)
		goto exit;

	pprev_enqueue_end = f_render->prev_t_enqueue_end;
	prev_enqueue_end = f_render->t_enqueue_end;
	dequeue_start_ts = f_render->t_dequeue_start;

	f_render->t_dequeue_end = dequeue_end_time;
	f_render->dequeue_length = dequeue_end_time - f_render->t_dequeue_start;

	if (f_render->target_render_flag) {
		l2q_enable_pid_final = f_render->attr.l2q_enable_by_pid;
		if (fpsgo_touch_latency_ko_ready && l2q_enable_pid_final) {
			f_render->is_logic_valid = fpsgo_comp2fstb_get_logic_head(pid, f_render->buffer_id,
				f_render->tgid, dequeue_end_time, prev_enqueue_end, pprev_enqueue_end,
				dequeue_start_ts, &f_render->logic_head_ts, &f_render->has_logic_head);
			fpsgo_comp_set_logic_head(f_render, sf_buf_id,
				f_render->logic_head_ts, f_render->has_logic_head);
		}
		fpsgo_comp2fbt_deq_end(f_render, dequeue_end_time);
		fpsgo_systrace_c_fbt_debug(-300, 0, f_render->dequeue_length,
			"%d_%d-dequeue_length", pid, f_render->frame_type);
	}

exit:
	fpsgo_thread_unlock(&f_render->thr_mlock);
	fpsgo_render_tree_unlock(__func__);
}

void fpsgo_ctrl2comp_acquire(int p_pid, int c_pid, int c_tid,
	int api, unsigned long long buffer_id, unsigned long long ts)
{
	struct acquire_info *iter = NULL;

	fpsgo_render_tree_lock(__func__);

	if (api == WINDOW_DISCONNECT)
		fpsgo_delete_acquire_info(0, c_tid, buffer_id);
	else {
		iter = fpsgo_add_acquire_info(p_pid, c_pid, c_tid,
			api, buffer_id, ts);
		if (iter)
			iter->ts = ts;
	}

	fpsgo_render_tree_unlock(__func__);
}

void fpsgo_ctrl2comp_buffer_count(int pid, int buffer_count,
	unsigned long long buffer_id)
{
	struct render_info *iter;

	fpsgo_render_tree_lock(__func__);
	iter = fpsgo_search_and_add_render_info(pid, buffer_id, 0);
	if (iter)
		iter->buffer_count = buffer_count;
	fpsgo_render_tree_unlock(__func__);
}

static void fpsgo_user_boost(int render_tid, unsigned long long buffer_id,
	unsigned long long tcpu, unsigned long long ts, int skip)
{
	struct render_info *iter = NULL;

	fpsgo_render_tree_lock(__func__);
	iter = fpsgo_search_and_add_render_info(render_tid, buffer_id, 0);
	if (!iter)
		goto out;

	fpsgo_thread_lock(&iter->thr_mlock);
	if (!iter->p_blc)
		fpsgo_base2fbt_node_init(iter);

	iter->raw_runtime = tcpu;
	iter->running_time = tcpu;
	iter->Q2Q_time = ts - iter->t_enqueue_end;

	iter->t_enqueue_end = ts;
	if (iter->t_enqueue_start && ts > iter->t_enqueue_start)
		iter->enqueue_length = ts - iter->t_enqueue_start;
	else
		iter->enqueue_length = 0;
	iter->enqueue_length_real = iter->enqueue_length;
	if (iter->t_dequeue_start && iter->t_dequeue_end > iter->t_dequeue_start)
		iter->dequeue_length = iter->t_dequeue_end - iter->t_dequeue_start;
	else
		iter->dequeue_length = 0;

	if (iter->t_enqueue_end && !skip) {
		fpsgo_comp2fbt_frame_start(iter, ts);
	}
	fpsgo_thread_unlock(&iter->thr_mlock);

out:
	fpsgo_render_tree_unlock(__func__);
	fpsgo_com_notify_fpsgo_is_boost(1);
}

static void fpsgo_user_deboost(int render_tid, unsigned long long buffer_id)
{
	struct render_info *iter = NULL;

	fpsgo_render_tree_lock(__func__);
	iter = fpsgo_search_and_add_render_info(render_tid, buffer_id, 0);
	if (!iter)
		goto out;
	fpsgo_thread_lock(&iter->thr_mlock);
	fpsgo_stop_boost_by_render(iter);
	fpsgo_thread_unlock(&iter->thr_mlock);

out:
	fpsgo_render_tree_unlock(__func__);
}

void fpsgo_other2comp_control_resume(int render_tid, unsigned long long buffer_id)
{
	unsigned long long ts = fpsgo_get_time();

	mutex_lock(&user_hint_lock);
	fpsgo_user_boost(render_tid, buffer_id, 0, ts, 1);
	fpsgo_systrace_c_fbt(render_tid, buffer_id, 1, "user_control_resume");
	mutex_unlock(&user_hint_lock);
}
EXPORT_SYMBOL(fpsgo_other2comp_control_resume);

void fpsgo_other2comp_control_pause(int render_tid, unsigned long long buffer_id)
{
	mutex_lock(&user_hint_lock);
	fpsgo_user_deboost(render_tid, buffer_id);
	fpsgo_systrace_c_fbt(render_tid, buffer_id, 1, "user_control_pause");
	mutex_unlock(&user_hint_lock);
}
EXPORT_SYMBOL(fpsgo_other2comp_control_pause);

void fpsgo_other2comp_user_close(int tgid, int render_tid, unsigned long long buffer_id)
{
	struct fpsgo_boost_attr attr_iter;

	fpsgo_user_deboost(render_tid, buffer_id);

	fpsgo_render_tree_lock(__func__);
	fpsgo_delete_render_info(fpsgo_search_and_add_render_info(render_tid, buffer_id, 0));
	fpsgo_render_tree_unlock(__func__);

	memset(&attr_iter, BY_PID_DEFAULT_VAL, sizeof(struct fpsgo_boost_attr));
	set_fpsgo_attr(1, render_tid, 0, &attr_iter);

	fpsgo_other2fstb_set_target(1, render_tid, 0, 0, 0, 0, buffer_id);

	fpsgo_comp2fstb_delete_render_info(render_tid, buffer_id);
	fpsgo_comp2xgf_delete_render_info(render_tid, buffer_id);

	fpsgo_systrace_c_fbt(render_tid, buffer_id, 1, "user_close");
}
EXPORT_SYMBOL(fpsgo_other2comp_user_close);

/*
 * Create necessary data structure of FPSGO when user want to use self-defined frame, target, critical tasks
 * @tgid: process id
 * @render_tid: one key of FPSGO data structure
 * @buffer_id: one key of FPSGO data structure
 * @dep_arr: task id array of critical tasks, NULL: use fpsgo_other2xgf_set_critical_tasks to set later
 * @dep_num: the number of critical tasks, 0: use fpsgo_other2xgf_set_critical_tasks to set later
 * @target_time: user self-defined target time, 0: use fpsgo_other2fstb_set_target to set later
 */
int fpsgo_other2comp_user_create(int tgid, int render_tid, unsigned long long buffer_id,
	int *dep_arr, int dep_num, unsigned long long target_time)
{
	int i;
	int ret = 0;
	unsigned long local_master_type = 0;
	struct task_info *tmp_dep_arr = NULL;
	struct render_info *iter = NULL;

	tmp_dep_arr = kcalloc(dep_num, sizeof(struct task_info), GFP_KERNEL);
	if (!tmp_dep_arr && dep_num > 0)
		return -ENOMEM;
	for (i = 0; i < dep_num; i++)
		tmp_dep_arr[i].pid = dep_arr[i];

	ret = fpsgo_other2xgf_set_critical_tasks(render_tid, buffer_id, tmp_dep_arr, dep_num, 1);
	kfree(tmp_dep_arr);

	fpsgo_other2fstb_set_target(1, render_tid, 1, 0, 0, target_time, buffer_id);
	fpsgo_comp2fstb_queue_time_update(render_tid, buffer_id, fpsgo_get_time(), RENDER_INFO_HWUI_NONE);

	fpsgo_render_tree_lock(__func__);
	iter = fpsgo_search_and_add_render_info(render_tid, buffer_id, 1);
	if (iter) {
		fpsgo_thread_lock(&iter->thr_mlock);
		iter->tgid = tgid;
		iter->buffer_id = buffer_id;
		iter->api = NATIVE_WINDOW_API_EGL;
		iter->frame_type = USER_FRAME_TYPE;
		set_bit(USER_TYPE, &local_master_type);
		iter->master_type = local_master_type;
		fpsgo_thread_unlock(&iter->thr_mlock);
	}
	fpsgo_render_tree_unlock(__func__);

	fpsgo_systrace_c_fbt(render_tid, buffer_id, ret, "user_create");
	fpsgo_com_prepare_to_do_recycle();
	return ret;
}
EXPORT_SYMBOL(fpsgo_other2comp_user_create);

int fpsgo_other2comp_report_workload(int tgid, int render_tid, unsigned long long buffer_id,
	unsigned long long tcpu, unsigned long long ts)
{
	unsigned long long local_tcpu = tcpu;
	unsigned long long local_ts = ts;
	unsigned long long garbage = 0;

	mutex_lock(&user_hint_lock);

	xgf_trace("[user][xgf][%d][0x%llx] | local_tcpu:%llu local_ts:%llu",
		render_tid, buffer_id, local_tcpu, local_ts);

	// frame start: use cpu_time and q_end_time to boost
	fpsgo_user_boost(render_tid, buffer_id, local_tcpu, local_ts, 0);

	fpsgo_comp2fstb_queue_time_update(render_tid, buffer_id, local_ts, RENDER_INFO_HWUI_NONE);
	fpsgo_other2xgf_calculate_dep(render_tid, buffer_id,
			&garbage, &garbage, &garbage, 0, local_ts,
			0, 0, 0, 0, 1);

	fpsgo_systrace_c_fbt(render_tid, buffer_id, local_tcpu, "user_report_workload");
	mutex_unlock(&user_hint_lock);

	return 0;
}
EXPORT_SYMBOL(fpsgo_other2comp_report_workload);

int fpsgo_other2comp_set_timestamp(int tgid, int render_tid, unsigned long long buffer_id,
	int flag, unsigned long long ts)
{
	int ret = 0;
	struct render_info *iter = NULL;

	fpsgo_render_tree_lock(__func__);
	iter = fpsgo_search_and_add_render_info(render_tid, buffer_id, 0);
	if (!iter) {
		ret = -1;
		goto out;
	}

	fpsgo_thread_lock(&iter->thr_mlock);
	switch (flag) {
	case FPSGO_DEQUEUE_START:
		iter->t_dequeue_start = ts;
		break;
	case FPSGO_DEQUEUE_END:
		iter->t_dequeue_end = ts;
		break;
	case FPSGO_ENQUEUE_START:
		iter->t_enqueue_start = ts;
		break;
	case FPSGO_ENQUEUE_END:
		iter->t_enqueue_end = ts;
		break;
	case FPSGO_BUFFER_QUOTA:
		iter->buffer_quota_ts = ts;
		break;
	default:
		ret = -1;
		break;
	}
	fpsgo_thread_unlock(&iter->thr_mlock);

out:
	fpsgo_render_tree_unlock(__func__);

	return ret;
}
EXPORT_SYMBOL(fpsgo_other2comp_set_timestamp);

/*
 * General API for notify FPSGO control
 * @mode: 0 by process control, 1 by render control
 * @pid: tgid with mode is 0, render pid with mode is 1
 * @set_ctrl: boost is 1, deboost is 0
 * @buffer_id: 0 with mode is 0, render buffer id with mode is 1
 */
int switch_fpsgo_control(int mode, int pid, int set_ctrl, unsigned long long buffer_id)
{
	fpsgo_render_tree_lock(__func__);
	set_ctrl ? fpsgo_search_and_add_fps_control_info(mode, pid, buffer_id, 1) :
				fpsgo_delete_fps_control_info(mode, pid, buffer_id);
	fpsgo_render_tree_unlock(__func__);

	return 0;
}
EXPORT_SYMBOL(switch_fpsgo_control);

/*
 * General API to set FPSGO FBT algorithm
 * @mode: 0 by process control, 1 by render control
 * @pid: tgid with mode is 0, render pid with mode is 1
 * @request_attr: user setting of FPSGO FBT algorithm
 */
int set_fpsgo_attr(int mode, int pid, int set, struct fpsgo_boost_attr *request_boost_attr)
{
	struct fpsgo_attr_by_pid *iter = NULL;

	fpsgo_render_tree_lock(__func__);
	if (set) {
		iter = mode ? fpsgo_find_attr_by_tid(pid, 1) : fpsgo_find_attr_by_pid(pid, 1);
		if (iter && request_boost_attr)
			memcpy(&iter->attr, request_boost_attr, sizeof(struct fpsgo_boost_attr));
	} else
		mode ? delete_attr_by_tid(pid) : delete_attr_by_pid(pid);
	fpsgo_render_tree_unlock(__func__);

	return 0;
}
EXPORT_SYMBOL(set_fpsgo_attr);

/*
 * General API to notify which specific task or all tasks of which specific process
 *   not let FPSGO boost
 * @mode: 0 by specific process, 1 by specific task
 * @id: tgid with mode is 0, pid with mode is 1
 */
int fpsgo_other2comp_set_no_boost_info(int mode, int id, int set)
{
	fpsgo_render_tree_lock(__func__);
	set ? fpsgo_get_no_boost_info(mode, id, 1) : fpsgo_delete_no_boost_info(mode, id);
	fpsgo_render_tree_unlock(__func__);

	return 0;
}
EXPORT_SYMBOL(fpsgo_other2comp_set_no_boost_info);

/*
 * General API to get FPSGO render information from Android Framework
 * @mode: 0 get queueBuffer related information, 1 get acquireBuffer related information
 * @max_num: the number of data structure malloc by user to query information
 * @num: the actual number of data structure which get from FPSGO
 * @arr: memory pointer malloc by user to query information
 */
int fpsgo_other2comp_get_render_fw_info(int mode, int max_num, int *num, struct render_fw_info *arr)
{
	int index = 0;
	struct render_info *render_iter;
	struct acquire_info *acquire_iter;
	struct hlist_node *h;
	HLIST_HEAD(info_list);

	if (!arr || !num || max_num <= 0)
		return -EINVAL;

	// mode = 0 get render_info, mode = 1 get acquire_info
	fpsgo_render_tree_lock(__func__);
	if (mode == 0) {
		fpsgo_get_all_render_info(&info_list);
		hlist_for_each_entry_safe(render_iter, h, &info_list, render_list_node) {
			hlist_del(&render_iter->render_list_node);
			if (index >= max_num || index < 0)
				continue;
			arr[index].consumer_tgid = -1;
			arr[index].consumer_pid = -1;
			arr[index].producer_tgid = render_iter->tgid;
			arr[index].producer_pid = render_iter->pid;
			arr[index].api = render_iter->api;
			arr[index].hwui = render_iter->hwui;
			arr[index].queue_SF = render_iter->queue_SF;
			arr[index].buffer_count = render_iter->buffer_count;
			arr[index].frame_type = render_iter->frame_type;
			arr[index].boosting = !!render_iter->boost_info.last_blc;
			arr[index].buffer_id = render_iter->buffer_id;
			index++;
		}
	} else if (mode == 1) {
		fpsgo_get_all_acquire_info(&info_list);
		hlist_for_each_entry_safe(acquire_iter, h, &info_list, list_node) {
			hlist_del(&acquire_iter->list_node);
			if (index >= max_num || index < 0)
				continue;
			arr[index].consumer_tgid = acquire_iter->c_pid;
			arr[index].consumer_pid = acquire_iter->c_tid;
			arr[index].producer_tgid = acquire_iter->p_pid;
			arr[index].producer_pid = -1;
			arr[index].api = acquire_iter->api;
			arr[index].hwui = -1;
			arr[index].queue_SF = 0;
			arr[index].buffer_count = -1;
			arr[index].frame_type = -1;
			arr[index].boosting = -1;
			arr[index].buffer_id = acquire_iter->buffer_id;
			index++;
		}
	}
	*num = index;
	fpsgo_render_tree_unlock(__func__);

	return 0;
}
EXPORT_SYMBOL(fpsgo_other2comp_get_render_fw_info);

static void fpsgo_com_notify_to_do_recycle(struct work_struct *work)
{
	int ret1, ret2, ret3;

	ret1 = fpsgo_check_render_info_status();
	ret2 = fpsgo_comp2fstb_do_recycle();
	ret3 = fpsgo_comp2xgf_do_recycle();

	mutex_lock(&wait_enable_lock);
	fpsgo_check_wait_enable_info_status();
	mutex_unlock(&wait_enable_lock);

	mutex_lock(&recycle_lock);

	if (ret1 && ret2 && ret3) {
		recycle_idle_cnt++;
		if (recycle_idle_cnt >= FPSGO_MAX_RECYCLE_IDLE_CNT) {
			recycle_active = 0;
			goto out;
		}
	}

	hrtimer_start(&recycle_hrt, ktime_set(0, NSEC_PER_SEC), HRTIMER_MODE_REL);

out:
	mutex_unlock(&recycle_lock);
}

int notify_fpsgo_touch_latency_ko_ready(void)
{
	fpsgo_touch_latency_ko_ready = 1;
	return 0;
}
EXPORT_SYMBOL(notify_fpsgo_touch_latency_ko_ready);

int fpsgo_ktf2comp_test_check_BQ_type(int *a_p_pid_arr,  int *a_c_pid_arr,
	int *a_c_tid_arr, int *a_api_arr, unsigned long long *a_bufID_arr, int a_num,
	int *r_tgid_arr, int *r_pid_arr, unsigned long long *r_bufID_arr, int *r_api_arr,
	int *r_frame_type_arr, int *r_hwui_arr, int *r_tfps_arr, int *r_qfps_arr,
	int *final_bq_type_arr, int r_num)
{
	return 1;
}
EXPORT_SYMBOL(fpsgo_ktf2comp_test_check_BQ_type);

int fpsgo_ktf2comp_test_queue_dequeue(int pid, unsigned long long bufID, int frame_num,
	unsigned long enq_length_us, unsigned long deq_length_us, unsigned long q2q_us)
{
	int i;
	unsigned long long ts;
	struct fpsgo_attr_by_pid *iter = NULL;

	fpsgo_render_tree_lock(__func__);
	iter = fpsgo_find_attr_by_pid(pid, 0);
	if (iter) {
		iter->attr.rescue_enable_by_pid = 1;
		iter->attr.rescue_second_enable_by_pid = 1;
		iter->attr.filter_frame_enable_by_pid = 1;
		iter->attr.separate_aa_by_pid = 1;
		iter->attr.boost_affinity_by_pid = 2;
		iter->attr.gcc_enable_by_pid = 2;
	}
	fpsgo_render_tree_unlock(__func__);

	for (i = 0; i < frame_num; i++) {
		ts = fpsgo_get_time();
		fpsgo_ctrl2comp_enqueue_start(pid, ts, bufID);
		usleep_range(enq_length_us, enq_length_us * 2);
		ts = fpsgo_get_time();
		fpsgo_ctrl2comp_enqueue_end(pid, ts, bufID, 0);
		usleep_range(1000, 2000);
		ts = fpsgo_get_time();
		fpsgo_ctrl2comp_dequeue_start(pid, ts, bufID);
		usleep_range(deq_length_us, deq_length_us * 2);
		ts = fpsgo_get_time();
		fpsgo_ctrl2comp_dequeue_end(pid, ts, bufID, 0);
		usleep_range(q2q_us, q2q_us * 2);
	}

	return 0;
}
EXPORT_SYMBOL(fpsgo_ktf2comp_test_queue_dequeue);

#define FPSGO_COM_SYSFS_READ(name, show, variable); \
static ssize_t name##_show(struct kobject *kobj, \
		struct kobj_attribute *attr, \
		char *buf) \
{ \
	if ((show)) \
		return scnprintf(buf, PAGE_SIZE, "%d\n", (variable)); \
	else \
		return 0; \
}

#define FPSGO_COM_SYSFS_WRITE_VALUE(name, variable, min, max); \
static ssize_t name##_store(struct kobject *kobj, \
		struct kobj_attribute *attr, \
		const char *buf, size_t count) \
{ \
	char *acBuffer = NULL; \
	int arg; \
\
	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL); \
	if (!acBuffer) \
		goto out; \
\
	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) { \
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) { \
			if (kstrtoint(acBuffer, 0, &arg) == 0) { \
				if (arg >= (min) && arg <= (max)) { \
					(variable) = arg; \
				} \
			} \
		} \
	} \
\
out: \
	kfree(acBuffer); \
	return count; \
}

#define FPSGO_COM_SYSFS_WRITE_POLICY_CMD(name, cmd, min, max); \
static ssize_t name##_store(struct kobject *kobj, \
		struct kobj_attribute *attr, \
		const char *buf, size_t count) \
{ \
	char *acBuffer = NULL; \
	int tgid; \
	int arg; \
	unsigned long long ts = fpsgo_get_time(); \
\
	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL); \
	if (!acBuffer) \
		goto out; \
\
	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) { \
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) { \
			if (sscanf(acBuffer, "%d %d", &tgid, &arg) == 2) { \
				mutex_lock(&fpsgo_com_policy_cmd_lock); \
				if (arg >= (min) && arg <= (max)) \
					fpsgo_com_set_policy_cmd(cmd, arg, tgid, ts, 1); \
				else \
					fpsgo_com_set_policy_cmd(cmd, BY_PID_DEFAULT_VAL, \
						tgid, ts, 0); \
				mutex_unlock(&fpsgo_com_policy_cmd_lock); \
			} \
		} \
	} \
\
out: \
	kfree(acBuffer); \
	return count; \
}

FPSGO_COM_SYSFS_READ(receive_fw_info_enable, 1, receive_fw_info_enable);
static ssize_t receive_fw_info_enable_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	char *acBuffer = NULL;
	int arg;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if (count > 0 && count < FPSGO_SYSFS_MAX_BUFF_SIZE) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg))
				goto out;

			if (arg >= 0 && arg <= INT_MAX - 1 && receive_fw_info_enable != arg) {
				receive_fw_info_enable = arg;

				mutex_lock(&wait_enable_lock);
				fpsgo_wake_up_all_wait_enable_info();
				mutex_unlock(&wait_enable_lock);
			}
		}
	}

out:
	kfree(acBuffer);
	return count;
}
static KOBJ_ATTR_RW(receive_fw_info_enable);

FPSGO_COM_SYSFS_WRITE_POLICY_CMD(bypass_non_SF_by_pid, 0, 0, 1);
static KOBJ_ATTR_WO(bypass_non_SF_by_pid);

FPSGO_COM_SYSFS_WRITE_POLICY_CMD(control_api_mask_by_pid, 1, 0, 31);
static KOBJ_ATTR_WO(control_api_mask_by_pid);

FPSGO_COM_SYSFS_WRITE_POLICY_CMD(dep_loading_thr_by_pid, 2, 0, 100);
static KOBJ_ATTR_WO(dep_loading_thr_by_pid);

FPSGO_COM_SYSFS_WRITE_POLICY_CMD(cam_bypass_window_ms_by_pid, 3, 0, 60000);
static KOBJ_ATTR_WO(cam_bypass_window_ms_by_pid);

static ssize_t fpsgo_com_policy_cmd_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char *temp = NULL;
	int i = 1;
	int pos = 0;
	int length = 0;
	struct fpsgo_com_policy_cmd *iter;
	struct rb_root *rbr;
	struct rb_node *rbn;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	mutex_lock(&fpsgo_com_policy_cmd_lock);

	rbr = &fpsgo_com_policy_cmd_tree;
	for (rbn = rb_first(rbr); rbn; rbn = rb_next(rbn)) {
		iter = rb_entry(rbn, struct fpsgo_com_policy_cmd, rb_node);
		length = scnprintf(temp + pos,
			FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"%dth\ttgid:%d\tbypass_non_SF:%d\tcontrol_api_mask:%d\tdep_loading_thr_by_pid:%d(%d)\tts:%llu\n",
			i,
			iter->tgid,
			iter->bypass_non_SF_by_pid,
			iter->control_api_mask_by_pid,
			iter->dep_loading_thr_by_pid,
			iter->cam_bypass_window_ms_by_pid,
			iter->ts);
		pos += length;
		i++;
	}

	mutex_unlock(&fpsgo_com_policy_cmd_lock);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static KOBJ_ATTR_RO(fpsgo_com_policy_cmd);

static ssize_t fpsgo_control_info_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char *temp = NULL;
	int i;
	int pos = 0;
	int length = 0;
	int count = 0;
	struct fps_control_info *arr = NULL;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	arr = kcalloc(FPSGO_MAX_TREE_SIZE, sizeof(struct fps_control_info), GFP_KERNEL);
	if (!temp || !arr)
		goto out;

	count = fpsgo_get_all_fps_control_info(0, FPSGO_MAX_TREE_SIZE, arr);
	if (count < 0)
		goto out;
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
		"\nby process num:%d\n", count);
	pos += length;
	for (i = 0; i < count; i++) {
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"\tprocess:%d ts:%llu\n", arr[i].pid, arr[i].ts);
		pos += length;
	}

	count = fpsgo_get_all_fps_control_info(1, FPSGO_MAX_TREE_SIZE, arr);
	if (count < 0)
		goto out;
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
		"\nby render num:%d\n", count);
	pos += length;
	for (i = 0; i < count; i++) {
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"\trender:%d buffer_id:0x%llx ts:%llu\n",
			arr[i].pid, arr[i].buffer_id, arr[i].ts);
		pos += length;
	}

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(arr);
	kfree(temp);
	return length;
}
static KOBJ_ATTR_RO(fpsgo_control_info);

static ssize_t fpsgo_control_by_process_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char *acBuffer = NULL;
	int tgid = 0, value = 0;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (sscanf(acBuffer, "%d %d", &tgid, &value) == 2)
				switch_fpsgo_control(0, tgid, !!value, 0);
		}
	}

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_WO(fpsgo_control_by_process);

static ssize_t fpsgo_control_by_render_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char *acBuffer = NULL;
	int pid = 0, value = 0;
	unsigned long long buffer_id = 0;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (sscanf(acBuffer, "%d %llu %d", &pid, &buffer_id, &value) == 3)
				switch_fpsgo_control(1, pid, !!value, buffer_id);
		}
	}

out:
	kfree(acBuffer);
	return count;
}
static KOBJ_ATTR_WO(fpsgo_control_by_render);

static ssize_t fpsgo_no_boost_info_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char *temp = NULL;
	int i;
	int pos = 0;
	int length = 0;
	int count = 0;
	struct no_boost_info *arr = NULL;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	arr = kcalloc(FPSGO_MAX_NO_BOOST_INFO_SIZE, sizeof(struct no_boost_info), GFP_KERNEL);
	if (!temp || !arr)
		goto out;

	count = fpsgo_get_all_no_boost_info(FPSGO_MAX_NO_BOOST_INFO_SIZE, arr);
	if (count <= 0)
		goto out;
	for (i = 0; i < count; i++) {
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"mode:%d id:%d\n", arr[i].mode, arr[i].specific_id);
		pos += length;
	}

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(arr);
	kfree(temp);
	return length;
}
static KOBJ_ATTR_RO(fpsgo_no_boost_info);

static ssize_t fpsgo_no_boost_by_process_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char *acBuffer = NULL;
	int arg = 0;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				fpsgo_other2comp_set_no_boost_info(0, arg > 0 ? arg : -arg, arg > 0);
		}
	}

out:
	kfree(acBuffer);
	return count;
}
static KOBJ_ATTR_WO(fpsgo_no_boost_by_process);

static ssize_t fpsgo_no_boost_by_thread_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char *acBuffer = NULL;
	int arg = 0;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				fpsgo_other2comp_set_no_boost_info(1, arg > 0 ? arg : -arg, arg > 0);
		}
	}

out:
	kfree(acBuffer);
	return count;
}
static KOBJ_ATTR_WO(fpsgo_no_boost_by_thread);

static ssize_t is_fpsgo_boosting_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char *temp = NULL;
	int posi = 0;
	int length = 0;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	mutex_lock(&fpsgo_boost_lock);
	length = scnprintf(temp + posi,
		FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
		"fpsgo is boosting = %d\n", fpsgo_is_boosting);
	posi += length;
	mutex_unlock(&fpsgo_boost_lock);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static KOBJ_ATTR_RO(is_fpsgo_boosting);

void fpsgo_ktf2comp_fuzz_test_node(char *input_data, int op, int cmd)
{
	struct kobject *kobj = NULL;
	struct kobj_attribute *attr = NULL;
	char *buf = NULL;

	kobj = kzalloc(sizeof(struct kobject), GFP_KERNEL);
	if (!kobj)
		goto out;

	attr = kzalloc(sizeof(struct kobj_attribute), GFP_KERNEL);
	if (!attr)
		goto out;

	buf = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!buf)
		goto out;

	if (input_data && op)
		scnprintf(buf, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", input_data);

	switch (cmd) {
	case BYPASS_NON_SF_GLOBAL:
		break;
	case BYPASS_NON_SF_BY_PID:
		if (op)
			fpsgo_ktf_test_write_node(kobj, attr, buf, bypass_non_SF_by_pid_store);
		break;
	case CONTROL_API_MASK_GLOBAL:
		break;
	case CONTROL_API_MASK_BY_PID:
		if (op)
			fpsgo_ktf_test_write_node(kobj, attr, buf, control_api_mask_by_pid_store);
		break;
	case CONTROL_HWUI_GLOBAL:
		break;
	case CONTROL_HWUI_BY_PID:
		break;
	case FPSGO_CONTROL_GLOBAL:
		break;
	case FPSGO_CONTROL_BY_PID:
		if (op)
			fpsgo_ktf_test_write_node(kobj, attr, buf, fpsgo_control_by_process_store);
		break;
	case FPS_ALIGN_MARGIN:
		break;
	case FPSGO_COM_POLICY_CMD:
		if (!op)
			fpsgo_ktf_test_read_node(kobj, attr, buf, fpsgo_com_policy_cmd_show);
		break;
	case IS_FPSGO_BOOSTING:
		if (!op)
			fpsgo_ktf_test_read_node(kobj, attr, buf, is_fpsgo_boosting_show);
		break;
	default:
		break;
	}

out:
	kfree(buf);
	kfree(attr);
	kfree(kobj);
}
EXPORT_SYMBOL(fpsgo_ktf2comp_fuzz_test_node);

void init_fpsgo_frame_info_cb_list(void)
{
	int i;
	struct render_frame_info_cb *iter = NULL;

	for (i = 0; i < FPSGO_MAX_CALLBACK_NUM; i++) {
		iter = &fpsgo_frame_info_cb_list[i];
		iter->mask = 0;
		iter->func_cb = NULL;
		memset(&iter->info_iter, 0, sizeof(struct render_frame_info));
	}
}

void __exit fpsgo_composer_exit(void)
{
	hrtimer_cancel(&recycle_hrt);
	destroy_workqueue(composer_wq);

	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_fpsgo_control_info);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_fpsgo_control_by_process);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_fpsgo_control_by_render);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_fpsgo_no_boost_info);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_fpsgo_no_boost_by_process);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_fpsgo_no_boost_by_thread);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_bypass_non_SF_by_pid);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_control_api_mask_by_pid);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_dep_loading_thr_by_pid);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_fpsgo_com_policy_cmd);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_is_fpsgo_boosting);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_cam_bypass_window_ms_by_pid);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_receive_fw_info_enable);

	fpsgo_sysfs_remove_dir(&comp_kobj);
}

int __init fpsgo_composer_init(void)
{
	fpsgo_com_policy_cmd_tree = RB_ROOT;

	composer_wq = alloc_ordered_workqueue("composer_wq", WQ_MEM_RECLAIM | WQ_HIGHPRI);
	hrtimer_init(&recycle_hrt, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	recycle_hrt.function = &prepare_do_recycle;

	hrtimer_start(&recycle_hrt, ktime_set(0, NSEC_PER_SEC), HRTIMER_MODE_REL);

	init_fpsgo_frame_info_cb_list();

	cur_l2q_info_index = 0;

	if (!fpsgo_sysfs_create_dir(NULL, "composer", &comp_kobj)) {
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_fpsgo_control_info);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_fpsgo_control_by_process);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_fpsgo_control_by_render);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_fpsgo_no_boost_info);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_fpsgo_no_boost_by_process);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_fpsgo_no_boost_by_thread);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_bypass_non_SF_by_pid);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_control_api_mask_by_pid);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_dep_loading_thr_by_pid);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_fpsgo_com_policy_cmd);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_is_fpsgo_boosting);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_cam_bypass_window_ms_by_pid);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_receive_fw_info_enable);
	}

	return 0;
}

