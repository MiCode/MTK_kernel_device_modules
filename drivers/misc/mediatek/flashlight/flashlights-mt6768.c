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
#include "../../../power/supply/wt_chg/charger_class/charger_class.h"

/* device tree should be defined in flashlight-dt.h */
#ifndef mt6768_DTNAME
#define mt6768_DTNAME "mediatek,flashlights_mt6768"
#endif

#define mt6768_NAME "flashlights-mt6768"

/* define registers */
#define mt6768_REG_SILICON_REVISION (0x00)

#define mt6768_REG_FLASH_FEATURE      (0x08)
#define mt6768_INDUCTOR_CURRENT_LIMIT (0x40)
#define mt6768_FLASH_RAMP_TIME        (0x00)
#define mt6768_FLASH_TIMEOUT          (0x07)

#define mt6768_TORCH_CURRENT_CONTROL (0x04)

#define mt6768_REG_ENABLE (0x01)
#define mt6768_ENABLE_STANDBY (0x00)
#define mt6768_ENABLE_TORCH (0x02)
#define mt6768_ENABLE_FLASH (0x03)

#define mt6768_REG_FLAG (0x0B)

/* define level */
#define mt6768_LEVEL_NUM 15
#define mt6768_LEVEL_TORCH 7
#define mt6768_HW_TIMEOUT 800 /* ms */
#define mt6768_FLASH_CURRENT 700   /* ma */
//N85 flash
#define mt6768_PREVIEW_DUTY 1
#define mt6768_PREVIEW_CURRENT 100   /* ma */
#define mt6768_TORCH_DUTY 2
#define mt6768_TORCH_CURRENT 113   /* ma */
#define mt6768_LONGEXPOSURE_DUTY 5
#define mt6768_LONGEXPOSURE_CURRENT 350   /* ma */

#if 0
#define mt6768_PINCTRL_PINSTATE_LOW 0
#define mt6768_PINCTRL_PINSTATE_HIGH 1
#define mt6768_PINCTRL_STATE_HW_EN_HIGH "hwen_high"
#define mt6768_PINCTRL_STATE_HW_EN_LOW  "hwen_low"
static struct pinctrl *mt6768_pinctrl;
static struct pinctrl_state *mt6768_hw_en_high;
static struct pinctrl_state *mt6768_hw_en_low;
#endif
int flash_flag = 0;
/* define mutex and work queue */
static DEFINE_MUTEX(mt6768_mutex);
static struct work_struct mt6768_work;
static int get_duty;
//static int mt6768_pinctrl_set(int state);
/* mt6768 revision */
//static int is_mt6768lt;

/* define usage count */
static int use_count;

/* platform data */
struct mt6768_platform_data {
	int channel_num;
	struct flashlight_device_id *dev_id;
};

extern int subpmic_camera_set_led_flash_curr(int index, int ma);
extern int subpmic_camera_set_led_flash_enable(int index, bool en);
extern int subpmic_camera_set_led_torch_curr(int index, int ma);
extern int subpmic_camera_set_led_torch_enable(int index, bool en);

/******************************************************************************
 * mt6768 operations
 *****************************************************************************/
// 0x06->100ma,0x07->113ma,0x1A->350ma,
static const int mt6768_current[mt6768_LEVEL_NUM] = {
	25, 100, 113, 200, 275, 350, 425, 500, 575, 650,
	725, 800, 875, 950, 1000
};

static const int mt6768_flash[mt6768_LEVEL_NUM] = {
	25, 100, 113, 200, 275, 350, 425, 500, 575, 650,
	725, 800, 875, 950, 1000
};

static const int mt6768_torch[mt6768_LEVEL_NUM] = {
	25, 100, 113, 200, 275, 350, 425
};

static int mt6768_level = -1;

static int mt6768_is_torch(int level)
{
	if (level >= mt6768_LEVEL_TORCH)
		return -1;

	return 0;
}

