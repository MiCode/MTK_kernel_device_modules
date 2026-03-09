// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/types.h>
#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#endif
//#include <mt-plat/mtk_boot.h>
//#include <mt-plat/upmu_common.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/power_supply.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include "sgm415xx.h"
#include "charger_class.h"
#include "mtk_charger.h"
/**********************************************************
 *
 *   [I2C Slave Setting]
 *
 *********************************************************/
#define SGM415XX_REG_NUM    (0x0F)
static const struct charger_properties sgm415xx_chg_props = {
		.alias_name = "sgm415xx",	};
static struct sgm_vendor_info s_chg_vendor[MAX_PN_ID] = {
	[SGM41513_PN_ID]   = { "sgm41513",0},
	[SGM41513D_PN_ID]  = { "sgm41513D", BIT(3)},
	[SGM41516_PN_ID]   = { "sgm41516", (BIT(6)| BIT(5))},
	[SGM41516D_PN_ID]  = { "sgm41516D",(BIT(6)| BIT(5)| BIT(3))},
	[SGM41541_PN_ID]   = { "sgm41541", (BIT(6)| BIT(5))},
	[SGM41542_PN_ID]   = { "sgm41542", (BIT(6)| BIT(5)| BIT(3))},
	[SGM41543_PN_ID]   = { "sgm41543",  BIT(6)},
	[SGM41543D_PN_ID]  = { "sgm41543D",(BIT(6)| BIT(3))},
	[SGM41516E_PN_ID]  = { "sgm41516E",(BIT(6)| BIT(5)| BIT(4))},
};
// static const char *const sgm415xx_chrg_types[] = {
// 	"No input",
// 	"Reserved",
// 	"SDP",
//     "CDP",
// 	"DCP",
// 	"UNKNOW",
// 	"Non-standard",
// 	"OTG"
// };
// static const char *const sgm415xx_chrg_stat[] = {
// 	"Charge disable",
// 	"Pre-charge",
// 	"Fast charging",
//     "Charging terminated",
// };
// static const char *const sgm415xx_chrg_fault[] = {
// 	"Normal",
// 	"Input fault (VBUS OVP or VBAT < VVBUS < 3.8V)",
//     "Thermal shutdown",
// 	"Charge safety timer expired",
// };
// static const char *const sgm415xx_ntc_fault[] = {
// 	"Normal",
// 	"Reserved",
//     "warm",
// 	"cold",
// 	"hot",
// };
/* SGM415XX REG06 BOOST_LIM[5:4], uV */
// static const unsigned int BOOST_VOLT_LIMIT[] = {
// 	4850000, 5000000, 5150000, 5300000
// };
/* SGM415XX REG02 BOOST_LIM[7:7], uA */
static const unsigned int BOOST_CURRENT_LIMIT[MAX_PN_ID][2] = {
	{500000, 1200000},{500000, 1200000},{500000, 1200000},{500000, 1200000},
	{1200000, 2000000},{1200000, 2000000},{1200000, 2000000},{1200000, 2000000},
};
static const unsigned int IPRECHG_CURRENT_STABLE[] = {
	5000, 10000, 15000, 20000, 30000, 40000, 50000, 60000,
	80000, 100000, 120000, 140000, 160000, 180000, 200000, 240000
};
static const unsigned int ITERM_CURRENT_STABLE[] = {
	5000, 10000, 15000, 20000, 30000, 40000, 50000, 60000,
	80000, 100000, 120000, 140000, 160000, 180000, 200000, 240000
};
#if 1
static enum power_supply_usb_type sgm415xx_usb_type[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID
};
#endif
/**********************************************************
 *
 *   [Global Variable]
 *
 *********************************************************/
static struct power_supply_desc sgm415xx_power_supply_desc;
static struct charger_device *s_chg_dev;
/**********************************************************
 *
 *   [I2C Function For Read/Write sgm415xx]
 *
 *********************************************************/
