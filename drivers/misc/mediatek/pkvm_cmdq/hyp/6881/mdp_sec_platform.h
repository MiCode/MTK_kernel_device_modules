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
	MDP_DAPC_ASSIGN_SYS2(CMDQ_SEC_ISP_IMGI, DAPC_INDEX_IMG1_APB_S_7),
	MDP_DAPC_ASSIGN_SYS2(CMDQ_SEC_ISP_IMGI, DAPC_INDEX_IMG1_APB_S_49),
	MDP_DAPC_ASSIGN_SYS2(CMDQ_SEC_ISP_IMGI, DAPC_INDEX_IMG1_APB_S_50),
	MDP_DAPC_ASSIGN_SYS2(CMDQ_SEC_ISP_IMGI, DAPC_INDEX_IMG1_APB_S_51),
	MDP_DAPC_ASSIGN_SYS2(CMDQ_SEC_ISP_IMGI, DAPC_INDEX_IMG1_APB_S_75),
	MDP_DAPC_ASSIGN_SYS2(CMDQ_SEC_ISP_IMGI, DAPC_INDEX_IMG1_APB_S_76),
	MDP_DAPC_ASSIGN_SYS2(CMDQ_SEC_ISP_IMGI, DAPC_INDEX_IMG1_APB_S_90),
	MDP_DAPC_ASSIGN_SYS2(CMDQ_SEC_ISP_IMGI, DAPC_INDEX_IMG1_APB_S_100),
	MDP_DAPC_ASSIGN_SYS2(CMDQ_SEC_ISP_IMGI, DAPC_INDEX_IMG1_APB_S_101),
	MDP_DAPC_ASSIGN_SYS2(CMDQ_SEC_ISP_IMGI, DAPC_INDEX_IMG1_APB_S_102),
	MDP_DAPC_ASSIGN_SYS2(CMDQ_SEC_ISP_IMGI, DAPC_INDEX_IMG1_APB_S_145),
};

/* PORTING: Define engines need access secure DRAM
 *
 * Add engines in mdp_secure_port.
 * More detail refer engine_secure_port in cmdq_platform_def.h
 */
static const struct engine_secure_port mdp_secure_port[] = {

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
};

#endif	/*  __MDP_SEC_PLATFORM_H__ */
