/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __TMP_BTS_H__
#define __TMP_BTS_H__

/* chip dependent */

#define APPLY_PRECISE_NTC_TABLE
#define APPLY_AUXADC_CALI_DATA
#define APPLY_PRECISE_CHARGEIC_TEMP
#define APPLY_PRECISE_USB_TEMP

#define AUX_IN0_NTC (0)
#define AUX_IN1_NTC (1)

/* usb ntc */
#define AUX_IN2_NTC (2)

/* charge ic ntc */
#define AUX_IN4_NTC (4)

#define BTS_RAP_PULL_UP_R		390000 /* 390K, pull up resister */

#define BTS_TAP_OVER_CRITICAL_LOW	4397119 /* base on 100K NTC temp
						 * default value -40 deg
						 */

#define BTS_RAP_PULL_UP_VOLTAGE		1800 /* 1.8V ,pull up voltage */

#define BTS_RAP_NTC_TABLE		7 /* default is NCP15WF104F03RC(100K) */

#define BTS_RAP_ADC_CHANNEL		AUX_IN0_NTC /* default is 0 */

#define BTSMDPA_RAP_PULL_UP_R		390000 /* 390K, pull up resister */

#define BTSMDPA_TAP_OVER_CRITICAL_LOW	4397119 /* base on 100K NTC temp
						 * default value -40 deg
						 */

#define BTSMDPA_RAP_PULL_UP_VOLTAGE	1800 /* 1.8V ,pull up voltage */

#define BTSMDPA_RAP_NTC_TABLE		7 /* default is NCP15WF104F03RC(100K) */

#define BTSMDPA_RAP_ADC_CHANNEL		AUX_IN1_NTC /* default is 1 */

/* usb add ntc node -s */
#define USB_RAP_PULL_UP_R          100000      /* 100K, pull up resister */
#define USB_TAP_OVER_CRITICAL_LOW  4397119     /* base on 100K NTC temp default value -40 deg */
#define USB_RAP_PULL_UP_VOLTAGE    1800        /* 1.8V ,pull up voltage */
#define USB_RAP_NTC_TABLE          7           /* default is NCP15WF104F03RC(100K) */
#define USB_RAP_ADC_CHANNEL        AUX_IN2_NTC /* default is 4 */
/* usb add ntc node -e */

/* charge ic add ntc node -s */
#define CHARGEIC_RAP_PULL_UP_R          390000      /* 390K, pull up resister */
#define CHARGEIC_TAP_OVER_CRITICAL_LOW  4397119     /* base on 100K NTC temp default value -40 deg */
#define CHARGEIC_RAP_PULL_UP_VOLTAGE    1800        /* 1.8V ,pull up voltage */
#define CHARGEIC_RAP_NTC_TABLE          7           /* default is NCP15WF104F03RC(100K) */
#define CHARGEIC_RAP_ADC_CHANNEL        AUX_IN4_NTC /* default is 4 */
/* charge ic add ntc node -e */

extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int *rawdata);
extern int IMM_IsAdcInitReady(void);

#endif	/* __TMP_BTS_H__ */
