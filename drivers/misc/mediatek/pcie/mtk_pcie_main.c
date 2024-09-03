// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/pci.h>
#include <linux/phy/phy.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/slab.h>

#include "mtk-pcie.h"
#include "mtk_pcie_lib.h"
#include "../drivers/pci/pci.h"
#include "../../../phy/mediatek/phy-mtk-io.h"

#define IOCTL_DEV_IOCTLID	'P'
#define PCIE_SMT_TEST_SLOT	_IOW(IOCTL_DEV_IOCTLID, 0, int)
#define IOCTL_DEV_FTM		'a'
#define PCIE_SMT_FTM_CMD	_IOW(IOCTL_DEV_FTM, IOCTL_DEV_FTM, char*)

#define PCIE_SMT_DEV_NAME	"pcie_smt"
#define PCIE_SYSFS_NAME		"pcie_test"

struct cmd_tbl {
	char *cmd_name;
	int (*cb_func)(int argc, char **argv);
	char *help;
};

/**
 * struct match_table - differentiate between IC
 * @compatible: compatible info of phy in dts
 * @phy_base: physical address of each port
 * @test_lane: number of test lane per port
 * @max_lane: number of max lane per port
 * @max_speed: max speed of each port
 * @max_port: max number of ports
 */
struct match_table {
	char compatible[128];
	unsigned int phy_base[PCIE_PORT_MAX];
	unsigned int mac_base[PCIE_PORT_MAX];
	unsigned int test_lane[PCIE_PORT_MAX];
	unsigned int max_lane[PCIE_PORT_MAX];
	unsigned int max_speed[PCIE_PORT_MAX];
	unsigned int max_port;
	struct pcie_test_lib *test_lib[PCIE_PORT_MAX];
};

static struct match_table test_table[] = {
	/* MT6880 PCIe info */
	{
	.compatible = "mediatek,mt6880-pcie-phy",
	.phy_base = {0x11e40000, 0x11e60000, 0x11e80000, 0x11ea0000},
	.test_lane = {1, 1, 1, 1},
	.max_lane = {2, 2, 1, 1},
	.max_speed = {3, 3, 3, 3},
	.max_port = 4,
	.test_lib = {&pcie_sphy2_test_lib, &pcie_sphy2_test_lib,
		     &pcie_sphy2_test_lib, &pcie_sphy2_test_lib},
	},
	/* MT6980 PCIe info */
	{
	.compatible = "mediatek,mt6980-pcie-phy",
	.phy_base = {0x11310000, 0x11e00000, 0x11e20000},
	.test_lane = {2, 2, 2},
	.max_lane = {2, 2, 2},
	.max_speed = {4, 3, 3},
	.max_port = 3,
	.test_lib = {&pcie_sphy4_test_lib, &pcie_sphy2_test_lib,
		     &pcie_sphy2_test_lib},
	},
	/* MT2737 PCIe info */
	{
	.compatible = "mediatek,mt2737-pcie-phy",
	.phy_base = {0x11310000, 0x11e00000},
	.test_lane = {2, 2},
	.max_lane = {2, 2},
	.max_speed = {4, 3},
	.max_port = 2,
	.test_lib = {&pcie_sphy4_test_lib, &pcie_sphy2_test_lib},
	},
	/* MT6989 PCIe info */
	{
	.compatible = "mediatek,mt6989-pcie-phy",
	.phy_base = {0x11100000, 0x11120000},
	.test_lane = {1, 2},
	.max_lane = {1, 2},
	.max_speed = {4, 3},
	.max_port = 2,
	.test_lib = {&pcie_sphy4_test_lib, &pcie_sphy2_test_lib},
	},
	/* MT6991 PCIe info */
	{
	.compatible = "mediatek,mt6991-pcie-phy",
	.phy_base = {0x16900000, 0x16930000, 0x16960000},
	.mac_base = {0x16910000, 0x16940000, 0x16970000},
	.test_lane = {1, 2, 1},
	.max_lane = {1, 2, 1},
	.max_speed = {4, 4, 4},
	.max_port = 3,
	.test_lib = {&pcie_sphy3_test_lib, &pcie_sphy3_test_lib,
		     &pcie_sphy3_test_lib},
	},
};

static struct mtk_pcie_info *pcie_smt;
static int mtk_pcie_test_ctrl(char *buf);
static int mtk_pcie_phy_power_on(struct mtk_pcie_info *smt_info, int port);
static void mtk_pcie_phy_power_off(struct mtk_pcie_info *smt_info, int port);

/* Set partition when use PCIe PHY debug probe table */
static void mtk_pcie_phy_dbg_set_partition(void __iomem *phy_base, u32 partition)
{
	writel_relaxed(partition, phy_base + PCIE_PHYD_TOP);
}

