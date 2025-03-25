/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef __MTK_IRQ_MON__
#define __MTK_IRQ_MON__

enum irq_mon_aee_type {
	IRQ_MON_AEE_TYPE_BURST_IRQ = 0,
	IRQ_MON_AEE_TYPE_IRQ_LONG,
	IRQ_MON_AEE_TYPE_LONG_IRQOFF,
};
typedef int (*aee_callback_t)(unsigned int irq, enum irq_mon_aee_type type);

#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR)
int irq_mon_aee_period_set(unsigned int irq, unsigned int period);
int irq_mon_aee_callback_register(unsigned int irq, aee_callback_t fn);
void irq_mon_aee_callback_unregister(unsigned int irq);
void mt_aee_dump_irq_info(void);
void __irq_log_store(const char *func, int line);
void irq_log_entry_store(void *func);
void irq_log_exit_store(void *func);
#define irq_log_store() __irq_log_store(__func__, __LINE__)
#else
static inline int irq_mon_aee_period_set(unsigned int irq, unsigned int period)
{
	return 0;
}
static inline int irq_mon_aee_callback_register(unsigned int irq,
						aee_callback_t fn)
{
	return -EOPNOTSUPP;
}
static inline void irq_mon_aee_callback_unregister(unsigned int irq)
{
}
#define mt_aee_dump_irq_info() do {} while (0)
static inline void __irq_log_store(const char *func, int line)
{
}
static inline void irq_log_entry_store(void *func)
{
}
static inline void irq_log_exit_store(void *func)
{
}
#define irq_log_store() do {} while (0)
#endif
#endif
