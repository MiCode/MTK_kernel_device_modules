// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/usb/typec.h>
#include <linux/usb/typec_mux.h>
#include <linux/atomic.h>
#include <linux/interrupt.h>
#include <linux/regmap.h>
#include <linux/of_device.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/usb/typec_dp.h>

#if IS_ENABLED(CONFIG_MTK_USB_TYPEC_MUX)
#include "mux_switch.h"
#endif

#if IS_ENABLED(CONFIG_DEVICE_MODULES_USB_MTU3)
#include "mtu3.h"
#endif

#define TX_EQ_COUNT 3
#define SUSPEND_RESUME_TIMEOUT (HZ) /* 1s */

#define ps5170_ORIENTATION_NONE                 0x80

/* USB Only */
#define ps5170_ORIENTATION_NORMAL               0xc0
#define ps5170_ORIENTATION_FLIP                 0xd0
/* DP Only */
#define ps5170_ORIENTATION_NORMAL_DP            0xa0
#define ps5170_ORIENTATION_FLIP_DP              0xb0
/* USB + DP */
#define ps5170_ORIENTATION_NORMAL_USBDP         0xe0
#define ps5170_ORIENTATION_FLIP_USBDP           0xf0

/* PMIC Voter offset */
#define VS_VOTER_EN_LO 0x0
#define VS_VOTER_EN_LO_SET 0x1
#define VS_VOTER_EN_LO_CLR 0x2

enum redriver_smc_request {
	REDRIVER_SMC_PMIC_REQUEST = 4,
	REDRIVER_SMC_PMIC_RELEASE,
};

enum redriver_power_state {
	REDRIVER_DP_CONFIG = 0,
	REDRIVER_DISABLE,
};

/*
 * 0 init
 * 1 off mode
 * 2 USB only mode        NORMAL
 * 3 USB only mode        FLIP
 * 4 DP only mode 4-lane  NORMAL
 * 5 DP only mode 4-lane  FLIP
 * 6 DP 2 lane + USB mode NORMAL
 * 7 DP 2 lane + USB mode FLIP
 */

enum ps5170_mode {
	PS5170_MODE_INIT = 0,
	PS5170_MODE_NONE,
	PS5170_MODE_USB_FRONT,
	PS5170_MODE_USB_BACK,
	PS5170_MODE_DP4L_FRONT,
	PS5170_MODE_DP4L_BACK,
	PS5170_MODE_DP_FRONT,
	PS5170_MODE_DP_BACK,
};

struct ps5170 {
	struct device *dev;
	struct i2c_client *i2c;
	struct typec_switch_dev *sw;
	struct typec_mux_dev *mux;
	struct pinctrl *pinctrl;
	struct pinctrl_state *enable;
	struct pinctrl_state *disable;
	struct mutex lock;
	enum typec_orientation orientation;
	struct work_struct reconfig_dp_work;
	struct workqueue_struct *wq;

	atomic_t in_sleep;

	/* pmic vs voter */
	struct regmap *vsv;
	u32 vsv_reg;
	u32 vsv_mask;
	u32 vsv_vers;

	u32 tx_eq[TX_EQ_COUNT];
	bool tx_eq_tuning;

	uint8_t pin_assign;
	u8 polarity;
	bool is_dp;
	enum ps5170_mode current_mode;

#if IS_ENABLED(CONFIG_DEVICE_MODULES_USB_MTU3)
	struct ssusb_mtk *ssusb;
	struct notifier_block ssusb_nb;
	bool usb_sync_enabled;
	bool usb_on;
#endif
};

struct set_mode_work_data {
	struct ps5170 *ps;
	struct work_struct set_mode_work;
	enum ps5170_mode new_mode;
	bool free_work;
};

void ps5170_smc_request(struct ps5170 *ps,
	enum redriver_power_state state)
{
	struct arm_smccc_res res;
	int op;

	dev_info(ps->dev, "%s state = %d\n", __func__, state);

	switch (state) {
	case REDRIVER_DP_CONFIG:
		op = REDRIVER_SMC_PMIC_REQUEST;
		break;
	case REDRIVER_DISABLE:
		op = REDRIVER_SMC_PMIC_RELEASE;
		break;
	default:
		return;
	}

