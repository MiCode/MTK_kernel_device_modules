// SPDX-License-Identifier: GPL-2.0
/*
 *  MediaTek ALSA SoC Audio DAI SRC Control
 *
 *  Copyright (c) 2023 MediaTek Inc.
 *  Author: Yujie Xiao <yujie.xiao@mediatek.com>
 */

#include <linux/regmap.h>
#include <sound/pcm_params.h>
#include "mt6881-afe-common.h"
#include "mt6881-interconnection.h"
#include "mt6881-afe-clk.h"

#define GSRC_REG

#define AUTO_TUNE_FREQ2		0x1
#define AUTO_TUNE_FREQ3		0x1

#define FREQ_CALI_CYCLE(x)	(((x - 1) & 0xffff) << 16)
#define MT6881_SRC_NUM		(MT6881_DAI_SRC_NUM - MT6881_DAI_SRC_0)

#define SRC_FREQ_CALC_DENOMINATOR_49M		(0x3C00)
#define SRC_FREQ_CALC_DENOMINATOR_45M		(0x3720)
#define SRC_FREQ_CALC_DENOMINATOR_26M_48	(0x130B)
#define SRC_FREQ_CALC_DENOMINATOR_26M_96	(0x1FBD)
#define SRC_FREQ_CALC_DENOMINATOR_26M_441	(0x1FBD)

/* cali clk */
enum {
	NO_NEED_CALI_CLK = -1,
	CALI_CLK_26M,
	CALI_CLK_49M,
	CALI_CLK_45M,
};

/* cali lrck source */
enum {
	NO_SLAVE_DOMAIN = -1,
	AFE_CONNSYS_I2S_IN_WS,
	AFE_PCM0_SYNC_OUT,
	AFE_PCM0_SYNC_IN,
	AFE_PCM1_SYNC_IN,
	ETDM_OUT0_MASTER_LRCK,
	ETDM_OUT1_MASTER_LRCK,
	ETDM_OUT2_MASTER_LRCK,
	ETDM_OUT3_MASTER_LRCK,
	ETDM_OUT4_MASTER_LRCK,
	ETDM_OUT5_MASTER_LRCK,
	ETDM_OUT6_MASTER_LRCK,
	ETDM_OUT7_MASTER_LRCK,
	ETDM_IN0_SAMPLE_END,
	ETDM_IN1_SAMPLE_END,
	ETDM_IN2_SAMPLE_END,
	ETDM_IN3_SAMPLE_END,
	ETDM_IN4_SAMPLE_END,
	ETDM_IN5_SAMPLE_END,
	ETDM_IN6_SAMPLE_END,
	ETDM_IN7_SAMPLE_END,
};

/* cali domain */
enum {
	CLK_DOMAIN_NULL  = -1,
	CLK_DOMAIN_HOPPING,
	CLK_DOMAIN_APLL,
	CLK_DOMAIN_SPDIF,
	CLK_DOMAIN_HDMI,
	CLK_DOMAIN_EARC,
	CLK_DOMAIN_LINEIN,
	CLK_DOMAIN_SLAVE,
	CLK_DOMAIN_NUM,
};

/* in fs*/
enum {
	MTK_AFE_RATE_CONNSYS_I2S_EXT,
	MTK_AFE_RATE_PCM0_EXT,
	MTK_AFE_RATE_PCM1_EXT,
	MTK_AFE_RATE_ZERO,
	MTK_AFE_RATE_ETDM_OUT0,
	MTK_AFE_RATE_ETDM_OUT1,
	MTK_AFE_RATE_ETDM_OUT2,
	MTK_AFE_RATE_ETDM_OUT3,
	MTK_AFE_RATE_ETDM_OUT4,
	MTK_AFE_RATE_ETDM_OUT5,
	MTK_AFE_RATE_ETDM_OUT6,
	MTK_AFE_RATE_ETDM_OUT7,
	MTK_AFE_RATE_ETDM_IN0,
	MTK_AFE_RATE_ETDM_IN1,
	MTK_AFE_RATE_ETDM_IN2,
	MTK_AFE_RATE_ETDM_IN3,
	MTK_AFE_RATE_ETDM_IN4,
	MTK_AFE_RATE_ETDM_IN5,
	MTK_AFE_RATE_ETDM_IN6,
	MTK_AFE_RATE_ETDM_IN7,
};

enum {
	OFS_ASM_FREQ_0,
	OFS_ASM_FREQ_1,
	IFS_ASM_FREQ_2,
	IFS_ASM_FREQ_3
};

enum {
	CALI_USE_PERIOD_OUT,
	CALI_USE_FREQ_OUT,
};

struct src_domain {
	unsigned int lrck_source;
	unsigned int in_sel_domain;
	unsigned int in_sel_fs;
	unsigned int out_sel_domain;
	unsigned int out_sel_fs;
};

static const struct src_domain src_domain_info[] = {
	{
		/* DL SLAVE -> SRC -> I2SOUTx */
		.in_sel_domain = CLK_DOMAIN_SLAVE,
		.in_sel_fs = MTK_AFE_RATE_ETDM_IN4,
		.out_sel_domain = CLK_DOMAIN_APLL,
		.out_sel_fs = 0,
	},
	{
		/* I2SINx -> SRC -> UL SLAVE */
		.in_sel_domain = CLK_DOMAIN_APLL,
		.in_sel_fs = 0,
		.out_sel_domain = CLK_DOMAIN_SLAVE,
		.out_sel_fs = MTK_AFE_RATE_ETDM_IN4,
	},
	{
		/* I2SINx SLAVE -> SRC -> UL MASTER */
		.in_sel_domain = CLK_DOMAIN_SLAVE,
		.in_sel_fs = MTK_AFE_RATE_ETDM_IN4,
		.out_sel_domain = CLK_DOMAIN_HOPPING,
		.out_sel_fs = 0,
	},
	{
		/* DL MASTER -> SRC -> I2SOUTx SLAVE */
		.in_sel_domain = CLK_DOMAIN_HOPPING,
		.in_sel_fs = 0,
		.out_sel_domain = CLK_DOMAIN_SLAVE,
		.out_sel_fs = MTK_AFE_RATE_ETDM_IN4,
	},
};

struct mtk_afe_src_priv {
	struct snd_pcm_substream *substream;
	int dl_rate;
	int ul_rate;
	int cali_rx;
	int cali_tx;
	int cali_clk;
	int cali_cycle;
	int lrck_source;
	int one_heart_mode;
};

struct mtk_dai_src_reg {
	unsigned int con0;
	unsigned int con1;
	unsigned int con2;
	unsigned int con3;
	unsigned int con4;
	unsigned int con5;
	unsigned int con6;
	unsigned int con7;
	unsigned int con8;
	unsigned int con9;
	unsigned int con10;
	unsigned int con11;
	unsigned int con12;
	unsigned int con13;
	unsigned int con14;
};

static const struct mtk_dai_src_reg src_reg[MT6881_SRC_NUM] = {
	{
		.con0 = AFE_GASRC0_NEW_CON0,
		.con1 = AFE_GASRC0_NEW_CON1,
		.con2 = AFE_GASRC0_NEW_CON2,
		.con3 = AFE_GASRC0_NEW_CON3,
		.con4 = AFE_GASRC0_NEW_CON4,
		.con5 = AFE_GASRC0_NEW_CON5,
		.con6 = AFE_GASRC0_NEW_CON6,
		.con7 = AFE_GASRC0_NEW_CON7,
		.con8 = AFE_GASRC0_NEW_CON8,
		.con9 = AFE_GASRC0_NEW_CON9,
		.con10 = AFE_GASRC0_NEW_CON10,
		.con11 = AFE_GASRC0_NEW_CON11,
		.con12 = AFE_GASRC0_NEW_CON12,
		.con13 = AFE_GASRC0_NEW_CON13,
		.con14 = AFE_GASRC0_NEW_CON14,
	},
	{
		.con0 = AFE_GASRC1_NEW_CON0,
		.con1 = AFE_GASRC1_NEW_CON1,
		.con2 = AFE_GASRC1_NEW_CON2,
		.con3 = AFE_GASRC1_NEW_CON3,
		.con4 = AFE_GASRC1_NEW_CON4,
		.con5 = AFE_GASRC1_NEW_CON5,
		.con6 = AFE_GASRC1_NEW_CON6,
		.con7 = AFE_GASRC1_NEW_CON7,
		.con8 = AFE_GASRC1_NEW_CON8,
		.con9 = AFE_GASRC1_NEW_CON9,
		.con10 = AFE_GASRC1_NEW_CON10,
		.con11 = AFE_GASRC1_NEW_CON11,
		.con12 = AFE_GASRC1_NEW_CON12,
		.con13 = AFE_GASRC1_NEW_CON13,
		.con14 = AFE_GASRC1_NEW_CON14,
	},
	{
		.con0 = AFE_GASRC2_NEW_CON0,
		.con1 = AFE_GASRC2_NEW_CON1,
		.con2 = AFE_GASRC2_NEW_CON2,
		.con3 = AFE_GASRC2_NEW_CON3,
		.con4 = AFE_GASRC2_NEW_CON4,
		.con5 = AFE_GASRC2_NEW_CON5,
		.con6 = AFE_GASRC2_NEW_CON6,
		.con7 = AFE_GASRC2_NEW_CON7,
		.con8 = AFE_GASRC2_NEW_CON8,
		.con9 = AFE_GASRC2_NEW_CON9,
		.con10 = AFE_GASRC2_NEW_CON10,
		.con11 = AFE_GASRC2_NEW_CON11,
		.con12 = AFE_GASRC2_NEW_CON12,
		.con13 = AFE_GASRC2_NEW_CON13,
		.con14 = AFE_GASRC2_NEW_CON14,
	},
	{
		.con0 = AFE_GASRC3_NEW_CON0,
		.con1 = AFE_GASRC3_NEW_CON1,
		.con2 = AFE_GASRC3_NEW_CON2,
		.con3 = AFE_GASRC3_NEW_CON3,
		.con4 = AFE_GASRC3_NEW_CON4,
		.con5 = AFE_GASRC3_NEW_CON5,
		.con6 = AFE_GASRC3_NEW_CON6,
		.con7 = AFE_GASRC3_NEW_CON7,
		.con8 = AFE_GASRC3_NEW_CON8,
		.con9 = AFE_GASRC3_NEW_CON9,
		.con10 = AFE_GASRC3_NEW_CON10,
		.con11 = AFE_GASRC3_NEW_CON11,
		.con12 = AFE_GASRC3_NEW_CON12,
		.con13 = AFE_GASRC3_NEW_CON13,
		.con14 = AFE_GASRC3_NEW_CON14,
	},
	{
		.con0 = AFE_GASRC4_NEW_CON0,
		.con1 = AFE_GASRC4_NEW_CON1,
		.con2 = AFE_GASRC4_NEW_CON2,
		.con3 = AFE_GASRC4_NEW_CON3,
		.con4 = AFE_GASRC4_NEW_CON4,
		.con5 = AFE_GASRC4_NEW_CON5,
		.con6 = AFE_GASRC4_NEW_CON6,
		.con7 = AFE_GASRC4_NEW_CON7,
		.con8 = AFE_GASRC4_NEW_CON8,
		.con9 = AFE_GASRC4_NEW_CON9,
		.con10 = AFE_GASRC4_NEW_CON10,
		.con11 = AFE_GASRC4_NEW_CON11,
		.con12 = AFE_GASRC4_NEW_CON12,
		.con13 = AFE_GASRC4_NEW_CON13,
		.con14 = AFE_GASRC4_NEW_CON14,
	},
	{
		.con0 = AFE_GASRC5_NEW_CON0,
		.con1 = AFE_GASRC5_NEW_CON1,
		.con2 = AFE_GASRC5_NEW_CON2,
		.con3 = AFE_GASRC5_NEW_CON3,
		.con4 = AFE_GASRC5_NEW_CON4,
		.con5 = AFE_GASRC5_NEW_CON5,
		.con6 = AFE_GASRC5_NEW_CON6,
		.con7 = AFE_GASRC5_NEW_CON7,
		.con8 = AFE_GASRC5_NEW_CON8,
		.con9 = AFE_GASRC5_NEW_CON9,
		.con10 = AFE_GASRC5_NEW_CON10,
		.con11 = AFE_GASRC5_NEW_CON11,
		.con12 = AFE_GASRC5_NEW_CON12,
		.con13 = AFE_GASRC5_NEW_CON13,
		.con14 = AFE_GASRC5_NEW_CON14,
	},
	{
		.con0 = AFE_GASRC6_NEW_CON0,
		.con1 = AFE_GASRC6_NEW_CON1,
		.con2 = AFE_GASRC6_NEW_CON2,
		.con3 = AFE_GASRC6_NEW_CON3,
		.con4 = AFE_GASRC6_NEW_CON4,
		.con5 = AFE_GASRC6_NEW_CON5,
		.con6 = AFE_GASRC6_NEW_CON6,
		.con7 = AFE_GASRC6_NEW_CON7,
		.con8 = AFE_GASRC6_NEW_CON8,
		.con9 = AFE_GASRC6_NEW_CON9,
		.con10 = AFE_GASRC6_NEW_CON10,
		.con11 = AFE_GASRC6_NEW_CON11,
		.con12 = AFE_GASRC6_NEW_CON12,
		.con13 = AFE_GASRC6_NEW_CON13,
		.con14 = AFE_GASRC6_NEW_CON14,
	},
	{
		.con0 = AFE_GASRC7_NEW_CON0,
		.con1 = AFE_GASRC7_NEW_CON1,
		.con2 = AFE_GASRC7_NEW_CON2,
		.con3 = AFE_GASRC7_NEW_CON3,
		.con4 = AFE_GASRC7_NEW_CON4,
		.con5 = AFE_GASRC7_NEW_CON5,
		.con6 = AFE_GASRC7_NEW_CON6,
		.con7 = AFE_GASRC7_NEW_CON7,
		.con8 = AFE_GASRC7_NEW_CON8,
		.con9 = AFE_GASRC7_NEW_CON9,
		.con10 = AFE_GASRC7_NEW_CON10,
		.con11 = AFE_GASRC7_NEW_CON11,
		.con12 = AFE_GASRC7_NEW_CON12,
		.con13 = AFE_GASRC7_NEW_CON13,
		.con14 = AFE_GASRC7_NEW_CON14,
	},
};

static const unsigned int src_iir_coeff_32_to_16[] = {
	0x1bd356f3, 0x012e014f, 0x1bd356f3, 0x081be0a6, 0xe28e2407, 0x00000002,
	0x0d7d8ee8, 0x01b9274d, 0x0d7d8ee8, 0x09857a7b, 0xe4cae309, 0x00000002,
	0x0c999cbe, 0x038e89c5, 0x0c999cbe, 0x0beae5bc, 0xe7ded2a4, 0x00000002,
	0x0b4b6e2c, 0x061cd206, 0x0b4b6e2c, 0x0f6a2551, 0xec069422, 0x00000002,
	0x13ad5974, 0x129397e7, 0x13ad5974, 0x13d3c166, 0xf11cacb8, 0x00000002,
	0x126195d4, 0x1b259a6c, 0x126195d4, 0x184cdd94, 0xf634a151, 0x00000002,
	0x092aa1ea, 0x11add077, 0x092aa1ea, 0x3682199e, 0xf31b28fc, 0x00000001,
	0x0e09b91b, 0x0010b76f, 0x0e09b91b, 0x0f0e2575, 0xc19d364a, 0x00000001
};

static const unsigned int src_iir_coeff_44_to_16[] = {
	0x0c4deacd, 0xf5b3be35, 0x0c4deacd, 0x20349d1f, 0xe0b9a80d, 0x00000002,
	0x0c5dbbaa, 0xf6157998, 0x0c5dbbaa, 0x200c143d, 0xe25209ea, 0x00000002,
	0x0a9de1bd, 0xf85ee460, 0x0a9de1bd, 0x206099de, 0xe46a166c, 0x00000002,
	0x081f9a34, 0xfb7ffe47, 0x081f9a34, 0x212dd0f7, 0xe753c9ab, 0x00000002,
	0x0a6f9ddb, 0xfd863e9e, 0x0a6f9ddb, 0x226bd8a2, 0xeb2ead0b, 0x00000002,
	0x05497d0e, 0x01ebd7f0, 0x05497d0e, 0x23eba2f6, 0xef958aff, 0x00000002,
	0x008e7c5f, 0x00be6aad, 0x008e7c5f, 0x4a74b30a, 0xe6b0319a, 0x00000001,
	0x00000000, 0x38f3c5aa, 0x38f3c5aa, 0x012e1306, 0x00000000, 0x00000006
};

