// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <linux/component.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <mtk_drm_ddp_comp.h>

#include "mtk-mml-color.h"
#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"
#include "mtk-mml-dle-adaptor.h"
#include "mtk-mml-pq-core.h"

#include "tile_driver.h"
#include "mtk-mml-tile.h"
#include "tile_mdp_func.h"

#undef pr_fmt
#define pr_fmt(fmt) "[mml_pq_chist]" fmt


#define CHIST_WAIT_TIMEOUT_MS (50)
#define DS_REG_NUM (36)
#define REG_NOT_SUPPORT 0xfff
#define DS_CLARITY_REG_NUM (42)
#define VALID_CONTOUR_HIST_VALUE (0x07FFFFFF)
#define call_hw_op(_comp, op, ...) \
	(_comp->hw_ops->op ? _comp->hw_ops->op(_comp, ##__VA_ARGS__) : 0)


enum mml_chist_reg_index {
	CHIST_00,
	HIST_CFG_00,
	HIST_CFG_01,
	CHIST_CTRL,
	CHIST_INTEN,
	CHIST_INTSTA,
	CHIST_STATUS,
	CHIST_CFG,
	CHIST_INPUT_COUNT,
	CHIST_OUTPUT_COUNT,
	CHIST_INPUT_SIZE,
	CHIST_OUTPUT_OFFSET,
	CHIST_OUTPUT_SIZE,
	CHIST_BLANK_WIDTH,
	/* REGION_PQ_SIZE_PARAMETER_MODE_SEGMENTATION_LENGTH */
	CHIST_REGION_PQ_PARAM,
	CHIST_SHADOW_CTRL,
	CONTOUR_HIST_00,
	CHIST_STATUS_00,
	TDHSP_C_BOOST_MAIN,
	CHIST_REG_MAX_COUNT
};

static const u16 chist_reg_table_mt6985[CHIST_REG_MAX_COUNT] = {
	[CHIST_00] = 0x000,
	[HIST_CFG_00] = 0x064,
	[HIST_CFG_01] = 0x068,
	[CHIST_CTRL] = 0x100,
	[CHIST_INTEN] = 0x104,
	[CHIST_INTSTA] = 0x108,
	[CHIST_STATUS] = 0x10c,
	[CHIST_CFG] = 0x110,
	[CHIST_INPUT_COUNT] = 0x114,
	[CHIST_OUTPUT_COUNT] = 0x11c,
	[CHIST_INPUT_SIZE] = 0x120,
	[CHIST_OUTPUT_OFFSET] = 0x124,
	[CHIST_OUTPUT_SIZE] = 0x128,
	[CHIST_BLANK_WIDTH] = 0x12c,
	[CHIST_REGION_PQ_PARAM] = 0x680,
	[CHIST_SHADOW_CTRL] = 0x724,
	[CONTOUR_HIST_00] = 0x3dc,
	[CHIST_STATUS_00] = 0x644,
	[TDHSP_C_BOOST_MAIN] = 0x0E0
};

struct chist_data {
	u32 tile_width;
	/* u32 min_hfg_width; 9: HFG min + CHIST crop */
	u16 gpr[MML_PIPE_CNT];
	u16 cpr[MML_PIPE_CNT];
	const u16 *reg_table;
	u8 rb_mode;
	bool wrot_pending;	/* WA: enable wrot yuv422/420 pending zero */
};

/*
static const struct chist_data mt6993_mmlt_chist_data = {
	.tile_width = 528,
	.gpr = {CMDQ_GPR_R12, CMDQ_GPR_R14},
	.cpr = {CMDQ_CPR_MML0_PQ0_ADDR, CMDQ_CPR_MML0_PQ1_ADDR},
	.reg_table = chist_reg_table_mt6985,
	.rb_mode = RB_EOF_MODE,
	.wrot_pending = true,
};
*/

static const struct chist_data mt6993_mmlf_chist_data = {
	.tile_width = 3332,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.cpr = {CMDQ_CPR_MML_PQ0_ADDR, CMDQ_CPR_MML_PQ1_ADDR},
	.reg_table = chist_reg_table_mt6985,
	.rb_mode = RB_EOF_MODE,
	.wrot_pending = true,
};

struct mml_comp_chist {
	struct mtk_ddp_comp ddp_comp;
	struct mml_comp comp;
	const struct chist_data *data;
	bool ddp_bound;
	u8 pipe;
	u8 out_idx;
	u16 event_eof;
	u32 jobid;
	struct mml_pq_task *pq_task;
	bool dual;
	bool hist_cmd_done;
	bool clarity_readback;
	bool dc_readback;
	bool dpc;
	struct mml_comp *mmlsys_comp;
	struct mutex hist_cmd_lock;
	struct mml_pq_readback_buffer *chist_hist[MML_PIPE_CNT];
	struct cmdq_client *clt;
	struct cmdq_pkt *hist_pkts[MML_PIPE_CNT];
	struct workqueue_struct *chist_hist_wq;
	struct work_struct chist_hist_task;
	struct mml_pq_config pq_config;
	struct mml_pq_frame_data frame_data;
	struct mml_dev *mml;
};

enum chist_label_index {
	CHIST_REUSE_LABEL = 0,
	CHIST_POLLGPR_0 = DS_REG_NUM+DS_CLARITY_REG_NUM,
	CHIST_POLLGPR_1,
	CHIST_LABEL_TOTAL
};

/* meta data for each different frame config */
struct chist_frame_data {
	u32 out_hist_xs;
	u32 out_hist_ys;
	u32 cut_pos_x;
	u16 labels[CHIST_LABEL_TOTAL];
	bool is_clarity_need_readback;
	bool is_dc_need_readback;
	bool config_success;
};

#define chist_frm_data(i)	((struct chist_frame_data *)(i->data))

static inline struct mml_comp_chist *comp_to_chist(struct mml_comp *comp)
{
	return container_of(comp, struct mml_comp_chist, comp);
}

static s32 chist_prepare(struct mml_comp *comp, struct mml_task *task,
			 struct mml_comp_config *ccfg)
{
	struct chist_frame_data *chist_frm = NULL;

	chist_frm = kzalloc(sizeof(*chist_frm), GFP_KERNEL);

	ccfg->data = chist_frm;
	return 0;
}

static s32 chist_buf_prepare(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	s32 ret = 0;

	if (!dest->pq_config.en_sharp && !dest->pq_config.en_dc)
		return ret;

	mml_pq_trace_ex_begin("%s", __func__);
	ret = mml_pq_set_comp_config(task);
	mml_pq_trace_ex_end();
	return ret;
}

static s32 chist_tile_prepare(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg,
			      struct tile_func_block *func,
			      union mml_tile_data *data)
{
	const struct mml_frame_config *cfg = task->config;
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];

	func->enable_flag = dest->pq_config.en_hdr || dest->pq_config.en_dre;

	return 0;
}

static const struct mml_comp_tile_ops chist_tile_ops = {
	.prepare = chist_tile_prepare,
};

static u32 chist_get_label_count(struct mml_comp *comp, struct mml_task *task,
			struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];

	mml_pq_msg("%s pipe_id[%d] engine_id[%d] en_sharp[%d]", __func__,
		ccfg->pipe, comp->id, dest->pq_config.en_sharp);

	if (!dest->pq_config.en_sharp && !dest->pq_config.en_dc)
		return 0;

	return CHIST_LABEL_TOTAL;
}

