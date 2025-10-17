// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
*/

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/debugfs.h>
#include <linux/bitops.h>
#include <linux/math64.h>
#include <linux/phy/phy.h>
#include <linux/pinctrl/consumer.h>
#include "sc2201.h"

#define PHY_MODE_BC11_SET 1
#define PHY_MODE_BC11_CLR 2
enum sc2201_usbsw {
	USBSW_CHG = 0,
	USBSW_USB,
};

static int sc2201_charger_set_usbsw(struct sc2201_chip *sc, enum sc2201_usbsw usbsw)
{
	int ret = 0, mode = (usbsw == USBSW_CHG) ? PHY_MODE_BC11_SET : PHY_MODE_BC11_CLR;
	struct phy *phy;

	sc2201_err("%s, usbsw = %d\n", __func__, usbsw);

	phy = phy_get(sc->dev, "usb2-phy");
	if (IS_ERR_OR_NULL(phy)) {
		sc2201_err("%s, Failed to get usb2-phy\n", __func__);
		return -ENODEV;
	}

	ret = phy_set_mode_ext(phy, PHY_MODE_USB_DEVICE, mode);
	if (ret)
		sc2201_err("%s, Failed to set phy ext mode\n", __func__);

	phy_put(sc->dev, phy);
	return ret;
}

/*********************I2C API*********************/
/**
 * return 0 for successful operation,
 * return negative value for err operation
 */
static int sc2201_i2c_write_bytes(struct sc2201_chip *sc, uint16_t reg, uint8_t len, uint8_t *val)
{
	struct i2c_client *i2c = sc->client;
	struct i2c_msg xfer[2];
	int ret;
	uint8_t *reg_data;
	reg_data = (uint8_t*)kmalloc(2 + len, GFP_KERNEL);
	if (!reg_data) {
		return -ENOMEM;
	}
	// memset(reg_data, 0x00, 2 + len);

	reg_data[0] = reg & 0xff,
	reg_data[1] = reg >> 8;
	memcpy(reg_data + 2, val, len);

	xfer[0].addr    = i2c->addr;
	xfer[0].flags   = 0;
	xfer[0].len     = 2 + len;
	xfer[0].buf     = (void *)reg_data;

	ret = i2c_transfer(i2c->adapter, xfer, 1);
	kfree(reg_data);
	if (ret == 1)
		return 0;
	else if (ret < 0)
		return ret;
	else
		return -EIO;
}

/**
 * return 0 for successful operation,
 * return negative value for err operation
 */
static int sc2201_i2c_read_bytes(struct sc2201_chip *sc, uint16_t reg, uint8_t len, uint8_t *val)
{
	struct i2c_client *i2c = sc->client;
	struct i2c_msg xfer[2];
	int ret;
	uint8_t reg_addr[2] = {reg & 0xff, reg >> 8};

	xfer[0].addr    = i2c->addr;
	xfer[0].flags   = 0;
	xfer[0].len     = 2;
	xfer[0].buf     = (void *)reg_addr;

	xfer[1].addr    = i2c->addr;
	xfer[1].flags   = I2C_M_RD;
	xfer[1].len     = len;
	xfer[1].buf     = val;

	ret = i2c_transfer(i2c->adapter, xfer, 2);
	if (ret == 2)
		return 0;
	else if (ret < 0)
		return ret;
	else
		return -EIO;
}

static int sc2201_i2c_write_byte(struct sc2201_chip *sc, uint16_t reg, uint8_t val)
{
	return sc2201_i2c_write_bytes(sc, reg, 1, &val);
}

static int sc2201_i2c_read_byte(struct sc2201_chip *sc, uint16_t reg, uint8_t *val)
{
	return sc2201_i2c_read_bytes(sc, reg, 1, val);
}

static int sc2201_field_read(struct sc2201_chip *sc,
							enum sc2201_fields field_id, int *val)
{
	int ret;
	uint8_t reg_val = 0;
	uint8_t mask = GENMASK(sc2201_reg_fields[field_id].msb, sc2201_reg_fields[field_id].lsb);

	ret = sc2201_i2c_read_byte(sc, sc2201_reg_fields[field_id].reg, &reg_val);
	if (ret < 0) {
		sc2201_err("sc2201 read field %d fail: %d\n", field_id, ret);
		return ret;
	}

