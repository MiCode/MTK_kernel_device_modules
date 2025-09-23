/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef PKVM_SENINF_IOCTL_H
#define PKVM_SENINF_IOCTL_H

#define PKVM_SENINF_DEV_NAME "pkvm_seninf"

enum pkvm_seninf_ioctl_cmd {
	IOCTL_ID_PKVM_SENINF_IS_ENABLED,
	IOCTL_ID_PKVM_SENINF_CHECKPIPE,
	IOCTL_ID_PKVM_SENINF_FREE,
};

#endif // PKVM_SENINF_IOCTL_H
