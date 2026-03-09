// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
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

#include <dt-bindings/power/mt6781-power.h>

// Define MTCMOS Bus Protect Mask
#define DIS_PROT_STEP1_0_MASK            (BIT(11)|BIT(12))
#define DIS_PROT_STEP2_0_MASK            (BIT(1)|BIT(2) |BIT(10) |BIT(11))
#define MD1_PROT_STEP1_0_MASK            (BIT(7))
#define MD1_PROT_STEP2_0_MASK            (BIT(3)|BIT(4))
#define MD1_PROT_STEP2_1_MASK            (BIT(6))
#define CONN_PROT_STEP1_0_MASK           (BIT(18))
#define CONN_PROT_STEP2_0_MASK           (BIT(14))
#define CONN_PROT_STEP3_0_MASK           (BIT(13))
#define CONN_PROT_STEP4_0_MASK           (BIT(16))
#define MFG1_PROT_STEP1_0_MASK           (BIT(27)|BIT(28))
#define MFG1_PROT_STEP2_0_MASK           (BIT(21)|BIT(22))
#define MFG1_PROT_STEP3_0_MASK           (BIT(25))
#define MFG1_PROT_STEP4_0_MASK           (BIT(29))
#define ISP_PROT_STEP1_0_MASK            (BIT(23))
#define ISP_PROT_STEP2_0_MASK            (BIT(15))
#define IPE_PROT_STEP1_0_MASK            (BIT(24))
#define IPE_PROT_STEP2_0_MASK            (BIT(16))
#define VDE_PROT_STEP1_0_MASK            (BIT(30))
#define VDE_PROT_STEP2_0_MASK            (BIT(17))
#define VEN_PROT_STEP1_0_MASK            (BIT(31))
#define VEN_PROT_STEP2_0_MASK            (BIT(19))
#define CAM_PROT_STEP1_0_MASK            (BIT(21)|BIT(22))
#define CAM_PROT_STEP2_0_MASK            (BIT(13)|BIT(14))

enum regmap_type {
	INVALID_TYPE = 0,
	IFR_TYPE,
	BUS_TYPE_NUM,
};

static const char *bus_list[BUS_TYPE_NUM] = {
	[IFR_TYPE] = "infracfg",
};


static const struct scp_domain_data scp_domain_data_MT6781[] = {

	[MT6781_POWER_DOMAIN_MD] = {
		.name = "md",
		.sta_mask = BIT(0),
		.ctl_offs = 0x300,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},

	[MT6781_POWER_DOMAIN_CONN] = {
		.name = "conn",
		.sta_mask = BIT(1),
		.ctl_offs = 0x304,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0250, 0x0258,
				CONN_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				CONN_PROT_STEP2_0_MASK),
		},
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},

	[MT6781_POWER_DOMAIN_DIS] = {
		.name = "disp",
		.sta_mask = BIT(21),
		.ctl_offs = 0x0354,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"disp"},
		.subsys_clk_prefix = "disp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0250, 0x0258,
				DIS_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				DIS_PROT_STEP2_0_MASK),
		},
	},

	[MT6781_POWER_DOMAIN_ISP] = {
		.name = "isp",
		.sta_mask = BIT(13),
		.ctl_offs = 0x0334,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"isp"},
		.subsys_clk_prefix = "isp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0250, 0x0258,
				ISP_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0250, 0x0258,
				ISP_PROT_STEP2_0_MASK),
		},
	},

	[MT6781_POWER_DOMAIN_ISP2] = {
		.name = "isp2",
		.sta_mask = BIT(14),
		.ctl_offs = 0x338,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),

	},

	[MT6781_POWER_DOMAIN_IPE] = {
		.name = "ipe",
		.sta_mask = BIT(15),
		.ctl_offs = 0x33C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ipe"},
		.subsys_clk_prefix = "ipe",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0250, 0x0258,
				IPE_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0250, 0x0258,
				IPE_PROT_STEP2_0_MASK),
		},
	},


	[MT6781_POWER_DOMAIN_VDE] = {
		.name = "vde",
		.sta_mask = BIT(16),
		.ctl_offs = 0x0340,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"vde"},
		.subsys_clk_prefix = "vde",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE,  0x02A8, 0x02AC, 0x0250, 0x0258,
				VDE_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE,  0x02A8, 0x02AC, 0x0250, 0x0258,
				VDE_PROT_STEP2_0_MASK),
		},

	},

	[MT6781_POWER_DOMAIN_VEN] = {
		.name = "ven",
		.sta_mask = BIT(18),
		.ctl_offs = 0x0348,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ven"},
		.subsys_clk_prefix = "ven",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0250, 0x0258,
				VEN_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0250, 0x0258,
				VEN_PROT_STEP2_0_MASK),
		},
	},

	[MT6781_POWER_DOMAIN_CAM] = {
		.name = "cam",
		.sta_mask = BIT(23),
		.ctl_offs = 0x035C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"cam"},
		.subsys_clk_prefix = "cam",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0250, 0x0258,
				CAM_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0250, 0x0258,
				CAM_PROT_STEP2_0_MASK),
		},
	},

	[MT6781_POWER_DOMAIN_CAM_RAWA] = {
		.name = "cam_rawa",
		.sta_mask = BIT(24),
		.ctl_offs = 0x360,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},

	[MT6781_POWER_DOMAIN_CAM_RAWB] = {
		.name = "cam_rawb",
		.sta_mask = BIT(25),
		.ctl_offs = 0x364,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},

	[MT6781_POWER_DOMAIN_MFG0] = {
		.name = "mfg0",
		.sta_mask = BIT(2),
		.ctl_offs = 0x308,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},

	[MT6781_POWER_DOMAIN_MFG1] = {
		.name = "mfg1",
		.sta_mask = BIT(3),
		.ctl_offs = 0x30C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},

	[MT6781_POWER_DOMAIN_MFG2] = {
		.name = "mfg2",
		.sta_mask = BIT(4),
		.ctl_offs = 0x310,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},

	[MT6781_POWER_DOMAIN_MFG3] = {
		.name = "mfg3",
		.sta_mask = BIT(5),
		.ctl_offs = 0x314,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},

	[MT6781_POWER_DOMAIN_CSI] = {
		.name = "csi",
		.sta_mask = BIT(6),
		.ctl_offs = 0x318,
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},
};

