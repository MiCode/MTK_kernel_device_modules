 /* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2020 Texas Instruments Incorporated - http://www.ti.com/ */
/**/
#ifndef BQ25985_CHARGER_H
#define BQ25985_CHARGER_H

#define BQ25985_MANUFACTURER "Texas Instruments"
#define READBIT(X,N)             (X&(0x1<<N))
#define USEBITS(X,N,M)           (X&(~(~0X0)<<N)&(X&((~0X0)<<M)))
#define RESETBIT(X,N)            ((~(0x1<<N))&X)
#define BQ25985_BATOVP			0x0
#define BQ25985_BATOVP_ALM		0x1
#define BQ25985_BATOCP			0x2
#define BQ25985_BATOCP_DIS		BIT(7)
#define BQ25985_BATOCP_ALM		0x3
#define BQ25985_BATUCP_ALM		0x4
#define BQ25985_CHRGR_CTRL_1	0x5
#define BQ25985_BUSUCP_DIS		BIT(7)
#define BQ25985_BUSOVP			0x6
#define BQ25985_BUSOVP_ALM		0x7
#define BQ25985_BUSOCP			0x8
#define BQ25985_BUSOCP_ALM		0x9
#define BQ25985_TEMP_CONTROL	0xA
#define BQ25985_TSBAT_FLT_DIS		BIT(2)
#define BQ25985_TDIE_ALM		0xB
#define BQ25985_TSBUS_FLT		0xC
#define BQ25985_VAC_CONTROL		0xD
#define BQ25985_VAC1OVP_MASK		    GENMASK(7, 5)//
#define BQ25985_VAC2OVP_MASK		    GENMASK(4, 2)//
//#define BQ25985_VAC_CONTROL		0xE
#define BQ25985_CHRGR_CTRL_2	0xE
#define BQ25985_CHRGR_CTRL_3	0xF
#define BQ25985_WATCHDOG_MASK	    GENMASK(4, 3)//
#define BQ25985_WATCHDOG_DIS	    BIT(2)//
#define BQ25985_CHRGR_CTRL_4	0x10
#define BQ25985_VBUS_ERRLO_MASK	    BIT(1)
#define BQ25985_VBUS_ERRHI_MASK	    BIT(0)
#define BQ25985_CHRGR_CTRL_5	0x11
#define BQ25985_EN_SC41         	BIT(7)
#define BQ25985_EN_BYPASS		BIT(6)
#define BQ25985_EXT_REVERSE_EN		BIT(5)
#define BQ25985_REVERSE_EN		BIT(4)
#define BQ25985_CHG_EN			BIT(3)
#define BQ25985_ACDRV_EN		BIT(2)
#define BQ25985_CHRGR_CTRL_6    0X12
#define BQ25985_STAT1			0x13
#define BQ25985_STAT2			0x14
#define BQ25985_STAT3			0x15
#define BQ25985_STAT4			0x16
#define BQ25985_STAT5			0x17
#define BQ25985_FLAG1			0x18
#define BQ25985_FLAG2			0x19
#define BQ25985_FLAG3			0x1A
#define BQ25985_FLAG4			0x1B
#define BQ25985_FLAG5			0x1C
#define BQ25985_MASK1			0x1D
#define BQ25985_MASK2			0x1E
#define BQ25985_MASK3			0x1F
#define BQ25985_MASK4			0x20
#define BQ25985_MASK5			0x21
#define BQ25985_DEVICE_INFO_COPY		0x22
#define BQ25985_ADC_CONTROL1		0x23
#define BQ25985_ADC_CONTROL2		0x24
#define BQ25985_IBUS_ADC_MSB		0x27
#define BQ25985_IBUS_ADC_LSB		0x26
#define BQ25985_VBUS_ADC_MSB		0x29
#define BQ25985_VBUS_ADC_LSB		0x28
//--------------------------------------
#define BQ25985_VAC1_ADC_MSB		0x2B
#define BQ25985_VAC1_ADC_LSB		0x2A
//--------------------------------------
#define BQ25985_VAC2_ADC_LSB		0x2D
#define BQ25985_VAC2_ADC_MSB		0x2C
#define BQ25985_VOUT_ADC_LSB		0x2F
#define BQ25985_VOUT_ADC_MSB		0x2E
#define BQ25985_VBAT1_ADC_LSB		0x31
#define BQ25985_VBAT1_ADC_MSB		0x30
#define BQ25985_VBAT2_ADC_LSB		0x33
#define BQ25985_VBAT2_ADC_MSB		0x32
//--------------------------------------
#define BQ25985_IBAT_ADC_LSB		0x35
#define BQ25985_IBAT_ADC_MSB		0x34
#define BQ25985_TSBAT_ADC_LSB		0x37
#define BQ25985_TSBAT_ADC_MSB		0x36
#define BQ25985_TDIE_ADC_LSB		0x39
#define BQ25985_TDIE_ADC_MSB		0x38
//#define BQ25985_DEGLITCH_TIME		0x39
//#define BQ25985_CHRGR_CTRL_6	    0x3A
#define BQ25985_DP_ADC_MSB          0X3A
#define BQ25985_DP_ADC_LSB          0X3B
#define BQ25985_DM_ADC_MSB          0X3C
#define BQ25985_DM_ADC_LSB          0X3D

