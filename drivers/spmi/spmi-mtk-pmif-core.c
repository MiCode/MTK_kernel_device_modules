// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2023 MediaTek Inc.

#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/interrupt.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/sched/clock.h>
#include <linux/sched/mm.h>
#include <linux/spmi.h>
#include <linux/irq.h>
#include "mt-plat/mtk_ccci_common.h"
#include <mtk-spmi-pmic-debug.h>
#include "spmi-mtk.h"
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
#include <mt-plat/mrdump.h>
#endif
#define DUMP_LIMIT 0x1000

#define SWINF_IDLE	0x00
#define SWINF_WFVLDCLR	0x06

#define GET_SWINF(x)	(((x) >> 1) & 0x7)
#define GET_SWINF_ERR(x)	(((x) >> 18) & 0x1)
#define GET_SPMI_NACK_SLVID(x)	(((x) >> 8) & 0xf)
#define GET_SPMI_NACK_ERR_TYPE(x)   (((x) >> 4) & 0xf)

#define PMIF_CMD_REG_0		0
#define PMIF_CMD_REG		1
#define PMIF_CMD_EXT_REG	2
#define PMIF_CMD_EXT_REG_LONG	3

/* 0: SPMI-M, 1: SPMI-P */
#define PMIF_PMIFID_SPMI0		0
#define PMIF_PMIFID_SPMI1		1

#define PMIF_DELAY_US   10
//#define PMIF_TIMEOUT    (10 * 1000)
#define PMIF_TIMEOUT    (100 * 1000)

#define PMIF_CHAN_OFFSET 0x5

#define SPMI_OP_ST_BUSY 1

#define MAX_MONITOR_LIST_SIZE	64
#define MONITOR_PAIR_ITEM_NUM	2

#define PMIF_CH_MD_DVFS_HW	1

#define PMIF_IRQDESC(name) { #name, pmif_##name##_irq_handler, -1}

#define MT6316_PMIC_HWCID_L_ADDR                                        (0x208)
#define MT6316_PMIC_HWCID_H_ADDR                                 (0x209)
#define PMIC_RG_INT_RAW_STATUS_ADDR_MON_HIT_ADDR                 (0x240)
#define PMIC_RG_SPMI_DBGMUX_OUT_L_ADDR                           (0x42b)
#define PMIC_RG_SPMI_DBGMUX_OUT_H_ADDR                           (0x42c)
#define PMIC_RG_SPMI_DBGMUX_SEL_ADDR                             (0x42d)
#define PMIC_RG_SPMI_DBGMUX_SEL_MASK                             (0x3f)
#define PMIC_RG_SPMI_DBGMUX_SEL_SHIFT                            (0)
#define PMIC_RG_DEBUG_EN_TRIG_ADDR                               (0x42d)
#define PMIC_RG_DEBUG_EN_TRIG_SHIFT                              (6)
#define PMIC_RG_DEBUG_DIS_TRIG_ADDR                              (0x42d)
#define PMIC_RG_DEBUG_DIS_TRIG_SHIFT                             (7)
#define PMIC_RG_DEBUG_EN_RD_CMD_ADDR                             (0x42e)

#define PMIC_RG_SYSTEM_INFO_CON0_L                              (0xd9b)
#define PMIC_RG_SYSTEM_INFO_CON0_H                              (0xd9c)
#define PMIC_RG_SYSTEM_INFO_CON1_L                              (0xd9d)
#define PMIC_RG_SYSTEM_INFO_CON1_H                              (0xd9e)
#define PMIC_RG_SYSTEM_INFO_CON2_L                              (0xd9f)
#define PMIC_RG_SYSTEM_INFO_CON2_H                              (0xda0)

enum {
	SPMI_WRITE_NACK = 1,
	SPMI_READ_PARITY_ERROR,
	SPMI_BYTECNT_ERROR,
	SPMI_GIP_READ_ERROR,
	PMIF_MPU_VIOLATION
};

enum {
	SPMI_MASTER_0 = 0,
	SPMI_MASTER_1,
	SPMI_MASTER_P_1,
	SPMI_MASTER_MAX
};

enum spmi_slave {
	SPMI_SLAVE_0 = 0,
	SPMI_SLAVE_1,
	SPMI_SLAVE_2,
	SPMI_SLAVE_3,
	SPMI_SLAVE_4,
	SPMI_SLAVE_5,
	SPMI_SLAVE_6,
	SPMI_SLAVE_7,
	SPMI_SLAVE_8,
	SPMI_SLAVE_9,
	SPMI_SLAVE_10,
	SPMI_SLAVE_11,
	SPMI_SLAVE_12,
	SPMI_SLAVE_13,
	SPMI_SLAVE_14,
	SPMI_SLAVE_15
};

struct pmif_irq_desc {
	const char *name;
	irq_handler_t irq_handler;
	int irq;
};

enum pmif_regs {
	PMIF_INIT_DONE,
	PMIF_INF_EN,
	MD_AUXADC_RDATA_0_ADDR,
	MD_AUXADC_RDATA_1_ADDR,
	MD_AUXADC_RDATA_2_ADDR,
	PMIF_ARB_EN,
	PMIF_CMDISSUE_EN,
	PMIF_TIMER_CTRL,
	PMIF_SPI_MODE_CTRL,
	PMIF_IRQ_EVENT_EN_0,
	PMIF_IRQ_FLAG_0,
	PMIF_IRQ_CLR_0,
	PMIF_IRQ_EVENT_EN_1,
	PMIF_IRQ_FLAG_1,
	PMIF_IRQ_CLR_1,
	PMIF_IRQ_EVENT_EN_2,
	PMIF_IRQ_FLAG_2,
	PMIF_IRQ_CLR_2,
	PMIF_IRQ_EVENT_EN_3,
	PMIF_IRQ_FLAG_3,
	PMIF_IRQ_CLR_3,
	PMIF_IRQ_EVENT_EN_4,
	PMIF_IRQ_FLAG_4,
	PMIF_IRQ_CLR_4,
	PMIF_WDT_EVENT_EN_0,
	PMIF_WDT_FLAG_0,
	PMIF_WDT_EVENT_EN_1,
	PMIF_WDT_FLAG_1,
	PMIF_SWINF_0_STA,
	PMIF_SWINF_0_WDATA_31_0,
	PMIF_SWINF_0_RDATA_31_0,
	PMIF_SWINF_0_ACC,
	PMIF_SWINF_0_VLD_CLR,
	PMIF_SWINF_1_STA,
	PMIF_SWINF_1_WDATA_31_0,
	PMIF_SWINF_1_RDATA_31_0,
	PMIF_SWINF_1_ACC,
	PMIF_SWINF_1_VLD_CLR,
	PMIF_SWINF_2_STA,
	PMIF_SWINF_2_WDATA_31_0,
	PMIF_SWINF_2_RDATA_31_0,
	PMIF_SWINF_2_ACC,
	PMIF_SWINF_2_VLD_CLR,
	PMIF_SWINF_3_STA,
	PMIF_SWINF_3_WDATA_31_0,
	PMIF_SWINF_3_RDATA_31_0,
	PMIF_SWINF_3_ACC,
	PMIF_SWINF_3_VLD_CLR,
	PMIF_PMIC_SWINF_0_PER,
	PMIF_PMIC_SWINF_1_PER,
	PMIF_ACC_VIO_INFO_0,
	PMIF_ACC_VIO_INFO_1,
	PMIF_ACC_VIO_INFO_2,
};
static const u32 mt6xxx_regs[] = {
	[PMIF_INIT_DONE] =			0x0000,
	[PMIF_INF_EN] =				0x0024,
	[MD_AUXADC_RDATA_0_ADDR] =		0x0080,
	[MD_AUXADC_RDATA_1_ADDR] =		0x0084,
	[MD_AUXADC_RDATA_2_ADDR] =		0x0088,
	[PMIF_ARB_EN] =				0x0150,
	[PMIF_CMDISSUE_EN] =			0x03B8,
	[PMIF_TIMER_CTRL] =			0x03E4,
	[PMIF_SPI_MODE_CTRL] =			0x0408,
	[PMIF_IRQ_EVENT_EN_0] =			0x0420,
	[PMIF_IRQ_FLAG_0] =			0x0428,
	[PMIF_IRQ_CLR_0] =			0x042C,
	[PMIF_IRQ_EVENT_EN_1] =			0x0430,
	[PMIF_IRQ_FLAG_1] =			0x0438,
	[PMIF_IRQ_CLR_1] =			0x043C,
	[PMIF_IRQ_EVENT_EN_2] =			0x0440,
	[PMIF_IRQ_FLAG_2] =			0x0448,
	[PMIF_IRQ_CLR_2] =			0x044C,
	[PMIF_IRQ_EVENT_EN_3] =			0x0450,
	[PMIF_IRQ_FLAG_3] =			0x0458,
	[PMIF_IRQ_CLR_3] =			0x045C,
	[PMIF_IRQ_EVENT_EN_4] =			0x0460,
	[PMIF_IRQ_FLAG_4] =			0x0468,
	[PMIF_IRQ_CLR_4] =			0x046C,
	[PMIF_WDT_EVENT_EN_0] =			0x0474,
	[PMIF_WDT_FLAG_0] =			0x0478,
	[PMIF_WDT_EVENT_EN_1] =			0x047C,
	[PMIF_WDT_FLAG_1] =			0x0480,
	[PMIF_SWINF_0_ACC] =			0x0800,
	[PMIF_SWINF_0_WDATA_31_0] =		0x0804,
	[PMIF_SWINF_0_RDATA_31_0] =		0x0814,
	[PMIF_SWINF_0_VLD_CLR] =		0x0824,
	[PMIF_SWINF_0_STA] =			0x0828,
	[PMIF_SWINF_1_ACC] =			0x0840,
	[PMIF_SWINF_1_WDATA_31_0] =		0x0844,
	[PMIF_SWINF_1_RDATA_31_0] =		0x0854,
	[PMIF_SWINF_1_VLD_CLR] =		0x0864,
	[PMIF_SWINF_1_STA] =			0x0868,
	[PMIF_SWINF_2_ACC] =			0x0880,
	[PMIF_SWINF_2_WDATA_31_0] =		0x0884,
	[PMIF_SWINF_2_RDATA_31_0] =		0x0894,
	[PMIF_SWINF_2_VLD_CLR] =		0x08A4,
	[PMIF_SWINF_2_STA] =			0x08A8,
	[PMIF_SWINF_3_ACC] =			0x08C0,
	[PMIF_SWINF_3_WDATA_31_0] =		0x08C4,
	[PMIF_SWINF_3_RDATA_31_0] =		0x08D4,
	[PMIF_SWINF_3_VLD_CLR] =		0x08E4,
	[PMIF_SWINF_3_STA] =			0x08E8,
	[PMIF_PMIC_SWINF_0_PER] =		0x093C,
	[PMIF_PMIC_SWINF_1_PER] =		0x0940,
	[PMIF_ACC_VIO_INFO_0] =			0x0980,
	[PMIF_ACC_VIO_INFO_1] =			0x0984,
	[PMIF_ACC_VIO_INFO_2] =			0x0988,
};

static const u32 mt6853_regs[] = {
	[PMIF_INIT_DONE] =			0x0000,
	[PMIF_INF_EN] =				0x0024,
	[PMIF_ARB_EN] =				0x0150,
	[PMIF_CMDISSUE_EN] =			0x03B8,
	[PMIF_TIMER_CTRL] =			0x03E4,
	[PMIF_SPI_MODE_CTRL] =			0x0408,
	[PMIF_IRQ_EVENT_EN_0] =			0x0420,
	[PMIF_IRQ_FLAG_0] =			0x0428,
	[PMIF_IRQ_CLR_0] =			0x042C,
	[PMIF_IRQ_EVENT_EN_1] =			0x0430,
	[PMIF_IRQ_FLAG_1] =			0x0438,
	[PMIF_IRQ_CLR_1] =			0x043C,
	[PMIF_IRQ_EVENT_EN_2] =			0x0440,
	[PMIF_IRQ_FLAG_2] =			0x0448,
	[PMIF_IRQ_CLR_2] =			0x044C,
	[PMIF_IRQ_EVENT_EN_3] =			0x0450,
	[PMIF_IRQ_FLAG_3] =			0x0458,
	[PMIF_IRQ_CLR_3] =			0x045C,
	[PMIF_IRQ_EVENT_EN_4] =			0x0460,
	[PMIF_IRQ_FLAG_4] =			0x0468,
	[PMIF_IRQ_CLR_4] =			0x046C,
	[PMIF_WDT_EVENT_EN_0] =			0x0474,
	[PMIF_WDT_FLAG_0] =			0x0478,
	[PMIF_WDT_EVENT_EN_1] =			0x047C,
	[PMIF_WDT_FLAG_1] =			0x0480,
	[PMIF_SWINF_0_ACC] =			0x0C00,
	[PMIF_SWINF_0_WDATA_31_0] =		0x0C04,
	[PMIF_SWINF_0_RDATA_31_0] =		0x0C14,
	[PMIF_SWINF_0_VLD_CLR] =		0x0C24,
	[PMIF_SWINF_0_STA] =			0x0C28,
	[PMIF_SWINF_1_ACC] =			0x0C40,
	[PMIF_SWINF_1_WDATA_31_0] =		0x0C44,
	[PMIF_SWINF_1_RDATA_31_0] =		0x0C54,
	[PMIF_SWINF_1_VLD_CLR] =		0x0C64,
	[PMIF_SWINF_1_STA] =			0x0C68,
	[PMIF_SWINF_2_ACC] =			0x0C80,
	[PMIF_SWINF_2_WDATA_31_0] =		0x0C84,
	[PMIF_SWINF_2_RDATA_31_0] =		0x0C94,
	[PMIF_SWINF_2_VLD_CLR] =		0x0CA4,
	[PMIF_SWINF_2_STA] =			0x0CA8,
	[PMIF_SWINF_3_ACC] =			0x0CC0,
	[PMIF_SWINF_3_WDATA_31_0] =		0x0CC4,
	[PMIF_SWINF_3_RDATA_31_0] =		0x0CD4,
	[PMIF_SWINF_3_VLD_CLR] =		0x0CE4,
	[PMIF_SWINF_3_STA] =			0x0CE8,
};

