/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mt6858-afe-clk.h  --  Mediatek 6858 afe clock ctrl definition
 *
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Shu-wei Hsu <Shu-wei.Hsu@mediatek.com>
 */

#ifndef _MT6858_AFE_CLOCK_CTRL_H_
#define _MT6858_AFE_CLOCK_CTRL_H_

#define AP_PLL_CON3 0x000c
#define PLLEN_ALL 0x0070
#define PLLEN_ALL_SET 0x0074
#define PLLEN_ALL_CLR 0x0078

#define APLL1_CON0 0x0334
#define APLL1_CON1 0x0338
#define APLL1_CON2 0x033c
#define APLL1_CON4 0x0344
#define APLL1_TUNER_CON0 0x0040

#define APLL2_CON0 0x0348
#define APLL2_CON1 0x034c
#define APLL2_CON2 0x0350
#define APLL2_CON4 0x0358
#define APLL2_TUNER_CON0 0x0044

#define CLK_CFG_6 0x0070
#define CLK_CFG_7 0x0080
#define CLK_CFG_9 0x00a0
#define CLK_CFG_10 0x00b0
#define CLK_CFG_11 0x00c0
#define CLK_CFG_12 0x00d0
#define CLK_CFG_13 0x00e0
#define CLK_CFG_UPDATE 0x004
#define CLK_CFG_UPDATE1 0x008

#define CLK_AUDDIV_0 0x0320
#define CLK_AUDDIV_1 0x0324
#define CLK_AUDDIV_2 0x0328
#define CLK_AUDDIV_3 0x0334
#define CLK_AUDDIV_4 0x0338
#define CLK_AUDDIV_5 0x033c

#define CKSYS_AUD_TOP_CFG 0x032c
#define CKSYS_AUD_TOP_MON 0x0330

#define APLL1_EN		BIT(5)
#define APLL1_CLR		BIT(5)
#define APLL1_TUNER_EN		BIT(0)
#define APLL1_SDM_PCW_CHG	BIT(31)

#define APLL2_EN		BIT(6)
#define APLL2_CLR		BIT(6)
#define APLL2_TUNER_EN		BIT(5)
#define APLL2_SDM_PCW_CHG	BIT(31)

/* CLK_AUDDIV_0 */
#define APLL12_DIV_I2SIN1_PDN_SFT               1
#define APLL12_DIV_I2SIN1_PDN_MASK              0x1
#define APLL12_DIV_I2SIN1_PDN_MASK_SFT          (0x1 << 1)
#define APLL12_DIV_I2SIN2_PDN_SFT               2
#define APLL12_DIV_I2SIN2_PDN_MASK              0x1
#define APLL12_DIV_I2SIN2_PDN_MASK_SFT          (0x1 << 2)
#define APLL12_DIV_I2SIN4_PDN_SFT               4
#define APLL12_DIV_I2SIN4_PDN_MASK              0x1
#define APLL12_DIV_I2SIN4_PDN_MASK_SFT          (0x1 << 4)
#define APLL12_DIV_I2SOUT1_PDN_SFT              7
#define APLL12_DIV_I2SOUT1_PDN_MASK             0x1
#define APLL12_DIV_I2SOUT1_PDN_MASK_SFT         (0x1 << 7)
#define APLL12_DIV_I2SOUT2_PDN_SFT              8
#define APLL12_DIV_I2SOUT2_PDN_MASK             0x1
#define APLL12_DIV_I2SOUT2_PDN_MASK_SFT         (0x1 << 8)
#define APLL12_DIV_I2SOUT4_PDN_SFT              10
#define APLL12_DIV_I2SOUT4_PDN_MASK             0x1
#define APLL12_DIV_I2SOUT4_PDN_MASK_SFT         (0x1 << 10)
#define APLL12_DIV_FMI2S_PDN_SFT                12
#define APLL12_DIV_FMI2S_PDN_MASK               0x1
#define APLL12_DIV_FMI2S_PDN_MASK_SFT           (0x1 << 12)
#define APLL_I2SIN1_MCK_SEL_SFT                 17
#define APLL_I2SIN1_MCK_SEL_MASK                0x1
#define APLL_I2SIN1_MCK_SEL_MASK_SFT            (0x1 << 17)
#define APLL_I2SIN2_MCK_SEL_SFT                 18
#define APLL_I2SIN2_MCK_SEL_MASK                0x1
#define APLL_I2SIN2_MCK_SEL_MASK_SFT            (0x1 << 18)
#define APLL_I2SIN4_MCK_SEL_SFT                 20
#define APLL_I2SIN4_MCK_SEL_MASK                0x1
#define APLL_I2SIN4_MCK_SEL_MASK_SFT            (0x1 << 20)
#define APLL_I2SOUT1_MCK_SEL_SFT                23
#define APLL_I2SOUT1_MCK_SEL_MASK               0x1
#define APLL_I2SOUT1_MCK_SEL_MASK_SFT           (0x1 << 23)
#define APLL_I2SOUT2_MCK_SEL_SFT                24
#define APLL_I2SOUT2_MCK_SEL_MASK               0x1
#define APLL_I2SOUT2_MCK_SEL_MASK_SFT           (0x1 << 24)
#define APLL_I2SOUT4_MCK_SEL_SFT                26
#define APLL_I2SOUT4_MCK_SEL_MASK               0x1
#define APLL_I2SOUT4_MCK_SEL_MASK_SFT           (0x1 << 26)
#define APLL_FMI2S_MCK_SEL_SFT                  28
#define APLL_FMI2S_MCK_SEL_MASK                 0x1
#define APLL_FMI2S_MCK_SEL_MASK_SFT             (0x1 << 28)