#define BQ25985_VBUS_ERRLO_ERRHI    0X3E
#define BQ25985_HVDCP1              0X3F
#define BQ25985_HVDCP2              0X40
#define BQ25985_CHICKEN1            0X41
#define BQ25985_CHICKEN2            0X42
#define BQ25985_CHICKEN3            0X43
#define BQ25985_CHICKEN4            0X44
#define BQ25985_DEVICE_INFO         0X6E

//------------------------------------------



#define BQ25985_BUSOCP_SC41_MAX_uA	    5000000//
#define BQ25985_BUSOCP_SC41_MIN_uA      1250000//
#define BQ25985_BUSOCP_SC41_STEP_uA      250000//
#define BQ25985_BUSOCP_SC41_OFFSET_uA   1250000//
#define BQ25985_BUSOCP_SC41_DFLT_uA     3000000//

#define BQ25985_BUSOVP_SC41_STEP_uV	      100000//
#define BQ25985_BUSOVP_SC41_OFFSET_uV	14000000////
#define BQ25985_BUSOVP_SC41_MIN_uV	    14000000//
#define BQ25985_BUSOVP_SC41_MAX_uV	    26700000//
#define BQ25985_BUSOVP_SC41_DFLT_uV	    20300000//

#define BQ25985_BATOCP_MAX_uA	    15000000//
#define BQ25985_BATOCP_MIN_uA        2000000//
#define BQ25985_BATOCP_STEP_uA        500000//
#define BQ25985_BATOCP_OFFSET_uA      500000//
#define BQ25985_BATOCP_DFLT_uA       8500000//

#define BQ25985_BATOVP_STEP_uV	       12500//
#define BQ25985_BATOVP_OFFSET_uV	 4500000////
#define BQ25985_BATOVP_MIN_uV	     4500000//
#define BQ25985_BATOVP_MAX_uV	     5200000//
#define BQ25985_BATOVP_DFLT_uV	     4850000//


#define BQ25985_BUSOCP_SC21_MAX_uA	    9000000//
#define BQ25985_BUSOCP_SC21_MIN_uA	    2500000//
#define BQ25985_BUSOCP_SC21_STEP_uA	     500000//
#define BQ25985_BUSOCP_SC21_OFFSET_uA	2500000//
#define BQ25985_BUSOCP_SC21_DFLT_uA	    5500000//

#define BQ25985_BUSOVP_SC21_STEP_uV	       50000////
#define BQ25985_BUSOVP_SC21_OFFSET_uV	 7000000////
#define BQ25985_BUSOVP_SC21_MIN_uV	     7000000//
#define BQ25985_BUSOVP_SC21_MAX_uV	    13350000//
#define BQ25985_BUSOVP_SC21_DFLT_uV	    10000000//

#define BQ25985_BUSOCP_BYP_MAX_uA	    9000000//
#define BQ25985_BUSOCP_BYP_MIN_uA	    2500000//
#define BQ25985_BUSOCP_BYP_STEP_uA	     500000//
#define BQ25985_BUSOCP_BYP_OFFSET_uA	2500000//
#define BQ25985_BUSOCP_BYP_DFLT_uA	    5500000//

#define BQ25985_BUSOVP_BYP_STEP_uV	      25000//
#define BQ25985_BUSOVP_BYP_OFFSET_uV	3500000//
#define BQ25985_BUSOVP_BYP_MIN_uV	    3500000//
#define BQ25985_BUSOVP_BYP_MAX_uV	    6675000//
#define BQ25985_BUSOVP_BYP_DFLT_uV	    5000000//?


#define CP_FORWARD_4_TO_1             0
#define CP_FORWARD_2_TO_1             1
#define CP_FORWARD_1_TO_1             2
#define REVERSE_1_1_CONVERTER         6



#define BQ25985_BATOCP_MASK		    GENMASK(4, 0)//
#define BQ25985_ENABLE_HIZ		    0xff//
#define BQ25985_DISABLE_HIZ		    0x0//
#define BQ25985_STAT1_OVP_MASK		(BIT(7) |BIT(6) | BIT(5) | BIT(4)| BIT(0))//?
#define BQ25985_STAT3_OVP_MASK		(BIT(7) | BIT(6))//
#define BQ25985_STAT1_OCP_MASK		BIT(3)//
#define BQ25985_STAT2_OCP_MASK		(BIT(6) | BIT(1))//
#define BQ25985_STAT4_TFLT_MASK		(BIT(5) | BIT(3) | BIT(2) | BIT(1))//GENMASK(5, 1),第四位定义变化
#define BQ25985_WD_STAT			    BIT(0)//
#define BQ25985_PRESENT_MASK		GENMASK(4, 2)//
#define BQ25985_EN_HIZ			    BIT(6)//
#define BQ25985_ADC_EN			    BIT(7)//

#define BQ25985_ADC_VOLT_STEP_uV    1000//
#define BQ25985_ADC_CURR_STEP_uA    1000//
#define BQ25985_ADC_POLARITY_BIT	BIT(7)//此处为极性判断

#define BQ25985_WATCHDOG_MAX	    30000//?
#define BQ25985_WATCHDOG_MIN	    2000//?
#define BQ25985_NUM_WD_VAL	        4//

#endif /* BQ25985_CHARGER_H */
