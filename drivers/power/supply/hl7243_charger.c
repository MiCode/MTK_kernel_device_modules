// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2024 Halo Microelectronice Co., Ltd.
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
#include <linux/mca/common/mca_log.h>
#include <linux/mca/common/mca_event.h>
#include <linux/mca/common/mca_charge_mievent.h>
#define CONFIG_MTK_CLASS 1
#define CONFIG_MTK_CHARGER_V5P10 1

#ifdef CONFIG_MTK_CLASS
#include "mtk_charger.h"
#include "charger_class.h"

#ifdef CONFIG_MTK_CHARGER_V4P19
#include "mtk_charger_intf.h"
#endif /*CONFIG_MTK_CHARGER_V4P19*/

#endif /*CONFIG_MTK_CLASS*/


#define HL7243_DRV_VERSION              "1.0.1_G"
#define HL7243_SS_TIMEOUT_DISABLE	0
#define HL7243_SS_TIMEOUT_20MS		1
#define HL7243_SS_TIMEOUT_100MS		2
#define HL7243_SS_TIMEOUT_320MS		3
#define HL7243_SS_TIMEOUT_1280MS	4
#define HL7243_SS_TIMEOUT_5120MS	5
#define HL7243_SS_TIMEOUT_20480MS	6
#define HL7243_SS_TIMEOUT_81920MS	7
#define CP_FORWARD_4_TO_1             0
#define CP_FORWARD_2_TO_1             1
#define CP_FORWARD_1_TO_1             2

/********i2c basic read/write interface***********/
// static int cp_read_word(struct i2c_client *client, u8 reg, u16 *val)
// {
// 	s32 ret;
// 	ret = i2c_smbus_read_word_data(client, reg);

// 	if (ret < 0) {
// 		dev_err(&client->dev, "i2c read word fail: can't read from reg 0x%02X, errcode=%d\n", reg, ret);
// 		return ret;
// 	}
// 	*val = (u16)ret;

// 	return 0;
// }

#ifdef DEBUG_CODE
static int cp_write_word(struct i2c_client *client, u8 reg, u16 val)
{
    s32 ret;
    ret = i2c_smbus_write_word_data(client, reg, val);

    if (ret < 0) {
        dev_err(&client->dev, "i2c write word fail: can't write to reg 0x%02X\n", reg);
        return ret;
    }

    return 0;
}
#endif

// static int cp_read_byte(struct i2c_client *client, u8 reg, u8 *val)
// {
// 	s32 ret;
// 	ret = i2c_smbus_read_byte_data(client, reg);

// 	if (ret < 0) {
// 		dev_err(&client->dev, "i2c read byte fail: can't read from reg 0x%02X, errcode=%d\n", reg, ret);
// 		return ret;
// 	}
// 	*val = (u8)ret;

// 	return 0;
// }

// static int cp_write_byte(struct i2c_client *client, u8 reg, u8 val)
// {
// 	s32 ret;
// 	ret = i2c_smbus_write_byte_data(client, reg, val);

// 	if (ret < 0) {
// 		dev_err(&client->dev, "i2c write byte fail: can't write to reg 0x%02X, errcode=%d\n", reg, ret);
// 		return ret;
// 	}

// 	return 0;
// }

#ifdef DEBUG_CODE
static int cp_read_block(struct i2c_client *client, u8 reg, u8 *buf, int len)
{
    int ret;
    int i;

    for (i = 0; i < len; i++) {
        ret = i2c_smbus_read_byte_data(client, reg + i);
        if (ret < 0) {
            dev_err(&client->dev, "i2c read reg 0x%02X faild\n", reg + i);
            return ret;
        }
        buf[i] = ret;
    }

    return 0;
}

static int cp_write_block(struct i2c_client *client, u8 reg, u8 *buf, int len)
{
    int ret;
    int i;

    for (i = 0; i < len; i++) {
        ret = i2c_smbus_write_byte_data(client, reg + i, buf[i]);
        if (ret < 0) {
            dev_err(&client->dev, "i2c write reg 0x%02X faild\n", reg + i);
            return ret;
        }
    }

    return 0;
}
#endif

// static int cp_read_i2c_block_data(struct i2c_client *client, u8 reg, unsigned short len, u8 *buf)
// {
// 	int ret;

// 	if (!client || !buf) {
// 		dev_err(&client->dev, "null pointer read\n");
// 		return -EINVAL;
// 	}

// 	ret = i2c_smbus_read_i2c_block_data(client, reg, len, buf);
// 	if (ret < 0) {
// 		dev_err(&client->dev, "I2C SMBus read failed, ret=%d\n", ret);
// 		return ret;
// 	}

// 	return ret;
// }

// static int cp_update_bits(struct i2c_client *client, u8 reg, u8 mask, u8 val)
// {
// 	u8 tmp;

// 	cp_read_byte(client, reg, &tmp);
// 	tmp &= ~mask;
// 	tmp |= val & mask;
// 	cp_write_byte(client, reg, tmp);

// 	return 0;
// }

enum {
    HL7243_STANDALONG = 0,
    HL7243_MASTER,
    HL7243_SLAVE,
};

static const char* hl7243_psy_name[] = {
    [HL7243_STANDALONG] = "cp_master",
    [HL7243_MASTER] = "cp_master",
    [HL7243_SLAVE] = "cp_slave",
};

static const char* hl7243_irq_name[] = {
    [HL7243_STANDALONG] = "hl7243-standalone-irq",
    [HL7243_MASTER] = "hl7243-master-irq",
    [HL7243_SLAVE] = "hl7243-slave-irq",
};

// static int hl7243_mode_data[] = {
//     [HL7243_STANDALONG] = HL7243_STANDALONG,
//     [HL7243_MASTER] = HL7243_MASTER,
//     [HL7243_SLAVE] = HL7243_SLAVE,
// };

enum {
    ADC_VUSB,
    ADC_VWPC,
    ADC_VBUS,
    ADC_IBUS,
    ADC_VBAT,
    ADC_VOUT,
    ADC_TDIE,
    ADC_MAX_NUM,
} HL7243_ADC_CH;

static const u32 hl7243_adc_accuracy_tbl[ADC_MAX_NUM] = {
    35000,	/* VUSB */
    35000,	/* VWPC */
    35000,	/* VUSB */
    150000,	/* IBUS */
    20000,	/* VBAT */
    20000,	/* VOUT */
    4,	/* TDIE */
};

static const int hl7243_adc_m[] =
{ 625, 625, 625,220,125, 125, 25 };

static const int hl7243_adc_l[] =
{100, 100, 100,100,100, 100, 100};

enum hl7243_notify {
    HL7243_NOTIFY_OTHER = 0,
    HL7243_NOTIFY_IBUSUCPF,
    HL7243_NOTIFY_IBUSUCPR,
    HL7243_NOTIFY_IBUSOCP = 4,
    HL7243_NOTIFY_VBUSOVP,
    HL7243_NOTIFY_VBATOVP = 7,
    HL7243_NOTIFY_VOUTOVP,
};
/*
enum hl7243_error_stata {
    ERROR_VBUS_HIGH = 0,
    ERROR_VBUS_LOW,
    ERROR_VBUS_OVP,
    ERROR_IBUS_OCP,
    ERROR_VBAT_OVP,
    ERROR_IBAT_OCP,
};
*/
enum hl7243_error_stata {
    VUSB_OV_STS_ERR = 0,
    VUSB_UV_STS_ERR,
    VWPC_OV_STS_ERR,
    VWPC_UV_STS_ERR,
    VBUS_OV_STS_ERR,
    OVPGATE_STS_ERR,
    WPCGATE_STS_ERR,
    VBUS_UV_STS_ERR,
};

enum hl7243_dev_mode {
    FWD_4_1_CHG_M =0,
    FWD_2_1_CHG_M,
    FWD_1_1_CHG_M,
    RESERVED_M,
    REV_1_4_CON_M,
    REV_1_2_CON_M,
    REV_1_1_CON_M,
    HL7243_DEV_MODE_MAX,
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
    {   .reg = 0x00, .len = 7, .bit = {
            {.mask = BIT(0), .name = "THSD_I", .notify = HL7243_NOTIFY_OTHER},
            {.mask = BIT(2), .name = "VOUT_OV_I", .notify = HL7243_NOTIFY_VOUTOVP},
            {.mask = BIT(3), .name = "VBAT_OV_I", .notify = HL7243_NOTIFY_VBATOVP},
            {.mask = BIT(4), .name = "VOUT_OK_REV_I", .notify = HL7243_NOTIFY_OTHER},
            {.mask = BIT(5), .name = "WDOG_I", .notify = HL7243_NOTIFY_OTHER},
            {.mask = BIT(6), .name = "VOUT_OK_CHG_I", .notify = HL7243_NOTIFY_OTHER},
            {.mask = BIT(7), .name = "STATE_CHG_I", .notify = HL7243_NOTIFY_OTHER},
        },
    },
    {   .reg = 0x01, .len = 8, .bit = {
            {.mask = BIT(0), .name = "TRACK_UV_I", .notify = HL7243_NOTIFY_OTHER},
            {.mask = BIT(1), .name = "TRACK_OV_I", .notify = HL7243_NOTIFY_OTHER},
            {.mask = BIT(2), .name = "VBUS_UV_I", .notify = HL7243_NOTIFY_OTHER},
            {.mask = BIT(3), .name = "VBUS_OV_I", .notify = HL7243_NOTIFY_VBUSOVP},
            {.mask = BIT(4), .name = "VWPC_UV_I", .notify = HL7243_NOTIFY_OTHER},
            {.mask = BIT(5), .name = "VWPC_OV_I", .notify = HL7243_NOTIFY_VBUSOVP},
            {.mask = BIT(6), .name = "VUSB_UV_I", .notify = HL7243_NOTIFY_OTHER},
            {.mask = BIT(7), .name = "VUSB_OV_I", .notify = HL7243_NOTIFY_VBUSOVP},
        },
    },
    {   .reg = 0x02, .len = 7, .bit = {
            {.mask = BIT(0), .name = "SCC_SHORT_I", .notify = HL7243_NOTIFY_OTHER},
            {.mask = BIT(1), .name = "CFLY_SHORT_I", .notify = HL7243_NOTIFY_OTHER},
            {.mask = BIT(2), .name = "FET_SHORT_I", .notify = HL7243_NOTIFY_OTHER},
            {.mask = BIT(3), .name = "SS_TO_I", .notify = HL7243_NOTIFY_OTHER},
            {.mask = BIT(4), .name = "IBUS_UCPR_I", .notify = HL7243_NOTIFY_IBUSUCPR},
            {.mask = BIT(5), .name = "IBUS_UCPF_I", .notify = HL7243_NOTIFY_IBUSUCPF},
            {.mask = BIT(7), .name = "IBUS_OCP_I", .notify = HL7243_NOTIFY_IBUSOCP},
        },
    },
    /*
    { .reg = 0x06, .len = 6, .bit = {
                {.mask = BIT(0), .name = "WDOG_STS", .notify = HL7243_NOTIFY_OTHER},
                {.mask = BIT(1), .name = "VOUT_OK_REV_STS", .notify = HL7243_NOTIFY_OTHER},
                {.mask = BIT(2), .name = "VOUT_OK_CHG_STS", .notify = HL7243_NOTIFY_OTHER},
                {.mask = (BIT(7)|BIT(6)|BIT(5)), .name = "STATE_CHG_STS", .notify = HL7243_NOTIFY_OTHER},
                },
    },
    { .reg = 0x07, .len = 6, .bit = {
                {.mask = BIT(1), .name = "WPCGATTE_STS", .notify = HL7243_NOTIFY_OTHER},
                {.mask = BIT(2), .name = "OVPGATE_STS", .notify = HL7243_NOTIFY_OTHER},
                {.mask = BIT(3), .name = "WPCGATE_DET_STS", .notify = HL7243_NOTIFY_OTHER},
                {.mask = BIT(4), .name = "OVPGATE_DET_STS", .notify = HL7243_NOTIFY_OTHER},
                {.mask = BIT(6), .name = "VBAT_OVP_STS", .notify = HL7243_NOTIFY_VBATOVP},
                {.mask = BIT(7), .name = "VOUT_OVP_STS", .notify = HL7243_NOTIFY_VOUTOVP},
                },
    },
    { .reg = 0x08, .len = 8, .bit = {
                {.mask = BIT(0), .name = "TRACK_UV_STS", .notify = HL7243_NOTIFY_OTHER},
                {.mask = BIT(1), .name = "TRACK_OV_STS", .notify = HL7243_NOTIFY_OTHER},
                {.mask = BIT(2), .name = "VBUS_UV_STS", .notify = HL7243_NOTIFY_OTHER},
                {.mask = BIT(3), .name = "VBUS_OV_STS", .notify = HL7243_NOTIFY_VBUSOVP},
                {.mask = BIT(4), .name = "VWPC_UV_STS", .notify = HL7243_NOTIFY_OTHER},
                {.mask = BIT(5), .name = "VWPC_OV_STS", .notify = HL7243_NOTIFY_OTHER},
                {.mask = BIT(6), .name = "VUSB_UV_STS", .notify = HL7243_NOTIFY_OTHER},
                {.mask = BIT(7), .name = "VUSB_OV_STS", .notify = HL7243_NOTIFY_OTHER},
                },
    },    */
};


/************************************************************************/
#define HL7243_DEVICE_ID                0x0C

#define HL7243_REG22                    0x22
#define HL7243_REGMAX                   0xFF
#define HL7243_REGMAX_DUMP              0x30

#define HL7243_TESTMODE_ADDR            0x4e

