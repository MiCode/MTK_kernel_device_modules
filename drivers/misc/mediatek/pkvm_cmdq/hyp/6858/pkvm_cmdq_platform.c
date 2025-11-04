// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include "pkvm_cmdq_hyp.h"
#include "pkvm_cmdq_platform.h"
#include <asm/kvm_pkvm_module.h>
#include <pkvm_sys.h>
#include "mtk-larb-port.h"
#include "cmdq_sec_iwc_common.h"

#include "mdp_sec_platform.h"
#include "haM4uApi.h"
#include "gce.h"

#define GCED_BASE_PA 0x1e980000
#define GCEM_BASE_PA 0x1e990000

static uint32_t gce_base_va;
static uint8_t cmdq_id;

const struct pkvm_module_ops *pkvm_cmdq_plat_ops;
#define CALL_FROM_PLAT_OPS(fn, ...) pkvm_cmdq_plat_ops->fn(__VA_ARGS__)

void cmdq_set_plat_ops(const struct pkvm_module_ops *ops)
{
	pkvm_cmdq_plat_ops = ops;
	CALL_FROM_PLAT_OPS(puts, __func__);
	CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_MSG "enter");
}

uint32_t cmdq_tz_get_gce_base_va(void)
{
	return gce_base_va;
}

void cmdq_tz_setup(uint8_t hwid)
{
	CALL_FROM_PLAT_OPS(puts, __func__);

	if (hwid == 0) {
		gce_base_va = GCED_BASE_PA;
		CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_MSG "setup gce-d");
	} else if (hwid == 1) {
		gce_base_va = GCEM_BASE_PA;
		CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_MSG "setup gce-m");
	}
	cmdq_id = hwid;
}

uint32_t cmdq_secio_type(const uint32_t addr, const uint32_t cmdq_id)
{
	// CALL_FROM_PLAT_OPS(puts, __func__);
	// CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_MSG "addr:");
	// CALL_FROM_PLAT_OPS(putx64, (u64)addr);
	switch (addr) {
	case CMDQ_SECIO_TYPE_RANGE_0
		... CMDQ_SECIO_TYPE_RANGE_1:
		if (cmdq_id == 0)
			return SECIO_GCE;
		else
			return SECIO_GCE_M;
	case CMDQ_SECIO_TYPE_RANGE_7
		... CMDQ_SECIO_TYPE_RANGE_8:
		if (cmdq_id == 0)
			return SECIO_GCE_SECURITY;
		else
			return SECIO_GCE_M_SECURITY;
	default:
		return SECIO_MAX;
	}
	return SECIO_MAX;
}

uint32_t cmdq_secio_read(const uint32_t addr)
{
	uint32_t secio_type = 0;
	uint32_t val;

	secio_type = cmdq_secio_type(CMDQ_SECIO_TYPE_GET_OFFSET(addr), cmdq_id);

	SECIO_READ(secio_type,
		CMDQ_SECIO_GET_OFFSET(addr), &val);

	return val;
}

void cmdq_secio_write(const uint32_t addr, const uint32_t val)
{
	uint32_t secio_type = 0;

	secio_type = cmdq_secio_type(CMDQ_SECIO_TYPE_GET_OFFSET(addr), cmdq_id);

	SECIO_WRITE(secio_type,
		CMDQ_SECIO_GET_OFFSET(addr), val);

	// CALL_FROM_PLAT_OPS(puts, __func__);
	// CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_MSG "ret:");
	// CALL_FROM_PLAT_OPS(putx64, (u64)ret);
	// CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_MSG "secio_type:");
	// CALL_FROM_PLAT_OPS(putx64, (u64)secio_type);
	// CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_MSG "addr:");
	// CALL_FROM_PLAT_OPS(putx64, (u64)CMDQ_SECIO_GET_OFFSET(addr));
	// CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_MSG "val:");
	// CALL_FROM_PLAT_OPS(putx64, (u64)val);
	// CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_MSG "read:");
	// CALL_FROM_PLAT_OPS(putx64, (u64)CMDQ_REG_GET32(addr));
}

int32_t cmdq_drv_imgsys_set_slc(void *data)
{
	return 0;
}