/* CLK_AUDDIV_1 */
#define APLL12_DIV_I2SIN1_INV_SFT               1
#define APLL12_DIV_I2SIN1_INV_MASK              0x1
#define APLL12_DIV_I2SIN1_INV_MASK_SFT          (0x1 << 1)
#define APLL12_DIV_I2SIN2_INV_SFT               2
#define APLL12_DIV_I2SIN2_INV_MASK              0x1
#define APLL12_DIV_I2SIN2_INV_MASK_SFT          (0x1 << 2)
#define APLL12_DIV_I2SIN4_INV_SFT               4
#define APLL12_DIV_I2SIN4_INV_MASK              0x1
#define APLL12_DIV_I2SIN4_INV_MASK_SFT          (0x1 << 4)
#define APLL12_DIV_I2SOUT1_INV_SFT              7
#define APLL12_DIV_I2SOUT1_INV_MASK             0x1
#define APLL12_DIV_I2SOUT1_INV_MASK_SFT         (0x1 << 7)
#define APLL12_DIV_I2SOUT2_INV_SFT              8
#define APLL12_DIV_I2SOUT2_INV_MASK             0x1
#define APLL12_DIV_I2SOUT2_INV_MASK_SFT         (0x1 << 8)
#define APLL12_DIV_I2SOUT4_INV_SFT              10
#define APLL12_DIV_I2SOUT4_INV_MASK             0x1
#define APLL12_DIV_I2SOUT4_INV_MASK_SFT         (0x1 << 10)
#define APLL12_DIV_FMI2S_INV_SFT                12
#define APLL12_DIV_FMI2S_INV_MASK               0x1
#define APLL12_DIV_FMI2S_INV_MASK_SFT           (0x1 << 12)

/* CLK_AUDDIV_2 */
#define APLL12_CK_DIV_I2SIN1_SFT                8
#define APLL12_CK_DIV_I2SIN1_MASK               0xff
#define APLL12_CK_DIV_I2SIN1_MASK_SFT           (0xff << 8)
#define APLL12_CK_DIV_I2SIN2_SFT                16
#define APLL12_CK_DIV_I2SIN2_MASK               0xff
#define APLL12_CK_DIV_I2SIN2_MASK_SFT           (0xff << 16)

/* AUD_TOP_CFG */
#define AUD_TOP_CFG_SFT                         0
#define AUD_TOP_CFG_MASK                        0xffffffff
#define AUD_TOP_CFG_MASK_SFT                    (0xffffffff << 0)

/* AUD_TOP_MON */
#define AUD_TOP_MON_SFT                         0
#define AUD_TOP_MON_MASK                        0xffffffff
#define AUD_TOP_MON_MASK_SFT                    (0xffffffff << 0)

/* CLK_AUDDIV_3 */
#define APLL12_CK_DIV_I2SIN4_SFT                0
#define APLL12_CK_DIV_I2SIN4_MASK               0xff
#define APLL12_CK_DIV_I2SIN4_MASK_SFT           (0xff << 0)
#define APLL12_CK_DIV_I2SOUT1_SFT               24
#define APLL12_CK_DIV_I2SOUT1_MASK              0xff
#define APLL12_CK_DIV_I2SOUT1_MASK_SFT          (0xff << 24)

