/* SPDX-License-Identifier: GPL-2.0 */
/*
 * sy6541.h
 *
 * charge-pump ic driver
 *
 * Copyright (c) 2023-2023 Xiaomi Technologies Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */
#ifndef __SY6541_HEADER__
#define __SY6541_HEADER__

#define CP_FORWARD_4_TO_1             0
#define CP_FORWARD_2_TO_1             1
#define CP_FORWARD_1_TO_1             2

#define USB_OVP_TH_4TO1	 22000
#define USB_OVP_TH_2TO1	 14000
#define USB_OVP_TH_1TO1	 6500
#define BUS_OVP_TH_1TO1	 6000
#define BAT_OCP_TH 8000
#define WPC_OVP_TH  22000
#define OUT_OVP_TH 5000
#define PMID2OUT_UVP_TH 100
#define PMID2OUT_OVP_TH 600
#define BAT_OVP_TH 4500
#define BAT_OVP_ALARM_TH 4400
#define CP_DEFAULT_FSW 600

#define ERROR_RECOVERY_COUNT	5

enum sy6541_cp_mode {
	CP_MODE_FORWARD_4_1 = 0,
	CP_MODE_FORWARD_2_1,
	CP_MODE_FORWARD_1_1,
	CP_MODE_RESERVED_3,
	CP_MODE_REVERSE_1_4,
	CP_MODE_REVERSE_1_2,
	CP_MODE_REVERSE_1_1,
	CP_MODE_RESERVED_7,
	CP_MODE_MAX,
};

enum sy6541_cp_sts {
	VOUT_OK_REV_STAT,
	VOUT_OK_CHG_STAT,
	VOUT_INSERT_STAT,
	VBUS_PRESENT_STAT,
	VWPC_PRESENT_STAT,
	VUSB_PRESENT_STAT,
};

enum sy6541_cp_pmid_error_stat {
	CP_PMID_ERROR_OK,
	CP_PMID_ERROR_LOW,
	CP_PMID_ERROR_HIGH,
};

enum  sc8581_adc_ch {
	ADC_IBUS,
	ADC_VBUS,
	ADC_VUSB,
	ADC_VWPC,
	ADC_VOUT,
	ADC_VBAT,
	ADC_TDIE,
	ADC_MAX_NUM,
};

enum cp_number {
	SC8581_MASTER,
	SC8581_SLAVE,
};

enum chg_mode {
	CP_MODE_DIV4,
	CP_MODE_DIV2,
	CP_MODE_DIV1,
	CP_MODE_DIV_MAX,
};

static const char* sy6541_psy_name[] = {
    [SC8581_MASTER] = "cp_master",
    [SC8581_SLAVE] = "cp_slave",
};

struct sy6541_cfg {
	unsigned int bat_ovp_th;
	unsigned int bat_ovp_alarm_th;
	unsigned int wpc_ovp_th;
	unsigned int out_ovp_th;
	unsigned int pmid2out_uvp_th;
	unsigned int pmid2out_ovp_th;
	unsigned int bus_ovp_th[CP_MODE_DIV_MAX];
	unsigned int usb_ovp_th[CP_MODE_DIV_MAX];
	unsigned int bus_ocp_th[CP_MODE_DIV_MAX];

};

struct sy65411_fsw_cfg {
	int max;
	int min;
	int step;
};

struct sy6541_device {
	struct i2c_client *client;
	struct device *dev;
	struct device *sysfs_dev;
	struct charger_device *chg_dev;
	struct charger_properties chg_props;
	struct power_supply_desc psy_desc;
	struct power_supply_config psy_cfg;
	struct power_supply *fc2_psy;
	struct regmap *regmap;
	struct sy6541_cfg cfg;
	struct sy65411_fsw_cfg fsw_cfg;
	bool chip_ok;
	char log_tag[25];
	int work_mode;
	int operation_mode;
	int chip_vendor;
	u8 adc_mode;
	unsigned int revision;
	unsigned int product_cfg;
	int cp_role;

	struct delayed_work irq_handle_work;
	int irq_gpio;
	int irq;
	int nlpm_gpio;
	int vbat_volt;
	bool ovpgate_en;
	bool i2c_is_working;
	bool support_reverse_quick_charge;
	bool usb_present;
};

struct mtk_cp_sysfs_field_info {
	struct device_attribute attr;
	enum cp_property prop;
	int (*set)(struct sy6541_device *sc,
		struct mtk_cp_sysfs_field_info *attr, int val);
	int (*get)(struct sy6541_device *sc,
		struct mtk_cp_sysfs_field_info *attr, int *val);
};