void cmdq_tz_assign_tzmp_command(struct TaskStruct *pTask)
{
#define MAE_SECURE_SUPPORT (1)
#define MAE_BASE	0x34310000

	if (pTask->hwid == 1 && pTask->thread == 9) { /* cmdq task */
		cmdq_task_wfe(pTask, CMDQ_SYNC_TOKEN_TZMP_AIE_WAIT);
		cmdq_task_write_value_addr(pTask, GCEM_BASE_PA + 0xbc, 0xfeedbeaf, UINT_MAX);
		cmdq_task_set_event(pTask, CMDQ_SYNC_TOKEN_TZMP_AIE_SET);
	} else if (pTask->hwid == 0 && pTask->thread == 10) { /* MDP task */
		cmdq_tz_mdp_handle(pTask);
	} else if (pTask->hwid == 1 && pTask->thread == 10) { /* img task */
		cmdq_task_wfe(pTask, CMDQ_SYNC_TOKEN_TZMP_ISP_WAIT);

		/* imgsys dapc enable */
		pTask->enginesNeedDAPC = 1LL << CMDQ_SEC_ISP_IMGI;
		cmdq_tz_set_dapc_security_reg(pTask, true, true);

		/* Set imgsys GDomain and NSbit */
		cmdq_drv_imgsys_set_domain((void *)pTask, true);

		/* imgsys dapc disable */
		cmdq_tz_set_dapc_security_reg(pTask, false, true);

		cmdq_task_set_event(pTask, CMDQ_SYNC_TOKEN_TZMP_ISP_SET);

		cmdq_task_wfe(pTask, CMDQ_SYNC_TOKEN_TZMP_ISP_WAIT);

		/* imgsys dapc enable */
		cmdq_tz_set_dapc_security_reg(pTask, true, true);

		/* Clear imgsys GDomain and NSbit */
		cmdq_drv_imgsys_set_domain((void *)pTask, false);

		/* imgsys dapc disable */
		cmdq_tz_set_dapc_security_reg(pTask, false, true);

		cmdq_task_set_event(pTask, CMDQ_SYNC_TOKEN_TZMP_ISP_SET);
	} else if (pTask->hwid == 1 && pTask->thread == 11) { /* aie task */
		cmdq_task_wfe(pTask, CMDQ_SYNC_TOKEN_TZMP_AIE_WAIT);
		//dapc
		cmdq_task_write_value_addr(pTask, FDVT_ENABLE_HW, 0x00000111, UINT_MAX);
		cmdq_task_write_value_addr(pTask, FDVT_LOOP_HW, 0x00006002, UINT_MAX);
		cmdq_task_write_value_addr(pTask, FDVT_INT_EN_HW, 0x0, UINT_MAX);

		uint32_t RS_IOVA = 0, FD_IOVA = 0, YUV_IOVA = 0, POSE_IOVA = 0;

		Get_RSConfig_IOVA(&RS_IOVA);
		Get_FDConfig_IOVA(&FD_IOVA);
		Get_YUVConfig_IOVA(&YUV_IOVA);
		Get_FDPOSE_IOVA(&POSE_IOVA);

		cmdq_task_write_value_addr(pTask, FDVT_RS_CON_BASE_ADR_HW, RS_IOVA, UINT_MAX);
		cmdq_task_write_value_addr(pTask, FDVT_FD_CON_BASE_ADR_HW, FD_IOVA, UINT_MAX);
		cmdq_task_write_value_addr(pTask, FDVT_YUV2RGB_CON_BASE_ADR_HW, YUV_IOVA, UINT_MAX);

		cmdq_task_write_value_addr(pTask, FDVT_P0_GDOMIAN, 0x2, 0x2);
		cmdq_task_write_value_addr(pTask, FDVT_P1_GDOMIAN, 0x2, 0x2);
		cmdq_task_write_value_addr(pTask, FDVT_P2_GDOMIAN, 0x2, 0x2);
		cmdq_task_write_value_addr(pTask, FDVT_P3_GDOMIAN, 0x2, 0x2);
		cmdq_task_write_value_addr(pTask, FDVT_P0_GDOMIAN, 0xc0, 0x1f0);
		cmdq_task_write_value_addr(pTask, FDVT_P1_GDOMIAN, 0xc0, 0x1f0);
		cmdq_task_write_value_addr(pTask, FDVT_P2_GDOMIAN, 0xc0, 0x1f0);
		cmdq_task_write_value_addr(pTask, FDVT_P3_GDOMIAN, 0xc0, 0x1f0);


		cmdq_task_write_value_addr(pTask, FDVT_START_HW, 0x1, UINT_MAX);

		cmdq_task_wfe(pTask, CMDQ_EVENT_IPE_FDVT_DONE);

		cmdq_task_write_value_addr(pTask, FDVT_START_HW, 0x0, UINT_MAX);

		cmdq_task_write_value_addr(pTask, FDVT_ENABLE_HW, 0x00000100, UINT_MAX);
		cmdq_task_write_value_addr(pTask, FDVT_LOOP_HW, 0x00000300, UINT_MAX);
		cmdq_task_write_value_addr(pTask, FDVT_INT_EN_HW, 0x1, UINT_MAX);


		cmdq_task_write_value_addr(pTask, FDVT_FD_CON_BASE_ADR_HW, POSE_IOVA, UINT_MAX);


		cmdq_task_write_value_addr(pTask, FDVT_START_HW, 0x1, UINT_MAX);

		cmdq_task_wfe(pTask, CMDQ_EVENT_IPE_FDVT_DONE);

		cmdq_task_write_value_addr(pTask, FDVT_START_HW, 0x0, UINT_MAX);

		cmdq_task_write_value_addr(pTask, FDVT_P0_GDOMIAN, 0x0, 0x2);
		cmdq_task_write_value_addr(pTask, FDVT_P1_GDOMIAN, 0x0, 0x2);
		cmdq_task_write_value_addr(pTask, FDVT_P2_GDOMIAN, 0x0, 0x2);
		cmdq_task_write_value_addr(pTask, FDVT_P3_GDOMIAN, 0x0, 0x2);
		cmdq_task_write_value_addr(pTask, FDVT_P0_GDOMIAN, 0x0, 0x1f0);
		cmdq_task_write_value_addr(pTask, FDVT_P1_GDOMIAN, 0x0, 0x1f0);
		cmdq_task_write_value_addr(pTask, FDVT_P2_GDOMIAN, 0x0, 0x1f0);
		cmdq_task_write_value_addr(pTask, FDVT_P3_GDOMIAN, 0x0, 0x1f0);

		cmdq_task_set_event(pTask, CMDQ_SYNC_TOKEN_TZMP_AIE_SET);
	} else if(pTask->hwid == 1 && pTask->thread == 8) {
		cmdq_task_wfe(pTask, CMDQ_SYNC_TOKEN_TZMP_ISC_WAIT);
		// devapc enable
		cmdq_drv_imgsys_set_slc((void *)pTask);
		// devapc disable
		cmdq_task_set_event(pTask, CMDQ_SYNC_TOKEN_TZMP_ISC_SET);
	}
}

void cmdq_task_cb(struct TaskStruct *pTask)
{
	if (pTask->throwAEE) {
		CALL_FROM_PLAT_OPS(puts, __func__);
		CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_MSG "hwid:");
		CALL_FROM_PLAT_OPS(putx64, (u64)pTask->hwid);
		CALL_FROM_PLAT_OPS(puts, PFX_CMDQ_MSG "thread:");
		CALL_FROM_PLAT_OPS(putx64, (u64)pTask->thread);
	}

	if (pTask->hwid == 2 && pTask->thread == 8)
		cmdq_drv_imgsys_slc_cb();
}

bool m4u_larb_port_without_aid(const uint32_t port)
{
	switch (port) {
	case M4U_LARB20_PORT0:
	case M4U_LARB20_PORT1:
	case M4U_LARB20_PORT2:
	case M4U_LARB20_PORT3:
		return true;
	default:
		return false;
	}
}

bool is_mdp_thread(const int32_t hwid, const int32_t thrd)
{
	return hwid == MDP_HWID && thrd == MDP_THR_IDX;
}
