/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef SAR_DETECTOR_H
#define SAR_DETECTOR_H
#include <linux/ioctl.h>
int __init sar_detector_init(void);
void __exit sar_detector_exit(void);
#endif
