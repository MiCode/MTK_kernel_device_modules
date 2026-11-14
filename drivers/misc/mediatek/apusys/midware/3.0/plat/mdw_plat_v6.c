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
/* dvfs policy support */
#include "mdw_dplcy.h"


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
	uint64_t flags;
	uint64_t function_bitmask;
	uint32_t app_type;
	uint32_t hse_num;
	uint32_t vlm_max;
	/* subcmds info */
	uint32_t num_subcmds;
	uint32_t subcmds_offset; //struct mdw_rv_msg_sc
	/* appendix cmdbufs info */
	uint32_t appendix_cmdbuf_infos_num;
	uint32_t appendix_cmdbuf_infos_offset; //struct mdw_rv_msg_appendix_cmdbuf_infos
	/* cmdbufs */
	uint32_t num_cmdbufs;
	uint32_t cmdbuf_infos_offset;
	/* dependnecy description */
	uint32_t execute_order_offset;
	uint32_t predecessors_size;
	uint32_t predecessors_offset;
	uint32_t pack_friends_size;
	uint32_t pack_friends_offset;
	uint32_t end_vertices_size;
	uint32_t end_vertices_offset;
	uint32_t exec_infos_offset;
	/* history params */
	uint32_t inference_ms;
	uint32_t tolerance_ms;
	/* cmd done */
	uint32_t cmd_done; // up output
	/* dvfs target time */
	uint64_t auto_dvfs_target_time;
} __packed;

struct mdw_rv_msg_sc {
	/* cmd params */
	uint32_t type;
	uint32_t vlm_usage;
	uint32_t vlm_ctx_id;
	uint32_t boost;
	uint32_t turbo_boost;
	uint32_t hse_en;
	uint32_t affinity;
	uint32_t trigger_type;
	/* cmdbufs info */
	uint32_t cmdbuf_start_idx;
	uint32_t num_cmdbufs;
	/* predecessor */
	uint32_t predecessors_start_idx;
	uint32_t predecessors_num;
	/* pack friend */
	uint32_t pack_friends_start_idx;
	uint32_t pack_friends_num;
	/* reserved for up */
	uint32_t up_reserve[2];
} __packed;

struct mdw_rv_msg_appendix_cmdbuf_infos {
	uint32_t owner; //enum apu_appendix_cb_owner
	uint32_t cmdbuf_idx;
} __packed;

