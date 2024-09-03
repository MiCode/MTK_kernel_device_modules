// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/io.h>

#include "mtk-pcie.h"
#include "mtk_pcie_lib.h"
#include "../../../phy/mediatek/phy-mtk-io.h"

#define PEXTP_DIG_GLB_28		0x28
#define RG_XTP_FRC_MAC_L1SS_EN		BIT(4)
#define RG_XTP_MAC_L1SS_EN		BIT(5)
#define RG_XTP_FRC_MAC_RX_EI_DIS	BIT(8)
#define RG_XTP_MAC_RX_EI_DIS		BIT(9)
#define RG_XTP_FRC_MAC_TX_CM_DIS	BIT(10)
#define RG_XTP_MAC_TX_CM_DIS		BIT(11)
#define RG_XTP_TX_PTG_EN		BIT(30)
#define PEXTP_DIG_GLB_70		0x70
#define RG_XTP_PIPE_IN_FR_RG		BIT(0)
#define RG_XTP_PIPE_UPDT		BIT(4)
#define RG_XTP_FRC_PIPE_PD_ASYNC	BIT(8)
#define RG_XTP_PIPE_PD_ASYNC		GENMASK(11, 10)
#define RG_XTP_PIPE_PD_SYNC		GENMASK(13, 12)
#define RG_XTP_PIPE_RATE		GENMASK(17, 16)

#define PEXTP_SIFSLV_DIG_LN_TRX		0x3000
#define PEXTP_DIG_LN_TRX_48		(PEXTP_SIFSLV_DIG_LN_TRX + 0x48)
#define RG_XTP_LN_TX_LCTXCM1		GENMASK(13, 8)
#define RG_XTP_LN_FRC_TX_LCTXCM1	BIT(14)
#define RG_XTP_LN_TX_LCTXC0		GENMASK(21, 16)
#define RG_XTP_LN_FRC_TX_LCTXC0		BIT(22)
#define RG_XTP_LN_TX_LCTXCP1		GENMASK(29, 24)
#define RG_XTP_LN_FRC_TX_LCTXCP1	BIT(30)

#define PEXTP_SIFSLV_DIG_LN_TX		0x4000
#define PEXTP_DIG_LN_TX_10		(PEXTP_SIFSLV_DIG_LN_TX + 0x10)
#define RG_XTP_LN_TX_PTG_TYPE		GENMASK(3, 0)
#define RG_XTP_LN_TX_PTG_TYPE_UPD	BIT(6)
#define RG_XTP_LN_TX_PTG_EN		BIT(7)

#define PEXTP_SIFSLV_DIG_LN_RX		0x5000
#define PEXTP_DIG_LN_RX_1C		(PEXTP_SIFSLV_DIG_LN_RX + 0x1c)
#define RG_XTP_LN_RX_PTC_EN		BIT(1)
#define RG_XTP_LN_RX_PTC_TYPE		GENMASK(7, 4)
#define PEXTP_DIG_LN_RX_RGS_C8		(PEXTP_SIFSLV_DIG_LN_RX + 0xc8)
#define RGS_XTP_LN_RX_PRBS_LOCK		BIT(0)
#define RGS_XTP_LN_RX_PRBS_PASS		BIT(1)
#define RGS_XTP_LN_RX_STATUS		(RGS_XTP_LN_RX_PRBS_PASS | RGS_XTP_LN_RX_PRBS_LOCK)

#define PEXTP_SIFSLV_ANA_LN_TRX		0xa000
#define PEXTP_ANA_LN_TRX_08		(PEXTP_SIFSLV_ANA_LN_TRX + 0x8)
#define RG_XTP_LN_TX_CMDRV_PD		BIT(9)

static void pcie_phy_set_pipe_rate_sphy2(void __iomem *phy_base, u32 rate)
{
	mtk_phy_set_bits(phy_base + PEXTP_DIG_GLB_70, RG_XTP_PIPE_IN_FR_RG);
	mtk_phy_update_field(phy_base + PEXTP_DIG_GLB_70, RG_XTP_PIPE_RATE,
			     rate);

	mtk_phy_set_bits(phy_base + PEXTP_DIG_GLB_70, RG_XTP_PIPE_UPDT);
	usleep_range(100, 200);
	mtk_phy_clear_bits(phy_base + PEXTP_DIG_GLB_70, RG_XTP_PIPE_UPDT);
}