	arm_smccc_smc(MTK_SIP_KERNEL_USB_CONTROL,
		op, 0, 0, 0, 0, 0, 0, &res);
}

void ps5170_vsvoter_set(struct ps5170 *ps)
{
	u32 reg, msk, val;

	if (IS_ERR_OR_NULL(ps->vsv))
		return;

	/* write 1 to set and clr, update reg address */
	reg = ps->vsv_reg + VS_VOTER_EN_LO_SET;
	msk = ps->vsv_mask;
	val = ps->vsv_mask;

	regmap_update_bits(ps->vsv, reg, msk, val);
	dev_info(ps->dev, "%s set voter for vs\n", __func__);

}

void ps5170_vsvoter_clr(struct ps5170 *ps)
{
	u32 reg, msk, val;

	if (IS_ERR_OR_NULL(ps->vsv))
		return;

	/* write 1 to set and clr, update reg address */
	reg = ps->vsv_reg + VS_VOTER_EN_LO_CLR;
	msk = ps->vsv_mask;
	val = ps->vsv_mask;

	regmap_update_bits(ps->vsv, reg, msk, val);
	dev_info(ps->dev, "%s clr voter for vs\n", __func__);

}

static int ps5170_vsvoter_of_property_parse(struct ps5170 *ps,
				struct device_node *dn)
{
	struct of_phandle_args args;
	struct platform_device *pdev;
	int ret;

	/* vs vote function is optional */
	if (!of_property_read_bool(dn, "mediatek,vs-voter"))
		return 0;

	ret = of_parse_phandle_with_fixed_args(dn,
		"mediatek,vs-voter", 3, 0, &args);
	if (ret)
		return ret;

	pdev = of_find_device_by_node(args.np->child);
	if (!pdev)
		return -ENODEV;

	ps->vsv = dev_get_regmap(pdev->dev.parent, NULL);
	if (!ps->vsv)
		return -ENODEV;

	ps->vsv_reg = args.args[0];
	ps->vsv_mask = args.args[1];
	ps->vsv_vers = args.args[2];
	dev_info(ps->dev, "vsv - reg:0x%x, mask:0x%x, version:%d\n",
			ps->vsv_reg, ps->vsv_mask, ps->vsv_vers);

	return PTR_ERR_OR_ZERO(ps->vsv);
}

static void ps5170_driving_of_property_parse(struct ps5170 *ps)
{

	ps->tx_eq_tuning = false;

	if (!device_property_read_u32_array(ps->dev, "mediatek,tx-eq",
		ps->tx_eq, TX_EQ_COUNT))
		ps->tx_eq_tuning = true;

	dev_info(ps->dev, "tx_eq: <0x50>:0x%x, <0x54>:0x%x, <0x5d>:0x%x\n",
			ps->tx_eq[0], ps->tx_eq[1], ps->tx_eq[2]);

}

