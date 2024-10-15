/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (c) 2024 Mediatek Inc.
*/

#define pr_fmt(fmt)	"[sc8960x]:%s: " fmt, __func__

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
#include <linux/bitops.h>
#include <linux/math64.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/iio/consumer.h>

#include "mtk_charger.h"
#include "mtk_battery.h"
#include "mtk_gauge.h"

#include "sc8960x_reg.h"
#include "charger_class.h"

#define SC8960X_DRV_VERSION			"1.0.0_G"

#define SC8960X_VBUS_OVP			13000000

static int sc8960x_get_vbus(struct charger_device *chgdev, u32 *vbus);
enum {
	PN_SC89601D, 
};

static int pn_data[] = {
	[PN_SC89601D] = 0x03,
};

struct chg_para{
	int vlim;
	int ilim;

	int vreg;
	int ichg;
};

struct sc8960x_platform_data {
	int iprechg;
	int iterm;

	int boostv;
	int boosti;
	int vac_ovp;

	struct chg_para usb;
};

struct sc8960x {
	struct device *dev;
	struct i2c_client *client;

	int part_no;
	int revision;

	const char *chg_dev_name;
	const char *eint_name;

	int irq_gpio;
	int chg_en_gpio;
    
	struct power_supply_desc psy_desc;
	int psy_usb_type;

	int status;
	int irq;

	struct mutex i2c_rw_lock;

	bool charge_enabled;	/* Register bit status */
	bool power_good;
	int vbus_stat;

	struct sc8960x_platform_data *platform_data;
	struct charger_device *chg_dev;

	struct power_supply *psy;
	struct iio_channel *chan_vcdt_voltage;
	struct regulator_dev *otg_rdev;
};

static const struct charger_properties sc8960x_chg_props = {
	.alias_name = "sc89601d_chg",
};

static int __sc8960x_read_reg(struct sc8960x *sc, u8 reg, u8 *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(sc->client, reg);
	if (ret < 0) {
		pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*data = (u8) ret;

	return 0;
}

static int __sc8960x_write_reg(struct sc8960x *sc, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(sc->client, reg, val);
	if (ret < 0) {
		pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
			val, reg, ret);
		return ret;
	}
	return 0;
}

static int sc8960x_read_byte(struct sc8960x *sc, u8 reg, u8 *data)
{
	int ret;

	mutex_lock(&sc->i2c_rw_lock);
	ret = __sc8960x_read_reg(sc, reg, data);
	mutex_unlock(&sc->i2c_rw_lock);

	return ret;
}

static int sc8960x_write_byte(struct sc8960x *sc, u8 reg, u8 data)
{
	int ret;

	mutex_lock(&sc->i2c_rw_lock);
	ret = __sc8960x_write_reg(sc, reg, data);
	mutex_unlock(&sc->i2c_rw_lock);

	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

	return ret;
}

static int sc8960x_update_bits(struct sc8960x *sc, u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	mutex_lock(&sc->i2c_rw_lock);
	ret = __sc8960x_read_reg(sc, reg, &tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	ret = __sc8960x_write_reg(sc, reg, tmp);
	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&sc->i2c_rw_lock);
	return ret;
}

static int sc8960x_enable_otg(struct sc8960x *sc)
{
	u8 val = REG01_OTG_ENABLE << REG01_OTG_CONFIG_SHIFT;

	return sc8960x_update_bits(sc, SC8960X_REG_01,
				REG01_OTG_CONFIG_MASK, val);
}

static int sc8960x_disable_otg(struct sc8960x *sc)
{
	u8 val = REG01_OTG_DISABLE << REG01_OTG_CONFIG_SHIFT;

	return sc8960x_update_bits(sc, SC8960X_REG_01,
				REG01_OTG_CONFIG_MASK, val);
}

static int sc8960x_enable_charger(struct sc8960x *sc)
{
	u8 val = REG01_CHG_ENABLE << REG01_CHG_CONFIG_SHIFT;

	return sc8960x_update_bits(sc, SC8960X_REG_01, 
				REG01_CHG_CONFIG_MASK, val);
}

static int sc8960x_disable_charger(struct sc8960x *sc)
{
	u8 val = REG01_CHG_DISABLE << REG01_CHG_CONFIG_SHIFT;

	return sc8960x_update_bits(sc, SC8960X_REG_01, 
				REG01_CHG_CONFIG_MASK, val);
}

static int sc8960x_set_chargecurrent(struct sc8960x *sc, int curr)
{
	u8 ichg;

	if (curr < REG02_ICHG_BASE)
		curr = REG02_ICHG_BASE;

	ichg = (curr - REG02_ICHG_BASE) / REG02_ICHG_LSB;

	return sc8960x_update_bits(sc, SC8960X_REG_02, 
						REG02_ICHG_MASK, ichg << REG02_ICHG_SHIFT);
}

static int sc8960x_set_term_current(struct sc8960x *sc, int curr)
{
	u8 iterm;

	if (curr < REG03_ITERM_BASE)
		curr = REG03_ITERM_BASE;

	iterm = (curr - REG03_ITERM_BASE) / REG03_ITERM_LSB;

	return sc8960x_update_bits(sc, SC8960X_REG_03, 
						REG03_ITERM_MASK, iterm << REG03_ITERM_SHIFT);

}

static int sc8960x_get_term_current(struct sc8960x *sc, int *curr)
{
	u8 reg_val;
	int iterm;
	int ret;

	ret = sc8960x_read_byte(sc, SC8960X_REG_03, &reg_val);
	if (!ret) {
		iterm = (reg_val & REG03_ITERM_MASK) >> REG03_ITERM_SHIFT;
		iterm = iterm * REG03_ITERM_LSB + REG03_ITERM_BASE;

		*curr = iterm * 1000;
	}
	return ret;
}

static int sc8960x_set_prechg_current(struct sc8960x *sc, int curr)
{
	u8 iprechg;

	if (curr < REG03_IPRECHG_BASE)
		curr = REG03_IPRECHG_BASE;

	iprechg = (curr - REG03_IPRECHG_BASE) / REG03_IPRECHG_LSB;

	return sc8960x_update_bits(sc, SC8960X_REG_03, 
						REG03_IPRECHG_MASK, iprechg << REG03_IPRECHG_SHIFT);

}

