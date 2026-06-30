// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/sched/clock.h>

#include "mdw.h"
#include "mdw_plat.h"
#include "mdw_cmn.h"
#include "mdw_rv.h"
#include "mdw_mem_pool.h"
#include "mdw_cmd.h"
#include "mdw_cb_appendix.h"
#include "mdw_sanity.h"

/* exit id support */
#include "mdw_ext.h"
/* power budget support */
#include "mdw_pb.h"
/* cmd history support */
#include "mdw_ch.h"

/* rv msg cmd v4 */
struct mdw_rv_msg_cmd {
	/* ids */
	uint64_t session_id;
	uint64_t cmd_id;
	uint64_t inference_id;
	uint32_t pid;
	uint32_t tgid;
	/* params */
	uint32_t priority;
	uint32_t hardlimit;
	uint32_t softlimit;
	uint32_t fastmem_ms;
	uint32_t power_plcy;
	uint32_t power_dtime;
	uint32_t power_etime;
	uint32_t app_type;

	uint32_t num_subcmds;
	uint32_t subcmds_offset;
	uint32_t num_cmdbufs;
	uint32_t cmdbuf_infos_offset;

	uint32_t apummu_tbl_infos_offset;
	uint32_t adj_matrix_offset;
	uint32_t exec_infos_offset;
	uint32_t num_links;
	uint32_t link_offset;

	/* history params */
	uint32_t inference_ms;
	uint32_t tolerance_ms;
	/* cmd done */
	uint32_t cmd_done;
} __packed;

struct mdw_rv_msg_sc {
	/* params */
	uint32_t type;
	uint32_t suggest_time;
	uint32_t vlm_usage;
	uint32_t vlm_ctx_id;
	uint32_t vlm_force;
	uint32_t boost;
	uint32_t turbo_boost;
	uint32_t min_boost;
	uint32_t max_boost;
	uint32_t hse_en;
	uint32_t driver_time;
	uint32_t ip_time;
	uint32_t bw;
	uint32_t pack_id;
	uint32_t affinity;
	uint32_t trigger_type;
	/* cmdbufs info */
	uint32_t cmdbuf_start_idx;
	uint32_t num_cmdbufs;
	/* history info */
	uint32_t history_ip_time;
} __packed;

struct mdw_rv_msg_cb {
	uint64_t device_va;
	uint32_t size;
} __packed;

struct mdw_rv_msg_ammu {
	uint32_t cb_head_device_va;  /* cb.dva, for rv debug only */
	uint32_t table_head_device_va; /* cb.dva + offset */
	uint32_t table_size; /* query apummu appendix cb size */
} __packed;

struct mdw_rv_sc_link {
	uint32_t producer_idx;
	uint32_t consumer_idx;
	uint32_t vid;
	uint64_t va;
	uint64_t x;
	uint64_t y;
} __packed;

/****************************** internal functions ******************************/

/* create rv cb for cmd, return rv cb */
static struct mdw_mem_map *mdw_plat_v4_create_msg(struct mdw_cmd *c)
{
	struct mdw_mem_map *rv_cb = NULL;
	struct mdw_fpriv *mpriv = c->mpriv;
	uint64_t cb_size = 0;
	uint32_t acc_cb = 0, i = 0, j = 0, appendix_num = 0;
	uint32_t subcmds_ofs = 0, cmdbuf_infos_ofs = 0, exec_infos_ofs = 0;
	uint32_t adj_matrix_ofs = 0, apummu_tbl_infos_ofs = 0, link_ofs = 0;
	uint32_t appendix_cmdbufs_ofs = 0, apummu_table_size = 0;
	struct mdw_rv_msg_cmd *rmc = NULL;
	struct mdw_rv_msg_sc *rmsc = NULL;
	struct mdw_rv_msg_cb *rmcb = NULL;
	struct mdw_rv_sc_link *rl = NULL;
	struct mdw_rv_msg_ammu *rmammu = NULL;
	struct apusys_cmd_info cmd_info;

	mdw_trace_begin("apumdw:rv_cmd_create");

	/* get appendix num */
	appendix_num = mdw_cb_appendix_num();

	/* check mem address for rv */
	if (MDW_IS_HIGHADDR(c->exec_infos->device_va) ||
		MDW_IS_HIGHADDR(c->cmdbufs->device_va)) {
		mdw_drv_err("rv dva high addr(0x%llx/0x%llx)\n",
			c->cmdbufs->device_va, c->exec_infos->device_va);
		goto out;
	}

	/* calc size and offset */
	/* 1. rv_msg_cmd */
	if (check_add_overflow(cb_size, sizeof(struct mdw_rv_msg_cmd), &cb_size))
		goto cb_overflow;
	cb_size = MDW_ALIGN(cb_size, MDW_DEFAULT_ALIGN);