static const u32 mt6873_regs[] = {
	[PMIF_INIT_DONE] =	0x0000,
	[PMIF_INF_EN] =		0x0024,
	[PMIF_ARB_EN] =		0x0150,
	[PMIF_CMDISSUE_EN] =	0x03B4,
	[PMIF_TIMER_CTRL] =	0x03E0,
	[PMIF_SPI_MODE_CTRL] =	0x0400,
	[PMIF_IRQ_EVENT_EN_0] =	0x0418,
	[PMIF_IRQ_FLAG_0] =	0x0420,
	[PMIF_IRQ_CLR_0] =	0x0424,
	[PMIF_IRQ_EVENT_EN_1] =	0x0428,
	[PMIF_IRQ_FLAG_1] =	0x0430,
	[PMIF_IRQ_CLR_1] =	0x0434,
	[PMIF_IRQ_EVENT_EN_2] =	0x0438,
	[PMIF_IRQ_FLAG_2] =	0x0440,
	[PMIF_IRQ_CLR_2] =	0x0444,
	[PMIF_IRQ_EVENT_EN_3] =	0x0448,
	[PMIF_IRQ_FLAG_3] =	0x0450,
	[PMIF_IRQ_CLR_3] =	0x0454,
	[PMIF_IRQ_EVENT_EN_4] =	0x0458,
	[PMIF_IRQ_FLAG_4] =	0x0460,
	[PMIF_IRQ_CLR_4] =	0x0464,
	[PMIF_WDT_EVENT_EN_0] =	0x046C,
	[PMIF_WDT_FLAG_0] =	0x0470,
	[PMIF_WDT_EVENT_EN_1] =	0x0474,
	[PMIF_WDT_FLAG_1] =	0x0478,
	[PMIF_SWINF_0_ACC] =	0x0C00,
	[PMIF_SWINF_0_WDATA_31_0] =	0x0C04,
	[PMIF_SWINF_0_RDATA_31_0] =	0x0C14,
	[PMIF_SWINF_0_VLD_CLR] =	0x0C24,
	[PMIF_SWINF_0_STA] =	0x0C28,
	[PMIF_SWINF_1_ACC] =	0x0C40,
	[PMIF_SWINF_1_WDATA_31_0] =	0x0C44,
	[PMIF_SWINF_1_RDATA_31_0] =	0x0C54,
	[PMIF_SWINF_1_VLD_CLR] =	0x0C64,
	[PMIF_SWINF_1_STA] =	0x0C68,
	[PMIF_SWINF_2_ACC] =	0x0C80,
	[PMIF_SWINF_2_WDATA_31_0] =	0x0C84,
	[PMIF_SWINF_2_RDATA_31_0] =	0x0C94,
	[PMIF_SWINF_2_VLD_CLR] =	0x0CA4,
	[PMIF_SWINF_2_STA] =	0x0CA8,
	[PMIF_SWINF_3_ACC] =	0x0CC0,
	[PMIF_SWINF_3_WDATA_31_0] =	0x0CC4,
	[PMIF_SWINF_3_RDATA_31_0] =	0x0CD4,
	[PMIF_SWINF_3_VLD_CLR] =	0x0CE4,
	[PMIF_SWINF_3_STA] =	0x0CE8,
};

static const u32 mt6853_spmi_regs[] = {
	[SPMI_OP_ST_CTRL] =	0x0000,
	[SPMI_GRP_ID_EN] =	0x0004,
	[SPMI_OP_ST_STA] =	0x0008,
	[SPMI_MST_SAMPL] =	0x000C,
	[SPMI_MST_REQ_EN] =	0x0010,
	[SPMI_MST_RCS_CTRL] =	0x0014,
	[SPMI_SLV_3_0_EINT] =	0x0020,
	[SPMI_SLV_7_4_EINT] =	0x0024,
	[SPMI_SLV_B_8_EINT] =	0x0028,
	[SPMI_SLV_F_C_EINT] =	0x002C,
	[SPMI_REC_CTRL] =	0x0040,
	[SPMI_REC0] =		0x0044,
	[SPMI_REC1] =		0x0048,
	[SPMI_REC2] =		0x004C,
	[SPMI_REC3] =		0x0050,
	[SPMI_REC4] =		0x0054,
	[SPMI_REC_CMD_DEC] =	0x005C,
	[SPMI_WDT_REC] =	0x0060,
	[SPMI_DEC_DBG] =	0x00F8,
	[SPMI_MST_DBG] =	0x00FC,
};

static const u32 mt6873_spmi_regs[] = {
	[SPMI_OP_ST_CTRL] =	0x0000,
	[SPMI_GRP_ID_EN] =	0x0004,
	[SPMI_OP_ST_STA] =	0x0008,
	[SPMI_MST_SAMPL] =	0x000c,
	[SPMI_MST_REQ_EN] =	0x0010,
	[SPMI_REC_CTRL] =	0x0040,
	[SPMI_REC0] =		0x0044,
	[SPMI_REC1] =		0x0048,
	[SPMI_REC2] =		0x004c,
	[SPMI_REC3] =		0x0050,
	[SPMI_REC4] =		0x0054,
	[SPMI_MST_DBG] =	0x00fc,
};

enum {
	SPMI_RCS_SR_BIT,
	SPMI_RCS_A_BIT
};

enum {
	SPMI_RCS_MST_W = 1,
	SPMI_RCS_SLV_W = 3
};

enum {
	IRQ_PMIF_SWINF_ACC_ERR_0 = 3,
	IRQ_PMIF_SWINF_ACC_ERR_1 = 4,
	IRQ_PMIF_SWINF_ACC_ERR_2 = 5,
	IRQ_PMIF_SWINF_ACC_ERR_3 = 6,
	IRQ_PMIF_SWINF_ACC_ERR_4 = 7,
	IRQ_PMIF_HWINF_0_CMD_VIO_0  = 7,
	IRQ_PMIF_SWINF_ACC_ERR_5 = 8,
	IRQ_PMIF_SWINF_ACC_ERR_0_V2 = 23,
	IRQ_PMIF_SWINF_ACC_ERR_1_V2 = 24,
	IRQ_PMIF_SWINF_ACC_ERR_2_V2 = 25,
	IRQ_PMIF_SWINF_ACC_ERR_3_V2 = 26,
	IRQ_PMIF_SWINF_ACC_ERR_4_V2 = 27,
	IRQ_PMIF_SWINF_ACC_ERR_5_V2 = 28,
	IRQ_HW_MONITOR_V4           = 29,
	IRQ_WDT_V4                  = 30,
	IRQ_PMIF_HWINF_0_CMD_VIO_1  = 30,
	IRQ_ALL_PMIC_MPU_VIO_V4     = 31,
};

enum {
	MT6316_E3 = 0,
	MT6316_E4 = 1,
};

static struct spmi_dev spmidev[16];
static struct spmi_nack_monitor_pair nack_monitor_list[MAX_MONITOR_LIST_SIZE];
static int nack_monitor_list_size;

struct spmi_dev *get_spmi_device(int slaveid)
{
	if ((slaveid < SPMI_SLAVE_0) || (slaveid > SPMI_SLAVE_15)) {
		pr_notice("[SPMI] get_spmi_device invalid slave id %d\n", slaveid);
		return NULL;
	}

	if (!spmidev[slaveid].exist)
		pr_notice("[SPMI] slave id %d is not existed\n", slaveid);

	return &spmidev[slaveid];
}

static void spmi_dev_parse(struct platform_device *pdev)
{
	int i = 0, j = 0, ret = 0;
	u32 spmi_dev_mask = 0;

	ret = of_property_read_u32(pdev->dev.of_node, "spmi-dev-mask",
				  &spmi_dev_mask);
	if (ret)
		dev_info(&pdev->dev, "No spmi-dev-mask found in dts, default all PMIC on SPMI-M\n");

	j = spmi_dev_mask;
	for (i = 0; i < 16; i++) {
		if (j & (1 << i))
			spmidev[i].path = 1; //slvid i is in path p
		else
			spmidev[i].path = 0;
	}
}

static void spmi_nack_monitor_list_parse(struct platform_device *pdev)
{
	int i = 0, ret = 0;
	int nack_monitor_list_arr_size = 0;

	nack_monitor_list_arr_size = of_property_count_u32_elems(pdev->dev.of_node, "spmi-nack-monitor-list");
	if (nack_monitor_list_arr_size < 0) {
		dev_notice(&pdev->dev,
			"Failed to get spmi-nack-monitor-list, ret = %d\n", nack_monitor_list_arr_size);
		return;
	}

	/* NACK monitor list are not in pair */
	if (nack_monitor_list_arr_size % MONITOR_PAIR_ITEM_NUM != 0) {
		dev_notice(&pdev->dev,
			"SPMI nack monitor list slvid, addr are not in pair, array size = %d\n",
			nack_monitor_list_arr_size);
	}

	nack_monitor_list_size = nack_monitor_list_arr_size / MONITOR_PAIR_ITEM_NUM;
	dev_notice(&pdev->dev,
			"nack_monitor_list_arr_size = %d, nack_monitor_list_size = %d\n",
			nack_monitor_list_arr_size,
			nack_monitor_list_size);

	/* Monitor list size too large */
	if (nack_monitor_list_size > MAX_MONITOR_LIST_SIZE) {
		dev_notice(&pdev->dev,
			"SPMI nack monitor list size is too large = %d, max size is %d\n",
			nack_monitor_list_size, MAX_MONITOR_LIST_SIZE);
		dev_notice(&pdev->dev,
			"Register over number %d in spmi-nack-monitor-list will be ignored\n",
			MAX_MONITOR_LIST_SIZE - 1);
		nack_monitor_list_size = MAX_MONITOR_LIST_SIZE;
	}

	for (i = 0; i < nack_monitor_list_size; i++) {
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"spmi-nack-monitor-list",
				i * MONITOR_PAIR_ITEM_NUM,
				&nack_monitor_list[i].slvid);
		if (ret) {
			dev_notice(&pdev->dev,
				"spmi-nack-monitor-list slvid read fail\n");
		}

		ret = of_property_read_u32_index(pdev->dev.of_node,
				"spmi-nack-monitor-list",
				i * MONITOR_PAIR_ITEM_NUM + 1,
				&nack_monitor_list[i].addr);
		if (ret) {
			dev_notice(&pdev->dev,
				"spmi-nack-monitor-list addr read fail\n");
		}
	}
}

static bool in_spmi_nack_monitor_list(u32 spmi_nack)
{
	unsigned int i = 0;
	unsigned int spmi_nack_addr = 0;

	for (i = 0; i < nack_monitor_list_size; i++) {
		if (((spmi_nack & 0x0f00) >> 8) == nack_monitor_list[i].slvid) {
			spmi_nack_addr = (spmi_nack & 0xffff0000) >> 16;
			if (spmi_nack_addr == nack_monitor_list[i].addr) {
				pr_notice("%s Match SPMI NACK moinitor list\n", __func__);
				pr_notice("%s Monitor list pair: %d, slvid: 0x%x, addr: 0x%x\n",
					__func__, i,
					nack_monitor_list[i].slvid,
					nack_monitor_list[i].addr);
				return true;
			}
		}
	}
	pr_notice("%s Not in SPMI NACK monitor list\n", __func__);
	return false;
}

static int mt6316_revision_check(struct pmif *arb, unsigned int slvid)
{
	u8 rdata = 0;

	arb->spmic->read_cmd(arb->spmic, SPMI_CMD_EXT_READL,
		slvid, MT6316_PMIC_HWCID_L_ADDR, &rdata, 1);

	if (rdata == 0x30)
		return MT6316_E4;
	else
		return MT6316_E3;
}

unsigned long long get_current_time_ms(void)
{
	unsigned long long cur_ts;

	cur_ts = sched_clock();
	do_div(cur_ts, 1000000);
	return cur_ts;
}

static void store_nack_info(struct pmif *arb, unsigned int nack_rec0, unsigned int nack_rec1)
{
	u8 wdata = 0;
	u8 slvid = 0;
	u8 error_type = 0;
	u8 cur_time_24 = 0, cur_time_23_16 = 0, cur_time_15_8 = 0, cur_time_7_0 = 0;
	unsigned long long cur_krn_time_sec = 0;

	/* Address [7:0] */
	wdata = (nack_rec0 >> 16) & 0xff;
	arb->spmic->write_cmd(arb->spmic, SPMI_CMD_EXT_WRITEL, 0x4,
		PMIC_RG_SYSTEM_INFO_CON0_H, &wdata, 1);

	/* Data [7:0] */
	wdata = nack_rec1 & 0xff;
	arb->spmic->write_cmd(arb->spmic, SPMI_CMD_EXT_WRITEL, 0x4,
		PMIC_RG_SYSTEM_INFO_CON0_L, &wdata, 1);

	/* Slvid [7:4] | error_type [3:1] | timer_24[0] */
	slvid = GET_SPMI_NACK_SLVID(nack_rec0);

	wdata = slvid << 4;

	error_type = GET_SPMI_NACK_ERR_TYPE(nack_rec0);

	if (error_type & (0x1 << 0)) {
		wdata |= (SPMI_WRITE_NACK << 1);
	} else if (error_type & (0x1 << 1)) {
		wdata |= (SPMI_READ_PARITY_ERROR << 1);
	} else if (error_type & (0x1 << 2)) {
		wdata |= (SPMI_BYTECNT_ERROR << 1);
	} else if (error_type & (0x1 << 3)) {
		wdata |= (SPMI_GIP_READ_ERROR << 1);
	} else {
		wdata |= (PMIF_MPU_VIOLATION << 1);
	}

	cur_krn_time_sec = get_current_time_ms() / 1000;
	cur_time_24 = (cur_krn_time_sec >> 24) & (0x1);
	cur_time_23_16 = (cur_krn_time_sec >> 16) & (0xff);
	cur_time_15_8 = (cur_krn_time_sec >> 8) & (0xff);
	cur_time_7_0 = (cur_krn_time_sec) & (0xff);

	wdata |= cur_time_24;
	arb->spmic->write_cmd(arb->spmic, SPMI_CMD_EXT_WRITEL, 0x4,
		PMIC_RG_SYSTEM_INFO_CON1_H, &wdata, 1);

	/* timer_23_16[7:0] */
	wdata = cur_time_23_16;
	arb->spmic->write_cmd(arb->spmic, SPMI_CMD_EXT_WRITEL, 0x4,
		PMIC_RG_SYSTEM_INFO_CON1_L, &wdata, 1);

	/* timer_15_8[7:0] */
	wdata = cur_time_15_8;
	arb->spmic->write_cmd(arb->spmic, SPMI_CMD_EXT_WRITEL, 0x4,
		PMIC_RG_SYSTEM_INFO_CON2_H, &wdata, 1);

	/* timer_7_0[7:0] */
	wdata = cur_time_7_0;
	arb->spmic->write_cmd(arb->spmic, SPMI_CMD_EXT_WRITEL, 0x4,
		PMIC_RG_SYSTEM_INFO_CON2_L, &wdata, 1);
}

static u32 pmif_readl(void __iomem *addr, struct pmif *arb, enum pmif_regs reg)
{
	return readl(addr + arb->regs[reg]);
}

static void pmif_writel(void __iomem *addr, struct pmif *arb, u32 val, enum pmif_regs reg)
{
	writel(val, addr + arb->regs[reg]);
}

static void mtk_spmi_writel(void __iomem *addr, struct pmif *arb, u32 val, enum spmi_regs reg)
{
	writel(val, addr + arb->spmimst_regs[reg]);
}


static u32 mtk_spmi_readl(void __iomem *addr, struct pmif *arb, enum spmi_regs reg)
{
	return readl(addr + arb->spmimst_regs[reg]);
}

static bool pmif_is_fsm_vldclr(void __iomem *addr, struct pmif *arb)
{
	u32 reg_rdata;

	reg_rdata = pmif_readl(addr, arb, arb->chan.ch_sta);
	return GET_SWINF(reg_rdata) == SWINF_WFVLDCLR;
}

static int pmif_arb_cmd(struct spmi_controller *ctrl, u8 opc, u8 sid)
{
	struct pmif *arb = spmi_controller_get_drvdata(ctrl);
	u32 rdata;
	u8 cmd;
	int ret;

	/* Check the opcode */
	if (opc == SPMI_CMD_RESET)
		cmd = 0;
	else if (opc == SPMI_CMD_SLEEP)
		cmd = 1;
	else if (opc == SPMI_CMD_SHUTDOWN)
		cmd = 2;
	else if (opc == SPMI_CMD_WAKEUP)
		cmd = 3;
	else
		return -EINVAL;

	mtk_spmi_writel(arb->spmimst_base[spmidev[sid].path], arb, (cmd << 0x4) | sid, SPMI_OP_ST_CTRL);
	ret = readl_poll_timeout_atomic(arb->spmimst_base[spmidev[sid].path] + arb->spmimst_regs[SPMI_OP_ST_STA],
					rdata, (rdata & SPMI_OP_ST_BUSY) == SPMI_OP_ST_BUSY,
					PMIF_DELAY_US, PMIF_TIMEOUT);
	if (ret < 0)
		dev_err(&ctrl->dev, "timeout, err = %d\r\n", ret);

	return ret;
}

