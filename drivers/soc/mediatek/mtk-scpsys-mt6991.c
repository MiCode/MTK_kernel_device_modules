// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Chong-ming Wei <chong-ming.wei@mediatek.com>
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

#include <dt-bindings/power/mt6991-power.h>

#define SCPSYS_BRINGUP			(0)
#if SCPSYS_BRINGUP
#define default_cap			(MTK_SCPD_BYPASS_OFF)
#else
#define default_cap			(0)
#endif

#define MT6991_TOP_AXI_PROT_EN_SLEEP0_MD	(BIT(29))
#define MT6991_TOP_AXI_PROT_EN_SLEEP1_MD	(BIT(0))
#define MT6991_SPM_PROT_EN_BUS_CONN	(BIT(1))
#define MT6991_SPM_PROT_EN_BUS_SSUSB_DP_PHY_P0	(BIT(6))
#define MT6991_SPM_PROT_EN_BUS_SSUSB_P0	(BIT(7))
#define MT6991_SPM_PROT_EN_BUS_SSUSB_P1	(BIT(8))
#define MT6991_SPM_PROT_EN_BUS_SSUSB_P23	(BIT(9))
#define MT6991_SPM_PROT_EN_BUS_SSUSB_PHY_P2	(BIT(10))
#define MT6991_SPM_PROT_EN_BUS_PEXTP_MAC0	(BIT(13))
#define MT6991_SPM_PROT_EN_BUS_PEXTP_MAC1	(BIT(14))
#define MT6991_SPM_PROT_EN_BUS_PEXTP_MAC2	(BIT(15))
#define MT6991_SPM_PROT_EN_BUS_PEXTP_PHY0	(BIT(16))
#define MT6991_SPM_PROT_EN_BUS_PEXTP_PHY1	(BIT(17))
#define MT6991_SPM_PROT_EN_BUS_PEXTP_PHY2	(BIT(18))
#define MT6991_SPM_PROT_EN_BUS_AUDIO	(BIT(19))
#define MT6991_SPM_PROT_EN_BUS_ADSP_TOP	(BIT(21))
#define MT6991_SPM_PROT_EN_BUS_ADSP_INFRA	(BIT(22))
#define MT6991_SPM_PROT_EN_BUS_ADSP_AO	(BIT(23))
#define MT6991_SPM_PROT_EN_BUS_MM_PROC	(BIT(24))
#define MT6991_SPM_PROT_EN_BUS_SSRSYS	(BIT(10))
#define MT6991_SPM_PROT_EN_BUS_SPU_ISE	(BIT(11))
#define MT6991_SPM_PROT_EN_BUS_SPU_HWROT	(BIT(12))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_ISP_TRAW	(BIT(10))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_ISP_DIP	(BIT(11))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_ISP_MAIN	(BIT(12))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_ISP_VCORE	(BIT(0))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_ISP_WPE_EIS	(BIT(13))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_ISP_WPE_TNR	(BIT(14))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_ISP_WPE_LITE	(BIT(15))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_VDE0	(BIT(16))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_VDE1	(BIT(17))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_VDE_VCORE0	(BIT(1))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_VEN0	(BIT(2))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_VEN1	(BIT(18))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_VEN2	(BIT(19))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_MRAW	(BIT(20))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_RAWA	(BIT(21))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_RAWB	(BIT(22))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_RAWC	(BIT(23))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_RMSA	(BIT(24))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_RMSB	(BIT(25))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_RMSC	(BIT(26))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_MAIN	(BIT(27))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_VCORE	(BIT(3))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_CCU	(BIT(28))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_DISP_VCORE	(BIT(4))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_DIS0	(BIT(30))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_DIS1	(BIT(31))
#define MT6991_MMPC_PROT_EN_MMPC_BUS1_OVL0	(BIT(0))
#define MT6991_MMPC_PROT_EN_MMPC_BUS1_OVL1	(BIT(1))
#define MT6991_MMPC_PROT_EN_MMPC_BUS1_DISP_EDPTX	(BIT(2))
#define MT6991_MMPC_PROT_EN_MMPC_BUS1_DISP_DPTX	(BIT(3))
#define MT6991_MMPC_PROT_EN_MMPC_BUS1_MML0	(BIT(4))
#define MT6991_MMPC_PROT_EN_MMPC_BUS1_MML1	(BIT(5))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_MM_INFRA0	(BIT(5))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_MM_INFRA1	(BIT(6))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_MM_INFRA_AO	(BIT(7))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_CSI_BS_RX	(BIT(8))
#define MT6991_MMPC_PROT_EN_MMPC_BUS0_CSI_LS_RX	(BIT(9))