	reg_val &= mask;
	reg_val >>= sc2201_reg_fields[field_id].lsb;

	*val = reg_val;
	return ret;
}

static int sc2201_field_write(struct sc2201_chip *sc,
							enum sc2201_fields field_id, int val)
{
	int ret;
	uint8_t reg_val = 0, tmp = 0;
	uint8_t mask = GENMASK(sc2201_reg_fields[field_id].msb, sc2201_reg_fields[field_id].lsb);

	ret = sc2201_i2c_read_byte(sc, sc2201_reg_fields[field_id].reg, &reg_val);
	if (ret < 0) {
		goto out;
	}

	tmp = reg_val & ~mask;
	val <<= sc2201_reg_fields[field_id].lsb;
	tmp |= val  & mask;

	if (sc2201_reg_fields[field_id].force_write || tmp != reg_val) {
		ret = sc2201_i2c_write_byte(sc, sc2201_reg_fields[field_id].reg, tmp);
	}

out:
	if (ret < 0) {
		sc2201_err("sc2201 write field %d fail: %d\n", field_id, ret);
	}
	return ret;
}

static int sc2201_dump_registers(void *parent)
{
	struct sc2201_chip *sc = parent;
	int ret = 0;
	int i = 0;
	uint8_t data[SC2201_REGNUM] = {0};

	ret = sc2201_i2c_read_bytes(sc, 0x00, SC2201_REGNUM, data);
	if (ret == 0) {
		for (i = 0; i < SC2201_REGNUM; ++i) {
			sc2201_info("%s reg[0x%02x] = 0x%02x\n", __func__, i, data[i]);
		}
	}
	return ret;
}

/*********************CHIP API*********************/
__maybe_unused
static int sc2201_detect_device(struct sc2201_chip *sc)
{
	int ret = 0;
	uint8_t dev_id[2] = {0};

	ret = sc2201_i2c_read_bytes(sc, SC2201_REG_DEVICE_ID0, 2, dev_id);
	if (ret) {
		sc2201_err("read dev id failed(%d)\n", ret);
		return ret;
	}

	if (dev_id[0] != SC2201_DEVICE_ID0 && dev_id[1] != SC2201_DEVICE_ID1) {
		sc2201_err("read dev_id err\n");
		return -ENODEV;
	}
	return ret;
}

__maybe_unused
static int sc2201_hvdcp_en(struct sc2201_chip *sc, int enable)
{
	uint8_t reg_val;
	int ret;
	ret = sc2201_i2c_read_byte(sc, SC2201_REG_HVDCP_EN, &reg_val);
	if (ret < 0) {
		sc2201_err("sc2201 read hvdcp config fail\n");
		return ret;
	}
	sc2201_info("sc2201 read hvdcp reg %d\n", reg_val);
	reg_val &= 0x7f;
	reg_val |= (!!enable) << 7;
	sc2201_info("sc2201 write hvdcp reg %d\n", reg_val);
	return sc2201_i2c_write_byte(sc, SC2201_REG_HVDCP_EN, reg_val);
}

__maybe_unused
static int sc2201_dpdm_en(struct sc2201_chip *sc, int enable)
{
	return sc2201_field_write(sc, DPDM_EN, !!enable);
}

__maybe_unused
static int sc2201_reg_reset(struct sc2201_chip *sc)
{
	return sc2201_field_write(sc, REG_RST, 1);
}

__maybe_unused
static int sc2201_tsd_en(struct sc2201_chip *sc, int enable)
{
	return sc2201_field_write(sc, TSD_EN, !!enable);
}

__maybe_unused
static int sc2201_get_tsd_stat(struct sc2201_chip *sc, int *stat)
{
	int reg_val;
	int ret = sc2201_field_read(sc, TSD_STAT, &reg_val);
	if (ret < 0) {
		dev_err(sc->dev, "read tsd state failed(%d)\n", ret);
		return ret;
	}
	*stat = !!reg_val;
	return ret;
}

