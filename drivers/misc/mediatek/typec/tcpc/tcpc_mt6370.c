// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pm_runtime.h>
#include <linux/pm_wakeup.h>
#include <linux/property.h>
#include <linux/regmap.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include <uapi/linux/sched/types.h>

#include "inc/pd_dbg_info.h"
#include "inc/tcpci.h"
#include "inc/mt6370.h"

#include <linux/sched/rt.h>

/* #define DEBUG_GPIO	66 */
#define DEBUG_GPIO 0

#define MT6370_DRV_VERSION	"1.0.1_U_MTK"

struct mt6370_tcpc_data {
	struct i2c_client *client;
	struct device *dev;
	struct regmap *rmap;
	struct tcpc_desc *tcpc_desc;
	struct tcpc_device *tcpc;
	struct kthread_worker irq_worker;
	struct kthread_work irq_work;
	struct task_struct *irq_worker_task;
	struct gpio_desc *irq_gpio;
	struct mutex irq_lock;
	int irq;
	int did;
	bool is_suspended;
	bool irq_while_suspended;
};

static inline int mt6370_write8(struct mt6370_tcpc_data *ddata, u32 reg, u8 data)
{
	return regmap_write(ddata->rmap, reg, data);
}

static inline int mt6370_write16(struct mt6370_tcpc_data *ddata, u32 reg, u16 data)
{
	data = cpu_to_le16(data);
	return regmap_bulk_write(ddata->rmap, reg, &data, 2);
}

static inline int mt6370_read8(struct mt6370_tcpc_data *ddata, u32 reg, u8 *data)
{
	int ret;
	u32 _data = 0;

	ret = regmap_read(ddata->rmap, reg, &_data);
	if (ret < 0)
		return ret;

	*data = _data;
	return 0;
}

static inline int mt6370_read16(struct mt6370_tcpc_data *ddata, u32 reg, u16 *data)
{
	int ret;

	ret = regmap_bulk_read(ddata->rmap, reg, data, 2);
	if (ret)
		return ret;

	*data = le16_to_cpu(*data);
	return 0;
}

static inline int mt6370_bulk_write(struct mt6370_tcpc_data *ddata, u32 reg,
				    const void *data, size_t count)
{
	return regmap_bulk_write(ddata->rmap, reg, data, count);
}


static inline int mt6370_bulk_read(struct mt6370_tcpc_data *ddata, u32 reg,
				   void *data, size_t count)
{
	return regmap_bulk_read(ddata->rmap, reg, data, count);
}

static inline int mt6370_update_bits(struct mt6370_tcpc_data *ddata, u32 reg,
				     u8 mask, u8 data)
{
	return regmap_update_bits(ddata->rmap, reg, mask, data);
}

static inline int mt6370_set_bits(struct mt6370_tcpc_data *ddata, u32 reg,
				  u8 mask)
{
	return mt6370_update_bits(ddata, reg, mask, mask);
}

static inline int mt6370_clr_bits(struct mt6370_tcpc_data *ddata, u32 reg,
				  u8 mask)
{
	return mt6370_update_bits(ddata, reg, mask, 0);
}

static inline int mt6370_software_reset(struct mt6370_tcpc_data *ddata)
{
	int ret;

	ret = mt6370_write8(ddata, MT6370_REG_SWRESET, 1);
	if (ret)
		return ret;

	usleep_range(1000, 2000);
	return 0;
}

static inline int mt6370_command(struct mt6370_tcpc_data *ddata, u8 cmd)
{
	return mt6370_write8(ddata, TCPC_V10_REG_COMMAND, cmd);
}

static int mt6370_init_alert_mask(struct mt6370_tcpc_data *ddata)
{
	u16 mask = TCPC_V10_REG_ALERT_CC_STATUS | TCPC_V10_REG_ALERT_POWER_STATUS;

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
	/* Need to handle RX overflow */
	mask |= TCPC_V10_REG_ALERT_TX_SUCCESS | TCPC_V10_REG_ALERT_TX_DISCARDED
			| TCPC_V10_REG_ALERT_TX_FAILED
			| TCPC_V10_REG_ALERT_RX_HARD_RST
			| TCPC_V10_REG_ALERT_RX_STATUS
			| TCPC_V10_REG_RX_OVERFLOW;
#endif

	mask |= TCPC_REG_ALERT_FAULT;

	return mt6370_write16(ddata, TCPC_V10_REG_ALERT_MASK, mask);
}

static inline int mt6370_init_power_status_mask(struct mt6370_tcpc_data *ddata)
{
	return mt6370_write8(ddata, TCPC_V10_REG_POWER_STATUS_MASK,
			     TCPC_V10_REG_POWER_STATUS_VBUS_PRES);
}

static inline int mt6370_init_fault_mask(struct mt6370_tcpc_data *ddata)
{
	return mt6370_write8(ddata, TCPC_V10_REG_FAULT_STATUS_MASK,
			     TCPC_V10_REG_FAULT_STATUS_VCONN_OV |
			     TCPC_V10_REG_FAULT_STATUS_VCONN_OC);
}

static inline int mt6370_init_mt_mask(struct mt6370_tcpc_data *ddata)
{
	return mt6370_write8(ddata, MT6370_REG_MT_MASK,
			     MT6370_REG_M_WAKEUP | MT6370_REG_M_VBUS_80);
}

static void mt6370_irq_work_handler(struct kthread_work *work)
{
	struct mt6370_tcpc_data *ddata = container_of(work,
						      struct mt6370_tcpc_data,
						      irq_work);
	int ret = 0, gpio_val;

	MT6370_INFO("++\n");
	/* make sure I2C bus had resumed */
	/* reinit_completion(&ddata->tcpc->alert_done); */
	tcpci_lock_typec(ddata->tcpc);

	do {
		ret = tcpci_alert(ddata->tcpc);
		if (ret < 0)
			break;

		gpio_val = gpiod_get_value(ddata->irq_gpio);
	} while (gpio_val == 0);

	tcpci_unlock_typec(ddata->tcpc);

	/* complete(&ddata->tcpc->alert_done); */
	pm_relax(ddata->dev);
	MT6370_INFO("--\n");
}

