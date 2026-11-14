#include "fp_driver.h"
#include "fp_power_ctrl.h"

/* -------------------------------------------------------------------- */
/* fingerprint chip hardware configuration				*/
/* -------------------------------------------------------------------- */
#define DTS_VOlT_REGULATER "vfp"
#define DTS_NETLINK_NUM "netlink-event"
#define DTS_IRQ_GPIO "xiaomi,gpio_irq"
#define DTS_PINCTL_RESET_HIGH "reset_high"
#define DTS_PINCTL_RESET_LOW "reset_low"

int fp_parse_dts(struct fp_device *fp_dev)
{
	int ret;
	struct device_node *node = fp_dev->driver_device->dev.of_node;
	FUNC_ENTRY();
	if (node) {
		/*get irq resourece */
		fp_dev->irq_gpio = of_get_named_gpio(node, DTS_IRQ_GPIO, 0);
		pr_debug("fp::irq_gpio:%d\n", fp_dev->irq_gpio);
		if (!gpio_is_valid(fp_dev->irq_gpio)) {
			pr_debug("IRQ GPIO is invalid.\n");
			return -EPERM;
		}
		fp_dev->irq_num = gpio_to_irq(fp_dev->irq_gpio);
		of_property_read_u32(node, DTS_NETLINK_NUM,
				     &fp_dev->fp_netlink_num);
	} else {
		pr_debug("device node is null\n");
		return -EPERM;
	}
	if (fp_dev->driver_device) {
#ifdef FP_POWER_GPIO
		int rc = 0;
		fp_dev->pwr_gpio = of_get_named_gpio(node, "power_enable", 0);
		if (fp_dev->pwr_gpio < 0) {
			pr_err("falied to get pwr_gpio!\n");
			return fp_dev->pwr_gpio;
		}

		rc = devm_gpio_request(&fp_dev->driver_device->dev, fp_dev->pwr_gpio, "power_enable");

		if (rc) {
			pr_err("failed to request power_enable, rc = %d\n", rc);
			return -EPERM;
		}
		printk("fp gpio_get_value : %d\n",gpio_get_value(fp_dev->pwr_gpio));
#endif

		fp_dev->pinctrl = pinctrl_get(&fp_dev->driver_device->dev);
		if (IS_ERR(fp_dev->pinctrl)) {
			ret = PTR_ERR(fp_dev->pinctrl);
			pr_debug("can't find fingerprint pinctrl\n");
			return ret;
		}
		fp_dev->pins_reset_high = pinctrl_lookup_state(
			fp_dev->pinctrl, DTS_PINCTL_RESET_HIGH);
		if (IS_ERR(fp_dev->pins_reset_high)) {
			ret = PTR_ERR(fp_dev->pins_reset_high);
			pr_debug("can't find  pinctrl reset_high\n");
			return ret;
		}
		fp_dev->pins_reset_low = pinctrl_lookup_state(
			fp_dev->pinctrl, DTS_PINCTL_RESET_LOW);
		if (IS_ERR(fp_dev->pins_reset_low)) {
			ret = PTR_ERR(fp_dev->pins_reset_low);
			pr_debug("can't find  pinctrl reset_low\n");
			return ret;
		} else {
			pinctrl_select_state(fp_dev->pinctrl,
					     fp_dev->pins_reset_low);
		}

		fp_dev->pins_spiio_spi_mode =
			pinctrl_lookup_state(fp_dev->pinctrl, "spiio_spi_mode");
		if (IS_ERR(fp_dev->pins_spiio_spi_mode)) {
			ret = PTR_ERR(fp_dev->pins_spiio_spi_mode);
			pr_debug(
				"%s can't find fingerprint pinctrl spiio_spi_mode\n",
				__func__);
		} else {
			pinctrl_select_state(fp_dev->pinctrl,
					     fp_dev->pins_spiio_spi_mode);
		}

		fp_dev->pins_spiio_gpio_mode = pinctrl_lookup_state(
			fp_dev->pinctrl, "spiio_gpio_mode");
		if (IS_ERR(fp_dev->pins_spiio_gpio_mode)) {
			ret = PTR_ERR(fp_dev->pins_spiio_gpio_mode);
			pr_debug("%s can't find fingerprint spiio_gpio_mode\n",
				 __func__);
		}

		fp_dev->pins_eint_default =
			pinctrl_lookup_state(fp_dev->pinctrl, "eint_default");
		if (IS_ERR(fp_dev->pins_eint_default)) {
			ret = PTR_ERR(fp_dev->pins_eint_default);
			pr_debug(
				"%s can't find fingerprint pinctrl pins_eint_default\n",
				__func__);
			return ret;
		}

		fp_dev->pins_eint_pulldown =
			pinctrl_lookup_state(fp_dev->pinctrl, "eint_pulldown");
		if (IS_ERR(fp_dev->pins_eint_pulldown)) {
			ret = PTR_ERR(fp_dev->pins_eint_pulldown);
			pr_debug(
				"%s can't find fingerprint pinctrl pins_eint_pulldown\n",
				__func__);
			return ret;
		}
		pr_debug("get pinctrl success!\n");
	} else {
		pr_debug("platform device is null\n");
		return -EPERM;
	}
	FUNC_EXIT();
	return 0;
}

