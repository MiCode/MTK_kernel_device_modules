/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef _TINY_CRC8_H_
#define _TINY_CRC8_H_

#include <linux/types.h>

uint8_t tiny_crc8(const void *ptr, size_t len);

#endif