static irqreturn_t mt6370_intr_handler(int irq, void *data)
{
	struct mt6370_tcpc_data *ddata = data;

	MT6370_INFO("++\n");

	mutex_lock(&ddata->irq_lock);
	if (ddata->is_suspended) {
		dev_notice(ddata->dev, "%s irq while suspended\n", __func__);
		ddata->irq_while_suspended = true;
		disable_irq_nosync(ddata->irq);
		mutex_unlock(&ddata->irq_lock);
		return IRQ_NONE;
	}

	mutex_unlock(&ddata->irq_lock);

	pm_stay_awake(ddata->dev);
	kthread_queue_work(&ddata->irq_worker, &ddata->irq_work);
	return IRQ_HANDLED;
}

static int mt6370_init_alert(struct mt6370_tcpc_data *ddata)
{
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };
	int ret;

	dev_info(ddata->dev, "%s, ++\n", __func__);

	/* Clear Alert Mask & Status */
	mt6370_write16(ddata, TCPC_V10_REG_ALERT_MASK, 0);
	mt6370_write16(ddata, TCPC_V10_REG_ALERT, 0xffff);

	ddata->irq_gpio = devm_gpiod_get(ddata->dev, "mt6370pd-intr", GPIOD_IN);
#if DEBUG_GPIO
	gpio_request(DEBUG_GPIO, "debug_latency_pin");
	gpio_direction_output(DEBUG_GPIO, 1);
#endif
	if (IS_ERR(ddata->irq_gpio)) {
		dev_err(ddata->dev, "%s, Failed to get gpio from irq\n", __func__);
		return PTR_ERR(ddata->irq_gpio);
	}

	ret = gpiod_direction_input(ddata->irq_gpio);
	if (ret) {
		dev_err(ddata->dev, "%s, Failed to set GPIO%d as input pin(%d)\n",
			__func__, desc_to_gpio(ddata->irq_gpio), ret);
		return ret;
	}

	ret = gpiod_to_irq(ddata->irq_gpio);
	if (ddata->irq < 0) {
		dev_err(ddata->dev, "Failed to get irq from gpio(%d)\n", ret);
		return ret;
	}

	ddata->irq = ret;

	dev_info(ddata->dev, "%s, IRQ number = %d, GPIO number = %d\n",
		 __func__, ddata->irq, desc_to_gpio(ddata->irq_gpio));

	kthread_init_worker(&ddata->irq_worker);
	ddata->irq_worker_task = kthread_run(kthread_worker_fn, &ddata->irq_worker, "%s",
					     ddata->tcpc_desc->name);
	if (IS_ERR(ddata->irq_worker_task)) {
		dev_err(ddata->dev, "%s, Failed to create tcpc task\n", __func__);
		return -EINVAL;
	}

	sched_setscheduler(ddata->irq_worker_task, SCHED_FIFO, &param);
	kthread_init_work(&ddata->irq_work, mt6370_irq_work_handler);

	device_init_wakeup(ddata->dev, true);
	ret = devm_request_threaded_irq(ddata->dev, ddata->irq, NULL, mt6370_intr_handler,
					IRQF_TRIGGER_LOW | IRQF_ONESHOT,
					dev_name(ddata->dev), ddata);
	if (ret) {
		dev_err(ddata->dev, "%s, Failed to request irq%d (ret = %d)\n",
			__func__, ddata->irq, ret);
		return -EINVAL;
	}

	enable_irq_wake(ddata->irq);
	return 0;
}

static int mt6370_alert_status_clear(struct tcpc_device *tcpc, u32 mask)
{
	struct mt6370_tcpc_data *ddata = tcpc_get_dev_data(tcpc);
	int ret = 0;
	u16 mask_t1;
	u8 mask_t2;

	/* Write 1 clear */
	mask_t1 = (u16)mask;
	if (mask_t1) {
		ret = mt6370_write16(ddata, TCPC_V10_REG_ALERT, mask_t1);
		if (ret) {
			dev_err(ddata->dev, "%s, Failed to write TCPC_V10_REG_ALERT\n", __func__);
			return ret;
		}
	}

	mask_t2 = mask >> 16;
	if (mask_t2) {
		ret = mt6370_write8(ddata, MT6370_REG_MT_INT, mask_t2);
		if (ret)
			dev_err(ddata->dev, "%s, Failed to write MT6370_REG_MT_INT\n", __func__);
	}

	return ret;
}

static int mt6370_set_clock_gating(struct mt6370_tcpc_data *ddata, bool en)
{
	int ret;

#if CONFIG_TCPC_CLOCK_GATING
	u8 clk2 = MT6370_REG_CLK_DIV_600K_EN | MT6370_REG_CLK_DIV_300K_EN |
		  MT6370_REG_CLK_CK_300K_EN;
	u8 clk3 = MT6370_REG_CLK_DIV_2P4M_EN;
	int i = 0;

	if (en) {
		for (i = 0; i < 2; i++) {
			ret = mt6370_alert_status_clear(ddata->tcpc, TCPC_REG_ALERT_RX_ALL_MASK);
			if (ret)
				return ret;
		}
	} else {
		clk2 |= MT6370_REG_CLK_BCLK2_EN | MT6370_REG_CLK_BCLK_EN;
		clk3 |= MT6370_REG_CLK_CK_24M_EN | MT6370_REG_CLK_PCLK_EN;
	}

	ret = mt6370_write8(ddata, MT6370_REG_CLK_CTRL2, clk2);
	if (ret) {
		dev_err(ddata->dev, "%s, Failed to write MT6370_REG_CLK_CTRL2\n", __func__);
		return ret;
	}

	ret = mt6370_write8(ddata, MT6370_REG_CLK_CTRL3, clk3);
	if (ret)
		dev_err(ddata->dev, "%s, Failed to write MT6370_REG_CLK_CTRL3\n", __func__);
#endif	/* CONFIG_TCPC_CLOCK_GATING */

	return ret;
}

