/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Wendy-ST Lin <wendy-st.lin@mediatek.com>
 */
#ifndef MMQOS_MMUP_H
#define MMQOS_MMUP_H

#include "mmqos-global.h"

#define IPI_TIMEOUT_MS	(200U)

#if IS_ENABLED(CONFIG_MTK_MMQOS_VCP)
int mmqos_mmup_init_thread(void *data);
int mtk_mmqos_enable_mmup(const bool enable);
int mmqos_mmup_ipi_send(const u8 func, u32 data);
#else
static inline int mmqos_mmup_init_thread(void *data)
{	return -EINVAL; }
static inline int mtk_mmqos_enable_mmup(const bool enable)
{	return -EINVAL; }
static inline int mmqos_mmup_ipi_send(const u8 func, u32 data)
{	return -EINVAL; }
#endif /* CONFIG_MTK_MMQOS_VCP*/

struct mmqos_mmup_ipi_data {
	uint8_t func;
	uint8_t idx;
	uint8_t ack;
	uint32_t base;
};

enum mmup_ipi_func_id {
	FUNC_MMUP_SYNC_STATE,
	FUNC_MMUP_SMMU_FACTOR,
	FUNC_MMUP_THRESHOLD_US,
	FUNC_MMUP_LOG,
	FUNC_MMUP_NUM
};
#endif /* MMQOS_VCP_H */
