// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
*/
#define pr_fmt(fmt)	"[sc858x] %s: " fmt, __func__

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

#define SC858X_DRV_VERSION              "1.0.1_G"

#define FORWARD_4_1_CHARGER_MODE     0
#define FORWARD_2_1_CHARGER_MODE     1
#define FORWARD_1_1_CHARGER_MODE     2
#define FORWARD_1_1_CHARGER_MODE1    3
#define REVERSE_1_4_CONVERTER_MODE   4
#define REVERSE_1_2_CONVERTER_MODE   5
#define REVERSE_1_1_CONVERTER_MODE   6
#define REVERSE_1_1_CONVERTER_MODE1  7
#define CP_FORWARD_4_TO_1             0
#define CP_FORWARD_2_TO_1             1
#define CP_FORWARD_1_TO_1             2

enum {
    SC858X_STANDALONG = 0,
    SC858X_MASTER,
    SC858X_SLAVE,
};

static const char* sc858x_psy_name[] = {
    [SC858X_STANDALONG] = "cp_master",
    [SC858X_MASTER] = "cp_master",
    [SC858X_SLAVE] = "cp_slave",
};

static const char* sc858x_irq_name[] = {
    [SC858X_STANDALONG] = "sc858x-standalone-irq",
    [SC858X_MASTER] = "sc858x-master-irq",
    [SC858X_SLAVE] = "sc858x-slave-irq",
};

static int sc858x_mode_data[] = {
    [SC858X_STANDALONG] = SC858X_STANDALONG,
    [SC858X_MASTER] = SC858X_MASTER,
    [SC858X_SLAVE] = SC858X_SLAVE,
};

typedef enum {
    ADC_IBUS,
    ADC_VBUS,
    ADC_VUSB,
    ADC_VWPC,
    ADC_VOUT,
    ADC_VBAT,
    ADC_IBAT,
    ADC_TBAT,
    ADC_TDIE,
    ADC_MAX_NUM,
}ADC_CH;

static const int sc858x_adc_m[] = 
    {15625, 625, 625, 625, 125, 125, 375, 9766, 5};

static const int sc858x_adc_l[] = 
    {10000, 100, 100, 100, 100, 100, 100, 100000, 10};

enum sc858x_notify {
    SC858X_NOTIFY_OTHER = 0,
	SC858X_NOTIFY_IBUSUCPF,
	SC858X_NOTIFY_VBUSOVPALM,
	SC858X_NOTIFY_VBATOVPALM,
	SC858X_NOTIFY_IBUSOCP,
	SC858X_NOTIFY_VBUSOVP,
	SC858X_NOTIFY_IBATOCP,
	SC858X_NOTIFY_VBATOVP,
	SC858X_NOTIFY_VOUTOVP,
	SC858X_NOTIFY_VDROVP,
};

enum sc858x_error_stata {
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
    { .reg = 0x01, .len = 1, .bit = {
        {.mask = BIT(5), .name = "vbat ovp flag", .notify = SC858X_NOTIFY_VBATOVP},
        },
    },
    { .reg = 0x02, .len = 1, .bit = {
        {.mask = BIT(4), .name = "ibat ocp flag", .notify = SC858X_NOTIFY_IBATOCP},
        },
    },
    { .reg = 0x03, .len = 1, .bit = {
        {.mask = BIT(5), .name = "vusb ovp flag", .notify = SC858X_NOTIFY_VBUSOVP},
        },
    },
    { .reg = 0x04, .len = 1, .bit = {
        {.mask = BIT(5), .name = "vwpc ovp flag", .notify = SC858X_NOTIFY_VBUSOVP},
        },
    },
    { .reg = 0x06, .len = 1, .bit = {
        {.mask = BIT(5), .name = "ibus ocp flag", .notify = SC858X_NOTIFY_IBUSOCP},
        },
    },
    { .reg = 0x07, .len = 2, .bit = {
        {.mask = BIT(2), .name = "ibus ucp rising flag", .notify = SC858X_NOTIFY_OTHER},
        {.mask = BIT(0), .name = "ibus ucp falling flag", .notify = SC858X_NOTIFY_OTHER},
        },
    },
    { .reg = 0x08, .len = 1, .bit = {
        {.mask = BIT(3), .name = "pmid2out ovp flag", .notify = SC858X_NOTIFY_VBUSOVP},
        },
    },
    { .reg = 0x09, .len = 1, .bit = {
        {.mask = BIT(3), .name = "pmid2out uvp flag", .notify = SC858X_NOTIFY_VBUSOVP},
        },
    },
    { .reg = 0x0A, .len = 2, .bit = {
        {.mask = BIT(7), .name = "por flag", .notify = SC858X_NOTIFY_OTHER},
        {.mask = BIT(0), .name = "pin diag fail flag", .notify = SC858X_NOTIFY_OTHER},
        },
    },
    { .reg = 0x11, .len = 7, .bit = {
        {.mask = BIT(6), .name = "vout ok sw range flag", .notify = SC858X_NOTIFY_OTHER},
        {.mask = BIT(5), .name = "vout ok rev flag", .notify = SC858X_NOTIFY_OTHER},
        {.mask = BIT(4), .name = "vout ok chg flag", .notify = SC858X_NOTIFY_OTHER},
        {.mask = BIT(3), .name = "vout insert flag", .notify = SC858X_NOTIFY_OTHER},
        {.mask = BIT(2), .name = "vbus present flag", .notify = SC858X_NOTIFY_OTHER},
        {.mask = BIT(1), .name = "vwpc insert flag", .notify = SC858X_NOTIFY_OTHER},
        {.mask = BIT(0), .name = "vusb insert flag", .notify = SC858X_NOTIFY_OTHER},
        },
    },
    { .reg = 0x13, .len = 8, .bit = {
        {.mask = BIT(7), .name = "tsbat flt flag", .notify = SC858X_NOTIFY_OTHER},
        {.mask = BIT(6), .name = "tshut flag flag", .notify = SC858X_NOTIFY_OTHER},
        {.mask = BIT(5), .name = "ss timeout flag", .notify = SC858X_NOTIFY_OTHER},
        {.mask = BIT(4), .name = "wd timeout flag", .notify = SC858X_NOTIFY_OTHER},
        {.mask = BIT(3), .name = "conv ocp flag", .notify = SC858X_NOTIFY_OTHER},
        {.mask = BIT(2), .name = "ss fail flag", .notify = SC858X_NOTIFY_OTHER},
        {.mask = BIT(1), .name = "vbus ovp flag", .notify = SC858X_NOTIFY_VBUSOVP},
        {.mask = BIT(0), .name = "vout ovp flag", .notify = SC858X_NOTIFY_VBUSOVP},
        },
    },
};


