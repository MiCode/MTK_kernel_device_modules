// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Dennis YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */

#include <linux/component.h>
#include <linux/dma-mapping.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/math64.h>
#include <soc/mediatek/smi.h>

#include "mtk-mml-driver.h"
#include "mtk-mml-tile.h"
#include "mtk-mml-sys.h"
#include "mtk-mml-mmp.h"
#include "mtk-mml-dle-adaptor.h"
#include "tile_driver.h"
#include "tile_mdp_func.h"

#include "mtk-mml-pq-core.h"

#undef pr_fmt
#define pr_fmt(fmt) "[mml_pq_c3d]" fmt

#define C3D_WAIT_TIMEOUT_MS 50
#define C3D_REG_NUM 60
#define REG_NOT_SUPPORT 0xfff
#define C3D_LABEL_COUNT 10

enum mml_color_reg_index {
	C3D_EN,
	C3D_CFG,
	C3D_RESET,
	C3D_INTEN,
	C3D_INTSTA,
	C3D_STATUS,
	C3D_INPUT_COUNT,
	C3D_OUTPUT_COUNT,
	C3D_CHKSUM,
	C3D_SIZE,
	C3D_DUMMY_REG,
	C3D_ATPG,
	C3D_SHADOW_CTRL,
	C3D_SRAM_CFG,
	C3D_SRAM_STATUS,
	C3D_SRAM_RW_IF_0,
	C3D_SRAM_RW_IF_1,
	C3D_SRAM_RW_IF_2,
	C3D_SRAM_RW_IF_3,
	C3D_SRAM_PINGPONG,
	C3D_LFSR_CFG,
	C3D_LFSR_SPEED,
	C3D_SRAM_STATUS_2,
	C3D_Y2R_09,
	C3D_R2Y_09,
	C3D_RIDX_00_01,
	C3D_RIDX_02_03,
	C3D_RIDX_04_05,
	C3D_RIDX_06_07,
	C3D_RIDX_08_09,
	C3D_RIDX_10_11,
	C3D_RIDX_12_13,
	C3D_RIDX_14_15,
	C3D_RIDX_16,
	C3D_GIDX_00_01,
	C3D_GIDX_02_03,
	C3D_GIDX_04_05,
	C3D_GIDX_06_07,
	C3D_GIDX_08_09,
	C3D_GIDX_10_11,
	C3D_GIDX_12_13,
	C3D_GIDX_14_15,
	C3D_GIDX_16,
	C3D_BIDX_00_01,
	C3D_BIDX_02_03,
	C3D_BIDX_04_05,
	C3D_BIDX_06_07,
	C3D_BIDX_08_09,
	C3D_BIDX_10_11,
	C3D_BIDX_12_13,
	C3D_BIDX_14_15,
	C3D_BIDX_16,
	C3D_REG_MAX_COUNT
};

/* C3D register offset */
static const u16 c3d_reg_table_mt6989[C3D_REG_MAX_COUNT] = {
	[C3D_EN] = 0x000,
	[C3D_CFG] = 0x004,
	[C3D_RESET] = 0x008,
	[C3D_INTEN] = 0x00c,
	[C3D_INTSTA] = 0x010,
	[C3D_STATUS] = 0x014,
	[C3D_INPUT_COUNT] = 0x018,
	[C3D_OUTPUT_COUNT] = 0x01c,
	[C3D_CHKSUM] = 0x020,
	[C3D_SIZE] = 0x024,
	[C3D_DUMMY_REG] = 0x028,
	[C3D_ATPG] = 0x02c,
	[C3D_SHADOW_CTRL] = 0x030,
	[C3D_SRAM_CFG] = 0x074,
	[C3D_SRAM_STATUS] = 0x078,
	[C3D_SRAM_RW_IF_0] = 0x07C,
	[C3D_SRAM_RW_IF_1] = 0x080,
	[C3D_SRAM_RW_IF_2] = 0x084,
	[C3D_SRAM_RW_IF_3] = 0x088,
	[C3D_SRAM_PINGPONG] = 0x08c,
	[C3D_LFSR_CFG] = 0x090,
	[C3D_LFSR_SPEED] = 0x094,
	[C3D_SRAM_STATUS_2] = 0x098,
	[C3D_Y2R_09] = 0x0E8,
	[C3D_R2Y_09] = 0x0C0,
};

