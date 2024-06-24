// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Author: KY Liu <ky.liu@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_opp.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

#include "scpsys.h"
#include "mtk-scpsys.h"

#include <dt-bindings/power/mt6899-power.h>

#define SCPSYS_BRINGUP			(0)
#if SCPSYS_BRINGUP
#define default_cap			(MTK_SCPD_BYPASS_OFF)
#else
#define default_cap			(0)
#endif

#define MT6899_TOP_AXI_PROT_EN_INFRASYS1_MD	(BIT(29) | BIT(30))
#define MT6899_EMI_NEMICFG_AO_MEM_PROT_REG_PROT_EN_GLITCH_MD	(BIT(6) | BIT(7))
#define MT6899_EMI_SEMICFG_AO_MEM_PROT_REG_PROT_EN_GLITCH_MD	(BIT(6) | BIT(7))
#define MT6899_TOP_AXI_PROT_EN_INFRASYS0_CONN	(BIT(25) | BIT(28))
#define MT6899_TOP_AXI_PROT_EN_CONNSYS0_CONN	(BIT(1))
#define MT6899_TOP_AXI_PROT_EN_INFRASYS0_CONN_2ND	(BIT(26))
#define MT6899_TOP_AXI_PROT_EN_CONNSYS0_CONN_2ND	(BIT(0))
#define MT6899_TOP_AXI_PROT_EN_PERISYS0_PERI_USB0	(BIT(9))
#define MT6899_TOP_AXI_PROT_EN_PERISYS0_PERI_AUDIO	(BIT(6))
#define MT6899_VLP_AXI_PROT_EN_ADSP_TOP	(BIT(14) | BIT(16) |  \
			BIT(18))
#define MT6899_VLP_AXI_PROT_EN_ADSP_TOP_2ND	(BIT(25))
#define MT6899_VLP_AXI_PROT_EN1_ADSP_TOP	(BIT(14) | BIT(20))
#define MT6899_TOP_AXI_PROT_EN_SSR0_ADSP_INFRA	(BIT(15))
#define MT6899_VLP_AXI_PROT_EN_ADSP_INFRA	(BIT(14) | BIT(16) |  \
			BIT(18) | BIT(23))
#define MT6899_VLP_AXI_PROT_EN1_ADSP_INFRA	(BIT(16))
#define MT6899_VLP_AXI_PROT_EN_ADSP_INFRA_2ND	(BIT(25))
#define MT6899_VLP_AXI_PROT_EN1_ADSP_INFRA_2ND	(BIT(14) | BIT(17))
#define MT6899_VLP_AXI_PROT_EN_ADSP_INFRA_3RD	(BIT(21) | BIT(22))
#define MT6899_VLP_AXI_PROT_EN1_ADSP_INFRA_3RD	(BIT(18) | BIT(21) |  \
			BIT(22))
#define MT6899_VLP_AXI_PROT_EN1_ADSP_AO	(BIT(16) | BIT(24))
#define MT6899_VLP_AXI_PROT_EN_ADSP_AO	(BIT(23))
#define MT6899_VLP_AXI_PROT_EN1_ADSP_AO_2ND	(BIT(17))
#define MT6899_VLP_AXI_PROT_EN_ADSP_AO_2ND	(BIT(21) | BIT(22))
#define MT6899_VLP_AXI_PROT_EN1_ADSP_AO_3RD	(BIT(18) | BIT(20) |  \
			BIT(21) | BIT(22))
