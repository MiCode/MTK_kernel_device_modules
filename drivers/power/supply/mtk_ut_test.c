#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mca/common/mca_log.h>
#include "pmic_voter.h"
#include "bq28z610.h"

#ifndef MCA_LOG_TAG
#define MCA_LOG_TAG "ut_test"
#endif

#define UT_VOTER_MONITOR_INTERVAL_MS 2000
#define UT_PARSE_DT_DELAY_TIME_MS (20 * 1000)

struct test_item {
	const char *item_name;
	const char *dts_key;
	const char *dts_value;
};

struct boolean_item {
	const char *item_name;
	const char *dts_key;
	bool supported;
};

static struct test_item ti[] = {
	{"cycle_volt", "mi,cycle_volt", NULL},
	{"cycle_step_curr", "mi,cycle_step_curr", NULL},
	{"temp_term_curr", "mi,temp_term_curr", NULL},
	{"thermal", "mi,thermal", NULL}
};

static struct boolean_item bi[] = {
	{"lossless_rechg", "mi,lossless_rechg", false}
};

struct ut_info {
	struct device *dev;
	struct device_node *soc_node;
	struct class ut_class;
	const char *config_str;
	bool voter_ok;
	struct delayed_work voter_monitor_work;
	struct delayed_work parse_dt_work;
	struct votable *fcc_votable;
};

static void ut_voter_monitor_work(struct work_struct *work)
{
	struct ut_info *ut = container_of(work, struct ut_info, voter_monitor_work.work);

	ut->voter_ok = true;
	if (!ut->fcc_votable) {
		ut->fcc_votable = find_votable("CHARGER_FCC");
		if (!ut->fcc_votable)
			ut->voter_ok = false;
	}

	if (ut->voter_ok) {
		mca_log_err("voter is ok");
		return;
	} else {
		mca_log_err("voter not ready, wait %dms to retry\n", UT_VOTER_MONITOR_INTERVAL_MS);
		schedule_delayed_work(&ut->voter_monitor_work, msecs_to_jiffies(UT_VOTER_MONITOR_INTERVAL_MS));
	}
}

static int parse_config_string(struct ut_info *ut)
{
	struct device_node *node = ut->dev->of_node;
	u32 buf_size = 0;
	int i, ret;
	int pack_vendor_id = -1;
	char *str_p = NULL;
	char *country = "";
	char *pack_vendor = "";
	char *dev_name = "";
	char dts_key[64] = {0};
	bool has_gbl_batt_para;

	has_gbl_batt_para = of_property_read_bool(node, "has-global-batt-para");
	if (has_gbl_batt_para) {
		bms_get_property(BMS_PROP_BATTERY_PACK_VENDOR, &pack_vendor_id);
		country = "_cn";

		switch (pack_vendor_id) {
		case 0:
			pack_vendor = "_byd";
			break;
		case 1:
			pack_vendor = "_coslight";
			break;
		case 2:
			pack_vendor = "_swd";
			break;
		case 3:
			pack_vendor = "_nvt";
			break;
		case 4:
			pack_vendor = "_scud";
			break;
		case 5:
			pack_vendor = "_tws";
			break;
		case 6:
			pack_vendor = "_lishen";
			break;
		case 7:
			pack_vendor = "_desay";
			break;
		default:
			pack_vendor = "_nvt";
			break;
		}
	}

	for (i = 0; i < ARRAY_SIZE(ti); ++i) {
		scnprintf(dts_key, sizeof(dts_key) - 1, "%s%s%s%s", ti[i].dts_key, pack_vendor, country, dev_name);
		mca_log_info("dts_key: %s\n", dts_key);
		of_property_read_string(node, dts_key, &ti[i].dts_value);
		mca_log_info("%s : %s\n", dts_key, ti[i].dts_value);
		if (!ti[i].dts_value)
			mca_log_err("read property %s failed!\n", dts_key);
		else
			buf_size += strlen(ti[i].item_name) + strlen(ti[i].dts_value) + 2 * sizeof(char);
	}
	for (i = 0; i < ARRAY_SIZE(bi); ++i) {
		scnprintf(dts_key, sizeof(dts_key) - 1, "%s%s%s%s", bi[i].dts_key, pack_vendor, country, dev_name);
		mca_log_info("dts_key: %s\n", dts_key);
		bi[i].supported = of_property_read_bool(node, dts_key);
		if (bi[i].supported)
			buf_size += strlen(bi[i].item_name) + sizeof(char);
		mca_log_info("%s : %s\n", dts_key, bi[i].supported ? "true" : "false");
	}

	buf_size += 1;
	str_p = devm_kzalloc(ut->dev, buf_size, GFP_KERNEL);
	if (!str_p) {
		mca_log_err("memory alloc for output str failed!\n");
		return -ENOMEM;
	}
	ut->config_str = str_p;
	
	for (i = 0; i < ARRAY_SIZE(ti); ++i) {
		if (ti[i].dts_value) {
			size_t remain = buf_size - (str_p - ut->config_str);
	
			if (i == 0)
				ret = scnprintf(str_p, remain, "%s,%s",
								ti[i].item_name, ti[i].dts_value);
			else
				ret = scnprintf(str_p, remain, ",%s,%s",
								ti[i].item_name, ti[i].dts_value);
	
			if (ret < 0) {
				mca_log_err("scnprintf %s failed, ret=%d\n",
							ti[i].item_name, ret);
				return -1;
			}
			str_p += ret;
		}
	}
	
	for (i = 0; i < ARRAY_SIZE(bi); ++i) {
		if (bi[i].supported) {
			size_t remain = buf_size - (str_p - ut->config_str);
	
			ret = scnprintf(str_p, remain, ",%s", bi[i].item_name);
			if (ret < 0) {
				mca_log_err("scnprintf %s failed, ret=%d\n",
							bi[i].item_name, ret);
				return -1;
			}
			str_p += ret;
		}
	}

	return 0;
}