static const u16 c3d_reg_table_mt6993[C3D_REG_MAX_COUNT] = {
	[C3D_EN] = 0x000,
	[C3D_CFG] = 0x004,
	[C3D_RESET] = 0x008,
	[C3D_INTEN] = 0x00c,
	[C3D_INTSTA] = 0x010,
	[C3D_STATUS] = 0x014,
	[C3D_INPUT_COUNT] = 0x018,
	[C3D_OUTPUT_COUNT] = 0x01c,
	[C3D_CHKSUM] = 0x020,
	[C3D_SIZE] = 0x024,
	[C3D_DUMMY_REG] = 0x028,
	[C3D_ATPG] = 0x02c,
	[C3D_SHADOW_CTRL] = 0x030,
	[C3D_SRAM_CFG] = 0x074,
	[C3D_SRAM_STATUS] = 0x078,
	[C3D_SRAM_RW_IF_0] = 0x07C,
	[C3D_SRAM_RW_IF_1] = 0x080,
	[C3D_SRAM_RW_IF_2] = 0x084,
	[C3D_SRAM_RW_IF_3] = 0x088,
	[C3D_SRAM_PINGPONG] = 0x08c,
	[C3D_LFSR_CFG] = 0x090,
	[C3D_LFSR_SPEED] = 0x094,
	[C3D_SRAM_STATUS_2] = 0x098,
	[C3D_Y2R_09] = 0x0E8,
	[C3D_R2Y_09] = 0x0C0,
	[C3D_RIDX_00_01] = 0x0F0,
	[C3D_RIDX_02_03] = 0x0F4,
	[C3D_RIDX_04_05] = 0x0F8,
	[C3D_RIDX_06_07] = 0x0FC,
	[C3D_RIDX_08_09] = 0x100,
	[C3D_RIDX_10_11] = 0x104,
	[C3D_RIDX_12_13] = 0x108,
	[C3D_RIDX_14_15] = 0x10C,
	[C3D_RIDX_16] = 0x110,
	[C3D_GIDX_00_01] = 0x114,
	[C3D_GIDX_02_03] = 0x118,
	[C3D_GIDX_04_05] = 0x11C,
	[C3D_GIDX_06_07] = 0x120,
	[C3D_GIDX_08_09] = 0x124,
	[C3D_GIDX_10_11] = 0x128,
	[C3D_GIDX_12_13] = 0x12C,
	[C3D_GIDX_14_15] = 0x130,
	[C3D_GIDX_16] = 0x134,
	[C3D_BIDX_00_01] = 0x138,
	[C3D_BIDX_02_03] = 0x13C,
	[C3D_BIDX_04_05] = 0x140,
	[C3D_BIDX_06_07] = 0x144,
	[C3D_BIDX_08_09] = 0x148,
	[C3D_BIDX_10_11] = 0x14C,
	[C3D_BIDX_12_13] = 0x150,
	[C3D_BIDX_14_15] = 0x154,
	[C3D_BIDX_16] = 0x158,
};

enum c3d_label_index {
	C3D_REUSE_LABEL = 0,
	C3D_POLLGPR_0 = C3D_LUT_NUM + C3D_PROG_IDX_REG_NUM,
	C3D_POLLGPR_1,
	C3D_LABEL_TOTAL
};

struct c3d_data {
	const u16 *reg_table;
	const u32 sram_start_addr;
	const u32 sram_end_addr;
	u16 gpr[MML_PIPE_CNT];
	u16 c3d_lut_num;
	bool sup_prog_idx;
};

static const struct c3d_data mt6989_c3d_data = {
	.reg_table = c3d_reg_table_mt6989,
	.sram_start_addr = 0,
	.sram_end_addr = 2912,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.c3d_lut_num = 729,
};

static const struct c3d_data mt6991_mmlt_c3d_data = {
	.reg_table = c3d_reg_table_mt6989,
	.sram_start_addr = 0,
	.sram_end_addr = 2912,
	.gpr = {CMDQ_GPR_R12, CMDQ_GPR_R14},
	.c3d_lut_num = 729,
};