static int pmif_spmi_read_cmd(struct spmi_controller *ctrl, u8 opc, u8 sid,
			      u16 addr, u8 *buf, size_t len)
{
	struct pmif *arb = spmi_controller_get_drvdata(ctrl);
	struct ch_reg *inf_reg = NULL;
	int ret;
	u32 data = 0;
	u8 bc = len - 1;
	unsigned long flags_m = 0;
	int swinf_err = 0;

	/* Check for argument validation. */
	if (sid & ~(0xf))
		return -EINVAL;

	if (!arb)
		return -EINVAL;

	inf_reg = &arb->chan;
	/* Check the opcode */
	if (opc >= 0x60 && opc <= 0x7f)
		opc = PMIF_CMD_REG;
	else if (opc >= 0x20 && opc <= 0x2f)
		opc = PMIF_CMD_EXT_REG_LONG;
	else if (opc >= 0x38 && opc <= 0x3f)
		opc = PMIF_CMD_EXT_REG_LONG;
	else
		return -EINVAL;

	raw_spin_lock_irqsave(&arb->lock_m, flags_m);

	/* Wait for Software Interface FSM state to be IDLE. */
	ret = readl_poll_timeout_atomic(arb->pmif_base[spmidev[sid].path] + arb->regs[inf_reg->ch_sta],
					data, GET_SWINF(data) == SWINF_IDLE,
					PMIF_DELAY_US, PMIF_TIMEOUT);
	if (ret < 0) {
		dev_err(&ctrl->dev, "check IDLE timeout, read 0x%x, sta=0x%x, SPMI_DBG=0x%x\n",
			addr, pmif_readl(arb->pmif_base[spmidev[sid].path], arb, inf_reg->ch_sta),
			readl(arb->spmimst_base[spmidev[sid].path] + arb->spmimst_regs[SPMI_MST_DBG]));
		/* set channel ready if the data has transferred */
		if (pmif_is_fsm_vldclr(arb->pmif_base[spmidev[sid].path], arb))
			pmif_writel(arb->pmif_base[spmidev[sid].path], arb, 1, inf_reg->ch_rdy);

		raw_spin_unlock_irqrestore(&arb->lock_m, flags_m);

		return ret;
	}

	/* Send the command. */
	pmif_writel(arb->pmif_base[spmidev[sid].path], arb,
		    (opc << 30) | (sid << 24) | (bc << 16) | addr,
		    inf_reg->ch_send);

	/* Wait for Software Interface FSM state to be WFVLDCLR,
	 *
	 * read the data and clear the valid flag.
	 */
	ret = readl_poll_timeout_atomic(arb->pmif_base[spmidev[sid].path] + arb->regs[inf_reg->ch_sta],
					data, GET_SWINF(data) == SWINF_WFVLDCLR,
					PMIF_DELAY_US, PMIF_TIMEOUT);
	if (ret < 0) {
		dev_err(&ctrl->dev, "check WFVLDCLR timeout, read 0x%x, sta=0x%x, SPMI_DBG=0x%x\n",
			addr, pmif_readl(arb->pmif_base[spmidev[sid].path], arb, inf_reg->ch_sta),
			readl(arb->spmimst_base[spmidev[sid].path] + arb->spmimst_regs[SPMI_MST_DBG]));

		raw_spin_unlock_irqrestore(&arb->lock_m, flags_m);

		return ret;
	}

	swinf_err = GET_SWINF_ERR(data);

	data = pmif_readl(arb->pmif_base[spmidev[sid].path], arb, inf_reg->rdata);
	memcpy(buf, &data, (bc & 3) + 1);
	pmif_writel(arb->pmif_base[spmidev[sid].path], arb, 1, inf_reg->ch_rdy);

	if (swinf_err) {
		raw_spin_unlock_irqrestore(&arb->lock_m, flags_m);
		return -EIO;
	}

	raw_spin_unlock_irqrestore(&arb->lock_m, flags_m);

	return 0;
}

static int pmif_spmi_write_cmd(struct spmi_controller *ctrl, u8 opc, u8 sid,
			       u16 addr, const u8 *buf, size_t len)
{
	struct pmif *arb = spmi_controller_get_drvdata(ctrl);
	struct ch_reg *inf_reg = NULL;
	int ret;
	u32 data = 0;
	u8 bc = len - 1;
	unsigned long flags_m = 0;
	int swinf_err = 0;

	/* Check for argument validation. */
	if (sid & ~(0xf))
		return -EINVAL;

	if (!arb)
		return -EINVAL;

	inf_reg = &arb->chan;

	/* Check the opcode */
	if (opc >= 0x40 && opc <= 0x5F)
		opc = PMIF_CMD_REG;
	else if (opc <= 0x0F)
		opc = PMIF_CMD_EXT_REG_LONG;
	else if (opc >= 0x30 && opc <= 0x37)
		opc = PMIF_CMD_EXT_REG_LONG;
	else if (opc >= 0x80)
		opc = PMIF_CMD_REG_0;
	else
		return -EINVAL;

	raw_spin_lock_irqsave(&arb->lock_m, flags_m);
	/* Wait for Software Interface FSM state to be IDLE. */
	ret = readl_poll_timeout_atomic(arb->pmif_base[spmidev[sid].path] + arb->regs[inf_reg->ch_sta],
					data, GET_SWINF(data) == SWINF_IDLE,
					PMIF_DELAY_US, PMIF_TIMEOUT);
	if (ret < 0) {
		dev_err(&ctrl->dev, "check IDLE timeout, read 0x%x, sta=0x%x, SPMI_DBG=0x%x\n",
			addr, pmif_readl(arb->pmif_base[spmidev[sid].path], arb, inf_reg->ch_sta),
			readl(arb->spmimst_base[spmidev[sid].path] + arb->spmimst_regs[SPMI_MST_DBG]));
		/* set channel ready if the data has transferred */
		if (pmif_is_fsm_vldclr(arb->pmif_base[spmidev[sid].path], arb))
			pmif_writel(arb->pmif_base[spmidev[sid].path], arb, 1, inf_reg->ch_rdy);

		raw_spin_unlock_irqrestore(&arb->lock_m, flags_m);

		return ret;
	}

	/* Set the write data. */
	memcpy(&data, buf, (bc & 3) + 1);
	pmif_writel(arb->pmif_base[spmidev[sid].path], arb, data, inf_reg->wdata);

	/* Send the command. */
	pmif_writel(arb->pmif_base[spmidev[sid].path], arb,
		    (opc << 30) | BIT(29) | (sid << 24) | (bc << 16) | addr,
		    inf_reg->ch_send);

	/* Wait for Software Interface FSM state to be IDLE. */
	ret = readl_poll_timeout_atomic(arb->pmif_base[spmidev[sid].path] + arb->regs[inf_reg->ch_sta],
					data, GET_SWINF(data) == SWINF_IDLE,
					PMIF_DELAY_US, PMIF_TIMEOUT);
	if (ret < 0) {
		dev_err(&ctrl->dev, "check IDLE timeout, read 0x%x, sta=0x%x, SPMI_DBG=0x%x\n",
			addr, pmif_readl(arb->pmif_base[spmidev[sid].path], arb, inf_reg->ch_sta),
			readl(arb->spmimst_base[spmidev[sid].path] + arb->spmimst_regs[SPMI_MST_DBG]));
		/* set channel ready if the data has transferred */
		if (pmif_is_fsm_vldclr(arb->pmif_base[spmidev[sid].path], arb))
			pmif_writel(arb->pmif_base[spmidev[sid].path], arb, 1, inf_reg->ch_rdy);

		raw_spin_unlock_irqrestore(&arb->lock_m, flags_m);

		return ret;
	}

	swinf_err = GET_SWINF_ERR(data);
	if (swinf_err) {
		raw_spin_unlock_irqrestore(&arb->lock_m, flags_m);
		return -EIO;
	}

	raw_spin_unlock_irqrestore(&arb->lock_m, flags_m);

	return 0;
}

static struct platform_driver mtk_spmi_driver;

static struct pmif mt6853_pmif_arb[] = {
	{
		.regs = mt6853_regs,
		.spmimst_regs = mt6853_spmi_regs,
		.soc_chan = 2,
		.caps = 1,
	},
};

static struct pmif mt6877_pmif_arb[] = {
	{
		.regs = mt6xxx_regs,
		.spmimst_regs = mt6853_spmi_regs,
		.soc_chan = 2,
		.caps = 1,
	},
};

static struct pmif mt6873_pmif_arb[] = {
	{
		.regs = mt6873_regs,
		.spmimst_regs = mt6873_spmi_regs,
		.soc_chan = 2,
		.caps = 1,
	},
};

static struct pmif mt6893_pmif_arb[] = {
	{
		.regs = mt6873_regs,
		.spmimst_regs = mt6873_spmi_regs,
		.soc_chan = 2,
		.caps = 1,
	},
};

static struct pmif mt6xxx_pmif_arb[] = {
	{
		.regs = mt6xxx_regs,
		.spmimst_regs = mt6853_spmi_regs,
		.soc_chan = 2,
		.mstid = SPMI_MASTER_1,
		.pmifid = PMIF_PMIFID_SPMI0,
		.read_cmd = pmif_spmi_read_cmd,
		.write_cmd = pmif_spmi_write_cmd,
		.caps = 2,
	},
};

static struct pmif mt6989_pmif_arb[] = {
	{
		.regs = mt6xxx_regs,
		.spmimst_regs = mt6853_spmi_regs,
		.soc_chan = 2,
		.mstid = SPMI_MASTER_1,
		.pmifid = PMIF_PMIFID_SPMI0,
		.read_cmd = pmif_spmi_read_cmd,
		.write_cmd = pmif_spmi_write_cmd,
		.caps = 3,
	},
};

int (*register_spmi_md_force_assert)(unsigned int id, char *buf, unsigned int len) = NULL;
EXPORT_SYMBOL(register_spmi_md_force_assert);

static void pmif_pmif_acc_vio_irq_handler(int irq, void *data)
{
	struct pmif *arb = data;
	u32 vio_chan = 0xFFFFFFFF;
	spmi_dump_pmif_record_reg(0, 0, 0);
	vio_chan = spmi_dump_pmif_acc_vio_reg();
	if (vio_chan == PMIF_CH_MD_DVFS_HW) {
		/* Trigger MDEE */
		pr_notice("[PMIF] MD DVFS HW MPU violation!\n");
		if (register_spmi_md_force_assert != NULL) {
			pr_notice("[PMIF]:Trigger MD assert DONE\n");
			register_spmi_md_force_assert(ID_SPMI_FORCE_MD_ASSERT, NULL, 0);
		}
	} else {
		pr_notice("[PMIF] Other channel violation!\n");
	}

	store_nack_info(arb, 0, 0);

#if (IS_ENABLED(CONFIG_MTK_AEE_FEATURE))
	aee_kernel_warning("PMIF", "PMIF:pmif_acc_vio");
#endif
	pr_notice("[PMIF]:pmif_acc_vio\n");
}

static void pmif_swinf_err_irq_handler(int irq_0, int irq_1, int irq_2, void *data, int idx)
{
	struct pmif *arb = data;
	int swintf_num = 0;

	if (irq_0)
		swintf_num = idx - ((arb->swintf_err_idx[0])%32);
	else if (irq_1)
		swintf_num = idx - ((arb->swintf_err_idx_m2[0])%32);
	else
		swintf_num = idx - ((arb->swintf_err_idx_p[0])%32);

	spmi_dump_pmif_record_reg(irq_0, irq_1, irq_2);
	pr_notice("[PMIF] %s swintf#%d err irq_m/m2/p 0x%x/0x%x/0x%x\n", __func__,
		swintf_num, irq_0, irq_1, irq_2);
}

static void pmif_hwinf_err_irq_handler(int irq_0, int irq_1, int irq_2, void *data, int idx)
{
	struct pmif *arb = data;
	int hwintf_num = 0;

	if (irq_0)
		hwintf_num = idx - ((arb->hwintf_err_idx[0])%32);
	else if (irq_1)
		hwintf_num = idx - ((arb->hwintf_err_idx_m2[0])%32);
	else
		hwintf_num = idx - ((arb->hwintf_err_idx_p[0])%32);

	spmi_dump_pmif_record_reg(irq_0, irq_1, irq_2);
	pr_notice("[PMIF] %s hwintf#%d err irq_m/m2/p 0x%x/0x%x/0x%x\n", __func__,
		hwintf_num, irq_0, irq_1, irq_2);
}

static void pmif_hw_monitor_irq_handler(int irq, void *data)
{
	spmi_dump_pmif_record_reg(0, 0, 0);
	if (IS_ENABLED(CONFIG_MTK_AEE_FEATURE))
		aee_kernel_warning("PMIF", "PMIF:pmif_hw_monitor_match");

	pr_notice("[PMIF]:pmif_hw_monitor_match\n");
}

static void pmif_wdt_irq_handler(int irq, void *data)
{
	struct pmif *arb = data;

	spmi_dump_pmif_busy_reg();
	spmi_dump_pmif_record_reg(0, 0, 0);
	spmi_dump_wdt_reg();
	pmif_writel(arb->pmif_base[0], arb, 0x40000000, PMIF_IRQ_CLR_0);
	if (!IS_ERR_OR_NULL(arb->pmif_base[1]))
		pmif_writel(arb->pmif_base[1], arb, 0x40000000, PMIF_IRQ_CLR_0);
	pr_notice("[PMIF]:WDT IRQ HANDLER DONE\n");
}

static void pmif_swinf_acc_err_0_irq_handler(int irq, void *data)
{
	struct pmif *arb = data;

	pmif_writel(arb->pmif_base[0], arb, 0x0, MD_AUXADC_RDATA_0_ADDR);
	pmif_writel(arb->pmif_base[0], arb, (u32)get_current_time_ms(), MD_AUXADC_RDATA_1_ADDR);
	if (!IS_ERR_OR_NULL(arb->pmif_base[1])) {
		pmif_writel(arb->pmif_base[1], arb, 0x0, MD_AUXADC_RDATA_0_ADDR);
		pmif_writel(arb->pmif_base[1], arb, (u32)get_current_time_ms(), MD_AUXADC_RDATA_1_ADDR);
	}
	pr_notice("[PMIF]:SWINF_ACC_ERR_0\n");
}

static void pmif_swinf_acc_err_1_irq_handler(int irq, void *data)
{
	struct pmif *arb = data;

	pmif_writel(arb->pmif_base[0], arb, 0x1, MD_AUXADC_RDATA_0_ADDR);
	pmif_writel(arb->pmif_base[0], arb, (u32)get_current_time_ms(), MD_AUXADC_RDATA_1_ADDR);
	if (!IS_ERR_OR_NULL(arb->pmif_base[1])) {
		pmif_writel(arb->pmif_base[1], arb, 0x1, MD_AUXADC_RDATA_0_ADDR);
		pmif_writel(arb->pmif_base[1], arb, (u32)get_current_time_ms(), MD_AUXADC_RDATA_1_ADDR);
	}
	pr_notice("[PMIF]:SWINF_ACC_ERR_1\n");
}