enum hl7243_reg_range {
    HL7243_VUSB_OVP,
    HL7243_VWPC_OVP,
    HL7243_VBUS_OVP,
    HL7243_IBUS_OCP,
    HL7243_VOUT_OVP,
    HL7243_VBAT_OVP,
    HL7243_IBUS_UCP,
    HL7243_FSW_SET,
    HL7243_TRACK_OV,
    HL7243_TRACK_UV,
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

#define HL7243_CHG_RANGE(_min, _max, _step, _offset, _ru) \
{ \
    .min = _min, \
    .max = _max, \
    .step = _step, \
    .offset = _offset, \
    .round_up = _ru, \
}

#define HL7243_CHG_RANGE_T(_table, _ru) \
    { .table = _table, .num_table = ARRAY_SIZE(_table), .round_up = _ru, }

const u32 HL7243_VUSB_OVP_TABLE[16]={11000,12000,13000,14000,
                             15000,16000,17000,18000,
                             19000,20000,21000,22000,
                             23000,24000,25000,7500};
const u32 HL7243_VWPC_OVP_TABLE[16]={11000,12000,13000,14000,
                             15000,16000,17000,18000,
                             19000,20000,21000,22000,
                             23000,24000,25000,7500};

static const struct reg_range hl7243_reg_range[] = {
    [HL7243_VUSB_OVP]      = HL7243_CHG_RANGE_T(HL7243_VUSB_OVP_TABLE, true),
    [HL7243_VWPC_OVP]      = HL7243_CHG_RANGE_T(HL7243_VWPC_OVP_TABLE, true),
    [HL7243_VBUS_OVP]      = HL7243_CHG_RANGE(15000, 27000, 800, 15000, false),
    [HL7243_IBUS_OCP]      = HL7243_CHG_RANGE(2000, 7500, 100, 2000, false),
    [HL7243_VOUT_OVP]      = HL7243_CHG_RANGE(4800, 5400, 200, 4800, false),
    [HL7243_VBAT_OVP]      = HL7243_CHG_RANGE(4300, 5245, 15, 4300, false),
    [HL7243_IBUS_UCP]      = HL7243_CHG_RANGE(200, 500, 100, 200, false),
    [HL7243_FSW_SET]       = HL7243_CHG_RANGE(150, 1700, 50, 150, false),
    [HL7243_TRACK_OV]      = HL7243_CHG_RANGE(200, 900, 100, 200, false),
    [HL7243_TRACK_UV]      = HL7243_CHG_RANGE(100, 450, 50, 100, false),
};
#if 0
enum hl7243_fields {
    INT1,   /*REG00*/
    INT2,   /*REG01*/
    INT3,   /*REG02*/
    INT1_MSK,/*REG03*/
    INT2_MSK,/*REG04*/
    INT3_MSK, /*REG05*/
    STATUS1,/*REG06*/
    STATUS2,/*REG07*/
    STATUS3,/*REG08*/
    VUSB_OVP,/*REG09*/
    VWPC_OVP,/*REG0A*/
    VBUS_OCP,/*REG0B*/
    IBUS_OCP,/*REG0C*/
    IBUS_UCP,/*REG0D*/
    VOUT_OVP,/*REG0E*/
    VBAT_OVP,/*REG0F*/
    TRACK,/*REG10*/
    FSW_CFG,/*REG11*/
    DEV_EN,/*REG12*/
    EXT_FET_CTL,/*REG13*/
    CTRL_0,/*REG14*/
    CTRL_1,/*REG15*/
    IIN_SNS_ADJ,/*REG16*/
    RESERVED1,    /*0x17~0x1F RSVD*/
    ADC_CTRL0,/*REG20*/
    ADC_CTRL1,/*REG21*/
    ADC_VUSB_1,/*REG22*/
    ADC_VUSB_0,/*REG23*/
    ADC_VWPC_1, /*REG24*/
    ADC_VWPC_0,/*REG25*/
    ADC_VBUS_1,/*REG26*/
    ADC_VBUS_0,/*REG27*/
    ADC_IBUS_1,/*REG28*/
    ADC_IBUS_0, /*REG29*/
    ADC_VBAT_1, /*REG2A*/
    ADC_VBAT_0,/*REG2B*/
    ADC_VOUT_1,/*REG2C*/
    ADC_VOUT_0,/*REG2D*/
    ADC_TDIE_1, /*REG2E*/
    ADC_TDIE_0, /*REG2F*/
    RESERVED2,/*0x30~0x6D RSVD*/
    DEVICE_ID,/*REG6E*/
    DEVICE_REV,/*REG6F*/
    F_MAX_FIELDS,
};
#endif
enum hl7243_status_bits {
    /*REG06*/
    STATE_CHG_STS_BIT,
    VOUT_OK_CHG_STS_BIT,
    VOUT_OK_REV_STS_BIT,
    VDOG_STS_BIT,
    /*REG07*/
    VOUT_OVP_STS_BIT,
    VBAT_OVP_STS_BIT,
    OVPGATE_DET_STS_BIT,
    WPCGATE_DET_STS_BIT,
    OVPGATE_STS_BIT,
    WPCGATE_STS_BIT,
    /*REG08*/
    VUSB_OV_STS_BIT,
    VUSB_UV_STS_BIT,
    VWPC_OV_STS_BIT,
    VWPC_UV_STS_BIT,
    VBUS_OV_STS_BIT,
    VBUS_UV_STS_BIT,
    TRACK_OV_STS_BIT,
    TRACK_UV_STS_BIT,
};
enum hl7243_fields {
    /*REG00*/
    STATE_CHG_I,
    VOUT_OK_CHG_I,
    VDOG_I,
    VOUT_OK_REV_I,
    VBAT_OV_I,
    VOUT_OV_I,
    THSD_I,
    INT1_I,
    /*REG01*/
    VUSB_OV_I,
    VUSB_UV_I,
    VWPC_OV_I,
    VWPC_UV_I,
    VBUS_OV_I,
    VBUS_UV_I,
    TRACK_OV_I,
    TRACK_UV_I,
    INT2_I,
    /*REG02*/
    IBUS_OCP_I,
    IBUS_UCPF_I,
    IBUS_UCPR_I,
    SS_TO_I,
    FET_SHORT_I,
    CFLY_SHORT_I,
    OVERLOAD_I,
    INT3_I,
    /*REG03*/
    STATE_CHG_M,
    VOUT_OK_CHG_M,
    VDOG_M,
    VOUT_OK_REV_M,
    VBAT_OV_M,
    VOUT_OV_M,
    THSD_M,
    INT1_M,
    /*REG04*/
    VUSB_OV_M,
    VUSB_UV_M,
    VWPC_OV_M,
    VWPC_UV_M,
    VBUS_OV_M,
    VBUS_UV_M,
    TRACK_OV_M,
    TRACK_UV_M,
    INT2_M,
    /*REG05*/
    IBUS_OCP_M,
    IBUS_UCPF_M,
    IBUS_UCPR_M,
    SS_TO_M,
    FET_SHORT_M,
    CFLY_SHORT_M,
    OVERLOAD_M,
    INT3_M,
    /*REG06*/
    STATE_CHG_STS,
    VOUT_OK_CHG_STS,
    VOUT_OK_REV_STS,
    VDOG_STS,
    /*REG07*/
    VOUT_OVP_STS,
    VBAT_OVP_STS,
    OVPGATE_DET_STS,
    WPCGATE_DET_STS,
    OVPGATE_STS,
    WPCGATE_STS,
    /*REG08*/
    VUSB_OV_STS,
    VUSB_UV_STS,
    VWPC_OV_STS,
    VWPC_UV_STS,
    VBUS_OV_STS,
    VBUS_UV_STS,
    TRACK_OV_STS,
    TRACK_UV_STS,
    /*REG09*/
    VUSB_DEB_SEL,
    OVPGATE_OFF,
    VUSB_VGS_SEL,
    VUSB_OVP_TH,
    /*REG0A*/
    VWPC_DEB_SEL,
    WPCATE_OFF,
    WPC_VGS_SEL,
    VWPC_OVP_TH,
    /*REG0B*/
    VBUS_PD_CFG,
    VBUS_OVP_DIS,
    TVBUS_OVP_DEB,
    VBUS_OVP_TH,
    /*REG0C*/
    IBUS_OCP_DIS,
    TIBUS_OV_DEB,
    IBUS_OCP_TH,
    /*REG0D*/
    IBUS_UCP_EN,
    AUTO_UCP_REC_EN,
    IBUS_UCP_TH,
    UCPF_DEB_SEL,
    UCPR_DEB_SEL,
    UCP_BLANK_SEL,
    /*REG0E*/
    VOUT_OVP_DIS,
    VOUT_OVP_TH,
    /*REG0F*/
    VBAT_OVP_DIS,
    TVBAT_OVP_DEB,
    VBAT_OVP_TH,
    /*REG10*/
    TRACK_OV_DIS,
    TRACK_UV_DIS,
    TRACK_OV_TH,
    TRACK_UV_TH,
    /*REG11*/
    FSW_SET,
    FSW_SHIFT,
    /*REG12*/
    CHG_EN,
    DEV_MODE,
    /*REG13*/
    EXTFET_MAN_EN,
    OVPGATE_EN,
    WPCGATE_EN,
    BYPASS_EN,
    VUSB_PD_EN,
    VWPC_PD_EN,
    /*REG14*/
    INT_TYPE,
    SS_TIMEOUT,
    /*REG15*/
    SET_RST,
    WD_DIS,
    WD_TMR,
    /*REG16*/
    REG_16,
    /*reg17*/
    DITHER_EN,
    DITHER_RATE,
    DITHER_LIMIT,
    /*reg1e*/
    REG_1E,
    /*reg1f*/
    REG_1F,
    /*REG20*/
    //ADC_REG_COPY,
    //ADC_MAN_COPY,
    //ADC_MODE_CFG,
    ADC_AVG_TIME,
    ADC_EN,
    /*REG21*/
    VUSB_ADC_DIS,
    VWPC_ADC_DIS,
    VBUS_ADC_DIS,
    IBUS_ADC_DIS,
    VBAT_ADC_DIS,
    TDIE_ADC_DIS,
    VOUT_ADC_DIS,
    /*REG60*/
    REG_60,
    /*REG6E*/
    DEVICE_ID,
    /*REG6F*/
    DEVICE_REV,
        /*REGFE*/
    REG_FE,
    F_MAX_FIELDS,
};



//REGISTER
static const struct reg_field hl7243_reg_fields[] = {
    /*reg00*/
    [STATE_CHG_I] = REG_FIELD(0x00, 7, 7),
    [VOUT_OK_CHG_I] = REG_FIELD(0x00, 6, 6),
    [VDOG_I] = REG_FIELD(0x00, 5, 5),
    [VOUT_OK_REV_I] = REG_FIELD(0x00, 4, 4),
    [VBAT_OV_I] = REG_FIELD(0x00, 3, 3),
    [VOUT_OV_I] = REG_FIELD(0x00, 2, 2),
    [THSD_I] = REG_FIELD(0x00, 0, 0),
    [INT1_I] = REG_FIELD(0x00, 0, 7),
    /*reg01*/
    [VUSB_OV_I] = REG_FIELD(0x01, 7, 7),
    [VUSB_UV_I] = REG_FIELD(0x01, 6, 6),
    [VWPC_OV_I] = REG_FIELD(0x01, 5, 5),
    [VWPC_UV_I] = REG_FIELD(0x01, 4, 4),
    [VBUS_OV_I] = REG_FIELD(0x01, 3, 3),
    [VBUS_UV_I] = REG_FIELD(0x01, 2, 2),
    [TRACK_OV_I] = REG_FIELD(0x01, 1, 1),
    [TRACK_UV_I] = REG_FIELD(0x01, 0, 0),
    [INT2_I] = REG_FIELD(0x01, 0, 7),
    /*reg02*/
    [IBUS_OCP_I] = REG_FIELD(0x02, 7, 7),
    [IBUS_UCPF_I] = REG_FIELD(0x02, 5, 5),
    [IBUS_UCPR_I] = REG_FIELD(0x02, 4, 4),
    [SS_TO_I] = REG_FIELD(0x02, 3, 3),
    [FET_SHORT_I] = REG_FIELD(0x02, 2, 2),
    [CFLY_SHORT_I] = REG_FIELD(0x02, 1, 1),
    [OVERLOAD_I] = REG_FIELD(0x02, 0, 0),
    [INT3_I] = REG_FIELD(0x02, 0, 7),
    /*reg03*/
    [STATE_CHG_M] = REG_FIELD(0x03, 7, 7),
    [VOUT_OK_CHG_M] = REG_FIELD(0x03, 6, 6),
    [VDOG_M] = REG_FIELD(0x03, 5, 5),
    [VOUT_OK_REV_M] = REG_FIELD(0x03, 4, 4),
    [VBAT_OV_M] = REG_FIELD(0x03, 3, 3),
    [VOUT_OV_M] = REG_FIELD(0x03, 2, 2),
    [THSD_M] = REG_FIELD(0x03, 0, 0),
    [INT1_M] = REG_FIELD(0x03, 0, 7),
    /*reg04*/
    [VUSB_OV_M] = REG_FIELD(0x04, 7, 7),
    [VUSB_UV_M] = REG_FIELD(0x04, 6, 6),
    [VWPC_OV_M] = REG_FIELD(0x04, 5, 5),
    [VWPC_UV_M] = REG_FIELD(0x04, 4, 4),
    [VBUS_OV_M] = REG_FIELD(0x04, 3, 3),
    [VBUS_UV_M] = REG_FIELD(0x04, 2, 2),
    [TRACK_OV_M] = REG_FIELD(0x04, 1, 1),
    [TRACK_UV_M] = REG_FIELD(0x04, 0, 0),
    [INT2_M] = REG_FIELD(0x04, 0, 7),
    /*reg05*/
    [IBUS_OCP_M] = REG_FIELD(0x05, 7, 7),
    [IBUS_UCPF_M] = REG_FIELD(0x05, 5, 5),
    [IBUS_UCPR_M] = REG_FIELD(0x05, 4, 4),
    [SS_TO_M] = REG_FIELD(0x05, 3, 3),
    [FET_SHORT_M] = REG_FIELD(0x05, 2, 2),
    [CFLY_SHORT_M] = REG_FIELD(0x05, 1, 1),
    [OVERLOAD_M] = REG_FIELD(0x05, 0, 0),
    [INT3_M] = REG_FIELD(0x05, 0, 7),
    /*REG06*/
    [STATE_CHG_STS] = REG_FIELD(0x06, 5, 7),
    [VOUT_OK_CHG_STS] = REG_FIELD(0x06, 2, 2),
    [VOUT_OK_REV_STS] = REG_FIELD(0x06, 1, 1),
    [VDOG_STS] = REG_FIELD(0x06, 0, 0),
    /*REG07*/
    [VOUT_OVP_STS] = REG_FIELD(0x07, 7, 7),
    [VBAT_OVP_STS] = REG_FIELD(0x07, 6, 6),
    [OVPGATE_DET_STS] = REG_FIELD(0x07, 4, 4),
    [WPCGATE_DET_STS] = REG_FIELD(0x07, 3, 3),
    [OVPGATE_STS] = REG_FIELD(0x07, 2, 2),
    [WPCGATE_STS] = REG_FIELD(0x07, 1, 1),
    /*REG08*/
    [VUSB_OV_STS] = REG_FIELD(0x08, 7, 7),
    [VUSB_UV_STS] = REG_FIELD(0x08, 6, 6),
    [VWPC_OV_STS] = REG_FIELD(0x08, 4, 4),
    [VWPC_UV_STS] = REG_FIELD(0x08, 4, 4),
    [VBUS_OV_STS] = REG_FIELD(0x08, 3, 3),
    [VBUS_UV_STS] = REG_FIELD(0x08, 2, 2),
    [TRACK_OV_STS] = REG_FIELD(0x08, 1, 1),
    [TRACK_UV_STS] = REG_FIELD(0x08, 0, 0),
    /*reg09*/
    [VUSB_DEB_SEL] = REG_FIELD(0x09, 6, 7),
    [OVPGATE_OFF] = REG_FIELD(0x09, 5, 5), /*VUSB_CTL_OFF*/
    [VUSB_VGS_SEL] = REG_FIELD(0x09, 4, 4),
    [VUSB_OVP_TH] = REG_FIELD(0x09, 0, 3),
    /*reg0a*/
    [VWPC_DEB_SEL] = REG_FIELD(0x0a, 6, 7),
    [WPCATE_OFF] = REG_FIELD(0x0a, 5, 5),
    [WPC_VGS_SEL] = REG_FIELD(0x0a, 4, 4),
    [VWPC_OVP_TH] = REG_FIELD(0x0a, 0, 3),
    /*reg0b*/
    [VBUS_PD_CFG] = REG_FIELD(0x0b, 7, 7),
    [VBUS_OVP_DIS] = REG_FIELD(0x0b, 6, 6),
    [TVBUS_OVP_DEB] = REG_FIELD(0x0b, 5, 5),
    [VBUS_OVP_TH] = REG_FIELD(0x0b, 0, 3),
    /*reg0c*/
    [IBUS_OCP_DIS] = REG_FIELD(0x0c, 7, 7),
    [TIBUS_OV_DEB] = REG_FIELD(0x0c, 6, 6),
    [IBUS_OCP_TH] = REG_FIELD(0x0c, 0, 5),
    /*reg0d*/
    [IBUS_UCP_EN] = REG_FIELD(0x0d, 7, 7),
    [AUTO_UCP_REC_EN] = REG_FIELD(0x0d, 6, 6),
    [IBUS_UCP_TH] = REG_FIELD(0x0d, 4, 5),
    [UCPF_DEB_SEL] = REG_FIELD(0x0d, 2, 3),
    [UCPR_DEB_SEL] = REG_FIELD(0x0d, 1, 1),
    [UCP_BLANK_SEL] = REG_FIELD(0x0d, 0, 0),
    /*reg0e*/
    [VOUT_OVP_DIS] = REG_FIELD(0x0e, 2, 2),
    [VOUT_OVP_TH] = REG_FIELD(0x0e, 0, 1),
    /*reg0f*/
    [VBAT_OVP_DIS] = REG_FIELD(0x0f, 7, 7),
    [TVBAT_OVP_DEB] = REG_FIELD(0x0f, 6, 6),
    [VBAT_OVP_TH] = REG_FIELD(0x0f, 0, 5),
    /*reg10*/
    [TRACK_OV_DIS] = REG_FIELD(0x10, 7, 7),
    [TRACK_UV_DIS] = REG_FIELD(0x10, 6, 6),
    [TRACK_OV_TH] = REG_FIELD(0x10, 3, 5),
    [TRACK_UV_TH] = REG_FIELD(0x10, 0, 2),
    /*reg11*/
    [FSW_SET] = REG_FIELD(0x11, 3, 7),
    [FSW_SHIFT] = REG_FIELD(0x11, 1, 2),
    /*reg12*/
    [CHG_EN] = REG_FIELD(0x12, 3, 3),
    [DEV_MODE] = REG_FIELD(0x12, 0, 2),
    /*reg13*/
    [OVPGATE_EN] = REG_FIELD(0x13, 6, 6),
    [WPCGATE_EN] = REG_FIELD(0x13, 5, 5),
    [BYPASS_EN] = REG_FIELD(0x13, 4, 4),
    [VUSB_PD_EN] = REG_FIELD(0x13, 2, 3),
    [VWPC_PD_EN] = REG_FIELD(0x13, 0, 1),
    /*reg14*/
    [INT_TYPE] = REG_FIELD(0x14, 6, 6),
    [SS_TIMEOUT] = REG_FIELD(0x14, 0, 2),
    /*reg15*/
    [SET_RST] = REG_FIELD(0x15, 5, 7),
    [WD_DIS] = REG_FIELD(0x15, 3, 3),
    [WD_TMR] = REG_FIELD(0x15, 0, 2),
    /*reg16*/
    [REG_16] = REG_FIELD(0x16, 0, 7),
    /*reg17*/
    [DITHER_EN] = REG_FIELD(0x17, 7, 7),
    [DITHER_RATE] = REG_FIELD(0x17, 5, 6),
    [DITHER_LIMIT] = REG_FIELD(0x17, 1, 4),
    /*reg1e*/
    [REG_1E] = REG_FIELD(0x1e, 0, 7),
        /*reg1f*/
    [REG_1F] = REG_FIELD(0x1f, 0, 7),
    /*reg20*/
    //[ADC_REG_COPY] = REG_FIELD(0x20, 7, 7),
    //[ADC_MAN_COPY] = REG_FIELD(0x20, 6, 6),
    //[ADC_MODE_CFG] = REG_FIELD(0x20, 3, 4),
    [ADC_AVG_TIME] = REG_FIELD(0x20, 1, 2),
    [ADC_EN] = REG_FIELD(0x20, 0, 0),
    /*reg21*/
    [VUSB_ADC_DIS] = REG_FIELD(0x21, 7, 7),
    [VWPC_ADC_DIS] = REG_FIELD(0x21, 6, 6),
    [VBUS_ADC_DIS] = REG_FIELD(0x21, 5, 5),
    [IBUS_ADC_DIS] = REG_FIELD(0x21, 4, 4),
    [VBAT_ADC_DIS] = REG_FIELD(0x21, 3, 3),
    [TDIE_ADC_DIS] = REG_FIELD(0x21, 2, 2),
    [VOUT_ADC_DIS] = REG_FIELD(0x21, 1, 1),
        /*reg60*/
    [REG_60] = REG_FIELD(0x60, 0, 7),
    /*reg6e*/
    [DEVICE_ID] = REG_FIELD(0x6e, 0, 7),
    /*reg6f*/
    [DEVICE_REV] = REG_FIELD(0x6f, 0, 7),
    [REG_FE] = REG_FIELD(0xfe, 0, 7),
};
static const struct regmap_config hl7243_regmap_config = {
    .reg_bits = 8,
    .val_bits = 8,

    .max_register = HL7243_REGMAX,
};
/************************************************************************/

struct hl7243_cfg_e {
    int vbat_ovp_dis;
    int vbat_ovp;
    int ibat_ocp_dis;
    int ibat_ocp;
    int vusb_ovp_dis;
    int vusb_ovp;
    int vwpc_ovp_dis;
    int vwpc_ovp;
    int vbus_ovp_dis;
    int vbus_ovp;
    int vout_ovp_dis;
    int vout_ovp;
    int ibus_ocp_dis;
    int ibus_ocp;
    int ibus_ucp_en;
    int ibus_ucp;
    int track_uv_dis;
    int track_uv;
    int track_ov_dis;
    int track_ov;
    int fsw_set;
    int ss_timeout;
    int wd_timer;
    int ibat_sns_r;
    int mode;
    int dev_mode;
    int tshut_dis;
};

struct hl7243_chip {
    struct device *dev;
    struct i2c_client *client;
    struct regmap *regmap;
    struct regmap_field *rmap_fields[F_MAX_FIELDS];

