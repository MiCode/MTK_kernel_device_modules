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
#define RX_XTP_FRC_MAC_L1SS_EN		BIT(4)
#define RX_XTP_MAC_L1SS_EN		BIT(5)
#define RG_XTP_FRC_MAC_RX_EI_DIS	BIT(8)
#define RG_XTP_MAC_RX_EI_DIS		BIT(9)
#define RG_XTP_FRC_MAC_TX_CM_DIS	BIT(10)
#define RG_XTP_MAC_TX_CM_DIS		BIT(11)
#define RG_XTP_TX_PTG_EN		BIT(30)
#define PEXTP_DIG_GLB_70		0x70
#define RG_XTP_PIPE_IN_FR_RG		BIT(0)
#define RG_XTP_PIPE_UPDT		BIT(4)
#define RG_XTP_FRC_PIPE_POWER_DOWN_ASYNC	BIT(8)
#define RG_XTP_PIPE_POWER_DOWN_ASYNC	GENMASK(11, 10)
#define RG_XTP_PIPE_POWER_DOWN_SYNC	GENMASK(13, 12)
#define RG_XTP_PIPE_RATE		GENMASK(17, 16)

#define PEXTP_SIFSLV_DIG_LN_TRX		0x3000
#define PEXTP_DIG_LN_TRX_48		0x48
#define RG_XTP_LN_TX_LCTXCM1		GENMASK(13, 8)
#define RG_XTP_LN_FRC_TX_LCTXCM1	BIT(14)
#define RG_XTP_LN_TX_LCTXC0		GENMASK(21, 16)
#define RG_XTP_LN_FRX_TX_LCTXC0		BIT(22)
#define RG_XTP_LN_TX_LCTXCP1		GENMASK(29, 24)
#define RG_XTP_LN_FRC_TX_LCTXCP1	BIT(30)

#define PEXTP_DIG_LN_TX			0x4000
#define PEXTP_DIG_LN_TX_10		0x10
#define RG_XTP_LN_TX_PTG_TYPE		GENMASK(3, 0)
#define RG_XTP_LN_TX_PTG_TYPE_UPD	BIT(6)
#define RG_XTP_LN_TX_PTG_EN		BIT(7)

#define PEXTP_DIG_LN_RX			0x5000
#define PEXTP_DIG_LN_RX_1C		0x1c
#define RG_XTP_LN_RX_PTC_EN		BIT(1)
#define RG_XTP_LN_RX_PTC_TYPE		GENMASK(7, 4)
#define PEXTP_DIG_LN_RX_RGS_CC		0xcc
#define RG_XTP_LN_RX_PTC_RX_LOCK	BIT(0)
#define RG_XTP_LN_RX_PTC_RX_PASS	BIT(1)
#define RG_XTP_LN_RX_PTC_RX_PASSTH	BIT(2)
#define RG_XTP_LN_RX_PTC_RX_ERRCNT	GENMASK(32, 16)

#define PEXTP_SIFSLV_ANA_LN_TRX		0xa000
#define PEXTP_ANA_LN_TRX_08		0x8
#define RG_XTP_LN_TX_CMDRV_PD		BIT(5)

static void pcie_phy_set_pipe_rate_sphy3(void __iomem *phy_base, u32 rate)
{
	mtk_phy_set_bits(phy_base + PEXTP_DIG_GLB_70, RG_XTP_PIPE_IN_FR_RG);
	mtk_phy_update_field(phy_base + PEXTP_DIG_GLB_70, RG_XTP_PIPE_RATE, rate);

	mtk_phy_set_bits(phy_base + PEXTP_DIG_GLB_70, RG_XTP_PIPE_UPDT);
	usleep_range(100, 200);
	mtk_phy_clear_bits(phy_base + PEXTP_DIG_GLB_70, RG_XTP_PIPE_UPDT);
}

