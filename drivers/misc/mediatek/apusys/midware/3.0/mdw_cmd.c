// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/uaccess.h>
#include <linux/sync_file.h>
#include <linux/sched/clock.h>

#include "mdw_trace.h"
#include "mdw_cmn.h"
#include "mdw_cmd.h"
#include "mdw_fence.h"
#include "mdw_mem_pool.h"
#include "mdw_rv_tag.h"

/* 64-bit execid : [world(4bit) | session_id(28bit) | counter(32bit)] */
#define mdw_world (1ULL)
#define MDW_CMD_GEN_INFID(session, cnt) ((mdw_world << 60) | ((session & 0xfffffff) << 32) \
	 | (cnt & 0xffffffff))

/* 64-bit sync info : [cbfc vid(32bit) | cbfc_en(16bit) | hse_en(16bit)] */
#define MDW_CMD_GEN_SYNC_INFO(vid, cbfc_en, hse_en) ((vid << 32) | ( cbfc_en << 16) \
	 | (hse_en))

static void mdw_cmd_cmdbuf_out(struct mdw_cmd *c)
{
	struct mdw_subcmd_kinfo *ksubcmd = NULL;
	struct mdw_fpriv *mpriv = c->mpriv;
	unsigned int i = 0, j = 0;

	if (!c->cmdbufs || c->cmd_state == MDW_CMD_STATE_ERROR
		|| c->cmd_state == MDW_CMD_STATE_POSTPROCESS_DONE)
		return;

	/* flush cmdbufs and execinfos */
	if (mdw_mem_invalidate(mpriv, c->cmdbufs))
		mdw_drv_warn("s(0x%llx)c(0x%llx) invalidate cmdbufs(%llu) fail\n",
			(uint64_t)mpriv, c->kid, c->cmdbufs->size);

	for (i = 0; i < c->num_subcmds; i++) {
		ksubcmd = &c->ksubcmds[i];
		for (j = 0; j < ksubcmd->info->num_cmdbufs; j++) {
			if (!ksubcmd->ori_cbs[j]) {
				mdw_drv_warn("no ori mems(%d-%d)\n", i, j);
				continue;
			}

			/* cmdbuf copy out */
			if (ksubcmd->cmdbufs[j].direction != MDW_CB_IN) {
				mdw_trace_begin("apumdw:cbs_copy_out|cb:%u-%u size:%llu type:%u",
					i, j, ksubcmd->ori_cbs[j]->size, ksubcmd->info->type);
				memcpy(ksubcmd->ori_cbs[j]->vaddr,
					(void *)ksubcmd->kvaddrs[j],
					ksubcmd->ori_cbs[j]->size);
				mdw_trace_end();
			}
		}
	}
}

static int mdw_cmd_preprocess(struct mdw_cmd *c)
{
	struct mdw_device *mdev = c->mpriv->mdev;
	int ret = 0;

	mdw_cmd_debug("\n");
	c->start_ts = sched_clock();
	c->cmd_state = MDW_CMD_STATE_RUN;

	ret = mdev->plat_funcs->preprocess_cmd(c);
	if (ret)
		mdw_drv_err("cmd plat preprocess failed\n");

	return ret;
}

static void mdw_cmd_update_einfos(struct mdw_cmd *c)
{
	if (c->cmd_state == MDW_CMD_STATE_POSTPROCESS_DONE) {
		mdw_flw_debug("Skip update einfos\n");
		return;
	}

	mdw_flw_debug("\n");
	c->end_ts = sched_clock();
	c->einfos->c.total_us = (c->end_ts - c->start_ts) / 1000;
	c->einfos->c.inference_id = c->inference_id;
}

static int mdw_cmd_postprocess(struct mdw_cmd *c, int ipi_ret)
{
	struct mdw_device *mdev = c->mpriv->mdev;
	int ret = 0;

	mdw_flw_debug("\n");

	/* post process such as copy exec info */
	if (mdev->plat_funcs->postprocess_cmd(c))
		mdw_drv_err("cmd postprocess failed\n");

	/* copy cmdbuf to user */
	mdw_cmd_cmdbuf_out(c);

	/* update einfos */
	mdw_cmd_update_einfos(c);

	/* check subcmds return value */
	ret = mdev->plat_funcs->check_sc_rets(c, ipi_ret);

	c->cmd_state = MDW_CMD_STATE_POSTPROCESS_DONE;

	return ret;
}

static void mdw_cmd_late_postprocess(struct mdw_cmd *c)
{
	struct mdw_device *mdev = c->mpriv->mdev;

	mdw_flw_debug("\n");

	if (mdev->plat_funcs->late_postprocess_cmd(c))
		mdw_drv_err("cmd late postprocess failed\n");

	/* reset cmd state */
	c->cmd_state = MDW_CMD_STATE_IDLE;
}

static void mdw_cmd_put_cmdbufs(struct mdw_fpriv *mpriv, struct mdw_cmd *c)
{
	struct mdw_subcmd_kinfo *ksubcmd = NULL;
	unsigned int i = 0, j = 0;

	if (!c->cmdbufs)
		return;

	mdw_trace_begin("apumdw:cbs_put|c:0x%llx num_subcmds:%u num_cmdbufs:%u",
		c->kid, c->num_subcmds, c->num_cmdbufs);

	for (i = 0; i < c->num_subcmds; i++) {
		ksubcmd = &c->ksubcmds[i];
		for (j = 0; j < ksubcmd->info->num_cmdbufs; j++) {
			if (!ksubcmd->ori_cbs[j]) {
				mdw_drv_warn("no ori mems(%d-%d)\n", i, j);
				continue;
			}

			/* put mem */
			ksubcmd->ori_cbs[j]->put(ksubcmd->ori_cbs[j]);
			ksubcmd->ori_cbs[j] = NULL;
		}
	}

	mdw_mem_pool_free(c->cmdbufs);
	c->cmdbufs = NULL;

	mdw_trace_end();
}