static void pcie_phy_force_pipe_p0_sphy2(void __iomem *phy_base)
{
	mtk_phy_set_bits(phy_base + PEXTP_DIG_GLB_70, RG_XTP_PIPE_IN_FR_RG);
	mtk_phy_update_field(phy_base + PEXTP_DIG_GLB_70, RG_XTP_PIPE_PD_ASYNC, 0x0);
	mtk_phy_update_field(phy_base + PEXTP_DIG_GLB_70, RG_XTP_PIPE_PD_SYNC, 0x0);
	mtk_phy_set_bits(phy_base + PEXTP_DIG_GLB_70, RG_XTP_FRC_PIPE_PD_ASYNC);

	mtk_phy_set_bits(phy_base + PEXTP_DIG_GLB_70, RG_XTP_PIPE_UPDT);
	usleep_range(100, 200);
	mtk_phy_clear_bits(phy_base + PEXTP_DIG_GLB_70, RG_XTP_PIPE_UPDT);
}

static void pcie_phy_set_tx_compliance_sphy2(void __iomem *phy_base, int lane)
{
	u32 lane_ofs = lane * PEXTP_LANE_OFFSET;

	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TX_10 + lane_ofs,
			     RG_XTP_LN_TX_PTG_TYPE, 0x6);
	mtk_phy_set_bits(phy_base + PEXTP_DIG_LN_TX_10 + lane_ofs,
			 RG_XTP_LN_TX_PTG_EN);

	mtk_phy_set_bits(phy_base + PEXTP_DIG_LN_TX_10 + lane_ofs,
			 RG_XTP_LN_TX_PTG_TYPE_UPD);
	usleep_range(100, 200);
	mtk_phy_clear_bits(phy_base + PEXTP_DIG_LN_TX_10 + lane_ofs,
			   RG_XTP_LN_TX_PTG_TYPE_UPD);
}

static void pcie_phy_tx_35db_sphy2(void __iomem *phy_base, int lane)
{
	u32 lane_ofs = lane * PEXTP_LANE_OFFSET;

	mtk_phy_clear_bits(phy_base + PEXTP_ANA_LN_TRX_08 + lane_ofs,
			   RG_XTP_LN_TX_CMDRV_PD);

	mtk_phy_set_bits(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			 RG_XTP_LN_FRC_TX_LCTXCM1);
	mtk_phy_set_bits(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			 RG_XTP_LN_FRC_TX_LCTXC0);
	mtk_phy_set_bits(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			 RG_XTP_LN_FRC_TX_LCTXCP1);

	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXCM1, 0x0);
	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXCP1, 0x8);
	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXC0, 0x28);
}

static void pcie_phy_tx_6db_sphy2(void __iomem *phy_base, int lane)
{
	u32 lane_ofs = lane * PEXTP_LANE_OFFSET;

	mtk_phy_clear_bits(phy_base + PEXTP_ANA_LN_TRX_08 + lane_ofs,
			   RG_XTP_LN_TX_CMDRV_PD);

	mtk_phy_set_bits(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			 RG_XTP_LN_FRC_TX_LCTXCM1);
	mtk_phy_set_bits(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			 RG_XTP_LN_FRC_TX_LCTXC0);
	mtk_phy_set_bits(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			 RG_XTP_LN_FRC_TX_LCTXCP1);

	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXCM1, 0x0);
	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXCP1, 0xc);
	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXC0, 0x24);
}

static void sphy2_compliance_gen3_p0(void __iomem *phy_base, int lane)
{
	u32 lane_ofs = lane * PEXTP_LANE_OFFSET;

	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXCM1, 0x0);
	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXCP1, 0xc);
	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXC0, 0x24);
}

static void sphy2_compliance_gen3_p1(void __iomem *phy_base, int lane)
{
	u32 lane_ofs = lane * PEXTP_LANE_OFFSET;

	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXCM1, 0x0);
	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXCP1, 0x8);
	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXC0, 0x28);
}

static void sphy2_compliance_gen3_p2(void __iomem *phy_base, int lane)
{
	u32 lane_ofs = lane * PEXTP_LANE_OFFSET;

	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXCM1, 0x0);
	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXCP1, 0xa);
	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXC0, 0x26);
}

static void sphy2_compliance_gen3_p3(void __iomem *phy_base, int lane)
{
	u32 lane_ofs = lane * PEXTP_LANE_OFFSET;

	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXCM1, 0x0);
	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXCP1, 0x6);
	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXC0, 0x2a);
}

