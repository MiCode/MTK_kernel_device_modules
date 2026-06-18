/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef _ocp2131_SW_H_
#define _ocp2131_SW_H_

extern int ocp2131_i2c_read_bytes(unsigned char addr, unsigned char *returnData);
extern int ocp2131_i2c_write_bytes(unsigned char addr, unsigned char value);

#endif
