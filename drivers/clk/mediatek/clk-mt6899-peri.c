// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: KY Liu <ky.liu@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6899-clk.h>

#define MT_CCF_BRINGUP		0

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs impc_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPC(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impc_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMPC_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate impc_clks[] = {
	GATE_IMPC(CLK_IMPC_I2C10, "impc_i2c10",
			"i2c_ck"/* parent */, 0),
	GATE_IMPC_V(CLK_IMPC_I2C10_I2C, "impc_i2c10_i2c",
			"impc_i2c10"/* parent */),
	GATE_IMPC(CLK_IMPC_I2C11, "impc_i2c11",
			"i2c_ck"/* parent */, 1),
	GATE_IMPC_V(CLK_IMPC_I2C11_I2C, "impc_i2c11_i2c",
			"impc_i2c11"/* parent */),
	GATE_IMPC(CLK_IMPC_I2C12, "impc_i2c12",
			"i2c_ck"/* parent */, 2),
	GATE_IMPC_V(CLK_IMPC_I2C12_I2C, "impc_i2c12_i2c",
			"impc_i2c12"/* parent */),
	GATE_IMPC(CLK_IMPC_I2C13, "impc_i2c13",
			"i2c_ck"/* parent */, 3),
	GATE_IMPC_V(CLK_IMPC_I2C13_I2C, "impc_i2c13_i2c",
			"impc_i2c13"/* parent */),
};

static const struct mtk_clk_desc impc_mcd = {
	.clks = impc_clks,
	.num_clks = CLK_IMPC_NR_CLK,
};

static const struct mtk_gate_regs impe_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPE(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impe_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMPE_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate impe_clks[] = {
	GATE_IMPE(CLK_IMPE_I3C4, "impe_i3c4",
			"i2c_ck"/* parent */, 0),
	GATE_IMPE_V(CLK_IMPE_I3C4_I2C, "impe_i3c4_i2c",
			"impe_i3c4"/* parent */),
	GATE_IMPE(CLK_IMPE_I3C8, "impe_i3c8",
			"i2c_ck"/* parent */, 1),
	GATE_IMPE_V(CLK_IMPE_I3C8_I2C, "impe_i3c8_i2c",
			"impe_i3c8"/* parent */),
};

static const struct mtk_clk_desc impe_mcd = {
	.clks = impe_clks,
	.num_clks = CLK_IMPE_NR_CLK,
};

static const struct mtk_gate_regs impen_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPEN(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impen_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMPEN_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate impen_clks[] = {
	GATE_IMPEN(CLK_IMPEN_I3C2, "impen_i3c2",
			"i2c_ck"/* parent */, 0),
	GATE_IMPEN_V(CLK_IMPEN_I3C2_I2C, "impen_i3c2_i2c",
			"impen_i3c2"/* parent */),
};

static const struct mtk_clk_desc impen_mcd = {
	.clks = impen_clks,
	.num_clks = CLK_IMPEN_NR_CLK,
};

static const struct mtk_gate_regs impes_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPES(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impes_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMPES_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate impes_clks[] = {
	GATE_IMPES(CLK_IMPES_I3C9, "impes_i3c9",
			"i2c_ck"/* parent */, 0),
	GATE_IMPES_V(CLK_IMPES_I3C9_I2C, "impes_i3c9_i2c",
			"impes_i3c9"/* parent */),
};

static const struct mtk_clk_desc impes_mcd = {
	.clks = impes_clks,
	.num_clks = CLK_IMPES_NR_CLK,
};

static const struct mtk_gate_regs impn_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPN(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impn_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMPN_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate impn_clks[] = {
	GATE_IMPN(CLK_IMPN_I2C3, "impn_i2c3",
			"i2c_ck"/* parent */, 0),
	GATE_IMPN_V(CLK_IMPN_I2C3_I2C, "impn_i2c3_i2c",
			"impn_i2c3"/* parent */),
	GATE_IMPN(CLK_IMPN_I2C5, "impn_i2c5",
			"i2c_ck"/* parent */, 1),
	GATE_IMPN_V(CLK_IMPN_I2C5_I2C, "impn_i2c5_i2c",
			"impn_i2c5"/* parent */),
};

static const struct mtk_clk_desc impn_mcd = {
	.clks = impn_clks,
	.num_clks = CLK_IMPN_NR_CLK,
};

static const struct mtk_gate_regs imps_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPS(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &imps_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMPS_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate imps_clks[] = {
	GATE_IMPS(CLK_IMPS_I3C0, "imps_i3c0",
			"i2c_ck"/* parent */, 0),
	GATE_IMPS_V(CLK_IMPS_I3C0_I2C, "imps_i3c0_i2c",
			"imps_i3c0"/* parent */),
	GATE_IMPS(CLK_IMPS_I3C1, "imps_i3c1",
			"i2c_ck"/* parent */, 1),
	GATE_IMPS_V(CLK_IMPS_I3C1_I2C, "imps_i3c1_i2c",
			"imps_i3c1"/* parent */),
	GATE_IMPS(CLK_IMPS_I3C7, "imps_i3c7",
			"i2c_ck"/* parent */, 2),
	GATE_IMPS_V(CLK_IMPS_I3C7_I2C, "imps_i3c7_i2c",
			"imps_i3c7"/* parent */),
};

static const struct mtk_clk_desc imps_mcd = {
	.clks = imps_clks,
	.num_clks = CLK_IMPS_NR_CLK,
};

static const struct mtk_gate_regs impw_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPW(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impw_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMPW_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate impw_clks[] = {
	GATE_IMPW(CLK_IMPW_I2C6, "impw_i2c6",
			"i2c_ck"/* parent */, 0),
	GATE_IMPW_V(CLK_IMPW_I2C6_I2C, "impw_i2c6_i2c",
			"impw_i2c6"/* parent */),
};

static const struct mtk_clk_desc impw_mcd = {
	.clks = impw_clks,
	.num_clks = CLK_IMPW_NR_CLK,
};

static const struct mtk_gate_regs perao0_cg_regs = {
	.set_ofs = 0x24,
	.clr_ofs = 0x28,
	.sta_ofs = 0x10,
};

static const struct mtk_gate_regs perao1_cg_regs = {
	.set_ofs = 0x2C,
	.clr_ofs = 0x30,
	.sta_ofs = 0x14,
};

static const struct mtk_gate_regs perao2_cg_regs = {
	.set_ofs = 0x34,
	.clr_ofs = 0x38,
	.sta_ofs = 0x18,
};

#define GATE_PERAO0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &perao0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_PERAO0_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

#define GATE_PERAO1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &perao1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_PERAO1_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

#define GATE_PERAO2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &perao2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_PERAO2_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate perao_clks[] = {
	/* PERAO0 */
	GATE_PERAO0(CLK_PERAOP_UART0, "peraop_uart0",
			"uart_ck"/* parent */, 0),
	GATE_PERAO0_V(CLK_PERAOP_UART0_UART, "peraop_uart0_uart",
			"peraop_uart0"/* parent */),
	GATE_PERAO0(CLK_PERAOP_UART1, "peraop_uart1",
			"uart_ck"/* parent */, 1),
	GATE_PERAO0_V(CLK_PERAOP_UART1_UART, "peraop_uart1_uart",
			"peraop_uart1"/* parent */),
	GATE_PERAO0(CLK_PERAOP_UART2, "peraop_uart2",
			"uart_ck"/* parent */, 2),
	GATE_PERAO0_V(CLK_PERAOP_UART2_UART, "peraop_uart2_uart",
			"peraop_uart2"/* parent */),
	GATE_PERAO0(CLK_PERAOP_UART3, "peraop_uart3",
			"uart_ck"/* parent */, 3),
	GATE_PERAO0_V(CLK_PERAOP_UART3_UART, "peraop_uart3_uart",
			"peraop_uart3"/* parent */),
	GATE_PERAO0(CLK_PERAOP_PWM_H, "peraop_pwm_h",
			"peri_faxi_ck"/* parent */, 4),
	GATE_PERAO0_V(CLK_PERAOP_PWM_H_PWM, "peraop_pwm_h_pwm",
			"peraop_pwm_h"/* parent */),
	GATE_PERAO0(CLK_PERAOP_PWM_B, "peraop_pwm_b",
			"pwm_ck"/* parent */, 5),
	GATE_PERAO0_V(CLK_PERAOP_PWM_B_PWM, "peraop_pwm_b_pwm",
			"peraop_pwm_b"/* parent */),
	GATE_PERAO0(CLK_PERAOP_PWM_FB1, "peraop_pwm_fb1",
			"pwm_ck"/* parent */, 6),
	GATE_PERAO0_V(CLK_PERAOP_PWM_FB1_PWM, "peraop_pwm_fb1_pwm",
			"peraop_pwm_fb1"/* parent */),
	GATE_PERAO0(CLK_PERAOP_PWM_FB2, "peraop_pwm_fb2",
			"pwm_ck"/* parent */, 7),
	GATE_PERAO0_V(CLK_PERAOP_PWM_FB2_PWM, "peraop_pwm_fb2_pwm",
			"peraop_pwm_fb2"/* parent */),
	GATE_PERAO0(CLK_PERAOP_PWM_FB3, "peraop_pwm_fb3",
			"pwm_ck"/* parent */, 8),
	GATE_PERAO0_V(CLK_PERAOP_PWM_FB3_PWM, "peraop_pwm_fb3_pwm",
			"peraop_pwm_fb3"/* parent */),
	GATE_PERAO0(CLK_PERAOP_PWM_FB4, "peraop_pwm_fb4",
			"pwm_ck"/* parent */, 9),
	GATE_PERAO0_V(CLK_PERAOP_PWM_FB4_PWM, "peraop_pwm_fb4_pwm",
			"peraop_pwm_fb4"/* parent */),
	GATE_PERAO0(CLK_PERAOP_SPI0_B, "peraop_spi0_b",
			"spi0_b_ck"/* parent */, 12),
	GATE_PERAO0_V(CLK_PERAOP_SPI0_B_SPI, "peraop_spi0_b_spi",
			"peraop_spi0_b"/* parent */),
	GATE_PERAO0(CLK_PERAOP_SPI1_B, "peraop_spi1_b",
			"spi1_b_ck"/* parent */, 13),
	GATE_PERAO0_V(CLK_PERAOP_SPI1_B_SPI, "peraop_spi1_b_spi",
			"peraop_spi1_b"/* parent */),
	GATE_PERAO0(CLK_PERAOP_SPI2_B, "peraop_spi2_b",
			"spi2_b_ck"/* parent */, 14),
	GATE_PERAO0_V(CLK_PERAOP_SPI2_B_SPI, "peraop_spi2_b_spi",
			"peraop_spi2_b"/* parent */),
	GATE_PERAO0(CLK_PERAOP_SPI3_B, "peraop_spi3_b",
			"spi3_b_ck"/* parent */, 15),
	GATE_PERAO0_V(CLK_PERAOP_SPI3_B_SPI, "peraop_spi3_b_spi",
			"peraop_spi3_b"/* parent */),
	GATE_PERAO0(CLK_PERAOP_SPI4_B, "peraop_spi4_b",
			"spi4_b_ck"/* parent */, 16),
	GATE_PERAO0_V(CLK_PERAOP_SPI4_B_SPI, "peraop_spi4_b_spi",
			"peraop_spi4_b"/* parent */),
	GATE_PERAO0(CLK_PERAOP_SPI5_B, "peraop_spi5_b",
			"spi5_b_ck"/* parent */, 17),
	GATE_PERAO0_V(CLK_PERAOP_SPI5_B_SPI, "peraop_spi5_b_spi",
			"peraop_spi5_b"/* parent */),
	GATE_PERAO0(CLK_PERAOP_SPI6_B, "peraop_spi6_b",
			"spi6_b_ck"/* parent */, 18),
	GATE_PERAO0_V(CLK_PERAOP_SPI6_B_SPI, "peraop_spi6_b_spi",
			"peraop_spi6_b"/* parent */),
	GATE_PERAO0(CLK_PERAOP_SPI7_B, "peraop_spi7_b",
			"spi7_b_ck"/* parent */, 19),
	GATE_PERAO0_V(CLK_PERAOP_SPI7_B_SPI, "peraop_spi7_b_spi",
			"peraop_spi7_b"/* parent */),
	/* PERAO1 */
	GATE_PERAO1(CLK_PERAOP_DMA_B, "peraop_dma_b",
			"peri_faxi_ck"/* parent */, 1),
	GATE_PERAO1_V(CLK_PERAOP_DMA_B_UART, "peraop_dma_b_uart",
			"peraop_dma_b"/* parent */),
	GATE_PERAO1_V(CLK_PERAOP_DMA_B_I2C, "peraop_dma_b_i2c",
			"peraop_dma_b"/* parent */),
	GATE_PERAO1(CLK_PERAOP_SSUSB0_FRMCNT, "peraop_ssusb0_frmcnt",
			"ssusb_fmcnt_ck"/* parent */, 4),
	GATE_PERAO1_V(CLK_PERAOP_SSUSB0_FRMCNT_USB, "peraop_ssusb0_frmcnt_usb",
			"peraop_ssusb0_frmcnt"/* parent */),
	GATE_PERAO1(CLK_PERAOP_MSDC1, "peraop_msdc1",
			"msdc30_1_ck"/* parent */, 10),
	GATE_PERAO1_V(CLK_PERAOP_MSDC1_MSDC1, "peraop_msdc1_msdc1",
			"peraop_msdc1"/* parent */),
	GATE_PERAO1(CLK_PERAOP_MSDC1_F, "peraop_msdc1_f",
			"peri_faxi_ck"/* parent */, 11),
	GATE_PERAO1_V(CLK_PERAOP_MSDC1_F_MSDC1, "peraop_msdc1_f_msdc1",
			"peraop_msdc1_f"/* parent */),
	GATE_PERAO1(CLK_PERAOP_MSDC1_H, "peraop_msdc1_h",
			"peri_faxi_ck"/* parent */, 12),
	GATE_PERAO1_V(CLK_PERAOP_MSDC1_H_MSDC1, "peraop_msdc1_h_msdc1",
			"peraop_msdc1_h"/* parent */),
	GATE_PERAO1(CLK_PERAOP_MSDC2, "peraop_msdc2",
			"msdc30_1_ck"/* parent */, 13),
	GATE_PERAO1_V(CLK_PERAOP_MSDC2_MSDC2, "peraop_msdc2_msdc2",
			"peraop_msdc2"/* parent */),
	GATE_PERAO1(CLK_PERAOP_MSDC2_F, "peraop_msdc2_f",
			"peri_faxi_ck"/* parent */, 14),
	GATE_PERAO1_V(CLK_PERAOP_MSDC2_F_MSDC2, "peraop_msdc2_f_msdc2",
			"peraop_msdc2_f"/* parent */),
	GATE_PERAO1(CLK_PERAOP_MSDC2_H, "peraop_msdc2_h",
			"peri_faxi_ck"/* parent */, 15),
	GATE_PERAO1_V(CLK_PERAOP_MSDC2_H_MSDC2, "peraop_msdc2_h_msdc2",
			"peraop_msdc2_h"/* parent */),
	/* PERAO2 */
	GATE_PERAO2(CLK_PERAOP_AUDIO_SLV, "peraop_audio_slv",
			"peri_faxi_ck"/* parent */, 0),
	GATE_PERAO2_V(CLK_PERAOP_AUDIO_SLV_AFE, "peraop_audio_slv_afe",
			"peraop_audio_slv"/* parent */),
	GATE_PERAO2(CLK_PERAOP_AUDIO_MST, "peraop_audio_mst",
			"peri_faxi_ck"/* parent */, 1),
	GATE_PERAO2_V(CLK_PERAOP_AUDIO_MST_AFE, "peraop_audio_mst_afe",
			"peraop_audio_mst"/* parent */),
	GATE_PERAO2(CLK_PERAOP_AUDIO_INTBUS, "peraop_audio_intbus",
			"aud_intbus_ck"/* parent */, 2),
	GATE_PERAO2_V(CLK_PERAOP_AUDIO_INTBUS_AFE, "peraop_audio_intbus_afe",
			"peraop_audio_intbus"/* parent */),
};

static const struct mtk_clk_desc perao_mcd = {
	.clks = perao_clks,
	.num_clks = CLK_PERAO_NR_CLK,
};

static const struct mtk_gate_regs ufsao_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xC,
	.sta_ofs = 0x4,
};

#define GATE_UFSAO(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ufsao_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_UFSAO_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate ufsao_clks[] = {
	GATE_UFSAO(CLK_UFSAO_UNIPRO_SYS, "ufsao_unipro_sys",
			"ufs_ck"/* parent */, 3),
	GATE_UFSAO_V(CLK_UFSAO_UNIPRO_SYS_UFS, "ufsao_unipro_sys_ufs",
			"ufsao_unipro_sys"/* parent */),
	GATE_UFSAO(CLK_UFSAO_U_PHY_SAP, "ufsao_u_phy_sap",
			"vlp_infra_26m_ck"/* parent */, 8),
	GATE_UFSAO_V(CLK_UFSAO_U_PHY_SAP_UFS, "ufsao_u_phy_sap_ufs",
			"ufsao_u_phy_sap"/* parent */),
	GATE_UFSAO(CLK_UFSAO_U_PHY_TOP_AHB_S_BUSCK, "ufsao_u_phy_ahb_s_busck",
			"ufs_faxi_ck"/* parent */, 9),
	GATE_UFSAO_V(CLK_UFSAO_U_PHY_TOP_AHB_S_BUSCK_UFS, "ufsao_u_phy_ahb_s_busck_ufs",
			"ufsao_u_phy_ahb_s_busck"/* parent */),
	GATE_UFSAO(CLK_UFSAO_U_AO_FREE_26M, "ufsao_u_ao_26m_ck",
			"vlp_infra_26m_ck"/* parent */, 24),
	GATE_UFSAO_V(CLK_UFSAO_U_AO_FREE_26M_UFS, "ufsao_u_ao_26m_ck_ufs",
			"ufsao_u_ao_26m_ck"/* parent */),
};

static const struct mtk_clk_desc ufsao_mcd = {
	.clks = ufsao_clks,
	.num_clks = CLK_UFSAO_NR_CLK,
};

static const struct mtk_gate_regs ufspdn_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xC,
	.sta_ofs = 0x4,
};

#define GATE_UFSPDN(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ufspdn_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_UFSPDN_V(_id, _name, _parent) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate ufspdn_clks[] = {
	GATE_UFSPDN(CLK_UFSPDN_UFSHCI_UFS, "ufspdn_ufshci_ufs",
			"ufs_ck"/* parent */, 0),
	GATE_UFSPDN_V(CLK_UFSPDN_UFSHCI_UFS_UFS, "ufspdn_ufshci_ufs_ufs",
			"ufspdn_ufshci_ufs"/* parent */),
	GATE_UFSPDN(CLK_UFSPDN_UFSHCI_AES, "ufspdn_ufshci_aes",
			"aes_ufsfde_ck"/* parent */, 1),
	GATE_UFSPDN_V(CLK_UFSPDN_UFSHCI_AES_UFS, "ufspdn_ufshci_aes_ufs",
			"ufspdn_ufshci_aes"/* parent */),
	GATE_UFSPDN(CLK_UFSPDN_UFSHCI_U_AHB, "ufspdn_ufshci_u_ahb",
			"ufs_faxi_ck"/* parent */, 3),
	GATE_UFSPDN_V(CLK_UFSPDN_UFSHCI_U_AHB_UFS, "ufspdn_ufshci_u_ahb_ufs",
			"ufspdn_ufshci_u_ahb"/* parent */),
	GATE_UFSPDN(CLK_UFSPDN_UFSHCI_U_AXI, "ufspdn_ufshci_u_axi",
			"ufs_fmem_sub_ck"/* parent */, 5),
	GATE_UFSPDN_V(CLK_UFSPDN_UFSHCI_U_AXI_UFS, "ufspdn_ufshci_u_axi_ufs",
			"ufspdn_ufshci_u_axi"/* parent */),
	GATE_UFSPDN(CLK_UFSPDN_U_FREE_26M, "ufspdn_u_26m_ck",
			"vlp_infra_26m_ck"/* parent */, 6),
	GATE_UFSPDN_V(CLK_UFSPDN_U_FREE_26M_UFS, "ufspdn_u_26m_ck_ufs",
			"ufspdn_u_26m_ck"/* parent */),
};

static const struct mtk_clk_desc ufspdn_mcd = {
	.clks = ufspdn_clks,
	.num_clks = CLK_UFSPDN_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6899_peri[] = {
	{
		.compatible = "mediatek,mt6899-imp_iic_wrap_c",
		.data = &impc_mcd,
	}, {
		.compatible = "mediatek,mt6899-imp_iic_wrap_e",
		.data = &impe_mcd,
	}, {
		.compatible = "mediatek,mt6899-imp_iic_wrap_en",
		.data = &impen_mcd,
	}, {
		.compatible = "mediatek,mt6899-imp_iic_wrap_es",
		.data = &impes_mcd,
	}, {
		.compatible = "mediatek,mt6899-imp_iic_wrap_n",
		.data = &impn_mcd,
	}, {
		.compatible = "mediatek,mt6899-imp_iic_wrap_s",
		.data = &imps_mcd,
	}, {
		.compatible = "mediatek,mt6899-imp_iic_wrap_w",
		.data = &impw_mcd,
	}, {
		.compatible = "mediatek,mt6899-pericfg_ao",
		.data = &perao_mcd,
	}, {
		.compatible = "mediatek,mt6899-ufscfg_ao",
		.data = &ufsao_mcd,
	}, {
		.compatible = "mediatek,mt6899-ufscfg_pdn",
		.data = &ufspdn_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6899_peri_grp_probe(struct platform_device *pdev)
{
	int r;

#if MT_CCF_BRINGUP
	pr_notice("%s: %s init begin\n", __func__, pdev->name);
#endif

	r = mtk_clk_simple_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

#if MT_CCF_BRINGUP
	pr_notice("%s: %s init end\n", __func__, pdev->name);
#endif

	return r;
}

static struct platform_driver clk_mt6899_peri_drv = {
	.probe = clk_mt6899_peri_grp_probe,
	.driver = {
		.name = "clk-mt6899-peri",
		.of_match_table = of_match_clk_mt6899_peri,
	},
};

module_platform_driver(clk_mt6899_peri_drv);
MODULE_LICENSE("GPL");
