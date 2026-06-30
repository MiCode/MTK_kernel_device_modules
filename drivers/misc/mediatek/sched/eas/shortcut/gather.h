/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef _GATHER_H
#define _GATHER_H

bool gather_to_gear(struct task_struct *p, int *end_index);

void set_gear_hints_gathering_th(int tsk_util);

void reset_gear_hints_gathering_th(void);

int get_gear_hints_gathering_th(void);

#endif /* _GATHER_H */
