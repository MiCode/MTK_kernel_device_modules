// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/min_heap.h>
#include <linux/slab.h>

#include "mdw.h"
#include "mdw_cmn.h"
#include "mdw_cmd.h"
#include "mdw_ch.h"
#include "mdw_rv.h"
#include "mdw_rv_tag.h"

#define MDW_CH_HLIST_HASH_ORDER (6)
#define MDW_CH_PREDICT_CMD_NUM (16)
#define MDW_CH_IPTIME_TOLERANCE_TH (10)
#define MDW_CH_PERIOD_TOLERANCE_TH (10)
#define MDW_CH_AVAILABLE_PERIOD_CNT (2)
#define MDW_CH_DEFAULT_DETIME_MS (2000)
#define MDW_CH_IN_THRESHOLD_PERCENT(x, y, thres) \
	(abs((int64_t)x - (int64_t)y) < ((int64_t)y * thres / 100) ? true : false)

/* appendix buffer definition */
struct mdw_ch_rv_sc_msg {
	uint32_t ip_time;
} __packed;

struct mdw_ch_rv_cmd_msg {
	uint32_t uid;
	uint32_t num_subcmds;
} __packed;

/* cmd history table definition */
struct mdw_ch_tbl {
	/* history basic struct */
	uint64_t uid;
	struct hlist_node ch_hash_node; //to mdw_ch_mgr
	uint64_t period_cnt;
	uint32_t num_subcmds;

	/* history cmd time info */
	uint64_t h_end_ts;
	uint64_t h_start_ts;
	uint64_t h_period;
	uint64_t h_exec_time;

	/* history subcmd einfo */
	struct mdw_subcmd_exec_info *h_sc_einfo;

	struct mdw_fpriv *mpriv;
};

DEFINE_MIN_HEAP(uint64_t, mdw_min_heap);

struct mdw_ch_mgr {
	DECLARE_HASHTABLE(ch_tbl_hash, MDW_CH_HLIST_HASH_ORDER);

	uint64_t idle_time_ts;
	uint64_t predict_cmd_ts[MDW_CH_PREDICT_CMD_NUM];

	struct mdw_min_heap heap;
	struct mutex mtx;   // cmd history talbe mtx
	struct mutex h_mtx; // heap mtx

	struct mdw_device *mdev;
};

struct mdw_ch_mgr *g_ch_mgr = NULL;

//--------------------------------------------
// heap functions
static void mdw_ch_swap_uint64(void *lhs, void *rhs, void __always_unused *args)
{
	uint64_t temp = *(uint64_t *)lhs;

	*(uint64_t *)lhs = *(uint64_t *)rhs;
	*(uint64_t *)rhs = temp;
}

static bool mdw_ch_less_than(const void *lhs, const void *rhs, void __always_unused *args)
{
	return *(uint64_t *)lhs < *(uint64_t *)rhs;
}

static const struct min_heap_callbacks mdw_ch_min_heap_funcs = {
	.less = mdw_ch_less_than,
	.swp = mdw_ch_swap_uint64,
};

//--------------------------------------------
static struct mdw_ch_tbl *mdw_ch_find_tbl(struct mdw_cmd *c)
{
	struct mdw_ch_tbl *tmp = NULL;

	hash_for_each_possible(g_ch_mgr->ch_tbl_hash, tmp, ch_hash_node, c->uid) {
		if (tmp->mpriv == c->mpriv && tmp->uid == c->uid)
			goto out;
	}

	tmp = NULL;

out:
	return tmp;
}

int mdw_ch_pollcmd_timeout(uint32_t *flag, uint32_t poll_interval_us, uint32_t poll_timeout_us,
	struct mdw_cmd *c)
{
	struct mdw_ch_tbl *ch_tbl = NULL;
	uint32_t poll_acc_us = 0;
	int ret = -ETIME;

	mutex_lock(&g_ch_mgr->mtx);
	ch_tbl = mdw_ch_find_tbl(c);
	mutex_unlock(&g_ch_mgr->mtx);
	if (ch_tbl == NULL) {
		mdw_drv_warn("find ch table failed(0x%llx)\n", c->uid);
		goto out;
	}

	if (g_mdw_poll_timeout)
		poll_timeout_us = g_mdw_poll_timeout;

