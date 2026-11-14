/* SPDX-License-Identifier: GPL-2.0 */
/**
 * Copyright (c) 2025 Huaqin Technology(Shanghai) Co., Ltd.
 */

#include <linux/module.h>
#include "hq_notify.h"

// subpmic/buck charger notifer intertface to other module
static BLOCKING_NOTIFIER_HEAD(charger_notifier);
static BLOCKING_NOTIFIER_HEAD(cp_notifier);
static ATOMIC_NOTIFIER_HEAD(fg_notifier);
static BLOCKING_NOTIFIER_HEAD(chargermanager_notifier);

// chargepump notifer intertface to other module
// TODO: add chargepump notifier
//static BLOCKING_NOTIFIER_HEAD(chargepump_notifiier);

/* charge ic -> other module */
int hq_charger_notifier_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&charger_notifier, nb);
}
EXPORT_SYMBOL_GPL(hq_charger_notifier_register);

int hq_charger_notifier_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&charger_notifier, nb);
}
EXPORT_SYMBOL_GPL(hq_charger_notifier_unregister);

int hq_charger_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&charger_notifier, val, v);
}
EXPORT_SYMBOL_GPL(hq_charger_notifier_call_chain);

/* charger pump -> other module */
int hq_cp_notifier_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&cp_notifier, nb);
}
EXPORT_SYMBOL_GPL(hq_cp_notifier_register);

int hq_cp_notifier_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&cp_notifier, nb);
}
EXPORT_SYMBOL_GPL(hq_cp_notifier_unregister);

int hq_cp_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&cp_notifier, val, v);
}
EXPORT_SYMBOL_GPL(hq_cp_notifier_call_chain);

/* charger manager -> other module */
int hq_chargermanager_notifier_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&chargermanager_notifier, nb);
}
EXPORT_SYMBOL_GPL(hq_chargermanager_notifier_register);

int hq_chargermanager_notifier_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&chargermanager_notifier, nb);
}
EXPORT_SYMBOL_GPL(hq_chargermanager_notifier_unregister);

int hq_chargermanager_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&chargermanager_notifier, val, v);
}
EXPORT_SYMBOL_GPL(hq_chargermanager_notifier_call_chain);

/* fuel gauge -> other module */
int hq_fg_notifier_register(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&fg_notifier, nb);
}
EXPORT_SYMBOL_GPL(hq_fg_notifier_register);

int hq_fg_notifier_unregister(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister(&fg_notifier, nb);
}
EXPORT_SYMBOL_GPL(hq_fg_notifier_unregister);

int hq_fg_notifier_call_chain(unsigned long val, void *v)
{
	return atomic_notifier_call_chain(&fg_notifier, val, v);
}
EXPORT_SYMBOL_GPL(hq_fg_notifier_call_chain);

MODULE_AUTHOR("pengyuzhe@huaqin.com");
MODULE_DESCRIPTION("hq notify common driver");
MODULE_LICENSE("GPL v2");