struct mdw_rv_msg_cb {
	uint64_t device_va;
	uint32_t size;
} __packed;
/***********************************************************************/
static void mdw_plat_v6_show_msg(struct mdw_mem_map *map)
{
	struct mdw_rv_msg_cmd *rmc = (struct mdw_rv_msg_cmd *)map->vaddr;
	struct mdw_rv_msg_sc *rmsc = NULL;
	struct mdw_rv_msg_cb *rcb = NULL;
	uint32_t i = 0;

	/* show cmd info */
	mdw_cmd_debug("cmd info(0x%llx): pid(%u/%u) session(0x%llx) id(0x%llx) inf_id(0x%llx)\n",
		map->device_va, rmc->pid, rmc->tgid, rmc->session_id, rmc->cmd_id, rmc->inference_id);
	mdw_cmd_debug("  subcmds(%u/%u) cmdbufs(%u/%u) appendix(%u/%u)\n",
		rmc->num_subcmds, rmc->subcmds_offset, rmc->num_cmdbufs, rmc->cmdbuf_infos_offset,
		rmc->appendix_cmdbuf_infos_num, rmc->appendix_cmdbuf_infos_offset);
	mdw_cmd_debug("  exec_order(%u) predecessors(%u/%u) pack_friends(%u/%u) exec_infos(%u)\n",
		rmc->execute_order_offset, rmc->predecessors_size, rmc->predecessors_offset,
		rmc->pack_friends_size, rmc->pack_friends_offset, rmc->exec_infos_offset);
	mdw_cmd_debug("  priority(%u) hardlimit(%u) softlimit(%u) fastms(%u) pwr_plcy(%u) dtime(%u) etime(%u)\n",
		rmc->priority, rmc->hardlimit, rmc->softlimit, rmc->fastmem_ms,
		rmc->power_plcy, rmc->power_dtime, rmc->power_etime);
	mdw_cmd_debug("  end_vertices(%u/%u) function_bitmask(0x%llx) app_type(%u) hse_num(%u) vlm_max(%u)\n",
		rmc->end_vertices_size, rmc->end_vertices_offset, rmc->function_bitmask, rmc->app_type,
		rmc->hse_num, rmc->vlm_max);
	mdw_cmd_debug("  tol_t(%u) inf_t(%u) dvfs_target_t(%llu)\n",
		rmc->tolerance_ms, rmc->inference_ms, rmc->auto_dvfs_target_time);

	/* cmdbufs */
	rcb = (struct mdw_rv_msg_cb *)(map->vaddr + rmc->cmdbuf_infos_offset);
	mdw_cmd_debug(" cmdbufs:\n");
	for (i = 0; i < rmc->num_cmdbufs; i++) {
		mdw_cmd_debug("    cmdbufs[%u](0x%llx/%u)\n", i, rcb->device_va, rcb->size);
		rcb++;
	}

	/* appendix */
	mdw_cmd_debug(" appendix cmdbufs:\n");
	for (i = 0; i < rmc->appendix_cmdbuf_infos_num; i++) {
		mdw_cmd_debug("    appendix[%u](0x%llx/%u)\n", i, rcb->device_va, rcb->size);
		rcb++;
	}

	if (mdw_debug_on(MDW_DBG_CMD)) {
		/* execute_order */
		print_hex_dump(KERN_INFO, "  execute_order order: ",
			DUMP_PREFIX_OFFSET, 16, 4, map->vaddr + rmc->execute_order_offset,
			sizeof(uint32_t) * rmc->num_subcmds, 0);

		/* predecessor*/
		print_hex_dump(KERN_INFO, "  predecessor: ",
			DUMP_PREFIX_OFFSET, 16, 4, map->vaddr + rmc->predecessors_offset,
			rmc->predecessors_size, 0);

		/* pack_friends */
		print_hex_dump(KERN_INFO, "  pack_friends: ",
			DUMP_PREFIX_OFFSET, 16, 4, map->vaddr + rmc->pack_friends_offset,
			rmc->pack_friends_size, 0);

		/* end_vertices */
		print_hex_dump(KERN_INFO, "  end_vertices: ",
			DUMP_PREFIX_OFFSET, 16, 4, map->vaddr + rmc->end_vertices_offset,
			rmc->end_vertices_size, 0);
	}

	rmsc = (struct mdw_rv_msg_sc *)(map->vaddr + rmc->subcmds_offset);
	for (i = 0; i < rmc->num_subcmds; i++) {
		mdw_cmd_debug("  subcmd[%u]:\n", i);
		mdw_cmd_debug("    type(%u) vlm(%u/%u) boost(%u/%u) hse_en(%u) affinity(0x%x) trigger(%u)\n",
			rmsc->type, rmsc->vlm_ctx_id, rmsc->vlm_usage, rmsc->boost, rmsc->turbo_boost,
			rmsc->hse_en, rmsc->affinity, rmsc->trigger_type);
		mdw_cmd_debug("    cmdbufs_info(%u/%u) predecessor(%u/%u) pack_friends(%u/%u)\n",
			rmsc->cmdbuf_start_idx, rmsc->num_cmdbufs, rmsc->predecessors_start_idx, rmsc->predecessors_num,
			rmsc->pack_friends_start_idx, rmsc->pack_friends_num);
		rmsc++;
	}
}

static int mdw_plat_v6_late_init(struct mdw_device *mdev)
{
	int ret = 0;

	mdw_drv_debug("\n");

	/* assign library version */
	mdev->mdw_ver = 6;
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

	/* support dvfs policy */
	ret = mdw_dplcy_init(mdev);
	if (ret) {
		mdw_drv_err("mdw dvfs policy init failed\n");
		goto deinit_ch;
	}

	/* support rv execution */
	ret = mdw_rv_late_init(mdev);
	if (ret) {
		mdw_drv_err("mdw rv late init failed\n");
		goto deinit_dp;
	}

	goto out;

deinit_dp:
	mdw_dplcy_deinit(mdev);
deinit_ch:
	mdw_ch_deinit(mdev);
deinit_pb:
	mdw_pb_deinit(mdev);
out:
	return ret;
}

