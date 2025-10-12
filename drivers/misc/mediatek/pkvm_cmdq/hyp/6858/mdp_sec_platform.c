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

#define WAIT_FOR_EVENTS(pTask, events, count) \
	for (int i = 0; i < count; i++) \
		cmdq_task_wfe(pTask, events[i])
#define MDP_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/* mutex offset */
#define BIT_MUTEX_RDMA0 (1LL << 0)
#define BIT_MUTEX_WROT0 (1LL << 12)
#define BIT_MUTEX_WROT2 (1LL << 22)
#define BIT_MUTEX_HDR0 (1LL << 2)
#define BIT_MUTEX_AAL0 (1LL << 4)
#define BIT_MUTEX_RSZ0 (1LL << 6)
#define BIT_MUTEX_RSZ2 (1LL << 20)
#define BIT_MUTEX_TDSHP0 (1LL << 8)
#define BIT_MUTEX_DLI_ASYNC0 (1LL << 16)
#define BIT_MUTEX_DLI_ASYNC1 (1LL << 17)
#define BIT_MUTEX_DLO_ASYNC0 (1LL << 18)
#define BIT_MUTEX_DLO_ASYNC1 (1LL << 19)
#define BIT_MUTEX_IMG_DL_RELAY0 (1LL << 24)
#define BIT_MUTEX_IMG_DL_RELAY1 (1LL << 25)
/* security disable offset */
#define BIT_Sec_MOUT_RST (1LL << 0)
#define BIT_Sec_DLI0_SEL_IN (1LL << 1)
#define BIT_Sec_DLI1_SEL_IN (1LL << 2)
#define BIT_Sec_RDMA0_MOUT_EN (1LL << 3)
#define BIT_Sec_PQ0_SEL_IN (1LL << 5)
#define BIT_Sec_PQ1_SEL_IN (1LL << 6)
#define BIT_Sec_WROT0_SEL_IN (1LL << 7)
#define BIT_Sec_PQ0_SOUT_SEL (1LL << 9)
#define BIT_Sec_PQ1_SOUT_SEL (1LL << 10)
#define BIT_Sec_DLO0_SOUT_SEL (1LL << 11)
#define BIT_Sec_DLO1_SOUT_SEL (1LL << 12)
#define BIT_Sec_ISP0_MOUT_EN (1LL << 21)
#define BIT_Sec_ISP1_MOUT_EN (1LL << 22)
#define BIT_Sec_AAL0_MOUT_EN (1LL << 23)
#define BIT_Sec_BYP0_MOUT_EN (1LL << 13)
#define BIT_Sec_BYP1_MOUT_EN (1LL << 14)
#define BIT_Sec_BYP0_SEL_IN (1LL << 15)
#define BIT_Sec_BYP1_SEL_IN (1LL << 16)
#define BIT_Sec_RSZ2_SEL_IN (1LL << 17)
#define BIT_Sec_AID_SEL (1LL << 19)
#define BIT_Sec_AID_SEL_MDOE (1LL << 20)

/* sconnect path offset */
#define BIT_Offset_MOUT_RST 0xF04
#define BIT_Offset_DLI0_SEL_IN 0xF14
#define BIT_Offset_DLI1_SEL_IN 0xF18
#define BIT_Offset_RDMA0_MOUT_EN 0xF20
#define BIT_Offset_PQ0_SEL_IN 0xF30
#define BIT_Offset_PQ1_SEL_IN 0xF34
#define BIT_Offset_WROT0_SEL_IN 0xF70
#define BIT_Offset_PQ0_SOUT_SEL 0xF80
#define BIT_Offset_PQ1_SOUT_SEL 0xF84
#define BIT_Offset_DLO0_SOUT_SEL 0xF88
#define BIT_Offset_DLO1_SOUT_SEL 0xF8C
#define BIT_Offset_ISP0_MOUT_EN 0xFB0
#define BIT_Offset_ISP1_MOUT_EN 0xFB4
#define BIT_Offset_AAL0_MOUT_EN 0xFB8
#define BIT_Offset_BYP0_MOUT_EN 0xF90
#define BIT_Offset_BYP1_MOUT_EN 0xF94
#define BIT_Offset_BYP0_SEL_IN 0xF98
#define BIT_Offset_BYP1_SEL_IN 0xF9C
#define BIT_Offset_RSZ2_SEL_IN 0xFA0
#define BIT_Offset_AID_SEL 0xFA8
#define BIT_Offset_AID_SEL_MDOE 0xFAC

#define ROUTINE1_MUTEX_BITS ((BIT_MUTEX_RDMA0) | \
				 (BIT_MUTEX_WROT0))

#define ROUTINE1_SECURE_BITS ((BIT_Sec_MOUT_RST) | \
				 (BIT_Sec_AID_SEL_MDOE) | \
				 (BIT_Sec_BYP0_SEL_IN) | \
				 (BIT_Sec_BYP0_MOUT_EN))

#define ROUTINE2_MUTEX_BITS ((BIT_MUTEX_RDMA0) | \
				 (BIT_MUTEX_HDR0) | \
				 (BIT_MUTEX_AAL0) | \
				 (BIT_MUTEX_RSZ0) | \
				 (BIT_MUTEX_TDSHP0) | \
				 (BIT_MUTEX_WROT0))

#define ROUTINE2_SECURE_BITS ((BIT_Sec_MOUT_RST) | \
				 (BIT_Sec_MOUT_RST) | \
				 (BIT_Sec_AID_SEL_MDOE) | \
				 (BIT_Sec_RDMA0_MOUT_EN) | \
				 (BIT_Sec_BYP0_SEL_IN) | \
				 (BIT_Sec_DLI0_SEL_IN) | \
				 (BIT_Sec_PQ0_SEL_IN) | \
				 (BIT_Sec_AAL0_MOUT_EN) | \
				 (BIT_Sec_DLO0_SOUT_SEL) | \
				 (BIT_Sec_BYP0_MOUT_EN) | \
				 (BIT_Sec_WROT0_SEL_IN) | \
				 (BIT_Sec_PQ0_SOUT_SEL))


#define ROUTINE3_MUTEX_BITS ((BIT_MUTEX_RDMA0) | \
				 (BIT_MUTEX_HDR0) | \
				 (BIT_MUTEX_AAL0) | \
				 (BIT_MUTEX_RSZ0) | \
				 (BIT_MUTEX_TDSHP0) | \
				 (BIT_MUTEX_RSZ2) | \
				 (BIT_MUTEX_WROT2) | \
				 (BIT_MUTEX_WROT0))

#define ROUTINE3_SECURE_BITS ((BIT_Sec_MOUT_RST) | \
				 (BIT_Sec_MOUT_RST) | \
				 (BIT_Sec_AID_SEL_MDOE) | \
				 (BIT_Sec_RDMA0_MOUT_EN) | \
				 (BIT_Sec_BYP0_SEL_IN) | \
				 (BIT_Sec_DLI0_SEL_IN) | \
				 (BIT_Sec_PQ0_SEL_IN) | \
				 (BIT_Sec_AAL0_MOUT_EN) | \
				 (BIT_Sec_DLO0_SOUT_SEL) | \
				 (BIT_Sec_BYP0_MOUT_EN) | \
				 (BIT_Sec_WROT0_SEL_IN) | \
				 (BIT_Sec_RSZ2_SEL_IN) | \
				 (BIT_Sec_PQ0_SOUT_SEL))


#define ROUTINE4_MUTEX_BITS ((BIT_MUTEX_DLI_ASYNC0) | \
				 (BIT_MUTEX_IMG_DL_RELAY0) | \
				 (BIT_MUTEX_RSZ2) | \
				 (BIT_MUTEX_WROT2))

#define ROUTINE4_SECURE_BITS ((BIT_Sec_MOUT_RST) | \
				 (BIT_Sec_AID_SEL_MDOE) | \
				 (BIT_Sec_ISP0_MOUT_EN) | \
				 (BIT_Sec_RSZ2_SEL_IN))

#define ROUTINE5_MUTEX_BITS ((BIT_MUTEX_DLI_ASYNC0) | \
				 (BIT_MUTEX_IMG_DL_RELAY0) | \
				 (BIT_MUTEX_HDR0) | \
				 (BIT_MUTEX_AAL0) | \
				 (BIT_MUTEX_RSZ2) | \
				 (BIT_MUTEX_WROT2))