static int sc8960x_set_chargevolt(struct sc8960x *sc, int volt)
{
	u8 val;
	u8 vreg_ft;
	int ret;

	if (volt < REG04_VREG_BASE)
		volt = REG04_VREG_BASE;
	
	if (((volt - REG04_VREG_BASE) % REG04_VREG_LSB) / 8 == 1) {
		volt -= 8;
		vreg_ft = REG0D_VREG_FT_INC8MV;
	}
	else if (((volt - REG04_VREG_BASE) % REG04_VREG_LSB) / 8 == 2) {
		volt -= 16;
		vreg_ft = REG0D_VREG_FT_INC16MV;
	}
	else if (((volt - REG04_VREG_BASE) % REG04_VREG_LSB) / 8 == 3) {
		volt -= 24;
		vreg_ft = REG0D_VREG_FT_INC24MV;
	}
	else
		vreg_ft = REG0D_VREG_FT_DEFAULT;

	val = (volt - REG04_VREG_BASE) / REG04_VREG_LSB;
	ret = sc8960x_update_bits(sc, SC8960X_REG_04, 
						REG04_VREG_MASK, val << REG04_VREG_SHIFT);
	if (ret) {
		dev_err(sc->dev, "%s: failed to set charger volt\n", __func__);
		return ret;
	}

	ret = sc8960x_update_bits(sc, SC8960X_REG_0D, REG0D_VBAT_REG_FT_MASK,
					vreg_ft << REG0D_VBAT_REG_FT_SHIFT);
	if (ret) {
		dev_err(sc->dev, "%s: failed to set charger volt ft\n", __func__);
		return ret;
	}

	return ret;
}

static int sc8960x_get_chargevol(struct sc8960x *sc, int *volt)
{
	u8 reg_val;
	int vchg = 0;
	u8 vreg_ft;
	int ret;

	ret = sc8960x_read_byte(sc, SC8960X_REG_04, &reg_val);
	if (!ret) {
		vchg = (reg_val & REG04_VREG_MASK) >> REG04_VREG_SHIFT;
		vchg = vchg * REG04_VREG_LSB + REG04_VREG_BASE;
	}

	ret = sc8960x_read_byte(sc, SC8960X_REG_0D, &reg_val);
	if (!ret) {
		vreg_ft = (reg_val & REG0D_VBAT_REG_FT_MASK) >> REG0D_VBAT_REG_FT_SHIFT;
		vchg += (vreg_ft * REG0D_VREG_FT_INC_LSB + REG0D_VREG_FT_INC_BASE);
	}

	*volt = vchg * 1000;

	return ret;
}

static int sc8960x_set_input_volt_limit(struct sc8960x *sc, int volt)
{
	u8 val;

	if (volt < REG06_VINDPM_BASE)
		volt = REG06_VINDPM_BASE;

	if (volt <= REG06_VINDPM_RANGE_MAX) {
		val = (volt - REG06_VINDPM_BASE) / REG06_VINDPM_LSB;
	} else if (volt <= REG06_VINDPM_SPE_8000MV) {
		val = REG06_VINDPM_SPE1;
	} else if (volt <= REG06_VINDPM_SPE_8200MV) {
		val = REG06_VINDPM_SPE2;
	} else {
		val = REG06_VINDPM_SPE3;
	}
		
	return sc8960x_update_bits(sc, SC8960X_REG_06, 
						REG06_VINDPM_MASK, val << REG06_VINDPM_SHIFT);
}

static int sc8960x_get_input_volt_limit(struct sc8960x *sc, u32 *volt)
{
	u8 reg_val;
	int vchg;
	int ret;

	ret = sc8960x_read_byte(sc, SC8960X_REG_06, &reg_val);
	if (!ret) {
		vchg = (reg_val & REG06_VINDPM_MASK) >> REG06_VINDPM_SHIFT;
		if (vchg < REG06_VINDPM_SPE1) {
			vchg = vchg * REG06_VINDPM_LSB + REG06_VINDPM_BASE;
		} else if (vchg == REG06_VINDPM_SPE1) {
			vchg = REG06_VINDPM_SPE_8000MV;
		} else if (vchg == REG06_VINDPM_SPE2) {
			vchg = REG06_VINDPM_SPE_8200MV;
		} else {
			vchg = REG06_VINDPM_SPE_8400MV;
		}
		
		*volt = vchg * 1000;
	}
	return ret;
}

static int sc8960x_set_input_current_limit(struct sc8960x *sc, int curr)
{
	u8 val;

	if (curr < REG00_IINLIM_BASE)
		curr = REG00_IINLIM_BASE;

	val = (curr - REG00_IINLIM_BASE) / REG00_IINLIM_LSB;

	return sc8960x_update_bits(sc, SC8960X_REG_00, REG00_IINLIM_MASK, 
						val << REG00_IINLIM_SHIFT);
}

static int sc8960x_get_input_current_limit(struct  sc8960x *sc, u32 *curr)
{
	u8 reg_val;
	int icl;
	int ret;

	ret = sc8960x_read_byte(sc, SC8960X_REG_00, &reg_val);
	if (!ret) {
		icl = (reg_val & REG00_IINLIM_MASK) >> REG00_IINLIM_SHIFT;
		icl = icl * REG00_IINLIM_LSB + REG00_IINLIM_BASE;
		*curr = icl * 1000;
	}

	return ret;
}

static int sc8960x_set_watchdog_timer(struct sc8960x *sc, u8 timeout)
{
	u8 val;

	val = (timeout - REG05_WDT_LSB) / REG05_WDT_BASE;
	val <<= REG05_WDT_SHIFT;

	return sc8960x_update_bits(sc, SC8960X_REG_05, 
						REG05_WDT_MASK, val); 
}

static int sc8960x_disable_watchdog_timer(struct sc8960x *sc)
{
	u8 val = REG05_WDT_DISABLE << REG05_WDT_SHIFT;

	return sc8960x_update_bits(sc, SC8960X_REG_05, 
						REG05_WDT_MASK, val);
}

static int sc8960x_reset_watchdog_timer(struct sc8960x *sc)
{
	u8 val = REG01_WDT_RESET << REG01_WDT_RESET_SHIFT;

	return sc8960x_update_bits(sc, SC8960X_REG_01, 
						REG01_WDT_RESET_MASK, val);
}

static int sc8960x_pg_state_check(struct sc8960x *sc)
{
	int ret;
	int i;
	u8 val;

	for(i=0;i<5;i++)
	{
		ret = sc8960x_read_byte(sc, SC8960X_REG_08, &val);
		if(ret)
		{
			pr_info("%s, read fail!!, ret=%d\n",__func__, ret);
			continue;
		}

		val = val & REG08_PG_STAT_MASK;
		val = val >> REG08_PG_STAT_SHIFT;
		if(val == REG08_POWER_GOOD)
		{
			pr_info("%s, REG08_POWER_GOOD!!, i=%d\n",__func__, i);
			break;
		}

		msleep(100);
	}
	
	return ret;
}

static int sc8960x_force_dpdm(struct sc8960x *sc)
{
	int ret;
	u8 val = REG07_FORCE_DPDM << REG07_FORCE_DPDM_SHIFT;

	ret = sc8960x_update_bits(sc, SC8960X_REG_07, 
						REG07_FORCE_DPDM_MASK, val);

	pr_info("Force DPDM %s\n", !ret ? "successfully" : "failed");
	
	return ret;

}

#if 0
static int sc8960x_reset_chip(struct sc8960x *sc)
{
	int ret;
	u8 val = REG0B_REG_RESET << REG0B_REG_RESET_SHIFT;

	ret = sc8960x_update_bits(sc, SC8960X_REG_0B, 
						REG0B_REG_RESET_MASK, val);
	return ret;
}
#endif
static int sc8960x_enable_hiz_mode(struct sc8960x *sc, bool en)
{
	u8 val = (en ? REG00_HIZ_ENABLE : REG00_HIZ_DISABLE) << REG00_ENHIZ_SHIFT;

	return sc8960x_update_bits(sc, SC8960X_REG_00, 
						REG00_ENHIZ_MASK, val);
}