static void pcie_phy_force_pipe_p0_sphy3(void __iomem *phy_base)
{
	mtk_phy_set_bits(phy_base + PEXTP_DIG_GLB_70, RG_XTP_PIPE_IN_FR_RG);
	mtk_phy_update_field(phy_base + PEXTP_DIG_GLB_70,
			     RG_XTP_PIPE_POWER_DOWN_ASYNC, 0x0);
	mtk_phy_update_field(phy_base + PEXTP_DIG_GLB_70,
			     RG_XTP_PIPE_POWER_DOWN_SYNC, 0x0);
	mtk_phy_set_bits(phy_base + PEXTP_DIG_GLB_70,
			 RG_XTP_FRC_PIPE_POWER_DOWN_ASYNC);

	mtk_phy_set_bits(phy_base + PEXTP_DIG_GLB_70, RG_XTP_PIPE_UPDT);
	usleep_range(100, 200);
	mtk_phy_clear_bits(phy_base + PEXTP_DIG_GLB_70, RG_XTP_PIPE_UPDT);
}

static void pcie_phy_set_tx_compliance_sphy3(void __iomem *phy_base, int lane)
{
	mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TX + PEXTP_DIG_LN_TX_10 +
			     lane * PEXTP_LANE_OFFSET, RG_XTP_LN_TX_PTG_TYPE,
			     0x6);
	mtk_phy_set_bits(phy_base + PEXTP_DIG_LN_TX + PEXTP_DIG_LN_TX_10 +
			 lane * PEXTP_LANE_OFFSET, RG_XTP_LN_TX_PTG_EN);

	mtk_phy_set_bits(phy_base + PEXTP_DIG_LN_TX + PEXTP_DIG_LN_TX_10 +
			 lane * PEXTP_LANE_OFFSET, RG_XTP_LN_TX_PTG_TYPE_UPD);
	usleep_range(100, 200);
	mtk_phy_clear_bits(phy_base + PEXTP_DIG_LN_TX + PEXTP_DIG_LN_TX_10 +
			   lane * PEXTP_LANE_OFFSET,
			   RG_XTP_LN_TX_PTG_TYPE_UPD);
}

static void pcie_phy_tx_35db_sphy3(void __iomem *phy_base, int lane)
{
	mtk_phy_clear_bits(phy_base + PEXTP_SIFSLV_ANA_LN_TRX +
			   PEXTP_ANA_LN_TRX_08 + lane * PEXTP_LANE_OFFSET,
			   RG_XTP_LN_TX_CMDRV_PD);

	mtk_phy_set_bits(phy_base + PEXTP_SIFSLV_DIG_LN_TRX +
			 PEXTP_DIG_LN_TRX_48 + lane * PEXTP_LANE_OFFSET,
			 RG_XTP_LN_FRC_TX_LCTXCM1);
	mtk_phy_set_bits(phy_base + PEXTP_SIFSLV_DIG_LN_TRX +
			 PEXTP_DIG_LN_TRX_48 + lane * PEXTP_LANE_OFFSET,
			 RG_XTP_LN_FRX_TX_LCTXC0);
	mtk_phy_set_bits(phy_base + PEXTP_SIFSLV_DIG_LN_TRX +
			 PEXTP_DIG_LN_TRX_48 + lane * PEXTP_LANE_OFFSET,
			 RG_XTP_LN_FRC_TX_LCTXCP1);

	mtk_phy_update_field(phy_base + PEXTP_SIFSLV_DIG_LN_TRX +
			     PEXTP_DIG_LN_TRX_48 + lane * PEXTP_LANE_OFFSET,
			     RG_XTP_LN_TX_LCTXCM1, 0x0);
	mtk_phy_update_field(phy_base + PEXTP_SIFSLV_DIG_LN_TRX +
			     PEXTP_DIG_LN_TRX_48 + lane * PEXTP_LANE_OFFSET,
			     RG_XTP_LN_TX_LCTXCP1, 0x8);
	mtk_phy_update_field(phy_base + PEXTP_SIFSLV_DIG_LN_TRX +
			     PEXTP_DIG_LN_TRX_48 + lane * PEXTP_LANE_OFFSET,
			     RG_XTP_LN_TX_LCTXC0, 0x28);
}