static const unsigned int src_iir_coeff_44_to_32[] = {

	0x0db45c84, 0x1113e68a, 0x0db45c84, 0xdf58fbd3, 0xe0e51ba2, 0x00000002,
	0x0e0c4d8f, 0x11eaf5ef, 0x0e0c4d8f, 0xe11e9264, 0xe2da4b80, 0x00000002,
	0x0cf2558c, 0x1154c11a, 0x0cf2558c, 0xe41c6288, 0xe570c517, 0x00000002,
	0x0b5132d7, 0x10545ecd, 0x0b5132d7, 0xe8e2e944, 0xe92f8dc6, 0x00000002,
	0x1234ffbb, 0x1cfba5c7, 0x1234ffbb, 0xf00653e0, 0xee9406e3, 0x00000002,
	0x0cfd073a, 0x170277ad, 0x0cfd073a, 0xf96e16e7, 0xf59562f9, 0x00000002,
	0x08506c2b, 0x1011cd72, 0x08506c2b, 0x164a9eae, 0xe4203311, 0xffffffff,
	0x00000000, 0x3d58af1e, 0x3d58af1e, 0x001bee13, 0x00000000, 0x00000007
};

static const unsigned int src_iir_coeff_48_to_16[] = {
	0x0296a398, 0xfd69dca0, 0x0296a398, 0x209438ae, 0xe01ff8bd, 0x00000002,
	0x0f4ff31a, 0xf0d6d390, 0x0f4ff31a, 0x209bc955, 0xe076c324, 0x00000002,
	0x0e848ff6, 0xf1fe6347, 0x0e848ff6, 0x20cfd5ae, 0xe12123ee, 0x00000002,
	0x14852eaf, 0xed794a1e, 0x14852eaf, 0x21503c83, 0xe28b3223, 0x00000002,
	0x1362223b, 0xf17676c3, 0x1362223b, 0x225be0ce, 0xe56963fa, 0x00000002,
	0x097f5e31, 0xfca98852, 0x097f5e31, 0x24310c19, 0xea69523c, 0x00000002,
	0x0698d721, 0x04abec68, 0x0698d721, 0x4ced8edd, 0xe134d677, 0x00000001,
	0x00000000, 0x3aebe58f, 0x3aebe58f, 0x04f3b027, 0x00000000, 0x00000004,
};

static const unsigned int src_iir_coeff_48_to_32[] = {
	0x0eca2fa9, 0x0f2b0cd3, 0x0eca2fa9, 0xf50313ef, 0xf15857a7, 0x00000003,
	0x0ee239a9, 0x1045115c, 0x0ee239a9, 0xec9f2976, 0xe5090807, 0x00000002,
	0x0ec57a45, 0x11d000f7, 0x0ec57a45, 0xf0bb67bb, 0xe84c86de, 0x00000002,
	0x0e85ba7e, 0x13ee7e9a, 0x0e85ba7e, 0xf6c74ebb, 0xecdba82c, 0x00000002,
	0x1cba1ac9, 0x2da90ada, 0x1cba1ac9, 0xfecba589, 0xf2c756e1, 0x00000002,
	0x0f79dec4, 0x1c27f5e0, 0x0f79dec4, 0x03c44399, 0xfc96c6aa, 0x00000003,
	0x1104a702, 0x21a72c89, 0x1104a702, 0x1b6a6fb8, 0xfb5ee0f2, 0x00000001,
	0x0622fc30, 0x061a0c67, 0x0622fc30, 0xe88911f2, 0xe0da327a, 0x00000002
};

static const unsigned int src_iir_coeff_48_to_44[] = {
	0x04c9f583, 0x09432e05, 0x04c9f583, 0xe2110f3c, 0xf02e6fc0, 0x00000003,
	0x07ba6f6a, 0x0efa321a, 0x07ba6f6a, 0xe28bbeec, 0xf0961f38, 0x00000003,
	0x078c0697, 0x0eaf26e3, 0x078c0697, 0xe34c6da5, 0xf1252a3b, 0x00000003,
	0x0740313a, 0x0e309947, 0x0740313a, 0xe493cc7c, 0xf20a3c0c, 0x00000003,
	0x0d782eb5, 0x1a8c0013, 0x0d782eb5, 0xe6e46d5d, 0xf39f3526, 0x00000003,
	0x0b9857c6, 0x17043894, 0x0b9857c6, 0xeb3b4422, 0xf68fd3be, 0x00000003,
	0x0444fe1c, 0x08857bff, 0x0444fe1c, 0xc9c6b7ed, 0xedbd865b, 0x00000001,
	0x00000000, 0x7fffffff, 0x7fffffff, 0xffbc6f93, 0x00000000, 0x00000007
};

static const unsigned int src_iir_coeff_96_to_16[] = {
	0x05c89f29, 0xf6443184, 0x05c89f29, 0x1bbe0f00, 0xf034bf19, 0x00000003,
	0x05e47be3, 0xf6284bfe, 0x05e47be3, 0x1b73d610, 0xf0a9a268, 0x00000003,
	0x09eb6c29, 0xefbc8df5, 0x09eb6c29, 0x365264ff, 0xe286ce76, 0x00000002,
	0x0741f28e, 0xf492d155, 0x0741f28e, 0x35a08621, 0xe4320cfe, 0x00000002,
	0x087cdc22, 0xf3daa1c7, 0x087cdc22, 0x34c55ef0, 0xe6664705, 0x00000002,
	0x038022af, 0xfc43da62, 0x038022af, 0x33d2b188, 0xe8e92eb8, 0x00000002,
	0x001de8ed, 0x0001bd74, 0x001de8ed, 0x33061aa8, 0xeb0d6ae7, 0x00000002,
	0x00000000, 0x3abd8743, 0x3abd8743, 0x032b3f7f, 0x00000000, 0x00000005
};

static const unsigned int src_iir_coeff_96_to_44[] = {
	0x1b4feb25, 0xfa1874df, 0x1b4feb25, 0x0fc84364, 0xe27e7427, 0x00000002,
	0x0d22ad1f, 0xfe465ea8, 0x0d22ad1f, 0x10d89ab2, 0xe4aa760e, 0x00000002,
	0x0c17b497, 0x004c9a14, 0x0c17b497, 0x12ba36ef, 0xe7a11513, 0x00000002,
	0x0a968b87, 0x031b65c2, 0x0a968b87, 0x157c39d1, 0xeb9561ce, 0x00000002,
	0x11cea26a, 0x0d025bcc, 0x11cea26a, 0x18ef4a32, 0xf05a2342, 0x00000002,
	0x0fe5d188, 0x156af55c, 0x0fe5d188, 0x1c6234df, 0xf50cd288, 0x00000002,
	0x07a1ea25, 0x0e900dd7, 0x07a1ea25, 0x3d441ae6, 0xf0314c15, 0x00000001,
	0x0dd3517a, 0xfc7f1621, 0x0dd3517a, 0x1ee4972a, 0xc193ad77, 0x00000001
};

struct src_freq {
	unsigned int rate;
	unsigned int freq_val;
};
static const struct src_freq src_palette_no_cali[] = {
	{ .rate =  8000,		.freq_val = 0x050000},
	{ .rate =  11025,		.freq_val = 0x06E400},
	{ .rate =  12000,		.freq_val = 0x078000},
	{ .rate =  16000,		.freq_val = 0x0A0000},
	{ .rate =  22050,		.freq_val = 0x0DC800},
	{ .rate =  24000,		.freq_val = 0x0F0000},
	{ .rate =  32000,		.freq_val = 0x140000},
	{ .rate =  44100,		.freq_val = 0x1B9000},
	{ .rate =  48000,		.freq_val = 0x1E0000},
	{ .rate =  88200,		.freq_val = 0x372000},
	{ .rate =  96000,		.freq_val = 0x3C0000},
	{ .rate =  176400,		.freq_val = 0x6E4000},
	{ .rate =  192000,		.freq_val = 0x780000},
	{ .rate =  352800,		.freq_val = 0xDC8000},
	{ .rate =  384000,		.freq_val = 0xF00000},
};

static const struct src_freq src_freq_palette_49m_45m_64_cycles[] = {
	{ .rate =  8000,		.freq_val = 0x050000},
	{ .rate =  11025,		.freq_val = 0x06E400},
	{ .rate =  12000,		.freq_val = 0x078000},
	{ .rate =  16000,		.freq_val = 0x0A0000},
	{ .rate =  22050,		.freq_val = 0x0DC800},
	{ .rate =  24000,		.freq_val = 0x0F0000},
	{ .rate =  32000,		.freq_val = 0x140000},
	{ .rate =  44100,		.freq_val = 0x1B9000},
	{ .rate =  48000,		.freq_val = 0x1E0000},
	{ .rate =  88200,		.freq_val = 0x372000},
	{ .rate =  96000,		.freq_val = 0x3C0000},
	{ .rate =  176400,		.freq_val = 0x6E4000},
	{ .rate =  192000,		.freq_val = 0x780000},
	{ .rate =  352800,		.freq_val = 0xDC8000},
	{ .rate =  384000,		.freq_val = 0xF00000},
};

static const struct src_freq src_freq_palette_26m_48_cycles[] = {
	{ .rate =  8000,		.freq_val = 0x040000},
	{ .rate =  11025,		.freq_val = 0x058333},
	{ .rate =  12000,		.freq_val = 0x060000},
	{ .rate =  16000,		.freq_val = 0x080000},
	{ .rate =  22050,		.freq_val = 0x0B0666},
	{ .rate =  24000,		.freq_val = 0x0C0000},
	{ .rate =  32000,		.freq_val = 0x100000},
	{ .rate =  44100,		.freq_val = 0x160CCC},
	{ .rate =  48000,		.freq_val = 0x180000},
	{ .rate =  88200,		.freq_val = 0x2C1999},
	{ .rate =  96000,		.freq_val = 0x300000},
	{ .rate =  176400,		.freq_val = 0x583333},
	{ .rate =  192000,		.freq_val = 0x600000},
	{ .rate =  352800,		.freq_val = 0xB06666},
	{ .rate =  384000,		.freq_val = 0xC00000},
};

static const struct src_freq src_freq_palette_26m_96_cycles[] = {
	{ .rate =  8000,		.freq_val = 0x035555},
	{ .rate =  11025,		.freq_val = 0x049800},
	{ .rate =  12000,		.freq_val = 0x050000},
	{ .rate =  16000,		.freq_val = 0x06AAAA},
	{ .rate =  22050,		.freq_val = 0x093000},
	{ .rate =  24000,		.freq_val = 0x0A0000},
	{ .rate =  32000,		.freq_val = 0x0D5555},
	{ .rate =  44100,		.freq_val = 0x126000},
	{ .rate =  48000,		.freq_val = 0x140000},
	{ .rate =  88200,		.freq_val = 0x24C000},
	{ .rate =  96000,		.freq_val = 0x280000},
	{ .rate =  176400,		.freq_val = 0x498000},
	{ .rate =  192000,		.freq_val = 0x500000},
	{ .rate =  352800,		.freq_val = 0x930000},
	{ .rate =  384000,		.freq_val = 0xA00000},
};

static const struct src_freq src_freq_palette_26m_441_cycles[] = {
	{ .rate =  8000,		.freq_val = 0x00B9C2},
	{ .rate =  11025,		.freq_val = 0x010000},
	{ .rate =  12000,		.freq_val = 0x0116A3},
	{ .rate =  16000,		.freq_val = 0x017384},
	{ .rate =  22050,		.freq_val = 0x020000},
	{ .rate =  24000,		.freq_val = 0x022D47},
	{ .rate =  32000,		.freq_val = 0x02E709},
	{ .rate =  44100,		.freq_val = 0x040000},
	{ .rate =  48000,		.freq_val = 0x045A8E},
	{ .rate =  88200,		.freq_val = 0x080000},
	{ .rate =  96000,		.freq_val = 0x08B51D},
	{ .rate =  176400,		.freq_val = 0x100000},
	{ .rate =  192000,		.freq_val = 0x116A3B},
	{ .rate =  352800,		.freq_val = 0x200000},
	{ .rate =  384000,		.freq_val = 0x22D476},
};

static const struct src_freq src_period_palette_49m_64_cycles[] = {
	{ .rate =  8000,		.freq_val = 0x060000},
	{ .rate =  11025,		.freq_val = 0x045A8F},
	{ .rate =  12000,		.freq_val = 0x040000},
	{ .rate =  16000,		.freq_val = 0x030000},
	{ .rate =  22050,		.freq_val = 0x022D47},
	{ .rate =  24000,		.freq_val = 0x020000},
	{ .rate =  32000,		.freq_val = 0x018000},
	{ .rate =  44100,		.freq_val = 0x0116A4},
	{ .rate =  48000,		.freq_val = 0x010000},
	{ .rate =  88200,		.freq_val = 0x008B52},
	{ .rate =  96000,		.freq_val = 0x008000},
	{ .rate =  176400,		.freq_val = 0x0045A9},
	{ .rate =  192000,		.freq_val = 0x004000},
	{ .rate =  352800,		.freq_val = 0x0022D4},
	{ .rate =  384000,		.freq_val = 0x002000},
};

static const struct src_freq src_period_palette_45m_64_cycles[] = {
	{ .rate =  8000,		.freq_val = 0x058332},
	{ .rate =  11025,		.freq_val = 0x040000},
	{ .rate =  12000,		.freq_val = 0x03accc},
	{ .rate =  16000,		.freq_val = 0x02c199},
	{ .rate =  22050,		.freq_val = 0x020000},
	{ .rate =  24000,		.freq_val = 0x01d666},
	{ .rate =  32000,		.freq_val = 0x0160cc},
	{ .rate =  44100,		.freq_val = 0x010000},
	{ .rate =  48000,		.freq_val = 0x00eb33},
	{ .rate =  88200,		.freq_val = 0x008000},
	{ .rate =  96000,		.freq_val = 0x007599},
	{ .rate =  176400,		.freq_val = 0x004000},
	{ .rate =  192000,		.freq_val = 0x003acd},
	{ .rate =  352800,		.freq_val = 0x002000},
	{ .rate =  384000,		.freq_val = 0x001d66},
};

static const struct src_freq src_period_palette_26m_48_cycles[] = {
	{ .rate =  8000,		.freq_val = 0x026160},
	{ .rate =  11025,		.freq_val = 0x01BA2D},
	{ .rate =  12000,		.freq_val = 0x019640},
	{ .rate =  16000,		.freq_val = 0x0130B0},
	{ .rate =  22050,		.freq_val = 0x00DD17},
	{ .rate =  24000,		.freq_val = 0x00CB20},
	{ .rate =  32000,		.freq_val = 0x009858},
	{ .rate =  44100,		.freq_val = 0x006E8B},
	{ .rate =  48000,		.freq_val = 0x006590},
	{ .rate =  88200,		.freq_val = 0x003746},
	{ .rate =  96000,		.freq_val = 0x0032C8},
	{ .rate =  176400,		.freq_val = 0x001BA3},
	{ .rate =  192000,		.freq_val = 0x001964},
	{ .rate =  352800,		.freq_val = 0x000DD1},
	{ .rate =  384000,		.freq_val = 0x000CB2},
};

static const struct src_freq src_period_palette_26m_96_cycles[] = {
	{ .rate =  8000,		.freq_val = 0x04C2C0},
	{ .rate =  11025,		.freq_val = 0x03745B},
	{ .rate =  12000,		.freq_val = 0x032C80},
	{ .rate =  16000,		.freq_val = 0x026160},
	{ .rate =  22050,		.freq_val = 0x01BA2D},
	{ .rate =  24000,		.freq_val = 0x019640},
	{ .rate =  32000,		.freq_val = 0x0130B0},
	{ .rate =  44100,		.freq_val = 0x00DD17},
	{ .rate =  48000,		.freq_val = 0x00CB20},
	{ .rate =  88200,		.freq_val = 0x006E8B},
	{ .rate =  96000,		.freq_val = 0x006590},
	{ .rate =  176400,		.freq_val = 0x003746},
	{ .rate =  192000,		.freq_val = 0x0032C8},
	{ .rate =  352800,		.freq_val = 0x001BA3},
	{ .rate =  384000,		.freq_val = 0x001964},
};