static int ps5170_init(struct ps5170 *ps)
{
	/* Configure PS5170 redriver */

	i2c_smbus_write_byte_data(ps->i2c, 0x9d, 0x80);
	/* add a delay */
	mdelay(20);
	i2c_smbus_write_byte_data(ps->i2c, 0x9d, 0x00);
	/* auto power down */
	i2c_smbus_write_byte_data(ps->i2c, 0x40, 0x80);
	/* Force AUX RX data reverse */
	i2c_smbus_write_byte_data(ps->i2c, 0x9F, 0x02);
	/* Fine tune LFPS swing */
	/* i2c_smbus_write_byte_data(ps->i2c, 0x8d, 0x01); */
	/* Fine tune LFPS swing */
	/* i2c_smbus_write_byte_data(ps->i2c, 0x90, 0x01); */
	i2c_smbus_write_byte_data(ps->i2c, 0x51, 0x87);
	i2c_smbus_write_byte_data(ps->i2c, 0x50, 0x20);
	i2c_smbus_write_byte_data(ps->i2c, 0x54, 0x11);
	i2c_smbus_write_byte_data(ps->i2c, 0x5d, 0x66);
	i2c_smbus_write_byte_data(ps->i2c, 0x52, 0x20);
	i2c_smbus_write_byte_data(ps->i2c, 0x55, 0x00);
	i2c_smbus_write_byte_data(ps->i2c, 0x56, 0x00);
	i2c_smbus_write_byte_data(ps->i2c, 0x57, 0x00);
	i2c_smbus_write_byte_data(ps->i2c, 0x58, 0x00);
	i2c_smbus_write_byte_data(ps->i2c, 0x59, 0x00);
	i2c_smbus_write_byte_data(ps->i2c, 0x5a, 0x00);
	i2c_smbus_write_byte_data(ps->i2c, 0x5b, 0x00);
	i2c_smbus_write_byte_data(ps->i2c, 0x5e, 0x06);
	i2c_smbus_write_byte_data(ps->i2c, 0x5f, 0x00);
	i2c_smbus_write_byte_data(ps->i2c, 0x60, 0x00);
	i2c_smbus_write_byte_data(ps->i2c, 0x61, 0x03);
	i2c_smbus_write_byte_data(ps->i2c, 0x65, 0x40);
	i2c_smbus_write_byte_data(ps->i2c, 0x66, 0x00);
	i2c_smbus_write_byte_data(ps->i2c, 0x67, 0x03);
	i2c_smbus_write_byte_data(ps->i2c, 0x75, 0x0C);
	i2c_smbus_write_byte_data(ps->i2c, 0x77, 0x00);
	i2c_smbus_write_byte_data(ps->i2c, 0x78, 0x7C);

	if (ps->tx_eq_tuning == true) {
		i2c_smbus_write_byte_data(ps->i2c, 0x50, ps->tx_eq[0]);
		i2c_smbus_write_byte_data(ps->i2c, 0x54, ps->tx_eq[1]);
		i2c_smbus_write_byte_data(ps->i2c, 0x5d, ps->tx_eq[2]);
	}

	return 0;

}

static bool is_dp_mode(enum ps5170_mode mode)
{
	if (mode == PS5170_MODE_DP_FRONT || mode == PS5170_MODE_DP_BACK ||
		mode == PS5170_MODE_DP4L_FRONT || mode == PS5170_MODE_DP4L_BACK)
		return true;
	else
		return false;
}

static void ps5170_dp_disable(struct ps5170 *ps)
{
	dev_info(ps->dev, "%s Disable DP\n", __func__);
	/* switch off */
	i2c_smbus_write_byte_data(ps->i2c, 0x40, 0x80);
	/* Disable AUX channel */
	i2c_smbus_write_byte_data(ps->i2c, 0xa0, 0x02);
	/* HPD low */
	i2c_smbus_write_byte_data(ps->i2c, 0xa1, 0x00);
	mdelay(10);
	/* Release PMIC LPM Request */
	ps5170_smc_request(ps, REDRIVER_DISABLE);
}

static void ps5170_dp_enable(struct ps5170 *ps, enum ps5170_mode mode)
{
	dev_info(ps->dev, "%s Enable DP %d\n", __func__, mode);
	/* Request PMIC LPM resource */
	ps5170_smc_request(ps, REDRIVER_DP_CONFIG);

	switch (mode) {
	case PS5170_MODE_DP4L_FRONT:
		/* DP Only mode 4-lane */
		i2c_smbus_write_byte_data(ps->i2c, 0x40, ps5170_ORIENTATION_NORMAL_DP);
		break;
	case PS5170_MODE_DP4L_BACK:
		/* DP Only mode 4-lane */
		i2c_smbus_write_byte_data(ps->i2c, 0x40, ps5170_ORIENTATION_FLIP_DP);
		break;
	case PS5170_MODE_DP_FRONT:
		/*  DP 2 lane + USB mode */
		i2c_smbus_write_byte_data(ps->i2c, 0x40, ps5170_ORIENTATION_NORMAL_USBDP);
		break;
	case PS5170_MODE_DP_BACK:
			/*  DP 2 lane + USB mode */
		i2c_smbus_write_byte_data(ps->i2c, 0x40, ps5170_ORIENTATION_FLIP_USBDP);
		break;
	default:
		break;
	}

	/* Enable AUX channel */
	i2c_smbus_write_byte_data(ps->i2c, 0xa0, 0x00);
	/* HPD */
	i2c_smbus_write_byte_data(ps->i2c, 0xa1, 0x04);
}