static void pmif_swinf_acc_err_2_irq_handler(int irq, void *data)
{
	struct pmif *arb = data;

	pmif_writel(arb->pmif_base[0], arb, 0x2, MD_AUXADC_RDATA_0_ADDR);
	pmif_writel(arb->pmif_base[0], arb, (u32)get_current_time_ms(), MD_AUXADC_RDATA_1_ADDR);
	if (!IS_ERR_OR_NULL(arb->pmif_base[1])) {
		pmif_writel(arb->pmif_base[1], arb, 0x2, MD_AUXADC_RDATA_0_ADDR);
		pmif_writel(arb->pmif_base[1], arb, (u32)get_current_time_ms(), MD_AUXADC_RDATA_1_ADDR);
	}
	pr_notice("[PMIF]:SWINF_ACC_ERR_2\n");
}

static void pmif_swinf_acc_err_3_irq_handler(int irq, void *data)
{
	struct pmif *arb = data;

	pmif_writel(arb->pmif_base[0], arb, 0x3, MD_AUXADC_RDATA_0_ADDR);
	pmif_writel(arb->pmif_base[0], arb, (u32)get_current_time_ms(), MD_AUXADC_RDATA_1_ADDR);
	if (!IS_ERR_OR_NULL(arb->pmif_base[1])) {
		pmif_writel(arb->pmif_base[1], arb, 0x3, MD_AUXADC_RDATA_0_ADDR);
		pmif_writel(arb->pmif_base[1], arb, (u32)get_current_time_ms(), MD_AUXADC_RDATA_1_ADDR);
	}
	pr_notice("[PMIF]:SWINF_ACC_ERR_3\n");
}

static void pmif_swinf_acc_err_4_irq_handler(int irq, void *data)
{
	struct pmif *arb = data;

	pmif_writel(arb->pmif_base[0], arb, 0x4, MD_AUXADC_RDATA_0_ADDR);
	pmif_writel(arb->pmif_base[0], arb, (u32)get_current_time_ms(), MD_AUXADC_RDATA_1_ADDR);
	if (!IS_ERR_OR_NULL(arb->pmif_base[1])) {
		pmif_writel(arb->pmif_base[1], arb, 0x4, MD_AUXADC_RDATA_0_ADDR);
		pmif_writel(arb->pmif_base[1], arb, (u32)get_current_time_ms(), MD_AUXADC_RDATA_1_ADDR);
	}
	pr_notice("[PMIF]:SWINF_ACC_ERR_4\n");
}

static void pmif_swinf_acc_err_5_irq_handler(int irq, void *data)
{
	struct pmif *arb = data;

	pmif_writel(arb->pmif_base[0], arb, 0x5, MD_AUXADC_RDATA_0_ADDR);
	pmif_writel(arb->pmif_base[0], arb, (u32)get_current_time_ms(), MD_AUXADC_RDATA_1_ADDR);
	if (!IS_ERR_OR_NULL(arb->pmif_base[1])) {
		pmif_writel(arb->pmif_base[1], arb, 0x5, MD_AUXADC_RDATA_0_ADDR);
		pmif_writel(arb->pmif_base[1], arb, (u32)get_current_time_ms(), MD_AUXADC_RDATA_1_ADDR);
	}
	pr_notice("[PMIF]:SWINF_ACC_ERR_5\n");
}

static void pmif_hwinf_cmd_vio_irq_handler(int irq_0, int irq_1, int irq_2, void *data, int idx)
{
	unsigned int buf = 0;
	//if fail path is pmif-m/p set 0/1 to notify md
	if (irq_1 || irq_2)
		buf = 1;

	pr_notice("[PMIF] MD HW CMD violation!\n");
	if (register_spmi_md_force_assert != NULL) {
		pr_notice("[PMIF]:Trigger MD assert DONE\n");
		register_spmi_md_force_assert(ID_PMIF_FORCE_MD_ASSERT, (char *)&buf, sizeof(unsigned int));
	}
	spmi_dump_pmif_record_reg(irq_0, irq_1, irq_2);
	pr_notice("[PMIF]:HWINF_CMD_VIO_0 irq_0/1/2/buf 0x%x/0x%x/0x%x/0x%x done\n", irq_0, irq_1, irq_2, buf);
}

static irqreturn_t pmif_event_0_irq_handler(int irq, void *data)
{
	struct pmif *arb = data;
	int irq_0 = 0, irq_1 = 0, irq_2 = 0, idx = 0;

	__pm_stay_awake(arb->pmif_m_Thread_lock);
	mutex_lock(&arb->pmif_m_mutex);

	irq_0 = pmif_readl(arb->pmif_base[0], arb, PMIF_IRQ_FLAG_0);
	if (!IS_ERR_OR_NULL(arb->pmif_base[1]))
		irq_1 = pmif_readl(arb->pmif_base[1], arb, PMIF_IRQ_FLAG_0);
	if (!IS_ERR_OR_NULL(arb->pmif_base[2]))
		irq_2 = pmif_readl(arb->pmif_base[2], arb, PMIF_IRQ_FLAG_0);

	if ((irq_0 == 0) && (irq_1 == 0) && (irq_2 == 0)) {
		mutex_unlock(&arb->pmif_m_mutex);
		__pm_relax(arb->pmif_m_Thread_lock);
		return IRQ_NONE;
	}

	for (idx = 0; idx < 32; idx++) {
		if (((irq_0 & (0x1 << idx)) != 0) || ((irq_1 & (0x1 << idx)) != 0) || ((irq_2 & (0x1 << idx)) != 0)) {
			switch (idx) {
			case IRQ_WDT_V4:
				pmif_wdt_irq_handler(irq, data);
			break;
			case IRQ_HW_MONITOR_V4:
				pmif_hw_monitor_irq_handler(irq, data);
			break;
			case IRQ_ALL_PMIC_MPU_VIO_V4:
				pmif_pmif_acc_vio_irq_handler(irq, data);
			break;
			default:
				pr_notice("%s IRQ[%d] triggered\n",
					__func__, idx);
				spmi_dump_pmif_record_reg(0, 0, 0);
			break;
			}
			if (irq_0) {
				pmif_writel(arb->pmif_base[0], arb, irq_0, PMIF_IRQ_CLR_0);
				pr_notice("%s IRQ[%d] pmif-0 cleared\n", __func__, idx);
			} else if (irq_1) {
				pmif_writel(arb->pmif_base[1], arb, irq_1, PMIF_IRQ_CLR_0);
				pr_notice("%s IRQ[%d] pmif-1 cleared\n", __func__, idx);
			} else if (irq_2) {
				pmif_writel(arb->pmif_base[2], arb, irq_2, PMIF_IRQ_CLR_0);
				pr_notice("%s IRQ[%d] pmif-2 cleared\n", __func__, idx);
			} else
				pr_notice("%s IRQ[%d] is not cleared due to empty flags\n",
					__func__, idx);
			break;
		}
	}
	mutex_unlock(&arb->pmif_m_mutex);
	__pm_relax(arb->pmif_m_Thread_lock);

	return IRQ_HANDLED;
}

static irqreturn_t pmif_event_1_irq_handler(int irq, void *data)
{
	struct pmif *arb = data;
	int irq_0 = 0, irq_1 = 0, irq_2 = 0, idx = 0;

	__pm_stay_awake(arb->pmif_m_Thread_lock);
	mutex_lock(&arb->pmif_m_mutex);

	irq_0 = pmif_readl(arb->pmif_base[0], arb, PMIF_IRQ_FLAG_1);
	if (!IS_ERR_OR_NULL(arb->pmif_base[1]))
		irq_1 = pmif_readl(arb->pmif_base[1], arb, PMIF_IRQ_FLAG_1);

	if (!IS_ERR_OR_NULL(arb->pmif_base[2]))
		irq_2 = pmif_readl(arb->pmif_base[2], arb, PMIF_IRQ_FLAG_1);

	if ((irq_0 == 0) && (irq_1 == 0) && (irq_2 == 0)) {
		mutex_unlock(&arb->pmif_m_mutex);
		__pm_relax(arb->pmif_m_Thread_lock);
		return IRQ_NONE;
	}
	for (idx = 0; idx < 32; idx++) {
		if (((irq_0 & (0x1 << idx)) != 0) || ((irq_1 & (0x1 << idx)) != 0)
			|| ((irq_2 & (0x1 << idx)) != 0)) {
			if (((idx >= (arb->hwintf_err_idx[0] % 32)) &&
				(idx <= (arb->hwintf_err_idx[1] % 32))) ||
				((idx >= (arb->hwintf_err_idx_m2[0] % 32)) &&
				(idx <= (arb->hwintf_err_idx_m2[1] % 32))) ||
				((idx >= (arb->hwintf_err_idx_p[0] % 32)) &&
				(idx <= (arb->hwintf_err_idx_p[1] % 32)))) {
				pmif_hwinf_err_irq_handler(irq_0, irq_1, irq_2, data, idx);
			} else {
				switch (idx) {
				case IRQ_PMIF_HWINF_0_CMD_VIO_1:
					pmif_hwinf_cmd_vio_irq_handler(irq_0, irq_1, irq_2, data, idx);
				break;
				default:
					pr_notice("%s IRQ[%d] triggered\n",
						__func__, idx);
					spmi_dump_pmif_record_reg(0, 0, 0);
				break;
				}
			}
			if (irq_0) {
				pmif_writel(arb->pmif_base[0], arb, irq_0, PMIF_IRQ_CLR_1);
				pr_notice("%s IRQ[%d] pmif-0 cleared\n", __func__, idx);
			} else if (irq_1) {
				pmif_writel(arb->pmif_base[1], arb, irq_1, PMIF_IRQ_CLR_1);
				pr_notice("%s IRQ[%d] pmif-1 cleared\n", __func__, idx);
			} else if (irq_2) {
				pmif_writel(arb->pmif_base[2], arb, irq_2, PMIF_IRQ_CLR_1);
				pr_notice("%s IRQ[%d] pmif-2 cleared\n", __func__, idx);
			} else
				pr_notice("%s IRQ[%d] is not cleared due to empty flags\n",
					__func__, idx);
			break;
		}
	}
	mutex_unlock(&arb->pmif_m_mutex);
	__pm_relax(arb->pmif_m_Thread_lock);

	return IRQ_HANDLED;
}

static irqreturn_t pmif_event_2_irq_handler(int irq, void *data)
{
	struct pmif *arb = data;
	int irq_0 = 0, irq_1 = 0, irq_2 = 0, idx = 0;

	__pm_stay_awake(arb->pmif_m_Thread_lock);
	mutex_lock(&arb->pmif_m_mutex);

	irq_0 = pmif_readl(arb->pmif_base[0], arb, PMIF_IRQ_FLAG_2);
	if (!IS_ERR_OR_NULL(arb->pmif_base[1]))
		irq_1 = pmif_readl(arb->pmif_base[1], arb, PMIF_IRQ_FLAG_2);

	if (!IS_ERR_OR_NULL(arb->pmif_base[2]))
		irq_2 = pmif_readl(arb->pmif_base[2], arb, PMIF_IRQ_FLAG_2);

	if ((irq_0 == 0) && (irq_1 == 0) && (irq_2 == 0)) {
		mutex_unlock(&arb->pmif_m_mutex);
		__pm_relax(arb->pmif_m_Thread_lock);
		return IRQ_NONE;
	}

	for (idx = 0; idx < 32; idx++) {
		if (((irq_0 & (0x1 << idx)) != 0) || ((irq_1 & (0x1 << idx)) != 0)
			|| ((irq_2 & (0x1 << idx)) != 0)) {
			switch (idx) {
			case IRQ_PMIF_HWINF_0_CMD_VIO_0:
				pmif_hwinf_cmd_vio_irq_handler(irq_0, irq_1, irq_2, data, idx);
			break;
			default:
				pr_notice("%s IRQ[%d] triggered\n",
					__func__, idx);
				spmi_dump_pmif_record_reg(0, 0, 0);
			break;
			}
			if (irq_0) {
				pmif_writel(arb->pmif_base[0], arb, irq_0, PMIF_IRQ_CLR_2);
				pr_notice("%s IRQ[%d] pmif-0 cleared\n", __func__, idx);
			} else if (irq_1) {
				pmif_writel(arb->pmif_base[1], arb, irq_1, PMIF_IRQ_CLR_2);
				pr_notice("%s IRQ[%d] pmif-1 cleared\n", __func__, idx);
			} else if (irq_2) {
				pmif_writel(arb->pmif_base[2], arb, irq_2, PMIF_IRQ_CLR_2);
				pr_notice("%s IRQ[%d] pmif-2 cleared\n", __func__, idx);
			} else
				pr_notice("%s IRQ[%d] is not cleared due to empty flags\n",
					__func__, idx);
			break;
		}
	}
	mutex_unlock(&arb->pmif_m_mutex);
	__pm_relax(arb->pmif_m_Thread_lock);

	return IRQ_HANDLED;
}

static irqreturn_t pmif_event_3_irq_handler(int irq, void *data)
{
	struct pmif *arb = data;
	int irq_0 = 0, irq_1 = 0, irq_2 = 0, idx = 0;

	__pm_stay_awake(arb->pmif_m_Thread_lock);
	mutex_lock(&arb->pmif_m_mutex);

	irq_0 = pmif_readl(arb->pmif_base[0], arb, PMIF_IRQ_FLAG_3);
	if (!IS_ERR_OR_NULL(arb->pmif_base[1]))
		irq_1 = pmif_readl(arb->pmif_base[1], arb, PMIF_IRQ_FLAG_3);
	if (!IS_ERR_OR_NULL(arb->pmif_base[2]))
		irq_2 = pmif_readl(arb->pmif_base[2], arb, PMIF_IRQ_FLAG_3);

	if ((irq_0 == 0) && (irq_1 == 0) && (irq_2 == 0)) {
		mutex_unlock(&arb->pmif_m_mutex);
		__pm_relax(arb->pmif_m_Thread_lock);
		return IRQ_NONE;
	}

	for (idx = 0; idx < 32; idx++) {
		if (((irq_0 & (0x1 << idx)) != 0) || ((irq_1 & (0x1 << idx)) != 0) ||
			((irq_2 & (0x1 << idx)) != 0)) {
			if (((idx >= (arb->swintf_err_idx[0] % 32)) &&
				(idx <= (arb->swintf_err_idx[1] % 32))) ||
				((idx >= (arb->swintf_err_idx_m2[0] % 32)) &&
				(idx <= (arb->swintf_err_idx_m2[1] % 32))) ||
				((idx >= (arb->swintf_err_idx_p[0] % 32)) &&
				(idx <= (arb->swintf_err_idx_p[1] % 32)))) {
				pmif_swinf_err_irq_handler(irq_0, irq_1, irq_2, data, idx);
			} else {
				switch (idx) {
				case IRQ_PMIF_SWINF_ACC_ERR_0:
				case IRQ_PMIF_SWINF_ACC_ERR_0_V2:
					pmif_swinf_acc_err_0_irq_handler(irq, data);
				break;
				case IRQ_PMIF_SWINF_ACC_ERR_1:
				case IRQ_PMIF_SWINF_ACC_ERR_1_V2:
					pmif_swinf_acc_err_1_irq_handler(irq, data);
				break;
				case IRQ_PMIF_SWINF_ACC_ERR_2:
				case IRQ_PMIF_SWINF_ACC_ERR_2_V2:
					pmif_swinf_acc_err_2_irq_handler(irq, data);
				break;
				/* Use caps to distinguish platform if they have same irq number */
				case IRQ_PMIF_SWINF_ACC_ERR_3_V2:
					pmif_swinf_acc_err_3_irq_handler(irq, data);
				break;
				case IRQ_PMIF_SWINF_ACC_ERR_4:
				case IRQ_PMIF_SWINF_ACC_ERR_4_V2:
					pmif_swinf_acc_err_4_irq_handler(irq, data);
				break;
				case IRQ_PMIF_SWINF_ACC_ERR_5:
				case IRQ_PMIF_SWINF_ACC_ERR_5_V2:
					pmif_swinf_acc_err_5_irq_handler(irq, data);
				break;
				default:
					pr_notice("%s IRQ[%d] triggered\n",
						__func__, idx);
					spmi_dump_pmif_record_reg(0, 0, 0);
				break;
				}
			}
			/* Don't clear MD SW SWINF ACC ERR flag for re-send mechanism */
			if (irq_0) {
				if ((!(irq_0 & (0x1 << IRQ_PMIF_SWINF_ACC_ERR_0))) &&
					(!(irq_0 & (0x1 << IRQ_PMIF_SWINF_ACC_ERR_0_V2))))
					pmif_writel(arb->pmif_base[0], arb, irq_0, PMIF_IRQ_CLR_3);
				pr_notice("%s IRQ[%d] pmif-0 cleared\n", __func__, idx);
			} else if (irq_1) {
				if ((!(irq_1 & (0x1 << IRQ_PMIF_SWINF_ACC_ERR_0))) &&
					(!(irq_1 & (0x1 << IRQ_PMIF_SWINF_ACC_ERR_0_V2))))
					pmif_writel(arb->pmif_base[1], arb, irq_1, PMIF_IRQ_CLR_3);
				pr_notice("%s IRQ[%d] pmif-1 cleared\n", __func__, idx);
			} else if (irq_2) {
				pmif_writel(arb->pmif_base[2], arb, irq_2, PMIF_IRQ_CLR_3);
				pr_notice("%s IRQ[%d] pmif-2 cleared\n", __func__, idx);
			} else
				pr_notice("%s IRQ[%d] is not cleared due to empty flags\n",
					__func__, idx);
			break;
		}
	}
	mutex_unlock(&arb->pmif_m_mutex);
	__pm_relax(arb->pmif_m_Thread_lock);

	return IRQ_HANDLED;
}