static void chist_init(struct mml_comp *comp, struct cmdq_pkt *pkt, const phys_addr_t base_pa,
	bool shadow)
{
	struct mml_comp_chist *chist = comp_to_chist(comp);

	cmdq_pkt_write(pkt, NULL, base_pa + chist->data->reg_table[CHIST_CTRL], 0x1, 0x00000001);
	cmdq_pkt_write(pkt, NULL, base_pa + chist->data->reg_table[CHIST_CFG], 0x2, 0x00000002);
	/* Enable shadow */
	cmdq_pkt_write(pkt, NULL, base_pa + chist->data->reg_table[CHIST_SHADOW_CTRL],
		(shadow ? 0 : 1) | 0x2, U32_MAX);
}

static void chist_relay(struct mml_comp *comp, struct cmdq_pkt *pkt, const phys_addr_t base_pa,
			u32 relay)
{
	struct mml_comp_chist *chist = comp_to_chist(comp);

	/* 17	ALPHA_EN
	 * 0	RELAY_MODE
	 */
	cmdq_pkt_write(pkt, NULL, base_pa + chist->data->reg_table[CHIST_CFG], relay, 0x00020001);
}

static void chist_config_region_pq(struct mml_comp *comp, struct cmdq_pkt *pkt,
			const phys_addr_t base_pa, const struct mml_pq_config *cfg)
{
	struct mml_comp_chist *chist = comp_to_chist(comp);

	if (chist->data->reg_table[CHIST_REGION_PQ_PARAM] != REG_NOT_SUPPORT) {

		mml_pq_msg("%s:en_region_pq[%d] en_color[%d]", __func__,
			cfg->en_region_pq, cfg->en_color);

		if (!cfg->en_region_pq)
			cmdq_pkt_write(pkt, NULL,
				base_pa + chist->data->reg_table[CHIST_REGION_PQ_PARAM],
				0, U32_MAX);
		else if (cfg->en_region_pq && !cfg->en_color)
			cmdq_pkt_write(pkt, NULL,
				base_pa + chist->data->reg_table[CHIST_REGION_PQ_PARAM],
				0x5C1B0, U32_MAX);
		else if (cfg->en_region_pq && cfg->en_color)
			cmdq_pkt_write(pkt, NULL,
				base_pa + chist->data->reg_table[CHIST_REGION_PQ_PARAM],
				0xDC1B0, U32_MAX);
	}
}

static s32 chist_config_init(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg)
{
	chist_init(comp, task->pkts[ccfg->pipe], comp->base_pa, task->config->shadow);
	return 0;
}

static struct mml_pq_comp_config_result *get_chist_comp_config_result(
	struct mml_task *task)
{
	return task->pq_task->comp_config.result;
}

static s32 chist_hist_ctrl(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg,
			    struct mml_pq_comp_config_result *result)
{
	struct mml_comp_chist *chist = comp_to_chist(comp);
	struct mml_frame_config *cfg = task->config;
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	struct chist_frame_data *chist_frm = chist_frm_data(ccfg);

	if (!result->is_dc_need_readback && !result->is_clarity_need_readback)
		return 0;

	mml_pq_set_chist_status(task->pq_task, ccfg->node->out_idx);

	if (task->pq_task->read_status.chist_comp == MML_PQ_HIST_IDLE) {

		if (!chist->clt)
			chist->clt = mml_get_cmdq_clt(cfg->mml,
				ccfg->pipe + GCE_THREAD_START);

		if ((!chist->hist_pkts[ccfg->pipe] && chist->clt))
			chist->hist_pkts[ccfg->pipe] =
				cmdq_pkt_create(chist->clt);

		if (!chist->chist_hist[ccfg->pipe])
			chist->chist_hist[ccfg->pipe] =
				kzalloc(sizeof(struct mml_pq_readback_buffer),
				GFP_KERNEL);

		if (chist->chist_hist[ccfg->pipe] &&
			!chist->chist_hist[ccfg->pipe]->va &&
			chist->clt)
			chist->chist_hist[ccfg->pipe]->va =
				(u32 *)cmdq_mbox_buf_alloc(chist->clt,
				&chist->chist_hist[ccfg->pipe]->pa);

		if (chist->hist_pkts[ccfg->pipe] && chist->chist_hist[ccfg->pipe]->va) {

			chist->pq_task = task->pq_task;
			mml_pq_get_pq_task(chist->pq_task);

			chist->pipe = ccfg->pipe;
			chist->dual = cfg->dual;
			chist->out_idx = ccfg->node->out_idx;
			chist->jobid = task->job.jobid;

			chist->frame_data.size_info.out_rotate[ccfg->node->out_idx] =
				cfg->out_rotate[ccfg->node->out_idx];
			memcpy(&chist->pq_config, &dest->pq_config,
				sizeof(struct mml_pq_config));
			chist->clarity_readback = chist_frm->is_clarity_need_readback;
			chist->dc_readback = chist_frm->is_dc_need_readback;
			memcpy(&chist->frame_data.pq_param, task->pq_param,
				MML_MAX_OUTPUTS * sizeof(struct mml_pq_param));
			memcpy(&chist->frame_data.info, &task->config->info,
				sizeof(struct mml_frame_info));
			memcpy(&chist->frame_data.frame_out, &task->config->frame_out,
				MML_MAX_OUTPUTS * sizeof(struct mml_frame_size));
			memcpy(&chist->frame_data.size_info.frame_in_crop_s[0],
				&cfg->frame_in_crop[0],	MML_MAX_OUTPUTS * sizeof(struct mml_crop));

			chist->frame_data.size_info.crop_size_s.width =
				cfg->frame_tile_sz.width;
			chist->frame_data.size_info.crop_size_s.height=
				cfg->frame_tile_sz.height;
			chist->frame_data.size_info.frame_size_s.width = cfg->frame_in.width;
			chist->frame_data.size_info.frame_size_s.height = cfg->frame_in.height;


			chist->hist_pkts[ccfg->pipe]->no_irq = task->config->dpc;

			if (chist->data->rb_mode == RB_EOF_MODE) {
				mml_clock_lock(task->config->mml);
				/* ccf power on */
				call_hw_op(task->config->path[0]->mmlsys,
					mminfra_pw_enable);
				call_hw_op(task->config->path[0]->mmlsys,
					pw_enable, task->config->info.mode, false);
				/* dpc exception flow on */
				mml_msg_dpc("%s dpc exception flow on", __func__);
				mml_dpc_exc_keep(task->config->mml, comp->sysid);
				call_hw_op(comp, clk_enable);
				mml_clock_unlock(task->config->mml);
				mml_lock_wake_lock(chist->mml, true);
				chist->dpc = task->config->dpc;
				chist->mmlsys_comp =
					task->config->path[0]->mmlsys;
			}

			queue_work(chist->chist_hist_wq, &chist->chist_hist_task);
		}
	}
	mml_pq_ir_log("%s end jobid[%d] pipe[%d] engine[%d]",
		__func__, task->job.jobid, ccfg->pipe, comp->id);