static const struct src_freq src_period_palette_26m_441_cycles[] = {
	{ .rate =  8000,		.freq_val = 0x15DEA2},
	{ .rate =  11025,		.freq_val = 0x0FDE80},
	{ .rate =  12000,		.freq_val = 0x0E946C},
	{ .rate =  16000,		.freq_val = 0x0AEF51},
	{ .rate =  22050,		.freq_val = 0x07EF40},
	{ .rate =  24000,		.freq_val = 0x074A36},
	{ .rate =  32000,		.freq_val = 0x0577A8},
	{ .rate =  44100,		.freq_val = 0x03F7A0},
	{ .rate =  48000,		.freq_val = 0x03A51B},
	{ .rate =  88200,		.freq_val = 0x01FBD0},
	{ .rate =  96000,		.freq_val = 0x01D28D},
	{ .rate =  176400,		.freq_val = 0x00FDE8},
	{ .rate =  192000,		.freq_val = 0x00E947},
	{ .rate =  352800,		.freq_val = 0x007EF4},
	{ .rate =  384000,		.freq_val = 0x0074A3},
};

static const struct src_freq src_autorst_high_49m_64_cycles[] = {
	{ .rate =  8000,		.freq_val = 0x066000},
	{ .rate =  11025,		.freq_val = 0x04a000},
	{ .rate =  12000,		.freq_val = 0x044000},
	{ .rate =  16000,		.freq_val = 0x033000},
	{ .rate =  22050,		.freq_val = 0x025000},
	{ .rate =  24000,		.freq_val = 0x022000},
	{ .rate =  32000,		.freq_val = 0x01a000},
	{ .rate =  44100,		.freq_val = 0x012800},
	{ .rate =  48000,		.freq_val = 0x011000},
	{ .rate =  88200,		.freq_val = 0x009400},
	{ .rate =  96000,		.freq_val = 0x008800},
	{ .rate =  176400,		.freq_val = 0x004a00},
	{ .rate =  192000,		.freq_val = 0x004400},
	{ .rate =  352800,		.freq_val = 0x002500},
	{ .rate =  384000,		.freq_val = 0x002200},
};

static const struct src_freq src_autorst_low_49m_64_cycles[] = {
	{ .rate =  8000,		.freq_val = 0x05a000},
	{ .rate =  11025,		.freq_val = 0x042000},
	{ .rate =  12000,		.freq_val = 0x03c000},
	{ .rate =  16000,		.freq_val = 0x02d000},
	{ .rate =  22050,		.freq_val = 0x021000},
	{ .rate =  24000,		.freq_val = 0x01e000},
	{ .rate =  32000,		.freq_val = 0x016000},
	{ .rate =  44100,		.freq_val = 0x011000},
	{ .rate =  48000,		.freq_val = 0x00f000},
	{ .rate =  88200,		.freq_val = 0x008300},
	{ .rate =  96000,		.freq_val = 0x007800},
	{ .rate =  176400,		.freq_val = 0x004100},
	{ .rate =  192000,		.freq_val = 0x003c00},
	{ .rate =  352800,		.freq_val = 0x002100},
	{ .rate =  384000,		.freq_val = 0x001e00},
};

static const struct src_freq src_autorst_high_45m_64_cycles[] = {
	{ .rate =  8000,		.freq_val = 0x05d000},
	{ .rate =  11025,		.freq_val = 0x044000},
	{ .rate =  12000,		.freq_val = 0x03e000},
	{ .rate =  16000,		.freq_val = 0x02e000},
	{ .rate =  22050,		.freq_val = 0x022000},
	{ .rate =  24000,		.freq_val = 0x01f000},
	{ .rate =  32000,		.freq_val = 0x017000},
	{ .rate =  44100,		.freq_val = 0x011000},
	{ .rate =  48000,		.freq_val = 0x00fa00},
	{ .rate =  88200,		.freq_val = 0x008800},
	{ .rate =  96000,		.freq_val = 0x007c00},
	{ .rate =  176400,		.freq_val = 0x004400},
	{ .rate =  192000,		.freq_val = 0x003e00},
	{ .rate =  352800,		.freq_val = 0x002200},
	{ .rate =  384000,		.freq_val = 0x001f00},
};

static const struct src_freq src_autorst_low_45m_64_cycles[] = {
	{ .rate =  8000,		.freq_val = 0x053000},
	{ .rate =  11025,		.freq_val = 0x03c000},
	{ .rate =  12000,		.freq_val = 0x038000},
	{ .rate =  16000,		.freq_val = 0x02a000},
	{ .rate =  22050,		.freq_val = 0x01e000},
	{ .rate =  24000,		.freq_val = 0x01c000},
	{ .rate =  32000,		.freq_val = 0x015000},
	{ .rate =  44100,		.freq_val = 0x00f000},
	{ .rate =  48000,		.freq_val = 0x00dc00},
	{ .rate =  88200,		.freq_val = 0x007800},
	{ .rate =  96000,		.freq_val = 0x006e00},
	{ .rate =  176400,		.freq_val = 0x003c00},
	{ .rate =  192000,		.freq_val = 0x003700},
	{ .rate =  352800,		.freq_val = 0x001e00},
	{ .rate =  384000,		.freq_val = 0x001c00},
};

static const struct src_freq src_autorst_high_26m_48_cycles[] = {
	{ .rate =  8000,		.freq_val = 0x028800},
	{ .rate =  11025,		.freq_val = 0x01D800},
	{ .rate =  12000,		.freq_val = 0x01B000},
	{ .rate =  16000,		.freq_val = 0x014000},
	{ .rate =  22050,		.freq_val = 0x00E800},
	{ .rate =  24000,		.freq_val = 0x00D800},
	{ .rate =  32000,		.freq_val = 0x00A000},
	{ .rate =  44100,		.freq_val = 0x007580},
	{ .rate =  48000,		.freq_val = 0x006B80},
	{ .rate =  88200,		.freq_val = 0x003A80},
	{ .rate =  96000,		.freq_val = 0x003600},
	{ .rate =  176400,		.freq_val = 0x001D80},
	{ .rate =  192000,		.freq_val = 0x001B00},
	{ .rate =  352800,		.freq_val = 0x000E80},
	{ .rate =  384000,		.freq_val = 0x000D80},
};

static const struct src_freq src_autorst_low_26m_48_cycles[] = {
	{ .rate =  8000,		.freq_val = 0x023800},
	{ .rate =  11025,		.freq_val = 0x01A000},
	{ .rate =  12000,		.freq_val = 0x018000},
	{ .rate =  16000,		.freq_val = 0x012000},
	{ .rate =  22050,		.freq_val = 0x00D000},
	{ .rate =  24000,		.freq_val = 0x00C000},
	{ .rate =  32000,		.freq_val = 0x009000},
	{ .rate =  44100,		.freq_val = 0x006780},
	{ .rate =  48000,		.freq_val = 0x005F00},
	{ .rate =  88200,		.freq_val = 0x003400},
	{ .rate =  96000,		.freq_val = 0x002F80},
	{ .rate =  176400,		.freq_val = 0x001A00},
	{ .rate =  192000,		.freq_val = 0x001800},
	{ .rate =  352800,		.freq_val = 0x000D00},
	{ .rate =  384000,		.freq_val = 0x000C00},
};

static const struct src_freq src_autorst_high_26m_96_cycles[] = {
	{ .rate =  8000,		.freq_val = 0x051000},
	{ .rate =  11025,		.freq_val = 0x03B000},
	{ .rate =  12000,		.freq_val = 0x036000},
	{ .rate =  16000,		.freq_val = 0x028000},
	{ .rate =  22050,		.freq_val = 0x01D000},
	{ .rate =  24000,		.freq_val = 0x01B000},
	{ .rate =  32000,		.freq_val = 0x014000},
	{ .rate =  44100,		.freq_val = 0x00EB00},
	{ .rate =  48000,		.freq_val = 0x00D700},
	{ .rate =  88200,		.freq_val = 0x007500},
	{ .rate =  96000,		.freq_val = 0x006C00},
	{ .rate =  176400,		.freq_val = 0x003B00},
	{ .rate =  192000,		.freq_val = 0x003600},
	{ .rate =  352800,		.freq_val = 0x001D00},
	{ .rate =  384000,		.freq_val = 0x001B00},
};

static const struct src_freq src_autorst_low_26m_96_cycles[] = {
	{ .rate =  8000,		.freq_val = 0x047000},
	{ .rate =  11025,		.freq_val = 0x034000},
	{ .rate =  12000,		.freq_val = 0x030000},
	{ .rate =  16000,		.freq_val = 0x024000},
	{ .rate =  22050,		.freq_val = 0x01A000},
	{ .rate =  24000,		.freq_val = 0x018000},
	{ .rate =  32000,		.freq_val = 0x012000},
	{ .rate =  44100,		.freq_val = 0x00CF00},
	{ .rate =  48000,		.freq_val = 0x00BE00},
	{ .rate =  88200,		.freq_val = 0x006800},
	{ .rate =  96000,		.freq_val = 0x005F00},
	{ .rate =  176400,		.freq_val = 0x003400},
	{ .rate =  192000,		.freq_val = 0x003000},
	{ .rate =  352800,		.freq_val = 0x001A00},
	{ .rate =  384000,		.freq_val = 0x001800},
};

static const struct src_freq src_autorst_high_26m_441_cycles[] = {
	{ .rate =  8000,		.freq_val = 0x170000},
	{ .rate =  11025,		.freq_val = 0x110000},
	{ .rate =  12000,		.freq_val = 0x0F8000},
	{ .rate =  16000,		.freq_val = 0x0BA000},
	{ .rate =  22050,		.freq_val = 0x087000},
	{ .rate =  24000,		.freq_val = 0x07C000},
	{ .rate =  32000,		.freq_val = 0x05D000},
	{ .rate =  44100,		.freq_val = 0x044000},
	{ .rate =  48000,		.freq_val = 0x03F000},
	{ .rate =  88200,		.freq_val = 0x022000},
	{ .rate =  96000,		.freq_val = 0x01F000},
	{ .rate =  176400,		.freq_val = 0x011000},
	{ .rate =  192000,		.freq_val = 0x00F800},
	{ .rate =  352800,		.freq_val = 0x008700},
	{ .rate =  384000,		.freq_val = 0x007C00},
};

static const struct src_freq src_autorst_low_26m_441_cycles[] = {
	{ .rate =  8000,		.freq_val = 0x150000},
	{ .rate =  11025,		.freq_val = 0x0EE000},
	{ .rate =  12000,		.freq_val = 0x0DB000},
	{ .rate =  16000,		.freq_val = 0x0A4000},
	{ .rate =  22050,		.freq_val = 0x077000},
	{ .rate =  24000,		.freq_val = 0x06D000},
	{ .rate =  32000,		.freq_val = 0x052000},
	{ .rate =  44100,		.freq_val = 0x03C000},
	{ .rate =  48000,		.freq_val = 0x037000},
	{ .rate =  88200,		.freq_val = 0x01E000},
	{ .rate =  96000,		.freq_val = 0x01B000},
	{ .rate =  176400,		.freq_val = 0x00EE00},
	{ .rate =  192000,		.freq_val = 0x00DB00},
	{ .rate =  352800,		.freq_val = 0x007700},
	{ .rate =  384000,		.freq_val = 0x006D00},
};

struct snd_soc_dai *get_be_cpu_dai(struct snd_soc_dapm_widget *w, int stream)
{
	bool playback = (stream == SNDRV_PCM_STREAM_PLAYBACK) ? true : false;
	struct snd_soc_dapm_path *path = NULL;
	struct snd_soc_dai *dai = NULL;
	struct snd_soc_dai *ret = NULL;

	if (!w)
		return NULL;

	if (playback) {
		snd_soc_dapm_widget_for_each_sink_path(w, path) {
			struct snd_soc_dapm_widget *sink;

			if (path && path->sink) {
				if (!path->connect)
					continue;
				sink = path->sink;
				dai = sink->priv;
				if (dai && sink->id == snd_soc_dapm_dai_in)
					return dai;
				ret = get_be_cpu_dai(path->sink, stream);
				if (ret)
					return ret;
			}
		}
	} else {
		snd_soc_dapm_widget_for_each_source_path(w, path) {
			struct snd_soc_dapm_widget *source;

			if (path && path->source) {
				if (!path->connect)
					continue;

				source = path->source;
				dai = source->priv;
				if (dai && source->id == snd_soc_dapm_dai_out)
					return dai;
				ret = get_be_cpu_dai(source, stream);
				if (ret)
					return ret;
			}
		}
	}
	return NULL;
}

static int mtk_get_src_freq_mode_val(struct mtk_base_afe *afe,
			    int id, int rate)
{
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_src_priv *src_priv = afe_priv->dai_priv[id];
	int cali_clk = src_priv->cali_clk;
	int cali_cycle = src_priv->cali_cycle;
	const struct src_freq *freq_palette = NULL;
	int tbl_size = 0;

	if (cali_clk == CALI_CLK_26M) {
		if (cali_cycle == 48) {
			freq_palette = src_freq_palette_26m_48_cycles;
			tbl_size = ARRAY_SIZE(src_freq_palette_26m_48_cycles);
		} else if (cali_cycle == 96) {
			freq_palette = src_freq_palette_26m_96_cycles;
			tbl_size = ARRAY_SIZE(src_freq_palette_26m_96_cycles);
		} else if (cali_cycle == 441) {
			freq_palette = src_freq_palette_26m_441_cycles;
			tbl_size = ARRAY_SIZE(src_freq_palette_26m_441_cycles);
		}
	} else if (cali_clk == CALI_CLK_49M || cali_clk == CALI_CLK_45M) {
		if (cali_cycle == 64) {
			freq_palette = src_freq_palette_49m_45m_64_cycles;
			tbl_size = ARRAY_SIZE(src_freq_palette_49m_45m_64_cycles);
		}
	} else {
		freq_palette = src_palette_no_cali;
		tbl_size = ARRAY_SIZE(src_palette_no_cali);
	}
	if (!freq_palette) {
		dev_info(afe->dev, "%s(), cannot find freq_palette, cali_clk %d, rate %d, cali_cycle %d\n",
				__func__, cali_clk, rate, cali_cycle);
		return 0;
	}

	switch (rate) {
	case 8000:
		return freq_palette[0].freq_val;
	case 11025:
		return freq_palette[1].freq_val;
	case 12000:
		return freq_palette[2].freq_val;
	case 16000:
		return freq_palette[3].freq_val;
	case 22050:
		return freq_palette[4].freq_val;
	case 24000:
		return freq_palette[5].freq_val;
	case 32000:
		return freq_palette[6].freq_val;
	case 44100:
		return freq_palette[7].freq_val;
	case 48000:
		return freq_palette[8].freq_val;
	case 88200:
		return freq_palette[9].freq_val;
	case 96000:
		return freq_palette[10].freq_val;
	case 176400:
		return freq_palette[11].freq_val;
	case 192000:
		return freq_palette[12].freq_val;
	case 352800:
		return freq_palette[13].freq_val;
	case 384000:
		return freq_palette[14].freq_val;
	default:
		dev_info(afe->dev, "%s(), rate %d invalid!!!\n",
			 __func__, rate);
		AUDIO_AEE("rate invalid");
		return 0;
	}
}

static int mtk_get_src_period_mode_val(struct mtk_base_afe *afe,
			    int id, int rate)
{
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_src_priv *src_priv = afe_priv->dai_priv[id];
	int cali_clk = src_priv->cali_clk;
	int cali_cycle = src_priv->cali_cycle;
	const struct src_freq *period_palette = NULL;
	int tbl_size = 0;

	if  (cali_clk == CALI_CLK_45M) {
		if (cali_cycle == 64) {
			period_palette = src_period_palette_45m_64_cycles;
			tbl_size = ARRAY_SIZE(src_period_palette_45m_64_cycles);
		}
	} else if (cali_clk == CALI_CLK_49M) {
		if (cali_cycle == 64) {
			period_palette = src_period_palette_49m_64_cycles;
			tbl_size = ARRAY_SIZE(src_period_palette_49m_64_cycles);
		}
	} else if (cali_clk == CALI_CLK_26M) {
		if (cali_cycle == 48) {
			period_palette = src_period_palette_26m_48_cycles;
			tbl_size = ARRAY_SIZE(src_period_palette_26m_48_cycles);
		} else if (cali_cycle == 96) {
			period_palette = src_period_palette_26m_96_cycles;
			tbl_size = ARRAY_SIZE(src_period_palette_26m_96_cycles);
		} else if (cali_cycle == 441) {
			period_palette = src_period_palette_26m_441_cycles;
			tbl_size = ARRAY_SIZE(src_period_palette_26m_441_cycles);
		}
	}

	if (!period_palette) {
		dev_info(afe->dev, "%s, error: cannot find period_palette! cali_clk=%u, cali_cycle=%u\n",
		       __func__, cali_clk, cali_cycle);
		return 0;
	}

	switch (rate) {
	case 8000:
		return period_palette[0].freq_val;
	case 11025:
		return period_palette[1].freq_val;
	case 12000:
		return period_palette[2].freq_val;
	case 16000:
		return period_palette[3].freq_val;
	case 22050:
		return period_palette[4].freq_val;
	case 24000:
		return period_palette[5].freq_val;
	case 32000:
		return period_palette[6].freq_val;
	case 44100:
		return period_palette[7].freq_val;
	case 48000:
		return period_palette[8].freq_val;
	case 88200:
		return period_palette[9].freq_val;
	case 96000:
		return period_palette[10].freq_val;
	case 176400:
		return period_palette[11].freq_val;
	case 192000:
		return period_palette[12].freq_val;
	case 352800:
		return period_palette[13].freq_val;
	case 384000:
		return period_palette[14].freq_val;
	default:
		dev_info(afe->dev, "%s(), rate %d invalid!!!\n",
			 __func__, rate);
		AUDIO_AEE("rate invalid");
		return 0;
	}
}