__maybe_unused
static int sc2201_wd_timeout(struct sc2201_chip *sc, int time_ms)
{
	int reg_val = 0x00;
	if (time_ms > 5000) {
		reg_val = 0x05;
	} else if (time_ms > 1000) {
		reg_val = 0x04;
	} else if (time_ms > 500) {
		reg_val = 0x03;
	} else if (time_ms > 200) {
		reg_val = 0x02;
	} else if (time_ms > 0) {
		reg_val = 0x01;
	} else {
		reg_val = 0x00;
	}
	return sc2201_field_write(sc, WD_TIMEOUT, reg_val);
}

static int sc2201_inter_force_dpdm(struct sc2201_chip *sc) {
	int ret;

	sc2201_info("sc2201_inter_force_dpdm\n");
	ret = sc2201_field_write(sc, EN_BC1P2, 0);
	ret |= sc2201_dpdm_en(sc, 1);
	if (ret) {
		sc2201_err("write en bc12 failed(%d)\n", ret);
		return ret;
	}
	mutex_lock(&sc->bc_detect_lock);
	if (sc->bc12_detect) {
		sc2201_err("bc12_detect is true, return!\n");
		mutex_unlock(&sc->bc_detect_lock);
		return -EBUSY;
	}
	ret = sc2201_field_write(sc, EN_BC1P2, 1);
	if (ret) {
		sc2201_err("write en bc12 failed(%d)\n", ret);
		return ret;
	}
	sc->bc12_detect = true;
	sc->bc12_detect_done = false;
	mutex_unlock(&sc->bc_detect_lock);

	schedule_delayed_work(&sc->bc12_timeout_dwork,
							msecs_to_jiffies(3400));
	ret = sc2201_field_write(sc, FORCE_BC1P2, 1);
	if (ret) {
		sc2201_err("write force bc12 failed(%d)\n", ret);
		return ret;
	}
	return ret;
}

int sc2201_get_charger_type(struct sc2201_chip *sc) {
	int ret;
	int reg_val = 0;

	ret = sc2201_field_read(sc, BC1P2_TYPE0, &reg_val);
	if (ret < 0) {
		return ret;
	}
	if (sc->bc12_detect_done == false)
		return ret;
	switch (reg_val) {
	case BC12_TYPE_NO_INPUT:
		sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		break;
	case BC12_TYPE_SDP:
		sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		//ret = sc2201_charger_set_usbsw(sc, USBSW_USB);
		if (ret)
			sc2201_info("%s: set usbsw chg contral fail\n", __func__);
		break;
	case BC12_TYPE_CDP:
		sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_CDP;
		//ret = sc2201_charger_set_usbsw(sc, USBSW_USB);
		if (ret)
			sc2201_info("%s: set usbsw chg contral fail\n", __func__);
		break;
	case BC12_TYPE_DCP:
	case BC12_TYPE_HVDCP:
		sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;
	case BC12_TYPE_UNKNOWN:
		sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		if (sc->force_detect_count < 10) {
			schedule_delayed_work(&sc->force_detect_dwork,
								msecs_to_jiffies(2000));
		}
		break;
	case BC12_TYPE_NONSTAND:
		sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;
	default:
		sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;

		break;
	}
	sc->bc12_type = reg_val;
	sc2201_info("%s vbus stat: 0x%02x\n", __func__, reg_val);
	return ret;
}
EXPORT_SYMBOL(sc2201_get_charger_type);

static bool sc2201_charger_is_usb_rdy(struct device *dev)
{
	struct device_node *node;
	bool ready = true;

	node = of_parse_phandle(dev->of_node, "usb", 0);
	if (node) {
		ready = !of_property_read_bool(node, "cdp-block");
		sc2201_info("%s, usb ready = %d\n", __func__,  ready);
	} else
		sc2201_info("%s, usb node missing or invalid\n", __func__);

	return ready;
}

int sc2201_force_dpdm(struct sc2201_chip *sc)
{
	static const int max_wait_cnt = 250;
	int i = 0, ret = 0;
	bool en = true;

	/* CDP port specific process */
	for (i = 0; i < max_wait_cnt; i++) {
		if (sc2201_charger_is_usb_rdy(sc->dev))
			break;
		else
			msleep(100);
	}
	if (i == max_wait_cnt) {
		sc2201_info("%s, CDP timeout\n", __func__);
		en = false;
	} else
		sc2201_info("%s, CDP free\n", __func__);
	sc->force_detect_count = 0;
	if(en) {
		ret = sc2201_charger_set_usbsw(sc, USBSW_CHG);
		ret = sc2201_inter_force_dpdm(sc);
	}
	return ret;
}
EXPORT_SYMBOL(sc2201_force_dpdm);

