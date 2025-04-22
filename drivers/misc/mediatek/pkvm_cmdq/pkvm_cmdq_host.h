/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <asm/kvm_pkvm_module.h>

void kvm_nvhe_sym(cmdq_hyp_submit_task)(struct user_pt_regs *);
void kvm_nvhe_sym(cmdq_hyp_res_release)(struct user_pt_regs *);
void kvm_nvhe_sym(cmdq_hyp_cancel_task)(struct user_pt_regs *);
void kvm_nvhe_sym(cmdq_hyp_path_res_allocate)(struct user_pt_regs *);
void kvm_nvhe_sym(cmdq_hyp_path_res_release)(struct user_pt_regs *);
void kvm_nvhe_sym(cmdq_hyp_pkvm_init)(struct user_pt_regs *);
void kvm_nvhe_sym(cmdq_hyp_pkvm_disable)(struct user_pt_regs *);
void kvm_nvhe_sym(cmdq_hyp_get_memory)(struct user_pt_regs *);
void kvm_nvhe_sym(cmdq_hyp_cam_preview_support)(struct user_pt_regs *);

int kvm_nvhe_sym(cmdq_hyp_init)(const struct pkvm_module_ops *ops);