	/* 2. adj_matrix_ofs and size */
	adj_matrix_ofs = cb_size;
	if (check_add_overflow(cb_size, (c->num_subcmds * c->num_subcmds * sizeof(uint8_t)), &cb_size))
		goto cb_overflow;
	cb_size = MDW_ALIGN(cb_size, MDW_DEFAULT_ALIGN);

	/* 3. subcmds_ofs, mdw_rv_msg_sc[] */
	subcmds_ofs = cb_size;
	if (check_add_overflow(cb_size, (c->num_subcmds * sizeof(struct mdw_rv_msg_sc)), &cb_size))
		goto cb_overflow;
	cb_size = MDW_ALIGN(cb_size, MDW_DEFAULT_ALIGN);

	/* 4. cmdbuf_infos_ofs, mdw_rv_msg_cb[] */
	cmdbuf_infos_ofs = cb_size;
	if (check_add_overflow(cb_size, (c->num_cmdbufs * sizeof(struct mdw_rv_msg_cb)), &cb_size))
		goto cb_overflow;
	cb_size = MDW_ALIGN(cb_size, MDW_DEFAULT_ALIGN);

	/* 5. apummu_tbl_infos_ofs for mdw_rv_msg_ammu, info for apummu table appendix cb */
	apummu_tbl_infos_ofs = cb_size;
	if (check_add_overflow(cb_size, sizeof(struct mdw_rv_msg_ammu), &cb_size))
		goto cb_overflow;
	cb_size = MDW_ALIGN(cb_size, MDW_DEFAULT_ALIGN);

	/* 6. exec infos */
	exec_infos_ofs = cb_size;
	if (check_add_overflow(cb_size, c->exec_infos->size, &cb_size))
		goto cb_overflow;
	cb_size = MDW_ALIGN(cb_size, MDW_DEFAULT_ALIGN);

	/* 7. one appendix cb for apummu table */
	i = 0;
	appendix_cmdbufs_ofs = cb_size;
	apummu_table_size = mdw_cb_appendix_size_by_idx(i, c->num_subcmds);
	mdw_cmd_debug("appendix-#%u for apummu offset(%llu) table_size(%u)\n", i,
		cb_size, apummu_table_size);
	if (check_add_overflow(cb_size, apummu_table_size, &cb_size))
		goto cb_overflow;
	cb_size = MDW_ALIGN(cb_size, MDW_DEFAULT_ALIGN);

	/* 8. (optional) link_ofs, mdw_rv_sc_link[] */
	if (c->num_links) {
		link_ofs = cb_size;
		if (check_add_overflow(cb_size, c->num_links * sizeof(struct mdw_rv_sc_link), &cb_size))
			goto cb_overflow;
		cb_size = MDW_ALIGN(cb_size, MDW_DEFAULT_ALIGN);

		// if calc links successfully, dump for debug
		mdw_cmd_debug("num_links(%u) link_ofs(%u) size(%lu/%llu)",
			c->num_links, link_ofs, c->num_links * sizeof(struct mdw_rv_sc_link), cb_size);
	}


	/* allocate communicate buffer */
	rv_cb = mdw_mem_pool_alloc(&mpriv->cmd_buf_pool, cb_size,
		MDW_DEFAULT_ALIGN);
	if (!rv_cb) {
		mdw_drv_err("c(0x%llx) alloc cb size(%llu) fail\n",
			c->kid, cb_size);
		goto out;
	}

	/* assign cmd info */
	rmc = (struct mdw_rv_msg_cmd *)rv_cb->vaddr;
	rmc->session_id = c->mpriv->id;
	rmc->cmd_id = c->kid;
	rmc->pid = (uint32_t)c->pid;
	rmc->tgid = (uint32_t)c->tgid;
	rmc->priority = c->priority;
	rmc->hardlimit = c->hardlimit;
	rmc->softlimit = c->softlimit;
	rmc->fastmem_ms = c->fastmem_ms;
	rmc->power_plcy = c->power_plcy;
	rmc->power_dtime = c->power_dtime;
	rmc->power_etime = c->power_etime;
	rmc->app_type = c->app_type;
	rmc->num_subcmds = c->num_subcmds;
	rmc->num_cmdbufs = c->num_cmdbufs;
	rmc->subcmds_offset = subcmds_ofs;
	rmc->cmdbuf_infos_offset = cmdbuf_infos_ofs;
	rmc->apummu_tbl_infos_offset = apummu_tbl_infos_ofs; /* APUMMU Table offest */
	rmc->adj_matrix_offset = adj_matrix_ofs;
	rmc->exec_infos_offset = exec_infos_ofs;
	rmc->num_links = c->num_links;
	rmc->link_offset = link_ofs;
	rmc->inference_ms = c->inference_ms;
	rmc->tolerance_ms = c->tolerance_ms;
	rmc->inference_id = c->inference_id;

