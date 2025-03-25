// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include "pkvm_cmdq_platform.h"
#include "pkvm_cmdq_hyp.h"
#include <asm/kvm_pkvm_module.h>

const struct pkvm_module_ops *pkvm_cmdq_isp_ops;
#define CALL_FROM_CMDQ_ISP_OPS(fn, ...) pkvm_cmdq_isp_ops->fn(__VA_ARGS__)

void cmdq_set_isp_ops(const struct pkvm_module_ops *ops)
{
	pkvm_cmdq_isp_ops = ops;
	CALL_FROM_CMDQ_ISP_OPS(puts, __func__);
	CALL_FROM_CMDQ_ISP_OPS(puts, PFX_CMDQ_MSG "enter");
}

#define SLC_SEC_THD (8)
void cmdq_drv_imgsys_slc_cb(void)
{
	CALL_FROM_CMDQ_ISP_OPS(puts, __func__);
	CALL_FROM_CMDQ_ISP_OPS(puts, PFX_CMDQ_MSG "spr1:");
	CALL_FROM_CMDQ_ISP_OPS(putx64, (u64)CMDQ_REG_GET32(CMDQ_THR_SPR1(SLC_SEC_THD)));
	CALL_FROM_CMDQ_ISP_OPS(puts, PFX_CMDQ_MSG "spr2:");
	CALL_FROM_CMDQ_ISP_OPS(putx64, (u64)CMDQ_REG_GET32(CMDQ_THR_SPR2(SLC_SEC_THD)));
	CALL_FROM_CMDQ_ISP_OPS(puts, PFX_CMDQ_MSG "spr3:");
	CALL_FROM_CMDQ_ISP_OPS(putx64, (u64)CMDQ_REG_GET32(CMDQ_THR_SPR3(SLC_SEC_THD)));
}

