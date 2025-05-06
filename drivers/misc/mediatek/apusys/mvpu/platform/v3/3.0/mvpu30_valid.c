// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>

#include "apusys_device.h"
#include "mdw_trace.h"

#include "mvpu_plat.h"
#include "mvpu_sysfs.h"
#include "mvpu30_request.h"
#include "mvpu30_sec.h"
#include "mvpu30_handler.h"
#include "mvpu30_ipi.h"

int mvpu30_validation(void *hnd)
{
	int ret = 0;
	struct apusys_cmd_valid_handle *cmd_hnd;
	struct apusys_cmdbuf *cmdbuf;

	mdw_trace_begin("[MVPU] %s", __func__);

	cmd_hnd = hnd;
	if (cmd_hnd->session == NULL) {
		pr_info("[MVPU][Sec] [ERROR] APUSYS_CMD_VALIDATE: session is NULL\n");
		ret = -1;
		goto END;
	}
	if (cmd_hnd->num_cmdbufs < MVPU_MIN_CMDBUF_NUM) {
		pr_info("[MVPU][Sec] [ERROR] %s get wrong num_cmdbufs: %d\n",
				__func__, cmd_hnd->num_cmdbufs);
		ret = -1;
		goto END;
	}

	cmdbuf = cmd_hnd->cmdbufs;
	if (cmdbuf[MVPU_CMD_INFO_IDX].size != sizeof(struct mvpu_request_v30)) {
		pr_info("[MVPU][Sec] [ERROR] get wrong cmdbuf size: 0x%x, should be 0x%lx\n",
				cmdbuf[MVPU_CMD_INFO_IDX].size,
				sizeof(struct mvpu_request_v30));
		ret = -1;
		goto END;
	}

END:
	mdw_trace_end();

	return ret;
}

