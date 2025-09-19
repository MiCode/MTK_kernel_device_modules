#define pr_fmt(fmt)     "[bq25985] %s: " fmt, __func__

#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio/consumer.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include "bq25985_charger.h"
#include "mtk_charger.h"
#include "charger_class.h"

#define TEST 1
struct bq25985_state {
	bool dischg;
	bool ovp;
	bool ocp;
	bool wdt;
	bool tflt;
	bool online;
	bool ce;
	bool hiz;
	bool bypass;
	bool sc21;
	bool sc41;

	u32 vbat_adc;
	u32 vsys_adc;
	u32 ibat_adc;
	u32 vac2_adc;
	u32 ibus_adc;
	u32 vac1_adc;
	u32 tsbat_adc;
	u32 tdie_dac;
};

enum bq25985_id {
	BQ25985,
};

struct bq25985_chip_info {

	int model_id;
	const struct regmap_config *regmap_config;

	/*int busocp_def;
	int busocp_sc_max;
	int busocp_byp_max;
	int busocp_sc_min;
	int busocp_byp_min;

	int busovp_sc_def;
	int busovp_byp_def;
	int busovp_sc_step;

	int busovp_sc_offset;
	int busovp_byp_step;
	int busovp_byp_offset;
	int busovp_sc_min;
	int busovp_sc_max;
	int busovp_byp_min;
	int busovp_byp_max;

	int batovp_def;
	int batovp_max;
	int batovp_min;
	int batovp_step;
	int batovp_offset;

	int batocp_def;
	int batocp_max;
	*/
	int	busocp_sc41_max;
	int	busocp_sc41_min;
	int	busocp_sc41_step;
	int	busocp_sc41_offset;
	int busocp_sc41_def;

	int busovp_sc41_max;
	int	busovp_sc41_min;
	int busovp_sc41_step;
	int busovp_sc41_offset;
	int busovp_sc41_def;

	int busocp_sc21_max;
	int	busocp_sc21_min;
	int	busocp_sc21_step;
	int	busocp_sc21_offset;
	int busocp_sc21_def;

	int busovp_sc21_max;
	int busovp_sc21_min;
	int busovp_sc21_step;
	int busovp_sc21_offset;
	int busovp_sc21_def;

	int	busocp_bypass_max;
	int	busocp_bypass_min;
	int	busocp_bypass_step;
	int	busocp_bypass_offset;
	int busocp_bypass_def;

	int busovp_bypass_max;
	int busovp_bypass_min;
	int busovp_bypass_step;
	int busovp_bypass_offset;
	int busovp_bypass_def;

	int	batocp_max;
	int	batocp_min;
	int	batocp_step;
	int	batocp_offset;
	int batocp_def;
	int	batovp_max;
	int	batovp_min;
	int	batovp_step;
	int	batovp_offset;
	int batovp_def;
	
};

struct bq25985_init_data {
	u32 ichg;
	u32 bypass_ilim;
	u32 sc41_ilim;
	u32 sc21_ilim;
	u32 vreg;
	u32 iterm;
	u32 iprechg;
	u32 bypass_vlim;
	u32 sc41_vlim;
	u32 sc21_vlim;
	u32 ichg_max;
	u32 vreg_max;
};

struct bq25985_device {
	struct i2c_client *client;
	struct device *dev;
	struct power_supply *charger;
	struct power_supply *battery;
	struct mutex lock;
	struct regmap *regmap;

	char model_name[I2C_NAME_SIZE];

	struct bq25985_init_data init_data;
	const struct bq25985_chip_info *chip_info;
	struct bq25985_state state;
	int watchdog_timer;
	bool chg_en;
	struct charger_device *chg_dev;
	struct charger_properties chg_props;
	int chip_ok;
};

struct mtk_cp_sysfs_field_info {
	struct device_attribute attr;
	enum cp_property prop;
	int (*set)(struct bq25985_device *bq,
		struct mtk_cp_sysfs_field_info *attr, int val);
	int (*get)(struct bq25985_device *bq,
		struct mtk_cp_sysfs_field_info *attr, int *val);
};

//寄存器默认值
#if 0
static struct reg_default bq25985_reg_defs[] = {
	{BQ25985_BATOVP, 0x08},//
	{BQ25985_BATOVP_ALM, 0x0},//
	{BQ25985_BATOCP, 0x19},//
	{BQ25985_BATOCP_ALM, 0x18},//
	{BQ25985_BATUCP_ALM, 0x28},//
	{BQ25985_CHRGR_CTRL_1, 0x0c},//
	{BQ25985_BUSOVP, 0x8},//
	{BQ25985_BUSOVP_ALM, 0x22},//
	{BQ25985_BUSOCP, 0x77},//
	{BQ25985_BUSOCP_ALM, 0x46},//
	{BQ25985_TEMP_CONTROL, 0x60},//
	{BQ25985_TDIE_ALM, 0xC8},//
	{BQ25985_TSBUS_FLT, 0x15},//
	//---------------------------//
	{BQ25985_VAC_CONTROL, 0x08},//
	{BQ25985_CHRGR_CTRL_2, 0x21},//
	{BQ25985_CHRGR_CTRL_3, 0x1},//
	{BQ25985_CHRGR_CTRL_4, 0x70},//
	{BQ25985_CHRGR_CTRL_5, 0x6},//
	{BQ25985_CHRGR_CTRL_6, 0x20},//
	{BQ25985_STAT1, 0x0},//
	{BQ25985_STAT2, 0x0},//
	{BQ25985_STAT3, 0x0},//
	{BQ25985_STAT4, 0x0},//
	{BQ25985_STAT5, 0x0},//
	{BQ25985_FLAG1, 0x0},//
	{BQ25985_FLAG2, 0x0},//
	{BQ25985_FLAG3, 0x0},//
	{BQ25985_FLAG4, 0x0},//
	{BQ25985_FLAG5, 0x0},//
	{BQ25985_MASK1, 0x0},//
	{BQ25985_MASK2, 0x0},//
	{BQ25985_MASK3, 0x0},//
	{BQ25985_MASK4, 0x0},//
	{BQ25985_MASK5, 0x0},//
	{BQ25985_DEVICE_INFO_COPY, 0x0},//

	{BQ25985_ADC_CONTROL1, 0x0},//
	{BQ25985_ADC_CONTROL2, 0x0},//
	{BQ25985_ADC_CONTROL3, 0x0},//

	{BQ25985_IBUS_ADC_LSB, 0x0},//
	{BQ25985_IBUS_ADC_MSB, 0x0},//

	{BQ25985_VBUS_ADC_LSB, 0x0},//
	{BQ25985_VBUS_ADC_MSB, 0x0},//

	{BQ25985_VAC1_ADC_LSB, 0x0},//
	{BQ25985_VAC1_ADC_MSB, 0x0},//

	{BQ25985_VAC2_ADC_LSB, 0x0},//
	{BQ25985_VAC2_ADC_MSB, 0x0},//

	{BQ25985_VOUT_ADC_LSB, 0x0},//
	{BQ25985_VOUT_ADC_MSB, 0x0},//

	{BQ25985_VBAT1_ADC_LSB, 0x0},//
	{BQ25985_VBAT1_ADC_MSB, 0x0},//
	{BQ25985_VBAT2_ADC_LSB, 0x0},//
	{BQ25985_VBAT2_ADC_MSB, 0x0},//
	{BQ25985_IBAT_ADC_LSB, 0x0},//
	{BQ25985_IBAT_ADC_MSB, 0x0},//

	/*{BQ25985_TSBUS_ADC_LSB, 0x0},
	{BQ25985_TSBUS_ADC_MSB, 0x0},*/

	{BQ25985_TSBAT_ADC_LSB, 0x0},//
	{BQ25985_TSBAT_ADC_MSB, 0x0},//

	{BQ25985_TDIE_ADC_LSB, 0x0},//
	{BQ25985_TDIE_ADC_MSB, 0x0},//

	{BQ25985_DP_ADC_LSB, 0x0},//
	{BQ25985_DP_ADC_MSB, 0x0},//

	{BQ25985_VBUS_ERRLO_ERRHI, 0x10},//
	{BQ25985_HVDCP1, 0x20},//
	{BQ25985_HVDCP2, 0x0},//
	{BQ25985_CHICKEN1, 0x0},//
	{BQ25985_CHICKEN2, 0x0},//
	{BQ25985_CHICKEN3, 0x0},//
	{BQ25985_CHICKEN4, 0x0},//
	{BQ25985_DEVICE_INFO, 0x0},//


//	{BQ25985_DEGLITCH_TIME, 0x0},

//	{BQ25985_CHRGR_CTRL_6, 0x0},
};
#endif

/* add the regs you want to dump here */
static unsigned int bq25985_reg_list[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
	0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x6E,
};

static int bq25985_dump_register(struct bq25985_device *bq)
{
	unsigned int data[100] = {0,};
	int i = 0, reg_num = ARRAY_SIZE(bq25985_reg_list);
	int len = 0, idx = 0, idx_total = 0, len_sysfs = 0;
	char buf_tmp[256] = {0,};

	for (i = 0; i < reg_num; i++) {
		regmap_read(bq->regmap, bq25985_reg_list[i], &data[i]);
		len = scnprintf(buf_tmp + strlen(buf_tmp), PAGE_SIZE - idx,
				"[0x%02X]=0x%02X,", bq25985_reg_list[i], data[i]);
		idx += len;

		if (((i + 1) % 8 == 0) || ((i + 1) == reg_num)) {
			pr_err("[bq25985] %s\n", buf_tmp);

			memset(buf_tmp, 0x0, sizeof(buf_tmp));

			idx_total += len_sysfs;
			idx = 0;
		}
	}

	return 0;
}



static int bq25985_watchdog_time[BQ25985_NUM_WD_VAL] = {500, 1000, 5000,	//
							30000};

