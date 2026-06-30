/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __ISP6_PKVM_SENINF_HYP_H__
#define __ISP6_PKVM_SENINF_HYP_H__

#include <asm/kvm_pkvm_module.h>

#define PFX "[pkvm_seninf] "
#define CALL_FROM_OPS(fn, ...) pkvm_seninf_ops->fn(__VA_ARGS__)

#ifdef memset
#undef memset
#endif

#ifdef memcpy
#undef memcpy
#endif

extern const struct pkvm_module_ops *pkvm_seninf_ops;

void *memset(void *dst, int c, size_t count);

#endif // __ISP6_PKVM_SENINF_HYP_H__