static int mdw_cmd_get_cmdbufs(struct mdw_fpriv *mpriv, struct mdw_cmd *c)
{
	unsigned int i = 0, j = 0, ofs = 0;
	int ret = -EINVAL;
	struct mdw_subcmd_kinfo *ksubcmd = NULL;
	struct mdw_mem *m = NULL;
	struct apusys_cmdbuf *acbs = NULL;

	mdw_trace_begin("apumdw:cbs_get|c:0x%llx num_subcmds:%u num_cmdbufs:%u",
		c->kid, c->num_subcmds, c->num_cmdbufs);

	if (!c->size_cmdbufs || c->cmdbufs)
		goto out;

	c->cmdbufs = mdw_mem_pool_alloc(&mpriv->cmd_buf_pool, c->size_cmdbufs,
		MDW_DEFAULT_ALIGN);
	if (!c->cmdbufs) {
		mdw_drv_err("s(0x%llx)c(0x%llx) alloc buffer for duplicate fail\n",
		(uint64_t) mpriv, c->kid);
		ret = -ENOMEM;
		goto out;
	}

	/* alloc mem for duplicated cmdbuf */
	for (i = 0; i < c->num_subcmds; i++) {
		mdw_cmd_debug("sc(0x%llx-%u) #cmdbufs(%u)\n",
			c->kid, i, c->ksubcmds[i].info->num_cmdbufs);

		acbs = kcalloc(c->ksubcmds[i].info->num_cmdbufs, sizeof(*acbs), GFP_KERNEL);
		if (!acbs)
			goto free_cmdbufs;

		ksubcmd = &c->ksubcmds[i];
		for (j = 0; j < ksubcmd->info->num_cmdbufs; j++) {
			/* calc align */
			if (ksubcmd->cmdbufs[j].align)
				ofs = MDW_ALIGN(ofs, ksubcmd->cmdbufs[j].align);
			else
				ofs = MDW_ALIGN(ofs, MDW_DEFAULT_ALIGN);

			mdw_cmd_debug("sc(0x%llx-%u) cb#%u offset(%u)\n",
				c->kid, i, j, ofs);

			/* get mem from handle */
			m = mdw_mem_get_mem_by_handle(mpriv, ksubcmd->cmdbufs[j].handle);
			if (!m) {
				mdw_drv_err("sc(0x%llx-%u) cb#%u(%llu) get fail\n",
					c->kid, i, j,
					ksubcmd->cmdbufs[j].handle);
				ret = -EINVAL;
				goto free_cmdbufs;
			}
			/* get refcnt */
			m->get(m);

			/* check mem boundary */
			if (m->vaddr == NULL ||
				ksubcmd->cmdbufs[j].size != m->size) {
				mdw_drv_err("sc(0x%llx-%u) cb#%u invalid range(%pK/%u/%llu)\n",
					c->kid, i, j, m->vaddr,
					ksubcmd->cmdbufs[j].size,
					m->size);
				ret = -EINVAL;
				goto free_cmdbufs;
			}

			/* cmdbuf copy in */
			if (ksubcmd->cmdbufs[j].direction != MDW_CB_OUT) {
				mdw_trace_begin("apumdw:cbs_copy_in|cb:%u-%u size:%u type:%u",
					i, j,
					ksubcmd->cmdbufs[j].size,
					ksubcmd->info->type);
				memcpy(c->cmdbufs->vaddr + ofs,
					m->vaddr,
					ksubcmd->cmdbufs[j].size);
				mdw_trace_end();
			}

			/* record buffer info */
			ksubcmd->ori_cbs[j] = m;
			ksubcmd->kvaddrs[j] =
				(uint64_t)(c->cmdbufs->vaddr + ofs);
			ksubcmd->daddrs[j] =
				(uint64_t)(c->cmdbufs->device_va + ofs);
			ofs += ksubcmd->cmdbufs[j].size;

			mdw_cmd_debug("sc(0x%llx-%u) cb#%u (0x%llx/0x%llx/%u)\n",
				c->kid, i, j,
				ksubcmd->kvaddrs[j],
				ksubcmd->daddrs[j],
				ksubcmd->cmdbufs[j].size);

			acbs[j].kva = (void *)ksubcmd->kvaddrs[j];
			acbs[j].size = ksubcmd->cmdbufs[j].size;
		}

		mdw_trace_begin("apumdw:dev validation|c:0x%llx type:%u",
			c->kid, ksubcmd->info->type);
		ret = mdw_dev_validation(mpriv, ksubcmd->info->type,
			c, acbs, ksubcmd->info->num_cmdbufs);
		mdw_trace_end();
		kfree(acbs);
		acbs = NULL;
		if (ret) {
			mdw_drv_err("sc(0x%llx-%u) dev(%u) validate cb(%u) fail(%d)\n",
				c->kid, i, ksubcmd->info->type, ksubcmd->info->num_cmdbufs, ret);
			goto free_cmdbufs;
		}
	}

	/* flush cmdbufs */
	if (mdw_mem_flush(mpriv, c->cmdbufs))
		mdw_drv_warn("s(0x%llx) c(0x%llx) flush cmdbufs(%llu) fail\n",
			(uint64_t)mpriv, c->kid, c->cmdbufs->size);

	ret = 0;
	goto out;

free_cmdbufs:
	mdw_cmd_put_cmdbufs(mpriv, c);
	kfree(acbs);
out:
	mdw_cmd_debug("ret(%d)\n", ret);
	mdw_trace_end();
	return ret;
}

