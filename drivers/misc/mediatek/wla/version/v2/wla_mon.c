// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include "wla.h"

static struct wla_monitor *wla_mon;

static void __set_sig_sel(unsigned int sel_n, uint32_t val, uint32_t mask)
{
	switch (sel_n) {
	case 0:
		wla_write_raw(WLAPM_MON1, val, mask);
		break;
	case 1:
		wla_write_raw(WLAPM_MON1, val << 16, mask << 16);
		break;
	case 2:
		wla_write_raw(WLAPM_MON2, val << 16, mask << 16);
		break;
	}
}

static void __set_bit_sel(unsigned int sel_n, uint32_t val, uint32_t mask)
{
	switch (sel_n) {
	case 0:
		wla_write_raw(WLAPM_MON3, val, mask);
		break;
	case 1:
		wla_write_raw(WLAPM_MON3, val << 16, mask << 16);
		break;
	case 2:
		wla_write_raw(WLAPM_MON4, val, mask);
		break;
	case 3:
		wla_write_raw(WLAPM_MON4, val << 16, mask << 16);
		break;
	case 4:
		wla_write_raw(WLAPM_MON5, val << 16, mask << 16);
		break;
	}
}

static uint32_t __get_sig_sel(unsigned int sel_n, uint32_t mask)
{
	uint32_t val = 0;

	switch (sel_n) {
	case 0:
		val = wla_read_raw(WLAPM_MON1, mask);
		break;
	case 1:
		val = wla_read_raw(WLAPM_MON1, mask << 16) >> 16;
		break;
	case 2:
		val = wla_read_raw(WLAPM_MON2, mask << 16) >> 16;
		break;
	default:
		pr_info("[wla] %s: invalid sel_n\n", __func__);
		break;
	}

	return val;
}

static uint32_t __get_bit_sel(unsigned int sel_n, uint32_t mask)
{
	uint32_t val = 0;

	switch (sel_n) {
	case 0:
		val = wla_read_raw(WLAPM_MON3, mask);
		break;
	case 1:
		val = wla_read_raw(WLAPM_MON3, mask << 16) >> 16;
		break;
	case 2:
		val = wla_read_raw(WLAPM_MON4, mask);
		break;
	case 3:
		val = wla_read_raw(WLAPM_MON4, mask << 16) >> 16;
		break;
	case 4:
		val = wla_read_raw(WLAPM_MON5, mask << 16) >> 16;
		break;
	default:
		pr_info("[wla] %s: invalid sel_n\n", __func__);
		break;
	}

	return val;
}

void wla_mon_ch_sel(unsigned int ch, struct wla_mon_ch_setting *ch_set)
{
	uint32_t sel_value, mask;
	unsigned int sel_ch_start, sel_reg_n, shift;

	if (!ch_set || !wla_mon)
		return;

	if (ch >= wla_mon->ch_hw_max)
		return;

	sel_value = ch_set->mux.sig_sel & 0x7;
	sel_ch_start = (ch * 3) % 16;
	sel_reg_n = (ch * 3) / 16;

	if (sel_ch_start > 13 && sel_ch_start < 16) {
		mask = (1U << (16 - sel_ch_start)) - 1;
		__set_sig_sel(sel_reg_n, sel_value << sel_ch_start,
						mask << sel_ch_start);
		sel_reg_n++;
		shift = 16 - sel_ch_start;
		mask = (~mask) & 0x7;
		__set_sig_sel(sel_reg_n, sel_value >> shift, mask >> shift);
	} else {
		__set_sig_sel(sel_reg_n, sel_value << sel_ch_start,
						0x7U << sel_ch_start);
	}

	sel_value = ch_set->mux.bit_sel & 0x1F;
	sel_ch_start = (ch * 5) % 16;
	sel_reg_n = (ch * 5) / 16;

	if (sel_ch_start > 11 && sel_ch_start < 16) {
		mask = (1U << (16 - sel_ch_start)) - 1;
		__set_bit_sel(sel_reg_n, sel_value << sel_ch_start,
						mask << sel_ch_start);
		sel_reg_n++;
		shift = 16 - sel_ch_start;
		mask = (~mask) & 0x1f;
		__set_bit_sel(sel_reg_n, sel_value >> shift, mask >> shift);
	} else {
		__set_bit_sel(sel_reg_n, sel_value << sel_ch_start,
						0x1fU << sel_ch_start);
	}

	/* 0x0:rising, 0x1:falling, 0x2:high, 0x3:low */
	wla_write_raw(WLAPM_MON6, ch_set->trig_type << (ch * 2), 0x3U << (ch * 2));
}
EXPORT_SYMBOL(wla_mon_ch_sel);