	if (ch_tbl->h_exec_time > poll_timeout_us) {
		mdw_cmd_debug("skip poll cmd\n");
		goto out;
	}

	/* check flag */
	if (flag == NULL || poll_interval_us == 0 || poll_timeout_us == 0) {
		mdw_drv_err("invalid args (%pK/%u/%u)\n", flag, poll_interval_us, poll_timeout_us);
		return -EINVAL;
	}

	if (ch_tbl->h_exec_time)
		usleep_range(MDW_POLLTIME_SLEEP_TH(ch_tbl->h_exec_time),
			MDW_POLLTIME_SLEEP_TH(ch_tbl->h_exec_time)+10);

	mdw_cmd_debug("poll_interval_us = %u, poll_timeout_us = %u\n", poll_interval_us, poll_timeout_us);

	mdw_trace_begin("apumdw:poll_cmd");
	while (poll_acc_us < poll_timeout_us) {
		if (*flag) {
			ret = 0;
			mdw_cmd_debug("poll cmd done, total time = %u\n", poll_acc_us);
			break;
		}

		udelay(poll_interval_us);
		poll_acc_us += poll_interval_us;
	}
	mdw_trace_end();
out:
	return ret;
}

//--------------------------------------------
static uint64_t mdw_ch_period_avg(uint64_t old_period, uint64_t new_period)
{
	uint64_t period = 0;

	period = (old_period + new_period) / 2;

	return period;
}

static void mdw_ch_reset_heap(void)
{
	int i = 0;

	mdw_flw_debug("\n");

	mutex_lock(&g_ch_mgr->h_mtx);
	/* reset min heap */
	g_ch_mgr->heap.nr = 0;
	for (i = 0; i < MDW_NUM_PREDICT_CMD; i++)
		g_ch_mgr->predict_cmd_ts[i] = 0;
	mutex_unlock(&g_ch_mgr->h_mtx);
}

static void mdw_ch_min_heap_sanity_check(void)
{
	mdw_flw_debug("heap_num(%u)\n", g_ch_mgr->heap.nr);

	if (g_ch_mgr->heap.nr >= MDW_CH_PREDICT_CMD_NUM)
		mdw_ch_reset_heap();
}

static bool mdw_ch_is_perf_mode(struct mdw_cmd *c)
{
	/* check cmd mode */
	if (c->power_plcy == MDW_POWERPOLICY_PERFORMANCE ||
		c->power_plcy == MDW_POWERPOLICY_SUSTAINABLE) {
		mdw_flw_debug("cmd is performace policy\n");
		/* reset history */
		mdw_ch_reset_heap();
		return true;
	}
	return false;
}

static void mdw_ch_handle_period(struct mdw_ch_tbl *ch_tbl, struct mdw_cmd *c)
{
	uint64_t interval = 0;

	mdw_flw_debug("c_ts(0x%llx/0x%llx) h_ts(0x%llx/0x%llx) h_period(%llu) period_cnt(%llu)\n",
		c->start_ts, c->end_ts,
		ch_tbl->h_start_ts, ch_tbl->h_end_ts,
		ch_tbl->h_period, ch_tbl->period_cnt);

	/* no history cmd */
	if (!ch_tbl->h_start_ts)
		return;

	/* cmd overlap case */
	if (c->start_ts <= ch_tbl->h_end_ts) {
		ch_tbl->h_period = 0;
		ch_tbl->period_cnt = 0;
		return;
	}

	interval = (c->start_ts - ch_tbl->h_start_ts);

	/* initial h_period */
	if (!ch_tbl->h_period) {
		mdw_flw_debug("init period_ts(%llu) interval(%llu)\n",
				ch_tbl->h_period, interval);
		ch_tbl->h_period = interval;
		ch_tbl->period_cnt++;
		return;
	}

	/* check interval time and cal h_period */
	if (MDW_CH_IN_THRESHOLD_PERCENT(ch_tbl->h_period, interval, MDW_CH_PERIOD_TOLERANCE_TH)) {
		mdw_flw_debug("period h_period_ts(%llu) interval(%llu)\n",
				ch_tbl->h_period, interval);
		ch_tbl->h_period = mdw_ch_period_avg(ch_tbl->h_period, interval);

		if (ch_tbl->period_cnt < MDW_NUM_HISTORY)
			ch_tbl->period_cnt++;
	} else {
		mdw_flw_debug("no period h_period_ts(%llu) interval(%llu)\n",
				 ch_tbl->h_period, interval);
		ch_tbl->h_period = interval;
		ch_tbl->period_cnt = 1;
	}
}