static int mdw_cmd_create_infos(struct mdw_fpriv *mpriv, struct mdw_cmd *c)
{
	unsigned int i = 0, j = 0, total_size = 0, tmp_size = 0;
	struct mdw_subcmd_exec_info *sc_einfo = NULL;
	int ret = -ENOMEM;

	c->einfos = c->exec_infos->vaddr;
	if (!c->einfos) {
		mdw_drv_err("invalid exec info addr\n");
		return -EINVAL;
	}
	/* clear run infos for return */
	memset(c->exec_infos->vaddr, 0, c->exec_infos->size);

	/* alloc kernel subcmd related structure */
	c->ksubcmds = kzalloc(c->num_subcmds * sizeof(*c->ksubcmds),
		GFP_KERNEL);
	if (!c->ksubcmds) {
		mdw_drv_err("alloc kernel subcmd failed\n");
		ret = -ENOMEM;
		goto out;
	}

	sc_einfo = &c->einfos->sc;

	for (i = 0; i < c->num_subcmds; i++) {
		c->ksubcmds[i].info = &c->subcmds[i];
		mdw_cmd_debug("subcmd(%u)(%u/%u/%u/%u/%u/%u/%u/%u/0x%llx)\n",
			i, c->subcmds[i].type,
			c->subcmds[i].suggest_time, c->subcmds[i].vlm_usage,
			c->subcmds[i].vlm_ctx_id, c->subcmds[i].vlm_force,
			c->subcmds[i].boost, c->subcmds[i].pack_id,
			c->subcmds[i].num_cmdbufs, c->subcmds[i].cmdbufs);

		/* kva for oroginal buffer */
		c->ksubcmds[i].ori_cbs = kcalloc(c->subcmds[i].num_cmdbufs,
			sizeof(c->ksubcmds[i].ori_cbs), GFP_KERNEL);
		if (!c->ksubcmds[i].ori_cbs)
			goto free_cmdbufs;

		/* record kva for duplicate */
		c->ksubcmds[i].kvaddrs = kcalloc(c->subcmds[i].num_cmdbufs,
			sizeof(*c->ksubcmds[i].kvaddrs), GFP_KERNEL);
		if (!c->ksubcmds[i].kvaddrs)
			goto free_cmdbufs;

		/* record dva for cmdbufs */
		c->ksubcmds[i].daddrs = kcalloc(c->subcmds[i].num_cmdbufs,
			sizeof(*c->ksubcmds[i].daddrs), GFP_KERNEL);
		if (!c->ksubcmds[i].daddrs)
			goto free_cmdbufs;

		/* allocate for subcmd cmdbuf */
		c->ksubcmds[i].cmdbufs = kcalloc(c->subcmds[i].num_cmdbufs,
			sizeof(*c->ksubcmds[i].cmdbufs), GFP_KERNEL);
		if (!c->ksubcmds[i].cmdbufs)
			goto free_cmdbufs;

		/* copy cmdbuf info */
		if (copy_from_user(c->ksubcmds[i].cmdbufs,
			(void __user *)c->subcmds[i].cmdbufs,
			c->subcmds[i].num_cmdbufs *
			sizeof(*c->ksubcmds[i].cmdbufs))) {
			goto free_cmdbufs;
		}

		c->ksubcmds[i].sc_einfo = &sc_einfo[i];

		/* accumulate cmdbuf size with alignment */
		for (j = 0; j < c->subcmds[i].num_cmdbufs; j++) {
			c->num_cmdbufs++;
			/* alignment */
			if (c->ksubcmds[i].cmdbufs[j].align) {
				tmp_size = MDW_ALIGN(total_size,
					c->ksubcmds[i].cmdbufs[j].align);
			} else {
				tmp_size = MDW_ALIGN(total_size, MDW_DEFAULT_ALIGN);
			}
			if (tmp_size < total_size) {
				mdw_drv_err("cmdbuf(%u,%u) size align overflow(%u/%u/%u)\n",
					i, j, total_size,
					c->ksubcmds[i].cmdbufs[j].align, tmp_size);
				goto free_cmdbufs;
			}
			total_size = tmp_size;

			/* accumulator */
			tmp_size = total_size + c->ksubcmds[i].cmdbufs[j].size;
			if (tmp_size < total_size) {
				mdw_drv_err("cmdbuf(%u,%u) size overflow(%u/%u/%u)\n",
					i, j, total_size,
					c->ksubcmds[i].cmdbufs[j].size, tmp_size);
				goto free_cmdbufs;
			}
			total_size = tmp_size;
		}
	}
	/* align cmdbuf tail offset for apummu table */
	tmp_size = MDW_ALIGN(total_size, MDW_DEFAULT_ALIGN);
	if (tmp_size < total_size) {
		mdw_drv_err("cmdbuf end size align overflow(%u/%u)\n",
			total_size, tmp_size);
		goto free_cmdbufs;
	}
	total_size = tmp_size;
	c->size_cmdbufs = total_size;

	mdw_cmd_debug("sc(0x%llx) cb_num(%u) total size(%u)\n",
		c->kid, c->num_cmdbufs, c->size_cmdbufs);

	mdw_drv_debug("[todo] remove apummutable\n");

	ret = mdw_cmd_get_cmdbufs(c->mpriv, c);
	if (ret)
		goto free_cmdbufs;

	goto out;

free_cmdbufs:
	for (i = 0; i < c->num_subcmds; i++) {
		/* free dvaddrs */
		kfree(c->ksubcmds[i].daddrs);
		c->ksubcmds[i].daddrs = NULL;

		/* free kvaddrs */
		kfree(c->ksubcmds[i].kvaddrs);
		c->ksubcmds[i].kvaddrs = NULL;

		/* free ori kvas */
		kfree(c->ksubcmds[i].ori_cbs);
		c->ksubcmds[i].ori_cbs = NULL;

		/* free cmdbufs */
		kfree(c->ksubcmds[i].cmdbufs);
		c->ksubcmds[i].cmdbufs = NULL;
	}

	kfree(c->ksubcmds);
out:
	return ret;
}

