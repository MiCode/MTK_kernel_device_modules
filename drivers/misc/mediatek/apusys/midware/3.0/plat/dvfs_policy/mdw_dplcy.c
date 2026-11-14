// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/slab.h>
#include <linux/rwsem.h>

#include "mdw.h"
#include "mdw_cmn.h"
#include "mdw_dplcy.h"
#include "mdw_rv.h"

#define MDW_DPLCY_VERSION (1)
#define MDW_DPLCY_HASH_ORDER (3)

#define PERCENT_SHIFT (7)
#define MAX_CON_CMD (3)
#define OPP_MAX_NUM (16)
#define TMA_POINT_NUM (9)
/* inf_time in us */
struct policy_cb {
	uint64_t start_ts_us;
	uint64_t end_ts_us;
	uint64_t cmd_tb_id;
	/* from uP, clear recommand_opp when dvfs_target_time change */
	uint64_t dvfs_target_time;

	uint32_t history_inf_time[MAX_CON_CMD][OPP_MAX_NUM];
	uint32_t history_point_tma[TMA_POINT_NUM];
	uint32_t history_point_percent[TMA_POINT_NUM];

	uint32_t record_total_tma;
	uint32_t record_point_tma[TMA_POINT_NUM];
	uint32_t record_point_inf_us[TMA_POINT_NUM];
	uint32_t record_point_accu_boost[TMA_POINT_NUM];

	/* for debugging */
	int32_t unexpect_code;

	uint8_t history_num;
	uint8_t version;
	uint8_t con_current_cmd;
	int8_t recommand_opp[MAX_CON_CMD];
	int8_t exist_pending_instance;
} __packed;

struct mdw_dplcy_cmd_tb {
	// for same cmd execution racing
	struct mutex cmd_mtx;

	uint64_t dvfs_target_time;
	uint32_t history_tma;
	uint32_t history_point_tma[TMA_POINT_NUM];
	uint32_t history_point_percent[TMA_POINT_NUM];
	uint32_t history_inf_time[MAX_CON_CMD][OPP_MAX_NUM];
	int32_t iter_round;
	uint8_t record_quality;
	uint8_t valid_record_num;
	int8_t recommand_opp[MAX_CON_CMD];

	struct hlist_node hash_node; //to mdw_ch_mgr

	uint32_t num_subcmds;
	uint64_t uid;
	struct mdw_fpriv *mpriv;
};

// Read lock: search
// Write Lock: insert(cmd run)/delete(session delete)
struct mdw_dplcy_mgr {
	// for all_cmd_hash racing
	struct rw_semaphore rw_sem;
	DECLARE_HASHTABLE(all_cmd_hash, MDW_DPLCY_HASH_ORDER);
	struct mdw_device *mdev;
};

struct mdw_dplcy_mgr *g_dplcy_mgr = NULL;

