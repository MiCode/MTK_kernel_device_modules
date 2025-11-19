// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: Claude Yen <claude.yen@mediatek.com>
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

#include <dt-bindings/power/mt6881-power.h>

#define SCPSYS_BRINGUP			(1)
#if SCPSYS_BRINGUP
#define default_cap			(MTK_SCPD_BYPASS_OFF)
#else
#define default_cap			(0)
#endif

#define MT6881_TOP_AXI_PROT_EN_MCU_STA_0_CONN	(BIT(1))
#define MT6881_TOP_AXI_PROT_EN_INFRASYS_STA_1_CONN	(BIT(12))
#define MT6881_TOP_AXI_PROT_EN_MCU_STA_0_CONN_2ND	(BIT(0))
#define MT6881_TOP_AXI_PROT_EN_INFRASYS_STA_0_CONN	(BIT(8))
#define MT6881_TOP_AXI_PROT_EN_PERISYS_STA_0_AUDIO	(BIT(6))
#define MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_MAIN	(BIT(2))
#define MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_MAIN_2ND	(BIT(3))
#define MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_MAIN_3RD	(BIT(4))
#define MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_MAIN_4RD	(BIT(5))
#define MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_DIP1	(BIT(6))
#define MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_DIP1_2ND	(BIT(7))
#define MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_VCORE	(BIT(8))
#define MT6881_TOP_AXI_PROT_EN_MMSYS_STA_1_ISP_VCORE	(BIT(7) | BIT(8))
#define MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_VDE0	(BIT(20))
#define MT6881_TOP_AXI_PROT_EN_MMSYS_STA_1_VDE0	(BIT(13))
#define MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_VEN0	(BIT(12))
#define MT6881_TOP_AXI_PROT_EN_MMSYS_STA_1_VEN0	(BIT(12))
#define MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_MAIN	(BIT(29))
#define MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_MAIN_2ND	(BIT(30))
#define MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_SUBA	(BIT(25))
#define MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_SUBA_2ND	(BIT(26))
#define MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_SUBB	(BIT(27))
#define MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_SUBB_2ND	(BIT(28))
#define MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_VCORE	(BIT(31))
#define MT6881_TOP_AXI_PROT_EN_MMSYS_STA_1_CAM_VCORE	(BIT(9) | BIT(10))
#define MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_DIS0	(BIT(0) | BIT(1) |  \
			BIT(18))
#define MT6881_TOP_AXI_PROT_EN_MMSYS_STA_1_MM_INFRA	(BIT(1) | BIT(2) |  \
			BIT(3) | BIT(6))
#define MT6881_TOP_AXI_PROT_EN_INFRASYS_STA_1_MM_INFRA	(BIT(11))
#define MT6881_TOP_AXI_PROT_EN_MMSYS_STA_1_MM_INFRA_2ND	(BIT(0) | BIT(5) |  \
			BIT(7) | BIT(8) |  \
			BIT(9) | BIT(10) |  \
			BIT(11) | BIT(12) |  \
			BIT(13) | BIT(14) |  \
			BIT(15))
#define MT6881_TOP_AXI_PROT_EN_INFRASYS_STA_0_MM_INFRA	(BIT(16))
#define MT6881_VLP_AXI_PROT_EN_MM_PROC	(BIT(8))
#define MT6881_VLP_AXI_PROT_EN_MM_PROC_2ND	(BIT(9) | BIT(10))
#define MT6881_TOP_AXI_PROT_EN_PERISYS_STA_0_SSUSB	(BIT(7))

enum regmap_type {
	INVALID_TYPE = 0,
	IFR_TYPE = 1,
	VLP_TYPE = 2,
	BUS_TYPE_NUM,
};

static const char *bus_list[BUS_TYPE_NUM] = {
	[IFR_TYPE] = "infra-infracfg-ao-reg-bus",
	[VLP_TYPE] = "vlpcfg-reg-bus",
};

/*
 * MT6881 power domain support
 */

