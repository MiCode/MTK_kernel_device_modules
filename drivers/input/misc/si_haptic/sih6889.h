/*
 *  Silicon Integrated Co., Ltd haptic sih6889 header file
 *
 *  Copyright (c) 2024 shanfa <shanfa.tang@si-in.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation
 */

#ifndef _SIH6889_H_
#define _SIH6889_H_

#include "haptic.h"

#define SIH6889_CHIPID_REG_VALUE                    0x89
#define SIH6889_CHIPID_REG_ADDR                     0x00
#define SIH6889_BRK_VBOOST_COE						10
#define SIH6889_VBOOST_MUL_COE						4096
#define SIH6889_DETECT_FIFO_ARRAY_MAX               50
#define SIH6889_READ_FIFO_MAX_DATA_LEN				9
#define SIH6889_FIFO_PACK_SIZE						5
#define SIH6889_FIFO_READ_DATA_LEN					6
#define SIH6889_LPF_GAIN_COE						4
#define SIH6889_DETECT_BEMF_COE						4096
#define SIH6889_DETECT_ADCV							16
#define SIH6889_BEMF_MV_COE							1000
#define SIH6889_DETECT_ADCV_COE						10
#define SIH6889_DETECT_F0_COE						786432000
#define SIH6889_DETECT_F0_AMPLI_COE					10
#define SIH6889_TRACKING_F0_COE						1966080000
#define SIH6889_F0_CAL_COE							1000000
#define SIH6889_F0_AMPLI_COE						10
#define SIH6889_F0_DELTA							28800000
#define SIH6889_F0_CALI_DELTA						2880
#define SIH6889_F0_VAL_MAX							1800
#define SIH6889_F0_VAL_MIN							1600
#define SIH6889_RL_AMP_COE							1000000
#define SIH6889_RL_DIV_COE							18375
#define SIH6889_B0_RL_AMP_COE						10000
#define SIH6889_B0_RL_DIV_COE						189
#define SIH6889_RL_SAR_CODE_DIVIDER					84
#define SIH6889_RL_CONFIG_REG_NUM					3
#define SIH6889_ADC_COE								305
#define SIH6889_ADC_AMPLIFY_COE						100
#define SIH6889_RL_MODIFY							4
#define SIH6889_RL_MODIFY_COE						10
#define SIH6889_LPF_GAIN_COE						4
#define SIH6889_OSC_CALI_COE						100000
#define SIH6889_DRV_BOOST_BASE						45
#define SIH6889_DRV_BOOST_SETP_COE					1000
#define SIH6889_DRV_BOOST_SETP						625
#define SIH6889_STANDARD_VBAT						4000
#define SIH6889_VBAT_MIN							3000
#define SIH6889_VBAT_MAX							5500
#define SIH6889_VBAT_AMPLIFY_COE					1000
#define SIH6889_OSC_RTL_DATA_LEN					1000
#define SIH6889_PWM_SAMPLE_48KHZ					48
#define SIH6889_PWM_SAMPLE_24KHZ					24
#define SIH6889_PWM_SAMPLE_12KHZ					12
#define SIH6889_READ_CHIP_ID_MAX_TRY				8
#define SIH6889_RL_DETECT_MAX_TRY					10
#define SIH6889_GET_VBAT_MAX_TRY					10
#define SIH6889_DRV_VBOOST_MIN						6
#define SIH6889_DRV_VBOOST_MAX						11
#define SIH6889_DRV_VBOOST_COEFFICIENT				10
#define SIH6889_RL_OFFSET							1300
#define SIH6889_PLATFORM_HWINFO						0x0c

#define SIH6889_TEST_ON								0x1f
#define SIH6889_TEST_OFF							0xf1
#define SIH6889_DBG_ON								0x2e
#define SIH6889_DBG_OFF								0xe2
#define SIH6889_TEST_ON_DBG_ON						0x5a
#define SIH6889_TEST_OFF_DBG_OFF					0xa5

#define SIH6889_VBAT_2_8V							280
#define SIH6889_VBAT_3_0V							300
#define SIH6889_VBAT_3_2V							320

#define SIH6889_LP_PVDD_6_5V						65
#define SIH6889_LP_PVDD_7_0V						70
#define SIH6889_LP_PVDD_7_5V						75
#define SIH6889_LP_PVDD_9_0V						90

#define SIH6889_LP_IPEAK_A							40

extern haptic_func_t sih_6889_func_list;

typedef enum SIH6889_CONT_PARA {
	SIH6889_CONT_PARA_SEQ0 = 0,
	SIH6889_CONT_PARA_SEQ1 = 1,
	SIH6889_CONT_PARA_SEQ2 = 2,
	SIH6889_CONT_PARA_ASMOOTH = 3,
	SIH6889_CONT_PARA_TH_LEN = 4,
	SIH6889_CONT_PARA_TH_NUM = 5,
	SIH6889_CONT_PARA_AMPLI = 6,
} sih6889_cont_para_e;

typedef enum SIH6889_BST_MODE {
	SIH6889_BST_MODE_PLAYBACK = 0,
	SIH6889_BST_MODE_BRK,
	SIH6889_BST_MODE_TRIG,
} sih6889_bst_mode_e;

#endif /* _SIH6889_H_ */