static const struct c3d_data mt6993_mmlf_c3d_data = {
	.reg_table = c3d_reg_table_mt6993,
	.sram_start_addr = 0,
	.sram_end_addr = 2912,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.c3d_lut_num = 4913,
	.sup_prog_idx = true,
};

struct mml_comp_c3d {
	struct mml_comp comp;
	const struct c3d_data *data;
};

/* meta data for each different frame config */
struct c3d_frame_data {
	u16 labels[C3D_LABEL_TOTAL];
	struct mml_reuse_array reuse_lut;
	struct mml_reuse_offset offs_lut[C3D_LABEL_COUNT];
	bool config_success;
};

static inline struct c3d_frame_data *c3d_frm_data(struct mml_comp_config *ccfg)
{
	return ccfg->data;
}

static inline struct mml_comp_c3d *comp_to_c3d(struct mml_comp *comp)
{
	return container_of(comp, struct mml_comp_c3d, comp);
}

static s32 c3d_prepare(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg)
{
	struct c3d_frame_data *c3d_frm;

	c3d_frm = kzalloc(sizeof(*c3d_frm), GFP_KERNEL);
	if (unlikely(!c3d_frm))
		return -ENOMEM;

	c3d_frm->reuse_lut.offs = c3d_frm->offs_lut;
	c3d_frm->reuse_lut.offs_size = ARRAY_SIZE(c3d_frm->offs_lut);

	ccfg->data = c3d_frm;
	return 0;
}

static void c3d_relay(struct mml_comp *comp, struct cmdq_pkt *pkt,
	const phys_addr_t base_pa, bool relay, bool alpha)
{
	struct mml_comp_c3d *c3d = comp_to_c3d(comp);

	mml_pq_msg("%s relay[%d]", __func__, relay);
	if (relay)
		cmdq_pkt_write(pkt, NULL, base_pa + c3d->data->reg_table[C3D_CFG],
			0x227, U32_MAX);
	else {
		if (alpha)
			cmdq_pkt_write(pkt, NULL, base_pa + c3d->data->reg_table[C3D_CFG],
				(c3d->data->c3d_lut_num == 729)? 0x266 : 0x276, U32_MAX);
		else
			cmdq_pkt_write(pkt, NULL, base_pa + c3d->data->reg_table[C3D_CFG],
				(c3d->data->c3d_lut_num == 729)? 0x246 : 0x256, U32_MAX);
	}
}

s32 c3d_tile_prepare(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg,
	struct tile_func_block *func,
	union mml_tile_data *data)
{
	const struct mml_frame_config *cfg = task->config;
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	const struct mml_frame_size *frame_in = &cfg->frame_in;
	const struct mml_crop *crop = &cfg->frame_in_crop[ccfg->node->out_idx];

	func->enable_flag = dest->pq_config.en_c3d;

	if (cfg->info.dest_cnt == 1 &&
	    (crop->r.width != frame_in->width ||
	    crop->r.height != frame_in->height)) {
		func->full_size_x_in = cfg->frame_tile_sz.width + crop->r.left;
		func->full_size_y_in = cfg->frame_tile_sz.height + crop->r.top;
	} else {
		func->full_size_x_in = frame_in->width;
		func->full_size_y_in = frame_in->height;
	}
	func->full_size_x_out = func->full_size_x_in;
	func->full_size_y_out = func->full_size_y_in;
	return 0;
}

static const struct mml_comp_tile_ops c3d_tile_ops = {
	.prepare = c3d_tile_prepare,
};

static s32 c3d_buf_prepare(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	s32 ret = 0;

	if (!dest->pq_config.en_c3d)
		return ret;

	mml_pq_trace_ex_begin("mml_%s", __func__);
	ret = mml_pq_set_comp_config(task);
	mml_pq_trace_ex_end();
	return ret;
}

static u32 c3d_get_label_count(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];

	mml_pq_msg("%s pipe_id[%d] engine_id[%d] en_c3d[%d]", __func__,
		ccfg->pipe, comp->id, dest->pq_config.en_c3d);

	if (!dest->pq_config.en_c3d)
		return 0;

	return C3D_LABEL_TOTAL;
}

