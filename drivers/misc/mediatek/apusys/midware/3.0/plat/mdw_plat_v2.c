// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mdw_plat.h"
#include "mdw_cmn.h"
#include "mdw_rv.h"
#include "mdw_mem_pool.h"
#include "mdw_cmd.h"
#include "mdw_sanity.h"

/* for rv cmd v2 */
struct mdw_rv_msg_cmd {
	/* ids */
	uint64_t session_id;
	uint64_t cmd_id;
	uint32_t pid;
	uint32_t tgid;
	/* params */
	uint32_t priority;
	uint32_t hardlimit;
	uint32_t softlimit;
	uint32_t power_save;
	uint32_t power_plcy;
	uint32_t power_dtime;
	uint32_t app_type;
	uint32_t num_subcmds;
	uint32_t subcmds_offset;
	uint32_t num_cmdbufs;
	uint32_t cmdbuf_infos_offset;
	uint32_t adj_matrix_offset;
	uint32_t exec_infos_offset;
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
	/* cmdbufs info */
	uint32_t cmdbuf_start_idx;
	uint32_t num_cmdbufs;
} __packed;

struct mdw_rv_msg_cb {
	uint64_t device_va;
	uint32_t size;
} __packed;

/* for rv exec info v2 */
struct mdw_rv_cmd_exec_info {
	uint64_t sc_rets;
	int64_t ret;
	uint64_t total_us;
	uint64_t reserved;
};

struct mdw_rv_subcmd_exec_info {
	uint32_t driver_time;
	uint32_t ip_time;
	uint32_t ip_start_ts;
	uint32_t ip_end_ts;
	uint32_t bw;
	uint32_t boost;
	uint32_t tcm_usage;
	int32_t ret;
};

/* called by plat funcs */
static struct mdw_mem_map *mdw_plat_v2_create_msg(struct mdw_cmd *c)
{
	struct mdw_mem_map *rv_cb = NULL;
	struct mdw_fpriv *mpriv = c->mpriv;
	uint64_t cb_size = 0;
	uint32_t acc_cb = 0, i = 0, j = 0;
	uint32_t subcmds_ofs = 0, cmdbuf_infos_ofs = 0, adj_matrix_ofs = 0;
	uint32_t exec_infos_ofs = 0;
	struct mdw_rv_msg_cmd *rmc = NULL;
	struct mdw_rv_msg_sc *rmsc = NULL;
	struct mdw_rv_msg_cb *rmcb = NULL;
	uint64_t rv_einfo_size = sizeof(struct mdw_rv_cmd_exec_info) +
		(c->num_subcmds * sizeof(struct mdw_rv_subcmd_exec_info));

	mdw_trace_begin("apumdw:rv_cmd_create");

	/* check mem address for rv */
	if (MDW_IS_HIGHADDR(c->exec_infos->device_va) ||
		MDW_IS_HIGHADDR(c->cmdbufs->device_va)) {
		mdw_drv_err("rv dva high addr(0x%llx/0x%llx)\n",
			c->cmdbufs->device_va, c->exec_infos->device_va);
		goto out;
	}

	/* calc size and offset */
	if (check_add_overflow(cb_size, sizeof(struct mdw_rv_msg_cmd), &cb_size))
		goto cb_overflow;
	cb_size = MDW_ALIGN(cb_size, MDW_DEFAULT_ALIGN);

	/* calc adj_matrix_ofs and size */
	adj_matrix_ofs = cb_size;
	if (check_add_overflow(cb_size, (c->num_subcmds * c->num_subcmds * sizeof(uint8_t)), &cb_size))
		goto cb_overflow;
	cb_size = MDW_ALIGN(cb_size, MDW_DEFAULT_ALIGN);

	/* calc subcmds_ofs and size*/
	subcmds_ofs = cb_size;
	if (check_add_overflow(cb_size, (c->num_subcmds * sizeof(struct mdw_rv_msg_sc)), &cb_size))
		goto cb_overflow;
	cb_size = MDW_ALIGN(cb_size, MDW_DEFAULT_ALIGN);

	/* calc cmdbuf_infos_ofs */
	cmdbuf_infos_ofs = cb_size;
	if (check_add_overflow(cb_size, (c->num_cmdbufs * sizeof(struct mdw_rv_msg_cb)), &cb_size))
		goto cb_overflow;
	cb_size = MDW_ALIGN(cb_size, MDW_DEFAULT_ALIGN);