int wla_mon_get_ch_sel(unsigned int ch, struct wla_mon_ch_setting *ch_set)
{
	unsigned int sel_ch_start, sel_reg_n, shift;
	uint32_t mask, val;
	uint32_t val_l = 0, val_h = 0;

	if (!ch_set || !wla_mon)
		return WLA_FAIL;

	if (ch >= wla_mon->ch_hw_max)
		return WLA_FAIL;

	sel_ch_start = (ch * 3) % 16;
	sel_reg_n = (ch * 3) / 16;

	if (sel_ch_start > 13 && sel_ch_start < 16) {
		mask = (1U << (16 - sel_ch_start)) - 1;
		val_l = __get_sig_sel(sel_reg_n, mask << sel_ch_start);
		val_l = val_l >> sel_ch_start;
		sel_reg_n++;
		shift = 16 - sel_ch_start;
		mask = (~mask) & 0x7;
		val_h = __get_sig_sel(sel_reg_n,  mask >> shift);
		ch_set->mux.sig_sel = (val_h << shift) | val_l;
	} else {
		ch_set->mux.sig_sel = __get_sig_sel(sel_reg_n,
								0x7U << sel_ch_start);
		ch_set->mux.sig_sel = ch_set->mux.sig_sel >> sel_ch_start;
	}

	sel_ch_start = (ch * 5) % 16;
	sel_reg_n = (ch * 5) / 16;

	if (sel_ch_start > 11 && sel_ch_start < 16) {
		mask = (1U << (16 - sel_ch_start)) - 1;
		val_l = __get_bit_sel(sel_reg_n, mask << sel_ch_start);
		val_l = val_l >> sel_ch_start;
		sel_reg_n++;
		shift = 16 - sel_ch_start;
		mask = (~mask) & 0x1f;
		val_h = __get_bit_sel(sel_reg_n, mask >> shift);
		ch_set->mux.bit_sel = (val_h << shift) | val_l;
	} else {
		ch_set->mux.bit_sel = __get_bit_sel(sel_reg_n,
								0x1fU << sel_ch_start);
		ch_set->mux.bit_sel = ch_set->mux.bit_sel >> sel_ch_start;
	}

	/* 0x0:rising, 0x1:falling, 0x2:high, 0x3:low */
	val = wla_read_raw(WLAPM_MON6, 0x3U << (ch * 2));
	ch_set->trig_type = val >> (ch * 2);

	return WLA_SUCCESS;
}
EXPORT_SYMBOL(wla_mon_get_ch_sel);

void wla_mon_set_ddr_sta_mux(unsigned int stat_n, unsigned int mux)
{
	if (!wla_mon)
		return;

	if (stat_n < 1 || stat_n > wla_mon->ddr_sta_mux_hw_max)
		return;

	if (stat_n <= 3) {
		wla_write_raw(WLAPM_DDREN_DBG, (mux << ((stat_n - 1) * 6)) << 12,
					(0x3fU << ((stat_n - 1) * 6)) << 12);
	} else {
		wla_write_raw(WLAPM_2P0_DDREN_CTRL14, mux << ((stat_n - 4) * 6),
					0x3fU << ((stat_n - 4) * 6));
	}
}
EXPORT_SYMBOL(wla_mon_set_ddr_sta_mux);

