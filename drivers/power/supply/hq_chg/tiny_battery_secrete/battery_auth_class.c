
#include <linux/module.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/gpio/driver.h>
#include <linux/of_gpio.h>
#include "battery_auth_class.h"
static struct class *auth_class;
static struct gpio_desc *gpiod;
#ifdef MODULE
static char __chg_cmdline[1024];
static char *chg_cmdline = __chg_cmdline;
const char *chg_get_cmd(void)
{
	struct device_node * of_chosen = NULL;
	char *bootargs = NULL;
	if (__chg_cmdline[0] != 0)
		return chg_cmdline;
	of_chosen = of_find_node_by_path("/chosen");
	if (of_chosen) {
		bootargs = (char *)of_get_property(
					of_chosen, "bootargs", NULL);
		if (!bootargs)
			pr_err("%s: failed to get bootargs\n", __func__);
		else {
			strncpy(__chg_cmdline, bootargs, 512);
			pr_err("%s: bootargs: %s\n", __func__, bootargs);
		}
	} else
		pr_err("%s: failed to get /chosen \n", __func__);
	return chg_cmdline;
}
#else
const char *chg_get_cmd(void)
{
	return saved_command_line;
}
#endif
static ssize_t name_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct auth_device *auth = to_auth_device(dev);
	return snprintf(buf, 20, "%s\n",
			auth->name ? auth->name : "anonymous");
}
static DEVICE_ATTR_RO(name);
static struct attribute *auth_attrs[] = {
	&dev_attr_name.attr,
	NULL,
};
static const struct attribute_group auth_group = {
	.attrs = auth_attrs,
};
static const struct attribute_group *auth_groups[] = {
	&auth_group,
	NULL,
};
int auth_device_set_cycle_count(struct auth_device *auth_dev, u32 set_cycle, u32 get_cycle)
{
	if (auth_dev != NULL && auth_dev->ops != NULL &&
	    auth_dev->ops->set_cycle_count != NULL)
		return auth_dev->ops->set_cycle_count(auth_dev, set_cycle, get_cycle);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(auth_device_set_cycle_count);
int auth_device_get_cycle_count(struct auth_device *auth_dev, u32 * cycle)
{
	if (auth_dev != NULL && auth_dev->ops != NULL &&
	    auth_dev->ops->get_cycle_count != NULL)
		return auth_dev->ops->get_cycle_count(auth_dev, cycle);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(auth_device_get_cycle_count);
int auth_device_get_first_usage_date(struct auth_device *auth_dev, u8 *first_usage_date, int len)
{
	if (auth_dev != NULL && auth_dev->ops != NULL &&
		auth_dev->ops->get_first_usage_date != NULL)
		return auth_dev->ops->get_first_usage_date(auth_dev, first_usage_date, len);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(auth_device_get_first_usage_date);
int auth_device_set_first_usage_date(struct auth_device *auth_dev, u8 *first_usage_date, int len)
{
	if (auth_dev != NULL && auth_dev->ops != NULL &&
		auth_dev->ops->set_first_usage_date != NULL)
		return auth_dev->ops->set_first_usage_date(auth_dev, first_usage_date, len);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(auth_device_set_first_usage_date);
static void auth_device_release(struct device *dev)
{
	struct auth_device *auth_dev = to_auth_device(dev);
	kfree(auth_dev);
}
struct auth_device *auth_device_register(const char *name,
					 struct device *parent,
					 void *devdata,
					 const struct auth_ops *ops)
{
	struct auth_device *auth_dev;
	int rc;
	char *cmdline_sub_str = NULL;
	pr_err("%s: name = %s\n", __func__, name);
	auth_dev = kzalloc(sizeof(*auth_dev), GFP_KERNEL);
	if (!auth_dev)
		return ERR_PTR(-ENOMEM);
	raw_spin_lock_init(&auth_dev->io_lock);
	auth_dev->dev.class = auth_class;
	auth_dev->dev.parent = parent;
	auth_dev->dev.release = auth_device_release;
	auth_dev->gpiod = gpiod;
	dev_set_name(&auth_dev->dev, name);
	dev_set_drvdata(&auth_dev->dev, devdata);
	rc = device_register(&auth_dev->dev);
	if (rc) {
		kfree(auth_dev);
		pr_err("%s: error\n", __func__);
		return ERR_PTR(rc);
	}
	auth_dev->ops = ops;
	/* LK will append battery secret result to command line */
	cmdline_sub_str = strstr(chg_get_cmd(), SECRET_IC);
	if (cmdline_sub_str) {
		sscanf(cmdline_sub_str, SECRET_IC"%d", &auth_dev->secret_ic);
		pr_info("%s: auth device secret_ic=%d\n", __func__, auth_dev->secret_ic);
	} else {
		pr_err("%s: cann't find %s in kernel commandline\n", __func__, SECRET_IC);
	}
	cmdline_sub_str = strstr(chg_get_cmd(), BATT_SN);
	if (cmdline_sub_str) {
		sscanf(cmdline_sub_str, BATT_SN"%s", auth_dev->batt_sn);
		pr_info("%s: auth device batt_sn=%s\n", __func__, auth_dev->batt_sn);
	} else {
		pr_err("%s: cann't find %s in kernel commandline\n", __func__, BATT_SN);
	}
	cmdline_sub_str = strstr(chg_get_cmd(), BATTERY_ID);
	if (cmdline_sub_str) {
		sscanf(cmdline_sub_str, BATTERY_ID"%d", &auth_dev->battery_id);
		pr_info("%s: auth device battery_id=%d\n", __func__, auth_dev->battery_id);
	} else {
		pr_err("%s: cann't find %s in kernel commandline\n", __func__, BATTERY_ID);
	}
	cmdline_sub_str = strstr(chg_get_cmd(), MI_AUTH_RESULT);
	if (cmdline_sub_str) {
		sscanf(cmdline_sub_str, MI_AUTH_RESULT"%d", &auth_dev->auth_result);
		pr_info("%s: auth device auth_result=%d\n", __func__, auth_dev->auth_result);
	} else {
		pr_err("%s: cann't find %s in kernel commandline\n", __func__, MI_AUTH_RESULT);
	}
	return auth_dev;
}
EXPORT_SYMBOL(auth_device_register);
void auth_device_unregister(struct auth_device *auth_dev)
{
	if (!auth_dev)
		return;
	auth_dev->ops = NULL;
	device_unregister(&auth_dev->dev);
}
EXPORT_SYMBOL(auth_device_unregister);
static int auth_match_device_by_name(struct device *dev, const void *data)
{
	const char *name = data;
	return strcmp(dev_name(dev), name) == 0;
}
struct auth_device *get_batt_auth_by_name(const char *name)
{
	struct device *dev;
	if (!name)
		return (struct auth_device *) NULL;
	dev = class_find_device(auth_class, NULL, name,
				auth_match_device_by_name);
	return dev ? to_auth_device(dev) : NULL;
}
EXPORT_SYMBOL(get_batt_auth_by_name);
int auth_device_get_ui_soh(struct auth_device *auth_dev, u8 *ui_soh_data, int len)
{
	if (auth_dev != NULL && auth_dev->ops != NULL &&
	    auth_dev->ops->get_ui_soh != NULL)
		return auth_dev->ops->get_ui_soh(auth_dev, ui_soh_data, len);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(auth_device_get_ui_soh);
int auth_device_set_ui_soh(struct auth_device *auth_dev, u8 *ui_soh_data, int len)
{
	if (auth_dev != NULL && auth_dev->ops != NULL &&
	    auth_dev->ops->set_ui_soh != NULL)
		return auth_dev->ops->set_ui_soh(auth_dev, ui_soh_data, len);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(auth_device_set_ui_soh);
int auth_device_get_raw_soh(struct auth_device *auth_dev)
{
	if (auth_dev != NULL && auth_dev->ops != NULL &&
	    auth_dev->ops->get_raw_soh != NULL)
		return auth_dev->ops->get_raw_soh(auth_dev);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(auth_device_get_raw_soh);
int auth_device_set_raw_soh(struct auth_device *auth_dev, int raw_soh)
{
	if (auth_dev != NULL && auth_dev->ops != NULL &&
	    auth_dev->ops->set_raw_soh != NULL)
		return auth_dev->ops->set_raw_soh(auth_dev, raw_soh);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(auth_device_set_raw_soh);
static int onewire_gpio_probe(struct platform_device *pdev)
{
	enum gpiod_flags gflags = GPIOD_OUT_LOW_OPEN_DRAIN;
	auth_class = class_create("battery_auth");
	if (IS_ERR(auth_class)) {
		pr_notice("Unable to create auth class\n");
		return -ENODEV;
	}
	auth_class->dev_groups = auth_groups;
	gpiod = devm_gpiod_get_index(&(pdev->dev), NULL, 0, gflags);
	if (IS_ERR(gpiod)) {
		pr_err("%s gpio_request (pin) failed\n", __func__);
		return PTR_ERR(gpiod);
	}
	gpiod_direction_output(gpiod, 1);
	pr_err("%s success\n", __func__);
	return 0;
}
static const struct of_device_id onewire_gpio_dt_match[] = {
	{.compatible = "xiaomi,onewire_gpio"},
	{},
};
static struct platform_driver onewire_gpio_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "onewire_gpio",
		   .of_match_table = onewire_gpio_dt_match,
		   },
	.probe = onewire_gpio_probe,
};
static int __init batt_auth_class_init(void)
{
	return platform_driver_register(&onewire_gpio_driver);
}
static void __exit batt_auth_class_exit(void)
{
	platform_driver_unregister(&onewire_gpio_driver);
}
module_init(batt_auth_class_init);
module_exit(batt_auth_class_exit);
MODULE_LICENSE("GPL");