    struct hl7243_cfg_e cfg;
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
    bool chip_ok;
    bool bypass_support;
    u32 fault_type;
    bool en_failed;
    int pmic_ovp_en_gpio;
    bool acdrv_manual;
    int rev_ibus_adc_rate;


#ifdef CONFIG_MTK_CLASS
    struct charger_device *chg_dev;
    struct charger_properties chg_props;
#endif /*CONFIG_MTK_CLASS*/

#ifdef CONFIG_HALOCP_DVCHG_CLASS
    struct dvchg_dev *charger_pump;
#endif /*CONFIG_HALOCP_DVCHG_CLASS*/

    const char *chg_dev_name;
    struct power_supply_desc psy_desc;
    struct power_supply_config psy_cfg;
    struct power_supply *psy;
};

static int cp_get_adc_data(struct hl7243_chip *hl, int channel,  u32 *result);

struct mtk_cp_sysfs_field_info {
    struct device_attribute attr;
    enum cp_property prop;
    int (*set)(struct hl7243_chip *hl,
        struct mtk_cp_sysfs_field_info *attr, int val);
    int (*get)(struct hl7243_chip *hl,
        struct mtk_cp_sysfs_field_info *attr, int *val);
};

#ifdef CONFIG_MTK_CLASS
static const struct charger_properties hl7243_chg_props = {
    .alias_name = "hl7243_chg",
};
#endif /*CONFIG_MTK_CLASS*/


/********************COMMON API***********************/
static u8 val2reg(enum hl7243_reg_range id, u32 val)
{
    int i;
    u8 reg;
    const struct reg_range *range= &hl7243_reg_range[id];
    int max_less_idx = -1;
    int min_greater_idx = -1;
    u32 max_less_val = 0;
    u32 min_greater_val = 0xffffffff;
    u32 max_val = 0;
    u32 min_val = 0xffffffff;
    int min_idx = 0;
    int max_idx = 0;


    if (!range)
        return val;

    if (range->table) {
        for (i = 0; i < range->num_table; i++) {
            if (val == range->table[i]) {
                return i;
            } else if (range->table[i] < val && range->table[i] > max_less_val) {
                max_less_val = range->table[i];
                max_less_idx = i;
            } else if (range->table[i] > val && range->table[i] < min_greater_val) {
                min_greater_val = range->table[i];
                min_greater_idx = i;
            }
            if(max_val<range->table[i]) {
                max_val = range->table[i];
                max_idx = i;
            }
            if(min_val>range->table[i]) {
                min_val = range->table[i];
                min_idx = i;
            }

        }
        if(min_greater_idx == -1) {
            min_greater_idx = max_idx;
        }
        if(max_less_idx == -1) {
            max_less_idx = min_idx;
        }
        return range->round_up ? min_greater_idx:max_less_idx;
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

static u32 reg2val(enum hl7243_reg_range id, u8 reg)
{
    const struct reg_range *range= &hl7243_reg_range[id];
    if (!range)
        return reg;
    return range->table ? range->table[reg] :
        range->offset + range->step * reg;
}
/*********************************************************/
static int hl7243_field_read(struct hl7243_chip *hl,
                            enum hl7243_fields field_id, int *val)
{
    int ret;
    ret = regmap_field_read(hl->rmap_fields[field_id], val);
    if (ret < 0) {
        dev_err(hl->dev, "hl7243 read field %d fail: %d\n", field_id, ret);
    }
    return ret;
}

static int hl7243_field_write(struct hl7243_chip *hl,
                            enum hl7243_fields field_id, int val)
{
    int ret;

    ret = regmap_field_write(hl->rmap_fields[field_id], val);
    if (ret < 0) {
        dev_err(hl->dev, "hl7243 write field %d fail: %d\n", field_id, ret);
    }
    return ret;
}

static int _hl7243_write_reg(struct hl7243_chip *hl, u8 reg, u8 data)
{
    int ret;

    ret = i2c_smbus_write_byte_data(hl->client, reg, data);
    if (ret < 0) {
        dev_err(hl->dev, "write_reg: fail write 0x%02x to reg 0x%02x\n", data, reg);
        return ret;
    }

    return 0;
}

static int hl7243_read_block(struct hl7243_chip *hl,
                            int reg, uint8_t *val, int len)
{
    int ret;
    ret = regmap_bulk_read(hl->regmap, reg, val, len);
    if (ret < 0) {
        dev_err(hl->dev, "hl7243 read %02x block failed %d\n", reg, ret);
    }
    return ret;
}


/*******************************************************/
static int hl7243_detect_device(struct hl7243_chip *hl)
{
    int ret;
    int val;

    ret = hl7243_field_read(hl, DEVICE_ID, &val);
    if (ret < 0) {
        dev_err(hl->dev, "%s fail(%d)\n", __func__, ret);
        return ret;
    }

    if (val != HL7243_DEVICE_ID) {
        dev_err(hl->dev, "%s not find HL7243, ID = 0x%02x\n", __func__, val);
        return -EINVAL;
    }
    dev_err(hl->dev, "%s find HL7243, ID = 0x%02x\n", __func__, val);
    return ret;
}
static int hl7243_reg_reset(struct hl7243_chip *hl)
{
    return hl7243_field_write(hl, SET_RST, 6);
}

int hl7243_dump_reg(struct hl7243_chip *hl)
{
    int ret;
    int i;
    int val;
    for (i = 0; i <= HL7243_REGMAX_DUMP; i++) {
        ret = regmap_read(hl->regmap, i, &val);
        dev_err(hl->dev, "%s reg[0x%02x] = 0x%02x\n",
                __func__, i, val);
    }
    ret = regmap_read(hl->regmap, 0x6E, &val);
        dev_err(hl->dev, "%s reg[0x%02x] = 0x%02x\n",
                __func__, 0x6E, val);
    ret = regmap_read(hl->regmap, 0x6F, &val);
        dev_err(hl->dev, "%s reg[0x%02x] = 0x%02x\n",
                __func__, 0x6F, val);                

    return ret;
}
int hl7243_set_dev_mode(struct hl7243_chip *hl, int dev_mode)
{
    int ret,val;
    int dev_mode_val;
    dev_info(hl->dev,"%s:%d",__func__,dev_mode);
    switch (dev_mode) {
    case FWD_4_1_CHG_M:
        dev_mode_val= FWD_4_1_CHG_M;
        break;
    case FWD_2_1_CHG_M:
        dev_mode_val= FWD_2_1_CHG_M;
        break;
    case FWD_1_1_CHG_M:
        dev_mode_val= FWD_1_1_CHG_M;
        break;
    case REV_1_4_CON_M:
        dev_mode_val= REV_1_4_CON_M;
        break;
    case REV_1_2_CON_M:
        dev_mode_val= REV_1_2_CON_M;
        break;
    case REV_1_1_CON_M:
        dev_mode_val= REV_1_1_CON_M;
        break;
    default:
        dev_mode_val= FWD_4_1_CHG_M;
        break;
    }
    ret = hl7243_field_read(hl,CHG_EN,&val);
    if(ret)
    {
        dev_err(hl->dev, "%s charge enabled can not change mode\n", __func__);
        return -1;
    }
    ret = hl7243_field_write(hl, DEV_MODE, dev_mode_val);
    if (ret < 0) {
        dev_err(hl->dev, "%s fail to read DEV_MODE(%d)\n", __func__, ret);
    }
    hl->cfg.dev_mode = dev_mode;
    return ret;
}

static int hl7243_get_dev_mode(struct hl7243_chip *hl, int* mode)
{
    int ret,val;

    ret = hl7243_field_read(hl, DEV_MODE, &val);
    *mode = val;

    return ret;
}
static int HL7243_register_write(struct hl7243_chip *hl, u8 slv_addr, u8 addr, u8 data)
{
    int ret = 0;
    u8 back_addr = hl->client->addr;
    hl->client->addr = slv_addr;
    ret = i2c_smbus_write_byte_data(hl->client, addr, data);
    hl->client->addr = back_addr;
    return ret;
}

static int HL7243_register_read(struct hl7243_chip *hl, u8 slv_addr, u8 addr, int *data)
{
    int ret = 0;
    u8 back_addr = hl->client->addr;
    hl->client->addr = slv_addr;
    ret = i2c_smbus_read_byte_data(hl->client, addr);
    if(ret>=0)
    {
        *data = ret;
    } else {
        pr_err("%s read reg:0x%2x err:%d.\n",__func__, addr, ret);
    }
    hl->client->addr = back_addr;
    return ret;
}

static int hl7243_set_watch_dog_time(struct hl7243_chip *hl, int time)
{
        int val;
        dev_info(hl->dev,"%s:%d\n",__func__,time);
        hl7243_field_read(hl, WD_TMR, &val);
        dev_info(hl->dev,"%s:watch_dog timer=%d\n",__func__,val);
        hl7243_field_read(hl, WD_DIS, &val);
        dev_info(hl->dev,"%s:watch_dog disable=%d\n",__func__,val);
        if(time)
        {
                hl7243_field_write(hl, WD_DIS, 0);
                hl7243_field_write(hl, WD_TMR, time);
        }else
        {
                hl7243_field_write(hl, WD_DIS, 1);
        }
        return 0;
}

int hl7243_enable_adc(struct hl7243_chip *hl, bool en)
{
    dev_info(hl->dev, "%s: %d\n", __func__, en);

    if (en)
        hl7243_set_watch_dog_time(hl, 4);    // watchdog timeout 10s
    else
        hl7243_set_watch_dog_time(hl, 0);    // disable watchdog

    return hl7243_field_write(hl, ADC_EN, !!en);
}

static void hl7243_getcfgandshow(struct hl7243_chip *hl,enum hl7243_fields field_id,enum hl7243_reg_range id,char *name,bool en)
{
    int val;
    hl7243_field_read(hl,field_id,&val);
    if(en)
        dev_err(hl->dev,"%s:%s,%d\n",__func__,name,val);
    else
        dev_err(hl->dev,"%s:%s,%d, reg:%d\n",__func__,name,reg2val(id,val),val);
}
void hl7243_showcfg(struct hl7243_chip *hl)
{
    hl7243_getcfgandshow(hl,VBAT_OVP_DIS,0,"VBAT_OVP_DIS",1);
    hl7243_getcfgandshow(hl,VBAT_OVP_TH,HL7243_VBAT_OVP,"VBAT_OVP",0);
    hl7243_getcfgandshow(hl,VUSB_OVP_TH,HL7243_VUSB_OVP,"VUSB_OVP",0);
    hl7243_getcfgandshow(hl,VWPC_OVP_TH,HL7243_VWPC_OVP,"VWPC_OVP",0);
    hl7243_getcfgandshow(hl,VBUS_OVP_DIS,0,"VBUS_OVP_DIS",1);
    hl7243_getcfgandshow(hl,VBUS_OVP_TH,HL7243_VBUS_OVP,"VBUS_OVP",0);
    hl7243_getcfgandshow(hl,VOUT_OVP_DIS,0,"VOUT_OVP_DIS",1);
    hl7243_getcfgandshow(hl,VOUT_OVP_TH,HL7243_VOUT_OVP,"VOUT_OVP",0);
    hl7243_getcfgandshow(hl,IBUS_OCP_DIS,0,"IBUS_OCP_DIS",1);
    hl7243_getcfgandshow(hl,IBUS_OCP_TH,HL7243_IBUS_OCP,"IBUS_OCP",0);
    hl7243_getcfgandshow(hl,IBUS_UCP_EN,0,"IBUS_UCP_EN",1);
    hl7243_getcfgandshow(hl,IBUS_UCP_TH,HL7243_IBUS_UCP,"IBUS_UCP",0);
    hl7243_getcfgandshow(hl,TRACK_OV_DIS,0,"TRACK_OV_DIS",1);
    hl7243_getcfgandshow(hl,TRACK_OV_TH,HL7243_TRACK_OV,"TRACK_OV",0);
    hl7243_getcfgandshow(hl,TRACK_UV_DIS,0,"TRACK_UV_DIS",1);
    hl7243_getcfgandshow(hl,TRACK_UV_TH,HL7243_TRACK_UV,"TRACK_UV",0);
    hl7243_getcfgandshow(hl,FSW_SET,HL7243_FSW_SET,"FSW_SET",0);
    hl7243_getcfgandshow(hl,SS_TIMEOUT,0,"SS_TIMEOUT",1);
    hl7243_getcfgandshow(hl,WD_TMR,0,"WD_TIMER",1);
    hl7243_getcfgandshow(hl,DEV_MODE,0,"DEV_MODE",1);
}
static int hl7243_init_device(struct hl7243_chip *hl)
{
    int ret = 0;
    int i;
    int val;
    int trm_isgn12_post;
    struct {
        enum hl7243_fields field_id;
        int conv_data;
    } props[] = {
        {VBAT_OVP_DIS, hl->cfg.vbat_ovp_dis},
        {VBAT_OVP_TH, val2reg(HL7243_VBAT_OVP,hl->cfg.vbat_ovp)},
        {VUSB_OVP_TH, val2reg(HL7243_VUSB_OVP,hl->cfg.vusb_ovp)},
        {VWPC_OVP_TH, val2reg(HL7243_VWPC_OVP,hl->cfg.vwpc_ovp)},
        {VBUS_OVP_DIS, hl->cfg.vbus_ovp_dis},
        {VBUS_OVP_TH, val2reg(HL7243_VBUS_OVP,hl->cfg.vbus_ovp)},
        {VOUT_OVP_DIS, hl->cfg.vout_ovp_dis},
        {VOUT_OVP_TH, val2reg(HL7243_VOUT_OVP,hl->cfg.vout_ovp)},
        {IBUS_OCP_DIS, hl->cfg.ibus_ocp_dis},
        {IBUS_OCP_TH, val2reg(HL7243_IBUS_OCP,hl->cfg.ibus_ocp)},
        {IBUS_UCP_EN, hl->cfg.ibus_ucp_en},
        {IBUS_UCP_TH, val2reg(HL7243_IBUS_UCP,hl->cfg.ibus_ucp)},
        {TRACK_UV_DIS, hl->cfg.track_uv_dis},
        {TRACK_UV_TH, val2reg(HL7243_TRACK_UV,hl->cfg.track_uv)},
        {TRACK_OV_DIS, hl->cfg.track_ov_dis},
        {TRACK_OV_TH, val2reg(HL7243_TRACK_OV,hl->cfg.track_ov)},
        {FSW_SET, val2reg(HL7243_FSW_SET,hl->cfg.fsw_set)},
        {SS_TIMEOUT, hl->cfg.ss_timeout},
        {WD_TMR, hl->cfg.wd_timer},
        {DEV_MODE, hl->cfg.dev_mode},
    };
    ret = hl7243_reg_reset(hl);
    if (ret < 0) {
        dev_err(hl->dev, "%s Failed to reset registers(%d)\n", __func__, ret);
    }
    msleep(10);

    for (i = 0; i < ARRAY_SIZE(props); i++) {
        //dev_err(hl->dev, "init write id:%d reg:%x value: %d\n", props[i].field_id,hl7243_reg_fields[props[i].field_id].reg, props[i].conv_data);
        ret = hl7243_field_write(hl, props[i].field_id, props[i].conv_data);
    }
    if (ret < 0) {
        dev_err(hl->dev, "%s Failed to write props registers(%d)\n", __func__, ret);
    }

    //hl7243_enable_adc(hl, true);


    ret = HL7243_register_read(hl,HL7243_TESTMODE_ADDR,0x07,&val);   // read second slave addr
    dev_err(hl->dev, "%s HL7243_register_read 0x%x return:0x%02x val:0x%x\n", __func__, HL7243_TESTMODE_ADDR, ret, val);
    hl7243_field_write(hl, REG_FE, 0xF9);
    hl7243_field_write(hl, REG_FE, 0x9F);

    hl7243_field_write(hl, REG_1E, 0x7d);                        // en_resnt

    ret = HL7243_register_read(hl,HL7243_TESTMODE_ADDR,0x07,&val);   // test mode can read second slave addr
    dev_err(hl->dev, "%s test mode HL7243_register_read 0x%x return:0x%02x val:0x%x\n", __func__, HL7243_TESTMODE_ADDR, ret, val);
    ret |= HL7243_register_write(hl,HL7243_TESTMODE_ADDR,0x05,0x30);
    ret |= HL7243_register_write(hl,HL7243_TESTMODE_ADDR,0x38,0x08);
    ret |= HL7243_register_write(hl,HL7243_TESTMODE_ADDR,0x39,0x02);
    ret |= HL7243_register_write(hl,HL7243_TESTMODE_ADDR,0x35,0x20);
    ret |= HL7243_register_write(hl,HL7243_TESTMODE_ADDR,0x3a,0x70);
    ret |= HL7243_register_write(hl,HL7243_TESTMODE_ADDR,0x3b,0x07);

    ret |= hl7243_field_write(hl, REG_60, 0x00);
    ret |= hl7243_field_write(hl, REG_16, 0x0c);
    ret |= hl7243_field_write(hl, REG_1F, 0xe1);

    ret |= HL7243_register_write(hl,HL7243_TESTMODE_ADDR,0xBF,0xBF);
    ret |= HL7243_register_write(hl,HL7243_TESTMODE_ADDR,0xBD,0x41);
    msleep(5);
    ret |= HL7243_register_read(hl,HL7243_TESTMODE_ADDR,0xBC,&trm_isgn12_post);
    if (ret < 0) {
        dev_err(hl->dev, "%s HL7243: read ISGN12 failed, ret=%d, use default\n", __func__, ret);
        hl->rev_ibus_adc_rate = 293;
    } else if (trm_isgn12_post == 0) {
        hl->rev_ibus_adc_rate = 293;
    } else {
        hl->rev_ibus_adc_rate = 200000 / (trm_isgn12_post + 512);
    }
    ret |= hl7243_field_write(hl, REG_FE, 0x00);
    if (ret < 0) {
        dev_err(hl->dev, "%s Failed to write trim registers(%d)\n", __func__, ret);
    }

    hl7243_field_read(hl, INT1_I, &val);
    hl7243_field_read(hl, INT2_I, &val);
    hl7243_field_read(hl, INT3_I, &val);
    hl7243_field_write(hl, INT1_M, 0x00);
    hl7243_field_write(hl, INT2_M, 0x00);
    hl7243_field_write(hl, INT3_M, 0x00);
        
    hl7243_dump_reg(hl);
    hl->fault_type = 0;
    hl->acdrv_manual = 0;
    hl7243_showcfg(hl);
    return ret;
}
static int hl7243_set_present(struct hl7243_chip *hl, bool present)
{
    hl->usb_present = present;

    if (present)
        hl7243_init_device(hl);
    else
        hl7243_enable_adc(hl, false);
    return 0;
}
int hl7243_enable_charge(struct hl7243_chip *hl, bool en)
{
    int ret,val;
    int mode=0;
    int cnt;
    int vbusuv;
    int dnoten=0;
    int with5v=0;
    dev_info(hl->dev,"%s:%d\n",__func__,en);
    ret = hl7243_field_read(hl, DEV_MODE, &mode);
    if (ret < 0)
    {
        dev_err(hl->dev, "%s fail to read DEV_MODE(%d)\n", __func__, ret);
        return -1;
    }
    if(en)
    {
        if((mode>=REV_1_4_CON_M)&&(mode<=HL7243_DEV_MODE_MAX))
        {
            ret = hl7243_field_write(hl, REG_FE, 0xf9);
            ret += hl7243_field_write(hl, REG_FE, 0x9f);
            if((mode==REV_1_1_CON_M)||(mode==HL7243_DEV_MODE_MAX))
                ret += hl7243_field_write(hl, REG_1F, 0xe3);
            if(mode!=REV_1_2_CON_M) // REV_1_4 and REV_1_1 check vbus state
            {
                ret += hl7243_field_write(hl, DEV_MODE, 0); // set to FWD_4_1 to check vbus state
                msleep(1);
                ret += hl7243_field_read(hl, VBUS_UV_STS, &vbusuv);
                ret += hl7243_field_write(hl, DEV_MODE, mode); //
                if(vbusuv==0) // with load 5v
                {
                    dev_err(hl->dev, "%s with vbus 5v set to rev_1_4 or rev_1_1 ", __func__);
                    if((mode==REV_1_1_CON_M)||(mode==HL7243_DEV_MODE_MAX))
                        dnoten = 1;
                    with5v = 1;
                }
            }
            if((mode==REV_1_4_CON_M)||(mode==REV_1_2_CON_M))
            {
                ret += hl7243_field_write(hl, REG_16, 0x0e);
                ret += hl7243_field_write(hl, REG_60, 0x41);
            }
            if((mode!=REV_1_2_CON_M)&&(!with5v))
            {
                ret += hl7243_field_write(hl, REG_60, 0x3c);
                msleep(1);
                ret += hl7243_field_write(hl, REG_60, 0x34);
            }
            msleep(100);
        }
        if((ret < 0)||(dnoten))
        {
            dev_err(hl->dev, "%s fail to write before charge en(%d)\n", __func__, ret);
        }else{
            ret = hl7243_field_write(hl, CHG_EN, !!en);
            if (ret < 0) {
                dev_err(hl->dev, "%s fail to write CHG_EN(%d)\n", __func__, ret);
            }
            cnt = 0;
            while (cnt < 40)
            {
                ret += hl7243_field_read(hl, STATE_CHG_STS, &val);
                if ((val != 0) && (val != 1)) {
                    break;
                }
                msleep(10);
                cnt++;
            }
            dev_err(hl->dev, "%s CHG_EN(%d) startup cnt:%d status:%d\n", __func__, en, cnt, val);
            if (cnt >= 40)
            {
                dev_err(hl->dev, "%s CHG_EN(%d) startup timeout\n", __func__, en);
            }
        }
        if((mode>=REV_1_4_CON_M)&&(mode<=HL7243_DEV_MODE_MAX))
        {
            if((mode==REV_1_4_CON_M)||(mode==REV_1_2_CON_M))
            {
                ret += hl7243_field_write(hl, REG_60, 0x00);
                msleep(50);
                ret += hl7243_field_write(hl, REG_16, 0x0c);
            }
            if((mode!=REV_1_2_CON_M)&&(!with5v))
            {
                ret += hl7243_field_write(hl, REG_60, 0x30);
                ret += hl7243_field_write(hl, REG_60, 0x00);
            }
            if((mode==REV_1_1_CON_M)||(mode==HL7243_DEV_MODE_MAX)) {
                ret += hl7243_field_write(hl, REG_1F, 0xe1);
            }

            ret += hl7243_field_write(hl, REG_FE, 00);
        }
    }else
    {
        ret = hl7243_field_write(hl, CHG_EN, !!en);
        if (ret < 0)
            dev_err(hl->dev, "%s fail to write CHG_EN(%d)\n", __func__, ret);
    }
    
    if (ret < 0)
        dev_err(hl->dev, "%s fail to read write register(%d)\n", __func__, ret);
    // hl7243_dump_reg(hl);
    return ret;
}

static int hl7243_check_charge_enabled(struct hl7243_chip *hl, bool *enabled)
{
    int ret, val;
    u8 sts_buf[3];
    u8 intr_buf[3];
    ret = hl7243_field_read(hl, STATE_CHG_STS, &val);
    if(val > 1) {
        *enabled = 1;
    } else {
        *enabled = 0;
    }
    hl7243_read_block(hl,0x0,intr_buf,3);
    hl7243_read_block(hl,0x6,sts_buf,3);
    dev_info(hl->dev,"%s:%d intr = %x %x %x sts = %x %x %x \n", __func__, val,intr_buf[0],intr_buf[1],intr_buf[2],sts_buf[0],sts_buf[1],sts_buf[2]);
    return ret;
}

static int hl7243_get_usb_gate_enable(struct hl7243_chip *hl, bool* enable)
{
    int ret,val;

    ret = hl7243_field_read(hl, OVPGATE_EN, &val);
    if (ret < 0) {
        return ret;
    }

    *enable = !!val;
    return ret;
}

__maybe_unused static int hl7243_get_ibus_ocp_disable(struct hl7243_chip *hl, bool* enable)
{
    int ret,val;

    ret = hl7243_field_read(hl, IBUS_OCP_DIS, &val);
    if (ret < 0) {
        return ret;
    }

    *enable = !!val;
    return ret;
}

__maybe_unused static int hl7243_get_status(struct hl7243_chip *hl, uint32_t *status)
{
    int ret, val;
    *status = 0;
    ret = hl7243_field_read(hl, VOUT_OVP_STS, &val);
    if (ret < 0) {
        dev_err(hl->dev, "%s fail to read VOUT_OVP_STS(%d)\n", __func__, ret);
        return ret;
    }
    if (val != 0) {
        *status |= BIT(VOUT_OVP_STS_BIT);
    }
    ret = hl7243_field_read(hl, VBAT_OVP_STS, &val);
    if (ret < 0) {
        dev_err(hl->dev, "%s fail to read VBAT_OVP_STS(%d)\n", __func__, ret);
        return ret;
    }
    if (val != 0) {
        *status |= BIT(VBAT_OVP_STS_BIT);
    }
    
    ret = hl7243_field_read(hl, VUSB_OV_STS, &val);
    if (ret < 0) {
        dev_err(hl->dev, "%s fail to read VUSB_OV_STS(%d)\n", __func__, ret);
        return ret;
    }
    if (val != 0) {
        *status |= BIT(VUSB_OV_STS_BIT);

    }

    ret = hl7243_field_read(hl, VUSB_UV_STS, &val);
    if (ret < 0) {
        dev_err(hl->dev, "%s fail to read VUSB_UV_STS(%d)\n", __func__, ret);
        return ret;
    }
    if (val != 0) {
        *status |= BIT(VUSB_UV_STS_BIT);
    }

    ret = hl7243_field_read(hl, VWPC_OV_STS, &val);
    if (ret < 0) {
        dev_err(hl->dev, "%s fail to read VWPC_OV_STS(%d)\n", __func__, ret);
        return ret;
    }
    if (val != 0) {
        *status |= BIT(VWPC_OV_STS_BIT);
    }

    ret = hl7243_field_read(hl, VWPC_UV_STS, &val);
    if (ret < 0) {
        dev_err(hl->dev, "%s fail to read VWPC_UV_STS(%d)\n", __func__, ret);
        return ret;
    }
    if (val != 0) {
        *status |= BIT(VWPC_UV_STS_BIT);
    }

    ret = hl7243_field_read(hl, VBUS_OV_STS, &val);
    if (ret < 0) {
        dev_err(hl->dev, "%s fail to read VBUS_OV_STS(%d)\n", __func__, ret);
        return ret;
    }
    if (val != 0)
        *status |= BIT(VBUS_OV_STS_BIT);

    ret = hl7243_field_read(hl, VBUS_UV_STS, &val);
    if (ret < 0) {
        dev_err(hl->dev, "%s fail to read VBUS_UV_STS(%d)\n", __func__, ret);
        return ret;
    }
    if (val != 0)
        *status |= BIT(VBUS_UV_STS_BIT);


    ret = hl7243_field_read(hl, OVPGATE_STS, &val);
    if (ret < 0) {
        dev_err(hl->dev, "%s fail to read OVPGATE_STS(%d)\n", __func__, ret);
        return ret;
    }
    if (val != 0) {
        *status |= BIT(OVPGATE_STS_BIT);
    }

    ret = hl7243_field_read(hl, WPCGATE_STS, &val);
    if (ret < 0) {
        dev_err(hl->dev, "%s fail to read WPCGATE_STS(%d)\n", __func__, ret);
        return ret;
    }
    if (val != 0) {
        *status |= BIT(WPCGATE_STS_BIT);
    }

    ret = hl7243_field_read(hl, TRACK_OV_STS, &val);
    if (ret < 0) {
        dev_err(hl->dev, "%s fail to read TRACK_OV_STS(%d)\n", __func__, ret);
        return ret;
    }
    if (val != 0) {
        *status |= BIT(TRACK_OV_STS_BIT);
    }

    ret = hl7243_field_read(hl, TRACK_UV_STS, &val);
    if (ret < 0) {
        dev_err(hl->dev, "%s fail to read TRACK_UV_STS(%d)\n", __func__, ret);
        return ret;
    }
    if (val != 0) {
        *status |= BIT(TRACK_UV_STS_BIT);
    }

    ret = hl7243_field_read(hl, VDOG_STS, &val);
    if (ret < 0) {
        dev_err(hl->dev, "%s fail to read VDOG_STS(%d)\n", __func__, ret);
        return ret;
    }
    if (val != 0)
        *status |= BIT(VDOG_STS_BIT);
    return ret;

}


int hl7243_get_adc_data(struct hl7243_chip *hl,
                        int channel, int *result)
{
    uint8_t val[2] = {0};
    int ret;

    if(channel >= ADC_MAX_NUM)
        return -EINVAL;

    ret = hl7243_read_block(hl, HL7243_REG22 + (channel << 1), val, 2);
    if (ret < 0) {
        return ret;
    }
    if((channel == ADC_IBUS) && (hl->cfg.dev_mode == REV_1_2_CON_M)) {
        *result = (val[1] | (val[0] << 4)) *
                hl->rev_ibus_adc_rate / 100;
    } else {
        *result = (val[1] | (val[0] << 4)) *
                hl7243_adc_m[channel] / hl7243_adc_l[channel];
    }

    //dev_info(hl->dev,"%s %d %d", __func__, channel, *result);

    return ret;
}

static int hl7243_set_busovp_th(struct hl7243_chip *hl, int threshold)
{
    int reg_val;
    int temp_threshold = 0;
    int ret;
    int val;

    ret = hl7243_field_read(hl, DEV_MODE, &val);
    if (ret < 0) {
        dev_err(hl->dev, "%s fail to read MODE(%d)\n", __func__, ret);
        return ret;
    }

    if ((val == FWD_4_1_CHG_M)||(val == REV_1_4_CON_M)) {
        temp_threshold = threshold;
    } else if ((val == FWD_1_1_CHG_M)||(val == REV_1_1_CON_M))
    {
        temp_threshold = threshold * 4;
    } else if ((val == FWD_2_1_CHG_M)||(val == REV_1_2_CON_M))
    {
        temp_threshold = threshold * 2;
    } else {
        return -1;
    }

    reg_val = val2reg(HL7243_VBUS_OVP, temp_threshold);
    dev_info(hl->dev,"%s:%d-%d", __func__, threshold, reg_val);

    return hl7243_field_write(hl, VBUS_OVP_TH, reg_val);
}

static int hl7243_set_busocp_th(struct hl7243_chip *hl, int threshold)
{
    int reg_val = val2reg(HL7243_IBUS_OCP, threshold);

    dev_info(hl->dev,"%s:%d-%d", __func__, threshold, reg_val);

    return hl7243_field_write(hl, IBUS_OCP_TH, reg_val);
}

static int hl7243_set_batovp_th(struct hl7243_chip *hl, int threshold)
{
    int reg_val = val2reg(HL7243_VBAT_OVP, threshold);

    dev_info(hl->dev,"%s:%d-%d", __func__, threshold, reg_val);

    return hl7243_field_write(hl, VBAT_OVP_TH, reg_val);
}

static int hl7243_set_usbovp_th(struct hl7243_chip *hl, int threshold)
{
    int reg_val = val2reg(HL7243_VUSB_OVP, threshold);

    dev_info(hl->dev,"%s:%d-%d", __func__, threshold, reg_val);

    return hl7243_field_write(hl, VUSB_OVP_TH, reg_val);
}

static int hl7243_set_vwpcovp_th(struct hl7243_chip *hl, int threshold)
{
    int reg_val = val2reg(HL7243_VWPC_OVP, threshold);

    dev_info(hl->dev,"%s:%d-%d", __func__, threshold, reg_val);

    return hl7243_field_write(hl, VWPC_OVP_TH, reg_val);
}
static int hl7243_set_batocp_th(struct hl7243_chip *hl, int threshold)
{
    //int reg_val = val2reg(HL7243_IBAT_OCP, threshold);

    //dev_info(hl->dev,"%s:%d-%d", __func__, threshold, reg_val);
    //dev_info(hl->dev,"%s:hl7243 no ibat ocp", __func__);
    // return hl7243_field_write(hl, IBAT_OCP, reg_val);
    return 0;
}

static int hl7243_set_vbusovp_alarm(struct hl7243_chip *hl, int threshold)
{
    dev_info(hl->dev,"%s:%d", __func__, threshold);

    return 0;
}

static int hl7243_set_vbatovp_alarm(struct hl7243_chip *hl, int threshold)
{
    dev_info(hl->dev,"%s:%d", __func__, threshold);

    return 0;
}


static int hl7243_is_vbuslowerr(struct hl7243_chip *hl, bool *err)
{
    int ret;
    int val;

    ret = hl7243_field_read(hl, VBUS_UV_STS, &val);
    if(ret < 0) {
        return ret;
    }

    dev_info(hl->dev,"%s:%d",__func__,val);

    *err = (bool)val;

    return ret;
}



/*********************mtk charger interface start**********************************/
#ifdef CONFIG_MTK_CLASS
static inline int to_hl7243_adc(enum adc_channel chan)
{
    switch (chan) {
    case ADC_CHANNEL_VBUS:
        return ADC_VBUS;
    case ADC_CHANNEL_VBAT:
        return ADC_VBAT;
    case ADC_CHANNEL_IBUS:
        return ADC_IBUS;
    //case ADC_CHANNEL_IBAT:
    //	return ADC_IBAT;
    case ADC_CHANNEL_TEMP_JC:
        return ADC_TDIE;
    case ADC_CHANNEL_VOUT:
        return ADC_VOUT;
    default:
        break;
    }
    return ADC_MAX_NUM;
}


static int mtk_hl7243_is_chg_enabled(struct charger_device *chg_dev, bool *en)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);
    int ret;

    ret = hl7243_check_charge_enabled(hl, en);

    return ret;
}

static int mtk_hl7243_dev_mode (struct charger_device *chg_dev,int dev_mode )
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);
    int ret;

    ret = hl7243_set_dev_mode(hl,dev_mode);

    return ret;
}