static void mdw_cmd_delete_infos(struct mdw_fpriv *mpriv, struct mdw_cmd *c)
{
	unsigned int i = 0;

	mdw_cmd_put_cmdbufs(mpriv, c);

	for (i = 0; i < c->num_subcmds; i++) {
		/* free dvaddrs */
		kfree(c->ksubcmds[i].daddrs);
		c->ksubcmds[i].daddrs = NULL;

		/* free kvaddrs */
		kfree(c->ksubcmds[i].kvaddrs);
		c->ksubcmds[i].kvaddrs = NULL;

		/* free ori kvas */
		kfree(c->ksubcmds[i].ori_cbs);
		c->ksubcmds[i].ori_cbs = NULL;

		/* free cmdbufs */
		kfree(c->ksubcmds[i].cmdbufs);
		c->ksubcmds[i].cmdbufs = NULL;
	}
}

void mdw_cmd_unvoke_map(struct mdw_cmd *c)
{
	struct mdw_cmd_map_invoke *cm_invoke = NULL, *tmp = NULL;

	mutex_lock(&c->cm_mtx);
	list_for_each_entry_safe(cm_invoke, tmp, &c->map_invokes, c_node) {
		list_del(&cm_invoke->c_node);
		mdw_cmd_debug("s(0x%llx)c(0x%llx) unvoke m(0x%llx/%llu)\n",
			(uint64_t)c->mpriv, (uint64_t)c,
			cm_invoke->map->device_va,
			cm_invoke->map->size);
		cm_invoke->map->put(cm_invoke->map);
		kfree(cm_invoke);
	}
	mutex_unlock(&c->cm_mtx);
}

int mdw_cmd_invoke_map(struct mdw_cmd *c, struct mdw_mem_map *map)
{
	struct mdw_cmd_map_invoke *cm_invoke = NULL;
	int ret = 0;

	if (map == NULL)
		return -EINVAL;

	mutex_lock(&c->cm_mtx);
	/* query */
	list_for_each_entry(cm_invoke, &c->map_invokes, c_node) {
		/* already invoked */
		if (cm_invoke->map == map) {
			mdw_cmd_debug("s(0x%llx)c(0x%llx)m(0x%llx/%llu) already invoked\n",
				(uint64_t)c->mpriv, (uint64_t)c, map->device_va, map->size);
			goto unlock;
		}
	}

	cm_invoke = kzalloc(sizeof(*cm_invoke), GFP_KERNEL);
	if (cm_invoke == NULL) {
		mdw_drv_err("s(0x%llx)c(0x%llx)m(0x%llx/%llu) create cm_invoke failed\n",
			(uint64_t)c->mpriv, (uint64_t)c, map->device_va, map->size);
		ret = -ENOMEM;
		goto unlock;
	}

	map->get(map);
	cm_invoke->map = map;
	list_add_tail(&cm_invoke->c_node, &c->map_invokes);

	mdw_cmd_debug("s(0x%llx)c(0x%llx) invoke m(0x%llx/%llu)\n",
		(uint64_t)c->mpriv, (uint64_t)c, map->device_va, map->size);

unlock:
	mutex_unlock(&c->cm_mtx);
	return ret;
}

static void mdw_cmd_release(struct kref *ref)
{
	struct mdw_cmd *c =
			container_of(ref, struct mdw_cmd, ref);
	struct mdw_fpriv *mpriv = c->mpriv;

	mdw_cmd_show(c, mdw_drv_debug);
	mdw_trace_begin("apumdw:cmd_release|inf_id(0x%08x,0x%08x)",
			 (uint32_t)(c->inference_id >> 32),
			 (uint32_t)(c->inference_id & 0xFFFFFFFF));
	mdw_cmd_unvoke_map(c);
	if (mpriv->mdev->plat_funcs->delete_cmd_priv(c))
		mdw_drv_err("delete plat priv failed\n");
	mdw_cmd_delete_infos(c->mpriv, c);
	c->exec_infos->put(c->exec_infos);
	kfree(c->end_vertices);
	kfree(c->pack_friends);
	kfree(c->predecessors);
	kfree(c->execute_orders);
	kfree(c->links);
	kfree(c->adj_matrix);
	kfree(c->ksubcmds);
	kfree(c->subcmds);
	kfree(c);

	mpriv->put_ref(mpriv);
	mdw_trace_end();
}

static void mdw_cmd_put(struct mdw_cmd *c)
{
	mdw_cmd_debug("before(0x%llx) ref put ref(%u)\n",
		c->kid, kref_read(&c->ref));
	kref_put(&c->ref, mdw_cmd_release);
}

static void mdw_cmd_get(struct mdw_cmd *c)
{
	kref_get(&c->ref);
	mdw_cmd_debug("cmd(0x%llx) after ref get ref(%u)\n",
		c->kid, kref_read(&c->ref));
}

static void mdw_cmd_delete(struct mdw_cmd *c)
{
	mdw_cmd_show(c, mdw_drv_debug);
	c->put_ref(c);
}

static void mdw_cmd_delete_async(struct mdw_cmd *c)
{
	struct mdw_device *mdev = c->mpriv->mdev;
	mdw_drv_debug("\n");

	/* add cmd list to delete */
	mutex_lock(&mdev->c_mtx);
	list_add_tail(&c->d_node, &mdev->d_cmds);
	mutex_unlock(&mdev->c_mtx);
	mdw_drv_debug("\n");

	schedule_work(&mdev->c_wk);
}