static void ps5170_set_mode_work(struct work_struct *work)
{
	struct set_mode_work_data *work_data =
		container_of(work, struct set_mode_work_data, set_mode_work);
	struct ps5170 *ps = work_data->ps;
	enum ps5170_mode current_mode, new_mode;
	bool do_free_work;
	unsigned long timeout;

	new_mode = work_data->new_mode;
	current_mode = ps->current_mode;
	do_free_work = work_data->free_work;

	dev_info(ps->dev, "%s from %d to %d\n", __func__, current_mode, new_mode);

	if (current_mode == new_mode)
		goto same_mode;

	timeout = jiffies + SUSPEND_RESUME_TIMEOUT;

	while (time_before(jiffies, timeout)) {
		if (!atomic_read(&ps->in_sleep))
			break;
		dev_info(ps->dev, "wait for suspend/resume complete\n");
		msleep(20);
	}

	/* power on first when mode switch to on */
	if (current_mode == PS5170_MODE_NONE && new_mode != PS5170_MODE_NONE) {
		/* vote vs to enable */
		ps5170_vsvoter_set(ps);
		mdelay(10);
		/* switch on */
		if (ps->enable) {
			pinctrl_select_state(ps->pinctrl, ps->enable);
			mdelay(20);
		}
		ps5170_init(ps);
	}

	/* apply usb mode and dp mode setting */
	if (new_mode == PS5170_MODE_USB_FRONT)
		i2c_smbus_write_byte_data(ps->i2c, 0x40,
			ps5170_ORIENTATION_NORMAL);
	else if (new_mode == PS5170_MODE_USB_BACK)
		i2c_smbus_write_byte_data(ps->i2c, 0x40,
			ps5170_ORIENTATION_FLIP);
	else if (is_dp_mode(new_mode))
		ps5170_dp_enable(ps, new_mode);

	/* power off when mode switch to off */
	if (current_mode != PS5170_MODE_NONE && new_mode == PS5170_MODE_NONE) {
		/* apply dp off first */
		if (is_dp_mode(current_mode))
			ps5170_dp_disable(ps);

		/* switch off */
		i2c_smbus_write_byte_data(ps->i2c, 0x40, 0x80);
		if (ps->disable)
			pinctrl_select_state(ps->pinctrl, ps->disable);
		mdelay(10);
		/* vote vs to disable  */
		ps5170_vsvoter_clr(ps);
	}

	ps->current_mode = new_mode;

same_mode:
	if (do_free_work)
		kfree(work_data);
}

static void ps5170_set_mode(struct ps5170 *ps, enum ps5170_mode mode, bool sync)
{
	struct set_mode_work_data *work_data;

	work_data = kzalloc(sizeof(struct set_mode_work_data), GFP_ATOMIC);
	if (!work_data)
		return;

	INIT_WORK(&work_data->set_mode_work, ps5170_set_mode_work);

	work_data->ps = ps;
	work_data->new_mode = mode;
	work_data->free_work = !sync;
	queue_work(ps->wq, &work_data->set_mode_work);

	/* wait for the work to complete */
	if (sync) {
		flush_work(&work_data->set_mode_work);
		kfree(work_data);
	}
}

static int ps5170_switch_set(struct typec_switch_dev *sw,
			enum typec_orientation orientation)
{
	struct ps5170 *ps = typec_switch_get_drvdata(sw);

	dev_info(ps->dev, "%s %d\n", __func__, orientation);

	mutex_lock(&ps->lock);

	switch (orientation) {
	case TYPEC_ORIENTATION_NONE:
		ps5170_set_mode(ps, PS5170_MODE_NONE, true);
		ps->pin_assign = 0;
		ps->is_dp = false;
		break;
	case TYPEC_ORIENTATION_NORMAL:
		if (ps->usb_on)
			ps5170_set_mode(ps, PS5170_MODE_USB_FRONT, false);
		break;
	case TYPEC_ORIENTATION_REVERSE:
		if (ps->usb_on)
			ps5170_set_mode(ps, PS5170_MODE_USB_BACK, false);
		break;
	default:
		break;
	}

