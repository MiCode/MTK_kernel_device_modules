/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __BQ25960_H__
#define __BQ25960_H__

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
#include <linux/regmap.h>
#include "mtk_charger.h"
#include "charger_class.h"

#define BQ25960_DRV_VERSION              "1.0.0_G"
#define CP_FORWARD_4_TO_1             0
#define CP_FORWARD_2_TO_1             1
#define CP_FORWARD_1_TO_1             2
#define BQ25960_ROLE_REVERSE          1

enum {
	BQ25960,
	BQ25960_REVERSE,
};

static const char* bq25960_psy_name[] = {
	[BQ25960] = "cp_standalone",
};

static const char* bq25960_irq_name[] = {
	[BQ25960] = "ti-cp-standalone-irq",
};

static int bq25960_mode_data[] = {
	[BQ25960] = BQ25960,
	[BQ25960_REVERSE] = BQ25960_ROLE_REVERSE,
};

enum bq25960_notify {
	BQ25960_NOTIFY_OTHER = 0,
	BQ25960_NOTIFY_IBUSUCPF,
	BQ25960_NOTIFY_VBUSOVPALM,
	BQ25960_NOTIFY_VBATOVPALM,
	BQ25960_NOTIFY_IBUSOCP,
	BQ25960_NOTIFY_VBUSOVP,
	BQ25960_NOTIFY_IBATOCP,
	BQ25960_NOTIFY_VBATOVP,
	BQ25960_NOTIFY_VOUTOVP,
	BQ25960_NOTIFY_VDROVP,
	BQ25960_NOTIFY_VACOVP,
	BQ25960_NOTIFY_TDIEFLT,
};

enum ti_adc_channel {
	ADC_GET_VBUS = 0,
	ADC_GET_VSYS,
	ADC_GET_VBAT,
	ADC_GET_VAC,
	ADC_GET_IBUS,
	ADC_GET_IBAT,
	ADC_GET_TSBUS,
	ADC_GET_TSBAT,
	ADC_GET_TDIE,
	ADC_GET_MAX,
};

enum bq25960_error_stata {
	ERROR_VBUS_HIGH = 0,
	//ERROR_VBUS_LOW,
	ERROR_VBUS_OVP,
	ERROR_IBUS_OCP,
	ERROR_VBAT_OVP,
	ERROR_IBAT_OCP,
};

struct flag_bit {
	int notify;
	int mask;
	char *name;
};

struct intr_flag {
	int reg;
	int len;
	struct flag_bit bit[8];
};

