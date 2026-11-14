#include "fp_power_ctrl.h"

static struct device *fp_dev;
static struct fp_power_ctrl_data *fp_data;

int fp_power_ctrl_on(void)
{
	/* TODO: LDO configure */
	int status = 0;
	int voltage = 0;
	pr_info("%s,power_status =%d\n", __func__, fp_data->power_status);
	if (fp_data->power_status == 1) {
		pr_info("fp_pwr have already powered on\n");
	} else {
		pr_info("%s power on regulator_set_voltage before status: %d!!\n", __func__, status);
		status = regulator_set_voltage(fp_data->vreg, 3300000, 3300000);
		if (status < 0) {
			pr_err("%s power on regulator_set_voltage failed status: %d!!\n", __func__, status);
			return status;
		} else {
			pr_info("%s power on regulator_set_voltage status: %d!!\n", __func__, status);
		}
		status = regulator_enable(fp_data->vreg);
		if (status < 0) {
			pr_err("%s power on regulator_enable failed status: %d!!\n", __func__, status);
			return status;
		} else {
			pr_info("%s power on regulator_enable status %d!!\n", __func__, status);
			fp_data->power_status = 1;
		}
		voltage = regulator_get_voltage(fp_data->vreg);
		pr_info("fp_pwr power on regulator_value %d!!\n", voltage);
	}
	return status;
}
EXPORT_SYMBOL(fp_power_ctrl_on);

int fp_power_ctrl_off(void)
{
	int status = 0;
	pr_info("%s,power_status =%d\n",  __func__, fp_data->power_status);
	if (fp_data->power_status == 0) {
		pr_info("has already powered off\n");
	} else {
		if (fp_data->vreg != NULL) {
			status = regulator_disable(fp_data->vreg);
			if (status < 0) {
				pr_err("%s power off regulator_disable failed status: %d!!\n", __func__, status);
				return status;
			} else {
				pr_info("fp_pwr power off success: %d!!\n", status);
				fp_data->power_status = 0;
			}
		} else {
			pr_info("%s,fp_data->vreg is NULL!!\n",  __func__);
		}
	}
	return status;
}
EXPORT_SYMBOL(fp_power_ctrl_off);

static int fp_power_ctrl_probe(struct platform_device *pdev)
{
	int ret = 0;
	fp_dev = &pdev->dev;
	pr_info("%s enter\n", __func__);
	fp_data = devm_kzalloc(fp_dev, sizeof(struct fp_power_ctrl_data), GFP_KERNEL);
	if (!fp_data) {
		pr_err("%s fp_data is NULL!\n", __func__);
		return -ENOMEM;
	}
	fp_data->power_status = 0;
	fp_data->vreg = devm_regulator_get(fp_dev, "vfp");
	if (fp_data->vreg == NULL) {
		pr_err("%s fp_data->vreg is NULL!\n", __func__);
		return -EINVAL;
	}
	fp_data->dev = fp_dev;
	platform_set_drvdata(pdev, fp_data);
	ret = fp_power_ctrl_on();
	pr_info("%s fp_power_ctrl_on status = %d\n", __func__, ret);
	return ret;
}
static void fp_power_ctrl_remove(struct platform_device *pdev)
{
	pr_info("%s enter\n", __func__);
	devm_regulator_put(fp_data->vreg);
	//return 0;
}
static const struct of_device_id fp_power_ctrl_of_match[] = {
	{ .compatible = "fp_power_ctrl_status", },
	{},
};
static struct platform_driver fp_power_ctrl_status_driver = {
	.driver = {
		.name = "fp_power_ctrl_status",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(fp_power_ctrl_of_match),
	},
	.probe = fp_power_ctrl_probe,
	.remove = fp_power_ctrl_remove,
};
module_platform_driver(fp_power_ctrl_status_driver);
MODULE_DESCRIPTION("FP power ctrl");
MODULE_LICENSE("GPL");
