// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include "mvpu_plat.h"
#include "mvpu30_ipi.h"
#include "mvpu30_handler.h"
#include "mvpu30_sec.h"

struct mvpu_sec_ops mvpu30_sec_ops = {
	.mvpu_sec_init = mvpu30_sec_init,
	.mvpu_load_img = mvpu30_load_img,
	.mvpu_sec_sysfs_init = mvpu30_sec_sysfs_init
};

struct mvpu_ops mvpu30_ops = {
	.mvpu_ipi_init = mvpu30_ipi_init,
	.mvpu_ipi_deinit = mvpu30_ipi_deinit,
	.mvpu_ipi_send = mvpu30_ipi_send,
	.mvpu_handler_lite_init = mvpu30_handler_lite_init,
	.mvpu_handler_lite = mvpu30_handler_lite,
};

struct mvpu_platdata mvpu_mt6993_platdata = {
	.sw_preemption_level = 1,
	.sw_ver = MVPU_SW_VER_MVPU30,
	.ops = &mvpu30_ops,
	.sec_ops = &mvpu30_sec_ops
};
