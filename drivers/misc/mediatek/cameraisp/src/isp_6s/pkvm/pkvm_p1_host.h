/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */
#include <asm/kvm_pkvm_module.h>
#include "pkvm_p1_ioctl.h"

#define PFX "[pkvm_p1] "

#define PORT_IDX_IMGO 6

#define DAPC_IDX_REG_CAMCTL_R1_CAMCTL_DMA_EN  5
#define DAPC_IDX_REG_CAMCTL_R1_CAMCTL_DMA2_EN 6
#define DAPC_IDX_REG_CAMCTL_R1_CAMCTL_SEL     7
#define DAPC_IDX_REG_CAMCTL_R1_LCES_OUT_SIZE  15


/*******************************************************************************
 * Constants
 ******************************************************************************/
static const uint16_t gisp_drv_reg_addr[DAPC_NUM_CQ] = {
	0x000,
	0x004,
	0x008,
	0x00C,
	0x010,
	0x014,
	0x018,
	0x040,
	0x044,
	0x930,
	0x934,
	0x938,
	0x93C,
	0xA08,
	0xC48,
	0x808,
	0x1648,
	0x1398,
	0x5040,
	0x5044,
	0x5048,
	0x504C,
	0x5050,
	0x5054,
	0x5058,
	0x505C,
	0x5060,
	0x5064,
	0x5068,
	0x506C,
	0x5070,
	0x5074,
	0x5078,
	0x507C
};

/*******************************************************************************
 * Hypervisor APIs
 ******************************************************************************/
int kvm_nvhe_sym(p1_hyp_init)(const struct pkvm_module_ops *ops);
void kvm_nvhe_sym(pkvm_p1_hyp_sec_config)(struct user_pt_regs *);
void kvm_nvhe_sym(pkvm_p1_hyp_set_sec_cam)(struct user_pt_regs *);
void kvm_nvhe_sym(pkvm_p1_hyp_set_dapc_auth)(struct user_pt_regs *);
void kvm_nvhe_sym(pkvm_p1_hyp_set_dapc_reg)(struct user_pt_regs *);
void kvm_nvhe_sym(pkvm_p1_hyp_APC_CamIspProtCtl)(struct user_pt_regs *);
void kvm_nvhe_sym(pkvm_p1_hyp_get_sec_fh_info)(struct user_pt_regs *);
void kvm_nvhe_sym(pkvm_p1_hyp_uninit)(struct user_pt_regs *);