static inline int mt6370_init_cc_params(struct mt6370_tcpc_data *ddata, u8 cc_res)
{
	int ret = 0;

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
#if CONFIG_USB_PD_SNK_DFT_NO_GOOD_CRC
	u8 en, sel;

	if (cc_res == TYPEC_CC_VOLT_SNK_DFT) { /* 0.55 */
		en = 1;
		sel = 0x81;
	} else { /* 0.4 & 0.7 */
		en = 0;
		sel = 0x80;
	}

	ret = mt6370_write8(ddata, MT6370_REG_BMCIO_RXDZEN, en);
	if (ret) {
		dev_err(ddata->dev, "%s, Failed to write MT6370_REG_BMCIO_RXDZEN\n", __func__);
		return ret;
	}

	ret = mt6370_write8(ddata, MT6370_REG_BMCIO_RXDZSEL, sel);
	if (ret)
		dev_err(ddata->dev, "%s, Failed to write MT6370_REG_BMCIO_RXDZSE:\n", __func__);
#endif	/* CONFIG_USB_PD_SNK_DFT_NO_GOOD_CRC */
#endif	/* CONFIG_USB_POWER_DELIVERY */

	return ret;
}

static int mt6370_tcpc_init(struct tcpc_device *tcpc, bool sw_reset)
{
	struct mt6370_tcpc_data *ddata = tcpc_get_dev_data(tcpc);
	bool retry_discard_old = false;
	int ret;

	MT6370_INFO("\n");

	if (sw_reset) {
		ret = mt6370_software_reset(ddata);
		if (ret)
			return ret;
	}

	/* For No-GoodCRC Case (0x70) */
	ret = mt6370_write8(ddata, MT6370_REG_PHY_CTRL2, 0x38);
	if (ret) {
		dev_err(ddata->dev, "%s, Failed to write MT6370_REG_PHY_CTRL2\n", __func__);
		return ret;
	}

	ret = mt6370_write8(ddata, MT6370_REG_PHY_CTRL3, 0x82);
	if (ret) {
		dev_err(ddata->dev, "%s, Failed to write MT6370_REG_PHY_CTRL3\n", __func__);
		return ret;
	}

	ret = mt6370_write8(ddata, MT6370_REG_PHY_CTRL11, 0xfc);
	if (ret) {
		dev_err(ddata->dev, "%s, Failed to write MT6370_REG_PHY_CTRL11\n", __func__);
		return ret;
	}

	ret = mt6370_write8(ddata, MT6370_REG_PHY_CTRL12, 0x50);
	if (ret) {
		dev_err(ddata->dev, "%s, Failed to write MT6370_REG_PHY_CTRL12\n", __func__);
		return ret;
	}

#if CONFIG_TCPC_I2CRST_EN
	ret = mt6370_write8(ddata, MT6370_REG_I2CRST_CTRL, MT6370_REG_I2CRST_SET(true, 0x0f));
	if (ret) {
		dev_err(ddata->dev, "%s, Failed to write MT6370_REG_I2CRST_CTRL\n", __func__);
		return ret;
	}
#endif	/* CONFIG_TCPC_I2CRST_EN */

	/* UFP Both RD setting */
	/* DRP = 0, RpVal = 0 (Default), Rd, Rd */
	ret = mt6370_write8(ddata, TCPC_V10_REG_ROLE_CTRL,
			    TCPC_V10_REG_ROLE_CTRL_RES_SET(0, 0, CC_RD, CC_RD));
	if (ret) {
		dev_err(ddata->dev, "%s, Failed to write TCPC_V10_REG_ROLE_CTRL\n", __func__);
		return ret;
	}

	if (ddata->did == MT6370_DID_A) {
		ret = mt6370_write8(ddata, TCPC_V10_REG_FAULT_CTRL,
				    TCPC_V10_REG_FAULT_CTRL_DIS_VCONN_OV);
		if (ret) {
			dev_err(ddata->dev, "%s, Failed to write TCPC_V10_REG_FAULT_CTRL\n",
				__func__);
			return ret;
		}
	}

	/*
	 * CC Detect Debounce : 26.7*val us
	 * Transition window count : spec 12~20us, based on 2.4MHz
	 * DRP Toggle Cycle : 51.2 + 6.4*val ms
	 * DRP Duty Ctrl : dcSRC / 1024
	 */

	ret = mt6370_write8(ddata, MT6370_REG_TCPC_FILTER, 10);
	if (ret) {
		dev_err(ddata->dev, "%s, Failed to write TCPC_FILTER\n", __func__);
		return ret;
	}

	ret = mt6370_write8(ddata, MT6370_REG_DRP_TOGGLE_CYCLE, 0);
	if (ret) {
		dev_err(ddata->dev, "%s, Failed to write DRP_TOGGLE_CYCLE\n", __func__);
		return ret;
	}

	ret = mt6370_write16(ddata, MT6370_REG_DRP_DUTY_CTRL, TCPC_NORMAL_RP_DUTY);
	if (ret) {
		dev_err(ddata->dev, "%s, Failed to write TCPC_NORMAL_RP_DUTY\n", __func__);
		return ret;
	}

	/* RX/TX Clock Gating (Auto Mode)*/
	if (!sw_reset)
		mt6370_set_clock_gating(ddata, true);

	if (!(tcpc->tcpc_flags & TCPC_FLAGS_RETRY_CRC_DISCARD))
		retry_discard_old = true;

	/* For BIST, Change Transition Toggle Counter (Noise) from 3 to 7 */
	ret = mt6370_write8(ddata, MT6370_REG_PHY_CTRL1,
			    MT6370_REG_PHY_CTRL1_SET(retry_discard_old, 7, 0, 1));
	if (ret)
		return ret;


	tcpci_alert_status_clear(tcpc, 0xffffffff);

	mt6370_init_power_status_mask(ddata);
	mt6370_init_alert_mask(ddata);
	mt6370_init_fault_mask(ddata);
	mt6370_init_mt_mask(ddata);

	/* CK_300K from 320K, SHIPPING off, AUTOIDLE enable, TIMEOUT = 6.4ms */
	ret = mt6370_write8(ddata, MT6370_REG_IDLE_CTRL,
			    MT6370_REG_IDLE_SET(0, 1, 1, 0));
	if (ret)
		return ret;

	mdelay(1);

	return 0;
}

static inline int mt6370_fault_status_vconn_ov(struct mt6370_tcpc_data *ddata)
{
	u8 status;
	int ret;

	ret = mt6370_read8(ddata, MT6370_REG_BMC_CTRL, &status);
	if (ret)
		return ret;

	status &= ~MT6370_REG_DISCHARGE_EN;
	return mt6370_write8(ddata, MT6370_REG_BMC_CTRL, status);
}

