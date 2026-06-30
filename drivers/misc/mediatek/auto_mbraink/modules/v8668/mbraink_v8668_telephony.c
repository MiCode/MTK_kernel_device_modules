// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>

#include <mbraink_modules_ops_def.h>
#include "mbraink_v8668_telephony.h"

#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
#include "mtk_ccci_common.h"
#endif

#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
static void mbrain_ccci_mb_event_notify(unsigned long event, void *data)
{
	char netlink_buf[NETLINK_EVENT_AUTO_MESSAGE_SIZE] = {'\0'};
	int n __maybe_unused = 0;
	struct md_wakeup_source *wakeup_source = (struct md_wakeup_source *)data;

	if (!wakeup_source) {
		pr_info("[%s] wakeup_source is null\n", __func__);
		return;
	}
	if (event != E_EVENT_AUTO_CCCI_WAKE_UP_NOTIFY)
		return;
	n = snprintf(netlink_buf, NETLINK_EVENT_AUTO_MESSAGE_SIZE,
		"%s:%s:%s",
		NETLINK_EVENT_CCCI_WAKEUP_NOTIFY,
		wakeup_source->mdWakeupType,
		wakeup_source->wakeupSource);
	pr_notice("[%s] mdWakeupType = %s, wakeupSource = %s\n", __func__,
		wakeup_source->mdWakeupType, wakeup_source->wakeupSource);

	if (n < 0 || n >= NETLINK_EVENT_AUTO_MESSAGE_SIZE) {
		pr_info("%s : snprintf error n = %d\n", __func__, n);
		return;
	}
	mbraink_netlink_send_msg(netlink_buf);
}

int mbraink_ccci_init(void)
{
	ccci_md_wakeup_source_register(mbrain_ccci_mb_event_notify);
	return 0;
}

int mbraink_ccci_deinit(void)
{
	ccci_md_wakeup_source_unregister();
	return 0;
}
#endif

int mbraink_v8668_telephony_init(void)
{
	int ret = 0;

#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
	ret = mbraink_ccci_init();
	if (ret)
		pr_notice("mbraink ccci init failed.\n");
#endif
	return ret;
}

int mbraink_v8668_telephony_deinit(void)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
	mbraink_ccci_deinit();
#endif

	return ret;
}
