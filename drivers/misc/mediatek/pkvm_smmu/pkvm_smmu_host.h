/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
void kvm_nvhe_sym(add_smmu_device)(struct kvm_cpu_context *ctx);
void kvm_nvhe_sym(mtk_smmu_share)(struct kvm_cpu_context *ctx);
void kvm_nvhe_sym(pctrl_setup)(struct kvm_cpu_context *ctx);
void kvm_nvhe_sym(mtk_smmu_host_debug)(struct kvm_cpu_context *ctx);
void kvm_nvhe_sym(setup_vm)(struct kvm_cpu_context *ctx);
void kvm_nvhe_sym(mtk_iommu_init)(struct kvm_cpu_context *ctx);
void kvm_nvhe_sym(smmu_finalise)(struct kvm_cpu_context *ctx);
/* For page base mapping */
void kvm_nvhe_sym(mtk_smmu_secure_v2)(struct kvm_cpu_context *ctx);
void kvm_nvhe_sym(mtk_smmu_unsecure_v2)(struct kvm_cpu_context *ctx);
/* For region base mapping */
void kvm_nvhe_sym(mtk_smmu_secure)(struct kvm_cpu_context *ctx);
void kvm_nvhe_sym(mtk_smmu_unsecure)(struct kvm_cpu_context *ctx);
extern struct kvm_iommu_ops kvm_nvhe_sym(smmu_ops);
