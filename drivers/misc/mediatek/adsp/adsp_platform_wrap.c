// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "adsp_core.h"
#include "adsp_platform_driver.h"
#include "adsp_platform_interface.h"

void adsp_set_swirq(u32 cid)
{
	if (unlikely(cid >= get_adsp_core_total()))
		return;

	if (unlikely(!adspsys))
		return;

	if (adspsys->hw_ops.set_swirq)
		adspsys->hw_ops.set_swirq(cid);
}

u32 adsp_check_swirq(u32 cid)
{
	if (unlikely(cid >= get_adsp_core_total()))
		return 0;

	if (unlikely(!adspsys))
		return 0;

	if (adspsys->hw_ops.check_swirq)
		return adspsys->hw_ops.check_swirq(cid);
	else
		return 0;
}

void adsp_clear_spm(u32 cid)
{
	if (unlikely(cid >= get_adsp_core_total()))
		return;

	if (unlikely(!adspsys))
		return;

	if (adspsys->hw_ops.clr_spm)
		adspsys->hw_ops.clr_spm(cid);
}

bool check_hifi_status(u32 mask)
{
	if (unlikely(!adspsys))
		return false;

	if (adspsys->hw_ops.check_hifi_status)
		return adspsys->hw_ops.check_hifi_status(mask);
	else
		return false;
}

u32 read_adsp_sys_status(u32 cid)
{
	if (unlikely(!adspsys))
		return 0;

	if (adspsys->hw_ops.read_adsp_sys_status)
		return adspsys->hw_ops.read_adsp_sys_status(cid);
	else
		return 0;
}

u32 get_adsp_sys_status(struct adsp_priv *pdata)
{
	u32 status;

	if (has_system_l2sram())
		status = read_adsp_sys_status(pdata->id);
	else
		adsp_copy_from_sharedmem(pdata,
				 ADSP_SHAREDMEM_SYS_STATUS,
				 &status, sizeof(status));
	return status;
}

bool is_adsp_axibus_idle(u32 *backup)
{
	if (unlikely(!adspsys))
		return true;

	if (adspsys->hw_ops.is_adsp_axibus_idle)
		return adspsys->hw_ops.is_adsp_axibus_idle(backup);
	else
		return true;
}

void adsp_toggle_semaphore(u32 bit)
{
	if (unlikely(!adspsys))
		return;

	if(adspsys->hw_ops.toggle_semaphore)
		adspsys->hw_ops.toggle_semaphore(bit);
}

u32 adsp_get_semaphore(u32 bit)
{
	if (unlikely(!adspsys))
		return 0;

	if (adspsys->hw_ops.get_semaphore)
		return adspsys->hw_ops.get_semaphore(bit);
	else
		return 0;
}

bool check_core_active(u32 cid)
{
	if (unlikely(cid >= get_adsp_core_total()))
		return false;

	if (unlikely(!adspsys))
		return false;

	if (adspsys->hw_ops.check_core_active)
		return adspsys->hw_ops.check_core_active(cid);
	else
	 	return false;
}

bool adsp_is_pre_lock_support(void)
{
	if (unlikely(!adspsys))
		return false;

	return (adspsys->infracfg_rsv != NULL);
}

int _adsp_pre_lock(u32 cid, bool is_lock)
{
	if (unlikely(cid >= get_adsp_core_total()))
		return -1;

	if (unlikely(!adspsys))
		return -1;

	if (adspsys->hw_ops.pre_lock)
		return adspsys->hw_ops.pre_lock(cid, true);
	else
		return -1;
}