static int mtk_hl7243_enable_chg(struct charger_device *chg_dev, bool en)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);
    int ret;

    ret = hl7243_enable_charge(hl,en);

    return ret;
}


static int mtk_hl7243_set_vbusovp(struct charger_device *chg_dev, u32 uV)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);
    int mv;
    mv = uV / 1000;

    return hl7243_set_busovp_th(hl, mv);
}

static int mtk_hl7243_set_ibusocp(struct charger_device *chg_dev, u32 uA)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);
    int ma;
    ma = uA / 1000;

    return hl7243_set_busocp_th(hl, ma);
}

static int mtk_hl7243_set_vbatovp(struct charger_device *chg_dev, u32 uV)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);
    int ret;

    ret = hl7243_set_batovp_th(hl, uV/1000);
    if (ret < 0)
        return ret;

    return ret;
}

static int mtk_hl7243_set_ibatocp(struct charger_device *chg_dev, u32 uA)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);
    int ret;

    ret = hl7243_set_batocp_th(hl, uA/1000);
    if (ret < 0)
        return ret;

    return ret;
}


static int mtk_hl7243_get_adc(struct charger_device *chg_dev, enum adc_channel chan,
                            int *min, int *max)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);

    hl7243_get_adc_data(hl, to_hl7243_adc(chan), max);

    if(chan != ADC_CHANNEL_TEMP_JC)
        *max = *max * 1000;

    if (min != max)
        *min = *max;

    return 0;
}