static uint32_t mdw_dplcy_appendix_cb_size(uint32_t num_subcmds)
{
	uint32_t appendix_size = 0;

	if (check_add_overflow(appendix_size, sizeof(struct policy_cb), &appendix_size)) {
		mdw_drv_warn("check appendix cmd overflow(%lu/%lu)\n", sizeof(struct policy_cb),
			     sizeof(struct policy_cb));
		return 0;
	}

	return appendix_size;
}
static void mdw_dplcy_dump_cmd_tb(struct mdw_dplcy_cmd_tb *cmd_tb)
{
	int i;
	mdw_flw_debug("uid(0x%llx) target(%llu) round(%u) num_subcmds(%u) session(%llu)\n",
		      cmd_tb->uid, cmd_tb->dvfs_target_time, cmd_tb->iter_round,
		      cmd_tb->num_subcmds, (uint64_t)cmd_tb->mpriv);
	mdw_flw_debug(
		"history_tma(0x%x) record_quality(%u) valid_record_num(%u) recommand_opp(%d/%d/%d)\n",
		cmd_tb->history_tma, cmd_tb->record_quality, cmd_tb->valid_record_num,
		cmd_tb->recommand_opp[0], cmd_tb->recommand_opp[1], cmd_tb->recommand_opp[2]);

	mdw_flw_debug("history_point_tma(0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x)\n",
		      cmd_tb->history_point_tma[0], cmd_tb->history_point_tma[1],
		      cmd_tb->history_point_tma[2], cmd_tb->history_point_tma[3],
		      cmd_tb->history_point_tma[4], cmd_tb->history_point_tma[5],
		      cmd_tb->history_point_tma[6], cmd_tb->history_point_tma[7],
		      cmd_tb->history_point_tma[8]);
	mdw_flw_debug("history_point_percent(0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x)\n",
		      cmd_tb->history_point_percent[0], cmd_tb->history_point_percent[1],
		      cmd_tb->history_point_percent[2], cmd_tb->history_point_percent[3],
		      cmd_tb->history_point_percent[4], cmd_tb->history_point_percent[5],
		      cmd_tb->history_point_percent[6], cmd_tb->history_point_percent[7],
		      cmd_tb->history_point_percent[8]);
	for (i = 0; i < MAX_CON_CMD; i++)
		mdw_flw_debug(
			"history_inf_time-%d (%u/%u/%u/%u/%u/%u/%u/%u/%u/%u/%u/%u/%u/%u/%u/%u)\n",
			i, cmd_tb->history_inf_time[i][0], cmd_tb->history_inf_time[i][1],
			cmd_tb->history_inf_time[i][2], cmd_tb->history_inf_time[i][3],
			cmd_tb->history_inf_time[i][4], cmd_tb->history_inf_time[i][5],
			cmd_tb->history_inf_time[i][6], cmd_tb->history_inf_time[i][7],
			cmd_tb->history_inf_time[i][8], cmd_tb->history_inf_time[i][9],
			cmd_tb->history_inf_time[i][10], cmd_tb->history_inf_time[i][11],
			cmd_tb->history_inf_time[i][12], cmd_tb->history_inf_time[i][13],
			cmd_tb->history_inf_time[i][14], cmd_tb->history_inf_time[i][15]);
}
static void mdw_dplcy_dump_cb(struct policy_cb *cb)
{
	// TODO change to va_start/va_arg for define ??
	int i;

	mdw_flw_debug(
		"cmd_tb_ic(0x%llx) target(%llu) total(%llu) history_num(%u) start_ts(%llu) end_ts(%llu)\n",
		cb->cmd_tb_id, cb->dvfs_target_time, cb->end_ts_us - cb->start_ts_us,
		cb->history_num, cb->start_ts_us, cb->end_ts_us);
	for (i = 0; i < MAX_CON_CMD; i++)
		mdw_flw_debug(
			"history_inf_time-%d (%u/%u/%u/%u/%u/%u/%u/%u/%u/%u/%u/%u/%u/%u/%u/%u)\n",
			i, cb->history_inf_time[i][0], cb->history_inf_time[i][1],
			cb->history_inf_time[i][2], cb->history_inf_time[i][3],
			cb->history_inf_time[i][4], cb->history_inf_time[i][5],
			cb->history_inf_time[i][6], cb->history_inf_time[i][7],
			cb->history_inf_time[i][8], cb->history_inf_time[i][9],
			cb->history_inf_time[i][10], cb->history_inf_time[i][11],
			cb->history_inf_time[i][12], cb->history_inf_time[i][13],
			cb->history_inf_time[i][14], cb->history_inf_time[i][15]);

	mdw_flw_debug("history_point_tma(0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x)\n",
		      cb->history_point_tma[0], cb->history_point_tma[1], cb->history_point_tma[2],
		      cb->history_point_tma[3], cb->history_point_tma[4], cb->history_point_tma[5],
		      cb->history_point_tma[6], cb->history_point_tma[7], cb->history_point_tma[8]);
	mdw_flw_debug("history_point_percent(0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x)\n",
		      cb->history_point_percent[0], cb->history_point_percent[1],
		      cb->history_point_percent[2], cb->history_point_percent[3],
		      cb->history_point_percent[4], cb->history_point_percent[5],
		      cb->history_point_percent[6], cb->history_point_percent[7],
		      cb->history_point_percent[8]);

	mdw_flw_debug("record_point_tma(0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x)\n",
		      cb->record_point_tma[0], cb->record_point_tma[1], cb->record_point_tma[2],
		      cb->record_point_tma[3], cb->record_point_tma[4], cb->record_point_tma[5],
		      cb->record_point_tma[6], cb->record_point_tma[7], cb->record_point_tma[8]);
	mdw_flw_debug("record_point_inf_us(%u/%u/%u/%u/%u/%u/%u/%u/%u)\n",
		      cb->record_point_inf_us[0], cb->record_point_inf_us[1],
		      cb->record_point_inf_us[2], cb->record_point_inf_us[3],
		      cb->record_point_inf_us[4], cb->record_point_inf_us[5],
		      cb->record_point_inf_us[6], cb->record_point_inf_us[7],
		      cb->record_point_inf_us[8]);
	mdw_flw_debug("record_point_accu_boost(0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x)\n",
		      cb->record_point_accu_boost[0], cb->record_point_accu_boost[1],
		      cb->record_point_accu_boost[2], cb->record_point_accu_boost[3],
		      cb->record_point_accu_boost[4], cb->record_point_accu_boost[5],
		      cb->record_point_accu_boost[6], cb->record_point_accu_boost[7],
		      cb->record_point_accu_boost[8]);

	mdw_flw_debug(
		"record_total_tma(%u) version(%u) con_current_cmd(%u) recommand_opp(%d/%d/%d) exist_pending_instance(%d)\n",
		cb->record_total_tma, cb->version, cb->con_current_cmd, cb->recommand_opp[0],
		cb->recommand_opp[1], cb->recommand_opp[2], cb->exist_pending_instance);
}