static void c3d_init(struct mml_comp *comp, struct cmdq_pkt *pkt, const phys_addr_t base_pa,
	bool shadow)
{
	struct mml_comp_c3d *c3d = comp_to_c3d(comp);

	/* power on */
	cmdq_pkt_write(pkt, NULL,
		base_pa + c3d->data->reg_table[C3D_EN], 0x1, U32_MAX);
	/* Enable shadow */
	cmdq_pkt_write(pkt, NULL,
		base_pa + c3d->data->reg_table[C3D_SHADOW_CTRL],
		(shadow ? 0 : 1) | 0x2, U32_MAX);
}

static s32 c3d_config_init(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg)
{
	c3d_init(comp, task->pkts[ccfg->pipe], comp->base_pa, task->config->shadow);
	return 0;
}

static struct mml_pq_comp_config_result *get_c3d_comp_config_result(
	struct mml_task *task)
{
	return task->pq_task->comp_config.result;
}

static u32 read_reg_value(struct mml_comp *comp, u16 reg)
{
	void __iomem *base = comp->base;

	if (reg == REG_NOT_SUPPORT) {
		mml_err("%s c3d reg is not support", __func__);
		return 0xFFFFFFFF;
	}

	return readl(base + reg);
}

static s32 c3d_config_frame(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_comp_c3d *c3d = comp_to_c3d(comp);
	struct c3d_frame_data *c3d_frm = c3d_frm_data(ccfg);
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	const phys_addr_t base_pa = comp->base_pa;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	struct mml_pq_comp_config_result *result = NULL;
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];
	struct mml_pq_reg *regs = NULL;
	u32 *c3d_lut = NULL, *c3d_prog_idx = NULL;
	u32 gpr = c3d->data->gpr[ccfg->pipe];
	s32 ret = 0;
	u32 addr = c3d->data->sram_start_addr;
	u32 i=0;
	u32 alpha = cfg->info.alpha ? 1 : 0;

	mml_pq_trace_ex_begin("%s %d", __func__, cfg->info.mode);
	c3d_relay(comp, pkt, base_pa, !dest->pq_config.en_c3d, alpha);
	if (!dest->pq_config.en_c3d)
		goto exit;

	do {
		ret = mml_pq_get_comp_config_result(task, C3D_WAIT_TIMEOUT_MS);
		if (ret) {
			mml_pq_comp_config_clear(task);
			c3d_frm->config_success = false;
			c3d_relay(comp, pkt, base_pa, true, alpha);
			mml_pq_err("get c3d param timeout: %d in %dms",
				ret, C3D_WAIT_TIMEOUT_MS);
			ret = -ETIMEDOUT;
			goto exit;
		}

		result = get_c3d_comp_config_result(task);
		if (!result) {
			c3d_frm->config_success = false;
			c3d_relay(comp, pkt, base_pa, true, alpha);
			mml_pq_err("%s: not get result from user lib", __func__);
			ret = -EBUSY;
			goto exit;
		}
	} while ((mml_pq_debug_mode & MML_PQ_SET_TEST) && result->is_set_test);

	regs = result->c3d_regs;
	c3d_lut = result->c3d_lut;
	c3d_prog_idx = result->c3d_prog_idx;

	cmdq_pkt_write(pkt, NULL, base_pa + c3d->data->reg_table[C3D_SRAM_CFG],
		(0 << 6)|(0 << 5)|(1 << 4), (0x7 << 4));
	cmdq_pkt_write(pkt, NULL,
		base_pa + c3d->data->reg_table[C3D_SRAM_RW_IF_0], addr, U32_MAX);
	cmdq_pkt_poll(pkt, NULL, (0x1 << 16),
		base_pa + c3d->data->reg_table[C3D_SRAM_STATUS], (0x1 << 16), gpr);
	for (i = 0, addr=0; i < result->c3d_lut_num; i++, addr+=4)
		mml_write(comp->id, pkt, base_pa + c3d->data->reg_table[C3D_SRAM_RW_IF_1],
			c3d_lut[i], U32_MAX, reuse, cache, &c3d_frm->labels[i]);

	if (c3d->data->sup_prog_idx) {
		mml_pq_msg("%s: c3d_prog_idx_reg_num = %d", __func__, result->c3d_prog_idx_reg_num);
		for (i = 0; i < result->c3d_prog_idx_reg_num; i++) {
			mml_write(comp->id, pkt, base_pa + c3d->data->reg_table[C3D_RIDX_00_01 + i],
				c3d_prog_idx[i], 0x03FF03FF, reuse, cache,
				&c3d_frm->labels[i + result->c3d_lut_num]);
			mml_pq_msg("%s: c3d_prog_idx[%d] = %d", __func__, i, c3d_prog_idx[i]);
		}
	}

	mml_pq_msg("%s:config c3d regs, count: %d", __func__, result->c3d_reg_cnt);
	c3d_frm->config_success = true;
	for (i = 0; i < result->c3d_reg_cnt; i++) {
		cmdq_pkt_write(pkt, NULL, base_pa + regs[i].offset,
			regs[i].value, regs[i].mask);
		mml_pq_msg("[C3D][config][%x] = %#x mask(%#x)",
			regs[i].offset, regs[i].value, regs[i].mask);
	}

	mml_pq_msg("%s: success ", __func__);
