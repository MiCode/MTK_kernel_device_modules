/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

int kvm_nvhe_sym(pkvm_mgmt_hyp_init)(const struct pkvm_module_ops *ops);
int kvm_nvhe_sym(hyp_pmm_hal_register)(void *hal);

void kvm_nvhe_sym(hyp_pmm_assign_buffer_v2)(struct user_pt_regs *);
void kvm_nvhe_sym(hyp_pmm_unassign_buffer_v2)(struct user_pt_regs *);
void kvm_nvhe_sym(hyp_pmm_defragment)(struct user_pt_regs *);
void kvm_nvhe_sym(hyp_pmm_secure_range)(u64 pa, u64 size, u8 attr);
void kvm_nvhe_sym(hyp_pmm_unsecure_range)(u64 pa, u64 size, u8 attr);

typedef int TZ_RESULT;
TZ_RESULT kvm_nvhe_sym(SECIO_WRITE)(uint32_t io_type, uint32_t reg_offset, uint32_t write_val);
TZ_RESULT kvm_nvhe_sym(SECIO_READ)(uint32_t io_type, uint32_t reg_offset, uint32_t *read_val);