static int bq25985_get_input_curr_lim(struct bq25985_device *bq)//----------
{
	unsigned int busocp_reg_code;
	int ret;
	int flag;

	ret = regmap_read(bq->regmap, BQ25985_BUSOCP, &busocp_reg_code);
	if (ret)
		return ret;
	ret = regmap_read(bq->regmap, BQ25985_CHRGR_CTRL_5, &flag);//
	if (ret)
		return ret;//寄存器读取错误	
	if((READBIT(flag,6))&&((READBIT(flag,7))==0))//BYPASS 模式判断
		return (busocp_reg_code * BQ25985_BUSOCP_BYP_STEP_uA) + BQ25985_BUSOCP_BYP_OFFSET_uA;
	if(!(READBIT(flag,6)))//SC41/SC21 模式判断
		{
			if(READBIT(flag,7))
				return (busocp_reg_code * BQ25985_BUSOCP_SC21_STEP_uA) + BQ25985_BUSOCP_SC21_OFFSET_uA;
			if(!READBIT(flag,7))
				return (busocp_reg_code * BQ25985_BUSOCP_SC41_STEP_uA) + BQ25985_BUSOCP_SC41_OFFSET_uA;
		}
	return -1;
}

static int bq25985_set_hiz(struct bq25985_device *bq, int setting)//
{
	return regmap_update_bits(bq->regmap, BQ25985_CHRGR_CTRL_2,
			BQ25985_EN_HIZ, setting);
}

static int bq25985_set_input_curr_lim(struct bq25985_device *bq, int busocp)//
{
	unsigned int busocp_reg_code;
	int ret;
	int alarm;
	if (!busocp)
		return bq25985_set_hiz(bq, BQ25985_ENABLE_HIZ);

	bq25985_set_hiz(bq, BQ25985_DISABLE_HIZ);

	if (busocp < BQ25985_BUSOCP_SC41_MIN_uA)
		busocp = BQ25985_BUSOCP_SC21_MIN_uA;

	if (bq->state.bypass)
	{
		busocp = min(busocp, bq->chip_info->busocp_bypass_max);
		busocp_reg_code = (busocp - BQ25985_BUSOCP_BYP_OFFSET_uA)
						/ BQ25985_BUSOCP_BYP_STEP_uA;
		alarm=(busocp-1000000)/250000;
	}
	else if(bq->state.sc41)
	{
		busocp = min(busocp, bq->chip_info->busocp_sc41_max);
		busocp_reg_code = (busocp - BQ25985_BUSOCP_SC41_OFFSET_uA)
						/ BQ25985_BUSOCP_SC41_STEP_uA;
		busocp_reg_code = busocp_reg_code<<4;
		alarm=(busocp-1000000)/250000;
	}
	else
	{
		busocp = min(busocp, bq->chip_info->busocp_sc21_max);
		busocp_reg_code = (busocp - BQ25985_BUSOCP_SC21_OFFSET_uA)
						/ BQ25985_BUSOCP_SC21_STEP_uA;
		alarm=(busocp-1000000)/250000;
	}


	ret = regmap_write(bq->regmap, BQ25985_BUSOCP, busocp_reg_code);
	if (ret)
	{	
		pr_err("0x8 write error \n");
		return ret;
	}
	return regmap_write(bq->regmap, BQ25985_BUSOCP_ALM, alarm);
}

static int bq25985_get_input_volt_lim(struct bq25985_device *bq)
{
	unsigned int busovp_reg_code;
	int ret;
	ret = regmap_read(bq->regmap, BQ25985_BUSOVP, &busovp_reg_code);
	if (ret)
		return ret;
	if (bq->state.bypass) {
		if(READBIT(busovp_reg_code,4))
		{
			return 6*1000*1000;
		}
		else
		{
			return 5.6*1000*1000;
		}
	} else if(bq->state.sc41) {
		if(READBIT(busovp_reg_code,6))
		{
			return 22*1000*1000;
		}
		else
		{
			return 21*1000*1000;
		}
	}else
	{
		if(READBIT(busovp_reg_code,5))
		{
			return 12*1000*1000;
		}
		else
		{
			return 11*1000*1000;
		}
	}



	return -1;
}

static int bq25985_set_input_volt_lim(struct bq25985_device *bq, int busovp)//-----
{
	unsigned int busovp_reg_code;
	int ret;

	if (bq->state.bypass) {
		if(busovp>=6*1000*1000)
		{
			ret=regmap_update_bits(bq->regmap,BQ25985_BUSOVP,BIT(4),1);
			if(ret)
				return ret;
			busovp_reg_code = (6*1000*1000 - BQ25985_BUSOVP_BYP_OFFSET_uV) / 
			BQ25985_BUSOVP_BYP_STEP_uV;
		}
		else
		{
			ret=regmap_update_bits(bq->regmap,BQ25985_BUSOVP,BIT(4),0);
			if(ret)
				return ret;
			busovp_reg_code = (5.6*1000*1000 - BQ25985_BUSOVP_BYP_OFFSET_uV) /
			BQ25985_BUSOVP_BYP_STEP_uV;
		}
	} else if(bq->state.sc41)
	{
		if(busovp>=22*1000*1000)
		{
			ret=regmap_update_bits(bq->regmap,BQ25985_BUSOVP,BIT(6),1);
			if(ret)
				return ret;
			busovp_reg_code = (22*1000*1000 - BQ25985_BUSOVP_SC41_OFFSET_uV) / 
			BQ25985_BUSOVP_SC41_STEP_uV;
		}
		else
		{
			ret=regmap_update_bits(bq->regmap,BQ25985_BUSOVP,BIT(6),0);
			if(ret)
				return ret;
			busovp_reg_code = (21*1000*1000 - BQ25985_BUSOVP_SC41_OFFSET_uV) / 
			BQ25985_BUSOVP_SC41_STEP_uV;
		}
	}
	else
	{
		if(busovp>=12*1000*1000)
		{
			ret=regmap_update_bits(bq->regmap,BQ25985_BUSOVP,BIT(5),1);
			if(ret)
				return ret;
			busovp_reg_code = (12*1000*1000 - BQ25985_BUSOVP_SC21_OFFSET_uV) / 
			BQ25985_BUSOVP_SC21_STEP_uV;
		}
		else
		{
			ret=regmap_update_bits(bq->regmap,BQ25985_BUSOVP,BIT(5),0);
			if(ret)
				return ret;
			busovp_reg_code = (11*1000*1000 - BQ25985_BUSOVP_SC21_OFFSET_uV) / 
			BQ25985_BUSOVP_SC21_STEP_uV;
		}
	} 
	return regmap_write(bq->regmap, BQ25985_BUSOVP_ALM, busovp_reg_code);
}

static int bq25985_get_const_charge_curr(struct bq25985_device *bq)//--------
{
	unsigned int batocp_reg_code;
	int ret;

	ret = regmap_read(bq->regmap, BQ25985_BATOCP, &batocp_reg_code);
	if (ret)
		return ret;

	return (batocp_reg_code & BQ25985_BATOCP_MASK) *	//
						BQ25985_BATOCP_STEP_uA+BQ25985_BATOCP_OFFSET_uA;
}

static int bq25985_set_const_charge_curr(struct bq25985_device *bq, int batocp)//-----------
{
	unsigned int batocp_reg_code;
	int ret;

	batocp = max(batocp, BQ25985_BATOCP_MIN_uA);
	batocp = min(batocp, bq->chip_info->batocp_max);

	batocp_reg_code = ((batocp-BQ25985_BATOCP_OFFSET_uA) / BQ25985_BATOCP_STEP_uA);

	ret = regmap_update_bits(bq->regmap, BQ25985_BATOCP,
				BQ25985_BATOCP_MASK, batocp_reg_code);
	if (ret)
		return ret;

	return regmap_update_bits(bq->regmap, BQ25985_BATOCP_ALM,
				BQ25985_BATOCP_MASK, batocp_reg_code);
}

static int bq25985_get_const_charge_volt(struct bq25985_device *bq)//----------
{
	unsigned int batovp_reg_code;
	int ret;

	ret = regmap_read(bq->regmap, BQ25985_BATOVP, &batovp_reg_code);
	if (ret)
		return ret;
	
	return ((USEBITS(batovp_reg_code,5,0) * bq->chip_info->batovp_step) +
			bq->chip_info->batovp_offset);
}

static int bq25985_set_const_charge_volt(struct bq25985_device *bq, int batovp)//------
{
	unsigned int batovp_reg_code;
	int ret;

	if (batovp < bq->chip_info->batovp_min)
		batovp = bq->chip_info->batovp_min;

	if (batovp > bq->chip_info->batovp_max)
		batovp = bq->chip_info->batovp_max;

	batovp_reg_code = (batovp - bq->chip_info->batovp_offset) /
						bq->chip_info->batovp_step;

	ret = regmap_write(bq->regmap, BQ25985_BATOVP, batovp_reg_code);
	if (ret)
		return ret;
	batovp_reg_code=((batovp-4200*1000)/(25*1000));//此处BATOVP_ALM与BATOVP精度单位不同
	return regmap_write(bq->regmap, BQ25985_BATOVP_ALM, batovp_reg_code);
}

static int bq25985_set_bypass(struct bq25985_device *bq)
{
	int ret;

	ret = regmap_update_bits(bq->regmap, BQ25985_CHRGR_CTRL_5,
			BQ25985_REVERSE_EN, 0);
	ret = regmap_update_bits(bq->regmap, BQ25985_CHRGR_CTRL_5,
			BQ25985_EXT_REVERSE_EN, 0);
	ret = regmap_update_bits(bq->regmap, BQ25985_CHRGR_CTRL_5,
			BQ25985_EN_BYPASS, 0x1 << 6);
	if (ret)
		pr_err("%s:failed to set cp charge mode\n", __func__);

	bq->state.sc41 = false;
	bq->state.sc21 = false;
	bq->state.bypass = true;

	return bq->state.bypass;
}