/* Read the PCIe PHY internal signal corresponding to the debug probe table bus */
static u32 mtk_pcie_phy_dbg_read_bus(void __iomem *phy_base, u32 lane_num, u32 bus)
{
	int probe_sel = 0;

	switch (lane_num) {
	case MTK_LANE_0:
		probe_sel = PCIE_LN0_PRB_SEL;
		break;
	case MTK_LANE_1:
		probe_sel = PCIE_LN1_PRB_SEL;
		break;
	case MTK_LANE_2:
		probe_sel = PCIE_LN2_PRB_SEL;
		break;
	case MTK_LANE_3:
		probe_sel = PCIE_LN3_PRB_SEL;
		break;
	default:
		pr_info("Unsupported lane number: [%d]\n", lane_num);
		return -ENOTTY;
	}

	writel_relaxed(bus, phy_base + probe_sel);
	return readl_relaxed(phy_base + PEXTP_DIG_PROBE_OUT);
}

/* For lane margin PHY dump */
static int mtk_pcie_multi_lanes_dump(struct mtk_pcie_info *pcie_smt, u32 port, u32 lane_num)
{
	void __iomem *pcie_phy_sif = pcie_smt->regs[port];
	u32 used_length = 0, val = 0;
	u32 pln[45] = {0};
	int ret_val = 0;

	mtk_pcie_phy_dbg_set_partition(pcie_phy_sif, 0x4);
	pln[0] = lane_num;
	//lane PHYA DA
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0xad);
	pln[1] = PCIE_BITS_VAL(val, 7, 7);//da_xtp_lnx_rx_cdr_en
	pln[2] = PCIE_BITS_VAL(val, 6, 6);//da_xtp_lnx_rx_afe_en
	pln[3] = PCIE_BITS_VAL(val, 2, 2);//da_xtp_lnx_rx_cdr_lck2ref
	pln[4] = PCIE_BITS_VAL(val, 1, 1);//da_xtp_lnx_rx_cdr_track
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0x6f);
	pln[5] = PCIE_BITS_VAL(val, 7, 7);//da_xtp_lnx_rx_aeq_en
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0x12);
	pln[6] = PCIE_BITS_VAL(val, 4, 4);//da_xtp_lnx_tx_drv_en
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0x11);
	pln[7] = PCIE_BITS_VAL(val, 3, 3);//da_xtp_lnx_tx_ser_en
	pln[8] = PCIE_BITS_VAL(val, 0, 0);//da_xtp_lnx_tx_data_en
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0xfd);
	pln[9] = PCIE_BITS_VAL(val, 1, 1);//da_xtp_lnx_tx_cal_ckon
	//lane PHYA AD
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0x8b);
	pln[10] = PCIE_BITS_VAL(val, 6, 6);//ad_xtp_lnx_rx_sgdt_out
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0x8f);
	pln[11] = PCIE_BITS_VAL(val, 5, 5);//ad_xtp_lnx_rx_cdr_dig_sta
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0x8e);
	pln[12] = PCIE_BITS_VAL(val, 6, 0);//ad_xtp_lnx_rx_cdr_kband_state
	pln[13] = PCIE_BITS_VAL(val, 7, 7);//ad_xtp_lnx_rx_cal_kband_done
	//lane FEDIG AD
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0x64);
	pln[14] = PCIE_BITS_VAL(val, 5, 5);//ad_xtp_lnx_rx_cal_done
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0xa8);
	pln[15] = PCIE_BITS_VAL(val, 3, 3);//ad_xtp_lnx_rx_cal_ok
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0xba);
	pln[16] = PCIE_BITS_VAL(val, 5, 0);//ad_xtp_lnx_rx_cal_compos
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0xbb);
	pln[17] = PCIE_BITS_VAL(val, 5, 0);//ad_xtp_lnx_rx_cal_lvshos
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0xbc);
	pln[18] = PCIE_BITS_VAL(val, 5, 0);//ad_xtp_lnx_rx_cal_ctle1ios
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0xbd);
	pln[19] = PCIE_BITS_VAL(val, 5, 0);//ad_xtp_lnx_rx_cal_ctle1vos
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0xc0);
	pln[20] = PCIE_BITS_VAL(val, 5, 0);//ad_xtp_lnx_rx_cal_vgaq1ios
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0xc1);
	pln[21] = PCIE_BITS_VAL(val, 5, 0);//ad_xtp_lnx_rx_cal_vga1vos
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0xc2);
	pln[22] = PCIE_BITS_VAL(val, 5, 0);//ad_xtp_lnx_rx_cal_vga2ios
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0xc3);
	pln[23] = PCIE_BITS_VAL(val, 5, 0);//ad_xtp_lnx_rx_cal_vga2vos
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0xd3);
	pln[24] = PCIE_BITS_VAL(val, 8, 0);//ad_xtp_lnx_rx_aeq_saos
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0x9c);
	pln[25] = PCIE_BITS_VAL(val, 7, 4);//ad_xtp_lnx_rx_cal_state
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0x10a);
	pln[26] = PCIE_BITS_VAL(val, 4, 0);//ad_xtp_lnx_rx_aeq_egeq_ph
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0x69);
	pln[27] = PCIE_BITS_VAL(val, 2, 2);//ad_xtp_lnx_rx_aeq_done
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0x13f);
	pln[28] = PCIE_BITS_VAL(val, 7, 0);//ad_xtp_lnx_rx_aeq_state
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0xd4);
	pln[29] = PCIE_BITS_VAL(val, 2, 0);//ad_xtp_lnx_rx_aeq_strb_att
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0xd6);
	pln[30] = PCIE_BITS_VAL(val, 4, 0);//ad_xtp_lnx_rx_aeq_strb_ctle
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0xd5);
	pln[31] = PCIE_BITS_VAL(val, 4, 0);//ad_xtp_lnx_rx_aeq_strb_vga
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0xdd);
	pln[32] = PCIE_BITS_VAL(val, 4, 0);//ad_xtp_lnx_rx_aeq_strb_dfetp7
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0xdc);
	pln[33] = PCIE_BITS_VAL(val, 4, 0);//ad_xtp_lnx_rx_aeq_strb_dfetp6
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0xdb);
	pln[34] = PCIE_BITS_VAL(val, 4, 0);//ad_xtp_lnx_rx_aeq_strb_dfetp5
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0xda);
	pln[35] = PCIE_BITS_VAL(val, 4, 0);//ad_xtp_lnx_rx_aeq_strb_dfetp4
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0xd9);
	pln[36] = PCIE_BITS_VAL(val, 5, 0);//ad_xtp_lnx_rx_aeq_strb_dfetp3
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0xd8);
	pln[37] = PCIE_BITS_VAL(val, 5, 0);//ad_xtp_lnx_rx_aeq_strb_dfetp2
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0xd7);
	pln[38] = PCIE_BITS_VAL(val, 6, 0);//ad_xtp_lnx_rx_aeq_strb_dfetp1
	//TX DA Interface
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0x02);
	pln[39] = PCIE_BITS_VAL(val, 5, 0);//da_xtp_lnx_tx_lctxcm1
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0x03);
	pln[40] = PCIE_BITS_VAL(val, 5, 0);//da_xtp_lnx_tx_lctxc0
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0x04);
	pln[41] = PCIE_BITS_VAL(val, 5, 0);//da_xtp_lnx_tx_lctxcpl
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0xca);
	pln[42] = PCIE_BITS_VAL(val, 5, 0);//da_xtp_lnx_rx_aeq_rmtxcm1
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0xcb);
	pln[43] = PCIE_BITS_VAL(val, 5, 0);//da_xtp_lnx_rx_aeq_rmtxc0
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, lane_num, 0xcc);
	pln[44] = PCIE_BITS_VAL(val, 5, 0);//da_xtp_lnx_rx_aeq_rmtxcp1

	used_length = strlen(pcie_smt->response);
	pr_info("[%s, %d] used_length=%d, pcie_smt->max_lane[port]=%d\n",
		__func__, __LINE__, used_length, pcie_smt->max_lane[port]);

	ret_val = snprintf(pcie_smt->response + used_length, MAX_BUF_SZ - used_length,
		 "\nLane%d:cdr_en:%#x\nafe_en:%#x\ncdr_lck2ref:%#x\ncdr_track:%#x\n"
		 "aeq_en:%#x\ndrv_en:%#x\nser_en:%#x\ndata_en:%#x\ncal_ckon:%#x\n"
		 "sgdt_out:%#x\ncdr_dig_sta:%#x\ncdr_kband_state:%#x\ncal_kband_done:%#x\n"
		 "cal_done:%#x\ncal_ok:%#x\ncompos:%#x\nlvshos:%#x\nctle1ios:%#x\n"
		 "ctle1vos:%#x\nvgaq1ios:%#x\nvga1vos:%#x\nvga2ios:%#x\nvga2vos:%#x\n"
		 "aeq_saos:%#x\ncal_state:%#x\negeq_ph:%#x\ndone:%#x\nstate:%#x\n"
		 "att:%#x\nctle:%#x\nvga:%#x\ndfetp7:%#x\ndfetp6:%#x\ndfetp5:%#x\n"
		 "dfetp4:%#x\ndfetp3:%#x\ndfetp2:%#x\ndfetp1:%#x\nlctxcm1:%#x\n"
		 "lctxc0:%#x\nlctxcpl:%#x\nrmtxcm1:%#x\nrmtxc0:%#x\nrmtxcp1:%#x\nend",
		 pln[0], pln[1], pln[2], pln[3], pln[4], pln[5], pln[6],
		 pln[7], pln[8], pln[9], pln[10], pln[11], pln[12], pln[13],
		 pln[14], pln[15], pln[16], pln[17], pln[18], pln[19], pln[20],
		 pln[21], pln[22], pln[23], pln[24], pln[25], pln[26], pln[27],
		 pln[28], pln[29], pln[30], pln[31], pln[32], pln[33], pln[34],
		 pln[35], pln[36], pln[37], pln[38], pln[39], pln[40], pln[41],
		 pln[42], pln[43], pln[44]);
	if (ret_val < 0)
		pr_info("[%s, %d] snprintf encoding error\n", __func__, __LINE__);

	return ret_val;
}