int32_t cmdq_drv_imgsys_set_domain(void *data, bool isSet)
{
	struct TaskStruct *pTask   = NULL;

	pTask = (struct TaskStruct *)data;

	if (isSet) {
		/* ME */
		cmdq_task_write_value_addr(pTask, (0x34070000 + 0x148), 0x10000A8, UINT_MAX);

		/* WPE-TNR */
		cmdq_task_write_value_addr(pTask, (0x34200000 + 0xE60), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34200000 + 0xE64), 0xA8A8A800, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34200000 + 0xE68), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34200000 + 0xE6C), 0xA8A8A800, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34200000 + 0xE70), 0xA8A8A800, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34200000 + 0xE74), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34200000 + 0xE78), 0xA8A80000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34200000 + 0xE7C), 0x1C3FFC00, UINT_MAX);
		/* OMC-TNR */
		cmdq_task_write_value_addr(pTask, (0x34540000 + 0xE60), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34540000 + 0xE64), 0xA8A8A800, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34540000 + 0xE68), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34540000 + 0xE6C), 0xA8A8A800, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34540000 + 0xE70), 0xA8A8A800, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34540000 + 0xE74), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34540000 + 0xE78), 0xA8A80000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34540000 + 0xE7C), 0x1C3FFC00, UINT_MAX);
		/* WPE_LITE */
		cmdq_task_write_value_addr(pTask, (0x34600000 + 0xE60), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34600000 + 0xE64), 0xA8A8A800, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34600000 + 0xE68), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34600000 + 0xE6C), 0xA8A8A800, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34600000 + 0xE70), 0xA8A8A800, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34600000 + 0xE74), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34600000 + 0xE78), 0xA8A80000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34600000 + 0xE7C), 0x1C3FFC00, UINT_MAX);
		/* OMC-LITE */
		cmdq_task_write_value_addr(pTask, (0x34640000 + 0xE60), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34640000 + 0xE64), 0xA8A8A800, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34640000 + 0xE68), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34640000 + 0xE6C), 0xA8A8A800, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34640000 + 0xE70), 0xA8A8A800, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34640000 + 0xE74), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34640000 + 0xE78), 0xA8A80000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34640000 + 0xE7C), 0x1C3FFC00, UINT_MAX);

		/* TRAW */
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x20), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x24), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x28), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x2C), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x30), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x34), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x38), 0x0000A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x3C), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x40), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x44), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x48), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x4C), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x5C), 0x00A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x60), 0x000000A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x64), 0xF300FF0F, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x68), 0x7000FFFF, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x6C), 0x00000001, UINT_MAX);

		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x20), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x24), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x28), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x2C), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x30), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x34), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x38), 0x0000A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x3C), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x40), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x44), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x48), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x4C), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x5C), 0x00A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x60), 0x000000A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x64), 0xF300FF0F, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x68), 0x7000FFFF, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x6C), 0x00000001, UINT_MAX);

		/* DIP */
		cmdq_task_write_value_addr(pTask, (0x34101000 + 0x20), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34101000 + 0x24), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34101000 + 0x28), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34101000 + 0x2C), 0x00A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34101000 + 0x30), 0xA8A80000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34101000 + 0x34), 0xA800A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34101000 + 0x38), 0xA8A8A800, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34101000 + 0x3C), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34101000 + 0x40), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34101000 + 0x44), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34101000 + 0x48), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34101000 + 0x4C), 0xA8000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34101000 + 0x50), 0x0000A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34101000 + 0x64), 0x7EBC7F0F, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34101000 + 0x68), 0x00020FFE, UINT_MAX);
		/* DIP_NR */
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x20), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x24), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x28), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x2C), 0xA80000A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x38), 0x0000A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x3C), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x40), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x44), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x48), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x4C), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x50), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x54), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x58), 0xA80000A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x5C), 0x000000A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x60), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x64), 0x0000A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x6C), 0xF3009FFF, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x70), 0x19FFFFFF, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x74), 0x0000003F, UINT_MAX);

		/* PQDIP */
		cmdq_task_write_value_addr(pTask, (0x34211000 + 0x20), 0xA8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34211000 + 0x64), 0x7, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34212000 + 0xF30), 0xA8800000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34511000 + 0x20), 0xA8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34511000 + 0x64), 0x7, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34512000 + 0xF30), 0xA8800000, UINT_MAX);
	} else {
		/* ME */
		cmdq_task_write_value_addr(pTask, (0x34070000 + 0x148), 0x0, UINT_MAX);

		/* WPE-TNR */
		cmdq_task_write_value_addr(pTask, (0x34200000 + 0xE60), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34200000 + 0xE64), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34200000 + 0xE68), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34200000 + 0xE6C), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34200000 + 0xE70), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34200000 + 0xE74), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34200000 + 0xE78), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34200000 + 0xE7C), 0x0, UINT_MAX);
		/* OMC-TNR */
		cmdq_task_write_value_addr(pTask, (0x34540000 + 0xE60), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34540000 + 0xE64), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34540000 + 0xE68), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34540000 + 0xE6C), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34540000 + 0xE70), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34540000 + 0xE74), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34540000 + 0xE78), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34540000 + 0xE7C), 0x0, UINT_MAX);
		/* WPE_LITE */
		cmdq_task_write_value_addr(pTask, (0x34600000 + 0xE60), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34600000 + 0xE64), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34600000 + 0xE68), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34600000 + 0xE6C), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34600000 + 0xE70), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34600000 + 0xE74), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34600000 + 0xE78), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34600000 + 0xE7C), 0x0, UINT_MAX);
		/* OMC-LITE */
		cmdq_task_write_value_addr(pTask, (0x34640000 + 0xE60), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34640000 + 0xE64), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34640000 + 0xE68), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34640000 + 0xE6C), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34640000 + 0xE70), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34640000 + 0xE74), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34640000 + 0xE78), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34640000 + 0xE7C), 0x0, UINT_MAX);

		/* TRAW */
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x20), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x24), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x28), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x2C), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x30), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x34), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x38), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x3C), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x40), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x44), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x48), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x4C), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x5C), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x60), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x64), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x68), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34704000 + 0x6C), 0x0, UINT_MAX);

		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x20), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x24), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x28), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x2C), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x30), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x34), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x38), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x3C), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x40), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x44), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x48), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x4C), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x5C), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x60), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x64), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x68), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34044000 + 0x6C), 0x0, UINT_MAX);

		/* DIP */
		cmdq_task_write_value_addr(pTask, (0x34101000 + 0x20), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34101000 + 0x24), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34101000 + 0x28), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34101000 + 0x2C), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34101000 + 0x30), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34101000 + 0x34), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34101000 + 0x38), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34101000 + 0x3C), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34101000 + 0x40), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34101000 + 0x44), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34101000 + 0x48), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34101000 + 0x4C), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34101000 + 0x50), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34101000 + 0x64), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34101000 + 0x68), 0x0, UINT_MAX);
		/* DIP_NR */
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x20), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x24), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x28), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x2C), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x38), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x3C), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x40), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x44), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x48), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x4C), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x50), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x54), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x58), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x5C), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x60), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x64), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x6C), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x70), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34161000 + 0x74), 0x0, UINT_MAX);
		/* PQDIP */
		cmdq_task_write_value_addr(pTask, (0x34211000 + 0x20), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34211000 + 0x64), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34212000 + 0xF30), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34511000 + 0x20), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34511000 + 0x64), 0x0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34512000 + 0xF30), 0x0, UINT_MAX);
	}

	return 0;
}