static int sc8960x_enable_term(struct sc8960x *sc, bool en)
{
	u8 val = (en ? REG05_TERM_ENABLE : REG05_TERM_DISABLE) << REG05_EN_TERM_SHIFT;

	return sc8960x_update_bits(sc, SC8960X_REG_05, 
						REG05_EN_TERM_MASK, val);
}

static int sc8960x_set_boost_current(struct sc8960x *sc, int curr)
{
	u8 temp;

	if (curr <= 500)
		temp = REG02_BOOST_LIM_0P5A << REG02_BOOST_LIM_SHIFT;
	else
		temp = REG02_BOOST_LIM_1P2A << REG02_BOOST_LIM_SHIFT;

	return sc8960x_update_bits(sc, SC8960X_REG_02, 
				REG02_BOOST_LIM_MASK, temp );

}

static int sc8960x_set_boost_voltage(struct sc8960x *sc, int volt)
{
	u8 val = 0;

	if (volt < REG06_BOOSTV_BASE)
		volt = REG06_BOOSTV_BASE;

	val = (volt - REG06_BOOSTV_BASE) / REG06_BOOSTV_LSB; 


	sc8960x_update_bits(sc, SC8960X_REG_06, 
				REG06_BOOSTV1_MASK,
				((val & 0x06) >> 1) << REG06_BOOSTV1_SHIFT);

	sc8960x_update_bits(sc, SC8960X_REG_0D, 
				REG0D_BOOSTV3_MASK,
				((val & 0x08) >> 3) << REG0D_BOOSTV3_SHIFT);

	return sc8960x_update_bits(sc, SC8960X_REG_0D, 
				REG0D_BOOSTV0_MASK,
				((val & 0x01) >> 0) << REG0D_BOOSTV0_SHIFT);
}

static int sc8960x_set_acovp_threshold(struct sc8960x *sc, int volt)
{
	u8 val;

	if (volt == 14200)
		val = 3;
	else if (volt == 11000)
		val = 2;
	else if (volt == 6400)
		val = 1;
	else
		val = 0;

	return sc8960x_update_bits(sc, SC8960X_REG_06, REG06_OVP_MASK,
				   val << REG06_OVP_SHIFT);
}

static int sc8960x_enable_safety_timer(struct sc8960x *sc, bool en)
{
	u8 val = en ? REG05_CHG_TIMER_ENABLE : REG05_CHG_TIMER_DISABLE;

	return sc8960x_update_bits(sc, SC8960X_REG_05, 
				REG05_EN_TIMER_MASK,
				val << REG05_EN_TIMER_SHIFT);
}

static struct sc8960x_platform_data *sc8960x_parse_dt(struct device_node *np,
							struct sc8960x *sc)
{
	int ret;
	struct sc8960x_platform_data *pdata;
	pdata = devm_kzalloc(sc->dev, sizeof(struct sc8960x_platform_data),
				GFP_KERNEL);
	if (!pdata)
		return NULL;

	if (of_property_read_string(np, "charger-name", &sc->chg_dev_name) < 0) {
		sc->chg_dev_name = "primary_chg";
		pr_warn("no charger name\n");
	}

	if (of_property_read_string(np, "eint-name", &sc->eint_name) < 0) {
		sc->eint_name = "chr_stat";
		pr_warn("no eint name\n");
	}

	ret = of_property_read_u32(np, "sc,sc8960x,usb-vlim", &pdata->usb.vlim);
	if (ret) {
		pdata->usb.vlim = 4500;
		pr_err("Failed to read node of sc,sc8960x,usb-vlim\n");
	}

	sc->irq_gpio = of_get_named_gpio(np, "sc,intr-gpio", 0);
	if (sc->irq_gpio < 0)
		pr_err("sc,intr-gpio is not available\n");
	else
		pr_err("intr-gpio:%d\n",sc->irq_gpio);

	sc->chg_en_gpio = of_get_named_gpio(np, "sc,chg-en-gpio", 0);
	if (sc->chg_en_gpio < 0)
		pr_err("sc,chg-en-gpio is not available\n");
	else {
		pr_err("chg_en_gpio:%d\n",sc->chg_en_gpio);
		gpio_direction_output(sc->chg_en_gpio, 0);
	}

	

	ret = of_property_read_u32(np, "sc,sc8960x,usb-ilim", &pdata->usb.ilim);
	if (ret) {
		pdata->usb.ilim = 2000;
		pr_err("Failed to read node of sc,sc8960x,usb-ilim\n");
	}

	ret = of_property_read_u32(np, "sc,sc8960x,usb-vreg", &pdata->usb.vreg);
	if (ret) {
		pdata->usb.vreg = 4200;
		pr_err("Failed to read node of sc,sc8960x,usb-vreg\n");
	}

	ret = of_property_read_u32(np, "sc,sc8960x,usb-ichg", &pdata->usb.ichg);
	if (ret) {
		pdata->usb.ichg = 2000;
		pr_err("Failed to read node of sc,sc8960x,usb-ichg\n");
	}

	ret = of_property_read_u32(np, "sc,sc8960x,precharge-current",
				&pdata->iprechg);
	if (ret) {
		pdata->iprechg = 180;
		pr_err("Failed to read node of sc,sc8960x,precharge-current\n");
	}

	ret = of_property_read_u32(np, "sc,sc8960x,termination-current",
				&pdata->iterm);
	if (ret) {
		pdata->iterm = 180;
		pr_err
			("Failed to read node of sc,sc8960x,termination-current\n");
	}

	ret = of_property_read_u32(np, "sc,sc8960x,boost-voltage",
				&pdata->boostv);
	if (ret) {
		pdata->boostv = 5100;
		pr_err("Failed to read node of sc,sc8960x,boost-voltage\n");
	}

	ret = of_property_read_u32(np, "sc,sc8960x,boost-current",
				&pdata->boosti);
	if (ret) {
		pdata->boosti = 1200;
		pr_err("Failed to read node of sc,sc8960x,boost-current\n");
	}

	ret = of_property_read_u32(np, "sc,sc8960x,vac-ovp-threshold",
				   &pdata->vac_ovp);
	if (ret) {
		pdata->vac_ovp = 6500;
		pr_err("Failed to read node of sc,sc8960x,vac-ovp-threshold\n");
	}

	return pdata;
}