#define MT6899_VLP_AXI_PROT_EN1_ADSP_AO_4RD	(BIT(23) | BIT(25))
#define MT6899_TOP_AXI_PROT_EN_SSR0_ADSP_AO	(BIT(20) | BIT(21))
#define MT6899_TOP_AXI_PROT_EN_MMSYS2_ISP_TRAW	(BIT(7))
#define MT6899_TOP_AXI_PROT_EN_MMSYS2_ISP_TRAW_2ND	(BIT(14))
#define MT6899_TOP_AXI_PROT_EN_MMSYS2_ISP_DIP1	(BIT(2))
#define MT6899_TOP_AXI_PROT_EN_MMSYS2_ISP_DIP1_2ND	(BIT(3))
#define MT6899_TOP_AXI_PROT_EN_MMSYS0_ISP_MAIN	(BIT(3))
#define MT6899_TOP_AXI_PROT_EN_MMSYS0_ISP_MAIN_2ND	(BIT(5))
#define MT6899_TOP_AXI_PROT_EN_MMSYS2_ISP_MAIN	(BIT(0))
#define MT6899_TOP_AXI_PROT_EN_MMSYS2_ISP_MAIN_2ND	(BIT(1))
#define MT6899_TOP_AXI_PROT_EN_MMSYS0_ISP_VCORE	(BIT(14))
#define MT6899_TOP_AXI_PROT_EN_MMSYS0_ISP_VCORE_2ND	(BIT(15) | BIT(25))
#define MT6899_TOP_AXI_PROT_EN_MMSYS0_VDE0	(BIT(10))
#define MT6899_TOP_AXI_PROT_EN_MMSYS0_VDE0_2ND	(BIT(11))
#define MT6899_TOP_AXI_PROT_EN_MMSYS0_VDE1	(BIT(20))
#define MT6899_TOP_AXI_PROT_EN_MMSYS0_VDE1_2ND	(BIT(21))
#define MT6899_TOP_AXI_PROT_EN_MMSYS0_VEN0	(BIT(12))
#define MT6899_TOP_AXI_PROT_EN_MMSYS1_VEN0	(BIT(14))
#define MT6899_TOP_AXI_PROT_EN_MMSYS0_VEN0_2ND	(BIT(13))
#define MT6899_TOP_AXI_PROT_EN_MMSYS1_VEN0_2ND	(BIT(15))
#define MT6899_TOP_AXI_PROT_EN_MMSYS0_VEN1	(BIT(22))
#define MT6899_TOP_AXI_PROT_EN_MMSYS1_VEN1	(BIT(16))
#define MT6899_TOP_AXI_PROT_EN_MMSYS0_VEN1_2ND	(BIT(23))
#define MT6899_TOP_AXI_PROT_EN_MMSYS1_VEN1_2ND	(BIT(17))
#define MT6899_TOP_AXI_PROT_EN_MMSYS0_CAM_MRAW	(BIT(7))
#define MT6899_TOP_AXI_PROT_EN_MMSYS0_CAM_MRAW_2ND	(BIT(26))
#define MT6899_TOP_AXI_PROT_EN_MMSYS2_CAM_SUBA	(BIT(9))
#define MT6899_TOP_AXI_PROT_EN_MMSYS2_CAM_SUBA_2ND	(BIT(6))
#define MT6899_TOP_AXI_PROT_EN_MMSYS0_CAM_SUBB	(BIT(9))
#define MT6899_TOP_AXI_PROT_EN_MMSYS2_CAM_SUBB	(BIT(8))
#define MT6899_TOP_AXI_PROT_EN_MMSYS2_CAM_SUBC	(BIT(11))
#define MT6899_TOP_AXI_PROT_EN_MMSYS2_CAM_SUBC_2ND	(BIT(10))
#define MT6899_TOP_AXI_PROT_EN_MMSYS0_CAM_MAIN	(BIT(16))
#define MT6899_TOP_AXI_PROT_EN_MMSYS2_CAM_MAIN	(BIT(4))
#define MT6899_TOP_AXI_PROT_EN_MMSYS2_CAM_VCORE	(BIT(5))
#define MT6899_TOP_AXI_PROT_EN_CCUSYS0_CAM_VCORE	(BIT(12))
#define MT6899_TOP_AXI_PROT_EN_INFRASYS0_CAM_VCORE	(BIT(27))
#define MT6899_TOP_AXI_PROT_EN_MMSYS0_CAM_VCORE	(BIT(17) | BIT(27) |  \
			BIT(31))
#define MT6899_TOP_AXI_PROT_EN_MMSYS1_CAM_VCORE	(BIT(19))
#define MT6899_TOP_AXI_PROT_EN_CCUSYS0_CAM_CCU	(BIT(9) | BIT(10))
#define MT6899_TOP_AXI_PROT_EN_CCUSYS0_CAM_CCU_2ND	(BIT(8) | BIT(11))
#define MT6899_TOP_AXI_PROT_EN_MMSYS1_DISP_VCORE	(BIT(23) | BIT(25) |  \
			BIT(27) | BIT(29))
#define MT6899_TOP_AXI_PROT_EN_MMSYS2_DISP_VCORE	(BIT(16) | BIT(17))
#define MT6899_TOP_AXI_PROT_EN_MMSYS3_DISP_VCORE	(BIT(22) | BIT(23) |  \
			BIT(24) | BIT(25) |  \
			BIT(26) | BIT(27))
#define MT6899_TOP_AXI_PROT_EN_MMSYS1_DISP_VCORE_2ND	(BIT(2) | BIT(10) |  \
			BIT(12) | BIT(13) |  \
			BIT(20) | BIT(22))
#define MT6899_TOP_AXI_PROT_EN_MMSYS0_DISP_VCORE	(BIT(1))
#define MT6899_TOP_AXI_PROT_EN_MMSYS1_DISP_VCORE_3RD	(BIT(1) | BIT(3) |  \
			BIT(11))
