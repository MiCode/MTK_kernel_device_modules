/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": %s: " fmt, __func__
#define DEBUG
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/pinctrl/consumer.h>

#include "flashlight-core.h"
#include "flashlight-dt.h"

/* define device tree */
/* TODO: modify temp device tree name */
#ifndef CUST_GPIO_DTNAME
#define CUST_GPIO_DTNAME "mediatek,flashlights_ocp8132"
#endif

/* TODO: define driver name */
#define CUST_NAME "flashlights_ocp8132"

/* define registers */
/* TODO: define register */

/* define mutex and work queue */
static DEFINE_MUTEX(cust_mutex);
static struct work_struct cust_work;

/* define pinctrl */
/* TODO: define pinctrl */
#define CUST_GPIO_FLASH 32
#define CUST_GPIO_TORCH 16
#define CUST_GPIO_TIMEOUT 400 /* ms */
#define CUST_CHANNEL_CH1 0
#define CUST_CHANNEL_CH2 1
#define CUST_PINCTRL_PINSTATE_OFF 0
#define CUST_PINCTRL_PINSTATE_ON 1
#define CUST_PINCTRL_PINSTATE_LOW 1
#define CUST_PINCTRL_PINSTATE_HIGH 2
#define CUST_PINCTRL_STATE_EN_HIGH "hw_ch0_high"
#define CUST_PINCTRL_STATE_EN_LOW  "hw_ch0_low"
#define CUST_PINCTRL_STATE_STROBE_HIGH "hw_ch1_high"
#define CUST_PINCTRL_STATE_STROBE_LOW  "hw_ch1_low"
static struct pinctrl *cust_pinctrl;
static struct pinctrl_state *cust_en_high;
static struct pinctrl_state *cust_en_low;
static struct pinctrl_state *cust_strobe_high;
static struct pinctrl_state *cust_strobe_low;

/* define usage count */
static int use_count;

/* platform data */
struct cust_platform_data {
	int channel_num;
	struct flashlight_device_id *dev_id;
};

static int cust_gpio_level_ch1;
static int cust_gpio_level_ch2;

static int cust_is_charger_ready(void)
{
    return FLASHLIGHT_CHARGER_READY;
}

static int cust_gpio_is_torch(int level)
{
	if ((CUST_PINCTRL_PINSTATE_OFF < level) && (level <= CUST_GPIO_TORCH))
		return 1;

	return 0;
}

static int cust_gpio_is_strobe(int level)
{
	if ((CUST_GPIO_TORCH < level) && (level <= CUST_GPIO_FLASH))
		return 1;

	return 0;
}

static int cust_gpio_verify_level(int level)
{
	if (level < 0)
		level = 0;
	else if (level >= CUST_GPIO_FLASH)
		level = CUST_GPIO_FLASH;

	return level;
}

/******************************************************************************
 * Pinctrl configuration
 *****************************************************************************/
static int cust_pinctrl_init(struct platform_device *pdev)
{
	int ret = 0;

	/* get pinctrl */
	cust_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(cust_pinctrl)) {
		pr_err("Failed to get flashlight pinctrl.\n");
		ret = PTR_ERR(cust_pinctrl);
		return ret;
	}

	/* Flashlight EN pin initialization */
	cust_en_high = pinctrl_lookup_state(
			cust_pinctrl, CUST_PINCTRL_STATE_EN_HIGH);
	if (IS_ERR(cust_en_high)) {
		pr_err("Failed to init (%s)\n", CUST_PINCTRL_STATE_EN_HIGH);
		ret = PTR_ERR(cust_en_high);
	}
	cust_en_low = pinctrl_lookup_state(
			cust_pinctrl, CUST_PINCTRL_STATE_EN_LOW);
	if (IS_ERR(cust_en_low)) {
		pr_err("Failed to init (%s)\n", CUST_PINCTRL_STATE_EN_LOW);
		ret = PTR_ERR(cust_en_low);
	}
	/* Flashlight STROBE pin initialization */
	cust_strobe_high = pinctrl_lookup_state(
			cust_pinctrl, CUST_PINCTRL_STATE_STROBE_HIGH);
	if (IS_ERR(cust_strobe_high)) {
		pr_err("Failed to init (%s)\n", CUST_PINCTRL_STATE_STROBE_HIGH);
		ret = PTR_ERR(cust_strobe_high);
	}
	cust_strobe_low = pinctrl_lookup_state(
			cust_pinctrl, CUST_PINCTRL_STATE_STROBE_LOW);
	if (IS_ERR(cust_strobe_low)) {
		pr_err("Failed to init (%s)\n", CUST_PINCTRL_STATE_STROBE_LOW);
		ret = PTR_ERR(cust_strobe_low);
	}

	return ret;
}

