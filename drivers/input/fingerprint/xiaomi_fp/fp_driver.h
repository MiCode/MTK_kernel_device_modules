/*
 * Copyright (C) 2022-2023, Xiaomi, Inc.
 * All Rights Reserved.
 */
#ifndef FP_DRIVER_H
#define FP_DRIVER_H

/**************************debug******************************/
#define DEBUG
#define pr_fmt(fmt) "xiaomi-fp %s: " fmt, __func__
#define FUNC_ENTRY() pr_debug(" enter\n")
#define FUNC_EXIT() pr_debug(" exit\n")

#include <linux/types.h>
#include <linux/netlink.h>
#include <linux/cdev.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/fb.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/init.h>
#include <asm/atomic.h>
#include <linux/timer.h>
#include <linux/input.h>
#include <linux/version.h>

/*#define XIAOMI_DRM_INTERFACE_WA*/
#include <linux/notifier.h>

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#include <linux/clk.h>

#include <net/sock.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>


#include <linux/of_address.h>

#include  <linux/regulator/consumer.h>

#ifdef XIAOMI_DRM_INTERFACE_WA
#include <drm/drm_bridge.h>
#include <drm/drm_notifier_mi.h>
#endif
#include <linux/poll.h>


#define FP_KEY_INPUT_HOME		KEY_SELECT
#define FP_KEY_INPUT_MENU		KEY_MENU
#define FP_KEY_INPUT_BACK		KEY_BACK
#define FP_KEY_INPUT_POWER		KEY_POWER
#define FP_KEY_INPUT_CAMERA		KEY_CAMERA
#define FP_KEY_INPUT_KPENTER            KEY_KPENTER


typedef enum fp_key_event {
	FP_KEY_NONE = 0,
	FP_KEY_HOME,
	FP_KEY_POWER,
	FP_KEY_MENU,
	FP_KEY_BACK,
	FP_KEY_CAMERA,
} fp_key_event_t;

struct fp_key {
	enum fp_key_event key;
	uint32_t value;		/* key down = 1, key up = 0 */
};

struct fp_key_map {
	unsigned int type;
	unsigned int code;
};

enum fp_netlink_cmd {
	FP_NETLINK_TEST = 0,
	FP_NETLINK_IRQ = 1,
	FP_NETLINK_SCREEN_OFF,
	FP_NETLINK_SCREEN_ON
};

struct fp_ioc_chip_info {
	u8 vendor_id;
	u8 mode;
	u8 operation;
	u8 reserved[5];
};
/**********************IO Magic**********************/
#define FP_IOC_MAGIC	'g'
#define FP_IOC_INIT			_IOR(FP_IOC_MAGIC, 0, u8)
#define FP_IOC_EXIT			_IO(FP_IOC_MAGIC, 1)
#define FP_IOC_RESET			_IO(FP_IOC_MAGIC, 2)

#define FP_IOC_ENABLE_IRQ		_IO(FP_IOC_MAGIC, 3)
#define FP_IOC_DISABLE_IRQ		_IO(FP_IOC_MAGIC, 4)

#define FP_IOC_ENABLE_SPI_CLK           _IOW(FP_IOC_MAGIC, 5, uint32_t)
#define FP_IOC_DISABLE_SPI_CLK		_IO(FP_IOC_MAGIC, 6)

#define FP_IOC_ENABLE_POWER		_IO(FP_IOC_MAGIC, 7)
#define FP_IOC_DISABLE_POWER		_IO(FP_IOC_MAGIC, 8)

#define FP_IOC_INPUT_KEY_EVENT		_IOW(FP_IOC_MAGIC, 9, struct fp_key)

/* fp sensor has change to sleep mode while screen off */
#define FP_IOC_ENTER_SLEEP_MODE		_IO(FP_IOC_MAGIC, 10)
#define FP_IOC_GET_FW_INFO		_IOR(FP_IOC_MAGIC, 11, u8)
#define FP_IOC_REMOVE			_IO(FP_IOC_MAGIC, 12)
#define FP_IOC_CHIP_INFO		_IOW(FP_IOC_MAGIC, 13, struct fp_ioc_chip_info)

#define FP_IOC_MAXNR    		19	/* THIS MACRO IS NOT USED NOW... */

#define DRIVER_COMPATIBLE "xiaomi,xiaomi-fp"
struct fp_device {
	dev_t devt;
	struct cdev cdev;
	struct device *device;
	struct class *class;
	struct list_head device_entry;
	struct platform_device *driver_device;

	struct input_dev *input;
	struct notifier_block notifier;
	char device_available;
	char fb_black;
	char wait_finger_down;
	u_int32_t  netlink_num;
	int  netlink_enabled;
	int  poll_have_data;
	wait_queue_head_t wait_queue;
/**************************Pin******************************/
	signed irq_gpio;
	int irq_num;
	int irq_enabled;
	struct regulator *vreg;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_eint_default;
	struct pinctrl_state *pins_eint_pulldown;
	struct pinctrl_state *pins_spiio_spi_mode;
	struct pinctrl_state *pins_spiio_gpio_mode;
	struct pinctrl_state *pins_reset_high;
	struct pinctrl_state *pins_reset_low;
/**************************Pin******************************/
};

/**********************function defination**********************/

/*netlink function*/

void netlink_send(struct fp_device *fp_dev, const int command);
void netlink_recv(struct sk_buff *__skb);
int netlink_init(struct fp_device *fp_dev);
int netlink_destroy(struct fp_device *fp_dev);

/*fp_platform function refererce */
int  fp_parse_dts(struct fp_device *fp_dev);
int  fp_power_config(struct fp_device *fp_dev);
void fp_power_on(struct fp_device *fp_dev);
void fp_power_off(struct fp_device *fp_dev);
void fp_hw_reset(struct fp_device *fp_dev, u8 delay);
void fp_enable_irq(struct fp_device *fp_dev);
void fp_disable_irq(struct fp_device *fp_dev);
void fp_kernel_key_input(struct fp_device *fp_dev, struct fp_key *fp_key);

#endif /* FP_DRIVER_H */
