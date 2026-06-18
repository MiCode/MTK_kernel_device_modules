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
#ifdef CONFIG_MTK_CLASS
#include "charger_class.h"
#ifdef CONFIG_MTK_CHARGER_V4P19
#include "mtk_charger_intf.h"
#endif /*CONFIG_MTK_CHARGER_V4P19*/
#endif /*CONFIG_MTK_CLASS*/
#ifdef CONFIG_SOUTHCHIP_DVCHG_CLASS
#include "dvchg_class.h"
#endif /*CONFIG_SOUTHCHIP_DVCHG_CLASS*/
#ifdef CONFIG_SC854X_PROTOCOL
#include "sc854x_protocol.h"
#endif /*CONFIG_SC854X_PROTOCOL*/
#define SC854x_DRV_VERSION              "1.1.0_G"
enum {
    SC8545_ID,
    SC8546_ID,
    SC8547_ID,
    SC8548_ID,
    SC8549_ID,
};
static int id_data[] = {
    [SC8545_ID] = 0x66,
    [SC8546_ID] = 0x67,
    [SC8547_ID] = 0x66,
    [SC8548_ID] = 0x49,
    [SC8549_ID] = 0x66,
};
enum {
    SC854X_STANDALONG = 0,
    SC854X_MASTER,
    SC854X_SLAVE,
};
static const char* sc854x_psy_name[] = {
    [SC854X_STANDALONG] = "sc-cp-standalone",
    [SC854X_MASTER] = "sc-cp-master",
    [SC854X_SLAVE] = "sc-cp-slave",
};
static const char* sc854x_irq_name[] = {
    [SC854X_STANDALONG] = "sc854x-standalone-irq",
    [SC854X_MASTER] = "sc854x-master-irq",
    [SC854X_SLAVE] = "sc854x-slave-irq",
};
static int sc854x_mode_data[] = {
    [SC854X_STANDALONG] = SC854X_STANDALONG,
    [SC854X_MASTER]     = SC854X_MASTER,
    [SC854X_SLAVE]      = SC854X_SLAVE,
};
typedef enum{
    STANDALONE = 0,
    MASTER,
    SLAVE,
}WORK_MODE;
typedef enum {
    ADC_IBUS,
    ADC_VBUS,
    ADC_VAC,
    ADC_VOUT,
    ADC_VBAT,
    ADC_IBAT,
    ADC_TDIE,
    ADC_MAX_NUM,
}SC_854X_ADC_CH;
#ifdef CONFIG_MTK_CLASS
static const u32 sc854x_adc_accuracy_tbl[ADC_MAX_NUM] = {
    150000, /* IBUS */
    35000,  /* VBUS */
    35000,  /* VAC  */
    20000,  /* VOUT */
    20000,  /* VBAT */
    200000, /* IBAT */
    4,      /* TDIE */
};
#endif /*CONFIG_MTK_CLASS*/
static const int sc854x_adc_m[] =
    {1875, 375, 5, 125, 125, 3125, 5};
static const int sc854x_adc_l[] =
    {1000, 100, 1, 100, 100, 1000, 10};
