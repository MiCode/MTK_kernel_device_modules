/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */
#include <linux/mutex.h>

#ifdef CONFIG_SUPPORT_SOUTHCHIP_PDPHY
struct extcon_notifier_block {
	struct notifier_block nb;
	struct mtk_extcon_info *extcon;
};
#endif


struct mtk_extcon_info {
	struct device *dev;
	struct extcon_dev *edev;
	struct usb_role_switch *role_sw;
#ifdef CONFIG_SUPPORT_SOUTHCHIP_PDPHY
	struct pinctrl *pinctrl;
	struct pinctrl_state *enable;
	struct pinctrl_state *disable;
#endif
	unsigned int c_role; /* current data role */
	struct workqueue_struct *extcon_wq;
	struct regulator *vbus;
	unsigned int vbus_vol;
	unsigned int vbus_vol_max;
	unsigned int vbus_vol_request;
	unsigned int vbus_cur;
	bool vbus_on;
	struct power_supply *usb_psy;
	struct notifier_block psy_nb;
	struct delayed_work wq_psy;
#if IS_ENABLED(CONFIG_TCPC_CLASS)
#ifdef CONFIG_SUPPORT_SOUTHCHIP_PDPHY
	struct tcpc_device **tcpc_dev;
	struct extcon_notifier_block *tcpc_nb;
#else
	struct tcpc_device *tcpc_dev;
	struct notifier_block tcpc_nb;
#endif
#endif
	bool bypss_typec_sink;
	/* id/vbus gpio */
	struct gpio_desc *id_gpiod;
	struct gpio_desc *vbus_gpiod;
	int id_irq;
	int vbus_irq;
	struct delayed_work wq_detcable;
	unsigned int vbus_limit_cur;
	bool vbus_cur_inlimit;
#ifdef CONFIG_SUPPORT_SOUTHCHIP_PDPHY
	struct mutex mutex;
	u32 nr_port;
#endif
	int vdd_boost_en_gpio_a;
	int vdd_boost_en_gpio_b;
	unsigned int data_port;
	unsigned int cur_port;
	unsigned int enable_port;
	unsigned int delay_mode;
	bool revchg_vbus_v2;
	int attached_now;
};

struct usb_role_info {
	struct mtk_extcon_info *extcon;
	struct delayed_work dwork;
	unsigned int d_role; /* desire data role */
};


#define PORT0_ENABLE	1 /* port0 enable */
#define PORT1_ENABLE	2 /* port1 enable */

int mtk_usb_extcon_set_role(struct mtk_extcon_info *extcon,
						unsigned int role);

enum {
	DUAL_PROP_MODE_UFP = 0,
	DUAL_PROP_MODE_DFP,
	DUAL_PROP_MODE_NONE,
};

enum {
	DUAL_PROP_PR_SRC = 0,
	DUAL_PROP_PR_SNK,
	DUAL_PROP_PR_NONE,
};

int usb_port_use(void);

#define USB_GPIO_DEB_US	(2000)
#define USB_GPIO_IRQ_FLAG   \
	(IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT)
