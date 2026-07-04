/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _ktz8866_SW_H_
#define _ktz8866_SW_H_

extern int lcd_set_bias(int enable);
extern int lcd_set_bl_bias_reg(int enable);
extern int lcd_reg_id;
extern int ktz8864_read_id(void);
extern int iic_backlight_set(unsigned int value);
#endif