static void sphy2_compliance_gen3_p4(void __iomem *phy_base, int lane)
{
	u32 lane_ofs = lane * PEXTP_LANE_OFFSET;

	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXCM1, 0x0);
	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXCP1, 0x0);
	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXC0, 0x30);
}

static void sphy2_compliance_gen3_p5(void __iomem *phy_base, int lane)
{
	u32 lane_ofs = lane * PEXTP_LANE_OFFSET;

	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXCM1, 0x0);
	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXCP1, 0x0);
	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXC0, 0x2c);
}

static void sphy2_compliance_gen3_p6(void __iomem *phy_base, int lane)
{
	u32 lane_ofs = lane * PEXTP_LANE_OFFSET;

	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXCM1, 0x0);
	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXCP1, 0x0);
	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXC0, 0x2a);
}

static void sphy2_compliance_gen3_p7(void __iomem *phy_base, int lane)
{
	u32 lane_ofs = lane * PEXTP_LANE_OFFSET;

	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXCM1, 0x4);
	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXCP1, 0xa);
	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXC0, 0x22);
}

static void sphy2_compliance_gen3_p8(void __iomem *phy_base, int lane)
{
	u32 lane_ofs = lane * PEXTP_LANE_OFFSET;

	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXCM1, 0x6);
	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXCP1, 0x6);
	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXC0, 0x24);
}

static void sphy2_compliance_gen3_p9(void __iomem *phy_base, int lane)
{
	u32 lane_ofs = lane * PEXTP_LANE_OFFSET;

	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXCM1, 0x8);
	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXCP1, 0x0);
	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXC0, 0x28);
}

static void sphy2_compliance_gen3_p10(void __iomem *phy_base, int lane)
{
	u32 lane_ofs = lane * PEXTP_LANE_OFFSET;

	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXCM1, 0x0);
	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXCP1, 0x10);
	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
			     RG_XTP_LN_TX_LCTXC0, 0x20);
}

static void sphy2_compliance_gen3_preset(void __iomem *phy_base, int lane_num,
					 void (*cb)(void __iomem *, int))
{
	int i;

	for (i = 0; i < lane_num; i++) {
		u32 lane_ofs = i * PEXTP_LANE_OFFSET;

		mtk_phy_clear_bits(phy_base + PEXTP_ANA_LN_TRX_08 + lane_ofs,
			   RG_XTP_LN_TX_CMDRV_PD);

		mtk_phy_set_bits(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
				RG_XTP_LN_FRC_TX_LCTXCM1);
		mtk_phy_set_bits(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
				RG_XTP_LN_FRC_TX_LCTXC0);
		mtk_phy_set_bits(phy_base + PEXTP_DIG_LN_TRX_48 + lane_ofs,
				RG_XTP_LN_FRC_TX_LCTXCP1);

		cb(phy_base, i);
	}
}

static struct preset_cb pcie_sphy2_gen3_preset[] = {
	{
		.name = "p0",
		.cb = &sphy2_compliance_gen3_p0,
	},
	{
		.name = "p1",
		.cb = &sphy2_compliance_gen3_p1,
	},
	{
		.name = "p2",
		.cb = &sphy2_compliance_gen3_p2,
	},
	{
		.name = "p3",
		.cb = &sphy2_compliance_gen3_p3,
	},
	{
		.name = "p4",
		.cb = &sphy2_compliance_gen3_p4,
	},
	{
		.name = "p5",
		.cb = &sphy2_compliance_gen3_p5,
	},
	{
		.name = "p6",
		.cb = &sphy2_compliance_gen3_p6,
	},
	{
		.name = "p7",
		.cb = &sphy2_compliance_gen3_p7,
	},
	{
		.name = "p8",
		.cb = &sphy2_compliance_gen3_p8,
	},
	{
		.name = "p9",
		.cb = &sphy2_compliance_gen3_p9,
	},
	{
		.name = "p10",
		.cb = &sphy2_compliance_gen3_p10,
	},
	{ .name = NULL, },
};