#define ROUTINE5_SECURE_BITS ((BIT_Sec_MOUT_RST) | \
				 (BIT_Sec_MOUT_RST) | \
				 (BIT_Sec_AID_SEL_MDOE) | \
				 (BIT_Sec_ISP0_MOUT_EN) | \
				 (BIT_Sec_DLI0_SEL_IN) | \
				 (BIT_Sec_RDMA0_MOUT_EN) | \
				 (BIT_Sec_AAL0_MOUT_EN) | \
				 (BIT_Sec_RSZ2_SEL_IN))


#define ROUTINE6_MUTEX_BITS ((BIT_MUTEX_DLI_ASYNC0) | \
				 (BIT_MUTEX_IMG_DL_RELAY0) | \
				 (BIT_MUTEX_HDR0) | \
				 (BIT_MUTEX_AAL0) | \
				 (BIT_MUTEX_RSZ0) | \
				 (BIT_MUTEX_TDSHP0) | \
				 (BIT_MUTEX_WROT0))

#define ROUTINE6_SECURE_BITS ((BIT_Sec_MOUT_RST) | \
				 (BIT_Sec_AID_SEL_MDOE) | \
				 (BIT_Sec_ISP0_MOUT_EN) | \
				 (BIT_Sec_DLI0_SEL_IN) | \
				 (BIT_Sec_RDMA0_MOUT_EN) | \
				 (BIT_Sec_PQ0_SEL_IN) | \
				 (BIT_Sec_AAL0_MOUT_EN) | \
				 (BIT_Sec_WROT0_SEL_IN) | \
				 (BIT_Sec_PQ0_SOUT_SEL) | \
				 (BIT_Sec_BYP0_SEL_IN) | \
				 (BIT_Sec_DLO0_SOUT_SEL))

#define ROUTINE7_MUTEX_BITS ((BIT_MUTEX_DLI_ASYNC0) | \
				 (BIT_MUTEX_IMG_DL_RELAY0) | \
				 (BIT_MUTEX_HDR0) | \
				 (BIT_MUTEX_AAL0) | \
				 (BIT_MUTEX_RSZ0) | \
				 (BIT_MUTEX_RSZ2) | \
				 (BIT_MUTEX_TDSHP0) | \
				 (BIT_MUTEX_WROT2) | \
				 (BIT_MUTEX_WROT0))

#define ROUTINE7_SECURE_BITS ((BIT_Sec_MOUT_RST) | \
				 (BIT_Sec_AID_SEL_MDOE) | \
				 (BIT_Sec_ISP0_MOUT_EN) | \
				 (BIT_Sec_DLI0_SEL_IN) | \
				 (BIT_Sec_RDMA0_MOUT_EN) | \
				 (BIT_Sec_PQ0_SEL_IN) | \
				 (BIT_Sec_AAL0_MOUT_EN) | \
				 (BIT_Sec_WROT0_SEL_IN) | \
				 (BIT_Sec_PQ0_SOUT_SEL) | \
				 (BIT_Sec_BYP0_SEL_IN) | \
				 (BIT_Sec_DLO0_SOUT_SEL) | \
				 (BIT_Sec_RSZ2_SEL_IN))

#define ROUTINE8_MUTEX_BITS ((BIT_MUTEX_DLI_ASYNC1) | \
				 (BIT_MUTEX_IMG_DL_RELAY1) | \
				 (BIT_MUTEX_RSZ2) | \
				 (BIT_MUTEX_WROT2))

#define ROUTINE8_SECURE_BITS ((BIT_Sec_MOUT_RST) | \
				 (BIT_Sec_AID_SEL_MDOE) | \
				 (BIT_Sec_ISP1_MOUT_EN) | \
				 (BIT_Sec_RSZ2_SEL_IN))

#define ROUTINE9_MUTEX_BITS ((BIT_MUTEX_DLI_ASYNC1) | \
				 (BIT_MUTEX_IMG_DL_RELAY1) | \
				 (BIT_MUTEX_HDR0) | \
				 (BIT_MUTEX_AAL0) | \
				 (BIT_MUTEX_RSZ2) | \
				 (BIT_MUTEX_WROT2))

#define ROUTINE9_SECURE_BITS ((BIT_Sec_MOUT_RST) | \
				 (BIT_Sec_MOUT_RST) | \
				 (BIT_Sec_AID_SEL_MDOE) | \
				 (BIT_Sec_ISP1_MOUT_EN) | \
				 (BIT_Sec_DLI0_SEL_IN) | \
				 (BIT_Sec_RDMA0_MOUT_EN) | \
				 (BIT_Sec_AAL0_MOUT_EN) | \
				 (BIT_Sec_RSZ2_SEL_IN))

#define ROUTINE10_MUTEX_BITS ((BIT_MUTEX_DLI_ASYNC1) | \
				 (BIT_MUTEX_IMG_DL_RELAY1) | \
				 (BIT_MUTEX_HDR0) | \
				 (BIT_MUTEX_AAL0) | \
				 (BIT_MUTEX_RSZ0) | \
				 (BIT_MUTEX_TDSHP0) | \
				 (BIT_MUTEX_WROT0))

#define ROUTINE10_SECURE_BITS ((BIT_Sec_MOUT_RST) | \
				 (BIT_Sec_AID_SEL_MDOE) | \
				 (BIT_Sec_ISP1_MOUT_EN) | \
				 (BIT_Sec_DLI0_SEL_IN) | \
				 (BIT_Sec_RDMA0_MOUT_EN) | \
				 (BIT_Sec_PQ0_SEL_IN) | \
				 (BIT_Sec_AAL0_MOUT_EN) | \
				 (BIT_Sec_WROT0_SEL_IN) | \
				 (BIT_Sec_PQ0_SOUT_SEL) | \
				 (BIT_Sec_BYP0_SEL_IN) | \
				 (BIT_Sec_DLO0_SOUT_SEL))


#define ROUTINE11_MUTEX_BITS ((BIT_MUTEX_DLI_ASYNC1) | \
				 (BIT_MUTEX_IMG_DL_RELAY1) | \
				 (BIT_MUTEX_HDR0) | \
				 (BIT_MUTEX_AAL0) | \
				 (BIT_MUTEX_RSZ0) | \
				 (BIT_MUTEX_RSZ2) | \
				 (BIT_MUTEX_TDSHP0) | \
				 (BIT_MUTEX_WROT2) | \
				 (BIT_MUTEX_WROT0))

#define ROUTINE11_SECURE_BITS ((BIT_Sec_MOUT_RST) | \
				 (BIT_Sec_AID_SEL_MDOE) | \
				 (BIT_Sec_ISP1_MOUT_EN) | \
				 (BIT_Sec_DLI0_SEL_IN) | \
				 (BIT_Sec_RDMA0_MOUT_EN) | \
				 (BIT_Sec_PQ0_SEL_IN) | \
				 (BIT_Sec_AAL0_MOUT_EN) | \
				 (BIT_Sec_WROT0_SEL_IN) | \
				 (BIT_Sec_PQ0_SOUT_SEL) | \
				 (BIT_Sec_BYP0_SEL_IN) | \
				 (BIT_Sec_DLO0_SOUT_SEL) | \
				 (BIT_Sec_RSZ2_SEL_IN))

#define ROUTINE12_MUTEX_BITS ((BIT_MUTEX_DLI_ASYNC0) | \
				(BIT_MUTEX_IMG_DL_RELAY0) | \
				(BIT_MUTEX_DLO_ASYNC0))

#define ROUTINE12_SECURE_BITS ((BIT_Sec_MOUT_RST) | \
				 (BIT_Sec_AID_SEL_MDOE) | \
				 (BIT_Sec_ISP0_MOUT_EN) | \
				 (BIT_Sec_DLI0_SEL_IN) | \
				 (BIT_Sec_RDMA0_MOUT_EN) | \
				 (BIT_Sec_WROT0_SEL_IN))

/* mdp setting */

