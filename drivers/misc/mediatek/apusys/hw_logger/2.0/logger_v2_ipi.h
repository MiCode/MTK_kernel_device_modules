// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __LOGGER_V2_IPI_H__
#define __LOGGER_V2_IPI_H__

#include <linux/seq_file.h>
#include "apu.h"

void set_log_level(int level);
int logger_v2_ipi_init(struct mtk_apu *apu);
void logger_v2_ipi_remove(struct mtk_apu *apu);
int logger_v2_rpc_dump(void);
int logger_v2_debug_info_dump(struct seq_file *s);
int logger_v2_counting_hw_sema_reader_trylock(void);
int logger_v2_counting_hw_sema_reader_unlock(void);

#endif