	/* copy links */
	rl = (void *)rmc + rmc->link_offset;
	for (i = 0; i < c->num_links; i++) {
		rl[i].producer_idx = c->links[i].producer_idx;
		rl[i].consumer_idx = c->links[i].consumer_idx;
		rl[i].vid = c->links[i].vid;
		rl[i].va = c->links[i].va;
		rl[i].x = c->links[i].x;
		rl[i].y = c->links[i].y;
	}

	/* assign subcmds info */
	rmsc = (void *)rmc + rmc->subcmds_offset;
	rmcb = (void *)rmc + rmc->cmdbuf_infos_offset;
	for (i = 0; i < c->num_subcmds; i++) {
		rmsc[i].type = c->subcmds[i].type;
		rmsc[i].suggest_time = c->subcmds[i].suggest_time;
		rmsc[i].vlm_usage = c->subcmds[i].vlm_usage;
		rmsc[i].vlm_ctx_id = c->subcmds[i].vlm_ctx_id;
		rmsc[i].vlm_force = c->subcmds[i].vlm_force;
		rmsc[i].boost = c->subcmds[i].boost;
		rmsc[i].ip_time = c->subcmds[i].ip_time;
		rmsc[i].driver_time = c->subcmds[i].driver_time;
		rmsc[i].bw = c->subcmds[i].bw;
		rmsc[i].turbo_boost = c->subcmds[i].turbo_boost;
		rmsc[i].min_boost = c->subcmds[i].min_boost;
		rmsc[i].max_boost = c->subcmds[i].max_boost;
		rmsc[i].hse_en = c->subcmds[i].hse_en;
		rmsc[i].pack_id = c->subcmds[i].pack_id;
		rmsc[i].affinity = c->subcmds[i].affinity;
		rmsc[i].num_cmdbufs = c->subcmds[i].num_cmdbufs;
		rmsc[i].cmdbuf_start_idx = acc_cb;
		rmsc[i].trigger_type = c->subcmds[i].trigger_type;

		/* assign cmd buf of subcmd */
		for (j = 0; j < rmsc[i].num_cmdbufs; j++) {
			rmcb[acc_cb + j].size =
				c->ksubcmds[i].cmdbufs[j].size;
			rmcb[acc_cb + j].device_va =
				c->ksubcmds[i].daddrs[j];
			mdw_cmd_debug("sc(%u) #%u-cmdbufs 0x%llx/%u\n",
				i, j,
				rmcb[acc_cb + j].device_va,
				rmcb[acc_cb + j].size);
		}
		acc_cb += c->subcmds[i].num_cmdbufs;
	}

	/* copy adj matrix */
	memcpy((void *)rmc + rmc->adj_matrix_offset, c->adj_matrix,
		c->num_subcmds * c->num_subcmds * sizeof(uint8_t));

	/* assign apummu info */
	rmammu = (void *)rmc + rmc->apummu_tbl_infos_offset;
	rmammu->cb_head_device_va = rv_cb->device_va;
	rmammu->table_head_device_va = rv_cb->device_va + appendix_cmdbufs_ofs;  // uint32_t + uint32_t
	rmammu->table_size = apummu_table_size;

	mdw_cmd_debug("rmammu(0x%x/0x%x) size(%u)\n",
		rmammu->cb_head_device_va, rmammu->table_head_device_va, rmammu->table_size);

	/* assign cmd info for appendix cmdbuf */
	cmd_info.session = (uint64_t)c->mpriv;
	cmd_info.session_id = c->mpriv->id;
	cmd_info.cmd_uid = c->uid;
	cmd_info.num_subcmds = c->num_subcmds;
	cmd_info.power_plcy = c->power_plcy;

	/* assign only one appendix cmdbuf for apummu */
	i = 0;
	if (mdw_cb_appendix_process(APU_APPENDIX_CB_CREATE, i, &cmd_info,
		(void *)rmc + appendix_cmdbufs_ofs, apummu_table_size))
		mdw_drv_err("apummu appendix(%u) process(%d) failed\n", i, APU_APPENDIX_CB_CREATE);

	goto out;

cb_overflow:
	mdw_drv_err("cb_size overflow(%llu)\n", cb_size);
	rv_cb = NULL;
out:
	mdw_trace_end();
	mdw_cmd_debug("\n");