static int bq25985_set_sc41(struct bq25985_device *bq)
{
	int ret;

	ret = regmap_update_bits(bq->regmap, BQ25985_CHRGR_CTRL_5,
			BQ25985_REVERSE_EN, 0);
	ret = regmap_update_bits(bq->regmap, BQ25985_CHRGR_CTRL_5,
			BQ25985_EXT_REVERSE_EN, 0);
	ret = regmap_update_bits(bq->regmap, BQ25985_CHRGR_CTRL_5,
			BQ25985_EN_SC41, 0);
	ret = regmap_update_bits(bq->regmap, BQ25985_CHRGR_CTRL_5,
					BQ25985_EN_BYPASS, 0);
	if(ret)
		pr_err("%s:failed to set cp charge mode\n", __func__);

	bq->state.sc41 = true;
	bq->state.sc21 = false;
	bq->state.bypass= false;

	return bq->state.sc41;
}

static int bq25985_set_sc21(struct bq25985_device *bq)
{
	int ret;

	ret = regmap_update_bits(bq->regmap, BQ25985_CHRGR_CTRL_5,
			BQ25985_REVERSE_EN, 0);
	ret = regmap_update_bits(bq->regmap, BQ25985_CHRGR_CTRL_5,
			BQ25985_EXT_REVERSE_EN, 0);
	ret = regmap_update_bits(bq->regmap, BQ25985_CHRGR_CTRL_5,
			BQ25985_EN_SC41, 0x1 << 7);
	ret = regmap_update_bits(bq->regmap, BQ25985_CHRGR_CTRL_5,
					BQ25985_EN_BYPASS, 0);
	if(ret)
		pr_err("%s:failed to set cp charge mode\n", __func__);

	bq->state.sc41 = false;
	bq->state.sc21 = true;
	bq->state.bypass= false;

	return bq->state.sc21;
}

static int bq25985_set_revert_mode(struct bq25985_device *bq)
{
	int ret;

	ret = regmap_update_bits(bq->regmap, BQ25985_CHRGR_CTRL_5,
			BQ25985_REVERSE_EN, 0x1 << 4);
	ret = regmap_update_bits(bq->regmap, BQ25985_CHRGR_CTRL_5,
			BQ25985_EXT_REVERSE_EN, 0x1 << 5);
	if (ret)
		pr_err("%s:failed to set cp charge mode\n", __func__);

	bq->state.sc41 = false;
	bq->state.sc21 = false;
	bq->state.bypass = true;

	return 0;
}

#if 0
static int bq25985_enable_acdrv_manual(struct bq25985_device *bq, bool en_acdrv)
{
	int ret;

	if (en_acdrv)
		ret = regmap_update_bits(bq->regmap, BQ25985_CHRGR_CTRL_5,
					BQ25985_ACDRV_EN, 0x1 << 2);
	else
		ret = regmap_update_bits(bq->regmap, BQ25985_CHRGR_CTRL_5,
					BQ25985_ACDRV_EN, 0);
	if (ret)
		pr_err("%s:failed to set acdrv manual \n", __func__);

	pr_err("enable=%d \n", en_acdrv);

	return 0;
}
#endif

static int bq25985_set_chg_en(struct bq25985_device *bq, bool en_chg)
{
	int ret;

	if (en_chg)
		ret = regmap_update_bits(bq->regmap, BQ25985_CHRGR_CTRL_5,
					BQ25985_CHG_EN, 0x1 << 3);
	else
		ret = regmap_update_bits(bq->regmap, BQ25985_CHRGR_CTRL_5,
					BQ25985_CHG_EN, 0);
	if (ret)
		pr_err("%s:failed to set chg en\n", __func__);

	bq->state.ce = en_chg;
	pr_err("enable=%d \n", en_chg);

	return 0;
}

static int bq25985_get_chg_en(struct bq25985_device *bq, bool *en_chg)
{
	int ret;
	unsigned int reg;
	ret=regmap_read(bq->regmap, BQ25985_CHRGR_CTRL_5,
			&reg);
	if(ret)
		pr_err("%s:failed to set chg en\n", __func__);

	reg&=BQ25985_CHG_EN;
	*en_chg = !!reg;
	pr_err("enable=%d \n", *en_chg);

	return 0;
}

static int bq25985_get_adc_ibus(struct bq25985_device *bq)
{
	int ibus_adc_lsb, ibus_adc_msb;
	u16 ibus_adc;
	int ret;

	ret = regmap_read(bq->regmap, BQ25985_IBUS_ADC_LSB, &ibus_adc_lsb);
	ret = regmap_read(bq->regmap, BQ25985_IBUS_ADC_MSB, &ibus_adc_msb);
	if (ret)
		pr_err("%s:failed to get get ibus\n", __func__);

	ibus_adc = (ibus_adc_msb << 8) | ibus_adc_lsb;
	pr_err("ibus=%d,msb[0x%02x]=0x%02x,lsb[0x%02x]=0x%02x\n", ibus_adc, BQ25985_IBUS_ADC_MSB, ibus_adc_msb, BQ25985_IBUS_ADC_LSB, ibus_adc_lsb);

	if (ibus_adc_msb & BQ25985_ADC_POLARITY_BIT)
		return (ibus_adc ^ 0xffff) + 1;

	return ibus_adc;
}

static int bq25985_get_adc_vbus(struct bq25985_device *bq)
{
	int vbus_adc_lsb, vbus_adc_msb;
	u16 vbus_adc;
	int ret;

	ret = regmap_read(bq->regmap, BQ25985_VBUS_ADC_LSB, &vbus_adc_lsb);
	ret = regmap_read(bq->regmap, BQ25985_VBUS_ADC_MSB, &vbus_adc_msb);
	if (ret)
		pr_err("%s:failed to get get vbus\n", __func__);

	vbus_adc = (vbus_adc_msb << 8) | vbus_adc_lsb;

	pr_err("vbus_adc=%d,msb=%d,lsb=%d\n", vbus_adc, vbus_adc_msb, vbus_adc_lsb);

	return vbus_adc;
}

static int bq25985_get_adc_vac1(struct bq25985_device *bq)
{
	int vbus_adc_lsb, vbus_adc_msb;
	u16 vbus_adc;
	int ret;

	ret = regmap_read(bq->regmap, BQ25985_VAC1_ADC_LSB, &vbus_adc_lsb);
	ret = regmap_read(bq->regmap, BQ25985_VAC1_ADC_MSB, &vbus_adc_msb);
	if (ret)
		pr_err("%s:failed to get get vac1\n", __func__);

	vbus_adc = (vbus_adc_msb << 8) | vbus_adc_lsb;

	return vbus_adc;
}

static int bq25985_get_adc_vac2(struct bq25985_device *bq)
{
	int vbus_adc_lsb, vbus_adc_msb;
	u16 vbus_adc;
	int ret;

	ret = regmap_read(bq->regmap, BQ25985_VAC2_ADC_LSB, &vbus_adc_lsb);
	ret = regmap_read(bq->regmap, BQ25985_VAC2_ADC_MSB, &vbus_adc_msb);
	if (ret)
		pr_err("%s:failed to get get vac2\n", __func__);

	vbus_adc = (vbus_adc_msb << 8) | vbus_adc_lsb;

	return vbus_adc;
}

static int bq25985_get_ibat_adc(struct bq25985_device *bq)//------------
{
	int ret;
	int ibat_adc_lsb=0xffff, ibat_adc_msb=0xffff;
	int ibat_adc;
	#if TEST
	int test=0xffff;

	ret = regmap_read(bq->regmap, 0x0, &test);
	pr_err("0x0 =%d ret=%d\n",test,ret);
	ret = regmap_read(bq->regmap, 0x1, &test);
	pr_err("0x1 =%d ret=%d\n",test,ret);
	ret = regmap_read(bq->regmap, 0x2, &test);
	pr_err("0x2 =%d ret=%d\n",test,ret);
		ret = regmap_read(bq->regmap, 0x3, &test);
	pr_err("0x3 =%d ret=%d\n",test,ret);
		ret = regmap_read(bq->regmap, 0x4, &test);
	pr_err("0x4 =%d ret=%d\n",test,ret);
		ret = regmap_read(bq->regmap, 0x5, &test);
	pr_err("0x5 =%d ret=%d\n",test,ret);
	ret = regmap_read(bq->regmap, 0x30, &test);
	pr_err("0x30 =%d ret=%d\n",test,ret);
	ret = regmap_read(bq->regmap, 0x31, &test);
	pr_err("0x31 =%d ret=%d\n",test,ret);
	#endif
	pr_err("ibat_adc_lsb=%d\nibat_adc_msb=%d\n",ibat_adc_lsb,ibat_adc_msb);
	ret = regmap_read(bq->regmap, BQ25985_IBAT_ADC_MSB, &ibat_adc_msb);
	if (ret)
	{
		pr_err("ibat_adc_msb read error :%d ret=%d\n",ibat_adc_msb,ret);
	}
	ret = regmap_read(bq->regmap, BQ25985_IBAT_ADC_LSB, &ibat_adc_lsb);
	if(ret)
	{
		pr_err("ibat_adc_lsb read error :%d ret=%d\n",ibat_adc_lsb,ret);
	}

	ibat_adc = (ibat_adc_msb << 8) | ibat_adc_lsb;

	if (ibat_adc_msb & BQ25985_ADC_POLARITY_BIT)
		return ((ibat_adc ^ 0xffff) + 1) * BQ25985_ADC_CURR_STEP_uA;

	return ibat_adc;
}

static int bq25985_get_adc_vbat(struct bq25985_device *bq)//--------------
{
	int vsys_adc_lsb, vsys_adc_msb;
	u16 vsys_adc;
	int ret;

	ret = regmap_read(bq->regmap, BQ25985_VBAT1_ADC_LSB, &vsys_adc_lsb);
	ret = regmap_read(bq->regmap,BQ25985_VBAT1_ADC_MSB, &vsys_adc_msb);
	if (ret)
		pr_err("%s:failed to get get vac2\n", __func__);

	vsys_adc = (vsys_adc_msb << 8) | vsys_adc_lsb;
	//pr_err("vsys_adc_lsb read error = %d\n",vsys_adc);
	return vsys_adc;
}