exit:
	mml_pq_trace_ex_end();
	return ret;
}

static s32 c3d_config_tile(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg, u32 idx)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	struct mml_comp_c3d *c3d = comp_to_c3d(comp);

	struct mml_tile_engine *tile = config_get_tile(cfg, ccfg, idx);

	u32 width = tile->in.xe - tile->in.xs + 1;
	u32 height = tile->in.ye - tile->in.ys + 1;

	mml_pq_msg("%s width[%d] height[%d] en_c3d[%d]", __func__,
		width, height, dest->pq_config.en_c3d);

	cmdq_pkt_write(pkt, NULL, base_pa + c3d->data->reg_table[C3D_SIZE],
		((width & 0xffff) << 16) | ((height & 0xffff)), U32_MAX);
	return 0;
}

static s32 c3d_config_post(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg)
{
	struct mml_frame_dest *dest = &task->config->info.dest[ccfg->node->out_idx];

	if (dest->pq_config.en_c3d)
		mml_pq_put_comp_config_result(task);
	return 0;
}

static s32 c3d_reconfig_frame(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	struct mml_pq_comp_config_result *result = NULL;
	struct c3d_frame_data *c3d_frm = c3d_frm_data(ccfg);
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	struct mml_comp_c3d *c3d = comp_to_c3d(comp);
	u32 *c3d_lut = NULL, *c3d_prog_idx = NULL;
	u32 i=0;
	s32 ret = 0;


	mml_pq_trace_ex_begin("%s %d", __func__, cfg->info.mode);
	if (!dest->pq_config.en_c3d || !c3d_frm->config_success)
		goto exit;

	do {
		if ((mml_pq_debug_mode & MML_PQ_FORCE_TIMEOUT_DBG) ||
			mml_pq_get_comp_config_result(task, C3D_WAIT_TIMEOUT_MS)) {
			mml_pq_comp_config_clear(task);
			ret = -ETIMEDOUT;
			mml_pq_err("%s: %s c3d param timeout: %d in %dms", __func__,
				(mml_pq_debug_mode & MML_PQ_FORCE_TIMEOUT_DBG) ? "simulate" : "get",
				ret, C3D_WAIT_TIMEOUT_MS);
			goto exit;
		}

		result = get_c3d_comp_config_result(task);
		if (!result || !c3d_frm->config_success || !result->c3d_reg_cnt) {
			mml_pq_err("%s: not get result from user lib", __func__);
			ret = -EBUSY;
			goto exit;
		}
	} while ((mml_pq_debug_mode & MML_PQ_SET_TEST) && result->is_set_test);

	c3d_lut = result->c3d_lut;
	c3d_prog_idx = result->c3d_prog_idx;
	for (i = 0 ; i < result->c3d_lut_num; i++)
		mml_update(comp->id, reuse, c3d_frm->labels[i], c3d_lut[i]);

	if (c3d->data->sup_prog_idx) {
		mml_pq_msg("%s: c3d_prog_idx_reg_num = %d", __func__, result->c3d_prog_idx_reg_num);
		for (i = 0; i < result->c3d_prog_idx_reg_num; i++) {
			mml_update(comp->id, reuse, c3d_frm->labels[i + result->c3d_lut_num],
				c3d_prog_idx[i]);
			mml_pq_msg("%s: c3d_prog_idx[%d] = %d", __func__, i, c3d_prog_idx[i]);
		}
	}

	mml_pq_msg("%s: success ", __func__);
exit:
	mml_pq_trace_ex_end();
	return ret;
}