int wla_mon_get_ddr_sta_mux(unsigned int stat_n, unsigned int *mux)
{
	uint32_t val;

	if (!wla_mon)
		return WLA_FAIL;

	if (stat_n < 1 || stat_n > wla_mon->ddr_sta_mux_hw_max)
		return WLA_FAIL;

	if (stat_n <= 3) {
		val = wla_read_raw(WLAPM_DDREN_DBG,
						(0x3fU << ((stat_n - 1) * 6)) << 12);
		*mux = (val >> ((stat_n - 1) * 6)) >> 12;
	} else {
		val = wla_read_raw(WLAPM_2P0_DDREN_CTRL14,
						0x3fU << ((stat_n - 4) * 6));
		*mux = val >> ((stat_n - 4) * 6);
	}

	return WLA_SUCCESS;
}
EXPORT_SYMBOL(wla_mon_get_ddr_sta_mux);

void wla_mon_set_pt_sta_mux(unsigned int stat_n, unsigned int mux)
{
	if (!wla_mon)
		return;

	if (stat_n < 1 || stat_n > wla_mon->pt_sta_mux_hw_max)
		return;

	wla_write_raw(WLAPM_2P0_PTCTRL99, (mux << ((stat_n - 1) * 3)) << 26,
				(0x7U << ((stat_n - 1) * 3)) << 26);
}
EXPORT_SYMBOL(wla_mon_set_pt_sta_mux);

int wla_mon_get_pt_sta_mux(unsigned int stat_n, unsigned int *mux)
{
	uint32_t val;

	if (!wla_mon)
		return WLA_FAIL;

	if (stat_n < 1 || stat_n > wla_mon->pt_sta_mux_hw_max)
		return WLA_FAIL;

	val = wla_read_raw(WLAPM_2P0_PTCTRL99,
					(0x7U << ((stat_n - 1) * 3)) << 26);
	*mux = (val >> ((stat_n - 1) * 3)) >> 26;

	return WLA_SUCCESS;
}
EXPORT_SYMBOL(wla_mon_get_pt_sta_mux);

void wla_mon_set_vlc_mux(unsigned int mux_num, struct wla_mon_mux_sel *mux)
{
	unsigned int addr_offset, shift;

	if (!mux || !wla_mon)
		return;

	if (mux_num >= wla_mon->vlc_mux_hw_max)
		return;

	addr_offset = mux_num & ~0x3;
	shift = (mux_num % 4) * 8;
	wla_write_raw(WLAPM_2P0_VLC_DBGMUX0 + addr_offset,
				(mux->sig_sel << 0x5) << shift, 0xe0 << shift);
	wla_write_raw(WLAPM_2P0_VLC_DBGMUX0 + addr_offset,
				mux->bit_sel << shift, 0x1f << shift);
}
EXPORT_SYMBOL(wla_mon_set_vlc_mux);

int wla_mon_get_vlc_mux(unsigned int mux_num, struct wla_mon_mux_sel *mux)
{
	unsigned int addr_offset, shift;

	if (!mux || !wla_mon)
		return WLA_FAIL;

	if (mux_num >= wla_mon->vlc_mux_hw_max)
		return WLA_FAIL;

	addr_offset = mux_num & ~0x3;
	shift = (mux_num % 4) * 8;
	mux->sig_sel = (wla_read_raw(WLAPM_2P0_VLC_DBGMUX0 + addr_offset,
						0xe0 << shift) >> 0x5) >> shift;
	mux->bit_sel = wla_read_raw(WLAPM_2P0_VLC_DBGMUX0 + addr_offset,
						0x1f << shift);
	mux->bit_sel >>= shift;

	return WLA_SUCCESS;
}
EXPORT_SYMBOL(wla_mon_get_vlc_mux);

