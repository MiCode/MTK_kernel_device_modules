// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */
#include "pkvm_cmdq_platform.h"
#include "pkvm_cmdq_hyp.h"
#include "isp_sec_public.h"
#include <asm/kvm_pkvm_module.h>

const struct pkvm_module_ops *pkvm_cmdq_fdvt_ops;
#define CALL_FROM_CMDQ_FDVT_OPS(fn, ...) pkvm_cmdq_fdvt_ops->fn(__VA_ARGS__)

void cmdq_set_fdvt_ops(const struct pkvm_module_ops *ops)
{
	pkvm_cmdq_fdvt_ops = ops;
	CALL_FROM_CMDQ_FDVT_OPS(puts, __func__);
	CALL_FROM_CMDQ_FDVT_OPS(puts, PFX_CMDQ_MSG "enter");
}

int32_t cmdq_drv_isp_setup_task_fd(void *data, uint32_t size,
	struct isp_exec_metadata *isp_execmeta,
	struct iwcCmdqSecStatus_t *secStatus)
{
	return 0;
}

int32_t cmdq_drv_isp_setup_iova(void *data, uint32_t size,
	struct isp_exec_metadata *isp_execmeta,
	struct iwcCmdqSecStatus_t *secStatus)
{
	return 0;
}

int32_t cmdq_drv_isp_setup_task_cq(
	struct iwc_cq_meta *msgex,
	struct iwc_cq_meta2 *msgex2,
	struct isp_meta_cq *isp_execmeta,
	struct iwcCmdqSecStatus_t *secStatus)
{
	return 0;
}
