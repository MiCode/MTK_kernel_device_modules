/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#if !defined(__HANGDET_H__)
#define __HANGDET_H__

#include <linux/sched.h>

void percpu_debug_timer_init(void);
void save_timer_list_info(void);
void timer_list_debug_init(void);
void timer_list_debug_exit(void);

#if !IS_ENABLED(CONFIG_ARM64)
extern int nr_ipi_get(void);
extern struct irq_desc **ipi_desc_get(void);
#define __pa_nodebug(x)		__virt_to_phys_nodebug((unsigned long)(x))
#else
struct slp_history {
	int cpu;
	unsigned long long sc;
	struct hrtimer *timer;
};
#endif


#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR_DEBUG)
struct arch_timer_caller_history_struct {
	unsigned long timer_caller_ip;
	u64 timer_called;
	ktime_t now;
	char comm[TASK_COMM_LEN];
};
#endif

#endif
