// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __LOGGER_V2_PALT_H__
#define __LOGGER_V2_PALT_H__

#define HWLOG_LINE_SIZE    (64) // 64bytes pre line

enum LOG_BUFF_TYPE {
	LOG_BUFF_NP,
	MAX_LOG_BUFF_TYPE
};

extern void mt_irq_dump_status(unsigned int irq);
void logger_v2_irq_debug_status_dump(void);
void logger_v2_buf_invalidate(enum LOG_BUFF_TYPE buff_type);
void logger_v2_clear_buf(enum LOG_BUFF_TYPE buff_type);
int logger_v2_get_buf_info(enum LOG_BUFF_TYPE buff_type,
	char **buf_base, unsigned int *buf_size);
unsigned int logger_v2_get_w_ofs(void);

#endif /* __LOGGER_V2_PALT_H__ */
