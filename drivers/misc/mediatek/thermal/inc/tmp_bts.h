/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __TMP_BTS_H__
#define __TMP_BTS_H__

/* chip dependent */

#define APPLY_PRECISE_NTC_TABLE
#define APPLY_AUXADC_CALI_DATA
/* n85 cpu thermal ntc */
#define APPLY_PRECISE_BTS_TEMP
#define APPLY_PRECISE_LTEPA_TEMP
#define APPLY_PRECISE_CHARGEIC_TEMP
#define APPLY_PRECISE_LCD_TEMP
#define APPLY_PRECISE_CPU_TEMP

#define AUX_IN0_NTC (0)
#define AUX_IN1_NTC (1)
/* n85 cpu thermal ntc */
#define AUX_IN3_NTC (3)
#define AUX_IN4_NTC (4)


/****************************cpu_thermal_ntc************************************************/
#define WTCPU_RAP_PULL_UP_R		        22000 /* 100K, pull up resister */
#define WTCPU_TAP_OVER_CRITICAL_LOW	        4397119 /* base on 100K NTC temp default value -40 deg */
#define WTCPU_RAP_PULL_UP_VOLTAGE	        1800 /* 1.8V ,pull up voltage */
#define WTCPU_RAP_NTC_TABLE		        7 /* default is NCP15WF104F03RC(100K) */
#define WTCPU_RAP_ADC_CHANNEL		        AUX_IN0_NTC /* default is 0 */
/******************************end**********************************************************/

/****************************bts_thermal_ntc************************************************/
#define BTS_RAP_PULL_UP_R		        100000 /* 100K, pull up resister */
#define BTS_TAP_OVER_CRITICAL_LOW	        4397119 /* base on 100K NTC temp
						 * default value -40 deg*/
#define BTS_RAP_PULL_UP_VOLTAGE		        1800 /* 1.8V ,pull up voltage */
#define BTS_RAP_NTC_TABLE		        7 /* default is NCP15WF104F03RC(100K) */
#define BTS_RAP_ADC_CHANNEL		        AUX_IN3_NTC /* default is 0 */
/******************************end**********************************************************/

#define BTSMDPA_RAP_PULL_UP_R		390000 /* 390K, pull up resister */

#define BTSMDPA_TAP_OVER_CRITICAL_LOW	4397119 /* base on 100K NTC temp
						 * default value -40 deg
						 */

#define BTSMDPA_RAP_PULL_UP_VOLTAGE	1800 /* 1.8V ,pull up voltage */

#define BTSMDPA_RAP_NTC_TABLE		7 /* default is NCP15WF104F03RC(100K) */

#define BTSMDPA_RAP_ADC_CHANNEL		AUX_IN1_NTC /* default is 1 */

/****************************pa_thermal_ntc************************************************/
#define LTEPA_RAP_PULL_UP_R			100000	/* 390K, pull up resister */
#define BTLTEPA_TAP_OVER_CRITICAL_LOW		4397119	/* base on 100K NTC temp default value -40 deg */
#define BTLTEPA_RAP_PULL_UP_VOLTAGE		1800	/* 1.8V ,pull up voltage */
#define BTLTEPA_RAP_NTC_TABLE			7		/* default is NCP15WF104F03RC(100K) */
#define BTLTEPA_RAP_ADC_CHANNEL			AUX_IN1_NTC /* default is 1 */
/****************************end************************************************/
 
/****************************charger_thermal_ntc********************************/
#define CHARGEIC_RAP_PULL_UP_R			100000	/* 390K, pull up resister */
#define BTCHARGEIC_TAP_OVER_CRITICAL_LOW	4397119	/* base on 100K NTC temp default value -40 deg */
#define BTCHARGEIC_RAP_PULL_UP_VOLTAGE		1800	/* 1.8V ,pull up voltage */
#define BTCHARGEIC_RAP_NTC_TABLE		7
/****************************end************************************************/
 
/****************************lcd_thermal_ntc********************************/
#define LCD_RAP_PULL_UP_R			100000	/* 390K, pull up resister */
#define BTLCD_TAP_OVER_CRITICAL_LOW	        4397119	/* base on 100K NTC temp default value -40 deg */
#define BTLCD_RAP_PULL_UP_VOLTAGE		1800	/* 1.8V ,pull up voltage */
#define BTLCD_RAP_NTC_TABLE			7		/* default is NCP15WF104F03RC(100K) */
#define BTLCD_RAP_ADC_CHANNEL			AUX_IN4_NTC /* default is 1 */
/****************************end************************************************/


extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int *rawdata);
extern int IMM_IsAdcInitReady(void);

#endif	/* __TMP_BTS_H__ */