static void mdw_plat_v6_late_deinit(struct mdw_device *mdev)
{
	mdw_drv_debug("\n");

	mdw_rv_late_deinit(mdev);
	mdw_dplcy_deinit(mdev);
	mdw_ch_deinit(mdev);
	mdw_pb_deinit(mdev);
}

static void mdw_plat_v6_appendix_process(struct mdw_cmd *c, enum apu_appendix_cb_type type)
{
	struct apusys_cmd_info cmd_info;
	struct mdw_rv_cmd *rv_cmd = (struct mdw_rv_cmd *)c->plat_priv;
	struct mdw_rv_msg_cmd *rmc = NULL;
	struct mdw_rv_msg_cb *rmcb = NULL;
	struct mdw_rv_msg_appendix_cmdbuf_infos *rmaci = NULL;
	uint32_t i = 0;
	uint64_t oft = 0;

	/* assign cmd info */
	cmd_info.session_id = (uint64_t)c->mpriv;
	cmd_info.cmd_uid = c->uid;
	cmd_info.num_subcmds = c->num_subcmds;
	cmd_info.power_plcy = c->power_plcy;

	rmc = (struct mdw_rv_msg_cmd *)rv_cmd->cb->vaddr;
	rmcb = (void*)rmc + rmc->cmdbuf_infos_offset;
	rmaci = (void *)rmc + rmc->appendix_cmdbuf_infos_offset;

	for (i = 0; i < rmc->appendix_cmdbuf_infos_num; i++) {
		oft = rmcb[rmaci->cmdbuf_idx].device_va - rv_cmd->cb->device_va;
		mdw_cb_appendix_process(type, i, &cmd_info,
			(void *)rmc + oft, rmcb[rmaci->cmdbuf_idx].size);
		mdw_flw_debug("rmc(%pK) rmcb-#%u(%pK) oft(%llu|0x%llx/0x%llx)\n",
			rmc, rmaci->cmdbuf_idx, rmcb, oft, rmcb[rmaci->cmdbuf_idx].device_va, rv_cmd->cb->device_va);

		rmaci++;
	}
}