#define MT6899_TOP_AXI_PROT_EN_MMSYS2_DISP_VCORE_2ND	(BIT(13) | BIT(27))
#define MT6899_TOP_AXI_PROT_EN_MMSYS1_MML0	(BIT(0))
#define MT6899_TOP_AXI_PROT_EN_MMSYS2_MML0	(BIT(31))
#define MT6899_TOP_AXI_PROT_EN_MMSYS3_MML0	(BIT(16) | BIT(17))
#define MT6899_TOP_AXI_PROT_EN_MMSYS2_MML1	(BIT(29) | BIT(30))
#define MT6899_TOP_AXI_PROT_EN_MMSYS3_MML1	(BIT(18) | BIT(19))
#define MT6899_TOP_AXI_PROT_EN_MMSYS1_DIS0	(BIT(18) | BIT(21))
#define MT6899_TOP_AXI_PROT_EN_MMSYS3_DIS0	(BIT(20) | BIT(21))
#define MT6899_TOP_AXI_PROT_EN_MMSYS1_DIS1	(BIT(0) | BIT(18) |  \
			BIT(21))
#define MT6899_TOP_AXI_PROT_EN_MMSYS2_DIS1	(BIT(20) | BIT(21) |  \
			BIT(22) | BIT(23) |  \
			BIT(24) | BIT(25) |  \
			BIT(26) | BIT(28) |  \
			BIT(29) | BIT(30) |  \
			BIT(31))
#define MT6899_TOP_AXI_PROT_EN_MMSYS3_DIS1	(BIT(0) | BIT(1) |  \
			BIT(2) | BIT(3) |  \
			BIT(4) | BIT(5) |  \
			BIT(6) | BIT(7) |  \
			BIT(16) | BIT(17) |  \
			BIT(18) | BIT(19) |  \
			BIT(20) | BIT(21))
#define MT6899_TOP_AXI_PROT_EN_MMSYS1_DIS1_2ND	(BIT(23) | BIT(25) |  \
			BIT(27) | BIT(29))
#define MT6899_TOP_AXI_PROT_EN_MMSYS2_DIS1_2ND	(BIT(16) | BIT(17))
#define MT6899_TOP_AXI_PROT_EN_MMSYS3_DIS1_2ND	(BIT(22) | BIT(23) |  \
			BIT(24) | BIT(25) |  \
			BIT(26) | BIT(27))
#define MT6899_TOP_AXI_PROT_EN_MMSYS2_OVL0	(BIT(20) | BIT(21) |  \
			BIT(22) | BIT(23) |  \
			BIT(24) | BIT(25) |  \
			BIT(26) | BIT(28))
#define MT6899_TOP_AXI_PROT_EN_MMSYS3_OVL0	(BIT(0) | BIT(1) |  \
			BIT(2) | BIT(3) |  \
			BIT(4) | BIT(5) |  \
			BIT(6) | BIT(7))
#define MT6899_TOP_AXI_PROT_EN_INFRASYS1_MM_INFRA	(BIT(10))
#define MT6899_TOP_AXI_PROT_EN_MMSYS0_MM_INFRA	(BIT(0) | BIT(2) |  \
			BIT(4) | BIT(6))
#define MT6899_TOP_AXI_PROT_EN_MMSYS1_MM_INFRA	(BIT(4) | BIT(6))
#define MT6899_TOP_AXI_PROT_EN_MMSYS2_MM_INFRA	(BIT(12))
#define MT6899_TOP_AXI_PROT_EN_MMSYS3_MM_INFRA	(BIT(28) | BIT(29))
#define MT6899_TOP_AXI_PROT_EN_INFRASYS0_MM_INFRA	(BIT(8) | BIT(9))
#define MT6899_TOP_AXI_PROT_EN_MMSYS1_MM_INFRA_2ND	(BIT(9))
#define MT6899_TOP_AXI_PROT_EN_MMSYS2_MM_INFRA_2ND	(BIT(15))
#define MT6899_TOP_AXI_PROT_EN_MMSYS3_MM_INFRA_2ND	(BIT(30) | BIT(31))
#define MT6899_TOP_AXI_PROT_EN_EMISYS0_MM_INFRA	(BIT(21) | BIT(22))
#define MT6899_TOP_AXI_PROT_EN_EMISYS1_MM_INFRA	(BIT(21) | BIT(22))

enum regmap_type {
	INVALID_TYPE = 0,
	IFR_TYPE = 1,
	EMI_NEMICFG_AO_MEM_PROT_REG_TYPE = 2,
	EMI_SEMICFG_AO_MEM_PROT_REG_TYPE = 3,
	VLP_TYPE = 4,
	BUS_TYPE_NUM,
};

static const char *bus_list[BUS_TYPE_NUM] = {
	[IFR_TYPE] = "infra-ifrbus-ao-reg-bus",
	[EMI_NEMICFG_AO_MEM_PROT_REG_TYPE] = "emi-nemicfg-ao-mem-prot-reg-bus",
	[EMI_SEMICFG_AO_MEM_PROT_REG_TYPE] = "emi-semicfg-ao-mem-prot-reg-bus",
	[VLP_TYPE] = "vlpcfg",
};

