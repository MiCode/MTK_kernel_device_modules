/* SPDX-License-Identifier: GPL-2.0 */
/*
 * sc7601_reg.h
 *
 * Load-switch ic reg
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

#ifndef __SC7601_HEADER__
#define __SC7601_HEADER__

/* Register 00h */
#define SC7601_REG_00                           0x00
/* Register 01h */
#define SC7601_REG_01                           0x01
#define SC760X_IBAT_CHG_LIM_MAST                0xFF
#define SC760X_IBAT_CHG_LIM_SHIFT               0
#define SC760X_IBAT_CHG_LIM_BASE                50
#define SC760X_IBAT_CHG_LIM_LSB                 50

/* Register 02h */
#define SC7601_REG_02                           0x02
#define SC760X_POW_LIM_DIS_MASK                 0x80
#define SC760X_POW_LIM_DIS_SHIFT                7
#define SC760X_POW_LIM_DIS_ENABLE               0
#define SC760X_POW_LIM_DIS_DISABLE              1

#define SC760X_VBALANCE_DIS_MASK                0x20
#define SC760X_VBALANCE_DIS_SHIFT               5
#define SC760X_VBALANCE_DIS_ENABLE              0
#define SC760X_VBALANCE_DIS_DISABLE             1

#define SC760X_POW_LIM_MASK                     0x0F
#define SC760X_POW_LIM_SHIFT                    0
#define SC760X_POW_LIM_BASE                     100
#define SC760X_POW_LIM_LSB                      100

/* Register 03h */
#define SC7601_REG_03                           0x03
#define SC760X_ILIM_DIS_MASK                    0x80
#define SC760X_ILIM_ENABLE                      0
#define SC760X_ILIM_DISABLE                     1

#define SC760X_IBAT_CHG_LIM_DIS_MASK            0x40
#define SC760X_IBAT_CHG_LIM_DIS_ENABLE          0
#define SC760X_IBAT_CHG_LIM_DIS_DISABLE         1

#define SC760X_BAT_DET_DIS_MASK                 0x20
#define SC760X_BAT_DET_ENABLE                   0
#define SC760X_BAT_DET_DISABLE                  1

#define SC760X_VDIFF_CHECK_DIS_MASK             0x10
#define SC760X_VDIFF_CHECK_DIS_ENABLE           0
#define SC760X_VDIFF_CHECK_DIS_DISABLE          1

#define SC760X_LS_OFF_MASK                      0x08
#define SC760X_LS_AUTO                          0
#define SC760X_LS_MOSFET_OFF                    1

#define SC760X_SHIP_EN_MASK                     0x0F
#define SC760X_SHIP_EN_SHIFT                    0
#define SC760X_SHIP_EN_VAL                      7


/* Register 04h */
#define SC7601_REG_04                           0x04
#define SC760X_REG_RST_MASK                     0x80
#define SC760X_REG_RESET_SHIFT                  7
#define SC760X_REG_RESET                        1
#define SC760X_REG_NO_RESET                     0

#define SC760X_EN_LOWPOWER_MASK                 0x40
#define SC760X_REG_LOWPOWER_SHIFT               6
#define SC760X_NO_ACTION                        0
#define SC760X_LOWPOWER_MODE                    1

#define SC760X_VDIFF_OPEN_TH_MASK               0x30
#define SC760X_VDIFF_OPEN_TH_SHIFT              4
#define SC760X_VDIFF_OPEN_TH_100MV              0
#define SC760X_VDIFF_OPEN_TH_300MV              1
#define SC760X_VDIFF_OPEN_TH_400MV              2
#define SC760X_VDIFF_OPEN_TH_800MV              3

#define SC760X_AUTO_BSM_DIS_MASK                0x08
#define SC760X_AUTO_BSM_DIS_ENABLE              0
#define SC760X_AUTO_BSM_DIS_DISABLE             1

#define SC760X_AUTO_BSM_TH_MASK                 0x04
#define SC760X_AUTO_BSM_TH_100MV                0
#define SC760X_AUTO_BSM_TH_50MV                 1

#define SC760X_SHIP_WT_MASK                     0x01
#define SC760X_SHIP_WT_NOALLOW                  0
#define SC760X_SHIP_WT_ALLOW                    1

