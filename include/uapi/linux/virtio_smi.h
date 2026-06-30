/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Virtio_smi definition v0.1
 *
 * Copyright (C) 2019 Arm Ltd.
 * Copyright (c) 2024 MediaTek Inc.
 */
#ifndef _VIRTIO_SMI_H
#define _VIRTIO_SMI_H

#include <linux/types.h>
/* for larb: id is 0 ~ MTK_LARB_NR_MAX(64)
 * for common: id is 0~ MTK_COMMON_NR_MAX(33)
 */
int vsmi_larb_power_on(unsigned int id);
int vsmi_larb_power_off(unsigned int id);
int vsmi_common_power_on(unsigned int id);
int vsmi_common_power_off(unsigned int id);
#endif
