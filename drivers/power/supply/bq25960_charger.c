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
#include <linux/regmap.h>
#include <linux/hardware_info.h>
#include "mtk_charger.h"
#include "charger_class.h"

#define BQ25960_DRV_VERSION              "1.0.0_G"
#define CP_FORWARD_4_TO_1             0
#define CP_FORWARD_2_TO_1             1
#define CP_FORWARD_1_TO_1             2

enum {
    BQ25960,
};

static const char* bq25960_psy_name[] = {
    [BQ25960] = "ti-cp-standalone",
};

static const char* bq25960_irq_name[] = {
    [BQ25960] = "ti-cp-standalone-irq",
};

static int bq25960_mode_data[] = {
    [BQ25960] = BQ25960,
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
                {.mask = BIT(6), .name = "vac2 ovp flag", .notify = BQ25960_NOTIFY_OTHER},
                {.mask = BIT(7), .name = "vac1 ovp flag", .notify = BQ25960_NOTIFY_OTHER},
                },
    },
    { .reg = 0x1b, .len = 8, .bit = {
                {.mask = BIT(0), .name = "wd timeout flag", .notify = BQ25960_NOTIFY_OTHER},
                {.mask = BIT(1), .name = "tdie alm flag", .notify = BQ25960_NOTIFY_OTHER},
                {.mask = BIT(2), .name = "tshut flag", .notify = BQ25960_NOTIFY_OTHER},
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
    [BQ25960_VBUS_OVP]      = BQ25960_CHG_RANGE(7000, 12750, 50, 7000, false),
    [BQ25960_IBUS_OCP]      = BQ25960_CHG_RANGE(1000, 6500, 250, 1000, false),
    [BQ25960_VBUS_OVP_ALM]  = BQ25960_CHG_RANGE(7000, 13350, 50, 7000, false),
    [BQ25960_VBAT_OVP_ALM]  = BQ25960_CHG_RANGE(3500, 4770, 10, 3500, false),
};

enum bq25960_fields {
    VBAT_OVP_DIS, VBAT_OVP,                     //REG00
    VBAT_OVP_ALM_DIS, VBAT_OVP_ALM,             //REG01
    IBAT_OCP_DIS, IBAT_OCP,                     //REG02
    IBAT_OCP_ALM_DIS, IBAT_OCP_ALM,             //REG03
    IBUS_UCP_DIS, IBUS_UCP, CHG_CONFIG_2,       //REG05
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
    .max_register = BQ25960_REGMAX,
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

    int vbus_error;
};

struct mtk_cp_sysfs_field_info {
	struct device_attribute attr;
	enum cp_property prop;
	int (*set)(struct bq25960 *bq,
		struct mtk_cp_sysfs_field_info *attr, int val);
	int (*get)(struct bq25960 *bq,
		struct mtk_cp_sysfs_field_info *attr, int *val);
};

/********************COMMON API***********************/
__maybe_unused static u8 val2reg(enum bq25960_reg_range id, u32 val)
{
    int i;
    u8 reg;
    const struct reg_range *range= &bq25960_reg_range[id];
    if (!range)
        return val;
    if (range->table) {
        if (val <= range->table[0])
            return 0;
        for (i = 1; i < range->num_table - 1; i++) {
            if (val == range->table[i])
                return i;
            if (val > range->table[i] &&
                val < range->table[i + 1])
                return range->round_up ? i + 1 : i;
        }
        return range->num_table - 1;
    }
    if (val <= range->min)
        reg = 0;
    else if (val >= range->max)
        reg = (range->max - range->offset) / range->step;
    else if (range->round_up)
        reg = (val - range->offset) / range->step + 1;
    else
        reg = (val - range->offset) / range->step;
    return reg;
}
__maybe_unused static u32 reg2val(enum bq25960_reg_range id, u8 reg)
{
    const struct reg_range *range= &bq25960_reg_range[id];
    if (!range)
        return reg;
    return range->table ? range->table[reg] :
                  range->offset + range->step * reg;
}

/*********************************************************/
static int bq25960_field_read(struct bq25960 *bq,
                enum bq25960_fields field_id, int *val)
{
    int ret;
    ret = regmap_field_read(bq->rmap_fields[field_id], val);
    if (ret < 0) {
        dev_err(bq->dev, "bq25960 read field %d fail: %d\n", field_id, ret);
    }
    
    return ret;
}

static int bq25960_field_write(struct bq25960 *bq,
                enum bq25960_fields field_id, int val)
{
    int ret;
    
    ret = regmap_field_write(bq->rmap_fields[field_id], val);
    if (ret < 0) {
        dev_err(bq->dev, "bq25960 read field %d fail: %d\n", field_id, ret);
    }
    
    return ret;
}

static int bq25960_read_block(struct bq25960 *bq,
                int reg, uint8_t *val, int len)
{
    int ret;
    ret = regmap_bulk_read(bq->regmap, reg, val, len);
    if (ret < 0) {
        dev_err(bq->dev, "bq25960 read %02x block failed %d\n", reg, ret);
    }
    return ret;
}

/*******************************************************/
static int bq25960_detect_device(struct bq25960 *bq)
{
    int ret;
    int val;
    ret = bq25960_field_read(bq, DEVICE_ID, &val);
    if (ret < 0) {
        dev_err(bq->dev, "%s fail(%d)\n", __func__, ret);
        return ret;
    }
    if (val != BQ25960_DEVICE_ID) {
        dev_info(bq->dev, "%s not find BQ25960, ID = 0x%02x\n", __func__, ret);

        return -EINVAL;
    }
    return ret;
}

static int bq25960_reg_reset(struct bq25960 *bq)
{
    return bq25960_field_write(bq, REG_RST, 1);
}

static int bq25960_adjust_ovp(struct bq25960 *bq)
{
    int ret;
    //modify the VAC1_OVP to 12V
    ret = bq25960_field_write(bq, VAC1_OVP, 2);
    return ret;
}

__maybe_unused static int bq25960_dump_reg(struct bq25960 *bq)
{
    int ret;
    int i;
    int val;
    for (i = 0; i <= BQ25960_REGMAX; i++) {
        ret = regmap_read(bq->regmap, i, &val);
        dev_info(bq->dev, "%s reg[0x%02x] = 0x%02x\n", 
                __func__, i, val);
    }
    return ret;
}

static int bq25960_enable_charge(struct bq25960 *bq, bool en)
{
    int ret;
    dev_info(bq->dev,"%s:%d",__func__,en);
    ret = bq25960_field_write(bq, IBUS_UCP, 1);
    ret = bq25960_field_write(bq,CHG_CONFIG_2, 1);
    ret = bq25960_field_write(bq,TSBUS_FLT_DIS, 1);
    ret = bq25960_field_write(bq,TSBAT_FLT_DIS, 1);
    ret = bq25960_field_write(bq,WD_TIMEOUT_DIS, 1);
    ret = bq25960_field_write(bq, CHG_EN, !!en);
    return ret;
}

static int bq25960_check_charge_enabled(struct bq25960 *bq, bool *enabled)
{
    int ret, val;
    ret = bq25960_field_read(bq, CP_SWITCHING_STAT, &val);
    *enabled = (bool)val;
    dev_info(bq->dev,"%s:%d", __func__, val);

    return ret;
}

__maybe_unused static int bq25960_get_status(struct bq25960 *bq, uint32_t *status)
{
    int ret, val;
    *status = 0;

    ret = bq25960_field_read(bq, VBUS_ERRORHI_STAT, &val);
    if (ret < 0) {
        dev_err(bq->dev, "%s fail to read VBUS_ERRORHI_STAT(%d)\n", __func__, ret);
        return ret;
    }

    if (val != 0)
        *status |= BIT(ERROR_VBUS_HIGH);

    return ret;
}

static int bq25960_enable_adc(struct bq25960 *bq, bool en)
{
    dev_err(bq->dev,"%s:%d", __func__, en);
    return bq25960_field_write(bq, ADC_EN, !!en);
}
__maybe_unused static int bq25960_set_adc_scanrate(struct bq25960 *bq, bool oneshot)
{
    dev_info(bq->dev,"%s:%d",__func__,oneshot);
    return bq25960_field_write(bq, ADC_RATE, !!oneshot);
}

static int bq25960_get_adc_ibus(struct bq25960 *bq)
{
	int ibus_adc_lsb, ibus_adc_msb;
	u16 ibus_adc;
	int ret;

	ret = regmap_read(bq->regmap, BQ25960_IBUS_ADC_MSB, &ibus_adc_msb);
	if (ret)
		return ret;

	ret = regmap_read(bq->regmap, BQ25960_IBUS_ADC_LSB, &ibus_adc_lsb);
	if (ret)
		return ret;

	ibus_adc = (ibus_adc_msb << 8) | ibus_adc_lsb;

	if (ibus_adc_msb & BQ25960_ADC_POLARITY_BIT)
		return ((ibus_adc ^ 0xffff) + 1);

	return ibus_adc;
}

static int bq25960_get_adc_vbus(struct bq25960 *bq)
{
	int vbus_adc_lsb, vbus_adc_msb;
	u16 vbus_adc;
	int ret;

	ret = regmap_read(bq->regmap, BQ25960_VBUS_ADC_MSB, &vbus_adc_msb);
	if (ret)
		return ret;

	ret = regmap_read(bq->regmap, BQ25960_VBUS_ADC_LSB, &vbus_adc_lsb);
	if (ret)
		return ret;

	vbus_adc = (vbus_adc_msb << 8) | vbus_adc_lsb;

	return vbus_adc;
}

static int bq25960_get_adc_ibat(struct bq25960 *bq)
{
	int ret;
	int ibat_adc_lsb, ibat_adc_msb;
	int ibat_adc;

	ret = regmap_read(bq->regmap, BQ25960_IBAT_ADC_MSB, &ibat_adc_msb);
	if (ret)
		return ret;

	ret = regmap_read(bq->regmap, BQ25960_IBAT_ADC_LSB, &ibat_adc_lsb);
	if (ret)
		return ret;

	ibat_adc = (ibat_adc_msb << 8) | ibat_adc_lsb;

	if (ibat_adc_msb & BQ25960_ADC_POLARITY_BIT)
		return ((ibat_adc ^ 0xffff) + 1);

	return ibat_adc;
}

static int bq25960_get_adc_vbat(struct bq25960 *bq)
{
	int vbat_adc_lsb, vbat_adc_msb;
	u16 vbat_adc;
	int ret;

	ret = regmap_read(bq->regmap, BQ25960_VBAT_ADC_MSB, &vbat_adc_msb);
	if (ret)
		return ret;

	ret = regmap_read(bq->regmap, BQ25960_VBAT_ADC_LSB, &vbat_adc_lsb);
	if (ret)
		return ret;

	vbat_adc = (vbat_adc_msb << 8) | vbat_adc_lsb;

	return vbat_adc;
}

static int bq25960_get_adc_tbat(struct bq25960 *bq)
{
	int tbat_adc_lsb, tbat_adc_msb;
	u16 tbat_adc;
	int ret;

	ret = regmap_read(bq->regmap, BQ25960_TSBAT_ADC_MSB, &tbat_adc_msb);
	if (ret)
		return ret;

	ret = regmap_read(bq->regmap, BQ25960_TSBAT_ADC_LSB, &tbat_adc_lsb);
	if (ret)
		return ret;

	tbat_adc = (tbat_adc_msb << 8) | tbat_adc_lsb;

	return tbat_adc;
}
/*
static int bq25960_get_adc_tbus(struct bq25960 *bq)
{
	int tbus_adc_lsb, tbus_adc_msb;
	u16 tbus_adc;
	int ret;

	ret = regmap_read(bq->regmap, BQ25960_TSBUS_ADC_MSB, &tbus_adc_msb);
	if (ret)
		return ret;

	ret = regmap_read(bq->regmap, BQ25960_TSBUS_ADC_LSB, &tbus_adc_lsb);
	if (ret)
		return ret;

	tbus_adc = (tbus_adc_msb << 8) | tbus_adc_lsb;

	return tbus_adc;
}
*/
static int bq25960_get_adc_tdie(struct bq25960 *bq)
{
	int tdie_adc_lsb, tdie_adc_msb;
	u16 tdie_adc;
	int ret;

	ret = regmap_read(bq->regmap, BQ25960_TSBUS_ADC_MSB, &tdie_adc_msb);
	if (ret)
		return ret;

	ret = regmap_read(bq->regmap, BQ25960_TSBUS_ADC_LSB, &tdie_adc_lsb);
	if (ret)
		return ret;

	tdie_adc = (tdie_adc_msb << 8) | tdie_adc_lsb;

	return tdie_adc;
}

__maybe_unused static int bq25960_set_busovp_th(struct bq25960 *bq, int threshold)
{
    int reg_val = val2reg(BQ25960_VBUS_OVP, threshold);
    dev_info(bq->dev,"%s:%d-%d", __func__, threshold, reg_val);
    return bq25960_field_write(bq, VBUS_OVP, reg_val);
}
__maybe_unused static int bq25960_set_busocp_th(struct bq25960 *bq, int threshold)
{
    int reg_val = val2reg(BQ25960_IBUS_OCP, threshold);
    
    dev_info(bq->dev,"%s:%d-%d", __func__, threshold, reg_val);
    return bq25960_field_write(bq, IBUS_OCP, reg_val);
}
__maybe_unused static int bq25960_set_batovp_th(struct bq25960 *bq, int threshold)
{
    int reg_val = val2reg(BQ25960_VBAT_OVP, threshold);
    
    dev_info(bq->dev,"%s:%d-%d", __func__, threshold, reg_val);
    return bq25960_field_write(bq, VBAT_OVP, reg_val);
}
__maybe_unused static int bq25960_set_batocp_th(struct bq25960 *bq, int threshold)
{
    int reg_val = val2reg(BQ25960_IBAT_OCP, threshold);
    
    dev_info(bq->dev,"%s:%d-%d", __func__, threshold, reg_val);
    return bq25960_field_write(bq, IBAT_OCP, reg_val);
}
__maybe_unused static int bq25960_set_vbusovp_alarm(struct bq25960 *bq, int threshold)
{
    int reg_val = val2reg(BQ25960_VBUS_OVP_ALM, threshold);
    
    dev_info(bq->dev,"%s:%d-%d", __func__, threshold, reg_val);
    return bq25960_field_write(bq, VBUS_OVP_ALM, reg_val);
}  
__maybe_unused static int bq25960_set_vbatovp_alarm(struct bq25960 *bq, int threshold)
{
    int reg_val = val2reg(BQ25960_VBAT_OVP_ALM, threshold);
    
    dev_info(bq->dev,"%s:%d-%d", __func__, threshold, reg_val);
    return bq25960_field_write(bq, VBAT_OVP_ALM, reg_val);
}  
__maybe_unused static int bq25960_disable_vbatovp_alarm(struct bq25960 *bq, bool en)
{
    int ret;
    dev_info(bq->dev,"%s:%d",__func__,en);
    ret = bq25960_field_write(bq, VBAT_OVP_ALM_DIS, !!en);
    return ret;
}
__maybe_unused static int bq25960_disable_vbusovp_alarm(struct bq25960 *bq, bool en)
{
    int ret;
    dev_info(bq->dev,"%s:%d",__func__,en);
    ret = bq25960_field_write(bq, VBUS_OVP_ALM_DIS, !!en);
    return ret;
}

__maybe_unused static int bq25960_set_acdrv_enable(struct bq25960 *bq, bool en)
{
	int ret = 0;

	pr_err("%s :%d\n", __func__, en);

	if(en) {
		ret = bq25960_field_write(bq, OTG_EN, 1);
		msleep(10);
		ret = bq25960_field_write(bq, DIS_ACDRV_BOTH, 0);
		ret = bq25960_field_write(bq, ACDRV1_STAT, 1);
	} else {
		ret = bq25960_field_write(bq, VBUS_PD_EN, 1);
		msleep(10);
		ret = bq25960_field_write(bq, ACDRV1_STAT, 0);
		ret = bq25960_field_write(bq, OTG_EN, 0);
	}

	return ret;
}

static int bq25960_init_device(struct bq25960 *bq)
{
    int ret = 0;
    int i;
    struct {
        enum bq25960_fields field_id;
        int conv_data;
    } props[] = {
        {VBAT_OVP_DIS, bq->cfg.vbat_ovp_dis},
        {VBAT_OVP, bq->cfg.vbat_ovp},
        {VBAT_OVP_ALM_DIS, bq->cfg.vbat_ovp_alm_dis},
        {VBAT_OVP_ALM, bq->cfg.vbat_ovp_alm},
        {IBAT_OCP_DIS, bq->cfg.ibat_ocp_dis},
        {IBAT_OCP, bq->cfg.ibat_ocp},
        {IBAT_OCP_ALM_DIS, bq->cfg.ibat_ocp_alm_dis},
        {IBAT_OCP_ALM, bq->cfg.ibat_ocp_alm},
        {IBUS_UCP_DIS, bq->cfg.ibus_ucp_dis},
        {IBUS_UCP, bq->cfg.ibus_ucp},
        //{VBUS_IN_RANGE_DIS, bq->cfg.vbus_in_range_dis},
        {CHG_CONFIG_2, bq->cfg.chg_config_2},
        {VBUS_PD_EN, bq->cfg.vbus_pd_en},
        {VBUS_OVP, bq->cfg.vbus_ovp},
        {VBUS_OVP_ALM_DIS, bq->cfg.vbus_ovp_alm_dis},
        {VBUS_OVP_ALM, bq->cfg.vbus_ovp_alm},
        //{IBUS_OCP_DIS, bq->cfg.ibus_ocp_dis},
        {IBUS_OCP, bq->cfg.ibus_ocp},
        //{TSHUT_DIS, bq->cfg.tshut_dis},
        {TSBUS_FLT_DIS, bq->cfg.tsbus_flt_dis},
        {TSBAT_FLT_DIS, bq->cfg.tsbat_flt_dis},
        {TDIE_ALM, bq->cfg.tdie_alm},
        {TSBUS_FLT, bq->cfg.tsbus_flt},
        {TSBAT_FLT, bq->cfg.tsbat_flt},
        {VAC1_OVP, bq->cfg.vac1_ovp},
        {VAC2_OVP, bq->cfg.vac2_ovp},
        {VAC1_PD_EN, bq->cfg.vac1_pd_en},
        {VAC2_PD_EN, bq->cfg.vac2_pd_en},
        {FSW_SET, bq->cfg.fsw_set},
        {WD_TIMEOUT, bq->cfg.wd_timeout},
        {WD_TIMEOUT_DIS, bq->cfg.wd_timeout_dis},
        {SET_IBAT_SNS_RES, bq->cfg.ibat_sns_r},
        {SS_TIMEOUT, bq->cfg.ss_timeout},
        {IBUS_UCP_FALL_DG, bq->cfg.ibus_ucp_fall_dg},
        {VOUT_OVP_DIS, bq->cfg.vout_ovp_dis},
        {VOUT_OVP, bq->cfg.vout_ovp},
        {MS, bq->cfg.ms},
        //{PMID2OUT_UVP, bq->cfg.pmid2out_uvp},
        //{PMID2OUT_OVP, bq->cfg.pmid2out_ovp},
    };

    ret = bq25960_reg_reset(bq);
    if (ret < 0) {
        dev_err(bq->dev, "%s Failed to reset registers(%d)\n", __func__, ret);
    }

    msleep(10);

    for (i = 0; i < ARRAY_SIZE(props); i++) {
        ret = bq25960_field_write(bq, props[i].field_id, props[i].conv_data);
    }
    bq25960_enable_adc(bq, true);

   //bq25960_dump_reg(bq);
    return ret;
}

/********************creat devices note start*************************************************/
static ssize_t bq25960_show_registers(struct device *dev,
                struct device_attribute *attr, char *buf)
{
    struct bq25960 *bq = dev_get_drvdata(dev);
    u8 addr;
    int val;
    u8 tmpbuf[300];
    int len;
    int idx = 0;
    int ret;
    idx = snprintf(buf, PAGE_SIZE, "%s:\n", "bq25960");
    for (addr = 0x0; addr <= BQ25960_REGMAX; addr++) {
        ret = regmap_read(bq->regmap, addr, &val);
        if (ret == 0) {
            len = snprintf(tmpbuf, PAGE_SIZE - idx,
                    "Reg[%.2X] = 0x%.2x\n", addr, val);
            memcpy(&buf[idx], tmpbuf, len);
            idx += len;
        }
    }
    return idx;
}

static ssize_t bq25960_store_register(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct bq25960 *bq = dev_get_drvdata(dev);
    int ret;
    unsigned int reg;
    unsigned int val;
    ret = sscanf(buf, "%x %x", &reg, &val);
    if (ret == 2 && reg <= BQ25960_REGMAX)
        regmap_write(bq->regmap, (unsigned char)reg, (unsigned char)val);
    return count;
}
static DEVICE_ATTR(registers, 0660, bq25960_show_registers, bq25960_store_register);
static void bq25960_create_device_node(struct device *dev)
{
    device_create_file(dev, &dev_attr_registers);
}
/********************creat devices note end*************************************************/
/*
* interrupt does nothing, just info event chagne, other module could get info
* through power supply interface
*/

static void bq25960_check_fault_status(struct bq25960 *bq)
{
    int ret;
    u8 flag = 0;
    int i,j;

    for (i=0;i < ARRAY_SIZE(cp_intr_flag);i++) {
        ret = bq25960_read_block(bq, cp_intr_flag[i].reg, &flag, 1);
        for (j=0; j <  cp_intr_flag[i].len; j++) {
            if (flag & cp_intr_flag[i].bit[j].mask) {
                dev_info(bq->dev,"trigger :%s\n",cp_intr_flag[i].bit[j].name);
            }
        }
    }

}

static irqreturn_t bq25960_irq_handler(int irq, void *data)
{
    struct bq25960 *bq = data;
    dev_info(bq->dev,"INT OCCURED\n");
    bq25960_check_fault_status(bq);
    power_supply_changed(bq->psy);
    return IRQ_HANDLED;
}

static int bq25960_register_interrupt(struct bq25960 *bq)
{
    int ret;
    if (gpio_is_valid(bq->irq_gpio)) {
        ret = gpio_request_one(bq->irq_gpio, GPIOF_DIR_IN,"bq25960_irq");
        if (ret) {
            dev_err(bq->dev,"failed to request bq25960_irq\n");
            return -EINVAL;
        }
        bq->irq = gpio_to_irq(bq->irq_gpio);
        if (bq->irq < 0) {
            dev_err(bq->dev,"failed to gpio_to_irq\n");
            return -EINVAL;
        }
    } else {
        dev_info(bq->dev,"irq gpio not provided\n");
        return -EINVAL;
    }
    if (bq->irq) {
        ret = devm_request_threaded_irq(&bq->client->dev, bq->irq,
                NULL, bq25960_irq_handler,
                IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                bq25960_irq_name[bq->mode], bq);
        if (ret < 0) {
            dev_err(bq->dev,"request irq for irq=%d failed, ret =%d\n",
                            bq->irq, ret);
            return ret;
        }
        enable_irq_wake(bq->irq);
    }
    return ret;
}

static enum power_supply_property bq25960_charger_props[] = {
    POWER_SUPPLY_PROP_ONLINE,
    POWER_SUPPLY_PROP_PRESENT,
    POWER_SUPPLY_PROP_VOLTAGE_NOW,
    POWER_SUPPLY_PROP_CURRENT_NOW,
    POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
    POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
    POWER_SUPPLY_PROP_TEMP,
};

static int bq25960_charger_get_property(struct power_supply *psy,
                enum power_supply_property psp,
                union power_supply_propval *val)
{
    struct bq25960 *bq = power_supply_get_drvdata(psy);
    //int result;
    int ret;
    switch (psp) {
    case POWER_SUPPLY_PROP_ONLINE:
        bq25960_check_charge_enabled(bq, &bq->charge_enabled);
        val->intval = bq->charge_enabled;
        break;
    case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = bq25960_get_adc_vbus(bq);
		if (ret < 0)
			return ret;

		val->intval = ret;
        break;
    case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = bq25960_get_adc_ibus(bq);
		if (ret < 0)
			return ret;

		val->intval = ret;
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = bq25960_get_adc_vbat(bq);
		if (ret < 0)
			return ret;

		val->intval = ret;
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = bq25960_get_adc_ibat(bq);
		if (ret < 0)
			return ret;

		val->intval = ret;
        break;
    case POWER_SUPPLY_PROP_TEMP:
		ret = bq25960_get_adc_tbat(bq);
		if (ret < 0)
			return ret;

		val->intval = ret;
        break;
    default:
        return -EINVAL;
    }
    return 0;
}

static int bq25960_charger_set_property(struct power_supply *psy,
                    enum power_supply_property prop,
                    const union power_supply_propval *val)
{
    struct bq25960 *bq = power_supply_get_drvdata(psy);
    switch (prop) {
    case POWER_SUPPLY_PROP_ONLINE:
        bq25960_enable_charge(bq, val->intval);
        pr_info("POWER_SUPPLY_PROP_ONLINE: %s\n",val->intval ? "enable" : "disable");
        break;
    default:
        return -EINVAL;
    }
    return 0;
}

static int bq25960_charger_is_writeable(struct power_supply *psy,
                    enum power_supply_property prop)
{
    return 0;
}

static int bq25960_psy_register(struct bq25960 *bq)
{
    bq->psy_cfg.drv_data = bq;
    bq->psy_cfg.of_node = bq->dev->of_node;
    bq->psy_desc.name = bq25960_psy_name[bq->mode];
    bq->psy_desc.type = POWER_SUPPLY_TYPE_MAINS;
    bq->psy_desc.properties = bq25960_charger_props;
    bq->psy_desc.num_properties = ARRAY_SIZE(bq25960_charger_props);
    bq->psy_desc.get_property = bq25960_charger_get_property;
    bq->psy_desc.set_property = bq25960_charger_set_property;
    bq->psy_desc.property_is_writeable = bq25960_charger_is_writeable;
    bq->psy = devm_power_supply_register(bq->dev, 
            &bq->psy_desc, &bq->psy_cfg);
    if (IS_ERR(bq->psy)) {
        dev_info(bq->dev, "%s failed to register psy\n", __func__);
        return PTR_ERR(bq->psy);
    }
    dev_info(bq->dev, "%s power supply register successfully\n", bq->psy_desc.name);

    return 0;
}


#if 0
static int bq25960_set_work_mode(struct bq25960 *bq, int mode)
{
    bq->mode = mode;
    dev_err(bq->dev,"work mode is %s\n", bq->mode == BQ25960_STANDALONE 
        ? "standalone" : (bq->mode == BQ25960_MASTER ? "master" : "slave"));
    return 0;
}
#endif

static int ti_bq25960_set_enable(struct charger_device *charger_pump, bool enable)
{
	struct bq25960 *bq = charger_get_data(charger_pump);
	int ret;

	ret = bq25960_enable_charge(bq, enable);

	return ret;
}

static int ti_bq25960_get_is_enable(struct charger_device *charger_pump, bool *enable)
{
	struct bq25960 *bq = charger_get_data(charger_pump);
	int ret;

	ret = bq25960_check_charge_enabled(bq, enable);

	return ret;
}

__maybe_unused static int ti_bq25960_get_status(struct charger_device *charger_pump, uint32_t *status)
{
	struct bq25960 *bq = charger_get_data(charger_pump);
	int ret = 0;

	ret = bq25960_get_status(bq, status);

	return ret;
}



__maybe_unused static int ti_bq25960_get_adc_value(struct charger_device *charger_pump, enum ti_adc_channel ch, u32 *value)
{
	struct bq25960 *bq = charger_get_data(charger_pump);
	int ret = 0;

    switch (ch) {
	case ADC_GET_VBUS:
		*value = bq25960_get_adc_vbus(bq);
        break;
	case ADC_GET_VBAT:
		*value = bq25960_get_adc_vbat(bq);
        break;
	case ADC_GET_IBUS:
		*value = bq25960_get_adc_ibus(bq);
        break;
	case ADC_GET_IBAT:
		*value = bq25960_get_adc_ibat(bq);
        break;
	case ADC_GET_TDIE:
		*value = bq25960_get_adc_tdie(bq);
        break;
	default:
		break;
	}

	return ret;
}

static int ti_bq25960_set_enable_adc(struct charger_device *charger_pump, bool en)
{
	struct bq25960 *bq = charger_get_data(charger_pump);
	int ret = 0;

	ret = bq25960_enable_adc(bq, en);

	return ret;
}

static int ti_bq25960_set_cp_workmode(struct charger_device *charger_pump, int workmode)
{
	struct bq25960 *bq = charger_get_data(charger_pump);
	int ret = 0;

	if (workmode == CP_FORWARD_1_TO_1) {
		ret = bq25960_field_write(bq, BYPASS_EN, 1);
		pr_err("%s set cp workmode = 1\n", __func__);
	} else {
		ret = bq25960_field_write(bq, BYPASS_EN, 0);
		pr_err("%s set cp workmode = 0\n", __func__);
	}

	return ret;
}

__maybe_unused static int ti_bq25960_get_cp_workmode(struct charger_device *charger_pump, int *workmode)
{
	int ret, val;
	struct bq25960 *bq = charger_get_data(charger_pump);

	ret = bq25960_field_read(bq, BYPASS_EN, &val);

	*workmode = val;
	pr_err("%s :get cp workmode = %d\n", __func__, *workmode);

	return ret;
}

static int ti_bq25960_get_vbus(struct charger_device *chg_dev, u32 *val)
{
	int ret = 0;
	struct bq25960 *bq = charger_get_data(chg_dev);

	ret = bq25960_get_adc_vbus(bq);
	if(ret < 0)
		pr_err("failed to get cp charge vbus\n");
    else
        *val = ret;

	pr_err("%s :vbus=%d\n", __func__, *val);
	return ret;
}

static int ti_bq25960_get_ibus(struct charger_device *chg_dev, u32 *val)
{
	int ret = 0;
	struct bq25960 *bq = charger_get_data(chg_dev);

	ret = bq25960_get_adc_ibus(bq);
	if(ret < 0)
		pr_err("failed to get cp charge vbus\n");
    else
        *val = ret;

	pr_err("%s :ibus=%d\n", __func__, *val);
	return ret;
}

static int ti_bq25960_get_vbatt(struct charger_device *chg_dev, u32 *val)
{
	int ret = 0;
	struct bq25960 *bq = charger_get_data(chg_dev);

	ret = bq25960_get_adc_vbat(bq);
	if(ret < 0)
		pr_err("failed to get cp charge vbus\n");
    else
        *val = ret;

	pr_err("%s :vbat=%d\n", __func__, *val);
	return ret;
}

static int ti_bq25960_get_ibat(struct charger_device *chg_dev, u32 *val)
{
	int ret = 0;
	struct bq25960 *bq = charger_get_data(chg_dev);

	ret = bq25960_get_adc_ibat(bq);
	if(ret < 0)
		pr_err("failed to get cp charge vbus\n");
    else
        *val = ret;

	pr_err("%s :ibat=%d\n", __func__, *val);
	return ret;
}

static int ti_bq25960_is_bypass_enabled(struct charger_device *chg_dev, bool *enabled)
{
	int ret = 0, mode = 0;

	ret = ti_bq25960_get_cp_workmode(chg_dev, &mode);
	if (ret){
		pr_err("failed to read cp workmode.\n");
		return ret;
	}

	if(mode == 1)
		*enabled = true;
	else
		*enabled = false;

	pr_err("op_mode=%d, bypass_mode=%d\n", mode, *enabled);

	return ret;
}

static int ti_bq25960_get_bypass_support(struct charger_device *chg_dev, bool *enabled)
{
	*enabled = 1;
	pr_err("%s bq25960 support bypass mode:%d\n", __func__, *enabled);

	return 0;
}

static int ti_bq25960_init_protection(struct bq25960 *bq, int forward_work_mode)
{
	int ret = 0;

	if (forward_work_mode == CP_FORWARD_1_TO_1) {
		ret = bq25960_set_busovp_th(bq, 6000);
		ret = bq25960_set_busocp_th(bq, 5500);
		// ret = sc858x_set_usbovp_th(sc, 6500); //缺少VAC1与VAC2 OVP设置
	} else {
		ret = bq25960_set_busovp_th(bq, 11000);
		ret = bq25960_set_busocp_th(bq, 6375);
		// ret = sc858x_set_usbovp_th(sc, 14000); //缺少VAC1与VAC2 OVP设置
	}

	return ret;
}

static int ti_bq25960_device_init(struct charger_device *chg_dev, int value)
{
	struct bq25960 *bq = charger_get_data(chg_dev);
	int ret = 0;

	ret = ti_bq25960_init_protection(bq, value);
	if(ret < 0)
		pr_err("%s failed to init cp charge.\n", __func__);

	return ret;
}

static int ti_bq25960_enable_acdrv_manual(struct charger_device *chg_dev, bool enable)
{
	struct bq25960 *bq = charger_get_data(chg_dev);
	int ret = 0;

	ret = bq25960_set_acdrv_enable(bq, enable);
	if (ret)
		pr_err("failed enable cp acdrv manual\n");

	bq25960_dump_reg(bq);

	return ret;
}

static int ops_cp_get_chip_ok(struct charger_device *chg_dev, int *val)
{
	struct bq25960 *bq = charger_get_data(chg_dev);
	int ret = 0;

	if (bq)
		*val = bq->chip_ok;
	else
		*val = 0;

	pr_err("chip_ok=%d \n", *val);

	return ret;
}

static int ti_bq25960_dump_reg(struct charger_device *chg_dev)
{
	int ret = 0;
	struct bq25960 *bq = charger_get_data(chg_dev);

	ret = bq25960_dump_reg(bq);
	return ret;
}

static const struct charger_ops bq25960_chargerpump_ops = {
	.enable = ti_bq25960_set_enable,
	.is_enabled = ti_bq25960_get_is_enable,
	.get_vbus_adc = ti_bq25960_get_vbus,
	.get_ibus_adc = ti_bq25960_get_ibus,
	.cp_get_vbatt = ti_bq25960_get_vbatt,
	.get_ibat_adc = ti_bq25960_get_ibat,
	.cp_set_mode = ti_bq25960_set_cp_workmode,
 	.is_bypass_enabled = ti_bq25960_is_bypass_enabled,
 	.cp_device_init = ti_bq25960_device_init,
	.cp_enable_adc = ti_bq25960_set_enable_adc,
	.cp_get_bypass_support = ti_bq25960_get_bypass_support,
	.cp_dump_register = ti_bq25960_dump_reg,
	.enable_acdrv_manual = ti_bq25960_enable_acdrv_manual,
	.cp_chip_ok = ops_cp_get_chip_ok,
};

static int bq25960_parse_dt(struct bq25960 *bq, struct device *dev)
{
    struct device_node *np = dev->of_node;
    int i;
    int ret;
    struct {
        char *name;
        int *conv_data;
    } props[] = {
        {"bq,bq25960,vbat-ovp-dis", &(bq->cfg.vbat_ovp_dis)},
        {"bq,bq25960,vbat-ovp", &(bq->cfg.vbat_ovp)},
        {"bq,bq25960,vbat-ovp-alm-dis", &(bq->cfg.vbat_ovp_alm_dis)},
        {"bq,bq25960,vbat-ovp-alm", &(bq->cfg.vbat_ovp_alm)},
        {"bq,bq25960,ibat-ocp-dis", &(bq->cfg.ibat_ocp_dis)},
        {"bq,bq25960,ibat-ocp", &(bq->cfg.ibat_ocp)},
        {"bq,bq25960,ibat-ocp-alm-dis", &(bq->cfg.ibat_ocp_alm_dis)},
        {"bq,bq25960,ibat-ocp-alm", &(bq->cfg.ibat_ocp_alm)},
        {"bq,bq25960,ibus-ucp-dis", &(bq->cfg.ibus_ucp_dis)},
        {"bq,bq25960,ibus-ucp", &(bq->cfg.ibus_ucp)},
        {"bq,bq25960,chg-config-2", &(bq->cfg.chg_config_2)},
        //{"bq,bq25960,vbus-in-range-dis", &(bq->cfg.vbus_in_range_dis)},
        {"bq,bq25960,vbus-pd-en", &(bq->cfg.vbus_pd_en)},
        {"bq,bq25960,vbus-ovp", &(bq->cfg.vbus_ovp)},
        {"bq,bq25960,vbus-ovp-alm-dis", &(bq->cfg.vbus_ovp_alm_dis)},
        {"bq,bq25960,vbus-ovp-alm", &(bq->cfg.vbus_ovp_alm)},
        //{"bq,bq25960,ibus-ocp-dis", &(bq->cfg.ibus_ocp_dis)},
        {"bq,bq25960,ibus-ocp", &(bq->cfg.ibus_ocp)},
        //{"bq,bq25960,tshut-dis", &(bq->cfg.tshut_dis)},
        {"bq,bq25960,tsbus-flt-dis", &(bq->cfg.tsbus_flt_dis)},
        {"bq,bq25960,tsbat-flt-dis", &(bq->cfg.tsbat_flt_dis)},
        {"bq,bq25960,tdie-alm", &(bq->cfg.tdie_alm)},
        {"bq,bq25960,tsbus-flt", &(bq->cfg.tsbus_flt)},
        {"bq,bq25960,tsbat-flt", &(bq->cfg.tsbat_flt)},
        {"bq,bq25960,vac1-ovp", &(bq->cfg.vac1_ovp)},
        {"bq,bq25960,vac2-ovp", &(bq->cfg.vac2_ovp)},
        {"bq,bq25960,vac1-pd-en", &(bq->cfg.vac1_pd_en)},
        {"bq,bq25960,vac2-pd-en", &(bq->cfg.vac2_pd_en)},
        {"bq,bq25960,fsw-set", &(bq->cfg.fsw_set)},
        {"bq,bq25960,wd-timeout", &(bq->cfg.wd_timeout)},
        {"bq,bq25960,wd-timeout-dis", &(bq->cfg.wd_timeout_dis)},
        {"bq,bq25960,ibat-sns-r", &(bq->cfg.ibat_sns_r)},
        {"bq,bq25960,ss-timeout", &(bq->cfg.ss_timeout)},
        {"bq,bq25960,ibus-ucp-fall-dg", &(bq->cfg.ibus_ucp_fall_dg)},
        {"bq,bq25960,vout-ovp-dis", &(bq->cfg.vout_ovp_dis)},
        {"bq,bq25960,vout-ovp", &(bq->cfg.vout_ovp)},
        {"bq,bq25960,ms", &(bq->cfg.ms)},
        //{"bq,bq25960,pmid2out-uvp", &(bq->cfg.pmid2out_uvp)},
        //{"bq,bq25960,pmid2out-ovp", &(bq->cfg.pmid2out_ovp)},
    };
    /* initialize data for optional properties */
    for (i = 0; i < ARRAY_SIZE(props); i++) {
        ret = of_property_read_u32(np, props[i].name,
                        props[i].conv_data);
        if (ret < 0) {
            dev_err(bq->dev, "can not read %s \n", props[i].name);
            return ret;
        }
        //dev_err(bq->dev, "parse_dt %s is 0x%x \n", props[i].name,props[i].conv_data);
    }
    bq->irq_gpio = of_get_named_gpio(np, "bq25960,intr_gpio", 0);
    if (!gpio_is_valid(bq->irq_gpio)) {
        dev_info(bq->dev,"fail to valid gpio : %d\n", bq->irq_gpio);
        return -EINVAL;
    }
    if (of_property_read_string(np, "charger_name", &bq->chg_dev_name) < 0) {
		bq->chg_dev_name = "charger";
		dev_info(bq->dev, "no charger name\n");
	}
    return 0;
}

static int bq25960_register_charger(struct bq25960 *bq)
{
	bq->chg_dev = charger_device_register("cp_master", bq->dev, bq, &bq25960_chargerpump_ops, &bq->chg_props);

	return 0;
}

static int cp_vbus_get(struct bq25960 *bq,
	struct mtk_cp_sysfs_field_info *attr,
	int *val)
{

	if (bq) {
		*val = bq25960_get_adc_vbus(bq);
	} else
		*val = 0;

	chr_err("cp_vbus=%d\n", *val);
	return 0;
}

static int cp_ibus_get(struct bq25960 *bq,
	struct mtk_cp_sysfs_field_info *attr,
	int *val)
{
	if (bq) {
		*val = bq25960_get_adc_ibus(bq);
	} else
		*val = 0;

	chr_err("cp_ibus=%d\n", *val);
	return 0;
}

static int cp_tdie_get(struct bq25960 *bq,
	struct mtk_cp_sysfs_field_info *attr,
	int *val)
{
	if (bq) {
		*val = bq25960_get_adc_tdie(bq);
	} else
		*val = 0;

	chr_err("cp_tdie=%d\n",*val);
	return 0;
}

static int chip_ok_get(struct bq25960 *bq,
	struct mtk_cp_sysfs_field_info *attr,
	int *val)
{
	if (bq)
		*val = bq->chip_ok;
	else
		*val = 0;

	return 0;
}

static ssize_t cp_sysfs_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *psy;
	struct bq25960 *bq;
	struct mtk_cp_sysfs_field_info *usb_attr;
	int val;
	ssize_t ret;

	ret = kstrtos32(buf, 0, &val);
	if (ret < 0)
		return ret;

	psy = dev_get_drvdata(dev);
	bq = (struct bq25960 *)power_supply_get_drvdata(psy);

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
	struct bq25960 *bq;
	struct mtk_cp_sysfs_field_info *usb_attr;
	int val = 0;
	ssize_t count;

	psy = dev_get_drvdata(dev);
	bq = (struct bq25960 *)power_supply_get_drvdata(psy);

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

static struct of_device_id bq25960_charger_match_table[] = {
    {   .compatible = "bq,bq25960-standalone", 
        .data = &bq25960_mode_data[BQ25960],
    },

    {},
};
#if 0
static void bq25960_check_alarm_status(struct bq25960 *bq)
{
	int ret;
	u8 flag = 0;
	u8 stat = 0;
	mutex_lock(&bq->data_lock);
	mutex_unlock(&bq->data_lock);
}
#endif

static int bq25960_charger_probe(struct i2c_client *client,
                    const struct i2c_device_id *id)
{
	struct bq25960 *bq;
	const struct of_device_id *match;
	struct device_node *node = client->dev.of_node;
	int ret, i;

	dev_info(&client->dev, "%s (%s)\n", __func__, BQ25960_DRV_VERSION);
	
	bq = devm_kzalloc(&client->dev, sizeof(struct bq25960), GFP_KERNEL);
	if (!bq) {
		ret = -ENOMEM;
		goto err_kzalloc;
	}

	bq->dev = &client->dev;
	bq->client = client;
	bq->regmap = devm_regmap_init_i2c(client,
                            &bq25960_regmap_config);
	if (IS_ERR(bq->regmap)) {
		dev_info(bq->dev, "Failed to initialize regmap\n");
		ret = PTR_ERR(bq->regmap);
	    goto err_regmap_init;
    }

    for (i = 0; i < ARRAY_SIZE(bq25960_reg_fields); i++) {
        const struct reg_field *reg_fields = bq25960_reg_fields;
        bq->rmap_fields[i] =
            devm_regmap_field_alloc(bq->dev,
                        bq->regmap,
                        reg_fields[i]);
        if (IS_ERR(bq->rmap_fields[i])) {
            dev_info(bq->dev, "cannot allocate regmap field\n");
            ret = PTR_ERR(bq->rmap_fields[i]);
            goto err_regmap_field;
        }
    }

    ret = bq25960_detect_device(bq);
    if (ret < 0) {
        dev_err(bq->dev, "%s detect device fail\n", __func__);
        goto err_detect_dev;
    }

    i2c_set_clientdata(client, bq);
    bq25960_create_device_node(&(client->dev));

    match = of_match_node(bq25960_charger_match_table, node);
    if (match == NULL) {
        dev_err(bq->dev, "device tree match not found!\n");
        goto err_match_node;
    }

    ret = bq25960_parse_dt(bq, &client->dev);
    if (ret < 0) {
        dev_err(bq->dev, "%s parse dt failed(%d)\n", __func__, ret);
        goto err_parse_dt;
    }

    ret = bq25960_init_device(bq);
    if (ret < 0) {
        dev_err(bq->dev, "%s init device failed(%d)\n", __func__, ret);
        goto err_init_device;
    }

    ret = bq25960_psy_register(bq);
    if (ret < 0) {
        dev_err(bq->dev, "%s psy register failed(%d)\n", __func__, ret);
        goto err_register_psy;
    }

    cp_sysfs_create_group(bq->psy);

    ret = bq25960_register_interrupt(bq);
    if (ret < 0) {
        dev_err(bq->dev, "%s register irq fail(%d)\n",
                        __func__, ret);
        goto err_register_irq;
    }

    ret = bq25960_register_charger(bq);
	if(ret < 0)
		goto err_register_bq_charger;

    ret = bq25960_adjust_ovp(bq);
    if (ret < 0) {
        pr_err("error to adjust the ovp \n");
        goto err_adjust_ovp;
    }

    bq->chip_ok = true;
    dev_info(bq->dev, "bq25960 probe successfully!\n");
    hardwareinfo_set_prop(HARDWARE_SUB_CHARGER_MASTER, "bq25960_charger_pump");
    return 0;

err_adjust_ovp:
err_register_psy:
err_register_irq:
err_register_bq_charger:
err_init_device:
	power_supply_unregister(bq->psy);
err_detect_dev:
err_match_node:
err_parse_dt:
err_regmap_init:
err_regmap_field:
	devm_kfree(&client->dev, bq);
err_kzalloc:
	dev_err(&client->dev,"bq25960 probe fail\n");
    return ret;
}

static void bq25960_charger_remove(struct i2c_client *client)
{
    struct bq25960 *bq = i2c_get_clientdata(client);

    power_supply_unregister(bq->psy);
    devm_kfree(&client->dev, bq);
    return;
}

static void bq25960_charger_shutdown(struct i2c_client *client)
{
	struct bq25960 *bq = i2c_get_clientdata(client);
	bq25960_enable_adc(bq, false);
	pr_err("shutdown success\n");
}

#ifdef CONFIG_PM_SLEEP
static int bq25960_suspend(struct device *dev)
{
    struct bq25960 *bq = dev_get_drvdata(dev);

    dev_info(bq->dev, "Suspend successfully!");
    if (device_may_wakeup(dev))
        enable_irq_wake(bq->irq);
    disable_irq(bq->irq);
    return 0;
}

static int bq25960_resume(struct device *dev)
{
    struct bq25960 *bq = dev_get_drvdata(dev);

    dev_info(bq->dev, "Resume successfully!");
    if (device_may_wakeup(dev))
        disable_irq_wake(bq->irq);
    enable_irq(bq->irq);
    return 0;
}

static const struct dev_pm_ops bq25960_pm = {
    SET_SYSTEM_SLEEP_PM_OPS(bq25960_suspend, bq25960_resume)
};
#endif

static struct i2c_driver bq25960_charger_driver = {
    .driver     = {
        .name   = "bq25960",
        .owner  = THIS_MODULE,
        .of_match_table = bq25960_charger_match_table,
#ifdef CONFIG_PM_SLEEP
        .pm = &bq25960_pm,
#endif
    },
    .probe      = bq25960_charger_probe,
    .remove     = bq25960_charger_remove,
    .shutdown   = bq25960_charger_shutdown,
};

module_i2c_driver(bq25960_charger_driver);
MODULE_DESCRIPTION("TI BQ25960 Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("South Chip <Aiden-yu@southchip.com>");