enum MDP_mode {
	mdp_wrot0,
	mdp_wrot2,
	mdp_2_out,
	camin_wrot0,
	camin_wrot2,
	camin_2_out,
	camin2_wrot0,
	camin2_wrot2,
	camin2_2_out,
	imgi_img2o,
};

struct ConnectEngine {
	uint32_t offset;
	uint32_t value;
};

struct Routine_set {
	struct ConnectEngine *connectEngines;			//ConnectEngine
	const size_t numConnectEngines;			//ConnectEngine
	uint32_t *dapcEngines;					//DAPC Engine
	const size_t numDapcEngines;			//DAPC Engine
	uint32_t mutexBits;						//Mutex Bits
	uint32_t secureBits;					//Secure Bits
};

uint32_t mdp_wrot0_portEngines[] = {CMDQ_SEC_MDP_RDMA0, CMDQ_SEC_MDP_WROT0};
size_t mdp_wrot0_num_PortEngine = MDP_ARRAY_SIZE(mdp_wrot0_portEngines);

uint32_t mdp_wrot0_events[] = {
	CMDQ_EVENT_MDPSYS_MDP_RDMA0_SOF,
	CMDQ_EVENT_MDPSYS_MDP_RDMA0_FRAME_DONE,
	CMDQ_EVENT_MDPSYS_MDP_WROT0_SOF,
	CMDQ_EVENT_MDPSYS_MDP_WROT0_FRAME_DONE
};
size_t mdp_wrot0_num_event = MDP_ARRAY_SIZE(mdp_wrot0_events);

uint32_t mdp_wrot2_portEngines[] = {CMDQ_SEC_MDP_RDMA0, CMDQ_SEC_MDP_WROT2};
size_t mdp_wrot2_num_PortEngine = MDP_ARRAY_SIZE(mdp_wrot2_portEngines);

uint32_t mdp_wrot2_events[] = {
	CMDQ_EVENT_MDPSYS_MDP_RDMA0_SOF,
	CMDQ_EVENT_MDPSYS_MDP_RDMA0_FRAME_DONE,
	CMDQ_EVENT_MDPSYS_MDP_WROT2_SOF,
	CMDQ_EVENT_MDPSYS_MDP_WROT2_FRAME_DONE
};
size_t mdp_wrot2_num_event = MDP_ARRAY_SIZE(mdp_wrot2_events);

uint32_t mdp_2_out_portEngines[] = {
	CMDQ_SEC_MDP_RDMA0,
	CMDQ_SEC_MDP_WROT0,
	CMDQ_SEC_MDP_WROT2
};
size_t mdp_2_out_num_PortEngine = MDP_ARRAY_SIZE(mdp_2_out_portEngines);

uint32_t mdp_2_out_events[] = {
	CMDQ_EVENT_MDPSYS_MDP_RDMA0_SOF,
	CMDQ_EVENT_MDPSYS_MDP_RDMA0_FRAME_DONE,
	CMDQ_EVENT_MDPSYS_MDP_WROT0_SOF,
	CMDQ_EVENT_MDPSYS_MDP_WROT0_FRAME_DONE,
	CMDQ_EVENT_MDPSYS_MDP_WROT2_SOF,
	CMDQ_EVENT_MDPSYS_MDP_WROT2_FRAME_DONE
};
size_t mdp_2_out_num_event = MDP_ARRAY_SIZE(mdp_2_out_events);

uint32_t camin_wrot0_portEngines[] = {CMDQ_SEC_MDP_WROT0};
size_t camin_wrot0_num_PortEngine = MDP_ARRAY_SIZE(camin_wrot0_portEngines);

uint32_t camin_wrot0_events[] = {
	CMDQ_EVENT_MDPSYS_MDP_WROT0_SOF,
	CMDQ_EVENT_MDPSYS_MDP_WROT0_FRAME_DONE
};
size_t camin_wrot0_num_event = MDP_ARRAY_SIZE(camin_wrot0_events);

uint32_t camin_wrot2_portEngines[] = {CMDQ_SEC_MDP_WROT2};
size_t camin_wrot2_num_PortEngine = MDP_ARRAY_SIZE(camin_wrot2_portEngines);

uint32_t camin_wrot2_events[] = {
	CMDQ_EVENT_MDPSYS_MDP_WROT2_SOF,
	CMDQ_EVENT_MDPSYS_MDP_WROT2_FRAME_DONE
};
size_t camin_wrot2_num_event = MDP_ARRAY_SIZE(camin_wrot2_events);

uint32_t camin_2_out_portEngines[] = {
	CMDQ_SEC_MDP_WROT0,
	CMDQ_SEC_MDP_WROT2,
};
size_t camin_2_out_num_PortEngine = MDP_ARRAY_SIZE(camin_2_out_portEngines);

uint32_t camin_2_out_events[] = {
	CMDQ_EVENT_MDPSYS_MDP_WROT0_SOF,
	CMDQ_EVENT_MDPSYS_MDP_WROT0_FRAME_DONE,
	CMDQ_EVENT_MDPSYS_MDP_WROT2_SOF,
	CMDQ_EVENT_MDPSYS_MDP_WROT2_FRAME_DONE
};
size_t camin_2_out_num_event = MDP_ARRAY_SIZE(camin_2_out_events);

uint32_t camin2_wrot0_portEngines[] = {CMDQ_SEC_MDP_RDMA0, CMDQ_SEC_MDP_WROT0};
size_t camin2_wrot0_num_PortEngine = MDP_ARRAY_SIZE(camin2_wrot0_portEngines);

uint32_t camin2_wrot0_events[] = {
	CMDQ_EVENT_MDPSYS_MDP_WROT0_SOF,
	CMDQ_EVENT_MDPSYS_MDP_WROT0_FRAME_DONE
};
size_t camin2_wrot0_num_event = MDP_ARRAY_SIZE(camin2_wrot0_events);

uint32_t camin2_wrot2_portEngines[] = {CMDQ_SEC_MDP_WROT2};
size_t camin2_wrot2_num_PortEngine = MDP_ARRAY_SIZE(camin2_wrot2_portEngines);

uint32_t camin2_wrot2_events[] = {
	CMDQ_EVENT_MDPSYS_MDP_WROT2_SOF,
	CMDQ_EVENT_MDPSYS_MDP_WROT2_FRAME_DONE
};
size_t camin2_wrot2_num_event = MDP_ARRAY_SIZE(camin2_wrot2_events);

uint32_t camin2_2_out_portEngines[] = {
	CMDQ_SEC_MDP_WROT0,
	CMDQ_SEC_MDP_WROT2,
};
size_t camin2_2_out_num_PortEngine = MDP_ARRAY_SIZE(camin2_2_out_portEngines);

uint32_t camin2_2_out_events[] = {
	CMDQ_EVENT_MDPSYS_MDP_WROT0_SOF,
	CMDQ_EVENT_MDPSYS_MDP_WROT0_FRAME_DONE,
	CMDQ_EVENT_MDPSYS_MDP_WROT2_SOF,
	CMDQ_EVENT_MDPSYS_MDP_WROT2_FRAME_DONE
};
size_t camin2_2_out_num_event = MDP_ARRAY_SIZE(camin2_2_out_events);

uint32_t imgi_img2o_portEngines[] = {};
size_t imgi_img2o_num_PortEngine = MDP_ARRAY_SIZE(imgi_img2o_portEngines);

uint32_t imgi_img2o_events[] = {};
size_t imgi_img2o_num_event = MDP_ARRAY_SIZE(imgi_img2o_events);


/* 1. mdp routine func */
void rt_set_dapc(struct TaskStruct *pTask,enum MDP_mode mode, uint32_t *dapcEngines,
				 const size_t dapcCount, bool enable)
{
	for (int i = 0; i < dapcCount; i++) {
		pTask->enginesNeedDAPC = 1LL << dapcEngines[i];
		cmdq_tz_set_dapc_security_reg(pTask, enable, true);
	}
}

void rt_set_port(struct TaskStruct *pTask, uint32_t *portEngines,
				 int portCount, bool enable)
{
	for (int i = 0; i < portCount; i++) {
		pTask->enginesNeedPortSecurity = 1LL << portEngines[i];
		cmdq_tz_set_port_security_reg(pTask, enable, true);
	}
}