static inline int mt6370_fault_status_vconn_oc(struct mt6370_tcpc_data *ddata)
{
	return mt6370_write8(ddata, TCPC_V10_REG_FAULT_STATUS_MASK,
			     TCPC_V10_REG_FAULT_STATUS_VCONN_OV);
}

static int mt6370_fault_status_clear(struct tcpc_device *tcpc, u8 status)
{
	struct mt6370_tcpc_data *ddata = tcpc_get_dev_data(tcpc);
	int ret;

	if (status & TCPC_V10_REG_FAULT_STATUS_VCONN_OV) {
		ret = mt6370_fault_status_vconn_ov(ddata);
		if (ret)
			return ret;
	}

	if (status & TCPC_V10_REG_FAULT_STATUS_VCONN_OC) {
		ret = mt6370_fault_status_vconn_oc(ddata);
		if (ret)
			return ret;
	}

	return mt6370_write8(ddata, TCPC_V10_REG_FAULT_STATUS, status);
}

static int mt6370_get_alert_mask(struct tcpc_device *tcpc, u32 *mask)
{
	struct mt6370_tcpc_data *ddata = tcpc_get_dev_data(tcpc);
	u16 alert_mask;
	int ret;
	u8 v2;

	ret = mt6370_read16(ddata, TCPC_V10_REG_ALERT_MASK, &alert_mask);
	if (ret)
		return ret;

	*mask = alert_mask;

	ret = mt6370_read8(ddata, MT6370_REG_MT_MASK, &v2);
	if (ret)
		return ret;

	*mask |= v2 << 16;
	return 0;
}

static int mt6370_get_alert_status(struct tcpc_device *tcpc, u32 *alert)
{
	struct mt6370_tcpc_data *ddata = tcpc_get_dev_data(tcpc);
	u16 alert_val;
	int ret;
	u8 v2;

	ret = mt6370_read16(ddata, TCPC_V10_REG_ALERT, &alert_val);
	if (ret)
		return ret;

	*alert = alert_val;

	ret = mt6370_read8(ddata, MT6370_REG_MT_INT, &v2);
	if (ret)
		return ret;

	*alert |= v2 << 16;
	return 0;
}

static int mt6370_get_power_status(struct tcpc_device *tcpc, u16 *pwr_status)
{
	struct mt6370_tcpc_data *ddata = tcpc_get_dev_data(tcpc);
	u8 status;
	int ret;

	ret = mt6370_read8(ddata, TCPC_V10_REG_POWER_STATUS, &status);
	if (ret)
		return ret;

	*pwr_status = 0;

	if (status & TCPC_V10_REG_POWER_STATUS_VBUS_PRES)
		*pwr_status |= TCPC_REG_POWER_STATUS_VBUS_PRES;

	ret = mt6370_read8(ddata, MT6370_REG_MT_STATUS, &status);
	if (ret)
		return ret;

	if (status & MT6370_REG_VBUS_80)
		*pwr_status |= TCPC_REG_POWER_STATUS_EXT_VSAFE0V;

	return 0;
}

static int mt6370_get_fault_status(struct tcpc_device *tcpc, u8 *status)
{
	struct mt6370_tcpc_data *ddata = tcpc_get_dev_data(tcpc);

	return mt6370_read8(ddata, TCPC_V10_REG_FAULT_STATUS, status);
}

static int mt6370_get_cc(struct tcpc_device *tcpc, int *cc1, int *cc2)
{
	struct mt6370_tcpc_data *ddata = tcpc_get_dev_data(tcpc);
	u8 status, role_ctrl, cc_role;
	bool act_as_sink, act_as_drp;
	int ret;

	ret = mt6370_read8(ddata, TCPC_V10_REG_CC_STATUS, &status);
	if (ret)
		return ret;

	ret = mt6370_read8(ddata, TCPC_V10_REG_ROLE_CTRL, &role_ctrl);
	if (ret)
		return ret;

	if (status & TCPC_V10_REG_CC_STATUS_DRP_TOGGLING) {
		*cc1 = TYPEC_CC_DRP_TOGGLING;
		*cc2 = TYPEC_CC_DRP_TOGGLING;
		return 0;
	}

	*cc1 = TCPC_V10_REG_CC_STATUS_CC1(status);
	*cc2 = TCPC_V10_REG_CC_STATUS_CC2(status);

	act_as_drp = TCPC_V10_REG_ROLE_CTRL_DRP & role_ctrl;

	if (act_as_drp) {
		act_as_sink = TCPC_V10_REG_CC_STATUS_DRP_RESULT(status);
	} else {
		if (tcpc->typec_polarity)
			cc_role = TCPC_V10_REG_CC_STATUS_CC2(role_ctrl);
		else
			cc_role = TCPC_V10_REG_CC_STATUS_CC1(role_ctrl);

		if (cc_role == TYPEC_CC_RP)
			act_as_sink = false;
		else
			act_as_sink = true;
	}

	/*
	 * If status is not open, then OR in termination to convert to
	 * enum tcpc_cc_voltage_status.
	 */
	if (*cc1 != TYPEC_CC_VOLT_OPEN)
		*cc1 |= (act_as_sink << 2);

	if (*cc2 != TYPEC_CC_VOLT_OPEN)
		*cc2 |= (act_as_sink << 2);

	return mt6370_init_cc_params(ddata, (u8)tcpc->typec_polarity ? *cc2 : *cc1);
}

static int mt6370_enable_vsafe0v_detect(struct mt6370_tcpc_data *ddata, bool enable)
{
	u8 status;
	int ret;

	ret = mt6370_read8(ddata, MT6370_REG_MT_MASK, &status);
	if (ret)
		return ret;

	if (enable)
		status |= MT6370_REG_M_VBUS_80;
	else
		status &= ~MT6370_REG_M_VBUS_80;

	return mt6370_write8(ddata, MT6370_REG_MT_MASK, status);
}