/*
 * MT6899 power domain support
 */

static const struct scp_domain_data scp_domain_mt6899_spm_data[] = {
	[MT6899_POWER_DOMAIN_MD] = {
		.name = "md",
		.ctl_offs = 0xE00,
		.extb_iso_offs = 0xF18,
		.extb_iso_bits = 0x3,
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0x024, 0x028, 0x020, 0x02c,
				MT6899_TOP_AXI_PROT_EN_INFRASYS1_MD),
			BUS_PROT(EMI_NEMICFG_AO_MEM_PROT_REG_TYPE, 0x84, 0x88, 0x80, 0x8c,
				MT6899_EMI_NEMICFG_AO_MEM_PROT_REG_PROT_EN_GLITCH_MD),
			BUS_PROT(EMI_SEMICFG_AO_MEM_PROT_REG_TYPE, 0x84, 0x88, 0x80, 0x8c,
				MT6899_EMI_SEMICFG_AO_MEM_PROT_REG_PROT_EN_GLITCH_MD),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_MD_OPS | MTK_SCPD_BYPASS_INIT_ON | MTK_SCPD_REMOVE_MD_RSTB,
	},
	[MT6899_POWER_DOMAIN_CONN] = {
		.name = "conn",
		.ctl_offs = 0xE04,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x004, 0x008, 0x000, 0x00c,
				MT6899_TOP_AXI_PROT_EN_INFRASYS0_CONN),
			BUS_PROT_IGN(IFR_TYPE, 0x1c4, 0x1c8, 0x1c0, 0x1cc,
				MT6899_TOP_AXI_PROT_EN_CONNSYS0_CONN),
			BUS_PROT_IGN(IFR_TYPE, 0x004, 0x008, 0x000, 0x00c,
				MT6899_TOP_AXI_PROT_EN_INFRASYS0_CONN_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x1c4, 0x1c8, 0x1c0, 0x1cc,
				MT6899_TOP_AXI_PROT_EN_CONNSYS0_CONN_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6899_POWER_DOMAIN_PERI_USB0] = {
		.name = "peri-usb0",
		.ctl_offs = 0xE10,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0e4, 0x0e8, 0x0e0, 0x0ec,
				MT6899_TOP_AXI_PROT_EN_PERISYS0_PERI_USB0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6899_POWER_DOMAIN_PERI_AUDIO] = {
		.name = "peri-audio",
		.ctl_offs = 0xE14,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"peri_audio"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0e4, 0x0e8, 0x0e0, 0x0ec,
				MT6899_TOP_AXI_PROT_EN_PERISYS0_PERI_AUDIO),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6899_POWER_DOMAIN_ADSP_TOP_DORMANT] = {
		.name = "adsp-top-dormant",
		.ctl_offs = 0xE2C,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6899_VLP_AXI_PROT_EN_ADSP_TOP),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6899_VLP_AXI_PROT_EN_ADSP_TOP_2ND),
			BUS_PROT_IGN(VLP_TYPE, 0x0234, 0x0238, 0x0230, 0x0240,
				MT6899_VLP_AXI_PROT_EN1_ADSP_TOP),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON
				| default_cap,
	},
	[MT6899_POWER_DOMAIN_ADSP_INFRA] = {
		.name = "adsp-infra",
		.ctl_offs = 0xE30,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x804, 0x808, 0x800, 0x80c,
				MT6899_TOP_AXI_PROT_EN_SSR0_ADSP_INFRA),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6899_VLP_AXI_PROT_EN_ADSP_INFRA),
			BUS_PROT_IGN(VLP_TYPE, 0x0234, 0x0238, 0x0230, 0x0240,
				MT6899_VLP_AXI_PROT_EN1_ADSP_INFRA),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6899_VLP_AXI_PROT_EN_ADSP_INFRA_2ND),
			BUS_PROT_IGN(VLP_TYPE, 0x0234, 0x0238, 0x0230, 0x0240,
				MT6899_VLP_AXI_PROT_EN1_ADSP_INFRA_2ND),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6899_VLP_AXI_PROT_EN_ADSP_INFRA_3RD),
			BUS_PROT_IGN(VLP_TYPE, 0x0234, 0x0238, 0x0230, 0x0240,
				MT6899_VLP_AXI_PROT_EN1_ADSP_INFRA_3RD),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6899_POWER_DOMAIN_ADSP_AO] = {
		.name = "adsp-ao",
		.ctl_offs = 0xE34,
		.basic_clk_name = {"adsp_ao"},
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0234, 0x0238, 0x0230, 0x0240,
				MT6899_VLP_AXI_PROT_EN1_ADSP_AO),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6899_VLP_AXI_PROT_EN_ADSP_AO),
			BUS_PROT_IGN(VLP_TYPE, 0x0234, 0x0238, 0x0230, 0x0240,
				MT6899_VLP_AXI_PROT_EN1_ADSP_AO_2ND),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6899_VLP_AXI_PROT_EN_ADSP_AO_2ND),
			BUS_PROT_IGN(VLP_TYPE, 0x0234, 0x0238, 0x0230, 0x0240,
				MT6899_VLP_AXI_PROT_EN1_ADSP_AO_3RD),
			BUS_PROT_IGN(VLP_TYPE, 0x0234, 0x0238, 0x0230, 0x0240,
				MT6899_VLP_AXI_PROT_EN1_ADSP_AO_4RD),
			BUS_PROT_IGN(IFR_TYPE, 0x804, 0x808, 0x800, 0x80c,
				MT6899_TOP_AXI_PROT_EN_SSR0_ADSP_AO),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
};