static int bq25985_get_state(struct bq25985_device *bq,	///缺少反向充电和电流极性判断
				struct bq25985_state *state)
{
	unsigned int chg_ctrl_5;
	unsigned int chg_ctrl_2;
	unsigned int stat1;
	unsigned int stat2;
	unsigned int stat3;
	unsigned int stat4;
	unsigned int stat5;
	unsigned int ibat_adc_msb;
	int ret;

	ret = regmap_read(bq->regmap, BQ25985_STAT1, &stat1);
	if (ret)
		return ret;

	ret = regmap_read(bq->regmap, BQ25985_STAT2, &stat2);
	if (ret)
		return ret;

	ret = regmap_read(bq->regmap, BQ25985_STAT3, &stat3);
	if (ret)
		return ret;

	ret = regmap_read(bq->regmap, BQ25985_STAT4, &stat4);
	if (ret)
		return ret;

	ret = regmap_read(bq->regmap, BQ25985_STAT5, &stat5);
	if (ret)
		return ret;

	ret = regmap_read(bq->regmap, BQ25985_CHRGR_CTRL_5, &chg_ctrl_5);
	if (ret)
		return ret;

	ret = regmap_read(bq->regmap, BQ25985_CHRGR_CTRL_2, &chg_ctrl_2);
	if (ret)
		return ret;
	ret = regmap_read(bq->regmap, BQ25985_IBAT_ADC_MSB, &ibat_adc_msb);
	if (ret)
		return ret;

	pr_err("0x%02x=0x%02x, 0x%02x=0x%02x, 0x%02x=0x%02x, 0x%02x=0x%02x, 0x%02x=0x%02x\n",
		BQ25985_STAT1, stat1, BQ25985_STAT2, stat2, BQ25985_STAT3, stat3,
		BQ25985_STAT4, stat4, BQ25985_STAT5, stat5);

	state->dischg = ibat_adc_msb & BQ25985_ADC_POLARITY_BIT;          ///////????????
	state->ovp = (stat1 & BQ25985_STAT1_OVP_MASK) |////
		(stat3 & BQ25985_STAT3_OVP_MASK);//
	state->ocp = (stat1 & BQ25985_STAT1_OCP_MASK) |//
		(stat2 & BQ25985_STAT2_OCP_MASK);//
	state->tflt = stat4 & BQ25985_STAT4_TFLT_MASK;//
	state->wdt = stat4 & BQ25985_WD_STAT;//
	state->online = stat3 & BQ25985_PRESENT_MASK;//
	state->ce = chg_ctrl_5 & BQ25985_CHG_EN;//
	state->hiz = chg_ctrl_2 & BQ25985_EN_HIZ;//
	state->bypass = chg_ctrl_5 & BQ25985_EN_BYPASS;//
	state->sc41=!(chg_ctrl_5 & BQ25985_EN_SC41);//
	state->sc21=(chg_ctrl_5 & BQ25985_EN_SC41);//

	return 0;
}


static int bq25985_get_battery_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct bq25985_device *bq = power_supply_get_drvdata(psy);////??????
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		val->intval = bq->init_data.ichg_max;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		val->intval = bq->init_data.vreg_max;
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = bq25985_get_ibat_adc(bq);//
		val->intval = ret;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = bq25985_get_adc_vbat(bq);//
		if (ret < 0)
			return ret;

		val->intval = ret;
		break;

	default:
		#if TEST
		pr_err("bq25985_get_battery_property error:%d\n",psp);
		#endif
		return -EINVAL;
	}

	return ret;
}

static int bq25985_set_charger_property(struct power_supply *psy,//
		enum power_supply_property prop,
		const union power_supply_propval *val)
{
	struct bq25985_device *bq = power_supply_get_drvdata(psy);
	int ret = -EINVAL;

	switch (prop) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = bq25985_set_input_curr_lim(bq, val->intval);//
		if (ret)
			return ret;
		break;

	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		ret = bq25985_set_input_volt_lim(bq, val->intval);//
		if (ret)
			return ret;
		break;
#if 0
	case POWER_SUPPLY_PROP_CHARGE_TYPE_TO_BYPASS://
		ret = bq25985_set_bypass(bq, val->intval);
		if (ret)
			return ret;
		break;

	case POWER_SUPPLY_PROP_CHARGE_TYPE_TO_SC41://
		ret = bq25985_set_sc41(bq, val->intval);
		if (ret)
			return ret;
		break;

	case POWER_SUPPLY_PROP_CHARGE_TYPE_TO_SC21://
		ret = bq25985_set_sc21(bq, val->intval);
		if (ret)
			return ret;
		break;
#endif
	case POWER_SUPPLY_PROP_STATUS:
		ret = bq25985_set_chg_en(bq, val->intval);//
		if (ret)
			return ret;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = bq25985_set_const_charge_curr(bq, val->intval);//
		if (ret)
			return ret;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = bq25985_set_const_charge_volt(bq, val->intval);//
		if (ret)
			return ret;
		break;

	default:
		#if TEST
		pr_err("bq25985_set_charger_property error\n");
		#endif
		return -EINVAL;
	}

	return ret;
}

#if 0
static int bq25985_get_const_charge_state_sc41(struct bq25985_device *bq)
{
	int ret;
	unsigned int reg;
	ret=regmap_read(bq->regmap, BQ25985_CHRGR_CTRL_5,
			&reg);
	if(ret)
	{
		return ret;
	}
	reg&=BQ25985_EN_SC41;
	return reg;
}
static int bq25985_get_const_charge_state_sc21(struct bq25985_device *bq)
{
	int ret;
	unsigned int reg;
	ret=regmap_read(bq->regmap, BQ25985_CHRGR_CTRL_5,
			&reg);
	if(ret)
	{
		return ret;
	}
	reg^=BQ25985_EN_SC41;
	return reg;
}
#endif

static int bq25985_get_const_charge_state_bypass(struct bq25985_device *bq)
{
	int ret;
	unsigned int reg;
	ret=regmap_read(bq->regmap, BQ25985_CHRGR_CTRL_5,
			&reg);
	if(ret)
	{
		return ret;
	}
	reg&=BQ25985_EN_BYPASS;
	return reg;
}

static int bq25985_get_charger_property(struct power_supply *psy,	//?????
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct bq25985_device *bq = power_supply_get_drvdata(psy);
	struct bq25985_state state;
	int ret = 0;

	mutex_lock(&bq->lock);
	ret = bq25985_get_state(bq, &state);//
	mutex_unlock(&bq->lock);
	if (ret)
		return ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = BQ25985_MANUFACTURER;	//
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = bq->model_name;	//
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = state.online;		//
		break;

	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		ret = bq25985_get_input_volt_lim(bq);	//
		if (ret < 0)
			return ret;
		val->intval = ret;
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = bq25985_get_input_curr_lim(bq);//
		if (ret < 0)
			return ret;

		val->intval = ret;
		break;

	case POWER_SUPPLY_PROP_HEALTH:	//
		val->intval = POWER_SUPPLY_HEALTH_GOOD;

		if (state.tflt)
			val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
		else if (state.ovp)
			val->intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		else if (state.ocp)
			val->intval = POWER_SUPPLY_HEALTH_OVERCURRENT;
		else if (state.wdt)
			val->intval =
				POWER_SUPPLY_HEALTH_WATCHDOG_TIMER_EXPIRE;
		break;

	case POWER_SUPPLY_PROP_STATUS://
		val->intval = POWER_SUPPLY_STATUS_UNKNOWN;

		if ((state.ce) && (!state.hiz))
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		else if (state.dischg)
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		else if (!state.ce)
			val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;

	case POWER_SUPPLY_PROP_CHARGE_TYPE://??????????to be correct...
		val->intval = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;

		if (!state.ce)
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		else if (state.bypass)
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		else if (!state.bypass)
			val->intval = POWER_SUPPLY_CHARGE_TYPE_STANDARD;
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = bq25985_get_adc_ibus(bq);///
		if (ret < 0)
			return ret;

		val->intval = ret;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = bq25985_get_adc_vbus(bq);//
		if (ret < 0)
			return ret;

		val->intval = ret;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = bq25985_get_const_charge_curr(bq);//
		if (ret < 0)
			return ret;

		val->intval = ret;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = bq25985_get_const_charge_volt(bq);//
		if (ret < 0)
			return ret;

		val->intval = ret;
		break;
#if 0
	case POWER_SUPPLY_PROP_CHARGE_TYPE_TO_BYPASS:
		ret =bq25985_get_const_charge_state_bypass(bq);
		if (ret<0)
			return ret;

		val->intval = ret;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE_TO_SC41:
		ret =bq25985_get_const_charge_state_sc41(bq);
		if (ret<0)
			return ret;

		val->intval = ret;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE_TO_SC21:
		ret =bq25985_get_const_charge_state_sc21(bq);
		if (ret<0)
			return ret;

		val->intval = ret;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE_TO_REVERSE_CHARGE: //反向充电占位
		ret=0;
		val->intval=ret;
		break;
#endif
	default:
		#if TEST
		pr_err("bq25985_get_charger_property error psp=%d\n",psp);
		#endif
		return -EINVAL;
	}

	return ret;
}


static bool bq25985_state_changed(struct bq25985_device *bq,	/////
				  struct bq25985_state *new_state)
{
	struct bq25985_state old_state;

	mutex_lock(&bq->lock);
	old_state = bq->state;
	mutex_unlock(&bq->lock);

	return (old_state.dischg != new_state->dischg ||
		old_state.ovp != new_state->ovp ||
		old_state.ocp != new_state->ocp ||
		old_state.online != new_state->online ||
		old_state.wdt != new_state->wdt ||
		old_state.tflt != new_state->tflt ||
		old_state.ce != new_state->ce ||
		old_state.hiz != new_state->hiz ||
		old_state.bypass != new_state->bypass||
		old_state.sc41 != new_state->sc41||		///
		old_state.sc21 != new_state->sc21		///
		);
}