enum regmap_type {
	INVALID_TYPE = 0,
	IFR_TYPE = 1,
	SPM_TYPE = 2,
	MMPC_TYPE = 3,
	BUS_TYPE_NUM,
};

static const char *bus_list[BUS_TYPE_NUM] = {
	[IFR_TYPE] = "apifrbus-ao-io-reg-bus",
	[SPM_TYPE] = "spm",
	[MMPC_TYPE] = "mmpc",
};

/*
 * MT6991 power domain support
 */

static const struct scp_domain_data scp_domain_mt6991_spm_data[] = {
	[MT6991_POWER_DOMAIN_MD] = {
		.name = "md",
		.ctl_offs = 0xE00,
		.extb_iso_offs = 0xEFC,
		.extb_iso_bits = 0x3,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x4, 0x8, 0x0, 0xC,
				MT6991_TOP_AXI_PROT_EN_SLEEP0_MD),
			BUS_PROT_IGN(IFR_TYPE, 0x24, 0x28, 0x20, 0x2C,
				MT6991_TOP_AXI_PROT_EN_SLEEP1_MD),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_MD_OPS |
			MTK_SCPD_BYPASS_INIT_ON | MTK_SCPD_REMOVE_MD_RSTB,
	},
	[MT6991_POWER_DOMAIN_CONN] = {
		.name = "conn",
		.ctl_offs = 0xE04,
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_CONN),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6991_POWER_DOMAIN_SSUSB_DP_PHY_P0] = {
		.name = "ssusb-dp-phy-p0",
		.ctl_offs = 0xE18,
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_SSUSB_DP_PHY_P0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_SSUSB_P0] = {
		.name = "ssusb-p0",
		.ctl_offs = 0xE1C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_SSUSB_P0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_SSUSB_P1] = {
		.name = "ssusb-p1",
		.ctl_offs = 0xE20,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_SSUSB_P1),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_SSUSB_P23] = {
		.name = "ssusb-p23",
		.ctl_offs = 0xE24,
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_SSUSB_P23),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_SSUSB_PHY_P2] = {
		.name = "ssusb-phy-p2",
		.ctl_offs = 0xE28,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_SSUSB_PHY_P2),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_PEXTP_MAC0] = {
		.name = "pextp-mac0",
		.ctl_offs = 0xE34,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_PEXTP_MAC0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_PEXTP_PHY_RTFF | MTK_SCPD_RTFF_DELAY
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_PEXTP_MAC1] = {
		.name = "pextp-mac1",
		.ctl_offs = 0xE38,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_PEXTP_MAC1),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_PEXTP_PHY_RTFF | MTK_SCPD_RTFF_DELAY
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_PEXTP_MAC2] = {
		.name = "pextp-mac2",
		.ctl_offs = 0xE3C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_PEXTP_MAC2),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_PEXTP_PHY_RTFF | MTK_SCPD_RTFF_DELAY
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_PEXTP_PHY0] = {
		.name = "pextp-phy0",
		.ctl_offs = 0xE40,
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_PEXTP_PHY0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_PEXTP_PHY_RTFF | MTK_SCPD_RTFF_DELAY
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_PEXTP_PHY1] = {
		.name = "pextp-phy1",
		.ctl_offs = 0xE44,
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_PEXTP_PHY1),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_PEXTP_PHY_RTFF | MTK_SCPD_RTFF_DELAY
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_PEXTP_PHY2] = {
		.name = "pextp-phy2",
		.ctl_offs = 0xE48,
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_PEXTP_PHY2),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_PEXTP_PHY_RTFF | MTK_SCPD_RTFF_DELAY
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_AUDIO] = {
		.name = "audio",
		.ctl_offs = 0xE4C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_AUDIO),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_ADSP_TOP_DORMANT] = {
		.name = "adsp-top-dormant",
		.ctl_offs = 0xE54,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_ADSP_TOP),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_ADSP_INFRA] = {
		.name = "adsp-infra",
		.ctl_offs = 0xE58,
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_ADSP_INFRA),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_ADSP_AO] = {
		.name = "adsp-ao",
		.ctl_offs = 0xE5C,
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_ADSP_AO),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_MM_PROC_DORMANT] = {
		.name = "mm-proc-dormant",
		.ctl_offs = 0xE60,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xDC, 0xE0, 0xD8, 0x208,
				MT6991_SPM_PROT_EN_BUS_MM_PROC),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_SSR] = {
		.name = "ssrsys",
		.ctl_offs = 0xE84,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xE8, 0xEC, 0xE4, 0x20C,
				MT6991_SPM_PROT_EN_BUS_SSRSYS),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_SPU_ISE] = {
		.name = "spu-ise",
		.ctl_offs = 0xE88,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xE8, 0xEC, 0xE4, 0x20C,
				MT6991_SPM_PROT_EN_BUS_SPU_ISE),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_SPU_HWROT] = {
		.name = "spu-hwrot",
		.ctl_offs = 0xE8C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(SPM_TYPE, 0xE8, 0xEC, 0xE4, 0x20C,
				MT6991_SPM_PROT_EN_BUS_SPU_HWROT),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
};