static int sc8960x_get_charge_state(struct sc8960x *sc, int *state)
{
	int ret;
	u8 val;

	ret = sc8960x_read_byte(sc, SC8960X_REG_08, &val);
	if (!ret) {
		if ((val & REG08_VBUS_STAT_MASK) >> REG08_VBUS_STAT_SHIFT 
				== REG08_VBUS_TYPE_OTG) {
			*state = POWER_SUPPLY_STATUS_DISCHARGING;
			return ret;
		}
		val = val & REG08_CHRG_STAT_MASK;
		val = val >> REG08_CHRG_STAT_SHIFT;

		pr_err("%s, val=%d, charge_enabled=%d\n", __func__, val, sc->charge_enabled);
		
		switch (val)
		{
		case REG08_CHRG_STAT_IDLE:
			if(sc->charge_enabled) {
				*state = POWER_SUPPLY_STATUS_CHARGING;
			}
			else {
				*state = POWER_SUPPLY_STATUS_NOT_CHARGING;
			}	
			break;
		case REG08_CHRG_STAT_PRECHG:
		case REG08_CHRG_STAT_FASTCHG:
			*state = POWER_SUPPLY_STATUS_CHARGING;
			break;
		case REG08_CHRG_STAT_CHGDONE:
			*state = POWER_SUPPLY_STATUS_FULL;
			break;
		default:
			*state = POWER_SUPPLY_STATUS_UNKNOWN;
			break;
		}
	}

	pr_err("%s, state=%d, ret=%d\n", __func__, *state, ret);

	return ret;
}

#if 1
static struct mtk_charger *sc8960x_hal_get_charger_info(void)
{
	struct mtk_charger *info = NULL;
	struct power_supply *chg_psy = NULL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (IS_ERR_OR_NULL(chg_psy)) {
		pr_err("%s Couldn't get chg_psy\n", __func__);
		return NULL;
	} else {
		info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
		if (info == NULL) {
			pr_err("%s info is NULL\n", __func__);
			return NULL;
		}
	}

	return info;
}
#endif

static bool sc8960x_get_vbus_state(struct sc8960x *sc)
{
	int ret;
	u8 reg_val = 0;

	ret = sc8960x_read_byte(sc, SC8960X_REG_0A, &reg_val);
	if (ret) {
		pr_err("read reg[SC8960X_REG_0A] failed!!\n");
		return false;
	}

	pr_err("read reg[SC8960X_REG_0A]=0x%x\n",reg_val);

	return !!(reg_val & REG0A_VBUS_GD_MASK);
}

static int sc8960x_get_charger_type(struct sc8960x *sc)
{
	int ret;

	u8 reg_val = 0;
	struct mtk_charger *pinfo = NULL;

	pinfo = sc8960x_hal_get_charger_info();
	if(pinfo == NULL) {
		pr_err("%s sc8960x_hal_get_charger_info is NULL\n",__func__);
		return -EINVAL;
	}
        
	ret = sc8960x_read_byte(sc, SC8960X_REG_08, &reg_val);
	if (ret)
		return ret;

	sc->vbus_stat = (reg_val & REG08_VBUS_STAT_MASK);
	sc->vbus_stat >>= REG08_VBUS_STAT_SHIFT;

	pr_info("sc8960x:vbus_stat=%d\n",sc->vbus_stat);

	switch (sc->vbus_stat) {
	case REG08_VBUS_TYPE_NONE:
		sc->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		break;
	case REG08_VBUS_TYPE_SDP:
		sc->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		break;
	case REG08_VBUS_TYPE_CDP:
		sc->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
		sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_CDP;
		break;
	case REG08_VBUS_TYPE_DCP:
	case REG08_VBUS_TYPE_HVDCP:
		sc->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;
	case REG08_VBUS_TYPE_UNKNOWN:
		sc->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		break;
	case REG08_VBUS_TYPE_NON_STD:
		sc->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;
	case REG08_VBUS_TYPE_OTG:
		sc->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		return 0;	
	default:
		sc->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		break;
	}

	// if(pinfo->pd_type == MTK_PD_CONNECT_PE_READY_SNK_PD30 || pinfo->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) {
	// 	sc->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
	// 	sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;		
	// }

	return 0;
}

static irqreturn_t sc8960x_irq_handler(int irq, void *data)
{
#if 0
	int ret;
	bool prev_pg;
	struct sc8960x *sc = data;

	int retry = 0;

	prev_pg = sc->power_good;

	sc->power_good = sc8960x_get_vbus_state(sc);

	pr_err("%s, power_good:%d, psy_usb_type:%d\n", __func__, sc->power_good, sc->psy_usb_type);

	if (!prev_pg && sc->power_good){
		pr_err("adapter/usb inserted\n");
	} else if (prev_pg && !sc->power_good) {
		pr_err("adapter/usb removed\n");
	} else {
		return IRQ_HANDLED;
	}

	while(1) {

		mdelay(100);

		ret = sc8960x_get_charger_type(sc);
		
		pr_err("power_good:%d, psy_usb_type:%d\n", sc->power_good, sc->psy_usb_type);
		
		if(sc->power_good && sc->psy_usb_type != POWER_SUPPLY_USB_TYPE_UNKNOWN) {
			break;
		}
		else if(!sc->power_good && sc->psy_usb_type == POWER_SUPPLY_USB_TYPE_UNKNOWN) {
			break;
		}
		
		if(retry >= 10) {
			break;
		}
		else
			retry++;
	}

	power_supply_changed(sc->psy);
#endif
	return IRQ_HANDLED;
}

static int sc8960x_register_interrupt(struct sc8960x *sc)
{
	int ret = 0;

	ret = devm_gpio_request(sc->dev, sc->irq_gpio, "chr-irq");
	if (ret < 0) {
		pr_err("failed to request GPIO%d ; ret = %d", sc->irq_gpio, ret);
		return ret;
	}

	ret = gpio_direction_input(sc->irq_gpio);
	if (ret < 0) {
		pr_err("failed to set GPIO%d ; ret = %d", sc->irq_gpio, ret);
		return ret;
	}

	sc->irq = gpio_to_irq(sc->irq_gpio);
	if (sc->irq < 0) {
		pr_err("failed gpio to irq %d ; GPIO%d", sc->irq, sc->irq_gpio);
		return ret;
	}

	ret = devm_request_threaded_irq(sc->dev, sc->irq, NULL,
					sc8960x_irq_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"chr_stat", sc);
	if (ret < 0) {
		pr_err("request thread irq failed:%d\n", ret);
		return ret;
	}else{
		pr_err("request thread irq pass:%d  sc->irq =%d\n", ret, sc->irq);
	}

	enable_irq_wake(sc->irq);

	return 0;
}

static int sc8960x_init_device(struct sc8960x *sc)
{
	int ret;
	sc8960x_disable_watchdog_timer(sc);
	sc8960x_set_watchdog_timer(sc, 40);

	ret = sc8960x_set_prechg_current(sc, sc->platform_data->iprechg);
	if (ret)
		pr_err("Failed to set prechg current, ret = %d\n", ret);

	ret = sc8960x_set_term_current(sc, sc->platform_data->iterm);
	if (ret)
		pr_err("Failed to set termination current, ret = %d\n", ret);

	ret = sc8960x_set_boost_voltage(sc, sc->platform_data->boostv);
	if (ret)
		pr_err("Failed to set boost voltage, ret = %d\n", ret);

	ret = sc8960x_set_boost_current(sc, sc->platform_data->boosti);
	if (ret)
		pr_err("Failed to set boost current, ret = %d\n", ret);

	ret = sc8960x_set_acovp_threshold(sc, sc->platform_data->vac_ovp);
	if (ret)
		pr_err("Failed to set acovp threshold, ret = %d\n", ret);
    
	return 0;
}