static int mt6768_verify_level(int level)
{
	if (level < 0)
		level = 0;
	else if (level >= mt6768_LEVEL_NUM)
		level = mt6768_LEVEL_NUM - 1;

	return level;
}

/* flashlight enable function */
static int mt6768_enable(void)
{
#if 0
	unsigned char reg, val;


	reg = mt6768_REG_ENABLE;
	if (!mt6768_is_torch(mt6768_level)) {
		/* torch mode */
		val = mt6768_ENABLE_TORCH;
	} else {
		/* flash mode */
		val = mt6768_ENABLE_FLASH;
	}
	val = mt6768_ENABLE_TORCH;
#endif

	return 0;
}

/* flashlight disable function */
static int mt6768_disable(void)
{
	//unsigned char reg, val;

	//reg = mt6768_REG_ENABLE;
	//val = mt6768_ENABLE_STANDBY;

	//return mt6768_write_reg(mt6768_i2c_client, reg, val);
	return 0;
}

/* set flashlight level */
static int mt6768_set_level(int level)
{
	level = mt6768_verify_level(level);
	pr_info("mt6768_verify_level = %d.\n", level);
	mt6768_level = level;

	/* set brightness level */
	if (!mt6768_is_torch(level)) {
		subpmic_camera_set_led_torch_curr(0, mt6768_torch[level]);
	} else {
		subpmic_camera_set_led_flash_curr(0, mt6768_flash[level]);
	}

	return 0;
}
static int mt6768_get_flag(void)
{
	return 0;
}

/* flashlight init */
int mt6768_init(void)
{
	return 0;
}

/* flashlight uninit */
int mt6768_uninit(void)
{
	mt6768_disable();

	return 0;
}

/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static struct hrtimer mt6768_timer;
static unsigned int mt6768_timeout_ms;

static void mt6768_work_disable(struct work_struct *data)
{
	pr_info("work queue callback\n");
	mt6768_disable();
}

static enum hrtimer_restart mt6768_timer_func(struct hrtimer *timer)
{
	schedule_work(&mt6768_work);
	return HRTIMER_NORESTART;
}