static int mt6370_set_cc(struct tcpc_device *tcpc, int pull)
{
	int rp_lvl = TYPEC_CC_PULL_GET_RP_LVL(pull), pull1, pull2, ret;
	struct mt6370_tcpc_data *ddata = tcpc_get_dev_data(tcpc);
	u8 data;

	MT6370_INFO("pull = 0x%02X\n", pull);
	pull = TYPEC_CC_PULL_GET_RES(pull);
	if (pull == TYPEC_CC_DRP) {
		data = TCPC_V10_REG_ROLE_CTRL_RES_SET(1, rp_lvl, TYPEC_CC_RD, TYPEC_CC_RD);

		ret = mt6370_write8(ddata, TCPC_V10_REG_ROLE_CTRL, data);
		if (ret)
			return ret;

		mt6370_enable_vsafe0v_detect(ddata, false);
		ret = mt6370_command(ddata, TCPM_CMD_LOOK_CONNECTION);
	} else {
#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
		if (pull == TYPEC_CC_RD && tcpc->pd_wait_pr_swap_complete)
			mt6370_init_cc_params(ddata, TYPEC_CC_VOLT_SNK_DFT);
#endif	/* CONFIG_USB_POWER_DELIVERY */

		pull1 = pull2 = pull;

		if (pull == TYPEC_CC_RP && tcpc->typec_is_attached_src) {
			if (tcpc->typec_polarity)
				pull1 = TYPEC_CC_OPEN;
			else
				pull2 = TYPEC_CC_OPEN;
		}

		data = TCPC_V10_REG_ROLE_CTRL_RES_SET(0, rp_lvl, pull1, pull2);
		ret = mt6370_write8(ddata, TCPC_V10_REG_ROLE_CTRL, data);
	}

	return ret;
}

static int mt6370_set_polarity(struct tcpc_device *tcpc, int polarity)
{
	struct mt6370_tcpc_data *ddata = tcpc_get_dev_data(tcpc);
	u8 tcpc_ctrl;
	int ret;

	if (polarity < 0 || polarity > 1)
		return -EOVERFLOW;

	ret = mt6370_init_cc_params(ddata, tcpc->typec_remote_cc[polarity]);
	if (ret)
		return ret;

	ret = mt6370_read8(ddata, TCPC_V10_REG_TCPC_CTRL, &tcpc_ctrl);
	if (ret)
		return ret;

	tcpc_ctrl &= ~TCPC_V10_REG_TCPC_CTRL_PLUG_ORIENT;
	tcpc_ctrl |= polarity ? TCPC_V10_REG_TCPC_CTRL_PLUG_ORIENT : 0;

	return mt6370_write8(ddata, TCPC_V10_REG_TCPC_CTRL, tcpc_ctrl);
}

static int mt6370_set_vconn(struct tcpc_device *tcpc, int enable)
{
	struct mt6370_tcpc_data *ddata = tcpc_get_dev_data(tcpc);
	u8 data;
	int ret;

	ret = mt6370_read8(ddata, TCPC_V10_REG_POWER_CTRL, &data);
	if (ret)
		return ret;

	data &= ~TCPC_V10_REG_POWER_CTRL_VCONN;
	data |= enable ? TCPC_V10_REG_POWER_CTRL_VCONN : 0;

	ret = mt6370_write8(ddata, TCPC_V10_REG_POWER_CTRL, data);
	if (ret)
		return ret;

	if (enable)
		mt6370_init_fault_mask(ddata);

	return ret;
}

static int mt6370_is_vsafe0v(struct tcpc_device *tcpc)
{
	struct mt6370_tcpc_data *ddata = tcpc_get_dev_data(tcpc);
	u8 data;
	int ret;

	ret = mt6370_read8(ddata, MT6370_REG_MT_STATUS, &data);
	if (ret)
		return ret;

	return (data & MT6370_REG_VBUS_80) != 0;
}

static int mt6370_set_low_power_mode(struct tcpc_device *tcpc, bool en, int pull)
{
	struct mt6370_tcpc_data *ddata = tcpc_get_dev_data(tcpc);
	int ret;
	u8 data;

	ret = mt6370_write8(ddata, MT6370_REG_IDLE_CTRL, MT6370_REG_IDLE_SET(0, 1, en ? 0 : 1, 0));
	if (ret)
		return ret;

	ret = mt6370_enable_vsafe0v_detect(ddata, !en);
	if (ret < 0)
		return ret;

	if (en) {
		data = MT6370_REG_BMCIO_LPEN;

		if (TYPEC_CC_PULL_GET_RES(pull) == TYPEC_CC_RP)
			data |= MT6370_REG_BMCIO_LPRPRD;

#if CONFIG_TYPEC_CAP_NORP_SRC
		data |= MT6370_REG_BMCIO_BG_EN | MT6370_REG_VBUS_DET_EN;
#endif	/* CONFIG_TYPEC_CAP_NORP_SRC */
	} else
		data = MT6370_REG_BMCIO_BG_EN | MT6370_REG_VBUS_DET_EN | MT6370_REG_BMCIO_OSC_EN;

	return mt6370_write8(ddata, MT6370_REG_BMC_CTRL, data);
}

static int mt6370_tcpc_deinit(struct tcpc_device *tcpc)
{
	struct mt6370_tcpc_data *ddata = tcpc_get_dev_data(tcpc);
	int ret = 0;

#if CONFIG_TCPC_SHUTDOWN_CC_DETACH
	mt6370_set_cc(tcpc, TYPEC_CC_OPEN);

	ret = mt6370_write8(ddata, MT6370_REG_I2CRST_CTRL,
			    MT6370_REG_I2CRST_SET(true, 4));
	if (ret)
		return ret;
#else
	ret = mt6370_write8(ddata, MT6370_REG_SWRESET, 1);

#endif	/* CONFIG_TCPC_SHUTDOWN_CC_DETACH */

	return ret;
}

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
static int mt6370_set_msg_header(struct tcpc_device *tcpc, u8 power_role, u8 data_role)
{
	struct mt6370_tcpc_data *ddata = tcpc_get_dev_data(tcpc);

	return mt6370_write8(ddata, TCPC_V10_REG_MSG_HDR_INFO,
			     TCPC_V10_REG_MSG_HDR_INFO_SET(data_role, power_role));
}

static int mt6370_protocol_reset(struct tcpc_device *tcpc)
{
	struct mt6370_tcpc_data *ddata = tcpc_get_dev_data(tcpc);
	int ret;

	ret = mt6370_write8(ddata, MT6370_REG_PRL_FSM_RESET, 0);
	if (ret)
		return ret;

	mdelay(1);

	return mt6370_write8(ddata, MT6370_REG_PRL_FSM_RESET, 1);
}