	return rv_cb;
}
static void mdw_plat_v4_delete_msg(struct mdw_mem_map *rv_cb)
{
	mdw_cmd_debug("\n");
	mdw_mem_pool_free(rv_cb);
}
static void mdw_plat_v4_show_msg(struct mdw_mem_map *rv_cb)
{
	struct mdw_rv_msg_cmd *rmc = (struct mdw_rv_msg_cmd *)rv_cb->vaddr;
	struct mdw_rv_msg_sc *rmsc = NULL;
	struct mdw_rv_msg_cb *rmcb = NULL;
	struct mdw_rv_sc_link *rl = NULL;
	struct mdw_rv_msg_ammu *rmammu = NULL;
	uint32_t i = 0;

	/* show rv cmd info */
	mdw_cmd_debug("session_id(0x%llx) cmd_id(0x%llx) inference_id(0x%llx) pid(%u) tgid(%u)\n",
		rmc->session_id, rmc->cmd_id, rmc->inference_id, rmc->pid, rmc->tgid);
	mdw_cmd_debug("  num_subcmds(%u) subcmds_offset(%u) num_cmdbufs(%u) cmdbuf_infos_offset(%u)\n",
		rmc->num_subcmds, rmc->subcmds_offset, rmc->num_cmdbufs, rmc->cmdbuf_infos_offset);
	mdw_cmd_debug("  apummu_tbl_infos_offset(%u) adj_matrix_offset(%u) exec_infos_offset(%u)\n",
		rmc->apummu_tbl_infos_offset, rmc->adj_matrix_offset, rmc->exec_infos_offset);
	mdw_cmd_debug("  num_links(%u) link_offset(%u)\n",
		rmc->num_links, rmc->link_offset);
	mdw_cmd_debug("  priority(%u) hardlimit(%u) softlimit(%u) fastms(%u) pwr_plcy(%u) dtime(%u) etime(%u)\n",
		rmc->priority, rmc->hardlimit, rmc->softlimit, rmc->fastmem_ms,
		rmc->power_plcy, rmc->power_dtime, rmc->power_etime);
	mdw_cmd_debug("  inference_m(%u), tolerance_ms(%u) cmd_done(0x%x)\n",
		rmc->inference_ms, rmc->tolerance_ms, rmc->cmd_done);

	/* show all cmdbuf info */
	rmcb = (struct mdw_rv_msg_cb *)(rv_cb->vaddr + rmc->cmdbuf_infos_offset);
	mdw_cmd_debug(" cmdbufs:\n");
	for (i = 0; i < rmc->num_cmdbufs; i++) {
		mdw_cmd_debug("    cmdbufs[%u](0x%llx/%u)\n", i, rmcb->device_va, rmcb->size);
		rmcb++;
	}

	/* show apummu table info */
	rmammu = (struct mdw_rv_msg_ammu *)(rv_cb->vaddr + rmc->apummu_tbl_infos_offset);
	mdw_cmd_debug("  apummu table info(0x%x/0x%x) size(%u)\n",
		rmammu->cb_head_device_va, rmammu->table_head_device_va, rmammu->table_size);

	/* show adj matrix info */
	if (mdw_debug_on(MDW_DBG_CMD)) {
		print_hex_dump(KERN_INFO, "  adj_matrix: ",
			DUMP_PREFIX_OFFSET, 16, 4, (rv_cb->vaddr + rmc->adj_matrix_offset),
			(rmc->num_subcmds * rmc->num_subcmds * sizeof(uint8_t)), 0);
	}

	/* show all subcmd info */
	rmsc = (struct mdw_rv_msg_sc *)(rv_cb->vaddr + rmc->subcmds_offset);
	for (i = 0; i < rmc->num_subcmds; i++) {
		mdw_cmd_debug("  subcmd[%u]:\n", i);
		mdw_cmd_debug("    type(%u) vlm(%u/%u) boost(%u/%u) hse_en(%u) affinity(0x%x) trigger(%u)\n",
			rmsc->type, rmsc->vlm_ctx_id, rmsc->vlm_usage, rmsc->boost, rmsc->turbo_boost,
			rmsc->hse_en, rmsc->affinity, rmsc->trigger_type);
		mdw_cmd_debug("    suggest_time(%u) min_boost(%u) max_boost(%u) pack_id(%u) affinity(%u)\n",
			rmsc->suggest_time, rmsc->min_boost, rmsc->max_boost,
			rmsc->pack_id, rmsc->affinity
		);
		mdw_cmd_debug("    driver_time(%u) ip_time(%u)\n", rmsc->driver_time, rmsc->ip_time);

		mdw_cmd_debug("    cmdbufs_info(%u/%u) history_ip_time(%u)\n",
			rmsc->cmdbuf_start_idx, rmsc->num_cmdbufs, rmsc->history_ip_time);
		rmsc++;
	}

	/* show links info */
	rl = (struct mdw_rv_sc_link *)(rv_cb->vaddr + rmc->link_offset);
	for (i = 0; i < rmc->num_links; i++) {
		mdw_cmd_debug("  link[%u]:\n", i);
		mdw_cmd_debug("    producer_idx(%u) consumer_idx(%u) vid(%u)\n",
			rl->producer_idx, rl->consumer_idx, rl->vid);
		mdw_cmd_debug("    va(%llx) x(%llx) y(%llx)\n",
			rl->va, rl->x, rl->y);
	}
}