static int __sgm415xx_read_byte(struct sgm415xx_device *sgm, u8 reg, u8 *data)
{
    s32 ret;
    ret = i2c_smbus_read_byte_data(sgm->client, reg);
    if (ret < 0) {
        pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
        return ret;
    }
    *data = (u8) ret;
    return 0;
}
static int __sgm415xx_write_byte(struct sgm415xx_device *sgm, int reg, u8 val)
{
    s32 ret;
    ret = i2c_smbus_write_byte_data(sgm->client, reg, val);
    if (ret < 0) {
        pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
               val, reg, ret);
        return ret;
    }
    return 0;
}
static int sgm415xx_read_reg(struct sgm415xx_device *sgm, u8 reg, u8 *data)
{
	int ret;
	mutex_lock(&sgm->i2c_rw_lock);
	ret = __sgm415xx_read_byte(sgm, reg, data);
	mutex_unlock(&sgm->i2c_rw_lock);
	return ret;
}
// #if 0
// static int sgm415xx_write_reg(struct sgm415xx_device *sgm, u8 reg, u8 val)
// {
// 	int ret;
// 	mutex_lock(&sgm->i2c_rw_lock);
// 	ret = __sgm415xx_write_byte(sgm, reg, val);
// 	mutex_unlock(&sgm->i2c_rw_lock);
// 	if (ret)
// 		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
// 	return ret;
// }
// #endif
static int sgm415xx_update_bits(struct sgm415xx_device *sgm, u8 reg,
					u8 mask, u8 val)
{
	int ret;
	u8 tmp;
	mutex_lock(&sgm->i2c_rw_lock);
	ret = __sgm415xx_read_byte(sgm, reg, &tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}
	tmp &= ~mask;
	tmp |= val & mask;
	ret = __sgm415xx_write_byte(sgm, reg, tmp);
	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
out:
	mutex_unlock(&sgm->i2c_rw_lock);
	return ret;
}
/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/
 static int sgm415xx_set_watchdog_timer(struct sgm415xx_device *sgm, int time)
{
	int ret;
	u8 reg_val;
	if (time == 0)
		reg_val = SGM415XX_WDT_TIMER_DISABLE;
	else if (time == 40)
		reg_val = SGM415XX_WDT_TIMER_40S;
	else if (time == 80)
		reg_val = SGM415XX_WDT_TIMER_80S;
	else
		reg_val = SGM415XX_WDT_TIMER_160S;
	ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_5,
				SGM415XX_WDT_TIMER_MASK, reg_val);
	return ret;
}
//  #if 0
//  static int sgm415xx_get_term_curr(struct sgm415xx_device *sgm)
// {
// 	int ret;
// 	u8 reg_val;
// 	int curr;
// 	int offset = SGM415XX_TERMCHRG_I_MIN_uA;
// 	ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_3, &reg_val);
// 	if (ret)
// 		return ret;
// 	reg_val &= SGM415XX_TERMCHRG_CUR_MASK;
// 	curr = reg_val * SGM415XX_TERMCHRG_CURRENT_STEP_uA + offset;
// 	return curr;
// }
// static int sgm415xx_get_prechrg_curr(struct sgm415xx_device *sgm)
// {
// 	int ret;
// 	u8 reg_val;
// 	int curr;
// 	int offset = SGM415XX_PRECHRG_I_MIN_uA;
// 	ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_3, &reg_val);
// 	if (ret)
// 		return ret;
// 	reg_val = (reg_val&SGM415XX_PRECHRG_CUR_MASK)>>4;
// 	curr = reg_val * SGM415XX_PRECHRG_CURRENT_STEP_uA + offset;
// 	return curr;
// }
// #endif
static int sgm415xx_set_term_curr(struct charger_device *chg_dev, u32 uA)
{
	u8 reg_val;
	struct sgm415xx_device *sgm = charger_get_data(chg_dev);
	if (sgm->device_id == SGM41513_PN_ID || sgm->device_id == SGM41513D_PN_ID)
	{
		for(reg_val = 1; reg_val < 16 && uA >= ITERM_CURRENT_STABLE[reg_val]; reg_val++)
		;
		reg_val--;
	}
	else
	{
		if (uA < SGM415XX_TERMCHRG_I_MIN_uA)
			uA = SGM415XX_TERMCHRG_I_MIN_uA;
		else if (uA > SGM415XX_TERMCHRG_I_MAX_uA)
			uA = SGM415XX_TERMCHRG_I_MAX_uA;
		reg_val = (uA - SGM415XX_TERMCHRG_I_MIN_uA) / SGM415XX_TERMCHRG_CURRENT_STEP_uA;
	}
	return sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_3,
				  SGM415XX_TERMCHRG_CUR_MASK, reg_val);
}
static int sgm415xx_set_prechrg_curr(struct sgm415xx_device *sgm, int uA)
{
	u8 reg_val;
	if (sgm->device_id == SGM41513_PN_ID || sgm->device_id == SGM41513D_PN_ID)
	{
		for(reg_val = 1; reg_val < 16 && uA >= IPRECHG_CURRENT_STABLE[reg_val]; reg_val++)
			;
		reg_val--;
	}
	else
	{
		if (uA < SGM415XX_PRECHRG_I_MIN_uA)
			uA = SGM415XX_PRECHRG_I_MIN_uA;
		else if (uA > SGM415XX_PRECHRG_I_MAX_uA)
			uA = SGM415XX_PRECHRG_I_MAX_uA;
		reg_val = (uA - SGM415XX_PRECHRG_I_MIN_uA) / SGM415XX_PRECHRG_CURRENT_STEP_uA;
	}
	reg_val = reg_val << 4;
	return sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_3,
				  SGM415XX_PRECHRG_CUR_MASK, reg_val);
}
static int sgm415xx_get_ichg_curr(struct charger_device *chg_dev, u32 *uA)
{
	int ret;
	u8 ichg;
    u32 curr;
	struct sgm415xx_device *sgm = charger_get_data(chg_dev);
	ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_2, &ichg);
	if (ret)
	{
		dev_err(sgm->dev,"%s,fail\n",__func__);
		return ret;
	}
	ichg &= SGM415XX_ICHRG_I_MASK;
	if (sgm->device_id == SGM41513_PN_ID || sgm->device_id == SGM41513D_PN_ID)
	{
		if (ichg <= 0x8)
			curr = ichg * 5000;
		else if (ichg <= 0xF)
			curr = 40000 + (ichg - 0x8) * 10000;
		else if (ichg <= 0x17)
			curr = 110000 + (ichg - 0xF) * 20000;
		else if (ichg <= 0x20)
			curr = 270000 + (ichg - 0x17) * 30000;
		else if (ichg <= 0x30)
			curr = 540000 + (ichg - 0x20) * 60000;
		else if (ichg <= 0x3C)
			curr = 1500000 + (ichg - 0x30) * 120000;
		else
			curr = 3000000;
	}
	else
		curr = ichg * SGM415XX_ICHRG_I_STEP_uA;
	*uA = curr;
	return 0;
}
static int sgm415xx_get_minichg_curr(struct charger_device *chg_dev, u32 *uA)
{
	*uA = SGM415XX_ICHRG_I_MIN_uA;
	return 0;
}
static int sgm415xx_set_ichrg_curr(struct charger_device *chg_dev, unsigned int uA)
{
	int ret;
	u8 reg_val;
	struct sgm415xx_device *sgm = charger_get_data(chg_dev);
	pr_err("%s set ichg = %d.\n",__func__, uA);
	if (uA < SGM415XX_ICHRG_I_MIN_uA)
		uA = SGM415XX_ICHRG_I_MIN_uA;
	else if ( uA > sgm->init_data.max_ichg)
		uA = sgm->init_data.max_ichg;
	if (sgm->device_id == SGM41513_PN_ID || sgm->device_id == SGM41513D_PN_ID)
	{
		if (uA <= 40000)
			reg_val = uA / 5000;
		else if (uA <= 110000)
			reg_val = 0x08 + (uA -40000) / 10000;
		else if (uA <= 270000)
			reg_val = 0x0F + (uA -110000) / 20000;
		else if (uA <= 540000)
			reg_val = 0x17 + (uA -270000) / 30000;
		else if (uA <= 1500000)
			reg_val = 0x20 + (uA -540000) / 60000;
		else if (uA <= 2940000)
			reg_val = 0x30 + (uA -1500000) / 120000;
		else
			reg_val = 0x3d;
	}
	else
		reg_val = uA / SGM415XX_ICHRG_I_STEP_uA;
	ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_2,
				  SGM415XX_ICHRG_I_MASK, reg_val);
	return ret;
}
static int sgm415xx_set_chrg_volt(struct charger_device *chg_dev, u32 chrg_volt)
{
	int ret;
	u8 reg_val;
	struct sgm415xx_device *sgm = charger_get_data(chg_dev);
	if (chrg_volt < SGM415XX_VREG_V_MIN_uV)
		chrg_volt = SGM415XX_VREG_V_MIN_uV;
	else if (chrg_volt > SGM415XX_VREG_V_MAX_uV)
		chrg_volt = SGM415XX_VREG_V_MAX_uV;
	reg_val = (chrg_volt-SGM415XX_VREG_V_MIN_uV) / SGM415XX_VREG_V_STEP_uV;
	reg_val = reg_val<<3;
	ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_4,
				  SGM415XX_VREG_V_MASK, reg_val);
	return ret;
}
static int sgm415xx_get_chrg_volt(struct charger_device *chg_dev,unsigned int *volt)
{
	int ret;
	u8 vreg_val;
	struct sgm415xx_device *sgm = charger_get_data(chg_dev);
	ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_4, &vreg_val);
	if (ret)
		return ret;
	vreg_val = (vreg_val & SGM415XX_VREG_V_MASK)>>3;
	if (15 == vreg_val)
		*volt = 4352000; //default
	else if (vreg_val < 25)
		*volt = vreg_val*SGM415XX_VREG_V_STEP_uV + SGM415XX_VREG_V_MIN_uV;
	return 0;
}
static int sgm415xx_get_vindpm_offset_os(struct sgm415xx_device *sgm)
{
	int ret;
	u8 reg_val;
	ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_f, &reg_val);
	if (ret)
		return ret;
	reg_val = reg_val & SGM415XX_VINDPM_OS_MASK;
	return reg_val;
}
static int sgm415xx_set_vindpm_offset_os(struct sgm415xx_device *sgm,u8 offset_os)
{
	int ret;
	ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_f,
				  SGM415XX_VINDPM_OS_MASK, offset_os);
	if (ret){
		pr_err("%s fail\n",__func__);
		return ret;
	}
	return ret;
}
static int sgm415xx_set_input_volt_lim(struct charger_device *chg_dev, unsigned int vindpm)
{
	int ret;
	unsigned int offset;
	u8 reg_val;
	u8 os_val;
	struct sgm415xx_device *sgm = charger_get_data(chg_dev);
	if (vindpm < SGM415XX_VINDPM_V_MIN_uV ||
	    vindpm > SGM415XX_VINDPM_V_MAX_uV)
		return -EINVAL;
	if (vindpm < 5900000){
		os_val = 0;
		offset = 3900000;
	}
	else if (vindpm >= 5900000 && vindpm < 7500000){
		os_val = 1;
		offset = 5900000; //uv
	}
	else if (vindpm >= 7500000 && vindpm < 10500000){
		os_val = 2;
		offset = 7500000; //uv
	}
	else{
		os_val = 3;
		offset = 10500000; //uv
	}
	sgm415xx_set_vindpm_offset_os(sgm,os_val);
	reg_val = (vindpm - offset) / SGM415XX_VINDPM_STEP_uV;
	ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_6,
				  SGM415XX_VINDPM_V_MASK, reg_val);
	return ret;
}
static int sgm415xx_get_input_volt_lim(struct charger_device *chg_dev, u32 *uV)
{
	int ret;
	int offset;
	u8 vlim;
	int temp;
	struct sgm415xx_device *sgm = charger_get_data(chg_dev);
	ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_6, &vlim);
	if (ret)
	{
		dev_err(sgm->dev,"%s,fail\n",__func__);
		return ret;
	}
	temp = sgm415xx_get_vindpm_offset_os(sgm);
	if (0 == temp)
		offset = 3900000; //uv
	else if (1 == temp)
		offset = 5900000;
	else if (2 == temp)
		offset = 7500000;
	else if (3 == temp)
		offset = 10500000;
	else
		return temp;
	*uV = offset + (vlim & 0x0F) * SGM415XX_VINDPM_STEP_uV;
	return 0;
}
static int sgm415xx_get_input_minvolt_lim(struct charger_device *chg_dev, bool *uV)
{
	struct sgm415xx_device *sgm = charger_get_data(chg_dev);
	u8 reg_val = 0;
	int ret = 0;
	ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_a, &reg_val);
	*uV = ((reg_val >> 6) & 0x01);
	return ret;
}
static int sgm415xx_set_input_curr_lim(struct charger_device *chg_dev, unsigned int iindpm)
{
	int ret;
	u8 reg_val;
	struct sgm415xx_device *sgm = charger_get_data(chg_dev);
	if (iindpm < SGM415XX_IINDPM_I_MIN_uA ||
			iindpm > sgm->init_data.max_ilim)
		return -EINVAL;
	if (sgm->device_id == SGM41513_PN_ID || sgm->device_id == SGM41513D_PN_ID)
		reg_val = (iindpm-SGM415XX_IINDPM_I_MIN_uA) / SGM415XX_IINDPM_STEP_uA;
	else
	{
		if (iindpm >= SGM415XX_IINDPM_I_MIN_uA && iindpm <= 3100000)//default
			reg_val = (iindpm-SGM415XX_IINDPM_I_MIN_uA) / SGM415XX_IINDPM_STEP_uA;
		else if (iindpm > 3100000 && iindpm < sgm->init_data.max_ilim)
			reg_val = 0x1E;
		else
			reg_val = SGM415XX_IINDPM_I_MASK;
	}
	ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_0,
				  SGM415XX_IINDPM_I_MASK, reg_val);
	return ret;
}
static int sgm415xx_get_input_curr_lim(struct charger_device *chg_dev,unsigned int *ilim)
{
	int ret;
	u8 reg_val;
	struct sgm415xx_device *sgm = charger_get_data(chg_dev);
	ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_0, &reg_val);
	if (ret)
	{
		dev_err(sgm->dev,"%s,fail\n",__func__);
		return ret;
	}
	if (sgm->device_id == SGM41513_PN_ID || sgm->device_id == SGM41513D_PN_ID)
		*ilim = (reg_val & SGM415XX_IINDPM_I_MASK)*SGM415XX_IINDPM_STEP_uA + SGM415XX_IINDPM_I_MIN_uA;
	else
	{
		if (SGM415XX_IINDPM_I_MASK == (reg_val & SGM415XX_IINDPM_I_MASK))
			*ilim =  sgm->init_data.max_ilim;
		else
			*ilim = (reg_val & SGM415XX_IINDPM_I_MASK)*SGM415XX_IINDPM_STEP_uA + SGM415XX_IINDPM_I_MIN_uA;
	}
	return 0;
}
static int sgm415xx_get_input_mincurr_lim(struct charger_device *chg_dev,u32 *ilim)
{
	*ilim = SGM415XX_IINDPM_I_MIN_uA;
	return 0;
}
static int sgm415xx_get_state(struct sgm415xx_device *sgm,
			     struct sgm415xx_state *state)
{
	u8 status;
	u8 fault;
	u8 chrg_param;
	int ret;
	ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_STAT, &status);
	if (ret){
		ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_STAT, &status);
		if (ret){
			pr_err("%s read SGM415XX_CHRG_STAT fail\n",__func__);
			return ret;
		}
	}
	state->status = status;
	pr_info("SGM415xx_CHRG_STAT[0x%x]: 0x%x\n", SGM415XX_CHRG_STAT, status);
	state->vbus_type = status & SGM415XX_VBUS_STAT_MASK;
	state->chrg_stat = status & SGM415XX_CHG_STAT_MASK;
	state->online = !!(status & SGM415XX_PG_STAT);
	state->therm_stat = !!(status & SGM415XX_THERM_STAT);
	state->vsys_stat = !!(status & SGM415XX_VSYS_STAT);
	ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_FAULT, &fault);
	if (ret){
		pr_err("%s read SGM415XX_CHRG_FAULT fail\n",__func__);
		return ret;
	}
	if (fault != 0) {
		pr_info("SGM415xx_CHRG_FAULT_9[0x%x]: 0x%x\n", SGM415XX_CHRG_FAULT, fault);
	}
	state->fault_status = fault;
	state->wdt_fault = !!(fault & SGM415XX_WDT_FAULT_MASK);
	state->boost_fault = !!(fault & SGM415XX_BOOST_FAULT_MASK);
	state->chrg_fault = (fault & SGM415XX_CHRG_FAULT_MASK) >> SGM415XX_CHRG_FAULT_SHIFT;
	state->bat_fault = !!(fault & SGM415XX_BAT_FAULT_MASK);
	state->ntc_fault = fault & SGM415XX_TEMP_MASK;
	state->health = state->ntc_fault;
	ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_a, &chrg_param);
	if (ret){
		pr_err("%s read SGM415XX_CHRG_CTRL_a fail\n",__func__);
		return ret;
	}
	state->vbus_gd = !!(chrg_param & SGM415XX_VBUS_GOOD);
	pr_info("SGM415xx_CHRG_CTRL_a[0x%x]: 0x%x\n", SGM415XX_CHRG_CTRL_a, chrg_param);
	// pr_info("vbus_type: %s ,chrg_stat: %s\n", sgm415xx_chrg_types[state->vbus_type >> SGM415XX_VBUS_STAT_SHIFT],sgm415xx_chrg_stat[state->chrg_stat >> SGM415XX_CHG_STAT_SHIFT]);
	// pr_info("online: 0x%x ,therm_stat: 0x%x,vsys_stat: 0x%x\n", state->online,state->therm_stat,state->vsys_stat);
	// pr_info("wdt_fault: 0x%x,boost_fault: 0x%x,bat_fault: 0x%x\n", state->wdt_fault,state->boost_fault,state->bat_fault);
	// pr_info("chrg_fault: %s,ntc_fault: %s\n", sgm415xx_chrg_fault[state->chrg_fault],sgm415xx_ntc_fault[state->ntc_fault]);
	// pr_info("vbus_gd: 0x%x\n", state->vbus_gd);
	return 0;
}
// #if 0
// static int sgm415xx_set_hiz_en(struct charger_device *chg_dev, bool hiz_en)
// {
// 	u8 reg_val;
// 	struct sgm415xx_device *sgm = charger_get_data(chg_dev);
// 	dev_notice(sgm->dev, "%s:%d", __func__, hiz_en);
// 	reg_val = hiz_en ? SGM415XX_HIZ_EN : 0;
// 	return sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_0,
// 				  SGM415XX_HIZ_EN, reg_val);
// }
// #endif
static int sgm415xx_enable_charger(struct sgm415xx_device *sgm)
{
    int ret;
    ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_1, SGM415XX_CHRG_EN,
                     SGM415XX_CHRG_EN);
    return ret;
}
static int sgm415xx_disable_charger(struct sgm415xx_device *sgm)
{
    int ret;
    ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_1, SGM415XX_CHRG_EN,
                     0);
    return ret;
}
static int sgm415xx_is_charging(struct charger_device *chg_dev,bool *en)
{
	int ret;
	u8 val;
	struct sgm415xx_device *sgm = charger_get_data(chg_dev);
	ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_1, &val);
	if (ret){
		pr_err("%s read SGM415XX_CHRG_CTRL_a fail\n",__func__);
		return ret;
	}
	*en = (val&SGM415XX_CHRG_EN)? 1 : 0;
	return ret;
}
static int sgm415xx_charging_switch(struct charger_device *chg_dev,bool enable)
{
	int ret;
	struct sgm415xx_device *sgm = charger_get_data(chg_dev);
	if (enable)
		ret = sgm415xx_enable_charger(sgm);
	else
		ret = sgm415xx_disable_charger(sgm);
	power_supply_changed(sgm->charger);
	pr_err("%s set charger %s.\n",__func__, enable ? "enable" : "disable");
	return ret;
}
static int sgm415xx_set_recharge_volt(struct sgm415xx_device *sgm, int mV)
{
	u8 reg_val;
	reg_val = (mV - SGM415XX_VRECHRG_OFFSET_mV) / SGM415XX_VRECHRG_STEP_mV;
	return sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_4,
				  SGM415XX_VRECHARGE, reg_val);
}
static int sgm415xx_set_wdt_rst(struct sgm415xx_device *sgm, bool is_rst)
{
	u8 val;
	if (is_rst)
		val = SGM415XX_WDT_RST_MASK;
	else
		val = 0;
	return sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_1,
				  SGM415XX_WDT_RST_MASK, val);
}
/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/
static int sgm415xx_dump_register(struct charger_device *chg_dev)
{
	unsigned char i = 0;
	unsigned int ret = 0;
	unsigned char sgm415xx_reg[SGM415XX_REG_NUM+1] = { 0 };
	struct sgm415xx_device *sgm = charger_get_data(chg_dev);
	for (i = 0; i < SGM415XX_REG_NUM+1; i++) {
		if (i == 0x0E) continue;
		ret = sgm415xx_read_reg(sgm,i, &sgm415xx_reg[i]);
		if (ret != 0) {
			pr_info("[sgm415xx] i2c transfor error\n");
			return 1;
		}
		pr_info("%s,[0x%x]=0x%x ",__func__, i, sgm415xx_reg[i]);
	}
	return 0;
}
/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/
static bool sgm415xx_hw_chipid_detect(struct sgm415xx_device *sgm)
{
	int ret = 0;
	u8 val = 0;
	int index = 0;
	ret = sgm415xx_read_reg(sgm,SGM415XX_CHRG_CTRL_b,&val);
	if (ret < 0)
	{
		pr_info("[%s] read fail\n", __func__);
		return false;
	}
	val = val & SGM415XX_PN_MASK;
	for (index = 0; index < MAX_PN_ID; index++ )
	{
		if (s_chg_vendor[index].part_id == val)
		{
			if (sgm->client->addr != SGM41516_SLAVE_ADDR && (index == SGM41516_PN_ID || index == SGM41516D_PN_ID))
				continue;
			sgm->device_id = index;
			pr_info("[%s] device name %s\n", __func__,s_chg_vendor[index].name);
			if (sgm->device_id == SGM41513_PN_ID || sgm->device_id == SGM41513D_PN_ID)
			{
				sgm->init_data.max_ichg = 3000000;
				sgm->init_data.max_ilim = 3200000;
			}
			else
			{
				sgm->init_data.max_ichg = 3780000;
				sgm->init_data.max_ilim = 3800000;
			}
			return true;
		}
	}
	pr_info("[%s] don't found device ,Reg[0x0B]=0x%x\n", __func__,val);
	return false;
}
static int sgm415xx_reset_watch_dog_timer(struct charger_device
		*chg_dev)
{
	int ret;
	struct sgm415xx_device *sgm = charger_get_data(chg_dev);
	pr_info("charging_reset_watch_dog_timer\n");
	ret = sgm415xx_set_wdt_rst(sgm,true);	/* RST watchdog */
	return ret;
}
static int sgm415xx_get_charging_status(struct charger_device *chg_dev,
				       bool *is_done)
{
	struct sgm415xx_device *sgm = charger_get_data(chg_dev);
	if (sgm->state.chrg_stat == SGM415XX_TERM_CHRG)
		*is_done = true;
	else
		*is_done = false;
	return 0;
}
static int sgm415xx_enable_safetytimer(struct charger_device *chg_dev,bool en)
{
	struct sgm415xx_device *sgm = charger_get_data(chg_dev);
	int ret = 0;
	ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_5,
				SGM415XX_SAFETY_TIMER_EN, en ? SGM415XX_SAFETY_TIMER_EN : 0);
	return ret;
}
static int sgm415xx_get_is_safetytimer_enable(struct charger_device
		*chg_dev,bool *en)
{
	int ret = 0;
	u8 val = 0;
	struct sgm415xx_device *sgm = charger_get_data(chg_dev);
	ret = sgm415xx_read_reg(sgm,SGM415XX_CHRG_CTRL_5,&val);
	if (ret < 0)
	{
		pr_info("[%s] read SGM415XX_CHRG_CTRL_5 fail\n", __func__);
		return ret;
	}
	*en = !!(val & SGM415XX_SAFETY_TIMER_EN);
	return 0;
}
static int sgm415xx_en_pe_current_partern(struct charger_device
		*chg_dev,bool is_up)
{
	int ret = 0;
	int val;
	struct sgm415xx_device *sgm = charger_get_data(chg_dev);
	if (sgm->device_id == SGM41542_PN_ID || sgm->device_id == SGM41516D_PN_ID || sgm->device_id == SGM41543D_PN_ID)
	{
		if (is_up)
			val = SGM415XX_EN_PUMPX | SGM415XX_PUMPX_UP;
		else
			val = SGM415XX_EN_PUMPX | SGM415XX_PUMPX_DN;
		ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_d,
					SGM415XX_EN_PUMPX, val);
		if (ret < 0)
		{
			dev_err(sgm->dev,"[%s] increase or reduce voltage  fail\n", __func__);
			return ret;
		}
		return 0;
	}
	else
	{
		dev_err(sgm->dev,"%s ,not support\n",__func__);
		return 0;
	}
}
static enum power_supply_property sgm415xx_power_supply_props[] = {
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	//POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
};
static int sgm415xx_property_is_writeable(struct power_supply *psy,
					 enum power_supply_property prop)
{
	switch (prop) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
	case POWER_SUPPLY_PROP_STATUS:
		return true;
	default:
		return false;
	}
}
static int sgm415xx_charger_set_property(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val)
{
	//struct sgm415xx_device *sgm = power_supply_get_drvdata(psy);
	int ret = 0;
	switch (prop) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = sgm415xx_set_input_curr_lim(s_chg_dev, val->intval);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		sgm415xx_charging_switch(s_chg_dev,!!val->intval);
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		ret = sgm415xx_set_input_volt_lim(s_chg_dev, val->intval);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		ret = sgm415xx_charging_switch(s_chg_dev,!!val->intval);
		break;
	default:
		return -EINVAL;
	}
	return ret;
}
static int sgm415xx_charger_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct sgm415xx_device *sgm = power_supply_get_drvdata(psy);
	struct sgm415xx_state state;
	int ret = 0;
	u32 value;
	u8 st;
	mutex_lock(&sgm->lock);
	//ret = sgm415xx_get_state(sgm, &state);
	state = sgm->state;
	mutex_unlock(&sgm->lock);
	if (ret)
		return ret;
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		sgm415xx_read_reg(sgm, SGM415XX_CHRG_STAT, &st);
		pr_info("%s, SGM415xx_CHRG_STAT[0x%x]: 0x%x\n", __func__, SGM415XX_CHRG_STAT, st);
		state.status = st;
		state.vbus_type = st & SGM415XX_VBUS_STAT_MASK;
		state.chrg_stat = st & SGM415XX_CHG_STAT_MASK;
		state.online = !!(st & SGM415XX_PG_STAT);
		state.therm_stat = !!(st & SGM415XX_THERM_STAT);
		state.vsys_stat = !!(st & SGM415XX_VSYS_STAT);
		sgm->state = state;
		//pr_info("%s, state->chrg_stat: %d.\n", __func__, state.chrg_stat);
		if (!state.vbus_type || (state.vbus_type == SGM415XX_OTG_MODE))
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		else if (!state.chrg_stat)
			val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		else if (state.chrg_stat == SGM415XX_TERM_CHRG)
			val->intval = POWER_SUPPLY_STATUS_FULL;
		else
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		switch (state.chrg_stat) {
		case SGM415XX_PRECHRG:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			break;
		case SGM415XX_FAST_CHRG:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
			break;
		case SGM415XX_TERM_CHRG:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			break;
		case SGM415XX_NOT_CHRGING:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
			break;
		default:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
		}
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = SGM415XX_MANUFACTURER;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = s_chg_vendor[sgm->device_id].name;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = state.online;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = state.vbus_gd;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = sgm->psy_desc.type;
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		val->intval = sgm->psy_usb_type;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if (state.chrg_fault & 0xF8)
			val->intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		else
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
		switch (state.health) {
		case SGM415XX_TEMP_HOT:
			val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
			break;
		case SGM415XX_TEMP_WARM:
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
			break;
		case SGM415XX_TEMP_COOL:
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
			break;
		case SGM415XX_TEMP_COLD:
			val->intval = POWER_SUPPLY_HEALTH_COLD;
			break;
		}
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = sgm415xx_get_chrg_volt(sgm->chg_dev, &(val->intval));
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = sgm415xx_get_ichg_curr(sgm->chg_dev, &(val->intval));
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
  		ret = sgm415xx_get_ichg_curr(sgm->chg_dev, &(val->intval));
  		break;
  	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
  		ret = sgm415xx_get_chrg_volt(sgm->chg_dev, &(val->intval));
  		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		ret = sgm415xx_get_input_volt_lim(s_chg_dev,&value);
		if (ret < 0)
			return ret;
		val->intval = value;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		sgm415xx_get_input_curr_lim(s_chg_dev,&value);
		if (ret < 0)
			return ret;
		val->intval = value;
		break;
	default:
		return -EINVAL;
	}
	sgm415xx_dump_register(sgm->chg_dev);
	return ret;
}
static bool sgm415xx_state_changed(struct sgm415xx_device *sgm,
				  struct sgm415xx_state *new_state)
{
	struct sgm415xx_state old_state;
	mutex_lock(&sgm->lock);
	old_state = sgm->state;
	mutex_unlock(&sgm->lock);
	return (old_state.status!= new_state->status ||
		old_state.fault_status != new_state->fault_status);
}
static bool sgm415xx_dpdm_detect_is_done(struct sgm415xx_device *sgm)
{
	u8 chrg_flag, ret,val;
	ret = sgm415xx_read_reg(sgm, SGM415XX_INPUT_DET, &chrg_flag);
	if(ret) {
		dev_err(sgm->dev, "Check DPDM detecte error\n");
		return false;
	}
	ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_7, &val);
	if(ret) {
		dev_err(sgm->dev, "Check DPDM detecte error\n");
		return false;
	}
	if (chrg_flag & SGM415XX_INPUT_DET_DONE)
		return true;
	else
		return false;
}
static void charger_monitor_work_func(struct work_struct *work)
{
	int ret = 0;
	struct sgm415xx_device * sgm = NULL;
	struct delayed_work *charge_monitor_work = NULL;
	//static u8 last_chg_method = 0;
	struct sgm415xx_state state;
	charge_monitor_work = container_of(work, struct delayed_work, work);
	if(charge_monitor_work == NULL) {
		pr_err("Cann't get charge_monitor_work\n");
		return ;
	}
	sgm = container_of(charge_monitor_work, struct sgm415xx_device, charge_monitor_work);
	if(sgm == NULL) {
		pr_err("Cann't get sgm \n");
		return ;
	}
	ret = sgm415xx_get_state(sgm, &state);
	if (ret){
			pr_err("%s  sgm415xx_get_state fail\n",__func__);
		}
	mutex_lock(&sgm->lock);
	sgm->state = state;
	mutex_unlock(&sgm->lock);
	if(!sgm->state.vbus_gd) {
		dev_err(sgm->dev, "Vbus not present, disable charge\n");
		sgm415xx_disable_charger(sgm);
		goto OUT;
	}
	if(!state.online)
	{
		dev_err(sgm->dev, "Vbus not online\n");
		goto OUT;
	}
	sgm415xx_dump_register(sgm->chg_dev);
	pr_err("%s\n",__func__);
OUT:
	schedule_delayed_work(&sgm->charge_monitor_work, 10*HZ);
}
static void charger_detect_work_func(struct work_struct *work)
{
	struct delayed_work *charge_detect_delayed_work = NULL;
	struct sgm415xx_device * sgm = NULL;
	//static int charge_type_old = 0;
	int curr_in_limit = 0;
	pr_info("[%s]\n", __func__);
	charge_detect_delayed_work = container_of(work, struct delayed_work, work);
	if(charge_detect_delayed_work == NULL) {
		pr_err("Cann't get charge_detect_delayed_work\n");
		return ;
	}
	sgm = container_of(charge_detect_delayed_work, struct sgm415xx_device, charge_detect_delayed_work);
	if(sgm == NULL) {
		pr_err("Cann't get sgm415xx_device\n");
		return ;
	}
	if(!sgm->state.vbus_gd) {
		dev_err(sgm->dev, "Vbus not present, disable charge\n");
		sgm415xx_disable_charger(sgm);
		goto err;
	}
	if(!sgm->state.online)
	{
		dev_err(sgm->dev, "Vbus not online\n");
		goto err;
	}
	if(!sgm415xx_dpdm_detect_is_done(sgm)) {
		dev_err(sgm->dev, "DPDM detecte not done.\n");
		goto err;
	}
	if (SGM41513D_PN_ID == sgm->device_id || SGM41516D_PN_ID == sgm->device_id || SGM41543D_PN_ID == sgm->device_id ||
													SGM41542_PN_ID == sgm->device_id || SGM41516E_PN_ID == sgm->device_id)
	{
		switch(sgm->state.vbus_type) {
			case SGM415XX_USB_SDP:
			pr_info("SGM415xx charger type: SDP\n");
			sgm->psy_desc.type = POWER_SUPPLY_TYPE_USB;
			sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
			curr_in_limit = 500000;
			break;
		case SGM415XX_USB_CDP:
			pr_info("SGM415xx charger type: CDP\n");
			curr_in_limit = 1000000;
			sgm->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
			sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_CDP;
			break;
		case SGM415XX_USB_DCP:
			pr_info("SGM415xx charger type: DCP\n");
			sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
			sgm->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
			curr_in_limit = 2000000;
			break;
		case SGM415XX_UNKNOWN:
			sgm->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		 	sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
			pr_info("SGM415xx charger type: UNKNOWN\n");
			curr_in_limit = 500000;
			break;
		case SGM415XX_NON_STANDARD:
			sgm->psy_desc.type = POWER_SUPPLY_TYPE_USB;
			sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
			pr_info("SGM415xx charger type: NON_STANDARD\n");
			curr_in_limit = 900000;
			break;
		case SGM415XX_OTG_MODE:
			pr_info("SGM415xx charger type: OTG\n");
			sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
			break;
		default:
			pr_info("SGM415xx charger type: default\n");
			sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
			curr_in_limit = 500000;
			break;
		}
		//set charge parameters
		pr_info("Update: curr_in_limit = %d\n", curr_in_limit);
		sgm415xx_set_input_curr_lim(sgm->chg_dev, curr_in_limit);
		sgm415xx_set_ichrg_curr(sgm->chg_dev, curr_in_limit);
	}
	//enable charge
	sgm415xx_enable_charger(sgm);
err:
	//release wakelock
	//power_supply_changed(sgm->charger);
	//mdelay(500);
	power_supply_changed(sgm->charger);
	dev_err(sgm->dev, "Relax wakelock\n");
	__pm_relax(sgm->charger_wakelock);
	return;
}
static irqreturn_t sgm415xx_irq_handler_thread(int irq, void *private)
{
	struct sgm415xx_device *sgm = private;
	struct sgm415xx_state state;
	//lock wakelock
	pr_info("%s entry\n",__func__);
    if (!sgm->charger_wakelock->active)
		__pm_stay_awake(sgm->charger_wakelock);
	sgm415xx_get_state(sgm, &state);
	if (!sgm415xx_state_changed(sgm,&state))
	{
		__pm_relax(sgm->charger_wakelock);
		return IRQ_HANDLED;
	}
	mutex_lock(&sgm->lock);
	sgm->state = state;
	mutex_unlock(&sgm->lock);
	schedule_delayed_work(&sgm->charge_detect_delayed_work, 10);
	return IRQ_HANDLED;
}
static char *sgm415xx_charger_supplied_to[] = {
	"main-battery",
	 "battery",
	 "mtk-master-charger"
};
static struct power_supply_desc sgm415xx_power_supply_desc = {
	.name = "primary_chg",
	.type = POWER_SUPPLY_TYPE_USB,
	.usb_types = sgm415xx_usb_type,
	.num_usb_types = ARRAY_SIZE(sgm415xx_usb_type),
	.properties = sgm415xx_power_supply_props,
	.num_properties = ARRAY_SIZE(sgm415xx_power_supply_props),
	.get_property = sgm415xx_charger_get_property,
	.set_property = sgm415xx_charger_set_property,
	.property_is_writeable = sgm415xx_property_is_writeable,
};
static int sgm415xx_power_supply_init(struct sgm415xx_device *sgm,
							struct device *dev)
{
	struct power_supply_config psy_cfg = { .drv_data = sgm,
						.of_node = dev->of_node, };
	psy_cfg.supplied_to = sgm415xx_charger_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(sgm415xx_charger_supplied_to);
	sgm->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
	sgm->charger = devm_power_supply_register(sgm->dev,
						 &sgm415xx_power_supply_desc,
						 &psy_cfg);
	if (IS_ERR(sgm->charger))
		return -EINVAL;
	return 0;
}
static int sgm415xx_hw_init(struct sgm415xx_device *sgm)
{
	int ret = 0;
	struct power_supply_battery_info bat_info = { };
	bat_info.constant_charge_current_max_ua = 1000000;
	bat_info.constant_charge_voltage_max_uv = 4350000;
	bat_info.precharge_current_ua =
			SGM415XX_PRECHRG_I_DEF_uA;
	bat_info.charge_term_current_ua =
			SGM415XX_TERMCHRG_I_DEF_uA;
	sgm415xx_set_watchdog_timer(sgm,0);
	sgm415xx_disable_charger(sgm);
	ret = sgm415xx_set_ichrg_curr(s_chg_dev,
				bat_info.constant_charge_current_max_ua);
	if (ret)
		goto err_out;
	ret = sgm415xx_set_prechrg_curr(sgm, bat_info.precharge_current_ua);
	if (ret)
		goto err_out;
	ret = sgm415xx_set_chrg_volt(s_chg_dev,
				bat_info.constant_charge_voltage_max_uv);
	if (ret)
		goto err_out;
	ret = sgm415xx_set_term_curr(s_chg_dev, bat_info.charge_term_current_ua);
	if (ret)
		goto err_out;
	ret = sgm415xx_set_input_volt_lim(s_chg_dev, sgm->init_data.vlim);
	if (ret)
		goto err_out;
	ret = sgm415xx_set_input_curr_lim(s_chg_dev, sgm->init_data.ilim);
	if (ret)
		goto err_out;
	// #if 0
	// ret = sgm415xx_set_vac_ovp(sgm);//14V
	// if (ret)
	// 	goto err_out;
	// #endif
	ret = sgm415xx_set_recharge_volt(sgm, 200);//100~200mv
	if (ret)
		goto err_out;
	__sgm415xx_write_byte(sgm, SGM415XX_CHRG_CTRL_a, 0x03);
	dev_notice(sgm->dev, "ichrg_curr:%d prechrg_curr:%d chrg_vol:%d"
		" term_curr:%d input_curr_lim:%d",
		bat_info.constant_charge_current_max_ua,
		bat_info.precharge_current_ua,
		bat_info.constant_charge_voltage_max_uv,
		bat_info.charge_term_current_ua,
		sgm->init_data.ilim);
	return 0;
err_out:
	return ret;
}
static int sgm415xx_parse_dt(struct sgm415xx_device *sgm)
{
	int ret;
	int irq_gpio = 0, irqn = 0;
	// int chg_en_gpio = 0;
	ret = device_property_read_u32(sgm->dev,
				       "sgm,input-voltage-limit-microvolt",
				       &sgm->init_data.vlim);
	if (ret)
		sgm->init_data.vlim = 4600000;
	if (sgm->init_data.vlim > SGM415XX_VINDPM_V_MAX_uV ||
	    sgm->init_data.vlim < SGM415XX_VINDPM_V_MIN_uV)
		return -EINVAL;
	ret = device_property_read_u32(sgm->dev,
				       "sgm,input-voltage-max-ilim-microvolt",
				       &sgm->init_data.max_ilim);
	if (ret)
		sgm->init_data.max_ilim = 3200000;
	ret = device_property_read_u32(sgm->dev,
				       "sgm,input-current-limit-microamp",
				       &sgm->init_data.ilim);
	if (ret)
		sgm->init_data.ilim = 500000;
	if (sgm->init_data.ilim > sgm->init_data.max_ilim ||
	    sgm->init_data.ilim < SGM415XX_IINDPM_I_MIN_uA)
		return -EINVAL;
	//irq_gpio = of_get_named_gpio(sgm->dev->of_node, "sgm,irq-gpio", 0);
	irq_gpio = of_get_named_gpio(sgm->dev->of_node, "sgm,irq-gpio", 0);
	if (!gpio_is_valid(irq_gpio))
	{
		dev_err(sgm->dev, "%s: %d gpio get failed\n", __func__, irq_gpio);
		return -EINVAL;
	}
	ret = gpio_request(irq_gpio, "sgm415xx irq pin");
	if (ret) {
		dev_err(sgm->dev, "%s: %d gpio request failed\n", __func__, irq_gpio);
		return ret;
	}
	gpio_direction_input(irq_gpio);
	irqn = gpio_to_irq(irq_gpio);
	if (irqn < 0) {
		dev_err(sgm->dev, "%s:%d gpio_to_irq failed\n", __func__, irqn);
		return irqn;
	}
	sgm->client->irq = irqn;
	// chg_en_gpio = of_get_named_gpio(sgm->dev->of_node, "sgm,chg-en-gpio", 0);
	// if (!gpio_is_valid(chg_en_gpio))
	// {
	// 	pr_info( "%s: __line=%d gpio get failed \n", __func__,__LINE__);
	// 	dev_err(sgm->dev, "%s: %d gpio get failed\n", __func__, chg_en_gpio);
	// 	return -EINVAL;
	// }
	// ret = gpio_request(chg_en_gpio, "sgm chg en pin");
	// if (ret) {
	// 	pr_info( "%s: __line=%d gpio request failed \n", __func__,__LINE__);
	// 	dev_err(sgm->dev, "%s: %d gpio request failed\n", __func__, chg_en_gpio);
	// 	return ret;
	// }
	// gpio_direction_output(chg_en_gpio,0);//default enable charge
	return 0;
}
/*
__maybe_unused static int sgm415xx_is_enabled_otg()
{
	u8 temp = 0;
	int ret = 0;
	struct sgm415xx_device *sgm = charger_get_data(s_chg_dev);
	ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_1, &temp);
	return (temp&SGM415XX_OTG_EN)? 1 : 0;
}*/
static int sgm415xx_enable_otg(struct charger_device *chg_dev, bool en)
{
	int ret = 0;
	struct sgm415xx_device *sgm = charger_get_data(s_chg_dev);
	pr_info("%s en = %d\n", __func__, en);
	ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_1, SGM415XX_OTG_EN,
                     en ? SGM415XX_OTG_EN : 0);
	return ret;
}
// #if 0
// static int sgm415xx_set_boost_voltage_limit(struct charger_device
// 		*chg_dev, u32 uV)
// {
// 	int ret = 0;
// 	char reg_val = -1;
// 	int i = 0;
// 	struct sgm415xx_device *sgm = charger_get_data(chg_dev);
// 	while(i<4){
// 		if (uV == BOOST_VOLT_LIMIT[i]){
// 			reg_val = i;
// 			break;
// 		}
// 		i++;
// 	}
// 	if (reg_val < 0)
// 		return reg_val;
// 	reg_val = reg_val << 4;
// 	ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_6,
// 				  SGM415XX_BOOSTV, reg_val);
// 	return ret;
// }
// #endif
static int sgm415xx_set_boost_current_limit(struct charger_device *chg_dev, u32 uA)
{
	int ret = 0;
	struct sgm415xx_device *sgm = charger_get_data(chg_dev);
	u8 val = 0;
	if (SGM41516E_PN_ID == sgm->device_id) {
		if (uA >= 2000000)
			val = 0;
		else if (uA >= 1500000)
			val = 1;
		else if (uA >= 1200000)
			val = 2;
		else
			val = 3;
		ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_7, SGM41516E_BOOST_LIM, val); 
	} else {
		if (uA == BOOST_CURRENT_LIMIT[sgm->device_id][0]){
		ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_2, SGM415XX_BOOST_LIM,
                     0);
		}
		else if (uA == BOOST_CURRENT_LIMIT[sgm->device_id][1]){
			ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_2, SGM415XX_BOOST_LIM,
    	                 BIT(7));
		}
		else
		{
			dev_err(sgm->dev, "%s:current isn't range\n", __func__);
		}
	}
	return ret;
}
static struct charger_ops sgm415xx_chg_ops = {
	.dump_registers = sgm415xx_dump_register,
	/* cable plug in/out */
   //.plug_in = mt6375_plug_in,
   //.plug_out = mt6375_plug_out,
   /* enable */
   .enable = sgm415xx_charging_switch,
   .is_enabled = sgm415xx_is_charging,
   /* charging current */
   .set_charging_current = sgm415xx_set_ichrg_curr,
   .get_charging_current = sgm415xx_get_ichg_curr,
   .get_min_charging_current = sgm415xx_get_minichg_curr,
   /* charging voltage */
   .set_constant_voltage = sgm415xx_set_chrg_volt,
   .get_constant_voltage = sgm415xx_get_chrg_volt,
   /* input current limit */
   .set_input_current = sgm415xx_set_input_curr_lim,
   .get_input_current = sgm415xx_get_input_curr_lim,
   .get_min_input_current = sgm415xx_get_input_mincurr_lim,
   /* MIVR */
   .set_mivr = sgm415xx_set_input_volt_lim,
   .get_mivr = sgm415xx_get_input_volt_lim,
   .get_mivr_state = sgm415xx_get_input_minvolt_lim,
   /* ADC */
   //.get_adc = mt6375_get_adc,
   //.get_vbus_adc = mt6375_get_vbus,
   //.get_ibus_adc = mt6375_get_ibus,
   //.get_ibat_adc = mt6375_get_ibat,
   //.get_tchg_adc = mt6375_get_tchg,
   //.get_zcv = mt6375_get_zcv,
   /* charing termination */
   .set_eoc_current = sgm415xx_set_term_curr,
   //.enable_termination = mt6375_enable_te,
   //.reset_eoc_state = mt6375_reset_eoc_state,
   //.safety_check = mt6375_sw_check_eoc,
   .is_charging_done = sgm415xx_get_charging_status,
   /* power path */
   //.enable_powerpath = mt6375_enable_buck,
   //.is_powerpath_enabled = mt6375_is_buck_enabled,
   /* timer */
   .enable_safety_timer = sgm415xx_enable_safetytimer,
   .is_safety_timer_enabled = sgm415xx_get_is_safetytimer_enable,
   .kick_wdt = sgm415xx_reset_watch_dog_timer,
   /* AICL */
   //.run_aicl = mt6375_run_aicc,
   /* PE+/PE+20 */
	.send_ta_current_pattern = sgm415xx_en_pe_current_partern,
   //.set_pe20_efficiency_table = mt6375_set_pe20_efficiency_table,
   //.send_ta20_current_pattern = mt6375_set_pe20_current_pattern,
   //.reset_ta = mt6375_reset_pe_ta,
   //.enable_cable_drop_comp = mt6
   /* OTG */
	.enable_otg = sgm415xx_enable_otg,
	.set_boost_current_limit = sgm415xx_set_boost_current_limit,
};
static int sgm415xx_driver_probe(struct i2c_client *client)
{
	int ret = 0;
	struct device *dev = &client->dev;
	struct sgm415xx_device *sgm;
    char *name = NULL;
	struct sgm415xx_state state;
	pr_err("[%s], %d.\n", __func__, __LINE__);
	sgm = devm_kzalloc(dev, sizeof(*sgm), GFP_KERNEL);
	if (!sgm) {
		pr_err("[%s], %d.\n", __func__, __LINE__);
		return -ENOMEM;
	}
	sgm->client = client;
	sgm->dev = dev;
	mutex_init(&sgm->lock);
	mutex_init(&sgm->i2c_rw_lock);
	i2c_set_clientdata(client, sgm);
	ret = sgm415xx_parse_dt(sgm);
	if (ret) {
		pr_err("[%s], %d.\n", __func__, __LINE__);
		return ret;
	}
	ret = sgm415xx_hw_chipid_detect(sgm);
	if (ret == false) {
		pr_err("[%s], %d.\n", __func__, __LINE__);
		return ret;
	}
	name = devm_kasprintf(sgm->dev, GFP_KERNEL, "%s","sgm415xx suspend wakelock");
	sgm->charger_wakelock =	wakeup_source_register(sgm->dev, name);
	/* Register charger device */
	sgm->chg_dev = charger_device_register("primary_chg",
						&client->dev, sgm,
						&sgm415xx_chg_ops,
						&sgm415xx_chg_props);
	if (IS_ERR_OR_NULL(sgm->chg_dev)) {
		ret = PTR_ERR(sgm->chg_dev);
		pr_err("[%s], %d.\n", __func__, __LINE__);
		return ret;
	}
	s_chg_dev=sgm->chg_dev;
	INIT_DELAYED_WORK(&sgm->charge_detect_delayed_work, charger_detect_work_func);
	INIT_DELAYED_WORK(&sgm->charge_monitor_work, charger_monitor_work_func);
	if (client->irq) {
		ret = devm_request_threaded_irq(dev, client->irq, NULL,
						sgm415xx_irq_handler_thread,
						IRQF_TRIGGER_FALLING |
						IRQF_ONESHOT,
						dev_name(&client->dev), sgm);
		if (ret)
		{
			pr_err("[%s], %d.\n", __func__, __LINE__);
			return ret;
		}
		enable_irq_wake(client->irq);
	}
	ret = sgm415xx_power_supply_init(sgm, dev);
	if (ret) {
		pr_err("Failed to register power supply\n");
		return ret;
	}
	ret = sgm415xx_hw_init(sgm);
	if (ret) {
		dev_err(dev, "Cannot initialize the chip.\n");
		return ret;
	}
	schedule_delayed_work(&sgm->charge_monitor_work,100);
	pr_err("[%s], %d.\n", __func__, __LINE__);
	sgm415xx_get_state(sgm, &state);
	sgm->state = state;
	schedule_delayed_work(&sgm->charge_detect_delayed_work, 500);
	//OTG setting
	//sgm415xx_set_otg_voltage(s_chg_dev, 5000000); //5V
	//sgm415xx_set_otg_current(s_chg_dev, 1200000); //1.2A
	//sgm415xx_vbus_regulator_register(sgm);
	pr_err("[%s] successed, line %d.\n", __func__, __LINE__);
	return 0;
}
static void sgm415xx_charger_remove(struct i2c_client *client)
{
    struct sgm415xx_device *sgm = i2c_get_clientdata(client);
    cancel_delayed_work_sync(&sgm->charge_monitor_work);
    //regulator_unregister(sgm->otg_rdev);
    power_supply_unregister(sgm->charger);
	mutex_destroy(&sgm->lock);
    mutex_destroy(&sgm->i2c_rw_lock);
}
static void sgm415xx_charger_shutdown(struct i2c_client *client)
{
    int ret = 0;
	struct sgm415xx_device *sgm = i2c_get_clientdata(client);
    ret = sgm415xx_disable_charger(sgm);
    if (ret) {
        pr_err("Failed to disable charger, ret = %d\n", ret);
    }
    pr_info("sgm415xx_charger_shutdown\n");
}
static const struct i2c_device_id sgm415xx_i2c_ids[] = {
	{ "sgm41541", 0 },
	{ "sgm41542", 1 },
	{ "sgm41543", 2 },
	{ "sgm41543D", 3 },
	{ "sgm41513", 4 },
	{ "sgm41513A", 5 },
	{ "sgm41513D", 6 },
	{ "sgm41516", 7 },
	{ "sgm41516D", 8 },
	{ "sgm41516E", 9 },
	{},
};
MODULE_DEVICE_TABLE(i2c, sgm415xx_i2c_ids);
static const struct of_device_id sgm415xx_of_match[] = {
	{ .compatible = "sgm,sgm41541", },
	{ .compatible = "sgm,sgm41542", },
	{ .compatible = "sgm,sgm41543", },
	{ .compatible = "sgm,sgm41543D", },
	{ .compatible = "sgm,sgm41513", },
	{ .compatible = "sgm,sgm41513A", },
	{ .compatible = "sgm,sgm41513D", },
	{ .compatible = "sgm,sgm41516", },
	{ .compatible = "sgm,sgm41516D", },
	{ .compatible = "sgm,sgm41516E", },
	{ },
};
MODULE_DEVICE_TABLE(of, sgm415xx_of_match);
static struct i2c_driver sgm415xx_driver = {
	.driver = {
		.name = "sgm415xx-charger",
		.of_match_table = sgm415xx_of_match,
	},
	.probe = sgm415xx_driver_probe,
	.remove = sgm415xx_charger_remove,
	.shutdown = sgm415xx_charger_shutdown,
	.id_table = sgm415xx_i2c_ids,
};
module_i2c_driver(sgm415xx_driver);
MODULE_AUTHOR(" qhq <Allen_qin@sg-micro.com>");
MODULE_DESCRIPTION("sgm415xx charger driver");
MODULE_LICENSE("GPL v2");