static int mt6370_set_rx_enable(struct tcpc_device *tcpc, u8 enable)
{
	struct mt6370_tcpc_data *ddata = tcpc_get_dev_data(tcpc);
	int ret = 0;

	if (enable) {
		ret = mt6370_set_clock_gating(ddata, false);
		if (ret)
			return ret;
	}

	ret = mt6370_write8(ddata, TCPC_V10_REG_RX_DETECT, enable);
	if (ret)
		return ret;

	if (!enable) {
		mt6370_protocol_reset(tcpc);
		ret = mt6370_set_clock_gating(ddata, true);
	}

	return ret;
}

static int mt6370_get_message(struct tcpc_device *tcpc, u32 *payload, u16 *msg_head,
			      enum tcpm_transmit_type *frame_type)
{
	struct mt6370_tcpc_data *ddata = tcpc_get_dev_data(tcpc);
	u8 cnt = 0, buf[4];
	int ret;

	ret = mt6370_bulk_read(ddata, TCPC_V10_REG_RX_BYTE_CNT, buf, 4);
	if (ret)
		return ret;

	cnt = buf[0];
	*frame_type = buf[1];
	*msg_head = le16_to_cpu(*(u16 *)&buf[2]);

	MT6370_INFO("Count is %d\n", cnt);
	MT6370_INFO("FrameType is %d\n", *frame_type);
	MT6370_INFO("MessageType is %d\n", PD_HEADER_TYPE(*msg_head));

	/* TCPC 1.0 ==> no need to subtract the size of msg_head */
	if (cnt > 3) {
		cnt -= 3; /* MSG_HDR */
		ret = mt6370_bulk_read(ddata, TCPC_V10_REG_RX_DATA, (u8 *)payload, cnt);
	}

	return ret;
}

static int mt6370_set_bist_carrier_mode(struct tcpc_device *tcpc, u8 pattern)
{
	/* Don't support this function */
	return 0;
}

/* transmit count (1byte) + message header (2byte) + data object (7*4) */
#define MT6370_TRANSMIT_MAX_SIZE (1 + sizeof(u16) + sizeof(u32) * 7)

#if CONFIG_USB_PD_RETRY_CRC_DISCARD
static int mt6370_retransmit(struct tcpc_device *tcpc)
{
	struct mt6370_tcpc_data *ddata = tcpc_get_dev_data(tcpc);

	return mt6370_write8(ddata, TCPC_V10_REG_TRANSMIT,
			     TCPC_V10_REG_TRANSMIT_SET(tcpc->pd_retry_count, TCPC_TX_SOP));
}
#endif

static int mt6370_transmit(struct tcpc_device *tcpc, enum tcpm_transmit_type type,
			   u16 header, const u32 *data)
{
	struct mt6370_tcpc_data *ddata = tcpc_get_dev_data(tcpc);
	u8 temp[MT6370_TRANSMIT_MAX_SIZE];
	int ret, data_cnt, packet_cnt;

	MT6370_INFO("%s, ++\n", __func__);
	if (type < TCPC_TX_HARD_RESET) {
		data_cnt = sizeof(u32) * PD_HEADER_CNT(header);
		packet_cnt = data_cnt + sizeof(u16);

		temp[0] = packet_cnt;
		memcpy(temp + 1, (u8 *)&header, 2);
		if (data_cnt > 0)
			memcpy(temp+3, (u8 *)data, data_cnt);

		ret = mt6370_bulk_write(ddata, TCPC_V10_REG_TX_BYTE_CNT,
					(u8 *)temp, packet_cnt + 1);
		if (ret)
			return ret;
	}

	return mt6370_write8(ddata, TCPC_V10_REG_TRANSMIT,
			     TCPC_V10_REG_TRANSMIT_SET(tcpc->pd_retry_count, type));
}

static int mt6370_set_bist_test_mode(struct tcpc_device *tcpc, bool en)
{
	struct mt6370_tcpc_data *ddata = tcpc_get_dev_data(tcpc);
	u8 data;
	int ret;

	ret = mt6370_read8(ddata, TCPC_V10_REG_TCPC_CTRL, &data);
	if (ret)
		return ret;

	data &= ~TCPC_V10_REG_TCPC_CTRL_BIST_TEST_MODE;
	data |= en ? TCPC_V10_REG_TCPC_CTRL_BIST_TEST_MODE : 0;

	return mt6370_write8(ddata, TCPC_V10_REG_TCPC_CTRL, data);
}
#endif /* CONFIG_USB_POWER_DELIVERY */

#if CONFIG_TYPEC_CAP_FORCE_DISCHARGE
#if CONFIG_TCPC_FORCE_DISCHARGE_IC
static int mt6370_set_force_discharge(struct tcpc_device *tcpc, bool en, int mv)
{
	struct mt6370_tcpc_data *ddata = tcpc_get_dev_data(tcpc);
	u8 data;
	int ret;

	ret = mt6370_read8(ddata, TCPC_V10_REG_POWER_CTRL, &data);
	if (ret)
		return ret;

	data &= ~TCPC_V10_REG_FORCE_DISC_EN;
	data |= en ? TCPC_V10_REG_FORCE_DISC_EN : 0;

	return mt6370_write8(ddata, TCPC_V10_REG_POWER_CTRL, data);
}
#endif	/* CONFIG_TCPC_FORCE_DISCHARGE_IC */
#endif	/* CONFIG_TYPEC_CAP_FORCE_DISCHARGE */

