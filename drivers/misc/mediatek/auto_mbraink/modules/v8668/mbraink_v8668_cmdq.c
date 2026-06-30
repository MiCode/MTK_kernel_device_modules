// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#include <linux/mailbox/mtk-cmdq-mailbox-ext.h>
#include <linux/uio.h>
#include <proto.h>

#include "mbraink_v8668_cmdq.h"

#if IS_ENABLED(CONFIG_CMDQ_MBRAIN)
int mbrain_cmdq_latency_notify(char *data)
{
	char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};
	int n __maybe_unused = 0;

	if (!data) {
		pr_info("[%s] cmdq_mbrain is null\n", __func__);
		return -1;
	}

	n = snprintf(netlink_buf, NETLINK_EVENT_MESSAGE_SIZE,
		"%s:%llu:%llu:%llu:%llu:%lu:%lu:%lu:%lu:%lu:%lu:%lu:%lu:%lu:%lu:%lu",
		NETLINK_EVENT_CMDQ_LATENCY_NOTIFY,
		101ULL,
		102ULL,
		103ULL,
		104ULL,
		105UL,
		106UL,
		107UL,
		108UL,
		109UL,
		110UL,
		111UL,
		112UL,
		113UL,
		114UL,
		115UL);


	if (n < 0 || n > NETLINK_EVENT_MESSAGE_SIZE) {
		pr_info("%s : snprintf error n = %d\n", __func__, n);
		return -1;
	}
	mbraink_netlink_send_msg(netlink_buf);

	return 0;
}

int mbraink_auto_cmdq_event_notify(struct notifier_block *nb, unsigned long event, void *data)
{
	switch (event) {
	case CMDQ_LATENCY_TO_MB:
		mbrain_cmdq_latency_notify((char *)data);
		return NOTIFY_DONE;
	default:
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block cmdq_mb_nb = {
	.notifier_call = mbraink_auto_cmdq_event_notify,
};
#endif

int mbraink_v8668_cmdq_init(void)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_CMDQ_MBRAIN)
	cmdq_mb_register(&cmdq_mb_nb);
#endif

	return ret;
}

int mbraink_v8668_cmdq_deinit(void)
{
	int ret = 0;

#if IS_ENABLED(CONFIG_CMDQ_MBRAIN)
	cmdq_mb_unregister(&cmdq_mb_nb);
#endif

	return ret;
}