/* Register 00h */
#define SY6541_REG_00									0x00
#define SY6541_DEVICE_VER								0x09
/* Register 01h */
#define SY6541_REG_01									0x01
#define SY6541_BAT_OVP_DIS_MASK							0x80
#define SY6541_BAT_OVP_DIS_SHIFT						7
#define SY6541_BAT_OVP_ENABLE							0
#define SY6541_BAT_OVP_DISABLE							1
#define SY6541_BAT_OVP_MASK_MASK						0x40
#define SY6541_BAT_OVP_MASK_SHIFT						6
#define SY6541_BAT_OVP_NOT_MASK							0
#define SY6541_BAT_OVP_IS_MASK							1
#define SY6541_BAT_OVP_FLAG_MASK						0x20
#define SY6541_BAT_OVP_FLAG_SHIFT						5
#define SY6541_BAT_OVP_MASK								0x1f
#define SY6541_BAT_OVP_SHIFT							0
#define SY6541_BAT_OVP_BASE								4450
#define SY6541_BAT_OVP_LSB								25
/* Register 02h */
#define SY6541_REG_02									0x02
#define SY6541_OVPFET_DRV_CURR_MASK						0xF0
#define SY6541_OVPFET_DRV_CURR_SHIFT					4
#define SY6541_OVPFET_DRV_CURR_BASE						8
#define SY6541_OVPFET_DRV_CURR_LSB						5/10
#define SY6541_VBUS_ERRORHI_RF_MASK						0x0C
#define SY6541_VBUS_ERRORHI_RF_SHIFT					2
#define SY6541_VBUS_ERRORHI_RF_0P1_VOUT					0
#define SY6541_VBUS_ERRORHI_RF_0P15_VOUT				1
#define SY6541_VBUS_ERRORHI_RF_0P2_VOUT					2
#define SY6541_VBUS_ERRORHI_RF_0P25_VOUT				3
#define SY6541_VBUS_ERRORLO_RF_MASK						0x03
#define SY6541_VBUS_ERRORLO_RF_SHIFT					2
#define SY6541_VBUS_ERRORLO_RF_0_VOUT					0
#define SY6541_VBUS_ERRORLO_RF_0P01_VOUT				1
#define SY6541_VBUS_ERRORLO_RF_0P02_VOUT				2
#define SY6541_VBUS_ERRORLO_RF_0P03_VOUT				3
/* Register 03h */
#define SY6541_REG_03									0x03
#define SY6541_OVPGATE_ON_DG_MASK						0x80
#define SY6541_OVPGATE_ON_DG_SHIFT						7
#define SY6541_OVPGATE_ON_DG_40MS						0
#define SY6541_OVPGATE_ON_DG_10MS						1
#define SY6541_USB_OVP_MASK_MASK						0x40
#define SY6541_USB_OVP_MASK_SHIFT						6
#define SY6541_USB_OVP_NOT_MASK							0
#define SY6541_USB_OVP_IS_MASK							1
#define SY6541_USB_OVP_FLAG_MASK						0x20
#define SY6541_USB_OVP_FLAG_SHIFT						5
#define SY6541_USB_OVP_STAT_MASK						0x10
#define SY6541_USB_OVP_STAT_SHIFT						4
#define SY6541_USB_OVP_MASK								0x0F
#define SY6541_USB_OVP_SHIFT							0
#define SY6541_USB_OVP_BASE								11000
#define SY6541_USB_OVP_LSB								1000
#define SY6541_USB_OVP_7PV5								0x0F
/* Register 04h */
#define SY6541_REG_04									0x04
#define SY6541_WPCGATE_ON_DG_MASK						0x80
#define SY6541_WPCGATE_ON_DG_SHIFT						7
#define SY6541_WPCGATE_ON_DG_10MS						0
#define SY6541_WPCGATE_ON_DG_40MS						1
#define SY6541_WPC_OVP_MASK_MASK						0x40
#define SY6541_WPC_OVP_MASK_SHIFT						6
#define SY6541_WPC_OVP_NOT_MASK							0
#define SY6541_WPC_OVP_IS_MASK							1
#define SY6541_WPC_OVP_FLAG_MASK						0x20
#define SY6541_WPC_OVP_FLAG_SHIFT						5
#define SY6541_WPC_OVP_STAT_MASK						0x10
#define SY6541_WPC_OVP_STAT_SHIFT						4
#define SY6541_WPC_OVP_MASK								0x0F
#define SY6541_WPC_OVP_SHIFT							0
#define SY6541_WPC_OVP_BASE								11000
#define SY6541_WPC_OVP_LSB								1000
#define SY6541_WPC_OVP_7PV5								0x0F
/* Register 05h */
#define SY6541_REG_05									0x05
#define SY6541_BUS_OVP_MASK								0xFC
#define SY6541_BUS_OVP_SHIFT							2
#define SY6541_BUS_OVP_41MODE_BASE						15000
#define SY6541_BUS_OVP_41MODE_LSB						800
#define SY6541_BUS_OVP_21MODE_BASE						7500
#define SY6541_BUS_OVP_21MODE_LSB						400
#define SY6541_BUS_OVP_11MODE_BASE						3750
#define SY6541_BUS_OVP_11MODE_LSB						200
#define SY6541_OUT_OVP_MASK								0x03
#define SY6541_OUT_OVP_SHIFT							0
#define SY6541_OUT_OVP_BASE								4800
#define SY6541_OUT_OVP_LSB								200
/* Register 06h */
#define SY6541_REG_06									0x06
#define SY6541_BUS_OCP_DIS_MASK							0x80
#define SY6541_BUS_OCP_DIS_SHIFT						7
#define SY6541_BUS_OCP_ENABLE							0
#define SY6541_BUS_OCP_DISABLE							1
#define SY6541_BUS_OCP_MASK_MASK						0x40
#define SY6541_BUS_OCP_MASK_SHIFT						6
#define SY6541_BUS_OCP_NOT_MASK							0
#define SY6541_BUS_OCP_IS_MASK							1
#define SY6541_BUS_OCP_FLAG_MASK						0x20
#define SY6541_BUS_OCP_FLAG_SHIFT						5
#define SY6541_BUS_OCP_MASK								0x1F
#define SY6541_BUS_OCP_SHIFT							0
#define SY6541_BUS_OCP_BASE								2100
#define SY6541_BUS_OCP_LSB								150
/* Register 07h */
#define SY6541_REG_07									0x07
#define SY6541_BUS_UCP_DIS_MASK							0x80
#define SY6541_BUS_UCP_DIS_SHIFT						7
#define SY6541_BUS_UCP_ENABLE							0
#define SY6541_BUS_UCP_DISABLE							1
#define SY6541_CBST_SHORT_OPEN_DIS_MASK					0x40
#define SY6541_CBST_SHORT_OPEN_DIS_SHIFT				6
#define SY6541_CBST_SHORT_OPEN_ENABLE					0
#define SY6541_CBST_SHORT_OPEN_DISABLE					1
#define SY6541_BUS_UCP_FALL_DG_MASK						0x30
#define SY6541_BUS_UCP_FALL_DG_SHIFT					4
#define SY6541_BUS_UCP_FALL_DG_8US						0
#define SY6541_BUS_UCP_FALL_DG_4MS						1
#define SY6541_BUS_UCP_FALL_DG_32MS						2
#define SY6541_BUS_UCP_FALL_DG_512MS					3
#define SY6541_BUS_UCP_RISE_MASK_MASK					0x08
#define SY6541_BUS_UCP_RISE_MASK_SHIFT					3
#define SY6541_BUS_UCP_RISE_NOT_MASK					0
#define SY6541_BUS_UCP_RISE_IS_MASK						1
#define SY6541_BUS_UCP_RISE_FLAG_MASK					0x04
#define SY6541_BUS_UCP_RISE_FLAG_SHIFT					2
#define SY6541_BUS_UCP_FALL_MASK_MASK					0x02
#define SY6541_BUS_UCP_FALL_MASK_SHIFT					1
#define SY6541_BUS_UCP_FALL_NOT_MASK					0
#define SY6541_BUS_UCP_FALL_IS_MASK						1
#define SY6541_BUS_UCP_FALL_FLAG_MASK					0x01
#define SY6541_BUS_UCP_FALL_FLAG_SHIFT					0
/* Register 08h */
#define SY6541_REG_08									0x08
#define SY6541_BUS_OCP_PEAK_DIS_MASK					0x80
#define SY6541_BUS_OCP_PEAK_DIS_SHIFT					7
#define SY6541_BUS_OCP_PEAK_ENABLE						0
#define SY6541_BUS_OCP_PEAK_DISABLE						1
#define SY6541_CFLY_SHORT_OPEN_DIS_MASK					0x40
#define SY6541_CFLY_SHORT_OPEN_DIS_SHIFT				6
#define SY6541_CFLY_SHORT_OPEN_ENABLE					0
#define SY6541_CFLY_SHORT_OPEN_DISABLE					1
#define SY6541_BUS_RCP_PEAK_DIS_MASK					0x20
#define SY6541_BUS_RCP_PEAK_DIS_SHIFT					5
#define SY6541_BUS_RCP_PEAK_ENABLE						0
#define SY6541_BUS_RCP_PEAK_DISABLE						1
#define SY6541_BUS_OCP_PEAK_MASK_MASK					0x10
#define SY6541_BUS_OCP_PEAK_MASK_SHIFT					4
#define SY6541_BUS_OCP_PEAK_NOT_MASK					0
#define SY6541_BUS_OCP_PEAK_IS_MASK						1
#define SY6541_BUS_OCP_PEAK_FLAG_MASK					0x08
#define SY6541_BUS_OCP_PEAK_FLAG_SHIFT					3
#define SY6541_VBUS_ERROR_HI_DEG_MASK					0x04
#define SY6541_VBUS_ERROR_HI_DEG_SHIFT					2
#define SY6541_VBUS_ERROR_HI_4US						0
#define SY6541_VBUS_ERROR_HI_120US						1
#define SY6541_VBUS_ERROR_LO_DEG_MASK					0x02
#define SY6541_VBUS_ERROR_LO_DEG_SHIFT					1
#define SY6541_VBUS_ERROR_LO_10US						0
#define SY6541_VBUS_ERROR_LO_1MS						1
#define SY6541_BUS_OCP_PEAK_MASK						0x01
#define SY6541_BUS_OCP_PEAK_SHIFT						0
#define SY6541_BUS_OCP_PEAK_41MODE_10A					0
#define SY6541_BUS_OCP_PEAK_41MODE_12A					1
#define SY6541_BUS_OCP_PEAK_21MODE_10A					0
#define SY6541_BUS_OCP_PEAK_21MODE_12A					1
#define SY6541_BUS_OCP_PEAK_11MODE_8A					0
#define SY6541_BUS_OCP_PEAK_11MODE_10A					1
/* Register 09h */
#define SY6541_REG_09									0x09
#define SY6541_CVT_OCP_PEAK_DIS_MASK					0x80
#define SY6541_CVT_OCP_PEAK_DIS_SHIFT					7
#define SY6541_CVT_OCP_PEAK_ENABLE						0
#define SY6541_CVT_OCP_PEAK_DISABLE						1
#define SY6541_CVT_OCP_PEAK_MASK_MASK					0x20
#define SY6541_CVT_OCP_PEAK_MASK_SHIFT					5
#define SY6541_CVT_OCP_PEAK_NOT_MASK					0
#define SY6541_CVT_OCP_PEAK_IS_MASK						1
#define SY6541_CVT_OCP_PEAK_FLAG_MASK					0x10
#define SY6541_CVT_OCP_PEAK_FLAG_SHIFT					4
/* Register 0Ah */
#define SY6541_REG_0A									0x0A
#define SY6541_POR_FLAG_MASK							0x80
#define SY6541_POR_FLAG_SHIFT							7
#define SY6541_ACRB_WPC_STAT_MASK						0x40
#define SY6541_ACRB_WPC_STAT_SHIFT						6
#define SY6541_WPCGATE_TIED_TO_GND						0
#define SY6541_WPCGATE_NOT_TIED_TO_GND					1
#define SY6541_ACRB_USB_STAT_MASK						0x20
#define SY6541_ACRB_USB_STAT_SHIFT						5
#define SY6541_OVPGATE_TIED_TO_GND						0
#define SY6541_OVPGATE_NOT_TIED_TO_GND					1
#define SY6541_VBUS_ERRORLO_FLAG_MASK					0x10
#define SY6541_VBUS_ERRORLO_FLAG_SHIFT					4
#define SY6541_VBUS_ERRORHI_FLAG_MASK					0x08
#define SY6541_VBUS_ERRORHI_FLAG_SHIFT					3
#define SY6541_QB_ON_STAT_MASK							0x04
#define SY6541_QB_ON_STAT_SHIFT							2
#define SY6541_QB_OFF									0
#define SY6541_QB_ON									1
#define SY6541_CP_SWITCHING_STAT_MASK					0x02
#define SY6541_CP_SWITCHING_STAT_SHIFT					1
#define SY6541_CP_NOT_SWITCHING							0
#define SY6541_CP_SWITCHING								1
#define SY6541_CFLY_SHORT_OPEN_FLAG_MASK				0x01
#define SY6541_CFLY_SHORT_OPEN_FLAG_SHIFT				0
/* Register 0Bh */
#define SY6541_REG_0B									0x0B
#define SY6541_CHG_EN_MASK								0x80
#define SY6541_CHG_EN_SHIFT								7
#define SY6541_CHG_ENABLE								1
#define SY6541_CHG_DISABLE								0
#define SY6541_OTG_EN_MASK								0x40
#define SY6541_OTG_EN_SHIFT								6
#define SY6541_OTG_ENABLE								1
#define SY6541_OTG_DISABLE								0
#define SY6541_ACDRV_MANUAL_EN_MASK						0x20
#define SY6541_ACDRV_MANUAL_EN_SHIFT					5
#define SY6541_ACDRV_AUTO_MODE							0
#define SY6541_ACDRV_MANUAL_MODE						1
#define SY6541_WPCGATE_EN_MASK							0x10
#define SY6541_WPCGATE_EN_SHIFT							4
#define SY6541_WPCGATE_ENABLE							1
#define SY6541_WPCGATE_DISABLE							0
#define SY6541_OVPGATE_EN_MASK							0x08
#define SY6541_OVPGATE_EN_SHIFT							3
#define SY6541_OVPGATE_ENABLE							1
#define SY6541_OVPGATE_DISABLE							0
#define SY6541_VBUS_PD_EN_MASK							0x04
#define SY6541_VBUS_PD_EN_SHIFT							2
#define SY6541_VBUS_PD_ENABLE							1
#define SY6541_VBUS_PD_DISABLE							0
#define SY6541_VWPC_PD_EN_MASK							0x02
#define SY6541_VWPC_PD_EN_SHIFT							1
#define SY6541_VWPC_PD_ENABLE							1
#define SY6541_VWPC_PD_DISABLE							0
#define SY6541_VUSB_PD_EN_MASK							0x01
#define SY6541_VUSB_PD_EN_SHIFT							0
#define SY6541_VUSB_PD_ENABLE							1
#define SY6541_VUSB_PD_DISABLE							0
/* Register 0Ch */
#define SY6541_REG_0C									0x0C
#define SY6541_FSW_SET_MASK								0x78
#define SY6541_FSW_MIN									300
#define SY6541_FSW_MAX									1050
#define SY6541_FSW_STEP									50
#define SY6541_FSW_SET_SHIFT							3
#define SY6541_FSW_SET_300K								0
#define SY6541_FSW_SET_350K								1
#define SY6541_FSW_SET_400K								2
#define SY6541_FSW_SET_450K								3
#define SY6541_FSW_SET_500K								4
#define SY6541_FSW_SET_550K								5
#define SY6541_FSW_SET_600K								6
#define SY6541_FSW_SET_650K								7
#define SY6541_FSW_SET_700K								8
#define SY6541_FSW_SET_750K								9
#define SY6541_FSW_SET_800K								10
#define SY6541_FSW_SET_850K								11
#define SY6541_FSW_SET_900K								12
#define SY6541_FSW_SET_950K								13
#define SY6541_FSW_SET_1000K							14
#define SY6541_FSW_SET_1050K							15
#define SY6541_FREQ_SHIFT_MASK							0x04
#define SY6541_FREQ_SHIFT_SHIFT							2
#define SY6541_FSW_NORMANL								0
#define SY6541_FSW_SPREAD								1
#define SY6541_CHG_FSHIFT_MASK							0x03
#define SY6541_CHG_FSHIFT_SHIFT							0
#define SY6541_CHG_FSHIFT_NOMINAL						0
#define SY6541_CHG_FSHIFT_ADD_10						1
#define SY6541_CHG_FSHIFT_MINUS_10						2
#define SY6541_CHG_FSHIFT_NOMINAL_1						3
/* Register 0Dh */
#define SY6541_REG_0D									0x0D
#define SY6541_VBUS_ERRORHI_DIS_MASK					0x80
#define SY6541_VBUS_ERRORHI_DIS_SHIFT					7
#define SY6541_VBUS_ERRORHI_DISABLE						1
#define SY6541_VBUS_ERRORHI_ENABLE						0
#define SY6541_VBUS_ERRORLO_DIS_MASK					0x40
#define SY6541_VBUS_ERRORLO_DIS_SHIFT					6
#define SY6541_VBUS_ERRORLO_DISABLE						1
#define SY6541_VBUS_ERRORLO_ENABLE						0
#define SY6541_IBUS_UCP_TIMEOUT_SET_MASK				0x38
#define SY6541_IBUS_UCP_TIMEOUT_SET_SHIFT				3
#define SY6541_IBUS_UCP_TIMEOUT_DISABLE					0
#define SY6541_IBUS_UCP_TIMEOUT_320MS					1
#define SY6541_IBUS_UCP_TIMEOUT_320MS_1					2
#define SY6541_IBUS_UCP_TIMEOUT_320MS_2					3
#define SY6541_IBUS_UCP_TIMEOUT_1280MS					4
#define SY6541_IBUS_UCP_TIMEOUT_5120MS					5
#define SY6541_IBUS_UCP_TIMEOUT_20480MS					6
#define SY6541_IBUS_UCP_TIMEOUT_81920MS					7
#define SY6541_WD_TIMEOUT_SET_MASK						0x07
#define SY6541_WD_TIMEOUT_SET_SHIFT						0
#define SY6541_WD_TIMEOUT_DISABLE						0
#define SY6541_WD_TIMEOUT_0P2S							1
#define SY6541_WD_TIMEOUT_0P5S							2
#define SY6541_WD_TIMEOUT_1S							3
#define SY6541_WD_TIMEOUT_5S							4
#define SY6541_WD_TIMEOUT_30S							5
/* Register 0Eh */
#define SY6541_REG_0E									0x0E
#define SY6541_VBAT_OVP_DG_MASK							0x20
#define SY6541_VBAT_OVP_DG_SHIFT						5
#define SY6541_VBAT_OVP_NO_DG							0
#define SY6541_VBAT_OVP_DG_10US							1
#define SY6541_REG_RST_MASK								0x08
#define SY6541_REG_RST_SHIFT							3
#define SY6541_REG_NO_RESET								0
#define SY6541_REG_RESET								1
#define SY6541_MODE_MASK								0x07
#define SY6541_MODE_SHIFT								0
#define SY6541_FORWARD_4_1_CHARGER_MODE					0
#define SY6541_FORWARD_2_1_CHARGER_MODE					1
#define SY6541_FORWARD_1_1_CHARGER_MODE					2
#define SY6541_FORWARD_1_1_CHARGER_MODE1				3
#define SY6541_REVERSE_1_4_CONVERTER_MODE				4
#define SY6541_REVERSE_1_2_CONVERTER_MODE				5
#define SY6541_REVERSE_1_1_CONVERTER_MODE				6
#define SY6541_REVERSE_1_1_CONVERTER_MODE1				7
/* Register 0Fh */
#define SY6541_REG_0F									0x0F
#define SY6541_OVPGATE_STAT_MASK						0x80
#define SY6541_OVPFATE_STAT_SHIFT						7
#define SY6541_OVPFATE_OFF								0
#define SY6541_OVPFATE_ON								1
#define SY6541_WPCGATE_STAT_MASK						0x40
#define SY6541_WPCFATE_STAT_SHIFT						6
#define SY6541_WPCFATE_OFF								0
#define SY6541_WPCFATE_ON								1
#define SY6541_TDIE_OTP_DIS_MASK						0x10
#define SY6541_TDIE_OTP_DIS_SHIFT						4
#define SY6541_TDIE_OTP_ENABLE							0
#define SY6541_TDIE_OTP_DISABLE							1
#define SY6541_VWPC_OVP_DIS_MASK						0x08
#define SY6541_VWPC_OVP_DIS_SHIFT						3
#define SY6541_VWPC_OVP_ENABLE							0
#define SY6541_VWPC_OVP_DISABLE							1
#define SY6541_VUSB_OVP_DIS_MASK						0x04
#define SY6541_VUSB_OVP_DIS_SHIFT						2
#define SY6541_VUSB_OVP_ENABLE							0
#define SY6541_VUSB_OVP_DISABLE							1
#define SY6541_VBUS_OVP_DIS_MASK						0x02
#define SY6541_VBUS_OVP_DIS_SHIFT						1
#define SY6541_VBUS_OVP_ENABLE							0
#define SY6541_VBUS_OVP_DISABLE							1
#define SY6541_VOUT_OVP_DIS_MASK						0x01
#define SY6541_VOUT_OVP_DIS_SHIFT						0
#define SY6541_VOUT_OVP_ENABLE							0
#define SY6541_VOUT_OVP_DISABLE							1
/* Register 10h */
#define SY6541_REG_10									0x10
#define SY6541_VBUS_OK_CHG_STAT_MASK					0x80
#define SY6541_VBUS_OK_CHG_STAT_SHIFT					7
#define SY6541_VOUT_OK_SW_REGN_STAT_MASK				0x40
#define SY6541_VOUT_OK_SW_REGN_STAT_SHIFT				6
#define SY6541_VOUT_OK_REV_STAT_MASK					0x20
#define SY6541_VOUT_OK_REV_STAT_SHIFT					5
#define SY6541_VOUT_OK_CHG_STAT_MASK					0x10
#define SY6541_VOUT_OK_CHG_STAT_SHIFT					4
#define SY6541_VOUT_INSERT_STAT_MASK					0x08
#define SY6541_VOUT_INSERT_STAT_SHIFT					3
#define SY6541_VBUS_PRESENT_STAT_MASK					0x04
#define SY6541_VBUS_PRESENT_STAT_SHIFT					2
#define SY6541_VWPC_INSERT_STAT_MASK					0x02
#define SY6541_VWPC_INSERT_STAT_SHIFT					1
#define SY6541_VUSB_INSERT_STAT_MASK					0x01
#define SY6541_VUSB_INSERT_STAT_SHIFT					0
/* Register 11h */
#define SY6541_REG_11									0x11
#define SY6541_VBUS_OK_CHG_FLAG_MASK					0x80
#define SY6541_VBUS_OK_CHG_FLAG_SHIFT					7
#define SY6541_VOUT_OK_SW_REGN_FLAG_MASK				0x40
#define SY6541_VOUT_OK_SW_REGN_FLAG_SHIFT				6
#define SY6541_VOUT_OK_REV_FLAG_MASK					0x20
#define SY6541_VOUT_OK_REV_FLAG_SHIFT					5
#define SY6541_VOUT_OK_CHG_FLAG_MASK					0x10
#define SY6541_VOUT_OK_CHG_FLAG_SHIFT					4
#define SY6541_VOUT_INSERT_FLAG_MASK					0x08
#define SY6541_VOUT_INSERT_FLAG_SHIFT					3
#define SY6541_VBUS_PRESENT_FLAG_MASK					0x04
#define SY6541_VBUS_PRESENT_FLAG_SHIFT					2
#define SY6541_VWPC_INSERT_FLAG_MASK					0x02
#define SY6541_VWPC_INSERT_FLAG_SHIFT					1
#define SY6541_VUSB_INSERT_FLAG_MASK					0x01
#define SY6541_VUSB_INSERT_FLAG_SHIFT					0
/* Register 12h */
#define SY6541_REG_12									0x12
#define SY6541_VBUS_OK_CHG_MASK_MASK					0x80
#define SY6541_VBUS_OK_CHG_MASK_SHIFT					7
#define SY6541_VBUS_OK_CHG_NOT_MASK						0
#define SY6541_VBUS_OK_CHG_IS_MASK						1
#define SY6541_VOUT_OK_SW_REGN_MASK_MASK				0x40
#define SY6541_VOUT_OK_SW_REGN_MASK_SHIFT				6
#define SY6541_VOUT_OK_SW_REGN_NOT_MASK					0
#define SY6541_VOUT_OK_SW_REGN_IS_MASK					1
#define SY6541_VOUT_OK_REV_MASK_MASK					0x20
#define SY6541_VOUT_OK_REV_MASK_SHIFT					5
#define SY6541_VOUT_OK_REV_NOT_MASK						0
#define SY6541_VOUT_OK_REV_IS_MASK						1
#define SY6541_VOUT_OK_CHG_MASK_MASK					0x10
#define SY6541_VOUT_OK_CHG_MASK_SHIFT					4
#define SY6541_VOUT_OK_CHG_NOT_MASK						0
#define SY6541_VOUT_OK_CHG_IS_MASK						1
#define SY6541_VOUT_INSERT_MASK_MASK					0x08
#define SY6541_VOUT_INSERT_MASK_SHIFT					3
#define SY6541_VOUT_INSERT_NOT_MASK						0
#define SY6541_VOUT_INSERT_IS_MASK						1
#define SY6541_VBUS_PRESENT_MASK_MASK					0x04
#define SY6541_VBUS_PRESENT_MASK_SHIFT					2
#define SY6541_VBUS_PRESENT_NOT_MASK					0
#define SY6541_VBUS_PRESENT_IS_MASK						1
#define SY6541_VWPC_INSERT_MASK_MASK					0x02
#define SY6541_VWPC_INSERT_MASK_SHIFT					1
#define SY6541_VWPC_INSERT_NOT_MASK						0
#define SY6541_VWPC_INSERT_IS_MASK						1
#define SY6541_VUSB_INSERT_MASK_MASK					0x01
#define SY6541_VUSB_INSERT_MASK_SHIFT					0
#define SY6541_VUSB_INSERT_NOT_MASK						0
#define SY6541_VUSB_INSERT_IS_MASK						1
/* Register 13h */
#define SY6541_REG_13									0x13
#define SY6541_IBUS_RCP_PEAK_FLAG_MASK					0x80
#define SY6541_IBUS_RCP_PEAK_FLAG_SHIFT					7
#define SY6541_TDIE_OTP_FLAG_MASK						0x40
#define SY6541_TDIE_OTP_FLAG_SHIFT						6
#define SY6541_IBUS_UCP_TIMEOUT_FLAG_MASK				0x20
#define SY6541_IBUS_UCP_TIMEOUT_FLAG_SHIFT				5
#define SY6541_WD_TIMEOUT_FLAG_MASK						0x10
#define SY6541_WD_TIMEOUT_FLAG_SHIFT					4
#define SY6541_SS_FAIL_FLAG_MASK						0x04
#define SY6541_SS_FAIL_FLAG_SHIFT						2
#define SY6541_VBUS_OVP_FLAG_MASK						0x02
#define SY6541_VBUS_OVP_FLAG_SHIFT						1
#define SY6541_VOUT_OVP_FLAG_MASK						0x01
#define SY6541_VOUT_OVP_FLAG_SHIFT						0
/* Register 14h */
#define SY6541_REG_14									0x14
#define SY6541_TDIE_OTP_MASK_MASK						0x40
#define SY6541_TDIE_OTP_MASK_SHIFT						6
#define SY6541_TDIE_OTP_NOT_MASK						0
#define SY6541_TDIE_OTP_IS_MASK							1
#define SY6541_IBUS_UCP_TIMEOUT_MASK_MASK				0x20
#define SY6541_IBUS_UCP_TIMEOUT_MASK_SHIFT				5
#define SY6541_IBUS_UCP_TIMEOUT_NOT_MASK				0
#define SY6541_IBUS_UCP_TIMEOUT_IS_MASK					1
#define SY6541_WD_TIMEOUT_MASK_MASK						0x10
#define SY6541_WD_TIMEOUT_MASK_SHIFT					4
#define SY6541_WD_TIMEOUT_NOT_MASK						0
#define SY6541_WD_TIMEOUT_IS_MASK						1
#define SY6541_SS_FAIL_MASK_MASK						0x04
#define SY6541_SS_FAIL_MASK_SHIFT						2
#define SY6541_SS_FAIL_NOT_MASK							0
#define SY6541_SS_FAIL_IS_MASK							1
#define SY6541_VBUS_OVP_MASK_MASK						0x02
#define SY6541_VBUS_OVP_MASK_SHIFT						1
#define SY6541_VBUS_OVP_NOT_MASK						0
#define SY6541_VBUS_OVP_IS_MASK							1
#define SY6541_VOUT_OVP_MASK_MASK						0x01
#define SY6541_VOUT_OVP_MASK_SHIFT						0
#define SY6541_VOUT_OVP_NOT_MASK						0
#define SY6541_VOUT_OVP_IS_MASK							1
/* Register 15h */
#define SY6541_REG_15									0x15
#define SY6541_ADC_EN_MASK								0x80
#define SY6541_ADC_EN_SHIFT								7
#define SY6541_ADC_DISABLE								0
#define SY6541_ADC_ENABLE								1
#define SY6541_ADC_RATE_MASK							0x40
#define SY6541_ADC_RATE_SHIFT							6
#define SY6541_ADC_RATE_CONTINOUS						0
#define SY6541_ADC_RATE_ONESHOT							1
#define SY6541_ADC_DONE_STAT_MASK						0x20
#define SY6541_ADC_DONE_STAT_SHIFT						5
#define SY6541_ADC_DONE_FLAG_MASK						0x10
#define SY6541_ADC_DONE_FALG_SHIFT						4
#define SY6541_ADC_DONE_MASK_MASK						0x08
#define SY6541_ADC_DONE_MASK_SHIFT						3
#define SY6541_ADC_DONE_NOT_MASK						0
#define SY6541_ADC_DONE_IS_MASK							1
#define SY6541_IBUS_ADC_DIS_MASK						0x01
#define SY6541_IBUS_ADC_DIS_SHIFT						0
#define SY6541_IBUS_ADC_ENABLE							0
#define SY6541_IBUS_ADC_DISABLE							1
/* Register 16h */
#define SY6541_REG_16									0x16
#define SY6541_VBUS_ADC_DIS_MASK						0x80
#define SY6541_VBUS_ADC_DIS_SHIFT						7
#define SY6541_VBUS_ADC_ENABLE							0
#define SY6541_VBUS_ADC_DISABLE							1
#define SY6541_VUSB_ADC_DIS_MASK						0x40
#define SY6541_VUSB_ADC_DIS_SHIFT						6
#define SY6541_VUSB_ADC_ENABLE							0
#define SY6541_VUSB_ADC_DISABLE							1
#define SY6541_VWPC_ADC_DIS_MASK						0x20
#define SY6541_VWPC_ADC_DIS_SHIFT						5
#define SY6541_VWPC_ADC_ENABLE							0
#define SY6541_VWPC_ADC_DISABLE							1
#define SY6541_VOUT_ADC_DIS_MASK						0x10
#define SY6541_VOUT_ADC_DIS_SHIFT						4
#define SY6541_VOUT_ADC_ENABLE							0
#define SY6541_VOUT_ADC_DISABLE							1
#define SY6541_VBAT_ADC_DIS_MASK						0x08
#define SY6541_VBAT_ADC_DIS_SHIFT						3
#define SY6541_VBAT_ADC_ENABLE							0
#define SY6541_VBAT_ADC_DISABLE							1
#define SY6541_TDIE_ADC_DIS_MASK						0x04
#define SY6541_TDIE_ADC_DIS_SHIFT						2
#define SY6541_TDIE_ADC_ENABLE							0
#define SY6541_TDIE_ADC_DISABLE							1
#define SY6541_ADC_OSR_MASK								0x03
#define SY6541_ADC_OSR_SHIFT							0
#define SY6541_ADC_OSR_512US							0
#define SY6541_ADC_OSR_1024US							1
#define SY6541_ADC_OSR_2048US							2
#define SY6541_ADC_OSR_4096US							3
/* Register 17h */
#define SY6541_REG_17									0x17
#define SY6541_IBUS_SIGN_BIT_MASK						0x80
#define SY6541_IBUS_SIGN_BIT_SHIFT						7
#define SY6541_IBUS_POS									0
#define SY6541_IBUS_NEG									1
#define SY6541_IBUS_POL_H_MASK							0x3F
#define SY6541_IBUS_ADC_LSB								732/1000
/* Register 18h */
#define SY6541_REG_18									0x18
#define SY6541_IBUS_POL_L_MASK							0xFF
/* Register 19h */
#define SY6541_REG_19									0x19
#define SY6541_VBUS_POL_H_MASK							0x7F
#define SY6541_VBUS_ADC_LSB								11718/10000
/* Register 1Ah */
#define SY6541_REG_1A									0x1A
#define SY6541_VBUS_POL_L_MASK							0xFF
/* Register 1Bh */
#define SY6541_REG_1B									0x1B
#define SY6541_VUSB_POL_H_MASK							0x7F
#define SY6541_VUSB_ADC_LSB								11718/10000
/* Register 1Ch */
#define SY6541_REG_1C									0x1C
#define SY6541_VUSB_POL_L_MASK							0xFF
/* Register 1Dh */
#define SY6541_REG_1D									0x1D
#define SY6541_VWPC_POL_H_MASK							0x7F
#define SY6541_VWPC_ADC_LSB								11718/10000
/* Register 1Eh */
#define SY6541_REG_1E									0x1E
#define SY6541_VWPC_POL_L_MASK							0xFF
/* Register 1Fh */
#define SY6541_REG_1F									0x1F
#define SY6541_VOUT_POL_H_MASK							0x7F
#define SY6541_VOUT_ADC_LSB								22/100
/* Register 20h */
#define SY6541_REG_20									0x20
#define SY6541_VOUT_POL_L_MASK							0xFF
/* Register 21h */
#define SY6541_REG_21									0x21
#define SY6541_VBAT_POL_H_MASK							0x7F
#define SY6541_VBAT_ADC_LSB								22/100
/* Register 22h */
#define SY6541_REG_22									0x22
#define SY6541_VBAT_POL_L_MASK							0xFF
/* Register 23h */
#define SY6541_REG_23									0x23
#define SY6541_TDIE_POL_H_MASK							0x01
#define SY6541_TDIE_ADC_LSB								495/1000
/* Register 24h */
#define SY6541_REG_24									0x24
#define SY6541_TDIE_POL_L_MASK							0xFF
/* Register 25h */
#define SY6541_REG_25									0x25
#define SY6541_IBUS_RCP_DIS_MASK						0x80
#define SY6541_IBUS_RCP_DIS_SHIFT						7
#define SY6541_IBUS_RCP_ENABLE							0
#define SY6541_IBUS_RCP_DISABLE							1
#define SY6541_CBST_SHORT_OPEN_FLAG_MASK				0x40
#define SY6541_CBST_SHORT_OPEN_FLAG_SHIFT				6
#define SY6541_IBUS_RCP_MASK_MASK						0x20
#define SY6541_IBUS_RCP_MASK_SHIFT						5
#define SY6541_IBUS_RCP_NOT_MASK						0
#define SY6541_IBUS_RCP_IS_MASK							1
#define SY6541_VWPC_REMOVE_MASK_MASK					0x10
#define SY6541_VWPC_REMOVE_MASK_SHIFT					4
#define SY6541_VWPC_REMOVE_NOT_MASK						0
#define SY6541_VWPC_REMOVE_IS_MASK						1
#define SY6541_VUSB_REMOVE_MASK_MASK					0x08
#define SY6541_VUSB_REMOVE_MASK_SHIFT					3
#define SY6541_VUSB_REMOVE_NOT_MASK						0
#define SY6541_VUSB_REMOVE_IS_MASK						1
#define SY6541_IBUS_RCP_FLAG_MASK						0x04
#define SY6541_IBUS_RCP_FLAG_SHIFT						2
#define SY6541_VWPC_REMOVE_FLAG_MASK					0x02
#define SY6541_VWPC_REMOVE_FLAG_SHIFT					1
#define SY6541_VUSB_REMOVE_FLAG_MASK					0x01
#define SY6541_VUSB_REMOVE_FLAG_SHIFT					0
/* Register 26h */
#define SY6541_REG_26									0x26
#define SY6541_IBUS_RCP_RNG_MASK						0x30
#define SY6541_IBUS_RCP_RNG_SHIFT						4
#define SY6541_IBUS_RCP_14MODE_0P6A						0
#define SY6541_IBUS_RCP_14MODE_0P6A_1					1
#define SY6541_IBUS_RCP_14MODE_1P5A						2
#define SY6541_IBUS_RCP_14MODE_1P8A						3
#define SY6541_IBUS_RCP_12MODE_1P2A						0
#define SY6541_IBUS_RCP_12MODE_1P2A_1					1
#define SY6541_IBUS_RCP_12MODE_3P0A						2
#define SY6541_IBUS_RCP_12MODE_3P6A						3
#define SY6541_IBUS_RCP_11MODE_0P6A						0
#define SY6541_IBUS_RCP_11MODE_2P4A						1
#define SY6541_IBUS_RCP_11MODE_6P0A						2
#define SY6541_IBUS_RCP_11MODE_7P2A						3
#define SY6541_IBUS_UCP_FALL_BLANKING_SET_MASK			0x0C
#define SY6541_IBUS_UCP_FALL_BLANKING_SET_SHIFT			2
#define SY6541_IBUS_UCP_FALL_BLANKING_100MS				0
#define SY6541_IBUS_UCP_FALL_BLANKING_200MS				1
#define SY6541_IBUS_UCP_FALL_BLANKING_400MS				2
#define SY6541_IBUS_UCP_FALL_BLANKING_800MS				3
#define SY6541_IBUS_UCP_EN_METHOD_SEL_MASK				0x02
#define SY6541_IBUS_UCP_EN_METHOD_SEL_SHIFT				1
#define SY6541_IBUS_UCP_EN_AFTER_BLANK					0
#define SY6541_IBUS_UCP_EN_AFTER_RISE					1
#define SY6541_ACDRV_BYPASS_EN_MASK						0x01
#define SY6541_ACDRV_BYPASS_EN_SHIFT					0
#define SY6541_ACDRV_BYPASS_DISABLE						0
#define SY6541_ACDRV_BYPASS_ENABLE						1
/* Register 27h */
#define SY6541_REG_27									0x27
#define SY6541_CBST_SHORT_OPEN_MASK_MASK				0x20
#define SY6541_CBST_SHORT_OPEN_MASK_SHIFT				5
#define SY6541_CBST_SHORT_OPEN_NOT_MASK					0
#define SY6541_CBST_SHORT_OPEN_IS_MASK					1
#define SY6541_IBUS_RCP_PEAK_MASK_MASK					0x10
#define SY6541_IBUS_RCP_PEAK_MASK_SHIFT					4
#define SY6541_IBUS_RCP_PEAK_NOT_MASK					0
#define SY6541_IBUS_RCP_PEAK_IS_MASK					1
#define SY6541_CFLY_SHORT_OPEN_MASK_MASK				0x08
#define SY6541_CFLY_SHORT_OPEN_MASK_SHIFT				3
#define SY6541_CFLY_SHORT_OPEN_NOT_MASK					0
#define SY6541_CFLY_SHORT_OPEN_IS_MASK					1
#define SY6541_VBUS_ERRORHI_MASK_MASK					0x04
#define SY6541_VBUS_ERRORHI_MASK_SHIFT					2
#define SY6541_VBUS_ERRORHI_NOT_MASK					0
#define SY6541_VBUS_ERRORHI_IS_MASK						1
#define SY6541_VBUS_ERRORLO_MASK_MASK					0x02
#define SY6541_VBUS_ERRORLO_MASK_SHIFT					1
#define SY6541_VBUS_ERRORLO_NOT_MASK					0
#define SY6541_VBUS_ERRORLO_IS_MASK						1
#define SY6541_POR_MASK_MASK							0x01
#define SY6541_POR_MASK_SHIFT							0
#define SY6541_POR_NOT_MASK								0
#define SY6541_POR_IS_MASK								1
/* Register 28h */
#define SY6541_REG_28									0x28
#define SY6541_L_EN_MASK								0x80
#define SY6541_L_EN_SHIFT								7
#define SY6541_L_ENABLE									0
#define SY6541_L_DISABLE								1
#define SY6541_DEAD_TIME_SET_MASK						0x03
#define SY6541_DEAD_TIME_SET_SHIFT						0
#define SY6541_DEAD_TIME_20NS							0
#define SY6541_DEAD_TIME_25NS							1
#define SY6541_DEAD_TIME_35NS							2
#define SY6541_DEAD_TIME_45NS							3
/* Register 6Eh */
#define SY6541_REG_6E									0x6E
/* Register F3h */
#define SY6541_REG_F3									0xF3
#define SY6541_DT_BIT_MASK								0x08
#define SY6541_DT_BIT_SHIFT								3
#define SY6541_DT_BIT_DISABLE							0
#define SY6541_DT_BIT_ENABLE							1
/* Register F4h */
#define SY6541_REG_F4									0xF4
#define SY6541_AUTO_FSW_EN_MASK							0x04
#define SY6541_AUTO_FSW_EN_SHIFT						2
#define SY6541_AUTO_FSW_DISABLE							0
#define SY6541_AUTO_FSW_ENABLE							1
/* Register F6h */
#define SY6541_REG_F6									0xF6
#define SY6541_DT_SET_MASK								0x20
#define SY6541_DT_SET_SHIFT								5
#define SY6541_DT_SET_DISABLE							0
#define SY6541_DT_SET_ENABLE							1

#endif  /* __SY6541_REG_H__ */