int sc2201_get_type(struct sc2201_chip *sc)
{
	sc2201_info("%s: app get type %d\n", __func__, sc->bc12_type);
	return sc->bc12_type;
}

void sc2201_removed_reset(struct sc2201_chip *sc)
{
	sc2201_info("%s: adapter/usb removed\n", __func__);
	cancel_delayed_work_sync(&sc->force_detect_dwork);
	cancel_delayed_work_sync(&sc->bc12_timeout_dwork);
	mutex_lock(&sc->bc_detect_lock);
	sc->bc12_detect = false;
	sc->bc12_detect_done = false;
	sc->force_detect_count = 0;
	sc->bc12_type = BC12_TYPE_NO_INPUT;
	sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
	sc->psy_desc_type = POWER_SUPPLY_TYPE_UNKNOWN;
	sc->xmusb350_psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	mutex_unlock(&sc->bc_detect_lock);
	sc2201_field_write(sc, EN_BC1P2, 0);
	sc2201_field_write(sc, DPDM_EN, 0);
}
EXPORT_SYMBOL(sc2201_removed_reset);

/**********************debug*********************/
#ifdef CONFIG_ENABLE_SYSFS_DEBUG

static int get_parameters(char *buf, unsigned long *param, int num_of_par)
{
	int cnt = 0;
	char *token = strsep(&buf, "-");

	for (cnt = 0; cnt < num_of_par; cnt++) {
		if (token) {
			if (kstrtoul(token, 0, &param[cnt]) != 0)
				return -EINVAL;
			token = strsep(&buf, "\n");
		} else
			return -EINVAL;
	}

	return 0;
}

static ssize_t sc2201_chg_test_store_property(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct sc2201_chip *sc = dev_get_drvdata(dev);
	int ret/*, i*/;
	int get_val;
	long int val[2] = {0};
	ret = get_parameters((char *)buf, val, 2);
	if (ret < 0) {
		sc2201_err("get parameters fail\n");
		return -EINVAL;
	}
	sc2201_info("parameters %ld-%ld\n",val[0], val[1]);
	switch (val[0]) {
	case 1:
		sc2201_dpdm_en(sc, val[1]);
		break;
	case 2:
		sc2201_reg_reset(sc);
		break;
	case 3:
		sc2201_tsd_en(sc, val[1]);
		break;
	case 4:
		sc2201_wd_timeout(sc, val[1]);
		break;
	case 5:
		sc2201_get_tsd_stat(sc, &get_val);
		sc2201_info("get_parameters %d\n", get_val);
		break;
	case 6:
		sc2201_force_dpdm(sc);
		break;
	case 7:
		sc2201_get_type(sc);
		break;
	case 8:
		sc2201_hvdcp_en(sc, val[1]);
		break;
	case 9:
		sc2201_removed_reset(sc);
		break;
	default:
		break;
	}

	return count;
}

static ssize_t sc2201_chg_test_show_property(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int ret;
	ret = snprintf(buf, 512, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
		"1-value: dpdm_en",
		"2-value: reg_reset",
		"3-value: tsd_en",
		"4-value: wd_timeout",
		"5-value: get_tsd_stat",
		"6-value: force_dpdm",
		"7-value: get_type",
		"8-value: set_hvdcp",
		"9-value: sc2201_removed_reset");
	return ret;
}

static DEVICE_ATTR(test, 0660, sc2201_chg_test_show_property,
								sc2201_chg_test_store_property);

static void sc2201_chg_sysfs_file_init(struct device *dev)
{
	device_create_file(dev, &dev_attr_test);
}
#endif /* CONFIG_ENABLE_SYSFS_DEBUG */