enum sc858x_fields{
    DEVICE_VER,
    VBAT_OVP_DIS, VBAT_OVP_FLAG, VBAT_OVP,
    IBAT_OCP_DIS, IBAT_OCP_FLAG, IBAT_OCP,
    VUSB_OVP_FLAG, VUSB_OVP,
    VWPC_OVP_FLAG, VWPC_OVP,
    VBUS_OVP, VOUT_OVP,
    IBUS_OCP_DIS, IBUS_OCP_FLAG, IBUS_OCP,
    IBUS_UCP_DIS, IBUS_UCP_FALL_DG,
    PMID2OUT_OVP_DIS, PMID2OUT_OVP_FLAG, PMID2OUT_OVP,
    PMID2OUT_UVP_DIS, PMID2OUT_UVP_FLAG, PMID2OUT_UVP,
    CP_SWITCHING,
    VOUT_INSERT_STAT, VBUS_PRESET_STAT, VWPC_PRESENT_STAT, VUSB_PRESENT_STAT,
    CP_EN, QB_EN, ACDRV_MANUAL_EN, WPCGATE_EN, OVPGATE_EN, VBUS_PD_EN,
            VWPC_PD_EN, VUSB_PD_EN,
    FSW_SET,
    VBUS_INRANGE_DET_DIS, SS_TIMEOUT, WD_TIMEOUT,
    SYNC_FUNCTION_EN, SYNC_MASTER_EN, IBAT_SNS_RES, REG_RST, MODE,
    TSBAT_FLT_DIS, TSHUT_DIS, VWPC_OVP_DIS, VUSB_OVP_DIS, 
            VBUS_OVP_DIS, VOUT_OVP_DIS,
    ADC_EN, ADC_RATE,
    IBUS_RCP_DIS, IBUS_RCP_FLAG, VWPC_REMOVE_FLAG, VUSB_REMOVE_FLAG,
    DEVICE_ID,
    F_MAX_FIELDS,
};

struct sc858x_cfg_e {
    int bat_ovp_disable;
    int bat_ovp_threshold;
    int bat_ocp_disable;
    int bat_ocp_threshold;
    int usb_ovp_threshold;
    int wpc_ovp_threshold;
    int bus_ovp_threshold;
    int out_ovp_threshold;
    int bus_ocp_disable;
    int bus_ocp_threshold;
    int bus_ucp_fall_dg;
    int pmid2out_ovp_thershold;
    int pmid2out_uvp_thershold;
    int fsw_set;
    int ss_timeout;
    int wd_timeout;
    int mode;
    int tsbat_flt_disable;
    int tshut_disable;
    int wpc_ovp_disable;
    int usb_ovp_disable;
    int bus_ovp_disable;
    int out_ovp_disable;
    int sense_r_mohm;
};

struct sc858x_chip {
    struct device *dev;
    struct i2c_client *client;
    struct regmap *regmap;

    struct regmap_field *rmap_fields[F_MAX_FIELDS];

    struct sc858x_cfg_e cfg;

    int irq_gpio;
    int irq;

    bool usb_present;
    bool charge_enabled;
    bool qb_enabled;
    bool batt_present;
    bool vbus_present;
    bool vusb_present;
    bool vwpc_present;
    bool chg_en;
    int work_mode;
    int vbat_volt;
    int ibat_curr;
    int bat_temp;
    int vbus_volt;
    int ibus_curr;
    int vusb_volt;
    int vwpc_volt;
    int die_temp;
    int chip_ok;
    int product_name;

    int mode;
    int vbus_error;
    int acdrv_mode;
    int ovpgate_state;
    int wpcgate_state;
    int op_mode;

    struct power_supply_desc psy_desc;
    struct power_supply_config psy_cfg;
    struct power_supply *fc2_psy;
    struct charger_device *chg_dev;
    struct charger_properties chg_props;
};

struct mtk_cp_sysfs_field_info {
	struct device_attribute attr;
	enum cp_property prop;
	int (*set)(struct sc858x_chip *sc,
		struct mtk_cp_sysfs_field_info *attr, int val);
	int (*get)(struct sc858x_chip *sc,
		struct mtk_cp_sysfs_field_info *attr, int *val);
};