static int cust_pinctrl_set(int pin, int state)
{
	int ret = 0;

	if (IS_ERR(cust_pinctrl)) {
		pr_err("pinctrl is not available\n");
		return -1;
	}

	switch (pin) {
	case CUST_CHANNEL_CH1:
		if (state == CUST_PINCTRL_PINSTATE_OFF &&
				!IS_ERR(cust_en_low) && !IS_ERR(cust_strobe_low))
		{
			pinctrl_select_state(cust_pinctrl, cust_en_low);
			pinctrl_select_state(cust_pinctrl, cust_strobe_low);
		} else if (state == CUST_PINCTRL_PINSTATE_ON) {
			if (cust_gpio_is_torch(cust_gpio_level_ch1) &&
				!IS_ERR(cust_en_high) && !IS_ERR(cust_strobe_low))
			{
				pinctrl_select_state(cust_pinctrl, cust_en_high);
				pinctrl_select_state(cust_pinctrl, cust_strobe_low);
				pr_debug("torch on\n");
			} else if (cust_gpio_is_strobe(cust_gpio_level_ch1) &&
				!IS_ERR(cust_en_low) && !IS_ERR(cust_strobe_high))
			{
				pinctrl_select_state(cust_pinctrl, cust_en_high);
				pinctrl_select_state(cust_pinctrl, cust_strobe_high);
				pr_debug("strobe on\n");
			} else {
				pr_err("level(%d) or pin error \n", cust_gpio_level_ch1);
 			}
 		} else {
			pr_err("set err, pin(%d) state(%d)\n", pin, state);
 		}
		break;
	default:
		pr_err("default set err, pin(%d) state(%d)\n", pin, state);
		break;
	}
	pr_info("pin(%d) state(%d)\n", pin, state);

	return ret;
}


/******************************************************************************
 * cust operations
 *****************************************************************************/
/* flashlight enable function */
static int cust_enable(int channel, int enable)
{
	int pin = channel, state = enable;

	/* TODO: wrap enable function */

	return cust_pinctrl_set(pin, state);
}

/* flashlight disable function */
static int cust_disable(int channel, int enable)
{
	int pin = channel, state = enable;

	/* TODO: wrap disable function */

	return cust_pinctrl_set(pin, state);
}

/* set flashlight level */
static int cust_set_level(int channel, int level)
{
	int pin = channel, state = level;

	/* TODO: wrap set level function */

	if (pin == CUST_CHANNEL_CH1) {
		cust_gpio_level_ch1 = state;
	} else if (pin == CUST_CHANNEL_CH2) {
		cust_gpio_level_ch2 = state;
	}
	return 0;
}

/* set flashlight scenario */
static int cust_gpio_set_scenario(int scenario)
{
	return 0;
}

/* flashlight init */
static int cust_init(void)
{
	int pin = 0, state = 0;

	/* TODO: wrap init function */

	return cust_pinctrl_set(pin, state);
}

/* flashlight uninit */
static int cust_uninit(void)
{
	int pin = 0, state = 0;

	/* TODO: wrap uninit function */

	return cust_pinctrl_set(pin, state);
}

/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static struct hrtimer cust_timer;
static unsigned int cust_timeout_ms;

static void cust_work_disable(struct work_struct *data)
{
	pr_debug("work queue callback\n");
	cust_disable(0, 0);
}

