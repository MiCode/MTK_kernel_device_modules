/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Mediatek MT6991 audio driver interconnection definition
 *
 *  Copyright (c) 2023 MediaTek Inc.
 *  Author: Yujie Xiao <yujie.xiao@mediatek.com>
 */

#ifndef _MT6991_INTERCONNECTION_H_
#define _MT6991_INTERCONNECTION_H_

/* in port define */
#define I_CONNSYS_I2S_CH1 0
#define I_CONNSYS_I2S_CH2 1
#define I_GAIN0_OUT_CH1 6
#define I_GAIN0_OUT_CH2 7
#define I_GAIN1_OUT_CH1 8
#define I_GAIN1_OUT_CH2 9
#define I_GAIN2_OUT_CH1 10
#define I_GAIN2_OUT_CH2 11
#define I_GAIN3_OUT_CH1 12
#define I_GAIN3_OUT_CH2 13
#define I_STF_CH1 14
#define I_ADDA_UL_CH1 16
#define I_ADDA_UL_CH2 17
#define I_ADDA_UL_CH3 18
#define I_ADDA_UL_CH4 19
#define I_UL_PROX_CH1 20
#define I_UL_PROX_CH2 21
#define I_ADDA_UL_CH5 24
#define I_ADDA_UL_CH6 25
#define I_DMIC0_CH1 28
#define I_DMIC0_CH2 29
#define I_DMIC1_CH1 30
#define I_DMIC1_CH2 31

/* in port define >= 32 */
#define I_32_OFFSET 32
#define I_DL0_CH1 (32 - I_32_OFFSET)
#define I_DL0_CH2 (33 - I_32_OFFSET)
#define I_DL1_CH1 (34 - I_32_OFFSET)
#define I_DL1_CH2 (35 - I_32_OFFSET)
#define I_DL2_CH1 (36 - I_32_OFFSET)
#define I_DL2_CH2 (37 - I_32_OFFSET)
#define I_DL3_CH1 (38 - I_32_OFFSET)
#define I_DL3_CH2 (39 - I_32_OFFSET)
#define I_DL4_CH1 (40 - I_32_OFFSET)
#define I_DL4_CH2 (41 - I_32_OFFSET)
#define I_DL5_CH1 (42 - I_32_OFFSET)
#define I_DL5_CH2 (43 - I_32_OFFSET)
#define I_DL6_CH1 (44 - I_32_OFFSET)
#define I_DL6_CH2 (45 - I_32_OFFSET)
#define I_DL7_CH1 (46 - I_32_OFFSET)
#define I_DL7_CH2 (47 - I_32_OFFSET)
#define I_DL8_CH1 (48 - I_32_OFFSET)
#define I_DL8_CH2 (49 - I_32_OFFSET)
#define I_DL_4CH_CH1 (50 - I_32_OFFSET)
#define I_DL_4CH_CH2 (51 - I_32_OFFSET)
#define I_DL_4CH_CH3 (52 - I_32_OFFSET)
#define I_DL_4CH_CH4 (53 - I_32_OFFSET)
#define I_DL_24CH_CH1 (54 - I_32_OFFSET)
#define I_DL_24CH_CH2 (55 - I_32_OFFSET)
#define I_DL_24CH_CH3 (56 - I_32_OFFSET)
#define I_DL_24CH_CH4 (57 - I_32_OFFSET)
#define I_DL_24CH_CH5 (58 - I_32_OFFSET)
#define I_DL_24CH_CH6 (59 - I_32_OFFSET)
#define I_DL_24CH_CH7 (60 - I_32_OFFSET)
#define I_DL_24CH_CH8 (61 - I_32_OFFSET)
#define I_DL_24CH_CH9 (62 - I_32_OFFSET)
#define I_DL_24CH_CH10 (63 - I_32_OFFSET)

/* in port define >= 64 */
#define I_64_OFFSET 64
#define I_DL_24CH_CH11 (64 - I_64_OFFSET)
#define I_DL_24CH_CH12 (65 - I_64_OFFSET)
#define I_DL_24CH_CH13 (66 - I_64_OFFSET)
#define I_DL_24CH_CH14 (67 - I_64_OFFSET)
#define I_DL_24CH_CH15 (68 - I_64_OFFSET)
#define I_DL_24CH_CH16 (69 - I_64_OFFSET)
#define I_DL23_CH1 (78 - I_64_OFFSET)
#define I_DL23_CH2 (79 - I_64_OFFSET)
#define I_DL24_CH1 (80 - I_64_OFFSET)
#define I_DL24_CH2 (81 - I_64_OFFSET)
#define I_DL25_CH1 (82 - I_64_OFFSET)
#define I_DL25_CH2 (83 - I_64_OFFSET)
#define I_DL26_CH1 (84 - I_64_OFFSET)
#define I_DL26_CH2 (85 - I_64_OFFSET)