static const struct scp_domain_data scp_domain_mt6881_spm_data[] = {
	[MT6881_POWER_DOMAIN_CONN] = {
		.name = "conn",
		.ctl_offs = 0xE04,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C94, 0x0C98, 0x0C90, 0x0C9C,
				MT6881_TOP_AXI_PROT_EN_MCU_STA_0_CONN),
			BUS_PROT_IGN(IFR_TYPE, 0x0C54, 0x0C58, 0x0C50, 0x0C5C,
				MT6881_TOP_AXI_PROT_EN_INFRASYS_STA_1_CONN),
			BUS_PROT_IGN(IFR_TYPE, 0x0C94, 0x0C98, 0x0C90, 0x0C9C,
				MT6881_TOP_AXI_PROT_EN_MCU_STA_0_CONN_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C44, 0x0C48, 0x0C40, 0x0C4C,
				MT6881_TOP_AXI_PROT_EN_INFRASYS_STA_0_CONN),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6881_POWER_DOMAIN_AUDIO] = {
		.name = "audio",
		.ctl_offs = 0xE18,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		//.basic_clk_name = {"audio_0"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C84, 0x0C88, 0x0C80, 0x0C8C,
				MT6881_TOP_AXI_PROT_EN_PERISYS_STA_0_AUDIO),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6881_POWER_DOMAIN_ISP_MAIN] = {
		.name = "isp-main",
		.ctl_offs = 0xE28,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		//.basic_clk_name = {"img1_0", "ipe_0"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_MAIN),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_MAIN_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_MAIN_3RD),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_MAIN_4RD),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6881_POWER_DOMAIN_ISP_DIP1] = {
		.name = "isp-dip1",
		.ctl_offs = 0xE2C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		//.basic_clk_name = {"img1_0"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_DIP1),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_DIP1_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6881_POWER_DOMAIN_ISP_VCORE] = {
		.name = "isp-vcore",
		.ctl_offs = 0xE34,
		//.basic_clk_name = {"img1_0"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6881_TOP_AXI_PROT_EN_MMSYS_STA_1_ISP_VCORE),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6881_POWER_DOMAIN_VDE0] = {
		.name = "vde0",
		.ctl_offs = 0xE38,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		//.basic_clk_name = {"vde0_0"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_VDE0),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6881_TOP_AXI_PROT_EN_MMSYS_STA_1_VDE0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6881_POWER_DOMAIN_VEN0] = {
		.name = "ven0",
		.ctl_offs = 0xE40,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		//.basic_clk_name = {"ven0_0"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_VEN0),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6881_TOP_AXI_PROT_EN_MMSYS_STA_1_VEN0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6881_POWER_DOMAIN_CAM_MAIN] = {
		.name = "cam-main",
		.ctl_offs = 0xE48,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		//.basic_clk_name = {"cam_0"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_MAIN),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_MAIN_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6881_POWER_DOMAIN_CAM_SUBA] = {
		.name = "cam-suba",
		.ctl_offs = 0xE50,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		//.basic_clk_name = {"cam_0"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_SUBA),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_SUBA_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6881_POWER_DOMAIN_CAM_SUBB] = {
		.name = "cam-subb",
		.ctl_offs = 0xE54,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		//.basic_clk_name = {"cam_0"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_SUBB),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_SUBB_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6881_POWER_DOMAIN_CAM_VCORE] = {
		.name = "cam-vcore",
		.ctl_offs = 0xE5C,
		//.basic_clk_name = {"mm_infra_0"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6881_TOP_AXI_PROT_EN_MMSYS_STA_1_CAM_VCORE),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6881_POWER_DOMAIN_DIS0_SHUTDOWN] = {
		.name = "dis0-shutdown",
		.ctl_offs = 0xE70,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		//.basic_clk_name = {"dis0_0"},
		//.subsys_clk_prefix = "dis0",
		//.subsys_lp_clk_prefix = "dis0_lp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6881_TOP_AXI_PROT_EN_MMSYS_STA_0_DIS0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6881_POWER_DOMAIN_MM_INFRA_SHUTDOWN] = {
		.name = "mm-infra-shutdown",
		.ctl_offs = 0xE78,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		//.basic_clk_name = {"mm_infra_0"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6881_TOP_AXI_PROT_EN_MMSYS_STA_1_MM_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x0C54, 0x0C58, 0x0C50, 0x0C5C,
				MT6881_TOP_AXI_PROT_EN_INFRASYS_STA_1_MM_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6881_TOP_AXI_PROT_EN_MMSYS_STA_1_MM_INFRA_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C44, 0x0C48, 0x0C40, 0x0C4C,
				MT6881_TOP_AXI_PROT_EN_INFRASYS_STA_0_MM_INFRA),
		},
		.caps =  MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6881_POWER_DOMAIN_MM_PROC_DORMANT] = {
		.name = "mm-proc-dormant",
		.ctl_offs = 0xE7C,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		//.basic_clk_name = {"mm_proc_0"},
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6881_VLP_AXI_PROT_EN_MM_PROC),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6881_VLP_AXI_PROT_EN_MM_PROC_2ND),
		},
		.caps = MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6881_POWER_DOMAIN_CSI_RX] = {
		.name = "csi-rx",
		.ctl_offs = 0xE9C,
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6881_POWER_DOMAIN_SSUSB] = {
		.name = "ssusb",
		.ctl_offs = 0xEA8,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C84, 0x0C88, 0x0C80, 0x0C8C,
				MT6881_TOP_AXI_PROT_EN_PERISYS_STA_0_SSUSB),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6881_POWER_DOMAIN_APU] = {
		.name = "apu",
		.caps = MTK_SCPD_APU_OPS | MTK_SCPD_BYPASS_INIT_ON,
	},
};