static enum hrtimer_restart cust_timer_func(struct hrtimer *timer)
{
	schedule_work(&cust_work);
	return HRTIMER_NORESTART;
}


/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
static int cust_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;
	ktime_t ktime;
	unsigned int s;
	unsigned int ns;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		pr_debug("FLASH_IOC_SET_TIME_OUT_TIME_MS(%d): %d\n",
				channel, (int)fl_arg->arg);
		cust_timeout_ms = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		pr_debug("FLASH_IOC_SET_DUTY(%d): %d\n",
				channel, (int)fl_arg->arg);
		cust_set_level(channel, fl_arg->arg);
		break;

	case FLASH_IOC_SET_SCENARIO:
		pr_debug("FLASH_IOC_SET_SCENARIO(%d): %d\n",
				channel, (int)fl_arg->arg);
		cust_gpio_set_scenario(fl_arg->arg);
		break;

	case FLASH_IOC_SET_ONOFF:
		pr_debug("FLASH_IOC_SET_ONOFF(%d): %d\n",
				channel, (int)fl_arg->arg);
		if (fl_arg->arg == 1) {
			if (cust_timeout_ms) {
				s = cust_timeout_ms / 1000;
				ns = cust_timeout_ms % 1000 * 1000000;
				ktime = ktime_set(s, ns);
				hrtimer_start(&cust_timer, ktime,
						HRTIMER_MODE_REL);
			}
			cust_enable(channel, fl_arg->arg);
		} else {
			cust_disable(channel, fl_arg->arg);
			hrtimer_cancel(&cust_timer);
		}
		break;

	case FLASH_IOC_IS_CHARGER_READY:
		pr_debug("FLASH_IOC_IS_CHARGER_READY(%d)\n", channel);
		fl_arg->arg = cust_is_charger_ready();
		pr_debug("FLASH_IOC_IS_CHARGER_READY(%d)\n", fl_arg->arg);
		break;

	case FLASH_IOC_GET_DUTY_NUMBER:
		pr_debug("FLASH_IOC_GET_DUTY_NUMBER(%d)\n", channel);
		fl_arg->arg = CUST_GPIO_FLASH;
		break;

	case FLASH_IOC_GET_MAX_TORCH_DUTY:
		pr_debug("FLASH_IOC_GET_MAX_TORCH_DUTY(%d)\n", channel);
		fl_arg->arg = CUST_GPIO_TORCH;
		break;

	case FLASH_IOC_GET_DUTY_CURRENT:
		fl_arg->arg = cust_gpio_verify_level(fl_arg->arg);
		pr_debug("FLASH_IOC_GET_DUTY_CURRENT(%d): %d\n",
				channel, (int)fl_arg->arg);
		break;

	case FLASH_IOC_GET_HW_TIMEOUT:
		pr_debug("FLASH_IOC_GET_HW_TIMEOUT(%d)\n", channel);
		fl_arg->arg = CUST_GPIO_TIMEOUT;
		break;

	default:
		/*
		pr_info("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		*/
		return -ENOTTY;
	}

	return 0;
}