static const struct scp_subdomain scp_subdomain_mt6991_spm[] = {
	{MT6991_POWER_DOMAIN_SSUSB_P0, MT6991_POWER_DOMAIN_SSUSB_DP_PHY_P0},
	{MT6991_POWER_DOMAIN_SSUSB_P23, MT6991_POWER_DOMAIN_SSUSB_PHY_P2},
	{MT6991_POWER_DOMAIN_PEXTP_MAC0, MT6991_POWER_DOMAIN_PEXTP_PHY0},
	{MT6991_POWER_DOMAIN_PEXTP_MAC1, MT6991_POWER_DOMAIN_PEXTP_PHY1},
	{MT6991_POWER_DOMAIN_PEXTP_MAC2, MT6991_POWER_DOMAIN_PEXTP_PHY2},
	{MT6991_POWER_DOMAIN_ADSP_INFRA, MT6991_POWER_DOMAIN_AUDIO},
	{MT6991_POWER_DOMAIN_ADSP_INFRA, MT6991_POWER_DOMAIN_ADSP_TOP_DORMANT},
	{MT6991_POWER_DOMAIN_ADSP_AO, MT6991_POWER_DOMAIN_ADSP_INFRA},
};

static const struct scp_soc_data mt6991_spm_data = {
	.domains = scp_domain_mt6991_spm_data,
	.num_domains = MT6991_SPM_POWER_DOMAIN_NR,
	.subdomains = scp_subdomain_mt6991_spm,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt6991_spm),
	.regs = {
		.pwr_sta_offs = 0xF14,
		.pwr_sta2nd_offs = 0xF18,
	}
};