static const struct scp_subdomain scp_subdomain_mt6881_spm[] = {
	{MT6881_POWER_DOMAIN_ISP_VCORE, MT6881_POWER_DOMAIN_ISP_MAIN},
	{MT6881_POWER_DOMAIN_ISP_MAIN, MT6881_POWER_DOMAIN_ISP_DIP1},
	{MT6881_POWER_DOMAIN_MM_INFRA_SHUTDOWN, MT6881_POWER_DOMAIN_ISP_VCORE},
	{MT6881_POWER_DOMAIN_MM_INFRA_SHUTDOWN, MT6881_POWER_DOMAIN_VDE0},
	{MT6881_POWER_DOMAIN_MM_INFRA_SHUTDOWN, MT6881_POWER_DOMAIN_VEN0},
	{MT6881_POWER_DOMAIN_CAM_VCORE, MT6881_POWER_DOMAIN_CAM_MAIN},
	{MT6881_POWER_DOMAIN_CAM_MAIN, MT6881_POWER_DOMAIN_CAM_SUBA},
	{MT6881_POWER_DOMAIN_CAM_MAIN, MT6881_POWER_DOMAIN_CAM_SUBB},
	{MT6881_POWER_DOMAIN_MM_INFRA_SHUTDOWN, MT6881_POWER_DOMAIN_CAM_VCORE},
	{MT6881_POWER_DOMAIN_MM_INFRA_SHUTDOWN, MT6881_POWER_DOMAIN_MM_PROC_DORMANT},
};

static const struct scp_soc_data mt6881_spm_data = {
	.domains = scp_domain_mt6881_spm_data,
	.num_domains = MT6881_SPM_POWER_DOMAIN_NR,
	.subdomains = scp_subdomain_mt6881_spm,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt6881_spm),
	.regs = {
		.pwr_sta_offs = 0xF40,
		.pwr_sta2nd_offs = 0xF44,
	}
};

/*
 * scpsys driver init
 */

static const struct of_device_id of_scpsys_match_tbl[] = {
	{
		.compatible = "mediatek,mt6881-scpsys",
		.data = &mt6881_spm_data,
	}, {
		/* sentinel */
	}
};

static int mt6881_scpsys_probe(struct platform_device *pdev)
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

static struct platform_driver mt6881_scpsys_drv = {
	.probe = mt6881_scpsys_probe,
	.driver = {
		.name = "mtk-scpsys-mt6881",
		.suppress_bind_attrs = true,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_scpsys_match_tbl),
	},
};

module_platform_driver(mt6881_scpsys_drv);
MODULE_LICENSE("GPL");