static struct mdw_mem_map *mdw_plat_v6_create_msg(struct mdw_cmd *c)
{
	struct apusys_cmd_info cmd_info;
	struct mdw_mem_map *rv_cmdbuf = NULL;
	struct mdw_fpriv *mpriv = c->mpriv;
	uint64_t cb_size = 0;
	uint32_t acc_cb = 0, i = 0, j = 0;
	uint32_t subcmds_ofs = 0, cmdbuf_infos_ofs = 0, exec_infos_ofs = 0;
	uint32_t execute_order_ofs = 0, predecessor_ofs = 0, pack_friend_ofs = 0, end_vertices_ofs = 0;
	uint32_t appendix_infos_ofs = 0, appendix_cmdbufs_ofs = 0, appendix_num = 0, tmp_ofs = 0;
	struct mdw_rv_msg_cmd *rmc = NULL;
	struct mdw_rv_msg_sc *rmsc = NULL;
	struct mdw_rv_msg_cb *rmcb = NULL;
	struct mdw_rv_msg_appendix_cmdbuf_infos *rmaci = NULL;

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
	if (check_add_overflow(cb_size, sizeof(struct mdw_rv_msg_cmd), &cb_size))
		goto cb_overflow;
	cb_size = MDW_ALIGN(cb_size, MDW_DEFAULT_ALIGN);
	/* subcmds */
	subcmds_ofs = cb_size;
	if (check_add_overflow(cb_size, (c->num_subcmds * sizeof(struct mdw_rv_msg_sc)), &cb_size))
		goto cb_overflow;
	cb_size = MDW_ALIGN(cb_size, MDW_DEFAULT_ALIGN);
	/* appendix cmdbufs infos */
	appendix_infos_ofs = cb_size;
	if (check_add_overflow(cb_size, (appendix_num * sizeof(struct mdw_rv_msg_appendix_cmdbuf_infos)), &cb_size))
		goto cb_overflow;
	cb_size = MDW_ALIGN(cb_size, MDW_DEFAULT_ALIGN);
	/* cmfbufs - include appendix */
	cmdbuf_infos_ofs = cb_size;
	if (check_add_overflow(cb_size, (c->num_cmdbufs + appendix_num) * sizeof(struct mdw_rv_msg_cb), &cb_size))
		goto cb_overflow;
	cb_size = MDW_ALIGN(cb_size, MDW_DEFAULT_ALIGN);
	/* execute orders */
	execute_order_ofs = cb_size;
	if (check_add_overflow(cb_size, (c->num_subcmds * sizeof(*c->execute_orders)), &cb_size))
		goto cb_overflow;
	cb_size = MDW_ALIGN(cb_size, MDW_DEFAULT_ALIGN);
	/* predecessors */
	predecessor_ofs = cb_size;
	if (check_add_overflow(cb_size, c->predecessors_size, &cb_size))
		goto cb_overflow;
	cb_size = MDW_ALIGN(cb_size, MDW_DEFAULT_ALIGN);
	/* pack friends */
	pack_friend_ofs = cb_size;
	if (check_add_overflow(cb_size, c->pack_friends_size, &cb_size))
		goto cb_overflow;
	cb_size = MDW_ALIGN(cb_size, MDW_DEFAULT_ALIGN);
	/* end leafs */
	end_vertices_ofs = cb_size;
	if (check_add_overflow(cb_size, c->end_vertices_size, &cb_size))
		goto cb_overflow;
	cb_size = MDW_ALIGN(cb_size, MDW_DEFAULT_ALIGN);
	/* exec infos */
	exec_infos_ofs = cb_size;
	if (check_add_overflow(cb_size, c->exec_infos->size, &cb_size))
		goto cb_overflow;
	cb_size = MDW_ALIGN(cb_size, MDW_DEFAULT_ALIGN);
	/* appendix cmdbufs content */
	appendix_cmdbufs_ofs = cb_size;
	for (i = 0; i < appendix_num; i++) {
		mdw_cmd_debug("appendix-#%u cb_size(%llu)\n", i, cb_size);
		if (check_add_overflow(cb_size, mdw_cb_appendix_size_by_idx(i, c->num_subcmds), &cb_size))
			goto cb_overflow;
		cb_size = MDW_ALIGN(cb_size, MDW_DEFAULT_ALIGN);
	}
	/* allocate communicate buffer */
	rv_cmdbuf = mdw_mem_pool_alloc(&mpriv->cmd_buf_pool, cb_size,
		MDW_DEFAULT_ALIGN);
	if (!rv_cmdbuf) {
		mdw_drv_err("c(0x%llx) alloc cb size(%llu) fail\n",
			c->kid, cb_size);
		goto out;
	}

	/* assign cmd info */
	rmc = (struct mdw_rv_msg_cmd *)rv_cmdbuf->vaddr;
	rmc->session_id = (uint64_t)c->mpriv;
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
	rmc->hse_num = c->hse_num;
	rmc->function_bitmask = c->function_bitmask;
	rmc->num_subcmds = c->num_subcmds;
	rmc->num_cmdbufs = c->num_cmdbufs;
	rmc->subcmds_offset = subcmds_ofs;
	rmc->cmdbuf_infos_offset = cmdbuf_infos_ofs;
	rmc->appendix_cmdbuf_infos_num = appendix_num;
	rmc->appendix_cmdbuf_infos_offset = appendix_infos_ofs;
	rmc->execute_order_offset = execute_order_ofs;
	rmc->predecessors_size = c->predecessors_size;
	rmc->predecessors_offset = predecessor_ofs;
	rmc->pack_friends_size = c->pack_friends_size;
	rmc->pack_friends_offset = pack_friend_ofs;
	rmc->end_vertices_size = c->end_vertices_size;
	rmc->end_vertices_offset = end_vertices_ofs;
	rmc->exec_infos_offset = exec_infos_ofs;
	rmc->inference_ms = c->inference_ms;
	rmc->tolerance_ms = c->tolerance_ms;
	rmc->inference_id = c->inference_id;
	rmc->auto_dvfs_target_time = c->auto_dvfs_target_time;
	//mdw_rv_cmd_print(rmc);

