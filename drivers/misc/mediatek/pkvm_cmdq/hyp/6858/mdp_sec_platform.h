/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */
#ifndef __MDP_SEC_PLATFORM_H__
#define __MDP_SEC_PLATFORM_H__

#include "mtk-larb-port.h"
#include "pkvm_cmdq_hyp.h"
#include "tlDapcIndex.h"
#include "gce.h"
#include "cmdq_sec_iwc_common.h"

#define MDPSYS_BASE_ADDR 0x1F000000
#define SEC_DISABLE_ADDR 0x1F000F10
#define DISP_MUTEX3_MOD0 0x1F001090
#define MDP_MUTEX3_MOD0_EN 0x1f001080
#define MDP_RDMA0_BASE 0x1F003000
#define MDP_WROT0_BASE 0x1F00F000
#define MDP_WROT2_BASE 0x1F015000
#define MDP_HDR0_BASE 0x1F005000
#define MDP_AAL0_BASE 0x1F007000
#define MDP_RSZ0_BASE 0x1F009000
#define MDP_RSZ2_BASE 0x1F013000
#define MDP_TDSHP0_BASE 0x1F00B000

static const struct cmdq_sec_handle_reg mdp_sec_handle_regs[] = {
	/* MDP_RDMA0 */
	{.addr = (MDP_RDMA0_BASE & 0xF00), .engine = CMDQ_SEC_MDP_RDMA0},
	{.addr = (MDP_RDMA0_BASE & 0xF08), .engine = CMDQ_SEC_MDP_RDMA0},
	{.addr = (MDP_RDMA0_BASE & 0xF10), .engine = CMDQ_SEC_MDP_RDMA0},
	{.addr = (MDP_RDMA0_BASE & 0xF30), .engine = CMDQ_SEC_MDP_RDMA0},
	{.addr = (MDP_RDMA0_BASE & 0xF34), .engine = CMDQ_SEC_MDP_RDMA0},
	{.addr = (MDP_RDMA0_BASE & 0xF38), .engine = CMDQ_SEC_MDP_RDMA0},

	/* MDP_WROT0 */
	{.addr = (MDP_WROT0_BASE & 0xF00), .engine = CMDQ_SEC_MDP_WROT0},
	{.addr = (MDP_WROT0_BASE & 0xF04), .engine = CMDQ_SEC_MDP_WROT0},
	{.addr = (MDP_WROT0_BASE & 0xF08), .engine = CMDQ_SEC_MDP_WROT0},
	{.addr = (MDP_WROT0_BASE & 0xF34), .engine = CMDQ_SEC_MDP_WROT0},
	{.addr = (MDP_WROT0_BASE & 0xF38), .engine = CMDQ_SEC_MDP_WROT0},
	{.addr = (MDP_WROT0_BASE & 0xF3C), .engine = CMDQ_SEC_MDP_WROT0},

	/* MDP_WROT2 */
	{.addr = (MDP_WROT2_BASE & 0xF00), .engine = CMDQ_SEC_MDP_WROT2},
	{.addr = (MDP_WROT2_BASE & 0xF04), .engine = CMDQ_SEC_MDP_WROT2},
	{.addr = (MDP_WROT2_BASE & 0xF08), .engine = CMDQ_SEC_MDP_WROT2},
	{.addr = (MDP_WROT2_BASE & 0xF34), .engine = CMDQ_SEC_MDP_WROT2},
	{.addr = (MDP_WROT2_BASE & 0xF38), .engine = CMDQ_SEC_MDP_WROT2},
	{.addr = (MDP_WROT2_BASE & 0xF3C), .engine = CMDQ_SEC_MDP_WROT2},

	/* MDP_HDR0 */
	{.addr = (MDP_HDR0_BASE & 0xF00), .engine = CMDQ_SEC_MDP_HDR0},
	{.addr = (MDP_HDR0_BASE & 0xF04), .engine = CMDQ_SEC_MDP_HDR0},
	{.addr = (MDP_HDR0_BASE & 0xF08), .engine = CMDQ_SEC_MDP_HDR0},
	{.addr = (MDP_HDR0_BASE & 0xF34), .engine = CMDQ_SEC_MDP_HDR0},
	{.addr = (MDP_HDR0_BASE & 0xF38), .engine = CMDQ_SEC_MDP_HDR0},
	{.addr = (MDP_HDR0_BASE & 0xF3C), .engine = CMDQ_SEC_MDP_HDR0},

	/* MDP_AAL0 */
	{.addr = (MDP_AAL0_BASE & 0xF00), .engine = CMDQ_SEC_MDP_AAL0},
	{.addr = (MDP_AAL0_BASE & 0xF04), .engine = CMDQ_SEC_MDP_AAL0},
	{.addr = (MDP_AAL0_BASE & 0xF08), .engine = CMDQ_SEC_MDP_AAL0},
	{.addr = (MDP_AAL0_BASE & 0xF34), .engine = CMDQ_SEC_MDP_AAL0},
	{.addr = (MDP_AAL0_BASE & 0xF38), .engine = CMDQ_SEC_MDP_AAL0},
	{.addr = (MDP_AAL0_BASE & 0xF3C), .engine = CMDQ_SEC_MDP_AAL0},

	/* MDP_TDSHP0 */
	{.addr = (MDP_TDSHP0_BASE & 0xF00), .engine = CMDQ_SEC_MDP_TDSHP0},
	{.addr = (MDP_TDSHP0_BASE & 0xF04), .engine = CMDQ_SEC_MDP_TDSHP0},
	{.addr = (MDP_TDSHP0_BASE & 0xF08), .engine = CMDQ_SEC_MDP_TDSHP0},
	{.addr = (MDP_TDSHP0_BASE & 0xF34), .engine = CMDQ_SEC_MDP_TDSHP0},
	{.addr = (MDP_TDSHP0_BASE & 0xF38), .engine = CMDQ_SEC_MDP_TDSHP0},
	{.addr = (MDP_TDSHP0_BASE & 0xF3C), .engine = CMDQ_SEC_MDP_TDSHP0},

	/* MDP_RSZ0 */
	{.addr = (MDP_RSZ0_BASE & 0xF00), .engine = CMDQ_SEC_MDP_RSZ0},
	{.addr = (MDP_RSZ0_BASE & 0xF04), .engine = CMDQ_SEC_MDP_RSZ0},
	{.addr = (MDP_RSZ0_BASE & 0xF08), .engine = CMDQ_SEC_MDP_RSZ0},
	{.addr = (MDP_RSZ0_BASE & 0xF34), .engine = CMDQ_SEC_MDP_RSZ0},
	{.addr = (MDP_RSZ0_BASE & 0xF38), .engine = CMDQ_SEC_MDP_RSZ0},
	{.addr = (MDP_RSZ0_BASE & 0xF3C), .engine = CMDQ_SEC_MDP_RSZ0},

	/* MDP_RSZ2 */
	{.addr = (MDP_RSZ2_BASE & 0xF00), .engine = CMDQ_SEC_MDP_RSZ2},
	{.addr = (MDP_RSZ2_BASE & 0xF04), .engine = CMDQ_SEC_MDP_RSZ2},
	{.addr = (MDP_RSZ2_BASE & 0xF08), .engine = CMDQ_SEC_MDP_RSZ2},
	{.addr = (MDP_RSZ2_BASE & 0xF34), .engine = CMDQ_SEC_MDP_RSZ2},
	{.addr = (MDP_RSZ2_BASE & 0xF38), .engine = CMDQ_SEC_MDP_RSZ2},
	{.addr = (MDP_RSZ2_BASE & 0xF3C), .engine = CMDQ_SEC_MDP_RSZ2},
};

