/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __CCCI_FSM_H__
#define __CCCI_FSM_H__

enum MD_IRQ_TYPE {
	MD_IRQ_WDT,
	MD_IRQ_CCIF_EX,
};

int ccci_fsm_init(void);
int ccci_fsm_recv_control_packet(struct sk_buff *skb);
int ccci_fsm_recv_status_packet(struct sk_buff *skb);
int ccci_fsm_recv_md_interrupt(enum MD_IRQ_TYPE type);
long ccci_fsm_ioctl(unsigned int cmd, unsigned long arg);
enum MD_STATE ccci_fsm_get_md_state(void);
enum MD_STATE_FOR_USER ccci_fsm_get_md_state_for_user(void);

extern void mdee_set_ex_time_str(unsigned int type, char *str);

int ccci_fsm_increase_devapc_dump_counter(void);

u8 ccci_md_get_support_microsecond_version(void);

#endif /* __CCCI_FSM_H__ */