static int mdw_dplcy_preprocess(struct policy_cb *cb, struct mdw_dplcy_cmd_tb *cmd_tb)
{
	cb->version = MDW_DPLCY_VERSION;
	cb->dvfs_target_time = cmd_tb->dvfs_target_time;
	memcpy(cb->history_inf_time, cmd_tb->history_inf_time,
	       MAX_CON_CMD * OPP_MAX_NUM * sizeof(uint32_t));
	memcpy(cb->recommand_opp, cmd_tb->recommand_opp, sizeof(cb->recommand_opp));
	if (cmd_tb->valid_record_num > 0) {
		cb->history_num = cmd_tb->valid_record_num;
		memcpy(cb->history_point_tma, cmd_tb->history_point_tma,
		       TMA_POINT_NUM * sizeof(uint32_t));
		memcpy(cb->history_point_percent, cmd_tb->history_point_percent,
		       TMA_POINT_NUM * sizeof(uint32_t));
	}
	cb->record_total_tma = cmd_tb->history_tma;
	mdw_flw_debug("\n");
	mdw_dplcy_dump_cmd_tb(cmd_tb);
	return 0;
}
static inline uint8_t mdw_dplcy_record_rank(struct policy_cb *cb, bool exist_opp_diff)
{
	/* Quality From good to bad record
	* 5. (con_current_cmd == 0 && opp fix)
	* 4. [POLICY_OPP_FINE_TUNE] (con_current_cmd == 0 && opp unstable)
	* 3. [TODO]  (con_current_cmd != 0 && exist_pending_instance == false && opp fix)
	* 2. [TODO]  (con_current_cmd != 0 && exist_pending_instance == false && opp unstable)
	*/
	if (cb->con_current_cmd == 0) {
		if (!exist_opp_diff)
			return 5;
		if (POLICY_OPP_FINE_TUNE)
			return 4;
	} else if (cb->exist_pending_instance == false) {
		if (!exist_opp_diff)
			return 3;
		if (POLICY_OPP_FINE_TUNE)
			return 2;
	}

	return 0;
}
static inline uint32_t mdw_dplcy_cal_close_opp(uint64_t total_us_opp, uint64_t total_us)
{
	uint64_t avg_opp;
	avg_opp = total_us_opp / total_us;
	// avg_opp * total_us -------total_us_opp -- avg_opp+1 * total_us
	if (total_us_opp - avg_opp * total_us > (avg_opp + 1) * total_us - total_us_opp)
		avg_opp = avg_opp + 1;
	return avg_opp;
}
static int mdw_dplcy_postprocess(struct policy_cb *cb, struct mdw_dplcy_cmd_tb *cmd_tb)
{
	uint64_t total_us_opp = 0;
	uint64_t total_us = cb->end_ts_us - cb->start_ts_us;
	uint64_t avg_opp, record_avg_opp[TMA_POINT_NUM], tmp_us;
	uint8_t record_quality = 0, valid_record_num;
	bool record_to_history = true, exist_opp_diff = false;
	int i;

	if (cb->con_current_cmd == 0 || total_us == 0 || cb->record_total_tma == 0)
		return 0;
	mdw_dplcy_dump_cb(cb);
	mdw_dplcy_dump_cmd_tb(cmd_tb);

	if (cb->unexpect_code != 0) {
		mdw_drv_err("dplcy err(%d) session(0x%llx) c-uid(0x%llx/0x%llx)\n",
			      cb->unexpect_code, (uint64_t)cmd_tb->mpriv, cb->cmd_tb_id,
			      cmd_tb->uid);
		mdw_exception("dplcy err\n");
	}

	cb->con_current_cmd =
		(cb->con_current_cmd > MAX_CON_CMD) ? MAX_CON_CMD : cb->con_current_cmd;
	/* minus 1 due to include self cmd */
	cb->con_current_cmd = cb->con_current_cmd - 1;
	for (i = 0; i < TMA_POINT_NUM; i++) {
		total_us_opp += cb->record_point_accu_boost[i];
		if (cb->record_point_inf_us[i] == 0)
			break;
	}
	valid_record_num = i;
	avg_opp = mdw_dplcy_cal_close_opp(total_us_opp, total_us);

	/* update recommand opp. to achieve iterate goal */
	if (cb->dvfs_target_time != cmd_tb->dvfs_target_time) {
		memset(cmd_tb->recommand_opp, -1, sizeof(cmd_tb->recommand_opp));
		cmd_tb->iter_round = 0;
		cmd_tb->dvfs_target_time = cb->dvfs_target_time;
	}
	if (cmd_tb->iter_round != 0) {
		cmd_tb->recommand_opp[cb->con_current_cmd] = avg_opp;
		/* fill out initial value */
		for (i = cb->con_current_cmd; i < MAX_CON_CMD - 1; i++) {
			if (cmd_tb->recommand_opp[i+1] == -1)
				cmd_tb->recommand_opp[i+1] = cmd_tb->recommand_opp[i];
		}
	}
	cmd_tb->iter_round++;

	if (total_us > cmd_tb->history_inf_time[cb->con_current_cmd][avg_opp])
		cmd_tb->history_inf_time[cb->con_current_cmd][avg_opp] = total_us;
	for (i = 0; i < MAX_CON_CMD - 1; i++) {
		/* history_inf_time[0][OPP_MAX_NUM] must < history_inf_time[1][OPP_MAX_NUM] */
		if (cmd_tb->history_inf_time[i][avg_opp] > cmd_tb->history_inf_time[i + 1][avg_opp])
			cmd_tb->history_inf_time[i + 1][avg_opp] =
				cmd_tb->history_inf_time[i][avg_opp];
	}
	mdw_flw_debug("uid(0x%llx) opp(%llu) history_inf_time(%u/%u/%u)\n", cmd_tb->uid, avg_opp,
		      cmd_tb->history_inf_time[0][avg_opp], cmd_tb->history_inf_time[1][avg_opp],
		      cmd_tb->history_inf_time[2][avg_opp]);
	for (i = 0; i < valid_record_num; i++) {
		if (i == 0)
			record_avg_opp[0] = mdw_dplcy_cal_close_opp(cb->record_point_accu_boost[i],
								    cb->record_point_inf_us[0]);
		else
			record_avg_opp[i] = mdw_dplcy_cal_close_opp(
				cb->record_point_accu_boost[i],
				cb->record_point_inf_us[i] - cb->record_point_inf_us[i - 1]);

		if (record_avg_opp[i] != avg_opp) {
			exist_opp_diff = true;
		}
	}

	record_quality = mdw_dplcy_record_rank(cb, exist_opp_diff);
	if (record_quality > cmd_tb->record_quality ||
	    (record_quality == cmd_tb->record_quality &&
	     valid_record_num > cmd_tb->valid_record_num)) {
		for (i = 0; i < valid_record_num; i++) {
			if (record_avg_opp[i] != avg_opp) {
				if (!POLICY_OPP_FINE_TUNE) {
					record_to_history = false;
					break;
				} else {
					/* No data to calculated */
					if (cmd_tb->history_inf_time[0][avg_opp] == 0 ||
					    cmd_tb->history_inf_time[0][record_avg_opp[i]] == 0) {
						record_to_history = false;
						break;
					}
					tmp_us = cb->record_point_inf_us[i] *
						 cmd_tb->history_inf_time[0][avg_opp];
					tmp_us = tmp_us /
						 cmd_tb->history_inf_time[0][record_avg_opp[i]];
					cb->record_point_inf_us[i] = tmp_us;
				}
			}
		}
	} else {
		record_to_history = false;
	}
	mdw_flw_debug("uid(0x%llx) to_history(%d) record_quality(%u/%u) num(%u/%u) diff(%d)\n",
		      cmd_tb->uid, record_to_history, record_quality, cmd_tb->record_quality,
		      valid_record_num, cmd_tb->valid_record_num, exist_opp_diff);
	if (record_to_history) {
		total_us = cb->end_ts_us - cb->start_ts_us;
		for (i = 0; i < TMA_POINT_NUM; i++) {
			if (cb->record_point_inf_us[i] == 0)
				break;
			cmd_tb->history_point_tma[i] = cb->record_point_tma[i];
			cmd_tb->history_point_percent[i] =
				(cb->record_point_inf_us[i] << PERCENT_SHIFT) / total_us;
		}
		cmd_tb->record_quality = record_quality;
		cmd_tb->valid_record_num = valid_record_num;
	}
	cmd_tb->history_tma = cb->record_total_tma;
	return 0;
}