	return 0;
}


static s32 chist_config_frame(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];

	struct chist_frame_data *chist_frm = chist_frm_data(ccfg);
	struct mml_frame_data *src = &cfg->info.src;
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];
	struct mml_comp_chist *chist = comp_to_chist(comp);
	u32 alpha = cfg->info.alpha ? 1 << 17 : 0;

	const phys_addr_t base_pa = comp->base_pa;
	struct mml_pq_comp_config_result *result;
	s32 ret;
	s32 i;
	struct mml_pq_reg *regs = NULL;
	s8 mode = task->config->info.mode;

	mml_pq_msg("%s pipe_id[%d] engine_id[%d] en_sharp[%d]", __func__,
		ccfg->pipe, comp->id, dest->pq_config.en_sharp);

	if (MML_FMT_10BIT(src->format) || MML_FMT_10BIT(dest->data.format)) {
		cmdq_pkt_write(pkt, NULL,
			base_pa + chist->data->reg_table[CHIST_CTRL], 0, 0x00000004);
	} else {
		cmdq_pkt_write(pkt, NULL,
			base_pa + chist->data->reg_table[CHIST_CTRL], 0x4, 0x00000004);
	}

	chist_config_region_pq(comp, pkt, base_pa, &dest->pq_config);

	if (!dest->pq_config.en_sharp && !dest->pq_config.en_dc) {
		/* relay mode */
		if (cfg->info.mode == MML_MODE_DDP_ADDON ||
			cfg->info.mode == MML_MODE_DIRECT_LINK) {
			/* enable to crop */
			chist_relay(comp, pkt, base_pa, alpha | 0x1);
			cmdq_pkt_write(pkt, NULL,
				base_pa + chist->data->reg_table[CHIST_00], 0, 1 << 31);
		} else {
			chist_relay(comp, pkt, base_pa, alpha | 0x1);
		}
		return 0;
	}

	chist_relay(comp, pkt, base_pa, alpha);

	do {
		ret = mml_pq_get_comp_config_result(task, CHIST_WAIT_TIMEOUT_MS);
		if (ret) {
			mml_pq_comp_config_clear(task);
			chist_frm->config_success = false;
			if (dest->pq_config.en_region_pq) {
				// DON'T relay hardware, but disable sub-module
				cmdq_pkt_write(pkt, NULL,
					base_pa + chist->data->reg_table[CHIST_00],
					1 << 31|1 << 28, 1 << 31|1 << 28);
				cmdq_pkt_write(pkt, NULL,
					base_pa + chist->data->reg_table[TDHSP_C_BOOST_MAIN],
					1 << 13, 1 << 13);
				// enable map input, disable map output
				cmdq_pkt_write(pkt, NULL,
					base_pa + chist->data->reg_table[CHIST_REGION_PQ_PARAM],
					0x5C1B0, U32_MAX);
			} else
				chist_relay(comp, pkt, base_pa, alpha | 0x1);
			mml_pq_err("%s:get ds param timeout: %d in %dms", __func__,
				ret, CHIST_WAIT_TIMEOUT_MS);
			ret = -ETIMEDOUT;
			goto exit;
		}

		result = get_chist_comp_config_result(task);
		if (!result) {
			chist_frm->config_success = false;
			if (dest->pq_config.en_region_pq) {
				// DON'T relay hardware, but disable sub-module
				cmdq_pkt_write(pkt, NULL,
					base_pa + chist->data->reg_table[CHIST_00],
					1 << 31|1 << 28, 1 << 31|1 << 28);
				cmdq_pkt_write(pkt, NULL,
					base_pa + chist->data->reg_table[TDHSP_C_BOOST_MAIN],
					1 << 13, 1 << 13);
				// enable map input, disable map output
				cmdq_pkt_write(pkt, NULL,
					base_pa + chist->data->reg_table[CHIST_REGION_PQ_PARAM],
					0x5C1B0, U32_MAX);
			} else
				chist_relay(comp, pkt, base_pa, alpha | 0x1);
			mml_pq_err("%s: not get result from user lib", __func__);
			ret = -EBUSY;
			goto exit;
		}
	} while ((mml_pq_debug_mode & MML_PQ_SET_TEST) && result->is_set_test);

	regs = result->ds_regs;

	/* TODO: use different regs */
	mml_pq_msg("%s:config ds regs, count: %d", __func__, result->ds_reg_cnt);
	chist_frm->config_success = true;
	for (i = 0; i < result->ds_reg_cnt; i++) {
		mml_write(comp->id, pkt, base_pa + regs[i].offset, regs[i].value,
			regs[i].mask, reuse, cache,
			&chist_frm->labels[i]);

		mml_pq_msg("[ds][config][%x] = %#x mask(%#x)",
			regs[i].offset, regs[i].value, regs[i].mask);
	}

	chist_frm->is_clarity_need_readback = result->is_clarity_need_readback;
	chist_frm->is_dc_need_readback = result->is_dc_need_readback;

	if ((mode == MML_MODE_DDP_ADDON || mode == MML_MODE_DIRECT_LINK) &&
		((dest->pq_config.en_dre && dest->pq_config.en_sharp) ||
		dest->pq_config.en_dc))
		chist_hist_ctrl(comp, task, ccfg, result);

exit:
	return ret;
}