static int mdw_cmd_sanity_check(struct mdw_cmd *c)
{
	int ret = -EINVAL;
	struct mdw_device *mdev = c->mpriv->mdev;

	mdw_trace_begin("apumdw:cmd_sanity_check|c:0x%llx", (uint64_t)c);
	mdw_cmd_debug("num_subcmd(%u) c_execinfo_size(%lu) sc_execinfo_size(%lu)\n",
		c->num_subcmds, sizeof(struct mdw_cmd_exec_info), sizeof(struct mdw_subcmd_exec_info));

	ret = mdev->plat_funcs->cmd_sanity_check(c);
	mdw_trace_end();

	return ret;
}

static int mdw_cmd_run(struct mdw_cmd *c)
{
	struct mdw_fpriv *mpriv = c->mpriv;
	struct mdw_device *mdev = mpriv->mdev;
	struct dma_fence *f = NULL;
	int ret = 0;

	mdw_cmd_show(c, mdw_cmd_debug);

	f = &c->fence->base_fence;

	mdw_cmd_preprocess(c);
	ret = mdev->plat_funcs->run_cmd(c);
	if (ret < 0) {
		mdw_drv_err("run cmd fail, ret(%d)\n", ret);
		c->cmd_state = MDW_CMD_STATE_ERROR;
		mdw_cmd_postprocess(c, 0);

		/* setup fence return value */
		dma_fence_set_error(f, ret);

		/* inform user space */
		if (dma_fence_signal(f)) {
			mdw_drv_warn("signal fence fail\n");
			if (f->ops->get_timeline_name && f->ops->get_driver_name) {
				mdw_drv_warn(" fence name(%s-%s)\n",
				f->ops->get_driver_name(f), f->ops->get_timeline_name(f));
			}
		}
		mdw_cmd_late_postprocess(c);
	}

	return ret;
}

static int mdw_cmd_complete(struct mdw_cmd *c, int ipi_ret)
{
	struct dma_fence *f = &c->fence->base_fence;
	struct mdw_fpriv *mpriv = c->mpriv;
	uint64_t ts1 = 0, ts2 = 0;
	int ret = 0;

	ts1 = sched_clock();
	mdw_trace_begin("apumdw:cmd_complete|cmd:uid(0x%llx) inf_id(0x%08x,0x%08x)",
			 c->uid,
			 (uint32_t)(c->inference_id >> 32),
			 (uint32_t)(c->inference_id & 0xFFFFFFFF));
	mutex_lock(&c->mtx);
	ts2 = sched_clock();
	c->enter_complt_time = ts2 - ts1;

	/* handle cmdbuf for user */
	ret = mdw_cmd_postprocess(c, ipi_ret);
	ts1 = sched_clock();
	c->cmdbuf_out_time = ts1 - ts2;

	mdw_flw_debug("s(0x%llx) c(%s/0x%llx/0x%llx/0x%llx) ipi_ret(%d) ret(%d) sc_rets(0x%llx) complete, pid(%d/%d)(%d)\n",
		(uint64_t)mpriv, c->comm, c->uid, c->kid, c->inference_id,
		ipi_ret, ret, c->einfos->c.sc_rets,
		c->pid, c->tgid, task_pid_nr(current));

	if (ret) {
		mdw_drv_err("s(0x%llx) c(%s/0x%llx/0x%llx/0x%llx) ret(%d/0x%llx) time(%llu) pid(%d/%d)\n",
			(uint64_t)mpriv, c->comm, c->uid, c->kid, c->inference_id,
			ret, c->einfos->c.sc_rets,
			c->einfos->c.total_us, c->pid, c->tgid);
		dma_fence_set_error(f, ret);

		if (mdw_debug_on(MDW_DBG_EXP))
			mdw_exception("exec fail\n");
	} else {
		mdw_flw_debug("s(0x%llx) c(%s/0x%llx/0x%llx/0x%llx) ret(%d/0x%llx) time(%llu) pid(%d/%d)\n",
			(uint64_t)mpriv, c->comm, c->uid, c->kid, c->inference_id,
			ret, c->einfos->c.sc_rets,
			c->einfos->c.total_us, c->pid, c->tgid);
	}

	/* signal done */
	c->fence = NULL;
	if (dma_fence_signal(f)) {
		mdw_drv_warn("c(0x%llx) signal fence fail\n", (uint64_t)c);
		if (f->ops->get_timeline_name && f->ops->get_driver_name) {
			mdw_drv_warn(" fence name(%s-%s)\n",
			f->ops->get_driver_name(f), f->ops->get_timeline_name(f));
		}
	}
	dma_fence_put(f);
	ts2 = sched_clock();
	c->handle_cmd_result_time = ts2 - ts1;
	mdw_flw_debug("c(0x%llx) signal done\n", c->kid);

	/* late post process */
	mdw_cmd_late_postprocess(c);

	mdw_flw_debug("c(0x%llx) complete done\n", c->kid);
	mutex_unlock(&c->mtx);
	up(&c->exec_sem);
	mdw_cmd_deque_trace(c, MDW_CMD_DEQUE);

	/* put cmd execution ref */
	c->put_ref(c);
	mdw_trace_end();

	return 0;
}

static void mdw_cmd_trigger_func(struct work_struct *wk)
{
	struct mdw_cmd *c =
		container_of(wk, struct mdw_cmd, t_wk);
	int ret = 0;

	if (c->wait_fence) {
		dma_fence_wait(c->wait_fence, false);
		dma_fence_put(c->wait_fence);
		mdw_flw_debug("s(0x%llx) c(0x%llx) inf_id(0x%llx) wait fence done, start run\n",
			(uint64_t)c->mpriv, c->kid, c->inference_id);
	} else {
		mdw_flw_debug("no fence to wait, trigger directly\n");
	}

	mutex_lock(&c->mtx);
	ret = mdw_cmd_run(c);
	if (ret < 0) {
		mdw_drv_err("run cmd fail\n");
		mdw_fence_delete(c);
		c->put_ref(c);
	}
	mutex_unlock(&c->mtx);
}

