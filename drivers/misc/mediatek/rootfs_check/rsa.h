/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 MediaTek Inc.
 */

#ifndef _RSA_H
#define _RSA_H

#define RSA_LEN 256
#define SHA256_LEN 32

uint8_t *rsa_encryptdecrypt(const uint8_t *sig,
					const uint8_t *e, const uint8_t *n);

#endif
