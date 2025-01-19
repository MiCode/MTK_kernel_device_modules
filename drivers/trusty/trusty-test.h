/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020 Google, Inc.
 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef _TRUSTY_TEST_H
#define _TRUSTY_TEST_H

#define SMC_SC_TEST_VERSION SMC_STDCALL_NR(SMC_ENTITY_TEST, 0)
#define SMC_SC_TEST_SHARED_MEM_RW SMC_STDCALL_NR(SMC_ENTITY_TEST, 1)

/**
 * SMC_SC_TEST_CLOBBER_FPSIMD_CLOBBER - Test save and clobber of FP/SIMD
 * registers during an NS <-> TF-A <-> Trusty roundtrip.
 *
 * Return: 0 on success, or one of the libsm errors otherwise.
 *
 * Save the current values of the secure-side FP registers,
 * then set all of them to random values.
 */
#define SMC_FC_TEST_CLOBBER_FPSIMD_CLOBBER SMC_FASTCALL_NR(SMC_ENTITY_TEST, 0)

/**
 * SMC_SC_TEST_CLOBBER_FPSIMD_CHECK - Check and restore FP/SIMD
 * registers after an NS <-> TF-A <-> Trusty roundtrip.
 *
 * Return: 0 on success, or one of the libsm errors otherwise.
 *
 * The call should immediately follow a corresponding clobber,
 * since the latter stores some internal state in Trusty.
 */
#define SMC_FC_TEST_CLOBBER_FPSIMD_CHECK SMC_FASTCALL_NR(SMC_ENTITY_TEST, 1)

#define TRUSTY_STDCALLTEST_API_VERSION 1

void trusty_fpsimd_save_state(struct user_fpsimd_state *fp_regs);
void trusty_fpsimd_load_state(struct user_fpsimd_state *fp_regs);

#endif /* _TRUSTY_TEST_H */