static irqreturn_t bq25985_irq_handler_thread(int irq, void *private)
{
	struct bq25985_device *bq = private;
	struct bq25985_state state;
	int ret;

	ret = bq25985_get_state(bq, &state);//
	if (ret < 0)
		goto irq_out;

	if (!bq25985_state_changed(bq, &state))
		goto irq_out;

	mutex_lock(&bq->lock);
	bq->state = state;
	mutex_unlock(&bq->lock);

	power_supply_changed(bq->charger);//???????????????????

irq_out:
	return IRQ_HANDLED;
}

static enum power_supply_property bq25985_power_supply_props[] = {//
	POWER_SUPPLY_PROP_MANUFACTURER,//
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
#if 0
	POWER_SUPPLY_PROP_CHARGE_TYPE_TO_BYPASS,
	POWER_SUPPLY_PROP_CHARGE_TYPE_TO_SC41,
	POWER_SUPPLY_PROP_CHARGE_TYPE_TO_SC21,
	POWER_SUPPLY_PROP_CHARGE_TYPE_TO_REVERSE_CHARGE,//反向充电占位
#endif
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

static enum power_supply_property bq25985_battery_props[] = { //
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

static char *bq25985_charger_supplied_to[] = {
	"main-battery",
};


static int bq25985_property_is_writeable(struct power_supply *psy,//
					 enum power_supply_property prop)
{			
	switch (prop) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
#if 0
	case POWER_SUPPLY_PROP_CHARGE_TYPE_TO_BYPASS:
	case POWER_SUPPLY_PROP_CHARGE_TYPE_TO_SC41:
	case POWER_SUPPLY_PROP_CHARGE_TYPE_TO_SC21:
#endif
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
	// case POWER_SUPPLY_PROP_CHARGE_TYPE_TO_REVERSE_CHARGE://反向充电占位
		return true;
	default:
		return false;
	}
}

static const struct power_supply_desc bq25985_power_supply_desc = {
	.name = "cp_master",
	.type = POWER_SUPPLY_TYPE_MAINS,
	.properties = bq25985_power_supply_props,
	.num_properties = ARRAY_SIZE(bq25985_power_supply_props),
	.get_property = bq25985_get_charger_property,//
	.set_property = bq25985_set_charger_property,//
	.property_is_writeable = bq25985_property_is_writeable,//
};

static struct power_supply_desc bq25985_battery_desc = {
	.name			= "bq25985-battery",
	.type			= POWER_SUPPLY_TYPE_BATTERY,
	.get_property		= bq25985_get_battery_property,//
	.properties		= bq25985_battery_props,
	.num_properties		= ARRAY_SIZE(bq25985_battery_props),
	.property_is_writeable	= bq25985_property_is_writeable,//
};

static bool bq25985_is_volatile_reg(struct device *dev, unsigned int reg)//???
{
	switch (reg) {
	case BQ25985_VAC_CONTROL://X
	case BQ25985_CHRGR_CTRL_2:
	case BQ25985_STAT1:
	case BQ25985_STAT2:
	case BQ25985_STAT3:
	case BQ25985_STAT4:
	case BQ25985_STAT5:
	case BQ25985_FLAG1:
	case BQ25985_FLAG2:
	case BQ25985_FLAG3:
	case BQ25985_FLAG4:
	case BQ25985_FLAG5:
	case BQ25985_MASK1:
	case BQ25985_MASK2:
	case BQ25985_MASK3:
	case BQ25985_MASK4:
	case BQ25985_MASK5:
	case BQ25985_ADC_CONTROL1:
	case BQ25985_ADC_CONTROL2:
	//case BQ25985_ADC_CONTROL3:
	case BQ25985_VBUS_ADC_LSB:
	case BQ25985_VBUS_ADC_MSB:
	case BQ25985_VAC1_ADC_MSB:
	case BQ25985_VAC1_ADC_LSB:
	case BQ25985_VAC2_ADC_MSB:
	case BQ25985_VAC2_ADC_LSB:
	case BQ25985_VOUT_ADC_MSB:
	case BQ25985_VOUT_ADC_LSB:
	case BQ25985_VBAT1_ADC_MSB:
	case BQ25985_VBAT1_ADC_LSB:
	case BQ25985_VBAT2_ADC_MSB:
	case BQ25985_VBAT2_ADC_LSB:
	case BQ25985_TDIE_ADC_MSB:
	case BQ25985_TDIE_ADC_LSB:
	case BQ25985_DP_ADC_MSB:
	case BQ25985_DP_ADC_LSB:
	case BQ25985_DM_ADC_MSB:
	case BQ25985_DM_ADC_LSB:
	case BQ25985_VBUS_ERRLO_ERRHI:
	case BQ25985_HVDCP1:
	case BQ25985_HVDCP2:
	//chicken未添加
		return true;
	default:
		return false;
	}
}

static const struct regmap_config bq25985_regmap_config = {///////////
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = BQ25985_DEVICE_INFO, ///
	//.reg_defaults	= bq25985_reg_defs,
	//.num_reg_defaults = ARRAY_SIZE(bq25985_reg_defs),
	.cache_type = REGCACHE_NONE,//REGCACHE_RBTREE,
	.volatile_reg = bq25985_is_volatile_reg, //??
};

static const struct bq25985_chip_info bq25985_chip_info_tbl[] = {
	[BQ25985]={
		.model_id = BQ25985,
		.regmap_config = &bq25985_regmap_config,
		.busocp_sc41_min = BQ25985_BUSOCP_SC41_MIN_uA,
		.busocp_sc41_max = BQ25985_BUSOCP_SC41_MAX_uA,
		.busocp_sc41_step = BQ25985_BUSOCP_SC41_STEP_uA,
		.busocp_sc41_offset = BQ25985_BUSOCP_SC41_OFFSET_uA,
		.busocp_sc41_def = BQ25985_BUSOCP_SC41_DFLT_uA,
		.busocp_sc21_min = BQ25985_BUSOCP_SC21_MIN_uA,
		.busocp_sc21_max = BQ25985_BUSOCP_SC21_MAX_uA,
		.busocp_sc21_step = BQ25985_BUSOCP_SC21_STEP_uA,
		.busocp_sc21_offset = BQ25985_BUSOCP_SC21_OFFSET_uA,
		.busocp_sc21_def = BQ25985_BUSOCP_SC21_DFLT_uA,
		.busocp_bypass_max = BQ25985_BUSOCP_BYP_MAX_uA,
		.busocp_bypass_min = BQ25985_BUSOCP_BYP_MIN_uA,
		.busocp_bypass_step = BQ25985_BUSOCP_BYP_STEP_uA,
		.busocp_bypass_offset = BQ25985_BUSOCP_BYP_OFFSET_uA,
		.busocp_bypass_def = BQ25985_BUSOCP_BYP_DFLT_uA,	
		
		.busovp_sc41_min = BQ25985_BUSOVP_SC41_MIN_uV,
		.busovp_sc41_max = BQ25985_BUSOVP_SC41_MAX_uV,
		.busovp_sc41_step = BQ25985_BUSOVP_SC41_STEP_uV,
		.busovp_sc41_offset = BQ25985_BUSOVP_SC41_OFFSET_uV,
		.busovp_sc41_def = BQ25985_BUSOVP_SC41_DFLT_uV,
		.busovp_sc21_min = BQ25985_BUSOVP_SC21_MIN_uV,
		.busovp_sc21_max = BQ25985_BUSOVP_SC21_MAX_uV,
		.busovp_sc21_step = BQ25985_BUSOVP_SC21_STEP_uV,
		.busovp_sc21_offset = BQ25985_BUSOVP_SC21_OFFSET_uV,
		.busovp_sc21_def = BQ25985_BUSOVP_SC21_DFLT_uV,
		.busovp_bypass_max = BQ25985_BUSOVP_BYP_MAX_uV,
		.busovp_bypass_min = BQ25985_BUSOVP_BYP_MIN_uV,
		.busovp_bypass_step = BQ25985_BUSOVP_BYP_STEP_uV,
		.busovp_bypass_offset = BQ25985_BUSOVP_BYP_OFFSET_uV,
		.busovp_bypass_def = BQ25985_BUSOVP_BYP_DFLT_uV,
		
		.batocp_max=BQ25985_BATOCP_MAX_uA,
		.batocp_min=BQ25985_BATOCP_MIN_uA,
		.batocp_step=BQ25985_BATOCP_STEP_uA,
		.batocp_offset=BQ25985_BATOCP_OFFSET_uA,
		.batocp_def=BQ25985_BATOCP_DFLT_uA,
		.batovp_max=BQ25985_BATOVP_MAX_uV,
		.batovp_min=BQ25985_BATOVP_MIN_uV,
		.batovp_step=BQ25985_BATOVP_STEP_uV,
		.batovp_offset=BQ25985_BATOVP_OFFSET_uV,
		.batovp_def=BQ25985_BATOVP_DFLT_uV,
	},
};

static int bq25985_power_supply_init(struct bq25985_device *bq,//?????
							struct device *dev)
{
	struct power_supply_config psy_cfg = { .drv_data = bq,
						.of_node = dev->of_node, };

	psy_cfg.supplied_to = bq25985_charger_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(bq25985_charger_supplied_to);

