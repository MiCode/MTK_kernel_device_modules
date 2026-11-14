// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": %s: " fmt, __func__

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
#include <linux/slab.h>
#if 0
#include <linux/pinctrl/consumer.h>
#endif
#include <linux/kobject.h>
#include <linux/sysfs.h>



#include "flashlight-core.h"
#include "flashlight-dt.h"
#include "hq_led_class.h"

/* device tree should be defined in flashlight-dt.h */
#ifndef mt6789_DTNAME
#define mt6789_DTNAME "mediatek,flashlights_mt6789"
#endif

#define mt6789_NAME "flashlights-mt6789"

/* define registers */
#define mt6789_REG_SILICON_REVISION (0x00)

#define mt6789_REG_FLASH_FEATURE      (0x08)
#define mt6789_INDUCTOR_CURRENT_LIMIT (0x40)
#define mt6789_FLASH_RAMP_TIME        (0x00)
#define mt6789_FLASH_TIMEOUT          (0x07)

#define mt6789_TORCH_CURRENT_CONTROL (0x04)

#define mt6789_REG_ENABLE (0x01)
#define mt6789_ENABLE_STANDBY (0x00)
#define mt6789_ENABLE_TORCH (0x02)
#define mt6789_ENABLE_FLASH (0x03)

#define mt6789_REG_FLAG (0x0B)

/* define level */
#define mt6789_LEVEL_NUM 26
#define mt6789_LEVEL_TORCH 14
#define mt6789_HW_TIMEOUT 800 /* ms */
/*N6 code for HQ-304409 by xiexinli at 20230712 start*/
#define mt6789_FLASH_CURRENT 1000   /* ma */
#define mt6789_TORCH_CURRENT 110   /* ma */
#define mt6789_LONGEXPOSURE_CURRENT 400   /* ma */
#define mt6789_LONGEXPOSURE_DUTY 12
#define mt6789_TORCH_DUTY 6
/*N6 code for HQ-304409 by xiexinli at 20230711 end*/
#if 0
#define mt6789_PINCTRL_PINSTATE_LOW 0
#define mt6789_PINCTRL_PINSTATE_HIGH 1
#define mt6789_PINCTRL_STATE_HW_EN_HIGH "hwen_high"
#define mt6789_PINCTRL_STATE_HW_EN_LOW  "hwen_low"
static struct pinctrl *mt6789_pinctrl;
static struct pinctrl_state *mt6789_hw_en_high;
static struct pinctrl_state *mt6789_hw_en_low;
#endif
/*N6 code for HQ-307144 by xiexinli at 20230711 start*/
static char node_one_buf[20] = {"0"};
/*N6 code for HQ-307144 by xiexinli at 20230711 end*/
/* define mutex and work queue */
static DEFINE_MUTEX(mt6789_mutex);
static struct work_struct mt6789_work;
static int get_duty;
//static int mt6789_pinctrl_set(int state);
/* mt6789 revision */

/* define usage count */
static int use_count;

/* platform data */
struct mt6789_platform_data {
	int channel_num;
	struct flashlight_device_id *dev_id;
};

struct subpmic_led_dev *g_led_dev = NULL;

/******************************************************************************
 * mt6789 operations
 *****************************************************************************/
static const int mt6789_current[mt6789_LEVEL_NUM] = {
        15,   30,  45, 60, 85, 100, 120,  160,  210,  260,
	 320, 380, 420, 460, 520,  580,  650,  700, 740, 800,
	 830, 880, 910, 970, 1030, 1100
};

static const int mt6370_flash[mt6789_LEVEL_NUM] = {
	  15,   30,  45, 60, 85, 100, 120,  160,  210,  260,
	 320, 380, 420, 460, 520,  580,  650,  700, 740, 800,
	 830, 880, 910, 970, 1030, 1100
};

static const int mt6370_torch[mt6789_LEVEL_TORCH] = {
	   15,   30,  45, 60, 85, 100, 120,  160,  210,  260,
	 320, 380, 420, 500
};

#if 0
static int mt6789_is_torch(int level)
{
	if (level >= mt6789_LEVEL_TORCH)
		return -1;

	return 0;
}
#endif


int mt6789_camera_set_led_flash_curr(enum SUBPMIC_LED_ID id, int ma)
{
	if (!g_led_dev) {
		g_led_dev = subpmic_led_find_dev_by_name("subpmic_led");
		if (g_led_dev) {
			pr_info("%s failed to subpmic led device\n", __func__);
			return -ENODEV;
		}
	}

	return subpmic_camera_set_led_flash_curr(g_led_dev, id, ma);
}