/* Register 05h */
#define SC7601_REG_05                           0x05
#define SC760X_ITRICHG_MASK                     0xE0
#define SC760X_ITRICHG_SHIFT                    5
#define SC760X_ITRICHG_BASE                     12500
#define SC760X_ITRICHG_LSB                      12500

/* Register 06h */
#define SC7601_REG_06                           0x06
#define SC760X_IPRECHG_MASK                     0xF0
#define SC760X_IPRECHG_SHIFT                    4
#define SC760X_IPRECHG_BASE                     50
#define SC760X_IPRECHG_LSB                      50
#define SC760X_VFCCHG_MASK                      0x0F
#define SC760X_VFCCHG_SHIFT                     0
#define SC760X_VFCCHG_BASE                      2800
#define SC760X_VFCCHG_LSB                       50

/* Register 07h */
#define SC7601_REG_07                           0x07
#define SC760X_CHG_OVP_DIS_MASK                 0x80
#define SC760X_CHG_OVP_DIS_ENABLE               0
#define SC760X_CHG_OVP_DIS_DISABLE              1

#define SC760X_CHG_OVP_MASK                     0x40
#define SC760X_CHG_OVP_5000MV                   0
#define SC760X_CHG_OVP_5600MV                   1

/* Register 08h */
#define SC7601_REG_08                           0x08
#define SC760X_BAT_OVP_MASK                     0x7C
#define SC760X_BAT_OVP_SHIFT                    2
#define SC760X_BAT_OVP_BASE                     4000
#define SC760X_BAT_OVP_LSB                      50

/* Register 09h */
#define SC7601_REG_09                           0x09
#define SC760X_CHG_OCP_DIS_MASK                 0x80
#define SC760X_CHG_OCP_DIS_ENABLE               0
#define SC760X_CHG_OCP_DIS_DISABLE              1

#define SC760X_CHG_OCP_MASK                     0x70
#define SC760X_CHG_OCP_SHIFT                    4
#define SC760X_CHG_OCP_BASE                     10
#define SC760X_CHG_OCP_LSB                      1

/* Register 0Ah */
#define SC7601_REG_0A                           0x0A
#define SC760X_DSG_OCP_DIS_MASK                 0x80
#define SC760X_DSG_OCP_DIS_ENABLE               0
#define SC760X_DSG_OCP_DIS_DISABLE              1

#define SC760X_DSG_OCP_MASK                     0x70
#define SC760X_DSG_OCP_SHIFT                    4
#define SC760X_DSG_OCP_BASE                     10
#define SC760X_DSG_OCP_LSB                      1

/* Register 0Bh */
#define SC7601_REG_0B                           0x0B
#define SC760X_TDIE_FLT_DIS_MASK                0x80
#define SC760X_TDIE_FLT_DIS_ENABLE              0
#define SC760X_TDIE_FLT_DIS_DISABLE             1

#define SC760X_TDIE_FLT_MASK                    0x0F
#define SC760X_TDIE_FLT_SHIFT                   0
#define SC760X_TDIE_FLT_BASE                    80
#define SC760X_TDIE_FLT_LSB                     5

/* Register 0Ch */
#define SC7601_REG_0C                           0x0C
#define SC760X_TDIE_ALM_DIS_MASK                0x80
#define SC760X_TDIE_ALM_DIS_ENABLE              0
#define SC760X_TDIE_ALM_DIS_DISABLE             1

#define SC760X_TDIE_ALM_MASK                    0x0F
#define SC760X_TDIE_ALM_SHIFT                   0
#define SC760X_TDIE_ALM_BASE                    80
#define SC760X_TDIE_ALM_LSB                     5


/* Register 0Dh */
#define SC7601_REG_0D                           0x0D
#define CHG_OVP_DEG_MASK                        0xC0
#define CHG_OVP_DEG_5US                         0
#define CHG_OVP_DEG_10MS                        1
#define CHG_OVP_DEG_200MS                       2
#define CHG_OVP_DEG_1000MS                      3

#define BAT_OVP_DEG_MASK                        0x30
#define BAT_OVP_DEG_5US                         0
#define BAT_OVP_DEG_10MS                        1
#define BAT_OVP_DEG_200MS                       2
#define BAT_OVP_DEG_1000MS                      3

