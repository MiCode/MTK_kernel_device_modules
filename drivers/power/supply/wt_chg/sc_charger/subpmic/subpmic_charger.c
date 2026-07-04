// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2022 - 2023 SOUTHCHIP Semiconductor Technology(Shanghai) Co., Ltd.
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/types.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/usb/phy.h>
#include <linux/acpi.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/sched/prio.h>
#include <uapi/linux/sched/types.h>
#include <linux/platform_device.h>
#include <linux/hardware_info.h>

#include "../protocol/bc12/soft_bc12.h"
#include "../../charger_class/charger_class.h"
#include "subpmic.h"
#include "../../charger_class/dvchg_class.h"
#ifdef CONFIG_ENABLE_BOOT_DEBUG
#include "../../common/get_boot.h"
#endif
#include "../../../../../misc/mediatek/typec/sc_tcpc/inc/tcpci_core.h"

#define SC6601_DEVICE_ID            0X66
#define SY6976_DEVICE_ID            0X67
#define SC6601_1P1_DEVICE_ID        0x61
#define SC6601A_DEVICE_ID           0x62
#define SUBPMIC_REG_HK_DID          0X00

#define SUBPMIC_CHARGER_VERSION         "1.0.0"

struct buck_init_data {
    u32 vsyslim;
    u32 batsns_en;
    u32 vbat;
    u32 ichg;
    u32 vindpm;
    u32 iindpm_dis;
    u32 iindpm;
    u32 ico_enable;
    u32 iindpm_ico;
    u32 vprechg;
    u32 iprechg;
    u32 iterm_en;
    u32 iterm;
    u32 rechg_dis;
    u32 rechg_dg;
    u32 rechg_volt;
    u32 vboost;
    u32 iboost;
    u32 conv_ocp_dis;
    u32 tsbat_jeita_dis;
    u32 ibat_ocp_dis;
    u32 vpmid_ovp_otg_dis;
    u32 vbat_ovp_buck_dis;
    u32 ibat_ocp;
    u32 safety_timer;
};

enum subpmic_chg_fields {
    F_SY_CID_SEL,F_SY_CID_EN,/*REG01*/
    F_VAC_OVP,F_VBUS_OVP,
    F_TSBUS_FLT,
    F_TSBAT_FLT,
    F_ACDRV_MANUAL_PRE,F_ACDRV_EN,F_ACDRV_MANUAL_EN,F_WD_TIME_RST,F_WD_TIMER,
    F_REG_RST,F_VBUS_PD,F_VAC_PD,F_SC_CID_EN, /*REG08*/
    F_ADC_EN,F_ADC_RATE,F_ADC_FREEZE,F_BATID_ADC_EN,
    F_EDL_ACTIVE_LEVEL,
    /******* charger *******/
    F_VSYS_MIN,     /* REG30 */
    F_BATSNS_EN,F_VBAT, /* REG31 */
    F_ICHG_CC, /* REG32 */
    F_VINDPM_VBAT,F_VINDPM_DIS,F_VINDPM, /* REG33 */
    F_IINDPM_DIS,F_IINDPM,  /* REG34 */
    F_FORCE_ICO,F_ICO_EN,F_IINDPM_ICO,  /* REG35 */
    F_VBAT_PRECHG,F_IPRECHG,    /* REG36 */
    F_TERM_EN,F_ITERM,  /* REG37 */
    F_RECHG_DIS,F_RECHG_DG,F_VRECHG,    /* REG38 */
    F_VBOOST,F_IBOOST,  /* REG39 */
    F_CONV_OCP_DIS,F_TSBAT_JEITA_DIS,F_IBAT_OCP_DIS,F_VPMID_OVP_OTG_DIS,F_VBAT_OVP_BUCK_DIS,    /* REG3A */
    F_T_BATFET_RST,F_T_PD_nRST,F_BATFET_RST_EN,F_BATFET_DLY,F_BATFET_DIS,F_nRST_SHIPMODE_DIS,   /* REG3B */
    F_HIZ_EN,F_PERFORMANCE_EN,F_DIS_BUCKCHG_PATH,F_DIS_SLEEP_FOR_OTG,F_QB_EN,F_BOOST_EN,F_CHG_EN,   /* REG3C */
    F_VBAT_TRACK,F_IBATOCP,F_VSYSOVP_DIS,F_VSYSOVP_TH,  /* REG3D */
    F_BAT_COMP,F_VCLAMP,F_JEITA_ISET_COOL,F_JEITA_VSET_WARM,    /* REG3E */
    F_TMR2X_EN,F_CHG_TIMER_EN,F_CHG_TIMER,F_TDIE_REG_DIS,F_TDIE_REG,F_PFM_DIS,  /* REG3F */
    F_BAT_COMP_OFF,F_VBAT_LOW_OTG,F_BOOST_FREQ,F_BUCK_FREQ,F_BAT_LOAD_EN, /* REG40 */
    /*
    F_VSYS_SHORT_STAT,F_VSLEEP_BUCK_STAT,F_VBAT_DPL_STAT,F_VBAT_LOW_BOOST_STAT,F_VBUS_GOOD_STAT,
    F_CHG_STAT,F_BOOST_OK_STAT,F_VSYSMIN_REG_STAT,F_QB_ON_STAT,F_BATFET_STAT,
    F_TDIE_REG_STAT,F_TSBAT_COOL_STAT,F_TSBAT_WARM_STAT,F_ICO_STAT,F_IINDPM_STAT,F_VINDPM_STAT, */
    F_JEITA_COOL_TEMP,F_JEITA_WARM_TEMP,F_BOOST_NTC_HOT_TEMP,F_BOOST_NTC_COLD_TEMP, /* REG56 */
    F_TESTM_EN, /* REG5D */
    F_KEY_EN_OWN,   /* REG5E */
    /****** led ********/
    F_TRPT,F_FL_TX_EN,F_TLED2_EN,F_TLED1_EN,F_FLED2_EN,F_FLED1_EN,  /* reg80 */
    F_FLED1_BR, /* reg81 */
    F_FLED2_BR,/* reg82 */
    F_FTIMEOUT,F_FRPT,F_FTIMEOUT_EN,/* reg83 */
    F_TLED1_BR,/* reg84 */
    F_TLED2_BR,/* reg85 */
    F_PMID_FLED_OVP_DEG,F_VBAT_MIN_FLED,F_VBAT_MIN_FLED_DEG,F_LED_POWER,/* reg86 */
    /****** DPDPM ******/
    F_FORCE_INDET,F_AUTO_INDET_EN,F_HVDCP_EN,F_QC_EN,
    F_DP_DRIV,F_DM_DRIV,F_BC1_2_VDAT_REF_SET,F_BC1_2_DP_DM_SINK_CAP,
    F_QC2_V_MAX,F_QC3_PULS,F_QC3_MINUS,F_QC3_5_16_PLUS,F_QC3_5_16_MINUS,F_QC3_5_3_SEQ,F_QC3_5_2_SEQ,
    F_I2C_DPDM_BYPASS_EN,F_DPDM_PULL_UP_EN,F_WDT_TFCP_MASK,F_WDT_TFCP_FLAG,F_WDT_TFCP_RST,F_WDT_TFCP_CFG,F_WDT_TFCP_DIS,
    F_VBUS_STAT,F_BC1_2_DONE,F_DP_OVP,F_DM_OVP,
    F_DM_500K_PD_EN,F_DP_500K_PD_EN,F_DM_SINK_EN,F_DP_SINK_EN,F_DP_SRC_10UA,

    F_MAX_FIELDS,
};

enum ADC_MODULE {
    ADC_IBUS = 0,
    ADC_VBUS,
    ADC_VAC,
    ADC_VBATSNS,
    ADC_VBAT,
    ADC_IBAT,
    ADC_VSYS,
    ADC_TSBUS,
    ADC_TSBAT,
    ADC_TDIE,
    ADC_BATID,
};

static const u32 adc_step[] = { 
    2500, 3750, 5000, 1250, 1250,
    1220, 1250, 9766, 9766, 5, 156,
};

enum {
    SUBPMIC_CHG_STATE_NO_CHG = 0,
    SUBPMIC_CHG_STATE_TRICK,
    SUBPMIC_CHG_STATE_PRECHG,
    SUBPMIC_CHG_STATE_CC,
    SUBPMIC_CHG_STATE_CV,
    SUBPMIC_CHG_STATE_TERM,
};

enum DPDM_DRIVE {
    DPDM_HIZ = 0,
    DPDM_20K_DOWN,
    DPDM_V0_6,
    DPDM_V1_8,
    DPDM_V2_7,
    DPDM_V3_3,
    DPDM_500K_DOWN,
};

enum DPDM_CAP {
    DPDM_CAP_SNK_50UA = 0,
    DPDM_CAP_SNK_100UA,
    DPDM_CAP_SRC_10UA,
    DPDM_CAP_SRC_250UA,
    DPDM_CAP_DISABLE,
};

struct chip_state {
    bool online;
    bool boost_good;
    int vbus_type;
    int chg_state;
    int vindpm;
};

enum {
    IRQ_HK = 0,
    IRQ_BUCK,
    IRQ_DPDM,
    IRQ_LED,
    IRQ_MAX,
};

enum LED_FLASH_MODULE {
    LED1_FLASH = 0,
    LED2_FLASH,
    LED_ALL_FLASH,
};

struct subpmic_chg_device {
    struct i2c_client *client;
    struct device *dev;
    struct regmap *rmap;
    struct regmap_field *rmap_fields[F_MAX_FIELDS];

    struct buck_init_data buck_init;
    struct chip_state state;

    struct delayed_work led_work;
    enum LED_FLASH_MODULE led_index;
    struct completion flash_run;
    struct completion flash_end;
    bool led_state;
    atomic_t led_work_running;
    
    unsigned long request_otg;
    int irq[IRQ_MAX];

    struct charger_dev *charger;
    struct dvchg_dev *charger_pump;

    bool use_soft_bc12;
    struct soft_bc12 *bc;
    struct notifier_block bc12_result_nb;

    struct mutex bc_detect_lock;

    struct work_struct qc_detect_work;
    int qc_result;
    int qc_vbus;
    bool is_sy6976;
    bool is_sc6601;
    bool is_force_dpdm;
    bool ship_mode;
    struct tcpc_device *tcpc;
    struct timer_list bc12_timeout;
    bool last_chg_done;
}*g_subpmicdevice;

static int subpmic_chg_get_otg_status(struct charger_dev *charger,bool *en);
static int subpmic_chg_set_chg(struct charger_dev *charger, bool en);
static int subpmic_chg_set_otg_volt(struct charger_dev *charger, int mv);
static int subpmic_chg_request_otg(struct subpmic_chg_device *sc, int index, bool en);
static int subpmic_chg_set_wd_timeout(struct charger_dev *charger, int ms);
static int subpmic_chg_request_qc20(struct charger_dev *charger, int mv);
static int subpmic_chg_request_qc30(struct charger_dev *charger, int mv);
static int subpmic_chg_request_qc35(struct charger_dev *charger, int mv);

static int subpmic_chg_field_read(struct subpmic_chg_device *sc,
			      enum subpmic_chg_fields field_id)
{
	int ret;
	int val;

	ret = regmap_field_read(sc->rmap_fields[field_id], &val);
	if (ret < 0) {
        dev_err(sc->dev, "i2c field read failed\n");
		return ret;
    }

	return val;
}

static int subpmic_chg_field_write(struct subpmic_chg_device *sc,
			       enum subpmic_chg_fields field_id, u8 val)
{
    int ret = 0;
    ret = regmap_field_write(sc->rmap_fields[field_id], val);
    if (ret < 0) {
        dev_err(sc->dev, "i2c field write failed\n");
    }
	return ret;
}

static int subpmic_chg_bulk_read(struct subpmic_chg_device *sc, u8 reg,
                                u8 *val,size_t count)
{
    int ret = 0;
    ret = regmap_bulk_read(sc->rmap, reg, val, count);
    if (ret < 0) {
        dev_err(sc->dev, "i2c bulk read failed\n");
    }
    return ret;
}

static int subpmic_chg_bulk_write(struct subpmic_chg_device *sc, u8 reg,
                                u8 *val, size_t count)
{
    int ret = 0;
    ret = regmap_bulk_write(sc->rmap, reg, val, count);
    if (ret < 0) {
        dev_err(sc->dev, "i2c bulk write failed\n");
    }
    return ret;
}

static int subpmic_chg_write_byte(struct subpmic_chg_device *sc, u8 reg,
                                                    u8 val)
{
    u8 temp = val;
    return subpmic_chg_bulk_write(sc, reg, &temp, 1);
}

static int subpmic_chg_read_byte(struct subpmic_chg_device *sc, u8 reg,
                                                    u8 *val)
{
    return subpmic_chg_bulk_read(sc, reg, val, 1);
}