static s32 chist_config_tile(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg, u32 idx)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;
	struct chist_frame_data *chist_frm = chist_frm_data(ccfg);
	struct mml_tile_engine *tile = config_get_tile(cfg, ccfg, idx);
	struct mml_comp_chist *chist = comp_to_chist(comp);
	u16 tile_cnt = cfg->frame_tile[ccfg->pipe]->tile_cnt;
	const struct mml_crop *crop = &cfg->frame_in_crop[ccfg->node->out_idx];

	u32 chist_input_w = 0;
	u32 chist_input_h = 0;
	u32 chist_output_w = 0;
	u32 chist_output_h = 0;
	u32 chist_crop_x_offset = 0;
	u32 chist_crop_y_offset = 0;
	u32 chist_hist_left_start = 0, chist_hist_top_start = 0;
	u32 hist_win_x_start = 0, hist_win_x_end = 0;
	u32 hist_win_y_start = 0, hist_win_y_end = 0;
	u32 hist_first_tile = 0, hist_last_tile = 0;

	mml_pq_msg("%s idx[%d] engine_id[%d]", __func__, idx, comp->id);

	chist_input_w = tile->in.xe - tile->in.xs + 1;
	chist_input_h = tile->in.ye - tile->in.ys + 1;
	chist_output_w = tile->out.xe - tile->out.xs + 1;
	chist_output_h = tile->out.ye - tile->out.ys + 1;
	chist_crop_x_offset = tile->out.xs - tile->in.xs;
	chist_crop_y_offset = tile->out.ys - tile->in.ys;

	if (!idx) {
		if (task->config->dual)
			chist_frm->cut_pos_x = crop->r.width / 2;
		else
			chist_frm->cut_pos_x = crop->r.width;
		if (ccfg->pipe)
			chist_frm->out_hist_xs = chist_frm->cut_pos_x;
	}

	chist_frm->out_hist_ys = idx ? (tile->out.ye + 1) : 0;

	chist_hist_left_start =
		(tile->out.xs > chist_frm->out_hist_xs) ? tile->out.xs : chist_frm->out_hist_xs;
	chist_hist_top_start =
		(tile->out.ys > chist_frm->out_hist_ys) ? tile->out.ys  : chist_frm->out_hist_ys;

	hist_win_x_start = chist_hist_left_start - tile->in.xs;
	if (task->config->dual && !ccfg->pipe && (idx + 1 >= tile_cnt))
		hist_win_x_end = chist_frm->cut_pos_x - tile->in.xs - 1;
	else
		hist_win_x_end = tile->out.xe - tile->in.xs;

	hist_win_y_start = chist_hist_top_start - tile->in.ys;
	hist_win_y_end = tile->out.xe - tile->in.xs;


	if (!idx) {
		if (task->config->dual && ccfg->pipe)
			chist_frm->out_hist_xs = tile->in.xs + hist_win_x_end + 1;
		else
			chist_frm->out_hist_xs = tile->out.xe + 1;
		hist_first_tile = 1;
		hist_last_tile = (tile_cnt == 1) ? 1 : 0;
	} else if (idx + 1 >= tile_cnt) {
		chist_frm->out_hist_xs = 0;
		hist_first_tile = 0;
		hist_last_tile = 1;
	} else {
		if (task->config->dual && ccfg->pipe)
			chist_frm->out_hist_xs = tile->in.xs + hist_win_x_end + 1;
		else
			chist_frm->out_hist_xs = tile->out.xe + 1;
		hist_first_tile = 0;
		hist_last_tile = 0;
	}

	cmdq_pkt_write(pkt, NULL, base_pa + chist->data->reg_table[CHIST_INPUT_SIZE],
		(chist_input_w << 16) + chist_input_h, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + chist->data->reg_table[CHIST_OUTPUT_OFFSET],
		(chist_crop_x_offset << 16) + chist_crop_y_offset, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + chist->data->reg_table[CHIST_OUTPUT_SIZE],
		(chist_output_w << 16) + chist_output_h, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + chist->data->reg_table[HIST_CFG_00],
		(hist_win_x_end << 16) | (hist_win_x_start << 0), U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + chist->data->reg_table[HIST_CFG_01],
		(hist_win_y_end << 16) | (hist_win_y_start << 0), U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + chist->data->reg_table[CHIST_CFG],
		(hist_last_tile << 14) | (hist_first_tile << 15), (1 << 14) | (1 << 15));

	return 0;
}

static void chist_readback_cmdq(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg)
{
	struct mml_comp_chist *chist = comp_to_chist(comp);
	struct chist_frame_data *chist_frm = chist_frm_data(ccfg);
	const phys_addr_t base_pa = comp->base_pa;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	struct mml_frame_config *cfg = task->config;
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];

	u8 pipe = ccfg->pipe;
	dma_addr_t pa = 0;
	struct cmdq_operand lop, rop;
	u32 i = 0;

	const u16 idx_val = CMDQ_THR_SPR_IDX2;
	const u16 idx_out = chist->data->cpr[ccfg->pipe];
	const u16 idx_out64 = CMDQ_CPR_TO_CPR64(idx_out);

	mml_pq_get_readback_buffer(task, pipe, &(task->pq_task->chist_hist[pipe]));

	if (unlikely(!task->pq_task->chist_hist[pipe])) {
		mml_pq_err("%s job_id[%d] engine_id[%d] chist_hist is null",
			__func__, task->job.jobid, comp->id);
		return;
	}

	pa = task->pq_task->chist_hist[pipe]->pa;

	/* readback to this pa */
	mml_assign(comp->id, pkt, idx_out, (u32)pa,
		reuse, cache, &chist_frm->labels[CHIST_POLLGPR_0]);
	mml_assign(comp->id, pkt, idx_out + 1, (u32)DO_SHIFT_RIGHT(pa, 32),
		reuse, cache, &chist_frm->labels[CHIST_POLLGPR_1]);

	/* read contour histogram status */
	for (i = 0; i < CHIST_CONTOUR_HIST_NUM; i++) {
		cmdq_pkt_read_addr(pkt, base_pa +
			chist->data->reg_table[CONTOUR_HIST_00] + i * 4, idx_val);
		cmdq_pkt_write_reg_indriect(pkt, idx_out64, idx_val, U32_MAX);

		lop.reg = true;
		lop.idx = idx_out;
		rop.reg = false;
		rop.value = 4;
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_out, &lop, &rop);
	}

	/* read chist clarity status */
	if (chist->data->reg_table[CHIST_STATUS_00] != REG_NOT_SUPPORT) {
		for (i = 0; i < CHIST_CLARITY_STATUS_NUM; i++) {
			cmdq_pkt_read_addr(pkt, base_pa +
				chist->data->reg_table[CHIST_STATUS_00] + i * 4, idx_val);
			cmdq_pkt_write_reg_indriect(pkt, idx_out64, idx_val, U32_MAX);

			lop.reg = true;
			lop.idx = idx_out;
			rop.reg = false;
			rop.value = 4;
			cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_out, &lop, &rop);
		}
	}


	mml_pq_rb_msg("%s end job_id[%d] engine_id[%d] va[%p] pa[%pad] pkt[%p]",
		__func__, task->job.jobid, comp->id, task->pq_task->chist_hist[pipe]->va,
		&task->pq_task->chist_hist[pipe]->pa, pkt);
}


static s32 chist_config_post(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg)
{
	struct mml_frame_dest *dest = &task->config->info.dest[ccfg->node->out_idx];
	s8 mode = task->config->info.mode;

	/*Skip readback, need to add flow in IR//DL*/
	if (!mml_isdc(mode))
		goto put_comp_config;

	if ((dest->pq_config.en_sharp && dest->pq_config.en_dre) ||
		dest->pq_config.en_dc)
		chist_readback_cmdq(comp, task, ccfg);

put_comp_config:
	if (dest->pq_config.en_sharp || dest->pq_config.en_dc)
		mml_pq_put_comp_config_result(task);
	return 0;
}

