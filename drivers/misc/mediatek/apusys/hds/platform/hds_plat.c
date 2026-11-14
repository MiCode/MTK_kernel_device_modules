// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <hds.h>
#include <hds_plat.h>

#define NPU_HDS_GET_HW_VERSION(ver) (ver & 0xffff)

static int hds_plat_init(struct apu_hds_device *hdev)
{
	apu_hds_err("apu hds not initialized\n");

	return -ENODEV;
}

static void hds_plat_deinit(struct apu_hds_device *hdev)
{
	apu_hds_err("apu hds not initialized\n");
}

static int hds_cmd_postprocess_late(struct apu_hds_device *hdev, void *va, uint32_t size,
	uint32_t power_plcy)
{
	apu_hds_err("apu hds not initialized\n");
	return -ENODEV;
}

static struct hds_plat_func hds_hw_plat_func = {
	.plat_init = hds_plat_init,
	.plat_deinit = hds_plat_deinit,
	.cmd_postprocess_late = hds_cmd_postprocess_late,
};

struct hds_plat_func *hds_plat_get_funcs(uint32_t version_hw, uint32_t version_date, uint32_t version_revision)
{
	struct hds_plat_func *hpf = NULL;

	if (NPU_HDS_GET_HW_VERSION(version_hw) == 0x0100)
		hpf = &hds_hw_plat_func_1_0;
	else
		hpf = &hds_hw_plat_func;

	return hpf;
}