static const struct reg_field subpmic_chg_reg_fields[] = {
    [F_SY_CID_SEL]      = REG_FIELD(SUBPMIC_REG_HK_GEN_STATE,6,6),
    [F_SY_CID_EN]       = REG_FIELD(SUBPMIC_REG_HK_GEN_STATE,5,5),
    [F_VAC_OVP]         = REG_FIELD(SUBPMIC_REG_VAC_VBUS_OVP,4,7),
    [F_VBUS_OVP]        = REG_FIELD(SUBPMIC_REG_VAC_VBUS_OVP,0,2),
    [F_TSBUS_FLT]       = REG_FIELD(SUBPMIC_REG_VAC_VBUS_OVP,0,7),
    [F_TSBAT_FLT]       = REG_FIELD(SUBPMIC_REG_VAC_VBUS_OVP,0,7),
    [F_ACDRV_MANUAL_PRE]= REG_FIELD(SUBPMIC_REG_HK_CTRL,7,7),
    [F_ACDRV_EN]        = REG_FIELD(SUBPMIC_REG_HK_CTRL,5,5),
    [F_ACDRV_MANUAL_EN] = REG_FIELD(SUBPMIC_REG_HK_CTRL,4,4),
    [F_WD_TIME_RST]     = REG_FIELD(SUBPMIC_REG_HK_CTRL,3,3),
    [F_WD_TIMER]        = REG_FIELD(SUBPMIC_REG_HK_CTRL,0,2),
    [F_REG_RST]         = REG_FIELD(SUBPMIC_REG_HK_CTRL + 1,7,7),
    [F_VBUS_PD]         = REG_FIELD(SUBPMIC_REG_HK_CTRL + 1, 6, 6),
    [F_VAC_PD]          = REG_FIELD(SUBPMIC_REG_HK_CTRL + 1, 5, 5),
    [F_SC_CID_EN]          = REG_FIELD(SUBPMIC_REG_HK_CTRL + 1, 3, 3),
    [F_ADC_EN]          = REG_FIELD(SUBPMIC_REG_HK_ADC_CTRL,7,7),
    [F_ADC_RATE]          = REG_FIELD(SUBPMIC_REG_HK_ADC_CTRL,6,6),
    [F_ADC_FREEZE]      = REG_FIELD(SUBPMIC_REG_HK_ADC_CTRL, 5, 5),
    [F_BATID_ADC_EN]    = REG_FIELD(SUBPMIC_REG_HK_ADC_CTRL,3,3),
    [F_EDL_ACTIVE_LEVEL]= REG_FIELD(0x27, 1, 1),
    /* Charger */
    /* REG30 */
    [F_VSYS_MIN]        = REG_FIELD(SUBPMIC_REG_VSYS_MIN, 0, 2),
    /* REG31 */
    [F_BATSNS_EN]       = REG_FIELD(SUBPMIC_REG_VBAT, 7, 7),
    [F_VBAT]            = REG_FIELD(SUBPMIC_REG_VBAT, 0, 6),
    /* REG32 */
    [F_ICHG_CC]         = REG_FIELD(SUBPMIC_REG_ICHG_CC, 0, 6),
    /* REG33 */
    [F_VINDPM_VBAT]     = REG_FIELD(SUBPMIC_REG_VINDPM, 5, 6),
    [F_VINDPM_DIS]      = REG_FIELD(SUBPMIC_REG_VINDPM, 4, 4),
    [F_VINDPM]          = REG_FIELD(SUBPMIC_REG_VINDPM, 0, 3),
    /* REG34 */
    [F_IINDPM_DIS]      = REG_FIELD(SUBPMIC_REG_IINDPM, 7, 7),
    [F_IINDPM]          = REG_FIELD(SUBPMIC_REG_IINDPM, 0, 5),
    /* REG35 */
    [F_FORCE_ICO]       = REG_FIELD(SUBPMIC_REG_ICO_CTRL, 7, 7),
    [F_ICO_EN]          = REG_FIELD(SUBPMIC_REG_ICO_CTRL, 6, 6),
    [F_IINDPM_ICO]      = REG_FIELD(SUBPMIC_REG_ICO_CTRL, 0, 5),
    /* REG36 */
    [F_VBAT_PRECHG]     = REG_FIELD(SUBPMIC_REG_PRECHARGE_CTRL, 6, 7),
    [F_IPRECHG]         = REG_FIELD(SUBPMIC_REG_PRECHARGE_CTRL, 0, 3),
    /* REG37 */
    [F_TERM_EN]         = REG_FIELD(SUBPMIC_REG_TERMINATION_CTRL, 7, 7),
    [F_ITERM]           = REG_FIELD(SUBPMIC_REG_TERMINATION_CTRL, 0, 4),
    /* REG38 */
    [F_RECHG_DIS]       = REG_FIELD(SUBPMIC_REG_RECHARGE_CTRL, 4, 4),
    [F_RECHG_DG]        = REG_FIELD(SUBPMIC_REG_RECHARGE_CTRL, 2, 3),
    [F_VRECHG]          = REG_FIELD(SUBPMIC_REG_RECHARGE_CTRL, 0, 1),
    /* REG39 */
    [F_VBOOST]          = REG_FIELD(SUBPMIC_REG_VBOOST_CTRL, 3, 7),
    [F_IBOOST]          = REG_FIELD(SUBPMIC_REG_VBOOST_CTRL, 0, 2),
    /* REG3A */
    [F_CONV_OCP_DIS]    = REG_FIELD(SUBPMIC_REG_PROTECTION_DIS, 4, 4),
    [F_TSBAT_JEITA_DIS] = REG_FIELD(SUBPMIC_REG_PROTECTION_DIS, 3, 3),
    [F_IBAT_OCP_DIS]    = REG_FIELD(SUBPMIC_REG_PROTECTION_DIS, 2, 2),
    [F_VPMID_OVP_OTG_DIS] = REG_FIELD(SUBPMIC_REG_PROTECTION_DIS, 1, 1),
    [F_VBAT_OVP_BUCK_DIS] = REG_FIELD(SUBPMIC_REG_PROTECTION_DIS, 0, 0),
    /* REG3B */
    [F_T_BATFET_RST]    = REG_FIELD(SUBPMIC_REG_RESET_CTRL, 5, 5),
    [F_BATFET_RST_EN]   = REG_FIELD(SUBPMIC_REG_RESET_CTRL, 3, 3),
    [F_BATFET_DLY]      = REG_FIELD(SUBPMIC_REG_RESET_CTRL, 2, 2),
    [F_BATFET_DIS]      = REG_FIELD(SUBPMIC_REG_RESET_CTRL, 1, 1),
    /* REG3C */
    [F_HIZ_EN]          = REG_FIELD(SUBPMIC_REG_CHG_CTRL, 7, 7),
    [F_PERFORMANCE_EN]  = REG_FIELD(SUBPMIC_REG_CHG_CTRL, 6, 6),
    [F_DIS_BUCKCHG_PATH] = REG_FIELD(SUBPMIC_REG_CHG_CTRL, 5, 5),
    [F_DIS_SLEEP_FOR_OTG] = REG_FIELD(SUBPMIC_REG_CHG_CTRL, 4, 4),
    [F_QB_EN]           = REG_FIELD(SUBPMIC_REG_CHG_CTRL, 2, 2),
    [F_BOOST_EN]        = REG_FIELD(SUBPMIC_REG_CHG_CTRL, 1, 1),
    [F_CHG_EN]          = REG_FIELD(SUBPMIC_REG_CHG_CTRL, 0, 0),
    /* REG3D */
    [F_VBAT_TRACK]      = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 1, 5, 5),
    [F_IBATOCP]         = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 1, 3, 4),
    [F_VSYSOVP_DIS]     = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 1, 2, 2),
    [F_VSYSOVP_TH]      = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 1, 0, 1),
    /* REG3E */
    [F_BAT_COMP]        = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 2, 5, 7),
    [F_VCLAMP]          = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 2, 2, 4),
    [F_JEITA_ISET_COOL] = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 2, 1, 1),
    [F_JEITA_VSET_WARM] = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 2, 0, 0),
    /* REG3F */
    [F_TMR2X_EN]        = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 3, 7, 7),
    [F_CHG_TIMER_EN]    = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 3, 6, 6),
    [F_CHG_TIMER]       = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 3, 4, 5),
    [F_TDIE_REG_DIS]    = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 3, 3, 3),
    [F_TDIE_REG]        = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 3, 1, 2),
    [F_PFM_DIS]         = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 3, 0, 0),
    /* REG40 */
    [F_BAT_COMP_OFF]    = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 4, 6, 7),
    [F_VBAT_LOW_OTG]    = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 4, 5, 5),
    [F_BOOST_FREQ]      = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 4, 3, 4),
    [F_BUCK_FREQ]       = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 4, 1, 2),
    [F_BAT_LOAD_EN]     = REG_FIELD(SUBPMIC_REG_CHG_CTRL + 4, 0, 0),
    /* REG56 */
    [F_JEITA_COOL_TEMP] = REG_FIELD(SUBPMIC_REG_JEITA_TEMP, 6, 7),
    [F_JEITA_WARM_TEMP] = REG_FIELD(SUBPMIC_REG_JEITA_TEMP, 4, 5),
    [F_BOOST_NTC_HOT_TEMP]  = REG_FIELD(SUBPMIC_REG_JEITA_TEMP, 2, 3),
    [F_BOOST_NTC_COLD_TEMP] = REG_FIELD(SUBPMIC_REG_JEITA_TEMP, 0, 0),
    /* REG5D */
    [F_TESTM_EN]            = REG_FIELD(SUBPMIC_REG_Internal, 0, 0),
    /* REG5E */
    [F_KEY_EN_OWN]          = REG_FIELD(SUBPMIC_REG_Internal + 1, 0, 0),
    /*LED*/
    /*REG80*/
    [F_TRPT]         = REG_FIELD(SUBPMIC_REG_LED_CTRL,0,2),
    [F_FL_TX_EN]     = REG_FIELD(SUBPMIC_REG_LED_CTRL,3,3),
    [F_TLED2_EN]     = REG_FIELD(SUBPMIC_REG_LED_CTRL,4,4),
    [F_TLED1_EN]     = REG_FIELD(SUBPMIC_REG_LED_CTRL,5,5),
    [F_FLED2_EN]     = REG_FIELD(SUBPMIC_REG_LED_CTRL,6,6),
    [F_FLED1_EN]     = REG_FIELD(SUBPMIC_REG_LED_CTRL,7,7),
    [F_FLED1_BR]     = REG_FIELD(SUBPMIC_REG_FLED1_BR_CTR,0,6),
    [F_FLED2_BR]     = REG_FIELD(SUBPMIC_REG_FLED2_BR_CTR,0,6),
    [F_FTIMEOUT]     = REG_FIELD(SUBPMIC_REG_FLED_TIMER,0,3),
    [F_FRPT]         = REG_FIELD(SUBPMIC_REG_FLED_TIMER,4,6),
    [F_FTIMEOUT_EN]  = REG_FIELD(SUBPMIC_REG_FLED_TIMER,7,7),
    [F_TLED1_BR]     = REG_FIELD(SUBPMIC_REG_TLED1_BR_CTR,0,6),
    [F_TLED2_BR]     = REG_FIELD(SUBPMIC_REG_TLED2_BR_CTR,0,6),
    [F_PMID_FLED_OVP_DEG] = REG_FIELD(SUBPMIC_REG_LED_PRO,0,1),
    [F_VBAT_MIN_FLED]     = REG_FIELD(SUBPMIC_REG_LED_PRO,2,4),
    [F_VBAT_MIN_FLED_DEG] = REG_FIELD(SUBPMIC_REG_LED_PRO,5,6),
    [F_LED_POWER]         = REG_FIELD(SUBPMIC_REG_LED_PRO,7,7),
    /* DPDM */
    /* REG90 */
    [F_FORCE_INDET]     = REG_FIELD(SUBPMIC_REG_DPDM_EN, 7, 7),
    [F_AUTO_INDET_EN]   = REG_FIELD(SUBPMIC_REG_DPDM_EN, 6, 6),
    [F_HVDCP_EN]        = REG_FIELD(SUBPMIC_REG_DPDM_EN, 5, 5),
    [F_QC_EN]           = REG_FIELD(SUBPMIC_REG_DPDM_EN, 0, 0),
    /* REG91 */
    [F_DP_DRIV]         = REG_FIELD(SUBPMIC_REG_DPDM_CTRL, 5, 7),
    [F_DM_DRIV]         = REG_FIELD(SUBPMIC_REG_DPDM_CTRL, 2, 4),
    [F_BC1_2_VDAT_REF_SET] = REG_FIELD(SUBPMIC_REG_DPDM_CTRL, 1, 1),
    [F_BC1_2_DP_DM_SINK_CAP] = REG_FIELD(SUBPMIC_REG_DPDM_CTRL, 0, 0),
    /* REG92 */
    [F_QC2_V_MAX]       = REG_FIELD(SUBPMIC_REG_DPDM_QC_CTRL, 0, 1),
    [F_QC3_PULS]       = REG_FIELD(SUBPMIC_REG_DPDM_QC_CTRL, 2, 2),
    [F_QC3_MINUS]       = REG_FIELD(SUBPMIC_REG_DPDM_QC_CTRL, 3, 3),
    [F_QC3_5_16_PLUS]       = REG_FIELD(SUBPMIC_REG_DPDM_QC_CTRL, 4, 4),
    [F_QC3_5_16_MINUS]       = REG_FIELD(SUBPMIC_REG_DPDM_QC_CTRL, 5, 5),
    [F_QC3_5_3_SEQ]       = REG_FIELD(SUBPMIC_REG_DPDM_QC_CTRL, 6, 6),
    [F_QC3_5_2_SEQ]       = REG_FIELD(SUBPMIC_REG_DPDM_QC_CTRL, 7, 7),
    /* REG94 */
    [F_VBUS_STAT]       = REG_FIELD(SUBPMIC_REG_DPDM_INT_FLAG, 5, 7),
    [F_BC1_2_DONE]      = REG_FIELD(SUBPMIC_REG_DPDM_INT_FLAG, 2, 2),
    [F_DP_OVP]          = REG_FIELD(SUBPMIC_REG_DPDM_INT_FLAG, 1, 1),
    [F_DM_OVP]          = REG_FIELD(SUBPMIC_REG_DPDM_INT_FLAG, 0, 0),
    /* REG9D */
    [F_DM_500K_PD_EN]   = REG_FIELD(SUBPMIC_REG_DPDM_CTRL_2, 7, 7),
    [F_DP_500K_PD_EN]   = REG_FIELD(SUBPMIC_REG_DPDM_CTRL_2, 6, 6),
    [F_DM_SINK_EN]      = REG_FIELD(SUBPMIC_REG_DPDM_CTRL_2, 5, 5),
    [F_DP_SINK_EN]      = REG_FIELD(SUBPMIC_REG_DPDM_CTRL_2, 4, 4),
    [F_DP_SRC_10UA]     = REG_FIELD(SUBPMIC_REG_DPDM_CTRL_2, 3, 3),
};

static int subpmic_chg_dump_regs(struct subpmic_chg_device *sc,char *buf)
{
    int ret = 0,reg = 0;
    u8 val = 0;
    int count = 0;
    for (reg = SUBPMIC_REG_DEVICE_ID; reg < SUBPMIC_REG_MAX; reg++) {
        ret = subpmic_chg_read_byte(sc, reg, &val);
        if (ret < 0)
            return ret;
        if (buf)
            count += snprintf(buf + count, PAGE_SIZE - count, 
                                "[0x%x] -> 0x%x\n", reg, val);
        dev_info(sc->dev, "[0x%x] -> 0x%x\n", reg, val);
    }
    return count;
}

/**
 * DPDM Module
 */
static int subpmic_chg_set_dp_drive(struct subpmic_chg_device *sc, enum DPDM_DRIVE state)
{
    switch (state) {
    case DPDM_500K_DOWN:
        subpmic_chg_field_write(sc, F_DP_DRIV, DPDM_HIZ);
        subpmic_chg_field_write(sc, F_DP_500K_PD_EN, true);
        break;
    default:
        subpmic_chg_field_write(sc, F_DP_DRIV, state);
        break;
    }

    return 0;
}

static int subpmic_chg_set_dm_drive(struct subpmic_chg_device *sc, enum DPDM_DRIVE state)
{
    subpmic_chg_write_byte(sc, SUBPMIC_REG_DPDM_INTERNAL + 1, 0x00);
    
    switch (state) {
    case DPDM_500K_DOWN:
        subpmic_chg_field_write(sc, F_DM_DRIV, 0);
        subpmic_chg_field_write(sc, F_DM_500K_PD_EN, 1);
        break;
    case DPDM_V1_8:
        subpmic_chg_write_byte(sc, SUBPMIC_REG_DPDM_INTERNAL + 2, 0x2a);
        subpmic_chg_write_byte(sc, SUBPMIC_REG_DPDM_INTERNAL + 1, 0x0a);
        break;
    default:
        subpmic_chg_field_write(sc, F_DM_DRIV, state);
        break;
    }

    return 0;
}

static int subpmic_chg_set_dp_cap(struct subpmic_chg_device *sc, enum DPDM_CAP cap)
{

    switch (cap) {
    case DPDM_CAP_SNK_50UA:
        subpmic_chg_field_write(sc, F_DP_SINK_EN, true);
        subpmic_chg_field_write(sc, F_BC1_2_DP_DM_SINK_CAP, false);
        break;
    case DPDM_CAP_SNK_100UA:
        subpmic_chg_field_write(sc, F_DP_SINK_EN, true);
        subpmic_chg_field_write(sc, F_BC1_2_DP_DM_SINK_CAP, true);
        break;
    case DPDM_CAP_SRC_10UA:
        subpmic_chg_field_write(sc, F_DP_SINK_EN, false);
        subpmic_chg_field_write(sc, F_DP_SRC_10UA, true);
        break;
    case DPDM_CAP_SRC_250UA:
        subpmic_chg_field_write(sc, F_DP_SINK_EN, false);
        subpmic_chg_field_write(sc, F_DP_SRC_10UA, false);
        break;
    default:
        subpmic_chg_field_write(sc, F_DP_SINK_EN, false);
        break;    
    }

    return 0;
}