static struct mdw_cmd *mdw_cmd_create(struct mdw_fpriv *mpriv,
	struct mdw_cmd_in *in)
{
	struct mdw_cmd *c = NULL;

	mdw_trace_begin("apumdw:cmd_create|s:0x%llx", (uint64_t)mpriv);

	/* alloc mdw cmd */
	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		goto out;

	mutex_init(&c->mtx);
	mutex_init(&c->cm_mtx);
	INIT_LIST_HEAD(&c->map_invokes);
	c->mpriv = mpriv;
	//atomic_set(&c->is_running, 0);

	/* setup cmd info */
	c->pid = task_pid_nr(current);
	c->tgid = task_tgid_nr(current);
	c->kid = (uint64_t)c;
	c->uid = in->exec.uid;
	c->u_pid = in->exec.u_pid;
	get_task_comm(c->comm, current);
	c->priority = in->exec.priority;
	c->hardlimit = in->exec.hardlimit;
	c->softlimit = in->exec.softlimit;
	c->power_save = in->exec.power_save;
	c->power_plcy = in->exec.power_plcy;
	c->power_dtime = in->exec.power_dtime;
	c->power_etime = in->exec.power_etime;
	c->fastmem_ms = in->exec.fastmem_ms;
	c->app_type = in->exec.app_type;
	c->hse_num = in->exec.hse_num;
	c->function_bitmask = in->exec.function_bitmask;
	c->num_subcmds = in->exec.num_subcmds;
	c->inference_ms = in->exec.inference_ms;
	c->tolerance_ms = in->exec.tolerance_ms;
	c->is_dtime_set = in->exec.is_dtime_set;
	c->num_links = in->exec.num_links;
	c->auto_dvfs_target_time = in->exec.auto_dvfs_target_time;
	c->predecessors_num = in->exec.predecessors_num;
	if (check_mul_overflow(c->predecessors_num, sizeof(*c->predecessors), &c->predecessors_size)) {
		mdw_drv_err("predecessors_num overflow\n");
		goto free_cmd;
	}
	c->pack_friends_num = in->exec.pack_friends_num;
	if (check_mul_overflow(c->pack_friends_num, sizeof(*c->pack_friends), &c->pack_friends_size)) {
		mdw_drv_err("pack_friends_num overflow\n");
		goto free_cmd;
	}
	c->end_vertices_num = in->exec.end_vertices_num;
	if (check_mul_overflow(c->end_vertices_num, sizeof(*c->end_vertices), &c->end_vertices_size)) {
		mdw_drv_err("end_vertices_num overflow\n");
		goto free_cmd;
	}
	/* callback functions */
	c->complete = mdw_cmd_complete;
	c->get_ref = mdw_cmd_get;
	c->put_ref = mdw_cmd_put;

	/* get exec info buffer */
	c->exec_infos = mdw_mem_get_mem_by_handle(mpriv, in->exec.exec_infos);
	if (!c->exec_infos) {
		mdw_drv_err("get exec info fail\n");
		goto free_cmd;
	}
	/* get refcnt */
	c->exec_infos->get(c->exec_infos);

	/* subcmds */
	c->subcmds = kzalloc(c->num_subcmds * sizeof(*c->subcmds), GFP_KERNEL);
	if (!c->subcmds)
		goto put_execinfos;

	if (copy_from_user(c->subcmds, (void __user *)in->exec.subcmd_infos,
		c->num_subcmds * sizeof(*c->subcmds))) {
		mdw_drv_err("copy subcmds fail\n");
		goto free_subcmds;
	}

	/* adj matrix */
	if (in->exec.adj_matrix) {
		mdw_cmd_debug("copy adj_matrix\n");
		c->adj_matrix = kzalloc(c->num_subcmds *
			c->num_subcmds * sizeof(*c->adj_matrix), GFP_KERNEL);

		if (copy_from_user(c->adj_matrix, (void __user *)in->exec.adj_matrix,
			(c->num_subcmds * c->num_subcmds * sizeof(*c->adj_matrix)))) {
			mdw_drv_err("copy adj matrix fail\n");
			goto free_adj;
		}

		if (mdw_debug_on(MDW_DBG_CMD)) {
			print_hex_dump(KERN_INFO, "[apusys] adj matrix: ",
				DUMP_PREFIX_OFFSET, 16, 1, c->adj_matrix,
				c->num_subcmds * c->num_subcmds, 0);
		}
	}

	/* link */
	if (c->num_links) {
		mdw_cmd_debug("copy links(%u)\n", c->num_links);
		c->links = kcalloc(c->num_links, sizeof(*c->links), GFP_KERNEL);
		if (!c->links)
			goto free_adj;
		if (copy_from_user(c->links, (void __user *)in->exec.links,
			c->num_links * sizeof(*c->links))) {
			mdw_drv_err("copy links fail\n");
			goto free_link;
		}
	}

	/* execute order */
	if (in->exec.execute_orders) {
		mdw_cmd_debug("copy execute_orders\n");
		c->execute_orders = kcalloc(c->num_subcmds, sizeof(*c->execute_orders), GFP_KERNEL);
		if  (!c->execute_orders) {
			mdw_drv_err("allocate execute_orders fail\n");
			goto free_link;
		}

		if (copy_from_user(c->execute_orders, (void __user *)in->exec.execute_orders,
			(c->num_subcmds * sizeof(*c->execute_orders)))) {
			mdw_drv_err("copy execute_orders fail\n");
			goto free_exec_orders;
		}
	}

	/* predecessors */
	if (c->predecessors_num) {
		mdw_cmd_debug("copy predecessors(%u)\n", c->predecessors_num);
		c->predecessors = kcalloc(c->predecessors_num, sizeof(*c->predecessors), GFP_KERNEL);
		if (!c->predecessors) {
			mdw_drv_err("allocate predecessors fail\n");
			goto free_exec_orders;
		}

		if (copy_from_user(c->predecessors, (void __user *)in->exec.predecessors,
			c->predecessors_size)) {
			mdw_drv_err("copy predecessors fail\n");
			goto free_predecessors;
		}
	}