static int cust_open(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int cust_release(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int cust_set_driver(int set)
{
	int ret = 0;

	/* set chip and usage count */
	mutex_lock(&cust_mutex);
	if (set) {
		if (!use_count)
			ret = cust_init();
		use_count++;
		pr_info("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = cust_uninit();
		if (use_count < 0)
			use_count = 0;
		pr_info("Unset driver: %d\n", use_count);
	}
	mutex_unlock(&cust_mutex);

	return ret;
}

static ssize_t cust_strobe_store(struct flashlight_arg arg)
{
	cust_set_driver(1);
	cust_set_level(arg.channel, arg.level);
	cust_timeout_ms = 0;
	cust_enable(arg.channel, arg.level);
	msleep(arg.dur);
	cust_disable(arg.channel, arg.level);
	cust_set_driver(0);

	return 0;
}

static struct flashlight_operations cust_ops = {
	cust_open,
	cust_release,
	cust_ioctl,
	cust_strobe_store,
	cust_set_driver
};


/******************************************************************************
 * Platform device and driver
 *****************************************************************************/
static int cust_chip_init(void)
{
	/* NOTE: Chip initialication move to "set driver" for power saving.
	 * cust_init();
	 */

	return 0;
}

//static int flash_is_use = 0;
unsigned char last_val = 0;
static ssize_t mt_cust_gpio_torch_brightness_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	unsigned char reg_val = 0;
	reg_val  = last_val;

	return snprintf(buf, 10, "%d\n", reg_val);

}

static ssize_t mt_cust_gpio_torch_brightness_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	int ret = 0;
	unsigned long value = 0;

	ret = kstrtoul(buf, 10, &value);
	if (ret < 0)
		return ret;
	pr_info("%s value is %lu,length is %lu\n",__func__,value,strlen(buf));

	value = cust_gpio_verify_level(value);
	last_val = value;

    if (value > 0) {
		cust_set_level(CUST_CHANNEL_CH1, value);
		cust_pinctrl_set(CUST_CHANNEL_CH1, CUST_PINCTRL_PINSTATE_LOW);
    } else {
		cust_pinctrl_set(CUST_CHANNEL_CH1, CUST_PINCTRL_PINSTATE_OFF);
    }

	if(strlen(buf) >= 2)
		count = strlen(buf) - 1;
	else
		count = 1;

	return count;
}

static DEVICE_ATTR(torch_brightness, 0664, mt_cust_gpio_torch_brightness_show, mt_cust_gpio_torch_brightness_store);
//static DEVICE_ATTR(flash_brightness, 0664, mt6360_flash_brightness_show, mt6360_flash_brightness_store);

static struct attribute *cust_gpio_attributes[] = {
	&dev_attr_torch_brightness.attr,
//	&dev_attr_flash_brightness.attr,
	NULL
};

static struct attribute_group cust_gpio_attribute_group = {
	.attrs = cust_gpio_attributes
};

static int cust_parse_dt(struct device *dev,
		struct cust_platform_data *pdata)
{
	struct device_node *np, *cnp;
	u32 decouple = 0;
	int i = 0;

	if (!dev || !dev->of_node || !pdata)
		return -ENODEV;

	np = dev->of_node;

	pdata->channel_num = of_get_child_count(np);
	if (!pdata->channel_num) {
		pr_info("Parse no dt, node.\n");
		return 0;
	}
	pr_info("Channel number(%d).\n", pdata->channel_num);

	if (of_property_read_u32(np, "decouple", &decouple))
		pr_info("Parse no dt, decouple.\n");

	pdata->dev_id = devm_kzalloc(dev,
			pdata->channel_num *
			sizeof(struct flashlight_device_id),
			GFP_KERNEL);
	if (!pdata->dev_id)
		return -ENOMEM;

	for_each_child_of_node(np, cnp) {
		if (of_property_read_u32(cnp, "type", &pdata->dev_id[i].type))
			goto err_node_put;
		if (of_property_read_u32(cnp, "ct", &pdata->dev_id[i].ct))
			goto err_node_put;
		if (of_property_read_u32(cnp, "part", &pdata->dev_id[i].part))
			goto err_node_put;
		snprintf(pdata->dev_id[i].name, FLASHLIGHT_NAME_SIZE,
				CUST_NAME);
		pdata->dev_id[i].channel = i;
		pdata->dev_id[i].decouple = decouple;

		pr_info("Parse dt (type,ct,part,name,channel,decouple)=(%d,%d,%d,%s,%d,%d).\n",
				pdata->dev_id[i].type, pdata->dev_id[i].ct,
				pdata->dev_id[i].part, pdata->dev_id[i].name,
				pdata->dev_id[i].channel,
				pdata->dev_id[i].decouple);
		i++;
	}

	return 0;

err_node_put:
	of_node_put(cnp);
	return -EINVAL;
}

static int cust_probe(struct platform_device *pdev)
{
	struct cust_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int err;
	int i;

	pr_info("CHYL Probe start.\n");

	/* init pinctrl */
	if (cust_pinctrl_init(pdev)) {
		pr_err("Failed to init pinctrl.\n");
		err = -EFAULT;
		goto err;
	}

	/* init platform data */
	if (!pdata) {
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			err = -ENOMEM;
			goto err;
		}
		pdev->dev.platform_data = pdata;
		err = cust_parse_dt(&pdev->dev, pdata);
		if (err)
			goto err;
	}

	/* init work queue */
	INIT_WORK(&cust_work, cust_work_disable);

	/* init timer */
	hrtimer_init(&cust_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	cust_timer.function = cust_timer_func;
	cust_timeout_ms = 100;

	/* init chip hw */
	cust_chip_init();

	/* clear usage count */
	use_count = 0;

	/* register flashlight device */
	if (pdata->channel_num) {
		for (i = 0; i < pdata->channel_num; i++)
			if (flashlight_dev_register_by_device_id(
						&pdata->dev_id[i],
						&cust_ops)) {
				err = -EFAULT;
				goto err;
			}
	} else {
		if (flashlight_dev_register(CUST_NAME, &cust_ops)) {
			err = -EFAULT;
			goto err;
		}
	}

	err = sysfs_create_group(&pdev->dev.kobj, &cust_gpio_attribute_group);
	if (err < 0) {
		dev_info(&pdev->dev, "%s error creating sysfs attr files\n",
			 __func__);
	}

	pr_info("CHYL Probe done.\n");

	return 0;
err:
	return err;
}

static int cust_remove(struct platform_device *pdev)
{
	struct cust_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int i;

	pr_debug("Remove start.\n");

	pdev->dev.platform_data = NULL;

	/* unregister flashlight device */
	if (pdata && pdata->channel_num)
		for (i = 0; i < pdata->channel_num; i++)
			flashlight_dev_unregister_by_device_id(
					&pdata->dev_id[i]);
	else
		flashlight_dev_unregister(CUST_NAME);

	/* flush work queue */
	flush_work(&cust_work);

	pr_debug("Remove done.\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id cust_gpio_of_match[] = {
	{.compatible = CUST_GPIO_DTNAME},
	{},
};
MODULE_DEVICE_TABLE(of, cust_gpio_of_match);
#else
static struct platform_device cust_gpio_platform_device[] = {
	{
		.name = CUST_NAME,
		.id = 0,
		.dev = {}
	},
	{}
};
MODULE_DEVICE_TABLE(platform, cust_gpio_platform_device);
#endif

static struct platform_driver cust_platform_driver = {
	.probe = cust_probe,
	.remove = cust_remove,
	.driver = {
		.name = CUST_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = cust_gpio_of_match,
#endif
	},
};

static int flashlight_cust_init(void)
{
	int ret;

	printk("CHYL Init start.\n");

#ifndef CONFIG_OF
	ret = platform_device_register(&cust_gpio_platform_device);
	if (ret) {
		pr_err("Failed to register platform device\n");
		return ret;
	}
#endif

	ret = platform_driver_register(&cust_platform_driver);
	if (ret) {
		pr_err("Failed to register platform driver\n");
		return ret;
	}

	pr_info("Init done.\n");

	return 0;
}

static void  flashlight_cust_exit(void)
{
	pr_debug("Exit start.\n");

	platform_driver_unregister(&cust_platform_driver);

	pr_debug("Exit done.\n");
}
void flashlights_ocp8132_init_robe(void)
{
    printk("robe_debug: %s run. begin.\n", __func__);
    flashlight_cust_init();
    printk("robe_debug: %s run. end.\n", __func__);
    return;
}
EXPORT_SYMBOL(flashlights_ocp8132_init_robe);
void flashlights_ocp8132_exit_robe(void)
{
    printk("robe_debug: %s run. begin.\n", __func__);
    flashlight_cust_exit();
    printk("robe_debug: %s run. end.\n", __func__);
    return;
}
EXPORT_SYMBOL(flashlights_ocp8132_exit_robe);