static int subpmic_chg_set_dm_cap(struct subpmic_chg_device *sc, enum DPDM_CAP cap)
{
    switch (cap) {
    case DPDM_CAP_SNK_50UA:
        subpmic_chg_field_write(sc, F_DM_SINK_EN, true);
        subpmic_chg_field_write(sc, F_BC1_2_DP_DM_SINK_CAP, false);
        break;
    case DPDM_CAP_SNK_100UA:
        subpmic_chg_field_write(sc, F_DM_SINK_EN, true);
        subpmic_chg_field_write(sc, F_BC1_2_DP_DM_SINK_CAP, true);
        break;
    default:
        subpmic_chg_field_write(sc, F_DM_SINK_EN, false);
        break;
    }

    return 0;
}

static int subpmic_chg_auto_dpdm_enable(struct subpmic_chg_device *sc, bool en)
{
    return subpmic_chg_field_write(sc, F_AUTO_INDET_EN, en);
}
/***************************************************************************/
/**
 * Soft BC1.2 ops
 */
static int subpmic_chg_bc12_init(struct soft_bc12 *bc)
{
    struct subpmic_chg_device *sc = bc->private;
    int ret = 0;

    ret = subpmic_chg_field_read(sc, F_AUTO_INDET_EN);
    if (ret > 0) {
        subpmic_chg_field_write(sc, F_AUTO_INDET_EN, false);
        msleep(500);
    }
    subpmic_chg_set_dm_drive(sc, DPDM_HIZ);
    subpmic_chg_set_dp_drive(sc, DPDM_HIZ);
    return 0;
}

static int subpmic_chg_bc12_deinit(struct soft_bc12 *bc)
{
    struct subpmic_chg_device *sc = bc->private;
    // set DPDM out to 3.3v , vooc/ufcs need
    return subpmic_chg_write_byte(sc, SUBPMIC_REG_DPDM_INTERNAL + 2, 0x6a);
}

static int subpmic_chg_update_bc12_state(struct soft_bc12 *bc)
{
    struct subpmic_chg_device *sc = bc->private;
    int ret;
    uint8_t dp, dm;
    // must set REG_DPDM_INTERNAL -> 0xa0
    ret = subpmic_chg_write_byte(sc, SUBPMIC_REG_DPDM_INTERNAL, 0xa0);
    if (ret < 0)
        return ret;
    
    udelay(1000);
    
    ret = subpmic_chg_read_byte(sc, SUBPMIC_REG_DP_STAT, &dp);
    switch (dp) {
    case 0x00:
        bc->dp_state = DPDM_V0_TO_V0_325;
        break;
    case 0x01:
        bc->dp_state = DPDM_V0_325_TO_V1;
        break;
    case 0x03:
        bc->dp_state = DPDM_V1_TO_V1_35;
        break;
    case 0x07:
        bc->dp_state = DPDM_V1_35_TO_V22;
        break;
    case 0x0F:
        bc->dp_state = DPDM_V2_2_TO_V3;
        break;
    case 0x1F:
        bc->dp_state = DPDM_V3;
        break;
    default:
        break;
    }
    ret = subpmic_chg_read_byte(sc, SUBPMIC_REG_DM_STAT, &dm);
    switch (dm) {
    case 0x00:
        bc->dm_state = DPDM_V0_TO_V0_325;
        break;
    case 0x01:
        bc->dm_state = DPDM_V0_325_TO_V1;
        break;
    case 0x03:
        bc->dm_state = DPDM_V1_TO_V1_35;
        break;
    case 0x07:
        bc->dm_state = DPDM_V1_35_TO_V22;
        break;
    case 0x0F:
        bc->dm_state = DPDM_V2_2_TO_V3;
        break;
    case 0x1F:
        bc->dm_state = DPDM_V3;
        break;
    default:
        break;
    }
    
    return 0;
}

static int subpmic_chg_set_bc12_state(struct soft_bc12 *bc, enum DPDM_SET_STATE state)
{
    struct subpmic_chg_device *sc = bc->private;

    switch (state) {
    case DPDM_HIZ_STATE:
        subpmic_chg_set_dm_drive(sc, DPDM_HIZ);
        subpmic_chg_set_dp_drive(sc, DPDM_HIZ);
        subpmic_chg_set_dp_cap(sc, DPDM_CAP_DISABLE);
        subpmic_chg_set_dm_cap(sc, DPDM_CAP_DISABLE);
        break;
    case DPDM_FLOATING_STATE:
        subpmic_chg_set_dp_drive(sc, DPDM_V2_7);
        subpmic_chg_set_dm_drive(sc, DPDM_20K_DOWN);
        subpmic_chg_set_dp_cap(sc, DPDM_CAP_SRC_10UA);
        break;
    case DPDM_PRIMARY_STATE:
        subpmic_chg_set_dp_drive(sc, DPDM_V0_6);
        subpmic_chg_set_dp_cap(sc, DPDM_CAP_SRC_250UA);
        subpmic_chg_set_dm_drive(sc, DPDM_HIZ);
        subpmic_chg_set_dm_cap(sc, DPDM_CAP_SNK_100UA);
        break;
    case DPDM_SECONDARY_STATE:
        subpmic_chg_set_dp_cap(sc, DPDM_CAP_SNK_100UA);
        subpmic_chg_set_dm_drive(sc, DPDM_V1_8);
        break;
    case DPDM_HVDCP_STATE:
        subpmic_chg_set_dm_cap(sc, DPDM_CAP_SNK_100UA);
        subpmic_chg_set_dm_drive(sc, DPDM_HIZ);
        subpmic_chg_set_dp_drive(sc, DPDM_V0_6);
        break;
    default:
        break;
    }

    return 0;
}

static int subpmic_chg_get_online(struct charger_dev *charger, bool *en);
static int subpmic_chg_bc12_get_vbus_online(struct soft_bc12 *bc)
{
    struct subpmic_chg_device *sc = bc->private;
    struct charger_dev *charger = sc->charger;
    bool en = 0;
    int ret = subpmic_chg_get_online(charger, &en);
    if (ret < 0)
        return 0;
    return en;
}
/***************************************************************************/
/**
 * BUCK Module
 */
static int subpmic_chg_set_ico_enable(struct subpmic_chg_device *sc, bool en)
{
    return subpmic_chg_field_write(sc, F_ICO_EN, en);
}

static int __subpmic_chg_get_chg_status(struct subpmic_chg_device *sc)
{
    int ret;
    u8 val;
    ret = subpmic_chg_read_byte(sc, SUBPMIC_REG_CHG_INT_STAT + 1, &val);
    if (ret < 0)
        return ret;
    val >>= 5;
    val &= 0x7;
    return val;    
}

static int subpmic_chg_mask_buck_irq(struct subpmic_chg_device *sc, int irq_channel)
{
    int ret;
    u8 val[3] = {0};

    ret = subpmic_chg_bulk_read(sc, SUBPMIC_REG_CHG_INT_MASK, val, 3);
    if (ret < 0) {
        return ret;
    }
    val[0] |= irq_channel;
    val[1] |= irq_channel >> 8;
    val[2] |= irq_channel >> 16;

    return subpmic_chg_bulk_write(sc, SUBPMIC_REG_CHG_INT_MASK, val, 3);    
}

int subpmic_chg_unmask_buck_irq(struct subpmic_chg_device *sc, int irq_channel)
{
    int ret;
    u8 val[3] = {0};

    ret = subpmic_chg_bulk_read(sc, SUBPMIC_REG_CHG_INT_MASK, val, 3);
    if (ret < 0) {
        return ret;
    }
    val[0] &= ~(irq_channel);
    val[1] &= ~(irq_channel >> 8);
    val[2] &= ~(irq_channel >> 16);

    return subpmic_chg_bulk_write(sc, SUBPMIC_REG_CHG_INT_MASK, val, 3);
}
EXPORT_SYMBOL(subpmic_chg_unmask_buck_irq);

static int subpmic_chg_set_sys_volt(struct subpmic_chg_device *sc, int mv)
{
    int i = 0;

    if (mv < vsys_min[0])
        mv = vsys_min[0];
    if (mv > vsys_min[ARRAY_SIZE(vsys_min) - 1])
        mv = vsys_min[ARRAY_SIZE(vsys_min) - 1];

    for (i = 0; i < ARRAY_SIZE(vsys_min); i++) {
        if (mv <= vsys_min[i])
            break;
    }
    return subpmic_chg_field_write(sc, F_VSYS_MIN, i);
}
/***************************************************************************/
/** 
 * Hourse Keeping Module
 */
int subpmic_chg_set_adc_func(struct subpmic_chg_device *sc, int adc_channel, bool en)
{
    int ret;
    u8 val[2] = {0};
    ret = subpmic_chg_bulk_read(sc, SUBPMIC_REG_HK_ADC_CTRL, val, 2);
    if (ret < 0)
    {
        return ret;
    }
    val[0] = en ? val[0] | (adc_channel >> 8) : val[0] & ~(adc_channel >> 8);
    val[1] = en ? val[1] | adc_channel : val[1] & ~adc_channel;

    return subpmic_chg_bulk_write(sc, SUBPMIC_REG_HK_ADC_CTRL, val, 2);
}
EXPORT_SYMBOL(subpmic_chg_set_adc_func);

static int subpmic_chg_get_adc(struct subpmic_chg_device *sc,
                                    enum ADC_MODULE id)
{
    u32 reg = SUBPMIC_REG_HK_IBUS_ADC + id * 2;
    u8 val[2] = {0};
    u32 ret = 0;
    ret = subpmic_chg_field_read(sc, F_ADC_EN);
    if (ret <= 0)
        return ret;
    if (id == ADC_BATID) {
        if(!sc->is_sy6976){
            subpmic_chg_field_write(sc, F_BATID_ADC_EN, true);
            mdelay(100); 
        }
        else{
            subpmic_chg_field_write(sc, F_BATID_ADC_EN, true);
            subpmic_chg_field_write(sc, F_ADC_RATE, true);
            subpmic_chg_field_write(sc, F_ADC_RATE, false);
            subpmic_chg_field_write(sc, F_ADC_RATE, true);
            mdelay(100);
        }
    }
    subpmic_chg_field_write(sc, F_ADC_FREEZE, true);
    ret = subpmic_chg_bulk_read(sc, reg, val, sizeof(val));
    if (ret < 0) {
        return ret;
    }
    ret = val[1] + (val[0] << 8);
    if (id == ADC_TDIE) {
        ret = (440 - ret) / 2;
    } else if (id == ADC_BATID) {
        if(sc->is_sy6976){
            ret = 156 * ret / 1000;
            subpmic_chg_field_write(sc, F_ADC_RATE, false);
        }
        else
            ret = 16233 * ret / 100000;
    } else if (id == ADC_TSBUS) {
        // get percentage
        ret = ret * adc_step[id] / 200000;
        ret = 100 * ret / (100 - ret) * 1000;
    } else if (id == ADC_TSBAT) {
        if (sc->use_soft_bc12) {
            ret = ret * adc_step[id] / 200000;
        } else {
            ret = ret * adc_step[id] / 100000;
        }
        ret = 100 * ret / (100 - ret) * 1000;
    } else {
        ret *= adc_step[id];
    }

    subpmic_chg_field_write(sc, F_ADC_FREEZE, false);

    return ret;
}

static int subpmic_chg_chip_reset(struct subpmic_chg_device *sc)
{
    return subpmic_chg_field_write(sc, F_REG_RST, true);
}

static int subpmic_chg_set_acdrv(struct subpmic_chg_device *sc, bool en)
{
    int ret;
    int cnt = 0;
    int from_ic;
    from_ic = subpmic_chg_field_read(sc, F_ACDRV_EN);
    do {
        if (cnt++ > 3) {
            dev_err(sc->dev, "[ERR]set acdrv failed\n");
            return -EIO;
        }

        ret = subpmic_chg_field_write(sc, F_ACDRV_EN, en);
        if (ret < 0)
            continue;

        from_ic = subpmic_chg_field_read(sc, F_ACDRV_EN);
        if (from_ic < 0)
            continue;
    } while (en != from_ic);
    dev_info(sc->dev, "acdrv set %d success\n", (int)en);
    return 0;
}

static int subpmic_chg_set_vac_ovp(struct subpmic_chg_device *sc, int mv)
{
    if (mv <= 6500)
        mv = 0;
    else if (mv <= 11000)
        mv = 1;
    else if (mv <= 12000)
        mv = 2;
    else if (mv <= 13000)
        mv = 3;
    else if (mv <= 14000)
        mv = 4;
    else if (mv <= 15000)
        mv = 5;
    else if (mv <= 16000)
        mv = 6;
    else
        mv = 7;
    return subpmic_chg_field_write(sc, F_VAC_OVP, mv);
}

static int subpmic_chg_set_vbus_ovp(struct subpmic_chg_device *sc, int mv)
{
    if (mv <= 6000)
        mv = 0;
    else if (mv <= 10000)
        mv = 1;
    else if (mv <= 12000)
        mv = 2;
    else
        mv = 3;
    return subpmic_chg_field_write(sc, F_VBUS_OVP, mv);
}

static int subpmic_chg_mask_hk_irq(struct subpmic_chg_device *sc, int irq_channel)
{
    u8 val = 0;
    subpmic_chg_read_byte(sc, SUBPMIC_REG_HK_INT_MASK, &val);
    val |= irq_channel;
    return subpmic_chg_write_byte(sc, SUBPMIC_REG_HK_INT_MASK, val);
}

int subpmic_chg_unmask_hk_irq(struct subpmic_chg_device *sc, int irq_channel)
{
    u8 val = 0;
    subpmic_chg_read_byte(sc, SUBPMIC_REG_HK_INT_MASK, &val);
    val &= ~irq_channel;
    return subpmic_chg_write_byte(sc, SUBPMIC_REG_HK_INT_MASK, val);
}
EXPORT_SYMBOL(subpmic_chg_unmask_hk_irq);

/***************************************************************************/

/**
 * QC Module
 */
enum QC_STATUS {
    QC_NONE = 0,
    QC2_MODE,
    QC3_MODE,
    QC3_5_18W_MODE,
    QC3_5_27W_MODE,
    QC3_5_45W_MODE,
};

static int subpmic_chg_hvdcp_detect(struct subpmic_chg_device *sc)
{
    int ret = 0;
    uint8_t dm = 0;
    if (!sc->is_sy6976) {
        ret = subpmic_chg_write_byte(sc, SUBPMIC_REG_DPDM_INTERNAL, 0xa0);
        if (ret < 0)
            return ret;
        
        udelay(1000);
    } else {
        subpmic_chg_field_write(sc, F_QC_EN, true);
    }
    ret = subpmic_chg_read_byte(sc, SUBPMIC_REG_DM_STAT, &dm);
    if (dm != 0x00) {
        return -1;
    }
    return 0;
}