static const struct scp_subdomain scp_subdomain_MT6781[] = {
	{MT6781_POWER_DOMAIN_DIS, MT6781_POWER_DOMAIN_CAM},
	{MT6781_POWER_DOMAIN_DIS, MT6781_POWER_DOMAIN_ISP},
	{MT6781_POWER_DOMAIN_DIS, MT6781_POWER_DOMAIN_ISP2},
	{MT6781_POWER_DOMAIN_DIS, MT6781_POWER_DOMAIN_IPE},
	{MT6781_POWER_DOMAIN_DIS, MT6781_POWER_DOMAIN_VDE},
	{MT6781_POWER_DOMAIN_DIS, MT6781_POWER_DOMAIN_VEN},
	{MT6781_POWER_DOMAIN_CAM, MT6781_POWER_DOMAIN_CAM_RAWA},
	{MT6781_POWER_DOMAIN_CAM, MT6781_POWER_DOMAIN_CAM_RAWB},
	{MT6781_POWER_DOMAIN_MFG0, MT6781_POWER_DOMAIN_MFG1},
	{MT6781_POWER_DOMAIN_MFG1, MT6781_POWER_DOMAIN_MFG2},
	{MT6781_POWER_DOMAIN_MFG2, MT6781_POWER_DOMAIN_MFG3},
};

static const struct scp_soc_data MT6781_data = {
	.domains = scp_domain_data_MT6781,
	.num_domains = MT6781_POWER_DOMAIN_NR,
	.subdomains = scp_subdomain_MT6781,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_MT6781),
	.regs = {
		.pwr_sta_offs = 0x016C,
		.pwr_sta2nd_offs = 0x0170
	}
};

/*
 * scpsys driver init
 */

static const struct of_device_id of_scpsys_match_tbl[] = {
	{
		.compatible = "mediatek,mt6781-scpsys",
		.data = &MT6781_data,
	}, {
		/* sentinel */
	}
};

static int mt6781_scpsys_probe(struct platform_device *pdev)
{
	const struct scp_subdomain *sd;
	const struct scp_soc_data *soc;
	struct scp *scp;
	struct genpd_onecell_data *pd_data;
	int i, ret;

	soc = of_device_get_match_data(&pdev->dev);

	scp = init_scp(pdev, soc->domains, soc->num_domains, &soc->regs, bus_list, BUS_TYPE_NUM);

	if (IS_ERR(scp))
		return PTR_ERR(scp);

	mtk_register_power_domains(pdev, scp, soc->num_domains);

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

static struct platform_driver mt6781_scpsys_drv = {
	.probe = mt6781_scpsys_probe,
	.driver = {
		.name = "mtk-scpsys-mt6781",
		.suppress_bind_attrs = true,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_scpsys_match_tbl),
	},
};

module_platform_driver(mt6781_scpsys_drv);
MODULE_LICENSE("GPL");