//REGISTER
static const struct reg_field sc858x_reg_fields[] = {
    /*reg00*/
    [DEVICE_VER] = REG_FIELD(0x00, 0, 7),
    /*reg01*/
    [VBAT_OVP_DIS] = REG_FIELD(0x01, 7, 7),
    [VBAT_OVP_FLAG] = REG_FIELD(0x01, 5, 5),
    [VBAT_OVP] = REG_FIELD(0x01, 0, 4),
    /*reg02*/
    [IBAT_OCP_DIS] = REG_FIELD(0x02, 7, 7),
    [IBAT_OCP_FLAG] = REG_FIELD(0x02, 4, 4),
    [IBAT_OCP] = REG_FIELD(0x02, 0, 3),
    /*reg03*/
    [VUSB_OVP_FLAG] = REG_FIELD(0x03, 5, 5),
    [VUSB_OVP] = REG_FIELD(0x03, 0, 3),
    /*reg04*/
    [VWPC_OVP_FLAG] = REG_FIELD(0x04, 5, 5),
    [VWPC_OVP] = REG_FIELD(0x04, 0, 3),
    /*reg05*/
    [VBUS_OVP] = REG_FIELD(0x05, 2, 7),
    [VOUT_OVP] = REG_FIELD(0x05, 0, 1),
    /*reg06*/
    [IBUS_OCP_DIS] = REG_FIELD(0x06, 7, 7),
    [IBUS_OCP_FLAG] = REG_FIELD(0x06, 5, 5),
    [IBUS_OCP] = REG_FIELD(0x06, 0, 4),
    /*reg07*/
    [IBUS_UCP_DIS] = REG_FIELD(0x07, 7, 7),
    [IBUS_UCP_FALL_DG] = REG_FIELD(0x07, 4, 5),
    /*reg08*/
    [PMID2OUT_OVP_DIS] = REG_FIELD(0x08, 7, 7),
    [PMID2OUT_OVP_FLAG] = REG_FIELD(0x08, 3, 3),
    [PMID2OUT_OVP] = REG_FIELD(0x08, 0, 2),
    /*reg09*/
    [PMID2OUT_UVP_DIS] = REG_FIELD(0x09, 7, 7),
    [PMID2OUT_UVP_FLAG] = REG_FIELD(0x09, 3, 3),
    [PMID2OUT_UVP] = REG_FIELD(0x09, 0, 2),
    /*reg0A*/
    [CP_SWITCHING] = REG_FIELD(0x0A, 1, 1),
    /*reg0b*/
    [CP_EN] = REG_FIELD(0x0B, 7, 7),
    [QB_EN] = REG_FIELD(0x0B, 6, 6),
    [ACDRV_MANUAL_EN] = REG_FIELD(0x0B, 5, 5),
    [WPCGATE_EN] = REG_FIELD(0x0B, 4, 4),
    [OVPGATE_EN] = REG_FIELD(0x0B, 3, 3),
    [VBUS_PD_EN] = REG_FIELD(0x0B, 2, 2),
    [VWPC_PD_EN] = REG_FIELD(0x0B, 1, 1),
    [VUSB_PD_EN] = REG_FIELD(0x0B, 0, 0),
    /*reg0c*/
    [FSW_SET] = REG_FIELD(0x0C, 3, 7),
    /*reg0d*/
    [VBUS_INRANGE_DET_DIS] = REG_FIELD(0x0D, 7, 7),
    [SS_TIMEOUT] = REG_FIELD(0x0D, 3, 6),
    [WD_TIMEOUT] = REG_FIELD(0x0D, 0, 2),
    /*reg0e*/
    [SYNC_FUNCTION_EN] = REG_FIELD(0x0E, 7, 7),
    [SYNC_MASTER_EN] = REG_FIELD(0x0E, 6, 6),
    [IBAT_SNS_RES] = REG_FIELD(0x0E, 4, 4),
    [REG_RST] = REG_FIELD(0x0E, 3, 3),
    [MODE] = REG_FIELD(0x0E, 0, 2),
    /*reg0f*/
    [TSBAT_FLT_DIS] = REG_FIELD(0x0F, 5, 5),
    [TSHUT_DIS] = REG_FIELD(0x0F, 4, 4),
    [VWPC_OVP_DIS] = REG_FIELD(0x0F, 3, 3),
    [VUSB_OVP_DIS] = REG_FIELD(0x0F, 2, 2),
    [VBUS_OVP_DIS] = REG_FIELD(0x0F, 1, 1),
    [VOUT_OVP_DIS] = REG_FIELD(0x0F, 0, 0),
    /*ret10*/
    [VOUT_INSERT_STAT] = REG_FIELD(0x10, 3, 3),
    [VBUS_PRESET_STAT] = REG_FIELD(0x10, 2, 2),
    [VWPC_PRESENT_STAT] = REG_FIELD(0x10, 1, 1),
    [VUSB_PRESENT_STAT] = REG_FIELD(0x10, 0, 0),
    /*reg15*/
    [ADC_EN] = REG_FIELD(0x15, 7, 7),
    [ADC_RATE] = REG_FIELD(0x15, 6, 6),
    /*reg40*/
    [IBUS_RCP_DIS] = REG_FIELD(0x40, 7, 7),
    [IBUS_RCP_FLAG] = REG_FIELD(0x40, 2, 2),
    [VWPC_REMOVE_FLAG] = REG_FIELD(0x40, 1, 1),
    [VUSB_REMOVE_FLAG] = REG_FIELD(0x40, 0, 0),
    /*reg6e*/
    [DEVICE_ID] = REG_FIELD(0x6E, 0, 7),
};

static const struct regmap_config sc858x_regmap_config = {
    .reg_bits = 8,
    .val_bits = 8,
};

/*********************************************************/
static int sc858x_field_read(struct sc858x_chip *sc,
                enum sc858x_fields field_id, int *val)
{
    int ret;

    ret = regmap_field_read(sc->rmap_fields[field_id], val);
    if (ret < 0) {
        pr_err("sc858x read field %d fail: %d\n", field_id, ret);
    }
    
    return ret;
}

static int sc858x_field_write(struct sc858x_chip *sc,
                enum sc858x_fields field_id, int val)
{
    int ret;
    
    ret = regmap_field_write(sc->rmap_fields[field_id], val);
    if (ret < 0) {
        pr_err("sc858x read field %d fail: %d\n", field_id, ret);
    }
    
    return ret;
}

static int sc858x_read_block(struct sc858x_chip *sc,
                int reg, uint8_t *val, int len)
{
    int ret;

    ret = regmap_bulk_read(sc->regmap, reg, val, len);
    if (ret < 0) {
        pr_err("sc858x read %02x block failed %d\n", reg, ret);
    }

    return ret;
}

static void sc858x_abnormal_charging_judge(unsigned int *data)
{
	if(data == NULL)
		return;
	if((data[16] & 0x08) == 0)
	{
		pr_err("VOUT UVLO\n");
	}
	if((data[16] & 0x3F) != 0x3D)
	{
		pr_err("VIN have problem\n");
	}
	if(data[10] & 0x08)
	{
		pr_err("VBUS_ERRORHI_STAT\n");
	}
	if(data[10] & 0x10)
	{
		pr_err("VBUS_ERRORLO_STAT\n");
	}
	if(data[7] & 0x01)
	{
		pr_err("IBUS_UCP_FALL_FLAG\n");
	}
	if(data[10] & 0x01)
	{
		pr_err("CBOOT dio\n");
	}
	if(data[1] & 0x20)
	{
		pr_err("VBAT_OVP\n");
	}
	if(data[2] & 0x10)
	{
		pr_err("IBAT_OCP\n");
	}
	if(data[3] & 0x20)
	{
		pr_err("VBUS_OVP\n");
	}
	if(data[4] & 0x20)
	{
		pr_err("VWPC_OVP\n");
	}
	if(data[6] & 0x20)
	{
		pr_err("IBUS_OCP\n");
	}
	if(data[7] & 0x01)
	{
		pr_err("IBUS_UCP\n");
	}
	if(data[8] & 0x08)
	{
		pr_err("PMID2OUT_OVP\n");
	}
	if(data[8] & 0x08)
	{
		pr_err("PMID2OUT_UVP\n");
	}
	if(data[10] & 0x80)
	{
		pr_err("POR_FLAG\n");
	}
	return;
}

/*******************************************************/
/* add the regs you want to dump here */
static unsigned int sc858x_reg_list[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
	0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x13, 0x14, 0x15, 0x16, 0x29, 0x6E,
};

static int sc858x_dump_register(struct sc858x_chip *sc)
{
	unsigned int data[100] = {0,};
	int i = 0, reg_num = ARRAY_SIZE(sc858x_reg_list);
	int len = 0, idx = 0, idx_total = 0, len_sysfs = 0;
	char buf_tmp[256] = {0,};

	for (i = 0; i < reg_num; i++) {
		regmap_read(sc->regmap, sc858x_reg_list[i], &data[i]);
		len = scnprintf(buf_tmp + strlen(buf_tmp), PAGE_SIZE - idx,
				"[0x%02X]=0x%02X,", sc858x_reg_list[i], data[i]);
		idx += len;

		if (((i + 1) % 8 == 0) || ((i + 1) == reg_num)) {
			pr_err("%s\n", buf_tmp);

			memset(buf_tmp, 0x0, sizeof(buf_tmp));

			idx_total += len_sysfs;
			idx = 0;
		}
	}
     if((data[10] & 0x02) == 0)
	{
		pr_err("cp switching stop, enter abnormal charging judge 0x%02x 0x%02x\n", data[10], data[10] & 0x02);
		sc858x_abnormal_charging_judge(data);
	}

	return 0;
}