extern int charger_manager_get_pd_active(void);
static int subpmic_chg_get_online(struct charger_dev *charger, bool *en);
static void qc_detect_workfunc(struct work_struct *work)
{
    struct subpmic_chg_device *sc = container_of(work,
                struct subpmic_chg_device, qc_detect_work);
    int cnt = 0,i=0;
    bool vbus_online = false;
    int pd_active = 0;
    u8 read_val = 0;
    u8 val = 0,ret = 0,result = 0;
    dev_info(sc->dev, "%s start\n", __func__);
    sc->qc_result = QC_NONE;

    pd_active = charger_manager_get_pd_active();
    if(pd_active) {
        pr_err("%s pd_active not detect qc\n", __func__);
        return;
    }

    subpmic_chg_field_write(sc, F_QC2_V_MAX, 3);
    subpmic_chg_read_byte(sc, SUBPMIC_REG_DPDM_QC_CTRL, &read_val);
    pr_err("after write 3 read SUBPMIC_REG_DPDM_QC_CTRL 0x%x", read_val);
    subpmic_chg_read_byte(sc, SUBPMIC_REG_DPDM_CTRL, &read_val);
    pr_err("after write 3 read SUBPMIC_REG_DPDM_CTRL 0x%x", read_val);
    subpmic_chg_field_write(sc, F_QC_EN, true);
    if(sc->is_sy6976){
        subpmic_chg_field_write(sc, F_HVDCP_EN, 1);
        subpmic_chg_field_write(sc, F_FORCE_INDET, 1);
    }
    do {
        msleep(200);
        subpmic_chg_get_online(sc->charger, &vbus_online);
        if (!vbus_online)
            break;
        if (subpmic_chg_hvdcp_detect(sc) == 0)
            break;
    } while(cnt++ < 10);
    if (!vbus_online) {
        subpmic_chg_field_write(sc, F_QC_EN, false);
        if(sc->is_sy6976)
            subpmic_chg_field_write(sc, F_HVDCP_EN, false);
        goto out;
    }
    if (subpmic_chg_hvdcp_detect(sc) < 0) {
        subpmic_chg_field_write(sc, F_QC_EN, false);
        if(sc->is_sy6976)
            subpmic_chg_field_write(sc, F_HVDCP_EN, false);
        goto out;
    }
    if(!sc->is_sy6976)
	sc->qc_result = QC2_MODE;
      
out:
    // set qc volt to qc2 5v , clear plus state
    subpmic_chg_write_byte(sc, SUBPMIC_REG_DPDM_QC_CTRL, 0x03);
     if(sc->is_sy6976){
        for(i=0;i<10;i++){
            msleep(300);
            ret = subpmic_chg_read_byte(sc, SUBPMIC_REG_DPDM_INT_FLAG, &val);
            if (ret < 0) {
            dev_info(sc->dev, "subpmic_chg_dpdm_alert_handler read val error\n");
            }
            dev_info(sc->dev, "SUBPMIC_REG_DPDM_INT_FLAG val:%x\n",val);
            if (val & SUBPMIC_DPDM_BC12_DETECT_DONE) {
                result = (val >> 5) & 0x7;
                sc->state.vbus_type = result;
                sc->is_force_dpdm = false;
                mutex_unlock(&sc->bc_detect_lock);
                break; 
            }
        }
        if (sc->state.vbus_type == VBUS_TYPE_HVDCP) {
            sc->qc_result = QC2_MODE;
            subpmic_chg_field_write(sc, F_QC2_V_MAX, 0);
            sc->qc_vbus = 9000;
            charger_changed(sc->charger);
        }
    }else{
        msleep(100);
        if (sc->qc_result == QC2_MODE){
             sc->state.vbus_type = VBUS_TYPE_HVDCP;
             sc->qc_vbus = 9000;
        }          
        if (sc->qc_result != QC_NONE)
            subpmic_chg_field_write(sc, F_QC2_V_MAX, 0);
        charger_changed(sc->charger);
    }
    dev_info(sc->dev, "%s end\n", __func__);
}

static int subpmic_chg_qc_identify(struct charger_dev *charger)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);
    if (work_busy(&sc->qc_detect_work)) {
        dev_err(sc->dev, "qc_detect work running\n");
        return -EBUSY;
    }
    sc->qc_vbus = 5000;
    schedule_work(&sc->qc_detect_work);
    return 0;
}

static int subpmic_chg_is_sy6976(struct charger_dev *charger)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);
    if(sc->is_sy6976)
        dev_err(sc->dev, "charger ic is sy6976\n");
    return sc->is_sy6976;
}

static int subpmic_chg_request_vbus(struct subpmic_chg_device *sc, int mv, int step) 
{
    int count = 0, i;
    int ret = 0;
    count = (mv - sc->qc_vbus) / step;
    for (i = 0; i < abs(count); i++) {
        if (count > 0)
            ret |= subpmic_chg_field_write(sc, F_QC3_PULS, true);
        else
            ret |= subpmic_chg_field_write(sc, F_QC3_MINUS, true);
        mdelay(8);
    }
    if (ret >= 0)
        sc->qc_vbus = mv;
    return ret;
}
/***************************************************************************/
#ifdef CONFIG_SOUTHCHIP_CHARGER_CLASS_MODULE
/***************************************************************************/
/*                                                                         */
/*                         subpmic class ops                               */
/*                                                                         */
/***************************************************************************/
static int subpmic_chg_charger_get_adc(struct charger_dev *charger, 
            enum sc_adc_channel channel, uint32_t *value)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);
    int ret = 0;
    switch (channel) {
    case SC_ADC_VBUS:
        ret = subpmic_chg_get_adc(sc, ADC_VBUS) / 1000;
        break;
    case SC_ADC_VSYS:
        ret = subpmic_chg_get_adc(sc, ADC_VSYS) / 1000;
        break;
    case SC_ADC_VBAT:
        ret = subpmic_chg_get_adc(sc, ADC_VBAT) / 1000;
        break;
    case SC_ADC_VAC:
        ret = subpmic_chg_get_adc(sc, ADC_VAC) / 1000;
        break;
    case SC_ADC_IBUS:
        ret = subpmic_chg_get_adc(sc, ADC_IBUS) / 1000;
        break;
    case SC_ADC_IBAT:
        ret = subpmic_chg_get_adc(sc, ADC_IBAT) / 1000;
        break;
    case SC_ADC_TSBUS:
        ret = subpmic_chg_get_adc(sc, ADC_TSBUS);
        break;
    case SC_ADC_TSBAT:
        ret = subpmic_chg_get_adc(sc, ADC_TSBAT);
        break;
    case SC_ADC_TDIE:
        ret = subpmic_chg_get_adc(sc, ADC_TDIE);
        break;
    case SC_ADC_BATTID:
         ret = subpmic_chg_get_adc(sc, ADC_BATID);
        break;
    default:
        ret = -1;
        break;
    }
    if (ret < 0)
        return ret;
    
    *value = ret;
    
    return 0;
}

static int subpmic_chg_get_online(struct charger_dev *charger, bool *en)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);
    *en = sc->state.online;
    return 0;
}

static int subpmic_chg_get_vbus_type(struct charger_dev *charger,
                                        enum vbus_type *vbus_type)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);
    *vbus_type = sc->state.vbus_type;
    dev_info(sc->dev, "%s vbus_type:%d\n",__func__,*vbus_type);
    return 0;
}

static int subpmic_chg_is_charge_done(struct charger_dev *charger,
                                    bool *en)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);
    *en = __subpmic_chg_get_chg_status(sc) == SUBPMIC_CHG_STATE_TERM;
    return 0;
}

static int subpmic_chg_get_hiz_status(struct charger_dev *charger, bool *en)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);
    int ret = 0;
    ret = subpmic_chg_field_read(sc, F_HIZ_EN);
    if (ret < 0)
        return ret;
    *en = ret;
    return 0;
}

static int subpmic_chg_get_input_volt_lmt(struct charger_dev *charger,
                                                    uint32_t *mv)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);
    *mv = sc->state.vindpm;
    return 0;
}

static int subpmic_chg_get_input_curr_lmt(struct charger_dev *charger,
                                                    uint32_t *ma)
{
    int ret;
    struct subpmic_chg_device *sc = charger_get_private(charger);
    ret = subpmic_chg_field_read(sc, F_IINDPM);
    if (ret < 0)
        return ret;
    *ma = SUBPMIC_BUCK_IINDPM_STEP * ret + SUBPMIC_BUCK_IINDPM_OFFSET;
    return 0;
}

static int subpmic_chg_get_chg_status(struct charger_dev *charger,
                                        uint32_t *chg_status)
{
    int state = 0;
    struct subpmic_chg_device *sc = charger_get_private(charger);

    state = __subpmic_chg_get_chg_status(sc);
    switch (state) {
    case SUBPMIC_CHG_STATE_CC:
    case SUBPMIC_CHG_STATE_CV:
        *chg_status = POWER_SUPPLY_CHARGE_TYPE_FAST;
        break;
    case SUBPMIC_CHG_STATE_PRECHG:
    case SUBPMIC_CHG_STATE_TRICK:
        *chg_status = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
        break;
    case SUBPMIC_CHG_STATE_TERM:
        *chg_status = POWER_SUPPLY_CHARGE_TYPE_NONE;
        break;
    default:
        *chg_status = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
    }
    return 0;
}

static int subpmic_chg_get_otg_status(struct charger_dev *charger,
                                            bool *en)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);
    int ret = 0;
    ret = subpmic_chg_field_read(sc, F_BOOST_EN);
    if (ret < 0)
        return ret;
    *en = ret;
    return 0;
}

static int subpmic_chg_get_term_curr(struct charger_dev *charger,
                                        uint32_t *ma)
{
    int ret = 0;
    struct subpmic_chg_device *sc = charger_get_private(charger);
    ret = subpmic_chg_field_read(sc, F_ITERM);
    if (ret < 0)
        return ret;
    *ma = ret * SUBPMIC_BUCK_ITERM_STEP + SUBPMIC_BUCK_ITERM_OFFSET;
    return ret;
}

static int subpmic_chg_get_term_volt(struct charger_dev *charger,
                                        uint32_t *mv)
{
    int ret = 0;
    struct subpmic_chg_device *sc = charger_get_private(charger);
    ret = subpmic_chg_field_read(sc, F_VBAT);
    if (ret < 0)
        return ret;
    *mv = ret * SUBPMIC_BUCK_VBAT_STEP + SUBPMIC_BUCK_VBAT_OFFSET;
    return 0;
}

static int subpmic_chg_set_hiz(struct charger_dev *charger, bool en)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);
    return subpmic_chg_field_write(sc, F_HIZ_EN, en);
}

static int subpmic_chg_set_input_curr_lmt(struct charger_dev *charger, int ma)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);
    int ret;
    pr_err("%s set curr:%d\n",__func__,ma);
    if (ma < 100) {
        /* In Preformance Mode (Buck Off , REGN ON) */
        if (sc->is_sy6976)
                return subpmic_chg_field_write(sc, F_DIS_BUCKCHG_PATH, true);
        else
                return subpmic_chg_field_write(sc, F_PERFORMANCE_EN, true);
    }
    if (sc->is_sy6976)
    	ret = subpmic_chg_field_write(sc, F_DIS_BUCKCHG_PATH, false);
    else
    	ret = subpmic_chg_field_write(sc, F_PERFORMANCE_EN, false);

    if (ret < 0)
        return ret;
    if (ma < SUBPMIC_BUCK_IINDPM_MIN) ma = SUBPMIC_BUCK_IINDPM_MIN;
    if (ma > SUBPMIC_BUCK_IINDPM_MAX) ma = SUBPMIC_BUCK_IINDPM_MAX;
    ma = (ma - SUBPMIC_BUCK_IINDPM_OFFSET) / SUBPMIC_BUCK_IINDPM_STEP;

    return subpmic_chg_field_write(sc, F_IINDPM, ma);
}

static int subpmic_chg_set_input_volt_lmt(struct charger_dev *charger, int mv)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);
    int i = 0, ret;
    if (mv < vindpm[0])
        mv = vindpm[0];
    if (mv > vindpm[ARRAY_SIZE(vindpm) - 1])
        mv = vindpm[ARRAY_SIZE(vindpm) - 1];

    for (i = 0; i < ARRAY_SIZE(vindpm); i++) {
        if (mv <= vindpm[i])
            break;
    }
    ret = subpmic_chg_field_write(sc, F_VINDPM, i);
    if (ret < 0)
        return ret;
    sc->state.vindpm = vindpm[i];
    return 0;
}

static int subpmic_chg_set_ichg(struct charger_dev *charger, int ma)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);
    if (ma <= SUBPMIC_BUCK_ICHG_MIN) {
        ma = SUBPMIC_BUCK_ICHG_MIN;
    } else if (ma >= SUBPMIC_BUCK_ICHG_MAX) {
        ma = SUBPMIC_BUCK_ICHG_MAX;
    }   
    ma = (ma - SUBPMIC_BUCK_ICHG_OFFSET) / SUBPMIC_BUCK_ICHG_STEP;
    return subpmic_chg_field_write(sc, F_ICHG_CC, ma);
}

static int subpmic_chg_get_ichg(struct charger_dev *charger)
{
    int val = 0;
    struct subpmic_chg_device *sc = charger_get_private(charger);
    val = subpmic_chg_field_read(sc, F_ICHG_CC);
    val = val * SUBPMIC_BUCK_ICHG_STEP;
    return val;
}

static int subpmic_chg_set_chg(struct charger_dev *charger, bool en)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);
    return subpmic_chg_field_write(sc, F_CHG_EN, en);
}

static int subpmic_chg_get_chg(struct charger_dev *charger)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);
    return subpmic_chg_field_read(sc, F_CHG_EN);
}

extern int tcpm_set_remote_power_cap(struct tcpc_device *tcpc, int mv, int ma);
extern struct tcpc_device *tcpc_dev_get_by_name(const char *name);
extern bool endurance_protect;
static int subpmic_chg_get_status(struct charger_dev *charger, int *chg_status) {
    struct subpmic_chg_device *sc = charger_get_private(charger);
    bool hiz_status = false;
    bool chg_en = false;
    bool chg_done = false;
    subpmic_chg_is_charge_done(charger,&chg_done);
    subpmic_chg_get_hiz_status(charger, &hiz_status);
    chg_en = subpmic_chg_get_chg(charger);
    *chg_status =  POWER_SUPPLY_STATUS_UNKNOWN;
    sc->tcpc = tcpc_dev_get_by_name("type_c_port0");
    printk("last_chg_done = %d\n", sc->last_chg_done);
    printk("chg_en:%d,hiz_status:%d,chg_done:%d\n", chg_en,hiz_status,chg_done );
    if (chg_done) {
        *chg_status = POWER_SUPPLY_STATUS_FULL;
        if (!sc->last_chg_done) {
            if (sc->qc_result == QC2_MODE && (sc->qc_vbus != 5000)) {
                subpmic_chg_write_byte(sc, SUBPMIC_REG_DPDM_QC_CTRL, 0x03);
                sc->qc_vbus = 5000;
            }
            if(sc->tcpc && charger_manager_get_pd_active()){
                tcpm_set_remote_power_cap(sc->tcpc,5000,2000);
            }
            sc->last_chg_done = true;
            printk("last_chg_done set:%d\n", sc->last_chg_done);
        }
    }     
    else {
        if ((chg_en) && (!hiz_status)){
            *chg_status = POWER_SUPPLY_STATUS_CHARGING;
        }
        else if (!chg_en && !endurance_protect){
            *chg_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
        }
        else{
            *chg_status = POWER_SUPPLY_STATUS_DISCHARGING;
        }
        sc->last_chg_done = false;
        printk("last_chg_done set:%d\n", sc->last_chg_done);
    }
    return 0;

}

