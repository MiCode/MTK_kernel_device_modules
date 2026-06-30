/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __MCUPM_IPI_ID_H__
#define __MCUPM_IPI_ID_H__

#include <linux/soc/mediatek/mtk_tinysys_ipi.h>

/* define module id here ... */
#define CH_S_PLATFORM	0
#define CH_S_CPU_DVFS	1
#define CH_S_FHCTL	2
#define CH_S_MCDI	3
#define CH_S_SUSPEND	4
#define CH_IPIR_C_MET   5
#define CH_IPIS_C_MET   6
#define CH_S_EEMSN      7
#define CH_S_WLC        8
#define CH_S_PTP3       9
#define CH_S_10         10
#define CH_S_11         11
#define CH_S_12         12
#define CH_S_13         13
#define CH_S_14         14
#define CH_S_15         15

enum mtk_multi_mcupm {
	mst,
	slv0,
	slv1,
	max_mcupm,
};
enum multi_mcupm_chan_id {
	CHAN_PLATFORM,
	CHAN_CPU_DVFS,
	CHAN_FHCTL,
	CHAN_SUSPEND,
	CHAN_MCDI,
	CHAN_IPIR_C_MET,
	CHAN_IPIS_C_MET,
	CHAN_EEMSN,
	CHAN_WLC,
	CHAN_S_PTP3,
	CHAN_S10,
	CHAN_S11,
	CHAN_S12,
	CHAN_S13,
	CHAN_S14,
	CHAN_S15,
	MCUPM_CHAN_MAX,
};


extern struct mtk_mbox_device mcupm_mboxdev;
extern struct mtk_ipi_device mcupm_ipidev;

void *get_mcupm_ipidev(void);
u32 get_mcupms_ipidev_number(void);
#define GET_MCUPM_IPIDEV(t) \
	(t < get_mcupms_ipidev_number() ? (&(((struct mtk_ipi_device *)get_mcupm_ipidev())[t])) : NULL)
#define GET_MCUPM_MBOXDEV(t) \
	(t < get_mcupms_ipidev_number() ? (((struct mtk_ipi_device *)get_mcupm_ipidev())[t].mbdev) : NULL)

#endif /* __MCUPM_IPI_ID_H__ */