static void mdw_dplcy_clear_cmd_tb(struct mdw_dplcy_cmd_tb *cmd_tb)
{
	mdw_flw_debug("clear dplcy tbl(%pK/0x%llx/0x%llx)\n", cmd_tb, (uint64_t)cmd_tb->mpriv,
		      cmd_tb->uid);
	memset(cmd_tb->recommand_opp, -1, sizeof(cmd_tb->recommand_opp));
	memset(cmd_tb->history_point_tma, 0, sizeof(cmd_tb->history_point_tma));
	memset(cmd_tb->history_point_percent, 0, sizeof(cmd_tb->history_point_percent));
	cmd_tb->num_subcmds = 0;
	cmd_tb->history_tma = 0;
	cmd_tb->record_quality = 0;
	cmd_tb->iter_round = 0;
	cmd_tb->dvfs_target_time = 0;
}

static void mdw_dplcy_delete_cmd_tb(struct mdw_dplcy_cmd_tb *cmd_tb)
{
	struct mdw_fpriv *mpriv = cmd_tb->mpriv;
	mdw_flw_debug("delete dplcy tbl(%pK/0x%llx/0x%llx)\n", cmd_tb, (uint64_t)cmd_tb->mpriv,
		      cmd_tb->uid);
	hash_del(&cmd_tb->hash_node);
	devm_kfree(mpriv->dev, cmd_tb);
}

