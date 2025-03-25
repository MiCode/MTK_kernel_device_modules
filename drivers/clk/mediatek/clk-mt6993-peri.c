// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: Kuan-hsin.Lee <kuan-hsin.lee@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6993-clk.h>

#define MT_CCF_BRINGUP         0

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

#define GATE_IMPC_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

static const struct mtk_gate impc_clks[] = {
	GATE_IMPC(IMPC_I2C12, "impc_i2c12",
			"cksys_i2c_peri_ck"/* parent */, 0),
	GATE_IMPC_V(IMPC_I2C12_I2C, "impc_i2c12_i2c",
	        "impc_i2c12"/* parent */),
	GATE_IMPC(IMPC_I2C13, "impc_i2c13",
			"cksys_i2c_peri_ck"/* parent */, 2),
	GATE_IMPC_V(IMPC_I2C13_I2C, "impc_i2c13_i2c",
	        "impc_i2c13"/* parent */),
	GATE_IMPC(IMPC_I2C14, "impc_i2c14",
			"cksys_i2c_peri_ck"/* parent */, 3),
	GATE_IMPC_V(IMPC_I2C14_I2C, "impc_i2c14_i2c",
	        "impc_i2c14"/* parent */),
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

#define GATE_IMPE_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

static const struct mtk_gate impe_clks[] = {
	GATE_IMPE(IMPE_I2C5, "impe_i2c5",
			"cksys_i2c_east_ck"/* parent */, 0),
	GATE_IMPE_V(IMPE_I2C5_I2C, "impe_i2c5_i2c",
	        "impe_i2c5"/* parent */),
	GATE_IMPE(IMPE_I2C2, "impe_i2c2",
			"cksys_i2c_east_ck"/* parent */, 1),
	GATE_IMPE_V(IMPE_I2C2_I2C, "impe_i2c2_i2c",
	        "impe_i2c2"/* parent */),
	GATE_IMPE(IMPE_I2C4, "impe_i2c4",
			"cksys_i2c_east_ck"/* parent */, 2),
	GATE_IMPE_V(IMPE_I2C4_I2C, "impe_i2c4_i2c",
	        "impe_i2c4"/* parent */),
	GATE_IMPE(IMPE_I2C7, "impe_i2c7",
			"cksys_i2c_east_ck"/* parent */, 3),
	GATE_IMPE_V(IMPE_I2C7_I2C, "impe_i2c7_i2c",
	        "impe_i2c7"/* parent */),
	GATE_IMPE(IMPE_I2C8, "impe_i2c8",
			"cksys_i2c_east_ck"/* parent */, 4),
	GATE_IMPE_V(IMPE_I2C8_I2C, "impe_i2c8_i2c",
	        "impe_i2c8"/* parent */),
	GATE_IMPE(IMPE_I2C11, "impe_i2c11",
			"cksys_i2c_east_ck"/* parent */, 5),
	GATE_IMPE_V(IMPE_I2C11_I2C, "impe_i2c11_i2c",
	        "impe_i2c11"/* parent */),
};

static const struct mtk_clk_desc impe_mcd = {
	.clks = impe_clks,
	.num_clks = CLK_IMPE_NR_CLK,
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

#define GATE_IMPN_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

static const struct mtk_gate impn_clks[] = {
	GATE_IMPN(IMPN_I2C1, "impn_i2c1",
			"cksys_i2c_north_ck"/* parent */, 0),
	GATE_IMPN_V(IMPN_I2C1_I2C, "impn_i2c1_i2c",
	        "impn_i2c1"/* parent */),
	GATE_IMPN(IMPN_I2C9, "impn_i2c9",
			"cksys_i2c_north_ck"/* parent */, 1),
	GATE_IMPN_V(IMPN_I2C9_I2C, "impn_i2c9_i2c",
	        "impn_i2c9"/* parent */),
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

#define GATE_IMPS_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

static const struct mtk_gate imps_clks[] = {
	GATE_IMPS(IMPS_I2C0, "imps_i2c0",
			"cksys_i2c_west_ck"/* parent */, 0),
	GATE_IMPS_V(IMPS_I2C0_I2C, "imps_i2c0_i2c",
	        "imps_i2c0"/* parent */),
	GATE_IMPS(IMPS_I2C3, "imps_i2c3",
			"cksys_i2c_west_ck"/* parent */, 1),
	GATE_IMPS_V(IMPS_I2C3_I2C, "imps_i2c3_i2c",
	        "imps_i2c3"/* parent */),
	GATE_IMPS(IMPS_I2C6, "imps_i2c6",
			"cksys_i2c_west_ck"/* parent */, 2),
	GATE_IMPS_V(IMPS_I2C6_I2C, "imps_i2c6_i2c",
	        "imps_i2c6"/* parent */),
	GATE_IMPS(IMPS_I2C10, "imps_i2c10",
			"cksys_i2c_west_ck"/* parent */, 3),
	GATE_IMPS_V(IMPS_I2C10_I2C, "imps_i2c10_i2c",
	        "imps_i2c10"/* parent */),
};

static const struct mtk_clk_desc imps_mcd = {
	.clks = imps_clks,
	.num_clks = CLK_IMPS_NR_CLK,
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

static const struct mtk_gate_regs perao1_hwv_regs = {
	.set_ofs = 0x54,
	.clr_ofs = 0x58,
	.sta_ofs = 0x1261C,
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

#define GATE_PERAO0_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_PERAO1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &perao1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_PERAO1_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_HWV_PERAO1(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.hwv_comp = "hw-voter-regmap",				\
		.regs = &perao1_cg_regs,			\
		.hwv_regs = &perao1_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_generic_ap_hwv_ops,				\
		.dma_ops = &mtk_clk_gate_ops_setclr,			\
		.flags = CLK_USE_HW_VOTER,				\
	}

#define GATE_PERAO2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &perao2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_PERAO2_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

static const struct mtk_gate perao_clks[] = {
	/* PERAO0 */
	GATE_PERAO0(PERAO_U_UART0_BCLK, "perao_u_uart0_bclk",
			"cksys_uart_ck"/* parent */, 0),
	GATE_PERAO0_V(PERAO_U_UART0_BCLK_UART, "perao_u_uart0_bclk_uart",
	        "perao_u_uart0_bclk"/* parent */),
	GATE_PERAO0(PERAO_U_UART1_BCLK, "perao_u_uart1_bclk",
			"cksys_uart_ck"/* parent */, 1),
	GATE_PERAO0_V(PERAO_U_UART1_BCLK_UART, "perao_u_uart1_bclk_uart",
	        "perao_u_uart1_bclk"/* parent */),
	GATE_PERAO0(PERAO_U_UART2_BCLK, "perao_u_uart2_bclk",
			"cksys_uart_ck"/* parent */, 2),
	GATE_PERAO0_V(PERAO_U_UART2_BCLK_UART, "perao_u_uart2_bclk_uart",
	        "perao_u_uart2_bclk"/* parent */),
	GATE_PERAO0(PERAO_U_UART3_BCLK, "perao_u_uart3_bclk",
			"cksys_uart_ck"/* parent */, 3),
	GATE_PERAO0_V(PERAO_U_UART3_BCLK_UART, "perao_u_uart3_bclk_uart",
	        "perao_u_uart3_bclk"/* parent */),
	GATE_PERAO0(PERAO_U_UART4_BCLK, "perao_u_uart4_bclk",
			"cksys_uart_ck"/* parent */, 4),
	GATE_PERAO0_V(PERAO_U_UART4_BCLK_UART, "perao_u_uart4_bclk_uart",
	        "perao_u_uart4_bclk"/* parent */),
	GATE_PERAO0(PERAO_U_UART5_BCLK, "perao_u_uart5_bclk",
			"cksys_uart_ck"/* parent */, 5),
	GATE_PERAO0_V(PERAO_U_UART5_BCLK_UART, "perao_u_uart5_bclk_uart",
	        "perao_u_uart5_bclk"/* parent */),
	GATE_PERAO0(PERAO_U_PWM_X16W_HCLK, "perao_u_pwm_x16w",
			"cksys_peri_axi_peri_ck"/* parent */, 12),
	GATE_PERAO0_V(PERAO_U_PWM_X16W_HCLK_PWM, "perao_u_pwm_x16w_pwm",
	        "perao_u_pwm_x16w"/* parent */),
	GATE_PERAO0(PERAO_U_PWM_X16W_BCLK, "perao_u_pwm_x16w_bclk",
			"cksys_vlp_f26m_ck"/* parent */, 13),
	GATE_PERAO0_V(PERAO_U_PWM_X16W_BCLK_PWM, "perao_u_pwm_x16w_bclk_pwm",
	        "perao_u_pwm_x16w_bclk"/* parent */),
	GATE_PERAO0(PERAO_U_PWM_PWM_BCLK0, "perao_u_pwm_pwm_bclk0",
			"clk_null"/* parent */, 14),
	GATE_PERAO0_V(PERAO_U_PWM_PWM_BCLK0_PWM, "perao_u_pwm_pwm_bclk0_pwm",
	        "perao_u_pwm_pwm_bclk0"/* parent */),
	GATE_PERAO0(PERAO_U_PWM_PWM_BCLK1, "perao_u_pwm_pwm_bclk1",
			"clk_null"/* parent */, 15),
	GATE_PERAO0_V(PERAO_U_PWM_PWM_BCLK1_PWM, "perao_u_pwm_pwm_bclk1_pwm",
	        "perao_u_pwm_pwm_bclk1"/* parent */),
	GATE_PERAO0(PERAO_U_PWM_PWM_BCLK2, "perao_u_pwm_pwm_bclk2",
			"clk_null"/* parent */, 16),
	GATE_PERAO0_V(PERAO_U_PWM_PWM_BCLK2_PWM, "perao_u_pwm_pwm_bclk2_pwm",
	        "perao_u_pwm_pwm_bclk2"/* parent */),
	GATE_PERAO0(PERAO_U_PWM_PWM_BCLK3, "perao_u_pwm_pwm_bclk3",
			"clk_null"/* parent */, 17),
	GATE_PERAO0_V(PERAO_U_PWM_PWM_BCLK3_PWM, "perao_u_pwm_pwm_bclk3_pwm",
	        "perao_u_pwm_pwm_bclk3"/* parent */),
	GATE_PERAO0(PERAO_U_PWM_PWM_BCLK4, "perao_u_pwm_pwm_bclk4",
			"clk_null"/* parent */, 18),
	GATE_PERAO0_V(PERAO_U_PWM_PWM_BCLK4_PWM, "perao_u_pwm_pwm_bclk4_pwm",
	        "perao_u_pwm_pwm_bclk4"/* parent */),
	GATE_PERAO0(PERAO_U_PWM_PWM_BCLK5, "perao_u_pwm_pwm_bclk5",
			"clk_null"/* parent */, 19),
	GATE_PERAO0_V(PERAO_U_PWM_PWM_BCLK5_PWM, "perao_u_pwm_pwm_bclk5_pwm",
	        "perao_u_pwm_pwm_bclk5"/* parent */),
	GATE_PERAO0(PERAO_U_PWM_PWM_BCLK6, "perao_u_pwm_pwm_bclk6",
			"clk_null"/* parent */, 20),
	GATE_PERAO0_V(PERAO_U_PWM_PWM_BCLK6_PWM, "perao_u_pwm_pwm_bclk6_pwm",
	        "perao_u_pwm_pwm_bclk6"/* parent */),
	GATE_PERAO0(PERAO_U_PWM_PWM_BCLK7, "perao_u_pwm_pwm_bclk7",
			"clk_null"/* parent */, 21),
	GATE_PERAO0_V(PERAO_U_PWM_PWM_BCLK7_PWM, "perao_u_pwm_pwm_bclk7_pwm",
	        "perao_u_pwm_pwm_bclk7"/* parent */),
	GATE_PERAO0(PERAO_U_PWM_PWM_BCLK8, "perao_u_pwm_pwm_bclk8",
			"clk_null"/* parent */, 22),
	GATE_PERAO0_V(PERAO_U_PWM_PWM_BCLK8_PWM, "perao_u_pwm_pwm_bclk8_pwm",
	        "perao_u_pwm_pwm_bclk8"/* parent */),
	GATE_PERAO0(PERAO_U_PWM_PWM_BCLK9, "perao_u_pwm_pwm_bclk9",
			"clk_null"/* parent */, 23),
	GATE_PERAO0_V(PERAO_U_PWM_PWM_BCLK9_PWM, "perao_u_pwm_pwm_bclk9_pwm",
	        "perao_u_pwm_pwm_bclk9"/* parent */),
	/* PERAO1 */
	GATE_HWV_PERAO1(PERAO_U_SPI0_BCLK, "perao_u_spi0_bclk",
			"cksys_spi0_b_ck"/* parent */, 0),
	GATE_PERAO1_V(PERAO_U_SPI0_BCLK_SPI, "perao_u_spi0_bclk_spi",
	        "perao_u_spi0_bclk"/* parent */),
	GATE_HWV_PERAO1(PERAO_U_SPI1_BCLK, "perao_u_spi1_bclk",
			"cksys_spi1_b_ck"/* parent */, 2),
	GATE_PERAO1_V(PERAO_U_SPI1_BCLK_SPI, "perao_u_spi1_bclk_spi",
	        "perao_u_spi1_bclk"/* parent */),
	GATE_HWV_PERAO1(PERAO_U_SPI2_BCLK, "perao_u_spi2_bclk",
			"cksys_spi2_b_ck"/* parent */, 3),
	GATE_PERAO1_V(PERAO_U_SPI2_BCLK_SPI, "perao_u_spi2_bclk_spi",
	        "perao_u_spi2_bclk"/* parent */),
	GATE_HWV_PERAO1(PERAO_U_SPI3_BCLK, "perao_u_spi3_bclk",
			"cksys_spi3_b_ck"/* parent */, 4),
	GATE_PERAO1_V(PERAO_U_SPI3_BCLK_SPI, "perao_u_spi3_bclk_spi",
	        "perao_u_spi3_bclk"/* parent */),
	GATE_HWV_PERAO1(PERAO_U_SPI4_BCLK, "perao_u_spi4_bclk",
			"cksys_spi4_b_ck"/* parent */, 5),
	GATE_PERAO1_V(PERAO_U_SPI4_BCLK_SPI, "perao_u_spi4_bclk_spi",
	        "perao_u_spi4_bclk"/* parent */),
	GATE_HWV_PERAO1(PERAO_U_SPI5_BCLK, "perao_u_spi5_bclk",
			"cksys_spi5_b_ck"/* parent */, 6),
	GATE_PERAO1_V(PERAO_U_SPI5_BCLK_SPI, "perao_u_spi5_bclk_spi",
	        "perao_u_spi5_bclk"/* parent */),
	GATE_HWV_PERAO1(PERAO_U_SPI6_BCLK, "perao_u_spi6_bclk",
			"cksys_spi6_b_ck"/* parent */, 7),
	GATE_PERAO1_V(PERAO_U_SPI6_BCLK_SPI, "perao_u_spi6_bclk_spi",
	        "perao_u_spi6_bclk"/* parent */),
	GATE_HWV_PERAO1(PERAO_U_SPI7_BCLK, "perao_u_spi7_bclk",
			"cksys_spi7_b_ck"/* parent */, 8),
	GATE_PERAO1_V(PERAO_U_SPI7_BCLK_SPI, "perao_u_spi7_bclk_spi",
	        "perao_u_spi7_bclk"/* parent */),
	GATE_PERAO1(PERAO_U_AP_DMA_X32W_BCLK, "perao_u_ap_dma_x32w_bclk",
			"cksys_peri_axi_peri_ck"/* parent */, 26),
	GATE_PERAO1_V(PERAO_U_AP_DMA_X32W_BCLK_UART, "perao_u_ap_dma_x32w_bclk_uart",
	        "perao_u_ap_dma_x32w_bclk"/* parent */),
	GATE_PERAO1_V(PERAO_U_AP_DMA_X32W_BCLK_I2C, "perao_u_ap_dma_x32w_bclk_i2c",
	        "perao_u_ap_dma_x32w_bclk"/* parent */),
	/* PERAO2 */
	GATE_PERAO2(PERAO_U_MSDC1_MSDC_SRC, "perao_u_msdc1_msdc_src",
			"cksys_msdc30_1_src_ck"/* parent */, 1),
	GATE_PERAO2_V(PERAO_U_MSDC1_MSDC_SRC_MSDC, "perao_u_msdc1_msdc_src_msdc",
	        "perao_u_msdc1_msdc_src"/* parent */),
	GATE_PERAO2(PERAO_U_MSDC1_HCLK, "perao_u_msdc1",
			"cksys_msdc30_1_h_ck"/* parent */, 2),
	GATE_PERAO2_V(PERAO_U_MSDC1_HCLK_MSDC, "perao_u_msdc1_msdc",
	        "perao_u_msdc1"/* parent */),
	GATE_PERAO2(PERAO_U_MSDC1_AXI, "perao_u_msdc1_axi",
			"cksys_peri_axi_peri_ck"/* parent */, 3),
	GATE_PERAO2_V(PERAO_U_MSDC1_AXI_MSDC, "perao_u_msdc1_axi_msdc",
	        "perao_u_msdc1_axi"/* parent */),
	GATE_PERAO2(PERAO_U_MSDC1_HCLK_WRAP, "perao_u_msdc1_h_wrap",
			"cksys_peri_axi_peri_ck"/* parent */, 4),
	GATE_PERAO2_V(PERAO_U_MSDC1_HCLK_WRAP_MSDC, "perao_u_msdc1_h_wrap_msdc",
	        "perao_u_msdc1_h_wrap"/* parent */),
	GATE_PERAO2(PERAO_U_MSDC2_MSDC_SRC, "perao_u_msdc2_msdc_src",
			"cksys_msdc30_2_src_ck"/* parent */, 10),
	GATE_PERAO2_V(PERAO_U_MSDC2_MSDC_SRC_MSDC, "perao_u_msdc2_msdc_src_msdc",
	        "perao_u_msdc2_msdc_src"/* parent */),
	GATE_PERAO2(PERAO_U_MSDC2_HCLK, "perao_u_msdc2",
			"cksys_msdc30_2_h_ck"/* parent */, 11),
	GATE_PERAO2_V(PERAO_U_MSDC2_HCLK_MSDC, "perao_u_msdc2_msdc",
	        "perao_u_msdc2"/* parent */),
	GATE_PERAO2(PERAO_U_MSDC2_AXI, "perao_u_msdc2_axi",
			"cksys_peri_axi_peri_ck"/* parent */, 12),
	GATE_PERAO2_V(PERAO_U_MSDC2_AXI_MSDC, "perao_u_msdc2_axi_msdc",
	        "perao_u_msdc2_axi"/* parent */),
	GATE_PERAO2(PERAO_U_MSDC2_HCLK_WRAP, "perao_u_msdc2_h_wrap",
			"cksys_peri_axi_peri_ck"/* parent */, 13),
	GATE_PERAO2_V(PERAO_U_MSDC2_HCLK_WRAP_MSDC, "perao_u_msdc2_h_wrap_msdc",
	        "perao_u_msdc2_h_wrap"/* parent */),
};

static const struct mtk_clk_desc perao_mcd = {
	.clks = perao_clks,
	.num_clks = CLK_PERAO_NR_CLK,
};

static const struct mtk_gate_regs pextp0_cg_regs = {
	.set_ofs = 0x18,
	.clr_ofs = 0x1C,
	.sta_ofs = 0x14,
};

#define GATE_PEXTP0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &pextp0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_PEXTP0_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

static const struct mtk_gate pextp0_clks[] = {
	GATE_PEXTP0(PEXTP0_MAC0_TL, "pextp0_mac0_tl",
			"cksys_tl_ck"/* parent */, 0),
	GATE_PEXTP0_V(PEXTP0_MAC0_TL_PEXTP, "pextp0_mac0_tl_pextp",
	        "pextp0_mac0_tl"/* parent */),
	GATE_PEXTP0(PEXTP0_MAC0_REF, "pextp0_mac0_ref",
			"cksys_vlp_f26m_ck"/* parent */, 1),
	GATE_PEXTP0_V(PEXTP0_MAC0_REF_PEXTP, "pextp0_mac0_ref_pextp",
	        "pextp0_mac0_ref"/* parent */),
	GATE_PEXTP0(PEXTP0_PHY0_MCU_BUS, "pextp0_phy0_mcu_bus",
			"cksys_vlp_f26m_ck"/* parent */, 6),
	GATE_PEXTP0_V(PEXTP0_PHY0_MCU_BUS_PEXTP, "pextp0_phy0_mcu_bus_pextp",
	        "pextp0_phy0_mcu_bus"/* parent */),
	GATE_PEXTP0(PEXTP0_PHY0_PEXTP_REF, "pextp0_phy0_pextp_ref",
			"cksys_vlp_f26m_ck"/* parent */, 7),
	GATE_PEXTP0_V(PEXTP0_PHY0_PEXTP_REF_PEXTP, "pextp0_phy0_pextp_ref_pextp",
	        "pextp0_phy0_pextp_ref"/* parent */),
	GATE_PEXTP0(PEXTP0_MAC0_AXI_250, "pextp0_mac0_axi_250",
			"cksys_peri_m_pextp0_ck"/* parent */, 12),
	GATE_PEXTP0_V(PEXTP0_MAC0_AXI_250_PEXTP, "pextp0_mac0_axi_250_pextp",
	        "pextp0_mac0_axi_250"/* parent */),
	GATE_PEXTP0(PEXTP0_MAC0_AHB_APB, "pextp0_mac0_ahb_apb",
			"cksys_peri_axi_pextp0_ck"/* parent */, 13),
	GATE_PEXTP0_V(PEXTP0_MAC0_AHB_APB_PEXTP, "pextp0_mac0_ahb_apb_pextp",
	        "pextp0_mac0_ahb_apb"/* parent */),
	GATE_PEXTP0(PEXTP0_MAC0_PL_P, "pextp0_mac0_pl_p",
			"cksys_vlp_f26m_ck"/* parent */, 14),
	GATE_PEXTP0_V(PEXTP0_MAC0_PL_P_PEXTP, "pextp0_mac0_pl_p_pextp",
	        "pextp0_mac0_pl_p"/* parent */),
};

static const struct mtk_clk_desc pextp0_mcd = {
	.clks = pextp0_clks,
	.num_clks = CLK_PEXTP0_NR_CLK,
};

static const struct mtk_gate_regs pextp1_cg_regs = {
	.set_ofs = 0x18,
	.clr_ofs = 0x1C,
	.sta_ofs = 0x14,
};

#define GATE_PEXTP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &pextp1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_PEXTP1_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

static const struct mtk_gate pextp1_clks[] = {
	GATE_PEXTP1(PEXTP1_MAC1_TL, "pextp1_mac1_tl",
			"cksys_tl_p1_ck"/* parent */, 0),
	GATE_PEXTP1_V(PEXTP1_MAC1_TL_PEXTP, "pextp1_mac1_tl_pextp",
	        "pextp1_mac1_tl"/* parent */),
	GATE_PEXTP1(PEXTP1_MAC1_REF, "pextp1_mac1_ref",
			"cksys_vlp_f26m_ck"/* parent */, 1),
	GATE_PEXTP1_V(PEXTP1_MAC1_REF_PEXTP, "pextp1_mac1_ref_pextp",
	        "pextp1_mac1_ref"/* parent */),
	GATE_PEXTP1(PEXTP1_PHY1_MCU_BUS, "pextp1_phy1_mcu_bus",
			"cksys_vlp_f26m_ck"/* parent */, 8),
	GATE_PEXTP1_V(PEXTP1_PHY1_MCU_BUS_PEXTP, "pextp1_phy1_mcu_bus_pextp",
	        "pextp1_phy1_mcu_bus"/* parent */),
	GATE_PEXTP1(PEXTP1_PHY1_PEXTP_REF, "pextp1_phy1_pextp_ref",
			"cksys_vlp_f26m_ck"/* parent */, 9),
	GATE_PEXTP1_V(PEXTP1_PHY1_PEXTP_REF_PEXTP, "pextp1_phy1_pextp_ref_pextp",
	        "pextp1_phy1_pextp_ref"/* parent */),
	GATE_PEXTP1(PEXTP1_MAC1_AXI_250, "pextp1_mac1_axi_250",
			"cksys_peri_m_pextp0_ck"/* parent */, 16),
	GATE_PEXTP1_V(PEXTP1_MAC1_AXI_250_PEXTP, "pextp1_mac1_axi_250_pextp",
	        "pextp1_mac1_axi_250"/* parent */),
	GATE_PEXTP1(PEXTP1_MAC1_AHB_APB, "pextp1_mac1_ahb_apb",
			"cksys_peri_axi_pextp1_ck"/* parent */, 17),
	GATE_PEXTP1_V(PEXTP1_MAC1_AHB_APB_PEXTP, "pextp1_mac1_ahb_apb_pextp",
	        "pextp1_mac1_ahb_apb"/* parent */),
	GATE_PEXTP1(PEXTP1_MAC1_PL_P, "pextp1_mac1_pl_p",
			"cksys_vlp_f26m_ck"/* parent */, 18),
	GATE_PEXTP1_V(PEXTP1_MAC1_PL_P_PEXTP, "pextp1_mac1_pl_p_pextp",
	        "pextp1_mac1_pl_p"/* parent */),
};

static const struct mtk_clk_desc pextp1_mcd = {
	.clks = pextp1_clks,
	.num_clks = CLK_PEXTP1_NR_CLK,
};

static const struct mtk_gate_regs scp_fast_i3c_cg_regs = {
	.set_ofs = 0xE18,
	.clr_ofs = 0xE14,
	.sta_ofs = 0xE10,
};

#define GATE_SCP_FAST_I3C(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &scp_fast_i3c_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_SCP_FAST_I3C_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

static const struct mtk_gate scp_fast_i3c_clks[] = {
	GATE_SCP_FAST_I3C(SCP_FAST_I3C_0, "scp_fast_i3c_0",
			"cksys_vlp_ulposc_ck"/* parent */, 0),
	GATE_SCP_FAST_I3C_V(SCP_FAST_I3C_0_I2C, "scp_fast_i3c_0_i2c",
	        "scp_fast_i3c_0"/* parent */),
	GATE_SCP_FAST_I3C(SCP_FAST_I3C_1, "scp_fast_i3c_1",
			"cksys_vlp_ulposc_ck"/* parent */, 1),
	GATE_SCP_FAST_I3C_V(SCP_FAST_I3C_1_I2C, "scp_fast_i3c_1_i2c",
	        "scp_fast_i3c_1"/* parent */),
	GATE_SCP_FAST_I3C(SCP_FAST_I3C_2, "scp_fast_i3c_2",
			"cksys_vlp_ulposc_ck"/* parent */, 2),
	GATE_SCP_FAST_I3C_V(SCP_FAST_I3C_2_I2C, "scp_fast_i3c_2_i2c",
	        "scp_fast_i3c_2"/* parent */),
};

static const struct mtk_clk_desc scp_fast_i3c_mcd = {
	.clks = scp_fast_i3c_clks,
	.num_clks = CLK_SCP_FAST_I3C_NR_CLK,
};

static const struct mtk_gate_regs scp_i3c_cg_regs = {
	.set_ofs = 0xE18,
	.clr_ofs = 0xE14,
	.sta_ofs = 0xE10,
};

#define GATE_SCP_I3C(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &scp_i3c_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_SCP_I3C_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

static const struct mtk_gate scp_i3c_clks[] = {
	GATE_SCP_I3C(SCP_I3C_I2C0, "scp_i3c_i2c0",
			"cksys_vlp_ulposc_ck"/* parent */, 0),
	GATE_SCP_I3C_V(SCP_I3C_I2C0_SCP_I2C, "scp_i3c_i2c0_scp_i2c",
	        "scp_i3c_i2c0"/* parent */),
	GATE_SCP_I3C(SCP_I3C_I2C1, "scp_i3c_i2c1",
			"cksys_vlp_ulposc_ck"/* parent */, 1),
	GATE_SCP_I3C_V(SCP_I3C_I2C1_SCP_I2C, "scp_i3c_i2c1_scp_i2c",
	        "scp_i3c_i2c1"/* parent */),
	GATE_SCP_I3C(SCP_I3C_I2C2, "scp_i3c_i2c2",
			"cksys_vlp_ulposc_ck"/* parent */, 2),
	GATE_SCP_I3C_V(SCP_I3C_I2C2_SCP_I2C, "scp_i3c_i2c2_scp_i2c",
	        "scp_i3c_i2c2"/* parent */),
	GATE_SCP_I3C(SCP_I3C_I2C3, "scp_i3c_i2c3",
			"cksys_vlp_ulposc_ck"/* parent */, 3),
	GATE_SCP_I3C_V(SCP_I3C_I2C3_SCP_I2C, "scp_i3c_i2c3_scp_i2c",
	        "scp_i3c_i2c3"/* parent */),
	GATE_SCP_I3C(SCP_I3C_I2C4, "scp_i3c_i2c4",
			"cksys_vlp_ulposc_ck"/* parent */, 4),
	GATE_SCP_I3C_V(SCP_I3C_I2C4_SCP_I2C, "scp_i3c_i2c4_scp_i2c",
	        "scp_i3c_i2c4"/* parent */),
	GATE_SCP_I3C(SCP_I3C_I2C5, "scp_i3c_i2c5",
			"cksys_vlp_ulposc_ck"/* parent */, 5),
	GATE_SCP_I3C_V(SCP_I3C_I2C5_SCP_I2C, "scp_i3c_i2c5_scp_i2c",
	        "scp_i3c_i2c5"/* parent */),
	GATE_SCP_I3C(SCP_I3C_I2C6, "scp_i3c_i2c6",
			"cksys_vlp_ulposc_ck"/* parent */, 6),
	GATE_SCP_I3C_V(SCP_I3C_I2C6_SCP_I2C, "scp_i3c_i2c6_scp_i2c",
	        "scp_i3c_i2c6"/* parent */),
	GATE_SCP_I3C(SCP_I3C_I2C7, "scp_i3c_i2c7",
			"cksys_vlp_ulposc_ck"/* parent */, 7),
	GATE_SCP_I3C_V(SCP_I3C_I2C7_SCP_I2C, "scp_i3c_i2c7_scp_i2c",
	        "scp_i3c_i2c7"/* parent */),
};

static const struct mtk_clk_desc scp_i3c_mcd = {
	.clks = scp_i3c_clks,
	.num_clks = CLK_SCP_I3C_NR_CLK,
};

static const struct mtk_gate_regs ufs0ao0_cg_regs = {
	.set_ofs = 0x108,
	.clr_ofs = 0x10C,
	.sta_ofs = 0x104,
};

static const struct mtk_gate_regs ufs0ao1_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xC,
	.sta_ofs = 0x4,
};

#define GATE_UFS0AO0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ufs0ao0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_UFS0AO0_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_UFS0AO1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ufs0ao1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_UFS0AO1_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

static const struct mtk_gate ufs0ao_clks[] = {
	/* UFS0AO0 */
	GATE_UFS0AO0(UFS0AO_UFSHCI_UFS, "ufs0ao_ufshci_ufs",
			"cksys_u_0_ck"/* parent */, 0),
	GATE_UFS0AO0_V(UFS0AO_UFSHCI_UFS_UFS, "ufs0ao_ufshci_ufs_ufs",
	        "ufs0ao_ufshci_ufs"/* parent */),
	GATE_UFS0AO0(UFS0AO_UFSHCI_AES, "ufs0ao_ufshci_aes",
			"cksys_aes_ufsfde_0_ck"/* parent */, 1),
	GATE_UFS0AO0_V(UFS0AO_UFSHCI_AES_UFS, "ufs0ao_ufshci_aes_ufs",
	        "ufs0ao_ufshci_aes"/* parent */),
	/* UFS0AO1 */
	GATE_UFS0AO1(UFS0AO_UNIPRO_TX_SYM, "ufs0ao_unipro_tx_sym",
			"cksys_vlp_f26m_ck"/* parent */, 0),
	GATE_UFS0AO1_V(UFS0AO_UNIPRO_TX_SYM_UFS, "ufs0ao_unipro_tx_sym_ufs",
	        "ufs0ao_unipro_tx_sym"/* parent */),
	GATE_UFS0AO1(UFS0AO_UNIPRO_RX_SYM0, "ufs0ao_unipro_rx_sym0",
			"cksys_vlp_f26m_ck"/* parent */, 1),
	GATE_UFS0AO1_V(UFS0AO_UNIPRO_RX_SYM0_UFS, "ufs0ao_unipro_rx_sym0_ufs",
	        "ufs0ao_unipro_rx_sym0"/* parent */),
	GATE_UFS0AO1(UFS0AO_UNIPRO_RX_SYM1, "ufs0ao_unipro_rx_sym1",
			"cksys_vlp_f26m_ck"/* parent */, 2),
	GATE_UFS0AO1_V(UFS0AO_UNIPRO_RX_SYM1_UFS, "ufs0ao_unipro_rx_sym1_ufs",
	        "ufs0ao_unipro_rx_sym1"/* parent */),
	GATE_UFS0AO1(UFS0AO_UNIPRO_SYS, "ufs0ao_unipro_sys",
			"cksys_u_0_ck"/* parent */, 3),
	GATE_UFS0AO1_V(UFS0AO_UNIPRO_SYS_UFS, "ufs0ao_unipro_sys_ufs",
	        "ufs0ao_unipro_sys"/* parent */),
	GATE_UFS0AO1(UFS0AO_UNIPRO_SAP, "ufs0ao_unipro_sap",
			"cksys_vlp_f26m_ck"/* parent */, 4),
	GATE_UFS0AO1_V(UFS0AO_UNIPRO_SAP_UFS, "ufs0ao_unipro_sap_ufs",
	        "ufs0ao_unipro_sap"/* parent */),
	GATE_UFS0AO1(UFS0AO_U_PHY_SAP, "ufs0ao_u_phy_sap",
			"cksys_vlp_f26m_ck"/* parent */, 8),
	GATE_UFS0AO1_V(UFS0AO_U_PHY_SAP_UFS, "ufs0ao_u_phy_sap_ufs",
	        "ufs0ao_u_phy_sap"/* parent */),
	GATE_UFS0AO1(UFS0AO_U_PHY_TOP_AHB_S_BUSCK, "ufs0ao_u_phy_ahb_s_busck",
			"cksys_peri_axi_ufs0_ck"/* parent */, 9),
	GATE_UFS0AO1_V(UFS0AO_U_PHY_TOP_AHB_S_BUSCK_UFS, "ufs0ao_u_phy_ahb_s_busck_ufs",
	        "ufs0ao_u_phy_ahb_s_busck"/* parent */),
};

static const struct mtk_clk_desc ufs0ao_mcd = {
	.clks = ufs0ao_clks,
	.num_clks = CLK_UFS0AO_NR_CLK,
};

static const struct mtk_gate_regs ufs1ao0_cg_regs = {
	.set_ofs = 0x108,
	.clr_ofs = 0x10C,
	.sta_ofs = 0x104,
};

static const struct mtk_gate_regs ufs1ao1_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xC,
	.sta_ofs = 0x4,
};

#define GATE_UFS1AO0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ufs1ao0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_UFS1AO0_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

#define GATE_UFS1AO1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ufs1ao1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_UFS1AO1_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

static const struct mtk_gate ufs1ao_clks[] = {
	/* UFS1AO0 */
	GATE_UFS1AO0(UFS1AO_UFSHCI_UFS, "ufs1ao_ufshci_ufs",
			"cksys_u_1_ck"/* parent */, 0),
	GATE_UFS1AO0_V(UFS1AO_UFSHCI_UFS_UFS, "ufs1ao_ufshci_ufs_ufs",
	        "ufs1ao_ufshci_ufs"/* parent */),
	GATE_UFS1AO0(UFS1AO_UFSHCI_AES, "ufs1ao_ufshci_aes",
			"cksys_aes_ufsfde_1_ck"/* parent */, 1),
	GATE_UFS1AO0_V(UFS1AO_UFSHCI_AES_UFS, "ufs1ao_ufshci_aes_ufs",
	        "ufs1ao_ufshci_aes"/* parent */),
	/* UFS1AO1 */
	GATE_UFS1AO1(UFS1AO_UNIPRO_TX_SYM, "ufs1ao_unipro_tx_sym",
			"cksys_vlp_f26m_ck"/* parent */, 0),
	GATE_UFS1AO1_V(UFS1AO_UNIPRO_TX_SYM_UFS, "ufs1ao_unipro_tx_sym_ufs",
	        "ufs1ao_unipro_tx_sym"/* parent */),
	GATE_UFS1AO1(UFS1AO_UNIPRO_RX_SYM0, "ufs1ao_unipro_rx_sym0",
			"cksys_vlp_f26m_ck"/* parent */, 1),
	GATE_UFS1AO1_V(UFS1AO_UNIPRO_RX_SYM0_UFS, "ufs1ao_unipro_rx_sym0_ufs",
	        "ufs1ao_unipro_rx_sym0"/* parent */),
	GATE_UFS1AO1(UFS1AO_UNIPRO_RX_SYM1, "ufs1ao_unipro_rx_sym1",
			"cksys_vlp_f26m_ck"/* parent */, 2),
	GATE_UFS1AO1_V(UFS1AO_UNIPRO_RX_SYM1_UFS, "ufs1ao_unipro_rx_sym1_ufs",
	        "ufs1ao_unipro_rx_sym1"/* parent */),
	GATE_UFS1AO1(UFS1AO_UNIPRO_SYS, "ufs1ao_unipro_sys",
			"cksys_u_1_ck"/* parent */, 3),
	GATE_UFS1AO1_V(UFS1AO_UNIPRO_SYS_UFS, "ufs1ao_unipro_sys_ufs",
	        "ufs1ao_unipro_sys"/* parent */),
	GATE_UFS1AO1(UFS1AO_UNIPRO_SAP, "ufs1ao_unipro_sap",
			"cksys_vlp_f26m_ck"/* parent */, 4),
	GATE_UFS1AO1_V(UFS1AO_UNIPRO_SAP_UFS, "ufs1ao_unipro_sap_ufs",
	        "ufs1ao_unipro_sap"/* parent */),
	GATE_UFS1AO1(UFS1AO_U_PHY_SAP, "ufs1ao_u_phy_sap",
			"cksys_vlp_f26m_ck"/* parent */, 8),
	GATE_UFS1AO1_V(UFS1AO_U_PHY_SAP_UFS, "ufs1ao_u_phy_sap_ufs",
	        "ufs1ao_u_phy_sap"/* parent */),
	GATE_UFS1AO1(UFS1AO_U_PHY_TOP_AHB_S_BUSCK, "ufs1ao_u_phy_ahb_s_busck",
			"cksys_peri_axi_ufs1_ck"/* parent */, 9),
	GATE_UFS1AO1_V(UFS1AO_U_PHY_TOP_AHB_S_BUSCK_UFS, "ufs1ao_u_phy_ahb_s_busck_ufs",
	        "ufs1ao_u_phy_ahb_s_busck"/* parent */),
};

static const struct mtk_clk_desc ufs1ao_mcd = {
	.clks = ufs1ao_clks,
	.num_clks = CLK_UFS1AO_NR_CLK,
};

static const struct mtk_gate_regs usb_ao_cg_regs = {
	.set_ofs = 0x24,
	.clr_ofs = 0x28,
	.sta_ofs = 0x10,
};

#define GATE_USB_AO(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &usb_ao_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_USB_AO_V(_id, _name, _parent) {    \
        .id = _id,              \
        .name = _name,              \
        .parent_name = _parent,         \
    }

static const struct mtk_gate usb_ao_clks[] = {
	GATE_USB_AO(USB_AO_USB0_SSUSB0_FRMCNT, "usb_ao_usb0_ssusb0_frmcnt",
			"cksys_vlp_usb_vlpwire"/* parent */, 2),
	GATE_USB_AO_V(USB_AO_USB0_SSUSB0_FRMCNT_SSUSB, "usb_ao_usb0_ssusb0_frmcnt_ssusb",
	        "usb_ao_usb0_ssusb0_frmcnt"/* parent */),
};

static const struct mtk_clk_desc usb_ao_mcd = {
	.clks = usb_ao_clks,
	.num_clks = CLK_USB_AO_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6993_peri[] = {
	{
		.compatible = "mediatek,mt6993-imp_iic_wrap_c",
		.data = &impc_mcd,
	}, {
		.compatible = "mediatek,mt6993-imp_iic_wrap_e",
		.data = &impe_mcd,
	}, {
		.compatible = "mediatek,mt6993-imp_iic_wrap_n",
		.data = &impn_mcd,
	}, {
		.compatible = "mediatek,mt6993-imp_iic_wrap_s",
		.data = &imps_mcd,
	}, {
		.compatible = "mediatek,mt6993-pericfg_ao",
		.data = &perao_mcd,
	}, {
		.compatible = "mediatek,mt6993-pextp0cfg_ao",
		.data = &pextp0_mcd,
	}, {
		.compatible = "mediatek,mt6993-pextp1cfg_ao",
		.data = &pextp1_mcd,
	}, {
		.compatible = "mediatek,mt6993-scp_fast_i3c",
		.data = &scp_fast_i3c_mcd,
	}, {
		.compatible = "mediatek,mt6993-scp_i3c",
		.data = &scp_i3c_mcd,
	}, {
		.compatible = "mediatek,mt6993-ufs0cfg_ao",
		.data = &ufs0ao_mcd,
	}, {
		.compatible = "mediatek,mt6993-ufs1cfg_ao",
		.data = &ufs1ao_mcd,
	}, {
		.compatible = "mediatek,mt6993-usb0cfg_ao",
		.data = &usb_ao_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6993_peri_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6993_peri_drv = {
	.probe = clk_mt6993_peri_grp_probe,
	.driver = {
		.name = "clk-mt6993-peri",
		.of_match_table = of_match_clk_mt6993_peri,
	},
};

module_platform_driver(clk_mt6993_peri_drv);
MODULE_LICENSE("GPL");