void rt_set_path(struct TaskStruct *pTask, struct ConnectEngine *connectEngines,
				 const size_t count, bool enable)
{
	for (int i = 0; i < count; i++) {
		cmdq_task_write_value_addr(pTask, (MDPSYS_BASE_ADDR + connectEngines[i].offset),
			enable ? connectEngines[i].value : 0x0, UINT_MAX);
	}
}

void rt_set_mdpsys(struct TaskStruct *pTask, uint32_t securityValues, bool enable)
{
	cmdq_task_write_value_addr(pTask, SEC_DISABLE_ADDR,
								enable ? securityValues : 0x0, UINT_MAX);
}

void rt_mutex(struct TaskStruct *pTask, uint32_t mutexValue, bool enable)
{
	cmdq_task_write_value_addr(pTask, DISP_MUTEX3_MOD0,
								enable ? mutexValue : 0x0, UINT_MAX);
	cmdq_task_write_value_addr(pTask, MDP_MUTEX3_MOD0_EN,
								enable ? 0x1 : 0x0, UINT_MAX);
}

void rt_exe(struct TaskStruct *pTask,enum MDP_mode mode, uint32_t *dapcEngines,
			const size_t dapcCount, struct ConnectEngine *connectEngines,
			size_t numConnect, uint32_t securityValues, uint32_t mutexValue)
{
	uint32_t *portEngines;
	size_t portCount;
	uint32_t *events;
	size_t eventCount;

	switch (mode) {
	case mdp_wrot0:
		portEngines = mdp_wrot0_portEngines;
		portCount = mdp_wrot0_num_PortEngine;
		events = mdp_wrot0_events;
		eventCount = mdp_wrot0_num_event;
		break;
	case mdp_wrot2:
		portEngines = mdp_wrot2_portEngines;
		portCount = mdp_wrot2_num_PortEngine;
		events = mdp_wrot2_events;
		eventCount = mdp_wrot2_num_event;
		break;
	case mdp_2_out:
		portEngines = mdp_2_out_portEngines;
		portCount = mdp_2_out_num_PortEngine;
		events = mdp_2_out_events;
		eventCount = mdp_2_out_num_event;
		break;
	case camin_wrot0:
		portEngines = camin_wrot0_portEngines;
		portCount = camin_wrot0_num_PortEngine;
		events = camin_wrot0_events;
		eventCount = camin_wrot0_num_event;
		break;
	case camin_wrot2:
		portEngines = camin_wrot2_portEngines;
		portCount = camin_wrot2_num_PortEngine;
		events = camin_wrot2_events;
		eventCount = camin_wrot2_num_event;
		break;
	case camin_2_out:
		portEngines = camin_2_out_portEngines;
		portCount = camin_2_out_num_PortEngine;
		events = camin_2_out_events;
		eventCount = camin_2_out_num_event;
		break;
	case camin2_wrot0:
		portEngines = camin2_wrot0_portEngines;
		portCount = camin2_wrot0_num_PortEngine;
		events = camin2_wrot0_events;
		eventCount = camin2_wrot0_num_event;
		break;
	case camin2_wrot2:
		portEngines = camin2_wrot2_portEngines;
		portCount = camin2_wrot2_num_PortEngine;
		events = camin2_wrot2_events;
		eventCount = camin2_wrot2_num_event;
		break;
	case camin2_2_out:
		portEngines = camin2_2_out_portEngines;
		portCount = camin2_2_out_num_PortEngine;
		events = camin2_2_out_events;
		eventCount = camin2_2_out_num_event;
		break;
	case imgi_img2o:
		portEngines = imgi_img2o_portEngines;
		portCount = imgi_img2o_num_PortEngine;
		events = imgi_img2o_events;
		eventCount = imgi_img2o_num_event;
		break;
	default:
		return; // Handle invalid mode
	}

	/* Set MDP */
	rt_set_dapc(pTask, mode, dapcEngines, dapcCount, true);
	cmdq_task_write_value_addr(pTask, GCED_BASE_PA + 0xa8, 0xBAD1, UINT_MAX);

	if (mode != mdp_wrot0 && mode != mdp_wrot2 && mode != mdp_2_out)
		cmdq_tz_isp_secure((void *)pTask, true);
	cmdq_task_write_value_addr(pTask, GCED_BASE_PA + 0xac, 0xBAD2, UINT_MAX);

	rt_set_mdpsys(pTask, securityValues, true);
	rt_set_path(pTask, connectEngines, numConnect, true);
	rt_set_port(pTask, portEngines, portCount, true);
	/* Start execution */
	rt_mutex(pTask, mutexValue, true);
	cmdq_task_write_value_addr(pTask, GCED_BASE_PA + 0xbC, 0xBADBAD, UINT_MAX);


	/* Set ISP event and wait ISP frame done */
	if (mode != mdp_wrot0 && mode != mdp_wrot2 && mode != mdp_2_out) {
		cmdq_task_set_event(pTask, CMDQ_SYNC_TOKEN_TZMP_ISP_SET);
		cmdq_task_wfe(pTask, CMDQ_SYNC_TOKEN_TZMP_ISP_WAIT);

	}

	/* Wait MDP frame done */
	WAIT_FOR_EVENTS(pTask, events, eventCount);

	/* Clean & Reset */
	rt_mutex(pTask, mutexValue, false);
	rt_set_port(pTask, portEngines, portCount, false);
	rt_set_path(pTask, connectEngines, numConnect, false);
	rt_set_mdpsys(pTask, securityValues, false);
	cmdq_task_write_value_addr(pTask, GCED_BASE_PA + 0xb0, 0xBAD3, UINT_MAX);

	if (mode != mdp_wrot0 && mode != mdp_wrot2 && mode != mdp_2_out)
		cmdq_tz_isp_normal((void *)pTask, true);

	cmdq_task_write_value_addr(pTask, GCED_BASE_PA + 0xb4, 0xBAD4, UINT_MAX);
	rt_set_dapc(pTask, mode, dapcEngines, dapcCount, false);

}

/* 2. routine setting */
struct ConnectEngine connectEngines_1[] = {
	{BIT_Offset_BYP0_SEL_IN, 0x0},
	{BIT_Offset_BYP0_MOUT_EN, 0x1}
};

struct ConnectEngine connectEngines_2[] = {
	{BIT_Offset_BYP0_MOUT_EN, 0x4},
	{BIT_Offset_RDMA0_MOUT_EN, 0x1},
	{BIT_Offset_BYP0_SEL_IN, 0x1},
	{BIT_Offset_AAL0_MOUT_EN, 0x1},
	{BIT_Offset_WROT0_SEL_IN, 0x1},
};

struct ConnectEngine connectEngines_3[] = {
	{BIT_Offset_BYP0_MOUT_EN, 0x6},
	{BIT_Offset_DLI0_SEL_IN, 0x0},
	{BIT_Offset_RDMA0_MOUT_EN, 0x1},
	{BIT_Offset_PQ0_SEL_IN, 0x0},
	{BIT_Offset_AAL0_MOUT_EN, 0x1},
	{BIT_Offset_WROT0_SEL_IN, 0x1},
	{BIT_Offset_PQ0_SOUT_SEL, 0x0},
	{BIT_Offset_BYP0_SEL_IN, 0x1},
	{BIT_Offset_DLO0_SOUT_SEL, 0x0},
	{BIT_Offset_RSZ2_SEL_IN, 0x1},
};

struct ConnectEngine connectEngines_4[] = {
	{BIT_Offset_ISP0_MOUT_EN, 0x2},
	{BIT_Offset_RSZ2_SEL_IN, 0x2},
};

struct ConnectEngine connectEngines_5[] = {
	{BIT_Offset_ISP0_MOUT_EN, 0x1},
	{BIT_Offset_DLI0_SEL_IN, 0x2},
	{BIT_Offset_RDMA0_MOUT_EN, 0x1},
	{BIT_Offset_AAL0_MOUT_EN, 0x2},
	{BIT_Offset_RSZ2_SEL_IN, 0x4},
};