static void pcie_phy_tx_6db_sphy3(void __iomem *phy_base, int lane)
{
	mtk_phy_clear_bits(phy_base + PEXTP_SIFSLV_ANA_LN_TRX +
			   PEXTP_ANA_LN_TRX_08 + lane * PEXTP_LANE_OFFSET,
			   RG_XTP_LN_TX_CMDRV_PD);

	mtk_phy_set_bits(phy_base + PEXTP_SIFSLV_DIG_LN_TRX +
			 PEXTP_DIG_LN_TRX_48 + lane * PEXTP_LANE_OFFSET,
			 RG_XTP_LN_FRC_TX_LCTXCM1);
	mtk_phy_set_bits(phy_base + PEXTP_SIFSLV_DIG_LN_TRX +
			 PEXTP_DIG_LN_TRX_48 + lane * PEXTP_LANE_OFFSET,
			 RG_XTP_LN_FRX_TX_LCTXC0);
	mtk_phy_set_bits(phy_base + PEXTP_SIFSLV_DIG_LN_TRX +
			 PEXTP_DIG_LN_TRX_48 + lane * PEXTP_LANE_OFFSET,
			 RG_XTP_LN_FRC_TX_LCTXCP1);

	mtk_phy_update_field(phy_base + PEXTP_SIFSLV_DIG_LN_TRX +
			     PEXTP_DIG_LN_TRX_48 + lane * PEXTP_LANE_OFFSET,
			     RG_XTP_LN_TX_LCTXCM1, 0x0);
	mtk_phy_update_field(phy_base + PEXTP_SIFSLV_DIG_LN_TRX +
			     PEXTP_DIG_LN_TRX_48 + lane * PEXTP_LANE_OFFSET,
			     RG_XTP_LN_TX_LCTXCP1, 0xc);
	mtk_phy_update_field(phy_base + PEXTP_SIFSLV_DIG_LN_TRX +
			     PEXTP_DIG_LN_TRX_48 + lane * PEXTP_LANE_OFFSET,
			     RG_XTP_LN_TX_LCTXC0, 0x24);
}

static void sphy3_compliance_p4(void __iomem *phy_base, int lane)
{
	u32 lane_offset = lane * PEXTP_LANE_OFFSET;

	mtk_phy_update_field(phy_base + PEXTP_SIFSLV_DIG_LN_TRX +
			     PEXTP_DIG_LN_TRX_48 + lane_offset,
			     RG_XTP_LN_TX_LCTXCM1, 0x0);
	mtk_phy_update_field(phy_base + PEXTP_SIFSLV_DIG_LN_TRX +
			     PEXTP_DIG_LN_TRX_48 + lane_offset,
			     RG_XTP_LN_TX_LCTXCP1, 0x0);
	mtk_phy_update_field(phy_base + PEXTP_SIFSLV_DIG_LN_TRX +
			     PEXTP_DIG_LN_TRX_48 + lane_offset,
			     RG_XTP_LN_TX_LCTXC0, 0x30);
}

static void sphy3_compliance_p7(void __iomem *phy_base, int lane)
{
	u32 lane_offset = lane * PEXTP_LANE_OFFSET;

	mtk_phy_update_field(phy_base + PEXTP_SIFSLV_DIG_LN_TRX +
			     PEXTP_DIG_LN_TRX_48 + lane_offset,
			     RG_XTP_LN_TX_LCTXCM1, 0x4);
	mtk_phy_update_field(phy_base + PEXTP_SIFSLV_DIG_LN_TRX +
			     PEXTP_DIG_LN_TRX_48 + lane_offset,
			     RG_XTP_LN_TX_LCTXCP1, 0xa);
	mtk_phy_update_field(phy_base + PEXTP_SIFSLV_DIG_LN_TRX +
			     PEXTP_DIG_LN_TRX_48 + lane_offset,
			     RG_XTP_LN_TX_LCTXC0, 0x22);
}