/* CLK_AUDDIV_4 */
#define APLL12_CK_DIV_I2SOUT2_SFT               0
#define APLL12_CK_DIV_I2SOUT2_MASK              0xff
#define APLL12_CK_DIV_I2SOUT2_MASK_SFT          (0xff << 0)
#define APLL12_CK_DIV_I2SOUT4_SFT               16
#define APLL12_CK_DIV_I2SOUT4_MASK              0xff
#define APLL12_CK_DIV_I2SOUT4_MASK_SFT          (0xff << 16)

/* CLK_AUDDIV_5 */
#define APLL12_CK_DIV_FMI2S_SFT                 0
#define APLL12_CK_DIV_FMI2S_MASK                0xff
#define APLL12_CK_DIV_FMI2S_MASK_SFT            (0xff << 0)

/* APLL */
#define APLL1_W_NAME "APLL1"
#define APLL2_W_NAME "APLL2"
enum {
	MT6858_APLL1 = 0,
	MT6858_APLL2,
};

enum {
	CLK_AFE_APLL1_AUDIO,
	CLK_AFE_APLL2_AUDIO,
	CLK_AFE_APLL_TUNER1_AUDIO,
	CLK_AFE_APLL_TUNER2_AUDIO,
	CLK_AFE_AUDIO_F26M_AUDIO,
	CLK_AFE_AUDIO_HOPPING_AUDIO,
	CLK_AFE_DL0_DAC_AUDIO,
	CLK_AFE_DL0_DAC_HIRES_AUDIO,
	CLK_AFE_DL0_PREDIS_AUDIO,
	CLK_AFE_UL0_ADC_AUDIO,
	CLK_AFE_UL0_ADC_HIRES_AUDIO,
	CLK_AFE_UL1_ADC_AUDIO,
	CLK_AFE_UL1_ADC_HIRES_AUDIO,
	CLK_TOP_AUD_1_SEL,
	CLK_TOP_AUD_2_SEL,
	CLK_TOP_AUD_ENGEN1_SEL,
	CLK_TOP_AUD_ENGEN2_SEL,
	CLK_TOP_AUD_INTBUS_SEL,
	CLK_TOP_AUDIO_H_SEL,
	CLK_PERAO_P_AUDIO0_AUDIO,
	CLK_PERAO_P_AUDIO1_AUDIO,
	CLK_PERAO_P_AUDIO2_AUDIO,
	CLK_TOP_APLL1,
	CLK_TOP_APLL1_D2,
	CLK_TOP_APLL1_D4,
	CLK_TOP_APLL2,
	CLK_TOP_APLL2_D2,
	CLK_TOP_APLL2_D4,
	CLK_TOP_APLL_I2SIN1_MCK_SEL,
	CLK_TOP_APLL12_CK_DIV_I2SIN1,
	CLK_TOP_TCK_26M_MX9,
	CLK_NUM
};

struct mtk_base_afe;

int mt6858_init_clock(struct mtk_base_afe *afe);
int mt6858_afe_enable_clock(struct mtk_base_afe *afe);
void mt6858_afe_disable_clock(struct mtk_base_afe *afe);
int mt6858_afe_disable_apll(struct mtk_base_afe *afe);
int mt6858_afe_enable_ao_clock(struct mtk_base_afe *afe);


int mt6858_afe_sram_request(struct mtk_base_afe *afe);
void mt6858_afe_sram_release(struct mtk_base_afe *afe);

int mt6858_afe_dram_request(struct device *dev);
int mt6858_afe_dram_release(struct device *dev);

int mt6858_apll1_enable(struct mtk_base_afe *afe);
void mt6858_apll1_disable(struct mtk_base_afe *afe);

int mt6858_apll2_enable(struct mtk_base_afe *afe);
void mt6858_apll2_disable(struct mtk_base_afe *afe);

int mt6858_get_apll_rate(struct mtk_base_afe *afe, int apll);
int mt6858_get_apll_by_rate(struct mtk_base_afe *afe, int rate);
int mt6858_get_apll_by_name(struct mtk_base_afe *afe, const char *name);

extern void aud_intbus_mux_sel(unsigned int aud_idx);

/* these will be replaced by using CCF */
int mt6858_mck_enable(struct mtk_base_afe *afe, int mck_id, int rate);
int mt6858_mck_disable(struct mtk_base_afe *afe, int mck_id, int rate);

int mt6858_set_audio_int_bus_parent(struct mtk_base_afe *afe, int clk_id);

#endif