static const struct scp_subdomain scp_subdomain_mt6899_spm[] = {
	{MT6899_POWER_DOMAIN_ADSP_INFRA, MT6899_POWER_DOMAIN_ADSP_TOP_DORMANT},
	{MT6899_POWER_DOMAIN_ADSP_AO, MT6899_POWER_DOMAIN_ADSP_INFRA},
};

static const struct scp_soc_data mt6899_spm_data = {
	.domains = scp_domain_mt6899_spm_data,
	.num_domains = MT6899_SPM_POWER_DOMAIN_NR,
	.subdomains = scp_subdomain_mt6899_spm,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt6899_spm),
	.regs = {
		.pwr_sta_offs = 0xF28,
		.pwr_sta2nd_offs = 0xF2C,
	}
};

static const struct scp_domain_data scp_domain_mt6899_hfrp_data[] = {
	[MT6899_POWER_DOMAIN_ISP_TRAW] = {
		.name = "isp-traw",
		.ctl_offs = 0xE40,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6899_TOP_AXI_PROT_EN_MMSYS2_ISP_TRAW),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6899_TOP_AXI_PROT_EN_MMSYS2_ISP_TRAW_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6899_POWER_DOMAIN_ISP_DIP1] = {
		.name = "isp-dip1",
		.ctl_offs = 0xE44,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6899_TOP_AXI_PROT_EN_MMSYS2_ISP_DIP1),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6899_TOP_AXI_PROT_EN_MMSYS2_ISP_DIP1_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6899_POWER_DOMAIN_ISP_MAIN] = {
		.name = "isp-main",
		.ctl_offs = 0xE48,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"isp_main_0", "isp_main_1"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6899_TOP_AXI_PROT_EN_MMSYS0_ISP_MAIN),
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6899_TOP_AXI_PROT_EN_MMSYS0_ISP_MAIN_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6899_TOP_AXI_PROT_EN_MMSYS2_ISP_MAIN),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6899_TOP_AXI_PROT_EN_MMSYS2_ISP_MAIN_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6899_POWER_DOMAIN_ISP_VCORE] = {
		.name = "isp-vcore",
		.ctl_offs = 0xE4C,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6899_TOP_AXI_PROT_EN_MMSYS0_ISP_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6899_TOP_AXI_PROT_EN_MMSYS0_ISP_VCORE_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6899_POWER_DOMAIN_VDE0] = {
		.name = "vde0",
		.ctl_offs = 0xE50,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"vde"},
		.subsys_clk_prefix = "vde0",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6899_TOP_AXI_PROT_EN_MMSYS0_VDE0),
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6899_TOP_AXI_PROT_EN_MMSYS0_VDE0_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6899_POWER_DOMAIN_VDE1] = {
		.name = "vde1",
		.ctl_offs = 0xE54,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"vde"},
		.subsys_clk_prefix = "vde1",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6899_TOP_AXI_PROT_EN_MMSYS0_VDE1),
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6899_TOP_AXI_PROT_EN_MMSYS0_VDE1_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6899_POWER_DOMAIN_VEN0] = {
		.name = "ven0",
		.ctl_offs = 0xE60,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ven"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6899_TOP_AXI_PROT_EN_MMSYS0_VEN0),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6899_TOP_AXI_PROT_EN_MMSYS1_VEN0),
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6899_TOP_AXI_PROT_EN_MMSYS0_VEN0_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6899_TOP_AXI_PROT_EN_MMSYS1_VEN0_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6899_POWER_DOMAIN_VEN1] = {
		.name = "ven1",
		.ctl_offs = 0xE64,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ven"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6899_TOP_AXI_PROT_EN_MMSYS0_VEN1),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6899_TOP_AXI_PROT_EN_MMSYS1_VEN1),
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6899_TOP_AXI_PROT_EN_MMSYS0_VEN1_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6899_TOP_AXI_PROT_EN_MMSYS1_VEN1_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6899_POWER_DOMAIN_CAM_MRAW] = {
		.name = "cam-mraw",
		.ctl_offs = 0xE6C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"cam_mraw"},
		.subsys_clk_prefix = "cam_mraw",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6899_TOP_AXI_PROT_EN_MMSYS0_CAM_MRAW),
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6899_TOP_AXI_PROT_EN_MMSYS0_CAM_MRAW_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6899_POWER_DOMAIN_CAM_SUBA] = {
		.name = "cam-suba",
		.ctl_offs = 0xE70,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"cam"},
		.subsys_clk_prefix = "cam_suba",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6899_TOP_AXI_PROT_EN_MMSYS2_CAM_SUBA),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6899_TOP_AXI_PROT_EN_MMSYS2_CAM_SUBA_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6899_POWER_DOMAIN_CAM_SUBB] = {
		.name = "cam-subb",
		.ctl_offs = 0xE74,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"cam"},
		.subsys_clk_prefix = "cam_subb",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6899_TOP_AXI_PROT_EN_MMSYS0_CAM_SUBB),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6899_TOP_AXI_PROT_EN_MMSYS2_CAM_SUBB),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6899_POWER_DOMAIN_CAM_SUBC] = {
		.name = "cam-subc",
		.ctl_offs = 0xE78,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"cam"},
		.subsys_clk_prefix = "cam_subc",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6899_TOP_AXI_PROT_EN_MMSYS2_CAM_SUBC),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6899_TOP_AXI_PROT_EN_MMSYS2_CAM_SUBC_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6899_POWER_DOMAIN_CAM_MAIN] = {
		.name = "cam-main",
		.ctl_offs = 0xE7C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"cam"},
		.subsys_clk_prefix = "cam_main",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6899_TOP_AXI_PROT_EN_MMSYS0_CAM_MAIN),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6899_TOP_AXI_PROT_EN_MMSYS2_CAM_MAIN),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6899_POWER_DOMAIN_CAM_VCORE] = {
		.name = "cam-vcore",
		.ctl_offs = 0xE80,
		.basic_clk_name = {"cam_vcore_0", "cam_vcore_1"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6899_TOP_AXI_PROT_EN_MMSYS2_CAM_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x264, 0x268, 0x260, 0x26c,
				MT6899_TOP_AXI_PROT_EN_CCUSYS0_CAM_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x004, 0x008, 0x000, 0x00c,
				MT6899_TOP_AXI_PROT_EN_INFRASYS0_CAM_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6899_TOP_AXI_PROT_EN_MMSYS0_CAM_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6899_TOP_AXI_PROT_EN_MMSYS1_CAM_VCORE),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6899_POWER_DOMAIN_CAM_CCU] = {
		.name = "cam-ccu",
		.ctl_offs = 0xE84,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"cam_ccu_0", "cam_ccu_1"},
		.subsys_clk_prefix = "cam_ccu",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x264, 0x268, 0x260, 0x26c,
				MT6899_TOP_AXI_PROT_EN_CCUSYS0_CAM_CCU),
			BUS_PROT_IGN(IFR_TYPE, 0x264, 0x268, 0x260, 0x26c,
				MT6899_TOP_AXI_PROT_EN_CCUSYS0_CAM_CCU_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6899_POWER_DOMAIN_CAM_CCU_AO] = {
		.name = "cam-ccu-ao",
		.ctl_offs = 0xE88,
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6899_POWER_DOMAIN_DISP_VCORE] = {
		.name = "disp-vcore",
		.ctl_offs = 0xE8C,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6899_TOP_AXI_PROT_EN_MMSYS1_DISP_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6899_TOP_AXI_PROT_EN_MMSYS2_DISP_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x2a4, 0x2a8, 0x2A0, 0x2ac,
				MT6899_TOP_AXI_PROT_EN_MMSYS3_DISP_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6899_TOP_AXI_PROT_EN_MMSYS1_DISP_VCORE_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6899_TOP_AXI_PROT_EN_MMSYS0_DISP_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6899_TOP_AXI_PROT_EN_MMSYS1_DISP_VCORE_3RD),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6899_TOP_AXI_PROT_EN_MMSYS2_DISP_VCORE_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6899_POWER_DOMAIN_MML0_SHUTDOWN] = {
		.name = "mml0-shutdown",
		.ctl_offs = 0xE90,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mml"},
		.subsys_clk_prefix = "mml0",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6899_TOP_AXI_PROT_EN_MMSYS1_MML0),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6899_TOP_AXI_PROT_EN_MMSYS2_MML0),
			BUS_PROT_IGN(IFR_TYPE, 0x2a4, 0x2a8, 0x2A0, 0x2ac,
				MT6899_TOP_AXI_PROT_EN_MMSYS3_MML0),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF
				| default_cap,
	},
	[MT6899_POWER_DOMAIN_MML1_SHUTDOWN] = {
		.name = "mml1-shutdown",
		.ctl_offs = 0xE94,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mml"},
		.subsys_clk_prefix = "mml1",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6899_TOP_AXI_PROT_EN_MMSYS2_MML1),
			BUS_PROT_IGN(IFR_TYPE, 0x2a4, 0x2a8, 0x2A0, 0x2ac,
				MT6899_TOP_AXI_PROT_EN_MMSYS3_MML1),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF
				| default_cap,
	},
	[MT6899_POWER_DOMAIN_DIS0_SHUTDOWN] = {
		.name = "dis0-shutdown",
		.ctl_offs = 0xE98,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"disp"},
		.subsys_clk_prefix = "dis0",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6899_TOP_AXI_PROT_EN_MMSYS1_DIS0),
			BUS_PROT_IGN(IFR_TYPE, 0x2a4, 0x2a8, 0x2A0, 0x2ac,
				MT6899_TOP_AXI_PROT_EN_MMSYS3_DIS0),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_IS_PWR_CON_ON
				| default_cap,
	},
	[MT6899_POWER_DOMAIN_DIS1_SHUTDOWN] = {
		.name = "dis1-shutdown",
		.ctl_offs = 0xE9C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"disp"},
		.subsys_clk_prefix = "dis1",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6899_TOP_AXI_PROT_EN_MMSYS1_DIS1),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6899_TOP_AXI_PROT_EN_MMSYS2_DIS1),
			BUS_PROT_IGN(IFR_TYPE, 0x2a4, 0x2a8, 0x2A0, 0x2ac,
				MT6899_TOP_AXI_PROT_EN_MMSYS3_DIS1),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6899_TOP_AXI_PROT_EN_MMSYS1_DIS1_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6899_TOP_AXI_PROT_EN_MMSYS2_DIS1_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x2a4, 0x2a8, 0x2A0, 0x2ac,
				MT6899_TOP_AXI_PROT_EN_MMSYS3_DIS1_2ND),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_IS_PWR_CON_ON
				| default_cap,
	},
	[MT6899_POWER_DOMAIN_OVL0_SHUTDOWN] = {
		.name = "ovl0-shutdown",
		.ctl_offs = 0xEA0,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"disp"},
		.subsys_clk_prefix = "ovl0",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6899_TOP_AXI_PROT_EN_MMSYS2_OVL0),
			BUS_PROT_IGN(IFR_TYPE, 0x2a4, 0x2a8, 0x2A0, 0x2ac,
				MT6899_TOP_AXI_PROT_EN_MMSYS3_OVL0),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_IS_PWR_CON_ON
				| default_cap,
	},
	[MT6899_POWER_DOMAIN_MM_INFRA] = {
		.name = "mm-infra",
		.ctl_offs = 0xEA8,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x024, 0x028, 0x020, 0x02c,
				MT6899_TOP_AXI_PROT_EN_INFRASYS1_MM_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6899_TOP_AXI_PROT_EN_MMSYS0_MM_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6899_TOP_AXI_PROT_EN_MMSYS1_MM_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6899_TOP_AXI_PROT_EN_MMSYS2_MM_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x2a4, 0x2a8, 0x2A0, 0x2ac,
				MT6899_TOP_AXI_PROT_EN_MMSYS3_MM_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x004, 0x008, 0x000, 0x00c,
				MT6899_TOP_AXI_PROT_EN_INFRASYS0_MM_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6899_TOP_AXI_PROT_EN_MMSYS1_MM_INFRA_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6899_TOP_AXI_PROT_EN_MMSYS2_MM_INFRA_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x2a4, 0x2a8, 0x2A0, 0x2ac,
				MT6899_TOP_AXI_PROT_EN_MMSYS3_MM_INFRA_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x124, 0x128, 0x120, 0x12c,
				MT6899_TOP_AXI_PROT_EN_EMISYS0_MM_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x104, 0x108, 0x100, 0x10c,
				MT6899_TOP_AXI_PROT_EN_EMISYS1_MM_INFRA),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6899_POWER_DOMAIN_DP_TX] = {
		.name = "dp-tx",
		.ctl_offs = 0xEB0,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6899_POWER_DOMAIN_CSI_RX] = {
		.name = "csi-rx",
		.ctl_offs = 0xEB4,
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
};

