
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * sy6541.h
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
#ifndef __SY6541_H__
#define __SY6541_H__

#define SY6541_DEVICE_ID 0x46

// extern int sy_cp_enable_adc(struct sc8581_device *bq, bool enable);
// extern int is_sy6541_cp(struct sc8581_device *bq);
extern int sy6541_probe(struct i2c_client *client);

#endif /* __SY6541_H__ */