static int mtk_hl7243_get_adc_accuracy(struct charger_device *chg_dev,
                                    enum adc_channel chan, int *min, int *max)
{
    *min = *max = hl7243_adc_accuracy_tbl[to_hl7243_adc(chan)];
    return 0;
}


static int mtk_hl7243_get_vbus_adc(struct charger_device *chg_dev, u32 *val)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);
    hl7243_get_adc_data(hl, to_hl7243_adc(ADC_CHANNEL_VBUS), val);

    return 0;
}

static int mtk_hl7243_get_ibus_adc(struct charger_device *chg_dev, u32 *val)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);
    hl7243_get_adc_data(hl, to_hl7243_adc(ADC_CHANNEL_IBUS), val);
    pr_info("%s, ibus:%d\n",__func__,*val);

    return 0;
}

static int mtk_hl7243_is_vbuslowerr(struct charger_device *chg_dev, bool *err)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);

    return hl7243_is_vbuslowerr(hl,err);
}

static int mtk_hl7243_set_vbatovp_alarm(struct charger_device *chg_dev, u32 uV)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);
    int ret;

    ret = hl7243_set_vbatovp_alarm(hl, uV/1000);
    if (ret < 0)
        return ret;

    return ret;
}

static int mtk_hl7243_reset_vbatovp_alarm(struct charger_device *chg_dev)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);
    dev_err(hl->dev, "%s",__func__);
    return 0;
}