static void sphy3_compliance_preset(void __iomem *phy_base, int lane_num,
				    void (*cb)(void __iomem *, int))
{
	int i;

	for (i = 0; i < lane_num; i++) {
		mtk_phy_clear_bits(phy_base + PEXTP_SIFSLV_ANA_LN_TRX +
			   PEXTP_ANA_LN_TRX_08 + i * PEXTP_LANE_OFFSET,
			   RG_XTP_LN_TX_CMDRV_PD);

		mtk_phy_set_bits(phy_base + PEXTP_SIFSLV_DIG_LN_TRX +
				PEXTP_DIG_LN_TRX_48 + i * PEXTP_LANE_OFFSET,
				RG_XTP_LN_FRC_TX_LCTXCM1);
		mtk_phy_set_bits(phy_base + PEXTP_SIFSLV_DIG_LN_TRX +
				PEXTP_DIG_LN_TRX_48 + i * PEXTP_LANE_OFFSET,
				RG_XTP_LN_FRX_TX_LCTXC0);
		mtk_phy_set_bits(phy_base + PEXTP_SIFSLV_DIG_LN_TRX +
				PEXTP_DIG_LN_TRX_48 + i * PEXTP_LANE_OFFSET,
				RG_XTP_LN_FRC_TX_LCTXCP1);

		cb(phy_base, i);
	}
}

/* Currently we only got P4 and p7 preset setting */
static struct preset_cb pcie_sphy3_preset[] = {
	{
		.name = "p4",
		.cb = &sphy3_compliance_p4,
	},
	{
		.name = "p7",
		.cb = &sphy3_compliance_p7,
	},
	{ .name = NULL, },
};

static void mtk_pcie_compliance_gen1_sphy3(struct mtk_pcie_info *pcie_smt,
					   int port)
{
	void __iomem *phy_base = pcie_smt->regs[port];
	int i, lane_num = pcie_smt->max_lane[port];

	if (!phy_base) {
		pr_info("%s, %d\n", __func__, __LINE__);
		return;
	}

	/* PCIe rate = Gen1 */
	pcie_phy_set_pipe_rate_sphy3(phy_base, PIPE_RATE_GEN1);

	/* Force Pipe P0 */
	pcie_phy_force_pipe_p0_sphy3(phy_base);

	for (i = 0; i < lane_num; i++) {
		/* Set TX output compliance pattern */
		pcie_phy_set_tx_compliance_sphy3(phy_base, i);
		/* TX -3.5dB: (Cp1,C0,Cm1)=(12,36,0) */
		pcie_phy_tx_35db_sphy3(phy_base, i);
	}

	pr_info("Gen1 compliance setting completed!\n");
}

static void mtk_pcie_compliance_gen2_35db_sphy3(struct mtk_pcie_info *pcie_smt,
						int port)
{
	void __iomem *phy_base = pcie_smt->regs[port];
	int i, lane_num = pcie_smt->max_lane[port];

	/* PCIe rate = Gen2 */
	pcie_phy_set_pipe_rate_sphy3(phy_base, PIPE_RATE_GEN2);

	/* Force Pipe P0 */
	pcie_phy_force_pipe_p0_sphy3(phy_base);

	for (i = 0; i < lane_num; i++) {
		/* Set TX output compliance pattern */
		pcie_phy_set_tx_compliance_sphy3(phy_base, i);
		/* TX -3.5dB: (Cp1,C0,Cm1)=(12,36,0) */
		pcie_phy_tx_35db_sphy3(phy_base, i);
	}

	pr_info("Gen2 3.5db compliance setting completed!\n");
}

static void mtk_pcie_compliance_gen2_6db_sphy3(struct mtk_pcie_info *pcie_smt,
					       int port)
{
	void __iomem *phy_base = pcie_smt->regs[port];
	int i, lane_num = pcie_smt->max_lane[port];

	/* PCIe rate = Gen2 */
	pcie_phy_set_pipe_rate_sphy3(phy_base, PIPE_RATE_GEN2);

	/* Force Pipe P0 */
	pcie_phy_force_pipe_p0_sphy3(phy_base);

	for (i = 0; i < lane_num; i++) {
		/* Set TX output compliance pattern */
		pcie_phy_set_tx_compliance_sphy3(phy_base, i);
		/* TX -6dB: (Cp1,C0,Cm1)=(12,36,0) */
		pcie_phy_tx_6db_sphy3(phy_base, i);
	}

	pr_info("Gen2 6db compliance setting completed!\n");
}