static struct intr_flag cp_intr_flag[] = {
	{ .reg = 0x18, .len = 7, .bit = {
		{.mask = BIT(0), .name = "vbus ovp alm flag", .notify = BQ25960_NOTIFY_VBUSOVPALM},
		{.mask = BIT(1), .name = "vbus ovp flag", .notify = BQ25960_NOTIFY_VBUSOVP},
		{.mask = BIT(3), .name = "ibat ocp alm flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(4), .name = "ibat ocp flag", .notify = BQ25960_NOTIFY_IBATOCP},
		{.mask = BIT(5), .name = "vout ovp flag", .notify = BQ25960_NOTIFY_VOUTOVP},
		{.mask = BIT(6), .name = "vbat ovp alm flag", .notify = BQ25960_NOTIFY_VBATOVPALM},
		{.mask = BIT(7), .name = "vbat ovp flag", .notify = BQ25960_NOTIFY_VBATOVP},
		},
	},
	{ .reg = 0x19, .len = 3, .bit = {
		{.mask = BIT(2), .name = "pin diag fall flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(5), .name = "ibus ucp fall flag", .notify = BQ25960_NOTIFY_IBUSUCPF},
		{.mask = BIT(7), .name = "ibus ocp flag", .notify = BQ25960_NOTIFY_IBUSOCP},
		},
	},
	{ .reg = 0x1a, .len = 8, .bit = {
		{.mask = BIT(0), .name = "acrb2 config flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(1), .name = "acrb1 config flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(2), .name = "vbus present flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(3), .name = "vac2 insert flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(4), .name = "vac1 insert flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(5), .name = "vat insert flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(6), .name = "vac2 ovp flag", .notify = BQ25960_NOTIFY_VACOVP},
		{.mask = BIT(7), .name = "vac1 ovp flag", .notify = BQ25960_NOTIFY_VACOVP},
		},
	},
	{ .reg = 0x1b, .len = 8, .bit = {
		{.mask = BIT(0), .name = "wd timeout flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(1), .name = "tdie alm flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(2), .name = "tshut flag", .notify = BQ25960_NOTIFY_TDIEFLT},
		{.mask = BIT(3), .name = "tsbat flt flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(4), .name = "tsbus flt flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(5), .name = "tsbus tsbat alm flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(6), .name = "ss timeout flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(7), .name = "adc done flag", .notify = BQ25960_NOTIFY_OTHER},
		},
	},
	{ .reg = 0x1c, .len = 2, .bit = {
		//{.mask = BIT(3), .name = "vbus errorlo flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(4), .name = "vbus errorhi flag", .notify = BQ25960_NOTIFY_OTHER},
		{.mask = BIT(6), .name = "cp switching flag", .notify = BQ25960_NOTIFY_OTHER},
		},
	},
};

#define BQ25960_DEVICE_ID                0x00
#define BQ25960_VBAT_OVP_MIN             3500
#define BQ25960_VBAT_OVP_MAX             4770
#define BQ25960_VBAT_OVP_STEP            10
#define BQ25960_IBAT_OCP_MIN             2000
#define BQ25960_IBAT_OCP_MAX             8500
#define BQ25960_IBAT_OCP_STEP            100
#define BQ25960_VBUS_OVP_MIN             7000
#define BQ25960_VBUS_OVP_MAX             12750
#define BQ25960_VBUS_OVP_STEP            50
#define BQ25960_IBUS_OCP_MIN             1000
#define BQ25960_IBUS_OCP_MAX             6500
#define BQ25960_IBUS_OCP_STEP            250
#define BQ25960_REG18                    0x18
#define BQ25960_REG25                    0x25
#define BQ25960_REGMAX                   0x37
#define BQ25960_AND_BQ25960H_REGMAX      0xff

/* [REVCHG] Register reverse charging */
#define BQ25960_REG_05                   0x05
#define BQ25960_REG_0F                   0x0F

#define BQ25960_REG_10                   0x10
#define BQ25960_REG_0E                   0x0E
#define BQ25960_REG_06                   0x06
#define BQ25960_REG_0A                   0x0A
#define BQ25960_REG_9A                   0x9A
#define BQ25960_REG_9B                   0x9B
#define BQ25960_REG_A0                   0xA0
#define BQ25960_REG_F9                   0xF9
#define BQ25960_REG_FA                   0xFA
#define BQ25960_REG_A8                   0xA8

/*FOR ADC*/
#define BQ25960_IBUS_ADC_MSB		0x25
#define BQ25960_IBUS_ADC_LSB		0x26
#define BQ25960_VBUS_ADC_MSB		0x27
#define BQ25960_VBUS_ADC_LSB		0x28
#define BQ25960_VBAT_ADC_MSB		0x2F
#define BQ25960_VBAT_ADC_LSB		0x30
#define BQ25960_IBAT_ADC_MSB		0x31
#define BQ25960_IBAT_ADC_LSB		0x32
#define BQ25960_TSBUS_ADC_MSB		0x33
#define BQ25960_TSBUS_ADC_LSB		0x34
#define BQ25960_TSBAT_ADC_MSB		0x35
#define BQ25960_TSBAT_ADC_LSB		0x36
#define BQ25960_TDIE_ADC_MSB		0x37
#define BQ25960_TDIE_ADC_LSB		0x38
#define BQ25960_ADC_POLARITY_BIT	BIT(7)

enum bq25960_reg_range {
	BQ25960_VBAT_OVP,
	BQ25960_IBAT_OCP,
	BQ25960_VBUS_OVP,
	BQ25960_IBUS_OCP,
	BQ25960_VBUS_OVP_ALM,
	BQ25960_VBAT_OVP_ALM,
};

struct reg_range {
	u32 min;
	u32 max;
	u32 step;
	u32 offset;
	const u32 *table;
	u16 num_table;
	bool round_up;
};

#define BQ25960_CHG_RANGE(_min, _max, _step, _offset, _ru) \
{ \
	.min = _min, \
	.max = _max, \
	.step = _step, \
	.offset = _offset, \
	.round_up = _ru, \
}
#define BQ25960_CHG_RANGE_T(_table, _ru) \
    { .table = _table, .num_table = ARRAY_SIZE(_table), .round_up = _ru, }
static const struct reg_range bq25960_reg_range[] = {
    [BQ25960_VBAT_OVP]      = BQ25960_CHG_RANGE(3500, 4770, 10, 3500, false),
    [BQ25960_IBAT_OCP]      = BQ25960_CHG_RANGE(2000, 8000, 100, 2000, false),
    [BQ25960_VBUS_OVP]      = BQ25960_CHG_RANGE(7000, 13350, 50, 7000, false),
    [BQ25960_IBUS_OCP]      = BQ25960_CHG_RANGE(1000, 6500, 250, 1000, false),
    [BQ25960_VBUS_OVP_ALM]  = BQ25960_CHG_RANGE(7000, 13350, 50, 7000, false),
    [BQ25960_VBAT_OVP_ALM]  = BQ25960_CHG_RANGE(3500, 4770, 10, 3500, false),
};

enum bq25960_fields {
	VBAT_OVP_DIS, VBAT_OVP,                     //REG00
	VBAT_OVP_ALM_DIS, VBAT_OVP_ALM,             //REG01
	IBAT_OCP_DIS, IBAT_OCP,                     //REG02
	IBAT_OCP_ALM_DIS, IBAT_OCP_ALM,             //REG03
	IBUS_UCP_DIS, IBUS_UCP, CHG_CONFIG_2, VBUS_ERRHI_DIS,      //REG05
	VBUS_PD_EN, VBUS_OVP,                       //REG06
	VBUS_OVP_ALM_DIS, VBUS_OVP_ALM,             //REG07
	/*IBUS_OCP_DIS,*/ IBUS_OCP,                 //REG08
	TSHUT_DIS, TSBUS_FLT_DIS, TSBAT_FLT_DIS,    //REG0A
	TDIE_ALM,                                   //REG0B
	TSBUS_FLT,                                  //REG0C
	TSBAT_FLT,                                  //REG0D
	VAC1_OVP, VAC2_OVP, VAC1_PD_EN, VAC2_PD_EN, //REG0E
	REG_RST, HIZ_EN, OTG_EN, CHG_EN, BYPASS_EN, DIS_ACDRV_BOTH, ACDRV1_STAT, ACDRV2_STAT,   //REG0F
	FSW_SET, WD_TIMEOUT, WD_TIMEOUT_DIS,                //REG10
	SET_IBAT_SNS_RES, SS_TIMEOUT, IBUS_UCP_FALL_DG,     //REG11
	VOUT_OVP_DIS, VOUT_OVP, MS,                         //REG12
	VBAT_OVP_ALM_STAT,IBAT_OCP_ALM_STAT,                //REG13
	IBUS_OCP_STAT,IBUS_UCP_FALL_STAT,                   //REG14
	VBAT_INSERT_STAT, VBUS_PRESENT_STAT,                //REG15
	CP_SWITCHING_STAT,VBUS_ERRORHI_STAT,/*VBUS_ERRORLO_STAT,*/  ////REG17
	DEVICE_ID,                                  //REG22
	ADC_EN, ADC_RATE,                           //REG23
	F_MAX_FIELDS,
};
//REGISTER
static const struct reg_field bq25960_reg_fields[] = {
	/*reg00*/
	[VBAT_OVP_DIS] = REG_FIELD(0x00, 7, 7),
	[VBAT_OVP] = REG_FIELD(0x00, 0, 6),
	/*reg01*/
	[VBAT_OVP_ALM_DIS] = REG_FIELD(0x01, 7, 7),
	[VBAT_OVP_ALM] = REG_FIELD(0x01, 0, 6),
	/*reg02*/
	[IBAT_OCP_DIS] = REG_FIELD(0x02, 7, 7),
	[IBAT_OCP] = REG_FIELD(0x02, 0, 6),
	/*reg03*/
	[IBAT_OCP_ALM_DIS] = REG_FIELD(0x03, 7, 7),
	[IBAT_OCP_ALM] = REG_FIELD(0x03, 0, 6),
	/*reg05*/
	[IBUS_UCP_DIS] = REG_FIELD(0x05, 7, 7),
	[IBUS_UCP] = REG_FIELD(0x05, 6, 6),
	[CHG_CONFIG_2] = REG_FIELD(0x05, 3, 3),
	[VBUS_ERRHI_DIS] = REG_FIELD(0x05, 2, 2),
	/*reg06*/
	[VBUS_PD_EN] = REG_FIELD(0x06, 7, 7),
	[VBUS_OVP] = REG_FIELD(0x06, 0, 6),
	/*reg07*/
	[VBUS_OVP_ALM_DIS] = REG_FIELD(0x07, 7, 7),
	[VBUS_OVP_ALM] = REG_FIELD(0x07, 0, 6),
	/*reg08*/
	//[IBUS_OCP_DIS] = REG_FIELD(0x08, 7, 7),
	[IBUS_OCP] = REG_FIELD(0x08, 0, 4),
	/*reg0A*/
	[TSHUT_DIS] = REG_FIELD(0x0A, 7, 7),
	[TSBUS_FLT_DIS] = REG_FIELD(0x0A, 3, 3),
	[TSBAT_FLT_DIS] = REG_FIELD(0x0A, 2, 2),
	/*reg0B*/
	[TDIE_ALM] = REG_FIELD(0x0B, 0, 7),
	/*reg0C*/
	[TSBUS_FLT] = REG_FIELD(0x0C, 0, 7),
	/*reg0D*/
	[TSBAT_FLT] = REG_FIELD(0x0D, 0, 7),
	/*reg0E*/
	[VAC1_OVP] = REG_FIELD(0x0E, 5, 7),
	[VAC2_OVP] = REG_FIELD(0x0E, 2, 4),
	[VAC1_PD_EN] = REG_FIELD(0x0E, 1, 1),
	[VAC2_PD_EN] = REG_FIELD(0x0E, 0, 0),
	/*reg0F*/
	[REG_RST] = REG_FIELD(0x0F, 7, 7),
	[HIZ_EN] = REG_FIELD(0x0F, 6, 6),
	[OTG_EN] = REG_FIELD(0x0F, 5, 5),
	[CHG_EN] = REG_FIELD(0x0F, 4, 4),
	[BYPASS_EN] = REG_FIELD(0x0F, 3, 3),
	[DIS_ACDRV_BOTH] = REG_FIELD(0x0F, 2, 2),
	[ACDRV1_STAT] = REG_FIELD(0x0F, 1, 1),
	[ACDRV2_STAT] = REG_FIELD(0x0F, 0, 0),
	/*reg10*/
	[FSW_SET] = REG_FIELD(0x10, 5, 7),
	[WD_TIMEOUT] = REG_FIELD(0x10, 3, 4),
	[WD_TIMEOUT_DIS] = REG_FIELD(0x10, 2, 2),
	/*reg11*/
	[SET_IBAT_SNS_RES] = REG_FIELD(0x11, 7, 7),
	[SS_TIMEOUT] = REG_FIELD(0x11, 4, 6),
	[IBUS_UCP_FALL_DG] = REG_FIELD(0x11, 2, 3),
	/*reg12*/
	[VOUT_OVP_DIS] = REG_FIELD(0x12, 7, 7),
	[VOUT_OVP] = REG_FIELD(0x12, 5, 6),
	[MS] = REG_FIELD(0x12, 0, 1),
		/*reg13*/
	[VBAT_OVP_ALM_STAT] = REG_FIELD(0x13, 6, 6),
	[IBAT_OCP_ALM_STAT] = REG_FIELD(0x13, 3, 3),
	/*reg14*/
	[IBUS_OCP_STAT] = REG_FIELD(0x14, 7, 7),
	[IBUS_UCP_FALL_STAT] = REG_FIELD(0x14, 5, 5),
	/*reg15*/
	[VBAT_INSERT_STAT] = REG_FIELD(0x15, 5, 5),
	[VBUS_PRESENT_STAT] = REG_FIELD(0x15, 2, 2),
	/*reg17*/
	[CP_SWITCHING_STAT] = REG_FIELD(0x17, 6, 6), //CONV_ACTIVE_STAT
	[VBUS_ERRORHI_STAT] = REG_FIELD(0x17, 4, 4),
	//[VBUS_ERRORLO_STAT] = REG_FIELD(0x17, 3, 3),
	/*reg22*/
	[DEVICE_ID] = REG_FIELD(0x22, 0, 7),
	/*reg23*/
	[ADC_EN] = REG_FIELD(0x23, 7, 7),
	[ADC_RATE] = REG_FIELD(0x23, 6, 6),
};

static const struct regmap_config bq25960_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = BQ25960_AND_BQ25960H_REGMAX,
};


struct bq25960_cfg_e {
	/*reg00*/
	int vbat_ovp_dis;
	int vbat_ovp;
	/*reg01*/
	int vbat_ovp_alm_dis;
	int vbat_ovp_alm;
	/*reg02*/
	int ibat_ocp_dis;
	int ibat_ocp;
	/*reg03*/
	int ibat_ocp_alm_dis;
	int ibat_ocp_alm;
	/*reg05*/
	int ibus_ucp_dis;
	int ibus_ucp;
	int chg_config_2;
	/*reg06*/
	int vbus_pd_en;
	int vbus_ovp;
	/*reg07*/
	int vbus_ovp_alm_dis;
	int vbus_ovp_alm;
	/*reg08*/
	int ibus_ocp_dis;
	int ibus_ocp;
	/*reg0a*/
	int tshut_dis;
	int tsbus_flt_dis;
	int tsbat_flt_dis;
	/*reg0b*/
	int tdie_alm;
	/*reg0c*/
	int tsbus_flt;
	/*reg0d*/
	int tsbat_flt;
	/*reg0e*/
	int vac1_ovp;
	int vac2_ovp;
	int vac1_pd_en;
	int vac2_pd_en;
	/*reg10*/
	int fsw_set;
	int wd_timeout;
	int wd_timeout_dis;
	/*reg11*/
	int ibat_sns_r;
	int ss_timeout;
	int ibus_ucp_fall_dg;
	/*reg12*/
	int vout_ovp_dis;
	int vout_ovp;
	int ms;
};

struct bq25960 {
	struct device *dev;
	struct i2c_client *client;
	struct regmap *regmap;
	struct regmap_field *rmap_fields[F_MAX_FIELDS];
	struct bq25960_cfg_e cfg;
	int irq_gpio;
	int irq;
	int mode;
	bool charge_enabled;
	int usb_present;
	int vbus_volt;
	int ibus_curr;
	int vbat_volt;
	int ibat_curr;
	int die_temp;
	int chip_ok;
	const char *chg_dev_name;
	struct charger_device *chg_dev;
	struct charger_properties chg_props;
	struct power_supply_desc psy_desc;
	struct power_supply_config psy_cfg;
	struct power_supply *psy;
	bool batt_present;
	bool vbus_present;
	struct mutex data_lock;
	struct mutex i2c_rw_lock;
	int vbus_error;
	struct delayed_work set_rcp_work;
};

extern int bq25960h_set_otg_preconfigure(bool en);
extern int bq25960h_enable_otg(bool en);

#endif /* __BQ25960_H__ */