static struct tcpc_ops mt6370_tcpc_ops = {
	.init = mt6370_tcpc_init,
	.alert_status_clear = mt6370_alert_status_clear,
	.fault_status_clear = mt6370_fault_status_clear,
	.get_alert_mask = mt6370_get_alert_mask,
	.get_alert_status = mt6370_get_alert_status,
	.get_power_status = mt6370_get_power_status,
	.get_fault_status = mt6370_get_fault_status,
	.get_cc = mt6370_get_cc,
	.set_cc = mt6370_set_cc,
	.set_polarity = mt6370_set_polarity,
	.set_vconn = mt6370_set_vconn,
	.deinit = mt6370_tcpc_deinit,
	.is_vsafe0v = mt6370_is_vsafe0v,
	.set_low_power_mode = mt6370_set_low_power_mode,

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
	.set_msg_header = mt6370_set_msg_header,
	.set_rx_enable = mt6370_set_rx_enable,
	.protocol_reset = mt6370_protocol_reset,
	.get_message = mt6370_get_message,
	.transmit = mt6370_transmit,
	.set_bist_test_mode = mt6370_set_bist_test_mode,
	.set_bist_carrier_mode = mt6370_set_bist_carrier_mode,
#endif	/* CONFIG_USB_POWER_DELIVERY */

#if CONFIG_USB_PD_RETRY_CRC_DISCARD
	.retransmit = mt6370_retransmit,
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */

#if CONFIG_TYPEC_CAP_FORCE_DISCHARGE
#if CONFIG_TCPC_FORCE_DISCHARGE_IC
	.set_force_discharge = mt6370_set_force_discharge,
#endif	/* CONFIG_TCPC_FORCE_DISCHARGE_IC */
#endif	/* CONFIG_TYPEC_CAP_FORCE_DISCHARGE */
};

static int mt6370_register_tcpcdev(struct mt6370_tcpc_data *ddata)
{
	struct device_node *boot_np, *np = ddata->dev->of_node;
	struct tcpc_desc *desc = ddata->tcpc_desc;
	struct device *dev = ddata->dev;
	const struct {
		u32 size;
		u32 tag;
		u32 boot_mode;
		u32 boot_type;
	} *tag;
	u32 val;

	dev_info(dev, "%s, ++\n", __func__);

	/* Common TCPC desc DT */
	if (device_property_read_string(dev, "tcpc,name", &desc->name))
		desc->name = "type_c_port0";

	if (!device_property_read_u32(dev, "tcpc,role-def", &val)) {
		if (val >= TYPEC_ROLE_NR)
			desc->role_def = TYPEC_ROLE_DRP;
		else
			desc->role_def = val;
	}

	if (!device_property_read_u32(dev, "tcpc,rp-level", &val)) {
		switch (val) {
		case TYPEC_RP_DFT:
		case TYPEC_RP_1_5:
		case TYPEC_RP_3_0:
			desc->rp_lvl = val;
			break;
		default:
			desc->rp_lvl = TYPEC_RP_DFT;
			break;
		}
	}

#if CONFIG_TCPC_VCONN_SUPPLY_MODE
	if (!device_property_read_u32(dev, "tcpc,vconn-supply", &val)) {
		if (val >= TCPC_VCONN_SUPPLY_NR)
			desc->vconn_supply = TCPC_VCONN_SUPPLY_ALWAYS;
		else
			desc->vconn_supply = val;
	}
#endif	/* CONFIG_TCPC_VCONN_SUPPLY_MODE */

	ddata->tcpc = tcpc_device_register(dev, desc, &mt6370_tcpc_ops, ddata);
	if (IS_ERR(ddata->tcpc))
		return -EINVAL;

	/* Optional TCPC DT */
#if CONFIG_USB_PD_DISABLE_PE
	ddata->tcpc->disable_pe = device_property_read_bool(dev, "tcpc,disable-pe");
#endif	/* CONFIG_USB_PD_DISABLE_PE */

	/* Mediatek Boot mode DT */
	boot_np = of_parse_phandle(np, "boot-mode", 0);
	if (!boot_np) {
		dev_err(dev, "Failed to get bootmode phandle\n");
		return -ENODEV;
	}

	tag = of_get_property(boot_np, "atag,boot", NULL);
	if (!tag) {
		dev_err(dev, "Failed to get atag,boot\n");
		return -EINVAL;
	}

	dev_info(ddata->dev, "%s, sz:0x%x tag:0x%x mode:0x%x type:0x%x\n",
		 __func__, tag->size, tag->tag, tag->boot_mode, tag->boot_type);

	ddata->tcpc->bootmode = tag->boot_mode;

#if CONFIG_USB_PD_RETRY_CRC_DISCARD
	ddata->tcpc->tcpc_flags |= TCPC_FLAGS_RETRY_CRC_DISCARD;
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */

#if CONFIG_USB_PD_REV30
	ddata->tcpc->tcpc_flags |= TCPC_FLAGS_PD_REV30;

	if (ddata->tcpc->tcpc_flags & TCPC_FLAGS_PD_REV30)
		dev_info(dev, "%s, PD_REV30\n", __func__);
	else
		dev_info(dev, "%s, PD_REV20\n", __func__);
#endif	/* CONFIG_USB_PD_REV30 */

	ddata->tcpc->tcpc_flags |= TCPC_FLAGS_ALERT_V10;

	return 0;
}

#define MEDIATEK_6370_VID	0x29cf
#define MEDIATEK_6370_PID	0x5081

static int mt6370_check_revision(struct mt6370_tcpc_data *ddata)
{
	struct device *dev = ddata->dev;
	u16 vid, pid, did;
	int ret;

	ret = mt6370_read16(ddata, TCPC_V10_REG_VID, &vid);
	if (ret) {
		dev_err(dev, "%s, Failed to read vid(ret:%d)\n", __func__, ret);
		return -EIO;
	}

	if (vid != MEDIATEK_6370_VID) {
		dev_err(dev, "%s, Error! vid(0x%04X) is invalid!\n", __func__, vid);
		return -ENODEV;
	}

	ret = mt6370_read16(ddata, TCPC_V10_REG_PID, &pid);
	if (ret) {
		dev_err(dev, "%s, Failed to read pid(ret:%d)\n", __func__, ret);
		return -EIO;
	}

	/* Add MT6371 chip TCPC pid check for compatible */
	if (pid != MEDIATEK_6370_PID && pid != 0x5101 && pid != 0x6372) {
		dev_err(dev, "%s, Error! pid(0x%04X) is invalid!\n", __func__, pid);
		return -ENODEV;
	}

	ret = mt6370_write8(ddata, MT6370_REG_SWRESET, 1);
	if (ret) {
		dev_err(dev, "%s, Failed to set sw reseti(ret:%d)\n", __func__, ret);
		return ret;
	}

	usleep_range(1000, 2000);

	ret = mt6370_read16(ddata, TCPC_V10_REG_DID, &did);
	if (ret) {
		dev_err(dev, "%s, Failed to read did(ret:%d)\n", __func__, ret);
		return -EIO;
	}

	ddata->did = did;
	dev_info(ddata->dev, "%s, mt6370 tcpc device ID = 0x%04X\n", __func__, did);

	return 0;
}