static int subpmic_chg_set_otg(struct subpmic_chg_device *sc, bool en)
{
    int ret;
    int cnt = 0;
    u8 boost_state;    
    ret = subpmic_chg_set_chg(sc->charger, !en);
    if (ret < 0) {
        return ret;
    }
    do {
        boost_state = 0;
        ret = subpmic_chg_field_write(sc, F_BOOST_EN, en ? true : false);
        if (ret < 0) {
            subpmic_chg_set_chg(sc->charger, true);
            return ret;
        }
        mdelay(30);
        ret = subpmic_chg_read_byte(sc, SUBPMIC_REG_CHG_INT_STAT + 1, &boost_state);
        if (cnt++ > 3) {
            subpmic_chg_set_chg(sc->charger, true);
            return -EIO;
        }
    }while (en != (!!(boost_state & BIT(4))));
    dev_info(sc->dev, "otg set success");
    return 0;
}

enum {
    SUBPMIC_NORMAL_USE_OTG = 0,
    SUBPMIC_LED_USE_OTG,
};

static int subpmic_chg_request_otg(struct subpmic_chg_device *sc, int index, bool en)
{
    int ret = 0;

    printk("request otg en = %d\n",en);
    if (en)
        set_bit(index, &sc->request_otg);
    else 
        clear_bit(index, &sc->request_otg);

    dev_info(sc->dev, "now request_otg = %lx\n", sc->request_otg);

    if (sc->is_sy6976) {
        ret = subpmic_chg_set_otg(sc, en);
        if (ret < 0) {
            if (en)
                clear_bit(index, &sc->request_otg);
            else 
                set_bit(index, &sc->request_otg);
            return ret;
        }

        //BA76 must BOOST_EN first then QB_EN
        dev_err(sc->dev, "uu+ >>>>>now request_otg = %lx\n", sc->request_otg);
        if (index != SUBPMIC_LED_USE_OTG && en) {
            ret = subpmic_chg_field_write(sc, F_QB_EN, true);
            dev_err(sc->dev, "QB_EN=1.................");//uu311+
        } else if (index != SUBPMIC_LED_USE_OTG && !en){
            ret = subpmic_chg_field_write(sc, F_QB_EN, false);
        }
    //uu312+.
    //uu314+  check en to change 0x07 
        if (en)
            ret = subpmic_chg_write_byte(sc, 0x07, 0x30);//uu314
        else
            ret = subpmic_chg_write_byte(sc, 0x07, 0x00);//uu314
        if (ret < 0)
            dev_err(sc->dev, "sy6976 write 0x07 failed\n");
    } else {
        if (index != SUBPMIC_LED_USE_OTG && en) {
            ret = subpmic_chg_field_write(sc, F_QB_EN, true);
        } else if (index != SUBPMIC_LED_USE_OTG && !en){
            ret = subpmic_chg_field_write(sc, F_QB_EN, false);
        }

        if (!en && sc->request_otg) {
            return 0;
        }
        ret = subpmic_chg_set_otg(sc, en);
        if (ret < 0) {
            if (en)
                clear_bit(index, &sc->request_otg);
            else 
                set_bit(index, &sc->request_otg);
            return ret;
        }
    }

    return 0;
}

static int subpmic_chg_normal_request_otg(struct charger_dev *charger, bool en)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);
    return subpmic_chg_request_otg(sc, SUBPMIC_NORMAL_USE_OTG, en);
}

static int subpmic_chg_set_otg_curr(struct charger_dev *charger, int ma)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);
    int i = 0;
    if (ma < boost_curr[0])
        ma = boost_curr[0];
    if (ma > boost_curr[ARRAY_SIZE(boost_curr) - 1])
        ma = boost_curr[ARRAY_SIZE(boost_curr) - 1];

    for (i = 0; i <= ARRAY_SIZE(boost_curr); i++) {
        if (ma < boost_curr[i])
            break;
    }

    return subpmic_chg_field_write(sc, F_IBOOST, i);
}

static int subpmic_chg_set_otg_volt(struct charger_dev *charger, int mv)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);


    if (mv < SUBPMIC_BUCK_OTG_VOLT_MIN)
        mv = SUBPMIC_BUCK_OTG_VOLT_MIN;
    if (mv > SUBPMIC_BUCK_OTG_VOLT_MAX)
        mv = SUBPMIC_BUCK_OTG_VOLT_MAX;

    mv = (mv - SUBPMIC_BUCK_OTG_VOLT_OFFSET) / SUBPMIC_BUCK_OTG_VOLT_STEP;
    return subpmic_chg_field_write(sc, F_VBOOST, mv);
}

static int subpmic_chg_set_term(struct charger_dev *charger, bool en)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);
    return subpmic_chg_field_write(sc, F_TERM_EN, en);
}

static int subpmic_chg_set_term_curr(struct charger_dev *charger, int ma)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);
    
    if (ma < SUBPMIC_BUCK_ITERM_MIN)
        ma = SUBPMIC_BUCK_ITERM_MIN;
    if (ma > SUBPMIC_BUCK_ITERM_MAX)
        ma = SUBPMIC_BUCK_ITERM_MAX;

    ma = (ma - SUBPMIC_BUCK_ITERM_OFFSET) / SUBPMIC_BUCK_ITERM_STEP;
    return subpmic_chg_field_write(sc, F_ITERM, ma);
}

static int subpmic_chg_set_term_volt(struct charger_dev *charger, int mv)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);
    if (mv < SUBPMIC_BUCK_VBAT_MIN)
        mv = SUBPMIC_BUCK_VBAT_MIN;
    if (mv > SUBPMIC_BUCK_VBAT_MAX)
        mv = SUBPMIC_BUCK_VBAT_MAX;

    mv = (mv - SUBPMIC_BUCK_VBAT_OFFSET) / SUBPMIC_BUCK_VBAT_STEP;

    return subpmic_chg_field_write(sc, F_VBAT, mv);
}

static int subpmic_chg_adc_enable(struct charger_dev *charger, bool en)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);
    return subpmic_chg_field_write(sc, F_ADC_EN, en);
}

static int subpmic_chg_set_prechg_volt(struct charger_dev *charger, int mv)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);
    int i = 0, ret;
    if (mv < prechg_volt[0])
        mv = prechg_volt[0];
    if (mv > prechg_volt[ARRAY_SIZE(prechg_volt) - 1])
        mv = prechg_volt[ARRAY_SIZE(prechg_volt) - 1];

    for (i = 0; i < ARRAY_SIZE(prechg_volt); i++) {
        if (mv <= prechg_volt[i])
            break;
    }
    ret = subpmic_chg_field_write(sc, F_VBAT_PRECHG, i);
    if (ret < 0)
        return ret;
        
    return 0;
}

static int subpmic_chg_set_prechg_curr(struct charger_dev *charger, int ma)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);
    if (ma < SUBPMIC_BUCK_PRE_CURR_MIN)
        ma = SUBPMIC_BUCK_PRE_CURR_MIN;
    if (ma > SUBPMIC_BUCK_PRE_CURR_MAX)
        ma = SUBPMIC_BUCK_PRE_CURR_MAX;

    ma = (ma - SUBPMIC_BUCK_PRE_CURR_OFFSET) / SUBPMIC_BUCK_PRE_CURR_STEP;
    return subpmic_chg_field_write(sc, F_IPRECHG, ma);

}   

extern struct tcpc_device *tcpc_dev_get_by_name(const char *name);
extern int sc660x_tcpc_set_shutdownoff(struct tcpc_device *tcpc);
static int subpmic_chg_set_shipmode(struct subpmic_chg_device *sc, bool en)
{
    int ret = 0;

    if (sc->is_sc6601){
        sc->tcpc = tcpc_dev_get_by_name("type_c_port0");
         sc660x_tcpc_set_shutdownoff(sc->tcpc);   
    }
    if (en) {
		ret = subpmic_chg_field_write(sc, F_BATFET_DLY, false);
		ret |= subpmic_chg_field_write(sc, F_BATFET_DIS, true);
	} else {
		ret |= subpmic_chg_field_write(sc, F_BATFET_DIS, false);
	}

    return ret;
}

static int sc_chg_set_shipmode(struct charger_dev *charger, bool en)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);
    sc->ship_mode = en;
    return 0;
}

static int subpmic_chg_set_rechg_vol(struct charger_dev *charger, int val)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);
    int i = 0;
    if (val < rechg_volt[0])
        val = rechg_volt[0];
    if (val > rechg_volt[ARRAY_SIZE(rechg_volt)-1])
        val = rechg_volt[ARRAY_SIZE(rechg_volt)-1];

    for (i = 0; i < ARRAY_SIZE(rechg_volt); i++) {
        if (val <= rechg_volt[i])
            break;
    }
    return subpmic_chg_field_write(sc, F_VRECHG, i);
}

static void bc12_timeout_func(struct timer_list *timer)
{
    struct subpmic_chg_device *sc = container_of(timer,
                    struct subpmic_chg_device, bc12_timeout);
    dev_info(sc->dev, "BC1.2 timeout\n");
    mutex_unlock(&sc->bc_detect_lock);
    charger_changed(sc->charger);
}

static int bc12_timeout_start(struct subpmic_chg_device *sc)
{
    del_timer(&sc->bc12_timeout);
    sc->bc12_timeout.expires = jiffies + msecs_to_jiffies(300);
    sc->bc12_timeout.function = bc12_timeout_func;
    add_timer(&sc->bc12_timeout);
    return 0;
}

static int bc12_timeout_cancel(struct subpmic_chg_device *sc)
{
    del_timer(&sc->bc12_timeout);
    return 0;
}

extern void Charger_Detect_Init(void);
extern void Charger_Detect_Release(void);
static int subpmic_chg_force_dpdm(struct charger_dev *charger)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);
    dev_info(sc->dev,"subpmic_chg_force_dpdm enter\n");
    Charger_Detect_Init();
    if (sc->use_soft_bc12)
        return bc12_detect_start(sc->bc);
    else {
        if (mutex_trylock(&sc->bc_detect_lock) == 0) {
            dev_info(sc->dev, "bc_detect_lock");
            return -EBUSY;
        }
        if(!sc->is_sy6976)
        	bc12_timeout_start(sc);
        dev_err(sc->dev,"return subpmic_chg_field_write\n");
        sc->is_force_dpdm = true;
        return subpmic_chg_field_write(sc, F_FORCE_INDET, true);
    }
}

static int subpmic_chg_request_dpdm(struct charger_dev *charger, bool en)
{
    // struct subpmic_chg_device *sc = charger_get_private(charger);
    // todo
    return 0;
}

static int subpmic_chg_set_wd_timeout(struct charger_dev *charger, int ms)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);
    int i = 0;
    if (ms < subpmic_chg_wd_time[0])
        ms = subpmic_chg_wd_time[0];
    if (ms > subpmic_chg_wd_time[ARRAY_SIZE(subpmic_chg_wd_time) - 1])
        ms = subpmic_chg_wd_time[ARRAY_SIZE(subpmic_chg_wd_time) - 1];

    for (i = 0; i < ARRAY_SIZE(subpmic_chg_wd_time); i++) {
        if (ms <= subpmic_chg_wd_time[i])
            break;
    }
    return subpmic_chg_field_write(sc, F_WD_TIMER, i);
}

static int subpmic_chg_kick_wd(struct charger_dev *charger)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);
    return subpmic_chg_field_write(sc, F_WD_TIME_RST, true);
}

static int subpmic_chg_request_qc20(struct charger_dev *charger, int mv)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);
    int val = 0;
    if (sc->qc_result != QC2_MODE) {
        return -EIO;
    }
    subpmic_chg_field_write(sc, F_QC_EN, true);
    if (mv == 5000) val = 3;
    else if (mv == 9000) val = 0;
    else if (mv == 12000) val = 1;

    return subpmic_chg_field_write(sc, F_QC2_V_MAX, val);
}

static int subpmic_chg_request_qc30(struct charger_dev *charger, int mv)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);
    if (sc->qc_result != QC3_MODE) {
        return -EIO;
    }
    return subpmic_chg_request_vbus(sc, mv, 200);
}

static int subpmic_chg_request_qc35(struct charger_dev *charger, int mv)
{
    struct subpmic_chg_device *sc = charger_get_private(charger);
    if (sc->qc_result != QC3_5_18W_MODE &&
        sc->qc_result != QC3_5_27W_MODE &&
        sc->qc_result != QC3_5_45W_MODE ) {
        return -EIO;
    }
    return subpmic_chg_request_vbus(sc, mv, 20);
}
#endif
/************************ end subpmic ops *********************************/
static irqreturn_t subpmic_chg_led_alert_handler(int irq, void *data)
{
    struct subpmic_chg_device *sc = data;
    int ret = 0;
    u8 val = 0;
    ret = subpmic_chg_read_byte(sc, SUBPMIC_REG_LED_FLAG, &val);
    if (ret < 0) {
        return ret;
    }
    dev_info(sc->dev, "LED Flag -> %x\n", val);

    if (val & SUBPMIC_LED_FLAG_FLASH_DONE) {
        dev_info(sc->dev, "led flash done\n");
        complete(&sc->flash_end);
    }
    return IRQ_HANDLED;
}

static irqreturn_t subpmic_chg_dpdm_alert_handler(int irq, void *data)
{
    struct subpmic_chg_device *sc = data;
    int ret = 0;
    u8 val = 0, result = 0,val1 = 0;
    
    if (!sc->use_soft_bc12) {
        ret = subpmic_chg_read_byte(sc, SUBPMIC_REG_DPDM_INT_FLAG, &val);
        if (ret < 0) {
            dev_info(sc->dev, "subpmic_chg_dpdm_alert_handler read val error\n");
            return ret;
        }
        dev_info(sc->dev, "subpmic_chg_dpdm_alert_handler val:%x\n",val);
        if (val & SUBPMIC_DPDM_BC12_DETECT_DONE) {
            if(!sc->is_sy6976)
            	bc12_timeout_cancel(sc);
            ret = subpmic_chg_read_byte(sc, SUBPMIC_REG_CHG_INT_STAT, &val1);
            if (ret < 0)
                dev_info(sc->dev, "%s read pwgd fail\n",__func__);
            dev_info(sc->dev, "%s read pwgd:%x\n,online = %d",__func__,val1,sc->state.online);
            if (!(val1 & BIT(0)))
            	result = NONE;
            else
                result = (val >> 5) & 0x7;
            sc->state.vbus_type = result;
            mutex_unlock(&sc->bc_detect_lock);
            charger_changed(sc->charger);
        }
        Charger_Detect_Release();
    }
    return IRQ_HANDLED;
}

static int subpmic_chg_bc12_notify_cb(struct notifier_block *nb, 
                    unsigned long event, void *data)
{
    struct soft_bc12 *bc = data;
    struct subpmic_chg_device *sc = bc->private;
    if (sc->use_soft_bc12) {
        dev_info(sc->dev, "BC1.2 COMPLETE : -> %ld\n", event);
        sc->state.vbus_type = event;
        charger_changed(sc->charger);
    }
    return NOTIFY_OK;
}