static int mtk_hl7243_set_vbusovp_alarm(struct charger_device *chg_dev, u32 uV)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);
    int ret;

    ret = hl7243_set_vbusovp_alarm(hl, uV/1000);
    if (ret < 0)
        return ret;

    return ret;
}

static int mtk_hl7243_reset_vbusovp_alarm(struct charger_device *chg_dev)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);
    dev_err(hl->dev, "%s",__func__);
    return 0;
}

static int mtk_hl7243_init_chip(struct charger_device *chg_dev)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);

    return hl7243_init_device(hl);
}


static int hl7243_init_protection(struct hl7243_chip *hl, int forward_work_mode)
{
    int ret = 0;

    pr_err("%s forward_work_mode=%d\n", __func__, forward_work_mode);
    if (forward_work_mode == CP_FORWARD_4_TO_1) {
        ret = hl7243_set_busovp_th(hl, 22000);
        ret = hl7243_set_busocp_th(hl, 4625);
        ret = hl7243_set_usbovp_th(hl, 21000);
        ret = hl7243_set_vwpcovp_th(hl, 22000);
    } else if (forward_work_mode == CP_FORWARD_2_TO_1) {
        ret = hl7243_set_busovp_th(hl, 11000);
        ret = hl7243_set_busocp_th(hl, 5000);
        ret = hl7243_set_usbovp_th(hl, 12000);
        ret = hl7243_set_vwpcovp_th(hl, 14000);
    } else {
        ret = hl7243_set_busovp_th(hl, 6000);
        ret = hl7243_set_busocp_th(hl, 5500);
        ret = hl7243_set_usbovp_th(hl, 6500);
        ret = hl7243_set_vwpcovp_th(hl, 6500);
    }

    return ret;
}

static int mtk_hl7243_device_init_chip(struct charger_device *chg_dev, int val)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);

    return hl7243_init_protection(hl, val);
}

static int hl7243_rev_chg_config(struct hl7243_chip *hl, bool enable)
{
    int ret = 0;

    dev_info(hl->dev,"%s:%d cfg.ibus_ocp_dis:%d ", __func__, enable, hl->cfg.ibus_ocp_dis);
    if(enable)
    {
        ret = hl7243_field_write(hl, IBUS_OCP_DIS, enable);
    }
    else
    {
        ret = hl7243_field_write(hl, IBUS_OCP_DIS, hl->cfg.ibus_ocp_dis);
    }
    if(ret != 0){
        dev_err(hl->dev, "%s failed %s ibus ocp protection\n", __func__, enable ? "disable" : "restore");
    }

    return ret;
}

__maybe_unused static int mtk_hl7243_rev_chg_config(struct charger_device *chg_dev, bool enable)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);
    int ret;
    ret = hl7243_rev_chg_config(hl,enable);

    return ret;
}

static int cp_get_adc_data(struct hl7243_chip *hl, int channel,  u32 *result)
{
    int ret = 0;

    ret = hl7243_get_adc_data(hl, channel, result);
    if (ret)
        dev_err(hl->dev, "failed get ADC value\n");
    return ret;
}

static int mtk_hl7243_cp_enable_adc(struct charger_device *chg_dev, bool en)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);
    int ret;

    ret = hl7243_enable_adc(hl, en);

    return ret;
}

static int mtk_hl7243_cp_dump_register(struct charger_device *chg_dev)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);
    int ret;

    ret = hl7243_dump_reg(hl);
    return ret;
}

static int mtk_hl7243_cp_reg_reset(struct charger_device *chg_dev)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);
    int ret;

    ret = _hl7243_write_reg(hl, 0x15, 0xc4);
    if (ret)
        pr_err("%s failed reg reset\n", __func__);

    return ret;
}

static int mtk_hl7243_cp_get_bypass_support(struct charger_device *chg_dev, bool *support)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);
    hl->bypass_support = false;
    *support = hl->bypass_support;

    return 0;
}

static int mtk_hl7243_is_bypass_enabled(struct charger_device *chg_dev, bool *en)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);
    int ret, mode;
    ret = hl7243_get_dev_mode(hl, &mode);
    if (mode == FWD_1_1_CHG_M) {
        *en = true;
    } else {
        *en = false;
    }
    pr_info("%s en:%d,mode:%d\n", __func__, *en, mode);

    return 0;
}


static int mtk_hl7243_set_pmic_ovp_en(struct charger_device *chg_dev, bool enable)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);
    int ret = 0;
    int gpio_enable_val = 0;
    int en = !!enable;

    if (gpio_is_valid(hl->pmic_ovp_en_gpio)) {
        ret = gpio_request(hl->pmic_ovp_en_gpio, "en_pmic_ovp_gpio");
        if (ret) {
            pr_err("%s: unable to request enable gpio [%d]\n",
                    __func__, hl->pmic_ovp_en_gpio);
            return 0;
        }

        ret = gpio_direction_output(hl->pmic_ovp_en_gpio, !en);
        if (ret) {
            pr_err("%s: cannot set direction for pmic ovp gpio [%d]\n",
                    __func__, hl->pmic_ovp_en_gpio);
            return 0;
        }
        gpio_enable_val = gpio_get_value(hl->pmic_ovp_en_gpio);
        pr_err("pmic ovp gpio val is :%d\n", gpio_enable_val);
        gpio_free(hl->pmic_ovp_en_gpio);
    }

    return ret;

    pr_info("%s\n",__func__);

}
static int hl7243_set_ss_timout(struct hl7243_chip *hl, int timeout)
{
    int ret = 0;
    u8 val;
    switch (timeout) {
        case 0:
            val = HL7243_SS_TIMEOUT_DISABLE;
            break;
        case 20:
            val = HL7243_SS_TIMEOUT_20MS;
            break;
        case 100:
            val = HL7243_SS_TIMEOUT_100MS;
            break;
        case 320:
            val = HL7243_SS_TIMEOUT_320MS;
            break;
        case 1280:
            val = HL7243_SS_TIMEOUT_1280MS;
            break;
        case 5120:
            val = HL7243_SS_TIMEOUT_5120MS;
            break;
        case 20480:
            val = HL7243_SS_TIMEOUT_20480MS;
            break;
        case 81920:
            val = HL7243_SS_TIMEOUT_81920MS;
            break;
        default:
            val = HL7243_SS_TIMEOUT_DISABLE;
            break;
    }
    ret = hl7243_field_write(hl, SS_TIMEOUT, val);
    if (ret)
        pr_err("%s failed set SS_TIMEOUT, ret=%d \n", __func__,ret);

    pr_err("%s: ss_timeout: %d val=%d\n", __func__, timeout, val);

    return ret;
}
static int hl7243_enable_busucp(struct hl7243_chip *hl, bool enable)
{
    int ret = 0;

    ret = hl7243_field_write(hl, IBUS_UCP_EN,enable);
    if (ret)
        pr_err("%s failed set IBUS_UCP_EN, ret=%d \n", __func__, ret);

    return ret;
}

static int hl7243_enable_usb_gate(struct hl7243_chip *hl, bool enable)
{
    int ret = 0;

    ret = hl7243_field_write(hl, OVPGATE_EN, enable);
    if (ret)
        pr_err("%s failed set OVPGATE_EN, ret=%d \n", __func__, ret);

    return ret;
}

static int mtk_hl7243_set_ibus_ucp_en(struct charger_device *chg_dev, bool enable)
{
    int ret,ss_timeout;
    struct hl7243_chip *hl = charger_get_data(chg_dev);

    if (enable)
        ss_timeout = 5120;
    else
        ss_timeout = 0;
    ret = hl7243_set_ss_timout(hl,ss_timeout);
    ret |= hl7243_enable_busucp(hl,enable);

    return ret;
}
static int mtk_hl7243_cp_chip_ok(struct charger_device *chg_dev, int *val)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);
    *val = hl->chip_ok;

    return 0;
}

static int mtk_hl7243_cp_get_tdie(struct charger_device *chg_dev, u32 *val)
{
    int ret;
    struct hl7243_chip *hl = charger_get_data(chg_dev);

    ret = hl7243_get_adc_data(hl, ADC_TDIE, val);
    if (ret)
        pr_err("%s failed to get cp tdie\n", __func__);
    pr_err("%s tdie=%d\n", __func__, *val);

    return ret;
}

static int mtk_hl7243_cp_get_fault_type(struct charger_device *chg_dev, u32 *val)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);

    if(hl)
        *val = hl->fault_type;
    else 
        *val = 0;

    return 0;

}
static int mtk_hl7243_cp_clear_fault_type(struct charger_device *chg_dev)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);
    hl->fault_type = 0;

    return 0;

}

static int mtk_hl7243_enable_acdrv_manual(struct charger_device *chg_dev, bool en)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);
    int ret=0;

    pr_info("%s manual mode %d\n",__func__, en);
    hl->acdrv_manual = en;
    if(!en)
    {
        ret = hl7243_field_write(hl, OVPGATE_EN,1);
        ret += hl7243_field_write(hl, WPCGATE_EN,1);
        if (ret)
            pr_err("%s failed to set ovpgate and wpcgate\n", __func__);
    }
    return ret;
}

static int mtk_hl7243_set_usb_gate_en(struct charger_device *chg_dev, bool enable)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);
    int ret=0;

    pr_err("%s: usb_gate_en %d\n", __func__, enable);
    if(!hl->acdrv_manual)
    {
        pr_err("%s: acdrv_manual==0 can not set usb gate\n",__func__);
    }else
    {
        ret = hl7243_field_write(hl, OVPGATE_EN, enable);
        if (ret)
            pr_err("%s failed to set ovp gate\n", __func__);
    }
    /*
    if(enable)
    {
        ret = hl7243_field_write(hl, WPCGATE_EN,0);
        ret += hl7243_field_write(hl, OVPGATE_EN,1);
    }else
    {
        ret = hl7243_field_write(hl, WPCGATE_EN,1);
        ret += hl7243_field_write(hl, OVPGATE_EN,1);
    }*/

    return ret;
}

static int mtk_hl7243_set_wpc_gate_en(struct charger_device *chg_dev, bool enable)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);
    int ret=0;

    pr_err("%s: wpc_gate_en %d\n", __func__, enable);
    if(!hl->acdrv_manual)
    {
        pr_err("%s: acdrv_manual==0 can not set wpc gate\n", __func__);
    }else
    {
        ret = hl7243_field_write(hl, WPCGATE_EN, enable);
        if (ret)
            pr_err("%s failed to set wpc gate\n", __func__);
    }
    /*
    if(enable)
    {
        ret = hl7243_field_write(hl, OVPGATE_EN,0);
        ret += hl7243_field_write(hl, WPCGATE_EN,1);
    }else
    {
        ret = hl7243_field_write(hl, OVPGATE_EN,1);
        ret += hl7243_field_write(hl, WPCGATE_EN,1);
    }*/
    return ret;
}

static int mtk_hl7243_cp_get_en_fail_status(struct charger_device *chg_dev, bool* enable)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);
    if(hl)
        *enable = hl->en_failed;
    else 
        *enable = 0;

    return 0;

}

static int mtk_hl7243_cp_set_en_fail_status(struct charger_device *chg_dev, bool enable)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);

    hl->en_failed = enable;
    return 0;
}

static int hl7243_revchg_init_protection(struct hl7243_chip *hl, int revchg_work_mode)
{
    int ret = 0;

    pr_err("%s revchg_work_mode=%d\n", __func__, revchg_work_mode);

    if (revchg_work_mode == REV_1_4_CON_M) {
        if (ret == 0) ret = hl7243_set_busovp_th(hl, 22000);
        if (ret == 0) ret = hl7243_set_busocp_th(hl, 4625);
        if (ret == 0) ret = hl7243_set_usbovp_th(hl, 21000);
        if (ret == 0) ret = hl7243_set_vwpcovp_th(hl, 22000);
    } else if (revchg_work_mode == REV_1_2_CON_M) {
        if (ret == 0) ret = hl7243_set_busovp_th(hl, 11000);
        if (ret == 0) ret = hl7243_set_busocp_th(hl, 5000);
        if (ret == 0) ret = hl7243_set_usbovp_th(hl, 12000);
        if (ret == 0) ret = hl7243_set_vwpcovp_th(hl, 14000);
    } else {
        if (ret == 0) ret = hl7243_set_busovp_th(hl, 6000);
        if (ret == 0) ret = hl7243_set_busocp_th(hl, 5500);
        if (ret == 0) ret = hl7243_set_usbovp_th(hl, 6500);
        if (ret == 0) ret = hl7243_set_vwpcovp_th(hl, 6500);
    }

    return ret;
}

