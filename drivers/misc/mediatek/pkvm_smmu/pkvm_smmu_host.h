/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

/* Common macro definitions */
#define F_BIT_SET(bit)      (1<<(bit))

/* PCTRL setting */
#define PCTRL_CON0          (0x0)
#define PCTRL_CON2          (0x8)
#define ACP_SWITCH_LOCK_EN  (F_BIT_SET(4))
#define SSYS3_SWITCH_MASK   (F_BIT_SET(11))

void kvm_nvhe_sym(add_smmu_device)(struct user_pt_regs *);
void kvm_nvhe_sym(mtk_smmu_share)(struct user_pt_regs *);
void kvm_nvhe_sym(pctrl_setup)(struct user_pt_regs *);
void kvm_nvhe_sym(mtk_smmu_host_debug)(struct user_pt_regs *);
void kvm_nvhe_sym(setup_vm)(struct user_pt_regs *);
void kvm_nvhe_sym(mtk_iommu_init)(struct user_pt_regs *);
void kvm_nvhe_sym(smmu_finalise)(struct user_pt_regs *);
/* For page base mapping */
void kvm_nvhe_sym(mtk_smmu_secure_v2)(struct user_pt_regs *);
void kvm_nvhe_sym(mtk_smmu_unsecure_v2)(struct user_pt_regs *);
/* For region base mapping */
void kvm_nvhe_sym(mtk_smmu_secure)(struct user_pt_regs *);
void kvm_nvhe_sym(mtk_smmu_unsecure)(struct user_pt_regs *);
/* For smmu s2 page table merge */
void kvm_nvhe_sym(smmu_merge_s2_table)(struct user_pt_regs *);
/* Map DVM MI register in to hypervisor */
void kvm_nvhe_sym(dvm_mi_reg_mapping)(struct user_pt_regs *);
extern struct kvm_iommu_ops kvm_nvhe_sym(smmu_ops);
