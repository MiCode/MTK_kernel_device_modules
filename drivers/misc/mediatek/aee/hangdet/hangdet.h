/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#if !defined(__HANGDET_H__)
#define __HANGDET_H__

void percpu_debug_timer_init(void);
void save_timer_list_info(void);
void timer_list_debug_init(void);
void timer_list_debug_exit(void);

#endif
