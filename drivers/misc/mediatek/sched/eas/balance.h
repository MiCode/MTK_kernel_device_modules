/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef _BALANCE_H
#define _BALANCE_H

void hook_can_migrate_task(void *data, struct task_struct *p, int dst_cpu, int *can_migrate);

void check_for_migration(struct task_struct *p);

#endif /* _BALANCE_H */