static void mdw_plat_v4_appendix_process(struct mdw_cmd *c, enum apu_appendix_cb_type type)
{
	struct apusys_cmd_info cmd_info;
	struct mdw_rv_cmd *rv_cmd = (struct mdw_rv_cmd *)c->plat_priv;
	struct mdw_rv_msg_cmd *rmc = NULL;
	struct mdw_rv_msg_ammu *rmammu = NULL;
	uint32_t i = 0, appendix_num = 0, appendix_cmdbufs_ofs = 0;

	/* check appendix num */
	appendix_num = mdw_cb_appendix_num();

	mdw_flw_debug("appendix(%u) process(%d)\n", i, type);

	/* calc appendix cb offset */
	rmc = (struct mdw_rv_msg_cmd *)rv_cmd->cb->vaddr;
	rmammu = (void *)rmc + rmc->apummu_tbl_infos_offset;
	appendix_cmdbufs_ofs = (rmammu->table_head_device_va - rmammu->cb_head_device_va);

	/* assign cmd info */
	cmd_info.session_id = c->mpriv->id;
	cmd_info.cmd_uid = c->uid;
	cmd_info.num_subcmds = c->num_subcmds;
	cmd_info.power_plcy = c->power_plcy;

	/* call appendix process for apummu */
	i = 0;
	if (mdw_cb_appendix_process(type, i, &cmd_info,
		(void *)rmc + appendix_cmdbufs_ofs, rmammu->table_size)) {
		mdw_drv_err("apummu appendix(%u) process(%d) failed\n", i, type);
	}
}

static void mdw_plat_v4_reset_info(struct mdw_rv_msg_cmd *rmc, struct mdw_cmd *c,
				 struct mdw_mem_map *rv_cb)
{
	struct mdw_rv_cmd *rc = (struct mdw_rv_cmd *)c->plat_priv;

	/* clear einfos */
	memset((void *)rmc + rmc->exec_infos_offset, 0, c->exec_infos->size);
	/* clear exec ret */
	c->einfos->c.ret = 0;
	c->einfos->c.sc_rets = 0;
	rmc->cmd_done = 0;

	/* update inference id */
	rmc->inference_id = c->inference_id;

	if (mdw_mem_flush(c->mpriv, rc->cb))
		mdw_drv_warn("s(0x%llx) c(0x%llx/0x%llx) flush rv cbs(%llu) fail\n",
			c->mpriv->id, c->kid, c->inference_id, rc->cb->size);
}

/* v4 subcmd sanity check */
static int mdw_plat_v4_sc_sanity_check(struct mdw_cmd *c)
{
	unsigned int i = 0;

	/* subcmd info */
	for (i = 0; i < c->num_subcmds; i++) {
		if (c->subcmds[i].type >= MDW_DEV_MAX ||
			c->subcmds[i].vlm_ctx_id >= MDW_SUBCMD_MAX ||
			c->subcmds[i].boost > MDW_BOOST_MAX ||
			c->subcmds[i].pack_id >= MDW_SUBCMD_MAX) {
			mdw_drv_err("subcmd(%u) invalid (%u/%u/%u)\n",
				i, c->subcmds[i].type,
				c->subcmds[i].boost,
				c->subcmds[i].pack_id);
			return -EINVAL;
		}
	}

	return 0;
}

/****************************** interfaces for mdw_plat_func_v4 *******************************/

static int mdw_plat_v4_late_init(struct mdw_device *mdev)
{
	int ret = 0;

	mdw_drv_debug("\n");

	/* assign library version */
	mdev->mdw_ver = 4;
	mdw_drv_info("mdw_ver = %u\n", mdev->mdw_ver);

	/* support power budget */
	ret = mdw_pb_init(mdev);
	if (ret) {
		mdw_drv_err("mdw power budget init failed\n");
		goto out;
	}

	/* support cmd history */
	ret = mdw_ch_init(mdev);
	if (ret) {
		mdw_drv_err("mdw cmd history init failed\n");
		goto deinit_pb;
	}

	/* support rv execution */
	ret = mdw_rv_late_init(mdev);
	if (ret) {
		mdw_drv_err("mdw rv late init failed\n");
		goto deinit_ch;
	}

	goto out;

deinit_ch:
	mdw_ch_deinit(mdev);
deinit_pb:
	mdw_pb_deinit(mdev);
out:
	return ret;
}