static const struct mml_comp_config_ops c3d_cfg_ops = {
	.prepare = c3d_prepare,
	.buf_prepare = c3d_buf_prepare,
	.get_label_count = c3d_get_label_count,
	.init = c3d_config_init,
	.frame = c3d_config_frame,
	.tile = c3d_config_tile,
	.post = c3d_config_post,
	.reframe = c3d_reconfig_frame,
	.repost = c3d_config_post,
};

static void c3d_debug_dump(struct mml_comp *comp)
{
	struct mml_comp_c3d *c3d = comp_to_c3d(comp);
	void __iomem *base = comp->base;
	u32 value[25];
	u32 shadow_ctrl;

	mml_err("c3d component %u dump:", comp->id);

	/* Enable shadow read working */
	shadow_ctrl = read_reg_value(comp, c3d->data->reg_table[C3D_SHADOW_CTRL]);
	shadow_ctrl |= 0x4;
	writel(shadow_ctrl, base + c3d->data->reg_table[C3D_SHADOW_CTRL]);

	value[0] = readl(base + c3d->data->reg_table[C3D_CFG]);
	mml_err("C3D_CFG %#010x", value[0]);

	value[0] = read_reg_value(comp, c3d->data->reg_table[C3D_EN]);
	value[1] = read_reg_value(comp, c3d->data->reg_table[C3D_CFG]);
	value[2] = read_reg_value(comp, c3d->data->reg_table[C3D_RESET]);
	value[3] = read_reg_value(comp, c3d->data->reg_table[C3D_INTEN]);
	value[4] = read_reg_value(comp, c3d->data->reg_table[C3D_INTSTA]);
	value[5] = read_reg_value(comp, c3d->data->reg_table[C3D_STATUS]);
	value[6] = read_reg_value(comp, c3d->data->reg_table[C3D_INPUT_COUNT]);
	value[7] = read_reg_value(comp, c3d->data->reg_table[C3D_OUTPUT_COUNT]);
	value[8] = read_reg_value(comp, c3d->data->reg_table[C3D_CHKSUM]);
	value[9] = read_reg_value(comp, c3d->data->reg_table[C3D_SIZE]);
	value[10] = read_reg_value(comp, c3d->data->reg_table[C3D_DUMMY_REG]);
	value[11] = read_reg_value(comp, c3d->data->reg_table[C3D_ATPG]);
	value[12] = read_reg_value(comp, c3d->data->reg_table[C3D_SHADOW_CTRL]);
	value[13] = read_reg_value(comp, c3d->data->reg_table[C3D_SRAM_CFG]);
	value[14] = read_reg_value(comp, c3d->data->reg_table[C3D_SRAM_STATUS]);
	value[15] = read_reg_value(comp, c3d->data->reg_table[C3D_SRAM_RW_IF_0]);
	value[16] = read_reg_value(comp, c3d->data->reg_table[C3D_SRAM_RW_IF_1]);
	value[17] = read_reg_value(comp, c3d->data->reg_table[C3D_SRAM_RW_IF_2]);
	value[18] = read_reg_value(comp, c3d->data->reg_table[C3D_SRAM_RW_IF_3]);
	value[19] = read_reg_value(comp, c3d->data->reg_table[C3D_SRAM_PINGPONG]);
	value[20] = read_reg_value(comp, c3d->data->reg_table[C3D_LFSR_CFG]);
	value[21] = read_reg_value(comp, c3d->data->reg_table[C3D_LFSR_SPEED]);
	value[22] = read_reg_value(comp, c3d->data->reg_table[C3D_SRAM_STATUS_2]);
	value[23] = read_reg_value(comp, c3d->data->reg_table[C3D_Y2R_09]);
	value[24] = read_reg_value(comp, c3d->data->reg_table[C3D_R2Y_09]);

	mml_err("C3D_EN %#010x C3D_CFG %#010x C3D_RESET %#010x C3D_INTEN %#010x C3D_INTSTA %#010x",
		value[0], value[1], value[2], value[3], value[4]);
	mml_err("C3D_STATUS %#010x C3D_INPUT_COUNT %#010x C3D_OUTPUT_COUNT %#010x",
		value[5], value[6], value[7]);
	mml_err("C3D_CHKSUM %#010x C3D_SIZE %#010x C3D_DUMMY_REG %#010x C3D_ATPG %#010x",
		value[8], value[9], value[10], value[11]);
	mml_err("C3D_SHADOW_CTRL %#010x C3D_SRAM_CFG %#010x C3D_SRAM_STATUS %#010x",
		value[12], value[13], value[14]);
	mml_err("C3D_SRAM_RW_IF_0 %#010x C3D_SRAM_RW_IF_1 %#010x C3D_SRAM_RW_IF_2 %#010x",
		value[15], value[16], value[17]);
	mml_err("C3D_SRAM_RW_IF_3 %#010x C3D_SRAM_PINGPONG %#010x C3D_LFSR_CFG %#010x",
		value[18], value[19], value[20]);
	mml_err("C3D_LFSR_SPEED %#010x C3D_SRAM_STATUS_2 %#010x C3D_Y2R_09 %#010x",
		value[21], value[22], value[23]);
	mml_err("C3D_R2Y_09 %#010x", value[24]);
}