/* PORTING: Macro to get dapc index / offset / bit
 *
 * The SLAVE_TYPE_PREFIX_INFRA_SYS1 and SLAVE_TYPE_PREFIX_INFRA_SYS2 refer to
 * tlDapcIndex.h. Please read this header to know your hardware belong to SYS1
 * or SYS2
 */

#define MDP_GET_DAPC_IDX(dapc_idx)	(dapc_idx - SLAVE_TYPE_PREFIX_INFRA_SYS1)
#define MDP_GET_DAPC_OFF(dapc_idx)	(MDP_GET_DAPC_IDX(dapc_idx) / 0x10)
#define MDP_GET_DAPC_BIT(dapc_idx)	((MDP_GET_DAPC_IDX(dapc_idx) * 2) % 0x20)
#define MDP_DAPC_ASSIGN(eng_flag, dapc_idx)	{ \
	.engine_flag = 1LL << eng_flag, \
	.dapc_reg_offset = MDP_GET_DAPC_OFF(dapc_idx), \
	.bit = MDP_GET_DAPC_BIT(dapc_idx), \
	.sys = 0, \
	}
#define MDP_GET_DAPC_IDX_SYS2(dapc_idx)	(dapc_idx - SLAVE_TYPE_PREFIX_INFRA_SYS2)
#define MDP_GET_DAPC_OFF_SYS2(dapc_idx)	(MDP_GET_DAPC_IDX_SYS2(dapc_idx) / 0x10)
#define MDP_GET_DAPC_BIT_SYS2(dapc_idx)	((MDP_GET_DAPC_IDX_SYS2(dapc_idx) * 2) % 0x20)
#define MDP_DAPC_ASSIGN_SYS2(eng_flag, dapc_idx)	{ \
	.engine_flag = 1LL << eng_flag, \
	.dapc_reg_offset = MDP_GET_DAPC_OFF_SYS2(dapc_idx), \
	.bit = MDP_GET_DAPC_BIT_SYS2(dapc_idx), \
	.sys = 1, \
	}


