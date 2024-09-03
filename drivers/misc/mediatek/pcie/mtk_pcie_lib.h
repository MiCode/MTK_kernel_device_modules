/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_PCIE_LIB_H__
#define __MTK_PCIE_LIB_H__

#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/pci.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/slab.h>

#define PCIE_PORT_MAX		4
#define BUF_SIZE		50
#define MAX_BUF_SZ		2000

enum PCIE_PIPE_RATE {
	PIPE_RATE_GEN1 = 0,
	PIPE_RATE_GEN2,
	PIPE_RATE_GEN3,
	PIPE_RATE_GEN4,
};

struct mtk_pcie_info;

struct pcie_test_lib {
	int (*loopback)(struct mtk_pcie_info *smt_info, int port);
	int (*compliance)(struct mtk_pcie_info *smt_info, int port, char *cmd,
			  char *preset);
};

struct preset_cb {
	char *name;
	void (*cb)(void __iomem *phy_base, int lane_num);
};

struct mtk_pcie_info {
	char *name;
	void __iomem **regs;
	void __iomem **mac_regs;
	struct phy *phys[PCIE_PORT_MAX];
	struct clk_bulk_data *clks[PCIE_PORT_MAX];
	struct reset_control *phy_reset[PCIE_PORT_MAX];
	struct reset_control *mac_reset[PCIE_PORT_MAX];
	int num_clks[PCIE_PORT_MAX];
	unsigned int *test_lane;
	unsigned int *max_lane;
	unsigned int *max_speed;
	unsigned int max_port;
	unsigned int eye[4];
	char response[MAX_BUF_SZ];
	struct pcie_test_lib *test_lib[PCIE_PORT_MAX];
	struct platform_device *pdev[PCIE_PORT_MAX];

	/* char device */
	struct cdev smt_cdev;
	struct class *f_class;
	struct kobject *pcie_test_kobj;
};

unsigned int mtk_get_value(char *str);
int mtk_pcie_lane_margin_entry(struct mtk_pcie_info *pcie_smt, int port, int mode);

extern struct pcie_test_lib pcie_sphy2_test_lib;
extern struct pcie_test_lib pcie_sphy3_test_lib;
extern struct pcie_test_lib pcie_sphy4_test_lib;

#endif /*__MTK_PCIE_LIB_H__*/