	/* calc exec_infos_ofs */
	exec_infos_ofs = cb_size;
	if (check_add_overflow(cb_size, rv_einfo_size, &cb_size))
		goto cb_overflow;

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
	rmc->session_id = (uint64_t)c->mpriv;
	rmc->cmd_id = c->kid;
	rmc->pid = (uint32_t)c->pid;
	rmc->tgid = (uint32_t)c->tgid;
	rmc->priority = c->priority;
	rmc->hardlimit = c->hardlimit;
	rmc->softlimit = c->softlimit;
	rmc->power_save = c->power_save;
	rmc->power_plcy = c->power_plcy;
	rmc->power_dtime = c->power_dtime;
	rmc->app_type = c->app_type;
	rmc->num_subcmds = c->num_subcmds;
	rmc->num_cmdbufs = c->num_cmdbufs;
	rmc->subcmds_offset = subcmds_ofs;
	rmc->cmdbuf_infos_offset = cmdbuf_infos_ofs;
	rmc->adj_matrix_offset = adj_matrix_ofs;
	rmc->exec_infos_offset = exec_infos_ofs;

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
		rmsc[i].num_cmdbufs = c->subcmds[i].num_cmdbufs;
		rmsc[i].cmdbuf_start_idx = acc_cb;

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
		(c->num_subcmds * c->num_subcmds * sizeof(uint8_t)));

	goto out;

cb_overflow:
	mdw_drv_err("cb_size overflow(%llu)\n", cb_size);
	rv_cb = NULL;
out:
	mdw_trace_end();
	mdw_cmd_debug("\n");

	return rv_cb;
}

static void mdw_plat_v2_delete_msg(struct mdw_mem_map *rv_cb, struct mdw_cmd *c)
{
	mdw_cmd_debug("\n");
	mdw_mem_pool_free(rv_cb);
}
static void mdw_plat_v2_reset_info(struct mdw_rv_msg_cmd *rmc, struct mdw_cmd *c,
									struct mdw_mem_map *map)
{
	struct mdw_rv_cmd *rc = (struct mdw_rv_cmd *)c->plat_priv;
	uint64_t rv_einfo_size = sizeof(struct mdw_rv_cmd_exec_info) +
		(c->num_subcmds * sizeof(struct mdw_rv_subcmd_exec_info));

	/* clear rv exec infos */
	memset((void *)rmc + rmc->exec_infos_offset, 0, rv_einfo_size);

	/* clear exec ret */
	c->einfos->c.ret = 0;
	c->einfos->c.sc_rets = 0;

	if (mdw_mem_flush(c->mpriv, rc->cb))
		mdw_drv_warn("s(0x%llx) c(0x%llx/0x%llx) flush rv cbs(%llu) fail\n",
			(uint64_t)c->mpriv, c->kid, c->inference_id, rc->cb->size);
}
static void mdw_plat_v2_show_msg(struct mdw_mem_map *map)
{
	struct mdw_rv_msg_cmd *rmc = (struct mdw_rv_msg_cmd *)map->vaddr;
	struct mdw_rv_msg_sc *rmsc = NULL;
	struct mdw_rv_msg_cb *rcb = NULL;
	uint32_t i = 0;

	/* show cmd info */
	mdw_cmd_debug("cmd info(0x%llx): pid(%u/%u) session(0x%llx) id(0x%llx)\n",
		map->device_va, rmc->pid, rmc->tgid, rmc->session_id, rmc->cmd_id);
	mdw_cmd_debug("  subcmds(%u/%u) cmdbufs(%u/%u)\n",
		rmc->num_subcmds, rmc->subcmds_offset, rmc->num_cmdbufs, rmc->cmdbuf_infos_offset);
	mdw_cmd_debug("  priority(%u) hardlimit(%u) softlimit(%u) power_save(%u) pwr_plcy(%u) dtime(%u)\n",
		rmc->priority, rmc->hardlimit, rmc->softlimit,
		rmc->power_save, rmc->power_plcy, rmc->power_dtime);
	mdw_cmd_debug("  exec_infos(%u) app_type(%u)\n",
		rmc->exec_infos_offset, rmc->app_type);

	/* cmdbufs */
	rcb = (struct mdw_rv_msg_cb *)(map->vaddr + rmc->cmdbuf_infos_offset);
	mdw_cmd_debug(" cmdbufs:\n");
	for (i = 0; i < rmc->num_cmdbufs; i++) {
		mdw_cmd_debug("    cmdbufs[%u](0x%llx/%u)\n", i, rcb->device_va, rcb->size);
		rcb++;
	}

	/* adj matrix */
	if (mdw_debug_on(MDW_DBG_CMD)) {
		print_hex_dump(KERN_INFO, "  adj_matrix: ",
			DUMP_PREFIX_OFFSET, 16, 4, map->vaddr + rmc->adj_matrix_offset,
			(rmc->num_subcmds * rmc->num_subcmds * sizeof(uint8_t)), 0);
	}

	/* subcmds */
	rmsc = (struct mdw_rv_msg_sc *)(map->vaddr + rmc->subcmds_offset);
	for (i = 0; i < rmc->num_subcmds; i++) {
		mdw_cmd_debug("  subcmd[%u]:\n", i);
		mdw_cmd_debug("    type(%u) vlm(%u/%u) boost(%u/%u) hse_en(%u)\n",
			rmsc->type, rmsc->vlm_ctx_id, rmsc->vlm_usage, rmsc->boost, rmsc->turbo_boost,
			rmsc->hse_en);
		mdw_cmd_debug("    cmdbufs_info(%u/%u)\n",
			rmsc->cmdbuf_start_idx, rmsc->num_cmdbufs);
		rmsc++;
	}
}