	ps->orientation = orientation;
	mutex_unlock(&ps->lock);
	return 0;
}

/*
 * case
 * 4 Pin Assignment C 4-lans
 * 16 Pin Assignment E 4-lans
 * 8 Pin Assignment D 2-lans
 * 32 Pin Assignment F 2-lans
 */

static void ps5170_reconfig_dp_work(struct work_struct *data)
{
	struct ps5170 *ps = container_of(data, struct ps5170, reconfig_dp_work);

	dev_info(ps->dev, "Reconfig DP channel, pin_assign = %d, polarity = %d.\n"
				, ps->pin_assign, ps->orientation);

	mutex_lock(&ps->lock);

	switch (ps->pin_assign) {
	case 4:
	case 16:
		/* Set mode to none first to reset the state */
		ps5170_set_mode(ps, PS5170_MODE_NONE, false);
		ps5170_set_mode(ps, (ps->orientation == TYPEC_ORIENTATION_NORMAL)
			? PS5170_MODE_DP4L_FRONT : PS5170_MODE_DP4L_BACK, false);
		break;
	case 8:
	case 32:
		/* Set mode to none first to reset the state */
		ps5170_set_mode(ps, PS5170_MODE_NONE, false);
		ps5170_set_mode(ps, (ps->orientation == TYPEC_ORIENTATION_NORMAL)
			? PS5170_MODE_DP_FRONT : PS5170_MODE_DP_BACK, false);
		break;
	default:
		dev_info(ps->dev, "%s Pin Assignment not support\n", __func__);
		break;
	}

	mutex_unlock(&ps->lock);
}

static int ps5170_mux_set(struct typec_mux_dev *mux, struct typec_mux_state *state)
{
	struct ps5170 *ps = typec_mux_get_drvdata(mux);
	struct typec_displayport_data *dp_data = state->data;

	if (dp_data == NULL) {
		dev_info(ps->dev, "%s data is NULL, reject.\n", __func__);
		return 0;
	}

	dev_info(ps->dev, "%s %d\n", __func__, dp_data->conf);
	/* dev_info(ps->dev, "dp_data->status = %d\n", dp_data->status); */
	/* dev_info(ps->dev, "state->mode = %lu\n", state->mode); */
	/* dev_info(ps->dev, "ps->orientation = %d\n", ps->orientation); */

	mutex_lock(&ps->lock);

	if (dp_data->conf != 0) {
		switch (dp_data->conf) {
		case 4:
		case 16:
			ps5170_set_mode(ps, (ps->orientation == TYPEC_ORIENTATION_NORMAL)
				? PS5170_MODE_DP4L_FRONT : PS5170_MODE_DP4L_BACK, false);
			break;
		case 8:
		case 32:
			ps5170_set_mode(ps, (ps->orientation == TYPEC_ORIENTATION_NORMAL)
				? PS5170_MODE_DP_FRONT : PS5170_MODE_DP_BACK, false);
			break;
		default:
			dev_info(ps->dev, "%s Pin Assignment not support\n", __func__);
			goto not_support;
		}

		ps->pin_assign = dp_data->conf;
		ps->is_dp = true;
	}

not_support:
	mutex_unlock(&ps->lock);

	return 0;
}

#if IS_ENABLED(CONFIG_DEVICE_MODULES_USB_MTU3)
static int ssusb_power_notifier(struct notifier_block *nb,	unsigned long event, void *data)
{
	struct ps5170 *ps = container_of(nb, struct ps5170, ssusb_nb);

	dev_info(ps->dev, "%s event: %lu, dp: %d\n", __func__, event, ps->is_dp);

	mutex_lock(&ps->lock);

	if (!ps->usb_sync_enabled || ps->is_dp)
		goto skip;

	switch (event) {
	case SSUSB_POWER_EVENT_ON:
		if  (ps->orientation == TYPEC_ORIENTATION_NORMAL)
			ps5170_set_mode(ps, PS5170_MODE_USB_FRONT, true);
		else if (ps->orientation == TYPEC_ORIENTATION_REVERSE)
			ps5170_set_mode(ps, PS5170_MODE_USB_BACK, true);
		ps->usb_on = true;
		break;
	case SSUSB_POWER_EVENT_OFF:
		if (ps->orientation == TYPEC_ORIENTATION_NORMAL ||
			ps->orientation == TYPEC_ORIENTATION_REVERSE)
			ps5170_set_mode(ps, PS5170_MODE_NONE, true);
		ps->usb_on = false;
		break;
	default:
		break;
	}

skip:
	mutex_unlock(&ps->lock);
	return NOTIFY_OK;
}

