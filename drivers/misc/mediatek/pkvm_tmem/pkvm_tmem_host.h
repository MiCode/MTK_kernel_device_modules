/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __PKVM_TMEM_HOST_H__
#define __PKVM_TMEM_HOST_H__

int __kvm_nvhe_hyp_tmem_init(const struct pkvm_module_ops *ops);
void __kvm_nvhe_hyp_region_protect(struct kvm_cpu_context *ctx);
void __kvm_nvhe_hyp_region_unprotect(struct kvm_cpu_context *ctx);
void __kvm_nvhe_hyp_page_protect(struct kvm_cpu_context *ctx);
void __kvm_nvhe_hyp_page_unprotect(struct kvm_cpu_context *ctx);

#endif