static void mdw_plat_v4_late_deinit(struct mdw_device *mdev)
{
	mdw_drv_debug("\n");

	mdw_rv_late_deinit(mdev);
	mdw_ch_deinit(mdev);
	mdw_pb_deinit(mdev);
}

static int mdw_plat_v4_register_device(struct apusys_device *adev)
{
	mdw_drv_debug("\n");
	return 0;
}

static int mdw_plat_v4_unregister_device(struct apusys_device *adev)
{
	mdw_drv_debug("\n");
	return 0;
}

static int mdw_plat_v4_create_session(struct mdw_fpriv *mpriv)
{
	int ret = 0;

	mdw_drv_debug("\n");
	mdw_pb_get(MDW_POWERPOLICY_DEFAULT, MDW_PB_DEBOUNCE_MS);

	ret = mdw_ch_session_create(mpriv);  // empty right now
	if (ret)
		mdw_drv_err("create cmd history session fail(%d)\n", ret);

	return ret;
}

static int mdw_plat_v4_delete_session(struct mdw_fpriv *mpriv)
{
	mdw_drv_debug("\n");
	mdw_ch_session_delete(mpriv);
	return 0;
}

static int mdw_plat_v4_create_cmd_priv(struct mdw_cmd *c)
{
	int ret = 0;
	struct mdw_rv_cmd *rc = NULL;

	/* check arguments */
	if (!IS_ERR_OR_NULL(c->plat_priv)) {
		mdw_drv_err("cmd priv not empty\n");
		ret = -EINVAL;
		goto out;
	}

	/* create rv cmd for rv trigger */
	rc = devm_kzalloc(c->mpriv->dev, sizeof(*rc), GFP_KERNEL);
	if (IS_ERR_OR_NULL(rc)) {
		mdw_drv_err("alloc plat cmd priv fail\n");
		ret = -ENOMEM;
		goto out;
	}

	/* create rv communicate buffer */
	rc->cb = mdw_plat_v4_create_msg(c);
	if (!rc->cb) {
		mdw_drv_err("create cmd msg fail\n");
		ret = -ENOMEM;
		goto free_cp_priv;
	}

	/* create cmd history */
	ret = mdw_ch_cmd_create_tbl(c);
	if (ret) {
		mdw_drv_err("create cmd history fail\n");
		goto delete_msg;
	}

	rc->c = c;

	mdw_drv_debug("\n");
	c->plat_priv = rc;
	c->rvid = (uint64_t)&rc->s_msg;

	/* get ext id */
	mdw_ext_cmd_get_id(c);

	goto out;

delete_msg:
	mdw_plat_v4_delete_msg(rc->cb);
free_cp_priv:
	devm_kfree(c->mpriv->dev, rc);
out:
	return ret;
}

static int mdw_plat_v4_delete_cmd_priv(struct mdw_cmd *c)
{
	int ret = 0;
	struct mdw_rv_cmd *rc = NULL;

	if (IS_ERR_OR_NULL(c->plat_priv)) {
		mdw_drv_err("cmd priv is already free\n");
		return -EINVAL;
	}

	/* get ext id */
	mdw_ext_cmd_put_id(c);

	rc = c->plat_priv;
	if (rc->cb == NULL) {
		mdw_drv_err("rv cb is NULL\n");
		ret = -ENOMSG;
	} else {
		mdw_plat_v4_delete_msg(rc->cb);
	}

	devm_kfree(c->mpriv->dev, c->plat_priv);
	c->plat_priv = NULL;

	mdw_drv_debug("\n");

	return ret;
}

/* called before cmd run */
static int mdw_plat_v4_preprocess_cmd(struct mdw_cmd *c)
{
	struct mdw_rv_cmd *rc = (struct mdw_rv_cmd *)c->plat_priv;

	mdw_drv_debug("msg(%pK) sync id (%lld)\n", &rc->s_msg.msg, rc->s_msg.msg.sync_id);
	/* get power budget */
	mdw_pb_get(c->power_plcy, 0);
	rc->start_ts_ns = c->start_ts;
	atomic_inc(&c->mpriv->mdev->cmd_running);

	return 0;
}

