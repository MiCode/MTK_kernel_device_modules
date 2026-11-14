// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/hashtable.h>
#include "apusys_device.h"
#include "mdw_cmn.h"
#include "mdw_cb_appendix.h"
#include "mdw.h"

struct cb_appendix_info {
	/* callback functions from caller */
	enum apu_appendix_cb_owner owner;
	cb_apu_appendix_cmdbuf_size cb_size;
	cb_apu_appendix_cmdbuf_process cb_process;

	/* internal params */
	uint32_t idx;
	struct hlist_node hash_node;
};

static struct mutex g_acb_mtx;
static uint32_t g_idx_count;
static bool mdw_appendix_inited;
DECLARE_HASHTABLE(g_acb_hash, 4);

uint32_t mdw_cb_appendix_num(void)
{
	mdw_flw_debug("appendix num(%u)\n", g_idx_count);
	return g_idx_count;
}

uint32_t mdw_cb_appendix_get_owner(uint32_t idx)
{
	struct cb_appendix_info *acb_info = NULL;
	uint32_t ret = 0;

	mutex_lock(&g_acb_mtx);
	hash_for_each_possible(g_acb_hash, acb_info, hash_node, idx) {
		if (acb_info->idx == idx) {
			ret = acb_info->owner;
			break;
		}
	}
	mutex_unlock(&g_acb_mtx);

	mdw_flw_debug("appendix-#%u owner(%u)\n", idx, ret);

	return ret;
}

uint32_t mdw_cb_appendix_size_by_idx(uint32_t idx, uint32_t num_subcmds)
{
	struct cb_appendix_info *acb_info = NULL;
	uint32_t ret = 0;

	mutex_lock(&g_acb_mtx);
	hash_for_each_possible(g_acb_hash, acb_info, hash_node, idx) {
		if (acb_info->idx == idx) {
			ret = acb_info->cb_size(num_subcmds);
			mdw_flw_debug("appendix-#%u owner(%u) num_subcmds(%u) size(%u)\n",
				idx, acb_info->owner, num_subcmds, ret);
			break;
		}
	}
	mutex_unlock(&g_acb_mtx);

	return ret;
}

int mdw_cb_appendix_process(int process_type, uint32_t idx, struct apusys_cmd_info *cmd_info,
	void *va, uint32_t size)
{
	struct cb_appendix_info *acb_info = NULL;
	int ret = -EINVAL;

	mdw_flw_debug("appendix(%u) process type(%d) cmdbuf(%pK/%u)\n", idx, process_type, va, size);

	mutex_lock(&g_acb_mtx);
	hash_for_each_possible(g_acb_hash, acb_info, hash_node, idx) {
		if (acb_info->idx == idx) {
			mdw_flw_debug("appendix-#%u owner(%u) id(0x%llx/0x%llx) process type(%d) cmdbuf(%pK/%u)\n",
				idx, acb_info->owner, cmd_info->session_id, cmd_info->cmd_uid, process_type, va, size);
			ret = acb_info->cb_process(process_type, cmd_info, va, size);
			break;
		}
	}
	mutex_unlock(&g_acb_mtx);

	return ret;
}

int mdw_cb_appendix_init(void)
{
	if (mdw_appendix_inited == true)
		return 0;

	INIT_HLIST_HEAD(g_acb_hash);
	mutex_init(&g_acb_mtx);
	g_idx_count = 0;
	mdw_appendix_inited = true;

	return 0;
}

void mdw_cb_appendix_deinit(void)
{
	struct cb_appendix_info *acb_info = NULL;
	struct hlist_node *tmp = NULL;
	int i = 0;

	mutex_lock(&g_acb_mtx);
	hash_for_each_safe(g_acb_hash, i, tmp, acb_info, hash_node) {
		mdw_flw_debug("delete acb_info(%u)\n", acb_info->idx);
		hlist_del(&acb_info->hash_node);
		kfree(acb_info);
	}
	mutex_unlock(&g_acb_mtx);

	mdw_appendix_inited = false;
}

int apusys_request_cmdbuf_appendix(enum apu_appendix_cb_owner owner, cb_apu_appendix_cmdbuf_size cb_size,
	cb_apu_appendix_cmdbuf_process cb_process)
{
	struct cb_appendix_info *acb_info = NULL;
	int ret = 0;

	mdw_cb_appendix_init();

	if (cb_size == NULL || cb_process == NULL) {
		mdw_drv_err("invalid param(/%pK/%pK)\n", cb_size, cb_process);
		return -EINVAL;
	}

	mdw_drv_debug("(%pK/%pK)\n", cb_size, cb_process);

	acb_info = kzalloc(sizeof(*acb_info), GFP_KERNEL);
	if (IS_ERR_OR_NULL(acb_info))
		return -ENOMEM;

	mutex_lock(&g_acb_mtx);
	acb_info->owner = owner;
	acb_info->cb_size = cb_size;
	acb_info->cb_process = cb_process;
	acb_info->idx = g_idx_count;
	mdw_flw_debug("add acb_info #-%u, owner(%u)\n", acb_info->idx, acb_info->owner);
	hash_add(g_acb_hash, &acb_info->hash_node, acb_info->idx);
	if (check_add_overflow(g_idx_count, 1, &g_idx_count)) {
		mdw_drv_err("request number overflow\n");
		ret = -EOVERFLOW;
	}
	mutex_unlock(&g_acb_mtx);

	return ret;
}