static struct mdw_dplcy_cmd_tb *mdw_dplcy_create_cmd_tb(struct policy_cb *cb, uint64_t session_id,
							uint64_t cmd_uid, uint32_t num_subcmds)
{
	struct mdw_dplcy_cmd_tb *cmd_tb = NULL;
	struct mdw_fpriv *mpriv = (struct mdw_fpriv *)session_id;

	cmd_tb = devm_kzalloc(mpriv->dev, sizeof(*cmd_tb), GFP_KERNEL);
	if (IS_ERR_OR_NULL(cmd_tb)) {
		mdw_drv_err("create fail dplcy tbl(0x%llx/0x%llx) for cb(%pK)\n", session_id,
			    cmd_uid, cb);
		cmd_tb = NULL;
		goto out;
	}

	mutex_init(&cmd_tb->cmd_mtx);
	cmd_tb->uid = cmd_uid;
	cmd_tb->mpriv = mpriv;
	cmd_tb->num_subcmds = num_subcmds;
	memset(cmd_tb->recommand_opp, -1, sizeof(cmd_tb->recommand_opp));

	mdw_flw_debug("create dplcy tbl(%pK/0x%llx/0x%llx) for cb(%pK)\n", cmd_tb, session_id,
		      cmd_uid, cb);
out:
	return cmd_tb;
}