static const struct scp_subdomain scp_subdomain_mt6899_hfrp[] = {
	{MT6899_POWER_DOMAIN_ISP_MAIN, MT6899_POWER_DOMAIN_ISP_TRAW},
	{MT6899_POWER_DOMAIN_ISP_MAIN, MT6899_POWER_DOMAIN_ISP_DIP1},
	{MT6899_POWER_DOMAIN_ISP_VCORE, MT6899_POWER_DOMAIN_ISP_MAIN},
	{MT6899_POWER_DOMAIN_MM_INFRA, MT6899_POWER_DOMAIN_ISP_VCORE},
	{MT6899_POWER_DOMAIN_MM_INFRA, MT6899_POWER_DOMAIN_VDE0},
	{MT6899_POWER_DOMAIN_MM_INFRA, MT6899_POWER_DOMAIN_VDE1},
	{MT6899_POWER_DOMAIN_MM_INFRA, MT6899_POWER_DOMAIN_VEN0},
	{MT6899_POWER_DOMAIN_VEN0, MT6899_POWER_DOMAIN_VEN1},
	{MT6899_POWER_DOMAIN_CAM_MAIN, MT6899_POWER_DOMAIN_CAM_MRAW},
	{MT6899_POWER_DOMAIN_CAM_MAIN, MT6899_POWER_DOMAIN_CAM_SUBA},
	{MT6899_POWER_DOMAIN_CAM_MAIN, MT6899_POWER_DOMAIN_CAM_SUBB},
	{MT6899_POWER_DOMAIN_CAM_MAIN, MT6899_POWER_DOMAIN_CAM_SUBC},
	{MT6899_POWER_DOMAIN_CAM_VCORE, MT6899_POWER_DOMAIN_CAM_MAIN},
	{MT6899_POWER_DOMAIN_MM_INFRA, MT6899_POWER_DOMAIN_CAM_VCORE},
	{MT6899_POWER_DOMAIN_CAM_VCORE, MT6899_POWER_DOMAIN_CAM_CCU},
	{MT6899_POWER_DOMAIN_CAM_CCU, MT6899_POWER_DOMAIN_CAM_CCU_AO},
	{MT6899_POWER_DOMAIN_MM_INFRA, MT6899_POWER_DOMAIN_DISP_VCORE},
	{MT6899_POWER_DOMAIN_DIS1_SHUTDOWN, MT6899_POWER_DOMAIN_MML0_SHUTDOWN},
	{MT6899_POWER_DOMAIN_DIS1_SHUTDOWN, MT6899_POWER_DOMAIN_MML1_SHUTDOWN},
	{MT6899_POWER_DOMAIN_DIS1_SHUTDOWN, MT6899_POWER_DOMAIN_DIS0_SHUTDOWN},
	{MT6899_POWER_DOMAIN_DISP_VCORE, MT6899_POWER_DOMAIN_DIS1_SHUTDOWN},
	{MT6899_POWER_DOMAIN_DIS1_SHUTDOWN, MT6899_POWER_DOMAIN_OVL0_SHUTDOWN},
	{MT6899_POWER_DOMAIN_DIS1_SHUTDOWN, MT6899_POWER_DOMAIN_DP_TX},
};