static s32 chist_reconfig_frame(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;

	struct chist_frame_data *chist_frm = chist_frm_data(ccfg);
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];

	struct mml_pq_comp_config_result *result = NULL;
	s32 ret = 0;
	s32 i;
	struct mml_pq_reg *regs = NULL;
	s8 mode = task->config->info.mode;

	if (!dest->pq_config.en_sharp && !dest->pq_config.en_dc)
		return ret;

	do {
		ret = mml_pq_get_comp_config_result(task, CHIST_WAIT_TIMEOUT_MS);
		if (ret) {
			mml_pq_comp_config_clear(task);
			mml_pq_err("get chist param timeout: %d in %dms",
				ret, CHIST_WAIT_TIMEOUT_MS);
			ret = -ETIMEDOUT;
			goto exit;
		}

		result = get_chist_comp_config_result(task);
		if (!result || !chist_frm->config_success || !result->ds_reg_cnt) {
			mml_pq_err("%s: not get result from user lib", __func__);
			ret = -EBUSY;
			goto exit;
		}
	} while ((mml_pq_debug_mode & MML_PQ_SET_TEST) && result->is_set_test);

	regs = result->ds_regs;
	/* TODO: use different regs */
	mml_pq_msg("%s:config ds regs, count: %d is_set_test[%d]", __func__, result->ds_reg_cnt,
		result->is_set_test);
	for (i = 0; i < result->ds_reg_cnt; i++) {
		mml_update(comp->id, reuse, chist_frm->labels[i], regs[i].value);
		mml_pq_msg("[ds][config][%x] = %#x mask(%#x)",
			regs[i].offset, regs[i].value, regs[i].mask);
	}
	chist_frm->is_clarity_need_readback = result->is_clarity_need_readback;
	chist_frm->is_dc_need_readback = result->is_dc_need_readback;

	if ((mode == MML_MODE_DDP_ADDON || mode == MML_MODE_DIRECT_LINK) &&
		((dest->pq_config.en_dre && dest->pq_config.en_sharp) ||
		dest->pq_config.en_dc))
		chist_hist_ctrl(comp, task, ccfg, result);
exit:
	return ret;
}

static s32 chist_config_repost(struct mml_comp *comp, struct mml_task *task,
			       struct mml_comp_config *ccfg)
{
	struct mml_frame_dest *dest = &task->config->info.dest[ccfg->node->out_idx];
	struct chist_frame_data *chist_frm = chist_frm_data(ccfg);
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	u8 pipe = ccfg->pipe;
	s8 mode = task->config->info.mode;

	if (!mml_isdc(mode))
		goto put_comp_config;

	if ((dest->pq_config.en_sharp && dest->pq_config.en_dre) ||
		dest->pq_config.en_dc) {

		mml_pq_get_readback_buffer(task, pipe, &(task->pq_task->chist_hist[pipe]));

		if (unlikely(!task->pq_task->chist_hist[pipe])) {
			mml_pq_err("%s job_id[%d] chist_hist is null", __func__,
				task->job.jobid);
			goto put_comp_config;
		}

		mml_update(comp->id, reuse, chist_frm->labels[CHIST_POLLGPR_0],
			(u32)task->pq_task->chist_hist[pipe]->pa);
		mml_update(comp->id, reuse, chist_frm->labels[CHIST_POLLGPR_1],
			(u32)DO_SHIFT_RIGHT(task->pq_task->chist_hist[pipe]->pa, 32));
	}

put_comp_config:
	if (dest->pq_config.en_sharp || dest->pq_config.en_dc)
		mml_pq_put_comp_config_result(task);
	return 0;
}

static const struct mml_comp_config_ops chist_cfg_ops = {
	.prepare = chist_prepare,
	.buf_prepare = chist_buf_prepare,
	.get_label_count = chist_get_label_count,
	.init = chist_config_init,
	.frame = chist_config_frame,
	.tile = chist_config_tile,
	.post = chist_config_post,
	.reframe = chist_reconfig_frame,
	.repost = chist_config_repost,
};

static void chist_task_done_readback(struct mml_comp *comp, struct mml_task *task,
					 struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct chist_frame_data *chist_frm = chist_frm_data(ccfg);
	struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	struct mml_comp_chist *chist = comp_to_chist(comp);
	u8 pipe = ccfg->pipe;
	u32 offset = 0, i = 0;

	mml_pq_trace_ex_begin("%s comp[%d]", __func__, comp->id);
	mml_pq_msg("%s clarity_readback[%d] id[%d] en_sharp[%d] chist_hist[%lx]", __func__,
			chist_frm->is_clarity_need_readback, comp->id,
			dest->pq_config.en_sharp, (unsigned long)&(task->pq_task->chist_hist[pipe]));

	if (((!dest->pq_config.en_sharp || !dest->pq_config.en_dre) &&
		!dest->pq_config.en_dc) || !task->pq_task->chist_hist[pipe])
		goto exit;

	mml_pq_rb_msg("%s job_id[%d] id[%d] pipe[%d] en_dc[%d] va[%p] pa[%pad] offset[%d]",
		__func__, task->job.jobid, comp->id, ccfg->pipe,
		dest->pq_config.en_dc, task->pq_task->chist_hist[pipe]->va,
		&task->pq_task->chist_hist[pipe]->pa,
		task->pq_task->chist_hist[pipe]->va_offset);


	mml_pq_rb_msg("%s job_id[%d] hist[0~4]={%08x, %08x, %08x, %08x, %08x}",
		__func__, task->job.jobid,
		task->pq_task->chist_hist[pipe]->va[offset/4+0],
		task->pq_task->chist_hist[pipe]->va[offset/4+1],
		task->pq_task->chist_hist[pipe]->va[offset/4+2],
		task->pq_task->chist_hist[pipe]->va[offset/4+3],
		task->pq_task->chist_hist[pipe]->va[offset/4+4]);

	mml_pq_rb_msg("%s job_id[%d] hist[10~14]={%08x, %08x, %08x, %08x, %08x}",
		__func__, task->job.jobid,
		task->pq_task->chist_hist[pipe]->va[offset/4+10],
		task->pq_task->chist_hist[pipe]->va[offset/4+11],
		task->pq_task->chist_hist[pipe]->va[offset/4+12],
		task->pq_task->chist_hist[pipe]->va[offset/4+13],
		task->pq_task->chist_hist[pipe]->va[offset/4+14]);

	mml_pq_rb_msg("%s job_id[%d]",
		__func__, task->job.jobid);

	/*remain code for ping-pong in the feature*/
	if (((!dest->pq_config.en_sharp || !dest->pq_config.en_dre) &&
		!dest->pq_config.en_dc)) {
		void __iomem *base = comp->base;
		s32 i;
		u32 *phist = kmalloc((CHIST_CONTOUR_HIST_NUM+
			CHIST_CLARITY_STATUS_NUM)*sizeof(u32), GFP_KERNEL);

		for (i = 0; i < CHIST_CONTOUR_HIST_NUM; i++)
			phist[i] = readl(base + chist->data->reg_table[CONTOUR_HIST_00]);

		for (i = 0; i < CHIST_CLARITY_STATUS_NUM; i++)
			phist[i] = readl(base + chist->data->reg_table[CONTOUR_HIST_00]);

		if (chist_frm->is_dc_need_readback)
			mml_pq_dc_readback(task, ccfg->pipe, &phist[0]);
		if (chist_frm->is_clarity_need_readback)
			mml_pq_clarity_readback(task, ccfg->pipe,
				&phist[CHIST_CONTOUR_HIST_NUM],
				CHIST_CLARITY_HIST_START,
				CHIST_CLARITY_STATUS_NUM);

	}

	if (chist_frm->is_dc_need_readback) {
		if (mml_pq_debug_mode & MML_PQ_HIST_CHECK) {
			for (i = 0; i < CHIST_CONTOUR_HIST_NUM; i++)
				if (task->pq_task->chist_hist[pipe]->va[offset/4+i] >
					VALID_CONTOUR_HIST_VALUE)
					mml_pq_util_aee("MML_PQ_CHIST Histogram Error",
						"CONTOUR Histogram error need to check jobid:%d",
						task->job.jobid);
		}
		mml_pq_dc_readback(task, ccfg->pipe,
			&(task->pq_task->chist_hist[pipe]->va[offset/4+0]));
	}

	if (chist_frm->is_clarity_need_readback) {

		mml_pq_rb_msg("%s job_id[%d] clarity_hist[0~4]={%08x, %08x, %08x, %08x, %08x}",
			__func__, task->job.jobid,
			task->pq_task->chist_hist[pipe]->va[offset/4+CHIST_CONTOUR_HIST_NUM+0],
			task->pq_task->chist_hist[pipe]->va[offset/4+CHIST_CONTOUR_HIST_NUM+1],
			task->pq_task->chist_hist[pipe]->va[offset/4+CHIST_CONTOUR_HIST_NUM+2],
			task->pq_task->chist_hist[pipe]->va[offset/4+CHIST_CONTOUR_HIST_NUM+3],
			task->pq_task->chist_hist[pipe]->va[offset/4+CHIST_CONTOUR_HIST_NUM+4]);

		mml_pq_clarity_readback(task, ccfg->pipe,
			&(task->pq_task->chist_hist[pipe]->va[
			offset/4+CHIST_CONTOUR_HIST_NUM]),
			CHIST_CLARITY_HIST_START, CHIST_CLARITY_STATUS_NUM);
	}

	mml_pq_put_readback_buffer(task, pipe, &(task->pq_task->chist_hist[pipe]));
exit:
	mml_pq_trace_ex_end();
}