struct ConnectEngine connectEngines_6[] = {
	{BIT_Offset_ISP0_MOUT_EN, 0x1},
	{BIT_Offset_DLI0_SEL_IN, 0x2},
	{BIT_Offset_RDMA0_MOUT_EN, 0x1},
	{BIT_Offset_PQ0_SEL_IN, 0x0},
	{BIT_Offset_AAL0_MOUT_EN, 0x1},
	{BIT_Offset_WROT0_SEL_IN, 0x1},
	{BIT_Offset_PQ0_SOUT_SEL, 0x0},
	{BIT_Offset_BYP0_SEL_IN, 0x1},
	{BIT_Offset_DLO0_SOUT_SEL, 0x0},
};

struct ConnectEngine connectEngines_7[] = {
	{BIT_Offset_ISP0_MOUT_EN, 0x3},
	{BIT_Offset_DLI0_SEL_IN, 0x2},
	{BIT_Offset_RDMA0_MOUT_EN, 0x1},
	{BIT_Offset_PQ0_SEL_IN, 0x0},
	{BIT_Offset_AAL0_MOUT_EN, 0x1},
	{BIT_Offset_WROT0_SEL_IN, 0x1},
	{BIT_Offset_PQ0_SOUT_SEL, 0x0},
	{BIT_Offset_BYP0_SEL_IN, 0x1},
	{BIT_Offset_DLO0_SOUT_SEL, 0x0},
	{BIT_Sec_RSZ2_SEL_IN, 0x2}
};

struct ConnectEngine connectEngines_8[] = {
	{BIT_Offset_ISP1_MOUT_EN, 0x8},
	{BIT_Offset_RSZ2_SEL_IN, 0x3},
};

struct ConnectEngine connectEngines_9[] = {
	{BIT_Offset_ISP1_MOUT_EN, 0x4},
	{BIT_Offset_DLI0_SEL_IN, 0x3},
	{BIT_Offset_RDMA0_MOUT_EN, 0x1},
	{BIT_Offset_AAL0_MOUT_EN, 0x2},
	{BIT_Offset_RSZ2_SEL_IN, 0x4},
};

struct ConnectEngine connectEngines_10[] = {
	{BIT_Offset_ISP1_MOUT_EN, 0x4},
	{BIT_Offset_DLI0_SEL_IN, 0x3},
	{BIT_Offset_RDMA0_MOUT_EN, 0x1},
	{BIT_Offset_PQ0_SEL_IN, 0x0},
	{BIT_Offset_AAL0_MOUT_EN, 0x1},
	{BIT_Offset_WROT0_SEL_IN, 0x1},
	{BIT_Offset_PQ0_SOUT_SEL, 0x0},
	{BIT_Offset_BYP0_SEL_IN, 0x1},
	{BIT_Offset_DLO0_SOUT_SEL, 0x0},
};

struct ConnectEngine connectEngines_11[] = {
	{BIT_Offset_ISP1_MOUT_EN, 0xC},
	{BIT_Offset_DLI0_SEL_IN, 0x3},
	{BIT_Offset_RDMA0_MOUT_EN, 0x1},
	{BIT_Offset_PQ0_SEL_IN, 0x0},
	{BIT_Offset_AAL0_MOUT_EN, 0x1},
	{BIT_Offset_WROT0_SEL_IN, 0x1},
	{BIT_Offset_PQ0_SOUT_SEL, 0x0},
	{BIT_Offset_BYP0_SEL_IN, 0x1},
	{BIT_Offset_DLO0_SOUT_SEL, 0x0},
	{BIT_Offset_RSZ2_SEL_IN, 0x3},
};

struct ConnectEngine connectEngines_12[] = {
	{BIT_Offset_ISP0_MOUT_EN, 0x1},
	{BIT_Offset_DLI0_SEL_IN, 0x2},
	{BIT_Offset_RDMA0_MOUT_EN, 0x2},
	{BIT_Offset_WROT0_SEL_IN, 0x0},
};


uint32_t dapcEngine_1[] = {CMDQ_SEC_MDP_RDMA0, CMDQ_SEC_MDP_WROT0};
const size_t numConnect_1 = MDP_ARRAY_SIZE(connectEngines_1);
const size_t num_DapcEngine_1 = MDP_ARRAY_SIZE(dapcEngine_1);

uint32_t dapcEngine_2[] = {
	CMDQ_SEC_MDP_RDMA0,
	CMDQ_SEC_MDP_HDR0,
	CMDQ_SEC_MDP_AAL0,
	CMDQ_SEC_MDP_RSZ0,
	CMDQ_SEC_MDP_TDSHP0,
	CMDQ_SEC_MDP_WROT0
};
const size_t numConnect_2 = MDP_ARRAY_SIZE(connectEngines_2);
const size_t num_DapcEngine_2 = MDP_ARRAY_SIZE(dapcEngine_2);

uint32_t dapcEngine_3[] = {
	CMDQ_SEC_MDP_RDMA0,
	CMDQ_SEC_MDP_HDR0,
	CMDQ_SEC_MDP_AAL0,
	CMDQ_SEC_MDP_RSZ0,
	CMDQ_SEC_MDP_TDSHP0,
	CMDQ_SEC_MDP_RSZ2,
	CMDQ_SEC_MDP_WROT2,
	CMDQ_SEC_MDP_WROT0
};
const size_t numConnect_3 = MDP_ARRAY_SIZE(connectEngines_3);
const size_t num_DapcEngine_3 = MDP_ARRAY_SIZE(dapcEngine_3);

uint32_t dapcEngine_4[] = {
	CMDQ_SEC_MDP_RSZ2,
	CMDQ_SEC_MDP_WROT2
};
const size_t numConnect_4 = MDP_ARRAY_SIZE(connectEngines_4);
const size_t num_DapcEngine_4 = MDP_ARRAY_SIZE(dapcEngine_4);

uint32_t dapcEngine_5[] = {
	CMDQ_SEC_MDP_HDR0,
	CMDQ_SEC_MDP_AAL0,
	CMDQ_SEC_MDP_RSZ2,
	CMDQ_SEC_MDP_WROT2
};
const size_t numConnect_5 = MDP_ARRAY_SIZE(connectEngines_5);
const size_t num_DapcEngine_5 = MDP_ARRAY_SIZE(dapcEngine_5);

uint32_t dapcEngine_6[] = {
	CMDQ_SEC_MDP_HDR0,
	CMDQ_SEC_MDP_AAL0,
	CMDQ_SEC_MDP_RSZ0,
	CMDQ_SEC_MDP_TDSHP0,
	CMDQ_SEC_MDP_WROT0
};
const size_t numConnect_6 = MDP_ARRAY_SIZE(connectEngines_6);
const size_t num_DapcEngine_6 = MDP_ARRAY_SIZE(dapcEngine_6);

uint32_t dapcEngine_7[] = {
	CMDQ_SEC_MDP_HDR0,
	CMDQ_SEC_MDP_AAL0,
	CMDQ_SEC_MDP_RSZ0,
	CMDQ_SEC_MDP_RSZ2,
	CMDQ_SEC_MDP_TDSHP0,
	CMDQ_SEC_MDP_WROT2,
	CMDQ_SEC_MDP_WROT0
};
const size_t numConnect_7 = MDP_ARRAY_SIZE(connectEngines_7);
const size_t num_DapcEngine_7 = MDP_ARRAY_SIZE(dapcEngine_7);

uint32_t dapcEngine_8[] = {
	CMDQ_SEC_MDP_RSZ2,
	CMDQ_SEC_MDP_WROT2
};
const size_t numConnect_8 = MDP_ARRAY_SIZE(connectEngines_8);
const size_t num_DapcEngine_8 = MDP_ARRAY_SIZE(dapcEngine_8);

uint32_t dapcEngine_9[] = {
	CMDQ_SEC_MDP_HDR0,
	CMDQ_SEC_MDP_AAL0,
	CMDQ_SEC_MDP_RSZ2,
	CMDQ_SEC_MDP_WROT2
};
const size_t numConnect_9 = MDP_ARRAY_SIZE(connectEngines_9);
const size_t num_DapcEngine_9 = MDP_ARRAY_SIZE(dapcEngine_9);

