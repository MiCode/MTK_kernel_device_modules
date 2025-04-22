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
		cmdq_task_write_value_addr(pTask, (0x3451A000 + 0x000), 0x00000A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3451A000 + 0x004), 0x0000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3451A000 + 0x008), 0x0000001, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3451A000 + 0x00C), 0x0000000, UINT_MAX);

		/* WPE-TNR */
		cmdq_task_write_value_addr(pTask, (0x34218000 + 0x000), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34218100 + 0x000), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34218100 + 0x004), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34218100 + 0x008), 0x00A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34218100 + 0x00C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34218100 + 0x010), 0x00A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34218100 + 0x014), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34218100 + 0x018), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34218100 + 0x01C), 0x0000E1FC, UINT_MAX);
		/* WPE_LITE */
		cmdq_task_write_value_addr(pTask, (0x34618000 + 0x000), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34618100 + 0x000), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34618100 + 0x004), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34618100 + 0x008), 0x00A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34618100 + 0x00C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34618100 + 0x010), 0x00A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34618100 + 0x014), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34618100 + 0x018), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34618100 + 0x01C), 0x0000E1FC, UINT_MAX);
		/* OMC-TNR */
		cmdq_task_write_value_addr(pTask, (0x34518000 + 0x000), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34518100 + 0x000), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34518100 + 0x004), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34518100 + 0x008), 0x00A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34518100 + 0x00C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34518100 + 0x010), 0x00A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34518100 + 0x014), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34518100 + 0x01C), 0x0000E1FC, UINT_MAX);
		/* OMC-LITE */
		cmdq_task_write_value_addr(pTask, (0x34619000 + 0x000), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34619100 + 0x000), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34619100 + 0x004), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34619100 + 0x008), 0x00A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34619100 + 0x00C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34619100 + 0x010), 0x00A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34619100 + 0x014), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34619100 + 0x01C), 0x0000E1FC, UINT_MAX);

		/* TRAW */
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x00), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x04), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x08), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x0C), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x10), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x14), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x18), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x1C), 0x0000A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x20), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x24), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x28), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x2C), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x30), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x34), 0x0000A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x38), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x3C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x40), 0xA800A800, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x44), 0x00A800A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x48), 0xA8A8A800, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x4C), 0xF300FFF0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x50), 0xA003FFFF, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x54), 0x000000E5, UINT_MAX);

		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x00), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x04), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x08), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x0C), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x10), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x14), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x18), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x1C), 0x0000A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x20), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x24), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x28), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x2C), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x30), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x34), 0x0000A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x38), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x3C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x40), 0xA800A800, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x44), 0x00A800A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x48), 0xA8A8A800, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x4C), 0xF300FFF0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x50), 0xA003FFFF, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x54), 0x000000E5, UINT_MAX);

		/* DIP: CID*/
		cmdq_task_write_value_addr(pTask, (0x34118000 + 0x00), 0x00000000, UINT_MAX);
		/* DIPTOP */
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x00), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x04), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x08), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x0C), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x10), 0x00A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x14), 0xA8A80000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x18), 0x000000A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x1C), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x20), 0x0000A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x24), 0xA8A80000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x28), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x2C), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x30), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x34), 0x000000A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x38), 0x0000A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x3C), 0xA8A80000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x40), 0x000000A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x44), 0x3F1C7FF0, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x4C), 0x1C31FFFC, UINT_MAX);

		/* DIP_NR */
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x00), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x04), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x08), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x0C), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x10), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x14), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x18), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x1C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x20), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x24), 0x00A8A800, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x28), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x2C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x30), 0xA8000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x34), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x38), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x3C), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x40), 0xA8A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x44), 0x00A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x48), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x4C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x50), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x54), 0x00007FFF, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x58), 0xFFFF8006, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x5C), 0x00000007, UINT_MAX);

		/* PQDIP-A */
		cmdq_task_write_value_addr(pTask, (0x34219000 + 0x00), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34219200 + 0x00), 0x00000000, UINT_MAX);
		if (mtkcam_security_cam_normal_preview_support)
			cmdq_task_write_value_addr(pTask, (0x34252000 + 0xF30), 0x00000000, UINT_MAX);
		else
			cmdq_task_write_value_addr(pTask, (0x34252000 + 0xF30), 0xA8800000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34219200 + 0x04), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34219200 + 0x08), 0x00A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34219200 + 0x0C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34219200 + 0x10), 0x00000070, UINT_MAX);

		/* PQDIP-B*/
		cmdq_task_write_value_addr(pTask, (0x34519000 + 0x00), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34519200 + 0x00), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34542000 + 0xF30), 0xA8800000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34519200 + 0x04), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34519200 + 0x08), 0x00A8A8A8, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34519200 + 0x0C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34519200 + 0x10), 0x00000070, UINT_MAX);

	} else {
		/* ME */
		cmdq_task_write_value_addr(pTask, (0x3451A000 + 0x000), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3451A000 + 0x004), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3451A000 + 0x008), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3451A000 + 0x00C), 0x00000000, UINT_MAX);

		/* WPE-TNR */
		cmdq_task_write_value_addr(pTask, (0x34218000 + 0x000), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34218100 + 0x000), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34218100 + 0x004), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34218100 + 0x008), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34218100 + 0x00C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34218100 + 0x010), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34218100 + 0x014), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34218100 + 0x018), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34218100 + 0x01C), 0x00000000, UINT_MAX);
		/* WPE_LITE */
		cmdq_task_write_value_addr(pTask, (0x34618000 + 0x000), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34618100 + 0x000), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34618100 + 0x004), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34618100 + 0x008), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34618100 + 0x00C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34618100 + 0x010), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34618100 + 0x014), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34618100 + 0x018), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34618100 + 0x01C), 0x00000000, UINT_MAX);
		/* OMC-TNR */
		cmdq_task_write_value_addr(pTask, (0x34518000 + 0x000), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34518100 + 0x000), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34518100 + 0x004), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34518100 + 0x008), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34518100 + 0x00C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34518100 + 0x010), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34518100 + 0x014), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34518100 + 0x01C), 0x00000000, UINT_MAX);
		/* OMC-LITE */
		cmdq_task_write_value_addr(pTask, (0x34619000 + 0x000), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34619100 + 0x000), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34619100 + 0x004), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34619100 + 0x008), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34619100 + 0x00C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34619100 + 0x010), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34619100 + 0x014), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34619100 + 0x01C), 0x00000000, UINT_MAX);

		/* TRAW */
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x00), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x04), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x08), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x0C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x10), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x14), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x18), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x1C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x20), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x24), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x28), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x2C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x30), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x34), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x38), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x3C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x40), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x44), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x48), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x4C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x50), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34718500 + 0x54), 0x00000000, UINT_MAX);

		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x00), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x04), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x08), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x0C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x10), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x14), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x18), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x1C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x20), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x24), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x28), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x2C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x30), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x34), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x38), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x3C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x40), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x44), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x48), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x4C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x50), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3400B500 + 0x54), 0x00000000, UINT_MAX);

		/* DIP: CID*/
		cmdq_task_write_value_addr(pTask, (0x34118000 + 0x00), 0x00000000, UINT_MAX);
		/* DIPTOP */
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x00), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x04), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x08), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x0C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x10), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x14), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x18), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x1C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x20), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x24), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x28), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x2C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x30), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x34), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x38), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x3C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x40), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x44), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34119000 + 0x4C), 0x00000000, UINT_MAX);

		/* DIP_NR */
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x00), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x04), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x08), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x0C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x10), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x14), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x18), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x1C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x20), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x24), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x28), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x2C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x30), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x34), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x38), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x3C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x40), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x44), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x48), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x4C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x50), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x54), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x58), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x3411A000 + 0x5C), 0x00000000, UINT_MAX);

		/* PQDIP-A */
		cmdq_task_write_value_addr(pTask, (0x34219000 + 0x00), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34219200 + 0x00), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34252000 + 0xF30), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34219200 + 0x04), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34219200 + 0x08), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34219200 + 0x0C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34219200 + 0x10), 0x00000000, UINT_MAX);

		/* PQDIP-B*/
		cmdq_task_write_value_addr(pTask, (0x34519000 + 0x00), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34519200 + 0x00), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34542000 + 0xF30), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34519200 + 0x04), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34519200 + 0x08), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34519200 + 0x0C), 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, (0x34519200 + 0x10), 0x00000000, UINT_MAX);
	}

	return 0;
}