const unsigned int *get_iir_coeff(unsigned int rate_in,
				  unsigned int rate_out,
				  unsigned int *param_num)
{
	if ((rate_in == 32000 && rate_out == 16000) ||
		(rate_in == 96000 && rate_out == 48000)) {
		*param_num = ARRAY_SIZE(src_iir_coeff_32_to_16);
		return src_iir_coeff_32_to_16;
	} else if (rate_in == 44100 && rate_out == 16000) {
		*param_num = ARRAY_SIZE(src_iir_coeff_44_to_16);
		return src_iir_coeff_44_to_16;
	} else if (rate_in == 44100 && rate_out == 32000) {
		*param_num = ARRAY_SIZE(src_iir_coeff_44_to_32);
		return src_iir_coeff_44_to_32;
	} else if ((rate_in == 48000 && rate_out == 16000) ||
		   (rate_in == 96000 && rate_out == 32000)) {
		*param_num = ARRAY_SIZE(src_iir_coeff_48_to_16);
		return src_iir_coeff_48_to_16;
	} else if (rate_in == 48000 && rate_out == 32000) {
		*param_num = ARRAY_SIZE(src_iir_coeff_48_to_32);
		return src_iir_coeff_48_to_32;
	} else if (rate_in == 48000 && rate_out == 44100) {
		*param_num = ARRAY_SIZE(src_iir_coeff_48_to_44);
		return src_iir_coeff_48_to_44;
	} else if (rate_in == 96000 && rate_out == 16000) {
		*param_num = ARRAY_SIZE(src_iir_coeff_96_to_16);
		return src_iir_coeff_96_to_16;
	} else if ((rate_in == 96000 && rate_out == 44100) ||
		   (rate_in == 48000 && rate_out == 22050)) {
		*param_num = ARRAY_SIZE(src_iir_coeff_96_to_44);
		return src_iir_coeff_96_to_44;
	}

	*param_num = 0;
	return NULL;
}

static bool mtk_src_need_tracking(struct snd_pcm_substream *substream,
				  struct mtk_base_afe *afe, int id)
{
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_src_priv *src_priv = afe_priv->dai_priv[id];
	int lrck_source = src_priv->lrck_source;
	int cali_clk = src_priv->cali_clk;

	/* if lrck_source is no slave domain, don't need tracking */
	if (lrck_source == NO_SLAVE_DOMAIN) {
		src_priv->cali_rx = 0;
		src_priv->cali_tx = 0;
		return false;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (cali_clk == CALI_CLK_26M)
			src_priv->cali_tx = 1;
		else
			src_priv->cali_rx = 1;
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (cali_clk == CALI_CLK_26M)
			src_priv->cali_rx = 1;
		else
			src_priv->cali_tx = 1;
	}

	return true;
}

static int mtk_set_src_cali_domain_fs(struct mtk_base_afe *afe,
			    int id, int rate)
{
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_src_priv *src_priv = afe_priv->dai_priv[id];
	struct snd_pcm_substream *substream = src_priv->substream;
	unsigned int reg = 0;
	unsigned int in_fs = 0, out_fs = 0;
	unsigned int in_domain = 0, out_domain = 0;
	int idx = id - MT6881_DAI_SRC_0;
	unsigned int rate_reg = mt6881_rate_transform(afe->dev, rate, id);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (src_priv->cali_rx) {
			in_domain = src_domain_info[0].in_sel_domain;
			in_fs = src_domain_info[0].in_sel_fs;
			out_domain = src_domain_info[0].out_sel_domain;
			out_fs = rate_reg;
		} else if (src_priv->cali_tx) {
			in_domain = src_domain_info[3].in_sel_domain;
			in_fs = rate_reg;
			out_domain = src_domain_info[3].out_sel_domain;
			out_fs = src_domain_info[3].out_sel_fs;
		}
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (src_priv->cali_tx) {
			in_domain = src_domain_info[1].in_sel_domain;
			in_fs = rate_reg;
			out_domain = src_domain_info[1].out_sel_domain;
			out_fs = src_domain_info[1].out_sel_fs;
		} else if (src_priv->cali_rx) {
			in_domain = src_domain_info[2].in_sel_domain;
			in_fs = src_domain_info[2].in_sel_fs;
			out_domain = src_domain_info[2].out_sel_domain;
			out_fs = rate_reg;
		}
	}

	reg = src_reg[idx].con5;
	/* input domain */
	regmap_update_bits(afe->regmap,
				reg,
				IN_EN_SEL_DOMAIN_MASK_SFT,
				in_domain << IN_EN_SEL_DOMAIN_SFT);
	/* input fs */
	regmap_update_bits(afe->regmap,
				reg,
				IN_EN_SEL_FS_MASK_SFT,
				in_fs << IN_EN_SEL_FS_SFT);
	/* out domain */
	regmap_update_bits(afe->regmap,
				reg,
				OUT_EN_SEL_DOMAIN_MASK_SFT,
				out_domain << OUT_EN_SEL_DOMAIN_SFT);
	/* output fs */
	regmap_update_bits(afe->regmap,
				reg,
				OUT_EN_SEL_FS_MASK_SFT,
				out_fs << OUT_EN_SEL_FS_SFT);

	return 0;
}

static int mtk_set_src_autorst_threshold(struct mtk_base_afe *afe,
			    int id, int rate)
{
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_src_priv *src_priv = afe_priv->dai_priv[id];
	const struct src_freq *autorst_high_table = NULL;
	const struct src_freq *autorst_low_table = NULL;
	int tbl_size_high = 0, tbl_size_low = 0;
	int autorst_high = 0, autorst_low = 0;
	int idx = id - MT6881_DAI_SRC_0;
	int cali_clk = src_priv->cali_clk;
	int cali_cycle = src_priv->cali_cycle;

	if (cali_clk == CALI_CLK_26M) {
		if (cali_cycle == 48) {
			autorst_high_table = src_autorst_high_26m_48_cycles;
			tbl_size_high = ARRAY_SIZE(src_autorst_high_26m_48_cycles);
			autorst_low_table = src_autorst_low_26m_48_cycles;
			tbl_size_low = ARRAY_SIZE(src_autorst_low_26m_48_cycles);
		} else if (cali_cycle == 96) {
			autorst_high_table = src_autorst_high_26m_96_cycles;
			tbl_size_high = ARRAY_SIZE(src_autorst_high_26m_96_cycles);
			autorst_low_table = src_autorst_low_26m_96_cycles;
			tbl_size_low = ARRAY_SIZE(src_autorst_low_26m_96_cycles);
		} else if (cali_cycle == 441) {
			autorst_high_table = src_autorst_high_26m_441_cycles;
			tbl_size_high = ARRAY_SIZE(src_autorst_high_26m_441_cycles);
			autorst_low_table = src_autorst_low_26m_441_cycles;
			tbl_size_low = ARRAY_SIZE(src_autorst_low_26m_441_cycles);
		}
	} else if (cali_clk == CALI_CLK_49M) {
		if (cali_cycle == 64) {
			autorst_high_table = src_autorst_high_49m_64_cycles;
			tbl_size_high = ARRAY_SIZE(src_autorst_high_49m_64_cycles);
			autorst_low_table = src_autorst_low_49m_64_cycles;
			tbl_size_low = ARRAY_SIZE(src_autorst_low_49m_64_cycles);
		}
	} else if (cali_clk == CALI_CLK_45M) {
		if (cali_cycle == 64) {
			autorst_high_table = src_autorst_high_45m_64_cycles;
			tbl_size_high = ARRAY_SIZE(src_autorst_high_45m_64_cycles);
			autorst_low_table = src_autorst_low_45m_64_cycles;
			tbl_size_low = ARRAY_SIZE(src_autorst_low_45m_64_cycles);
		}
	}

	if (!autorst_high_table || !autorst_low_table) {
		dev_info(afe->dev, "%s(), cannot find threshold table, cali_clk %d, cali_cycle %d\n",
				__func__, cali_clk, cali_cycle);
		return -EINVAL;
	}

	switch (rate) {
	case 8000:
		autorst_high = autorst_high_table[0].freq_val;
		autorst_low = autorst_low_table[0].freq_val;
		break;
	case 11025:
		autorst_high = autorst_high_table[1].freq_val;
		autorst_low = autorst_low_table[1].freq_val;
		break;
	case 12000:
		autorst_high = autorst_high_table[2].freq_val;
		autorst_low = autorst_low_table[2].freq_val;
		break;
	case 16000:
		autorst_high = autorst_high_table[3].freq_val;
		autorst_low = autorst_low_table[3].freq_val;
		break;
	case 22050:
		autorst_high = autorst_high_table[4].freq_val;
		autorst_low = autorst_low_table[4].freq_val;
		break;
	case 24000:
		autorst_high = autorst_high_table[5].freq_val;
		autorst_low = autorst_low_table[5].freq_val;
		break;
	case 32000:
		autorst_high = autorst_high_table[6].freq_val;
		autorst_low = autorst_low_table[6].freq_val;
		break;
	case 44100:
		autorst_high = autorst_high_table[7].freq_val;
		autorst_low = autorst_low_table[7].freq_val;
		break;
	case 48000:
		autorst_high = autorst_high_table[8].freq_val;
		autorst_low = autorst_low_table[8].freq_val;
		break;
	case 88200:
		autorst_high = autorst_high_table[9].freq_val;
		autorst_low = autorst_low_table[9].freq_val;
		break;
	case 96000:
		autorst_high = autorst_high_table[10].freq_val;
		autorst_low = autorst_low_table[10].freq_val;
		break;
	case 176400:
		autorst_high = autorst_high_table[11].freq_val;
		autorst_low = autorst_low_table[11].freq_val;
		break;
	case 192000:
		autorst_high = autorst_high_table[12].freq_val;
		autorst_low = autorst_low_table[12].freq_val;
		break;
	case 352800:
		autorst_high = autorst_high_table[13].freq_val;
		autorst_low = autorst_low_table[13].freq_val;
		break;
	case 384000:
		autorst_high = autorst_high_table[14].freq_val;
		autorst_low = autorst_low_table[14].freq_val;
		break;
	default:
		dev_info(afe->dev, "%s(), rate %d invalid!!!\n",
			 __func__, rate);
		AUDIO_AEE("rate invalid");
		return -EINVAL;
	}

	regmap_write(afe->regmap, src_reg[idx].con13, autorst_high);
	regmap_write(afe->regmap, src_reg[idx].con14, autorst_low);

	return 0;
}

static int mtk_set_src_denominator(struct mtk_base_afe *afe, int id)
{
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_src_priv *src_priv = afe_priv->dai_priv[id];
	int idx = id - MT6881_DAI_SRC_0;
	int cali_clk = src_priv->cali_clk;
	int cali_cycle = src_priv->cali_cycle;
	unsigned int reg = 0, val = 0;

	if (cali_clk == CALI_CLK_26M) {
		if (cali_cycle == 48) {
			val = SRC_FREQ_CALC_DENOMINATOR_26M_48;
		} else if (cali_cycle == 96) {
			val = SRC_FREQ_CALC_DENOMINATOR_26M_96;
		} else if (cali_cycle == 441) {
			val = SRC_FREQ_CALC_DENOMINATOR_26M_441;
		} else {
			dev_info(afe->dev, "%s() unknown cali_cycle %d for 26M\n",
					__func__, cali_cycle);
			return -EINVAL;
		}
	} else {
		if (cali_clk == CALI_CLK_45M && cali_cycle == 64) {
			val = SRC_FREQ_CALC_DENOMINATOR_45M;
		} else if (cali_clk == CALI_CLK_49M && cali_cycle == 64) {
			val = SRC_FREQ_CALC_DENOMINATOR_49M;
		} else {
			dev_info(afe->dev, "%s() unknown cali_clk %d, cali_cycle %d\n",
					__func__, cali_clk, cali_cycle);
			return -EINVAL;
		}
	}

	reg = src_reg[idx].con7;
	regmap_write(afe->regmap, reg, val);

	return 0;
}

static int mtk_set_src_cali_signal(struct mtk_base_afe *afe, int id)
{
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_src_priv *src_priv = afe_priv->dai_priv[id];
	int idx = id - MT6881_DAI_SRC_0;
	int lrck_source = src_priv->lrck_source;

	regmap_update_bits(afe->regmap, src_reg[idx].con5,
			CALI_LRCK_SEL_MASK_SFT,
			(lrck_source % 8) << CALI_LRCK_SEL_SFT);
	regmap_update_bits(afe->regmap, src_reg[idx].con6,
			FREQ_CALI_SEL_MASK_SFT,
			(lrck_source / 8) << FREQ_CALI_SEL_SFT);

	return 0;
}

static int mtk_set_src_autoupdate(struct mtk_base_afe *afe, int id)
{
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_src_priv *src_priv = afe_priv->dai_priv[id];
	unsigned int reg = 0, val = 0;
	int idx = id - MT6881_DAI_SRC_0;

	reg = src_reg[idx].con6;
	/* enable auto update by HW, rx AUTO_TUNE_FREQ2 */
	val = src_priv->cali_rx ? AUTO_TUNE_FREQ2 : 0x0;
	regmap_update_bits(afe->regmap,
				reg,
				AUTO_TUNE_FREQ2_MASK_SFT,
				val << AUTO_TUNE_FREQ2_SFT);
	/* enable auto update by HW, tx AUTO_TUNE_FREQ3 */
	val = src_priv->cali_rx ?  0x0 : AUTO_TUNE_FREQ3;
	regmap_update_bits(afe->regmap,
				reg,
				AUTO_TUNE_FREQ3_MASK_SFT,
				val << AUTO_TUNE_FREQ3_SFT);
	/* use freq/period cali result to update asm_freq_2 and asm_freq_3 */
	val = src_priv->cali_rx ? CALI_USE_FREQ_OUT : CALI_USE_PERIOD_OUT;
	regmap_update_bits(afe->regmap,
				reg,
				CALI_USE_FREQ_OUT_MASK_SFT,
				val << CALI_USE_FREQ_OUT_SFT);

	reg = src_reg[idx].con0;
	/* IFS_SEL, rx auto update by HW */
	val = src_priv->cali_rx ? IFS_ASM_FREQ_2 : IFS_ASM_FREQ_3;
	regmap_update_bits(afe->regmap, reg,
				CHSET0_IFS_SEL_MASK_SFT,
				val << CHSET0_IFS_SEL_SFT);
	/* OFS_SEL rx fiexd fs*/
	val = src_priv->cali_rx ? OFS_ASM_FREQ_1 : OFS_ASM_FREQ_0;
	regmap_update_bits(afe->regmap, reg,
				CHSET0_OFS_SEL_MASK_SFT,
				val << CHSET0_OFS_SEL_SFT);

	return 0;
}

static int mtk_set_src_one_heart_mode(struct mtk_base_afe *afe, int id)
{
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_src_priv *src_priv = afe_priv->dai_priv[id];
	int idx = id - MT6881_DAI_SRC_0;
	int one_heart_mode = src_priv->one_heart_mode;

	regmap_update_bits(afe->regmap, src_reg[idx].con0,
				ONE_HEART_MASK_SFT,
				one_heart_mode << ONE_HEART_SFT);

	return 0;
}