static irqreturn_t subpmic_chg_buck_alert_handler(int irq, void *data)
{
    struct subpmic_chg_device *sc = data;
    int ret;
    u8 val[3];
    u32 flt, state;
    

    ret = subpmic_chg_bulk_read(sc, SUBPMIC_REG_CHG_FLT_FLG, val, 2);
    if (ret < 0) {
        return ret;
    }
    flt = val[0] + (val[1] << 8);
    if (flt != 0) {
        dev_err(sc->dev, "Buck FAULT : 0x%x\n", flt);
    }

    ret = subpmic_chg_bulk_read(sc, SUBPMIC_REG_CHG_INT_STAT, val, 3);
    if (ret < 0) {
        return ret;
    }
    state = val[0] + (val[1] << 8) + (val[2] << 16);
    dev_info(sc->dev, "Buck State : 0x%x\n", state);

    sc->state.boost_good = !!(state & SUBPMIC_BUCK_FLAG_BOOST_GOOD);
    
    sc->state.chg_state = SUBPMIC_BUCK_GET_CHG_STATE(state);

    charger_changed(sc->charger);

    return IRQ_HANDLED;
}

int subpmic_chg_get_vbus_good(void)
{
    struct subpmic_chg_device *sc = g_subpmicdevice;
    int ret = 0;
    int vbus_good = 0;
    uint8_t val = 0;
    
    if(sc != NULL) {
        ret = subpmic_chg_read_byte(sc, SUBPMIC_REG_CHG_INT_STAT, &val);
        if (ret < 0) {
            dev_err(sc->dev, "%s fail!!\n",__func__);
            return 0;
        }
        vbus_good = val & BIT(0);
        dev_err(sc->dev, "%s vbus_good:%d\n",__func__,vbus_good);
    }
    return vbus_good;
}
EXPORT_SYMBOL(subpmic_chg_get_vbus_good);

extern int sy6976_subpmic_cid_detect(void);
extern int tcpci_alert_power_status_changed(struct tcpc_device *tcpc);
static irqreturn_t subpmic_chg_hk_alert_handler(int irq, void *data)
{
    struct subpmic_chg_device *sc = data;
    static int last_vbus_present = 0;
    static int last_online = 0;
    int ret = 0;
    uint8_t val = 0,val1 = 0, result = 0;

    ret = subpmic_chg_read_byte(sc, SUBPMIC_REG_HK_FLT_FLG, &val);
    if (ret < 0)
        goto out;
    if (val != 0) {
        dev_err(sc->dev, "Hourse Keeping FAULT : 0x%x\n", val);
    }
    if (sc->is_sy6976)
        sy6976_subpmic_cid_detect();
    ret = subpmic_chg_read_byte(sc, SUBPMIC_REG_HK_INT_STAT, &val);
    if (ret < 0)
        goto out;
    if (sc->is_sy6976) {
        if (((val & 0x02) >> 1) == 0 && last_vbus_present == 1)
        {
            dev_err(sc->dev, "buck vbus0");
            if (sc->tcpc)
                tcpci_alert_power_status_changed(sc->tcpc);
        }
        last_vbus_present = (val & 0x02) >> 1;
    }
    ret = subpmic_chg_read_byte(sc, SUBPMIC_REG_CHG_INT_STAT, &val1);
    if (ret < 0)
        goto out;

    dev_err(sc->dev, " subpmic_chg_hk_alert_handler HK_INT_STAT=0x%x,CHG_INT_STAT=0x%x\n", val, val1);
    if (sc->is_sy6976) {
        sc->state.online = (val & BIT(0)) && (val & BIT(1)) && (val1 & BIT(0));
      	if (!last_online && sc->state.online)
          subpmic_chg_set_input_curr_lmt(sc->charger,100);	
    } else {
        sc->state.online = (val & BIT(0)) && (val & BIT(1));
    }
    if (last_online && !sc->state.online) {
        dev_info(sc->dev, "!!! plug out\n");
        if (sc->use_soft_bc12)
            bc12_detect_stop(sc->bc);
        // wait qc_work end
        cancel_work_sync(&sc->qc_detect_work);
        // relese qc
        subpmic_chg_field_write(sc, F_QC_EN, false);
        subpmic_chg_field_write(sc, F_HVDCP_EN, false);
        mutex_unlock(&sc->bc_detect_lock);
        sc->state.vbus_type = NONE;
        sc->is_force_dpdm = false;
    }

    dev_info(sc->dev, "Hourse Keeping State : 0x%x\n", val);

    if (sc->is_sy6976) {
        if (sc->state.online && sc->is_force_dpdm) {
            if (!sc->use_soft_bc12) {
                ret = subpmic_chg_read_byte(sc, SUBPMIC_REG_DPDM_INT_FLAG, &val);
                if (ret < 0) {
                    dev_info(sc->dev, "SUBPMIC_REG_DPDM_INT_FLAG read val error\n");
                    return ret;
                }
                dev_info(sc->dev, "SUBPMIC_REG_DPDM_INT_FLAG val:%x\n",val);
                if (val & SUBPMIC_DPDM_BC12_DETECT_DONE) {
                    result = (val >> 5) & 0x7;
                    sc->state.vbus_type = result;
                    subpmic_chg_set_input_curr_lmt(sc->charger,100);
                    sc->is_force_dpdm = false;
                    mutex_unlock(&sc->bc_detect_lock);
                }
            }
            Charger_Detect_Release();
        }
    }

    last_online = sc->state.online;
    charger_changed(sc->charger);
out:
    return IRQ_HANDLED;
}

/************************** LED Module *******************************/
static int subpmic_chg_led1_flash_enable(struct subpmic_chg_device *sc, bool en)
{
    return subpmic_chg_field_write(sc, F_FLED1_EN, en);
}

static int subpmic_chg_led2_flash_enable(struct subpmic_chg_device *sc, bool en)
{
    return subpmic_chg_field_write(sc, F_FLED2_EN, en);
}

static int subpmic_chg_led1_torch_enable(struct subpmic_chg_device *sc, bool en)
{
    return subpmic_chg_field_write(sc, F_TLED1_EN, en);
}

static int subpmic_chg_led2_torch_enable(struct subpmic_chg_device *sc, bool en)
{
    return subpmic_chg_field_write(sc, F_TLED2_EN, en);
}

static int subpmic_chg_set_led1_flash_curr(struct subpmic_chg_device *sc, int curr)
{
    if (curr < SUBPMIC_LED_FLASH_CURR_MIN) 
        curr = SUBPMIC_LED_FLASH_CURR_MIN;
    if (curr > SUBPMIC_LED_FLASH_CURR_MAX) 
        curr = SUBPMIC_LED_FLASH_CURR_MAX;
    curr = (curr * 1000 - SUBPMIC_LED_FLASH_CURR_OFFSET) / SUBPMIC_LED_FLASH_CURR_STEP;

    return subpmic_chg_field_write(sc, F_FLED1_BR, curr);
}

static int subpmic_chg_set_led2_flash_curr(struct subpmic_chg_device *sc, int curr)
{
    if (curr < SUBPMIC_LED_FLASH_CURR_MIN) 
        curr = SUBPMIC_LED_FLASH_CURR_MIN;
    if (curr > SUBPMIC_LED_FLASH_CURR_MAX) 
        curr = SUBPMIC_LED_FLASH_CURR_MAX;
    curr = (curr * 1000 - SUBPMIC_LED_FLASH_CURR_OFFSET) / SUBPMIC_LED_FLASH_CURR_STEP;

    return subpmic_chg_field_write(sc, F_FLED2_BR, curr);
}

static int subpmic_chg_set_led1_torch_curr(struct subpmic_chg_device *sc, int curr)
{
    if (curr < SUBPMIC_LED_TORCH_CURR_MIN) 
        curr = SUBPMIC_LED_TORCH_CURR_MIN;
    if (curr > SUBPMIC_LED_TORCH_CURR_MAX) 
        curr = SUBPMIC_LED_TORCH_CURR_MAX;
    curr = (curr * 1000 - SUBPMIC_LED_TORCH_CURR_OFFSET) / SUBPMIC_LED_TORCH_CURR_STEP;

    return subpmic_chg_field_write(sc, F_TLED1_BR, curr);
}

static int subpmic_chg_set_led2_torch_curr(struct subpmic_chg_device *sc, int curr)
{
    if (curr < SUBPMIC_LED_TORCH_CURR_MIN) 
        curr = SUBPMIC_LED_TORCH_CURR_MIN;
    if (curr > SUBPMIC_LED_TORCH_CURR_MAX) 
        curr = SUBPMIC_LED_TORCH_CURR_MAX;
    curr = (curr * 1000 - SUBPMIC_LED_TORCH_CURR_OFFSET) / SUBPMIC_LED_TORCH_CURR_STEP;

    return subpmic_chg_field_write(sc, F_TLED2_BR, curr);
}

static int subpmic_chg_set_led_flash_timer(struct subpmic_chg_device *sc, int ms)
{
    int i = 0;

    if (ms < 0) {
        return subpmic_chg_field_write(sc, F_FTIMEOUT_EN, false);
    }

    subpmic_chg_field_write(sc, F_FTIMEOUT_EN, true);

    if (ms < led_time[0])
        ms = led_time[0];
    if (ms > led_time[ARRAY_SIZE(led_time) - 1])
        ms = led_time[ARRAY_SIZE(led_time) - 1];

    for (i = 0; i < ARRAY_SIZE(led_time); i++) {
        if (ms <= led_time[i])
            break;
    }
    return subpmic_chg_field_write(sc, F_FTIMEOUT, i);
}

/*
static int subpmic_chg_set_led_flag_mask(struct subpmic_chg_device *sc, int mask)
{
    uint8_t val = 0;
    int ret;
    ret = subpmic_chg_read_byte(sc, SUBPMIC_REG_LED_MASK, &val);
    if (ret < 0)
        return ret;
    val |= mask;
    return subpmic_chg_write_byte(sc, SUBPMIC_REG_LED_MASK, val);
}

static int subpmic_chg_set_led_flag_unmask(struct subpmic_chg_device *sc, int mask)
{
    uint8_t val = 0;
    int ret;
    ret = subpmic_chg_read_byte(sc, SUBPMIC_REG_LED_MASK, &val);
    if (ret < 0)
        return ret;
    val &= ~mask;
    return subpmic_chg_write_byte(sc, SUBPMIC_REG_LED_MASK, val);
}
*/

static int subpmic_chg_set_led_vbat_min(struct subpmic_chg_device *sc, int mv)
{
    if (mv <= SUBPMIC_LED_VBAT_MIN_MIN)
        mv = SUBPMIC_LED_VBAT_MIN_MIN;
    if (mv >= SUBPMIC_LED_VBAT_MIN_MAX)
        mv = SUBPMIC_LED_VBAT_MIN_MAX;
    mv = (mv - SUBPMIC_LED_VBAT_MIN_OFFSET) / SUBPMIC_LED_VBAT_MIN_STEP;
    return subpmic_chg_field_write(sc, F_VBAT_MIN_FLED, mv);
}

static int subpmic_chg_set_led_flash_curr(struct subpmic_chg_device *sc, int index, int ma)
{
    int ret = 0;
     printk("subpmic_chg_set_led_flash_curr index = %d,ma = %d\n",index, ma);
    switch (index) {
    case LED1_FLASH:
        ret = subpmic_chg_set_led1_flash_curr(sc, ma);
        break;
    case LED2_FLASH:
        ret = subpmic_chg_set_led2_flash_curr(sc, ma);
        break;
    case LED_ALL_FLASH:
        ret = subpmic_chg_set_led1_flash_curr(sc, ma);
        ret |= subpmic_chg_set_led2_flash_curr(sc, ma);
        break;
    default:
        ret = -1;
        break;
    }
    if (ret < 0)
        return ret;

    if (atomic_read(&sc->led_work_running) == 0) {
        atomic_set(&sc->led_work_running, 1);
        reinit_completion(&sc->flash_run);
        reinit_completion(&sc->flash_end);
        schedule_delayed_work(&sc->led_work, msecs_to_jiffies(0));
    }
    dev_info(sc->dev, "[%s] index : %d, curr : %d", __func__, index, ma);
    return 0;
}

/*N6 code for HQ-305606 by xiexinli at 20230710 start*/
__maybe_unused
int subpmic_camera_set_led_flash_curr(int index, int ma){
    if (!g_subpmicdevice)
    return -1;

    return subpmic_chg_set_led_flash_curr(g_subpmicdevice, index, ma);
}

EXPORT_SYMBOL(subpmic_camera_set_led_flash_curr);
/*N6 code for HQ-305606 by xiexinli at 20230710 end*/
static int subpmic_chg_set_led_flash_enable(struct subpmic_chg_device *sc, int index, bool en)
{
    //subpmic_chg_set_led_flag_unmask(sc, SUBPMIC_LED_OVP_MASK);
    sc->led_state = en;
    sc->led_index = index;
    complete(&sc->flash_run);
    if(sc->is_sy6976){
        subpmic_chg_field_write(sc, F_FLED1_EN, en);//reg80 -> 0x81
        dev_info(sc->dev, "[%s] sy6976 need zhis ", __func__);
    }
    dev_info(sc->dev, "[%s] index : %d, en : %d", __func__, index, en);
    return 0;
}

int subpmic_camera_set_led_flash_enable(int index, bool en){
    if (!g_subpmicdevice)
    return -1;
    return subpmic_chg_set_led_flash_enable(g_subpmicdevice, index, en);
}
EXPORT_SYMBOL(subpmic_camera_set_led_flash_enable);
/*N6 code for HQ-305606 by xiexinli at 20230710 end*/
int subpmic_chg_set_led_torch_curr(struct subpmic_chg_device *sc, int index, int ma)
{
    int ret;
    dev_info(sc->dev, "[%s] index : %d", __func__, index);
    if (index == 0) {
        ret = subpmic_chg_set_led1_torch_curr(sc, ma);
    } else {
        ret = subpmic_chg_set_led2_torch_curr(sc, ma);
    }
    return ret;
}

__maybe_unused
int subpmic_camera_set_led_torch_curr(int index, int ma){
    if (!g_subpmicdevice){
    return -1;
    }
    return subpmic_chg_set_led_torch_curr(g_subpmicdevice, index, ma);
}

EXPORT_SYMBOL(subpmic_camera_set_led_torch_curr);


int subpmic_chg_set_led_torch_enable(struct subpmic_chg_device *sc, int index, bool en)
{
    int ret;
    dev_info(sc->dev, "[%s] index : %d", __func__, index);
    if (index == 0) {
        ret = subpmic_chg_led1_torch_enable(sc, en);//reg80 -> 0x21
    } else {
        ret = subpmic_chg_led2_torch_enable(sc, en);
    }
    return ret;
}


__maybe_unused
int subpmic_camera_set_led_torch_enable(int index, bool en){
    if (!g_subpmicdevice){
    return -1;
    }
    return subpmic_chg_set_led_torch_enable(g_subpmicdevice, index, en);
}
EXPORT_SYMBOL(subpmic_camera_set_led_torch_enable);