static void mtk_pcie_compliance_gen1_sphy2(struct mtk_pcie_info *pcie_smt,
					   int port)
{
	void __iomem *phy_base = pcie_smt->regs[port];
	int i, lane_num = pcie_smt->max_lane[port];

	/* PCIe rate = Gen1 */
	pcie_phy_set_pipe_rate_sphy2(phy_base, PIPE_RATE_GEN1);

	/* Force Pipe P0 */
	pcie_phy_force_pipe_p0_sphy2(phy_base);

	for (i = 0; i < lane_num; i++) {
		/* Set TX output compliance pattern */
		pcie_phy_set_tx_compliance_sphy2(phy_base, i);
		/* TX -3.5dB: (Cp1,C0,Cm1)=(8,40,0) */
		pcie_phy_tx_35db_sphy2(phy_base, i);
	}

	pr_info("Gen1 compliance setting completed!\n");
}

static void mtk_pcie_compliance_gen2_35db_sphy2(struct mtk_pcie_info *pcie_smt,
						int port)
{
	void __iomem *phy_base = pcie_smt->regs[port];
	int i, lane_num = pcie_smt->max_lane[port];

	/* PCIe rate = Gen2 */
	pcie_phy_set_pipe_rate_sphy2(phy_base, PIPE_RATE_GEN2);

	/* Force Pipe P0 */
	pcie_phy_force_pipe_p0_sphy2(phy_base);

	for (i = 0; i < lane_num; i++) {
		/* Set TX output compliance pattern */
		pcie_phy_set_tx_compliance_sphy2(phy_base, i);
		/* TX -3.5dB: (Cp1,C0,Cm1)=(8,40,0) */
		pcie_phy_tx_35db_sphy2(phy_base, i);
	}

	pr_info("Gen2 3.5db compliance setting completed!\n");
}

static void mtk_pcie_compliance_gen2_6db_sphy2(struct mtk_pcie_info *pcie_smt,
					       int port)
{
	void __iomem *phy_base = pcie_smt->regs[port];
	int i, lane_num = pcie_smt->max_lane[port];

	/* PCIe rate = Gen2 */
	pcie_phy_set_pipe_rate_sphy2(phy_base, PIPE_RATE_GEN2);

	/* Force Pipe P0 */
	pcie_phy_force_pipe_p0_sphy2(phy_base);

	for (i = 0; i < lane_num; i++) {
		/* Set TX output compliance pattern */
		pcie_phy_set_tx_compliance_sphy2(phy_base, i);
		/* TX -6dB: (Cp1,C0,Cm1)=(12,36,0) */
		pcie_phy_tx_6db_sphy2(phy_base, i);
	}

	pr_info("Gen2 6db compliance setting completed!\n");
}

static void mtk_pcie_compliance_gen3_sphy2(struct mtk_pcie_info *pcie_smt,
					   int port, char *preset)
{
	void __iomem *phy_base = pcie_smt->regs[port];
	int i, lane_num = pcie_smt->max_lane[port];
	struct preset_cb *preset_list = pcie_sphy2_gen3_preset;

	/* PCIe rate = Gen3 */
	pcie_phy_set_pipe_rate_sphy2(phy_base, PIPE_RATE_GEN3);

	/* Force Pipe P0 */
	pcie_phy_force_pipe_p0_sphy2(phy_base);

	/* Set TX output compliance pattern */
	for (i = 0; i < lane_num; i++)
		pcie_phy_set_tx_compliance_sphy2(phy_base, i);

	while (preset_list->name) {
		if (!strcmp(preset_list->name, preset)) {
			if (preset_list->cb) {
				sphy2_compliance_gen3_preset(phy_base, lane_num,
							     preset_list->cb);
				pr_info("Gen3 compliance setting completed!\n");
				return;
			}

			pr_info("preset function not found\n");
			break;
		}
		preset_list++;
	}
}

static int mtk_pcie_compliance_sphy2(struct mtk_pcie_info *pcie_smt, int port,
				     char *cmd, char *preset)
{
	if (!strcmp(cmd, "gen1")) {
		pr_info("Start SPHY2 GEN1 compliance test\n");
		mtk_pcie_compliance_gen1_sphy2(pcie_smt, port);
	} else if (!strcmp(cmd, "gen2_35db")) {
		pr_info("Start SPHY2 GEN2 3.5db compliance test\n");
		mtk_pcie_compliance_gen2_35db_sphy2(pcie_smt, port);
	} else if (!strcmp(cmd, "gen2_6db")) {
		pr_info("Start SPHY2 GEN2 6db compliance test\n");
		mtk_pcie_compliance_gen2_6db_sphy2(pcie_smt, port);
	} else if (!strcmp(cmd, "gen3")) {
		pr_info("Start SPHY2 GEN3 compliance test\n");
		mtk_pcie_compliance_gen3_sphy2(pcie_smt, port, preset);
	} else {
		pr_info("Unknown command: %s\n", cmd);
		return -EINVAL;
	}

	return 0;
}