int mt6789_camera_set_led_flash_time(enum SUBPMIC_LED_ID id, int ms)
{
	if (!g_led_dev) {
		g_led_dev = subpmic_led_find_dev_by_name("subpmic_led");
		if (g_led_dev) {
			pr_info("%s failed to subpmic led device\n", __func__);
			return -ENODEV;
		}
	}

	return subpmic_camera_set_led_flash_time(g_led_dev, id, ms);
}


int mt6789_camera_set_led_flash_enable(enum SUBPMIC_LED_ID id, bool en)
{
	if (!g_led_dev) {
		g_led_dev = subpmic_led_find_dev_by_name("subpmic_led");
		if (g_led_dev) {
			pr_info("%s failed to subpmic led device\n", __func__);
			return -ENODEV;
		}
	}

	return subpmic_camera_set_led_flash_enable(g_led_dev, id, en);
}

int mt6789_camera_set_led_torch_curr(enum SUBPMIC_LED_ID id, int ma)
{
	if (!g_led_dev) {
		g_led_dev = subpmic_led_find_dev_by_name("subpmic_led");
		if (g_led_dev) {
			pr_info("%s failed to subpmic led device\n", __func__);
			return -ENODEV;
		}
	}

	return subpmic_camera_set_led_torch_curr(g_led_dev, id, ma);
}

int mt6789_camera_set_led_torch_enable(enum SUBPMIC_LED_ID id, bool en)
{
	if (!g_led_dev) {
		g_led_dev = subpmic_led_find_dev_by_name("subpmic_led");
		if (g_led_dev) {
			pr_info("%s failed to subpmic led device\n", __func__);
			return -ENODEV;
		}
	}

	return subpmic_camera_set_led_torch_enable(g_led_dev, id, en);
}

static int mt6789_verify_level(int level)
{
	if (level < 0)
		level = 0;
	else if (level >= mt6789_LEVEL_NUM)
		level = mt6789_LEVEL_NUM - 1;

	return level;
}

/* flashlight enable function */
static int mt6789_enable(void)
{
	unsigned char reg, val;

	reg = mt6789_REG_ENABLE;
#if 0
	if (!mt6789_is_torch(mt6789_level)) {
		/* torch mode */
		val = mt6789_ENABLE_TORCH;
	} else {
		/* flash mode */
		val = mt6789_ENABLE_FLASH;
	}
#endif
	val = mt6789_ENABLE_TORCH;

	return 0;
}

/* flashlight disable function */
static int mt6789_disable(void)
{
	//unsigned char reg, val;

	//reg = mt6789_REG_ENABLE;
	//val = mt6789_ENABLE_STANDBY;

	//return mt6789_write_reg(mt6789_i2c_client, reg, val);
	return 0;
}

/* set flashlight level */
static int mt6789_set_level(int level)
{
	return 0;
}
static int mt6789_get_flag(void)
{
	return 0;
}

/* flashlight init */
int mt6789_init(void)
{
	return 0;
}

/* flashlight uninit */
int mt6789_uninit(void)
{
	mt6789_disable();

	return 0;
}

/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static struct hrtimer mt6789_timer;
static unsigned int mt6789_timeout_ms;

static void mt6789_work_disable(struct work_struct *data)
{
	pr_info("work queue callback\n");
	mt6789_disable();
}

static enum hrtimer_restart mt6789_timer_func(struct hrtimer *timer)
{
	schedule_work(&mt6789_work);
	return HRTIMER_NORESTART;
}