static int sc858x_enable_charge(struct sc858x_chip *sc, bool enable)
{
    return sc858x_field_write(sc, CP_EN, !!enable);
}

static int sc858x_check_charge_enabled(struct sc858x_chip *sc, bool *enabled)
{
    int ret;
    int val;

    ret = sc858x_field_read(sc, CP_SWITCHING, &val);
    if (ret < 0) {
        return ret;
    }

    *enabled = !!val;
    return ret;
}

static int sc858x_enable_acdrv_manual(struct sc858x_chip *sc, bool enable)
{
	pr_err("manual=%d\n", enable);
	return sc858x_field_write(sc, ACDRV_MANUAL_EN, !!enable);
}

static int sc858x_enable_ovpgate(struct sc858x_chip *sc, bool enable)
{
	pr_err("ovpgate=%d\n", enable);
	return sc858x_field_write(sc, OVPGATE_EN, !!enable);
}

#if 0
static int sc858x_enable_qb(struct sc858x_chip *sc, bool enable)
{
    return sc858x_field_write(sc, QB_EN, !!enable);
}

static int sc858x_check_qb_enabled(struct sc858x_chip *sc, bool *enabled)
{
    int ret;
    int val;

    ret = sc858x_field_read(sc, QB_EN, &val);
    if (ret < 0) {
        return ret;
    }

    *enabled = !!val;
    return ret;
}
#endif

static int sc858x_set_operation_mode(struct sc858x_chip *sc, int mode)
{
    return sc858x_field_write(sc, MODE, mode);
}

static int sc858x_get_operation_mode(struct sc858x_chip *sc, int *mode)
{
    return sc858x_field_read(sc, MODE, mode);
}

static int sc858x_enable_adc(struct sc858x_chip *sc, bool enable)
{
    return sc858x_field_write(sc, ADC_EN, !!enable);
}

static int sc858x_get_adc_data(struct sc858x_chip *sc, int channel, int *result)
{
    int reg = 0x17 + channel * 2;
    u8 val[2] = {0};
    int ret;

    ret = sc858x_read_block(sc, reg, val, 2);
    if (ret) {
        return ret;
    }

    *result = (val[1] | (val[0] << 8)) * sc858x_adc_m[channel] / sc858x_adc_l[channel];

    return ret;
}

#define SC858x_BUS_OVP_41MODE_BASE          14000
#define SC858x_BUS_OVP_41MODE_LSB           200
#define SC858x_BUS_OVP_21MODE_BASE          7000
#define SC858x_BUS_OVP_21MODE_LSB           100
#define SC858x_BUS_OVP_11MODE_BASE          3500
#define SC858x_BUS_OVP_11MODE_LSB           50

/* SC8561_BUS_OVP */
static int sc858x_set_busovp_th(struct sc858x_chip *sc, int threshold)
{
	int ret = 0;
	int val;

	if (sc->op_mode == FORWARD_4_1_CHARGER_MODE
			|| sc->op_mode == REVERSE_1_4_CONVERTER_MODE) {
		if (threshold < 14000)
			threshold = 14000;
		else if (threshold > 22000)
			threshold = 22000;
		val = (threshold - SC858x_BUS_OVP_41MODE_BASE) / SC858x_BUS_OVP_41MODE_LSB;
	}
	else if (sc->op_mode == FORWARD_2_1_CHARGER_MODE
			|| sc->op_mode == REVERSE_1_2_CONVERTER_MODE) {
		if (threshold < 7000)
			threshold = 7000;
		else if (threshold > 13300)
			threshold = 13300;
		val = (threshold - SC858x_BUS_OVP_21MODE_BASE) / SC858x_BUS_OVP_21MODE_LSB;
	}
	else {
		if (threshold < 3500)
			threshold = 3500;
		else if (threshold > 5500)
			threshold = 5500;
		val = (threshold - SC858x_BUS_OVP_11MODE_BASE) / SC858x_BUS_OVP_11MODE_LSB;
	}

	pr_err("%s: mode = %d, val=%d,%d", __func__, sc->op_mode, threshold, val);

	ret = sc858x_field_write(sc, VBUS_OVP, val);
	if (ret) {
		pr_err("failed set VBUS_OVP, ret=%d \n", ret);
	}

	return ret;
}

#define SC858x_BUS_OCP_BASE                 2500
#define SC858x_BUS_OCP_LSB                  125

/* SC858x_BUS_OCP */
static int sc858x_set_busocp_th(struct sc858x_chip *sc, int threshold)
{
    int ret = 0;
    int val;

    if (threshold < SC858x_BUS_OCP_BASE)
        threshold = SC858x_BUS_OCP_BASE;
    val = (threshold - SC858x_BUS_OCP_BASE) / SC858x_BUS_OCP_LSB;

	pr_err("%s: val=%d,%d", __func__, threshold, val);

	ret = sc858x_field_write(sc, IBUS_OCP, val);
	if (ret) {
		pr_err("failed set IBUS_OCP, ret=%d \n", ret);
	}

    return ret;
}

#define SC858x_USB_OVP_BASE                 11000
#define SC858x_USB_OVP_LSB                  1000
#define SC858x_USB_OVP_6PV5                 0x0F

/* SC858x_USB_OVP */
static int sc858x_set_usbovp_th(struct sc858x_chip *sc, int threshold)
{
    int ret = 0;
    int val;

    if (threshold == 6500) {
        val = SC858x_USB_OVP_6PV5;
	} else {
        val = (threshold - SC858x_USB_OVP_BASE) / SC858x_USB_OVP_LSB;
	}

	pr_err("%s: val=%d,%d", __func__, threshold, val);

	ret = sc858x_field_write(sc, VUSB_OVP, val);
	if (ret) {
		pr_err("failed set IBUS_OCP, ret=%d \n", ret);
	}

    return ret;
}

/*******************************************************/
static int sc858x_init_device(struct sc858x_chip *sc);
static int sc858x_set_present(struct sc858x_chip *sc, bool present)
{
    sc->usb_present = present;

    if (present)
        sc858x_init_device(sc);
    else
        sc858x_enable_adc(sc, false);
    return 0;
}