static void chist_init_frame_done_event(struct mml_comp *comp, u32 event)
{
	struct mml_comp_chist *chist = comp_to_chist(comp);

	if (!chist->event_eof)
		chist->event_eof = event;
}

static const struct mml_comp_hw_ops chist_hw_ops = {
	.init_frame_done_event = &chist_init_frame_done_event,
	.clk_enable = &mml_comp_clk_enable,
	.clk_disable = &mml_comp_clk_disable,
	.task_done = chist_task_done_readback,
};

static u32 read_reg_value(struct mml_comp *comp, u16 reg)
{
	void __iomem *base = comp->base;

	if (reg == REG_NOT_SUPPORT) {
		mml_err("%s chist reg is not support", __func__);
		return 0xFFFFFFFF;
	}

	return readl(base + reg);
}

static void chist_debug_dump(struct mml_comp *comp)
{
	struct mml_comp_chist *chist = comp_to_chist(comp);
	void __iomem *base = comp->base;
	u32 value[12];
	u32 shadow_ctrl;

	mml_err("chist component %u dump:", comp->id);

	/* Enable shadow read working */
	shadow_ctrl = read_reg_value(comp, chist->data->reg_table[CHIST_SHADOW_CTRL]);
	shadow_ctrl |= 0x4;
	writel(shadow_ctrl, base + chist->data->reg_table[CHIST_SHADOW_CTRL]);

	value[0] = read_reg_value(comp, chist->data->reg_table[CHIST_CTRL]);
	value[1] = read_reg_value(comp, chist->data->reg_table[CHIST_INTEN]);
	value[2] = read_reg_value(comp, chist->data->reg_table[CHIST_INTSTA]);
	value[3] = read_reg_value(comp, chist->data->reg_table[CHIST_STATUS]);
	value[4] = read_reg_value(comp, chist->data->reg_table[CHIST_CFG]);
	value[5] = read_reg_value(comp, chist->data->reg_table[CHIST_INPUT_COUNT]);
	value[6] = read_reg_value(comp, chist->data->reg_table[CHIST_OUTPUT_COUNT]);
	value[7] = read_reg_value(comp, chist->data->reg_table[CHIST_INPUT_SIZE]);
	value[8] = read_reg_value(comp, chist->data->reg_table[CHIST_OUTPUT_OFFSET]);
	value[9] = read_reg_value(comp, chist->data->reg_table[CHIST_OUTPUT_SIZE]);
	value[10] = read_reg_value(comp, chist->data->reg_table[CHIST_BLANK_WIDTH]);

	mml_err("CHIST_CTRL %#010x CHIST_INTEN %#010x CHIST_INTSTA %#010x CHIST_STATUS %#010x",
		value[0], value[1], value[2], value[3]);
	mml_err("CHIST_CFG %#010x CHIST_INPUT_COUNT %#010x CHIST_OUTPUT_COUNT %#010x",
		value[4], value[5], value[6]);
	mml_err("CHIST_INPUT_SIZE %#010x CHIST_OUTPUT_OFFSET %#010x CHIST_OUTPUT_SIZE %#010x",
		value[7], value[8], value[9]);
	mml_err("CHIST_BLANK_WIDTH %#010x", value[10]);

	if (chist->data->reg_table[CHIST_REGION_PQ_PARAM] != REG_NOT_SUPPORT) {
		value[11] = readl(base + chist->data->reg_table[CHIST_REGION_PQ_PARAM]);
		mml_err("CHIST_REGION_PQ_PARAM %#010x", value[11]);
	}
}

