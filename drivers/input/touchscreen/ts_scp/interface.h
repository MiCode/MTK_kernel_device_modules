/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef _TS_SCP_INTERFACE_H_
#define _TS_SCP_INTERFACE_H_

struct generic_pinctrl {
    struct pinctrl *pinctrl;
    struct pinctrl_state *spi_default;
    struct pinctrl_state *int_active;
    struct pinctrl_state *rst_active;
    struct pinctrl_state *int_suspend;
    struct pinctrl_state *rst_suspend;
    struct pinctrl_state *touch_mode_ap;
    struct pinctrl_state *touch_mode_scp;
};

extern struct generic_pinctrl *generic_pinctrl;
extern void (*generic_irq_enable)(bool enable);
extern void (*generic_esd_control)(bool enable);
extern int (*generic_power_on_reinit_func)(void);

void connect_irq(void (*irq_func)(bool enable));
void connect_esd_control(void (*esd_ctrl_func)(bool enable));
void generic_power_on_reinit_register(int (*reinit_func)(void));
int generic_pinctrl_init(void);
int generic_pinctrl_remove(void);
void connect_pinctrl(struct pinctrl *pinctrl);
void connect_pinctrl_spi(struct pinctrl_state *spi_default);
void connect_pinctrl_int_resume(struct pinctrl_state *int_active);
void connect_pinctrl_rst_resume(struct pinctrl_state *rst_active);
void connect_pinctrl_int_suspend(struct pinctrl_state *int_suspend);
void connect_pinctrl_rst_suspend(struct pinctrl_state *rst_suspend);
void connect_pinctrl_touch_mode_ap(struct pinctrl_state *touch_mode_ap);
void connect_pinctrl_touch_mode_scp(struct pinctrl_state *touch_mode_scp);

#endif