static uint64_t mdw_ch_get_next_cmd_in_ts(struct mdw_cmd *c)
{
	uint64_t predict_start_ts = 0;
	int heap_nr = 0;
	uint32_t i = 0;

	/* check predict cmd exist */
	if (!g_ch_mgr->heap.nr) {
		mdw_flw_debug("no enough history\n");
		return 0;
	}

	/* check heap predict start_ts with cmd */
	heap_nr = g_ch_mgr->heap.nr;
	for (i = 0; i < heap_nr; i++) {
		predict_start_ts = g_ch_mgr->predict_cmd_ts[0];
		if (c->end_ts >= predict_start_ts) {
			mdw_flw_debug("predict cmd start_ts(%llu) is invalid\n", predict_start_ts);
			min_heap_pop(&g_ch_mgr->heap, &mdw_ch_min_heap_funcs, NULL);
			predict_start_ts = 0;
			continue;
		} else {
			mdw_flw_debug("predict cmd start_ts(%llu) is valid\n", predict_start_ts);
			break;
		}
	}

	return predict_start_ts;
}

static void mdw_ch_handle_iptime(struct mdw_ch_tbl *ch_tbl, struct mdw_cmd *c)
{
	struct mdw_subcmd_exec_info *sc_einfo = NULL;
	uint32_t h_iptime = 0, c_iptime = 0, i = 0;
	uint64_t sc_sync_info = 0;
	int64_t vsid = 0;

	/* get sc execution information */
	sc_einfo = &c->einfos->sc;
	if (!sc_einfo)
		return;

	/* get current and history ip time */
	for (i = 0; i < c->num_subcmds; i++) {
		h_iptime = ch_tbl->h_sc_einfo[i].ip_time;
		c_iptime = sc_einfo[i].ip_time;

		if (sc_einfo[i].was_preempted || sc_einfo[i].ret) {
			mdw_flw_debug("sc was preempted or failed, skip this iptime\n");
			mdw_subcmd_trace(c, i, vsid, ch_tbl->h_sc_einfo[i].ip_time, sc_sync_info, MDW_CMD_SCHED);
			continue;
		}

		if (MDW_CH_IN_THRESHOLD_PERCENT(c_iptime, h_iptime, MDW_CH_IPTIME_TOLERANCE_TH)) {
			/* under 10% difference, get max */
			ch_tbl->h_sc_einfo[i].ip_time = max(c_iptime, h_iptime);
		} else {
			/* over 10% difference, apply current ip time */
			ch_tbl->h_sc_einfo[i].ip_time = c_iptime;
		}
		mdw_subcmd_trace(c, i, vsid, ch_tbl->h_sc_einfo[i].ip_time, sc_sync_info ,MDW_CMD_SCHED);
	}
}