/**********************interrupt*********************/
static irqreturn_t sc2201_irq_handler(int irq, void *data)
{
	int ret, reg_val;
	struct sc2201_chip *sc = (struct sc2201_chip *)data;

	sc2201_info("%s\n", __func__);
	if (delayed_work_pending(&sc->bc12_timeout_dwork)) {
		ret = sc2201_field_read(sc, DET_DONE_FLAG, &reg_val);
		if (ret) {
			sc2201_err("%s read det done flag err %d\n", __func__, ret);
			return IRQ_HANDLED;
		}
		if (reg_val == 1) {
			cancel_delayed_work_sync(&sc->bc12_timeout_dwork);
			sc2201_info( "%s bc12 handler end \n", __func__);
			mutex_lock(&sc->bc_detect_lock);
			sc->bc12_detect = false;
			sc->bc12_detect_done = true;
			mutex_unlock(&sc->bc_detect_lock);
			sc2201_get_charger_type(sc);
			ret = sc2201_charger_set_usbsw(sc, USBSW_USB);
			if (sc->bc12_type != BC12_TYPE_NO_INPUT) {
				if (!sc->usb_psy) {
					sc2201_err("retry to check usb_psy\n");
					sc->usb_psy = power_supply_get_by_name("usb");
				}
				if (sc->usb_psy != NULL)
					power_supply_changed(sc->usb_psy);
			}
		}
	}
	sc2201_dump_registers(sc);
	return IRQ_HANDLED;
}

static int sc2201_irq_register(struct sc2201_chip *sc)
{
	int ret;

	ret = gpio_request_one(sc->irq_gpio, GPIOF_DIR_IN, "sc2201_irq");
	if (ret) {
		sc2201_err("failed to request %d\n", sc->irq_gpio);
		return -EINVAL;
	}

	sc->irq = gpio_to_irq(sc->irq_gpio);
	if (sc->irq < 0) {
		sc2201_err("failed to gpio_to_irq\n");
		return -EINVAL;
	}

	ret = devm_request_threaded_irq(sc->dev, sc->irq, NULL,
					sc2201_irq_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"sc_irq", sc);
	if (ret < 0) {
		sc2201_err("request thread irq failed:%d\n", ret);
		return ret;
	}

	enable_irq_wake(sc->irq);
	device_init_wakeup(sc->dev, true);

	return ret;
}
static void sc2201_force_detection_dwork_handler(struct work_struct *work) {
	int ret;
	struct sc2201_chip *sc = container_of(work,
										struct sc2201_chip,
										force_detect_dwork.work);
	ret = sc2201_inter_force_dpdm(sc);
	if (ret) {
		sc2201_err("%s: force dpdm failed(%d)\n", __func__, ret);
		return;
	}

	sc->force_detect_count++;
}

static void sc2201_bc12_timeout_dwork_handler(struct work_struct *work) {
	int ret;
	int force_dpdm_stat = 0;
	struct sc2201_chip *sc = container_of(work,
										struct sc2201_chip,
										bc12_timeout_dwork.work);

	ret = sc2201_field_read(sc, DET_DONE_FLAG, &force_dpdm_stat);
	sc2201_info("%s force_dpdm = %d\n", __func__, force_dpdm_stat);

	mutex_lock(&sc->bc_detect_lock);
	sc->bc12_detect = false;
	mutex_unlock(&sc->bc_detect_lock);
	if (!force_dpdm_stat) {
		sc2201_info("BC1.2 timeout\n");
		sc2201_inter_force_dpdm(sc);
	}
}

 static enum power_supply_property xmusb350_properties[] = {
 	POWER_SUPPLY_PROP_MODEL_NAME,
 	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_USB_TYPE,
 };

 static int xmusb350_property_is_writeable(struct power_supply *psy,
 					    enum power_supply_property psp)
 {
 	switch (psp) {
 	case POWER_SUPPLY_PROP_ONLINE:
 		return 1;
 	default:
 		return 0;
 	}
 	return 0;
 }

 int xm_get_chg_type(struct sc2201_chip *sc){
	int bc12_type = BC12_TYPE_NO_INPUT;
	if (sc->bc12_detect_done == false)
		return 0;
	bc12_type = sc2201_get_type(sc);
	switch (bc12_type) {
	case BC12_TYPE_NO_INPUT:
		sc->psy_desc_type = POWER_SUPPLY_TYPE_UNKNOWN;
		break;
	case BC12_TYPE_SDP:
		sc->psy_desc_type = POWER_SUPPLY_TYPE_USB_TYPE_C;
		break;
	case BC12_TYPE_CDP:
		sc->psy_desc_type = POWER_SUPPLY_TYPE_USB_CDP;
		break;
	case BC12_TYPE_DCP:
	case BC12_TYPE_HVDCP:
		sc->psy_desc_type = POWER_SUPPLY_TYPE_USB_DCP;
		break;
	case BC12_TYPE_UNKNOWN:
		sc->psy_desc_type = POWER_SUPPLY_TYPE_USB_TYPE_C;
		break;
	case BC12_TYPE_NONSTAND:
		sc->psy_desc_type = POWER_SUPPLY_TYPE_USB_TYPE_C;
		break;
	default:
		sc->psy_desc_type = POWER_SUPPLY_TYPE_UNKNOWN;

		break;
	}
	sc2201_info("%s psy_desc_type=%d\n", __func__, sc->psy_desc_type);
	return 0;
 }