enum sc854x_notify {
    SC854X_NOTIFY_OTHER = 0,
    SC854X_NOTIFY_IBUSUCPF,
    SC854X_NOTIFY_VBUSOVPALM,
    SC854X_NOTIFY_VBATOVPALM,
    SC854X_NOTIFY_IBUSOCP,
    SC854X_NOTIFY_VBUSOVP,
    SC854X_NOTIFY_IBATOCP,
    SC854X_NOTIFY_VBATOVP,
    SC854X_NOTIFY_VOUTOVP,
    SC854X_NOTIFY_VDROVP,
};
enum sc854x_error_stata {
    ERROR_VBUS_HIGH = 0,
    ERROR_VBUS_LOW,
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
    { .reg = 0x02, .len = 1, .bit = {
                {.mask = BIT(4), .name = "vac ovp flag", .notify = SC854X_NOTIFY_OTHER}},
    },
    { .reg = 0x03, .len = 1, .bit = {
                {.mask = 0x08, .name = "vdrop ovp flag", .notify = SC854X_NOTIFY_OTHER}},
    },
    { .reg = 0x06, .len = 4, .bit = {
                {.mask = 0x01, .name = "pin diag fail", .notify = SC854X_NOTIFY_OTHER},
                {.mask = 0x02, .name = "reg timeout", .notify = SC854X_NOTIFY_OTHER},
                {.mask = 0x08, .name = "ss timeout", .notify = SC854X_NOTIFY_OTHER},
                {.mask = 0x80, .name = "tshut flag", .notify = SC854X_NOTIFY_OTHER}},
    },
    { .reg = 0x09, .len = 3, .bit = {
                {.mask = 0x08, .name = "wd timeout flag", .notify = SC854X_NOTIFY_OTHER},
                {.mask = 0x20, .name = "ibus ucp rise", .notify = SC854X_NOTIFY_OTHER},
                {.mask = 0x40, .name = "por flag", .notify = SC854X_NOTIFY_OTHER}},
    },
    { .reg = 0x0a, .len = 1, .bit = {
                {.mask = 0x08, .name = "vbatreg active", .notify = SC854X_NOTIFY_OTHER}},
    },
    { .reg = 0x0b, .len = 1, .bit = {
                {.mask = 0x08, .name = "ibatreg active", .notify = SC854X_NOTIFY_OTHER}},
    },
    { .reg = 0x0c, .len = 1, .bit = {
                {.mask = 0x04, .name = "vbus uvlo fall", .notify = SC854X_NOTIFY_OTHER}},
    },
    { .reg = 0x0d, .len = 3, .bit = {
                {.mask = 0x04, .name = "pmid2out ovp", .notify = SC854X_NOTIFY_OTHER},
                {.mask = 0x08, .name = "pmid2out uvp", .notify = SC854X_NOTIFY_OTHER}},
    },
    { .reg = 0x0f, .len = 8, .bit = {
                {.mask = 0x01, .name = "vbat in flag", .notify = SC854X_NOTIFY_OTHER},
                {.mask = 0x02, .name = "adapt in flag", .notify = SC854X_NOTIFY_OTHER},
                {.mask = 0x04, .name = "ibus ucp flag", .notify = SC854X_NOTIFY_OTHER},
                {.mask = 0x08, .name = "ibus ocp flag", .notify = SC854X_NOTIFY_IBUSOCP},
                {.mask = 0x10, .name = "vbus ovp flag", .notify = SC854X_NOTIFY_VBUSOVP},
                {.mask = 0x20, .name = "ibat ocp flag", .notify = SC854X_NOTIFY_IBATOCP},
                {.mask = 0x40, .name = "vbat ovp flag", .notify = SC854X_NOTIFY_VBATOVP},
                {.mask = 0x80, .name = "vout ovp flag", .notify = SC854X_NOTIFY_VOUTOVP}},
    },
};
/************************************************************************/
#define SC854x_REGMAX                   0xE9
#define SC854X_REG_13                   0x13
#define SC854X_VBUS_OVP_MIN                6000
#define SC854X_VBUS_OVP_MAX                12350
#define SC854X_VBUS_OVP_LSB                50
#define SC854X_IBUS_OCP_MIN                1200
#define SC854X_IBUS_OCP_MAX                5700
#define SC854X_IBUS_OCP_LSB                300
#define SC854X_BAT_OVP_MIN                 3500
#define SC854X_BAT_OVP_MAX                 5075
#define SC854X_BAT_OVP_LSB                 25
#define SC854X_BAT_OCP_MIN                 2000
#define SC854X_BAT_OCP_MAX                 8300
#define SC854X_BAT_OCP_LSB                 100
#define SC854X_AC_OVP_MIN                  11000
#define SC854X_AC_OVP_MAX                  17000
#define SC854X_AC_OVP_LSB                   1000
#define SC854X_AC_OVP_6P5V                  6500
enum sc854x_reg_range {
    SC854X_VBAT_OVP,
    SC854X_IBAT_OCP,
    SC854X_VBUS_OVP,
    SC854X_IBUS_OCP,
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
// static const u32 SC854X_iboost[] = {
//     500, 1200,
// };
// static const u32 SC854X_vindpm[] = {
//     3900, 4000, 4100, 4200, 4300, 4400, 4500, 4600,
//     4700, 4800, 4900, 5000, 5100, 8000, 8200, 8400,
// };
#define SC854X_CHG_RANGE(_min, _max, _step, _offset, _ru) \
{ \
    .min = _min, \
    .max = _max, \
    .step = _step, \
    .offset = _offset, \
    .round_up = _ru, \
}
#define SC854X_CHG_RANGE_T(_table, _ru) \
    { .table = _table, .num_table = ARRAY_SIZE(_table), .round_up = _ru, }
static const struct reg_range sc854x_reg_range[] = {
    [SC854X_VBAT_OVP]      = SC854X_CHG_RANGE(3500, 5075, 25, 3500, false),
    [SC854X_IBAT_OCP]      = SC854X_CHG_RANGE(2000, 8300, 100, 2000, false),
    [SC854X_VBUS_OVP]      = SC854X_CHG_RANGE(6000, 12350, 50, 6000, false),
    [SC854X_IBUS_OCP]      = SC854X_CHG_RANGE(1200, 5700, 300, 1200, false),
};
enum sc854x_fields {
    VBAT_OVP_DIS, VBAT_OVP,
    IBAT_OCP_DIS, IBAT_OCP,
    VAC_OVP_DIS, OVPGATE_DIS, VAC_OVP_STAT,  VAC_OVP_FLAG, VAC_OVP_MASK, VAC_OVP,
    VDROP_OVP_DIS, VDROP_OVP_STAT, VDROP_OVP_FLAG, VDROP_OVP_MASK, VDROP_OVP_THRESHOLD_SET,VDROP_DEGLITCH_SET,
    VBUS_OVP_DIS, VBUS_OVP,
    IBUS_UCP_DIS, IBUS_OCP_DIS, IBUS_UCP_FALL_DEGLITCH_SET, IBUS_OCP,  /**/
    TSHUT_FLAG, TSHUT_STAT, VBUS_ERRORLO_STAT, VBUS_ERRORHI_STAT, SS_TIMEOUT_FLAG,
    CP_SWITCHING_STAT, REG_TIMEOUT_FLAG, PIN_DIAG_FAIL_FLAG, /**/
    CHG_EN, REG_RST, FREQ_SHIFT, FSW_SET,
    SS_TIMEOUT, REG_TIMEOUT_DIS, VOUT_OVP_DIS, SET_IBAT_SNS_RES, VBUS_PD_EN, VAC_PD_EN,
    MODE, POR_FLAG, IBUS_UCP_RISE_FLAG, IBUS_UCP_RISE_MASK, WD_TIMEOUT_FLAG, WD_TIMEOUT,
    VBAT_REG_EN, VBATREG_ACTIVE_STAT, VBATREG_ACTIVE_FLAG, VBATREG_ACTIVE_MASK, SET_VBATREG,
    IBAT_REG_EN, IBATREG_ACTIVE_STAT, IBATREG_ACTIVE_FLAG, IBATREG_ACTIVE_MASK, SET_IBATREG,
    VBUS_UVLO_FALL_MASK, VBUS_UVLO_FALL_FLAG, FORVE_VAC_OK, ACDRV_HI_EN,
    PMID2OUT_UVP, PMID2OUT_OVP, PMID2OUT_UVP_FLAG, PMID2OUT_OVP_FLAG, PMID2OUT_UVP_STAT, PMID2OUT_OVP_STAT,
    ADC_EN, ADC_RATE,  ADC_DONE_STAT, ADC_DONE_FLAG, ADC_DONE_MASK,
    VBUS_ADC_DIS, VAC_ADC_DIS, VOUT_ADC_DIS, VBAT_ADC_DIS, IBAT_ADC_DIS, IBUS_ADC_DIS, TDIE_ADC_DIS,
    DEVICE_ID,
    VBUS_IN_RANGE_DIS,
    F_MAX_FIELDS,
};
//REGISTER
static const struct reg_field sc854x_reg_fields[] = {
    /*reg00*/
    [VBAT_OVP_DIS] = REG_FIELD(0x00, 7, 7),
    [VBAT_OVP] = REG_FIELD(0x00, 0, 6),
    /*reg01*/
    [IBAT_OCP_DIS] = REG_FIELD(0x01, 7, 7),
    [IBAT_OCP] = REG_FIELD(0x01, 0, 6),
    /*reg02*/
    [VAC_OVP_DIS] = REG_FIELD(0x02, 7, 7),
    [OVPGATE_DIS] = REG_FIELD(0x02, 6, 6),
    [VAC_OVP_STAT] = REG_FIELD(0x02, 5, 5),
    [VAC_OVP_FLAG] = REG_FIELD(0x02, 4, 4),
    [VAC_OVP_MASK] = REG_FIELD(0x02, 3, 3),
    [VAC_OVP] = REG_FIELD(0x02, 0, 2),
    /*reg03*/
    [VDROP_OVP_DIS] = REG_FIELD(0x03, 7, 7),
    [VDROP_OVP_STAT] = REG_FIELD(0x03, 4, 4),
    [VDROP_OVP_FLAG] = REG_FIELD(0x03, 3, 3),
    [VDROP_OVP_MASK] = REG_FIELD(0x03, 2, 2),
    [VDROP_OVP_THRESHOLD_SET] = REG_FIELD(0x03, 1, 1),
    [VDROP_DEGLITCH_SET] = REG_FIELD(0x03, 0, 0),
    /*reg04*/
    [VBUS_OVP_DIS] = REG_FIELD(0x04, 7, 7),
    [VBUS_OVP] = REG_FIELD(0x04, 0, 6),
    /*reg05*/
    [IBUS_UCP_DIS] = REG_FIELD(0x05, 7, 7),
    [IBUS_OCP_DIS] = REG_FIELD(0x05, 6, 6),
    [IBUS_UCP_FALL_DEGLITCH_SET] = REG_FIELD(0x05, 4, 5),
    [IBUS_OCP] = REG_FIELD(0x05, 0, 3),
    /*reg06*/
    [TSHUT_FLAG] = REG_FIELD(0x06, 7, 7),
    [TSHUT_STAT] = REG_FIELD(0x06, 6, 6),
    [VBUS_ERRORLO_STAT] = REG_FIELD(0x06, 5, 5),
    [VBUS_ERRORHI_STAT] = REG_FIELD(0x06, 4, 4),
    [SS_TIMEOUT_FLAG] = REG_FIELD(0x06, 3, 3),
    [CP_SWITCHING_STAT] = REG_FIELD(0x06, 2, 2),
    [REG_TIMEOUT_FLAG] = REG_FIELD(0x06, 1, 1),
    [PIN_DIAG_FAIL_FLAG] = REG_FIELD(0x06, 0, 0),
    /*reg07*/
    [CHG_EN] = REG_FIELD(0x07, 7, 7),
    [REG_RST] = REG_FIELD(0x07, 6, 6),
    [FREQ_SHIFT] = REG_FIELD(0x07, 3, 4),
    [FSW_SET] = REG_FIELD(0x07, 0, 2),
    /*reg08*/
    [SS_TIMEOUT] = REG_FIELD(0x08, 5, 7),
    [REG_TIMEOUT_DIS] = REG_FIELD(0x08, 4, 4),
    [VOUT_OVP_DIS] = REG_FIELD(0x08, 3, 3),
    [SET_IBAT_SNS_RES] = REG_FIELD(0x08, 2, 2),
    [VBUS_PD_EN] = REG_FIELD(0x08, 1, 1),
    [VAC_PD_EN] = REG_FIELD(0x08, 0, 0),
    /*reg09*/
    [MODE] = REG_FIELD(0x09, 7, 7),
    [POR_FLAG] = REG_FIELD(0x09, 6, 6),
    [IBUS_UCP_RISE_FLAG] = REG_FIELD(0x09, 5, 5),
    [IBUS_UCP_RISE_MASK] = REG_FIELD(0x09, 4, 4),
    [WD_TIMEOUT_FLAG] = REG_FIELD(0x09, 3, 3),
    [WD_TIMEOUT] = REG_FIELD(0x09, 0, 2),
    /*reg0A*/
    [VBAT_REG_EN] = REG_FIELD(0x0a, 7, 7),
    [VBATREG_ACTIVE_STAT] = REG_FIELD(0x0a, 4, 4),
    [VBATREG_ACTIVE_FLAG] = REG_FIELD(0x0a, 3, 3),
    [VBATREG_ACTIVE_MASK] = REG_FIELD(0x0a, 2, 2),
    [SET_VBATREG] = REG_FIELD(0x0a, 0, 1),
    /*reg0B*/
    [IBAT_REG_EN] = REG_FIELD(0x0B, 7, 7),
    [IBATREG_ACTIVE_STAT] = REG_FIELD(0x0B, 4, 4),
    [IBATREG_ACTIVE_FLAG] = REG_FIELD(0x0B, 3, 3),
    [IBATREG_ACTIVE_MASK] = REG_FIELD(0x0B, 2, 2),
    [SET_IBATREG] = REG_FIELD(0x0B, 0, 1),
    /*reg0C*/
    [VBUS_UVLO_FALL_MASK] = REG_FIELD(0x0C, 3, 3),
    [VBUS_UVLO_FALL_FLAG] = REG_FIELD(0x0C, 2, 2),
    [FORVE_VAC_OK] = REG_FIELD(0x0C, 1, 1),
    [ACDRV_HI_EN] = REG_FIELD(0x0C, 0, 0),
    /*reg0D*/
    [PMID2OUT_UVP] = REG_FIELD(0x0D, 6, 7),
    [PMID2OUT_OVP] = REG_FIELD(0x0D, 4, 5),
    [PMID2OUT_UVP_FLAG] = REG_FIELD(0x0D, 3, 3),
    [PMID2OUT_OVP_FLAG] = REG_FIELD(0x0D, 2, 2),
    [PMID2OUT_UVP_STAT] = REG_FIELD(0x0D, 1, 1),
    [PMID2OUT_OVP_STAT] = REG_FIELD(0x0D, 0, 0),
    /*reg11*/
    [ADC_EN] = REG_FIELD(0x11, 7, 7),
    [ADC_RATE] = REG_FIELD(0x11, 6, 6),
    [ADC_DONE_STAT] = REG_FIELD(0x11, 2, 2),
    [ADC_DONE_FLAG] = REG_FIELD(0x11, 1, 1),
    [ADC_DONE_MASK] = REG_FIELD(0x11, 0, 0),
    /*reg12*/
    [VBUS_ADC_DIS] = REG_FIELD(0x12, 6, 6),
    [VAC_ADC_DIS] = REG_FIELD(0x12, 5, 5),
    [VOUT_ADC_DIS] = REG_FIELD(0x12, 4, 4),
    [VBAT_ADC_DIS] = REG_FIELD(0x12, 3, 3),
    [IBAT_ADC_DIS] = REG_FIELD(0x12, 2, 2),
    [IBUS_ADC_DIS] = REG_FIELD(0x12, 1, 1),
    [TDIE_ADC_DIS] = REG_FIELD(0x12, 0, 0),
    /*reg36*/
    [DEVICE_ID] = REG_FIELD(0x36, 0, 7),
    /*reg3c*/
    [VBUS_IN_RANGE_DIS] = REG_FIELD(0x3C, 6, 6),
};
static const struct regmap_config sc854x_regmap_config = {
    .reg_bits = 8,
    .val_bits = 8,
    .max_register = SC854x_REGMAX,
};
/************************************************************************/
struct sc854x_cfg {
    int adc_rate;
    int bat_ovp_disable;
    int bat_ocp_disable;
    int vdrop_ovp_disable;
    int bus_ovp_disable;
    int bus_ucp_disable;
    int bus_ocp_disable;
    int bat_ovp_th;
    int bat_ocp_th;
    int ac_ovp_th;
    int bus_ovp_th;
    int bus_ocp_th;
    int sense_r_mohm;
    int reg_timeout_disable;
    int ibat_reg_enabled;
    int vbat_reg_enabled;
    int ibat_reg_th;
    int vbat_reg_th;
    int vdrop_deglitch;
    int vdrop_ovp_th;
};
struct sc854x {
    struct device *dev;
    struct device *sys_dev;
    struct i2c_client *client;
    struct regmap *regmap;
    struct regmap_field *rmap_fields[F_MAX_FIELDS];
    const char *chg_dev_name;
    int mode;
    int irq_gpio;
    int irq;
    struct mutex data_lock;
    struct mutex irq_complete;
    bool irq_waiting;
    bool irq_disabled;
    bool resume_completed;
    bool usb_present;
    bool charge_enabled;    /* Register bit status */
    int vbus_error;
    /* ADC reading */
    int vbat_volt;
    int vbus_volt;
    int vout_volt;
    int vac_volt;
    int ibat_curr;
    int ibus_curr;
    int die_temp;
    struct sc854x_cfg cfg;
    int skip_writes;
    int skip_reads;
    struct sc854x_platform_data *platform_data;
#ifdef CONFIG_MTK_CLASS
    struct charger_device *chg_dev;
#endif /*CONFIG_MTK_CLASS*/
#ifdef CONFIG_SOUTHCHIP_DVCHG_CLASS
    struct dvchg_dev *charger_pump;
#endif /*CONFIG_SOUTHCHIP_DVCHG_CLASS*/
    struct power_supply_desc psy_desc;
    struct power_supply_config psy_cfg;
    struct power_supply *fc2_psy;
};
#ifdef CONFIG_MTK_CLASS
static const struct charger_properties sc854x_chg_props = {
    .alias_name = "sc854x_chg",
};
#endif /*CONFIG_MTK_CLASS*/
/********************COMMON API***********************/
__maybe_unused static u8 val2reg(enum sc854x_reg_range id, u32 val)
{
    int i;
    u8 reg;
    const struct reg_range *range= &sc854x_reg_range[id];
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
__maybe_unused static u32 reg2val(enum sc854x_reg_range id, u8 reg)
{
    const struct reg_range *range= &sc854x_reg_range[id];
    if (!range)
        return reg;
    return range->table ? range->table[reg] :
                range->offset + range->step * reg;
}
/*********************************************************/
static int sc854x_field_read(struct sc854x *sc,
                enum sc854x_fields field_id, int *val)
{
    int ret;
    ret = regmap_field_read(sc->rmap_fields[field_id], val);
    if (ret < 0) {
        dev_err(sc->dev,"sc854x read field %d fail: %d\n", field_id, ret);
    }
    return ret;
}
static int sc854x_field_write(struct sc854x *sc,
                enum sc854x_fields field_id, int val)
{
    int ret;
    ret = regmap_field_write(sc->rmap_fields[field_id], val);
    if (ret < 0) {
        dev_err(sc->dev,"sc854x read field %d fail: %d\n", field_id, ret);
    }
    return ret;
}
static int sc854x_read_block(struct sc854x *sc,
                int reg, uint8_t *val, int len)
{
    int ret;
    ret = regmap_bulk_read(sc->regmap, reg, val, len);
    if (ret < 0) {
        dev_err(sc->dev,"sc854x read %02x block failed %d\n", reg, ret);
    }
    return ret;
}
static int sc854x_write_byte(struct sc854x *sc, int reg, uint8_t val)
{
    int ret;
    ret = regmap_write(sc->regmap, reg, val);
    if (ret < 0) {
        dev_err(sc->dev,"sc854x write %02x failed %d\n", reg, ret);
    }
    return ret;
}
/***********************charger interface start********************************/
__maybe_unused static int sc854x_detect_device(struct sc854x *sc)
{
    int ret;
    int data;
    ret = sc854x_field_read(sc, DEVICE_ID, &data);
    if (ret < 0) {
        dev_err(sc->dev, "%s fail(%d)\n", __func__, ret);
        return ret;
    }
    dev_info(sc->dev,"%s id 0x%2x",__func__,data);
    if (ret == 0) {
        if(data != id_data[SC8545_ID] && data != id_data[SC8546_ID]
            && data != id_data[SC8547_ID] && data != id_data[SC8548_ID]
            && data != id_data[SC8549_ID]) {
            return -ENOMEM;
        }
    }
    return ret;
}
__maybe_unused static int sc854x_reg_reset(struct sc854x *sc)
{
    return sc854x_field_write(sc, REG_RST, 1);
}
__maybe_unused static int sc854x_dump_reg(struct sc854x *sc)
{
    int ret;
    int i;
    int val;
    for (i = 0; i <= 0x3C; i++) {
        ret = regmap_read(sc->regmap, i, &val);
        dev_err(sc->dev,"%s reg[0x%02x] = 0x%02x\n",
                __func__, i, val);
    }
    return ret;
}
__maybe_unused static int sc854x_check_charge_enabled(struct sc854x *sc, bool *enabled)
{
    int ret;
    int val;
    ret = sc854x_field_read(sc, CP_SWITCHING_STAT, &val);
    *enabled = (bool)val;
    dev_info(sc->dev,"%s:%d",__func__,val);
    return ret;
}
__maybe_unused static int sc854x_get_status(struct sc854x *sc, uint32_t *status)
{
    int ret, val;
    *status = 0;
    sc854x_dump_reg(sc);
    ret = sc854x_field_read(sc, VBUS_ERRORHI_STAT, &val);
    if (ret < 0) {
        dev_err(sc->dev, "%s fail to read VBUS_ERRORHI_STAT(%d)\n", __func__, ret);
        return ret;
    }
    if (val != 0)
        *status |= BIT(ERROR_VBUS_HIGH);
    ret = sc854x_field_read(sc, VBUS_ERRORLO_STAT, &val);
    if (ret < 0) {
        dev_err(sc->dev, "%s fail to read VBUS_ERRORLO_STAT(%d)\n", __func__, ret);
        return ret;
    }
    if (val != 0)
        *status |= BIT(ERROR_VBUS_LOW);
    dev_info(sc->dev,"%s (%d)\n", __func__, *status);
    return ret;
}
__maybe_unused static int sc854x_enable_adc(struct sc854x *sc, bool enable)
{
    dev_info(sc->dev,"%s:%d",__func__,enable);
    return sc854x_field_write(sc, ADC_EN, !!enable);
}
__maybe_unused static int sc854x_set_adc_scanrate(struct sc854x *sc, bool oneshot)
{
    dev_info(sc->dev,"%s:%d",__func__,oneshot);
    return sc854x_field_write(sc, ADC_RATE, !!oneshot);
}
__maybe_unused static int sc854x_get_adc_data(struct sc854x *sc, int channel,  int *result)
{
    int ret;
    uint8_t val[2] = {0};
    if(channel >= ADC_MAX_NUM)
        return -EINVAL;
    sc854x_enable_adc(sc, true);
    msleep(20);
    ret = sc854x_read_block(sc, SC854X_REG_13 + (channel << 1), val, 2);
    if (ret < 0)
        return ret;
    *result = (val[1] | (val[0] << 8)) *
                sc854x_adc_m[channel] / sc854x_adc_l[channel];
    dev_info(sc->dev,"%s %d", __func__, *result);
    sc854x_enable_adc(sc, false);
    return ret;
}
__maybe_unused static int sc854x_set_busovp_th(struct sc854x *sc, int threshold)
{
    int reg_val = val2reg(SC854X_VBUS_OVP, threshold);
    dev_info(sc->dev,"%s:%d-%d", __func__, threshold, reg_val);
    return sc854x_field_write(sc, VBUS_OVP, reg_val);
}
__maybe_unused static int sc854x_set_busocp_th(struct sc854x *sc, int threshold)
{
    int reg_val = val2reg(SC854X_IBUS_OCP, threshold);
    dev_info(sc->dev,"%s:%d-%d", __func__, threshold, reg_val);
    return sc854x_field_write(sc, IBUS_OCP, reg_val);
}
__maybe_unused static int sc854x_set_batovp_th(struct sc854x *sc, int threshold)
{
    int reg_val = val2reg(SC854X_VBAT_OVP, threshold);
    dev_info(sc->dev,"%s:%d-%d", __func__, threshold, reg_val);
    return sc854x_field_write(sc, VBAT_OVP, reg_val);
}
__maybe_unused static int sc854x_set_batocp_th(struct sc854x *sc, int threshold)
{
    int reg_val = val2reg(SC854X_IBAT_OCP, threshold);
    dev_info(sc->dev,"%s:%d-%d", __func__, threshold, reg_val);
    return sc854x_field_write(sc, IBAT_OCP, reg_val);
}
__maybe_unused static int sc854x_set_acovp_th(struct sc854x *sc, int threshold)
{
    dev_info(sc->dev,"%s:%d",__func__,threshold);
    if (threshold == SC854X_AC_OVP_6P5V) {
        threshold = 0x07;
        return sc854x_field_write(sc, VAC_OVP, threshold);
    } else if (threshold < SC854X_AC_OVP_MIN)
        threshold = SC854X_AC_OVP_MIN;
    if (threshold > SC854X_AC_OVP_MAX)
        threshold = SC854X_AC_OVP_MAX;
    threshold = (threshold - SC854X_AC_OVP_MIN) / SC854X_AC_OVP_LSB;
    return sc854x_field_write(sc, VAC_OVP, threshold);
}
__maybe_unused static int sc854x_disable_vbus_range(struct sc854x *sc)
{
    return sc854x_field_write(sc, VBUS_IN_RANGE_DIS, 1);
}
__maybe_unused static int sc854x_is_vbuslowerr(struct sc854x *sc, bool *err)
{
    int ret;
    int val;
    ret = sc854x_field_read(sc, VBUS_ERRORLO_STAT, &val);
    dev_info(sc->dev,"%s:%d",__func__,val);
    *err = (bool)val;
    return ret;
}
__maybe_unused static int sc854x_enable_charge(struct sc854x *sc, bool en)
{
    int ret = 0;
    int vbus_value = 0, vout_value = 0, value = 0;
    int vbus_hi = 0, vbus_low = 0;
    dev_info(sc->dev,"%s:%d",__func__,en);
    if (!en) {
        ret |= sc854x_field_write(sc, CHG_EN, !!en);
        return ret;
    } else {
        ret = sc854x_get_adc_data(sc, ADC_VBUS, &vbus_value);
        ret |= sc854x_get_adc_data(sc, ADC_VOUT, &vout_value);
        dev_info(sc->dev,"%s: vbus/vout:%d / %d = %d \r\n", __func__, vbus_value, vout_value, vbus_value*100/vout_value);
        ret |= sc854x_field_read(sc, MODE, &value);
        dev_info(sc->dev,"%s: mode:%d %s \r\n", __func__, value, (value == 0 ?"4:1":(value == 1 ?"2:1":"else")));
        ret |= sc854x_field_read(sc, VBUS_ERRORLO_STAT, &vbus_low);
        ret |= sc854x_field_read(sc, VBUS_ERRORHI_STAT, &vbus_hi);
        dev_info(sc->dev,"%s: high:%d  low:%d \r\n", __func__, vbus_hi, vbus_low);
        ret |= sc854x_field_write(sc, CHG_EN, !!en);
        disable_irq(sc->irq);
        mdelay(300);
        ret |= sc854x_field_read(sc, PIN_DIAG_FAIL_FLAG, &value);
        dev_info(sc->dev,"%s: pin diag fail:%d \r\n", __func__, value);
        ret |= sc854x_field_read(sc, CP_SWITCHING_STAT, &value);
        if (!value) {
            dev_info(sc->dev,"%s:enable fail \r\n", __func__);
            sc854x_dump_reg(sc);
        } else {
            dev_info(sc->dev,"%s:enable success \r\n", __func__);
        }
        enable_irq(sc->irq);
    }
    return ret;
}
__maybe_unused static int sc854x_init_device(struct sc854x *sc)
{
    int ret = 0;
    int i;
    struct {
        enum sc854x_fields field_id;
        int conv_data;
    } props[] = {
        {VBAT_OVP_DIS, sc->cfg.bat_ovp_disable},
        {IBAT_OCP_DIS, sc->cfg.bat_ocp_disable},
        {IBUS_UCP_DIS, sc->cfg.bus_ucp_disable},
        {IBUS_OCP_DIS, sc->cfg.bus_ocp_disable},
        {VBUS_OVP_DIS, sc->cfg.bus_ovp_disable},
        {VDROP_OVP_DIS, sc->cfg.vdrop_ovp_disable},
        {REG_TIMEOUT_DIS, sc->cfg.reg_timeout_disable},
        {IBAT_REG_EN, sc->cfg.ibat_reg_enabled},
        {VBAT_REG_EN, sc->cfg.vbat_reg_enabled},
        {IBAT_OCP, sc->cfg.bat_ocp_th},
        {VBAT_OVP, sc->cfg.bat_ovp_th},
        {IBUS_OCP, sc->cfg.bus_ocp_th},
        {VBUS_OVP, sc->cfg.bus_ovp_th},
        {VAC_OVP, sc->cfg.ac_ovp_th},
        {SET_IBAT_SNS_RES, sc->cfg.sense_r_mohm},
        {SET_VBATREG, sc->cfg.ibat_reg_th},
        {SET_IBATREG, sc->cfg.vbat_reg_th},
        {VDROP_OVP_THRESHOLD_SET, sc->cfg.vdrop_deglitch},
        {VDROP_DEGLITCH_SET, sc->cfg.vdrop_ovp_th},
    };
    // ret = sc854x_reg_reset(sc);
    // if (ret < 0) {
    //     dev_err(sc->dev,"%s Failed to reset registers(%d)\n", __func__, ret);
    // }
    msleep(10);
    for (i = 0; i < ARRAY_SIZE(props); i++) {
        dev_info(sc->dev,"%s (%d)\n", __func__, props[i].conv_data);
        ret = sc854x_field_write(sc, props[i].field_id, props[i].conv_data);
    }
    /*if (sc->mode == SLAVE) {
        sc854x_disable_vbus_range(sc);
    }*/
    sc854x_dump_reg(sc);
    return ret;
}
/*********************mtk charger interface start**********************************/
#ifdef CONFIG_MTK_CLASS
static inline int to_sc854x_adc(enum adc_channel chan)
{
    switch (chan) {
    case ADC_CHANNEL_VBUS:
        return ADC_VBUS;
    case ADC_CHANNEL_VBAT:
        return ADC_VBAT;
    case ADC_CHANNEL_IBUS:
        return ADC_IBUS;
    case ADC_CHANNEL_IBAT:
        return ADC_IBAT;
    case ADC_CHANNEL_TEMP_JC:
        return ADC_TDIE;
    case ADC_CHANNEL_VOUT:
        return ADC_VOUT;
    default:
        break;
    }
    return ADC_MAX_NUM;
}
static int mtk_sc854x_set_vbatovp(struct charger_device *chg_dev, u32 uV)
{
    struct sc854x *sc = charger_get_data(chg_dev);
    int ret;
    ret = sc854x_set_batovp_th(sc, uV/1000);
    if (ret < 0)
        return ret;
    return ret;
}
static int mtk_sc854x_set_ibatocp(struct charger_device *chg_dev, u32 uA)
{
    struct sc854x *sc = charger_get_data(chg_dev);
    int ret;
    ret = sc854x_set_batocp_th(sc, uA/1000);
    if (ret < 0)
        return ret;
    return ret;
}
static int mtk_sc854x_set_vbusovp(struct charger_device *chg_dev, u32 uV)
{
    struct sc854x *sc = charger_get_data(chg_dev);
    int mv;
    mv = uV / 1000;
    return sc854x_set_busovp_th(sc, mv);
}
static int mtk_sc854x_set_ibusocp(struct charger_device *chg_dev, u32 uA)
{
    struct sc854x *sc = charger_get_data(chg_dev);
    int ma;
    ma = uA / 1000;
    return sc854x_set_busocp_th(sc, ma);
}
static int mtk_sc854x_enable_chg(struct charger_device *chg_dev, bool en)
{
    struct sc854x *sc = charger_get_data(chg_dev);
    int ret;
    ret = sc854x_enable_charge(sc,en);
    return ret;
}
static int mtk_sc854x_is_chg_enabled(struct charger_device *chg_dev, bool *en)
{
    struct sc854x *sc = charger_get_data(chg_dev);
    int ret;
    ret = sc854x_check_charge_enabled(sc, en);
    return ret;
}
static int mtk_sc854x_get_adc(struct charger_device *chg_dev, enum adc_channel chan,
            int *min, int *max)
{
    struct sc854x *sc = charger_get_data(chg_dev);
    sc854x_get_adc_data(sc, to_sc854x_adc(chan), max);
    if(chan != ADC_CHANNEL_TEMP_JC)
        *max = *max * 1000;
    if (min != max)
        *min = *max;
    return 0;
}
static int mtk_sc854x_get_adc_accuracy(struct charger_device *chg_dev,
                enum adc_channel chan, int *min, int *max)
{
    *min = *max = sc854x_adc_accuracy_tbl[to_sc854x_adc(chan)];
    return 0;
}
static int mtk_sc854x_is_vbuslowerr(struct charger_device *chg_dev, bool *err)
{
    struct sc854x *sc = charger_get_data(chg_dev);
    return sc854x_is_vbuslowerr(sc,err);
}
static int mtk_sc854x_set_vbatovp_alarm(struct charger_device *chg_dev, u32 uV)
{
    struct sc854x *sc = charger_get_data(chg_dev);
    dev_info(sc->dev,"%s",__func__);
    return 0;
}
static int mtk_sc854x_reset_vbatovp_alarm(struct charger_device *chg_dev)
{
    struct sc854x *sc = charger_get_data(chg_dev);
    dev_info(sc->dev,"%s",__func__);
    return 0;
}
static int mtk_sc854x_set_vbusovp_alarm(struct charger_device *chg_dev, u32 uV)
{
    struct sc854x *sc = charger_get_data(chg_dev);
    dev_info(sc->dev,"%s",__func__);
    return 0;
}
static int mtk_sc854x_reset_vbusovp_alarm(struct charger_device *chg_dev)
{
    struct sc854x *sc = charger_get_data(chg_dev);
    dev_info(sc->dev,"%s",__func__);
    return 0;
}
static int mtk_sc854x_init_chip(struct charger_device *chg_dev)
{
    struct sc854x *sc = charger_get_data(chg_dev);
    return sc854x_init_device(sc);
}
static const struct charger_ops sc854x_chg_ops = {
    .enable = mtk_sc854x_enable_chg,
    .is_enabled = mtk_sc854x_is_chg_enabled,
    .get_adc = mtk_sc854x_get_adc,
    .get_adc_accuracy = mtk_sc854x_get_adc_accuracy,
    .set_vbusovp = mtk_sc854x_set_vbusovp,
    .set_ibusocp = mtk_sc854x_set_ibusocp,
    .set_vbatovp = mtk_sc854x_set_vbatovp,
    .set_ibatocp = mtk_sc854x_set_ibatocp,
    .init_chip = mtk_sc854x_init_chip,
    .is_vbuslowerr = mtk_sc854x_is_vbuslowerr,
    .set_vbatovp_alarm = mtk_sc854x_set_vbatovp_alarm,
    .reset_vbatovp_alarm = mtk_sc854x_reset_vbatovp_alarm,
    .set_vbusovp_alarm = mtk_sc854x_set_vbusovp_alarm,
    .reset_vbusovp_alarm = mtk_sc854x_reset_vbusovp_alarm,
};
#endif /*CONFIG_MTK_CLASS*/
/********************mtk charger interface end*************************************************/
#ifdef CONFIG_SOUTHCHIP_DVCHG_CLASS
static inline int sc_to_sc854x_adc(enum sc_adc_channel chan)
{
    switch (chan) {
    case SC_ADC_VBUS:
        return ADC_VBUS;
    case SC_ADC_VBAT:
        return ADC_VBAT;
    case SC_ADC_IBUS:
        return ADC_IBUS;
    case SC_ADC_IBAT:
        return ADC_IBAT;
    case SC_ADC_TDIE:
        return ADC_TDIE;
    default:
        break;
    }
    return ADC_MAX_NUM;
}
static int sc_sc854x_set_enable(struct dvchg_dev *charger_pump, bool enable)
{
    struct sc854x *sc = dvchg_get_private(charger_pump);
    int ret;
    ret = sc854x_enable_charge(sc,enable);
    return ret;
}
static int sc_sc854x_get_is_enable(struct dvchg_dev *charger_pump, bool *enable)
{
    struct sc854x *sc = dvchg_get_private(charger_pump);
    int ret;
    ret = sc854x_check_charge_enabled(sc, enable);
    return ret;
}
static int sc_sc854x_get_status(struct dvchg_dev *charger_pump, uint32_t *status)
{
    struct sc854x *sc = dvchg_get_private(charger_pump);
    int ret = 0;
    ret = sc854x_get_status(sc, status);
    return ret;
}
static int sc_sc854x_get_adc_value(struct dvchg_dev *charger_pump, enum sc_adc_channel ch, int *value)
{
    struct sc854x *sc = dvchg_get_private(charger_pump);
    int ret = 0;
    ret = sc854x_get_adc_data(sc, sc_to_sc854x_adc(ch), value);
    return ret;
}
static struct dvchg_ops sc_sc854x_dvchg_ops = {
    .set_enable = sc_sc854x_set_enable,
    .get_status = sc_sc854x_get_status,
    .get_is_enable = sc_sc854x_get_is_enable,
    .get_adc_value = sc_sc854x_get_adc_value,
};
#endif /*CONFIG_SOUTHCHIP_DVCHG_CLASS*/
/********************creat devices note start*************************************************/
static ssize_t sc854x_show_registers(struct device *dev,
                struct device_attribute *attr, char *buf)
{
    struct sc854x *sc = dev_get_drvdata(dev);
    u8 addr, val, tmpbuf[300];
    int len, idx, ret;
    idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sc854x");
    for (addr = 0x0; addr <= 0x3C; addr++) {
        if((addr < 0x24) || (addr > 0x2B && addr < 0x33)
            || addr == 0x36 || addr == 0x3C) {
            ret = sc854x_read_block(sc, addr, &val, 1);
            if (ret == 0) {
                len = snprintf(tmpbuf, PAGE_SIZE - idx,
                        "Reg[%.2X] = 0x%.2x\n", addr, val);
                memcpy(&buf[idx], tmpbuf, len);
                idx += len;
            }
        }
    }
    return idx;
}
static ssize_t sc854x_store_register(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct sc854x *sc = dev_get_drvdata(dev);
    int ret;
    unsigned int reg, val;
    ret = sscanf(buf, "%x %x", &reg, &val);
    if (ret == 2 && reg <= 0x3C)
        sc854x_write_byte(sc, (unsigned char)reg, (unsigned char)val);
    return count;
}
static DEVICE_ATTR(registers, 0660, sc854x_show_registers, sc854x_store_register);
static void sc854x_create_device_node(struct device *dev)
{
    device_create_file(dev, &dev_attr_registers);
}
/********************creat devices note end*************************************************/
/*
* interrupt does nothing, just info event chagne, other module could get info
* through power supply interface
*/
#ifdef CONFIG_MTK_CLASS
static inline int status_reg_to_charger(enum sc854x_notify notify)
{
    switch (notify) {
    case SC854X_NOTIFY_IBUSUCPF:
        return CHARGER_DEV_NOTIFY_IBUSUCP_FALL;
    case SC854X_NOTIFY_VBUSOVPALM:
        return CHARGER_DEV_NOTIFY_VBUSOVP_ALARM;
    case SC854X_NOTIFY_VBATOVPALM:
        return CHARGER_DEV_NOTIFY_VBATOVP_ALARM;
    case SC854X_NOTIFY_IBUSOCP:
        return CHARGER_DEV_NOTIFY_IBUSOCP;
    case SC854X_NOTIFY_VBUSOVP:
        return CHARGER_DEV_NOTIFY_VBUS_OVP;
    case SC854X_NOTIFY_IBATOCP:
        return CHARGER_DEV_NOTIFY_IBATOCP;
    case SC854X_NOTIFY_VBATOVP:
        return CHARGER_DEV_NOTIFY_BAT_OVP;
    case SC854X_NOTIFY_VOUTOVP:
        return CHARGER_DEV_NOTIFY_VOUTOVP;
    default:
        return -EINVAL;
        break;
    }
    return -EINVAL;
}
#endif /*CONFIG_MTK_CLASS*/
__maybe_unused
static void sc854x_dump_check_fault_status(struct sc854x *sc)
{
    int ret;
    u8 flag = 0;
    int i,j,k;
#ifdef CONFIG_MTK_CLASS
    int noti;
#endif /*CONFIG_MTK_CLASS*/
    for (i = 0; i <= 0x3c; i++) {
        ret = sc854x_read_block(sc, i, &flag, 1);
        if (ret){
			pr_err("%s  sc854x_read_block fail\n",__func__);
		}
        dev_err(sc->dev, "%s reg[0x%02x] = 0x%02x\n", __func__, i, flag);
        for (k=0; k < ARRAY_SIZE(cp_intr_flag); k++) {
            if (cp_intr_flag[k].reg == i){
                for (j=0; j <  cp_intr_flag[k].len; j++) {
                    if (flag & cp_intr_flag[k].bit[j].mask) {
                        dev_err(sc->dev,"trigger :%s\n",cp_intr_flag[k].bit[j].name);
        #ifdef CONFIG_MTK_CLASS
                        noti = status_reg_to_charger(cp_intr_flag[k].bit[j].notify);
                        if(noti >= 0) {
                            charger_dev_notify(sc->chg_dev, noti);
                        }
        #endif /*CONFIG_MTK_CLASS*/
                    }
                }
            }
        }
    }
}
static irqreturn_t sc854x_charger_interrupt(int irq, void *dev_id)
{
    struct sc854x *sc = dev_id;
    dev_err(sc->dev,"INT OCCURED\n");
#if 1
    mutex_lock(&sc->irq_complete);
    sc->irq_waiting = true;
    if (!sc->resume_completed) {
        dev_dbg(sc->dev, "IRQ triggered before device-resume\n");
        if (!sc->irq_disabled) {
            disable_irq_nosync(irq);
            sc->irq_disabled = true;
        }
        mutex_unlock(&sc->irq_complete);
        return IRQ_HANDLED;
    }
    sc->irq_waiting = false;
#ifdef CONFIG_SC854X_PROTOCOL
    sc854x_protocol_irq(sc->dev);
#endif /*CONFIG_SC854X_PROTOCOL*/
    sc854x_dump_check_fault_status(sc);
    mutex_unlock(&sc->irq_complete);
#endif
    power_supply_changed(sc->fc2_psy);
    return IRQ_HANDLED;
}
static int sc854x_irq_register(struct sc854x *sc)
{
    int ret;
    if (gpio_is_valid(sc->irq_gpio)) {
        ret = gpio_request_one(sc->irq_gpio, GPIOF_DIR_IN,"sc854x_irq");
        if (ret) {
            dev_err(sc->dev,"failed to request sc854x_irq\n");
            return -EINVAL;
        }
        sc->irq = gpio_to_irq(sc->irq_gpio);
        if (sc->irq < 0) {
            dev_err(sc->dev,"failed to gpio_to_irq\n");
            return -EINVAL;
        }
    } else {
        dev_err(sc->dev,"irq gpio not provided\n");
        return -EINVAL;
    }
    if (sc->irq) {
        ret = devm_request_threaded_irq(&sc->client->dev, sc->irq,
                NULL, sc854x_charger_interrupt,
                IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                sc854x_irq_name[sc->mode], sc);
        if (ret < 0) {
            dev_err(sc->dev,"request irq for irq=%d failed, ret =%d\n",
                            sc->irq, ret);
            return ret;
        }
        enable_irq_wake(sc->irq);
    }
    return ret;
}
/********************interrupte end*************************************************/
/************************psy start**************************************/
static enum power_supply_property sc854x_charger_props[] = {
    POWER_SUPPLY_PROP_ONLINE,
    POWER_SUPPLY_PROP_VOLTAGE_NOW,
    POWER_SUPPLY_PROP_CURRENT_NOW,
    POWER_SUPPLY_PROP_TEMP,
};
static int sc854x_charger_get_property(struct power_supply *psy,
                enum power_supply_property psp,
                union power_supply_propval *val)
{
    struct sc854x *sc = power_supply_get_drvdata(psy);
    int result;
    int ret;
    switch (psp) {
    case POWER_SUPPLY_PROP_ONLINE:
        sc854x_check_charge_enabled(sc, &sc->charge_enabled);
        val->intval = sc->charge_enabled;
        break;
    case POWER_SUPPLY_PROP_VOLTAGE_NOW:
        ret = sc854x_get_adc_data(sc, ADC_VBUS, &result);
        if (!ret)
            sc->vbus_volt = result;
        val->intval = sc->vbus_volt;
        break;
    case POWER_SUPPLY_PROP_CURRENT_NOW:
        ret = sc854x_get_adc_data(sc, ADC_IBUS, &result);
        if (!ret)
            sc->ibus_curr = result;
        val->intval = sc->ibus_curr;
        break;
    case POWER_SUPPLY_PROP_TEMP:
        ret = sc854x_get_adc_data(sc, ADC_TDIE, &result);
        if (!ret)
            sc->die_temp = result;
        val->intval = sc->die_temp;
        break;
    default:
        return -EINVAL;
    }
    return 0;
}
static int sc854x_charger_set_property(struct power_supply *psy,
                    enum power_supply_property prop,
                    const union power_supply_propval *val)
{
    struct sc854x *sc = power_supply_get_drvdata(psy);
    dev_err(sc->dev,"prop = %d\n",  prop);
    switch (prop) {
    case POWER_SUPPLY_PROP_ONLINE:
        sc854x_enable_charge(sc, val->intval);
        sc854x_check_charge_enabled(sc, &sc->charge_enabled);
        dev_err(sc->dev,"POWER_SUPPLY_PROP_ONLINE: %s\n",
                val->intval ? "enable" : "disable");
        break;
    default:
        return -EINVAL;
    }
    return 0;
}
static int sc854x_charger_is_writeable(struct power_supply *psy,
                    enum power_supply_property prop)
{
    int ret;
    switch (prop) {
    case POWER_SUPPLY_PROP_ONLINE:
        ret = 1;
        break;
    default:
        ret = 0;
        break;
    }
    return ret;
}
static int sc854x_psy_register(struct sc854x *sc)
{
    sc->psy_cfg.drv_data = sc;
    sc->psy_cfg.of_node = sc->dev->of_node;
    sc->psy_desc.name = sc854x_psy_name[sc->mode];
    sc->psy_desc.type = POWER_SUPPLY_TYPE_MAINS;
    sc->psy_desc.properties = sc854x_charger_props;
    sc->psy_desc.num_properties = ARRAY_SIZE(sc854x_charger_props);
    sc->psy_desc.get_property = sc854x_charger_get_property;
    sc->psy_desc.set_property = sc854x_charger_set_property;
    sc->psy_desc.property_is_writeable = sc854x_charger_is_writeable;
    sc->fc2_psy = devm_power_supply_register(sc->dev,
            &sc->psy_desc, &sc->psy_cfg);
    if (IS_ERR(sc->fc2_psy)) {
        dev_err(sc->dev,"failed to register fc2_psy\n");
        return PTR_ERR(sc->fc2_psy);
    }
    dev_err(sc->dev,"%s power supply register successfully\n", sc->psy_desc.name);
    return 0;
}
/************************psy end**************************************/
static int sc854x_set_work_mode(struct sc854x *sc, int mode)
{
    switch (mode)
    {
    case SC854X_STANDALONG:
        sc->mode = STANDALONE;
        break;
    case SC854X_MASTER:
        sc->mode = MASTER;
        break;
    case SC854X_SLAVE:
        sc->mode = SLAVE;
        break;
    default:
        dev_err(sc->dev,"Not find work mode\n");
        return -ENODEV;
    }
    dev_err(sc->dev,"work mode is %s\n", sc->mode == STANDALONE 
        ? "standalone" : (sc->mode == MASTER ? "master" : "slave"));
    return 0;
}
static int sc854x_parse_dt(struct sc854x *sc, struct device *dev)
{
    struct device_node *np = dev->of_node;
    int i;
    int ret;
    struct {
        char *name;
        int *conv_data;
    } props[] = {
        {"sc,sc854x,bat-ovp-disable", &(sc->cfg.bat_ovp_disable)},
        {"sc,sc854x,bat-ocp-disable", &(sc->cfg.bat_ocp_disable)},
        {"sc,sc854x,vdrop-ovp-disable", &(sc->cfg.vdrop_ovp_disable)},
        {"sc,sc854x,bus-ovp-disable", &(sc->cfg.bus_ovp_disable)},
        {"sc,sc854x,bus-ucp-disable", &(sc->cfg.bus_ucp_disable)},
        {"sc,sc854x,bus-ocp-disable", &(sc->cfg.bus_ocp_disable)},
        {"sc,sc854x,reg-timeout-disable", &(sc->cfg.reg_timeout_disable)},
        {"sc,sc854x,ibat-regulation-enable", &(sc->cfg.ibat_reg_enabled)},
        {"sc,sc854x,vbat-regulation-enable", &(sc->cfg.vbat_reg_enabled)},
        {"sc,sc854x,bat-ovp-threshold", &(sc->cfg.bat_ovp_th)},
        {"sc,sc854x,bat-ocp-threshold", &(sc->cfg.bat_ocp_th)},
        {"sc,sc854x,ac-ovp-threshold", &(sc->cfg.ac_ovp_th)},
        {"sc,sc854x,bus-ovp-threshold", &(sc->cfg.bus_ovp_th)},
        {"sc,sc854x,bus-ocp-threshold", &(sc->cfg.bus_ocp_th)},
        {"sc,sc854x,sense-resistor-mohm", &(sc->cfg.sense_r_mohm)},
        {"sc,sc854x,ibat-regulation-threshold", &(sc->cfg.ibat_reg_th)},
        {"sc,sc854x,vbat-regulation-threshold", &(sc->cfg.vbat_reg_th)},
        {"sc,sc854x,vdrop-deglitch", &(sc->cfg.vdrop_deglitch)},
        {"sc,sc854x,vdrop-ovp-threshold", &(sc->cfg.vdrop_ovp_th)},
        {"sc,sc854x,adc-rate", &(sc->cfg.adc_rate)},
    };
    sc->irq_gpio = of_get_named_gpio(np, "sc,sc854x,irq-gpio", 0);
    if (!gpio_is_valid(sc->irq_gpio)) {
        dev_err(sc->dev,"fail to valid gpio : %d\n", sc->irq_gpio);
        return -EINVAL;
    }
    np = of_find_node_by_name(dev->of_node, "charger");
    if (!np) {
        dev_err(sc->dev, "not fine charger node\n");
        return -EINVAL;
    }
#ifdef CONFIG_MTK_CHARGER_V5P10
    if (of_property_read_string(np, "charger_name", &sc->chg_dev_name) < 0) {
        sc->chg_dev_name = "charger";
        dev_err(sc->dev,"no charger name\n");
    }
#elif defined(CONFIG_MTK_CHARGER_V4P19)
    if (of_property_read_string(np, "charger_name_v4_19", &sc->chg_dev_name) < 0) {
        sc->chg_dev_name = "charger";
        dev_err(sc->dev,"no charger name\n");
    }
#endif /*CONFIG_MTK_CHARGER_V4P19*/
    /* initialize data for optional properties */
    for (i = 0; i < ARRAY_SIZE(props); i++) {
        ret = device_property_read_u32(dev, props[i].name,
                        props[i].conv_data);
        if (ret < 0) {
            dev_err(sc->dev, "can not read %s \n", props[i].name);
            return ret;
        }
    }
    return 0;
}
static struct of_device_id sc854x_charger_match_table[] = {
    {   .compatible = "sc,sc854x-standalone",
        .data = &sc854x_mode_data[SC854X_STANDALONG],},
    {   .compatible = "sc,sc854x-master",
        .data = &sc854x_mode_data[SC854X_MASTER],},
    {   .compatible = "sc,sc854x-slave",
        .data = &sc854x_mode_data[SC854X_SLAVE],},
    {},
};
static void sc854x_set_reserv_option(struct sc854x *sc)
{
    sc854x_write_byte(sc, 0xED, 0x45);
    sc854x_write_byte(sc, 0xED, 0x66);
    sc854x_write_byte(sc, 0xED, 0x46);
    sc854x_write_byte(sc, 0xED, 0x4C);
    sc854x_write_byte(sc, 0xE1, 0x10);
    sc854x_write_byte(sc, 0xED, 0xFF);
}
static int sc854x_charger_probe(struct i2c_client *client)
{
    struct sc854x *sc;
    const struct of_device_id *match;
    struct device_node *node = client->dev.of_node;
    int ret, i;
    pr_info("[%s]\n", __func__);
    dev_err(&client->dev, "%s (%s)\n", __func__, SC854x_DRV_VERSION);
    sc = devm_kzalloc(&client->dev, sizeof(struct sc854x), GFP_KERNEL);
    if (!sc) {
        ret = -ENOMEM;
        goto err_kzalloc;
    }
    sc->dev = &client->dev;
    sc->client = client;
    sc->regmap = devm_regmap_init_i2c(client,
                            &sc854x_regmap_config);
    if (IS_ERR(sc->regmap)) {
        dev_err(sc->dev,"Failed to initialize regmap\n");
        ret = PTR_ERR(sc->regmap);
        goto err_regmap_init;
    }
    for (i = 0; i < ARRAY_SIZE(sc854x_reg_fields); i++) {
        const struct reg_field *reg_fields = sc854x_reg_fields;
        sc->rmap_fields[i] =
            devm_regmap_field_alloc(sc->dev,
                        sc->regmap,
                        reg_fields[i]);
        if (IS_ERR(sc->rmap_fields[i])) {
            dev_err(sc->dev,"cannot allocate regmap field\n");
            ret = PTR_ERR(sc->rmap_fields[i]);
            goto err_regmap_field;
        }
    }
    mutex_init(&sc->data_lock);
    mutex_init(&sc->irq_complete);
    sc->resume_completed = true;
    sc->irq_waiting = false;
    ret = sc854x_detect_device(sc);
    if (ret) {
        dev_err(sc->dev,"No sc854x device found!\n");
        goto err_detect_dev;
    }
    i2c_set_clientdata(client, sc);
    sc854x_create_device_node(&(client->dev));
    match = of_match_node(sc854x_charger_match_table, node);
    if (match == NULL) {
        dev_err(sc->dev,"device tree match not found!\n");
        goto err_match_node;
    }
    sc854x_set_work_mode(sc, *(int *)match->data);
    if (ret) {
        dev_err(sc->dev,"Fail to set work mode!\n");
        goto err_set_mode;
    }
    ret = sc854x_parse_dt(sc, &client->dev);
    if (ret) {
        dev_err(sc->dev,"Fail to parse dt!\n");
        goto err_parse_dt;
    }
    ret = sc854x_init_device(sc);
    if (ret < 0) {
        dev_err(sc->dev, "%s init device failed(%d)\n", __func__, ret);
        goto err_init_device;
    }
    sc854x_reg_reset(sc);
    sc854x_set_reserv_option(sc);
    ret = sc854x_psy_register(sc);
    if (ret) {
        dev_err(sc->dev,"Fail to register psy!\n");
        goto err_register_psy;
    }
#ifdef CONFIG_SC854X_PROTOCOL
    ret = sc854x_protocol_init(sc->dev, sc->regmap);
    if (ret) {
        dev_err(sc->dev,"Fail to init protocol!\n");
    }
#endif /*CONFIG_SC854X_PROTOCOL*/
    ret = sc854x_irq_register(sc);
    if (ret < 0) {
        dev_err(sc->dev,"Fail to register irq!\n");
        goto err_register_irq;
    }
#ifdef CONFIG_MTK_CHARGER_V5P10
    sc->chg_dev = charger_device_register(sc->chg_dev_name,
                        &client->dev, sc,
                        &sc854x_chg_ops,
                        &sc854x_chg_props);
    if (IS_ERR_OR_NULL(sc->chg_dev)) {
        ret = PTR_ERR(sc->chg_dev);
        dev_err(sc->dev,"Fail to register charger!\n");
        goto err_register_charger;
    }
#elif defined(CONFIG_MTK_CHARGER_V4P19)
    sc->chg_dev = charger_device_register(sc->chg_dev_name,
                        &client->dev, sc,
                        &sc854x_chg_ops,
                        &sc854x_chg_props);
    if (IS_ERR_OR_NULL(sc->chg_dev)) {
        ret = PTR_ERR(sc->chg_dev);
        dev_err(sc->dev,"Fail to register charger!\n");
        goto err_register_charger;
    }
#endif
#ifdef CONFIG_SOUTHCHIP_DVCHG_CLASS
    sc->charger_pump = dvchg_register("sc_dvchg",
                            sc->dev, &sc_sc854x_dvchg_ops, sc);
    if (IS_ERR_OR_NULL(sc->charger_pump)) {
        ret = PTR_ERR(sc->charger_pump);
        dev_err(sc->dev,"Fail to register charger!\n");
        goto err_register_sc_charger;
    }
#endif /* CONFIG_SOUTHCHIP_DVCHG_CLASS */
    device_init_wakeup(sc->dev, 1);
    dev_err(sc->dev,"sc854x probe successfully!\n");
    return 0;
err_register_psy:
err_register_irq:
#ifdef CONFIG_MTK_CLASS
err_register_charger:
#endif /*CONFIG_MTK_CLASS*/
#ifdef CONFIG_SOUTHCHIP_DVCHG_CLASS
err_register_sc_charger:
#endif /*CONFIG_SOUTHCHIP_DVCHG_CLASS*/
err_init_device:
    power_supply_unregister(sc->fc2_psy);
err_detect_dev:
err_match_node:
err_set_mode:
err_parse_dt:
    mutex_destroy(&sc->data_lock);
    mutex_destroy(&sc->irq_complete);
err_regmap_init:
err_regmap_field:
    devm_kfree(&client->dev, sc);
err_kzalloc:
    dev_err(&client->dev,"sc854x probe fail\n");
    return ret;
}
#ifdef CONFIG_PM_SLEEP
static inline bool is_device_suspended(struct sc854x *sc)
{
    return !sc->resume_completed;
}
static int sc854x_suspend(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct sc854x *sc = i2c_get_clientdata(client);
    mutex_lock(&sc->irq_complete);
    sc->resume_completed = false;
    mutex_unlock(&sc->irq_complete);
    dev_err(sc->dev,"Suspend successfully!");
    return 0;
}
static int sc854x_suspend_noirq(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct sc854x *sc = i2c_get_clientdata(client);
    if (sc->irq_waiting) {
        pr_err_ratelimited("Aborting suspend, an interrupt was detected while suspending\n");
        return -EBUSY;
    }
    return 0;
}
static int sc854x_resume(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct sc854x *sc = i2c_get_clientdata(client);
    mutex_lock(&sc->irq_complete);
    sc->resume_completed = true;
    if (sc->irq_waiting) {
        sc->irq_disabled = false;
        enable_irq(client->irq);
        mutex_unlock(&sc->irq_complete);
        sc854x_charger_interrupt(client->irq, sc);
    } else {
        mutex_unlock(&sc->irq_complete);
    }
    power_supply_changed(sc->fc2_psy);
    dev_err(sc->dev,"Resume successfully!");
    return 0;
}
static const struct dev_pm_ops sc854x_pm_ops = {
    .resume         = sc854x_resume,
    .suspend_noirq  = sc854x_suspend_noirq,
    .suspend        = sc854x_suspend,
};
#endif /*CONFIG_PM_SLEEP*/
static void sc854x_charger_remove(struct i2c_client *client)
{
    struct sc854x *sc = i2c_get_clientdata(client);
#ifdef CONFIG_SC854X_PROTOCOL
    sc854x_protocol_deinit(sc->dev, sc->regmap);
#endif /*CONFIG_SC854X_PROTOCOL*/
    power_supply_unregister(sc->fc2_psy);
    mutex_destroy(&sc->data_lock);
    mutex_destroy(&sc->irq_complete);
    devm_kfree(&client->dev, sc);
}
static void sc854x_charger_shutdown(struct i2c_client *client)
{
    struct sc854x *sc = i2c_get_clientdata(client);
#ifdef CONFIG_SC854X_PROTOCOL
    sc854x_protocol_deinit(sc->dev, sc->regmap);
#endif /*CONFIG_SC854X_PROTOCOL*/
    power_supply_unregister(sc->fc2_psy);
    mutex_destroy(&sc->data_lock);
    mutex_destroy(&sc->irq_complete);
    devm_kfree(&client->dev, sc);
}
static struct i2c_driver sc854x_charger_driver = {
    .driver     = {
        .name   = "sc854x-charger",
        .owner  = THIS_MODULE,
        .of_match_table = sc854x_charger_match_table,
#ifdef CONFIG_PM_SLEEP
        .pm    = &sc854x_pm_ops,
#endif /*CONFIG_PM_SLEEP*/
    },
    .probe      = sc854x_charger_probe,
    .remove     = sc854x_charger_remove,
    .shutdown   = sc854x_charger_shutdown,
};
module_i2c_driver(sc854x_charger_driver);
MODULE_DESCRIPTION("SC SC854X Charge Pump Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("South Chip <Aiden-yu@southchip.com>");
