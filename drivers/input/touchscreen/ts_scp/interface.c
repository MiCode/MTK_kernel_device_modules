// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) "ts_scp_interface " fmt

#include <linux/string.h>
#include <linux/input.h>
#include "ts_scp_core.h"

struct generic_pinctrl *generic_pinctrl;
void (*generic_irq_enable)(bool enable);
void (*generic_esd_control)(bool enable);
int (*generic_power_on_reinit_func)(void) = NULL;

void connect_irq(void (*irq_func)(bool enable))
{
	ts_scp_info("connect irq");
	generic_irq_enable = irq_func;
}
EXPORT_SYMBOL_GPL(connect_irq);

void connect_esd_control(void (*esd_ctrl_func)(bool enable))
{
	ts_scp_info("connect esd control");
	generic_esd_control = esd_ctrl_func;
}
EXPORT_SYMBOL_GPL(connect_esd_control);

void generic_power_on_reinit_register(int (*reinit_func)(void))
{
	ts_scp_info("generic_power_on_reinit register");
	generic_power_on_reinit_func = reinit_func;
}
EXPORT_SYMBOL_GPL(generic_power_on_reinit_register);

int generic_pinctrl_init(void)
{
	generic_pinctrl = kmalloc(sizeof(struct generic_pinctrl), GFP_KERNEL);
    if (!generic_pinctrl) {
        ts_scp_err("Failed to allocate memory for generic_pinctrl");
        return -ENOMEM;
    }

    generic_pinctrl->pinctrl = NULL;
    generic_pinctrl->spi_default = NULL;
    generic_pinctrl->int_active = NULL;
    generic_pinctrl->rst_active = NULL;
    generic_pinctrl->int_suspend = NULL;
    generic_pinctrl->rst_suspend = NULL;
	generic_pinctrl->touch_mode_ap = NULL;
	generic_pinctrl->touch_mode_scp = NULL;

	return 0;
}
EXPORT_SYMBOL_GPL(generic_pinctrl_init);

int generic_pinctrl_remove(void)
{
	generic_pinctrl->pinctrl = NULL;
    generic_pinctrl->spi_default = NULL;
    generic_pinctrl->int_active = NULL;
    generic_pinctrl->rst_active = NULL;
    generic_pinctrl->int_suspend = NULL;
    generic_pinctrl->rst_suspend = NULL;
	generic_pinctrl->touch_mode_ap = NULL;
	generic_pinctrl->touch_mode_scp = NULL;

	kfree(generic_pinctrl);
    generic_pinctrl = NULL;

	return 0;
}
EXPORT_SYMBOL_GPL(generic_pinctrl_remove);

void connect_pinctrl(struct pinctrl *pinctrl)
{
	if (!pinctrl) {
        ts_scp_err("Invalid pinctrl passed to connect_pinctrl");
        return;
    }
	if (!generic_pinctrl) {
        ts_scp_err("generic_pinctrl is not initialized");
        return;
    }
	generic_pinctrl->pinctrl = pinctrl;
	ts_scp_info("connect_pinctrl success");
}
EXPORT_SYMBOL_GPL(connect_pinctrl);

void connect_pinctrl_spi(struct pinctrl_state *spi_default)
{
	if (!spi_default) {
        ts_scp_err("Invalid pinctrl state passed to connect_pinctrl_spi");
        return;
    }
	generic_pinctrl->spi_default = spi_default;
	ts_scp_info("connect_pinctrl_spi success");
}
EXPORT_SYMBOL_GPL(connect_pinctrl_spi);

void connect_pinctrl_int_resume(struct pinctrl_state *int_active)
{
	if (!int_active) {
        ts_scp_err("Invalid pinctrl state passed to connect_pinctrl_int_resume");
        return;
    }
	generic_pinctrl->int_active = int_active;
	ts_scp_info("connect_pinctrl_int_resume success");
}
EXPORT_SYMBOL_GPL(connect_pinctrl_int_resume);

void connect_pinctrl_rst_resume(struct pinctrl_state *rst_active)
{
	if (!rst_active) {
        ts_scp_err("Invalid pinctrl state passed to connect_pinctrl_rst_resume");
        return;
    }
	generic_pinctrl->rst_active = rst_active;
	ts_scp_info("connect_pinctrl_rst_resume success");
}
EXPORT_SYMBOL_GPL(connect_pinctrl_rst_resume);

void connect_pinctrl_int_suspend(struct pinctrl_state *int_suspend)
{
	if (!int_suspend) {
        ts_scp_err("Invalid pinctrl state passed to connect_pinctrl_int_suspend");
        return;
    }
	generic_pinctrl->int_suspend = int_suspend;
	ts_scp_info("connect_pinctrl_int_suspend success");
}
EXPORT_SYMBOL_GPL(connect_pinctrl_int_suspend);

void connect_pinctrl_rst_suspend(struct pinctrl_state *rst_suspend)
{
	if (!rst_suspend) {
        ts_scp_err("Invalid pinctrl state passed to connect_pinctrl_rst_suspend");
        return;
    }
	generic_pinctrl->rst_suspend = rst_suspend;
	ts_scp_info("connect_pinctrl_rst_suspend success");
}
EXPORT_SYMBOL_GPL(connect_pinctrl_rst_suspend);

void connect_pinctrl_touch_mode_ap(struct pinctrl_state *touch_mode_ap)
{
	if (!touch_mode_ap) {
        ts_scp_err("Invalid pinctrl state passed to connect_pinctrl_touch_mode_ap");
        return;
    }
	generic_pinctrl->touch_mode_ap = touch_mode_ap;
	ts_scp_info("connect_pinctrl_touch_mode_ap success");
}
EXPORT_SYMBOL_GPL(connect_pinctrl_touch_mode_ap);

void connect_pinctrl_touch_mode_scp(struct pinctrl_state *touch_mode_scp)
{
	if (!touch_mode_scp) {
        ts_scp_err("Invalid pinctrl state passed to connect_pinctrl_touch_mode_scp");
        return;
    }
	generic_pinctrl->touch_mode_scp = touch_mode_scp;
	ts_scp_info("connect_pinctrl_touch_mode_scp success");
}
EXPORT_SYMBOL_GPL(connect_pinctrl_touch_mode_scp);