static int mtk_src_cali_enable(struct mtk_base_afe *afe, int id, int enable)
{
	unsigned int reg = 0;
	int idx = id - MT6881_DAI_SRC_0;

	dev_dbg(afe->dev, "%s() id %d, enable %d\n", __func__, idx, enable);

	reg = src_reg[idx].con6;
	regmap_update_bits(afe->regmap,
			   reg,
			   CALI_EN_MASK_SFT,
			   enable << CALI_EN_SFT);

	return 0;
}

static int mtk_src_enable(struct mtk_base_afe *afe, int id, int enable)
{
	unsigned int reg = 0;
	int idx = id - MT6881_DAI_SRC_0;

	dev_info(afe->dev, "%s() id %d, enable %d\n", __func__, idx, enable);

	reg = src_reg[idx].con0;
	/* ASM_ON */
	regmap_update_bits(afe->regmap, reg,
			   ASM_ON_MASK_SFT,
			   enable << ASM_ON_SFT);

	return 0;
}

static int mtk_src_reset(struct mtk_base_afe *afe, int id)
{
	int idx = id - MT6881_DAI_SRC_0;

	regmap_update_bits(afe->regmap, src_reg[idx].con5,
						SOFT_RESET_MASK_SFT,
						0x1 << SOFT_RESET_SFT);
	regmap_update_bits(afe->regmap, src_reg[idx].con5,
						SOFT_RESET_MASK_SFT,
						0x0 << SOFT_RESET_SFT);
	return 0;
}

static int mtk_src_clear(struct mtk_base_afe *afe, int id)
{
	int idx = id - MT6881_DAI_SRC_0;

	regmap_update_bits(afe->regmap, src_reg[idx].con0,
						CHSET_STR_CLR_MASK_SFT,
						0x1 << CHSET_STR_CLR_SFT);
	return 0;
}

//#define DEBUG_COEFF
static int mtk_set_src_param(struct mtk_base_afe *afe, int id)
{
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	struct snd_pcm_substream *substream;
	struct mtk_afe_src_priv *src_priv = afe_priv->dai_priv[id];
	int idx = id - MT6881_DAI_SRC_0;
	int cali_cycle = src_priv->cali_cycle;
	int cali_clk = src_priv->cali_clk;
	int lrck_source = src_priv->lrck_source;
	unsigned int iir_coeff_num;
	unsigned int iir_stage;
	unsigned int in_freq_mode = 0, out_freq_mode = 0;
	unsigned int in_period_mode = 0, out_period_mode = 0;
	unsigned int reg = 0, val = 0, mask = 0;
	int rate_in = src_priv->dl_rate;
	int rate_out = src_priv->ul_rate;

	dev_info(afe->dev, "%s() cali_cycle %d, cali_clk %d, lrck_source %d\n",
		__func__, cali_cycle, cali_clk, lrck_source);

	if (!src_priv->substream) {
		dev_info(afe->dev, "%s() substream NULL\n", __func__);
		return -EINVAL;
	}
	substream = src_priv->substream;

	regmap_write(afe->regmap, src_reg[idx].con0, 0x0);
	regmap_write(afe->regmap, src_reg[idx].con6, 0x0);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (src_priv->cali_rx) {
			/* input/output 1x_en source */
			mtk_set_src_cali_domain_fs(afe, id, rate_out);

			/* auto reset threshold */
			mtk_set_src_autorst_threshold(afe, id, rate_in);

			/* set RX IFS/OFS defalut */
			in_freq_mode = mtk_get_src_freq_mode_val(afe, id, rate_in);
			out_freq_mode = mtk_get_src_freq_mode_val(afe, id, rate_out);
			/* RX_OFS fixed */
			regmap_update_bits(afe->regmap, src_reg[idx].con2,
						ASM_FREQ_1_MASK_SFT,
						out_freq_mode << ASM_FREQ_1_SFT);
			/* RX_IFS update by HW */
			regmap_update_bits(afe->regmap, src_reg[idx].con3,
						ASM_FREQ_2_MASK_SFT,
						in_freq_mode << ASM_FREQ_2_SFT);
		} else if (src_priv->cali_tx) {
			/* input/output 1x_en source */
			mtk_set_src_cali_domain_fs(afe, id, rate_in);

			/* auto reset threshold */
			mtk_set_src_autorst_threshold(afe, id, rate_out);

			/* set TX IFS/OFS defalut */
			in_period_mode = mtk_get_src_period_mode_val(afe, id, rate_in);
			out_period_mode = mtk_get_src_period_mode_val(afe, id, rate_out);
			/* TX_OFS fixed */
			regmap_update_bits(afe->regmap, src_reg[idx].con1,
						ASM_FREQ_1_MASK_SFT,
						in_period_mode << ASM_FREQ_1_SFT);
			/* TX_IFS, update by HW */
			regmap_update_bits(afe->regmap, src_reg[idx].con4,
						ASM_FREQ_2_MASK_SFT,
						out_period_mode << ASM_FREQ_2_SFT);

		}
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (src_priv->cali_tx) {
			/* input/output 1x_en source */
			mtk_set_src_cali_domain_fs(afe, id, rate_in);

			/* auto reset threshold */
			mtk_set_src_autorst_threshold(afe, id, rate_out);

			/* set TX IFS/OFS defalut */
			in_period_mode = mtk_get_src_period_mode_val(afe, id, rate_in);
			out_period_mode = mtk_get_src_period_mode_val(afe, id, rate_out);
			/* TX_OFS fixed */
			regmap_update_bits(afe->regmap, src_reg[idx].con1,
						ASM_FREQ_1_MASK_SFT,
						in_period_mode << ASM_FREQ_1_SFT);
			/* TX_IFS, update by HW */
			regmap_update_bits(afe->regmap, src_reg[idx].con4,
						ASM_FREQ_2_MASK_SFT,
						out_period_mode << ASM_FREQ_2_SFT);
		} else if (src_priv->cali_rx) {
			/* input/output 1x_en source */
			mtk_set_src_cali_domain_fs(afe, id, rate_out);

			/* auto reset threshold */
			mtk_set_src_autorst_threshold(afe, id, rate_in);

			/* set RX IFS/OFS defalut */
			in_freq_mode = mtk_get_src_freq_mode_val(afe, id, rate_in);
			out_freq_mode = mtk_get_src_freq_mode_val(afe, id, rate_out);
			/* RX_OFS fixed */
			regmap_update_bits(afe->regmap, src_reg[idx].con2,
						ASM_FREQ_1_MASK_SFT,
						out_freq_mode << ASM_FREQ_1_SFT);
			/* RX_IFS update by HW */
			regmap_update_bits(afe->regmap, src_reg[idx].con3,
						ASM_FREQ_2_MASK_SFT,
						in_freq_mode << ASM_FREQ_2_SFT);
		}
	}

	dev_dbg(afe->dev, "%s, id %d, dir %d, cali_tx %d, cali_rx %d\n",
			__func__,
			idx,
			substream->stream,
			src_priv->cali_tx,
			src_priv->cali_rx);

	if (src_priv->cali_rx || src_priv->cali_tx) {
		/* cali ck */
		regmap_update_bits(afe->regmap, src_reg[idx].con5,
				CALI_CK_SEL_MASK_SFT,
				cali_clk << CALI_CK_SEL_SFT);

		/* cali signal */
		mtk_set_src_cali_signal(afe, id);

		/* denominator */
		mtk_set_src_denominator(afe, id);

		/* cali cycle */
		reg = src_reg[idx].con6;
		val = FREQ_CALI_CYCLE(cali_cycle) |
				FREQ_CALI_AUTORST_EN_MASK_SFT |
				COMP_FREQ_RES_EN_MASK_SFT |
				FREQ_CALI_BP_DGL_MASK_SFT |
				FREQ_CALI_AUTO_RESTART_MASK_SFT;
		mask = FREQ_CALI_CYCLE_MASK_SFT |
				FREQ_CALI_AUTORST_EN_MASK_SFT |
				COMP_FREQ_RES_EN_MASK_SFT |
				FREQ_CALI_BP_DGL_MASK_SFT |
				FREQ_CALI_AUTO_RESTART_MASK_SFT;
		regmap_update_bits(afe->regmap,
				   reg,
				   mask,
				   val);

		/* auto update */
		mtk_set_src_autoupdate(afe, id);

		/* one heart mode */
		mtk_set_src_one_heart_mode(afe, id);
	} else {/* no cali */
		in_freq_mode = mtk_get_src_freq_mode_val(afe, id, rate_in);
		out_freq_mode = mtk_get_src_freq_mode_val(afe, id, rate_out);
		/* RX_OFS fixed */
		regmap_update_bits(afe->regmap, src_reg[idx].con2,
						ASM_FREQ_1_MASK_SFT,
						out_freq_mode << ASM_FREQ_1_SFT);
		/* RX_IFS update by HW */
		regmap_update_bits(afe->regmap, src_reg[idx].con3,
						ASM_FREQ_2_MASK_SFT,
						in_freq_mode << ASM_FREQ_2_SFT);

		/* IFS_SEL, rx auto update by HW */
		regmap_update_bits(afe->regmap, src_reg[idx].con0,
				   CHSET0_IFS_SEL_MASK_SFT,
				   IFS_ASM_FREQ_2 << CHSET0_IFS_SEL_SFT);
		/* OFS_SEL rx fiexd fs*/
		regmap_update_bits(afe->regmap, src_reg[idx].con0,
				   CHSET0_OFS_SEL_MASK_SFT,
				   OFS_ASM_FREQ_1 << CHSET0_OFS_SEL_SFT);
	}

	/* set iir if in_rate > out_rate */
	if (rate_in > rate_out) {
		int i;
#ifdef DEBUG_COEFF
		int reg_val;
#endif
		const unsigned int *iir_coeff = get_iir_coeff(rate_in, rate_out,
						&iir_coeff_num);

		if (iir_coeff_num == 0 || !iir_coeff) {
			AUDIO_AEE("iir coeff error");
			return -EINVAL;
		}

		/* COEFF_SRAM_CTRL */
		regmap_update_bits(afe->regmap, src_reg[idx].con0,
				   COEFF_SRAM_CTRL_MASK_SFT,
				   0x1 << COEFF_SRAM_CTRL_SFT);
#ifdef DEBUG_COEFF
		regmap_read(afe->regmap, src_reg[idx].con0,
			    &reg_val);

		dev_info(afe->dev, "%s(), AFE_GASRC0_NEW_CON0 0x%x\n",
			 __func__, reg_val);
#endif
		/* Clear coeff history to r/w coeff from the first position */
		regmap_update_bits(afe->regmap, src_reg[idx].con11,
				   COEFF_SRAM_ADR_MASK_SFT,
				   0x0);
		/* Write SRC coeff, should not read the reg during write */
		for (i = 0; i < iir_coeff_num; i++)
			regmap_write(afe->regmap, src_reg[idx].con10,
				     iir_coeff[i]);

#ifdef DEBUG_COEFF
		regmap_update_bits(afe->regmap, src_reg[idx].con11,
				   COEFF_SRAM_ADR_MASK_SFT,
				   0x0);

		for (i = 0; i < iir_coeff_num; i++) {
			regmap_read(afe->regmap, src_reg[idx].con10,
				    &reg_val);
			dev_info(afe->dev, "%s(), i = %d, coeff = 0x%x\n",
				 __func__, i, reg_val);
		}
#endif
		/* disable sram access */
		regmap_update_bits(afe->regmap, src_reg[idx].con0,
				   COEFF_SRAM_CTRL_MASK_SFT,
				   0x0);
		/* CHSET_IIR_STAGE */
		iir_stage = (iir_coeff_num / 6) - 1;
		regmap_update_bits(afe->regmap, src_reg[idx].con0,
				   CHSET0_IIR_STAGE_MASK_SFT,
				   iir_stage << CHSET0_IIR_STAGE_SFT);
		/* CHSET_IIR_EN */
		regmap_update_bits(afe->regmap, src_reg[idx].con0,
				   CHSET0_IIR_EN_MASK_SFT,
				   0x1 << CHSET0_IIR_EN_SFT);
	} else {
		/* CHSET_IIR_EN off */
		regmap_update_bits(afe->regmap, src_reg[idx].con0,
				   CHSET0_IIR_EN_MASK_SFT, 0x0);
	}

	return 0;
}

#define HW_SRC_0_EN_W_NAME "HW_SRC_0_Enable"
#define HW_SRC_1_EN_W_NAME "HW_SRC_1_Enable"
#define HW_SRC_2_EN_W_NAME "HW_SRC_2_Enable"
#define HW_SRC_3_EN_W_NAME "HW_SRC_3_Enable"
#define HW_SRC_4_EN_W_NAME "HW_SRC_4_Enable"
#define HW_SRC_5_EN_W_NAME "HW_SRC_5_Enable"
#define HW_SRC_6_EN_W_NAME "HW_SRC_6_Enable"
#define HW_SRC_7_EN_W_NAME "HW_SRC_7_Enable"

static int mtk_hw_src_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol,
			    int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	int id = 0;
	struct mtk_afe_src_priv *src_priv;

	if (strcmp(w->name, HW_SRC_0_EN_W_NAME) == 0)
		id = MT6881_DAI_SRC_0;
	else if (strcmp(w->name, HW_SRC_1_EN_W_NAME) == 0)
		id = MT6881_DAI_SRC_1;
	else if (strcmp(w->name, HW_SRC_2_EN_W_NAME) == 0)
		id = MT6881_DAI_SRC_2;
	else if (strcmp(w->name, HW_SRC_3_EN_W_NAME) == 0)
		id = MT6881_DAI_SRC_3;
	else if (strcmp(w->name, HW_SRC_4_EN_W_NAME) == 0)
		id = MT6881_DAI_SRC_4;
	else if (strcmp(w->name, HW_SRC_5_EN_W_NAME) == 0)
		id = MT6881_DAI_SRC_5;
	else if (strcmp(w->name, HW_SRC_6_EN_W_NAME) == 0)
		id = MT6881_DAI_SRC_6;
	else if (strcmp(w->name, HW_SRC_7_EN_W_NAME) == 0)
		id = MT6881_DAI_SRC_7;

	if (id >= MT6881_DAI_SRC_0 && id < MT6881_DAI_SRC_NUM)
		src_priv = afe_priv->dai_priv[id];
	else {
		AUDIO_AEE("dai id negative");
		return -EINVAL;
	}

	dev_dbg(afe->dev, "%s(), name %s, event 0x%x, id %d, src_priv %p, dl_rate %d, ul_rate %d\n",
		 __func__,
		 w->name, event,
		 id, src_priv,
		 src_priv->dl_rate,
		 src_priv->ul_rate);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mtk_set_src_param(afe, id);
		break;
	case SND_SOC_DAPM_POST_PMU:
#ifdef GSRC_REG
		if (src_priv->cali_rx || src_priv->cali_tx)
			mtk_src_cali_enable(afe, id, 1);
		mtk_src_enable(afe, id, 0x1);
#endif
		break;
	case SND_SOC_DAPM_PRE_PMD:
#ifdef GSRC_REG
		if (src_priv->cali_rx || src_priv->cali_tx)
			mtk_src_cali_enable(afe, id, 0);
		mtk_src_enable(afe, id, 0x0);
		src_priv->cali_rx = 0;
		src_priv->cali_tx = 0;
#endif
		break;
	default:
		break;
	}

	return 0;
}

/* dai component */
static const struct snd_kcontrol_new mtk_hw_src_0_in_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN180_1,
				    I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN180_1,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN180_1,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN180_1,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH1", AFE_CONN180_1,
				    I_DL4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH1", AFE_CONN180_1,
				    I_DL5_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN180_1,
				    I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN180_1,
				    I_DL_24CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH1", AFE_CONN180_3,
				    I_DL45_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN180_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH1", AFE_CONN180_4,
				    I_I2SIN0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN1_CH1", AFE_CONN180_4,
				    I_I2SIN1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH1", AFE_CONN180_5,
				    I_I2SIN6_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_hw_src_0_in_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN181_1,
				    I_DL0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN181_1,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN181_1,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN181_1,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH2", AFE_CONN181_1,
				    I_DL4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH2", AFE_CONN181_1,
				    I_DL5_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN181_1,
				    I_DL6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN181_1,
				    I_DL_24CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH2", AFE_CONN181_3,
				    I_DL45_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN181_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH2", AFE_CONN181_4,
				    I_I2SIN0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN1_CH2", AFE_CONN181_4,
				    I_I2SIN1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH2", AFE_CONN180_5,
				    I_I2SIN6_CH2, 1, 0),
};