static int ssusb_power_notifier_init(struct ps5170 *ps)
{
	int ret = 0;

	/* set usb_on to true as default state */
	ps->usb_on = true;

	ps->usb_sync_enabled = of_property_read_bool(ps->dev->of_node, "enable-usb-sync");
	if (!ps->usb_sync_enabled)
		goto done;

	ps->ssusb = ssusb_get_drvdata(ps->dev);
	if (!ps->ssusb) {
		dev_info(ps->dev, "failed to get ssusb drvdata\n");
		ret = -EINVAL;
		goto done;
	}

	ps->ssusb_nb.notifier_call = ssusb_power_notifier;
	ret = ssusb_power_register_notifier(ps->ssusb, &ps->ssusb_nb);
	if (ret < 0) {
		dev_info(ps->dev, "failed to register notifier for ssusb power\n");
		ps->ssusb_nb.notifier_call = NULL;
		goto done;
	}
	/* set usb_on to false if register notifier succeed */
	ps->usb_on = false;
done:
	dev_info(ps->dev, "usb_sync_enabled: %d, ret: %d\n", ps->usb_sync_enabled, ret);
	return ret;
}
#endif

static int ps5170_pinctrl_init(struct ps5170 *ps)
{
	struct device *dev = ps->dev;
	int ret = 0;

	ps->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(ps->pinctrl)) {
		ret = PTR_ERR(ps->pinctrl);
		dev_info(dev, "failed to get pinctrl, ret=%d\n", ret);
		return ret;
	}

	ps->enable =
		pinctrl_lookup_state(ps->pinctrl, "enable");

	if (IS_ERR(ps->enable)) {
		dev_info(dev, "Can *NOT* find enable\n");
		ps->enable = NULL;
	} else
		dev_info(dev, "Find enable\n");

	ps->disable =
		pinctrl_lookup_state(ps->pinctrl, "disable");

	if (IS_ERR(ps->disable)) {
		dev_info(dev, "Can *NOT* find disable\n");
		ps->disable = NULL;
	} else
		dev_info(dev, "Find disable\n");

	return ret;
}

static int ps5170_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct typec_switch_desc sw_desc = { };
	struct typec_mux_desc mux_desc = { };
	struct ps5170 *ps;
	int ret = 0;

	ps = devm_kzalloc(dev, sizeof(*ps), GFP_KERNEL);
	if (!ps)
		return -ENOMEM;

	ps->i2c = client;
	ps->dev = dev;

	atomic_set(&ps->in_sleep, 0);
	ps->pin_assign = 0;
	ps->is_dp = false;

	ret = ps5170_vsvoter_of_property_parse(ps, node);
	if (ret)
		dev_info(dev, "failed to parse vsv property\n");

	ps5170_driving_of_property_parse(ps);

	/* Setting Switch callback */
	sw_desc.drvdata = ps;
	sw_desc.fwnode = dev->fwnode;
	sw_desc.set = ps5170_switch_set;
#if IS_ENABLED(CONFIG_MTK_USB_TYPEC_MUX)
	ps->sw = mtk_typec_switch_register(dev, &sw_desc);
#else
	ps->sw = typec_switch_register(dev, &sw_desc);
#endif
	if (IS_ERR(ps->sw)) {
		dev_info(dev, "error registering typec switch: %ld\n",
			PTR_ERR(ps->sw));
		return PTR_ERR(ps->sw);
	}

	/* Setting MUX callback */
	mux_desc.drvdata = ps;
	mux_desc.fwnode = dev->fwnode;
	mux_desc.set = ps5170_mux_set;
