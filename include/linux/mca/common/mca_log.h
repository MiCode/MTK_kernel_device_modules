/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mca_log.h
 *
 * mca log driver
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
#ifndef __MCA_LOG_H__
#define __MCA_LOG_H__

#define _mca_log_err(fmt, ...) __mca_log_err(fmt, ##__VA_ARGS__)
#define _mca_log_info(fmt, ...) __mca_log_info(fmt, ##__VA_ARGS__)
#define _mca_log_debug(fmt, ...) __mca_log_debug(fmt, ##__VA_ARGS__)
#define mca_log_err(fmt, ...) _mca_log_err("["MCA_LOG_TAG"]""%s:%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define mca_log_info(fmt, ...) _mca_log_info("["MCA_LOG_TAG"]""%s:%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define mca_log_debug(fmt, ...) _mca_log_debug("["MCA_LOG_TAG"]""%s:%d " fmt, __func__, __LINE__, ##__VA_ARGS__)

#define mca_log_jirabot(fmt, ...) _mca_log_err("[ARCH-TF-CHARGER]["MCA_LOG_TAG"]""%s:%d " fmt, __func__, __LINE__, ##__VA_ARGS__)

enum mca_charge_log_id_ele {
	MCA_CHARGE_LOG_ID_BUSINESS_CHG = 0,
	MCA_CHARGE_LOG_ID_THERMAL,
	MCA_CHARGE_LOG_ID_BATTERY_INFO,
	MCA_CHARGE_LOG_ID_FG_MASTER_IC,
	MCA_CHARGE_LOG_ID_FG_SLAVE_IC,
	MCA_CHARGE_LOG_ID_CP_MASTER_IC,
	MCA_CHARGE_LOG_ID_CP_SLAVE_IC,
	MCA_CHARGE_LOG_ID_USCP,
	MCA_CHARGE_LOG_ID_MAX,
};

struct mca_log_charge_log_ops {
	int (*dump_log_head)(void *data, char *buf, int size);
	int (*dump_log_context)(void *data, char *buf, int size);
};

void __mca_log_err(const char *format, ...);
void __mca_log_info(const char *format, ...);
void __mca_log_debug(const char *format, ...);
void mca_log_charge_log_register(enum mca_charge_log_id_ele type,
	struct mca_log_charge_log_ops *ops, void *data);
int mca_log_get_charge_boot_mode(void);

#endif /* __MCA_LOG_H__ */