static void determine_initial_status(struct sc8960x *sc)
{
	sc8960x_irq_handler(sc->irq, (void *) sc);
}

static int sc8960x_detect_device(struct sc8960x *sc)
{
	int ret;
	u8 data;

	ret = sc8960x_read_byte(sc, SC8960X_REG_0B, &data);
	if (!ret) {
		sc->part_no = (data & REG0B_PN_MASK) >> REG0B_PN_SHIFT;
		sc->revision =
			(data & REG0B_DEV_REV_MASK) >> REG0B_DEV_REV_SHIFT;
	}

	return ret;
}

static void sc8960x_dump_regs(struct sc8960x *sc)
{
	int addr;
	u8 val;
	int ret;

	for (addr = 0x0; addr <= SC8960X_REG_0E; addr++) {
		ret = sc8960x_read_byte(sc, addr, &val);
		if (ret == 0)
			pr_err("Reg[%.2x] = 0x%.2x\n", addr, val);
	}
}

static ssize_t
sc8960x_show_registers(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct sc8960x *sc = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[200];
	int len;
	int idx = 0;
	int ret;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sc8960x Reg");
	for (addr = 0x0; addr <= SC8960X_REG_0E; addr++) {
		ret = sc8960x_read_byte(sc, addr, &val);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,
					"Reg[%.2x] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t
sc8960x_store_registers(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	struct sc8960x *sc = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg < SC8960X_REG_0E) {
		sc8960x_write_byte(sc, (unsigned char) reg,
				(unsigned char) val);
	}

	return count;
}

static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR, sc8960x_show_registers,
		sc8960x_store_registers);

static struct attribute *sc8960x_attributes[] = {
	&dev_attr_registers.attr,
	NULL,
};

static const struct attribute_group sc8960x_attr_group = {
	.attrs = sc8960x_attributes,
};

static int sc8960x_charging(struct charger_device *chg_dev, bool enable)
{

	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;
	u8 val;

	if (enable)
		ret = sc8960x_enable_charger(sc);
	else
		ret = sc8960x_disable_charger(sc);

	pr_err("%s charger %s\n", enable ? "enable" : "disable",
		!ret ? "successfully" : "failed");

	ret = sc8960x_read_byte(sc, SC8960X_REG_01, &val);

	if (!ret)
		sc->charge_enabled = !!(val & REG01_CHG_CONFIG_MASK);

	return ret;
}

static int sc8960x_enable_vindpm_track(struct sc8960x *sc)
{
	int ret;
	u8 val = REG07_VDPM_BAT_TRACK_250MV << REG07_VDPM_BAT_TRACK_SHIFT;

	ret = sc8960x_update_bits(sc, SC8960X_REG_07, 
						REG07_VDPM_BAT_TRACK_MASK, val);

	pr_info("sc8960x vindpm track successfully\n");
	
	return ret;

}

static int sc8960x_plug_in(struct charger_device *chg_dev)
{

	int ret;
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

	pr_info("%s\n", __func__);

	ret = sc8960x_charging(chg_dev, true);

	if (ret)
		pr_err("Failed to enable charging:%d\n", ret);

	sc8960x_enable_vindpm_track(sc);
	
	return ret;
}

static int sc8960x_plug_out(struct charger_device *chg_dev)
{
	int ret;

	pr_info("%s\n", __func__);

	ret = sc8960x_charging(chg_dev, false);

	if (ret)
		pr_err("Failed to disable charging:%d\n", ret);

	return ret;
}

static int sc8960x_dump_register(struct charger_device *chg_dev)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

	sc8960x_dump_regs(sc);

	return 0;
}

static int sc8960x_is_charging_enable(struct charger_device *chg_dev, bool *en)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

	*en = sc->charge_enabled;

	return 0;
}

static int sc8960x_is_charging_done(struct charger_device *chg_dev, bool *done)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 val;

	ret = sc8960x_read_byte(sc, SC8960X_REG_08, &val);
	if (!ret) {
		val = val & REG08_CHRG_STAT_MASK;
		val = val >> REG08_CHRG_STAT_SHIFT;
		*done = (val == REG08_CHRG_STAT_CHGDONE);
	}

	return ret;
}

static int sc8960x_set_ichg(struct charger_device *chg_dev, u32 curr)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

	pr_err("charge curr = %d\n", curr);

	return sc8960x_set_chargecurrent(sc, curr / 1000);
}

static int sc8960x_get_ichg(struct charger_device *chg_dev, u32 *curr)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int ichg;
	int ret;

	ret = sc8960x_read_byte(sc, SC8960X_REG_02, &reg_val);
	if (!ret) {
		ichg = (reg_val & REG02_ICHG_MASK) >> REG02_ICHG_SHIFT;
		ichg = ichg * REG02_ICHG_LSB + REG02_ICHG_BASE;
		*curr = ichg * 1000;
	}

	return ret;
}

static int sc8960x_get_min_ichg(struct charger_device *chg_dev, u32 *curr)
{
	*curr = 60 * 1000;

	return 0;
}

static int sc8960x_set_vchg(struct charger_device *chg_dev, u32 volt)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

	pr_err("charge volt = %d\n", volt);

	return sc8960x_set_chargevolt(sc, volt / 1000);
}

static int sc8960x_get_vchg(struct charger_device *chg_dev, u32 *volt)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

	return sc8960x_get_chargevol(sc, volt);
}

static int sc8960x_set_ivl(struct charger_device *chg_dev, u32 volt)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

	pr_err("vindpm volt = %d\n", volt);

	return sc8960x_set_input_volt_limit(sc, volt / 1000);
}

static int sc8960x_get_ivl(struct charger_device *chgdev, u32 *volt)
{
	struct sc8960x *sc = dev_get_drvdata(&chgdev->dev);

	return sc8960x_get_input_volt_limit(sc, volt);
}

static int sc8960x_set_icl(struct charger_device *chg_dev, u32 curr)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

	pr_err("indpm curr = %d\n", curr);

	return sc8960x_set_input_current_limit(sc, curr / 1000);
}

static int sc8960x_get_icl(struct charger_device *chg_dev, u32 *curr)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
	
	return sc8960x_get_input_current_limit(sc, curr);
}

static int sc8960x_kick_wdt(struct charger_device *chg_dev)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

	return sc8960x_reset_watchdog_timer(sc);
}

static int sc8960x_set_ieoc(struct charger_device *chgdev, u32 ieoc)
{
	struct sc8960x *sc = dev_get_drvdata(&chgdev->dev);
	
	return sc8960x_set_term_current(sc, ieoc / 1000);
}

static int sc8960x_enable_te(struct charger_device *chgdev, bool en)
{
	struct sc8960x *sc = dev_get_drvdata(&chgdev->dev);

	return sc8960x_enable_term(sc, en);
}