/* in port define >= 128 */
#define I_128_OFFSET 128
#define I_PCM_0_CAP_CH1 (130 - I_128_OFFSET)
#define I_PCM_0_CAP_CH2 (131 - I_128_OFFSET)
#define I_PCM_1_CAP_CH1 (132 - I_128_OFFSET)
#define I_PCM_1_CAP_CH2 (133 - I_128_OFFSET)
#define I_I2SIN0_CH1 (134 - I_128_OFFSET)
#define I_I2SIN0_CH2 (135 - I_128_OFFSET)
#define I_I2SIN1_CH1 (136 - I_128_OFFSET)
#define I_I2SIN1_CH2 (137 - I_128_OFFSET)
#define I_I2SIN2_CH1 (138 - I_128_OFFSET)
#define I_I2SIN2_CH2 (139 - I_128_OFFSET)
#define I_I2SIN3_CH1 (140 - I_128_OFFSET)
#define I_I2SIN3_CH2 (141 - I_128_OFFSET)
#define I_I2SIN4_CH1 (142 - I_128_OFFSET)
#define I_I2SIN4_CH2 (143 - I_128_OFFSET)
#define I_I2SIN4_CH3 (144 - I_128_OFFSET)
#define I_I2SIN4_CH4 (145 - I_128_OFFSET)
#define I_I2SIN4_CH5 (146 - I_128_OFFSET)
#define I_I2SIN4_CH6 (147 - I_128_OFFSET)
#define I_I2SIN4_CH7 (148 - I_128_OFFSET)
#define I_I2SIN4_CH8 (149 - I_128_OFFSET)

#define I_I2SIN5_CH1 (150 - I_128_OFFSET)
#define I_I2SIN5_CH2 (151 - I_128_OFFSET)
#define I_I2SIN5_CH3 (152 - I_128_OFFSET)
#define I_I2SIN5_CH4 (153 - I_128_OFFSET)
#define I_I2SIN5_CH5 (154 - I_128_OFFSET)
#define I_I2SIN5_CH6 (155 - I_128_OFFSET)
#define I_I2SIN5_CH7 (156 - I_128_OFFSET)
#define I_I2SIN5_CH8 (157 - I_128_OFFSET)
#define I_I2SIN5_CH9 (158 - I_128_OFFSET)
#define I_I2SIN5_CH10 (159 - I_128_OFFSET)

/* in port define >= 160 */
#define I_160_OFFSET 160
#define I_I2SIN5_CH11 (160 - I_160_OFFSET)
#define I_I2SIN5_CH12 (161 - I_160_OFFSET)
#define I_I2SIN5_CH13 (162 - I_160_OFFSET)
#define I_I2SIN5_CH14 (163 - I_160_OFFSET)
#define I_I2SIN5_CH15 (164 - I_160_OFFSET)
#define I_I2SIN5_CH16 (165 - I_160_OFFSET)

#define I_I2SIN6_CH1 (166 - I_160_OFFSET)
#define I_I2SIN6_CH2 (167 - I_160_OFFSET)
#define I_I2SIN6_CH3 (168 - I_160_OFFSET)
#define I_I2SIN6_CH4 (169 - I_160_OFFSET)
#define I_I2SIN6_CH5 (170 - I_160_OFFSET)
#define I_I2SIN6_CH6 (171 - I_160_OFFSET)
#define I_I2SIN6_CH7 (172 - I_160_OFFSET)
#define I_I2SIN6_CH8 (173 - I_160_OFFSET)
#define I_I2SIN6_CH9 (174 - I_160_OFFSET)
#define I_I2SIN6_CH10 (175 - I_160_OFFSET)
#define I_I2SIN6_CH11 (176 - I_160_OFFSET)
#define I_I2SIN6_CH12 (177 - I_160_OFFSET)
#define I_I2SIN6_CH13 (178 - I_160_OFFSET)
#define I_I2SIN6_CH14 (179 - I_160_OFFSET)
#define I_I2SIN6_CH15 (180 - I_160_OFFSET)
#define I_I2SIN6_CH16 (181 - I_160_OFFSET)

/* in port define >= 192 */
#define I_192_OFFSET 192
#define I_SRC_0_OUT_CH1 (198 - I_192_OFFSET)
#define I_SRC_0_OUT_CH2 (199 - I_192_OFFSET)
#define I_SRC_1_OUT_CH1 (200 - I_192_OFFSET)
#define I_SRC_1_OUT_CH2 (201 - I_192_OFFSET)
#define I_SRC_2_OUT_CH1 (202 - I_192_OFFSET)
#define I_SRC_2_OUT_CH2 (203 - I_192_OFFSET)
#define I_SRC_3_OUT_CH1 (204 - I_192_OFFSET)
#define I_SRC_3_OUT_CH2 (205 - I_192_OFFSET)

#endif
