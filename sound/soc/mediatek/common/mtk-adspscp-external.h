/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 MediaTek Inc.
 */

#ifndef MTK_ADSPSCP_EXTERNAL_H_
#define MTK_ADSPSCP_EXTERNAL_H_

#include <scp.h>
#include <linux/notifier.h>

typedef int (*scp_awake_lock_cb_t)(void *scp_id);
typedef int (*scp_awake_unlock_cb_t)(void *scp_id);
typedef int (*scp_clr_spm_reg_cb_t)(void *__unused);
typedef unsigned int (*is_scp_ready_cb_t)(enum scp_core_id id);
typedef void (*scp_A_register_notify_cb_t)(struct notifier_block *nb);

/* scp callback, decouple scp/ipi */
struct scp_system_callback_op {
	int (*scp_awake_lock_cb)(void *scp_id);
	int (*scp_awake_unlock_cb)(void *scp_id);
	int (*scp_clr_spm_reg_cb)(void *__unused);
	unsigned int (*is_scp_ready_cb)(enum scp_core_id id);
	void (*scp_A_register_notify_cb)(struct notifier_block *nb);
};

int scp_system_cb_init(struct scp_system_callback_op *sys_callback);

int scp_awake_lock_wrap(void *scp_id);
int scp_awake_unlock_wrap(void *scp_id);
int scp_clr_spm_reg_cb_wrap(void *__unused);
unsigned int is_scp_ready_wrap(int scp_id);
void scp_A_register_notify_wrap(struct notifier_block *nb);
struct scp_system_callback_op *get_scp_system_op(void);

#endif /* MTK_ADSPSCP_EXTERNAL_H_ */