static int sc8960x_enable_hz(struct charger_device *chgdev, bool en)
{
	struct sc8960x *sc = dev_get_drvdata(&chgdev->dev);

	return sc8960x_enable_hiz_mode(sc, en);
}


static int sc8960x_set_ta20_current_pattern(struct charger_device *chgdev, u32 uV)
{
    struct sc8960x *sc = dev_get_drvdata(&chgdev->dev);
    u8 val = 0;
    int curr;
    int i;

    if (uV < 5500000) {
        uV = 5500000;
    } else if (uV > 15000000) {
        uV = 15000000;
    }

    val = (uV - 5500000) / 500000;

    sc8960x_get_input_current_limit(sc, &curr);

    pr_err("%s ta20 vol=%duV, val=%d, curr=%d\n", __func__, uV, val, curr);

    sc8960x_set_input_current_limit(sc, 100);
    msleep(150);
    for (i = 4; i >= 0; i--) {
        if (val & (1 << i)) {
            pr_err("%s bit[%d] = 1\n", __func__, i);
            sc8960x_set_input_current_limit(sc, 800);
            msleep(100);
            sc8960x_set_input_current_limit(sc, 100);
            msleep(50);
        } else {
            pr_err("%s bit[%d] = 0\n", __func__, i);
            sc8960x_set_input_current_limit(sc, 800);
            msleep(50);
            sc8960x_set_input_current_limit(sc, 100);
            msleep(100);
        }
    }
    sc8960x_set_input_current_limit(sc, 800);
    msleep(150);
    sc8960x_set_input_current_limit(sc, 100);
    msleep(150);

    return sc8960x_set_input_current_limit(sc, 800);
}

static int sc8960x_set_ta20_reset(struct charger_device *chgdev)
{
    struct sc8960x *sc = dev_get_drvdata(&chgdev->dev);
    int curr;
    int ret;
	
    ret = sc8960x_get_input_current_limit(sc, &curr);

    ret = sc8960x_set_input_current_limit(sc, 100);
    msleep(300);
    ret = sc8960x_set_input_current_limit(sc, curr/1000);

    return ret;
}

static int sc8960x_do_event(struct charger_device *chgdev, u32 event, u32 args)
{
	struct sc8960x *sc = dev_get_drvdata(&chgdev->dev);

	switch (event) {
	case EVENT_FULL:
	case EVENT_RECHARGE:
	case EVENT_DISCHARGE:
		power_supply_changed(sc->psy);
		break;
	default:
		break;
	}
	return 0;
}

static int sc8960x_set_otg(struct charger_device *chg_dev, bool en)
{
	int ret;
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

	if (en) {
		ret = sc8960x_disable_charger(sc);
		ret = sc8960x_enable_otg(sc);
	}
	else {
		ret = sc8960x_disable_otg(sc);
	}

	pr_err("%s OTG %s\n", en ? "enable" : "disable",
		!ret ? "successfully" : "failed");

	return ret;
}

static int sc8960x_set_safety_timer(struct charger_device *chg_dev, bool en)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

	return sc8960x_enable_safety_timer(sc, en);
}

static int sc8960x_is_safety_timer_enabled(struct charger_device *chg_dev,
					bool *en)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 reg_val;

	ret = sc8960x_read_byte(sc, SC8960X_REG_05, &reg_val);

	if (!ret)
		*en = !!(reg_val & REG05_EN_TIMER_MASK);

	return ret;
}

static int sc8960x_set_boost_ilmt(struct charger_device *chg_dev, u32 curr)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
	int ret;

	pr_err("otg curr = %d\n", curr);

	ret = sc8960x_set_boost_current(sc, curr / 1000);

	return ret;
}

static enum power_supply_usb_type sc8960x_chg_psy_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_DCP,
};


static enum power_supply_property sc8960x_chg_psy_properties[] = {
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

static int sc8960x_chg_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_ENERGY_EMPTY:
		return 1;
	default:
		return 0;
	}
	return 0;
}