/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
static int mt6789_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;
  	int duty = 0;
	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	pr_info("[xxdd_cmd1] [%s][%d] cmd = [%d]",__func__,__LINE__,_IOC_NR(cmd));

	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		pr_info("FLASH_IOC_SET_TIME_OUT_TIME_MS(%d): %d\n",
				channel, (int)fl_arg->arg);
		mt6789_timeout_ms = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		pr_info("FLASH_IOC_SET_DUTY(%d): %d\n",
				channel, (int)fl_arg->arg);
		mt6789_set_level(fl_arg->arg);
		mutex_lock(&mt6789_mutex);
		get_duty=fl_arg->arg;
		if(get_duty<0)
		{
			get_duty=0;
		}else if(get_duty>25)
		{
			get_duty=25;
		}
            	duty = get_duty;
		mutex_unlock(&mt6789_mutex);
		if(duty==mt6789_LONGEXPOSURE_DUTY || duty==mt6789_TORCH_DUTY )
		{
			mt6789_camera_set_led_torch_curr(SUBPMIC_LED1, mt6370_torch[duty]);
		}else
		{
			mt6789_camera_set_led_flash_curr(SUBPMIC_LED1, mt6370_flash[duty]);
		}
		break;
	case FLASH_IOC_SET_ONOFF:
		pr_info("FLASH_IOC_SET_ONOFF(%d): %d\n",
				channel, (int)fl_arg->arg);
            	mutex_lock(&mt6789_mutex);
		duty = get_duty;
		mutex_unlock(&mt6789_mutex);
		if (fl_arg->arg == 1) {
			if(duty==mt6789_LONGEXPOSURE_DUTY || duty==mt6789_TORCH_DUTY )
			{
				mt6789_camera_set_led_torch_curr(SUBPMIC_LED1, mt6370_torch[duty]);
				mt6789_camera_set_led_torch_enable(SUBPMIC_LED1, true);
			}else
			{
				mt6789_camera_set_led_flash_enable(SUBPMIC_LED1, true);
			}
		} else {
			if(duty==mt6789_LONGEXPOSURE_DUTY || duty==mt6789_TORCH_DUTY )
			{
				mt6789_camera_set_led_torch_enable(SUBPMIC_LED1, false);
			}else
			{
				mt6789_camera_set_led_flash_enable(SUBPMIC_LED1, false);
			}
		}
		break;

	case FLASH_IOC_GET_DUTY_NUMBER:
		pr_info("FLASH_IOC_GET_DUTY_NUMBER(%d)\n", channel);
		fl_arg->arg = mt6789_LEVEL_NUM;
		break;

	case FLASH_IOC_GET_MAX_TORCH_DUTY:
		pr_info("FLASH_IOC_GET_MAX_TORCH_DUTY(%d)\n", channel);
		fl_arg->arg = mt6789_LEVEL_TORCH - 1;
		break;

	case FLASH_IOC_GET_DUTY_CURRENT:
		fl_arg->arg = mt6789_verify_level(fl_arg->arg);
		pr_info("FLASH_IOC_GET_DUTY_CURRENT(%d): %d\n",
				channel, (int)fl_arg->arg);
		fl_arg->arg = mt6789_current[fl_arg->arg];
		break;

	case FLASH_IOC_GET_HW_TIMEOUT:
		pr_info("FLASH_IOC_GET_HW_TIMEOUT(%d)\n", channel);
		fl_arg->arg = mt6789_HW_TIMEOUT;
		break;

	case FLASH_IOC_GET_HW_FAULT:
		pr_info("FLASH_IOC_GET_HW_FAULT(%d)\n", channel);
		fl_arg->arg = mt6789_get_flag();
		break;

	default:
		pr_info("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int mt6789_open(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int mt6789_release(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int mt6789_set_driver(int set)
{
	int ret = 0;

	/* set chip and usage count */
	pr_info("[xxdd] [%s][%d]",__func__,__LINE__);
	mutex_lock(&mt6789_mutex);
	if (set) {
		if (!use_count)
			ret = mt6789_init();
		use_count++;
		pr_info("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = mt6789_uninit();
		if (use_count < 0)
			use_count = 0;
		pr_info("Unset driver: %d\n", use_count);
	}
	mutex_unlock(&mt6789_mutex);
	pr_info("[xxdd] [%s][%d]",__func__,__LINE__);

	return ret;
}

static ssize_t mt6789_strobe_store(struct flashlight_arg arg)
{
	mt6789_set_driver(1);
	mt6789_set_level(arg.level);
	mt6789_timeout_ms = 0;
	mt6789_enable();
	msleep(arg.dur);
	mt6789_disable();
	mt6789_set_driver(0);

	return 0;
}
/*N6 code for HQ-307144 by xiexinli at 20230711 start*/
static ssize_t att_store_mt6789(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	sprintf(node_one_buf, "%s", buf);
	pr_info("echo mt6789_flash debug buf,   %s ", buf);
	if ((strcmp ("0", buf) == 0) || (strcmp ("0\x0a", buf) == 0)) {
		pr_info(" mt6789_flash  0");
		mt6789_camera_set_led_torch_enable(SUBPMIC_LED1, false);
		mt6789_set_driver(0);
	} else {
		pr_info(" mt6789_flash  1");
		mt6789_set_driver(1);
		mt6789_camera_set_led_torch_curr(SUBPMIC_LED1, mt6789_TORCH_CURRENT);
		mt6789_timeout_ms = 0;
		mt6789_camera_set_led_torch_enable(SUBPMIC_LED1, true);
	}
	return count;
}

static ssize_t att_show_mt6789(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%s", node_one_buf);
}

static DEVICE_ATTR(mt6789_flash, 0664, att_show_mt6789, att_store_mt6789);
/*N6 code for HQ-307144 by xiexinli at 20230711 end*/
static struct flashlight_operations mt6789_ops = {
	mt6789_open,
	mt6789_release,
	mt6789_ioctl,
	mt6789_strobe_store,
	mt6789_set_driver
};

static int mt6789_parse_dt(struct device *dev,
		struct mt6789_platform_data *pdata)
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
				mt6789_NAME);
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

/******************************************************************************
 * Platform device and driver
 *****************************************************************************/
static int mt6789_probe(struct platform_device *pdev)
{
	struct mt6789_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int err;
	int i;

	pr_debug("Probe start.\n");
	/* init platform data */
	if (!pdata) {
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			err = -ENOMEM;
			goto err_free;
		}
		pr_info("to devm_kzalloc.\n");
		pdev->dev.platform_data = pdata;
		err = mt6789_parse_dt(&pdev->dev, pdata);
		if (err) {
			pr_info("error  to mt6789_parse_dt.\n");
			goto err_free;
		}
	}

	g_led_dev = subpmic_led_find_dev_by_name("subpmic_led");
	if (g_led_dev)
		pr_info("%s failed to subpmic led device\n", __func__);

	/* init work queue */
	INIT_WORK(&mt6789_work, mt6789_work_disable);
	pr_info("OK  to INIT_WORK.\n");

	/* init timer */
	hrtimer_init(&mt6789_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	mt6789_timer.function = mt6789_timer_func;
	mt6789_timeout_ms = 800;

	/* clear usage count */
	use_count = 0;

	/* register flashlight device */
	if (pdata->channel_num) {
		for (i = 0; i < pdata->channel_num; i++)
			if (flashlight_dev_register_by_device_id(
						&pdata->dev_id[i],
						&mt6789_ops)) {
				err = -EFAULT;
				pr_info("error  to flashlight_dev_register_by_device_id.\n");
				goto err_free;
			}
	} else {
		if (flashlight_dev_register(mt6789_NAME, &mt6789_ops)) {
			err = -EFAULT;
			pr_info("error  to flashlight_dev_register.\n");
			goto err_free;
		}
	}
/*N6 code for HQ-307144 by xiexinli at 20230711 start*/
	err = sysfs_create_file(&pdev->dev.kobj, &dev_attr_mt6789_flash.attr);
/*N6 code for HQ-307144 by xiexinli at 20230711 end*/
	pr_info("mt6789 Probe done.\n");

	return 0;
err_free:
	pr_info("mt6789 Probe failed.\n");
	return err;
}

static void mt6789_remove(struct platform_device *pdev)
{
	struct mt6789_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int i;

	pr_info("Remove start.\n");

	pdev->dev.platform_data = NULL;

	/* unregister flashlight device */
	if (pdata && pdata->channel_num)
		for (i = 0; i < pdata->channel_num; i++)
			flashlight_dev_unregister_by_device_id(
					&pdata->dev_id[i]);
	else
		flashlight_dev_unregister(mt6789_NAME);

	/* flush work queue */
	flush_work(&mt6789_work);

	pr_info("Remove done.\n");
}

static void mt6789_shutdown(struct platform_device *pdev)
{
        pr_info(" mt6789_shutdown start.\n");
	mt6789_camera_set_led_flash_enable(SUBPMIC_LED1, false);
	mt6789_camera_set_led_torch_enable(SUBPMIC_LED1, false);
}

#ifdef CONFIG_OF
static const struct of_device_id mt6789_of_match[] = {
	{.compatible = mt6789_DTNAME},
	{},
};
MODULE_DEVICE_TABLE(of, mt6789_of_match);
#else
static struct platform_device mt6789_platform_device[] = {
	{
		.name = mt6789_NAME,
		.id = 0,
		.dev = {}
	},
	{}
};
MODULE_DEVICE_TABLE(platform, mt6789_platform_device);
#endif

static struct platform_driver mt6789_platform_driver = {
	.probe = mt6789_probe,
	.remove = mt6789_remove,
        .shutdown = mt6789_shutdown,
	.driver = {
		.name = mt6789_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = mt6789_of_match,
#endif
	},
};

static int __init flashlight_mt6789_init(void)
{
	int ret;

	pr_info("Init start.\n");

#ifndef CONFIG_OF
	ret = platform_device_register(&mt6789_platform_device);
	if (ret) {
		pr_info("Failed to register platform device\n");
		return ret;
	}
#endif
pr_info("Jamin mt6789  %s",mt6789_platform_driver.driver.of_match_table[0].compatible);

	ret = platform_driver_register(&mt6789_platform_driver);
	if (ret) {
		pr_info("Failed to register platform driver\n");
		return ret;
	}

	pr_info("Init done.\n");

	return 0;
}

static void __exit flashlight_mt6789_exit(void)
{
	pr_info("Exit start.\n");

	platform_driver_unregister(&mt6789_platform_driver);

	pr_info("Exit done.\n");
}

module_init(flashlight_mt6789_init);
module_exit(flashlight_mt6789_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xi Chen <xixi.chen@mediatek.com>");
MODULE_DESCRIPTION("MTK Flashlight mt6789 Driver");