static int hl7243_set_revchg(struct hl7243_chip *hl, bool enable, int cp_mode_set)
{
    int cp_mode = -1;
    int ret;
    bool ovpgate_enable = false, charging_enable = false;

    if (enable) {
        ret = hl7243_set_dev_mode(hl, cp_mode_set);
        if (ret) {
            pr_err("%s: failed to set dev mode, ret=%d\n", __func__, ret);
            return ret;
        }

        hl7243_enable_usb_gate(hl, true);

        ret = hl7243_revchg_init_protection(hl, cp_mode_set);
        if (ret) {
            pr_err("%s: init protection failed, ret=%d\n", __func__, ret);
            return ret;
        }

        ret = hl7243_get_usb_gate_enable(hl, &ovpgate_enable);
        if (ret) {
            pr_err("%s: failed to get usb gate enable, ret=%d\n", __func__, ret);
            ovpgate_enable = false;
        }

        ret = hl7243_get_dev_mode(hl, &cp_mode);
        if (ret) {
            pr_err("%s: failed to get dev mode, ret=%d\n", __func__, ret);
            cp_mode = -1;
        }

        pr_info("%s: cp_mode=%d, ovpgate_enable=%d\n", __func__, cp_mode, ovpgate_enable);

        if ((cp_mode != cp_mode_set) || (!ovpgate_enable))
            return -EINVAL;

    } else {
        hl7243_enable_charge(hl, false);
        hl7243_set_dev_mode(hl, FWD_4_1_CHG_M);

        ret = hl7243_get_dev_mode(hl, &cp_mode);
        if (ret) {
            pr_err("%s: failed to get dev mode, ret=%d\n", __func__, ret);
            cp_mode = -1;
        }

        ret = hl7243_check_charge_enabled(hl, &charging_enable);
        if (ret) {
            pr_err("%s: failed to check charge enable, ret=%d\n", __func__, ret);
            charging_enable = true;
        }

        pr_info("%s: cp_mode=%d, charging_enable=%d\n", __func__, cp_mode, charging_enable);

        if ((cp_mode != FWD_4_1_CHG_M) || charging_enable)
            return -EINVAL;
    }

    return 0;
}

#define CP_SET_REVCHG_RETRY 10
static int mtk_hl7243_cp_set_revchg(struct charger_device *chg_dev, bool enable, int cp_mode)
{
    struct hl7243_chip *hl = charger_get_data(chg_dev);
    int ret = 0;
    int i = 0;

    for (i = 0; i < CP_SET_REVCHG_RETRY; i++) {
        ret = hl7243_set_revchg(hl, enable, cp_mode);
        if (ret == 0)
            break;
        pr_err("%s failed set revchg, retry: %d\n", __func__, i);
        mdelay(20);
    }

    return ret;
}

static const struct charger_ops hl7243_chg_ops = {
    // .dev_mode = mtk_hl7243_dev_mode,
    .enable = mtk_hl7243_enable_chg,
    .is_enabled = mtk_hl7243_is_chg_enabled,
    .get_adc = mtk_hl7243_get_adc,
    .get_adc_accuracy = mtk_hl7243_get_adc_accuracy,
    .set_vbusovp = mtk_hl7243_set_vbusovp,
    .set_ibusocp = mtk_hl7243_set_ibusocp,
    .set_vbatovp = mtk_hl7243_set_vbatovp,
    .set_ibatocp = mtk_hl7243_set_ibatocp,
    .init_chip = mtk_hl7243_init_chip,
    .is_vbuslowerr = mtk_hl7243_is_vbuslowerr,
    .set_vbatovp_alarm = mtk_hl7243_set_vbatovp_alarm,
    .reset_vbatovp_alarm = mtk_hl7243_reset_vbatovp_alarm,
    .set_vbusovp_alarm = mtk_hl7243_set_vbusovp_alarm,
    .reset_vbusovp_alarm = mtk_hl7243_reset_vbusovp_alarm,

    .get_vbus_adc = mtk_hl7243_get_vbus_adc,
    .get_ibus_adc = mtk_hl7243_get_ibus_adc,
    // .get_ibat_adc = mtk_hl7243_get_ibat_adc, //not support
    .cp_set_mode = mtk_hl7243_dev_mode,
    .is_bypass_enabled = mtk_hl7243_is_bypass_enabled,
    .cp_device_init = mtk_hl7243_device_init_chip,
    .cp_enable_adc = mtk_hl7243_cp_enable_adc,
    .cp_get_bypass_support = mtk_hl7243_cp_get_bypass_support,
    .cp_dump_register = mtk_hl7243_cp_dump_register,
    .enable_acdrv_manual = mtk_hl7243_enable_acdrv_manual,
    .set_pmic_ovp_en = mtk_hl7243_set_pmic_ovp_en,
    .set_ibus_ucp_en = mtk_hl7243_set_ibus_ucp_en,
    .cp_chip_ok = mtk_hl7243_cp_chip_ok,
    .cp_get_tdie = mtk_hl7243_cp_get_tdie,
    .cp_get_fault_type = mtk_hl7243_cp_get_fault_type,
    .cp_clear_fault_type = mtk_hl7243_cp_clear_fault_type,
    .set_usb_gate_en = mtk_hl7243_set_usb_gate_en,
    .cp_enable_ovpgate = mtk_hl7243_set_usb_gate_en,
    .set_wpc_gate_en = mtk_hl7243_set_wpc_gate_en,
    .cp_enable_wpcgate = mtk_hl7243_set_wpc_gate_en,
    .cp_set_revchg = mtk_hl7243_cp_set_revchg,
    .cp_get_en_fail_status = mtk_hl7243_cp_get_en_fail_status,
    .cp_set_en_fail_status = mtk_hl7243_cp_set_en_fail_status,
    .cp_reg_reset = mtk_hl7243_cp_reg_reset,
};
#endif /*CONFIG_MTK_CLASS*/
/********************mtk charger interface end*************************************************/

/********************creat devices note start*************************************************/
// static ssize_t hl7243_show_registers(struct device *dev,
//                                      struct device_attribute *attr, char *buf)
// {
//     struct hl7243_chip *hl = dev_get_drvdata(dev);
//     u8 addr;
//     int val;
//     u8 tmpbuf[300];
//     int len;
//     int idx = 0;
//     int ret;

//     idx = snprintf(buf, PAGE_SIZE, "%s:\n", "hl7243");
//     for (addr = 0x0; addr <= HL7243_REGMAX; addr++) {
//         ret = regmap_read(hl->regmap, addr, &val);
//         if (ret == 0) {
//             len = snprintf(tmpbuf, PAGE_SIZE - idx,
//                            "Reg[%.2X] = 0x%.2x\n", addr, val);
//             memcpy(&buf[idx], tmpbuf, len);
//             idx += len;
//         }
//     }

//     return idx;
// }

// static ssize_t hl7243_store_register(struct device *dev,
//                                      struct device_attribute *attr, const char *buf, size_t count)
// {
//     struct hl7243_chip *hl = dev_get_drvdata(dev);
//     int ret;
//     unsigned int reg;
//     unsigned int val;

//     ret = sscanf(buf, "%x %x", &reg, &val);
//     if (ret == 2 && reg <= HL7243_REGMAX)
//         regmap_write(hl->regmap, (unsigned char)reg, (unsigned char)val);

//     return count;
// }

// static DEVICE_ATTR(registers, 0660, hl7243_show_registers, hl7243_store_register);

// static void hl7243_create_device_node(struct device *dev)
// {
//     device_create_file(dev, &dev_attr_registers);
// }
/********************creat devices note end*************************************************/


/*
* interrupt does nothing, just info event chagne, other module could get info
* through power supply interface
*/
#ifdef CONFIG_MTK_CLASS
static inline int status_reg_to_charger(enum hl7243_notify notify)
{
    switch (notify) {
    case HL7243_NOTIFY_IBUSOCP:
        return CHARGER_DEV_NOTIFY_IBUSOCP;
    case HL7243_NOTIFY_VBUSOVP:
        return CHARGER_DEV_NOTIFY_VBUS_OVP;
    case HL7243_NOTIFY_VBATOVP:
        return CHARGER_DEV_NOTIFY_BAT_OVP;
    case HL7243_NOTIFY_VOUTOVP:
        return CHARGER_DEV_NOTIFY_VOUTOVP;
    default:
        return -EINVAL;
        break;
    }
    return -EINVAL;
}
#endif /*CONFIG_MTK_CLASS*/
__maybe_unused
void hl7243_check_fault_status(struct hl7243_chip *hl)
{
    int ret;
    u8 flag = 0;
    int status;
    int i,j;
    u8 sts_buf[6];
#ifdef CONFIG_MTK_CLASS
    int noti;
#endif /*CONFIG_MTK_CLASS*/
    status = 0;
    for (i=0; i < ARRAY_SIZE(cp_intr_flag); i++) {
        ret = hl7243_read_block(hl, cp_intr_flag[i].reg, &flag, 1);
        status += flag;
        if(flag)
            dev_err(hl->dev,"intr reg:%x :0x%x\n",cp_intr_flag[i].reg,flag);
        for (j=0; j <  cp_intr_flag[i].len; j++) {
            if (flag & cp_intr_flag[i].bit[j].mask) {
                dev_err(hl->dev, "trigger :%s\n",cp_intr_flag[i].bit[j].name);
#ifdef CONFIG_MTK_CLASS
                hl->fault_type |= BIT(cp_intr_flag[i].bit[j].notify);
                noti = status_reg_to_charger(cp_intr_flag[i].bit[j].notify);
                if(noti >= 0) {
                    charger_dev_notify(hl->chg_dev, noti);
                }
#endif /*CONFIG_MTK_CLASS*/
            }
        }
    }
    if(status)
    {
        hl7243_field_read(hl, STATE_CHG_STS, &status);
        hl7243_read_block(hl,0x6,sts_buf,3);
        dev_err(hl->dev, "charge status :%d sts = %x %x %x \n",status,sts_buf[0],sts_buf[1],sts_buf[2]);
        hl7243_read_block(hl,0x22,sts_buf,6);
        dev_err(hl->dev, "vusb vwpc vbus %d %d %d \n",((sts_buf[0]<<4)|(sts_buf[1]&0xf))*625,((sts_buf[2]<<4)|(sts_buf[3]&0xf))*625,((sts_buf[4]<<4)|(sts_buf[5]&0xf))*625);
    }
}

static irqreturn_t hl7243_irq_handler(int irq, void *data)
{
    struct hl7243_chip *hl = data;

    dev_err(hl->dev, "INT OCCURED\n");

    hl7243_check_fault_status(hl);
    // hl7243_dump_reg(hl);

    power_supply_changed(hl->psy);

    return IRQ_HANDLED;
}

static int hl7243_register_interrupt(struct hl7243_chip *hl)
{
    int ret;

    if (gpio_is_valid(hl->irq_gpio)) {
        ret = gpio_request_one(hl->irq_gpio, GPIOF_DIR_IN,"hl7243_irq");
        if (ret) {
            dev_err(hl->dev, "failed to request hl7243_irq\n");
            return -EINVAL;
        }
        hl->irq = gpio_to_irq(hl->irq_gpio);
        if (hl->irq < 0) {
            dev_err(hl->dev, "failed to gpio_to_irq\n");
            return -EINVAL;
        }
    } else {
        dev_err(hl->dev, "irq gpio not provided\n");
        return -EINVAL;
    }

    if (hl->irq) {
        ret = devm_request_threaded_irq(&hl->client->dev, hl->irq,
                                        NULL, hl7243_irq_handler,
                                        IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                                        hl7243_irq_name[hl->mode], hl);

        if (ret < 0) {
            dev_err(hl->dev, "request irq for irq=%d failed, ret =%d\n",
                    hl->irq, ret);
            return ret;
        }
        enable_irq_wake(hl->irq);
    }

    return ret;
}
/********************interrupte end*************************************************/


/************************psy start**************************************/
static enum power_supply_property hl7243_charger_props[] = {
    POWER_SUPPLY_PROP_ONLINE,
    POWER_SUPPLY_PROP_PRESENT,
    POWER_SUPPLY_PROP_VOLTAGE_NOW,
    POWER_SUPPLY_PROP_CURRENT_NOW,
    POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
    POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
    POWER_SUPPLY_PROP_TEMP,
};

static int hl7243_charger_get_property(struct power_supply *psy,
                                    enum power_supply_property psp,
                                    union power_supply_propval *val)
{
    struct hl7243_chip *hl = power_supply_get_drvdata(psy);
    int result;
    int ret;

    switch (psp) {
    case POWER_SUPPLY_PROP_ONLINE:
        hl7243_check_charge_enabled(hl, &hl->charge_enabled);
        val->intval = hl->charge_enabled;
        break;
    case POWER_SUPPLY_PROP_VOLTAGE_NOW:
        ret = hl7243_get_adc_data(hl, ADC_VBUS, &result);
        if (!ret)
            hl->vbus_volt = result;
        val->intval = hl->vbus_volt;
        break;
    case POWER_SUPPLY_PROP_CURRENT_NOW:
        ret = hl7243_get_adc_data(hl, ADC_IBUS, &result);
        if (!ret)
            hl->ibus_curr = result;
        val->intval = hl->ibus_curr;
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
        ret = hl7243_get_adc_data(hl, ADC_VBAT, &result);
        if (!ret)
            hl->vbat_volt = result;
        val->intval = hl->vbat_volt;
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
        // ret = hl7243_get_adc_data(hl, ADC_IBAT, &result);
        //if (!ret)
        //    hl->ibat_curr = result;
        //val->intval = hl->ibat_curr;
        break;
    case POWER_SUPPLY_PROP_TEMP:
        ret = hl7243_get_adc_data(hl, ADC_TDIE, &result);
        if (!ret)
            hl->die_temp = result;
        val->intval = hl->die_temp;
        break;
    case POWER_SUPPLY_PROP_PRESENT:
        val->intval = hl->usb_present;
        break;
    default:
        return -EINVAL;
    }

    return 0;
}

static int hl7243_charger_set_property(struct power_supply *psy,
                                    enum power_supply_property prop,
                                    const union power_supply_propval *val)
{
    struct hl7243_chip *hl = power_supply_get_drvdata(psy);

    switch (prop) {
    case POWER_SUPPLY_PROP_ONLINE:
        //hl7243_enable_charge(hl, val->intval);
        dev_info(hl->dev, "POWER_SUPPLY_PROP_ONLINE: %s\n",
                val->intval ? "enable" : "disable");
        break;
    case POWER_SUPPLY_PROP_PRESENT:
        hl7243_set_present(hl, !!val->intval);
        dev_info(hl->dev, "POWER_SUPPLY_PROP_PRESENT: %s\n",
                val->intval ? "enable" : "disable");
        break;
    default:
        return -EINVAL;
    }

    return 0;
}

static int hl7243_charger_is_writeable(struct power_supply *psy,
                                    enum power_supply_property prop)
{
    int ret;

    switch (prop) {
    case POWER_SUPPLY_PROP_PRESENT:
        ret = 1;
        break;
    default:
        ret = 0;
        break;
    }
    return ret;
}