static struct mdw_dplcy_cmd_tb *mdw_dplcy_find_cmd_tb(struct policy_cb *cb, uint64_t session_id,
						      uint64_t cmd_uid)
{
	struct mdw_dplcy_cmd_tb *tmp = NULL;
	struct mdw_fpriv *mpriv = (struct mdw_fpriv *)session_id;
	hash_for_each_possible (g_dplcy_mgr->all_cmd_hash, tmp, hash_node, cmd_uid) {
		if (tmp->mpriv == mpriv && tmp->uid == cmd_uid)
			goto out;
	}
	tmp = NULL;
out:
	return tmp;
}

static int mdw_dplcy_appendix_cb_process(enum apu_appendix_cb_type type,
	struct apusys_cmd_info *cmd_info, void *va, uint32_t size)
{
	int ret = 0;
	struct policy_cb *cb = (struct policy_cb *)va;
	struct mdw_dplcy_cmd_tb *cmd_tb = NULL;

	if (cmd_info == NULL)
		return -EINVAL;

	/* check argument */
	if (!size || va == NULL || !cmd_info->num_subcmds)
		return -EINVAL;

	/* check size */
	if (size != sizeof(struct policy_cb)) {
		mdw_drv_err("size not matched(%u/%lu)\n", size, sizeof(struct policy_cb));
		return -EINVAL;
	}

	mdw_flw_debug("type(%u) id(0x%llx/0x%llx) appendix(%pK/%u)\n",
		type, cmd_info->session_id, cmd_info->cmd_uid, va, size);