#define CHG_OCP_DEG_MASK                        0x0C
#define CHG_OCP_DEG_50US                        0
#define CHG_OCP_DEG_1000US                      1
#define CHG_OCP_DEG_2000US                      2
#define CHG_OCP_DEG_5000US                      3

#define DSG_OCP_DEG_MASK                        0x03
#define DSG_OCP_DEG_50US                        0
#define DSG_OCP_DEG_1000US                      1
#define DSG_OCP_DEG_2000US                      2
#define DSG_OCP_DEG_5000US                      3

/* Register 0Eh */
#define SC7601_REG_0E                           0x0E
#define AUTO_BSM_DEG_MASK                       0xC0
#define AUTO_BSM_DEG_1S                         0
#define AUTO_BSM_DEG_2S                         1
#define AUTO_BSM_DEG_5S                         2
#define AUTO_BSM_DEG_10S                        3


/* Register 0Fh */
#define SC7601_REG_0F                           0x0F
#define WORK_MODE_MASK                          0xE0
#define WORK_MODE_FULLY_OFF                     0
#define WORK_MODE_TRICKLE_CHG                   1
#define WORK_MODE_PRE_CHG                       2
#define WORK_MODE_FULL_ON                       3
#define WORK_MODE_CURR_REGULATION               4
#define WORK_MODE_POWER_REGULATION              5


/* Register 10h */
#define SC7601_REG_10                           0x10
#define CHG_OVP_STAT_MASK                       0x80
#define CHG_OVP_NOT_TRIGGLED                    0
#define CHG_OVP_TRIGGLED                        1

#define BAT_OVP_STAT_MASK                       0x40
#define BAT_OVP_NOT_TRIGGLED                    0
#define BAT_OVP_TRIGGLED                        1

#define CHG_OCP_STAT_MASK                       0x20
#define CHG_OCP_NOT_TRIGGLED                    0
#define CHG_OCP_TRIGGLED                        1

#define DSG_OCP_STAT_MASK                       0x10
#define DSG_OCP_NOT_TRIGGLED                    0
#define DSG_OCP_TRIGGLED                        1

#define TDIE_FLT_STAT_MASK                      0x08
#define TDIE_FLT_NOT_TRIGGLED                   0
#define TDIE_FLT_TRIGGLED                       1

#define TDIE_ALM_STAT_MASK                      0x04
#define TDIE_ALM_NOT_TRIGGLED                   0
#define TDIE_ALM_TRIGGLED                       1

/* Register 0Bh */
#define SC7601_REG_11                           0x11
/* Register 0Bh */
#define SC7601_REG_12                           0x12
/* Register 0Bh */
#define SC7601_REG_13                           0x13
/* Register 0Bh */
#define SC7601_REG_14                           0x14
/* Register 15h */
#define SC7601_REG_15                           0x15
#define ADC_EN_MASK                             0x80
#define ADC_DISABLE                             0
#define ADC_ENABLE                              1

#define ADC_RATE_MASK                           0x40
#define ADC_RATE_CONTINUOUS                     0
#define ADC_RATE_ONE_SHOT                       1


/* Register 0Bh */
#define SC7601_REG_16                           0x16
/* Register 0Bh */
#define SC7601_REG_17                           0x17
/* Register 0Bh */
#define SC7601_REG_18                           0x18
/* Register 0Bh */
#define SC7601_REG_19                           0x19
/* Register 0Bh */
#define SC7601_REG_1A                           0x1A
/* Register 0Bh */
#define SC7601_REG_1B                           0x1B
/* Register 0Bh */
#define SC7601_REG_1C                           0x1C
/* Register 0Bh */
#define SC7601_REG_1D                           0x1D
/* Register 0Bh */
#define SC7601_REG_1E                           0x1E
/* Register 0Bh */
#define SC7601_REG_1F                           0x1F
/* Register 0Bh */
#define SC7601_REG_20                           0x20

#define SC760X_WORK_MODE_MASK               0xE0
#define SC760X_WORK_MODE_SHIFT              5

//adc ctrl
#define SC760X_ADC_EN_MASK                  0x80
#define SC760X_ADC_DIS                      0
#define SC760X_ADC_EN                       BIT(7)

//adc
#define SC760X_ADC_BASE                     0x17
#endif