void fp_power_on(struct fp_device *fp_dev)
{
	FUNC_ENTRY();
	if (fp_dev->device_available == 1) {
		pr_err("have already powered on\n");
	} else {
#ifdef FP_POWER_REGULATOR
		fp_power_ctrl_on();
#endif

#ifdef FP_POWER_GPIO
	if (gpio_is_valid(fp_dev->pwr_gpio)) {
		gpio_set_value(fp_dev->pwr_gpio, 1);
		pr_debug("power on gpio result: %d!!\n", gpio_get_value(fp_dev->pwr_gpio));
	} else {
		pr_debug("%s: power on gpio_is_invalid\n", __func__);
	}
#endif
	}
		fp_dev->device_available = 1;
}

void fp_power_off(struct fp_device *fp_dev)
{
	FUNC_ENTRY();
	if (fp_dev->device_available == 0) {
		pr_err("has already powered off\n");
	} else {
#ifdef FP_POWER_REGULATOR
		fp_power_ctrl_off();
#endif

#ifdef FP_POWER_GPIO
		int status = 0;
		if (gpio_is_valid(fp_dev->pwr_gpio)) {
			status = gpio_direction_output(fp_dev->pwr_gpio, 0);
			pr_debug("power off gpio result: %d!!\n", status);
		} else {
			pr_debug("%s: power off gpio_is_invalid\n", __func__);
		}
#endif
	}
		fp_dev->device_available = 0;
}

/* delay ms after reset */
void fp_hw_reset(struct fp_device *fp_dev, u8 delay_ms)
{
	pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_reset_low);
	mdelay(5);
	pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_reset_high);
	mdelay(delay_ms);
}

void fp_enable_irq(struct fp_device *fp_dev)
{
	if (1 == fp_dev->irq_enabled) {
		pr_debug("irq already enabled\n");
	} else {
		enable_irq(fp_dev->irq_num);
		fp_dev->irq_enabled = 1;
		pr_debug("enable irq!\n");
	}
}

void fp_disable_irq(struct fp_device *fp_dev)
{
	if (0 == fp_dev->irq_enabled) {
		pr_debug("irq already disabled\n");
	} else {
		disable_irq(fp_dev->irq_num);
		fp_dev->irq_enabled = 0;
		pr_debug("disable irq!\n");
	}
}

void fp_kernel_key_input(struct fp_device *fp_dev, struct fp_key *fp_key)
{
	uint32_t key_input = 0;

	if (FP_KEY_HOME == fp_key->key) {
		key_input = FP_KEY_INPUT_HOME;
	} else if (FP_KEY_POWER == fp_key->key) {
		key_input = FP_KEY_INPUT_POWER;
	} else if (FP_KEY_CAMERA == fp_key->key) {
		key_input = FP_KEY_INPUT_CAMERA;
	} else {
		/* add special key define */
		key_input = fp_key->key;
	}

	pr_debug("received key event[%d], key=%d, value=%d\n", key_input,
		 fp_key->key, fp_key->value);

	if ((FP_KEY_POWER == fp_key->key || FP_KEY_CAMERA == fp_key->key) &&
	    (fp_key->value == 1)) {
		input_report_key(fp_dev->input, key_input, 1);
		input_sync(fp_dev->input);
		input_report_key(fp_dev->input, key_input, 0);
		input_sync(fp_dev->input);
	}

	if (FP_KEY_HOME == fp_key->key) {
		input_report_key(fp_dev->input, key_input, fp_key->value);
		input_sync(fp_dev->input);
	}
}