	/* pack friends */
	if (c->pack_friends_num) {
		mdw_cmd_debug("copy pack_friends(%u)\n", c->pack_friends_num);
		c->pack_friends = kcalloc(c->pack_friends_num, sizeof(*c->pack_friends), GFP_KERNEL);
		if (!c->pack_friends) {
			mdw_drv_err("allocate pack_friends fail\n");
			goto free_predecessors;
		}

		if (copy_from_user(c->pack_friends, (void __user *)in->exec.pack_friends,
			c->pack_friends_size)) {
			mdw_drv_err("copy pack_friends fail\n");
			goto free_pack_friends;
		}
	}

	/* end leaf */
	if (c->end_vertices_num) {
		mdw_cmd_debug("copy end vertices(%u)\n", c->end_vertices_num);
		c->end_vertices = kcalloc(c->end_vertices_num, sizeof(*c->end_vertices), GFP_KERNEL);
		if (!c->end_vertices) {
			mdw_drv_err("alloc end_vertices failed\n");
			goto free_pack_friends;
		}

		if (copy_from_user(c->end_vertices, (void __user *)in->exec.end_vertices, c->end_vertices_size)) {
			mdw_drv_err("copy end_vertices failed\n");
			goto free_end_vertices;
		}
	}

	/* check input params */
	if (mdw_cmd_sanity_check(c)) {
		mdw_drv_err("cmd sanity check fail\n");
		goto free_link;
	}

	/* cmdbufs */
	if (mdw_cmd_create_infos(c->mpriv, c)) {
		mdw_drv_err("create cmd infos fail\n");
		goto free_pack_friends;
	}

	/* priv */
	if (mpriv->mdev->plat_funcs->create_cmd_priv(c)) {
		mdw_drv_err("create platform priv fail\n");
		goto delete_infos;
	}

	c->mpriv->get_ref(c->mpriv);

	INIT_WORK(&c->t_wk, &mdw_cmd_trigger_func);
	sema_init(&c->exec_sem, 1);
	kref_init(&c->ref);

	mdw_cmd_show(c, mdw_drv_debug);

	goto out;

delete_infos:
	mdw_cmd_delete_infos(c->mpriv, c);
free_end_vertices:
	kfree(c->end_vertices);
free_pack_friends:
	kfree(c->pack_friends);
free_predecessors:
	kfree(c->predecessors);
free_exec_orders:
	kfree(c->execute_orders);
free_link:
	kfree(c->links);
free_adj:
	kfree(c->adj_matrix);
free_subcmds:
	kfree(c->subcmds);
put_execinfos:
	c->exec_infos->put(c->exec_infos);
free_cmd:
	kfree(c);
	c = NULL;
out:
	mdw_trace_end();
	return c;
}