	bq->charger = devm_power_supply_register(bq->dev,
						 &bq25985_power_supply_desc,
						 &psy_cfg);
	if (IS_ERR(bq->charger))
	{
		#if TEST
		pr_err("charger devm_power_supply_register error\n");
		#endif
		return -EINVAL;
	}
	bq->battery = devm_power_supply_register(bq->dev,
						      &bq25985_battery_desc,
						      &psy_cfg);
	if (IS_ERR(bq->battery))
	{
		#if TEST
		pr_err("battery devm_power_supply_register error\n");
		#endif
		return -EINVAL;
	}
	return 0;
}

static int bq25985_hw_init(struct bq25985_device *bq)
{
	//struct power_supply_battery_info bat_info = { };
	int wd_reg_val = BQ25985_WATCHDOG_DIS;//
	int wd_max_val = BQ25985_NUM_WD_VAL - 1;//??
	int ret = 0;
	int curr_val;
	int volt_val;
	unsigned int temp=0;
	int i;

	if (bq->watchdog_timer) {
		if (bq->watchdog_timer >= bq25985_watchdog_time[wd_max_val])
			wd_reg_val = wd_max_val;
		else {
			for (i = 0; i < wd_max_val; i++) {
				if (bq->watchdog_timer >= bq25985_watchdog_time[i] && //
				    bq->watchdog_timer < bq25985_watchdog_time[i + 1]) {
					wd_reg_val = i;
					break;
				}
			}
		}
	}
	pr_err("wd_reg_val=%x\n",wd_reg_val);
	ret = regmap_update_bits(bq->regmap, BQ25985_CHRGR_CTRL_3,//
			BQ25985_WATCHDOG_MASK, wd_reg_val);
	if (ret)
	{
		pr_err("watchdog updata error\n");
		return ret;
	}
	/* disable wdt */
	ret = regmap_update_bits(bq->regmap, BQ25985_CHRGR_CTRL_3,
			BQ25985_WATCHDOG_DIS, 0x1 << 2);
	if (ret)
	{
		pr_err("watchdog updata error\n");
		return ret;
	}
	/* disable TSBAT */
	ret = regmap_update_bits(bq->regmap, BQ25985_TEMP_CONTROL,
			BQ25985_TSBAT_FLT_DIS, 0x1 << 2);
	if (ret)
	{
		pr_err("TSBAT_FLT_DIS updata error\n");
		return ret;
	}
	/* disable BUSUCP */
#if 0
	ret = regmap_update_bits(bq->regmap, BQ25985_CHRGR_CTRL_1,
			BQ25985_BUSUCP_DIS, 0x1 << 7);
	if (ret)
	{
		pr_err("BUSUCP updata error\n");
		return ret;
	}
#endif
	/* disable BATOCP */
	ret = regmap_update_bits(bq->regmap, BQ25985_BATOCP,
			BQ25985_BATOCP_DIS, 0x1 << 7);
	if (ret)
	{
		pr_err("BATOCP updata error\n");
		return ret;
	}
	/* VAC1 OVP init 22V */
	ret = regmap_update_bits(bq->regmap, BQ25985_VAC_CONTROL,
			BQ25985_VAC1OVP_MASK, 0x5 << 5);
	if (ret)
	{
		pr_err("VAC1OVP updata error\n");
		return ret;
	}
	/* VAC2 OVP init 22V */
	ret = regmap_update_bits(bq->regmap, BQ25985_VAC_CONTROL,
			BQ25985_VAC2OVP_MASK, 0x5 << 2);
	if (ret)
	{
		pr_err("VAC2OVP updata error\n");
		return ret;
	}
	/* VBUS_ERRLO:1ms VBUS_ERRHI:128us */
	ret = regmap_update_bits(bq->regmap, BQ25985_CHRGR_CTRL_4,
			BQ25985_VBUS_ERRLO_MASK, 0x1 << 1);
	if (ret)
	{
		pr_err("VBUS_ERRLO updata error\n");
		return ret;
	}
	ret = regmap_update_bits(bq->regmap, BQ25985_CHRGR_CTRL_4,
			BQ25985_VBUS_ERRHI_MASK, 0x1);
	if (ret)
	{
		pr_err("VBUS_ERRHI updata error\n");
		return ret;
	}
		
#if 0
	ret = power_supply_get_battery_info(bq->charger, &bat_info);//??
	if (ret) {
		dev_warn(bq->dev, "battery info missing\n");
		return -EINVAL;
	}

	bq->init_data.ichg_max = bat_info.constant_charge_current_max_ua;
	bq->init_data.vreg_max = bat_info.constant_charge_voltage_max_uv;
#endif

	if (bq->state.bypass) {
		/*ret = regmap_update_bits(bq->regmap, BQ25985_CHRGR_CTRL_5,//
					BQ25985_EN_BYPASS, BQ25985_EN_BYPASS);*/
		ret=regmap_read(bq->regmap, BQ25985_CHRGR_CTRL_5, &temp);
		if (ret)
		{	
			pr_err("bypass error0:%d\n",ret);
			return ret;
		}
		pr_err("bypass temp=%d\n",temp);
		temp|=BIT(6);
		pr_err("bypass temp=%d\n",temp);
		ret=regmap_write(bq->regmap,BQ25985_CHRGR_CTRL_5,temp);
		if (ret)
		{	
			pr_err("bypass error:%d\n",ret);
			return ret;
		}
		curr_val = bq->init_data.bypass_ilim;
		volt_val = bq->init_data.bypass_vlim;
	} else if(bq->state.sc41) {
		ret=regmap_read(bq->regmap, BQ25985_CHRGR_CTRL_5, &temp);
		if (ret)
		{	
			pr_err("bypass error0:%d\n",ret);
			return ret;
		}
		pr_err("sc41 temp=%d\n",temp);
		temp=RESETBIT(temp,7);
		udelay(100);
		pr_err("sc41 temp=%d\n",temp);
		ret=regmap_write(bq->regmap,BQ25985_CHRGR_CTRL_5,temp);
		if (ret)
		{	
			pr_err("sc41 error:%d\n",ret);
			return ret;
		}
			curr_val = bq->init_data.sc41_ilim;
			volt_val = bq->init_data.sc41_vlim;
	} else {
		ret=regmap_read(bq->regmap, BQ25985_CHRGR_CTRL_5, &temp);
		if (ret)
		{	
			pr_err("sc21 error0:%d\n",ret);
			return ret;
		}
		pr_err("sc21 temp=%d\n",temp);
		temp|=BQ25985_EN_SC41;
		pr_err("sc21 temp=%d\n",temp);
		ret=regmap_write(bq->regmap,BQ25985_CHRGR_CTRL_5,temp);
		if (ret)
		{	
			pr_err("sc21 error:%d\n",ret);
			return ret;
		}
			curr_val = bq->init_data.sc21_ilim;
			volt_val = bq->init_data.sc21_vlim;
	}

	ret = bq25985_set_input_curr_lim(bq, curr_val);
	if (ret)
	{
		pr_err("set input_curr_lim error\n");
		return ret;
	}
	ret = bq25985_set_input_volt_lim(bq, volt_val);
	if (ret)
	{
		pr_err("set input vol lim error\n");
		return ret;
	}
	ret=regmap_read(bq->regmap, BQ25985_ADC_CONTROL1, &temp);
	if (ret)
		{	
			pr_err("read adc_control error0:%d\n",ret);
			return ret;
		}
	pr_err("temp=%d\n",temp);
	temp|=BIT(7);
	temp|=BIT(5);
	temp|=BIT(3);
	pr_err("temp=%d\n",temp);
	udelay(100);
	ret=regmap_write(bq->regmap,BQ25985_ADC_CONTROL1,temp);
	if (ret)
		{	
			pr_err("write adc control1 back error:%d\n",ret);
			return ret;
		}
	/*return regmap_update_bits(bq->regmap, BQ25985_ADC_CONTROL1,
				 BQ25985_ADC_EN, BQ25985_ADC_EN);*/
	return ret;
}
static int bq25985_parse_dt(struct bq25985_device *bq)//??
{
	int ret;
	int temp;
	ret = device_property_read_u32(bq->dev, "ti,watchdog-timeout-ms",//
				       &bq->watchdog_timer);
	if (ret)
		bq->watchdog_timer = BQ25985_WATCHDOG_MIN;
	if (bq->watchdog_timer > BQ25985_WATCHDOG_MAX ||
	    bq->watchdog_timer < BQ25985_WATCHDOG_MIN)
		return -EINVAL;

	ret = device_property_read_u32(bq->dev,
						      "ti,bypass-enable",&temp);
	bq->state.bypass=temp;
	ret = device_property_read_u32(bq->dev,
						      "ti,sc41-enable",&temp);
	bq->state.sc41=temp;
	ret = device_property_read_u32(bq->dev,
						      "ti,sc21-enable",&temp);
	bq->state.sc21=temp;
	pr_err("%d\n",bq->state.bypass+bq->state.sc21+bq->state.sc41);
	pr_err("%d,%d,%d",bq->state.bypass,bq->state.sc41,bq->state.sc21);
	if(((int)bq->state.bypass+(int)bq->state.sc41+(int)bq->state.sc21)>1) {
		return -12;
	}
	//if(bq->state.sc41==true) {
		ret = device_property_read_u32(bq->dev,//
				       "ti,sc41-ovp-limit-microvolt",
				       &bq->init_data.sc41_vlim);
		if (ret)
			bq->init_data.sc41_vlim = bq->chip_info->busovp_sc41_def;
		pr_err("sc41ovp=%d,def=%d\n",bq->init_data.sc41_vlim,bq->chip_info->busovp_sc41_def);
		if (bq->init_data.sc41_vlim > bq->chip_info->busovp_sc41_max ||
	    	bq->init_data.sc41_vlim < bq->chip_info->busovp_sc41_min) {
			dev_err(bq->dev, "SC41 ovp limit is out of range\n");
			return -EINVAL;
		}

		ret = device_property_read_u32(bq->dev,//
				       "ti,sc41-ocp-limit-microamp",
				       &bq->init_data.sc41_ilim);
		if (ret)
			bq->init_data.sc41_ilim = bq->chip_info->busocp_sc41_def;
		if (bq->init_data.sc41_ilim > bq->chip_info->busocp_sc41_max ||
	    	bq->init_data.sc41_ilim < bq->chip_info->busocp_sc41_min) {
			dev_err(bq->dev, "SC41 ocp limit is out of range\n");
			return -EINVAL;
		}
	//} else if(bq->state.sc21==true) {
		ret = device_property_read_u32(bq->dev,//
				       "ti,sc21-ovp-limit-microvolt",
				       &bq->init_data.sc21_vlim);
		if (ret)
			bq->init_data.sc21_vlim = bq->chip_info->busovp_sc21_def;

		if (bq->init_data.sc21_vlim > bq->chip_info->busovp_sc21_max ||
	    	bq->init_data.sc21_vlim < bq->chip_info->busovp_sc21_min) {
			dev_err(bq->dev, "SC21 ovp limit is out of range\n");
			return -EINVAL;
		}

		ret = device_property_read_u32(bq->dev,//
				       "ti,sc21-ocp-limit-microamp",
				       &bq->init_data.sc21_ilim);
		if (ret)
			bq->init_data.sc21_ilim = bq->chip_info->busocp_sc21_def;
		if (bq->init_data.sc21_ilim > bq->chip_info->busocp_sc21_max ||
	    	bq->init_data.sc21_ilim < bq->chip_info->busocp_sc21_min) {
			dev_err(bq->dev, "SC21 ocp limit is out of range\n");
			return -EINVAL;
		}
	//} else {
		ret = device_property_read_u32(bq->dev,//
				       "ti,bypass-ovp-limit-microvolt",
				       &bq->init_data.bypass_vlim);
		if (ret)
			bq->init_data.bypass_vlim = bq->chip_info->busovp_bypass_def;

		if (bq->init_data.bypass_vlim > bq->chip_info->busovp_bypass_max ||
	    	bq->init_data.bypass_vlim < bq->chip_info->busovp_bypass_min) {
			dev_err(bq->dev, "Bypass ovp limit is out of range\n");
			return -EINVAL;
		}



		ret = device_property_read_u32(bq->dev,//
				       "ti,bypass-ocp-limit-microamp",
				       &bq->init_data.bypass_ilim);
		if (ret)
			bq->init_data.bypass_ilim = bq->chip_info->busocp_bypass_def;
		if (bq->init_data.bypass_ilim > bq->chip_info->busocp_bypass_max ||
	    	bq->init_data.bypass_ilim < bq->chip_info->busocp_bypass_min) {
			dev_err(bq->dev, "Bypass ocp limit is out of range\n");
			return -EINVAL;
		}
	//}
	#if TEST
	int test;
	ret = regmap_read(bq->regmap, 0x0, &test);
	pr_err("0x0 =%d ret=%d\n",test,ret);
	ret = regmap_read(bq->regmap, 0x1, &test);
	pr_err("0x1 =%d ret=%d\n",test,ret);
	ret = regmap_read(bq->regmap, 0x2, &test);
	pr_err("0x2 =%d ret=%d\n",test,ret);
		ret = regmap_read(bq->regmap, 0x3, &test);
	pr_err("0x3 =%d ret=%d\n",test,ret);
		ret = regmap_read(bq->regmap, 0x4, &test);
	pr_err("0x4 =%d ret=%d\n",test,ret);
		ret = regmap_read(bq->regmap, 0x5, &test);
	pr_err("0x5 =%d ret=%d\n",test,ret);
	ret = regmap_read(bq->regmap, 0x30, &test);
	pr_err("0x30 =%d ret=%d\n",test,ret);
	ret = regmap_read(bq->regmap, 0x31, &test);
	pr_err("0x31 =%d ret=%d\n",test,ret);
	#endif
	return 0;
}

static int ops_cp_enable_charge(struct charger_device *chg_dev, bool enable)
{
	struct bq25985_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	ret = bq25985_set_chg_en(bq, !!enable);
	if (ret)
		pr_err("failed enable cp charge\n");

	bq->chg_en = enable;
	// pr_err("enable=%d \n", enable);

	return ret;
}

static int ops_cp_get_charge_enabled(struct charger_device *chg_dev, bool *enable)
{
	struct bq25985_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	ret = bq25985_get_chg_en(bq, enable);
	if (ret)
		pr_err("failed to get cp charge enable\n");

	// pr_err("enable=%d \n", *enable);

	return ret;
}

static int ops_cp_dump_register(struct charger_device *chg_dev)
{
	struct bq25985_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	ret = bq25985_dump_register(bq);
	if (ret)
		pr_err("failed dump register.\n");

	return ret;
}


static int ops_cp_get_vbus(struct charger_device *chg_dev, u32 *val)
{
	struct bq25985_device *bq = charger_get_data(chg_dev);
	int ret = 0;
	int vbat = 0;
	int vac1 = 0;
	int vac2 = 0;

	ret = bq25985_get_adc_vbus(bq);
	if (ret < 0)
		pr_err("failed to get cp charge vbus\n");
	vbat = bq25985_get_adc_vbat(bq);
	vac1 = bq25985_get_adc_vac1(bq);
	vac2 = bq25985_get_adc_vac2(bq);

	*val = ret;
	pr_err("vbus=%d,vbat=%d,vac1=%d,vac2=%d\n", *val, vbat, vac1, vac2);

	return ret;
}

static int ops_cp_get_ibus(struct charger_device *chg_dev, u32 *val)
{
	struct bq25985_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	ret = bq25985_get_adc_ibus(bq);
	if (ret < 0)
		pr_err("failed to get cp charge ibus\n");

	*val = ret;
	pr_err("ibus=%d \n", *val);

	return ret;
}

#if 0
static int ops_cp_get_vbatt(struct charger_device *chg_dev, u32 *val)
{
	struct bq25985_device *bq = charger_get_data(chg_dev);
	int ret = 0;
	int channel = ADC_VBAT;

	ret = bq25985_get_adc_data(bq, channel, val);
	if (ret)
		pr_err("failed to get cp charge ibatt\n");

	pr_err("vbatt=%d channel=%d\n", *val, channel);

	return ret;
}
#endif

static int ops_cp_get_ibatt(struct charger_device *chg_dev, u32 *val)
{
	struct bq25985_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	ret = bq25985_get_adc_ibus(bq);
	if (ret < 0)
		pr_err("failed to get cp charge ibatt\n");

	*val = ret;
	pr_err("ibatt=%d \n", *val);

	return ret;
}

static int ops_cp_set_mode(struct charger_device *chg_dev, int value)
{
	struct bq25985_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	switch (value) {
		case CP_FORWARD_4_TO_1:
			ret = bq25985_set_sc41(bq);
			if (ret < 0)
				pr_err("failed to set sc41 mode\n");
			break;
		case CP_FORWARD_2_TO_1:
			ret = bq25985_set_sc21(bq);
			if (ret < 0)
				pr_err("failed to set sc21 mode\n");
			break;
		case CP_FORWARD_1_TO_1:
			ret = bq25985_set_bypass(bq);
			if (ret < 0)
				pr_err("failed to set bypass mode\n");
			break;
		case REVERSE_1_1_CONVERTER:
			ret = bq25985_set_revert_mode(bq);
			if (ret < 0)
				pr_err("failed to set revert mode\n");
			break;
		default:
			pr_err("have not mode match.\n");
			break;
	}
	pr_err("set_mode=%d\n", value);

	return ret;
}

static int bq25985_get_bypass_enable(struct bq25985_device *bq, bool *enable)
{
	int ret = 0;

	ret = bq25985_get_const_charge_state_bypass(bq);
	if (ret < 0) {
		pr_err("failed to read device id\n");
		return 0;
	}

	if (ret) {
		*enable = true;
	} else {
		*enable = false;
	}

	pr_err("op_mode=%d, bypass_mode=%d", ret, *enable);

	return ret;
}

static int ops_cp_is_bypass_enabled(struct charger_device *chg_dev, bool *enabled)
{
	struct bq25985_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	ret = bq25985_get_bypass_enable(bq, enabled);
	if (ret)
		pr_err("failed to get cp charge bypass enable\n");

	return ret;
}

static int bq25985_init_protection(struct bq25985_device *bq, int forward_work_mode)
{
	int ret = 0;
	int curr_val;
	int volt_val;

	if (bq->state.bypass) {
		curr_val = bq->init_data.bypass_ilim;
		volt_val = bq->init_data.bypass_vlim;
		/* VAC1 OVP init 6.5V */
		ret = regmap_update_bits(bq->regmap, BQ25985_VAC_CONTROL,
				BQ25985_VAC1OVP_MASK, 0);
		if (ret)
		{
			pr_err("VAC1OVP updata error\n");
			return ret;
		}
	} else if(bq->state.sc41) {
		curr_val = bq->init_data.sc41_ilim;
		volt_val = bq->init_data.sc41_vlim;
		/* VAC1 OVP init 14V */
		ret = regmap_update_bits(bq->regmap, BQ25985_VAC_CONTROL,
				BQ25985_VAC1OVP_MASK, 0x5 << 5);
		if (ret)
		{
			pr_err("VAC1OVP updata error\n");
			return ret;
		}
	} else {
		curr_val = bq->init_data.sc21_ilim;
		volt_val= bq->init_data.sc21_vlim;
		/* VAC1 OVP init 22V */
		ret = regmap_update_bits(bq->regmap, BQ25985_VAC_CONTROL,
				BQ25985_VAC1OVP_MASK, 0x2 << 5);
		if (ret)
		{
			pr_err("VAC1OVP updata error\n");
			return ret;
		}
	}

	ret = bq25985_set_input_curr_lim(bq, curr_val);
	if (ret)
	{
		pr_err("set input_curr_lim error\n");
		return ret;
	}
	ret = bq25985_set_input_volt_lim(bq, volt_val);
	if (ret)
	{
		pr_err("set input vol lim error\n");
		return ret;
	}

	return ret;
}

static int ops_cp_device_init(struct charger_device *chg_dev, int value)
{
	struct bq25985_device *bq = charger_get_data(chg_dev);
	int ret = 0;

        //ret = bq25985_init_device(bq);
	ret = bq25985_init_protection(bq, value);
	if (ret < 0)
		pr_err("failed to init cp charge\n");

	return ret;
}

static int ops_cp_enable_adc(struct charger_device *chg_dev, bool enable)
{
	//struct bq25985_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	pr_err("enable_adc=%d\n", enable);

	return ret;
}

static int ops_cp_get_bypass_support(struct charger_device *chg_dev, bool *enabled)
{
	*enabled = 0;
	pr_err("bypass_support=%d\n", *enabled);

	return 0;
}

static int ops_enable_acdrv_manual(struct charger_device *chg_dev, bool enable)
{
	// struct bq25985_device *bq = charger_get_data(chg_dev);
	int ret = 0;

	/*
	ret = bq25985_enable_acdrv_manual(bq, !!enable);
	if (ret)
		pr_err("failed enable cp acdrv manual\n");
	*/
	pr_err("ops_enable_acdrv_manual=%d\n", enable);

	return ret;
}

static const struct charger_ops bq25985_chg_ops = {
	.enable = ops_cp_enable_charge,
	.is_enabled = ops_cp_get_charge_enabled,
	.get_vbus_adc = ops_cp_get_vbus,
	.get_ibus_adc = ops_cp_get_ibus,
	//.cp_get_vbatt = ops_cp_get_vbatt,
	.get_ibat_adc = ops_cp_get_ibatt,
	.cp_set_mode = ops_cp_set_mode,
	.is_bypass_enabled = ops_cp_is_bypass_enabled,
	.cp_device_init = ops_cp_device_init,
	.cp_enable_adc = ops_cp_enable_adc,
	.cp_get_bypass_support = ops_cp_get_bypass_support,
	.cp_dump_register = ops_cp_dump_register,
	.enable_acdrv_manual = ops_enable_acdrv_manual,
};

static int bq25985_register_charger(struct bq25985_device *bq)
{
	bq->chg_dev = charger_device_register("cp_master", bq->dev, bq, &bq25985_chg_ops, &bq->chg_props);

	return 0;
}

static int cp_vbus_get(struct bq25985_device *bq,
	struct mtk_cp_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;

	if (bq) {
		ret = bq25985_get_adc_vbus(bq);
		*val = ret;
	} else
		*val = 0;
	chr_err("%s cp_vbus=%d\n",  __func__, *val);
	return 0;
}

static int cp_ibus_get(struct bq25985_device *bq,
	struct mtk_cp_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;

	if (bq) {
		ret = bq25985_get_adc_ibus(bq);
		*val = ret;
	} else
		*val = 0;
	chr_err("%s cp_ibus=%d\n", __func__, *val);
	return 0;
}

static int cp_tdie_get(struct bq25985_device *bq,
	struct mtk_cp_sysfs_field_info *attr,
	int *val)
{
	//int ret = 0;
	u32 data = 0;

	if (bq) {
		//ret = sc858x_get_adc_data(sc, ADC_TDIE, &data);
		*val = data;
	} else
		*val = 0;
	//ln_err("%s %d\n", __func__, *val);
	return 0;
}

static int chip_ok_get(struct bq25985_device *bq,
	struct mtk_cp_sysfs_field_info *attr,
	int *val)
{
	if (bq)
		*val = bq->chip_ok;
	else
		*val = 0;
	//ln_err("%s %d\n", __func__, *val);
	return 0;
}

static ssize_t cp_sysfs_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *psy;
	struct bq25985_device *bq;
	struct mtk_cp_sysfs_field_info *usb_attr;
	int val;
	ssize_t ret;