static void subpmic_chg_led_flash_done_workfunc(struct work_struct *work)
{
    struct subpmic_chg_device *sc = container_of(work, 
                    struct subpmic_chg_device, led_work.work);
    struct charger_dev *charger = sc->charger;
    int ret = 0, now_vbus = 0;
    bool in_otg = false;

    ret = subpmic_chg_get_otg_status(charger, &in_otg);
    if (ret < 0)
        return;

    if (in_otg)
        goto en_led_flash;

    // mask irq
    disable_irq(sc->irq[IRQ_HK]);
    disable_irq(sc->irq[IRQ_BUCK]);

    // must todo
    ret = subpmic_chg_set_chg(sc->charger, false);
    ret |= subpmic_chg_field_write(sc, F_DIS_BUCKCHG_PATH, true);
    ret |= subpmic_chg_field_write(sc, F_DIS_SLEEP_FOR_OTG, true);
    // set otg volt curr
    now_vbus = subpmic_chg_get_adc(sc, ADC_VBUS) / 1000;
    dev_info(sc->dev, "now vbus = %d\n",now_vbus);
    if (now_vbus < 0)
        goto err_set_acdrv;
    if (now_vbus == 0)
        ret |= subpmic_chg_set_otg_volt(sc->charger, 5000);
    else 
        ret |= subpmic_chg_set_otg_volt(sc->charger, now_vbus);
    if (ret < 0) {
        goto err_set_acdrv;
    }

en_led_flash:
    ret = subpmic_chg_request_otg(sc, 1, true);
    if (ret < 0) {
        goto err_set_otg;
    }
    // wait flash en cmd
    // todo
    if (wait_for_completion_timeout(&sc->flash_run,
                        msecs_to_jiffies(60000)) < 0) {
        goto err_en_flash;
    }
    if (!sc->led_state) {
        goto err_en_flash;
    }
    // open flash led
    switch (sc->led_index) {
    case LED1_FLASH:
        ret = subpmic_chg_led1_flash_enable(sc, true);
        break;
    case LED2_FLASH:
        ret = subpmic_chg_led2_flash_enable(sc, true);
        break;
    case LED_ALL_FLASH:
        ret = subpmic_chg_led1_flash_enable(sc, true);
        ret |= subpmic_chg_led2_flash_enable(sc, true);
        break;
    }
    if (ret < 0) {
        // Must close otg after otg set success
        goto err_en_flash;
    }

    wait_for_completion_timeout(&sc->flash_end, msecs_to_jiffies(1000));

    switch (sc->led_index) {
    case LED1_FLASH:
        subpmic_chg_led1_flash_enable(sc, false);
        break;
    case LED2_FLASH:
        subpmic_chg_led2_flash_enable(sc, false);
        break;
    case LED_ALL_FLASH:
        subpmic_chg_led1_flash_enable(sc, false);
        subpmic_chg_led2_flash_enable(sc, false);
        break;
    }

err_en_flash:
    subpmic_chg_request_otg(sc, 1, false);
    if (in_otg)
        goto out;

err_set_otg:
    subpmic_chg_set_otg_volt(sc->charger, 5000);
err_set_acdrv:
    // unmask irq
    subpmic_chg_field_write(sc, F_DIS_BUCKCHG_PATH, false);
    subpmic_chg_field_write(sc, F_DIS_SLEEP_FOR_OTG, false);
    //subpmic_chg_set_led_flag_mask(sc, SUBPMIC_LED_OVP_MASK);
    mdelay(300);
    enable_irq(sc->irq[IRQ_HK]);
    enable_irq(sc->irq[IRQ_BUCK]);
    subpmic_chg_hk_alert_handler(0, sc);
    subpmic_chg_buck_alert_handler(0, sc);
out:
    atomic_set(&sc->led_work_running, 0);
    return;
}
/***************************************************************************/
static int subpmic_chg_request_irq_thread(struct subpmic_chg_device *sc)
{
    int i = 0, ret = 0;
    const struct {
        char *name;
        irq_handler_t hdlr;
    } subpmic_chg_chg_irqs[] = {
        {"Hourse Keeping", subpmic_chg_hk_alert_handler},
        {"Buck Charger", subpmic_chg_buck_alert_handler},
        {"DPDM", subpmic_chg_dpdm_alert_handler},
        {"LED", subpmic_chg_led_alert_handler},
    };

    for (i = 0; i < ARRAY_SIZE(subpmic_chg_chg_irqs); i++) {
        ret = platform_get_irq_byname(to_platform_device(sc->dev),
                                    subpmic_chg_chg_irqs[i].name);
        if (ret < 0) {
            dev_err(sc->dev, "failed to get irq %s\n",subpmic_chg_chg_irqs[i].name);
            return ret;
        }

        sc->irq[i] = ret;

        dev_info(sc->dev, "%s irq = %d\n", subpmic_chg_chg_irqs[i].name, ret);
        ret = devm_request_threaded_irq(sc->dev, ret, NULL,
                        subpmic_chg_chg_irqs[i].hdlr, IRQF_ONESHOT,
                        dev_name(sc->dev), sc);
        if (ret < 0) {
            dev_err(sc->dev, "failed to request irq %s\n", subpmic_chg_chg_irqs[i].name);
            return ret;
        }

    }

    return 0;
}

static int subpmic_chg_enter_test_mode(struct subpmic_chg_device *sc, bool en)
{
    char str[] = "ENTERPRISE";
    uint8_t val;
    int ret, i;
    do {
        ret = subpmic_chg_read_byte(sc, 0xc1, &val);
        // not in test mode
        if (ret < 0 && !en) {
            dev_info(sc->dev, "not in test mode\n");
            break;
        }
        // in test mode
        if (ret >= 0 && val == 0 && en) {
            dev_info(sc->dev, "in test mode\n");
            break;
        }
        for (i = 0; i < (ARRAY_SIZE(str) - 1); i++) {
            ret = subpmic_chg_write_byte(sc, 0x5d, str[i]);
            if (ret < 0)
                return ret;
        }
    } while (true);

    return 0;
}

static int subpmic_chg_led_hw_init(struct subpmic_chg_device *sc)
{
    int ret;
    // select external source
    ret = subpmic_chg_field_write(sc, F_LED_POWER, 0);
    ret |= subpmic_chg_set_led_vbat_min(sc, 2800);
    // set flash timeout disable
    ret |= subpmic_chg_set_led_flash_timer(sc, SUBPMIC_DEFAULT_FLASH_TIMEOUT);
    //ret |= subpmic_chg_set_led_flag_mask(sc, SUBPMIC_LED_OVP_MASK);
    return ret;
}

static int subpmic_cid_en(struct subpmic_chg_device *sc)
{
    int ret = 0;

    if (sc->is_sy6976) {
        ret |= subpmic_chg_field_write(sc, F_SY_CID_SEL, true);
        ret |= subpmic_chg_field_write(sc, F_SY_CID_EN, true);
    } else {
        ret |= subpmic_chg_field_write(sc, F_SC_CID_EN, true);
    }

    if(ret < 0) {
        pr_err("%s,enable cid failed!\n", __func__);
        return ret;
    }
    return 0;
}

extern int chg_sc660x_set_continuous_time(struct tcpc_device *tcpc);
static int subpmic_chg_hw_init(struct subpmic_chg_device *sc)
{
    int ret, i = 0;
    u8 val = 0;
    struct charger_dev *charger = sc->charger;
    const struct {
        enum subpmic_chg_fields field;
        u8 val;
    } buck_init[] = {
        {F_BATSNS_EN,       sc->buck_init.batsns_en},
        {F_VBAT,            (sc->buck_init.vbat - SUBPMIC_BUCK_VBAT_OFFSET) / SUBPMIC_BUCK_VBAT_STEP},
        {F_ICHG_CC,         (sc->buck_init.ichg - SUBPMIC_BUCK_ICHG_OFFSET) / SUBPMIC_BUCK_ICHG_STEP},
        {F_IINDPM_DIS,      sc->buck_init.iindpm_dis},
        {F_VBAT_PRECHG,     sc->buck_init.vprechg},
        {F_IPRECHG,         (sc->buck_init.iprechg - SUBPMIC_BUCK_IPRECHG_OFFSET) / SUBPMIC_BUCK_IPRECHG_STEP},
        {F_TERM_EN,         sc->buck_init.iterm_en},
        {F_ITERM,           (sc->buck_init.iterm - SUBPMIC_BUCK_ITERM_OFFSET) / SUBPMIC_BUCK_ITERM_STEP},
        {F_RECHG_DIS,       sc->buck_init.rechg_dis},
        {F_RECHG_DG,        sc->buck_init.rechg_dg},
        {F_VRECHG,          sc->buck_init.rechg_volt},
        {F_CONV_OCP_DIS,    sc->buck_init.conv_ocp_dis},
        {F_TSBAT_JEITA_DIS, true},
        {F_IBAT_OCP_DIS,    sc->buck_init.ibat_ocp_dis},
        {F_VPMID_OVP_OTG_DIS,sc->buck_init.vpmid_ovp_otg_dis},
        {F_VBAT_OVP_BUCK_DIS,sc->buck_init.vbat_ovp_buck_dis},
        {F_IBATOCP,         sc->buck_init.ibat_ocp},
        {F_QB_EN,           false},
        {F_ACDRV_MANUAL_EN, true},
        {F_ICO_EN,          false},
        {F_EDL_ACTIVE_LEVEL,true},
        {F_ACDRV_MANUAL_PRE,true},
        {F_HVDCP_EN,        false},
        {F_CHG_TIMER,       sc->buck_init.safety_timer},
    };

    // reset all registers without flag registers
    ret = subpmic_chg_chip_reset(sc);
    // this need watchdog, if i2c operate failed, watchdog reset
    ret |= subpmic_chg_set_chg(charger, false);
    // set buck freq = 1M , boost freq = 1M
    ret |= subpmic_chg_write_byte(sc, SUBPMIC_REG_CHG_CTRL + 4, 0x8a);
    ret |= subpmic_chg_set_chg(charger, true);

    ret |= subpmic_chg_set_wd_timeout(charger, 0);

    if (!sc->is_sy6976){
        ret |= subpmic_chg_enter_test_mode(sc, true);
#if 1
        sc->tcpc = tcpc_dev_get_by_name("type_c_port0");
        if (sc->tcpc) {
		pr_err("gx set pd reg 0xe7\n");
        	chg_sc660x_set_continuous_time(sc->tcpc);
        }
#endif
        ret |= subpmic_chg_write_byte(sc, 0x75, 0x09);
        ret |= subpmic_chg_write_byte(sc, 0x8c, 0x44);
        ret |= subpmic_chg_read_byte(sc, 0xFC, &val);
        val |= BIT(5);
        ret |= subpmic_chg_write_byte(sc, 0xFC, val);
        ret |= subpmic_chg_enter_test_mode(sc, false);
    }
    
    ret |= subpmic_chg_set_vac_ovp(sc, 12000);

    ret |= subpmic_chg_set_vbus_ovp(sc, 12000);

    ret |= subpmic_chg_set_ico_enable(sc, false);

    if (!sc->is_sy6976)
    {
        ret |= subpmic_chg_set_acdrv(sc, true);
    }

    ret |= subpmic_chg_set_sys_volt(sc, 3500);

    ret |= subpmic_chg_mask_hk_irq(sc, SUBPMIC_HK_RESET_MASK |
                    SUBPMIC_HK_ADC_DONE_MASK | SUBPMIC_HK_REGN_OK_MASK |
                    SUBPMIC_HK_VAC_PRESENT_MASK);

    ret |= subpmic_chg_mask_buck_irq(sc, SUBPMIC_BUCK_ICO_MASK |
                    SUBPMIC_BUCK_IINDPM_MASK | SUBPMIC_BUCK_VINDPM_MASK |
                    SUBPMIC_BUCK_CHG_MASK | SUBPMIC_BUCK_QB_ON_MASK |
                    SUBPMIC_BUCK_VSYSMIN_MASK);

    ret |= subpmic_chg_auto_dpdm_enable(sc, false);

    // mask qc int
    ret |= subpmic_chg_write_byte(sc, SUBPMIC_REG_QC3_INT_MASK, 0xFF);

    for (i = 0;i < ARRAY_SIZE(buck_init);i++) {
        ret |= subpmic_chg_field_write(sc, 
                    buck_init[i].field, buck_init[i].val);
    }

    ret |= subpmic_chg_led_hw_init(sc);

    /* set jeita, cool -> 5, warm -> 54.5 */
    ret |= subpmic_chg_field_write(sc, F_JEITA_COOL_TEMP, 0);
    ret |= subpmic_chg_field_write(sc, F_JEITA_WARM_TEMP, 3);
    ret |= subpmic_chg_field_write(sc, F_BATFET_RST_EN, 0);

    ret |= subpmic_chg_write_byte(sc, 0x60, 0xb0);
    ret |= subpmic_cid_en(sc);
    if(ret < 0)
        return ret;
    pr_err("%s,succ\n", __func__);
    return 0;
}

static struct soft_bc12_ops bc12_ops = {
    .init = subpmic_chg_bc12_init,
    .deinit = subpmic_chg_bc12_deinit,
    .update_bc12_state = subpmic_chg_update_bc12_state,
    .set_bc12_state = subpmic_chg_set_bc12_state,
    .get_vbus_online = subpmic_chg_bc12_get_vbus_online,
};

#ifdef CONFIG_SOUTHCHIP_CHARGER_CLASS_MODULE
static struct charger_ops charger_ops = {
    .get_adc = subpmic_chg_charger_get_adc,
    .get_vbus_type = subpmic_chg_get_vbus_type,
    .get_online = subpmic_chg_get_online,
    .is_charge_done = subpmic_chg_is_charge_done,
    .get_hiz_status = subpmic_chg_get_hiz_status,
    .get_chg_status1 = subpmic_chg_get_status,
    .get_input_volt_lmt = subpmic_chg_get_input_volt_lmt,
    .get_input_curr_lmt = subpmic_chg_get_input_curr_lmt,
    .get_chg_status = subpmic_chg_get_chg_status,
    .get_otg_status = subpmic_chg_get_otg_status,
    .get_term_curr = subpmic_chg_get_term_curr,
    .get_term_volt = subpmic_chg_get_term_volt,
    .set_hiz = subpmic_chg_set_hiz,
    .set_input_curr_lmt = subpmic_chg_set_input_curr_lmt,
    .set_input_volt_lmt = subpmic_chg_set_input_volt_lmt,
    .set_ichg = subpmic_chg_set_ichg,
    .get_ichg = subpmic_chg_get_ichg,
    .set_chg = subpmic_chg_set_chg,
    .set_otg = subpmic_chg_normal_request_otg,
    .set_otg_curr = subpmic_chg_set_otg_curr,
    .set_otg_volt = subpmic_chg_set_otg_volt,
    .set_term = subpmic_chg_set_term,
    .set_term_curr = subpmic_chg_set_term_curr,
    .set_term_volt = subpmic_chg_set_term_volt,
    .adc_enable = subpmic_chg_adc_enable,
    .set_prechg_volt = subpmic_chg_set_prechg_volt,
    .set_prechg_curr = subpmic_chg_set_prechg_curr,
    .force_dpdm = subpmic_chg_force_dpdm,
    .request_dpdm = subpmic_chg_request_dpdm,
    .set_wd_timeout = subpmic_chg_set_wd_timeout,
    .kick_wd = subpmic_chg_kick_wd,
    .set_shipmode = sc_chg_set_shipmode,
    .set_rechg_vol = subpmic_chg_set_rechg_vol,
    .qc_identify = subpmic_chg_qc_identify,
    .is_sy6976 = subpmic_chg_is_sy6976,
};
#endif /* CONFIG_HUAQIN_CHARGER_CLASS */