EXPORT_SYMBOL(xm_get_chg_type);

 static int xmusb350_get_property(struct power_supply *psy, enum power_supply_property prop, union power_supply_propval *val)
 {
	 struct sc2201_chip *chip = power_supply_get_drvdata(psy);

	 switch (prop) {
	 case POWER_SUPPLY_PROP_MODEL_NAME:
		 val->strval = "SC2201";
		 break;
	 case POWER_SUPPLY_PROP_ONLINE:
		 val->intval = chip->typec_online;
		 break;
	case POWER_SUPPLY_PROP_TYPE:
		xm_get_chg_type(chip);
		val->intval = chip->psy_desc_type;
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		val->intval = chip->psy_usb_type;
		break;
	 default:
		 sc2201_err("%s: unsupported property %d\n", __func__, prop);
 	}
	return 0;
 }

 static int xmusb350_set_property(struct power_supply *psy,
 				   enum power_supply_property psp,
 				   const union power_supply_propval *val)
 {
 	struct sc2201_chip *sc = power_supply_get_drvdata(psy);
 	int ret = 0, online = 0;

 	switch (psp) {
 	case POWER_SUPPLY_PROP_ONLINE:
 		online = ONLINE_GET_ATTACH(val->intval);
		sc->typec_online = online;
 		sc2201_err("set onlie property =%d\n", online);
		/*if(online){
			sc2201_force_dpdm(sc);
			sc2201_err("typec plug in, sc2201 force dpdm\n");
		}else{
			sc2201_removed_reset(sc);
			sc2201_err("typec plug out, sc2201 reset\n");
		}
 		if (ret < 0)
 			sc2201_err("failed to enable xmusb350 chg det\n");*/
 		break;
 	default:
 		return -ENODATA;
 	}
 	return ret;
 }

 static char *sc2201_psy_supplied_to[] = {
	"battery",
	"mtk-master-charger",
};

 static int xmusb350_init_psy(struct sc2201_chip *chip)
 {
 	struct power_supply_config xmusb350_psy_cfg = {};

	chip->xmusb350_psy_desc.name = "sc2201",
 	chip->xmusb350_psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN,
	chip->xmusb350_psy_desc.usb_types = sc2201_charger_psy_usb_types,
	chip->xmusb350_psy_desc.num_usb_types = ARRAY_SIZE(sc2201_charger_psy_usb_types),
 	chip->xmusb350_psy_desc.properties = xmusb350_properties,
 	chip->xmusb350_psy_desc.num_properties = ARRAY_SIZE(xmusb350_properties),
 	chip->xmusb350_psy_desc.property_is_writeable = xmusb350_property_is_writeable,
 	chip->xmusb350_psy_desc.get_property = xmusb350_get_property,
 	chip->xmusb350_psy_desc.set_property = xmusb350_set_property,
 	xmusb350_psy_cfg.drv_data = chip;
	xmusb350_psy_cfg.supplied_to = sc2201_psy_supplied_to,
	xmusb350_psy_cfg.num_supplicants = ARRAY_SIZE(sc2201_psy_supplied_to),
 	chip->xmusb350_psy = power_supply_register(chip->dev, &chip->xmusb350_psy_desc, &xmusb350_psy_cfg);
 	if (IS_ERR(chip->xmusb350_psy)) {
 		sc2201_err("failed to register sc2201 psy\n");
 		return PTR_ERR(chip->xmusb350_psy);
  	}

  	chip->usb_psy = power_supply_get_by_name("usb");
  	if (!chip->usb_psy) {
  		sc2201_err("failed to check usb_psy\n");
  	// 	return -1;
  	}

  	// chip->charger_psy = devm_power_supply_get_by_phandle(chip->dev, "charger");
  	// if (!chip->charger_psy) {
  	// 	sc2201_err("failed to check charger_psy\n");
  	// 	return -1;
  	// }

  	// chip->bms_psy = power_supply_get_by_name("bms");
  	// if (!chip->bms_psy) {
  	// 	sc2201_err("failed to check bms_psy\n");
  	// }

  	return 0;
  }