	ret = kstrtos32(buf, 0, &val);
	if (ret < 0)
		return ret;

	psy = dev_get_drvdata(dev);
	bq = (struct bq25985_device *)power_supply_get_drvdata(psy);

	usb_attr = container_of(attr,
		struct mtk_cp_sysfs_field_info, attr);
	if (usb_attr->set != NULL)
		usb_attr->set(bq, usb_attr, val);

	return count;
}

static ssize_t cp_sysfs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *psy;
	struct bq25985_device *bq;
	struct mtk_cp_sysfs_field_info *usb_attr;
	int val = 0;
	ssize_t count;

	psy = dev_get_drvdata(dev);
	bq = (struct bq25985_device *)power_supply_get_drvdata(psy);

	usb_attr = container_of(attr,
		struct mtk_cp_sysfs_field_info, attr);
	if (usb_attr->get != NULL)
		usb_attr->get(bq, usb_attr, &val);

	count = scnprintf(buf, PAGE_SIZE, "%d\n", val);
	return count;
}

static struct mtk_cp_sysfs_field_info cp_sysfs_field_tbl[] = {
	CP_SYSFS_FIELD_RO(cp_vbus, CP_PROP_VBUS),
	CP_SYSFS_FIELD_RO(cp_ibus, CP_PROP_IBUS),
	CP_SYSFS_FIELD_RO(cp_tdie, CP_PROP_TDIE),
	CP_SYSFS_FIELD_RO(chip_ok, CP_PROP_CHIP_OK),
};