#if IS_ENABLED(CONFIG_MTK_USB_TYPEC_MUX)
	ps->mux = mtk_typec_mux_register(dev, &mux_desc);
#else
	ps->mux = typec_switch_register(dev, &mux_desc);
#endif
	if (IS_ERR(ps->mux)) {
		dev_info(dev, "error registering typec mux: %ld\n",
			PTR_ERR(ps->mux));
		return PTR_ERR(ps->mux);
	}

	i2c_set_clientdata(client, ps);

	ret = ps5170_pinctrl_init(ps);
	if (ret < 0) {
#if IS_ENABLED(CONFIG_MTK_USB_TYPEC_MUX)
		mtk_typec_switch_unregister(ps->sw);
#else
		mtk_typec_switch_unregister(ps->sw);
#endif
		return ret;
	}

#if IS_ENABLED(CONFIG_DEVICE_MODULES_USB_MTU3)
	ssusb_power_notifier_init(ps);
#endif

	ps->wq = create_singlethread_workqueue("ps5170_wq");
	if (!ps->wq)
		return -ENOMEM;

	mutex_init(&ps->lock);
	INIT_WORK(&ps->reconfig_dp_work, ps5170_reconfig_dp_work);

	/* switch off after init done */
	ps5170_switch_set(ps->sw, TYPEC_ORIENTATION_NONE);
	dev_info(dev, "probe done\n");
	return ret;
}

static void ps5170_remove(struct i2c_client *client)
{
	struct ps5170 *ps = i2c_get_clientdata(client);

#if IS_ENABLED(CONFIG_DEVICE_MODULES_USB_MTU3)
	if (ps->ssusb && ps->ssusb_nb.notifier_call)
		ssusb_power_unregister_notifier(ps->ssusb, &ps->ssusb_nb);
#endif

	mtk_typec_switch_unregister(ps->sw);
	typec_mux_unregister(ps->mux);
	/* typec_switch_unregister(pi->sw); */
}

static int __maybe_unused ps5170_suspend(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);

	if (device_may_wakeup(dev))
		enable_irq_wake(i2c->irq);
	return 0;
}

static int __maybe_unused ps5170_resume(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);

	if (device_may_wakeup(dev))
		disable_irq_wake(i2c->irq);
	return 0;
}

static int ps5170_suspend_noirq(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct ps5170 *ps = i2c_get_clientdata(i2c);

	atomic_set(&ps->in_sleep, 1);

	/* pull low en pin to enter deep idle mode */
	pinctrl_select_state(ps->pinctrl, ps->disable);
	return 0;
}

static int ps5170_resume_noirq(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct ps5170 *ps = i2c_get_clientdata(i2c);

	atomic_set(&ps->in_sleep, 0);

	if (ps->pin_assign) {
		schedule_work(&ps->reconfig_dp_work);
	} else {
		/* pull high en pin to enter normal mode if connected */
		if (ps->usb_on && (ps->orientation == TYPEC_ORIENTATION_NORMAL ||
			ps->orientation == TYPEC_ORIENTATION_REVERSE))
			pinctrl_select_state(ps->pinctrl, ps->enable);
		else
			pinctrl_select_state(ps->pinctrl, ps->disable);
	}
	return 0;
}

static const struct dev_pm_ops ps5170_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ps5170_suspend, ps5170_resume)
		SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(ps5170_suspend_noirq,
			ps5170_resume_noirq)
};

static const struct i2c_device_id ps5170_table[] = {
	{ "ps5170" },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ps5170_table);

static const struct of_device_id ps5170_of_match[] = {
	{.compatible = "parade,ps5170"},
	{ },
};
MODULE_DEVICE_TABLE(of, ps5170_of_match);

static struct i2c_driver ps5170_driver = {
	.driver = {
		.name = "ps5170",
		.pm = &ps5170_pm_ops,
		.of_match_table = ps5170_of_match,
	},
	.probe = ps5170_probe,
	.remove	= ps5170_remove,
	.id_table = ps5170_table,
};
module_i2c_driver(ps5170_driver);

MODULE_DESCRIPTION("ps5170 Type-C Redriver");
MODULE_LICENSE("GPL v2");