static irqreturn_t pmif_event_4_irq_handler(int irq, void *data)
{
	struct pmif *arb = data;
	int irq_0 = 0, irq_1 = 0, irq_2 = 0, idx = 0;

	__pm_stay_awake(arb->pmif_m_Thread_lock);
	mutex_lock(&arb->pmif_m_mutex);

	irq_0 = pmif_readl(arb->pmif_base[0], arb, PMIF_IRQ_FLAG_4);
	if (!IS_ERR_OR_NULL(arb->pmif_base[1]))
		irq_1 = pmif_readl(arb->pmif_base[1], arb, PMIF_IRQ_FLAG_4);
	if (!IS_ERR_OR_NULL(arb->pmif_base[2]))
		irq_2 = pmif_readl(arb->pmif_base[2], arb, PMIF_IRQ_FLAG_4);

	if ((irq_0 == 0) && (irq_1 == 0) && (irq_2 == 0)) {
		mutex_unlock(&arb->pmif_m_mutex);
		__pm_relax(arb->pmif_m_Thread_lock);
		return IRQ_NONE;
	}

	for (idx = 0; idx < 32; idx++) {
		if (((irq_0 & (0x1 << idx)) != 0) || ((irq_1 & (0x1 << idx)) != 0) ||
			((irq_2 & (0x1 << idx)) != 0)) {
			switch (idx) {
			default:
				pr_notice("%s IRQ[%d] triggered\n",
					__func__, idx);
				spmi_dump_pmif_record_reg(0, 0, 0);
			break;
			}
			if (irq_0)
				pmif_writel(arb->pmif_base[0], arb, irq_0, PMIF_IRQ_CLR_4);
			else if (irq_1)
				pmif_writel(arb->pmif_base[1], arb, irq_1, PMIF_IRQ_CLR_4);
			else if (irq_2)
				pmif_writel(arb->pmif_base[2], arb, irq_2, PMIF_IRQ_CLR_4);
			else
				pr_notice("%s IRQ[%d] is not cleared due to empty flags\n",
					__func__, idx);
			break;
		}
	}
	mutex_unlock(&arb->pmif_m_mutex);
	__pm_relax(arb->pmif_m_Thread_lock);

	return IRQ_HANDLED;
}

static struct pmif_irq_desc pmif_event_irq[] = {
	PMIF_IRQDESC(event_0),
	PMIF_IRQDESC(event_1),
	PMIF_IRQDESC(event_2),
	PMIF_IRQDESC(event_3),
	PMIF_IRQDESC(event_4),
};

static void pmif_irq_register(struct platform_device *pdev,
		struct pmif *arb, int irq)
{
	int i = 0, ret = 0, path_cnt = 0;
	u32 irq_event_en[5] = {0};
	u32 irq_event_en_m2[5] = {0};
	u32 irq_event_en_p[5] = {0};

	for (i = 0; i < ARRAY_SIZE(pmif_event_irq); i++) {
		if (!pmif_event_irq[i].name)
			continue;
		ret = devm_request_threaded_irq(&pdev->dev, irq, NULL,
				pmif_event_irq[i].irq_handler,
				IRQF_TRIGGER_HIGH | IRQF_ONESHOT | IRQF_SHARED,
				pmif_event_irq[i].name, arb);
		if (ret < 0) {
			dev_notice(&pdev->dev, "request %s irq fail\n",
				pmif_event_irq[i].name);
			continue;
		}
		pmif_event_irq[i].irq = irq;
	}

	ret = of_property_read_u32_array(pdev->dev.of_node, "irq-event-en",
		irq_event_en, ARRAY_SIZE(irq_event_en));
	if (!ret) {
		pmif_writel(arb->pmif_base[path_cnt], arb, irq_event_en[0] |
			pmif_readl(arb->pmif_base[path_cnt],
			arb, PMIF_IRQ_EVENT_EN_0), PMIF_IRQ_EVENT_EN_0);
		pmif_writel(arb->pmif_base[path_cnt], arb, irq_event_en[1] |
			pmif_readl(arb->pmif_base[path_cnt],
			arb, PMIF_IRQ_EVENT_EN_1), PMIF_IRQ_EVENT_EN_1);
		pmif_writel(arb->pmif_base[path_cnt], arb, irq_event_en[2] |
			pmif_readl(arb->pmif_base[path_cnt],
			arb, PMIF_IRQ_EVENT_EN_2), PMIF_IRQ_EVENT_EN_2);
		pmif_writel(arb->pmif_base[path_cnt], arb, irq_event_en[3] |
			pmif_readl(arb->pmif_base[path_cnt],
			arb, PMIF_IRQ_EVENT_EN_3), PMIF_IRQ_EVENT_EN_3);
		pmif_writel(arb->pmif_base[path_cnt], arb, irq_event_en[4] |
			pmif_readl(arb->pmif_base[path_cnt],
			arb, PMIF_IRQ_EVENT_EN_4), PMIF_IRQ_EVENT_EN_4);
		path_cnt++;
	} else {
		pr_notice("%s no irq-event-en found\n",	__func__);
	}


	ret = of_property_read_u32_array(pdev->dev.of_node, "irq-event-en-m2",
		irq_event_en_m2, ARRAY_SIZE(irq_event_en_m2));
	if (!ret) {
		if (!IS_ERR_OR_NULL(arb->pmif_base[path_cnt])) {
			pmif_writel(arb->pmif_base[path_cnt], arb, irq_event_en_m2[0] |
				pmif_readl(arb->pmif_base[path_cnt],
		arb, PMIF_IRQ_EVENT_EN_0), PMIF_IRQ_EVENT_EN_0);
			pmif_writel(arb->pmif_base[path_cnt], arb, irq_event_en_m2[1] |
				pmif_readl(arb->pmif_base[path_cnt],
		arb, PMIF_IRQ_EVENT_EN_1), PMIF_IRQ_EVENT_EN_1);
			pmif_writel(arb->pmif_base[path_cnt], arb, irq_event_en_m2[2] |
				pmif_readl(arb->pmif_base[path_cnt],
		arb, PMIF_IRQ_EVENT_EN_2), PMIF_IRQ_EVENT_EN_2);
			pmif_writel(arb->pmif_base[path_cnt], arb, irq_event_en_m2[3] |
				pmif_readl(arb->pmif_base[path_cnt],
		arb, PMIF_IRQ_EVENT_EN_3), PMIF_IRQ_EVENT_EN_3);
			pmif_writel(arb->pmif_base[path_cnt], arb, irq_event_en_m2[4] |
				pmif_readl(arb->pmif_base[path_cnt],
		arb, PMIF_IRQ_EVENT_EN_4), PMIF_IRQ_EVENT_EN_4);
			path_cnt++;
		}
	} else {
		pr_notice("%s no irq-event-en-m2 found\n", __func__);
	}

	ret = of_property_read_u32_array(pdev->dev.of_node, "irq-event-en-p",
		irq_event_en_p, ARRAY_SIZE(irq_event_en_p));
	if (!ret) {
		if (!IS_ERR_OR_NULL(arb->pmif_base[path_cnt])) {
			pmif_writel(arb->pmif_base[path_cnt], arb, irq_event_en_p[0] |
				pmif_readl(arb->pmif_base[path_cnt],
			arb, PMIF_IRQ_EVENT_EN_0), PMIF_IRQ_EVENT_EN_0);
			pmif_writel(arb->pmif_base[path_cnt], arb, irq_event_en_p[1] |
				pmif_readl(arb->pmif_base[path_cnt],
			arb, PMIF_IRQ_EVENT_EN_1), PMIF_IRQ_EVENT_EN_1);
			pmif_writel(arb->pmif_base[path_cnt], arb, irq_event_en_p[2] |
				pmif_readl(arb->pmif_base[path_cnt],
			arb, PMIF_IRQ_EVENT_EN_2), PMIF_IRQ_EVENT_EN_2);
			pmif_writel(arb->pmif_base[path_cnt], arb, irq_event_en_p[3] |
				pmif_readl(arb->pmif_base[path_cnt],
			arb, PMIF_IRQ_EVENT_EN_3), PMIF_IRQ_EVENT_EN_3);
			pmif_writel(arb->pmif_base[path_cnt], arb, irq_event_en_p[4] |
				pmif_readl(arb->pmif_base[path_cnt],
			arb, PMIF_IRQ_EVENT_EN_4), PMIF_IRQ_EVENT_EN_4);
	}
	} else {
		pr_notice("%s no irq-event-en-p found\n", __func__);
	}
}

static void dump_spmi_pmic_dbg_rg(struct pmif *arb, unsigned int nack_0, unsigned int nack_1, unsigned int nack_2)
{
	u8 rdata = 0, rdata1 = 0, rdata2 =0, val = 0, org = 0;
	u8 dbg_data = 0, idx, addr, data = 0, cmd, addr1;
	u16 pmic_addr;
	int i = 0;
	unsigned short hwcidaddr = 0;
	unsigned int slvid = 0, sid = 0, sid_start = 0, sid_end = 0;

	if (nack_0) {
		hwcidaddr = 0x9;
		slvid = GET_SPMI_NACK_SLVID(nack_0);
		sid_start = sid_end = slvid;
	} else if (nack_1) {
		slvid = GET_SPMI_NACK_SLVID(nack_1);
		hwcidaddr = 0x209;
		sid_start = sid_end = slvid;
	} else if (nack_2) {
		hwcidaddr = 0x9;
		slvid = GET_SPMI_NACK_SLVID(nack_2);
		sid_start = sid_end = slvid;
	} else {
		slvid = 0x4;
		hwcidaddr = 0x9;
		sid_start = sid_end = slvid;
		pr_info("%s no m/p_nack detect\n",__func__);
	}

	/* Disable read command log */
	val = 0;
	arb->spmic->write_cmd(arb->spmic, SPMI_CMD_EXT_WRITEL, slvid,
		PMIC_RG_DEBUG_EN_RD_CMD_ADDR, &val, 1);

	/* pause debug log feature by setting RG_DEBUG_DIS_TRIG 0->1->0 */
	val = 0;
	arb->spmic->write_cmd(arb->spmic, SPMI_CMD_EXT_WRITEL, slvid,
		PMIC_RG_DEBUG_DIS_TRIG_ADDR, &val, 1);
	val = 0x1 << PMIC_RG_DEBUG_DIS_TRIG_SHIFT;
	arb->spmic->write_cmd(arb->spmic, SPMI_CMD_EXT_WRITEL, slvid,
		PMIC_RG_DEBUG_DIS_TRIG_ADDR, &val, 1);
	val = 0;
	arb->spmic->write_cmd(arb->spmic, SPMI_CMD_EXT_WRITEL, slvid,
		PMIC_RG_DEBUG_DIS_TRIG_ADDR, &val, 1);

		/* DBGMUX_SEL = 0 */
	arb->spmic->read_cmd(arb->spmic, SPMI_CMD_EXT_READL,
		slvid, PMIC_RG_SPMI_DBGMUX_SEL_ADDR, &org, 1);
	org &= ~(PMIC_RG_SPMI_DBGMUX_SEL_MASK << PMIC_RG_SPMI_DBGMUX_SEL_SHIFT);
	org |= (0 << PMIC_RG_SPMI_DBGMUX_SEL_SHIFT);
	arb->spmic->write_cmd(arb->spmic, SPMI_CMD_EXT_WRITEL, slvid,
		PMIC_RG_SPMI_DBGMUX_SEL_ADDR, &org, 1);
	/* read spmi_debug[15:0] data*/
	arb->spmic->read_cmd(arb->spmic, SPMI_CMD_EXT_READL, slvid,
		PMIC_RG_SPMI_DBGMUX_OUT_L_ADDR, &dbg_data, 1);

	pr_info ("%s dbg_data:0x%x\n",__func__,dbg_data);
	idx = dbg_data & 0xF;
	for (i = 0; i < 16; i++) {
		/* debug_addr start from index 1 */
		arb->spmic->read_cmd(arb->spmic, SPMI_CMD_EXT_READL,
			slvid, PMIC_RG_SPMI_DBGMUX_SEL_ADDR, &org, 1);
		org &= ~(PMIC_RG_SPMI_DBGMUX_SEL_MASK << PMIC_RG_SPMI_DBGMUX_SEL_SHIFT);
		org |= ((((idx + i) % 16) + 1) << PMIC_RG_SPMI_DBGMUX_SEL_SHIFT);
		arb->spmic->write_cmd(arb->spmic, SPMI_CMD_EXT_WRITEL, slvid,
			PMIC_RG_SPMI_DBGMUX_SEL_ADDR, &org, 1);
		/* read spmi_debug[15:0] data*/
		arb->spmic->read_cmd(arb->spmic, SPMI_CMD_EXT_READL, slvid,
			PMIC_RG_SPMI_DBGMUX_OUT_L_ADDR, &addr, 1);
		arb->spmic->read_cmd(arb->spmic, SPMI_CMD_EXT_READL, slvid,
			PMIC_RG_SPMI_DBGMUX_OUT_H_ADDR, &addr1, 1);

		pmic_addr = (addr1 << 8) | addr;
		arb->spmic->read_cmd(arb->spmic, SPMI_CMD_EXT_READL,
			slvid, PMIC_RG_SPMI_DBGMUX_SEL_ADDR, &org, 1);
		org &= ~(PMIC_RG_SPMI_DBGMUX_SEL_MASK << PMIC_RG_SPMI_DBGMUX_SEL_SHIFT);
		org |= ((((idx + i) % 16) + 17) << PMIC_RG_SPMI_DBGMUX_SEL_SHIFT);
		arb->spmic->write_cmd(arb->spmic, SPMI_CMD_EXT_WRITEL, slvid,
			PMIC_RG_SPMI_DBGMUX_SEL_ADDR, &org, 1);
		/* read spmi_debug[15:0] data*/
		arb->spmic->read_cmd(arb->spmic, SPMI_CMD_EXT_READL, slvid,
			PMIC_RG_SPMI_DBGMUX_OUT_L_ADDR, &data, 1);
		arb->spmic->read_cmd(arb->spmic, SPMI_CMD_EXT_READL, slvid,
			PMIC_RG_SPMI_DBGMUX_OUT_H_ADDR, &dbg_data, 1);
		cmd = dbg_data & 0x7;
		pr_info("slvid=0x%x record %s addr=0x%x, data=0x%x, cmd=%d%s\n",
					slvid, cmd <= 3 ? "write" : "read",
					pmic_addr, data, cmd,
					i == 15 ? "(the last)" : "");
	}

	for (i = 33; i < 38; i++) {
		val = i;
		for (sid = sid_start; sid <= sid_end; sid ++) {
			arb->spmic->write_cmd(arb->spmic, SPMI_CMD_EXT_WRITEL, sid,
				PMIC_RG_SPMI_DBGMUX_SEL_ADDR, &val, 1);
			arb->spmic->read_cmd(arb->spmic, SPMI_CMD_EXT_READL, sid,
				PMIC_RG_SPMI_DBGMUX_SEL_ADDR, &rdata, 1);
			arb->spmic->read_cmd(arb->spmic, SPMI_CMD_EXT_READL, sid,
				PMIC_RG_SPMI_DBGMUX_OUT_H_ADDR, &rdata1, 1);
			arb->spmic->read_cmd(arb->spmic, SPMI_CMD_EXT_READL, sid,
				PMIC_RG_SPMI_DBGMUX_OUT_L_ADDR, &rdata2, 1);
			pr_notice("%s sid 0x%x DBG_SEL %d DBG_OUT_H 0x%x DBG_OUT_L 0x%x\n",
				__func__,sid, rdata, rdata1, rdata2);
		}
	}
	val = 0;
	for (sid = sid_start; sid <= sid_end; sid ++) {
		arb->spmic->write_cmd(arb->spmic, SPMI_CMD_EXT_WRITEL, sid,
			PMIC_RG_SPMI_DBGMUX_SEL_ADDR, &val, 1);
	}
	/* Disable read command log */
	/* pause debug log feature by setting RG_DEBUG_DIS_TRIG 1->0 */
	val = 0;
	arb->spmic->write_cmd(arb->spmic, SPMI_CMD_EXT_WRITEL, slvid,
		PMIC_RG_DEBUG_DIS_TRIG_ADDR, &val, 1);
	/* enable debug log feature by setting RG_DEBUG_EN_TRIG 0->1->0 */
	val = 0x1 << PMIC_RG_DEBUG_EN_TRIG_SHIFT;
	arb->spmic->write_cmd(arb->spmic, SPMI_CMD_EXT_WRITEL, slvid,
		PMIC_RG_DEBUG_EN_TRIG_ADDR, &val, 1);
	val = 0;
	arb->spmic->write_cmd(arb->spmic, SPMI_CMD_EXT_WRITEL, slvid,
		PMIC_RG_DEBUG_EN_TRIG_ADDR, &val, 1);

	for (sid = sid_start; sid <= sid_end; sid ++) {
		arb->spmic->read_cmd(arb->spmic, SPMI_CMD_EXT_READL, sid,
			PMIC_RG_INT_RAW_STATUS_ADDR_MON_HIT_ADDR, &rdata, 1);
		arb->spmic->read_cmd(arb->spmic, SPMI_CMD_EXT_READL, sid,
			hwcidaddr, &rdata1, 1);
		pr_notice("%s sid 0x%x INT_RAW_STA 0x%x cid 0x%x\n",
			__func__, sid, rdata, rdata1);
	}
}

