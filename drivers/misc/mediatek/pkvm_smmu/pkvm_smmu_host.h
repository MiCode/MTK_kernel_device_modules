/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
void __kvm_nvhe_add_smmu_device(struct kvm_cpu_context *ctx);
void __kvm_nvhe_mtk_smmu_secure_v2(struct kvm_cpu_context *ctx);
void __kvm_nvhe_mtk_smmu_unsecure_v2(struct kvm_cpu_context *ctx);
void __kvm_nvhe_mtk_smmu_share(struct kvm_cpu_context *ctx);
void __kvm_nvhe_pctrl_setup(struct kvm_cpu_context *ctx);
void __kvm_nvhe_mtk_smmu_host_debug(struct kvm_cpu_context *ctx);
void __kvm_nvhe_setup_vm(struct kvm_cpu_context *ctx);
void __kvm_nvhe_mtk_iommu_init(struct kvm_cpu_context *ctx);
extern struct kvm_iommu_ops kvm_nvhe_sym(smmu_ops);
