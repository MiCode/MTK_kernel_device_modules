/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __PCIE_TEST_LIB_H__
#define __PCIE_TEST_LIB_H__

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/io.h>

#define PCIE_PORT_MAX		4

struct pcie_test_lib;

struct mtk_pcie_info {
	char *name;
	void __iomem **regs;
	struct phy *phys[PCIE_PORT_MAX];
	struct clk_bulk_data *clks[PCIE_PORT_MAX];
	int num_clks[PCIE_PORT_MAX];
	struct pcie_test_lib *test_lib[PCIE_PORT_MAX];
	unsigned int *test_lane;
	unsigned int *max_lane;
	unsigned int *max_speed;
	unsigned int max_port;

	/* char device */
	struct cdev smt_cdev;
	struct class *f_class;
	struct kobject *pcie_test_kobj;
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
	unsigned int test_lane[PCIE_PORT_MAX];
	unsigned int max_lane[PCIE_PORT_MAX];
	unsigned int max_speed[PCIE_PORT_MAX];
	unsigned int max_port;
	struct pcie_test_lib *test_lib[PCIE_PORT_MAX];
};

struct pcie_test_lib {
	int (*loopback)(struct mtk_pcie_info *smt_info, int port);
	void (*compliance)(struct mtk_pcie_info *smt_info, int port, char *cmd,
			   char *preset);
};

struct preset_cb {
	char *name;
	void (*cb)(void __iomem *phy_base, int lane_num);
};

extern struct pcie_test_lib pcie_sphy2_test_lib;
extern struct pcie_test_lib pcie_sphy3_test_lib;
extern struct pcie_test_lib pcie_sphy4_test_lib;

#endif
