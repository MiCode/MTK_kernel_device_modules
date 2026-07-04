#ifndef __CHARGER_MANAGER_H__
#define __CHARGER_MANAGER_H__

enum main_chg_notifier_events {
        MAIN_CHG_NONE_EVENT = 0,
        MAIN_CHG_USB_PLUGIN_EVENT = 1,
        MAIN_CHG_ENABLE_EVENT = 2,
};

extern struct srcu_notifier_head main_chg_notifier;
extern int main_chg_reg_notifier(struct notifier_block *nb);
extern int main_chg_unreg_notifier(struct notifier_block *nb);
extern int main_chg_notifier_call_chain(unsigned long event, int val);

#endif