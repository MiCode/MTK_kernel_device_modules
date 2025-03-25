/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2022 ARM Ltd.
 * Copyright (C) 2023 Google, Inc.
 */
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#ifndef __LINUX_TRUSTY_FFA_H
#define __LINUX_TRUSTY_FFA_H

#include <linux/types.h>
#include <linux/uuid.h>
#ifndef MTK_ADAPTED
#include <linux/arm_ffa.h>
#endif

#ifdef MTK_ADAPTED
#include "ffa_v11/arm_ffa.h"
#endif

/**
 * DOC: FF-A driver version requirements
 *
 * This Trusty driver is dependent on a separate 'FF-A' driver.
 * To ensure comaptibility, the version of the FF-A driver
 * must be the same major version and a lower minor version as that
 * supported by this Trusty driver (defined below). Newer
 * versions of the FF-A driver may require changes to this driver
 * before adjusting the defines below.
 */
#define TRUSTY_FFA_VERSION_MAJOR	(1U)
#define TRUSTY_FFA_VERSION_MINOR	(0U)
#define TRUSTY_FFA_VERSION_MAJOR_SHIFT	(16U)
#define TRUSTY_FFA_VERSION_MAJOR_MASK	(0x7fffU)
#define TRUSTY_FFA_VERSION_MINOR_SHIFT	(0U)
#define TRUSTY_FFA_VERSION_MINOR_MASK	(0xffffU)

#define TO_TRUSTY_FFA_MAJOR(v)					\
	  ((u16)(((v) >> TRUSTY_FFA_VERSION_MAJOR_SHIFT) &	\
		 TRUSTY_FFA_VERSION_MAJOR_MASK))

#define TO_TRUSTY_FFA_MINOR(v)					\
	  ((u16)(((v) >> TRUSTY_FFA_VERSION_MINOR_SHIFT) &	\
		 TRUSTY_FFA_VERSION_MINOR_MASK))

int trusty_ffa_transport_init(void);
void trusty_ffa_transport_exit(void);

#endif /* __LINUX_TRUSTY_FFA_H */
