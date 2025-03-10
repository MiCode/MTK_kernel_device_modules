/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <asm/kvm_pkvm_module.h>

#define PFX "[pkvm_isp] "

void kvm_nvhe_sym(isp_hyp_checkpipe)(struct user_pt_regs *);
void kvm_nvhe_sym(isp_hyp_free)(struct user_pt_regs *);
int kvm_nvhe_sym(isp_hyp_init)(const struct pkvm_module_ops *ops);
