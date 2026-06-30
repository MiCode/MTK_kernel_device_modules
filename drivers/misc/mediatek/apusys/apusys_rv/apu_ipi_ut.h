/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef APU_IPI_UT_H
#define APU_IPI_UT_H

#include <linux/kernel.h>

int apu_ipi_ut_init(struct mtk_apu *apu);
void apu_ipi_ut_exit(void);
void apu_power_on_off_profile(u32 on, u32 off,
	uint64_t time_diff_ns, uint64_t time_diff_ns2);
int rv_bsp_rx_init(struct mtk_apu *apu);
void rv_bsp_rx_exit(void);

#endif /* APU_IPI_UT_H */