static int sc8960x_chg_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	int ret = 0;
	u32 _val;
	int data;
	u32 vbus = 0;

	struct sc8960x *sc = power_supply_get_drvdata(psy);

	pr_err("%s psp=%d\n",__func__, psp);
	
	switch (psp) {
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = "SouthChip";
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval  = sc->power_good;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		if(sc->power_good)
		{
			ret = sc8960x_get_vbus(sc->chg_dev, &vbus);
			if(ret < 0) {
				pr_err("%s, sc8960x_get_vbus failed ret:%d\n",__func__, ret);
			}

			pr_err("%s, vbus=%d, sc->charge_enabled:%d\n",__func__, vbus, sc->charge_enabled);

			if(vbus > SC8960X_VBUS_OVP) {
				if(sc->charge_enabled)
					_val = POWER_SUPPLY_STATUS_CHARGING;
				else
					_val = POWER_SUPPLY_STATUS_NOT_CHARGING;
			}
			else
			{
				ret = sc8960x_get_charge_state(sc, &_val);
				if (ret < 0)
					break;
			}
		}
		else
		{
			_val = POWER_SUPPLY_STATUS_NOT_CHARGING;
		}

		val->intval = _val;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = sc8960x_get_chargevol(sc, &data);
		if (ret < 0)
			break;
		val->intval = data;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = sc8960x_get_input_current_limit(sc, &data);
		if (ret < 0)
			break;
		val->intval = data;
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		ret = sc8960x_get_input_volt_limit(sc, &data);
		if (ret < 0)
			break;
		val->intval = data;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		ret = sc8960x_get_term_current(sc, &data);
		if (ret < 0)
			break;
		val->intval = data;
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		val->intval = sc->psy_usb_type;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (sc->psy_desc.type == POWER_SUPPLY_TYPE_USB)
			val->intval = 500000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (sc->psy_desc.type == POWER_SUPPLY_TYPE_USB)
			val->intval = 5000000;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = sc->psy_desc.type;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

void sc8960x_chg_attach_process(struct sc8960x *sc, int attach)
{
	int ret;
	bool prev_pg;
	bool curr_pg;
	int retry = 0;
	u32 vbus = 0;

#ifdef CONFIG_CHAMSION_EXT_OTG_VBUS
	if(attach == 0) {
		pr_err("adapter/usb removed attach:%d\n", attach);
		sc->power_good = 0;
		sc->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		power_supply_changed(sc->psy);
		return;
	}
#endif

	prev_pg = sc->power_good;

	while(1) {
		ret = sc8960x_get_vbus(sc->chg_dev, &vbus);
		if(ret < 0) {
			pr_err("%s, get vbus failed ret:%d\n",__func__, ret);
			return;
		}

		pr_err("%s, vbus:%d\n", __func__, vbus);
		
		curr_pg = sc8960x_get_vbus_state(sc);
		if(!prev_pg && curr_pg) {
			pr_err("adapter/usb inserted\n");
			sc->power_good = 1;
			break;
		}
		else if(prev_pg && !curr_pg && vbus < SC8960X_VBUS_OVP) {
			pr_err("adapter/usb removed\n");
			sc->power_good = 0;
			break;
		}
		else {
			if(retry >= 20) {
				pr_err("%s, adapter state check timeout!!\n", __func__);
				return;
			}
			else {
				mdelay(100);
				retry++;
			}
		}
	}

	pr_err("%s, adapter state check retry:%d\n",__func__,  retry);

	retry = 0;

	while(1) {
		ret = sc8960x_get_charger_type(sc);
		
		pr_err("power_good:%d, psy_usb_type:%d\n", sc->power_good, sc->psy_usb_type);
		
		if(sc->power_good && 
			sc->vbus_stat != REG08_VBUS_TYPE_NONE && 
			sc->vbus_stat != REG08_VBUS_TYPE_OTG && 
			sc->psy_usb_type != POWER_SUPPLY_USB_TYPE_UNKNOWN) {
			break;
		}
		else if(!sc->power_good && 
			(sc->vbus_stat == REG08_VBUS_TYPE_NONE || sc->vbus_stat == REG08_VBUS_TYPE_OTG) && 
			sc->psy_usb_type == POWER_SUPPLY_USB_TYPE_UNKNOWN) {
			break;
		}
		
		if(retry >= 10) {
			pr_err("%s, charger type check timeout!!\n", __func__);
			return;
		}
		else {
			mdelay(100);
			retry++;
		}
	}
	
	pr_err("%s, charger type check retry:%d\n",__func__,  retry);

	power_supply_changed(sc->psy);
}

static int sc8960x_chg_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	int ret = 0;
	struct sc8960x *sc = power_supply_get_drvdata(psy);
	sc8960x_force_dpdm(sc);
	sc8960x_pg_state_check(sc);
	pr_err("%s psp=%d\n",__func__, psp);
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		sc8960x_chg_attach_process(sc, val->intval);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		if (val->intval) {
			ret = sc8960x_enable_charger(sc);
		} else {
			ret = sc8960x_disable_charger(sc);
		}
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = sc8960x_set_chargecurrent(sc, val->intval / 1000);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = sc8960x_set_chargevolt(sc, val->intval / 1000);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = sc8960x_set_input_current_limit(sc, val->intval / 1000);
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		ret = sc8960x_set_input_volt_limit(sc, val->intval / 1000);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		ret = sc8960x_set_term_current(sc, val->intval / 1000);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static char *sc8960x_psy_supplied_to[] = {
	"battery",
	"mtk-master-charger",
};

static const struct power_supply_desc sc8960x_psy_desc = {
	.name = "ext_charger_type",
	.type = POWER_SUPPLY_TYPE_USB,
	.usb_types = sc8960x_chg_psy_usb_types,
	.num_usb_types = ARRAY_SIZE(sc8960x_chg_psy_usb_types),
	.properties = sc8960x_chg_psy_properties,
	.num_properties = ARRAY_SIZE(sc8960x_chg_psy_properties),
	.property_is_writeable = sc8960x_chg_property_is_writeable,
	.get_property = sc8960x_chg_get_property,
	.set_property = sc8960x_chg_set_property,
};

static int sc8960x_chg_init_psy(struct sc8960x *sc)
{
	struct power_supply_config cfg = {
		.drv_data = sc,
		.of_node = sc->dev->of_node,
		.supplied_to = sc8960x_psy_supplied_to,
		.num_supplicants = ARRAY_SIZE(sc8960x_psy_supplied_to),
	};

	pr_err("%s\n", __func__);
	memcpy(&sc->psy_desc, &sc8960x_psy_desc, sizeof(sc->psy_desc));
	//sc->psy_desc.name = dev_name(sc->dev);
	sc->psy = devm_power_supply_register(sc->dev, &sc->psy_desc,
						&cfg);
	return IS_ERR(sc->psy) ? PTR_ERR(sc->psy) : 0;
}

static int sc8960x_get_ibus(struct charger_device *chgdev, u32 *ibus)
{
	return 0;
}

static int sc8960x_get_vbus(struct charger_device *chgdev, u32 *vbus)
{
	struct sc8960x *sc = charger_get_data(chgdev);
	static struct mtk_gauge *gauge;
	static struct power_supply *psy;
	int ret = 0;
	int val = 0;

	if (psy == NULL) {
		psy = power_supply_get_by_name("mtk-gauge");
		if (psy == NULL) {
			pr_err("[%s]psy is not rdy\n", __func__);
			return -EINVAL;
		}
	}

	if (gauge == NULL) {
		gauge = (struct mtk_gauge *)power_supply_get_drvdata(psy);
		if (gauge == NULL) {
			pr_err("[%s]mtk_gauge is not rdy\n", __func__);
			return -EINVAL;
		}
	}

	if (!IS_ERR(sc->chan_vcdt_voltage)) {
		ret = iio_read_channel_processed(sc->chan_vcdt_voltage, &val);
		if (ret < 0) {
			pr_err("[%s]read fail,ret=%d\n", __func__, ret);
		}
	} else {
		pr_err("[%s]chan error\n", __func__);
	}

	val = (((gauge->gm->fg_cust_data.r_charger_1 +
		 gauge->gm->fg_cust_data.r_charger_2) *
		100 * val) /
	       gauge->gm->fg_cust_data.r_charger_2) /
	      100;
	*vbus = val * 1000;

	return ret;
}

static int sc8960x_enable_vbus(struct regulator_dev *rdev)
{
	int ret = 0;
	struct sc8960x *sc = rdev->reg_data;

	pr_info("%s\n", __func__);

	ret = sc8960x_disable_charger(sc);
	ret = sc8960x_enable_otg(sc);
    
	return ret;
}

static int sc8960x_disable_vbus(struct regulator_dev *rdev)
{
	int ret = 0;
	struct sc8960x *sc = rdev->reg_data;

	pr_info("%s\n", __func__);

	ret = sc8960x_disable_otg(sc);

	return ret;
}

static int sc8960x_is_enabled_vbus(struct regulator_dev *rdev)
{
	struct sc8960x *sc = rdev->reg_data;
	u8 val = REG01_OTG_ENABLE << REG01_OTG_CONFIG_SHIFT;
	u8 temp = 0;

	pr_info("%s\n", __func__);

	sc8960x_read_byte(sc, SC8960X_REG_01, &temp);
	return (temp & val) ? 1 : 0;
}

static struct regulator_ops sc8960x_vbus_ops = {
	.enable = sc8960x_enable_vbus,
	.disable = sc8960x_disable_vbus,
	.is_enabled = sc8960x_is_enabled_vbus,
};

static struct regulator_desc sc8960x_otg_rdesc = {
	.of_match = "sc8960x,otg-vbus",
	.name = "usb-otg-vbus",
	.ops = &sc8960x_vbus_ops,
	.owner = THIS_MODULE,
	.type = REGULATOR_VOLTAGE,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static int sc8960x_vbus_regulator_register(struct sc8960x *sc)
{
	struct regulator_config config = {};
	int ret = 0;
	/* otg regulator */
	config.dev = sc->dev;
	config.driver_data = sc;
	sc->otg_rdev =
		devm_regulator_register(sc->dev, &sc8960x_otg_rdesc, &config);
	sc->otg_rdev->constraints->valid_ops_mask |= REGULATOR_CHANGE_STATUS;
	if (IS_ERR(sc->otg_rdev)) {
		ret = PTR_ERR(sc->otg_rdev);
		pr_info("%s: register otg regulator failed (%d)\n", __func__,
			ret);
	}
	return ret;
}

static struct charger_ops sc8960x_chg_ops = {
	/* cable plug in/out */
	.plug_in = sc8960x_plug_in,
	.plug_out = sc8960x_plug_out,
	/* enable */
	.enable = sc8960x_charging,
	.is_enabled = sc8960x_is_charging_enable,
	.is_chip_enabled = sc8960x_is_charging_enable, 
	/* charging current */
	.set_charging_current = sc8960x_set_ichg,
	.get_charging_current = sc8960x_get_ichg,
	.get_min_charging_current = sc8960x_get_min_ichg,
	/* charging voltage */
	.set_constant_voltage = sc8960x_set_vchg,
	.get_constant_voltage = sc8960x_get_vchg,
	/* input current limit */
	.set_input_current = sc8960x_set_icl,
	.get_input_current = sc8960x_get_icl,
	.get_min_input_current = NULL,
	/* MIVR */
	.set_mivr = sc8960x_set_ivl,
	.get_mivr = sc8960x_get_ivl,
	.get_mivr_state = NULL,
	/* ADC */
	.get_adc = NULL,
	.get_vbus_adc = sc8960x_get_vbus,
	.get_ibus_adc = sc8960x_get_ibus,
	.get_ibat_adc = NULL,
	.get_tchg_adc = NULL,
	.get_zcv = NULL,
	/* charing termination */
	.set_eoc_current = sc8960x_set_ieoc,
	.enable_termination = sc8960x_enable_te,
	.reset_eoc_state = NULL,
	.safety_check = NULL,
	.is_charging_done = sc8960x_is_charging_done,
	/* power path */
	.enable_powerpath = NULL,
	.is_powerpath_enabled = NULL,
	/* timer */
	.enable_safety_timer = sc8960x_set_safety_timer,
	.is_safety_timer_enabled = sc8960x_is_safety_timer_enabled,
	.kick_wdt = sc8960x_kick_wdt,
	/* AICL */
	.run_aicl = NULL,
	/* PE+/PE+20 */
	.send_ta_current_pattern = NULL,
	.set_pe20_efficiency_table = NULL,
	.send_ta20_current_pattern = sc8960x_set_ta20_current_pattern,
	.reset_ta = sc8960x_set_ta20_reset,
	.enable_cable_drop_comp = NULL,
	/* OTG */
	.set_boost_current_limit = sc8960x_set_boost_ilmt,
	.enable_otg = sc8960x_set_otg,
	.enable_discharge = NULL,
	/* charger type detection */
	.enable_chg_type_det = NULL,
	/* misc */
	.dump_registers = sc8960x_dump_register,
	.enable_hz = sc8960x_enable_hz,
	/* event */
	.event = sc8960x_do_event,
	/* 6pin battery */
	.enable_6pin_battery_charging = NULL,
};

static struct of_device_id sc8960x_charger_match_table[] = {
	{
	.compatible = "sc,sc89601d_charger",
	.data = &pn_data[PN_SC89601D],
	},
	{},
};
MODULE_DEVICE_TABLE(of, sc8960x_charger_match_table);


static int sc8960x_charger_probe(struct i2c_client *client)
{
	struct sc8960x *sc;
	const struct of_device_id *match;
	struct device_node *node = client->dev.of_node;

	int ret = 0;
	
	client->addr = 0x6b;

	//pr_info("%s (%s)\n", __func__, SC8960X_DRV_VERSION);
	pr_info("Mingye: %s (%s)\n", __func__, SC8960X_DRV_VERSION);

	sc = devm_kzalloc(&client->dev, sizeof(struct sc8960x), GFP_KERNEL);
	if (!sc)
		return -ENOMEM;

	sc->dev = &client->dev;
	sc->client = client;

	i2c_set_clientdata(client, sc);

	mutex_init(&sc->i2c_rw_lock);

	ret = sc8960x_detect_device(sc);
	if (ret) {
		pr_err("No sc8960x device found!\n");
		return -ENODEV;
	}

	match = of_match_node(sc8960x_charger_match_table, node);
	if (match == NULL) {
		pr_err("device tree match not found\n");
		return -EINVAL;
	}

	sc->platform_data = sc8960x_parse_dt(node, sc);

	if (!sc->platform_data) {
		pr_err("No platform data provided.\n");
		return -EINVAL;
	}

	ret = sc8960x_chg_init_psy(sc);
	if (ret < 0) {
		pr_err("failed to init power supply\n");
		return ret;
	}

	ret = sc8960x_init_device(sc);
	if (ret) {
		pr_err("Failed to init device\n");
		return ret;
	}

	sc8960x_register_interrupt(sc);

	sc->chg_dev = charger_device_register(sc->chg_dev_name,
						&client->dev, sc,
						&sc8960x_chg_ops,
						&sc8960x_chg_props);
	if (IS_ERR_OR_NULL(sc->chg_dev)) {
		ret = PTR_ERR(sc->chg_dev);
		return ret;
	}

	ret = sysfs_create_group(&sc->dev->kobj, &sc8960x_attr_group);
	if (ret)
		dev_err(sc->dev, "failed to register sysfs. err: %d\n", ret);

	determine_initial_status(sc);

	sc->chan_vcdt_voltage =
		devm_iio_channel_get(sc->dev, "pmic_vcdt_voltage");
	if (IS_ERR(sc->chan_vcdt_voltage)) {
		ret = PTR_ERR(sc->chan_vcdt_voltage);
		dev_err(sc->dev, "chan_vcdt_voltage auxadc get fail, ret=%d\n",
			ret);
	}

	sc8960x_vbus_regulator_register(sc);

	pr_err("sc8960x probe successfully, Part Num:%d, Revision:%d\n!",
		sc->part_no, sc->revision);

	return 0;
}

static void sc8960x_charger_remove(struct i2c_client *client)
{
	struct sc8960x *sc = i2c_get_clientdata(client);

	mutex_destroy(&sc->i2c_rw_lock);

	sysfs_remove_group(&sc->dev->kobj, &sc8960x_attr_group);

	return ;
}

static void sc8960x_charger_shutdown(struct i2c_client *client)
{

}

static struct i2c_driver sc8960x_charger_driver = {
	.driver = {
		.name = "sc8960x-charger",
		.owner = THIS_MODULE,
		.of_match_table = sc8960x_charger_match_table,
		},

	.probe = sc8960x_charger_probe,
	.remove = sc8960x_charger_remove,
	.shutdown = sc8960x_charger_shutdown,

};

module_i2c_driver(sc8960x_charger_driver);

MODULE_DESCRIPTION("SC SC8960X Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("South Chip <Aiden-yu@southchip.com>");

