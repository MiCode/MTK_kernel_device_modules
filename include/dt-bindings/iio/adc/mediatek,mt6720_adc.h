/* SPDX-License-Identifier: GPL-2.0-only OR BSD-2-Clause */
/*
 * This header provides macros for MT6720 ADC device bindings.
 *
 * Copyright (c) 2025 Mediatek Inc.
 * Copyright (c) 2025 Richtek Technology Corp.
 * Author: Gene Chen <gene_chen@richtek.com>
 */

#ifndef _DT_BINDINGS_IIO_ADC_MEDIATEK_MT6720_ADC_H
#define _DT_BINDINGS_IIO_ADC_MEDIATEK_MT6720_ADC_H

/* ADC channel idx. */
#define MT6720_ADC_FGVBAT	0
#define MT6720_ADC_CHGVIN	1
#define MT6720_ADC_USBDP	2
#define MT6720_ADC_VSYS		3
#define MT6720_ADC_VBAT		4
#define MT6720_ADC_IBUS		5
#define MT6720_ADC_IBAT		6
#define MT6720_ADC_USBDM	7
#define MT6720_ADC_TEMPJC	8
#define MT6720_ADC_VREFTS	9
#define MT6720_ADC_TS		10
#define MT6720_ADC_PDVBUS	11
#define MT6720_ADC_CC1		12
#define MT6720_ADC_CC2		13
#define MT6720_ADC_SBU1		14
#define MT6720_ADC_SBU2		15
#define MT6720_ADC_DIV2		16
#define MT6720_ADC_ZCV		17
#define MT6720_ADC_VBATMON	18
#define MT6720_ADC_MAX_CHANNEL	19

#endif