static int mt6370_tcpc_probe(struct platform_device *pdev)
{
	struct mt6370_tcpc_data *ddata;
	struct device *dev;
	int ret = 0;

	dev_info(&pdev->dev, "%s, %s ++\n", __func__, MT6370_DRV_VERSION);

	ddata = devm_kzalloc(&pdev->dev, sizeof(*ddata), GFP_KERNEL);
	if (!ddata)
		return -ENOMEM;

	ddata->dev = &pdev->dev;
	dev = ddata->dev;
	platform_set_drvdata(pdev, ddata);
	mutex_init(&ddata->irq_lock);
	ddata->is_suspended = false;
	ddata->irq_while_suspended = false;

	ddata->rmap = dev_get_regmap(dev->parent, NULL);
	if (!ddata->rmap) {
		dev_err(dev, "Failed to get regmap\n");
		ret = -ENODEV;
		goto err_mutex_init;
	}

	ret = mt6370_check_revision(ddata);
	if (ret)
		goto err_mutex_init;

	ddata->tcpc_desc = devm_kzalloc(dev, sizeof(*ddata->tcpc_desc), GFP_KERNEL);
	if (!ddata->tcpc_desc) {
		ret = -ENOMEM;
		goto err_mutex_init;
	}

	ret = mt6370_register_tcpcdev(ddata);
	if (ret < 0) {
		dev_err(dev, "Failed to register mt6370 tcpc dev\n");
		goto err_tcpc_reg;
	}

	ret = mt6370_init_alert(ddata);
	if (ret < 0) {
		dev_err(dev, "Failed to init mt6370 alert\n");
		goto err_tcpc_reg;
	}

	dev_info(dev, "%s probe OK!\n", __func__);
	return 0;

err_tcpc_reg:
	tcpc_device_unregister(dev, ddata->tcpc);
err_mutex_init:
	mutex_destroy(&ddata->irq_lock);
	return ret;
}

static int mt6370_tcpc_remove(struct platform_device *pdev)
{
	struct mt6370_tcpc_data *ddata = platform_get_drvdata(pdev);

	if (ddata) {
		tcpc_device_unregister(ddata->dev, ddata->tcpc);
		mutex_destroy(&ddata->irq_lock);
		device_init_wakeup(&pdev->dev, false);
	}

	return 0;
}

#if CONFIG_PM
static int mt6370_tcpc_suspend(struct device *dev)
{
	struct mt6370_tcpc_data *ddata = dev_get_drvdata(dev);

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
	if (ddata->tcpc->pd_wait_hard_reset_complete) {
		dev_info(dev, "%s, WAITING HRESET - NO SUSPEND\n", __func__);
		return -EAGAIN;
	}
#endif

	dev_info(dev, "%s, irq_gpio = %d\n", __func__, gpiod_get_value(ddata->irq_gpio));

	mutex_lock(&ddata->irq_lock);
	ddata->is_suspended = true;
	mutex_unlock(&ddata->irq_lock);

	synchronize_irq(ddata->irq);

	return 0;
}

static int mt6370_tcpc_resume(struct device *dev)
{
	struct mt6370_tcpc_data *ddata = dev_get_drvdata(dev);

	dev_info(dev, "%s, irq_gpio = %d\n", __func__, gpiod_get_value(ddata->irq_gpio));

	mutex_lock(&ddata->irq_lock);
	if (ddata->irq_while_suspended) {
		enable_irq(ddata->irq);
		ddata->irq_while_suspended = false;
	}

	ddata->is_suspended = false;
	mutex_unlock(&ddata->irq_lock);

	return 0;
}

static void mt6370_tcpc_shutdown(struct platform_device *pdev)
{
	struct mt6370_tcpc_data *ddata = platform_get_drvdata(pdev);

	if (ddata->irq) {
		disable_irq(ddata->irq);
		kthread_flush_worker(&ddata->irq_worker);
		kthread_stop(ddata->irq_worker_task);
	}

	tcpm_shutdown(ddata->tcpc);
}

#if IS_ENABLED(CONFIG_PM_RUNTIME)
static int mt6370_tcpc_pm_suspend_runtime(struct device *dev)
{
	dev_dbg(dev, "%s, pm_runtime: suspending...\n", __func__);
	return 0;
}

static int mt6370_tcpc_pm_resume_runtime(struct device *dev)
{
	dev_dbg(dev, "%s, pm_runtime: resuming...\n", __func__);
	return 0;
}
#endif /* CONFIG_PM_RUNTIME */

static const struct dev_pm_ops mt6370_tcpc_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mt6370_tcpc_suspend, mt6370_tcpc_resume)
#if IS_ENABLED(CONFIG_PM_RUNTIME)
	SET_RUNTIME_PM_OPS(mt6370_tcpc_pm_suspend_runtime, mt6370_tcpc_pm_resume_runtime, NULL)
#endif /* CONFIG_PM_RUNTIME */
};

#define MT6370_TCPC_PM_OPS	(&mt6370_tcpc_pm_ops)
#else
#define MT6370_TCPC_PM_OPS	(NULL)
#endif /* CONFIG_PM */

static const struct of_device_id __maybe_unused mt6370_tcpc_of_match[] = {
	{.compatible = "mediatek,mt6370-tcpc",},
	{},
};

static struct platform_driver mt6370_tcpc_driver = {
	.driver = {
		.name = "mt6370-tcpc",
		.of_match_table = mt6370_tcpc_of_match,
		.pm = MT6370_TCPC_PM_OPS,
	},
	.probe = mt6370_tcpc_probe,
	.remove = mt6370_tcpc_remove,
	.shutdown = mt6370_tcpc_shutdown,
};
module_platform_driver(mt6370_tcpc_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MT6370 TCPC Driver");
MODULE_VERSION(MT6370_DRV_VERSION);

/**** Release Note ****
 * 1.0.1_U_MTK
 * (1) Update to K6.1 tcpc 2.0.8 (update v2.0.27_MTK)
 */