/********************SYSTEM API*********************/
static int sc2201_parse_dt(struct sc2201_chip *sc)
{
	struct device_node *np = sc->client->dev.of_node;
	struct pinctrl *bc12_pinctrl;
	struct pinctrl_state *bc12_cfg;

	int ret, i;
	const struct {
		const char *name;
		u32 *val;
	} param_data[] = {
		{"sc,sc2201,wd_timeout",      &(sc->sc2201_param.wd_timeout)},
		{"sc,sc2201,dpdm_en",      &(sc->sc2201_param.dpdm_en)},
	};

	bc12_pinctrl = devm_pinctrl_get(sc->dev);
	if (! IS_ERR_OR_NULL(bc12_pinctrl)) {
		bc12_cfg = pinctrl_lookup_state(bc12_pinctrl, "charger_third_pd_phy");
		if (! IS_ERR_OR_NULL(bc12_cfg)) {
			pinctrl_select_state(bc12_pinctrl, bc12_cfg);
			sc2201_err("success to config bc12_cfg\n");
		} else {
			sc2201_err("failed to parse bc12_cfg\n");
		}
	} else {
		sc2201_err("sc2201 failed to get pinctrl\n");
	}
	for (i = 0;i < ARRAY_SIZE(param_data);i++) {
		ret = of_property_read_u32(np, param_data[i].name, param_data[i].val);
		if (ret < 0) {
			sc2201_err("not find property %s\n", param_data[i].name);
			return ret;
		} else {
			sc2201_err("%s: %d\n", param_data[i].name, (int)*param_data[i].val);
		}
	}

	sc->irq_gpio = of_get_named_gpio(np, "sc2201,intr-gpio", 0);
	if (!gpio_is_valid(sc->irq_gpio)) {
		sc2201_err("fail to valid gpio : %d\n", sc->irq_gpio);
		return -EINVAL;
	}

	return ret;
}

static int sc2201_hw_init(struct sc2201_chip *sc)
{
	int ret = 0;
	int i = 0;
	const struct {
		enum sc2201_fields field;
		int val;
	} param_init[] = {
		{WD_TIMEOUT,  sc->sc2201_param.wd_timeout},
		{DPDM_EN,     sc->sc2201_param.dpdm_en},
	};

	for (i = 0;i < ARRAY_SIZE(param_init);++i) {
		ret = sc2201_field_write(sc, param_init[i].field, param_init[i].val);
		if (ret) {
			sc2201_err("write field %d failed\n", param_init[i].field);
			return ret;
		}
	}
	return ret;
}

static ssize_t sc2201_store_register(struct device *dev,
									struct device_attribute *attr,
									const char *buf, size_t count) {
	return 0;
}

static ssize_t sc2201_chg_show_regs(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct sc2201_chip *sc = dev_get_drvdata(dev);
	return sc2201_dump_registers(sc);
}

static DEVICE_ATTR(registers, 0660, sc2201_chg_show_regs, sc2201_store_register);

static void sc2201_create_device_node(struct device *dev)
{
	device_create_file(dev, &dev_attr_registers);
}

static struct of_device_id sc2201_charger_match_table[] = {
	{ .compatible = "sc,sc2201", },
	{ },
};

MODULE_DEVICE_TABLE(of, sc2201_charger_match_table);

