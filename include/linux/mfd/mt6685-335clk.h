/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */
#ifndef _335_DCXO_H_
#define _335_DCXO_H_

/* just be called by 335 module for dcxo */
extern void mt6685_set_335_dcxo_mode(unsigned int mode);
extern void mt6685_set_335_dcxo(bool enable);
#endif
