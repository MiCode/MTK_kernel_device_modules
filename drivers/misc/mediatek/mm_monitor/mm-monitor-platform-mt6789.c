// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */
#include "mtk-mm-monitor-controller.h"
#include <linux/io.h>
#include <linux/of_device.h>
#include <dt-bindings/memory/mt6789-smi-pd.h>

#define MM_MONITOR_SUBSYS_MAX 28
#define MM_MONITOR_AXI_MAX 32

/* fake engine related settings */
/* CTI related settings */
/* EMI monitor settings */
/* MUX ID */

u16 get_freq_from_mux_id(enum MUX_ID id)
{
	return 0;
}
EXPORT_SYMBOL(get_freq_from_mux_id);

u32 get_mmmc_subsys_max(void)
{
	return MM_MONITOR_SUBSYS_MAX;
}
EXPORT_SYMBOL(get_mmmc_subsys_max);

u32 get_mminfra_pd(void)
{
	return MT6991_SMI_PD_MMINFRA1;
}
EXPORT_SYMBOL(get_mminfra_pd);

s32 is_valid_offset_value(u32 hw, u32 id, u32 offset, u32 value)
{
	return 0;
}
EXPORT_SYMBOL(is_valid_offset_value);

void enable_mminfra_funnel(void)
{
	return;
}
EXPORT_SYMBOL(enable_mminfra_funnel);

void mminfra_fake_engine_bus_settings(void)
{
	return;
}
EXPORT_SYMBOL(mminfra_fake_engine_bus_settings);

void emi_moniter_settings(void)
{
	return;
}
EXPORT_SYMBOL(emi_moniter_settings);

u32 get_power_domains(int index)
{
	return 0;
}
EXPORT_SYMBOL(get_power_domains);

static int __init mm_monitor_platform_init(void)
{
	MM_MONITOR_DBG("enter");

	return 0;
}
module_init(mm_monitor_platform_init);
MODULE_LICENSE("GPL");