static int subpmic_chg_parse_dtb(struct subpmic_chg_device *sc,
                                struct device_node *np)
{
    int ret, i;
    const struct {
        const char * name;
        u32 * val;
    } buck_data[] = {
        {"sc,vsys-limit",       &(sc->buck_init.vsyslim)},
        {"sc,batsnc-enable",    &(sc->buck_init.batsns_en)},
        {"sc,vbat"      ,       &(sc->buck_init.vbat)},
        {"sc,charge-curr",      &(sc->buck_init.ichg)},
        {"sc,iindpm-disable",   &(sc->buck_init.iindpm_dis)},
        {"sc,input-curr-limit", &(sc->buck_init.iindpm)},
        {"sc,ico-enable",       &(sc->buck_init.ico_enable)},
        {"sc,iindpm-ico",       &(sc->buck_init.iindpm_ico)},
        {"sc,precharge-volt",   &(sc->buck_init.vprechg)},
        {"sc,precharge-curr",   &(sc->buck_init.iprechg)},
        {"sc,term-en",          &(sc->buck_init.iterm_en)},
        {"sc,term-curr",        &(sc->buck_init.iterm)},
        {"sc,rechg-dis",        &(sc->buck_init.rechg_dis)},
        {"sc,rechg-dg",         &(sc->buck_init.rechg_dg)},
        {"sc,rechg-volt",       &(sc->buck_init.rechg_volt)},
        {"sc,boost-voltage",    &(sc->buck_init.vboost)},
        {"sc,boost-max-current",&(sc->buck_init.iboost)},
        {"sc,conv-ocp-dis",     &(sc->buck_init.conv_ocp_dis)},
        {"sc,tsbat-jeita-dis",  &(sc->buck_init.tsbat_jeita_dis)},
        {"sc,ibat-ocp-dis",     &(sc->buck_init.ibat_ocp_dis)},
        {"sc,vpmid-ovp-otg-dis",&(sc->buck_init.vpmid_ovp_otg_dis)},
        {"sc,vbat-ovp-buck-dis",&(sc->buck_init.vbat_ovp_buck_dis)},
        {"sc,ibat-ocp",         &(sc->buck_init.ibat_ocp)},
        {"sc,safety-timer",     &(sc->buck_init.safety_timer)},
    };

    for (i = 0;i < ARRAY_SIZE(buck_data);i++) {
        ret = of_property_read_u32(np, buck_data[i].name,
                                      buck_data[i].val);
        if (ret < 0) {
            dev_err(sc->dev, "not find property %s\n",  
                                buck_data[i].name);
            return ret;
        }else{
            dev_info(sc->dev,"%s: %d\n", buck_data[i].name,
                        (int)*buck_data[i].val);
        }
    } 
    return 0;
}

#ifdef CONFIG_ENABLE_SYSFS_DEBUG
static ssize_t subpmic_chg_show_regs(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
    struct subpmic_chg_device *sc = dev_get_drvdata(dev);
    return subpmic_chg_dump_regs(sc, buf);
}

static int get_parameters(char *buf, unsigned long *param, int num_of_par)
{
	int cnt = 0;
	char *token = strsep(&buf, " ");

	for (cnt = 0; cnt < num_of_par; cnt++) {
		if (token) {
			if (kstrtoul(token, 0, &param[cnt]) != 0)
				return -EINVAL;

			token = strsep(&buf, " ");
		} else
			return -EINVAL;
	}

	return 0;
}

static ssize_t subpmic_chg_test_store_property(struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    struct subpmic_chg_device *sc = dev_get_drvdata(dev);
    int ret/*, i*/;
	long int val;
    ret = get_parameters((char *)buf, &val, 1);
    if (ret < 0) {
        dev_err(dev, "get parameters fail\n");
        return -EINVAL;
    }

    switch (val) {
    case 1: /* enable otg */
        subpmic_chg_request_otg(sc, 0, true);
        break;
    case 2: /* disenable otg */
        subpmic_chg_request_otg(sc, 0, false);
        break;
    case 3: /* open led1 flash mode */
        subpmic_chg_set_led_flash_curr(sc, LED1_FLASH, 500);
        subpmic_chg_set_led_flash_enable(sc, LED1_FLASH, true);
        break;
    case 4:
        subpmic_chg_set_led_flash_enable(sc, LED1_FLASH, false);
        break;
    case 5:
	subpmic_chg_set_led_torch_curr(sc, LED1_FLASH, 250);
	subpmic_chg_set_led_torch_enable(sc, LED1_FLASH, true);
        break;
    case 6:
	subpmic_chg_set_led_torch_enable(sc, LED1_FLASH, false);
        break;
    case 7:
        subpmic_chg_set_led_flash_enable(sc, LED1_FLASH, true);
	subpmic_chg_set_led_torch_enable(sc, LED2_FLASH, true);
        break;
    case 8:
        subpmic_chg_set_led_flash_enable(sc, LED_ALL_FLASH, true);
	subpmic_chg_set_led_torch_enable(sc, LED1_FLASH, true);
	subpmic_chg_set_led_flash_enable(sc, LED2_FLASH, true);
        break;
    case 9:
        subpmic_chg_set_led_flash_enable(sc, LED_ALL_FLASH, false);
        break;
    case 10:
        switch (subpmic_chg_qc_identify(sc->charger)) {
        case QC2_MODE:
            subpmic_chg_request_qc20(sc->charger, 9000);
            break;
        case QC3_MODE:
            subpmic_chg_request_qc30(sc->charger, 9000);
            mdelay(2000);
            subpmic_chg_request_qc30(sc->charger, 5000);
            break;
        case QC3_5_18W_MODE:
        case QC3_5_27W_MODE:
        case QC3_5_45W_MODE:
            subpmic_chg_request_qc35(sc->charger, 9000);
            mdelay(2000);
            subpmic_chg_request_qc35(sc->charger, 5000);
            break;
        default:
            break;    
        }
        break;
    case 11:
        subpmic_chg_request_qc35(sc->charger, 9000);
        break;
    case 12:
        subpmic_chg_field_write(sc, F_QC3_PULS, true);
        break;
    case 13:
        subpmic_chg_field_write(sc, F_QC3_MINUS, true);
        break;
    default:
        break;
    }

    return count;
}

static ssize_t subpmic_chg_test_show_property(struct device *dev,
                    struct device_attribute *attr,
                    char *buf)
{
    int ret;
    ret = snprintf(buf, 256, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
        "1: otg enable", "2: otg disable", 
        "3: en led1 flash", "4: dis led1 flash",
        "5: en led1 torch", "6: dis led1 torch","7: enter shipmode");
    return ret;
}

static DEVICE_ATTR(showregs, 0440, subpmic_chg_show_regs, NULL);
static DEVICE_ATTR(test, 0660, subpmic_chg_test_show_property, 
                                subpmic_chg_test_store_property);

static void subpmic_chg_sysfs_file_init(struct device *dev)
{
    device_create_file(dev, &dev_attr_showregs);
    device_create_file(dev, &dev_attr_test);
}
#endif /* CONFIG_ENABLE_SYSFS_DEBUG */

int chg_batt_id_res = 0;
EXPORT_SYMBOL(chg_batt_id_res);
static int get_battery_id_res(struct subpmic_chg_device *sc)
{
	int ret = 0;
	int val = 0;
	int i;
	for(i = 0; i < 14; i++){
		ret = subpmic_chg_adc_enable(sc->charger, true);
		ret = subpmic_chg_charger_get_adc(sc->charger, SC_ADC_BATTID, &val);
		if(ret < 0)
			pr_err("[%s]real fail,ret = %d\n", __func__, ret);
		pr_err("[%s]batid_res = %d,i = %d\n", __func__,val, i);
		if(val != 0)
			break;
	}
	ret = subpmic_chg_adc_enable(sc->charger, false);
	return val;
}

static int subpmic_chg_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct subpmic_chg_device *sc;
    int i, ret;
    u8 did = 0;
    dev_info(dev, "%s (%s)\n", __func__, SUBPMIC_CHARGER_VERSION);

    sc = devm_kzalloc(dev, sizeof(*sc), GFP_KERNEL);
    if (!sc)
        return -ENOMEM;

    sc->rmap = dev_get_regmap(dev->parent, NULL);
    if (!sc->rmap) {
        dev_err(dev, "failed to get regmap\n");
        return -ENODEV;
    }

    ret = regmap_bulk_read(sc->rmap, SUBPMIC_REG_HK_DID, &did, 1);
    pr_err("%s did = 0x%x\n", __func__, did);
    if (did == SY6976_DEVICE_ID){
        sc->is_sy6976 = true;
        hardwareinfo_set_prop(HARDWARE_CHARGER_IC, "SY6976_CHARGER");
    } else if (did == SC6601A_DEVICE_ID){
        sc->is_sc6601 = true;
        hardwareinfo_set_prop(HARDWARE_CHARGER_IC, "SC6601_CHARGER");
    } else {
    	sc->is_sy6976 = false;
    	sc->is_sc6601 = false; 
    }


    sc->dev = dev;
    platform_set_drvdata(pdev, sc);

    for (i = 0; i < ARRAY_SIZE(subpmic_chg_reg_fields); i++) {
        sc->rmap_fields[i] = devm_regmap_field_alloc(dev, 
                            sc->rmap, subpmic_chg_reg_fields[i]);
        if (IS_ERR(sc->rmap_fields[i])) {
            dev_err(dev, "cannot allocate regmap field\n");
            return PTR_ERR(sc->rmap_fields[i]);
        }
    }
#ifdef CONFIG_SOUTHCHIP_CHARGER_CLASS_MODULE
    sc->charger = charger_register("sc_charger",
                             sc->dev, &charger_ops, sc);
    if (!sc->charger) {
        ret = PTR_ERR(sc->charger);
        goto err;
    }
#endif /* CONFIG_SOUTHCHIP_CHARGER_CLASS_MODULE */

    ret = subpmic_chg_parse_dtb(sc, dev->of_node);
    if (ret < 0) {
        dev_err(dev, "dtb parse failed\n");
        goto err_1;
    }

    ret = subpmic_chg_hw_init(sc);
    if (ret < 0) {
        dev_err(dev, "hw init failed\n");
        goto err_1;
    }

    ret = subpmic_chg_request_irq_thread(sc);
    if (ret < 0) {
        dev_err(dev, "irq request failed\n");
        goto err_1;
    }

    sc->use_soft_bc12 = false;
    if (sc->use_soft_bc12) {
        sc->bc = bc12_register(sc, &bc12_ops, false);
        if (!sc->bc) {
            ret = PTR_ERR(sc->bc);
            goto err_1;
        }
        sc->bc12_result_nb.notifier_call = subpmic_chg_bc12_notify_cb;
        bc12_register_notifier(sc->bc, &sc->bc12_result_nb);
    }
    
    mutex_init(&sc->bc_detect_lock);
    INIT_WORK(&sc->qc_detect_work, qc_detect_workfunc);
    INIT_DELAYED_WORK(&sc->led_work, subpmic_chg_led_flash_done_workfunc);
    init_completion(&sc->flash_end);
    init_completion(&sc->flash_run);
    sc->ship_mode = false;
    sc->led_state = false;
    sc->request_otg = 0;
    sc->last_chg_done = false;
    g_subpmicdevice = sc;

#ifdef CONFIG_ENABLE_SYSFS_DEBUG
    subpmic_chg_sysfs_file_init(sc->dev);
#endif /* CONFIG_ENABLE_SYSFS_DEBUG */

    subpmic_chg_hk_alert_handler(0, sc);
    subpmic_chg_buck_alert_handler(0, sc);
    subpmic_chg_dpdm_alert_handler(0, sc);
    subpmic_chg_led_alert_handler(0, sc);
    chg_batt_id_res = get_battery_id_res(sc);
    dev_info(dev, "probe success\n");

    return 0;

err_1:
#ifdef CONFIG_SOUTHCHIP_CHARGER_CLASS_MODULE
    charger_unregister(sc->charger);
#endif /* CONFIG_SOUTHCHIP_CHARGER_CLASS_MODULE */
err:
    dev_info(dev, "probe failed\n");
    return ret;
}

static int subpmic_chg_remove(struct platform_device *pdev)
{
    struct subpmic_chg_device *sc = platform_get_drvdata(pdev);
    int i = 0;
    for (i = 0; i < IRQ_MAX; i++) {
        disable_irq(sc->irq[i]);
    }

    if (sc->use_soft_bc12) {
        bc12_unregister_notifier(sc->bc, &sc->bc12_result_nb);
        bc12_unregister(sc->bc);
    }
#ifdef CONFIG_SOUTHCHIP_CHARGER_CLASS_MODULE
    charger_unregister(sc->charger);
#endif /* CONFIG_SOUTHCHIP_CHARGER_CLASS_MODULE */
    return 0;
}

static void subpmic_chg_shutdown(struct platform_device *pdev)
{
    struct subpmic_chg_device *sc = platform_get_drvdata(pdev);
    int ret = 0;

    if(sc->ship_mode == true) {
        sc->ship_mode = false;
        ret |= subpmic_chg_field_write(sc, F_ACDRV_MANUAL_EN, false);
        if (ret < 0)
            dev_info(sc->dev, "%s: set ACDRV mode fail\n", __func__);
        else
            dev_info(sc->dev, "%s: set ACDRV auto-mode success\n", __func__);
        dev_info(sc->dev, "%s: sc->ship_mode = %d\n",__func__, sc->ship_mode);
        ret = subpmic_chg_set_shipmode(sc, true);
        if (ret < 0)
            dev_info(sc->dev, "%s: set ship mode fail\n", __func__);
        else
            dev_info(sc->dev, "%s: set ship mode success\n", __func__);
        ret = subpmic_chg_field_read(sc,F_BATFET_DIS);
	    dev_info(sc->dev, "%s: F_BATFET_DIS:%d\n", __func__,ret);
        mdelay(100);
    }
    else{
        ret = subpmic_chg_field_read(sc, F_QC_EN);
        dev_info(sc->dev, "%s: qc_en =%d\n", __func__,ret);
        ret = subpmic_chg_field_write(sc, F_QC_EN, false);
        if (ret < 0)
            dev_info(sc->dev, "%s: write qc_en fail\n", __func__);
    }
}

static const struct of_device_id subpmic_chg_of_match[] = {
    {.compatible = "southchip,subpmic_chg",},
    {.compatible = "sy6976,subpmic_chg",},
    {},
};
MODULE_DEVICE_TABLE(of, subpmic_chg_of_match);

static struct platform_driver subpmic_chg_driver = {
    .driver = {
        .name = "subpmic_chg",
        .of_match_table = of_match_ptr(subpmic_chg_of_match),
    },
    .probe = subpmic_chg_probe,
    .remove = subpmic_chg_remove,
    .shutdown = subpmic_chg_shutdown,
};

module_platform_driver(subpmic_chg_driver);

MODULE_AUTHOR("Yongsheng Zhan <Yongsheng-Zhan@southchip.com>");
MODULE_DESCRIPTION("sc6601 driver");
MODULE_LICENSE("GPL v2");