uint32_t dapcEngine_10[] = {
	CMDQ_SEC_MDP_HDR0,
	CMDQ_SEC_MDP_AAL0,
	CMDQ_SEC_MDP_RSZ0,
	CMDQ_SEC_MDP_TDSHP0,
	CMDQ_SEC_MDP_WROT0
};
const size_t numConnect_10 = MDP_ARRAY_SIZE(connectEngines_10);
const size_t num_DapcEngine_10 = MDP_ARRAY_SIZE(dapcEngine_10);

uint32_t dapcEngine_11[] = {
	CMDQ_SEC_MDP_HDR0,
	CMDQ_SEC_MDP_AAL0,
	CMDQ_SEC_MDP_RSZ0,
	CMDQ_SEC_MDP_RSZ2,
	CMDQ_SEC_MDP_TDSHP0,
	CMDQ_SEC_MDP_WROT2,
	CMDQ_SEC_MDP_WROT0
};
const size_t numConnect_11 = MDP_ARRAY_SIZE(connectEngines_11);
const size_t num_DapcEngine_11 = MDP_ARRAY_SIZE(dapcEngine_11);

uint32_t dapcEngine_12[] = {};
const size_t numConnect_12 = MDP_ARRAY_SIZE(connectEngines_12);
const size_t num_DapcEngine_12 = MDP_ARRAY_SIZE(dapcEngine_12);

struct Routine_set rt_1 = {
	.connectEngines = connectEngines_1,
	.numConnectEngines = numConnect_1,
	.dapcEngines = dapcEngine_1,
	.numDapcEngines = num_DapcEngine_1,
	.mutexBits = ROUTINE1_MUTEX_BITS,
	.secureBits = ROUTINE1_SECURE_BITS,
};

struct Routine_set rt_2 = {
	.connectEngines = connectEngines_2,
	.numConnectEngines = numConnect_2,
	.dapcEngines = dapcEngine_2,
	.numDapcEngines = num_DapcEngine_2,
	.mutexBits = ROUTINE2_MUTEX_BITS,
	.secureBits = ROUTINE2_SECURE_BITS,
};

struct Routine_set rt_3 = {
	.connectEngines = connectEngines_3,
	.numConnectEngines = numConnect_3,
	.dapcEngines = dapcEngine_3,
	.numDapcEngines = num_DapcEngine_3,
	.mutexBits = ROUTINE3_MUTEX_BITS,
	.secureBits = ROUTINE3_SECURE_BITS,
};

struct Routine_set rt_4 = {
	.connectEngines = connectEngines_4,
	.numConnectEngines = numConnect_4,
	.dapcEngines = dapcEngine_4,
	.numDapcEngines = num_DapcEngine_4,
	.mutexBits = ROUTINE4_MUTEX_BITS,
	.secureBits = ROUTINE4_SECURE_BITS,
};

struct Routine_set rt_5 = {
	.connectEngines = connectEngines_5,
	.numConnectEngines = numConnect_5,
	.dapcEngines = dapcEngine_5,
	.numDapcEngines = num_DapcEngine_5,
	.mutexBits = ROUTINE5_MUTEX_BITS,
	.secureBits = ROUTINE5_SECURE_BITS,
};

struct Routine_set rt_6 = {
	.connectEngines = connectEngines_6,
	.numConnectEngines = numConnect_6,
	.dapcEngines = dapcEngine_6,
	.numDapcEngines = num_DapcEngine_6,
	.mutexBits = ROUTINE6_MUTEX_BITS,
	.secureBits = ROUTINE6_SECURE_BITS,
};

struct Routine_set rt_7 = {
	.connectEngines = connectEngines_7,
	.numConnectEngines = numConnect_7,
	.dapcEngines = dapcEngine_7,
	.numDapcEngines = num_DapcEngine_7,
	.mutexBits = ROUTINE7_MUTEX_BITS,
	.secureBits = ROUTINE7_SECURE_BITS,
};

struct Routine_set rt_8 = {
	.connectEngines = connectEngines_8,
	.numConnectEngines = numConnect_8,
	.dapcEngines = dapcEngine_8,
	.numDapcEngines = num_DapcEngine_8,
	.mutexBits = ROUTINE8_MUTEX_BITS,
	.secureBits = ROUTINE8_SECURE_BITS,
};

struct Routine_set rt_9 = {
	.connectEngines = connectEngines_9,
	.numConnectEngines = numConnect_9,
	.dapcEngines = dapcEngine_9,
	.numDapcEngines = num_DapcEngine_9,
	.mutexBits = ROUTINE9_MUTEX_BITS,
	.secureBits = ROUTINE9_SECURE_BITS,
};

struct Routine_set rt_10 = {
	.connectEngines = connectEngines_10,
	.numConnectEngines = numConnect_10,
	.dapcEngines = dapcEngine_10,
	.numDapcEngines = num_DapcEngine_10,
	.mutexBits = ROUTINE10_MUTEX_BITS,
	.secureBits = ROUTINE10_SECURE_BITS,
};

struct Routine_set rt_11 = {
	.connectEngines = connectEngines_11,
	.numConnectEngines = numConnect_11,
	.dapcEngines = dapcEngine_11,
	.numDapcEngines = num_DapcEngine_11,
	.mutexBits = ROUTINE11_MUTEX_BITS,
	.secureBits = ROUTINE11_SECURE_BITS,
};

struct Routine_set rt_12 = {
	.connectEngines = connectEngines_12,
	.numConnectEngines = numConnect_12,
	.dapcEngines = dapcEngine_12,
	.numDapcEngines = num_DapcEngine_12,
	.mutexBits = ROUTINE12_MUTEX_BITS,
	.secureBits = ROUTINE12_SECURE_BITS,
};


/* 3. routine definition*/
void Routine1(struct TaskStruct *pTask)
{
	rt_exe(pTask, mdp_wrot0, rt_1.dapcEngines, rt_1.numDapcEngines,
				rt_1.connectEngines, rt_1.numConnectEngines,
				rt_1.secureBits, rt_1.mutexBits);
	cmdq_task_set_event(pTask, CMDQ_SYNC_TOKEN_TZMP_MDP_SET);
	cmdq_task_finalize_loop(pTask);
}
void Routine2(struct TaskStruct *pTask)
{
	rt_exe(pTask, mdp_wrot0, rt_2.dapcEngines, rt_2.numDapcEngines,
				rt_2.connectEngines, rt_2.numConnectEngines,
				rt_2.secureBits, rt_2.mutexBits);

	cmdq_task_set_event(pTask, CMDQ_SYNC_TOKEN_TZMP_MDP_SET);
	cmdq_task_finalize_loop(pTask);
}

void Routine3(struct TaskStruct *pTask)
{
	rt_exe(pTask, mdp_2_out, rt_3.dapcEngines, rt_3.numDapcEngines,
				rt_3.connectEngines, rt_3.numConnectEngines,
				rt_3.secureBits, rt_3.mutexBits);

	cmdq_task_set_event(pTask, CMDQ_SYNC_TOKEN_TZMP_MDP_SET);
	cmdq_task_finalize_loop(pTask);

}

void Routine4(struct TaskStruct *pTask)
{
	rt_exe(pTask, camin_wrot2, rt_4.dapcEngines, rt_4.numDapcEngines,
			rt_4.connectEngines, rt_4.numConnectEngines,
			rt_4.secureBits, rt_4.mutexBits);

	cmdq_task_set_event(pTask, CMDQ_SYNC_TOKEN_TZMP_MDP_SET);
	cmdq_task_finalize_loop(pTask);

}

void Routine5(struct TaskStruct *pTask)
{
	rt_exe(pTask, camin_wrot2, rt_5.dapcEngines, rt_5.numDapcEngines,
			rt_5.connectEngines, rt_5.numConnectEngines,
			rt_5.secureBits, rt_5.mutexBits);

	cmdq_task_set_event(pTask, CMDQ_SYNC_TOKEN_TZMP_MDP_SET);
	cmdq_task_finalize_loop(pTask);

}

void Routine6(struct TaskStruct *pTask)
{
	rt_exe(pTask, camin_wrot0, rt_6.dapcEngines, rt_6.numDapcEngines,
			rt_6.connectEngines, rt_6.numConnectEngines,
			rt_6.secureBits, rt_6.mutexBits);

	cmdq_task_set_event(pTask, CMDQ_SYNC_TOKEN_TZMP_MDP_SET);
	cmdq_task_finalize_loop(pTask);
}