static void ut_test_parse_dt_work(struct work_struct *work)
{
	struct ut_info *ut = container_of(work, struct ut_info, parse_dt_work.work);

	parse_config_string(ut);
}

static ssize_t config_show(const struct class *c, const struct class_attribute *attr, char *buf)
{
	struct ut_info *ut = container_of(c, struct ut_info, ut_class);

	return scnprintf(buf, PAGE_SIZE, "%s\n", ut->config_str ? ut->config_str : "");
}
static CLASS_ATTR_RO(config);

static ssize_t current_fcc_show(const struct class *c, const struct class_attribute *attr, char *buf)
{
	struct ut_info *ut = container_of(c, struct ut_info, ut_class);
	int value = 0;

    if (ut->fcc_votable) {
        value = get_effective_result(ut->fcc_votable);
	}

	return scnprintf(buf, PAGE_SIZE, "%d\n", value * 1000);
}
static CLASS_ATTR_RO(current_fcc);

static struct attribute *ut_test_attrs[] = {
	&class_attr_config.attr,
	&class_attr_current_fcc.attr,
	NULL,
};
ATTRIBUTE_GROUPS(ut_test);

static int ut_test_probe(struct platform_device *pdev)
{
	struct ut_info *ut;
	struct device *dev = &pdev->dev;
	int rc;

	mca_log_err("start\n");
	ut = devm_kzalloc(&pdev->dev, sizeof(*ut), GFP_KERNEL);
	if (!ut) {
		mca_log_err("memory alloc failed!\n");
		return -ENOMEM;
	}
	ut->dev = dev;

	platform_set_drvdata(pdev, ut);

	ut->ut_class.name = "ut_test";
	ut->ut_class.class_groups = ut_test_groups;
	rc = class_register(&ut->ut_class);
	if (rc < 0) {
		mca_log_err("Failed to create ut_class, rc = %d\n", rc);
		return rc;
	}
	INIT_DELAYED_WORK(&ut->voter_monitor_work, ut_voter_monitor_work);
	INIT_DELAYED_WORK(&ut->parse_dt_work, ut_test_parse_dt_work);
	schedule_delayed_work(&ut->parse_dt_work, msecs_to_jiffies(UT_PARSE_DT_DELAY_TIME_MS));
	schedule_delayed_work(&ut->voter_monitor_work, msecs_to_jiffies(UT_VOTER_MONITOR_INTERVAL_MS));
	mca_log_err("success\n");
	return 0;
}

static int ut_test_remove(struct platform_device *pdev)
{
	struct ut_info *ut = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&ut->parse_dt_work);
	cancel_delayed_work_sync(&ut->voter_monitor_work);

	class_unregister(&ut->ut_class);
	return 0;
}

static const struct of_device_id ut_test_of_match[] = {
	{ .compatible = "charge,ut-test" },
	{}
};
MODULE_DEVICE_TABLE(of, ut_test_of_match);

static struct platform_driver ut_test_driver = {
	.probe = ut_test_probe,
	.remove = ut_test_remove,
	.driver = {
		.name = "ut_test",
		.of_match_table = ut_test_of_match,
	},
};

static int __init ut_test_init(void)
{
	int rc;

	rc = platform_driver_register(&ut_test_driver);
	if (rc < 0)
		pr_err("ut_test driver register failed!\n");

	return rc;
}
module_init(ut_test_init);

static void __exit ut_test_exit(void)
{
	platform_driver_unregister(&ut_test_driver);
}
module_exit(ut_test_exit);

MODULE_DESCRIPTION("ut test driver");
MODULE_AUTHOR("shiweijun1@xiaomi.com");
MODULE_LICENSE("GPL v2");