static int hl7243_psy_register(struct hl7243_chip *hl)
{
    hl->psy_cfg.drv_data = hl;
    hl->psy_cfg.of_node = hl->dev->of_node;

    hl->psy_desc.name = hl7243_psy_name[hl->mode];

    hl->psy_desc.type = POWER_SUPPLY_TYPE_MAINS;
    hl->psy_desc.properties = hl7243_charger_props;
    hl->psy_desc.num_properties = ARRAY_SIZE(hl7243_charger_props);
    hl->psy_desc.get_property = hl7243_charger_get_property;
    hl->psy_desc.set_property = hl7243_charger_set_property;
    hl->psy_desc.property_is_writeable = hl7243_charger_is_writeable;


    hl->psy = devm_power_supply_register(hl->dev,
                                        &hl->psy_desc, &hl->psy_cfg);
    if (IS_ERR(hl->psy)) {
        dev_err(hl->dev, "%s failed to register psy\n", __func__);
        return PTR_ERR(hl->psy);
    }

    dev_info(hl->dev, "%s power supply register successfully\n", hl->psy_desc.name);

    return 0;
}


/************************psy end**************************************/

// static int hl7243_set_work_mode(struct hl7243_chip *hl, int mode)
// {
//     hl->mode = mode;

//     dev_err(hl->dev,"work mode is %s\n", hl->mode == HL7243_STANDALONG
//             ? "standalone" : (hl->mode == HL7243_MASTER ? "master" : "slave"));

//     return 0;
// }
static int hl7243_parse_dt(struct hl7243_chip *hl, struct device *dev)
{
    struct device_node *np = dev->of_node;
    int i;
    int ret;
    struct {
        char *name;
        int *conv_data;
    } props[] = {
        {"hl,hl7243,vbat-ovp-dis", &(hl->cfg.vbat_ovp_dis)},
        {"hl,hl7243,vbat-ovp", &(hl->cfg.vbat_ovp)},
        {"hl,hl7243,vusb-ovp", &(hl->cfg.vusb_ovp)},
        {"hl,hl7243,vwpc-ovp", &(hl->cfg.vwpc_ovp)},
        {"hl,hl7243,vbus-ovp-dis", &(hl->cfg.vbus_ovp_dis)},
        {"hl,hl7243,vbus-ovp", &(hl->cfg.vbus_ovp)},
        {"hl,hl7243,vout-ovp-dis", &(hl->cfg.vout_ovp_dis)},
        {"hl,hl7243,vout-ovp", &(hl->cfg.vout_ovp)},
        {"hl,hl7243,ibus-ocp-dis", &(hl->cfg.ibus_ocp_dis)},
        {"hl,hl7243,ibus-ocp", &(hl->cfg.ibus_ocp)},
        {"hl,hl7243,ibus-ucp-en", &(hl->cfg.ibus_ucp_en)},
        {"hl,hl7243,ibus-ucp", &(hl->cfg.ibus_ucp)},
        {"hl,hl7243,track-ov-dis", &(hl->cfg.track_ov_dis)},
        {"hl,hl7243,track-ov", &(hl->cfg.track_ov)},
        {"hl,hl7243,track-uv-dis", &(hl->cfg.track_uv_dis)},
        {"hl,hl7243,track-uv", &(hl->cfg.track_uv)},
        {"hl,hl7243,fsw-set", &(hl->cfg.fsw_set)},
        {"hl,hl7243,ss-timeout", &(hl->cfg.ss_timeout)},
        {"hl,hl7243,wd-timer", &(hl->cfg.wd_timer)},
        {"hl,hl7243,dev_mode", &(hl->cfg.dev_mode)},
    };

    /* initialize data for optional properties */
    for (i = 0; i < ARRAY_SIZE(props); i++) {
        ret = of_property_read_u32(np, props[i].name,
                                props[i].conv_data);
        if (ret < 0) {
            dev_err(hl->dev, "can not read %s \n", props[i].name);
            return ret;
        }
    }

    of_property_read_u32(np, "ic_role", &hl->mode);
    dev_err(hl->dev,"work_mode is %d\n", hl->mode);

    hl->irq_gpio = of_get_named_gpio(np, "hl7243,intr_gpio", 0);
    if (!gpio_is_valid(hl->irq_gpio)) {
        dev_err(hl->dev, "fail to valid gpio : %d\n", hl->irq_gpio);
        return -EINVAL;
    }
    hl->pmic_ovp_en_gpio = of_get_named_gpio(np, "mt6375_ovp_en_gpio", 0);
    if (!gpio_is_valid(hl->pmic_ovp_en_gpio)) {
        pr_err("%s failed to parse mt6375_ovp_en_gpio\n", __func__);
        return -EINVAL;
    }
#ifdef CONFIG_MTK_CHARGER_V5P10
    if (of_property_read_string(np, "charger_name", &hl->chg_dev_name) < 0) {
        hl->chg_dev_name = "cp_master";
        dev_err(hl->dev, "no charger name\n");
    }
#elif defined(CONFIG_MTK_CHARGER_V4P19)
    if (of_property_read_string(np, "charger_name_v4_19", &hl->chg_dev_name) < 0) {
        hl->chg_dev_name = "charger";
        dev_err(hl->dev, "no charger name\n");
    }
#endif /*CONFIG_MTK_CHARGER_V4P19*/

    return 0;
}

// static struct of_device_id hl7243_charger_match_table[] = {
//     {   .compatible = "hl,hl7243-standalone",
//         .data = &hl7243_mode_data[HL7243_STANDALONG], },
//     {   .compatible = "hl,hl7243-master",
//         .data = &hl7243_mode_data[HL7243_MASTER], },
//     {   .compatible = "hl,hl7243-slave",
//         .data = &hl7243_mode_data[HL7243_SLAVE], },
// };


static int cp_vbus_get(struct hl7243_chip *hl,
    struct mtk_cp_sysfs_field_info *attr,
    int *val)
{
    int ret = 0;
    u32 data = 0;

    if (hl) {
        ret = cp_get_adc_data(hl, ADC_VBUS, &data);
        *val = data;
    } else {
        *val = 0;
    }
    dev_err(hl->dev, "cp_vbus=%d\n", *val);
    return 0;
}

static int cp_ibus_get(struct hl7243_chip *hl,
    struct mtk_cp_sysfs_field_info *attr,
    int *val)
{
    int ret = 0;
    u32 data = 0;

    if (hl) {
        ret = cp_get_adc_data(hl, ADC_IBUS, &data);
        *val = data;
    } else {
        *val = 0;
    }
    dev_err(hl->dev, "cp_ibus=%d\n", *val);
    return 0;
}

static int cp_tdie_get(struct hl7243_chip *hl,
    struct mtk_cp_sysfs_field_info *attr,
    int *val)
{
    int ret = 0;
    u32 data = 0;

    if (hl) {
        ret = cp_get_adc_data(hl, ADC_TDIE, &data);
        *val = data;
    } else
        *val = 0;
    //ln_err("%s %d\n", __func__, *val);
    return 0;
}

static int chip_ok_get(struct hl7243_chip *hl,
    struct mtk_cp_sysfs_field_info *attr,
    int *val)
{
    if (hl)
        *val = hl->chip_ok;
    else
        *val = 0;
    //ln_err("%s %d\n", __func__, *val);
    return 0;
}

static ssize_t cp_sysfs_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct power_supply *psy;
    struct hl7243_chip *hl;
    struct mtk_cp_sysfs_field_info *usb_attr;
    int val;
    ssize_t ret;

    ret = kstrtos32(buf, 0, &val);
    if (ret < 0)
        return ret;

    psy = dev_get_drvdata(dev);
    hl = (struct hl7243_chip *)power_supply_get_drvdata(psy);

    usb_attr = container_of(attr,
        struct mtk_cp_sysfs_field_info, attr);
    if (usb_attr->set != NULL)
        usb_attr->set(hl, usb_attr, val);

    return count;
}

static ssize_t cp_sysfs_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct power_supply *psy;
    struct hl7243_chip *hl;
    struct mtk_cp_sysfs_field_info *usb_attr;
    int val = 0;
    ssize_t count;

    psy = dev_get_drvdata(dev);
    hl = (struct hl7243_chip *)power_supply_get_drvdata(psy);

    usb_attr = container_of(attr,
        struct mtk_cp_sysfs_field_info, attr);
    if (usb_attr->get != NULL)
        usb_attr->get(hl, usb_attr, &val);

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

static int cp_sysfs_create_group(struct power_supply *psy)
{
    cp_sysfs_init_attrs();

    return sysfs_create_group(&psy->dev.kobj,
            &cp_sysfs_attr_group);
}

int hl7243_charger_probe(struct i2c_client *client)
{
    struct hl7243_chip *hl;
    // const struct of_device_id *match;
    // struct device_node *node = client->dev.of_node;
    int ret, i;

    dev_err(&client->dev, "%s (%s)\n", __func__, HL7243_DRV_VERSION);

    hl = devm_kzalloc(&client->dev, sizeof(struct hl7243_chip), GFP_KERNEL);
    if (!hl) {
        ret = -ENOMEM;
        goto err_kzalloc;
    }

    hl->dev = &client->dev;
    hl->client = client;

    hl->regmap = devm_regmap_init_i2c(client,
                                    &hl7243_regmap_config);
    if (IS_ERR(hl->regmap)) {
        dev_err(hl->dev, "Failed to initialize regmap\n");
        ret = PTR_ERR(hl->regmap);
        goto err_regmap_init;
    }

    for (i = 0; i < ARRAY_SIZE(hl7243_reg_fields); i++) {
        const struct reg_field *reg_fields = hl7243_reg_fields;

        hl->rmap_fields[i] =
            devm_regmap_field_alloc(hl->dev,
                                    hl->regmap,
                                    reg_fields[i]);
        if (IS_ERR(hl->rmap_fields[i])) {
            dev_err(hl->dev, "cannot allocate regmap field\n");
            ret = PTR_ERR(hl->rmap_fields[i]);
            goto err_regmap_field;
        }
    }

    ret = hl7243_detect_device(hl);
    if (ret < 0) {
        dev_err(hl->dev, "%s detect device fail\n", __func__);
        goto err_detect_dev;
    }

    //i2c_set_clientdata(client, hl);
    // hl7243_create_device_node(&(client->dev));

    // match = of_match_node(hl7243_charger_match_table, node);
    // if (match == NULL) {
    //     dev_err(hl->dev, "device tree match not found!\n");
    //     goto err_match_node;
    // }

    // hl7243_set_work_mode(hl, *(int *)match->data);
    // if (ret) {
    //     dev_err(hl->dev,"Fail to set work mode!\n");
    //     goto err_set_mode;
    // }

    ret = hl7243_parse_dt(hl, &client->dev);
    if (ret < 0) {
        dev_err(hl->dev, "%s parse dt failed(%d)\n", __func__, ret);
        goto err_parse_dt;
    }

    ret = hl7243_init_device(hl);
    if (ret < 0) {
        dev_err(hl->dev, "%s init device failed(%d)\n", __func__, ret);
        goto err_init_device;
    }

    ret = hl7243_psy_register(hl);
    if (ret < 0) {
        dev_err(hl->dev, "%s psy register failed(%d)\n", __func__, ret);
        goto err_register_psy;
    }

    ret = hl7243_register_interrupt(hl);
    if (ret < 0) {
        dev_err(hl->dev, "%s register irq fail(%d)\n",
                __func__, ret);
        goto err_register_irq;
    }

    ret = cp_sysfs_create_group(hl->psy);
    if (ret) {
        dev_err(hl->dev, "cp_sysfs_create_group failed %d\n", ret);
        return ret;
    }

#ifdef CONFIG_MTK_CLASS
    hl->chg_dev = charger_device_register(hl->chg_dev_name, &client->dev, hl, &hl7243_chg_ops, &hl->chg_props);
    if (IS_ERR_OR_NULL(hl->chg_dev)) {
        ret = PTR_ERR(hl->chg_dev);
        dev_err(hl->dev,"Fail to register charger!\n");
        goto err_register_mtk_charger;
    }
#endif /*CONFIG_MTK_CLASS*/

    hl->chip_ok = true;

    dev_err(hl->dev, "hl7243[%s] probe successfully!\n",
            hl->mode == HL7243_STANDALONG ? "standalong" :
            hl->mode == HL7243_MASTER ? "master" : "slave");
    return 0;

err_register_psy:
err_register_irq:
#ifdef CONFIG_MTK_CLASS
err_register_mtk_charger:
#endif /*CONFIG_MTK_CLASS*/
#ifdef CONFIG_HALOCP_DVCHG_CLASS
err_register_hl_charger:
#endif /*CONFIG_HALOCP_DVCHG_CLASS*/
err_init_device:
    power_supply_unregister(hl->psy);
err_detect_dev:
// err_match_node:
// err_set_mode:
err_parse_dt:
err_regmap_init:
err_regmap_field:
    devm_kfree(&client->dev, hl);
err_kzalloc:
    dev_err(&client->dev, "hl7243 probe fail\n");
    return ret;
}


// static void hl7243_charger_remove(struct i2c_client *client)
// {
//     struct hl7243_chip *hl = i2c_get_clientdata(client);

//     power_supply_unregister(hl->psy);
//     devm_kfree(&client->dev, hl);
//     return;
// }

// #ifdef CONFIG_PM_SLEEP
// static int hl7243_suspend(struct device *dev)
// {
//     struct hl7243_chip *hl = dev_get_drvdata(dev);

//     dev_info(hl->dev, "Suspend successfully!");
//     if (device_may_wakeup(dev)) {
//         enable_irq_wake(hl->irq);
//     }
//     disable_irq(hl->irq);

//     return 0;
// }
// static int hl7243_resume(struct device *dev)
// {
//     struct hl7243_chip *hl = dev_get_drvdata(dev);

//     dev_info(hl->dev, "Resume successfully!");
//     if (device_may_wakeup(dev)) {
//         disable_irq_wake(hl->irq);
//     }
//     enable_irq(hl->irq);

//     return 0;
// }

// static const struct dev_pm_ops hl7243_pm = {
//     SET_SYSTEM_SLEEP_PM_OPS(hl7243_suspend, hl7243_resume)
// };
// #endif

// static struct i2c_driver hl7243_charger_driver = {
//     .driver     = {
//         .name   = "hl7243",
//         .owner  = THIS_MODULE,
//         .of_match_table = hl7243_charger_match_table,
// #ifdef CONFIG_PM_SLEEP
//         .pm = &hl7243_pm,
// #endif
//     },
//     .probe      = hl7243_charger_probe,
//     .remove     = hl7243_charger_remove,
// };

// module_i2c_driver(hl7243_charger_driver);
EXPORT_SYMBOL(hl7243_charger_probe);

MODULE_DESCRIPTION("Halo HL7243 Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Halo Micro <joy.zhang@hmi.halomicro.com>");
