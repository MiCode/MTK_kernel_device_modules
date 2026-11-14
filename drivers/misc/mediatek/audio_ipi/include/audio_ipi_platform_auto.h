/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __AUDIO_IPI_PLATFORM_AUTO_H__
#define __AUDIO_IPI_PLATFORM_AUTO_H__

#if IS_ENABLED(CONFIG_DEVICE_MODULES_VHOST_ADSP)
extern long audio_ipi_kernel_ioctl(unsigned int cmd, unsigned long arg);
#endif
#endif /*__AUDIO_IPI_PLATFORM_AUTO_H__ */
