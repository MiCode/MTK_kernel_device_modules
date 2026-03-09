
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * hl7243.h
 *
 * charge-pump ic driver
 *
 * Copyright (c) 2023-2023 Xiaomi Technologies Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */
 #ifndef __HL7243_H__
 #define __HL7243_H__
 #define HL7243_DEVICE_ID 0x0C
 // extern int sy_cp_enable_adc(struct sc8581_device *bq, bool enable);
 // extern int is_hl7243_cp(struct sc8581_device *bq);

 extern int hl7243_charger_probe(struct i2c_client *client);
 #endif /* __HL7243_H__ */
 