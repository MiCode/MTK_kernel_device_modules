/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <asm/kvm_pkvm_module.h>

/* TZMP sw token */
#define CMDQ_SYNC_TOKEN_TZMP_ISC_WAIT			888
#define CMDQ_SYNC_TOKEN_TZMP_ISC_SET			889
#define CMDQ_SYNC_TOKEN_TZMP_ISP_WAIT			655
#define CMDQ_SYNC_TOKEN_TZMP_ISP_SET			656
#define CMDQ_SYNC_TOKEN_TZMP_AIE_WAIT			657
#define CMDQ_SYNC_TOKEN_TZMP_AIE_SET			658
#define CMDQ_SYNC_TOKEN_TZMP_ADL_WAIT			659
#define CMDQ_SYNC_TOKEN_TZMP_ADL_SET			660

#define CMDQ_SECIO_TYPE_RANGE_0     0x0
#define CMDQ_SECIO_TYPE_RANGE_1     0x1000
#define CMDQ_SECIO_TYPE_RANGE_7     0x70000
#define CMDQ_SECIO_TYPE_RANGE_8     0x80000

#define GCE_BASE_VA                   cmdq_tz_get_gce_base_va()

#define CMDQ_SECIO_GET_OFFSET(addr) (addr & 0xFFF)
#define CMDQ_SECIO_TYPE_GET_OFFSET(addr) (addr - GCE_BASE_VA)
#define CMDQ_THR_SECURITY(id)        (GCE_BASE_VA + (0x080 * id) + 0x70118)

#define CMDQ_PKVM_REG_SHIFT_ADDR(addr)	(((uint64_t)(addr) >> 3) + BIT(28))
#define CMDQ_PKVM_REG_REVERT_ADDR(addr)	(((uint64_t)(addr) << 3) - BIT(31))

/* Register definition for CMDQ usage */
#define DEVAPC_MMINFRA_AO_SYS0_BASE	0x30050000
#define DEVAPC_MMINFRA_AO_SYS1_BASE	0x30051000
#define DAPC_BASE		DEVAPC_MMINFRA_AO_SYS0_BASE
#define DAPC_BASE2		DEVAPC_MMINFRA_AO_SYS1_BASE

/* Slave Type */
#define SLAVE_TYPE_PREFIX_INFRA_SYS1	(0x0)
#define SLAVE_TYPE_PREFIX_INFRA_SYS2	(0x255)

#define DAPC_REG_PA(sys, idx)	(dapc_base_pa[sys] + idx * 0x4)

#define DAPC_SYS_CNT		2

#define CMDQ_DAPC_SYS1_CNT	256
#define CMDQ_DAPC_SYS2_CNT	256
/* this count contains DAPC SYS1 + SYS2 */
#define CMDQ_MAX_DAPC_COUNT	(CMDQ_DAPC_SYS1_CNT + CMDQ_DAPC_SYS2_CNT)

#define CMDQ_SPECIAL_SUBSYS_ADDR (99)

/* MM slaves' config index */
#define DAPC_IMG_APB_S			(144)	//0x15000000
#define DAPC_IMG_APB_S_15		(159)	//0x15040000
#define DAPC_IMG_APB_S_21		(165)	//0x15100000
#define DAPC_IMG_APB_S_27		(171)	//0x15160000
#define DAPC_IMG_APB_S_32		(176)	//0x15200000
#define DAPC_IMG_APB_S_33		(177)	//0x15210000
#define DAPC_IMG_APB_S_38		(182)	//0x15310000
#define DAPC_IMG_APB_S_39		(183)	//0x15320000
#define DAPC_IMG_APB_S_40		(184)	//0x15330000
#define DAPC_IMG_APB_S_45		(189)	//0x15500000
#define DAPC_IMG_APB_S_46		(190)	//0x15510000
#define DAPC_IMG_APB_S_50		(194)	//0x15600000
#define DAPC_IMG_APB_S_56		(200)	//0x15700000

#define MDP_GET_DAPC_IDX(dapc_idx)	(dapc_idx - SLAVE_TYPE_PREFIX_INFRA_SYS1)
#define MDP_GET_DAPC_OFF(dapc_idx)	(MDP_GET_DAPC_IDX(dapc_idx) / 0x10)
#define MDP_GET_DAPC_BIT(dapc_idx)	((MDP_GET_DAPC_IDX(dapc_idx) * 2) % 0x20)
#define MDP_DAPC_ASSIGN(eng_flag, dapc_idx)	{ \
	.engine_flag = 1LL << eng_flag, \
	.dapc_reg_offset = MDP_GET_DAPC_OFF(dapc_idx), \
	.bit = MDP_GET_DAPC_BIT(dapc_idx), \
	.sys = 0, \
}

void cmdq_secio_write(const uint32_t addr, const uint32_t val);
uint32_t cmdq_secio_read(const uint32_t addr);
uint32_t cmdq_tz_get_gce_base_va(void);
void cmdq_tz_setup(uint8_t hwid);
void cmdq_drv_imgsys_slc_cb(void);
int32_t cmdq_drv_imgsys_set_domain(void *data, bool isSet);