static const enum power_supply_property sc858x_charger_props[] = {
    POWER_SUPPLY_PROP_PRESENT,
    POWER_SUPPLY_PROP_STATUS,
    POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

static int sc858x_charger_get_property(struct power_supply *psy,
                enum power_supply_property psp,
                union power_supply_propval *val)
{
    struct sc858x_chip *sc = power_supply_get_drvdata(psy);
    //int result;
    //int ret;
    //int reg_val;

    switch (psp) {
    case POWER_SUPPLY_PROP_STATUS:
        val->intval = 0;
        break;
    case POWER_SUPPLY_PROP_PRESENT:
        val->intval = sc->usb_present;
        break;
    case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	sc858x_get_adc_data(sc, ADC_VBAT, &sc->vbat_volt);
	val->intval = sc->vbat_volt;
	break;
    default:
        return -EINVAL;
    }

    return 0;
}

static int sc858x_charger_set_property(struct power_supply *psy,
                    enum power_supply_property prop,
                    const union power_supply_propval *val)
{
    struct sc858x_chip *sc = power_supply_get_drvdata(psy);
    
    switch (prop) {
    case POWER_SUPPLY_PROP_PRESENT:
        sc858x_set_present(sc, !!val->intval);
        break;
    default:
        return -EINVAL;
    }

    return 0;
}

static int sc858x_charger_is_writeable(struct power_supply *psy,
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

static int sc858x_psy_register(struct sc858x_chip *sc)
{
    sc->psy_cfg.drv_data = sc;
    sc->psy_cfg.of_node = sc->dev->of_node;

    sc->psy_desc.name = sc858x_psy_name[sc->mode];

    sc->psy_desc.type = POWER_SUPPLY_TYPE_MAINS;
    sc->psy_desc.properties = sc858x_charger_props;
    sc->psy_desc.num_properties = ARRAY_SIZE(sc858x_charger_props);
    sc->psy_desc.get_property = sc858x_charger_get_property;
    sc->psy_desc.set_property = sc858x_charger_set_property;
    sc->psy_desc.property_is_writeable = sc858x_charger_is_writeable;


    sc->fc2_psy = devm_power_supply_register(sc->dev, 
            &sc->psy_desc, &sc->psy_cfg);
    if (IS_ERR(sc->fc2_psy)) {
        pr_err("failed to register fc2_psy\n");
        return PTR_ERR(sc->fc2_psy);
    }

    pr_info("%s power supply register successfully\n", sc->psy_desc.name);

    return 0;
}

static int sc858x_parse_dt(struct sc858x_chip *sc, struct device *dev)
{
    struct device_node *np = dev->of_node;
    int i;
    int ret;
    struct {
        char *name;
        int *conv_data;
    } props[] = {
        {"sc,sc858x,bat-ovp-disable", &(sc->cfg.bat_ovp_disable)},
        {"sc,sc858x,bat-ovp-threshold", &(sc->cfg.bat_ovp_threshold)},
        {"sc,sc858x,bat-ocp-disable", &(sc->cfg.bat_ocp_disable)},
        {"sc,sc858x,bat-ocp-threshold", &(sc->cfg.bat_ocp_threshold)},
        {"sc,sc858x,usb-ovp-threshold", &(sc->cfg.usb_ovp_threshold)},
        {"sc,sc858x,wpc-ovp-threshold", &(sc->cfg.wpc_ovp_threshold)},
        {"sc,sc858x,bus-ovp-threshold", &(sc->cfg.bus_ovp_threshold)},
        {"sc,sc858x,out-ovp-threshold", &(sc->cfg.out_ovp_threshold)},
        {"sc,sc858x,bus-ocp-disable", &(sc->cfg.bus_ocp_disable)},
        {"sc,sc858x,bus-ocp-threshold", &(sc->cfg.bus_ocp_threshold)},
        {"sc,sc858x,bus-ucp-fall-dg", &(sc->cfg.bus_ucp_fall_dg)},
        {"sc,sc858x,pmid2out-ovp-threshold", &(sc->cfg.pmid2out_ovp_thershold)},
        {"sc,sc858x,pmid2out-uvp-threshold", &(sc->cfg.pmid2out_uvp_thershold)},
        {"sc,sc858x,fsw-set", &(sc->cfg.fsw_set)},
        {"sc,sc858x,ss-timeout", &(sc->cfg.ss_timeout)},
        {"sc,sc858x,wd-timeout", &(sc->cfg.wd_timeout)},
        {"sc,sc858x,mode", &(sc->cfg.mode)},
        {"sc,sc858x,tsbat-flt-disable", &(sc->cfg.tsbat_flt_disable)},
        {"sc,sc858x,tshut-disable", &(sc->cfg.tshut_disable)},
        {"sc,sc858x,wpc-ovp-disable", &(sc->cfg.wpc_ovp_disable)},
        {"sc,sc858x,usb-ovp-disable", &(sc->cfg.usb_ovp_disable)},
        {"sc,sc858x,bus-ovp-disable", &(sc->cfg.bus_ovp_disable)},
        {"sc,sc858x,out-ovp-disable", &(sc->cfg.out_ovp_disable)},
        {"sc,sc858x,sense-r-mohm", &(sc->cfg.sense_r_mohm)},
    };

    /* initialize data for optional properties */
    for (i = 0; i < ARRAY_SIZE(props); i++) {
        ret = of_property_read_u32(np, props[i].name,
                        props[i].conv_data);
        if (ret < 0) {
            pr_err("can not read %s \n", props[i].name);
            continue;
        }
    }

    sc->irq_gpio = of_get_named_gpio(np, "sc,sc858x,irq-gpio", 0);
    if (!gpio_is_valid(sc->irq_gpio)) {
        dev_err(sc->dev,"fail to valid gpio : %d\n", sc->irq_gpio);
        return -EINVAL;
    }

    /* change init parameter for GL version 67W charge */
    if (sc->product_name == DUCHAMP_GL) {
        sc->cfg.usb_ovp_threshold = 1; /*11000 + val x 1000*/
	sc->cfg.wpc_ovp_threshold = 1; /*11000 + val x 1000*/
	sc->cfg.bus_ovp_threshold = 40; /*7000 + val x 100*/
	sc->cfg.bus_ocp_threshold = 31; /*2500 + val x 125*/
    }

    return ret;
}

static int sc858x_init_device(struct sc858x_chip *sc)
{
    int ret = 0;
    int i;
    struct {
        enum sc858x_fields field_id;
        int conv_data;
    } props[] = {
        {VBAT_OVP_DIS, sc->cfg.bat_ovp_disable},
        {VBAT_OVP, sc->cfg.bat_ovp_threshold},
        {IBAT_OCP_DIS, sc->cfg.bat_ocp_disable},
        {IBAT_OCP, sc->cfg.bat_ocp_threshold},
        {VUSB_OVP, sc->cfg.usb_ovp_threshold},
        {VWPC_OVP, sc->cfg.wpc_ovp_threshold},
        {VBUS_OVP, sc->cfg.bus_ovp_threshold},
        {VOUT_OVP, sc->cfg.out_ovp_threshold},
        {IBUS_OCP_DIS, sc->cfg.bus_ocp_disable},
        {IBUS_OCP, sc->cfg.bus_ocp_threshold},
        {IBUS_UCP_FALL_DG, sc->cfg.bus_ucp_fall_dg},
        {PMID2OUT_OVP, sc->cfg.pmid2out_ovp_thershold},
        {PMID2OUT_UVP, sc->cfg.pmid2out_uvp_thershold},
        {FSW_SET, sc->cfg.fsw_set},
        {SS_TIMEOUT, sc->cfg.ss_timeout},
        {WD_TIMEOUT, sc->cfg.wd_timeout},
        {MODE, sc->cfg.mode},
        {TSBAT_FLT_DIS, sc->cfg.tsbat_flt_disable},
        {TSHUT_DIS, sc->cfg.tshut_disable},
        {VWPC_OVP_DIS, sc->cfg.wpc_ovp_disable},
        {VUSB_OVP_DIS, sc->cfg.usb_ovp_disable},
        {VBUS_OVP_DIS, sc->cfg.bus_ovp_disable},
        {VOUT_OVP_DIS, sc->cfg.out_ovp_disable},
        {IBAT_SNS_RES, sc->cfg.sense_r_mohm},
    };

    ret = sc858x_field_write(sc, REG_RST, 1);

    for (i = 0; i < ARRAY_SIZE(props); i++) {
        ret = sc858x_field_write(sc, props[i].field_id, props[i].conv_data);
    }

    if (sc->mode == SC858X_MASTER) {
        sc858x_field_write(sc, SYNC_FUNCTION_EN, 1);
        sc858x_field_write(sc, SYNC_MASTER_EN, 1);
    } else if (sc->mode == SC858X_SLAVE) {
        sc858x_field_write(sc, SYNC_FUNCTION_EN, 1);
        sc858x_field_write(sc, SYNC_MASTER_EN, 0);
    }

    sc858x_enable_adc(sc, true);

    ret = sc858x_field_write(sc, IBUS_RCP_DIS, 1);

    regmap_write(sc->regmap, 0x7C, 0x01);

    sc858x_enable_acdrv_manual(sc, false);
    sc858x_enable_ovpgate(sc, true);

    ret = sc858x_dump_register(sc);
    if (ret)
	    pr_err("failed dump registers, ret=%d \n", ret);

    return ret;
}

static ssize_t sc858x_show_registers(struct device *dev,
                struct device_attribute *attr, char *buf)
{
    struct sc858x_chip *sc = dev_get_drvdata(dev);
    u8 addr;
    int val;
    u8 tmpbuf[300];
    int len;
    int idx = 0;
    int ret;

    idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sc858x");
    for (addr = 0x0; addr <= 0x7c; addr++) {
        if (addr <= 0x29 || addr == 0x40 || addr == 0x6e || addr == 0x7c) {
            ret = regmap_read(sc->regmap, addr, &val);
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

static ssize_t sc858x_store_register(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct sc858x_chip *sc = dev_get_drvdata(dev);
    int ret;
    unsigned int reg;
    unsigned int val;

    ret = sscanf(buf, "%x %x", &reg, &val);
    if (ret == 2 && (reg <= 0x29 || reg == 0x40 || reg == 0x6e || reg == 0x7c))
        regmap_write(sc->regmap, (unsigned char)reg, (unsigned char)val);

    return count;
}


static DEVICE_ATTR(registers, 0660, sc858x_show_registers, sc858x_store_register);

static void sc858x_create_device_node(struct device *dev)
{
    device_create_file(dev, &dev_attr_registers);
}

static void sc858x_check_fault_status(struct sc858x_chip *sc)
{
    int ret;
    int flag = 0;
    int i,j;

    for (i=0;i < ARRAY_SIZE(cp_intr_flag);i++) {
        ret = regmap_read(sc->regmap, cp_intr_flag[i].reg, &flag);
        for (j=0; j <  cp_intr_flag[i].len; j++) {
            if (flag & cp_intr_flag[i].bit[j].mask) {
                dev_err(sc->dev,"trigger :%s\n",cp_intr_flag[i].bit[j].name);
            }
        }
    }
}

static irqreturn_t sc858x_irq_handler(int irq, void *dev_id)
{
    struct sc858x_chip *sc = dev_id;

    dev_err(sc->dev,"INT OCCURED\n");

    sc858x_check_fault_status(sc);

    power_supply_changed(sc->fc2_psy);

    return IRQ_HANDLED;
}

static int sc858x_register_interrupt(struct sc858x_chip *sc)
{
    int ret;

    if (gpio_is_valid(sc->irq_gpio)) {
        ret = gpio_request_one(sc->irq_gpio, GPIOF_DIR_IN,"sc858x_irq");
        if (ret) {
            dev_err(sc->dev,"failed to request sc858x_irq\n");
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
                NULL, sc858x_irq_handler,
                IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                sc858x_irq_name[sc->mode], sc);

        if (ret < 0) {
            dev_err(sc->dev,"request irq for irq=%d failed, ret =%d\n",
                            sc->irq, ret);
            return ret;
        }
        enable_irq_wake(sc->irq);
    }

    return ret;
}

static void determine_initial_status(struct sc858x_chip *sc)
{
    if (sc->client->irq)
        sc858x_irq_handler(sc->client->irq, sc);
}

static struct of_device_id sc858x_of_match[] = {
    {
        .compatible = "sc,sc858x-standalone",
        .data = &sc858x_mode_data[SC858X_STANDALONG],
    },
    {
        .compatible = "sc,sc858x-master",
        .data = &sc858x_mode_data[SC858X_MASTER],
    },
    {
        .compatible = "sc,sc858x-slave",
        .data = &sc858x_mode_data[SC858X_SLAVE],
    },
    {},
};


static int ops_cp_enable_charge(struct charger_device *chg_dev, bool enable)
{
	struct sc858x_chip *sc = charger_get_data(chg_dev);
	int ret = 0;

	ret = sc858x_enable_charge(sc, enable);
	if (ret)
		pr_err("failed enable cp charge\n");

	sc->chg_en = enable;
	pr_err("enable=%d \n", enable);

	return ret;
}

static int ops_cp_get_charge_enabled(struct charger_device *chg_dev, bool *enable)
{
	struct sc858x_chip *sc = charger_get_data(chg_dev);
	int ret = 0;

	ret = sc858x_check_charge_enabled(sc, enable);
	if (ret)
		pr_err("failed to get cp charge enable\n");

	pr_err("enable=%d \n", *enable);

	return ret;
}

static int ops_cp_get_vbus(struct charger_device *chg_dev, u32 *val)
{
	struct sc858x_chip *sc = charger_get_data(chg_dev);
	int ret = 0;
	int channel = ADC_VBUS;

	ret = sc858x_get_adc_data(sc, channel, val);
	if (ret)
		pr_err("failed to get cp charge vbus\n");

	pr_err("vbus=%d channel=%d\n", *val, channel);

	return ret;
}

static int ops_cp_get_ibus(struct charger_device *chg_dev, u32 *val)
{
	struct sc858x_chip *sc = charger_get_data(chg_dev);
	int ret = 0;
	int channel = ADC_IBUS;

	ret = sc858x_get_adc_data(sc, channel, val);
	if (ret)
		pr_err("failed to get cp charge ibus\n");

	pr_err("ibus=%d channel=%d\n", *val, channel);

	return ret;
}

#if 0
static int ops_cp_get_vbatt(struct charger_device *chg_dev, u32 *val)
{
	struct sc858x_chip *sc = charger_get_data(chg_dev);
	int ret = 0;
	int channel = ADC_VBAT;

	ret = sc858x_get_adc_data(sc, channel, val);
	if (ret)
		pr_err("failed to get cp charge ibatt\n");

	pr_err("vbatt=%d channel=%d\n", *val, channel);

	return ret;
}
#endif

static int ops_cp_get_ibatt(struct charger_device *chg_dev, u32 *val)
{
	struct sc858x_chip *sc = charger_get_data(chg_dev);
	int ret = 0;
	int channel = ADC_IBAT;

	ret = sc858x_get_adc_data(sc, channel, val);
	if (ret)
		pr_err("failed to get cp charge ibatt\n");

	pr_err("ibatt=%d channel=%d\n", *val, channel);

	return ret;
}

static int ops_cp_set_mode(struct charger_device *chg_dev, int value)
{
	struct sc858x_chip *sc = charger_get_data(chg_dev);
	int ret = 0;

	ret = sc858x_set_operation_mode(sc, value);
	if (ret)
		pr_err("failed to set cp charge mode\n");

	sc->op_mode = value;

	pr_err("set_mode=%d\n", value);

	return ret;
}

static int sc858x_get_bypass_enable(struct sc858x_chip *sc, bool *enable)
{
	int ret = 0;
	int mode;

	ret = sc858x_get_operation_mode(sc,  &mode);
	if (ret < 0) {
		pr_err("failed to read device id\n");
		return ret;
	}

	if (mode == 2) {
		*enable = true;
	} else {
		*enable = false;
	}

	pr_err("op_mode=%d, bypass_mode=%d", mode, *enable);

	return ret;
}

static int ops_cp_is_bypass_enabled(struct charger_device *chg_dev, bool *enabled)
{
	struct sc858x_chip *sc = charger_get_data(chg_dev);
	int ret = 0;

	ret = sc858x_get_bypass_enable(sc, enabled);
	if (ret)
		pr_err("failed to get cp charge bypass enable\n");

	return ret;
}

static int sc858x_init_protection(struct sc858x_chip *sc, int forward_work_mode)
{
	int ret = 0;

	if (forward_work_mode == CP_FORWARD_4_TO_1) {
		ret = sc858x_set_busovp_th(sc, 22000);
		ret = sc858x_set_busocp_th(sc, 4375);
		ret = sc858x_set_usbovp_th(sc, 22000);
	} else if (forward_work_mode == CP_FORWARD_2_TO_1) {
		ret = sc858x_set_busovp_th(sc, 11000);
		if (sc->product_name == DUCHAMP_GL) {
			ret = sc858x_set_busocp_th(sc, 6375);
		} else {
			ret = sc858x_set_busocp_th(sc, 5000);
		}
		ret = sc858x_set_usbovp_th(sc, 14000);
	} else {
		ret = sc858x_set_busovp_th(sc, 6000);
		ret = sc858x_set_busocp_th(sc, 5500);
		ret = sc858x_set_usbovp_th(sc, 6500);
	}

	return ret;
}

static int ops_cp_device_init(struct charger_device *chg_dev, int value)
{
	struct sc858x_chip *sc = charger_get_data(chg_dev);
	int ret = 0;

        //ret = sc858x_init_device(sc);
	ret = sc858x_init_protection(sc, value);
	if (ret)
		pr_err("failed to init cp charge\n");

	return ret;
}

static int ops_cp_enable_adc(struct charger_device *chg_dev, bool enable)
{
	struct sc858x_chip *sc = charger_get_data(chg_dev);
	int ret = 0;

	ret = sc858x_enable_adc(sc, enable);
	if (ret)
		pr_err("failed to enable cp charge adc\n");

	pr_err("enable_adc=%d\n", enable);

	return ret;
}

static int ops_cp_dump_register(struct charger_device *chg_dev)
{
	struct sc858x_chip *sc = charger_get_data(chg_dev);
	int ret = 0;

	ret = sc858x_dump_register(sc);
	if (ret)
		pr_err("failed dump registers, ret=%d \n", ret);

	return ret;
}

static int ops_enable_acdrv_manual(struct charger_device *chg_dev, bool enable)
{
	struct sc858x_chip *sc = charger_get_data(chg_dev);
	int ret = 0;

	ret = sc858x_enable_acdrv_manual(sc, enable);
	if (ret)
		pr_err("failed enable cp acdrv manual\n");

	return ret;
}

static int ops_cp_get_bypass_support(struct charger_device *chg_dev, bool *enabled)
{
	*enabled = 0;
	pr_err("bypass_support=%d\n", *enabled);

	return 0;
}

static const struct charger_ops sc858x_chg_ops = {
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

static int sc858x_register_charger(struct sc858x_chip *sc, int work_mode)
{
	switch (work_mode) {
	case SC858X_STANDALONG:
		sc->chg_dev = charger_device_register("cp_master", sc->dev, sc, &sc858x_chg_ops, &sc->chg_props);
		break;
	case SC858X_SLAVE:
		sc->chg_dev = charger_device_register("cp_slave", sc->dev, sc, &sc858x_chg_ops, &sc->chg_props);
		break;
	case SC858X_MASTER:
		sc->chg_dev = charger_device_register("cp_master", sc->dev, sc, &sc858x_chg_ops, &sc->chg_props);
		break;
	default:
		dev_err(sc->dev, "not support work_mode\n");
		return -EINVAL;
	}

	return 0;
}

static int cp_vbus_get(struct sc858x_chip *sc,
	struct mtk_cp_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u32 data = 0;

	if (sc) {
		ret = sc858x_get_adc_data(sc, ADC_VBUS, &data);
		*val = data;
	} else
		*val = 0;
	chr_err("%s: cp_vbus=%d\n",  __func__, *val);
	return 0;
}

static int cp_ibus_get(struct sc858x_chip *sc,
	struct mtk_cp_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u32 data = 0;

	if (sc) {
		ret = sc858x_get_adc_data(sc, ADC_IBUS, &data);
		*val = data;
	} else
		*val = 0;
	chr_err("%s: cp_ibus=%d\n", __func__, *val);
	return 0;
}

static int cp_tdie_get(struct sc858x_chip *sc,
	struct mtk_cp_sysfs_field_info *attr,
	int *val)
{
	int ret = 0;
	u32 data = 0;

	if (sc) {
		ret = sc858x_get_adc_data(sc, ADC_TDIE, &data);
		*val = data;
	} else
		*val = 0;
	//ln_err("%s %d\n", __func__, *val);
	return 0;
}

static int chip_ok_get(struct sc858x_chip *sc,
	struct mtk_cp_sysfs_field_info *attr,
	int *val)
{
	if (sc)
		*val = sc->chip_ok;
	else
		*val = 0;
	//ln_err("%s %d\n", __func__, *val);
	return 0;
}

static ssize_t cp_sysfs_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *psy;
	struct sc858x_chip *sc;
	struct mtk_cp_sysfs_field_info *usb_attr;
	int val;
	ssize_t ret;

	ret = kstrtos32(buf, 0, &val);
	if (ret < 0)
		return ret;

	psy = dev_get_drvdata(dev);
	sc = (struct sc858x_chip *)power_supply_get_drvdata(psy);

	usb_attr = container_of(attr,
		struct mtk_cp_sysfs_field_info, attr);
	if (usb_attr->set != NULL)
		usb_attr->set(sc, usb_attr, val);

	return count;
}

static ssize_t cp_sysfs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *psy;
	struct sc858x_chip *sc;
	struct mtk_cp_sysfs_field_info *usb_attr;
	int val = 0;
	ssize_t count;

	psy = dev_get_drvdata(dev);
	sc = (struct sc858x_chip *)power_supply_get_drvdata(psy);

	usb_attr = container_of(attr,
		struct mtk_cp_sysfs_field_info, attr);
	if (usb_attr->get != NULL)
		usb_attr->get(sc, usb_attr, &val);

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

static int sc858x_charger_probe(struct i2c_client *client,
                    const struct i2c_device_id *id)
{
    struct sc858x_chip *sc;
    const struct of_device_id *match;
    struct device_node *node = client->dev.of_node;
    int ret = 0;
    int i;

    dev_err(&client->dev, "%s (%s)\n", __func__, SC858X_DRV_VERSION);

    sc = devm_kzalloc(&client->dev, sizeof(struct sc858x_chip), GFP_KERNEL);
    if (!sc)
	    return -ENOMEM;

    sc->product_name = DUCHAMP_CN;
    dev_err(&client->dev, "%s: product_name=%d\n", __func__, sc->product_name);

    sc->dev = &client->dev;
    sc->client = client;

    sc->regmap = devm_regmap_init_i2c(client,
                            &sc858x_regmap_config);
    if (IS_ERR(sc->regmap)) {
        dev_err(sc->dev, "Failed to initialize regmap\n");
        return -EINVAL;
    }

    for (i = 0; i < ARRAY_SIZE(sc858x_reg_fields); i++) {
        const struct reg_field *reg_fields = sc858x_reg_fields;

        sc->rmap_fields[i] =
            devm_regmap_field_alloc(sc->dev,
                        sc->regmap,
                        reg_fields[i]);
        if (IS_ERR(sc->rmap_fields[i])) {
            dev_err(sc->dev, "cannot allocate regmap field\n");
            return PTR_ERR(sc->rmap_fields[i]);
        }
    }

    i2c_set_clientdata(client, sc);
    sc858x_create_device_node(&(client->dev));

    match = of_match_node(sc858x_of_match, node);
    if (match == NULL) {
        pr_err("device tree match not found!\n");
        return -ENODEV;
    }

    sc->mode = *(int *)match->data;

    ret = sc858x_parse_dt(sc, &client->dev);
    if (ret)
        goto err_parse_dt;

    ret = sc858x_init_device(sc);
    if (ret)
        goto err_init_device;
    
    ret = sc858x_psy_register(sc);
    if (ret)
        goto err_register_psy;

    cp_sysfs_create_group(sc->fc2_psy);
    
    ret = sc858x_register_interrupt(sc);
    if (ret < 0) {
        dev_err(sc->dev, "%s register irq fail(%d)\n",
                    __func__, ret);
        goto err_register_irq;
    }

    /* charger class register */
    ret = sc858x_register_charger(sc, SC858X_STANDALONG);
    if (ret) {
	    pr_err("failed to register charger\n");
	    return ret;
    }

    determine_initial_status(sc);

    sc->chip_ok = true;

    dev_err(sc->dev, "sc858x[%s] probe successfully!\n", 
            sc->mode == SC858X_STANDALONG ? "standalong" :
            sc->mode == SC858X_MASTER ? "master" : "slave");

    return 0;

err_register_irq:
err_register_psy:
err_init_device:
err_parse_dt:
    devm_kfree(&client->dev, sc);
    dev_err(sc->dev, "sc858x probe failed!\n");
    return ret;
}

static void sc858x_charger_remove(struct i2c_client *client)
{
    struct sc858x_chip *sc = i2c_get_clientdata(client);

    power_supply_unregister(sc->fc2_psy);
    devm_kfree(&client->dev, sc);
}

#ifdef CONFIG_PM_SLEEP
static int sc858x_suspend(struct device *dev)
{
    struct sc858x_chip *sc = dev_get_drvdata(dev);

    dev_info(sc->dev, "Suspend successfully!");
    if (device_may_wakeup(dev))
        enable_irq_wake(sc->irq);
    disable_irq(sc->irq);

    return 0;
}
static int sc858x_resume(struct device *dev)
{
    struct sc858x_chip *sc = dev_get_drvdata(dev);

    dev_info(sc->dev, "Resume successfully!");
    if (device_may_wakeup(dev))
        disable_irq_wake(sc->irq);
    enable_irq(sc->irq);

    return 0;
}

static const struct dev_pm_ops sc858x_pm = {
    SET_SYSTEM_SLEEP_PM_OPS(sc858x_suspend, sc858x_resume)
};
#endif

static struct i2c_driver sc858x_charger_driver = {
    .driver     = {
        .name   = "sc858x",
        .owner  = THIS_MODULE,
        .of_match_table = sc858x_of_match,
#ifdef CONFIG_PM_SLEEP
        .pm = &sc858x_pm,
#endif
    },
    .probe      = sc858x_charger_probe,
    .remove     = sc858x_charger_remove,
};

module_i2c_driver(sc858x_charger_driver);

MODULE_DESCRIPTION("SC SC858X ChargePump Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("South Chip <Aiden-yu@southchip.com>");