static int mdw_plat_v4_run_cmd(struct mdw_cmd *c)
{
	int ret = 0;
	struct mdw_rv_cmd *rc = (struct mdw_rv_cmd *)c->plat_priv;
	struct mdw_rv_msg_cmd *rmc = NULL;

	if (rc == NULL || rc->cb == NULL || rc->cb->vaddr == NULL)
		return -EINVAL;

	/* get rv msg cmd */
	rmc = (struct mdw_rv_msg_cmd *)rc->cb->vaddr;

	/* clear execute info and update inf_id */
	mdw_plat_v4_reset_info(rmc, c, rc->cb);

	/* dump for debug */
	mdw_plat_v4_show_msg(rc->cb);

	/* preprocess for apummu */
	mdw_plat_v4_appendix_process(c, APU_APPENDIX_CB_PREPROCESS);

	/* trigger cmd */
	ret = mdw_rv_dev_run_cmd(c->mpriv, rc);

	/* polling cmd done if performance mode */
	if (c->power_plcy == MDW_POWERPOLICY_PERFORMANCE) {
		if (!mdw_ch_pollcmd_timeout(&rmc->cmd_done, 1, MDW_POLL_TIMEOUT, c))
			ret = EALREADY;
	}

	mdw_drv_debug("\n");

	return 0;
}

/* called after run done, before copy to user */
static int mdw_plat_v4_postprocess_cmd(struct mdw_cmd *c)
{
	struct mdw_rv_cmd *rc = (struct mdw_rv_cmd *)c->plat_priv;
	struct mdw_rv_msg_cmd *rmc = NULL;
	int ret = 0;

	mdw_drv_debug("msg(%pK) sync id (%lld)\n", &rc->s_msg.msg, rc->s_msg.msg.sync_id);

	if (c->cmd_state == MDW_CMD_STATE_POSTPROCESS_DONE) {
		mdw_drv_debug("cmd already postprocess done\n");
		goto out;
	}

	/* copy exec info out */
	/* invalidate */
	if (mdw_mem_invalidate(c->mpriv, rc->cb))
		mdw_drv_warn("s(0x%llx) c(0x%llx/0x%llx/0x%llx) invalidate rcbs(%llu) fail\n",
			c->mpriv->id, c->uid, c->kid,
			c->inference_id, rc->cb->size);

	/* copy exec infos */
	rmc = (struct mdw_rv_msg_cmd *)rc->cb->vaddr;
	if (rmc->exec_infos_offset + c->exec_infos->size <= rc->cb->size) {
		memcpy(c->exec_infos->vaddr,
			rc->cb->vaddr + rmc->exec_infos_offset,
			c->exec_infos->size);
	} else {
		mdw_drv_warn("c(0x%llx/0x%llx/0x%llx) execinfos(%llu/%llu) not matched\n",
			c->uid, c->kid, c->inference_id,
			rmc->exec_infos_offset + c->exec_infos->size,
			rc->cb->size);
		ret = -EINVAL;
	}

	/* postprocess appendix */
	mdw_plat_v4_appendix_process(c, APU_APPENDIX_CB_POSTPROCESS);

out:
	return ret;
}

/* called after signal done to user */
static int mdw_plat_v4_late_postprocess_cmd(struct mdw_cmd *c)
{
	bool need_dtime_handle = false;
	uint64_t ts1 = 0, ts2 = 0;

	mdw_flw_debug("\n");
	if (c->cmd_state == MDW_CMD_STATE_IDLE) {
		mdw_drv_debug("cmd already late postprocess done\n");
		goto out;
	}
	/* postprocess appendix */
	mdw_plat_v4_appendix_process(c, APU_APPENDIX_CB_POSTPROCESS_LATE);

	ts1 = sched_clock();
	mdw_pb_put(c->power_plcy);
	ts2 = sched_clock();
	c->pb_put_time = ts2 - ts1;
	atomic_dec(&c->mpriv->mdev->cmd_running);

	/* update cmd history */
	ts1 = sched_clock();
	need_dtime_handle = mdw_ch_cmd_exec_update(c);
	ts2 = sched_clock();
	c->load_aware_pwroff_time = ts2 - ts1;

	/* handle dtime */
	if (need_dtime_handle == true)
		mdw_rv_dev_dtime_handle((struct mdw_rv_dev *)c->mpriv->mdev->dev_specific, c);
out:
	return 0;
}