static int sc2201_charger_probe(struct i2c_client *client)
{
	struct sc2201_chip *sc;
	int ret = 0;

	sc2201_info("%s (%s)\n", __func__, SC2201_DRV_VERSION);

	sc = devm_kzalloc(&client->dev, sizeof(struct sc2201_chip), GFP_KERNEL);
	if (!sc)
		return -ENOMEM;

	sc->dev = &client->dev;
	sc->client = client;
	i2c_set_clientdata(client, sc);

	ret = sc2201_detect_device(sc);
	if (ret) {
		sc2201_err("No sc2201 device found! errcode %d\n", ret);
		goto err_detect_dev;
	}
	sc2201_create_device_node(&(client->dev));

	INIT_DELAYED_WORK(&sc->force_detect_dwork,
						sc2201_force_detection_dwork_handler);
	INIT_DELAYED_WORK(&sc->bc12_timeout_dwork,
						sc2201_bc12_timeout_dwork_handler);

	ret = sc2201_parse_dt(sc);
	if (ret) {
		sc2201_err("%s parse dt failed(%d)\n", __func__, ret);
		goto err_parse_dt;
	}
	mutex_init(&sc->bc_detect_lock);
	sc->bc12_detect = false;
	sc->bc12_detect_done = false;
	sc->force_detect_count = 0;

	ret = sc2201_hw_init(sc);
	if (ret) {
		sc2201_err("%s failed to init device(%d)\n", __func__, ret);
		goto err_init_device;
	}

#ifdef CONFIG_ENABLE_SYSFS_DEBUG
	sc2201_chg_sysfs_file_init(sc->dev);
#endif /* CONFIG_ENABLE_SYSFS_DEBUG */

	ret = xmusb350_init_psy(sc);
	if (ret) {
		sc2201_err("failed to init psy\n");
		goto err_init_psy;
	}

	ret = sc2201_irq_register(sc);
	if (ret) {
		sc2201_err("%s irq register failed(%d)\n", __func__, ret);
		goto err_irq_register;
	}

	// determine_initial_status(sc);

	sc2201_info("sc2201 probe successfully\n!");
	return ret;

err_irq_register:
	power_supply_unregister(sc->xmusb350_psy);
err_init_psy:
err_parse_dt:
err_init_device:
err_detect_dev:
	sc2201_info("sc2201 prob failed\n");
	devm_kfree(&client->dev, sc);
	return -ENODEV;
}

static void sc2201_charger_remove(struct i2c_client *client)
{
	struct sc2201_chip *sc = i2c_get_clientdata(client);
	sc2201_info("%s\n", __func__);
	sc2201_reg_reset(sc);
	disable_irq(sc->irq);

}

#ifdef CONFIG_PM_SLEEP
static int sc2201_suspend(struct device *dev)
{
	struct sc2201_chip *sc = dev_get_drvdata(dev);

	sc2201_info("%s\n", __func__);

	if (device_may_wakeup(dev))
		enable_irq_wake(sc->client->irq);

	disable_irq(sc->client->irq);

	return 0;
}

static int sc2201_resume(struct device *dev)
{
	struct sc2201_chip *sc = dev_get_drvdata(dev);

	sc2201_info("%s\n", __func__);

	enable_irq(sc->client->irq);
	if (device_may_wakeup(dev))
		disable_irq_wake(sc->client->irq);

	return 0;
}

static SIMPLE_DEV_PM_OPS(sc2201_pm_ops, sc2201_suspend, sc2201_resume);
#endif /*CONFIG_PM_SLEEP*/

static struct i2c_driver sc2201_charger_driver = {
	.driver = {
		.name = "sc2201",
		.owner = THIS_MODULE,
		.of_match_table = sc2201_charger_match_table,
#ifdef CONFIG_PM_SLEEP
		.pm = &sc2201_pm_ops,
#endif /*CONFIG_PM_SLEEP*/
	},

	.probe = sc2201_charger_probe,
	.remove = sc2201_charger_remove,
};

module_i2c_driver(sc2201_charger_driver);

MODULE_AUTHOR("SouthChip<yaohui-mao@southchip.com>");
MODULE_DESCRIPTION("SC2201 Charger Driver");
MODULE_LICENSE("GPL v2");

