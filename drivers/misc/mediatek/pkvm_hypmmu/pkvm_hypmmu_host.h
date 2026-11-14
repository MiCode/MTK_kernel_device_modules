/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <asm/kvm_pkvm_module.h>

extern unsigned long mod_token;
extern struct kvm_iommu_ops kvm_nvhe_sym(hypmmu_ops);

int init_mtkiommu(void);
int init_gpumpu(void);
int init_inframpu(void);

/* mtk-iommu */
int kvm_nvhe_sym(pkvm_hypmmu_load_init)(const struct pkvm_module_ops *ops);
void kvm_nvhe_sym(mtk_iommu_hyp_init)(struct user_pt_regs *regs);
void kvm_nvhe_sym(mtk_iommu_add_device)(struct user_pt_regs *regs);
void kvm_nvhe_sym(iommu_finalise)(struct user_pt_regs *regs);

/* gpu-mpu */
void kvm_nvhe_sym(gpu_mpu_hyp_init)(struct user_pt_regs *regs);

/* infra-mpu */
void kvm_nvhe_sym(infra_mpu_hyp_init)(struct user_pt_regs *regs);