void Routine7(struct TaskStruct *pTask)
{
	rt_exe(pTask, camin_2_out, rt_7.dapcEngines, rt_7.numDapcEngines,
			rt_7.connectEngines, rt_7.numConnectEngines,
			rt_7.secureBits, rt_7.mutexBits);

	cmdq_task_set_event(pTask, CMDQ_SYNC_TOKEN_TZMP_MDP_SET);
	cmdq_task_finalize_loop(pTask);
}

void Routine8(struct TaskStruct *pTask)
{
	rt_exe(pTask, camin2_wrot2, rt_8.dapcEngines, rt_8.numDapcEngines,
			rt_8.connectEngines, rt_8.numConnectEngines,
			rt_8.secureBits, rt_8.mutexBits);

	cmdq_task_set_event(pTask, CMDQ_SYNC_TOKEN_TZMP_MDP_SET);
	cmdq_task_finalize_loop(pTask);
}

void Routine9(struct TaskStruct *pTask)
{
	rt_exe(pTask, camin2_wrot2, rt_9.dapcEngines, rt_9.numDapcEngines,
			rt_9.connectEngines, rt_9.numConnectEngines,
			rt_9.secureBits, rt_9.mutexBits);

	cmdq_task_set_event(pTask, CMDQ_SYNC_TOKEN_TZMP_MDP_SET);
	cmdq_task_finalize_loop(pTask);
}

void Routine10(struct TaskStruct *pTask)
{
	rt_exe(pTask, camin2_wrot0, rt_10.dapcEngines, rt_10.numDapcEngines,
			rt_10.connectEngines, rt_10.numConnectEngines,
			rt_10.secureBits, rt_10.mutexBits);

	cmdq_task_set_event(pTask, CMDQ_SYNC_TOKEN_TZMP_MDP_SET);
	cmdq_task_finalize_loop(pTask);
}

void Routine11(struct TaskStruct *pTask)
{
	rt_exe(pTask, camin2_2_out, rt_11.dapcEngines, rt_11.numDapcEngines,
			rt_11.connectEngines, rt_11.numConnectEngines,
			rt_11.secureBits, rt_11.mutexBits);

	cmdq_task_set_event(pTask, CMDQ_SYNC_TOKEN_TZMP_MDP_SET);
	cmdq_task_finalize_loop(pTask);
}

void Routine12(struct TaskStruct *pTask)
{
	rt_exe(pTask, imgi_img2o, rt_12.dapcEngines, rt_12.numDapcEngines,
			rt_12.connectEngines, rt_12.numConnectEngines,
			rt_12.secureBits, rt_12.mutexBits);

	cmdq_task_set_event(pTask, CMDQ_SYNC_TOKEN_TZMP_MDP_SET);
	cmdq_task_finalize_loop(pTask);
}