/* PORTING: Define engines write to DRAM
 *
 * Add engines in mdp_engines list.
 * These engines will protect by devapc
 * More detail refer cmdq_protect_engine in dapc_platform.h
 *
 * Please read tlDapcIndex.h to know DAPC index.
 */
static const struct cmdq_protect_engine mdp_dapc_engines[] = {
	MDP_DAPC_ASSIGN(CMDQ_SEC_ISP_IMGI, DAPC_INDEX_IMG1_APB_S_1),
	MDP_DAPC_ASSIGN(CMDQ_SEC_ISP_IMGI, DAPC_INDEX_IMG1_APB_S_2),
	MDP_DAPC_ASSIGN(CMDQ_SEC_ISP_IMGI, DAPC_INDEX_IMG1_APB_S_3),
	MDP_DAPC_ASSIGN(CMDQ_SEC_ISP_IMGI, DAPC_INDEX_IMG1_APB_S_4),
	MDP_DAPC_ASSIGN(CMDQ_SEC_ISP_IMGI, DAPC_INDEX_IMG1_APB_S_5),
	MDP_DAPC_ASSIGN(CMDQ_SEC_ISP_IMGI, DAPC_INDEX_IMG1_APB_S_6),
	MDP_DAPC_ASSIGN(CMDQ_SEC_ISP_IMGI, DAPC_INDEX_IMG1_APB_S_7),
	MDP_DAPC_ASSIGN(CMDQ_SEC_ISP_IMGI, DAPC_INDEX_IMG1_APB_S_8),
	MDP_DAPC_ASSIGN(CMDQ_SEC_ISP_IMGI, DAPC_INDEX_IMG1_APB_S_11),
	MDP_DAPC_ASSIGN(CMDQ_SEC_ISP_IMGI, DAPC_INDEX_IMG1_APB_S_12),

	MDP_DAPC_ASSIGN(CMDQ_SEC_ISP_IMGI, DAPC_INDEX_IMG1_APB_S_0),
	MDP_DAPC_ASSIGN(CMDQ_SEC_ISP_IMG2O, DAPC_INDEX_IMG1_APB_S_1),
	MDP_DAPC_ASSIGN(CMDQ_SEC_ISP_IMG3O, DAPC_INDEX_IMG1_APB_S_1),
	MDP_DAPC_ASSIGN_SYS2(CMDQ_SEC_MDP_RDMA0, DAPC_INDEX_MDP_APB_S_0),
	MDP_DAPC_ASSIGN_SYS2(CMDQ_SEC_MDP_RDMA0, DAPC_INDEX_MDP_APB_S_3),
	MDP_DAPC_ASSIGN_SYS2(CMDQ_SEC_MDP_HDR0, DAPC_INDEX_MDP_APB_S_5),
	MDP_DAPC_ASSIGN_SYS2(CMDQ_SEC_MDP_AAL0, DAPC_INDEX_MDP_APB_S_7),
	MDP_DAPC_ASSIGN_SYS2(CMDQ_SEC_MDP_RSZ0, DAPC_INDEX_MDP_APB_S_9),
	MDP_DAPC_ASSIGN_SYS2(CMDQ_SEC_MDP_TDSHP0, DAPC_INDEX_MDP_APB_S_11),
	MDP_DAPC_ASSIGN_SYS2(CMDQ_SEC_MDP_WROT0, DAPC_INDEX_MDP_APB_S_15),
	MDP_DAPC_ASSIGN_SYS2(CMDQ_SEC_MDP_RSZ2, DAPC_INDEX_MDP_APB_S_19),
	MDP_DAPC_ASSIGN_SYS2(CMDQ_SEC_MDP_WROT2, DAPC_INDEX_MDP_APB_S_21),
};