static const struct mml_comp_debug_ops chist_debug_ops = {
	.dump = &chist_debug_dump,
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_chist *chist = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	s32 ret;

	if (!drm_dev) {
		ret = mml_register_comp(master, &chist->comp);
		if (ret)
			dev_err(dev, "Failed to register mml component %s: %d\n",
				dev->of_node->full_name, ret);
		chist->mml = dev_get_drvdata(master);
	} else {
		ret = mml_ddp_comp_register(drm_dev, &chist->ddp_comp);
		if (ret)
			dev_err(dev, "Failed to register ddp component %s: %d\n",
				dev->of_node->full_name, ret);
		else
			chist->ddp_bound = true;
	}
	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_chist *chist = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	if (!drm_dev) {
		mml_unregister_comp(master, &chist->comp);
	} else {
		mml_ddp_comp_unregister(drm_dev, &chist->ddp_comp);
		chist->ddp_bound = false;
	}
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static struct mml_comp_chist *dbg_probed_components[4];
static int dbg_probed_count;

static void chist_histdone_cb(struct cmdq_cb_data data)
{
	struct cmdq_pkt *pkt = (struct cmdq_pkt *)data.data;
	struct mml_comp_chist *chist = (struct mml_comp_chist *)pkt->user_data;
	struct mml_comp *comp = &chist->comp;
	u32 pipe;

	mml_pq_ir_log("%s jobid[%d] chist->hist_pkts[0] = [%p] hdr->hist_pkts[1] = [%p]",
		__func__, chist->jobid, chist->hist_pkts[0], chist->hist_pkts[1]);

	if (mml_pq_chist_hist_reading(chist->pq_task, chist->out_idx, chist->pipe))
		return;

	if (pkt == chist->hist_pkts[0]) {
		pipe = 0;
	} else if (pkt == chist->hist_pkts[1]) {
		pipe = 1;
	} else {
		mml_err("%s task %p pkt %p not match both pipe (%p and %p)",
			__func__, chist, pkt, chist->hist_pkts[0],
			chist->hist_pkts[1]);
		return;
	}

	mml_pq_ir_log("%s hist[0~4]={%08x, %08x, %08x, %08x, %08x}",
		__func__,
		chist->chist_hist[pipe]->va[0],
		chist->chist_hist[pipe]->va[1],
		chist->chist_hist[pipe]->va[2],
		chist->chist_hist[pipe]->va[3],
		chist->chist_hist[pipe]->va[4]);

	mml_pq_ir_log("%s hist[5~9]={%08x, %08x, %08x, %08x, %08x}",
		__func__,
		chist->chist_hist[pipe]->va[5],
		chist->chist_hist[pipe]->va[6],
		chist->chist_hist[pipe]->va[7],
		chist->chist_hist[pipe]->va[8],
		chist->chist_hist[pipe]->va[9]);

	mml_pq_ir_log("%s hist[10~14]={%08x, %08x, %08x, %08x, %08x}",
		__func__,
		chist->chist_hist[pipe]->va[10],
		chist->chist_hist[pipe]->va[11],
		chist->chist_hist[pipe]->va[12],
		chist->chist_hist[pipe]->va[13],
		chist->chist_hist[pipe]->va[14]);

	mml_pq_ir_log("%s hist[15~19]={%08x, %08x, %08x, %08x, %08x}",
		__func__,
		chist->chist_hist[pipe]->va[15],
		chist->chist_hist[pipe]->va[16],
		chist->chist_hist[pipe]->va[17],
		chist->chist_hist[pipe]->va[18],
		chist->chist_hist[pipe]->va[19]);

	mml_pq_ir_log("%s hist[20~24]={%08x, %08x, %08x, %08x, %08x}",
		__func__,
		chist->chist_hist[pipe]->va[20],
		chist->chist_hist[pipe]->va[21],
		chist->chist_hist[pipe]->va[22],
		chist->chist_hist[pipe]->va[23],
		chist->chist_hist[pipe]->va[24]);

	mml_pq_ir_log("%s hist[25~29]={%08x, %08x, %08x, %08x, %08x}",
		__func__,
		chist->chist_hist[pipe]->va[25],
		chist->chist_hist[pipe]->va[26],
		chist->chist_hist[pipe]->va[27],
		chist->chist_hist[pipe]->va[28],
		chist->chist_hist[pipe]->va[29]);

#if !IS_ENABLED(CONFIG_MTK_MML_LEGACY)
	if (chist->dc_readback)
		mml_pq_ir_dc_readback(chist->pq_task, chist->frame_data,
			chist->pipe, &(chist->chist_hist[pipe]->va[0]),
			chist->jobid, 0, chist->dual);

	if (chist->clarity_readback)
		mml_pq_ir_clarity_readback(chist->pq_task, chist->frame_data,
			chist->pipe, &(chist->chist_hist[pipe]->va[CHIST_CONTOUR_HIST_NUM]),
			chist->jobid, CHIST_CLARITY_STATUS_NUM, CHIST_CLARITY_HIST_START,
			chist->dual);
#endif

	if (chist->data->rb_mode == RB_EOF_MODE) {
		mml_clock_lock(chist->mml);
		call_hw_op(comp, clk_disable, chist->dpc);
		/* dpc exception flow off */
		mml_msg_dpc("%s dpc exception flow off", __func__);
		mml_dpc_exc_release(chist->mml, comp->sysid);
		/* ccf power off */
		call_hw_op(chist->mmlsys_comp, pw_disable,
			chist->pq_task->task->config->info.mode, false);
		call_hw_op(chist->mmlsys_comp, mminfra_pw_enable);
		mml_clock_unlock(chist->mml);
		mml_lock_wake_lock(chist->mml, false);
	}

	mml_pq_put_pq_task(chist->pq_task);

	mml_pq_chist_flag_check(chist->dual, chist->out_idx);

	mml_pq_ir_log("%s end jobid[%d] pkt[%p] hdr[%p] pipe[%d]",
		__func__, chist->jobid, pkt, chist, pipe);
	mml_trace_end();
}

static void chist_err_dump_cb(struct cmdq_cb_data data)
{
	struct mml_comp_chist *chist = (struct mml_comp_chist *)data.data;

	if (!chist) {
		mml_pq_err("%s chist is null", __func__);
		return;
	}

	mml_pq_ir_log("%s jobid[%d] chist->hist_pkts[0] = [%p] hdr->hist_pkts[1] = [%p]",
		__func__, chist->jobid, chist->hist_pkts[0], chist->hist_pkts[1]);

}


static void chist_hist_work(struct work_struct *work_item)
{
	struct mml_comp_chist *chist = NULL;
	struct mml_comp *comp = NULL;
	struct cmdq_pkt *pkt = NULL;
	struct cmdq_operand lop, rop;

	const u16 idx_val = CMDQ_THR_SPR_IDX2;
	u16 idx_out = 0;
	u16 idx_out64 = 0;

	u8 pipe = 0;
	phys_addr_t base_pa = 0;
	dma_addr_t pa = 0;
	u32 i = 0;

	chist = container_of(work_item, struct mml_comp_chist, chist_hist_task);

	if (!chist) {
		mml_pq_err("%s comp_chist is null", __func__);
		return;
	}

	pipe = chist->pipe;
	comp = &chist->comp;
	base_pa = comp->base_pa;
	pkt = chist->hist_pkts[pipe];
	idx_out = chist->data->cpr[chist->pipe];

	mml_pq_ir_log("%s job_id[%d] eng_id[%d] cmd_buf_size[%zu] hist_cmd_done[%d]",
		__func__, chist->jobid, comp->id,
		pkt->cmd_buf_size, chist->hist_cmd_done);

	idx_out64 = CMDQ_CPR_TO_CPR64(idx_out);

	if (unlikely(!chist->chist_hist[pipe])) {
		mml_pq_err("%s job_id[%d] eng_id[%d] pipe[%d] pkt[%p] chist_hist is null",
			__func__, chist->jobid, comp->id, pipe,
			chist->hist_pkts[pipe]);
		return;
	}

	mutex_lock(&chist->hist_cmd_lock);
	if (chist->hist_cmd_done) {
		mutex_unlock(&chist->hist_cmd_lock);
		goto chist_hist_cmd_done;
	}

	cmdq_pkt_wfe(pkt, chist->event_eof);

	pa = chist->chist_hist[pipe]->pa;

	/* readback to this pa */
	cmdq_pkt_assign_command(pkt, idx_out, (u32)pa);
	cmdq_pkt_assign_command(pkt, idx_out + 1, (u32)DO_SHIFT_RIGHT(pa, 32));

	/* read contour histogram status */
	for (i = 0; i < CHIST_CONTOUR_HIST_NUM; i++) {
		cmdq_pkt_read_addr(pkt, base_pa +
			chist->data->reg_table[CONTOUR_HIST_00] + i * 4, idx_val);
		cmdq_pkt_write_reg_indriect(pkt, idx_out64, idx_val, U32_MAX);

		lop.reg = true;
		lop.idx = idx_out;
		rop.reg = false;
		rop.value = 4;
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_out, &lop, &rop);
	}

	/* read chist clarity status */
	if (chist->data->reg_table[CHIST_STATUS_00] != REG_NOT_SUPPORT) {
		for (i = 0; i < CHIST_CLARITY_STATUS_NUM; i++) {
			cmdq_pkt_read_addr(pkt, base_pa +
				chist->data->reg_table[CHIST_STATUS_00] + i * 4, idx_val);
			cmdq_pkt_write_reg_indriect(pkt, idx_out64, idx_val, U32_MAX);

			lop.reg = true;
			lop.idx = idx_out;
			rop.reg = false;
			rop.value = 4;
			cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_out, &lop, &rop);
		}
	}

	mml_pq_rb_msg("%s end job_id[%d] engine_id[%d] va[%p] pa[%pad] pkt[%p]",
		__func__, chist->jobid, comp->id, chist->chist_hist[pipe]->va,
		&chist->chist_hist[pipe]->pa, pkt);

	chist->hist_cmd_done = true;

	mml_pq_rb_msg("%s end engine_id[%d] va[%p] pa[%pad] pkt[%p]",
		__func__, comp->id, chist->chist_hist[pipe]->va,
		&chist->chist_hist[pipe]->pa, pkt);

	pkt->user_data = chist;
	pkt->err_cb.cb = chist_err_dump_cb;
	pkt->err_cb.data = (void *)chist;

	mutex_unlock(&chist->hist_cmd_lock);