static const struct scp_soc_data mt6899_hfrp_data = {
	.domains = scp_domain_mt6899_hfrp_data,
	.num_domains = MT6899_HFRP_POWER_DOMAIN_NR,
	.subdomains = scp_subdomain_mt6899_hfrp,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt6899_hfrp),
	.regs = {
		.pwr_sta_offs = 0xF28,
		.pwr_sta2nd_offs = 0xF2C,
	}
};

/*
 * scpsys driver init
 */

static const struct of_device_id of_scpsys_match_tbl[] = {
	{
		.compatible = "mediatek,mt6899-scpsys",
		.data = &mt6899_spm_data,
	}, {
		.compatible = "mediatek,mt6899-hfrpsys",
		.data = &mt6899_hfrp_data,
	}, {
		/* sentinel */
	}
};

static int mt6899_scpsys_probe(struct platform_device *pdev)
{
	const struct scp_subdomain *sd;
	const struct scp_soc_data *soc;
	struct scp *scp;
	struct genpd_onecell_data *pd_data;
	int i, ret;

	soc = of_device_get_match_data(&pdev->dev);
	if (!soc)
		return -EINVAL;

	scp = init_scp(pdev, soc->domains, soc->num_domains, &soc->regs, bus_list, BUS_TYPE_NUM);
	if (IS_ERR(scp))
		return PTR_ERR(scp);

	ret = mtk_register_power_domains(pdev, scp, soc->num_domains);
	if (ret)
		return ret;

	pd_data = &scp->pd_data;

	for (i = 0, sd = soc->subdomains; i < soc->num_subdomains; i++, sd++) {
		ret = pm_genpd_add_subdomain(pd_data->domains[sd->origin],
					     pd_data->domains[sd->subdomain]);
		if (ret && IS_ENABLED(CONFIG_PM)) {
			dev_err(&pdev->dev, "Failed to add subdomain: %d\n",
				ret);
			return ret;
		}
	}

	return 0;
}

static struct platform_driver mt6899_scpsys_drv = {
	.probe = mt6899_scpsys_probe,
	.driver = {
		.name = "mtk-scpsys-mt6899",
		.suppress_bind_attrs = true,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_scpsys_match_tbl),
	},
};

module_platform_driver(mt6899_scpsys_drv);
MODULE_LICENSE("GPL");