static irqreturn_t spmi_nack_irq_handler(int irq, void *data)
{
	struct pmif *arb = data;
	int flag = 0, assert_flag = 0;
	unsigned int spmi_nack_0 = 0, spmi_nack_1 = 0, spmi_nack_2 = 0,
				 spmi_nack_data_0 = 0, spmi_nack_data_1 = 0, spmi_nack_data_2 = 0,
				 spmi_rcs_nack_0 = 0, spmi_rcs_nack_1 = 0, spmi_rcs_nack_2 = 0,
				 spmi_debug_nack_0 = 0, spmi_debug_nack_1 = 0, spmi_debug_nack_2 = 0,
				 spmi_mst_nack_0 = 0, spmi_mst_nack_1 = 0, spmi_mst_nack_2 = 0,
				 spmi_wdt_rec_0 = 0, spmi_wdt_rec_1 = 0, spmi_wdt_rec_2 = 0;

	__pm_stay_awake(arb->pmif_m_Thread_lock);
	mutex_lock(&arb->pmif_m_mutex);

	spmi_nack_0 = mtk_spmi_readl(arb->spmimst_base[0], arb, SPMI_REC0);
	spmi_nack_data_0 = mtk_spmi_readl(arb->spmimst_base[0], arb, SPMI_REC1);
	spmi_rcs_nack_0 = mtk_spmi_readl(arb->spmimst_base[0], arb, SPMI_REC_CMD_DEC);
	spmi_debug_nack_0 = mtk_spmi_readl(arb->spmimst_base[0], arb, SPMI_DEC_DBG);
	spmi_mst_nack_0 = mtk_spmi_readl(arb->spmimst_base[0], arb, SPMI_MST_DBG);
	spmi_wdt_rec_0 = mtk_spmi_readl(arb->spmimst_base[0], arb, SPMI_WDT_REC);

	if (!IS_ERR_OR_NULL(arb->spmimst_base[1])) {
		spmi_nack_1 = mtk_spmi_readl(arb->spmimst_base[1], arb, SPMI_REC0);
		spmi_nack_data_1 = mtk_spmi_readl(arb->spmimst_base[1], arb, SPMI_REC1);
		spmi_rcs_nack_1 = mtk_spmi_readl(arb->spmimst_base[1], arb, SPMI_REC_CMD_DEC);
		spmi_debug_nack_1 = mtk_spmi_readl(arb->spmimst_base[1], arb, SPMI_DEC_DBG);
		spmi_mst_nack_1 = mtk_spmi_readl(arb->spmimst_base[1], arb, SPMI_MST_DBG);
		spmi_wdt_rec_1 = mtk_spmi_readl(arb->spmimst_base[1], arb, SPMI_WDT_REC);
	}
	if (!IS_ERR_OR_NULL(arb->spmimst_base[2])) {
		spmi_nack_2 = mtk_spmi_readl(arb->spmimst_base[2], arb, SPMI_REC0);
		spmi_nack_data_2 = mtk_spmi_readl(arb->spmimst_base[2], arb, SPMI_REC1);
		spmi_rcs_nack_2 = mtk_spmi_readl(arb->spmimst_base[2], arb, SPMI_REC_CMD_DEC);
		spmi_debug_nack_2 = mtk_spmi_readl(arb->spmimst_base[2], arb, SPMI_DEC_DBG);
		spmi_mst_nack_2 = mtk_spmi_readl(arb->spmimst_base[2], arb, SPMI_MST_DBG);
		spmi_wdt_rec_2 = mtk_spmi_readl(arb->spmimst_base[2], arb, SPMI_WDT_REC);
	}

	if (spmi_nack_1)
		store_nack_info(arb, spmi_nack_1, spmi_nack_data_1);

	pr_notice("SPMI_REC0-0/1/2:0x%x/0x%x/0x%x SPMI_REC1-0/1/2:0x%x/0x%x/0x%x\n",
		spmi_nack_0, spmi_nack_1, spmi_nack_2,
		spmi_nack_data_0, spmi_nack_data_1, spmi_nack_data_2);

	// Write fail nack, causing OP_ST_NACK/PMIF_NACK/PMIF_BYTE_ERR/PMIF_GRP_RD_ERR
	if ((spmi_nack_0 & 0xD8) || (spmi_nack_1 & 0xD8) || (spmi_nack_2 & 0xD8)) {
		dump_spmi_pmic_dbg_rg(arb, spmi_nack_0, spmi_nack_1, spmi_nack_2);
		spmi_slvid_nack_cnt_add(spmi_nack_0, spmi_nack_1);
		pr_notice("%s spmi transaction fail (Write) irq triggered", __func__);
		pr_notice("SPMI_REC0-0/1/2:0x%x/0x%x/0x%x SPMI_REC1-0/1/2:0x%x/0x%x/0x%x\n",
			spmi_nack_0, spmi_nack_1, spmi_nack_2,
			spmi_nack_data_0, spmi_nack_data_1, spmi_nack_data_2);
		if (arb->caps == 3) {
			if (spmi_nack_0 & 0xD8) {
				flag = 1;
			} else {
				if ((GET_SPMI_NACK_SLVID(spmi_nack_1)) == 0xf) {
					pr_notice("%s, Avoid trigger AEE event while writing slvid:0xf for SWRGO project\n", __func__);
					flag = 0;
				} else {
					flag = 1;
				}
			}
		} else {
			if (spmi_nack_0 & 0xD8) {
				flag = 1;
				assert_flag = 1;
			} else if (spmi_nack_1 & 0xD8) {
				flag = 1;
				assert_flag = (mt6316_revision_check(arb,
					GET_SPMI_NACK_SLVID(spmi_nack_1)) == MT6316_E4) ? 1 : 0;
			} else if (spmi_nack_2 & 0xD8) {
				flag = 1;
				assert_flag = 0;
			}
		}
	}
	if ((spmi_rcs_nack_0 & 0xC0000) || (spmi_rcs_nack_1 & 0xC0000) || (spmi_rcs_nack_2 & 0xC0000)) {
		pr_notice("%s spmi_rcs transaction fail irq triggered SPMI_REC_CMD_DEC-0/1/2:0x%x/0x%x/0x%x\n",
			__func__, spmi_rcs_nack_0, spmi_rcs_nack_1, spmi_rcs_nack_2);
		flag = 0;
	}
	if ((spmi_debug_nack_0 & 0xF0000) || (spmi_debug_nack_1 & 0xF0000) || (spmi_debug_nack_2 & 0xF0000)) {
		pr_notice("%s spmi_debug_nack transaction fail irq triggered SPMI_DEC_DBG-0/1/2:0x%x/0x%x/0x%x\n",
			__func__, spmi_debug_nack_0, spmi_debug_nack_1, spmi_debug_nack_2);
		flag = 0;
	}
	if ((spmi_mst_nack_0 & 0xC0000) || (spmi_mst_nack_1 & 0xC0000) || (spmi_mst_nack_2 & 0xC0000)) {
		pr_notice("%s spmi_mst_nack transaction fail irq triggered SPMI_MST_DBG-0/1/2:0x%x/0x%x/0x%x\n",
		__func__, spmi_mst_nack_0, spmi_mst_nack_1, spmi_mst_nack_2);
		flag = 0;
	}
	// Read fail nack, causing parity error
	if ((spmi_nack_0 & 0x20) || (spmi_nack_1 & 0x20) || (spmi_nack_2 & 0x20)) {
		if (arb->caps == 3) {
			if (spmi_nack_0 & 0x20) {
				flag = (in_spmi_nack_monitor_list(spmi_nack_0)) ? 1 : 0;
			} else {
				if ((GET_SPMI_NACK_SLVID(spmi_nack_1)) == 0xf) {
					pr_notice("%s, Avoid trigger AEE event while writing slvid:0xf for SWRGO project\n", __func__);
					flag = 0;
				} else {
					flag = 1;
				}
			}
		} else {
			dump_spmi_pmic_dbg_rg(arb, spmi_nack_0, spmi_nack_1, spmi_nack_2);
			if (spmi_nack_0 & 0x20) {
				flag = (in_spmi_nack_monitor_list(spmi_nack_0)) ? 1 : 0;
				assert_flag = (in_spmi_nack_monitor_list(spmi_nack_0)) ? 1 : 0;
			} else if (spmi_nack_1 & 0x20) {
				flag = 1;
				if ((mt6316_revision_check(arb, GET_SPMI_NACK_SLVID(spmi_nack_1)) ==
					MT6316_E4) && (in_spmi_nack_monitor_list(spmi_nack_1))) {
					assert_flag = 1;
				} else {
					assert_flag = 0;
				}
			} else if (spmi_nack_2 & 0x20) {
				flag = 0;
				assert_flag = 0;
			}
		}
		pr_notice("%s spmi transaction fail (Read) irq triggered", __func__);
		pr_notice("SPMI_REC0-0/1/2:0x%x/0x%x/0x%x SPMI_REC1-0/1/2:0x%x/0x%x/0x%x\n",
			spmi_nack_0, spmi_nack_1, spmi_nack_2, spmi_nack_data_0, spmi_nack_data_1, spmi_nack_data_2);
	}
	/* SPMI WDT IRQ triggered */
	if (spmi_wdt_rec_0 || spmi_wdt_rec_1 || spmi_wdt_rec_2) {
		pr_notice("%s SPMI WDT IRQ triggered\n", __func__);
		pr_notice("%s SPMI_WDT_REC-0/1/2:0x%x/0x%x/0x%x\n", __func__, spmi_wdt_rec_0,
			spmi_wdt_rec_1, spmi_wdt_rec_2);
		flag = 1;
	}

	if (flag) {
		/* trigger AEE event*/
		if (IS_ENABLED(CONFIG_MTK_AEE_FEATURE))
			aee_kernel_warning("SPMI", "SPMI:transaction_fail");
	}
	/* clear irq*/
	if ((spmi_nack_0 & 0xF8) || (spmi_rcs_nack_0 & 0xC0000) ||
		(spmi_debug_nack_0 & 0xF0000) || (spmi_mst_nack_0 & 0xC0000)) {
		mtk_spmi_writel(arb->spmimst_base[0], arb, 0x3, SPMI_REC_CTRL);
	} else if ((spmi_nack_1 & 0xF8) || (spmi_rcs_nack_1 & 0xC0000) ||
		(spmi_debug_nack_1 & 0xF0000) || (spmi_mst_nack_1 & 0xC0000)) {
		if (spmi_nack_1 & 0xD8) {
			pr_notice("SPMI_REC0-0/1/2:0x%x/0x%x/0x%x SPMI_REC1-0/1/2:0x%x/0x%x/0x%x\n",
				spmi_nack_0, spmi_nack_1, spmi_nack_2,
				spmi_nack_data_0, spmi_nack_data_1, spmi_nack_data_2);
			pr_notice("%s spmi_rcs transaction fail irq triggered SPMI_REC_CMD_DEC-0/1/2:0x%x/0x%x/0x%x\n",
				__func__, spmi_rcs_nack_0, spmi_rcs_nack_1, spmi_rcs_nack_2);
			pr_notice("%s spmi_debug_nack transaction fail irq triggered SPMI_DEC_DBG-0/1/2:0x%x/0x%x/0x%x\n",
				__func__, spmi_debug_nack_0, spmi_debug_nack_1, spmi_debug_nack_2);
			pr_notice("%s spmi_mst_nack transaction fail irq triggered SPMI_MST_DBG-0/1/2:0x%x/0x%x/0x%x\n",
			__func__, spmi_mst_nack_0, spmi_mst_nack_1, spmi_mst_nack_2);
		}
		mtk_spmi_writel(arb->spmimst_base[1], arb, 0x3, SPMI_REC_CTRL);
	} else if (spmi_wdt_rec_0 || spmi_wdt_rec_1 || spmi_wdt_rec_2) {
		pr_notice("%s SPMI_WDT_REC:0/1/2:0x%x/0x%x/0x%x\n", __func__, spmi_wdt_rec_0,
			spmi_wdt_rec_1, spmi_wdt_rec_2);
			mtk_spmi_writel(arb->spmimst_base[0], arb, 0x7, SPMI_REC_CTRL);
		if (!IS_ERR_OR_NULL(arb->spmimst_base[1]))
			mtk_spmi_writel(arb->spmimst_base[1], arb, 0x7, SPMI_REC_CTRL);
		if (!IS_ERR_OR_NULL(arb->spmimst_base[2]))
			mtk_spmi_writel(arb->spmimst_base[2], arb, 0x7, SPMI_REC_CTRL);
		pr_notice("%s SPMI_WDT_IRQ is cleared\n", __func__);
	} else {
		mtk_spmi_writel(arb->spmimst_base[0], arb, 0x7, SPMI_REC_CTRL);
		if (!IS_ERR_OR_NULL(arb->spmimst_base[1]))
			mtk_spmi_writel(arb->spmimst_base[1], arb, 0x7, SPMI_REC_CTRL);
		if (!IS_ERR_OR_NULL(arb->spmimst_base[2]))
			mtk_spmi_writel(arb->spmimst_base[2], arb, 0x7, SPMI_REC_CTRL);
		pr_notice("%s Force clear IRQ\n", __func__);
	}

	if (assert_flag)
		BUG_ON(1);

	mutex_unlock(&arb->pmif_m_mutex);
	__pm_relax(arb->pmif_m_Thread_lock);

	return IRQ_HANDLED;
}