/* For lane margin PHY dump */
static int mtk_pcie_lanemargin_phy_dump(struct mtk_pcie_info *pcie_smt, u32 port)
{
	void __iomem *pcie_phy_sif = pcie_smt->regs[port];
	int used_length = 0;
	int ret_val = 0;
	u32 glb[10] = {0};
	u32 val = 0;
	u32 i = 0;

	pr_info("pcie lane margin phy dump start\n");

	used_length = strlen(pcie_smt->response);
	pr_info("[%s, %d] used_length=%d\n", __func__, __LINE__, used_length);

	if (used_length >= MAX_BUF_SZ) {
		pr_info("[%s, %d] Buffer overflow risk, increase macro size.\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret_val = snprintf(pcie_smt->response + used_length, MAX_BUF_SZ - used_length,
			   "\nleft:%d\nright:%d\nup:%d\ndown:%d\n",
			   pcie_smt->eye[0], pcie_smt->eye[1], pcie_smt->eye[2], pcie_smt->eye[3]);
	if (ret_val < 0) {
		pr_info("[%s, %d] snprintf encoding error\n", __func__, __LINE__);
		return ret_val;
	}

	//PHY GLB DA/AD Interface
	mtk_pcie_phy_dbg_set_partition(pcie_phy_sif, 0x4);
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, MTK_LANE_0, 0xff);
	glb[0] = PCIE_BITS_VAL(val, 7, 0);//dummy RG is 0xff
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, MTK_LANE_0, 0xfb);
	glb[1] = PCIE_BITS_VAL(val, 7, 7);//da_xtp_glb_bias_en
	glb[2] = PCIE_BITS_VAL(val, 5, 5);//da_xtp_glb_ckdet_en
	glb[3] = PCIE_BITS_VAL(val, 4, 4);//da_xtp_glb_tpll0_en
	glb[4] = PCIE_BITS_VAL(val, 3, 3);//da_xtp_glb_tpll1_en
	val = mtk_pcie_phy_dbg_read_bus(pcie_phy_sif, MTK_LANE_0, 0xfd);
	glb[5] = PCIE_BITS_VAL(val, 6, 6);//ad_xtp_glb_ckdet_out
	val = readl_relaxed(pcie_phy_sif + 0x9000 + 0x50);
	glb[6] = PCIE_BITS_VAL(val, 15, 8);//ad_xtp_glb_tpll0_dig_mon
	val = readl_relaxed(pcie_phy_sif + 0x9000 + 0x54);
	glb[7] = PCIE_BITS_VAL(val, 0, 0);//ad_xtp_glb_tpll0_kband_cplt
	glb[8] = PCIE_BITS_VAL(val, 1, 1);//ad_xtp_glb_tpll1_kband_cplt
	glb[9] = PCIE_BITS_VAL(val, 31, 24);//ad_xtp_glb_tpll1_dig_mon

	used_length = strlen(pcie_smt->response);
	pr_info("[%s, %d] used_length=%d\n", __func__, __LINE__, used_length);
	ret_val = snprintf(pcie_smt->response + used_length, MAX_BUF_SZ - used_length,
		 "\ndummy:%#x\nbias_en:%#x\nckdet_en:%#x\ntpll0_en:%#x\n"
		 "tpll1_en:%#x\nckdet_out:%#x\ntpll0_dig_mon:%#x\n"
		 "tpll0_kband_cplt:%#x\ntpll1_kband_cplt:%#x\ntpll1_dig_mon:%#x\n",
		 glb[0], glb[1], glb[2], glb[3], glb[4],
		 glb[5], glb[6], glb[7], glb[8], glb[9]);
	if (ret_val < 0) {
		pr_info("[%s, %d] snprintf encoding error\n", __func__, __LINE__);
		return ret_val;
	}

	for (i = 0; i < pcie_smt->test_lane[port]; i++) {
		ret_val = mtk_pcie_multi_lanes_dump(pcie_smt, port, i);
		if (ret_val < 0)
			return ret_val;
	}

	used_length = strlen(pcie_smt->response);
	pr_info("PCIe%d PHY dump:length=%d, info:%s\n", port, used_length, pcie_smt->response);

	pr_info("pcie lane margin phy dump end\n");

	return 0;
}