static struct attribute *
	cp_sysfs_attrs[ARRAY_SIZE(cp_sysfs_field_tbl) + 1];

static const struct attribute_group cp_sysfs_attr_group = {
	.attrs = cp_sysfs_attrs,
};

static void cp_sysfs_init_attrs(void)
{
	int i, limit = ARRAY_SIZE(cp_sysfs_field_tbl);

	for (i = 0; i < limit; i++)
		cp_sysfs_attrs[i] = &cp_sysfs_field_tbl[i].attr.attr;

	cp_sysfs_attrs[limit] = NULL; /* Has additional entry for this */
}

int cp_sysfs_create_group(struct power_supply *psy)
{
	cp_sysfs_init_attrs();

	return sysfs_create_group(&psy->dev.kobj,
			&cp_sysfs_attr_group);
}

static int bq25985_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct bq25985_device *bq;
	int ret;
	int device_id = 0;

	pr_err("start probe!!\n");
	bq = devm_kzalloc(dev, sizeof(*bq), GFP_KERNEL);
	if (!bq)
		return -ENOMEM;

	bq->client = client;
	bq->dev = dev;
	mutex_init(&bq->lock);

	// strncpy(bq->model_name, id->name, I2C_NAME_SIZE);
	// bq->chip_info = &bq25985_chip_info_tbl[id->driver_data];
	bq->chip_info = &bq25985_chip_info_tbl[BQ25985];

	bq->regmap = devm_regmap_init_i2c(client,
					  bq->chip_info->regmap_config);
	if (IS_ERR(bq->regmap)) {
		dev_err(dev, "Failed to allocate register map\n");
		return PTR_ERR(bq->regmap);
	}

	i2c_set_clientdata(client, bq);

	ret = regmap_read(bq->regmap, BQ25985_DEVICE_INFO, &device_id);
	if (ret < 0 || device_id != 0x30) {
		pr_err("hw bq25985 match not found!\n");
		return -ENODEV;
	}

	#if TEST
	int test;
	ret = regmap_read(bq->regmap, 0x0, &test);
	pr_err("0x0 =%d ret=%d\n",test,ret);
	ret = regmap_read(bq->regmap, 0x1, &test);
	pr_err("0x1 =%d ret=%d\n",test,ret);
	ret = regmap_read(bq->regmap, 0x2, &test);
	pr_err("0x2 =%d ret=%d\n",test,ret);
		ret = regmap_read(bq->regmap, 0x3, &test);
	pr_err("0x3 =%d ret=%d\n",test,ret);
		ret = regmap_read(bq->regmap, 0x4, &test);
	pr_err("0x4 =%d ret=%d\n",test,ret);
		ret = regmap_read(bq->regmap, 0x5, &test);
	pr_err("0x5 =%d ret=%d\n",test,ret);
	ret = regmap_read(bq->regmap, 0x30, &test);
	pr_err("0x30 =%d ret=%d\n",test,ret);
	ret = regmap_read(bq->regmap, 0x31, &test);
	pr_err("0x31 =%d ret=%d\n",test,ret);
	#endif
	ret = bq25985_parse_dt(bq);
	if (ret) {
		dev_err(dev, "Failed to read device tree properties%d\n", ret);
		return ret;
	}

	if (client->irq) {
		ret = devm_request_threaded_irq(dev, client->irq, NULL,
						bq25985_irq_handler_thread,
						IRQF_TRIGGER_FALLING |
						IRQF_ONESHOT,
						dev_name(&client->dev), bq);
		if (ret)
			return ret; 
	}
	ret = bq25985_power_supply_init(bq, dev);//
	if (ret) {
		dev_err(dev, "Failed to register power supply\n");
		return ret;
	}

	cp_sysfs_create_group(bq->charger);

	/* charger class register */
	ret = bq25985_register_charger(bq);
	if (ret) {
		pr_err("failed to register charger\n");
		return ret;
	}

	ret = bq25985_hw_init(bq);//
	if (ret) {
		dev_err(dev, "Cannot initialize the chip.\n");
		return ret;
	}
	bq->chip_ok = true;

	bq25985_dump_register(bq);
	pr_err("probe successfully!!\n");

	return 0;
}

static const struct i2c_device_id bq25985_i2c_ids[] = {
	{ "ti,bq25985", BQ25985 },
	{},
};
MODULE_DEVICE_TABLE(i2c, bq25985_i2c_ids);
static const struct of_device_id bq25985_of_match[] = {
	{ .compatible = "ti,bq25985", .data = (void *)BQ25985 },
	{ },
};
MODULE_DEVICE_TABLE(of, bq25985_of_match);
static struct i2c_driver bq25985_driver = {
	.driver = {
		.name = "bq25985",
		.of_match_table = bq25985_of_match,
	},
	.probe = bq25985_probe,
	.id_table = bq25985_i2c_ids,
};

module_i2c_driver(bq25985_driver);
MODULE_AUTHOR("Miao.junguo@ZLingsmart.com");
MODULE_DESCRIPTION("BQ25985_charger_driver");
MODULE_LICENSE("GPL");