void wla_mon_set_tfc_mux(unsigned int mux_num, struct wla_mon_mux_sel *mux)
{
	unsigned int addr_offset, shift;

	if (!mux || !wla_mon)
		return;

	if (mux_num >= wla_mon->tfc_mux_hw_max)
		return;

	addr_offset = mux_num & ~0x3;
	shift = (mux_num % 4) * 8;
	wla_write_raw(WLAPM_2P0_TFC_DBGMUX0 + addr_offset,
				(mux->sig_sel << 0x5) << shift, 0xe0 << shift);
	wla_write_raw(WLAPM_2P0_TFC_DBGMUX0 + addr_offset,
				mux->bit_sel << shift, 0x1f << shift);
}
EXPORT_SYMBOL(wla_mon_set_tfc_mux);

int wla_mon_get_tfc_mux(unsigned int mux_num, struct wla_mon_mux_sel *mux)
{
	unsigned int addr_offset, shift;

	if (!mux || !wla_mon)
		return WLA_FAIL;

	if (mux_num >= wla_mon->tfc_mux_hw_max)
		return WLA_FAIL;

	addr_offset = mux_num & ~0x3;
	shift = (mux_num % 4) * 8;
	mux->sig_sel = (wla_read_raw(WLAPM_2P0_TFC_DBGMUX0 + addr_offset,
						0xe0 << shift) >> 0x5) >> shift;
	mux->bit_sel = wla_read_raw(WLAPM_2P0_TFC_DBGMUX0 + addr_offset,
						0x1f << shift);
	mux->bit_sel >>= shift;

	return WLA_SUCCESS;
}
EXPORT_SYMBOL(wla_mon_get_tfc_mux);

void wla_mon_set_pmsr_ocla_mux(unsigned int mux_num,
							struct wla_mon_mux_sel *mux)
{
	unsigned int addr_offset, shift;

	if (!mux || !wla_mon)
		return;

	if (mux_num >= wla_mon->pmsr_ocla_mux_hw_max)
		return;

	addr_offset = mux_num & ~0x3;
	shift = (mux_num % 4) * 8;
	wla_write_raw(WLAPM_2P0_DBGMUX0 + addr_offset,
				(mux->sig_sel << 0x5) << shift, 0xe0 << shift);
	wla_write_raw(WLAPM_2P0_DBGMUX0 + addr_offset,
				mux->bit_sel << shift, 0x1f << shift);
}
EXPORT_SYMBOL(wla_mon_set_pmsr_ocla_mux);

int wla_mon_get_pmsr_ocla_mux(unsigned int mux_num, struct wla_mon_mux_sel *mux)
{
	unsigned int addr_offset, shift;

	if (!mux || !wla_mon)
		return WLA_FAIL;

	if (mux_num >= wla_mon->pmsr_ocla_mux_hw_max)
		return WLA_FAIL;

	addr_offset = mux_num & ~0x3;
	shift = (mux_num % 4) * 8;
	mux->sig_sel = (wla_read_raw(WLAPM_2P0_DBGMUX0 + addr_offset,
						0xe0 << shift) >> 0x5) >> shift;
	mux->bit_sel = wla_read_raw(WLAPM_2P0_DBGMUX0 + addr_offset,
						0x1f << shift);
	mux->bit_sel >>= shift;

	return WLA_SUCCESS;
}
EXPORT_SYMBOL(wla_mon_get_pmsr_ocla_mux);

int wla_mon_start(unsigned int win_len_sec)
{
	uint32_t cnt;

	if (win_len_sec > 150)
		return WLA_FAIL;

	cnt = win_len_sec * 26000000;
	wla_write_field(WLAPM_MON0, 0x36, GENMASK(31, 24));
	wla_write_field(WLAPM_MON25, 0, GENMASK(31, 0));
	wla_write_field(WLAPM_MON0, 0x0, GENMASK(31, 24));
	wla_write_field(WLAPM_MON25, cnt, GENMASK(31, 0));
	wla_write_field(WLAPM_MON0, 0x3, GENMASK(31, 28));
	wla_write_field(WLAPM_MON0, 0x0, WLAPM_PMSR_IRQ_CLEAR);
	return WLA_SUCCESS;
}
EXPORT_SYMBOL(wla_mon_start);

