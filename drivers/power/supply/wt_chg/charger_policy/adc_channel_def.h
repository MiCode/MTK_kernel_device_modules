// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
 */

#ifndef __LINUX_SOUTHCHIP_ADC_CHANNEL_DEF_H__
#define __LINUX_SOUTHCHIP_ADC_CHANNEL_DEF_H__

enum sc_adc_channel {
	SC_ADC_VBUS = 0,
	SC_ADC_VSYS,
	SC_ADC_VBAT,
	SC_ADC_VAC,
	SC_ADC_IBUS,
	SC_ADC_IBAT,

	SC_ADC_TSBUS,
	SC_ADC_TSBAT,
	SC_ADC_TDIE,
	SC_ADC_BATTID,
	SC_ADC_MAX,
};

#endif /* __LINUX_SOUTHCHIP_ADC_CHANNEL_DEF_H__ */