/* plat funcs for v2 */
static int mdw_plat_v2_late_init(struct mdw_device *mdev)
{
	int ret = 0;

	mdw_drv_debug("\n");

	/* assign library version */
	mdev->mdw_ver = 2;
	mdw_drv_info("mdw_ver = %u\n", mdev->mdw_ver);

	/* support rv execution */
	ret = mdw_rv_late_init(mdev);
	if (ret)
		mdw_drv_err("mdw rv late init failed\n");

	return ret;
}

static int mdw_plat_v2_register_device(struct apusys_device *adev)
{
	mdw_drv_debug("\n");
	return 0;
}
static int mdw_plat_v2_unregister_device(struct apusys_device *adev)
{
	mdw_drv_debug("\n");
	return 0;
}
static int mdw_plat_v2_create_session(struct mdw_fpriv *mpriv)
{
	mdw_drv_debug("\n");
	return 0;
}
static int mdw_plat_v2_delete_session(struct mdw_fpriv *mpriv)
{
	mdw_drv_debug("\n");
	return 0;
}

static int mdw_plat_v2_create_cmd_priv(struct mdw_cmd *c)
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
	rc->cb = mdw_plat_v2_create_msg(c);
	if (!rc->cb) {
		mdw_drv_err("create rv cmd msg fail\n");
		ret = -ENOMEM;
		goto free_cp_priv;
	}

	rc->c = c;

	mdw_drv_debug("\n");
	c->plat_priv = rc;
	c->rvid = (uint64_t)&rc->s_msg;

	goto out;

free_cp_priv:
	devm_kfree(c->mpriv->dev, rc);
out:
	return ret;
}
static int mdw_plat_v2_delete_cmd_priv(struct mdw_cmd *c)
{
	int ret = 0;
	struct mdw_rv_cmd *rc = NULL;

	if (IS_ERR_OR_NULL(c->plat_priv)) {
		mdw_drv_err("cmd priv is already free\n");
		return -EINVAL;
	}

	rc = c->plat_priv;
	if (rc->cb == NULL) {
		mdw_drv_err("rv cmd cmdbuf is NULL\n");
		ret = -ENOMSG;
	} else {
		mdw_plat_v2_delete_msg(rc->cb, c);
	}

	devm_kfree(c->mpriv->dev, c->plat_priv);
	c->plat_priv = NULL;

	mdw_drv_debug("\n");

	return ret;
}