static void mtk_pcie_compliance_gen3_sphy3(struct mtk_pcie_info *pcie_smt,
					   int port, char *preset)
{
	void __iomem *phy_base = pcie_smt->regs[port];
	struct preset_cb *preset_list = pcie_sphy3_preset;
	int i, lane_num = pcie_smt->max_lane[port];

	/* PCIe rate = Gen3 */
	pcie_phy_set_pipe_rate_sphy3(phy_base, PIPE_RATE_GEN3);

	/* Force Pipe P0 */
	pcie_phy_force_pipe_p0_sphy3(phy_base);

	/* Set TX output compliance pattern */
	for (i = 0; i < lane_num; i++)
		pcie_phy_set_tx_compliance_sphy3(phy_base, i);

	while (preset_list->name) {
		if (!strcmp(preset_list->name, preset)) {
			if (preset_list->cb) {
				sphy3_compliance_preset(phy_base, lane_num,
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

static void mtk_pcie_compliance_gen4_sphy3(struct mtk_pcie_info *pcie_smt,
					   int port, char *preset)
{
	void __iomem *phy_base = pcie_smt->regs[port];
	struct preset_cb *preset_list = pcie_sphy3_preset;
	int i, lane_num = pcie_smt->max_lane[port];

	/* PCIe rate = Gen4 */
	pcie_phy_set_pipe_rate_sphy3(phy_base, PIPE_RATE_GEN4);

	/* Force Pipe P0 */
	pcie_phy_force_pipe_p0_sphy3(phy_base);

	/* Set TX output compliance pattern */
	for (i = 0; i < lane_num; i++)
		pcie_phy_set_tx_compliance_sphy3(phy_base, i);

	while (preset_list->name) {
		if (!strcmp(preset_list->name, preset)) {
			if (preset_list->cb) {
				sphy3_compliance_preset(phy_base, lane_num,
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

static int mtk_pcie_compliance_sphy3(struct mtk_pcie_info *pcie_smt, int port,
				      char *cmd, char *preset)
{
	if (!strcmp(cmd, "gen1")) {
		pr_info("Start SPHY3 GEN1 compliance test\n");
		mtk_pcie_compliance_gen1_sphy3(pcie_smt, port);
	} else if (!strcmp(cmd, "gen2_35db")) {
		pr_info("Start SPHY3 GEN2 3.5db compliance test\n");
		mtk_pcie_compliance_gen2_35db_sphy3(pcie_smt, port);
	} else if (!strcmp(cmd, "gen2_6db")) {
		pr_info("Start SPHY3 GEN2 6db compliance test\n");
		mtk_pcie_compliance_gen2_6db_sphy3(pcie_smt, port);
	} else if (!strcmp(cmd, "gen3")) {
		pr_info("Start SPHY3 GEN3 compliance test\n");
		mtk_pcie_compliance_gen3_sphy3(pcie_smt, port, preset);
	} else if (!strcmp(cmd, "gen4")) {
		pr_info("Start SPHY3 GEN4 compliance test\n");
		mtk_pcie_compliance_gen4_sphy3(pcie_smt, port, preset);
	} else {
		pr_info("Unknown command: %s\n", cmd);
		return -EINVAL;
	}

	return 0;
}

static int mtk_pcie_loopback_sphy3(struct mtk_pcie_info *pcie_smt, int port)
{
	int val = 0, ret = 0, i = 0, err_count = 0;
	void __iomem *phy_base = pcie_smt->regs[port];

	pr_info("pcie loopback test start\n");

	/* L1ss enable */
	mtk_phy_set_bits(phy_base + PEXTP_DIG_GLB_28, RX_XTP_MAC_L1SS_EN);
	mtk_phy_set_bits(phy_base + PEXTP_DIG_GLB_28, RX_XTP_FRC_MAC_L1SS_EN);
	mtk_phy_clear_bits(phy_base + PEXTP_DIG_GLB_28, RG_XTP_MAC_RX_EI_DIS);
	mtk_phy_set_bits(phy_base + PEXTP_DIG_GLB_28, RG_XTP_FRC_MAC_RX_EI_DIS);
	mtk_phy_clear_bits(phy_base + PEXTP_DIG_GLB_28, RG_XTP_MAC_TX_CM_DIS);
	mtk_phy_set_bits(phy_base + PEXTP_DIG_GLB_28, RG_XTP_FRC_MAC_TX_CM_DIS);

	/* Set Rate=Gen1 */
	mtk_phy_set_bits(phy_base + PEXTP_DIG_GLB_70, RG_XTP_PIPE_IN_FR_RG);
	mtk_phy_update_field(phy_base + PEXTP_DIG_GLB_70, RG_XTP_PIPE_RATE, 0x0);
	mtk_phy_set_bits(phy_base + PEXTP_DIG_GLB_70, RG_XTP_PIPE_UPDT);
	mtk_phy_clear_bits(phy_base + PEXTP_DIG_GLB_70, RG_XTP_PIPE_UPDT);

	/* Force PIPE (P0) */
	mtk_phy_set_bits(phy_base + PEXTP_DIG_GLB_70, RG_XTP_PIPE_IN_FR_RG);
	mtk_phy_update_field(phy_base + PEXTP_DIG_GLB_70, RG_XTP_PIPE_POWER_DOWN_ASYNC, 0x0);
	mtk_phy_update_field(phy_base + PEXTP_DIG_GLB_70, RG_XTP_PIPE_POWER_DOWN_SYNC, 0x0);
	mtk_phy_set_bits(phy_base + PEXTP_DIG_GLB_70, RG_XTP_FRC_PIPE_POWER_DOWN_ASYNC);
	mtk_phy_set_bits(phy_base + PEXTP_DIG_GLB_70, RG_XTP_PIPE_UPDT);
	mtk_phy_clear_bits(phy_base + PEXTP_DIG_GLB_70, RG_XTP_PIPE_UPDT);

	/* Set TX output Pattern for each lane */
	for (i = 0; i < pcie_smt->test_lane[port]; i++) {
		mtk_phy_update_field(phy_base + PEXTP_DIG_LN_TX + PEXTP_DIG_LN_TX_10 +
				     PEXTP_LANE_OFFSET * i, RG_XTP_LN_TX_PTG_TYPE, 0x6);
	}

	/* Set TX PTG Enable */
	mtk_phy_set_bits(phy_base + PEXTP_DIG_GLB_28, RG_XTP_TX_PTG_EN);

	/* Set RX Pattern Checker (Type & Enable)  for each lane */
	for (i = 0; i < pcie_smt->test_lane[port]; i++) {
		mtk_phy_set_bits(phy_base + PEXTP_DIG_LN_RX + PEXTP_DIG_LN_RX_1C +
				 PEXTP_LANE_OFFSET * i, RG_XTP_LN_RX_PTC_EN);
		mtk_phy_update_field(phy_base + PEXTP_DIG_LN_RX + PEXTP_DIG_LN_RX_1C +
				     PEXTP_LANE_OFFSET * i, RG_XTP_LN_RX_PTC_TYPE, 0x6);
	}

	/* toggle ptc_en for status counter clear */
	for (i = 0; i < pcie_smt->test_lane[port]; i++) {
		mtk_phy_clear_bits(phy_base + PEXTP_DIG_LN_RX + PEXTP_DIG_LN_RX_1C +
				   PEXTP_LANE_OFFSET * i, RG_XTP_LN_RX_PTC_EN);
		mtk_phy_set_bits(phy_base + PEXTP_DIG_LN_RX + PEXTP_DIG_LN_RX_1C +
				 PEXTP_LANE_OFFSET * i, RG_XTP_LN_RX_PTC_EN);
	}

	msleep(50);
	/* RX Check status */
	for (i = 0; i < pcie_smt->test_lane[port]; i++) {
		val = readl(phy_base + PEXTP_DIG_LN_RX + PEXTP_DIG_LN_RX_RGS_CC + PEXTP_LANE_OFFSET * i);
		if ((val & (RG_XTP_LN_RX_PTC_RX_LOCK | RG_XTP_LN_RX_PTC_RX_PASS)) != 0x3) {
			err_count = val >> 16;
			pr_info("PCIe lane%i test failed: %#x!\n", i, val);
			pr_info("lane%i error count: %d\n", i, err_count);
			ret = -EINVAL;
		} else {
			pr_info("lane%i loopback test success!\n", i);
		}
	}

	return ret;
}

struct pcie_test_lib pcie_sphy3_test_lib = {
	.loopback = mtk_pcie_loopback_sphy3,
	.compliance = mtk_pcie_compliance_sphy3,
};