chist_hist_cmd_done:
	if (chist->pq_config.en_hdr)
		wait_for_completion(&chist->pq_task->hdr_curve_ready[chist->pipe]);

	cmdq_pkt_refinalize(pkt);
	cmdq_pkt_flush_threaded(pkt, chist_histdone_cb, (void *)chist->hist_pkts[pipe]);

	mml_pq_ir_log("%s end job_id[%d] pkts[%p %p] id[%d] size[%zu] cmd_done[%d] irq[%d]",
		__func__, chist->jobid,
		chist->hist_pkts[0], chist->hist_pkts[1], comp->id,
		pkt->cmd_buf_size, chist->hist_cmd_done,
		pkt->no_irq);
}

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_comp_chist *priv;
	s32 ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	priv->data = of_device_get_match_data(dev);

	ret = mml_comp_init(pdev, &priv->comp);
	if (ret) {
		dev_err(dev, "Failed to init mml component: %d\n", ret);
		return ret;
	}

	if (of_property_read_u16(dev->of_node, "event-frame-done",
				 &priv->event_eof))
		dev_err(dev, "read event frame_done fail\n");

	priv->chist_hist_wq = create_singlethread_workqueue("chist_hist_read");
	INIT_WORK(&priv->chist_hist_task, chist_hist_work);

	mutex_init(&priv->hist_cmd_lock);

	/* assign ops */
	priv->comp.tile_ops = &chist_tile_ops;
	priv->comp.config_ops = &chist_cfg_ops;
	priv->comp.hw_ops = &chist_hw_ops;
	priv->comp.debug_ops = &chist_debug_ops;

	dbg_probed_components[dbg_probed_count++] = priv;

	ret = mml_comp_add(priv->comp.id, dev, &mml_comp_ops);

	return ret;
}

static void remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mml_comp_ops);
	component_del(&pdev->dev, &mml_comp_ops);
}

const struct of_device_id mml_chist_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6993-mml1_chist0",
		.data = &mt6993_mmlf_chist_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, mml_chist_driver_dt_match);

struct platform_driver mml_chist_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mml-chist",
		.owner = THIS_MODULE,
		.of_match_table = mml_chist_driver_dt_match,
	},
};

//module_platform_driver(mml_chist_driver);

static s32 dbg_case;
static s32 dbg_set(const char *val, const struct kernel_param *kp)
{
	s32 result;

	result = kstrtos32(val, 0, &dbg_case);
	mml_log("%s: debug_case=%d", __func__, dbg_case);

	switch (dbg_case) {
	case 0:
		mml_log("use read to dump component status");
		break;
	default:
		mml_err("invalid debug_case: %d", dbg_case);
		break;
	}
	return result;
}

static s32 dbg_get(char *buf, const struct kernel_param *kp)
{
	s32 length = 0;
	u32 i;

	switch (dbg_case) {
	case 0:
		length += snprintf(buf + length, PAGE_SIZE - length,
			"[%d] probed count: %d\n", dbg_case, dbg_probed_count);
		for (i = 0; i < dbg_probed_count; i++) {
			struct mml_comp *comp = &dbg_probed_components[i]->comp;

			length += snprintf(buf + length, PAGE_SIZE - length,
				"  - [%d] mml comp_id: %d.%d @%pa name: %s bound: %d\n", i,
				comp->id, comp->sub_idx, &comp->base_pa,
				comp->name ? comp->name : "(null)", comp->bound);
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  -         larb_port: %d @%pa pw: %d clk: %d\n",
				comp->larb_port, &comp->larb_base,
				comp->pw_cnt, comp->clk_cnt);
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  -     ddp comp_id: %d bound: %d\n",
				dbg_probed_components[i]->ddp_comp.id,
				dbg_probed_components[i]->ddp_bound);
		}
		break;
	default:
		mml_err("not support read for debug_case: %d", dbg_case);
		break;
	}
	buf[length] = '\0';

	return length;
}

static const struct kernel_param_ops dbg_param_ops = {
	.set = dbg_set,
	.get = dbg_get,
};
module_param_cb(chist_debug, &dbg_param_ops, NULL, 0644);
MODULE_PARM_DESC(chist_debug, "mml chist debug case");

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML CHIST driver");
MODULE_LICENSE("GPL v2");
