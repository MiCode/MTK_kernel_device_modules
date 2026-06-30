// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/rpmsg.h>
#include "hds.h"
#include "apusys_device.h"

static uint32_t hds_cmd_appendix_cb_size(uint32_t num_subcmds)
{
	uint32_t appendix_size = 0;

	if (!g_hdev->pmu_lv)
		return 0;

	if (check_mul_overflow(num_subcmds, g_hdev->per_subcmd_appendix_size, &appendix_size)) {
		apu_hds_warn("check appendix subcmd overflow(%u/%u/%u)\n",
			g_hdev->per_cmd_appendix_size, g_hdev->per_subcmd_appendix_size, num_subcmds);
		return 0;
	}

	if (check_add_overflow(appendix_size, g_hdev->per_cmd_appendix_size, &appendix_size)) {
		apu_hds_warn("check appendix cmd overflow(%u/%u/%u)\n",
			g_hdev->per_cmd_appendix_size, g_hdev->per_subcmd_appendix_size, num_subcmds);
		return 0;
	}

	return appendix_size;
}

static int hds_cmd_appendix_cb_process(enum apu_appendix_cb_type type,
	struct apusys_cmd_info *cmd_info, void *va, uint32_t size)
{
	int ret = 0;

	if (cmd_info == NULL)
		return -EINVAL;

	/* check argument */
	if (!size || va == NULL || !cmd_info->num_subcmds)
		return 0;

	/* check size */
	if (size != g_hdev->per_cmd_appendix_size + cmd_info->num_subcmds * g_hdev->per_subcmd_appendix_size) {
		apu_hds_err("size not matched(%u/%u)\n",
			size, g_hdev->per_cmd_appendix_size + cmd_info->num_subcmds * g_hdev->per_subcmd_appendix_size);
		return -EINVAL;
	}

	apu_hds_debug("type(%u) id(0x%llx/0x%llx) appendix(%pK/%u)\n",
		type, cmd_info->session_id, cmd_info->cmd_uid, va, size);

	switch (type) {
	case APU_APPENDIX_CB_POSTPROCESS_LATE:
		ret = g_hdev->plat_func->cmd_postprocess_late(g_hdev, va, size, cmd_info->power_plcy);
		if (ret)
			apu_hds_err("cmd postprocess late failed(%d)\n", ret);
		break;
	default:
		break;
	};

	return ret;
}

int hds_cmd_init(void)
{
	int ret = 0;

	ret = apusys_request_cmdbuf_appendix(APU_APPENDIX_CB_OWNER_HDS, hds_cmd_appendix_cb_size, hds_cmd_appendix_cb_process);
	if (ret)
		apu_hds_err("request appendix cmdbuf failed(%d)\n", ret);

	return ret;
}