static int mdw_plat_v4_check_sc_rets(struct mdw_cmd *c, int ipi_ret)
{
	uint32_t idx = 0, is_dma = 0, is_aps = 0;
	int ret = ipi_ret;  // ret from mdw_rv_ipi_cmplt_cmd
	DECLARE_BITMAP(tmp, 64);

	/* check cmd not postprocess yet */
	if (c->cmd_state == MDW_CMD_STATE_POSTPROCESS_DONE) {
		mdw_drv_debug("cmd already postprocess done\n");
		goto out;
	}

	/* check subcmds return value */
	if (c->einfos->c.sc_rets) {
		if (!ipi_ret)
			ret = -EIO;

		memcpy(&tmp, &c->einfos->c.sc_rets, sizeof(c->einfos->c.sc_rets));

		/* extract fail subcmd */
		do {
			idx = find_next_bit((unsigned long *)&tmp, c->num_subcmds, idx);
			if (idx >= c->num_subcmds)
				break;

			mdw_drv_warn("sc(0x%llx-#%u) type(%u) softlimit(%u) boost(%u) fail\n",
				c->kid, idx, c->subcmds[idx].type,
				c->softlimit, c->subcmds[idx].boost);
			switch (c->subcmds[idx].type) {
			case APUSYS_DEVICE_EDMA:
				is_dma++;
				break;
			case APUSYS_DEVICE_APS:
				is_aps++;
				break;
			default:
				break;
			}

			idx++;
		} while (idx < c->num_subcmds);

		/* trigger exception if dma */
		if (is_dma) {
			mdw_drv_err("dma exec fail: s(0x%llx) pid(%d/%d) c(0x%llx) ret(%d/0x%llx)\n",
				c->mpriv->id, c->pid, c->tgid,
				c->kid, ret, c->einfos->c.sc_rets);
			dma_exception("dma exec fail\n");
		}
		if (is_aps) {
			mdw_drv_err("aps exec fail: s(0x%llx) pid(%d/%d) c(0x%llx) ret(%d/0x%llx)\n",
				c->mpriv->id, c->pid, c->tgid,
				c->kid, ret, c->einfos->c.sc_rets);
			aps_exception("aps exec fail\n");
		}
	}
	c->einfos->c.ret = ret;

out:
	return ret;
}

static int mdw_plat_v4_cmd_sanity_check(struct mdw_cmd *c)
{
	int ret = -EINVAL;

	mdw_flw_debug("\n");
	/* 1. basic check */
	if (c->priority >= MDW_PRIORITY_MAX ||
		c->num_subcmds > MDW_SUBCMD_MAX ||
		c->num_links > c->num_subcmds) {
		mdw_drv_err("s(0x%llx)cmd invalid(0x%llx/0x%llx)(%u/%u/%u)\n",
			c->mpriv->id, c->uid, c->kid,
			c->priority, c->num_subcmds, c->num_links);
		goto out;
	}

	/* 2. exec info size check */
	ret = mdw_sanity_einfo_check(c);
	if (ret) {
		mdw_drv_err("cmd sanity check: einfo error\n");
		goto out;
	}

	/* 3. adj matrix check */
	ret = mdw_sanity_adj_check(c);
	if (ret) {
		mdw_drv_err("cmd sanity check: adj matrix error\n");
		goto out;
	}

	/* 4. links check */
	ret = mdw_sanity_link_check(c);
	if (ret) {
		mdw_drv_err("cmd sanity check: links error\n");
		goto out;
	}

	/* 5. subcmd info check */
	ret = mdw_plat_v4_sc_sanity_check(c);
	if (ret) {
		mdw_drv_err("cmd sanity check: subcmd error\n");
		goto out;
	}

	ret = 0;
out:
	return ret;
}

/* mdw_plat_func for v4 */
const struct mdw_plat_func mdw_plat_func_v4 = {
	.late_init = mdw_plat_v4_late_init,
	.late_deinit = mdw_plat_v4_late_deinit,
	.sw_init = mdw_rv_sw_init,
	.sw_deinit = mdw_rv_sw_deinit,
	.set_power = mdw_rv_set_power,
	.ucmd =	mdw_rv_ucmd,
	.set_param = mdw_rv_set_param,
	.get_info = mdw_rv_get_info,

	.register_device = mdw_plat_v4_register_device,
	.unregister_device = mdw_plat_v4_unregister_device,

	.create_session = mdw_plat_v4_create_session,
	.delete_session = mdw_plat_v4_delete_session,
	.create_cmd_priv = mdw_plat_v4_create_cmd_priv,
	.delete_cmd_priv = mdw_plat_v4_delete_cmd_priv,

	.run_cmd = mdw_plat_v4_run_cmd,
	.preprocess_cmd = mdw_plat_v4_preprocess_cmd,
	.postprocess_cmd = mdw_plat_v4_postprocess_cmd,
	.late_postprocess_cmd =	mdw_plat_v4_late_postprocess_cmd,

	.check_sc_rets = mdw_plat_v4_check_sc_rets,
	.cmd_sanity_check = mdw_plat_v4_cmd_sanity_check,
};