	/* assign subcmds info */
	rmsc = (void *)rmc + rmc->subcmds_offset;
	rmcb = (void *)rmc + rmc->cmdbuf_infos_offset;
	for (i = 0; i < c->num_subcmds; i++) {
		rmsc[i].type = c->subcmds[i].type;
		rmsc[i].vlm_usage = c->subcmds[i].vlm_usage;
		rmsc[i].vlm_ctx_id = c->subcmds[i].vlm_ctx_id;
		rmsc[i].boost = c->subcmds[i].boost;
		rmsc[i].turbo_boost = c->subcmds[i].turbo_boost;
		rmsc[i].hse_en = c->subcmds[i].hse_en;
		rmsc[i].affinity = c->subcmds[i].affinity;
		rmsc[i].num_cmdbufs = c->subcmds[i].num_cmdbufs;
		rmsc[i].cmdbuf_start_idx = acc_cb;
		rmsc[i].trigger_type = c->subcmds[i].trigger_type;
		rmsc[i].predecessors_start_idx = c->subcmds[i].predecessors_start_idx;
		rmsc[i].predecessors_num = c->subcmds[i].predecessors_num;
		rmsc[i].pack_friends_start_idx = c->subcmds[i].pack_friends_start_idx;
		rmsc[i].pack_friends_num = c->subcmds[i].pack_friends_num;
		/* get max subcmd vlm usag */
		rmc->vlm_max = max(rmc->vlm_max, rmsc[i].vlm_usage);

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

	/* assign execute order */
	memcpy((void *)rmc + rmc->execute_order_offset, c->execute_orders,
		c->num_subcmds * sizeof(*c->execute_orders));

	/* assign predecessors */
	memcpy((void *)rmc + rmc->predecessors_offset, c->predecessors,
		c->predecessors_size);

	/* assign pack friends */
	memcpy((void *)rmc + rmc->pack_friends_offset, c->pack_friends,
		c->pack_friends_size);

	/* assign end leafs */
	memcpy((void *)rmc + rmc->end_vertices_offset, c->end_vertices,
		c->end_vertices_size);

	/* assign cmd info */
	cmd_info.session_id = (uint64_t)c->mpriv;
	cmd_info.cmd_uid = c->uid;
	cmd_info.num_subcmds = c->num_subcmds;
	cmd_info.power_plcy = c->power_plcy;

	/* assign appendix cmdbufs infos and content by callback */
	rmaci = (void *)rmc + rmc->appendix_cmdbuf_infos_offset;
	tmp_ofs = appendix_cmdbufs_ofs;
	for (i = 0; i < rmc->appendix_cmdbuf_infos_num; i++) {
		rmaci->owner = mdw_cb_appendix_get_owner(i);
		rmaci->cmdbuf_idx = i + c->num_cmdbufs;
		rmcb[rmaci->cmdbuf_idx].size = mdw_cb_appendix_size_by_idx(i, c->num_subcmds);
		rmcb[rmaci->cmdbuf_idx].device_va = rv_cmdbuf->device_va + tmp_ofs;

		mdw_flw_debug("appendix-#%u info_oft(%u) oft(%u) rmcb-#%u(0x%llx/%u)\n",
			i, rmc->appendix_cmdbuf_infos_offset, tmp_ofs,
			rmaci->cmdbuf_idx, rmcb[rmaci->cmdbuf_idx].device_va,
			rmcb[rmaci->cmdbuf_idx].size);

		if (mdw_cb_appendix_process(APU_APPENDIX_CB_CREATE, i, &cmd_info,
			(void *)rmc + tmp_ofs, rmcb[rmaci->cmdbuf_idx].size))
			mdw_drv_warn("appendix(%u) process(%d) failed\n", i, APU_APPENDIX_CB_CREATE);

		tmp_ofs += rmcb[rmaci->cmdbuf_idx].size;
		tmp_ofs = MDW_ALIGN(tmp_ofs, MDW_DEFAULT_ALIGN);

		rmaci++;
	}

	goto out;

cb_overflow:
	mdw_drv_err("cb_size overflow(%llu)\n", cb_size);
	rv_cmdbuf = NULL;
out:
	mdw_trace_end();
	mdw_cmd_debug("\n");

	return rv_cmdbuf;
}

static void mdw_plat_v6_delete_msg(struct mdw_mem_map *rv_cb)
{
	mdw_cmd_debug("\n");
	mdw_mem_pool_free(rv_cb);
}

static int mdw_plat_v6_register_device(struct apusys_device *adev)
{
	mdw_drv_debug("\n");
	return 0;
}

static int mdw_plat_v6_unregister_device(struct apusys_device *adev)
{
	mdw_drv_debug("\n");
	return 0;
}

static int mdw_plat_v6_create_session(struct mdw_fpriv *mpriv)
{
	int ret = 0;

	mdw_drv_debug("\n");
	mdw_pb_get(MDW_POWERPOLICY_DEFAULT, MDW_PB_DEBOUNCE_MS);

	ret = mdw_ch_session_create(mpriv);
	if (ret)
		mdw_drv_err("create cmd history session fail(%d)\n", ret);

	ret = mdw_dplcy_session_create(mpriv);
	if (ret)
		mdw_drv_err("create dplcy session fail(%d)\n", ret);

	return ret;
}

static int mdw_plat_v6_delete_session(struct mdw_fpriv *mpriv)
{
	mdw_drv_debug("\n");
	mdw_ch_session_delete(mpriv);
	mdw_dplcy_session_delete(mpriv);
	return 0;
}

static int mdw_plat_v6_create_cmd_priv(struct mdw_cmd *c)
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

