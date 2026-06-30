/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_QOS_COMMON_H__
#define __MTK_QOS_COMMON_H__

struct mtk_qos;
struct platform_device;

enum triggerReason {
	TRI_BW_TOTAL,
	TRI_DRAM_TOTAL,
	TRI_OPP_CHG,
	TRI_NEAR_FULL,
	TRI_SW_EMI,
	TRI_SW_LTR,
	TRI_SW_MET,
	TRI_SW_CM,
	TRI_SW_SLBC,
	TRI_SW_SWPM,
	TRI_SW_PMQOS,
	NR_TRI
};

struct mtk_qos_soc {
	const struct qos_ipi_cmd *ipi_pin;
	const struct qos_sram_addr *sram_pin;
};

struct mtk_qos {
	struct device *dev;
	const struct mtk_qos_soc *soc;
	int dram_type;
	void __iomem *regs;
	unsigned int regsize;
	bool legacy_support_v1;
};

extern unsigned int evt_tri_dbg_tbl[NR_TRI];
extern int mtk_qos_probe(struct platform_device *pdev,
			const struct mtk_qos_soc *soc);
extern void qos_ipi_init(struct mtk_qos *qos);
extern void qos_ipi_recv_init(struct mtk_qos *qos);
extern void qos_force_polling_mode(int enable, unsigned int userID);
extern void qos_evt_tri_dbg_enable(int enable);
extern void qos_evt_tri_dbg_read(void);
extern int qos_get_ipi_cmd(int idx);
extern unsigned int is_mtk_qos_enable(void);
#endif

