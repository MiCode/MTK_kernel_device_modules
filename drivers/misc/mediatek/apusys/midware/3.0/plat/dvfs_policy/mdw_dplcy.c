// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/min_heap.h>
#include <linux/slab.h>

#include "mdw.h"
#include "mdw_cmn.h"
#include "mdw_dplcy.h"
#include "mdw_rv.h"

#define MDW_DPLCY_VERSION (1)
#define MDW_DPLCY_BW_HIS_LENGTH (16)

struct mdw_dplcy_subcmd {
	uint32_t cmd_bw_his[MDW_DPLCY_BW_HIS_LENGTH];
} __packed;

struct mdw_dplcy_cmd {
	uint32_t version;
	uint32_t cmd_id;
} __packed;

static uint32_t mdw_dplcy_appendix_cb_size(uint32_t num_subcmds)
{
	uint32_t appendix_size = 0;

	if (check_mul_overflow(num_subcmds, sizeof(struct mdw_dplcy_subcmd), &appendix_size)) {
		mdw_drv_warn("check appendix subcmd overflow(%lu/%lu/%u)\n",
			sizeof(struct mdw_dplcy_cmd), sizeof(struct mdw_dplcy_subcmd), num_subcmds);
		return 0;
	}

	if (check_add_overflow(appendix_size, sizeof(struct mdw_dplcy_cmd), &appendix_size)) {
		mdw_drv_warn("check appendix cmd overflow(%lu/%lu/%u)\n",
			sizeof(struct mdw_dplcy_cmd), sizeof(struct mdw_dplcy_subcmd), num_subcmds);
		return 0;
	}

	return appendix_size;
}

static int mdw_dplcy_appendix_cb_process(enum apu_appendix_cb_type type, uint64_t session_id,
	uint64_t cmd_uid, uint32_t num_subcmds, void *va, uint32_t size)
{
	int ret = 0;
	struct mdw_dplcy_cmd *dcmd = (struct mdw_dplcy_cmd *)va;

	/* check argument */
	if (!size || va == NULL || !num_subcmds)
		return -EINVAL;

	/* check size */
	if (size != sizeof(struct mdw_dplcy_cmd) + num_subcmds * sizeof(struct mdw_dplcy_subcmd)) {
		mdw_drv_err("size not matched(%u/%lu)\n",
			size, sizeof(struct mdw_dplcy_cmd) + num_subcmds * sizeof(struct mdw_dplcy_subcmd));
		return -EINVAL;
	}

	mdw_flw_debug("type(%u) id(0x%llx/0x%llx) appendix(%pK/%u)\n", type, session_id, cmd_uid, va, size);

	switch (type) {
	case APU_APPENDIX_CB_CREATE:
		break;
	case APU_APPENDIX_CB_PREPROCESS:
		dcmd->version = 123; //todo, test
		break;
	case APU_APPENDIX_CB_POSTPROCESS:
		break;
	case APU_APPENDIX_CB_POSTPROCESS_LATE:
		break;
	case APU_APPENDIX_CB_DELETE:
		break;
	default:
		break;
	};

	return ret;
}

int mdw_dplcy_init(void)
{
	int ret = 0;

	ret = apusys_request_cmdbuf_appendix(APU_APPENDIX_CB_OWNER_DVFS_POLICY,
		mdw_dplcy_appendix_cb_size, mdw_dplcy_appendix_cb_process);
	if (ret)
		mdw_drv_err("request appendix cmdbuf failed(%d)\n", ret);

	return ret;
}

void mdw_dplcy_deinit(void)
{

}