	/* create rv cmdbuf */
	rc->cb = mdw_plat_v6_create_msg(c);
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
	mdw_plat_v6_delete_msg(rc->cb);
free_cp_priv:
	devm_kfree(c->mpriv->dev, rc);
out:
	return ret;
}

static int mdw_plat_v6_delete_cmd_priv(struct mdw_cmd *c)
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
		mdw_drv_err("rv cmd cmdbuf is NULL\n");
		ret = -ENOMSG;
	} else {
		mdw_plat_v6_delete_msg(rc->cb);
	}

	devm_kfree(c->mpriv->dev, c->plat_priv);
	c->plat_priv = NULL;

	mdw_drv_debug("\n");

	return ret;
}

static void mdw_plat_v6_reset_info(struct mdw_rv_msg_cmd *rmc, struct mdw_cmd *c,
				 struct mdw_mem_map *map)
{
	struct mdw_rv_cmd *rc = (struct mdw_rv_cmd *)c->plat_priv;
	struct mdw_rv_msg_sc *rmsc = NULL;
	uint32_t i = 0;

	/* clear einfos */
	memset((void *)rmc + rmc->exec_infos_offset, 0, c->exec_infos->size);
	/* clear exec ret */
	c->einfos->c.ret = 0;
	c->einfos->c.sc_rets = 0;
	rmc->cmd_done = 0;
	/* clear up reserve */
	rmsc = (struct mdw_rv_msg_sc *)(map->vaddr + rmc->subcmds_offset);
	for (i = 0; i < rmc->num_subcmds; i++) {
		memset(rmsc->up_reserve, 0, sizeof(rmsc->up_reserve));
		rmsc++;
	}
	/* update inference id */
	rmc->inference_id = c->inference_id;

	if (mdw_mem_flush(c->mpriv, rc->cb))
		mdw_drv_warn("s(0x%llx) c(0x%llx/0x%llx) flush rv cbs(%llu) fail\n",
			(uint64_t)c->mpriv, c->kid, c->inference_id, rc->cb->size);
}