static int mdw_plat_v2_preprocess_cmd(struct mdw_cmd *c)
{
	struct mdw_rv_cmd *rc = (struct mdw_rv_cmd *)c->plat_priv;

	mdw_drv_debug("msg(%pK) sync id (%lld)\n", &rc->s_msg.msg, rc->s_msg.msg.sync_id);
	rc->start_ts_ns = c->start_ts;
	atomic_inc(&c->mpriv->mdev->cmd_running);

	/* set affinity if in PERFORMANCE_MODE */
	mdw_rv_cmd_set_affinity(c, true);

	return 0;
}
static int mdw_plat_v2_run_cmd(struct mdw_cmd *c)
{
	int ret = 0;
	struct mdw_rv_cmd *rc = (struct mdw_rv_cmd *)c->plat_priv;
	struct mdw_rv_msg_cmd *rmc = NULL;

	if (rc == NULL || rc->cb == NULL || rc->cb->vaddr == NULL)
		return -EINVAL;

	/* assign rv msg */
	rmc = (struct mdw_rv_msg_cmd *)rc->cb->vaddr;

	/* clear execute info and update inf_id */
	mdw_plat_v2_reset_info(rmc, c, rc->cb);

	mdw_plat_v2_show_msg(rc->cb);

	/* trigger cmd */
	ret = mdw_rv_dev_run_cmd(c->mpriv, rc);

	mdw_drv_debug("\n");

	return ret;
}
static int mdw_plat_v2_postprocess_cmd(struct mdw_cmd *c)
{
	struct mdw_rv_cmd *rc = (struct mdw_rv_cmd *)c->plat_priv;
	struct mdw_rv_msg_cmd *rmc = NULL;
	uint64_t rv_cmd_einfo_size = sizeof(struct mdw_rv_cmd_exec_info),
			 rv_sc_einfo_size = sizeof(struct mdw_rv_subcmd_exec_info),
			 rv_einfo_size = rv_cmd_einfo_size + (c->num_subcmds * rv_sc_einfo_size);
	int ret = 0;

	mdw_drv_debug("msg(%pK) sync id (%lld)\n", &rc->s_msg.msg, rc->s_msg.msg.sync_id);

	if (c->cmd_state == MDW_CMD_STATE_POSTPROCESS_DONE) {
		mdw_drv_debug("cmd already postprocess done\n");
		goto out;
	}

	/* invalidate */
	if (mdw_mem_invalidate(c->mpriv, rc->cb))
		mdw_drv_warn("s(0x%llx) c(0x%llx/0x%llx/0x%llx) invalidate rcbs(%llu) fail\n",
			(uint64_t)c->mpriv, c->uid, c->kid,
			c->inference_id, rc->cb->size);

	/* copy exec info out */
	rmc = (struct mdw_rv_msg_cmd *)rc->cb->vaddr;
	if (rmc->exec_infos_offset + rv_einfo_size <= rc->cb->size &&
		rv_cmd_einfo_size <= sizeof(struct mdw_cmd_exec_info) &&
		rv_sc_einfo_size <= sizeof(struct mdw_subcmd_exec_info) &&
		rv_einfo_size <= c->exec_infos->size) {
		mdw_rv_einfo_copy_out(c, (void *)rmc + rmc->exec_infos_offset, rv_cmd_einfo_size, rv_sc_einfo_size);
	} else {
		mdw_drv_warn("c(0x%llx/0x%llx/0x%llx) execinfos not matched\n",
			c->uid, c->kid, c->inference_id);
		mdw_drv_debug("  cmd_exec_info(%llu/%lu) subcmd_exec_info(%llu/%lu) execinfos(%llu/%llu)\n",
			rv_cmd_einfo_size, sizeof(struct mdw_cmd_exec_info),
			rv_sc_einfo_size, sizeof(struct mdw_subcmd_exec_info),
			rv_einfo_size, c->exec_infos->size);
		ret = -EINVAL;
	}

out:
	return ret;
}
static int mdw_plat_v2_late_postprocess_cmd(struct mdw_cmd *c)
{
	mdw_flw_debug("\n");
	if (c->cmd_state == MDW_CMD_STATE_IDLE) {
		mdw_drv_debug("cmd already late postprocess done\n");
		goto out;
	}

	atomic_dec(&c->mpriv->mdev->cmd_running);
	mdw_rv_cmd_set_affinity(c, false);

out:
	return 0;
}
static int mdw_plat_v2_check_sc_rets(struct mdw_cmd *c, int ipi_ret)
{
	uint64_t idx = c->einfos->c.sc_rets, is_dma = 0;
	int ret = ipi_ret;  // ret from mdw_rv_ipi_cmplt_cmd
	DECLARE_BITMAP(tmp, 64);

	if (c->cmd_state == MDW_CMD_STATE_POSTPROCESS_DONE) {
		mdw_drv_debug("cmd already postprocess done\n");
		goto out;
	}

	if (c->einfos->c.sc_rets) {
		if (!ipi_ret)
			ret = -EIO;

		memcpy(&tmp, &c->einfos->c.sc_rets, sizeof(c->einfos->c.sc_rets));

		/* extract fail subcmd */
		do {
			idx = find_next_bit((unsigned long *)&tmp, c->num_subcmds, idx);
			if (idx >= c->num_subcmds)
				break;

			mdw_drv_warn("sc(0x%llx-#%llu) type(%u) softlimit(%u) boost(%u) fail\n",
				c->kid, idx, c->subcmds[idx].type,
				c->softlimit, c->subcmds[idx].boost);
			if (c->subcmds[idx].type == APUSYS_DEVICE_EDMA)
				is_dma++;

			idx++;
		} while (idx < c->num_subcmds);

		/* trigger exception if dma */
		if (is_dma) {
			mdw_drv_err("s(0x%llx)pid(%d/%d)c(0x%llx)fail(%d/0x%llx)\n",
				(uint64_t)c->mpriv, c->pid, c->tgid,
				c->kid, ret, c->einfos->c.sc_rets);
			dma_exception("EDMA exec fail\n");
		}
	}
	c->einfos->c.ret = ret;
out:
	return ret;
}