static const struct snd_kcontrol_new mtk_hw_src_1_in_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN182_1,
				    I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN182_1,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN182_1,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN182_1,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH1", AFE_CONN182_1,
				    I_DL4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH1", AFE_CONN182_1,
				    I_DL5_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN182_1,
				    I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN182_1,
				    I_DL_24CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH3", AFE_CONN182_1,
				    I_DL_24CH_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH1", AFE_CONN182_3,
				    I_DL45_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN1_OUT_CH1", AFE_CONN182_0,
				    I_GAIN1_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH1", AFE_CONN182_4,
				    I_I2SIN0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN1_CH1", AFE_CONN182_4,
				    I_I2SIN1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN2_CH1", AFE_CONN182_4,
				    I_I2SIN2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN180_4,
				    I_PCM_1_CAP_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_hw_src_1_in_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN183_1,
				    I_DL0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN183_1,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN183_1,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN183_1,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH2", AFE_CONN183_1,
				    I_DL4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH2", AFE_CONN183_1,
				    I_DL5_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN183_1,
				    I_DL6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN183_1,
				    I_DL_24CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH4", AFE_CONN183_1,
				    I_DL_24CH_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH2", AFE_CONN183_3,
				    I_DL45_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN1_OUT_CH2", AFE_CONN183_0,
				    I_GAIN1_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH2", AFE_CONN183_4,
				    I_I2SIN0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN1_CH2", AFE_CONN183_4,
				    I_I2SIN1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN2_CH2", AFE_CONN183_4,
				    I_I2SIN2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH2", AFE_CONN180_4,
				    I_PCM_1_CAP_CH2, 1, 0),
};

static const struct snd_kcontrol_new mtk_hw_src_2_in_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN184_1,
				    I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN184_1,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN184_1,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN184_1,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH1", AFE_CONN184_1,
				    I_DL4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH1", AFE_CONN184_1,
				    I_DL5_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN184_1,
				    I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN184_1,
				    I_DL_24CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH5", AFE_CONN184_1,
				    I_DL_24CH_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH1", AFE_CONN184_3,
				    I_DL45_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN184_4,
				    I_PCM_1_CAP_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_hw_src_2_in_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN185_1,
				    I_DL0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN185_1,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN185_1,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN185_1,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH2", AFE_CONN185_1,
				    I_DL4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH2", AFE_CONN185_1,
				    I_DL5_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN185_1,
				    I_DL6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN185_1,
				    I_DL_24CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH6", AFE_CONN185_1,
				    I_DL_24CH_CH6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH2", AFE_CONN185_3,
				    I_DL45_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN185_4,
				    I_PCM_1_CAP_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_hw_src_3_in_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN186_1,
				    I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN186_1,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN186_1,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN186_1,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH1", AFE_CONN186_1,
				    I_DL4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH1", AFE_CONN186_1,
				    I_DL5_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN186_1,
				    I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN186_1,
				    I_DL_24CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH7", AFE_CONN186_1,
				    I_DL_24CH_CH7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH1", AFE_CONN186_3,
				    I_DL45_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_hw_src_3_in_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN187_1,
				    I_DL0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN187_1,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN187_1,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN187_1,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH2", AFE_CONN187_1,
				    I_DL4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH2", AFE_CONN187_1,
				    I_DL5_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN187_1,
				    I_DL6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN187_1,
				    I_DL_24CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH8", AFE_CONN187_1,
				    I_DL_24CH_CH8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH2", AFE_CONN187_3,
				    I_DL45_CH2, 1, 0),
};

static const struct snd_kcontrol_new mtk_hw_src_4_in_ch1_mix[] = {

};

static const struct snd_kcontrol_new mtk_hw_src_4_in_ch2_mix[] = {

};

static const struct snd_kcontrol_new mtk_hw_src_5_in_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH1", AFE_CONN190_4,
				    I_I2SIN0_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_hw_src_5_in_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH2", AFE_CONN191_4,
				    I_I2SIN0_CH2, 1, 0),
};

static const struct snd_kcontrol_new mtk_hw_src_6_in_ch1_mix[] = {

};

static const struct snd_kcontrol_new mtk_hw_src_6_in_ch2_mix[] = {

};

static const struct snd_kcontrol_new mtk_hw_src_7_in_ch1_mix[] = {

};

static const struct snd_kcontrol_new mtk_hw_src_7_in_ch2_mix[] = {

};

static int get_src_id_from_name(const char *name)
{
	if (strncmp(name, "SRC0", 4) == 0)
		return MT6881_DAI_SRC_0;
	else if (strncmp(name, "SRC1", 4) == 0)
		return MT6881_DAI_SRC_1;
	else if (strncmp(name, "SRC2", 4) == 0)
		return MT6881_DAI_SRC_2;
	else if (strncmp(name, "SRC3", 4) == 0)
		return MT6881_DAI_SRC_3;
	else if (strncmp(name, "SRC4", 4) == 0)
		return MT6881_DAI_SRC_4;
	else if (strncmp(name, "SRC5", 4) == 0)
		return MT6881_DAI_SRC_5;
	else if (strncmp(name, "SRC6", 4) == 0)
		return MT6881_DAI_SRC_6;
	else if (strncmp(name, "SRC7", 4) == 0)
		return MT6881_DAI_SRC_7;
	else
		return -EINVAL;
}

static int mtk_hw_src_param_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_src_priv *src_priv = NULL;
	int id = 0;

	id = get_src_id_from_name(kcontrol->id.name);
	if (id < 0 || id >= MT6881_DAI_SRC_NUM) {
		dev_info(afe->dev, "%s(), invalid id:%d\n", __func__, id);
		return -EINVAL;
	}

	src_priv = afe_priv->dai_priv[id];

	if (strstr(kcontrol->id.name, "CALI_CLK"))
		ucontrol->value.integer.value[0] = src_priv->cali_clk;
	else if (strstr(kcontrol->id.name, "LRCK_SOURCE"))
		ucontrol->value.integer.value[0] = src_priv->lrck_source;
	else if (strstr(kcontrol->id.name, "ONE_HEART_MODE"))
		ucontrol->value.integer.value[0] = src_priv->one_heart_mode;
	else if (strstr(kcontrol->id.name, "CALI_CYCLE")) {
		switch (src_priv->cali_cycle) {
		case 48:
			ucontrol->value.integer.value[0] = 0;
			break;
		case 64:
			ucontrol->value.integer.value[0] = 1;
			break;
		case 96:
			ucontrol->value.integer.value[0] = 2;
			break;
		case 441:
			ucontrol->value.integer.value[0] = 3;
			break;
		default:
			dev_info(afe->dev, "%s(), %d cali cycle invalid\n", __func__, id);
			ucontrol->value.integer.value[0] = 1;
			break;
		}
	}

	return 0;
}

static int mtk_hw_src_param_set(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_src_priv *src_priv = NULL;
	int id = 0;

	id = get_src_id_from_name(kcontrol->id.name);
	if (id < 0 || id >= MT6881_DAI_SRC_NUM) {
		dev_info(afe->dev, "%s(), invalid id:%d\n", __func__, id);
		return -EINVAL;
	}

	src_priv = afe_priv->dai_priv[id];

	if (strstr(kcontrol->id.name, "CALI_CLK"))
		src_priv->cali_clk = ucontrol->value.integer.value[0];
	else if (strstr(kcontrol->id.name, "CALI_LRCK_SOURCE"))
		src_priv->lrck_source = ucontrol->value.integer.value[0];
	else if (strstr(kcontrol->id.name, "ONE_HEART_MODE"))
		src_priv->one_heart_mode = ucontrol->value.integer.value[0];
	else if (strstr(kcontrol->id.name, "CALI_CYCLE")) {
		switch (ucontrol->value.integer.value[0]) {
		case 0:
			src_priv->cali_cycle = 48;
			break;
		case 1:
			src_priv->cali_cycle = 64;
			break;
		case 2:
			src_priv->cali_cycle = 96;
			break;
		case 3:
			src_priv->cali_cycle = 441;
			break;
		default:
			dev_info(afe->dev, "%s(), %d cali cycle invalid\n", __func__, id);
			src_priv->cali_cycle = 64;
			break;
		}
	}
	dev_info(afe->dev, "%s(), id %d, calick %d, calicycle %d, lrcksource %d, oneheart %d\n",
				__func__, id,
				src_priv->cali_clk,
				src_priv->cali_cycle,
				src_priv->lrck_source,
				src_priv->one_heart_mode);
	return 0;
}

static const char *const cali_clk[] = {
	"CALI_CLK_26M", "CALI_CLK_49M", "CALI_CLK_45M"
};
static const char *const cali_cycle[] = {
	"48CYCLES", "64CYCLES", "96CYCLES", "441CYCLES"
};
static const char *const cali_lrck_source[] = {
	"AFE_CONNSYS_I2S_IN_WS", "AFE_PCM0_SYNC_OUT", "AFE_PCM0_SYNC_IN", "AFE_PCM1_SYNC_IN",
	"ETDM_OUT0_MASTER_LRCK", "ETDM_OUT1_MASTER_LRCK", "ETDM_OUT2_MASTER_LRCK",
	"ETDM_OUT3_MASTER_LRCK", "ETDM_OUT4_MASTER_LRCK", "ETDM_OUT5_MASTER_LRCK",
	"ETDM_OUT6_MASTER_LRCK", "ETDM_OUT7_MASTER_LRCK", "ETDM_IN0_SAMPLE_END",
	"ETDM_IN1_SAMPLE_END", "ETDM_IN2_SAMPLE_END", "ETDM_IN3_SAMPLE_END",
	"ETDM_IN4_SAMPLE_END", "ETDM_IN5_SAMPLE_END", "ETDM_IN6_SAMPLE_END",
	"ETDM_IN7_SAMPLE_END"
};
static const char *const one_heart_mode[] = {
	"OFF", "ON"
};
static const struct soc_enum src_param_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(cali_clk), cali_clk),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(cali_cycle), cali_cycle),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(cali_lrck_source), cali_lrck_source),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(one_heart_mode), one_heart_mode),
};

static const struct snd_kcontrol_new mtk_dai_src_controls[] = {
	SOC_ENUM_EXT("SRC2_CALI_CLK", src_param_enum[0],
		       mtk_hw_src_param_get, mtk_hw_src_param_set),
	SOC_ENUM_EXT("SRC2_CALI_CYCLE", src_param_enum[1],
		       mtk_hw_src_param_get, mtk_hw_src_param_set),
	SOC_ENUM_EXT("SRC2_CALI_LRCK_SOURCE", src_param_enum[2],
		       mtk_hw_src_param_get, mtk_hw_src_param_set),
	SOC_ENUM_EXT("SRC2_ONE_HEART_MODE", src_param_enum[3],
		       mtk_hw_src_param_get, mtk_hw_src_param_set),
	SOC_ENUM_EXT("SRC3_CALI_CLK", src_param_enum[0],
		       mtk_hw_src_param_get, mtk_hw_src_param_set),
	SOC_ENUM_EXT("SRC3_CALI_CYCLE", src_param_enum[1],
		       mtk_hw_src_param_get, mtk_hw_src_param_set),
	SOC_ENUM_EXT("SRC3_CALI_LRCK_SOURCE", src_param_enum[2],
		       mtk_hw_src_param_get, mtk_hw_src_param_set),
	SOC_ENUM_EXT("SRC3_ONE_HEART_MODE", src_param_enum[3],
		       mtk_hw_src_param_get, mtk_hw_src_param_set),
	SOC_ENUM_EXT("SRC4_CALI_CLK", src_param_enum[0],
		       mtk_hw_src_param_get, mtk_hw_src_param_set),
	SOC_ENUM_EXT("SRC4_CALI_CYCLE", src_param_enum[1],
		       mtk_hw_src_param_get, mtk_hw_src_param_set),
	SOC_ENUM_EXT("SRC4_CALI_LRCK_SOURCE", src_param_enum[2],
		       mtk_hw_src_param_get, mtk_hw_src_param_set),
	SOC_ENUM_EXT("SRC4_ONE_HEART_MODE", src_param_enum[3],
		       mtk_hw_src_param_get, mtk_hw_src_param_set),
	SOC_ENUM_EXT("SRC5_CALI_CLK", src_param_enum[0],
		       mtk_hw_src_param_get, mtk_hw_src_param_set),
	SOC_ENUM_EXT("SRC5_CALI_CYCLE", src_param_enum[1],
		       mtk_hw_src_param_get, mtk_hw_src_param_set),
	SOC_ENUM_EXT("SRC5_CALI_LRCK_SOURCE", src_param_enum[2],
		       mtk_hw_src_param_get, mtk_hw_src_param_set),
	SOC_ENUM_EXT("SRC5_ONE_HEART_MODE", src_param_enum[3],
		       mtk_hw_src_param_get, mtk_hw_src_param_set),
	SOC_ENUM_EXT("SRC6_CALI_CLK", src_param_enum[0],
		       mtk_hw_src_param_get, mtk_hw_src_param_set),
	SOC_ENUM_EXT("SRC6_CALI_CYCLE", src_param_enum[1],
		       mtk_hw_src_param_get, mtk_hw_src_param_set),
	SOC_ENUM_EXT("SRC6_CALI_LRCK_SOURCE", src_param_enum[2],
		       mtk_hw_src_param_get, mtk_hw_src_param_set),
	SOC_ENUM_EXT("SRC6_ONE_HEART_MODE", src_param_enum[3],
		       mtk_hw_src_param_get, mtk_hw_src_param_set),
	SOC_ENUM_EXT("SRC7_CALI_CLK", src_param_enum[0],
		       mtk_hw_src_param_get, mtk_hw_src_param_set),
	SOC_ENUM_EXT("SRC7_CALI_CYCLE", src_param_enum[1],
		       mtk_hw_src_param_get, mtk_hw_src_param_set),
	SOC_ENUM_EXT("SRC7_CALI_LRCK_SOURCE", src_param_enum[2],
		       mtk_hw_src_param_get, mtk_hw_src_param_set),
	SOC_ENUM_EXT("SRC7_ONE_HEART_MODE", src_param_enum[3],
		       mtk_hw_src_param_get, mtk_hw_src_param_set),
};

static const char * const hw_src_in_out_mux_text[] = {
	"connect",
};

static SOC_ENUM_SINGLE_VIRT_DECL(hw_src_output_mux_enum,
	hw_src_in_out_mux_text);

static const struct snd_kcontrol_new hw_src_output_mux =
	SOC_DAPM_ENUM("HW SRC Sink", hw_src_output_mux_enum);

static SOC_ENUM_SINGLE_VIRT_DECL(hw_src_input_mux_enum,
	hw_src_in_out_mux_text);

static const struct snd_kcontrol_new hw_src_input_mux =
	SOC_DAPM_ENUM("HW SRC Source", hw_src_input_mux_enum);