static int mtk_pcie_loopback_sphy2(struct mtk_pcie_info *pcie_smt, int port)
{
	void __iomem *phy_base = pcie_smt->regs[port];
	u32 lane_ofs;
	int val = 0, ret = 0, i = 0, err_count = 0;

	pr_info("Start SPHY2 loopback test\n");

	/* L1ss = enable */
	mtk_phy_set_bits(phy_base + PEXTP_DIG_GLB_28, RG_XTP_MAC_L1SS_EN);
	mtk_phy_set_bits(phy_base + PEXTP_DIG_GLB_28, RG_XTP_FRC_MAC_L1SS_EN);
	mtk_phy_clear_bits(phy_base + PEXTP_DIG_GLB_28, RG_XTP_MAC_RX_EI_DIS);
	mtk_phy_set_bits(phy_base + PEXTP_DIG_GLB_28, RG_XTP_FRC_MAC_RX_EI_DIS);
	mtk_phy_clear_bits(phy_base + PEXTP_DIG_GLB_28, RG_XTP_MAC_TX_CM_DIS);
	mtk_phy_set_bits(phy_base + PEXTP_DIG_GLB_28, RG_XTP_FRC_MAC_TX_CM_DIS);

	/* Set Rate=Gen1 */
	pcie_phy_set_pipe_rate_sphy2(phy_base, PIPE_RATE_GEN1);

	/* Force PIPE (P0) */
	pcie_phy_force_pipe_p0_sphy2(phy_base);

	/* Set TX output Pattern for each lane */
	for (i = 0; i < pcie_smt->test_lane[port]; i++) {
		lane_ofs = i * PEXTP_LANE_OFFSET;
		mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TX_10 + lane_ofs,
				     RG_XTP_LN_TX_PTG_TYPE, 0xd);
	}

	/* Set TX PTG Enable */
	mtk_phy_set_bits(phy_base + PEXTP_DIG_GLB_28, RG_XTP_TX_PTG_EN);

	/* Set RX Pattern Checker (Type & Enable)  for each lane */
	for (i = 0; i < pcie_smt->test_lane[port]; i++) {
		lane_ofs = i * PEXTP_LANE_OFFSET;
		mtk_phy_set_bits(phy_base + PEXTP_DIG_LN_RX_1C + lane_ofs,
				 RG_XTP_LN_RX_PTC_EN);
		mtk_phy_update_field(phy_base + PEXTP_DIG_LN_RX_1C + lane_ofs,
				     RG_XTP_LN_RX_PTC_TYPE, 0xd);
	}

	/* toggle ptc_en for status counter clear for each lane */
	for (i = 0; i < pcie_smt->test_lane[port]; i++) {
		lane_ofs = i * PEXTP_LANE_OFFSET;
		mtk_phy_clear_bits(phy_base + PEXTP_DIG_LN_RX_1C + lane_ofs,
				   RG_XTP_LN_RX_PTC_EN);
		usleep_range(100, 200);
		mtk_phy_set_bits(phy_base + PEXTP_DIG_LN_RX_1C + lane_ofs,
				 RG_XTP_LN_RX_PTC_EN);
	}

	msleep(50);
	/* Check status */
	for (i = 0; i < pcie_smt->test_lane[port]; i++) {
		lane_ofs = i * PEXTP_LANE_OFFSET;
		val = readl(phy_base + PEXTP_DIG_LN_RX_RGS_C8 + lane_ofs);
		if ((val & RGS_XTP_LN_RX_STATUS) != 0x3) {
			err_count = val >> 12;
			pr_info("PCIe lane%i test failed: %#x!\n", i, val);
			pr_info("lane%i error count: %d\n", i, err_count);
			ret = -EINVAL;
		} else {
			pr_info("lane%i loopback test success!\n", i);
		}
	}

	return ret;
}

struct pcie_test_lib pcie_sphy2_test_lib = {
	.loopback = mtk_pcie_loopback_sphy2,
	.compliance = mtk_pcie_compliance_sphy2,
};