static bool mdw_ch_handle_fast_power_onoff(struct mdw_ch_tbl *ch_tbl, struct mdw_cmd *c)
{
	uint64_t predict_start_ts = 0, predict_idle = 0;
	struct mdw_device *mdev = g_ch_mgr->mdev;
	bool need_dtime_handle = false;

	/* update period */
	mdw_ch_handle_period(ch_tbl, c);

	/* calculate predict cmd_start_ts and push to min heap */
	if (ch_tbl->period_cnt >= MDW_CH_AVAILABLE_PERIOD_CNT) {
		predict_start_ts = c->start_ts + ch_tbl->h_period;
		mdw_ch_min_heap_sanity_check();
		min_heap_push(&g_ch_mgr->heap, &predict_start_ts, &mdw_ch_min_heap_funcs, NULL);
		mdw_flw_debug("predict_start_ts(%llu) nr(%d)",
				 g_ch_mgr->predict_cmd_ts[0], g_ch_mgr->heap.nr);
	}

	/* check support power fast power on off */
	if (mdev->support_power_fast_on_off == false) {
		mdw_flw_debug("no support power fast on off\n");
		goto out;
	}

	/* get next cmd in ts */
	mutex_lock(&g_ch_mgr->h_mtx);
	predict_start_ts = mdw_ch_get_next_cmd_in_ts(c);
	mutex_unlock(&g_ch_mgr->h_mtx);
	if (!predict_start_ts) {
		mdw_flw_debug("no valid predict cmd in heap\n");
		need_dtime_handle = true;
		goto out;
	}

	/* check predict idle time */
	predict_idle = (predict_start_ts - c->end_ts) / 1000;

	/* apply power on/off by conditions */
	if (predict_idle > mdev->power_gain_time_us &&
		mdev->support_power_fast_on_off == true &&
		!atomic_read(&mdev->cmd_running) &&
		(c->power_dtime > MDW_CH_DEFAULT_DETIME_MS || !c->is_dtime_set)) {
		mdw_flw_debug("apply fast power off, time(%llu/%u), activated cmd(%u) dtime(%u/0x%llx)\n",
			predict_idle, mdev->power_gain_time_us, atomic_read(&mdev->cmd_running),
			c->power_dtime, c->is_dtime_set);

		g_mdw_pwroff_cnt++;
		mdw_trace_begin("apumdw:power_off|pwroff_cnt(%u)", g_mdw_pwroff_cnt);
		/* suggest fast power off */
		if (mdw_rv_power_onoff(mdev, MDW_APU_POWER_OFF))
			mdw_drv_err("fastpower off failed\n");
		mdw_trace_end();
	} else {
		mdw_flw_debug("not apply fast power off, time(%llu/%u), activated cmd(%u) dtime(%u/0x%llx)\n",
			predict_idle, mdev->power_gain_time_us, atomic_read(&mdev->cmd_running),
			c->power_dtime, c->is_dtime_set);
		need_dtime_handle = true;
	}
out:
	return need_dtime_handle;
}

static void mdw_ch_cmd_delete_tbl(struct mdw_ch_tbl *tbl)
{
	struct mdw_fpriv *mpriv = tbl->mpriv;

	mdw_flw_debug("delete ch tbl(0x%llx/%u)\n", tbl->uid, tbl->num_subcmds);
	hash_del(&tbl->ch_hash_node);
	devm_kfree(mpriv->dev, tbl->h_sc_einfo);
	devm_kfree(mpriv->dev, tbl);
}

static bool mdw_ch_exec_time_check(uint64_t h_exec_time, uint64_t exec_time)
{
	uint64_t exec_time_th = 0;

	exec_time_th = MDW_EXECTIME_TOLERANCE_TH(h_exec_time);
	if (abs(exec_time - h_exec_time) < exec_time_th)
		return true;

	return false;
}

//--------------------------------------------
int mdw_ch_cmd_create_tbl(struct mdw_cmd *c)
{
	struct mdw_ch_tbl *ch_tbl = NULL;
	int ret = 0;

	mutex_lock(&g_ch_mgr->mtx);

	ch_tbl = mdw_ch_find_tbl(c);
	if (ch_tbl != NULL) {
		if (ch_tbl->num_subcmds == c->num_subcmds) {
			goto out;
		} else {
			mdw_flw_debug("num subcmds not matched(%u/%u), create new tbl\n",
				ch_tbl->num_subcmds, c->num_subcmds);
			mdw_ch_cmd_delete_tbl(ch_tbl);
			ch_tbl = NULL;
		}
	}

	/* alloc cmd history table */
	ch_tbl = devm_kzalloc(c->mpriv->dev, sizeof(*ch_tbl), GFP_KERNEL);
	if (IS_ERR_OR_NULL(ch_tbl)) {
		ret = -ENOMEM;
		goto out;
	}

	/* alloc subcmd history */
	ch_tbl->h_sc_einfo = devm_kzalloc(c->mpriv->dev,
		c->num_subcmds * sizeof(*ch_tbl), GFP_KERNEL);
	if (IS_ERR_OR_NULL(ch_tbl->h_sc_einfo)) {
		ret = -ENOMEM;
		goto free_ch_tbl;
	}

	ch_tbl->uid = c->uid;
	ch_tbl->num_subcmds = c->num_subcmds;
	ch_tbl->mpriv = c->mpriv;

	hash_add(g_ch_mgr->ch_tbl_hash, &ch_tbl->ch_hash_node, ch_tbl->uid);
	mdw_flw_debug("create ch tbl(0x%llx/%u)\n", ch_tbl->uid, ch_tbl->num_subcmds);

	goto out;

free_ch_tbl:
	devm_kfree(c->mpriv->dev, ch_tbl);
out:
	mutex_unlock(&g_ch_mgr->mtx);
	return ret;
}