	switch (type) {
	case APU_APPENDIX_CB_CREATE:
		down_read(&g_dplcy_mgr->rw_sem);
		cmd_tb = mdw_dplcy_find_cmd_tb(cb, cmd_info->session_id, cmd_info->cmd_uid);
		up_read(&g_dplcy_mgr->rw_sem);

		if (cmd_tb == NULL) {
			cmd_tb = mdw_dplcy_create_cmd_tb(cb, cmd_info->session_id, cmd_info->cmd_uid,
				cmd_info->num_subcmds);
			if (cmd_tb == NULL) {
				ret = -EINVAL;
				goto out;
			}
			down_write(&g_dplcy_mgr->rw_sem);
			hash_add(g_dplcy_mgr->all_cmd_hash, &cmd_tb->hash_node, cmd_tb->uid);
			up_write(&g_dplcy_mgr->rw_sem);
		}

		if (cmd_tb->num_subcmds != cmd_info->num_subcmds) {
			down_read(&g_dplcy_mgr->rw_sem);
			mutex_lock(&cmd_tb->cmd_mtx);

			mdw_dplcy_clear_cmd_tb(cmd_tb);
			cmd_tb->num_subcmds = cmd_info->num_subcmds;

			mutex_unlock(&cmd_tb->cmd_mtx);
			up_read(&g_dplcy_mgr->rw_sem);
		}

		cb->cmd_tb_id = (uint64_t)cmd_tb;
		break;
	case APU_APPENDIX_CB_PREPROCESS:
		cmd_tb = (struct mdw_dplcy_cmd_tb *)cb->cmd_tb_id;
		if (cmd_tb == NULL || cmd_tb->uid != cmd_info->cmd_uid) {
			mdw_drv_err("dplcy cmd tbl not match(0x%llx/%pK)\n", cmd_info->cmd_uid, cmd_tb);
			ret = -EINVAL;
			goto out;
		}
		down_read(&g_dplcy_mgr->rw_sem);
		mutex_lock(&cmd_tb->cmd_mtx);

		mdw_dplcy_preprocess(cb, cmd_tb);

		mutex_unlock(&cmd_tb->cmd_mtx);
		up_read(&g_dplcy_mgr->rw_sem);
		break;
	case APU_APPENDIX_CB_POSTPROCESS:
		break;
	case APU_APPENDIX_CB_POSTPROCESS_LATE:
		cmd_tb = (struct mdw_dplcy_cmd_tb *)cb->cmd_tb_id;
		if (cmd_tb == NULL || cmd_tb->uid != cmd_info->cmd_uid) {
			mdw_drv_err("dplcy cmd tbl not match(0x%llx/%pK)\n", cmd_info->cmd_uid, cmd_tb);
			ret = -EINVAL;
			goto out;
		}
		down_read(&g_dplcy_mgr->rw_sem);
		mutex_lock(&cmd_tb->cmd_mtx);

		mdw_dplcy_postprocess(cb, cmd_tb);

		mutex_unlock(&cmd_tb->cmd_mtx);
		up_read(&g_dplcy_mgr->rw_sem);
		break;
	case APU_APPENDIX_CB_DELETE:
		break;
	default:
		break;
	};

out:
	return ret;
}

int mdw_dplcy_session_create(struct mdw_fpriv *mpriv)
{
	mdw_flw_debug("\n");
	return 0;
}
void mdw_dplcy_session_delete(struct mdw_fpriv *mpriv)
{
	struct mdw_dplcy_cmd_tb *cmd_tb = NULL;
	struct hlist_node *tmp = NULL;
	int i = 0;

	down_write(&g_dplcy_mgr->rw_sem);
	hash_for_each_safe (g_dplcy_mgr->all_cmd_hash, i, tmp, cmd_tb, hash_node) {
		if (cmd_tb->mpriv == mpriv) {
			mdw_dplcy_delete_cmd_tb(cmd_tb);
		}
	}
	up_write(&g_dplcy_mgr->rw_sem);

	mdw_flw_debug("\n");
}
int mdw_dplcy_init(struct mdw_device *mdev)
{
	int ret = 0;

	ret = apusys_request_cmdbuf_appendix(APU_APPENDIX_CB_OWNER_DVFS_POLICY,
					     mdw_dplcy_appendix_cb_size,
					     mdw_dplcy_appendix_cb_process);
	if (ret)
		mdw_drv_err("request appendix cmdbuf failed(%d)\n", ret);

	g_dplcy_mgr =
		devm_kzalloc(mdw_dev->misc_dev->this_device, sizeof(*g_dplcy_mgr), GFP_KERNEL);
	if (IS_ERR_OR_NULL(g_dplcy_mgr))
		return -ENOMEM;

	mdw_flw_debug("\n");

	g_dplcy_mgr->mdev = mdev;
	hash_init(g_dplcy_mgr->all_cmd_hash);
	init_rwsem(&g_dplcy_mgr->rw_sem);

	return ret;
}

void mdw_dplcy_deinit(struct mdw_device *mdev)
{
	mdw_flw_debug("\n");
	devm_kfree(mdw_dev->misc_dev->this_device, g_dplcy_mgr);
}
