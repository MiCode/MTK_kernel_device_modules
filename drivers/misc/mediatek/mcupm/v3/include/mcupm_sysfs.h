/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef _MCUPM_SYSFS_H_
#define _MCUPM_SYSFS_H_

extern int multi_mcupm_plt_ackdata[max_mcupm];

int mcupms_sysfs_misc_init(void);
int mcupm_sysfs_create_file(struct device_attribute *attr);
int mcupm_sysfs_create_mcupm_alive(void);


#endif // _MCUM_SYSFS_H_