static void c3d_reset(struct mml_comp *comp, struct mml_frame_config *cfg, u32 pipe)
{
}

static const struct mml_comp_debug_ops c3d_debug_ops = {
	.dump = &c3d_debug_dump,
	.reset = &c3d_reset,
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_c3d *c3d = dev_get_drvdata(dev);
	s32 ret;

	ret = mml_register_comp(master, &c3d->comp);
	if (ret)
		dev_err(dev, "Failed to register mml component %s: %d\n",
			dev->of_node->full_name, ret);
	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_c3d *c3d = dev_get_drvdata(dev);

	mml_unregister_comp(master, &c3d->comp);
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static struct mml_comp_c3d *dbg_probed_components[4];
static int dbg_probed_count;

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_comp_c3d *priv;
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

	/* assign ops */
	priv->comp.tile_ops = &c3d_tile_ops;
	priv->comp.config_ops = &c3d_cfg_ops;
	priv->comp.debug_ops = &c3d_debug_ops;

	dbg_probed_components[dbg_probed_count++] = priv;

	mml_comp_add(priv->comp.id, dev, &mml_comp_ops);

	return 0;
}

static void remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mml_comp_ops);
}

const struct of_device_id mml_c3d_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6989-mml_c3d",
		.data = &mt6989_c3d_data,
	},
	{
		.compatible = "mediatek,mt6991-mml1_c3d0",
		.data = &mt6989_c3d_data,
	},
	{
		.compatible = "mediatek,mt6991-mml0_c3d0",
		.data = &mt6991_mmlt_c3d_data,
	},
	{
		.compatible = "mediatek,mt6899-mml1_c3d0",
		.data = &mt6989_c3d_data,
	},
	{
		.compatible = "mediatek,mt6899-mml0_c3d0",
		.data = &mt6991_mmlt_c3d_data,
	},
	{
		.compatible = "mediatek,mt6993-mml1_c3d0",
		.data = &mt6993_mmlf_c3d_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, mml_c3d_driver_dt_match);

struct platform_driver mml_c3d_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mml-c3d",
		.owner = THIS_MODULE,
		.of_match_table = mml_c3d_driver_dt_match,
	},
};

//module_platform_driver(mml_c3d_driver);

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML C3D driver");
MODULE_LICENSE("GPL");