static int mdw_plat_v2_sc_sanity_check(struct mdw_cmd *c)
{
	unsigned int i = 0;

	mdw_flw_debug("\n");

	/* subcmd info */
	for (i = 0; i < c->num_subcmds; i++) {
		if (c->subcmds[i].type >= MDW_DEV_MAX ||
			c->subcmds[i].vlm_ctx_id >= MDW_SUBCMD_MAX ||
			c->subcmds[i].boost > MDW_BOOST_MAX ||
			c->subcmds[i].pack_id >= MDW_SUBCMD_MAX) {
			mdw_drv_err("subcmd(%u) invalid (%u/%u/%u)\n",
				i, c->subcmds[i].type,
				c->subcmds[i].boost,
				c->subcmds[i].pack_id
			);
			return -EINVAL;
		}
	}

	return 0;
}

static int mdw_plat_v2_cmd_sanity_check(struct mdw_cmd *c)
{
	int ret = -EINVAL;

	mdw_flw_debug("\n");

	/* check cmd params */
	if (c->priority >= MDW_PRIORITY_MAX ||
		c->num_subcmds > MDW_SUBCMD_MAX) {
		mdw_drv_err("s(0x%llx)cmd invalid(0x%llx/0x%llx)(%u/%u)\n",
			(uint64_t)c->mpriv, c->uid, c->kid,
			c->priority, c->num_subcmds);
		goto out;
	}

	/* check exec infos */
	ret = mdw_sanity_einfo_check(c);
	if (ret) {
		mdw_drv_err("cmd sanity check: einfo error\n");
		goto out;
	}

	/* check adj matrix */
	ret = mdw_sanity_adj_check(c);
	if (ret) {
		mdw_drv_err("cmd sanity check: adj matrix error\n");
		goto out;
	}

	/* check subcmd info */
	ret = mdw_plat_v2_sc_sanity_check(c);
	if (ret) {
		mdw_drv_err("cmd sanity check: subcmd error\n");
		goto out;
	}

	ret = 0;
out:
	return ret;
}

/* mdw_plat_func for v2 */
const struct mdw_plat_func mdw_plat_func_v2 = {
	.late_init =   mdw_plat_v2_late_init,
	.late_deinit = mdw_rv_late_deinit,
	.sw_init =     mdw_rv_sw_init,
	.sw_deinit =   mdw_rv_sw_deinit,
	.set_power =   mdw_rv_set_power,
	.ucmd =        mdw_rv_ucmd,
	.set_param =   mdw_rv_set_param,
	.get_info =    mdw_rv_get_info,

	.register_device =   mdw_plat_v2_register_device,
	.unregister_device = mdw_plat_v2_unregister_device,

	.create_session = mdw_plat_v2_create_session,
	.delete_session = mdw_plat_v2_delete_session,
	.create_cmd_priv = mdw_plat_v2_create_cmd_priv,
	.delete_cmd_priv = mdw_plat_v2_delete_cmd_priv,

	.run_cmd              = mdw_plat_v2_run_cmd,
	.preprocess_cmd       = mdw_plat_v2_preprocess_cmd,
	.postprocess_cmd      = mdw_plat_v2_postprocess_cmd,
	.late_postprocess_cmd = mdw_plat_v2_late_postprocess_cmd,

	.check_sc_rets = mdw_plat_v2_check_sc_rets,
	.cmd_sanity_check = mdw_plat_v2_cmd_sanity_check,
};
