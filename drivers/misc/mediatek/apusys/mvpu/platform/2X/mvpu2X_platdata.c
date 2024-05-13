// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include "mvpu_plat.h"
#include "mvpu2X_ipi.h"
#include "mvpu2X_handler.h"
#include "mvpu2X_sec.h"

struct mvpu_sec_ops mvpu2X_sec_ops = {
	.mvpu_sec_init = mvpu2X_sec_init,
	.mvpu_load_img = mvpu2X_load_img,
	.mvpu_sec_sysfs_init = mvpu2X_sec_sysfs_init
};

struct mvpu_ops mvpu2X_ops = {
	.mvpu_ipi_init = mvpu2X_ipi_init,
	.mvpu_ipi_deinit = mvpu2X_ipi_deinit,
	.mvpu_ipi_send = mvpu2X_ipi_send,
	.mvpu_handler_lite_init = mvpu2X_handler_lite_init,
	.mvpu_handler_lite = mvpu2X_handler_lite,
};

struct mvpu_ops mvpu25a_ops = {
	.mvpu_ipi_init = mvpu25a_ipi_init,
	.mvpu_ipi_deinit = mvpu25a_ipi_deinit,
	.mvpu_ipi_send = mvpu2X_ipi_send,
	.mvpu_handler_lite_init = mvpu2X_handler_lite_init,
	.mvpu_handler_lite = mvpu2X_handler_lite,
};

struct mvpu_platdata mvpu_mt6983_platdata = {
	.sw_preemption_level = 1,
	.sw_ver = MVPU_SW_VER_MVPU2X,
	.ops = &mvpu2X_ops,
	.sec_ops = &mvpu2X_sec_ops
};

struct mvpu_platdata mvpu_mt8139_platdata = {
	.sw_preemption_level = 1,
	.sw_ver = MVPU_SW_VER_MVPU2X,
	.ops = &mvpu2X_ops,
	.sec_ops = &mvpu2X_sec_ops
};

struct mvpu_platdata mvpu_mt6879_platdata = {
	.sw_preemption_level = 1,
	.sw_ver = MVPU_SW_VER_MVPU2X,
	.ops = &mvpu2X_ops,
	.sec_ops = &mvpu2X_sec_ops
};

struct mvpu_platdata mvpu_mt6895_platdata = {
	.sw_preemption_level = 1,
	.sw_ver = MVPU_SW_VER_MVPU2X,
	.ops = &mvpu2X_ops,
	.sec_ops = &mvpu2X_sec_ops
};

struct mvpu_platdata mvpu_mt6985_platdata = {
	.sw_preemption_level = 1,
	.sw_ver = MVPU_SW_VER_MVPU2X,
	.ops = &mvpu2X_ops,
	.sec_ops = &mvpu2X_sec_ops
};

struct mvpu_platdata mvpu_mt6886_platdata = {
	.sw_preemption_level = 1,
	.sw_ver = MVPU_SW_VER_MVPU2X,
	.ops = &mvpu2X_ops,
	.sec_ops = &mvpu2X_sec_ops
};

struct mvpu_platdata mvpu_mt6897_platdata = {
	.sw_preemption_level = 1,
	.sw_ver = MVPU_SW_VER_MVPU2X,
	.ops = &mvpu2X_ops,
	.sec_ops = &mvpu2X_sec_ops
};

struct mvpu_platdata mvpu_mt6989_platdata = {
	.sw_preemption_level = 1,
	.sw_ver = MVPU_SW_VER_MVPU2X,
	.ops = &mvpu2X_ops,
	.sec_ops = &mvpu2X_sec_ops
};

struct mvpu_platdata mvpu_mt6991_platdata = {
	.sw_preemption_level = 1,
	.sw_ver = MVPU_SW_VER_MVPU25a,
	.ops = &mvpu25a_ops,
	.sec_ops = &mvpu2X_sec_ops
};