static int mdw_cmd_ioctl_run(struct mdw_fpriv *mpriv, union mdw_cmd_args *args)
{
	struct mdw_cmd_in *in = (struct mdw_cmd_in *)args;
	struct mdw_cmd *c = NULL, *prev_c = NULL;
	struct sync_file *sync_file = NULL;
	struct dma_fence *f = NULL;
	int ret = 0, fd = 0, wait_fd = 0, run_ret = 0;
	unsigned long timeout = msecs_to_jiffies(MDW_CMD_TIMEOUT_MS);

	mdw_trace_begin("apumdw:user_run");

	/* get wait fd */
	wait_fd = in->exec.fence;

	mutex_lock(&mpriv->mtx);

	prev_c = (struct mdw_cmd *)idr_find(&mpriv->cmds, in->id);

	/* delete stale cmd if not run_stale */
	if (in->op != MDW_CMD_IOCTL_RUN_STALE && prev_c) {
		if (prev_c != idr_remove(&mpriv->cmds, prev_c->id))
			mdw_drv_warn("remove id(%d) conflict\n", prev_c->id);
		mdw_cmd_delete_async(prev_c);
	}

	/* create new cmd for enq and run */
	if (in->op == MDW_CMD_IOCTL_ENQ || in->op == MDW_CMD_IOCTL_RUN) {
		c = mdw_cmd_create(mpriv, in);
		if (IS_ERR_OR_NULL(c)) {
			ret = -EINVAL;
			mdw_drv_err("create cmd fail, ret(%d)\n", ret);
			goto out;
		}
		/* alloc idr */
		c->id = idr_alloc_cyclic(&mpriv->cmds, c, MDW_CMD_IDR_MIN, MDW_CMD_IDR_MAX, GFP_KERNEL);
		if (c->id < MDW_CMD_IDR_MIN) {
			mdw_drv_err("alloc idr fail\n");
			ret = -EUSERS;
			goto delete_cmd;
		}
		/* return input fence wait_fd (enq does NOT use fence) */
		fd = wait_fd;
	} else if (in->op == MDW_CMD_IOCTL_RUN_STALE && prev_c) {
		/* run stale, set prev as current cmd */
		c = prev_c;
		prev_c = NULL;
		mdw_cmd_debug("run stale mode\n");
	} else {
		mdw_drv_err("unexcept operation op(%d) id(%lld)\n", in->op, in->id);
		ret = -EINVAL;
		goto out;
	}

	/* run_cmd for run and run_stale */
	if (in->op == MDW_CMD_IOCTL_RUN || in->op == MDW_CMD_IOCTL_RUN_STALE) {
		/* get ref cnt */
		c->get_ref(c);

		/* take semaphore */
		if (down_trylock(&c->exec_sem)) {
			mutex_unlock(&mpriv->mtx);
			ret = down_timeout(&c->exec_sem, timeout);
			mutex_lock(&mpriv->mtx);
			if (ret) {
				mdw_drv_err("s(0x%llx) inf_id(0x%llx) take semaphore timeout\n",
						(uint64_t)c->mpriv, c->inference_id);
				c->put_ref(c);
				goto out;
			}
		}
		mutex_lock(&c->mtx);

		/* prepare sync_file fd */
		fd = get_unused_fd_flags(O_CLOEXEC);
		if (fd < 0) {
			mdw_drv_err("get unused fd(%d) fail\n", fd);
			goto remove_idr;
		}

		/* prepare fence */
		ret = mdw_fence_init(c);
		if (ret) {
			mdw_drv_err("create fence fail, ret(%d)\n", ret);
			goto put_fd;
		}

		/* assign dma_fence */
		mdw_drv_debug("fence(%pK/%pK)\n", c->fence, &c->fence->base_fence);
		f = &c->fence->base_fence;

		/* create sync file*/
		sync_file = sync_file_create(&c->fence->base_fence);
		if (!sync_file) {
			mdw_drv_err("create sync file fail\n");
			ret = -ENOMEM;
			goto delete_fence;
		}

		/* generate cmd inference id */
		c->inference_id = MDW_CMD_GEN_INFID((uint64_t) mpriv, mpriv->counter++);

		mdw_cmd_trace(c, MDW_CMD_ENQUE);

		/* check wait fence from other module */
		mdw_flw_debug("s(0x%llx) inf_id(0x%llx) wait fence(%d)...\n",
				(uint64_t)c->mpriv, c->inference_id, wait_fd);

		c->wait_fence = sync_file_get_fence(wait_fd);
		if (!c->wait_fence) {
			mdw_flw_debug("s(0x%llx) inf_id(0x%llx) no wait fence, trigger directly\n",
				(uint64_t)c->mpriv, c->inference_id);
			run_ret = mdw_cmd_run(c);
		} else {
			/* wait fence from wq */
			schedule_work(&c->t_wk);

			mdw_flw_debug("wait fence, fd(%d)\n", wait_fd);
			if (c->wait_fence->ops->get_timeline_name && c->wait_fence->ops->get_driver_name) {
				mdw_flw_debug("wait fence, fence name(%s-%s)\n",
					c->wait_fence->ops->get_driver_name(f),
					c->wait_fence->ops->get_timeline_name(f));
			}
		}

		if (run_ret < 0) {
			mdw_drv_err("run cmd fail, ret(%d)\n", run_ret);
			ret = run_ret;
			goto put_sync_file;
		} else if (run_ret > 0) {
			mdw_cmd_debug("poll done, ret(%d)\n", run_ret);
			mdw_cmd_postprocess(c, 0);
		}

		/* assign sync file to fd */
		fd_install(fd, sync_file->file);
	}

	memset(args, 0, sizeof(*args));

	/* return fd */
	args->out.exec.fence = fd;
	args->out.exec.id = c->id;
	if (run_ret > 0)
		args->out.exec.cmd_done_usr = true;
	args->out.exec.ext_id = c->ext_id;
	mdw_flw_debug("async fd(%d) id(%d) extid(0x%llx) inference_id(0x%llx)\n",
			 fd, c->id, c->ext_id, c->inference_id);

	mutex_unlock(&c->mtx);
	goto out;

put_sync_file:
	fput(sync_file->file);
delete_fence:
	mdw_fence_delete(c);
put_fd:
	put_unused_fd(fd);
remove_idr:
	if (c != idr_remove(&mpriv->cmds, c->id))
		mdw_drv_warn("remove id(%d) conflict\n", c->id);
	mutex_unlock(&c->mtx);
	up(&c->exec_sem);
delete_cmd:
	mdw_cmd_delete(c);
out:
	mutex_unlock(&mpriv->mtx);

	mdw_trace_end();

	return ret;
}

void mdw_cmd_release_session(struct mdw_fpriv *mpriv)
{
	struct mdw_cmd *c = NULL;
	void *entry = NULL;
	int id = 0;

	idr_for_each_entry(&mpriv->cmds, entry, id) {
		c = idr_remove(&mpriv->cmds, id);
		if (!c) {
			mdw_drv_warn("invalidate cmd id(%d)\n", id);
		} else {
			mdw_cmd_debug("remove redundant c id(%d)\n", id);
			mdw_cmd_delete(c);
		}
	}
}

static int mdw_cmd_ioctl_del(struct mdw_fpriv *mpriv, union mdw_cmd_args *args)
{
	struct mdw_cmd_in *in = (struct mdw_cmd_in *)args;
	struct mdw_cmd *c = NULL;
	int ret = 0;

	mdw_trace_begin("apumdw:user_delete");

	mutex_lock(&mpriv->mtx);
	c = idr_remove(&mpriv->cmds, in->id);
	if (!c)
		mdw_drv_warn("remove cmd idr conflict(%lld)\n", in->id);
	else
		mdw_cmd_delete(c);
	mutex_unlock(&mpriv->mtx);

	mdw_trace_end();

	return ret;
}

int mdw_cmd_ioctl(struct mdw_fpriv *mpriv, void *data)
{
	union mdw_cmd_args *args = (union mdw_cmd_args *)data;
	int ret = 0;

	mdw_flw_debug("s(0x%llx) op::%d\n", (uint64_t)mpriv, args->in.op);

	switch (args->in.op) {
	case MDW_CMD_IOCTL_RUN:
	case MDW_CMD_IOCTL_RUN_STALE:
	case MDW_CMD_IOCTL_ENQ:
		ret = mdw_cmd_ioctl_run(mpriv, args);

		break;
	case MDW_CMD_IOCTL_DEL:
		ret = mdw_cmd_ioctl_del(mpriv, args);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	mdw_flw_debug("done\n");

	return ret;
}
