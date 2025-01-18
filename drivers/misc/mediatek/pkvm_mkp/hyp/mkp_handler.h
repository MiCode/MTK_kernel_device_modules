/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __MKP_HANDLER_H
#define __MKP_HANDLER_H

#include <asm/kvm_pkvm_module.h>
#include "handle.h"

#define MKP_EXCEPTION_NO_ERROR	0
#define MKP_EXCEPTION_NORMAL	1

u32 mkp_sync_handler(struct kvm_cpu_context *ctx);
int mkp_perm_fault_handler(struct kvm_cpu_context *host_ctxt, u64 esr, u64 addr);
void mkp_illegal_abt_notifier(struct kvm_cpu_context *ctx);

#endif
