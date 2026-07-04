/*
 *  linux/drivers/video/fb_notify.c
 *
 *  Copyright (C) 2006 Antonino Daplas <adaplas@pol.net>
 *
 *	2001 - Documented with DocBook
 *	- Brad Douglas <brad@neruo.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */
#include <linux/fb.h>
#include <linux/notifier.h>
#include <linux/export.h>
#include "tpd_notify.h"

static RAW_NOTIFIER_HEAD(tpd_charger_detect_notifier_list);

/**
 *      fb_register_client - register a client notifier
 *      @nb: notifier block to callback on events
 */
int tpd_charger_detect_register_client(struct notifier_block *nb)
{
        return raw_notifier_chain_register(&tpd_charger_detect_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(tpd_charger_detect_register_client);

/**
 *      fb_unregister_client - unregister a client notifier
 *      @nb: notifier block to callback on events
 */
int tpd_charger_detect_unregister_client(struct notifier_block *nb)
{
        return raw_notifier_chain_unregister(&tpd_charger_detect_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(tpd_charger_detect_unregister_client);

int tpd_charger_detect_notifier_call_chain(unsigned long val, void *v)
{
        return raw_notifier_call_chain(&tpd_charger_detect_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(tpd_charger_detect_notifier_call_chain);


static BLOCKING_NOTIFIER_HEAD(tpd_notifier_list);

/**
 *	fb_register_client - register a client notifier
 *	@nb: notifier block to callback on events
 */
int tpd_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&tpd_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(tpd_register_client);

/**
 *	fb_unregister_client - unregister a client notifier
 *	@nb: notifier block to callback on events
 */
int tpd_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&tpd_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(tpd_unregister_client);

/**
 * fb_notifier_call_chain - notify clients of fb_events
 *
 */
int tpd_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&tpd_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(tpd_notifier_call_chain);


struct notifier_block psenable_nb;
int ps_state;

int ps_enable_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{

	pr_info("[virtual_prox] %s: level = %d\n", __func__, (int)event);
	ps_state = (int)event;

	return 0;
}
EXPORT_SYMBOL_GPL(ps_enable_notifier_callback);

int tpd_register_psenable_callback(void)
{
	pr_info("[virtual_prox] %s\n", __func__);

	memset(&psenable_nb, 0, sizeof(psenable_nb));
	psenable_nb.notifier_call = ps_enable_notifier_callback;

	return ps_enable_register_notifier(&psenable_nb);
}
EXPORT_SYMBOL_GPL(tpd_register_psenable_callback);

extern int (*ps_tpd)(struct notifier_block *nb);
extern int ps_register_recive_touch_event_callback(void);

static int __init tpd_notify_init(void)
{
	ps_tpd = tpd_register_client;

	if (ps_register_recive_touch_event_callback()) {
		pr_err("virtual_prox %s fail ret\n", __func__);
		return -1;
	}

	pr_info("%s enter\n", __func__);
	return 0;
}

static void __exit tpd_notify_exit(void)
{
	ps_tpd = NULL;
	pr_info("%s exit\n", __func__);
}

module_init(tpd_notify_init);
module_exit(tpd_notify_exit);
MODULE_LICENSE("GPL");