static int spmi_nack_irq_register(struct platform_device *pdev,
		struct pmif *arb, int irq)
{
	int ret = 0;

	ret = devm_request_threaded_irq(&pdev->dev, irq, NULL,
				spmi_nack_irq_handler,
				IRQF_TRIGGER_HIGH | IRQF_ONESHOT | IRQF_SHARED,
				"spmi_nack_irq", arb);
	if (ret < 0) {
		dev_notice(&pdev->dev, "request %s irq fail\n",
			"spmi_nack_irq");
	}
	return ret;
}

static void rcs_irq_lock(struct irq_data *data)
{
	struct pmif *arb = irq_data_get_irq_chip_data(data);

	mutex_lock(&arb->rcs_m_irqlock);
}

static void rcs_irq_sync_unlock(struct irq_data *data)
{
	struct pmif *arb = irq_data_get_irq_chip_data(data);

	mutex_unlock(&arb->rcs_m_irqlock);
}

static void rcs_irq_enable(struct irq_data *data)
{
	unsigned int hwirq = irqd_to_hwirq(data);
	struct pmif *arb = irq_data_get_irq_chip_data(data);

	arb->rcs_enable_hwirq[hwirq] = true;
}

static void rcs_irq_disable(struct irq_data *data)
{
	unsigned int hwirq = irqd_to_hwirq(data);
	struct pmif *arb = irq_data_get_irq_chip_data(data);

	arb->rcs_enable_hwirq[hwirq] = false;
}

static const struct irq_chip rcs_irq_chip = {
	.name			= "rcs_irq",
	.irq_bus_lock		= rcs_irq_lock,
	.irq_bus_sync_unlock	= rcs_irq_sync_unlock,
	.irq_enable		= rcs_irq_enable,
	.irq_disable		= rcs_irq_disable,
};

static const struct irq_chip rcs_irq_chip_m2 = {
	.name			= "rcs_irq_m2",
	.irq_bus_lock		= rcs_irq_lock,
	.irq_bus_sync_unlock	= rcs_irq_sync_unlock,
	.irq_enable		= rcs_irq_enable,
	.irq_disable		= rcs_irq_disable,
};

static const struct irq_chip rcs_irq_chip_p = {
	.name			= "rcs_irq_p",
	.irq_bus_lock		= rcs_irq_lock,
	.irq_bus_sync_unlock	= rcs_irq_sync_unlock,
	.irq_enable		= rcs_irq_enable,
	.irq_disable		= rcs_irq_disable,
};

static int rcs_irq_map(struct irq_domain *d, unsigned int virq,
			irq_hw_number_t hw)
{
	struct pmif *arb = d->host_data;

	irq_set_chip_data(virq, arb);
	irq_set_chip(virq, &arb->irq_chip);
	irq_set_nested_thread(virq, 1);
	irq_set_parent(virq, arb->rcs_irq);
	irq_set_noprobe(virq);

	return 0;
}

static const struct irq_domain_ops rcs_irq_domain_ops = {
	.map	= rcs_irq_map,
	.xlate	= irq_domain_xlate_onetwocell,
};

static irqreturn_t rcs_irq_handler(int irq, void *data)
{
	struct pmif *arb = data;
	unsigned int slv_irq_sta_0 = 0, slv_irq_sta_1 = 0, slv_irq_sta_2 = 0;
	int i;

	for (i = 0; i < SPMI_MAX_SLAVE_ID; i++) {
		slv_irq_sta_0 = mtk_spmi_readl(arb->spmimst_base[0], arb, SPMI_SLV_3_0_EINT + (i / 4));
		slv_irq_sta_0 = (slv_irq_sta_0 >> ((i % 4) * 8)) & 0xFF;

		if (!IS_ERR_OR_NULL(arb->spmimst_base[1])) {
			slv_irq_sta_1 = mtk_spmi_readl(arb->spmimst_base[1], arb, SPMI_SLV_3_0_EINT + (i / 4));
			slv_irq_sta_1 = (slv_irq_sta_1 >> ((i % 4) * 8)) & 0xFF;
		}
		if (!IS_ERR_OR_NULL(arb->spmimst_base[2])) {
			slv_irq_sta_2 = mtk_spmi_readl(arb->spmimst_base[2], arb, SPMI_SLV_3_0_EINT + (i / 4));
			slv_irq_sta_2 = (slv_irq_sta_2 >> ((i % 4) * 8)) & 0xFF;
		}

		/* need to clear using 0xFF to avoid new irq happen
		 * after read SPMI_SLV_3_0_EINT + (i/4) value then use
		 * this value to clean
		 */
		if (slv_irq_sta_0) {
			mtk_spmi_writel(arb->spmimst_base[0], arb, (0xFF << ((i % 4) * 8)),
					SPMI_SLV_3_0_EINT + (i / 4));
			if (arb->rcs_enable_hwirq[i] && slv_irq_sta_0) {
				dev_info(&arb->spmic->dev,
					"spmi-0 hwirq=%d, sta=0x%x\n", i, slv_irq_sta_0);
				handle_nested_irq(irq_find_mapping(arb->domain, i));
			}
		} else if (slv_irq_sta_1) {
			if (!IS_ERR_OR_NULL(arb->spmimst_base[1])) {
				mtk_spmi_writel(arb->spmimst_base[1], arb, (0xFF << ((i % 4) * 8)),
					SPMI_SLV_3_0_EINT + (i / 4));
				if (arb->rcs_enable_hwirq[i] && slv_irq_sta_1) {
					dev_info(&arb->spmic->dev,
						"spmi-1 hwirq=%d, sta=0x%x\n", i, slv_irq_sta_1);
					handle_nested_irq(irq_find_mapping(arb->domain, i));
				}
			}
		} else if (slv_irq_sta_2) {
			if (!IS_ERR_OR_NULL(arb->spmimst_base[2])) {
				mtk_spmi_writel(arb->spmimst_base[2], arb, (0xFF << ((i % 4) * 8)),
					SPMI_SLV_3_0_EINT + (i / 4));
				if (arb->rcs_enable_hwirq[i] && slv_irq_sta_2) {
					dev_info(&arb->spmic->dev,
						"spmi-2 hwirq=%d, sta=0x%x\n", i, slv_irq_sta_2);
					handle_nested_irq(irq_find_mapping(arb->domain, i));
				}
			}
		}
	}
	return IRQ_HANDLED;
}

static int rcs_irq_register(struct platform_device *pdev,
			    struct pmif *arb, int irq)
{
	int i, ret = 0;

	mutex_init(&arb->rcs_m_irqlock);
	mutex_init(&arb->rcs_p_irqlock);
	arb->rcs_enable_hwirq = devm_kcalloc(&pdev->dev, SPMI_MAX_SLAVE_ID,
					     sizeof(*arb->rcs_enable_hwirq),
					     GFP_KERNEL);
	if (!arb->rcs_enable_hwirq)
		return -ENOMEM;

	if (arb->rcs_irq == irq)
		arb->irq_chip = rcs_irq_chip;
	else if (arb->rcs_irq_m2 == irq)
		arb->irq_chip_m2 = rcs_irq_chip_m2;
	else if (arb->rcs_irq_p == irq)
		arb->irq_chip_p = rcs_irq_chip_p;
	else
		dev_notice(&pdev->dev, "no rcs irq %d registered\n", irq);

	arb->domain = irq_domain_add_linear(pdev->dev.of_node,
					    SPMI_MAX_SLAVE_ID,
					    &rcs_irq_domain_ops, arb);
	if (!arb->domain) {
		dev_notice(&pdev->dev, "Failed to create IRQ domain\n");
		return -ENODEV;
	}
	/* clear all slave irq status */
	for (i = 0; i < SPMI_MAX_SLAVE_ID; i++) {
		mtk_spmi_writel(arb->spmimst_base[0], arb, (0xFF << ((i % 4) * 8)),
				SPMI_SLV_3_0_EINT + (i / 4));
		if (!IS_ERR_OR_NULL(arb->spmimst_base[1])) {
			mtk_spmi_writel(arb->spmimst_base[1], arb, (0xFF << ((i % 4) * 8)),
				SPMI_SLV_3_0_EINT + (i / 4));
		}
		if (!IS_ERR_OR_NULL(arb->spmimst_base[2])) {
			mtk_spmi_writel(arb->spmimst_base[2], arb, (0xFF << ((i % 4) * 8)),
				SPMI_SLV_3_0_EINT + (i / 4));
		}
	}
	ret = devm_request_threaded_irq(&pdev->dev, irq, NULL,
					rcs_irq_handler, IRQF_ONESHOT,
					rcs_irq_chip.name, arb);
	if (ret < 0) {
		dev_notice(&pdev->dev, "Failed to request IRQ=%d, ret = %d\n",
			   irq, ret);
		return ret;
	}
	enable_irq_wake(irq);

	return ret;
}

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
static void pmif_spmi_mrdump_register(struct platform_device *pdev, struct pmif *arb)
{
	u32 reg[12] = {0};
	int ret;

	ret = of_property_read_u32_array(pdev->dev.of_node, "reg", reg, ARRAY_SIZE(reg));
	if (ret < 0) {
		dev_notice(&pdev->dev, "Failed to request reg from SPMI node\n");
		return;
	}

	if (reg[3] > DUMP_LIMIT || reg[7] > DUMP_LIMIT || reg[11] > DUMP_LIMIT) {
		dev_info(&pdev->dev, "%s: dump size > 4K\n", __func__);
		return;
	}

	ret = mrdump_mini_add_extra_file((unsigned long)arb->pmif_base[0], reg[1], reg[3], "PMIF_M_DATA");
	if (ret)
		dev_info(&pdev->dev, "%s: PMIF_M_DATA add fail(%d)\n",
			 __func__, ret);

	ret = mrdump_mini_add_extra_file((unsigned long)arb->pmif_base[1], reg[1], reg[3], "PMIF_P_DATA");
	if (ret)
		dev_info(&pdev->dev, "%s: PMIF_P_DATA add fail(%d)\n",
			 __func__, ret);

	ret = mrdump_mini_add_extra_file((unsigned long)arb->spmimst_base[0],
					reg[5], reg[7], "SPMI_M_DATA");
	if (ret)
		dev_info(&pdev->dev, "%s: SPMI_M_DATA add fail(%d)\n",
			__func__, ret);

	ret = mrdump_mini_add_extra_file((unsigned long)arb->spmimst_base[1],
					reg[5], reg[7], "SPMI_P_DATA");
	if (ret)
		dev_info(&pdev->dev, "%s: SPMI_P_DATA add fail(%d)\n",
			__func__, ret);

	ret = mrdump_mini_add_extra_file((unsigned long)arb->busdbgregs,
					reg[9], reg[11], "DEM_DBG_DATA");
	if (ret)
		dev_info(&pdev->dev, "%s: DEM_DBG_DATA add fail(%d)\n",
			__func__, ret);
}
#endif

static int mtk_spmi_probe(struct platform_device *pdev)
{
	struct pmif *arb;
	struct resource *res;
	struct spmi_controller *ctrl;
	int err = 0;
	int spmi_path_cnt = 0;
#if defined(CONFIG_FPGA_EARLY_PORTING)
	u8 id_l = 0, id_h = 0, val = 0, test_id = 0x5;
	u16 hwcid_l_addr = 0x8, hwcid_h_addr = 0x9, test_w_addr = 0x3a7;
#endif
	ctrl = spmi_controller_alloc(&pdev->dev, sizeof(*arb));
	if (!ctrl)
		return -ENOMEM;

	ctrl->cmd = pmif_arb_cmd;
	ctrl->read_cmd = pmif_spmi_read_cmd;
	ctrl->write_cmd = pmif_spmi_write_cmd;

	/* re-assign of_id->data */
	spmi_controller_set_drvdata(ctrl, (void *)of_device_get_match_data(&pdev->dev));
	arb = spmi_controller_get_drvdata(ctrl);
	arb->spmic = ctrl;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pmif");
	arb->pmif_base[spmi_path_cnt] = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR_OR_NULL(arb->pmif_base[spmi_path_cnt])) {
		dev_notice(&pdev->dev, "[PMIF]:no pmif found\n");
		err = PTR_ERR(arb->pmif_base[spmi_path_cnt]);
		goto err_put_ctrl;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "spmimst");
	arb->spmimst_base[spmi_path_cnt] = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR_OR_NULL(arb->spmimst_base[spmi_path_cnt]))
		dev_notice(&pdev->dev, "[PMIF]:no spmimst found\n");
	else
		spmi_path_cnt++;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pmif-m2");
	arb->pmif_base[spmi_path_cnt] = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR_OR_NULL(arb->pmif_base[spmi_path_cnt]))
		dev_notice(&pdev->dev, "[PMIF]:no pmif-m2 found\n");

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "spmimst-m2");
	arb->spmimst_base[spmi_path_cnt] = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR_OR_NULL(arb->spmimst_base[spmi_path_cnt]))
		dev_notice(&pdev->dev, "[PMIF]:no spmimst-m2 found\n");
	else
		spmi_path_cnt++;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pmif-p");
	arb->pmif_base[spmi_path_cnt] = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR_OR_NULL(arb->pmif_base[spmi_path_cnt]))
		dev_notice(&pdev->dev, "[PMIF]:no pmif-p found\n");

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "spmimst-p");
	arb->spmimst_base[spmi_path_cnt] = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR_OR_NULL(arb->spmimst_base[spmi_path_cnt]))
		dev_notice(&pdev->dev, "[PMIF]:no spmimst-p found\n");
	else
		spmi_path_cnt++;

	err = of_property_read_u32_array(pdev->dev.of_node, "hwinf-err-irq-idx",
		arb->hwintf_err_idx, ARRAY_SIZE(arb->hwintf_err_idx));
	if (err)
		dev_info(&pdev->dev, "[PMIF]: No hwinf-err-irq-idx found\n");

	err = of_property_read_u32_array(pdev->dev.of_node, "swinf-err-irq-idx",
		arb->swintf_err_idx, ARRAY_SIZE(arb->swintf_err_idx));
	if (err)
		dev_info(&pdev->dev, "[PMIF]: No swinf-err-irq-idx found\n");

	err = of_property_read_u32_array(pdev->dev.of_node, "hwinf-err-irq-idx-p",
		arb->hwintf_err_idx_p, ARRAY_SIZE(arb->hwintf_err_idx_p));
	if (err) {
		of_property_read_u32_array(pdev->dev.of_node, "hwinf-err-irq-idx",
		arb->hwintf_err_idx_p, ARRAY_SIZE(arb->hwintf_err_idx_p));
		dev_info(&pdev->dev, "[PMIF]: No hwinf-err-irq-idx-p found, copy from m\n");
	}

	err = of_property_read_u32_array(pdev->dev.of_node, "swinf-err-irq-idx-p",
		arb->swintf_err_idx_p, ARRAY_SIZE(arb->swintf_err_idx_p));
	if (err) {
		of_property_read_u32_array(pdev->dev.of_node, "swinf-err-irq-idx",
		arb->swintf_err_idx_p, ARRAY_SIZE(arb->swintf_err_idx_p));
		dev_info(&pdev->dev, "[PMIF]: No swinf-err-irq-idx-p found, copy from m\n");
	}

	err = of_property_read_u32_array(pdev->dev.of_node, "hwinf-err-irq-idx-m2",
		arb->hwintf_err_idx_m2, ARRAY_SIZE(arb->hwintf_err_idx_m2));
	if (err) {
		of_property_read_u32_array(pdev->dev.of_node, "swinf-err-irq-idx",
		arb->hwintf_err_idx_m2, ARRAY_SIZE(arb->hwintf_err_idx_m2));
		dev_info(&pdev->dev, "[PMIF]: No hwinf-err-irq-idx-m2 found, copy from m\n");
	}

	err = of_property_read_u32_array(pdev->dev.of_node, "swinf-err-irq-idx-m2",
		arb->swintf_err_idx_m2, ARRAY_SIZE(arb->swintf_err_idx_m2));
	if (err) {
		of_property_read_u32_array(pdev->dev.of_node, "swinf-err-irq-idx",
		arb->swintf_err_idx_m2, ARRAY_SIZE(arb->swintf_err_idx_m2));
		dev_info(&pdev->dev, "[PMIF]: No swinf-err-irq-idx-m2 found, copy from m\n");
	}
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "bugdbg");
	arb->busdbgregs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR_OR_NULL(arb->busdbgregs))
		dev_notice(&pdev->dev, "[PMIF]:no bus dbg regs found\n");