/**
 * mdw_plat_v6_run_cmd() - Use rpmsg_sendto to power on or off
 * @c: mdw command
 *
 * Return:
 *  0: Success
 *  > 0: Command done already, should return success to user directly
 *  < 0: Failed
 */
static int mdw_plat_v6_run_cmd(struct mdw_cmd *c)
{
	int ret = 0;
	struct mdw_rv_cmd *rc = (struct mdw_rv_cmd *)c->plat_priv;
	struct mdw_rv_msg_cmd *rmc = NULL;

	if (rc == NULL || rc->cb == NULL || rc->cb->vaddr == NULL)
		return -EINVAL;

	/* assign rv msg */
	rmc = (struct mdw_rv_msg_cmd *)rc->cb->vaddr;

	/* clear execute info and update inf_id */
	mdw_plat_v6_reset_info(rmc, c, rc->cb);

	mdw_plat_v6_show_msg(rc->cb);

	/* preprocess appendix */
	mdw_plat_v6_appendix_process(c, APU_APPENDIX_CB_PREPROCESS);

	/* trigger cmd */
	ret = mdw_rv_dev_run_cmd(c->mpriv, rc);

	/* polling cmd done if performance mode */
	if (c->power_plcy == MDW_POWERPOLICY_PERFORMANCE) {
		if (!mdw_ch_pollcmd_timeout(&rmc->cmd_done, 1, MDW_POLL_TIMEOUT, c))
			ret = EALREADY;
	}

	mdw_drv_debug("\n");

	return ret;
}

static int mdw_plat_v6_preprocess_cmd(struct mdw_cmd *c)
{
	struct mdw_rv_cmd *rc = (struct mdw_rv_cmd *)c->plat_priv;

	mdw_drv_debug("msg(%pK) sync id (%lld)\n", &rc->s_msg.msg, rc->s_msg.msg.sync_id);
	mdw_pb_get(c->power_plcy, 0);
	rc->start_ts_ns = c->start_ts;
	atomic_inc(&c->mpriv->mdev->cmd_running);

	return 0;
}

static int mdw_plat_v6_postprocess_cmd(struct mdw_cmd *c)
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
			(uint64_t)c->mpriv, c->uid, c->kid,
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
	mdw_plat_v6_appendix_process(c, APU_APPENDIX_CB_POSTPROCESS);

out:
	return ret;
}