static const struct snd_soc_dapm_widget mtk_dai_src_widgets[] = {
	/* inter-connections */
	SND_SOC_DAPM_MIXER("HW_SRC_0_IN_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_hw_src_0_in_ch1_mix,
			   ARRAY_SIZE(mtk_hw_src_0_in_ch1_mix)),
	SND_SOC_DAPM_MIXER("HW_SRC_0_IN_CH2", SND_SOC_NOPM, 0, 0,
			   mtk_hw_src_0_in_ch2_mix,
			   ARRAY_SIZE(mtk_hw_src_0_in_ch2_mix)),
	SND_SOC_DAPM_MIXER("HW_SRC_1_IN_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_hw_src_1_in_ch1_mix,
			   ARRAY_SIZE(mtk_hw_src_1_in_ch1_mix)),
	SND_SOC_DAPM_MIXER("HW_SRC_1_IN_CH2", SND_SOC_NOPM, 0, 0,
			   mtk_hw_src_1_in_ch2_mix,
			   ARRAY_SIZE(mtk_hw_src_1_in_ch2_mix)),
	SND_SOC_DAPM_MIXER("HW_SRC_2_IN_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_hw_src_2_in_ch1_mix,
			   ARRAY_SIZE(mtk_hw_src_2_in_ch1_mix)),
	SND_SOC_DAPM_MIXER("HW_SRC_2_IN_CH2", SND_SOC_NOPM, 0, 0,
			   mtk_hw_src_2_in_ch2_mix,
			   ARRAY_SIZE(mtk_hw_src_2_in_ch2_mix)),
	SND_SOC_DAPM_MIXER("HW_SRC_3_IN_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_hw_src_3_in_ch1_mix,
			   ARRAY_SIZE(mtk_hw_src_3_in_ch1_mix)),
	SND_SOC_DAPM_MIXER("HW_SRC_3_IN_CH2", SND_SOC_NOPM, 0, 0,
			   mtk_hw_src_3_in_ch2_mix,
			   ARRAY_SIZE(mtk_hw_src_3_in_ch2_mix)),
	SND_SOC_DAPM_MIXER("HW_SRC_4_IN_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_hw_src_4_in_ch1_mix,
			   ARRAY_SIZE(mtk_hw_src_4_in_ch1_mix)),
	SND_SOC_DAPM_MIXER("HW_SRC_4_IN_CH2", SND_SOC_NOPM, 0, 0,
			   mtk_hw_src_4_in_ch2_mix,
			   ARRAY_SIZE(mtk_hw_src_4_in_ch2_mix)),
	SND_SOC_DAPM_MIXER("HW_SRC_5_IN_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_hw_src_5_in_ch1_mix,
			   ARRAY_SIZE(mtk_hw_src_5_in_ch1_mix)),
	SND_SOC_DAPM_MIXER("HW_SRC_5_IN_CH2", SND_SOC_NOPM, 0, 0,
			   mtk_hw_src_5_in_ch2_mix,
			   ARRAY_SIZE(mtk_hw_src_5_in_ch2_mix)),
	SND_SOC_DAPM_MIXER("HW_SRC_6_IN_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_hw_src_6_in_ch1_mix,
			   ARRAY_SIZE(mtk_hw_src_6_in_ch1_mix)),
	SND_SOC_DAPM_MIXER("HW_SRC_6_IN_CH2", SND_SOC_NOPM, 0, 0,
			   mtk_hw_src_6_in_ch2_mix,
			   ARRAY_SIZE(mtk_hw_src_6_in_ch2_mix)),
	SND_SOC_DAPM_MIXER("HW_SRC_7_IN_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_hw_src_7_in_ch1_mix,
			   ARRAY_SIZE(mtk_hw_src_7_in_ch1_mix)),
	SND_SOC_DAPM_MIXER("HW_SRC_7_IN_CH2", SND_SOC_NOPM, 0, 0,
			   mtk_hw_src_7_in_ch2_mix,
			   ARRAY_SIZE(mtk_hw_src_7_in_ch2_mix)),

	SND_SOC_DAPM_SUPPLY(HW_SRC_0_EN_W_NAME,
			    SND_SOC_NOPM, 0, 0,
			    mtk_hw_src_event,
			    SND_SOC_DAPM_PRE_PMU |
			    SND_SOC_DAPM_POST_PMU |
			    SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY(HW_SRC_1_EN_W_NAME,
			    SND_SOC_NOPM, 0, 0,
			    mtk_hw_src_event,
			    SND_SOC_DAPM_PRE_PMU |
			    SND_SOC_DAPM_POST_PMU |
			    SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY(HW_SRC_2_EN_W_NAME,
			    SND_SOC_NOPM, 0, 0,
			    mtk_hw_src_event,
			    SND_SOC_DAPM_PRE_PMU |
			    SND_SOC_DAPM_POST_PMU |
			    SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY(HW_SRC_3_EN_W_NAME,
			    SND_SOC_NOPM, 0, 0,
			    mtk_hw_src_event,
			    SND_SOC_DAPM_PRE_PMU |
			    SND_SOC_DAPM_POST_PMU |
			    SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY(HW_SRC_4_EN_W_NAME,
			    SND_SOC_NOPM, 0, 0,
			    mtk_hw_src_event,
			    SND_SOC_DAPM_PRE_PMU |
			    SND_SOC_DAPM_POST_PMU |
			    SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY(HW_SRC_5_EN_W_NAME,
			    SND_SOC_NOPM, 0, 0,
			    mtk_hw_src_event,
			    SND_SOC_DAPM_PRE_PMU |
			    SND_SOC_DAPM_POST_PMU |
			    SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY(HW_SRC_6_EN_W_NAME,
			    SND_SOC_NOPM, 0, 0,
			    mtk_hw_src_event,
			    SND_SOC_DAPM_PRE_PMU |
			    SND_SOC_DAPM_POST_PMU |
			    SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY(HW_SRC_7_EN_W_NAME,
			    SND_SOC_NOPM, 0, 0,
			    mtk_hw_src_event,
			    SND_SOC_DAPM_PRE_PMU |
			    SND_SOC_DAPM_POST_PMU |
			    SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_INPUT("HW SRC 0 Out Endpoint"),
	SND_SOC_DAPM_INPUT("HW SRC 1 Out Endpoint"),

	SND_SOC_DAPM_OUTPUT("HW SRC 0 In Endpoint"),
	SND_SOC_DAPM_OUTPUT("HW SRC 1 In Endpoint"),

	SND_SOC_DAPM_MUX("HW_SRC_2_In_Mux", SND_SOC_NOPM, 0, 0,
		&hw_src_input_mux),
	SND_SOC_DAPM_MUX("HW_SRC_3_In_Mux", SND_SOC_NOPM, 0, 0,
		&hw_src_input_mux),
	SND_SOC_DAPM_MUX("HW_SRC_4_In_Mux", SND_SOC_NOPM, 0, 0,
		&hw_src_input_mux),
	SND_SOC_DAPM_MUX("HW_SRC_5_In_Mux", SND_SOC_NOPM, 0, 0,
		&hw_src_input_mux),
	SND_SOC_DAPM_MUX("HW_SRC_6_In_Mux", SND_SOC_NOPM, 0, 0,
		&hw_src_input_mux),
	SND_SOC_DAPM_MUX("HW_SRC_7_In_Mux", SND_SOC_NOPM, 0, 0,
		&hw_src_input_mux),
};

static int mtk_afe_src_en_connect(struct snd_soc_dapm_widget *source,
				  struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_dapm_widget *w = source;
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_src_priv *src_priv = NULL;
	int ret = 0;

	if (strcmp(w->name, HW_SRC_0_EN_W_NAME) == 0)
		src_priv = afe_priv->dai_priv[MT6881_DAI_SRC_0];
	else if (strcmp(w->name, HW_SRC_1_EN_W_NAME) == 0)
		src_priv = afe_priv->dai_priv[MT6881_DAI_SRC_1];
	else if (strcmp(w->name, HW_SRC_2_EN_W_NAME) == 0)
		src_priv = afe_priv->dai_priv[MT6881_DAI_SRC_2];
	else if (strcmp(w->name, HW_SRC_3_EN_W_NAME) == 0)
		src_priv = afe_priv->dai_priv[MT6881_DAI_SRC_3];
	else if (strcmp(w->name, HW_SRC_4_EN_W_NAME) == 0)
		src_priv = afe_priv->dai_priv[MT6881_DAI_SRC_4];
	else if (strcmp(w->name, HW_SRC_5_EN_W_NAME) == 0)
		src_priv = afe_priv->dai_priv[MT6881_DAI_SRC_5];
	else if (strcmp(w->name, HW_SRC_6_EN_W_NAME) == 0)
		src_priv = afe_priv->dai_priv[MT6881_DAI_SRC_6];
	else if (strcmp(w->name, HW_SRC_7_EN_W_NAME) == 0)
		src_priv = afe_priv->dai_priv[MT6881_DAI_SRC_7];

	if (!src_priv) {
		AUDIO_AEE("src_priv == NULL");
		return 0;
	}
	if (src_priv->dl_rate > 0 && src_priv->ul_rate == 0) {
		struct snd_soc_dai *dai = NULL;
		struct mtk_afe_i2s_priv *i2s_priv = NULL;

		dai = get_be_cpu_dai(sink, SNDRV_PCM_STREAM_PLAYBACK);
		if (dai) {
			if (dai->id >= MT6881_DAI_I2S_OUT0 && dai->id <= MT6881_DAI_I2S_OUT6) {
				i2s_priv = afe_priv->dai_priv[dai->id];
				if (i2s_priv->fixup_rate != 0)
					src_priv->ul_rate = i2s_priv->fixup_rate;
				else
					src_priv->ul_rate = dai->rate;
			}
			dev_dbg(afe->dev, "%s(), dl_rate:%d, ul_rate:%d, dai:%s playback\n",
					__func__, src_priv->dl_rate, src_priv->ul_rate, dai->name);
		}
	}
	if (src_priv->dl_rate == 0 && src_priv->ul_rate > 0) {
		struct snd_soc_dai *dai = NULL;
		struct mtk_afe_i2s_priv *i2s_priv = NULL;

		dai = get_be_cpu_dai(sink, SNDRV_PCM_STREAM_CAPTURE);
		if (dai) {
			if (dai->id >= MT6881_DAI_I2S_IN0 && dai->id <= MT6881_DAI_I2S_IN6) {
				i2s_priv = afe_priv->dai_priv[dai->id];
				if (i2s_priv->fixup_rate != 0)
					src_priv->dl_rate = i2s_priv->fixup_rate;
				else
					src_priv->dl_rate = dai->rate;
			}
			dev_dbg(afe->dev, "%s(), dl_rate:%d, ul_rate:%d, dai:%s capture\n",
					__func__, src_priv->dl_rate, src_priv->ul_rate, dai->name);
		}
	}

	ret = (src_priv->dl_rate > 0 && src_priv->ul_rate > 0) ? 1 : 0;
	if (ret)
		dev_info(afe->dev,
			 "%s(), source %s, sink %s, dl_rate %d, ul_rate %d\n",
			 __func__, source->name, sink->name,
			 src_priv->dl_rate, src_priv->ul_rate);

	return ret;
}

static int mtk_afe_src_apll_connect(struct snd_soc_dapm_widget *source,
				     struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_dapm_widget *w = sink;
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	struct mtk_afe_src_priv *src_priv = NULL;
	int cur_apll;
	int need_apll;

	if (strcmp(w->name, HW_SRC_0_EN_W_NAME) == 0)
		src_priv = afe_priv->dai_priv[MT6881_DAI_SRC_0];
	else if (strcmp(w->name, HW_SRC_1_EN_W_NAME) == 0)
		src_priv = afe_priv->dai_priv[MT6881_DAI_SRC_1];
	else if (strcmp(w->name, HW_SRC_2_EN_W_NAME) == 0)
		src_priv = afe_priv->dai_priv[MT6881_DAI_SRC_2];
	else if (strcmp(w->name, HW_SRC_3_EN_W_NAME) == 0)
		src_priv = afe_priv->dai_priv[MT6881_DAI_SRC_3];
	else if (strcmp(w->name, HW_SRC_4_EN_W_NAME) == 0)
		src_priv = afe_priv->dai_priv[MT6881_DAI_SRC_4];
	else if (strcmp(w->name, HW_SRC_5_EN_W_NAME) == 0)
		src_priv = afe_priv->dai_priv[MT6881_DAI_SRC_5];
	else if (strcmp(w->name, HW_SRC_6_EN_W_NAME) == 0)
		src_priv = afe_priv->dai_priv[MT6881_DAI_SRC_6];
	else if (strcmp(w->name, HW_SRC_7_EN_W_NAME) == 0)
		src_priv = afe_priv->dai_priv[MT6881_DAI_SRC_7];

	if (!src_priv) {
		AUDIO_AEE("src_priv == NULL");
		return 0;
	}

	/* which apll */
	cur_apll = mt6881_get_apll_by_name(afe, source->name);
	/* choose APLL from SRC IFS rate */
	need_apll = mt6881_get_apll_by_rate(afe, src_priv->ul_rate);

	return (need_apll == cur_apll) ? 1 : 0;
}

static const struct snd_soc_dapm_route mtk_dai_src_routes[] = {
	{"HW_SRC_0_IN_CH1", "DL0_CH1", "DL0"},
	{"HW_SRC_0_IN_CH2", "DL0_CH2", "DL0"},
	{"HW_SRC_1_IN_CH1", "DL0_CH1", "DL0"},
	{"HW_SRC_1_IN_CH2", "DL0_CH2", "DL0"},
	{"HW_SRC_2_IN_CH1", "DL0_CH1", "DL0"},
	{"HW_SRC_2_IN_CH2", "DL0_CH2", "DL0"},
	{"HW_SRC_3_IN_CH1", "DL0_CH1", "DL0"},
	{"HW_SRC_3_IN_CH2", "DL0_CH2", "DL0"},

	{"HW_SRC_0_IN_CH1", "DL1_CH1", "DL1"},
	{"HW_SRC_0_IN_CH2", "DL1_CH2", "DL1"},
	{"HW_SRC_1_IN_CH1", "DL1_CH1", "DL1"},
	{"HW_SRC_1_IN_CH2", "DL1_CH2", "DL1"},
	{"HW_SRC_2_IN_CH1", "DL1_CH1", "DL1"},
	{"HW_SRC_2_IN_CH2", "DL1_CH2", "DL1"},
	{"HW_SRC_3_IN_CH1", "DL1_CH1", "DL1"},
	{"HW_SRC_3_IN_CH2", "DL1_CH2", "DL1"},

	{"HW_SRC_0_IN_CH1", "DL2_CH1", "DL2"},
	{"HW_SRC_0_IN_CH2", "DL2_CH2", "DL2"},
	{"HW_SRC_1_IN_CH1", "DL2_CH1", "DL2"},
	{"HW_SRC_1_IN_CH2", "DL2_CH2", "DL2"},
	{"HW_SRC_2_IN_CH1", "DL2_CH1", "DL2"},
	{"HW_SRC_2_IN_CH2", "DL2_CH2", "DL2"},
	{"HW_SRC_3_IN_CH1", "DL2_CH1", "DL2"},
	{"HW_SRC_3_IN_CH2", "DL2_CH2", "DL2"},

	{"HW_SRC_0_IN_CH1", "DL3_CH1", "DL3"},
	{"HW_SRC_0_IN_CH2", "DL3_CH2", "DL3"},
	{"HW_SRC_1_IN_CH1", "DL3_CH1", "DL3"},
	{"HW_SRC_1_IN_CH2", "DL3_CH2", "DL3"},
	{"HW_SRC_2_IN_CH1", "DL3_CH1", "DL3"},
	{"HW_SRC_2_IN_CH2", "DL3_CH2", "DL3"},
	{"HW_SRC_3_IN_CH1", "DL3_CH1", "DL3"},
	{"HW_SRC_3_IN_CH2", "DL3_CH2", "DL3"},

	{"HW_SRC_0_IN_CH1", "DL4_CH1", "DL4"},
	{"HW_SRC_0_IN_CH2", "DL4_CH2", "DL4"},
	{"HW_SRC_1_IN_CH1", "DL4_CH1", "DL4"},
	{"HW_SRC_1_IN_CH2", "DL4_CH2", "DL4"},
	{"HW_SRC_2_IN_CH1", "DL4_CH1", "DL4"},
	{"HW_SRC_2_IN_CH2", "DL4_CH2", "DL4"},
	{"HW_SRC_3_IN_CH1", "DL4_CH1", "DL4"},
	{"HW_SRC_3_IN_CH2", "DL4_CH2", "DL4"},

	{"HW_SRC_0_IN_CH1", "DL5_CH1", "DL5"},
	{"HW_SRC_0_IN_CH2", "DL5_CH2", "DL5"},
	{"HW_SRC_1_IN_CH1", "DL5_CH1", "DL5"},
	{"HW_SRC_1_IN_CH2", "DL5_CH2", "DL5"},
	{"HW_SRC_2_IN_CH1", "DL5_CH1", "DL5"},
	{"HW_SRC_2_IN_CH2", "DL5_CH2", "DL5"},
	{"HW_SRC_3_IN_CH1", "DL5_CH1", "DL5"},
	{"HW_SRC_3_IN_CH2", "DL5_CH2", "DL5"},

	{"HW_SRC_0_IN_CH1", "DL6_CH1", "DL6"},
	{"HW_SRC_0_IN_CH2", "DL6_CH2", "DL6"},
	{"HW_SRC_1_IN_CH1", "DL6_CH1", "DL6"},
	{"HW_SRC_1_IN_CH2", "DL6_CH2", "DL6"},
	{"HW_SRC_2_IN_CH1", "DL6_CH1", "DL6"},
	{"HW_SRC_2_IN_CH2", "DL6_CH2", "DL6"},
	{"HW_SRC_3_IN_CH1", "DL6_CH1", "DL6"},
	{"HW_SRC_3_IN_CH2", "DL6_CH2", "DL6"},

	{"HW_SRC_0_IN_CH1", "DL24_CH1", "DL6"},
	{"HW_SRC_0_IN_CH2", "DL24_CH2", "DL6"},
	{"HW_SRC_1_IN_CH1", "DL24_CH1", "DL6"},
	{"HW_SRC_1_IN_CH2", "DL24_CH2", "DL6"},
	{"HW_SRC_2_IN_CH1", "DL24_CH1", "DL6"},
	{"HW_SRC_2_IN_CH2", "DL24_CH2", "DL6"},
	{"HW_SRC_3_IN_CH1", "DL24_CH1", "DL6"},
	{"HW_SRC_3_IN_CH2", "DL24_CH2", "DL6"},
	{"HW_SRC_0_IN_CH1", "DL_24CH_CH1", "DL_24CH"},
	{"HW_SRC_0_IN_CH2", "DL_24CH_CH2", "DL_24CH"},
	{"HW_SRC_1_IN_CH1", "DL_24CH_CH1", "DL_24CH"},
	{"HW_SRC_1_IN_CH2", "DL_24CH_CH2", "DL_24CH"},
	{"HW_SRC_2_IN_CH1", "DL_24CH_CH1", "DL_24CH"},
	{"HW_SRC_2_IN_CH2", "DL_24CH_CH2", "DL_24CH"},
	{"HW_SRC_3_IN_CH1", "DL_24CH_CH1", "DL_24CH"},
	{"HW_SRC_3_IN_CH2", "DL_24CH_CH2", "DL_24CH"},

	{"HW_SRC_1_IN_CH1", "DL_24CH_CH3", "DL_24CH"},
	{"HW_SRC_1_IN_CH2", "DL_24CH_CH4", "DL_24CH"},
	{"HW_SRC_2_IN_CH1", "DL_24CH_CH5", "DL_24CH"},
	{"HW_SRC_2_IN_CH2", "DL_24CH_CH6", "DL_24CH"},
	{"HW_SRC_3_IN_CH1", "DL_24CH_CH7", "DL_24CH"},
	{"HW_SRC_3_IN_CH2", "DL_24CH_CH8", "DL_24CH"},
	{"HW_SRC_4_IN_CH1", "DL_24CH_CH9", "DL_24CH"},
	{"HW_SRC_4_IN_CH2", "DL_24CH_CH10", "DL_24CH"},
	{"HW_SRC_5_IN_CH1", "DL_24CH_CH11", "DL_24CH"},
	{"HW_SRC_5_IN_CH2", "DL_24CH_CH12", "DL_24CH"},

	{"HW_SRC_2_IN_CH1", "PCM_1_CAP_CH1", "PCM 1 Capture"},
	{"HW_SRC_2_IN_CH2", "PCM_1_CAP_CH1", "PCM 1 Capture"},

	{"HW_SRC_1_IN_CH1", "I2SIN0_CH1", "I2SIN0"},
	{"HW_SRC_1_IN_CH2", "I2SIN0_CH2", "I2SIN0"},
	{"HW_SRC_1_IN_CH1", "I2SIN1_CH1", "I2SIN1"},
	{"HW_SRC_1_IN_CH2", "I2SIN1_CH2", "I2SIN1"},
	{"HW_SRC_5_IN_CH1", "I2SIN0_CH1", "I2SIN0"},
	{"HW_SRC_5_IN_CH2", "I2SIN0_CH2", "I2SIN0"},

	{"HW_SRC_0_In", NULL, "HW_SRC_0_IN_CH1"},
	{"HW_SRC_0_In", NULL, "HW_SRC_0_IN_CH2"},
	{"HW_SRC_1_In", NULL, "HW_SRC_1_IN_CH1"},
	{"HW_SRC_1_In", NULL, "HW_SRC_1_IN_CH2"},
	{"HW_SRC_2_In", NULL, "HW_SRC_2_IN_CH1"},
	{"HW_SRC_2_In", NULL, "HW_SRC_2_IN_CH2"},
	{"HW_SRC_3_In", NULL, "HW_SRC_3_IN_CH1"},
	{"HW_SRC_3_In", NULL, "HW_SRC_3_IN_CH2"},
	{"HW_SRC_4_In", NULL, "HW_SRC_4_IN_CH1"},
	{"HW_SRC_4_In", NULL, "HW_SRC_4_IN_CH2"},
	{"HW_SRC_5_In", NULL, "HW_SRC_5_IN_CH1"},
	{"HW_SRC_5_In", NULL, "HW_SRC_5_IN_CH2"},
	{"HW_SRC_6_In", NULL, "HW_SRC_6_IN_CH1"},
	{"HW_SRC_6_In", NULL, "HW_SRC_6_IN_CH2"},
	{"HW_SRC_7_In", NULL, "HW_SRC_7_IN_CH1"},
	{"HW_SRC_7_In", NULL, "HW_SRC_7_IN_CH2"},

	{"HW_SRC_0_In", NULL, HW_SRC_0_EN_W_NAME, mtk_afe_src_en_connect},
	{"HW_SRC_0_Out", NULL, HW_SRC_0_EN_W_NAME, mtk_afe_src_en_connect},
	{"HW_SRC_1_In", NULL, HW_SRC_1_EN_W_NAME, mtk_afe_src_en_connect},
	{"HW_SRC_1_Out", NULL, HW_SRC_1_EN_W_NAME, mtk_afe_src_en_connect},
	{"HW_SRC_2_In", NULL, HW_SRC_2_EN_W_NAME, mtk_afe_src_en_connect},
	{"HW_SRC_2_Out", NULL, HW_SRC_2_EN_W_NAME, mtk_afe_src_en_connect},
	{"HW_SRC_3_In", NULL, HW_SRC_3_EN_W_NAME, mtk_afe_src_en_connect},
	{"HW_SRC_3_Out", NULL, HW_SRC_3_EN_W_NAME, mtk_afe_src_en_connect},
	{"HW_SRC_4_In", NULL, HW_SRC_4_EN_W_NAME, mtk_afe_src_en_connect},
	{"HW_SRC_4_Out", NULL, HW_SRC_4_EN_W_NAME, mtk_afe_src_en_connect},
	{"HW_SRC_5_In", NULL, HW_SRC_5_EN_W_NAME, mtk_afe_src_en_connect},
	{"HW_SRC_5_Out", NULL, HW_SRC_5_EN_W_NAME, mtk_afe_src_en_connect},
	{"HW_SRC_6_In", NULL, HW_SRC_6_EN_W_NAME, mtk_afe_src_en_connect},
	{"HW_SRC_6_Out", NULL, HW_SRC_6_EN_W_NAME, mtk_afe_src_en_connect},
	{"HW_SRC_7_In", NULL, HW_SRC_7_EN_W_NAME, mtk_afe_src_en_connect},
	{"HW_SRC_7_Out", NULL, HW_SRC_7_EN_W_NAME, mtk_afe_src_en_connect},

#if !defined(IS_FPGA_EARLY_PORTING)
	{HW_SRC_0_EN_W_NAME, NULL, "vlp_mux_audio_h"},
	{HW_SRC_1_EN_W_NAME, NULL, "vlp_mux_audio_h"},
	{HW_SRC_2_EN_W_NAME, NULL, "vlp_mux_audio_h"},
	{HW_SRC_3_EN_W_NAME, NULL, "vlp_mux_audio_h"},
	{HW_SRC_4_EN_W_NAME, NULL, "vlp_mux_audio_h"},
	{HW_SRC_5_EN_W_NAME, NULL, "vlp_mux_audio_h"},
	{HW_SRC_6_EN_W_NAME, NULL, "vlp_mux_audio_h"},
	{HW_SRC_7_EN_W_NAME, NULL, "vlp_mux_audio_h"},
#endif
	/* hires source from apll1 */
	{HW_SRC_0_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_src_apll_connect},
	{HW_SRC_1_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_src_apll_connect},
	{HW_SRC_2_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_src_apll_connect},
	{HW_SRC_3_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_src_apll_connect},
	{HW_SRC_4_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_src_apll_connect},
	{HW_SRC_5_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_src_apll_connect},
	{HW_SRC_6_EN_W_NAME, NULL, APLL2_W_NAME, mtk_afe_src_apll_connect},
	{HW_SRC_7_EN_W_NAME, NULL, APLL1_W_NAME, mtk_afe_src_apll_connect},

	{"HW SRC 0 In Endpoint", NULL, "HW_SRC_0_In"},
	{"HW SRC 1 In Endpoint", NULL, "HW_SRC_1_In"},

	{"HW_SRC_0_Out", NULL, "HW SRC 0 Out Endpoint"},
	{"HW_SRC_1_Out", NULL, "HW SRC 1 Out Endpoint"},

	{"HW_SRC_2_In_Mux", "connect", "HW_SRC_2_IN_CH1"},
	{"HW_SRC_2_In_Mux", "connect", "HW_SRC_2_IN_CH2"},
	{"HW_SRC_2_Out", NULL, "HW_SRC_2_In_Mux"},

	{"HW_SRC_3_In_Mux", "connect", "HW_SRC_3_IN_CH1"},
	{"HW_SRC_3_In_Mux", "connect", "HW_SRC_3_IN_CH2"},
	{"HW_SRC_3_Out", NULL, "HW_SRC_3_In_Mux"},

	{"HW_SRC_4_In_Mux", "connect", "HW_SRC_4_IN_CH1"},
	{"HW_SRC_4_In_Mux", "connect", "HW_SRC_4_IN_CH2"},
	{"HW_SRC_4_Out", NULL, "HW_SRC_4_In_Mux"},

	{"HW_SRC_5_In_Mux", "connect", "HW_SRC_5_IN_CH1"},
	{"HW_SRC_5_In_Mux", "connect", "HW_SRC_5_IN_CH2"},
	{"HW_SRC_5_Out", NULL, "HW_SRC_5_In_Mux"},

	{"HW_SRC_6_In_Mux", "connect", "HW_SRC_6_IN_CH1"},
	{"HW_SRC_6_In_Mux", "connect", "HW_SRC_6_IN_CH2"},
	{"HW_SRC_6_Out", NULL, "HW_SRC_6_In_Mux"},

	{"HW_SRC_7_In_Mux", "connect", "HW_SRC_7_IN_CH1"},
	{"HW_SRC_7_In_Mux", "connect", "HW_SRC_7_IN_CH2"},
	{"HW_SRC_7_Out", NULL, "HW_SRC_7_In_Mux"},
};

/* dai ops */
static int mtk_dai_src_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	int id = dai->id;
	struct mtk_afe_src_priv *src_priv = afe_priv->dai_priv[id];
	unsigned int rate = params_rate(params);

	src_priv->substream = substream;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		src_priv->dl_rate = rate;
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		src_priv->ul_rate = rate;

	mtk_src_need_tracking(substream, afe, id);

	mtk_src_reset(afe, id);
	mtk_src_clear(afe, id);

	dev_info(afe->dev, "%s, id %d, dir %d, rate %d, cali_tx %d, cali_rx %d\n",
		__func__,
		id - MT6881_DAI_SRC_0,
		substream->stream,
		rate,
		src_priv->cali_tx,
		src_priv->cali_rx);

	return 0;
}

static int mtk_dai_src_hw_free(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt6881_afe_private *afe_priv = afe->platform_priv;
	int id = dai->id;
	struct mtk_afe_src_priv *src_priv = afe_priv->dai_priv[id];

	dev_info(afe->dev, "%s(), id %d, stream %d\n",
		 __func__,
		 id,
		 substream->stream);

	src_priv->dl_rate = 0;
	src_priv->ul_rate = 0;

	if (src_priv->substream)
		src_priv->substream = NULL;

	src_priv->cali_rx = 0;
	src_priv->cali_tx = 0;

	return 0;
}

static const struct snd_soc_dai_ops mtk_dai_src_ops = {
	.hw_params = mtk_dai_src_hw_params,
	.hw_free = mtk_dai_src_hw_free,
};

/* dai driver */
#define MTK_SRC_RATES (SNDRV_PCM_RATE_8000_48000 |\
		       SNDRV_PCM_RATE_88200 |\
		       SNDRV_PCM_RATE_96000 |\
		       SNDRV_PCM_RATE_176400 |\
		       SNDRV_PCM_RATE_192000)

#define MTK_SRC_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			 SNDRV_PCM_FMTBIT_S24_LE |\
			 SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver mtk_dai_src_driver[] = {
	{
		.name = "HW_SRC_0",
		.id = MT6881_DAI_SRC_0,
		.playback = {
			.stream_name = "HW_SRC_0_In",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_SRC_RATES,
			.formats = MTK_SRC_FORMATS,
		},
		.capture = {
			.stream_name = "HW_SRC_0_Out",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_SRC_RATES,
			.formats = MTK_SRC_FORMATS,
		},
		.ops = &mtk_dai_src_ops,
	},
	{
		.name = "HW_SRC_1",
		.id = MT6881_DAI_SRC_1,
		.playback = {
			.stream_name = "HW_SRC_1_In",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_SRC_RATES,
			.formats = MTK_SRC_FORMATS,
		},
		.capture = {
			.stream_name = "HW_SRC_1_Out",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_SRC_RATES,
			.formats = MTK_SRC_FORMATS,
		},
		.ops = &mtk_dai_src_ops,
	},
	{
		.name = "HW_SRC_2",
		.id = MT6881_DAI_SRC_2,
		.playback = {
			.stream_name = "HW_SRC_2_In",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_SRC_RATES,
			.formats = MTK_SRC_FORMATS,
		},
		.capture = {
			.stream_name = "HW_SRC_2_Out",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_SRC_RATES,
			.formats = MTK_SRC_FORMATS,
		},
		.ops = &mtk_dai_src_ops,
	},
	{
		.name = "HW_SRC_3",
		.id = MT6881_DAI_SRC_3,
		.playback = {
			.stream_name = "HW_SRC_3_In",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_SRC_RATES,
			.formats = MTK_SRC_FORMATS,
		},
		.capture = {
			.stream_name = "HW_SRC_3_Out",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_SRC_RATES,
			.formats = MTK_SRC_FORMATS,
		},
		.ops = &mtk_dai_src_ops,
	},
	{
		.name = "HW_SRC_4",
		.id = MT6881_DAI_SRC_4,
		.playback = {
			.stream_name = "HW_SRC_4_In",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_SRC_RATES,
			.formats = MTK_SRC_FORMATS,
		},
		.capture = {
			.stream_name = "HW_SRC_4_Out",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_SRC_RATES,
			.formats = MTK_SRC_FORMATS,
		},
		.ops = &mtk_dai_src_ops,
	},
	{
		.name = "HW_SRC_5",
		.id = MT6881_DAI_SRC_5,
		.playback = {
			.stream_name = "HW_SRC_5_In",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_SRC_RATES,
			.formats = MTK_SRC_FORMATS,
		},
		.capture = {
			.stream_name = "HW_SRC_5_Out",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_SRC_RATES,
			.formats = MTK_SRC_FORMATS,
		},
		.ops = &mtk_dai_src_ops,
	},
	{
		.name = "HW_SRC_6",
		.id = MT6881_DAI_SRC_6,
		.playback = {
			.stream_name = "HW_SRC_6_In",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_SRC_RATES,
			.formats = MTK_SRC_FORMATS,
		},
		.capture = {
			.stream_name = "HW_SRC_6_Out",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_SRC_RATES,
			.formats = MTK_SRC_FORMATS,
		},
		.ops = &mtk_dai_src_ops,
	},
	{
		.name = "HW_SRC_7",
		.id = MT6881_DAI_SRC_7,
		.playback = {
			.stream_name = "HW_SRC_7_In",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_SRC_RATES,
			.formats = MTK_SRC_FORMATS,
		},
		.capture = {
			.stream_name = "HW_SRC_7_Out",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_SRC_RATES,
			.formats = MTK_SRC_FORMATS,
		},
		.ops = &mtk_dai_src_ops,
	},
};

int mt6881_dai_src_register(struct mtk_base_afe *afe)
{
	struct mtk_base_afe_dai *dai;
	int ret;
	int i;

	dev_info(afe->dev, "%s() successfully start\n", __func__);

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = mtk_dai_src_driver;
	dai->num_dai_drivers = ARRAY_SIZE(mtk_dai_src_driver);

	dai->controls = mtk_dai_src_controls;
	dai->num_controls = ARRAY_SIZE(mtk_dai_src_controls);
	dai->dapm_widgets = mtk_dai_src_widgets;
	dai->num_dapm_widgets = ARRAY_SIZE(mtk_dai_src_widgets);
	dai->dapm_routes = mtk_dai_src_routes;
	dai->num_dapm_routes = ARRAY_SIZE(mtk_dai_src_routes);

	/* set dai priv */
	for (i = 0; i < MT6881_SRC_NUM; i++) {
		ret = mt6881_dai_set_priv(afe, i + MT6881_DAI_SRC_0,
					  sizeof(struct mtk_afe_src_priv),
					  NULL);
		if (ret)
			return ret;
	}

	return 0;
}

