/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef _COMPRESS_H
#define _COMPRESS_H

struct relax_enough_util {
	int util;
	bool user;
};

struct shortcut_compress_relax_enough_args {
	int cluster_idx;
	int util;
};

void compress_init(void);

void update_shortcut_compress_relax_enough_cpu_util(int cluster_idx);
void update_shortcut_compress_relax_enough_tsk_util(int cluster_idx);

int compress_to_cpu(struct task_struct *p, unsigned long *tsk_min_clp, unsigned long *tsk_max_clp, int order_index);
int compress_to_cpu_pro(struct task_struct *p, unsigned long *tsk_min_clp, unsigned long *tsk_max_clp, int order_index);
int compress_to_cpu_air(struct task_struct *p, int order_index);

void set_shortcut_compress_rate(int rate);
void set_shortcut_compress_relax_enough_cpu_util(int cluster_idx, int cpu_util);
void set_shortcut_compress_relax_enough_tsk_util(int cluster_idx, int tsk_util);

void reset_shortcut_compress_rate(void);
void reset_shortcut_compress_relax_enough_cpu_util(int cluster_idx);
void reset_shortcut_compress_relax_enough_tsk_util(int cluster_idx);

int get_shortcut_compress_rate(void);
int get_shortcut_compress_relax_enough_cpu_util(int cluster_idx);
int get_shortcut_compress_relax_enough_tsk_util(int cluster_idx);

#endif /* _COMPRESS_H */
