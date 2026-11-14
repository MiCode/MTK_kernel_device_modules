#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/iio/consumer.h>
#include <linux/of_platform.h>
#include <linux/iio/consumer.h>

struct charge_thermal_info {
	struct device *dev;
	struct thermal_zone_device *tz_dev;
	struct iio_channel *channel;
	s32 *lookup_table;
	int nlookup_table;
};
#if 0
static int charge_thermal_adc_to_temp(struct charge_thermal_info *bti, int val)
{
	int temp, temp_hi, temp_lo, adc_hi, adc_lo;
	int i;

	if (!bti->lookup_table)
		return val;

	for (i = 0; i < bti->nlookup_table; i++) {
		if (val >= bti->lookup_table[2 * i + 1])
			break;
	}

	if (i == 0) {
		temp = bti->lookup_table[0];
	} else if (i >= bti->nlookup_table) {
		temp = bti->lookup_table[2 * (bti->nlookup_table - 1)];
	} else {
		adc_hi = bti->lookup_table[2 * i - 1];
		adc_lo = bti->lookup_table[2 * i + 1];

		temp_hi = bti->lookup_table[2 * i - 2];
		temp_lo = bti->lookup_table[2 * i];

		temp = temp_hi + mult_frac(temp_lo - temp_hi, val - adc_hi,
					   adc_lo - adc_hi);
	}

	return temp;
}
#endif
static int charge_thermal_get_temp(struct thermal_zone_device *tz, int *temp)
{
#if 0
	struct charge_thermal_info *bti = data;
	int val;
	int ret;

	ret = iio_read_channel_processed(bti->channel, &val);
	if (ret < 0) {
		dev_err(bti->dev, "IIO channel read failed %d\n", ret);
		return ret;
	}
	*temp = charge_thermal_adc_to_temp(bti, val);

	return 0;
#endif
	*temp = 25;
	return 0;
}

static const struct thermal_zone_device_ops charge_thermal_ops = {
	.get_temp = charge_thermal_get_temp,
};

static int charge_thermal_read_linear_lookup_table(struct device *dev,
						 struct charge_thermal_info *bti)
{
	struct device_node *np = dev->of_node;
	int ntable;
	int ret;

	ntable = of_property_count_elems_of_size(np, "temperature-lookup-table",
						 sizeof(u32));
	if (ntable <= 0) {
		dev_err(dev, "lookup table is not found\n");
		return 0;
	}

	if (ntable % 2) {
		dev_err(dev, "Pair of temperature vs ADC read value missing\n");
		return -EINVAL;
	}

	bti->lookup_table = devm_kcalloc(dev,
					 ntable, sizeof(*bti->lookup_table),
					 GFP_KERNEL);
	if (!bti->lookup_table)
		return -ENOMEM;

	ret = of_property_read_u32_array(np, "temperature-lookup-table",
					 (u32 *)bti->lookup_table, ntable);
	if (ret < 0) {
		dev_err(dev, "Failed to read temperature lookup table: %d\n",
			ret);
		return ret;
	}

	bti->nlookup_table = ntable / 2;

	return 0;
}

static int charge_thermal_probe(struct platform_device *pdev)
{
	struct charge_thermal_info *bti;
	int ret;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "Only DT based supported\n");
		return -ENODEV;
	}

	bti = devm_kzalloc(&pdev->dev, sizeof(*bti), GFP_KERNEL);
	if (!bti)
		return -ENOMEM;

	bti->channel = devm_iio_channel_get(&pdev->dev, "charge-adc-channel");
	if (IS_ERR(bti->channel)) {
		ret = PTR_ERR(bti->channel);
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev, "IIO channel not found: %d\n", ret);
		return ret;
	}

	ret = charge_thermal_read_linear_lookup_table(&pdev->dev, bti);
	if (ret < 0)
		return ret;

	bti->dev = &pdev->dev;
	platform_set_drvdata(pdev, bti);

	bti->tz_dev = devm_thermal_of_zone_register(&pdev->dev, 0, bti,
							   &charge_thermal_ops);
	if (IS_ERR(bti->tz_dev)) {
		ret = PTR_ERR(bti->tz_dev);
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev,
				"Thermal zone sensor register failed: %d\n",
				ret);
		return ret;
	}

	return 0;
}

static const struct of_device_id of_adc_thermal_match[] = {
	{ .compatible = "charge-adc-thermal", },
	{},
};
MODULE_DEVICE_TABLE(of, of_adc_thermal_match);

static struct platform_driver charge_thermal_driver = {
	.driver = {
		.name = "charge-adc-thermal",
		.of_match_table = of_adc_thermal_match,
	},
	.probe = charge_thermal_probe,
};

module_platform_driver(charge_thermal_driver);

MODULE_LICENSE("GPL v2");
