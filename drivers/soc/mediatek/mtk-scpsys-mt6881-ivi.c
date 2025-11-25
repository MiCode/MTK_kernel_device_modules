// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: Qiqi Wang <qiqi.wang@mediatek.com>
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

#include <dt-bindings/power/mt6881-ivi-power.h>

#define SCPSYS_BRINGUP			(0)
#if SCPSYS_BRINGUP
#define default_cap			(MTK_SCPD_BYPASS_OFF)
#else
#define default_cap			(0)
#endif

#define MT6881_VLP_AXI_PROT_EN_PCIE	(BIT(24))
#define MT6881_VLP_AXI_PROT_EN_PCIE_2ND	(BIT(25))
#define MT6881_VLP_AXI_PROT_EN_PCIE_3RD	(BIT(23))
#define MT6881_VLP_AXI_PROT_EN_PCIE_4RD	(BIT(22))
#define MT6881_VLP_AXI_PROT_EN_PCIE_ACK	(BIT(26))
#define MT6881_VLP_AXI_PROT_EN_PCIE_ACK_2ND	(BIT(27))
#define MT6881_VLP_AXI_PROT_EN_PCIE_ACK_3RD	(BIT(25))
#define MT6881_VLP_AXI_PROT_EN_PCIE_ACK_4RD	(BIT(24))

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
 * mt6881 ivi power domain support
 */

static const struct scp_domain_data scp_domain_mt6881_ivi_spm_data[] = {
	[MT6881_POWER_DOMAIN_PCIE] = {
		.name = "pcie",
		.ctl_offs = 0xE68,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_ACK(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6881_VLP_AXI_PROT_EN_PCIE, MT6881_VLP_AXI_PROT_EN_PCIE_ACK),
			BUS_PROT_ACK(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6881_VLP_AXI_PROT_EN_PCIE_2ND, MT6881_VLP_AXI_PROT_EN_PCIE_ACK_2ND),
			BUS_PROT_ACK(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6881_VLP_AXI_PROT_EN_PCIE_3RD, MT6881_VLP_AXI_PROT_EN_PCIE_ACK_3RD),
			BUS_PROT_ACK(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6881_VLP_AXI_PROT_EN_PCIE_4RD, MT6881_VLP_AXI_PROT_EN_PCIE_ACK_4RD),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6881_POWER_DOMAIN_PCIE_PHY] = {
		.name = "pcie_phy",
		.ctl_offs = 0xE6C,
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
};

static const struct scp_soc_data mt6881_ivi_spm_data = {
	.domains = scp_domain_mt6881_ivi_spm_data,
	.num_domains = MT6881_IVI_SPM_POWER_DOMAIN_NR,
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
		.compatible = "mediatek,mt6881-ivi-scpsys",
		.data = &mt6881_ivi_spm_data,
	}, {
		/* sentinel */
	}
};

static int mt6881_ivi_scpsys_probe(struct platform_device *pdev)
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
			dev_info(&pdev->dev, "Failed to add subdomain: %d\n",
				ret);
			return ret;
		}
	}

	return 0;
}

static struct platform_driver mt6881_ivi_scpsys_drv = {
	.probe = mt6881_ivi_scpsys_probe,
	.driver = {
		.name = "mtk-scpsys-mt6881-ivi",
		.suppress_bind_attrs = true,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_scpsys_match_tbl),
	},
};

module_platform_driver(mt6881_ivi_scpsys_drv);
MODULE_LICENSE("GPL");