void wla_mon_disable(void)
{
	wla_write_field(WLAPM_MON0, 0x36, GENMASK(31, 24));
	wla_write_field(WLAPM_MON25, 0, GENMASK(31, 0));
	wla_write_field(WLAPM_MON0, 0x0, GENMASK(31, 24));
}
EXPORT_SYMBOL(wla_mon_disable);

unsigned int wla_mon_get_ch_hw_max(void)
{
	if (!wla_mon)
		return 0;

	return wla_mon->ch_hw_max;
}
EXPORT_SYMBOL(wla_mon_get_ch_hw_max);

unsigned int wla_mon_get_ddr_sta_mux_hw_max(void)
{
	if (!wla_mon)
		return 0;

	return wla_mon->ddr_sta_mux_hw_max;
}
EXPORT_SYMBOL(wla_mon_get_ddr_sta_mux_hw_max);

unsigned int wla_mon_get_pt_sta_mux_hw_max(void)
{
	if (!wla_mon)
		return 0;

	return wla_mon->pt_sta_mux_hw_max;
}
EXPORT_SYMBOL(wla_mon_get_pt_sta_mux_hw_max);

unsigned int wla_mon_get_vlc_mux_hw_max(void)
{
	if (!wla_mon)
		return 0;

	return wla_mon->vlc_mux_hw_max;
}
EXPORT_SYMBOL(wla_mon_get_vlc_mux_hw_max);

unsigned int wla_mon_get_tfc_mux_hw_max(void)
{
	if (!wla_mon)
		return 0;

	return wla_mon->tfc_mux_hw_max;
}
EXPORT_SYMBOL(wla_mon_get_tfc_mux_hw_max);

unsigned int wla_mon_get_pmsr_ocla_mux_hw_max(void)
{
	if (!wla_mon)
		return 0;

	return wla_mon->pmsr_ocla_mux_hw_max;
}
EXPORT_SYMBOL(wla_mon_get_pmsr_ocla_mux_hw_max);

int wla_mon_get_ch_cnt(unsigned int ch, uint32_t *north_cnt,
							uint32_t *south_cnt)
{
	if (!wla_mon)
		return WLA_FAIL;

	if (ch >= wla_mon->ch_hw_max)
		return WLA_FAIL;

	if (wla_read_field(WLAPM_MON7, WLAPM_PMSR_IRQ_B))
		return WLA_FAIL;

	if (north_cnt)
		*north_cnt = wla_read_field(WLAPM_MON9 + (ch * 4), GENMASK(31, 0));

	return WLA_SUCCESS;
}
EXPORT_SYMBOL(wla_mon_get_ch_cnt);

int wla_mon_init(struct wla_monitor *mon)
{
	unsigned int num;

	if (mon == NULL)
		return WLA_FAIL;

	wla_mon = mon;

	for (num = 0; num < mon->ch_num; num++)
		wla_mon_ch_sel(num, &mon->ch[num]);

	for (num = 1; num <= mon->ddr_sta_mux_num; num++)
		wla_mon_set_ddr_sta_mux(num, mon->ddr_sta_mux[num]);

	for (num = 0; num < mon->vlc_mux_num; num++)
		wla_mon_set_vlc_mux(num, &mon->vlc_mux[num]);

	for (num = 0; num < mon->tfc_mux_num; num++)
		wla_mon_set_tfc_mux(num, &mon->tfc_mux[num]);

	for (num = 0; num < mon->pmsr_ocla_mux_num; num++)
		wla_mon_set_pmsr_ocla_mux(num, &mon->pmsr_ocla_mux[num]);

	return WLA_SUCCESS;
}