static int mtk_pcie_loopback_test(struct mtk_pcie_info *pcie_smt, unsigned int port)
{
	int (*loopback_callback)(struct mtk_pcie_info *smt_info, int port);

	if (!pcie_smt->pdev[port])
		return -ENODEV;

	loopback_callback = pcie_smt->test_lib[port]->loopback;
	if (!loopback_callback) {
		pr_info("loopback function not found\n");
		return -EOPNOTSUPP;
	}

	return loopback_callback(pcie_smt, port);
}

static int mtk_pcie_ioctl_loopback(struct mtk_pcie_info *pcie_smt, int port)
{
	if (!pcie_smt) {
		pr_info("pcie_smt not found\n");
		return -ENODEV;
	}

	if (!pcie_smt->regs[port]) {
		pr_info("phy_base is not initialed!\n");
		return -EINVAL;
	}

	if (port >= pcie_smt->max_port) {
		pr_info("Unsupported slot number: [%d]\n", port);
		return -EINVAL;
	}

	return mtk_pcie_loopback_test(pcie_smt, port);
}

static int mtk_pcie_test_open(struct inode *inode, struct file *file)
{
	struct mtk_pcie_info *pcie_smt;

	pcie_smt = container_of(inode->i_cdev, struct mtk_pcie_info, smt_cdev);
	file->private_data = pcie_smt;
	pr_info("%s: successful\n", __func__);

	return 0;
}

static int mtk_pcie_test_release(struct inode *inode, struct file *file)
{

	pr_info("%s: successful\n", __func__);
	return 0;
}