static const struct scp_domain_data scp_domain_mt6991_mmpc_data[] = {
	[MT6991_POWER_DOMAIN_ISP_TRAW] = {
		.name = "isp-traw",
		.ctl_offs = 0x000,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_ISP_TRAW),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_ISP_DIP] = {
		.name = "isp-dip",
		.ctl_offs = 0x004,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_ISP_DIP),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_ISP_MAIN] = {
		.name = "isp-main",
		.ctl_offs = 0x008,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_ISP_MAIN),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_ISP_VCORE] = {
		.name = "isp-vcore",
		.ctl_offs = 0x00C,
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_ISP_VCORE),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_ISP_WPE_EIS] = {
		.name = "isp-wpe-eis",
		.ctl_offs = 0x010,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_ISP_WPE_EIS),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_ISP_WPE_TNR] = {
		.name = "isp-wpe-tnr",
		.ctl_offs = 0x014,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_ISP_WPE_TNR),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_ISP_WPE_LITE] = {
		.name = "isp-wpe-lite",
		.ctl_offs = 0x018,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_ISP_WPE_LITE),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_VDE0] = {
		.name = "vde0",
		.ctl_offs = 0x01C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_VDE0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_VDE1] = {
		.name = "vde1",
		.ctl_offs = 0x020,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_VDE1),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_VDE_VCORE0] = {
		.name = "vde-vcore0",
		.ctl_offs = 0x024,
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_VDE_VCORE0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_VEN0] = {
		.name = "ven0",
		.ctl_offs = 0x028,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_VEN0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_VEN1] = {
		.name = "ven1",
		.ctl_offs = 0x02C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_VEN1),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_VEN2] = {
		.name = "ven2",
		.ctl_offs = 0x030,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_VEN2),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_CAM_MRAW] = {
		.name = "cam-mraw",
		.ctl_offs = 0x034,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_MRAW),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_CAM_RAWA] = {
		.name = "cam-rawa",
		.ctl_offs = 0x038,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_RAWA),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_CAM_RAWB] = {
		.name = "cam-rawb",
		.ctl_offs = 0x03C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_RAWB),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_CAM_RAWC] = {
		.name = "cam-rawc",
		.ctl_offs = 0x040,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_RAWC),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_CAM_RMSA] = {
		.name = "cam-rmsa",
		.ctl_offs = 0x044,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_RMSA),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_CAM_RMSB] = {
		.name = "cam-rmsb",
		.ctl_offs = 0x048,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_RMSB),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_CAM_RMSC] = {
		.name = "cam-rmsc",
		.ctl_offs = 0x04C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_RMSC),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_CAM_MAIN] = {
		.name = "cam-main",
		.ctl_offs = 0x050,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_MAIN),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_CAM_VCORE] = {
		.name = "cam-vcore",
		.ctl_offs = 0x054,
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_VCORE),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_CAM_CCU] = {
		.name = "cam-ccu",
		.ctl_offs = 0x058,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_CAM_CCU),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_DISP_VCORE] = {
		.name = "disp-vcore",
		.ctl_offs = 0x060,
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_DISP_VCORE),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_DIS0_DORMANT] = {
		.name = "dis0-dormant",
		.ctl_offs = 0x064,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_DIS0),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_DIS1_DORMANT] = {
		.name = "dis1-dormant",
		.ctl_offs = 0x068,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_DIS1),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_OVL0_DORMANT] = {
		.name = "ovl0-dormant",
		.ctl_offs = 0x06C,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x184, 0x188, 0x180, 0x1B4,
				MT6991_MMPC_PROT_EN_MMPC_BUS1_OVL0),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_OVL1_DORMANT] = {
		.name = "ovl1-dormant",
		.ctl_offs = 0x070,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x184, 0x188, 0x180, 0x1B4,
				MT6991_MMPC_PROT_EN_MMPC_BUS1_OVL1),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_DISP_EDPTX_DORMANT] = {
		.name = "disp-edptx-dormant",
		.ctl_offs = 0x074,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x184, 0x188, 0x180, 0x1B4,
				MT6991_MMPC_PROT_EN_MMPC_BUS1_DISP_EDPTX),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_DISP_DPTX_DORMANT] = {
		.name = "disp-dptx-dormant",
		.ctl_offs = 0x078,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x184, 0x188, 0x180, 0x1B4,
				MT6991_MMPC_PROT_EN_MMPC_BUS1_DISP_DPTX),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_MML0_DORMANT] = {
		.name = "mml0-dormant",
		.ctl_offs = 0x07C,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x184, 0x188, 0x180, 0x1B4,
				MT6991_MMPC_PROT_EN_MMPC_BUS1_MML0),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_MML1_DORMANT] = {
		.name = "mml1-dormant",
		.ctl_offs = 0x080,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x184, 0x188, 0x180, 0x1B4,
				MT6991_MMPC_PROT_EN_MMPC_BUS1_MML1),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF
				| default_cap,
	},
	[MT6991_POWER_DOMAIN_MM_INFRA0] = {
		.name = "mm-infra0",
		.ctl_offs = 0x084,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_MM_INFRA0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_MM_INFRA1] = {
		.name = "mm-infra1",
		.ctl_offs = 0x088,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_MM_INFRA1),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_MM_INFRA_AO] = {
		.name = "mm-infra-ao",
		.ctl_offs = 0x08C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_MM_INFRA_AO),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_CSI_BS_RX] = {
		.name = "csi-bs-rx",
		.ctl_offs = 0x090,
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_CSI_BS_RX),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_CSI_LS_RX] = {
		.name = "csi-ls-rx",
		.ctl_offs = 0x094,
		.bp_table = {
			BUS_PROT_IGN(MMPC_TYPE, 0x174, 0x178, 0x170, 0x1B0,
				MT6991_MMPC_PROT_EN_MMPC_BUS0_CSI_LS_RX),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_DSI_PHY0] = {
		.name = "dsi-phy0",
		.ctl_offs = 0x0F0,
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_DSI_PHY1] = {
		.name = "dsi-phy1",
		.ctl_offs = 0x0F4,
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6991_POWER_DOMAIN_DSI_PHY2] = {
		.name = "dsi-phy2",
		.ctl_offs = 0x0F8,
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
};

static const struct scp_subdomain scp_subdomain_mt6991_mmpc[] = {
	{MT6991_POWER_DOMAIN_ISP_MAIN, MT6991_POWER_DOMAIN_ISP_TRAW},
	{MT6991_POWER_DOMAIN_ISP_MAIN, MT6991_POWER_DOMAIN_ISP_DIP},
	{MT6991_POWER_DOMAIN_ISP_VCORE, MT6991_POWER_DOMAIN_ISP_MAIN},
	{MT6991_POWER_DOMAIN_MM_INFRA1, MT6991_POWER_DOMAIN_ISP_VCORE},
	{MT6991_POWER_DOMAIN_ISP_MAIN, MT6991_POWER_DOMAIN_ISP_WPE_EIS},
	{MT6991_POWER_DOMAIN_ISP_MAIN, MT6991_POWER_DOMAIN_ISP_WPE_TNR},
	{MT6991_POWER_DOMAIN_ISP_MAIN, MT6991_POWER_DOMAIN_ISP_WPE_LITE},
	{MT6991_POWER_DOMAIN_VDE_VCORE0, MT6991_POWER_DOMAIN_VDE0},
	{MT6991_POWER_DOMAIN_VDE_VCORE0, MT6991_POWER_DOMAIN_VDE1},
	{MT6991_POWER_DOMAIN_MM_INFRA1, MT6991_POWER_DOMAIN_VDE_VCORE0},
	{MT6991_POWER_DOMAIN_MM_INFRA1, MT6991_POWER_DOMAIN_VEN0},
	{MT6991_POWER_DOMAIN_VEN0, MT6991_POWER_DOMAIN_VEN1},
	{MT6991_POWER_DOMAIN_VEN1, MT6991_POWER_DOMAIN_VEN2},
	{MT6991_POWER_DOMAIN_CAM_MAIN, MT6991_POWER_DOMAIN_CAM_MRAW},
	{MT6991_POWER_DOMAIN_CAM_MAIN, MT6991_POWER_DOMAIN_CAM_RAWA},
	{MT6991_POWER_DOMAIN_CAM_MAIN, MT6991_POWER_DOMAIN_CAM_RAWB},
	{MT6991_POWER_DOMAIN_CAM_MAIN, MT6991_POWER_DOMAIN_CAM_RAWC},
	{MT6991_POWER_DOMAIN_CAM_RAWA, MT6991_POWER_DOMAIN_CAM_RMSA},
	{MT6991_POWER_DOMAIN_CAM_RAWB, MT6991_POWER_DOMAIN_CAM_RMSB},
	{MT6991_POWER_DOMAIN_CAM_RAWC, MT6991_POWER_DOMAIN_CAM_RMSC},
	{MT6991_POWER_DOMAIN_CAM_VCORE, MT6991_POWER_DOMAIN_CAM_MAIN},
	{MT6991_POWER_DOMAIN_MM_INFRA1, MT6991_POWER_DOMAIN_CAM_VCORE},
	{MT6991_POWER_DOMAIN_CAM_VCORE, MT6991_POWER_DOMAIN_CAM_CCU},
	{MT6991_POWER_DOMAIN_MM_INFRA1, MT6991_POWER_DOMAIN_DISP_VCORE},
	{MT6991_POWER_DOMAIN_DISP_VCORE, MT6991_POWER_DOMAIN_DIS0_DORMANT},
	{MT6991_POWER_DOMAIN_DISP_VCORE, MT6991_POWER_DOMAIN_DIS1_DORMANT},
	{MT6991_POWER_DOMAIN_DISP_VCORE, MT6991_POWER_DOMAIN_OVL0_DORMANT},
	{MT6991_POWER_DOMAIN_DISP_VCORE, MT6991_POWER_DOMAIN_OVL1_DORMANT},
	{MT6991_POWER_DOMAIN_DISP_VCORE, MT6991_POWER_DOMAIN_DISP_EDPTX_DORMANT},
	{MT6991_POWER_DOMAIN_DISP_VCORE, MT6991_POWER_DOMAIN_DISP_DPTX_DORMANT},
	{MT6991_POWER_DOMAIN_DISP_VCORE, MT6991_POWER_DOMAIN_MML0_DORMANT},
	{MT6991_POWER_DOMAIN_DISP_VCORE, MT6991_POWER_DOMAIN_MML1_DORMANT},
	{MT6991_POWER_DOMAIN_MM_INFRA_AO, MT6991_POWER_DOMAIN_MM_INFRA0},
	{MT6991_POWER_DOMAIN_MM_INFRA0, MT6991_POWER_DOMAIN_MM_INFRA1},
	{MT6991_POWER_DOMAIN_MM_PROC_DORMANT, MT6991_POWER_DOMAIN_MM_INFRA_AO},
	{MT6991_POWER_DOMAIN_MM_PROC_DORMANT, MT6991_POWER_DOMAIN_CSI_BS_RX},
	{MT6991_POWER_DOMAIN_MM_PROC_DORMANT, MT6991_POWER_DOMAIN_CSI_LS_RX},
	{MT6991_POWER_DOMAIN_MM_PROC_DORMANT, MT6991_POWER_DOMAIN_DSI_PHY0},
	{MT6991_POWER_DOMAIN_MM_PROC_DORMANT, MT6991_POWER_DOMAIN_DSI_PHY1},
	{MT6991_POWER_DOMAIN_MM_PROC_DORMANT, MT6991_POWER_DOMAIN_DSI_PHY2},
};

static const struct scp_soc_data mt6991_mmpc_data = {
	.domains = scp_domain_mt6991_mmpc_data,
	.num_domains = MT6991_MMPC_POWER_DOMAIN_NR,
	.subdomains = scp_subdomain_mt6991_mmpc,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt6991_mmpc),
	.regs = {
		.pwr_sta_offs = 0xF14,
		.pwr_sta2nd_offs = 0xF18,
	}
};

/*
 * scpsys driver init
 */

static const struct of_device_id of_scpsys_match_tbl[] = {
	{
		.compatible = "mediatek,mt6991-scpsys",
		.data = &mt6991_spm_data,
	}, {
		.compatible = "mediatek,mt6991-hfrpsys",
		.data = &mt6991_mmpc_data,
	}, {
		/* sentinel */
	}
};

static int mt6991_scpsys_probe(struct platform_device *pdev)
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

static struct platform_driver mt6991_scpsys_drv = {
	.probe = mt6991_scpsys_probe,
	.driver = {
		.name = "mtk-scpsys-mt6991",
		.suppress_bind_attrs = true,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_scpsys_match_tbl),
	},
};

module_platform_driver(mt6991_scpsys_drv);
MODULE_LICENSE("GPL");