/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
static int mt6768_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;
	ktime_t ktime;
	unsigned int s;
	unsigned int ns;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	pr_info("[xxdd_cmd1] [%s][%d] cmd = [%d]",__func__,__LINE__,_IOC_NR(cmd));

	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		pr_info("FLASH_IOC_SET_TIME_OUT_TIME_MS(%d): %d\n",
				channel, (int)fl_arg->arg);
		mt6768_timeout_ms = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		pr_info("FLASH_IOC_SET_DUTY(%d): %d\n",
				channel, (int)fl_arg->arg);
		get_duty = (int)fl_arg->arg;
		mt6768_set_level(get_duty);
		break;

	case FLASH_IOC_SET_ONOFF:
		pr_info("FLASH_IOC_SET_ONOFF(%d): %d\n",
				channel, (int)fl_arg->arg);
		if (fl_arg->arg == 1) {
			if (mt6768_timeout_ms) {
				s = mt6768_timeout_ms / 1000;
				ns = mt6768_timeout_ms % 1000 * 1000000;
				ktime = ktime_set(s, ns);
				hrtimer_start(&mt6768_timer, ktime,
						HRTIMER_MODE_REL);
			}
			if(!mt6768_is_torch(get_duty)){
				subpmic_camera_set_led_torch_enable(0, true);
			} else {
				subpmic_camera_set_led_flash_enable(0, true);
			}
		} else {
			if(!mt6768_is_torch(get_duty)){
				subpmic_camera_set_led_torch_enable(0, false);
			} else {
				subpmic_camera_set_led_flash_enable(0, false);
			}
			hrtimer_cancel(&mt6768_timer);
		}
		break;

	case FLASH_IOC_GET_DUTY_NUMBER:
		pr_info("FLASH_IOC_GET_DUTY_NUMBER(%d)\n", channel);
		fl_arg->arg = mt6768_LEVEL_NUM;
		break;

	case FLASH_IOC_GET_MAX_TORCH_DUTY:
		pr_info("FLASH_IOC_GET_MAX_TORCH_DUTY(%d)\n", channel);
		fl_arg->arg = mt6768_LEVEL_TORCH - 1;
		break;

	case FLASH_IOC_GET_DUTY_CURRENT:
		fl_arg->arg = mt6768_verify_level(fl_arg->arg);
		pr_info("FLASH_IOC_GET_DUTY_CURRENT(%d): %d\n",
				channel, (int)fl_arg->arg);
		fl_arg->arg = mt6768_current[fl_arg->arg];
		break;

	case FLASH_IOC_GET_HW_TIMEOUT:
		pr_info("FLASH_IOC_GET_HW_TIMEOUT(%d)\n", channel);
		fl_arg->arg = mt6768_HW_TIMEOUT;
		break;

	case FLASH_IOC_GET_HW_FAULT:
		pr_info("FLASH_IOC_GET_HW_FAULT(%d)\n", channel);
		fl_arg->arg = mt6768_get_flag();
		break;

	default:
		pr_info("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int mt6768_open(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int mt6768_release(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int mt6768_set_driver(int set)
{
	int ret = 0;

	/* set chip and usage count */
	pr_info("[xxdd] [%s][%d]",__func__,__LINE__);
	mutex_lock(&mt6768_mutex);
	if (set) {
		if (!use_count)
			ret = mt6768_init();
		use_count++;
		pr_info("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = mt6768_uninit();
		if (use_count < 0)
			use_count = 0;
		pr_info("Unset driver: %d\n", use_count);
	}
	mutex_unlock(&mt6768_mutex);
	pr_info("[xxdd] [%s][%d]",__func__,__LINE__);

	return ret;
}

static ssize_t mt6768_strobe_store(struct flashlight_arg arg)
{
	mt6768_set_driver(1);
	mt6768_set_level(arg.level);
	mt6768_timeout_ms = 0;
	mt6768_enable();
	msleep(arg.dur);
	mt6768_disable();
	mt6768_set_driver(0);

	return 0;
}
#if 1
static ssize_t att_store_mt6768(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int val = 0;
	int ret = 0;
	if (buf != NULL && count != 0) {
		pr_info("mt6768_flash buf = %s", buf);
		ret = kstrtoint(buf, 10, &val);
		if (ret < 0) {
			pr_err(	"[%s] val is %d ??\n", __func__, val);
			val = 0;
		}
		flash_flag = val < 0 ? 0 : val;
	}
	pr_info("echo mt6768_flash debug flash_flag = %d", flash_flag);
	if (flash_flag == 0) {
		pr_info("mt6768_flash 0 close");
		subpmic_camera_set_led_torch_enable(0, false);
		mt6768_set_driver(0);
	} else if (flash_flag == 2){
		pr_info("mt6768_flash 2 torch current");
		mt6768_set_driver(1);
		subpmic_camera_set_led_torch_curr(0, mt6768_TORCH_CURRENT);
		mt6768_timeout_ms = 0;
		subpmic_camera_set_led_torch_enable(0, true);
	} else if (flash_flag == 3){
		pr_info("mt6768_flash 3 longexposure current");
		mt6768_set_driver(1);
		subpmic_camera_set_led_torch_curr(0, mt6768_LONGEXPOSURE_CURRENT);
		mt6768_timeout_ms = 0;
		subpmic_camera_set_led_torch_enable(0, true);
	} else {
		pr_info("mt6768_flash 1 open");
		mt6768_set_driver(1);
		subpmic_camera_set_led_torch_curr(0, 100);
		mt6768_timeout_ms = 0;
		subpmic_camera_set_led_torch_enable(0, true);
	}
	return count;
}

static ssize_t att_show_mt6768(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", flash_flag);
}

static DEVICE_ATTR(mt6768_flash, 0664, att_show_mt6768, att_store_mt6768);
#endif
static struct flashlight_operations mt6768_ops = {
	mt6768_open,
	mt6768_release,
	mt6768_ioctl,
	mt6768_strobe_store,
	mt6768_set_driver
};

static int mt6768_parse_dt(struct device *dev,
		struct mt6768_platform_data *pdata)
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
				mt6768_NAME);
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
static int mt6768_probe(struct platform_device *pdev)
{
	struct mt6768_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int ret = 0;
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
		err = mt6768_parse_dt(&pdev->dev, pdata);
		if (err) {
			pr_info("error  to mt6768_parse_dt.\n");
			goto err_free;
		}
	}

	/* init work queue */
	INIT_WORK(&mt6768_work, mt6768_work_disable);
	pr_info("OK  to INIT_WORK.\n");

	/* init timer */
	hrtimer_init(&mt6768_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	mt6768_timer.function = mt6768_timer_func;
	mt6768_timeout_ms = 800;

	/* clear usage count */
	use_count = 0;

	/* register flashlight device */
	if (pdata->channel_num) {
		for (i = 0; i < pdata->channel_num; i++)
			if (flashlight_dev_register_by_device_id(
						&pdata->dev_id[i],
						&mt6768_ops)) {
				err = -EFAULT;
				pr_info("error  to flashlight_dev_register_by_device_id.\n");
				goto err_free;
			}
	} else {
		if (flashlight_dev_register(mt6768_NAME, &mt6768_ops)) {
			err = -EFAULT;
			pr_info("error  to flashlight_dev_register.\n");
			goto err_free;
		}
	}
	ret = sysfs_create_file(&pdev->dev.kobj, &dev_attr_mt6768_flash.attr);
	if(ret == 0)
	pr_info("mt6768 Probe done.\n");

	return 0;
err_free:
	pr_info("mt6768 Probe failed.\n");
	return err;
}

static int mt6768_remove(struct platform_device *pdev)
{
	struct mt6768_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int i;

	pr_info("Remove start.\n");

	pdev->dev.platform_data = NULL;

	/* unregister flashlight device */
	if (pdata && pdata->channel_num)
		for (i = 0; i < pdata->channel_num; i++)
			flashlight_dev_unregister_by_device_id(
					&pdata->dev_id[i]);
	else
		flashlight_dev_unregister(mt6768_NAME);

	/* flush work queue */
	flush_work(&mt6768_work);

	pr_info("Remove done.\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt6768_of_match[] = {
	{.compatible = mt6768_DTNAME},
	{},
};
MODULE_DEVICE_TABLE(of, mt6768_of_match);
#else
static struct platform_device mt6768_platform_device[] = {
	{
		.name = mt6768_NAME,
		.id = 0,
		.dev = {}
	},
	{}
};
MODULE_DEVICE_TABLE(platform, mt6768_platform_device);
#endif

static struct platform_driver mt6768_platform_driver = {
	.probe = mt6768_probe,
	.remove = mt6768_remove,
	.driver = {
		.name = mt6768_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = mt6768_of_match,
#endif
	},
};

static int __init flashlight_mt6768_init(void)
{
	int ret;

	pr_info("Init start.\n");

#ifndef CONFIG_OF
	ret = platform_device_register(&mt6768_platform_device);
	if (ret) {
		pr_info("Failed to register platform device\n");
		return ret;
	}
#endif
pr_info("Jamin mt6768  %s",mt6768_platform_driver.driver.of_match_table[0].compatible);

	ret = platform_driver_register(&mt6768_platform_driver);
	if (ret) {
		pr_info("Failed to register platform driver\n");
		return ret;
	}

	pr_info("Init done.\n");

	return 0;
}

static void __exit flashlight_mt6768_exit(void)
{
	pr_info("Exit start.\n");

	platform_driver_unregister(&mt6768_platform_driver);

	pr_info("Exit done.\n");
}

module_init(flashlight_mt6768_init);
module_exit(flashlight_mt6768_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xi Chen <xixi.chen@mediatek.com>");
MODULE_DESCRIPTION("MTK Flashlight mt6768 Driver");