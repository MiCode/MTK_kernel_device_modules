/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */
#ifndef __MDP_SEC_PLATFORM_H__
#define __MDP_SEC_PLATFORM_H__

#include "mtk-larb-port.h"
#include "pkvm_cmdq_hyp.h"
#include "tlDapcIndex.h"
#include "cmdq_sec_iwc_common.h"

#define MDP_RDMA0_BASE 0x1F003000
#define MDP_WROT0_BASE 0x1F00F000
#define MDP_WROT2_BASE 0x1F015000

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

	MDP_DAPC_ASSIGN_SYS2(CMDQ_SEC_MDP_RDMA0, DAPC_INDEX_MDP_APB_S_3),
	MDP_DAPC_ASSIGN_SYS2(CMDQ_SEC_MDP_WROT0, DAPC_INDEX_MDP_APB_S_15),
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

static const struct engine_secure_port isp_secure_boundary[] = {
};

static const uint64_t mdp_secure_input = (1LL << CMDQ_SEC_MDP_RDMA0);

static const uint64_t isp_engines = GENMASK(CMDQ_SEC_ISP_UFBCO, CMDQ_SEC_ISP_IMGI);

static const uint64_t isp_standalone_engines =
	(1LL << CMDQ_SEC_ISP_IMGI) |
	(1LL << CMDQ_SEC_ISP_SMXIO) |
	(1LL << CMDQ_SEC_ISP_DMGI_DEPI);

/* PORTING: Define registers which may store secure handle
 *
 * Add register address wihch may store handle.
 * CMDQ may block and AEE if secure handle set to register out of
 * this list.
 *
 * For mt6855:
 *	MDP_RDMA0 LSB offset 0xF00 / 0xF04 / 0xF08
 *	MDP_RDMA0 MSB offset 0xF30 / 0xF34 / 0xF38
 *	MDP_WROT0 LSB offset 0xF00 / 0xF04 / 0xF08
 *	MDP_WROT0 MSB offset 0xF34 / 0xF38 / 0xF3C
 */
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
};



#endif	/*  __MDP_SEC_PLATFORM_H__ */