void cmdq_tz_mdp_handle(struct TaskStruct *pTask)
{
	uint32_t jump_tag_1, jump_tag_2, jump_tag_3, jump_tag_4,
			jump_tag_5, jump_tag_6, jump_tag_7, jump_tag_8,
			jump_tag_9, jump_tag_10, jump_tag_11, jump_tag_12;
	uint64_t shift_pa, curr_pa;
	const uint8_t SPR1 = 1;
	struct cmdq_operand lop, rop;
	struct cmdq_instruction *cmdq_inst;

	cmdq_task_wfe(pTask, CMDQ_SYNC_TOKEN_TZMP_MDP_WAIT);
	cmdq_task_write_value_addr(pTask, GCED_BASE_PA + 0xa4, 0xBAD0, UINT_MAX);

/* condition routine 1 */
	/* other codition jump is same above */
	// assign spr1 = 0, will be revise later
	cmdq_task_assign_command(pTask, SPR1, 0);
	// save assign spr1 instruction offset
	jump_tag_1 = pTask->commandSize - CMDQ_INST_SIZE;
	lop.reg = true;
	lop.idx = 0x8005;
	rop.reg = false;
	rop.value = 0x1;
	// rop.value = 0x0;
	// if verify routine 1 pass, then jump
	cmdq_task_cond_jump(pTask, SPR1, &lop, &rop, CMDQ_EQUAL);
	// verify_mdp_cpr(pTask, SPR1, lop, rop, 0x1);

/* condition routine 2 */
	/* other codition jump is same above */
	// assign spr1 = 0, will be revise later
	cmdq_task_assign_command(pTask, SPR1, 0);
	// save assign spr1 instruction offset
	jump_tag_2 = pTask->commandSize - CMDQ_INST_SIZE;
	lop.reg = true;
	lop.idx = 0x8005;
	rop.reg = false;
	rop.value = 0x2;
	cmdq_task_cond_jump(pTask, SPR1, &lop, &rop, CMDQ_EQUAL);

/* condition routine 3 */
	cmdq_task_assign_command(pTask, SPR1, 0);
	jump_tag_3 = pTask->commandSize - CMDQ_INST_SIZE;
	lop.reg = true;
	lop.idx = 0x8005;
	rop.reg = false;
	rop.value = 0x3;
	cmdq_task_cond_jump(pTask, SPR1, &lop, &rop, CMDQ_EQUAL);

/* condition routine 4 */
	cmdq_task_assign_command(pTask, SPR1, 0);
	jump_tag_4 = pTask->commandSize - CMDQ_INST_SIZE;
	lop.reg = true;
	lop.idx = 0x8005;
	rop.reg = false;
	rop.value = 0x4;
	cmdq_task_cond_jump(pTask, SPR1, &lop, &rop, CMDQ_EQUAL);

/* condition routine 5 */
	cmdq_task_assign_command(pTask, SPR1, 0);
	jump_tag_5 = pTask->commandSize - CMDQ_INST_SIZE;
	lop.reg = true;
	lop.idx = 0x8005;
	rop.reg = false;
	rop.value = 0x5;
	cmdq_task_cond_jump(pTask, SPR1, &lop, &rop, CMDQ_EQUAL);

/* condition routine 6 */
	cmdq_task_assign_command(pTask, SPR1, 0);
	jump_tag_6 = pTask->commandSize - CMDQ_INST_SIZE;
	lop.reg = true;
	lop.idx = 0x8005;
	rop.reg = false;
	rop.value = 0x6;
	cmdq_task_cond_jump(pTask, SPR1, &lop, &rop, CMDQ_EQUAL);

/* condition routine 7 */
	cmdq_task_assign_command(pTask, SPR1, 0);
	jump_tag_7 = pTask->commandSize - CMDQ_INST_SIZE;
	lop.reg = true;
	lop.idx = 0x8005;
	rop.reg = false;
	rop.value = 0x7;
	cmdq_task_cond_jump(pTask, SPR1, &lop, &rop, CMDQ_EQUAL);

/* condition routine 8 */
	cmdq_task_assign_command(pTask, SPR1, 0);
	jump_tag_8 = pTask->commandSize - CMDQ_INST_SIZE;
	lop.reg = true;
	lop.idx = 0x8005;
	rop.reg = false;
	rop.value = 0x8;
	cmdq_task_cond_jump(pTask, SPR1, &lop, &rop, CMDQ_EQUAL);

/* condition routine 9 */
	cmdq_task_assign_command(pTask, SPR1, 0);
	jump_tag_9 = pTask->commandSize - CMDQ_INST_SIZE;
	lop.reg = true;
	lop.idx = 0x8005;
	rop.reg = false;
	rop.value = 0x9;
	cmdq_task_cond_jump(pTask, SPR1, &lop, &rop, CMDQ_EQUAL);

/* condition routine 10 */
	cmdq_task_assign_command(pTask, SPR1, 0);
	jump_tag_10 = pTask->commandSize - CMDQ_INST_SIZE;
	lop.reg = true;
	lop.idx = 0x8005;
	rop.reg = false;
	rop.value = 0xA;
	cmdq_task_cond_jump(pTask, SPR1, &lop, &rop, CMDQ_EQUAL);

/* condition routine 11 */
	cmdq_task_assign_command(pTask, SPR1, 0);
	jump_tag_11 = pTask->commandSize - CMDQ_INST_SIZE;
	lop.reg = true;
	lop.idx = 0x8005;
	rop.reg = false;
	rop.value = 0xB;
	cmdq_task_cond_jump(pTask, SPR1, &lop, &rop, CMDQ_EQUAL);

/* condition routine 12 */
	cmdq_task_assign_command(pTask, SPR1, 0);
	jump_tag_12 = pTask->commandSize - CMDQ_INST_SIZE;
	lop.reg = true;
	lop.idx = 0x8005;
	rop.reg = false;
	rop.value = 0xC;
	cmdq_task_cond_jump(pTask, SPR1, &lop, &rop, CMDQ_EQUAL);


	/* other codition jump is same above */
	// CPU Action, Not GCE instruction
	/* Set CPR for routine 1 */
	/* address of spr 1 = routine 1's position */

/* Set CPR for routine 1 */
	cmdq_inst = (struct cmdq_instruction *)
	cmdq_task_get_va_by_offset(pTask, jump_tag_1);
	curr_pa = cmdq_task_get_curr_pa(pTask);
	shift_pa = CMDQ_PKVM_REG_SHIFT_ADDR(curr_pa);
	cmdq_inst->arg_b = CMDQ_GET_ARG_B(shift_pa);
	cmdq_inst->arg_c = CMDQ_GET_ARG_C(shift_pa);
	/* GCE Execute routine 1 start*/
	Routine1(pTask);
	// cmdq_task_finalize_loop(pTask);
	// /* GCE Execute routine 1 END*/

/* Set CPR for routine 2 */
	cmdq_inst = (struct cmdq_instruction *)
	cmdq_task_get_va_by_offset(pTask, jump_tag_2);
	curr_pa = cmdq_task_get_curr_pa(pTask);
	shift_pa = CMDQ_PKVM_REG_SHIFT_ADDR(curr_pa);
	cmdq_inst->arg_b = CMDQ_GET_ARG_B(shift_pa);
	cmdq_inst->arg_c = CMDQ_GET_ARG_C(shift_pa);
	/* GCE Execute routine 2 start*/
	Routine2(pTask);

/* Set CPR for routine 3 */
	cmdq_inst = (struct cmdq_instruction *)
	cmdq_task_get_va_by_offset(pTask, jump_tag_3);
	curr_pa = cmdq_task_get_curr_pa(pTask);
	shift_pa = CMDQ_PKVM_REG_SHIFT_ADDR(curr_pa);
	cmdq_inst->arg_b = CMDQ_GET_ARG_B(shift_pa);
	cmdq_inst->arg_c = CMDQ_GET_ARG_C(shift_pa);
	/* GCE Execute routine 3 start*/
	Routine3(pTask);

/* Set CPR for routine 4 */
	cmdq_inst = (struct cmdq_instruction *)
	cmdq_task_get_va_by_offset(pTask, jump_tag_4);
	curr_pa = cmdq_task_get_curr_pa(pTask);
	shift_pa = CMDQ_PKVM_REG_SHIFT_ADDR(curr_pa);
	cmdq_inst->arg_b = CMDQ_GET_ARG_B(shift_pa);
	cmdq_inst->arg_c = CMDQ_GET_ARG_C(shift_pa);
	/* GCE Execute routine 4 start*/
	Routine4(pTask);

/* Set CPR for routine 5 */
	cmdq_inst = (struct cmdq_instruction *)
	cmdq_task_get_va_by_offset(pTask, jump_tag_5);
	curr_pa = cmdq_task_get_curr_pa(pTask);
	shift_pa = CMDQ_PKVM_REG_SHIFT_ADDR(curr_pa);
	cmdq_inst->arg_b = CMDQ_GET_ARG_B(shift_pa);
	cmdq_inst->arg_c = CMDQ_GET_ARG_C(shift_pa);
	/* GCE Execute routine 5 start*/
	Routine5(pTask);

/* Set CPR for routine 6 */
	cmdq_inst = (struct cmdq_instruction *)
	cmdq_task_get_va_by_offset(pTask, jump_tag_6);
	curr_pa = cmdq_task_get_curr_pa(pTask);
	shift_pa = CMDQ_PKVM_REG_SHIFT_ADDR(curr_pa);
	cmdq_inst->arg_b = CMDQ_GET_ARG_B(shift_pa);
	cmdq_inst->arg_c = CMDQ_GET_ARG_C(shift_pa);
	/* GCE Execute routine 6 start*/
	Routine6(pTask);

/* Set CPR for routine 7 */
	cmdq_inst = (struct cmdq_instruction *)
	cmdq_task_get_va_by_offset(pTask, jump_tag_7);
	curr_pa = cmdq_task_get_curr_pa(pTask);
	shift_pa = CMDQ_PKVM_REG_SHIFT_ADDR(curr_pa);
	cmdq_inst->arg_b = CMDQ_GET_ARG_B(shift_pa);
	cmdq_inst->arg_c = CMDQ_GET_ARG_C(shift_pa);
	/* GCE Execute routine 7 start*/
	Routine7(pTask);

/* Set CPR for routine 8 */
	cmdq_inst = (struct cmdq_instruction *)
	cmdq_task_get_va_by_offset(pTask, jump_tag_8);
	curr_pa = cmdq_task_get_curr_pa(pTask);
	shift_pa = CMDQ_PKVM_REG_SHIFT_ADDR(curr_pa);
	cmdq_inst->arg_b = CMDQ_GET_ARG_B(shift_pa);
	cmdq_inst->arg_c = CMDQ_GET_ARG_C(shift_pa);
	/* GCE Execute routine 8 start*/
	Routine8(pTask);

/* Set CPR for routine 9 */
	cmdq_inst = (struct cmdq_instruction *)
	cmdq_task_get_va_by_offset(pTask, jump_tag_9);
	curr_pa = cmdq_task_get_curr_pa(pTask);
	shift_pa = CMDQ_PKVM_REG_SHIFT_ADDR(curr_pa);
	cmdq_inst->arg_b = CMDQ_GET_ARG_B(shift_pa);
	cmdq_inst->arg_c = CMDQ_GET_ARG_C(shift_pa);
	/* GCE Execute routine 9 start*/
	Routine9(pTask);

/* Set CPR for routine 10 */
	cmdq_inst = (struct cmdq_instruction *)
	cmdq_task_get_va_by_offset(pTask, jump_tag_10);
	curr_pa = cmdq_task_get_curr_pa(pTask);
	shift_pa = CMDQ_PKVM_REG_SHIFT_ADDR(curr_pa);
	cmdq_inst->arg_b = CMDQ_GET_ARG_B(shift_pa);
	cmdq_inst->arg_c = CMDQ_GET_ARG_C(shift_pa);
	/* GCE Execute routine 10 start*/
	Routine10(pTask);


/* Set CPR for routine 11 */
	cmdq_inst = (struct cmdq_instruction *)
	cmdq_task_get_va_by_offset(pTask, jump_tag_11);
	curr_pa = cmdq_task_get_curr_pa(pTask);
	shift_pa = CMDQ_PKVM_REG_SHIFT_ADDR(curr_pa);
	cmdq_inst->arg_b = CMDQ_GET_ARG_B(shift_pa);
	cmdq_inst->arg_c = CMDQ_GET_ARG_C(shift_pa);
	/* GCE Execute routine 11 start*/
	Routine11(pTask);

/* Set CPR for routine 12 */
	cmdq_inst = (struct cmdq_instruction *)
	cmdq_task_get_va_by_offset(pTask, jump_tag_12);
	curr_pa = cmdq_task_get_curr_pa(pTask);
	shift_pa = CMDQ_PKVM_REG_SHIFT_ADDR(curr_pa);
	cmdq_inst->arg_b = CMDQ_GET_ARG_B(shift_pa);
	cmdq_inst->arg_c = CMDQ_GET_ARG_C(shift_pa);
	/* GCE Execute routine 12 start*/
	Routine12(pTask);

	cmdqUtilPrintHexDump("[CMDQ][pkvm]", pTask->pVABase, pTask->commandSize,
		pTask->MVABase);
}

