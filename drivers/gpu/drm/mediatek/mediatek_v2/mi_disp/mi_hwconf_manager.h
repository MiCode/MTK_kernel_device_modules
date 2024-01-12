/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2020 XiaoMi, Inc. All rights reserved.
 */

#ifndef _MI_HWCONF_MANAGER_H_
#define _MI_HWCONF_MANAGER_H_

static inline int register_hw_component_info(char *component_name) {return 0;}
static inline int add_hw_component_info(char *component_name, char *key, char *value) {return 0;}
static inline int unregister_hw_component_info(char *component_name) {return 0;}
static inline int register_hw_monitor_info(char *component_name) {return 0;}
static inline int add_hw_monitor_info(char *component_name, char *mon_key, char *mon_value) {return 0;}
static inline int update_hw_monitor_info(char *component_name, char *mon_key, char *mon_value) {return 0;}
static inline int hw_monitor_notifier_register(struct notifier_block *nb) {return 0;}
static inline int hw_monitor_notifier_unregister(struct notifier_block *nb) {return 0;}
static inline int unregister_hw_monitor_info(char *component_name) {return 0;}
int hwconf_init(void) { return 0; }
void hwconf_exit(void) {}

#endif /* _MI_HWCONF_MANAGER_H_ */