bool mdw_ch_cmd_exec_update(struct mdw_cmd *c)
{
	struct mdw_ch_tbl *ch_tbl = NULL;
	bool need_dtime_handle = false;

	mdw_drv_debug("\n");

	if (c->cmd_state == MDW_CMD_STATE_ERROR) {
		mdw_flw_debug("cmd execute error, bypass update\n");
		need_dtime_handle = true;
		goto out;
	}

	mutex_lock(&g_ch_mgr->mtx);

	/* get cmd history table */
	ch_tbl = mdw_ch_find_tbl(c);
	if (ch_tbl == NULL) {
		mdw_drv_warn("find ch table failed(0x%llx)\n", c->uid);
		need_dtime_handle = true;
		goto lock_out;
	}

	/* initial or update h_exec_time */
	if (!ch_tbl->h_exec_time ||
		 mdw_ch_exec_time_check(ch_tbl->h_exec_time, c->einfos->c.total_us))
		ch_tbl->h_exec_time = c->einfos->c.total_us;
	else
		ch_tbl->h_exec_time = min(ch_tbl->h_exec_time,  c->einfos->c.total_us);

	mdw_cmd_debug("h_exec_time(%llu)\n", ch_tbl->h_exec_time);

	if (mdw_ch_is_perf_mode(c)) {
		mdw_flw_debug("perf mode cmd, bypass fast power off\n");
		need_dtime_handle = true;
		goto lock_out;
	}

	/* update ip time for rv */
	mdw_ch_handle_iptime(ch_tbl, c);

	/* handle fast power onoff by heap */
	need_dtime_handle = mdw_ch_handle_fast_power_onoff(ch_tbl, c);

	/* record cmd end_ts */
	ch_tbl->h_start_ts = c->start_ts;
	ch_tbl->h_end_ts = c->end_ts;

lock_out:
	mutex_unlock(&g_ch_mgr->mtx);
out:
	return need_dtime_handle;
}

/*
 * @brief create a cmd history table for the session
 */
int mdw_ch_session_create(struct mdw_fpriv *mpriv)
{
	return 0;
}

void mdw_ch_session_delete(struct mdw_fpriv *mpriv)
{
	struct mdw_ch_tbl *ch_tbl = NULL;
	struct hlist_node *tmp = NULL;
	int i = 0;

	mutex_lock(&g_ch_mgr->mtx);
	hash_for_each_safe(g_ch_mgr->ch_tbl_hash, i, tmp, ch_tbl, ch_hash_node) {
		if (ch_tbl->mpriv == mpriv) {
			mdw_flw_debug("delete ch tbl uid(0x%llx)\n", ch_tbl->uid);
			mdw_ch_cmd_delete_tbl(ch_tbl);
		}
	}

	mdw_ch_reset_heap();
	mutex_unlock(&g_ch_mgr->mtx);
}

int mdw_ch_init(struct mdw_device *mdev)
{
	if (g_ch_mgr != NULL || mdev == NULL)
		return -EINVAL;

	g_ch_mgr = devm_kzalloc(mdw_dev->misc_dev->this_device, sizeof(*g_ch_mgr), GFP_KERNEL);
	if (IS_ERR_OR_NULL(g_ch_mgr))
		return -ENOMEM;

	mdw_flw_debug("\n");

	g_ch_mgr->mdev = mdev;
	mutex_init(&g_ch_mgr->mtx);
	mutex_init(&g_ch_mgr->h_mtx);
	hash_init(g_ch_mgr->ch_tbl_hash);

	/* init heap */
	g_ch_mgr->heap.data = g_ch_mgr->predict_cmd_ts;
	g_ch_mgr->heap.nr = 0;
	g_ch_mgr->heap.size = ARRAY_SIZE(g_ch_mgr->predict_cmd_ts);

	return 0;
}

void mdw_ch_deinit(struct mdw_device *mdev)
{
	if (g_ch_mgr == NULL || mdev == NULL)
		return;

	mdw_flw_debug("\n");

	devm_kfree(mdw_dev->misc_dev->this_device, g_ch_mgr);
}
