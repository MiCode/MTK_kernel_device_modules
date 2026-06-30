// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/errno.h>
//#include <linux/io.h>

#include "mtk_disp_mbrain.h"
#include "mtk_drm_drv.h"

static void (*notify_callback)(enum MB_DISP_EVENT event, void *data);

int disp_mb_register(void (*callback)(enum MB_DISP_EVENT event, void *data))
{
	notify_callback = callback;
	pr_info("DISP_MB %s done\n", __func__);
	return 0;
}
EXPORT_SYMBOL(disp_mb_register);

int disp_mb_unregister(void)
{
	pr_info("DISP_MB");

	return 0;
}
EXPORT_SYMBOL(disp_mb_unregister);

int disp_mb_event_trigger(struct mb_disp_data *mb_data)
{
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_AUTO_MBRAIN) && IS_ENABLED(CONFIG_MTK_MBRAINK_AUTO)
	if (notify_callback == NULL) {
		pr_info("error: DISP_MB No callback registered\n");
		return -EINVAL;
	}

	if (!mb_data) {
		pr_info("error: DISP_MB mb_data is NULL");
		return -EINVAL;
	}

	if (mb_data->event < 0 || mb_data->event >= DISP_EVENT_MAX) {
		pr_info("error: DISP_MB invalid event id\n");
		return -EINVAL;
	}

	mb_data->disp_clk_normal = 624000000;
	mb_data->disp_power_normal = 725;
	mb_data->disp_clk_cur = 624000000;
	mb_data->disp_power_cur = 725;

	pr_info("DISP_MB %s event:%d\n", __func__, mb_data->event);

	notify_callback(mb_data->event, mb_data);

#endif
	return 0;
}
EXPORT_SYMBOL(disp_mb_event_trigger);