static ssize_t mtk_pcie_test_read(struct file *file, char *buf, size_t count,
		loff_t *ptr)
{

	pr_info("%s: returning zero bytes\n", __func__);
	return 0;
}

static ssize_t mtk_pcie_test_write(struct file *file, const char *buf,
		size_t count, loff_t *ppos)
{

	pr_info("%s: accepting zero bytes\n", __func__);
	return 0;
}

static long mtk_pcie_test_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	struct mtk_pcie_info *pcie_smt = file->private_data;
	char *p = "lane_margin 0 both";
	char buf[BUF_SIZE];

	memset(buf, 0, sizeof(buf));

	switch (cmd) {
	case PCIE_SMT_FTM_CMD:
		if (copy_from_user(buf, (const char __user *)arg, sizeof(buf))) {
			pr_info("%s: copy_from_user failed\n", __func__);
			return -EFAULT;
		}
		p = buf;
		if (mtk_pcie_test_ctrl(p))
			pr_info("%s: mtk_pcie_test_ctrl failed\n", __func__);

		if (copy_to_user((void __user *)arg, pcie_smt->response,
				 strlen(pcie_smt->response) + 1)) {
			pr_info("%s: copy_to_user failed\n", __func__);
			return -EFAULT;
		}
		break;
	case PCIE_SMT_TEST_SLOT:
		pr_info("pcie_smt port: %d\r\n", (unsigned int)arg);

		return mtk_pcie_ioctl_loopback(pcie_smt, (unsigned int)arg);
	default:
		return -ENOTTY;
	}

	return 0;
}

static const struct file_operations pcie_smt_fops = {
	.owner = THIS_MODULE,
	.read = mtk_pcie_test_read,
	.write = mtk_pcie_test_write,
	.unlocked_ioctl = mtk_pcie_test_ioctl,
	.compat_ioctl = mtk_pcie_test_ioctl,
	.open = mtk_pcie_test_open,
	.release = mtk_pcie_test_release,
};

static int mtk_pcie_sysfs_loopback(int argc, char **argv)
{
	unsigned int port = 0, lane = 1;
	int ret = 0;

	if (argc > 1) {
		port = mtk_get_value(argv[1]);
		if (port >= pcie_smt->max_port) {
			pr_info("Unsupported port number: %d\n", port);
			return -EINVAL;
		}
		pr_info("Test port number is %d.\n", port);
	}

	if (argc > 2) {
		lane = mtk_get_value(argv[2]);
		if (lane > pcie_smt->max_lane[port] || lane <= 0) {
			pr_info("Unsupported number of lane: %d*lane\n", lane);
			return -EINVAL;
		}
		pr_info("Test number of lane: %d*lane\n", lane);
		pcie_smt->test_lane[port] = lane;
	}

	ret = mtk_pcie_phy_power_on(pcie_smt, port);
	if (ret)
		goto exit;

	ret = mtk_pcie_loopback_test(pcie_smt, port);

	mtk_pcie_phy_power_off(pcie_smt, port);

exit:
	if (ret)
		strscpy(pcie_smt->response, "test fail", MAX_BUF_SZ);
	else
		strscpy(pcie_smt->response, "test pass", MAX_BUF_SZ);

	return ret;
}

static int mtk_pcie_sysfs_compliance(int argc, char **argv)
{
	int (*compliance_cb)(struct mtk_pcie_info *smt_info, int port,
			    char *cmd, char *preset);
	int port = 0, ret;

	if (argc > 1) {
		port = mtk_get_value(argv[1]);
		if (port >= pcie_smt->max_port) {
			pr_info("Unsupported port number: %d\n", port);
			return -EINVAL;
		}
		pr_info("Test port number is %d.\n", port);
	}

	if (!pcie_smt->pdev[port]) {
		pr_info("Platform device not found\n");
		return -ENODEV;
	}

	compliance_cb = pcie_smt->test_lib[port]->compliance;
	if (!compliance_cb) {
		pr_info("compliance function not found\n");
		return -EOPNOTSUPP;
	}

	if (argc > 2) {
		if (!strcmp(argv[2], "start")) {
			ret = mtk_pcie_phy_power_on(pcie_smt, port);
			if (ret) {
				pr_info("PHY power-on failed with ret = %d\n", ret);
				return ret;
			}

			pr_info("PHY power-on completed. Please excute 'end' cmd to power off after test ended.\n");
		} else if (!strcmp(argv[2], "end")) {
			mtk_pcie_phy_power_off(pcie_smt, port);
			pr_info("PHY power-off completed.\n");
		} else {
			if (!pm_runtime_active(&pcie_smt->phys[port]->dev)) {
				pr_info("Please excute 'start' cmd before excuting compliance command.\n");
				goto out;
			}

			return compliance_cb(pcie_smt, port, argv[2], argc > 3 ? argv[3] : "p7");
		}
	}
out:
	return 0;
}

