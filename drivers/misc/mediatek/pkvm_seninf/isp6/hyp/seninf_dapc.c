// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#include <pkvm_sys.h>
#include "seninf_dapc.h"

int seninf_dapc_lock(void)
{
	APC_ImgsensorProtEnable();
	return 0;
}

int seninf_dapc_unlock(void)
{
	APC_ImgsensorProtDisable();
	return 0;
}
