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
}


int32_t cmdq_drv_imgsys_set_domain(void *data, bool isSet)
{
	struct TaskStruct *pTask   = NULL;

	pTask = (struct TaskStruct *)data;

	return 0;
}

int32_t cmdq_tz_isp_secure(void *data, bool isSet)
{
	struct TaskStruct *pTask   = NULL;

	pTask = (struct TaskStruct *)data;

	if (isSet) {
		/* imgsys dapc enable */
		pTask->enginesNeedDAPC = 1LL << CMDQ_SEC_ISP_IMGI;
		cmdq_tz_set_dapc_security_reg(pTask, true, true);

		/* DMA GSecure & Gdomain */
		cmdq_task_write_value_addr(pTask,  0x150210f0, 0x00002C2C, UINT_MAX);		// img2o
		cmdq_task_write_value_addr(pTask,  0x150210e8, 0x002C2C2C, UINT_MAX);		// img3o
		cmdq_task_write_value_addr(pTask,  0x150210c0, 0x0000002C, UINT_MAX);		// imgi
		cmdq_task_write_value_addr(pTask,  0x150210d4, 0x00002C2C, UINT_MAX);		// imgbi/imgci
		cmdq_task_write_value_addr(pTask,  0x150210cc, 0x002C2C2C, UINT_MAX);		// vipi

		cmdq_task_write_value_addr(pTask,  0x150210c8, 0x2C2C2C2C, UINT_MAX);		//smti_d1 ~ smti_d4
		cmdq_task_write_value_addr(pTask,  0x150210dc, 0x00002C2C, UINT_MAX);		//smti_d5 ~ smti_d6
		cmdq_task_write_value_addr(pTask,  0x150210e0, 0x2C2C2C2C, UINT_MAX);		//smto_d2 ~ smto_d5
		cmdq_task_write_value_addr(pTask,  0x150210ec, 0x00002C2C, UINT_MAX);		//smto_d1,smto_d6

		/* imgsys dapc disable */
		cmdq_tz_set_dapc_security_reg(pTask, false, true);
	}

	return 0;
}

int32_t cmdq_tz_isp_normal(void *data, bool isSet)
{
	struct TaskStruct *pTask   = NULL;

	pTask = (struct TaskStruct *)data;

	if (isSet) {
		/* imgsys dapc enable */
		pTask->enginesNeedDAPC = 1LL << CMDQ_SEC_ISP_IMGI;
		cmdq_tz_set_dapc_security_reg(pTask, true, true);

		cmdq_task_write_value_addr(pTask, 0x150210c0, 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, 0x150210c4, 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, 0x150210c8, 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, 0x150210cc, 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, 0x150210d0, 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, 0x150210d4, 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, 0x150210d8, 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, 0x150210dc, 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, 0x150210e0, 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, 0x150210e4, 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, 0x150210e8, 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, 0x150210ec, 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, 0x150210f0, 0x00000000, UINT_MAX);
		cmdq_task_write_value_addr(pTask, 0x150210f4, 0x00000000, UINT_MAX);

		/* imgsys dapc disable */
		cmdq_tz_set_dapc_security_reg(pTask, false, true);
	}

	return 0;
}