static int mdw_plat_v6_late_postprocess_cmd(struct mdw_cmd *c)
{
	bool need_dtime_handle = false;
	uint64_t ts1 = 0, ts2 = 0;

	mdw_flw_debug("\n");
	if (c->cmd_state == MDW_CMD_STATE_IDLE) {
		mdw_drv_debug("cmd already late postprocess done\n");
		goto out;
	}
	/* postprocess appendix */
	mdw_plat_v6_appendix_process(c, APU_APPENDIX_CB_POSTPROCESS_LATE);

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

static int mdw_plat_v6_check_sc_rets(struct mdw_cmd *c, int ipi_ret)
{
	uint64_t idx = c->einfos->c.sc_rets;
	int ret = ipi_ret;

	if (c->cmd_state == MDW_CMD_STATE_POSTPROCESS_DONE) {
		mdw_drv_debug("cmd already postprocess done\n");
		goto out;
	}

	if (idx >= c->num_subcmds) {
		mdw_drv_err("invalid subcmd index %llu\n", idx);
		return -EINVAL;
	}

	if (c->einfos->c.ret) {
		if (!ipi_ret)
			ret = -EIO;

		mdw_drv_err("sc(0x%llx-#%llu) type(%u) softlimit(%u) boost(%u) fail\n",
			c->kid, idx, c->subcmds[idx].type,
			c->softlimit, c->subcmds[idx].boost);
	}
	c->einfos->c.ret = ret;
out:
	return ret;
}

static int mdw_plat_v6_sc_sanity_check(struct mdw_cmd *c)
{
	unsigned int i = 0;

	mdw_flw_debug("\n");

	/* subcmd info */
	for (i = 0; i < c->num_subcmds; i++) {
		if (c->subcmds[i].type >= MDW_DEV_MAX ||
			c->subcmds[i].boost > MDW_BOOST_MAX ||
			c->subcmds[i].predecessors_start_idx > c->predecessors_num ||
			c->subcmds[i].predecessors_num >= c->num_subcmds ||
			c->subcmds[i].pack_friends_start_idx >= c->pack_friends_num ||
			c->subcmds[i].pack_friends_start_idx > c->pack_friends_num ||
			c->subcmds[i].pack_friends_num > c->num_subcmds) {
			mdw_drv_err("subcmd(%u) invalid (%u/%u/%u)(%u/%u|%u/%u)(%u/%u|%u/%u)\n",
				i, c->subcmds[i].type,
				c->subcmds[i].boost,
				c->subcmds[i].pack_id,
				c->subcmds[i].predecessors_start_idx,
				c->predecessors_num,
				c->subcmds[i].predecessors_num,
				c->num_subcmds,
				c->subcmds[i].pack_friends_start_idx,
				c->pack_friends_num,
				c->subcmds[i].pack_friends_num,
				c->num_subcmds);
			return -EINVAL;
		}
	}

	return 0;
}

static int mdw_plat_v6_cmd_sanity_check(struct mdw_cmd *c)
{
	int ret = -EINVAL;

	mdw_flw_debug("\n");

	/* check cmd params */
	if (c->priority >= MDW_PRIORITY_MAX ||
		c->num_links > c->num_subcmds ||
		c->end_vertices_num > c->num_subcmds) {
		mdw_drv_err("s(0x%llx)cmd invalid(0x%llx/0x%llx)(%u/%u/%u/%u)\n",
			(uint64_t)c->mpriv, c->uid, c->kid,
			c->priority, c->num_subcmds, c->num_links,
			c->end_vertices_num);
		goto out;
	}

	/* check exec infos */
	ret = mdw_sanity_einfo_check(c);
	if (ret) {
		mdw_drv_err("cmd sanity check: einfo error\n");
		goto out;
	}

	/* check execute order */
	ret = mdw_sanity_order_check(c);
	if (ret) {
		mdw_drv_err("cmd sanity check: order error\n");
		goto out;
	}

	/* check links */
	ret = mdw_sanity_link_check(c);
	if (ret) {
		mdw_drv_err("cmd sanity check: links error\n");
		goto out;
	}

	/* check subcmd info */
	ret = mdw_plat_v6_sc_sanity_check(c);
	if (ret) {
		mdw_drv_err("cmd sanity check: subcmd error\n");
		goto out;
	}

	ret = 0;
out:
	return ret;
}

const struct mdw_plat_func mdw_plat_func_v6 = {
	.late_init = mdw_plat_v6_late_init,
	.late_deinit = mdw_plat_v6_late_deinit,
	.sw_init = mdw_rv_sw_init,
	.sw_deinit = mdw_rv_sw_deinit,
	.set_power = mdw_rv_set_power,
	.ucmd = mdw_rv_ucmd,
	.set_param = mdw_rv_set_param,
	.get_info = mdw_rv_get_info,

	.register_device = mdw_plat_v6_register_device,
	.unregister_device = mdw_plat_v6_unregister_device,

	.create_session = mdw_plat_v6_create_session,
	.delete_session = mdw_plat_v6_delete_session,
	.create_cmd_priv = mdw_plat_v6_create_cmd_priv,
	.delete_cmd_priv = mdw_plat_v6_delete_cmd_priv,
	.run_cmd = mdw_plat_v6_run_cmd,
	.preprocess_cmd = mdw_plat_v6_preprocess_cmd,
	.postprocess_cmd = mdw_plat_v6_postprocess_cmd,
	.late_postprocess_cmd = mdw_plat_v6_late_postprocess_cmd,

	.check_sc_rets = mdw_plat_v6_check_sc_rets,
	.cmd_sanity_check = mdw_plat_v6_cmd_sanity_check,
};