#endif

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	if (arb->caps == 1) {
		arb->pmif_sys_ck = devm_clk_get(&pdev->dev, "pmif_sys_ck");
		if (IS_ERR_OR_NULL(arb->pmif_sys_ck)) {
			dev_notice(&pdev->dev, "[PMIF]:failed to get ap clock: %ld\n",
			PTR_ERR(arb->pmif_sys_ck));
			goto err_put_ctrl;
		}

		arb->pmif_tmr_ck = devm_clk_get(&pdev->dev, "pmif_tmr_ck");
		if (IS_ERR_OR_NULL(arb->pmif_tmr_ck)) {
			dev_notice(&pdev->dev, "[PMIF]:failed to get tmr clock: %ld\n",
			PTR_ERR(arb->pmif_tmr_ck));
			goto err_put_ctrl;
		}

		arb->spmimst_clk_mux = devm_clk_get(&pdev->dev, "spmimst_clk_mux");
		if (IS_ERR_OR_NULL(arb->spmimst_clk_mux)) {
			dev_notice(&pdev->dev, "[SPMIMST]:failed to get clock: %ld\n",
			PTR_ERR(arb->spmimst_clk_mux));
			goto err_put_ctrl;
		} else
			err = clk_prepare_enable(arb->spmimst_clk_mux);
		if (err) {
			dev_notice(&pdev->dev, "[SPMIMST]:failed to enable spmi master clk\n");
			goto err_put_ctrl;
		}
	}
#else
	dev_notice(&pdev->dev, "[PMIF]:no need to get clock at fpga\n");
#endif /* #if defined(CONFIG_MTK_FPGA) || defined(CONFIG_FPGA_EARLY_PORTING) */
	arb->chan.ch_sta = PMIF_SWINF_0_STA + (PMIF_CHAN_OFFSET * arb->soc_chan);
	arb->chan.wdata = PMIF_SWINF_0_WDATA_31_0 + (PMIF_CHAN_OFFSET * arb->soc_chan);
	arb->chan.rdata = PMIF_SWINF_0_RDATA_31_0 + (PMIF_CHAN_OFFSET * arb->soc_chan);
	arb->chan.ch_send = PMIF_SWINF_0_ACC + (PMIF_CHAN_OFFSET * arb->soc_chan);
	arb->chan.ch_rdy = PMIF_SWINF_0_VLD_CLR + (PMIF_CHAN_OFFSET * arb->soc_chan);

	raw_spin_lock_init(&arb->lock_m);
	raw_spin_lock_init(&arb->lock_p);
	arb->pmif_m_Thread_lock =
		wakeup_source_register(NULL, "pmif_m wakelock");
	arb->pmif_p_Thread_lock =
		wakeup_source_register(NULL, "pmif_p wakelock");
	mutex_init(&arb->pmif_m_mutex);
	mutex_init(&arb->pmif_p_mutex);

	/* enable debugger */
	spmi_pmif_dbg_init(ctrl);
	spmi_pmif_create_attr(&mtk_spmi_driver.driver);

	spmi_nack_monitor_list_parse(pdev);

	arb->irq = platform_get_irq_byname(pdev, "pmif_irq");
	if (arb->irq < 0)
		dev_notice(&pdev->dev,
			   "Failed to get pmif_irq, ret = %d\n", arb->irq);

	pmif_irq_register(pdev, arb, arb->irq);

	arb->irq_p = platform_get_irq_byname(pdev, "pmif_p_irq");
	if (arb->irq_p < 0)
		dev_notice(&pdev->dev,
			   "Failed to get pmif_p_irq, ret = %d\n", arb->irq_p);

	pmif_irq_register(pdev, arb, arb->irq_p);

	arb->irq_m2 = platform_get_irq_byname(pdev, "pmif_m2_irq");
	if (arb->irq_m2 < 0)
		dev_notice(&pdev->dev,
			   "Failed to get pmif_m2_irq, ret = %d\n", arb->irq_m2);

	pmif_irq_register(pdev, arb, arb->irq_m2);

	arb->rcs_irq = platform_get_irq_byname(pdev, "rcs_irq");
	if (arb->rcs_irq < 0) {
		dev_notice(&pdev->dev,
			   "Failed to get rcs_irq, ret = %d\n", arb->rcs_irq);
	} else {
		err = rcs_irq_register(pdev, arb, arb->rcs_irq);
		if (err)
			dev_notice(&pdev->dev,
			   "Failed to register rcs_irq, ret = %d\n", arb->rcs_irq);
	}

	arb->rcs_irq_m2 = platform_get_irq_byname(pdev, "rcs_irq_m2");
	if (arb->rcs_irq_m2 < 0) {
		dev_notice(&pdev->dev,
			   "Failed to get rcs_irq_m2, ret = %d\n", arb->rcs_irq_m2);
	} else {
		err = rcs_irq_register(pdev, arb, arb->rcs_irq_m2);
		if (err)
			dev_notice(&pdev->dev,
			   "Failed to register rcs_irq_m2, ret = %d\n", arb->rcs_irq_m2);
	}

	arb->rcs_irq_p = platform_get_irq_byname(pdev, "rcs_irq_p");
	if (arb->rcs_irq_p < 0) {
		dev_notice(&pdev->dev,
			   "Failed to get rcs_irq_p, ret = %d\n", arb->rcs_irq_p);
	} else {
		err = rcs_irq_register(pdev, arb, arb->rcs_irq_p);
		if (err)
			dev_notice(&pdev->dev,
			   "Failed to register rcs_irq_p, ret = %d\n", arb->rcs_irq_p);
	}

	arb->spmi_nack_irq = platform_get_irq_byname(pdev, "spmi_nack_irq");
	if (arb->spmi_nack_irq < 0) {
		dev_notice(&pdev->dev,
			"Failed to get spmi_nack_irq, ret = %d\n", arb->spmi_nack_irq);
	} else {
		err = spmi_nack_irq_register(pdev, arb, arb->spmi_nack_irq);
		if (err)
			dev_notice(&pdev->dev,
				"Failed to register spmi_nack_irq, ret = %d\n",
				 arb->spmi_nack_irq);
	}

	arb->spmi_p_nack_irq = platform_get_irq_byname(pdev, "spmi_p_nack_irq");
	if (arb->spmi_p_nack_irq < 0) {
		dev_notice(&pdev->dev,
			"Failed to get spmi_p_nack_irq, ret = %d\n", arb->spmi_p_nack_irq);
	} else {
		err = spmi_nack_irq_register(pdev, arb, arb->spmi_p_nack_irq);
		if (err)
			dev_notice(&pdev->dev,
				"Failed to register spmi_p_nack_irq, ret = %d\n",
				 arb->spmi_p_nack_irq);
	}

	arb->spmi_m2_nack_irq = platform_get_irq_byname(pdev, "spmi_m2_nack_irq");
	if (arb->spmi_m2_nack_irq < 0) {
		dev_notice(&pdev->dev,
			"Failed to get spmi_m2_nack_irq, ret = %d\n", arb->spmi_m2_nack_irq);
	} else {
		err = spmi_nack_irq_register(pdev, arb, arb->spmi_m2_nack_irq);
		if (err)
			dev_notice(&pdev->dev,
				"Failed to register spmi_m2_nack_irq, ret = %d\n",
					 arb->spmi_m2_nack_irq);
	}

	spmi_dev_parse(pdev);
#if defined(CONFIG_FPGA_EARLY_PORTING)
	/* pmif/spmi initial setting */
	pmif_writel(arb->pmif_base[0], arb, 0xffffffff, PMIF_INF_EN);
	pmif_writel(arb->pmif_base[0], arb, 0xffffffff, PMIF_ARB_EN);
	pmif_writel(arb->pmif_base[0], arb, 0x1, PMIF_CMDISSUE_EN);
	pmif_writel(arb->pmif_base[0], arb, 0x1, PMIF_INIT_DONE);

	if (!IS_ERR_OR_NULL(arb->pmif_base[1])) {
		pmif_writel(arb->pmif_base[1], arb, 0xffffffff, PMIF_INF_EN);
		pmif_writel(arb->pmif_base[1], arb, 0xffffffff, PMIF_ARB_EN);
		pmif_writel(arb->pmif_base[1], arb, 0x1, PMIF_CMDISSUE_EN);
		pmif_writel(arb->pmif_base[1], arb, 0x1, PMIF_INIT_DONE);
	}
	if (!IS_ERR_OR_NULL(arb->pmif_base[2])) {
		pmif_writel(arb->pmif_base[2], arb, 0xffffffff, PMIF_INF_EN);
		pmif_writel(arb->pmif_base[2], arb, 0xffffffff, PMIF_ARB_EN);
		pmif_writel(arb->pmif_base[2], arb, 0x1, PMIF_CMDISSUE_EN);
		pmif_writel(arb->pmif_base[2], arb, 0x1, PMIF_INIT_DONE);
	}

	mtk_spmi_writel(arb->spmimst_base[0], arb, 0x1, SPMI_MST_REQ_EN);

	if (!IS_ERR_OR_NULL(arb->spmimst_base[1]))
		mtk_spmi_writel(arb->spmimst_base[1], arb, 0x1, SPMI_MST_REQ_EN);

	if (!IS_ERR_OR_NULL(arb->spmimst_base[2]))
		mtk_spmi_writel(arb->spmimst_base[2], arb, 0x1, SPMI_MST_REQ_EN);

	/* r/w verification */
	ctrl->read_cmd(ctrl, 0x38, test_id, hwcid_l_addr, &id_l, 1);
	ctrl->read_cmd(ctrl, 0x38, test_id, hwcid_h_addr, &id_h, 1);
	dev_notice(&pdev->dev, "%s PMIC=[0x%x%x]\n", __func__, id_h, id_l);
	val = 0x5a;
	ctrl->write_cmd(ctrl, 0x30, test_id, test_w_addr, &val, 1);
	val = 0x0;
	ctrl->read_cmd(ctrl, 0x38, test_id, test_w_addr, &val, 1);
	dev_notice(&pdev->dev, "%s check [0x%x] = 0x%x\n", __func__, test_w_addr, val);
#endif

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	/* add mrdump for reboot DB*/
	pmif_spmi_mrdump_register(pdev, arb);
#endif
	platform_set_drvdata(pdev, ctrl);

	err = spmi_controller_add(ctrl);
	if (err)
		goto err_domain_remove;

	return 0;

err_domain_remove:
	if (arb->domain)
		irq_domain_remove(arb->domain);
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	if (arb->caps == 1)
		clk_disable_unprepare(arb->spmimst_clk_mux);
#endif
err_put_ctrl:
	spmi_controller_put(ctrl);
	return err;
}

static int mtk_spmi_remove(struct platform_device *pdev)
{
	struct spmi_controller *ctrl = platform_get_drvdata(pdev);
	struct pmif *arb = spmi_controller_get_drvdata(ctrl);

	if (arb->domain)
		irq_domain_remove(arb->domain);
	spmi_controller_remove(ctrl);
	spmi_controller_put(ctrl);
	return 0;
}

static const struct of_device_id mtk_spmi_match_table[] = {
	{
		.compatible = "mediatek,mt6853-pmif-m",
		.data = &mt6853_pmif_arb,
	}, {
		.compatible = "mediatek,mt6833-spmi-m",
		.data = &mt6877_pmif_arb,
	}, {
		.compatible = "mediatek,mt6855-spmi",
		.data = &mt6xxx_pmif_arb,
	}, {
		.compatible = "mediatek,mt6873-spmi",
		.data = &mt6873_pmif_arb,
	}, {
		.compatible = "mediatek,mt6877-pmif-m",
		.data = &mt6877_pmif_arb,
	}, {
		.compatible = "mediatek,mt6879-spmi",
		.data = &mt6xxx_pmif_arb,
	}, {
		.compatible = "mediatek,mt6886-spmi",
		.data = &mt6xxx_pmif_arb,
	}, {
		.compatible = "mediatek,mt6893-spmi-m",
		.data = &mt6893_pmif_arb,
	}, {
		.compatible = "mediatek,mt6895-spmi",
		.data = &mt6xxx_pmif_arb,
	}, {
		.compatible = "mediatek,mt6897-spmi",
		.data = &mt6xxx_pmif_arb,
	}, {
		.compatible = "mediatek,mt6983-spmi",
		.data = &mt6xxx_pmif_arb,
	}, {
		.compatible = "mediatek,mt6899-spmi",
		.data = &mt6xxx_pmif_arb,
	}, {
		.compatible = "mediatek,mt6985-spmi",
		.data = &mt6xxx_pmif_arb,
	}, {
		.compatible = "mediatek,mt6989-spmi",
		.data = &mt6989_pmif_arb,
	}, {
		.compatible = "mediatek,mt6991-spmi",
		.data = &mt6xxx_pmif_arb,
	}, {
		/* sentinel */
	},
};
MODULE_DEVICE_TABLE(of, mtk_spmi_match_table);

static struct platform_driver mtk_spmi_driver = {
	.driver		= {
		.name	= "spmi-mtk",
		.of_match_table = of_match_ptr(mtk_spmi_match_table),
	},
	.probe		= mtk_spmi_probe,
	.remove		= mtk_spmi_remove,
};
module_platform_driver(mtk_spmi_driver);

MODULE_AUTHOR("Hsin-Hsiung Wang <hsin-hsiung.wang@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SPMI Driver");
MODULE_LICENSE("GPL");