static int t_pcie_lane_margin(int argc, char **argv)
{
	void __iomem *phy_base = NULL;
	void __iomem *mac_base = NULL;
	unsigned int port = 0, mode = MTK_PCIE_BOTH;
	int ret = 0, i = 0;

	if (argc > 1) {
		port = mtk_get_value(argv[1]);
		pr_info("Test port number is %d.\n", port);
	}

	if (argc > 2) {
		if(!strcmp(argv[2], "up")) {
			pr_info("Test upstream port(EP) LM\n");
			mode = MTK_PCIE_UP;
		} else if(!strcmp(argv[2], "down")) {
			pr_info("Test downstream port(RC) LM\n");
			mode = MTK_PCIE_DN;
		} else {
			pr_info("Test both port(RC+EP) LM\n");
		}
	}

	phy_base = pcie_smt->regs[port];
	mac_base = pcie_smt->mac_regs[port];

	mtk_phy_set_bits(mac_base + PCIE_LMR_0, VOL_MARGIN_SUPP);
	for (i = 0; i < pcie_smt->test_lane[port]; i++)
		mtk_phy_set_bits(phy_base + PEXTP_SIFSLV_DIG_LN_RX2 + PEXTP_DIG_LN_RX2_68 +
				 PEXTP_LANE_OFFSET * i, RG_XTP_LN_RX_EYES_VOL_SUP);

	memset(pcie_smt->eye, 0, sizeof(pcie_smt->eye));
	ret = mtk_pcie_lane_margin_entry(pcie_smt, port, mode);
	if (ret)
		strscpy(pcie_smt->response, "test fail", MAX_BUF_SZ);
	else
		strscpy(pcie_smt->response, "test pass", MAX_BUF_SZ);

	mtk_pcie_lanemargin_phy_dump(pcie_smt, port);

	mtk_phy_clear_bits(mac_base + PCIE_LMR_0, VOL_MARGIN_SUPP);
	for (i = 0; i < pcie_smt->test_lane[port]; i++)
		mtk_phy_clear_bits(phy_base + PEXTP_SIFSLV_DIG_LN_RX2 + PEXTP_DIG_LN_RX2_68 +
				   PEXTP_LANE_OFFSET * i, RG_XTP_LN_RX_EYES_VOL_SUP);

	return ret;
}

static struct cmd_tbl cmd_table[] = {
	{
		"loopback",
		&mtk_pcie_sysfs_loopback,
		"PCIe Loopback Test [port num] [num of lane]"
	},
	{
		"compliance",
		&mtk_pcie_sysfs_compliance,
		"PCIe Compliance Test [port num] [start/end/compliance type] ([optional preset])"
	},
	{
		"lane_margin",
		&t_pcie_lane_margin,
		"pcie lane margin test.[dn/up/both]",
	},
	{}
};

static int mtk_pcie_test_ctrl(char *buf)
{
	struct cmd_tbl *cmd_list = cmd_table;
	char *argv[10];
	int argc = 0;

	do {
		argv[argc] = strsep(&buf, " ");
		pr_info("[%d] %s\r\n", argc, argv[argc]);
		argc++;
	} while (buf);

	while (cmd_list->cmd_name != NULL) {
		if (!strcmp(cmd_list->cmd_name, argv[0])) {
			if (cmd_list->cb_func != NULL)
				return cmd_list->cb_func(argc, argv);

			pr_info("Call back function not found.\n");
			break;
		}
		cmd_list++;
	}

	return -EINVAL;
}

static ssize_t cli_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	struct cmd_tbl *cmd = cmd_table;

	pr_info("Support commands:\n");

	while (cmd->cmd_name != NULL || cmd->help != NULL) {
		pr_info("%-20s %s\n", cmd->cmd_name, cmd->help);
		cmd++;
	}

	pr_info("***************Test cmd user guide****************\n");
	pr_info("[port_num]       0, 1, 2 ....\n");
	pr_info("[lane_num]    1, 2, 4.\n");
	pr_info("[compliance_type]       gen1, gen2_35db, gen2_6db, gen3, gen4\n");
	pr_info("[preset(optional)] only for gen3 and gen4 compliance test\n");
	pr_info("Loopback test usage: echo loopback [port_num] [lane_num] > cli\n");
	pr_info("e.g: echo loopback 0 1 > cli\n");
	pr_info("Compliance test usage: echo compliance [port_num] [compliance_type] ([preset]) > cli\n");
	pr_info("e.g: echo compliance 0 gen2_6db > cli\n");
	pr_info("You can specify the preset type of GEN3 or GEN4 compliance as follow:\n");
	pr_info("e.g.: echo compliance 0 gen3 p4 > cli\n");
	pr_info("We use P7 by default if the preset type was not specified\n");

	return 1;
}

static ssize_t cli_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t n)
{
	int ret = 0;
	char *ch;

	ch = kmalloc(n, GFP_KERNEL);
	if (!ch)
		return -ENOMEM;

	memcpy(ch, buf, n);
	if (n > 0 && ch[n-1] == '\n')
		ch[n-1] = '\0';

	ret = mtk_pcie_test_ctrl(ch);
	if (ret < 0) {
		pr_info("pcie test cli fail\n");
		kfree(ch);
		return -EINVAL;
	}

	kfree(ch);
	pr_info("%s", buf);

	return n;
}

#define pcie_test_attr(_name) \
	static struct kobj_attribute _name##_attr = {   \
		.attr	= {.name = __stringify(_name),  \
			   .mode = 0600,                \
		},                                      \
		.show	= _name##_show,                 \
		.store	= _name##_store,                \
	}
