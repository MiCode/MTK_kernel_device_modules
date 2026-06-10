/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */
#ifndef _AUDIO_DCXO_H_
#define _AUDIO_DCXO_H_

struct mtk_aud_data {
	u32         mode_reg;
	u32         mode_shift;
	u32         mode_mshift;
	u32         en_reg;
	u32         en_shift;
	u32         en_mshift;
};

/* just be called by audio module for dcxo */
extern void mt6685_set_dcxo_mode(unsigned int mode);
extern void mt6685_set_dcxo(bool enable);
#endif
