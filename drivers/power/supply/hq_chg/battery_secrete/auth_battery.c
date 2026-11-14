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
#include <linux/power_supply.h>

#include "battery_auth_class.h"

enum {
	MAIN_SUPPLY = 0,
	SECEON_SUPPLY,
	THIRD_SUPPLY,
	MAX_SUPPLY,
};

static const char *auth_device_name[] = {
	"main_suppiler",
	"second_supplier",
	"third_supplier",
	"unknown",
};


#if IS_ENABLED(CONFIG_BUILD_TARGET_TANZANITE)
static char* battery_name_txt[] = {
	[BATTERY_VENDOR_FIRST] = "NVT",
	[BATTERY_VENDOR_SECOND] = "SWD",
	[BATTERY_VENDOR_THIRD] = "COS",
	[BATTERY_VENDOR_UNKNOW] = "UNKNOWN",
};
#endif

struct auth_data {
	struct auth_device *auth_dev[MAX_SUPPLY];

	struct power_supply *verify_psy;
	struct power_supply_desc desc;

	struct delayed_work dwork;

	bool auth_result;
	u8 batt_id;
	struct batt_info_dev *batt_info;
};

static struct auth_data *g_info;
static int auth_index = 0;

static enum power_supply_property verify_props[] = {
	POWER_SUPPLY_PROP_AUTHENTIC,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_MANUFACTURER,
};

static int verify_get_property(struct power_supply *psy,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
	struct auth_data *info = power_supply_get_drvdata(psy);
	pr_info("%s:%d\n", __func__, psp);

	switch (psp) {
	case POWER_SUPPLY_PROP_AUTHENTIC:
		val->intval = info->auth_result;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = info->batt_id;
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = auth_device_name[auth_index];
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

#define AUTHENTIC_COUNT_MAX 3
static void auth_battery_dwork(struct work_struct *work)
{
	int i = 0;
	struct auth_data *info = container_of(to_delayed_work(work),
					      struct auth_data, dwork);

	int authen_result;
	static int retry_authentic = 0;

	for (i = 0; i < MAX_SUPPLY; i++) {
		if (!info->auth_dev[i])
			continue;
		authen_result = auth_device_start_auth(info->auth_dev[i]);
		if (!authen_result) {
			auth_device_get_batt_id(info->auth_dev[i], &(info->batt_id));
			auth_index = i;
			break;
		}
	}

	if (i == MAX_SUPPLY) {
		retry_authentic++;
		if (retry_authentic < AUTHENTIC_COUNT_MAX) {
			pr_info
			    ("battery authentic work begin to restart %d\n",
			     retry_authentic);
			schedule_delayed_work(&(info->dwork),
					      msecs_to_jiffies(5000));
		}
		if (retry_authentic == AUTHENTIC_COUNT_MAX) {
			pr_info("authentic result is %s\n",
				(authen_result == 0) ? "success" : "fail");
			info->batt_id = 0xff;
		}
	} else
		pr_info("authentic result is %s\n",
			(authen_result == 0) ? "success" : "fail");

	info->auth_result = ((authen_result == 0) ? true : false);
}

int batt_auth_get_batt_id(struct batt_info_dev *batt_info)
{
	struct auth_data *info = batt_info_get_private(batt_info);
	if (IS_ERR_OR_NULL(info))
		return BATTERY_VENDOR_UNKNOW;
	else
		return info->batt_id;
}

char* batt_auth_get_batt_name(struct batt_info_dev *batt_info)
{
	struct auth_data *info = batt_info_get_private(batt_info);
	if (IS_ERR_OR_NULL(info))
		return battery_name_txt[BATTERY_VENDOR_UNKNOW];
	else
		return battery_name_txt[info->batt_id];
}

int batt_auth_get_chip_ok(struct batt_info_dev *batt_info)
{
	struct auth_data *info = batt_info_get_private(batt_info);
	if (IS_ERR_OR_NULL(info) || !info->auth_result)
		return false;
	else
		return true;
}

struct batt_info_ops batt_info_ops = {
	.get_batt_id = batt_auth_get_batt_id,
	.get_batt_name = batt_auth_get_batt_name,
	.get_chip_ok = batt_auth_get_chip_ok,
};

static int __init auth_battery_init(void)
{
	int ret = 0;
	int i = 0;
	struct auth_data *info;
	struct power_supply_config cfg = { };

	pr_info("%s enter\n", __func__);

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	for (i = 0; i < MAX_SUPPLY; i++) {
		info->auth_dev[i] = get_batt_auth_by_name(auth_device_name[i]);
		if (!info->auth_dev[i])
			break;
	}

	cfg.drv_data = info;
	info->desc.name = "batt_verify";
	info->desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->desc.properties = verify_props;
	info->desc.num_properties = ARRAY_SIZE(verify_props);
	info->desc.get_property = verify_get_property;
	info->verify_psy =
	    power_supply_register(NULL, &(info->desc), &cfg);
	if (!(info->verify_psy)) {
		pr_err("%s register verify psy fail\n", __func__);
	}

	INIT_DELAYED_WORK(&info->dwork, auth_battery_dwork);
	info->batt_id = 0xff;
	g_info = info;

	for (i = 0; i < MAX_SUPPLY; i++) {
		if (!info->auth_dev[i])
			continue;
		ret = auth_device_start_auth(info->auth_dev[i]);
		if (!ret) {
			auth_device_get_batt_id(info->auth_dev[i], &(info->batt_id));
			auth_index = i;
			break;
		}
	}
	info->batt_info = batt_info_register("batt_info", &info->auth_dev[auth_index]->dev, &batt_info_ops, info);
	if (i >= MAX_SUPPLY)
		schedule_delayed_work(&info->dwork, msecs_to_jiffies(500));
	else
		info->auth_result = true;

	return 0;
}

static void __exit auth_battery_exit(void)
{
	int i = 0;

	power_supply_unregister(g_info->verify_psy);

	for (i = 0; i < MAX_SUPPLY; i++)
		auth_device_unregister(g_info->auth_dev[i]);

	kfree(g_info);
}

module_init(auth_battery_init);
module_exit(auth_battery_exit);
MODULE_LICENSE("GPL");