pcie_test_attr(cli);

static struct attribute *pcie_test_func[] = {
	&cli_attr.attr,
	NULL,
};

static struct attribute_group pcie_test_kobj_group = {
	.attrs = pcie_test_func,
};

static struct device_node *mtk_pcie_find_node_by_port(int port)
{
	struct device_node *pcie_node = NULL;

	do {
		pcie_node = of_find_node_by_name(pcie_node, "pcie");
		if (port == of_get_pci_domain_nr(pcie_node))
			return pcie_node;
	} while (pcie_node);

	pr_info("pcie device node not found!\n");

	return NULL;
}

static int mtk_pcie_update_phy_info(struct mtk_pcie_info *pcie_smt, int port)
{
	struct platform_device *pdev;
	struct device_node *pcie_node = NULL;
	struct clk_bulk_data *clks;
	int num_clks;

	pcie_node = mtk_pcie_find_node_by_port(port);
	if (!pcie_node) {
		pr_info("failed to find pcie node\n");
		return -ENODEV;
	}

	pcie_smt->pdev[port] = pdev = of_find_device_by_node(pcie_node);
	if (!pdev) {
		pr_info("failed to find platform device\n");
		return -ENODEV;
	}

	num_clks = devm_clk_bulk_get_all(&pdev->dev, &clks);
	if (num_clks < 0) {
		pr_info("failed to get clocks\n");
		pcie_smt->num_clks[port] = 0;
		return num_clks;
	}

	pcie_smt->num_clks[port] = num_clks;
	pcie_smt->clks[port] = clks;

	pcie_smt->phys[port] = devm_phy_optional_get(&pdev->dev, "pcie-phy");
	if (IS_ERR(pcie_smt->phys[port]))
		return PTR_ERR(pcie_smt->phys[port]);

	pcie_smt->phy_reset[port] = reset_control_get_exclusive(&pdev->dev, "phy");
	if (IS_ERR(pcie_smt->phy_reset[port]))
		return PTR_ERR(pcie_smt->phy_reset[port]);

	pcie_smt->mac_reset[port] = reset_control_get_exclusive(&pdev->dev, "mac");
	if (IS_ERR(pcie_smt->mac_reset[port]))
		return PTR_ERR(pcie_smt->mac_reset[port]);

	return 0;
}

static int mtk_pcie_phy_power_on(struct mtk_pcie_info *smt_info, int port)
{
	struct phy *pcie_phy = smt_info->phys[port];
	struct clk_bulk_data *clks = smt_info->clks[port];
	int err = 0, num_clks = smt_info->num_clks[port];

	if (IS_ERR(pcie_phy) || !num_clks)
		return -ENODEV;

	if (pm_runtime_active(&pcie_phy->dev)) {
		pr_info("PHY already powered on\n");
		return 0;
	}

	/* PHY power on and enable pipe clock */
	reset_control_deassert(smt_info->phy_reset[port]);
	reset_control_deassert(smt_info->mac_reset[port]);

	err = phy_init(pcie_phy);
	if (err) {
		pr_info("failed to init phy\n");
		goto err_init;
	}

	err = phy_power_on(pcie_phy);
	if (err) {
		pr_info("failed to power on phy\n");
		goto err_power_on;
	}

	err = clk_bulk_prepare_enable(num_clks, clks);
	if (err) {
		pr_info("failed to enable clocks\n");
		goto err_clk_init;
	}

	return 0;

err_clk_init:
	phy_power_off(pcie_phy);
err_power_on:
	phy_exit(pcie_phy);
err_init:
	return err;
}

static void mtk_pcie_phy_power_off(struct mtk_pcie_info *smt_info, int port)
{
	struct phy *pcie_phy;

	if (!smt_info)
		return;

	if (port < 0 || port >= ARRAY_SIZE(smt_info->phys)) {
		pr_info("PCIe%d platform device not found!\n", port);
		return;
	}

	pcie_phy = smt_info->phys[port];

	if (!pm_runtime_active(&pcie_phy->dev)) {
		pr_info("PHY already powered off\n");
		return;
	}

	clk_bulk_disable_unprepare(smt_info->num_clks[port],
				   smt_info->clks[port]);
	phy_power_off(pcie_phy);
	phy_exit(pcie_phy);
	reset_control_assert(smt_info->phy_reset[port]);
	reset_control_assert(smt_info->mac_reset[port]);
}