/* PORTING: Define engines need access secure DRAM
 *
 * Add engines in mdp_secure_port.
 * More detail refer engine_secure_port in cmdq_platform_def.h
 */
static const struct engine_secure_port mdp_secure_port[] = {
	{.engine_flag = 1LL << CMDQ_SEC_MDP_RDMA0, .port = M4U_L2_P0_MDP_RDMA0},
	{.engine_flag = 1LL << CMDQ_SEC_MDP_WROT0, .port = M4U_L2_P2_MDP_WROT0_WROT},
	{.engine_flag = 1LL << CMDQ_SEC_MDP_WROT2, .port = M4U_L2_P3_MDP_WROT2_WDMA},
	{.engine_flag = 1LL << CMDQ_SEC_FDVT, .port = M4U_LARB20_PORT0},
	{.engine_flag = 1LL << CMDQ_SEC_FDVT, .port = M4U_LARB20_PORT1},
	{.engine_flag = 1LL << CMDQ_SEC_FDVT, .port = M4U_LARB20_PORT2},
	{.engine_flag = 1LL << CMDQ_SEC_FDVT, .port = M4U_LARB20_PORT3},
};

/**
 * Input 1: ['tRDMA0', 'tWROT0']
 * Path                Type               Register            Value
 * ----------------------------------------------------------
 * tRDMA0 -> tWROT0    MOUT(bit mask)   MDP_BYP0_MOUT_EN    0x1
 * tRDMA0 -> tWROT0    SEL_IN(mux idx)  MDP_BYP0_SEL_IN     0x0
 *
 *
 * Input 2: ['tRDMA0', 'tDLI0_SEL', 'tHDR0', 'tAAL0', 'tSCL0', 'tTDSHP0', 'tDLO0_SOUT', 'tWROT0']
 * Path                     Type                Register             Value
 * -----------------------------------------------------------------
 * tRDMA0 -> tDLI0_SEL      MOUT(bit mask)    MDP_BYP0_MOUT_EN     0x4
 * tRDMA0 -> tDLI0_SEL      SEL_IN(mux idx)   MDP_DLI0_SEL_IN      0x0
 * tDLI0_SEL -> tHDR0       MOUT(bit mask)    MDP_RDMA0_MOUT_EN    0x1
 * tDLI0_SEL -> tHDR0       SEL_IN(mux idx)   MDP_PQ0_SEL_IN       0x0
 * tAAL0 -> tSCL0           MOUT(bit mask)    MDP_AAL0_MOUT_EN     0x1
 * tTDSHP0 -> tDLO0_SOUT    SEL_IN(mux idx)   MDP_WROT0_SEL_IN     0x1
 * tTDSHP0 -> tDLO0_SOUT    SEL_OUT(mux idx)  MDP_PQ0_SOUT_SEL     0x0
 * tDLO0_SOUT -> tWROT0     SEL_IN(mux idx)   MDP_BYP0_SEL_IN      0x1
 * tDLO0_SOUT -> tWROT0     SEL_OUT(mux idx)  MDP_DLO0_SOUT_SEL    0x0
 *
 *
 * Input 3_1: ['tRDMA0', 'tDLI0_SEL', 'tHDR0', 'tAAL0', 'tSCL0', 'tTDSHP0', 'tDLO0_SOUT', 'tWROT0']
 * Path                     Type                Register             Value
 * -----------------------------------------------------------------
 * tRDMA0 -> tDLI0_SEL      MOUT(bit mask)    MDP_BYP0_MOUT_EN     0x4
 * tRDMA0 -> tDLI0_SEL      SEL_IN(mux idx)   MDP_DLI0_SEL_IN      0x0
 * tDLI0_SEL -> tHDR0       MOUT(bit mask)    MDP_RDMA0_MOUT_EN    0x1
 * tDLI0_SEL -> tHDR0       SEL_IN(mux idx)   MDP_PQ0_SEL_IN       0x0
 * tAAL0 -> tSCL0           MOUT(bit mask)    MDP_AAL0_MOUT_EN     0x1
 * tTDSHP0 -> tDLO0_SOUT    SEL_IN(mux idx)   MDP_WROT0_SEL_IN     0x1
 * tTDSHP0 -> tDLO0_SOUT    SEL_OUT(mux idx)  MDP_PQ0_SOUT_SEL     0x0
 * tDLO0_SOUT -> tWROT0     SEL_IN(mux idx)   MDP_BYP0_SEL_IN      0x1
 * tDLO0_SOUT -> tWROT0     SEL_OUT(mux idx)  MDP_DLO0_SOUT_SEL    0x0
 *
 *
 * Input 3_2: ['tRDMA0', 'tSCL2']
 * Path               Type               Register            Value
 * ---------------------------------------------------------
 * tRDMA0 -> tSCL2    MOUT(bit mask)   MDP_BYP0_MOUT_EN    0x2
 * tRDMA0 -> tSCL2    SEL_IN(mux idx)  MDP_RSZ2_SEL_IN     0x1
 *
 * Input 4: ['tCAMIN', 'tSCL2']
 * Path               Type               Register           Value
 * --------------------------------------------------------
 * tCAMIN -> tSCL2    MOUT(bit mask)   ISP0_MOUT_EN       0x2
 * tCAMIN -> tSCL2    SEL_IN(mux idx)  MDP_RSZ2_SEL_IN    0x2
 *
 *
 * Input 5: ['tCAMIN', 'tDLI0_SEL', 'tHDR0', 'tAAL0', 'tSCL2']
 * Path                   Type               Register             Value
 * --------------------------------------------------------------
 * tCAMIN -> tDLI0_SEL    MOUT(bit mask)   ISP0_MOUT_EN         0x1
 * tCAMIN -> tDLI0_SEL    SEL_IN(mux idx)  MDP_DLI0_SEL_IN      0x2
 * tDLI0_SEL -> tHDR0     MOUT(bit mask)   MDP_RDMA0_MOUT_EN    0x1
 * tDLI0_SEL -> tHDR0     SEL_IN(mux idx)  MDP_PQ0_SEL_IN       0x0
 * tAAL0 -> tSCL2         MOUT(bit mask)   MDP_AAL0_MOUT_EN     0x2
 * tAAL0 -> tSCL2         SEL_IN(mux idx)  MDP_RSZ2_SEL_IN      0x4
 *
 *
 * Input 6: ['tCAMIN', 'tDLI0_SEL', 'tHDR0', 'tAAL0', 'tSCL0', 'tTDSHP0', 'tDLO0_SOUT', 'tWROT0']
 * Path                     Type                Register             Value
 * -----------------------------------------------------------------
 * tCAMIN -> tDLI0_SEL      MOUT(bit mask)    ISP0_MOUT_EN         0x1
 * tCAMIN -> tDLI0_SEL      SEL_IN(mux idx)   MDP_DLI0_SEL_IN      0x2
 * tDLI0_SEL -> tHDR0       MOUT(bit mask)    MDP_RDMA0_MOUT_EN    0x1
 * tDLI0_SEL -> tHDR0       SEL_IN(mux idx)   MDP_PQ0_SEL_IN       0x0
 * tAAL0 -> tSCL0           MOUT(bit mask)    MDP_AAL0_MOUT_EN     0x1
 * tTDSHP0 -> tDLO0_SOUT    SEL_IN(mux idx)   MDP_WROT0_SEL_IN     0x1
 * tTDSHP0 -> tDLO0_SOUT    SEL_OUT(mux idx)  MDP_PQ0_SOUT_SEL     0x0
 * tDLO0_SOUT -> tWROT0     SEL_IN(mux idx)   MDP_BYP0_SEL_IN      0x1
 * tDLO0_SOUT -> tWROT0     SEL_OUT(mux idx)  MDP_DLO0_SOUT_SEL    0x0
 *
 *
 * Input 7_1: ['tCAMIN', 'tDLI0_SEL', 'tHDR0', 'tAAL0', 'tSCL0', 'tTDSHP0', 'tDLO0_SOUT', 'tWROT0']
 * Path                     Type                Register             Value
 * -----------------------------------------------------------------
 * tCAMIN -> tDLI0_SEL      MOUT(bit mask)    ISP0_MOUT_EN         0x1
 * tCAMIN -> tDLI0_SEL      SEL_IN(mux idx)   MDP_DLI0_SEL_IN      0x2
 * tDLI0_SEL -> tHDR0       MOUT(bit mask)    MDP_RDMA0_MOUT_EN    0x1
 * tDLI0_SEL -> tHDR0       SEL_IN(mux idx)   MDP_PQ0_SEL_IN       0x0
 * tAAL0 -> tSCL0           MOUT(bit mask)    MDP_AAL0_MOUT_EN     0x1
 * tTDSHP0 -> tDLO0_SOUT    SEL_IN(mux idx)   MDP_WROT0_SEL_IN     0x1
 * tTDSHP0 -> tDLO0_SOUT    SEL_OUT(mux idx)  MDP_PQ0_SOUT_SEL     0x0
 * tDLO0_SOUT -> tWROT0     SEL_IN(mux idx)   MDP_BYP0_SEL_IN      0x1
 * tDLO0_SOUT -> tWROT0     SEL_OUT(mux idx)  MDP_DLO0_SOUT_SEL    0x0
 *
 *
 * Input 7_2: ['tCAMIN', 'tSCL2']
 * Path               Type               Register           Value
 * --------------------------------------------------------
 * tCAMIN -> tSCL2    MOUT(bit mask)   ISP0_MOUT_EN       0x2
 * tCAMIN -> tSCL2    SEL_IN(mux idx)  MDP_RSZ2_SEL_IN    0x2
 *
 * Input 8: ['tCAMIN2', 'tSCL2']
 * Path                Type               Register           Value
 * ---------------------------------------------------------
 * tCAMIN2 -> tSCL2    MOUT(bit mask)   ISP1_MOUT_EN       0x8
 * tCAMIN2 -> tSCL2    SEL_IN(mux idx)  MDP_RSZ2_SEL_IN    0x3
 *
 *
 * Input 9: ['tCAMIN2', 'tDLI0_SEL', 'tHDR0', 'tAAL0', 'tSCL2']
 * Path                    Type               Register             Value
 * ---------------------------------------------------------------
 * tCAMIN2 -> tDLI0_SEL    MOUT(bit mask)   ISP1_MOUT_EN         0x4
 * tCAMIN2 -> tDLI0_SEL    SEL_IN(mux idx)  MDP_DLI0_SEL_IN      0x3
 * tDLI0_SEL -> tHDR0      MOUT(bit mask)   MDP_RDMA0_MOUT_EN    0x1
 * tDLI0_SEL -> tHDR0      SEL_IN(mux idx)  MDP_PQ0_SEL_IN       0x0
 * tAAL0 -> tSCL2          MOUT(bit mask)   MDP_AAL0_MOUT_EN     0x2
 * tAAL0 -> tSCL2          SEL_IN(mux idx)  MDP_RSZ2_SEL_IN      0x4
 *
 *
 * Input 10: ['tCAMIN2', 'tDLI0_SEL', 'tHDR0', 'tAAL0', 'tSCL0', 'tTDSHP0', 'tDLO0_SOUT', 'tWROT0']
 * Path                     Type                Register             Value
 * -----------------------------------------------------------------
 * tCAMIN2 -> tDLI0_SEL     MOUT(bit mask)    ISP1_MOUT_EN         0x4
 * tCAMIN2 -> tDLI0_SEL     SEL_IN(mux idx)   MDP_DLI0_SEL_IN      0x3
 * tDLI0_SEL -> tHDR0       MOUT(bit mask)    MDP_RDMA0_MOUT_EN    0x1
 * tDLI0_SEL -> tHDR0       SEL_IN(mux idx)   MDP_PQ0_SEL_IN       0x0
 * tAAL0 -> tSCL0           MOUT(bit mask)    MDP_AAL0_MOUT_EN     0x1
 * tTDSHP0 -> tDLO0_SOUT    SEL_IN(mux idx)   MDP_WROT0_SEL_IN     0x1
 * tTDSHP0 -> tDLO0_SOUT    SEL_OUT(mux idx)  MDP_PQ0_SOUT_SEL     0x0
 * tDLO0_SOUT -> tWROT0     SEL_IN(mux idx)   MDP_BYP0_SEL_IN      0x1
 * tDLO0_SOUT -> tWROT0     SEL_OUT(mux idx)  MDP_DLO0_SOUT_SEL    0x0
 *
 *
 * Input 11_1: ['tCAMIN2', 'tDLI0_SEL', 'tHDR0', 'tAAL0', 'tSCL0', 'tTDSHP0', 'tDLO0_SOUT', 'tWROT0']
 * Path                     Type                Register             Value
 * -----------------------------------------------------------------
 * tCAMIN2 -> tDLI0_SEL     MOUT(bit mask)    ISP1_MOUT_EN         0x4
 * tCAMIN2 -> tDLI0_SEL     SEL_IN(mux idx)   MDP_DLI0_SEL_IN      0x3
 * tDLI0_SEL -> tHDR0       MOUT(bit mask)    MDP_RDMA0_MOUT_EN    0x1
 * tDLI0_SEL -> tHDR0       SEL_IN(mux idx)   MDP_PQ0_SEL_IN       0x0
 * tAAL0 -> tSCL0           MOUT(bit mask)    MDP_AAL0_MOUT_EN     0x1
 * tTDSHP0 -> tDLO0_SOUT    SEL_IN(mux idx)   MDP_WROT0_SEL_IN     0x1
 * tTDSHP0 -> tDLO0_SOUT    SEL_OUT(mux idx)  MDP_PQ0_SOUT_SEL     0x0
 * tDLO0_SOUT -> tWROT0     SEL_IN(mux idx)   MDP_BYP0_SEL_IN      0x1
 * tDLO0_SOUT -> tWROT0     SEL_OUT(mux idx)  MDP_DLO0_SOUT_SEL    0x0
 *
 *
 * Input 11_2: ['tCAMIN2', 'tSCL2']
 * Path                Type               Register           Value
 * ---------------------------------------------------------
 * tCAMIN2 -> tSCL2    MOUT(bit mask)   ISP1_MOUT_EN       0x8
 * tCAMIN2 -> tSCL2    SEL_IN(mux idx)  MDP_RSZ2_SEL_IN    0x3
 *
 *
 * Input 12: ['tCAMIN', 'tDLI0_SEL', 'tDLO0_SOUT']
 * Path                       Type               Register             Value
 * ------------------------------------------------------------------
 * tCAMIN -> tDLI0_SEL        MOUT(bit mask)   ISP0_MOUT_EN         0x1
 * tCAMIN -> tDLI0_SEL        SEL_IN(mux idx)  MDP_DLI0_SEL_IN      0x2
 * tDLI0_SEL -> tDLO0_SOUT    MOUT(bit mask)   MDP_RDMA0_MOUT_EN    0x2
 * tDLI0_SEL -> tDLO0_SOUT    SEL_IN(mux idx)  MDP_WROT0_SEL_IN     0x0
 */

#endif	/*  __MDP_SEC_PLATFORM_H__ */