static int __init mtk_pcie_test_init(void)
{
	struct cdev *dev_ctx;
	struct device_node *np;
	dev_t dev = 0;
	int ret, size, i, j;

	pcie_smt = kzalloc(sizeof(*pcie_smt), GFP_KERNEL);
	if (!pcie_smt)
		return -ENOMEM;

	/* match test table*/
	size = sizeof(test_table)/sizeof(struct match_table);
	for (i = 0; i < size; i++) {
		np = of_find_compatible_node(NULL, NULL, test_table[i].compatible);
		if (np != NULL) {
			pr_info("Test table match success!\n");
			break;
		} else if (i == size -1) {
			pr_info("Test table match failed!\n");
			kfree(pcie_smt);
			return -1;
		}
	}

	/* Init struct pcie_smt */
	pcie_smt->name = PCIE_SMT_DEV_NAME;
	pcie_smt->max_port = test_table[i].max_port;
	pcie_smt->test_lane = &test_table[i].test_lane[0];
	pcie_smt->max_lane = &test_table[i].max_lane[0];
	pcie_smt->max_speed = &test_table[i].max_speed[0];

	pcie_smt->regs = kzalloc((pcie_smt->max_port) * (sizeof(*pcie_smt->regs)), GFP_KERNEL);
	if (!pcie_smt->regs) {
		ret = -ENOMEM;
		goto err_alloc_phy;
	}

	pcie_smt->mac_regs = kzalloc((pcie_smt->max_port) * (sizeof(*pcie_smt->mac_regs)), GFP_KERNEL);
	if (!pcie_smt->mac_regs) {
		ret = -ENOMEM;
		goto err_alloc_mac;
	}

	for (j = 0; j < pcie_smt->max_port; j++) {
		pcie_smt->regs[j] = ioremap(test_table[i].phy_base[j], 0xB000);
		if (!pcie_smt->regs[j]) {
			while (j > 0) {
				j--;
				iounmap(pcie_smt->regs[j]);
			}
			ret = -EIO;
			goto err_iomap_phy;
		}
	}

	for (j = 0; j < pcie_smt->max_port; j++) {
		pcie_smt->mac_regs[j] = ioremap(test_table[i].mac_base[j], 0x2000);
		if (!pcie_smt->mac_regs[j]) {
			while (j > 0) {
				j--;
				iounmap(pcie_smt->mac_regs[j]);
			}
			ret = -EIO;
			goto err_iomap_mac;
		}
	}

	for (j = 0; j < pcie_smt->max_port; j++) {
		ret = mtk_pcie_update_phy_info(pcie_smt, j);
		if (ret)
			continue;

		pcie_smt->test_lib[j] = test_table[i].test_lib[j];
	}

	/* dev node support */
	dev_ctx = &pcie_smt->smt_cdev;

	ret = alloc_chrdev_region(&dev, 0, 1, pcie_smt->name);
	if (ret) {
		pr_info("%s alloc_chrdev_region failed!\n", __func__);
		goto err_alloc_cdev_region;
	}

	cdev_init(dev_ctx, &pcie_smt_fops);
	dev_ctx->owner = THIS_MODULE;

	ret = cdev_add(dev_ctx, dev, 1);
	if (ret) {
		pr_info("failed to add cdev\n");
		goto err_alloc_cdev;
	}

	pcie_smt->f_class = class_create(pcie_smt->name);
	device_create(pcie_smt->f_class, NULL, dev_ctx->dev, NULL, "%s", pcie_smt->name);

	/* sysfs support */
	pcie_smt->pcie_test_kobj = kobject_create_and_add(PCIE_SYSFS_NAME, NULL);
	if (IS_ERR(pcie_smt->pcie_test_kobj)) {
		pr_info("PCIe test kobject creat fail!\n");
		ret = PTR_ERR(pcie_smt->pcie_test_kobj);
		goto err_kobj_create;
	}

	ret = sysfs_create_group(pcie_smt->pcie_test_kobj, &pcie_test_kobj_group);
	if (ret) {
		pr_info("PCIe test sysfs creat fail\n");
		goto err_sysfs_create;
	}

	return 0;

err_sysfs_create:
	kobject_del(pcie_smt->pcie_test_kobj);
err_kobj_create:
	device_destroy(pcie_smt->f_class, dev_ctx->dev);
	class_destroy(pcie_smt->f_class);
err_alloc_cdev:
	unregister_chrdev_region(dev, MINORMASK);
err_alloc_cdev_region:
	for (j = 0; j < pcie_smt->max_port; j++)
		iounmap(pcie_smt->mac_regs[j]);
err_iomap_mac:
	for (j = 0; j < pcie_smt->max_port; j++)
		iounmap(pcie_smt->regs[j]);
err_iomap_phy:
	kfree(pcie_smt->mac_regs);
err_alloc_mac:
	kfree(pcie_smt->regs);
err_alloc_phy:
	kfree(pcie_smt);

	return ret;
}

static void __exit mtk_pcie_test_exit(void)
{
	struct cdev *dev_ctx = &pcie_smt->smt_cdev;
	int port;

	/* remove char device */
	device_destroy(pcie_smt->f_class, dev_ctx->dev);
	class_destroy(pcie_smt->f_class);
	unregister_chrdev_region(dev_ctx->dev, MINORMASK);

	/* remove sysfs */
	sysfs_remove_group(pcie_smt->pcie_test_kobj, &pcie_test_kobj_group);
	kobject_del(pcie_smt->pcie_test_kobj);

	if (pcie_smt->regs) {
		for (port = 0; port < pcie_smt->max_port; port++) {
			iounmap(pcie_smt->regs[port]);
			iounmap(pcie_smt->mac_regs[port]);
		}

		kfree(pcie_smt->regs);
		kfree(pcie_smt->mac_regs);
	}

	kfree(pcie_smt);
}

module_init(mtk_pcie_test_init);
module_exit(mtk_pcie_test_exit);
